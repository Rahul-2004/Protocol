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

    SOCKET serverSocket = socket(AF_BTH, SOCK_STREAM, BTHPROTO_RFCOMM);
    if (serverSocket == INVALID_SOCKET) {
        std::cerr << "Socket creation failed: " << WSAGetLastError() << "\n";
        WSACleanup();
        return 1;
    }

    SOCKADDR_BTH serverAddr = { 0 };
    serverAddr.addressFamily = AF_BTH;
    serverAddr.btAddr = 0; // Listen on any local BT device
    serverAddr.serviceClassId = GUID_NULL;
    serverAddr.port = BT_PORT_ANY;

    if (bind(serverSocket, (SOCKADDR*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR) {
        std::cerr << "Bind failed: " << WSAGetLastError() << "\n";
        closesocket(serverSocket);
        WSACleanup();
        return 1;
    }

    int addrLen = sizeof(serverAddr);
    getsockname(serverSocket, (SOCKADDR*)&serverAddr, &addrLen);
    std::cout << "Listening on port: " << (int)serverAddr.port << "\n";

    if (listen(serverSocket, 1) == SOCKET_ERROR) {
        std::cerr << "Listen failed.\n";
        closesocket(serverSocket);
        WSACleanup();
        return 1;
    }

    std::cout << "Waiting for incoming Bluetooth connection...\n";
    SOCKET clientSocket = accept(serverSocket, NULL, NULL);
    if (clientSocket == INVALID_SOCKET) {
        std::cerr << "Accept failed.\n";
        closesocket(serverSocket);
        WSACleanup();
        return 1;
    }

    char buffer[1024];
    int bytesRead = recv(clientSocket, buffer, sizeof(buffer) - 1, 0);
    if (bytesRead > 0) {
        buffer[bytesRead] = '\0';
        std::cout << "Received: " << buffer << "\n";
    } else {
        std::cerr << "Recv failed.\n";
    }

    closesocket(clientSocket);
    closesocket(serverSocket);
    WSACleanup();

    std::cout << "Press Enter to exit...\n";
    std::cin.get();

    return 0;
}
