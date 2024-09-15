

#pragma once

#include "core/definitions.h"


#define DEBUG
#define DEBUG_ALL
#define FUNCTION_DEC __FUNCTION__  // or __PRETTY_FUNCTION__

#ifdef DEBUG

#define LOG_INFO(...) PalmyraOS::kernel::log("INFO ", FUNCTION_DEC, __LINE__, __VA_ARGS__)
#define LOG_WARN(...) PalmyraOS::kernel::log("WARN ", FUNCTION_DEC, __LINE__, __VA_ARGS__)
#define LOG_ERROR(...) PalmyraOS::kernel::log("ERROR", FUNCTION_DEC, __LINE__, __VA_ARGS__)

#ifdef DEBUG_ALL
#define LOG_DEBUG(...)  PalmyraOS::kernel::log("DEBUG", FUNCTION_DEC, __LINE__, __VA_ARGS__)
#else
#define LOG_DEBUG(...)
#endif

#else
#define LOG_INFO(...)
#define LOG_WARN(...)
#define LOG_ERROR(...) PalmyraOS::kernel::log("ERROR", FUNCTION_DEC, __LINE__, __VA_ARGS__)
#endif


namespace PalmyraOS::kernel
{

  /* Do not use directly, rather use the macros */
  [[maybe_unused]] void log(const char* level, const char* function, uint32_t line, const char* format, ...);

}