#include "core/network/ICMPSocket.h"
#include "core/kernel.h"
#include "core/network/IPv4.h"
#include "core/peripherals/Logger.h"
#include "libs/memory.h"
#include "palmyraOS/errono.h"
#include "palmyraOS/socket.h"

namespace PalmyraOS::kernel {

    // ==================== Static Member Initialization ====================

    ICMPSocket* ICMPSocket::socketRegistry_[MAX_SOCKETS] = {nullptr};
    uint8_t ICMPSocket::registryCount_                   = 0;

    // ==================== Packet Implementation ====================

    ICMPSocket::Packet::~Packet() {
        if (data) {
            heapManager.free(data);
            data = nullptr;
        }
    }

    ICMPSocket::Packet::Packet(Packet&& other) noexcept : srcIP(other.srcIP), data(other.data), size(other.size) {
        other.data = nullptr;  // Transfer ownership
        other.size = 0;
    }

    ICMPSocket::Packet& ICMPSocket::Packet::operator=(Packet&& other) noexcept {
        if (this != &other) {
            // Free existing data
            if (data) {
                heapManager.free(data);
            }

            // Transfer ownership
            srcIP      = other.srcIP;
            data       = other.data;
            size       = other.size;

            other.data = nullptr;
            other.size = 0;
        }
        return *this;
    }

    // ==================== Constructor / Destructor ====================

    ICMPSocket::ICMPSocket()
        : state_(State::Unbound), localIP_(0), remoteIP_(0), nonBlocking_(false), lastError_(0), receiveQueue_(nullptr) {

        // Allocate receive queue (MUST be heap-allocated)
        receiveQueue_ = heapManager.createInstance<KQueue<Packet>>();
        if (!receiveQueue_) {
            LOG_ERROR("ICMPSocket: Failed to allocate receive queue");
            lastError_ = ENOMEM;
            return;
        }

        // Register this socket globally (all ICMP sockets receive all ICMP packets)
        registerSocket(this);

        LOG_INFO("ICMPSocket: Created raw ICMP socket");
    }

    ICMPSocket::~ICMPSocket() {
        LOG_INFO("ICMPSocket: Destroying raw ICMP socket");

        // Unregister from global list
        unregisterSocket(this);

        // Free receive queue
        if (receiveQueue_) {
            heapManager.free(receiveQueue_);
            receiveQueue_ = nullptr;
        }
    }

    // ==================== ProtocolSocket Interface ====================

    int ICMPSocket::bind(uint32_t localIP, uint16_t localPort) {
        // ICMP has no ports, but we can bind to a local IP
        // (Though for raw sockets, this is typically not needed)
        localIP_ = localIP;
        state_   = State::Unbound;  // Still unbound in terms of connection
        LOG_INFO("ICMPSocket: Bind called (localIP set, port ignored for ICMP)");
        return 0;
    }

    int ICMPSocket::connect(uint32_t remoteIP, uint16_t remotePort) {
        // Connect for ICMP raw socket means "filter by source IP"
        // Port is ignored (ICMP has no ports)
        remoteIP_ = remoteIP;
        state_    = State::Connected;

        LOG_INFO("ICMPSocket: Connected to %u.%u.%u.%u (will filter by source)", (remoteIP_ >> 24) & 0xFF,
                 (remoteIP_ >> 16) & 0xFF, (remoteIP_ >> 8) & 0xFF, remoteIP_ & 0xFF);

        return 0;
    }

    size_t ICMPSocket::sendto(const char* buffer, size_t length, uint32_t destIP, uint16_t destPort) {
        // destPort is ignored for ICMP (no ports in ICMP)

        if (!buffer || length == 0) {
            return -EINVAL;
        }

        // Buffer contains: ICMP header (type, code, checksum, id, seq) + data
        // Send directly via IPv4 (protocol = 1 for ICMP)
        bool success = IPv4::sendPacket(destIP, IPv4::PROTOCOL_ICMP, (const uint8_t*)buffer, length);
        if (!success) {
            LOG_ERROR("ICMPSocket: Failed to send ICMP packet");
            return -EIO;
        }

        return length;
    }

    size_t ICMPSocket::recvfrom(char* buffer, size_t length, uint32_t* srcIP, uint16_t* srcPort) {
        if (!receiveQueue_) {
            LOG_ERROR("ICMPSocket: Receive queue not allocated");
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

        // Fill in source address
        if (srcIP) *srcIP = pkt.srcIP;
        if (srcPort) *srcPort = 0;  // ICMP has no ports

        // Packet destructor will free pkt.data
        return copySize;
    }

    int ICMPSocket::getBytesAvailable() const {
        if (!receiveQueue_ || receiveQueue_->empty()) {
            return 0;
        }
        return receiveQueue_->front().size;
    }

    int ICMPSocket::close() {
        state_ = State::Unbound;
        return 0;
    }

    bool ICMPSocket::isBound() const { return state_ != State::Unbound; }

    bool ICMPSocket::isConnected() const { return state_ == State::Connected; }

    uint32_t ICMPSocket::getLocalIP() const { return localIP_; }

    uint16_t ICMPSocket::getLocalPort() const { return 0; }  // ICMP has no ports

    uint32_t ICMPSocket::getRemoteIP() const { return remoteIP_; }

    uint16_t ICMPSocket::getRemotePort() const { return 0; }  // ICMP has no ports

    int ICMPSocket::setsockopt(int level, int optname, const void* optval, uint32_t optlen) {
        if (level != SOL_SOCKET) {
            LOG_WARN("ICMPSocket: Unsupported setsockopt level %d", level);
            return -ENOPROTOOPT;
        }

        switch (optname) {
            case SO_ERROR: {
                // SO_ERROR is read-only
                return -ENOPROTOOPT;
            }

            default:
                LOG_WARN("ICMPSocket: Unsupported setsockopt option %d", optname);
                return -ENOPROTOOPT;
        }
    }

    int ICMPSocket::getsockopt(int level, int optname, void* optval, uint32_t* optlen) {
        if (!optval || !optlen) return -EINVAL;

        if (level != SOL_SOCKET) {
            LOG_WARN("ICMPSocket: Unsupported getsockopt level %d", level);
            return -ENOPROTOOPT;
        }

        switch (optname) {
            case SO_TYPE: {
                if (*optlen < sizeof(int)) return -EINVAL;
                *(int*)optval = SOCK_RAW;
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

            default:
                LOG_WARN("ICMPSocket: Unsupported getsockopt option %d", optname);
                return -ENOPROTOOPT;
        }
    }

    int ICMPSocket::listen(int backlog) {
        LOG_ERROR("ICMPSocket: listen() not supported on raw ICMP sockets");
        return -EOPNOTSUPP;
    }

    ProtocolSocket* ICMPSocket::accept(uint32_t* remoteIP, uint16_t* remotePort) {
        LOG_ERROR("ICMPSocket: accept() not supported on raw ICMP sockets");
        return nullptr;
    }

    int ICMPSocket::shutdown(int how) {
        LOG_ERROR("ICMPSocket: shutdown() not supported on raw ICMP sockets");
        return -EOPNOTSUPP;
    }

    // ==================== Packet Delivery ====================

    void ICMPSocket::deliverPacket(uint32_t srcIP, const uint8_t* icmpData, uint32_t length) {
        if (!receiveQueue_) {
            LOG_ERROR("ICMPSocket: Receive queue not allocated");
            return;
        }

        // If connected, filter by source IP
        if (state_ == State::Connected && srcIP != remoteIP_) {
            return;  // Ignore packets from other sources
        }

        // Check queue size limit
        if (receiveQueue_->size() >= MAX_QUEUE_SIZE) {
            LOG_WARN("ICMPSocket: Receive queue full, dropping ICMP packet");
            return;
        }

        // Allocate packet data
        uint8_t* packetData = (uint8_t*)heapManager.alloc(length);
        if (!packetData) {
            LOG_ERROR("ICMPSocket: Failed to allocate packet data");
            return;
        }

        // Copy packet data
        memcpy(packetData, icmpData, length);

        // Create packet and enqueue
        Packet pkt;
        pkt.srcIP = srcIP;
        pkt.data  = packetData;
        pkt.size  = length;

        receiveQueue_->push(std::move(pkt));

        LOG_DEBUG("ICMPSocket: Queued ICMP packet from %u.%u.%u.%u (%u bytes)", (srcIP >> 24) & 0xFF,
                  (srcIP >> 16) & 0xFF, (srcIP >> 8) & 0xFF, srcIP & 0xFF, length);
    }

    // ==================== Socket Registry ====================

    void ICMPSocket::registerSocket(ICMPSocket* socket) {
        if (registryCount_ >= MAX_SOCKETS) {
            LOG_ERROR("ICMPSocket: Socket registry full");
            return;
        }

        for (uint8_t i = 0; i < MAX_SOCKETS; ++i) {
            if (socketRegistry_[i] == nullptr) {
                socketRegistry_[i] = socket;
                registryCount_++;
                LOG_INFO("ICMPSocket: Registered raw ICMP socket (%u/%u)", registryCount_, MAX_SOCKETS);
                return;
            }
        }
    }

    void ICMPSocket::unregisterSocket(ICMPSocket* socket) {
        for (uint8_t i = 0; i < MAX_SOCKETS; ++i) {
            if (socketRegistry_[i] == socket) {
                socketRegistry_[i] = nullptr;
                registryCount_--;
                LOG_INFO("ICMPSocket: Unregistered raw ICMP socket (%u/%u)", registryCount_, MAX_SOCKETS);
                return;
            }
        }
    }

    void ICMPSocket::deliverToAllSockets(uint32_t srcIP, const uint8_t* icmpData, uint32_t length) {
        // Broadcast to all registered raw ICMP sockets (Linux behavior)
        for (uint8_t i = 0; i < MAX_SOCKETS; ++i) {
            if (socketRegistry_[i] != nullptr) {
                socketRegistry_[i]->deliverPacket(srcIP, icmpData, length);
            }
        }
    }

}  // namespace PalmyraOS::kernel

