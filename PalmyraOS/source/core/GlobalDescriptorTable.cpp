

#include "core/GlobalDescriptorTable.h"

// External function to flush the GDT pointer. (Check gdt.asm)
extern "C" void flush_gdt_table(uint32_t _gdt_pointer);
extern "C" void flush_tss();


PalmyraOS::kernel::GDT::SegmentDescriptor::SegmentDescriptor(
	uint32_t address,
	uint32_t size,
	uint8_t access_type,
	uint8_t flags
) :
	addr_0_15{ (uint16_t)(address & 0xFFFF) },
	size_0_15{ (uint16_t)(size & 0xFFFF) },
	addr_16_23{ (uint8_t)((address >> 16) & 0xFF) },
	access_type{ access_type },
	flag_size_16_19{ (uint8_t)(((size >> 16) & 0x0F) | (flags & 0xF0)) },
	addr_24_31{ (uint8_t)((address >> 24) & 0xFF) }
{}

PalmyraOS::kernel::GDT::GlobalDescriptorTable::GlobalDescriptorTable(uint32_t InitialKernelStackPointer)
	:
	null_segment_selector(0, 0, 0, 0),
	// Code segment, execute/read, 4KB granularity, 32-bit mode
	kernel_code_segment_selector(0, 0xFFFFFF, 0x9A, 0xCF),
	// Data segment, read/write, 4KB granularity, 32-bit mode
	kernel_data_segment_selector(0, 0xFFFFFF, 0x92, 0xCF),
	// User code, execute/read, 4KB granularity, 32-bit mode
	user_code_segment_selector(0, 0xFFFFFF, 0xFA, 0xCF),
	// User data, read/write, 4KB granularity, 32-bit mode
	user_data_segment_selector(0, 0xFFFFFF, 0xF2, 0xCF),
	// Task state, 32-bit TSS (Available)
	task_state_descriptor((uint32_t)&tss_entry, sizeof(tss_entry_t), 0x89, 0x40)
{

	// Global Descriptor Table Segments
	DescriptorPointer gdt_p{};
	gdt_p.size    = sizeof(GlobalDescriptorTable) - 1;   // Set the size (subtracting 1 as per GDT definition)
	gdt_p.address = (uint32_t)this;                      // Set the address to the current instance of the GDT
	flush_gdt_table((uint32_t)&gdt_p);                   // Flush the GDT pointer using the external function

	// Task Switch Segment
	initializeTSS(InitialKernelStackPointer);
}

void PalmyraOS::kernel::GDT::GlobalDescriptorTable::initializeTSS(uint32_t esp)
{
	// initially all 0s
	tss_entry.ss0 = getKernelDataSegmentSelector();        // kernel data segment selector as is
	setKernelStack(esp);                                // set the kernel stack as given
	flush_tss();                                        // load the tss pointer to the CPU
}

void PalmyraOS::kernel::GDT::GlobalDescriptorTable::setKernelStack(uint32_t esp)
{
	tss_entry.esp0 = esp;
}
