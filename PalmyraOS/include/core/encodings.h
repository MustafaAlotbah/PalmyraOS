
#pragma once

#include "libs/string.h"
#include "core/memory/KernelHeapAllocator.h"


namespace PalmyraOS::kernel
{

  KString utf16le_to_utf8(const KWString& utf16le_string);

  // Function to convert UTF-8 encoded data to UTF-16LE
  KWString utf8_to_utf16le(const KString& utf8_string);

}