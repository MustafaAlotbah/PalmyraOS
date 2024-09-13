
#include "core/std/error_handling.h"
#include "core/panic.h"
#include "libs/stdio.h"


#include <cstdarg>


// globals
PalmyraOS::kernel::runtime::ExceptionHandler lengthErrorHandler_     = nullptr;
PalmyraOS::kernel::runtime::ExceptionHandler outOfRangeErrorHandler_ = nullptr;
PalmyraOS::kernel::runtime::ExceptionHandler badFunctionCallHandler_ = nullptr;


/**
 * @namespace std
 * @brief Namespace for standard library extensions. (replacing -fno-exceptions)
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
	  if (lengthErrorHandler_) lengthErrorHandler_(msg);
	  else PalmyraOS::kernel::kernelPanic("Length Error: %s", msg);
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
	  if (outOfRangeErrorHandler_) outOfRangeErrorHandler_(buffer);
	  else PalmyraOS::kernel::kernelPanic("Out of Range Error: %s", buffer);

	  va_end(args);
  }

  void __throw_out_of_range(const char* message)
  {
	  // pass the formatted message to kernel panic
	  if (outOfRangeErrorHandler_) outOfRangeErrorHandler_(message);
	  else PalmyraOS::kernel::kernelPanic("Out of Range Error: %s", message);
  }

  void __throw_bad_optional_access()
  {
	  PalmyraOS::kernel::kernelPanic("bad_optional_access: attempted to access an empty optional");
  }

  /**
   * @brief Handles bad function call errors in the kernel.
   *
   * This function is used to handle bad function call errors in a kernel environment.
   * Instead of throwing exceptions, it handles the error in a way suitable for the environment.
   */
  void __throw_bad_function_call()
  {
	  if (badFunctionCallHandler_)
		  badFunctionCallHandler_(
			  "Bad Function Call: Attempted to invoke an uninitialized function object"
		  );
	  else PalmyraOS::kernel::kernelPanic("Bad Function Call: Attempted to invoke an uninitialized function object");
  }
}

void PalmyraOS::kernel::runtime::setLengthErrorHandler(ExceptionHandler handler)
{
	lengthErrorHandler_ = handler;
}

void PalmyraOS::kernel::runtime::setOutOfRangeHandler(ExceptionHandler handler)
{
	outOfRangeErrorHandler_ = handler;
}

void PalmyraOS::kernel::runtime::setBadFunctionCallHandler(ExceptionHandler handler)
{
	badFunctionCallHandler_ = handler;
}

extern "C" void abort(void)
{
	PalmyraOS::kernel::kernelPanic("Abort called!");
}