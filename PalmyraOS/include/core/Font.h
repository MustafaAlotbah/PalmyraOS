#pragma once

#include "core/definitions.h"


namespace PalmyraOS::fonts
{
  /**
   * Maximum number of glyphs in a font.
   */
  constexpr int MAX_FONT_SIZE = 128;

  /**
   * Structure representing a single glyph (character) in a font.
   */
  struct Glyph
  {
	  uint16_t bitmap[12];    // Pointer to bitmap data
	  uint32_t width{ 0 };
	  uint32_t height{ 0 };
	  uint16_t offsetX{ 0 };
	  uint16_t offsetY{ 0 };

	  /**
	   * Assignment operator for the Glyph structure.
	   * Performs deep copy of the bitmap data.
	   */
	  Glyph& operator=(const Glyph& other);
  };

  /**
   * Class representing a font, which consists of multiple glyphs.
   */
  class Font
  {
   public:
	  /**
	   * Constructor that initializes a font with a given name.
	   * @param name The name of the font.
	   */
	  explicit Font(const char* name);

	  /**
	   * Retrieves a glyph for a given character.
	   * @param character The character for which to retrieve the glyph.
	   * @return The glyph corresponding to the character.
	   */
	  const Glyph& getGlyph(uint32_t character);

	  /**
	   * Sets a glyph for a given character.
	   * @param character The character for which to set the glyph.
	   * @param glyph The glyph to set.
	   */
	  void setGlyph(uint32_t character, Glyph glyph);

   private:
	  const char* name_{};                // Name of the font
	  Glyph glyphs_[MAX_FONT_SIZE];       // Array of glyphs

	  friend class FontManager;           // Allow FontManager to access private members
  };

  /**
   * Class responsible for managing fonts.
   */
  class FontManager
  {
   public:
	  /**
	   * Initializes the font manager and loads default fonts.
	   */
	  static void initialize();

	  /**
	   * Retrieves a font by its name.
	   * @param name The name of the font to retrieve.
	   * @return The font corresponding to the name.
	   */
	  static Font& getFont(const char* name);

   private:
	  static Font arial;  // Static instance of the Arial font
  };


} // namespace PalmyraOS::fonts
// namespace mustyOS
