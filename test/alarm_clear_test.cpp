// SPDX-License-Identifier: MIT
// Packet 17: the explicit alarm clear through the real owner and SDO client, with synthetic
// traffic only. Host evidence: no drive has run any of this.
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
const uint16_t CLEAR = 0x2301;
const uint16_t ALARM = L::MONITOR_ALARM;

M::Status attempt(Rig& r, L::ActionContext& action) {
    r.begin();
    return L::prepareClearAlarm(action, r.reservation, r.state, r.operation, r.now, r.deadline,
                                r.policy());
}
// Poll a persisting alarm for `cycles` complete cycles, then let the deadline end the attempt.
void persistUntilDeadline(Rig& r, L::ActionContext& action, unsigned cycles) {
    for (unsigned cycle = 0; cycle < cycles; ++cycle) {
        r.cycle(action, ALARM, 0);
        CHECK(action.state == M::ActionState::ACTIVE && action.phase == L::ActionPhase::WAITING);
        CHECK(!r.next(action).ready);
        r.turn(action.eligibleUs, O::ServiceBudget());
    }
    r.turn(r.deadline, O::ServiceBudget());
    CHECK(L::advanceAction(action, r.reservation, r.current, M::ActionOutcome::CANCELLED, r.now));
    CHECK(action.state == M::ActionState::FAILED && action.outcome == M::ActionOutcome::DEADLINE);
}

// Only FF05 with a reported alarm, mode 1 and running clear is admitted, on either 86C model.
// The code narrows the attempt; it never identifies the alarm.
void admissionScope() {
    for (unsigned model = 0; model < 2; ++model) {
        Rig r;
        if (model)
            r.reservation.model = L::SettingsModel::OL86_C;
        r.acquire(ALARM);
        L::ActionContext action;
        r.prepare(action, false);
        const L::AlarmIncident& incident = action.incident;
        CHECK(incident.present && incident.codeLink == L::AlarmCodeLink::UNRESOLVED);
        CHECK(incident.errorCode == 0xff05 && incident.errorRegister == 1 && incident.monitor == ALARM);
        CHECK(!action.baseline.present && !action.valueOneMayRemain && !action.coordinatesInvalidated);
        CHECK(action.actionKind == M::ActionKind::CLEAR_ALARM && action.phase == L::ActionPhase::COMMAND);
    }
    // FF01 is the only other named code, FF02..FF04 are never derived from LED counts, and zero or
    // an unknown code is not a narrower case.
    const uint16_t codes[] = {0, 0x0005, 0x05ff, 0xff00, 0xff01, 0xff02, 0xff03, 0xff04, 0xff06, 0xffff};
    for (uint16_t code : codes) {
        Rig r;
        r.acquire(ALARM, 1, code);
        L::ActionContext action;
        const Saved<L::ActionContext> empty(action);
        const M::Status status = attempt(r, action);
        CHECK(!status && status.detail == int32_t(M::ActionError::STATE));
        empty.unchanged(action);
    }
    struct Case {
        uint16_t monitor;
        uint8_t mode, mask;
    };
    const uint8_t all = 0x1f, noErrors = 0x1e, noHistory = 0x1d;
    const Case cases[] = {{0, 1, all},
                          {uint16_t(ALARM | L::MONITOR_RUNNING), 1, all},
                          {ALARM, 0, all},
                          {ALARM, 3, all},
                          {ALARM, 6, all},
                          {ALARM, 1, noErrors},
                          {ALARM, 1, noHistory}};
    for (const Case& item : cases) {
        Rig r;
        r.acquire(item.monitor, item.mode, 0xff05, item.mask);
        L::ActionContext action;
        const Saved<L::ActionContext> empty(action);
        const M::Status status = attempt(r, action);
        CHECK(!status && status.detail == int32_t(M::ActionError::STATE));
        empty.unchanged(action);
    }
    // A reservation for another configuration epoch, or an unbound model, is not this read's.
    for (unsigned which = 0; which < 2; ++which) {
        Rig r;
        r.acquire(ALARM);
        r.begin();
        L::AxisReservation stale = r.reservation;
        if (which)
            stale.model = L::SettingsModel::UNKNOWN;
        else
            ++stale.target.channel.binding.configurationGeneration;
        L::ActionContext action;
        const Saved<L::ActionContext> empty(action);
        CHECK(!L::prepareClearAlarm(action, stale, r.state, r.operation, r.now, r.deadline, r.policy()));
        empty.unchanged(action);
    }
}

// A retained 603F=FF05 beside a later alarm admits exactly as a fresh one would. The application
// can see that the code did not change since a report without the alarm, so the code is stale
// evidence; the action stays UNRESOLVED either way and claims nothing about the later cause.
void staleCodeWithLaterIncident() {
    Rig r;
    r.acquire(ALARM);
    L::ActionContext first;
    r.prepare(first, false);
    r.commands(first, false);
    r.cycle(first); // Alarm clear, then the one ERRORS read: 603F still FF05.
    CHECK(first.state == M::ActionState::SUCCEEDED && first.extraObservation.errorCode == 0xff05);
    const Saved<L::ActionContext> firstDone(first);

    const unsigned from = r.wire.sentCount;
    r.acquire(ALARM); // A later incident with an unknown cause; 603F reads the same code.
    L::ActionContext second;
    r.prepare(second, false);
    CHECK(second.incident.codeLink == L::AlarmCodeLink::UNRESOLVED &&
          second.incident.errorCode == first.extraObservation.errorCode &&
          second.incident.readOperation != first.incident.readOperation);
    const Saved<L::AlarmIncident> incident(second.incident);
    const unsigned admitted = r.wire.sentCount;
    r.commands(second, false);
    // The later alarm does not clear. Only MONITORS and STATUS are polled, no write repeats.
    persistUntilDeadline(r, second, 3);
    CHECK(second.execution == M::ActionExecution::ACKNOWLEDGED && second.uncertain &&
          second.completion == M::ActionCompletion::NOT_OBSERVED && !second.valueOneMayRemain);
    CHECK(second.observations == 3 && !second.finalErrorRead);
    r.onlyActionTraffic(from, CLEAR, 2);
    CHECK(r.uploadsOf(admitted, 0x603f) == 0 && r.uploadsOf(admitted, 0x1001) == 0 &&
          r.uploadsOf(admitted, 0x1003) == 0);
    incident.unchanged(second.incident);
    firstDone.unchanged(first);
}

// An observed alarm-clear report succeeds whatever ERRORS then reads, and keeps it raw: the
// value may stay latched, return to zero, or name another code. ERRORS is read once, 1003 never.
void latchedErrorValue() {
    const uint16_t finals[] = {0xff05, 0, 0xff01};
    for (uint16_t code : finals) {
        Rig r;
        r.errorRegister = 0x81;
        r.acquire(ALARM);
        L::ActionContext action;
        r.prepare(action, false);
        const Saved<L::AlarmIncident> incident(action.incident);
        const unsigned admitted = r.wire.sentCount;
        r.commands(action, false);
        r.cycle(action, 0, 0, code);
        CHECK(action.state == M::ActionState::SUCCEEDED && action.outcome == M::ActionOutcome::OBSERVED &&
              action.completion == M::ActionCompletion::OBSERVED && !action.valueOneMayRemain);
        CHECK(action.extraObservation.block == L::StateBlock::ERRORS &&
              action.extraObservation.errorCode == code &&
              action.extraObservation.errorRegister == (code ? 0x81 : 0));
        CHECK(r.uploadsOf(admitted, 0x603f) == 1 && r.uploadsOf(admitted, 0x1001) == 1 &&
              r.uploadsOf(admitted, 0x1003) == 0);
        incident.unchanged(action.incident);
        CHECK(action.incident.codeLink == L::AlarmCodeLink::UNRESOLVED);
        r.onlyActionTraffic(admitted, CLEAR, 2);
    }
}

// The controlword reset route has no path: no stop follows a reset word, and a 6040 terminal
// cannot settle a clear's work even with every identity copied.
void controlwordRouteRejected() {
    const uint16_t words[] = {0x0080, 0x008f, 0x0000};
    for (uint16_t word : words)
        CHECK(!L::stopPredecessorAllowed(word, L::StopKind::HALT) &&
              !L::stopPredecessorAllowed(word, L::StopKind::QUICK_STOP));
    Rig r;
    r.acquire(ALARM);
    L::ActionContext action;
    r.prepare(action, false);
    const unsigned from = r.wire.sentCount;
    const L::PreparedAction work = r.next(action);
    CHECK(work.ready && work.transfer.index == CLEAR && work.transfer.data[0] == 1);
    C::SdoCompletion forged;
    forged.transfer = work.transfer;
    forged.transfer.index = 0x6040;
    forged.transfer.data[0] = 0x80;
    forged.outcome = C::SdoOutcome::SUCCESS;
    forged.admittedUs = forged.completedUs = r.now;
    forged.nodeGeneration = r.current.nodeGeneration;
    forged.queued = forged.transmissionPossible = forged.transmissionConfirmed = true;
    forged.protocolAcknowledged = forged.effectPossible = true;
    const Saved<L::ActionContext> before(action);
    const M::Status status = L::advanceAction(action, r.reservation, r.current, forged, r.now);
    CHECK(!status && status.detail == int32_t(M::ActionError::CORRELATION));
    before.unchanged(action);
    r.commands(action, false);
    r.cycle(action);
    CHECK(action.state == M::ActionState::SUCCEEDED);
    r.onlyActionTraffic(from, CLEAR, 2); // No 6040 byte left the owner.
}

// Each write fails at each boundary. The prefix stays, nothing replays, and the value-1
// consequence is set exactly when the 1 may have landed and the 0 has no positive ACK.
void prefixFailureAtEveryStage() {
    for (unsigned slot = 0; slot < 2; ++slot)
        for (Stage stage : STAGES) {
            Rig r;
            r.acquire(ALARM);
            L::ActionContext action;
            r.prepare(action, false);
            const Saved<L::AlarmIncident> incident(action.incident);
            const unsigned from = r.wire.sentCount;
            if (slot)
                r.command(action, CLEAR, 1);
            const Saved<C::SdoCompletion> prefix(action.commandHistory[0]);
            const Rig::Failure f = r.failCommand(action, stage);
            CHECK(action.state == M::ActionState::FAILED &&
                  action.completion == M::ActionCompletion::NOT_OBSERVED);
            const bool acked = acknowledged(stage);
            CHECK(acked == (f.admitted && f.terminal.outcome == C::SdoOutcome::SUCCESS));
            CHECK(action.outcome == outcomeAt(stage));
            CHECK(action.commandEvidenceMask ==
                  uint8_t((1u << (slot + (f.admitted ? 1 : 0))) - 1));
            if (f.admitted)
                CHECK(action.commandHistory[slot].outcome == f.terminal.outcome);
            if (slot) {
                prefix.unchanged(action.commandHistory[0]);
                CHECK(action.commandHistory[0].protocolAcknowledged &&
                      action.execution == M::ActionExecution::ACKNOWLEDGED);
            } else {
                CHECK(action.execution == (!sent(stage) ? M::ActionExecution::NOT_TRANSMITTED
                                           : stage == Stage::ABORTED ? M::ActionExecution::REJECTED
                                           : acked ? M::ActionExecution::ACKNOWLEDGED
                                                   : M::ActionExecution::UNKNOWN));
            }
            const bool oneLanded = slot || sent(stage);
            CHECK(action.valueOneMayRemain == (oneLanded && !(slot && acked)));
            CHECK(action.uncertain == oneLanded);
            CHECK(r.services.unresolved(0) == ambiguous(stage));
            CHECK(r.wire.sentCount == f.sent + (sent(stage) ? 1u : 0u));
            r.onlyActionTraffic(from, CLEAR, slot + (sent(stage) ? 1u : 0u));
            incident.unchanged(action.incident);
            CHECK(r.axis.originKnown && r.reference.nativeKnown); // An alarm clear moves no epoch.
            if (f.admitted)
                r.lateReplyChangesNothing(action, f);
            // The 1 was never sent a second time.
            unsigned ones = 0;
            for (unsigned i = from; i < r.wire.sentCount; ++i)
                ones += isDownload(r.wire.sent[i]) && r.wire.sent[i].data[4] == 1;
            CHECK(ones == (oneLanded ? 1u : 0u));
        }
}

// The final 0 goes out and its ACK comes late: the route stays unresolved, the late ACK settles
// nothing, the value-1 consequence stays, and no new operation can start on that axis.
void delayedFinalZeroAck() {
    Rig r;
    r.acquire(ALARM);
    L::ActionContext action;
    r.prepare(action, false);
    r.command(action, CLEAR, 1);
    const Rig::Failure f = r.failCommand(action, Stage::TX_REPORTED);
    CHECK(f.terminal.outcome == C::SdoOutcome::TIMEOUT && f.terminal.effectUncertain &&
          f.terminal.transmissionConfirmed);
    CHECK(action.valueOneMayRemain && action.execution == M::ActionExecution::ACKNOWLEDGED &&
          action.outcome == M::ActionOutcome::SERVICE_TIMEOUT);
    r.lateReplyChangesNothing(action, f);
    CHECK(r.services.unresolved(0));
    CHECK(r.owner.release(f.id));
    CHECK(!r.owner.beginOperation(r.sequence, r.operation + 1, r.deadline, r.now));
    CHECK(r.services.unresolved(0) && action.valueOneMayRemain);
}

// A boot-up while the final 0 is in flight: the application ends the attempt INVALIDATED. The
// 0 has no ACK, so the value-1 consequence stays; the incident and the 1's ACK stay too.
// The same with the 1 in flight: its noted possible transmission alone sets the consequence,
// though no terminal for it is ever consumed.
void rebootDuringClear() {
    for (unsigned zero = 0; zero < 2; ++zero) {
        Rig r;
        r.acquire(ALARM);
        L::ActionContext action;
        r.prepare(action, false);
        if (zero)
            r.command(action, CLEAR, 1);
        const Saved<C::SdoCompletion> one(action.commandHistory[0]);
        const Saved<L::AlarmIncident> incident(action.incident);
        const L::PreparedAction work = r.next(action);
        const O::RequestId id = r.admit(work.transfer);
        r.turn(r.now);
        CHECK(action.transmissionNoted && action.effectsPossible);
        M::Frame boot = frame(0x701, 1);
        boot.data[0] = 0;
        r.wire.push(boot, r.now + 1);
        r.turn(r.now + 1, budget(8, 0, 8, 0));
        const C::SdoCompletion terminal = r.result(id);
        CHECK(terminal.outcome == C::SdoOutcome::REBOOT);
        // The lifecycle callback moved the node epoch, so this work no longer correlates.
        CHECK(!L::advanceAction(action, r.reservation, r.current, terminal, r.now));
        CHECK(L::advanceAction(action, r.reservation, r.current, M::ActionOutcome::INVALIDATED, r.now));
        CHECK(action.outcome == M::ActionOutcome::INVALIDATED && action.valueOneMayRemain &&
              action.commandEvidenceMask == zero);
        CHECK(action.execution ==
              (zero ? M::ActionExecution::ACKNOWLEDGED : M::ActionExecution::UNKNOWN));
        one.unchanged(action.commandHistory[0]);
        incident.unchanged(action.incident);
    }
}

// A new EMCY on this node during the clear: the application withdraws first, and the terminal
// in flight is kept as evidence. The owner keeps only its newest EMCY, so the application's
// own copy of the original is what survives. An EMCY 0000 is retained like any other.
void emcyDuringClear() {
    for (unsigned when = 0; when < 3; ++when) {
        Rig r;
        r.wire.push(emcyFrame(1, 0xff05, 0xa5), r.now + 1);
        r.turn(r.now + 1);
        const C::NetworkEvent original = r.services.node(0)->emcy;
        r.acquire(ALARM);
        L::ActionContext action;
        r.prepare(action, false);
        r.command(action, CLEAR, 1);
        const unsigned settled = when == 0 ? 0 : 2; // Fields of the cycle that settle first.
        if (when)
            r.command(action, CLEAR, 0);
        for (unsigned i = 0; i < settled; ++i) {
            const L::PreparedAction work = r.next(action);
            r.field(action, r.raw(work.transfer, 0, 1, 0xff05, 0));
        }
        const L::PreparedAction work = r.next(action);
        const O::RequestId id = r.admit(work.transfer);
        r.turn(r.now);
        const uint16_t code = when == 2 ? 0x0000 : 0xff05;
        r.wire.push(emcyFrame(1, code, 0x5a), r.now + 1);
        const bool download = work.transfer.direction == C::SdoDirection::DOWNLOAD;
        r.wire.push(reply(work.transfer, download ? 0 : r.raw(work.transfer, 0, 1, 0xff05, 0)), r.now + 1);
        r.turn(r.now + 1, budget(8, 0, 8, 0));
        CHECK(r.services.node(0)->emcyCount == 2 && r.services.node(0)->emcy.errorCode == code);
        const C::SdoCompletion terminal = r.result(id);
        if (when == 2) {
            // Application policy here: EMCY 0000 is not a new fault. The attempt goes on and
            // still needs its own observations; the EMCY is not recovery evidence.
            CHECK(L::advanceAction(action, r.reservation, r.current, terminal, r.now));
            r.cycle(action);
            CHECK(action.state == M::ActionState::SUCCEEDED);
        } else {
            CHECK(L::noteActionWithdrawal(action, r.reservation, r.current,
                                          M::ActionOutcome::DRIVE_FAULT, r.now));
            CHECK(L::advanceAction(action, r.reservation, r.current, terminal, r.now));
            CHECK(action.state == M::ActionState::FAILED &&
                  action.outcome == M::ActionOutcome::DRIVE_FAULT &&
                  action.execution == M::ActionExecution::ACKNOWLEDGED);
            // A clear withdrawn with its 0 in flight: that 0's ACK still counts.
            CHECK(action.valueOneMayRemain == false);
            if (when == 0)
                CHECK(action.commandHistory[1].outcome == C::SdoOutcome::SUCCESS);
            else
                CHECK(action.observationEvidencePresent &&
                      action.observationEvidence.transfer.index == terminal.transfer.index &&
                      action.observationEvidence.transfer.step == terminal.transfer.step &&
                      action.observationEvidence.completedUs == terminal.completedUs);
            CHECK(!r.next(action).ready);
        }
        CHECK(original.errorCode == 0xff05 && original.manufacturer[4] == 0xa5);
        CHECK(r.services.node(0)->emcy.manufacturer[4] == 0x5a);
    }
}

// A second node's EMCY and boot-up change nothing in node 1's clear, and node 2's alarm read
// cannot admit a clear on node 1's reservation.
void twoNodeIsolation() {
    Rig r;
    r.addSecondNode();
    // Node 2's own alarm read, on its own reservation and route.
    CHECK(r.owner.beginOperation(r.sequence2, 1, r.deadline, r.now));
    L::ReadContext other;
    CHECK(L::prepareState(other, r.current2, 1, r.now, r.deadline, options(), 0x0f));
    while (other.state == M::ReadState::ACTIVE) {
        L::PreparedRead work;
        CHECK(L::nextRead(other, r.now, work) && work.ready);
        const C::SdoCompletion terminal =
            r.exchange(work.transfer, r.raw(work.transfer, ALARM, 1, 0xff05, 0), false, &r.sequence2);
        CHECK(L::advanceRead(other, terminal, r.now));
    }
    CHECK(other.state == M::ReadState::SUCCEEDED);
    r.acquire(ALARM);
    L::ActionContext wrong;
    const Saved<L::ActionContext> empty(wrong);
    CHECK(!L::prepareClearAlarm(wrong, r.reservation, other, r.operation, r.now, r.deadline, r.policy()));
    empty.unchanged(wrong);
    CHECK(!L::prepareClearAlarm(wrong, r.reservation2, r.state, 1, r.now, r.deadline, r.policy()));
    empty.unchanged(wrong);
    L::ActionContext nodeTwo;
    CHECK(L::prepareClearAlarm(nodeTwo, r.reservation2, other, 1, r.now, r.deadline, r.policy()));
    CHECK(nodeTwo.reservation.target.channel.clientToServerId == 0x602);

    L::ActionContext action;
    r.prepare(action, false);
    const unsigned from = r.wire.sentCount;
    r.command(action, CLEAR, 1);
    r.wire.push(emcyFrame(2, 0xff05), r.now + 1);
    M::Frame boot = frame(0x702, 1);
    boot.data[0] = 0;
    r.wire.push(boot, r.now + 1);
    r.turn(r.now + 1, budget(8, 0, 8, 0));
    CHECK(r.services.node(1)->emcyCount == 1 && r.services.node(0)->emcyCount == 0);
    CHECK(r.invalidations == 0 && r.invalidations2 == 1);
    r.command(action, CLEAR, 0);
    r.cycle(action);
    CHECK(action.state == M::ActionState::SUCCEEDED && !action.valueOneMayRemain);
    r.onlyActionTraffic(from, CLEAR, 2);
    CHECK(r.axis.originKnown && r.reference.nativeKnown);
    // Node 2's boot-up withdraws its own prepared clear, and nothing ever went to 0x602.
    L::ReadTarget moved2 = r.current2;
    moved2.nodeGeneration = r.services.node(1)->route.generation;
    CHECK(moved2.nodeGeneration != r.current2.nodeGeneration);
    L::PreparedAction work;
    CHECK(!L::nextAction(nodeTwo, r.reservation2, moved2, r.now, work));
    CHECK(L::advanceAction(nodeTwo, r.reservation2, moved2, M::ActionOutcome::INVALIDATED, r.now));
    CHECK(nodeTwo.outcome == M::ActionOutcome::INVALIDATED && !nodeTwo.valueOneMayRemain &&
          nodeTwo.execution == M::ActionExecution::NOT_TRANSMITTED);
    for (unsigned i = 0; i < r.wire.sentCount; ++i)
        CHECK(!(r.wire.sent[i].id == 0x602 && isDownload(r.wire.sent[i])));
}

// The value-1 flag is part of the checked shape: a context whose flag disagrees with its own
// command history is refused by every call, so a caller cannot clear it or forge it.
void valueOneFlagIsChecked() {
    Rig r;
    r.acquire(ALARM);
    L::ActionContext action;
    r.prepare(action, false);
    L::PreparedAction work;
    action.valueOneMayRemain = true; // On an active context.
    CHECK(!L::nextAction(action, r.reservation, r.current, r.now, work));
    action.valueOneMayRemain = false;
    r.command(action, CLEAR, 1);
    CHECK(L::advanceAction(action, r.reservation, r.current, M::ActionOutcome::CANCELLED, r.now));
    CHECK(action.valueOneMayRemain);
    CHECK(L::nextAction(action, r.reservation, r.current, r.now, work) && !work.ready);
    action.valueOneMayRemain = false; // Cleared by a caller on a failed context.
    const Saved<L::PreparedAction> blank(work);
    CHECK(!L::nextAction(action, r.reservation, r.current, r.now, work));
    blank.unchanged(work);
    action.valueOneMayRemain = true;
    Rig s;
    s.acquire(ALARM);
    L::ActionContext done;
    s.prepare(done, false);
    s.commands(done, false);
    s.cycle(done);
    CHECK(done.state == M::ActionState::SUCCEEDED);
    done.valueOneMayRemain = true; // Forged on a success.
    CHECK(!L::nextAction(done, s.reservation, s.current, s.now, work));
}

// Cancellation and deadline races around the write in flight.
void cancellationAndDeadlineRaces() {
    // Cancel while the 1 is in flight; its ACK arrives anyway. The 0 is never sent.
    {
        Rig r;
        r.acquire(ALARM);
        L::ActionContext action;
        r.prepare(action, false);
        const unsigned from = r.wire.sentCount;
        const L::PreparedAction work = r.next(action);
        const O::RequestId id = r.admit(work.transfer);
        r.turn(r.now);
        CHECK(L::noteActionWithdrawal(action, r.reservation, r.current, M::ActionOutcome::CANCELLED, r.now));
        CHECK(!r.next(action).ready);
        r.wire.push(reply(work.transfer), r.now + 1);
        r.turn(r.now + 1, budget(8, 0, 8, 0));
        CHECK(L::advanceAction(action, r.reservation, r.current, r.result(id), r.now));
        CHECK(action.outcome == M::ActionOutcome::CANCELLED &&
              action.execution == M::ActionExecution::ACKNOWLEDGED && action.valueOneMayRemain);
        r.turn(r.now + 1000);
        r.onlyActionTraffic(from, CLEAR, 1);
    }
    // A withdrawal noted at the cutoff is the deadline.
    {
        Rig r;
        r.acquire(ALARM);
        L::ActionContext action;
        r.prepare(action, false);
        r.command(action, CLEAR, 1);
        const L::PreparedAction work = r.next(action);
        const O::RequestId id = r.admit(work.transfer);
        r.turn(r.now);
        r.turn(r.deadline, O::ServiceBudget());
        CHECK(L::noteActionWithdrawal(action, r.reservation, r.current, M::ActionOutcome::CANCELLED, r.now));
        CHECK(action.withdrawal == M::ActionOutcome::DEADLINE);
        r.turn(r.now, budget(8, 0, 8, 0)); // The 0's response budget ran out long ago.
        CHECK(L::advanceAction(action, r.reservation, r.current, r.result(id), r.now));
        CHECK(action.outcome == M::ActionOutcome::DEADLINE && action.valueOneMayRemain);
    }
    // A local DEADLINE before the cutoff is refused; at the cutoff it ends the attempt.
    {
        Rig r;
        r.acquire(ALARM);
        L::ActionContext action;
        r.prepare(action, false);
        const Saved<L::ActionContext> fresh(action);
        CHECK(!L::advanceAction(action, r.reservation, r.current, M::ActionOutcome::DEADLINE, r.now));
        fresh.unchanged(action);
        r.turn(r.deadline, O::ServiceBudget());
        CHECK(L::advanceAction(action, r.reservation, r.current, M::ActionOutcome::DEADLINE, r.now));
        CHECK(action.outcome == M::ActionOutcome::DEADLINE && !action.valueOneMayRemain &&
              action.execution == M::ActionExecution::NOT_TRANSMITTED);
    }
}

// A configuration or lifecycle change after preparation makes the frozen work unusable: every
// call refuses atomically until the application ends it INVALIDATED.
void staleConfigurationAfterPreparation() {
    for (unsigned which = 0; which < 2; ++which) {
        Rig r;
        r.acquire(ALARM);
        L::ActionContext action;
        r.prepare(action, false);
        r.command(action, CLEAR, 1);
        L::ReadTarget moved = r.current;
        if (which)
            ++moved.nodeGeneration;
        else
            ++moved.channel.binding.configurationGeneration;
        const Saved<L::ActionContext> before(action);
        L::PreparedAction work;
        const Saved<L::PreparedAction> blank(work);
        CHECK(!L::nextAction(action, r.reservation, moved, r.now, work));
        blank.unchanged(work);
        CHECK(!L::noteActionTransmission(action, r.reservation, moved, r.now));
        CHECK(!L::advanceAction(action, r.reservation, moved, M::ActionOutcome::CANCELLED, r.now));
        before.unchanged(action);
        CHECK(L::advanceAction(action, r.reservation, moved, M::ActionOutcome::INVALIDATED, r.now));
        CHECK(action.outcome == M::ActionOutcome::INVALIDATED && action.valueOneMayRemain &&
              action.commandEvidenceMask == 1);
    }
}
} // namespace
int main() {
    admissionScope();
    staleCodeWithLaterIncident();
    latchedErrorValue();
    controlwordRouteRejected();
    prefixFailureAtEveryStage();
    delayedFinalZeroAck();
    rebootDuringClear();
    emcyDuringClear();
    twoNodeIsolation();
    valueOneFlagIsChecked();
    cancellationAndDeadlineRaces();
    staleConfigurationAfterPreparation();
    std::printf("alarm clear: %u checks passed\n", checks);
}
