#include "MouseInjector.h"

#include <iostream>

namespace {
bool SendSingleInput(INPUT& input) {
    return SendInput(1, &input, sizeof(input)) == 1;
}

bool ResolveButton(MouseButton button, bool down, DWORD& flags, DWORD& mouseData) {
    mouseData = 0;
    switch (button) {
    case LEFT:
        flags = down ? MOUSEEVENTF_LEFTDOWN : MOUSEEVENTF_LEFTUP;
        return true;
    case RIGHT:
        flags = down ? MOUSEEVENTF_RIGHTDOWN : MOUSEEVENTF_RIGHTUP;
        return true;
    case MIDDLE:
        flags = down ? MOUSEEVENTF_MIDDLEDOWN : MOUSEEVENTF_MIDDLEUP;
        return true;
    case X1_BUTTON:
        flags = down ? MOUSEEVENTF_XDOWN : MOUSEEVENTF_XUP;
        mouseData = XBUTTON1;
        return true;
    case X2_BUTTON:
        flags = down ? MOUSEEVENTF_XDOWN : MOUSEEVENTF_XUP;
        mouseData = XBUTTON2;
        return true;
    default:
        return false;
    }
}

bool ButtonFromMask(uint8_t mask, MouseButton& button) {
    if (mask & BTN_LEFT) {
        button = LEFT;
        return true;
    }
    if (mask & BTN_RIGHT) {
        button = RIGHT;
        return true;
    }
    if (mask & BTN_MIDDLE) {
        button = MIDDLE;
        return true;
    }
    if (mask & BTN_X1) {
        button = X1_BUTTON;
        return true;
    }
    if (mask & BTN_X2) {
        button = X2_BUTTON;
        return true;
    }
    return false;
}
} // namespace

bool InjectMouseMoveAbsolute(int x, int y) {
    const int screenW = GetSystemMetrics(SM_CXSCREEN);
    const int screenH = GetSystemMetrics(SM_CYSCREEN);
    if (screenW <= 1 || screenH <= 1) {
        return false;
    }

    INPUT input{};
    input.type = INPUT_MOUSE;
    input.mi.dx = static_cast<LONG>((x * 65535.0f) / (screenW - 1));
    input.mi.dy = static_cast<LONG>((y * 65535.0f) / (screenH - 1));
    input.mi.dwFlags = MOUSEEVENTF_MOVE | MOUSEEVENTF_ABSOLUTE;
    return SendSingleInput(input);
}

bool InjectMouseMoveRelative(int dx, int dy) {
    INPUT input{};
    input.type = INPUT_MOUSE;
    input.mi.dx = static_cast<LONG>(dx);
    input.mi.dy = static_cast<LONG>(dy);
    input.mi.dwFlags = MOUSEEVENTF_MOVE;
    return SendSingleInput(input);
}

bool InjectMousePress(MouseButton button) {
    DWORD flags = 0;
    DWORD mouseData = 0;
    if (!ResolveButton(button, true, flags, mouseData)) {
        return false;
    }

    INPUT input{};
    input.type = INPUT_MOUSE;
    input.mi.dwFlags = flags;
    input.mi.mouseData = mouseData;
    return SendSingleInput(input);
}

bool InjectMouseRelease(MouseButton button) {
    DWORD flags = 0;
    DWORD mouseData = 0;
    if (!ResolveButton(button, false, flags, mouseData)) {
        return false;
    }

    INPUT input{};
    input.type = INPUT_MOUSE;
    input.mi.dwFlags = flags;
    input.mi.mouseData = mouseData;
    return SendSingleInput(input);
}

bool InjectMouseClick(MouseButton button) {
    return InjectMousePress(button) && InjectMouseRelease(button);
}

bool InjectMouseScroll(int amount) {
    INPUT input{};
    input.type = INPUT_MOUSE;
    input.mi.dwFlags = MOUSEEVENTF_WHEEL;
    input.mi.mouseData = static_cast<DWORD>(amount);
    return SendSingleInput(input);
}

bool InjectKeyboardKey(uint16_t virtualKey, bool down, bool extended) {
    INPUT input{};
    input.type = INPUT_KEYBOARD;
    input.ki.wVk = static_cast<WORD>(virtualKey);
    input.ki.dwFlags = down ? 0 : KEYEVENTF_KEYUP;
    if (extended) {
        input.ki.dwFlags |= KEYEVENTF_EXTENDEDKEY;
    }
    return SendSingleInput(input);
}

bool InjectInputPacket(const InputEventPacket& packet) {
    if (packet.type != TYPE_INPUT_EVENT || !ValidateChecksum(packet)) {
        return false;
    }

    switch (packet.eventType) {
    case EVENT_MOVE:
        return InjectMouseMoveRelative(GetPacketValueA(packet), GetPacketValueB(packet));

    case EVENT_CLICK: {
        MouseButton button{};
        if (!ButtonFromMask(GetPacketControl(packet), button)) {
            return false;
        }
        return packet.actionFlag ? InjectMousePress(button) : InjectMouseRelease(button);
    }

    case EVENT_SCROLL:
        return InjectMouseScroll(GetPacketValueA(packet));

    case EVENT_KEYBOARD:
        return InjectKeyboardKey(static_cast<uint16_t>(GetPacketValueA(packet)),
                                 packet.actionFlag != 0,
                                 (GetPacketFlags(packet) & 0x01) != 0);

    default:
        std::cerr << "Unknown input event type: " << static_cast<int>(packet.eventType) << "\n";
        return false;
    }
}
