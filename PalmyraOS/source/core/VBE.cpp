
#include "core/VBE.h"
#include "libs/stdio.h"        // snprintf
#include "libs/string.h"

///region PalmyraOS::kernel::Brush

PalmyraOS::kernel::Brush::Brush(PalmyraOS::kernel::FrameBuffer& frameBuffer)
	: frameBuffer_(frameBuffer)
{}

void PalmyraOS::kernel::Brush::fill(PalmyraOS::Color color)
{
	frameBuffer_.fill(color);
}

void PalmyraOS::kernel::Brush::fillRectangle(uint32_t x1, uint32_t y1, uint32_t x2, uint32_t y2, PalmyraOS::Color color)
{
	frameBuffer_.drawRect(x1, y1, x2, y2, color);
}

void PalmyraOS::kernel::Brush::drawPoint(uint32_t x, uint32_t y, PalmyraOS::Color color)
{
	frameBuffer_.drawPixel(x, y, color);
}

void PalmyraOS::kernel::Brush::drawVLine(uint32_t x, uint32_t y1, uint32_t y2, PalmyraOS::Color color)
{
	uint32_t* videoAddr = frameBuffer_.getBackBuffer();

	uint32_t width  = frameBuffer_.getWidth();
	uint32_t height = frameBuffer_.getHeight();

	if (x >= width) return; // x-coordinate out of bounds

	if (y1 > y2)
	{
		uint32_t temp = y1;
		y2 = y1;
		y1 = temp;
	}

	if (y1 >= height) return; // y1 is out of bounds
	if (y2 >= height) y2 = height - 1; // Clip y2 to the maximum height

	for (uint32_t y = y1; y <= y2; ++y)
	{
		videoAddr[y * width + x] = color.color;
	}
}

void PalmyraOS::kernel::Brush::drawHLine(uint32_t x1, uint32_t x2, uint32_t y, PalmyraOS::Color color)
{
	uint32_t* videoAddr = frameBuffer_.getBackBuffer();
	uint32_t width  = frameBuffer_.getWidth();
	uint32_t height = frameBuffer_.getHeight();

	if (y >= height) return; // y-coordinate out of bounds

	if (x1 > x2)
	{
		uint32_t temp = x1;
		x1 = x2;
		x2 = temp;
	}

	if (x1 >= width) return; // x1 is out of bounds
	if (x2 >= width) x2 = width - 1; // Clip x2 to the maximum width

	for (uint32_t x = x1; x <= x2; ++x)
	{
		videoAddr[y * width + x] = color.color;
	}
}

///endregion


///region PalmyraOS::kernel::TextRenderer

PalmyraOS::kernel::TextRenderer::TextRenderer(PalmyraOS::kernel::FrameBuffer& frameBuffer, PalmyraOS::fonts::Font& font)
	: frameBuffer_(frameBuffer), font_(font)
{}

void PalmyraOS::kernel::TextRenderer::reset()
{
	cursor_x = 0;
	cursor_y = 0;
}

void PalmyraOS::kernel::TextRenderer::putChar(char ch)
{
	// control characters
	if (ch == '\n')
	{    // new line
		cursor_x = 0;
		cursor_y += font_.getGlyph('A').height + lineSpacing_;
		return;
	}
	else if (ch == '\r')
	{    // go to line start
		cursor_x = 0;
		return;
	}
	else if (ch == '\t')
	{    // add a tab
		const size_t characterWidth = font_.getGlyph(' ').width;
		size_t       nextTabStop    = ((cursor_x / (tabSize_ * characterWidth)) + 1) * (tabSize_ * characterWidth);
		cursor_x = nextTabStop;
		return;
	}

	// get the frame buffer address
	uint32_t* backBuffer = frameBuffer_.getBackBuffer();

	// get Glyph from font
	auto& glyph = font_.getGlyph(ch); // TODO implement more parsing e.g. \u05468
	size_t advance_x = glyph.width + 0;
	size_t advance_y = glyph.height + 3;

	for (size_t _x = 0; _x < glyph.width; _x += 1)
	{
		for (size_t _y = 0; _y < glyph.height; _y += 1)
		{
			if ((glyph.bitmap[_x] & (1 << _y)) == (1 << _y))
			{

				uint32_t x_ = position_x + cursor_x + _x;
				uint32_t y_ = position_y + cursor_y + glyph.offsetY + glyph.height - _y;

				uint32_t index = x_ + (y_ * frameBuffer_.getWidth());
				backBuffer[index] = textColor_.getColorValue();
			}
		}
	}

	cursor_x += advance_x;

	// Handle line wrap
	if (cursor_x >= position_x + width)
	{
		cursor_x = 0;
		position_y += advance_y;
	}


}

void PalmyraOS::kernel::TextRenderer::putString(const char* str)
{
	// important to ensure end of for loop
	uint64_t    max = PalmyraOS::Constants::MaximumStackBuffer;
	for (size_t i   = 0; str[i] != 0; ++i)
	{
		putChar(str[i]);
		if (i >= max) break;
	}
}

PalmyraOS::kernel::TextRenderer& PalmyraOS::kernel::TextRenderer::operator<<(char ch)
{
	putChar(ch);
	return *this;
}

PalmyraOS::kernel::TextRenderer& PalmyraOS::kernel::TextRenderer::operator<<(const char* str)
{
	putString(str);
	return *this;
}

PalmyraOS::kernel::TextRenderer& PalmyraOS::kernel::TextRenderer::operator<<(PalmyraOS::Color color)
{
	textColor_ = color;
	return *this;
}

char PalmyraOS::kernel::TextRenderer::getFormat()
{
	switch (representation)
	{
		case NumeralSystem::Dec:
			return 'd';
		case NumeralSystem::Hex:
			return 'x';
		case NumeralSystem::Bin:
			return 'b';
		default:
			return 'd';
	}
}

PalmyraOS::kernel::TextRenderer& PalmyraOS::kernel::TextRenderer::operator<<(uint64_t num)
{
	char buffer[1024];  // Adjust size as necessary for your needs
	char format[3] = { '%', getFormat(), '\0' };    // %d, %x ...
	snprintf(buffer, sizeof(buffer), format, num);
	putString(buffer);
	return *this;
}

PalmyraOS::kernel::TextRenderer& PalmyraOS::kernel::TextRenderer::operator<<(uint32_t num)
{
	char buffer[1024];  // Adjust size as necessary for your needs
	char format[3] = { '%', getFormat(), '\0' };    // %d, %x ...
	snprintf(buffer, sizeof(buffer), format, num);
	putString(buffer);
	return *this;
}

PalmyraOS::kernel::TextRenderer& PalmyraOS::kernel::TextRenderer::operator<<(uint16_t num)
{
	char buffer[1024];  // Adjust size as necessary for your needs
	char format[3] = { '%', getFormat(), '\0' };    // %d, %x ...
	snprintf(buffer, sizeof(buffer), format, num);
	putString(buffer);
	return *this;
}

PalmyraOS::kernel::TextRenderer& PalmyraOS::kernel::TextRenderer::operator<<(int num)
{
	return operator<<((uint32_t)num);
}

PalmyraOS::kernel::TextRenderer& PalmyraOS::kernel::TextRenderer::operator<<(PalmyraOS::kernel::TextRenderer::NumeralSystem system)
{
	representation = system;
	return *this;
}

void PalmyraOS::kernel::TextRenderer::setPosition(uint32_t x, uint32_t y)
{
	position_x = x;
	position_y = y;
}

void PalmyraOS::kernel::TextRenderer::setSize(uint32_t w, uint32_t h)
{
	width  = w;
	height = h;
}

///endregion


///region PalmyraOS::kernel::VBE


PalmyraOS::kernel::VBE::VBE(vbe_mode_info_t* mode_, vbe_control_info_t* control_, uint32_t* backBuffer)
	: frameBuffer_(mode_->width, mode_->height, (uint32_t*)(uintptr_t)mode_->framebuffer, backBuffer),
	  vbe_mode_info_(*mode_), vbe_control_info_(*control_)
{
	vbe_mode_info_t   & vbe_mode_info = *mode_;
	vbe_control_info_t& control       = *control_;
}

PalmyraOS::kernel::FrameBuffer& PalmyraOS::kernel::VBE::getFrameBuffer()
{
	return frameBuffer_;
}

void PalmyraOS::kernel::VBE::swapBuffers()
{
	frameBuffer_.swapBuffers();
}

size_t PalmyraOS::kernel::VBE::getWidth() const
{
	return vbe_mode_info_.width;
}

size_t PalmyraOS::kernel::VBE::getHeight() const
{
	return vbe_mode_info_.height;
}

size_t PalmyraOS::kernel::VBE::getVideoMemorySize() const
{
	return vbe_control_info_.video_memory * 64 * 1024; // video_memory is in 64KB blocks
}

size_t PalmyraOS::kernel::VBE::getColorDepth() const
{
	return vbe_mode_info_.bpp;
}

uint16_t PalmyraOS::kernel::VBE::getWindowAttributes() const
{
	return vbe_mode_info_.attributes;
}

bool PalmyraOS::kernel::VBE::isModeSupported() const
{
	return (vbe_mode_info_.attributes & 0x0001) != 0;
}

// Check if optional hardware functions are available
bool PalmyraOS::kernel::VBE::isOptionalHardwareSupported() const
{
	return (vbe_mode_info_.attributes & 0x0002) != 0;
}

// Check if the mode is supported for BIOS output functions
bool PalmyraOS::kernel::VBE::isBiosOutputSupported() const
{
	return (vbe_mode_info_.attributes & 0x0004) != 0;
}

// Check if the mode is a color mode
bool PalmyraOS::kernel::VBE::isColorMode() const
{
	return (vbe_mode_info_.attributes & 0x0008) != 0;
}

// Check if the mode is a graphics mode
bool PalmyraOS::kernel::VBE::isGraphicsMode() const
{
	return (vbe_mode_info_.attributes & 0x0010) != 0;
}

// Check if the mode supports VGA-compatible windowed memory paging
bool PalmyraOS::kernel::VBE::isVGACompatibleWindowedMemoryPagingSupported() const
{
	return (vbe_mode_info_.attributes & 0x0020) != 0;
}

// Get the type of memory model (e.g., text, CGA, linear)
uint8_t PalmyraOS::kernel::VBE::getMemoryModel() const
{
	return vbe_mode_info_.memory_model;
}

const char* PalmyraOS::kernel::VBE::listVideoModes() const
{
	// Pointer to the array of video mode pointers
	auto* video_modes = reinterpret_cast<uint16_t*>(vbe_control_info_.video_modes);

	// Static buffer to store the result (ensure it is large enough to hold the mode list)
	static char result[1024];
	result[0] = '\0'; // Initialize to empty string

	// String buffer to store each mode
	char buffer[16];

	// Loop through the list of video modes until we encounter 0xFFFF
	if (video_modes == nullptr)
	{
		strcat(result, "No video modes available.\n");
	}
	else
	{
		strcat(result, "Supported Video Modes");
		strcat(result, ": (");
		for (uint16_t* mode = video_modes; *mode != 0xFFFF; ++mode)
		{
			snprintf(buffer, sizeof(buffer), "%x, ", *mode);
			strcat(result, buffer);
		}
		strcat(result, ")");
	}

	return result;
}

///endregion
