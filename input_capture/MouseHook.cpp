#include "MouseHook.h"

#include <cstdint>
#include <iostream>
#include <vector>

namespace {
HWND rawInputWindow = nullptr;
constexpr wchar_t kWindowClassName[] = L"CrossPcRawInputSampleWindow";

uint8_t ButtonMaskFromRawFlags(USHORT flags) {
    if (flags & (RI_MOUSE_LEFT_BUTTON_DOWN | RI_MOUSE_LEFT_BUTTON_UP)) {
        return 0x01;
    }
    if (flags & (RI_MOUSE_RIGHT_BUTTON_DOWN | RI_MOUSE_RIGHT_BUTTON_UP)) {
        return 0x02;
    }
    if (flags & (RI_MOUSE_MIDDLE_BUTTON_DOWN | RI_MOUSE_MIDDLE_BUTTON_UP)) {
        return 0x04;
    }
    if (flags & (RI_MOUSE_BUTTON_4_DOWN | RI_MOUSE_BUTTON_4_UP)) {
        return 0x08;
    }
    if (flags & (RI_MOUSE_BUTTON_5_DOWN | RI_MOUSE_BUTTON_5_UP)) {
        return 0x10;
    }
    return 0;
}

bool IsButtonDown(USHORT flags) {
    return (flags & (RI_MOUSE_LEFT_BUTTON_DOWN |
                     RI_MOUSE_RIGHT_BUTTON_DOWN |
                     RI_MOUSE_MIDDLE_BUTTON_DOWN |
                     RI_MOUSE_BUTTON_4_DOWN |
                     RI_MOUSE_BUTTON_5_DOWN)) != 0;
}

void HandleRawInput(LPARAM lParam) {
    UINT size = 0;
    if (GetRawInputData(reinterpret_cast<HRAWINPUT>(lParam), RID_INPUT, nullptr,
                        &size, sizeof(RAWINPUTHEADER)) != 0 || size == 0) {
        return;
    }

    std::vector<BYTE> buffer(size);
    if (GetRawInputData(reinterpret_cast<HRAWINPUT>(lParam), RID_INPUT, buffer.data(),
                        &size, sizeof(RAWINPUTHEADER)) != size) {
        return;
    }

    const auto* raw = reinterpret_cast<const RAWINPUT*>(buffer.data());
    if (raw->header.dwType == RIM_TYPEMOUSE) {
        const RAWMOUSE& mouse = raw->data.mouse;
        if (mouse.lLastX != 0 || mouse.lLastY != 0) {
            std::cout << "Raw mouse move dx=" << mouse.lLastX
                      << " dy=" << mouse.lLastY << "\n";
        }

        const USHORT flags = mouse.usButtonFlags;
        const uint8_t buttonMask = ButtonMaskFromRawFlags(flags);
        if (buttonMask != 0) {
            std::cout << "Raw mouse button mask=0x" << std::hex
                      << static_cast<int>(buttonMask) << std::dec
                      << (IsButtonDown(flags) ? " down" : " up") << "\n";
        }
        if (flags & RI_MOUSE_WHEEL) {
            std::cout << "Raw mouse wheel delta="
                      << static_cast<SHORT>(mouse.usButtonData) << "\n";
        }
    } else if (raw->header.dwType == RIM_TYPEKEYBOARD) {
        const RAWKEYBOARD& keyboard = raw->data.keyboard;
        const bool down = (keyboard.Flags & RI_KEY_BREAK) == 0;
        std::cout << "Raw keyboard vk=" << keyboard.VKey
                  << (down ? " down" : " up") << "\n";
    }
}

LRESULT CALLBACK RawInputWndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
    case WM_INPUT:
        HandleRawInput(lParam);
        return 0;
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    default:
        return DefWindowProc(hwnd, message, wParam, lParam);
    }
}
} // namespace

LRESULT CALLBACK MouseHookProc(int, WPARAM, LPARAM) {
    return 0;
}

bool InstallMouseHook() {
    if (rawInputWindow != nullptr) {
        return true;
    }

    WNDCLASSW wc{};
    wc.lpfnWndProc = RawInputWndProc;
    wc.hInstance = GetModuleHandle(nullptr);
    wc.lpszClassName = kWindowClassName;
    RegisterClassW(&wc);

    rawInputWindow = CreateWindowExW(0,
                                     kWindowClassName,
                                     L"Cross-PC Raw Input Capture",
                                     0,
                                     0,
                                     0,
                                     0,
                                     0,
                                     HWND_MESSAGE,
                                     nullptr,
                                     wc.hInstance,
                                     nullptr);
    if (rawInputWindow == nullptr) {
        return false;
    }

    RAWINPUTDEVICE devices[2]{};
    devices[0].usUsagePage = 0x01;
    devices[0].usUsage = 0x02;
    devices[0].dwFlags = RIDEV_INPUTSINK;
    devices[0].hwndTarget = rawInputWindow;

    devices[1].usUsagePage = 0x01;
    devices[1].usUsage = 0x06;
    devices[1].dwFlags = RIDEV_INPUTSINK;
    devices[1].hwndTarget = rawInputWindow;

    if (!RegisterRawInputDevices(devices, 2, sizeof(RAWINPUTDEVICE))) {
        DestroyWindow(rawInputWindow);
        rawInputWindow = nullptr;
        return false;
    }

    std::cout << "Raw Input capture installed in non-exclusive INPUTSINK mode.\n";
    std::cout << "Local OS input is not blocked because RIDEV_NOLEGACY is not used.\n";
    return true;
}

bool UninstallMouseHook() {
    if (rawInputWindow == nullptr) {
        return true;
    }

    RAWINPUTDEVICE devices[2]{};
    devices[0].usUsagePage = 0x01;
    devices[0].usUsage = 0x02;
    devices[0].dwFlags = RIDEV_REMOVE;
    devices[0].hwndTarget = nullptr;

    devices[1].usUsagePage = 0x01;
    devices[1].usUsage = 0x06;
    devices[1].dwFlags = RIDEV_REMOVE;
    devices[1].hwndTarget = nullptr;

    RegisterRawInputDevices(devices, 2, sizeof(RAWINPUTDEVICE));
    DestroyWindow(rawInputWindow);
    rawInputWindow = nullptr;
    return true;
}
