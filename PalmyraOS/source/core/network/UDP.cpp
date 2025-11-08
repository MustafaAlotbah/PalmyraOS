#include "core/network/UDP.h"
#include "core/network/Ethernet.h"
#include "core/network/IPv4.h"
#include "core/peripherals/Logger.h"
#include "libs/memory.h"
#include "libs/string.h"

namespace PalmyraOS::kernel {

    // ==================== Static Member Initialization ====================

    bool UDP::initialized_                               = false;
    uint16_t UDP::nextEphemeralPort_                     = DYNAMIC_PORT_START;
    UDP::PortBinding UDP::portBindings_[MAX_BOUND_PORTS] = {};
    uint8_t UDP::boundPortCount_                         = 0;

    // ==================== Lifecycle ====================

    bool UDP::initialize() {
        if (initialized_) {
            LOG_WARN("UDP: Already initialized");
            return true;
        }

        // Clear port binding table
        for (uint8_t i = 0; i < MAX_BOUND_PORTS; ++i) {
            portBindings_[i].port    = 0;
            portBindings_[i].handler = nullptr;
            portBindings_[i].active  = false;
        }

        boundPortCount_    = 0;
        nextEphemeralPort_ = DYNAMIC_PORT_START;
        initialized_       = true;

        LOG_INFO("UDP: Initialized (supports up to %u bound ports)", MAX_BOUND_PORTS);
        return true;
    }

    // ==================== Datagram Transmission ====================

    bool UDP::sendDatagram(uint32_t destinationIP, uint16_t destinationPort, uint16_t sourcePort, const uint8_t* data, uint32_t dataLength) {
        if (!initialized_) {
            LOG_ERROR("UDP: Not initialized");
            return false;
        }

        if (dataLength > MAX_DATAGRAM_SIZE) {
            LOG_ERROR("UDP: Datagram too large (%u bytes, maximum %u bytes)", dataLength, MAX_DATAGRAM_SIZE);
            return false;
        }

        // Auto-allocate ephemeral port if source port is 0
        uint16_t actualSourcePort = sourcePort;
        if (actualSourcePort == 0) {
            actualSourcePort = allocateEphemeralPort();
            if (actualSourcePort == 0) {
                LOG_ERROR("UDP: Failed to allocate ephemeral port");
                return false;
            }
        }

        // Calculate total UDP datagram length (header + data)
        const uint32_t datagramLength = HEADER_SIZE + dataLength;

        // Build UDP header in temporary buffer
        uint8_t datagram[MAX_DATAGRAM_SIZE];
        Header* udpHeader          = reinterpret_cast<Header*>(datagram);

        udpHeader->sourcePort      = ethernet::toBigEndian16(actualSourcePort);
        udpHeader->destinationPort = ethernet::toBigEndian16(destinationPort);
        udpHeader->length          = ethernet::toBigEndian16(static_cast<uint16_t>(datagramLength));
        udpHeader->checksum        = 0;  // Will calculate below

        // Copy payload data after UDP header
        if (data && dataLength > 0) { memcpy(datagram + HEADER_SIZE, data, dataLength); }

        // Calculate checksum with pseudo-header
        uint32_t localIP                    = IPv4::getLocalIP();
        const uint16_t checksumHostOrder    = calculateChecksumWithPseudoHeader(localIP, destinationIP, udpHeader, data, dataLength);

        const uint16_t checksumNetworkOrder = ethernet::toBigEndian16(checksumHostOrder == 0 ? static_cast<uint16_t>(0xFFFF) : checksumHostOrder);

        udpHeader->checksum                 = checksumNetworkOrder;

        // Send via IPv4 layer
        return IPv4::sendPacket(destinationIP, IPv4::PROTOCOL_UDP, datagram, datagramLength);
    }

    // ==================== Packet Handling ====================

    bool UDP::handleUDPPacket(const uint8_t* payload, uint32_t payloadLength, uint32_t sourceIP, uint32_t destinationIP) {
        LOG_INFO("UDP: Received datagram (%u bytes) from %u.%u.%u.%u", payloadLength, (sourceIP >> 24) & 0xFF, (sourceIP >> 16) & 0xFF, (sourceIP >> 8) & 0xFF, sourceIP & 0xFF);

        if (!initialized_) {
            LOG_WARN("UDP: Not initialized, discarding packet");
            return false;
        }

        // Validate minimum size (UDP header = 8 bytes)
        if (payloadLength < HEADER_SIZE) {
            LOG_WARN("UDP: Packet too small (%u bytes, minimum %u bytes)", payloadLength, HEADER_SIZE);
            return false;
        }

        // Parse UDP header
        const Header* udpHeader         = reinterpret_cast<const Header*>(payload);

        // Convert fields from network byte order
        const uint16_t sourcePort       = ethernet::fromBigEndian16(udpHeader->sourcePort);
        const uint16_t destinationPort  = ethernet::fromBigEndian16(udpHeader->destinationPort);
        const uint16_t datagramLength   = ethernet::fromBigEndian16(udpHeader->length);
        const uint16_t receivedChecksum = ethernet::fromBigEndian16(udpHeader->checksum);

        LOG_INFO("UDP: Source port=%u, Dest port=%u, Length=%u bytes", sourcePort, destinationPort, datagramLength);

        // Validate datagram length
        if (datagramLength < HEADER_SIZE || datagramLength > payloadLength) {
            LOG_WARN("UDP: Invalid datagram length (%u bytes in header, %u bytes received)", datagramLength, payloadLength);
            return false;
        }

        // Verify checksum if present (checksum = 0 means no checksum)
        if (receivedChecksum != 0) {
            // Calculate expected checksum
            const uint8_t* udpData            = payload + HEADER_SIZE;
            const uint32_t dataLength         = datagramLength - HEADER_SIZE;

            // Create temporary header with checksum = 0 for validation
            Header tempHeader                 = *udpHeader;
            tempHeader.checksum               = 0;

            const uint16_t calculatedChecksum = calculateChecksumWithPseudoHeader(sourceIP, destinationIP, &tempHeader, udpData, dataLength);

            if (calculatedChecksum != receivedChecksum) {
                LOG_WARN("UDP: Checksum mismatch (expected 0x%04X, got 0x%04X)", calculatedChecksum, receivedChecksum);
                return false;
            }
        }

        // Extract payload data
        const uint8_t* datagramData       = payload + HEADER_SIZE;
        const uint32_t datagramDataLength = datagramLength - HEADER_SIZE;

        // Dispatch to port handler if bound
        PortBinding* binding              = findPortBinding(destinationPort);
        if (binding && binding->active && binding->handler) {
            binding->handler(sourceIP, sourcePort, datagramData, datagramDataLength);
            return true;
        }

        // No handler registered for this port (silently discard, as per UDP spec)
        return false;
    }

    // ==================== Port Management ====================

    bool UDP::bindPort(uint16_t port, DatagramHandler handler) {
        if (!initialized_) {
            LOG_ERROR("UDP: Not initialized");
            return false;
        }

        if (port == 0 || !handler) {
            LOG_ERROR("UDP: Invalid port or handler");
            return false;
        }

        // Check if port already bound
        if (findPortBinding(port)) {
            LOG_ERROR("UDP: Port %u already bound", port);
            return false;
        }

        // Find free slot in binding table
        if (boundPortCount_ >= MAX_BOUND_PORTS) {
            LOG_ERROR("UDP: Port binding table full (%u/%u ports)", boundPortCount_, MAX_BOUND_PORTS);
            return false;
        }

        // Add new binding
        for (uint8_t i = 0; i < MAX_BOUND_PORTS; ++i) {
            if (!portBindings_[i].active) {
                portBindings_[i].port    = port;
                portBindings_[i].handler = handler;
                portBindings_[i].active  = true;
                boundPortCount_++;

                LOG_INFO("UDP: Bound port %u to handler (total bindings: %u/%u)", port, boundPortCount_, MAX_BOUND_PORTS);
                return true;
            }
        }

        return false;  // Should never reach here
    }

    bool UDP::unbindPort(uint16_t port) {
        if (!initialized_) { return false; }

        PortBinding* binding = findPortBinding(port);
        if (!binding) {
            LOG_WARN("UDP: Port %u not bound", port);
            return false;
        }

        binding->active  = false;
        binding->handler = nullptr;
        boundPortCount_--;

        LOG_INFO("UDP: Unbound port %u (total bindings: %u/%u)", port, boundPortCount_, MAX_BOUND_PORTS);
        return true;
    }

    uint16_t UDP::allocateEphemeralPort() {
        // Try to find an unused port in the dynamic range
        constexpr uint16_t MAX_ATTEMPTS = 100;

        for (uint16_t attempt = 0; attempt < MAX_ATTEMPTS; ++attempt) {
            uint16_t candidatePort = nextEphemeralPort_++;

            // Wrap around at end of dynamic range
            if (nextEphemeralPort_ > DYNAMIC_PORT_END) { nextEphemeralPort_ = DYNAMIC_PORT_START; }

            // Check if port is already bound
            if (!findPortBinding(candidatePort)) {
                return candidatePort;  // Found unused port
            }
        }

        LOG_ERROR("UDP: Failed to allocate ephemeral port after %u attempts", MAX_ATTEMPTS);
        return 0;  // No ports available
    }

    // ==================== Checksum Calculation ====================

    uint16_t UDP::calculateChecksumWithPseudoHeader(uint32_t sourceIP, uint32_t destinationIP, const Header* udpHeader, const uint8_t* data, uint32_t dataLength) {

        uint32_t checksumAccumulator = 0;
        const uint16_t udpLengthHost = static_cast<uint16_t>(HEADER_SIZE + dataLength);

        uint8_t pseudoHeader[12];
        pseudoHeader[0]          = static_cast<uint8_t>((sourceIP >> 24) & 0xFF);
        pseudoHeader[1]          = static_cast<uint8_t>((sourceIP >> 16) & 0xFF);
        pseudoHeader[2]          = static_cast<uint8_t>((sourceIP >> 8) & 0xFF);
        pseudoHeader[3]          = static_cast<uint8_t>(sourceIP & 0xFF);
        pseudoHeader[4]          = static_cast<uint8_t>((destinationIP >> 24) & 0xFF);
        pseudoHeader[5]          = static_cast<uint8_t>((destinationIP >> 16) & 0xFF);
        pseudoHeader[6]          = static_cast<uint8_t>((destinationIP >> 8) & 0xFF);
        pseudoHeader[7]          = static_cast<uint8_t>(destinationIP & 0xFF);
        pseudoHeader[8]          = 0;
        pseudoHeader[9]          = IPv4::PROTOCOL_UDP;
        pseudoHeader[10]         = static_cast<uint8_t>((udpLengthHost >> 8) & 0xFF);
        pseudoHeader[11]         = static_cast<uint8_t>(udpLengthHost & 0xFF);

        auto addBufferToChecksum = [&checksumAccumulator](const uint8_t* buffer, size_t length) {
            for (size_t i = 0; i < length; i += 2) {
                uint16_t word = static_cast<uint16_t>(buffer[i] << 8);
                if (i + 1 < length) { word |= buffer[i + 1]; }
                checksumAccumulator += word;
            }
        };

        addBufferToChecksum(pseudoHeader, sizeof(pseudoHeader));
        addBufferToChecksum(reinterpret_cast<const uint8_t*>(udpHeader), HEADER_SIZE);

        if (data && dataLength > 0) { addBufferToChecksum(data, dataLength); }

        while (checksumAccumulator >> 16) { checksumAccumulator = (checksumAccumulator & 0xFFFF) + (checksumAccumulator >> 16); }

        uint16_t result = static_cast<uint16_t>(~checksumAccumulator);
        if (result == 0) { result = 0xFFFF; }
        return result;
    }

    // ==================== Helper Methods ====================

    UDP::PortBinding* UDP::findPortBinding(uint16_t port) {
        for (uint8_t i = 0; i < MAX_BOUND_PORTS; ++i) {
            if (portBindings_[i].active && portBindings_[i].port == port) { return &portBindings_[i]; }
        }
        return nullptr;
    }

}  // namespace PalmyraOS::kernel
