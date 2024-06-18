#include "core/kernel.h"
#include "core/VBE.h"
#include "core/memory/paging.h"
#include "core/panic.h"
#include "tests/pagingTests.h"
#include "tests/allocatorTests.h"

#include <new>

// Globals


namespace PalmyraOS::kernel
{

  // Graphics
  PalmyraOS::kernel::VBE         * vbe_ptr          = nullptr;
  PalmyraOS::kernel::Brush       * brush_ptr        = nullptr;
  PalmyraOS::kernel::TextRenderer* textRenderer_ptr = nullptr;

  // CPU
  PalmyraOS::kernel::GDT::GlobalDescriptorTable     * gdt_ptr = nullptr;
  PalmyraOS::kernel::interrupts::InterruptController* idt_ptr = nullptr;;

  // Memory
  PalmyraOS::kernel::PagingDirectory* kernelPagingDirectory_ptr = nullptr;
  PalmyraOS::kernel::HeapManager heapManager;
  uint32_t                       kernelLastPage = 0;

}

bool PalmyraOS::kernel::initializeGraphics(vbe_mode_info_t* vbe_mode_info, vbe_control_info_t* vbe_control_info)
{
	/**
	 * @brief Initializes the graphics subsystem.
	 *
	 * This function allocates memory and initializes various components
	 * necessary for the graphics subsystem, including the VBE object,
	 * the font manager, the brush, and the text renderer.
	 *
	 * @param vbe_mode_info Pointer to the VBE mode information structure.
	 * @param vbe_control_info Pointer to the VBE control information structure.
	 * @return True if the graphics subsystem is successfully initialized, false otherwise.
	 */

	// Calculate the size of the VBE buffer
	uint32_t VBE_buffer_size = vbe_mode_info->width * vbe_mode_info->height * sizeof(uint32_t);

	// initialize VBE and framebuffer
	{
		// Allocate memory for the VBE object
		kernel::vbe_ptr = (VBE*)kernel::kmalloc(sizeof(VBE));
		if (kernel::vbe_ptr == nullptr) return false;

		// Construct the VBE object in the allocated memory
		new(kernel::vbe_ptr) VBE(
			vbe_mode_info,
			vbe_control_info,
			(uint32_t*)kernel::kmalloc(VBE_buffer_size)
		);
	}

	// Initialize the font manager
	fonts::FontManager::initialize();

	// Initialize kernel's brush
	{
		// Allocate memory for the brush object
		kernel::brush_ptr = (Brush*)kernel::kmalloc(sizeof(Brush));
		if (kernel::brush_ptr == nullptr) return false;

		// Construct the brush object in the allocated memory
		new(kernel::brush_ptr) Brush(kernel::vbe_ptr->getFrameBuffer());
	}

	// Initialize kernel's text renderer
	{
		// Allocate memory for the text renderer object
		kernel::textRenderer_ptr = (TextRenderer*)kernel::kmalloc(sizeof(TextRenderer));
		if (kernel::textRenderer_ptr == nullptr) return false;

		// Construct the text renderer object in the allocated memory
		new(kernel::textRenderer_ptr) TextRenderer(
			kernel::vbe_ptr->getFrameBuffer(),
			fonts::FontManager::getFont("Arial-12")
		);
	}

	// Everything is initialized successfully
	return true;
}

void PalmyraOS::kernel::clearScreen(bool drawLogo)
{
	/**
	 * @brief Clears the screen and optionally draws the logo.
	 *
	 * This function fills the screen with a black color and resets the text renderer.
	 * If the drawLogo parameter is true, it also draws the PalmyraOS logo and a horizontal line.
	 * Finally, it swaps the buffers to update the display.
	 *
	 * @param drawLogo A boolean indicating whether to draw the logo on the screen.
	 */

	auto& textRenderer = *kernel::textRenderer_ptr;

	// Fill the screen with black color and reset the text renderer
	brush_ptr->fill(Color::Black);
	textRenderer.reset();

	if (drawLogo)
	{
		// Draw the logo text
		textRenderer << Color::Orange << "Palmyra" << Color::LightBlue << "OS ";
		textRenderer << Color::White << "v0.01\n";

		// Draw a horizontal line below the logo text
		brush_ptr->drawHLine(1, 150, textRenderer.getCursorY() + 2, Color::White);
	}

	// Swap the buffers to update the display
	vbe_ptr->swapBuffers();
}

bool PalmyraOS::kernel::initializeGlobalDescriptorTable()
{
	/**
	 * @brief Initializes the Global Descriptor Table (GDT).
	 *
	 * This function allocates memory for the GDT object and constructs it in the allocated memory.
	 *
	 * @return True if the GDT is successfully initialized, false otherwise.
	 */

	// Initialize the Global Descriptor Table (GDT)
	{
		// Allocate memory for the GDT object
		kernel::gdt_ptr = (GDT::GlobalDescriptorTable*)kernel::kmalloc(sizeof(GDT::GlobalDescriptorTable));
		if (kernel::gdt_ptr == nullptr) return false;

		// Construct the GDT object in the allocated memory
		new(kernel::gdt_ptr) GDT::GlobalDescriptorTable(kernel::InitialKernelStackPointer);
	}
	return true;
}

bool PalmyraOS::kernel::initializeInterrupts()
{
	/**
	 * @brief Initializes the Interrupt Descriptor Table (IDT).
	 *
	 * This function allocates memory for the Interrupt Controller object and constructs it in the allocated memory.
	 *
	 * @return True if the IDT is successfully initialized, false otherwise.
	 */

	// Initialize the Interrupt Descriptor Table (IDT)
	{
		// Allocate memory for the Interrupt Controller object
		kernel::idt_ptr = (interrupts::InterruptController*)kernel::kmalloc(sizeof(interrupts::InterruptController));
		if (kernel::idt_ptr == nullptr) return false;

		// Construct the Interrupt Controller object in the allocated memory
		new(kernel::idt_ptr) interrupts::InterruptController(gdt_ptr);
	}
	return true;
}

bool PalmyraOS::kernel::initializePhysicalMemory(multiboot_info_t* x86_multiboot_info)
{
	/**
	 * @brief Initializes the physical memory manager.
	 *
	 * This function reserves all kernel space and some additional safe space,
	 * initializes the physical memory system, and reserves the video memory.
	 *
	 * @param x86_multiboot_info Pointer to the multiboot information structure.
	 * @return True if the physical memory manager is successfully initialized, false otherwise.
	 */

	// Reserve all kernel space and add some safe space
	// This method automatically reserves all kmalloc()ed space + SafeSpace
	PalmyraOS::kernel::PhysicalMemory::initialize(SafeSpace, x86_multiboot_info->mem_upper * 1024);

	// Reserve video memory to prevent other frames from overwriting it
	{
		// Number of frames/pages needed for the buffer
		auto* vbe_mode_info = (vbe_mode_info_t*)(uintptr_t)x86_multiboot_info->vbe_mode_info;
		uint32_t frameBufferSize   = vbe_ptr->getVideoMemorySize();
		uint32_t frameBufferFrames = (frameBufferSize >> PAGE_BITS) + 1;

		for (int i = 0; i < frameBufferFrames; ++i)
		{
			PalmyraOS::kernel::PhysicalMemory::reserveFrame(
				(void*)(vbe_mode_info->framebuffer + (i << PAGE_BITS))
			);
		}
	}
	return true;
}

bool PalmyraOS::kernel::initializeVirtualMemory(multiboot_info_t* x86_multiboot_info)
{
	/**
	 * @brief Initializes the virtual memory manager.
	 *
	 * This function initializes the kernel paging directory, maps the kernel and video memory by identity,
	 * switches to the new kernel paging directory, and initializes the paging system.
	 *
	 * @param x86_multiboot_info Pointer to the multiboot information structure.
	 * @return True if the virtual memory manager is successfully initialized, false otherwise.
	 */

	/* Here we assume physical memory has been initialized, hence we do not use kmalloc anymore
	 * Instead, we use PhysicalMemory::allocateFrames()
	*/

	// Initialize and ensure kernel directory is aligned ~ 8 KiB = 3 frames
	uint32_t PagingDirectoryFrames = (sizeof(PagingDirectory) >> PAGE_BITS) + 1;
	kernel::kernelPagingDirectory_ptr = (PagingDirectory*)PhysicalMemory::allocateFrames(PagingDirectoryFrames);

	// Ensure the pointer we have is aligned
	if ((uint32_t)kernel::kernelPagingDirectory_ptr & (PAGE_SIZE - 1))
		kernel::kernelPanic("Unaligned Kernel Directory at 0x%X", kernel::kernelPagingDirectory_ptr);

	// Map kernel space by identity
	{
		auto     kernelSpace = (uint32_t)PhysicalMemory::allocateFrame();
		kernel::kernelLastPage = kernelSpace >> PAGE_BITS;
		kernel::kernelPagingDirectory_ptr->mapPages(
			nullptr,
			nullptr,
			kernel::kernelLastPage,
			PageFlags::Present | PageFlags::ReadWrite
		);
	}

	// Map video memory by identity
	auto* vbe_mode_info = (vbe_mode_info_t*)(uintptr_t)x86_multiboot_info->vbe_mode_info;
	{
		uint32_t frameBufferSize   = vbe_ptr->getVideoMemorySize();
		uint32_t frameBufferFrames = (frameBufferSize >> PAGE_BITS) + 1;
		kernel::kernelPagingDirectory_ptr->mapPages(
			(void*)vbe_mode_info->framebuffer,
			(void*)vbe_mode_info->framebuffer,
			frameBufferFrames,
			PageFlags::Present | PageFlags::ReadWrite
		);
	}

	// Switch to the new kernel paging directory and initialize paging
	PalmyraOS::kernel::PagingManager::switchPageDirectory(kernel::kernelPagingDirectory_ptr);
	PalmyraOS::kernel::PagingManager::initialize();

	return true;
}

void PalmyraOS::kernel::testMemory()
{
	/**
	 * @brief Tests the memory system.
	 *
	 * This function performs various tests on the paging and heap systems
	 * to ensure their correct operation.
	 */

	// paging
	if (!Tests::Paging::testPagingBoundaries())
		kernel::kernelPanic("Testing Paging boundaries failed!");

	if (!Tests::Paging::testPageTableAllocation())
		kernel::kernelPanic("Testing Paging table allocation failed!");

	if (!Tests::Paging::testNullPointerException())
		kernel::kernelPanic("Testing Paging nullptr allocation failed!");

	// heap
	if (!Tests::Heap::testHeapAllocation())
		kernel::kernelPanic("Testing Heap allocation failed!");

//	if (!Tests::Heap::testHeapCoalescence())
//		kernel::kernelPanic("Testing Heap Coalescence failed!");

	// standard library
	if (!Tests::Allocator::testVector())
		kernel::kernelPanic("Testing Allocator Vector failed!");

	if (!Tests::Allocator::testVectorOfClasses())
		kernel::kernelPanic("Testing Allocator Vector Classes failed!");

	if (!Tests::Allocator::testString())
		kernel::kernelPanic("Testing Allocator String failed!");

}
