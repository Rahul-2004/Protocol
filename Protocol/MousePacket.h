#pragma once
#include <cstdint>
#include <cstring>

#pragma pack(push, 1)

// --- Core Packet Header ---
struct InputEventPacket {
    uint8_t type;         // e.g., 0x01 = INPUT_EVENT
    uint8_t sourceID;     // 0x01 or 0x02
    uint8_t eventType;    // MOVE, CLICK, etc.
    uint8_t actionFlag;   // e.g., down/up
    uint32_t seqNum;      // uint32, big endian if cross-platform
    uint8_t payload[8];   // max size to fit within 20 bytes total (BLE safe)
};

// --- Payload: Mouse Events ---
struct MouseEventPayload {
    uint8_t buttonMask;
    int16_t dx;
    int16_t dy;
    int16_t data;  // scroll amount or 0
};

// Optional: enum for source/event/action
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

// Serialization helpers
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
