/* file: PalmyraOS/core/files/partitions/MasterBootRecord.h
 * Master Boot Record (patent of Microsoft)
 *
 * References:
 * - MBR: https://en.wikipedia.org/wiki/Master_boot_record
 * - https://wiki.osdev.org/MBR_(x86)
 */

#pragma once
#include "core/definitions.h"


namespace PalmyraOS::kernel::vfs {

    /**
     * @brief The MasterBootRecord class represents the structure and operations
     * associated with the Master Boot Record (MBR) on a storage device.
     *
     * The MBR is a special type of boot sector located at the beginning of partitioned
     * storage devices. It contains information about the partitions on the disk and
     * the code to boot the operating system.
     */
    class MasterBootRecord {
    public:
        enum PartitionType : uint8_t {
            Invalid   = 0x0,
            FAT16     = 0x04,  // TODO: Not supported
            FAT16_LBA = 0x0E,  // TODO: Not supported
            FAT32     = 0x0B,  // TODO: Not supported
            FAT32_LBA = 0x0C,
            NTFS      = 0x07  // or exFat TODO: Not supported
        };

        /**
         * @brief Converts a PartitionType enum to a string representation.
         *
         * @param type The PartitionType enum value.
         * @return A string representation of the PartitionType.
         */
        PartitionType castToPartitionType(uint8_t value);

        /**
         * @brief Casts a raw uint8_t value to a PartitionType enum.
         *
         * @param value The raw uint8_t value.
         * @return The corresponding PartitionType enum.
         */
        static const char* toString(PartitionType type);

        /**
         * @brief Structure representing an entry in the MBR partition table.
         */
        struct Entry {
            bool isBootable    = false;    ///< Indicates if the partition is bootable.
            PartitionType type = Invalid;  ///< Type of the partition.
            uint32_t lbaStart  = 0x0;      ///< Start address of the partition in LBA.
            uint32_t lbaCount  = 0x0;      ///< Number of sectors in the partition.
        };

    public:
        /**
         * @brief Constructs a MasterBootRecord from a raw 512-byte sector.
         *
         * @param masterSector Pointer to the raw 512-byte MBR sector data.
         */
        explicit MasterBootRecord(const uint8_t* masterSector);

        /**
         * @brief Checks if the MBR is valid.
         *
         * @return true if the MBR is valid, false otherwise.
         */
        [[nodiscard]] bool isValid() const;

        /**
         * @brief Retrieves an entry from the partition table.
         *
         * This function returns the specified partition table entry. If the
         * entry number is out of bounds, it returns an invalid entry.
         *
         * @param entryNumber The index of the entry to retrieve (0-3).
         * @return The requested partition table entry.
         */
        Entry getEntry(uint8_t entryNumber);

    private:
        Entry entries_[4]{};  ///< Array of partition table entries.
        bool isValid_;        ///< Indicates if the MBR is valid.
    };

}  // namespace PalmyraOS::kernel::vfs