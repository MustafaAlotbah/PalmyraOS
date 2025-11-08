#pragma once

#include "core/definitions.h"
#include "core/network/Ethernet.h"

namespace PalmyraOS::kernel {

    /**
     * @brief Address Resolution Protocol (ARP) Implementation
     *
     * ARP resolves IPv4 addresses to MAC addresses on local networks.
     * Essential for Ethernet frame delivery - without ARP, we can't send frames
     * to other devices on the same network segment.
     *
     * **ARP Message Types:**
     * - REQUEST: "Who has IP X.X.X.X?" (broadcast to all)
     * - REPLY: "I have IP X.X.X.X, my MAC is YY:YY:YY:YY:YY:YY" (unicast)
     *
     * **Usage Pattern:**
     *   // Once at boot
     *   ARP::initialize(192.168.1.101, eth0_mac_address);
     *
     *   // When sending to IP (need MAC address)
     *   uint8_t destMAC[6];
     *   if (ARP::resolveMacAddress(192.168.1.1, destMAC)) {
     *       // Send frame to destMAC
     *   } else {
     *       // ARP cache miss - send ARP request and retry
     *   }
     *
     * **ARP Cache:**
     * - Automatic population on ARP replies
     * - Manual entry via resolveMacAddress() trigger
     * - Entries cached to avoid repeated ARP requests
     */
    class ARP {
    public:
        // ==================== Configuration Constants ====================

        /// @brief Maximum ARP cache entries
        static constexpr uint8_t MAX_CACHE_ENTRIES      = 32;

        /// @brief ARP timeout in seconds (entry validity period)
        static constexpr uint32_t CACHE_TIMEOUT_SECONDS = 300;  // 5 minutes

        /// @brief ARP request timeout in milliseconds (max wait for reply)
        static constexpr uint32_t REQUEST_TIMEOUT_MS    = 3000;  // 3 seconds

        /// @brief Maximum ARP request retries before giving up
        static constexpr uint8_t MAX_REQUEST_RETRIES    = 3;

        // ==================== ARP Operation Types ====================

        /// @brief ARP Request operation
        static constexpr uint16_t OPERATION_REQUEST     = 1;

        /// @brief ARP Reply operation
        static constexpr uint16_t OPERATION_REPLY       = 2;

        // ==================== Lifecycle ====================

        /**
         * @brief Initialize ARP subsystem
         *
         * Must be called once at system startup, after network interface is up.
         * Sets up the local MAC and IP address used for ARP operations.
         *
         * @param localIP Local IPv4 address in host byte order (e.g., 0xC0A80165 = 192.168.1.101)
         * @param localMAC Pointer to 6-byte local MAC address
         * @return true if initialization successful, false otherwise
         *
         * @note Called from kernelEntry.cpp during network initialization
         */
        static bool initialize(uint32_t localIP, const uint8_t* localMAC);

        /// @brief Check if ARP subsystem is initialized
        [[nodiscard]] static bool isInitialized() { return initialized_; }

        // ==================== ARP Cache Management ====================

        /**
         * @brief Resolve IPv4 address to MAC address
         *
         * Queries ARP cache for the MAC address of a given IPv4 address.
         * If not cached, sends ARP request and waits for reply.
         *
         * **Process:**
         * 1. Check ARP cache for entry
         * 2. If cached and not expired, return MAC address (fast path)
         * 3. If not cached or expired:
         *    a. Send ARP request (broadcast)
         *    b. Wait for ARP reply (up to REQUEST_TIMEOUT_MS)
         *    c. Cache the reply
         *    d. Return MAC address
         * 4. If timeout, retry up to MAX_REQUEST_RETRIES times
         * 5. If all retries fail, return false
         *
         * @param ipAddress IPv4 address to resolve (host byte order)
         * @param outMAC Pointer to 6-byte buffer for MAC address result
         * @return true if resolution successful (MAC address in outMAC), false on timeout
         *
         * @note This is a blocking operation (may take up to 3+ seconds on timeout)
         * @note Suitable for boot-time initialization, not for real-time traffic
         *
         * Example:
         *   uint8_t google_dns_mac[6];
         *   if (ARP::resolveMacAddress(0x08080808, google_dns_mac)) {  // 8.8.8.8
         *       LOG_INFO("Google DNS MAC: %02X:%02X:%02X:%02X:%02X:%02X", ...);
         *   } else {
         *       LOG_WARN("Could not resolve Google DNS MAC address");
         *   }
         */
        static bool resolveMacAddress(uint32_t ipAddress, uint8_t* outMAC);

        /**
         * @brief Add or update ARP cache entry manually
         *
         * Used when we know a MAC address for an IP (e.g., gateway, DHCP server).
         * Avoids waiting for ARP request/reply cycle.
         *
         * @param ipAddress IPv4 address
         * @param macAddress Pointer to 6-byte MAC address
         * @return true if added/updated successfully, false if cache full
         */
        static bool addCacheEntry(uint32_t ipAddress, const uint8_t* macAddress);

        /**
         * @brief Clear ARP cache
         *
         * Useful for network diagnostics or when network changes.
         */
        static void clearCache();

        /**
         * @brief Log all ARP cache entries
         *
         * Displays human-readable ARP cache for debugging.
         */
        static void logCache();

        // ==================== Packet Handling ====================

        /**
         * @brief Process incoming ARP packet
         *
         * Called from network dispatcher when an ARP frame is received.
         * Automatically replies to ARP requests and updates cache on replies.
         *
         * @param frame Ethernet frame containing ARP packet (including Ethernet header)
         * @param frameLength Total frame length (including Ethernet header and FCS)
         * @return true if packet processed successfully, false on error
         *
         * @note Called from interrupt handler - should be fast
         * @note Automatically handles:
         *       - ARP requests for our IP (sends reply)
         *       - ARP replies (updates cache, wakes waiting resolver)
         */
        static bool handleARPPacket(const uint8_t* frame, uint32_t frameLength);

        /**
         * @brief Send ARP request for a given IP address
         *
         * Broadcasts an ARP request asking "Who has this IP?"
         *
         * @param targetIP IPv4 address to query (host byte order)
         * @return true if request sent successfully, false on error
         *
         * @note Requires NetworkManager and default interface to be ready
         */
        static bool sendARPRequest(uint32_t targetIP);

        /**
         * @brief Send ARP reply to a requester
         *
         * Sends unicast ARP reply in response to an ARP request.
         *
         * @param targetIP Requester's IP address (from request)
         * @param targetMAC Requester's MAC address (from request)
         * @return true if reply sent successfully, false on error
         */
        static bool sendARPReply(uint32_t targetIP, const uint8_t* targetMAC);

    private:
        // ==================== Singleton Pattern ====================
        ARP()                      = delete;
        ~ARP()                     = delete;
        ARP(const ARP&)            = delete;
        ARP& operator=(const ARP&) = delete;

        // ==================== ARP Cache Entry ====================

        struct CacheEntry {
            uint32_t ipAddress;                              ///< IPv4 address (host byte order)
            uint8_t macAddress[ethernet::MAC_ADDRESS_SIZE];  ///< Resolved MAC address
            uint32_t timestamp;                              ///< Entry creation time (system ticks)
            bool valid;                                      ///< Entry validity flag
        };

        // ==================== ARP Packet Structure ====================

        /// @brief ARP packet format (RFC 826)
        struct ARPPacket {
            uint16_t hardwareType;                          ///< 1 = Ethernet
            uint16_t protocolType;                          ///< 0x0800 = IPv4
            uint8_t macAddressSize;                         ///< 6 for Ethernet
            uint8_t ipAddressSize;                          ///< 4 for IPv4
            uint16_t operation;                             ///< 1 = Request, 2 = Reply
            uint8_t senderMAC[ethernet::MAC_ADDRESS_SIZE];  ///< Sender MAC address
            uint32_t senderIP;                              ///< Sender IPv4 address
            uint8_t targetMAC[ethernet::MAC_ADDRESS_SIZE];  ///< Target MAC address (request: zeros)
            uint32_t targetIP;                              ///< Target IPv4 address
        } __attribute__((packed));

        /// @brief Size of ARP packet (without Ethernet header)
        static constexpr size_t PACKET_SIZE = sizeof(ARPPacket);

        // ==================== Static Members ====================

        /// @brief Initialization state
        static bool initialized_;

        /// @brief Local IPv4 address
        static uint32_t localIP_;

        /// @brief Local MAC address
        static uint8_t localMAC_[ethernet::MAC_ADDRESS_SIZE];

        /// @brief ARP cache (array of entries)
        static CacheEntry cache_[MAX_CACHE_ENTRIES];

        /// @brief Number of valid cache entries
        static uint8_t cacheCount_;

        // ==================== Helper Methods ====================

        /**
         * @brief Find cache entry by IP address
         *
         * @param ipAddress IPv4 address to search
         * @return Pointer to cache entry, or nullptr if not found
         */
        static CacheEntry* findCacheEntry(uint32_t ipAddress);

        /**
         * @brief Check if cache entry is expired
         *
         * @param entry Cache entry to check
         * @return true if entry has exceeded CACHE_TIMEOUT_SECONDS
         */
        static bool isCacheEntryExpired(const CacheEntry* entry);

        /**
         * @brief Get system timestamp for cache expiration tracking
         *
         * @return Current system time (in seconds or arbitrary units, only relative comparison matters)
         */
        static uint32_t getSystemTime();
    };

}  // namespace PalmyraOS::kernel
