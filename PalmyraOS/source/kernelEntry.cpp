#include "boot/multiboot.h"
#include "core/FrameBuffer.h"
#include "core/Font.h"
#include "core/VBE.h"
#include "core/kernel.h"
#include "core/panic.h"
#include "core/cpu.h"
#include "core/GlobalDescriptorTable.h"
#include "core/Interrupts.h"
#include "core/SystemClock.h"
#include "core/memory/PhysicalMemory.h"
#include "core/memory/Heap.h"
//#include "core/paging.h"
#include "core/Logger.h"


// Pointers to the start and end of the constructors section (see linker.ld)
extern "C" void (* first_constructor)();
extern "C" void (* last_constructor)();

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

// Enable protected mode. Defined in bootloader.asm.
extern "C" void enable_protected_mode();
extern "C" void enable_sse();
extern "C" uint32_t test_sse();
extern "C" uint32_t get_kernel_stack_start();
extern "C" uint32_t get_kernel_stack_end();
extern "C" uint32_t get_esp();
extern "C" uint32_t get_ss();

/**
 * Kernel entry point called from the bootloader with multiboot information.
 * @param x86_multiboot_info Pointer to the multiboot information structure.
 */
extern "C" [[noreturn]] [[maybe_unused]] void kernelEntry(multiboot_info_t* x86_multiboot_info);

//  Calls all constructors for global/static objects.
void callConstructors()
{
	// Iterate through the constructors and call each one
	// i: pointer to function address
	for (void (** i)() = &first_constructor; i != &last_constructor; i++)
	{
		(*i)();
	}
}

/**
 * Kernel entry function that is called from the bootloader.
 * Initializes VBE, sets up the kernel, and enters the main loop.
 * @param x86_multiboot_info Pointer to the multiboot information structure.
 */
[[noreturn]] void kernelEntry(multiboot_info_t* x86_multiboot_info)
{
	using namespace PalmyraOS;

	// ----------------------- Call Kernel Constructors -----------------------
	// first construct globals
	callConstructors();

	// ----------------------- Enable Protected Mode --------------------------
	// enter protected mode (32-bit)
	enable_protected_mode();
	LOG_INFO("Entered protected mode.");
	LOG_INFO("Memory Lower: %d KiB", x86_multiboot_info->mem_lower);
	LOG_INFO("Memory Upper: %d KiB", x86_multiboot_info->mem_upper);

	enable_sse();
	LOG_INFO("Enabled SSE.");
	// ----------------------- Initialize Graphics ----------------------------

	// Retrieve VBE mode information from the multiboot info structure
	auto* vbe_mode_info    = (vbe_mode_info_t*)(uintptr_t)x86_multiboot_info->vbe_mode_info;
	auto* vbe_control_info = (vbe_control_info_t*)(uintptr_t)x86_multiboot_info->vbe_control_info;
	kernel::initializeGraphics(vbe_mode_info, vbe_control_info);
	LOG_INFO("Initialized Graphics.");

	// dereference graphics tools
	auto& textRenderer = *kernel::textRenderer_ptr;

	// ensure we have enough stack here
	if (get_esp() < get_kernel_stack_end())
		kernel::kernelPanic("Kernel Stack overflow by 0x%X bytes", (get_kernel_stack_end() - get_esp()));

	// Logo
	kernel::clearScreen(true);


	// ----------------------- Initialize Global Descriptor Tables -------------
	if (kernel::initializeGlobalDescriptorTable())
	{
		textRenderer << "Initialized GDT\n" << SWAP_BUFF();
		LOG_INFO("Initialized GDT.");
	}
	else
	{
		kernel::kernelPanic("Failed to initialize the GDT");
	}


	// ----------------------- Initialize Interrupts ----------------------------
	if (kernel::initializeInterrupts())
	{
		textRenderer << "Initialized Interrupts\n" << SWAP_BUFF();
		LOG_INFO("Initialized Interrupts.");
	}
	else
	{
		kernel::kernelPanic("Failed to initialize Interrupts");
	}

	// initialize system clock with frequency of 250 Hz
	PalmyraOS::kernel::SystemClock::initialize(kernel::SystemClockFrequency);
	LOG_INFO("Initialized System Clock at %d Hz.", kernel::SystemClockFrequency);

	// Now enable maskable interrupts
	PalmyraOS::kernel::interrupts::InterruptController::enableInterrupts();
	LOG_INFO("Enabled Interrupts.");


	// ----------------------- Initialize Physical Memory -------------------------------
	textRenderer << "Initializing Physical Memory\n" << SWAP_BUFF();
	kernel::initializePhysicalMemory(x86_multiboot_info);


	// ----------------------- Initialize Virtual  Memory -------------------------------
	textRenderer << "Initializing Virtual Memory\n" << SWAP_BUFF();
	kernel::initializeVirtualMemory(x86_multiboot_info);
	textRenderer << "Virtual Memory is initialized\n" << SWAP_BUFF();


	kernel::testMemory();


	LOG_INFO("Moving to setup()...");
	kernel::setup();


	// should never arrive here
	while (true);
}

/**
 * Kernel setup function that initializes various subsystems and enters the main loop.
 */
[[noreturn]] void PalmyraOS::kernel::setup()
{

	// dummy time just to make sure the loop is running smoothly
	uint64_t dummy_up_time = PalmyraOS::kernel::SystemClock::getTicks();

	while (true)
	{
		// Clear the screen
		update(dummy_up_time);

		// update the up-time
		dummy_up_time = PalmyraOS::kernel::SystemClock::getTicks();

		// update video memory
		vbe_ptr->swapBuffers();
	}


	// should never arrive here
	while (true);
}

void PalmyraOS::kernel::update(uint64_t up_time)
{
	// render some information
	auto& vbe = *vbe_ptr;
	kernel::Brush        brush(vbe.getFrameBuffer());
	kernel::TextRenderer textRenderer(vbe.getFrameBuffer(), fonts::FontManager::getFont("Arial-12"));

	// Clear the screen
	brush.fill(Color::DarkBlue);

	// Logo
	textRenderer << Color::Orange << "Palmyra" << Color::LightBlue << "OS ";
	textRenderer << Color::White << "v0.01\n";
	brush.drawHLine(1, 150, textRenderer.getCursorY() + 2, Color::White);
	textRenderer << "\n";

	// Information
	textRenderer << "Screen Resolution: " << vbe.getWidth() << "x" << vbe.getHeight() << "\n";
	textRenderer << "Video Memory: " << vbe.getVideoMemorySize() / 1024 / 1024 << " MB\n";
	textRenderer << "Memory Model Code: " << vbe.getMemoryModel() << "\n";

	// ---
	{
		textRenderer << "TSC: " << CPU::getTSC() << "\n";
	}

	textRenderer << "Logical Cores: " << CPU::getNumLogicalCores() << "\n";
	textRenderer << "Physical Cores: " << CPU::getNumPhysicalCores() << "\n";
	char buffer[128] = { 0 };
	CPU::getVendorID(buffer);
	textRenderer << "Vendor: '" << buffer << "'\n";
	CPU::getProcessorBrand(buffer);
	textRenderer << "Brand: '" << buffer << "'\n";

	textRenderer << "Features: [";
	textRenderer << (CPU::isSSEAvailable() ? "SSE " : "");
	textRenderer << (CPU::isSSE2Available() ? "SSE2 " : "");
	textRenderer << (CPU::isSSE3Available() ? "SSE3 " : "");
	textRenderer << (CPU::isSSSE3Available() ? "SSSE3 " : "");
	textRenderer << (CPU::isSSE41Available() ? "SSSE41 " : "");
	textRenderer << (CPU::isSSE42Available() ? "SSSE42 " : "");
	textRenderer << (CPU::isAVXAvailable() ? "AVX " : "");
	textRenderer << (CPU::isAVX2Available() ? "AVX2 " : "");
	textRenderer << (CPU::isHyperThreadingAvailable() ? "HypT " : "");
	textRenderer << (CPU::is64BitSupported() ? "64BIT " : "");
	textRenderer << (CPU::isBMI1Available() ? "BMI1 " : "");
	textRenderer << (CPU::isBMI2Available() ? "BMI2 " : "");
	textRenderer << (CPU::isFMAAvailable() ? "FMA " : "");
	textRenderer << (CPU::isAESAvailable() ? "AES " : "");
	textRenderer << (CPU::isSHAAvailable() ? "SHA " : "");
	textRenderer << "]\n";

	textRenderer << "L Caches (KB): [";
	textRenderer << CPU::getL1CacheSize() << " ";
	textRenderer << CPU::getL2CacheSize() << " ";
	textRenderer << CPU::getL3CacheSize();
	textRenderer << "]\n";
	// --

	textRenderer << "Frames, Pages: " << PhysicalMemory::size() << " \n";

	textRenderer << "memory: " << HEX() << (uint32_t)&__mem_end << " \n";
	textRenderer << "multiboot: " << (uint32_t)&__mem_multiboot_start << ", " << (uint32_t)&__mem_multiboot_end
				 << " \n";
	textRenderer << "rodata: " << (uint32_t)&__mem_rodata_start << ", " << (uint32_t)&__mem_rodata_end << " \n";
	textRenderer << "text: " << (uint32_t)&__mem_text_start << ", " << (uint32_t)&__mem_text_end << " \n";
	textRenderer << "cons: " << (uint32_t)&__mem_cons_start << ", " << (uint32_t)&__mem_cons_end << " \n";
	textRenderer << "data: " << (uint32_t)&__mem_data_start << ", " << (uint32_t)&__mem_data_end << " \n";
	textRenderer << "bss: " << (uint32_t)&__mem_bss_start << ", " << (uint32_t)&__mem_bss_end << " \n";
	textRenderer << "end: " << (uint32_t)&__end << " \n";
	textRenderer << "placement: " << placement_address << DEC() << " \n";

	// Draw the memory usage on screen
	{
		uint32_t index   = 0;
		bool     doBreak = false;

		for (int y = 400; y < 599; ++y)
		{
			for (int x = 0; x < 800; ++x)
			{
				brush.drawPoint(
					x, y,
					PhysicalMemory::isFrameFree((void*)(index << 12)) ? (
						(index < (placement_address >> 12)) ? Color::Orange : Color::Red
					) : Color::LightBlue
				);
				index++;

				if (index >= PhysicalMemory::size()) doBreak = true;
				if (doBreak) break;
			}
			if (doBreak) break;
		}
	}


	// ---
	uint64_t fps = (PalmyraOS::kernel::SystemClock::getTicks() - up_time);
	textRenderer << "Ticks per Frame: " << fps << " \n";
	textRenderer << "System Time: " << PalmyraOS::kernel::SystemClock::getSeconds() << " s\n";

	uint64_t ticks = PalmyraOS::kernel::SystemClock::getTicks() - up_time;
	uint32_t _fps  = (ticks > 0) ? (SystemClockFrequency * 1000 / ticks) : 0;
	textRenderer << "FPS: " << _fps << " \n";

	textRenderer.reset();
}


