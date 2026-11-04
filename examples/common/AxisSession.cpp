// SPDX-License-Identifier: MIT
#include "AxisSession.h"
#include <MotorControlCAN/profiles/lichuan_c/State.h>

namespace MotorControlCANExample {
using MotorControlCANOwner::BusAdmission;
using MotorControlCANOwner::BusRequest;
using MotorControlCANOwner::Completion;
using MotorControlCANOwner::CompletionReason;
using MotorControlCANOwner::RequestId;
using MotorControlCANOwner::Sequence;
using MotorControlCANOwner::SequenceId;
namespace M = MotorControlCAN;
namespace C = MotorControlCAN::CANOPEN;
namespace L = MotorControlCAN::LICHUAN_C;
namespace {
bool isControlWrite(const C::SdoTransfer& t) {
    return t.direction == C::SdoDirection::DOWNLOAD && t.index == 0x6040 && !t.subindex && t.size == 2;
}
uint16_t word(const C::SdoTransfer& t) {
    return uint16_t(t.data[0] | (uint16_t(t.data[1]) << 8));
}
// The one write that leaves an enable admissible: selecting mode 1 is the manuals' first step.
bool isModeOneWrite(const C::SdoTransfer& t) {
    return t.direction == C::SdoDirection::DOWNLOAD && t.index == 0x6060 && !t.subindex &&
           t.size == 1 && t.data[0] == 1;
}
bool sameRequest(RequestId a, RequestId b) {
    return a.owner == b.owner && a.slot == b.slot && a.generation == b.generation;
}
bool sameSequence(SequenceId a, SequenceId b) {
    return a.owner == b.owner && a.slot == b.slot && a.generation == b.generation;
}
const char* const CLEAR_BLOCKED = "an earlier clear's value 1 may still be held; only a stop is admitted";
} // namespace

// The owner refuses a time before its own last one, so a session call at such a time would fail part
// way through, after this session had already changed.
uint64_t AxisSession::latestUs() const noexcept {
    const uint64_t owner = owner_.lastTimeUs();
    return owner > lastUs_ ? owner : lastUs_;
}
void AxisSession::dropControl() noexcept {
    controlPresent_ = false;
    suspended_ = RequestId();
}
// A download that closed with no frame able to have left changed nothing on the drive, so the word
// it withdrew is the latest again, unless a stop has taken over since.
void AxisSession::keepUnsentControl(const C::SdoCompletion& terminal) noexcept {
    if (!suspended_.owner || !sameRequest(suspended_, pending_))
        return;
    suspended_ = RequestId();
    if (!terminal.transmissionPossible && !stopActive()) {
        controlPresent_ = true;
        withdrawIfStale();
    }
}

bool AxisSession::open(const L::ReadTarget& target, L::SettingsModel model, uint8_t producer,
                       uint64_t now) noexcept {
    if (open_ || now < latestUs() || !target.nodeGeneration ||
        !C::validateSdoChannel(target.channel))
        return false;
    M::Generation node = 0;
    uint32_t sessions = 0;
    if (!owner_.nodeEpochs(target.channel.binding, node, sessions) || node != target.nodeGeneration)
        return false;
    // A route with no usable reservation still opens: only the best-effort branch needs none.
    SequenceId held;
    const bool adopted = owner_.findSequence(target.channel.binding, held);
    const bool taken = !adopted && owner_.beginSequence(target.channel.binding, producer, now, held);
    const Sequence* s = adopted || taken ? owner_.sequence(held) : nullptr;
    if (s && s->nodeGeneration != target.nodeGeneration) {
        if (taken)
            owner_.releaseSequence(held);
        return false;
    }
    target_ = target;
    sequence_ = s ? held : SequenceId();
    reservation_ = L::AxisReservation();
    reservation_.target = target;
    reservation_.model = model;
    if (s) {
        reservation_.owner = owner_.identity();
        reservation_.sequence = uint32_t(held.slot) + 1;
        reservation_.generation = held.generation;
        reservation_.acquiredUs = s->acquiredUs;
    }
    serverSessions_ = sessions;
    controllerEpoch_ = owner_.controllerEpoch();
    pending_ = urgent_ = superseded_ = RequestId();
    dropControl();
    recoveryUsed_ = false;
    L::resetAction(stops_[0]);
    L::resetAction(stops_[1]);
    L::resetAction(enable_);
    enableWithdrawn_ = M::ActionOutcome::NONE;
    // moveFenced_ stays: reopening cannot show that an earlier move's motion ended.
    L::resetMove(move_);
    L::resetRead(moveReads_[0]);
    L::resetRead(moveReads_[1]);
    moveRead_ = 0;
    moveComplete_ = 1;
    moveWithdrawn_ = M::MoveInterruption::NONE;
    moveRefusal_ = M::Status();
    moveAxis_ = nullptr;
    moveReference_ = nullptr;
    // clearBlocked_ stays: reopening cannot show that the drive's 2301 or 2302 returned to 0.
    L::resetAction(clear_);
    clearTarget_ = target;
    clearAxis_ = nullptr;
    clearReference_ = nullptr;
    clearWithdrawn_ = M::ActionOutcome::NONE;
    bestEffort_ = firstBestEffort_ = L::BestEffortStop();
    path_ = StopPath::NONE;
    ownsReservation_ = taken;
    open_ = true;
    lastUs_ = now;
    return true;
}
bool AxisSession::close() noexcept {
    if (!open_ || busy() || (ownsReservation_ && !owner_.releaseSequence(sequence_)))
        return false;
    open_ = false;
    dropControl();
    path_ = StopPath::NONE;
    return true;
}
bool AxisSession::usable() const noexcept {
    const Sequence* s = open_ && sequence_.owner ? owner_.sequence(sequence_) : nullptr;
    return s && s->nodeGeneration == target_.nodeGeneration &&
           M::sameBinding(s->binding, target_.channel.binding);
}
void AxisSession::withdrawIfStale() noexcept {
    if ((controlPresent_ || suspended_.owner) &&
        (!usable() || owner_.controllerEpoch() != controllerEpoch_))
        dropControl();
}
uint32_t AxisSession::beginOperation(uint64_t deadline, uint64_t now) noexcept {
    if (now < latestUs() || !usable() || busy())
        return 0;
    const uint32_t current = owner_.sequence(sequence_)->operation;
    const uint32_t next = current > operation_ ? current : operation_;
    if (next == UINT32_MAX || !owner_.beginOperation(sequence_, next + 1, deadline, now))
        return 0;
    operation_ = next + 1;
    lastUs_ = now;
    return operation_;
}
BusAdmission AxisSession::admitPrepared(const C::SdoTransfer& transfer, uint64_t now) noexcept {
    if (now < latestUs() || !open_ || busy())
        return BusAdmission::INVALID;
    return admitWork(transfer, now);
}
BusAdmission AxisSession::admitWork(const C::SdoTransfer& transfer, uint64_t now) noexcept {
    BusRequest work;
    work.sequence = sequence_;
    work.transfer = transfer;
    RequestId id;
    const BusAdmission admission = owner_.admit(work, now, id);
    if (admission == BusAdmission::ACCEPTED) {
        pending_ = id;
        lastUs_ = now;
        if (transfer.direction == C::SdoDirection::DOWNLOAD) {
            const bool present = controlPresent_;
            dropControl();
            if (present)
                suspended_ = id; // Its terminal decides whether control_ is the latest again.
            motionWritten_ |= !isModeOneWrite(transfer);
        }
    }
    return admission;
}
bool AxisSession::settle(C::SdoCompletion& output) noexcept {
    return !enableActive() && !moveActive() && !clearActive() && takeTerminal(output);
}
bool AxisSession::takeTerminal(C::SdoCompletion& output) noexcept {
    const Completion* terminal = owner_.result(pending_);
    if (!terminal)
        return false;
    const C::SdoCompletion copy = terminal->service;
    if (!owner_.release(pending_))
        return false;
    keepUnsentControl(copy);
    pending_ = RequestId();
    const Sequence* s = owner_.sequence(sequence_);
    // A write settling after a stop was admitted is older intent, not the latest word.
    if (copy.outcome == C::SdoOutcome::SUCCESS && isControlWrite(copy.transfer) && s &&
        s->operation == copy.transfer.operation && !stopActive()) {
        control_ = copy;
        controlPresent_ = true;
        suspended_ = RequestId();
        controllerEpoch_ = owner_.controllerEpoch();
    }
    // A 6060=1 that may have reached the drive unacknowledged leaves its effect unknown.
    motionWritten_ |= isModeOneWrite(copy.transfer) && copy.outcome != C::SdoOutcome::SUCCESS &&
                      copy.effectPossible;
    output = copy;
    return true;
}
M::Status AxisSession::requestEnable(const L::ReadContext& state, const L::EnableEvidence& evidence,
                                     const L::ActionOptions& options, uint64_t deadline,
                                     uint64_t now) noexcept {
    const int32_t stateError = static_cast<int32_t>(M::ActionError::STATE);
    if (now < latestUs())
        return M::Status(M::Err::INVALID_CONFIG, static_cast<int32_t>(M::ActionError::TIME),
                         "time is behind this session's or the owner's last time");
    if (!open_ || !usable())
        return M::Status(M::Err::INVALID_CONFIG, static_cast<int32_t>(M::ActionError::TARGET),
                         "no usable reservation for an enable");
    if (busy())
        return M::Status(M::Err::INVALID_CONFIG, stateError, "axis is busy");
    if (clearBlocked_)
        return M::Status(M::Err::INVALID_CONFIG, stateError, CLEAR_BLOCKED);
    if (motionWritten_)
        return M::Status(M::Err::INVALID_CONFIG, stateError,
                         "this session wrote to the drive; no fresh enable history remains");
    const uint32_t next = nextOperation();
    if (!next)
        return M::Status(M::Err::INVALID_CONFIG, stateError, "operation identities exhausted");
    const uint32_t last = next - 1;
    // Check everything before the owner moves; the context is written only once that succeeded.
    const M::Status checked =
        L::validateEnable(enable_, reservation_, state, evidence, last + 1, now, deadline, options);
    if (!checked)
        return checked;
    // After the library's checks, so that a closed gate or a zero trial reports as such.
    if (evidence.trial <= lastTrial_)
        return M::Status(M::Err::INVALID_CONFIG, stateError,
                         "this session has already consumed that trial identity");
    if (!owner_.beginOperation(sequence_, last + 1, deadline, now))
        return M::Status(M::Err::INVALID_CONFIG, stateError, "owner refused a new operation");
    // Same values as the check above, and only the owner moved, so this cannot refuse.
    L::prepareEnable(enable_, reservation_, state, evidence, last + 1, now, deadline, options);
    operation_ = last + 1;
    lastTrial_ = evidence.trial;
    enableWithdrawn_ = M::ActionOutcome::NONE;
    lastUs_ = now;
    return M::Ok();
}
uint32_t AxisSession::nextOperation() const noexcept {
    if (!usable())
        return 0;
    const uint32_t current = owner_.sequence(sequence_)->operation;
    const uint32_t last = current > operation_ ? current : operation_;
    return last == UINT32_MAX ? 0 : last + 1;
}
bool AxisSession::cancelEnable(uint64_t now, M::ActionOutcome reason) noexcept {
    if (now < latestUs() || !enableActive() ||
        (reason != M::ActionOutcome::CANCELLED && reason != M::ActionOutcome::DRIVE_FAULT))
        return false;
    lastUs_ = now;
    withdrawEnable(reason, now);
    return true;
}
// A request in flight keeps the slot and settles into this context, so that its evidence survives.
// The first reason wins. With nothing in flight the enable ends at once. Otherwise the library
// records the withdrawal, so that the terminal is consumed as evidence and cannot complete it.
void AxisSession::withdrawEnable(M::ActionOutcome reason, uint64_t now) noexcept {
    if (!enableActive() || enableWithdrawn_ != M::ActionOutcome::NONE)
        return;
    enableWithdrawn_ = reason;
    if (!pending_.owner)
        L::advanceAction(enable_, reservation_, reservation_.target, reason, now);
    else if (!L::noteActionWithdrawal(enable_, reservation_, reservation_.target, reason, now).isOk())
        L::noteActionWithdrawal(enable_, reservation_, reservation_.target,
                                M::ActionOutcome::INVALIDATED, now);
}
void AxisSession::serviceEnable(uint64_t now) noexcept {
    if (!enableActive())
        return;
    // As for a stop: the frozen reservation identifies this action, and real currentness is checked
    // below, so that a terminal is consumed with its evidence even after its route has gone.
    const L::ReadTarget& frozen = reservation_.target;
    if (pending_.owner) {
        C::SdoCompletion evidence;
        if (!takeTerminal(evidence))
            return;
        if (!L::advanceAction(enable_, reservation_, frozen, evidence, now).isOk())
            L::advanceAction(enable_, reservation_, frozen,
                             enableWithdrawn_ != M::ActionOutcome::NONE ? enableWithdrawn_
                                                                        : M::ActionOutcome::INVALIDATED,
                             now);
        if (!enableActive())
            return;
    }
    if (enableWithdrawn_ != M::ActionOutcome::NONE || now >= enable_.deadlineUs || !usable()) {
        L::advanceAction(enable_, reservation_, frozen,
                         now >= enable_.deadlineUs                 ? M::ActionOutcome::DEADLINE
                         : enableWithdrawn_ != M::ActionOutcome::NONE ? enableWithdrawn_
                                                                   : M::ActionOutcome::INVALIDATED,
                         now);
        return;
    }
    L::PreparedAction work;
    if (!L::nextAction(enable_, reservation_, frozen, now, work).isOk()) {
        L::advanceAction(enable_, reservation_, frozen, M::ActionOutcome::INVALIDATED, now);
        return;
    }
    if (!work.ready)
        return;
    const BusAdmission admission = admitWork(work.transfer, now);
    if (admission == BusAdmission::RESULTS_FULL)
        return; // Unsent, so the same work is simply offered again.
    if (admission != BusAdmission::ACCEPTED) {
        L::advanceAction(enable_, reservation_, frozen, M::ActionOutcome::INVALIDATED, now);
        return;
    }
    // As for a stop: nothing leaves before the next owner turn, so the notice is made here, and the
    // terminal refines it.
    if (enable_.phase == L::ActionPhase::COMMAND)
        L::noteActionTransmission(enable_, reservation_, frozen, now);
}
M::Status AxisSession::requestMove(const L::ReadContext& state, const L::IdentityObservation& identity,
                                   const L::MoveEvidence& evidence, const M::MoveRequest& request,
                                   const L::MoveProfile& profile, const L::DriverContext* stored,
                                   const M::AxisConfig& axis, const M::AxisReference* reference,
                                   const L::MoveOptions& options, uint64_t deadline,
                                   uint64_t now) noexcept {
    return startMove(false, state, identity, evidence, request, profile, stored, axis, reference,
                     options, deadline, now);
}
M::Status AxisSession::requestMoveRelative(const L::ReadContext& state,
                                           const L::IdentityObservation& identity,
                                           const L::MoveEvidence& evidence,
                                           const M::MoveRequest& request,
                                           const L::MoveProfile& profile,
                                           const L::DriverContext* stored, const M::AxisConfig& axis,
                                           const M::AxisReference& reference,
                                           const L::MoveOptions& options, uint64_t deadline,
                                           uint64_t now) noexcept {
    return startMove(true, state, identity, evidence, request, profile, stored, axis, &reference,
                     options, deadline, now);
}
// One request path for both moves; only the library call differs, and a relative witness is never
// null here.
M::Status AxisSession::startMove(bool relative, const L::ReadContext& state,
                                 const L::IdentityObservation& identity,
                                 const L::MoveEvidence& evidence, const M::MoveRequest& request,
                                 const L::MoveProfile& profile, const L::DriverContext* stored,
                                 const M::AxisConfig& axis, const M::AxisReference* reference,
                                 const L::MoveOptions& options, uint64_t deadline,
                                 uint64_t now) noexcept {
    const int32_t stateError = static_cast<int32_t>(M::MoveError::STATE);
    if (now < latestUs())
        return M::Status(M::Err::INVALID_CONFIG, static_cast<int32_t>(M::MoveError::TIME),
                         "time is behind this session's or the owner's last time");
    if (!open_ || !usable())
        return M::Status(M::Err::INVALID_CONFIG, static_cast<int32_t>(M::MoveError::TARGET),
                         "no usable reservation for a move");
    if (busy())
        return M::Status(M::Err::INVALID_CONFIG, stateError, "axis is busy");
    if (moveFenced_)
        return M::Status(M::Err::INVALID_CONFIG, stateError,
                         "an earlier trigger may still be moving the drive: new moves stay fenced");
    if (clearBlocked_)
        return M::Status(M::Err::INVALID_CONFIG, stateError, CLEAR_BLOCKED);
    withdrawIfStale();
    if (!controlPresent_)
        return M::Status(M::Err::INVALID_CONFIG, stateError, "no latest acknowledged controlword");
    // A reused read must be one this session retires on every move write, or a later reuse could
    // skip a value an earlier move changed.
    bool retained = false;
    for (unsigned i = 0; i < retainedCount_; ++i)
        retained |= stored && retained_[i] == stored;
    if (profile.setup == L::MoveSetup::REUSE_STORED && !retained)
        return M::Status(M::Err::INVALID_CONFIG, stateError,
                         "a reused profile read must be named with retainSettings");
    const uint32_t next = nextOperation();
    if (!next)
        return M::Status(M::Err::INVALID_CONFIG, stateError, "operation identities exhausted");
    // Check everything before the owner moves; the context is written only once that succeeded.
    const M::Status checked =
        relative ? L::validateMoveRelative(move_, reservation_, target_, state, control_, identity,
                                           evidence, request, profile, stored, axis, *reference, next,
                                           now, deadline, options)
                 : L::validateMoveAbsolute(move_, reservation_, target_, state, control_, identity,
                                           evidence, request, profile, stored, axis, reference, next,
                                           now, deadline, options);
    if (!checked)
        return checked;
    if (!owner_.beginOperation(sequence_, next, deadline, now))
        return M::Status(M::Err::INVALID_CONFIG, stateError, "owner refused a new operation");
    // Same values as the check above, and only the owner moved, so this cannot refuse.
    if (relative)
        L::prepareMoveRelative(move_, reservation_, target_, state, control_, identity, evidence,
                               request, profile, stored, axis, *reference, next, now, deadline,
                               options);
    else
        L::prepareMoveAbsolute(move_, reservation_, target_, state, control_, identity, evidence,
                               request, profile, stored, axis, reference, next, now, deadline,
                               options);
    L::resetRead(moveReads_[0]);
    L::resetRead(moveReads_[1]);
    moveRead_ = 0;
    moveComplete_ = 1;
    moveWithdrawn_ = M::MoveInterruption::NONE;
    moveRefusal_ = M::Status();
    moveAxis_ = &axis;
    moveReference_ = reference;
    operation_ = next;
    lastUs_ = now;
    return M::Ok();
}
bool AxisSession::cancelMove(uint64_t now, M::MoveInterruption reason) noexcept {
    if (now < latestUs() || !moveActive() ||
        (reason != M::MoveInterruption::CANCELLED && reason != M::MoveInterruption::DRIVE_FAULT))
        return false;
    lastUs_ = now;
    withdrawMove(reason, now);
    return true;
}
bool AxisSession::retainSettings(L::DriverContext* const* contexts, uint8_t count) noexcept {
    if (busy() || count > 4 || (count && !contexts))
        return false;
    for (unsigned i = 0; i < count; ++i)
        if (!contexts[i])
            return false;
    for (unsigned i = 0; i < 4; ++i)
        retained_[i] = i < count ? contexts[i] : nullptr;
    retainedCount_ = count;
    return true;
}
// A trigger that may have reached the drive with no observed completion leaves motion unknown, and
// nothing in this session can show it ended: a stop reports the controller, not the shaft. A server
// abort counts too: the frame reached the drive, and an abort does not establish that it had no effect.
void AxisSession::afterMove() noexcept {
    if (move_.execution != M::ActionExecution::NOT_TRANSMITTED &&
        move_.completion != M::ActionCompletion::OBSERVED)
        moveFenced_ = true;
    moveAxis_ = nullptr; // The borrow ends with the move.
    moveReference_ = nullptr;
}
// Nothing in flight: end it now. The move withdraws its admitted cycle, which no owner turn has
// carried yet, and keeps every earlier terminal.
void AxisSession::endMove(M::MoveInterruption reason, uint64_t now) noexcept {
    const L::ReadTarget& frozen = reservation_.target;
    L::ReadContext& read = moveReads_[moveRead_];
    if (!L::advanceMove(move_, read, reservation_, frozen, reason, now).isOk())
        L::advanceMove(move_, read, reservation_, frozen, M::MoveInterruption::INVALIDATED, now);
    afterMove();
}
// As for an enable: the first reason wins, and a request in flight keeps the slot and settles into
// this context as evidence, so that it cannot complete the move. Unlike an enable, work the owner has
// not yet handed to the controller is cancelled here: a queued parameter or trigger must not leave
// after the caller withdrew the move, even when the stop that withdrew it is then refused. Only its
// terminal says what happened. One that shows no frame could have left also leaves the controlword
// before it as the latest. Without a terminal, as after a cancel the owner refused, nothing is assumed:
// the work settles into the context later with its own evidence, and preflight() refuses it should it
// come up for transmission. Work that may have left is never cancelled, because that could quarantine
// the route.
void AxisSession::withdrawMove(M::MoveInterruption reason, uint64_t now) noexcept {
    if (!moveActive() || moveWithdrawn_ != M::MoveInterruption::NONE)
        return;
    moveWithdrawn_ = reason;
    if (pending_.owner && !owner_.result(pending_) && !owner_.mayHaveTransmitted(pending_))
        owner_.cancel(pending_, now);
    if (const Completion* closed = pending_.owner ? owner_.result(pending_) : nullptr)
        keepUnsentControl(closed->service);
    if (!pending_.owner)
        endMove(reason, now);
    else if (!L::noteMoveWithdrawal(move_, reservation_, reservation_.target, reason, now).isOk())
        L::noteMoveWithdrawal(move_, reservation_, reservation_.target,
                              M::MoveInterruption::INVALIDATED, now);
}
void AxisSession::serviceMove(uint64_t now) noexcept {
    if (!moveActive())
        return;
    // As for a stop: the frozen reservation identifies this move, and real currentness is checked
    // below, so that a terminal is consumed with its evidence even after its route has gone.
    const L::ReadTarget& frozen = reservation_.target;
    if (pending_.owner) {
        const Completion* closed = owner_.result(pending_);
        const bool guarded = closed && closed->reason == CompletionReason::PREFLIGHT_REJECTED;
        C::SdoCompletion evidence;
        if (!takeTerminal(evidence))
            return;
        // This session cancels move work only after recording a withdrawal, and preflight() refuses a
        // live move's download only with a reason. So a closure without a withdrawal is the owner
        // closing a withdrawn reservation's work, or a transmit guard: preflight's recheck or the
        // application's own. Record that actual reason first: the terminal is then kept as evidence,
        // and the move does not report a cancel it never made. A lost reply that quarantines the route
        // keeps its own outcome.
        if ((guarded || evidence.outcome == C::SdoOutcome::CANCELLED) &&
            moveWithdrawn_ == M::MoveInterruption::NONE) {
            if (guarded && moveRefusal_.isOk())
                moveRefusal_ = M::Status(M::Err::INVALID_CONFIG, static_cast<int32_t>(M::MoveError::STATE),
                                         "the application's transmit guard withdrew the move's download");
            moveWithdrawn_ = M::MoveInterruption::INVALIDATED;
            L::noteMoveWithdrawal(move_, reservation_, frozen, M::MoveInterruption::INVALIDATED, now);
        }
        L::ReadContext& read = moveReads_[moveRead_];
        if (!L::advanceMove(move_, read, reservation_, frozen, evidence, now).isOk())
            L::advanceMove(move_, read, reservation_, frozen,
                           moveWithdrawn_ != M::MoveInterruption::NONE ? moveWithdrawn_
                                                                       : M::MoveInterruption::INVALIDATED,
                           now);
        if (read.state == M::ReadState::SUCCEEDED && read.operation == move_.operation &&
            read.startedUs == move_.observationStartedUs)
            moveComplete_ = moveRead_;
        if (!moveActive()) {
            afterMove();
            return;
        }
    }
    if (moveWithdrawn_ != M::MoveInterruption::NONE || now >= move_.deadlineUs || !usable()) {
        endMove(now >= move_.deadlineUs                     ? M::MoveInterruption::DEADLINE
                : moveWithdrawn_ != M::MoveInterruption::NONE ? moveWithdrawn_
                                                              : M::MoveInterruption::INVALIDATED,
                now);
        return;
    }
    L::PreparedMove work;
    if (!L::nextMove(move_, moveReads_[moveRead_], reservation_, frozen, now, work).isOk()) {
        endMove(M::MoveInterruption::INVALIDATED, now);
        return;
    }
    if (work.observe) {
        // The other read keeps the last complete cycle for a stop request.
        const uint8_t slot = uint8_t(1u - moveComplete_);
        if (!L::beginMoveObservation(move_, moveReads_[slot], reservation_, frozen, now).isOk() ||
            !L::nextMove(move_, moveReads_[slot], reservation_, frozen, now, work).isOk()) {
            endMove(M::MoveInterruption::INVALIDATED, now);
            return;
        }
        moveRead_ = slot;
    }
    if (!work.ready)
        return;
    const BusAdmission admission = admitWork(work.transfer, now);
    if (admission == BusAdmission::RESULTS_FULL)
        return; // Unsent, so the same work is simply offered again.
    if (admission != BusAdmission::ACCEPTED) {
        endMove(M::MoveInterruption::INVALIDATED, now);
        return;
    }
    if (work.transfer.direction != C::SdoDirection::DOWNLOAD)
        return;
    // Nothing leaves before the next owner turn, which may hand the frame over. So the notice, and the
    // retirement of every retained setting it performs, happen here. The notice is also the move's
    // actual admission: it rechecks the borrowed axis and witness, so a later configureAxis, origin
    // change, reference invalidation or aged witness stops this write. preflight() reruns that recheck
    // at the handover when the application wires it. If the notice refuses, withdrawing the move
    // cancels the download before any turn could carry it.
    const M::Status noted = L::noteMoveTransmission(move_, reservation_, frozen, *moveAxis_, moveReference_,
                                                    retained_, retainedCount_, now);
    if (!noted.isOk()) {
        moveRefusal_ = noted;
        withdrawMove(M::MoveInterruption::INVALIDATED, now);
    }
}
M::Status AxisSession::requestClearAlarm(const L::ReadContext& state, const L::ActionOptions& options,
                                         uint64_t deadline, uint64_t now) noexcept {
    return startClear(state, 0, nullptr, nullptr, options, deadline, now);
}
M::Status AxisSession::requestCounterZero(const L::ReadContext& state, int64_t nativePosition,
                                          M::AxisConfig& axis, M::AxisReference& reference,
                                          const L::ActionOptions& options, uint64_t deadline,
                                          uint64_t now) noexcept {
    return startClear(state, nativePosition, &axis, &reference, options, deadline, now);
}
// One request path for both clears; a null axis is the alarm clear.
M::Status AxisSession::startClear(const L::ReadContext& state, int64_t nativePosition, M::AxisConfig* axis,
                                  M::AxisReference* reference, const L::ActionOptions& options,
                                  uint64_t deadline, uint64_t now) noexcept {
    const int32_t stateError = static_cast<int32_t>(M::ActionError::STATE);
    if (now < latestUs())
        return M::Status(M::Err::INVALID_CONFIG, static_cast<int32_t>(M::ActionError::TIME),
                         "time is behind this session's or the owner's last time");
    if (!open_ || !usable())
        return M::Status(M::Err::INVALID_CONFIG, static_cast<int32_t>(M::ActionError::TARGET),
                         "no usable reservation for a clear");
    if (busy())
        return M::Status(M::Err::INVALID_CONFIG, stateError, "axis is busy");
    if (clearBlocked_)
        return M::Status(M::Err::INVALID_CONFIG, stateError, CLEAR_BLOCKED);
    const uint32_t next = nextOperation();
    if (!next)
        return M::Status(M::Err::INVALID_CONFIG, stateError, "operation identities exhausted");
    // The library has no separate check for these two, but it validates everything before it writes
    // the context, so a refusal here changes nothing. The owner moves only after that.
    const M::Status prepared =
        axis ? L::prepareSetDevicePosition(clear_, reservation_, state, nativePosition, *axis, next, now,
                                           deadline, options)
             : L::prepareClearAlarm(clear_, reservation_, state, next, now, deadline, options);
    if (!prepared)
        return prepared;
    clearTarget_ = reservation_.target;
    clearWithdrawn_ = M::ActionOutcome::NONE;
    // Checked above but for work admitted around this session, which the class contract excludes.
    if (!owner_.beginOperation(sequence_, next, deadline, now)) {
        L::advanceAction(clear_, reservation_, clearTarget_, M::ActionOutcome::INVALIDATED, now);
        return M::Status(M::Err::INVALID_CONFIG, stateError, "owner refused a new operation");
    }
    clearAxis_ = axis;
    clearReference_ = reference;
    operation_ = next;
    lastUs_ = now;
    return M::Ok();
}
bool AxisSession::cancelClear(uint64_t now, M::ActionOutcome reason) noexcept {
    if (now < latestUs() || !clearActive() ||
        (reason != M::ActionOutcome::CANCELLED && reason != M::ActionOutcome::DRIVE_FAULT))
        return false;
    lastUs_ = now;
    withdrawClear(reason, now);
    return true;
}
// As for a move: the first reason wins, a write the owner has not yet handed to the controller is
// cancelled so that it cannot leave after the withdrawal, and a request in flight keeps the slot and
// settles into the context as evidence. Work that may have left is never cancelled, because that could
// quarantine the route. A 1 already sent is never followed by its 0 here: that would be a new write after
// the withdrawal, and valueOneMayRemain then records what may be held.
void AxisSession::withdrawClear(M::ActionOutcome reason, uint64_t now) noexcept {
    if (!clearActive() || clearWithdrawn_ != M::ActionOutcome::NONE)
        return;
    clearWithdrawn_ = reason;
    if (pending_.owner && !owner_.result(pending_) && !owner_.mayHaveTransmitted(pending_))
        owner_.cancel(pending_, now);
    if (const Completion* closed = pending_.owner ? owner_.result(pending_) : nullptr)
        keepUnsentControl(closed->service);
    if (!pending_.owner)
        endClear(reason, now);
    else if (!L::noteActionWithdrawal(clear_, reservation_, clearTarget_, reason, now).isOk())
        L::noteActionWithdrawal(clear_, reservation_, clearTarget_, M::ActionOutcome::INVALIDATED, now);
}
// Nothing in flight: end it now, keeping every earlier terminal.
void AxisSession::endClear(M::ActionOutcome reason, uint64_t now) noexcept {
    if (!L::advanceAction(clear_, reservation_, clearTarget_, reason, now).isOk())
        L::advanceAction(clear_, reservation_, clearTarget_, M::ActionOutcome::INVALIDATED, now);
    afterClear();
}
// The block outlives this context and every release: only a stop is admitted after a possibly held 1.
// The reservation follows the epoch a synchronization reached, now that no clear needs the old one. A
// synchronization still owed waits for service(), never inside a stop request.
void AxisSession::afterClear() noexcept {
    clearBlocked_ |= clear_.valueOneMayRemain;
    clearAxis_ = nullptr; // The borrow ends with the clear.
    clearReference_ = nullptr;
    reservation_.target.channel.binding = target_.channel.binding;
}
// A counter clear's 1 notice advanced the axis and clearTarget_ at once. The owner's reservation follows
// once nothing of this session is outstanding: its configuration callback then sees an axis already
// withdrawn, and this withdraws no coordinates. An unresolved route has no usable reservation, so it
// never moves. A latest controlword from the old epoch is withdrawn, as for any epoch change.
bool AxisSession::syncCounter(uint64_t now) noexcept {
    if (!clear_.coordinatesInvalidated ||
        M::sameBinding(target_.channel.binding, clearTarget_.channel.binding))
        return true;
    if (pending_.owner || urgent_.owner || superseded_.owner || stopActive() || !usable() ||
        !owner_.advanceConfiguration(sequence_, clearTarget_.channel.binding, now))
        return false;
    dropControl();
    target_.channel.binding = clearTarget_.channel.binding;
    if (!clearActive())
        reservation_.target.channel.binding = target_.channel.binding;
    lastUs_ = now;
    return true;
}
void AxisSession::serviceClear(uint64_t now) noexcept {
    if (!clearActive())
        return;
    // As for an enable: the frozen reservation identifies this clear, with clearTarget_ its current
    // target, and real currentness is checked below, so that a terminal is consumed with its evidence
    // even after its route has gone.
    if (pending_.owner) {
        C::SdoCompletion evidence;
        if (!takeTerminal(evidence))
            return;
        // As for a move: a cancel this session did not make, such as another producer's urgent operation
        // on this reservation, is recorded as INVALIDATED, so that the clear reports no cancel of its own.
        // A revoked reservation or a transmit guard's refusal already reads as an invalidation.
        if (evidence.outcome == C::SdoOutcome::CANCELLED && clearWithdrawn_ == M::ActionOutcome::NONE) {
            clearWithdrawn_ = M::ActionOutcome::INVALIDATED;
            L::noteActionWithdrawal(clear_, reservation_, clearTarget_, M::ActionOutcome::INVALIDATED, now);
        }
        if (!L::advanceAction(clear_, reservation_, clearTarget_, evidence, now).isOk()) {
            endClear(clearWithdrawn_ != M::ActionOutcome::NONE ? clearWithdrawn_
                                                               : M::ActionOutcome::INVALIDATED,
                     now);
            return;
        }
        if (!clearActive()) {
            afterClear();
            return;
        }
    }
    if (clearWithdrawn_ != M::ActionOutcome::NONE || now >= clear_.deadlineUs || !usable()) {
        endClear(now >= clear_.deadlineUs                       ? M::ActionOutcome::DEADLINE
                 : clearWithdrawn_ != M::ActionOutcome::NONE ? clearWithdrawn_
                                                             : M::ActionOutcome::INVALIDATED,
                 now);
        return;
    }
    // A counter clear reads on the epoch its 1 advanced, so the owner moves there before the first read.
    // A refusal is offered again on a later turn, until the deadline: it sends nothing.
    if (clear_.phase != L::ActionPhase::COMMAND && !syncCounter(now))
        return;
    L::PreparedAction work;
    if (!L::nextAction(clear_, reservation_, clearTarget_, now, work).isOk()) {
        endClear(M::ActionOutcome::INVALIDATED, now);
        return;
    }
    if (!work.ready)
        return;
    const BusAdmission admission = admitWork(work.transfer, now);
    if (admission == BusAdmission::RESULTS_FULL)
        return; // Unsent, so the same work is simply offered again.
    if (admission != BusAdmission::ACCEPTED) {
        endClear(M::ActionOutcome::INVALIDATED, now);
        return;
    }
    if (work.transfer.direction != C::SdoDirection::DOWNLOAD)
        return;
    // Nothing leaves before the next owner turn, so the notice is made here and the terminal refines it.
    // For a counter clear's 1 it is also the coordinate withdrawal, before the frame could leave. If it
    // refuses, withdrawing cancels the write before any turn could carry it.
    const M::Status noted =
        clear_.actionKind == M::ActionKind::SET_DEVICE_POSITION
            ? L::noteActionTransmission(clear_, reservation_, clearTarget_, *clearAxis_,
                                        *clearReference_, now)
            : L::noteActionTransmission(clear_, reservation_, clearTarget_, now);
    if (!noted.isOk())
        withdrawClear(M::ActionOutcome::INVALIDATED, now);
}
// noteMoveTransmission's recheck of a parameter or trigger download, without its notice or
// retirements, so that it can run again at the handover. Keep the two in step.
M::Status AxisSession::recheckMove(uint64_t now) const noexcept {
    const M::PreparedTarget& target = move_.parameters.target;
    M::Status s = M::validatePreparedTarget(target, *moveAxis_, moveReference_, now);
    if (!s) {
        s.detail += static_cast<int32_t>(L::PositionParameterError::AXIS_ERROR_BASE) +
                    static_cast<int32_t>(M::MoveError::PARAMETER_ERROR_BASE);
        return s;
    }
    // validatePreparedTarget has refused a used witness that is missing.
    if (target.referenceUsed && (!moveReference_->idle || !moveReference_->stationary))
        return M::Status(M::Err::INVALID_CONFIG, static_cast<int32_t>(M::MoveError::REFERENCE),
                         "the move's witness is no longer idle and stationary");
    if (now - move_.admission.earliestUs > move_.options.maximumAgeUs)
        return M::Status(M::Err::INVALID_CONFIG, static_cast<int32_t>(M::MoveError::STATE),
                         "the admission state read is older than its age cap");
    return M::Ok();
}
// Inside the owner's turn: reads this session and its borrowed inputs, and makes no owner call.
bool AxisSession::preflight(SequenceId sequence, const C::SdoTransfer& transfer, uint64_t now) noexcept {
    if (!sameSequence(sequence, sequence_) || transfer.direction != C::SdoDirection::DOWNLOAD)
        return true;
    if (clearActive() && transfer.operation == clear_.operation)
        return clearWithdrawn_ == M::ActionOutcome::NONE; // Withdrawn while it waited; its cancel lost.
    if (!moveActive() || transfer.operation != move_.operation)
        return true;
    if (moveWithdrawn_ != M::MoveInterruption::NONE)
        return false; // Withdrawn while it waited, and its cancel did not win.
    if (move_.phase == L::MovePhase::REARM)
        return true; // As at its notice: the rearm follows an observed completion.
    // A session time ahead of the owner's can only have aged the inputs further.
    const M::Status checked = recheckMove(now < lastUs_ ? lastUs_ : now);
    if (checked.isOk())
        return true;
    moveRefusal_ = checked;
    return false;
}
// Fresh MONITORS reporting the controller not running. Absent or stale state is not this report.
bool AxisSession::freshlyIdle(const L::ReadContext& state, const StopPolicy& policy,
                              uint64_t now) const noexcept {
    L::StateObservation monitors;
    return L::getStateBlock(state, L::StateBlock::MONITORS, target_, now, policy.options.maximumAgeUs,
                            monitors) &&
           !(monitors.monitor & L::MONITOR_RUNNING);
}
// Fresh evidence that this drive is idle and outside the stop's states refuses both paths:
// nothing establishes what 000B does in those states. A running report keeps the stop.
bool AxisSession::stateRefuses(const L::ReadContext& state, const StopPolicy& policy,
                               uint64_t now) const noexcept {
    L::StateObservation monitors, status;
    const uint64_t age = policy.options.maximumAgeUs;
    if (!L::getStateBlock(state, L::StateBlock::MONITORS, target_, now, age, monitors) ||
        (monitors.monitor & L::MONITOR_RUNNING))
        return false;
    return (monitors.monitor & (L::MONITOR_ALARM | L::MONITOR_RELEASED)) ||
           (L::getStateBlock(state, L::StateBlock::STATUS, target_, now, age, status) &&
            (status.modeRaw != 1 || (status.statusword & 7u) != 7));
}
StopPath AxisSession::requestStop(L::StopKind kind, const L::ReadContext& state,
                                  const StopPolicy& requested, uint64_t now) noexcept {
    // Even a stop refused below ends the enable, the move and the clear. Asking for a stop means no
    // further word, so a time that moved backward still ends them, at the last valid time: the later of
    // this session's and the owner's, at which the owner can still cancel a queued download.
    const uint64_t latest = latestUs();
    const uint64_t at = now < latest ? latest : now;
    if (open_ && ((enableActive() && enableWithdrawn_ == M::ActionOutcome::NONE) ||
                  (moveActive() && moveWithdrawn_ == M::MoveInterruption::NONE) ||
                  (clearActive() && clearWithdrawn_ == M::ActionOutcome::NONE))) {
        withdrawEnable(M::ActionOutcome::CANCELLED, at);
        withdrawMove(M::MoveInterruption::STOP, at);
        withdrawClear(M::ActionOutcome::CANCELLED, at);
        lastUs_ = at;
    }
    // These widths concern identity and group reads. A stop reads neither, so they cannot refuse a stop.
    StopPolicy policy = requested;
    policy.options.service.identityWidths = L::IdentityWidths::DECLARED_U16;
    policy.options.service.countWidths = L::CountWidths::DECLARED_U8;
    if (!open_ || kind > L::StopKind::QUICK_STOP || now < latest || policy.deadlineUs <= now ||
        (policy.bestEffortDeadlineUs && policy.bestEffortDeadlineUs < policy.deadlineUs) ||
        !L::stopOptionsValid(policy.options))
        return StopPath::NONE;
    const bool escalating = stopActive();
    if (escalating && (path_ != StopPath::CONFIRMED || activeKind_ != L::StopKind::HALT ||
                       kind != L::StopKind::QUICK_STOP))
        return StopPath::NONE; // Upward only, and nothing outranks a stop already on 000B.
    if (superseded_.owner && kind == L::StopKind::HALT)
        return StopPath::NONE; // Its context is still waiting for that terminal.
    withdrawIfStale();
    // A known word with no stop arrow, such as enable word 6 or 7, refuses only on a fresh idle report.
    // A fresh running report, or no fresh report, takes 000B, as the same word still in flight would.
    if ((controlPresent_ && !L::stopPredecessorAllowed(word(control_.transfer), kind) &&
         freshlyIdle(state, policy, now)) ||
        stateRefuses(state, policy, now))
        return StopPath::NONE;
    const Sequence* s = usable() ? owner_.sequence(sequence_) : nullptr;
    const uint32_t last = s && s->operation > operation_ ? s->operation : operation_;
    // One sequenced stop, one fallback and one held stop can each need an identity.
    if (last > UINT32_MAX - 3)
        return StopPath::NONE;
    const uint32_t next = last + 1;
    // Check everything before the owner moves; the context is written only once that succeeded.
    L::ActionContext& slot = stops_[static_cast<unsigned>(kind)];
    L::ActionContext& halt = stops_[static_cast<unsigned>(L::StopKind::HALT)];
    const M::Status prepared =
        escalating
            ? L::validateEscalation(slot, halt, next, now, policy.deadlineUs, policy.options)
        : controlPresent_
            ? L::validateStop(slot, reservation_, state, control_, kind, next, now, policy.deadlineUs,
                           policy.options)
            : M::Status(M::Err::INVALID_CONFIG, 0, "no latest controlword");
    const bool sequenced = s && prepared.isOk();
    M::Generation node = 0;
    uint32_t sessions = 0;
    if (!sequenced &&
        (!policy.bestEffortDeadlineUs || now >= policy.bestEffortDeadlineUs ||
         !owner_.nodeEpochs(target_.channel.binding, node, sessions) || sessions != serverSessions_))
        return StopPath::NONE;
    const uint64_t deadline = sequenced ? policy.deadlineUs : policy.bestEffortDeadlineUs;
    if (s && !owner_.beginUrgentOperation(sequence_, next, deadline, now) && sequenced)
        return StopPath::NONE;
    if (sequenced) {
        // Same values as the check above, and only the owner moved, so neither can refuse.
        if (escalating)
            L::prepareEscalation(slot, halt, next, now, policy.deadlineUs, policy.options);
        else
            L::prepareStop(slot, reservation_, state, control_, kind, next, now, policy.deadlineUs,
                           policy.options);
    }
    if (escalating) {
        // Its in-flight request settles into its own context and holds the slot until then.
        L::ActionContext& old = stops_[static_cast<unsigned>(L::StopKind::HALT)];
        superseded_ = urgent_;
        urgent_ = RequestId();
        // Superseded by escalation. With a request in flight its terminal is kept as evidence only,
        // so a cycle completing in it cannot end this HALT as SUCCEEDED.
        if (!superseded_.owner)
            L::advanceAction(old, reservation_, reservation_.target, M::ActionOutcome::CANCELLED, now);
        else
            L::noteActionWithdrawal(old, reservation_, reservation_.target, M::ActionOutcome::CANCELLED,
                                    now);
    }
    policy_ = policy;
    operation_ = next;
    suspended_ = RequestId(); // From here the stop decides the latest word.
    motionWritten_ = true; // Whichever path this takes, a stop frame may reach the drive.
    recoveryUsed_ = false;
    const uint32_t epoch = owner_.controllerEpoch();
    epochLimit_ = owner_.recoveryActive() ? epoch : epoch + 1; // A running recovery is the one.
    lastUs_ = now;
    if (sequenced) {
        activeKind_ = kind;
        path_ = StopPath::CONFIRMED;
        serviceConfirmed(now);
    } else {
        activeKind_ = L::StopKind::QUICK_STOP;
        if (!startBestEffort(now))
            path_ = StopPath::FINISHED;
    }
    return path_;
}
void AxisSession::service(uint64_t now) noexcept {
    if (!open_ || now < latestUs())
        return;
    lastUs_ = now;
    withdrawIfStale();
    settleSuperseded(now);
    serviceEnable(now);
    serviceMove(now);
    serviceClear(now);
    switch (path_) {
    case StopPath::CONFIRMED:
        serviceConfirmed(now);
        break;
    case StopPath::BEST_EFFORT:
        serviceBestEffort(now);
        break;
    case StopPath::HELD:
        serviceHeld(now);
        break;
    default:
        break;
    }
    // A counter clear that ended before the owner could follow it, as behind a stop.
    if (!clearActive())
        syncCounter(now);
}
// An escalated HALT's terminal belongs to its own context. It is older intent, so even a
// success does not become the latest controlword, and its withdrawal was noted at escalation.
void AxisSession::settleSuperseded(uint64_t now) noexcept {
    const Completion* terminal = superseded_.owner ? owner_.result(superseded_) : nullptr;
    if (!terminal)
        return;
    L::ActionContext& old = stops_[static_cast<unsigned>(L::StopKind::HALT)];
    const C::SdoCompletion evidence = terminal->service;
    L::advanceAction(old, reservation_, reservation_.target, evidence, now);
    if (old.state == M::ActionState::ACTIVE)
        L::advanceAction(old, reservation_, reservation_.target, M::ActionOutcome::CANCELLED, now);
    owner_.release(superseded_);
    superseded_ = RequestId();
}
// A node epoch that moved with no new server session is a local controller event.
bool AxisSession::controllerMoved(M::Generation before) const noexcept {
    M::Generation node = 0;
    uint32_t sessions = 0;
    return owner_.nodeEpochs(target_.channel.binding, node, sessions) && node != before &&
           sessions == serverSessions_;
}
void AxisSession::serviceConfirmed(uint64_t now) noexcept {
    L::ActionContext& a = stops_[static_cast<unsigned>(activeKind_)];
    // The frozen reservation identifies this action. Real currentness is checked below,
    // so that a terminal is consumed with its evidence even after its route has gone.
    const L::ReadTarget& frozen = reservation_.target;
    if (urgent_.owner) {
        const Completion* terminal = owner_.result(urgent_);
        if (!terminal)
            return;
        const bool wasCommand = a.phase == L::ActionPhase::COMMAND;
        const C::SdoCompletion evidence = terminal->service;
        if (!L::advanceAction(a, reservation_, frozen, evidence, now).isOk())
            L::advanceAction(a, reservation_, frozen, M::ActionOutcome::INVALIDATED, now);
        owner_.release(urgent_);
        urgent_ = RequestId();
        if (wasCommand && evidence.outcome == C::SdoOutcome::SUCCESS && usable()) {
            control_ = evidence;
            controlPresent_ = true;
            controllerEpoch_ = owner_.controllerEpoch();
        }
    }
    if (a.state != M::ActionState::ACTIVE) {
        afterConfirmedEnd(now);
        return;
    }
    if (now >= a.deadlineUs || !usable()) {
        L::advanceAction(a, reservation_, frozen,
                         now >= a.deadlineUs ? M::ActionOutcome::DEADLINE
                                             : M::ActionOutcome::INVALIDATED,
                         now);
        afterConfirmedEnd(now);
        return;
    }
    L::PreparedAction work;
    if (!L::nextAction(a, reservation_, frozen, now, work).isOk()) {
        L::advanceAction(a, reservation_, frozen, M::ActionOutcome::INVALIDATED, now);
        afterConfirmedEnd(now);
        return;
    }
    if (!work.ready)
        return;
    BusRequest request;
    request.sequence = sequence_;
    request.transfer = work.transfer;
    RequestId id;
    const BusAdmission admission = owner_.admitUrgent(request, now, id);
    if (admission == BusAdmission::RESULTS_FULL)
        return;
    if (admission != BusAdmission::ACCEPTED) {
        L::advanceAction(a, reservation_, frozen, M::ActionOutcome::INVALIDATED, now);
        afterConfirmedEnd(now);
        return;
    }
    urgent_ = id;
    if (a.phase == L::ActionPhase::COMMAND) {
        // Nothing can leave before the next owner turn, so the boundary is noted here, and the
        // terminal refines this notice.
        dropControl();
        L::noteActionTransmission(a, reservation_, frozen, now);
    }
}
void AxisSession::afterConfirmedEnd(uint64_t now) noexcept {
    const L::ActionContext& a = stops_[static_cast<unsigned>(activeKind_)];
    path_ = StopPath::FINISHED;
    if (a.phase != L::ActionPhase::COMMAND || a.execution != M::ActionExecution::NOT_TRANSMITTED ||
        a.effectsPossible)
        return; // A write that may have left is never sent again.
    if (controllerMoved(reservation_.target.nodeGeneration) && !recoveryUsed_)
        hold(now, policy_.deadlineUs);
    else if (policy_.bestEffortDeadlineUs)
        startBestEffort(now);
}
bool AxisSession::startBestEffort(uint64_t now) noexcept {
    M::Generation node = 0;
    uint32_t sessions = 0;
    if (!owner_.nodeEpochs(target_.channel.binding, node, sessions) || sessions != serverSessions_ ||
        now >= policy_.bestEffortDeadlineUs || !prepareHeld(now, policy_.bestEffortDeadlineUs))
        return false;
    serviceHeld(now);
    return true;
}
bool AxisSession::prepareHeld(uint64_t now, uint64_t deadline) noexcept {
    M::Generation node = 0;
    uint32_t sessions = 0;
    L::ReadTarget current = target_;
    if (owner_.nodeEpochs(target_.channel.binding, node, sessions))
        current.nodeGeneration = node;
    L::BestEffortStop candidate;
    if (operation_ == UINT32_MAX ||
        !L::prepareBestEffortStop(candidate, current, operation_ + 1, now, deadline,
                                  policy_.options.service)
             .isOk())
        return false;
    ++operation_;
    bestEffort_ = candidate;
    path_ = StopPath::HELD;
    return true;
}
void AxisSession::hold(uint64_t now, uint64_t deadline) noexcept {
    recoveryUsed_ = true;
    path_ = StopPath::FINISHED;
    if (now < deadline)
        prepareHeld(now, deadline);
}
void AxisSession::endHeld(M::ActionOutcome reason, uint64_t now) noexcept {
    L::advanceBestEffortStop(bestEffort_, reason, now);
    path_ = StopPath::FINISHED;
}
void AxisSession::serviceHeld(uint64_t now) noexcept {
    if (now >= bestEffort_.deadlineUs) {
        endHeld(M::ActionOutcome::DEADLINE, now);
        return;
    }
    M::Generation node = 0;
    uint32_t sessions = 0;
    if (!owner_.nodeEpochs(target_.channel.binding, node, sessions) || sessions != serverSessions_) {
        endHeld(M::ActionOutcome::INVALIDATED, now); // A new server session needs new intent.
        return;
    }
    if (owner_.controllerEpoch() > epochLimit_) {
        endHeld(M::ActionOutcome::TRANSPORT_ERROR, now); // A second recovery ends it unsent.
        return;
    }
    // A controller event moves the node epoch, and this frame has not left yet.
    bestEffort_.target.nodeGeneration = node;
    L::PreparedAction work;
    if (!L::nextBestEffortStop(bestEffort_, bestEffort_.target, now, work).isOk() || !work.ready) {
        endHeld(M::ActionOutcome::INVALIDATED, now);
        return;
    }
    RequestId id;
    const BusAdmission admission = owner_.admitBestEffortStop(work.transfer, now, id);
    if (admission == BusAdmission::ACCEPTED) {
        urgent_ = id;
        dropControl();
        L::noteBestEffortAdmission(bestEffort_, now);
        path_ = StopPath::BEST_EFFORT;
    } else if (admission != BusAdmission::INHIBITED && admission != BusAdmission::RESULTS_FULL) {
        endHeld(M::ActionOutcome::INVALIDATED, now);
    }
}
void AxisSession::serviceBestEffort(uint64_t now) noexcept {
    const Completion* terminal = owner_.result(urgent_);
    if (!terminal)
        return;
    const C::SdoCompletion evidence = terminal->service;
    L::advanceBestEffortStop(bestEffort_, evidence, now);
    owner_.release(urgent_);
    urgent_ = RequestId();
    path_ = StopPath::FINISHED;
    if (!evidence.transmissionPossible && !recoveryUsed_ &&
        controllerMoved(bestEffort_.target.nodeGeneration)) {
        firstBestEffort_ = bestEffort_;
        hold(now, bestEffort_.deadlineUs);
    }
}
} // namespace MotorControlCANExample
