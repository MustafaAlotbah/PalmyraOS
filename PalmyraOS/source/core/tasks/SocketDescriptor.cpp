#include "core/tasks/SocketDescriptor.h"
#include "core/kernel.h"
#include "core/network/Ethernet.h"
#include "core/network/ICMPSocket.h"
#include "core/network/UDPSocket.h"
#include "core/peripherals/Logger.h"
#include "libs/memory.h"
#include "palmyraOS/errono.h"
#include "palmyraOS/socket.h"  // For socket constants (AF_INET, SOCK_DGRAM, etc.)

namespace PalmyraOS::kernel {

    // ==================== Constructor / Destructor ====================

    SocketDescriptor::SocketDescriptor(int domain, int type, int protocol)
        : domain_(domain), type_(type), protocol_(protocol), protocolSocket_(nullptr) {

        // Validate domain
        if (domain != AF_INET) {
            LOG_ERROR("SocketDescriptor: Unsupported address family %d (only AF_INET supported)", domain);
            return;
        }

        // Validate socket type
        if (type != SOCK_DGRAM && type != SOCK_STREAM && type != SOCK_RAW) {
            LOG_ERROR("SocketDescriptor: Unsupported socket type %d", type);
            return;
        }

        // Auto-detect protocol if not specified
        if (protocol == 0) {
            if (type == SOCK_STREAM) {
                protocol_ = IPPROTO_TCP;
            } else if (type == SOCK_DGRAM) {
                protocol_ = IPPROTO_UDP;
            } else {
                LOG_ERROR("SocketDescriptor: SOCK_RAW requires explicit protocol");
                return;
            }
        }

        // Create appropriate protocol socket
        if (type == SOCK_DGRAM) {
            protocolSocket_ = heapManager.createInstance<UDPSocket>();
            if (!protocolSocket_) {
                LOG_ERROR("SocketDescriptor: Failed to create UDPSocket");
                return;
            }
            LOG_INFO("SocketDescriptor: Created UDP socket (domain=%d, type=%d, protocol=%d)", domain_, type_,
                     protocol_);
        } else if (type == SOCK_STREAM) {
            // TCP socket - future implementation
            LOG_ERROR("SocketDescriptor: TCP sockets not yet implemented");
            return;
        } else if (type == SOCK_RAW) {
            // Raw socket - protocol-specific
            if (protocol == IPPROTO_ICMP) {
                protocolSocket_ = heapManager.createInstance<ICMPSocket>();
                if (!protocolSocket_) {
                    LOG_ERROR("SocketDescriptor: Failed to create ICMPSocket");
                    return;
                }
                LOG_INFO("SocketDescriptor: Created raw ICMP socket (domain=%d, type=%d, protocol=%d)", domain_, type_,
                         protocol_);
            } else {
                LOG_ERROR("SocketDescriptor: Unsupported raw socket protocol %d (only IPPROTO_ICMP supported)", protocol);
                return;
            }
        }
    }

    SocketDescriptor::~SocketDescriptor() {
        LOG_INFO("SocketDescriptor: Destroying socket (domain=%d, type=%d)", domain_, type_);

        // Delete protocol socket (calls destructor which cleans up resources)
        if (protocolSocket_) {
            delete protocolSocket_;
            protocolSocket_ = nullptr;
        }
    }

    // ==================== Descriptor Interface Implementation ====================

    Descriptor::Kind SocketDescriptor::kind() const { return Kind::Socket; }

    size_t SocketDescriptor::read(char* buffer, size_t size) {
        if (!protocolSocket_) {
            LOG_ERROR("SocketDescriptor: Protocol socket not initialized");
            return -EBADF;
        }

        // Equivalent to recv(fd, buffer, size, 0)
        return protocolSocket_->recvfrom(buffer, size, nullptr, nullptr);
    }

    size_t SocketDescriptor::write(const char* buffer, size_t size) {
        if (!protocolSocket_) {
            LOG_ERROR("SocketDescriptor: Protocol socket not initialized");
            return -EBADF;
        }

        // Must be connected to use write() (no destination specified)
        if (!protocolSocket_->isConnected()) {
            return -ENOTCONN;
        }

        // Use connected destination
        uint32_t destIP   = protocolSocket_->getRemoteIP();
        uint16_t destPort = protocolSocket_->getRemotePort();

        return protocolSocket_->sendto(buffer, size, destIP, destPort);
    }

    int SocketDescriptor::ioctl(int request, void* arg) {
        if (!protocolSocket_) {
            LOG_ERROR("SocketDescriptor: Protocol socket not initialized");
            return -EBADF;
        }

        switch (request) {
            case FIONBIO: {
                // Set/clear non-blocking mode
                if (!arg) return -EINVAL;
                bool enable = *(int*)arg != 0;

                // UDP-specific (for now)
                if (type_ == SOCK_DGRAM) {
                    auto* udpSocket = static_cast<UDPSocket*>(protocolSocket_);
                    udpSocket->setNonBlocking(enable);
                    LOG_INFO("SocketDescriptor: Set non-blocking mode: %s", enable ? "enabled" : "disabled");
                    return 0;
                }

                // TCP - future implementation
                LOG_ERROR("SocketDescriptor: FIONBIO not yet implemented for TCP");
                return -EOPNOTSUPP;
            }

            case FIONREAD: {
                // Get number of bytes available to read
                if (!arg) return -EINVAL;
                *(int*)arg = protocolSocket_->getBytesAvailable();
                return 0;
            }

            default:
                LOG_WARN("SocketDescriptor: Unsupported ioctl request 0x%X", request);
                return -EINVAL;
        }
    }

    // ==================== Socket-Specific Operations ====================

    int SocketDescriptor::bind(const void* addr, uint32_t addrlen) {
        if (!protocolSocket_) {
            LOG_ERROR("SocketDescriptor: Protocol socket not initialized");
            return -EBADF;
        }

        if (!validateSockaddr(addr, addrlen)) {
            return -EINVAL;
        }

        // Parse address
        uint32_t ip;
        uint16_t port;
        parseSockaddr(addr, ip, port);

        // Validate port
        if (port == 0) {
            LOG_ERROR("SocketDescriptor: Cannot bind to port 0");
            return -EINVAL;
        }

        // Delegate to protocol socket
        return protocolSocket_->bind(ip, port);
    }

    int SocketDescriptor::connect(const void* addr, uint32_t addrlen) {
        if (!protocolSocket_) {
            LOG_ERROR("SocketDescriptor: Protocol socket not initialized");
            return -EBADF;
        }

        if (!validateSockaddr(addr, addrlen)) {
            return -EINVAL;
        }

        // Parse address
        uint32_t ip;
        uint16_t port;
        parseSockaddr(addr, ip, port);

        // Delegate to protocol socket
        return protocolSocket_->connect(ip, port);
    }

    size_t SocketDescriptor::sendto(const char* buffer, size_t length, int flags, const void* destAddr,
                                     uint32_t addrlen) {
        if (!protocolSocket_) {
            LOG_ERROR("SocketDescriptor: Protocol socket not initialized");
            return -EBADF;
        }

        uint32_t destIP;
        uint16_t destPort;

        // Determine destination
        if (destAddr && addrlen > 0) {
            // Explicit destination provided
            if (!validateSockaddr(destAddr, addrlen)) {
                return -EINVAL;
            }
            parseSockaddr(destAddr, destIP, destPort);
        } else if (protocolSocket_->isConnected()) {
            // Use connected destination
            destIP   = protocolSocket_->getRemoteIP();
            destPort = protocolSocket_->getRemotePort();
        } else {
            LOG_ERROR("SocketDescriptor: No destination specified and socket not connected");
            return -ENOTCONN;
        }

        // Delegate to protocol socket
        return protocolSocket_->sendto(buffer, length, destIP, destPort);
    }

    size_t SocketDescriptor::recvfrom(char* buffer, size_t length, int flags, void* srcAddr, uint32_t* addrlen) {
        if (!protocolSocket_) {
            LOG_ERROR("SocketDescriptor: Protocol socket not initialized");
            return -EBADF;
        }

        uint32_t srcIP   = 0;
        uint16_t srcPort = 0;

        // Delegate to protocol socket
        size_t result = protocolSocket_->recvfrom(buffer, length, &srcIP, &srcPort);

        // Fill in source address if requested
        if (result > 0 && srcAddr && addrlen) {
            buildSockaddr(srcAddr, srcIP, srcPort);
            *addrlen = sizeof(sockaddr_in);
        }

        return result;
    }

    int SocketDescriptor::setsockopt(int level, int optname, const void* optval, uint32_t optlen) {
        if (!protocolSocket_) {
            LOG_ERROR("SocketDescriptor: Protocol socket not initialized");
            return -EBADF;
        }

        return protocolSocket_->setsockopt(level, optname, optval, optlen);
    }

    int SocketDescriptor::getsockopt(int level, int optname, void* optval, uint32_t* optlen) {
        if (!protocolSocket_) {
            LOG_ERROR("SocketDescriptor: Protocol socket not initialized");
            return -EBADF;
        }

        return protocolSocket_->getsockopt(level, optname, optval, optlen);
    }

    int SocketDescriptor::getsockname(void* addr, uint32_t* addrlen) {
        if (!protocolSocket_) {
            LOG_ERROR("SocketDescriptor: Protocol socket not initialized");
            return -EBADF;
        }

        if (!addr || !addrlen) return -EINVAL;

        if (*addrlen < sizeof(sockaddr_in)) {
            return -EINVAL;
        }

        uint32_t localIP   = protocolSocket_->getLocalIP();
        uint16_t localPort = protocolSocket_->getLocalPort();

        buildSockaddr(addr, localIP, localPort);
        *addrlen = sizeof(sockaddr_in);

        return 0;
    }

    int SocketDescriptor::getpeername(void* addr, uint32_t* addrlen) {
        if (!protocolSocket_) {
            LOG_ERROR("SocketDescriptor: Protocol socket not initialized");
            return -EBADF;
        }

        if (!protocolSocket_->isConnected()) {
            return -ENOTCONN;
        }

        if (!addr || !addrlen) return -EINVAL;

        if (*addrlen < sizeof(sockaddr_in)) {
            return -EINVAL;
        }

        uint32_t remoteIP   = protocolSocket_->getRemoteIP();
        uint16_t remotePort = protocolSocket_->getRemotePort();

        buildSockaddr(addr, remoteIP, remotePort);
        *addrlen = sizeof(sockaddr_in);

        return 0;
    }

    int SocketDescriptor::listen(int backlog) {
        if (!protocolSocket_) {
            LOG_ERROR("SocketDescriptor: Protocol socket not initialized");
            return -EBADF;
        }

        return protocolSocket_->listen(backlog);
    }

    SocketDescriptor* SocketDescriptor::accept(void* addr, uint32_t* addrlen) {
        if (!protocolSocket_) {
            LOG_ERROR("SocketDescriptor: Protocol socket not initialized");
            return nullptr;
        }

        uint32_t remoteIP   = 0;
        uint16_t remotePort = 0;

        // Delegate to protocol socket
        ProtocolSocket* newProtocolSocket = protocolSocket_->accept(&remoteIP, &remotePort);
        if (!newProtocolSocket) {
            return nullptr;
        }

        // Create new SocketDescriptor wrapping the accepted socket
        // Note: We need to construct manually since we have an existing ProtocolSocket*
        // For now, return nullptr (full implementation requires refactoring constructor)
        LOG_ERROR("SocketDescriptor: accept() wrapper not yet fully implemented");
        delete newProtocolSocket;
        return nullptr;
    }

    int SocketDescriptor::shutdown(int how) {
        if (!protocolSocket_) {
            LOG_ERROR("SocketDescriptor: Protocol socket not initialized");
            return -EBADF;
        }

        return protocolSocket_->shutdown(how);
    }

    // ==================== Helper Methods ====================

    bool SocketDescriptor::validateSockaddr(const void* addr, uint32_t addrlen) const {
        if (!addr) {
            LOG_ERROR("SocketDescriptor: NULL address pointer");
            return false;
        }

        if (addrlen < sizeof(sockaddr_in)) {
            LOG_ERROR("SocketDescriptor: Address length too small (%u < %zu)", addrlen, sizeof(sockaddr_in));
            return false;
        }

        const sockaddr_in* sin = (const sockaddr_in*)addr;
        if (sin->sin_family != AF_INET) {
            LOG_ERROR("SocketDescriptor: Invalid address family %u (expected AF_INET=%d)", sin->sin_family, AF_INET);
            return false;
        }

        return true;
    }

    void SocketDescriptor::parseSockaddr(const void* addr, uint32_t& outIP, uint16_t& outPort) const {
        const sockaddr_in* sin = (const sockaddr_in*)addr;

        // Convert from network byte order to host byte order
        outIP   = ethernet::fromBigEndian32(sin->sin_addr);
        outPort = ethernet::fromBigEndian16(sin->sin_port);
    }

    void SocketDescriptor::buildSockaddr(void* addr, uint32_t ip, uint16_t port) const {
        sockaddr_in* sin = (sockaddr_in*)addr;

        sin->sin_family = AF_INET;
        sin->sin_port   = ethernet::toBigEndian16(port);
        sin->sin_addr   = ethernet::toBigEndian32(ip);
        memset(sin->sin_zero, 0, sizeof(sin->sin_zero));
    }

}  // namespace PalmyraOS::kernel
