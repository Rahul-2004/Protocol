#include "Observability.h"

#include <ctime>
#include <iomanip>
#include <iostream>
#include <sstream>

namespace {
std::string FormatClock(std::chrono::system_clock::time_point timePoint) {
    const std::time_t time = std::chrono::system_clock::to_time_t(timePoint);
    std::tm localTime{};
#if defined(_WIN32)
    localtime_s(&localTime, &time);
#else
    localtime_r(&time, &localTime);
#endif

    std::ostringstream out;
    out << std::put_time(&localTime, "%H:%M:%S");
    return out.str();
}

const char* OnOff(bool value) {
    return value ? "ON" : "OFF";
}
} // namespace

ObservabilityDashboard::ObservabilityDashboard()
    : running_(false),
      connected_(false),
      forwarding_(false),
      packetsSent_(0),
      packetsReceived_(0),
      packetsDropped_(0),
      packetsLost_(0),
      reconnects_(0),
      lastSeqSent_(0),
      lastSeqReceived_(0),
      lastLatencyMs_(0),
      averageLatencyMs_(0.0),
      latencySamples_(0),
      lastHeartbeat_(std::chrono::steady_clock::now()) {}

ObservabilityDashboard::~ObservabilityDashboard() {
    Stop();
}

void ObservabilityDashboard::Start(const std::string& roleName) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (running_) {
        return;
    }

    roleName_ = roleName;
    running_ = true;
    renderThread_ = std::thread(&ObservabilityDashboard::RenderLoop, this);
}

void ObservabilityDashboard::Stop() {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!running_) {
            return;
        }
        running_ = false;
    }
    cv_.notify_all();
    if (renderThread_.joinable()) {
        renderThread_.join();
    }
}

void ObservabilityDashboard::SetConnection(bool connected) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (connected_ != connected) {
        connected_ = connected;
        PushEventLocked(connected ? "connection established" : "connection lost");
    }
}

void ObservabilityDashboard::SetForwarding(bool forwarding) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (forwarding_ != forwarding) {
        forwarding_ = forwarding;
        PushEventLocked(forwarding ? "Forwarding Active" : "Back to Local Control");
    }
}

void ObservabilityDashboard::SetPeerState(const std::string& state) {
    std::lock_guard<std::mutex> lock(mutex_);
    peerState_ = state;
}

void ObservabilityDashboard::RecordSent(const InputEventPacket& packet) {
    std::lock_guard<std::mutex> lock(mutex_);
    ++packetsSent_;
    lastSeqSent_ = packet.seqNum;
}

void ObservabilityDashboard::RecordReceived(const InputEventPacket& packet) {
    std::lock_guard<std::mutex> lock(mutex_);
    ++packetsReceived_;
    if (packet.type == TYPE_INPUT_EVENT) {
        lastSeqReceived_ = packet.seqNum;
    }
}

void ObservabilityDashboard::RecordDrop(const std::string& reason) {
    std::lock_guard<std::mutex> lock(mutex_);
    ++packetsDropped_;
    PushEventLocked("drop: " + reason);
}

void ObservabilityDashboard::RecordPacketLoss(uint32_t missingPackets) {
    if (missingPackets == 0) {
        return;
    }

    std::lock_guard<std::mutex> lock(mutex_);
    packetsLost_ += missingPackets;
    PushEventLocked("packet gap detected: " + std::to_string(missingPackets));
}

void ObservabilityDashboard::RecordReconnect() {
    std::lock_guard<std::mutex> lock(mutex_);
    ++reconnects_;
    PushEventLocked("reconnect attempt " + std::to_string(reconnects_));
}

void ObservabilityDashboard::RecordLatency(uint32_t latencyMs) {
    std::lock_guard<std::mutex> lock(mutex_);
    lastLatencyMs_ = latencyMs;
    ++latencySamples_;
    averageLatencyMs_ += (static_cast<double>(latencyMs) - averageLatencyMs_) /
                         static_cast<double>(latencySamples_);
}

void ObservabilityDashboard::RecordHeartbeat() {
    std::lock_guard<std::mutex> lock(mutex_);
    lastHeartbeat_ = std::chrono::steady_clock::now();
}

void ObservabilityDashboard::AddEvent(const std::string& event) {
    std::lock_guard<std::mutex> lock(mutex_);
    PushEventLocked(event);
}

void ObservabilityDashboard::RenderLoop() {
    std::unique_lock<std::mutex> lock(mutex_);
    while (running_) {
        lock.unlock();
        RenderSnapshot();
        lock.lock();
        cv_.wait_for(lock, std::chrono::milliseconds(1000), [this] { return !running_; });
    }
}

void ObservabilityDashboard::RenderSnapshot() {
    std::lock_guard<std::mutex> lock(mutex_);

    const auto now = std::chrono::steady_clock::now();
    const auto heartbeatAge = std::chrono::duration_cast<std::chrono::seconds>(now - lastHeartbeat_).count();

    std::cout << "\x1b[2J\x1b[H";
    std::cout << "Cross-PC Input Relay Dashboard\n";
    std::cout << "Role:        " << roleName_ << "\n";
    std::cout << "Connected:   " << OnOff(connected_) << "\n";
    std::cout << "Forwarding:  " << OnOff(forwarding_) << "\n";
    std::cout << "Peer state:  " << (peerState_.empty() ? "unknown" : peerState_) << "\n";
    std::cout << "Last HB age: " << heartbeatAge << "s\n";
    std::cout << "\n";
    std::cout << "Packets sent:      " << packetsSent_ << "\n";
    std::cout << "Packets received:  " << packetsReceived_ << "\n";
    std::cout << "Packets dropped:   " << packetsDropped_ << "\n";
    std::cout << "Estimated loss:    " << packetsLost_ << "\n";
    std::cout << "Reconnect events:  " << reconnects_ << "\n";
    std::cout << "Last seq sent:     " << lastSeqSent_ << "\n";
    std::cout << "Last seq received: " << lastSeqReceived_ << "\n";
    std::cout << "Last latency:      " << lastLatencyMs_ << " ms\n";
    std::cout << "Avg latency:       " << std::fixed << std::setprecision(2) << averageLatencyMs_ << " ms\n";
    std::cout << "\nRecent events:\n";

    for (const auto& event : events_) {
        std::cout << "  [" << FormatClock(event.at) << "] " << event.text << "\n";
    }
    std::cout.flush();
}

void ObservabilityDashboard::PushEventLocked(const std::string& event) {
    events_.push_back({std::chrono::system_clock::now(), event});
    while (events_.size() > 8) {
        events_.pop_front();
    }
}
