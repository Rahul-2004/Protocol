#include "BluetoothTransport.h"

#include <algorithm>
#include <cctype>
#include <iomanip>
#include <sstream>
#include <utility>

#if defined(_WIN32)
#pragma comment(lib, "Ws2_32.lib")
#pragma comment(lib, "Bthprops.lib")

namespace {
std::string SocketErrorText(int errorCode) {
    std::ostringstream out;
    out << "WSA error " << errorCode;
    return out.str();
}
}

WinsockSession::WinsockSession() : started_(false) {
    WSADATA data{};
    const int rc = WSAStartup(MAKEWORD(2, 2), &data);
    if (rc != 0) {
        lastError_ = "WSAStartup failed: " + SocketErrorText(rc);
        return;
    }
    started_ = true;
}

WinsockSession::~WinsockSession() {
    if (started_) {
        WSACleanup();
    }
}

bool WinsockSession::Started() const {
    return started_;
}

const std::string& WinsockSession::LastError() const {
    return lastError_;
}

BluetoothTransport::BluetoothTransport()
    : socket_(INVALID_SOCKET), listenPort_(0) {}

BluetoothTransport::~BluetoothTransport() {
    Close();
}

BluetoothTransport::BluetoothTransport(BluetoothTransport&& other) noexcept
    : socket_(INVALID_SOCKET), listenPort_(0) {
    std::lock_guard<std::mutex> lock(other.mutex_);
    socket_ = other.socket_;
    listenPort_ = other.listenPort_;
    lastError_ = std::move(other.lastError_);
    other.socket_ = INVALID_SOCKET;
    other.listenPort_ = 0;
}

BluetoothTransport& BluetoothTransport::operator=(BluetoothTransport&& other) noexcept {
    if (this == &other) {
        return *this;
    }

    Close();
    std::scoped_lock lock(mutex_, other.mutex_);
    socket_ = other.socket_;
    listenPort_ = other.listenPort_;
    lastError_ = std::move(other.lastError_);
    other.socket_ = INVALID_SOCKET;
    other.listenPort_ = 0;
    return *this;
}

bool BluetoothTransport::Connect(BTH_ADDR address, ULONG port) {
    Close();

    SOCKET socket = ::socket(AF_BTH, SOCK_STREAM, BTHPROTO_RFCOMM);
    if (socket == INVALID_SOCKET) {
        SetLastSocketError("socket");
        return false;
    }

    SOCKADDR_BTH addr{};
    addr.addressFamily = AF_BTH;
    addr.btAddr = address;
    addr.port = port;

    if (::connect(socket, reinterpret_cast<SOCKADDR*>(&addr), sizeof(addr)) == SOCKET_ERROR) {
        SetLastSocketError("connect");
        closesocket(socket);
        return false;
    }

    std::lock_guard<std::mutex> lock(mutex_);
    socket_ = socket;
    listenPort_ = 0;
    lastError_.clear();
    return true;
}

bool BluetoothTransport::Listen(ULONG port) {
    Close();

    SOCKET socket = ::socket(AF_BTH, SOCK_STREAM, BTHPROTO_RFCOMM);
    if (socket == INVALID_SOCKET) {
        SetLastSocketError("socket");
        return false;
    }

    SOCKADDR_BTH addr{};
    addr.addressFamily = AF_BTH;
    addr.btAddr = 0;
    addr.port = port == 0 ? BT_PORT_ANY : port;

    if (::bind(socket, reinterpret_cast<SOCKADDR*>(&addr), sizeof(addr)) == SOCKET_ERROR) {
        SetLastSocketError("bind");
        closesocket(socket);
        return false;
    }

    int addrLen = sizeof(addr);
    if (::getsockname(socket, reinterpret_cast<SOCKADDR*>(&addr), &addrLen) == SOCKET_ERROR) {
        SetLastSocketError("getsockname");
        closesocket(socket);
        return false;
    }

    if (::listen(socket, 1) == SOCKET_ERROR) {
        SetLastSocketError("listen");
        closesocket(socket);
        return false;
    }

    std::lock_guard<std::mutex> lock(mutex_);
    socket_ = socket;
    listenPort_ = addr.port;
    lastError_.clear();
    return true;
}

bool BluetoothTransport::Accept(BluetoothTransport& client) {
    SOCKET accepted = INVALID_SOCKET;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (socket_ == INVALID_SOCKET) {
            lastError_ = "accept failed: socket is closed";
            return false;
        }
        accepted = ::accept(socket_, nullptr, nullptr);
    }

    if (accepted == INVALID_SOCKET) {
        SetLastSocketError("accept");
        return false;
    }

    client.Attach(accepted);
    return true;
}

bool BluetoothTransport::Attach(SOCKET socket) {
    Close();
    std::lock_guard<std::mutex> lock(mutex_);
    socket_ = socket;
    listenPort_ = 0;
    lastError_.clear();
    return socket_ != INVALID_SOCKET;
}

bool BluetoothTransport::SendPacket(const InputEventPacket& packet) {
    const auto buffer = SerializePacket(packet);
    return SendAll(buffer.data(), buffer.size());
}

ReceiveResult BluetoothTransport::ReceivePacket(InputEventPacket& packet) {
    uint8_t buffer[INPUT_EVENT_PACKET_SIZE]{};
    const ReceiveResult result = ReceiveExact(buffer, sizeof(buffer));
    if (result != ReceiveResult::Packet) {
        return result;
    }

    if (!DeserializePacket(buffer, sizeof(buffer), packet)) {
        lastError_ = "deserialize failed";
        return ReceiveResult::Error;
    }
    return ReceiveResult::Packet;
}

bool BluetoothTransport::SetReceiveTimeoutMs(int timeoutMs) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (socket_ == INVALID_SOCKET) {
        lastError_ = "setsockopt failed: socket is closed";
        return false;
    }

    DWORD timeout = static_cast<DWORD>(timeoutMs);
    if (::setsockopt(socket_, SOL_SOCKET, SO_RCVTIMEO,
                     reinterpret_cast<const char*>(&timeout), sizeof(timeout)) == SOCKET_ERROR) {
        SetLastSocketError("setsockopt(SO_RCVTIMEO)");
        return false;
    }
    return true;
}

void BluetoothTransport::Close() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (socket_ != INVALID_SOCKET) {
        closesocket(socket_);
        socket_ = INVALID_SOCKET;
    }
    listenPort_ = 0;
}

bool BluetoothTransport::IsOpen() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return socket_ != INVALID_SOCKET;
}

ULONG BluetoothTransport::ListenPort() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return listenPort_;
}

const std::string& BluetoothTransport::LastError() const {
    return lastError_;
}

bool BluetoothTransport::SendAll(const uint8_t* data, std::size_t length) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (socket_ == INVALID_SOCKET) {
        lastError_ = "send failed: socket is closed";
        return false;
    }

    std::size_t sentTotal = 0;
    while (sentTotal < length) {
        const int sent = ::send(socket_,
                                reinterpret_cast<const char*>(data + sentTotal),
                                static_cast<int>(length - sentTotal),
                                0);
        if (sent == SOCKET_ERROR || sent == 0) {
            SetLastSocketError("send");
            return false;
        }
        sentTotal += static_cast<std::size_t>(sent);
    }
    return true;
}

ReceiveResult BluetoothTransport::ReceiveExact(uint8_t* data, std::size_t length) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (socket_ == INVALID_SOCKET) {
        lastError_ = "recv failed: socket is closed";
        return ReceiveResult::Closed;
    }

    std::size_t receivedTotal = 0;
    while (receivedTotal < length) {
        const int received = ::recv(socket_,
                                    reinterpret_cast<char*>(data + receivedTotal),
                                    static_cast<int>(length - receivedTotal),
                                    0);
        if (received == 0) {
            lastError_ = "peer closed connection";
            return ReceiveResult::Closed;
        }
        if (received == SOCKET_ERROR) {
            const int error = WSAGetLastError();
            if (error == WSAETIMEDOUT) {
                return ReceiveResult::Timeout;
            }
            lastError_ = "recv failed: " + SocketErrorText(error);
            return ReceiveResult::Error;
        }
        receivedTotal += static_cast<std::size_t>(received);
    }
    return ReceiveResult::Packet;
}

void BluetoothTransport::SetLastSocketError(const char* operation) {
    lastError_ = std::string(operation) + " failed: " + SocketErrorText(WSAGetLastError());
}

#else

WinsockSession::WinsockSession() : started_(false), lastError_("Bluetooth transport is Windows-only") {}
WinsockSession::~WinsockSession() = default;
bool WinsockSession::Started() const { return started_; }
const std::string& WinsockSession::LastError() const { return lastError_; }

BluetoothTransport::BluetoothTransport() : socket_(INVALID_SOCKET), listenPort_(0), lastError_("Bluetooth transport is Windows-only") {}
BluetoothTransport::~BluetoothTransport() = default;
BluetoothTransport::BluetoothTransport(BluetoothTransport&& other) noexcept
    : socket_(other.socket_), listenPort_(other.listenPort_), lastError_(std::move(other.lastError_)) {
    other.socket_ = INVALID_SOCKET;
    other.listenPort_ = 0;
}
BluetoothTransport& BluetoothTransport::operator=(BluetoothTransport&& other) noexcept {
    if (this != &other) {
        socket_ = other.socket_;
        listenPort_ = other.listenPort_;
        lastError_ = std::move(other.lastError_);
        other.socket_ = INVALID_SOCKET;
        other.listenPort_ = 0;
    }
    return *this;
}
bool BluetoothTransport::Connect(BTH_ADDR, ULONG) { return false; }
bool BluetoothTransport::Listen(ULONG) { return false; }
bool BluetoothTransport::Accept(BluetoothTransport&) { return false; }
bool BluetoothTransport::Attach(SOCKET) { return false; }
bool BluetoothTransport::SendPacket(const InputEventPacket&) { return false; }
ReceiveResult BluetoothTransport::ReceivePacket(InputEventPacket&) { return ReceiveResult::Error; }
bool BluetoothTransport::SetReceiveTimeoutMs(int) { return false; }
void BluetoothTransport::Close() {}
bool BluetoothTransport::IsOpen() const { return false; }
ULONG BluetoothTransport::ListenPort() const { return 0; }
const std::string& BluetoothTransport::LastError() const { return lastError_; }
bool BluetoothTransport::SendAll(const uint8_t*, std::size_t) { return false; }
ReceiveResult BluetoothTransport::ReceiveExact(uint8_t*, std::size_t) { return ReceiveResult::Error; }
void BluetoothTransport::SetLastSocketError(const char*) {}

#endif

bool ParseBluetoothAddress(const std::string& text, BTH_ADDR& address) {
    std::string digits;
    digits.reserve(12);
    for (char ch : text) {
        if (ch == ':' || ch == '-' || ch == ' ') {
            continue;
        }
        if (ch == 'x' || ch == 'X') {
            continue;
        }
        if (!std::isxdigit(static_cast<unsigned char>(ch))) {
            return false;
        }
        digits.push_back(ch);
    }

    if (digits.empty() || digits.size() > 16) {
        return false;
    }

    uint64_t value = 0;
    std::istringstream in(digits);
    in >> std::hex >> value;
    if (!in || !in.eof()) {
        return false;
    }

    address = static_cast<BTH_ADDR>(value);
    return true;
}

std::string FormatBluetoothAddress(BTH_ADDR address) {
    std::ostringstream out;
    out << std::hex << std::setfill('0') << std::setw(12)
        << static_cast<uint64_t>(address);
    return out.str();
}
