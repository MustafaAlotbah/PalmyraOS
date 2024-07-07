
#pragma once


#include <utility>
#include <map>
#include <vector>

#include "core/definitions.h"
#include "core/kernel.h"
#include "core/panic.h"

// for type definitions
#include "libs/string.h"


namespace PalmyraOS::kernel
{

  /**
   * @brief Custom allocator using a heap manager.
   *
   * This class defines a custom allocator that uses a provided heap manager for memory allocation.
   *
   * @tparam T Type of elements to allocate.
   */
  template<typename T>
  class HeapAllocator
  {
   public:
	  using value_type = T;
	  using pointer = T*;
	  using const_pointer = const T*;
	  using reference = T&;
	  using const_reference = const T&;
	  using size_type = size_t;
	  using difference_type = std::ptrdiff_t;

	  /**
	   * @brief Constructor with heap manager.
	   *
	   * @param heap The heap manager to use for allocation.
	   */
	  explicit HeapAllocator(HeapManager& heap) : heapManager_(heap)
	  {}

	  /**
	   * @brief Rebind allocator to another type.
	   *
	   * @tparam U The new type for the allocator.
	   */
	  template<typename U>
	  struct rebind
	  {
		  using other = HeapAllocator<U>;
	  };

	  /**
	   * @brief Copy constructor for rebinded allocator.
	   *
	   * @tparam U The new type for the allocator.
	   */
	  template<typename U>
	  explicit HeapAllocator(const HeapAllocator<U>& other) noexcept : heapManager_(other.heapManager_)
	  {}

	  /**
	   * @brief Allocate memory for n elements.
	   *
	   * @param n Number of elements to allocate.
	   * @return Pointer to the allocated memory.
	   */
	  pointer allocate(size_type n)
	  {
		  auto p = static_cast<pointer>(heapManager_.alloc(n * sizeof(T)));
		  if (p == nullptr) kernel::kernelPanic("Heap Allocator Error::allocate p=nullptr!");
		  return p;
	  }

	  /**
	   * @brief Deallocate memory.
	   *
	   * @param p Pointer to the memory to deallocate.
	   * @param n Number of elements to deallocate.
	   */
	  void deallocate(pointer p, size_type n)
	  {
		  heapManager_.free(p);
	  }

	  /**
	   * @brief Construct an element in place.
	   *
	   * @tparam U Type of the element to construct.
	   * @tparam Args Types of constructor arguments.
	   * @param p Pointer to the location where the element should be constructed.
	   * @param args Arguments for the constructor.
	   */
	  template<typename U, typename... Args>
	  void construct(U* p, Args&& ... args)
	  {
		  if (p == nullptr) kernel::kernelPanic("Heap Allocator Error::construct p=nullptr!");
		  ::new(static_cast<void*>(p)) U(std::forward<Args>(args)...);
	  }

	  /**
	   * @brief Destroy an element in place.
	   *
	   * @tparam U Type of the element to destroy.
	   * @param p Pointer to the element to destroy.
	   */
	  template<typename U>
	  void destroy(U* p)
	  {
		  if (p == nullptr) kernel::kernelPanic("Heap Allocator Error::destroy p=nullptr!");
		  p->~U();
	  }

	  /**
	   * @brief Equality operator.
	   *
	   * @param other Another allocator to compare with.
	   * @return True if allocators are equal, false otherwise.
	   */
	  bool operator==(const HeapAllocator& other) const
	  {
		  return &heapManager_ == &other.heapManager_;
	  }

	  /**
	   * @brief Inequality operator.
	   *
	   * @param other Another allocator to compare with.
	   * @return True if allocators are not equal, false otherwise.
	   */
	  bool operator!=(const HeapAllocator& other) const
	  {
		  return *this != other;
	  }

   public:
	  HeapManager& heapManager_;     ///< Reference to the heap manager.
  };

  /**
   * @brief Custom allocator using the kernel's heap manager.
   *
   * This class extends the HeapAllocator to use the kernel's heap manager for memory allocation.
   *
   * @tparam T Type of elements to allocate.
   */
  template<typename T>
  class KernelHeapAllocator : public HeapAllocator<T>
  {
   public:
	  KernelHeapAllocator() : HeapAllocator<T>(kernel::heapManager)
	  {}

	  template<typename U>
	  struct rebind
	  {
		  using other = KernelHeapAllocator<U>;
	  };
  };


  /* Type Definitions*/
  /**
   * @typedef Kernel String
   * @brief A type definition for a string using the KernelHeapAllocator.
   */
  using KString = PalmyraOS::types::string<char, KernelHeapAllocator>;


  /**
   * @typedef Kernel Map
   * @brief A type definition for a map using the KernelHeapAllocator.
   *
   * @tparam KeyT Type of the key in the map.
   * @tparam ValT Type of the value in the map.
   */
  template<typename KeyT, typename ValT>
  using KMap = std::map<KeyT, ValT, std::less<KeyT>, KernelHeapAllocator<std::pair<const KeyT, ValT>>>;

  /**
   * @typedef Kernel Vector
   * @brief A type definition for a vector using the KernelHeapAllocator.
   *
   * @tparam Type Type of the elements in the vector.
   */
  template<typename Type>
  using KVector = std::vector<Type, KernelHeapAllocator<Type>>;

}