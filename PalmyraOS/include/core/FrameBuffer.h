
#pragma once

#include "boot/multiboot.h"
#include "core/definitions.h"


namespace PalmyraOS
{

  class Color
  {
   public:
	  // Union to hold RGBA components or a single uint32_t value
	  union
	  {
		  struct
		  {
			  uint8_t blue;
			  uint8_t green;
			  uint8_t red;
			  uint8_t alpha;
		  };
		  uint32_t color{};
	  };

	  // Constructor to initialize color components individually
	  Color(uint8_t red, uint8_t green, uint8_t blue, uint8_t alpha = 255);

	  // Constructor to initialize color from a single uint32_t value
	  explicit Color(uint32_t color);

	  // Default constructor
	  Color(); // Default to transparent black

	  // Method to return the uint32_t color value
	  [[nodiscard]] uint32_t getColorValue() const;

	  // Predefined static colors
	  static const Color Red;
	  static const Color Green;
	  static const Color Blue;
	  static const Color Cyan;
	  static const Color Magenta;
	  static const Color Yellow;
	  static const Color Black;
	  static const Color White;
	  static const Color Gray;
	  static const Color DarkRed;
	  static const Color DarkGreen;
	  static const Color DarkBlue;
	  static const Color LightRed;
	  static const Color LightGreen;
	  static const Color LightBlue;
	  static const Color Orange;
  };

  namespace kernel
  {
	class FrameBuffer
	{
	 public:
		explicit FrameBuffer(uint16_t width, uint16_t height, uint32_t* frontBuffer, uint32_t* backBuffer);

		uint32_t* getBackBuffer();

		// Drawing
		void fill(Color color);
		void drawRect(uint32_t x1, uint32_t y1, uint32_t x2, uint32_t y2, Color color);
		void drawPixel(uint32_t x, uint32_t y, Color color);

		void swapBuffers();

		// Public interface to access screen properties might be added here
		[[nodiscard]] uint16_t getWidth() const;
		[[nodiscard]] uint16_t getHeight() const;

		REMOVE_COPY(FrameBuffer);

	 private:
		uint16_t width_  = 0;
		uint16_t height_ = 0;
		uint32_t* buffer_     = nullptr;
		uint32_t* backBuffer_ = (uint32_t*)0x00E6'0000;
	};

  }


}