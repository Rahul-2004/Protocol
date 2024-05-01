#include <iostream>

int main() {
    std::cout << "This legacy sender prototype has been superseded by the "
              << "top-level input_relay sender mode.\n";
    std::cout << "Build with CMake and run:\n"
              << "  input_relay.exe sender <receiver-bt-address> [rfcomm-port]\n";
    return 0;
}
