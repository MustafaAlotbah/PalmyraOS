#include "core/network/ARP.h"
#include "core/network/NetworkManager.h"
#include "core/peripherals/Logger.h"
#include "libs/memory.h"
#include "libs/string.h"

namespace PalmyraOS::kernel {

    // ==================== Static Member Initialization ====================

    bool ARP::initialized_                             = false;
    uint32_t ARP::localIP_                             = 0;
    uint8_t ARP::localMAC_[ethernet::MAC_ADDRESS_SIZE] = {0};
    ARP::CacheEntry ARP::cache_[MAX_CACHE_ENTRIES]     = {};
    uint8_t ARP::cacheCount_                           = 0;

    // ==================== Lifecycle ====================

    bool ARP::initialize(uint32_t localIP, const uint8_t* localMAC) {
        if (initialized_) {
            LOG_WARN("ARP: Already initialized");
            return true;
        }

        if (!localMAC) {
            LOG_ERROR("ARP: Invalid local MAC address");
            return false;
        }

        localIP_ = localIP;
        memcpy(localMAC_, localMAC, ethernet::MAC_ADDRESS_SIZE);
        cacheCount_         = 0;

        initialized_        = true;

        // Format and display local address for debugging
        uint8_t ip_bytes[4] = {static_cast<uint8_t>((localIP >> 24) & 0xFF),
                               static_cast<uint8_t>((localIP >> 16) & 0xFF),
                               static_cast<uint8_t>((localIP >> 8) & 0xFF),
                               static_cast<uint8_t>(localIP & 0xFF)};

        LOG_INFO("ARP: Initialized with local IP %u.%u.%u.%u", ip_bytes[0], ip_bytes[1], ip_bytes[2], ip_bytes[3]);
        LOG_INFO("ARP: Local MAC %02X:%02X:%02X:%02X:%02X:%02X", localMAC[0], localMAC[1], localMAC[2], localMAC[3], localMAC[4], localMAC[5]);

        return true;
    }

    // ==================== ARP Cache Management ====================

    bool ARP::resolveMacAddress(uint32_t ipAddress, uint8_t* outMAC) {
        if (!initialized_) {
            LOG_ERROR("ARP: Not initialized");
            return false;
        }

        if (!outMAC) {
            LOG_ERROR("ARP: Invalid output MAC buffer");
            return false;
        }

        // Display target IP for debugging
        uint8_t ip_bytes[4] = {static_cast<uint8_t>((ipAddress >> 24) & 0xFF),
                               static_cast<uint8_t>((ipAddress >> 16) & 0xFF),
                               static_cast<uint8_t>((ipAddress >> 8) & 0xFF),
                               static_cast<uint8_t>(ipAddress & 0xFF)};

        LOG_INFO("ARP: Resolving %u.%u.%u.%u", ip_bytes[0], ip_bytes[1], ip_bytes[2], ip_bytes[3]);

        // Check cache first
        CacheEntry* entry = findCacheEntry(ipAddress);
        if (entry && entry->valid && !isCacheEntryExpired(entry)) {
            LOG_DEBUG("ARP: Cache HIT for %u.%u.%u.%u", ip_bytes[0], ip_bytes[1], ip_bytes[2], ip_bytes[3]);
            memcpy(outMAC, entry->macAddress, ethernet::MAC_ADDRESS_SIZE);
            LOG_INFO("ARP: Resolved to %02X:%02X:%02X:%02X:%02X:%02X", outMAC[0], outMAC[1], outMAC[2], outMAC[3], outMAC[4], outMAC[5]);
            return true;
        }

        // Cache miss or expired - send ARP request with retries
        LOG_DEBUG("ARP: Cache MISS for %u.%u.%u.%u, sending ARP request", ip_bytes[0], ip_bytes[1], ip_bytes[2], ip_bytes[3]);

        // Attempt resolution with retries (broadcasts ARP request and waits for reply)
        for (uint8_t attemptNumber = 0; attemptNumber < MAX_REQUEST_RETRIES; ++attemptNumber) {
            if (!sendARPRequest(ipAddress)) {
                LOG_ERROR("ARP: Failed to send request (attempt %u/%u)", attemptNumber + 1, MAX_REQUEST_RETRIES);
                continue;
            }

            // Wait for ARP reply by polling network interface
            // In production, this would use interrupt-driven packet reception
            auto networkInterface              = kernel::NetworkManager::getDefaultInterface();
            constexpr uint32_t POLL_ITERATIONS = 1000000;
            constexpr uint32_t POLL_FREQUENCY  = 1000;

            for (volatile uint32_t iteration = 0; iteration < POLL_ITERATIONS; ++iteration) {
                // Poll network interface every POLL_FREQUENCY iterations
                if ((iteration % POLL_FREQUENCY) == 0 && networkInterface) {
                    networkInterface->handleInterrupt();  // Process incoming packets
                }
            }

            // Check if ARP reply arrived and updated cache
            entry = findCacheEntry(ipAddress);
            if (entry && entry->valid && !isCacheEntryExpired(entry)) {
                LOG_INFO("ARP: Resolution successful on attempt %u/%u", attemptNumber + 1, MAX_REQUEST_RETRIES);
                memcpy(outMAC, entry->macAddress, ethernet::MAC_ADDRESS_SIZE);
                LOG_INFO("ARP: Resolved %u.%u.%u.%u → %02X:%02X:%02X:%02X:%02X:%02X",
                         ip_bytes[0],
                         ip_bytes[1],
                         ip_bytes[2],
                         ip_bytes[3],
                         outMAC[0],
                         outMAC[1],
                         outMAC[2],
                         outMAC[3],
                         outMAC[4],
                         outMAC[5]);
                return true;
            }
        }

        LOG_WARN("ARP: Resolution failed after %u attempts", MAX_REQUEST_RETRIES);
        return false;
    }

    bool ARP::addCacheEntry(uint32_t ipAddress, const uint8_t* macAddress) {
        if (!initialized_) {
            LOG_ERROR("ARP: Not initialized");
            return false;
        }

        if (!macAddress) {
            LOG_ERROR("ARP: Invalid MAC address");
            return false;
        }

        // Check if entry already exists
        CacheEntry* entry = findCacheEntry(ipAddress);
        if (entry) {
            // Update existing entry
            memcpy(entry->macAddress, macAddress, ethernet::MAC_ADDRESS_SIZE);
            entry->timestamp = getSystemTime();
            entry->valid     = true;
            LOG_DEBUG("ARP: Updated cache entry for IP");
            return true;
        }

        // Add new entry if space available
        if (cacheCount_ >= MAX_CACHE_ENTRIES) {
            LOG_WARN("ARP: Cache full, cannot add new entry");
            return false;
        }

        entry            = &cache_[cacheCount_];
        entry->ipAddress = ipAddress;
        memcpy(entry->macAddress, macAddress, ethernet::MAC_ADDRESS_SIZE);
        entry->timestamp = getSystemTime();
        entry->valid     = true;
        cacheCount_++;

        uint8_t ip_bytes[4] = {static_cast<uint8_t>((ipAddress >> 24) & 0xFF),
                               static_cast<uint8_t>((ipAddress >> 16) & 0xFF),
                               static_cast<uint8_t>((ipAddress >> 8) & 0xFF),
                               static_cast<uint8_t>(ipAddress & 0xFF)};

        LOG_DEBUG("ARP: Added cache entry for %u.%u.%u.%u -> %02X:%02X:%02X:%02X:%02X:%02X",
                  ip_bytes[0],
                  ip_bytes[1],
                  ip_bytes[2],
                  ip_bytes[3],
                  macAddress[0],
                  macAddress[1],
                  macAddress[2],
                  macAddress[3],
                  macAddress[4],
                  macAddress[5]);

        return true;
    }

    void ARP::clearCache() {
        if (!initialized_) return;

        for (uint8_t i = 0; i < cacheCount_; ++i) { cache_[i].valid = false; }
        cacheCount_ = 0;

        LOG_INFO("ARP: Cache cleared");
    }

    void ARP::logCache() {
        if (!initialized_) {
            LOG_WARN("ARP: Not initialized");
            return;
        }

        LOG_INFO("========================================");
        LOG_INFO("ARP Cache (%u entries):", cacheCount_);
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

            const char* status  = isCacheEntryExpired(&entry) ? "EXPIRED" : "VALID";

            LOG_INFO("  [%u] %u.%u.%u.%u -> %02X:%02X:%02X:%02X:%02X:%02X (%s)",
                     i,
                     ip_bytes[0],
                     ip_bytes[1],
                     ip_bytes[2],
                     ip_bytes[3],
                     entry.macAddress[0],
                     entry.macAddress[1],
                     entry.macAddress[2],
                     entry.macAddress[3],
                     entry.macAddress[4],
                     entry.macAddress[5],
                     status);
        }

        LOG_INFO("========================================");
    }

    // ==================== Packet Handling ====================

    bool ARP::handleARPPacket(const uint8_t* frame, uint32_t frameLength) {
        if (!initialized_) {
            LOG_WARN("ARP: Not initialized, dropping packet");
            return false;
        }

        // Validate frame size: Ethernet header + ARP packet (FCS not present in software buffers)
        if (frameLength < ethernet::HEADER_SIZE + PACKET_SIZE) {
            LOG_WARN("ARP: Frame too small (%u bytes)", frameLength);
            return false;
        }

        // Skip Ethernet header
        const uint8_t* arpData = frame + ethernet::HEADER_SIZE;
        const ARPPacket* arp   = reinterpret_cast<const ARPPacket*>(arpData);

        // Helpers for byte order
        auto be16              = [](uint16_t v) -> uint16_t { return ethernet::fromBigEndian16(v); };
        auto be32              = [](uint32_t v) -> uint32_t { return ((v & 0xFF) << 24) | ((v & 0xFF00) << 8) | ((v & 0xFF0000) >> 8) | ((v >> 24) & 0xFF); };

        // Validate ARP packet format
        if (be16(arp->hardwareType) != ethernet::HARDWARE_TYPE_ETHERNET || be16(arp->protocolType) != ethernet::PROTOCOL_TYPE_IPV4 ||
            arp->macAddressSize != ethernet::MAC_ADDRESS_SIZE || arp->ipAddressSize != ethernet::IPV4_ADDRESS_SIZE) {
            LOG_DEBUG("ARP: Invalid packet format");
            return false;
        }

        // Update cache with sender information (useful for reverse lookups)
        uint16_t op       = be16(arp->operation);
        uint32_t senderIP = be32(arp->senderIP);
        uint32_t targetIP = be32(arp->targetIP);
        if (op == OPERATION_REQUEST || op == OPERATION_REPLY) { addCacheEntry(senderIP, arp->senderMAC); }

        // Handle REQUEST
        if (op == OPERATION_REQUEST) {
            // Check if request is for our IP
            if (targetIP == localIP_) {
                LOG_DEBUG("ARP: Request for our IP, sending reply");
                sendARPReply(senderIP, arp->senderMAC);
            }
            return true;
        }

        // Handle REPLY
        if (op == OPERATION_REPLY) {
            LOG_DEBUG("ARP: Received reply");
            return true;
        }

        LOG_DEBUG("ARP: Unknown operation %u", op);
        return false;
    }

    // ==================== Packet Transmission ====================

    bool ARP::sendARPRequest(uint32_t targetIP) {
        if (!initialized_) {
            LOG_ERROR("ARP: Not initialized");
            return false;
        }

        // Allocate Ethernet frame buffer
        uint8_t frame[ethernet::MAX_FRAME_SIZE];

        // Build Ethernet header
        ethernet::FrameHeader* eth = reinterpret_cast<ethernet::FrameHeader*>(frame);
        memcpy(eth->destMAC, ethernet::BROADCAST_MAC, ethernet::MAC_ADDRESS_SIZE);
        memcpy(eth->srcMAC, localMAC_, ethernet::MAC_ADDRESS_SIZE);
        eth->etherType      = ethernet::toBigEndian16(ethernet::ETHERTYPE_ARP);

        // Build ARP packet
        ARPPacket* arp      = reinterpret_cast<ARPPacket*>(frame + ethernet::HEADER_SIZE);
        arp->hardwareType   = ethernet::toBigEndian16(ethernet::HARDWARE_TYPE_ETHERNET);
        arp->protocolType   = ethernet::toBigEndian16(ethernet::PROTOCOL_TYPE_IPV4);
        arp->macAddressSize = ethernet::MAC_ADDRESS_SIZE;
        arp->ipAddressSize  = ethernet::IPV4_ADDRESS_SIZE;
        arp->operation      = ethernet::toBigEndian16(OPERATION_REQUEST);

        memcpy(arp->senderMAC, localMAC_, ethernet::MAC_ADDRESS_SIZE);
        // Convert IPs to network byte order
        arp->senderIP = ((localIP_ & 0xFF) << 24) | ((localIP_ & 0xFF00) << 8) | ((localIP_ & 0xFF0000) >> 8) | ((localIP_ >> 24) & 0xFF);

        memset(arp->targetMAC, 0, ethernet::MAC_ADDRESS_SIZE);  // Unknown
        arp->targetIP        = ((targetIP & 0xFF) << 24) | ((targetIP & 0xFF00) << 8) | ((targetIP & 0xFF0000) >> 8) | ((targetIP >> 24) & 0xFF);

        // Calculate frame length
        uint32_t frameLength = ethernet::HEADER_SIZE + PACKET_SIZE;
        // Enforce minimum Ethernet frame size (pad if needed)
        if (frameLength < ethernet::MIN_FRAME_SIZE) {
            memset(frame + frameLength, 0, ethernet::MIN_FRAME_SIZE - frameLength);
            frameLength = ethernet::MIN_FRAME_SIZE;
        }

        // Send frame via network manager
        if (!NetworkManager::sendPacket(frame, frameLength)) {
            LOG_ERROR("ARP: Failed to send request");
            return false;
        }

        LOG_DEBUG("ARP: Request sent");
        return true;
    }

    bool ARP::sendARPReply(uint32_t targetIP, const uint8_t* targetMAC) {
        if (!initialized_) {
            LOG_ERROR("ARP: Not initialized");
            return false;
        }

        if (!targetMAC) {
            LOG_ERROR("ARP: Invalid target MAC");
            return false;
        }

        // Allocate Ethernet frame buffer
        uint8_t frame[ethernet::MAX_FRAME_SIZE];

        // Build Ethernet header
        ethernet::FrameHeader* eth = reinterpret_cast<ethernet::FrameHeader*>(frame);
        memcpy(eth->destMAC, targetMAC, ethernet::MAC_ADDRESS_SIZE);
        memcpy(eth->srcMAC, localMAC_, ethernet::MAC_ADDRESS_SIZE);
        eth->etherType      = ethernet::toBigEndian16(ethernet::ETHERTYPE_ARP);

        // Build ARP packet
        ARPPacket* arp      = reinterpret_cast<ARPPacket*>(frame + ethernet::HEADER_SIZE);
        arp->hardwareType   = ethernet::toBigEndian16(ethernet::HARDWARE_TYPE_ETHERNET);
        arp->protocolType   = ethernet::toBigEndian16(ethernet::PROTOCOL_TYPE_IPV4);
        arp->macAddressSize = ethernet::MAC_ADDRESS_SIZE;
        arp->ipAddressSize  = ethernet::IPV4_ADDRESS_SIZE;
        arp->operation      = ethernet::toBigEndian16(OPERATION_REPLY);

        memcpy(arp->senderMAC, localMAC_, ethernet::MAC_ADDRESS_SIZE);
        arp->senderIP = ((localIP_ & 0xFF) << 24) | ((localIP_ & 0xFF00) << 8) | ((localIP_ & 0xFF0000) >> 8) | ((localIP_ >> 24) & 0xFF);

        memcpy(arp->targetMAC, targetMAC, ethernet::MAC_ADDRESS_SIZE);
        arp->targetIP        = ((targetIP & 0xFF) << 24) | ((targetIP & 0xFF00) << 8) | ((targetIP & 0xFF0000) >> 8) | ((targetIP >> 24) & 0xFF);

        // Calculate frame length
        uint32_t frameLength = ethernet::HEADER_SIZE + PACKET_SIZE;
        // Enforce minimum frame size
        if (frameLength < ethernet::MIN_FRAME_SIZE) {
            memset(frame + frameLength, 0, ethernet::MIN_FRAME_SIZE - frameLength);
            frameLength = ethernet::MIN_FRAME_SIZE;
        }

        // Send frame via network manager
        if (!NetworkManager::sendPacket(frame, frameLength)) {
            LOG_ERROR("ARP: Failed to send reply");
            return false;
        }

        LOG_DEBUG("ARP: Reply sent");
        return true;
    }

    // ==================== Helper Methods ====================

    ARP::CacheEntry* ARP::findCacheEntry(uint32_t ipAddress) {
        for (uint8_t i = 0; i < cacheCount_; ++i) {
            if (cache_[i].valid && cache_[i].ipAddress == ipAddress) { return &cache_[i]; }
        }
        return nullptr;
    }

    bool ARP::isCacheEntryExpired(const CacheEntry* entry) {
        if (!entry) return true;

        // Simplified: assume never expires for now (no real timer)
        // In production, would track actual time
        return false;
    }

    uint32_t ARP::getSystemTime() {
        // Simplified: return dummy time
        // In production, would get actual system timer
        static uint32_t dummyTime = 0;
        return ++dummyTime;
    }

}  // namespace PalmyraOS::kernel
