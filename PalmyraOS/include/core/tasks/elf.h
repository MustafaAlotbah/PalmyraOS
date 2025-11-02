
#pragma once


#include "core/kernel.h"
#include "core/memory/KernelHeapAllocator.h"


namespace PalmyraOS::kernel {

    int loadElf(KVector<uint8_t>& elfFileContent);


}  // namespace PalmyraOS::kernel