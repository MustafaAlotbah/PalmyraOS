
#pragma once

#include <cstdint>
#include <cstddef>

#include "libs/stdlib.h"


void* malloc(size_t size);

void free(void* ptr);

// TODO void* calloc(size_t num, size_t size);

// TODO void* realloc(void* ptr, size_t size);
