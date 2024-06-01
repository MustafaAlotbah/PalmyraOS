#include "boot/multiboot.h"
#include "core/FrameBuffer.h"

// Pointers to the start and end of the constructors section (see linker.ld)
extern "C" void (* first_constructor)();
extern "C" void (* last_constructor)();
extern "C" void enable_protected_mode();
extern "C" void enable_sse();
extern "C" void enable_sse2();

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
[[noreturn]] void kernelEntry(multiboot_info_t* x86_multiboot_info)
{
	using namespace PalmyraOS;
	// first construct globals
	callConstructors();

	enable_protected_mode();
	enable_sse();
	// test VBE

	// Retrieve VBE mode information from the multiboot info structure
	auto* vbe_mode_info = (vbe_mode_info_t*)(uintptr_t)x86_multiboot_info->vbe_mode_info;
	auto frameWidth  = vbe_mode_info->width;
	auto frameHeight = vbe_mode_info->height;
	auto* framebufferAddr = (uint32_t*)(uintptr_t)vbe_mode_info->framebuffer;


	fonts::FontManager::initialize();

	kernel::FrameBuffer frameBuffer(frameWidth, frameHeight, framebufferAddr, (uint32_t*)0x00E6'0000);

	// Initialize color components to zero
	uint32_t red   = 0x00;
	uint32_t green = 0x00;
	uint32_t blue  = 0x00;

	// Infinite loop to cycle through colors and fill the screen
	while (true)
	{
		if (red < 240) red += 1;
		else if (green < 240) green += 1;
		else if (blue < 240) blue += 1;
		else
		{
			red   = 0x00;
			green = 0x00;
			blue  = 0x00;
		}

		// Fill the screen with the current color
		frameBuffer.fill(Color(red, green, blue));
		frameBuffer.swapBuffers();
	}
}
