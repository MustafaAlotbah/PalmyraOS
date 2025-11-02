#include "core/port.h"


namespace PalmyraOS::kernel::ports {
    // Base port class constructor
    BasePort::BasePort(uint16_t portNumber) : portNumber_{portNumber} {}

    // Write a byte to a port
    void BytePort::write(uint32_t data) const { __asm__ __volatile__("outb %0, %1" : : "a"(static_cast<uint8_t>(data)), "Nd"(portNumber_)); }

    // Read a byte from a port
    uint32_t BytePort::read() const {
        uint8_t result;
        __asm__ __volatile__("inb %1, %0" : "=a"(result) : "Nd"(portNumber_));
        return result;
    }

    // Write a byte to a port with a delay (for slower ports)
    void SlowBytePort::write(uint32_t data) const {
        __asm__ __volatile__("outb %0, %1\n"
                             "jmp 1f\n"
                             "1: jmp 1f\n"
                             "1:"
                             :
                             : "a"(static_cast<uint8_t>(data)), "Nd"(portNumber_));
    }

    // Write a word (2 bytes) to a port
    void WordPort::write(uint32_t data) const { __asm__ __volatile__("outw %0, %1" : : "a"(static_cast<uint16_t>(data)), "Nd"(portNumber_)); }

    // Read a word (2 bytes) from a port
    uint32_t WordPort::read() const {
        uint16_t result;
        __asm__ __volatile__("inw %1, %0" : "=a"(result) : "Nd"(portNumber_));
        return result;
    }

    // Write a double word (4 bytes) to a port
    void DoublePort::write(uint32_t data) const { __asm__ __volatile__("outl %0, %1" : : "a"(data), "Nd"(portNumber_)); }

    // Read a double word (4 bytes) from a port
    uint32_t DoublePort::read() const {
        uint32_t result;
        __asm__ __volatile__("inl %1, %0" : "=a"(result) : "Nd"(portNumber_));
        return result;
    }


}  // namespace PalmyraOS::kernel::ports