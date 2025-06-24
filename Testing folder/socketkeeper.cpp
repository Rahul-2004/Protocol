#include "socketkeeper.h"
#include <ws2bth.h>
#include <bthsdpdef.h>
#include <iostream>

#pragma comment(lib, "Ws2_32.lib")
#pragma comment(lib, "Bthprops.lib")

SOCKET createBluetoothSocket(ULONGLONG btAddress, int port) {
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        std::cerr << "WSAStartup failed.\n";
        return INVALID_SOCKET;
    }

    SOCKET sock = socket(AF_BTH, SOCK_STREAM, BTHPROTO_RFCOMM);
    if (sock == INVALID_SOCKET) {
        std::cerr << "Socket creation failed.\n";
        WSACleanup();
        return INVALID_SOCKET;
    }

    SOCKADDR_BTH addr = { 0 };
    addr.addressFamily = AF_BTH;
    addr.btAddr = btAddress;
    addr.serviceClassId = GUID_NULL;
    addr.port = port;

    std::cout << "Connecting to Bluetooth server...\n";
    if (connect(sock, (SOCKADDR*)&addr, sizeof(addr)) == SOCKET_ERROR) {
        std::cerr << "Connection failed. Error: " << WSAGetLastError() << "\n";
        closesocket(sock);
        WSACleanup();
        return INVALID_SOCKET;
    }

    std::cout << "Bluetooth connection established.\n";
    return sock;
}

void closeBluetoothSocket(SOCKET sock) {
    if (sock != INVALID_SOCKET) {
        closesocket(sock);
    }
    WSACleanup();
    std::cout << "Bluetooth socket closed.\n";
}
