/**
 * @brief This module is responsible for wrapping PalmyraOS specific functionalities
 * such as creating and disposing of windows.
 */

#pragma once

#include <utility>
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

  struct ClippedBounds
  {
	  uint32_t xMin;
	  uint32_t yMin;
	  uint32_t xMax;
	  uint32_t yMax;
	  bool     isClipped;  // Indicates whether the element is fully outside the clipping area
  };

  // TODO set background color
  class WindowGUI
  {
   public:
	  explicit WindowGUI(Window& window);

	  void render();
	  void pollEvents();
	  void swapBuffers();

	  PalmyraOS::kernel::Brush& brush();
	  PalmyraOS::kernel::TextRenderer& text();
	  std::pair<uint32_t, uint32_t> getFrameBufferSize();
	  std::pair<int, int> getMousePosition();
	  void setBackground(Color color);

	  // TODO issue if clicked on one button, dragged to another,
	  //   released there, the other button is activated (-> wasLeftDown unique by button)
	  bool button(
		  const char* text,
		  uint32_t x,
		  uint32_t y,
		  uint32_t width = 0, // 0 is automatic by text width
		  uint32_t height = 0,// 0 is automatic by text height
		  uint32_t margin = 2,
		  bool whileDown = false,
		  Color textColor = Color::Black,
		  Color backColor = Color::Primary,
		  Color colorHover = Color::PrimaryLight,
		  Color colorDown = Color::PrimaryDark
	  );

	  void fillRectangle(
		  uint32_t x,
		  uint32_t y,
		  uint32_t width,
		  uint32_t height,
		  Color backColor = Color::Primary
	  );

	  bool link(
		  const char* text,
		  bool whileDown = false,
		  Color color = Color::Primary,
		  Color colorHover = Color::PrimaryLight,
		  Color colorDown = Color::PrimaryDark
	  );

	  ClippedBounds clipToTextRenderer(uint32_t xMin, uint32_t yMin, uint32_t xMax, uint32_t yMax);

   private:
	  Window& window_;
	  void  * backBuffer_;
	  PalmyraOS::kernel::FrameBuffer  frameBuffer_;
	  PalmyraOS::kernel::Brush        brush_;
	  PalmyraOS::kernel::TextRenderer textRenderer_;
	  PalmyraOS::Color                backgroundColor_;
	  MouseEvent                      currentMouseEvent_;
	  bool                            wasLeftDown_ = false;
	  palmyra_window_status           currentWindowStatus_;
  };

  class Layout
  {
   public:
	  explicit Layout(WindowGUI& windowGui, int* scrollY, bool scrollable = false, size_t height = 0);
	  ~Layout();

	  [[maybe_unused]] inline uint32_t getX() const
	  { return prevPositionX_ + currCursorX_ + 1; }
	  [[nodiscard]] inline uint32_t getY() const
	  { return prevPositionY_ + currCursorY_ + 1; }
	  [[nodiscard]] inline uint32_t getWidth() const
	  { return currWidth_ - 2; }
	  [[nodiscard]] inline uint32_t getHeight() const
	  { return currHeight_ - 2; }

   private:
	  WindowGUI& windowGui_;
	  bool     scrollable_;
	  uint32_t scrollBarWidth_;

	  // previous state
	  uint32_t prevPositionX_;
	  uint32_t prevPositionY_;
	  uint32_t prevWidth_;
	  uint32_t prevHeight_;
	  uint32_t prevCursorY_;

	  // current state
	  uint32_t currCursorX_;
	  uint32_t currCursorY_;
	  uint32_t currWidth_;
	  uint32_t currHeight_;

	  // Scroll-related variables
	  int* scrollY_;
	  int currScrollY_;
  };


}
