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
        // payload.dx/dy ∈ [0..1]
        int screenW = GetSystemMetrics(SM_CXSCREEN);
        int screenH = GetSystemMetrics(SM_CYSCREEN);
        int x = static_cast<int>(payload.dx * screenW);
        int y = static_cast<int>(payload.dy * screenH);
        SetCursorPos(x, y);
        return true;
    }

    case EVENT_CLICK: {
        MouseButton btn;
        if      (payload.buttonMask & 0x01) btn = LEFT;
        else if (payload.buttonMask & 0x02) btn = RIGHT;
        else if (payload.buttonMask & 0x04) btn = MIDDLE;
        else return false;

        if (pkt.actionFlag)
            return InjectMousePress(btn);
        else
            return InjectMouseRelease(btn);
    }

    case EVENT_SCROLL:
        return InjectMouseScroll(payload.data);

    default:
        std::cerr << "[Receiver] Unknown event: " << int(pkt.eventType) << "\n";
        return false;
    }
}

int main() {
    WSADATA wsa;
    if (WSAStartup(MAKEWORD(2,2), &wsa) != 0) return 1;

    serverSocket = socket(AF_BTH, SOCK_STREAM, BTHPROTO_RFCOMM);
    if (serverSocket == INVALID_SOCKET) return 1;

    SOCKADDR_BTH bindAddr = {};
    bindAddr.addressFamily = AF_BTH;
    bindAddr.port = BT_PORT_ANY;
    bind(serverSocket, (SOCKADDR*)&bindAddr, sizeof(bindAddr));

    int addrLen = sizeof(bindAddr);
    getsockname(serverSocket, (SOCKADDR*)&bindAddr, &addrLen);
    std::cout << "[Receiver] Listening on port: " << bindAddr.port << "\n";

    listen(serverSocket, 1);
    clientSocket = accept(serverSocket, NULL, NULL);
    std::cout << "[Receiver] Client connected!\n";

    InputEventPacket pkt;
    while (recv(clientSocket, (char*)&pkt, sizeof(pkt), 0) > 0) {
        HandleMousePacket(pkt);
    }

    closesocket(clientSocket);
    closesocket(serverSocket);
    WSACleanup();
    return 0;
}
