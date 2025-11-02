/* file: PalmyraOS/core/files/partitions/Fat32.h
 * Fat32
 *
 * Purpose: Define the structures and classes for handling FAT32 file system partitions.
 *
 * This file is part of the Virtual File System (VFS) of the PalmyraOS kernel. The main purpose
 * is to manage FAT32 file systems, providing functionality to read directories, files, and
 * manage the structure of the FAT32 partition.
 * TODO: manage files, write to files etc..
 *
 * References:
 * - https://wiki.osdev.org/FAT
 * -
 * https://cscie92.dce.harvard.edu/spring2024/Microsoft%20Extensible%20Firmware%20Initiative%20FAT32%20File%20System%20Specification,%20Version%201.03,%2020001206.pdf
 */

#pragma once


#include "core/definitions.h"
#include "core/files/partitions/VirtualDisk.h"
#include "core/memory/KernelHeapAllocator.h"
#include "core/peripherals/ATA.h"
#include <optional>


namespace PalmyraOS::kernel::vfs {

    constexpr uint32_t DEFAULT_TIMEOUT = 200;

    class FAT32Partition;

    struct fat_dentry {
        char shortName[11];         ///< Short name in 8.3 format (8 chars name + 3 chars extension)
        uint8_t attribute;          ///< File attributes (e.g., read-only, hidden)
        uint8_t ntRes;              ///< Reserved for Windows NT
        uint8_t creationTimeTenth;  ///< Milliseconds part of the creation time
        uint16_t creationTime;      ///< Creation time (hours, minutes, seconds)
        uint16_t creationDate;      ///< Creation date (year, month, day)
        uint16_t lastAccessDate;    ///< Last access date
        uint16_t firstClusterHigh;  ///< High word of the first cluster number (used in FAT32)
        uint16_t writeTime;         ///< Last write time
        uint16_t writeDate;         ///< Last write date
        uint16_t firstClusterLow;   ///< Low word of the first cluster number
        uint32_t fileSize;          ///< Size of the file in bytes
    } __attribute__((packed));

    enum class EntryAttribute : uint8_t {
        Invalid   = 0x0,   ///< Invalid attribute
        ReadOnly  = 0x01,  ///< Read-only file
        Hidden    = 0x02,  ///< Hidden file
        System    = 0x04,  ///< System file
        VolumeID  = 0x08,  ///< Volume label
        Directory = 0x10,  ///< Directory
        Archive   = 0x20,  ///< Archive (regular file)
                           // Rest of the byte 0x40, 0x80 are reserved
    };

    class DirectoryEntry {
    public:
    public:
        explicit DirectoryEntry(uint32_t offset, uint32_t directoryStartCluster, KString longName, fat_dentry dentry);

        DirectoryEntry();

        [[nodiscard]] fat_dentry getFatDentry() const;

        // setters
        void setFileSize(uint32_t newFileSize);
        void setClusterChain(uint32_t startingCluster);

        // Getters for various attributes of the directory entry
        [[nodiscard]] uint32_t getOffset() const;
        [[nodiscard]] uint32_t getDirectoryCluster() const;
        [[nodiscard]] KString getNameShort() const;
        [[nodiscard]] EntryAttribute getAttributes() const;
        [[nodiscard]] uint8_t getNTRes() const;
        [[nodiscard]] uint8_t getCreationTimeMs() const;
        [[nodiscard]] uint16_t getCreationTime() const;
        [[nodiscard]] uint16_t getCreationDate() const;
        [[nodiscard]] uint16_t getLastAccessDate() const;
        [[nodiscard]] uint16_t getWriteTime() const;
        [[nodiscard]] uint16_t getWriteDate() const;
        [[nodiscard]] uint16_t getFirstCluster() const;
        [[nodiscard]] uint32_t getFileSize() const;
        [[nodiscard]] KString getNameLong() const;

    private:
        uint32_t offset_;                 ///< Offset in the directory
        uint32_t directoryStartCluster_;  ///< directoryStartCluster
        KString longName_;                ///< Long name of the entry
        KString shortName_;               ///< Short name (8.3 format)
        EntryAttribute attributes_;       ///< File attributes
        uint8_t NTRes_;                   ///< Reserved for Windows NT
        uint8_t creationTimeMs_;          ///< Milliseconds part of creation time
        uint16_t creationTime_;           ///< Creation time
        uint16_t creationDate_;           ///< Creation date
        uint16_t lastAccessDate_;         ///< Last access date
        uint16_t writeTime_;              ///< Last write time
        uint16_t writeDate_;              ///< Last write date
        uint16_t clusterChain_;           ///< First cluster in the cluster chain
        uint32_t fileSize_;               ///< File size in bytes
    };

    /**
     * @brief Represents a FAT32 partition
     *
     * This class provides methods to access and manage a FAT32 partition, including
     * reading directory entries and file data.
     */
    class FAT32Partition {
    public:
        enum class Type {
            Invalid,  ///< Invalid type
            FAT12,    ///< FAT12 file system
            FAT16,    ///< FAT16 file system
            FAT32     ///< FAT32 file system
        };

    public:
        explicit FAT32Partition(VirtualDisk<ATA>& diskDriver, uint32_t startSector, uint32_t countSectors);

        [[nodiscard]] Type getType();

        // Reading FAT Files from a cluster
        [[nodiscard]] KVector<uint8_t> readFile(uint32_t startCluster, uint32_t size) const;
        [[nodiscard]] KVector<uint8_t> readFile(uint32_t startCluster, uint32_t offset, uint32_t size) const;
        [[nodiscard]] KVector<uint8_t> readEntireFile(uint32_t startCluster) const;

        // Method to parse directory entries from raw data
        [[nodiscard]] KVector<DirectoryEntry> getDirectoryEntries(uint32_t directoryStartCluster) const;
        [[nodiscard]] DirectoryEntry resolvePathToEntry(const KString& path) const;

        // File methods
        [[nodiscard]] KVector<uint8_t> read(const DirectoryEntry& entry, uint32_t offset, uint32_t countBytes) const;
        [[nodiscard]] bool append(DirectoryEntry& entry, const KVector<uint8_t>& bytes);
        [[nodiscard]] bool write(DirectoryEntry& entry, const KVector<uint8_t>& bytes);

        // Directory methods TODO
        std::optional<DirectoryEntry> createFile(DirectoryEntry& directoryEntry, const KString& fileName, EntryAttribute attributes = EntryAttribute::Archive);

        /**
         * @brief Create a new directory within a parent directory
         *
         * Creates a new directory by:
         * 1. Creating a directory entry in the parent
         * 2. Allocating a cluster for the directory
         * 3. Initializing with "." and ".." entries (FAT32 spec compliant)
         * 4. Writing the directory to disk
         *
         * @param parentDirEntry The parent directory entry where the new directory will be created
         * @param dirName The name of the new directory (without path separators)
         * @return DirectoryEntry of the newly created directory on success, std::nullopt on failure
         */
        std::optional<DirectoryEntry> createDirectory(DirectoryEntry& parentDirEntry, const KString& dirName);

    private:
        // Helper methods for parsing the BIOS Parameter Block (BPB) and initializing fields
        [[nodiscard]] bool parseBIOSParameterBlock();
        [[nodiscard]] bool initializeAdditionalFields();
        [[nodiscard]] std::pair<uint32_t, uint32_t> calculateFATOffset(uint32_t n) const;

        // Methods for accessing and reading the FAT32 partition
        [[nodiscard]] uint32_t getNextCluster(uint32_t cluster) const;
        [[nodiscard]] uint32_t getSectorFromCluster(uint32_t cluster) const;
        [[nodiscard]] KVector<uint32_t> readClusterChain(uint32_t startCluster) const;
        [[nodiscard]] KVector<uint32_t> readClusterChain(uint32_t startCluster, uint32_t offset, uint32_t size) const;
        [[nodiscard]] std::optional<uint32_t> allocateCluster();
        void freeClusterChain(uint32_t startCluster);
        [[nodiscard]] bool setNextCluster(uint32_t cluster, uint32_t nextCluster);
        [[nodiscard]] bool writeCluster(uint32_t cluster, const KVector<uint8_t>& data);

        // File methods
        [[nodiscard]] KVector<uint8_t> fetchDataFromEntry(const DirectoryEntry&, uint32_t off, uint32_t count) const;


    private:
        // TODO directory methods
        [[nodiscard]] bool flushEntry(const DirectoryEntry& entry);

        static bool isValidSFNCharacter(char c);

        static KString generateUniqueShortName(const KString& longName, const KVector<KString>& existingShortNames);

        static uint8_t calculateShortNameChecksum(const char* shortName);

        static KVector<fat_dentry> createLFNEntries(const KString& longName, uint8_t checksum);

        static bool needsLFN(const KString& fileName);

    private:
        VirtualDisk<ATA>& diskDriver_;  // Reference to the ATA disk driver
        uint32_t startSector_;          // Starting sector of the partition
        uint32_t countSectors_;         // Total number of sectors in the partition
        uint32_t clusterSizeBytes_;     // cluster size in bytes

        // BIOS Parameter Block fields
        uint16_t sectorSize_{};            // Size of a sector in bytes
        uint8_t clusterSize_{};            // Number of sectors per cluster
        uint16_t countReservedSectors_{};  // Number of reserved sectors
        uint8_t countFATs_{};              // Number of FAT tables
        uint16_t countRootEntries_{};      // Number of root directory entries (FAT12/16 only)
        uint16_t countSectors16_{};        // Total number of sectors (FAT12/16 only)
        uint16_t fatSize16_{};             // Size of each FAT in sectors (FAT12/16 only)
        uint32_t hiddenSectors_{};         // Number of hidden sectors preceding the partition
        uint32_t totalSectors32_{};        // Total number of sectors (FAT32 only)
        uint32_t fatSize32_{};             // Size of each FAT in sectors (FAT32 only)
        uint16_t extendedFlags_{};         // Extended flags
        uint16_t fileSystemVersion_{};     // File system version
        uint32_t rootCluster_{};           // Cluster number of the root directory
        uint16_t fsInfoSector_{};          // Sector number of the FSInfo structure
        uint16_t backupBootSector_{};      // Sector number of the backup boot sector
        uint8_t driveNumber_{};            // Drive number
        uint8_t bootSignature_{};          // Extended boot signature
        uint32_t volumeID_{};              // Volume ID
        char volumeLabel_[11]{};           // Volume label
        char fileSystemType_[8]{};         // File system type

        // Additional calculated fields
        uint32_t rootDirSectors_{};              // Number of root directory sectors
        uint32_t fatSize_{};                     // Size of each FAT in sectors
        uint32_t totalSectors_{};                // Total number of sectors
        uint32_t firstDataSector_{};             // First data sector
        uint32_t reservedAndFatSectorsCount_{};  // Total number of reserved and FAT sectors
        uint32_t dataSectorCount_{};             // Total number of data sectors
        uint32_t countClusters_{};               // Total number of clusters
        Type type_;                              // FAT type (FAT12, FAT16, FAT32)
        uint8_t fatOffsetMult_{};                // FAT offset multiplier based on FAT type
    };

    // Operator overloads for bitwise operations on DirectoryEntry::Attribute
    EntryAttribute operator|(EntryAttribute lhs, EntryAttribute rhs);

    EntryAttribute& operator|=(EntryAttribute& lhs, EntryAttribute rhs);

    EntryAttribute operator&(EntryAttribute lhs, EntryAttribute rhs);


}  // namespace PalmyraOS::kernel::vfs
