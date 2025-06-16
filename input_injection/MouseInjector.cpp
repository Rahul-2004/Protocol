#include <windows.h>
#include <iostream>
#include "MouseInjector.h"
using namespace std;

int screenWidth = GetSystemMetrics(SM_CXSCREEN);
int screenHeight = GetSystemMetrics(SM_CYSCREEN);


bool InjectMouseMoveAbsolute(int x , int y){
    INPUT input = {0};
    input.type = INPUT_MOUSE;
    input.mi.dx = (x*65535)/screenWidth;
    input.mi.dy = (y*65535)/screenHeight;
    input.mi.dwFlags = MOUSEEVENTF_MOVE | MOUSEEVENTF_ABSOLUTE;

    UINT sent = SendInput(1,&input , sizeof(INPUT));
    if(sent == 0){
        std::cerr << "Injection failed. Error: " << GetLastError() << std::endl;
        return false;
    }
    return true;
}

bool InjectMouseClick(MouseButton button) {
    INPUT input = {0};
    input.type = INPUT_MOUSE;

    // Press
    if (button == LEFT) {
        input.mi.dwFlags = MOUSEEVENTF_LEFTDOWN;
    } else if (button == RIGHT) {
        input.mi.dwFlags = MOUSEEVENTF_RIGHTDOWN;
    } 
    else if(button == MIDDLE){
        input.mi.dwFlags = MOUSEEVENTF_MIDDLEDOWN;
    }
    else {
        std::cerr << "Invalid mouse button.\n";
        return false;
    }

    if (SendInput(1, &input, sizeof(INPUT)) == 0) {
        std::cerr << "Mouse press failed. Error: " << GetLastError() << std::endl;
        return false;
    }

    ZeroMemory(&input, sizeof(INPUT)); // reset struct

    // Release
    input.type = INPUT_MOUSE;

    if (button == LEFT) {
        input.mi.dwFlags = MOUSEEVENTF_LEFTUP;
    } else if(button == RIGHT){
        input.mi.dwFlags = MOUSEEVENTF_RIGHTUP;
    }
    else {
        input.mi.dwFlags = MOUSEEVENTF_MIDDLEUP;
    }

    if (SendInput(1, &input, sizeof(INPUT)) == 0) {
        std::cerr << "Mouse release failed. Error: " << GetLastError() << std::endl;
        return false;
    }

    return true;
}

bool InjectMouseScroll(int amount){
    INPUT input = {0};
    input.type = INPUT_MOUSE;
    input.mi.dwFlags = MOUSEEVENTF_WHEEL;
    input.mi.mouseData = amount;
    if (SendInput(1, &input, sizeof(INPUT)) == 0) {
        std::cerr << "Mouse press failed. Error: " << GetLastError() << std::endl;
        return false;
    }
    return true;
}

