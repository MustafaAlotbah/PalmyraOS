

#pragma once

#include "core/definitions.h"
#include "boot/multiboot.h"
#include "core/VBE.h"


namespace PalmyraOS::kernel
{

  [[noreturn]] void setup();

  extern PalmyraOS::kernel::VBE* vbe_ptr;

}


