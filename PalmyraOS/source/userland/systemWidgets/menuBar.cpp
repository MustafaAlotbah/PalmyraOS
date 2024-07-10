

#include "userland/systemWidgets/menuBar.h"

#include "palmyraOS/unistd.h"       // PalmyraOS API
#include "palmyraOS/time.h"
#include "palmyraOS/stdlib.h"    // malloc
#include "palmyraOS/stdio.h"    // printf, perror

#include <cstddef>                    // size_t
#include "libs/string.h"            // strlen
#include "libs/stdio.h"                // snprintf

// TODO: move these to libs (or put them in palmyraOS)
#include "core/FrameBuffer.h"        // FrameBuffer
#include "core/VBE.h"                // for Brush, TextRenderer
#include "core/Font.h"               // For Fonts


// Helper function to allocate memory for back buffer
uint32_t* allocateBackBuffer(size_t requiredMemory)
{
	volatile auto backBuffer = (uint32_t*)malloc(requiredMemory);
	if (backBuffer == MAP_FAILED)
	{
		perror("Failed to map memory\n");
		_exit(-1);
	}
	else printf("Success to map memory\n");
	return backBuffer;
}

// Helper function to initialize window
size_t initializeWindowWrapper(uint32_t** frontBuffer, size_t width, size_t height)
{
	size_t bufferId = initializeWindow(frontBuffer, 0, 0, width, height);
	if (bufferId == 0)
	{
		perror("Failed to initialize window\n");
		_exit(-1);
	}
	else printf("Success to initialize window\n");
	return bufferId;
}

// Helper function to calculate elapsed time in seconds
int calculateElapsedTimeInSeconds(const rtc_time& start, const rtc_time& current)
{
	int startSeconds   = start.tm_hour * 3600 + start.tm_min * 60 + start.tm_sec;
	int currentSeconds = current.tm_hour * 3600 + current.tm_min * 60 + current.tm_sec;
	return currentSeconds - startSeconds;
}

[[noreturn]] int PalmyraOS::Userland::builtin::MenuBar::main(uint32_t argc, char** argv)
{

	// Define dimensions and required memory for the window
	size_t width          = 1024;     // TODO Retrieve actual window width dynamically
	size_t height         = 20;
	size_t requiredMemory = width * height * sizeof(uint32_t);

	// Allocate memory for the back buffer
	uint32_t* backBuffer = allocateBackBuffer(requiredMemory);

	// Request and initialize the front buffer window
	uint32_t* frontBuffer = nullptr;
	size_t bufferId = initializeWindowWrapper(&frontBuffer, width, height);

	// Initialize rendering helpers
	PalmyraOS::kernel::FrameBuffer frameBuffer(width, height, frontBuffer, backBuffer);
	PalmyraOS::kernel::Brush       brush(frameBuffer);

	// Load the font for text rendering (requires kernel memory) TODO: only allowed at this stage, not anymore later
	PalmyraOS::kernel::TextRenderer textRenderer(frameBuffer, PalmyraOS::fonts::FontManager::getFont("Arial-12"));

	// Fill the initial background
	brush.fill(PalmyraOS::Color::White);


	timespec time_spec{};
	uint64_t count = 0;

	// Open the real-time clock device
	size_t epochTime_fd = open("/dev/rtc", 0);
	// assume it has worked

	// Initialize time structure
	rtc_time epochTime{};
	rtc_time startTime{};
	ioctl(epochTime_fd, RTC_RD_TIME, &startTime);

	// Main loop for rendering and updates
	while (true)
	{
		count++;
		clock_gettime(CLOCK_MONOTONIC, &time_spec);

		brush.fill(PalmyraOS::Color::Black);
		brush.drawHLine(0, width, height - 1, PalmyraOS::Color::White);
		brush.drawHLine(0, width, height - 2, PalmyraOS::Color::Gray);

		// Render the logo and system information
		textRenderer << Color::Orange << "Palmyra" << Color::LightBlue << "OS ";
		textRenderer << Color::White << "v0.01\t";
		textRenderer << "SysTime: " << time_spec.tv_sec << " s\t";
		textRenderer << "ProCount: " << count << "\t";

		// Render the current time if available
		if (epochTime_fd)
		{
			ioctl(epochTime_fd, RTC_RD_TIME, &epochTime);

			char clock_buffer[50];
			snprintf(
				clock_buffer, 50, "%04d-%02d-%02d %02d:%02d:%02d",
				epochTime.tm_year + 1900,
				epochTime.tm_mon + 1,
				epochTime.tm_mday,
				epochTime.tm_hour,
				epochTime.tm_min,
				epochTime.tm_sec
			);
			textRenderer.setCursor(width / 2 - 50, 0);    // Center the clock text
			textRenderer << clock_buffer;


			// Calculate elapsed time in seconds using helper function
			int elapsedTime = calculateElapsedTimeInSeconds(startTime, epochTime);

			// Calculate FPS
			double fps = count / (elapsedTime > 0 ? elapsedTime : 1);
			textRenderer << "\tFPS: " << (int)fps;
		}

		// Reset text renderer and swap frame buffers for next frame
		textRenderer.reset();
		frameBuffer.swapBuffers();

		// Yield to other processes
		sched_yield();
	}

	// Close the RTC device if we ever exit the loop (unlikely with [[noreturn]])
	close(epochTime_fd);
}
