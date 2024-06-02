
#pragma once

#include <cstdint>
#include <cstddef>

/**
 * Macro to delete the copy constructor and copy assignment operator for a class.
 * This ensures the class cannot be copied.
 */
#define REMOVE_COPY(cls) cls(const cls&) = delete; cls& operator=(const cls&) = delete;

/**
 * Typedef for size_t to ensure compatibility with the 32-bit system.
 */
typedef uint32_t size_t;

/**
 * Namespace for PalmyraOS constants.
 */
namespace PalmyraOS::Constants
{
  /* Maximum buffer size for working with strings on the stack. */
  constexpr uint32_t MaximumStackBuffer = 1024;
}
