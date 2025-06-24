#include <iostream>
#include <thread>
#include <chrono>

#include "socketkeeper.h"
#include "transmitdata.h"

int main() {
    ULONGLONG receiverMAC = 0x0000A0510B50BA5D; // Replace with real MAC
    int port = 4; // Must match receiver port

    SOCKET btSocket = createBluetoothSocket(receiverMAC, port);
    if (btSocket == INVALID_SOCKET) return 1;

    // Example loop to keep sending data
    for (int i = 0; i < 10; ++i) {
        std::string data = "Packet " + std::to_string(i);
        sendDataOverBluetooth(btSocket, data);
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    closeBluetoothSocket(btSocket);
    return 0;
}
