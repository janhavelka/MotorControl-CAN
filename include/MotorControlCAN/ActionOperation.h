/** @file ActionOperation.h
 * @brief Common action phase and evidence vocabulary, independent of wire protocol.
 * SPDX-License-Identifier: MIT
 */
#pragma once
#include <MotorControlCAN/Status.h>

namespace MotorControlCAN {
/** Explicit intent; a device counter clear is distinct from host origin or home. */
enum class ActionKind : uint8_t {
    STOP,               ///< Profile-selected explicit stop.
    CLEAR_ALARM,        ///< Documented fault-clear attempt, with fresh observations.
    SET_DEVICE_POSITION, ///< Only a profile's evidenced native counter operation.
    ENABLE               ///< Profile's explicit enable sequence, gated while its effect is unmeasured.
};
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
/** Native command-response capability, independent of controller CAN acknowledgement. */
enum class AcknowledgementSupport : uint8_t {
    UNKNOWN, ///< Selected protocol's response contract is not established.
    NONE, ///< Reviewed setter has no native acknowledgement.
    RESPONSE ///< A correlated native acceptance/rejection response exists.
};
/** Caller-qualified facts for one command, borrowed only during checkServiceEvidence.
 * These facts do not establish binding, permission, correlation or sample provenance;
 * the concrete service qualifies those before supplying them. No wire work is performed.
 */
struct ServiceEvidence {
    bool admitted = false; ///< Local owner accepted the command; no TX claim.
    bool transmissionPossible = false; ///< External transmission can no longer be excluded.
    bool transmissionConfirmed = false; ///< Controller identifies actual transmission.
    AcknowledgementSupport acknowledgement = AcknowledgementSupport::UNKNOWN; ///< Explicit native contract.
    bool acknowledged = false; ///< Checked matching native acceptance, never CAN ACK alone.
    bool rejected = false; ///< Checked matching native rejection; no rollback implied.
    bool completionObserved = false; ///< Qualified native completion report, not necessarily standstill.
};
/** Separate command execution and native completion projections; no physical qualification. */
struct ServiceOutcome {
    ActionExecution execution = ActionExecution::NOT_TRANSMITTED; ///< Native acceptance evidence.
    ActionCompletion completion = ActionCompletion::NOT_OBSERVED; ///< Native completion evidence.
};
/** Check consistent supplied facts and atomically project their narrow meaning.
 * @param evidence Caller-qualified borrowed facts; no pointers are retained.
 * @param output Unchanged on failure; must not overlap evidence.
 * @return INVALID_CONFIG for contradictory facts/overlap, otherwise OK. Admission or
 * confirmed TX without native response leaves execution UNKNOWN once TX is possible.
 * A no-ack setter does not acquire synthetic confirmation. completionObserved means
 * only the selected native report: trajectories may complete at nonzero velocity.
 */
Status checkServiceEvidence(const ServiceEvidence& evidence, ServiceOutcome& output) noexcept;
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
    OBSERVATION_SCOPE, ///< Mode/alarm/release/target report lies outside the supported observation contract.
    DRIVE_FAULT,       ///< Independently routed checked drive fault; no implicit fault clearing.
    ITERATION_EXHAUSTED, ///< Nonwrapping local step identity exhausted.
    STOP_SENT_UNCONFIRMED, ///< Best-effort stop left the controller; no reply can confirm it or any effect.
    SERVICE_TIMEOUT ///< Queue or response budget spent before the overall cutoff. Execution and possible transmission stay separate.
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
