/**
 * @file udp_echo_server.cpp
 * @brief Simple UDP echo server for testing PalmyraOS networking
 *
 * Listens on a specified port and echoes back any received UDP datagrams.
 * This is a standard network testing tool (RFC 862 - Echo Protocol).
 *
 * Usage:
 *   exec /bin/udp_echo.elf [port]
 *
 * Default port: 7 (standard echo port)
 *
 * Test from host:
 *   echo "Hello" | nc -u 10.0.2.15 7
 */

#include "userland/tests/udp_echo_server.h"
#include "palmyraOS/errono.h"
#include "palmyraOS/network.h"
#include "palmyraOS/socket.h"
#include "palmyraOS/stdio.h"
#include "palmyraOS/stdlib.h"
#include "palmyraOS/unistd.h"

namespace PalmyraOS::Userland::tests::UDPEchoServer {

    int main(uint32_t argc, char** argv) {
        // Parse port from command line arguments
        uint16_t port = 7;  // Default: standard echo port (RFC 862)
        if (argc > 1) {
            port = (uint16_t) atoi(argv[1]);
            if (port == 0) {
                printf("Usage: %s [port]\n", argv[0]);
                printf("  port: UDP port to listen on (default: 7)\n");
                return -1;
            }
        }

        printf("====================================\n");
        printf("UDP Echo Server\n");
        printf("====================================\n");
        printf("Starting on port %u...\n", port);

        // Create UDP socket
        int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
        if (sockfd < 0) {
            printf("ERROR: Failed to create socket (code: %d)\n", sockfd);
            return -1;
        }
        printf("Socket created (fd=%d)\n", sockfd);

        // Enable address reuse (useful for quick restarts)
        int reuseaddr = 1;
        setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &reuseaddr, sizeof(reuseaddr));

        // Bind to port
        struct sockaddr_in addr;
        addr.sin_family = AF_INET;
        addr.sin_port   = htons(port);
        addr.sin_addr   = htonl(INADDR_ANY);  // 0.0.0.0 - listen on all interfaces

        int bindResult  = bind(sockfd, (const struct sockaddr*) &addr, sizeof(addr));
        if (bindResult < 0) {
            printf("ERROR: Failed to bind to port %u (code: %d)\n", port, bindResult);
            printf("  (Port may already be in use)\n");
            close(sockfd);
            return -1;
        }

        printf("Bound to 0.0.0.0:%u\n", port);
        printf("Ready to echo UDP datagrams!\n");
        printf("====================================\n");
        printf("\n");
        printf("Test from host:\n");
        printf("  echo \"Hello\" | nc -u 10.0.2.15 %u\n", port);
        printf("\n");
        printf("Waiting for packets...\n");
        printf("\n");

        // Echo loop
        char buffer[1500];  // Maximum UDP payload (MTU limit)
        struct sockaddr_in client;
        uint32_t clientLen;
        uint32_t packetCount = 0;

        while (1) {
            clientLen        = sizeof(client);

            // Receive datagram
            ssize_t received = recvfrom(sockfd, buffer, sizeof(buffer) - 1, 0, (struct sockaddr*) &client, &clientLen);

            if (received > 0) {
                packetCount++;

                // Null-terminate for safe printing (if it's text)
                buffer[received]    = '\0';

                // Extract client address
                uint32_t clientIP   = ntohl(client.sin_addr);
                uint16_t clientPort = ntohs(client.sin_port);

                // Log received data
                printf("[#%u] Received %d bytes from %u.%u.%u.%u:%u\n",
                       packetCount,
                       (int) received,
                       (clientIP >> 24) & 0xFF,
                       (clientIP >> 16) & 0xFF,
                       (clientIP >> 8) & 0xFF,
                       clientIP & 0xFF,
                       clientPort);

                // Print first 60 chars of data (if printable)
                printf("  Data: \"");
                int printLen = (received < 60) ? received : 60;
                for (int i = 0; i < printLen; ++i) {
                    char c = buffer[i];
                    if (c >= 32 && c <= 126) { printf("%c", c); }
                    else { printf("."); }
                }
                if (received > 60) { printf("... (%d more bytes)", (int) (received - 60)); }
                printf("\"\n");

                // Echo back to client
                ssize_t sent = sendto(sockfd, buffer, received, 0, (struct sockaddr*) &client, sizeof(client));

                if (sent == received) { printf("  Echoed %d bytes back\n", (int) sent); }
                else { printf("  ERROR: sendto() returned %d (expected %d)\n", (int) sent, (int) received); }

                printf("\n");
            }
            else if (received < 0 && received != -EAGAIN) {
                // Real error (not just "no data available")
                printf("ERROR: recvfrom() returned %d\n", (int) received);
            }

            // Yield CPU to other processes (be a good citizen)
            sched_yield();
        }

        // Cleanup (unreachable for now - would need signal handling)
        close(sockfd);
        printf("Server stopped.\n");
        return 0;
    }

}  // namespace PalmyraOS::Userland::tests::UDPEchoServer
