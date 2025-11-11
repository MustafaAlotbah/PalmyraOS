#pragma once

#include <cstdint>

/* POSIX-compatible socket structures and constants */

#ifdef __cplusplus
extern "C" {
#endif

// ==================== Address Families ====================

#define AF_INET 2    // IPv4
#define AF_INET6 10  // IPv6 (future)

// ==================== Socket Types ====================

#define SOCK_STREAM 1  // TCP (connection-oriented, reliable, byte stream)
#define SOCK_DGRAM 2   // UDP (connectionless, unreliable, datagrams)
#define SOCK_RAW 3     // Raw IP (future)

// ==================== Protocols ====================

#define IPPROTO_IP 0    // Default protocol (auto-select based on socket type)
#define IPPROTO_ICMP 1  // Internet Control Message Protocol (for raw sockets)
#define IPPROTO_TCP 6   // Transmission Control Protocol
#define IPPROTO_UDP 17  // User Datagram Protocol

// ==================== Special Addresses ====================

#define INADDR_ANY ((uint32_t) 0x00000000)        // 0.0.0.0 - Bind to all interfaces
#define INADDR_LOOPBACK ((uint32_t) 0x7F000001)   // 127.0.0.1 - Loopback (host byte order)
#define INADDR_BROADCAST ((uint32_t) 0xFFFFFFFF)  // 255.255.255.255 - Broadcast

// ==================== Socket Options ====================

// Socket level
#define SOL_SOCKET 1  // Socket-level options

// Socket options (for setsockopt/getsockopt)
#define SO_REUSEADDR 2   // Allow reuse of local addresses
#define SO_TYPE 3        // Get socket type (read-only)
#define SO_ERROR 4       // Get and clear error status
#define SO_BROADCAST 6   // Enable broadcast messages
#define SO_SNDBUF 7      // Send buffer size
#define SO_RCVBUF 8      // Receive buffer size
#define SO_KEEPALIVE 9   // Keep connections alive
#define SO_OOBINLINE 10  // Leave out-of-band data inline

// ==================== Message Flags ====================

#define MSG_DONTWAIT 0x40  // Non-blocking operation
#define MSG_PEEK 0x02      // Peek at incoming messages
#define MSG_TRUNC 0x20     // Data was truncated
#define MSG_CTRUNC 0x08    // Control data was truncated

// ==================== Shutdown Options ====================

#define SHUT_RD 0    // Shutdown read side
#define SHUT_WR 1    // Shutdown write side
#define SHUT_RDWR 2  // Shutdown both read and write

// ==================== ioctl Commands ====================

#define FIONBIO 0x5421   // Set/clear non-blocking I/O
#define FIONREAD 0x541B  // Get number of bytes available to read

// Interface configuration
#define SIOCGIFADDR 0x8915   // Get interface address
#define SIOCSIFADDR 0x8916   // Set interface address
#define SIOCGIFFLAGS 0x8913  // Get interface flags
#define SIOCSIFFLAGS 0x8914  // Set interface flags
#define SIOCGIFMTU 0x8921    // Get MTU size

// ==================== Address Structures ====================

/**
 * @struct sockaddr
 * @brief Generic socket address structure
 *
 * This is the base structure that all protocol-specific
 * address structures can be cast to.
 */
struct sockaddr {
    uint16_t sa_family;  // Address family (AF_INET, AF_INET6, etc.)
    char sa_data[14];    // Protocol-specific address data
};

/**
 * @struct sockaddr_in
 * @brief IPv4 socket address structure
 *
 * Used for AF_INET sockets (IPv4).
 * All multi-byte fields are in network byte order (big-endian).
 */
struct sockaddr_in {
    uint16_t sin_family;  // Address family (always AF_INET for IPv4)
    uint16_t sin_port;    // Port number (network byte order)
    uint32_t sin_addr;    // IPv4 address (network byte order)
    uint8_t sin_zero[8];  // Padding to match sockaddr size (unused, should be zero)
};

/**
 * @typedef socklen_t
 * @brief Type for socket address length
 */
typedef uint32_t socklen_t;

// ==================== Byte Order Conversion Macros ====================

// Network byte order conversion - inline implementations for userland
// These work in both kernel and userland without dependencies
#ifdef __cplusplus
static inline uint16_t _byteswap16(uint16_t value) { return ((value & 0x00FF) << 8) | ((value & 0xFF00) >> 8); }

static inline uint32_t _byteswap32(uint32_t value) {
    return ((value & 0x000000FF) << 24) | ((value & 0x0000FF00) << 8) | ((value & 0x00FF0000) >> 8) | ((value & 0xFF000000) >> 24);
}

#define htons(x) (_byteswap16(x))
#define ntohs(x) (_byteswap16(x))
#define htonl(x) (_byteswap32(x))
#define ntohl(x) (_byteswap32(x))
#else
// C fallback
#define htons(x) ((uint16_t) (((x) << 8) | ((x) >> 8)))
#define ntohs(x) htons(x)
#define htonl(x) ((uint32_t) (((x) << 24) | (((x) & 0xFF00) << 8) | (((x) >> 8) & 0xFF00) | ((x) >> 24)))
#define ntohl(x) htonl(x)
#endif

#ifdef __cplusplus
}
#endif
