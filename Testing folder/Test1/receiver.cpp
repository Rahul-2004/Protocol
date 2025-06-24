// receiver.cpp

#include <winsock2.h>
#include <ws2bth.h>
#include <bthsdpdef.h>
#include <initguid.h>
#include <iostream>
#include "../../Protocol/MousePacket.h"
#include "../../input_injection/MouseInjector.h"

#pragma comment(lib, "Ws2_32.lib")
#pragma comment(lib, "Bthprops.lib")
#pragma comment(lib, "User32.lib")

SOCKET serverSocket = INVALID_SOCKET;
SOCKET clientSocket = INVALID_SOCKET;

bool HandleMousePacket(const InputEventPacket& pkt) {
    MouseEventPayload payload{};
    DeserializeMousePayload(pkt.payload, payload);

    switch (pkt.eventType) {
    case EVENT_MOVE: {
        int screenW = GetSystemMetrics(SM_CXSCREEN);
        int screenH = GetSystemMetrics(SM_CYSCREEN);

        int x = static_cast<int>(payload.dx * screenW);
        int y = static_cast<int>(payload.dy * screenH);

        SetCursorPos(x, y);
        return true;
    }

    case EVENT_CLICK: {
        if (payload.buttonMask & 0x01)
            return InjectMouseClick(pkt.actionFlag ? LEFT : LEFT);
        if (payload.buttonMask & 0x02)
            return InjectMouseClick(pkt.actionFlag ? RIGHT : RIGHT);
        if (payload.buttonMask & 0x04)
            return InjectMouseClick(pkt.actionFlag ? MIDDLE : MIDDLE);
        return false;
    }

    case EVENT_SCROLL:
        return InjectMouseScroll(payload.data);

    default:
        std::cerr << "[Receiver] Unknown event type\n";
        return false;
}

}

int main() {
    WSADATA wsa;
    if (WSAStartup(MAKEWORD(2,2), &wsa) != 0) {
        std::cerr << "[Receiver] WSAStartup failed\n";
        return 1;
    }

    serverSocket = socket(AF_BTH, SOCK_STREAM, BTHPROTO_RFCOMM);
    if (serverSocket == INVALID_SOCKET) {
        std::cerr << "[Receiver] socket() failed\n";
        WSACleanup();
        return 1;
    }

    SOCKADDR_BTH bindAddr = {};
    bindAddr.addressFamily = AF_BTH;
    bindAddr.port = BT_PORT_ANY;

    if (bind(serverSocket, (SOCKADDR*)&bindAddr, sizeof(bindAddr)) == SOCKET_ERROR) {
        std::cerr << "[Receiver] bind() failed\n";
        closesocket(serverSocket);
        WSACleanup();
        return 1;
    }

    int addrLen = sizeof(bindAddr);
    getsockname(serverSocket, (SOCKADDR*)&bindAddr, &addrLen);
    std::cout << "[Receiver] Listening on port: " << bindAddr.port << "\n";

    listen(serverSocket, 1);
    std::cout << "[Receiver] Waiting for sender...\n";

    clientSocket = accept(serverSocket, NULL, NULL);
    if (clientSocket == INVALID_SOCKET) {
        std::cerr << "[Receiver] accept() failed\n";
        closesocket(serverSocket);
        WSACleanup();
        return 1;
    }

    std::cout << "[Receiver] Client connected!\n";

    InputEventPacket pkt;
    while (true) {
        int received = recv(clientSocket, (char*)&pkt, sizeof(pkt), 0);
        if (received <= 0) break;

        HandleMousePacket(pkt);
    }

    closesocket(clientSocket);
    closesocket(serverSocket);
    WSACleanup();
    std::cout << "[Receiver] Exiting...\n";
    return 0;
}
