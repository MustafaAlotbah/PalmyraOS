#include "boot/multiboot.h"

// Pointers to the start and end of the constructors section (see linker.ld)
extern "C" void (* first_constructor)();
extern "C" void (* last_constructor)();

// Functions with C linkage for compatibility with assembly
extern "C" [[noreturn]] [[maybe_unused]] void kernelEntry(multiboot_info_t* x86_multiboot_info);

// Calls all global constructors; this is invoked before kernelMain (see loader in loader.s)
void callConstructors()
{
	// Iterate through the constructors and call each one
	// i: pointer to function address
	for (void (** i)() = &first_constructor; i!=&last_constructor; i++) {
		(*i)();
	}
}

// Function to fill the screen with a specified color
void fill_screen(uint32_t color, uint32_t* framebuffer, uint16_t frameWidth, uint16_t frameHeight);

// Entry point of the kernel; this function is called by the bootloader
[[noreturn]] void kernelEntry(multiboot_info_t* x86_multiboot_info)
{
	// first construct globals
	callConstructors();

	// test VBE

	// Retrieve VBE mode information from the multiboot info structure
	auto* vbe_mode_info = (vbe_mode_info_t*)(uintptr_t)x86_multiboot_info->vbe_mode_info;
	auto frameWidth = vbe_mode_info->width;
	auto frameHeight = vbe_mode_info->height;
	auto* framebuffer = (uint32_t*)(uintptr_t)vbe_mode_info->framebuffer;

	// Initialize color components to zero
	uint32_t red = 0x00;
	uint32_t green = 0x00;
	uint32_t blue = 0x00;

	// Infinite loop to cycle through colors and fill the screen
	while (true) {
		if (red<240) red += 1;
		else if (green<240) green += 1;
		else if (blue<240) blue += 1;
		else {
			red = 0x00;
			green = 0x00;
			blue = 0x00;
		}

		// Create a 32-bit color value (ARGB format)
		uint32_t color = COLOR(0xFF, red, green, blue);

		// Fill the screen with the current color
		fill_screen(color, framebuffer, frameWidth, frameHeight);
	}
}

// Function to fill the entire screen with a specified color
void fill_screen(uint32_t color, uint32_t* framebuffer, uint16_t frameWidth, uint16_t frameHeight)
{
	for (uint16_t y = 0; y<frameHeight; ++y) {
		for (uint16_t x = 0; x<frameWidth; ++x) {
			uint32_t index = x+(y*frameWidth);  // Calculate the index in the framebuffer
			framebuffer[index] = color;             // Set the pixel at the calculated index to the color
		}
	}
}