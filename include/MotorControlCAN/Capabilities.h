/** @file Capabilities.h
 * @brief Independent caller-supplied motor, feedback and operation availability facts.
 * SPDX-License-Identifier: MIT
 */
#pragma once
#include <MotorControlCAN/Status.h>

namespace MotorControlCAN {
/** Motor technology does not select a protocol, feedback source or command scale. */
enum class MotorKind : uint8_t {
    UNKNOWN,    ///< Technology unavailable.
    STEPPER,    ///< No full-step geometry is implied.
    BLDC,       ///< No Hall/encoder configuration is implied.
    BRUSHED_DC  ///< No feedback configuration is implied.
};
/** Selected feedback fact; its physical scale is supplied independently in UnitConfig. */
enum class FeedbackKind : uint8_t {
    UNKNOWN, ///< Selection unavailable.
    NONE,    ///< Explicit configuration without feedback.
    ENCODER, ///< Resolution and coordinate basis remain separate facts.
    HALL     ///< Does not imply encoder counts or a command scale.
};
/** Availability for one selected configuration/intent, not automatic profile detection.
 * False means unavailable. Applications own evidence, freshness and invalidation.
 * No field calibrates, enables, selects a mode or actuates a brake.
 */
struct Capabilities {
    MotorKind motor = MotorKind::UNKNOWN; ///< Independent selected motor fact.
    FeedbackKind feedback = FeedbackKind::UNKNOWN; ///< Independent selected feedback fact.
    bool position = false; ///< Position intent available under this configuration.
    bool velocity = false; ///< Velocity intent available under this configuration.
    bool torque = false; ///< Torque capability, not inferred from current settings.
    bool holdingBrake = false; ///< Mechanical holding brake, distinct from electrical braking modes.
    bool calibrationRequired = false; ///< This intent requires caller-qualified calibration.
    bool calibrationQualified = false; ///< Required calibration evidence supplied by the caller.
};
/** One explicitly requested availability check; no action is performed. */
enum class Capability : uint8_t {
    POSITION,      ///< Position intent.
    VELOCITY,      ///< Velocity intent.
    TORQUE,        ///< Torque intent.
    HOLDING_BRAKE  ///< Mechanical holding-brake capability.
};
/** Local capability-check details, never a native error or application health. */
enum class CapabilityError : int32_t {
    ARGUMENT = 1, ///< Invalid enum selection.
    UNAVAILABLE,  ///< Requested capability unavailable.
    CALIBRATION   ///< Required calibration is not qualified.
};
/** Check supplied availability and the calibration requirement for this intent.
 * @param facts Borrowed caller facts, unchanged and never retained.
 * @param requested One defined capability.
 * @return OK when declared available and qualified; UNSUPPORTED for absence,
 * INVALID_CONFIG for invalid enums or required unqualified calibration.
 * Pure and bounded: no I/O, allocation, clock, discovery or implicit action.
 */
Status requireCapability(const Capabilities& facts, Capability requested) noexcept;
} // namespace MotorControlCAN
