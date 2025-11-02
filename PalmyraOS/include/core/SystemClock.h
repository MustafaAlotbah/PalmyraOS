
#pragma once

#include "core/Interrupts.h"
#include "core/definitions.h"


namespace PalmyraOS::kernel {

    class SystemClock {
    private:
        static constexpr uint16_t PIT_CMD_PORT             = 0x43;
        static constexpr uint16_t PIT_DAT_PORT             = 0x40;
        static constexpr uint32_t PIT_FREQUENCY_MUL        = 3579545;  // ~ 1193182 Hz (Hardware specific)
        static constexpr uint32_t PIT_FREQUENCY_DIV        = 3;
        static constexpr uint8_t PIT_CMD_REPEAT_INTERRUPTS = 0x36;

    public:
        static void initialize(uint32_t frequency);
        static bool setFrequency(uint32_t frequency);
        static void attachHandler(interrupts::InterruptHandler func);
        static uint16_t readCurrentCount();
        static uint64_t getTicks();
        static uint64_t getMilliseconds();
        static uint64_t getNanoseconds();
        static uint64_t getSeconds();
        static inline uint32_t getFrequency() { return frequency_; };

    private:
        static uint32_t* handleInterrupt(interrupts::CPURegisters* regs);

    private:
        static ports::BytePort PITCommandPort;
        static ports::BytePort PITDataPort;

        static uint64_t ticks_;                        // Ticks counter
        static interrupts::InterruptHandler handler_;  // Handler for the interrupt
        static uint32_t frequency_;                    // Frequency of the timer
    };

}  // namespace PalmyraOS::kernel