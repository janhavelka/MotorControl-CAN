/** @file AxisSession.h
 * @brief One axis's application state: reservation, latest controlword, stop, enable, move and clear work.
 * SPDX-License-Identifier: MIT
 */
#pragma once
#include "CanBusOwner.h"
#include <MotorControlCAN/profiles/lichuan_c/Actions.h>
#include <MotorControlCAN/profiles/lichuan_c/Position.h>

namespace MotorControlCANExample {
/** Where a requested stop stands. Never a claim about the drive. */
enum class StopPath : uint8_t {
    NONE,        ///< No stop requested, or the request was refused with nothing changed.
    CONFIRMED,   ///< Sequenced stop in progress: stopAction(kind) holds its evidence.
    BEST_EFFORT, ///< The owner holds the 000B frame; only its terminal can settle it.
    HELD,        ///< This 000B has not been admitted; it waits for the owner or one recovery.
    FINISHED     ///< Terminal; contexts keep every outcome. Only a new request can stop again.
};
/** Caller limits for one stop request, fixed for its whole life. */
struct StopPolicy {
    MotorControlCAN::LICHUAN_C::ActionOptions options; ///< Nonzero service and observation budgets.
    /** Exclusive cutoff of the sequenced stop, and of a held stop that came from it; future
     * and immutable. */
    uint64_t deadlineUs = 0;
    /** Exclusive cutoff of the branch taken for missing history, stale state or an
     * unresolved route, at or after deadlineUs. Zero refuses that branch, so such a request is
     * refused. A stop proved never sent by a controller fault still waits under deadlineUs. */
    uint64_t bestEffortDeadlineUs = 0;
};
/** The application session of one axis over the sole owner. It keeps the history
 * prepareStop cannot see. Every command this axis sends goes through admitPrepared, so
 * the latest 6040 terminal here is the actual latest, and a copied terminal is never
 * authority. Writes outside the session need the reservation it holds. No clock, I/O,
 * retry or replay. One cooperative caller serializes all calls.
 *
 * Time: every member that takes a time refuses one before the later of this session's last time and
 * owner.lastTimeUs(), with nothing changed, because the owner would refuse it part way through.
 * requestStop alone still ends an active enable or move, at that later time.
 *
 * A stop waits for in-flight work to settle and never cancels it. If that leaves no
 * usable route before any stop frame could leave, the request takes the 000B best-effort
 * branch, when the policy allows. A stop proved never sent by a controller fault waits
 * across one explicit recovery and goes out once as 000B. A second recovery ends it. A
 * possibly sent frame is never sent again.
 *
 * Enable is gated: two specimens ran it once each, attended, on one firmware. It takes one member,
 * one request, one service branch and one advance. Its words pass through the same path as every other command, so an acknowledged 000F
 * becomes the latest controlword that a stop may follow. A stop request ends an active enable first,
 * even if the stop itself is refused: no further word follows it. The session refuses an enable once
 * it has itself admitted a write other than an acknowledged 6060=1. That record lasts as long as this
 * object, across close and open, because reopening cannot retire earlier drive work. A new object
 * knows nothing, as after an MCU restart, so the caller's EnableEvidence must speak for that time.
 *
 * The absolute and relative moves are gated too. They share one member, one request path, one
 * service branch and one advance; only the trigger word differs. Observation cycles run in two
 * session-owned reads in turn, so the last complete cycle stays available to requestStop while the
 * next one runs: pass moveObservation(). A stop request ends an active move first, even if the stop
 * itself is refused, and the held trigger word is the stop's predecessor. Withdrawing a move cancels
 * a download the owner has not yet handed to the controller. Only a cancel the owner reports as won
 * proves it unsent; otherwise its own terminal settles into the move and says whether it left, and
 * preflight() refuses it if it comes up for transmission. A download that closed provably unsent
 * leaves the controlword before it as the latest. After a move whose trigger may have reached the
 * drive without an observed completion, new moves stay fenced for this object's life: nothing here
 * can show that motion ended. No relative base is measured on any specimen, so every relative request
 * is refused today.
 *
 * The move's actual admission is rechecked when each parameter or trigger download is admitted:
 * the borrowed axis, the witness and the admission read's age. The owner hands the frame over at a
 * later turn. For the recheck to hold up to that moment, the application's Transport::beforeTransmit
 * must call preflight() of the session that holds the sequence. Without it the recheck holds only at
 * admission: an invalidateAxisReference, configureAxis or witness expiry in between does not stop that
 * frame, though it still refuses the next one.
 *
 * The alarm clear (2301) and the zero-only counter clear (2302) share one slot, one service branch and
 * one withdrawal; no specimen has run either. Each is one explicit request, and no write of it is ever
 * sent twice. A stop request ends an active clear first, even if the stop itself is refused, and cancels
 * a write the owner has not yet handed to the controller, as for a move. A clear whose value 1 may still
 * be held by the drive (the context's valueOneMayRemain) blocks every later request but a stop for this
 * object's life, across result release, close and open, as MotorControl-RS's actionConflicts does. A
 * counter clear withdraws the borrowed axis's coordinates when its 1 is admitted, once, and then moves the
 * owner's reservation to that new configuration epoch itself, withdrawing nothing a second time.
 */
class AxisSession {
  public:
    /** Borrow the owner; construction performs no I/O or reservation.
     * @param[in] owner Sole CAN owner, outlives this session.
     */
    AxisSession(MotorControlCANOwner::CanBusOwner& owner) noexcept : owner_(owner) {}
    AxisSession(const AxisSession&) = delete;            ///< Live handles are not copyable.
    AxisSession& operator=(const AxisSession&) = delete; ///< Stationary application storage.
    /** Open on the axis's usable reservation, or take a new one for producer. With none
     * available, as on an unresolved route, only the best-effort branch can run.
     * @param[in] target Current route and node epoch.
     * @param[in] model Applicable model, supplied by the application from its evidence.
     * @param[in] producer Fairness identity for a new reservation.
     * @param[in] nowUs Nondecreasing supplied microseconds.
     * @return False unchanged when already open or the target is not current.
     */
    bool open(const MotorControlCAN::LICHUAN_C::ReadTarget& target,
              MotorControlCAN::LICHUAN_C::SettingsModel model, uint8_t producer,
              uint64_t nowUs) noexcept;
    /** Close after every result is consumed, keeping no history. Releases only a
     * reservation this session took. A counter clear's owner synchronization that service() could not
     * yet make is dropped with that history: the application then calls
     * CanBusOwner::advanceConfiguration itself before it opens on the axis's new binding.
     * @return False unchanged while a command or stop is live or its result is unread.
     */
    bool close() noexcept;
    /** Take over an adopted reservation, so that close releases it. */
    void adoptReservation() noexcept { ownsReservation_ = sequence_.owner != nullptr; }
    /** @return True between a successful open and close. */
    bool isOpen() const noexcept { return open_; }
    /** @return The binding this session was opened on; meaningful only while open. */
    const MotorControlCAN::Binding& binding() const noexcept { return target_.channel.binding; }
    /** @return The reservation this session uses; meaningful only while open. */
    MotorControlCANOwner::SequenceId sequence() const noexcept { return sequence_; }
    /** @return True when a stop is in progress. */
    bool stopActive() const noexcept {
        return path_ != StopPath::NONE && path_ != StopPath::FINISHED;
    }
    /** @return True while an enable is neither finished nor failed. */
    bool enableActive() const noexcept {
        return enable_.state == MotorControlCAN::ActionState::ACTIVE;
    }
    /** @return True while a move is neither finished nor failed. */
    bool moveActive() const noexcept { return move_.state == MotorControlCAN::ActionState::ACTIVE; }
    /** @return True while an alarm clear or counter clear is neither finished nor failed. */
    bool clearActive() const noexcept { return clear_.state == MotorControlCAN::ActionState::ACTIVE; }
    /** @return True with a stop, enable, move or clear in progress or a request unsettled, which excludes
     * new operations and reads: starting one would orphan that terminal's history. */
    bool busy() const noexcept {
        return stopActive() || enableActive() || moveActive() || clearActive() || pending_.owner ||
               urgent_.owner || superseded_.owner;
    }
    /** Start a new operation for the next commands on this reservation.
     * @param[in] deadlineUs Immutable exclusive future cutoff.
     * @param[in] nowUs Nondecreasing supplied microseconds.
     * @return Operation identity to put in the commands; zero when refused, including
     * while earlier work is live or a stop is active.
     */
    uint32_t beginOperation(uint64_t deadlineUs, uint64_t nowUs) noexcept;
    /** Admit one yielded command. A download withdraws the latest controlword at once,
     * because from here it may reach the drive before any report says so.
     * @param[in] transfer Complete yielded work for the current operation.
     * @param[in] nowUs Nondecreasing supplied microseconds.
     * @return Owner admission. INVALID while another command is unread or a stop is active.
     */
    MotorControlCANOwner::BusAdmission admitPrepared(const MotorControlCAN::CANOPEN::SdoTransfer& transfer,
                                                     uint64_t nowUs) noexcept;
    /** Consume the terminal of the admitted command and release its slot.
     * A 6040 acknowledgement of the current operation becomes the latest controlword.
     * @param[out] output Owned copy, published only when true.
     * @return False while no terminal is available, or while an enable owns the request.
     */
    bool settle(MotorControlCAN::CANOPEN::SdoCompletion& output) noexcept;
    /** Request a stop on this axis.
     * Upward only: QUICK_STOP may replace a running HALT, and nothing else replaces a
     * running stop. Queued work retires with explicit results; in-flight work settles.
     * The sequenced path needs the latest controlword and fresh state; escalation uses the
     * running HALT's own acknowledgement and latest cycle. A known unsupported predecessor, such
     * as enable word 6 or 7, refuses the request only when fresh state reports the drive not
     * running. Fresh state reporting an idle drive that is alarmed, released, in another mode or
     * not enabled also refuses it. Otherwise, including a fresh running report after such a word
     * or a word still in flight, a missing condition takes the 000B branch, whatever kind was
     * asked for, when the policy allows it.
     * @param[in] kind Native selection for the sequenced path.
     * @param[in] state Application's latest complete state read. A read still in progress
     * counts as missing: keep the last complete copy for this. It must not be held inside
     * stopAction(kind), which this request overwrites: that also counts as missing. During or after
     * a move pass moveObservation(), whose cycle began after the held trigger word.
     * @param[in] requested Copied caller limits; invalid budgets refuse the request. Its identity
     * width setting is ignored, so options shared with identity reads cannot refuse a stop.
     * @param[in] nowUs Nondecreasing supplied microseconds. A time before this session's or the
     * owner's last time refuses the stop but still ends an active enable, move or clear, at the later
     * of those two times, where the owner can still cancel the move's or clear's unsent download.
     * @return The path taken; NONE when refused, with only a stale controlword withdrawn and an
     * active enable, move or clear ended, because asking for a stop means no further word of theirs. A
     * read held in enableAction() or clearAction() is judged after that action has ended. A stop
     * requested during a clear usually finds no latest controlword, since the clear's writes withdrew
     * it, and so takes the 000B branch when the policy allows; fresh state reporting an idle alarmed
     * drive refuses it.
     */
    StopPath requestStop(MotorControlCAN::LICHUAN_C::StopKind kind,
                         const MotorControlCAN::LICHUAN_C::ReadContext& state,
                         const StopPolicy& policy, uint64_t nowUs) noexcept;
    /** Request the gated position-mode enable sequence on this axis. No specimen has run it.
     * Starts a new owner operation, then prepares in place. Yields no traffic here: service() does.
     * @param[in] state Complete fresh SDO read under this reservation, as prepareEnable requires,
     * and not held inside enableAction().
     * @param[in] evidence Caller's explicit gate, trial, reservation, operation (use nextOperation),
     * expiry, start record, shaft observation and exact reviewed values. The session adds its own
     * record: it refuses after any write of its own other than 6060=1, and it consumes each trial
     * identity once, in rising order, for this object's life.
     * @param[in] options Copied budgets, as prepareEnable requires.
     * @param[in] deadlineUs Immutable exclusive cutoff of the whole sequence.
     * @param[in] nowUs Nondecreasing supplied microseconds.
     * @return prepareEnable's status, or a local refusal with ActionError::STATE, TARGET or TIME, which
     * includes clearBlocked(). On any refusal nothing changes, and on success no word has yet been
     * admitted.
     */
    MotorControlCAN::Status requestEnable(const MotorControlCAN::LICHUAN_C::ReadContext& state,
                                          const MotorControlCAN::LICHUAN_C::EnableEvidence& evidence,
                                          const MotorControlCAN::LICHUAN_C::ActionOptions& options,
                                          uint64_t deadlineUs, uint64_t nowUs) noexcept;
    /** Withdraw an active enable locally: no further word or read is admitted. It sends no stop and
     * undoes nothing. A request in flight settles into the context on a later service, as evidence
     * only: it cannot complete the enable, which ends with this reason. The owner
     * gives this session no EMCY view, so an application that routes a drive fault reports it here.
     * @param[in] nowUs Nondecreasing supplied microseconds.
     * @param[in] reason CANCELLED, or DRIVE_FAULT for an independently routed checked fault.
     * @return False when no enable is active, the reason is another value or time moved backward.
     */
    bool cancelEnable(uint64_t nowUs,
                      MotorControlCAN::ActionOutcome reason = MotorControlCAN::ActionOutcome::CANCELLED) noexcept;
    /** Request the gated finite absolute move on this axis. No specimen has run it.
     * Starts a new owner operation, then prepares in place. Yields no traffic here: service() does.
     * The session supplies the reservation, its actual latest acknowledged controlword, which must be
     * 000F, and the operation identity; prepareMoveAbsolute refuses anything else.
     * @param[in] state Complete fresh SDO read under this reservation after that 000F, as
     * prepareMoveAbsolute requires.
     * @param[in] identity Current identity read on this route and node epoch.
     * @param[in] evidence Caller's capability evidence; a closed gate refuses with UNSUPPORTED.
     * @param[in] request Unwrapped absolute target and positive speed.
     * @param[in] profile Native ramps and policies.
     * @param[in] stored MOTION_PARAMETERS read for REUSE_STORED, or null. For REUSE_STORED it must be
     * one of the contexts named with retainSettings, so every move write retires it.
     * @param[in] axis The application's coordinate configuration of this axis. Borrowed until the move
     * is terminal: every parameter and trigger admission, and preflight() when it is wired, rechecks
     * the move against it, so keep it stationary and current, and a later configureAxis or
     * invalidateAxisReference reaches the move.
     * @param[in] reference Optional idle stationary witness, or null; borrowed like axis when supplied.
     * @param[in] options Copied budgets.
     * @param[in] deadlineUs Immutable exclusive cutoff of the whole move.
     * @param[in] nowUs Nondecreasing supplied microseconds.
     * @return prepareMoveAbsolute's status, or a local refusal with MoveError::STATE, TARGET or TIME,
     * which includes moveFenced() and clearBlocked(). On any refusal nothing changes, and on success
     * nothing has been admitted.
     */
    MotorControlCAN::Status requestMove(const MotorControlCAN::LICHUAN_C::ReadContext& state,
                                        const MotorControlCAN::LICHUAN_C::IdentityObservation& identity,
                                        const MotorControlCAN::LICHUAN_C::MoveEvidence& evidence,
                                        const MotorControlCAN::MoveRequest& request,
                                        const MotorControlCAN::LICHUAN_C::MoveProfile& profile,
                                        const MotorControlCAN::LICHUAN_C::DriverContext* stored,
                                        const MotorControlCAN::AxisConfig& axis,
                                        const MotorControlCAN::AxisReference* reference,
                                        const MotorControlCAN::LICHUAN_C::MoveOptions& options,
                                        uint64_t deadlineUs, uint64_t nowUs) noexcept;
    /** Request the gated finite relative move on this axis, through the same path as requestMove. No
     * relative base is measured on any specimen, so this refuses with UNSUPPORTED unless the caller's
     * evidence names one. Each call is one new explicitly admitted edge: identical values twice are two
     * moves with two witnesses, never a retry.
     * @param[in] state As requestMove.
     * @param[in] identity As requestMove.
     * @param[in] evidence As requestMove, with the measured relativeBase.
     * @param[in] request Unwrapped relative position whose basis is that base, and positive speed.
     * @param[in] profile As requestMove.
     * @param[in] stored As requestMove.
     * @param[in] axis As requestMove, borrowed until the move is terminal.
     * @param[in] reference Required host witness of the request's basis, observed after the latest
     * acknowledged 000F; borrowed until the move is terminal and rechecked at every parameter and
     * trigger admission. A witness from before an earlier move is refused.
     * @param[in] options As requestMove.
     * @param[in] deadlineUs As requestMove.
     * @param[in] nowUs As requestMove.
     * @return prepareMoveRelative's status, or a local refusal as requestMove's.
     */
    MotorControlCAN::Status requestMoveRelative(const MotorControlCAN::LICHUAN_C::ReadContext& state,
                                                const MotorControlCAN::LICHUAN_C::IdentityObservation& identity,
                                                const MotorControlCAN::LICHUAN_C::MoveEvidence& evidence,
                                                const MotorControlCAN::MoveRequest& request,
                                                const MotorControlCAN::LICHUAN_C::MoveProfile& profile,
                                                const MotorControlCAN::LICHUAN_C::DriverContext* stored,
                                                const MotorControlCAN::AxisConfig& axis,
                                                const MotorControlCAN::AxisReference& reference,
                                                const MotorControlCAN::LICHUAN_C::MoveOptions& options,
                                                uint64_t deadlineUs, uint64_t nowUs) noexcept;
    /** Withdraw an active move locally: no further download or read is admitted, and one the owner has
     * not yet handed to the controller is cancelled. It sends no stop, rearm or replay, so the drive
     * may still be moving. A request in flight, or one whose cancel the owner did not report as won,
     * settles into the context later as evidence only, and its terminal says whether it left. An
     * application that routes a drive fault reports it here.
     * @param[in] nowUs Nondecreasing supplied microseconds, at or after owner.lastTimeUs().
     * @param[in] reason CANCELLED, or DRIVE_FAULT for an independently routed checked fault.
     * @return False, with nothing changed, when no move is active, the reason is another value or the
     * time is behind this session's or the owner's last time.
     */
    bool cancelMove(uint64_t nowUs, MotorControlCAN::MoveInterruption reason =
                                        MotorControlCAN::MoveInterruption::CANCELLED) noexcept;
    /** Request one explicit alarm clear on this axis through prepareClearAlarm: 2301 U16 1, then 0
     * after the 1's acknowledgement. No specimen has run it. Starts a new owner operation, then prepares
     * clearAction() in place. Yields no traffic here: service() admits each write and read on this
     * reservation, as for an enable. After the 0's acknowledgement it reads MONITORS and STATUS until 2100
     * reports the alarm clear, then ERRORS once. An alarm that persists after both acknowledgements is
     * that result: it is repolled until the deadline and never written again. Nothing follows the clear:
     * no enable, controlword, mode, target, save or resume, and requestEnable stays refused because this
     * session has written. An observed clear is a drive report, not proof that the cause is gone.
     * @param[in] state Complete fresh SDO read of ERRORS, ERROR_HISTORY, MONITORS and STATUS under this
     * reservation, as prepareClearAlarm requires: 2100 alarm set and running clear, mode 1, raw 603F
     * FF05. Not held inside clearAction(). Its raw values stay in clearAction().incident, whose codeLink
     * is UNRESOLVED: the code does not identify the alarm.
     * @param[in] options Copied budgets, as prepareClearAlarm requires.
     * @param[in] deadlineUs Immutable exclusive cutoff of both writes and every observation.
     * @param[in] nowUs Nondecreasing supplied microseconds.
     * @return prepareClearAlarm's status, or a local refusal with ActionError::STATE (busy, clearBlocked()
     * or identities exhausted), TARGET (no usable reservation) or TIME. On such a refusal nothing
     * changes, and on success nothing has been admitted. If the owner then refuses the operation, which
     * only work admitted around this session can cause, the prepared context ends INVALIDATED with
     * nothing sent and STATE is returned. The owner keeps only the latest EMCY: an application that
     * needs the incident's EMCY copies it before this call, and routes a later one to cancelClear.
     */
    MotorControlCAN::Status requestClearAlarm(const MotorControlCAN::LICHUAN_C::ReadContext& state,
                                              const MotorControlCAN::LICHUAN_C::ActionOptions& options,
                                              uint64_t deadlineUs, uint64_t nowUs) noexcept;
    /** Request one explicit zero-only device counter clear on this axis through
     * prepareSetDevicePosition: 2302 U16 0, 1, 0, each after the previous acknowledgement, all on the
     * original binding. No specimen has run it. Starts a new owner operation, then prepares
     * clearAction() in place. Yields no traffic here: service() admits each write. When the 1 is
     * admitted, before any owner turn could carry it, the axis-aware notice withdraws axis's origins and
     * soft-limit qualification and reference's witness and advances both axis epochs, exactly once;
     * configured limit values and units stay. That withdrawal stays whatever follows, even when the 1
     * is then cancelled unsent. Once the last write is acknowledged and released, service() moves the
     * owner's reservation to the new configuration epoch with CanBusOwner::advanceConfiguration, then
     * reads MONITORS, STATUS and FEEDBACK on it until raw 6064 reads 0. After any other end once the 1
     * was admitted, it makes the same move once nothing of this session is outstanding; an unresolved
     * route never moves. That synchronization withdraws no coordinates: an application observer
     * receiving its CONFIGURATION notice finds the axis already advanced and must not advance it again.
     * A raw zero is no home, host origin or counter mapping, and no save, enable or resumed target
     * follows.
     * @param[in] state Complete fresh SDO read of MONITORS, STATUS and FEEDBACK under this reservation,
     * as prepareSetDevicePosition requires: mode 1, 2100 running and alarm clear, 606C raw 0. Not held
     * inside clearAction(). This is the library's stationarity evidence, not an observed shaft.
     * @param[in] nativePosition Requested native device position. Only exactly 0 is supported; any
     * other value refuses with UNSUPPORTED, unchanged, before any state, axis or budget check.
     * @param[in,out] axis The application's coordinate configuration of this axis, matching this
     * reservation's binding with room in both epochs. Borrowed until the clear is terminal: every write's
     * notice checks it, and the 1's notice withdraws it. Change nothing in it meanwhile, or the next
     * write's notice refuses, that write is cancelled and the clear ends INVALIDATED.
     * @param[in,out] reference The application's position witness of this axis, withdrawn with the axis;
     * one with nativeKnown false when it keeps none. Borrowed like axis.
     * @param[in] options Copied budgets; maximumAgeUs must cover seven fields.
     * @param[in] deadlineUs Immutable exclusive cutoff of the writes, the synchronization and every
     * observation.
     * @param[in] nowUs Nondecreasing supplied microseconds.
     * @return prepareSetDevicePosition's status, or a local refusal as requestClearAlarm's.
     */
    MotorControlCAN::Status requestCounterZero(const MotorControlCAN::LICHUAN_C::ReadContext& state,
                                               int64_t nativePosition, MotorControlCAN::AxisConfig& axis,
                                               MotorControlCAN::AxisReference& reference,
                                               const MotorControlCAN::LICHUAN_C::ActionOptions& options,
                                               uint64_t deadlineUs, uint64_t nowUs) noexcept;
    /** Withdraw an active alarm clear or counter clear locally: no further write or read is admitted, and
     * a write the owner has not yet handed to the controller is cancelled. It sends no stop and undoes
     * nothing: a coordinate withdrawal stays, and a 1 that may have landed without an acknowledged 0
     * leaves valueOneMayRemain and clearBlocked(). A request in flight, or one whose cancel the owner did
     * not report as won, settles into the context later as evidence only. The owner gives this session no
     * EMCY view, so an application that routes a drive fault, such as an EMCY during the clear, reports it
     * here; whether that EMCY is a new fault is its policy, and its copy stays with it.
     * @param[in] nowUs Nondecreasing supplied microseconds, at or after owner.lastTimeUs().
     * @param[in] reason CANCELLED, or DRIVE_FAULT for an independently routed checked fault.
     * @return False, with nothing changed, when no clear is active, the reason is another value or the
     * time is behind this session's or the owner's last time.
     */
    bool cancelClear(uint64_t nowUs,
                     MotorControlCAN::ActionOutcome reason = MotorControlCAN::ActionOutcome::CANCELLED) noexcept;
    /** The pre-transmit recheck of this session's move downloads. Call it from the application's
     * Transport::beforeTransmit, for the session that holds the sequence, and return its result. It
     * runs inside the owner's service turn, performs no owner call and keeps only a refusal reason.
     * For a parameter or trigger download of the active move it reruns the recheck its admission ran,
     * at this time: the borrowed axis and witness by validatePreparedTarget, an idle stationary witness
     * and the admission read's age. A download of a withdrawn move is refused too, so it never leaves
     * after the withdrawal even if its cancel did not win. The owner then closes the download with
     * CompletionReason::PREFLIGHT_REJECTED, provably unsent, and the move ends INVALIDATED, or with its
     * withdrawal's reason, and moveRefusal() keeps why. A write of a withdrawn clear is refused the same
     * way, and its clear ends with its withdrawal's reason.
     * @param[in] sequence Reservation of the work about to be handed over.
     * @param[in] transfer Its download, borrowed for this call.
     * @param[in] nowUs The owner turn's processing time.
     * @return False to withdraw the download; true for every other sequence, operation, the rearm,
     * a live clear's writes and uploads.
     */
    bool preflight(MotorControlCANOwner::SequenceId sequence,
                   const MotorControlCAN::CANOPEN::SdoTransfer& transfer, uint64_t nowUs) noexcept;
    /** @return Why the latest move's actual admission refused a download: the status
     * noteMoveTransmission gave at admission, or the same recheck in preflight() just before
     * transmission. Its detail is noteMoveTransmission's: an AxisError plus PARAMETER_ERROR_BASE and
     * PositionParameterError::AXIS_ERROR_BASE for a changed, aged or invalidated target or witness,
     * REFERENCE for a moving witness, STATE for an aged admission read. A transmit guard of the
     * application's own that withdrew the download gives STATE. OK when no recheck refused. A refused
     * recheck ends the move INVALIDATED, with nothing of that download sent. Reset when a move starts. */
    const MotorControlCAN::Status& moveRefusal() const noexcept { return moveRefusal_; }
    /** Name the retained settings contexts of this axis that a move's parameter writes must retire.
     * Each is borrowed until replaced and must outlive this session's moves. Include a finished
     * settings context's readback. A context not named here cannot be reached.
     * @param[in] contexts Up to four non-null contexts, or null with count zero.
     * @param[in] count Entries in contexts.
     * @return False, with nothing changed, while busy or for an invalid list.
     */
    bool retainSettings(MotorControlCAN::LICHUAN_C::DriverContext* const* contexts, uint8_t count) noexcept;
    /** @return Retained context of the latest move; EMPTY if never prepared. */
    const MotorControlCAN::LICHUAN_C::MoveContext& moveAction() const noexcept { return move_; }
    /** @return The move's last complete observation cycle, or an EMPTY read before the first one. A
     * valid requestStop state once fresh; never the cycle in progress. */
    const MotorControlCAN::LICHUAN_C::ReadContext& moveObservation() const noexcept {
        return moveReads_[moveComplete_];
    }
    /** @return True once a move's trigger may have reached the drive without an observed completion;
     * new moves are refused for this object's life. */
    bool moveFenced() const noexcept { return moveFenced_; }
    /** @return Retained context of the latest requestClearAlarm or requestCounterZero; EMPTY if neither
     * was prepared since open. The two share this one slot, so a new request of either kind replaces it:
     * actionKind is CLEAR_ALARM or SET_DEVICE_POSITION. commandHistory keeps every write's terminal,
     * failure included; execution describes the 1; valueOneMayRemain and uncertain record what may have
     * landed. A read held in it and passed to requestStop is judged after that clear has ended. */
    const MotorControlCAN::LICHUAN_C::ActionContext& clearAction() const noexcept { return clear_; }
    /** @return True once a clear ended with valueOneMayRemain: its 1 may have reached the drive and its 0
     * has no positive acknowledgement, so 2301 or 2302 may still hold 1, with undocumented effect.
     * Every later request but a stop is then refused for this object's life, across result release,
     * close and open; nothing here resends either value or can show that the object returned to 0. A new
     * object, as after an MCU restart, knows nothing, so the application's own record must carry it. */
    bool clearBlocked() const noexcept { return clearBlocked_; }
    /** @return The operation identity requestEnable would run under now, for EnableEvidence::operation;
     * zero without a usable reservation or with identities exhausted. */
    uint32_t nextOperation() const noexcept;
    /** Advance session work after an owner service turn; performs no owner service.
     * Also withdraws the latest controlword when its route, reservation or epochs change, and moves
     * the reservation to a counter clear's new configuration epoch once nothing of it is outstanding.
     * @param[in] nowUs Nondecreasing supplied microseconds.
     */
    void service(uint64_t nowUs) noexcept;
    /** @return Current stop path. */
    StopPath stopPath() const noexcept { return path_; }
    /** @param[in] kind Selection to inspect.
     * @return Retained context of that sequenced stop; EMPTY if never prepared.
     */
    const MotorControlCAN::LICHUAN_C::ActionContext& stopAction(MotorControlCAN::LICHUAN_C::StopKind kind) const noexcept {
        return stops_[static_cast<unsigned>(kind)];
    }
    /** @return Retained context of the latest enable; EMPTY if never prepared. Its read is a
     * valid state argument for requestStop once a cycle after the acknowledged 000F has finished. */
    const MotorControlCAN::LICHUAN_C::ActionContext& enableAction() const noexcept { return enable_; }
    /** @return True once this object has admitted a write that refuses a later enable. */
    bool motionHistoryWritten() const noexcept { return motionWritten_; }
    /** @return Retained context of the best-effort branch; EMPTY if never taken. */
    const MotorControlCAN::LICHUAN_C::BestEffortStop& bestEffort() const noexcept { return bestEffort_; }
    /** @return Earlier best-effort attempt a controller fault ended unsent; EMPTY if none. */
    const MotorControlCAN::LICHUAN_C::BestEffortStop& firstBestEffort() const noexcept {
        return firstBestEffort_;
    }
    /** @return Latest acknowledged controlword terminal, or null once withdrawn. */
    const MotorControlCAN::CANOPEN::SdoCompletion* latestControl() const noexcept {
        return controlPresent_ ? &control_ : nullptr;
    }

  private:
    MotorControlCANOwner::CanBusOwner& owner_;
    MotorControlCAN::LICHUAN_C::ReadTarget target_;
    MotorControlCAN::LICHUAN_C::AxisReservation reservation_;
    MotorControlCANOwner::SequenceId sequence_;
    MotorControlCANOwner::RequestId pending_, urgent_, superseded_;
    // The admitted download that withdrew a present control_; if it closes provably unsent, control_
    // is the latest again. Cleared by any other change of the latest word.
    MotorControlCANOwner::RequestId suspended_;
    MotorControlCAN::CANOPEN::SdoCompletion control_;
    MotorControlCAN::LICHUAN_C::ActionContext stops_[2];
    MotorControlCAN::LICHUAN_C::ActionContext enable_;
    MotorControlCAN::LICHUAN_C::BestEffortStop bestEffort_, firstBestEffort_;
    StopPolicy policy_;
    MotorControlCAN::LICHUAN_C::StopKind activeKind_ = MotorControlCAN::LICHUAN_C::StopKind::HALT;
    StopPath path_ = StopPath::NONE;
    uint64_t lastUs_ = 0;
    uint32_t operation_ = 0;
    uint32_t controllerEpoch_ = 0, epochLimit_ = 0;
    uint32_t serverSessions_ = 0;
    bool open_ = false, ownsReservation_ = false, controlPresent_ = false, recoveryUsed_ = false;
    bool motionWritten_ = false;
    uint32_t lastTrial_ = 0;
    MotorControlCAN::ActionOutcome enableWithdrawn_ = MotorControlCAN::ActionOutcome::NONE;
    MotorControlCAN::LICHUAN_C::MoveContext move_;
    // Observation cycles alternate between these, so the last complete one survives the next.
    MotorControlCAN::LICHUAN_C::ReadContext moveReads_[2];
    uint8_t moveRead_ = 0, moveComplete_ = 1;
    MotorControlCAN::MoveInterruption moveWithdrawn_ = MotorControlCAN::MoveInterruption::NONE;
    MotorControlCAN::Status moveRefusal_;
    bool moveFenced_ = false;
    // The application's axis and witness, borrowed for the active move's admission rechecks only.
    const MotorControlCAN::AxisConfig* moveAxis_ = nullptr;
    const MotorControlCAN::AxisReference* moveReference_ = nullptr;
    MotorControlCAN::LICHUAN_C::DriverContext* retained_[4] = {};
    uint8_t retainedCount_ = 0;
    // The latest alarm clear or counter clear. One slot for both: they exclude each other, and a
    // second ActionContext would cost its whole size again in every session.
    MotorControlCAN::LICHUAN_C::ActionContext clear_;
    // The clear's current target: its frozen reservation's, until a counter clear's 1 notice advances the
    // configuration epoch. It cannot be reservation_.target, which that notice must not overlap.
    MotorControlCAN::LICHUAN_C::ReadTarget clearTarget_;
    // The application's axis and witness, borrowed for a counter clear's notices only.
    MotorControlCAN::AxisConfig* clearAxis_ = nullptr;
    MotorControlCAN::AxisReference* clearReference_ = nullptr;
    MotorControlCAN::ActionOutcome clearWithdrawn_ = MotorControlCAN::ActionOutcome::NONE;
    bool clearBlocked_ = false;

    bool usable() const noexcept;
    uint64_t latestUs() const noexcept;
    void dropControl() noexcept;
    void keepUnsentControl(const MotorControlCAN::CANOPEN::SdoCompletion& terminal) noexcept;
    MotorControlCAN::Status recheckMove(uint64_t nowUs) const noexcept;
    MotorControlCANOwner::BusAdmission admitWork(const MotorControlCAN::CANOPEN::SdoTransfer& transfer,
                                                 uint64_t nowUs) noexcept;
    bool takeTerminal(MotorControlCAN::CANOPEN::SdoCompletion& output) noexcept;
    void withdrawEnable(MotorControlCAN::ActionOutcome reason, uint64_t nowUs) noexcept;
    void serviceEnable(uint64_t nowUs) noexcept;
    MotorControlCAN::Status startMove(bool relative, const MotorControlCAN::LICHUAN_C::ReadContext& state,
                                      const MotorControlCAN::LICHUAN_C::IdentityObservation& identity,
                                      const MotorControlCAN::LICHUAN_C::MoveEvidence& evidence,
                                      const MotorControlCAN::MoveRequest& request,
                                      const MotorControlCAN::LICHUAN_C::MoveProfile& profile,
                                      const MotorControlCAN::LICHUAN_C::DriverContext* stored,
                                      const MotorControlCAN::AxisConfig& axis,
                                      const MotorControlCAN::AxisReference* reference,
                                      const MotorControlCAN::LICHUAN_C::MoveOptions& options,
                                      uint64_t deadlineUs, uint64_t nowUs) noexcept;
    void withdrawMove(MotorControlCAN::MoveInterruption reason, uint64_t nowUs) noexcept;
    void endMove(MotorControlCAN::MoveInterruption reason, uint64_t nowUs) noexcept;
    void afterMove() noexcept;
    void serviceMove(uint64_t nowUs) noexcept;
    MotorControlCAN::Status startClear(const MotorControlCAN::LICHUAN_C::ReadContext& state,
                                       int64_t nativePosition, MotorControlCAN::AxisConfig* axis,
                                       MotorControlCAN::AxisReference* reference,
                                       const MotorControlCAN::LICHUAN_C::ActionOptions& options,
                                       uint64_t deadlineUs, uint64_t nowUs) noexcept;
    void withdrawClear(MotorControlCAN::ActionOutcome reason, uint64_t nowUs) noexcept;
    void endClear(MotorControlCAN::ActionOutcome reason, uint64_t nowUs) noexcept;
    void afterClear() noexcept;
    bool syncCounter(uint64_t nowUs) noexcept;
    void serviceClear(uint64_t nowUs) noexcept;
    bool stateRefuses(const MotorControlCAN::LICHUAN_C::ReadContext& state, const StopPolicy& policy,
                      uint64_t nowUs) const noexcept;
    bool freshlyIdle(const MotorControlCAN::LICHUAN_C::ReadContext& state, const StopPolicy& policy,
                     uint64_t nowUs) const noexcept;
    bool controllerMoved(MotorControlCAN::Generation before) const noexcept;
    void settleSuperseded(uint64_t nowUs) noexcept;
    bool prepareHeld(uint64_t nowUs, uint64_t deadlineUs) noexcept;
    void withdrawIfStale() noexcept;
    void serviceConfirmed(uint64_t nowUs) noexcept;
    void serviceBestEffort(uint64_t nowUs) noexcept;
    void serviceHeld(uint64_t nowUs) noexcept;
    void afterConfirmedEnd(uint64_t nowUs) noexcept;
    bool startBestEffort(uint64_t nowUs) noexcept;
    void hold(uint64_t nowUs, uint64_t deadlineUs) noexcept;
    void endHeld(MotorControlCAN::ActionOutcome reason, uint64_t nowUs) noexcept;
};
} // namespace MotorControlCANExample
