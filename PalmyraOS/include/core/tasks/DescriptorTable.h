
#pragma once

#include "core/tasks/Descriptor.h"
#include "core/memory/KernelHeapAllocator.h"  // KMap
#include "palmyraOS/unistd.h"  // fd_t

namespace PalmyraOS::kernel {

    /**
     * @class DescriptorTable
     * @brief Per-process table mapping file descriptors to Descriptor objects
     * 
     * Manages the lifetime of all descriptors (files, pipes, sockets) for a process.
     * Each process has its own descriptor table with a flat integer namespace.
     * 
     * Descriptor numbering:
     * - 0: stdin  (standard input)
     * - 1: stdout (standard output)
     * - 2: stderr (standard error)
     * - 3+: dynamically allocated
     * 
     * Memory management:
     * - Descriptors are heap-allocated and owned by this table
     * - release() and destructor handle cleanup automatically
     * 
     * Thread-safety:
     * - Not thread-safe (process-local, no concurrent access expected)
     */
    class DescriptorTable {
    public:
        /**
         * @brief Construct an empty descriptor table
         * 
         * Initializes nextFd_ to 3, reserving 0, 1, 2 for standard streams.
         * Note: Standard streams are not automatically allocated - the caller
         * must allocate them explicitly if needed.
         */
        DescriptorTable();

        /**
         * @brief Destroy the descriptor table and free all descriptors
         * 
         * Calls delete on all remaining descriptors to prevent leaks.
         */
        ~DescriptorTable();

        /**
         * @brief Allocate a new file descriptor for a descriptor object
         * @param descriptor Heap-allocated descriptor (ownership transferred)
         * @return The allocated file descriptor number
         * 
         * The table takes ownership of the descriptor pointer.
         * The descriptor will be deleted when release() is called or the table is destroyed.
         * 
         * Example:
         *   auto* fileDesc = heapManager.createInstance<FileDescriptor>(inode, flags);
         *   fd_t fd = table.allocate(fileDesc);
         */
        fd_t allocate(Descriptor* descriptor);

        /**
         * @brief Release a file descriptor and free its descriptor
         * @param fd The file descriptor to release
         * 
         * Deletes the descriptor object and removes it from the table.
         * If fd is not in the table, does nothing (safe to call multiple times).
         * 
         * Corresponds to close() syscall.
         */
        void release(fd_t fd);

        /**
         * @brief Get the descriptor associated with a file descriptor
         * @param fd The file descriptor
         * @return Pointer to the descriptor, or nullptr if not found
         * 
         * Used by all I/O syscalls (read, write, lseek, ioctl, etc.).
         * The returned pointer is valid until release() is called on this fd.
         */
        [[nodiscard]] Descriptor* get(fd_t fd) const;

        /**
         * @brief Check if a file descriptor is allocated
         * @param fd The file descriptor to check
         * @return true if fd is in the table, false otherwise
         */
        [[nodiscard]] bool contains(fd_t fd) const;

        /**
         * @brief Get the number of open descriptors
         * @return Count of descriptors in the table
         */
        [[nodiscard]] size_t count() const;

    private:
        KMap<fd_t, Descriptor*> table_;  ///< Map of file descriptors to descriptors
        fd_t nextFd_;                    ///< Next file descriptor to allocate
    };

}  // namespace PalmyraOS::kernel

