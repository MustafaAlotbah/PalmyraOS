
#pragma once

#include "core/files/VirtualFileSystemBase.h"
#include "core/files/partitions/Fat32.h"


namespace PalmyraOS::kernel::vfs {

    class FAT32Archive : public InodeBase {
    public:
        explicit FAT32Archive(FAT32Partition& parentPartition, const DirectoryEntry& directoryEntry, InodeBase::Mode mode, InodeBase::UserID userId, InodeBase::GroupID groupId);

        size_t read(char* buffer, size_t size, size_t offset) override;

        int truncate(size_t newSize) override;

    private:
        FAT32Partition& parentPartition_;
        DirectoryEntry directoryEntry_;
    };

    class FAT32Directory : public InodeBase {
    public:
        explicit FAT32Directory(FAT32Partition& parentPartition, uint32_t directoryStartCluster, InodeBase::Mode mode, InodeBase::UserID userId, InodeBase::GroupID groupId);

        [[nodiscard]] KVector<std::pair<KString, InodeBase*>> getDentries(size_t offset, size_t count) final;

        InodeBase* getDentry(const KString& name) final;

        InodeBase* createFile(const KString& name, InodeBase::Mode mode, InodeBase::UserID userId, InodeBase::GroupID groupId) override;

    private:
        FAT32Partition& parentPartition_;
        uint32_t directoryStartCluster_;
    };

}  // namespace PalmyraOS::kernel::vfs