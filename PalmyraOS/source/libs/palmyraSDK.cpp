
#include "libs/palmyraSDK.h"
#include "libs/string.h"
#include "palmyraOS/stdio.h"
#include "palmyraOS/stdlib.h"       // For dynamic memory management


PalmyraOS::SDK::Window::Window(
	uint32_t x,
	uint32_t y,
	uint32_t width,
	uint32_t height,
	bool isMovable,
	const char* title
)
{
	window_info_ = {
		.x = x,
		.y = y,
		.width = width,
		.height = height,
		.movable = isMovable,
		.title = ""
	};
	strncpy(window_info_.title, title, sizeof(window_info_.title));

	// initialize Window
	window_id_ = initializeWindow(&frontBuffer_, &window_info_);

	if (window_id_ == 0) perror("Failed to initialize window\n");
	else printf("Success to initialize window\n");
}

PalmyraOS::SDK::Window::~Window()
{
	if (window_id_) closeWindow(window_id_);
}

uint32_t PalmyraOS::SDK::Window::getWidth() const
{
	return window_info_.width;
}

uint32_t PalmyraOS::SDK::Window::getHeight() const
{
	return window_info_.height;
}
uint32_t* PalmyraOS::SDK::Window::getFrontBuffer() const
{
	return frontBuffer_;
}

const char* PalmyraOS::SDK::Window::getTitle() const
{
	return window_info_.title;
}
uint32_t PalmyraOS::SDK::Window::getID() const
{
	return window_id_;
}

/**********************************************************/



PalmyraOS::SDK::WindowFrame::WindowFrame(Window& window)
	:
	window_(window),
	backBuffer_(malloc(window.getWidth() * window.getHeight() * sizeof(uint32_t))),
	frameBuffer_(window.getWidth(), window.getHeight(), window.getFrontBuffer(), (uint32_t*)backBuffer_),
	brush_(frameBuffer_),
	textRenderer_(frameBuffer_, PalmyraOS::fonts::FontManager::getFont("Arial-12"))
{
	if (backBuffer_ == MAP_FAILED) perror("Failed to map memory\n");
	else printf("Success to map memory\n");

	// adjust text Renderer position
	textRenderer_.setPosition(5, 0);
}

void PalmyraOS::SDK::WindowFrame::render()
{
	// Render the terminal UI frame
	brush_.fill(PalmyraOS::Color::DarkerGray);
	brush_.fillRectangle(0, 0, window_.getWidth(), 20, PalmyraOS::Color::DarkRed);
	brush_.drawFrame(0, 0, window_.getWidth(), window_.getHeight(), PalmyraOS::Color::White);
	brush_.drawHLine(0, window_.getWidth(), 20, PalmyraOS::Color::White);
	textRenderer_ << PalmyraOS::Color::White;
	textRenderer_.setCursor(1, 1);
	textRenderer_ << window_.getTitle();
	textRenderer_.reset();
	textRenderer_.setCursor(1, 21);
}
void PalmyraOS::SDK::WindowFrame::swapBuffers()
{
	textRenderer_.reset();
	frameBuffer_.swapBuffers();
	render();
}

PalmyraOS::kernel::Brush& PalmyraOS::SDK::WindowFrame::brush()
{
	return brush_;
}

PalmyraOS::kernel::TextRenderer& PalmyraOS::SDK::WindowFrame::text()
{
	return textRenderer_;
}
