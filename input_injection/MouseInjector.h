#pragma once

#include <windows.h>
#include <cstdint>

#include "../Protocol/MousePacket.h"

enum MouseButton { LEFT, RIGHT, MIDDLE, X1_BUTTON, X2_BUTTON };

bool InjectMouseMoveAbsolute(int x, int y);
bool InjectMouseMoveRelative(int dx, int dy);
bool InjectMousePress(MouseButton button);
bool InjectMouseRelease(MouseButton button);
bool InjectMouseClick(MouseButton button);
bool InjectMouseScroll(int amount);
bool InjectKeyboardKey(uint16_t virtualKey, bool down, bool extended);
bool InjectInputPacket(const InputEventPacket& packet);
