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

// Pointers to the start and end of the constructors section (see linker.ld)
extern "C" void (* first_constructor)();
extern "C" void (* last_constructor)();

// Enable protected mode. Defined in bootloader.asm.
extern "C" void enable_protected_mode();

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

	// first construct globals
	callConstructors();

	// enter protected mode (32-bit)
	enable_protected_mode();

	// Retrieve VBE mode information from the multiboot info structure
	auto* vbe_mode_info = (vbe_mode_info_t*)(uintptr_t)x86_multiboot_info->vbe_mode_info;
	auto* vbe_control_info = (vbe_control_info_t*)(uintptr_t)x86_multiboot_info->vbe_control_info;

	// main block (so it is never destructed)
	kernel::VBE vbe(vbe_mode_info, vbe_control_info, (uint32_t*)0x00E6'0000);
	kernel::vbe_ptr = &vbe;

	// initialize the fonts and set up the kernel
	fonts::FontManager::initialize();
	kernel::setup();

	// should never arrive here
	while (true);
}

/**
 * Kernel setup function that initializes various subsystems and enters the main loop.
 */
[[noreturn]] void PalmyraOS::kernel::setup()
{
	// reference system variables
	auto& vbe = *vbe_ptr;

	// render some information
	kernel::Brush        brush(vbe.getFrameBuffer());
	kernel::TextRenderer textRenderer(vbe.getFrameBuffer(), fonts::FontManager::getFont("Arial-12"));

	// Logo
	brush.fill(Color::Black);
	textRenderer << Color::Orange << "Palmyra" << Color::LightBlue << "OS ";
	textRenderer << Color::White << "v0.01\n";
	brush.drawHLine(1, 150, textRenderer.getCursorY() + 2, Color::White);
	vbe.swapBuffers();

	// initialize the global descriptor table with the kernel stack at 30 MB offset
	PalmyraOS::kernel::GDT::GlobalDescriptorTable gdt(30 * 1024 * 1024);
	textRenderer << "Loaded GDT\n";
	vbe.swapBuffers();

	// initialize the interrupts
	PalmyraOS::kernel::interrupts::InterruptController interruptController(&gdt);
	textRenderer << "Loaded IDT\n";
	vbe.swapBuffers();

	// initialize system clock with frequency of 250 Hz
	PalmyraOS::kernel::SystemClock::initialize(250);

	// Now enable maskable interrupts
	PalmyraOS::kernel::interrupts::InterruptController::enableInterrupts();

	// dummy time just to make sure the loop is running smoothly
	uint64_t dummy_up_time = PalmyraOS::kernel::SystemClock::getTicks();

	while (true)
	{
		// Clear the screen
		update(dummy_up_time);

		// update the up-time
		dummy_up_time = PalmyraOS::kernel::SystemClock::getTicks();

		// update video memory
		vbe.swapBuffers();
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
	brush.fill(Color::Black);

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

	// ---
	uint64_t fps = (PalmyraOS::kernel::SystemClock::getTicks() - up_time);
	textRenderer << "Ticks per Frame: " << fps << " \n";
	textRenderer << "System Time: " << PalmyraOS::kernel::SystemClock::getSeconds() << " s\n";


	textRenderer.reset();
}