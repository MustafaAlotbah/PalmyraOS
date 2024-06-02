#pragma once

#include "boot/multiboot.h"
#include "core/definitions.h"
#include "core/FrameBuffer.h"
#include "core/Font.h"


#define HEX() kernel::TextRenderer::NumeralSystem::Hex
#define DEC() kernel::TextRenderer::NumeralSystem::Dec
#define BIN() kernel::TextRenderer::NumeralSystem::Bin

namespace PalmyraOS::kernel
{


  class Brush
  {
   public:
	  explicit Brush(FrameBuffer& frameBuffer);

	  void fill(Color color);
	  void fillRectangle(uint32_t x1, uint32_t y1, uint32_t x2, uint32_t y2, Color color);
	  void drawPoint(uint32_t x, uint32_t y, Color color);
	  void drawVLine(uint32_t x, uint32_t y1, uint32_t y2, Color color);
	  void drawHLine(uint32_t x1, uint32_t x2, uint32_t y, Color color);

	  // TODO: Circles, Bresenham lines, Curves
   private:
	  FrameBuffer& frameBuffer_;
  };

  class TextRenderer
  {
   public:
	  enum class NumeralSystem
	  {
		  Hex, Dec, Bin
	  };

   public:
	  explicit TextRenderer(FrameBuffer& frameBuffer, fonts::Font& font);

	  void reset();

	  [[nodiscard]] inline uint32_t getCursorX() const
	  { return cursor_x; }
	  [[nodiscard]] inline uint32_t getCursorY() const
	  { return cursor_y; }

	  // Puts a character on the screen at the current x, y position
	  void putChar(char ch);

	  // change the position of the text rendering area
	  void setPosition(uint32_t x, uint32_t y);

	  // change the width and height of the text rendering area
	  void setSize(uint32_t w, uint32_t h);

	  // Puts a string on the screen
	  void putString(const char* str);

	  // Overload for << streaming operator for characters
	  TextRenderer& operator<<(char ch);

	  // Overload for << streaming operator for C-style strings
	  TextRenderer& operator<<(const char* str);

	  // Overload for << streaming operator for color commands
	  TextRenderer& operator<<(Color color);

	  // Overload for << streaming operator for color commands
	  TextRenderer& operator<<(uint64_t num);

	  // Overload for << streaming operator for color commands
	  TextRenderer& operator<<(uint32_t num);

	  // Overload for << streaming operator for color commands
	  TextRenderer& operator<<(int num);

	  // Overload for << streaming operator for color commands
	  TextRenderer& operator<<(uint16_t num);

	  // Overload for << streaming operator for color commands
	  TextRenderer& operator<<(NumeralSystem system);

   private:
	  char getFormat();

   private:
	  FrameBuffer& frameBuffer_;
	  fonts::Font& font_;
	  Color textColor_{ 255, 255, 255 };

	  NumeralSystem representation{ NumeralSystem::Dec };

	  uint32_t cursor_x{ 0 };
	  uint32_t cursor_y{ 0 };

	  uint32_t lineSpacing_{ 3 };
	  uint32_t tabSize_ = 8;  // Represents 8 characters per tab stop

	  size_t position_x = 0;
	  size_t position_y = 0;
	  size_t width      = 640;
	  size_t height     = 480;// TODO take care of this later (Scroll for example)

  };

  class VBE
  {
   public:


	  VBE(vbe_mode_info_t* mode_, vbe_control_info_t* control_, uint32_t* backBuffer);
	  void swapBuffers();

	  [[nodiscard]] size_t getWidth() const;
	  [[nodiscard]] size_t getHeight() const;
	  [[nodiscard]] size_t getVideoMemorySize() const;
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
	  FrameBuffer frameBuffer_;
	  vbe_mode_info_t   & vbe_mode_info_;
	  vbe_control_info_t& vbe_control_info_;
  };


}