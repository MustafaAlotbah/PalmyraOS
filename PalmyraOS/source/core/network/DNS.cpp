#include "core/network/DNS.h"
#include "core/network/ARP.h"
#include "core/network/Ethernet.h"
#include "core/network/IPv4.h"
#include "core/network/NetworkManager.h"
#include "core/network/UDP.h"
#include "core/peripherals/Logger.h"
#include "libs/memory.h"
#include "libs/string.h"

namespace PalmyraOS::kernel {

    // ==================== Static Member Initialization ====================

    bool DNS::initialized_                  = false;
    DNS::CacheEntry DNS::cache_[CACHE_SIZE] = {};
    uint8_t DNS::cacheCount_                = 0;

    // Static state for pending DNS query
    static struct {
        char queryDomain[DNS::MAX_DOMAIN_LENGTH];
        uint16_t transactionID;
        uint32_t serverIP;
        bool responseReceived;
        uint32_t resolvedIP;
    } pendingQuery_;

    // ==================== Lifecycle ====================

    bool DNS::initialize() {
        if (initialized_) {
            LOG_WARN("DNS: Already initialized");
            return true;
        }

        cacheCount_         = 0;
        initialized_        = true;

        // Determine DNS server based on network environment
        // VirtualBox NAT provides DNS at 10.0.2.3
        uint32_t primaryDNS = PRIMARY_DNS_SERVER;
        uint32_t localIP    = IPv4::getLocalIP();

        // Check if we're on VirtualBox NAT network (10.0.2.x)
        if ((localIP & 0xFFFFFF00) == 0x0A000200) {  // 10.0.2.0/24
            primaryDNS = 0x0A000203;                 // 10.0.2.3 (VirtualBox NAT DNS)
        }

        LOG_INFO("DNS: Initialized");
        LOG_INFO("DNS: Primary server: %u.%u.%u.%u", (primaryDNS >> 24) & 0xFF, (primaryDNS >> 16) & 0xFF, (primaryDNS >> 8) & 0xFF, primaryDNS & 0xFF);
        LOG_INFO("DNS: Secondary server: %u.%u.%u.%u",
                 (SECONDARY_DNS_SERVER >> 24) & 0xFF,
                 (SECONDARY_DNS_SERVER >> 16) & 0xFF,
                 (SECONDARY_DNS_SERVER >> 8) & 0xFF,
                 SECONDARY_DNS_SERVER & 0xFF);
        LOG_INFO("DNS: Cache ready (up to %u entries)", CACHE_SIZE);

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
        CacheEntry* cachedEntry = findCacheEntry(domainName);
        if (cachedEntry && cachedEntry->valid) {
            *outIP             = cachedEntry->ipAddress;

            uint8_t ipBytes[4] = {static_cast<uint8_t>((cachedEntry->ipAddress >> 24) & 0xFF),
                                  static_cast<uint8_t>((cachedEntry->ipAddress >> 16) & 0xFF),
                                  static_cast<uint8_t>((cachedEntry->ipAddress >> 8) & 0xFF),
                                  static_cast<uint8_t>(cachedEntry->ipAddress & 0xFF)};

            LOG_INFO("DNS: Cache HIT: '%s' -> %u.%u.%u.%u", domainName, ipBytes[0], ipBytes[1], ipBytes[2], ipBytes[3]);
            return true;
        }

        LOG_DEBUG("DNS: Cache MISS for '%s', sending query", domainName);

        // Determine DNS server (VirtualBox NAT or public)
        uint32_t dnsServer = PRIMARY_DNS_SERVER;
        uint32_t localIP   = IPv4::getLocalIP();
        if ((localIP & 0xFFFFFF00) == 0x0A000200) {  // 10.0.2.0/24
            dnsServer = 0x0A000203;                  // VirtualBox NAT DNS
        }

        // Send DNS query via UDP
        if (!sendDNSQuery(domainName, dnsServer)) {
            LOG_ERROR("DNS: Failed to send query");
            return false;
        }

        // Wait for response by polling network interface for incoming packets
        uint32_t waitTimeMs                 = 0;
        constexpr uint32_t POLL_INTERVAL_MS = 10;
        auto networkInterface               = NetworkManager::getDefaultInterface();

        while (waitTimeMs < QUERY_TIMEOUT_MS && !pendingQuery_.responseReceived) {
            // Poll network interface for incoming UDP packets
            for (volatile uint32_t i = 0; i < 100000; ++i) {
                if ((i % 1000) == 0 && networkInterface) {
                    networkInterface->handleInterrupt();  // Process incoming packets (including UDP)
                }
            }
            waitTimeMs += POLL_INTERVAL_MS;
        }

        if (!pendingQuery_.responseReceived) {
            LOG_WARN("DNS: Query timeout after %u ms", QUERY_TIMEOUT_MS);
            return false;
        }

        // Resolution successful
        *outIP             = pendingQuery_.resolvedIP;

        uint8_t ipBytes[4] = {static_cast<uint8_t>((pendingQuery_.resolvedIP >> 24) & 0xFF),
                              static_cast<uint8_t>((pendingQuery_.resolvedIP >> 16) & 0xFF),
                              static_cast<uint8_t>((pendingQuery_.resolvedIP >> 8) & 0xFF),
                              static_cast<uint8_t>(pendingQuery_.resolvedIP & 0xFF)};

        LOG_INFO("DNS: Successfully resolved '%s' -> %u.%u.%u.%u", domainName, ipBytes[0], ipBytes[1], ipBytes[2], ipBytes[3]);

        // Add to cache
        addCacheEntry(domainName, pendingQuery_.resolvedIP);

        return true;
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
        CacheEntry* existingEntry = findCacheEntry(domainName);
        if (existingEntry) {
            // Update existing entry
            existingEntry->ipAddress = ipAddress;
            existingEntry->timestamp = 0;  // Dummy timestamp
            existingEntry->valid     = true;

            uint8_t ipBytes[4]       = {static_cast<uint8_t>((ipAddress >> 24) & 0xFF),
                                        static_cast<uint8_t>((ipAddress >> 16) & 0xFF),
                                        static_cast<uint8_t>((ipAddress >> 8) & 0xFF),
                                        static_cast<uint8_t>(ipAddress & 0xFF)};

            LOG_DEBUG("DNS: Updated cache entry for '%s' -> %u.%u.%u.%u", domainName, ipBytes[0], ipBytes[1], ipBytes[2], ipBytes[3]);
            return true;
        }

        // Add new entry if space available
        if (cacheCount_ >= CACHE_SIZE) {
            LOG_WARN("DNS: Cache full, cannot add '%s'", domainName);
            return false;
        }

        CacheEntry* newEntry = &cache_[cacheCount_];
        strcpy(newEntry->domainName, domainName);
        newEntry->ipAddress = ipAddress;
        newEntry->timestamp = 0;  // Dummy timestamp
        newEntry->valid     = true;
        cacheCount_++;

        uint8_t ipBytes[4] = {static_cast<uint8_t>((ipAddress >> 24) & 0xFF),
                              static_cast<uint8_t>((ipAddress >> 16) & 0xFF),
                              static_cast<uint8_t>((ipAddress >> 8) & 0xFF),
                              static_cast<uint8_t>(ipAddress & 0xFF)};

        LOG_INFO("DNS: Added cache entry '%s' -> %u.%u.%u.%u", domainName, ipBytes[0], ipBytes[1], ipBytes[2], ipBytes[3]);

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

            uint8_t ipBytes[4] = {static_cast<uint8_t>((entry.ipAddress >> 24) & 0xFF),
                                  static_cast<uint8_t>((entry.ipAddress >> 16) & 0xFF),
                                  static_cast<uint8_t>((entry.ipAddress >> 8) & 0xFF),
                                  static_cast<uint8_t>(entry.ipAddress & 0xFF)};

            LOG_INFO("  [%u] %s -> %u.%u.%u.%u", i, entry.domainName, ipBytes[0], ipBytes[1], ipBytes[2], ipBytes[3]);
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

    bool DNS::sendDNSQuery(const char* domainName, uint32_t dnsServerIP) {
        if (!domainName) {
            LOG_ERROR("DNS: Invalid domain name");
            return false;
        }

        // Build DNS query packet (RFC 1035)
        uint8_t queryBuffer[512];  // Maximum DNS packet size over UDP
        uint32_t queryLength   = 0;

        // ========== DNS Header ==========
        DNSHeader* dnsHeader   = reinterpret_cast<DNSHeader*>(queryBuffer);
        dnsHeader->id          = ethernet::toBigEndian16(DNS_TRANSACTION_ID);
        dnsHeader->flags       = ethernet::toBigEndian16(0x0100);  // Standard query, recursion desired
        dnsHeader->questions   = ethernet::toBigEndian16(1);       // 1 question
        dnsHeader->answers     = 0;
        dnsHeader->authorities = 0;
        dnsHeader->additionals = 0;
        queryLength += sizeof(DNSHeader);

        // ========== Question Section ==========
        // Encode domain name (e.g., "google.com" -> [6]google[3]com[0])
        size_t encodedLength = encodeDomainName(domainName, queryBuffer + queryLength, sizeof(queryBuffer) - queryLength);
        if (encodedLength == 0) {
            LOG_ERROR("DNS: Failed to encode domain name '%s'", domainName);
            return false;
        }
        queryLength += encodedLength;

        // Add query type (A record = IPv4 address) and class (IN = Internet)
        uint16_t* queryType = reinterpret_cast<uint16_t*>(queryBuffer + queryLength);
        queryType[0]        = ethernet::toBigEndian16(DNS_TYPE_A);    // Type: A (IPv4 address)
        queryType[1]        = ethernet::toBigEndian16(DNS_CLASS_IN);  // Class: IN (Internet)
        queryLength += 4;

        // Set pending query state
        strcpy(pendingQuery_.queryDomain, domainName);
        pendingQuery_.transactionID    = DNS_TRANSACTION_ID;
        pendingQuery_.serverIP         = dnsServerIP;
        pendingQuery_.responseReceived = false;
        pendingQuery_.resolvedIP       = 0;

        // Bind port for receiving DNS response (if not already bound)
        static bool dnsPortBound       = false;
        if (!dnsPortBound) {
            // Use a fixed source port for DNS queries (could be ephemeral)
            constexpr uint16_t DNS_CLIENT_PORT = 54321;

            UDP::bindPort(DNS_CLIENT_PORT, [](uint32_t sourceIP, uint16_t sourcePort, const uint8_t* data, uint32_t dataLength) {
                // DNS response handler
                handleDNSResponse(data, dataLength, sourceIP);
            });
            dnsPortBound = true;
        }

        // Send DNS query via UDP to port 53
        constexpr uint16_t DNS_CLIENT_PORT = 54321;
        if (!UDP::sendDatagram(dnsServerIP, UDP::PORT_DNS, DNS_CLIENT_PORT, queryBuffer, queryLength)) {
            LOG_ERROR("DNS: Failed to send UDP query");
            return false;
        }

        LOG_INFO("DNS: Sent query for '%s' to %u.%u.%u.%u (transaction ID: 0x%04X, %u bytes)",
                 domainName,
                 (dnsServerIP >> 24) & 0xFF,
                 (dnsServerIP >> 16) & 0xFF,
                 (dnsServerIP >> 8) & 0xFF,
                 dnsServerIP & 0xFF,
                 DNS_TRANSACTION_ID,
                 queryLength);

        return true;
    }

    // ==================== DNS Response Handling ====================

    void DNS::handleDNSResponse(const uint8_t* responseData, uint32_t responseLength, uint32_t serverIP) {
        if (responseLength < sizeof(DNSHeader)) {
            LOG_WARN("DNS: Response too small (%u bytes)", responseLength);
            return;
        }

        // Parse DNS header
        const DNSHeader* dnsHeader = reinterpret_cast<const DNSHeader*>(responseData);
        uint16_t transactionID     = ethernet::fromBigEndian16(dnsHeader->id);
        uint16_t flags             = ethernet::fromBigEndian16(dnsHeader->flags);
        uint16_t questionCount     = ethernet::fromBigEndian16(dnsHeader->questions);
        uint16_t answerCount       = ethernet::fromBigEndian16(dnsHeader->answers);

        LOG_INFO("DNS: Received response (ID=0x%04X, questions=%u, answers=%u)", transactionID, questionCount, answerCount);

        // Check if this matches our pending query
        if (transactionID != pendingQuery_.transactionID) {
            LOG_WARN("DNS: Transaction ID mismatch (expected 0x%04X, got 0x%04X)", pendingQuery_.transactionID, transactionID);
            return;
        }

        // Check response code (bits 0-3 of flags)
        uint8_t responseCode = flags & 0x000F;
        if (responseCode != 0) {
            LOG_ERROR("DNS: Query failed with response code %u", responseCode);
            return;
        }

        // Check if it's a response (QR bit = bit 15)
        if ((flags & 0x8000) == 0) {
            LOG_WARN("DNS: Received query instead of response");
            return;
        }

        // Skip question section (we know the format since we sent the query)
        uint32_t offset = sizeof(DNSHeader);

        // Skip question: domain name + type (2) + class (2)
        // Domain name ends with 0x00
        while (offset < responseLength && responseData[offset] != 0) {
            uint8_t labelLength = responseData[offset];
            if (labelLength >= 192) {  // Compression pointer (starts with 11xxxxxx)
                offset += 2;           // Pointer is 2 bytes
                break;
            }
            offset += 1 + labelLength;  // Length byte + label bytes
        }
        offset += 1;  // Skip terminating 0x00
        offset += 4;  // Skip type (2) + class (2)

        // Parse answer section
        for (uint16_t answerIndex = 0; answerIndex < answerCount; ++answerIndex) {
            if (offset >= responseLength) break;

            // Skip name (use compression pointer if present)
            if ((responseData[offset] & 0xC0) == 0xC0) {
                offset += 2;  // Compression pointer
            }
            else {
                // Skip full name
                while (offset < responseLength && responseData[offset] != 0) { offset += 1 + responseData[offset]; }
                offset += 1;  // Skip 0x00
            }

            if (offset + 10 > responseLength) break;  // Need type(2) + class(2) + TTL(4) + rdlength(2)

            // Parse answer record
            uint16_t answerType  = (responseData[offset] << 8) | responseData[offset + 1];
            uint16_t answerClass = (responseData[offset + 2] << 8) | responseData[offset + 3];
            // uint32_t ttl       = ... (not used for now)
            uint16_t rdataLength = (responseData[offset + 8] << 8) | responseData[offset + 9];
            offset += 10;

            // Check for A record (IPv4 address)
            if (answerType == DNS_TYPE_A && answerClass == DNS_CLASS_IN && rdataLength == 4) {
                if (offset + 4 <= responseLength) {
                    // Extract IPv4 address (network byte order)
                    uint32_t resolvedIP = (static_cast<uint32_t>(responseData[offset]) << 24) | (static_cast<uint32_t>(responseData[offset + 1]) << 16) |
                                          (static_cast<uint32_t>(responseData[offset + 2]) << 8) | static_cast<uint32_t>(responseData[offset + 3]);

                    // Mark response received
                    pendingQuery_.responseReceived = true;
                    pendingQuery_.resolvedIP       = resolvedIP;

                    uint8_t ipBytes[4]             = {static_cast<uint8_t>((resolvedIP >> 24) & 0xFF),
                                                      static_cast<uint8_t>((resolvedIP >> 16) & 0xFF),
                                                      static_cast<uint8_t>((resolvedIP >> 8) & 0xFF),
                                                      static_cast<uint8_t>(resolvedIP & 0xFF)};

                    LOG_INFO("DNS: Parsed A record: %u.%u.%u.%u", ipBytes[0], ipBytes[1], ipBytes[2], ipBytes[3]);
                    return;  // Success!
                }
            }

            offset += rdataLength;  // Skip this answer's data
        }

        LOG_WARN("DNS: No A record found in response");
    }

    // ==================== Domain Name Encoding/Decoding ====================

    size_t DNS::encodeDomainName(const char* domainName, uint8_t* buffer, size_t bufferSize) {
        if (!domainName || !buffer || bufferSize < 2) { return 0; }

        size_t domainLength = strlen(domainName);
        if (domainLength == 0 || domainLength >= MAX_DOMAIN_LENGTH) { return 0; }

        size_t outputOffset = 0;
        size_t labelStart   = 0;

        // Encode each label (part between dots)
        // Example: "google.com" -> [6]google[3]com[0]
        for (size_t i = 0; i <= domainLength; ++i) {
            if (domainName[i] == '.' || domainName[i] == '\0') {
                size_t labelLength = i - labelStart;

                if (labelLength == 0) {
                    // Empty label (e.g., ".." or leading/trailing dot)
                    continue;
                }

                if (labelLength > 63) {
                    LOG_ERROR("DNS: Label too long (%u chars, max 63)", labelLength);
                    return 0;
                }

                if (outputOffset + 1 + labelLength >= bufferSize) {
                    LOG_ERROR("DNS: Buffer too small for domain encoding");
                    return 0;
                }

                // Write label length
                buffer[outputOffset++] = static_cast<uint8_t>(labelLength);

                // Write label characters (convert to lowercase)
                for (size_t j = 0; j < labelLength; ++j) {
                    char c = domainName[labelStart + j];
                    // Convert to lowercase for DNS (case-insensitive)
                    if (c >= 'A' && c <= 'Z') { c = c + ('a' - 'A'); }
                    buffer[outputOffset++] = c;
                }

                labelStart = i + 1;  // Start of next label
            }
        }

        // Write terminating null byte
        if (outputOffset >= bufferSize) { return 0; }
        buffer[outputOffset++] = 0;

        return outputOffset;
    }

    bool DNS::decodeDomainName(const uint8_t* buffer, size_t bufferSize, char* outDomainName, size_t outMaxLength) {
        if (!buffer || !outDomainName || outMaxLength == 0) { return false; }

        size_t inputOffset  = 0;
        size_t outputOffset = 0;

        while (inputOffset < bufferSize) {
            uint8_t labelLength = buffer[inputOffset++];

            // Check for terminating null
            if (labelLength == 0) {
                // Remove trailing dot if present
                if (outputOffset > 0 && outDomainName[outputOffset - 1] == '.') { outputOffset--; }
                outDomainName[outputOffset] = '\0';
                return true;
            }

            // Check for compression pointer (top 2 bits set: 11xxxxxx)
            if ((labelLength & 0xC0) == 0xC0) {
                // Compression not fully supported yet
                LOG_WARN("DNS: Domain name compression not supported");
                return false;
            }

            // Validate label length
            if (labelLength > 63 || inputOffset + labelLength > bufferSize) { return false; }

            // Add dot separator (except before first label)
            if (outputOffset > 0 && outputOffset < outMaxLength - 1) { outDomainName[outputOffset++] = '.'; }

            // Copy label
            for (uint8_t i = 0; i < labelLength && outputOffset < outMaxLength - 1; ++i) { outDomainName[outputOffset++] = buffer[inputOffset++]; }
        }

        outDomainName[outputOffset] = '\0';
        return false;  // Reached end without null terminator
    }

}  // namespace PalmyraOS::kernel
