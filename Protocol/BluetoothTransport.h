#pragma once

#include "MousePacket.h"

#include <cstdint>
#include <mutex>
#include <string>

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <winsock2.h>
#include <ws2bth.h>
#else
using BTH_ADDR = uint64_t;
using ULONG = unsigned long;
using SOCKET = intptr_t;
constexpr SOCKET INVALID_SOCKET = static_cast<SOCKET>(-1);
#endif

enum class ReceiveResult {
    Packet,
    Timeout,
    Closed,
    Error,
};

class WinsockSession {
public:
    WinsockSession();
    ~WinsockSession();

    WinsockSession(const WinsockSession&) = delete;
    WinsockSession& operator=(const WinsockSession&) = delete;

    bool Started() const;
    const std::string& LastError() const;

private:
    bool started_;
    std::string lastError_;
};

class BluetoothTransport {
public:
    BluetoothTransport();
    ~BluetoothTransport();

    BluetoothTransport(const BluetoothTransport&) = delete;
    BluetoothTransport& operator=(const BluetoothTransport&) = delete;

    BluetoothTransport(BluetoothTransport&& other) noexcept;
    BluetoothTransport& operator=(BluetoothTransport&& other) noexcept;

    bool Connect(BTH_ADDR address, ULONG port);
    bool Listen(ULONG port);
    bool Accept(BluetoothTransport& client);
    bool Attach(SOCKET socket);

    bool SendPacket(const InputEventPacket& packet);
    ReceiveResult ReceivePacket(InputEventPacket& packet);
    bool SetReceiveTimeoutMs(int timeoutMs);

    void Close();
    bool IsOpen() const;
    ULONG ListenPort() const;
    const std::string& LastError() const;

private:
    bool SendAll(const uint8_t* data, std::size_t length);
    ReceiveResult ReceiveExact(uint8_t* data, std::size_t length);
    void SetLastSocketError(const char* operation);

    mutable std::mutex mutex_;
    SOCKET socket_;
    ULONG listenPort_;
    std::string lastError_;
};

bool ParseBluetoothAddress(const std::string& text, BTH_ADDR& address);
std::string FormatBluetoothAddress(BTH_ADDR address);
