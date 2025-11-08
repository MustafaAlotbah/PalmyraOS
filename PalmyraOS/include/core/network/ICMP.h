#pragma once

#include "core/definitions.h"

namespace PalmyraOS::kernel {

    /**
     * @brief ICMP (Internet Control Message Protocol) Implementation
     *
     * Provides ping (Echo Request/Reply) functionality.
     * ICMP is used for diagnostics, error reporting, and connectivity testing.
     *
     * Message Types:
     * - 0: Echo Reply (response to ping)
     * - 3: Destination Unreachable (error)
     * - 8: Echo Request (ping)
     * - 11: Time Exceeded (TTL expired)
     *
     * Echo Message Format (8 bytes minimum):
     *   [Type (1)] [Code (1)] [Checksum (2)] [ID (2)] [Sequence (2)]
     *   [Timestamp (4)] [Data (variable)]
     */
    class ICMP {
    public:
        // ==================== Configuration Constants ====================

        /// @brief ICMP Echo Request type
        static constexpr uint8_t TYPE_ECHO_REQUEST = 8;

        /// @brief ICMP Echo Reply type
        static constexpr uint8_t TYPE_ECHO_REPLY = 0;

        /// @brief ICMP code (always 0 for echo)
        static constexpr uint8_t CODE_ECHO = 0;

        /// @brief Maximum ping timeout (milliseconds)
        static constexpr uint32_t PING_TIMEOUT_MS = 5000;  // 5 seconds

        /// @brief Ping packet ID (simplified, non-random)
        static constexpr uint16_t PING_ID = 0x1234;

        /// @brief Minimum ICMP message size
        static constexpr size_t MIN_MESSAGE_SIZE = 8;

        // ==================== Lifecycle ====================

        /**
         * @brief Initialize ICMP subsystem
         *
         * Must be called after IPv4 is initialized.
         *
         * @return true if initialization successful
         */
        static bool initialize();

        /// @brief Check if ICMP is initialized
        [[nodiscard]] static bool isInitialized() { return initialized_; }

        // ==================== Ping Functionality ====================

        /**
         * @brief Send ping (ICMP Echo Request) to target
         *
         * Sends a ping request and waits for reply.
         *
         * **Process:**
         * 1. Build ICMP Echo Request packet
         * 2. Send via IPv4
         * 3. Wait for ICMP Echo Reply (up to PING_TIMEOUT_MS)
         * 4. Calculate and return round-trip time
         *
         * @param targetIP Destination IP address (host byte order)
         * @param outRTTms Pointer to uint32_t for result (round-trip time in ms)
         * @return true if reply received, false on timeout
         *
         * Example:
         *   uint32_t rtt;
         *   if (ICMP::ping(0x08080808, &rtt)) {  // 8.8.8.8
         *       LOG_INFO("Ping successful! RTT: %u ms", rtt);
         *   } else {
         *       LOG_WARN("Ping timeout");
         *   }
         */
        static bool ping(uint32_t targetIP, uint32_t* outRTTms);

        /**
         * @brief Send ping with custom data
         *
         * @param targetIP Destination IP address
         * @param data Custom payload data
         * @param dataLength Payload length
         * @param outRTTms Pointer to uint32_t for RTT result
         * @return true if reply received
         */
        static bool pingWithData(uint32_t targetIP, const uint8_t* data,
                                 uint32_t dataLength, uint32_t* outRTTms);

        // ==================== Packet Handling ====================

        /**
         * @brief Process incoming ICMP packet
         *
         * Called from IPv4 dispatcher when ICMP packet is received.
         * Handles Echo Requests (sends replies) and Echo Replies (updates state).
         *
         * @param payload ICMP message (after IPv4 header)
         * @param payloadLength ICMP message length
         * @param sourceIP Source IPv4 address
         * @return true if packet processed successfully
         */
        static bool handleICMPPacket(const uint8_t* payload, uint32_t payloadLength,
                                     uint32_t sourceIP);

        /**
         * @brief Send ICMP Echo Reply
         *
         * Responds to incoming ping request.
         *
         * @param destIP Destination IP (requester's IP)
         * @param id Echo ID from request
         * @param sequence Echo sequence from request
         * @param data Echo data from request
         * @param dataLength Echo data length
         * @return true if sent successfully
         */
        static bool sendEchoReply(uint32_t destIP, uint16_t id, uint16_t sequence,
                                  const uint8_t* data, uint32_t dataLength);

    private:
        // ==================== Singleton Pattern ====================
        ICMP() = delete;
        ~ICMP() = delete;
        ICMP(const ICMP&) = delete;
        ICMP& operator=(const ICMP&) = delete;

        // ==================== ICMP Message Structure ====================

        /// @brief ICMP Echo message header
        struct EchoMessage {
            uint8_t type;           ///< Message type (8=Request, 0=Reply)
            uint8_t code;           ///< Code (always 0 for echo)
            uint16_t checksum;      ///< Message checksum
            uint16_t id;            ///< Echo ID
            uint16_t sequence;      ///< Echo sequence number
            // Data follows (variable length)
        } __attribute__((packed));

        /// @brief Size of ICMP echo header (without data)
        static constexpr size_t ECHO_HEADER_SIZE = sizeof(EchoMessage);

        // ==================== Ping State ====================

        /// @brief State for pending ping reply
        struct PingState {
            uint32_t targetIP;      ///< Target IP being pinged
            uint32_t sentTime;      ///< System time when request sent
            uint16_t id;            ///< Ping ID to match replies
            uint16_t sequence;      ///< Ping sequence to match replies
            bool replyReceived;     ///< true when reply arrives
            uint32_t replyTime;     ///< System time of reply
        };

        // ==================== Static Members ====================

        /// @brief Initialization state
        static bool initialized_;

        /// @brief Current ping state (simplified: one ping at a time)
        static PingState pendingPing_;

        // ==================== Helper Methods ====================

        /**
         * @brief Calculate ICMP checksum
         *
         * One's complement sum of 16-bit words.
         *
         * @param message ICMP message (checksum field must be 0)
         * @param length Message length
         * @return Checksum value
         */
        [[nodiscard]] static uint16_t calculateChecksum(const uint8_t* message,
                                                        uint32_t length);

        /**
         * @brief Get current system time (milliseconds)
         *
         * Used for RTT calculation and timeouts.
         *
         * @return Current time in milliseconds (arbitrary base)
         */
        [[nodiscard]] static uint32_t getSystemTimeMs();
    };

}  // namespace PalmyraOS::kernel

