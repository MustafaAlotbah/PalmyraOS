
#pragma once

#include "core/definitions.h"


// Read an unsigned 8-bit integer from data at the given offset
uint8_t get_uint8_t(const uint8_t* data, size_t offset);

// Read an unsigned 16-bit integer from data at the given offset
uint16_t get_uint16_t(const uint8_t* data, size_t offset);

// Read an unsigned 32-bit integer from data at the given offset
uint32_t get_uint32_t(const uint8_t* data, size_t offset);
