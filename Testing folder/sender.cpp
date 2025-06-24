#include <winsock2.h>
#include <ws2bth.h>
#include <bthsdpdef.h>
#include <initguid.h>
#include <iostream>

#pragma comment(lib, "Ws2_32.lib")
#pragma comment(lib, "Bthprops.lib")

int main() {
    WSADATA wsaData;
    WSAStartup(MAKEWORD(2, 2), &wsaData);

    SOCKET clientSocket = socket(AF_BTH, SOCK_STREAM, BTHPROTO_RFCOMM);
    if (clientSocket == INVALID_SOCKET) {
        std::cerr << "Socket creation failed\n";
        WSACleanup();
        return 1;
    }

    SOCKADDR_BTH serverAddr = {};
    serverAddr.addressFamily = AF_BTH;

    // 👇 Replace with your receiver's address (convert A0-51-0B-50-BA-5D to 0x0000A0510B50BA5D)
    serverAddr.btAddr = 0x0000A0510B50BA5D;
    serverAddr.serviceClassId = GUID_NULL;
    serverAddr.port = 4; // Must match receiver

    std::cout << "Connecting to receiver...\n";
    if (connect(clientSocket, (SOCKADDR*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR) {
        std::cerr << "Connection failed: " << WSAGetLastError() << "\n";
        closesocket(clientSocket);
        WSACleanup();
        return 1;
    }

    const char* message = "Hello from sender!";
    int sent = send(clientSocket, A lomessage, strlen(message), 0);
    if (sent == SOCKET_ERROR) {
        std::cerr << "Send failed: " << WSAGetLastError() << "\n";
    } else {
        std::cout << "Message sent!\n";
    }

    closesocket(clientSocket);
    WSACleanup();
    return 0;
}
