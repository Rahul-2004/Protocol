#include "../Protocol/MousePacket.h"

#include <cassert>
#include <cstdint>
#include <iostream>

int main() {
    auto hello = CreateHelloPacket(SOURCE_PC2, STATE_LOCAL_CONTROL, 7, 1234);
    assert(hello.type == TYPE_HELLO);
    assert(hello.sourceID == SOURCE_PC2);
    assert(hello.seqNum == 7);
    assert(GetPacketTimestampMs(hello) == 1234);
    assert(ValidateChecksum(hello));

    const auto bytes = SerializePacket(hello);
    InputEventPacket decoded{};
    assert(DeserializePacket(bytes.data(), bytes.size(), decoded));
    assert(decoded.type == TYPE_HELLO);
    assert(decoded.seqNum == 7);
    assert(ValidateChecksum(decoded));

    auto move = CreateMouseMovePacket(SOURCE_PC1, 10, 5, -3, 2222);
    assert(move.type == TYPE_INPUT_EVENT);
    assert(move.eventType == EVENT_MOVE);
    assert(GetPacketValueA(move) == 5);
    assert(GetPacketValueB(move) == -3);
    assert(ValidateChecksum(move));

    PacketDecision accepted = ShouldAcceptInputPacket(move, SOURCE_PC2, SOURCE_PC1, 8);
    assert(accepted.accepted);
    assert(accepted.missingPackets == 1);

    PacketDecision stale = ShouldAcceptInputPacket(move, SOURCE_PC2, SOURCE_PC1, 10);
    assert(!stale.accepted);

    auto looped = CreateMouseMovePacket(SOURCE_PC2, 11, 1, 1, 2223);
    PacketDecision loop = ShouldAcceptInputPacket(looped, SOURCE_PC2, SOURCE_PC1, 10);
    assert(!loop.accepted);

    auto corrupted = move;
    corrupted.payload[0] ^= 0xff;
    assert(!ValidateChecksum(corrupted));

    std::cout << "protocol self-test passed\n";
    return 0;
}
