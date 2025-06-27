// sender.cpp
#include <windows.h>
#include <ws2bth.h>
#include <bthsdpdef.h>
#include <initguid.h>
#include <iostream>
#include <deque>
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
    uint8_t  payload[12];
};
#pragma pack(pop)

struct MouseEventPayload {
    float    dx;
    float    dy;
    uint8_t  buttonMask;
    int16_t  data;
    uint8_t  padding;
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

// Global Bluetooth socket and sequence number
SOCKET btSocket = INVALID_SOCKET;
uint32_t seqNum = 0;

// Screen size (for normalizing)
static int senderW = GetSystemMetrics(SM_CXSCREEN);
static int senderH = GetSystemMetrics(SM_CYSCREEN);

// For toggle logic
static bool remoteMode = false;
static std::deque<DWORD64> middleClicks;  // timestamps of recent middle clicks

void SerializeMousePayload(const MouseEventPayload& m, uint8_t* out) {
    std::memcpy(out,      &m.dx,         sizeof(float));
    std::memcpy(out + 4,  &m.dy,         sizeof(float));
    std::memcpy(out + 8,  &m.buttonMask, sizeof(uint8_t));
    std::memcpy(out + 9,  &m.data,       sizeof(int16_t));
}

void SerializePacket(const InputEventPacket& p, uint8_t* out) {
    std::memcpy(out, &p, sizeof(p));
}

bool SendPacket(const InputEventPacket& pkt) {
    uint8_t buffer[sizeof(pkt)];
    SerializePacket(pkt, buffer);
    int sent = send(btSocket,
                    reinterpret_cast<const char*>(buffer),
                    sizeof(buffer),
                    0);
    return sent == sizeof(buffer);
}

LRESULT CALLBACK MouseHookProc(int nCode, WPARAM wParam, LPARAM lParam) {
    if (nCode < 0) 
        return CallNextHookEx(NULL, nCode, wParam, lParam);

    // Check for the toggle pattern on middle‐button down
    if (wParam == WM_MBUTTONDOWN) {
        DWORD64 now = GetTickCount64();
        middleClicks.push_back(now);

        // Remove any older than 1.5 seconds
        while (!middleClicks.empty() && now - middleClicks.front() > 1500) {
            middleClicks.pop_front();
        }

        // If we’ve clicked 5 times in 1.5s, flip modes
        if (middleClicks.size() >= 5) {
            remoteMode = !remoteMode;
            middleClicks.clear(); 
            std::cout << "[Sender] remoteMode = "
                      << (remoteMode ? "ON" : "OFF") << "\n";
            // Let that multi‐click go through locally
            return CallNextHookEx(NULL, nCode, wParam, lParam);
        }
    }

    // If remoteMode is off, don't intercept anything
    if (!remoteMode) {
        return CallNextHookEx(NULL, nCode, wParam, lParam);
    }

    // At this point we're in remoteMode: intercept & forward
    auto mi = reinterpret_cast<MSLLHOOKSTRUCT*>(lParam);
    InputEventPacket pkt{};
    MouseEventPayload m{};
    pkt.type     = TYPE_INPUT_EVENT;
    pkt.sourceID = SOURCE_PC1;

    bool handled = true;

    switch (wParam) {
        case WM_MOUSEMOVE:
            pkt.eventType  = EVENT_MOVE;
            pkt.actionFlag = 0;
            m.dx = mi->pt.x / static_cast<float>(senderW);
            m.dy = mi->pt.y / static_cast<float>(senderH);
            m.buttonMask = 0;
            m.data = 0;
            break;

        case WM_LBUTTONDOWN: case WM_RBUTTONDOWN: case WM_MBUTTONDOWN:
            pkt.eventType  = EVENT_CLICK;
            pkt.actionFlag = 1;
            m.dx = m.dy = 0;
            m.data = 0;
            m.buttonMask = (wParam == WM_LBUTTONDOWN ? BTN_LEFT :
                            wParam == WM_RBUTTONDOWN ? BTN_RIGHT :
                                                       BTN_MIDDLE);
            break;

        case WM_LBUTTONUP: case WM_RBUTTONUP: case WM_MBUTTONUP:
            pkt.eventType  = EVENT_CLICK;
            pkt.actionFlag = 0;
            m.dx = m.dy = 0;
            m.data = 0;
            m.buttonMask = (wParam == WM_LBUTTONUP ? BTN_LEFT :
                            wParam == WM_RBUTTONUP ? BTN_RIGHT :
                                                     BTN_MIDDLE);
            break;

        case WM_MOUSEWHEEL:
            pkt.eventType  = EVENT_SCROLL;
            pkt.actionFlag = 0;
            m.dx = m.dy = 0;
            m.buttonMask = 0;
            // note: for low-level hook, wheel delta is in mi->mouseData
            m.data = static_cast<int16_t>(GET_WHEEL_DELTA_WPARAM(mi->mouseData));
            break;

        default:
            handled = false;
    }

    if (handled) {
        pkt.seqNum = ++seqNum;
        SerializeMousePayload(m, pkt.payload);
        SendPacket(pkt);
        // swallow locally
        return 1;
    } else {
        return CallNextHookEx(NULL, nCode, wParam, lParam);
    }
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
    addr.btAddr = 0x0000A0510B50BA5D; // your receiver’s MAC
    addr.port = 4;                    // your RFCOMM port

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

    std::cout << "[Sender] Mouse hook active. Middle‑click 5× fast to toggle remote.\n";
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
