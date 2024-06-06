

#pragma once

#include "core/definitions.h"
#include "boot/multiboot.h"
#include "core/VBE.h"


namespace PalmyraOS::kernel
{

  /**
   * Sets up the kernel. This function is called after the system has been initialized.
   * It initializes various subsystems and enters the main kernel loop.
   */
  [[noreturn]] void setup();

  void update(uint64_t dummy_up_time);

  /**
   * Pointer to the VBE (VESA BIOS Extensions) object.
   * This global pointer is used to access VBE functions and information.
   */
  extern PalmyraOS::kernel::VBE* vbe_ptr;

}


