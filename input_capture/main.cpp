#include <iostream>
#include <windows.h>

#include "MouseHook.h"

int main() {
    if (!InstallMouseHook()) {
        std::cerr << "Failed to install Raw Input capture.\n";
        return 1;
    }

    std::cout << "Raw Input capture running. Press ESC in this console to exit.\n";

    MSG msg;
    while ((GetAsyncKeyState(VK_ESCAPE) & 0x8000) == 0) {
        while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
        Sleep(10);
    }

    UninstallMouseHook();
    return 0;
}
