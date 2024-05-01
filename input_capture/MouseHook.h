#ifndef MOUSEHOOK_H
#define MOUSEHOOK_H

#include <windows.h>

// Legacy name retained for older sample code. The implementation uses
// Windows Raw Input in non-exclusive INPUTSINK mode, not WH_MOUSE_LL hooks.
LRESULT CALLBACK MouseHookProc(int nCode, WPARAM wParam, LPARAM lParam);

bool InstallMouseHook();
bool UninstallMouseHook();

#endif // MOUSEHOOK_H
