
#include "core/peripherals/Mouse.h"
#include "core/tasks/WindowManager.h"


///region Static Variables

PalmyraOS::kernel::ports::BytePort  PalmyraOS::kernel::Mouse::commandPort_(0x64);
PalmyraOS::kernel::ports::BytePort  PalmyraOS::kernel::Mouse::dataPort_(0x60);

bool    PalmyraOS::kernel::Mouse::initialized_ = false;
uint8_t PalmyraOS::kernel::Mouse::buffer_[3]{ 0 };
uint8_t PalmyraOS::kernel::Mouse::offset_      = 0;

///endregion





bool PalmyraOS::kernel::Mouse::initialize()
{
	initialized_ = false;

	// 1: Enable the mouse device (PS2_ENABLE_SECOND_PORT)
	commandPort_.write(0xA8);
	dataPort_.read();                // Flush the output buffer

	// 2. Set controller configuration byte
	commandPort_.write(0x20);                // Command to read controller configuration
	uint8_t status = dataPort_.read() | 0x03;   // Modify configuration to enable interrupt on IRQ12
	commandPort_.write(0x60);                // Command to write controller configuration
	dataPort_.write(status);

	// Enable mouse packet streaming
	commandPort_.write(0xD4);
	dataPort_.write(0xF4);        // Enable reporting

	// Set the Mouse interrupt handler anyway (TODO move to end)
	interrupts::InterruptController::setInterruptHandler(0x2c, &handleInterrupt);

	if (dataPort_.read() != 0xFA) return false;

	// // Optional: Verify mouse is in correct reporting mode
	// commandPort.write(0xD4);
	// dataPort.write(0xF2);               // Request mouse ID
	// uint8_t mouseID = dataPort.read(); // Read ID
	// if (mouseID != 0x03) { // Standard PS/2 mouse ID
	//     return false;
	// }

	initialized_ = true;
	return true;
}

uint32_t* PalmyraOS::kernel::Mouse::handleInterrupt(PalmyraOS::kernel::interrupts::CPURegisters* regs)
{

	uint8_t status = commandPort_.read();              // Read the next byte from the mouse data port
	if (!(status & 0x20)) return (uint32_t*)regs;      // Check if the PS/2 data is from mouse

	uint8_t mouse_in = dataPort_.read();
	// process mouse packet
	switch (offset_) // keep track of the cycle
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
			offset_ = 0;
			break;

		default:
			break;
	}

	// Early exist if TODO...
	if (offset_ != 0) return (uint32_t*)regs;


	bool isLeftDown   = (buffer_[0] & (1 << 0));
	bool isRightDown  = (buffer_[0] & (1 << 1));
	bool isMiddleDown = (buffer_[0] & (1 << 2));

	int deltaX = static_cast<int8_t>(buffer_[1]); // NOLINT
	int deltaY = static_cast<int8_t>(buffer_[2]); // NOLINT

	WindowManager::queueMouseEvent(
		{
			deltaX,
			-deltaY,
			isLeftDown,
			isRightDown,
			isMiddleDown
		}
	);

	return (uint32_t*)regs;
}










