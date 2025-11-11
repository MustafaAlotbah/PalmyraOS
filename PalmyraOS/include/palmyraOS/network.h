#pragma once

#include <cstdint>
#include "palmyraOS/socket.h"

/**
 * @file network.h
 * @brief Userspace network helper functions for PalmyraOS
 *
 * This file provides high-level networking utilities that applications
 * can use without directly interacting with low-level socket APIs.
 *
 * Functions:
 * - gethostbyname() - Resolve hostname to IP address (DNS)
 * - ping() - Send ICMP echo request and measure RTT
 * - inet_addr() - Convert dotted decimal string to IP address
 * - inet_ntoa() - Convert IP address to dotted decimal string
 */

#ifdef __cplusplus
extern "C" {
#endif

// ==================== DNS Resolution ====================

/**
 * @brief Resolve hostname to IPv4 address (DNS lookup)
 *
 * Sends a DNS query to the configured DNS server (8.8.8.8 by default)
 * using UDP sockets. This is a userspace implementation that doesn't
 * require kernel DNS support.
 *
 * **Process:**
 * 1. Create UDP socket
 * 2. Build DNS query packet
 * 3. Send to DNS server (8.8.8.8:53)
 * 4. Wait for DNS response
 * 5. Parse response and extract IP address
 * 6. Close socket
 *
 * @param hostname Domain name to resolve (e.g., "google.com")
 * @param outIP Pointer to uint32_t for result IP (host byte order)
 * @param timeout_ms Timeout in milliseconds (0 = default 5000ms)
 * @return 0 on success, negative error code on failure
 *
 * Example:
 *   uint32_t ip;
 *   if (gethostbyname("google.com", &ip, 0) == 0) {
 *       printf("google.com = %u.%u.%u.%u\n",
 *              (ip >> 24) & 0xFF, (ip >> 16) & 0xFF,
 *              (ip >> 8) & 0xFF, ip & 0xFF);
 *   }
 */
int gethostbyname(const char* hostname, uint32_t* outIP, uint32_t timeout_ms);

/**
 * @brief Resolve hostname with custom DNS server
 *
 * Same as gethostbyname() but allows specifying a custom DNS server.
 *
 * @param hostname Domain name to resolve
 * @param outIP Pointer to uint32_t for result IP
 * @param dnsServer DNS server IP address (host byte order)
 * @param timeout_ms Timeout in milliseconds
 * @return 0 on success, negative error code on failure
 */
int gethostbyname_dns(const char* hostname, uint32_t* outIP, uint32_t dnsServer, uint32_t timeout_ms);

// ==================== ICMP Ping ====================

/**
 * @brief Ping a host (send ICMP echo request)
 *
 * Sends an ICMP Echo Request to the target IP and waits for a reply.
 * Measures the round-trip time (RTT).
 *
 * **Note:** Currently uses kernel ICMP via syscall. Future versions
 * will use raw sockets (SOCK_RAW, IPPROTO_ICMP) for pure userspace.
 *
 * @param targetIP Target IP address (host byte order)
 * @param outRTTms Pointer to uint32_t for RTT result (milliseconds)
 * @param timeout_ms Timeout in milliseconds (0 = default 5000ms)
 * @return 0 on success (reply received), negative error code on failure
 *
 * Example:
 *   uint32_t rtt;
 *   if (ping(0x08080808, &rtt, 0) == 0) {  // 8.8.8.8
 *       printf("Ping successful! RTT: %u ms\n", rtt);
 *   } else {
 *       printf("Ping failed or timed out\n");
 *   }
 */
int ping(uint32_t targetIP, uint32_t* outRTTms, uint32_t timeout_ms);

/**
 * @brief Ping a hostname (resolve + ping)
 *
 * Convenience function that combines DNS resolution and ping.
 *
 * @param hostname Hostname or IP string (e.g., "google.com" or "8.8.8.8")
 * @param outRTTms Pointer to uint32_t for RTT result
 * @param timeout_ms Timeout in milliseconds
 * @return 0 on success, negative error code on failure
 */
int ping_host(const char* hostname, uint32_t* outRTTms, uint32_t timeout_ms);

// ==================== IP Address Utilities ====================

/**
 * @brief Convert dotted-decimal string to IP address
 *
 * Parses strings like "192.168.1.1" into a 32-bit IP address.
 *
 * @param ipString IP address string (e.g., "8.8.8.8")
 * @return IP address in host byte order, or 0 on parse error
 *
 * Example:
 *   uint32_t ip = inet_addr("192.168.1.1");  // 0xC0A80101
 */
uint32_t inet_addr(const char* ipString);

/**
 * @brief Convert IP address to dotted-decimal string
 *
 * Converts 32-bit IP to string like "192.168.1.1".
 * Writes result to static buffer (not thread-safe).
 *
 * @param ip IP address (host byte order)
 * @return Pointer to static buffer containing IP string
 *
 * Example:
 *   const char* str = inet_ntoa(0x08080808);  // "8.8.8.8"
 */
const char* inet_ntoa(uint32_t ip);

/**
 * @brief Thread-safe version of inet_ntoa
 *
 * @param ip IP address (host byte order)
 * @param buffer Output buffer (must be at least 16 bytes)
 * @param bufferSize Size of output buffer
 * @return Pointer to buffer on success, nullptr on error
 */
const char* inet_ntoa_r(uint32_t ip, char* buffer, uint32_t bufferSize);

// ==================== DNS Constants ====================

/// @brief Google Public DNS (8.8.8.8)
#define DNS_SERVER_GOOGLE_PRIMARY 0x08080808

/// @brief Google Public DNS (8.8.4.4)
#define DNS_SERVER_GOOGLE_SECONDARY 0x08080404

/// @brief Cloudflare DNS (1.1.1.1)
#define DNS_SERVER_CLOUDFLARE_PRIMARY 0x01010101

/// @brief Cloudflare DNS (1.0.0.1)
#define DNS_SERVER_CLOUDFLARE_SECONDARY 0x01000001

/// @brief Default DNS server
#define DNS_SERVER_DEFAULT DNS_SERVER_GOOGLE_PRIMARY

/// @brief DNS server port
#define DNS_PORT 53

/// @brief Default timeout (5 seconds)
#define DEFAULT_NETWORK_TIMEOUT_MS 5000

#ifdef __cplusplus
}
#endif

