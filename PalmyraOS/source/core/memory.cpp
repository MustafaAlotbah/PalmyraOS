
#include "core/memory.h"


uint32_t* memcpy(uint32_t* dest, const uint32_t* src, size_t n)
{
	// Copy n bytes from src to dest
	for (size_t i = 0; i < n; ++i)
		dest[i] = src[i];

	return dest;
}
