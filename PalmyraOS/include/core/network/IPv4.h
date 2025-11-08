#pragma once

#include "core/definitions.h"
#include "core/network/Ethernet.h"

namespace PalmyraOS::kernel {

    /**
     * @brief IPv4 Internet Protocol Implementation
     *
     * Handles IPv4 packet parsing, routing, and forwarding.
     * Provides the foundation for UDP, TCP, and ICMP protocols.
     *
     * IPv4 Header Format (20 bytes minimum):
     *   [Version/IHL (1)] [DSCP/ECN (1)] [Total Length (2)]
     *   [Identification (2)] [Flags/Fragment Offset (2)]
     *   [TTL (1)] [Protocol (1)] [Checksum (2)]
     *   [Source IP (4)] [Destination IP (4)]
     *   [Options (variable)] [Payload (variable)]
     */
    class IPv4 {
    public:
        // ==================== Configuration Constants ====================

        /// @brief IPv4 version number (always 4)
        static constexpr uint8_t VERSION = 4;

        /// @brief Default Time-To-Live (max hops)
        static constexpr uint8_t DEFAULT_TTL = 64;

        /// @brief IPv4 header size without options (20 bytes)
        static constexpr size_t HEADER_SIZE = 20;

        /// @brief IPv4 protocol: ICMP (Internet Control Message Protocol)
        static constexpr uint8_t PROTOCOL_ICMP = 1;

        /// @brief IPv4 protocol: TCP (Transmission Control Protocol)
        static constexpr uint8_t PROTOCOL_TCP = 6;

        /// @brief IPv4 protocol: UDP (User Datagram Protocol)
        static constexpr uint8_t PROTOCOL_UDP = 17;

        // ==================== Lifecycle ====================

        /**
         * @brief Initialize IPv4 subsystem
         *
         * Must be called after network interface is up.
         *
         * @param localIP Local IPv4 address (host byte order)
         * @param subnetMask Subnet mask (host byte order)
         * @param gateway Default gateway IP (host byte order)
         * @return true if initialization successful
         */
        static bool initialize(uint32_t localIP, uint32_t subnetMask, uint32_t gateway);

        /// @brief Check if IPv4 is initialized
        [[nodiscard]] static bool isInitialized() { return initialized_; }

        // ==================== Packet Processing ====================

        /**
         * @brief Process incoming IPv4 packet
         *
         * Called from Ethernet dispatcher when IPv4 frame is received.
         * Validates header, checks TTL, routes packet to appropriate handler.
         *
         * @param frame Complete Ethernet frame (including Ethernet header)
         * @param frameLength Total frame length
         * @return true if packet processed successfully
         */
        static bool handleIPv4Packet(const uint8_t* frame, uint32_t frameLength);

        /**
         * @brief Send IPv4 packet
         *
         * Wraps payload in IPv4 header and sends via Ethernet.
         * Handles ARP for MAC address resolution if needed.
         *
         * @param destIP Destination IP address (host byte order)
         * @param protocol Protocol number (ICMP=1, TCP=6, UDP=17)
         * @param payload Packet payload (after IPv4 header)
         * @param payloadLength Payload length in bytes
         * @return true if sent successfully
         */
        static bool sendPacket(uint32_t destIP, uint8_t protocol,
                               const uint8_t* payload, uint32_t payloadLength);

        // ==================== Address Information ====================

        /// @brief Get local IPv4 address
        [[nodiscard]] static uint32_t getLocalIP() { return localIP_; }

        /// @brief Get subnet mask
        [[nodiscard]] static uint32_t getSubnetMask() { return subnetMask_; }

        /// @brief Get default gateway
        [[nodiscard]] static uint32_t getGateway() { return gateway_; }

        // ==================== Utility Functions ====================

        /**
         * @brief Check if IP address is on local network
         *
         * Compares destination IP with subnet to determine if routing
         * is direct (ARP for MAC) or via gateway.
         *
         * @param ip IP address to check (host byte order)
         * @return true if on same subnet as local IP
         */
        [[nodiscard]] static bool isLocalAddress(uint32_t ip);

        /**
         * @brief Calculate IPv4 checksum
         *
         * One's complement sum of 16-bit words in header.
         *
         * @param header IPv4 header (20 bytes, checksum field must be 0)
         * @return Checksum value (ready to write to header)
         */
        [[nodiscard]] static uint16_t calculateChecksum(const uint8_t* header);

    private:
        // ==================== Singleton Pattern ====================
        IPv4() = delete;
        ~IPv4() = delete;
        IPv4(const IPv4&) = delete;
        IPv4& operator=(const IPv4&) = delete;

        // ==================== IPv4 Header Structure ====================

        /// @brief IPv4 packet header
        struct Header {
            uint8_t versionAndIHL;      ///< Version (4 bits) + IHL (4 bits)
            uint8_t dscpAndECN;         ///< DSCP (6 bits) + ECN (2 bits)
            uint16_t totalLength;       ///< Total packet length (header + payload)
            uint16_t identification;    ///< Packet ID for fragmentation
            uint16_t flagsAndOffset;    ///< Flags (3 bits) + Fragment Offset (13 bits)
            uint8_t ttl;                ///< Time To Live
            uint8_t protocol;           ///< Protocol number (ICMP=1, TCP=6, UDP=17)
            uint16_t checksum;          ///< Header checksum
            uint32_t sourceIP;          ///< Source IPv4 address
            uint32_t destIP;            ///< Destination IPv4 address
        } __attribute__((packed));

        /// @brief Size of IPv4 header
        static constexpr size_t HEADER_SIZE_BYTES = sizeof(Header);

        // ==================== Static Members ====================

        /// @brief Initialization state
        static bool initialized_;

        /// @brief Local IPv4 address
        static uint32_t localIP_;

        /// @brief Subnet mask
        static uint32_t subnetMask_;

        /// @brief Default gateway address
        static uint32_t gateway_;

        // ==================== Helper Methods ====================

        /**
         * @brief Route packet to appropriate handler
         *
         * Based on protocol field, dispatch to ICMP, UDP, TCP handlers.
         *
         * @param header IPv4 header
         * @param payload Packet payload (after IPv4 header)
         * @param payloadLength Payload length
         * @return true if handled successfully
         */
        static bool routePacket(uint32_t sourceIP, uint8_t protocol,
                                const uint8_t* payload, uint32_t payloadLength);

        /**
         * @brief Convert 32-bit IP address to dotted decimal string
         *
         * @param ip IP address (host byte order)
         * @param buffer Output buffer (at least 16 bytes)
         * @return Pointer to buffer
         */
        static const char* ipToString(uint32_t ip, char* buffer);
    };

}  // namespace PalmyraOS::kernel

