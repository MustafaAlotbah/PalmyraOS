

#pragma once

#include "core/port.h"


namespace PalmyraOS::kernel {


    class CMOS {
        static constexpr uint32_t addressRegisterPort = 0x70;
        static constexpr uint32_t valueRegisterPort   = 0x71;

    public:
        static void write(uint8_t index, uint8_t value);
        static uint8_t read(uint8_t index);

    private:
        static ports::BytePort addressRegister;
        static ports::BytePort valueRegister;
    };

}  // namespace PalmyraOS::kernel