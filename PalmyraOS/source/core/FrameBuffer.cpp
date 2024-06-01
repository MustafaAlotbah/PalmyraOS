
#include "core/FrameBuffer.h"
#include "libs/memory.h"    // memcpy



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
	memcpy(buffer_, backBuffer_, width_ * height_);
}

uint16_t PalmyraOS::kernel::FrameBuffer::getWidth() const
{
	return width_;
}

uint16_t PalmyraOS::kernel::FrameBuffer::getHeight() const
{
	return height_;
}
