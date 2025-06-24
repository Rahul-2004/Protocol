#pragma once
#include <cstdint>
#include <cstring>

#pragma pack(push, 1)

// Core packet: 1 + 1 + 1 + 1 + 4 + 12 = 20 bytes
struct InputEventPacket {
    uint8_t type;
    uint8_t sourceID;
    uint8_t eventType;
    uint8_t actionFlag;
    uint32_t seqNum;
    uint8_t payload[12];  // increased from 8 to 12
};

struct MouseEventPayload {
    uint8_t buttonMask;
    float dx;
    float dy;
    int16_t data;
};

// Packet types
enum PacketType : uint8_t {
    TYPE_INPUT_EVENT = 0x01,
    TYPE_TOGGLE_ON   = 0x12,
    TYPE_TOGGLE_OFF  = 0x13,
    TYPE_HELLO       = 0x10,
    TYPE_ACK         = 0x11,
    TYPE_HEARTBEAT   = 0x15,
    TYPE_FLUSH_SEQ   = 0x16,
};

enum EventType : uint8_t {
    EVENT_MOVE   = 0x01,
    EVENT_CLICK  = 0x02,
    EVENT_SCROLL = 0x03,
};

constexpr uint8_t SOURCE_PC1 = 0x01;
constexpr uint8_t SOURCE_PC2 = 0x02;

// Helpers
inline void SerializeMousePayload(const MouseEventPayload& src, uint8_t* out) {
    std::memcpy(out, &src, sizeof(MouseEventPayload));
}

inline void DeserializeMousePayload(const uint8_t* in, MouseEventPayload& dst) {
    std::memcpy(&dst, in, sizeof(MouseEventPayload));
}

inline void SerializePacket(const InputEventPacket& packet, uint8_t* buffer) {
    std::memcpy(buffer, &packet, sizeof(InputEventPacket));
}

inline void DeserializePacket(const uint8_t* buffer, InputEventPacket& packet) {
    std::memcpy(&packet, buffer, sizeof(InputEventPacket));
}

#pragma pack(pop)
