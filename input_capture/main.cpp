#include <iostream>
#include <windows.h>
#include "MouseHook.h"
#include <conio.h>

int main() {
    if (!InstallMouseHook()) {
        std::cerr << "Failed to install mouse hook.\n";
        return 1;
    }

    std::cout << "Mouse hook installed. Press ESC to exit.\n";

    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        if (msg.message == WM_KEYDOWN && msg.wParam == VK_ESCAPE) {
            break;
        }
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    UninstallMouseHook();
    std::cout << "Mouse hook uninstalled. Exiting.\n";

    return 0;
}
