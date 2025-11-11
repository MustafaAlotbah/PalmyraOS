#pragma once

#include "core/memory/KernelHeapAllocator.h"
#include "core/network/ProtocolSocket.h"

namespace PalmyraOS::kernel {

    /**
     * @class UDPSocket
     * @brief UDP protocol socket implementation
     *
     * This class resides in the network protocol layer and handles
     * all UDP-specific logic:
     * - Datagram send/receive
     * - Packet queuing
     * - UDP layer integration (callbacks)
     * - Connectionless state management
     *
     * Memory Management:
     * - Packet data is heap-allocated
     * - Receive queue is heap-allocated (KQueue pattern)
     * - Move semantics prevent double-free
     *
     * Integration with UDP layer:
     * - Registers callback with UDP::bindPort()
     * - Uses socket registry to dispatch callbacks
     */
    class UDPSocket final : public ProtocolSocket {
    public:
        // ==================== Constructor / Destructor ====================

        /**
         * @brief Create a new UDP socket
         */
        UDPSocket();

        /**
         * @brief Destructor - cleanup UDP resources
         * Unbinds port and frees receive queue
         */
        ~UDPSocket() override;

        // ==================== ProtocolSocket Interface ====================

        int bind(uint32_t localIP, uint16_t localPort) override;
        int connect(uint32_t remoteIP, uint16_t remotePort) override;
        size_t sendto(const char* buffer, size_t length, uint32_t destIP, uint16_t destPort) override;
        size_t recvfrom(char* buffer, size_t length, uint32_t* srcIP, uint16_t* srcPort) override;
        int getBytesAvailable() const override;
        int close() override;

        [[nodiscard]] bool isBound() const override;
        [[nodiscard]] bool isConnected() const override;
        [[nodiscard]] uint32_t getLocalIP() const override;
        [[nodiscard]] uint16_t getLocalPort() const override;
        [[nodiscard]] uint32_t getRemoteIP() const override;
        [[nodiscard]] uint16_t getRemotePort() const override;

        int setsockopt(int level, int optname, const void* optval, uint32_t optlen) override;
        int getsockopt(int level, int optname, void* optval, uint32_t* optlen) override;

        // TCP-specific operations (not supported for UDP)
        int listen(int backlog) override;
        ProtocolSocket* accept(uint32_t* remoteIP, uint16_t* remotePort) override;
        int shutdown(int how) override;

        // ==================== UDP-Specific Options ====================

        /**
         * @brief Set non-blocking mode
         */
        void setNonBlocking(bool enabled) { nonBlocking_ = enabled; }

        /**
         * @brief Check if non-blocking mode is enabled
         */
        [[nodiscard]] bool isNonBlocking() const { return nonBlocking_; }

    private:
        // ==================== State ====================

        enum class State : uint8_t { Unbound, Bound, Connected };

        State state_;
        uint32_t localIP_;
        uint16_t localPort_;
        uint32_t remoteIP_;
        uint16_t remotePort_;

        // ==================== Options ====================

        bool nonBlocking_;
        bool reuseAddr_;
        bool broadcast_;
        int lastError_;

        // ==================== Receive Queue ====================

        /**
         * @struct Packet
         * @brief Received UDP datagram with source info
         */
        struct Packet {
            uint32_t srcIP;    ///< Source IP (host byte order)
            uint16_t srcPort;  ///< Source port (host byte order)
            uint8_t* data;     ///< Payload data (heap-allocated)
            uint32_t size;     ///< Payload size in bytes

            // Constructor
            Packet() : srcIP(0), srcPort(0), data(nullptr), size(0) {}

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

        // ==================== Callback System ====================

        /**
         * @brief Handle incoming UDP packet
         * Called from UDP layer callback
         */
        void handleIncomingPacket(uint32_t srcIP, uint16_t srcPort, const uint8_t* data, uint32_t length);

        /**
         * @brief Static trampoline for UDP callbacks
         * Routes packet to correct UDPSocket instance
         */
        static void udpReceiveCallbackTrampoline(uint32_t srcIP, uint16_t srcPort, const uint8_t* data, uint32_t length);

        // ==================== Socket Registry (for callback dispatch) ====================

        static constexpr size_t MAX_SOCKETS = 16;
        static UDPSocket* socketRegistry_[MAX_SOCKETS];
        static uint16_t registryPorts_[MAX_SOCKETS];
        static uint8_t registryCount_;

        static void registerSocket(uint16_t port, UDPSocket* socket);
        static void unregisterSocket(uint16_t port);
        static UDPSocket* findSocket(uint16_t port);
    };

}  // namespace PalmyraOS::kernel

