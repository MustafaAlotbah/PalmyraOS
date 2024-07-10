

#include "palmyraOS/stdio.h"
#include "palmyraOS/unistd.h"
#include "libs/stdio.h"
#include "libs/string.h"


int printf(const char* format, ...)
{
	va_list args;
	va_start(args, format);

	// Buffer to hold the formatted string
	char buffer[1024];

	// Format the string using snprintf
	int written = (int)vsnprintf(buffer, sizeof(buffer), format, args);

	// Write the formatted string to standard output
	if (written > 0)
	{
		write(1, buffer, written);
	}

	va_end(args);
	return written;
}

void perror(const char* str)
{
	if (str != nullptr && *str != '\0')
	{
		write(2, str, strlen(str));
		write(2, ": ", 2);
	}

	// TODO: stderror by errno, errno (TLS) Thread-Local Storage
//	const char *error_msg = strerror(errno);
//	if (error_msg != NULL) {
//		write(2, error_msg, strlen(error_msg));
//	}

	write(2, "\n", 1);
}