
#include "core/GlobalDescriptorTable.h"

// External function to flush the GDT pointer. (Check gdt.asm)
extern "C" void flush_gdt_table(uint32_t _gdt_pointer);
extern "C" void flush_tss();

// ---------------- SegmentDescriptor ----------------

using namespace PalmyraOS::kernel::GDT;

// Build a descriptor entry from a fully-typed Init struct
SegmentDescriptor::SegmentDescriptor(const SegmentDescriptorInput& input) {
    // Zero everything first for safety
    DescriptorBits z{};

    // Slice base address into three parts
    z.base_0_15    = static_cast<uint16_t>(input.base & 0xFFFF);
    z.base_16_23   = static_cast<uint8_t>((input.base >> 16) & 0xFF);
    z.base_24_31   = static_cast<uint8_t>((input.base >> 24) & 0xFF);

    // Slice 20-bit limit into two parts
    uint32_t lim20 = (input.limitRaw & 0xFFFFF);
    z.limit_0_15   = static_cast<uint16_t>(lim20 & 0xFFFF);
    z.limit_16_19  = static_cast<uint8_t>((lim20 >> 16) & 0x0F);

    // Set access byte fields
    z.segmentKind  = input.segmentKind;
    if (input.segmentKind == SegmentKind::CodeData) { z.segmentType = static_cast<uint8_t>(input.codeDataType); }
    else { z.segmentType = static_cast<uint8_t>(input.systemType); }
    z.privilege        = input.privilege;
    z.presence         = input.presence;

    // Set flags byte fields
    z.granularity      = input.granularity;
    z.defaultOperand32 = input.defaultOperand32;
    z.isLongMode       = input.isLongMode;  // Must be false for 32-bit protected mode
    z.isAvailableSW    = input.isAvailableSW;

    // Commit the descriptor bits to this instance
    bits               = z;
}

// ---------------- GlobalDescriptorTable ----------------

GlobalDescriptorTable::GlobalDescriptorTable(uint32_t InitialKernelStackPointer)
    :  // Null segment descriptor (mandatory first entry)
      null_segment_selector(SegmentDescriptor::SegmentDescriptorInput{
              .base             = 0,
              .limitRaw         = 0,
              .segmentKind      = SegmentKind::System,
              .systemType       = SystemType::Null,
              .privilege        = PrivilegeLevel::Ring0,
              .presence         = Presence::Absent,  // Not present
              .granularity      = Granularity::Byte,
              .defaultOperand32 = false,
              .isLongMode       = false,
              .isAvailableSW    = false,
      }),
      // Kernel code segment: execute/read, 4KB granularity, 32-bit mode
      kernel_code_segment_selector(SegmentDescriptor::SegmentDescriptorInput{
              .base             = 0,
              .limitRaw         = 0xFFFFF,  // Full 4GB with granularity
              .segmentKind      = SegmentKind::CodeData,
              .codeDataType     = CodeDataType::Code_ExecuteRead,
              .privilege        = PrivilegeLevel::Ring0,
              .presence         = Presence::Present,
              .granularity      = Granularity::Page,  // 4KB page granularity
              .defaultOperand32 = true,               // 32-bit mode
              .isLongMode       = false,
              .isAvailableSW    = false,
      }),
      // Kernel data segment: read/write, 4KB granularity, 32-bit mode
      kernel_data_segment_selector(SegmentDescriptor::SegmentDescriptorInput{
              .base             = 0,
              .limitRaw         = 0xFFFFF,  // Full 4GB with granularity
              .segmentKind      = SegmentKind::CodeData,
              .codeDataType     = CodeDataType::Data_ReadWrite,
              .privilege        = PrivilegeLevel::Ring0,
              .presence         = Presence::Present,
              .granularity      = Granularity::Page,  // 4KB page granularity
              .defaultOperand32 = true,               // 32-bit mode
              .isLongMode       = false,
              .isAvailableSW    = false,
      }),
      // User code segment: execute/read, 4KB granularity, 32-bit mode
      user_code_segment_selector(SegmentDescriptor::SegmentDescriptorInput{
              .base             = 0,
              .limitRaw         = 0xFFFFF,  // Full 4GB with granularity
              .segmentKind      = SegmentKind::CodeData,
              .codeDataType     = CodeDataType::Code_ExecuteRead,
              .privilege        = PrivilegeLevel::Ring3,  // User mode
              .presence         = Presence::Present,
              .granularity      = Granularity::Page,  // 4KB page granularity
              .defaultOperand32 = true,               // 32-bit mode
              .isLongMode       = false,
              .isAvailableSW    = false,
      }),
      // User data segment: read/write, 4KB granularity, 32-bit mode
      user_data_segment_selector(SegmentDescriptor::SegmentDescriptorInput{
              .base             = 0,
              .limitRaw         = 0xFFFFF,  // Full 4GB with granularity
              .segmentKind      = SegmentKind::CodeData,
              .codeDataType     = CodeDataType::Data_ReadWrite,
              .privilege        = PrivilegeLevel::Ring3,  // User mode
              .presence         = Presence::Present,
              .granularity      = Granularity::Page,  // 4KB page granularity
              .defaultOperand32 = true,               // 32-bit mode
              .isLongMode       = false,
              .isAvailableSW    = false,
      }),
      // Task State Segment: 32-bit TSS (Available), typically byte granularity
      task_state_descriptor(SegmentDescriptor::SegmentDescriptorInput{
              .base             = (uint32_t) &tss_entry,
              .limitRaw         = static_cast<uint32_t>(sizeof(tss_entry_t) - 1),
              .segmentKind      = SegmentKind::System,
              .systemType       = SystemType::TSS_Available,
              .privilege        = PrivilegeLevel::Ring0,
              .presence         = Presence::Present,
              .granularity      = Granularity::Byte,  // Byte granularity for TSS is typical
              .defaultOperand32 = true,               // 32-bit TSS
              .isLongMode       = false,
              .isAvailableSW    = false,
      }) {
    // Global Descriptor Table Segments
    DescriptorPointer gdt_p{};
    gdt_p.size    = sizeof(GlobalDescriptorTable) - 1;  // Set the size (subtracting 1 as per GDT definition)
    gdt_p.address = (uint32_t) this;                    // Set the address to the current instance of the GDT
    flush_gdt_table((uint32_t) &gdt_p);                 // Flush the GDT pointer using the external function

    // Task Switch Segment
    initializeTSS(InitialKernelStackPointer);
}

void GlobalDescriptorTable::initializeTSS(uint32_t esp) {
    // TSS entry is initially all zeros
    tss_entry.ss0 = getKernelDataSegmentSelector();  // Set kernel data segment selector
    setKernelStack(esp);                             // Set the kernel stack pointer
    flush_tss();                                     // Load the TSS pointer to the CPU
}

void GlobalDescriptorTable::setKernelStack(uint32_t esp) {
    tss_entry.esp0 = esp;  // Update kernel stack pointer in TSS
}
