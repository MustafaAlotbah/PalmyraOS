#pragma once

#include "core/memory/KernelHeapAllocator.h"
#include "core/network/ProtocolSocket.h"

namespace PalmyraOS::kernel {

    /**
     * @class ICMPSocket
     * @brief Raw ICMP socket implementation (Linux SOCK_RAW + IPPROTO_ICMP compatible)
     *
     * Provides raw ICMP access for userspace ping, traceroute, etc.
     * Matches Linux raw socket behavior:
     * - Receives ALL incoming ICMP packets (broadcast to all raw ICMP sockets)
     * - No port-based demultiplexing (ICMP has no ports)
     * - Userspace provides full ICMP header + data on send
     * - Userspace receives full ICMP header + data on receive
     *
     * Differences from UDPSocket:
     * - No bind() to ports (ICMP is protocol-based, not port-based)
     * - All sockets receive copies of all ICMP packets
     * - Port parameter in sendto/recvfrom is always 0 (ignored)
     *
     * Integration:
     * - Registers in global raw socket list (not port-based like UDP)
     * - IPv4 layer delivers all ICMP packets to all registered ICMPSockets
     * - Sends ICMP via IPv4::sendPacket() directly
     */
    class ICMPSocket final : public ProtocolSocket {
    public:
        // ==================== Constructor / Destructor ====================

        /**
         * @brief Create a new raw ICMP socket
         */
        ICMPSocket();

        /**
         * @brief Destructor - cleanup resources
         */
        ~ICMPSocket() override;

        // ==================== ProtocolSocket Interface ====================

        int bind(uint32_t localIP, uint16_t localPort) override;                                        // localPort ignored
        int connect(uint32_t remoteIP, uint16_t remotePort) override;                                   // remotePort ignored
        size_t sendto(const char* buffer, size_t length, uint32_t destIP, uint16_t destPort) override;  // destPort ignored
        size_t recvfrom(char* buffer, size_t length, uint32_t* srcIP, uint16_t* srcPort) override;      // srcPort always 0
        int getBytesAvailable() const override;
        int close() override;

        [[nodiscard]] bool isBound() const override;
        [[nodiscard]] bool isConnected() const override;
        [[nodiscard]] uint32_t getLocalIP() const override;
        [[nodiscard]] uint16_t getLocalPort() const override;  // Always returns 0
        [[nodiscard]] uint32_t getRemoteIP() const override;
        [[nodiscard]] uint16_t getRemotePort() const override;  // Always returns 0

        int setsockopt(int level, int optname, const void* optval, uint32_t optlen) override;
        int getsockopt(int level, int optname, void* optval, uint32_t* optlen) override;

        // TCP-specific operations (not supported for ICMP)
        int listen(int backlog) override;
        ProtocolSocket* accept(uint32_t* remoteIP, uint16_t* remotePort) override;
        int shutdown(int how) override;

        // ==================== ICMP-Specific Options ====================

        /**
         * @brief Set non-blocking mode
         */
        void setNonBlocking(bool enabled) { nonBlocking_ = enabled; }

        /**
         * @brief Check if non-blocking mode is enabled
         */
        [[nodiscard]] bool isNonBlocking() const { return nonBlocking_; }

        /**
         * @brief Deliver incoming ICMP packet to this socket
         *
         * Called by IPv4 layer when ICMP packet arrives.
         * All registered raw ICMP sockets receive all ICMP packets (broadcast).
         *
         * @param srcIP Source IP (host byte order)
         * @param icmpData Full ICMP packet (type, code, checksum, payload)
         * @param length ICMP packet length
         */
        void deliverPacket(uint32_t srcIP, const uint8_t* icmpData, uint32_t length);

    private:
        // ==================== State ====================

        enum class State : uint8_t { Unbound, Connected };

        State state_;
        uint32_t localIP_;
        uint32_t remoteIP_;  // For connected sockets (filter by source)

        // ==================== Options ====================

        bool nonBlocking_;
        int lastError_;

        // ==================== Receive Queue ====================

        /**
         * @struct Packet
         * @brief Received ICMP packet with source info
         */
        struct Packet {
            uint32_t srcIP;  ///< Source IP (host byte order)
            uint8_t* data;   ///< ICMP packet data (heap-allocated)
            uint32_t size;   ///< Packet size in bytes

            // Constructor
            Packet() : srcIP(0), data(nullptr), size(0) {}

            // Destructor - frees data
            ~Packet();

            // Move constructor (for KQueue)
            Packet(Packet&& other) noexcept;

            // Move assignment (for KQueue)
            Packet& operator=(Packet&& other) noexcept;

            // Delete copy constructor/assignment (prevent double-free)
            Packet(const Packet&)            = delete;
            Packet& operator=(const Packet&) = delete;
        };

        // Receive queue - MUST be heap-allocated (KQueue pattern)
        KQueue<Packet>* receiveQueue_;

        static constexpr size_t MAX_QUEUE_SIZE = 64;  ///< Maximum packets in queue

        // ==================== Socket Registry (for packet delivery) ====================

        static constexpr size_t MAX_SOCKETS    = 16;
        static ICMPSocket* socketRegistry_[MAX_SOCKETS];
        static uint8_t registryCount_;

        static void registerSocket(ICMPSocket* socket);
        static void unregisterSocket(ICMPSocket* socket);

    public:
        /**
         * @brief Deliver ICMP packet to all registered raw ICMP sockets
         *
         * Called by IPv4 layer. Broadcasts to all raw ICMP sockets.
         *
         * @param srcIP Source IP
         * @param icmpData Full ICMP packet
         * @param length Packet length
         */
        static void deliverToAllSockets(uint32_t srcIP, const uint8_t* icmpData, uint32_t length);
    };

}  // namespace PalmyraOS::kernel
