/*
 * The API of PalmyraOS
 * Partially POSIX compliant (under construction)
 * Check out
 * - https://github.com/spotify/linux/blob/master/arch/x86/include/asm/unistd_32.h
 * - https://man7.org/linux/man-pages/man2/syscalls.2.html
 * - https://github.com/torvalds/linux/blob/master/arch/x86/entry/syscalls/syscall_32.tbl
 * */

#pragma once

#include "palmyraOS/input.h"
#include <cstdint>

/* File Descriptors */
#define STDIN 0
#define STDOUT 1
#define STDERR 2

/* Type definitions */
typedef int32_t ssize_t;  // Signed size type for read/write return values


/* Specific Interrupts */

// 95XX Windows
#define INT_INIT_WINDOW 9500
#define INT_CLOSE_WINDOW 9501
#define INT_NEXT_KEY_EVENT 9502
#define INT_NEXT_MOUSE_EVENT 9503
#define INT_GET_WINDOW_STATUS 9504

// 96XX Processes
#define POSIX_INT_POSIX_SPAWN 9600  // posix_spawn (in linux, it's not its own syscall)


/* POSIX Interrupts */
#define POSIX_INT_EXIT 1
// FORK (syscall 2) is not supported yet
#define POSIX_INT_READ 3
#define POSIX_INT_WRITE 4
#define POSIX_INT_OPEN 5
#define POSIX_INT_CLOSE 6
#define POSIX_INT_WAITPID 7
#define POSIX_INT_UNLINK 10
#define POSIX_INT_LSEEK 19
#define POSIX_INT_GET_PID 20
#define POSIX_INT_MKDIR 39
#define POSIX_INT_RMDIR 40
#define POSIX_INT_BRK 45
#define POSIX_INT_IOCTL 54
#define POSIX_INT_REBOOT 88  // Linux compatible reboot syscall
#define POSIX_INT_MMAP 90
#define POSIX_INT_YIELD 158
#define POSIX_INT_GETUID 199
#define POSIX_INT_GETGID 200
#define POSIX_INT_GETEUID32 201
#define POSIX_INT_GETEGID32 202

#define POSIX_INT_GETTIME 228  // time.h (in linux, dependent on version)
#define POSIX_INT_SETTHREADAREA 243
#define POSIX_INT_CLOCK_NANOSLEEP_32 267  // NOT SUPPORTED
#define POSIX_INT_CLOCK_NANOSLEEP_64 407

/* From Linux */
#define LINUX_INT_GETDENTS 141
#define LINUX_INT_PRCTL 384

/* Socket syscalls (Linux x86-32 compatible) */
#define POSIX_INT_SOCKET 359
#define POSIX_INT_BIND 361
#define POSIX_INT_CONNECT 362
#define POSIX_INT_LISTEN 363
#define POSIX_INT_ACCEPT 364
#define POSIX_INT_GETSOCKOPT 365
#define POSIX_INT_SETSOCKOPT 366
#define POSIX_INT_GETSOCKNAME 367
#define POSIX_INT_GETPEERNAME 368
#define POSIX_INT_SENDTO 369
#define POSIX_INT_RECVFROM 371
#define POSIX_INT_SHUTDOWN 373


/* Linux reboot() magic numbers and commands */
#define LINUX_REBOOT_MAGIC1 0xfee1dead
#define LINUX_REBOOT_MAGIC2 0x07011995   // Mustafa Alotbah' birthday
#define LINUX_REBOOT_MAGIC2A 0x05121996  // Alternative magic
#define LINUX_REBOOT_MAGIC2B 0x16041998  // Alternative magic
#define LINUX_REBOOT_MAGIC2C 0x20112000  // Alternative magic

/* Linux reboot commands */
#define LINUX_REBOOT_CMD_RESTART 0x01234567    // Restart system
#define LINUX_REBOOT_CMD_HALT 0xCDEF0123       // Halt system (stop CPU)
#define LINUX_REBOOT_CMD_POWER_OFF 0x4321FEDC  // Power off system
#define LINUX_REBOOT_CMD_RESTART2 0xA1B2C3D4   // Restart with command string
#define LINUX_REBOOT_CMD_CAD_ON 0x89ABCDEF     // Enable Ctrl-Alt-Del
#define LINUX_REBOOT_CMD_CAD_OFF 0x00000000    // Disable Ctrl-Alt-Del


/* mmap protection flags */
#define PROT_READ 0x1
#define PROT_WRITE 0x2

/* mmap flags */
#define MAP_SHARED 0x01
#define MAP_PRIVATE 0x02
#define MAP_FIXED 0x10
#define MAP_ANONYMOUS 0x20

/* Error constant */
#define MAP_FAILED ((void*) -1)

/* RTC ioctl commands */
#define RTC_RD_TIME 0x80247009

/* Constants for whence argument in lseek */
#define SEEK_SET 0  // Seek from the beginning of the file
#define SEEK_CUR 1  // Seek from the current file offset
#define SEEK_END 2  // Seek from the end of the file

/* File open() flags - POSIX standard */
/* Access modes: Only one of O_RDONLY, O_WRONLY, or O_RDWR should be used */
#define O_RDONLY 0x00  // Open file for reading only (default, value 0) (TODO)
#define O_WRONLY 0x01  // Open file for writing only (TODO)
#define O_RDWR 0x02    // Open file for reading and writing (TODO)

/* File creation and truncation flags */
#define O_CREAT 0x40   // Create file if it doesn't exist. Requires mode parameter in open() call.
#define O_EXCL 0x80    // When used with O_CREAT, fail if file already exists (atomic creation) (TODO)
#define O_TRUNC 0x200  // If file exists and is opened for writing, truncate (erase) it to zero bytes

/* File positioning flags */
#define O_APPEND 0x400  // All write operations append to the end of the file, regardless of lseek() calls (TODO)

/* Constants for Arch Prctl */
// https://github.com/torvalds/linux/blob/master/arch/x86/include/uapi/asm/prctl.h
#define ARCH_SET_GS 0x1001
#define ARCH_SET_FS 0x1002  // Set FS segment base
#define ARCH_GET_FS 0x1003  // Get FS segment base
#define ARCH_GET_GS 0x1004

typedef uint32_t fd_t;


/**
 * @brief Retrieves the process identifier (PID) of the calling process.
 *
 * @return The PID of the calling process.
 */
uint32_t get_pid();

/**
 * @brief Terminates the calling process immediately with the specified exit code.
 *
 * This function does not perform any cleanup operations, such as flushing stdio buffers
 * or calling functions registered with atexit().
 *
 * @param exitCode The exit code to be returned to the operating system.
 */
void _exit(uint32_t exitCode);

/**
 * @brief Writes data to the specified file descriptor.
 *
 * @param fileDescriptor The file descriptor to which data will be written.
 * @param buffer A pointer to the buffer containing the data to be written.
 * @param count The number of bytes to write from the buffer.
 * @return The number of bytes written, or -1 if an error occurred.
 */
int write(uint32_t fileDescriptor, const void* buffer, uint32_t count);

/**
 * @brief Reads data from the specified file descriptor.
 *
 * @param fileDescriptor The file descriptor from which data will be read.
 * @param buffer A pointer to the buffer where the read data will be stored.
 * @param count The number of bytes to read into the buffer.
 * @return The number of bytes read, or -1 if an error occurred.
 */
int read(uint32_t fileDescriptor, void* buffer, uint32_t count);

/**
 * @brief Repositions the offset of the file descriptor.
 *
 * @param fd The file descriptor whose offset is to be changed.
 * @param offset The new offset to be set.
 * @param whence The directive that specifies how the offset should be interpreted:
 *               - SEEK_SET: Set the offset to the value specified by offset.
 *               - SEEK_CUR: Set the offset to the current location plus offset.
 *               - SEEK_END: Set the offset to the size of the file plus offset.
 * @return The resulting offset location as measured in bytes from the beginning of the file,
 *         or -1 if an error occurred.
 */
int32_t lseek(fd_t fd, int32_t offset, int whence);

/**
 * @brief Maps files or devices into memory.
 *
 * @param addr The starting address for the new mapping. If NULL, the kernel chooses the address.
 * @param length The length of the mapping.
 * @param prot The desired memory protection of the mapping.
 * @param flags Determines the visibility of the changes to the mapped area.
 * @param fd The file descriptor of the file to map.
 * @param offset The offset in the file where the mapping starts.
 * @return On success, returns a pointer to the mapped area. On failure, returns MAP_FAILED.
 */
void* mmap(void* addr, uint32_t length, int prot, int flags, int fd, uint32_t offset);

// PalmyraOS specific, returns id of the window
struct palmyra_window {
    uint32_t x;
    uint32_t y;
    uint32_t width;
    uint32_t height;
    bool movable;
    char title[50];
};

struct palmyra_window_status {
    uint32_t x      = 0;
    uint32_t y      = 0;
    uint32_t width  = 0;
    uint32_t height = 0;
    bool isActive   = false;
};

uint32_t initializeWindow(uint32_t** buffer, palmyra_window* palmyraWindow);

/**
 * @brief Closes the window identified by the given window ID.
 *
 * @param windowID The ID of the window to close.
 */
void closeWindow(uint32_t windowID);

KeyboardEvent nextKeyboardEvent(uint32_t windowID);

MouseEvent nextMouseEvent(uint32_t windowID);

palmyra_window_status getStatus(uint32_t windowID);


/**
 * @brief Yields the processor, allowing other threads to run.
 *
 * This function is typically used in multi-threaded applications to allow other threads
 * a chance to execute.
 *
 * @return 0 on success, or -1 if an error occurred.
 */
int sched_yield();

/**
 * @brief Opens a file or device.
 *
 * @param pathname The pathname of the file or device to open.
 * @param flags The flags to control how the file or device is opened.
 * @return The file descriptor on success, or -1 if an error occurred.
 */
int open(const char* pathname, int flags);

/**
 * @brief Closes a file descriptor.
 *
 * @param fd The file descriptor to close.
 * @return 0 on success, or -1 if an error occurred.
 */
int close(uint32_t fd);

/**
 * @brief Performs device-specific operations.
 *
 * @param fd The file descriptor referring to the device.
 * @param request The device-specific request code.
 * @param ... Additional arguments depending on the request code.
 * @return 0 on success, or -1 if an error occurred.
 */
int ioctl(uint32_t fd, uint32_t request, ...);

struct linux_dirent {
    long d_ino;
    size_t d_off;
    unsigned short d_reclen;
    char d_name[];
};

// dirent.h
#define DT_UNKNOWN 0
#define DT_FIFO 1
#define DT_CHR 2
#define DT_DIR 4
#define DT_BLK 6
#define DT_REG 8  // file


int getdents(unsigned int fd, linux_dirent* dirp, unsigned int count);

/**
 * @brief Starts a new process by spawning the specified ELF file.
 *
 * This function is a simplified variant of the POSIX `posix_spawn` function.
 * It spawns a new process to execute the specified ELF file. The `file_actions`
 * and `attrp` parameters are provided for compatibility with the POSIX standard,
 * but they are not used or implemented in this version.
 *
 * @param pid A pointer to a variable where the process ID of the new process will be stored.
 *            This will be of type `uint32_t` rather than the typical `pid_t` used in POSIX.
 * @param path The path to the ELF file to be executed.
 * @param file_actions Reserved for compatibility, but not used or implemented in this function.
 * @param attrp Reserved for compatibility, but not used or implemented in this function.
 * @param argv A null-terminated array of argument strings passed to the new program.
 * @param envp A null-terminated array of environment variables passed to the new program.
 * @return 0 on success, or a negative error code on failure.
 */
int posix_spawn(uint32_t* pid, const char* path, void* file_actions, void* attrp, char* const argv[], char* const envp[]);

/**
 * @brief Waits for a specific process to change state.
 *
 * @param pid The process ID of the child process to wait for.
 * @param status A pointer to an integer where the exit status of the child process will be stored.
 * @param options Options for controlling the behavior of the wait.
 * @return The process ID of the child that changed state, or -1 if an error occurred.
 */
uint32_t waitpid(uint32_t pid, int* status, int options);

int arch_prctl(int code, unsigned long addr);

int brk(void* end_data_segment);

int set_thread_area(struct user_desc* u_info);

uint32_t getuid();
uint32_t getgid();
uint32_t geteuid32();
uint32_t getegid32();

/**
 * @brief Creates a new directory.
 *
 * @param pathname The pathname of the directory to create.
 * @param mode The mode of the directory to create.
 * @return 0 on success, or -1 if an error occurred.
 */
int mkdir(const char* pathname, unsigned short mode);

/**
 * @brief Unlinks a file.
 *
 * @param pathname The pathname of the file to unlink.
 * @return 0 on success, or -1 if an error occurred.
 */
int unlink(const char* pathname);

/**
 * @brief Removes an empty directory.
 *
 * @param pathname The pathname of the directory to remove.
 * @return 0 on success, or -1 if an error occurred.
 */
int rmdir(const char* pathname);

/**
 * @brief Reboot or power off the system (Linux compatible)
 *
 * This is the Linux-compatible reboot syscall. It requires two magic numbers
 * for safety and a command specifying the action.
 *
 * @param magic Must be LINUX_REBOOT_MAGIC1 (0xfee1dead)
 * @param magic2 Must be one of LINUX_REBOOT_MAGIC2* values
 * @param cmd Command to execute (LINUX_REBOOT_CMD_*)
 * @param arg Optional argument for RESTART2 command
 * @return Does not return on success, -1 on error
 *
 * Commands:
 * - LINUX_REBOOT_CMD_RESTART: Restart the system
 * - LINUX_REBOOT_CMD_POWER_OFF: Power off the system
 * - LINUX_REBOOT_CMD_HALT: Halt the system (stop CPU)
 */
int reboot(int magic, int magic2, int cmd, void* arg);

// ==================== Socket API (POSIX-compatible) ====================

// Forward declarations
struct sockaddr;

/**
 * @brief Creates an endpoint for communication and returns a file descriptor.
 *
 * Creates a socket of the specified type and protocol within the specified domain.
 * The socket is used for network communication and can be used with read(), write(),
 * and socket-specific functions like sendto(), recvfrom(), etc.
 *
 * @param domain Communication domain (AF_INET for IPv4)
 * @param type Socket type:
 *             - SOCK_STREAM: TCP (connection-oriented, reliable byte stream)
 *             - SOCK_DGRAM:  UDP (connectionless, unreliable datagrams)
 *             - SOCK_RAW:    Raw IP (for ICMP ping, requires IPPROTO_ICMP)
 * @param protocol Protocol to use:
 *                 - 0: Auto-select based on type (TCP for STREAM, UDP for DGRAM)
 *                 - IPPROTO_ICMP (1): For raw ICMP sockets (ping, traceroute)
 *                 - IPPROTO_TCP (6): Transmission Control Protocol
 *                 - IPPROTO_UDP (17): User Datagram Protocol
 * @return Socket file descriptor on success, or negative error code on failure
 */
int socket(int domain, int type, int protocol);

/**
 * @brief Binds a socket to a local address and port.
 *
 * Assigns a local address (IP and port) to the socket. For server sockets, this
 * specifies the address on which to listen for incoming connections or datagrams.
 * For raw sockets (SOCK_RAW), the port field is ignored.
 *
 * @param sockfd Socket file descriptor
 * @param addr Pointer to sockaddr_in structure containing local address and port
 * @param addrlen Size of the addr structure (typically sizeof(sockaddr_in))
 * @return 0 on success, or negative error code on failure (e.g., -EADDRINUSE if port in use)
 */
int bind(int sockfd, const struct sockaddr* addr, uint32_t addrlen);

/**
 * @brief Connects a socket to a remote address.
 *
 * For TCP (SOCK_STREAM): Initiates a connection to the remote host.
 * For UDP (SOCK_DGRAM): Sets the default destination address for send()/write().
 * For raw sockets (SOCK_RAW): Filters received packets to only those from the remote address.
 *
 * @param sockfd Socket file descriptor
 * @param addr Pointer to sockaddr_in structure containing remote address and port
 * @param addrlen Size of the addr structure
 * @return 0 on success, or negative error code on failure
 */
int connect(int sockfd, const struct sockaddr* addr, uint32_t addrlen);

/**
 * @brief Sends a message to a specific destination address.
 *
 * Sends data to the specified destination address. For connection-oriented sockets
 * (TCP), the destination address is ignored and the connected peer is used.
 * For raw sockets, the port field in dest_addr is ignored (e.g., ICMP has no ports).
 *
 * @param sockfd Socket file descriptor
 * @param buf Pointer to data buffer to send
 * @param len Length of data in bytes
 * @param flags Message flags (MSG_DONTWAIT for non-blocking, etc.)
 * @param dest_addr Destination address (sockaddr_in structure)
 * @param addrlen Size of dest_addr structure
 * @return Number of bytes sent on success, or negative error code on failure
 */
ssize_t sendto(int sockfd, const void* buf, size_t len, int flags, const struct sockaddr* dest_addr, uint32_t addrlen);

/**
 * @brief Receives a message from a socket, storing the source address.
 *
 * Receives data from the socket and optionally stores the source address.
 * For connection-oriented sockets (TCP), src_addr is ignored.
 * For raw sockets, the port field in src_addr is always 0 (e.g., ICMP has no ports).
 *
 * @param sockfd Socket file descriptor
 * @param buf Pointer to buffer to store received data
 * @param len Maximum number of bytes to receive
 * @param flags Message flags (MSG_PEEK to peek without removing, etc.)
 * @param src_addr Output: Source address (can be NULL if not needed)
 * @param addrlen Input/Output: Size of src_addr buffer, updated with actual size
 * @return Number of bytes received on success, 0 if no data available (non-blocking),
 *         or negative error code on failure
 */
ssize_t recvfrom(int sockfd, void* buf, size_t len, int flags, struct sockaddr* src_addr, uint32_t* addrlen);

/**
 * @brief Sets options on a socket.
 *
 * Modifies socket behavior by setting various options. Common options include
 * SO_REUSEADDR (allow address reuse), SO_BROADCAST (enable broadcast), etc.
 *
 * @param sockfd Socket file descriptor
 * @param level Option level (SOL_SOCKET for socket-level options)
 * @param optname Option name (SO_REUSEADDR, SO_BROADCAST, etc.)
 * @param optval Pointer to option value
 * @param optlen Size of option value in bytes
 * @return 0 on success, or negative error code on failure
 */
int setsockopt(int sockfd, int level, int optname, const void* optval, uint32_t optlen);

/**
 * @brief Gets the current value of a socket option.
 *
 * Retrieves the current value of the specified socket option.
 *
 * @param sockfd Socket file descriptor
 * @param level Option level (SOL_SOCKET for socket-level options)
 * @param optname Option name (SO_TYPE, SO_ERROR, etc.)
 * @param optval Output: Pointer to buffer to store option value
 * @param optlen Input/Output: Pointer to size of optval buffer, updated with actual size
 * @return 0 on success, or negative error code on failure
 */
int getsockopt(int sockfd, int level, int optname, void* optval, uint32_t* optlen);

/**
 * @brief Gets the local address to which a socket is bound.
 *
 * Retrieves the current local address (IP and port) bound to the socket.
 * Useful for determining which port was assigned after binding to port 0 (auto-assign).
 *
 * @param sockfd Socket file descriptor
 * @param addr Output: Pointer to sockaddr structure to store local address
 * @param addrlen Input/Output: Pointer to size of addr buffer, updated with actual size
 * @return 0 on success, or negative error code on failure
 */
int getsockname(int sockfd, struct sockaddr* addr, uint32_t* addrlen);

/**
 * @brief Gets the address of the peer connected to the socket.
 *
 * Retrieves the remote address of the peer connected to this socket.
 * Only valid for connected sockets (after connect() or accept()).
 *
 * @param sockfd Socket file descriptor
 * @param addr Output: Pointer to sockaddr structure to store remote address
 * @param addrlen Input/Output: Pointer to size of addr buffer, updated with actual size
 * @return 0 on success, or -ENOTCONN if socket is not connected, or other negative error code
 */
int getpeername(int sockfd, struct sockaddr* addr, uint32_t* addrlen);

/**
 * @brief Marks a socket as passive, ready to accept incoming connections.
 *
 * Converts an active socket to a passive socket that will accept incoming connection
 * requests using accept(). Only valid for connection-oriented sockets (SOCK_STREAM/TCP).
 *
 * @param sockfd Socket file descriptor
 * @param backlog Maximum length of the queue of pending connections
 * @return 0 on success, or negative error code on failure
 */
int listen(int sockfd, int backlog);

/**
 * @brief Accepts an incoming connection on a listening socket.
 *
 * Extracts the first connection request from the queue of pending connections,
 * creates a new connected socket, and returns a new file descriptor for that socket.
 * The original listening socket remains open to accept further connections.
 * Only valid for connection-oriented sockets (SOCK_STREAM/TCP).
 *
 * @param sockfd Listening socket file descriptor
 * @param addr Output: Pointer to sockaddr structure to store peer address (can be NULL)
 * @param addrlen Input/Output: Pointer to size of addr buffer, updated with actual size (can be NULL)
 * @return New socket file descriptor for the accepted connection on success,
 *         or negative error code on failure
 */
int accept(int sockfd, struct sockaddr* addr, uint32_t* addrlen);

/**
 * @brief Shuts down part or all of a full-duplex connection.
 *
 * Disables further send and/or receive operations on a socket.
 * Only valid for connection-oriented sockets (SOCK_STREAM/TCP).
 *
 * @param sockfd Socket file descriptor
 * @param how Shutdown mode:
 *            - SHUT_RD (0): Disable further receive operations
 *            - SHUT_WR (1): Disable further send operations
 *            - SHUT_RDWR (2): Disable both send and receive operations
 * @return 0 on success, or negative error code on failure
 */
int shutdown(int sockfd, int how);
