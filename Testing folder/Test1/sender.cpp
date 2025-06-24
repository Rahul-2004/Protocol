#include <windows.h>
#include <ws2bth.h>
#include <bthsdpdef.h>
#include <initguid.h>
#include <iostream>
#include <cstdint>
#include <cstring>

#pragma comment(lib, "Ws2_32.lib")
#pragma comment(lib, "Bthprops.lib")

#pragma pack(push, 1)
struct InputEventPacket {
    uint8_t  type;
    uint8_t  sourceID;
    uint8_t  eventType;
    uint8_t  actionFlag;
    uint32_t seqNum;
    uint8_t  payload[12];  // Enough space for floats + extra
};
#pragma pack(pop)

struct MouseEventPayload {
    float    dx;         // bytes 0–3
    float    dy;         // bytes 4–7
    uint8_t  buttonMask; // byte  8
    int16_t  data;       // bytes 9–10
    uint8_t  padding;    // byte 11 (to fill out your 12-byte array)
};

enum : uint8_t {
    TYPE_INPUT_EVENT = 0x01,
    EVENT_MOVE       = 0x01,
    EVENT_CLICK      = 0x02,
    EVENT_SCROLL     = 0x03,
    SOURCE_PC1       = 0x01
};

constexpr uint8_t BTN_LEFT   = 0x01;
constexpr uint8_t BTN_RIGHT  = 0x02;
constexpr uint8_t BTN_MIDDLE = 0x04;

SOCKET btSocket = INVALID_SOCKET;
uint32_t seqNum = 0;

void SerializeMousePayload(const MouseEventPayload& m, uint8_t* out) {
    std::memcpy(out,      &m.dx,         sizeof(float));
    std::memcpy(out + 4,  &m.dy,         sizeof(float));
    std::memcpy(out + 8,  &m.buttonMask, sizeof(uint8_t));
    std::memcpy(out + 9,  &m.data,       sizeof(int16_t));
    // remaining bytes (payload[11]) unused
}

void SerializePacket(const InputEventPacket& p, uint8_t* out) {
    std::memcpy(out, &p, sizeof(p));
}

bool SendPacket(const InputEventPacket& pkt) {
    uint8_t buffer[sizeof(pkt)];
    SerializePacket(pkt, buffer);
    int sent = send(btSocket, reinterpret_cast<const char*>(buffer), sizeof(buffer), 0);
    return sent == sizeof(buffer);
}

LRESULT CALLBACK MouseHookProc(int nCode, WPARAM wParam, LPARAM lParam) {
    if (nCode < 0 || btSocket == INVALID_SOCKET)
        return CallNextHookEx(NULL, nCode, wParam, lParam);

    auto mi = reinterpret_cast<MSLLHOOKSTRUCT*>(lParam);
    InputEventPacket pkt{};
    MouseEventPayload m{};

    pkt.type     = TYPE_INPUT_EVENT;
    pkt.sourceID = SOURCE_PC1;

    switch (wParam) {
        case WM_MOUSEMOVE:
            pkt.eventType  = EVENT_MOVE;
            m.dx = static_cast<float>(mi->pt.x);
            m.dy = static_cast<float>(mi->pt.y);
            m.buttonMask = 0;
            m.data = 0;
            pkt.actionFlag = 0;
            break;

        case WM_LBUTTONDOWN: case WM_RBUTTONDOWN: case WM_MBUTTONDOWN:
            pkt.eventType  = EVENT_CLICK;
            pkt.actionFlag = 1;
            m.dx = m.dy = 0;
            m.data = 0;
            m.buttonMask = (wParam == WM_LBUTTONDOWN ? BTN_LEFT :
                            wParam == WM_RBUTTONDOWN ? BTN_RIGHT : BTN_MIDDLE);
            break;

        case WM_LBUTTONUP: case WM_RBUTTONUP: case WM_MBUTTONUP:
            pkt.eventType  = EVENT_CLICK;
            pkt.actionFlag = 0;
            m.dx = m.dy = 0;
            m.data = 0;
            m.buttonMask = (wParam == WM_LBUTTONUP ? BTN_LEFT :
                            wParam == WM_RBUTTONUP ? BTN_RIGHT : BTN_MIDDLE);
            break;

        case WM_MOUSEWHEEL:
            pkt.eventType  = EVENT_SCROLL;
            pkt.actionFlag = 0;
            m.dx = m.dy = 0;
            m.buttonMask = 0;
            m.data = GET_WHEEL_DELTA_WPARAM(mi->mouseData);
            break;

        default:
            return CallNextHookEx(NULL, nCode, wParam, lParam);
    }

    pkt.seqNum = ++seqNum;
    SerializeMousePayload(m, pkt.payload);

    if (!SendPacket(pkt)) {
        std::cerr << "[Sender] Failed to send packet, seq=" << pkt.seqNum << "\n";
    }

    return CallNextHookEx(NULL, nCode, wParam, lParam);
}

int main() {
    WSADATA wsa;
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
        std::cerr << "[Sender] WSAStartup failed\n";
        return 1;
    }

    btSocket = socket(AF_BTH, SOCK_STREAM, BTHPROTO_RFCOMM);
    if (btSocket == INVALID_SOCKET) {
        std::cerr << "[Sender] socket() failed\n";
        WSACleanup();
        return 1;
    }

    SOCKADDR_BTH addr = {};
    addr.addressFamily = AF_BTH;
    addr.btAddr = 0x0000A0510B50BA5D; // Replace with receiver MAC
    addr.port = 4; // Replace with correct RFCOMM port

    std::cout << "[Sender] Connecting...\n";
    if (connect(btSocket, (SOCKADDR*)&addr, sizeof(addr)) == SOCKET_ERROR) {
        std::cerr << "[Sender] connect() failed: " << WSAGetLastError() << "\n";
        closesocket(btSocket);
        WSACleanup();
        return 1;
    }

    std::cout << "[Sender] Connected.\n";

    HHOOK hhk = SetWindowsHookEx(WH_MOUSE_LL, MouseHookProc, NULL, 0);
    if (!hhk) {
        std::cerr << "[Sender] SetWindowsHookEx failed\n";
        closesocket(btSocket);
        WSACleanup();
        return 1;
    }

    std::cout << "[Sender] Mouse hook active. Press ESC to quit.\n";

    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        if (msg.message == WM_KEYDOWN && msg.wParam == VK_ESCAPE)
            break;
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    UnhookWindowsHookEx(hhk);
    closesocket(btSocket);
    WSACleanup();
    std::cout << "[Sender] Exiting.\n";
    return 0;
}
