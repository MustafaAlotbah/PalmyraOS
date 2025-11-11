#include "core/network/ProtocolSocket.h"
#include "core/kernel.h"  // For heapManager

namespace PalmyraOS::kernel {

    // ==================== Memory Management (Freestanding C++) ====================

    void* ProtocolSocket::operator new(size_t size) { return heapManager.alloc(size); }

    void* ProtocolSocket::operator new(size_t size, void* ptr) noexcept { return ptr; }

    void ProtocolSocket::operator delete(void* ptr) noexcept {
        if (ptr) heapManager.free(ptr);
    }

    void ProtocolSocket::operator delete(void* ptr, size_t size) noexcept {
        if (ptr) heapManager.free(ptr);
    }

}  // namespace PalmyraOS::kernel
