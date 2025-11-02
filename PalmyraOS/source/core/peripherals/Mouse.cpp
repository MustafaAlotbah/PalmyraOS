
#include "core/peripherals/Mouse.h"
#include "core/cpu.h"
#include "core/tasks/WindowManager.h"


/// region Static Variables

PalmyraOS::kernel::ports::BytePort PalmyraOS::kernel::Mouse::commandPort_(0x64);
PalmyraOS::kernel::ports::BytePort PalmyraOS::kernel::Mouse::dataPort_(0x60);

uint8_t PalmyraOS::kernel::Mouse::buffer_[3]{0};
uint8_t PalmyraOS::kernel::Mouse::offset_ = 0;

uint64_t PalmyraOS::kernel::Mouse::count_ = 0;


/// endregion


bool PalmyraOS::kernel::Mouse::initialize() {

    // Disable mouse interrupts during initialization
    waitForInputBufferEmpty();
    commandPort_.write(0x20);  // Command to read controller configuration byte

    waitForOutputBufferFull();
    uint8_t commandByte = dataPort_.read();

    // Modify the command byte:
    commandByte &= ~0x02;  // Disable mouse interrupt (clear bit 1)
    commandByte |= 0x01;   // Ensure keyboard interrupt remains enabled (bit 0)
    commandByte &= ~0x10;  // Ensure keyboard clock is enabled (clear bit 4)
    commandByte &= ~0x20;  // Ensure mouse clock is enabled (clear bit 5)

    // Write the modified command byte back
    waitForInputBufferEmpty();
    commandPort_.write(0x60);  // Command to write controller configuration byte
    waitForInputBufferEmpty();
    dataPort_.write(commandByte);

    // Enable the auxiliary (mouse) device
    waitForInputBufferEmpty();
    commandPort_.write(0xA8);

    // Clear any residual data
    while (commandPort_.read() & 0x01) { dataPort_.read(); }

    // Reset the mouse
    waitForInputBufferEmpty();
    commandPort_.write(0xD4);  // Send to mouse

    waitForInputBufferEmpty();
    dataPort_.write(0xFF);  // Mouse Reset command

    waitForOutputBufferFull();
    if (dataPort_.read() != 0xFA) {
        // Handle error
        return false;
    }

    // Wait for self-test completion code
    waitForOutputBufferFull();
    if (dataPort_.read() != 0xAA) {
        // Handle error
        return false;
    }

    // Mouse sends another ACK after reset
    waitForOutputBufferFull();
    if (dataPort_.read() != 0x00) {
        // Handle error
        return false;
    }

    // Set default settings (optional)
    waitForInputBufferEmpty();
    commandPort_.write(0xD4);  // Send to mouse

    waitForInputBufferEmpty();
    dataPort_.write(0xF6);  // Set defaults

    waitForOutputBufferFull();
    if (dataPort_.read() != 0xFA) {
        // Handle error
        return false;
    }

    // Enable data reporting
    waitForInputBufferEmpty();
    commandPort_.write(0xD4);  // Send to mouse

    waitForInputBufferEmpty();
    dataPort_.write(0xF4);  // Enable Data Reporting

    waitForOutputBufferFull();
    if (dataPort_.read() != 0xFA) {
        // Handle error
        return false;
    }

    // Re-enable mouse interrupts after initialization
    waitForInputBufferEmpty();
    commandPort_.write(0x20);  // Command to read controller configuration byte

    waitForOutputBufferFull();
    commandByte = dataPort_.read();

    commandByte |= 0x02;  // Enable mouse interrupt (set bit 1)

    waitForInputBufferEmpty();
    commandPort_.write(0x60);  // Command to write controller configuration byte
    waitForInputBufferEmpty();
    dataPort_.write(commandByte);

    // Set the mouse interrupt handler
    interrupts::InterruptController::setInterruptHandler(0x2C, &handleInterrupt);

    return true;
}

uint32_t* PalmyraOS::kernel::Mouse::handleInterrupt(PalmyraOS::kernel::interrupts::CPURegisters* regs) {
    // Check if data is available
    uint8_t status = commandPort_.read();

    // Check if the data is from the mouse
    if (!(status & 0x20)) return (uint32_t*) regs;

    count_++;

    uint8_t mouse_in = dataPort_.read();
    // process mouse packet
    switch (offset_)  // keep track of the cycle
    {
        case 0:
            // First byte contains flags and button states
            if ((mouse_in & 0x08) == 0) break;  // Not a valid packet, ignore
            buffer_[0] = mouse_in;
            offset_++;
            break;

        case 1:
            // Second byte contains X movement
            buffer_[1] = mouse_in;
            offset_++;
            break;

        case 2:
            // First byte contains Y movement
            buffer_[2] = mouse_in;
            offset_    = 0;
            break;

        default:
            // Should not reach here, reset offset
            offset_ = 0;
            break;
    }

    // Early exist if TODO...
    if (offset_ != 0) return (uint32_t*) regs;

    // Handle overflow bits (Bit 6 and Bit 7 are the X and Y sign bits)
    bool xOverflow    = (buffer_[0] & (1 << 6));  // X overflow (bit 6)
    bool yOverflow    = (buffer_[0] & (1 << 7));  // Y overflow (bit 7)

    bool isLeftDown   = (buffer_[0] & (1 << 0));
    bool isRightDown  = (buffer_[0] & (1 << 1));
    bool isMiddleDown = (buffer_[0] & (1 << 2));

    int deltaX        = static_cast<int8_t>(buffer_[1]);  // NOLINT
    int deltaY        = static_cast<int8_t>(buffer_[2]);  // NOLINT

    // Estimate velocity and apply smoothing
    if (xOverflow) deltaX = static_cast<int>(deltaX * 2);
    if (yOverflow) deltaY = static_cast<int>(deltaY * 2);

    WindowManager::queueMouseEvent({deltaX, -deltaY, isLeftDown, isRightDown, isMiddleDown, true});

    return (uint32_t*) regs;
}

bool PalmyraOS::kernel::Mouse::expectACK() {
    waitForOutputBufferFull();
    uint8_t response = dataPort_.read();
    return (response == 0xFA);  // 0xFA is ACK
}

void PalmyraOS::kernel::Mouse::waitForInputBufferEmpty() {
    while (commandPort_.read() & 0x02) {
        // Wait until Bit 1 (Input Buffer Status) is clear
    }
}

void PalmyraOS::kernel::Mouse::waitForOutputBufferFull() {
    while (!(commandPort_.read() & 0x01)) {
        // Wait until Bit 0 (Output Buffer Status) is set
    }
}
