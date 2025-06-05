#include "MouseHook.h"
#include <iostream>
#include <windows.h>
using namespace std;

static HHOOK mouseHook = NULL;//this is like the thing you create and pass to a function regarding what you want . so like a mousehook is like a token / reference that can tell a particular function that you have to tell me the result based on this thing which is the token 

LRESULT CALLBACK MouseHookProc(int nCode, WPARAM wParam, LPARAM lParam) {
    if (nCode >= 0) {
        MSLLHOOKSTRUCT* mouseInfo = (MSLLHOOKSTRUCT*)lParam;
        std::cout << "Mouse position: " << mouseInfo->pt.x << ", " << mouseInfo->pt.y << "\n";

        switch (wParam) {
            case WM_LBUTTONDBLCLK: std::cout << "Left button double-clicked\n"; break;
            case WM_MBUTTONDBLCLK: std::cout << "Middle button double-clicked\n"; break;
            case WM_RBUTTONDBLCLK: std::cout << "Right button double-clicked\n"; break;
            case WM_MOUSEMOVE: std::cout << "Mouse moved\n"; break;
            case WM_LBUTTONDOWN: std::cout << "Left button down\n"; break;
            case WM_RBUTTONDOWN: std::cout << "Right button down\n"; break;
            case WM_MBUTTONDOWN: std::cout << "Middle button down\n"; break;
            case WM_LBUTTONUP: std::cout << "Left button up\n"; break;
            case WM_RBUTTONUP: std::cout << "Right button up\n"; break;
            case WM_MBUTTONUP: std::cout << "Middle button up\n"; break;
            default: break;
        }
    }

    return CallNextHookEx(NULL, nCode, wParam, lParam);
}

bool InstallMouseHook() {
    mouseHook = SetWindowsHookEx(WH_MOUSE_LL, MouseHookProc, NULL, 0);
    return mouseHook != NULL;
}

bool UninstallMouseHook() {
    if (mouseHook) {  // if it's installed
        bool result = UnhookWindowsHookEx(mouseHook);
        if (result) {
            mouseHook = NULL;
        }
        return result;
    }
    return false; 
}

