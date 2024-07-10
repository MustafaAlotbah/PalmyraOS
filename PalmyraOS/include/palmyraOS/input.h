
#pragma once

#include <cstddef>
#include <cstdint>


struct KeyboardEvent
{
	char key         = 0;
	bool pressed     = false;    // 1 pressed, 0 released
	bool isCtrlDown  = false;
	bool isShiftDown = false;
	bool isAltDown   = false;
};