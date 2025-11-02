
#pragma once

#include <cstddef>
#include <cstdint>


struct KeyboardEvent {
    char key         = 0;
    bool pressed     = false;  // 1 pressed, 0 released
    bool isCtrlDown  = false;
    bool isShiftDown = false;
    bool isAltDown   = false;
    bool isValid     = false;
};

struct MouseEvent {
    int x             = 0;
    int y             = 0;
    bool isLeftDown   = false;
    bool isRightDown  = false;
    bool isMiddleDown = false;
    bool isEvent      = false;  // if event, it was queued, if false, only position and left key are valid.
};