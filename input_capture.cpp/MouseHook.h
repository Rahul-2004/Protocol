#ifndef MOUSEHOOK_H
#define MOUSEHOOK_H

#include <windows.h>

LRESULT CALLBACK MouseHookProc(int nCode, WPARAM wParam, LPARAM lParam);

bool InstallMouseHook();
bool UninstallMouseHook();

#endif // MOUSEHOOK_H
