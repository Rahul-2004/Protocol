#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string_view>

#pragma pack(push, 1)

// Fixed 20-byte wire packet. The 12-byte payload is intentionally retained
// for compatibility with the earlier prototype while reserving payload[11]
// as the XOR checksum byte.
struct InputEventPacket {
    uint8_t type;
    uint8_t sourceID;
    uint8_t eventType;
    uint8_t actionFlag;
    uint32_t seqNum;
    uint8_t payload[12];
};

#pragma pack(pop)

static_assert(sizeof(InputEventPacket) == 20, "InputEventPacket must remain 20 bytes");

struct MouseEventPayload {
    int16_t dx;
    int16_t dy;
    uint8_t buttonMask;
    int16_t data;
    uint16_t keyCode;
    uint8_t flags;
};

enum PacketType : uint8_t {
    TYPE_HELLO       = 0x10,
    TYPE_ACK         = 0x11,
    TYPE_TOGGLE_ON   = 0x12,
    TYPE_TOGGLE_OFF  = 0x13,
    TYPE_INPUT_EVENT = 0x14,
    TYPE_HEARTBEAT   = 0x15,
    TYPE_FLUSH_SEQ   = 0x16,
};

enum EventType : uint8_t {
    EVENT_NONE     = 0x00,
    EVENT_MOVE     = 0x01,
    EVENT_CLICK    = 0x02,
    EVENT_KEYBOARD = 0x03,
    EVENT_SCROLL   = 0x04,
};

enum RelayState : uint8_t {
    STATE_LOCAL_CONTROL = 0x00,
    STATE_FORWARDING    = 0x01,
    STATE_DISCONNECTED  = 0x02,
};

constexpr uint8_t SOURCE_PC1 = 0x01;
constexpr uint8_t SOURCE_PC2 = 0x02;

constexpr uint8_t BTN_LEFT   = 0x01;
constexpr uint8_t BTN_RIGHT  = 0x02;
constexpr uint8_t BTN_MIDDLE = 0x04;
constexpr uint8_t BTN_X1     = 0x08;
constexpr uint8_t BTN_X2     = 0x10;

constexpr std::size_t INPUT_EVENT_PACKET_SIZE = sizeof(InputEventPacket);
constexpr std::size_t PACKET_CHECKSUM_OFFSET = INPUT_EVENT_PACKET_SIZE - 1;

struct PacketDecision {
    bool accepted;
    const char* reason;
    uint32_t missingPackets;
};

uint32_t NowMilliseconds32();

uint32_t GetPacketTimestampMs(const InputEventPacket& packet);
void SetPacketTimestampMs(InputEventPacket& packet, uint32_t timestampMs);

int16_t GetPacketValueA(const InputEventPacket& packet);
void SetPacketValueA(InputEventPacket& packet, int16_t value);

int16_t GetPacketValueB(const InputEventPacket& packet);
void SetPacketValueB(InputEventPacket& packet, int16_t value);

uint8_t GetPacketControl(const InputEventPacket& packet);
void SetPacketControl(InputEventPacket& packet, uint8_t value);

uint8_t GetPacketFlags(const InputEventPacket& packet);
void SetPacketFlags(InputEventPacket& packet, uint8_t value);

uint8_t ComputeChecksum(const InputEventPacket& packet);
void FinalizePacket(InputEventPacket& packet);
bool ValidateChecksum(const InputEventPacket& packet);

std::array<uint8_t, INPUT_EVENT_PACKET_SIZE> SerializePacket(const InputEventPacket& packet);
void SerializePacket(const InputEventPacket& packet, uint8_t* buffer);
bool DeserializePacket(const uint8_t* buffer, std::size_t length, InputEventPacket& packet);
void DeserializePacket(const uint8_t* buffer, InputEventPacket& packet);

void SerializeMousePayload(const MouseEventPayload& src, uint8_t* out);
void DeserializeMousePayload(const uint8_t* in, MouseEventPayload& dst);

InputEventPacket CreateHelloPacket(uint8_t sourceID, RelayState state, uint32_t lastSeq, uint32_t timestampMs = NowMilliseconds32());
InputEventPacket CreateAckPacket(uint8_t sourceID, RelayState state, uint32_t currentSeq, uint32_t timestampMs = NowMilliseconds32());
InputEventPacket CreateTogglePacket(bool forwarding, uint8_t sourceID, uint32_t seq, uint32_t timestampMs = NowMilliseconds32());
InputEventPacket CreateHeartbeatPacket(uint8_t sourceID, uint32_t currentSeq, uint32_t timestampMs = NowMilliseconds32());
InputEventPacket CreateFlushSeqPacket(uint8_t sourceID, uint32_t flushSeq, uint32_t timestampMs = NowMilliseconds32());
InputEventPacket CreateMouseMovePacket(uint8_t sourceID, uint32_t seq, int16_t dx, int16_t dy, uint32_t timestampMs = NowMilliseconds32());
InputEventPacket CreateMouseMovePacket(uint32_t seq, int dx, int dy);
InputEventPacket CreateClickPacket(uint8_t sourceID, uint32_t seq, uint8_t buttonMask, bool down, uint32_t timestampMs = NowMilliseconds32());
InputEventPacket CreateClickPacket(uint32_t seq, uint8_t buttonMask, bool down);
InputEventPacket CreateScrollPacket(uint8_t sourceID, uint32_t seq, int16_t wheelDelta, uint32_t timestampMs = NowMilliseconds32());
InputEventPacket CreateKeyboardPacket(uint8_t sourceID, uint32_t seq, uint16_t virtualKey, bool down, bool extended, uint32_t timestampMs = NowMilliseconds32());

PacketDecision ShouldAcceptInputPacket(const InputEventPacket& packet,
                                      uint8_t localSourceID,
                                      uint8_t expectedRemoteSourceID,
                                      uint32_t lastSeqProcessed);

std::string_view PacketTypeName(uint8_t type);
std::string_view EventTypeName(uint8_t eventType);
std::string_view RelayStateName(uint8_t state);

void PrintPacket(const InputEventPacket& packet);
