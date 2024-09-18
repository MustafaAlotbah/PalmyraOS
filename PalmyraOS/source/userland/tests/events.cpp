
#include "userland/tests/events.h"

#include "libs/palmyraSDK.h"        // Window, Window Frame


int PalmyraOS::Userland::Tests::events::main(uint32_t argc, char** argv)
{

	// Create and set up the main application window
	SDK::Window    window(300, 300, 340, 200, true, "Events Tests");
	SDK::WindowGUI windowGui(window);

	KeyboardEvent keyboardEvent;
	MouseEvent    mouseEvent;

	uint32_t clickCounter = 0;
	int      scrollY      = 0;

	// layout
	int scrollY_layout = 0;

	while (true)
	{
		// Mimic scrolling here for testing
		windowGui.text().setCursor(windowGui.text().getCursorX(), scrollY);

		// manually fetch keyboard events
		while (true)
		{
			KeyboardEvent event = nextKeyboardEvent(window.getID());        // Fetch the next event
			if (!event.isValid) break;
			keyboardEvent = event; // get last valid event
		}
		windowGui.text() << "Keyboard: "
						 << "key: '" << keyboardEvent.key
						 << "' ["
						 << (keyboardEvent.isCtrlDown ? "CTRL " : "")
						 << (keyboardEvent.isAltDown ? "ALT " : "")
						 << (keyboardEvent.isShiftDown ? "SHIFT " : "")
						 << "]\n";

		// manually fetch mouse events
		while (true)
		{
			MouseEvent event = nextMouseEvent(window.getID());        // Fetch the next event
			if (!event.isEvent) break;
			mouseEvent = event; // get last valid event
		}
		windowGui.text() << "Mouse: "
						 << "Coors: (" << mouseEvent.x << ", " << mouseEvent.y
						 << ") ["
						 << (mouseEvent.isLeftDown ? "LEFT " : "")
						 << (mouseEvent.isMiddleDown ? "MIDDLE " : "")
						 << (mouseEvent.isRightDown ? "RIGHT " : "")
						 << "]\n";


		if (windowGui.button("click me", windowGui.text().getCursorX(), windowGui.text().getCursorY()))
		{
			clickCounter++;
			windowGui.text() << "Clicking " << clickCounter << " times.\n";
		}
		else
		{
			windowGui.text() << "Clicked " << clickCounter << " times.\n";
		}

		if (windowGui.link("or click me"))
		{
			clickCounter++;
		}

		windowGui.text() << "\n";

		if (windowGui.link("scroll down", true))
		{
			scrollY--;
		}

		windowGui.text() << " ";

		if (windowGui.link("scroll up", true))
		{
			scrollY++;
		}

		windowGui.text() << "\n";

		{
			SDK::Layout layout(windowGui, &scrollY_layout, true);
			for (int    i = 0; i < 10; ++i)
			{
				for (int j = 0; j < 5; ++j)
				{
					windowGui.text() << "Item (" << i << ", " << j << ") ";
				}
				windowGui.text() << "\n";
			}
		}

		windowGui.text() << "Somthing";

		// Reset text renderer and swap frame buffers for next frame then yield
		windowGui.swapBuffers();
		sched_yield();
	}


	return 0;
}
