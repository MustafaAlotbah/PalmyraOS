
#pragma once

#include <cstddef>  // size_t
#include <cstdint>  // uint32_t

namespace PalmyraOS::kernel {

    /**
     * @class Descriptor
     * @brief Abstract base class for all file descriptors (files, pipes, sockets, etc.)
     *
     * Provides a uniform interface for I/O operations on different descriptor types.
     * Concrete implementations (FileDescriptor, PipeDescriptor, SocketDescriptor)
     * derive from this class and implement type-specific behavior.
     *
     * Design follows POSIX semantics:
     * - Each descriptor has a Kind that can be queried at runtime
     * - Operations that don't make sense for a type return appropriate errors
     * - Memory management: descriptors are heap-allocated and owned by DescriptorTable
     */
    class Descriptor {
    public:
        /**
         * @enum Kind
         * @brief Identifies the type of descriptor
         */
        enum class Kind : uint8_t {
            File,   ///< Regular file or directory (seekable)
            Pipe,   ///< Pipe (not seekable, unidirectional)
            Socket  ///< Network socket (not seekable, bidirectional)
        };

        /**
         * @brief Get the type of this descriptor
         * @return The descriptor kind (File, Pipe, or Socket)
         */
        [[nodiscard]] virtual Kind kind() const               = 0;

        /**
         * @brief Read data from this descriptor
         * @param buffer Destination buffer
         * @param size Number of bytes to read
         * @return Number of bytes actually read, or negative error code
         */
        virtual size_t read(char* buffer, size_t size)        = 0;

        /**
         * @brief Write data to this descriptor
         * @param buffer Source buffer
         * @param size Number of bytes to write
         * @return Number of bytes actually written, or negative error code
         */
        virtual size_t write(const char* buffer, size_t size) = 0;

        /**
         * @brief Perform device-specific control operation
         * @param request Operation code
         * @param arg Operation-specific argument
         * @return 0 on success, negative error code on failure
         *
         * Default implementation returns -ENOTTY (not a typewriter / invalid ioctl)
         */
        virtual int ioctl(int request, void* arg)             = 0;

        /**
         * @brief Virtual destructor to ensure proper cleanup
         */
        virtual ~Descriptor()                                 = default;

        // ==================== Memory Management (Freestanding C++) ====================

        /// @brief Custom operator new for freestanding environment (global heap allocation)
        static void* operator new(size_t size);

        /// @brief Placement new operator (used by createInstance in HeapManager)
        static void* operator new(size_t size, void* ptr) noexcept;

        /// @brief Custom operator delete for unsized deallocation (virtual destructors)
        static void operator delete(void* ptr) noexcept;

        /// @brief Custom operator delete for sized deallocation (compiler optimization)
        static void operator delete(void* ptr, size_t size) noexcept;

        // Prevent copying (descriptors should be unique per fd)
        Descriptor(const Descriptor&)            = delete;
        Descriptor& operator=(const Descriptor&) = delete;

    protected:
        // Protected constructor - only derived classes can instantiate
        Descriptor() = default;
    };

}  // namespace PalmyraOS::kernel
