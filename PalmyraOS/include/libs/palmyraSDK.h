/**
 * @brief This module is responsible for wrapping PalmyraOS specific functionalities
 * such as creating and disposing of windows.
 */

#pragma once

#include "palmyraOS/unistd.h"

// TODO: move these to libs (or put them in palmyraOS)
#include "core/FrameBuffer.h"        // FrameBuffer
#include "core/VBE.h"                // for Brush, TextRenderer
#include "core/Font.h"               // For Fonts


namespace PalmyraOS::SDK
{

  class Window
  {
   public:
	  Window(uint32_t x, uint32_t y, uint32_t width, uint32_t height, bool isMovable, const char* title);
	  ~Window();
	  [[nodiscard]] uint32_t getWidth() const;
	  [[nodiscard]] uint32_t getHeight() const;
	  [[nodiscard]] uint32_t getID() const;
	  [[nodiscard]] uint32_t* getFrontBuffer() const;
	  [[nodiscard]] const char* getTitle() const;

   private:
	  palmyra_window window_info_{};
	  uint32_t       window_id_{ 0 };
	  uint32_t* frontBuffer_{ nullptr };
  };

  // TODO set background color
  class WindowFrame
  {
   public:
	  explicit WindowFrame(Window& window);

	  void render();
	  void swapBuffers();

	  PalmyraOS::kernel::Brush& brush();
	  PalmyraOS::kernel::TextRenderer& text();

   private:
	  Window& window_;
	  void  * backBuffer_;
	  PalmyraOS::kernel::FrameBuffer  frameBuffer_;
	  PalmyraOS::kernel::Brush        brush_;
	  PalmyraOS::kernel::TextRenderer textRenderer_;
  };

}
