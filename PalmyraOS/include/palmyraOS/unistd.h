/*
 * The API of PalmyraOS
 * Partially POSIX compliant (under construction)
 * Check out https://github.com/spotify/linux/blob/master/arch/x86/include/asm/unistd_32.h
 * https://man7.org/linux/man-pages/man2/syscalls.2.html
 * https://github.com/torvalds/linux/blob/master/arch/x86/entry/syscalls/syscall_32.tbl
 * */

#pragma once

#include <cstdint>
#include "palmyraOS/input.h"

/* File Descriptors */
#define STDIN 0
#define STDOUT 1
#define STDERR 2


/* Specific Interrupts */
#define INT_INIT_WINDOW 9595
#define INT_CLOSE_WINDOW 9596
#define INT_NEXT_KEY_EVENT 9597


/* POSIX Interrupts */
#define POSIX_INT_EXIT 1
#define POSIX_INT_READ 3
#define POSIX_INT_WRITE 4
#define POSIX_INT_GET_PID 20
#define POSIX_INT_OPEN 5
#define POSIX_INT_CLOSE 6
#define POSIX_INT_IOCTL 54
#define POSIX_INT_MMAP 90
#define POSIX_INT_YIELD 158
#define POSIX_INT_GETTIME 228    // time.h (in linux, dependent on version)
#define POSIX_INT_CLOCK_NANOSLEEP_32 267    // NOT SUPPORTED
#define POSIX_INT_CLOCK_NANOSLEEP_64 407

/* From Linux */
#define LINUX_INT_GETDENTS 141


/* mmap protection flags */
#define PROT_READ  0x1
#define PROT_WRITE 0x2

/* mmap flags */
#define MAP_SHARED    0x01
#define MAP_PRIVATE   0x02
#define MAP_FIXED     0x10
#define MAP_ANONYMOUS 0x20

/* Error constant */
#define MAP_FAILED ((void*)-1)

/* RTC ioctl commands */
#define RTC_RD_TIME 0x80247009


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
uint32_t initializeWindow(uint32_t** buffer, uint32_t x, uint32_t y, uint32_t width, uint32_t height);

/**
 * @brief Closes the window identified by the given window ID.
 *
 * @param windowID The ID of the window to close.
 */
void closeWindow(uint32_t windowID);

KeyboardEvent nextKeyboardEvent(uint32_t windowID);

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

struct linux_dirent
{
	long           d_ino;
	size_t         d_off;
	unsigned short d_reclen;
	char           d_name[];
};

// dirent.h
# define DT_UNKNOWN    0
# define DT_FIFO    1
# define DT_CHR        2
# define DT_DIR        4
# define DT_BLK        6
# define DT_REG        8


int getdents(unsigned int fd, linux_dirent* dirp, unsigned int count);
