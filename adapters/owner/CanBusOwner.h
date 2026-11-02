/** @file CanBusOwner.h
 * @brief Bounded cooperative application scheduling over one existing CANopen service store.
 * SPDX-License-Identifier: MIT
 * Not installed core. Resources and callbacks are stationary, borrowed for owner
 * lifetime. One context calls this owner; callbacks are bounded/nonreentrant.
 */
#pragma once
#include "CanBusTypes.h"

namespace MotorControlCANOwner {
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
    /** Admit one copied explicit NMT intent into the sole native manager slot.
     * @param work Command, physical bus/address or explicit broadcast, finite budgets.
     * @param nowUs Nondecreasing supplied microseconds.
     * @param output Native retained handle; changed only on success, no overlap.
     * @return False for occupied native result/unretired previous handoff, invalid
     * target, expired budget or inhibited transport. No TX occurs during admission.
     * NMT is independent of axis reservations; TX never confirms remote state.
     */
    bool admitNmt(const MotorControlCAN::CANOPEN::NmtTransfer& work, uint64_t nowUs,
                  MotorControlCAN::CANOPEN::NmtRequestId& output) noexcept;
    /** Withdraw one explicit NMT intent, preserving possible earlier effects.
     * @param id Exact retained native admission.
     * @param nowUs Nondecreasing processing microseconds; deadline wins at equality.
     * @return True only when cancellation wins. Exact retirement may remain pending.
     * Sends no motor stop, network command or replay; may attempt bounded local retirement.
     */
    bool cancelNmt(MotorControlCAN::CANOPEN::NmtRequestId id, uint64_t nowUs) noexcept;
    /** @param id Exact owner-admitted native identity.
     * @return Immutable terminal until releaseNmt/destruction, null pending/stale/foreign.
     */
    const MotorControlCAN::CANOPEN::NmtCompletion*
        nmtResult(MotorControlCAN::CANOPEN::NmtRequestId id) const noexcept;
    /** @param id Exact terminal identity.
     * @return True releases result only; unretired transport still blocks NMT reuse.
     */
    bool releaseNmt(MotorControlCAN::CANOPEN::NmtRequestId id) noexcept;
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
        TransportTxId id;
    } external_[MotorControlCAN::CANOPEN::SdoClient::CHANNELS], nmtExternal_;
    MotorControlCAN::CANOPEN::NmtRequestId nmtId_;
    bool nmtOccupied_ = false;
    MotorControlCAN::CANOPEN::NmtTransfer nmtTransfer_;
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
    bool peersResolved(const Binding& binding) const noexcept;
    BusAdmission countAdmission(BusAdmission result) noexcept;
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
    bool transmitNmt(uint64_t nowUs) noexcept;
    bool canTransmit(TransportTxId id, uint64_t nowUs) const noexcept;
    bool nmtBlocks(const Binding& binding) const noexcept;
    void settle(ExternalTx& tx, uint64_t nowUs, bool allowCallback) noexcept;
    void cancelSlot(unsigned slot, CompletionReason reason, uint64_t nowUs) noexcept;
    void settleExternal(uint64_t nowUs, bool allowCallback) noexcept;
    void closeRecovery(RecoveryOutcome outcome, uint64_t nowUs) noexcept;
    void refreshHealth(uint64_t nowUs) noexcept;
    void recordTerminal(const Slot& slot) noexcept;
    void trace(const Diagnostic& record) noexcept;
    int healthSlot(const Binding& binding) const noexcept;
};
} // namespace MotorControlCANOwner
