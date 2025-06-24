// sender.cpp

#include <windows.h>
#include <ws2bth.h>
#include <bthsdpdef.h>
#include <initguid.h>
#include <iostream>
#include <cstdint>
#include <cstring>

// link-with
#pragma comment(lib, "Ws2_32.lib")
#pragma comment(lib, "Bthprops.lib")

// —— Packet Definitions ——

// Core packet header
#pragma pack(push, 1)
struct InputEventPacket {
    uint8_t  type;
    uint8_t  sourceID;
    uint8_t  eventType;
    uint8_t  actionFlag;
    uint32_t seqNum;
    uint8_t  payload[8];
};
#pragma pack(pop)

// Mouse payload
struct MouseEventPayload {
    uint8_t  buttonMask;
    int16_t  dx, dy;
    int16_t  data;
};

// PacketType/EventType constants
enum : uint8_t {
    TYPE_INPUT_EVENT = 0x01,
    EVENT_MOVE       = 0x01,
    EVENT_CLICK      = 0x02,
    EVENT_SCROLL     = 0x03,
    SOURCE_PC1       = 0x01
};

// Button masks
constexpr uint8_t BTN_LEFT   = 0x01;
constexpr uint8_t BTN_RIGHT  = 0x02;
constexpr uint8_t BTN_MIDDLE = 0x04;

// Serialization helpers
inline void SerializeMousePayload(const MouseEventPayload& m, uint8_t* out) {
    std::memcpy(out, &m, sizeof(m));
}
inline void SerializePacket(const InputEventPacket& p, uint8_t* out) {
    std::memcpy(out, &p, sizeof(p));
}

// —— Networking & Hook globals ——
static SOCKET btSocket = INVALID_SOCKET;
static uint32_t seqNum  = 0;

// send one packet
bool SendPacket(const InputEventPacket& pkt) {
    uint8_t buffer[sizeof(pkt)];
    SerializePacket(pkt, buffer);
    return send(btSocket, reinterpret_cast<const char*>(buffer), sizeof(buffer), 0)
         == sizeof(buffer);
}

// —— Mouse hook callback ——
LRESULT CALLBACK MouseHookProc(int nCode, WPARAM wParam, LPARAM lParam) {
    if (nCode < 0 || btSocket == INVALID_SOCKET)
        return CallNextHookEx(NULL, nCode, wParam, lParam);

    auto mi = reinterpret_cast<MSLLHOOKSTRUCT*>(lParam);
    InputEventPacket pkt{};
    MouseEventPayload  m{};

    pkt.type     = TYPE_INPUT_EVENT;
    pkt.sourceID = SOURCE_PC1;

    switch (wParam) {
        case WM_MOUSEMOVE:
            pkt.eventType  = EVENT_MOVE;
            m.dx           = mi->pt.x;
            m.dy           = mi->pt.y;
            m.buttonMask   = 0;
            m.data         = 0;
            pkt.actionFlag = 0;
            break;

        case WM_LBUTTONDOWN: case WM_RBUTTONDOWN: case WM_MBUTTONDOWN:
            pkt.eventType  = EVENT_CLICK;
            pkt.actionFlag = 1;
            m.dx = m.dy = m.data = 0;
            m.buttonMask = (wParam==WM_LBUTTONDOWN?BTN_LEFT:
                           wParam==WM_RBUTTONDOWN?BTN_RIGHT:BTN_MIDDLE);
            break;

        case WM_LBUTTONUP: case WM_RBUTTONUP: case WM_MBUTTONUP:
            pkt.eventType  = EVENT_CLICK;
            pkt.actionFlag = 0;
            m.dx = m.dy = m.data = 0;
            m.buttonMask = (wParam==WM_LBUTTONUP?BTN_LEFT:
                           wParam==WM_RBUTTONUP?BTN_RIGHT:BTN_MIDDLE);
            break;

        case WM_MOUSEWHEEL:
            pkt.eventType  = EVENT_SCROLL;
            pkt.actionFlag = 0;
            m.dx = m.dy = 0;
            m.buttonMask   = 0;
            m.data         = GET_WHEEL_DELTA_WPARAM(mi->mouseData);
            break;

        default:
            return CallNextHookEx(NULL, nCode, wParam, lParam);
    }

    pkt.seqNum = ++seqNum;
    SerializeMousePayload(m, pkt.payload);
    if (!SendPacket(pkt))
        std::cerr << "[Sender] failed to send seq=" << pkt.seqNum << "\n";

    return CallNextHookEx(NULL, nCode, wParam, lParam);
}

// —— Main ——
int main() {
    // 1) Winsock + Bluetooth client
    WSADATA wsa;
    if (WSAStartup(MAKEWORD(2,2), &wsa) != 0) {
        std::cerr << "WSAStartup failed\n"; return 1;
    }
    btSocket = socket(AF_BTH, SOCK_STREAM, BTHPROTO_RFCOMM);
    if (btSocket == INVALID_SOCKET) {
        std::cerr << "socket() failed\n"; WSACleanup(); return 1;
    }

    SOCKADDR_BTH addr = {};
    addr.addressFamily = AF_BTH;
    addr.btAddr        = 0x0000A0510B50BA5D; // ← your receiver's MAC
    addr.port          = 4;                  // ← receiver's port

    std::cout << "[Sender] Connecting...\n";
    if (connect(btSocket, (SOCKADDR*)&addr, sizeof(addr)) == SOCKET_ERROR) {
        std::cerr << "connect() failed: " << WSAGetLastError() << "\n";
        closesocket(btSocket); WSACleanup(); return 1;
    }
    std::cout << "[Sender] Connected over Bluetooth.\n";

    // 2) Install hook
    HHOOK hhk = SetWindowsHookEx(WH_MOUSE_LL, MouseHookProc, NULL, 0);
    if (!hhk) {
        std::cerr << "SetWindowsHookEx failed\n";
        closesocket(btSocket); WSACleanup(); return 1;
    }
    std::cout << "[Sender] Mouse hook installed. Press ESC to quit.\n";

    // 3) Message loop (exit on ESC)
    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        if (msg.message==WM_KEYDOWN && msg.wParam==VK_ESCAPE) break;
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    // 4) Cleanup
    UnhookWindowsHookEx(hhk);
    closesocket(btSocket);
    WSACleanup();
    return 0;
}
