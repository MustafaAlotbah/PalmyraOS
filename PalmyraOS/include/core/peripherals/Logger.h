

#pragma once

#include "core/definitions.h"

// ============================================================================
// LOGGER CONFIGURATION & MACROS
// ============================================================================
// These macros provide a convenient interface for logging at different severity
// levels (ERROR, WARN, INFO, DEBUG). They automatically capture the function name
// and line number for better debugging information.

#define DEBUG
#define DEBUG_ALL
//#define DEBUG_TRACE
#define FUNCTION_DEC __FUNCTION__  // or __PRETTY_FUNCTION__

#ifdef DEBUG

// ERROR logs are sent slowly (with delays) to ensure they're not lost
#define LOG_ERROR(...) PalmyraOS::kernel::log("ERROR", true, FUNCTION_DEC, __LINE__, __VA_ARGS__)

// WARN logs are also sent slowly for important warnings
#define LOG_WARN(...) PalmyraOS::kernel::log("WARN ", true, FUNCTION_DEC, __LINE__, __VA_ARGS__)

// INFO logs are sent at normal speed (no delays)
#define LOG_INFO(...) PalmyraOS::kernel::log("INFO ", false, FUNCTION_DEC, __LINE__, __VA_ARGS__)

#ifdef DEBUG_ALL
// DEBUG logs provide detailed information during development
#define LOG_DEBUG(...)  PalmyraOS::kernel::log("DEBUG", false, FUNCTION_DEC, __LINE__, __VA_ARGS__)
#else
#define LOG_DEBUG(...)  // Disabled when DEBUG_ALL is not defined
#endif

#ifdef DEBUG_TRACE
// TRACE logs provide the most detailed information (rarely used)
#define LOG_TRACE(...)  PalmyraOS::kernel::log("TRACE", false, FUNCTION_DEC, __LINE__, __VA_ARGS__)
#else
#define LOG_TRACE(...)  // Disabled when DEBUG_TRACE is not defined
#endif

#else
// When DEBUG is not defined, disable all logs except ERROR
#define LOG_INFO(...)
#define LOG_WARN(...)
#define LOG_ERROR(...) PalmyraOS::kernel::log("ERROR", FUNCTION_DEC, __LINE__, __VA_ARGS__)
#endif


namespace PalmyraOS::kernel
{

  /**
   * @brief Initialize the serial port for logging output
   * 
   * This function configures the COM1 serial port (0x3F8) to:
   * - Set baud rate to the specified rate (default: 115200)
   * - Configure 8 data bits, 1 stop bit, no parity (8N1)
   * - Enable FIFO buffering for smooth data transmission
   * - Enable RTS/DTR modem control signals
   * 
   * MUST be called early in kernel initialization (right after protected mode)
   * before any LOG_* macros are used. Otherwise, log output will be silently
   * discarded to prevent undefined behavior.
   * 
   * @param baudRate The baud rate to use (default: 115200)
   *                 Standard values: 9600, 19200, 38400, 57600, 115200
   * @return true if initialization was successful, false otherwise
   * 
   * @note In an OS kernel, we use serial ports because:
   *       - Simple hardware interface (just read/write to I/O ports)
   *       - No OS dependencies (works before any drivers are initialized)
   *       - Universal debugging interface (works in QEMU, VirtualBox, real hardware)
   *       - Can redirect output to a file for analysis
   */
  bool initializeSerialPort(uint32_t baudRate = 115200);

  /**
   * @brief Internal logging function (do not use directly)
   * 
   * This is the core logging function that formats and sends log messages to
   * the serial port. Use the LOG_ERROR, LOG_WARN, LOG_INFO, LOG_DEBUG macros
   * instead, as they automatically capture function names and line numbers.
   * 
   * @param level      Log level as string ("ERROR", "WARN ", "INFO ", "DEBUG", "TRACE")
   * @param slow       If true, adds delays between characters (for critical logs)
   *                   If false, sends characters as fast as possible
   * @param function   Name of the function making the log call
   * @param line       Line number in the source file
   * @param format     Printf-style format string
   * @param ...        Variable arguments to format
   */
  [[maybe_unused]] void log(const char* level, bool slow, const char* function, uint32_t line, const char* format, ...);

}