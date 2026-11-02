/** @file ActionOperation.h
 * @brief Common action phase and evidence vocabulary, independent of wire protocol.
 * SPDX-License-Identifier: MIT
 */
#pragma once
#include <MotorControlCAN/Status.h>

namespace MotorControlCAN {
/** Local concrete operation phase; OK from an advancing call is not completion. */
enum class ActionState : uint8_t {
    EMPTY,     ///< No prepared work.
    ACTIVE,    ///< Command or observation remains pending.
    SUCCEEDED, ///< The explicitly documented controller observation was acquired.
    FAILED     ///< Immutable terminal; possible effects remain retained.
};
/** Command execution evidence, separate from later observations. */
enum class ActionExecution : uint8_t {
    NOT_TRANSMITTED, ///< No possible transmission established.
    ACKNOWLEDGED,    ///< Matching protocol command success retained.
    REJECTED,        ///< Checked native rejection; no rollback of earlier work implied.
    UNKNOWN          ///< Transmission possible without confirmed command success.
};
/** Checked controller report only, never independent shaft or torque measurement. */
enum class ActionCompletion : uint8_t {
    NOT_OBSERVED, ///< No qualifying completion report.
    OBSERVED      ///< Concrete action's documented report acquired.
};
/** Terminal operation result, independent of command execution and application health. */
enum class ActionOutcome : uint8_t {
    NONE,              ///< Active or empty.
    OBSERVED,          ///< Requested narrow controller observation acquired.
    PROTOCOL_REJECTED, ///< Checked native abort retained.
    REPLY_ERROR,       ///< Malformed service or value.
    TRANSPORT_ERROR,   ///< Owner/controller failure.
    CANCELLED,         ///< Local withdrawal, never another motor command.
    DEADLINE,          ///< Immutable inclusive processing cutoff reached.
    INVALIDATED,       ///< Lifecycle or exclusive history withdrawn.
    OBSERVATION_SCOPE, ///< Mode/alarm/release report lies outside the supported observation contract.
    DRIVE_FAULT,       ///< Independently routed checked drive fault; no implicit fault clearing.
    ITERATION_EXHAUSTED ///< Nonwrapping local step identity exhausted.
};
/** Local validation errors in Status::detail; raw protocol errors are retained separately. */
enum class ActionError : int32_t {
    ARGUMENT = 1, ///< Invalid policy, capacity or selection.
    TARGET,       ///< Current binding, lifecycle or continuous reservation mismatch.
    STATE,        ///< Wrong phase or unmet prerequisite.
    TIME,         ///< Invalid supplied monotonic time or cutoff.
    CORRELATION,  ///< Terminal does not match owned work.
    OVERLAP       ///< Mutable output aliases borrowed input.
};
} // namespace MotorControlCAN
