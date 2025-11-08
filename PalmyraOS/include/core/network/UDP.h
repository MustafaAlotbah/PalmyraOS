#pragma once

#include "core/definitions.h"

namespace PalmyraOS::kernel {

    /**
     * @brief UDP (User Datagram Protocol) Implementation
     *
     * Provides connectionless, unreliable datagram transmission.
     * UDP is used for DNS queries, DHCP, NTP, streaming media, and other
     * applications where low latency is more important than guaranteed delivery.
     *
     * **Characteristics:**
     * - Stateless (no connection setup/teardown)
     * - Unreliable (no ACKs, no retransmission)
     * - Unordered (packets can arrive out of sequence)
     * - Low overhead (8-byte header)
     *
     * **Common UDP Ports:**
     * - 53: DNS (Domain Name System)
     * - 67/68: DHCP (Dynamic Host Configuration Protocol)
     * - 123: NTP (Network Time Protocol)
     * - 161/162: SNMP (Simple Network Management Protocol)
     *
     * **UDP Datagram Format:**
     *   [UDP Header (8)] [Payload (variable)]
     *
     * @see IPv4 (transport layer)
     * @see DNS (application using UDP)
     */
    class UDP {
    public:
        // ==================== Configuration Constants ====================

        /// @brief Maximum UDP datagram size (64KB - IP header - UDP header)
        static constexpr uint32_t MAX_DATAGRAM_SIZE  = 65507;

        /// @brief UDP header size (fixed at 8 bytes)
        static constexpr size_t HEADER_SIZE          = 8;

        /// @brief Maximum number of bound ports (socket-like functionality)
        static constexpr uint8_t MAX_BOUND_PORTS     = 16;

        /// @brief Dynamic port range start (ephemeral ports)
        static constexpr uint16_t DYNAMIC_PORT_START = 49152;

        /// @brief Dynamic port range end
        static constexpr uint16_t DYNAMIC_PORT_END   = 65535;

        // ==================== Well-Known Port Numbers ====================

        /// @brief DNS port (Domain Name System)
        static constexpr uint16_t PORT_DNS           = 53;

        /// @brief DHCP client port
        static constexpr uint16_t PORT_DHCP_CLIENT   = 68;

        /// @brief DHCP server port
        static constexpr uint16_t PORT_DHCP_SERVER   = 67;

        /// @brief NTP port (Network Time Protocol)
        static constexpr uint16_t PORT_NTP           = 123;

        // ==================== Lifecycle ====================

        /**
         * @brief Initialize UDP subsystem
         *
         * Sets up port binding table and initializes state.
         * Must be called after IPv4 is initialized.
         *
         * @return true if initialization successful
         */
        static bool initialize();

        /// @brief Check if UDP is initialized
        [[nodiscard]] static bool isInitialized() { return initialized_; }

        // ==================== Datagram Transmission ====================

        /**
         * @brief Send UDP datagram
         *
         * Transmits a UDP datagram to the specified destination.
         *
         * **Process:**
         * 1. Build UDP header (source port, dest port, length, checksum)
         * 2. Calculate pseudo-header checksum (includes IP addresses)
         * 3. Send via IPv4 layer
         *
         * @param destinationIP Destination IPv4 address (host byte order)
         * @param destinationPort Destination UDP port (1-65535)
         * @param sourcePort Source UDP port (0 = auto-allocate ephemeral port)
         * @param data Payload data
         * @param dataLength Payload length in bytes
         * @return true if sent successfully, false on error
         *
         * @note Maximum payload: 65507 bytes (65535 - 20 IP header - 8 UDP header)
         * @note Checksum is optional but recommended (0 = no checksum)
         *
         * Example:
         *   uint8_t dnsQuery[32];
         *   // ... build DNS query ...
         *   UDP::sendDatagram(0x08080808, 53, 12345, dnsQuery, 32);
         */
        static bool sendDatagram(uint32_t destinationIP, uint16_t destinationPort, uint16_t sourcePort, const uint8_t* data, uint32_t dataLength);

        // ==================== Packet Handling ====================

        /**
         * @brief Process incoming UDP datagram
         *
         * Called from IPv4 dispatcher when UDP packet is received (protocol 17).
         * Validates header, checks checksum, and dispatches to port handler.
         *
         * @param payload UDP datagram (UDP header + data, after IPv4 header)
         * @param payloadLength Total UDP datagram length
         * @param sourceIP Source IPv4 address (host byte order)
         * @param destinationIP Destination IPv4 address (host byte order, for checksum)
         * @return true if packet processed successfully
         *
         * @note Called from interrupt context - should be fast
         */
        static bool handleUDPPacket(const uint8_t* payload, uint32_t payloadLength, uint32_t sourceIP, uint32_t destinationIP);

        // ==================== Port Management ====================

        /**
         * @brief Bind UDP port to handler
         *
         * Associates a UDP port with a callback function for incoming datagrams.
         * Used for implementing UDP "sockets" or service handlers.
         *
         * @param port Port number to bind (1-65535)
         * @param handler Callback function (sourceIP, sourcePort, data, length)
         * @return true if bound successfully, false if port already bound or table full
         *
         * Example:
         *   UDP::bindPort(53, [](uint32_t srcIP, uint16_t srcPort,
         *                        const uint8_t* data, uint32_t len) {
         *       DNS::handleResponse(data, len, srcIP);
         *   });
         */
        using DatagramHandler = void (*)(uint32_t sourceIP, uint16_t sourcePort, const uint8_t* data, uint32_t dataLength);
        static bool bindPort(uint16_t port, DatagramHandler handler);

        /**
         * @brief Unbind UDP port
         *
         * Removes port binding, freeing the port for reuse.
         *
         * @param port Port number to unbind
         * @return true if unbound successfully
         */
        static bool unbindPort(uint16_t port);

        /**
         * @brief Allocate ephemeral port
         *
         * Finds an unused port in the dynamic range (49152-65535) for outgoing datagrams.
         *
         * @return Port number, or 0 if no ports available
         */
        [[nodiscard]] static uint16_t allocateEphemeralPort();

    private:
        // ==================== Singleton Pattern ====================
        UDP()                      = delete;
        ~UDP()                     = delete;
        UDP(const UDP&)            = delete;
        UDP& operator=(const UDP&) = delete;

        // ==================== UDP Header Structure ====================

        /// @brief UDP datagram header (RFC 768)
        struct Header {
            uint16_t sourcePort;       ///< Source port (big-endian)
            uint16_t destinationPort;  ///< Destination port (big-endian)
            uint16_t length;           ///< Length of UDP header + data (big-endian)
            uint16_t checksum;         ///< Checksum (optional, 0 = no checksum, big-endian)
        } __attribute__((packed));

        /// @brief Size of UDP header
        static constexpr size_t HEADER_SIZE_BYTES = sizeof(Header);

        // ==================== Port Binding ====================

        /// @brief Port binding entry (associates port with handler)
        struct PortBinding {
            uint16_t port;            ///< Bound port number
            DatagramHandler handler;  ///< Callback function for incoming datagrams
            bool active;              ///< Binding is active
        };

        // ==================== Static Members ====================

        /// @brief Initialization state
        static bool initialized_;

        /// @brief Next ephemeral port to allocate (increments on each allocation)
        static uint16_t nextEphemeralPort_;

        /// @brief Port binding table
        static PortBinding portBindings_[MAX_BOUND_PORTS];

        /// @brief Number of active port bindings
        static uint8_t boundPortCount_;

        // ==================== Helper Methods ====================

        /**
         * @brief Calculate UDP checksum with pseudo-header
         *
         * UDP checksum includes a "pseudo-header" containing IP addresses
         * to detect misrouted packets.
         *
         * Pseudo-header format:
         *   Source IP (4) + Dest IP (4) + Zero (1) + Protocol (1) + UDP Length (2)
         *   = 12 bytes
         *
         * Checksum = One's complement sum of:
         *   - Pseudo-header (12 bytes)
         *   - UDP header (8 bytes, checksum field = 0)
         *   - UDP data (variable)
         *
         * @param sourceIP Source IPv4 address (host byte order)
         * @param destinationIP Destination IPv4 address (host byte order)
         * @param udpHeader UDP header (checksum field must be 0)
         * @param data UDP payload data
         * @param dataLength UDP payload length
         * @return Checksum value (big-endian, ready to write to header)
         */
        [[nodiscard]] static uint16_t
        calculateChecksumWithPseudoHeader(uint32_t sourceIP, uint32_t destinationIP, const Header* udpHeader, const uint8_t* data, uint32_t dataLength);

        /**
         * @brief Find port binding by port number
         *
         * @param port Port number to search
         * @return Pointer to binding, or nullptr if not found
         */
        static PortBinding* findPortBinding(uint16_t port);
    };

}  // namespace PalmyraOS::kernel
