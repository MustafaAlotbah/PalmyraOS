

#pragma once

#include "core/definitions.h"


#define DEBUG

#ifdef DEBUG
#define LOG_INFO(...) PalmyraOS::kernel::log("INFO", __PRETTY_FUNCTION__, __LINE__, __VA_ARGS__)
#define LOG_WARN(...) PalmyraOS::kernel::log("WARN", __PRETTY_FUNCTION__, __LINE__, __VA_ARGS__)
#define LOG_ERROR(...) PalmyraOS::kernel::log("ERROR", __PRETTY_FUNCTION__, __LINE__, __VA_ARGS__)
#else
#define LOG_INFO(...)
#define LOG_WARN(...)
#define LOG_ERROR(...) PalmyraOS::kernel::log("ERROR", __PRETTY_FUNCTION__, __LINE__, __VA_ARGS__)
#endif


namespace PalmyraOS::kernel
{

  /* Do not use directly, rather use the macros */
  [[maybe_unused]] void log(const char* level, const char* function, uint32_t line, const char* format, ...);

}