#include <iostream>
#include <windows.h>
#include "MouseInjector.h"

int main() {
    std::cout << "Starting mouse injection test..." << std::endl;

    // Move to (300, 300)
    if (InjectMouseMoveAbsolute(500, 500)) {
        std::cout << "Mouse moved to (300, 300)" << std::endl;
    }

    Sleep(500); // Wait 0.5 seconds

    // Left click
    if (InjectMouseClick(LEFT)) {
        std::cout << "Left click performed" << std::endl;
    }

    Sleep(500);

    // Right click
    if (InjectMouseClick(RIGHT)) {
        std::cout << "Right click performed" << std::endl;
    }

    Sleep(500);

    // Middle click
    if (InjectMouseClick(MIDDLE)) {
        std::cout << "Middle click performed" << std::endl;
    }

    Sleep(500);

    // Scroll up
    if (InjectMouseScroll(240)) {
        std::cout << "Scrolled up" << std::endl;
    }

    Sleep(500);

    // Scroll down
    if (InjectMouseScroll(-120)) {
        std::cout << "Scrolled down" << std::endl;
    }

    std::cout << "Test completed." << std::endl;
    return 0;
}
