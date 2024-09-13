
#pragma once


#include "core/port.h"
#include "core/Interrupts.h"


namespace PalmyraOS::kernel
{
  class Mouse
  {
   public:
	  static bool initialize();

   private:

	  static uint32_t* handleInterrupt(interrupts::CPURegisters* regs);

   private:
	  static ports::BytePort commandPort_;
	  static ports::BytePort dataPort_;

	  static bool    initialized_;
	  static uint8_t buffer_[3];
	  static uint8_t offset_;

  };

} // namespace PalmyraOS::kernel