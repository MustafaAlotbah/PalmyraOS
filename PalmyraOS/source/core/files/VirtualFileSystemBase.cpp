
#include "core/files/VirtualFileSystemBase.h"
#include <algorithm>  // std::find

#include "core/peripherals/Logger.h"

namespace PalmyraOS::kernel::vfs {

    /// region FileSystemType

    FileSystemType::FileSystemType(const KString& name, int flags) : name_(name), flags_(flags) {}
    const KString& FileSystemType::getName() const {
        // Return the name of the file system type.
        return name_;
    }
    int FileSystemType::getFlags() const {
        // Return the flags associated with the file system type.
        return flags_;
    }
    void FileSystemType::addSuperBlock(SuperBlockBase* superBlock) {
        // Add the given superBlock to the vector of superBlocks_.
        superBlocks_.push_back(superBlock);
    }

    /// endregion

    /// region SuperBlockBase

    SuperBlockBase::SuperBlockBase(size_t blockSize, FileSystemType* fileSystemType) : blockSize_(blockSize), fileSystemType_(fileSystemType) {}

    size_t SuperBlockBase::getBlockSize() const {
        // Return the block size of the file system.
        return blockSize_;
    }

    FileSystemType* SuperBlockBase::getFileSystemType() const {
        // Return the pointer to the associated file system type.
        return fileSystemType_;
    }

    void SuperBlockBase::addInode(InodeBase* inode) {
        // Add the given inode to the vector of inodes_.
        inodes_.push_back(inode);
    }

    // Remove an Inode from the superblock
    bool SuperBlockBase::removeInode(InodeBase* inode) {
        // Find the inode in the vector of inodes_.
        auto it = std::find(inodes_.begin(), inodes_.end(), inode);

        // If found, erase it from the vector.
        if (it != inodes_.end()) {
            inodes_.erase(it);
            return true;
        }

        // If not found, return false.
        return false;
    }
    InodeBase* SuperBlockBase::allocateInode(InodeBase::Type type, InodeBase::Mode mode, InodeBase::UserID userId, InodeBase::GroupID groupId) {
        // Create a new inode instance with specified type and default permissions
        auto inode = kernel::heapManager.createInstance<InodeBase>(type, mode, userId, groupId);

        // Ensure the allocation succeeded
        if (inode == nullptr) return nullptr;

        // Associate the new inode with this superblock
        inode->setSuperBlock(this);

        // Add the new inode to the superb-lock's list of inodes
        addInode(inode);
        return inode;
    }

    bool SuperBlockBase::destroyInode(InodeBase* inode) {
        // Remove the inode from the superblock's inode list
        if (!removeInode(inode)) return false;

        // Free the memory associated with the inode
        kernel::heapManager.free(inode);
        return true;
    }

    /// endregion


    /// region InodeBase
    size_t InodeBase::inodes = 1;

    InodeBase::InodeBase(Type type, Mode mode, UserID userId, GroupID groupId)
        : inodeNumber_(inodes++), mode_(mode), type_(type), userId_(userId), groupId_(groupId), size_(0),  // TODO better constructor
          accessTime_(0), modificationTime_(0), changeTime_(0), superBlock_(nullptr) {
        LOG_DEBUG("Constructing inode: %d", inodeNumber_);
    }

    size_t InodeBase::getInodeNumber() const {
        // Return the unique inode number.
        return inodeNumber_;
    }

    InodeBase::Mode InodeBase::getMode() const {
        // Return the mode (permissions) of the inode.
        return mode_;
    }

    InodeBase::UserID InodeBase::getUserId() const {
        // Return the user ID of the inode.
        return userId_;
    }

    InodeBase::GroupID InodeBase::getGroupId() const {
        // Return the group ID of the inode.
        return groupId_;
    }

    InodeBase::Type InodeBase::getType() const {
        // Return the type of the inode.
        return type_;
    }

    void InodeBase::setSuperBlock(SuperBlockBase* superBlock) {
        // Set the superBlock_ to the provided superBlock.
        superBlock_ = superBlock;
    }

    SuperBlockBase* InodeBase::getSuperBlock() {
        // Return the pointer to the associated superBlock.
        return superBlock_;
    }

    void InodeBase::addDentry(KString& name, InodeBase* dentry) {
        // Ensure the dentry is not null, otherwise, trigger a kernel panic.
        if (!dentry) kernelPanic("Called %s on a null dentry", __PRETTY_FUNCTION__);

        // Set the superBlock of the dentry to the same as this inode.
        dentry->setSuperBlock(getSuperBlock());

        // Add the dentry to the map of directory entries.
        dentries_[name] = dentry;
    }

    InodeBase* InodeBase::getDentry(const KString& name) {
        // Find the directory entry in the map.
        auto it = dentries_.find(name);

        // If found, return the associated inode.
        if (it != dentries_.end()) { return it->second; }

        // If not found, return nullptr.
        return nullptr;
    }

    size_t InodeBase::read(char* buffer, size_t size, size_t offset) {
        // By default, reading is not supported, so return 0 bytes read.
        return 0;
    }

    size_t InodeBase::write(const char* buffer, size_t size, size_t offset) {
        // By default, writing is not supported, so return 0 bytes written.
        return 0;
    }

    int InodeBase::open() {
        // By default, opening the inode returns a status code of 0.
        return 0;
    }

    int InodeBase::close() {
        // By default, closing the inode returns a status code of 0.
        return 0;
    }

    KVector<std::pair<KString, InodeBase*>> InodeBase::getDentries(size_t offset, size_t count) {
        // Create a vector to hold the directory entries.
        KVector<std::pair<KString, InodeBase*>> dentryVector;

        // Skip the entries up to the offset.
        auto it = dentries_.begin();
        std::advance(it, offset);

        // Add each entry in the map to the vector, considering the limit.
        for (size_t index = 0; it != dentries_.end() && index < offset + count; ++it, ++index) { dentryVector.emplace_back(*it); }

        // Return the vector of directory entries.
        return dentryVector;
    }

    bool InodeBase::removeDentry(const KString& name) {
        // Find the directory entry in the map.
        auto it = dentries_.find(name);

        // If found, erase it from the map.
        if (it != dentries_.end()) {
            heapManager.free(it->second);
            dentries_.erase(it);
            return true;  // Return true indicating the entry was removed.
        }

        // If not found, return false.
        return false;
    }

    int InodeBase::ioctl(int request, void* arg) {
        // By default, return an error indicating ioctl is not supported.
        return -1;  // Error code indicating unsupported operation
    }

    void InodeBase::clearDentries() {
        // destruct all inner nodes
        for (auto& [childName, inode]: dentries_) { heapManager.free(inode); }
        dentries_.clear();
    }

    InodeBase::~InodeBase() { clearDentries(); }

    size_t InodeBase::getSize() const { return size_; }

    /// endregion


    InodeBase::Mode operator|(InodeBase::Mode lhs, InodeBase::Mode rhs) {
        // Perform a bitwise OR operation on the mode values and cast back to InodeBase::Mode.
        return (InodeBase::Mode)((uint32_t) lhs | (uint32_t) rhs);
    }

}  // namespace PalmyraOS::kernel::vfs
