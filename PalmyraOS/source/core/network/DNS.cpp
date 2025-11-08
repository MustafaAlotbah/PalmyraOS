#include "core/network/DNS.h"
#include "core/network/ARP.h"
#include "core/peripherals/Logger.h"
#include "libs/memory.h"
#include "libs/string.h"

namespace PalmyraOS::kernel {

    // ==================== Static Member Initialization ====================

    bool DNS::initialized_                  = false;
    DNS::CacheEntry DNS::cache_[CACHE_SIZE] = {};
    uint8_t DNS::cacheCount_                = 0;

    // ==================== Lifecycle ====================

    bool DNS::initialize() {
        if (initialized_) {
            LOG_WARN("DNS: Already initialized");
            return true;
        }

        cacheCount_  = 0;
        initialized_ = true;

        LOG_INFO("DNS: Initialized");
        LOG_INFO("DNS: Primary server: 10.0.2.3");
        LOG_INFO("DNS: Secondary server: 8.8.8.8");
        LOG_INFO("DNS: Cache ready for domain name resolution");
        LOG_INFO("DNS: Note - Actual DNS queries require UDP/IP implementation");

        // Don't add hardcoded entries - we want to test actual resolution
        // Cache will be populated as queries are made and responses received

        return true;
    }

    // ==================== DNS Resolution ====================

    bool DNS::resolveDomain(const char* domainName, uint32_t* outIP) {
        if (!initialized_) {
            LOG_ERROR("DNS: Not initialized");
            return false;
        }

        if (!domainName || !outIP) {
            LOG_ERROR("DNS: Invalid parameters");
            return false;
        }

        LOG_INFO("DNS: Resolving domain '%s'", domainName);

        // Check cache first
        CacheEntry* entry = findCacheEntry(domainName);
        if (entry && entry->valid) {
            LOG_DEBUG("DNS: Cache HIT for '%s'", domainName);
            *outIP              = entry->ipAddress;

            // Format IP for display
            uint8_t ip_bytes[4] = {static_cast<uint8_t>((entry->ipAddress >> 24) & 0xFF),
                                   static_cast<uint8_t>((entry->ipAddress >> 16) & 0xFF),
                                   static_cast<uint8_t>((entry->ipAddress >> 8) & 0xFF),
                                   static_cast<uint8_t>(entry->ipAddress & 0xFF)};

            LOG_INFO("DNS: Cache HIT: '%s' = %u.%u.%u.%u", domainName, ip_bytes[0], ip_bytes[1], ip_bytes[2], ip_bytes[3]);
            return true;
        }

        LOG_DEBUG("DNS: Cache MISS for '%s'", domainName);
        LOG_INFO("DNS: Would query PRIMARY_DNS_SERVER (10.0.2.3) for '%s'", domainName);
        LOG_INFO("DNS: STATUS - DNS queries require UDP/IP stack implementation");
        LOG_WARN("DNS: Cannot resolve '%s' - UDP/IP stack not yet implemented", domainName);

        // TODO: Implement actual DNS queries via UDP when UDP/IP stack is ready
        // Once UDP/IP is implemented, sendDNSQuery() will:
        // 1. Build DNS query packet (transaction ID, domain name, query type)
        // 2. Send UDP packet to PRIMARY_DNS_SERVER:53
        // 3. Wait for response with timeout
        // 4. Parse response to extract IP address
        // 5. Add to cache
        // 6. Return result
        return false;
    }

    bool DNS::addCacheEntry(const char* domainName, uint32_t ipAddress) {
        if (!initialized_) {
            LOG_ERROR("DNS: Not initialized");
            return false;
        }

        if (!domainName) {
            LOG_ERROR("DNS: Invalid domain name");
            return false;
        }

        if (strlen(domainName) >= MAX_DOMAIN_LENGTH) {
            LOG_ERROR("DNS: Domain name too long");
            return false;
        }

        // Check if entry already exists
        CacheEntry* entry = findCacheEntry(domainName);
        if (entry) {
            // Update existing entry
            entry->ipAddress    = ipAddress;
            entry->timestamp    = 0;  // Dummy timestamp
            entry->valid        = true;

            uint8_t ip_bytes[4] = {static_cast<uint8_t>((ipAddress >> 24) & 0xFF),
                                   static_cast<uint8_t>((ipAddress >> 16) & 0xFF),
                                   static_cast<uint8_t>((ipAddress >> 8) & 0xFF),
                                   static_cast<uint8_t>(ipAddress & 0xFF)};

            LOG_DEBUG("DNS: Updated cache entry for '%s' -> %u.%u.%u.%u", domainName, ip_bytes[0], ip_bytes[1], ip_bytes[2], ip_bytes[3]);
            return true;
        }

        // Add new entry if space available
        if (cacheCount_ >= CACHE_SIZE) {
            LOG_WARN("DNS: Cache full, cannot add '%s'", domainName);
            return false;
        }

        entry = &cache_[cacheCount_];
        strcpy(entry->domainName, domainName);
        entry->ipAddress = ipAddress;
        entry->timestamp = 0;  // Dummy timestamp
        entry->valid     = true;
        cacheCount_++;

        uint8_t ip_bytes[4] = {static_cast<uint8_t>((ipAddress >> 24) & 0xFF),
                               static_cast<uint8_t>((ipAddress >> 16) & 0xFF),
                               static_cast<uint8_t>((ipAddress >> 8) & 0xFF),
                               static_cast<uint8_t>(ipAddress & 0xFF)};

        LOG_INFO("DNS: Added cache entry '%s' -> %u.%u.%u.%u", domainName, ip_bytes[0], ip_bytes[1], ip_bytes[2], ip_bytes[3]);

        return true;
    }

    void DNS::clearCache() {
        if (!initialized_) return;

        for (uint8_t i = 0; i < cacheCount_; ++i) { cache_[i].valid = false; }
        cacheCount_ = 0;

        LOG_INFO("DNS: Cache cleared");
    }

    void DNS::logCache() {
        if (!initialized_) {
            LOG_WARN("DNS: Not initialized");
            return;
        }

        LOG_INFO("========================================");
        LOG_INFO("DNS Cache (%u entries):", cacheCount_);
        LOG_INFO("========================================");

        if (cacheCount_ == 0) {
            LOG_INFO("  (empty)");
            LOG_INFO("========================================");
            return;
        }

        for (uint8_t i = 0; i < cacheCount_; ++i) {
            CacheEntry& entry = cache_[i];
            if (!entry.valid) continue;

            uint8_t ip_bytes[4] = {static_cast<uint8_t>((entry.ipAddress >> 24) & 0xFF),
                                   static_cast<uint8_t>((entry.ipAddress >> 16) & 0xFF),
                                   static_cast<uint8_t>((entry.ipAddress >> 8) & 0xFF),
                                   static_cast<uint8_t>(entry.ipAddress & 0xFF)};

            LOG_INFO("  [%u] %s -> %u.%u.%u.%u", i, entry.domainName, ip_bytes[0], ip_bytes[1], ip_bytes[2], ip_bytes[3]);
        }

        LOG_INFO("========================================");
    }

    // ==================== Helper Methods ====================

    DNS::CacheEntry* DNS::findCacheEntry(const char* domainName) {
        if (!domainName) return nullptr;

        for (uint8_t i = 0; i < cacheCount_; ++i) {
            if (cache_[i].valid && strcmp(cache_[i].domainName, domainName) == 0) { return &cache_[i]; }
        }
        return nullptr;
    }

    bool DNS::sendDNSQuery(const char* domainName, uint32_t dnsServer) {
        if (!domainName) {
            LOG_ERROR("DNS: Invalid domain name");
            return false;
        }

        LOG_DEBUG("DNS: Sending query for '%s' to DNS server 0x%08X", domainName, dnsServer);

        // TODO: Implement actual DNS query when UDP/IP stack is ready
        // For now, just log that we would send it
        LOG_INFO("DNS: Would send UDP packet to port 53 on 0x%08X", dnsServer);

        return false;  // Not yet implemented
    }

    size_t DNS::encodeDomainName(const char* domainName, uint8_t* buffer, size_t bufferSize) {
        if (!domainName || !buffer || bufferSize == 0) { return 0; }

        size_t pos        = 0;
        size_t labelStart = 0;

        for (size_t i = 0; domainName[i] != '\0' && pos < bufferSize - 1; ++i) {
            if (domainName[i] == '.') {
                // End of label - write label length
                size_t labelLength = i - labelStart;
                if (labelLength > 63) {
                    LOG_ERROR("DNS: Label too long");
                    return 0;
                }

                // Shift bytes and insert length
                if (pos > 0) {
                    buffer[pos++] = static_cast<uint8_t>(labelLength);
                    for (size_t j = 0; j < labelLength && pos < bufferSize; ++j) { buffer[pos++] = domainName[labelStart + j]; }
                }

                labelStart = i + 1;
            }
        }

        // Final label
        size_t labelLength = strlen(domainName) - labelStart;
        if (labelLength > 0 && labelLength <= 63 && pos < bufferSize - 1) {
            buffer[pos++] = static_cast<uint8_t>(labelLength);
            for (size_t j = 0; j < labelLength && pos < bufferSize - 1; ++j) { buffer[pos++] = domainName[labelStart + j]; }
        }

        // Null terminator
        if (pos < bufferSize) { buffer[pos++] = 0; }

        return pos;
    }

    bool DNS::decodeDomainName(const uint8_t* buffer, size_t bufferSize, char* outDomainName, size_t outMaxLength) {
        if (!buffer || !outDomainName || outMaxLength == 0) { return false; }

        size_t pos    = 0;
        size_t outPos = 0;

        while (pos < bufferSize && buffer[pos] != 0) {
            uint8_t len = buffer[pos++];

            if (outPos > 0 && outPos < outMaxLength) { outDomainName[outPos++] = '.'; }

            for (uint8_t i = 0; i < len && pos < bufferSize && outPos < outMaxLength; ++i) { outDomainName[outPos++] = buffer[pos++]; }
        }

        if (outPos < outMaxLength) { outDomainName[outPos] = '\0'; }
        else { return false; }

        return true;
    }

}  // namespace PalmyraOS::kernel
