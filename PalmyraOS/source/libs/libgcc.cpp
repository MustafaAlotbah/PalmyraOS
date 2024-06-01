/**
 * @file libgcc.cpp
 * @brief Unsigned 64-bit division and modulus functions for the linker and C++ runtime.
 *
 */

#include <cstdint>

extern "C" {  // Ensure the symbols are exported as-is for linking

/**
* @brief Performs unsigned 64-bit integer division.
*
* This function divides a 64-bit unsigned integer by another 64-bit unsigned integer
* and returns the quotient. It uses a bitwise algorithm to compute the result.
*
* @param dividend The 64-bit unsigned integer to be divided.
* @param divisor The 64-bit unsigned integer by which to divide.
* @return The quotient of the division. If the divisor is 0, the function returns 0.
*/
uint64_t __udivdi3(uint64_t dividend, uint64_t divisor) {
    if (divisor == 0) {
        // Division by zero is undefined behavior, but we'll return 0 for simplicity.
        return 0;
    }

    uint64_t quotient = 0;
    uint64_t remainder = 0;
    for (int i = 63; i >= 0; i--) {
        remainder <<= 1;
        remainder |= (dividend >> i) & 1;
        if (remainder >= divisor) {
            remainder -= divisor;
            quotient |= (uint64_t)1 << i;
        }
    }

    return quotient;
}

/**
 * @brief Performs unsigned 64-bit integer modulus operation.
 *
 * This function divides a 64-bit unsigned integer by another 64-bit unsigned integer
 * and returns the remainder. It uses a bitwise algorithm to compute the result.
 *
 * @param dividend The 64-bit unsigned integer to be divided.
 * @param divisor The 64-bit unsigned integer by which to divide.
 * @return The remainder of the division. If the divisor is 0, the function returns 0.
 */
uint64_t __umoddi3(uint64_t dividend, uint64_t divisor) {
    if (divisor == 0) {
        // Modulus by zero is undefined behavior, but we'll return 0 for simplicity.
        return 0;
    }

    uint64_t quotient = 0;
    uint64_t remainder = 0;
    for (int i = 63; i >= 0; i--) {
        remainder <<= 1;
        remainder |= (dividend >> i) & 1;
        if (remainder >= divisor) {
            remainder -= divisor;
            quotient |= (uint64_t)1 << i;
        }
    }

    return remainder;
}

}  // extern "C"