
#include "core/files/Fat32FileSystem.h"
#include "core/files/VirtualFileSystemBase.h"
#include "core/peripherals/Logger.h"
#include "libs/memory.h"


///region FAT32Directory

PalmyraOS::kernel::vfs::FAT32Directory::FAT32Directory(
	FAT32Partition& parentPartition,
	uint32_t directoryStartCluster,
	InodeBase::Mode mode,
	InodeBase::UserID userId,
	InodeBase::GroupID groupId
)
	:
	InodeBase(InodeBase::Type::Directory, mode, userId, groupId),
	parentPartition_(parentPartition),
	directoryStartCluster_(directoryStartCluster)
{

}

PalmyraOS::kernel::KVector<std::pair<PalmyraOS::kernel::KString, PalmyraOS::kernel::vfs::InodeBase*>>

PalmyraOS::kernel::vfs::FAT32Directory::getDentries(size_t offset, size_t count)
{

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
	for (int i = 2; i < entries.size(); ++i)
	{
		auto& entry = entries[i];
		// Add directory
		if ((uint8_t)(entry.getAttributes() & EntryAttribute::Directory))
		{
			auto longName = entry.getNameLong();

//			LOG_DEBUG("Entry '%s': %d", longName.c_str(), directoryStartCluster_);
			// Create a test inode with a lambda function for reading test string
			auto rtcNode = kernel::heapManager.createInstance<vfs::FAT32Directory>(
				parentPartition_,
				entry.getFirstCluster(),
				getMode(),
				getUserId(),
				getGroupId()
			);
			if (!rtcNode) continue;

			InodeBase::addDentry(longName, rtcNode);
		}

		// TODO Add Archive / File

		else if ((uint8_t)(entry.getAttributes() & EntryAttribute::Archive))
		{
			auto longName = entry.getNameLong();
			// Create a test inode with a lambda function for reading test string
			auto rtcNode  = kernel::heapManager.createInstance<vfs::FAT32Archive>(
				parentPartition_,
				entry,
				getMode(),
				getUserId(),
				getGroupId()
			);
			if (!rtcNode) continue;

			InodeBase::addDentry(longName, rtcNode);
		}

	}

	return InodeBase::getDentries(offset, count);
}

PalmyraOS::kernel::vfs::InodeBase* PalmyraOS::kernel::vfs::FAT32Directory::getDentry(const PalmyraOS::kernel::KString& name)
{
	// Just update in case it is empty which is clearl not as expected.
	if (dentries_.empty()) getDentries(0, 100);

	return InodeBase::getDentry(name);
}

///endregion


///region FAT32Archive

PalmyraOS::kernel::vfs::FAT32Archive::FAT32Archive(
	PalmyraOS::kernel::vfs::FAT32Partition& parentPartition,
	const DirectoryEntry& directoryEntry,
	PalmyraOS::kernel::vfs::InodeBase::Mode mode,
	PalmyraOS::kernel::vfs::InodeBase::UserID userId,
	PalmyraOS::kernel::vfs::InodeBase::GroupID groupId
)
	:
	InodeBase(InodeBase::Type::File, mode, userId, groupId),
	parentPartition_(parentPartition),
	directoryEntry_(directoryEntry)
{
	// TODO add other
	InodeBase::size_ = directoryEntry.getFileSize();
}

size_t PalmyraOS::kernel::vfs::FAT32Archive::read(char* buffer, size_t size, size_t offset)
{
	// If the offset is greater than or equal to the file size, return 0 as there's nothing to read
	if (offset >= getSize()) return 0;

	// Read the entire file data starting from the beginning
	KVector<uint8_t> data = parentPartition_.readFile(directoryEntry_.getFirstCluster(), offset, size);

	// Calculate the number of bytes to read
	size_t bytesToRead = std::min(size, data.size());

	// Copy the relevant data into the buffer
	memcpy(buffer, data.data(), bytesToRead);

	return bytesToRead;
}

///endregion
