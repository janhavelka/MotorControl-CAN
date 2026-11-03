/** @file MoveOperation.h
 * @brief Common finite-move intent and outcome distinctions, independent of wire protocol.
 * SPDX-License-Identifier: MIT
 * A profile owns its native units, trigger, acceptance and completion evidence. Nothing here
 * establishes a wire behavior, a status dialect, a relative basis or physical movement, and the
 * shared ActionState, ActionExecution, ActionCompletion and ActionOutcome keep their meanings.
 */
#pragma once
#include <MotorControlCAN/ActionOperation.h>
#include <MotorControlCAN/Axis.h>

namespace MotorControlCAN {
/** Common finite-move intent. A profile prepares each part once and refuses what it cannot qualify.
 * For a relative move, position.basis names the host witness the displacement is added to. That is
 * host arithmetic only: whether the drive adds the displacement to the same base is a separate
 * profile fact, and a profile without evidence of it refuses the request. */
struct MoveRequest {
    PositionRequest position; ///< Target intent, prepared with preparePosition.
    /** Profile speed, prepared with prepareVelocityTarget. It must be positive. The profile checks
     * that the prepared destination is the native object's own unit before anything is written. */
    VelocityRequest speed;
};
/** What a prepared move knows about its own displacement. */
enum class MoveDisplacement : uint8_t {
    /** Absolute target with no witness: no current position, distance or standstill is claimed, and
     * an absolute coordinate zero is not a zero displacement. */
    UNKNOWN,
    /** Nonzero endpoint minus a supplied idle, stationary witness, in command increments. A relative
     * move is always KNOWN: its displacement is the quantized request, and its endpoint is host
     * arithmetic on the witness, never an observed position. */
    KNOWN
};
/** Why the host ended a move before the move's own terminal. Never a motor command by itself, and
 * never a resume: a withdrawn move is not replayed. */
enum class MoveInterruption : uint8_t {
    NONE,        ///< Not interrupted.
    CANCELLED,   ///< Local cancel. Nothing is sent, so the drive may still be moving.
    STOP,        ///< An explicit stop was requested. The stop is its own operation and evidence.
    DRIVE_FAULT, ///< Independently routed checked drive fault. Nothing is cleared.
    INVALIDATED, ///< Route, reservation, node, configuration or owner lifecycle withdrawn.
    DEADLINE     ///< Immutable processing cutoff reached: `now >= deadlineUs`.
};
/** Local validation details in Status::detail for move calls. Protocol terminals stay in their
 * retained evidence and are never narrowed into these. */
enum class MoveError : int32_t {
    NONE,              ///< No error.
    ARGUMENT,          ///< Invalid policy, budget, selection or capacity.
    TARGET,            ///< Reservation, route or epoch differs from the frozen move.
    STATE,             ///< Wrong phase or unmet drive-state prerequisite.
    TIME,              ///< Invalid supplied time, or the overall cutoff was reached.
    CORRELATION,       ///< Terminal or observation does not match the move's own work.
    OVERLAP,           ///< Mutable output aliases a borrowed input.
    CAPABILITY,        ///< Closed gate, unmeasured firmware or an unsupported feature.
    RATE_BASIS,        ///< Prepared speed is not in the native object's unit.
    RANGE,             ///< Requested or effective native value outside the admitted range.
    ZERO_DISPLACEMENT, ///< Known effective displacement is zero; nothing is written.
    /** Supplied witness is not idle and stationary, is of the wrong basis, has no age cap where one
     * is needed, or was observed before the predecessor it must follow. */
    REFERENCE,
    ACCEPTANCE,        ///< The acceptance report is already present before the trigger.
    /** The drive's own relative base is not evidenced for the requested basis: unmeasured, a
     * different base, or a queued basis that needs a target queue. Returned with UNSUPPORTED. */
    RELATIVE_BASIS,
    PARAMETER_ERROR_BASE = 1000, ///< Added to a profile parameter-preparation detail.
    RATE_ERROR_BASE = 5000       ///< Added to a speed-preparation AxisError detail.
};
} // namespace MotorControlCAN
