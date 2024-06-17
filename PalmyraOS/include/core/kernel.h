

#pragma once

#include "core/definitions.h"
#include "boot/multiboot.h"
#include "core/VBE.h"
#include "core/memory/paging.h"
#include "core/memory/Heap.h"


extern uint32_t placement_address;

namespace PalmyraOS::kernel
{
  // Frequency of the Advanced Programmable Interrupt Controller (APIC)
  constexpr uint32_t SystemClockFrequency = 250;

  // The Kernel stack pointer when an interrupt is called (TSS)
  constexpr uint32_t InitialKernelStackPointer = 32 * 1024 * 1024;

  // Space to which we can still call kmalloc after we initialize the physical memory and before paging is enabled
  constexpr uint32_t SafeSpace = 32 * 1024 * 1024;

  /**
   * Sets up the kernel. This function is called after the system has been initialized.
   * It initializes various subsystems and enters the main kernel loop.
   */
  [[noreturn]] void setup();

  /**
   * @brief Updates the system with a dummy uptime.
   * @param dummy_up_time The dummy uptime value.
   */
  void update(uint64_t dummy_up_time);

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

}


