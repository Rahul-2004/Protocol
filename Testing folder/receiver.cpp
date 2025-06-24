#include "receiver.h"
#include <ws2bth.h>
#include <bthsdpdef.h>
#include <iostream>

#pragma comment(lib, "Ws2_32.lib")
#pragma comment(lib, "Bthprops.lib")

SOCKET setupBluetoothListener(int port) {
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        std::cerr << "WSAStartup failed.\n";
        return INVALID_SOCKET;
    }

    SOCKET serverSocket = socket(AF_BTH, SOCK_STREAM, BTHPROTO_RFCOMM);
    if (serverSocket == INVALID_SOCKET) {
        std::cerr << "Socket creation failed.\n";
        WSACleanup();
        return INVALID_SOCKET;
    }

    SOCKADDR_BTH addr = {};
    addr.addressFamily = AF_BTH;
    addr.port = port;

    if (bind(serverSocket, (SOCKADDR*)&addr, sizeof(addr)) == SOCKET_ERROR) {
        std::cerr << "Bind failed. Error: " << WSAGetLastError() << "\n";
        closesocket(serverSocket);
        WSACleanup();
        return INVALID_SOCKET;
    }

    if (listen(serverSocket, 1) == SOCKET_ERROR) {
        std::cerr << "Listen failed. Error: " << WSAGetLastError() << "\n";
        closesocket(serverSocket);
        WSACleanup();
        return INVALID_SOCKET;
    }

    std::cout << "Listening for Bluetooth connections on port " << port << "...\n";
    return serverSocket;
}

void acceptBluetoothConnection(SOCKET serverSocket, SOCKET& clientSocket) {
    SOCKADDR_BTH clientAddr = {};
    int addrLen = sizeof(clientAddr);

    clientSocket = accept(serverSocket, (SOCKADDR*)&clientAddr, &addrLen);
    if (clientSocket == INVALID_SOCKET) {
        std::cerr << "Accept failed. Error: " << WSAGetLastError() << "\n";
    } else {
        std::cout << "Client connected.\n";
    }
}

void receiveData(SOCKET clientSocket) {
    char buffer[1024];
    int bytesReceived;

    while ((bytesReceived = recv(clientSocket, buffer, sizeof(buffer) - 1, 0)) > 0) {
        buffer[bytesReceived] = '\0'; // Null-terminate
        std::cout << "Received: " << buffer << "\n";
    }

    if (bytesReceived == SOCKET_ERROR) {
        std::cerr << "Receive failed. Error: " << WSAGetLastError() << "\n";
    } else {
        std::cout << "Connection closed by client.\n";
    }
}

void cleanup(SOCKET serverSocket, SOCKET clientSocket) {
    if (clientSocket != INVALID_SOCKET) closesocket(clientSocket);
    if (serverSocket != INVALID_SOCKET) closesocket(serverSocket);
    WSACleanup();
    std::cout << "Sockets closed and cleaned up.\n";
}
