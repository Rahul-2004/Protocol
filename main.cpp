#if !defined(_WIN32)
#include <iostream>

int main() {
    std::cerr << "Cross-PC Input Relay is a Windows target because it uses "
              << "Raw Input, SendInput, and Winsock Bluetooth RFCOMM.\n";
    std::cerr << "Build and run this project on Windows. The portable protocol "
              << "module can still be compiled on non-Windows hosts.\n";
    return 1;
}
#else
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <deque>
#include <iostream>
#include <limits>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "Protocol/BluetoothTransport.h"
#include "Protocol/MousePacket.h"
#include "Protocol/Observability.h"
#include "input_injection/MouseInjector.h"

namespace {
using namespace std::chrono_literals;

constexpr auto kReconnectDelay = 2s;
constexpr auto kHeartbeatInterval = 5s;
constexpr auto kHeartbeatTimeout = 15s;
constexpr ULONG kDefaultRfcommPort = 4;

int16_t ClampI16(LONG value) {
    return static_cast<int16_t>(std::clamp<LONG>(
        value,
        static_cast<LONG>(std::numeric_limits<int16_t>::min()),
        static_cast<LONG>(std::numeric_limits<int16_t>::max())));
}

std::string ToString(std::string_view value) {
    return std::string(value.data(), value.size());
}

bool IsControlState(uint8_t state) {
    return state == STATE_LOCAL_CONTROL || state == STATE_FORWARDING || state == STATE_DISCONNECTED;
}

class ToggleMacroDetector {
public:
    bool OnMiddleDown(uint32_t nowMs) {
        if (!releasedSinceLastPress_) {
            return false;
        }

        releasedSinceLastPress_ = false;
        clicks_.push_back(nowMs);
        while (!clicks_.empty() && nowMs - clicks_.front() > 1500u) {
            clicks_.pop_front();
        }

        if (clicks_.size() >= 5) {
            clicks_.clear();
            return true;
        }
        return false;
    }

    void OnMiddleUp() {
        releasedSinceLastPress_ = true;
    }

private:
    bool releasedSinceLastPress_ = true;
    std::deque<uint32_t> clicks_;
};

class SenderApp {
public:
    SenderApp(BTH_ADDR receiverAddress, ULONG port)
        : receiverAddress_(receiverAddress), port_(port) {}

    int Run() {
        if (!winsock_.Started()) {
            std::cerr << winsock_.LastError() << "\n";
            return 1;
        }

        running_ = true;
        dashboard_.Start("PC1 sender - Raw Input + Bluetooth");
        dashboard_.AddEvent("five middle-button clicks within 1.5s toggles forwarding");

        if (!CreateHiddenWindow() || !RegisterRawInput()) {
            std::cerr << "Failed to initialize Raw Input: " << GetLastError() << "\n";
            Shutdown();
            return 1;
        }

        rxThread_ = std::thread(&SenderApp::ReceiveLoop, this);
        heartbeatThread_ = std::thread(&SenderApp::HeartbeatLoop, this);

        MSG msg{};
        while (running_ && GetMessage(&msg, nullptr, 0, 0) > 0) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }

        Shutdown();
        return 0;
    }

private:
    static LRESULT CALLBACK StaticWndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
        SenderApp* app = nullptr;
        if (message == WM_NCCREATE) {
            auto* create = reinterpret_cast<CREATESTRUCTW*>(lParam);
            app = reinterpret_cast<SenderApp*>(create->lpCreateParams);
            SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(app));
        } else {
            app = reinterpret_cast<SenderApp*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
        }

        if (app != nullptr) {
            return app->WndProc(hwnd, message, wParam, lParam);
        }
        return DefWindowProcW(hwnd, message, wParam, lParam);
    }

    LRESULT WndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
        switch (message) {
        case WM_INPUT:
            HandleRawInput(lParam);
            return 0;
        case WM_DESTROY:
            running_ = false;
            PostQuitMessage(0);
            return 0;
        default:
            return DefWindowProcW(hwnd, message, wParam, lParam);
        }
    }

    bool CreateHiddenWindow() {
        WNDCLASSW wc{};
        wc.lpfnWndProc = SenderApp::StaticWndProc;
        wc.hInstance = GetModuleHandleW(nullptr);
        wc.lpszClassName = L"CrossPcInputRelaySenderWindow";
        RegisterClassW(&wc);

        hwnd_ = CreateWindowExW(0,
                                wc.lpszClassName,
                                L"Cross-PC Input Relay Sender",
                                WS_OVERLAPPED,
                                0,
                                0,
                                0,
                                0,
                                nullptr,
                                nullptr,
                                wc.hInstance,
                                this);
        return hwnd_ != nullptr;
    }

    bool RegisterRawInput() const {
        RAWINPUTDEVICE devices[2]{};
        devices[0].usUsagePage = 0x01;
        devices[0].usUsage = 0x02;
        devices[0].dwFlags = RIDEV_INPUTSINK;
        devices[0].hwndTarget = hwnd_;

        devices[1].usUsagePage = 0x01;
        devices[1].usUsage = 0x06;
        devices[1].dwFlags = RIDEV_INPUTSINK;
        devices[1].hwndTarget = hwnd_;

        // RIDEV_NOLEGACY is intentionally not used: the local OS cursor and
        // keyboard continue to receive the original input events.
        return RegisterRawInputDevices(devices, 2, sizeof(RAWINPUTDEVICE)) == TRUE;
    }

    void HandleRawInput(LPARAM lParam) {
        UINT size = 0;
        if (GetRawInputData(reinterpret_cast<HRAWINPUT>(lParam), RID_INPUT, nullptr,
                            &size, sizeof(RAWINPUTHEADER)) != 0 || size == 0) {
            return;
        }

        std::vector<BYTE> buffer(size);
        if (GetRawInputData(reinterpret_cast<HRAWINPUT>(lParam), RID_INPUT, buffer.data(),
                            &size, sizeof(RAWINPUTHEADER)) != size) {
            return;
        }

        const auto* raw = reinterpret_cast<const RAWINPUT*>(buffer.data());
        if (raw->header.dwType == RIM_TYPEMOUSE) {
            HandleRawMouse(raw->data.mouse);
        } else if (raw->header.dwType == RIM_TYPEKEYBOARD) {
            HandleRawKeyboard(raw->data.keyboard);
        }
    }

    void HandleRawMouse(const RAWMOUSE& mouse) {
        const USHORT flags = mouse.usButtonFlags;
        bool toggled = false;

        if (flags & RI_MOUSE_MIDDLE_BUTTON_DOWN) {
            toggled = macro_.OnMiddleDown(NowMilliseconds32());
            if (toggled) {
                ToggleForwarding();
            }
        }
        if (flags & RI_MOUSE_MIDDLE_BUTTON_UP) {
            macro_.OnMiddleUp();
        }

        if (toggled || !forwarding_.load() || !connected_.load()) {
            return;
        }

        if (mouse.lLastX != 0 || mouse.lLastY != 0) {
            SendInputEvent(CreateMouseMovePacket(SOURCE_PC1,
                                                 NextSeq(),
                                                 ClampI16(mouse.lLastX),
                                                 ClampI16(mouse.lLastY)));
        }

        SendButtonIfPresent(flags, RI_MOUSE_LEFT_BUTTON_DOWN, RI_MOUSE_LEFT_BUTTON_UP, BTN_LEFT);
        SendButtonIfPresent(flags, RI_MOUSE_RIGHT_BUTTON_DOWN, RI_MOUSE_RIGHT_BUTTON_UP, BTN_RIGHT);
        SendButtonIfPresent(flags, RI_MOUSE_MIDDLE_BUTTON_DOWN, RI_MOUSE_MIDDLE_BUTTON_UP, BTN_MIDDLE);
        SendButtonIfPresent(flags, RI_MOUSE_BUTTON_4_DOWN, RI_MOUSE_BUTTON_4_UP, BTN_X1);
        SendButtonIfPresent(flags, RI_MOUSE_BUTTON_5_DOWN, RI_MOUSE_BUTTON_5_UP, BTN_X2);

        if (flags & RI_MOUSE_WHEEL) {
            SendInputEvent(CreateScrollPacket(SOURCE_PC1,
                                              NextSeq(),
                                              static_cast<int16_t>(static_cast<SHORT>(mouse.usButtonData))));
        }
    }

    void SendButtonIfPresent(USHORT flags, USHORT downFlag, USHORT upFlag, uint8_t buttonMask) {
        if (flags & downFlag) {
            SendInputEvent(CreateClickPacket(SOURCE_PC1, NextSeq(), buttonMask, true));
        }
        if (flags & upFlag) {
            SendInputEvent(CreateClickPacket(SOURCE_PC1, NextSeq(), buttonMask, false));
        }
    }

    void HandleRawKeyboard(const RAWKEYBOARD& keyboard) {
        if (!forwarding_.load() || !connected_.load() || keyboard.VKey == 255) {
            return;
        }

        const bool down = (keyboard.Flags & RI_KEY_BREAK) == 0;
        const bool extended = (keyboard.Flags & RI_KEY_E0) != 0;
        SendInputEvent(CreateKeyboardPacket(SOURCE_PC1,
                                            NextSeq(),
                                            static_cast<uint16_t>(keyboard.VKey),
                                            down,
                                            extended));
    }

    uint32_t NextSeq() {
        return ++seq_;
    }

    bool SendPacket(const InputEventPacket& packet) {
        if (!connected_.load()) {
            dashboard_.RecordDrop("send while disconnected");
            return false;
        }

        if (!transport_.SendPacket(packet)) {
            dashboard_.AddEvent("send failed: " + transport_.LastError());
            connected_ = false;
            dashboard_.SetConnection(false);
            transport_.Close();
            return false;
        }

        dashboard_.RecordSent(packet);
        return true;
    }

    void SendInputEvent(const InputEventPacket& packet) {
        if (!ValidateChecksum(packet)) {
            dashboard_.RecordDrop("local packet checksum build failed");
            return;
        }
        SendPacket(packet);
    }

    void ToggleForwarding() {
        const bool target = !forwarding_.load();
        forwarding_ = target;
        dashboard_.SetForwarding(target);

        const uint32_t currentSeq = seq_.load();
        if (!target) {
            SendPacket(CreateFlushSeqPacket(SOURCE_PC1, currentSeq));
            SendPacket(CreateTogglePacket(false, SOURCE_PC1, currentSeq));
            return;
        }

        bool acknowledged = false;
        for (int attempt = 1; attempt <= 3 && running_; ++attempt) {
            dashboard_.AddEvent("sending TOGGLE_ON attempt " + std::to_string(attempt));
            SendPacket(CreateTogglePacket(true, SOURCE_PC1, currentSeq));
            if (WaitForAck(currentSeq, STATE_FORWARDING, 200ms)) {
                acknowledged = true;
                break;
            }
        }

        if (!acknowledged) {
            forwarding_ = false;
            dashboard_.SetForwarding(false);
            dashboard_.AddEvent("remote did not ACK TOGGLE_ON; reverted to local");
        }
    }

    bool WaitForAck(uint32_t minimumSeq, uint8_t expectedState, std::chrono::milliseconds timeout) {
        const auto deadline = std::chrono::steady_clock::now() + timeout;
        while (std::chrono::steady_clock::now() < deadline) {
            if (ackSeq_.load() >= minimumSeq && remoteState_.load() == expectedState) {
                return true;
            }
            std::this_thread::sleep_for(20ms);
        }
        return false;
    }

    void ReceiveLoop() {
        while (running_) {
            if (!connected_.load()) {
                Reconnect();
                continue;
            }

            InputEventPacket packet{};
            const ReceiveResult result = transport_.ReceivePacket(packet);
            if (result == ReceiveResult::Timeout) {
                continue;
            }
            if (result != ReceiveResult::Packet) {
                dashboard_.AddEvent("receive failed: " + transport_.LastError());
                connected_ = false;
                dashboard_.SetConnection(false);
                forwarding_ = false;
                dashboard_.SetForwarding(false);
                transport_.Close();
                continue;
            }

            if (!ValidateChecksum(packet)) {
                dashboard_.RecordDrop("checksum mismatch from peer");
                continue;
            }

            dashboard_.RecordReceived(packet);
            HandleControlPacket(packet);
        }
    }

    void HandleControlPacket(const InputEventPacket& packet) {
        switch (packet.type) {
        case TYPE_HELLO: {
            const uint8_t peerState = packet.actionFlag;
            dashboard_.SetPeerState(ToString(RelayStateName(peerState)));
            SendPacket(CreateAckPacket(SOURCE_PC1,
                                       forwarding_.load() ? STATE_FORWARDING : STATE_LOCAL_CONTROL,
                                       seq_.load()));
            if (IsControlState(peerState) && peerState != remoteState_.load()) {
                remoteState_ = peerState;
            }
            if (peerState != (forwarding_.load() ? STATE_FORWARDING : STATE_LOCAL_CONTROL)) {
                SendPacket(CreateTogglePacket(forwarding_.load(), SOURCE_PC1, seq_.load()));
            }
            break;
        }
        case TYPE_ACK:
            ackSeq_ = packet.seqNum;
            if (IsControlState(packet.actionFlag)) {
                remoteState_ = packet.actionFlag;
                dashboard_.SetPeerState(ToString(RelayStateName(packet.actionFlag)));
            }
            break;
        case TYPE_HEARTBEAT: {
            dashboard_.RecordHeartbeat();
            const uint32_t now = NowMilliseconds32();
            dashboard_.RecordLatency(now - GetPacketTimestampMs(packet));
            break;
        }
        default:
            break;
        }
    }

    void HeartbeatLoop() {
        while (running_) {
            std::this_thread::sleep_for(kHeartbeatInterval);
            if (!running_) {
                break;
            }
            if (connected_.load() && forwarding_.load()) {
                SendPacket(CreateHeartbeatPacket(SOURCE_PC1, seq_.load()));
            }
        }
    }

    void Reconnect() {
        std::lock_guard<std::mutex> lock(reconnectMutex_);
        if (connected_.load() || !running_) {
            return;
        }

        while (running_ && !connected_.load()) {
            dashboard_.RecordReconnect();
            transport_.Close();
            if (transport_.Connect(receiverAddress_, port_)) {
                transport_.SetReceiveTimeoutMs(1000);
                connected_ = true;
                dashboard_.SetConnection(true);
                dashboard_.AddEvent("connected to " + FormatBluetoothAddress(receiverAddress_) +
                                    " on RFCOMM port " + std::to_string(port_));
                return;
            }

            dashboard_.AddEvent("connect failed: " + transport_.LastError());
            std::this_thread::sleep_for(kReconnectDelay);
        }
    }

    void Shutdown() {
        running_ = false;
        transport_.Close();
        if (hwnd_ != nullptr) {
            DestroyWindow(hwnd_);
            hwnd_ = nullptr;
        }
        if (rxThread_.joinable()) {
            rxThread_.join();
        }
        if (heartbeatThread_.joinable()) {
            heartbeatThread_.join();
        }
        dashboard_.Stop();
    }

    BTH_ADDR receiverAddress_{};
    ULONG port_{};
    WinsockSession winsock_;
    BluetoothTransport transport_;
    ObservabilityDashboard dashboard_;
    std::atomic<bool> running_{false};
    std::atomic<bool> connected_{false};
    std::atomic<bool> forwarding_{false};
    std::atomic<uint32_t> seq_{0};
    std::atomic<uint32_t> ackSeq_{0};
    std::atomic<uint8_t> remoteState_{STATE_LOCAL_CONTROL};
    std::mutex reconnectMutex_;
    std::thread rxThread_;
    std::thread heartbeatThread_;
    HWND hwnd_ = nullptr;
    ToggleMacroDetector macro_;
};

class ReceiverApp {
public:
    explicit ReceiverApp(ULONG port) : port_(port) {}

    int Run() {
        if (!winsock_.Started()) {
            std::cerr << winsock_.LastError() << "\n";
            return 1;
        }

        dashboard_.Start("PC2 receiver - injection + dashboard");
        if (!listener_.Listen(port_)) {
            std::cerr << "Bluetooth listen failed: " << listener_.LastError() << "\n";
            dashboard_.Stop();
            return 1;
        }

        running_ = true;
        dashboard_.AddEvent("listening on RFCOMM port " + std::to_string(listener_.ListenPort()));
        std::cout << "Receiver listening on RFCOMM port " << listener_.ListenPort() << "\n";

        while (running_) {
            BluetoothTransport client;
            dashboard_.AddEvent("waiting for PC1 connection");
            if (!listener_.Accept(client)) {
                dashboard_.AddEvent("accept failed: " + listener_.LastError());
                std::this_thread::sleep_for(1s);
                continue;
            }
            ProcessClient(client);
        }

        dashboard_.Stop();
        return 0;
    }

private:
    void ProcessClient(BluetoothTransport& client) {
        client.SetReceiveTimeoutMs(1000);
        connected_ = true;
        dashboard_.SetConnection(true);
        lastHeartbeat_ = std::chrono::steady_clock::now();
        SendPacket(client, CreateHelloPacket(SOURCE_PC2,
                                             forwarding_.load() ? STATE_FORWARDING : STATE_LOCAL_CONTROL,
                                             lastSeqProcessed_.load()));

        while (running_ && client.IsOpen()) {
            InputEventPacket packet{};
            const ReceiveResult result = client.ReceivePacket(packet);
            if (result == ReceiveResult::Timeout) {
                CheckHeartbeatTimeout(client);
                continue;
            }
            if (result != ReceiveResult::Packet) {
                dashboard_.AddEvent("connection ended: " + client.LastError());
                break;
            }

            if (!ValidateChecksum(packet)) {
                dashboard_.RecordDrop("checksum mismatch");
                continue;
            }

            dashboard_.RecordReceived(packet);
            ProcessPacket(client, packet);
        }

        connected_ = false;
        forwarding_ = false;
        dashboard_.SetConnection(false);
        dashboard_.SetForwarding(false);
        client.Close();
    }

    bool SendPacket(BluetoothTransport& client, const InputEventPacket& packet) {
        if (!client.SendPacket(packet)) {
            dashboard_.AddEvent("send failed: " + client.LastError());
            return false;
        }
        dashboard_.RecordSent(packet);
        return true;
    }

    void ProcessPacket(BluetoothTransport& client, const InputEventPacket& packet) {
        switch (packet.type) {
        case TYPE_ACK:
            ApplyPeerState(packet.actionFlag, packet.seqNum);
            break;
        case TYPE_TOGGLE_ON:
            forwarding_ = true;
            dashboard_.SetForwarding(true);
            dashboard_.SetPeerState("PC1 FORWARDING");
            SendPacket(client, CreateAckPacket(SOURCE_PC2, STATE_FORWARDING, packet.seqNum));
            break;
        case TYPE_TOGGLE_OFF:
            FlushThrough(packet.seqNum);
            forwarding_ = false;
            dashboard_.SetForwarding(false);
            dashboard_.SetPeerState("PC1 LOCAL_CONTROL");
            SendPacket(client, CreateAckPacket(SOURCE_PC2, STATE_LOCAL_CONTROL, packet.seqNum));
            break;
        case TYPE_FLUSH_SEQ:
            FlushThrough(packet.seqNum);
            break;
        case TYPE_HEARTBEAT:
            lastHeartbeat_ = std::chrono::steady_clock::now();
            dashboard_.RecordHeartbeat();
            SendPacket(client, CreateHeartbeatPacket(SOURCE_PC2,
                                                     lastSeqProcessed_.load(),
                                                     GetPacketTimestampMs(packet)));
            break;
        case TYPE_INPUT_EVENT:
            ProcessInputPacket(packet);
            break;
        case TYPE_HELLO:
            SendPacket(client, CreateAckPacket(SOURCE_PC2,
                                               forwarding_.load() ? STATE_FORWARDING : STATE_LOCAL_CONTROL,
                                               lastSeqProcessed_.load()));
            break;
        default:
            dashboard_.RecordDrop("unknown packet type");
            break;
        }
    }

    void ProcessInputPacket(const InputEventPacket& packet) {
        if (!forwarding_.load()) {
            dashboard_.RecordDrop("input received while local");
            return;
        }

        const PacketDecision decision = ShouldAcceptInputPacket(packet,
                                                                SOURCE_PC2,
                                                                SOURCE_PC1,
                                                                lastSeqProcessed_.load());
        if (!decision.accepted) {
            dashboard_.RecordDrop(decision.reason);
            return;
        }
        dashboard_.RecordPacketLoss(decision.missingPackets);

        if (!InjectInputPacket(packet)) {
            dashboard_.RecordDrop("SendInput injection failed");
            return;
        }

        lastSeqProcessed_ = packet.seqNum;
    }

    void ApplyPeerState(uint8_t state, uint32_t currentSeq) {
        if (!IsControlState(state)) {
            return;
        }
        forwarding_ = state == STATE_FORWARDING;
        dashboard_.SetForwarding(forwarding_.load());
        dashboard_.SetPeerState("PC1 " + ToString(RelayStateName(state)));
        FlushThrough(currentSeq);
    }

    void FlushThrough(uint32_t seq) {
        uint32_t current = lastSeqProcessed_.load();
        while (seq > current && !lastSeqProcessed_.compare_exchange_weak(current, seq)) {
        }
        dashboard_.AddEvent("flushed through seq " + std::to_string(seq));
    }

    void CheckHeartbeatTimeout(BluetoothTransport& client) {
        if (!forwarding_.load()) {
            return;
        }

        const auto elapsed = std::chrono::steady_clock::now() - lastHeartbeat_;
        if (elapsed >= kHeartbeatTimeout) {
            forwarding_ = false;
            dashboard_.SetForwarding(false);
            dashboard_.AddEvent("heartbeat timeout; reverting to local and forcing reconnect");
            client.Close();
        }
    }

    ULONG port_{};
    WinsockSession winsock_;
    BluetoothTransport listener_;
    ObservabilityDashboard dashboard_;
    std::atomic<bool> running_{false};
    std::atomic<bool> connected_{false};
    std::atomic<bool> forwarding_{false};
    std::atomic<uint32_t> lastSeqProcessed_{0};
    std::chrono::steady_clock::time_point lastHeartbeat_ = std::chrono::steady_clock::now();
};

ULONG ParsePort(const char* text) {
    const long value = std::strtol(text, nullptr, 10);
    if (value <= 0 || value > 30) {
        return kDefaultRfcommPort;
    }
    return static_cast<ULONG>(value);
}

void PrintUsage(const char* exe) {
    std::cerr << "Usage:\n";
    std::cerr << "  " << exe << " receiver [rfcomm-port]\n";
    std::cerr << "  " << exe << " sender <receiver-bt-address> [rfcomm-port]\n";
    std::cerr << "\nExamples:\n";
    std::cerr << "  " << exe << " receiver 4\n";
    std::cerr << "  " << exe << " sender 00:11:22:33:44:55 4\n";
}
} // namespace

int main(int argc, char** argv) {
    if (argc < 2) {
        PrintUsage(argv[0]);
        return 1;
    }

    const std::string mode = argv[1];
    if (mode == "receiver") {
        const ULONG port = argc >= 3 ? ParsePort(argv[2]) : kDefaultRfcommPort;
        ReceiverApp app(port);
        return app.Run();
    }

    if (mode == "sender") {
        if (argc < 3) {
            PrintUsage(argv[0]);
            return 1;
        }

        BTH_ADDR address = 0;
        if (!ParseBluetoothAddress(argv[2], address)) {
            std::cerr << "Invalid Bluetooth address: " << argv[2] << "\n";
            return 1;
        }

        const ULONG port = argc >= 4 ? ParsePort(argv[3]) : kDefaultRfcommPort;
        SenderApp app(address, port);
        return app.Run();
    }

    PrintUsage(argv[0]);
    return 1;
}
#endif
