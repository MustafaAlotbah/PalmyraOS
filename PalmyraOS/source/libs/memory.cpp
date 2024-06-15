#include "libs/memory.h"


void* memchr(void* ptr, uint8_t value, size_t num) {
	auto* p = (unsigned char*) ptr;
	for (size_t i = 0; i < num; i++) {
		if (p[i] == value) {
			return &p[i];
		}
	}
	return nullptr;
}

int memcmp(const void* ptr1, const void* ptr2, size_t num) {
	const auto* p1 = (const unsigned char*) ptr1;
	const auto* p2 = (const unsigned char*) ptr2;
	for (size_t i = 0; i < num; i++) {
		if (p1[i] != p2[i]) {
			return p1[i] - p2[i];
		}
	}
	return 0;
}

void* memcpy(void* destination, void* source, size_t num) {
	char* dest = (char*) destination;
	const char* src = (const char*) source;
	for (size_t i = 0; i < num; i++) {
		dest[i] = src[i];
	}
	return destination;
}

extern "C" uint32_t* memcpy(uint32_t* destination, const uint32_t* source, uint32_t num)
{
	for (size_t i = 0; i < num; i++) {
		destination[i] = source[i];
	}
	return destination;
}

void *memset(void* ptr, uint8_t value, size_t num) {
	auto* p = (unsigned char*) ptr;
	for (size_t i = 0; i < num; i++) {
		p[i] = value;
	}
	return ptr;
}

extern "C" void* memmove(void* dest, const void* src, size_t n)
{
	auto* d = (unsigned char*) dest;
	const auto* s = (const unsigned char*) src;
	if (d < s) {
		for (size_t i = 0; i < n; i++) {
			d[i] = s[i];
		}
	} else {
		for (size_t i = n; i != 0; i--) {
			d[i - 1] = s[i - 1];
		}
	}
	return dest;
}