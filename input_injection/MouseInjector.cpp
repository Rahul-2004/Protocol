// MouseInjector.cpp
#include "MouseInjector.h"
#include <windows.h>
#include <iostream>

bool InjectMouseMoveAbsolute(int x, int y) {
    // Convert pixel coordinates to normalized [0..65535]
    int screenW = GetSystemMetrics(SM_CXSCREEN);
    int screenH = GetSystemMetrics(SM_CYSCREEN);
    // Protect against divide-by-zero
    if (screenW <= 1 || screenH <= 1) return false;
    
    // Normalize
    float fx = (x * 65535.0f) / (screenW - 1);
    float fy = (y * 65535.0f) / (screenH - 1);
    
    INPUT inp;
    ZeroMemory(&inp, sizeof(inp));
    inp.type = INPUT_MOUSE;
    inp.mi.dx = static_cast<LONG>(fx);
    inp.mi.dy = static_cast<LONG>(fy);
    inp.mi.dwFlags = MOUSEEVENTF_MOVE | MOUSEEVENTF_ABSOLUTE;
    
    UINT sent = SendInput(1, &inp, sizeof(inp));
    return sent == 1;
}

bool InjectMousePress(MouseButton button) {
    INPUT inp;
    ZeroMemory(&inp, sizeof(inp));
    inp.type = INPUT_MOUSE;
    
    switch (button) {
        case LEFT:
            inp.mi.dwFlags = MOUSEEVENTF_LEFTDOWN;
            break;
        case RIGHT:
            inp.mi.dwFlags = MOUSEEVENTF_RIGHTDOWN;
            break;
        case MIDDLE:
            inp.mi.dwFlags = MOUSEEVENTF_MIDDLEDOWN;
            break;
        default:
            return false;
    }
    
    UINT sent = SendInput(1, &inp, sizeof(inp));
    return sent == 1;
}

bool InjectMouseRelease(MouseButton button) {
    INPUT inp;
    ZeroMemory(&inp, sizeof(inp));
    inp.type = INPUT_MOUSE;
    
    switch (button) {
        case LEFT:
            inp.mi.dwFlags = MOUSEEVENTF_LEFTUP;
            break;
        case RIGHT:
            inp.mi.dwFlags = MOUSEEVENTF_RIGHTUP;
            break;
        case MIDDLE:
            inp.mi.dwFlags = MOUSEEVENTF_MIDDLEUP;
            break;
        default:
            return false;
    }
    
    UINT sent = SendInput(1, &inp, sizeof(inp));
    return sent == 1;
}

bool InjectMouseClick(MouseButton button) {
    // Simple press + release
    bool down = InjectMousePress(button);
    bool up   = InjectMouseRelease(button);
    return down && up;
}

bool InjectMouseScroll(int amount) {
    INPUT inp;
    ZeroMemory(&inp, sizeof(inp));
    inp.type = INPUT_MOUSE;
    inp.mi.dwFlags = MOUSEEVENTF_WHEEL;
    inp.mi.mouseData = amount;
    
    UINT sent = SendInput(1, &inp, sizeof(inp));
    return sent == 1;
}
