#pragma once

/**
 * Converts the initial part of the string pointed to by `nptr` to a long integer value according to the given base,
 * which must be between 2 and 36 inclusive, or be the special value 0.
 * @param nptr Pointer to the null-terminated string to be interpreted.
 * @param endptr Pointer to a pointer to character. The address of the first invalid character is stored in the pointer that it points to.
 * @param base Base of the numerical representation used in the string.
 * @return The converted value as a long integer. If no valid conversion could be performed, it returns 0.
 */
long strtol(const char *nptr, char **endptr, int base);

/**
 * Converts the initial part of the string pointed to by `nptr` to an unsigned long integer value according to the given base,
 * which must be between 2 and 36 inclusive, or be the special value 0.
 * @param nptr Pointer to the null-terminated string to be interpreted.
 * @param endptr Pointer to a pointer to character. The address of the first invalid character is stored in the pointer that it points to.
 * @param base Base of the numerical representation used in the string.
 * @return The converted value as an unsigned long integer. If no valid conversion could be performed, it returns 0.
 */
unsigned long strtoul(const char *nptr, char **endptr, int base);

/**
 * Converts the string pointed to by `str` to an integer. This function assumes that the number is represented in decimal (base 10).
 * @param str Pointer to the null-terminated string to be interpreted.
 * @return The converted value as an integer. If no valid conversion could be performed, it returns 0.
 */
int atoi(const char *str);

/**
 * Converts an integer `num` to a string `str` representing the number expressed in base `base`.
 * If `upper_case` is true, uses uppercase letters; otherwise, uses lowercase.
 * @param num Integer to be converted.
 * @param str Pointer to the buffer where the converted string is stored.
 * @param base Base for the number representation, must be between 2 and 36.
 * @param upper_case Boolean indicating whether to use uppercase letters for bases greater than 10.
 */
void itoa(int num, char* str, int base, bool upper_case=false);



void reverse(char str[], int length);