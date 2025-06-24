#pragma once

#include <windows.h>


enum MouseButton {
    LEFT,
    RIGHT,
    MIDDLE
};

bool InjectMouseMoveAbsolute(int x, int y);
bool InjectMouseClick(MouseButton button);
bool InjectMouseScroll(int amount);

