
#pragma once

#include "core/definitions.h"
#include "core/GlobalDescriptorTable.h"
#include "core/port.h"


namespace PalmyraOS::kernel::interrupts
{
  // Forward Declarations
  struct CPURegisters;

  // Types
  typedef uint32_t* (* InterruptHandler)(CPURegisters* regs);

  // Definitions and Enums
  // Enum class to represent types of gates in the IDT.
  enum class GateType
  {
	  TaskGate      = 0b0101,
	  InterruptGate = 0b1110,
	  TrapGate      = 0b1111
  };

  // Process Control Block (PCB)does
  struct CPURegisters
  {
	  /* 5. Paging */
	  uint32_t cr3;                // Pointer to Paging Directory

	  /* 4. Data Segment Registers */
	  uint32_t gs;
	  uint32_t fs;
	  uint32_t es;
	  uint32_t ds;
	  /* 3. Pushed by pusha */
	  uint32_t edi;              // Data index
	  uint32_t esi;              // Source index (not "stack index")
	  uint32_t ebp;              // Stack base pointer
	  uint32_t esp;              // Stack pointer (useless?)
	  uint32_t ebx;              // Base register
	  uint32_t edx;              // Data register
	  uint32_t ecx;              // Counter
	  uint32_t eax;              // Accumulator
	  /* 2. Pushed by InterruptServiceRoutine */
	  uint32_t intNo;
	  /* 1.5. Automatically for Exceptions, manually for Interrupts */
	  uint32_t errorCode;
	  /* 1. Pushed automatically by the CPU */
	  uint32_t eip;              // Instruction pointer
	  uint32_t cs;               // Code segment
	  uint32_t eflags;           // Flags
	  /* 0.5. These are pushed ONLY in case of transitioning from user mode to kernel mode */
	  uint32_t userEsp;          // user stack pointer (user esp) ESP of process (previous privilege level)
	  uint32_t ss;               // Stack segment (previous privilege level)
  }__attribute__((packed));

  // Defines the pointer to the Interrupt Descriptor Table (IDT).
  struct InterruptPointer
  {
	  uint16_t size;          // limit
	  uint32_t address;       // base
  }__attribute__((packed));

  // Defines an interrupt entry of the Interrupt Descriptor Table (IDT)
  struct InterruptEntry
  {
	  uint16_t handlerAddressLow;   // Lower 16 bits of the interrupt handler function's address.
	  uint16_t selector;            // Selector for the code segment.
	  uint8_t  reserved;            // Reserved, should always be zero.
	  uint8_t  attributes;          // Specifies type, privilege level, and other attributes.
	  uint16_t handlerAddressHigh;  // Upper 16 bits of the interrupt handler function's address.
  }__attribute__((packed));

  // Manages the Interrupt Descriptor Table (IDT).
  class InterruptDescriptorTable
  {

   public:
	  explicit InterruptDescriptorTable(GDT::GlobalDescriptorTable* gdt);

	  // Sets a specific IDT entry.
	  void setDescriptor(
		  uint8_t interruptVector,
		  uint16_t codeSegmentSelector,
		  void (* handlerFunc)(),
		  uint8_t privilegeRing,
		  GateType gateType
	  );

	  void flush();

   private:
	  InterruptEntry descriptors[256]{};  // Array of IDT entries.
  };

  // Controls the Programmable Interrupt Controller (PIC).
  class PICManager
  {
   public:
	  // ports
	  static constexpr uint16_t PORT_PIC_MASTER_CMD   = 0x20;
	  static constexpr uint16_t PORT_PIC_MASTER_DATA  = 0x21;
	  static constexpr uint16_t PORT_PIC_SLAVE_CMD    = 0xA0;
	  static constexpr uint16_t PORT_PIC_SLAVE_DATA   = 0xA1;
	  // commands
	  static constexpr uint8_t  PIC_EOI               = 0x20;
	  static constexpr uint8_t  ICW1_INIT             = 0x11;
	  static constexpr uint8_t  ICW2_MASTER_OFFSET    = 0x20;
	  static constexpr uint8_t  ICW2_SLAVE_OFFSET     = 0x28;
	  static constexpr uint8_t  ICW3_MASTER_SLAVE     = 0x04;
	  static constexpr uint8_t  ICW3_SLAVE_ID         = 0x02;
	  static constexpr uint8_t  ICW4_8086_MODE        = 0x01;
	  static constexpr uint8_t  MASK_ALL_INTERRUPTS   = 0xFF;
	  static constexpr uint8_t  UNMASK_ALL_INTERRUPTS = 0x00;

   public:
	  PICManager();
	  void enableInterrupts();       // Enables hardware interrupts.
	  inline ports::BytePort& getMasterPicCommand()
	  { return masterPicCommand; };
	  inline ports::BytePort& getSlavePicCommand()
	  { return slavePicCommand; };
	  inline ports::BytePort& getMasterPicData()
	  { return masterPicData; };
	  inline ports::BytePort& getSlavePicData()
	  { return slavePicData; };

   private:
	  ports::BytePort masterPicCommand; // Master PIC command port.
	  ports::BytePort masterPicData;    // Master PIC data port.
	  ports::BytePort slavePicCommand;  // Slave PIC command port.
	  ports::BytePort slavePicData;     // Slave PIC data port.
  };

  // Central class managing the entire interrupt system.
  class InterruptController
  {

   public:
	  explicit InterruptController(GDT::GlobalDescriptorTable* gdt);
	  ~InterruptController() = default;

	  static void enableInterrupts();
	  static void disableInterrupts();
	  static void setInterruptHandler(uint8_t interrupt_number, InterruptHandler interrupt_handler);

	  REMOVE_COPY(InterruptController);
   public:
	  static PICManager* activePicManager;        // to be accessed by the global isr handler
   private:
	  InterruptDescriptorTable idtHandler;        // IDT handler instance.
	  PICManager               picManager{};      // PIC manager instance.
  };


}