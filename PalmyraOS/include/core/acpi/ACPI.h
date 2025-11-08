#pragma once

#include "ACPISpecific.h"
#include "ACPITables.h"

namespace PalmyraOS::kernel {

    /**
     * @class ACPI
     * @brief Advanced Configuration and Power Interface (ACPI) manager
     *
     * This class provides access to ACPI tables for hardware discovery,
     * power management, and system configuration.
     *
     * Features:
     * - Parses RSDP, RSDT/XSDT
     * - Discovers and validates all ACPI tables
     * - Provides access to specific tables (MADT, FADT, HPET, MCFG)
     * - Supports both ACPI 1.0 and 2.0+
     */
    class ACPI {
    public:
        /**
         * @brief Initialize ACPI from RSDP pointer
         *
         * @param rsdpAddress Physical address of RSDP (from Multiboot2 or BIOS scan)
         * @return True if ACPI was successfully initialized, false otherwise
         */
        static bool initialize(const uint8_t* rsdpAddress);

        /**
         * @brief Check if ACPI is initialized
         * @return True if ACPI is initialized, false otherwise
         */
        [[nodiscard]] static bool isInitialized() { return initialized_; }

        /**
         * @brief Get ACPI version
         * @return 1 for ACPI 1.0, 2+ for ACPI 2.0+
         */
        [[nodiscard]] static uint8_t getACPIVersion() { return version_; }

        /**
         * @brief Get the Multiple APIC Description Table (MADT)
         * @return Pointer to MADT, or nullptr if not found
         */
        [[nodiscard]] static const acpi::MADT* getMADT() { return madt_; }

        /**
         * @brief Get the Fixed ACPI Description Table (FADT)
         * @return Pointer to FADT, or nullptr if not found
         */
        [[nodiscard]] static const acpi::FADT* getFADT() { return fadt_; }

        /**
         * @brief Get the High Precision Event Timer table (HPET)
         * @return Pointer to HPET, or nullptr if not found
         */
        [[nodiscard]] static const acpi::HPET* getHPET() { return hpet_; }

        /**
         * @brief Get the Memory Mapped Configuration table (MCFG)
         * @return Pointer to MCFG, or nullptr if not found
         */
        [[nodiscard]] static const acpi::MCFG* getMCFG() { return mcfg_; }

        /**
         * @brief Find a table by signature
         *
         * @param signature 4-character signature (e.g., "APIC", "FACP", "HPET")
         * @return Pointer to table header, or nullptr if not found
         */
        [[nodiscard]] static const acpi::ACPISDTHeader* findTable(const char signature[4]);

        /**
         * @brief Log all discovered ACPI tables and their details
         */
        static void logAllTables();

        /**
         * @brief Log complete ACPI table header information
         */
        static void logTableHeader(const acpi::ACPISDTHeader* header, uint64_t address);

        /**
         * @brief Log detailed MADT information
         */
        static void logMADTDetails();

        /**
         * @brief Log detailed FADT information
         */
        static void logFADTDetails();

        /**
         * @brief Log detailed HPET information
         */
        static void logHPETDetails();

        /**
         * @brief Log detailed MCFG information
         */
        static void logMCFGDetails();

    private:
        // Initialization state
        static bool initialized_;
        static uint8_t version_;

        // Core ACPI structures
        static const acpi::RSDP* rsdp_;
        static const acpi::RSDT* rsdt_;
        static const acpi::XSDT* xsdt_;

        // Cached important tables
        static const acpi::MADT* madt_;
        static const acpi::FADT* fadt_;
        static const acpi::HPET* hpet_;
        static const acpi::MCFG* mcfg_;

        /**
         * @brief Validate a checksum for a memory region
         *
         * @param data Pointer to data
         * @param length Length of data in bytes
         * @return True if checksum is valid (sum == 0), false otherwise
         */
        static bool validateChecksum(const void* data, size_t length);

        /**
         * @brief Parse all tables from RSDT or XSDT
         */
        static void parseAllTables();

        /**
         * @brief Parse MADT entries (Local APIC, I/O APIC, etc.)
         */
        static void parseMADTEntries();
    };

}  // namespace PalmyraOS::kernel
