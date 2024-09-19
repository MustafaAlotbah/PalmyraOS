
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



PalmyraOS::SDK::WindowGUI::WindowGUI(Window& window)
	:
	window_(window),
	backBuffer_(malloc(window.getWidth() * window.getHeight() * sizeof(uint32_t))),
	frameBuffer_(window.getWidth(), window.getHeight(), window.getFrontBuffer(), (uint32_t*)backBuffer_),
	brush_(frameBuffer_),
	textRenderer_(frameBuffer_, PalmyraOS::fonts::FontManager::getFont("Arial-12")),
	backgroundColor_(Color::DarkGray)
{
	if (backBuffer_ == MAP_FAILED) perror("Failed to map memory\n");
	else printf("Success to map memory\n");
}

void PalmyraOS::SDK::WindowGUI::render()
{
	textRenderer_.setPosition(5, 0);
	textRenderer_.setSize(frameBuffer_.getWidth(), frameBuffer_.getHeight());
	currentWindowStatus_ = getStatus(window_.getID());
	Color barColor     = currentWindowStatus_.isActive ? PalmyraOS::Color::DarkerGray : PalmyraOS::Color::Black;
	Color borderColor  = currentWindowStatus_.isActive ? PalmyraOS::Color::Gray500 : PalmyraOS::Color::DarkerGray;
	Color stripesColor = currentWindowStatus_.isActive ? PalmyraOS::Color::Gray700 : PalmyraOS::Color::DarkGray;
	Color titleColor   = currentWindowStatus_.isActive ? PalmyraOS::Color::PrimaryLight : PalmyraOS::Color::Gray500;

	// Render the terminal UI frame
	brush_.fill(backgroundColor_);
	brush_.fillRectangle(0, 0, window_.getWidth(), 20, barColor);
	brush_.drawFrame(0, 0, window_.getWidth(), window_.getHeight(), borderColor);
	brush_.drawHLine(0, window_.getWidth(), 20, borderColor);
	textRenderer_ << titleColor;
	textRenderer_.setCursor(1, 1);
	textRenderer_ << window_.getTitle();

	// Horizontal lines on bar
	for (int y = 5; y <= 15; y += 5)
	{
		brush_.drawHLine(textRenderer_.getCursorX() + 10, currentWindowStatus_.width - 18, y, stripesColor);
	}

	Color exitColor = Color::DarkRed;
	Color exitHover = Color::Red;
	Color exitDown  = Color::DarkerRed;
	if (button("", currentWindowStatus_.width - 16, 3, 9, 15, 0, false, Color::Gray100, exitColor, exitHover, exitDown))
	{
		_exit(0);
	}

	textRenderer_.reset();
	textRenderer_.setPosition(3, 24);
	textRenderer_.setSize(frameBuffer_.getWidth() - 4, frameBuffer_.getHeight() - 4 - 21);
	textRenderer_ << PalmyraOS::Color::Gray100;

}

void PalmyraOS::SDK::WindowGUI::swapBuffers()
{
	textRenderer_.reset();
	frameBuffer_.swapBuffers();
	pollEvents();
	render();
}

PalmyraOS::kernel::Brush& PalmyraOS::SDK::WindowGUI::brush()
{
	return brush_;
}

PalmyraOS::kernel::TextRenderer& PalmyraOS::SDK::WindowGUI::text()
{
	return textRenderer_;
}

void PalmyraOS::SDK::WindowGUI::pollEvents()
{
	// as for now, we only
	wasLeftDown_ = currentMouseEvent_.isLeftDown;

	currentMouseEvent_ = nextMouseEvent(window_.getID());        // Fetch the next event

}

bool PalmyraOS::SDK::WindowGUI::button(
	const char* text,
	uint32_t x,
	uint32_t y,
	uint32_t width_,
	uint32_t height_,
	uint32_t margin,
	bool whileDown,
	Color textColor,
	Color backColor,
	Color colorHover,
	Color colorDown
)
{
	// set up width and height
	uint32_t width  = margin * 2;
	uint32_t height = margin * 2;

	if (width_ == 0) width += textRenderer_.calculateWidth(text);
	else width += width_;

	if (height_ == 0) height += textRenderer_.calculateHeight();
	else height += height_;

	// Use clipping logic to adjust the button boundaries
	ClippedBounds clipped = clipToTextRenderer(x, y, width, height);
	if (clipped.isClipped) return false;  // Skip rendering if button is fully outside the clipping area

	// Determine if hovering
	bool c1 = currentMouseEvent_.x >= clipped.xMin && currentMouseEvent_.x < clipped.xMax;
	bool c2 = currentMouseEvent_.y >= clipped.yMin && currentMouseEvent_.y < clipped.yMax;

	bool isHovering = c1 && c2;
	bool isActive   = currentWindowStatus_.isActive;
	bool isClicked  = isActive && isHovering && currentMouseEvent_.isLeftDown;

	Color background = isHovering ? colorHover : backColor;
	if (isClicked) background = colorDown;

	brush_.fillRectangle(clipped.xMin, clipped.yMin, clipped.xMax, clipped.yMax, background);

	Color currentColor = textRenderer_.getCurrentColor();
	textRenderer_.setCursor(x + margin, y);
	textRenderer_ << textColor << text << ' ' << currentColor;
	textRenderer_.setCursor(textRenderer_.getCursorX() + margin, textRenderer_.getCursorY());

	// Return true only when the button was pressed and then released while hovering
	if (isActive && wasLeftDown_ && !currentMouseEvent_.isLeftDown && isHovering) return true;

	// Return true continuously while the button is held down if `whileDown` is set to true
	if (isActive && whileDown && isHovering && currentMouseEvent_.isLeftDown) return true;

	return false;
}

bool PalmyraOS::SDK::WindowGUI::link(
	const char* text,
	bool whileDown,
	PalmyraOS::Color color,
	PalmyraOS::Color colorHover,
	PalmyraOS::Color colorDown
)
{
	uint32_t x      = textRenderer_.getCursorX();
	uint32_t y      = textRenderer_.getCursorY() + 3;
	uint32_t width  = textRenderer_.calculateWidth(text);
	uint32_t height = textRenderer_.calculateHeight() + 3;

	// Use clipping logic to adjust the button boundaries
	ClippedBounds clipped = clipToTextRenderer(x, y, width, height);

	// Skip rendering if button is fully outside the clipping area
	if (clipped.isClipped) return false;

	// determine if hovering
	bool c1 = currentMouseEvent_.x >= clipped.xMin && currentMouseEvent_.x < clipped.xMax;
	bool c2 = currentMouseEvent_.y >= clipped.yMin && currentMouseEvent_.y < clipped.yMax;

	bool isHovering = c1 && c2;
	bool isActive   = currentWindowStatus_.isActive;
	bool isClicked  = isActive && isHovering && currentMouseEvent_.isLeftDown;

	Color textColor = isHovering ? colorHover : color;
	if (isClicked) textColor = colorDown;

	Color currentColor = textRenderer_.getCurrentColor();
	textRenderer_ << textColor << text << ' ' << currentColor;
	brush_.drawHLine(clipped.xMin, clipped.xMax, clipped.yMax - 4, textColor);

	// Return true if the link was clicked and released
	if (isActive && wasLeftDown_ && !currentMouseEvent_.isLeftDown && isHovering) return true;

	// Return true continuously while the button is held down if `whileDown` is set to true
	if (isActive && whileDown && isHovering && currentMouseEvent_.isLeftDown) return true;

	return false;
}

std::pair<int, int> PalmyraOS::SDK::WindowGUI::getMousePosition()
{
	return { currentMouseEvent_.x, currentMouseEvent_.y };
}

void PalmyraOS::SDK::WindowGUI::setBackground(PalmyraOS::Color color)
{
	backgroundColor_ = color;
}

std::pair<uint32_t, uint32_t> PalmyraOS::SDK::WindowGUI::getFrameBufferSize()
{
	return { frameBuffer_.getWidth(), frameBuffer_.getHeight() };
}

PalmyraOS::SDK::ClippedBounds PalmyraOS::SDK::WindowGUI::clipToTextRenderer(
	uint32_t x,
	uint32_t y,
	uint32_t width,
	uint32_t height
)
{
	uint32_t xMin = textRenderer_.getPositionX() + x;
	uint32_t yMin = textRenderer_.getPositionY() + y;
	uint32_t xMax = xMin + width;
	uint32_t yMax = yMin + height;

	// Get the clipping area defined by the textRenderer_
	uint32_t clipXMin = textRenderer_.getPositionX();
	uint32_t clipYMin = textRenderer_.getPositionY();
	uint32_t clipXMax = clipXMin + textRenderer_.getWidth();
	uint32_t clipYMax = clipYMin + textRenderer_.getHeight();

	// Check if the element is fully outside the clipping area
	if (xMax < clipXMin || xMin > clipXMax || yMax < clipYMin || yMin > clipYMax)
	{
		return { 0, 0, 0, 0, true }; // Element is completely outside the clipping area
	}

	// Adjust element boundaries if they exceed the clipping area
	if (xMin < clipXMin) xMin = clipXMin;
	if (yMin < clipYMin) yMin = clipYMin;
	if (xMax > clipXMax) xMax = clipXMax;
	if (yMax > clipYMax) yMax = clipYMax;

	return { xMin, yMin, xMax, yMax, false };
}

void PalmyraOS::SDK::WindowGUI::fillRectangle(uint32_t x, uint32_t y, uint32_t width, uint32_t height, Color background)
{
	ClippedBounds clipped = clipToTextRenderer(x, y, width, height);
	if (clipped.isClipped) return;

	brush_.fillRectangle(clipped.xMin, clipped.yMin, clipped.xMax, clipped.yMax, background);
}

/**********************************************************/



PalmyraOS::SDK::Layout::Layout(WindowGUI& windowGui, int* scrollY, bool scrollable, size_t height)
	:
	windowGui_(windowGui),
	scrollable_(scrollable),
	scrollBarWidth_(scrollable_ ? 5 : 0),

	prevPositionX_(windowGui_.text().getPositionX()),
	prevPositionY_(windowGui_.text().getPositionY()),
	prevWidth_(windowGui_.text().getWidth()),
	prevHeight_(windowGui_.text().getHeight()),
	prevCursorY_(windowGui_.text().getCursorY()),

	currCursorX_(windowGui_.text().getCursorX() + 2),
	currCursorY_(windowGui_.text().getCursorY() + 2),
	currWidth_(prevWidth_ - currCursorX_ - scrollBarWidth_ - 2),
	currHeight_(prevHeight_ - currCursorY_ - 2), // Maximum Area
	scrollY_(scrollY)
{
	// if height is explicitly given, cap the  maximum area
	if (height > 0) currHeight_ = std::min<uint32_t>(height, currHeight_);


	currScrollY_ = scrollY_ ? *scrollY_ : 0;

	// Set the text rendering position and size according to the layout
	windowGui_.text().setPosition(prevPositionX_ + currCursorX_, prevPositionY_ + currCursorY_);
	windowGui_.text().setSize(currWidth_, currHeight_);
	windowGui.text().setCursor(0, currScrollY_);

	windowGui_.brush().drawHLine(
		windowGui_.text().getPositionX(),
		windowGui_.text().getPositionX() + windowGui_.text().getWidth(),
		windowGui_.text().getPositionY(),
		Color::Gray500
	);

}

PalmyraOS::SDK::Layout::~Layout()
{

	windowGui_.brush().drawHLine(
		windowGui_.text().getPositionX(),
		windowGui_.text().getPositionX() + windowGui_.text().getWidth(),
		windowGui_.text().getPositionY() + windowGui_.text().getHeight(),
		Color::Gray500
	);

	// Calculate content height
	int contentHeight = windowGui_.text().getCursorY() - currScrollY_;

	windowGui_.text().setCursor(0, prevCursorY_ + windowGui_.text().getHeight() + 4);
	windowGui_.text().setPosition(prevPositionX_, prevPositionY_);
	windowGui_.text().setSize(prevWidth_, prevHeight_);

	// Calculate Scrollbar Position and Size
	if (scrollable_ && scrollY_)
	{
		uint32_t scrollBarX      = prevPositionX_ + currWidth_ + 1;
		uint32_t scrollBarY      = currCursorY_;
		uint32_t scrollBarHeight = currHeight_;

		if (windowGui_.button(
			"",
			scrollBarX,
			scrollBarY,
			scrollBarWidth_,
			scrollBarHeight / 2,
			0,
			true
		)
			)
		{
			currScrollY_ = std::min(0, currScrollY_ + 1);
		}

		if (windowGui_.button(
			"",
			scrollBarX,
			scrollBarY + scrollBarHeight / 2,
			scrollBarWidth_,
			scrollBarHeight / 2,
			0,
			true
		)
			)
		{
			currScrollY_ = std::max<int>(currScrollY_ - 1, -(contentHeight - currHeight_));
		}

		// store the current scroll
		if (scrollY_) *scrollY_ = currScrollY_;

		windowGui_.brush().drawFrame(
			prevPositionX_ + scrollBarX,
			prevPositionY_ + scrollBarY,
			prevPositionX_ + scrollBarX + scrollBarWidth_,
			prevPositionY_ + scrollBarY + scrollBarHeight,
			Color::Yellow
		);
	}
}












