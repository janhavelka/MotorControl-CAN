/** @file CanBusOwner.h
 * @brief Bounded cooperative application scheduling over one existing CANopen service store.
 * SPDX-License-Identifier: MIT
 * Not installed core. Resources and callbacks are stationary, borrowed for owner
 * lifetime. One context calls this owner; callbacks are bounded/nonreentrant.
 */
#pragma once
#include "SdoClient.h"

namespace MotorControlCANExample {
using MotorControlCAN::Binding;
using MotorControlCAN::Frame;
class CanBusOwner;
/** Passive communication projection only; never drive readiness or an admission policy. */
enum class DriverState : uint8_t {
    UNINIT,   ///< No current qualified endpoint evidence.
    READY,    ///< Current communication evidence, no live failed-service streak.
    DEGRADED, ///< A failed attempted service has not been followed by a successful service.
    OFFLINE   ///< Explicitly unavailable controller or expired evidenced heartbeat deadline.
};
/** Cached binding-local service history. Timestamp zero is valid when its flag is true. */
struct Health {
    uint32_t totalSuccess = 0;  ///< Saturating completed attempted logical services, not frames.
    uint32_t totalFailures = 0; ///< Saturating failed attempted logical services.
    uint8_t consecutiveFailures = 0; ///< Saturating streak; only successful service clears it.
    uint64_t lastOkUs = 0;       ///< Supplied successful-service terminal processing microseconds.
    uint64_t lastErrorUs = 0;    ///< Supplied failed-service terminal processing microseconds.
    bool lastOkKnown = false;    ///< Makes lastOkUs meaningful, including zero.
    bool lastErrorKnown = false; ///< Makes lastErrorUs meaningful, including zero.
    MotorControlCAN::Status
        lastError; ///< Last failed service; detail is SdoOutcome, full abort stays in history.
    uint64_t lastSeenUs = 0;    ///< Checked delivery us, including qualified expired PDOs.
    bool lastSeenKnown = false; ///< Makes lastSeenUs meaningful; does not qualify freshness alone.
    DriverState state =
        DriverState::UNINIT; ///< Passive projection at latest supplied health-refresh time.
};
/** Interface lifetime totals, saturating at UINT32_MAX; no duplicate service totals. */
struct Stats {
    uint32_t txFrames = 0; ///< Exact matching final reports confirming actual transmission.
    uint32_t rxFrames = 0; ///< Frames returned by the receive callback, including rejected ingress.
    uint32_t txRejected = 0; ///< Refused enqueue or negative final report, once per frame.
    uint32_t rxDropped = 0;  ///< Invalid/stale/future ingress plus explicitly reported lost frames.
    uint32_t rxLost =
        0; ///< Subset explicitly reported missing by receiveOverflow; no raw frame exists.
    uint32_t unmatchedFrames = 0; ///< Valid ingress not accepted by normal protocol dispatch.
    uint32_t queueRejected =
        0; ///< Rejected ordinary/urgent admission attempts, including invalid input.
    uint32_t resultsRejected = 0; ///< Subset of queueRejected refused for occupied result capacity.
    uint32_t cancelled = 0; ///< Winning local request withdrawals, including group/recovery/guard.
    uint32_t timeouts = 0; ///< Request or recovery timeout terminals, including local queue expiry.
    uint32_t busOffEvents =
        0;                   ///< Accepted transitions into reported BUS_OFF, not repeated reports.
    uint32_t recoveries = 0; ///< Successfully retired explicit host/controller recoveries.
    uint32_t traceOverwritten = 0; ///< Oldest diagnostic records replaced at fixed trace capacity.
    uint64_t maxServiceGapUs = 0;  ///< Largest gap between accepted service turns; not a guarantee.
};
/** Local retained-result identity; absent from the wire, invalid after owner destruction. */
struct RequestId {
    const CanBusOwner* owner = nullptr; ///< Stationary originating owner.
    uint8_t slot = 0;                   ///< Reserved request/result slot.
    uint32_t generation = 0;            ///< Nonzero nonwrapping local serial.
};
/** Exclusive logical-axis reservation across operations and gaps between requests. */
struct SequenceId {
    const CanBusOwner* owner = nullptr; ///< Stationary originating owner.
    uint8_t slot = 0;                   ///< Fixed reservation slot.
    uint32_t generation = 0; ///< Nonzero nonwrapping local serial, never wire correlation.
};
/** Fully copied ordinary service input; profile preparation/advancement stays outside owner. */
struct BusRequest {
    SequenceId sequence; ///< Live exclusive axis reservation, including its producer.
    MotorControlCAN::CANOPEN::SdoTransfer transfer; ///< Owned bytes/expectations/absolute deadline.
};
/** Local admission only; none indicates controller transmission or remote success. */
enum class BusAdmission : uint8_t {
    ACCEPTED,      ///< Complete input copied and terminal slot reserved.
    INVALID,       ///< Invalid owner/route/operation/policy/shape or overlapping output.
    EXPIRED,       ///< Overall or operation deadline already reached.
    RESULTS_FULL,  ///< Every request/result slot occupied; no unread terminal is evicted.
    IDS_EXHAUSTED, ///< Nonwrapping owner serial exhausted.
    INHIBITED      ///< Controller recovery/retirement has not established safe local resumption.
};
/** Local owner closure, independent of the native service's outcome/effect fields. */
enum class CompletionReason : uint8_t {
    SERVICE,            ///< Actual adapter terminal, including abort/timeout/uncertain effects.
    QUEUE_EXPIRED,      ///< Owner waiting consumed initial queue budget without transmission.
    DEADLINE,           ///< Immutable overall deadline expired while waiting in the owner.
    INVALIDATED,        ///< Current binding/node/session no longer matches queued input.
    ADMISSION_FAILED,   ///< Adapter refused dispatch after owner admission; nothing replayed.
    PREFLIGHT_REJECTED, ///< Application pre-handoff guard withdrew work before handoff.
    CANCELLED, ///< Explicit local cancellation; possible remote effects remain native evidence.
    RECOVERY   ///< Explicit host recovery interrupted work; not a device reset or motor stop.
};
/** Per-request cancellation disposition; no implicit group cancellation. */
enum class Cancel : uint8_t {
    CANCELLED,        ///< This call closed the request; external retirement may remain unproved.
    ALREADY_TERMINAL, ///< Earlier service/deadline/terminal won and is unchanged.
    INVALID           ///< Wrong handle, backward time or reentrant call; unchanged.
};
/** One retained host/controller recovery identity, independent of ordinary/urgent slots. */
struct RecoveryId {
    const CanBusOwner* owner = nullptr; ///< Stationary origin.
    uint32_t generation = 0;            ///< Nonzero nonwrapping serial; never a wire identity.
};
/** Bounded controller callback progress; neither completion state qualifies a remote session. */
enum class RecoveryProgress : uint8_t {
    PENDING, ///< Local queue retirement/controller preparation has not finished.
    RETIRED, ///< All older local TX/RX/reports are retired and controller can accept new work.
    FAILED   ///< Retirement/readiness cannot be proved; transmission stays inhibited.
};
/** Immutable recovery closure, separate from interrupted operation results. */
enum class RecoveryOutcome : uint8_t {
    RETIRED, ///< Host/controller boundary established only; no SDO/PDO requalification.
    FAILED,  ///< Adapter refused/failed; no safe resumption claimed.
    TIMEOUT  ///< Exclusive budget expired; a late callback cannot change this result.
};
/** Retained copied recovery evidence until releaseRecovery; no device effect is certified. */
struct RecoveryCompletion {
    RecoveryId id;                ///< Canonical identity.
    uint32_t controllerEpoch = 0; ///< Newly allocated local epoch; cannot tag remote wire replies.
    uint64_t startedUs = 0;       ///< Supplied processing time; zero valid.
    uint64_t deadlineUs = 0;      ///< Immutable exclusive absolute cutoff.
    uint64_t completedUs = 0;     ///< First terminal processing time.
    RecoveryOutcome outcome = RecoveryOutcome::FAILED; ///< No implicit success default.
};
/** Immutable first terminal, retained non-consumingly until explicit release. */
struct Completion {
    RequestId id;            ///< Canonical owner result identity.
    SequenceId sequence;     ///< Reservation/producer identity at admission.
    uint8_t producer = 0;    ///< Application producer 1..PRODUCERS.
    uint64_t admittedUs = 0; ///< Owner admission time; zero valid, queue age starts here.
    CompletionReason reason = CompletionReason::SERVICE; ///< Local closure provenance.
    bool adapterAdmitted = false;    ///< Separate from owner admission and TX staging.
    bool controllerAccepted = false; ///< At least one frame accepted by controller callback.
    MotorControlCAN::CANOPEN::SdoCompletion service; ///< Complete native evidence and owned bytes.
};
/** One owned passive diagnostic; completion is meaningful only for TERMINAL. */
enum class DiagnosticKind : uint8_t {
    RX,         ///< Raw ingress after normal dispatch or owner rejection.
    TX,         ///< One enqueue attempt, not actual transmission evidence.
    TX_REPORT,  ///< Matching final controller report; never a protocol acknowledgement.
    TERMINAL,   ///< Complete copied request terminal, including raw abort and uncertainty.
    CONTROLLER, ///< Explicit application controller report.
    RECOVERY    ///< Explicit host recovery closure.
};
/** Bounded historical copy; no pointer into controller/stack buffers is retained. */
struct Diagnostic {
    DiagnosticKind kind = DiagnosticKind::RX; ///< Selects meaningful evidence fields.
    uint64_t processedUs = 0;       ///< Supplied owner processing microseconds; zero valid.
    uint32_t controllerEpoch = 0;   ///< Local ingress/handoff epoch, never a remote wire tag.
    Frame frame;                    ///< RX/TX wire copy; RTR payload is not inspected.
    bool receivedTimeKnown = false; ///< RX only; capture was explicitly supplied.
    uint64_t receivedUs = 0; ///< RX capture microseconds, including stale/future diagnostics.
    MotorControlCAN::CANOPEN::SdoTxId txId; ///< TX/TX_REPORT exact local frame identity.
    bool accepted = false; ///< RX dispatch, TX queue acceptance or TX_REPORT actual TX, by kind.
    Completion completion; ///< TERMINAL only; survives original result release.
    MotorControlCAN::CANOPEN::ControllerState controller =
        MotorControlCAN::CANOPEN::ControllerState::UNKNOWN; ///< CONTROLLER only.
    RecoveryOutcome recovery = RecoveryOutcome::FAILED;     ///< RECOVERY only.
};
/** Cached resource usage, computed by fixed scans; no controller queries. */
struct OwnerResources {
    uint8_t queued = 0;           ///< Admitted requests not yet in the native service.
    uint8_t active = 0;           ///< Native in-flight requests, at most CHANNELS.
    uint8_t retained = 0;         ///< Unreleased request terminals, ordinary plus urgent.
    uint8_t reservations = 0;     ///< Occupied logical sequence slots.
    uint8_t traceUsed = 0;        ///< Retained diagnostic copies, at most TRACE_CAPACITY.
    bool inhibited = false;       ///< Actual controller/recovery interlock, independent of Health.
    bool recoveryPending = false; ///< Explicit recovery awaiting its terminal.
};
/** One ingress copy; processing time is supplied separately to service(). */
struct ReceivedFrame {
    Frame frame; ///< Complete owned frame, including standard/extended and data/RTR shape.
    bool receivedTimeKnown = false; ///< No invented capture timestamp when false.
    uint64_t receivedUs = 0;        ///< Qualified capture microseconds, zero valid, not future.
    uint32_t controllerEpoch = 1;   ///< Stamp at adapter ingress, before queuing; never on dequeue.
};
/** Actual controller report, never inferred from enqueue acceptance. */
struct TxReport {
    MotorControlCAN::CANOPEN::SdoTxId id; ///< Exact copied native handoff identity.
    bool transmitted = false; ///< True actual TX/ACK; false failure with uncertain effect.
    uint32_t controllerEpoch =
        1; ///< Original enqueue epoch. Both results prove no later TX of this frame.
};
/** Narrow synchronous application/controller boundary; no hidden receiver/task.
 * Callback contexts outlive the owner. Calls never reenter it or service storage.
 * receive/takeTxReport publish complete outputs only on true and retain no pointers.
 */
struct Transport {
    void* context = nullptr; ///< Borrowed stationary application/controller context.
    bool (*receive)(void*, ReceivedFrame&) = nullptr; ///< At most one frame per call; false empty.
    bool (*transmit)(void*, const Frame&, MotorControlCAN::CANOPEN::SdoTxId, uint32_t) = nullptr;
    ///< One-shot enqueue, borrowing frame only during call; true is queue acceptance only.
    ///< Final argument is controller epoch; false conservatively permits uncertain acceptance.
    bool (*takeTxReport)(void*, TxReport&) = nullptr; ///< One copied actual controller report.
    bool (*beforeTransmit)(void*, SequenceId, const MotorControlCAN::CANOPEN::SdoTransfer&,
                           uint64_t) = nullptr;
    ///< Optional first-download guard; notify typed settings before possible TX.
    void (*observe)(void*, const ReceivedFrame&, uint64_t) = nullptr;
    ///< Optional raw copy after dispatch/rejection, including malformed/future diagnostics;
    ///< not qualified evidence and cannot consume authoritative input.
    bool (*retire)(void*, MotorControlCAN::CANOPEN::SdoTxId, uint32_t) = nullptr;
    ///< Bounded synchronous attempt: true proves this exact frame cannot leave any local
    ///< SDK/controller queue later, and retires all queued/future final reports for
    ///< this exact id/epoch. Earlier effects remain possible and may be unconfirmed.
    ///< If report retirement cannot be proved, return false. False/missing blocks
    ///< that channel until a matching final TX report or explicit successful recovery.
    ///< Never sends a CAN frame or resets the drive.
    bool (*beginRecovery)(void*, uint32_t) = nullptr;
    ///< Start bounded host/controller work for the new epoch, no CAN frames/replay.
    ///< False may have partial local effects; caller stays inhibited. Stop any older
    ///< recovery attempt before starting this one; callbacks retain no borrowed pointers.
    RecoveryProgress (*pollRecovery)(void*, uint32_t) = nullptr;
    ///< One bounded poll. RETIRED proves all prior local TX/RX/report queues retired,
    ///< including asynchronous callbacks, and controller readiness. New ingress must
    ///< carry the new epoch. It proves nothing about remote pending TX or SDO sessions.
};
/** Explicit per-turn budgets; values above fixed limits clamp, zero invokes no corresponding callback. */
struct ServiceBudget {
    uint8_t rx = 0;     ///< Max receive callbacks, clamped to RX_LIMIT; false stops draining.
    uint8_t tx = 0;     ///< Max one-shot enqueues, clamped to TX_LIMIT.
    uint8_t events = 0; ///< Max report callbacks, clamped to EVENT_LIMIT; nonzero also permits
                        ///< at most CHANNELS one-shot retirement callbacks after invalidation.
    uint8_t starts = 0; ///< Max new adapter admissions, clamped to START_LIMIT; needs TX budget.
    uint8_t recovery =
        0; ///< Max one recovery poll per turn; zero does not poll, deadline still expires.
};
/** Read-only reservation facts, not a second current node/binding cache. */
struct Sequence {
    Binding binding; ///< Reserved binding; explicit successful configuration sync may advance it.
    uint32_t nodeGeneration = 0; ///< Frozen lifecycle epoch; invalidation never requalifies it.
    uint8_t producer = 0;        ///< Producer 1..PRODUCERS, selected independently of CAN node.
    uint64_t acquiredUs = 0;     ///< Reservation acquisition before prerequisite reads; zero valid.
    uint32_t operation = 0;  ///< Explicit current operation, strictly increasing per reservation.
    uint64_t deadlineUs = 0; ///< Immutable exclusive deadline for this operation.
};
/** Application-owned request/result scheduler using the sole existing adapter store.
 * The borrowed adapter contains all node/channel/PDO/event storage and is used
 * exclusively through this owner for admit/receive/TX/result progression. Configure
 * routes/observers before use or in this same cooperative context while idle.
 * No second receiver, node table, heap, clocks, automatic retry or
 * motion executor. Transport callbacks are synchronous and nonreentrant.
 */
class CanBusOwner {
  public:
    enum {
        REQUESTS = 6,        ///< Queued + active + unread terminal capacity, shared ordinary slots.
        URGENT_REQUESTS = 1, ///< Reserved urgent request/result slot, never used by ordinary work.
        TOTAL_REQUESTS = REQUESTS + URGENT_REQUESTS, ///< All copied work/result storage.
        SEQUENCES = 2,     ///< Logical-axis reservations, independent of wire node numbering.
        PRODUCERS = 4,     ///< Cyclic eligible-producer fairness, FIFO within each sequence.
        RX_LIMIT = 8,      ///< Maximum receive callback calls per service turn.
        TX_LIMIT = 2,      ///< Maximum one-shot enqueue callbacks per turn.
        EVENT_LIMIT = 8,   ///< Maximum TX report callback calls per turn.
        START_LIMIT = 2,   ///< Maximum new native service admissions per turn.
        TRACE_CAPACITY = 8 ///< Oldest-first diagnostic copies; full storage overwrites oldest.
    };
    /** Borrow stationary exclusive resources; construction performs no I/O.
     * @param[in] services Sole existing adapter, already configured or configured while idle.
     * @param[in] transport Copied callback bundle; context remains borrowed for owner lifetime.
     * @param[in] identity Nonzero application owner identity, suitable for settings reservations.
     * @param[in] lastIssued Last local serial; never wraps. Nonzero may preserve an application
     * serial domain, but never establishes wire freshness or revives prior handles.
     * @param[in] controllerEpoch Nonzero local transport epoch; initial ingress uses it.
     * UINT32_MAX allows current work but rejects any recovery needing a new epoch.
     */
    CanBusOwner(MotorControlCAN::CANOPEN::SdoClient& services, const Transport& transport,
                uint32_t identity, uint32_t lastIssued = 0, uint32_t controllerEpoch = 1) noexcept;
    CanBusOwner(const CanBusOwner&) = delete;            ///< Stationary handles cannot be copied.
    CanBusOwner& operator=(const CanBusOwner&) = delete; ///< Cannot move live ownership.
    /** Reserve one actual logical binding, without a service or device action.
     * @param[in] binding Exact configured current channel binding; no node zero.
     * @param[in] producer Application identity 1..PRODUCERS.
     * @param[in] nowUs Nondecreasing supplied monotonic microseconds, zero valid.
     * @param[out] output Changed on success only; no overlap with owner or binding.
     * @return False atomically for stale/duplicate binding, capacity, time or exhausted IDs.
     */
    bool beginSequence(const Binding& binding, uint8_t producer, uint64_t nowUs,
                       SequenceId& output) noexcept;
    /** Start the next operation while retaining axis exclusion across prerequisite/settings reads.
     * @param[in] id Exact current non-invalidated reservation with no nonterminal requests.
     * Old retained terminals do not prevent a new operation and remain unchanged.
     * @param[in] operation Strictly greater nonzero operation identity than its predecessor.
     * @param[in] deadlineUs Exclusive immutable absolute cutoff, strictly after nowUs.
     * @param[in] nowUs Supplied nondecreasing microseconds.
     * @return False unchanged on invalid input; step sequence restarts only on success.
     */
    bool beginOperation(SequenceId id, uint32_t operation, uint64_t deadlineUs,
                        uint64_t nowUs) noexcept;
    /** Borrow current reservation facts without consuming them.
     * @param[in] id Exact current reservation.
     * @return Null if stale, foreign, invalidated or underlying route is unresolved/changed.
     */
    const Sequence* sequence(SequenceId id) const noexcept;
    /** Release axis exclusion only when all its request/results have been released.
     * @param[in] id Exact reserved identity; invalidated reservations may be released.
     * @return False unchanged for wrong ID, live work or unread terminals.
     */
    bool releaseSequence(SequenceId id) noexcept;
    /** Copy one yielded native request and reserve its immutable terminal slot.
     * @param[in] request Complete input; matching operation/deadline and increasing step.
     * @param[in] nowUs Same nondecreasing clock; queue/overall age begins here.
     * @param[out] output Unchanged on rejection; no overlap with owner or request.
     * @return ACCEPTED is local only. Rejection changes neither queue nor reservation.
     */
    BusAdmission admit(const BusRequest& request, uint64_t nowUs, RequestId& output) noexcept;
    /** Copy work into reserved urgent capacity; never preempts or stops existing work.
     * @param[in] request Same validation as admit; reuse the axis's live reservation.
     * FIFO within that reservation still applies. Explicitly cancel obsolete work
     * before beginning a new urgent operation; quarantined routes remain unavailable.
     * @param[in] nowUs Nondecreasing processing microseconds.
     * @param[out] output Published only on acceptance, with admit's nonaliasing rules.
     * @return Local admission only; ordinary pressure cannot consume this slot.
     */
    BusAdmission admitUrgent(const BusRequest& request, uint64_t nowUs, RequestId& output) noexcept;
    /** Close one request without progressing unsent work or reading controller input.
     * @param[in] id Exact request handle; other queued requests require explicit cancellation.
     * @param[in] nowUs Same clock; already reached native/owner deadlines win first.
     * @return First closure disposition. Up to CHANNELS bounded retire callbacks may
     * settle invalid external handoffs. Failure blocks local reuse, not observations.
     */
    Cancel cancel(RequestId id, uint64_t nowUs) noexcept;
    /** Revoke a reservation and cancel all its pending work, retaining every terminal.
     * @param[in] id Exact reservation; no fresh channel/session is established.
     * @param[in] nowUs Nondecreasing microseconds; no receive or new TX is performed.
     * @return False unchanged for invalid/time/reentrancy; true local invalidation only.
     */
    bool invalidate(SequenceId id, uint64_t nowUs) noexcept;
    /** Revoke all reservations of one application producer; no device-wide reset.
     * @param[in] producer Explicit producer 1..PRODUCERS, independent of CAN node ID.
     * @param[in] nowUs Nondecreasing microseconds; bounded by fixed requests/channels.
     * @return False unchanged for invalid input; old results remain inspectable.
     */
    bool invalidateProducer(uint8_t producer, uint64_t nowUs) noexcept;
    /** Begin explicit host/controller recovery, with independently retained result capacity.
     * @param[in] nowUs Nondecreasing microseconds; no hidden time acquisition.
     * @param[in] deadlineUs Exclusive absolute cutoff, strictly greater than nowUs.
     * @param[out] output Published on acceptance only; may not overlap owner/adapter.
     * @return True reserves a result, invalidates all reservations/node freshness and
     * invokes at most one beginRecovery callback. Missing/failed callback closes FAILED.
     * No wire command, SDO reopening, PDO qualification, replay or physical stop occurs.
     */
    bool recover(uint64_t nowUs, uint64_t deadlineUs, RecoveryId& output) noexcept;
    /** @param[in] id Exact recovery identity.
     * @return Immutable terminal view until release; null pending/stale/foreign.
     */
    const RecoveryCompletion* recoveryResult(RecoveryId id) const noexcept;
    /** @param[in] id Exact terminal recovery identity.
     * @return True releases only retained storage; a failure/timeout interlock survives.
     */
    bool releaseRecovery(RecoveryId id) noexcept;
    /** Report bus-off, recovery or RX loss through the same serialized owner.
     * @param[in] state Actual controller state, never inferred from a missing reply.
     * @param[in] nowUs Nondecreasing supplied time. Non-AVAILABLE invalidates work;
     * direct AVAILABLE cannot bypass an owner retirement/recovery interlock.
     * @return False for invalid input. Bounded retirement callbacks may run;
     * no CAN command, implicit reset or replay occurs.
     */
    bool reportController(MotorControlCAN::CANOPEN::ControllerState state, uint64_t nowUs) noexcept;
    /** Report a dropped authoritative RX frame: revoke dependent trust and inhibit TX.
     * @param[in] nowUs Nondecreasing processing microseconds.
     * @param[in] droppedFrames Nonzero reported minimum number lost; saturating addition.
     * @return Same as reportController(RECOVERING); recovery requires explicit request.
     */
    bool receiveOverflow(uint64_t nowUs, uint32_t droppedFrames = 1) noexcept;
    /** @return Current local transport epoch, never proof of remote freshness. */
    uint32_t controllerEpoch() const noexcept { return controllerEpoch_; }
    /** @return Latest accepted supplied monotonic microseconds, initially zero.
     * Cached only, never a clock read. Includes explicit application calls between
     * service turns; use this authority when a platform clock itself fails.
     */
    uint64_t lastTimeUs() const noexcept { return lastUs_; }
    /** Run one bounded cooperative turn; deadlines progress even with all budgets zero.
     * @param[in] nowUs Nondecreasing nonwrapping caller microseconds, zero valid.
     * @param[in] budget Clamped RX/TX/report/start/recovery budgets; zero prevents that work.
     * @return False without mutation for backward time/reentrancy/overlap. Accepted
     * frames enter normal adapter routing exactly once before observer copies.
     */
    bool service(uint64_t nowUs, const ServiceBudget& budget) noexcept;
    /** Inspect authoritative local TX retirement for one configured adapter slot.
     * @param[in] slot Existing fixed SDO channel slot, independent of wire node ID.
     * @return True only when native software staging is empty and the owner has
     * no external handoff awaiting a final report or exact retirement proof.
     * Invalid/unconfigured slots return false. Queued owner requests may still
     * exist: this is not an idle check or permission to replay/requalify work.
     * Cached serialized inspection only; no I/O, clock, callback or mutation.
     */
    bool localTxRetired(uint8_t slot) const noexcept;
    /** Non-consuming canonical result view.
     * @param[in] id Exact admitted request identity.
     * @return Immutable pointer until release/destruction; null pending/stale/foreign.
     */
    const Completion* result(RequestId id) const noexcept;
    /** Release one terminal slot only; never clears channel uncertainty or axis exclusion.
     * @param[in] id Exact terminal identity.
     * @return False unchanged for pending/stale/foreign IDs or callback reentrancy.
     */
    bool release(RequestId id) noexcept;
    /** Synchronize a completed exclusive settings transaction's configuration epoch.
     * @param[in] id Live reservation; every owner request on this node must be released,
     * and every external handoff on the node must have proved local retirement.
     * @param[in] current Same binding with exactly next configuration epoch, already
     * withdrawn by the application's pre-TX settings notification.
     * @param[in] nowUs Same monotonic caller time.
     * @return Adapter's checked idle advance only; preserves frozen settings token,
     * node history and quarantine. No device action, reference renewal or recovery.
     */
    bool advanceConfiguration(SequenceId id, const Binding& current, uint64_t nowUs) noexcept;
    /** Copy cached communication health at the most recent supplied owner time.
     * @param[in] binding Exact configured current binding, including configuration epoch.
     * @param[out] output Owned copy, unchanged for unknown/stale binding or input overlap.
     * @return True for a known binding, including UNINIT before any service turn.
     * No I/O, clock, callback or statistics update occurs. Call service to age evidence.
     */
    bool health(const Binding& binding, Health& output) const noexcept;
    /** Copy the cached communication projection; no alias named state().
     * @param[in] binding Same exact binding contract as health().
     * @param[out] output Unchanged on rejection; no overlap with owner/adapter/binding.
     * @return True only for a known binding; no I/O or clock access.
     */
    bool driverState(const Binding& binding, DriverState& output) const noexcept;
    /** @return Borrowed cached interface totals until mutation/destruction; copy to retain. */
    const Stats& stats() const noexcept { return stats_; }
    /** Clear interface totals/max gap and binding totalSuccess/totalFailures only.
     * @return False unchanged during callback reentrancy; otherwise true. Preserves
     * failure streak/error/times/state, history, active work, generations and interlocks.
     * The previous service timestamp survives, so the next observed gap remains honest.
     */
    bool clearStats() noexcept;
    /** Configure only a host heartbeat-age policy; never writes 1017 or obtains time.
     * @param[in] binding Exact configured binding.
     * @param[in] maximumAgeUs Inclusive age cap; zero disables this host monitor.
     * Only an effective known nonzero producer and observed qualified heartbeat
     * can expire; no startup timer or interval multiplier is invented.
     * @param[in] nowUs Supplied nondecreasing microseconds; zero valid.
     * @return False unchanged for unknown binding/time/reentrancy; otherwise refreshes
     * cached projection only. It never affects service admission.
     */
    bool setHeartbeatMaximumAge(const Binding& binding, uint64_t maximumAgeUs,
                                uint64_t nowUs) noexcept;
    /** Copy and consume one diagnostic history record, never authoritative RX.
     * @param[out] output Owned copy on success, unchanged if empty/overlapping storage.
     * @return True for one oldest record; bounded no-I/O call. A slow consumer merely
     * loses oldest copies, counted in traceOverwritten; normal routing never waits.
     */
    bool takeDiagnostic(Diagnostic& output) noexcept;
    /** @return Fixed-scan resource copy; no I/O, clocks, callbacks or mutable view. */
    OwnerResources resources() const noexcept;
    /** @return Explicit application owner identity, not a wire token. */
    uint32_t identity() const noexcept { return identity_; }

  private:
    struct Reservation {
        bool occupied = false, invalidated = false;
        uint32_t generation = 0, lastStep = 0;
        Sequence value;
    } sequences_[SEQUENCES];
    struct Slot {
        bool occupied = false, terminal = false, submitted = false, guarded = false;
        bool localCancellation = false;
        uint32_t generation = 0;
        uint64_t queueDeadlineUs = 0;
        BusRequest request;
        MotorControlCAN::CANOPEN::SdoRequestId native;
        Completion completion;
    } slots_[TOTAL_REQUESTS];
    struct ExternalTx {
        bool pending = false, blocked = false, retirementTried = false, rejected = false;
        uint32_t epoch = 0;
        MotorControlCAN::CANOPEN::SdoTxId id;
    } external_[MotorControlCAN::CANOPEN::SdoClient::CHANNELS];
    MotorControlCAN::CANOPEN::SdoClient& services_;
    Transport transport_;
    uint32_t identity_, lastId_;
    uint32_t controllerEpoch_;
    RecoveryCompletion recovery_;
    bool recoveryOccupied_ = false, recoveryTerminal_ = false, inhibited_ = false;
    uint64_t lastUs_ = 0;
    uint8_t nextProducer_ = 1, nextChannel_ = 0;
    int active_[MotorControlCAN::CANOPEN::SdoClient::CHANNELS] = {-1, -1};
    bool entered_ = false;
    struct HealthRecord {
        Binding binding;
        Health value;
        uint64_t heartbeatMaximumAgeUs = 0;
        bool bound = false;
    } health_[MotorControlCAN::CANOPEN::SdoClient::CHANNELS];
    Stats stats_;
    Diagnostic trace_[TRACE_CAPACITY];
    uint8_t traceHead_ = 0, traceCount_ = 0;
    bool serviceTimeKnown_ = false;
    uint64_t serviceTimeUs_ = 0;
    bool valid(SequenceId id) const noexcept;
    bool valid(RequestId id) const noexcept;
    int channel(const Binding& binding) const noexcept;
    int channel(const MotorControlCAN::CANOPEN::SdoChannel& route) const noexcept;
    bool occupied(SequenceId id) const noexcept;
    bool live(SequenceId id) const noexcept;
    BusAdmission admit(const BusRequest&, uint64_t, RequestId&, bool urgent) noexcept;
    int select(uint64_t nowUs) const noexcept;
    bool eligible(unsigned slot, uint64_t nowUs) const noexcept;
    void collect() noexcept;
    void expire(uint64_t nowUs) noexcept;
    void finish(unsigned slot, CompletionReason reason, uint64_t nowUs) noexcept;
    bool transmit(uint8_t channel, uint64_t nowUs) noexcept;
    void cancelSlot(unsigned slot, CompletionReason reason, uint64_t nowUs) noexcept;
    void settleExternal(uint64_t nowUs, bool allowCallback) noexcept;
    void closeRecovery(RecoveryOutcome outcome, uint64_t nowUs) noexcept;
    void refreshHealth(uint64_t nowUs) noexcept;
    void recordTerminal(const Slot& slot) noexcept;
    void trace(const Diagnostic& record) noexcept;
    int healthSlot(const Binding& binding) const noexcept;
};
} // namespace MotorControlCANExample
