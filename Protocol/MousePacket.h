#pragma once
#include <cstdint>
#include <cstring>

#pragma pack(push, 1)

struct MousePacket {
    uint8_t control;    // bitmask for buttons
    uint8_t eventType;  // type of event
    int16_t dx;
    int16_t dy;
    int16_t data;       // scroll or reserved
};

#pragma pack(pop)

// --- Control Flags ---
constexpr uint8_t CTRL_LEFT   = 0x01;
constexpr uint8_t CTRL_RIGHT  = 0x02;
constexpr uint8_t CTRL_MIDDLE = 0x04;

// --- Event Types ---
enum EventType : uint8_t {
    MOVE       = 0x01,
    CLICK      = 0x02,
    SCROLL     = 0x03,
    TOGGLE_ON  = 0x10,
    TOGGLE_OFF = 0x11,
    HELLO      = 0x12,
    HEARTBEAT  = 0x13,
};

// --- Serialization ---
inline void SerializeMousePacket(const MousePacket& packet, uint8_t* buffer) {
    std::memcpy(buffer, &packet, sizeof(MousePacket));
}

inline void DeserializeMousePacket(const uint8_t* buffer, MousePacket& packet) {
    std::memcpy(&packet, buffer, sizeof(MousePacket));
}
