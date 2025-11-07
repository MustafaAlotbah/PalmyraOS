#pragma once

#include "ACPITables.h"

namespace PalmyraOS::kernel::acpi {

    /**
     * @brief Multiple APIC Description Table (MADT)
     *
     * Describes the interrupt controllers in the system.
     * Essential for multi-core support and modern interrupt handling.
     */
    struct MADT {
        ACPISDTHeader header;       // Signature: "APIC"
        uint32_t localAPICAddress;  // Physical address of local APIC (usually 0xFEE00000)
        uint32_t flags;             // Flags (bit 0: PC-AT compatible dual 8259 PICs installed)
        // Followed by variable-length interrupt controller structures
        // uint8_t entries[];

        [[nodiscard]] const uint8_t* getEntriesStart() const { return reinterpret_cast<const uint8_t*>(this) + sizeof(MADT); }

        [[nodiscard]] uint32_t getEntriesLength() const { return header.length - sizeof(MADT); }

        [[nodiscard]] bool hasDual8259PICs() const { return (flags & 0x01) != 0; }
    } __attribute__((packed));

    /**
     * @brief MADT Entry Types
     */
    enum class MADTEntryType : uint8_t {
        LocalAPIC                = 0,  // Processor Local APIC
        IOAPIC                   = 1,  // I/O APIC
        InterruptSourceOverride  = 2,  // Interrupt Source Override
        NMISource                = 3,  // Non-Maskable Interrupt Source
        LocalAPICNMI             = 4,  // Local APIC NMI
        LocalAPICAddressOverride = 5,  // Local APIC Address Override
        ProcessorLocalx2APIC     = 9,  // Processor Local x2APIC (ACPI 4.0+)
    };

    /**
     * @brief MADT Entry Header
     */
    struct MADTEntryHeader {
        MADTEntryType type;
        uint8_t length;
    } __attribute__((packed));

    /**
     * @brief MADT Entry: Processor Local APIC
     */
    struct MADTLocalAPIC {
        MADTEntryHeader header;
        uint8_t processorID;  // ACPI Processor ID
        uint8_t apicID;       // Local APIC ID
        uint32_t flags;       // Bit 0: Processor enabled, Bit 1: Online capable
    } __attribute__((packed));

    /**
     * @brief MADT Entry: I/O APIC
     */
    struct MADTIOAPIC {
        MADTEntryHeader header;
        uint8_t ioApicID;  // I/O APIC ID
        uint8_t reserved;
        uint32_t ioApicAddress;              // Physical address of I/O APIC
        uint32_t globalSystemInterruptBase;  // GSI base for this I/O APIC
    } __attribute__((packed));

    /**
     * @brief MADT Entry: Interrupt Source Override
     */
    struct MADTInterruptOverride {
        MADTEntryHeader header;
        uint8_t bus;                     // Bus (always 0 for ISA)
        uint8_t source;                  // IRQ source
        uint32_t globalSystemInterrupt;  // GSI that this source triggers
        uint16_t flags;                  // MPS INTI flags
    } __attribute__((packed));

    /**
     * @brief Fixed ACPI Description Table (FADT)
     *
     * Contains information about fixed register blocks for power management, timers, etc.
     */
    struct FADT {
        ACPISDTHeader header;        // Signature: "FACP"
        uint32_t firmwareCtrl;       // Physical address of FACS
        uint32_t dsdt;               // Physical address of DSDT
        uint8_t reserved;            // Reserved
        uint8_t preferredPMProfile;  // Preferred power management profile
        uint16_t sciInterrupt;       // SCI interrupt vector
        uint32_t smiCommandPort;     // SMI command port
        uint8_t acpiEnable;          // Value to write to SMI_CMD to enable ACPI
        uint8_t acpiDisable;         // Value to write to SMI_CMD to disable ACPI
        uint8_t s4biosReq;           // Value to write to SMI_CMD for S4 BIOS request
        uint8_t pStateControl;       // P-state control
        uint32_t pm1aEventBlock;     // PM1a event register block
        uint32_t pm1bEventBlock;     // PM1b event register block
        uint32_t pm1aControlBlock;   // PM1a control register block
        uint32_t pm1bControlBlock;   // PM1b control register block
        uint32_t pm2ControlBlock;    // PM2 control register block
        uint32_t pmTimerBlock;       // PM Timer block
        uint32_t gpe0Block;          // GPE0 register block
        uint32_t gpe1Block;          // GPE1 register block
        uint8_t pm1EventLength;      // Length of PM1 event block
        uint8_t pm1ControlLength;    // Length of PM1 control block
        uint8_t pm2ControlLength;    // Length of PM2 control block
        uint8_t pmTimerLength;       // Length of PM Timer block (4 bytes)
        uint8_t gpe0Length;          // Length of GPE0 block
        uint8_t gpe1Length;          // Length of GPE1 block
        uint8_t gpe1Base;            // Offset within GPE number space
        uint8_t cStateControl;       // C-state control
        uint16_t worstC2Latency;     // Worst-case C2 latency
        uint16_t worstC3Latency;     // Worst-case C3 latency
        uint16_t flushSize;          // Cache flush size
        uint16_t flushStride;        // Cache flush stride
        uint8_t dutyOffset;          // Duty cycle offset in P_CNT
        uint8_t dutyWidth;           // Duty cycle width in bits
        uint8_t dayAlarm;            // RTC day alarm index
        uint8_t monthAlarm;          // RTC month alarm index
        uint8_t century;             // RTC century index
        uint16_t bootArchFlags;      // Boot architecture flags
        uint8_t reserved2;
        uint32_t flags;  // Fixed feature flags
        // ... (more fields for ACPI 2.0+)
    } __attribute__((packed));

    /**
     * @brief High Precision Event Timer (HPET)
     *
     * Describes HPET hardware for high-resolution timing.
     */
    struct HPET {
        ACPISDTHeader header;        // Signature: "HPET"
        uint32_t eventTimerBlockID;  // Hardware ID
        uint8_t addressSpaceID;      // 0 = System memory, 1 = System I/O
        uint8_t registerBitWidth;    // Bit width of register
        uint8_t registerBitOffset;   // Bit offset of register
        uint8_t reserved;            // Reserved
        uint64_t address;            // Physical address of HPET
        uint8_t hpetNumber;          // HPET sequence number
        uint16_t minimumTick;        // Minimum clock ticks
        uint8_t pageProtection;      // Page protection and OEM attribute
    } __attribute__((packed));

    /**
     * @brief Memory Mapped Configuration Space (MCFG)
     *
     * Describes PCI Express memory-mapped configuration space.
     */
    struct MCFG {
        ACPISDTHeader header;  // Signature: "MCFG"
        uint64_t reserved;
        // Followed by allocation structures
        // MCFGAllocation entries[];
    } __attribute__((packed));

    /**
     * @brief MCFG Allocation Structure
     */
    struct MCFGAllocation {
        uint64_t baseAddress;      // Base address of enhanced configuration mechanism
        uint16_t pciSegmentGroup;  // PCI segment group number
        uint8_t startBusNumber;    // Start PCI bus number
        uint8_t endBusNumber;      // End PCI bus number
        uint32_t reserved;
    } __attribute__((packed));

}  // namespace PalmyraOS::kernel::acpi
