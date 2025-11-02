/**
 * @file font.h
 * @brief Declaration of the Font, Glyph, and FontManager classes for handling font and glyph rendering.
 *
 * This header file provides the declarations for the `Font` class, which represents a collection of glyphs
 * for font rendering, and the `Glyph` structure, which encapsulates individual character representations.
 * The `FontManager` class is responsible for managing and initializing fonts. It allows retrieving fonts
 * by their name and provides an interface for setting and getting glyphs for specific characters.
 *
 * Key Features:
 * - The `Font` class manages a collection of `Glyph` objects and provides methods for glyph retrieval and management.
 * - The `Glyph` structure stores bitmap data and metadata related to each character's rendering properties.
 * - The `FontManager` class initializes default fonts and provides an interface to retrieve fonts by name.
 *
 * This file is a part of the low-level font and glyph rendering module and works closely with graphical
 * rendering functions to display text on screen.
 *
 * @date 01.07.2024
 * @contact mustafa.alotbah@gmail.com
 * @author Mustafa Alotbah
 */

#pragma once

#include "core/definitions.h"


namespace PalmyraOS {
    /**
     * @brief Maximum number of glyphs in a font.
     *
     * Defines the maximum number of glyphs (characters) that can be stored in a font.
     * The value of 128 corresponds to the basic ASCII character set.
     */
    constexpr int MAX_FONT_SIZE = 128;

    /**
     * @struct Glyph
     * @brief Represents a single glyph (character) in a font.
     *
     * The `Glyph` structure encapsulates the bitmap data and various metadata (width, height, offsets)
     * required to render a character on screen. Each glyph is essentially a representation of a single
     * character and includes its bitmap data for graphical rendering.
     */
    struct Glyph {
        uint16_t bitmap[12];  ///< Bitmap data representing the glyph.
        uint32_t width{0};    ///< Width of the glyph in pixels.
        uint32_t height{0};   ///< Height of the glyph in pixels.
        uint16_t offsetX{0};  ///< Horizontal offset for rendering the glyph.
        uint16_t offsetY{0};  ///< Vertical offset for rendering the glyph.

        /**
         * @brief Assignment operator for the Glyph structure.
         *
         * Performs a deep copy of the bitmap data and assigns the width, height, and offsets from another `Glyph`.
         *
         * @param other The glyph to copy from.
         * @return Reference to the current glyph instance.
         */
        Glyph& operator=(const Glyph& other);
    };


    /**
     * @class Font
     * @brief Represents a font, consisting of multiple glyphs.
     *
     * The `Font` class manages a collection of glyphs (characters) and provides functionality
     * to set and retrieve individual glyphs for a given character. A font is initialized with a
     * name and can store up to `MAX_FONT_SIZE` glyphs.
     */
    class Font {
    public:
        /**
         * @brief Constructor that initializes a font with a given name.
         *
         * Initializes the `Font` object and assigns a name to it. The name typically corresponds
         * to the font's style and size (e.g., "Arial-12").
         *
         * @param name The name of the font.
         */
        explicit Font(const char* name);

        /**
         * @brief Retrieves a glyph for a given character.
         *
         * This method returns the glyph associated with the provided character. If the character
         * is outside the supported range (greater than or equal to `MAX_FONT_SIZE`), a default glyph is returned.
         *
         * @param character The character for which to retrieve the glyph.
         * @return The glyph corresponding to the character.
         */
        const Glyph& getGlyph(uint32_t character);

        /**
         * @brief Sets a glyph for a given character.
         *
         * Associates a specified glyph with the given character in the font. This method allows
         * custom glyphs to be assigned to specific characters.
         *
         * @param character The character for which to set the glyph.
         * @param glyph The glyph to associate with the character.
         */
        void setGlyph(uint32_t character, Glyph glyph);

        /**
         * @brief Initializes the default fonts.
         *
         * This method initializes the default set of fonts, which can then be retrieved using
         * the `FontManager`. It ensures that the default `Arial12` font is loaded and ready for use.
         */
        static void initializeFonts();

    private:
        /**
         * @brief Initializes the Arial-12 font.
         *
         * This method sets up the individual glyphs for the Arial-12 font by mapping character codes
         * to their respective glyph data.
         */
        static void initializeArial12();

    public:
        static Font Arial12;  ///< Static instance of the Arial-12 font.

    private:
        const char* name_{};           ///< Name of the font (e.g., "Arial-12").
        Glyph glyphs_[MAX_FONT_SIZE];  ///< Array of glyphs representing the characters in the font.
        friend class FontManager;      ///< Grants `FontManager` access to the private members of `Font`.
    };

    /**
     * @class FontManager
     * @brief Class responsible for managing fonts.
     *
     * The `FontManager` class is responsible for initializing and managing fonts. It provides
     * an interface to retrieve fonts by their name and ensures that all fonts are properly initialized
     * before use. This class handles loading the default fonts.
     */
    class FontManager {
    public:
        /**
         * @brief Initializes the font manager and loads default fonts.
         *
         * This method loads the default fonts into memory, ensuring that they are available
         * for graphical rendering and text display operations.
         */
        static void initialize();


        /**
         * @brief Retrieves a font by its name.
         *
         * Returns a reference to the font corresponding to the specified name. If the font does not
         * exist, a default font is returned.
         *
         * @param name The name of the font to retrieve.
         * @return The font corresponding to the specified name.
         */
        static Font& getFont(const char* name);
    };


}  // namespace PalmyraOS
