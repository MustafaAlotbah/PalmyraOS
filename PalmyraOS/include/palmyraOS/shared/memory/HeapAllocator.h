
#pragma once


#include "core/definitions.h"
#include "libs/string.h"
#include "palmyraOS/shared/memory/Heap.h"
#include <vector>


namespace PalmyraOS::types {

    class UserHeapManager : public types::HeapManagerBase {
    public:
        ~UserHeapManager();
        void* allocateMemory(size_t size) final;
        void freePage(void* address) final;
    };

    /**
     * @brief Custom allocator using a heap manager.
     *
     * This class defines a custom allocator that uses a provided heap manager for memory allocation.
     *
     * @tparam T Type of elements to allocate.
     */
    template<typename T>
    class HeapAllocator {
    public:
        using value_type      = T;
        using pointer         = T*;
        using const_pointer   = const T*;
        using reference       = T&;
        using const_reference = const T&;
        using size_type       = size_t;
        using difference_type = std::ptrdiff_t;

        /**
         * @brief Constructor with heap manager.
         *
         * @param heap The heap manager to use for allocation.
         */
        explicit HeapAllocator(types::HeapManagerBase& heap) : heapManager_(heap) {}

        /**
         * @brief Rebind allocator to another type.
         *
         * @tparam U The new type for the allocator.
         */
        template<typename U>
        struct rebind {
            using other = HeapAllocator<U>;
        };

        /**
         * @brief Copy constructor for rebinded allocator.
         *
         * @tparam U The new type for the allocator.
         */
        template<typename U>
        explicit HeapAllocator(const HeapAllocator<U>& other) noexcept : heapManager_(other.heapManager_) {}

        /**
         * @brief Allocate memory for n elements.
         *
         * @param n Number of elements to allocate.
         * @return Pointer to the allocated memory.
         */
        pointer allocate(size_type n) {
            auto p = static_cast<pointer>(heapManager_.alloc(n * sizeof(T)));
            //		  if (p == nullptr) kernel::kernelPanic("Heap Allocator Error::allocate p=nullptr!");
            return p;
        }

        /**
         * @brief Deallocate memory.
         *
         * @param p Pointer to the memory to deallocate.
         * @param n Number of elements to deallocate.
         */
        void deallocate(pointer p, size_type n) { heapManager_.free(p); }

        /**
         * @brief Construct an element in place.
         *
         * @tparam U Type of the element to construct.
         * @tparam Args Types of constructor arguments.
         * @param p Pointer to the location where the element should be constructed.
         * @param args Arguments for the constructor.
         */
        template<typename U, typename... Args>
        void construct(U* p, Args&&... args) {
            //		  if (p == nullptr) kernel::kernelPanic("Heap Allocator Error::construct p=nullptr!");
            ::new (static_cast<void*>(p)) U(std::forward<Args>(args)...);
        }

        /**
         * @brief Destroy an element in place.
         *
         * @tparam U Type of the element to destroy.
         * @param p Pointer to the element to destroy.
         */
        template<typename U>
        void destroy(U* p) {
            if (p == nullptr)
                ;
            p->~U();
        }

        /**
         * @brief Equality operator.
         *
         * @param other Another allocator to compare with.
         * @return True if allocators are equal, false otherwise.
         */
        bool operator==(const HeapAllocator& other) const { return &heapManager_ == &other.heapManager_; }

        /**
         * @brief Inequality operator.
         *
         * @param other Another allocator to compare with.
         * @return True if allocators are not equal, false otherwise.
         */
        bool operator!=(const HeapAllocator& other) const { return *this != other; }

    public:
        types::HeapManagerBase& heapManager_;  ///< Reference to the heap manager.
    };

    // User Vector
    template<typename T, typename Alloc = HeapAllocator<T>>
    class UVector : public std::vector<T, Alloc> {
        using Base = std::vector<T, Alloc>;

    public:
        explicit UVector(UserHeapManager& heap) : Base(Alloc(heap)) {}

        UVector(UserHeapManager& heap, size_t count) : Base(count, Alloc(heap)) {}

        UVector(UserHeapManager& heap, size_t count, const T& value) : Base(count, value, Alloc(heap)) {}

        template<typename InputIt>
        UVector(UserHeapManager& heap, InputIt first, InputIt last) : Base(first, last, Alloc(heap)) {}

        UVector(UserHeapManager& heap, std::initializer_list<T> init) : Base(init, Alloc(heap)) {}
    };

    // User String
    template<typename T>
    class UString : public types::string<T, HeapAllocator> {
        using Base = types::string<T, HeapAllocator>;

    public:
        using Base::operator=;  // Bring assignment operators into scope
        using Base::split;

        explicit UString(UserHeapManager& heap) : Base(HeapAllocator<T>(heap)) {}

        explicit UString(UserHeapManager& heap, const char* initialValue) : Base(HeapAllocator<T>(heap)) { *this = initialValue; }

        UString(UserHeapManager& heap, size_t count) : Base(count, HeapAllocator<T>(heap)) {}

        UString(UserHeapManager& heap, size_t count, const T& value) : Base(count, value, HeapAllocator<T>(heap)) {}

        template<typename InputIt>
        UString(UserHeapManager& heap, InputIt first, InputIt last) : Base(first, last, HeapAllocator<T>(heap)) {}

        UString(UserHeapManager& heap, std::initializer_list<T> init) : Base(init, HeapAllocator<T>(heap)) {}

        void setHeapManager(UserHeapManager& heap) { this->allocator = HeapAllocator<T>(heap); }
    };


}  // namespace PalmyraOS::types