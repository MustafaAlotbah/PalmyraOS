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
uint64_t __udivdi3(uint64_t dividend, uint64_t divisor)
{
	if (divisor == 0)
	{
		// Division by zero is undefined behavior, but we'll return 0 for simplicity.
		return 0;
	}

	uint64_t quotient  = 0;
	uint64_t remainder = 0;

	for (int i = 63; i >= 0; i--)
	{
		remainder <<= 1;
		remainder |= (dividend >> i) & 1;
		if (remainder >= divisor)
		{
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
uint64_t __umoddi3(uint64_t dividend, uint64_t divisor)
{
	if (divisor == 0)
	{
		// Modulus by zero is undefined behavior, but we'll return 0 for simplicity.
		return 0;
	}

	uint64_t quotient  = 0;
	uint64_t remainder = 0;

	for (int i = 63; i >= 0; i--)
	{
		remainder <<= 1;
		remainder |= (dividend >> i) & 1;
		if (remainder >= divisor)
		{
			remainder -= divisor;
			quotient |= (uint64_t)1 << i;
		}
	}

	return remainder;
}

/**
 * @brief Performs signed 64-bit integer division.
 *
 * This function divides a 64-bit signed integer by another 64-bit signed integer
 * and returns the quotient. It uses the __udivdi3 function for the actual division
 * and adjusts for the sign of the result.
 *
 * @param dividend The 64-bit signed integer to be divided.
 * @param divisor The 64-bit signed integer by which to divide.
 * @return The quotient of the division. If the divisor is 0, the function returns 0.
 */
int64_t __divdi3(int64_t dividend, int64_t divisor)
{
	if (divisor == 0)
	{
		// Division by zero is undefined behavior, but we'll return 0 for simplicity.
		return 0;
	}

	bool negative_result = (dividend < 0) ^ (divisor < 0);

	uint64_t abs_dividend = (dividend < 0) ? -dividend : dividend;
	uint64_t abs_divisor  = (divisor < 0) ? -divisor : divisor;

	uint64_t abs_quotient = __udivdi3(abs_dividend, abs_divisor);

	return negative_result ? -abs_quotient : abs_quotient;
}

/**
 * @brief Performs signed 64-bit integer modulus operation.
 *
 * This function divides a 64-bit signed integer by another 64-bit signed integer
 * and returns the remainder. It uses the __umoddi3 function for the actual modulus
 * operation and adjusts for the sign of the result.
 *
 * @param dividend The 64-bit signed integer to be divided.
 * @param divisor The 64-bit signed integer by which to divide.
 * @return The remainder of the division. If the divisor is 0, the function returns 0.
 */
int64_t __moddi3(int64_t dividend, int64_t divisor)
{
	if (divisor == 0)
	{
		// Modulus by zero is undefined behavior, but we'll return 0 for simplicity.
		return 0;
	}

	uint64_t abs_dividend = (dividend < 0) ? -dividend : dividend;
	uint64_t abs_divisor  = (divisor < 0) ? -divisor : divisor;

	uint64_t abs_remainder = __umoddi3(abs_dividend, abs_divisor);

	return (dividend < 0) ? -abs_remainder : abs_remainder;
}

/**
 * @brief Performs signed 64-bit integer division and modulus operation.
 *
 * This function divides a 64-bit signed integer by another 64-bit signed integer
 * and returns both the quotient and the remainder. It uses the __divdi3 and __moddi3
 * functions for the actual division and modulus operations.
 *
 * @param dividend The 64-bit signed integer to be divided.
 * @param divisor The 64-bit signed integer by which to divide.
 * @param remainder Pointer to store the remainder of the division.
 * @return The quotient of the division. The remainder is stored in the variable pointed to by the remainder parameter.
 */
int64_t __divmoddi4(int64_t dividend, int64_t divisor, int64_t* remainder)
{
	if (divisor == 0)
	{
		// Division by zero is undefined behavior, but we'll return 0 for simplicity.
		if (remainder)
		{
			*remainder = 0;
		}
		return 0;
	}

	int64_t quotient = __divdi3(dividend, divisor);
	if (remainder)
	{
		*remainder = __moddi3(dividend, divisor);
	}

	return quotient;
}

/**
 * @brief Performs unsigned 64-bit integer division and modulus operation.
 *
 * This function divides a 64-bit unsigned integer by another 64-bit unsigned integer
 * and returns both the quotient and the remainder. It uses the __udivdi3 and __umoddi3
 * functions for the actual division and modulus operations.
 *
 * @param dividend The 64-bit unsigned integer to be divided.
 * @param divisor The 64-bit unsigned integer by which to divide.
 * @param remainder Pointer to store the remainder of the division.
 * @return The quotient of the division. The remainder is stored in the variable pointed to by the remainder parameter.
 */
uint64_t __udivmoddi4(uint64_t dividend, uint64_t divisor, uint64_t* remainder)
{
	if (divisor == 0)
	{
		// Division by zero is undefined behavior, but we'll return 0 for simplicity.
		if (remainder)
		{
			*remainder = 0;
		}
		return 0;
	}

	uint64_t quotient = __udivdi3(dividend, divisor);
	if (remainder)
	{
		*remainder = __umoddi3(dividend, divisor);
	}

	return quotient;
}

}  // extern "C"