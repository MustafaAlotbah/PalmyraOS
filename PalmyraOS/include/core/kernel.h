

#pragma once

#include "boot/multiboot.h"
#include "boot/multiboot2.h"
#include "core/Display.h"
#include "core/definitions.h"
#include "core/memory/KernelHeap.h"
#include "core/memory/paging.h"
#include "core/peripherals/ATA.h"
#include "core/peripherals/BGA.h"


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


namespace PalmyraOS::kernel {
    // Frequency of the Advanced Programmable Interrupt Controller (APIC)
    constexpr uint32_t SystemClockFrequency      = 250;

    // The Kernel stack pointer when an interrupt is called (TSS)
    constexpr uint32_t InitialKernelStackPointer = 32 * 1024 * 1024;

    // Space to which we can still call kmalloc after we initialize the physical memory and before paging is enabled
    constexpr uint32_t SafeSpace                 = 32 * 1024 * 1024;

    /**
     * Pointer to the VBE (VESA BIOS Extensions) object.
     * This global pointer is used to access VBE functions and information.
     */
    extern PalmyraOS::kernel::Display* display_ptr;

    extern PalmyraOS::kernel::Brush* brush_ptr;
    extern PalmyraOS::kernel::TextRenderer* textRenderer_ptr;
    extern PalmyraOS::kernel::PagingDirectory* kernelPagingDirectory_ptr;
    extern PalmyraOS::kernel::GDT::GlobalDescriptorTable* gdt_ptr;
    extern PalmyraOS::kernel::interrupts::InterruptController* idt_ptr;
    extern PalmyraOS::kernel::HeapManager heapManager;
    extern uint32_t kernelLastPage;
    extern PalmyraOS::kernel::ATA* ata_primary_master;
    extern PalmyraOS::kernel::ATA* ata_primary_slave;
    extern PalmyraOS::kernel::ATA* ata_secondary_master;
    extern PalmyraOS::kernel::ATA* ata_secondary_slave;

    /**
     * @brief Initializes the graphics system using Multiboot 2 information.
     * @param mb2Info Multiboot 2 information structure containing framebuffer/VBE data
     * @return True if initialization is successful, false otherwise.
     */
    bool initializeGraphics(const Multiboot2::MultibootInfo& mb2Info);

    /**
     * @brief Initializes the graphics system with explicit framebuffer information.
     *
     * This function is used when graphics information is not available from the
     * bootloader (e.g., when BGA is detected at runtime). It allocates memory
     * for the back buffer and initializes all graphics components.
     *
     * @param width Framebuffer width in pixels
     * @param height Framebuffer height in pixels
     * @param framebufferAddress Physical address of the framebuffer
     * @param bpp Bits per pixel (8, 16, 24, or 32)
     * @return True if initialization is successful, false otherwise.
     */
    bool initializeGraphicsWithFramebuffer(uint16_t width, uint16_t height, uint32_t framebufferAddress, uint16_t bpp);

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
     * @brief Initializes the physical memory manager using Multiboot 2 information.
     * @param multiboot2_info Multiboot 2 information structure
     * @return True if initialization is successful, false otherwise.
     */
    bool initializePhysicalMemory(const Multiboot2::MultibootInfo& multiboot2_info);

    /**
     * @brief Initializes the virtual memory manager using Multiboot 2 information.
     * @param multiboot2_info Multiboot 2 information structure
     * @return True if initialization is successful, false otherwise.
     */
    bool initializeVirtualMemory(const Multiboot2::MultibootInfo& multiboot2_info);

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

}  // namespace PalmyraOS::kernel
