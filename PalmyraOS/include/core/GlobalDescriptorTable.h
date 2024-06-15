
#pragma once

#include "core/definitions.h"

// gdt_pointer, tss_entry, SegmentDescriptor, descriptorTable

namespace PalmyraOS::kernel::GDT
{

// Structure representing the GDT Pointer in memory
  typedef struct DescriptorPointer
  {
	  uint16_t size;
	  uint32_t address;
  }__attribute__((packed)) gdt_pointer_t; // Ensure no compiler padding for accurate memory layout


  typedef struct tss_entry
  {
	  uint32_t prev_tss;    // The previous TSS, with hardware task switching these form a kind of backward linked list.
	  uint32_t esp0;        // The stack pointer to load when changing to kernel mode.
	  uint32_t ss0;         // The stack segment to load when changing to kernel mode.

	  // Everything below here is unused.
	  uint32_t esp1;        // Stack pointer for ring 1 (not used in this implementation).
	  uint32_t ss1;         // Stack segment for ring 1 (not used in this implementation).
	  uint32_t esp2;        // Stack pointer for ring 2 (not used in this implementation).
	  uint32_t ss2;         // Stack segment for ring 2 (not used in this implementation).

	  uint32_t cr3;         // Page directory base register (CR3).
	  uint32_t eip;         // Instruction pointer.
	  uint32_t eflags;      // Flags register.
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
  } __attribute((packed))  tss_entry_t;

// Class representing an individual segment descriptor in the GDT
  class SegmentDescriptor
  {      // Base address bits 24-31
   public:
	  SegmentDescriptor(uint32_t address, uint32_t size, uint8_t access_type, uint8_t flags);
	  ~SegmentDescriptor() = default;

	  // Inline function to retrieve full 32-bit base address
	  [[nodiscard]] inline uint32_t getBase() const
	  {
		  return (addr_24_31 << 24) | (addr_16_23 << 16) | addr_0_15;
	  }

	  // Inline function to retrieve full 32-bit limit
	  [[nodiscard]] inline uint32_t getLimit() const
	  {
		  return (flag_size_16_19 & 0xF) << 16 | size_0_15;
	  }

   private:
	  uint16_t size_0_15;             // Limit bits 0-15
	  uint16_t addr_0_15;             // Base address bits 0-15
	  uint8_t  addr_16_23;             // Base address bits 16-23
	  uint8_t  access_type;            // Access type flags (read/write, code/data, etc.)
	  uint8_t  flag_size_16_19;        // Flags and limit bits 16-19
	  uint8_t  addr_24_31;
  } __attribute__((packed));

  // Class representing the entire Global Descriptor Table
  class GlobalDescriptorTable
  {
   public:
	  explicit GlobalDescriptorTable(uint32_t InitialKernelStackPointer);
	  ~GlobalDescriptorTable() = default;

	  void setKernelStack(uint32_t esp);

	  // Function to retrieve the code segment selector offset
	  [[nodiscard]] inline uint16_t getKernelCodeSegmentSelector() const
	  {
		  // (uint8_t*)this : address of the table (class)
		  return ((uint32_t)&kernel_code_segment_selector - (uint32_t)this);
		  //     Address of the Descriptor               Addr of GDT
	  }

	  // Function to retrieve the code segment selector offset
	  [[nodiscard]] inline uint16_t getUserCodeSegmentSelector() const
	  {
		  // (uint8_t*)this : address of the table (class)
		  return ((uint32_t)&user_code_segment_selector - (uint32_t)this);
		  //     Address of the Descriptor               Addr of GDT
	  }

	  // Function to retrieve the data segment selector offset
	  [[nodiscard]] inline uint16_t getKernelDataSegmentSelector() const
	  {
		  return ((uint32_t)&kernel_data_segment_selector - (uint32_t)this);
	  }

	  // Function to retrieve the data segment selector offset
	  [[nodiscard]] inline uint16_t getUserDataSegmentSelector() const
	  {
		  return ((uint32_t)&user_data_segment_selector - (uint32_t)this);
	  }

	  // Function to retrieve the data segment selector offset
	  [[nodiscard]] inline uint16_t getTaskStateSegmentSelector() const
	  {
		  return ((uint32_t)&task_state_descriptor - (uint32_t)this);
	  }

   private:
	  void initializeTSS(uint32_t esp);

   public:
	  // Global Descriptor Table
	  SegmentDescriptor null_segment_selector;          // Null segment descriptor (mandatory)
	  SegmentDescriptor kernel_code_segment_selector;   // Kernel code segment descriptor
	  SegmentDescriptor kernel_data_segment_selector;   // Kernel data segment descriptor
	  SegmentDescriptor user_code_segment_selector;     // User space code segment descriptor
	  SegmentDescriptor user_data_segment_selector;     // User space data segment descriptor
	  SegmentDescriptor task_state_descriptor;            // Task State Segment descriptor
	  // TSS Entry
	  tss_entry_t       tss_entry{ 0 };                            // all 0s


  }__attribute__((packed));


}