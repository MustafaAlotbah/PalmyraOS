
#pragma once


#include "core/port.h"
#include "core/Interrupts.h"


namespace PalmyraOS::kernel
{
  class Mouse
  {
   public:
	  static bool initialize();

	  static inline uint64_t getCounter()
	  { return count_; }
   private:

	  static uint32_t* handleInterrupt(interrupts::CPURegisters* regs);
	  static bool expectACK();

	  static void waitForInputBufferEmpty();
	  static void waitForOutputBufferFull();

   private:
	  static ports::BytePort commandPort_;
	  static ports::BytePort dataPort_;

	  static uint8_t buffer_[3];
	  static uint8_t offset_;

	  static uint64_t count_;
  };

} // namespace PalmyraOS::kernel