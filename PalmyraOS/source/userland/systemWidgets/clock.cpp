
#include "userland/systemWidgets/clock.h"

#include "palmyraOS/unistd.h"       // Include PalmyraOS system calls
#include "palmyraOS/time.h"            // For sleeping
#include "palmyraOS/stdlib.h"       // For dynamic memory management
#include "palmyraOS/stdio.h"        // For standard input/output functions: printf, perror
#include "palmyraOS/HeapAllocator.h"// C++ heap allocator for efficient memory management
#include "palmyraOS/errono.h"
#include "libs/circularBuffer.h"    // For efficient FIFO buffer implementation

#include "libs/string.h"            // strlen
#include "libs/pmath.h"

// TODO: move these to libs (or put them in palmyraOS)
#include "core/FrameBuffer.h"        // FrameBuffer
#include "core/VBE.h"                // for Brush, TextRenderer
#include "core/Font.h"               // For Fonts


// Helper function to convert degrees to radians
inline double degToRad(double deg)
{
	return deg * 3.14 / 180.0;
}

int PalmyraOS::Userland::builtin::KernelClock::main(uint32_t argc, char** argv)
{


	// Initialize dynamic memory allocator for the application
	types::UserHeapManager heap;

	// Set initial window position and dimensions
	size_t x = 10, y = 10, width = 100, height = 120;

	// Allocate a large buffer for double buffering in graphics
	size_t total_size = (width * height * sizeof(uint32_t)) * 4096;
	void* backBuffer = malloc(total_size);
	if (backBuffer == MAP_FAILED) perror("Failed to map memory\n");
	else printf("Success to map memory\n");

	// Create and set up the main application window
	uint32_t* frontBuffer = nullptr;
	uint32_t window_id = initializeWindow(&frontBuffer, x, y, width, height);
	if (window_id == 0) perror("Failed to initialize window\n");
	else printf("Success to initialize window\n");

	// Initialize graphics objects for rendering
	PalmyraOS::kernel::FrameBuffer  frameBuffer(width, height, frontBuffer, (uint32_t*)backBuffer);
	PalmyraOS::kernel::Brush        brush(frameBuffer);
	PalmyraOS::kernel::TextRenderer textRenderer(frameBuffer, PalmyraOS::fonts::FontManager::getFont("Arial-12"));
	textRenderer.setPosition(5, 0);

	// Initialize time structure
	size_t   epochTime_fd = 0;
	rtc_time epochTime{};
	{
		epochTime_fd = open("/dev/rtc", 0);
		if (epochTime_fd) ioctl(epochTime_fd, RTC_RD_TIME, &epochTime);
	}
	// Constants for clock hand lengths
	constexpr int secondHandLength = 40;
	constexpr int minuteHandLength = 35;
	constexpr int hourHandLength   = 25;

	// Center of the clock
	const int centerX = width / 2;
	const int centerY = (height + 20) / 2;

	while (true)
	{

		// Render the terminal UI frame
		brush.fill(PalmyraOS::Color::DarkerGray);
		brush.fillRectangle(0, 0, width, 20, PalmyraOS::Color::DarkRed);
		brush.drawFrame(0, 0, width, height, PalmyraOS::Color::White);
		brush.drawHLine(0, width, 20, PalmyraOS::Color::White);
		textRenderer << PalmyraOS::Color::White;
		textRenderer.setCursor(1, 1);
		textRenderer << "Palmyra Clock\n";
		textRenderer.reset();
		textRenderer.setCursor(1, 21);


		// Render the current time if available
		if (epochTime_fd)
		{
			ioctl(epochTime_fd, RTC_RD_TIME, &epochTime);

			// Get the second, minute, and hour components
			int seconds = epochTime.tm_sec;
			int minutes = epochTime.tm_min;
			int hours   = epochTime.tm_hour % 12;

			// Calculate endpoints for the second hand using lookup tables
			int secondX = centerX + static_cast<int>(secondHandLength * math::sin_table[seconds]);
			int secondY = centerY - static_cast<int>(secondHandLength * math::cos_table[seconds]);

			// Calculate endpoints for the minute hand using lookup tables
			int minuteX = centerX + static_cast<int>(minuteHandLength * math::sin_table[minutes]);
			int minuteY = centerY - static_cast<int>(minuteHandLength * math::cos_table[minutes]);

			// Calculate endpoints for the hour hand (approximate hour angle based on minutes)
			int hourIndex = (hours * 5) + (minutes / 12); // approximate position of hour based on minutes
			int hourX     = centerX + static_cast<int>(hourHandLength * math::sin_table[hourIndex]);
			int hourY     = centerY - static_cast<int>(hourHandLength * math::cos_table[hourIndex]);

			// Draw the clock hands
			brush.drawLine(centerX, centerY, secondX, secondY, PalmyraOS::Color::LightBlue);     // Red second hand
			brush.drawLine(centerX, centerY, minuteX, minuteY, PalmyraOS::Color::White);   // White minute hand
			brush.drawLine(centerX, centerY, hourX, hourY, PalmyraOS::Color::Orange);        // Blue hour hand

		}


		// Reset text renderer and swap frame buffers for next frame
		textRenderer.reset();
		frameBuffer.swapBuffers();

		// Yield to other processes
		sched_yield();
	}


	return 0;
}
