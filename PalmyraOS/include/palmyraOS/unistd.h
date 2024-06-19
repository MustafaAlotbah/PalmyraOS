/*
 * The API of PalmyraOS
 * Partially POSIX compliant (under construction)
 * */

#pragma once

#include <cstdint>

/* File Descriptors */
#define STDIN 0
#define STDOUT 1
#define STDERR 2


/* Specific Interrupts */
#define INT_INIT_WINDOW 9595
#define INT_CLOSE_WINDOW 9596


/* POSIX Interrupts */
#define POSIX_INT_EXIT 1
#define POSIX_INT_WRITE 4
#define POSIX_INT_GET_PID 20
#define POSIX_INT_MMAP 90
#define POSIX_INT_YIELD 158
#define POSIX_INT_GETTIME 228    // time.h (in linux, dependent on version)


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

/**
 * @brief Yields the processor, allowing other threads to run.
 *
 * This function is typically used in multi-threaded applications to allow other threads
 * a chance to execute.
 *
 * @return 0 on success, or -1 if an error occurred.
 */
int sched_yield();