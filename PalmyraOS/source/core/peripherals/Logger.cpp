
#include "core/peripherals/Logger.h"

#include "core/port.h"
#include "core/cpu.h"

#include "libs/stdio.h"


#include <cstdarg>


PalmyraOS::kernel::ports::BytePort loggingPort(0x3F8);

void log_msg(bool slow, const char* message)
{
	uint64_t maxBuffer = 4096;
	if (slow)
	{
		// Slow output (for important logs)
		for (int i = 0; i < maxBuffer; ++i)
		{
			PalmyraOS::kernel::CPU::delay(512 * 1024);
			if (message[i] == '\0') break;
			loggingPort.write(message[i]);
		}
	}
	else
	{
		// Fast output (for frequent logs)
		for (int i = 0; i < maxBuffer; ++i)
		{
			if (message[i] == '\0') break;
			loggingPort.write(message[i]);
		}
	}
}

void log_msgf(bool slow, const char* format, ...)
{
	va_list args;
	va_start(args, format);
	char buffer[4096];  // Adjust size as necessary for your needs
	vsnprintf(buffer, sizeof(buffer), format, args);
	va_end(args);
	log_msg(slow, buffer);
}

void PalmyraOS::kernel::log(const char* level, bool slow, const char* function, uint32_t line, const char* format, ...)
{
	va_list args;
	va_start(args, format);
	char buffer[4096];  // Adjust size as necessary for your needs
	vsnprintf(buffer, sizeof(buffer), format, args);
	va_end(args);
	log_msgf(slow, "%s [%s:%d] %s\n", level, function, line, buffer);
}
