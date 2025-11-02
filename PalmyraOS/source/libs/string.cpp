#include "libs/string.h"
#include "libs/ctype.h"


extern "C" size_t strlen(const char* str) {
    const char* s;
    for (s = str; *s; ++s)
        ;
    return s - str;
}

char* strtok(char* s, const char* delim) {
    static char* last = nullptr;
    if (s == nullptr) { s = last; }
    if (s == nullptr) { return nullptr; }

    // Skip leading delimiters
    int ch;
    while ((ch = *s) && strchr(delim, ch)) { s++; }

    if (*s == '\0') {
        last = nullptr;
        return nullptr;
    }

    // Find the end of the token
    char* token_start = s;
    while ((ch = *s) && !strchr(delim, ch)) { s++; }

    if (*s) {
        *s   = '\0';
        last = s + 1;
    }
    else { last = nullptr; }

    return token_start;
}

char* strchr(const char* s, int c) {
    for (; *s != (char) c; s++) {
        if (*s == '\0') { return NULL; }
    }
    return (char*) s;
}

char* strsep(char** stringp, const char* delim) {
    if (!stringp || !*stringp) { return NULL; }
    char *start = *stringp, *end;
    while ((end = strpbrk(start, delim)) != NULL) {
        *end     = '\0';
        *stringp = end + 1;
        if (start != end) {  // Non-empty token.
            return start;
        }
        start = *stringp;  // Skip empty token.
    }
    *stringp = NULL;  // No more delimiters; this was the last token.
    return start;     // Return whatever remains.
}

int strcmp(const char* s1, const char* s2) {
    while (*s1 && (*s1 == *s2)) {
        s1++;
        s2++;
    }
    return *(const unsigned char*) s1 - *(const unsigned char*) s2;
}

int strcmp(const wchar_t* s1, const wchar_t* s2) {
    while (*s1 && (*s1 == *s2)) {
        s1++;
        s2++;
    }
    return *(const wchar_t*) s1 - *(const wchar_t*) s2;
}

int strncmp(const char* s1, const char* s2, size_t n) {
    if (n == 0) return 0;
    do {
        if (*s1 != *s2++) { return *(const unsigned char*) s1 - *(const unsigned char*) (s2 - 1); }
        if (*s1++ == '\0') { break; }
    } while (--n != 0);
    return 0;
}

int strcasecmp(const char* s1, const char* s2) {
    const auto* us1 = (const unsigned char*) s1;
    const auto* us2 = (const unsigned char*) s2;

    while (tolower(*us1) == tolower(*us2++)) {
        if (*us1++ == '\0') { return 0; }
    }
    return tolower(*us1) - tolower(*--us2);
}

int strncasecmp(const char* s1, const char* s2, size_t n) {
    if (n == 0) return 0;

    const auto* us1 = (const unsigned char*) s1;
    const auto* us2 = (const unsigned char*) s2;

    do {
        if (tolower(*us1) != tolower(*us2++)) { return tolower(*us1) - tolower(*--us2); }
        if (*us1++ == '\0') break;
    } while (--n != 0);
    return 0;
}

char* strcpy(char* dest, const char* src) {
    char* ret = dest;
    while ((*dest++ = *src++))
        ;
    return ret;
}

char* strncpy(char* dest, const char* src, size_t n) {
    char* ret = dest;
    do {
        if (!n--) return ret;
    } while ((*dest++ = *src++));
    while (n--) *dest++ = '\0';
    return ret;
}

char* strcat(char* dest, const char* src) {
    char* ret = dest;
    while (*dest) dest++;
    while ((*dest++ = *src++))
        ;
    return ret;
}

char* strpbrk(const char* s, const char* accept) {
    const char* a;
    for (; *s; s++) {                // Traverse every character in `s`
        for (a = accept; *a; a++) {  // Traverse every character in `accept`
            if (*s == *a) {
                return (char*) s;  // Return the pointer to the first matching character
            }
        }
    }
    return nullptr;  // If no match is found
}