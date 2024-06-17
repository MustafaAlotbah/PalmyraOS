
#pragma once

#include <cstdint>
#include <cstddef>

/**
 * @brief Macro to delete the copy constructor and copy assignment operator for a class.
 * This ensures the class cannot be copied.
 * @param cls The class name to apply the macro.
 */
#define REMOVE_COPY(cls) cls(const cls&) = delete; cls& operator=(const cls&) = delete;


/**
 * @brief Macro to define default move constructor and move assignment operator.
 * @param cls The class name to apply the macro.
 */
#define DEFINE_DEFAULT_MOVE(cls) cls(cls&& other) noexcept = default; cls& operator=(cls&& other) noexcept = default;

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
