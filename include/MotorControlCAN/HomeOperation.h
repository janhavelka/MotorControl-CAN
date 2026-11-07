/** @file HomeOperation.h
 * @brief Protocol-neutral homing interruption and validation vocabulary.
 * SPDX-License-Identifier: MIT
 */
#pragma once
#include <MotorControlCAN/ActionOperation.h>
#include <MotorControlCAN/Axis.h>

namespace MotorControlCAN {
/** Local termination never sends a stop, retries or resumes homing. */
enum class HomeInterruption : uint8_t {
    NONE,        ///< No withdrawal.
    CANCELLED,   ///< Caller ended local sequencing only.
    STOP,        ///< Separate explicit stop requested; its result remains independent.
    DRIVE_FAULT, ///< Independently routed drive fault.
    INVALIDATED, ///< Binding, configuration, reservation or lifecycle withdrawn.
    DEADLINE     ///< Immutable processing cutoff reached.
};
/** Function-specific Status::detail; native service errors stay in retained terminals. */
enum class HomeError : int32_t {
    NONE,                  ///< No error.
    ARGUMENT,              ///< Invalid method, policy, budget or capacity.
    TARGET,                ///< Frozen binding or generation no longer matches.
    STATE,                 ///< Phase or drive prerequisite unavailable.
    TIME,                  ///< Backward time, expired deadline or insufficient budget.
    CORRELATION,           ///< Terminal does not match the yielded service.
    OVERLAP,               ///< Mutable storage overlaps a borrowed input.
    CAPABILITY,            ///< Selected specimen, firmware or method qualification absent.
    RATE_BASIS,            ///< Rate destination is not integer motor turns per minute.
    RANGE,                 ///< Parameter exceeds documentary or qualified range.
    INPUT,                 ///< Required wiring, assignment or logical input state unavailable.
    REFERENCE,             ///< Fresh qualified command-coordinate relation unavailable.
    RATE_ERROR_BASE = 1000 ///< Added to the checked rate-preparation detail.
};
} // namespace MotorControlCAN
