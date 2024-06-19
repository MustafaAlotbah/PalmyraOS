

#include "userland/systemWidgets/menuBar.h"

#include "palmyraOS/unistd.h"       // PalmyraOS API
#include "palmyraOS/time.h"

#include <cstddef>                    // size_t
#include "libs/string.h"            // strlen

// TODO: move these to libs (or put them in palmyraOS)
#include "core/FrameBuffer.h"        // FrameBuffer
#include "core/VBE.h"                // for Brush, TextRenderer
#include "core/Font.h"               // For Fonts


[[noreturn]] int PalmyraOS::MenuBar::main(uint32_t argc, char** argv)
{
	// Calculate Required Memory
	size_t width          = 1024;
	size_t height         = 20;
	size_t requiredMemory = width * height * sizeof(uint32_t);

	// allocate memory for back buffer
	uint32_t* backBuffer = nullptr;
	{
		backBuffer = (uint32_t*)mmap(
			nullptr, requiredMemory, PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_PRIVATE, -1, 0
		);
		if (backBuffer == MAP_FAILED)
		{
			const char* message = "Failed to allocate memory\n";
			write(STDERR, message, strlen(message));
			_exit(-1);
		}
		else
		{
			const char* message = "Success to allocate memory\n";
			write(STDOUT, message, strlen(message));
		}
	}

	// Request a window
	uint32_t* frontBuffer = nullptr;
	size_t bufferId = 0;
	{
		bufferId = initializeWindow(&frontBuffer, 0, 0, width, height);
		if (bufferId == 0)
		{
			const char* message = "Failed to initialize window\n";
			write(STDERR, message, strlen(message));
			_exit(-1);
		}
		else
		{
			const char* message = "Success to initialize window\n";
			write(STDOUT, message, strlen(message));
		}
	}

	// Helper objects for rendering
	PalmyraOS::kernel::FrameBuffer frameBuffer(width, height, frontBuffer, backBuffer);
	PalmyraOS::kernel::Brush       brush(frameBuffer);

	// Font requires kernel memory! TODO: only allowed at this stage, not anymore later
	PalmyraOS::kernel::TextRenderer textRenderer(frameBuffer, PalmyraOS::fonts::FontManager::getFont("Arial-12"));

	// initially
	brush.fill(PalmyraOS::Color::White);


	timespec time_spec{};

	// main loop
	while (true)
	{
		clock_gettime(CLOCK_MONOTONIC, &time_spec);

		brush.fill(PalmyraOS::Color::Black);
		brush.drawHLine(0, width, height - 1, PalmyraOS::Color::White);
		textRenderer.reset();
		textRenderer << "my pid: " << get_pid() << "\t";
		textRenderer << "time: " << time_spec.tv_sec << " s\t";
		textRenderer << "time: " << time_spec.tv_nsec << " ns\t";

		frameBuffer.swapBuffers();

		// TODO later count time and fps
		sched_yield(); // this is all we ask for at every frame
	}


}
