#pragma once

#include "core/definitions.h"    // stdint + size_t

/**
 * Searches for the first occurrence of the character `value` within the first `num` bytes of the block of memory pointed by `ptr`.
 * @param ptr Pointer to the block of memory where the search is performed.
 * @param value Value to be located. The value is passed as an `uint8_t` but is internally converted to `unsigned char`.
 * @param num Number of bytes to be analyzed.
 * @return A pointer to the matching byte or NULL if the character does not occur in the given memory block.
 */
void* memchr(void* ptr, uint8_t value, size_t num);

/**
 * Compares the first `num` bytes of the block of memory pointed by `ptr1` to the first `num` bytes pointed by `ptr2`.
 * @param ptr1 Pointer to the block of memory.
 * @param ptr2 Pointer to the block of memory to be compared with.
 * @param num Number of bytes to compare.
 * @return An integer less than, equal to, or greater than zero if the first `num` bytes of `ptr1` are found, respectively,
 * to be less than, to match, or be greater than the first `num` bytes of `ptr2`.
 */
int memcmp(const void* ptr1, const void* ptr2, size_t num);

/**
 * Copies `num` bytes from the memory block pointed by `source` to the memory block pointed by `destination`.
 * @param destination Pointer to the destination array where the content is to be copied.
 * @param source Pointer to the source of data to be copied.
 * @param num Number of bytes to copy.
 * @return A pointer to `destination`.
 */
void* memcpy(void* destination, void* source, size_t num);

/**
 * Copies `num` double words from the memory block pointed by `source` to the memory block pointed by `destination`.
 * @param destination Pointer to the destination array where the content is to be copied.
 * @param source Pointer to the source of data to be copied.
 * @param num Number of double words to copy.
 * @return A pointer to `destination`.
 */
uint32_t* memcpy(uint32_t* destination, const uint32_t* source, uint32_t num);

/**
 * Fills the first `num` bytes of the memory area pointed to by `ptr` with the constant byte `value`.
 * @param ptr Pointer to the block of memory to fill.
 * @param value Value to be set. The value is passed as an `uint8_t` but is internally converted to `unsigned char`.
 * @param num Number of bytes to be set to the value.
 * @return A pointer to the memory area `ptr`.
 */
void *memset(void* ptr, uint8_t value, size_t num);

/**
 * Moves `n` bytes from the memory area `src` to the memory area `dest`. The memory areas may overlap: copying takes place as though
 * the bytes in `src` are first copied into a temporary array that does not overlap `src` or `dest`, and then copied to `dest`.
 * @param dest Pointer to the destination memory area.
 * @param src Pointer to the source memory area.
 * @param n Number of bytes to move.
 * @return A pointer to the destination memory area `dest`.
 */
void *memmove(void *dest, const void *src, size_t n);