

#include "libs/shared/memory/Heap.h"
//#include "core/memory/paging.h"

#define PAGE_SIZE 0x1000
#define PAGE_BITS 12


void PalmyraOS::types::HeapChunk::split(uint32_t size)
{
	// Ensure there is enough space for a new chunk
	if (size_ <= size + sizeof(HeapChunk)) return;

	// Create a new chunk at the address right after the allocated size in the current chunk
	auto* new_chunk = (HeapChunk*)((uintptr_t)this + sizeof(HeapChunk) + size);
	new_chunk->size_        =
		size_ - size - sizeof(HeapChunk);    // Set the size of the new chunk to the remaining size
	new_chunk->isAllocated_ = false;                            // Mark the new chunk as free
	new_chunk->next_        = next_;
	new_chunk->prev_        = this;

	// Attach the new chunk to the current chunk's next chunk
	if (next_) next_->prev_ = new_chunk;
	next_ = new_chunk;
	size_ = size;
}

void PalmyraOS::types::HeapChunk::tryMerge()
{
	// Merge with the next chunk if it is free and contiguous
	if (next_ && !next_->isAllocated_ && isPhysicallyContiguous(next_))
	{
		size_ += next_->size_ + sizeof(HeapChunk);
		next_ = next_->next_;
		if (next_) next_->prev_ = this;
	}

	// Merge with the previous chunk if it is free and contiguous
	if (prev_ && !prev_->isAllocated_ && prev_->isPhysicallyContiguous(this))
	{
		prev_->size_ += size_ + sizeof(HeapChunk);
		prev_->next_            = next_;
		if (next_) next_->prev_ = prev_;
	}
}

bool PalmyraOS::types::HeapChunk::isPhysicallyContiguous(PalmyraOS::types::HeapChunk* other)
{
	// Checks if the given chunk is physically contiguous with the current chunk
	return (uintptr_t)this + size_ + sizeof(HeapChunk) == (uintptr_t)other;
}

void* PalmyraOS::types::HeapManagerBase::alloc(uint32_t size, bool page_align)
{
	// Calculate the total size needed, including the chunk header
	uint32_t actualSize = size;

	// If page alignment is requested, align the size to the next page boundary
	if (page_align) actualSize = (actualSize + 0xFFF) & ~0xFFF;
	if (actualSize == 0) return nullptr;

	// Find the smallest free chunk that can fit the requested size
	HeapChunk* chunk = findSmallestHole(actualSize, page_align);

	// If no suitable chunk is found, request more memory
	if (!chunk)
	{
		requestMoreMemory(actualSize);

		// Try finding a suitable chunk again after requesting more memory
		chunk = findSmallestHole(actualSize, page_align);

		// If still no suitable chunk is found, return nullptr
		if (!chunk) return nullptr;
	}

	// Split the chunk to allocate the requested size and mark it as allocated
	chunk->split(actualSize);
	chunk->isAllocated_ = true;
	totalAllocatedMemory_ += chunk->size_;

	// Return a pointer to the memory right after the chunk header
	return (void*)((uintptr_t)chunk + sizeof(HeapChunk));
}

void* PalmyraOS::types::HeapManagerBase::requestMoreMemory(size_t size)
{
	// Align the requested size to the next page boundary
	size = (size + sizeof(types::HeapChunk) + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);

	// Allocate the required number of pages
	void* newMemory = allocateMemory(size);
	if (newMemory == nullptr) return nullptr; // Not enough Memory

	// Create a new chunk with the newly allocated memory
	auto* newChunk = (HeapChunk*)newMemory;
	newChunk->size_        = size - sizeof(HeapChunk);
	newChunk->isAllocated_ = false;
	newChunk->next_        = nullptr;
	newChunk->prev_        = nullptr;

	// If the heap list is empty, set the new chunk as the first chunk
	if (heapList_ == nullptr) heapList_ = newChunk;

		// Otherwise append it
	else
	{
		// Find the last chunk in the heap list
		HeapChunk* lastChunk = heapList_;
		while (lastChunk->next_ != nullptr) lastChunk = lastChunk->next_;

		// Append the new chunk to the end of the heap list
		lastChunk->next_ = newChunk;
		newChunk->prev_  = lastChunk;
	}

	// Update the heap end address and total memory size
	totalMemory_ += size;

	// Return the new chunk
	return newChunk;
}

void PalmyraOS::types::HeapManagerBase::free(void* p)
{
	if (!p) return;

	// Calculate the address of the chunk header
	auto* chunk = (HeapChunk*)((uintptr_t)p - sizeof(HeapChunk));
	chunk->isAllocated_ = false;
	totalAllocatedMemory_ -= chunk->size_;

	// Merge the chunk with adjacent free chunks
	chunk->tryMerge();

	// Coalesce all free blocks in the heap
	coalesceFreeBlocks();
}

void PalmyraOS::types::HeapManagerBase::coalesceFreeBlocks()
{
	// TODO merge all and not just immediate neighbors
	// Iterate through all chunks in the heap
	for (auto* chunk = heapList_; chunk; chunk = chunk->next_)
	{
		// Merge the chunk with adjacent free chunks if it is free
		if (!chunk->isAllocated_) chunk->tryMerge();
	}
}

void PalmyraOS::types::HeapManagerBase::expand(uint32_t new_size)
{
	// If the new size is less than or equal to the total memory, do nothing
	if (new_size <= totalMemory_) return;

	// Request more memory to expand the heap and update the total memory size
	requestMoreMemory(new_size - totalMemory_);
	totalMemory_ = new_size;
}

uint32_t PalmyraOS::types::HeapManagerBase::contract(uint32_t new_size)
{
	// If the new size is greater than or equal to the total memory, return the total memory
	if (new_size >= totalMemory_) return totalMemory_;

	// Update the total memory size and return the new total memory size
	totalMemory_ = new_size;
	return totalMemory_;
}

// Finds the smallest free chunk that can fit the requested size, optionally page-aligned
PalmyraOS::types::HeapChunk* PalmyraOS::types::HeapManagerBase::findSmallestHole(uint32_t size, bool page_align)
{
	// Initialize a pointer to the best fit chunk
	HeapChunk* bestFit = nullptr;
	for (auto* chunk   = heapList_; chunk; chunk = chunk->next_)
	{
		if (!chunk->isAllocated_ && chunk->size_ >= size)
		{
			if (page_align && ((uintptr_t(chunk) + sizeof(HeapChunk)) & 0xFFF)) continue;
			if (!bestFit || chunk->size_ < bestFit->size_)
			{
				bestFit = chunk;
			}
		}
	}
	return bestFit;
}
