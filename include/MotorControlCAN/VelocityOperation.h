/** @file VelocityOperation.h
 * @brief Finite velocity intent and local interruption vocabulary.
 * SPDX-License-Identifier: MIT
 */
#pragma once
#include <MotorControlCAN/ActionOperation.h>
#include <MotorControlCAN/Axis.h>

namespace MotorControlCAN {
/** A finite run; successful preparation starts durationUs, including setup and queue delay. */
struct FiniteVelocityRequest {
    VelocityRequest velocity; ///< Signed engineering rate, with explicit units and generation.
    uint64_t durationUs = 0; ///< Positive preparation-to-stop interval in nonwrapping microseconds.
};
/** Local withdrawal never sends a stop, enables a drive or changes a terminal result. */
enum class VelocityInterruption : uint8_t {
    NONE,        ///< No withdrawal.
    CANCELLED,   ///< Caller withdrew this operation; possible motion remains.
    STOP,        ///< Another explicit stop interrupted the run and owns its result.
    DRIVE_FAULT, ///< Checked fault, never automatically cleared.
    INVALIDATED, ///< Reservation, binding or node lifecycle changed.
    DEADLINE     ///< Inclusive processing-time cutoff reached.
};
/** Local velocity validation details; native errors remain in retained service evidence. */
enum class VelocityError : int32_t {
    ARGUMENT = 1, ///< Invalid request or policy.
    TARGET,       ///< Current provenance does not match preparation.
    STATE,        ///< Wrong phase or unproved prerequisite.
    TIME,         ///< Invalid time, interval or fixed deadline.
    CORRELATION,  ///< Evidence does not match yielded work.
    OVERLAP,      ///< Mutable output overlaps borrowed input.
    CAPABILITY,   ///< Required selected-firmware fact has no evidence.
    RATE_BASIS,   ///< Destination is not integer motor turns per minute.
    RANGE,        ///< Requested or effective native rate/ramp is outside its supported range.
    RATE_ERROR_BASE = 5000 ///< Offset for errors from pure exact rate preparation.
};
} // namespace MotorControlCAN
