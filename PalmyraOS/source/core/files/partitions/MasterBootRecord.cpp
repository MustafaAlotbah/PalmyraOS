
#include "core/files/partitions/MasterBootRecord.h"
#include "core/peripherals/Logger.h"


PalmyraOS::kernel::vfs::MasterBootRecord::MasterBootRecord(const uint8_t* masterSector)
	: isValid_(false)
{
	/* Sector Length:			0x200 (512) bytes
	 * Bootstrap (binary code) 	0
	 * First Entry:				0x1BE
	 * Second Entry:			0x1CE
	 * Third Entry:				0x1DE
	 * Fourth Entry:			0x1EE
	 * Signature 1:				0x1FE
	 * Signature 2:				0x1FF
	 */

	// Signature Check (bytes 510 and 511 must be 0x55 and 0xAA)
	if (masterSector[0x1FE] != 0x55 || masterSector[0x1FF] != 0xAA)
	{
		LOG_ERROR("Not an MBR boot sector!");
		isValid_ = false;
	}

	/**
	 * Entry:
	 * is bootable?		7-th bit of 0
	 * Partition Type	0x04
	 */

	// Get a pointer to the first partition entry (starts at offset 0x1BE)
	const uint8_t* raw_entry = masterSector + 0x1BE;

	// Decode partition entries
	for (auto& entry : entries_)
	{
		entry.type = castToPartitionType(raw_entry[0x04]);

		// Discard if invalid
		if (entry.type != Invalid)
		{
			entry.isBootable = raw_entry[0] & (1 << 7);
			entry.lbaStart   = raw_entry[0x8] | (raw_entry[0x9] << 8) | (raw_entry[0xA] << 16) | (raw_entry[0xB] << 24);
			entry.lbaCount   = raw_entry[0xC] | (raw_entry[0xD] << 8) | (raw_entry[0xE] << 16) | (raw_entry[0xF] << 24);
		}

		// Move to the next partition entry
		raw_entry += 16;
	}

	isValid_ = true;
}

bool PalmyraOS::kernel::vfs::MasterBootRecord::isValid() const
{
	return isValid_;
}

PalmyraOS::kernel::vfs::MasterBootRecord::PartitionType PalmyraOS::kernel::vfs::MasterBootRecord::castToPartitionType(
	uint8_t value
)
{
	switch (value)
	{
		case FAT16:
		case FAT16_LBA:
		case FAT32:
		case FAT32_LBA:
		case NTFS:
			return static_cast<PartitionType>(value);
		default:
			return Invalid;
	}
}

const char* PalmyraOS::kernel::vfs::MasterBootRecord::toString(PalmyraOS::kernel::vfs::MasterBootRecord::PartitionType type)
{
	switch (type)
	{
		case Invalid:
			return "Invalid";
		case FAT16:
			return "FAT16";
		case FAT16_LBA:
			return "FAT16 LBA";
		case FAT32:
			return "FAT32";
		case FAT32_LBA:
			return "FAT32 LBA";
		case NTFS:
			return "NTFS";
		default:
			return "Unknown";
	}
}

PalmyraOS::kernel::vfs::MasterBootRecord::Entry PalmyraOS::kernel::vfs::MasterBootRecord::getEntry(uint8_t entryNumber)
{
	if (entryNumber >= 4) return Entry{};

	return entries_[entryNumber];

}
