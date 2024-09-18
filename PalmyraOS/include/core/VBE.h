#pragma once

#include "boot/multiboot.h"
#include "core/definitions.h"
#include "core/FrameBuffer.h"
#include "core/Font.h"


/**
 * Macros to simplify usage of different numeral systems in text rendering.
 */
#define HEX() kernel::TextRenderer::NumeralSystem::Hex
#define DEC() kernel::TextRenderer::NumeralSystem::Dec
#define BIN() kernel::TextRenderer::NumeralSystem::Bin
#define SWAP_BUFF() kernel::TextRenderer::Command::SwapBuffers


namespace PalmyraOS::kernel
{

  /**
   * Class representing the VBE (VESA BIOS Extensions) interface for graphics.
   */
  class VBE
  {
   public:

	  /**
	   * Constructor to initialize VBE with mode and control information, and a back buffer.
	   * @param mode Pointer to the VBE mode information.
	   * @param control Pointer to the VBE control information.
	   * @param backBuffer Pointer to the back buffer.
	   */
	  VBE(vbe_mode_info_t* mode_, vbe_control_info_t* control_, uint32_t* backBuffer);

	  /**
	   * Swap the front and back buffers.
	   */
	  void swapBuffers();

	  [[nodiscard]] size_t getWidth() const;
	  [[nodiscard]] size_t getHeight() const;

	  /**
	   * Get the size of the video memory provided by hardware.
	   * @return The video memory size.
	   */
	  [[nodiscard]] size_t getVideoMemorySize() const;

	  /**
	   * Get the color depth (bits per pixel).
	   * @return The color depth.
	   */
	  [[nodiscard]] size_t getColorDepth() const;
	  [[nodiscard]] uint16_t getWindowAttributes() const;
	  [[nodiscard]] uint8_t getMemoryModel() const;
	  [[nodiscard]] const char* listVideoModes() const;

	  // Individual attribute checks
	  [[nodiscard]] bool isModeSupported() const;
	  [[nodiscard]] bool isOptionalHardwareSupported() const;
	  [[nodiscard]] bool isBiosOutputSupported() const;
	  [[nodiscard]] bool isColorMode() const;
	  [[nodiscard]] bool isGraphicsMode() const;
	  [[nodiscard]] bool isVGACompatibleWindowedMemoryPagingSupported() const;

	  FrameBuffer& getFrameBuffer();

	  REMOVE_COPY(VBE);
   private:
	  FrameBuffer frameBuffer_;                 // Frame buffer object
	  vbe_mode_info_t   & vbe_mode_info_;       // VBE mode information reference
	  vbe_control_info_t& vbe_control_info_;    // VBE control information reference
  };

  /**
   * Class representing a brush for drawing operations on the frame buffer.
   */
  class Brush
  {
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
	  FrameBuffer& frameBuffer_; // Reference to the frame buffer
  };

  /**
   * Class for rendering text on the frame buffer.
   */
  class TextRenderer
  {
   public:
	  /**
	   * Enumeration for different numeral systems.
	   */
	  enum class NumeralSystem
	  {
		  Hex, Dec, Bin
	  };
	  enum class Command
	  {
		  SwapBuffers
	  };

   public:
	  /**
	   * Constructor to initialize the text renderer with a frame buffer and font.
	   * @param frameBuffer Reference to the frame buffer.
	   * @param font Reference to the font.
	   */
	  explicit TextRenderer(FrameBuffer& frameBuffer, fonts::Font& font);

	  /**
	   * Reset the text renderer's cursor position to the origin.
	   */
	  void reset();

	  [[nodiscard]] inline int getCursorX() const
	  { return cursor_x; }
	  [[nodiscard]] inline int getCursorY() const
	  { return cursor_y; }

	  [[nodiscard]] inline uint32_t getPositionX() const
	  { return position_x; }
	  [[nodiscard]] inline uint32_t getPositionY() const
	  { return position_y; }

	  [[nodiscard]] inline uint32_t getWidth() const
	  { return width; }
	  [[nodiscard]] inline uint32_t getHeight() const
	  { return height; }

	  // Puts a character on the screen at the current x, y position
	  void putChar(char ch);

	  // change the position of the text rendering area
	  void setPosition(uint32_t x, uint32_t y);

	  // change the position of the cursor
	  void setCursor(int x, int y);

	  // change the width and height of the text rendering area
	  void setSize(uint32_t w, uint32_t h);

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
	  FrameBuffer& frameBuffer_;                            // Reference to the frame buffer
	  fonts::Font& font_;                                   // Reference to the font
	  Color         textColor_{ 255, 255, 255 };            // Current text color (default is white)
	  NumeralSystem representation{ NumeralSystem::Dec };   // Current numeral system (default is decimal)
	  uint8_t precision_{ 3 };                                // Current precision of floats(default is 3)

	  int cursor_x{ 0 };                                // Current x-coordinate of the cursor
	  int cursor_y{ 0 };                                // Current y-coordinate of the cursor

	  uint32_t lineSpacing_{ 3 };                            // Line spacing between text lines
	  uint32_t tabSize_ = 8;                                 // Represents 8 characters per tab stop

	  size_t position_x = 0;                                 // Current x-coordinate of the text area
	  size_t position_y = 0;                                 // Current y-coordinate of the text area
	  size_t width      = 640;                               // Width of the text area
	  size_t height     = 480;                               // Height of the text area
	  // TODO take care of this later (Scroll for example)

  };


}