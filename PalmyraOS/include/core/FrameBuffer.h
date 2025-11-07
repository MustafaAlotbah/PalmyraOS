
#pragma once

#include "core/definitions.h"


namespace PalmyraOS {

    /**
     * Class representing a color in ARGB format.
     */
    class Color {
    public:
        // Union to hold RGBA components or a single uint32_t value
        union {
            struct {
                uint8_t blue;
                uint8_t green;
                uint8_t red;
                uint8_t alpha;
            };
            uint32_t color{};  // Combined color value
        };

        // Constructor to initialize color components individually
        Color(uint8_t red, uint8_t green, uint8_t blue, uint8_t alpha = 255);

        // Constructor to initialize color from a single uint32_t value
        explicit Color(uint32_t color);

        /**
         * Default constructor initializes color to black.
         */
        Color();

        /**
         * Get the combined ARGB color value.
         * @return The combined ARGB color value.
         */
        [[nodiscard]] uint32_t getColorValue() const;

        // Predefined static colors

        /**
         * @reference material theme
         */
        /* Gray Scale*/
        static const Color White;
        static const Color Gray100;
        static const Color Gray200;
        static const Color Gray300;
        static const Color Gray400;
        static const Color Gray500;
        static const Color Gray600;
        static const Color Gray700;
        static const Color Gray800;
        static const Color DarkGray;
        static const Color DarkerGray;
        static const Color DarkestGray;
        static const Color Black;

        static const Color Red;
        static const Color Red100;
        static const Color Red200;
        static const Color Red300;
        static const Color Red400;
        static const Color Red500;
        static const Color Red600;
        static const Color Red700;
        static const Color Red800;
        static const Color Red900;
        static const Color DarkRed;
        static const Color DarkerRed;


        static const Color Green;
        static const Color Blue;
        static const Color Cyan;
        static const Color Magenta;
        static const Color Yellow;

        static const Color DarkGreen;
        static const Color DarkBlue;
        static const Color LightRed;
        static const Color LightGreen;
        static const Color LightBlue;
        static const Color LighterBlue;
        static const Color Orange;

        static const Color PrimaryDark;   // 900
        static const Color Primary;       // 800
        static const Color PrimaryLight;  // 400
        static const Color Secondary;
    };

    namespace kernel {
        /**
         * Class representing a frame buffer for drawing graphics.
         * This class does not assume working with memory and hence requires a set up `backBuffer`
         */
        class FrameBuffer {
        public:
            /**
             * Constructor to initialize the frame buffer with dimensions and buffers.
             * @param width Width of the frame buffer.
             * @param height Height of the frame buffer.
             * @param frontBuffer Pointer to the front buffer.
             * @param backBuffer Pointer to the back buffer.
             */
            explicit FrameBuffer(uint16_t width, uint16_t height, uint32_t* frontBuffer, uint32_t* backBuffer);

            /**
             * Get the back buffer for drawing.
             * @return Pointer to the back buffer.
             */
            uint32_t* getBackBuffer();

            // Drawing
            /**
             * Fill the entire frame buffer with a single color.
             * @param color Color to fill the frame buffer with.
             */
            void fill(Color color);

            /**
             * Draw a filled rectangle in the frame buffer.
             * @param x1 Top-left x-coordinate.
             * @param y1 Top-left y-coordinate.
             * @param x2 Bottom-right x-coordinate.
             * @param y2 Bottom-right y-coordinate.
             * @param color Color to fill the rectangle with.
             */
            void drawRect(uint32_t x1, uint32_t y1, uint32_t x2, uint32_t y2, Color color);

            /**
             * Draw a single pixel in the frame buffer.
             * @param x x-coordinate of the pixel.
             * @param y y-coordinate of the pixel.
             * @param color Color of the pixel.
             */
            void drawPixel(uint32_t x, uint32_t y, Color color);

            // copy backBuffer to frontBuffer
            void swapBuffers();

            // Public interface to access screen properties might be added here
            [[nodiscard]] uint16_t getWidth() const;
            [[nodiscard]] uint16_t getHeight() const;
            [[nodiscard]] uint32_t getSize() const;

            REMOVE_COPY(FrameBuffer);

        private:
            uint16_t width_       = 0;                        // Width of the frame buffer
            uint16_t height_      = 0;                        // Height of the frame buffer
            uint32_t* buffer_     = nullptr;                  // Pointer to the front buffer
            uint32_t* backBuffer_ = (uint32_t*) 0x00E6'0000;  // Pointer to the back buffer
        };

    }  // namespace kernel


}  // namespace PalmyraOS