
#pragma once

#include <cstdint>

typedef struct {
	uint16_t attributes;                // Mode attributes (e.g., supported, color, graphics)
	uint8_t window_a, window_b;         // Window identifiers for accessing video memory
	uint16_t granularity;               // Window granularity in kilobytes
	uint16_t window_size;               // Window size in kilobytes
	uint16_t segment_a, segment_b;      // Window segments in real mode addressing
	uint32_t win_func_ptr;              // Pointer to window function in real mode

	uint16_t pitch;                     // Bytes per scanline, important for calculating memory layout

	uint16_t width, height;             // Screen width and height in pixels
	uint8_t w_char, y_char;             // Character cell dimensions in pixels
	uint8_t planes;                     // Number of memory planes
	uint8_t bpp;                        // Bits per pixel
	uint8_t banks;                      // Number of banks in banked mode
	uint8_t memory_model;               // Type of memory model (e.g., text, CGA, linear)
	uint8_t bank_size;                  // Size of each bank in kilobytes
	uint8_t image_pages;                // Number of images pages that fit in memory
	uint8_t reserved0;                  // Reserved byte for alignment padding

	uint8_t red_mask, red_position;       // Mask and position of red component
	uint8_t green_mask, green_position;     // Mask and position of green component
	uint8_t blue_mask, blue_position;      // Mask and position of blue component
	uint8_t reserved_mask, reserved_position;  // Masks and positions for reserved field
	uint8_t direct_color_attributes;            // How color information is interpreted

	uint32_t framebuffer;                       // Physical address of the linear framebuffer
	uint32_t off_screen_mem_off;                // Pointer to start of off screen memory
	uint16_t off_screen_mem_size;               // Amount of off screen memory in kilobytes
	uint8_t reserved1[206];             // Padding to make the structure a certain size or for future expansion
} __attribute__((packed)) vbe_mode_info_t;

typedef struct {
	char signature[4];               // Signature to identify VBE; should be "VESA"
	uint16_t version;                // VBE version number
	uint32_t oem;                    // Pointer to OEM string
	uint32_t capabilities;           // Capabilities of the graphics controller
	uint32_t video_modes;            // Pointer to an array of supported video mode pointers
	uint16_t video_memory;           // Number of 64KB memory blocks available for video

	uint16_t software_rev;           // Software revision number
	uint32_t vendor;                 // Pointer to vendor name string
	uint32_t product_name;           // Pointer to product name string
	uint32_t product_rev;            // Pointer to product revision string

	uint8_t reserved[222];           // Reserved for future use
	uint8_t oem_data[256];           // OEM BIOSes can store their own data here
} __attribute__((packed)) vbe_control_info_t;

typedef struct {
	/* Multiboot info version number */
	uint32_t flags;

	/* Available memory from BIOS */
	uint32_t mem_lower;     // Amount of lower memory in kilobytes. Lower memory starts at address 0.
	uint32_t mem_upper;     // Amount of upper memory in kilobytes. Upper memory starts at address 1 megabyte.

	/* "root" partition */
	uint32_t boot_device;   // Encoded value indicating the BIOS disk device number and partition from which the boot was performed

	/* Kernel command line */
	uint32_t cmdline;       // Physical address of the command line to be passed to the kernel; a zero-terminated ASCII string

	/*----Boot-Module list--------*/
	uint32_t mods_count;    // Number of boot modules loaded along with the kernel
	void* mods_addr;        // Physical address of the first module structure

	/*----------------------------*/
	uint32_t num;           // Number of entries in the symbol table, used if flags indicate a symbol table is present
	uint32_t size;          // Size of each entry in the symbol table
	void* addr;             // Physical address of the first entry in the symbol table
	uint32_t shndx;         // Section number of the string table used as the index into the symbol table

	/*----------------------------*/
	uint32_t mmap_length;   // Total size of the memory map buffer
	void* mmap_addr;        // Physical address of the memory map buffer
	uint32_t drives_length; // Total size of the drives buffer
	void* drives_addr;      // Physical address of the first drive structure

	/*----ConfigTable----------------*/
	uint32_t config_table;  // Physical address of the ROM configuration table returned by the BIOS

	/*----Bootloader-----------------*/
	char* boot_loader_name; // Physical address of a zero terminated string containing the name of the bootloader

	/*----APM Table------------------*/
	uint32_t apm_table;     // Physical address of the APM table

	/*-----Video---------------------*/
	vbe_control_info_t* vbe_control_info;   // Pointer to the VBE control information returned by the VBE function 00h
	vbe_mode_info_t* vbe_mode_info;         // Pointer to the VBE mode information returned by the VBE function 01h
	uint16_t vbe_mode;                      // Current VBE mode
	uint16_t vbe_interface_seg;             // Real mode segment of a protected mode interface
	uint32_t vbe_interface_off;             // Offset of a protected mode interface
	uint32_t vbe_interface_len;             // Length of the VBE protected mode interface

	/*----FrameBuffer----------------*/
	char* framebuffer_addr;         // Physical address of the linear framebuffer; valid if the framebuffer_type is not 0
	uint32_t framebuffer_pitch;     // Number of bytes per scanline of the framebuffer
	uint32_t framebuffer_width;     // Width of the framebuffer in pixels
	uint32_t framebuffer_height;    // Height of the framebuffer in pixels
	uint32_t framebuffer_bpp;       // Bits per pixel of the framebuffer
	uint32_t framebuffer_type;      // Type of framebuffer; 0 for indexed color, 1 for RGB, 2 for EGA text

	/*----------------------------*/
	uint32_t color_info;            // Additional color information, specific use depends on context

} multiboot_info_t;

#define COLOR(A, R, G, B) ((uint32_t)(A) << 24 | (uint32_t)(R) << 16 | (uint32_t)(G) << 8 | (uint32_t)(B))

