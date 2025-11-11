

#include "core/tasks/FileDescriptor.h"
#include "palmyraOS/errono.h"  // For EBADF

namespace PalmyraOS::kernel {

    // ===== Constructor =====

    FileDescriptor::FileDescriptor(vfs::InodeBase* inode, int flags) : inode_(inode), offset_(0), flags_(flags) {}

    // ===== Descriptor interface implementation =====

    Descriptor::Kind FileDescriptor::kind() const { return Kind::File; }

    size_t FileDescriptor::read(char* buffer, size_t size) {
        if (!inode_) return 0;

        // Delegate to VFS inode
        size_t bytesRead = inode_->read(buffer, size, offset_);

        // Advance offset automatically
        offset_ += bytesRead;

        return bytesRead;
    }

    size_t FileDescriptor::write(const char* buffer, size_t size) {
        if (!inode_) return 0;

        // Delegate to VFS inode
        size_t bytesWritten = inode_->write(buffer, size, offset_);

        // Advance offset automatically
        offset_ += bytesWritten;

        return bytesWritten;
    }

    int FileDescriptor::ioctl(int request, void* arg) {
        if (!inode_) return -EBADF;

        // Delegate to VFS inode
        return inode_->ioctl(request, arg);
    }

    // ===== File-specific accessors =====

    vfs::InodeBase* FileDescriptor::getInode() const { return inode_; }

    int FileDescriptor::getFlags() const { return flags_; }

    size_t FileDescriptor::getOffset() const { return offset_; }

    void FileDescriptor::setOffset(size_t offset) { offset_ = offset; }

    void FileDescriptor::advanceOffset(size_t bytes) { offset_ += bytes; }

}  // namespace PalmyraOS::kernel
