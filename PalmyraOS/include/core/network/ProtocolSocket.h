#pragma once

#include <cstddef>  // size_t
#include <cstdint>  // uint32_t

namespace PalmyraOS::kernel {

    /**
     * @class ProtocolSocket
     * @brief Abstract base class for protocol-specific socket implementations
     *
     * This class sits in the network protocol layer and provides a uniform
     * interface for different socket protocols (UDP, TCP, etc.).
     *
     * Architecture:
     *   SocketDescriptor (descriptor layer) -> ProtocolSocket (protocol layer)
     *                                              \/
     *                               UDPSocket | TCPSocket | ICMPSocket
     *
     * Each protocol implements its own state management, packet handling,
     * and protocol-specific operations.
     */
    class ProtocolSocket {
    public:
        virtual ~ProtocolSocket() = default;

        // ==================== Memory Management (Freestanding C++) ====================
        static void* operator new(size_t size);
        static void* operator new(size_t size, void* ptr) noexcept;
        static void operator delete(void* ptr) noexcept;
        static void operator delete(void* ptr, size_t size) noexcept;

        // ==================== Core Operations ====================

        /**
         * @brief Bind socket to local address and port
         * @param localIP Local IP address (host byte order)
         * @param localPort Local port (host byte order)
         * @return 0 on success, negative error code on failure
         */
        virtual int bind(uint32_t localIP, uint16_t localPort)                                       = 0;

        /**
         * @brief Connect to remote address
         * @param remoteIP Remote IP address (host byte order)
         * @param remotePort Remote port (host byte order)
         * @return 0 on success, negative error code on failure
         */
        virtual int connect(uint32_t remoteIP, uint16_t remotePort)                                  = 0;

        /**
         * @brief Send data to a specific destination
         * @param buffer Data to send
         * @param length Length of data
         * @param destIP Destination IP (host byte order)
         * @param destPort Destination port (host byte order)
         * @return Bytes sent or negative error code
         */
        virtual size_t sendto(const char* buffer, size_t length, uint32_t destIP, uint16_t destPort) = 0;

        /**
         * @brief Receive data from socket
         * @param buffer Buffer to store data
         * @param length Buffer size
         * @param srcIP Output: Source IP (host byte order)
         * @param srcPort Output: Source port (host byte order)
         * @return Bytes received or negative error code
         */
        virtual size_t recvfrom(char* buffer, size_t length, uint32_t* srcIP, uint16_t* srcPort)     = 0;

        /**
         * @brief Get number of bytes available to read
         * @return Bytes available or negative error code
         */
        virtual int getBytesAvailable() const                                                        = 0;

        /**
         * @brief Close the socket
         * @return 0 on success, negative error code on failure
         */
        virtual int close()                                                                          = 0;

        // ==================== State Queries ====================

        /**
         * @brief Check if socket is bound
         */
        [[nodiscard]] virtual bool isBound() const                                                   = 0;

        /**
         * @brief Check if socket is connected
         */
        [[nodiscard]] virtual bool isConnected() const                                               = 0;

        /**
         * @brief Get local IP address
         * @return Local IP (host byte order)
         */
        [[nodiscard]] virtual uint32_t getLocalIP() const                                            = 0;

        /**
         * @brief Get local port
         * @return Local port (host byte order)
         */
        [[nodiscard]] virtual uint16_t getLocalPort() const                                          = 0;

        /**
         * @brief Get remote IP address
         * @return Remote IP (host byte order), 0 if not connected
         */
        [[nodiscard]] virtual uint32_t getRemoteIP() const                                           = 0;

        /**
         * @brief Get remote port
         * @return Remote port (host byte order), 0 if not connected
         */
        [[nodiscard]] virtual uint16_t getRemotePort() const                                         = 0;

        // ==================== Options ====================

        /**
         * @brief Set socket option
         * @param level Option level (SOL_SOCKET, etc.)
         * @param optname Option name
         * @param optval Option value
         * @param optlen Option length
         * @return 0 on success, negative error code on failure
         */
        virtual int setsockopt(int level, int optname, const void* optval, uint32_t optlen)          = 0;

        /**
         * @brief Get socket option
         * @param level Option level
         * @param optname Option name
         * @param optval Output: Option value
         * @param optlen Input/Output: Option length
         * @return 0 on success, negative error code on failure
         */
        virtual int getsockopt(int level, int optname, void* optval, uint32_t* optlen)               = 0;

        // ==================== TCP-Specific (optional, can return -EOPNOTSUPP) ====================

        /**
         * @brief Listen for incoming connections (TCP only)
         * @param backlog Maximum queue length
         * @return 0 on success, negative error code on failure
         */
        virtual int listen(int backlog)                                                              = 0;

        /**
         * @brief Accept incoming connection (TCP only)
         * @param remoteIP Output: Remote IP
         * @param remotePort Output: Remote port
         * @return New ProtocolSocket* or nullptr on error
         */
        virtual ProtocolSocket* accept(uint32_t* remoteIP, uint16_t* remotePort)                     = 0;

        /**
         * @brief Shutdown socket (TCP only)
         * @param how SHUT_RD, SHUT_WR, or SHUT_RDWR
         * @return 0 on success, negative error code on failure
         */
        virtual int shutdown(int how)                                                                = 0;

    protected:
        ProtocolSocket() = default;

    private:
        // Prevent copying
        ProtocolSocket(const ProtocolSocket&)            = delete;
        ProtocolSocket& operator=(const ProtocolSocket&) = delete;
    };

}  // namespace PalmyraOS::kernel
