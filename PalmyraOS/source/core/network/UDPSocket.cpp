#include "core/network/UDPSocket.h"
#include "core/kernel.h"
#include "core/network/UDP.h"
#include "core/peripherals/Logger.h"
#include "libs/memory.h"
#include "palmyraOS/errono.h"
#include "palmyraOS/socket.h"

namespace PalmyraOS::kernel {

    // ==================== Static Member Initialization ====================

    UDPSocket* UDPSocket::socketRegistry_[MAX_SOCKETS]  = {nullptr};
    uint16_t UDPSocket::registryPorts_[MAX_SOCKETS]     = {0};
    uint8_t UDPSocket::registryCount_                   = 0;

    // ==================== Packet Implementation ====================

    UDPSocket::Packet::~Packet() {
        if (data) {
            heapManager.free(data);
            data = nullptr;
        }
    }

    UDPSocket::Packet::Packet(Packet&& other) noexcept
        : srcIP(other.srcIP), srcPort(other.srcPort), data(other.data), size(other.size) {
        other.data = nullptr;  // Transfer ownership
        other.size = 0;
    }

    UDPSocket::Packet& UDPSocket::Packet::operator=(Packet&& other) noexcept {
        if (this != &other) {
            // Free existing data
            if (data) {
                heapManager.free(data);
            }

            // Transfer ownership
            srcIP      = other.srcIP;
            srcPort    = other.srcPort;
            data       = other.data;
            size       = other.size;

            other.data = nullptr;
            other.size = 0;
        }
        return *this;
    }

    // ==================== Constructor / Destructor ====================

    UDPSocket::UDPSocket()
        : state_(State::Unbound),
          localIP_(0),
          localPort_(0),
          remoteIP_(0),
          remotePort_(0),
          nonBlocking_(false),
          reuseAddr_(false),
          broadcast_(false),
          lastError_(0),
          receiveQueue_(nullptr) {

        // Allocate receive queue (MUST be heap-allocated, see WindowManager pattern)
        receiveQueue_ = heapManager.createInstance<KQueue<Packet>>();
        if (!receiveQueue_) {
            LOG_ERROR("UDPSocket: Failed to allocate receive queue");
            lastError_ = ENOMEM;
            return;
        }

        LOG_INFO("UDPSocket: Created UDP socket");
    }

    UDPSocket::~UDPSocket() {
        LOG_INFO("UDPSocket: Destroying socket (local=%u.%u.%u.%u:%u)", (localIP_ >> 24) & 0xFF, (localIP_ >> 16) & 0xFF,
                 (localIP_ >> 8) & 0xFF, localIP_ & 0xFF, localPort_);

        // Unbind port if bound
        if (state_ != State::Unbound && localPort_ != 0) {
            UDP::unbindPort(localPort_);
            unregisterSocket(localPort_);
        }

        // Free receive queue
        if (receiveQueue_) {
            // KQueue destructor will handle freeing queued Packet objects
            heapManager.free(receiveQueue_);
            receiveQueue_ = nullptr;
        }
    }

    // ==================== ProtocolSocket Interface ====================

    int UDPSocket::bind(uint32_t localIP, uint16_t localPort) {
        if (state_ != State::Unbound) {
            LOG_ERROR("UDPSocket: Cannot bind - socket already bound");
            return -EINVAL;
        }

        if (localPort == 0) {
            LOG_ERROR("UDPSocket: Cannot bind to port 0");
            return -EINVAL;
        }

        // Register callback with UDP layer
        bool success = UDP::bindPort(localPort, udpReceiveCallbackTrampoline);
        if (!success) {
            LOG_ERROR("UDPSocket: UDP::bindPort failed for port %u", localPort);
            return -EADDRINUSE;
        }

        // Register this socket instance
        registerSocket(localPort, this);

        // Save binding
        localIP_   = localIP;
        localPort_ = localPort;
        state_     = State::Bound;

        LOG_INFO("UDPSocket: Bound to %u.%u.%u.%u:%u", (localIP_ >> 24) & 0xFF, (localIP_ >> 16) & 0xFF,
                 (localIP_ >> 8) & 0xFF, localIP_ & 0xFF, localPort_);

        return 0;
    }

    int UDPSocket::connect(uint32_t remoteIP, uint16_t remotePort) {
        // UDP connect just sets default destination (no actual connection)
        remoteIP_   = remoteIP;
        remotePort_ = remotePort;
        state_      = State::Connected;

        LOG_INFO("UDPSocket: Connected to %u.%u.%u.%u:%u", (remoteIP_ >> 24) & 0xFF, (remoteIP_ >> 16) & 0xFF,
                 (remoteIP_ >> 8) & 0xFF, remoteIP_ & 0xFF, remotePort_);

        return 0;
    }

    size_t UDPSocket::sendto(const char* buffer, size_t length, uint32_t destIP, uint16_t destPort) {
        // Allocate source port if not bound
        uint16_t srcPort = localPort_;
        if (srcPort == 0) {
            srcPort = UDP::allocateEphemeralPort();
            if (srcPort == 0) {
                LOG_ERROR("UDPSocket: Failed to allocate ephemeral port");
                return -EAGAIN;
            }
            localPort_ = srcPort;
            
            // Register callback for receiving responses (CRITICAL FIX!)
            bool bindSuccess = UDP::bindPort(srcPort, udpReceiveCallbackTrampoline);
            if (!bindSuccess) {
                LOG_ERROR("UDPSocket: Failed to bind callback for ephemeral port %u", srcPort);
                localPort_ = 0;
                return -EADDRINUSE;
            }
            registerSocket(srcPort, this);
            state_ = State::Bound;
            LOG_INFO("UDPSocket: Auto-allocated and bound ephemeral port %u", srcPort);
        }

        // Send via UDP layer
        bool success = UDP::sendDatagram(destIP, destPort, srcPort, (const uint8_t*)buffer, length);
        if (!success) {
            LOG_ERROR("UDPSocket: UDP send failed");
            return -EIO;
        }

        return length;
    }

    size_t UDPSocket::recvfrom(char* buffer, size_t length, uint32_t* srcIP, uint16_t* srcPort) {
        if (!receiveQueue_) {
            LOG_ERROR("UDPSocket: Receive queue not allocated");
            return -EBADF;
        }

        // Check if data available
        if (receiveQueue_->empty()) {
            if (nonBlocking_) {
                return -EAGAIN;
            }
            // Blocking mode - return 0 for now (would need sleep/wake mechanism)
            return 0;
        }

        // Pop packet from queue
        Packet pkt = std::move(receiveQueue_->front());
        receiveQueue_->pop();

        // Copy data to buffer
        size_t copySize = (pkt.size < length) ? pkt.size : length;
        memcpy(buffer, pkt.data, copySize);

        // Fill in source address if requested
        if (srcIP) *srcIP = pkt.srcIP;
        if (srcPort) *srcPort = pkt.srcPort;

        // Packet destructor will free pkt.data
        return copySize;
    }

    int UDPSocket::getBytesAvailable() const {
        if (!receiveQueue_ || receiveQueue_->empty()) {
            return 0;
        }
        // Return size of next packet
        return receiveQueue_->front().size;
    }

    int UDPSocket::close() {
        if (state_ != State::Unbound && localPort_ != 0) {
            UDP::unbindPort(localPort_);
            unregisterSocket(localPort_);
        }
        state_     = State::Unbound;
        localPort_ = 0;
        return 0;
    }

    bool UDPSocket::isBound() const { return state_ == State::Bound || state_ == State::Connected; }

    bool UDPSocket::isConnected() const { return state_ == State::Connected; }

    uint32_t UDPSocket::getLocalIP() const { return localIP_; }

    uint16_t UDPSocket::getLocalPort() const { return localPort_; }

    uint32_t UDPSocket::getRemoteIP() const { return remoteIP_; }

    uint16_t UDPSocket::getRemotePort() const { return remotePort_; }

    int UDPSocket::setsockopt(int level, int optname, const void* optval, uint32_t optlen) {
        if (level != SOL_SOCKET) {
            LOG_WARN("UDPSocket: Unsupported setsockopt level %d", level);
            return -ENOPROTOOPT;
        }

        switch (optname) {
            case SO_REUSEADDR: {
                if (optlen < sizeof(int)) return -EINVAL;
                reuseAddr_ = *(const int*)optval != 0;
                LOG_INFO("UDPSocket: Set SO_REUSEADDR: %s", reuseAddr_ ? "enabled" : "disabled");
                return 0;
            }

            case SO_BROADCAST: {
                if (optlen < sizeof(int)) return -EINVAL;
                broadcast_ = *(const int*)optval != 0;
                LOG_INFO("UDPSocket: Set SO_BROADCAST: %s", broadcast_ ? "enabled" : "disabled");
                return 0;
            }

            default:
                LOG_WARN("UDPSocket: Unsupported setsockopt option %d", optname);
                return -ENOPROTOOPT;
        }
    }

    int UDPSocket::getsockopt(int level, int optname, void* optval, uint32_t* optlen) {
        if (!optval || !optlen) return -EINVAL;

        if (level != SOL_SOCKET) {
            LOG_WARN("UDPSocket: Unsupported getsockopt level %d", level);
            return -ENOPROTOOPT;
        }

        switch (optname) {
            case SO_TYPE: {
                if (*optlen < sizeof(int)) return -EINVAL;
                *(int*)optval = SOCK_DGRAM;
                *optlen       = sizeof(int);
                return 0;
            }

            case SO_ERROR: {
                if (*optlen < sizeof(int)) return -EINVAL;
                *(int*)optval = lastError_;
                *optlen       = sizeof(int);
                lastError_    = 0;  // Clear error after reading
                return 0;
            }

            case SO_REUSEADDR: {
                if (*optlen < sizeof(int)) return -EINVAL;
                *(int*)optval = reuseAddr_ ? 1 : 0;
                *optlen       = sizeof(int);
                return 0;
            }

            case SO_BROADCAST: {
                if (*optlen < sizeof(int)) return -EINVAL;
                *(int*)optval = broadcast_ ? 1 : 0;
                *optlen       = sizeof(int);
                return 0;
            }

            default:
                LOG_WARN("UDPSocket: Unsupported getsockopt option %d", optname);
                return -ENOPROTOOPT;
        }
    }

    int UDPSocket::listen(int backlog) {
        LOG_ERROR("UDPSocket: listen() not supported on UDP sockets");
        return -EOPNOTSUPP;
    }

    ProtocolSocket* UDPSocket::accept(uint32_t* remoteIP, uint16_t* remotePort) {
        LOG_ERROR("UDPSocket: accept() not supported on UDP sockets");
        return nullptr;
    }

    int UDPSocket::shutdown(int how) {
        LOG_ERROR("UDPSocket: shutdown() not supported on UDP sockets");
        return -EOPNOTSUPP;
    }

    // ==================== Callback System ====================

    void UDPSocket::handleIncomingPacket(uint32_t srcIP, uint16_t srcPort, const uint8_t* data, uint32_t length) {
        if (!receiveQueue_) {
            LOG_ERROR("UDPSocket: Receive queue not allocated");
            return;
        }

        // Check queue size limit
        if (receiveQueue_->size() >= MAX_QUEUE_SIZE) {
            LOG_WARN("UDPSocket: Receive queue full, dropping packet");
            return;
        }

        // Allocate packet data
        uint8_t* packetData = (uint8_t*)heapManager.alloc(length);
        if (!packetData) {
            LOG_ERROR("UDPSocket: Failed to allocate packet data");
            return;
        }

        // Copy packet data
        memcpy(packetData, data, length);

        // Create packet and enqueue
        Packet pkt;
        pkt.srcIP   = srcIP;
        pkt.srcPort = srcPort;
        pkt.data    = packetData;
        pkt.size    = length;

        receiveQueue_->push(std::move(pkt));

        LOG_INFO("UDPSocket: Queued packet from %u.%u.%u.%u:%u (%u bytes)", (srcIP >> 24) & 0xFF, (srcIP >> 16) & 0xFF,
                 (srcIP >> 8) & 0xFF, srcIP & 0xFF, srcPort, length);
    }

    void UDPSocket::udpReceiveCallbackTrampoline(uint32_t srcIP, uint16_t srcPort, const uint8_t* data, uint32_t length) {
        // This is called from interrupt context by UDP layer
        // We need to find the socket instance associated with the destination port

        // LIMITATION: UDP::bindPort callback doesn't provide destination port
        // For now, deliver to all registered sockets so that the right one can consume it.
        // This avoids starving userland sockets when kernel components are also bound.
        if (registryCount_ == 0) {
            LOG_WARN("UDPSocket: Callback received but no sockets registered");
            return;
        }

        for (uint8_t i = 0; i < MAX_SOCKETS; ++i) {
            if (socketRegistry_[i] != nullptr) {
                socketRegistry_[i]->handleIncomingPacket(srcIP, srcPort, data, length);
            }
        }
    }

    // ==================== Socket Registry ====================

    void UDPSocket::registerSocket(uint16_t port, UDPSocket* socket) {
        if (registryCount_ >= MAX_SOCKETS) {
            LOG_ERROR("UDPSocket: Socket registry full");
            return;
        }

        for (uint8_t i = 0; i < MAX_SOCKETS; ++i) {
            if (socketRegistry_[i] == nullptr) {
                socketRegistry_[i] = socket;
                registryPorts_[i]  = port;
                registryCount_++;
                LOG_INFO("UDPSocket: Registered socket for port %u", port);
                return;
            }
        }
    }

    void UDPSocket::unregisterSocket(uint16_t port) {
        for (uint8_t i = 0; i < MAX_SOCKETS; ++i) {
            if (registryPorts_[i] == port) {
                socketRegistry_[i] = nullptr;
                registryPorts_[i]  = 0;
                registryCount_--;
                LOG_INFO("UDPSocket: Unregistered socket for port %u", port);
                return;
            }
        }
    }

    UDPSocket* UDPSocket::findSocket(uint16_t port) {
        for (uint8_t i = 0; i < MAX_SOCKETS; ++i) {
            if (registryPorts_[i] == port) {
                return socketRegistry_[i];
            }
        }
        return nullptr;
    }

}  // namespace PalmyraOS::kernel

