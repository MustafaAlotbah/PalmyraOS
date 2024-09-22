
#include "core/panic.h"
#include "libs/stdio.h"
#include "core/kernel.h"
#include "core/Font.h"
#include "core/VBE.h"
#include "core/peripherals/Logger.h"

#include <cstdarg>  // Include for va_list and related functions


extern "C" void disable_interrupts();
extern "C" void disable_paging();


[[noreturn]] void kernelPanic_(const char* message)
{
	using namespace PalmyraOS;

	// first avoid any other interrupts
	disable_interrupts();

	// reference system variables
	auto& vbe = *PalmyraOS::kernel::vbe_ptr;

	// render some information
	kernel::Brush        brush(vbe.getFrameBuffer());
	kernel::TextRenderer textRenderer(vbe.getFrameBuffer(), Font::Arial12);

	// define text area boundaries
	uint16_t offset = 100;
	uint16_t width  = vbe.getWidth() - 2 * offset;
	uint16_t height = vbe.getHeight() - 2 * offset;

	textRenderer.setPosition(offset, offset);
	textRenderer.setSize(width, height);

	// define window boundaries
	uint16_t extendWindow  = 4;
	uint16_t window_offset = offset - extendWindow;
	uint16_t window_x2     = window_offset + width + 2 * extendWindow;
	uint16_t window_y2     = window_offset + height + 2 * extendWindow;

	// Window Background
	brush.fillRectangle(window_offset, window_offset, window_x2, window_y2, Color::DarkerGray);
	brush.drawHLine(window_offset, window_x2, window_offset, Color::Gray100);
	brush.drawHLine(window_offset, window_x2, window_y2, Color::Gray100);
	brush.drawVLine(window_offset, window_offset, window_y2, Color::Gray100);
	brush.drawVLine(window_x2, window_offset, window_y2, Color::Gray100);

	uint16_t window_bar_offset = window_offset + 2;
	uint16_t window_bar_x2     = window_x2 - 2;
	uint16_t window_bar_y2     = window_offset + 2 * extendWindow + 14;
	brush.fillRectangle(window_bar_offset, window_bar_offset, window_bar_x2, window_bar_y2, Color::DarkRed);

	textRenderer << Color::Orange << "Palmyra" << Color::LighterBlue << "OS ";
	textRenderer << Color::Gray100 << "Panic Screen\n";
	textRenderer.setCursor(0, textRenderer.getCursorY() + extendWindow);
	textRenderer << message;

	// update video memory
	vbe.getFrameBuffer().swapBuffers();

	LOG_ERROR("Panic");
	LOG_ERROR(message);

	// halt
	while (true);
}

void PalmyraOS::kernel::kernelPanic(const char* format, ...)
{
	disable_paging();

	va_list args;
	va_start(args, format);
	char buffer[4096];  // Adjust size as necessary for your needs
	vsnprintf(buffer, sizeof(buffer), format, args);
	va_end(args);

	kernelPanic_(buffer);
}