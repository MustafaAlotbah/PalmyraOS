
#include "core/peripherals/Keyboard.h"
#include "core/tasks/WindowManager.h"


const char qwertzToAscii[128] = {
	0,        // Null
	27,        // Escape
	'1', '2', '3', '4', '5', '6', '7', '8', '9', '0',        // 1234567890
	'B',
	'/',    // '`'
	8,    // backspace
	9,    // h tab
	'q', 'w', 'e', 'r', 't', 'z', 'u', 'i', 'o', 'p', 'u', '+',
	'\n', // Enter = \n
	0,    // ctrl
	'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', 'o', 'a', '#',
	0, '#', 'y', 'x', 'c', 'v', 'b', 'n', 'm', ',', '.', '/',
	0, // right shift
	'*', 0, ' '
};

PalmyraOS::kernel::ports::BytePort PalmyraOS::kernel::Keyboard::commandPort_(0x64);
PalmyraOS::kernel::ports::BytePort PalmyraOS::kernel::Keyboard::dataPort_(0x60);

size_t PalmyraOS::kernel::Keyboard::buffer_last_index_            = 0;
char   PalmyraOS::kernel::Keyboard::buffer_[Keyboard::bufferSize] = { 0 };

bool     PalmyraOS::kernel::Keyboard::isShiftPressed_ = false;
bool     PalmyraOS::kernel::Keyboard::isCtrlPressed_  = false;
bool     PalmyraOS::kernel::Keyboard::isAltPressed_   = false;
bool     PalmyraOS::kernel::Keyboard::isCapsLockOn_   = false;
bool     PalmyraOS::kernel::Keyboard::isNumLockOn_    = false;
bool     PalmyraOS::kernel::Keyboard::isScrollLockOn_ = false;
uint64_t PalmyraOS::kernel::Keyboard::counter_        = 0;

bool PalmyraOS::kernel::Keyboard::initialize()
{
	// Disable the keyboard
	waitForInputBufferEmpty();
	commandPort_.write(0xAD);

	// Clear the output buffer
	while (commandPort_.read() & 0x01)
	{
		dataPort_.read();
	}

	// Read the current command byte
	waitForInputBufferEmpty();
	commandPort_.write(0x20);  // Command to read controller configuration byte

	waitForOutputBufferFull();
	uint8_t commandByte = dataPort_.read();

	// Modify the command byte:
	commandByte |= 0x01;  // Enable keyboard interrupt (bit 0)
	commandByte |= 0x02;  // Ensure mouse interrupt is enabled (bit 1)
	commandByte &= ~0x10; // Enable keyboard clock (clear bit 4)
	commandByte &= ~0x20; // Ensure mouse clock is enabled (clear bit 5)

	// Write the modified command byte back
	waitForInputBufferEmpty();
	commandPort_.write(0x60);  // Command to write controller configuration byte
	waitForInputBufferEmpty();
	dataPort_.write(commandByte);

	// Enable the keyboard
	waitForInputBufferEmpty();
	commandPort_.write(0xAE);

	// Enable scanning
	waitForInputBufferEmpty();
	dataPort_.write(0xF4);

	waitForOutputBufferFull();
	uint8_t response = dataPort_.read();

	if (response != 0xFA)
	{
		// Handle error
		return false;
	}

	// Set the keyboard interrupt handler
	interrupts::InterruptController::setInterruptHandler(0x21, &handleInterrupt);

	return true;
}

bool PalmyraOS::kernel::Keyboard::togglKey(PalmyraOS::kernel::Keyboard::LockKey lockKey)
{
	return false;
}

bool PalmyraOS::kernel::Keyboard::updateLockKeyStatus()
{
	commandPort_.write(0xED);  // Send the command to read the LED status
	if (dataPort_.read() != 0xFA) return false;  // Wait for ACK

	uint8_t ledStatus = dataPort_.read();  // Read the LED status byte
	if (ledStatus == 0xFF) return false;  // Check for error

	// Update internal state according to the LED status byte
	isScrollLockOn_ = (ledStatus & 0x01) != 0;
	isNumLockOn_    = (ledStatus & 0x02) != 0;
	isCapsLockOn_   = (ledStatus & 0x04) != 0;

	return true;
}

void PalmyraOS::kernel::Keyboard::toggleLockKeys(uint8_t keyCode)
{
	dataPort_.write(0xED); // Set LEDs

	bool condition_01 = keyCode == 0x3A && !isCapsLockOn_;
	bool condition_02 = keyCode == 0x45 && !isNumLockOn_;
	bool condition_03 = keyCode == 0x46 && !isScrollLockOn_;

	if (condition_01 || condition_02 || condition_03)
	{
		dataPort_.write(0x0);
	}
	else
	{
		dataPort_.write(0x07);
	}

	if (dataPort_.read() == 0xFA)
	{
		if (keyCode == 0x3A) isCapsLockOn_   = !isCapsLockOn_;
		if (keyCode == 0x45) isNumLockOn_    = !isNumLockOn_;
		if (keyCode == 0x46) isScrollLockOn_ = !isScrollLockOn_;
	}
}

void PalmyraOS::kernel::Keyboard::initializeLockKeys()
{
	// Set initial state of Caps Lock, Num Lock, and Scroll Lock to off
	toggleLockKeys(0x3A); // Caps Lock
	toggleLockKeys(0x45); // Num Lock
	toggleLockKeys(0x46); // Scroll Lock
}

uint32_t* PalmyraOS::kernel::Keyboard::handleInterrupt(PalmyraOS::kernel::interrupts::CPURegisters* regs)
{

	// Ensure the interrupt is for the keyboard (IRQ1 corresponds to interrupt vector 0x21)
	if (regs->intNo != 0x21)
	{
		// Not a keyboard interrupt, ignore it
		return (uint32_t*)regs;
	}

	uint8_t  keyIndex = dataPort_.read();
	KeyState state    = KeyState::PRESSED;

	if (keyIndex >= 0x80)
	{
		keyIndex -= 0x80;
		state = KeyState::RELEASED;
	}

	if (keyIndex == 0x38)                                                // ALT
	{
		isAltPressed_ = (state == KeyState::PRESSED);
	}
	else if (keyIndex == 0x36 || keyIndex == 0x2A)                        // Shift
	{
		isShiftPressed_ = (state == KeyState::PRESSED);
	}
	else if (keyIndex == 0x1D)                                            // CTRL
	{
		isCtrlPressed_ = (state == KeyState::PRESSED);
	}
	else if (keyIndex == 0x3A || keyIndex == 0x45 || keyIndex == 0x46)
	{        // CTRL

		if (state == KeyState::PRESSED) toggleLockKeys(keyIndex);
	}

	counter_++;
	buffer_last_index_++;
	if (buffer_last_index_ >= bufferSize) buffer_last_index_ = 0;

	buffer_[buffer_last_index_] = qwertzToAscii[keyIndex];

	if (/*state == KeyState::RELEASED && */buffer_[buffer_last_index_] != 0)
	{
		WindowManager::queueKeyboardEvent(
			{
				buffer_[buffer_last_index_],
				state == KeyState::PRESSED,
				isCtrlPressed_,
				isShiftPressed_,
				isAltPressed_,
				true
			}
		);
	}

	return (uint32_t*)regs;
}

void PalmyraOS::kernel::Keyboard::waitForInputBufferEmpty()
{
	while (commandPort_.read() & 0x02)
	{
		// Wait until Bit 1 (Input Buffer Status) is clear
	}
}

void PalmyraOS::kernel::Keyboard::waitForOutputBufferFull()
{
	while (!(commandPort_.read() & 0x01))
	{
		// Wait until Bit 0 (Output Buffer Status) is set
	}
}