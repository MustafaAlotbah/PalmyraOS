
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

void PalmyraOS::kernel::Window::queueKeyboardEvent(KeyboardEvent event)
{
	if (keyboardsEvents_.size() > 20) keyboardsEvents_.pop();
	keyboardsEvents_.push(event);
}

KeyboardEvent PalmyraOS::kernel::Window::popKeyboardEvent()
{
	if (keyboardsEvents_.empty()) return {};

	KeyboardEvent front = keyboardsEvents_.front();
	keyboardsEvents_.pop();
	return front;
}

PalmyraOS::kernel::KVector<PalmyraOS::kernel::Window> PalmyraOS::kernel::WindowManager::windows_;
PalmyraOS::kernel::KQueue<KeyboardEvent>* PalmyraOS::kernel::WindowManager::keyboardsEvents_ = nullptr;

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

	// Manage the buffer size
	if (windows_.size() >= 2)
	{
		size_t   size = keyboardsEvents_->size();
		for (int i    = 0; i < size; ++i)
		{
			for (auto& window : windows_)
			{
				window.queueKeyboardEvent(keyboardsEvents_->front());
			}
			keyboardsEvents_->pop();
		}
	}
}

void PalmyraOS::kernel::WindowManager::initialize()
{
	windows_.reserve(20);

	/* Delayed initialization, because KQueue uses KDeque,
	 * KDeque constructor uses the heap, and the heap is not set directly.
	 */
	keyboardsEvents_ = heapManager.createInstance<KQueue<KeyboardEvent>>();
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

void PalmyraOS::kernel::WindowManager::queueKeyboardEvent(KeyboardEvent event)
{
	if (keyboardsEvents_)
	{
		keyboardsEvents_->push(event);
	}
}

KeyboardEvent PalmyraOS::kernel::WindowManager::popKeyboardEvent(uint32_t id)
{
	// TODO: Capture Alt+Tab, switch active window
	for (auto& window : windows_)
	{
		if (window.id_ == id)
		{
			return window.popKeyboardEvent();
		}
	}
	return {};
}
