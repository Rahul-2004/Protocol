#include <winsock2.h>
#include <ws2bth.h>
#include <bthsdpdef.h>
#include <initguid.h>
#include <iostream>

#pragma comment(lib, "Ws2_32.lib")
#pragma comment(lib, "Bthprops.lib")

int main() {
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        std::cerr << "WSAStartup failed.\n";
        return 1;
    }

    SOCKET clientSocket = socket(AF_BTH, SOCK_STREAM, BTHPROTO_RFCOMM);
    if (clientSocket == INVALID_SOCKET) {
        std::cerr << "Failed to create socket.\n";
        WSACleanup();
        return 1;
    }

    SOCKADDR_BTH addr = { 0 };
    addr.addressFamily = AF_BTH;
    addr.btAddr = 0x0000A0510B50BA5D;  // <- Your receiver MAC
    addr.serviceClassId = GUID_NULL;
    addr.port = 4;  // <- must match receiver's port

    std::cout << "Connecting to receiver...\n";
    if (connect(clientSocket, (SOCKADDR*)&addr, sizeof(addr)) == SOCKET_ERROR) {
        std::cerr << "Connection failed: " << WSAGetLastError() << "\n";
        closesocket(clientSocket);
        WSACleanup();
        return 1;
    }

    const char* msg = "Hello over Bluetooth!";
    if (send(clientSocket, msg, strlen(msg), 0) == SOCKET_ERROR) {
        std::cerr << "Send failed: " << WSAGetLastError() << "\n";
    } else {
        std::cout << "Message sent.\n";
    }

    closesocket(clientSocket);
    WSACleanup();

    std::cout << "Done. Press Enter to exit...\n";
    std::cin.get();
    return 0;
}
