#include "core/network/ICMP.h"
#include "core/network/Ethernet.h"
#include "core/network/IPv4.h"
#include "core/network/NetworkManager.h"
#include "core/peripherals/Logger.h"
#include "libs/memory.h"
#include "libs/string.h"

namespace PalmyraOS::kernel {

    // ==================== Static Member Initialization ====================

    bool ICMP::initialized_            = false;
    ICMP::PingState ICMP::pendingPing_ = {0, 0, 0, 0, false, 0};

    // ==================== Lifecycle ====================

    bool ICMP::initialize() {
        if (initialized_) {
            LOG_WARN("ICMP: Already initialized");
            return true;
        }

        initialized_ = true;
        LOG_INFO("ICMP: Initialized (Echo support enabled)");
        return true;
    }

    // ==================== Ping Functionality ====================

    bool ICMP::ping(uint32_t targetIP, uint32_t* outRTTms) { return pingWithData(targetIP, nullptr, 0, outRTTms); }

    bool ICMP::pingWithData(uint32_t targetIP, const uint8_t* data, uint32_t dataLength, uint32_t* outRTTms) {
        if (!initialized_) {
            LOG_ERROR("ICMP: Not initialized");
            return false;
        }

        if (!outRTTms) {
            LOG_ERROR("ICMP: Invalid output RTT buffer");
            return false;
        }

        // Log ping target
        uint8_t ip_bytes[4] = {static_cast<uint8_t>((targetIP >> 24) & 0xFF),
                               static_cast<uint8_t>((targetIP >> 16) & 0xFF),
                               static_cast<uint8_t>((targetIP >> 8) & 0xFF),
                               static_cast<uint8_t>(targetIP & 0xFF)};
        LOG_INFO("ICMP: Sending ping to %u.%u.%u.%u", ip_bytes[0], ip_bytes[1], ip_bytes[2], ip_bytes[3]);

        // Build ICMP Echo Request message
        uint32_t messageSize = ECHO_HEADER_SIZE + dataLength;
        uint8_t message[256];

        EchoMessage* echo = reinterpret_cast<EchoMessage*>(message);
        echo->type        = TYPE_ECHO_REQUEST;
        echo->code        = CODE_ECHO;
        echo->checksum    = 0;
        echo->id          = ethernet::toBigEndian16(PING_ID);
        echo->sequence    = ethernet::toBigEndian16(1);

        // Copy optional data
        if (data && dataLength > 0) { memcpy(message + ECHO_HEADER_SIZE, data, dataLength); }

        // Calculate checksum
        echo->checksum             = ethernet::toBigEndian16(calculateChecksum(message, messageSize));

        // Set pending ping state
        pendingPing_.targetIP      = targetIP;
        pendingPing_.sentTime      = getSystemTimeMs();
        pendingPing_.id            = PING_ID;
        pendingPing_.sequence      = 1;
        pendingPing_.replyReceived = false;

        // Send via IPv4
        if (!IPv4::sendPacket(targetIP, IPv4::PROTOCOL_ICMP, message, messageSize)) {
            LOG_ERROR("ICMP: Failed to send ping");
            return false;
        }

        LOG_DEBUG("ICMP: Ping sent, waiting for reply (timeout %u ms)", PING_TIMEOUT_MS);

        // Wait for reply with timeout (poll network interface)
        uint32_t waitTime = 0;
        auto eth0         = NetworkManager::getDefaultInterface();
        while (waitTime < PING_TIMEOUT_MS && !pendingPing_.replyReceived) {
            // Poll network interface for incoming packets
            for (volatile uint32_t i = 0; i < 100000; ++i) {
                if ((i % 1000) == 0 && eth0) {
                    eth0->handleInterrupt();  // Process incoming packets
                }
            }
            waitTime += 10;  // Approximate 10ms per iteration
        }

        if (!pendingPing_.replyReceived) {
            LOG_WARN("ICMP: Ping timeout after %u ms", PING_TIMEOUT_MS);
            return false;
        }

        // Calculate RTT
        uint32_t rtt = pendingPing_.replyTime - pendingPing_.sentTime;
        *outRTTms    = rtt;

        LOG_INFO("ICMP: Ping successful! RTT: %u ms", rtt);
        return true;
    }

    // ==================== Packet Handling ====================

    bool ICMP::handleICMPPacket(const uint8_t* payload, uint32_t payloadLength, uint32_t sourceIP) {
        if (!initialized_) {
            LOG_WARN("ICMP: Not initialized");
            return false;
        }

        if (payloadLength < ECHO_HEADER_SIZE) {
            LOG_DEBUG("ICMP: Message too small");
            return false;
        }

        const EchoMessage* echo = reinterpret_cast<const EchoMessage*>(payload);
        uint16_t echoId         = ethernet::fromBigEndian16(echo->id);
        uint16_t echoSeq        = ethernet::fromBigEndian16(echo->sequence);

        // Handle Echo Reply
        if (echo->type == TYPE_ECHO_REPLY) {
            LOG_INFO("ICMP:  Received Echo Reply from %u.%u.%u.%u (ID=%u, Seq=%u)",
                     (sourceIP >> 24) & 0xFF,
                     (sourceIP >> 16) & 0xFF,
                     (sourceIP >> 8) & 0xFF,
                     sourceIP & 0xFF,
                     echoId,
                     echoSeq);

            // Check if this matches our pending ping
            if (pendingPing_.targetIP == sourceIP && echoId == pendingPing_.id && echoSeq == pendingPing_.sequence) {
                pendingPing_.replyReceived = true;
                pendingPing_.replyTime     = getSystemTimeMs();
                LOG_INFO("ICMP:  Reply MATCHES pending ping!");
                return true;
            }

            LOG_WARN("ICMP: Reply doesn't match (expected target=%u.%u.%u.%u ID=%u Seq=%u)",
                     (pendingPing_.targetIP >> 24) & 0xFF,
                     (pendingPing_.targetIP >> 16) & 0xFF,
                     (pendingPing_.targetIP >> 8) & 0xFF,
                     pendingPing_.targetIP & 0xFF,
                     pendingPing_.id,
                     pendingPing_.sequence);
        }

        // Handle Echo Request
        if (echo->type == TYPE_ECHO_REQUEST) {
            LOG_DEBUG("ICMP: Received Echo Request from %u.%u.%u.%u", (sourceIP >> 24) & 0xFF, (sourceIP >> 16) & 0xFF, (sourceIP >> 8) & 0xFF, sourceIP & 0xFF);

            // Extract echo data
            uint32_t dataLength     = payloadLength - ECHO_HEADER_SIZE;
            const uint8_t* echoData = payload + ECHO_HEADER_SIZE;

            // Send reply
            return sendEchoReply(sourceIP, echoId, echoSeq, echoData, dataLength);
        }

        LOG_DEBUG("ICMP: Unsupported type %u", echo->type);
        return false;
    }

    bool ICMP::sendEchoReply(uint32_t destIP, uint16_t id, uint16_t sequence, const uint8_t* data, uint32_t dataLength) {
        if (!initialized_) {
            LOG_ERROR("ICMP: Not initialized");
            return false;
        }

        // Build ICMP Echo Reply message
        uint32_t messageSize = ECHO_HEADER_SIZE + dataLength;
        uint8_t message[256];

        EchoMessage* echo = reinterpret_cast<EchoMessage*>(message);
        echo->type        = TYPE_ECHO_REPLY;
        echo->code        = CODE_ECHO;
        echo->checksum    = 0;
        echo->id          = ethernet::toBigEndian16(id);
        echo->sequence    = ethernet::toBigEndian16(sequence);

        // Copy echo data
        if (data && dataLength > 0) { memcpy(message + ECHO_HEADER_SIZE, data, dataLength); }

        // Calculate checksum
        echo->checksum = ethernet::toBigEndian16(calculateChecksum(message, messageSize));

        // Send via IPv4
        LOG_DEBUG("ICMP: Sending Echo Reply to %u.%u.%u.%u", (destIP >> 24) & 0xFF, (destIP >> 16) & 0xFF, (destIP >> 8) & 0xFF, destIP & 0xFF);

        return IPv4::sendPacket(destIP, IPv4::PROTOCOL_ICMP, message, messageSize);
    }

    // ==================== Helper Methods ====================

    uint16_t ICMP::calculateChecksum(const uint8_t* message, uint32_t length) {
        uint32_t sum = 0;

        // Sum all 16-bit words
        for (uint32_t i = 0; i < length; i += 2) {
            uint16_t word = (message[i] << 8);
            if (i + 1 < length) { word |= message[i + 1]; }
            sum += word;
        }

        // Fold carries
        while (sum >> 16) { sum = (sum & 0xFFFF) + (sum >> 16); }

        // Return one's complement
        return static_cast<uint16_t>(~sum);
    }

    uint32_t ICMP::getSystemTimeMs() {
        // Simplified: return dummy time
        // In production, would use actual system timer
        static uint32_t dummyTime = 0;
        return ++dummyTime;
    }

}  // namespace PalmyraOS::kernel
