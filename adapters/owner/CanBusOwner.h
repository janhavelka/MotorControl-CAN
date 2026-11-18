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
 * Self-repair primitives (prompt 55): a response timeout after possible TX stages one
 * cleanup abort; a quarantine with possible TX (timeout, cancellation, bus-off, or a
 * controller event before a retired recovery) arms the binding's SdoReusePolicy for one
 * resync step (by default one drain marker the owner issues itself), never after a
 * best-effort stop; a handoff without a final report within the TX lifetime enters
 * NO_LISTENER. None of them resends work, restarts the controller or extends
 * a deadline, and stops keep priority over every repair step.
 */
class CanBusOwner {
  public:
    /** Storage counts and per-turn limits. REQUESTS and PRODUCERS come from the application's
     * capacity set (Capacity.h); URGENT_REQUESTS and SEQUENCES follow the SDO channel count,
     * because each channel anchors one binding with one usable reservation. The per-turn limits
     * and TRACE_CAPACITY are fixed: they bound one turn's work, not the number of axes.
     */
    enum {
        REQUESTS = int(MotorControlCAN::CANOPEN::AdapterCapacity::OWNER_REQUESTS), ///< Queued + active + unread terminal capacity, shared ordinary slots 0..REQUESTS-1.
        URGENT_REQUESTS = int(MotorControlCAN::CANOPEN::SdoClient::CHANNELS), ///< One reserved urgent request/result slot per channel, slot REQUESTS + channel, so one axis cannot block another's. Never used by ordinary work.
        TOTAL_REQUESTS = REQUESTS + URGENT_REQUESTS, ///< All copied work/result storage, at most 255.
        SEQUENCES = int(MotorControlCAN::CANOPEN::SdoClient::CHANNELS), ///< Logical-axis reservations, one per channel binding, independent of wire node numbering.
        PRODUCERS = int(MotorControlCAN::CANOPEN::AdapterCapacity::OWNER_PRODUCERS), ///< Cyclic eligible-producer fairness, identities 1..PRODUCERS, FIFO within each sequence.
        RX_LIMIT = 8,      ///< Maximum receive callback calls per service turn.
        TX_LIMIT = 2,      ///< Maximum one-shot enqueue callbacks per turn.
        EVENT_LIMIT = 8,   ///< Maximum TX report callback calls per turn.
        START_LIMIT = 2,   ///< Maximum new native service admissions per turn.
        TRACE_CAPACITY = 24, ///< Above one turn's RX, TX, report, terminal and controller records. Oldest-first diagnostic copies; full storage overwrites oldest.
        /** Default TX lifetime in microseconds, a host policy, not a device fact. On the S2 bench
         * the owner consumed each TX report about 3 ms after handoff and service gaps reached 4 to
         * 6 ms (docs/reports/2026-11-12_host_throughput.md section 1); the adapter keeps one SDK
         * frame in flight, so a frame can wait behind CHANNELS + 2 others at one submission per
         * pass. 100 ms covers that for the default capacity set with a wide margin, and equals the
         * bench example's SDO response timeout, so expiry is reported no later than the response
         * cutoff its own request already has. The submit-to-done maximum under load is unmeasured:
         * set it with setTxLifetime from the application's own measurement.
         */
        DEFAULT_TX_LIFETIME_US = 100000
    };
    /** Borrow stationary exclusive resources; construction performs no I/O.
     * @param[in] services Sole existing adapter, already configured or configured while idle.
     * @param[in] transport Copied callback bundle; context remains borrowed for owner lifetime.
     * @param[in] identity Nonzero application owner identity, suitable for `AxisReservation`.
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
     * @param[in] deadlineUs Immutable absolute deadline, strictly after nowUs. The operation's work
     * expires once processing time reaches it, `now >= deadlineUs`, even if a frame for it was
     * received earlier.
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
     * @return False unchanged for wrong ID, live work or unread terminals. True, with
     * nothing changed, for an identity whose slot was since reused: it holds nothing.
     */
    bool releaseSequence(SequenceId id) noexcept;
    /** Copy one yielded native request and reserve its immutable terminal slot.
     * @param[in] request Complete input; matching operation and increasing step. Its immutable
     * service deadline may be earlier than, but never later than, the operation deadline. A
     * finite run can thus retire start work at stop due while retaining its stop-settlement budget.
     * @param[in] nowUs Same nondecreasing clock; queue/overall age begins here.
     * @param[out] output Unchanged on rejection; no overlap with owner or request.
     * @return ACCEPTED is local only. NO_LISTENER while noListener() holds, with nothing sent.
     * Rejection changes neither queue nor reservation.
     */
    BusAdmission admit(const BusRequest& request, uint64_t nowUs, RequestId& output) noexcept;
    /** Start a new urgent operation on a live reservation, such as a stop during a move.
     * Retires this reservation's unsubmitted work as terminal with no TX. An in-flight
     * request is left to settle, because cancelling it could quarantine the route; the
     * new work waits for it. The new operation gets its own identity and deadline, and
     * old results stay readable.
     * @param[in] id Exact usable reservation. A quarantined route refuses this. Retired
     * urgent work keeps its channel's urgent slot until its result is released.
     * @param[in] operation Identity above the reservation's current operation.
     * @param[in] deadlineUs Immutable deadline, after nowUs, independent of the old work's.
     * @param[in] nowUs Nondecreasing processing microseconds.
     * @return True when the new operation began.
     */
    bool beginUrgentOperation(SequenceId id, uint32_t operation, uint64_t deadlineUs,
                              uint64_t nowUs) noexcept;
    /** Read a node's current epochs, for an application that must keep its targets current.
     * @param[in] binding Exact configured binding.
     * @param[out] nodeGeneration Current node epoch, which any invalidation advances.
     * @param[out] serverSessions Count that a local controller event does not advance.
     * @return False, with outputs unchanged, for an unknown binding or exhausted node.
     */
    bool nodeEpochs(const Binding& binding, MotorControlCAN::Generation& nodeGeneration,
                    uint32_t& serverSessions) const noexcept;
    /** Check cached exact route/node provenance without reservation, I/O or eligibility claims.
     * @param channel Already configured SDO route, including both COB-IDs.
     * @param nodeGeneration Expected nonzero current node epoch.
     * @return True only for a matching channel and current unexhausted node. Quarantine is not
     * cleared or hidden: a match permits host selection, not service admission.
     */
    bool matchesChannel(const MotorControlCAN::CANOPEN::SdoChannel& channel,
                        MotorControlCAN::Generation nodeGeneration) const noexcept;
    /** Find the usable reservation of a binding, for a producer that does not hold it.
     * @param[in] binding Exact current binding.
     * @param[out] output Published only when found.
     * @return False when none is usable, including on a quarantined route.
     */
    bool findSequence(const Binding& binding, SequenceId& output) const noexcept;
    /** Admit an explicit best-effort stop outside any reservation (DEC-2).
     * Accepts only `6040:00=000B`, in the channel's urgent slot. It may go out on an
     * unresolved route. It is transmitted at most once, and no reply can qualify it, so
     * its route ends unresolved. It waits for local work in ordinary slots to settle.
     * Stops keep priority over repair: once placement cannot fail, a marker the owner issued
     * itself on any channel of the node is cancelled (the route stays unresolved), and an unsent
     * cleanup abort on the stop's channel is dropped before dispatch (it settles nothing). Every
     * other slot content is kept, as before. Acceptance also ends every pending resync step on
     * the node under every policy, and none is armed there again until a reopening: the stop's
     * own reply may still come and could confirm a later 6040 write, so reopening after a stop
     * is explicit intent only (admitDrainMarker, or reacceptReuse after a refusal).
     * @param[in] transfer The exact stop with its own finite deadline.
     * @param[in] nowUs Nondecreasing processing microseconds.
     * @param[out] output Published only on acceptance.
     * @return INVALID for any other transfer or a past deadline; INHIBITED while the
     * controller, external TX or network state cannot send it; RESULTS_FULL while that
     * channel's urgent slot holds other work or an unread result.
     */
    BusAdmission admitBestEffortStop(const MotorControlCAN::CANOPEN::SdoTransfer& transfer,
                                     uint64_t nowUs, RequestId& output) noexcept;
    /** Copy work into reserved urgent capacity; never preempts or stops existing work.
     * @param[in] request Same validation as admit; reuse the axis's live reservation.
     * FIFO within that reservation still applies. Begin a new operation with
     * beginUrgentOperation to retire obsolete work; quarantined routes remain unavailable.
     * @param[in] nowUs Nondecreasing processing microseconds.
     * @param[out] output Published only on acceptance, with admit's nonaliasing rules.
     * @return Local admission only; ordinary pressure cannot consume this slot.
     */
    BusAdmission admitUrgent(const BusRequest& request, uint64_t nowUs, RequestId& output) noexcept;
    /** Admit one explicit drain marker for an unresolved route, outside any reservation (DEC-3,
     * amended by prompt 55). The binding's reuse policy states the premises (sole client,
     * ordered server) as accepted, never established; under SdoReusePolicy::NONE every marker
     * is refused. The marker uses the channel's urgent slot. On a matched success, and after its
     * own frame is locally retired, the owner invalidates every reservation on the node and
     * reopens the route, so new work needs a new reservation and carries
     * Completion::acceptedCorrelation. A configured expected value that differs invalidates the
     * binding instead. Failure leaves the route unresolved. Earlier effects stay uncertain and
     * nothing replays. Admitting one ends a pending automatic resync step on that route: a
     * further marker is always new explicit intent.
     * @param[in] transfer One accepted (DEC-1) expedited upload of 1-4 bytes whose object was
     * not retained on this route and is not its last completed pair; its own finite deadline.
     * @param[in] nowUs Nondecreasing processing microseconds.
     * @param[out] output Published only on acceptance; may not overlap the owner or the input.
     * @return ACCEPTED is local admission only. INHIBITED while local work is pending or blocked
     * or the controller is inhibited; INVALID for NONE, refused reuse, a failed marker check or
     * bad input; RESULTS_FULL while the urgent slot is occupied. NO_LISTENER never applies.
     */
    BusAdmission admitDrainMarker(const MotorControlCAN::CANOPEN::SdoTransfer& transfer,
                                  uint64_t nowUs, RequestId& output) noexcept;
    /** Compatibility form for callers that still state premises themselves; prefer the policy
     * form above. Refused under SdoReusePolicy::NONE; otherwise both supplied premises must hold,
     * as before prompt 55, and they are recorded as accepted, not established.
     * @param[in] transfer As the policy form.
     * @param[in] premises Caller's statement of the sole-client and ordered-server premises.
     * @param[in] nowUs Nondecreasing processing microseconds.
     * @param[out] output Published only on acceptance.
     * @return As the policy form; INVALID when either premise is false.
     */
    BusAdmission admitDrainMarker(const MotorControlCAN::CANOPEN::SdoTransfer& transfer,
                                  const MotorControlCAN::CANOPEN::SdoDrainPremises& premises,
                                  uint64_t nowUs, RequestId& output) noexcept;
    /** Set one binding's SDO route reuse policy and marker objects (prompt 55).
     * The default, before any call, is SdoReuse(): ACCEPTED_RISK_WITH_MARKER, markers 1000:00,
     * 1018:01, 1018:02 and 1018:03 tried in order, no expected values, marker budgets inherited
     * from the route's last request. A change applies to later resync steps; it does not reopen,
     * re-quarantine or cancel anything, and an admitted marker keeps its premises and budgets.
     * Changing to NONE drops a pending resync step.
     * @param[in] binding Exact configured SDO channel binding.
     * @param[in] reuse Policy within the enumeration; markerCount 1..SdoReuse::MARKER_OBJECTS;
     * each used object with a nonzero index, size 1..4 and an expected value that fits that size,
     * the used objects pairwise distinct; marker budgets any value, zero inheriting. Copied; no
     * pointer is retained.
     * @param[in] nowUs Nondecreasing supplied microseconds.
     * @return False, with nothing changed, for an unknown binding, invalid settings, backward
     * time, reentrancy or overlapping input.
     */
    bool setSdoReuse(const Binding& binding, const SdoReuse& reuse, uint64_t nowUs) noexcept;
    /** Copy one binding's reuse setting; no I/O, clock or mutation.
     * @param[in] binding Exact configured SDO channel binding.
     * @param[out] output Copy on success, unchanged otherwise; may not overlap the owner.
     * @return False for an unknown binding.
     */
    bool sdoReuse(const Binding& binding, SdoReuse& output) const noexcept;
    /** Re-accept reuse on a route where contradicting traffic stopped it, as an explicit
     * application decision. The owner never calls it. It also ends a best-effort stop's hold on
     * this route. The route stays unresolved; the binding's policy then takes its resync step
     * once, as after a timeout: one marker read
     * (ACCEPTED_RISK_WITH_MARKER, with the configured marker budgets or else the route's last
     * request budgets; with neither the step reports MARKER_UNAVAILABLE and admitDrainMarker
     * remains) or the no-marker reopening (ACCEPTED_RISK_WITHOUT_MARKER), each after local TX
     * retirement.
     * @param[in] binding Exact configured SDO channel binding whose reuse is refused.
     * @param[in] nowUs Nondecreasing supplied microseconds.
     * @return False, unchanged, for an unknown binding, policy NONE, no refusal, backward time or
     * reentrancy. True traces a REUSE_ACCEPTED diagnostic.
     */
    bool reacceptReuse(const Binding& binding, uint64_t nowUs) noexcept;
    /** Set the TX lifetime for every later accepted handoff check. A frame the transport accepted
     * that has no final report once lifetimeUs has elapsed is reported expired (Stats::txExpired,
     * a TX_EXPIRED diagnostic, OwnerResources::txExpired) and the interface enters NO_LISTENER.
     * The lifetime starts at the SDK submission an adapter reports (SdoClient::noteSubmitted,
     * noteNmtSubmitted, noteSyncSubmitted); a frame an adapter still holds behind another has
     * not started. Without such a report it starts at the owner's handoff. Nothing is counted
     * while a recovery is pending. The frame stays possibly sent and blocks its channel; nothing
     * is resent and no recovery starts. A late final report retires it normally; otherwise only
     * an explicit recover() retires it, as possibly sent.
     * @param[in] lifetimeUs Microseconds after that start, checked in service turns at equality;
     * zero disables the check. Default DEFAULT_TX_LIFETIME_US.
     * @param[in] nowUs Nondecreasing supplied microseconds.
     * @return False for backward time or reentrancy, unchanged.
     */
    bool setTxLifetime(uint64_t lifetimeUs, uint64_t nowUs) noexcept;
    /** @return Current TX lifetime in microseconds; zero means disabled. */
    uint64_t txLifetimeUs() const noexcept { return txLifetimeUs_; }
    /** NO_LISTENER interface state: entered when a TX lifetime expires, kept through recovery,
     * ended by a matching positive TX report or a valid current-epoch received frame whose
     * capture is not known to precede the start of NO_LISTENER (an exact or earliest bound
     * capture at or before that start does not end it; unknown capture does). While it lasts,
     * ordinary admit returns BusAdmission::NO_LISTENER with nothing sent and ordinary work
     * already queued does not start (its own queue cutoff still runs), while urgent work,
     * best-effort stops and explicit drain markers stay admissible, so one ACKed frame can end
     * it. No automatic resync step starts while it lasts, those re-armed by a retired recovery
     * included: they become eligible when it ends. NMT and SYNC admission and dispatch are
     * unchanged. Ending it restores controller evidence
     * only: no identity, route freshness, service qualification or motion readiness.
     * @return True while in NO_LISTENER. Cached, no I/O.
     */
    bool noListener() const noexcept { return noListener_; }
    /** Query whether cancelling a request could quarantine its route.
     * @param[in] id Current request.
     * @return True if a frame of it may have reached a controller.
     */
    bool mayHaveTransmitted(RequestId id) const noexcept;
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
     * This is the one controller-restart entry point: bus-off, RX overflow and an expired TX
     * lifetime all use it, keep their own causes (RecoveryCompletion::causes, the conditions
     * reported since the previous recovery began) and never call it themselves. A cause reported
     * while this recovery runs stays latched for the next one; a failed or timed-out recovery
     * hands its own causes on as well. Single flight: a pending or unread result refuses another
     * call.
     * @param[in] nowUs Nondecreasing microseconds; no hidden time acquisition.
     * @param[in] deadlineUs Absolute deadline, strictly greater than nowUs. The recovery times out
     * once processing time reaches it, `now >= deadlineUs`, before any later poll can report it
     * retired, even if the adapter finished earlier.
     * @param[out] output Published on acceptance only; may not overlap owner/adapter.
     * @return True reserves a result, invalidates all reservations/node freshness and
     * invokes at most one beginRecovery callback. Missing/failed callback closes FAILED.
     * No wire command, SDO reopening, PDO qualification, replay or physical stop occurs.
     * A RETIRED closure retires every pending handoff, an expired one too, as possibly sent:
     * routes it may answer on stay unresolved, and each route the controller event left
     * unresolved with possible TX gets its reuse policy's one resync step (not after a
     * best-effort stop). No resync step, armed then or before, starts in the service turn
     * whose poll retired the recovery: from the next turn on, so an application that sees the
     * closure can admit a held stop first, and the stop then ends the step on its node.
     * NO_LISTENER survives it.
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
    /** @return True while a begun recovery has no terminal yet. */
    bool recoveryActive() const noexcept { return recoveryOccupied_ && !recoveryTerminal_; }
    /** @return Latest accepted supplied monotonic microseconds, initially zero.
     * Cached only, never a clock read. Includes explicit application calls between
     * service turns; use this authority when a platform clock itself fails.
     */
    uint64_t lastTimeUs() const noexcept { return lastUs_; }
    /** Run one bounded cooperative turn; deadlines progress even with all budgets zero.
     * Order: deadlines, recovery poll, TX reports, RX (with presence observations), then the
     * TX lifetime check, so a report or frame taken in this turn prevents an expiry or ends
     * NO_LISTENER first. A staged cleanup abort waits while urgent work on another channel can
     * start this turn. A binding's resync step admits its marker only in a turn with TX and
     * start budget, after the route's local TX is retired and no stop waits on its node, never
     * in the turn whose recovery poll retired a recovery (the one-turn grace for a held stop)
     * and never while noListener() holds.
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
    /** Arm the explicit one-shot service, never schedule or transmit implicitly.
     * @param configured1005 Only default 0x80 is currently supported; custom route analysis is pending.
     * @param nowUs Nondecreasing caller microseconds.
     * @return False for conflict, known route collision, retained work, unretired TX or unavailable transport.
     * Caller establishes one producer per network and resolves any remote buffered RPDO hazard first.
     */
    bool armSync(uint32_t configured1005, uint64_t nowUs) noexcept;
    /** Disarm and cancel pending work locally; never stops motion or clears a remote RPDO buffer.
     * @param nowUs Nondecreasing caller time. @return False unchanged on time/reentrancy failure.
     */
    bool disarmSync(uint64_t nowUs) noexcept;
    /** Clear a foreign-producer latch while disarmed and locally retired; does not rearm.
     * @param evidenceId Nonzero application record establishing exclusive producer ownership.
     * @param nowUs Nondecreasing caller time. @return False while work/transport is retained or active.
     * Neither quiet time nor controller recovery is such evidence.
     */
    bool resolveSyncConflict(uint32_t evidenceId, uint64_t nowUs) noexcept;
    /** Copy one due DLC-zero work item into independent fixed request/result capacity.
     * @param work Owned candidate from nextSync; dueUs <= nowUs < expiresUs, nonzero generation.
     * @param nowUs Nondecreasing supplied microseconds. @param output Changed only on acceptance, no overlap.
     * @return Local admission only; never consumes ordinary or urgent SDO capacity.
     */
    BusAdmission admitSync(const MotorControlCAN::CANOPEN::SyncWork& work, uint64_t nowUs,
                          SyncRequestId& output) noexcept;
    /** @param id Exact admission. @param nowUs Caller time; deadline wins at equality.
     * @return True only when local cancellation wins; retirement may remain pending.
     */
    bool cancelSync(SyncRequestId id, uint64_t nowUs) noexcept;
    /** @param id Exact admission. @return Immutable retained terminal or null until closure. */
    const SyncTerminal* syncResult(SyncRequestId id) const noexcept;
    /** @param id Exact terminal. @return True releases storage only, not the TX interlock. */
    bool releaseSync(SyncRequestId id) noexcept;
    /** @return Explicit production permission, never inferred from controller recovery. */
    bool syncArmed() const noexcept { return syncArmed_; }
    /** @return Nonwrapping explicit-arm epoch; zero before first arm, never a wire identity.
     * Every successful arm advances it, including rearm between application service calls.
     * Disarm remains visible through syncArmed and syncDisarmOutcome; exhaustion refuses arm.
     */
    uint32_t syncArmGeneration() const noexcept { return syncArmGeneration_; }
    /** @return Last local disarm cause; meaningful while !syncArmed(), never a motor-stop result. */
    MotorControlCAN::CANOPEN::SyncOutcome syncDisarmOutcome() const noexcept { return syncDisarmOutcome_; }
    /** @return Latched foreign producer conflict, including while disarmed. */
    bool syncConflict() const noexcept { return syncConflict_; }
    /** @return Last foreign observation, including historical bounds; no freshness inference. */
    const SyncObservation& syncObservation() const noexcept { return syncObservation_; }
    /** @param id Exact handoff. @param nowUs Caller time. @return Current liveness/deadline only.
     * Adapter must also check syncTxPriorityReady before first SDK submission.
     */
    bool canTransmitSync(SyncRequestId id, uint64_t nowUs) const noexcept;
    /** @return False while any urgent request is pending; never preempts already active CAN TX. */
    bool syncTxPriorityReady() const noexcept;
    /** Adapter-only synchronous callback hook proving staging has not reached the SDK.
     * @param id Exact handoff. @return False for stale/nonpending identity; otherwise records unsent staging.
     */
    bool holdSyncHandoff(SyncRequestId id) noexcept;
    /** Record first actual SDK submission after the adapter checked eligibility.
     * @param id Exact handoff. @param nowUs Nondecreasing time. @return False if no longer eligible.
     */
    bool noteSyncSubmitted(SyncRequestId id, uint64_t nowUs) noexcept;
    /** Report proof that adapter staging never reached SDK; late reports cannot alter terminals.
     * @param id Exact admission. @param nowUs Caller time. @return True only for a current pending request.
     */
    bool withdrawSyncUnsent(SyncRequestId id, uint64_t nowUs) noexcept;
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
    /** Report whether release would succeed now, before work that depends on it.
     * @param[in] id Request whose result would be released.
     * @return True with a terminal result and outside an owner call.
     */
    bool canRelease(RequestId id) const noexcept;
    /** Synchronize a completed exclusive settings transaction's configuration epoch.
     * @param[in] id Live reservation; every owner request on this node must be released,
     * and every external handoff on the node must have proved local retirement.
     * @param[in] current Same binding with exactly next configuration epoch, already
     * withdrawn by the application's pre-TX settings notification.
     * @param[in] nowUs Same monotonic caller time.
     * @return Adapter's checked idle advance only; preserves frozen settings token,
     * node history and quarantine. No device action, reference renewal or recovery.
     * The adapter also requires its node premises: notably current ordinary SDO service
     * and no pending NMT transition for the node (SdoClient::configurationEligible). They
     * do not follow from a granted experiment or an acknowledged write, so check them
     * before the settings download is admitted; a refusal after a possible TX leaves that
     * write unconfirmed.
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
    /** Verify the stationary native-store graph without I/O or progress.
     * @param services Expected existing store. @return True only for the borrowed store. */
    bool usesServices(const MotorControlCAN::CANOPEN::SdoClient& services) const noexcept {
        return &services == &services_;
    }

  private:
    struct Reservation {
        bool occupied = false, invalidated = false;
        uint32_t generation = 0, lastStep = 0;
        Sequence value;
    } sequences_[SEQUENCES];
    struct Slot {
        bool occupied = false, terminal = false, submitted = false, guarded = false;
        bool localCancellation = false, outsideSequence = false, bestEffortStop = false;
        bool automaticMarker = false;
        MotorControlCAN::CANOPEN::SdoDrainPremises premises;
        uint32_t generation = 0;
        uint64_t queueDeadlineUs = 0;
        BusRequest request;
        MotorControlCAN::CANOPEN::SdoRequestId native;
        Completion completion;
    } slots_[TOTAL_REQUESTS];
    struct ExternalTx {
        bool pending = false, blocked = false, retirementTried = false, rejected = false;
        bool expired = false; // Reported once past the TX lifetime; still possibly sent.
        uint32_t epoch = 0;
        uint64_t handoffUs = 0;
        TransportTxId id;
    } external_[MotorControlCAN::CANOPEN::SdoClient::CHANNELS], nmtExternal_, syncExternal_;
    // Per channel: the binding's reuse setting and the state of its one pending resync step.
    struct Resync {
        SdoReuse reuse;
        bool pending = false;    // A timeout (or a re-acceptance) awaits the policy's step.
        bool mismatch = false;   // A marker value differed; invalidate instead of reopening.
        bool refusedSeen = false; // Last reported reuse refusal, to count each refusal once.
        bool stopped = false;    // A best-effort stop on the node: reopening is explicit intent only.
        bool possible = false;   // Possible TX when a controller event struck; re-armed when retired.
        bool settling = false;   // A marker succeeded; the reopening has not happened yet.
        uint32_t sessions = 0;
        uint32_t queueTimeoutUs = 0, responseTimeoutUs = 0; // Last request's budgets on the route.
    } resync_[MotorControlCAN::CANOPEN::SdoClient::CHANNELS];
    uint64_t txLifetimeUs_ = DEFAULT_TX_LIFETIME_US;
    bool noListener_ = false;
    uint64_t noListenerSinceUs_ = 0; // Processing time NO_LISTENER began.
    bool retiredThisTurn_ = false; // A recovery retired in this service turn: no resync step yet.
    RecoveryCauses causes_; // Reported since the current or last recovery began.
    SyncTerminal sync_;
    SyncObservation syncObservation_;
    bool syncOccupied_ = false, syncTerminal_ = false, syncArmed_ = false, syncConflict_ = false;
    bool syncHanded_ = false, syncHeld_ = false, syncHandoffCallback_ = false;
    uint32_t syncResolutionEvidence_ = 0;
    uint32_t syncArmGeneration_ = 0;
    MotorControlCAN::CANOPEN::SyncOutcome syncDisarmOutcome_ =
        MotorControlCAN::CANOPEN::SyncOutcome::INVALIDATED;
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
    int active_[MotorControlCAN::CANOPEN::SdoClient::CHANNELS]; ///< In-flight slot per channel; -1 none, set by the constructor.
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
    bool markerCurrent(const Slot& slot) const noexcept;
    BusAdmission placeOutside(uint8_t channel, const MotorControlCAN::CANOPEN::SdoTransfer& transfer,
                              uint64_t nowUs, RequestId& output) noexcept;
    BusAdmission admitMarker(const MotorControlCAN::CANOPEN::SdoTransfer& transfer,
                             const MotorControlCAN::CANOPEN::SdoDrainPremises& premises,
                             uint64_t nowUs, RequestId& output) noexcept;
    void reopenSettledRoutes(uint64_t nowUs) noexcept;
    bool markActive(unsigned channel) const noexcept;
    bool stopWaits(const Binding& binding) const noexcept;
    bool cleanupStaged(uint8_t channel) const noexcept;
    void armResync(unsigned channel) noexcept;
    void notePossible() noexcept;
    void resyncRoutes(uint64_t nowUs) noexcept;
    void markerTerminal(unsigned slot, uint64_t nowUs) noexcept;
    void noteReuse() noexcept;
    void expireHandoffs(uint64_t nowUs) noexcept;
    void listener(bool absent, uint64_t nowUs) noexcept;
    void resyncTrace(unsigned channel, ResyncStep step, uint64_t nowUs) noexcept;
    void observePresence(const ReceivedFrame& frame, bool accepted, uint64_t nowUs) noexcept;
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
    bool transmitSync(uint64_t nowUs) noexcept;
    bool syncRoutesCompatible() const noexcept;
    bool validSync(SyncRequestId id) const noexcept;
    void finishSync(MotorControlCAN::CANOPEN::SyncOutcome outcome, uint64_t nowUs) noexcept;
    void expireSync(uint64_t nowUs) noexcept;
    bool observeSync(const ReceivedFrame& frame, uint64_t nowUs) noexcept;
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
