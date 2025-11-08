#include "core/network/IPv4.h"
#include "core/network/ARP.h"
#include "core/network/Ethernet.h"
#include "core/network/ICMP.h"
#include "core/network/NetworkManager.h"
#include "core/peripherals/Logger.h"
#include "libs/memory.h"
#include "libs/string.h"

namespace PalmyraOS::kernel {

    // ==================== Static Member Initialization ====================

    bool IPv4::initialized_    = false;
    uint32_t IPv4::localIP_    = 0;
    uint32_t IPv4::subnetMask_ = 0;
    uint32_t IPv4::gateway_    = 0;

    // ==================== Lifecycle ====================

    bool IPv4::initialize(uint32_t localIP, uint32_t subnetMask, uint32_t gateway) {
        if (initialized_) {
            LOG_WARN("IPv4: Already initialized");
            return true;
        }

        localIP_            = localIP;
        subnetMask_         = subnetMask;
        gateway_            = gateway;
        initialized_        = true;

        // Format IP for display
        uint8_t ip_bytes[4] = {static_cast<uint8_t>((localIP >> 24) & 0xFF),
                               static_cast<uint8_t>((localIP >> 16) & 0xFF),
                               static_cast<uint8_t>((localIP >> 8) & 0xFF),
                               static_cast<uint8_t>(localIP & 0xFF)};

        LOG_INFO("IPv4: Initialized with IP %u.%u.%u.%u", ip_bytes[0], ip_bytes[1], ip_bytes[2], ip_bytes[3]);

        return true;
    }

    // ==================== Packet Processing ====================

    bool IPv4::handleIPv4Packet(const uint8_t* frame, uint32_t frameLength) {
        if (!initialized_) {
            LOG_WARN("IPv4: Not initialized");
            return false;
        }

        // Validate frame size: Ethernet header + IPv4 header minimum
        if (frameLength < ethernet::HEADER_SIZE + HEADER_SIZE) {
            LOG_DEBUG("IPv4: Frame too small");
            return false;
        }

        // Skip Ethernet header
        const uint8_t* ipData = frame + ethernet::HEADER_SIZE;
        const Header* header  = reinterpret_cast<const Header*>(ipData);

        auto fromBigEndian16  = [](uint16_t value) -> uint16_t { return static_cast<uint16_t>(((value & 0x00FF) << 8) | ((value & 0xFF00) >> 8)); };
        auto fromBigEndian32  = [](uint32_t value) -> uint32_t {
            return ((value & 0x000000FF) << 24) | ((value & 0x0000FF00) << 8) | ((value & 0x00FF0000) >> 8) | ((value & 0xFF000000) >> 24);
        };

        // Validate IPv4 header
        uint8_t version = (header->versionAndIHL >> 4) & 0x0F;
        if (version != VERSION) {
            LOG_DEBUG("IPv4: Invalid version %u", version);
            return false;
        }

        // Check TTL
        if (header->ttl == 0) {
            LOG_DEBUG("IPv4: TTL expired");
            return false;  // TODO: Send ICMP Time Exceeded
        }

        // Extract payload info first
        uint16_t totalLength   = fromBigEndian16(header->totalLength);
        uint8_t ihl            = (header->versionAndIHL & 0x0F) * 4;  // Header length in bytes
        uint32_t payloadLength = totalLength - ihl;

        // Check if packet is for us
        uint32_t destIP        = fromBigEndian32(header->destIP);
        if (destIP != localIP_) {
            LOG_DEBUG("IPv4: Packet not for us (dest=%u.%u.%u.%u, local=%u.%u.%u.%u)",
                      (destIP >> 24) & 0xFF,
                      (destIP >> 16) & 0xFF,
                      (destIP >> 8) & 0xFF,
                      destIP & 0xFF,
                      (localIP_ >> 24) & 0xFF,
                      (localIP_ >> 16) & 0xFF,
                      (localIP_ >> 8) & 0xFF,
                      localIP_ & 0xFF);
            return false;  // TODO: Forward if routing enabled
        }

        LOG_INFO("IPv4:  Received packet for us (protocol=%u, length=%u bytes)", header->protocol, totalLength);

        if (payloadLength > frameLength - ethernet::HEADER_SIZE - ihl) {
            LOG_DEBUG("IPv4: Invalid payload length");
            return false;
        }

        const uint8_t* payload = ipData + ihl;
        uint32_t sourceIP      = fromBigEndian32(header->sourceIP);

        // Route based on protocol
        return routePacket(sourceIP, header->protocol, payload, payloadLength);
    }

    bool IPv4::sendPacket(uint32_t destIP, uint8_t protocol, const uint8_t* payload, uint32_t payloadLength) {
        if (!initialized_) {
            LOG_ERROR("IPv4: Not initialized");
            return false;
        }

        // Allocate frame buffer
        uint32_t frameSize = ethernet::HEADER_SIZE + HEADER_SIZE + payloadLength;
        if (frameSize > ethernet::MAX_FRAME_SIZE) {
            LOG_ERROR("IPv4: Packet too large");
            return false;
        }

        uint8_t frame[ethernet::MAX_FRAME_SIZE];

        // Build Ethernet header
        ethernet::FrameHeader* eth = reinterpret_cast<ethernet::FrameHeader*>(frame);
        memset(eth->destMAC, 0, ethernet::MAC_ADDRESS_SIZE);  // Will fill with ARP
        memset(eth->srcMAC, 0, ethernet::MAC_ADDRESS_SIZE);   // Will fill from interface
        eth->etherType     = ethernet::toBigEndian16(ethernet::ETHERTYPE_IPV4);

        auto toBigEndian32 = [](uint32_t value) -> uint32_t {
            return ((value & 0x000000FF) << 24) | ((value & 0x0000FF00) << 8) | ((value & 0x00FF0000) >> 8) | ((value & 0xFF000000) >> 24);
        };

        // Build IPv4 header
        Header* ipHeader         = reinterpret_cast<Header*>(frame + ethernet::HEADER_SIZE);
        ipHeader->versionAndIHL  = (VERSION << 4) | (HEADER_SIZE / 4);
        ipHeader->dscpAndECN     = 0;
        uint16_t totalLength     = static_cast<uint16_t>(HEADER_SIZE + payloadLength);
        ipHeader->totalLength    = ethernet::toBigEndian16(totalLength);
        ipHeader->identification = ethernet::toBigEndian16(0x1234);
        ipHeader->flagsAndOffset = ethernet::toBigEndian16(0x0000);  // No fragmentation
        ipHeader->ttl            = DEFAULT_TTL;
        ipHeader->protocol       = protocol;
        ipHeader->checksum       = 0;
        ipHeader->sourceIP       = toBigEndian32(localIP_);
        ipHeader->destIP         = toBigEndian32(destIP);

        // Calculate checksum
        ipHeader->checksum       = ethernet::toBigEndian16(calculateChecksum(reinterpret_cast<uint8_t*>(ipHeader)));

        // Copy payload
        memcpy(frame + ethernet::HEADER_SIZE + HEADER_SIZE, payload, payloadLength);

        // Determine destination MAC (local or gateway)
        uint32_t nextHopIP = isLocalAddress(destIP) ? destIP : gateway_;
        uint8_t destMAC[ethernet::MAC_ADDRESS_SIZE];

        if (!ARP::resolveMacAddress(nextHopIP, destMAC)) {
            LOG_ERROR("IPv4: Cannot resolve MAC for next hop");
            return false;
        }

        // Fill Ethernet destination MAC
        memcpy(eth->destMAC, destMAC, ethernet::MAC_ADDRESS_SIZE);

        // Get source MAC from interface
        auto defaultIface = NetworkManager::getDefaultInterface();
        if (!defaultIface) {
            LOG_ERROR("IPv4: No network interface available");
            return false;
        }
        memcpy(eth->srcMAC, defaultIface->getMACAddress(), ethernet::MAC_ADDRESS_SIZE);

        // Send via network manager
        return NetworkManager::sendPacket(frame, frameSize);
    }

    // ==================== Address Information ====================

    bool IPv4::isLocalAddress(uint32_t ip) {
        // Check if IP is on same subnet
        return (ip & subnetMask_) == (localIP_ & subnetMask_);
    }

    uint16_t IPv4::calculateChecksum(const uint8_t* header) {
        uint32_t sum = 0;

        // Sum all 16-bit words
        for (size_t i = 0; i < HEADER_SIZE; i += 2) {
            uint16_t word = (header[i] << 8) | header[i + 1];
            sum += word;
        }

        // Fold carries
        while (sum >> 16) { sum = (sum & 0xFFFF) + (sum >> 16); }

        // Return one's complement
        return static_cast<uint16_t>(~sum);
    }

    // ==================== Helper Methods ====================

    bool IPv4::routePacket(uint32_t sourceIP, uint8_t protocol, const uint8_t* payload, uint32_t payloadLength) {
        LOG_DEBUG("IPv4: Routing packet protocol=%u", protocol);

        if (protocol == PROTOCOL_ICMP) { return ICMP::handleICMPPacket(payload, payloadLength, sourceIP); }

        // TODO: UDP and TCP routing

        LOG_DEBUG("IPv4: Unsupported protocol %u", protocol);
        return false;
    }

    const char* IPv4::ipToString(uint32_t ip, char* buffer) {
        uint8_t b[4] = {static_cast<uint8_t>((ip >> 24) & 0xFF), static_cast<uint8_t>((ip >> 16) & 0xFF), static_cast<uint8_t>((ip >> 8) & 0xFF), static_cast<uint8_t>(ip & 0xFF)};
        sprintf(buffer, "%u.%u.%u.%u", b[0], b[1], b[2], b[3]);
        return buffer;
    }

}  // namespace PalmyraOS::kernel
