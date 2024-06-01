
#pragma once

#include <cstdint>


#define REMOVE_COPY(cls) cls(const cls&) = delete; cls& operator=(const cls&) = delete;

typedef uint32_t size_t;
