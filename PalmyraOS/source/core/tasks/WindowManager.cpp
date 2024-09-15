
#include "core/tasks/ProcessManager.h"
#include "core/tasks/WindowManager.h"
#include "core/SystemClock.h"
#include "core/peripherals/RTC.h"
#include "libs/memory.h"
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

void PalmyraOS::kernel::Window::setPosition(uint32_t x, uint32_t y)
{
	x_ = x;
	y_ = y;
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

void PalmyraOS::kernel::Window::queueMouseEvent(MouseEvent event)
{
	if (mouseEvents_.size() > 20) mouseEvents_.pop();
	mouseEvents_.push(event);
}

MouseEvent PalmyraOS::kernel::Window::popMouseEvent()
{
	if (mouseEvents_.empty()) return {};

	MouseEvent front = mouseEvents_.front();
	mouseEvents_.pop();
	return front;
}

void PalmyraOS::kernel::Window::setMovable(bool status)
{
	isMovable_ = status;
}

/***********************************************************************************************/

PalmyraOS::kernel::KVector<PalmyraOS::kernel::Window> PalmyraOS::kernel::WindowManager::windows_;
PalmyraOS::kernel::KQueue<KeyboardEvent>* PalmyraOS::kernel::WindowManager::keyboardsEvents_ = nullptr;
PalmyraOS::kernel::KQueue<MouseEvent>   * PalmyraOS::kernel::WindowManager::mouseEvents_ = nullptr;
uint32_t PalmyraOS::kernel::WindowManager::activeWindowId_ = 0;

int  PalmyraOS::kernel::WindowManager::mouseX_            = 0;
int  PalmyraOS::kernel::WindowManager::mouseY_            = 0;
bool PalmyraOS::kernel::WindowManager::isLeftButtonDown_  = false;
bool PalmyraOS::kernel::WindowManager::wasLeftButtonDown_ = false;

PalmyraOS::kernel::DragState PalmyraOS::kernel::WindowManager::dragState_;
uint32_t PalmyraOS::kernel::WindowManager::update_ns_     = 16'000L; // 100Hz cap
uint64_t PalmyraOS::kernel::WindowManager::fps_           = 0;
bool     PalmyraOS::kernel::WindowManager::sortingNeeded_ = false;

/***********************************************************************************************/

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

	setActiveWindow(windows_.back().getID());  // Set the newly created window as active

	// Return a pointer to the new window
	return &windows_.back();
}

void PalmyraOS::kernel::WindowManager::composite()
{
	FrameBuffer & screenBuffer = PalmyraOS::kernel::vbe_ptr->getFrameBuffer();
	TextRenderer& textRenderer = *kernel::textRenderer_ptr;


	TaskManager::startAtomicOperation();
	screenBuffer.fill(Color::DarkGray); // Background

	// Composite each window onto the back buffer

	for (const auto& window : windows_)
	{
		composeWindow(screenBuffer, window);
	}

	// draw mouse cursor
	renderMouseCursor();

	// TODO Window Manager Resources for Realtime Debugging
	textRenderer.setPosition(20, screenBuffer.getHeight() - 20);
	textRenderer << "[Window " << activeWindowId_ << "]"
												  << "[FPS: " << fps_ << "]"
												  << "[Mem: "
												  << (PhysicalMemory::getAllocatedFrames() >> 8)  // pages to MiB
												  << "/"
												  << (PhysicalMemory::size() >> 8)
												  << " MiB]";
	textRenderer.reset();

	// Atomically Swap the buffers
	screenBuffer.swapBuffers();

	// Forward Events
	forwardKeyboardEvents();
	forwardMouseEvents();
	TaskManager::endAtomicOperation();

}

void PalmyraOS::kernel::WindowManager::initialize()
{
	windows_.reserve(20);

	/* Delayed initialization, because KQueue uses KDeque,
	 * KDeque constructor uses the heap, and the heap is not set directly.
	 */
	keyboardsEvents_ = heapManager.createInstance<KQueue<KeyboardEvent>>();
	mouseEvents_ = heapManager.createInstance<KQueue<MouseEvent>>();


	FrameBuffer& screenBuffer = PalmyraOS::kernel::vbe_ptr->getFrameBuffer();

	mouseX_ = screenBuffer.getWidth() / 2;
	mouseY_ = screenBuffer.getHeight() / 2;
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

void PalmyraOS::kernel::WindowManager::queueMouseEvent(MouseEvent event)
{
	FrameBuffer& screenBuffer = PalmyraOS::kernel::vbe_ptr->getFrameBuffer();
	size_t screenWidth  = screenBuffer.getWidth();
	size_t screenHeight = screenBuffer.getHeight();

	// Update mouse position
	updateMousePosition(event, screenWidth, screenHeight);
	updateMouseButtonState(event);

	// Optionally pass event to active window
	if (mouseEvents_) mouseEvents_->push(event);
}

void PalmyraOS::kernel::WindowManager::queueKeyboardEvent(KeyboardEvent event)
{
	if (event.key == '\t' && event.isAltDown)
	{
		// Find the index of the current active window
		auto currentIt = std::find_if(
			windows_.begin(), windows_.end(),
			[](const Window& window)
			{ return window.getID() == activeWindowId_; }
		);

		if (currentIt != windows_.end())
		{
			// Loop to find the next visible window
			auto nextIt = currentIt;
			do
			{
				++nextIt;

				// Wrap around to the first window
				if (nextIt == windows_.end()) nextIt = windows_.begin();
			} while (nextIt != currentIt && !nextIt->visible_);

			// If a visible window is found, set it as active
			if (nextIt->visible_)
			{
				setActiveWindow(nextIt->getID());
			}
		}

		// Do not pass event to windows
		return;
	}

	// Pass event
	if (keyboardsEvents_) keyboardsEvents_->push(event);
}

KeyboardEvent PalmyraOS::kernel::WindowManager::popKeyboardEvent(uint32_t id)
{
	for (auto& window : windows_)
	{
		if (window.id_ == id)
		{
			return window.popKeyboardEvent();
		}
	}
	return {};
}

void PalmyraOS::kernel::WindowManager::setActiveWindow(uint32_t id)
{
	for (auto& window : windows_)
	{
		if (window.id_ == id)
		{
			// Set the highest z-order for the active window
			window.z_ = windows_.size();
			activeWindowId_ = id;
		}
		else
		{
			// Decrement the z-order of other windows
			if (window.z_ > 0) window.z_ -= 1;
		}
	}
	sortingNeeded_ = true;
}

void PalmyraOS::kernel::WindowManager::composeWindow(
	PalmyraOS::kernel::FrameBuffer& buffer,
	const PalmyraOS::kernel::Window& window
)
{

	if (!window.visible_) return;

	TaskManager::startAtomicOperation();

	// Sort windows by z index MUST be here, so that mouse click doesn't affect it
	if (sortingNeeded_)
	{
		std::sort(
			windows_.begin(), windows_.end(), [](const Window& a, const Window& b)
			{
			  return a.z_ < b.z_;
			}
		);
		sortingNeeded_ = false;
	}

	size_t screenWidth  = buffer.getWidth();
	size_t screenHeight = buffer.getHeight();
	uint32_t* backBuffer = buffer.getBackBuffer(); // RBGA (A not used)

	// Compute the window's position and size
	auto    windowLeft   = static_cast<int32_t>(window.x_);
	auto    windowTop    = static_cast<int32_t>(window.y_);
	int32_t windowRight  = windowLeft + static_cast<int32_t>(window.width_);
	int32_t windowBottom = windowTop + static_cast<int32_t>(window.height_);

	// Compute the clipping area (intersection with the screen)
	int32_t clipLeft   = std::max(windowLeft, 0);
	int32_t clipTop    = std::max(windowTop, 0);
	int32_t clipRight  = std::min(windowRight, static_cast<int32_t>(screenWidth));
	int32_t clipBottom = std::min(windowBottom, static_cast<int32_t>(screenHeight));

	// If there's no overlap, skip this window
	if (clipLeft >= clipRight || clipTop >= clipBottom) return;

	// Calculate the starting positions in the window buffer and back buffer
	auto srcStartX  = static_cast<uint32_t>(clipLeft - windowLeft);
	auto srcStartY  = static_cast<uint32_t>(clipTop - windowTop);
	auto destStartX = static_cast<uint32_t>(clipLeft);
	auto destStartY = static_cast<uint32_t>(clipTop);

	auto copyWidth  = static_cast<uint32_t>(clipRight - clipLeft);
	auto copyHeight = static_cast<uint32_t>(clipBottom - clipTop);

	// Copy each line from the window buffer to the back buffer
	for (uint32_t y = 0; y < copyHeight; ++y)
	{
		uint32_t* srcPtr  = window.buffer_ + (srcStartY + y) * window.width_ + srcStartX;
		uint32_t* destPtr = backBuffer + (destStartY + y) * screenWidth + destStartX;

		// Copy the entire line at once
		memcpy(destPtr, srcPtr, copyWidth);
	}
	TaskManager::endAtomicOperation();

}

uint32_t PalmyraOS::kernel::WindowManager::getWindowAtPosition(int x, int y)
{

	// Iterate from the back of the vector to get windows with higher z-order first
	for (auto it = windows_.rbegin(); it != windows_.rend(); ++it)
	{
		const Window& window = *it;

		if (!window.visible_)
			continue;

		// Check if the point (x, y) is within the window's bounds
		if (x >= static_cast<int>(window.x_) &&
			x < static_cast<int>(window.x_ + window.width_) &&
			y >= static_cast<int>(window.y_) &&
			y < static_cast<int>(window.y_ + window.height_))
		{
			return window.getID();
		}
	}

	// No window found at the given position
	return 0;
}

void PalmyraOS::kernel::WindowManager::updateMousePosition(const MouseEvent& event, int screenWidth, int screenHeight)
{
	// Update mouse position
	mouseX_ += event.deltaX;
	mouseY_ += event.deltaY;

	// Clamp mouse position within screen bounds
	mouseX_ = std::clamp(mouseX_, 0, screenWidth - 1);
	mouseY_ = std::clamp(mouseY_, 0, screenHeight - 1);
}

void PalmyraOS::kernel::WindowManager::updateMouseButtonState(const MouseEvent& event)
{
	// Update mouse button states
	wasLeftButtonDown_ = isLeftButtonDown_;
	isLeftButtonDown_  = event.isLeftDown;

	if (isLeftButtonDown_)
	{
		// Mouse button was just pressed
		if (!wasLeftButtonDown_) startDragging();

			// Mouse button is held down and dragging
		else if (dragState_.isDragging) updateDragging();
	}
	else
	{
		// Mouse button was just released
		if (wasLeftButtonDown_ && dragState_.isDragging) stopDragging();
	}
}

void PalmyraOS::kernel::WindowManager::startDragging()
{
	uint32_t windowId = getWindowAtPosition(mouseX_, mouseY_);
	if (windowId == 0) return;

	Window* window = getWindowById(windowId);
	if (!window) return;
//	if (!window->isMovable_) return;

	dragState_.windowId   = windowId;
	dragState_.isDragging = true;
	setActiveWindow(windowId);

	dragState_.offsetX = mouseX_ - static_cast<int>(window->x_);
	dragState_.offsetY = mouseY_ - static_cast<int>(window->y_);
}

void PalmyraOS::kernel::WindowManager::updateDragging()
{
	if (!dragState_.isDragging || dragState_.windowId == 0) return;

	Window* window = getWindowById(dragState_.windowId);
	if (!window) return;

	// Clamp window position within screen bounds
	FrameBuffer& screenBuffer = PalmyraOS::kernel::vbe_ptr->getFrameBuffer();

	// Update window position based on mouse movement and offset
	int newX = mouseX_ - dragState_.offsetX;
	int newY = mouseY_ - dragState_.offsetY;

	// keep some of the window visible (2 pixels)
	newX =
		std::clamp(newX, static_cast<int>(-screenBuffer.getWidth() + 2), static_cast<int>(screenBuffer.getWidth() - 2));
	newY = std::clamp(newY, 0, static_cast<int>(screenBuffer.getHeight() - 2));

	window->setPosition(static_cast<uint32_t>(newX), static_cast<uint32_t>(newY));

}

void PalmyraOS::kernel::WindowManager::stopDragging()
{
	dragState_.windowId   = 0;
	dragState_.isDragging = false;
}

PalmyraOS::kernel::Window* PalmyraOS::kernel::WindowManager::getWindowById(uint32_t id)
{
	for (auto& window : windows_)
	{
		if (window.getID() == id) return &window;
	}
	return nullptr;
}

void PalmyraOS::kernel::WindowManager::forwardMouseEvents()
{
	// Empty events if no window is available to receive them.
	if (windows_.empty())
	{
		while (!mouseEvents_->empty()) mouseEvents_->pop();
		return;
	}


	size_t   size = mouseEvents_->size();
	for (int i    = 0; i < size; ++i)
	{
		for (auto& window : windows_)
		{
			if (window.getID() == activeWindowId_)
			{
				window.queueMouseEvent(mouseEvents_->front());
				mouseEvents_->pop();
				break;
			}
		}
	}
}

void PalmyraOS::kernel::WindowManager::forwardKeyboardEvents()
{
	// Empty events if no window is available to receive them.
	if (windows_.empty())
	{
		while (!keyboardsEvents_->empty()) keyboardsEvents_->pop();
		return;
	}

	size_t   size = keyboardsEvents_->size();
	for (int i    = 0; i < size; ++i)
	{
		for (auto& window : windows_)
		{
			if (window.getID() == activeWindowId_)
			{
				window.queueKeyboardEvent(keyboardsEvents_->front());
				keyboardsEvents_->pop();
				break;
			}
		}
	}
}

void PalmyraOS::kernel::WindowManager::renderMouseCursor()
{
	constexpr uint32_t cursorWidth  = 8;
	constexpr uint32_t cursorHeight = 12;
	kernel::brush_ptr->drawLine(mouseX_, mouseY_, mouseX_ + cursorWidth, mouseY_ + cursorHeight, Color::White);
	kernel::brush_ptr->drawVLine(mouseX_, mouseY_, mouseY_ + cursorHeight, Color::White);
	kernel::brush_ptr->drawHLine(mouseX_, mouseX_ + cursorWidth, mouseY_ + cursorHeight, Color::White);
}

int PalmyraOS::kernel::WindowManager::thread(uint32_t argc, char** argv)
{

	uint64_t start_time   = SystemClock::getNanoseconds();
	uint64_t current_time = 0;

	// FPS calculation
	uint64_t frame_index    = 0;
	uint64_t start_time_rtc = kernel::RTC::now();

	while (true)
	{
		// wait update every 16.666 ms ~ 60 fps
		current_time = SystemClock::getNanoseconds();
		while (current_time - start_time < update_ns_)
		{
			current_time = SystemClock::getNanoseconds();
			sched_yield();
		}

		// Calculate FPS as frames per second (1 second = 1,000,000,000 nanoseconds)
		fps_ = static_cast<uint32_t>(frame_index++ / (kernel::RTC::now() - start_time_rtc));

		// Call the composite function to render the windows
		kernel::WindowManager::composite();

		// Reset the start_time to current_time for the next frame update
		start_time = current_time;

		sched_yield();
	}

//	return 0;
}



