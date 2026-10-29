// SPDX-License-Identifier: MIT
#include <MotorControlCAN/canopen/Network.h>

#include <stddef.h>

namespace MotorControlCAN {
namespace CANOPEN {
namespace {
bool overlaps(const void* a, size_t aSize, const void* b, size_t bSize) {
    const uintptr_t first = reinterpret_cast<uintptr_t>(a);
    const uintptr_t second = reinterpret_cast<uintptr_t>(b);
    return first <= second ? second - first < aSize : first - second < bSize;
}
} // namespace

Status makeNmtRequest(NmtCommand command, uint8_t target, Frame& output) noexcept {
    switch (command) {
    case NmtCommand::START:
    case NmtCommand::STOP:
    case NmtCommand::PRE_OPERATIONAL:
    case NmtCommand::RESET_NODE:
    case NmtCommand::RESET_COMMUNICATION: break;
    default: return Status(Err::UNSUPPORTED, 0, "unsupported NMT command");
    }
    if (target > 127)
        return Status(Err::INVALID_CONFIG, 0, "invalid NMT target");
    Frame next;
    next.dlc = next.payloadLength = 2;
    next.data[0] = static_cast<uint8_t>(command);
    next.data[1] = target;
    output = next;
    return Ok();
}

Status validateNetworkRoute(const NetworkRoute& route) noexcept {
    if (!validateBinding(route.binding) || route.binding.endpoint.protocol != Protocol::CANOPEN ||
        !route.binding.endpoint.address || route.binding.endpoint.address > 127 ||
        !route.generation || !route.heartbeatId || route.heartbeatId > 0x7ff ||
        !route.emcyId || route.emcyId > 0x7ff || route.heartbeatId == route.emcyId)
        return Status(Err::INVALID_CONFIG, 0, "invalid configured CANopen network route");
    return Ok();
}

Status checkNetworkEvent(const Frame& frame, const NetworkRoute& expected,
                         const Binding& sourceBinding, Generation sourceGeneration,
                         NetworkEvent& output) noexcept {
    if (overlaps(&frame, sizeof frame, &output, sizeof output) ||
        overlaps(&expected, sizeof expected, &output, sizeof output) ||
        overlaps(&sourceBinding, sizeof sourceBinding, &output, sizeof output))
        return Status(Err::ILLEGAL_VALUE, 0, "network output overlaps borrowed input");
    Status status = validateNetworkRoute(expected);
    if (!status) return status;
    if (!sameBinding(expected.binding, sourceBinding) || expected.generation != sourceGeneration)
        return Status(Err::INVALID_CONFIG, 0, "network event provenance does not match route");
    status = validateFrame(frame);
    if (!status) return status;
    if (frame.extended || frame.rtr)
        return Status(Err::UNSUPPORTED, 0, "CANopen network route requires standard data frames");
    NetworkEvent next;
    next.route = expected;
    next.frame = frame;
    if (frame.id == expected.heartbeatId) {
        if (frame.payloadLength != 1)
            return Status(Err::FRAME_ERROR, 0, "heartbeat envelope requires one byte");
        switch (frame.data[0]) {
        case 0: next.state = NmtState::INITIALIZING; break;
        case 4: next.state = NmtState::STOPPED; break;
        case 5: next.state = NmtState::OPERATIONAL; break;
        case 127: next.state = NmtState::PRE_OPERATIONAL; break;
        default: return Status(Err::UNSUPPORTED, 0, "unsupported heartbeat state byte");
        }
        next.kind = next.state == NmtState::INITIALIZING ? NetworkEventKind::BOOTUP :
                                                          NetworkEventKind::HEARTBEAT;
    } else if (frame.id == expected.emcyId) {
        if (frame.payloadLength != 8)
            return Status(Err::FRAME_ERROR, 0, "emergency envelope requires eight bytes");
        next.kind = NetworkEventKind::EMCY;
        next.errorCode = static_cast<uint16_t>(uint16_t(frame.data[0]) |
                                               (uint16_t(frame.data[1]) << 8));
        next.errorRegister = frame.data[2];
        for (size_t i = 0; i < sizeof next.manufacturer; ++i) next.manufacturer[i] = frame.data[i + 3];
    } else {
        return Status(Err::FRAME_ERROR, 0, "identifier does not match configured network route");
    }
    output = next;
    return Ok();
}

} // namespace CANOPEN
} // namespace MotorControlCAN
