#include "boot/multiboot.h"
#include "core/FrameBuffer.h"
#include "core/Font.h"
#include "core/VBE.h"
#include "core/kernel.h"
#include "core/panic.h"

// Pointers to the start and end of the constructors section (see linker.ld)
extern "C" void (* first_constructor)();
extern "C" void (* last_constructor)();
extern "C" void enable_protected_mode();

// Functions with C linkage for compatibility with assembly
extern "C" [[noreturn]] [[maybe_unused]] void kernelEntry(multiboot_info_t* x86_multiboot_info);

// Calls all global constructors; this is invoked before kernelMain (see loader in loader.s)
void callConstructors()
{
	// Iterate through the constructors and call each one
	// i: pointer to function address
	for (void (** i)() = &first_constructor; i != &last_constructor; i++)
	{
		(*i)();
	}
}

// Entry point of the kernel; this function is called by the bootloader
// This function should initialize and set up the CPU and vital kernel variables
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
	kernel::VBE          vbe(vbe_mode_info, vbe_control_info, (uint32_t*)0x00E6'0000);
	kernel::vbe_ptr = &vbe;

	// initialize the fonts
	fonts::FontManager::initialize();

	kernel::setup();

	// should never arrive here
	while (true);
}

[[noreturn]] void PalmyraOS::kernel::setup()
{

	// reference system variables
	auto& vbe = *vbe_ptr;

	// render some information
	kernel::Brush        brush(vbe.getFrameBuffer());
	kernel::TextRenderer textRenderer(vbe.getFrameBuffer(), fonts::FontManager::getFont("Arial-12"));

	// dummy time just to make sure the loop is running smoothly
	uint64_t dummy_up_time = 0;

//	kernelPanic("wow sth happened %d", 456);

	// Infinite loop to cycle through colors and fill the screen
	while (true)
	{
		dummy_up_time++;

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
		textRenderer << "Time: " << dummy_up_time << "\n";
		textRenderer.reset();

		// update video memory
		vbe.getFrameBuffer().swapBuffers();
	}


	// should never arrive here
	while (true);
}