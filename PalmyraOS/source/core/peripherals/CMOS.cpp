

#include "core/peripherals/CMOS.h"


PalmyraOS::kernel::ports::BytePort PalmyraOS::kernel::CMOS::addressRegister(addressRegisterPort);
PalmyraOS::kernel::ports::BytePort PalmyraOS::kernel::CMOS::valueRegister(valueRegisterPort);

void PalmyraOS::kernel::CMOS::write(uint8_t index, uint8_t value) {
    addressRegister.write(index);
    valueRegister.write(value);
}
uint8_t PalmyraOS::kernel::CMOS::read(uint8_t index) {
    addressRegister.write(index);
    return valueRegister.read();
}
