
#include "core/memory/KernelHeap.h"
#include "core/memory/paging.h"


void* PalmyraOS::kernel::HeapManager::allocateMemory(size_t size) {
    // Allocate the required number of pages
    return PagingManager::allocatePages(size >> PAGE_BITS);
}

void PalmyraOS::kernel::HeapManager::freePage(void* address) { PagingManager::freePage((void*) address); }

PalmyraOS::kernel::HeapManager::~HeapManager() {
    using namespace types;
    HeapChunk* current         = HeapManagerBase::heapList_;
    HeapChunk* nextPageAligned = nullptr;

    while (current) {
        // Find the next page-aligned chunk
        nextPageAligned = current->next_;
        while (nextPageAligned && ((uintptr_t) nextPageAligned & 0xFFF) != 0) { nextPageAligned = nextPageAligned->next_; }

        // Free the current page-aligned chunk
        PagingManager::freePage((void*) current);

        // Move to the next page-aligned chunk
        current = nextPageAligned;
    }
}
