#pragma once


namespace PalmyraOS::kernel
{


  /**
 * Causes the system to halt with a formatted panic message.
 *
 * Similar to `kernelPanic`, this function is used when the kernel encounters an unrecoverable error
 * and needs to halt. However, this function allows the message to be formatted like printf,
 * supporting a variable number of arguments to produce a formatted string.
 * This is useful for including detailed error information (e.g., values of variables at the time
 * of the error) in the panic message.
 *
 * @param format A format string that contains the text to be written. This string can include
 *               format specifiers that are replaced by the values specified in subsequent
 *               additional arguments and formatted as requested.
 * @param ... Variable arguments providing data to be formatted according to the format string.
 */
  void kernelPanic(const char* format, ...);

}



