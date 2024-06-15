#include "libs/stdio.h"
#include "libs/string.h"
#include "libs/memory.h"
#include "libs/stdlib.h"


// Implementation of a simplified vsprintf
size_t vsprintf(char* str, const char* format, va_list args)
{
	const char* traverse;
	char      * s;
	int d;
	char* out = str;

	for (traverse = format; *traverse; traverse++)
	{
		if (*traverse != '%')
		{
			*out++ = *traverse;
			continue;
		}

		traverse++;  // Move past '%'

		if (*traverse == '%')
		{  // Handle escaped percent sign (%%)
			*out++ = '%';
			continue;
		}

		switch (*traverse)
		{
			case 's':  // String
				s = va_arg(args, char*);
				strcpy(out, s);
				out += strlen(s);
				break;
			case 'd':  // Decimal
				d = va_arg(args, int32_t);
				char num_str[40];
				itoa((int)d, num_str, 10);
				strcpy(out, num_str);
				out += strlen(num_str);
				break;
			case 'u':  // uint32_t
				d = va_arg(args, uint32_t);
				itoa((uint32_t)d, num_str, 10);
				strcpy(out, num_str);
				out += strlen(num_str);
				break;
			case 'b':  // binary
				d = va_arg(args, uint32_t);
				itoa((uint32_t)d, num_str, 2);
				strcpy(out, num_str);
				out += strlen(num_str);
				break;
			case 'x':  // Hexadecimal (lowercase)
				d = va_arg(args, uint32_t);
				itoa((uint32_t)d, num_str, 16);
				strcpy(out, num_str);
				out += strlen(num_str);
				break;
			case 'X':  // Hexadecimal (uppercase)
				d = va_arg(args, uint32_t);
				itoa((uint32_t)d, num_str, 16, true);
				strcpy(out, num_str);
				out += strlen(num_str);
				break;
			case 'z':  // size_t with 'u' for unsigned
				if (*(traverse + 1) == 'u')
				{
					traverse++;
					uint64_t z = va_arg(args, size_t);
					uitoa64(z, num_str, 10, false);
					strcpy(out, num_str);
					out += strlen(num_str);
				}
				else
				{
					*out++ = '%';
					*out++ = 'z';
				}
				break;
			default:
				*out++ = '%'; // Include the '%'
				*out++ = *traverse;  // and the following character as they are
				break;
		}
	}

	*out = '\0';  // Null-terminate the string
	return out - str;  // Number of characters written
}

// Implementation of a simplified sprintf
size_t sprintf(char* str, const char* format, ...)
{
	va_list ap;
	va_start(ap, format);
	size_t written = vsprintf(str, format, ap);
	va_end(ap);
	return written;
}

// Implementation of a simplified vsnprintf
size_t vsnprintf(char* str, size_t size, const char* format, va_list ap)
{
	char buffer[1024] = { 0 };

	size_t written = vsprintf(buffer, format, ap);

	if (written > size) written = size;  // Truncate if necessary
	memcpy(str, buffer, written);
	str[written] = '\0';  // Ensure null termination

	return written;
}

// Implementation of a simplified snprintf
size_t snprintf(char* str, size_t size, const char* format, ...)
{
	va_list ap;
	va_start(ap, format);
	size_t written = vsnprintf(str, size, format, ap);
	va_end(ap);
	return written;
}


