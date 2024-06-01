#pragma once

#include "core/definitions.h"


namespace PalmyraOS::fonts
{
  // Rasterized fonts
  constexpr int MAX_FONT_SIZE = 128;

  struct Glyph
  {
	  uint16_t bitmap[12];    // Pointer to bitmap data
	  uint32_t width{ 0 };
	  uint32_t height{ 0 };
	  uint16_t offsetX{ 0 };
	  uint16_t offsetY{ 0 };

	  // Copy Assignment Operator
	  Glyph& operator=(const Glyph& other);
  };

  class Font
  {
   public:
	  Font(const char* name);
	  // ~Font();

	  const Glyph& getGlyph(uint32_t character);
	  // inline uint32_t getSize() { return size_; }
   private:
	  void setGlyph(uint32_t character, Glyph glyph);

	  const char* name_{};
	  Glyph glyphs_[MAX_FONT_SIZE];

	  friend class FontManager;
  };

  class FontManager
  {
   public:
	  static void initialize();
	  static Font& getFont(const char* name);

   private:
	  static Font arial;
  };


} // namespace PalmyraOS::fonts
// namespace mustyOS
