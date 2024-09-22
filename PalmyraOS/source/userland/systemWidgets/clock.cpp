
#include "userland/systemWidgets/clock.h"

#include "palmyraOS/unistd.h"       // Include PalmyraOS system calls
#include "palmyraOS/time.h"         // For rtc_time truct
#include "palmyraOS/palmyraSDK.h"        // Window, Window Frame
#include "libs/pmath.h"                // sin, cos


int PalmyraOS::Userland::builtin::KernelClock::main(uint32_t argc, char** argv)
{

	// Create and set up the main application window
	SDK::Window    window(914, 30, 100, 120, true, "Clock");
	SDK::WindowGUI windowFrame(window);

	// Initialize time structure
	size_t   epochTime_fd = 0;
	rtc_time epochTime{};
	epochTime_fd = open("/dev/rtc", 0);
	if (epochTime_fd) ioctl(epochTime_fd, RTC_RD_TIME, &epochTime);

	// Constants for clock hand lengths
	constexpr int secondHandLength = 35;
	constexpr int minuteHandLength = 30;
	constexpr int hourHandLength   = 20;
	constexpr int clockRadius      = 40;

	// Center of the clock
	const int centerX = window.getWidth() / 2;
	const int centerY = (window.getHeight() + 20) / 2;

	while (true)
	{
		windowFrame.brush().fillCircle(centerX, centerY, 47, Color::DarkerGray);
		// Render the numbers around the clock
		windowFrame.text() << PalmyraOS::Color::Gray500;
		for (int i = 1; i <= 12; ++i)
		{
			int angle   = i * 30; // Each hour is 30 degrees apart
			int numberX = centerX + static_cast<int>(clockRadius * math::sin(angle));
			int numberY = centerY - static_cast<int>(clockRadius * math::cos(angle));

			// Adjust for better centering
			int xOffset = -3 - windowFrame.text().getPositionX();
			int yOffset = -8 - windowFrame.text().getPositionY();

			// Set text position and render the number
			windowFrame.text().setCursor(numberX + xOffset, numberY + yOffset);
			windowFrame.text() << i;
		}

		// Render the current time if available
		if (epochTime_fd)
		{
			ioctl(epochTime_fd, RTC_RD_TIME, &epochTime);

			// Get the second, minute, and hour components
			int seconds = epochTime.tm_sec;
			int minutes = epochTime.tm_min;
			int hours   = epochTime.tm_hour % 12;

			// Convert seconds, minutes, and hours to degrees
			int secondAngle = seconds * 6;                // 360 degrees / 60 seconds = 6 degrees per second
			int minuteAngle = minutes * 6;                // 360 degrees / 60 minutes = 6 degrees per minute
			int hourAngle = hours * 30 + (minutes / 2); // 360 degrees / 12 hours = 30 degrees per hour

			// Calculate endpoints for the second hand using lookup tables
			int secondX = centerX + static_cast<int>(secondHandLength * math::sin(secondAngle));
			int secondY = centerY - static_cast<int>(secondHandLength * math::cos(secondAngle));

			// Calculate endpoints for the minute hand using lookup tables
			int minuteX = centerX + static_cast<int>(minuteHandLength * math::sin(minuteAngle));
			int minuteY = centerY - static_cast<int>(minuteHandLength * math::cos(minuteAngle));

			// Calculate endpoints for the hour hand (approximate hour angle based on minutes)
			int hourX = centerX + static_cast<int>(hourHandLength * math::sin(hourAngle));
			int hourY = centerY - static_cast<int>(hourHandLength * math::cos(hourAngle));

			// Clock hands

			// Seconds
			windowFrame.brush().drawLine(centerX, centerY, secondX, secondY, Color::Gray300);

			// Minutes
			windowFrame.brush().drawLine(centerX, centerY, minuteX, minuteY, Color::Orange);

			// Hours
			windowFrame.brush().drawLine(centerX, centerY, hourX, hourY, Color::PrimaryLight);

		}

		// Reset text renderer and swap frame buffers for next frame
		windowFrame.swapBuffers();

		// Yield to other processes
		sched_yield();
	}

	return 0;
}
