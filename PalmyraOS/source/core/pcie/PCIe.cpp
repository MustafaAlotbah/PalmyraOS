#include "core/pcie/PCIe.h"
#include "core/acpi/ACPI.h"
#include "core/acpi/ACPISpecific.h"
#include "core/peripherals/Logger.h"

namespace PalmyraOS::kernel {

    // Static member initialization
    bool PCIe::initialized_      = false;
    uintptr_t PCIe::baseAddress_ = 0;
    uint16_t PCIe::segmentGroup_ = 0;
    uint8_t PCIe::startBus_      = 0;
    uint8_t PCIe::endBus_        = 0;
    uint32_t PCIe::deviceCount_  = 0;

    bool PCIe::initialize() {
        if (initialized_) {
            LOG_WARN("PCIe: Already initialized");
            return true;
        }

        if (!ACPI::isInitialized()) {
            LOG_ERROR("PCIe: ACPI not initialized");
            return false;
        }

        // Get MCFG table from ACPI
        const auto* mcfg = ACPI::getMCFG();
        if (!mcfg) {
            LOG_ERROR("PCIe: MCFG table not found in ACPI");
            return false;
        }

        // Parse first allocation entry (most systems have only one)
        uint32_t headerSize    = sizeof(acpi::ACPISDTHeader) + sizeof(uint64_t);
        const auto* allocation = reinterpret_cast<const acpi::MCFGAllocation*>(reinterpret_cast<const uint8_t*>(mcfg) + headerSize);

        baseAddress_           = static_cast<uintptr_t>(allocation->baseAddress);
        segmentGroup_          = allocation->pciSegmentGroup;
        startBus_              = allocation->startBusNumber;
        endBus_                = allocation->endBusNumber;

        LOG_INFO("PCIe: Initializing Enhanced Configuration Access Mechanism (ECAM)");
        LOG_INFO("PCIe: Base Address: 0x%p", (void*) baseAddress_);
        LOG_INFO("PCIe: Segment Group: %u", segmentGroup_);
        LOG_INFO("PCIe: Bus Range: %u-%u (%u buses)", startBus_, endBus_, (endBus_ - startBus_) + 1);

        initialized_ = true;
        return true;
    }

    volatile uint32_t* PCIe::getConfigAddress(uint8_t bus, uint8_t device, uint8_t function, uint16_t offset) {
        if (!initialized_) { return nullptr; }

        if (bus < startBus_ || bus > endBus_) { return nullptr; }

        if (device >= 32 || function >= 8) { return nullptr; }

        // PCIe ECAM address calculation:
        // Address = BaseAddress + (Bus << 20 | Device << 15 | Function << 12 | Offset)
        uintptr_t address = baseAddress_ + (static_cast<uintptr_t>(bus) << 20) + (static_cast<uintptr_t>(device) << 15) + (static_cast<uintptr_t>(function) << 12) +
                            (offset & 0xFFC);  // Ensure 4-byte alignment

        return reinterpret_cast<volatile uint32_t*>(address);
    }

    uint32_t PCIe::readConfig32(uint8_t bus, uint8_t device, uint8_t function, uint16_t offset) {
        volatile uint32_t* address = getConfigAddress(bus, device, function, offset);
        if (!address) return 0xFFFFFFFF;

        return *address;
    }

    void PCIe::writeConfig32(uint8_t bus, uint8_t device, uint8_t function, uint16_t offset, uint32_t value) {
        volatile uint32_t* address = getConfigAddress(bus, device, function, offset);
        if (!address) return;

        *address = value;
    }

    uint16_t PCIe::readConfig16(uint8_t bus, uint8_t device, uint8_t function, uint16_t offset) {
        uint32_t value = readConfig32(bus, device, function, offset & 0xFFFC);

        if ((offset & 2) != 0) return static_cast<uint16_t>(value >> 16);
        return static_cast<uint16_t>(value & 0xFFFF);
    }

    void PCIe::writeConfig16(uint8_t bus, uint8_t device, uint8_t function, uint16_t offset, uint16_t value) {
        uint32_t aligned_offset = offset & 0xFFFC;
        uint32_t old_value      = readConfig32(bus, device, function, aligned_offset);

        if ((offset & 2) != 0) { old_value = (old_value & 0x0000FFFF) | (static_cast<uint32_t>(value) << 16); }
        else { old_value = (old_value & 0xFFFF0000) | value; }

        writeConfig32(bus, device, function, aligned_offset, old_value);
    }

    uint8_t PCIe::readConfig8(uint8_t bus, uint8_t device, uint8_t function, uint16_t offset) {
        uint32_t value = readConfig32(bus, device, function, offset & 0xFFFC);
        return static_cast<uint8_t>((value >> ((offset & 3) * 8)) & 0xFF);
    }

    void PCIe::writeConfig8(uint8_t bus, uint8_t device, uint8_t function, uint16_t offset, uint8_t value) {
        uint32_t aligned_offset = offset & 0xFFFC;
        uint32_t old_value      = readConfig32(bus, device, function, aligned_offset);

        uint8_t byte_offset     = offset & 3;
        uint32_t mask           = 0xFF << (byte_offset * 8);
        old_value               = (old_value & ~mask) | (static_cast<uint32_t>(value) << (byte_offset * 8));

        writeConfig32(bus, device, function, aligned_offset, old_value);
    }

    bool PCIe::deviceExists(uint8_t bus, uint8_t device, uint8_t function) {
        uint16_t vendorID = readConfig16(bus, device, function, 0);
        return vendorID != 0xFFFF && vendorID != 0x0000;
    }

    const char* PCIe::getClassName(uint8_t classCode) {
        switch (classCode) {
            case 0x00: return "Unclassified";
            case 0x01: return "Mass Storage Controller";
            case 0x02: return "Network Controller";
            case 0x03: return "Display Controller";
            case 0x04: return "Multimedia Controller";
            case 0x05: return "Memory Controller";
            case 0x06: return "Bridge Device";
            case 0x07: return "Simple Communication Controller";
            case 0x08: return "Base System Peripheral";
            case 0x09: return "Input Device Controller";
            case 0x0A: return "Docking Station";
            case 0x0B: return "Processor";
            case 0x0C: return "Serial Bus Controller";
            case 0x0D: return "Wireless Controller";
            case 0x0E: return "Intelligent Controller";
            case 0x0F: return "Satellite Communication Controller";
            case 0x10: return "Encryption Controller";
            case 0x11: return "Signal Processing Controller";
            case 0x12: return "Processing Accelerator";
            case 0x13: return "Non-Essential Instrumentation";
            default: return "Unknown";
        }
    }

    void PCIe::enumerateDevices() {
        if (!initialized_) {
            LOG_ERROR("PCIe: Not initialized, cannot enumerate devices");
            return;
        }

        LOG_INFO("PCIe: Enumerating devices...");
        deviceCount_ = 0;

        for (uint16_t bus = startBus_; bus <= endBus_; ++bus) {
            for (uint8_t device = 0; device < 32; ++device) {
                // Check function 0 first
                if (!deviceExists(bus, device, 0)) { continue; }

                // Read device information
                uint16_t vendorID  = readConfig16(bus, device, 0, 0x00);
                uint16_t deviceID  = readConfig16(bus, device, 0, 0x02);
                uint8_t headerType = readConfig8(bus, device, 0, 0x0E);
                uint8_t classCode  = readConfig8(bus, device, 0, 0x0B);
                uint8_t subclass   = readConfig8(bus, device, 0, 0x0A);

                LOG_INFO("PCIe: [%02X:%02X.0] VID:0x%04X DID:0x%04X Class:0x%02X.%02X (%s)", bus, device, vendorID, deviceID, classCode, subclass, getClassName(classCode));

                deviceCount_++;

                // Check for multi-function device
                bool isMultiFunction = (headerType & 0x80) != 0;
                if (isMultiFunction) {
                    for (uint8_t function = 1; function < 8; ++function) {
                        if (deviceExists(bus, device, function)) {
                            vendorID  = readConfig16(bus, device, function, 0x00);
                            deviceID  = readConfig16(bus, device, function, 0x02);
                            classCode = readConfig8(bus, device, function, 0x0B);
                            subclass  = readConfig8(bus, device, function, 0x0A);

                            LOG_INFO("PCIe: [%02X:%02X.%u] VID:0x%04X DID:0x%04X Class:0x%02X.%02X (%s)",
                                     bus,
                                     device,
                                     function,
                                     vendorID,
                                     deviceID,
                                     classCode,
                                     subclass,
                                     getClassName(classCode));

                            deviceCount_++;
                        }
                    }
                }
            }
        }

        LOG_INFO("PCIe: Found %u devices", deviceCount_);
    }

}  // namespace PalmyraOS::kernel
