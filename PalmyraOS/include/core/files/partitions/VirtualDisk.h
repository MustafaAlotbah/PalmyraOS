
#pragma once

#include "core/definitions.h"
#include "core/peripherals/Logger.h"


namespace PalmyraOS::kernel {


    template<typename DiskDriver>
    class VirtualDisk {
    public:
        explicit VirtualDisk(DiskDriver& diskDriver, uint32_t startSector, uint32_t countSectors) : disk_(diskDriver), startSector_(startSector), countSectors_(countSectors) {}

        bool readSector(uint32_t logicalBlockAddress, uint8_t* buffer, uint32_t timeout) {
            // LOG_DEBUG("VirtualDisk: Reading sector 0x%X", logicalBlockAddress);
            if (!isValidSector(logicalBlockAddress)) {
                LOG_ERROR("VirtualDisk: Out of bounds error 0x%X-0x%X (0x%X)", startSector_, startSector_ + countSectors_ * 512, logicalBlockAddress);
                return false;  // Out of bounds
            }
            return disk_.readSector(logicalBlockAddress, buffer, timeout);
        }

        bool writeSector(uint32_t logicalBlockAddress, const uint8_t* buffer, uint32_t timeout) {
            if (!isValidSector(logicalBlockAddress)) {
                return false;  // Out of bounds
            }
            return disk_.writeSector(logicalBlockAddress, buffer, timeout);
        }

    private:
        [[nodiscard]] bool isValidSector(uint32_t logicalBlockAddress) const { return startSector_ <= logicalBlockAddress && logicalBlockAddress < countSectors_; }

        DiskDriver& disk_;
        uint32_t startSector_;   // Starting sector of the partition
        uint32_t countSectors_;  // Total number of sectors in the partition
    };

}  // namespace PalmyraOS::kernel