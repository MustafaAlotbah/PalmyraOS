
#pragma once

#include "core/definitions.h"

// gdt_pointer, tss_entry, SegmentDescriptor, descriptorTable

namespace PalmyraOS::kernel::GDT {

    // Structure representing the GDT Pointer in memory
    typedef struct DescriptorPointer {
        uint16_t size;
        uint32_t address;
    } __attribute__((packed)) gdt_pointer_t;  // Ensure no compiler padding for accurate memory layout

    // 32-bit Task State Segment
    typedef struct tss_entry {
        uint32_t prev_tss;  // The previous TSS, with hardware task switching these form a kind of backward linked list.
        uint32_t esp0;      // The stack pointer to load when changing to kernel mode.
        uint32_t ss0;       // The stack segment to load when changing to kernel mode.

        // Everything below here is unused in this setup.
        uint32_t esp1;  // Stack pointer for ring 1 (not used in this implementation).
        uint32_t ss1;   // Stack segment for ring 1 (not used in this implementation).
        uint32_t esp2;  // Stack pointer for ring 2 (not used in this implementation).
        uint32_t ss2;   // Stack segment for ring 2 (not used in this implementation).

        uint32_t cr3;     // Page directory base register (CR3).
        uint32_t eip;     // Instruction pointer.
        uint32_t eflags;  // Flags register.
        uint32_t eax;
        uint32_t ecx;
        uint32_t edx;
        uint32_t ebx;
        uint32_t esp;
        uint32_t ebp;
        uint32_t esi;
        uint32_t edi;
        uint32_t es;
        uint32_t cs;
        uint32_t ss;
        uint32_t ds;
        uint32_t fs;
        uint32_t gs;
        uint32_t ldt;
        uint16_t trap;
        uint16_t iomap_base;
    } __attribute__((packed)) tss_entry_t;

    // ------------------------------------------------------------------
    // Modern, self-describing enums for descriptor fields
    // ------------------------------------------------------------------

    // CPU privilege levels (DPL field in descriptor)
    enum class PrivilegeLevel : uint8_t {
        Ring0 = 0,  // Kernel mode
        Ring1 = 1,  // Ring 1 (not used)
        Ring2 = 2,  // Ring 2 (not used)
        Ring3 = 3   // User mode
    };

    // Descriptor type: System or Code/Data (S bit in descriptor)
    enum class SegmentKind : uint8_t {
        System   = 0,  // System descriptor (TSS, LDT, gates)
        CodeData = 1   // Code or data segment
    };

    // 4-bit TYPE field when SegmentKind == CodeData
    enum class CodeDataType : uint8_t {
        Data_ReadOnly             = 0x0,
        Data_ReadWrite            = 0x2,
        Data_ReadOnly_ExpandDown  = 0x4,
        Data_ReadWrite_ExpandDown = 0x6,
        Code_ExecuteOnly          = 0x8,
        Code_ExecuteRead          = 0xA,
        Code_ExecuteOnly_Conform  = 0xC,
        Code_ExecuteRead_Conform  = 0xE
    };

    // 4-bit TYPE field when SegmentKind == System (subset used in 32-bit protected mode)
    enum class SystemType : uint8_t {
        Null          = 0x0,
        LDT           = 0x2,  // Local Descriptor Table
        TSS_Available = 0x9,  // Task State Segment (available)
        TSS_Busy      = 0xB   // Task State Segment (busy)
        // Gate types can be added if needed for IDT entries in GDT
    };

    // Descriptor presence flag (P bit in descriptor)
    enum class Presence : uint8_t {
        Absent  = 0,  // Segment not present in memory
        Present = 1   // Segment present in memory
    };

    // Granularity flag (G bit in descriptor)
    enum class Granularity : uint8_t {
        Byte = 0,  // Limit is in bytes
        Page = 1   // Limit is in 4KB pages
    };

    // ------------------------------------------------------------------
    // Bitfield layout that mirrors Intel SDM exactly (8 bytes total)
    // ------------------------------------------------------------------
    struct DescriptorBits {
        uint16_t limit_0_15;  // Limit bits 0-15
        uint16_t base_0_15;   // Base address bits 0-15
        uint8_t base_16_23;   // Base address bits 16-23

        // Access byte (bits 40-47)
        uint8_t segmentType : 4;       // TYPE[3:0] - CodeDataType or SystemType
        SegmentKind segmentKind : 1;   // S bit: 0=System, 1=Code/Data
        PrivilegeLevel privilege : 2;  // DPL: Descriptor Privilege Level
        Presence presence : 1;         // P bit: Present flag

        // Flags byte (bits 48-55)
        uint8_t limit_16_19 : 4;      // Limit bits 16-19
        bool isAvailableSW : 1;       // AVL: Available for system software use
        bool isLongMode : 1;          // L: 64-bit code segment (must be 0 in 32-bit mode)
        bool defaultOperand32 : 1;    // D/B: Default operation size (0=16-bit, 1=32-bit)
        Granularity granularity : 1;  // G: Granularity (Byte or Page)

        uint8_t base_24_31;  // Base address bits 24-31
    } __attribute__((packed));
    static_assert(sizeof(DescriptorBits) == 8, "GDT entry must be exactly 8 bytes");

    // Class representing an individual segment descriptor in the GDT
    class SegmentDescriptor {
    public:
        // Explicit, self-documenting initializer for every field we actually use
        struct SegmentDescriptorInput {
            // Base and limit as you think of them
            uint32_t base             = 0;        // Linear base address
            uint32_t limitRaw         = 0xFFFFF;  // 20-bit limit before granularity is applied

            // Classification and type
            SegmentKind segmentKind   = SegmentKind::CodeData;
            CodeDataType codeDataType = CodeDataType::Code_ExecuteRead;  // Used if segmentKind == CodeData
            SystemType systemType     = SystemType::TSS_Available;       // Used if segmentKind == System

            PrivilegeLevel privilege  = PrivilegeLevel::Ring0;  // Descriptor Privilege Level

            // Access and flags
            Presence presence         = Presence::Present;  // P: Segment presence
            Granularity granularity   = Granularity::Page;  // G: Limit granularity (Byte or Page)
            bool defaultOperand32     = true;               // D/B: Default operand size (true = 32-bit, false = 16-bit)
            bool isLongMode           = false;              // L: Long mode (must be false for 32-bit protected mode)
            bool isAvailableSW        = false;              // AVL: Available for system use
        };

        // Constructor using modern Init struct
        explicit SegmentDescriptor(const SegmentDescriptorInput& in);
        ~SegmentDescriptor() = default;

        // Inline function to retrieve full 32-bit base address
        [[nodiscard]] inline uint32_t getBase() const { return (static_cast<uint32_t>(bits.base_24_31) << 24) | (static_cast<uint32_t>(bits.base_16_23) << 16) | bits.base_0_15; }

        // Inline function to retrieve raw 20-bit limit reconstructed as 32-bit value
        [[nodiscard]] inline uint32_t getLimit() const { return (static_cast<uint32_t>(bits.limit_16_19) << 16) | bits.limit_0_15; }

        // Effective limit after granularity is applied
        [[nodiscard]] inline uint32_t effectiveLimit() const {
            uint32_t raw = getLimit();
            return (bits.granularity == Granularity::Page) ? ((raw << 12) | 0xFFF) : raw;
        }

        // Descriptor introspection helpers
        [[nodiscard]] inline PrivilegeLevel getPrivilegeLevel() const { return bits.privilege; }
        [[nodiscard]] inline Presence getPresence() const { return bits.presence; }
        [[nodiscard]] inline SegmentKind getSegmentKind() const { return bits.segmentKind; }
        [[nodiscard]] inline Granularity getGranularity() const { return bits.granularity; }
        [[nodiscard]] inline uint8_t rawTypeNibble() const { return bits.segmentType; }

    private:
        DescriptorBits bits{};  // The actual 8-byte descriptor bits
    } __attribute__((packed));

    // Class representing the entire Global Descriptor Table
    class GlobalDescriptorTable {
    public:
        explicit GlobalDescriptorTable(uint32_t InitialKernelStackPointer);
        ~GlobalDescriptorTable() = default;

        // Update the kernel stack pointer in TSS (used for privilege level switches)
        void setKernelStack(uint32_t esp);

        // Function to retrieve the kernel code segment selector offset
        [[nodiscard]] inline uint16_t getKernelCodeSegmentSelector() const {
            // (uint8_t*)this : address of the table (class)
            return ((uint32_t) &kernel_code_segment_selector - (uint32_t) this);
            //     Address of the Descriptor               Addr of GDT
        }

        // Function to retrieve the user code segment selector offset
        [[nodiscard]] inline uint16_t getUserCodeSegmentSelector() const {
            // (uint8_t*)this : address of the table (class)
            return ((uint32_t) &user_code_segment_selector - (uint32_t) this);
            //     Address of the Descriptor               Addr of GDT
        }

        // Function to retrieve the kernel data segment selector offset
        [[nodiscard]] inline uint16_t getKernelDataSegmentSelector() const { return ((uint32_t) &kernel_data_segment_selector - (uint32_t) this); }

        // Function to retrieve the user data segment selector offset
        [[nodiscard]] inline uint16_t getUserDataSegmentSelector() const { return ((uint32_t) &user_data_segment_selector - (uint32_t) this); }

        // Function to retrieve the Task State Segment selector offset
        [[nodiscard]] inline uint16_t getTaskStateSegmentSelector() const { return ((uint32_t) &task_state_descriptor - (uint32_t) this); }

    private:
        // Initialize the Task State Segment with the given stack pointer
        void initializeTSS(uint32_t esp);

    public:
        // Global Descriptor Table entries (must be in this order)
        SegmentDescriptor null_segment_selector;         // Null segment descriptor (mandatory first entry)
        SegmentDescriptor kernel_code_segment_selector;  // Kernel code segment descriptor (Ring 0)
        SegmentDescriptor kernel_data_segment_selector;  // Kernel data segment descriptor (Ring 0)
        SegmentDescriptor user_code_segment_selector;    // User space code segment descriptor (Ring 3)
        SegmentDescriptor user_data_segment_selector;    // User space data segment descriptor (Ring 3)
        SegmentDescriptor task_state_descriptor;         // Task State Segment descriptor
        // TSS Entry
        tss_entry_t tss_entry{0};  // Task State Segment, initially all zeros


    } __attribute__((packed));


}  // namespace PalmyraOS::kernel::GDT