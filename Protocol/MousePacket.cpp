#include "MousePacket.h"
#include <iostream>
#include <iomanip>

// Simulate mouse movement packet creation
InputEventPacket CreateMouseMovePacket(uint32_t seq, int dx, int dy) {
    InputEventPacket packet{};
    packet.type       = TYPE_INPUT_EVENT;
    packet.sourceID   = SOURCE_PC1;
    packet.eventType  = EVENT_MOVE;
    packet.actionFlag = 0; // Not used for MOVE
    packet.seqNum     = seq;

    MouseEventPayload payload{};
    payload.buttonMask = 0;
    payload.dx = dx;
    payload.dy = dy;
    payload.data = 0;

    SerializeMousePayload(payload, packet.payload);
    return packet;
}

// Simulate a click event packet
InputEventPacket CreateClickPacket(uint32_t seq, uint8_t buttonMask, bool down) {
    InputEventPacket packet{};
    packet.type       = TYPE_INPUT_EVENT;
    packet.sourceID   = SOURCE_PC1;
    packet.eventType  = EVENT_CLICK;
    packet.actionFlag = down ? 1 : 0;
    packet.seqNum     = seq;

    MouseEventPayload payload{};
    payload.buttonMask = buttonMask;
    payload.dx = 0;
    payload.dy = 0;
    payload.data = 0;

    SerializeMousePayload(payload, packet.payload);
    return packet;
}

// Dump packet content for debugging
void PrintPacket(const InputEventPacket& packet) {
    std::cout << "Packet:\n";
    std::cout << "  Type:       0x" << std::hex << +packet.type << "\n";
    std::cout << "  SourceID:   " << std::dec << +packet.sourceID << "\n";
    std::cout << "  EventType:  0x" << std::hex << +packet.eventType << "\n";
    std::cout << "  ActionFlag: " << std::dec << +packet.actionFlag << "\n";
    std::cout << "  SeqNum:     " << packet.seqNum << "\n";

    MouseEventPayload payload;
    DeserializeMousePayload(packet.payload, payload);
    std::cout << "  Payload:\n";
    std::cout << "    Buttons:  " << std::hex << +payload.buttonMask << "\n";
    std::cout << "    dx:       " << std::dec << payload.dx << "\n";
    std::cout << "    dy:       " << payload.dy << "\n";
    std::cout << "    data:     " << payload.data << "\n";
}
