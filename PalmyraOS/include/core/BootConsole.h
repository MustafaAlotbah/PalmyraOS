#pragma once

#include "core/Display.h"

namespace PalmyraOS::kernel {

    /**
     * @brief Boot-time scrolling console with circular buffer
     *
     * A lightweight wrapper around TextRenderer that maintains a circular buffer
     * of text lines. When the number of lines exceeds the screen capacity, older
     * lines are automatically discarded and the display scrolls.
     *
     * Usage during kernel initialization:
     *   BootConsole console(*textRenderer_ptr, 1024, 30); // 1KB buffer, 30 lines max
     *   console << "Initializing...\n";
     *   console.flush();  // Updates display
     */
    class BootConsole {
    public:
        /**
         * @param renderer The TextRenderer to render to
         * @param bufferSize Size of circular buffer in bytes (e.g., 2048)
         * @param maxLines Maximum visible lines (e.g., 30)
         */
        BootConsole(TextRenderer& renderer, uint32_t bufferSize, uint32_t maxLines);
        ~BootConsole();

        // Stream operators
        BootConsole& operator<<(char ch);
        BootConsole& operator<<(const char* str);
        BootConsole& operator<<(uint64_t num);
        BootConsole& operator<<(uint32_t num);
        BootConsole& operator<<(int num);
        BootConsole& operator<<(TextRenderer::Command cmd);

        // Force redraw of visible lines
        void flush();

    private:
        void addChar(char ch);
        void addColorChange(Color color);
        void redrawVisibleLines();
        uint32_t countLines() const;
        uint32_t findLineStart(uint32_t lineIndex) const;

        TextRenderer& renderer_;

        // Circular buffer for text
        char* buffer_;          // Circular buffer
        uint32_t bufferSize_;   // Total buffer size
        uint32_t bufferHead_;   // Write position
        uint32_t bufferCount_;  // Bytes currently in buffer
        uint32_t maxLines_;     // Maximum visible lines
    };

}  // namespace PalmyraOS::kernel
