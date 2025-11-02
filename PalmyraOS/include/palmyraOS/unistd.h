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
#define POSIX_INT_BRK 45
#define POSIX_INT_IOCTL 54
#define POSIX_INT_MKDIR 39
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
