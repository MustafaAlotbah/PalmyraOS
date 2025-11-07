#include "core/acpi/PowerManagement.h"
#include "core/acpi/ACPI.h"
#include "core/peripherals/Logger.h"
#include "libs/string.h"

// Inline I/O port functions
namespace {
    inline void outb(uint16_t port, uint8_t value) { asm volatile("outb %0, %1" : : "a"(value), "Nd"(port)); }

    inline uint8_t inb(uint16_t port) {
        uint8_t result;
        asm volatile("inb %1, %0" : "=a"(result) : "Nd"(port));
        return result;
    }

    inline void outw(uint16_t port, uint16_t value) { asm volatile("outw %0, %1" : : "a"(value), "Nd"(port)); }
}  // namespace

namespace PalmyraOS::kernel {

    // Static member initialization
    bool PowerManagement::initialized_             = false;
    bool PowerManagement::hasResetReg_             = false;
    uint8_t PowerManagement::resetRegAddressSpace_ = 0;
    uint8_t PowerManagement::resetRegBitWidth_     = 0;
    uint8_t PowerManagement::resetRegBitOffset_    = 0;
    uint64_t PowerManagement::resetRegAddress_     = 0;
    uint8_t PowerManagement::resetValue_           = 0;
    uint32_t PowerManagement::pm1aControlBlock_    = 0;
    uint16_t PowerManagement::slp5TypeA_           = 0;
    uint16_t PowerManagement::slp5TypeB_           = 0;
    bool PowerManagement::hasS5_                   = false;

    bool PowerManagement::initialize() {
        if (initialized_) {
            LOG_WARN("PowerManagement: Already initialized");
            return true;
        }

        if (!ACPI::isInitialized()) {
            LOG_ERROR("PowerManagement: ACPI not initialized");
            return false;
        }

        LOG_INFO("PowerManagement: Initializing...");

        // Parse FADT
        parseFADT();

        // Parse DSDT for _S5 package
        parseS5FromDSDT();

        initialized_ = true;

        // Log capabilities
        LOG_INFO("PowerManagement: Capabilities:");
        if (hasResetReg_) { LOG_INFO("  - ACPI Reset: Available (Register: 0x%llX, Value: 0x%02X)", resetRegAddress_, resetValue_); }
        else { LOG_INFO("  - ACPI Reset: Not available (will use legacy)"); }

        if (hasS5_) { LOG_INFO("  - ACPI Shutdown: Available (SLP_TYPa=0x%X, SLP_TYPb=0x%X)", slp5TypeA_, slp5TypeB_); }
        else { LOG_INFO("  - ACPI Shutdown: Not available (will use legacy)"); }

        return true;
    }

    void PowerManagement::parseFADT() {
        const acpi::FADT* fadt = ACPI::getFADT();
        if (!fadt) {
            LOG_WARN("PowerManagement: FADT not found");
            return;
        }

        // Get PM1a control block
        pm1aControlBlock_ = fadt->pm1aControlBlock;
        LOG_DEBUG("PowerManagement: PM1a Control Block at I/O 0x%04X", pm1aControlBlock_);

        // Check for ACPI 2.0+ extended reset register (offset 116+ in FADT)
        // FADT size must be at least 129 bytes to have reset register
        if (fadt->header.length >= 129) {
            // Reset register is at offset 116 (12 bytes structure)
            const uint8_t* fadtBytes = reinterpret_cast<const uint8_t*>(fadt);

            resetRegAddressSpace_    = fadtBytes[116];  // Address space ID
            resetRegBitWidth_        = fadtBytes[117];  // Bit width
            resetRegBitOffset_       = fadtBytes[118];  // Bit offset
            uint8_t accessSize       = fadtBytes[119];  // Access size

            // Address is 64-bit at offset 120
            resetRegAddress_         = *reinterpret_cast<const uint64_t*>(&fadtBytes[120]);

            // Reset value is at offset 128
            resetValue_              = fadtBytes[128];
            LOG_DEBUG("PowerManagement: Reset register found (Space:%u, Width:%u, Offset:%u, Access:%u, Address:0x%llX, Value:0x%02X)",
                      resetRegAddressSpace_,
                      resetRegBitWidth_,
                      resetRegBitOffset_,
                      accessSize,
                      resetRegAddress_,
                      resetValue_);

            // Validate reset register
            if (resetRegAddress_ != 0 && resetValue_ != 0) {
                hasResetReg_ = true;
                LOG_DEBUG("PowerManagement: Reset register found (Space:%u, Addr:0x%llX, Value:0x%02X)", resetRegAddressSpace_, resetRegAddress_, resetValue_);
            }
        }
    }

    void PowerManagement::parseS5FromDSDT() {
        const acpi::FADT* fadt = ACPI::getFADT();
        if (!fadt || fadt->dsdt == 0) {
            LOG_WARN("PowerManagement: DSDT not available");
            return;
        }

        // Get DSDT
        const auto* dsdt = reinterpret_cast<const acpi::ACPISDTHeader*>(static_cast<uintptr_t>(fadt->dsdt));
        if (!dsdt->validate()) {
            LOG_ERROR("PowerManagement: DSDT validation failed");
            return;
        }

        LOG_DEBUG("PowerManagement: Parsing DSDT for _S5 package (size: %u bytes)", dsdt->length);

        // Simple _S5 parser (looks for "_S5_" signature followed by package)
        // This is a simplified AML parser - just enough to find _S5
        const uint8_t* dsdtData = reinterpret_cast<const uint8_t*>(dsdt);
        uint32_t dsdtLength     = dsdt->length;

        // Search for "_S5_" signature
        for (uint32_t i = 36; i < dsdtLength - 5; ++i) {  // Start after header (36 bytes)
            if (dsdtData[i] == '_' && dsdtData[i + 1] == 'S' && dsdtData[i + 2] == '5' && dsdtData[i + 3] == '_') {

                LOG_DEBUG("PowerManagement: Found _S5 at offset %u", i);

                // After "_S5_", expect a Package opcode (0x12)
                uint32_t j = i + 4;

                // Skip whitespace/scope operators
                while (j < dsdtLength && (dsdtData[j] == 0x00 || dsdtData[j] == 0x08)) { j++; }

                if (j >= dsdtLength) continue;

                // Check for Package opcode (0x12)
                if (dsdtData[j] == 0x12) {
                    j++;  // Skip package opcode

                    // Package length encoding (skip)
                    uint8_t pkgLeadByte = dsdtData[j++];
                    if ((pkgLeadByte & 0xC0) == 0x40) { j++; }          // 2-byte length
                    else if ((pkgLeadByte & 0xC0) == 0x80) { j += 2; }  // 3-byte length
                    else if ((pkgLeadByte & 0xC0) == 0xC0) { j += 3; }  // 4-byte length

                    // Number of elements
                    uint8_t numElements = dsdtData[j++];

                    if (numElements >= 2 && j + 2 < dsdtLength) {
                        // First element: SLP_TYPa (usually ByteConst 0x0A prefix)
                        if (dsdtData[j] == 0x0A) {  // ByteConst
                            slp5TypeA_ = dsdtData[j + 1];
                            j += 2;
                        }
                        else { slp5TypeA_ = dsdtData[j++]; }

                        // Second element: SLP_TYPb
                        if (dsdtData[j] == 0x0A) {  // ByteConst
                            slp5TypeB_ = dsdtData[j + 1];
                        }
                        else { slp5TypeB_ = dsdtData[j]; }

                        hasS5_ = true;
                        LOG_INFO("PowerManagement: _S5 package found (SLP_TYPa=0x%X, SLP_TYPb=0x%X)", slp5TypeA_, slp5TypeB_);
                        return;
                    }
                }
            }
        }

        LOG_WARN("PowerManagement: _S5 package not found in DSDT");
    }

    [[noreturn]] void PowerManagement::reboot() {
        LOG_INFO("PowerManagement: Rebooting system...");

        // Method 1: ACPI Reset Register
        if (hasResetReg_) {
            LOG_INFO("PowerManagement: Attempting ACPI reset...");
            acpiReset();
            // If we're still here, ACPI reset failed
        }

        // Method 2: Keyboard Controller Reset
        LOG_INFO("PowerManagement: Attempting keyboard controller reset...");
        keyboardReset();

        // Method 3: Triple Fault (last resort)
        LOG_WARN("PowerManagement: Attempting triple fault...");
        tripleFault();
    }

    [[noreturn]] void PowerManagement::shutdown() {
        LOG_INFO("PowerManagement: Shutting down system...");

        // Method 1: ACPI Shutdown (S5 state)
        if (hasS5_ && pm1aControlBlock_ != 0) {
            LOG_INFO("PowerManagement: Attempting ACPI shutdown...");
            acpiShutdown();
            // If we're still here, ACPI shutdown failed
        }

        // Method 2: APM Shutdown (legacy)
        LOG_INFO("PowerManagement: Attempting APM shutdown...");
        apmShutdown();

        // Method 3: Halt (if all else fails)
        LOG_WARN("PowerManagement: All shutdown methods failed, halting CPU");
        while (true) { asm volatile("cli; hlt"); }
    }

    bool PowerManagement::sleep(uint8_t sleepState) {
        LOG_INFO("PowerManagement: Sleep state S%u not yet implemented", sleepState);
        return false;
    }

    [[noreturn]] void PowerManagement::acpiReset() {
        if (resetRegAddressSpace_ == 1) {  // System I/O space
            uint16_t port = static_cast<uint16_t>(resetRegAddress_);
            LOG_DEBUG("PowerManagement: Writing 0x%02X to I/O port 0x%04X", resetValue_, port);
            outb(port, resetValue_);
        }
        else if (resetRegAddressSpace_ == 0) {  // System Memory space
            LOG_DEBUG("PowerManagement: Writing 0x%02X to memory 0x%llX", resetValue_, resetRegAddress_);
            auto* resetPtr = reinterpret_cast<volatile uint8_t*>(resetRegAddress_);
            *resetPtr      = resetValue_;
        }

        // Wait a moment for reset to take effect
        for (volatile int i = 0; i < 1000000; ++i) {}

        // If we're still here, reset failed - fall through to next method
        LOG_WARN("PowerManagement: ACPI reset failed");
        keyboardReset();
    }

    [[noreturn]] void PowerManagement::acpiShutdown() {
        // Disable interrupts
        asm volatile("cli");

        // PM1a control register format:
        // Bits 10-12: SLP_TYPa (sleep type)
        // Bit 13: SLP_EN (sleep enable)
        uint16_t pm1aValue = (slp5TypeA_ << 10) | (1 << 13);  // SLP_TYPa | SLP_EN

        LOG_DEBUG("PowerManagement: Writing 0x%04X to PM1a control (I/O 0x%04X)", pm1aValue, pm1aControlBlock_);

        outw(pm1aControlBlock_, pm1aValue);

        // Wait for shutdown to take effect
        for (volatile int i = 0; i < 1000000; ++i) {}

        // If we're still here, ACPI shutdown failed
        LOG_WARN("PowerManagement: ACPI shutdown failed");
        apmShutdown();
    }

    [[noreturn]] void PowerManagement::keyboardReset() {
        // Method: Pulse reset line via keyboard controller (port 0x64)
        LOG_DEBUG("PowerManagement: Pulsing keyboard controller reset line");

        // Wait for keyboard controller to be ready
        for (int i = 0; i < 1000; ++i) {
            if ((inb(0x64) & 0x02) == 0) break;
            for (volatile int j = 0; j < 1000; ++j) {}
        }

        // Send reset command (0xFE) to keyboard controller
        outb(0x64, 0xFE);

        // Wait for reset
        for (volatile int i = 0; i < 1000000; ++i) {}

        // If still here, keyboard reset failed
        LOG_WARN("PowerManagement: Keyboard reset failed");
        tripleFault();
    }

    [[noreturn]] void PowerManagement::apmShutdown() {
        // APM 1.1+ shutdown
        // Set APM version to 1.1
        asm volatile("mov $0x5301, %%ax\n"  // APM Installation Check
                     "xor %%bx, %%bx\n"     // APM BIOS (0x0000)
                     "int $0x15\n"
                     :
                     :
                     : "ax", "bx", "cx");

        // Connect APM interface
        asm volatile("mov $0x5303, %%ax\n"  // APM Connect Interface
                     "xor %%bx, %%bx\n"     // APM BIOS (0x0000)
                     "int $0x15\n"
                     :
                     :
                     : "ax", "bx");

        // Set APM version to 1.1
        asm volatile("mov $0x530E, %%ax\n"  // APM Set Version
                     "xor %%bx, %%bx\n"     // APM BIOS (0x0000)
                     "mov $0x0101, %%cx\n"  // Version 1.1
                     "int $0x15\n"
                     :
                     :
                     : "ax", "bx", "cx");

        // Power off
        asm volatile("mov $0x5307, %%ax\n"  // APM Set Power State
                     "mov $0x0001, %%bx\n"  // All devices
                     "mov $0x0003, %%cx\n"  // Power state: Off
                     "int $0x15\n"
                     :
                     :
                     : "ax", "bx", "cx");

        // If we're still here, APM shutdown failed
        LOG_WARN("PowerManagement: APM shutdown failed, halting");
        while (true) { asm volatile("cli; hlt"); }
    }

    [[noreturn]] void PowerManagement::tripleFault() {
        // Cause a triple fault by loading an invalid IDT
        LOG_DEBUG("PowerManagement: Triggering triple fault");

        struct {
            uint16_t limit;
            uint32_t base;
        } __attribute__((packed)) invalidIDT = {0, 0};

        asm volatile("lidt %0\n"
                     "int $0x03\n"  // Trigger interrupt with invalid IDT
                     :
                     : "m"(invalidIDT));

        // Should never reach here
        while (true) { asm volatile("hlt"); }
    }

}  // namespace PalmyraOS::kernel
