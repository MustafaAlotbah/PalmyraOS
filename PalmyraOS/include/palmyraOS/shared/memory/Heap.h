
#pragma once

#include <utility>
#include "core/definitions.h"


namespace PalmyraOS::types
{


  /**
   * @brief Represents a chunk of memory in the heap.
   *
   * A HeapChunk is a block of memory that can be allocated or freed.
   * It contains metadata about the size, allocation status, and neighboring chunks.
   */
  class HeapChunk
  {
   public:
	  /**
	   * @brief Splits the current chunk into two.
	   * @param size The size of the first chunk after the split.
	   */
	  void split(uint32_t size);

	  /**
	   * @brief Merges the current chunk with adjacent free chunks.
	   */
	  void tryMerge();

	  /**
	   * @brief Checks if the given chunk is physically contiguous with the current chunk.
	   * @param other Pointer to the other chunk.
	   * @return True if the chunks are contiguous, false otherwise.
	   */
	  bool isPhysicallyContiguous(HeapChunk* other);

   public:
	  uint32_t size_{ 0 };              // Size of the block.
	  bool     isAllocated_{ false };   // Free block if true.
	  HeapChunk* next_{ nullptr };      // Pointer to next header in the list of free blocks.
	  HeapChunk* prev_{ nullptr };      // Pointer to previous header in the list of free blocks.
  };

  /**
   * @brief Manages the heap memory allocation and de-allocation.
   *
   * The HeapManager handles the allocation and de-allocation of memory blocks
   * in the heap, as well as expanding and contracting the heap as needed.
   */
  class HeapManagerBase
  {
   public:
	  /**
	   * @brief Initializes the kernel heap.
	   */
	  HeapManagerBase() = default;

	  /**
	   * @brief Allocates a contiguous block of memory of the requested size.
	   * @param size The size of the memory block to allocate.
	   * @param page_align Whether to align the block to a page boundary.
	   * @return void* Pointer to the allocated memory block.
	   */
	  void* alloc(uint32_t size, bool page_align = false);

	  /**
	   * @brief Frees a block of memory.
	   * @param p Pointer to the memory block to free.
	   */
	  void free(void* p);

	  /**
	   * @brief Coalesces adjacent free blocks in the heap. TODO
	   */
	  void coalesceFreeBlocks();


	  template<class ClassName, class... Args>
	  constexpr ClassName* createInstance(Args&& ... args)
	  {
		  // Allocate memory for the instance
		  auto pointer = (ClassName*)alloc(sizeof(ClassName));

		  // Check if allocation was successful
		  // TODO Handle allocation failure, could be returning nullptr or throwing an exception
		  if (!pointer) return nullptr;

		  // Construct the instance in the allocated memory using placement new
		  return new(pointer) ClassName(std::forward<Args>(args)...);
	  }

   private:
	  /**
	   * @brief Requests more memory from the system.
	   * @param size The size of the additional memory to request.
	   * @return void* Pointer to the newly allocated memory.
	   */
	  void* requestMoreMemory(size_t size);

	  virtual void* allocateMemory(size_t numBytes) = 0;

	  virtual void freePage(void* address) = 0;


	  DEFINE_DEFAULT_MOVE(HeapManagerBase);
	  REMOVE_COPY(HeapManagerBase);
   public:

	  /**
	   * @brief Returns the total allocated memory.
	   * @return uint32_t Total allocated memory in bytes.
	   */
	  [[nodiscard]] inline uint32_t getTotalAllocatedMemory() const
	  { return totalAllocatedMemory_; }

	  /**
	   * @brief Returns the total free memory.
	   * @return uint32_t Total free memory in bytes.
	   */
	  [[nodiscard]] inline uint32_t getTotalFreeMemory() const
	  { return totalMemory_ - totalAllocatedMemory_; }

	  /**
	   * @brief Returns the total memory.
	   * @return uint32_t Total memory in bytes.
	   */
	  [[nodiscard]] inline uint32_t getTotalMemory() const
	  { return totalAllocatedMemory_; }

   private:
	  /**
	   * @brief Expands the heap to a new size.
	   * @param new_size The new size of the heap.
	   */
	  void expand(uint32_t new_size);

	  /**
	   * @brief Contracts the heap to a new size.
	   * @param new_size The new size of the heap.
	   * @return uint32_t The new total size of the heap.
	   */
	  uint32_t contract(uint32_t new_size);

	  /**
	   * @brief Finds the smallest free chunk that can fit the requested size.
	   * @param size The size of the requested memory block.
	   * @param page_align Whether to align the block to a page boundary.
	   * @return HeapChunk* Pointer to the found chunk or nullptr if no suitable chunk is found.
	   */
	  HeapChunk* findSmallestHole(uint32_t size, bool page_align);
   protected:
	  uint32_t totalMemory_{};          // Total size of all heap memory including overhead
	  uint32_t totalAllocatedMemory_{}; // Total size of allocated memory
	  HeapChunk* heapList_{};           // Pointer to the first block in the heap
  };


}













