
#pragma once

#include "core/port.h"
#include "core/Interrupts.h"


namespace PalmyraOS::kernel
{


  class Keyboard
  {
   public:
	  enum class LockKey
	  {
		  Num,
		  Scroll,
		  Cap
	  };
	  enum class ControlKey
	  {
		  Control,
		  Alt,
		  Shift
	  };
	  enum class KeyState
	  {
		  RELEASED = 0,
		  PRESSED  = 1,
	  };
   public:
	  static bool initialize();
	  static bool togglKey(LockKey lockKey);

	  static inline uint64_t getCount()
	  { return counter_; }

   private:
	  static bool updateLockKeyStatus();
	  static void initializeLockKeys();
	  static void toggleLockKeys(uint8_t keyCode);

	  static void waitForInputBufferEmpty();
	  static void waitForOutputBufferFull();

	  static uint32_t* handleInterrupt(interrupts::CPURegisters* regs);
   private:
	  static constexpr size_t bufferSize = 10;
	  static ports::BytePort  commandPort_;
	  static ports::BytePort  dataPort_;

	  static size_t buffer_last_index_;
	  static char   buffer_[bufferSize];

	  static bool isShiftPressed_;
	  static bool isCtrlPressed_;
	  static bool isAltPressed_;
	  static bool isCapsLockOn_;
	  static bool isNumLockOn_;
	  static bool isScrollLockOn_;
	  static uint64_t counter_;
  };


}