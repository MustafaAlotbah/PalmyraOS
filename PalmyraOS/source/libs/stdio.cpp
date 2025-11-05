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

// ============================================================================
// SCANF FAMILY IMPLEMENTATION
// ============================================================================

// Helper function: Skip whitespace in input string
static const char* skip_whitespace(const char* str) {
    if (!str) return str;
    while (*str && (*str == ' ' || *str == '\t' || *str == '\n' || *str == '\r')) { str++; }
    return str;
}

// Helper function: Parse integer from string (handles negative numbers)
static bool parse_int(const char** str_ptr, int* result) {
    if (!str_ptr || !*str_ptr || !result) return false;

    const char* str = skip_whitespace(*str_ptr);
    if (!*str) return false;

    bool negative = false;
    if (*str == '-') {
        negative = true;
        str++;
    }
    else if (*str == '+') { str++; }

    if (!(*str >= '0' && *str <= '9')) return false;

    int value = 0;
    while (*str >= '0' && *str <= '9') {
        value = value * 10 + (*str - '0');
        str++;
    }

    *result  = negative ? -value : value;
    *str_ptr = str;
    return true;
}

// Helper function: Parse unsigned integer from string
static bool parse_uint(const char** str_ptr, unsigned int* result) {
    if (!str_ptr || !*str_ptr || !result) return false;

    const char* str = skip_whitespace(*str_ptr);
    if (!*str) return false;

    if (!(*str >= '0' && *str <= '9')) return false;

    unsigned int value = 0;
    while (*str >= '0' && *str <= '9') {
        value = value * 10 + (*str - '0');
        str++;
    }

    *result  = value;
    *str_ptr = str;
    return true;
}

// Helper function: Parse hex number from string
static bool parse_hex(const char** str_ptr, unsigned int* result) {
    if (!str_ptr || !*str_ptr || !result) return false;

    const char* str = skip_whitespace(*str_ptr);

    // Handle optional "0x" prefix
    if (*str == '0' && (*(str + 1) == 'x' || *(str + 1) == 'X')) { str += 2; }

    if (!*str) return false;

    // Check if character is valid hex
    auto is_hex_digit = [](char c) { return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F'); };

    auto hex_to_int   = [](char c) {
        if (c >= '0' && c <= '9') return c - '0';
        if (c >= 'a' && c <= 'f') return c - 'a' + 10;
        if (c >= 'A' && c <= 'F') return c - 'A' + 10;
        return 0;
    };

    if (!is_hex_digit(*str)) return false;

    unsigned int value = 0;
    while (*str && is_hex_digit(*str)) {
        value = value * 16 + hex_to_int(*str);
        str++;
    }

    *result  = value;
    *str_ptr = str;
    return true;
}

// Helper function: Parse binary number from string
static bool parse_binary(const char** str_ptr, unsigned int* result) {
    if (!str_ptr || !*str_ptr || !result) return false;

    const char* str = skip_whitespace(*str_ptr);
    if (!*str || (*str != '0' && *str != '1')) return false;

    unsigned int value = 0;
    while (*str && (*str == '0' || *str == '1')) {
        value = value * 2 + (*str - '0');
        str++;
    }

    *result  = value;
    *str_ptr = str;
    return true;
}

// Helper function: Parse floating point from string
static bool parse_float(const char** str_ptr, double* result) {
    if (!str_ptr || !*str_ptr || !result) return false;

    const char* str = skip_whitespace(*str_ptr);
    if (!*str) return false;

    bool negative = false;
    if (*str == '-') {
        negative = true;
        str++;
    }
    else if (*str == '+') { str++; }

    if (!(*str >= '0' && *str <= '9') && *str != '.') return false;

    double value         = 0.0;
    double decimal_place = 1.0;
    bool seen_decimal    = false;

    while (*str) {
        if (*str == '.') {
            if (seen_decimal) break;
            seen_decimal = true;
            str++;
        }
        else if (*str >= '0' && *str <= '9') {
            if (seen_decimal) {
                decimal_place *= 0.1;
                value += (double) (*str - '0') * decimal_place;
            }
            else { value = value * 10.0 + (double) (*str - '0'); }
            str++;
        }
        else { break; }
    }

    *result  = negative ? -value : value;
    *str_ptr = str;
    return true;
}
/**
 * Parses a formatted string and extracts values into pointers.
 *
 * @param str Input string to parse
 * @param format Format string with embedded format specifiers
 * @param args Variable argument list (va_list) with pointers to store parsed values
 *
 * @return Number of successfully parsed and assigned items, or EOF on error
 *
 * Supported Format Specifiers:
 * ===========================
 *
 * %d          - Signed integer (int*)
 *               Example: sscanf("42", "%d", &i) sets i=42
 *
 * %u          - Unsigned integer (unsigned int*)
 *               Example: sscanf("100", "%u", &u) sets u=100
 *
 * %x / %X     - Hexadecimal unsigned (unsigned int*)
 *               Example: sscanf("FF", "%x", &h) sets h=255
 *
 * %b          - Binary (unsigned int*)
 *               Example: sscanf("1010", "%b", &b) sets b=10
 *
 * %f          - Floating point (double*)
 *               Example: sscanf("3.14", "%f", &f) sets f=3.14
 *
 * %s          - String - REQUIRES width specifier for safety (char*)
 *               Example: sscanf("hello", "%5s", buf) copies "hello" (max 5 chars)
 *               WARNING: %s without width is UNSAFE - will cause buffer overflow!
 *
 * %c          - Single character (char*)
 *               Example: sscanf("A", "%c", &c) sets c='A'
 *
 * %p          - Pointer address in hexadecimal (unsigned int* or void**)
 *               Example: sscanf("0x1000", "%p", &ptr)
 *
 * %lu / %ld   - Unsigned/signed long (unsigned long*, long*)
 *
 * %llu / %lld - Unsigned/signed long long (unsigned long long*, long long*)
 *
 * %zu         - Size type unsigned (size_t*)
 *
 * %%          - Literal percent sign (no argument consumed)
 *
 * Width Specifier (IMPORTANT for strings):
 * =======================================
 * %Ns         - Maximum N characters to read into buffer
 *               Example: sscanf("hello world", "%5s", buf)
 *               Reads "hello" (stops at space or after 5 chars, whichever comes first)
 *               This prevents buffer overflow!
 *
 * Whitespace Handling:
 * ===================
 * - Whitespace in format string is skipped in input
 * - Leading whitespace before numeric values is automatically skipped
 * - %s stops at whitespace unless width specified
 *
 * Safety Features:
 * ===============
 * - Returns count of successfully assigned items
 * - Safe pointer validation
 * - Width specifiers prevent buffer overflow for %s
 * - Type-safe conversions with error detection
 *
 * @note String format (%s) MUST use width specifier to prevent buffer overflow!
 * @note All pointers must be valid and correctly typed
 */
size_t vsscanf(const char* str, const char* format, va_list args) {
    if (!str || !format) return 0;

    size_t items_assigned = 0;
    const char* input     = str;

    for (const char* fmt = format; *fmt; fmt++) {
        if (*fmt != '%') {
            // Literal character - must match in input
            char input_char = *input;
            char fmt_char   = *fmt;

            // Skip whitespace in input if format has whitespace
            if (fmt_char == ' ' || fmt_char == '\t' || fmt_char == '\n') {
                input = skip_whitespace(input);
                fmt++;
                while (*fmt && (*fmt == ' ' || *fmt == '\t' || *fmt == '\n')) fmt++;
                fmt--;  // Back up one since loop will increment
                continue;
            }

            if (!*input || *input != fmt_char) return items_assigned;
            input++;
            continue;
        }

        fmt++;  // Move past '%'

        if (*fmt == '%') {
            // Escaped percent
            if (*input != '%') return items_assigned;
            input++;
            continue;
        }

        // Parse width specifier
        int width = -1;
        if (*fmt >= '0' && *fmt <= '9') {
            width = 0;
            while (*fmt >= '0' && *fmt <= '9') {
                width = width * 10 + (*fmt - '0');
                fmt++;
            }
        }

        // Skip leading whitespace
        input = skip_whitespace(input);

        switch (*fmt) {
            case 'd': {  // Signed integer
                int* ptr = va_arg(args, int*);
                if (!ptr) return items_assigned;
                if (!parse_int(&input, ptr)) return items_assigned;
                items_assigned++;
                break;
            }
            case 'u': {  // Unsigned integer
                unsigned int* ptr = va_arg(args, unsigned int*);
                if (!ptr) return items_assigned;
                if (!parse_uint(&input, ptr)) return items_assigned;
                items_assigned++;
                break;
            }
            case 'x':
            case 'X': {  // Hexadecimal
                unsigned int* ptr = va_arg(args, unsigned int*);
                if (!ptr) return items_assigned;
                if (!parse_hex(&input, ptr)) return items_assigned;
                items_assigned++;
                break;
            }
            case 'b': {  // Binary
                unsigned int* ptr = va_arg(args, unsigned int*);
                if (!ptr) return items_assigned;
                if (!parse_binary(&input, ptr)) return items_assigned;
                items_assigned++;
                break;
            }
            case 'f': {  // Floating point
                double* ptr = va_arg(args, double*);
                if (!ptr) return items_assigned;
                if (!parse_float(&input, ptr)) return items_assigned;
                items_assigned++;
                break;
            }
            case 's': {  // String (MUST have width for safety!)
                char* ptr = va_arg(args, char*);
                if (!ptr) return items_assigned;
                if (width <= 0) {
                    // UNSAFE: no width specified for %s
                    return items_assigned;  // Fail safely
                }

                int chars_read = 0;
                while (*input && chars_read < width - 1 && !isspace((unsigned char) *input)) {
                    *ptr++ = *input++;
                    chars_read++;
                }
                *ptr = '\0';

                if (chars_read == 0) return items_assigned;
                items_assigned++;
                break;
            }
            case 'c': {  // Character
                char* ptr = va_arg(args, char*);
                if (!ptr) return items_assigned;
                if (!*input) return items_assigned;
                *ptr = *input++;
                items_assigned++;
                break;
            }
            case 'p': {  // Pointer (hex format)
                void** ptr = va_arg(args, void**);
                if (!ptr) return items_assigned;
                unsigned int addr = 0;
                if (!parse_hex(&input, &addr)) return items_assigned;
                *ptr = (void*) (uintptr_t) addr;
                items_assigned++;
                break;
            }
            case 'l':  // Long variants: %ld, %lu, %lld, %llu
                if (*(fmt + 1) == 'd') {
                    fmt++;
                    long* ptr = va_arg(args, long*);
                    if (!ptr) return items_assigned;
                    int temp = 0;
                    if (!parse_int(&input, &temp)) return items_assigned;
                    *ptr = (long) temp;
                    items_assigned++;
                }
                else if (*(fmt + 1) == 'u') {
                    fmt++;
                    unsigned long* ptr = va_arg(args, unsigned long*);
                    if (!ptr) return items_assigned;
                    unsigned int temp = 0;
                    if (!parse_uint(&input, &temp)) return items_assigned;
                    *ptr = (unsigned long) temp;
                    items_assigned++;
                }
                else if (*(fmt + 1) == 'l') {
                    fmt++;
                    if (*(fmt + 1) == 'd') {
                        fmt++;
                        long long* ptr = va_arg(args, long long*);
                        if (!ptr) return items_assigned;
                        int temp = 0;
                        if (!parse_int(&input, &temp)) return items_assigned;
                        *ptr = (long long) temp;
                        items_assigned++;
                    }
                    else if (*(fmt + 1) == 'u') {
                        fmt++;
                        unsigned long long* ptr = va_arg(args, unsigned long long*);
                        if (!ptr) return items_assigned;
                        unsigned int temp = 0;
                        if (!parse_uint(&input, &temp)) return items_assigned;
                        *ptr = (unsigned long long) temp;
                        items_assigned++;
                    }
                }
                break;
            case 'z':  // size_t
                if (*(fmt + 1) == 'u') {
                    fmt++;
                    size_t* ptr = va_arg(args, size_t*);
                    if (!ptr) return items_assigned;
                    unsigned int temp = 0;
                    if (!parse_uint(&input, &temp)) return items_assigned;
                    *ptr = (size_t) temp;
                    items_assigned++;
                }
                break;
            default: return items_assigned;
        }
    }

    return items_assigned;
}

// Wrapper for sscanf
size_t sscanf(const char* str, const char* format, ...) {
    va_list args;
    va_start(args, format);
    size_t result = vsscanf(str, format, args);
    va_end(args);
    return result;
}
