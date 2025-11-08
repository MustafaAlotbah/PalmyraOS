#pragma once

#include "core/definitions.h"

namespace PalmyraOS::kernel {

    /**
     * @brief Domain Name System (DNS) Resolver
     *
     * Provides simplified DNS client for resolving domain names to IP addresses.
     * Currently uses hardcoded DNS servers; future versions will use DHCP.
     *
     * **DNS Resolution Process:**
     * 1. Parse domain name into DNS query format
     * 2. Send DNS query to DNS server (UDP port 53)
     * 3. Wait for DNS response
     * 4. Parse response and extract resolved IP address
     * 5. Cache result for future lookups
     *
     * **Common DNS Servers:**
     * - Google: 8.8.8.8, 8.8.4.4
     * - Cloudflare: 1.1.1.1, 1.0.0.1
     * - OpenDNS: 208.67.222.222, 208.67.220.220
     *
     * @note Currently simplified (requires implemented UDP/IP stack for actual queries)
     * @note This initial version demonstrates the interface and structure
     */
    class DNS {
    public:
        // ==================== Configuration Constants ====================

        /// @brief Primary DNS server address (Google DNS: 8.8.8.8)
        static constexpr uint32_t PRIMARY_DNS_SERVER   = 0x08080808;

        /// @brief Secondary DNS server address (Google DNS: 8.8.4.4)
        static constexpr uint32_t SECONDARY_DNS_SERVER = 0x08080404;

        /// @brief Maximum domain name length (e.g., "google.com")
        static constexpr size_t MAX_DOMAIN_LENGTH      = 255;

        /// @brief DNS query timeout in milliseconds
        static constexpr uint32_t QUERY_TIMEOUT_MS     = 5000;  // 5 seconds

        /// @brief DNS cache size
        static constexpr uint8_t CACHE_SIZE            = 32;

        // ==================== DNS Transaction IDs ====================

        /// @brief DNS transaction ID for queries (should be random, fixed for testing)
        static constexpr uint16_t DNS_TRANSACTION_ID   = 0x1234;

        /// @brief DNS query class: IN (Internet)
        static constexpr uint16_t DNS_CLASS_IN         = 1;

        /// @brief DNS query type: A (IPv4 address)
        static constexpr uint16_t DNS_TYPE_A           = 1;

        // ==================== Lifecycle ====================

        /**
         * @brief Initialize DNS subsystem
         *
         * Must be called after network interface is up and ARP/IP stack ready.
         *
         * @return true if initialization successful, false otherwise
         */
        static bool initialize();

        /// @brief Check if DNS subsystem is initialized
        [[nodiscard]] static bool isInitialized() { return initialized_; }

        // ==================== DNS Resolution ====================

        /**
         * @brief Resolve domain name to IPv4 address
         *
         * Queries DNS for the IPv4 address of a domain name.
         *
         * **Resolution Process:**
         * 1. Check cache for existing resolution
         * 2. If cached, return immediately (fast path)
         * 3. If not cached:
         *    a. Send DNS query to PRIMARY_DNS_SERVER
         *    b. Wait for response
         *    c. Parse response and cache result
         *    d. Return resolved IP
         * 4. On timeout, try SECONDARY_DNS_SERVER
         * 5. If all retries fail, return false
         *
         * @param domainName Domain name to resolve (e.g., "google.com")
         * @param outIP Pointer to uint32_t for result IP (host byte order)
         * @return true if resolution successful, false on timeout/error
         *
         * Example:
         *   uint32_t google_ip;
         *   if (DNS::resolveDomain("google.com", &google_ip)) {
         *       uint8_t b[4] = { google_ip >> 24, (google_ip >> 16) & 0xFF, ... };
         *       LOG_INFO("google.com = %u.%u.%u.%u", b[0], b[1], b[2], b[3]);
         *   }
         */
        static bool resolveDomain(const char* domainName, uint32_t* outIP);

        /**
         * @brief Manually add DNS cache entry
         *
         * Useful for hardcoding known domains without query.
         *
         * @param domainName Domain name
         * @param ipAddress IPv4 address (host byte order)
         * @return true if added successfully, false if cache full
         */
        static bool addCacheEntry(const char* domainName, uint32_t ipAddress);

        /// @brief Clear DNS cache
        static void clearCache();

        /// @brief Log all DNS cache entries
        static void logCache();

    private:
        // ==================== Singleton Pattern ====================
        DNS()                      = delete;
        ~DNS()                     = delete;
        DNS(const DNS&)            = delete;
        DNS& operator=(const DNS&) = delete;

        // ==================== DNS Cache Entry ====================

        struct CacheEntry {
            char domainName[MAX_DOMAIN_LENGTH];  ///< Cached domain name
            uint32_t ipAddress;                  ///< Resolved IPv4 address
            uint32_t timestamp;                  ///< Entry creation time
            bool valid;                          ///< Entry validity flag
        };

        // ==================== DNS Header (simplified RFC 1035) ====================

        /// @brief DNS query header format
        struct DNSHeader {
            uint16_t id;           ///< Transaction identifier
            uint16_t flags;        ///< Query/Response flags
            uint16_t questions;    ///< Number of questions
            uint16_t answers;      ///< Number of answer RRs
            uint16_t authorities;  ///< Number of authority RRs
            uint16_t additionals;  ///< Number of additional RRs
        } __attribute__((packed));

        // ==================== Static Members ====================

        /// @brief Initialization state
        static bool initialized_;

        /// @brief DNS cache
        static CacheEntry cache_[CACHE_SIZE];

        /// @brief Number of valid cache entries
        static uint8_t cacheCount_;

        // ==================== Helper Methods ====================

        /**
         * @brief Find cache entry by domain name
         *
         * @param domainName Domain name to search
         * @return Pointer to cache entry, or nullptr if not found
         */
        static CacheEntry* findCacheEntry(const char* domainName);

        /**
         * @brief Send DNS query for a domain
         *
         * @param domainName Domain name to query
         * @param dnsServer DNS server address
         * @return true if query sent successfully
         */
        static bool sendDNSQuery(const char* domainName, uint32_t dnsServer);

        /**
         * @brief Handle DNS response (called from UDP handler)
         *
         * @param responseData DNS response packet
         * @param responseLength Response length
         * @param serverIP Server that sent the response
         */
        static void handleDNSResponse(const uint8_t* responseData, uint32_t responseLength, uint32_t serverIP);

        /**
         * @brief Encode domain name to DNS format
         *
         * Converts "google.com" to DNS wire format:
         * - Preceded by label lengths
         * - No trailing dot encoding here
         *
         * @param domainName Input domain name
         * @param buffer Output buffer
         * @param bufferSize Maximum buffer size
         * @return Number of bytes written, or 0 on error
         *
         * Example: "google.com" -> [6]google[3]com[0]
         */
        static size_t encodeDomainName(const char* domainName, uint8_t* buffer, size_t bufferSize);

        /**
         * @brief Decode domain name from DNS format
         *
         * Converts DNS wire format back to human-readable domain name.
         *
         * @param buffer DNS wire format buffer
         * @param bufferSize Buffer size
         * @param outDomainName Output domain name buffer
         * @param outMaxLength Maximum output length
         * @return true if decoding successful
         */
        static bool decodeDomainName(const uint8_t* buffer, size_t bufferSize, char* outDomainName, size_t outMaxLength);
    };

}  // namespace PalmyraOS::kernel
