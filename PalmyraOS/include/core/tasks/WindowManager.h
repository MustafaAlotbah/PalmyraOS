

#pragma once

#include "core/definitions.h"
#include "core/memory/KernelHeapAllocator.h"

#include "palmyraOS/input.h"


namespace PalmyraOS::kernel
{

  struct DragState
  {
	  uint32_t windowId   = 0;    // ID of the window being dragged
	  int      offsetX    = 0;          // X-offset between mouse and window's top-left corner
	  int      offsetY    = 0;          // Y-offset between mouse and window's top-left corner
	  bool     isDragging = false;  // Indicates if dragging is in progress
  };


  /**
   * @class Window
   * @brief Represents a window in the PalmyraOS kernel.
   */
  class Window
  {
   public:
	  /**
	   * @brief Constructs a Window object.
	   * @param buffer Pointer to the buffer for the window's content.
	   * @param x The x-coordinate of the window.
	   * @param y The y-coordinate of the window.
	   * @param width The width of the window.
	   * @param height The height of the window.
	   */
	  explicit Window(uint32_t* buffer, uint32_t x, uint32_t y, uint32_t width, uint32_t height);

	  [[nodiscard]] inline uint32_t getID() const
	  { return id_; }

	  void queueKeyboardEvent(KeyboardEvent event);
	  void queueMouseEvent(MouseEvent event);

	  KeyboardEvent popKeyboardEvent();

	  MouseEvent popMouseEvent();

	  void setPosition(uint32_t x, uint32_t y);
   private:
	  static uint32_t count; ///< Static counter for window IDs.

   private:
	  uint32_t id_;             ///< The unique ID of the window.
	  uint32_t x_;              ///< The x-coordinate of the window.
	  uint32_t y_;              ///< The y-coordinate of the window.
	  uint32_t z_;              ///< The z-order of the window.
	  uint32_t width_;          ///< The width of the window.
	  uint32_t height_;         ///< The height of the window.
	  KString name_;          ///< The name of the window.
	  uint32_t* buffer_;        ///< Pointer to the buffer for the window's content.
	  bool visible_{ true };    ///< Visibility status of the window.

	  KQueue<KeyboardEvent> keyboardsEvents_;
	  KQueue<MouseEvent>    mouseEvents_;
	  friend class WindowManager;
  };

  /**
   * @class WindowManager
   * @brief Manages the creation, destruction, and compositing of windows in the PalmyraOS kernel.
   */
  class WindowManager
  {
   public:
	  /**
	   * @brief Initializes the window manager.
	   */
	  static void initialize();

	  /**
	   * @brief Requests the creation of a new window.
	   * @param buffer Pointer to the buffer for the window's content.
	   * @param x The x-coordinate of the window.
	   * @param y The y-coordinate of the window.
	   * @param width The width of the window.
	   * @param height The height of the window.
	   * @return Pointer to the created Window object.
	   */
	  static Window* requestWindow(uint32_t* buffer, uint32_t x, uint32_t y, uint32_t width, uint32_t height);

	  /**
	   * @brief Closes the window with the specified ID.
	   * @param id The ID of the window to be closed.
	   */
	  static void closeWindow(uint32_t id);

	  /**
	   * @brief Composites all the windows.
	   */
	  static void composite();

	  static void queueKeyboardEvent(KeyboardEvent event);

	  static void queueMouseEvent(MouseEvent event);

	  static KeyboardEvent popKeyboardEvent(uint32_t id);

	  static void setActiveWindow(uint32_t id);

	  static void composeWindow(FrameBuffer& buffer, const Window& window);

	  static void renderMouseCursor();

	  /**
	   * @brief Returns the ID of the topmost window at the given coordinates.
	   * @param x The x-coordinate.
	   * @param y The y-coordinate.
	   * @return The ID of the window under the coordinates, or 0 if none.
	   */
	  static uint32_t getWindowAtPosition(int x, int y);

	  static void forwardMouseEvents();
	  static void forwardKeyboardEvents();
	  static void updateMousePosition(const MouseEvent& event, int screenWidth, int screenHeight);
	  static void updateMouseButtonState(const MouseEvent& event);
	  static void startDragging();
	  static void updateDragging();
	  static void stopDragging();

	  // Utility methods
	  static Window* getWindowById(uint32_t id);

   private:
	  static KVector<Window> windows_; ///< Vector of all windows managed by the WindowManager. // TODO  KMap
	  static uint32_t activeWindowId_;

	  static KQueue<KeyboardEvent>* keyboardsEvents_;
	  static KQueue<MouseEvent>   * mouseEvents_;

	  // Mouse state tracking
	  static int  mouseX_;
	  static int  mouseY_;
	  static bool isLeftButtonDown_;
	  static bool wasLeftButtonDown_;

	  // Dragging state
	  static DragState dragState_;
  };


}
