
#include "palmyraOS/HeapAllocator.h"
#include "palmyraOS/stdlib.h" // malloc / free

void* PalmyraOS::types::UserHeapManager::allocateMemory(size_t size)
{
	return malloc(size);
}

void PalmyraOS::types::UserHeapManager::freePage(void* address)
{
	free(address);
}

PalmyraOS::types::UserHeapManager::~UserHeapManager()
{

	HeapChunk* current         = HeapManagerBase::heapList_;
	HeapChunk* nextPageAligned = nullptr;

	while (current)
	{
		// Find the next page-aligned chunk
		nextPageAligned = current->next_;
		while (nextPageAligned && ((uintptr_t)nextPageAligned & 0xFFF) != 0)
		{
			nextPageAligned = nextPageAligned->next_;
		}

		// Free the current page-aligned chunk
		freePage((void*)current);

		// Move to the next page-aligned chunk
		current = nextPageAligned;
	}
}