// MouseInjector.h

#pragma once
#include <windows.h>

enum MouseButton { LEFT, RIGHT, MIDDLE };

bool InjectMouseMoveAbsolute(int x, int y);
bool InjectMousePress(MouseButton button);
bool InjectMouseRelease(MouseButton button);
bool InjectMouseClick(MouseButton button);   // convenience: press+release
bool InjectMouseScroll(int amount);
