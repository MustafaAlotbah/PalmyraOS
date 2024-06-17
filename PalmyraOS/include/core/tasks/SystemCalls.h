
#pragma once

#include "core/Interrupts.h"


namespace PalmyraOS::kernel
{

  class SystemCallsManager
  {
   public:


	  static void initialize();

	  static uint32_t* handleInterrupt(interrupts::CPURegisters* regs);

   private:
  };


}