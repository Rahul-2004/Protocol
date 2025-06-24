#include "transmitdata.h"
#include <iostream>

bool sendDataOverBluetooth(SOCKET sock, const std::string& data) {
    int sent = send(sock, data.c_str(), static_cast<int>(data.size()), 0);
    if (sent == SOCKET_ERROR) {
        std::cerr << "Send failed. Error: " << WSAGetLastError() << "\n";
        return false;
    }
    std::cout << "Sent: " << data << "\n";
    return true;
}
