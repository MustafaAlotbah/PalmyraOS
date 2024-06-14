
#include "core/FrameBuffer.h"
#include "libs/memory.h"    // memcpy


extern "C" void* memcpy_sse(void* destination, const void* source, size_t num);


// PalmyraOS::Color
PalmyraOS::Color::Color(uint8_t red, uint8_t green, uint8_t blue, uint8_t alpha)
	: red(red), green(green), blue(blue), alpha(alpha)
{}

PalmyraOS::Color::Color(uint32_t color)
	: color(color)
{}

PalmyraOS::Color::Color()
	: color(0xFF000000)
{}

uint32_t PalmyraOS::Color::getColorValue() const
{
	return color;
}

// Define the static color constants
const PalmyraOS::Color PalmyraOS::Color::Red        = Color(252, 3, 32);
const PalmyraOS::Color PalmyraOS::Color::Green      = Color(90, 252, 3);
const PalmyraOS::Color PalmyraOS::Color::Blue       = Color(3, 53, 252);
const PalmyraOS::Color PalmyraOS::Color::Cyan       = Color(3, 173, 252);
const PalmyraOS::Color PalmyraOS::Color::Magenta    = Color(252, 3, 198);
const PalmyraOS::Color PalmyraOS::Color::Yellow     = Color(223, 252, 3);
const PalmyraOS::Color PalmyraOS::Color::Black      = Color(2, 2, 2);
const PalmyraOS::Color PalmyraOS::Color::White      = Color(245, 245, 245);
const PalmyraOS::Color PalmyraOS::Color::Gray       = Color(128, 128, 128);
const PalmyraOS::Color PalmyraOS::Color::DarkGray = Color(32, 32, 32);
const PalmyraOS::Color PalmyraOS::Color::DarkRed    = Color(145, 4, 21);
const PalmyraOS::Color PalmyraOS::Color::DarkGreen  = Color(58, 122, 9);
const PalmyraOS::Color PalmyraOS::Color::DarkBlue   = Color(10, 44, 145);
const PalmyraOS::Color PalmyraOS::Color::LightRed   = Color(237, 116, 134);
const PalmyraOS::Color PalmyraOS::Color::LightGreen = Color(144, 238, 144);
const PalmyraOS::Color PalmyraOS::Color::LightBlue  = Color(173, 216, 230);
const PalmyraOS::Color PalmyraOS::Color::Orange     = Color(252, 177, 3);

// PalmyraOS::kernel::FrameBuffer

PalmyraOS::kernel::FrameBuffer::FrameBuffer(
	uint16_t width,
	uint16_t height,
	uint32_t* frontBuffer,
	uint32_t* backBuffer
)
{
	width_      = width;
	height_     = height;
	buffer_     = frontBuffer;
	backBuffer_ = backBuffer;
}

uint32_t* PalmyraOS::kernel::FrameBuffer::getBackBuffer()
{
	return backBuffer_;
}

void PalmyraOS::kernel::FrameBuffer::fill(PalmyraOS::Color color)
{
	for (uint32_t i = 0; i < height_ * width_; ++i)
		backBuffer_[i] = color.getColorValue();
}

void PalmyraOS::kernel::FrameBuffer::drawRect(
	uint32_t x1,
	uint32_t y1,
	uint32_t x2,
	uint32_t y2,
	PalmyraOS::Color color
)
{
	for (uint32_t y = y1; y < y2; ++y)
	{
		for (uint32_t x = x1; x < x2; ++x)
		{
			uint32_t index = x + (y * width_);
			backBuffer_[index] = color.getColorValue();
		}
	}
}

void PalmyraOS::kernel::FrameBuffer::drawPixel(
	uint32_t x,
	uint32_t y,
	PalmyraOS::Color color
)
{
	uint32_t index = x + (y * width_);
	backBuffer_[index] = color.getColorValue();
}

void PalmyraOS::kernel::FrameBuffer::swapBuffers()
{
//	memcpy(buffer_, backBuffer_, width_ * height_);
	memcpy_sse(buffer_, backBuffer_, width_ * height_ * sizeof(uint32_t));
}

uint16_t PalmyraOS::kernel::FrameBuffer::getWidth() const
{
	return width_;
}

uint16_t PalmyraOS::kernel::FrameBuffer::getHeight() const
{
	return height_;
}

uint32_t PalmyraOS::kernel::FrameBuffer::getSize() const
{
	return width_ * height_ * sizeof(uint32_t);
}
