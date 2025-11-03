
#include "core/files/Fat32FileSystem.h"
#include "core/files/VirtualFileSystemBase.h"
#include "core/peripherals/Logger.h"
#include "libs/memory.h"


/// region FAT32Directory

PalmyraOS::kernel::vfs::FAT32Directory::FAT32Directory(FAT32Partition& parentPartition,
                                                       uint32_t directoryStartCluster,
                                                       InodeBase::Mode mode,
                                                       InodeBase::UserID userId,
                                                       InodeBase::GroupID groupId)
    : InodeBase(InodeBase::Type::Directory, mode, userId, groupId), parentPartition_(parentPartition), directoryStartCluster_(directoryStartCluster) {}

PalmyraOS::kernel::KVector<std::pair<PalmyraOS::kernel::KString, PalmyraOS::kernel::vfs::InodeBase*>>

PalmyraOS::kernel::vfs::FAT32Directory::getDentries(size_t offset, size_t count) {

    // Initialize all dentries again
    // LOG_DEBUG("Browsing Folder at %d:", directoryStartCluster_);

    // Clear all dentries

    // TODO: cache
    clearDentries();

    // Retrieve all dentries
    //	LOG_DEBUG("Retrieving Dentries");

    KVector<DirectoryEntry> entries = parentPartition_.getDirectoryEntries(directoryStartCluster_);

    //	LOG_DEBUG("Retrieved %d Dentries", entries.size());

    // Add them to local register
    // skip '.', '..'
    for (int i = 2; i < entries.size(); ++i) {
        auto& entry = entries[i];
        // Add directory
        if ((uint8_t) (entry.getAttributes() & EntryAttribute::Directory)) {
            auto longName = entry.getNameLong();

            //			LOG_DEBUG("Entry '%s': %d", longName.c_str(), directoryStartCluster_);
            // Create a test inode with a lambda function for reading test string
            auto rtcNode  = kernel::heapManager.createInstance<vfs::FAT32Directory>(parentPartition_, entry.getFirstCluster(), getMode(), getUserId(), getGroupId());
            if (!rtcNode) continue;

            InodeBase::addDentry(longName, rtcNode);
        }

        // TODO Add Archive / File

        else if ((uint8_t) (entry.getAttributes() & EntryAttribute::Archive)) {
            auto longName = entry.getNameLong();
            // Create a test inode with a lambda function for reading test string
            auto rtcNode  = kernel::heapManager.createInstance<vfs::FAT32Archive>(parentPartition_, entry, getMode(), getUserId(), getGroupId());
            if (!rtcNode) continue;

            InodeBase::addDentry(longName, rtcNode);
        }
    }

    return InodeBase::getDentries(offset, count);
}

PalmyraOS::kernel::vfs::InodeBase* PalmyraOS::kernel::vfs::FAT32Directory::getDentry(const PalmyraOS::kernel::KString& name) {
    // Just update in case it is empty which is clearl not as expected.
    if (dentries_.empty()) getDentries(0, 100);

    return InodeBase::getDentry(name);
}


PalmyraOS::kernel::vfs::InodeBase* PalmyraOS::kernel::vfs::FAT32Directory::createFile(const PalmyraOS::kernel::KString& name,
                                                                                      PalmyraOS::kernel::vfs::InodeBase::Mode mode,
                                                                                      PalmyraOS::kernel::vfs::InodeBase::UserID userId,
                                                                                      PalmyraOS::kernel::vfs::InodeBase::GroupID groupId) {

    // Construct a minimal DirectoryEntry representing this directory (for FAT32Partition::createFile)
    fat_dentry dirDentry{};
    memset(&dirDentry, 0, sizeof(dirDentry));
    dirDentry.attribute        = static_cast<uint8_t>(EntryAttribute::Directory);
    dirDentry.firstClusterLow  = static_cast<uint16_t>(directoryStartCluster_ & 0xFFFF);
    dirDentry.firstClusterHigh = static_cast<uint16_t>((directoryStartCluster_ >> 16) & 0xFFFF);
    DirectoryEntry parentDirEntry(/*offset*/ 0, /*directoryStartCluster*/ directoryStartCluster_, KString("."), dirDentry);

    auto created = parentPartition_.createFile(parentDirEntry, name, EntryAttribute::Archive);
    if (!created.has_value()) return nullptr;

    auto* fileInode = kernel::heapManager.createInstance<FAT32Archive>(parentPartition_, *created, mode, userId, groupId);
    if (!fileInode) return nullptr;

    // Register the new file under this directory's dentries
    KString fileName = name;  // make a mutable copy
    InodeBase::addDentry(fileName, fileInode);
    return fileInode;
}


PalmyraOS::kernel::vfs::InodeBase* PalmyraOS::kernel::vfs::FAT32Directory::createDirectory(const PalmyraOS::kernel::KString& name,
                                                                                           PalmyraOS::kernel::vfs::InodeBase::Mode mode,
                                                                                           PalmyraOS::kernel::vfs::InodeBase::UserID userId,
                                                                                           PalmyraOS::kernel::vfs::InodeBase::GroupID groupId) {

    // Construct a minimal DirectoryEntry representing this directory (for FAT32Partition::createDirectory)
    fat_dentry dirDentry{};
    memset(&dirDentry, 0, sizeof(dirDentry));
    dirDentry.attribute        = static_cast<uint8_t>(EntryAttribute::Directory);
    dirDentry.firstClusterLow  = static_cast<uint16_t>(directoryStartCluster_ & 0xFFFF);
    dirDentry.firstClusterHigh = static_cast<uint16_t>((directoryStartCluster_ >> 16) & 0xFFFF);
    DirectoryEntry parentDirEntry(/*offset*/ 0, /*directoryStartCluster*/ directoryStartCluster_, KString("."), dirDentry);

    auto created = parentPartition_.createDirectory(parentDirEntry, name);
    if (!created.has_value()) return nullptr;

    auto* dirInode = kernel::heapManager.createInstance<FAT32Directory>(parentPartition_, created->getFirstCluster(), mode, userId, groupId);
    if (!dirInode) return nullptr;

    // Register the new directory under this directory's dentries
    KString dirName = name;  // make a mutable copy
    InodeBase::addDentry(dirName, dirInode);
    return dirInode;
}

bool PalmyraOS::kernel::vfs::FAT32Directory::deleteFile(const KString& name) {
    fat_dentry dirDentry{};
    memset(&dirDentry, 0, sizeof(dirDentry));
    dirDentry.attribute        = static_cast<uint8_t>(EntryAttribute::Directory);
    dirDentry.firstClusterLow  = static_cast<uint16_t>(directoryStartCluster_ & 0xFFFF);
    dirDentry.firstClusterHigh = static_cast<uint16_t>((directoryStartCluster_ >> 16) & 0xFFFF);
    DirectoryEntry parentDirEntry(0, directoryStartCluster_, KString("."), dirDentry);

    bool success = parentPartition_.deleteFile(parentDirEntry, name);
    if (!success) return false;

    InodeBase::removeDentry(name);
    return true;
}

bool PalmyraOS::kernel::vfs::FAT32Directory::deleteDirectory(const KString& name) {
    fat_dentry dirDentry{};
    memset(&dirDentry, 0, sizeof(dirDentry));
    dirDentry.attribute        = static_cast<uint8_t>(EntryAttribute::Directory);
    dirDentry.firstClusterLow  = static_cast<uint16_t>(directoryStartCluster_ & 0xFFFF);
    dirDentry.firstClusterHigh = static_cast<uint16_t>((directoryStartCluster_ >> 16) & 0xFFFF);
    DirectoryEntry parentDirEntry(0, directoryStartCluster_, KString("."), dirDentry);

    bool success = parentPartition_.deleteDirectory(parentDirEntry, name);
    if (!success) return false;

    InodeBase::removeDentry(name);
    return true;
}

/// endregion


/// region FAT32Archive

PalmyraOS::kernel::vfs::FAT32Archive::FAT32Archive(PalmyraOS::kernel::vfs::FAT32Partition& parentPartition,
                                                   const DirectoryEntry& directoryEntry,
                                                   PalmyraOS::kernel::vfs::InodeBase::Mode mode,
                                                   PalmyraOS::kernel::vfs::InodeBase::UserID userId,
                                                   PalmyraOS::kernel::vfs::InodeBase::GroupID groupId)
    : InodeBase(InodeBase::Type::File, mode, userId, groupId), parentPartition_(parentPartition), directoryEntry_(directoryEntry) {
    // TODO add other
    InodeBase::size_ = directoryEntry.getFileSize();
}

size_t PalmyraOS::kernel::vfs::FAT32Archive::read(char* buffer, size_t size, size_t offset) {
    // If the offset is greater than or equal to the file size, return 0 as there's nothing to read
    if (offset >= getSize()) return 0;

    // Read the entire file data starting from the beginning
    KVector<uint8_t> data = parentPartition_.readFile(directoryEntry_.getFirstCluster(), offset, size);

    // Calculate the number of bytes to read
    size_t bytesToRead    = std::min(size, data.size());

    // Copy the relevant data into the buffer
    memcpy(buffer, data.data(), bytesToRead);

    return bytesToRead;
}

size_t PalmyraOS::kernel::vfs::FAT32Archive::write(const char* buffer, size_t size, size_t offset) {
    // Convert buffer to KVector for FAT32Partition compatibility
    KVector<uint8_t> data(size);
    memcpy(data.data(), buffer, size);

    // Delegate to partition-level write with offset
    bool success = parentPartition_.writeAtOffset(directoryEntry_, data, offset);

    if (!success) {
        return 0;  // Write failed, return 0 bytes written
    }

    // Update cached size if we extended the file
    size_t newSize = offset + size;
    if (newSize > getSize()) {
        InodeBase::size_ = newSize;
    }

    return size;  // Return number of bytes written
}

int PalmyraOS::kernel::vfs::FAT32Archive::truncate(size_t newSize) {
    // For now support truncating to zero only
    if (newSize != 0) return -1;

    KVector<uint8_t> empty;
    bool ok = parentPartition_.write(directoryEntry_, empty);
    if (!ok) return -1;

    // Update cached size and metadata locally
    InodeBase::size_ = 0;
    directoryEntry_.setFileSize(0);
    directoryEntry_.setClusterChain(0);
    return 0;
}

/// endregion
