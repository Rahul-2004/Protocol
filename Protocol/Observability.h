#pragma once

#include "MousePacket.h"

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <deque>
#include <mutex>
#include <string>
#include <thread>

class ObservabilityDashboard {
public:
    ObservabilityDashboard();
    ~ObservabilityDashboard();

    ObservabilityDashboard(const ObservabilityDashboard&) = delete;
    ObservabilityDashboard& operator=(const ObservabilityDashboard&) = delete;

    void Start(const std::string& roleName);
    void Stop();

    void SetConnection(bool connected);
    void SetForwarding(bool forwarding);
    void SetPeerState(const std::string& state);

    void RecordSent(const InputEventPacket& packet);
    void RecordReceived(const InputEventPacket& packet);
    void RecordDrop(const std::string& reason);
    void RecordPacketLoss(uint32_t missingPackets);
    void RecordReconnect();
    void RecordLatency(uint32_t latencyMs);
    void RecordHeartbeat();
    void AddEvent(const std::string& event);

private:
    struct EventEntry {
        std::chrono::system_clock::time_point at;
        std::string text;
    };

    void RenderLoop();
    void RenderSnapshot();
    void PushEventLocked(const std::string& event);

    std::mutex mutex_;
    std::condition_variable cv_;
    std::thread renderThread_;
    bool running_;

    std::string roleName_;
    bool connected_;
    bool forwarding_;
    std::string peerState_;

    uint64_t packetsSent_;
    uint64_t packetsReceived_;
    uint64_t packetsDropped_;
    uint64_t packetsLost_;
    uint64_t reconnects_;
    uint32_t lastSeqSent_;
    uint32_t lastSeqReceived_;
    uint32_t lastLatencyMs_;
    double averageLatencyMs_;
    uint64_t latencySamples_;
    std::chrono::steady_clock::time_point lastHeartbeat_;
    std::deque<EventEntry> events_;
};
