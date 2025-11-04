#include "libs/stdio.h"
#include "libs/memory.h"
#include "libs/stdlib.h"
#include "libs/string.h"

/**
 * @brief Simplified variable-argument sprintf implementation for PalmyraOS
 *
 * Formats a string according to a format specification and writes it to a buffer.
 * This is a lightweight alternative to standard sprintf with support for essential
 * format specifiers.
 *
 * @param str Output buffer where the formatted string will be written
 * @param format Format string with embedded format specifiers
 * @param args Variable argument list (va_list) containing values to format
 * @return Number of characters written to the buffer (excluding null terminator)
 *
 * Supported Format Specifiers:
 * ============================
 *
 * %s          - C-string (char*)
 *               Example: sprintf(buf, "Name: %s", "Alice"): "Name: Alice"
 *
 * %d          - Signed 32-bit integer (int32_t)
 *               Example: sprintf(buf, "Count: %d", -42): "Count: -42"
 *
 * %u          - Unsigned 32-bit integer (uint32_t)
 *               Example: sprintf(buf, "Value: %u", 12345): "Value: 12345"
 *
 * %lu         - Unsigned long (unsigned long)
 *               Example: sprintf(buf, "Ticks: %lu", 5000000): "Ticks: 5000000"
 *
 * %zu         - Size type unsigned (size_t)
 *               Example: sprintf(buf, "Size: %zu", sizeof(int)): "Size: 4"
 *
 * %x          - Hexadecimal unsigned (lowercase)
 *               Example: sprintf(buf, "Hex: %x", 255): "Hex: ff"
 *
 * %X          - Hexadecimal unsigned (uppercase)
 *               Example: sprintf(buf, "Hex: %X", 255): "Hex: FF"
 *
 * %b          - Binary representation
 *               Example: sprintf(buf, "Binary: %b", 15): "Binary: 1111"
 *
 * %f          - Double-precision floating point
 *               Precision can be specified: %.2f
 *               Example: sprintf(buf, "Pi: %.2f", 3.14159): "Pi: 3.14"
 *
 * %%          - Escaped percent sign (outputs single %)
 *               Example: sprintf(buf, "Discount: %%"): "Discount: %"
 *
 * Width Specifier:
 * ================
 * %5d         - Minimum field width (zero-padded for numbers)
 *               Example: sprintf(buf, "Value: %5d", 42): "Value: 00042"
 *
 * Precision Specifier (for %f):
 * ==============================
 * %.2f        - Floating point with 2 decimal places
 *               Example: sprintf(buf, "Result: %.2f", 1.23456): "Result: 1.23"
 *
 * @note This is a simplified implementation. Not all standard sprintf features are supported.
 * @note For size_t and unsigned long, use %zu and %lu respectively for portability.
 * @warning Buffer overflow is not checked; ensure the buffer is large enough for output.
 */
size_t vsprintf(char* str, const char* format, va_list args) {
    const char* traverse;
    char* s;
    int d;
    double f;
    uint32_t len;
    char* out = str;
    char num_str[40];

    for (traverse = format; *traverse; traverse++) {
        if (*traverse != '%') {
            *out++ = *traverse;
            continue;
        }

        traverse++;  // Move past '%'

        if (*traverse == '%') {  // Handle escaped percent sign (%%)
            *out++ = '%';
            continue;
        }

        // Parse width and precision
        int width     = 0;
        int precision = -1;
        if (*traverse >= '0' && *traverse <= '9') {
            width = atoi(traverse);
            while (*traverse >= '0' && *traverse <= '9') traverse++;
        }

        if (*traverse == '.') {
            traverse++;
            precision = atoi(traverse);
            while (*traverse >= '0' && *traverse <= '9') traverse++;
        }

        switch (*traverse) {
            case 's':  // String
                s = va_arg(args, char*);
                strcpy(out, s);
                out += strlen(s);
                break;
            case 'c':  // Single character
                d      = va_arg(args, int);
                *out++ = (char) d;
                break;
            case 'p':  // Pointer address in hexadecimal
                d = va_arg(args, int);
                itoa((uint32_t) d, num_str, 16);
                *out++ = '0';
                *out++ = 'x';
                strcpy(out, num_str);
                out += strlen(num_str);
                break;
            case 'd':  // Decimal
                d = va_arg(args, int32_t);
                itoa((int) d, num_str, 10);

                // Handle width formatting with padding
                len = strlen(num_str);
                if (width > len) {
                    memset(out, '0', width - len);
                    out += width - len;
                }

                strcpy(out, num_str);
                out += strlen(num_str);
                break;
            case 'i':  // Signed integer (alias for %d)
                d = va_arg(args, int32_t);
                itoa((int) d, num_str, 10);

                // Handle width formatting with padding
                len = strlen(num_str);
                if (width > len) {
                    memset(out, '0', width - len);
                    out += width - len;
                }

                strcpy(out, num_str);
                out += strlen(num_str);
                break;
            case 'u':  // uint32_t
                d = va_arg(args, uint32_t);
                itoa((uint32_t) d, num_str, 10);

                // Handle width formatting with padding
                len = strlen(num_str);
                if (width > len) {
                    memset(out, '0', width - len);
                    out += width - len;
                }

                strcpy(out, num_str);
                out += strlen(num_str);
                break;
            case 'b':  // binary
                d = va_arg(args, uint32_t);
                itoa((uint32_t) d, num_str, 2);
                strcpy(out, num_str);
                out += strlen(num_str);
                break;
            case 'x':  // Hexadecimal (lowercase)
                d = va_arg(args, uint32_t);
                itoa((uint32_t) d, num_str, 16);
                strcpy(out, num_str);
                out += strlen(num_str);
                break;
            case 'X':  // Hexadecimal (uppercase)
                d = va_arg(args, uint32_t);
                itoa((uint32_t) d, num_str, 16, true);
                strcpy(out, num_str);
                out += strlen(num_str);
                break;
            case 'z':  // size_t with 'u' for unsigned
                if (*(traverse + 1) == 'u') {
                    traverse++;
                    uint64_t z = va_arg(args, size_t);
                    uitoa64(z, num_str, 10, false);
                    strcpy(out, num_str);
                    out += strlen(num_str);
                }
                else {
                    *out++ = '%';
                    *out++ = 'z';
                }
                break;
            case 'l':  // long variants: %ld (signed), %lu (unsigned), %lld, %llu
                if (*(traverse + 1) == 'd') {
                    traverse++;
                    int64_t ld = va_arg(args, int64_t);
                    if (ld < 0) {
                        *out++ = '-';
                        ld     = -ld;
                    }
                    uitoa64((uint64_t) ld, num_str, 10, false);
                    strcpy(out, num_str);
                    out += strlen(num_str);
                }
                else if (*(traverse + 1) == 'u') {
                    traverse++;
                    uint64_t lu = va_arg(args, unsigned long);
                    uitoa64(lu, num_str, 10, false);
                    strcpy(out, num_str);
                    out += strlen(num_str);
                }
                else if (*(traverse + 1) == 'l') {
                    traverse++;
                    if (*(traverse + 1) == 'd') {
                        traverse++;
                        int64_t lld = va_arg(args, int64_t);
                        if (lld < 0) {
                            *out++ = '-';
                            lld    = -lld;
                        }
                        uitoa64((uint64_t) lld, num_str, 10, false);
                        strcpy(out, num_str);
                        out += strlen(num_str);
                    }
                    else if (*(traverse + 1) == 'u') {
                        traverse++;
                        uint64_t llu = va_arg(args, unsigned long long);
                        uitoa64(llu, num_str, 10, false);
                        strcpy(out, num_str);
                        out += strlen(num_str);
                    }
                    else {
                        *out++ = '%';
                        *out++ = 'l';
                        *out++ = 'l';
                    }
                }
                else {
                    *out++ = '%';
                    *out++ = 'l';
                }
                break;
            case 'f':  // Hexadecimal (uppercase)
                f = va_arg(args, double);
                if (precision == -1) precision = 6;  // Default precision is 6
                ftoa(f, num_str, precision);
                strcpy(out, num_str);
                out += strlen(num_str);
                break;
            default:
                *out++ = '%';        // Include the '%'
                *out++ = *traverse;  // and the following character as they are
                break;
        }
    }

    *out = '\0';       // Null-terminate the string
    return out - str;  // Number of characters written
}

// Implementation of a simplified sprintf
size_t sprintf(char* str, const char* format, ...) {
    va_list ap;
    va_start(ap, format);
    size_t written = vsprintf(str, format, ap);
    va_end(ap);
    return written;
}

// Implementation of a simplified vsnprintf
size_t vsnprintf(char* str, size_t size, const char* format, va_list ap) {
    char buffer[1024] = {0};

    size_t written    = vsprintf(buffer, format, ap);

    if (written > size) written = size;  // Truncate if necessary
    memcpy(str, buffer, written);
    str[written] = '\0';  // Ensure null termination

    return written;
}

// Implementation of a simplified snprintf
size_t snprintf(char* str, size_t size, const char* format, ...) {
    va_list ap;
    va_start(ap, format);
    size_t written = vsnprintf(str, size, format, ap);
    va_end(ap);
    return written;
}
