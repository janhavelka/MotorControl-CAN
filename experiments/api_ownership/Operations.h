/** @file Operations.h
 * @brief Bounded synthetic operations proving prompt-6 ownership, not a drive.
 * SPDX-License-Identifier: MIT
 */
#pragma once
#include "Contracts.h"

namespace MotorControlCAN {
/** Synthetic single-step profile for the host ownership proof only. */
namespace ProofProfile {

/** Synthetic objects selected only for the local fake peer; no vendor meaning. */
constexpr uint16_t IDENTITY_OBJECT = 0x2000;
/** Synthetic target write, not Lichuan/CiA402 staging, trigger or motion. */
constexpr uint16_t TARGET_OBJECT = 0x2001;

/** This proof has exactly one typed read kind; no generic result accessor. */
enum class ReadKind : uint8_t {
    IDENTITY ///< Four-byte synthetic marker read; not vendor identification.
};

/** Copied typed synthetic identity with its route and qualification time.
 * value is the fake peer's U32 marker, not vendor/product/firmware identity. */
struct Identity {
    uint32_t value = 0; ///< Copied fake peer marker; no model/firmware interpretation.
    Binding binding; ///< Original complete route and generation provenance.
    uint64_t observedUs = 0; ///< Supplied owner service microseconds, not drive sample time.
};

/** Caller-owned single-step read context. Public fields are inspection-only
 * between calls. Terminal evidence survives next/getter calls and owner release;
 * only explicit successful preparation replaces it. Mutation is serialized by
 * the caller; independent contexts require no shared core storage. */
struct ReadContext {
    Binding binding; ///< Frozen original identity, route and interpretation generations.
    OperationToken operation; ///< Host operation identity assigned at preparation.
    StepToken step; ///< Current synthetic step identity, one after preparation.
    ReadKind kind = ReadKind::IDENTITY; ///< Typed read selected by preparation.
    ReadState state = ReadState::EMPTY; ///< Concrete sequence phase; default unprepared.
    ReadOutcome outcome = ReadOutcome::NONE; ///< Retained operation outcome, distinct from call status.
    Status status; ///< Retained operation/failing-step status; default local success.
    uint64_t startedUs = 0; ///< Supplied preparation microseconds; zero is valid.
    uint64_t deadlineUs = 0; ///< Fixed exclusive supplied-microsecond deadline.
    uint64_t servicedUs = 0; ///< Last successfully consumed owner event/preparation time.
    ServiceEvidence evidence; ///< Last copied terminal service evidence, including failures.
    Identity identity; ///< Decoded marker, published only through checked getIdentity.
};

/** Caller-owned single-step absolute target prototype. request/configuration
 * are copied at preparation; nativeTarget is exact signed I32 command data.
 * ACK proves only the synthetic setter confirmation. Completion remains
 * NOT_OBSERVED, even on SUCCEEDED; no actual drive sequence is implemented.
 * uncertain records a potentially applied write after possible TX without
 * successful confirmation; even a checked abort need not exclude partial effects.
 * Terminal evidence persists until successful explicit preparation replaces it.
 * Concurrent access/mutation to the same context requires caller serialization. */
struct MoveContext {
    Binding binding; ///< Frozen original axis identity/route/configuration generations.
    OperationToken operation; ///< Local operation identity copied at preparation.
    StepToken step; ///< Current synthetic step identity, one after preparation.
    PositionRequest request; ///< Copied original exact requested coordinate/quantization intent.
    AxisConfig configuration; ///< Copied immutable scales, origin, limits and capability facts.
    int32_t nativeTarget = 0; ///< Exact converted target in native command increments.
    ActionState state = ActionState::EMPTY; ///< Concrete single-step operation phase.
    ActionOutcome outcome = ActionOutcome::NONE; ///< Retained operation outcome.
    Status status; ///< Retained operation/failing-step status, independent of advance return.
    ActionExecution execution = ActionExecution::NOT_TRANSMITTED; ///< Checked service execution evidence.
    ActionCompletion completion = ActionCompletion::NOT_OBSERVED; ///< No proof path observes motion completion.
    bool uncertain = false; ///< Possible write effects remain after possible TX without service success.
    uint64_t startedUs = 0; ///< Supplied preparation microseconds; zero is valid.
    uint64_t deadlineUs = 0; ///< Immutable exclusive deadline in supplied monotonic microseconds.
    uint64_t servicedUs = 0; ///< Last successfully consumed event/preparation time.
    ServiceEvidence evidence; ///< Copied terminal service bytes, abort and TX evidence.
};

/** Prepare one synthetic U32 identity upload; copies binding without I/O.
 * @param[in] binding Valid CANopen binding; addressed route checks belong to its owner.
 * @param[in] operation Nonzero local token, unique while prior work can remain.
 * @param[in] nowUs Supplied monotonic microseconds; zero is valid.
 * @param[in] deadlineUs Exclusive absolute deadline, strictly greater than nowUs.
 * @param[out] output Caller context, atomically replaced only on success.
 * @return OK for a valid prepared context, not a completed read; invalid binding,
 * token or deadline returns INVALID_CONFIG with ReadError detail; a proprietary
 * binding returns UNSUPPORTED before yielding CANopen service work. */
Status prepareIdentity(const Binding& binding, OperationToken operation,
                       uint64_t nowUs, uint64_t deadlineUs, ReadContext& output) noexcept;

/** Yield copied current work without advancing or admitting it.
 * @param[in] context Prepared/terminal context; EMPTY is invalid.
 * @param[in] nowUs Supplied monotonic time, >= last service and < deadline if active.
 * @param[out] output Owned request; changed only when ready is true on success.
 * @param[out] ready True for ACTIVE work; false for terminal, with output unchanged.
 * @return OK for valid work/terminal view; sequencing/time errors leave both
 * outputs unchanged. Repeated calls yield the same token; owner prevents replay.
 * This function performs no I/O, allocation, clock access or retries. */
Status nextRead(const ReadContext& context, uint64_t nowUs,
                ServiceWork& output, bool& ready) noexcept;

/** Apply one owner-qualified terminal event, copying its bytes.
 * @param[in,out] context ACTIVE context, atomically updated only for a valid event.
 * @param[in] evidence Matching binding/tokens. observedUs is the supplied current
 * owner service time, >= startedUs/servicedUs; there is no independent clock.
 * ACK/abort must precede the exclusive deadline. DEADLINE must
 * be at/after it. Input is borrowed only during this call.
 * @return OK means valid advancement, including failed terminal operation;
 * invalid correlation/shape/time leaves context unchanged. Protocol abort bits
 * remain in evidence.abortCode, never narrowed into Status::detail. */
Status advanceRead(ReadContext& context, const ServiceEvidence& evidence) noexcept;

/** Publish a complete correctly typed identity, preserving output on failure.
 * @param[in] context Retained successful identity context.
 * @param[in] currentBinding Current complete route/generation provenance.
 * @param[out] output Caller-owned result; no context pointer is retained.
 * @return OK only for SUCCEEDED/IDENTITY/current provenance; incomplete, wrong
 * kind or stale results return INVALID_CONFIG with ReadError detail. No I/O. */
Status getIdentity(const ReadContext& context, const Binding& currentBinding,
                   Identity& output) noexcept;

/** Prepare a synthetic absolute target write using production numeric primitives.
 * Only exact native I32 targets or motor/load TURNS/DEGREES are supported.
 * Uses production Units for reduction, cross-cancelled conversion and EXACT
 * integer quantization, followed by checked origin addition and native narrowing.
 * Full-step geometry/feedback are optional; malformed supplied facts still reject.
 * @param[in] request Exact target, matching generation, EXACT rounding and zero
 * quantization allowance. MOTOR/LOAD targets require explicit known origin.
 * @param[in] configuration Valid CANopen binding, position capability and any required
 * calibration qualification, selected command/gear scales and inclusive limits.
 * @param[in] operation Nonzero host token; does not correlate SDO on the wire.
 * @param[in] nowUs Supplied monotonic microseconds; zero is valid.
 * @param[in] deadlineUs Immutable exclusive absolute deadline, > nowUs.
 * @param[out] output Caller-owned copied context, unchanged on every rejection.
 * @return OK prepares only; unsupported capability/conversion is rejected before
 * work exists. Proprietary bindings reject before any SDO work is created.
 * Numeric detail is MoveError::UNIT_ERROR_BASE + UnitError; other detail uses MoveError.
 * No enabling, trigger, motor I/O, allocation or retry occurs. */
Status prepareMoveAbsolute(const PositionRequest& request, const AxisConfig& configuration,
                           OperationToken operation, uint64_t nowUs, uint64_t deadlineUs,
                           MoveContext& output) noexcept;

/** Yield the same owned target work/token until a valid terminal event.
 * @param[in] context Prepared/terminal context; EMPTY is invalid.
 * @param[in] nowUs Monotonic supplied time; active calls require nowUs < deadline.
 * @param[out] output Complete copied work, unchanged unless ready becomes true.
 * @param[out] ready True for ACTIVE, false for terminal; unchanged on error.
 * @return Local-call OK is no acknowledgement/completion claim. Time/state
 * errors preserve both outputs. No I/O, hidden time or automatic replay. */
Status nextMove(const MoveContext& context, uint64_t nowUs,
                ServiceWork& output, bool& ready) noexcept;

/** Advance the synthetic write from copied owner-qualified service evidence.
 * @param[in,out] context ACTIVE context; invalid calls leave all fields unchanged.
 * @param[in] evidence Matching binding/tokens; observedUs is supplied current
 * owner service time, >= startedUs/servicedUs, with no independent clock. ACK/abort must
 * precede deadline; DEADLINE must be at/after it. Borrowed for this call only.
 * @return OK means a valid terminal transition, including errors/cancellation.
 * ACK yields execution ACKNOWLEDGED and completion NOT_OBSERVED. Without a
 * checked ACK/abort, possible TX yields UNKNOWN/uncertain; a checked abort
 * yields REJECTED while retaining possible partial effects in uncertain.
 * Neither path retries or physically stops. No clock access, I/O, allocation
 * or shared mutable storage. */
Status advanceMove(MoveContext& context, const ServiceEvidence& evidence) noexcept;

} } // namespace MotorControlCAN::ProofProfile
