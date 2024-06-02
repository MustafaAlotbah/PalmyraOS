
#pragma once

#include <cstdint>
#include <cstddef>


#define REMOVE_COPY(cls) cls(const cls&) = delete; cls& operator=(const cls&) = delete;

typedef uint32_t size_t;

namespace PalmyraOS::Constants
{
  constexpr uint32_t MaximumStackBuffer = 1024;
}
