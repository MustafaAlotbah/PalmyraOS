#include "core/BootConsole.h"
#include "core/kernel.h"
#include "libs/memory.h"
#include "libs/stdio.h"
#include "libs/stdlib.h"

namespace PalmyraOS::kernel {

    BootConsole::BootConsole(TextRenderer& renderer, uint32_t bufferSize, uint32_t maxLines)
        : renderer_(renderer), bufferSize_(bufferSize), bufferHead_(0), bufferCount_(0), maxLines_(maxLines) {
        // Allocate circular buffer
        buffer_ = (char*) kmalloc(bufferSize);
        if (buffer_) { memset(buffer_, 0, bufferSize_); }

        // Clear renderer
        renderer_.reset();
    }

    BootConsole::~BootConsole() {
        if (buffer_) { heapManager.free(buffer_); }
    }

    // ==================== Stream Operators ====================

    BootConsole& BootConsole::operator<<(char ch) {
        addChar(ch);
        return *this;
    }

    BootConsole& BootConsole::operator<<(const char* str) {
        if (!str) return *this;
        while (*str) { addChar(*str++); }
        return *this;
    }

    BootConsole& BootConsole::operator<<(uint64_t num) {
        char buf[32];
        uitoa64(num, buf, 10, false);
        return *this << buf;
    }

    BootConsole& BootConsole::operator<<(uint32_t num) {
        char buf[32];
        itoa(num, buf, 10, false);  // Use overloaded itoa for uint32_t
        return *this << buf;
    }

    BootConsole& BootConsole::operator<<(int num) {
        char buf[32];
        itoa(num, buf, 10, false);  // Use itoa for int
        return *this << buf;
    }

    BootConsole& BootConsole::operator<<(TextRenderer::Command cmd) {
        renderer_ << cmd;
        flush();
        return *this;
    }

    // ==================== Core Logic ====================

    void BootConsole::addChar(char ch) {
        if (!buffer_) return;

        // Add to circular buffer
        buffer_[bufferHead_] = ch;
        bufferHead_          = (bufferHead_ + 1) % bufferSize_;

        if (bufferCount_ < bufferSize_) { bufferCount_++; }

        // After adding character, check if renderer cursor has overflowed
        // We need to check the ACTUAL cursor position, not just newline count
        int cursorY                      = renderer_.getCursorY();
        uint32_t textHeight              = renderer_.getHeight();
        constexpr uint32_t LINE_HEIGHT   = 20;  // Approximate line height
        constexpr uint32_t BOTTOM_MARGIN = 10;  // Reserve 2 lines at bottom for safety

        // If cursor position exceeds available height (with margin), discard oldest line
        if (static_cast<uint32_t>(cursorY) + LINE_HEIGHT + BOTTOM_MARGIN > textHeight) {
            // Remove oldest line from buffer (up to and including first '\n')
            uint32_t oldestIdx    = (bufferHead_ + bufferSize_ - bufferCount_) % bufferSize_;
            uint32_t bytesSkipped = 0;

            for (uint32_t i = 0; i < bufferCount_; ++i) {
                uint32_t idx = (oldestIdx + i) % bufferSize_;
                bytesSkipped++;
                if (buffer_[idx] == '\n') {
                    break;  // Found newline, stop here
                }
            }

            // Discard these bytes
            if (bytesSkipped > 0 && bytesSkipped <= bufferCount_) { bufferCount_ -= bytesSkipped; }

            // Redraw after removing old line
            redrawVisibleLines();
        }
    }

    uint32_t BootConsole::countLines() const {
        if (!buffer_ || bufferCount_ == 0) return 0;

        uint32_t lineCount = 0;
        uint32_t startIdx  = (bufferHead_ + bufferSize_ - bufferCount_) % bufferSize_;

        for (uint32_t i = 0; i < bufferCount_; ++i) {
            uint32_t idx = (startIdx + i) % bufferSize_;
            if (buffer_[idx] == '\n') { lineCount++; }
        }

        // If buffer doesn't end with \n, count the incomplete line
        if (bufferCount_ > 0) {
            uint32_t lastIdx = (bufferHead_ + bufferSize_ - 1) % bufferSize_;
            if (buffer_[lastIdx] != '\n') { lineCount++; }
        }

        return lineCount;
    }

    void BootConsole::redrawVisibleLines() {
        if (!buffer_) return;

        // Clear only the text rendering area (preserve logo at top)
        if (display_ptr && brush_ptr) {
            uint32_t x = renderer_.getPositionX();
            uint32_t y = renderer_.getPositionY();
            uint32_t w = renderer_.getWidth();
            uint32_t h = renderer_.getHeight();
            brush_ptr->fillRectangle(x, y, x + w, y + h, Color::Black);
        }

        // Reset cursor to start of text area
        renderer_.setCursor(0, 0);

        // Get buffer start position
        uint32_t startIdx = (bufferHead_ + bufferSize_ - bufferCount_) % bufferSize_;

        // Render all characters in buffer
        for (uint32_t i = 0; i < bufferCount_; ++i) {
            uint32_t idx = (startIdx + i) % bufferSize_;
            char ch      = buffer_[idx];

            // Use current color from renderer
            renderer_.putChar(ch);
        }

        // Swap buffers to make changes visible
        renderer_ << TextRenderer::Command::SwapBuffers;
    }

    void BootConsole::flush() { redrawVisibleLines(); }

    uint32_t BootConsole::findLineStart(uint32_t lineIndex) const {
        // Helper for future enhancements (e.g., rendering specific lines)
        uint32_t currentLine = 0;
        uint32_t startIdx    = (bufferHead_ + bufferSize_ - bufferCount_) % bufferSize_;

        for (uint32_t i = 0; i < bufferCount_; ++i) {
            if (currentLine == lineIndex) { return (startIdx + i) % bufferSize_; }

            uint32_t idx = (startIdx + i) % bufferSize_;
            if (buffer_[idx] == '\n') { currentLine++; }
        }

        return startIdx;
    }

}  // namespace PalmyraOS::kernel
