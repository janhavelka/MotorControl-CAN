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
} // namespace

bool AxisSession::open(const L::ReadTarget& target, L::SettingsModel model, uint8_t producer,
                       uint64_t now) noexcept {
    if (open_ || now < lastUs_ || !target.nodeGeneration ||
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
    controlPresent_ = recoveryUsed_ = false;
    L::resetAction(stops_[0]);
    L::resetAction(stops_[1]);
    L::resetAction(enable_);
    enableWithdrawn_ = M::ActionOutcome::NONE;
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
    open_ = controlPresent_ = false;
    path_ = StopPath::NONE;
    return true;
}
bool AxisSession::usable() const noexcept {
    const Sequence* s = open_ && sequence_.owner ? owner_.sequence(sequence_) : nullptr;
    return s && s->nodeGeneration == target_.nodeGeneration &&
           M::sameBinding(s->binding, target_.channel.binding);
}
void AxisSession::withdrawIfStale() noexcept {
    if (controlPresent_ && (!usable() || owner_.controllerEpoch() != controllerEpoch_))
        controlPresent_ = false;
}
uint32_t AxisSession::beginOperation(uint64_t deadline, uint64_t now) noexcept {
    if (now < lastUs_ || !usable() || busy())
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
    if (now < lastUs_ || !open_ || busy())
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
            controlPresent_ = false;
            motionWritten_ |= !isModeOneWrite(transfer);
        }
    }
    return admission;
}
bool AxisSession::settle(C::SdoCompletion& output) noexcept {
    return !enableActive() && takeTerminal(output);
}
bool AxisSession::takeTerminal(C::SdoCompletion& output) noexcept {
    const Completion* terminal = owner_.result(pending_);
    if (!terminal)
        return false;
    const C::SdoCompletion copy = terminal->service;
    if (!owner_.release(pending_))
        return false;
    pending_ = RequestId();
    const Sequence* s = owner_.sequence(sequence_);
    // A write settling after a stop was admitted is older intent, not the latest word.
    if (copy.outcome == C::SdoOutcome::SUCCESS && isControlWrite(copy.transfer) && s &&
        s->operation == copy.transfer.operation && !stopActive()) {
        control_ = copy;
        controlPresent_ = true;
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
    if (now < lastUs_ || !open_ || !usable())
        return M::Status(M::Err::INVALID_CONFIG, static_cast<int32_t>(M::ActionError::TARGET),
                         "no usable reservation for an enable");
    if (busy())
        return M::Status(M::Err::INVALID_CONFIG, stateError, "axis is busy");
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
    if (now < lastUs_ || !enableActive() ||
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
    // As for a stop: no adapter gives a pre-transmit hook, nothing leaves before the next owner
    // turn, and the terminal refines this notice.
    if (enable_.phase == L::ActionPhase::COMMAND)
        L::noteActionTransmission(enable_, reservation_, frozen, now);
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
    // Even a stop refused below ends the enable. Asking for a stop means no further word, so a time
    // that moved backward still ends it, at the last valid time.
    if (open_)
        withdrawEnable(M::ActionOutcome::CANCELLED, now < lastUs_ ? lastUs_ : now);
    // This width concerns identity reads. A stop reads none, so it cannot refuse a stop.
    StopPolicy policy = requested;
    policy.options.service.identityWidths = L::IdentityWidths::DECLARED_U16;
    if (!open_ || kind > L::StopKind::QUICK_STOP || now < lastUs_ || policy.deadlineUs <= now ||
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
    if (!open_ || now < lastUs_)
        return;
    lastUs_ = now;
    withdrawIfStale();
    settleSuperseded(now);
    serviceEnable(now);
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
        // No adapter provides a pre-transmit hook yet, so note the boundary here. Nothing
        // can leave before the next owner turn, and the terminal refines this notice.
        controlPresent_ = false;
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
        controlPresent_ = false;
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
