
#include "core/tasks/Descriptor.h"
#include "core/kernel.h"  // For heapManager

namespace PalmyraOS::kernel {

    // ==================== Custom Memory Management (Freestanding C++) ====================

    void* Descriptor::operator new(size_t size) { return heapManager.alloc(size); }

    // Placement new - memory already allocated
    void* Descriptor::operator new(size_t size, void* ptr) noexcept { return ptr; }

    void Descriptor::operator delete(void* ptr) noexcept {
        if (ptr) heapManager.free(ptr);
    }

    void Descriptor::operator delete(void* ptr, size_t size) noexcept {
        if (ptr) heapManager.free(ptr);
    }

}  // namespace PalmyraOS::kernel
