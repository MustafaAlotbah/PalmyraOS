

#pragma once

#include "core/definitions.h"
#include "core/memory/HeapAllocator.h"
//#include "libs/string.h"


namespace PalmyraOS::kernel
{


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

	  inline uint32_t getID()
	  { return id_; }
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

	  friend class WindowManager;
  };

  /**
   * @typedef WindowVector
   * @brief A type definition for a vector of Window objects using the KernelHeapAllocator.
   */
  typedef std::vector<Window, KernelHeapAllocator<Window>> WindowVector;

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

   private:
	  static WindowVector windows; ///< Vector of all windows managed by the WindowManager.
  };


}
