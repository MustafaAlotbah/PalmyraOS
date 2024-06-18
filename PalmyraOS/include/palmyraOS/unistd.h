/*
 * The API of PalmyraOS
 * Partially POSIX compliant (under construction)
 * */

#pragma once

#include <cstdint>


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