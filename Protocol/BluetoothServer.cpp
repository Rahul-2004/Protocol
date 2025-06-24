#include <winsock2.h>
#include <ws2bth.h>
#include <iostream>
#include "MousePacket.h"

#pragma comment(lib, "Ws2_32.lib")

int main() {
    // 1. Init Winsock
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData)) {
        std::cerr << "WSAStartup failed\n";
        return 1;
    }

    // 2. Create a Bluetooth socket
    SOCKET btSocket = socket(AF_BTH, SOCK_STREAM, BTHPROTO_RFCOMM);
    if (btSocket == INVALID_SOCKET) {
        std::cerr << "Socket creation failed\n";
        WSACleanup();
        return 1;
    }

    // 3. Set the server’s Bluetooth address manually
    BTH_ADDR btAddr = 0xA1B2C3D4E5F6;  // Replace with actual server address

    SOCKADDR_BTH sa = {};
    sa.addressFamily = AF_BTH;
    sa.btAddr = btAddr;
    sa.port = 0;  // Will use SDP or accept default (server must bind to BT_PORT_ANY)
    sa.serviceClassId = GUID_NULL; // Optional: match with server’s serviceClassId

    std::cout << "Connecting to Bluetooth server...\n";
    if (connect(btSocket, (SOCKADDR*)&sa, sizeof(sa)) == SOCKET_ERROR) {
        std::cerr << "Connection failed. Error: " << WSAGetLastError() << "\n";
        closesocket(btSocket);
        WSACleanup();
        return 1;
    }

    std::cout << "Connected.\n";

    // 4. Create dummy MousePacket
    MousePacket packet = {};
    packet.eventType = MOVE;
    packet.dx = 50;
    packet.dy = 100;
    packet.control = CTRL_LEFT;
    packet.data = 0;

    // 5. Serialize + Send
    uint8_t buffer[sizeof(MousePacket)];
    SerializeMousePacket(packet, buffer);

    int sent = send(btSocket, (const char*)buffer, sizeof(buffer), 0);
    if (sent == SOCKET_ERROR) {
        std::cerr << "Send failed. Error: " << WSAGetLastError() << "\n";
    } else {
        std::cout << "MousePacket sent (" << sent << " bytes)\n";
    }

    closesocket(btSocket);
    WSACleanup();
    return 0;
}
