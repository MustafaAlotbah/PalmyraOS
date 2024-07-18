
#pragma once

#include "libs/string.h"
#include "core/memory/KernelHeapAllocator.h"


namespace PalmyraOS::kernel
{

  KString utf16le_to_utf8(const KWString& utf16le_string);
}