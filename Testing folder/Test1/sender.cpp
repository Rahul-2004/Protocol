// Sender.cpp

#include <windows.h>
#include <ws2bth.h>
#include <bthsdpdef.h>
#include <initguid.h>
#include <iostream>
#include "MouseHook.h"
#include "MousePacket.h"

#pragma comment(lib, "Ws2_32.lib")
#pragma comment(lib, "Bthprops.lib")

// Button‐mask constants (for MouseEventPayload::buttonMask)
constexpr uint8_t BTN_LEFT   = 0x01;
constexpr uint8_t BTN_RIGHT  = 0x02;
constexpr uint8_t BTN_MIDDLE = 0x04;

// Bluetooth connection globals
static SOCKET btSocket = INVALID_SOCKET;
static uint32_t seqNum = 0;

// Serialize & send one InputEventPacket
bool SendPacket(const InputEventPacket& pkt) {
    uint8_t buffer[sizeof(InputEventPacket)];
    SerializePacket(pkt, buffer);
    int sent = send(btSocket, (const char*)buffer, sizeof(buffer), 0);
    return sent == sizeof(buffer);
}

// Low‐level mouse hook callback
LRESULT CALLBACK MouseHookProc(int nCode, WPARAM wParam, LPARAM lParam) {
    if (nCode < 0 || btSocket == INVALID_SOCKET)
        return CallNextHookEx(NULL, nCode, wParam, lParam);

    MSLLHOOKSTRUCT* mi = (MSLLHOOKSTRUCT*)lParam;
    InputEventPacket pkt{};
    MouseEventPayload payload{};

    pkt.type     = TYPE_INPUT_EVENT;
    pkt.sourceID = SOURCE_PC1;

    switch (wParam) {
        case WM_MOUSEMOVE:
            pkt.eventType  = EVENT_MOVE;
            pkt.actionFlag = 0;
            payload.dx     = mi->pt.x;
            payload.dy     = mi->pt.y;
            payload.buttonMask = 0;
            payload.data   = 0;
            break;

        case WM_LBUTTONDOWN:
        case WM_RBUTTONDOWN:
        case WM_MBUTTONDOWN:
            pkt.eventType  = EVENT_CLICK;
            pkt.actionFlag = 1;
            payload.dx     = 0;
            payload.dy     = 0;
            payload.data   = 0;
            payload.buttonMask = 
                (wParam==WM_LBUTTONDOWN ? BTN_LEFT :
                 wParam==WM_RBUTTONDOWN ? BTN_RIGHT :
                                          BTN_MIDDLE);
            break;

        case WM_LBUTTONUP:
        case WM_RBUTTONUP:
        case WM_MBUTTONUP:
            pkt.eventType  = EVENT_CLICK;
            pkt.actionFlag = 0;
            payload.dx     = 0;
            payload.dy     = 0;
            payload.data   = 0;
            payload.buttonMask = 
                (wParam==WM_LBUTTONUP ? BTN_LEFT :
                 wParam==WM_RBUTTONUP ? BTN_RIGHT :
                                        BTN_MIDDLE);
            break;

        case WM_MOUSEWHEEL:
            pkt.eventType  = EVENT_SCROLL;
            pkt.actionFlag = 0;
            payload.dx     = 0;
            payload.dy     = 0;
            payload.buttonMask = 0;
            payload.data   = GET_WHEEL_DELTA_WPARAM(mi->mouseData);
            break;

        default:
            return CallNextHookEx(NULL, nCode, wParam, lParam);
    }

    // assign sequence and payload, then send
    pkt.seqNum = ++seqNum;
    SerializeMousePayload(payload, pkt.payload);
    if (!SendPacket(pkt)) {
        std::cerr << "[Sender] send failed seq=" << pkt.seqNum << "\n";
    }

    return CallNextHookEx(NULL, nCode, wParam, lParam);
}

int main() {
    // 1) Initialize Winsock + Bluetooth socket
    WSADATA wsa;
    if (WSAStartup(MAKEWORD(2,2), &wsa) != 0) {
        std::cerr << "[Sender] WSAStartup failed\n";
        return 1;
    }

    btSocket = socket(AF_BTH, SOCK_STREAM, BTHPROTO_RFCOMM);
    if (btSocket == INVALID_SOCKET) {
        std::cerr << "[Sender] socket() failed\n";
        WSACleanup();
        return 1;
    }

    SOCKADDR_BTH serverAddr = {};
    serverAddr.addressFamily = AF_BTH;
    serverAddr.btAddr        = 0x0000A0510B50BA5D; // ← replace with your receiver’s MAC
    serverAddr.port          = 4;                  // ← must match receiver’s listening port

    std::cout << "[Sender] Connecting to 0x" 
              << std::hex << serverAddr.btAddr 
              << " port " << std::dec << serverAddr.port << "...\n";

    if (connect(btSocket, (SOCKADDR*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR) {
        std::cerr << "[Sender] connect() failed: " << WSAGetLastError() << "\n";
        closesocket(btSocket);
        WSACleanup();
        return 1;
    }
    std::cout << "[Sender] Connected.\n";

    // 2) Install the mouse hook
    if (!InstallMouseHook()) {
        std::cerr << "[Sender] InstallMouseHook() failed\n";
        closesocket(btSocket);
        WSACleanup();
        return 1;
    }
    std::cout << "[Sender] Hook installed. Press ESC to exit.\n";

    // 3) Run message loop, exit on ESC
    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        if (msg.message == WM_KEYDOWN && msg.wParam == VK_ESCAPE) break;
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    // 4) Cleanup
    UninstallMouseHook();
    closesocket(btSocket);
    WSACleanup();
    std::cout << "[Sender] Exited.\n";
    return 0;
}
