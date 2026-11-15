// SPDX-License-Identifier: MIT
#include <MotorControlCAN/Capabilities.h>

namespace MotorControlCAN {
Status requireCapability(const Capabilities& facts, Capability requested) noexcept {
    if (facts.motor > MotorKind::BRUSHED_DC || facts.feedback > FeedbackKind::HALL)
        return Status(Err::INVALID_CONFIG, int32_t(CapabilityError::ARGUMENT), "invalid capability fact");
    bool available = false;
    switch (requested) {
    case Capability::POSITION: available = facts.position; break;
    case Capability::VELOCITY: available = facts.velocity; break;
    case Capability::TORQUE: available = facts.torque; break;
    case Capability::HOLDING_BRAKE: available = facts.holdingBrake; break;
    default:
        return Status(Err::INVALID_CONFIG, int32_t(CapabilityError::ARGUMENT), "invalid capability selection");
    }
    if (!available)
        return Status(Err::UNSUPPORTED, int32_t(CapabilityError::UNAVAILABLE), "capability unavailable");
    if (facts.calibrationRequired && !facts.calibrationQualified)
        return Status(Err::INVALID_CONFIG, int32_t(CapabilityError::CALIBRATION), "calibration unqualified");
    return Ok();
}
} // namespace MotorControlCAN
