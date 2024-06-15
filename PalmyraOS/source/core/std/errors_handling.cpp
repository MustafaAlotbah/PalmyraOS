
#include "core/panic.h"
#include "libs/stdio.h"

#include <cstdarg>


/**
 * @namespace std
 * @brief Namespace for standard library extensions.
 */
namespace std
{

  /**
   * @brief Handles length errors in the kernel.
   *
   * This function is used to handle length errors in a kernel environment.
   * Instead of throwing exceptions, it handles the error in a way suitable for the environment.
   *
   * @param msg The error message to be displayed.
   */
  void __throw_length_error(const char* msg)
  {
	  PalmyraOS::kernel::kernelPanic("Length Error: %s", msg);
  }

  /**
   * @brief Handles out of range errors with formatted message.
   *
   * This function is used to handle out of range errors in a kernel environment.
   * It formats the error message using a variable argument list and handles the error appropriately.
   *
   * @param fmt The format string for the error message.
   * @param ... Variable arguments for the format string.
   */
  void __throw_out_of_range_fmt(const char* fmt, ...)
  {
	  va_list args;
	  va_start(args, fmt);

	  // format the incoming error message
	  char buffer[1024];
	  vsnprintf(buffer, sizeof(buffer), fmt, args);

	  // pass the formatted message to kernel panic
	  PalmyraOS::kernel::kernelPanic("Out of Range Error: %s", buffer);

	  va_end(args);
  }

}