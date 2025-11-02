#include <algorithm>
#include <limits>
#include <utility>

#include "core/encodings.h"  // utf16-le -> utf8
#include "core/files/partitions/Fat32.h"
#include "core/panic.h"  // panic
#include "core/peripherals/Logger.h"

#include "libs/memory.h"
#include "libs/utils.h"


namespace PalmyraOS::kernel::vfs {

    /// region FAT32Partition

    FAT32Partition::FAT32Partition(VirtualDisk<ATA>& diskDriver, uint32_t startSector, uint32_t countSectors)
        : diskDriver_(diskDriver), startSector_(startSector), countSectors_(countSectors), type_(Type::Invalid) {
        // Parse the BIOS Parameter Block to initialize fields
        if (!parseBIOSParameterBlock()) {
            LOG_ERROR("FAT32Partition construction failed: Unable to parse BIOS Parameter Block.");
            return;
        }

        // Initialize additional fields required for the FAT32 partition
        if (!initializeAdditionalFields()) {
            LOG_ERROR("FAT32Partition construction failed: Unable to initialize additional fields.");
            return;
        }

        clusterSizeBytes_ = clusterSize_ * sectorSize_;
    }

    bool FAT32Partition::parseBIOSParameterBlock() {
        uint8_t bootSector[512];
        bool diskStatus = diskDriver_.readSector(startSector_, bootSector, DEFAULT_TIMEOUT);
        if (!diskStatus) {
            LOG_ERROR("Failed to read boot sector from disk.");
            return false;
        }

        // Read and store values from the boot sector
        sectorSize_           = get_uint16_t(bootSector, 11);
        clusterSize_          = get_uint8_t(bootSector, 13);
        countReservedSectors_ = get_uint16_t(bootSector, 14);
        countFATs_            = get_uint8_t(bootSector, 16);
        countRootEntries_     = get_uint16_t(bootSector, 17);
        countSectors16_       = get_uint16_t(bootSector, 19);
        //	  mediaDescriptor_      = get_uint8_t(bootSector, 21);
        fatSize16_            = get_uint16_t(bootSector, 22);
        //	  sectorsPerTrack_      = get_uint16_t(bootSector, 24);
        //	  numberOfHeads_        = get_uint16_t(bootSector, 26);
        hiddenSectors_        = get_uint32_t(bootSector, 28);
        totalSectors32_       = get_uint32_t(bootSector, 32);
        fatSize32_            = get_uint32_t(bootSector, 36);
        extendedFlags_        = get_uint16_t(bootSector, 40);
        fileSystemVersion_    = get_uint16_t(bootSector, 42);
        rootCluster_          = get_uint32_t(bootSector, 44);
        fsInfoSector_         = get_uint16_t(bootSector, 48);
        backupBootSector_     = get_uint16_t(bootSector, 50);
        driveNumber_          = get_uint8_t(bootSector, 64);
        bootSignature_        = get_uint8_t(bootSector, 66);
        volumeID_             = get_uint32_t(bootSector, 67);
        memcpy(volumeLabel_, bootSector + 71, 11);
        memcpy(fileSystemType_, bootSector + 82, 8);

        // Log critical boot sector fields for VFAT debugging
        char oemName[9] = {0};
        memcpy(oemName, bootSector + 3, 8);
        LOG_WARN("[FAT32] Boot Sector OEM Name: '%.8s'", oemName);
        LOG_WARN("[FAT32] Boot Sector FS Type: '%.8s'", fileSystemType_);
        LOG_WARN("[FAT32] Boot Sector Volume Label: '%.11s'", volumeLabel_);
        LOG_WARN("[FAT32] Drive Number: 0x%02X", driveNumber_);
        LOG_WARN("[FAT32] Boot Signature: 0x%02X", bootSignature_);

        // some assertions
        if (sectorSize_ == 0 || clusterSize_ == 0) {
            LOG_ERROR("Invalid sector size or cluster size.");
            return false;
        }

        return true;
    }

    bool FAT32Partition::initializeAdditionalFields() {

        /**
         * This function calculates additional fields necessary for navigating and managing
         * the FAT32 file system. These include the number of root directory sectors, the size of the FAT,
         * the total number of sectors, and the first data sector. It also determines the FAT type (FAT12, FAT16, FAT32)
         * and sets up necessary multipliers and offsets.
         */


        // Calculate the number of root directory sectors (FAT12/16 only), (0 for FAT32)
        rootDirSectors_             = ((countRootEntries_ * 32) + (sectorSize_ - 1)) / sectorSize_;

        // Determine the FAT size based on the FAT16 and FAT32 fields.
        fatSize_                    = (fatSize16_ == 0) ? fatSize32_ : fatSize16_;

        // Determine the total number of sectors based on FAT12/16 and FAT32 fields.
        totalSectors_               = (countSectors16_ == 0) ? totalSectors32_ : countSectors16_;

        // Calculate the first data sector.
        firstDataSector_            = startSector_ + countReservedSectors_ + (countFATs_ * fatSize_) + rootDirSectors_;

        // Calculate the total number of reserved and FAT sectors.
        reservedAndFatSectorsCount_ = countReservedSectors_ + (countFATs_ * fatSize_) + rootDirSectors_;

        // Calculate the total number of data sectors.
        dataSectorCount_            = totalSectors_ - reservedAndFatSectorsCount_;

        // Calculate the total number of clusters.
        countClusters_              = dataSectorCount_ / clusterSize_;  // floor division

        // Determine the FAT type based on the number of clusters.
        if (countClusters_ < 3) type_ = Type::Invalid;
        else if (countClusters_ < 4085) type_ = Type::FAT12;
        else if (countClusters_ < 65525) type_ = Type::FAT16;
        else type_ = Type::FAT32;

        // Set the FAT offset multiplier based on the FAT type.
        if (type_ == Type::FAT16) fatOffsetMult_ = 2;
        else if (type_ == Type::FAT32) fatOffsetMult_ = 4;
        // FAT12 uses a multiplier of 1.5, not handled here.

        // Now, only testing FAT32
        if (type_ != Type::FAT32) {
            LOG_ERROR("FAT system is not FAT32. This feature is currently not tested.");
            return false;
        }

        return true;
    }

    std::pair<uint32_t, uint32_t> FAT32Partition::calculateFATOffset(uint32_t n) const {
        /**
         * This function calculates the sector and offset within the FAT where the entry
         * for a given cluster number can be found. This is essential for navigating the FAT to find
         * the next cluster in a file or directory chain.
         */

        // Calculate the FAT offset.
        uint32_t fatOffset       = fatOffsetMult_ * n;

        // Calculate the FAT sector number.
        uint32_t fatSectorNumber = countReservedSectors_ + (fatOffset / sectorSize_);  // floor

        // Calculate the entry offset within the sector.
        uint32_t fatEntryOffset  = fatOffset % sectorSize_;  // remainder

        return {fatSectorNumber + startSector_, fatEntryOffset};
    }

    uint32_t FAT32Partition::getNextCluster(uint32_t cluster) const {
        /**
         * This function retrieves the next cluster number in a cluster chain by reading
         * the FAT entry for the current cluster. This is used to follow the chain of clusters that
         * make up a file or directory.
         */

        if (cluster >= countClusters_) kernelPanic("%s: Invalid cluster index!", __PRETTY_FUNCTION__);

        // Calculate the sector and offset in the FAT.
        auto [fatSector, entryOffset] = calculateFATOffset(cluster);

        // Read the FAT sector from the disk.
        uint8_t sectorData[512];
        bool diskStatus = diskDriver_.readSector(fatSector, sectorData, DEFAULT_TIMEOUT);
        if (!diskStatus) {
            LOG_ERROR("Failed to read FAT sector %u.", fatSector);
            return 0;
        }

        // Interpret the next cluster number based on the FAT type.
        if (type_ == Type::FAT32) {
            uint32_t nextCluster = get_uint32_t(sectorData, entryOffset) & 0x0FFFFFFF;
            // Valid range check for FAT32
            if (nextCluster >= 0x0FFFFFF8) return 0xFFFFFFFF;  // End of chain marker
            return nextCluster;
        }
        else if (type_ == Type::FAT16) {
            uint16_t nextCluster = get_uint16_t(sectorData, entryOffset);
            // Valid range check for FAT16
            if (nextCluster >= 0xFFF8) return 0xFFFFFFFF;  // End of chain marker
            return nextCluster;
        }
        else if (type_ == Type::FAT12) {
            uint16_t nextCluster = get_uint16_t(sectorData, entryOffset);
            if (cluster & 1) {
                // For odd clusters, shift right by 4 bits
                nextCluster = nextCluster >> 4;
            }
            else {
                // For even clusters, mask the higher 4 bits
                nextCluster = nextCluster & 0x0FFF;
            }
            // Valid range check for FAT12
            if (nextCluster >= 0xFF8) return 0xFFFFFFFF;  // End of chain marker
            return nextCluster;
        }

        // We should not reach this part
        kernelPanic("function: %s:\nInvalid FAT type!", __PRETTY_FUNCTION__);
        return 0;
    }

    uint32_t FAT32Partition::getSectorFromCluster(uint32_t cluster) const {
        /**
         * This function calculates the starting sector of a given cluster. It translates
         * a cluster number into the corresponding sector number on the disk, which is necessary for
         * reading or writing data in the cluster.
         */

        if (cluster < rootCluster_ || cluster >= countClusters_) kernelPanic("%s: Invalid cluster index!", __PRETTY_FUNCTION__);

        // First data sector offset plus cluster offset.
        return firstDataSector_ + (cluster - 2) * clusterSize_;
    }

    KVector<uint32_t> FAT32Partition::readClusterChain(uint32_t startCluster) const {
        /**
         * This function reads the entire cluster chain starting from a given cluster.
         * It follows the chain of clusters that make up a file or directory by reading the FAT
         * to find each successive cluster in the chain.
         */

        if (startCluster >= countClusters_) kernelPanic("%s: Invalid cluster index!", __PRETTY_FUNCTION__);

        KVector<uint32_t> clusters;
        uint32_t currentCluster      = startCluster;
        const uint32_t maxIterations = 1024 * 1024;  // 1 GiB
        uint32_t iterations          = 0;

        // 0x0FFFFFF8 marks the end of the cluster chain in FAT32.
        while (currentCluster < 0x0FFFFFF8 && iterations < maxIterations) {
            // Check for loops in the cluster chain.
            if (std::find(clusters.begin(), clusters.end(), currentCluster) != clusters.end()) {
                LOG_ERROR("Detected loop in cluster chain at cluster %u.", currentCluster);
                break;
            }

            // free cluster
            if (currentCluster == 0) break;  // TODO investigation: is this to be expected?

            // Add the current cluster to the chain and move to the next cluster.
            clusters.push_back(currentCluster);
            currentCluster = getNextCluster(currentCluster);

            iterations++;
        }
        return clusters;
    }

    KVector<uint32_t> FAT32Partition::readClusterChain(uint32_t startCluster, uint32_t offset, uint32_t size) const {
        /**
         * This function reads the cluster chain starting by a given cluster and looks for the offset.
         * It follows the chain of clusters that make up a file or directory by reading the FAT
         * to find each successive cluster in the chain.
         */

        if (startCluster >= countClusters_) kernelPanic("%s: Invalid cluster index!", __PRETTY_FUNCTION__);

        // Calculate how many clusters we need to skip based on the offset
        uint32_t clusterSizeBytes = clusterSize_ * sectorSize_;
        uint32_t skipClusters     = offset / clusterSizeBytes;  // Number of clusters to skip based on the offset

        // Calculate how many clusters we need based on the size
        uint32_t requiredClusters = (size + clusterSizeBytes - 1) / clusterSizeBytes;  // Round up to the next cluster

        KVector<uint32_t> clusters;
        uint32_t currentCluster      = startCluster;
        uint32_t iterations          = 0;
        const uint32_t maxIterations = countClusters_;  // To avoid infinite loops in case of a corrupted FAT

        // Traverse the cluster chain, skipping unnecessary clusters and collecting only the required ones
        while (currentCluster < 0x0FFFFFF8 && iterations < maxIterations) {
            // If we've skipped enough clusters, start collecting them
            if (skipClusters == 0) {
                clusters.push_back(currentCluster);
                if (clusters.size() >= requiredClusters) break;  // Stop once we've collected enough clusters to satisfy the requested size
            }
            else {
                // Decrement the skip counter until we've skipped enough clusters
                skipClusters--;
            }

            // Move to the next cluster in the chain
            currentCluster = getNextCluster(currentCluster);
            iterations++;

            // Detect loops in the cluster chain (if the current cluster repeats)
            if (std::find(clusters.begin(), clusters.end(), currentCluster) != clusters.end()) {
                LOG_ERROR("Detected loop in cluster chain at cluster %u.", currentCluster);
                break;
            }
        }

        return clusters;
    }

    KVector<uint8_t> FAT32Partition::readFile(uint32_t startCluster, uint32_t size) const {
        /**
         * This function reads the clusters that make up a file, starting from the given
         * cluster. It returns the file data up to the specified size by following the cluster chain.
         */

        if (startCluster >= countClusters_) kernelPanic("%s: Invalid cluster index!", __PRETTY_FUNCTION__);

        KVector<uint8_t> data;
        KVector<uint32_t> clusters = readClusterChain(startCluster);
        auto bytesToRead           = size;

        // Maximum of ~2 GiB to avoid narrowing conversion with casting
        if (size > std::numeric_limits<KVector<uint8_t>::difference_type>::max()) {
            kernelPanic("%s: size of %u > std::numeric_limits<KVector<uint8_t>::difference_type>::max()!", __PRETTY_FUNCTION__, size);
        }

        // Read all clusters in the chain.
        for (const uint32_t& cluster: clusters) {
            if (bytesToRead <= 0) break;

            // Transform FAT cluster number to Disk Sector
            uint32_t sector = getSectorFromCluster(cluster);

            // Prepare a buffer for reading the sector.
            KVector<uint8_t> sectorData(sectorSize_ * clusterSize_);

            // Read the entire cluster into the buffer.
            for (uint8_t i = 0; i < clusterSize_; ++i) {
                bool diskStatus = diskDriver_.readSector(sector + i, sectorData.data() + i * sectorSize_, DEFAULT_TIMEOUT);
                if (!diskStatus) {
                    LOG_ERROR("Failed to read sector %u from disk.", sector + i);
                    return {};
                }
            }

            // Append the data to the file data buffer, up to the requested size.
            if (bytesToRead >= sectorData.size()) {
                data.insert(data.end(), sectorData.begin(), sectorData.end());
                bytesToRead -= static_cast<int32_t>(sectorData.size());
            }
            else {
                data.insert(data.end(), sectorData.begin(), sectorData.begin() + static_cast<KVector<uint8_t>::difference_type>(bytesToRead));
                bytesToRead = 0;
            }
        }

        return data;
    }

    KVector<uint8_t> FAT32Partition::readEntireFile(uint32_t startCluster) const {
        if (startCluster >= countClusters_) kernelPanic("%s: Invalid cluster index!", __PRETTY_FUNCTION__);
        return readFile(startCluster, std::numeric_limits<int32_t>::max());
    }

    KVector<DirectoryEntry> FAT32Partition::getDirectoryEntries(uint32_t directoryStartCluster) const {
        /**
         * Intuition: This function parses raw directory data to extract individual directory entries.
         * It processes both short name and long name entries, constructing complete directory entries
         * that can be used for further operations.
         */

        // TODO: add params: offset, count

        if (directoryStartCluster >= countClusters_)
            kernelPanic("%s: Invalid cluster index!\n"
                        "Count Clusters   : %d\n"
                        "Requested Cluster: %d\n",
                        __PRETTY_FUNCTION__,
                        countClusters_,
                        directoryStartCluster);

        KVector<uint8_t> data = readEntireFile(directoryStartCluster);
        KVector<DirectoryEntry> entries;
        KVector<std::pair<uint8_t, KWString>> longNameParts;
        uint8_t lfnChecksum = 0;  // Checksum from LFN entries

        for (size_t i = 0; i < data.size(); i += 32) {
            // Directory entry is a 32-byte structure.
            const uint8_t* entry = data.data() + i;

            if (entry[0] == 0x00) break;  // End of the directory
            if (entry[0] == 0xE5) {
                // Deleted entry - reset LFN collection since sequence is broken
                longNameParts.clear();
                lfnChecksum = 0;
                continue;
            }

            // Long name entry (part of a long name).
            // Per FAT32 spec line 1351: Use proper attribute mask check and verify not deleted
            // ATTR_LONG_NAME_MASK = 0x3F (ReadOnly|Hidden|System|VolumeID|Directory|Archive)
            // ATTR_LONG_NAME      = 0x0F (ReadOnly|Hidden|System|VolumeID)
            if ((entry[0] != 0xE5 && entry[0] != 0x00) && ((entry[11] & 0x3F) == 0x0F)) {
                // Check if this is the last LFN entry (marked with 0x40 bit).
                bool is_last_lfn = (entry[0] & 0x40) == 0x40;

                // Order of this LFN part (bits 0-4, mask out 0x40 flag).
                uint8_t order    = entry[0] & 0x1F;

                // Extract the LFN part from the entry.
                KWString part;
                {
                    for (size_t j = 1; j <= 10; j += 2) {
                        uint16_t utf16_char = (entry[j + 1] << 8) | entry[j];
                        part.push_back(utf16_char);
                    }
                    for (size_t j = 14; j <= 25; j += 2) {
                        uint16_t utf16_char = (entry[j + 1] << 8) | entry[j];
                        part.push_back(utf16_char);
                    }
                    for (size_t j = 28; j <= 31; j += 2) {
                        uint16_t utf16_char = (entry[j + 1] << 8) | entry[j];
                        part.push_back(utf16_char);
                    }
                }

                // Remove null characters from the LFN part.
                part.erase(std::remove(part.begin(), part.end(), wchar_t(0xFFFF)), part.end());
                part.erase(std::remove(part.begin(), part.end(), wchar_t(0x0000)), part.end());

                // Insert the LFN part into the list, sorted by order.
                longNameParts.insert(longNameParts.begin(), std::make_pair(order, part));

                // Start a new sequence if this is the last LFN part.
                // if (is_last_lfn) longNameParts = { std::make_pair(order, part) };
                lfnChecksum = entry[13];
            }

            else {
                // Regular directory entry.
                // Cast the 32-byte entry data to a fat_dentry structure.
                fat_dentry dentry = *(fat_dentry*) entry;

                // Combine all Long File Name (LFN) parts to form the complete long name.
                bool useLFN       = false;
                KString longName;
                if (!longNameParts.empty()) {
                    // Calculate checksum of the short name
                    uint8_t shortNameChecksum = calculateShortNameChecksum(reinterpret_cast<const char*>(dentry.shortName));

                    // Validate checksum matches
                    bool checksumValid        = (lfnChecksum == shortNameChecksum);

                    if (checksumValid) {
                        // Checksum matches - use the LFN entries
                        useLFN = true;
                        // Sort the LFN parts based on their order.
                        std::sort(longNameParts.begin(), longNameParts.end(), [](const auto& a, const auto& b) {
                            return a.first < b.first;  // Compare based on the first element (order).
                        });
                        KWString longNameUtf16;
                        for (const auto& part: longNameParts) longNameUtf16 += part.second;
                        longName = utf16le_to_utf8(longNameUtf16);
                    }
                    else {
                        // Checksum mismatch - ignore LFN entries, use short name only
                        LOG_WARN("LFN checksum mismatch for entry at offset %zu (LFN: 0x%02X, SFN: 0x%02X), ignoring LFN", i, lfnChecksum, shortNameChecksum);
                    }

                    longNameParts.clear();
                    lfnChecksum = 0;  // Reset for next entry
                }


                // Add the directory entry to the list.
                entries.emplace_back(i, directoryStartCluster, longName, dentry);
            }
        }

        return entries;
    }

    DirectoryEntry FAT32Partition::resolvePathToEntry(const KString& path) const {
        // Path must be absolute
        if (path.empty() || path[0] != '/') return {};

        // Tokenize the path to get individual directory/file names
        KVector<KString> tokens = path.split('/', true);

        // Special case for root directory
        if (tokens.empty() || (tokens.size() == 1 && tokens[0].empty())) {
            fat_dentry rootDentry = {.shortName         = "/",
                                     .attribute         = 0x10,  // Directory attribute
                                     .ntRes             = 0,
                                     .creationTimeTenth = 0,
                                     .creationTime      = 0,
                                     .creationDate      = 0,
                                     .lastAccessDate    = 0,
                                     .firstClusterHigh  = 0,
                                     .writeTime         = 0,
                                     .writeDate         = 0,
                                     .firstClusterLow   = static_cast<uint16_t>(rootCluster_ & 0xFFFF),
                                     .fileSize          = 0};
            return DirectoryEntry(0, 0, KString("/"), rootDentry);
        }

        // Start at the root directory
        uint32_t currentCluster = rootCluster_;

        // Iterate through the tokens to find the target node
        for (size_t i = 0; i < tokens.size(); ++i) {
            const KString& token = tokens[i];

            // Skip empty tokens (possible leading slash)
            if (token.empty()) continue;

            // Read the current directory cluster chain
            KVector<DirectoryEntry> entries = getDirectoryEntries(currentCluster);

            bool found                      = false;
            for (const auto& entry: entries) {
                if (entry.getNameLong() == token || entry.getNameShort() == token) {
                    // If this is the last token, return the entry
                    if (i == tokens.size() - 1) return entry;

                    if ((uint8_t) (entry.getAttributes() & EntryAttribute::Directory)) {
                        // If it's a directory, move to its cluster
                        currentCluster = entry.getFirstCluster();
                        found          = true;
                        break;
                    }

                    // Intermediate token is not a directory, path invalid
                    LOG_ERROR("Intermediate token is not a directory: %s", token.c_str());
                    return {};
                }
            }

            // Path not found
            if (!found) return {};
        }

        return {};
    }

    // Cluster Methods

    bool FAT32Partition::setNextCluster(uint32_t cluster, uint32_t nextCluster) {

        // Assert the cluster in within this partition
        if (cluster < 2 || cluster >= countClusters_) {
            kernelPanic("Attempt to set next cluster for an invalid cluster index! (%u)", cluster);
            return false;  // Error handling for invalid cluster index
        }

        // Calculate the cluster's sector and offset
        auto [fatSector, entryOffset] = calculateFATOffset(cluster);
        uint8_t sectorData[512];

        // read the containing sector (FAT)
        {
            bool diskStatus = diskDriver_.readSector(fatSector, sectorData, DEFAULT_TIMEOUT);
            if (!diskStatus) {
                LOG_ERROR("Failed to read sector: %u from disk.", fatSector);
                return false;
            }
        }

        // adjust the next cluster of the current cluster
        if (type_ == Type::FAT32) { *((uint32_t*) (sectorData + entryOffset)) = nextCluster & 0x0FFF'FFFF; }
        else if (type_ == Type::FAT16) { *((uint16_t*) (sectorData + entryOffset)) = nextCluster & 0xFFFF; }
        else if (type_ == Type::FAT12) {
            // IMPORTANT: Add FAT12 support
            // FAT12 uses 12-bit entries, which require special handling because they are not byte-aligned
            // This is a complex operation involving bit manipulation and should be implemented
            // For now, we log an error to prevent silent failures
            LOG_ERROR("FAT12 setNextCluster() is not yet implemented. Cluster chain update will fail.");
            return false;
        }
        else {
            // IMPORTANT: Add error handling for invalid FAT types
            // This should never happen if initialization is correct, but we should handle it gracefully
            LOG_ERROR("Invalid FAT type: %d in setNextCluster()", static_cast<int>(type_));
            return false;
        }

        // write the sector to the FAT again
        for (uint8_t fatNum = 0; fatNum < countFATs_; ++fatNum) {
            uint32_t targetSector = fatSector + (fatNum * fatSize_);
            bool diskStatus       = diskDriver_.writeSector(targetSector, sectorData, DEFAULT_TIMEOUT);
            if (!diskStatus) {
                LOG_ERROR("Failed to write FAT #%u sector: %u to disk.", fatNum, targetSector);
                return false;
            }
        }

        return true;
    }

    void FAT32Partition::freeClusterChain(uint32_t startCluster) {
        // Check if the cluster number is valid
        if (startCluster >= countClusters_) {
            kernelPanic("%s: Attempted to free an invalid cluster number: %u. Valid range is 0 to %u.", __PRETTY_FUNCTION__, startCluster, countClusters_ - 1);
        }

        uint32_t cluster = startCluster;
        while (cluster < 0x0FFFFFF8) {
            uint32_t nextCluster = getNextCluster(cluster);

            // Free the cluster
            if (!setNextCluster(cluster, 0)) { LOG_ERROR("Failed to free cluster %u", cluster); }

            // Check for invalid or end markers
            if (nextCluster == 0 || nextCluster > 0x0FFFFFF7) break;

            // Move to the next cluster in the chain
            cluster = nextCluster;
        }
    }

    std::optional<uint32_t> FAT32Partition::allocateCluster() {
        // TODO lock

        // This method will scan the FAT from the beginning of the data section to the end of the FAT32 volume.
        // We start from cluster 2 because clusters 0 and 1 are reserved for media descriptor and the FAT itself.
        for (uint32_t cluster = rootCluster_; cluster < countClusters_; ++cluster) {
            uint32_t nextCluster = getNextCluster(cluster);

            // Check if the cluster is free in the FAT (0x00000000 means free)
            if (nextCluster == 0x00000000) {
                // Mark this cluster as the end of the chain (0x0FFFFFFF indicates end of cluster chain in FAT32)
                if (!setNextCluster(cluster, 0x0FFFFFFF)) {
                    LOG_ERROR("Failed to allocate cluster %u", cluster);
                    return std::nullopt;
                }
                return cluster;
            }
        }

        // todo unlock

        LOG_ERROR("No free clusters available for allocation.");
        return std::nullopt;
    }

    bool FAT32Partition::writeCluster(uint32_t cluster, const KVector<uint8_t>& data) {
        if (data.size() > clusterSizeBytes_) {
            LOG_ERROR("Data size %u exceeds cluster size %u bytes", data.size(), clusterSizeBytes_);
            return false;
        }

        uint32_t sector = getSectorFromCluster(cluster);

        LOG_DEBUG("[writeCluster] Writing cluster %u (sector %u), data.size()=%u, clusterSizeBytes_=%u, clusterSize_=%u sectors",
                  cluster,
                  sector,
                  data.size(),
                  clusterSizeBytes_,
                  clusterSize_);

        // Show first 32 bytes being written
        if (data.size() >= 32) {
            LOG_DEBUG("[writeCluster] First 32 bytes: %02X %02X %02X %02X %02X %02X %02X %02X | %02X %02X %02X %02X [attr=%02X]",
                      data[0],
                      data[1],
                      data[2],
                      data[3],
                      data[4],
                      data[5],
                      data[6],
                      data[7],
                      data[8],
                      data[9],
                      data[10],
                      data[11],
                      data[11]);
        }

        for (uint8_t i = 0; i < clusterSize_; ++i) {
            uint32_t offset           = i * sectorSize_;
            const uint8_t* sectorData = (offset < data.size()) ? (data.data() + offset) : nullptr;

            if (offset >= data.size()) { LOG_ERROR("[writeCluster] ERROR: Trying to write sector %u but offset %u >= data.size() %u - READING GARBAGE!", i, offset, data.size()); }

            bool status = diskDriver_.writeSector(sector + i, data.data() + i * sectorSize_, DEFAULT_TIMEOUT);
            if (!status) {
                LOG_ERROR("Failed to write sector %u for cluster %u", sector + i, cluster);
                return false;
            }
        }

        return true;
    }

    // File Methods

    KVector<uint8_t> FAT32Partition::fetchDataFromEntry(const DirectoryEntry& entry, uint32_t offset, uint32_t countBytes) const {
        if (offset >= entry.getFileSize()) return {};

        // Limit the read operation within the file size bounds.
        uint32_t maxPossibleBytes = entry.getFileSize() - offset;
        if (countBytes > maxPossibleBytes) countBytes = maxPossibleBytes;

        KVector<uint8_t> fileData = readFile(entry.getFirstCluster(), entry.getFileSize());
        return {fileData.begin() + static_cast<KVector<uint8_t>::difference_type>(offset), fileData.begin() + static_cast<KVector<uint8_t>::difference_type>(offset + countBytes)};
    }

    KVector<uint8_t> FAT32Partition::read(const DirectoryEntry& entry, uint32_t offset, uint32_t countBytes) const { return fetchDataFromEntry(entry, offset, countBytes); }

    bool FAT32Partition::flushEntry(const DirectoryEntry& entry) {

        if ((uint8_t) (entry.getAttributes() & EntryAttribute::Invalid)) return false;

        // Get the FAT directory entry structure from the DirectoryEntry object
        fat_dentry dentry = entry.getFatDentry();
        LOG_WARN("[flushEntry] Writing entry with cluster %u, size %u at offset %u", (dentry.firstClusterHigh << 16) | dentry.firstClusterLow, dentry.fileSize, entry.getOffset());

        // Calculate the sector and offset in the directory where this entry is located
        uint32_t directoryCluster      = entry.getDirectoryCluster();
        uint32_t offsetInDirectory     = entry.getOffset();

        // Calculate the sector number within the cluster chain
        uint32_t sectorIndex           = offsetInDirectory / sectorSize_;
        uint32_t offsetWithinSector    = offsetInDirectory % sectorSize_;

        // Calculate the cluster and sector within the cluster chain
        uint32_t clusterIndex          = sectorIndex / clusterSize_;
        uint32_t sectorWithinCluster   = sectorIndex % clusterSize_;

        // Read the cluster chain to get the specific cluster
        KVector<uint32_t> clusterChain = readClusterChain(directoryCluster);

        // Invalid offset, cluster chain is shorter than expected
        if (clusterIndex >= clusterChain.size()) {
            LOG_ERROR("Invalid offset: cluster chain is shorter than expected.");
            return false;
        }

        uint32_t currentCluster = clusterChain[clusterIndex];
        uint32_t sector         = getSectorFromCluster(currentCluster) + sectorWithinCluster;

        // Read the sector containing the directory entry
        uint8_t sectorData[512];
        {
            bool diskStatus = diskDriver_.readSector(sector, sectorData, DEFAULT_TIMEOUT);
            if (!diskStatus) {
                LOG_ERROR("Failed to read sector %u from disk.", sector);
                return false;
            }
        }

        // Copy the FAT directory entry structure into the correct location within the sector data
        memcpy(sectorData + offsetWithinSector, &dentry, sizeof(fat_dentry));

        // Write the modified sector data back to the disk
        {
            bool diskStatus = diskDriver_.writeSector(sector, sectorData, DEFAULT_TIMEOUT);
            if (!diskStatus) {
                LOG_ERROR("Failed to write sector %u to disk.", sector);
                return false;
            }
        }

        return true;
    }

    bool FAT32Partition::append(DirectoryEntry& entry, const KVector<uint8_t>& bytes) {
        uint32_t fileSize              = entry.getFileSize();
        uint32_t lastClusterOffset     = fileSize % clusterSizeBytes_;
        uint32_t remainingBytes        = bytes.size();
        uint32_t writePosition         = 0;

        // Read the current cluster chain of the file
        KVector<uint32_t> clusterChain = readClusterChain(entry.getFirstCluster());
        uint32_t lastCluster           = clusterChain.empty() ? 0 : clusterChain.back();

        // IMPORTANT: Only write to existing space in the last cluster if:
        // 1. File is NOT empty (fileSize > 0) - empty files have no existing space
        // 2. AND the last cluster is not full (lastClusterOffset != 0) - can fit more data
        // 3. AND there is an existing cluster chain - must have at least one cluster
        //
        // The original condition was: (fileSize == 0 || lastClusterOffset != 0) && !clusterChain.empty()
        // This was WRONG because if fileSize == 0, the cluster chain should be empty,
        // making the condition contradictory and causing logic errors.
        if (fileSize > 0 && lastClusterOffset != 0 && !clusterChain.empty()) {
            uint32_t bytesToWrite        = std::min(clusterSizeBytes_ - lastClusterOffset, remainingBytes);
            KVector<uint8_t> clusterData = readFile(lastCluster, clusterSizeBytes_);

            // Copy new bytes to the correct position in the last cluster data
            std::copy(bytes.begin(),
                      bytes.begin() + static_cast<KVector<uint8_t>::difference_type>(bytesToWrite),
                      clusterData.begin() + static_cast<KVector<uint8_t>::difference_type>(lastClusterOffset));

            // Write back the modified cluster
            bool status = writeCluster(lastCluster, clusterData);
            if (!status) {
                LOG_ERROR("Failed to write the last cluster %u", lastCluster);
                return false;
            }

            remainingBytes -= bytesToWrite;
            writePosition += bytesToWrite;
        }

        // Allocate and write to new clusters if needed
        while (remainingBytes > 0) {
            auto newClusterOpt = allocateCluster();

            // Unable to allocate new cluster during append operation.
            if (!newClusterOpt) {
                LOG_ERROR("Failed to allocate new cluster during append operation.");
                return false;
            }

            uint32_t newCluster = newClusterOpt.value();
            if (lastCluster == 0) {
                // This is the first cluster being allocated for this file
                lastCluster = newCluster;
                if (entry.getFirstCluster() == 0) {
                    // Update the first cluster in the directory entry if it was previously empty
                    LOG_DEBUG("[append] Setting first cluster from %u to %u", entry.getFirstCluster(), newCluster);
                    entry.setClusterChain(newCluster);

                    // Flush changes back to the directory
                    LOG_DEBUG("[append] Flushing entry at offset %u in directory cluster %u", entry.getOffset(), entry.getDirectoryCluster());
                    bool status = flushEntry(entry);
                    if (!status) {
                        LOG_ERROR("Failed to flush entry after setting first cluster to %u", newCluster);
                        return false;
                    }
                    LOG_DEBUG("[append] Entry flushed successfully, cluster is now %u", entry.getFirstCluster());
                }
            }
            else {
                // Link the new cluster to the last one in the chain
                bool status = setNextCluster(lastCluster, newCluster);
                // TODO check and log + early exit here
            }

            lastCluster = newCluster;
            clusterChain.push_back(newCluster);

            uint32_t bytesToWrite = std::min(clusterSizeBytes_, remainingBytes);
            KVector<uint8_t> clusterData(bytes.begin() + static_cast<KVector<uint8_t>::difference_type>(writePosition),
                                         bytes.begin() + static_cast<KVector<uint8_t>::difference_type>(writePosition + bytesToWrite));

            // If the last cluster is not fully utilized, fill it with zeros
            if (clusterData.size() < clusterSizeBytes_) clusterData.resize(clusterSizeBytes_, 0);

            bool status = writeCluster(newCluster, clusterData);
            if (!status) {
                LOG_ERROR("Failed to write new cluster %u", newCluster);
                return false;
            }


            remainingBytes -= bytesToWrite;
            writePosition += bytesToWrite;
        }

        // Update the file size in the directory entry
        entry.setFileSize(fileSize + bytes.size());

        // Write the directory entry back to disk
        bool status = flushEntry(entry);
        if (!status) {
            LOG_ERROR("Failed to flush entry after appending data.");
            return false;
        }

        return true;
    }

    bool FAT32Partition::write(DirectoryEntry& entry, const KVector<uint8_t>& bytes) {
        // IMPORTANT: Do NOT allocate a cluster here. Let append() allocate the first cluster.
        // Reason: If we allocate here and append() also allocates, we waste a cluster (disk space leak).
        // This was causing disk fragmentation and wasted sectors.

        // Step 1: Free existing cluster chain (if any)
        // This ensures no orphaned clusters are left behind from previous writes
        if (entry.getFirstCluster() > 2) freeClusterChain(entry.getFirstCluster());

        // Step 2: Reset file metadata to prepare for new write
        // Set file size to zero to indicate empty file
        entry.setFileSize(0);

        // Clear the cluster chain pointer (set to 0) so append() will allocate the first cluster
        // This prevents the double-allocation bug
        entry.setClusterChain(0);

        // Step 3: Append new data (append() will handle first cluster allocation)
        {
            bool status = append(entry, bytes);
            if (!status) {
                LOG_ERROR("Failed to append new data during write operation.");
                return false;
            }
        }

        // Step 4: Update the directory entry to reflect all changes made
        {
            bool status = flushEntry(entry);
            if (!status) {
                LOG_ERROR("Failed to flush entry after write operation.");
                return false;
            }
        }

        return true;
    }

    FAT32Partition::Type FAT32Partition::getType() { return type_; }

    bool FAT32Partition::isValidSFNCharacter(char c) {
        static const char invalidChars[] = "\"*/:<>?\\|+,;=[]";
        return (isalnum(static_cast<unsigned char>(c)) || strchr("$%'-_@~`!(){}^#&", c)) && !strchr(invalidChars, c);
    }

    KString FAT32Partition::generateUniqueShortName(const KString& longName, const KVector<KString>& existingShortNames) {
        KString upperName(longName.c_str());
        upperName.toUpper();

        // Remove invalid characters and replace with underscores
        // CRITICAL: Do NOT replace the dot (.) as it's the extension separator!
        for (char& c: upperName) {
            if (c != '.' && !isValidSFNCharacter(c)) c = '_';
        }

        // Strip leading periods per FAT32 spec line 1257
        while (!upperName.empty() && upperName[0] == '.') { upperName = upperName.substr(1); }

        // Extract base name and extension
        size_t dotPos     = upperName.find_last_of(".");
        KString baseName  = (dotPos == KString::npos) ? upperName : upperName.substr(0, dotPos);
        KString extension = (dotPos == KString::npos) ? KString("") : upperName.substr(dotPos + 1);

        // Remove spaces and invalid characters
        baseName.erase(std::remove_if(baseName.begin(), baseName.end(), [](char c) { return c == ' '; }), baseName.end());
        extension.erase(std::remove_if(extension.begin(), extension.end(), [](char c) { return c == ' '; }), extension.end());

        // Truncate extension to 3 characters
        extension      = extension.substr(0, 3);

        // Per FAT32 spec line 1275-1281: If truncation/lossy conversion occurs, add numeric tail
        // Check if baseName needs truncation (> 8 chars) - if so, MUST add numeric tail
        bool needsTail = (baseName.size() > 8);

        // If no tail needed, try the plain truncated name first
        if (!needsTail) {
            baseName    = baseName.substr(0, 8);
            KString sfn = baseName;
            if (!extension.empty()) {
                sfn += '.';
                sfn += extension;
            }

            if (std::find(existingShortNames.begin(), existingShortNames.end(), sfn) == existingShortNames.end()) { return sfn; }
        }

        // Handle name collisions using tilde notation
        for (uint32_t num = 1; num <= 999999; ++num) {
            // Build decimal digits for num
            char digits[10];
            size_t digitCount = 0;
            uint32_t temp     = num;
            do {
                digits[digitCount++] = (char) ('0' + (temp % 10));
                temp /= 10;
            } while (temp > 0 && digitCount < 9);

            // Compute tail length: '~' + digits
            const size_t tailLen = 1 + digitCount;
            size_t baseLen       = 8 > tailLen ? (8 - tailLen) : 0;  // ensure total base+tail <= 8

            // Slice base according to remaining space (Windows uses 6 when tailLen=2)
            if (baseLen > baseName.size()) baseLen = baseName.size();

            // Build SFN base: first baseLen chars + '~' + digits in correct order
            KString sfnBaseNum = baseName.substr(0, baseLen);
            sfnBaseNum += '~';
            for (size_t i = 0; i < digitCount; ++i) sfnBaseNum += digits[digitCount - 1 - i];

            // Compose full SFN with extension
            KString sfnNum = sfnBaseNum;
            if (!extension.empty()) {
                sfnNum += '.';
                sfnNum += extension;
            }

            if (std::find(existingShortNames.begin(), existingShortNames.end(), sfnNum) == existingShortNames.end()) { return sfnNum; }
        }

        // If all attempts fail, return an empty string
        return KString("");
    }

    uint8_t FAT32Partition::calculateShortNameChecksum(const char* shortName) {
        uint8_t sum = 0;
        for (int i = 0; i < 11; ++i) { sum = ((sum & 1) ? 0x80 : 0) + (sum >> 1) + static_cast<uint8_t>(shortName[i]); }
        return sum;
    }

    bool FAT32Partition::needsLFN(const KString& fileName) {
        // Per FAT32 spec: LFN needed if name doesn't fit 8.3, has special chars, or HAS LOWERCASE
        // Spec line 1197: "Long names... are not converted to upper case and their original case value is preserved"
        // If original name has any lowercase, we MUST use LFN to preserve it

        // Extract base name and extension from ORIGINAL (case-preserved) fileName
        size_t dotPos     = fileName.find_last_of(".");
        KString baseName  = (dotPos == KString::npos) ? fileName : fileName.substr(0, dotPos);
        KString extension = (dotPos == KString::npos) ? KString("") : fileName.substr(dotPos + 1);

        // Check lengths - if too long, need LFN
        if (baseName.size() > 8 || extension.size() > 3) return true;

        // Check for lowercase - if ANY lowercase exists, need LFN to preserve case
        for (char c: baseName) {
            if (islower(static_cast<unsigned char>(c))) return true;
        }
        for (char c: extension) {
            if (islower(static_cast<unsigned char>(c))) return true;
        }

        // Convert to uppercase for validation checks
        KString upperBase(baseName.c_str());
        KString upperExt(extension.c_str());
        upperBase.toUpper();
        upperExt.toUpper();

        // Check for invalid SFN characters
        for (char c: upperBase) {
            if (!isValidSFNCharacter(c)) return true;
        }
        for (char c: upperExt) {
            if (!isValidSFNCharacter(c)) return true;
        }

        return false;
    }

    KVector<fat_dentry> FAT32Partition::createLFNEntries(const KString& longName, uint8_t checksum) {
        // Break the longName into chunks of 13 UTF-16 characters
        KVector<fat_dentry> lfnEntries;

        const char* nameStr = longName.c_str();
        size_t actualLen    = longName.size();

        // Debug: Log input string byte-by-byte
        LOG_DEBUG("[LFN-INPUT] longName='%s', actualLen=%zu", nameStr, actualLen);
        for (size_t i = 0; i < actualLen; ++i) {
            LOG_DEBUG("[LFN-INPUT] byte[%zu] = 0x%02X ('%c')", i, (uint8_t) nameStr[i], (nameStr[i] >= 32 && nameStr[i] < 127) ? nameStr[i] : '.');
        }

        // Convert only the actual filename characters (not including null terminator)
        // Manually convert UTF-8 to UTF-16LE without null terminator
        KWString unicodeName;
        size_t i = 0;
        LOG_DEBUG("[CONV-START] Starting UTF-8 to UTF-16 conversion, actualLen=%zu", actualLen);
        while (i < actualLen) {
            auto utf8_char = static_cast<unsigned char>(nameStr[i]);
            LOG_DEBUG("[CONV-LOOP] i=%zu, utf8_char=0x%02X, actualLen=%zu", i, utf8_char, actualLen);
            uint32_t codepoint = 0;
            size_t extra_bytes = 0;

            if (utf8_char <= 0x7F) {
                codepoint   = utf8_char;
                extra_bytes = 0;
                LOG_DEBUG("[CONV-ASCII] char=0x%02X -> codepoint=0x%04X", utf8_char, codepoint);
            }
            else if ((utf8_char & 0xE0) == 0xC0) {
                codepoint   = utf8_char & 0x1F;
                extra_bytes = 1;
            }
            else if ((utf8_char & 0xF0) == 0xE0) {
                codepoint   = utf8_char & 0x0F;
                extra_bytes = 2;
            }
            else if ((utf8_char & 0xF8) == 0xF0) {
                codepoint   = utf8_char & 0x07;
                extra_bytes = 3;
            }
            else {
                LOG_DEBUG("[CONV-SKIP] Invalid UTF-8 byte at i=%zu, skipping", i);
                ++i;
                continue;
            }

            if (i + extra_bytes >= actualLen) {
                LOG_WARN("[CONV-INCOMPLETE] i=%zu + extra_bytes=%zu >= actualLen=%zu, breaking", i, extra_bytes, actualLen);
                break;
            }

            for (size_t j = 1; j <= extra_bytes; ++j) {
                auto cc = static_cast<unsigned char>(nameStr[i + j]);
                if ((cc & 0xC0) != 0x80) {
                    i += j;
                    break;
                }
                codepoint = (codepoint << 6) | (cc & 0x3F);
            }

            i += extra_bytes + 1;
            LOG_DEBUG("[CONV-PUSH] Adding codepoint=0x%04X to vector (now size=%zu)", codepoint, unicodeName.size());

            if (codepoint <= 0xFFFF) {
                if (codepoint >= 0xD800 && codepoint <= 0xDFFF) { continue; }
                unicodeName.push_back(static_cast<uint16_t>(codepoint));
            }
            else if (codepoint <= 0x10FFFF) {
                codepoint -= 0x10000;
                uint16_t high_surrogate = 0xD800 | ((codepoint >> 10) & 0x3FF);
                uint16_t low_surrogate  = 0xDC00 | (codepoint & 0x3FF);
                unicodeName.push_back(high_surrogate);
                unicodeName.push_back(low_surrogate);
            }
        }
        LOG_DEBUG("[CONV-END] Conversion complete, unicodeName.size()=%zu", unicodeName.size());

        // Use unicodeName.size() directly - it now excludes the null terminator
        size_t actualUniChars = unicodeName.size();
        LOG_DEBUG("[ACTUAL-CHARS] Actual UTF-16 characters: %zu", actualUniChars);

        // Debug: Log converted UTF-16 string
        LOG_DEBUG("[LFN-UTF16] FINAL unicodeName actual chars=%zu", actualUniChars);
        for (size_t i = 0; i < actualUniChars; ++i) { LOG_DEBUG("[LFN-UTF16] FINAL char[%zu] = 0x%04X", i, unicodeName[i]); }

        // Calculate the number of LFN entries needed
        // Use actualUniChars (excluding null terminator) to calculate LFN entries
        size_t totalEntries = (actualUniChars + 12) / 13;  // Each LFN entry can hold 13 UTF-16 characters
        LOG_DEBUG("[LFN-CALC] actualUniChars=%zu, totalEntries needed=%zu", actualUniChars, totalEntries);

        for (size_t i = 0; i < totalEntries; ++i) {
            fat_dentry lfnEntry{};
            memset(&lfnEntry, 0, sizeof(lfnEntry));

            // Prepare a byte-wise view to fill LFN name fields by absolute offsets
            uint8_t* entryBytes = reinterpret_cast<uint8_t*>(&lfnEntry);

            // IMPORTANT: Initialize name fields to 0xFFFF per VFAT spec
            // Name1 (bytes 1-10): 5 UTF-16 chars
            for (size_t j = 0; j < 5; ++j) {
                entryBytes[1 + j * 2]     = 0xFF;
                entryBytes[1 + j * 2 + 1] = 0xFF;
            }
            // Name2 (bytes 14-25): 6 UTF-16 chars
            for (size_t j = 0; j < 6; ++j) {
                entryBytes[14 + j * 2]     = 0xFF;
                entryBytes[14 + j * 2 + 1] = 0xFF;
            }
            // Name3 (bytes 28-31): 2 UTF-16 chars
            for (size_t j = 0; j < 2; ++j) {
                entryBytes[28 + j * 2]     = 0xFF;
                entryBytes[28 + j * 2 + 1] = 0xFF;
            }

            // Set sequence number (n..1) where (n | 0x40) denotes the LAST logical LFN entry
            // Per FAT32 spec: First entry written to disk must have 0x40 set (last entry in LFN set)
            uint8_t seqNum = static_cast<uint8_t>(totalEntries - i);
            if (i == 0) seqNum |= 0x40;          // Mark as last entry in LFN set (first entry Windows reads)
            entryBytes[0]             = seqNum;  // LDIR_Ord

            // Set attribute (LFN), type (0), checksum, and reserved cluster (0)
            // Per FAT32 spec line 1007: LDIR_Attr must be ATTR_LONG_NAME (0x0F)
            // Bits 6-7 are reserved and should be 0, so we use 0x0F not 0xFF
            entryBytes[11]            = 0x0F;      // LDIR_Attr (ReadOnly|Hidden|System|VolumeID)
            entryBytes[12]            = 0x00;      // LDIR_Type (must be 0 for LFN entries)
            entryBytes[13]            = checksum;  // LDIR_Chksum (checksum of associated short name)
            // Per FAT32 spec line 1012: LDIR_FstClusLO must be zero (bytes 26-27)
            // Note: firstClusterHigh (bytes 20-21) should also be zero for LFN entries
            lfnEntry.firstClusterLow  = 0x0000;  // LDIR_FstClusLO (must be zero per spec line 1012)
            lfnEntry.firstClusterHigh = 0x0000;  // Also ensure high word is zero (not strictly required but safer)

            // Copy up to 13 UTF-16LE code units for this entry
            size_t charIndex          = (totalEntries - 1 - i) * 13;

            auto writeChar            = [&](size_t fieldOffset, size_t slotIdx, uint16_t value) {
                // fieldOffset: start offset of the field in the entry (1, 14, or 28)
                // slotIdx: 0-based index inside the field
                size_t byteOffset          = fieldOffset + slotIdx * 2;
                entryBytes[byteOffset]     = static_cast<uint8_t>(value & 0xFF);
                entryBytes[byteOffset + 1] = static_cast<uint8_t>((value >> 8) & 0xFF);
            };

            // Write exactly 13 UTF-16 code units for this entry with a single 0x0000 terminator
            // and 0xFFFF padding thereafter. Map positions 0..4 -> Name1, 5..10 -> Name2, 11..12 -> Name3.
            bool terminated = false;
            for (size_t j = 0; j < 13; ++j) {
                size_t globalIdx = charIndex + j;
                uint16_t value;
                if (!terminated && globalIdx < actualUniChars) {
                    value = unicodeName[globalIdx];
                    LOG_DEBUG("[LFN-CHAR] Entry %zu, slot %zu: globalIdx=%zu < actualUniChars=%zu, writing char 0x%04X", i, j, globalIdx, actualUniChars, value);
                }
                else if (!terminated && globalIdx == actualUniChars) {
                    value      = 0x0000;  // write exactly one terminator
                    terminated = true;
                    LOG_DEBUG("[LFN-TERM] Entry %zu, slot %zu: globalIdx=%zu == actualUniChars=%zu, writing terminator 0x0000", i, j, globalIdx, actualUniChars);
                }
                else {
                    value = 0xFFFF;  // padding
                    LOG_DEBUG("[LFN-PAD] Entry %zu, slot %zu: globalIdx=%zu, terminated=%d, writing padding 0xFFFF", i, j, globalIdx, terminated ? 1 : 0);
                }

                if (j < 5) writeChar(1, j, value);
                else if (j < 11) writeChar(14, j - 5, value);
                else /* j = 11..12 */ writeChar(28, j - 11, value);
            }

            lfnEntries.push_back(lfnEntry);

            // Debug: Log what we're writing
            LOG_DEBUG("[LFN] Entry %zu: seqNum=0x%02X, attr=0x%02X, checksum=0x%02X, charIndex=%zu", i, seqNum, entryBytes[11], checksum, charIndex);
        }

        LOG_DEBUG("[LFN] Created %zu LFN entries for '%s'", lfnEntries.size(), longName.c_str());
        return lfnEntries;
    }

    std::optional<DirectoryEntry> FAT32Partition::createFile(DirectoryEntry& directoryEntry, const KString& fileName, EntryAttribute attributes) {

        // Check if the directory entry is indeed a directory
        if (!(uint8_t) (directoryEntry.getAttributes() & EntryAttribute::Directory)) {
            LOG_ERROR("The specified entry is not a directory.");
            return std::nullopt;
        }

        // Read the directory entries in the specified directory
        KVector<DirectoryEntry> entries = getDirectoryEntries(directoryEntry.getFirstCluster());

        // Collect existing short names (in dotted 8.3 form) and check for existing file
        KVector<KString> existingShortNames;
        for (const auto& entry: entries) {
            if (entry.getNameLong() == fileName) {
                LOG_ERROR("A file with the same name already exists in the directory.");
                return std::nullopt;
            }

            KString raw11 = entry.getNameShort();               // 11-byte raw SFN
            KString base  = KString(raw11.c_str(), 8).strip();  // trim spaces
            KString ext   = KString(raw11.c_str() + 8, 3).strip();
            base.toUpper();
            ext.toUpper();
            KString dotted = base;
            if (!ext.empty()) {
                dotted += ".";
                dotted += ext;
            }
            existingShortNames.push_back(dotted);
        }

        // Generate unique short name (SFN)
        KString sfn = generateUniqueShortName(fileName, existingShortNames);
        if (sfn.empty()) {
            LOG_ERROR("Failed to generate a unique short name for the file.");
            return std::nullopt;
        }

        LOG_DEBUG("[createFile] generateUniqueShortName('%s') returned: '%s'", fileName.c_str(), sfn.c_str());


        // Create the main directory entry
        fat_dentry newDentry{};
        memset(&newDentry, 0, sizeof(newDentry));  // Initialize with zeros per spec
        memset(newDentry.shortName, ' ', 11);      // Only shortName gets spaces

        // Set the short name (8.3 format), pad with spaces
        size_t dotPos     = sfn.find('.');
        KString baseName  = (dotPos == KString::npos) ? sfn : sfn.substr(0, dotPos);
        KString extension = (dotPos == KString::npos) ? KString("") : sfn.substr(dotPos + 1);

        baseName.toUpper();
        extension.toUpper();

        // Pad baseName and extension with spaces to fit 8 and 3 characters
        char baseNamePadded[8];
        char extensionPadded[3];

        size_t baseNameLen  = std::min<size_t>(8, baseName.size());
        size_t extensionLen = std::min<size_t>(3, extension.size());

        // Initialize with spaces
        memset(baseNamePadded, ' ', 8);
        memset(extensionPadded, ' ', 3);

        // IMPORTANT: Copy the baseName and extension into the padded arrays
        // Do NOT use memcpy with c_str() because it may copy null terminators!
        // Per FAT32 spec: short names MUST be padded with spaces (0x20), not nulls (0x00)
        for (size_t i = 0; i < baseNameLen; ++i) {
            char c = baseName.c_str()[i];
            if (c != '\0')  // Stop at null terminator
            {
                baseNamePadded[i] = c;
            }
        }
        for (size_t i = 0; i < extensionLen; ++i) {
            char c = extension.c_str()[i];
            if (c != '\0')  // Stop at null terminator
            {
                extensionPadded[i] = c;
            }
        }

        // Copy baseNamePadded and extensionPadded into shortName
        memcpy(newDentry.shortName, baseNamePadded, 8);
        memcpy(newDentry.shortName + 8, extensionPadded, 3);

        // IMPORTANT: Calculate checksum from the FINAL padded 11-byte short name
        // The checksum must match exactly what's written to disk, not the raw string
        // This ensures LFN entries will match the directory entry when read back
        uint8_t checksum = calculateShortNameChecksum(reinterpret_cast<const char*>(newDentry.shortName));
        LOG_DEBUG("[SFN] Full 11-byte name: [%02X %02X %02X %02X %02X %02X %02X %02X . %02X %02X %02X], checksum: 0x%02X",
                  (uint8_t) newDentry.shortName[0],
                  (uint8_t) newDentry.shortName[1],
                  (uint8_t) newDentry.shortName[2],
                  (uint8_t) newDentry.shortName[3],
                  (uint8_t) newDentry.shortName[4],
                  (uint8_t) newDentry.shortName[5],
                  (uint8_t) newDentry.shortName[6],
                  (uint8_t) newDentry.shortName[7],
                  (uint8_t) newDentry.shortName[8],
                  (uint8_t) newDentry.shortName[9],
                  (uint8_t) newDentry.shortName[10],
                  checksum);
        LOG_DEBUG("[SFN] As string: '%.8s.%.3s'", newDentry.shortName, newDentry.shortName + 8);

        // Prepare LFN entries if needed
        KVector<fat_dentry> lfnEntries;
        if (needsLFN(fileName)) { lfnEntries = createLFNEntries(fileName, checksum); }

        // Set attributes and initial values
        newDentry.attribute               = static_cast<uint8_t>(attributes);
        newDentry.firstClusterLow         = 0;  // Initially zero, allocate when writing data
        newDentry.firstClusterHigh        = 0;
        newDentry.fileSize                = 0;

        // IMPORTANT: Per FAT32 spec, dates/times CANNOT be zero!
        // Windows rejects entries with zero dates as corrupted.
        // Date format: bits 0-4=day(1-31), bits 5-8=month(1-12), bits 9-15=year(since 1980)
        // Time format: bits 0-4=2sec(0-29), bits 5-10=min(0-59), bits 11-15=hour(0-23)
        // Using 2020-01-01 12:00:00 (40 years since 1980)
        uint16_t defaultDate              = (40 << 9) | (1 << 5) | 1;   // 2020-01-01
        uint16_t defaultTime              = (12 << 11) | (0 << 5) | 0;  // 12:00:00

        // Set time and date fields (could set to zero or get current time)
        newDentry.creationTimeTenth       = 0;
        newDentry.creationTime            = defaultTime;
        newDentry.creationDate            = defaultDate;
        newDentry.lastAccessDate          = defaultDate;
        newDentry.writeTime               = defaultTime;
        newDentry.writeDate               = defaultDate;

        // CRITICAL: Set NT reserved field for case preservation
        // Bit 3 (0x08) = lowercase base name
        // Bit 4 (0x10) = lowercase extension
        // Since we're using LFN, we should set this to 0
        newDentry.ntRes                   = 0x00;

        // Now, find free entries in the directory data
        // Need totalEntries = lfnEntries.size() + 1

        // IMPORTANT: Read only the actual directory size from its cluster chain.
        // Using an arbitrary large buffer (e.g., 10MB) causes incorrect insert offsets
        // which later produce invalid on-disk positions when flushing the entry.
        KVector<uint32_t> dirClusterChain = readClusterChain(directoryEntry.getFirstCluster());
        uint32_t actualDirBytes           = dirClusterChain.size() * clusterSize_ * sectorSize_;
        // Add a sane upper bound to avoid excessive allocations in damaged directories
        constexpr uint32_t MAX_DIR_SIZE   = 100 * 1024 * 1024;
        uint32_t dirReadSize              = std::min(actualDirBytes, MAX_DIR_SIZE);
        if (dirReadSize == 0) {
            // Directory with no readable clusters yet; operate on a single-cluster buffer
            dirReadSize = clusterSize_ * sectorSize_;
        }
        KVector<uint8_t> dirData  = readFile(directoryEntry.getFirstCluster(), dirReadSize);

        size_t totalEntriesNeeded = lfnEntries.size() + 1;
        size_t dirEntrySize       = sizeof(fat_dentry);
        size_t dirEntriesCount    = dirData.size() / dirEntrySize;

        // Find consecutive free entries
        size_t insertPos          = 0;
        bool foundFreeEntries     = false;

        for (size_t i = 0; i <= dirEntriesCount; ++i) {
            bool entriesFree = true;
            for (size_t j = 0; j < totalEntriesNeeded; ++j) {
                size_t offset = (i + j) * dirEntrySize;
                if (offset >= dirData.size()) {
                    // Reached the end, need to extend dirData
                    entriesFree = true;
                    break;
                }
                uint8_t firstByte = dirData[offset];
                if (firstByte != 0x00 && firstByte != 0xE5) {
                    entriesFree = false;
                    break;
                }
            }
            if (entriesFree) {
                insertPos        = i * dirEntrySize;
                foundFreeEntries = true;
                break;
            }
        }

        // Ensure the buffer can hold the entries at insert position
        size_t requiredSize = insertPos + totalEntriesNeeded * dirEntrySize;
        if (dirData.size() < requiredSize) {
            // Extend the directory data to accommodate new entries
            dirData.resize(requiredSize, 0);
        }

        // Now, insert the LFN entries and main entry into dirData at insertPos

        // Copy LFN entries and main entry into dirData in order

        // Write LFN entries in on-disk order: (n|0x40), n-1, ..., 1

        size_t entryOffset = insertPos;

        for (size_t i = 0; i < lfnEntries.size(); ++i) {
            memcpy(dirData.data() + entryOffset, &lfnEntries[i], dirEntrySize);
            entryOffset += dirEntrySize;
        }

        // Copy the main directory entry
        LOG_WARN("[createFile] Writing main entry at offset %u: cluster=%u, size=%u, attr=0x%02X",
                 entryOffset,
                 (newDentry.firstClusterHigh << 16) | newDentry.firstClusterLow,
                 newDentry.fileSize,
                 newDentry.attribute);
        memcpy(dirData.data() + entryOffset, &newDentry, dirEntrySize);
        entryOffset += dirEntrySize;

        // IMPORTANT: Ensure directory is properly terminated after new entries
        // After writing our entries, the next entry should be marked as end (0x00) if we're at the end
        // or if we extended the directory. This prevents directory parsing from reading garbage.
        if (entryOffset >= dirData.size()) {
            // We extended the directory - the resize() already zeroed it, but ensure end marker
            // This is redundant but safe since resize() zeros new memory
        }
        else if (entryOffset < dirData.size()) {
            // Check if next entry should be marked as end
            // Only mark as end if it was previously free (0x00 or 0xE5) and we're replacing it
            // If it's a valid entry, leave it alone
            uint8_t nextEntryFirstByte = dirData[entryOffset];
            if (nextEntryFirstByte == 0x00 || nextEntryFirstByte == 0xE5) {
                // Was free, ensure it stays as end marker
                dirData[entryOffset] = 0x00;
            }
        }

        // Now, update the directory clusters and write back to disk

        // Calculate the number of clusters needed to store the directory data
        size_t clusterSizeBytes        = clusterSize_ * sectorSize_;
        size_t totalClusters           = (dirData.size() + clusterSizeBytes - 1) / clusterSizeBytes;

        // Read the existing cluster chain of the directory
        KVector<uint32_t> clusterChain = readClusterChain(directoryEntry.getFirstCluster());

        // Allocate additional clusters if necessary
        while (clusterChain.size() < totalClusters) {
            auto additionalClusterOpt = allocateCluster();
            if (!additionalClusterOpt) {
                LOG_ERROR("Failed to allocate additional cluster for the directory.");
                return std::nullopt;
            }
            uint32_t additionalCluster = additionalClusterOpt.value();
            if (!clusterChain.empty()) { setNextCluster(clusterChain.back(), additionalCluster); }
            else {
                // This should not happen, but in case the directory has no clusters
                directoryEntry.setClusterChain(additionalCluster);
                flushEntry(directoryEntry);
            }
            clusterChain.push_back(additionalCluster);
        }

        // Mark the end of the cluster chain
        if (!clusterChain.empty()) {
            setNextCluster(clusterChain.back(), 0x0FFFFFFF);  // End of chain marker
        }

        // ========== DIAGNOSTIC LOGGING FOR DEBUG ==========
        // Calculate mainEntryOffset here since it's used before it's declared below
        uint32_t mainEntryOffset = static_cast<uint32_t>(insertPos + (lfnEntries.size() * dirEntrySize));

        LOG_WARN("[createFile] === DIAGNOSTIC: Directory Write Debug ===");
        LOG_WARN("[createFile] dirData.size() = %u bytes (%u entries)", dirData.size(), dirData.size() / 32);
        LOG_WARN("[createFile] insertPos = %u, totalEntriesNeeded = %u", insertPos, totalEntriesNeeded);
        LOG_WARN("[createFile] mainEntryOffset = %u", mainEntryOffset);
        LOG_WARN("[createFile] clusterSizeBytes = %u, totalClusters = %u", clusterSizeBytes, totalClusters);

        auto hexDigit  = [](uint8_t value) -> char { return (value < 10) ? static_cast<char>('0' + value) : static_cast<char>('A' + (value - 10)); };

        auto dumpBytes = [&](const char* label, size_t offset, size_t length) {
            if (offset >= dirData.size()) {
                LOG_WARN("[createFile] %s offset %u out of range (dirData.size=%u)", label, offset, dirData.size());
                return;
            }
            size_t end = std::min(dirData.size(), offset + length);
            for (size_t pos = offset; pos < end; pos += 16) {
                size_t chunk = std::min<size_t>(16, end - pos);
                char hexLine[(3 * 16) + 1];
                size_t index = 0;
                for (size_t j = 0; j < chunk && index + 2 < sizeof(hexLine); ++j) {
                    if (j > 0 && index + 3 < sizeof(hexLine)) { hexLine[index++] = ' '; }
                    uint8_t value    = dirData[pos + j];
                    hexLine[index++] = hexDigit(static_cast<uint8_t>(value >> 4));
                    hexLine[index++] = hexDigit(static_cast<uint8_t>(value & 0x0F));
                }
                hexLine[std::min(index, sizeof(hexLine) - 1)] = '\0';
                LOG_WARN("[createFile] %s offset %u (+%zu): %s", label, offset, pos - offset, hexLine);
            }
        };

        // Dump first 128 bytes (4 entries) of dirData to see what's being written
        LOG_WARN("[createFile] === Directory Data Dump (first 4 entries) ===");
        for (size_t i = 0; i < std::min(dirData.size(), size_t(128)); i += 32) { dumpBytes("Entry", i, 32); }

        // Dump the entries we are about to append
        dumpBytes("LFN start", insertPos, 32);
        dumpBytes("LFN next", insertPos + 32, 32);
        dumpBytes("Main entry", mainEntryOffset, 32);
        dumpBytes("Terminator", mainEntryOffset + 32, 32);

        // CRITICAL: Verify the actual LFN order on disk
        LOG_WARN("[createFile] === LFN Order Verification ===");
        for (size_t idx = 0; idx < lfnEntries.size(); ++idx) {
            size_t offset = insertPos + (idx * dirEntrySize);
            if (offset < dirData.size()) {
                LOG_WARN("[createFile] LFN[%u] at offset %u: seq=%02X, attr=%02X, checksum=%02X", idx, offset, dirData[offset], dirData[offset + 11], dirData[offset + 13]);
            }
        }
        LOG_WARN("[createFile] Main entry at offset %u: name[0]=%02X, attr=%02X", mainEntryOffset, dirData[mainEntryOffset], dirData[mainEntryOffset + 11]);

        // Show LFN entry details
        LOG_WARN("[createFile] === LFN Entries Details ===");
        for (size_t i = 0; i < lfnEntries.size(); ++i) {
            const fat_dentry& lfn = lfnEntries[i];
            LOG_WARN("[createFile] LFN[%u]: seq=%02X, attr=%02X, ntRes=%02X, chksum=%02X, fstClusLo=%04X",
                     i,
                     (uint8_t) lfn.shortName[0],
                     lfn.attribute,
                     lfn.ntRes,
                     lfn.creationTimeTenth,
                     lfn.firstClusterLow);
        }

        // Show main entry details
        LOG_WARN("[createFile] === Main Entry Details ===");
        LOG_WARN("[createFile] Main: name='%.11s', attr=%02X, size=%u, cluster=%u",
                 newDentry.shortName,
                 newDentry.attribute,
                 newDentry.fileSize,
                 ((uint32_t) newDentry.firstClusterHigh << 16) | newDentry.firstClusterLow);
        LOG_WARN("[createFile] === END DIAGNOSTIC ===");
        // ========== END DIAGNOSTIC LOGGING ==========

        // Write the updated directory data back to disk
        for (size_t i = 0; i < dirData.size(); i += clusterSizeBytes) {
            uint32_t cluster = clusterChain[i / clusterSizeBytes];
            size_t dataSize  = std::min(clusterSizeBytes, dirData.size() - i);
            KVector<uint8_t> clusterData(dirData.begin() + i, dirData.begin() + i + dataSize);
            if (clusterData.size() < clusterSizeBytes) {
                // Pad with zeros
                clusterData.resize(clusterSizeBytes, 0);
            }
            bool status = writeCluster(cluster, clusterData);
            if (!status) {
                LOG_ERROR("Failed to write the updated directory data to disk.");
                return std::nullopt;
            }
        }

        // Create the new DirectoryEntry object
        // IMPORTANT: Store offset to the MAIN entry, not the first LFN entry!
        // insertPos points to the first LFN entry, but we need the offset to the main entry
        // which is insertPos + (number of LFN entries * entry size)
        // Note: mainEntryOffset was already calculated above in diagnostic section
        DirectoryEntry newEntry(mainEntryOffset, directoryEntry.getFirstCluster(), fileName, newDentry);

        LOG_WARN("[createFile] Created entry at offset %u (LFN starts at %u) in directory cluster %u", mainEntryOffset, insertPos, directoryEntry.getFirstCluster());
        return newEntry;
    }

    KVector<uint8_t> FAT32Partition::readFile(uint32_t startCluster, uint32_t offset, uint32_t size) const {
        if (startCluster >= countClusters_) kernelPanic("%s: Invalid cluster index!", __PRETTY_FUNCTION__);

        // Calculate the starting cluster offset in the file and adjust accordingly
        uint32_t fileClusterSizeBytes = clusterSizeBytes_;              // Cluster size in bytes
        uint32_t skipBytes            = offset % fileClusterSizeBytes;  // Offset within the first cluster

        KVector<uint8_t> data;
        KVector<uint32_t> clusters = readClusterChain(startCluster, offset, size);  // Only get relevant clusters

        uint32_t bytesToRead       = size;

        // Skip the first `skipClusters` clusters and read only the remaining clusters
        for (size_t i = 0; i < clusters.size(); ++i) {
            if (bytesToRead == 0) break;

            // Get the current cluster and its corresponding sector on disk
            uint32_t cluster = clusters[i];
            uint32_t sector  = getSectorFromCluster(cluster);

            // Read the cluster data from disk into a buffer
            KVector<uint8_t> clusterData(clusterSizeBytes_);
            for (uint8_t j = 0; j < clusterSize_; ++j) {
                bool diskStatus = diskDriver_.readSector(sector + j, clusterData.data() + j * sectorSize_, DEFAULT_TIMEOUT);
                if (!diskStatus) {
                    LOG_ERROR("Failed to read sector %u from disk.", sector + j);
                    return {};
                }
            }

            // If we are reading the first cluster, skip the initial bytes corresponding to the offset
            uint32_t readStart     = (i == 0) ? skipBytes : 0;
            uint32_t readableBytes = std::min(bytesToRead, clusterSizeBytes_ - readStart);

            // Append the required data to the result
            data.insert(data.end(), clusterData.begin() + readStart, clusterData.begin() + readStart + readableBytes);
            bytesToRead -= readableBytes;
        }

        return data;
    }


    std::optional<DirectoryEntry> FAT32Partition::createDirectory(DirectoryEntry& parentDirEntry, const KString& dirName) {
        LOG_INFO("[createDirectory] Creating directory '%s' in parent cluster %u", dirName.c_str(), parentDirEntry.getFirstCluster());

        // Step 0: Pre-check - ensure no file/directory with this name already exists
        KVector<DirectoryEntry> existingEntries = getDirectoryEntries(parentDirEntry.getFirstCluster());
        for (const auto& entry: existingEntries) {
            if (entry.getNameLong() == dirName) {
                LOG_ERROR("[createDirectory] An entry with name '%s' already exists in this directory (is %s)",
                          dirName.c_str(),
                          ((uint8_t) (entry.getAttributes() & EntryAttribute::Directory)) ? "directory" : "file");
                return std::nullopt;
            }
        }

        // Step 1: Create the directory entry in the parent directory
        // This creates the entry but with firstCluster = 0 initially
        auto newDirEntry = createFile(parentDirEntry, dirName, EntryAttribute::Directory);
        if (!newDirEntry.has_value()) {
            LOG_ERROR("[createDirectory] Failed to create directory entry for '%s'", dirName.c_str());
            return std::nullopt;
        }

        DirectoryEntry& dirEntry = newDirEntry.value();
        LOG_DEBUG("[createDirectory] Directory entry created, now allocating cluster");

        // Step 2: Allocate a cluster for the directory
        auto clusterOpt = allocateCluster();
        if (!clusterOpt.has_value()) {
            LOG_ERROR("[createDirectory] Failed to allocate cluster for directory '%s'", dirName.c_str());
            // TODO: Cleanup - mark the directory entry as deleted since allocation failed
            return std::nullopt;
        }

        uint32_t dirCluster = clusterOpt.value();
        LOG_DEBUG("[createDirectory] Allocated cluster %u for directory '%s'", dirCluster, dirName.c_str());

        // Step 3: Get parent cluster number for ".." entry
        uint32_t parentCluster = parentDirEntry.getFirstCluster();

        // Step 4: Create "." entry (points to itself)
        fat_dentry dotEntry{};
        memset(&dotEntry, 0, sizeof(dotEntry));
        memset(dotEntry.shortName, ' ', 11);
        dotEntry.shortName[0]      = '.';

        // Set attributes and cluster info
        dotEntry.attribute         = static_cast<uint8_t>(EntryAttribute::Directory);
        dotEntry.firstClusterLow   = static_cast<uint16_t>(dirCluster & 0xFFFF);
        dotEntry.firstClusterHigh  = static_cast<uint16_t>((dirCluster >> 16) & 0xFFFF);
        dotEntry.fileSize          = 0;

        // Use same default timestamps as createFile (2020-01-01 12:00:00)
        uint16_t defaultDate       = (40 << 9) | (1 << 5) | 1;   // 2020-01-01
        uint16_t defaultTime       = (12 << 11) | (0 << 5) | 0;  // 12:00:00
        dotEntry.creationTimeTenth = 0;
        dotEntry.creationTime      = defaultTime;
        dotEntry.creationDate      = defaultDate;
        dotEntry.lastAccessDate    = defaultDate;
        dotEntry.writeTime         = defaultTime;
        dotEntry.writeDate         = defaultDate;
        dotEntry.ntRes             = 0x00;

        LOG_DEBUG("[createDirectory] Created '.' entry: cluster=%u", dirCluster);

        // Step 5: Create ".." entry (points to parent)
        fat_dentry dotdotEntry{};
        memset(&dotdotEntry, 0, sizeof(dotdotEntry));
        memset(dotdotEntry.shortName, ' ', 11);
        dotdotEntry.shortName[0]      = '.';
        dotdotEntry.shortName[1]      = '.';

        // Set attributes and cluster info
        dotdotEntry.attribute         = static_cast<uint8_t>(EntryAttribute::Directory);
        dotdotEntry.firstClusterLow   = static_cast<uint16_t>(parentCluster & 0xFFFF);
        dotdotEntry.firstClusterHigh  = static_cast<uint16_t>((parentCluster >> 16) & 0xFFFF);
        dotdotEntry.fileSize          = 0;

        // Use same timestamps
        dotdotEntry.creationTimeTenth = 0;
        dotdotEntry.creationTime      = defaultTime;
        dotdotEntry.creationDate      = defaultDate;
        dotdotEntry.lastAccessDate    = defaultDate;
        dotdotEntry.writeTime         = defaultTime;
        dotdotEntry.writeDate         = defaultDate;
        dotdotEntry.ntRes             = 0x00;

        LOG_DEBUG("[createDirectory] Created '..' entry: cluster=%u", parentCluster);

        // Step 6: Write both entries to the allocated cluster
        KVector<uint8_t> dirClusterData(clusterSizeBytes_, 0);

        // Copy "." entry at offset 0
        memcpy(dirClusterData.data(), &dotEntry, sizeof(fat_dentry));

        // Copy ".." entry at offset 32
        memcpy(dirClusterData.data() + 32, &dotdotEntry, sizeof(fat_dentry));

        // Rest of cluster is already zeroed (0x00 = end of directory marker)

        LOG_DEBUG("[createDirectory] Writing directory cluster %u with '.' and '..' entries", dirCluster);

        bool writeSuccess = writeCluster(dirCluster, dirClusterData);
        if (!writeSuccess) {
            LOG_ERROR("[createDirectory] Failed to write directory cluster %u to disk", dirCluster);
            freeClusterChain(dirCluster);
            // TODO: Cleanup - mark the directory entry as deleted
            return std::nullopt;
        }

        // Step 7: Mark cluster chain end in FAT (single cluster directory ends here)
        bool setChainSuccess = setNextCluster(dirCluster, 0x0FFFFFFF);
        if (!setChainSuccess) {
            LOG_ERROR("[createDirectory] Failed to set cluster chain end for cluster %u", dirCluster);
            freeClusterChain(dirCluster);
            // TODO: Cleanup - mark the directory entry as deleted
            return std::nullopt;
        }

        LOG_DEBUG("[createDirectory] Marked cluster %u as end-of-chain", dirCluster);

        // Step 8: Update the directory entry with the actual cluster
        dirEntry.setClusterChain(dirCluster);

        // Step 9: Flush the updated entry back to the parent directory on disk
        bool flushSuccess = flushEntry(dirEntry);
        if (!flushSuccess) {
            LOG_ERROR("[createDirectory] Failed to flush directory entry for '%s'", dirName.c_str());
            freeClusterChain(dirCluster);
            return std::nullopt;
        }

        LOG_WARN("[createDirectory] Successfully created directory '%s' at cluster %u", dirName.c_str(), dirCluster);
        return dirEntry;
    }

    bool FAT32Partition::deleteFile(DirectoryEntry& parentDirEntry, const KString& fileName) {
        LOG_INFO("[deleteFile] Deleting file '%s' from parent cluster %u", fileName.c_str(), parentDirEntry.getFirstCluster());

        // STEP 1: Read & Parse parent directory
        KVector<DirectoryEntry> entries = getDirectoryEntries(parentDirEntry.getFirstCluster());

        // STEP 2: Find target entry by name
        DirectoryEntry* targetEntry     = nullptr;
        for (auto& entry: entries) {
            if (entry.getNameLong() == fileName) {
                targetEntry = &entry;
                break;
            }
        }

        if (!targetEntry) {
            LOG_ERROR("[deleteFile] File '%s' not found in parent directory", fileName.c_str());
            return false;
        }

        // STEP 3: Validation - must be a regular file, not a directory
        if ((uint8_t) (targetEntry->getAttributes() & EntryAttribute::Directory)) {
            LOG_ERROR("[deleteFile] '%s' is a directory, not a file. Use deleteDirectory instead.", fileName.c_str());
            return false;
        }

        // STEP 4: Get target entry offset and first cluster
        uint32_t targetOffset = targetEntry->getOffset();
        uint32_t firstCluster = targetEntry->getFirstCluster();

        LOG_DEBUG("[deleteFile] Target entry at offset %u, first cluster %u", targetOffset, firstCluster);

        // STEP 5: Free the data cluster chain (if any)
        if (firstCluster > 2) {
            LOG_DEBUG("[deleteFile] Freeing cluster chain starting at %u", firstCluster);
            freeClusterChain(firstCluster);
        }

        // STEP 6: Read parent directory raw data (all bytes, including LFN entries)
        KVector<uint8_t> dirData = readEntireFile(parentDirEntry.getFirstCluster());
        if (dirData.empty()) {
            LOG_ERROR("[deleteFile] Failed to read parent directory data");
            return false;
        }

        LOG_DEBUG("[deleteFile] Parent directory data size: %u bytes", dirData.size());

        // STEP 7: Mark target entry as deleted (0xE5)
        if (targetOffset >= dirData.size()) {
            LOG_ERROR("[deleteFile] Target offset %u exceeds directory size %u", targetOffset, dirData.size());
            return false;
        }

        dirData[targetOffset] = 0xE5;
        LOG_DEBUG("[deleteFile] Marked main entry at offset %u as deleted (0xE5)", targetOffset);

        // STEP 8: Find and mark LFN entries as deleted
        // LFN entries come BEFORE the main entry in directory, each is 32 bytes
        // Go backwards from targetOffset checking if entries are LFN (attribute byte at offset 11 == 0x0F)

        uint32_t lfnCount = 0;

        if (targetOffset >= 32) {
            for (int32_t offset = static_cast<int32_t>(targetOffset) - 32; offset >= 0; offset -= 32) {
                uint8_t* entry    = dirData.data() + offset;
                uint8_t firstByte = entry[0];
                uint8_t attrByte  = entry[11];

                // Check if this is an LFN entry: attribute byte (offset 11) must be 0x0F
                // Per FAT32 spec line 1351, LFN entries have ATTR_LONG_NAME = 0x0F
                bool isLFN        = (attrByte & 0x3F) == 0x0F;

                if (!isLFN) {
                    // Not an LFN, stop searching backwards
                    LOG_DEBUG("[deleteFile] Reached non-LFN entry at offset %u, stopping LFN search", offset);
                    break;
                }

                // This is an LFN entry, mark it as deleted
                entry[0] = 0xE5;
                lfnCount++;
                LOG_DEBUG("[deleteFile] Marked LFN entry at offset %u as deleted (was seq=0x%02X)", offset, firstByte);

                // Check if this was the LAST (first written) LFN entry - marked with 0x40 bit in sequence number
                // If so, we found them all, so stop
                if ((firstByte & 0x40) != 0) {
                    LOG_DEBUG("[deleteFile] Found last LFN entry at offset %u (has 0x40 bit set)", offset);
                    break;
                }
            }
        }

        if (lfnCount > 0) { LOG_DEBUG("[deleteFile] Deleted %u LFN entries before main entry", lfnCount); }

        // STEP 9: Write modified directory data back to disk
        KVector<uint32_t> clusterChain = readClusterChain(parentDirEntry.getFirstCluster());
        if (clusterChain.empty()) {
            LOG_ERROR("[deleteFile] Parent directory has no clusters");
            return false;
        }

        size_t clusterSizeBytes = clusterSize_ * sectorSize_;

        // Write back all clusters (we modified the directory data)
        for (size_t i = 0; i < clusterChain.size(); ++i) {
            size_t dataOffset = i * clusterSizeBytes;

            // Don't read past end of dirData
            if (dataOffset >= dirData.size()) break;

            size_t dataSize = std::min(clusterSizeBytes, dirData.size() - dataOffset);

            KVector<uint8_t> clusterData(dirData.begin() + static_cast<KVector<uint8_t>::difference_type>(dataOffset),
                                         dirData.begin() + static_cast<KVector<uint8_t>::difference_type>(dataOffset + dataSize));

            // Pad with zeros if this is not a full cluster
            if (clusterData.size() < clusterSizeBytes) { clusterData.resize(clusterSizeBytes, 0); }

            bool writeSuccess = writeCluster(clusterChain[i], clusterData);
            if (!writeSuccess) {
                LOG_ERROR("[deleteFile] Failed to write directory cluster %u", clusterChain[i]);
                return false;
            }

            LOG_DEBUG("[deleteFile] Wrote directory cluster %u", clusterChain[i]);
        }

        LOG_WARN("[deleteFile] Successfully deleted file '%s' (%u LFN entries marked deleted)", fileName.c_str(), lfnCount);
        return true;
    }

    /// endregion


    /// region DirectoryEntry

    DirectoryEntry::DirectoryEntry(uint32_t offset, uint32_t directoryStartCluster, KString longName, fat_dentry dentry)
        : offset_(offset), directoryStartCluster_(directoryStartCluster), shortName_(dentry.shortName, 11), attributes_(static_cast<EntryAttribute>(dentry.attribute)),
          NTRes_(dentry.ntRes), creationTimeMs_(dentry.creationTimeTenth), creationTime_(dentry.creationTime), creationDate_(dentry.creationDate),
          lastAccessDate_(dentry.lastAccessDate), writeTime_(dentry.writeTime), writeDate_(dentry.writeDate),
          clusterChain_((dentry.firstClusterHigh << 16) | dentry.firstClusterLow), fileSize_(dentry.fileSize), longName_(std::move(longName)) {
        /**
         * The constructor initializes a DirectoryEntry object by setting its fields based
         * on the given offset, long name, and FAT directory entry. It also constructs the long name
         * if it is empty, using the short name and extension.
         */

        // Initialize the long name if it's empty.
        if (longName_.empty()) {
            if (dentry.attribute & static_cast<uint8_t>(EntryAttribute::Archive)) {
                // Construct the long name for regular files.
                KString name(dentry.shortName, 8);
                KString extension(dentry.shortName + 8, 3);

                // Process the short name for case conversion.
                if (dentry.ntRes & 0x08) name.toLower();
                if (dentry.ntRes & 0x10) extension.toLower();

                if (!extension.empty()) longName_ = name.strip() + "." + extension.strip();
                else longName_ = name.strip();
            }
            else {
                // Construct the long name for directories.
                KString name(dentry.shortName, 11);
                if (dentry.ntRes & 0x08) name.toLower();

                longName_ = name.strip();
            }
        }
    }

    uint32_t DirectoryEntry::getOffset() const { return offset_; }
    KString DirectoryEntry::getNameShort() const { return shortName_; }
    EntryAttribute DirectoryEntry::getAttributes() const { return attributes_; }
    uint8_t DirectoryEntry::getNTRes() const { return NTRes_; }
    uint8_t DirectoryEntry::getCreationTimeMs() const { return creationTimeMs_; }
    uint16_t DirectoryEntry::getCreationTime() const { return creationTime_; }
    uint16_t DirectoryEntry::getCreationDate() const { return creationDate_; }
    uint16_t DirectoryEntry::getLastAccessDate() const { return lastAccessDate_; }
    uint16_t DirectoryEntry::getWriteTime() const { return writeTime_; }
    uint16_t DirectoryEntry::getWriteDate() const { return writeDate_; }
    uint16_t DirectoryEntry::getFirstCluster() const { return clusterChain_; }
    uint32_t DirectoryEntry::getFileSize() const { return fileSize_; }
    KString DirectoryEntry::getNameLong() const { return longName_; }

    DirectoryEntry::DirectoryEntry()
        : offset_(0), directoryStartCluster_(0), longName_(), shortName_(), attributes_(EntryAttribute::Invalid), NTRes_(0), creationTimeMs_(0), creationTime_(0), creationDate_(0),
          lastAccessDate_(0), writeTime_(0), writeDate_(0), clusterChain_(0), fileSize_(0) {}

    fat_dentry DirectoryEntry::getFatDentry() const {
        fat_dentry d{};
        memcpy((void*) d.shortName, (void*) shortName_.c_str(), 11);
        d.attribute         = static_cast<uint8_t>(attributes_);
        d.ntRes             = NTRes_;
        d.creationTimeTenth = creationTimeMs_;
        d.creationTime      = creationTime_;
        d.creationDate      = creationDate_;
        d.lastAccessDate    = lastAccessDate_;
        d.firstClusterHigh  = static_cast<uint16_t>((clusterChain_ >> 16) & 0xFFFF);
        d.writeTime         = writeTime_;
        d.writeDate         = writeDate_;
        d.firstClusterLow   = static_cast<uint16_t>(clusterChain_ & 0xFFFF);
        d.fileSize          = fileSize_;
        return d;
    }

    void DirectoryEntry::setFileSize(uint32_t newFileSize) { fileSize_ = newFileSize; }

    uint32_t DirectoryEntry::getDirectoryCluster() const { return directoryStartCluster_; }

    void DirectoryEntry::setClusterChain(uint32_t startingCluster) { clusterChain_ = startingCluster; }

    /// endregion


    /// region FAT32Partition::Attribute

    EntryAttribute operator|(EntryAttribute lhs, EntryAttribute rhs) { return static_cast<EntryAttribute>(static_cast<uint8_t>(lhs) | static_cast<uint8_t>(rhs)); }

    EntryAttribute& operator|=(EntryAttribute& lhs, EntryAttribute rhs) {
        lhs = lhs | rhs;
        return lhs;
    }

    EntryAttribute operator&(EntryAttribute lhs, EntryAttribute rhs) { return static_cast<EntryAttribute>(static_cast<uint8_t>(lhs) & static_cast<uint8_t>(rhs)); }

    /// endregion


}  // namespace PalmyraOS::kernel::vfs
