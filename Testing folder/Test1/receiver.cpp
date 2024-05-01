#include <iostream>

int main() {
    std::cout << "This legacy receiver prototype has been superseded by the "
              << "top-level input_relay receiver mode.\n";
    std::cout << "Build with CMake and run:\n"
              << "  input_relay.exe receiver [rfcomm-port]\n";
    return 0;
}
