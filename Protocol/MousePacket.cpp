#include "MousePacket.h"

#include <chrono>
#include <cstring>
#include <iomanip>
#include <iostream>

namespace {
constexpr std::size_t kTimestampOffset = 0;
constexpr std::size_t kValueAOffset = 4;
constexpr std::size_t kValueBOffset = 6;
constexpr std::size_t kControlOffset = 8;
constexpr std::size_t kFlagsOffset = 9;
constexpr std::size_t kAuxOffset = 10;
constexpr std::size_t kChecksumPayloadOffset = 11;

uint32_t ReadU32LE(const uint8_t* data) {
    return static_cast<uint32_t>(data[0]) |
           (static_cast<uint32_t>(data[1]) << 8) |
           (static_cast<uint32_t>(data[2]) << 16) |
           (static_cast<uint32_t>(data[3]) << 24);
}

void WriteU32LE(uint8_t* data, uint32_t value) {
    data[0] = static_cast<uint8_t>(value & 0xffu);
    data[1] = static_cast<uint8_t>((value >> 8) & 0xffu);
    data[2] = static_cast<uint8_t>((value >> 16) & 0xffu);
    data[3] = static_cast<uint8_t>((value >> 24) & 0xffu);
}

int16_t ReadI16LE(const uint8_t* data) {
    const uint16_t value = static_cast<uint16_t>(data[0]) |
                           static_cast<uint16_t>(static_cast<uint16_t>(data[1]) << 8);
    return static_cast<int16_t>(value);
}

void WriteI16LE(uint8_t* data, int16_t value) {
    const auto raw = static_cast<uint16_t>(value);
    data[0] = static_cast<uint8_t>(raw & 0xffu);
    data[1] = static_cast<uint8_t>((raw >> 8) & 0xffu);
}

InputEventPacket CreatePacket(uint8_t type,
                              uint8_t sourceID,
                              uint8_t eventType,
                              uint8_t actionFlag,
                              uint32_t seq,
                              uint32_t timestampMs,
                              int16_t valueA,
                              int16_t valueB,
                              uint8_t control,
                              uint8_t flags,
                              uint8_t aux) {
    InputEventPacket packet{};
    packet.type = type;
    packet.sourceID = sourceID;
    packet.eventType = eventType;
    packet.actionFlag = actionFlag;
    packet.seqNum = seq;
    SetPacketTimestampMs(packet, timestampMs);
    SetPacketValueA(packet, valueA);
    SetPacketValueB(packet, valueB);
    SetPacketControl(packet, control);
    SetPacketFlags(packet, flags);
    packet.payload[kAuxOffset] = aux;
    FinalizePacket(packet);
    return packet;
}
} // namespace

uint32_t NowMilliseconds32() {
    using clock = std::chrono::steady_clock;
    const auto now = std::chrono::duration_cast<std::chrono::milliseconds>(
        clock::now().time_since_epoch());
    return static_cast<uint32_t>(now.count());
}

uint32_t GetPacketTimestampMs(const InputEventPacket& packet) {
    return ReadU32LE(packet.payload + kTimestampOffset);
}

void SetPacketTimestampMs(InputEventPacket& packet, uint32_t timestampMs) {
    WriteU32LE(packet.payload + kTimestampOffset, timestampMs);
}

int16_t GetPacketValueA(const InputEventPacket& packet) {
    return ReadI16LE(packet.payload + kValueAOffset);
}

void SetPacketValueA(InputEventPacket& packet, int16_t value) {
    WriteI16LE(packet.payload + kValueAOffset, value);
}

int16_t GetPacketValueB(const InputEventPacket& packet) {
    return ReadI16LE(packet.payload + kValueBOffset);
}

void SetPacketValueB(InputEventPacket& packet, int16_t value) {
    WriteI16LE(packet.payload + kValueBOffset, value);
}

uint8_t GetPacketControl(const InputEventPacket& packet) {
    return packet.payload[kControlOffset];
}

void SetPacketControl(InputEventPacket& packet, uint8_t value) {
    packet.payload[kControlOffset] = value;
}

uint8_t GetPacketFlags(const InputEventPacket& packet) {
    return packet.payload[kFlagsOffset];
}

void SetPacketFlags(InputEventPacket& packet, uint8_t value) {
    packet.payload[kFlagsOffset] = value;
}

uint8_t ComputeChecksum(const InputEventPacket& packet) {
    const auto bytes = SerializePacket(packet);
    uint8_t checksum = 0;
    for (std::size_t i = 0; i < PACKET_CHECKSUM_OFFSET; ++i) {
        checksum ^= bytes[i];
    }
    return checksum;
}

void FinalizePacket(InputEventPacket& packet) {
    packet.payload[kChecksumPayloadOffset] = 0;
    packet.payload[kChecksumPayloadOffset] = ComputeChecksum(packet);
}

bool ValidateChecksum(const InputEventPacket& packet) {
    return packet.payload[kChecksumPayloadOffset] == ComputeChecksum(packet);
}

std::array<uint8_t, INPUT_EVENT_PACKET_SIZE> SerializePacket(const InputEventPacket& packet) {
    std::array<uint8_t, INPUT_EVENT_PACKET_SIZE> buffer{};
    SerializePacket(packet, buffer.data());
    return buffer;
}

void SerializePacket(const InputEventPacket& packet, uint8_t* buffer) {
    buffer[0] = packet.type;
    buffer[1] = packet.sourceID;
    buffer[2] = packet.eventType;
    buffer[3] = packet.actionFlag;
    WriteU32LE(buffer + 4, packet.seqNum);
    std::memcpy(buffer + 8, packet.payload, sizeof(packet.payload));
}

bool DeserializePacket(const uint8_t* buffer, std::size_t length, InputEventPacket& packet) {
    if (length < INPUT_EVENT_PACKET_SIZE) {
        return false;
    }

    packet.type = buffer[0];
    packet.sourceID = buffer[1];
    packet.eventType = buffer[2];
    packet.actionFlag = buffer[3];
    packet.seqNum = ReadU32LE(buffer + 4);
    std::memcpy(packet.payload, buffer + 8, sizeof(packet.payload));
    return true;
}

void DeserializePacket(const uint8_t* buffer, InputEventPacket& packet) {
    (void)DeserializePacket(buffer, INPUT_EVENT_PACKET_SIZE, packet);
}

void SerializeMousePayload(const MouseEventPayload& src, uint8_t* out) {
    WriteI16LE(out + 0, src.dx);
    WriteI16LE(out + 2, src.dy);
    out[4] = src.buttonMask;
    WriteI16LE(out + 5, src.data);
    WriteI16LE(out + 7, static_cast<int16_t>(src.keyCode));
    out[9] = src.flags;
    out[10] = 0;
    out[11] = 0;
}

void DeserializeMousePayload(const uint8_t* in, MouseEventPayload& dst) {
    dst.dx = ReadI16LE(in + 0);
    dst.dy = ReadI16LE(in + 2);
    dst.buttonMask = in[4];
    dst.data = ReadI16LE(in + 5);
    dst.keyCode = static_cast<uint16_t>(ReadI16LE(in + 7));
    dst.flags = in[9];
}

InputEventPacket CreateHelloPacket(uint8_t sourceID, RelayState state, uint32_t lastSeq, uint32_t timestampMs) {
    return CreatePacket(TYPE_HELLO, sourceID, EVENT_NONE, static_cast<uint8_t>(state), lastSeq,
                        timestampMs, 0, 0, static_cast<uint8_t>(state), 0, 0);
}

InputEventPacket CreateAckPacket(uint8_t sourceID, RelayState state, uint32_t currentSeq, uint32_t timestampMs) {
    return CreatePacket(TYPE_ACK, sourceID, EVENT_NONE, static_cast<uint8_t>(state), currentSeq,
                        timestampMs, 0, 0, static_cast<uint8_t>(state), 0, 0);
}

InputEventPacket CreateTogglePacket(bool forwarding, uint8_t sourceID, uint32_t seq, uint32_t timestampMs) {
    return CreatePacket(forwarding ? TYPE_TOGGLE_ON : TYPE_TOGGLE_OFF,
                        sourceID,
                        EVENT_NONE,
                        forwarding ? STATE_FORWARDING : STATE_LOCAL_CONTROL,
                        seq,
                        timestampMs,
                        0,
                        0,
                        forwarding ? STATE_FORWARDING : STATE_LOCAL_CONTROL,
                        0,
                        0);
}

InputEventPacket CreateHeartbeatPacket(uint8_t sourceID, uint32_t currentSeq, uint32_t timestampMs) {
    return CreatePacket(TYPE_HEARTBEAT, sourceID, EVENT_NONE, 0, currentSeq, timestampMs, 0, 0, 0, 0, 0);
}

InputEventPacket CreateFlushSeqPacket(uint8_t sourceID, uint32_t flushSeq, uint32_t timestampMs) {
    return CreatePacket(TYPE_FLUSH_SEQ, sourceID, EVENT_NONE, 0, flushSeq, timestampMs, 0, 0, 0, 0, 0);
}

InputEventPacket CreateMouseMovePacket(uint8_t sourceID, uint32_t seq, int16_t dx, int16_t dy, uint32_t timestampMs) {
    return CreatePacket(TYPE_INPUT_EVENT, sourceID, EVENT_MOVE, 0, seq, timestampMs, dx, dy, 0, 0, 0);
}

InputEventPacket CreateMouseMovePacket(uint32_t seq, int dx, int dy) {
    return CreateMouseMovePacket(SOURCE_PC1, seq, static_cast<int16_t>(dx), static_cast<int16_t>(dy));
}

InputEventPacket CreateClickPacket(uint8_t sourceID, uint32_t seq, uint8_t buttonMask, bool down, uint32_t timestampMs) {
    return CreatePacket(TYPE_INPUT_EVENT, sourceID, EVENT_CLICK, down ? 1 : 0, seq, timestampMs,
                        0, 0, buttonMask, 0, 0);
}

InputEventPacket CreateClickPacket(uint32_t seq, uint8_t buttonMask, bool down) {
    return CreateClickPacket(SOURCE_PC1, seq, buttonMask, down);
}

InputEventPacket CreateScrollPacket(uint8_t sourceID, uint32_t seq, int16_t wheelDelta, uint32_t timestampMs) {
    return CreatePacket(TYPE_INPUT_EVENT, sourceID, EVENT_SCROLL, 0, seq, timestampMs,
                        wheelDelta, 0, 0, 0, 0);
}

InputEventPacket CreateKeyboardPacket(uint8_t sourceID, uint32_t seq, uint16_t virtualKey, bool down, bool extended, uint32_t timestampMs) {
    return CreatePacket(TYPE_INPUT_EVENT,
                        sourceID,
                        EVENT_KEYBOARD,
                        down ? 1 : 0,
                        seq,
                        timestampMs,
                        static_cast<int16_t>(virtualKey),
                        0,
                        static_cast<uint8_t>(virtualKey & 0xffu),
                        extended ? 1 : 0,
                        static_cast<uint8_t>((virtualKey >> 8) & 0xffu));
}

PacketDecision ShouldAcceptInputPacket(const InputEventPacket& packet,
                                      uint8_t localSourceID,
                                      uint8_t expectedRemoteSourceID,
                                      uint32_t lastSeqProcessed) {
    if (packet.type != TYPE_INPUT_EVENT) {
        return {false, "not input event", 0};
    }
    if (!ValidateChecksum(packet)) {
        return {false, "checksum mismatch", 0};
    }
    if (packet.sourceID == localSourceID) {
        return {false, "loop prevention", 0};
    }
    if (packet.sourceID != expectedRemoteSourceID) {
        return {false, "unexpected source", 0};
    }
    if (packet.seqNum <= lastSeqProcessed) {
        return {false, "duplicate or stale sequence", 0};
    }

    const uint32_t missing = packet.seqNum > lastSeqProcessed + 1
        ? packet.seqNum - lastSeqProcessed - 1
        : 0;
    return {true, "accepted", missing};
}

std::string_view PacketTypeName(uint8_t type) {
    switch (type) {
    case TYPE_HELLO: return "HELLO";
    case TYPE_ACK: return "ACK";
    case TYPE_TOGGLE_ON: return "TOGGLE_ON";
    case TYPE_TOGGLE_OFF: return "TOGGLE_OFF";
    case TYPE_INPUT_EVENT: return "INPUT_EVENT";
    case TYPE_HEARTBEAT: return "HEARTBEAT";
    case TYPE_FLUSH_SEQ: return "FLUSH_SEQ";
    default: return "UNKNOWN";
    }
}

std::string_view EventTypeName(uint8_t eventType) {
    switch (eventType) {
    case EVENT_NONE: return "NONE";
    case EVENT_MOVE: return "MOUSE_MOVE";
    case EVENT_CLICK: return "MOUSE_BUTTON";
    case EVENT_KEYBOARD: return "KEYBOARD";
    case EVENT_SCROLL: return "SCROLL";
    default: return "UNKNOWN";
    }
}

std::string_view RelayStateName(uint8_t state) {
    switch (state) {
    case STATE_LOCAL_CONTROL: return "LOCAL_CONTROL";
    case STATE_FORWARDING: return "FORWARDING";
    case STATE_DISCONNECTED: return "DISCONNECTED";
    default: return "UNKNOWN";
    }
}

void PrintPacket(const InputEventPacket& packet) {
    std::cout << "Packet " << PacketTypeName(packet.type) << "\n";
    std::cout << "  Source:    0x" << std::hex << static_cast<int>(packet.sourceID) << std::dec << "\n";
    std::cout << "  Event:     " << EventTypeName(packet.eventType) << "\n";
    std::cout << "  Action:    " << static_cast<int>(packet.actionFlag) << "\n";
    std::cout << "  Seq:       " << packet.seqNum << "\n";
    std::cout << "  Timestamp: " << GetPacketTimestampMs(packet) << " ms\n";
    std::cout << "  ValueA:    " << GetPacketValueA(packet) << "\n";
    std::cout << "  ValueB:    " << GetPacketValueB(packet) << "\n";
    std::cout << "  Control:   0x" << std::hex << static_cast<int>(GetPacketControl(packet)) << std::dec << "\n";
    std::cout << "  Checksum:  " << (ValidateChecksum(packet) ? "ok" : "bad") << "\n";
}
