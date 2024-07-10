
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

bool   PalmyraOS::kernel::Keyboard::initialized_                  = false;
size_t PalmyraOS::kernel::Keyboard::buffer_last_index_            = 0;
char   PalmyraOS::kernel::Keyboard::buffer_[Keyboard::bufferSize] = { 0 };

bool PalmyraOS::kernel::Keyboard::isShiftPressed_ = false;
bool PalmyraOS::kernel::Keyboard::isCtrlPressed_  = false;
bool PalmyraOS::kernel::Keyboard::isAltPressed_   = false;
bool PalmyraOS::kernel::Keyboard::isCapsLockOn_   = false;
bool PalmyraOS::kernel::Keyboard::isNumLockOn_    = false;
bool PalmyraOS::kernel::Keyboard::isScrollLockOn_ = false;

bool PalmyraOS::kernel::Keyboard::initialize()
{
	initialized_ = false;

	while (commandPort_.read() & 0x2);        // Wait to be ready

	commandPort_.write(0xAD);                // DISABLE

	// Clear previously pressed keys from the PIC's buffer
	while (commandPort_.read() & 0x1) dataPort_.read();

	commandPort_.write(0xAE);                // Enable the keyboard

	commandPort_.write(0x20);                // Command to get the current state
	uint8_t status = dataPort_.read();
	status = (status | 1) & ~0x10;
	commandPort_.write(0x60);                // Command to set state
	dataPort_.write(status);                 // Write the modified state

	dataPort_.write(0xF4);                   // Enable the scanning
	uint8_t response = dataPort_.read();

	// Set the keyboard interrupt handler anyway (TODO move to end)
	interrupts::InterruptController::setInterruptHandler(0x21, &handleInterrupt);

	// Handle error, maybe retry or log the error
	if (response != 0xFA) return false;

	updateLockKeyStatus();

	initialized_ = true;
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


	buffer_last_index_++;
	if (buffer_last_index_ >= bufferSize) buffer_last_index_ = 0;

	buffer_[buffer_last_index_] = qwertzToAscii[keyIndex];

	if (state == KeyState::RELEASED && buffer_[buffer_last_index_] != 0)
	{
		WindowManager::queueKeyboardEvent(
			{
				buffer_[buffer_last_index_],
				state == KeyState::PRESSED,
				isCtrlPressed_,
				isShiftPressed_,
				isAltPressed_
			}
		);
	}

	return (uint32_t*)regs;
}
