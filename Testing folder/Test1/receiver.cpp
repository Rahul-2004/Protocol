// Receiver.cpp
#include <windows.h>
#include <ws2bth.h>
#include <bthsdpdef.h>
#include <initguid.h>
#include <iostream>
#include "MouseInjector.h"
#include "MousePacket.h"

// link against Ws2_32.lib and Bthprops.lib
#pragma comment(lib, "Ws2_32.lib")
#pragma comment(lib, "Bthprops.lib")

bool HandlePacket(const InputEventPacket& pkt) {
    MouseEventPayload payload;
    DeserializeMousePayload(pkt.payload, payload);

    switch (pkt.eventType) {
        case EVENT_MOVE:
            // map payload.dx/payload.dy directly to absolute to simplify
            return InjectMouseMoveAbsolute(payload.dx, payload.dy);

        case EVENT_CLICK:
            if (pkt.actionFlag)
                return InjectMouseClick(static_cast<MouseButton>(payload.buttonMask));
            else
                return InjectMouseClick(static_cast<MouseButton>(payload.buttonMask));

        case EVENT_SCROLL:
            return InjectMouseScroll(payload.data);

        default:
            std::cerr << "[Receiver] Unknown eventType " << +pkt.eventType << "\n";
            return false;
    }
}

int main() {
    // 1) Setup Bluetooth server socket
    WSADATA wsa;
    if (WSAStartup(MAKEWORD(2,2), &wsa)) return 1;
    SOCKET srv = socket(AF_BTH, SOCK_STREAM, BTHPROTO_RFCOMM);
    if (srv == INVALID_SOCKET) return 1;

    SOCKADDR_BTH local = {};
    local.addressFamily = AF_BTH;
    local.btAddr        = 0;    // any adapter
    local.port          = 4;    // same port
    bind(srv, (SOCKADDR*)&local, sizeof(local));
    listen(srv, 1);

    std::cout << "[Receiver] Waiting for connection...\n";
    SOCKET cli = accept(srv, NULL, NULL);
    std::cout << "[Receiver] Client connected.\n";

    // 2) Receive loop
    uint8_t buf[sizeof(InputEventPacket)];
    int bytes;
    while ((bytes = recv(cli, (char*)buf, sizeof(buf), 0)) == sizeof(buf)) {
        InputEventPacket pkt;
        DeserializePacket(buf, pkt);
        if (!HandlePacket(pkt)) {
            std::cerr << "[Receiver] Handling failed\n";
        }
    }

    // 3) Cleanup
    closesocket(cli);
    closesocket(srv);
    WSACleanup();
    std::cout << "[Receiver] Exiting.\n";
    return 0;
}
