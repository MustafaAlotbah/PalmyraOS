#pragma once

#include "core/definitions.h"

namespace PalmyraOS::kernel {

    /**
     * @brief Ethernet Protocol Headers and Constants
     *
     * Defines the fundamental structure of Ethernet frames and protocol values
     * used throughout the network stack for frame identification and routing.
     *
     * Standard Ethernet Frame Format:
     *   [Dest MAC (6)] [Src MAC (6)] [EtherType (2)] [Payload (46-1500)] [FCS (4)]
     *   Total: 64-1518 bytes (including FCS)
     */
    namespace ethernet {

        // ==================== Ethernet Frame Structure ====================

        /// @brief Destination MAC address length (bytes)
        static constexpr size_t MAC_ADDRESS_SIZE = 6;

        /// @brief Ethernet frame header (before payload)
        struct FrameHeader {
            uint8_t destMAC[MAC_ADDRESS_SIZE];   ///< Destination MAC address
            uint8_t srcMAC[MAC_ADDRESS_SIZE];    ///< Source MAC address
            uint16_t etherType;                  ///< Protocol identifier (big-endian)
        } __attribute__((packed));

        /// @brief Size of Ethernet frame header (14 bytes)
        static constexpr size_t HEADER_SIZE = sizeof(FrameHeader);

        /// @brief Minimum payload size (46 bytes)
        static constexpr uint16_t MIN_PAYLOAD_SIZE = 46;

        /// @brief Maximum payload size (1500 bytes, standard MTU)
        static constexpr uint16_t MAX_PAYLOAD_SIZE = 1500;

        /// @brief Minimum frame size (header + min payload)
        static constexpr uint16_t MIN_FRAME_SIZE = HEADER_SIZE + MIN_PAYLOAD_SIZE;

        /// @brief Maximum frame size (header + max payload)
        static constexpr uint16_t MAX_FRAME_SIZE = HEADER_SIZE + MAX_PAYLOAD_SIZE;

        // ==================== EtherType Values ====================

        /// @brief IPv4 protocol
        static constexpr uint16_t ETHERTYPE_IPV4 = 0x0800;

        /// @brief Address Resolution Protocol
        static constexpr uint16_t ETHERTYPE_ARP = 0x0806;

        /// @brief IPv6 protocol
        static constexpr uint16_t ETHERTYPE_IPV6 = 0x86DD;

        // ==================== Broadcast MAC Address ====================

        /// @brief Broadcast MAC address (FF:FF:FF:FF:FF:FF)
        static constexpr uint8_t BROADCAST_MAC[MAC_ADDRESS_SIZE] = {
            0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF
        };

        // ==================== ARP-related Constants ====================

        /// @brief Ethernet hardware type for ARP (RFC 1340)
        static constexpr uint16_t HARDWARE_TYPE_ETHERNET = 1;

        /// @brief IPv4 protocol type (for ARP)
        static constexpr uint16_t PROTOCOL_TYPE_IPV4 = 0x0800;

        /// @brief IPv4 address size (bytes)
        static constexpr uint8_t IPV4_ADDRESS_SIZE = 4;

        /**
         * @brief Check if MAC address is broadcast
         *
         * @param mac MAC address to check (6 bytes)
         * @return true if all bytes are 0xFF
         */
        inline bool isBroadcastMAC(const uint8_t* mac) {
            for (size_t i = 0; i < MAC_ADDRESS_SIZE; ++i) {
                if (mac[i] != 0xFF) return false;
            }
            return true;
        }

        /**
         * @brief Convert 16-bit value to big-endian for EtherType
         *
         * @param value Native endian value
         * @return Big-endian value suitable for EtherType field
         */
        inline uint16_t toBigEndian16(uint16_t value) {
            return ((value & 0xFF) << 8) | ((value >> 8) & 0xFF);
        }

        /**
         * @brief Convert 16-bit big-endian to native endian
         *
         * @param value Big-endian value from frame
         * @return Native endian value
         */
        inline uint16_t fromBigEndian16(uint16_t value) {
            return toBigEndian16(value);  // Symmetric operation
        }

    }  // namespace ethernet

}  // namespace PalmyraOS::kernel

