
#include "core/FrameBuffer.h"
#include "libs/memory.h"  // memcpy
#include <algorithm>


// PalmyraOS::Color
PalmyraOS::Color::Color(uint8_t red, uint8_t green, uint8_t blue, uint8_t alpha) : red(red), green(green), blue(blue), alpha(alpha) {}

PalmyraOS::Color::Color(uint32_t color) : color(color) {}

PalmyraOS::Color::Color() : color(0xFF000000) {}

uint32_t PalmyraOS::Color::getColorValue() const { return color; }

// Define the static color constants
/* Gray Scale */
const PalmyraOS::Color PalmyraOS::Color::White        = Color(255, 255, 255);
const PalmyraOS::Color PalmyraOS::Color::Gray100      = Color(245, 245, 245);
const PalmyraOS::Color PalmyraOS::Color::Gray200      = Color(238, 238, 238);
const PalmyraOS::Color PalmyraOS::Color::Gray300      = Color(224, 224, 224);
const PalmyraOS::Color PalmyraOS::Color::Gray400      = Color(189, 189, 189);
const PalmyraOS::Color PalmyraOS::Color::Gray500      = Color(158, 158, 158);
const PalmyraOS::Color PalmyraOS::Color::Gray600      = Color(117, 117, 117);
const PalmyraOS::Color PalmyraOS::Color::Gray700      = Color(97, 97, 97);
const PalmyraOS::Color PalmyraOS::Color::Gray800      = Color(66, 66, 66);
const PalmyraOS::Color PalmyraOS::Color::DarkGray     = Color(33, 33, 33);
const PalmyraOS::Color PalmyraOS::Color::DarkerGray   = Color(18, 18, 18);
const PalmyraOS::Color PalmyraOS::Color::Black        = Color(2, 2, 2);

/* Red Scale */
const PalmyraOS::Color PalmyraOS::Color::Red          = Color(252, 3, 32);
const PalmyraOS::Color PalmyraOS::Color::DarkRed      = Color(145, 4, 21);
const PalmyraOS::Color PalmyraOS::Color::DarkerRed    = Color(92, 6, 16);
const PalmyraOS::Color PalmyraOS::Color::Red100       = Color(255, 205, 210);
const PalmyraOS::Color PalmyraOS::Color::Red200       = Color(239, 154, 154);
const PalmyraOS::Color PalmyraOS::Color::Red300       = Color(229, 115, 115);
const PalmyraOS::Color PalmyraOS::Color::Red400       = Color(239, 83, 80);
const PalmyraOS::Color PalmyraOS::Color::Red500       = Color(244, 67, 54);
const PalmyraOS::Color PalmyraOS::Color::Red600       = Color(229, 57, 53);
const PalmyraOS::Color PalmyraOS::Color::Red700       = Color(211, 47, 47);
const PalmyraOS::Color PalmyraOS::Color::Red800       = Color(198, 40, 40);
const PalmyraOS::Color PalmyraOS::Color::Red900       = Color(183, 28, 28);

const PalmyraOS::Color PalmyraOS::Color::Green        = Color(90, 252, 3);
const PalmyraOS::Color PalmyraOS::Color::Blue         = Color(12, 91, 237);
const PalmyraOS::Color PalmyraOS::Color::Cyan         = Color(3, 173, 252);
const PalmyraOS::Color PalmyraOS::Color::Magenta      = Color(252, 3, 198);
const PalmyraOS::Color PalmyraOS::Color::Yellow       = Color(223, 252, 3);

const PalmyraOS::Color PalmyraOS::Color::DarkGreen    = Color(58, 122, 9);
const PalmyraOS::Color PalmyraOS::Color::DarkBlue     = Color(18, 61, 148);
const PalmyraOS::Color PalmyraOS::Color::LightRed     = Color(237, 116, 134);
const PalmyraOS::Color PalmyraOS::Color::LightGreen   = Color(144, 238, 144);
const PalmyraOS::Color PalmyraOS::Color::LightBlue    = Color(84, 144, 204);
const PalmyraOS::Color PalmyraOS::Color::LighterBlue  = Color(173, 216, 230);
const PalmyraOS::Color PalmyraOS::Color::Orange       = Color(252, 177, 3);

const PalmyraOS::Color PalmyraOS::Color::PrimaryDark  = Color(89, 97, 2);
const PalmyraOS::Color PalmyraOS::Color::Primary      = Color(148, 177, 0);
const PalmyraOS::Color PalmyraOS::Color::PrimaryLight = Color(196, 245, 68);
const PalmyraOS::Color PalmyraOS::Color::Secondary    = Color(214, 189, 152);

// PalmyraOS::kernel::FrameBuffer

PalmyraOS::kernel::FrameBuffer::FrameBuffer(uint16_t width, uint16_t height, uint32_t* frontBuffer, uint32_t* backBuffer) {
    width_      = width;
    height_     = height;
    buffer_     = frontBuffer;
    backBuffer_ = backBuffer;
}

uint32_t* PalmyraOS::kernel::FrameBuffer::getBackBuffer() { return backBuffer_; }

void PalmyraOS::kernel::FrameBuffer::fill(PalmyraOS::Color color) {
    for (uint32_t i = 0; i < height_ * width_; ++i) backBuffer_[i] = color.getColorValue();
}

void PalmyraOS::kernel::FrameBuffer::drawRect(uint32_t x1, uint32_t y1, uint32_t x2, uint32_t y2, PalmyraOS::Color color) {
    // Early exit if the frame buffer dimensions are zero
    if (width_ == 0 || height_ == 0) return;

    // Clamp x1 and x2 to [0, width_]
    x1 = std::min(x1, static_cast<uint32_t>(width_));
    x2 = std::min(x2, static_cast<uint32_t>(width_));

    // Swap x1 and x2 if necessary to ensure x1 <= x2
    if (x1 > x2) std::swap(x1, x2);

    // Clamp y1 and y2 to [0, height_]
    y1 = std::min(y1, static_cast<uint32_t>(height_));
    y2 = std::min(y2, static_cast<uint32_t>(height_));

    // Swap y1 and y2 if necessary to ensure y1 <= y2
    if (y1 > y2) std::swap(y1, y2);

    // If the rectangle has no area after clamping, exit early
    if (x1 == x2 || y1 == y2) return;

    uint32_t colorValue = color.getColorValue();
    uint32_t length     = x2 - x1;

    // Optimize by reducing the nested loops to a single loop per row
    for (uint32_t y = y1; y < y2; ++y) {
        uint32_t index = x1 + y * width_;
        uint32_t* dest = backBuffer_ + index;

        // Efficiently set the row pixels to the color value
        for (uint32_t i = 0; i < length; ++i) { dest[i] = colorValue; }
    }
}

void PalmyraOS::kernel::FrameBuffer::drawPixel(uint32_t x, uint32_t y, PalmyraOS::Color color) {
    uint32_t index     = x + (y * width_);
    backBuffer_[index] = color.getColorValue();
}

void PalmyraOS::kernel::FrameBuffer::swapBuffers() {
    //	memcpy(buffer_, backBuffer_, width_ * height_);
    memcpy_sse(buffer_, backBuffer_, width_ * height_ * sizeof(uint32_t));
}

uint16_t PalmyraOS::kernel::FrameBuffer::getWidth() const { return width_; }

uint16_t PalmyraOS::kernel::FrameBuffer::getHeight() const { return height_; }

uint32_t PalmyraOS::kernel::FrameBuffer::getSize() const { return width_ * height_ * sizeof(uint32_t); }
