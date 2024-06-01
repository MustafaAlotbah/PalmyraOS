#pragma once

/**
 * Checks whether the character `c` is alphanumeric (either a letter or a number).
 * @param c Character to be checked, represented as an integer.
 * @return Non-zero value if `c` is alphanumeric, zero otherwise.
 */
int isalnum(int c);

/**
 * Checks whether the character `c` is alphabetic.
 * @param c Character to be checked, represented as an integer.
 * @return Non-zero value if `c` is alphabetic, zero otherwise.
 */
int isalpha(int c);

/**
 * Checks whether the character `c` is a blank character, that is ' ' or '\t'.
 * @param c Character to be checked, represented as an integer.
 * @return Non-zero value if `c` is a blank character, zero otherwise.
 */
int isblank(int c);

/**
 * Checks whether the character `c` is a control character.
 * @param c Character to be checked, represented as an integer.
 * @return Non-zero value if `c` is a control character, zero otherwise.
 */
int iscntrl(int c);

/**
 * Checks whether the character `c` is a decimal digit character.
 * @param c Character to be checked, represented as an integer.
 * @return Non-zero value if `c` is a digit, zero otherwise.
 */
int isdigit(int c);

/**
 * Checks whether the character `c` has a graphical representation other than space.
 * @param c Character to be checked, represented as an integer.
 * @return Non-zero value if `c` is a graphical character, zero otherwise.
 */
int isgraph(int c);

/**
 * Checks whether the character `c` is a lowercase letter.
 * @param c Character to be checked, represented as an integer.
 * @return Non-zero value if `c` is lowercase, zero otherwise.
 */
int islower(int c);

/**
 * Checks whether the character `c` is printable, including space (' ').
 * @param c Character to be checked, represented as an integer.
 * @return Non-zero value if `c` is printable, zero otherwise.
 */
int isprint(int c);

/**
 * Checks whether the character `c` is a punctuation character.
 * @param c Character to be checked, represented as an integer.
 * @return Non-zero value if `c` is a punctuation character, zero otherwise.
 */
int ispunct(int c);

/**
 * Checks whether the character `c` is a white-space character.
 * @param c Character to be checked, represented as an integer.
 * @return Non-zero value if `c` is a space character, zero otherwise.
 */
int isspace(int c);

/**
 * Checks whether the character `c` is an uppercase letter.
 * @param c Character to be checked, represented as an integer.
 * @return Non-zero value if `c` is uppercase, zero otherwise.
 */
int isupper(int c);

/**
 * Checks whether the character `c` is a hexadecimal digit.
 * @param c Character to be checked, represented as an integer.
 * @return Non-zero value if `c` is a hexadecimal digit, zero otherwise.
 */
int isxdigit(int c);

/**
 * Converts the character `c` to lowercase if it is an uppercase letter.
 * @param c Character to be converted, represented as an integer.
 * @return Lowercase version of `c` if it is an uppercase letter, otherwise `c`.
 */
int tolower(int c);

/**
 * Converts the character `c` to uppercase if it is a lowercase letter.
 * @param c Character to be converted, represented as an integer.
 * @return Uppercase version of `c` if it is a lowercase letter, otherwise `c`.
 */
int toupper(int c);
