#include "core/acpi/ACPITables.h"
#include "libs/string.h"

namespace PalmyraOS::kernel::acpi {

    bool RSDP::validate() const {
        // Check signature
        if (strncmp(signature, "RSD PTR ", 8) != 0) { return false; }

        // Validate ACPI 1.0 checksum (first 20 bytes)
        const uint8_t* bytes = reinterpret_cast<const uint8_t*>(this);
        uint8_t sum          = 0;
        for (size_t i = 0; i < 20; ++i) { sum += bytes[i]; }

        if (sum != 0) { return false; }

        // If ACPI 2.0+, validate extended checksum
        if (revision >= 2) {
            sum = 0;
            for (size_t i = 0; i < sizeof(RSDP); ++i) { sum += bytes[i]; }

            if (sum != 0) { return false; }
        }

        return true;
    }

    bool ACPISDTHeader::validate() const {
        // Validate checksum
        const uint8_t* bytes = reinterpret_cast<const uint8_t*>(this);
        uint8_t sum          = 0;
        for (uint32_t i = 0; i < length; ++i) { sum += bytes[i]; }

        return sum == 0;
    }

    bool ACPISDTHeader::matchSignature(const char sig[4]) const { return strncmp(signature, sig, 4) == 0; }

}  // namespace PalmyraOS::kernel::acpi
