#pragma once

#include "core/network/ProtocolSocket.h"
#include "core/tasks/Descriptor.h"
#include <cstdint>  // uint32_t, uint16_t

namespace PalmyraOS::kernel {

    /**
     * @class SocketDescriptor
     * @brief Descriptor-layer wrapper for protocol-specific sockets
     *
     * This class sits in the descriptor/file system layer and provides
     * a thin wrapper around protocol-specific socket implementations (UDP, TCP).
     *
     * Architecture (Proper Layer Separation):
     *   DescriptorTable (descriptor table)
     *        ↓
     *   SocketDescriptor (descriptor layer) - THIS CLASS
     *        ↓ (delegates to)
     *   ProtocolSocket (protocol layer)
     *        ↓ (implemented by)
     *   UDPSocket | TCPSocket (protocol implementations)
     *
     * Design Goals:
     * - Linux compatibility: Sockets are file descriptors
     * - Proper layer separation: Descriptor layer stays protocol-agnostic
     * - Delegation pattern: All protocol logic lives in ProtocolSocket subclasses
     * - Standard syscalls work: read()/write()/ioctl()/close()
     *
     * Memory Management:
     * - SocketDescriptor owns the ProtocolSocket*
     * - ProtocolSocket* is allocated via heapManager.createInstance<>()
     * - Destructor deletes ProtocolSocket*
     */
    class SocketDescriptor final : public Descriptor {
    public:
        // ==================== Constructor / Destructor ====================

        /**
         * @brief Create a new socket
         * @param domain Address family (AF_INET)
         * @param type Socket type (SOCK_STREAM, SOCK_DGRAM)
         * @param protocol Protocol (0 = auto, IPPROTO_TCP, IPPROTO_UDP)
         *
         * Corresponds to socket() syscall.
         * Creates appropriate ProtocolSocket implementation based on type.
         */
        explicit SocketDescriptor(int domain, int type, int protocol);

        /**
         * @brief Destructor - cleanup socket resources
         * Deletes ProtocolSocket*
         */
        ~SocketDescriptor() override;

        // ==================== Descriptor Interface (Inherited) ====================

        /**
         * @brief Identify this as a Socket descriptor
         */
        [[nodiscard]] Kind kind() const override;

        /**
         * @brief Read data from socket (like recv)
         * @param buffer Buffer to store data
         * @param size Number of bytes to read
         * @return Bytes read, 0 on EOF, or negative error code
         *
         * Delegates to ProtocolSocket::recvfrom()
         */
        size_t read(char* buffer, size_t size) override;

        /**
         * @brief Write data to socket (like send)
         * @param buffer Data to write
         * @param size Number of bytes to write
         * @return Bytes written or negative error code
         *
         * Must be connected. Delegates to ProtocolSocket::sendto()
         */
        size_t write(const char* buffer, size_t size) override;

        /**
         * @brief Socket ioctl operations
         * @param request ioctl command
         * @param arg Command-specific argument
         * @return 0 on success, negative error code on failure
         *
         * Supported commands:
         * - FIONBIO: Set/clear non-blocking mode
         * - FIONREAD: Get bytes available to read
         */
        int ioctl(int request, void* arg) override;

        // ==================== Socket-Specific Operations ====================

        /**
         * @brief Bind socket to local address and port
         * @param addr sockaddr_in structure with IP and port
         * @param addrlen Size of addr structure
         * @return 0 on success, negative error code on failure
         */
        int bind(const void* addr, uint32_t addrlen);

        /**
         * @brief Connect to remote address
         * @param addr sockaddr_in structure with remote IP and port
         * @param addrlen Size of addr structure
         * @return 0 on success, negative error code on failure
         */
        int connect(const void* addr, uint32_t addrlen);

        /**
         * @brief Send data to specific destination
         * @param buffer Data to send
         * @param length Length of data
         * @param flags Message flags (MSG_DONTWAIT, etc.)
         * @param destAddr Destination address
         * @param addrlen Size of destAddr
         * @return Bytes sent or negative error code
         */
        size_t sendto(const char* buffer, size_t length, int flags, const void* destAddr, uint32_t addrlen);

        /**
         * @brief Receive data from socket
         * @param buffer Buffer to store data
         * @param length Buffer size
         * @param flags Message flags (MSG_PEEK, etc.)
         * @param srcAddr Output: Source address
         * @param addrlen Output: Size of srcAddr
         * @return Bytes received or negative error code
         */
        size_t recvfrom(char* buffer, size_t length, int flags, void* srcAddr, uint32_t* addrlen);

        /**
         * @brief Set socket option
         */
        int setsockopt(int level, int optname, const void* optval, uint32_t optlen);

        /**
         * @brief Get socket option
         */
        int getsockopt(int level, int optname, void* optval, uint32_t* optlen);

        /**
         * @brief Get local socket address
         */
        int getsockname(void* addr, uint32_t* addrlen);

        /**
         * @brief Get remote socket address
         */
        int getpeername(void* addr, uint32_t* addrlen);

        // ==================== TCP Operations (Future) ====================

        /**
         * @brief Mark socket as passive (TCP only)
         */
        int listen(int backlog);

        /**
         * @brief Accept incoming connection (TCP only)
         */
        SocketDescriptor* accept(void* addr, uint32_t* addrlen);

        /**
         * @brief Shutdown socket (TCP only)
         */
        int shutdown(int how);

    private:
        // ==================== Descriptor Configuration ====================

        int domain_;    ///< AF_INET, AF_INET6 (for validation)
        int type_;      ///< SOCK_STREAM, SOCK_DGRAM (for validation)
        int protocol_;  ///< IPPROTO_TCP, IPPROTO_UDP, 0 (for validation)

        // ==================== Protocol Implementation ====================

        ProtocolSocket* protocolSocket_;  ///< Protocol-specific socket implementation (owned)

        // ==================== Helper Methods ====================

        /**
         * @brief Validate sockaddr structure
         * @return true if valid AF_INET address
         */
        bool validateSockaddr(const void* addr, uint32_t addrlen) const;

        /**
         * @brief Parse sockaddr_in to IP and port (network -> host byte order)
         * @param addr sockaddr_in structure
         * @param outIP Output: IP address (host byte order)
         * @param outPort Output: Port (host byte order)
         */
        void parseSockaddr(const void* addr, uint32_t& outIP, uint16_t& outPort) const;

        /**
         * @brief Build sockaddr_in from IP and port (host -> network byte order)
         * @param addr Output: sockaddr_in structure
         * @param ip IP address (host byte order)
         * @param port Port (host byte order)
         */
        void buildSockaddr(void* addr, uint32_t ip, uint16_t port) const;
    };

}  // namespace PalmyraOS::kernel
