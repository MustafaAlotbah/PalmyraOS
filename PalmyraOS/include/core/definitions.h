
#pragma once

#include <cstddef>
#include <cstdint>

/**
 * @brief Macro to delete the copy constructor and copy assignment operator for a class.
 * This ensures the class cannot be copied.
 * @param cls The class name to apply the macro.
 */
#define REMOVE_COPY(cls)                                                                                                                                       \
    cls(const cls&)            = delete;                                                                                                                       \
    cls& operator=(const cls&) = delete;


/**
 * @brief Macro to define default move constructor and move assignment operator.
 * @param cls The class name to apply the macro.
 */
#define DEFINE_DEFAULT_MOVE(cls)                                                                                                                               \
    cls(cls&& other) noexcept            = default;                                                                                                            \
    cls& operator=(cls&& other) noexcept = default;


/**
 * @brief Macro to compute the ceiling of the division of a value by the page size.
 * This is useful for aligning values to page boundaries.
 * @param value The value to be divided.
 */
#define CEIL_DIV_PAGE_SIZE(value) (((value) + (1 << PAGE_BITS) - 1) >> PAGE_BITS)

/**
 * Typedef for size_t to ensure compatibility with the 32-bit system.
 */
typedef uint32_t size_t;

/**
 * Namespace for PalmyraOS constants.
 */
namespace PalmyraOS::Constants {
    /* Maximum buffer size for working with strings on the stack. */
    constexpr uint32_t MaximumStackBuffer = 1024;
}  // namespace PalmyraOS::Constants
