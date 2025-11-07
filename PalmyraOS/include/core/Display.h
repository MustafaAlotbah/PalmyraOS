#pragma once

#include "core/Font.h"
#include "core/FrameBuffer.h"
#include "core/definitions.h"


/**
 * Macros to simplify usage of different numeral systems in text rendering.
 */
#define HEX() kernel::TextRenderer::NumeralSystem::Hex
#define DEC() kernel::TextRenderer::NumeralSystem::Dec
#define BIN() kernel::TextRenderer::NumeralSystem::Bin
#define SWAP_BUFF() kernel::TextRenderer::Command::SwapBuffers


namespace PalmyraOS::kernel {

    /**
     * Generic display driver for managing graphics output.
     * Works with any framebuffer source (VBE, UEFI GOP, Multiboot2, etc.)
     */
    class Display {
    public:
        /**
         * Constructor to initialize display with direct parameters.
         * @param width Display width in pixels
         * @param height Display height in pixels
         * @param framebufferAddress Physical address of the framebuffer
         * @param pitch Bytes per scanline (stride)
         * @param bitsPerPixel Bits per pixel (8, 16, 24, or 32)
         * @param backBuffer Pointer to the back buffer for double buffering
         */
        Display(uint16_t width, uint16_t height, uint32_t framebufferAddress, uint16_t pitch, uint8_t bitsPerPixel, uint32_t* backBuffer);

        /**
         * Swap the front and back buffers.
         */
        void swapBuffers();

        /**
         * Get display width in pixels.
         * @return Display width
         */
        [[nodiscard]] size_t getWidth() const;

        /**
         * Get display height in pixels.
         * @return Display height
         */
        [[nodiscard]] size_t getHeight() const;

        /**
         * Get the size of the video memory in bytes.
         * @return Video memory size
         */
        [[nodiscard]] size_t getVideoMemorySize() const;

        /**
         * Get the color depth (bits per pixel).
         * @return Bits per pixel
         */
        [[nodiscard]] size_t getColorDepth() const;

        /**
         * Get reference to the framebuffer.
         * @return FrameBuffer reference
         */
        FrameBuffer& getFrameBuffer();

        REMOVE_COPY(Display);

    private:
        FrameBuffer frameBuffer_;  // Frame buffer object
        uint16_t width_;           // Display width in pixels
        uint16_t height_;          // Display height in pixels
        uint8_t bitsPerPixel_;     // Bits per pixel (color depth)
        uint16_t pitch_;           // Bytes per scanline
    };

    /**
     * Class representing a brush for drawing operations on the frame buffer.
     */
    class Brush {
    public:
        /**
         * Constructor to initialize the brush with a frame buffer.
         * @param frameBuffer Reference to the frame buffer.
         */
        explicit Brush(FrameBuffer& frameBuffer);

        /**
         * Fill the entire frame buffer with a single color.
         * @param color The color to fill the frame buffer with.
         */
        void fill(Color color);

        /**
         * Fill a rectangle in the frame buffer with a specific color.
         * @param x1 The x-coordinate of the top-left corner.
         * @param y1 The y-coordinate of the top-left corner.
         * @param x2 The x-coordinate of the bottom-right corner.
         * @param y2 The y-coordinate of the bottom-right corner.
         * @param color The color to fill the rectangle with.
         */
        void fillRectangle(uint32_t x1, uint32_t y1, uint32_t x2, uint32_t y2, Color color);

        /**
         * Draw a rectangle in the frame buffer with a specific color.
         * @param x1 The x-coordinate of the top-left corner.
         * @param y1 The y-coordinate of the top-left corner.
         * @param x2 The x-coordinate of the bottom-right corner.
         * @param y2 The y-coordinate of the bottom-right corner.
         * @param color The color to fill the rectangle with.
         */
        void drawFrame(uint32_t x1, uint32_t y1, uint32_t x2, uint32_t y2, Color color);

        /**
         * Draw a circle using the Midpoint Circle Algorithm.
         * This function draws a circle with a given radius (r) centered at (cx, cy).
         * It can either draw just the outline of the circle or a filled circle depending on the `filled` parameter.
         *
         * @param cx The x-coordinate of the circle's center.
         * @param cy The y-coordinate of the circle's center.
         * @param r The radius of the circle.
         * @param color The color of the circle.
         * @param filled Boolean flag that indicates whether to fill the circle or draw only its perimeter.
         *               If true, the circle is filled; otherwise, only the perimeter is drawn.
         */
        void drawCircle(uint32_t cx, uint32_t cy, uint32_t r, PalmyraOS::Color color, bool filled = false);

        /**
         * Fill the entire circle with the specified color.
         * This function fills the circle by drawing symmetric horizontal scanlines within the bounds
         * of the circle using the Midpoint Circle Algorithm. It assumes the circle's center is at (cx, cy)
         * and uses the provided radius (r).
         *
         * @param cx The x-coordinate of the circle's center.
         * @param cy The y-coordinate of the circle's center.
         * @param r The radius of the circle.
         * @param color The color to fill the circle with.
         */
        void fillCircle(uint32_t cx, uint32_t cy, uint32_t r, PalmyraOS::Color color);

        /**
         * Plot symmetric points on the circle's perimeter using 8-way symmetry.
         * This function draws the 8 points of symmetry on the circle defined by its center (cx, cy) and the current
         * coordinates (x, y) from the midpoint circle algorithm.
         *
         * @param cx The x-coordinate of the circle's center.
         * @param cy The y-coordinate of the circle's center.
         * @param x The current x-offset from the center.
         * @param y The current y-offset from the center.
         * @param color The color of the points to be drawn.
         */
        void plotCirclePerimeterPoints(uint32_t cx, uint32_t cy, int x, int y, PalmyraOS::Color color);

        /**
         * Fill the circle using symmetric horizontal scanlines between the perimeter points.
         * This function draws horizontal lines (scanlines) between the symmetric points in the circle,
         * effectively filling the circle using 8-way symmetry.
         *
         * @param cx The x-coordinate of the circle's center.
         * @param cy The y-coordinate of the circle's center.
         * @param x The current x-offset from the center.
         * @param y The current y-offset from the center.
         * @param color The color of the scanlines to be drawn.
         */
        void fillCircleSymmetricScanlines(uint32_t cx, uint32_t cy, int x, int y, PalmyraOS::Color color);

        /**
         * Draw a point (pixel) in the frame buffer.
         * @param x The x-coordinate of the point.
         * @param y The y-coordinate of the point.
         * @param color The color of the point.
         */
        void drawPoint(uint32_t x, uint32_t y, Color color);

        /**
         * Draw a vertical line in the frame buffer.
         * @param x The x-coordinate of the line.
         * @param y1 The y-coordinate of the starting point.
         * @param y2 The y-coordinate of the ending point.
         * @param color The color of the line.
         */
        void drawVLine(uint32_t x, uint32_t y1, uint32_t y2, Color color);

        /**
         * Draw a horizontal line in the frame buffer.
         * @param x1 The x-coordinate of the starting point.
         * @param x2 The x-coordinate of the ending point.
         * @param y The y-coordinate of the line.
         * @param color The color of the line.
         */
        void drawHLine(uint32_t x1, uint32_t x2, uint32_t y, Color color);

        /**
         * Draw a line between two points using Bresenham's algorithm.
         * @param x1 The x-coordinate of the starting point.
         * @param y1 The y-coordinate of the starting point.
         * @param x2 The x-coordinate of the ending point.
         * @param y2 The y-coordinate of the ending point.
         * @param color The color of the line.
         */
        void drawLine(uint32_t x1, uint32_t y1, uint32_t x2, uint32_t y2, Color color);

        REMOVE_COPY(Brush);
        // TODO: Circles, Bresenham lines, Curves
    private:
        FrameBuffer& frameBuffer_;  // Reference to the frame buffer
    };

    /**
     * Class for rendering text on the frame buffer.
     */
    class TextRenderer {
    public:
        /**
         * Enumeration for different numeral systems.
         */
        enum class NumeralSystem { Hex, Dec, Bin };
        enum class Command { SwapBuffers };

    public:
        /**
         * Constructor to initialize the text renderer with a frame buffer and font.
         * @param frameBuffer Reference to the frame buffer.
         * @param font Reference to the font.
         */
        explicit TextRenderer(FrameBuffer& frameBuffer, Font& font);

        /**
         * Reset the text renderer's cursor position to the origin.
         */
        void reset();

        [[nodiscard]] inline int getCursorX() const { return cursor_x; }
        [[nodiscard]] inline int getCursorY() const { return cursor_y; }

        [[nodiscard]] inline uint32_t getPositionX() const { return position_x; }
        [[nodiscard]] inline uint32_t getPositionY() const { return position_y; }

        [[nodiscard]] inline uint32_t getWidth() const { return width; }
        [[nodiscard]] inline uint32_t getHeight() const { return height; }

        // Puts a character on the screen at the current x, y position
        void putChar(char ch);

        // change the position of the text rendering area
        void setPosition(uint32_t x, uint32_t y);

        // change the position of the cursor
        void setCursor(int x, int y);

        // change the width and height of the text rendering area
        void setSize(uint32_t w, uint32_t h);

        // Set the font for text rendering
        void setFont(Font& font);

        Color getCurrentColor();

        // Puts a string on the screen
        void putString(const char* str);

        uint32_t calculateWidth(const char* str);

        uint32_t calculateHeight();

        // Overload for << streaming operator for characters
        TextRenderer& operator<<(char ch);

        // Overload for << streaming operator for C-style strings
        TextRenderer& operator<<(const char* str);

        // Overload for << streaming operator for color commands
        TextRenderer& operator<<(Color color);

        // Overload for << streaming operator for color commands
        TextRenderer& operator<<(uint64_t num);

        // Overload for << streaming operator for color commands
        TextRenderer& operator<<(int64_t num);

        // Overload for << streaming operator for color commands
        TextRenderer& operator<<(uint32_t num);

        // Overload for << streaming operator for color commands
        TextRenderer& operator<<(int num);

        // Overload for << streaming operator for color commands
        TextRenderer& operator<<(uint16_t num);

        // Overload for << streaming operator for color commands
        TextRenderer& operator<<(double num);

        // Overload for << streaming operator for color commands
        TextRenderer& operator<<(NumeralSystem system);

        // Overload for << streaming operator for some commands
        TextRenderer& operator<<(Command command);

        void setPrecision(uint8_t precision);

        REMOVE_COPY(TextRenderer);

    private:
        char getFormat();

    private:
        FrameBuffer& frameBuffer_;                         // Reference to the frame buffer
        Font* font_;                                       // Pointer to the font (allows dynamic switching)
        Color textColor_{255, 255, 255};                   // Current text color (default is white)
        NumeralSystem representation{NumeralSystem::Dec};  // Current numeral system (default is decimal)
        uint8_t precision_{3};                             // Current precision of floats(default is 3)

        int cursor_x{0};  // Current x-coordinate of the cursor
        int cursor_y{0};  // Current y-coordinate of the cursor

        uint32_t lineSpacing_{3};  // Line spacing between text lines
        uint32_t tabSize_ = 8;     // Represents 8 characters per tab stop

        size_t position_x = 0;    // Current x-coordinate of the text area
        size_t position_y = 0;    // Current y-coordinate of the text area
        size_t width      = 640;  // Width of the text area
        size_t height     = 480;  // Height of the text area
                                  // TODO take care of this later (Scroll for example)
    };


}  // namespace PalmyraOS::kernel