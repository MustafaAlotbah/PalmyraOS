

#pragma once

#include "core/definitions.h"
#include "boot/multiboot.h"
#include "core/VBE.h"
#include "core/memory/paging.h"
#include "core/memory/KernelHeap.h"
#include "core/peripherals/ATA.h"


extern uint32_t placement_address;

// Memory regions defined by the linker script
extern "C" uint32_t __mem_end;
extern "C" uint32_t __mem_multiboot_start;
extern "C" uint32_t __mem_multiboot_end;
extern "C" uint32_t __mem_rodata_start;
extern "C" uint32_t __mem_rodata_end;
extern "C" uint32_t __mem_text_start;
extern "C" uint32_t __mem_text_end;
extern "C" uint32_t __mem_cons_start;
extern "C" uint32_t __mem_cons_end;
extern "C" uint32_t __mem_data_start;
extern "C" uint32_t __mem_data_end;
extern "C" uint32_t __mem_bss_start;
extern "C" uint32_t __mem_bss_end;
extern "C" uint32_t __end;


namespace PalmyraOS::kernel
{
  // Frequency of the Advanced Programmable Interrupt Controller (APIC)
  constexpr uint32_t SystemClockFrequency = 250;

  // The Kernel stack pointer when an interrupt is called (TSS)
  constexpr uint32_t InitialKernelStackPointer = 32 * 1024 * 1024;

  // Space to which we can still call kmalloc after we initialize the physical memory and before paging is enabled
  constexpr uint32_t SafeSpace = 32 * 1024 * 1024;

  /**
   * Pointer to the VBE (VESA BIOS Extensions) object.
   * This global pointer is used to access VBE functions and information.
   */
  extern PalmyraOS::kernel::VBE* vbe_ptr;

  extern PalmyraOS::kernel::Brush                          * brush_ptr;
  extern PalmyraOS::kernel::TextRenderer                   * textRenderer_ptr;
  extern PalmyraOS::kernel::PagingDirectory                * kernelPagingDirectory_ptr;
  extern PalmyraOS::kernel::GDT::GlobalDescriptorTable     * gdt_ptr;
  extern PalmyraOS::kernel::interrupts::InterruptController* idt_ptr;
  extern PalmyraOS::kernel::HeapManager heapManager;
  extern uint32_t                       kernelLastPage;
  extern PalmyraOS::kernel::ATA* ata_primary_master;
  extern PalmyraOS::kernel::ATA* ata_primary_slave;
  extern PalmyraOS::kernel::ATA* ata_secondary_master;
  extern PalmyraOS::kernel::ATA* ata_secondary_slave;

  /**
   * @brief Initializes the graphics system.
   * @param vbe_mode_info Pointer to VBE mode information.
   * @param vbe_control_info Pointer to VBE control information.
   * @return True if initialization is successful, false otherwise.
   */
  bool initializeGraphics(vbe_mode_info_t* vbe_mode_info, vbe_control_info_t* vbe_control_info);

  /**
   * @brief Clears the screen and optionally draws the logo.
   * @param drawLogo Whether to draw the logo on the screen.
   */
  void clearScreen(bool drawLogo);

  /**
   * @brief Initializes the Global Descriptor Table (GDT).
   * @return True if initialization is successful, false otherwise.
   */
  bool initializeGlobalDescriptorTable();

  /**
   * @brief Initializes the Interrupt Descriptor Table (IDT).
   * @return True if initialization is successful, false otherwise.
   */
  bool initializeInterrupts();

  /**
   * @brief Initializes the physical memory manager.
   * @param x86_multiboot_info Pointer to multiboot information.
   * @return True if initialization is successful, false otherwise.
   */
  bool initializePhysicalMemory(multiboot_info_t* x86_multiboot_info);

  /**
   * @brief Initializes the virtual memory manager.
   * @param x86_multiboot_info Pointer to multiboot information.
   * @return True if initialization is successful, false otherwise.
   */
  bool initializeVirtualMemory(multiboot_info_t* x86_multiboot_info);

  /**
   * @brief Tests the memory system.
   *
   * This function performs various tests on the memory system
   * to ensure its correct operation.
   */
  void testMemory();

  void initializeDrivers();

  void initializePartitions();

  void initializeBinaries();

}


