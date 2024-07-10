
#include "core/tasks/ProcessManager.h"
#include "core/tasks/WindowManager.h"
#include <algorithm>


uint32_t PalmyraOS::kernel::Window::count = 0;

PalmyraOS::kernel::Window::Window(uint32_t* buffer, uint32_t x, uint32_t y, uint32_t width, uint32_t heigh)
	: x_(x), y_(y), z_(0), width_(width), height_(heigh), buffer_(buffer)
{
	id_ = ++count;

	char str_buffer[50];
	name_.reserve(50);
	snprintf(str_buffer, sizeof(str_buffer), "Window %d", id_);
	name_ = "str_buffer";

}

PalmyraOS::kernel::WindowVector PalmyraOS::kernel::WindowManager::windows;

PalmyraOS::kernel::Window* PalmyraOS::kernel::WindowManager::requestWindow(
	uint32_t* buffer_,
	uint32_t x,
	uint32_t y,
	uint32_t width,
	uint32_t height
)
{
	// Add the new window to the vector
	windows_.emplace_back(buffer_, x, y, width, height);

	// Return a pointer to the new window
	return &windows_.back();
}

void PalmyraOS::kernel::WindowManager::composite()
{
	FrameBuffer& screenBuffer = PalmyraOS::kernel::vbe_ptr->getFrameBuffer();
	size_t screenWidth  = screenBuffer.getWidth();
	size_t screenHeight = screenBuffer.getHeight();
	uint32_t* backBuffer = screenBuffer.getBackBuffer(); // RBGA (A not used)
	screenBuffer.fill(Color::DarkGray); // Background

	TaskManager::startAtomicOperation();
	{
		// TODO: Composite Windows
		std::sort(
			windows_.begin(), windows_.end(), [](const Window& a, const Window& b)
			{
			  return a.z_ < b.z_;
			}
		);


		// Composite each window
		for (const auto& window : windows_)
		{
			if (!window.visible_) continue;
			for (uint32_t y = 0; y < window.height_; ++y)
			{
				for (uint32_t x = 0; x < window.width_; ++x)
				{
					// Calculate screen coordinates
					uint32_t screenX = window.x_ + x;
					uint32_t screenY = window.y_ + y;

					// Bounds checking
					if (screenX < screenWidth && screenY < screenHeight)
					{
						uint32_t pixel = window.buffer_[y * window.width_ + x];
						backBuffer[screenY * screenWidth + screenX] = pixel;
					}
				}
			}
		}
		screenBuffer.swapBuffers();
	}
	TaskManager::endAtomicOperation();
}
void PalmyraOS::kernel::WindowManager::initialize()
{
	windows.reserve(20);
	windows_.reserve(20);
}

void PalmyraOS::kernel::WindowManager::closeWindow(uint32_t id)
{
	for (auto it = windows_.begin(); it != windows_.end(); ++it)
	{
		if (it->id_ == id)
		{
			it->visible_ = false;
			windows_.erase(it);
			break;  // Exit the loop once the window is found and erased
		}
	}
}
