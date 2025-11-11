
#pragma once

#include "core/tasks/Descriptor.h"
#include "core/files/VirtualFileSystemBase.h"

namespace PalmyraOS::kernel {

    /**
     * @class FileDescriptor
     * @brief Descriptor for regular files and directories
     * 
     * Represents an open file with an associated VFS inode, file offset, and open flags.
     * This is the concrete implementation of Descriptor for file I/O operations.
     * 
     * Features:
     * - Maintains current file offset (seekable)
     * - Delegates read/write/ioctl operations to the underlying VFS inode
     * - Supports directory operations (getdents via inode)
     * - Thread-safe at descriptor level (each process has its own offset)
     * 
     * Lifecycle:
     * - Created by open() syscall via DescriptorTable::allocate()
     * - Owned by DescriptorTable (heap-allocated)
     * - Destroyed when close() is called or process exits
     */
    class FileDescriptor final : public Descriptor {
    public:
        /**
         * @brief Construct a new file descriptor
         * @param inode Pointer to the VFS inode
         * @param flags Open flags (O_RDONLY, O_WRONLY, O_RDWR, etc.)
         */
        explicit FileDescriptor(vfs::InodeBase* inode, int flags);

        /**
         * @brief Destructor (default - no special cleanup needed)
         */
        ~FileDescriptor() override = default;

        // ===== Descriptor interface implementation =====

        /**
         * @brief Identify this as a File descriptor
         * @return Always returns Kind::File
         */
        [[nodiscard]] Kind kind() const override;

        /**
         * @brief Read data from the file at the current offset
         * @param buffer Destination buffer
         * @param size Number of bytes to read
         * @return Number of bytes actually read (may be less than requested)
         * 
         * Delegates to inode->read() and advances offset automatically.
         */
        size_t read(char* buffer, size_t size) override;

        /**
         * @brief Write data to the file at the current offset
         * @param buffer Source buffer
         * @param size Number of bytes to write
         * @return Number of bytes actually written
         * 
         * Delegates to inode->write() and advances offset automatically.
         */
        size_t write(const char* buffer, size_t size) override;

        /**
         * @brief Perform device-specific control operation
         * @param request Operation code
         * @param arg Operation-specific argument
         * @return Result from inode->ioctl()
         * 
         * Delegates to inode->ioctl() for device files.
         */
        int ioctl(int request, void* arg) override;

        // ===== File-specific accessors =====

        /**
         * @brief Get the underlying VFS inode
         * @return Pointer to the inode
         * 
         * Used by syscalls that need direct inode access:
         * - lseek() to get file size for SEEK_END
         * - getdents() to read directory entries
         * - Type checking for directory operations
         */
        [[nodiscard]] vfs::InodeBase* getInode() const;

        /**
         * @brief Get the open flags
         * @return Flags passed to open() (O_RDONLY, O_WRONLY, O_RDWR, etc.)
         */
        [[nodiscard]] int getFlags() const;

        /**
         * @brief Get the current file offset
         * @return Current read/write position in bytes
         * 
         * Used by lseek() for SEEK_CUR calculations.
         */
        [[nodiscard]] size_t getOffset() const;

        /**
         * @brief Set the file offset to a specific position
         * @param offset New offset in bytes
         * 
         * Used by lseek() syscall to reposition the file pointer.
         */
        void setOffset(size_t offset);

        /**
         * @brief Advance the file offset by a number of bytes
         * @param bytes Number of bytes to advance
         * 
         * Called automatically after read/write operations.
         * Also used by getdents() to advance directory position.
         */
        void advanceOffset(size_t bytes);

    private:
        vfs::InodeBase* inode_;  ///< Underlying VFS inode (file/directory)
        size_t offset_;          ///< Current read/write position
        int flags_;              ///< Open flags (O_RDONLY, O_WRONLY, O_RDWR, etc.)
    };

}  // namespace PalmyraOS::kernel

