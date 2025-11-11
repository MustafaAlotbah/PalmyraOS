#include "palmyraOS/network.h"
#include "libs/memory.h"  // For memset, memcpy
#include "libs/string.h"  // For strlen, strncpy
#include "palmyraOS/errono.h"
#include "palmyraOS/stdio.h"   // For snprintf
#include "palmyraOS/unistd.h"  // For socket syscalls

// ==================== DNS Implementation (Userspace) ====================

// DNS header structure (RFC 1035)
struct DNSHeader {
    uint16_t id;           // Transaction ID
    uint16_t flags;        // Flags (QR, Opcode, AA, TC, RD, RA, Z, RCODE)
    uint16_t questions;    // Number of questions
    uint16_t answers;      // Number of answer RRs
    uint16_t authorities;  // Number of authority RRs
    uint16_t additionals;  // Number of additional RRs
} __attribute__((packed));

// DNS question structure
struct DNSQuestion {
    // Name is variable length (encoded), followed by:
    uint16_t type;    // Query type (A = 1)
    uint16_t class_;  // Query class (IN = 1)
} __attribute__((packed));

// DNS answer structure (simplified)
struct DNSAnswer {
    uint16_t name;    // Name (pointer or label)
    uint16_t type;    // Type (A = 1)
    uint16_t class_;  // Class (IN = 1)
    uint32_t ttl;     // Time to live
    uint16_t length;  // Data length
    // Data follows (4 bytes for IPv4)
} __attribute__((packed));

// Helper: Encode domain name in DNS format
// "google.com" -> "\x06google\x03com\x00"
static int encodeDNSName(const char* hostname, uint8_t* buffer, size_t bufferSize) {
    size_t hostnameLen = strlen(hostname);
    if (hostnameLen == 0 || hostnameLen > 253) return -EINVAL;
    if (bufferSize < hostnameLen + 2) return -E2BIG;

    size_t writePos     = 0;
    size_t segmentStart = 0;

    for (size_t i = 0; i <= hostnameLen; ++i) {
        if (i == hostnameLen || hostname[i] == '.') {
            size_t segmentLen = i - segmentStart;
            if (segmentLen == 0 || segmentLen > 63) return -EINVAL;

            buffer[writePos++] = (uint8_t) segmentLen;
            for (size_t j = 0; j < segmentLen; ++j) {
                char c = hostname[segmentStart + j];
                // Convert to lowercase (DNS is case-insensitive)
                if (c >= 'A' && c <= 'Z') c = c + ('a' - 'A');
                buffer[writePos++] = c;
            }
            segmentStart = i + 1;
        }
    }
    buffer[writePos++] = 0;  // Null terminator
    return (int) writePos;
}

// Helper: Parse DNS response and extract IP
static int parseDNSResponse(const uint8_t* response, size_t responseLen, uint32_t* outIP) {
    if (responseLen < sizeof(DNSHeader)) return -EINVAL;

    const DNSHeader* header = (const DNSHeader*) response;

    // Convert from network byte order to host byte order
    uint16_t answers        = ntohs(header->answers);  // Network to host!
    if (answers == 0) return -ENOENT;

    // Skip header
    size_t offset = sizeof(DNSHeader);

    // Skip question section
    // Question format: <name> <type> <class>
    // Name is variable length, ends with 0x00
    while (offset < responseLen && response[offset] != 0) {
        uint8_t labelLen = response[offset];
        if (labelLen > 63) return -EINVAL;  // Invalid label
        offset += labelLen + 1;
    }
    offset++;     // Skip null terminator
    offset += 4;  // Skip type (2 bytes) + class (2 bytes)

    if (offset >= responseLen) return -EINVAL;

    // Parse answer section
    while (answers > 0 && offset < responseLen) {
        // Answer name (can be pointer or label)
        if ((response[offset] & 0xC0) == 0xC0) {
            // Compressed name (pointer)
            offset += 2;
        }
        else {
            // Full name (skip to null terminator)
            while (offset < responseLen && response[offset] != 0) { offset += response[offset] + 1; }
            offset++;
        }

        if (offset + 10 > responseLen) return -EINVAL;

        uint16_t type    = (response[offset] << 8) | response[offset + 1];
        uint16_t dataLen = (response[offset + 8] << 8) | response[offset + 9];
        offset += 10;

        // Check if this is an A record (IPv4)
        if (type == 1 && dataLen == 4) {
            // Extract IPv4 address (network byte order -> host byte order)
            uint32_t ip =
                    ((uint32_t) response[offset] << 24) | ((uint32_t) response[offset + 1] << 16) | ((uint32_t) response[offset + 2] << 8) | ((uint32_t) response[offset + 3]);
            *outIP = ip;
            return 0;
        }

        offset += dataLen;
        answers--;
    }

    return -ENOENT;
}

int gethostbyname_dns(const char* hostname, uint32_t* outIP, uint32_t dnsServer, uint32_t timeout_ms) {
    if (!hostname || !outIP) return -EINVAL;
    if (timeout_ms == 0) timeout_ms = DEFAULT_NETWORK_TIMEOUT_MS;

    // Create UDP socket
    int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) {
        printf("gethostbyname: socket() failed with %d\n", sockfd);
        return sockfd;
    }
    printf("gethostbyname: Created socket fd=%d\n", sockfd);

    // Don't bind - UDPSocket::sendto() will auto-allocate ephemeral port
    // This avoids port conflicts with kernel DNS (which uses 54321)

    // Set non-blocking mode for timeout support
    int nonblock = 1;
    ioctl(sockfd, FIONBIO, &nonblock);

    // Build DNS query packet
    uint8_t queryPacket[512];
    memset(queryPacket, 0, sizeof(queryPacket));

    DNSHeader* header   = (DNSHeader*) queryPacket;
    header->id          = htons(0x1234);  // Transaction ID
    header->flags       = htons(0x0100);  // Standard query, recursion desired
    header->questions   = htons(1);
    header->answers     = 0;
    header->authorities = 0;
    header->additionals = 0;

    size_t offset       = sizeof(DNSHeader);

    // Encode hostname
    int nameLen         = encodeDNSName(hostname, queryPacket + offset, sizeof(queryPacket) - offset);
    if (nameLen < 0) {
        close(sockfd);
        return nameLen;
    }
    offset += nameLen;

    // Add question (type A, class IN)
    if (offset + 4 > sizeof(queryPacket)) {
        close(sockfd);
        return -E2BIG;
    }
    queryPacket[offset++] = 0;  // Type A (high byte)
    queryPacket[offset++] = 1;  // Type A (low byte)
    queryPacket[offset++] = 0;  // Class IN (high byte)
    queryPacket[offset++] = 1;  // Class IN (low byte)

    // Send DNS query to DNS server
    struct sockaddr_in dnsAddr;
    dnsAddr.sin_family = AF_INET;
    dnsAddr.sin_port   = htons(DNS_PORT);
    dnsAddr.sin_addr   = htonl(dnsServer);
    memset(dnsAddr.sin_zero, 0, sizeof(dnsAddr.sin_zero));

    printf("gethostbyname: Sending DNS query (%u bytes) to server 0x%X:53\n", (uint32_t) offset, dnsServer);
    ssize_t sentBytes = sendto(sockfd, (const char*) queryPacket, offset, 0, (const struct sockaddr*) &dnsAddr, sizeof(dnsAddr));
    if (sentBytes < 0) {
        printf("gethostbyname: sendto() failed with %d\n", (int) sentBytes);
        close(sockfd);
        return (int) sentBytes;
    }
    printf("gethostbyname: Sent %d bytes, waiting for response...\n", (int) sentBytes);

    // Wait for DNS response (with timeout)
    uint8_t responsePacket[512];
    struct sockaddr_in srcAddr;
    uint32_t srcAddrLen  = sizeof(srcAddr);

    // Simple polling loop for timeout
    uint32_t startTime   = 0;  // TODO: Get actual time
    uint32_t elapsedTime = 0;
    uint32_t retries     = 0;

    while (elapsedTime < timeout_ms) {
        ssize_t recvBytes = recvfrom(sockfd, (char*) responsePacket, sizeof(responsePacket), 0, (struct sockaddr*) &srcAddr, &srcAddrLen);

        if (recvBytes > 0) {
            // Got response, parse it
            printf("gethostbyname: Received %d bytes from DNS server\n", (int) recvBytes);
            int result = parseDNSResponse(responsePacket, recvBytes, outIP);
            if (result == 0) { printf("gethostbyname: Successfully resolved to IP 0x%X\n", *outIP); }
            else { printf("gethostbyname: Parse failed with error %d\n", result); }
            close(sockfd);
            return result;
        }
        // Treat both -EAGAIN (non-blocking) and 0 (our blocking stub) as "try again"
        else if (recvBytes == -EAGAIN || recvBytes == 0) {
            // no data yet, keep polling
        }
        else {
            // Real error
            printf("gethostbyname: recvfrom() error: %d\n", (int) recvBytes);
            close(sockfd);
            return (int) recvBytes;
        }

        // Sleep for a bit before retrying
        retries++;
        sched_yield();
        elapsedTime += 10;  // Approximate
    }

    printf("gethostbyname: Timeout after %u retries\n", retries);
    close(sockfd);
    return -ETIMEDOUT;
}

int gethostbyname(const char* hostname, uint32_t* outIP, uint32_t timeout_ms) {
    // Try a list of DNS servers for robustness (some environments use NAT DNS 10.0.2.3)
    const uint32_t candidates[] = {
            DNS_SERVER_DEFAULT,            // Build-time default
            0x0A000203,                    // 10.0.2.3 (VirtualBox NAT DNS)
            DNS_SERVER_CLOUDFLARE_PRIMARY  // 1.1.1.1
    };

    for (uint32_t i = 0; i < sizeof(candidates) / sizeof(candidates[0]); ++i) {
        int r = gethostbyname_dns(hostname, outIP, candidates[i], timeout_ms ? timeout_ms : DEFAULT_NETWORK_TIMEOUT_MS);
        if (r == 0) return 0;
    }
    return -ETIMEDOUT;
}

// ==================== ICMP Ping (Pure Userspace via Raw Sockets) ====================

// ICMP Echo Request/Reply structures (RFC 792)
struct ICMPHeader {
    uint8_t type;       // 8 = Echo Request, 0 = Echo Reply
    uint8_t code;       // 0 for echo
    uint16_t checksum;  // ICMP checksum
    uint16_t id;        // Identifier (for matching replies)
    uint16_t sequence;  // Sequence number
} __attribute__((packed));

// Calculate ICMP checksum (one's complement sum)
static uint16_t calculateICMPChecksum(const uint8_t* data, size_t length) {
    uint32_t sum        = 0;
    const uint16_t* ptr = (const uint16_t*) data;

    // Sum 16-bit words
    while (length > 1) {
        sum += *ptr++;
        length -= 2;
    }

    // Add leftover byte if odd length
    if (length > 0) { sum += *(uint8_t*) ptr; }

    // Fold 32-bit sum to 16 bits
    while (sum >> 16) { sum = (sum & 0xFFFF) + (sum >> 16); }

    return ~sum;  // One's complement
}

int ping(uint32_t targetIP, uint32_t* outRTTms, uint32_t timeout_ms) {
    if (!outRTTms) return -EINVAL;
    if (timeout_ms == 0) timeout_ms = DEFAULT_NETWORK_TIMEOUT_MS;

    // Create raw ICMP socket (Linux way)
    int sockfd = socket(AF_INET, SOCK_RAW, IPPROTO_ICMP);
    if (sockfd < 0) {
        printf("ping: Failed to create raw socket: %d\n", sockfd);
        return sockfd;
    }

    // Set non-blocking mode
    int nonblock = 1;
    ioctl(sockfd, FIONBIO, &nonblock);

    // Build ICMP Echo Request
    uint8_t packet[64];  // ICMP header + 56 bytes data
    memset(packet, 0, sizeof(packet));

    ICMPHeader* icmp = (ICMPHeader*) packet;
    icmp->type       = 8;  // Echo Request
    icmp->code       = 0;
    icmp->id         = htons(get_pid() & 0xFFFF);  // Use PID as ID
    icmp->sequence   = htons(1);
    icmp->checksum   = 0;

    // Fill data with pattern
    for (size_t i = sizeof(ICMPHeader); i < sizeof(packet); ++i) { packet[i] = (uint8_t) i; }

    // Calculate checksum
    icmp->checksum = calculateICMPChecksum(packet, sizeof(packet));

    // Send Echo Request
    struct sockaddr_in dest;
    dest.sin_family = AF_INET;
    dest.sin_port   = 0;  // Ignored for ICMP
    dest.sin_addr   = htonl(targetIP);
    memset(dest.sin_zero, 0, sizeof(dest.sin_zero));

    ssize_t sent = sendto(sockfd, (const char*) packet, sizeof(packet), 0, (const struct sockaddr*) &dest, sizeof(dest));
    if (sent < 0) {
        printf("ping: sendto() failed: %d\n", (int) sent);
        close(sockfd);
        return (int) sent;
    }

    // Wait for Echo Reply (poll with timeout)
    uint8_t recvBuf[1500];
    struct sockaddr_in from;
    uint32_t fromLen   = sizeof(from);
    uint32_t startTime = 0;  // TODO: Use actual time
    uint32_t elapsed   = 0;

    while (elapsed < timeout_ms) {
        ssize_t recvd = recvfrom(sockfd, (char*) recvBuf, sizeof(recvBuf), 0, (struct sockaddr*) &from, &fromLen);

        if (recvd > 0) {
            // Got ICMP packet, check if it's our Echo Reply
            ICMPHeader* reply = (ICMPHeader*) recvBuf;

            if (reply->type == 0 && ntohs(reply->id) == (get_pid() & 0xFFFF)) {
                // Echo Reply from our request!
                uint32_t fromIP = ntohl(from.sin_addr);
                if (fromIP == targetIP) {
                    *outRTTms = 1;  // TODO: Calculate actual RTT
                    close(sockfd);
                    return 0;
                }
            }
        }
        else if (recvd != -EAGAIN && recvd != 0) {
            // Real error
            close(sockfd);
            return (int) recvd;
        }

        sched_yield();
        elapsed += 10;
    }

    close(sockfd);
    return -ETIMEDOUT;
}

int ping_host(const char* hostname, uint32_t* outRTTms, uint32_t timeout_ms) {
    if (!hostname || !outRTTms) return -EINVAL;

    // Try to parse as IP address first
    uint32_t ip = inet_addr(hostname);
    if (ip != 0) {
        // Valid IP, ping directly
        return ping(ip, outRTTms, timeout_ms);
    }

    // Resolve hostname
    int result = gethostbyname(hostname, &ip, timeout_ms / 2);
    if (result != 0) return result;

    // Ping resolved IP
    return ping(ip, outRTTms, timeout_ms / 2);
}

// ==================== IP Address Utilities ====================

uint32_t inet_addr(const char* ipString) {
    if (!ipString) return 0;

    uint8_t octets[4] = {0};
    int octetIndex    = 0;
    int currentValue  = 0;
    bool hasDigit     = false;

    for (size_t i = 0; ipString[i] != '\0'; ++i) {
        if (ipString[i] >= '0' && ipString[i] <= '9') {
            currentValue = currentValue * 10 + (ipString[i] - '0');
            hasDigit     = true;

            if (currentValue > 255) return 0;  // Invalid octet
        }
        else if (ipString[i] == '.') {
            if (!hasDigit || octetIndex >= 3) return 0;  // Invalid format

            octets[octetIndex++] = (uint8_t) currentValue;
            currentValue         = 0;
            hasDigit             = false;
        }
        else {
            return 0;  // Invalid character
        }
    }

    // Store last octet
    if (!hasDigit || octetIndex != 3) return 0;  // Must have 4 octets
    octets[octetIndex] = (uint8_t) currentValue;

    // Combine octets into 32-bit IP (host byte order)
    return ((uint32_t) octets[0] << 24) | ((uint32_t) octets[1] << 16) | ((uint32_t) octets[2] << 8) | ((uint32_t) octets[3]);
}

const char* inet_ntoa_r(uint32_t ip, char* buffer, uint32_t bufferSize) {
    if (!buffer || bufferSize < 16) return nullptr;

    uint8_t b0 = (ip >> 24) & 0xFF;
    uint8_t b1 = (ip >> 16) & 0xFF;
    uint8_t b2 = (ip >> 8) & 0xFF;
    uint8_t b3 = ip & 0xFF;

    snprintf(buffer, bufferSize, "%u.%u.%u.%u", b0, b1, b2, b3);
    return buffer;
}

const char* inet_ntoa(uint32_t ip) {
    static char buffer[16];
    return inet_ntoa_r(ip, buffer, sizeof(buffer));
}
