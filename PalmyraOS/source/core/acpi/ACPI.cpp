#include "core/acpi/ACPI.h"
#include "core/peripherals/Logger.h"
#include <cstring>

namespace PalmyraOS::kernel {

    // Static member initialization
    bool ACPI::initialized_       = false;
    uint8_t ACPI::version_        = 0;
    const acpi::RSDP* ACPI::rsdp_ = nullptr;
    const acpi::RSDT* ACPI::rsdt_ = nullptr;
    const acpi::XSDT* ACPI::xsdt_ = nullptr;
    const acpi::MADT* ACPI::madt_ = nullptr;
    const acpi::FADT* ACPI::fadt_ = nullptr;
    const acpi::HPET* ACPI::hpet_ = nullptr;
    const acpi::MCFG* ACPI::mcfg_ = nullptr;

    bool ACPI::initialize(const uint8_t* rsdpAddress) {
        if (initialized_) {
            LOG_WARN("ACPI already initialized");
            return true;
        }

        if (rsdpAddress == nullptr) {
            LOG_ERROR("ACPI: Invalid RSDP address (nullptr)");
            return false;
        }

        // Cast to RSDP structure
        rsdp_ = reinterpret_cast<const acpi::RSDP*>(rsdpAddress);

        // Validate RSDP
        if (!rsdp_->validate()) {
            LOG_ERROR("ACPI: RSDP validation failed");
            return false;
        }

        version_ = rsdp_->revision;
        LOG_INFO("ACPI: RSDP validated (Revision %u)", version_);

        // Get RSDT or XSDT based on ACPI version
        if (rsdp_->isACPI2Plus() && rsdp_->xsdtAddress != 0) {
            // ACPI 2.0+ - use XSDT (64-bit pointers)
            xsdt_ = reinterpret_cast<const acpi::XSDT*>(static_cast<uintptr_t>(rsdp_->xsdtAddress));

            if (!xsdt_->header.validate()) {
                LOG_ERROR("ACPI: XSDT validation failed");
                return false;
            }

            LOG_INFO("ACPI: Using XSDT at 0x%llX", rsdp_->xsdtAddress);
        }
        else {
            // ACPI 1.0 - use RSDT (32-bit pointers)
            rsdt_ = reinterpret_cast<const acpi::RSDT*>(static_cast<uintptr_t>(rsdp_->rsdtAddress));

            if (!rsdt_->header.validate()) {
                LOG_ERROR("ACPI: RSDT validation failed");
                return false;
            }

            LOG_INFO("ACPI: Using RSDT at 0x%X", rsdp_->rsdtAddress);
        }

        // Parse all tables
        parseAllTables();

        initialized_ = true;
        return true;
    }

    bool ACPI::validateChecksum(const void* data, size_t length) {
        const uint8_t* bytes = reinterpret_cast<const uint8_t*>(data);
        uint8_t sum          = 0;

        for (size_t i = 0; i < length; ++i) { sum += bytes[i]; }

        return sum == 0;
    }

    void ACPI::parseAllTables() {
        if (xsdt_ != nullptr) {
            // Parse XSDT (ACPI 2.0+)
            uint32_t entryCount     = xsdt_->getEntryCount();
            const uint64_t* entries = xsdt_->getEntries();

            LOG_INFO("ACPI: Parsing %u tables from XSDT", entryCount);

            for (uint32_t i = 0; i < entryCount; ++i) {
                const auto* header = reinterpret_cast<const acpi::ACPISDTHeader*>(static_cast<uintptr_t>(entries[i]));

                if (!header->validate()) {
                    LOG_WARN("ACPI: Table %u has invalid checksum", i);
                    continue;
                }

                // Cache important tables
                if (header->matchSignature("APIC")) {
                    madt_ = reinterpret_cast<const acpi::MADT*>(header);
                    LOG_INFO("ACPI: Found MADT at 0x%llX", entries[i]);
                }
                else if (header->matchSignature("FACP")) {
                    fadt_ = reinterpret_cast<const acpi::FADT*>(header);
                    LOG_INFO("ACPI: Found FADT at 0x%llX", entries[i]);
                }
                else if (header->matchSignature("HPET")) {
                    hpet_ = reinterpret_cast<const acpi::HPET*>(header);
                    LOG_INFO("ACPI: Found HPET at 0x%llX", entries[i]);
                }
                else if (header->matchSignature("MCFG")) {
                    mcfg_ = reinterpret_cast<const acpi::MCFG*>(header);
                    LOG_INFO("ACPI: Found MCFG at 0x%llX", entries[i]);
                }
            }
        }
        else if (rsdt_ != nullptr) {
            // Parse RSDT (ACPI 1.0)
            uint32_t entryCount     = rsdt_->getEntryCount();
            const uint32_t* entries = rsdt_->getEntries();

            LOG_INFO("ACPI: Parsing %u tables from RSDT", entryCount);

            for (uint32_t i = 0; i < entryCount; ++i) {
                const auto* header = reinterpret_cast<const acpi::ACPISDTHeader*>(static_cast<uintptr_t>(entries[i]));

                if (!header->validate()) {
                    LOG_WARN("ACPI: Table %u has invalid checksum", i);
                    continue;
                }

                // Cache important tables
                if (header->matchSignature("APIC")) {
                    madt_ = reinterpret_cast<const acpi::MADT*>(header);
                    LOG_INFO("ACPI: Found MADT at 0x%X", entries[i]);
                }
                else if (header->matchSignature("FACP")) {
                    fadt_ = reinterpret_cast<const acpi::FADT*>(header);
                    LOG_INFO("ACPI: Found FADT at 0x%X", entries[i]);
                }
                else if (header->matchSignature("HPET")) {
                    hpet_ = reinterpret_cast<const acpi::HPET*>(header);
                    LOG_INFO("ACPI: Found HPET at 0x%X", entries[i]);
                }
                else if (header->matchSignature("MCFG")) {
                    mcfg_ = reinterpret_cast<const acpi::MCFG*>(header);
                    LOG_INFO("ACPI: Found MCFG at 0x%X", entries[i]);
                }
            }
        }
    }

    const acpi::ACPISDTHeader* ACPI::findTable(const char signature[4]) {
        if (!initialized_) { return nullptr; }

        if (xsdt_ != nullptr) {
            uint32_t entryCount     = xsdt_->getEntryCount();
            const uint64_t* entries = xsdt_->getEntries();

            for (uint32_t i = 0; i < entryCount; ++i) {
                const auto* header = reinterpret_cast<const acpi::ACPISDTHeader*>(static_cast<uintptr_t>(entries[i]));
                if (header->matchSignature(signature)) { return header; }
            }
        }
        else if (rsdt_ != nullptr) {
            uint32_t entryCount     = rsdt_->getEntryCount();
            const uint32_t* entries = rsdt_->getEntries();

            for (uint32_t i = 0; i < entryCount; ++i) {
                const auto* header = reinterpret_cast<const acpi::ACPISDTHeader*>(static_cast<uintptr_t>(entries[i]));
                if (header->matchSignature(signature)) { return header; }
            }
        }

        return nullptr;
    }

    void ACPI::logAllTables() {
        if (!initialized_) {
            LOG_WARN("ACPI: Not initialized");
            return;
        }

        LOG_INFO("================================================");
        LOG_INFO("ACPI Information");
        LOG_INFO("================================================");
        LOG_INFO("ACPI Version: %u.0", version_ == 0 ? 1 : version_);
        LOG_INFO("RSDP: 0x%p", rsdp_);

        if (rsdt_) { LOG_INFO("RSDT: 0x%X", rsdp_->rsdtAddress); }
        if (xsdt_) { LOG_INFO("XSDT: 0x%llX", rsdp_->xsdtAddress); }

        LOG_INFO("");
        LOG_INFO("Discovered Tables:");

        // Iterate through all tables and log them
        if (xsdt_ != nullptr) {
            uint32_t entryCount     = xsdt_->getEntryCount();
            const uint64_t* entries = xsdt_->getEntries();

            for (uint32_t i = 0; i < entryCount; ++i) {
                const auto* header = reinterpret_cast<const acpi::ACPISDTHeader*>(static_cast<uintptr_t>(entries[i]));
                char sig[5]        = {header->signature[0], header->signature[1], header->signature[2], header->signature[3], '\0'};
                bool valid         = header->validate();

                LOG_INFO("  [%s] at 0x%016llX (%u bytes) %s", sig, entries[i], header->length, valid ? "VALID" : "INVALID");
            }
        }
        else if (rsdt_ != nullptr) {
            uint32_t entryCount     = rsdt_->getEntryCount();
            const uint32_t* entries = rsdt_->getEntries();

            for (uint32_t i = 0; i < entryCount; ++i) {
                const auto* header = reinterpret_cast<const acpi::ACPISDTHeader*>(static_cast<uintptr_t>(entries[i]));
                char sig[5]        = {header->signature[0], header->signature[1], header->signature[2], header->signature[3], '\0'};
                bool valid         = header->validate();

                LOG_INFO("  [%s] at 0x%08X (%u bytes) %s", sig, entries[i], header->length, valid ? "VALID" : "INVALID");
            }
        }

        // Log detailed information for specific tables
        if (madt_) {
            LOG_INFO("");
            logMADTDetails();
        }

        if (fadt_) {
            LOG_INFO("");
            logFADTDetails();
        }

        if (hpet_) {
            LOG_INFO("");
            LOG_INFO("HPET Details:");
            LOG_INFO("  Base Address: 0x%llX", hpet_->address);
            LOG_INFO("  Minimum Tick: %u", hpet_->minimumTick);
        }

        if (mcfg_) {
            LOG_INFO("");
            LOG_INFO("MCFG Details:");
            LOG_INFO("  PCI Express Configuration Space found");
        }

        LOG_INFO("================================================");
    }

    void ACPI::logMADTDetails() {
        if (!madt_) { return; }

        LOG_INFO("MADT Details:");
        LOG_INFO("  Local APIC Address: 0x%08X", madt_->localAPICAddress);
        LOG_INFO("  Flags: 0x%08X %s", madt_->flags, madt_->hasDual8259PICs() ? "(Dual 8259 PICs present)" : "");

        // Parse MADT entries
        const uint8_t* entryPtr         = madt_->getEntriesStart();
        const uint8_t* endPtr           = entryPtr + madt_->getEntriesLength();

        uint32_t localAPICCount         = 0;
        uint32_t ioAPICCount            = 0;
        uint32_t interruptOverrideCount = 0;

        LOG_INFO("  Entries:");

        while (entryPtr < endPtr) {
            const auto* entryHeader = reinterpret_cast<const acpi::MADTEntryHeader*>(entryPtr);

            if (entryHeader->type == acpi::MADTEntryType::LocalAPIC) {
                const auto* lapic = reinterpret_cast<const acpi::MADTLocalAPIC*>(entryPtr);
                LOG_INFO("    - Local APIC: Processor %u, APIC ID %u%s", lapic->processorID, lapic->apicID, (lapic->flags & 0x01) ? " (Enabled)" : " (Disabled)");
                localAPICCount++;
            }
            else if (entryHeader->type == acpi::MADTEntryType::IOAPIC) {
                const auto* ioapic = reinterpret_cast<const acpi::MADTIOAPIC*>(entryPtr);
                LOG_INFO("    - I/O APIC: ID %u, Address 0x%08X, GSI Base %u", ioapic->ioApicID, ioapic->ioApicAddress, ioapic->globalSystemInterruptBase);
                ioAPICCount++;
            }
            else if (entryHeader->type == acpi::MADTEntryType::InterruptSourceOverride) {
                const auto* override = reinterpret_cast<const acpi::MADTInterruptOverride*>(entryPtr);
                LOG_INFO("    - Interrupt Override: IRQ %u -> GSI %u (Flags: 0x%04X)", override->source, override->globalSystemInterrupt, override->flags);
                interruptOverrideCount++;
            }

            entryPtr += entryHeader->length;
        }

        LOG_INFO("  Summary: %u Local APICs, %u I/O APICs, %u Interrupt Overrides", localAPICCount, ioAPICCount, interruptOverrideCount);
    }

    void ACPI::logFADTDetails() {
        if (!fadt_) { return; }

        LOG_INFO("FADT Details:");
        LOG_INFO("  Preferred PM Profile: %u", fadt_->preferredPMProfile);

        if (fadt_->pmTimerBlock != 0) { LOG_INFO("  PM Timer: I/O Port 0x%04X (%u-bit)", fadt_->pmTimerBlock, fadt_->pmTimerLength == 4 ? 32 : 24); }

        LOG_INFO("  SCI Interrupt: %u", fadt_->sciInterrupt);
    }

}  // namespace PalmyraOS::kernel
