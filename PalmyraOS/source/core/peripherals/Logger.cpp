
#include "core/peripherals/Logger.h"

#include "core/cpu.h"
#include "core/port.h"

#include "libs/stdio.h"

#include <cstdarg>

// ============================================================================
// SERIAL PORT HARDWARE DEFINITIONS (COM1 - 0x3F8)
// ============================================================================
// The x86 architecture uses I/O ports to communicate with hardware devices.
// COM1 (serial port 1) is the standard debugging port, located at I/O base 0x3F8.
//
// Each port address corresponds to a different UART register:

#define COM1_PORT_BASE 0x3F8

// Data I/O Register (read/write)
// - Write: Send a byte to the serial port
// - Read: Receive a byte from the serial port (when DLAB=0)
#define UART_DATA_PORT (COM1_PORT_BASE + 0)

// Interrupt Enable Register (write only, when DLAB=0)
// - Bit 0: Enable Received Data Available Interrupt
// - Bit 1: Enable Transmit Holding Register Empty Interrupt
// - We write 0x00 to disable all interrupts (we use polling instead)
#define UART_IER_PORT (COM1_PORT_BASE + 1)

// FIFO Control Register (write only)
// - Bit 0: Enable FIFO
// - Bit 1: Clear Receive FIFO
// - Bit 2: Clear Transmit FIFO
// - 0xC7 = enable FIFO + clear both FIFOs
// - FIFO allows the serial port to buffer multiple bytes, preventing data loss
#define UART_FCR_PORT (COM1_PORT_BASE + 2)

// Line Control Register (read/write)
// - Bit 7: Divisor Latch Access Bit (DLAB)
//   When DLAB=1, we can write the baud rate divisor (ports 0x3F8, 0x3F9)
//   When DLAB=0, we can use data/IER ports normally
// - Bits 0-1: Word length (0x03 = 8 bits)
// - Bit 2: Stop bits (0 = 1 stop bit)
// - Bits 3-5: Parity (0 = no parity)
// Result: 0x03 = 8 bits, 1 stop bit, no parity (8N1 - standard format)
#define UART_LCR_PORT (COM1_PORT_BASE + 3)

// Modem Control Register (write only)
// - Bit 0: DTR (Data Terminal Ready) - tells the device we're ready
// - Bit 1: RTS (Request To Send) - requests permission to send
// - 0x0B = DTR + RTS enabled (standard for communication)
#define UART_MCR_PORT (COM1_PORT_BASE + 4)

// Line Status Register (read only)
// - Bit 5: Transmitter Holding Register Empty (THRE)
//   This bit is 1 when the serial port is ready to accept the next byte
//   We use polling on this bit to know when to send the next character
#define UART_LSR_PORT (COM1_PORT_BASE + 5)

// Bit 5 of the Line Status Register (Transmitter Ready Flag)
#define UART_LSR_TRANSMIT_EMPTY 0x20

// ============================================================================
// SERIAL PORT OBJECTS
// ============================================================================
// We create BytePort objects for each register we need to access.
// These objects encapsulate the x86 I/O port read/write operations (inb/outb).

PalmyraOS::kernel::ports::BytePort loggingPort(UART_DATA_PORT);  // For sending data
PalmyraOS::kernel::ports::BytePort lsrPort(UART_LSR_PORT);       // To check if port is ready
PalmyraOS::kernel::ports::BytePort ierPort(UART_IER_PORT);       // Interrupt control
PalmyraOS::kernel::ports::BytePort fcrPort(UART_FCR_PORT);       // FIFO control
PalmyraOS::kernel::ports::BytePort lcrPort(UART_LCR_PORT);       // Line control (baud rate)
PalmyraOS::kernel::ports::BytePort mcrPort(UART_MCR_PORT);       // Modem control (RTS/DTR)

// Flag to track if serial port has been initialized
// Prevents logging before initialization (which would cause undefined behavior)
static bool serialPortInitialized = false;

// ============================================================================
// HELPER FUNCTION: Wait for Serial Port to be Ready
// ============================================================================
/**
 * @brief Poll the serial port until it's ready for transmission
 *
 * The UART (Universal Asynchronous Receiver Transmitter) has a transmit buffer
 * that can hold one byte. We must wait for this buffer to be empty before
 * sending the next byte. We do this by checking bit 5 of the Line Status Register.
 *
 * This is called "polling" - repeatedly checking hardware status instead of
 * using interrupts. Polling is simpler and works well for kernel debugging.
 *
 * @param timeoutIterations Maximum number of times to check before giving up
 * @return true if port is ready (THRE bit is set)
 *         false if timeout occurs (serial port may be disconnected)
 */
static bool waitForTransmitReady(uint32_t timeoutIterations = 10000) {
    // Loop up to timeoutIterations times
    for (uint32_t i = 0; i < timeoutIterations; ++i) {
        // Read the Line Status Register
        uint8_t status = lsrPort.read();

        // Check if bit 5 (THRE - Transmitter Holding Register Empty) is set
        // If it is, the transmit buffer is empty and ready for new data
        if (status & UART_LSR_TRANSMIT_EMPTY) {
            return true;  // Port is ready!
        }
        // If not ready, loop and try again
    }

    // If we've looped timeoutIterations times without the port becoming ready,
    // assume the serial port is disconnected and return false
    return false;
}

// ============================================================================
// MAIN INITIALIZATION FUNCTION
// ============================================================================
/**
 * @brief Initialize the COM1 serial port for logging
 *
 * UART Initialization Process:
 * 1. Disable interrupts (we use polling, not interrupts)
 * 2. Set baud rate using the Divisor Latch
 * 3. Configure data format (8N1 = 8 bits, no parity, 1 stop bit)
 * 4. Enable FIFO buffering
 * 5. Enable modem control signals (RTS/DTR)
 *
 * Baud Rate Calculation:
 * The divisor formula: divisor = 115200 / desired_baud_rate
 * For 115200 baud: divisor = 115200 / 115200 = 1
 * For 9600 baud: divisor = 115200 / 9600 = 12
 *
 * The divisor is a 16-bit value split into two 8-bit ports:
 * - Port 0x3F8 (when DLAB=1): Low byte of divisor
 * - Port 0x3F9 (when DLAB=1): High byte of divisor
 *
 * @param baudRate Target baud rate (default 115200)
 * @return true if initialization succeeded, false on error
 */
bool PalmyraOS::kernel::initializeSerialPort(uint32_t baudRate) {
    // Step 1: Calculate the divisor for the desired baud rate
    // ========================================
    // The UART clock is 1.8432 MHz. To get the desired baud rate:
    // divisor = 1.8432 MHz / (16 × baud_rate)
    // This simplifies to: divisor = 115200 / baud_rate
    uint16_t divisor = 115200 / baudRate;

    // Safety check: ensure divisor is at least 1
    if (divisor == 0) divisor = 1;

    // Step 2: Disable interrupts
    // ========================================
    // We write 0x00 to the Interrupt Enable Register to disable all serial interrupts.
    // This is because we use polling (checking status register) instead of interrupts
    // for kernel logging. Polling is simpler and safer during early kernel boot.
    ierPort.write(0x00);

    // Step 3: Enable DLAB to access baud rate registers
    // ========================================
    // DLAB (Divisor Latch Access Bit) is bit 7 of the Line Control Register.
    // When DLAB = 1, ports 0x3F8 and 0x3F9 become the baud rate divisor registers.
    // When DLAB = 0, they function as normal (data and interrupt enable).
    // 0x80 = 10000000 binary = just bit 7 set (DLAB=1, everything else default)
    lcrPort.write(0x80);

    // Step 4: Set the baud rate divisor
    // ========================================
    // The divisor is a 16-bit value. We write it LSB first, then MSB.
    // Port 0x3F8 (when DLAB=1): Low byte (bits 0-7)
    // Port 0x3F9 (when DLAB=1): High byte (bits 8-15)

    loggingPort.write(divisor & 0xFF);  // Low byte of divisor

    // Access port 0x3F9 (0x3F8 + 1) to write the high byte
    PalmyraOS::kernel::ports::BytePort dhrPort(COM1_PORT_BASE + 1);
    dhrPort.write((divisor >> 8) & 0xFF);  // High byte of divisor

    // Step 5: Configure line format and disable DLAB
    // ========================================
    // Set the line control register to:
    // - Disable DLAB (bit 7 = 0) so we can use data/IER ports again
    // - 8 data bits (bits 0-1 = 11 binary = 3)
    // - 1 stop bit (bit 2 = 0)
    // - No parity (bits 3-5 = 000 binary)
    // Result: 0x03 = 00000011 binary = 8N1 (standard serial format)
    lcrPort.write(0x03);

    // Step 6: Enable FIFO buffering
    // ========================================
    // FIFO (First In, First Out) buffer allows the serial port to buffer multiple bytes.
    // This prevents data loss if we can't write fast enough.
    // 0xC7 = 11000111 binary
    //   Bit 0: Enable FIFO
    //   Bit 1: Clear Receive FIFO (buffer)
    //   Bit 2: Clear Transmit FIFO (buffer)
    fcrPort.write(0xC7);

    // Step 7: Enable modem control signals
    // ========================================
    // Set DTR (Data Terminal Ready) and RTS (Request To Send) signals.
    // These tell the serial device that we're ready for communication.
    // 0x0B = 00001011 binary
    //   Bit 0: DTR (Data Terminal Ready) = 1
    //   Bit 1: RTS (Request To Send) = 1
    //   Bit 3: Out2 = 1
    mcrPort.write(0x0B);

    // Mark serial port as initialized
    serialPortInitialized = true;
    return true;
}

// ============================================================================
// CORE LOGGING FUNCTION
// ============================================================================
/**
 * @brief Send a message to the serial port, character by character
 *
 * This function writes each character of a message to the serial port,
 * checking that the port is ready before each write to prevent data loss.
 *
 * @param slow If true, add delays between characters (for critical error logs)
 *             If false, send as fast as possible
 * @param message The message string to send (null-terminated)
 */
void log_msg(bool slow, const char* message) {
    // Safety check: if serial port not initialized, silently discard the message
    // This prevents crashes if someone tries to log before initialization
    if (!serialPortInitialized) return;

    // Maximum characters to send (safety limit)
    uint64_t maxBuffer       = 4096;

    // Calculate delay iterations based on slow flag
    // When slow=true, we wait longer between characters (more reliable for errors)
    // When slow=false, we go as fast as possible (for normal logs)
    uint32_t delayIterations = slow ? 10000 : 0;

    // Loop through each character in the message
    for (int i = 0; i < maxBuffer; ++i) {
        // Stop at null terminator (end of string)
        if (message[i] == '\0') break;

        // Wait for the serial port to be ready before writing
        // This ensures we don't lose characters
        if (!waitForTransmitReady()) {
            // Timeout - serial port is not responding (may be disconnected)
            // Stop trying to send to prevent hang
            break;
        }

        // Write the character to the serial port data register
        loggingPort.write(message[i]);

        // If slow mode is enabled, add a delay to give the serial port time to process
        // This makes the transmission more reliable but slower
        if (slow && delayIterations > 0) { PalmyraOS::kernel::CPU::delay(delayIterations); }
    }
}

// ============================================================================
// FORMATTED LOGGING FUNCTION
// ============================================================================
/**
 * @brief Internal function to format and send a message
 *
 * This function uses vsnprintf to format a printf-style message,
 * then sends it via log_msg().
 *
 * @param slow Whether to use slow mode (delays between chars)
 * @param format Printf-style format string
 * @param ... Variable arguments for the format string
 */
void log_msgf(bool slow, const char* format, ...) {
    va_list args;
    va_start(args, format);

    // Buffer to hold the formatted message
    // 4096 bytes is large enough for most kernel logs
    char buffer[4096];

    // Format the message (like printf)
    // This converts the format string and arguments into a complete message
    vsnprintf(buffer, sizeof(buffer), format, args);

    va_end(args);

    // Send the formatted message to the serial port
    log_msg(slow, buffer);
}

// ============================================================================
// PUBLIC LOGGING API
// ============================================================================
/**
 * @brief Main logging function (called by LOG_ERROR, LOG_WARN, etc. macros)
 *
 * This function:
 * 1. Formats the user message with vsnprintf
 * 2. Prepends the log level, function name, and line number
 * 3. Appends a newline
 * 4. Sends everything to the serial port
 *
 * Example output:
 * ERROR [kernelEntry:150] Failed to initialize GDT
 * INFO  [kernel:200] System clock initialized at 250 Hz
 *
 * @param level The log level string ("ERROR", "WARN ", "INFO ", "DEBUG", "TRACE")
 * @param slow Whether to use slow transmission (for critical errors)
 * @param function The name of the function making the log call
 * @param line The line number in the source file
 * @param format Printf-style format string
 * @param ... Variable arguments for the format string
 */
void PalmyraOS::kernel::log(const char* level, bool slow, const char* function, uint32_t line, const char* format, ...) {
    // Step 1: Format the user's message
    va_list args;
    va_start(args, format);
    char buffer[4096];
    vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);

    // Step 2: Send a formatted log line with level, function, line number, and message
    // Format: "LEVEL [function:line] message\n"
    // The last parameter "\n" adds a newline at the end for readability
    log_msgf(slow, "%s [%s:%d] %s\n", level, function, line, buffer);
}
