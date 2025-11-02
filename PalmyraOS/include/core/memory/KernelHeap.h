
#pragma once

#include "palmyraOS/shared/memory/Heap.h"


namespace PalmyraOS::kernel {

    class HeapManager : public types::HeapManagerBase {
    public:
        ~HeapManager();
        void* allocateMemory(size_t size) final;
        void freePage(void* address) final;
    };
}  // namespace PalmyraOS::kernel