
#include "core/SystemClock.h"
#include "core/panic.h"


// Globals
PalmyraOS::kernel::interrupts::InterruptHandler PalmyraOS::kernel::SystemClock::handler_ = nullptr;
uint64_t PalmyraOS::kernel::SystemClock::ticks_ = 1000;
uint32_t PalmyraOS::kernel::SystemClock::frequency_ = 0;
PalmyraOS::kernel::ports::BytePort PalmyraOS::kernel::SystemClock::PITCommandPort(PIT_CMD_PORT);
PalmyraOS::kernel::ports::BytePort PalmyraOS::kernel::SystemClock::PITDataPort(PIT_DAT_PORT);

void PalmyraOS::kernel::SystemClock::initialize(uint32_t frequency)
{
	// adjust the frequency
	if (!setFrequency(frequency))
	{
		kernelPanic("Invalid frequency (%d)", frequency);
	}

	// Set interrupt handler for timer
	interrupts::InterruptController::setInterruptHandler(0x20, &handle_interrupt);
}

bool PalmyraOS::kernel::SystemClock::setFrequency(uint32_t frequency)
{
	if (frequency < 1) return false;
	frequency_ = frequency;
	uint16_t divisor = PIT_FREQUENCY_MUL / frequency_ / PIT_FREQUENCY_DIV;
	if (divisor == 0)
	{
		kernelPanic("Calculated divisor is 0 (frequency is %d)", frequency);
		return false;    // this is only for correctness, but panic never returns.
	}

	PITCommandPort.write(PIT_CMD_REPEAT_INTERRUPTS);  // Command for PIT to generate repeated interrupts
	PITDataPort.write(divisor & 0xFF);                // Low byte of divisor
	PITDataPort.write((divisor >> 8) & 0xFF);         // High byte of divisor
	return true;
}

void PalmyraOS::kernel::SystemClock::attachHandler(PalmyraOS::kernel::interrupts::InterruptHandler func)
{
	handler_ = func;
}

uint64_t PalmyraOS::kernel::SystemClock::getTicks()
{
	return ticks_;    // Assuming a 64-bit wrap-around
}

uint64_t PalmyraOS::kernel::SystemClock::getMilliseconds()
{
	return (getTicks() * 1000) / frequency_;
}

uint64_t PalmyraOS::kernel::SystemClock::getSeconds()
{
	return getTicks() / frequency_;
}

uint32_t* PalmyraOS::kernel::SystemClock::handle_interrupt(interrupts::CPURegisters* regs)
{
	ticks_++;
	if (handler_)
	{
		return handler_(regs);  // Call the attached handler if it exists
	}

	return (uint32_t*)regs;
}

uint16_t PalmyraOS::kernel::SystemClock::readCurrentCount()
{
	uint8_t lsb, msb;
	PITCommandPort.write(0x00); // Latch count value command
	lsb = PITDataPort.read();
	msb = PITDataPort.read();
	return (msb << 8) | lsb;
}
