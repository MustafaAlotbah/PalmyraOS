
#include "core/peripherals/Logger.h"

#include "core/port.h"
#include "core/cpu.h"

#include "libs/stdio.h"


#include <cstdarg>


PalmyraOS::kernel::ports::SlowBytePort loggingPort(0x3F8);

void log_msg(const char* message)
{
	uint64_t maxBuffer = 4096;
	for (int i         = 0; i < maxBuffer; ++i)
	{
		if (message[i] == '\0') break;
		loggingPort.write(message[i]);
	}
}

void log_msgf(const char* format, ...)
{
	va_list args;
	va_start(args, format);
	char buffer[4096];  // Adjust size as necessary for your needs
	vsnprintf(buffer, sizeof(buffer), format, args);
	va_end(args);
	log_msg(buffer);
}

void PalmyraOS::kernel::log(const char* level, const char* function, uint32_t line, const char* format, ...)
{
	va_list args;
	va_start(args, format);
	char buffer[4096];  // Adjust size as necessary for your needs
	vsnprintf(buffer, sizeof(buffer), format, args);
	va_end(args);
	log_msgf("%s [%s:%d] %s\n", level, function, line, buffer);
}
