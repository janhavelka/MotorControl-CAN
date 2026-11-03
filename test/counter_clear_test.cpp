// SPDX-License-Identifier: MIT
// Packet 18: the zero-only device counter clear and its reference withdrawal, through the real
// owner and SDO client, with synthetic traffic only. Host evidence: no drive has run any of this.
#include <cstdio>
#include <cstdlib>
#include <cstring>

static unsigned checks = 0;
#define CHECK(condition)                                                                           \
    do {                                                                                           \
        ++checks;                                                                                  \
        if (!(condition)) {                                                                        \
            std::fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #condition);                   \
            std::abort();                                                                          \
        }                                                                                          \
    } while (false)
#include "support/ReferenceRig.h"
namespace {
const uint16_t COUNTER = 0x2302;
const uint8_t FEEDBACK = uint8_t(1u << unsigned(L::StateBlock::FEEDBACK));

M::Status attempt(Rig& r, L::ActionContext& action, int64_t value = 0) {
    r.begin();
    return L::prepareSetDevicePosition(action, r.reservation, r.state, value, r.axis, r.operation,
                                       r.now, r.deadline, r.policy());
}
// A new FEEDBACK read on the current epoch, as a later explicit operation. Returns raw 6064.
int32_t freshFeedback(Rig& r) {
    r.deadline = r.now + 10000;
    r.begin();
    L::ReadContext read;
    CHECK(L::prepareState(read, r.current, r.operation, r.now, r.deadline, options(), FEEDBACK));
    while (read.state == M::ReadState::ACTIVE) {
        L::PreparedRead work;
        CHECK(L::nextRead(read, r.now, work) && work.ready);
        // Work on the other epoch never reaches the queue.
        O::BusRequest stale;
        stale.sequence = r.sequence;
        stale.transfer = work.transfer;
        ++stale.transfer.channel.binding.configurationGeneration;
        O::RequestId refused;
        CHECK(r.owner.admit(stale, r.now, refused) == O::BusAdmission::INVALID);
        const C::SdoCompletion terminal = r.exchange(work.transfer);
        CHECK(L::advanceRead(read, terminal, r.now));
    }
    L::StateObservation feedback;
    CHECK(L::getStateBlock(read, L::StateBlock::FEEDBACK, r.current, r.now, 1000, feedback));
    return feedback.actualPosition;
}

// Zero only, rejected before any other check; mode 1, running and alarm clear, 606C zero and a
// fresh FEEDBACK block; matching axis epochs with room. Every refusal leaves its outputs alone.
void admissionScope() {
    {
        L::ActionContext action;
        const Saved<L::ActionContext> empty(action);
        const L::AxisReservation reservation;
        const L::ReadContext state;
        const M::AxisConfig axis;
        const L::ActionOptions policy;
        const int64_t values[] = {1, -1, INT64_MIN, INT64_MAX};
        for (int64_t value : values)
            CHECK(L::prepareSetDevicePosition(action, reservation, state, value, axis, 1, 0, 1, policy)
                      .code == M::Err::UNSUPPORTED);
        empty.unchanged(action);
    }
    struct Case {
        uint16_t monitor;
        uint8_t mode, mask;
        int32_t velocity;
    };
    const uint8_t all = 0x1f, noFeedback = 0x0c;
    const Case cases[] = {{L::MONITOR_ALARM, 1, all, 0}, {L::MONITOR_RUNNING, 1, all, 0},
                          {0, 0, all, 0},                {0, 3, all, 0},
                          {0, 6, all, 0},                {0, 1, all, 5},
                          {0, 1, all, -1},               {0, 1, noFeedback, 0}};
    for (const Case& item : cases) {
        Rig r;
        r.feedbackVelocity = item.velocity;
        r.acquire(item.monitor, item.mode, 0, item.mask);
        L::ActionContext action;
        const Saved<L::ActionContext> empty(action);
        const Saved<M::AxisConfig> axis(r.axis);
        const M::Status status = attempt(r, action);
        CHECK(!status && status.detail == int32_t(M::ActionError::STATE));
        empty.unchanged(action);
        axis.unchanged(r.axis);
    }
    for (unsigned which = 0; which < 3; ++which) {
        Rig r;
        r.acquire(0, 1, 0);
        if (which == 0)
            ++r.axis.binding.configurationGeneration;
        else if (which == 1)
            r.axis.exhausted = true;
        else
            r.axis.referenceGeneration = 0;
        L::ActionContext action;
        const Saved<L::ActionContext> empty(action);
        const M::Status status = attempt(r, action);
        CHECK(!status && status.detail == int32_t(M::ActionError::TARGET));
        empty.unchanged(action);
    }
    // A historical 603F does not block a zero; only the reported alarm does.
    Rig r;
    r.feedbackPosition = -7;
    r.acquire(0, 1, 0xff05);
    L::ActionContext action;
    CHECK(attempt(r, action));
    CHECK(action.baseline.present && action.baseline.startPosition == -7 &&
          action.baseline.startVelocity == 0 && !action.incident.present &&
          action.originalReferenceGeneration == 1 && !action.coordinatesInvalidated);
}

// On success: one withdrawal at the first possible 1, one owner synchronization that does not
// withdraw again, a raw zero that publishes nothing, and every old snapshot left as it was.
void exactlyOneWithdrawalOnSuccess() {
    for (unsigned already = 0; already < 2; ++already) {
        Rig r;
        r.feedbackPosition = already ? 0 : 123;
        r.acquire(0);
        L::ActionContext action;
        r.prepare(action, true);
        const Saved<M::PreparedTarget> snapshot(r.prepared);
        const Saved<L::ReadContext> oldRead(r.state);
        const int64_t nativeMinimum = r.axis.nativeMinimum, nativeMaximum = r.axis.nativeMaximum;
        const unsigned from = r.wire.sentCount;
        r.command(action, COUNTER, 0);
        CHECK(r.axis.referenceGeneration == 1 && !action.coordinatesInvalidated);
        r.command(action, COUNTER, 1);
        CHECK(r.axis.referenceGeneration == 2 && r.axis.binding.configurationGeneration == 8 &&
              action.coordinatesInvalidated);
        const Saved<C::SdoCompletion> trigger(action.commandHistory[1]);
        r.command(action, COUNTER, 0);
        CHECK(r.axis.referenceGeneration == 2 && r.wire.notices == 3 && r.invalidations == 0);
        r.synchronize();
        r.cycle(action);
        CHECK(action.state == M::ActionState::SUCCEEDED && action.completion == M::ActionCompletion::OBSERVED &&
              !action.valueOneMayRemain && action.extraObservation.actualPosition == 0);
        // An already-zero baseline: the raw zero shows no change at all.
        CHECK(action.baseline.startPosition == (already ? 0 : 123));
        CHECK(r.axis.referenceGeneration == 2 && r.axis.binding.configurationGeneration == 8 &&
              r.invalidations == 1);
        CHECK(r.axis.nativeMinimum == nativeMinimum && r.axis.nativeMaximum == nativeMaximum);
        trigger.unchanged(action.commandHistory[1]);
        snapshot.unchanged(r.prepared);
        oldRead.unchanged(r.state);
        r.checkWithdrawn(); // No origin, home or witness from the raw zero.
        r.onlyActionTraffic(from, COUNTER, 3);
        CHECK(freshFeedback(r) == 0);
        r.checkWithdrawn();
    }
}

// What a failure leaves for later work: the same epochs with a definite pre-trigger failure; the
// withdrawn epochs, through a retirement-gated synchronization, after a possible trigger on a
// resolved route; and no new work at all while the route is unresolved, whatever epochs advance.
void laterAdmission(Rig& r, L::ActionContext& action, const Rig::Failure& f, bool withdrawn,
                    bool unresolved) {
    const Saved<L::ActionContext> failed(action);
    const Saved<M::PreparedTarget> snapshot(r.prepared);
    r.deadline = r.now + 10000;
    CHECK(r.services.unresolved(0) == unresolved);
    if (unresolved) {
        CHECK(!r.owner.advanceConfiguration(r.sequence, r.current.channel.binding, r.now));
        CHECK(!r.owner.beginOperation(r.sequence, r.operation + 1, r.deadline, r.now));
        if (f.admitted)
            CHECK(r.owner.release(f.id));
        CHECK(!r.owner.advanceConfiguration(r.sequence, r.current.channel.binding, r.now));
        CHECK(r.owner.releaseSequence(r.sequence));
        O::SequenceId replacement;
        CHECK(!r.owner.beginSequence(target().channel.binding, 1, r.now, replacement));
        CHECK(!r.owner.beginSequence(r.current.channel.binding, 1, r.now, replacement));
        CHECK(r.services.unresolved(0));
    } else if (withdrawn) {
        if (f.admitted) {
            CHECK(!r.owner.advanceConfiguration(r.sequence, r.current.channel.binding, r.now));
            CHECK(r.owner.release(f.id));
        }
        r.synchronize();
        CHECK(freshFeedback(r) == 0); // A new operation; it cannot complete the failed one.
    } else {
        if (f.admitted)
            CHECK(r.owner.release(f.id));
        r.checkIntact();
        L::StateObservation old;
        CHECK(L::getStateBlock(r.state, L::StateBlock::FEEDBACK, r.current, r.now, UINT64_MAX, old) &&
              old.actualPosition == 123);
        CHECK(freshFeedback(r) == 0);
        r.checkIntact();
    }
    if (withdrawn)
        r.checkWithdrawn();
    snapshot.unchanged(r.prepared);
    failed.unchanged(action);
}

// Each of 0, 1, 0 fails at each boundary, independently.
void stageMatrix() {
    for (unsigned slot = 0; slot < 3; ++slot)
        for (Stage stage : STAGES) {
            Rig r;
            r.acquire(0);
            L::ActionContext action;
            r.prepare(action, true);
            const Saved<L::CounterBaseline> baseline(action.baseline);
            const unsigned from = r.wire.sentCount;
            for (unsigned i = 0; i < slot; ++i)
                r.command(action, COUNTER, i == 1 ? 1 : 0);
            const Saved<C::SdoCompletion> trigger(action.commandHistory[1]);
            const Rig::Failure f = r.failCommand(action, stage);
            const bool acked = acknowledged(stage);
            CHECK(acked == (f.admitted && f.terminal.outcome == C::SdoOutcome::SUCCESS));
            const bool withdrawn = slot == 2 || (slot == 1 && sent(stage));
            CHECK(action.state == M::ActionState::FAILED &&
                  action.completion == M::ActionCompletion::NOT_OBSERVED);
            CHECK(action.outcome == outcomeAt(stage));
            CHECK(action.commandEvidenceMask ==
                  uint8_t((1u << (slot + (f.admitted ? 1 : 0))) - 1));
            for (unsigned i = 0; i < slot; ++i)
                CHECK(action.commandHistory[i].protocolAcknowledged);
            if (slot == 2)
                trigger.unchanged(action.commandHistory[1]); // The trigger ACK survives.
            CHECK(action.execution == (slot == 2 ? M::ActionExecution::ACKNOWLEDGED
                                       : slot == 0 || !sent(stage) ? M::ActionExecution::NOT_TRANSMITTED
                                       : stage == Stage::ABORTED ? M::ActionExecution::REJECTED
                                       : acked ? M::ActionExecution::ACKNOWLEDGED
                                               : M::ActionExecution::UNKNOWN));
            CHECK(action.coordinatesInvalidated == withdrawn);
            CHECK(action.valueOneMayRemain == (withdrawn && !(slot == 2 && acked)));
            CHECK(action.uncertain == (slot > 0 || sent(stage)));
            CHECK(r.axis.referenceGeneration == (withdrawn ? 2u : 1u) && r.invalidations == 0);
            CHECK(r.wire.notices == slot + (sent(stage) ? 1u : 0u));
            CHECK(r.services.unresolved(0) == ambiguous(stage));
            CHECK(r.wire.sentCount == f.sent + (sent(stage) ? 1u : 0u));
            r.onlyActionTraffic(from, COUNTER, slot + (sent(stage) ? 1u : 0u));
            baseline.unchanged(action.baseline);
            if (withdrawn)
                r.checkWithdrawn();
            else
                r.checkIntact();
            laterAdmission(r, action, f, withdrawn, ambiguous(stage));
            // Last, because a stray reply quarantines the route: it changes nothing here either.
            if (f.admitted)
                r.lateReplyChangesNothing(action, f);
        }
}

// The guard refuses the 1 before it can leave: a rejection before possible effect, so nothing
// is withdrawn and no epoch moves. Here the application's own witness no longer matches.
void guardRefusalLeavesValidity() {
    Rig r;
    r.acquire(0);
    L::ActionContext action;
    r.prepare(action, true);
    r.command(action, COUNTER, 0);
    r.reference.referenceGeneration = 5;
    const Saved<M::AxisConfig> axis(r.axis);
    const Saved<M::AxisReference> reference(r.reference);
    const L::PreparedAction work = r.next(action);
    const O::RequestId id = r.admit(work.transfer);
    const unsigned sent = r.wire.sentCount;
    r.turn(r.now);
    const C::SdoCompletion terminal = r.result(id);
    CHECK(!terminal.transmissionPossible && r.wire.sentCount == sent && r.wire.notices == 2);
    CHECK(L::advanceAction(action, r.reservation, r.current, terminal, r.now));
    CHECK(action.state == M::ActionState::FAILED && !action.coordinatesInvalidated &&
          action.execution == M::ActionExecution::NOT_TRANSMITTED && !action.valueOneMayRemain);
    axis.unchanged(r.axis);
    reference.unchanged(r.reference);
    r.checkIntact();
    L::StateObservation old;
    CHECK(L::getStateBlock(r.state, L::StateBlock::FEEDBACK, r.current, r.now, UINT64_MAX, old) &&
          old.actualPosition == 123);
    CHECK(!r.services.unresolved(0) && freshFeedback(r) == 0); // The old epoch is still the route's.
    r.checkIntact();
}

// Cancel while the 1 is in flight; its ACK arrives anyway. The withdrawal stays, the ACK is
// kept, the final 0 is never sent, and the route is resolved for an explicit synchronization.
void cancelWhileTriggerInFlight() {
    Rig r;
    r.acquire(0);
    L::ActionContext action;
    r.prepare(action, true);
    const unsigned from = r.wire.sentCount;
    r.command(action, COUNTER, 0);
    const L::PreparedAction work = r.next(action);
    const O::RequestId id = r.admit(work.transfer);
    r.turn(r.now);
    CHECK(action.coordinatesInvalidated);
    CHECK(L::noteActionWithdrawal(action, r.reservation, r.current, M::ActionOutcome::CANCELLED, r.now));
    r.wire.push(reply(work.transfer), r.now + 1);
    r.turn(r.now + 1, budget(8, 0, 8, 0));
    CHECK(L::advanceAction(action, r.reservation, r.current, r.result(id), r.now));
    CHECK(action.outcome == M::ActionOutcome::CANCELLED &&
          action.execution == M::ActionExecution::ACKNOWLEDGED &&
          action.commandHistory[1].protocolAcknowledged && action.valueOneMayRemain);
    r.turn(r.now + 1000);
    r.onlyActionTraffic(from, COUNTER, 2);
    r.checkWithdrawn();
    r.synchronize();
    CHECK(freshFeedback(r) == 0);
}

// A boot-up with the 1 in flight. The withdrawal made at its possible transmission stays; the
// application's lifecycle withdrawal is its own, separate step.
void rebootDuringTrigger() {
    Rig r;
    r.acquire(0);
    L::ActionContext action;
    r.prepare(action, true);
    r.command(action, COUNTER, 0);
    const L::PreparedAction work = r.next(action);
    const O::RequestId id = r.admit(work.transfer);
    r.turn(r.now);
    CHECK(action.coordinatesInvalidated && r.axis.referenceGeneration == 2);
    M::Frame boot = frame(0x701, 1);
    boot.data[0] = 0;
    r.wire.push(boot, r.now + 1);
    r.turn(r.now + 1, budget(8, 0, 8, 0));
    CHECK(r.result(id).outcome == C::SdoOutcome::REBOOT);
    CHECK(L::advanceAction(action, r.reservation, r.current, M::ActionOutcome::INVALIDATED, r.now));
    CHECK(action.outcome == M::ActionOutcome::INVALIDATED && action.valueOneMayRemain &&
          action.coordinatesInvalidated && action.commandEvidenceMask == 1 &&
          action.execution == M::ActionExecution::UNKNOWN);
    CHECK(!r.axis.originKnown && !r.reference.nativeKnown && r.axis.referenceGeneration > 2 &&
          !M::validatePreparedTarget(r.prepared, r.axis, nullptr, r.now));
}

// Epochs that reach their last value: the action uses it once and never wraps, and the next
// counter clear refuses before any work.
void generationExhaustion() {
    for (unsigned which = 0; which < 2; ++which) {
        Rig r(which ? UINT32_MAX - 1 : 7, which ? 1 : UINT32_MAX - 1);
        r.acquire(0);
        L::ActionContext action;
        r.prepare(action, true);
        for (unsigned i = 0; i < 3; ++i) {
            const L::PreparedAction work = r.next(action);
            CHECK(work.ready && work.transfer.data[0] == (i == 1 ? 1 : 0));
            r.field(action);
        }
        CHECK(r.axis.referenceGeneration == (which ? 2u : UINT32_MAX) &&
              r.axis.binding.configurationGeneration == (which ? UINT32_MAX : 8u) && !r.axis.exhausted);
        CHECK(r.owner.advanceConfiguration(r.sequence, r.current.channel.binding, r.now));
        r.cycle(action);
        CHECK(action.state == M::ActionState::SUCCEEDED && r.invalidations == 1);
        r.acquire(0);
        L::ActionContext next;
        const Saved<L::ActionContext> empty(next);
        const Saved<M::AxisConfig> axis(r.axis);
        CHECK(!attempt(r, next));
        empty.unchanged(next);
        axis.unchanged(r.axis);
    }
}

// Late responses after a terminal and after its release: the failed action and the wire stay
// as they were.
void lateResponseAfterRelease() {
    Rig r;
    r.acquire(0);
    L::ActionContext action;
    r.prepare(action, true);
    r.command(action, COUNTER, 0);
    r.command(action, COUNTER, 1);
    const C::SdoTransfer trigger = action.commandHistory[1].transfer;
    const Rig::Failure f = r.failCommand(action, Stage::QUEUED);
    CHECK(r.owner.release(f.id));
    const Saved<L::ActionContext> failed(action);
    const unsigned sent = r.wire.sentCount;
    r.wire.push(reply(trigger), r.now + 1);
    r.wire.push(reply(f.work), r.now + 2);
    r.turn(r.now + 2, budget(8, 0, 8, 0));
    failed.unchanged(action);
    CHECK(r.wire.sentCount == sent && action.valueOneMayRemain);
    r.checkWithdrawn();
    // Stray replies with no request outstanding quarantine the route; nothing here reopens it,
    // and the withdrawn epoch cannot be synchronized onto it.
    CHECK(r.services.unresolved(0));
    CHECK(!r.owner.advanceConfiguration(r.sequence, r.current.channel.binding, r.now));
    CHECK(!r.owner.beginOperation(r.sequence, r.operation + 1, r.deadline, r.now));
}
} // namespace
int main() {
    admissionScope();
    exactlyOneWithdrawalOnSuccess();
    stageMatrix();
    guardRefusalLeavesValidity();
    cancelWhileTriggerInFlight();
    rebootDuringTrigger();
    generationExhaustion();
    lateResponseAfterRelease();
    std::printf("counter clear: %u checks passed\n", checks);
}
