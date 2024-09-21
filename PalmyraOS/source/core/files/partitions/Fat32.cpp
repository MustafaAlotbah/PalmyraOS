

#include <utility>
#include <algorithm>
#include <limits>

#include "core/files/partitions/Fat32.h"
#include "core/encodings.h"  // utf16-le -> utf8
#include "core/panic.h"         // panic
#include "core/peripherals/Logger.h"

#include "libs/memory.h"
#include "libs/utils.h"


namespace PalmyraOS::kernel::vfs
{

  ///region FAT32Partition

  FAT32Partition::FAT32Partition(VirtualDisk<ATA>& diskDriver, uint32_t startSector, uint32_t countSectors)
	  : diskDriver_(diskDriver),
		startSector_(startSector),
		countSectors_(countSectors),
		type_(Type::Invalid)
  {
	  // Parse the BIOS Parameter Block to initialize fields
	  if (!parseBIOSParameterBlock())
	  {
		  LOG_ERROR("FAT32Partition construction failed: Unable to parse BIOS Parameter Block.");
		  return;
	  }

	  // Initialize additional fields required for the FAT32 partition
	  if (!initializeAdditionalFields())
	  {
		  LOG_ERROR("FAT32Partition construction failed: Unable to initialize additional fields.");
		  return;
	  }

	  clusterSizeBytes_ = clusterSize_ * sectorSize_;
  }

  bool FAT32Partition::parseBIOSParameterBlock()
  {
	  uint8_t bootSector[512];
	  bool    diskStatus = diskDriver_.readSector(startSector_, bootSector, DEFAULT_TIMEOUT);
	  if (!diskStatus)
	  {
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

	  // some assertions
	  if (sectorSize_ == 0 || clusterSize_ == 0)
	  {
		  LOG_ERROR("Invalid sector size or cluster size.");
		  return false;
	  }

	  return true;
  }

  bool FAT32Partition::initializeAdditionalFields()
  {

	  /**
	   * This function calculates additional fields necessary for navigating and managing
	   * the FAT32 file system. These include the number of root directory sectors, the size of the FAT,
	   * the total number of sectors, and the first data sector. It also determines the FAT type (FAT12, FAT16, FAT32)
	   * and sets up necessary multipliers and offsets.
	   */


	  // Calculate the number of root directory sectors (FAT12/16 only), (0 for FAT32)
	  rootDirSectors_ = ((countRootEntries_ * 32) + (sectorSize_ - 1)) / sectorSize_;

	  // Determine the FAT size based on the FAT16 and FAT32 fields.
	  fatSize_ = (fatSize16_ == 0) ? fatSize32_ : fatSize16_;

	  // Determine the total number of sectors based on FAT12/16 and FAT32 fields.
	  totalSectors_ = (countSectors16_ == 0) ? totalSectors32_ : countSectors16_;

	  // Calculate the first data sector.
	  firstDataSector_ = startSector_ + countReservedSectors_ + (countFATs_ * fatSize_) + rootDirSectors_;

	  // Calculate the total number of reserved and FAT sectors.
	  reservedAndFatSectorsCount_ = countReservedSectors_ + (countFATs_ * fatSize_) + rootDirSectors_;

	  // Calculate the total number of data sectors.
	  dataSectorCount_ = totalSectors_ - reservedAndFatSectorsCount_;

	  // Calculate the total number of clusters.
	  countClusters_ = dataSectorCount_ / clusterSize_; // floor division

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
	  if (type_ != Type::FAT32)
	  {
		  LOG_ERROR("FAT system is not FAT32. This feature is currently not tested.");
		  return false;
	  }

	  return true;
  }

  std::pair<uint32_t, uint32_t> FAT32Partition::calculateFATOffset(uint32_t n) const
  {
	  /**
	   * This function calculates the sector and offset within the FAT where the entry
	   * for a given cluster number can be found. This is essential for navigating the FAT to find
	   * the next cluster in a file or directory chain.
	   */

	  // Calculate the FAT offset.
	  uint32_t fatOffset = fatOffsetMult_ * n;

	  // Calculate the FAT sector number.
	  uint32_t fatSectorNumber = countReservedSectors_ + (fatOffset / sectorSize_);    // floor

	  // Calculate the entry offset within the sector.
	  uint32_t fatEntryOffset = fatOffset % sectorSize_;                              // remainder

	  return { fatSectorNumber + startSector_, fatEntryOffset };
  }

  uint32_t FAT32Partition::getNextCluster(uint32_t cluster) const
  {
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
	  bool    diskStatus = diskDriver_.readSector(fatSector, sectorData, DEFAULT_TIMEOUT);
	  if (!diskStatus)
	  {
		  LOG_ERROR("Failed to read FAT sector %u.", fatSector);
		  return 0;
	  }

	  // Interpret the next cluster number based on the FAT type.
	  if (type_ == Type::FAT32)
	  {
		  uint32_t nextCluster = get_uint32_t(sectorData, entryOffset) & 0x0FFFFFFF;
		  // Valid range check for FAT32
		  if (nextCluster >= 0x0FFFFFF8) return 0xFFFFFFFF; // End of chain marker
		  return nextCluster;
	  }
	  else if (type_ == Type::FAT16)
	  {
		  uint16_t nextCluster = get_uint16_t(sectorData, entryOffset);
		  // Valid range check for FAT16
		  if (nextCluster >= 0xFFF8) return 0xFFFFFFFF; // End of chain marker
		  return nextCluster;
	  }
	  else if (type_ == Type::FAT12)
	  {
		  uint16_t nextCluster = get_uint16_t(sectorData, entryOffset);
		  if (cluster & 1)
		  {
			  // For odd clusters, shift right by 4 bits
			  nextCluster = nextCluster >> 4;
		  }
		  else
		  {
			  // For even clusters, mask the higher 4 bits
			  nextCluster = nextCluster & 0x0FFF;
		  }
		  // Valid range check for FAT12
		  if (nextCluster >= 0xFF8) return 0xFFFFFFFF; // End of chain marker
		  return nextCluster;
	  }

	  // We should not reach this part
	  kernelPanic("function: %s:\nInvalid FAT type!", __PRETTY_FUNCTION__);
	  return 0;
  }

  uint32_t FAT32Partition::getSectorFromCluster(uint32_t cluster) const
  {
	  /**
	   * This function calculates the starting sector of a given cluster. It translates
	   * a cluster number into the corresponding sector number on the disk, which is necessary for
	   * reading or writing data in the cluster.
	   */

	  if (cluster < rootCluster_ || cluster >= countClusters_)
		  kernelPanic("%s: Invalid cluster index!", __PRETTY_FUNCTION__);

	  // First data sector offset plus cluster offset.
	  return firstDataSector_ + (cluster - 2) * clusterSize_;
  }

  KVector<uint32_t> FAT32Partition::readClusterChain(uint32_t startCluster) const
  {
	  /**
	   * This function reads the entire cluster chain starting from a given cluster.
	   * It follows the chain of clusters that make up a file or directory by reading the FAT
	   * to find each successive cluster in the chain.
	   */

	  if (startCluster >= countClusters_) kernelPanic("%s: Invalid cluster index!", __PRETTY_FUNCTION__);

	  KVector<uint32_t> clusters;
	  uint32_t          currentCluster = startCluster;
	  const uint32_t    maxIterations  = 1024 * 1024;  // 1 GiB
	  uint32_t          iterations     = 0;

	  // 0x0FFFFFF8 marks the end of the cluster chain in FAT32.
	  while (currentCluster < 0x0FFFFFF8 && iterations < maxIterations)
	  {
		  // Check for loops in the cluster chain.
		  if (std::find(clusters.begin(), clusters.end(), currentCluster) != clusters.end())
		  {
			  LOG_ERROR("Detected loop in cluster chain at cluster %u.", currentCluster);
			  break;
		  }

		  // free cluster
		  if (currentCluster == 0) break; // TODO investigation: is this to be expected?

		  // Add the current cluster to the chain and move to the next cluster.
		  clusters.push_back(currentCluster);
		  currentCluster = getNextCluster(currentCluster);

		  iterations++;
	  }
	  return clusters;
  }

  KVector<uint32_t> FAT32Partition::readClusterChain(uint32_t startCluster, uint32_t offset, uint32_t size) const
  {
	  /**
	   * This function reads the cluster chain starting by a given cluster and looks for the offset.
	   * It follows the chain of clusters that make up a file or directory by reading the FAT
	   * to find each successive cluster in the chain.
	   */

	  if (startCluster >= countClusters_)
		  kernelPanic("%s: Invalid cluster index!", __PRETTY_FUNCTION__);

	  // Calculate how many clusters we need to skip based on the offset
	  uint32_t clusterSizeBytes = clusterSize_ * sectorSize_;
	  uint32_t skipClusters     = offset / clusterSizeBytes;  // Number of clusters to skip based on the offset

	  // Calculate how many clusters we need based on the size
	  uint32_t requiredClusters = (size + clusterSizeBytes - 1) / clusterSizeBytes;  // Round up to the next cluster

	  KVector<uint32_t> clusters;
	  uint32_t          currentCluster = startCluster;
	  uint32_t          iterations     = 0;
	  const uint32_t    maxIterations  = countClusters_;  // To avoid infinite loops in case of a corrupted FAT

	  // Traverse the cluster chain, skipping unnecessary clusters and collecting only the required ones
	  while (currentCluster < 0x0FFFFFF8 && iterations < maxIterations)
	  {
		  // If we've skipped enough clusters, start collecting them
		  if (skipClusters == 0)
		  {
			  clusters.push_back(currentCluster);
			  if (clusters.size() >= requiredClusters)
				  break;  // Stop once we've collected enough clusters to satisfy the requested size
		  }
		  else
		  {
			  // Decrement the skip counter until we've skipped enough clusters
			  skipClusters--;
		  }

		  // Move to the next cluster in the chain
		  currentCluster = getNextCluster(currentCluster);
		  iterations++;

		  // Detect loops in the cluster chain (if the current cluster repeats)
		  if (std::find(clusters.begin(), clusters.end(), currentCluster) != clusters.end())
		  {
			  LOG_ERROR("Detected loop in cluster chain at cluster %u.", currentCluster);
			  break;
		  }
	  }

	  return clusters;
  }

  KVector<uint8_t> FAT32Partition::readFile(uint32_t startCluster, uint32_t size) const
  {
	  /**
	   * This function reads the clusters that make up a file, starting from the given
	   * cluster. It returns the file data up to the specified size by following the cluster chain.
	   */

	  if (startCluster >= countClusters_) kernelPanic("%s: Invalid cluster index!", __PRETTY_FUNCTION__);

	  KVector<uint8_t>  data;
	  KVector<uint32_t> clusters    = readClusterChain(startCluster);
	  auto              bytesToRead = size;

	  // Maximum of ~2 GiB to avoid narrowing conversion with casting
	  if (size > std::numeric_limits<KVector<uint8_t>::difference_type>::max())
	  {
		  kernelPanic(
			  "%s: size of %u > std::numeric_limits<KVector<uint8_t>::difference_type>::max()!",
			  __PRETTY_FUNCTION__, size
		  );
	  }

	  // Read all clusters in the chain.
	  for (const uint32_t& cluster : clusters)
	  {
		  if (bytesToRead <= 0) break;

		  // Transform FAT cluster number to Disk Sector
		  uint32_t sector = getSectorFromCluster(cluster);

		  // Prepare a buffer for reading the sector.
		  KVector<uint8_t> sectorData(sectorSize_ * clusterSize_);

		  // Read the entire cluster into the buffer.
		  for (uint8_t i = 0; i < clusterSize_; ++i)
		  {
			  bool
				  diskStatus = diskDriver_.readSector(sector + i, sectorData.data() + i * sectorSize_, DEFAULT_TIMEOUT);
			  if (!diskStatus)
			  {
				  LOG_ERROR("Failed to read sector %u from disk.", sector + i);
				  return {};
			  }
		  }

		  // Append the data to the file data buffer, up to the requested size.
		  if (bytesToRead >= sectorData.size())
		  {
			  data.insert(data.end(), sectorData.begin(), sectorData.end());
			  bytesToRead -= static_cast<int32_t>(sectorData.size());
		  }
		  else
		  {
			  data.insert(
				  data.end(),
				  sectorData.begin(),
				  sectorData.begin() + static_cast<KVector<uint8_t>::difference_type>(bytesToRead));
			  bytesToRead = 0;
		  }
	  }

	  return data;
  }

  KVector<uint8_t> FAT32Partition::readEntireFile(uint32_t startCluster) const
  {
	  if (startCluster >= countClusters_) kernelPanic("%s: Invalid cluster index!", __PRETTY_FUNCTION__);
	  return readFile(startCluster, std::numeric_limits<int32_t>::max());
  }

  KVector<DirectoryEntry> FAT32Partition::getDirectoryEntries(uint32_t directoryStartCluster) const
  {
	  /**
	   * Intuition: This function parses raw directory data to extract individual directory entries.
	   * It processes both short name and long name entries, constructing complete directory entries
	   * that can be used for further operations.
	   */

	  // TODO: add params: offset, count

	  if (directoryStartCluster >= countClusters_)
		  kernelPanic(
			  "%s: Invalid cluster index!\n"
			  "Count Clusters   : %d\n"
			  "Requested Cluster: %d\n",
			  __PRETTY_FUNCTION__,
			  countClusters_,
			  directoryStartCluster
		  );

	  KVector<uint8_t>                      data = readEntireFile(directoryStartCluster);
	  KVector<DirectoryEntry>               entries;
	  KVector<std::pair<uint8_t, KWString>> longNameParts;

	  for (size_t i = 0; i < data.size(); i += 32)
	  {
		  // Directory entry is a 32-byte structure.
		  const uint8_t* entry = data.data() + i;

		  if (entry[0] == 0x00) break;        // End of the directory
		  if (entry[0] == 0xE5) continue;     // Deleted entry, skip it

		  // Long name entry (part of a long name).
		  if (entry[11] == 0x0F)
		  {
			  // Check if this is the last LFN entry.
			  bool is_last_lfn = (entry[0] & 0x40) == 0x40;

			  // Order of this LFN part.
			  uint8_t order = entry[0] & 0x1F;

			  // Extract the LFN part from the entry.
			  KWString part;
			  {
				  for (size_t j = 1; j <= 10; j += 2)
				  {
					  uint16_t utf16_char = (entry[j + 1] << 8) | entry[j];
					  part.push_back(utf16_char);
				  }
				  for (size_t j = 14; j <= 25; j += 2)
				  {
					  uint16_t utf16_char = (entry[j + 1] << 8) | entry[j];
					  part.push_back(utf16_char);
				  }
				  for (size_t j = 28; j <= 31; j += 2)
				  {
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
			  if (is_last_lfn) longNameParts = { std::make_pair(order, part) };
		  }

		  else
		  {
			  // Regular directory entry.
			  // Cast the 32-byte entry data to a fat_dentry structure.
			  fat_dentry dentry = *(fat_dentry*)entry;

			  // Combine all Long File Name (LFN) parts to form the complete long name.
			  KString longName;
			  if (!longNameParts.empty())
			  {
				  // Sort the LFN parts based on their order.
				  std::sort(
					  longNameParts.begin(), longNameParts.end(), [](const auto& a, const auto& b)
					  {
						return a.first < b.first;  // Compare based on the first element (order).
					  }
				  );
				  KWString longNameUtf16;
				  for (const auto& part : longNameParts) longNameUtf16 += part.second;
				  longName = utf16le_to_utf8(longNameUtf16);
				  longNameParts.clear();
			  }


			  // Add the directory entry to the list.
			  entries.emplace_back(i, directoryStartCluster, longName, dentry);
		  }

	  }

	  return entries;
  }

  DirectoryEntry FAT32Partition::resolvePathToEntry(const KString& path) const
  {
	  // Path must be absolute
	  if (path.empty() || path[0] != '/') return {};

	  // Tokenize the path to get individual directory/file names
	  KVector<KString> tokens = path.split('/', true);

	  // Special case for root directory
	  if (tokens.empty() || (tokens.size() == 1 && tokens[0].empty()))
	  {
		  fat_dentry rootDentry = {
			  .shortName = "/",
			  .attribute = 0x10, // Directory attribute
			  .ntRes = 0,
			  .creationTimeTenth = 0,
			  .creationTime = 0,
			  .creationDate = 0,
			  .lastAccessDate = 0,
			  .firstClusterHigh = 0,
			  .writeTime = 0,
			  .writeDate = 0,
			  .firstClusterLow = static_cast<uint16_t>(rootCluster_ & 0xFFFF),
			  .fileSize = 0
		  };
		  return DirectoryEntry(0, 0, KString("/"), rootDentry);
	  }

	  // Start at the root directory
	  uint32_t currentCluster = rootCluster_;

	  // Iterate through the tokens to find the target node
	  for (size_t i = 0; i < tokens.size(); ++i)
	  {
		  const KString& token = tokens[i];

		  // Skip empty tokens (possible leading slash)
		  if (token.empty()) continue;

		  // Read the current directory cluster chain
		  KVector<DirectoryEntry> entries = getDirectoryEntries(currentCluster);

		  bool found = false;
		  for (const auto& entry : entries)
		  {
			  if (entry.getNameLong() == token || entry.getNameShort() == token)
			  {
				  // If this is the last token, return the entry
				  if (i == tokens.size() - 1) return entry;

				  if ((uint8_t)(entry.getAttributes() & EntryAttribute::Directory))
				  {
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

  bool FAT32Partition::setNextCluster(uint32_t cluster, uint32_t nextCluster)
  {

	  // Assert the cluster in within this partition
	  if (cluster < 2 || cluster >= countClusters_)
	  {
		  kernelPanic("Attempt to set next cluster for an invalid cluster index! (%u)", cluster);
		  return false; // Error handling for invalid cluster index
	  }

	  // Calculate the cluster's sector and offset
	  auto    [fatSector, entryOffset] = calculateFATOffset(cluster);
	  uint8_t sectorData[512];

	  // read the containing sector (FAT)
	  {
		  bool diskStatus = diskDriver_.readSector(fatSector, sectorData, DEFAULT_TIMEOUT);
		  if (!diskStatus)
		  {
			  LOG_ERROR("Failed to read sector: %u from disk.", fatSector);
			  return false;
		  }
	  }

	  // adjust the next cluster of the current cluster
	  if (type_ == Type::FAT32)
	  {
		  *((uint32_t * )(sectorData + entryOffset)) = nextCluster & 0x0FFF'FFFF;
	  }
	  else if (type_ == Type::FAT16)
	  {
		  *((uint16_t * )(sectorData + entryOffset)) = nextCluster & 0xFFFF;
	  }

	  // write the sector to the FAT again
	  {
		  bool diskStatus = diskDriver_.writeSector(fatSector, sectorData, DEFAULT_TIMEOUT);
		  if (!diskStatus)
		  {
			  LOG_ERROR("Failed to write sector: %u to disk.", fatSector);
			  return false;
		  }
	  }

	  return true;
  }

  void FAT32Partition::freeClusterChain(uint32_t startCluster)
  {
	  // Check if the cluster number is valid
	  if (startCluster >= countClusters_)
	  {
		  kernelPanic(
			  "%s: Attempted to free an invalid cluster number: %u. Valid range is 0 to %u.",
			  __PRETTY_FUNCTION__, startCluster, countClusters_ - 1
		  );
	  }

	  uint32_t cluster = startCluster;
	  while (cluster < 0x0FFFFFF8)
	  {
		  uint32_t nextCluster = getNextCluster(cluster);

		  // Free the cluster
		  if (!setNextCluster(cluster, 0))
		  {
			  LOG_ERROR("Failed to free cluster %u", cluster);
		  }

		  // Check for invalid or end markers
		  if (nextCluster == 0 || nextCluster > 0x0FFFFFF7) break;

		  // Move to the next cluster in the chain
		  cluster = nextCluster;
	  }
  }

  std::optional<uint32_t> FAT32Partition::allocateCluster()
  {
	  // TODO lock

	  // This method will scan the FAT from the beginning of the data section to the end of the FAT32 volume.
	  // We start from cluster 2 because clusters 0 and 1 are reserved for media descriptor and the FAT itself.
	  for (uint32_t cluster = rootCluster_; cluster < countClusters_; ++cluster)
	  {
		  uint32_t nextCluster = getNextCluster(cluster);

		  // Check if the cluster is free in the FAT (0x00000000 means free)
		  if (nextCluster == 0x00000000)
		  {
			  // Mark this cluster as the end of the chain (0x0FFFFFFF indicates end of cluster chain in FAT32)
			  if (!setNextCluster(cluster, 0x0FFFFFFF))
			  {
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

  bool FAT32Partition::writeCluster(uint32_t cluster, const KVector<uint8_t>& data)
  {
	  if (data.size() > clusterSizeBytes_)
	  {
		  LOG_ERROR("Data size %u exceeds cluster size %u bytes", data.size(), clusterSizeBytes_);
		  return false;
	  }

	  uint32_t sector = getSectorFromCluster(cluster);

	  for (uint8_t i = 0; i < clusterSize_; ++i)
	  {
		  bool status = diskDriver_.writeSector(sector + i, data.data() + i * sectorSize_, DEFAULT_TIMEOUT);
		  if (!status)
		  {
			  LOG_ERROR("Failed to write sector %u for cluster %u", sector + i, cluster);
			  return false;
		  }
	  }

	  return true;
  }

  // File Methods

  KVector<uint8_t> FAT32Partition::fetchDataFromEntry(
	  const DirectoryEntry& entry,
	  uint32_t offset,
	  uint32_t countBytes
  ) const
  {
	  if (offset >= entry.getFileSize()) return {};

	  // Limit the read operation within the file size bounds.
	  uint32_t maxPossibleBytes = entry.getFileSize() - offset;
	  if (countBytes > maxPossibleBytes) countBytes = maxPossibleBytes;

	  KVector<uint8_t> fileData = readFile(entry.getFirstCluster(), entry.getFileSize());
	  return {
		  fileData.begin() + static_cast<KVector<uint8_t>::difference_type>(offset),
		  fileData.begin() + static_cast<KVector<uint8_t>::difference_type>(offset + countBytes)
	  };
  }

  KVector<uint8_t> FAT32Partition::read(const DirectoryEntry& entry, uint32_t offset, uint32_t countBytes) const
  {
	  return fetchDataFromEntry(entry, offset, countBytes);
  }

  bool FAT32Partition::flushEntry(const DirectoryEntry& entry)
  {

	  if (entry.getAttributes() == EntryAttribute::Invalid) return false;

	  // Get the FAT directory entry structure from the DirectoryEntry object
	  fat_dentry dentry = entry.getFatDentry();

	  // Calculate the sector and offset in the directory where this entry is located
	  uint32_t directoryCluster  = entry.getDirectoryCluster();
	  uint32_t offsetInDirectory = entry.getOffset();

	  // Calculate the sector number within the cluster chain
	  uint32_t sectorIndex        = offsetInDirectory / sectorSize_;
	  uint32_t offsetWithinSector = offsetInDirectory % sectorSize_;

	  // Calculate the cluster and sector within the cluster chain
	  uint32_t clusterIndex        = sectorIndex / clusterSize_;
	  uint32_t sectorWithinCluster = sectorIndex % clusterSize_;

	  // Read the cluster chain to get the specific cluster
	  KVector<uint32_t> clusterChain = readClusterChain(directoryCluster);

	  // Invalid offset, cluster chain is shorter than expected
	  if (clusterIndex >= clusterChain.size())
	  {
		  LOG_ERROR("Invalid offset: cluster chain is shorter than expected.");
		  return false;
	  }

	  uint32_t currentCluster = clusterChain[clusterIndex];
	  uint32_t sector         = getSectorFromCluster(currentCluster) + sectorWithinCluster;

	  // Read the sector containing the directory entry
	  uint8_t sectorData[512];
	  {
		  bool diskStatus = diskDriver_.readSector(sector, sectorData, DEFAULT_TIMEOUT);
		  if (!diskStatus)
		  {
			  LOG_ERROR("Failed to read sector %u from disk.", sector);
			  return false;
		  }
	  }

	  // Copy the FAT directory entry structure into the correct location within the sector data
	  memcpy(sectorData + offsetWithinSector, &dentry, sizeof(fat_dentry));

	  // Write the modified sector data back to the disk
	  {
		  bool diskStatus = diskDriver_.writeSector(sector, sectorData, DEFAULT_TIMEOUT);
		  if (!diskStatus)
		  {
			  LOG_ERROR("Failed to write sector %u to disk.", sector);
			  return false;
		  }
	  }

	  return true;
  }

  bool FAT32Partition::append(DirectoryEntry& entry, const KVector<uint8_t>& bytes)
  {
	  uint32_t fileSize          = entry.getFileSize();
	  uint32_t lastClusterOffset = fileSize % clusterSizeBytes_;
	  uint32_t remainingBytes    = bytes.size();
	  uint32_t writePosition     = 0;

	  // Read the current cluster chain of the file
	  KVector<uint32_t> clusterChain = readClusterChain(entry.getFirstCluster());
	  uint32_t          lastCluster  = clusterChain.empty() ? 0 : clusterChain.back();

	  // Write to existing space in the last cluster
	  if ((fileSize == 0 || lastClusterOffset != 0) && !clusterChain.empty())
	  {
		  uint32_t         bytesToWrite = std::min(clusterSizeBytes_ - lastClusterOffset, remainingBytes);
		  KVector<uint8_t> clusterData  = readFile(lastCluster, clusterSizeBytes_);

		  // Copy new bytes to the correct position in the last cluster data
		  std::copy(
			  bytes.begin(),
			  bytes.begin() + static_cast<KVector<uint8_t>::difference_type>(bytesToWrite),
			  clusterData.begin() + static_cast<KVector<uint8_t>::difference_type>(lastClusterOffset)
		  );

		  // Write back the modified cluster
		  bool status = writeCluster(lastCluster, clusterData);
		  if (!status)
		  {
			  LOG_ERROR("Failed to write the last cluster %u", lastCluster);
			  return false;
		  }

		  remainingBytes -= bytesToWrite;
		  writePosition += bytesToWrite;
	  }

	  // Allocate and write to new clusters if needed
	  while (remainingBytes > 0)
	  {
		  auto newClusterOpt = allocateCluster();

		  // Unable to allocate new cluster during append operation.
		  if (!newClusterOpt)
		  {
			  LOG_ERROR("Failed to allocate new cluster during append operation.");
			  return false;
		  }

		  uint32_t newCluster = newClusterOpt.value();
		  if (lastCluster == 0)
		  {
			  // This is the first cluster being allocated for this file
			  lastCluster = newCluster;
			  if (entry.getFirstCluster() == 0)
			  {
				  // Update the first cluster in the directory entry if it was previously empty
				  entry.setClusterChain(newCluster);

				  // Flush changes back to the directory
				  bool status = flushEntry(entry);
				  if (!status)
				  {
					  LOG_ERROR("Failed to flush entry after setting first cluster to %u", newCluster);
					  return false;
				  }
			  }
		  }
		  else
		  {
			  // Link the new cluster to the last one in the chain
			  bool status = setNextCluster(lastCluster, newCluster);
			  // TODO check and log + early exit here
		  }

		  lastCluster = newCluster;
		  clusterChain.push_back(newCluster);

		  uint32_t         bytesToWrite = std::min(clusterSizeBytes_, remainingBytes);
		  KVector<uint8_t> clusterData(
			  bytes.begin() + static_cast<KVector<uint8_t>::difference_type>(writePosition),
			  bytes.begin() + static_cast<KVector<uint8_t>::difference_type>(writePosition + bytesToWrite)
		  );

		  // If the last cluster is not fully utilized, fill it with zeros
		  if (clusterData.size() < clusterSizeBytes_) clusterData.resize(clusterSizeBytes_, 0);

		  bool status = writeCluster(newCluster, clusterData);
		  if (!status)
		  {
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
	  if (!status)
	  {
		  LOG_ERROR("Failed to flush entry after appending data.");
		  return false;
	  }

	  return true;
  }

  bool FAT32Partition::write(DirectoryEntry& entry, const KVector<uint8_t>& bytes)
  {

	  // TODO set as 0 and let append allocate

	  // Allocate the first cluster for the file
	  auto firstClusterOpt = allocateCluster();

	  // Unable to allocate first cluster during write operation.
	  if (!firstClusterOpt)
	  {
		  LOG_ERROR("Failed to allocate first cluster during write operation.");
		  return false;
	  }

	  // Step 1: Free existing cluster chain
	  if (entry.getFirstCluster() > 2) freeClusterChain(entry.getFirstCluster());

	  // Step 2: Reset file size to zero before appending new data
	  entry.setFileSize(0);

	  entry.setClusterChain(firstClusterOpt.value());

	  // Step 3: Append new data
	  {
		  bool status = append(entry, bytes);
		  if (!status)
		  {
			  LOG_ERROR("Failed to append new data during write operation.");
			  return false;
		  }
	  }

	  // Step 4: Update the directory entry to reflect changes
	  {
		  bool status = flushEntry(entry);
		  if (!status)
		  {
			  LOG_ERROR("Failed to flush entry after write operation.");
			  return false;
		  }
	  }

	  return true;
  }

  FAT32Partition::Type FAT32Partition::getType()
  {
	  return type_;
  }

  bool FAT32Partition::isValidSFNCharacter(char c)
  {
	  static const char invalidChars[] = "\"*/:<>?\\|+,;=[]";
	  return (isalnum(static_cast<unsigned char>(c)) || strchr("$%'-_@~`!(){}^#&", c)) && !strchr(invalidChars, c);
  }

  KString FAT32Partition::generateUniqueShortName(const KString& longName, const KVector<KString>& existingShortNames)
  {
	  KString upperName(longName.c_str());
	  upperName.toUpper();

	  // Remove invalid characters and replace with underscores
	  for (char& c : upperName)
	  {
		  if (!isValidSFNCharacter(c))
			  c = '_';
	  }

	  // Extract base name and extension
	  size_t  dotPos    = upperName.find_last_of(".");
	  KString baseName  = (dotPos == KString::npos) ? upperName : upperName.substr(0, dotPos);
	  KString extension = (dotPos == KString::npos) ? KString("") : upperName.substr(dotPos + 1);

	  // Remove spaces and invalid characters
	  baseName.erase(
		  std::remove_if(
			  baseName.begin(), baseName.end(), [](char c)
			  { return c == ' '; }
		  ), baseName.end());
	  extension.erase(
		  std::remove_if(
			  extension.begin(), extension.end(), [](char c)
			  { return c == ' '; }
		  ), extension.end());

	  // Truncate base name and extension to 8 and 3 characters
	  baseName  = baseName.substr(0, 8);
	  extension = extension.substr(0, 3);

	  KString sfn = baseName;
	  if (!extension.empty())
	  {
		  sfn += '.';
		  sfn += extension;
	  }

	  if (std::find(existingShortNames.begin(), existingShortNames.end(), sfn) == existingShortNames.end())
	  {
		  return sfn;
	  }

	  // Handle name collisions using tilde notation
	  for (uint32_t num = 1; num <= 999999; ++num)
	  {
		  char buffer[10];
		  snprintf(buffer, sizeof(buffer), "~%d", num);
		  KString numStr(buffer);
		  KString sfnBaseNum = baseName.substr(0, std::min<size_t>(8 - numStr.size(), baseName.size())) + numStr;
		  KString sfnNum     = sfnBaseNum;
		  if (!extension.empty())
		  {
			  sfnNum += '.';
			  sfnNum += extension;
		  }
		  if (std::find(existingShortNames.begin(), existingShortNames.end(), sfnNum) == existingShortNames.end())
		  {
			  return sfnNum;
		  }
	  }

	  // If all attempts fail, return an empty string
	  return KString("");
  }

  uint8_t FAT32Partition::calculateShortNameChecksum(const char* shortName)
  {
	  uint8_t  sum = 0;
	  for (int i   = 0; i < 11; ++i)
	  {
		  sum = ((sum & 1) ? 0x80 : 0) + (sum >> 1) + static_cast<uint8_t>(shortName[i]);
	  }
	  return sum;
  }

  bool FAT32Partition::needsLFN(const KString& fileName)
  {
	  // Check if the fileName fits in 8.3 format and contains valid characters
	  // Implement the logic to determine if LFN is needed
	  // Return true if LFN is needed, false otherwise
	  // For simplicity, we'll assume that any name that doesn't fit 8.3 or contains invalid characters needs LFN

	  // Convert to uppercase
	  KString upperName(fileName.c_str());
	  upperName.toUpper();

	  // Extract base name and extension
	  size_t  dotPos    = upperName.find_last_of(".");
	  KString baseName  = (dotPos == KString::npos) ? upperName : upperName.substr(0, dotPos);
	  KString extension = (dotPos == KString::npos) ? KString("") : upperName.substr(dotPos + 1);

	  // Check lengths
	  if (baseName.size() > 8 || extension.size() > 3) return true;

	  // Check for invalid characters
	  for (char c : baseName)
	  {
		  if (!isValidSFNCharacter(c)) return true;
	  }

	  for (char c : extension)
	  {
		  if (!isValidSFNCharacter(c)) return true;
	  }

	  return false;
  }

  KVector<fat_dentry> FAT32Partition::createLFNEntries(const KString& longName, uint8_t checksum)
  {
	  // Break the longName into chunks of 13 UTF-16 characters
	  KVector<fat_dentry> lfnEntries;
	  KWString            unicodeName = utf8_to_utf16le(longName);

	  // Calculate the number of LFN entries needed
	  size_t totalEntries = (unicodeName.size() + 12) / 13; // Each LFN entry can hold 13 UTF-16 characters

	  for (size_t i = 0; i < totalEntries; ++i)
	  {
		  fat_dentry lfnEntry{};
		  memset(&lfnEntry, 0, sizeof(lfnEntry));

		  // Set sequence number
		  uint8_t seqNum = static_cast<uint8_t>(totalEntries - i);
		  if (i == 0)
			  seqNum |= 0x40; // Last LFN entry flag

		  lfnEntry.shortName[0] = seqNum;

		  // Set attribute
		  lfnEntry.attribute = 0x0F;

		  // Set checksum
		  lfnEntry.ntRes             = 0x00;
		  lfnEntry.creationTimeTenth = checksum;

		  // Set name parts
		  size_t charIndex = i * 13;

		  // First 5 UTF-16 characters
		  for (size_t j = 0; j < 5; ++j)
		  {
			  if (charIndex + j < unicodeName.size())
			  {
				  uint16_t wchar = unicodeName[charIndex + j];
				  lfnEntry.shortName[1 + j * 2] = wchar & 0xFF;
				  lfnEntry.shortName[2 + j * 2] = (wchar >> 8) & 0xFF;
			  }
			  else
			  {
				  lfnEntry.shortName[1 + j * 2] = 0xFF;
				  lfnEntry.shortName[2 + j * 2] = 0xFF;
			  }
		  }

		  // Next 6 UTF-16 characters
		  for (size_t j = 0; j < 6; ++j)
		  {
			  if (charIndex + 5 + j < unicodeName.size())
			  {
				  uint16_t wchar = unicodeName[charIndex + 5 + j];
				  lfnEntry.shortName[14 + j * 2] = wchar & 0xFF;
				  lfnEntry.shortName[15 + j * 2] = (wchar >> 8) & 0xFF;
			  }
			  else
			  {
				  lfnEntry.shortName[14 + j * 2] = 0xFF;
				  lfnEntry.shortName[15 + j * 2] = 0xFF;
			  }
		  }

		  // Last 2 UTF-16 characters
		  for (size_t j = 0; j < 2; ++j)
		  {
			  if (charIndex + 11 + j < unicodeName.size())
			  {
				  uint16_t wchar = unicodeName[charIndex + 11 + j];
				  lfnEntry.shortName[28 + j * 2] = wchar & 0xFF;
				  lfnEntry.shortName[29 + j * 2] = (wchar >> 8) & 0xFF;
			  }
			  else
			  {
				  lfnEntry.shortName[28 + j * 2] = 0xFF;
				  lfnEntry.shortName[29 + j * 2] = 0xFF;
			  }
		  }

		  // Set zero for reserved fields
		  lfnEntry.firstClusterLow = 0x0000;

		  lfnEntries.push_back(lfnEntry);
	  }

	  return lfnEntries;
  }

  std::optional<DirectoryEntry> FAT32Partition::createFile(
	  DirectoryEntry& directoryEntry,
	  const KString& fileName,
	  EntryAttribute attributes
  )
  {

	  // Check if the directory entry is indeed a directory
	  if (!(uint8_t)(directoryEntry.getAttributes() & EntryAttribute::Directory))
	  {
		  LOG_ERROR("The specified entry is not a directory.");
		  return std::nullopt;
	  }

	  // Read the directory entries in the specified directory
	  KVector<DirectoryEntry> entries = getDirectoryEntries(directoryEntry.getFirstCluster());

	  // Collect existing short names and check for existing file
	  KVector<KString> existingShortNames;
	  for (const auto& entry : entries)
	  {
		  if (entry.getNameLong() == fileName)
		  {
			  LOG_ERROR("A file with the same name already exists in the directory.");
			  return std::nullopt;
		  }
		  existingShortNames.push_back(entry.getNameShort());
	  }

	  // Generate unique short name (SFN)
	  KString sfn = generateUniqueShortName(fileName, existingShortNames);
	  if (sfn.empty())
	  {
		  LOG_ERROR("Failed to generate a unique short name for the file.");
		  return std::nullopt;
	  }

	  // Calculate SFN checksum
	  uint8_t checksum = calculateShortNameChecksum(sfn.c_str());

	  // Prepare LFN entries if needed
	  KVector<fat_dentry> lfnEntries;
	  if (needsLFN(fileName))
	  {
		  lfnEntries = createLFNEntries(fileName, checksum);
	  }

	  // Create the main directory entry
	  fat_dentry newDentry{};
	  memset(&newDentry, ' ', sizeof(newDentry));  // Initialize with spaces

	  // Set the short name (8.3 format), pad with spaces
	  size_t  dotPos    = sfn.find('.');
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

	  // Copy the baseName and extension into the padded arrays
	  memcpy(baseNamePadded, baseName.c_str(), baseNameLen);
	  memcpy(extensionPadded, extension.c_str(), extensionLen);

	  // Copy baseNamePadded and extensionPadded into shortName
	  memcpy(newDentry.shortName, baseNamePadded, 8);
	  memcpy(newDentry.shortName + 8, extensionPadded, 3);

	  // Set attributes and initial values
	  newDentry.attribute        = static_cast<uint8_t>(attributes);
	  newDentry.firstClusterLow  = 0; // Initially zero, allocate when writing data
	  newDentry.firstClusterHigh = 0;
	  newDentry.fileSize         = 0;

	  // Set time and date fields (could set to zero or get current time)
	  newDentry.creationTimeTenth = 0;
	  newDentry.creationTime      = 0;
	  newDentry.creationDate      = 0;
	  newDentry.lastAccessDate    = 0;
	  newDentry.writeTime         = 0;
	  newDentry.writeDate         = 0;

	  // Now, find free entries in the directory data
	  // Need totalEntries = lfnEntries.size() + 1

	  KVector<uint8_t> dirData = readFile(directoryEntry.getFirstCluster(), std::numeric_limits<int32_t>::max());

	  size_t totalEntriesNeeded = lfnEntries.size() + 1;
	  size_t dirEntrySize       = sizeof(fat_dentry);
	  size_t dirEntriesCount    = dirData.size() / dirEntrySize;

	  // Find consecutive free entries
	  size_t insertPos        = 0;
	  bool   foundFreeEntries = false;

	  for (size_t i = 0; i <= dirEntriesCount; ++i)
	  {
		  bool        entriesFree = true;
		  for (size_t j           = 0; j < totalEntriesNeeded; ++j)
		  {
			  size_t offset = (i + j) * dirEntrySize;
			  if (offset >= dirData.size())
			  {
				  // Reached the end, need to extend dirData
				  entriesFree = true;
				  break;
			  }
			  uint8_t firstByte = dirData[offset];
			  if (firstByte != 0x00 && firstByte != 0xE5)
			  {
				  entriesFree = false;
				  break;
			  }
		  }
		  if (entriesFree)
		  {
			  insertPos        = i * dirEntrySize;
			  foundFreeEntries = true;
			  break;
		  }
	  }

	  if (!foundFreeEntries)
	  {
		  // Need to extend dirData
		  insertPos = dirData.size();
		  dirData.resize(dirData.size() + totalEntriesNeeded * dirEntrySize, 0);
	  }

	  // Now, insert the LFN entries and main entry into dirData at insertPos

	  // Copy LFN entries and main entry into dirData in order

	  // LFN entries are inserted in reverse order (last one first)

	  size_t entryOffset = insertPos;

	  for (auto it = lfnEntries.rbegin(); it != lfnEntries.rend(); ++it)
	  {
		  memcpy(dirData.data() + entryOffset, &(*it), dirEntrySize);
		  entryOffset += dirEntrySize;
	  }

	  // Copy the main directory entry
	  memcpy(dirData.data() + entryOffset, &newDentry, dirEntrySize);
	  entryOffset += dirEntrySize;

	  // Now, update the directory clusters and write back to disk

	  // Calculate the number of clusters needed to store the directory data
	  size_t clusterSizeBytes = clusterSize_ * sectorSize_;
	  size_t totalClusters    = (dirData.size() + clusterSizeBytes - 1) / clusterSizeBytes;

	  // Read the existing cluster chain of the directory
	  KVector<uint32_t> clusterChain = readClusterChain(directoryEntry.getFirstCluster());

	  // Allocate additional clusters if necessary
	  while (clusterChain.size() < totalClusters)
	  {
		  auto additionalClusterOpt = allocateCluster();
		  if (!additionalClusterOpt)
		  {
			  LOG_ERROR("Failed to allocate additional cluster for the directory.");
			  return std::nullopt;
		  }
		  uint32_t additionalCluster = additionalClusterOpt.value();
		  if (!clusterChain.empty())
		  {
			  setNextCluster(clusterChain.back(), additionalCluster);
		  }
		  else
		  {
			  // This should not happen, but in case the directory has no clusters
			  directoryEntry.setClusterChain(additionalCluster);
			  flushEntry(directoryEntry);
		  }
		  clusterChain.push_back(additionalCluster);
	  }

	  // Mark the end of the cluster chain
	  if (!clusterChain.empty())
	  {
		  setNextCluster(clusterChain.back(), 0x0FFFFFFF); // End of chain marker
	  }

	  // Write the updated directory data back to disk
	  for (size_t i = 0; i < dirData.size(); i += clusterSizeBytes)
	  {
		  uint32_t         cluster  = clusterChain[i / clusterSizeBytes];
		  size_t           dataSize = std::min(clusterSizeBytes, dirData.size() - i);
		  KVector<uint8_t> clusterData(dirData.begin() + i, dirData.begin() + i + dataSize);
		  if (clusterData.size() < clusterSizeBytes)
		  {
			  // Pad with zeros
			  clusterData.resize(clusterSizeBytes, 0);
		  }
		  bool status = writeCluster(cluster, clusterData);
		  if (!status)
		  {
			  LOG_ERROR("Failed to write the updated directory data to disk.");
			  return std::nullopt;
		  }
	  }

	  // Create the new DirectoryEntry object
	  DirectoryEntry newEntry(
		  static_cast<uint32_t>(insertPos),
		  directoryEntry.getFirstCluster(),
		  fileName,
		  newDentry
	  );

	  LOG_ERROR("worked.");
	  return newEntry;
  }

  KVector<uint8_t> FAT32Partition::readFile(uint32_t startCluster, uint32_t offset, uint32_t size) const
  {
	  if (startCluster >= countClusters_) kernelPanic("%s: Invalid cluster index!", __PRETTY_FUNCTION__);

	  // Calculate the starting cluster offset in the file and adjust accordingly
	  uint32_t fileClusterSizeBytes = clusterSizeBytes_;  // Cluster size in bytes
	  uint32_t skipBytes            = offset % fileClusterSizeBytes;  // Offset within the first cluster

	  KVector<uint8_t>  data;
	  KVector<uint32_t> clusters = readClusterChain(startCluster, offset, size);  // Only get relevant clusters

	  uint32_t bytesToRead = size;

	  // Skip the first `skipClusters` clusters and read only the remaining clusters
	  for (size_t i = 0; i < clusters.size(); ++i)
	  {
		  if (bytesToRead == 0) break;

		  // Get the current cluster and its corresponding sector on disk
		  uint32_t cluster = clusters[i];
		  uint32_t sector  = getSectorFromCluster(cluster);

		  // Read the cluster data from disk into a buffer
		  KVector<uint8_t> clusterData(clusterSizeBytes_);
		  for (uint8_t     j = 0; j < clusterSize_; ++j)
		  {
			  bool diskStatus =
					   diskDriver_.readSector(sector + j, clusterData.data() + j * sectorSize_, DEFAULT_TIMEOUT);
			  if (!diskStatus)
			  {
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




  ///endregion



  ///region DirectoryEntry

  DirectoryEntry::DirectoryEntry(uint32_t offset, uint32_t directoryStartCluster, KString longName, fat_dentry dentry)
	  : offset_(offset),
		directoryStartCluster_(directoryStartCluster),
		shortName_(dentry.shortName, 11),
		attributes_(static_cast<EntryAttribute>(dentry.attribute)),
		NTRes_(dentry.ntRes),
		creationTimeMs_(dentry.creationTimeTenth),
		creationTime_(dentry.creationTime),
		creationDate_(dentry.creationDate),
		lastAccessDate_(dentry.lastAccessDate),
		writeTime_(dentry.writeTime),
		writeDate_(dentry.writeDate),
		clusterChain_((dentry.firstClusterHigh << 16) | dentry.firstClusterLow),
		fileSize_(dentry.fileSize),
		longName_(std::move(longName))
  {
	  /**
	   * The constructor initializes a DirectoryEntry object by setting its fields based
	   * on the given offset, long name, and FAT directory entry. It also constructs the long name
	   * if it is empty, using the short name and extension.
	   */

	  // Initialize the long name if it's empty.
	  if (longName_.empty())
	  {
		  if (dentry.attribute & static_cast<uint8_t>(EntryAttribute::Archive))
		  {
			  // Construct the long name for regular files.
			  KString name(dentry.shortName, 8);
			  KString extension(dentry.shortName + 8, 3);

			  // Process the short name for case conversion.
			  if (dentry.ntRes & 0x08) name.toLower();
			  if (dentry.ntRes & 0x10) extension.toLower();

			  if (!extension.empty()) longName_ = name.strip() + "." + extension.strip();
			  else longName_ = name.strip();
		  }
		  else
		  {
			  // Construct the long name for directories.
			  KString name(dentry.shortName, 11);
			  if (dentry.ntRes & 0x08) name.toLower();

			  longName_ = name.strip();
		  }
	  }
  }

  uint32_t DirectoryEntry::getOffset() const
  { return offset_; }
  KString DirectoryEntry::getNameShort() const
  { return shortName_; }
  EntryAttribute DirectoryEntry::getAttributes() const
  { return attributes_; }
  uint8_t DirectoryEntry::getNTRes() const
  { return NTRes_; }
  uint8_t DirectoryEntry::getCreationTimeMs() const
  { return creationTimeMs_; }
  uint16_t DirectoryEntry::getCreationTime() const
  { return creationTime_; }
  uint16_t DirectoryEntry::getCreationDate() const
  { return creationDate_; }
  uint16_t DirectoryEntry::getLastAccessDate() const
  { return lastAccessDate_; }
  uint16_t DirectoryEntry::getWriteTime() const
  { return writeTime_; }
  uint16_t DirectoryEntry::getWriteDate() const
  { return writeDate_; }
  uint16_t DirectoryEntry::getFirstCluster() const
  { return clusterChain_; }
  uint32_t DirectoryEntry::getFileSize() const
  { return fileSize_; }
  KString DirectoryEntry::getNameLong() const
  { return longName_; }

  DirectoryEntry::DirectoryEntry() :
	  offset_(0),
	  directoryStartCluster_(0),
	  longName_(),
	  shortName_(),
	  attributes_(EntryAttribute::Invalid),
	  NTRes_(0),
	  creationTimeMs_(0),
	  creationTime_(0),
	  creationDate_(0),
	  lastAccessDate_(0),
	  writeTime_(0),
	  writeDate_(0),
	  clusterChain_(0),
	  fileSize_(0)
  {}

  fat_dentry DirectoryEntry::getFatDentry() const
  {
	  fat_dentry d{};
	  memcpy((void*)d.shortName, (void*)shortName_.c_str(), 11);
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

  void DirectoryEntry::setFileSize(uint32_t newFileSize)
  {
	  fileSize_ = newFileSize;
  }

  uint32_t DirectoryEntry::getDirectoryCluster() const
  {
	  return directoryStartCluster_;
  }

  void DirectoryEntry::setClusterChain(uint32_t startingCluster)
  {
	  clusterChain_ = startingCluster;
  }

  ///endregion



  ///region FAT32Partition::Attribute

  EntryAttribute operator|(EntryAttribute lhs, EntryAttribute rhs)
  {
	  return static_cast<EntryAttribute>(static_cast<uint8_t>(lhs) | static_cast<uint8_t>(rhs));
  }

  EntryAttribute& operator|=(EntryAttribute& lhs, EntryAttribute rhs)
  {
	  lhs = lhs | rhs;
	  return lhs;
  }

  EntryAttribute operator&(EntryAttribute lhs, EntryAttribute rhs)
  {
	  return static_cast<EntryAttribute>(static_cast<uint8_t>(lhs) & static_cast<uint8_t>(rhs));
  }

  ///endregion


}


