
#pragma once

#include "core/definitions.h"


namespace PalmyraOS::kernel::runtime {

    typedef void (*ExceptionHandler)(const char* message);

    void setLengthErrorHandler(ExceptionHandler handler);
    void setOutOfRangeHandler(ExceptionHandler handler);
    void setBadFunctionCallHandler(ExceptionHandler handler);

}  // namespace PalmyraOS::kernel::runtime