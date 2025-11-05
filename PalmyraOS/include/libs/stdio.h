#pragma once

#include "core/definitions.h"  // stdint + size_t
#include <cstdarg>

/**
 * Writes formatted data from variable argument list to a sized buffer.
 *
 * @param str Pointer to a buffer where the resulting C-string is stored.
 * @param size Maximum number of bytes to be used in the buffer, including the null-terminating character.
 * @param format C string that contains a format string that follows the same specifications as format in printf.
 * @param ap A va_list representing a list of arguments to be formatted.
 *
 * @return The number of characters that would have been written if size were sufficiently large, not counting the terminating null character.
 * If an output error is encountered, a negative value is returned.
 */
size_t vsnprintf(char* str, size_t size, const char* format, va_list ap);

/**
 * Writes formatted data from variable argument list to a string.
 *
 * @param str Pointer to a buffer where the resulting C-string is stored.
 * @param format C string that contains the format string that follows the same specifications as format in printf.
 * @param args A va_list representing a list of arguments to be formatted.
 *
 * @return The number of characters written not including the null-terminating character. If an output error is encountered, a negative value is returned.
 */
size_t vsprintf(char* str, const char* format, va_list args);

/**
 * Writes formatted data to a string.
 *
 * @param str Pointer to a buffer where the resulting C-string is stored.
 * @param format C string that contains the format string that follows the same specifications as format in printf.
 * @param ... Variable arguments providing data to be formatted according to the format string.
 *
 * @return The number of characters written not including the null-terminating character. If an output error is encountered, a negative value is returned.
 */
size_t sprintf(char* str, const char* format, ...);

/**
 * Writes formatted data from variable argument list to a sized buffer.
 *
 * @param str Pointer to a buffer where the resulting C-string is stored.
 * @param size Maximum number of bytes to be used in the buffer, including the null-terminating character.
 * @param format C string that contains a format string that follows the same specifications as format in printf.
 * @param ... Variable arguments providing data to be formatted according to the format string.
 *
 * @return The number of characters that would have been written if size were sufficiently large, not counting the terminating null character.
 * If an output error is encountered, a negative value is returned.
 */
size_t snprintf(char* str, size_t size, const char* format, ...);


/**
 * Parses a formatted string and extracts values into pointers.
 *
 * @param str Input string to parse
 * @param format Format string with embedded format specifiers
 * @param args Variable argument list (va_list) with pointers to store parsed values
 *
 * @return Number of successfully parsed and assigned items, or EOF on error
 * @note String format (%s) MUST use width specifier to prevent buffer overflow!
 * @note All pointers must be valid and correctly typed
 */
size_t vsscanf(const char* str, const char* format, va_list args);

/**
 * Parses a formatted string and extracts values into pointers.
 *
 * @param str Input string to parse
 * @param format Format string with embedded format specifiers
 * @param ... Pointers to variables where parsed values will be stored
 *
 * @return Number of successfully parsed and assigned items
 *
 * @see vsscanf for detailed documentation and safety notes
 */
size_t sscanf(const char* str, const char* format, ...);