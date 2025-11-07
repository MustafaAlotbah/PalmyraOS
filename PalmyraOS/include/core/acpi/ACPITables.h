#pragma once

#include "core/definitions.h"

namespace PalmyraOS::kernel::acpi {

    /**
     * @brief Root System Description Pointer (RSDP) - ACPI entry point
     *
     * This structure is the entry point for ACPI. It can be found either:
     * 1. Provided by bootloader (Multiboot2, UEFI)
     * 2. By scanning BIOS memory (0xE0000-0xFFFFF)
     */
    struct RSDP {
        char signature[8];     // "RSD PTR " (with trailing space)
        uint8_t checksum;      // Checksum of first 20 bytes
        char oemID[6];         // OEM ID string
        uint8_t revision;      // 0 = ACPI 1.0, 2+ = ACPI 2.0+
        uint32_t rsdtAddress;  // Physical address of RSDT (32-bit)

        // ACPI 2.0+ fields
        uint32_t length;       // Length of the table (36 bytes for ACPI 2.0+)
        uint64_t xsdtAddress;  // Physical address of XSDT (64-bit)
        uint8_t extChecksum;   // Checksum of entire table including extended fields
        uint8_t reserved[3];   // Reserved, must be zero

        [[nodiscard]] bool validate() const;
        [[nodiscard]] bool isACPI2Plus() const { return revision >= 2; }
    } __attribute__((packed));

    /**
     * @brief System Description Table Header
     *
     * Common header for all ACPI tables (RSDT, XSDT, FADT, MADT, etc.)
     */
    struct ACPISDTHeader {
        char signature[4];         // Table signature (e.g., "RSDT", "FACP", "APIC")
        uint32_t length;           // Total length of the table including header
        uint8_t revision;          // Revision of the structure
        uint8_t checksum;          // Checksum of entire table (must sum to 0)
        char oemID[6];             // OEM ID
        char oemTableID[8];        // OEM Table ID
        uint32_t oemRevision;      // OEM Revision
        uint32_t creatorID;        // Creator ID (vendor ID of utility that created table)
        uint32_t creatorRevision;  // Creator revision

        [[nodiscard]] bool validate() const;
        [[nodiscard]] bool matchSignature(const char sig[4]) const;
    } __attribute__((packed));

    /**
     * @brief Root System Description Table (RSDT) - ACPI 1.0
     *
     * Contains 32-bit physical addresses of other ACPI tables
     */
    struct RSDT {
        ACPISDTHeader header;
        // Followed by array of 32-bit pointers to other tables
        // uint32_t entries[];

        [[nodiscard]] uint32_t getEntryCount() const { return (header.length - sizeof(ACPISDTHeader)) / sizeof(uint32_t); }

        [[nodiscard]] const uint32_t* getEntries() const { return reinterpret_cast<const uint32_t*>(reinterpret_cast<const uint8_t*>(this) + sizeof(ACPISDTHeader)); }
    } __attribute__((packed));

    /**
     * @brief Extended System Description Table (XSDT) - ACPI 2.0+
     *
     * Contains 64-bit physical addresses of other ACPI tables
     */
    struct XSDT {
        ACPISDTHeader header;
        // Followed by array of 64-bit pointers to other tables
        // uint64_t entries[];

        [[nodiscard]] uint32_t getEntryCount() const { return (header.length - sizeof(ACPISDTHeader)) / sizeof(uint64_t); }

        [[nodiscard]] const uint64_t* getEntries() const { return reinterpret_cast<const uint64_t*>(reinterpret_cast<const uint8_t*>(this) + sizeof(ACPISDTHeader)); }
    } __attribute__((packed));

    static_assert(sizeof(RSDP) == 36, "RSDP must be 36 bytes");
    static_assert(sizeof(ACPISDTHeader) == 36, "ACPISDTHeader must be 36 bytes");

}  // namespace PalmyraOS::kernel::acpi
