

#pragma once

#include "core/definitions.h"


#define DEBUG
#define DEBUG_ALL
//#define DEBUG_TRACE
#define FUNCTION_DEC __FUNCTION__  // or __PRETTY_FUNCTION__

#ifdef DEBUG

#define LOG_ERROR(...) PalmyraOS::kernel::log("ERROR", true, FUNCTION_DEC, __LINE__, __VA_ARGS__)
#define LOG_WARN(...) PalmyraOS::kernel::log("WARN ", true, FUNCTION_DEC, __LINE__, __VA_ARGS__)
#define LOG_INFO(...) PalmyraOS::kernel::log("INFO ", false, FUNCTION_DEC, __LINE__, __VA_ARGS__)

#ifdef DEBUG_ALL
#define LOG_DEBUG(...)  PalmyraOS::kernel::log("DEBUG", false, FUNCTION_DEC, __LINE__, __VA_ARGS__)
#else
#define LOG_DEBUG(...)
#endif

#ifdef DEBUG_TRACE
#define LOG_TRACE(...)  PalmyraOS::kernel::log("TRACE", false, FUNCTION_DEC, __LINE__, __VA_ARGS__)
#else
#define LOG_TRACE(...)
#endif

#else
#define LOG_INFO(...)
#define LOG_WARN(...)
#define LOG_ERROR(...) PalmyraOS::kernel::log("ERROR", FUNCTION_DEC, __LINE__, __VA_ARGS__)
#endif


namespace PalmyraOS::kernel
{

  /* Do not use directly, rather use the macros */
  [[maybe_unused]] void log(const char* level, bool slow, const char* function, uint32_t line, const char* format, ...);

}