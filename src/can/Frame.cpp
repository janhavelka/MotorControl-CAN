// SPDX-License-Identifier: MIT
#include <MotorControlCAN/can/Frame.h>

namespace MotorControlCAN {

Status validateFrame(const Frame& frame) noexcept {
    if (frame.fd) return Status(Err::UNSUPPORTED, 0, "Classical frame has no FD payload path");
    const uint32_t maximum = frame.extended ? 0x1FFFFFFFUL : 0x7FFUL;
    if (frame.brs || frame.esi || frame.id > maximum || frame.dlc > 8 ||
        frame.payloadLength > sizeof frame.data ||
        (frame.rtr && frame.payloadLength != 0) ||
        (!frame.rtr && frame.payloadLength != frame.dlc)) {
        return Status(Err::FRAME_ERROR, 0, "invalid CAN frame shape");
    }
    return Ok();
}

} // namespace MotorControlCAN
