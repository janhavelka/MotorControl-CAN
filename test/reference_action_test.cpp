// SPDX-License-Identifier: MIT
// Real service/owner integration with synthetic controller traffic only.
// No physical drive, firmware qualification or implicit enabling is represented.
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
void successfulActionsRetainEvidence() {
    for (unsigned position = 0; position < 2; ++position) {
        Rig r;
        if (position)
            r.reservation.model = L::SettingsModel::OL86_C;
        M::Frame emcy = frame(0x81, 8);
        emcy.data[0] = 5;
        emcy.data[1] = 0xff;
        emcy.data[2] = 1;
        emcy.data[7] = 0xa5;
        r.wire.push(emcy, r.now + 1);
        r.turn(r.now + 1);
        CHECK(r.services.node(0)->emcyCount == 1);
        const Saved<C::NetworkEvent> fault(r.services.node(0)->emcy);
        // Historical EMCY remains retained after a newer explicit state read.
        // A new fault after that read instead withdraws admission/completion.
        r.acquire(position ? 0 : 8);
        L::ActionContext action;
        const unsigned sent = r.wire.sentCount;
        r.prepare(action, position != 0);
        CHECK(action.incident.present == (position == 0));
        if (!position) {
            const L::AlarmIncident& incident = action.incident;
            CHECK(incident.readOperation == r.state.operation && incident.errorCode == 0xff05);
            CHECK(incident.errorCount == 2 && incident.errorHistory[0] == 0x12340001 &&
                  incident.errorHistory[1] == 0x12340002);
            CHECK(incident.earliestUs >= r.state.startedUs && incident.latestUs <= r.state.servicedUs);
            const L::StateBlock kept[] = {L::StateBlock::ERRORS, L::StateBlock::ERROR_HISTORY,
                                          L::StateBlock::MONITORS, L::StateBlock::STATUS};
            uint64_t earliest = UINT64_MAX, latest = 0;
            for (L::StateBlock block : kept) {
                const unsigned first = r.state.blockStart[unsigned(block)];
                for (unsigned i = first; i < first + r.state.blockLength[unsigned(block)]; ++i) {
                    const auto& value = r.state.values[i];
                    if (!value.valid)
                        continue;
                    earliest = value.provenance.earliestUs < earliest ? value.provenance.earliestUs : earliest;
                    latest = value.provenance.latestUs > latest ? value.provenance.latestUs : latest;
                }
            }
            CHECK(incident.earliestUs == earliest && incident.latestUs == latest);
            CHECK(incident.statusword == uint16_t(r.state.values[r.state.blockStart[unsigned(L::StateBlock::STATUS)]].raw));
            CHECK(incident.modeRaw == 1);
            const unsigned monitors = r.state.blockStart[unsigned(L::StateBlock::MONITORS)];
            CHECK(incident.monitor == uint16_t(r.state.values[monitors].raw) &&
                  incident.logicalInputs == uint16_t(r.state.values[monitors + 1].raw) &&
                  incident.logicalOutputs == uint16_t(r.state.values[monitors + 2].raw));
        }
        const Saved<L::AlarmIncident> original(action.incident);
        r.commands(action, position != 0);
        CHECK(action.execution == M::ActionExecution::ACKNOWLEDGED &&
              action.completion == M::ActionCompletion::NOT_OBSERVED);
        if (position)
            r.checkWithdrawn();
        const Saved<C::SdoCompletion> trigger(action.commandHistory[position ? 1 : 0]);
        r.cycle(action);
        CHECK(action.state == M::ActionState::SUCCEEDED &&
              action.outcome == M::ActionOutcome::OBSERVED && action.observations == 1 &&
              action.completion == M::ActionCompletion::OBSERVED);
        CHECK(r.wire.sentCount == sent + (position ? 10 : 9));
        trigger.unchanged(action.commandHistory[position ? 1 : 0]);
        original.unchanged(action.incident);
        fault.unchanged(r.services.node(0)->emcy);
        CHECK(r.services.node(0)->emcyCount == 1);
        if (position) {
            CHECK(action.extraObservation.actualPosition == 0);
            r.checkWithdrawn();
        } else {
            CHECK(action.extraObservation.errorCode == 0xff05 &&
                  action.monitorObservation.monitor == 0);
            CHECK(r.axis.originKnown && r.reference.nativeKnown &&
                  r.axis.binding.configurationGeneration == 7);
        }
        const Saved<L::ActionContext> terminal(action);
        CHECK(!r.next(action).ready);
        CHECK(!L::advanceAction(action, r.reservation, r.current, M::ActionOutcome::CANCELLED,
                                r.now));
        terminal.unchanged(action);
        for (unsigned i = sent; i < r.wire.sentCount; ++i) {
            const M::Frame& actual = r.wire.sent[i];
            CHECK(actual.id != 0 && !(actual.data[1] == 0x40 && actual.data[2] == 0x60));
        }
    }
}

void unsupportedValuesAndPoliciesAreAtomic() {
    Rig r;
    r.acquire(0);
    r.begin();
    L::ActionContext action;
    const Saved<L::ActionContext> before(action);
    const int64_t values[] = {INT64_MIN, -1, 1, INT64_MAX};
    for (int64_t value : values) {
        CHECK(L::prepareSetDevicePosition(action, r.reservation, r.state, value, r.axis,
                                          r.operation, r.now, r.deadline, r.policy())
                  .code == M::Err::UNSUPPORTED);
        before.unchanged(action);
    }
    for (unsigned position = 0; position < 2; ++position)
        for (unsigned scenario = 0; scenario < 9; ++scenario) {
            Rig bad;
            const uint16_t monitor = scenario == 0   ? 4
                                     : scenario == 1 ? (position ? 8 : 0)
                                     : position      ? 0
                                                     : 8;
            bad.acquire(monitor, scenario == 2 ? 3 : 1,
                        scenario == 3 && !position ? 0xff04 : 0xff05,
                        scenario == 4 && !position ? 0x1d : 0x1f);
            bad.begin();
            L::AxisReservation live = bad.reservation;
            M::AxisConfig axis = bad.axis;
            auto options = bad.policy();
            if (scenario == 5)
                live.model = L::SettingsModel::UNKNOWN;
            if (scenario == 6)
                options.maximumAgeUs = 0;
            if (scenario == 7)
                ++live.target.nodeGeneration;
            if (scenario == 8)
                live.acquiredUs = bad.now + 1;
            // Position admission deliberately does not require zero historical
            // 603F or an error-history block; those scenarios apply to clear only.
            if (position && (scenario == 3 || scenario == 4))
                continue;
            M::Status status =
                position
                    ? L::prepareSetDevicePosition(action, live, bad.state, 0, axis, bad.operation,
                                                  bad.now, bad.deadline, options)
                    : L::prepareClearAlarm(action, live, bad.state, bad.operation, bad.now,
                                           bad.deadline, options);
            CHECK(!status);
            before.unchanged(action);
        }
    for (unsigned which = 0; which < 2; ++which) {
        M::AxisConfig exhausted = r.axis;
        if (which)
            exhausted.referenceGeneration = UINT32_MAX;
        else
            exhausted.exhausted = true;
        CHECK(!L::prepareSetDevicePosition(action, r.reservation, r.state, 0, exhausted,
                                           r.operation, r.now, r.deadline, r.policy()));
        before.unchanged(action);
    }
    for (unsigned which = 0; which < 2; ++which) {
        Rig exhausted(which ? 7 : UINT32_MAX, which ? UINT32_MAX : 1);
        exhausted.acquire(0);
        exhausted.begin();
        const Saved<M::AxisConfig> axis(exhausted.axis);
        CHECK(!L::prepareSetDevicePosition(action, exhausted.reservation, exhausted.state, 0,
                                           exhausted.axis, exhausted.operation, exhausted.now,
                                           exhausted.deadline, exhausted.policy()));
        before.unchanged(action);
        axis.unchanged(exhausted.axis);
    }
}

void pendingObservationsAndDeadline() {
    for (unsigned position = 0; position < 2; ++position) {
        Rig r;
        r.acquire(position ? 0 : 8);
        L::ActionContext action;
        r.prepare(action, position != 0);
        r.commands(action, position != 0);
        r.cycle(action, position ? 0 : 8, position ? 123 : 0);
        CHECK(action.state == M::ActionState::ACTIVE && action.phase == L::ActionPhase::WAITING &&
              action.completion == M::ActionCompletion::NOT_OBSERVED);
        const unsigned sent = r.wire.sentCount;
        CHECK(!r.next(action).ready);
        r.turn(action.eligibleUs, O::ServiceBudget());
        r.cycle(action, position ? 0 : 8, position ? -123 : 0);
        CHECK(action.state == M::ActionState::ACTIVE && action.observations == 2);
        r.turn(r.deadline, O::ServiceBudget());
        CHECK(
            L::advanceAction(action, r.reservation, r.current, M::ActionOutcome::CANCELLED, r.now));
        CHECK(action.outcome == M::ActionOutcome::DEADLINE &&
              action.execution == M::ActionExecution::ACKNOWLEDGED && action.uncertain);
        // Alarm polling is MONITORS+STATUS only: ERRORS waits until the alarm has cleared.
        CHECK(r.wire.sentCount == sent + (position ? 7u : 5u));
        for (unsigned i = sent; i < r.wire.sentCount && !position; ++i)
            CHECK(!(r.wire.sent[i].data[1] == 0x01 && r.wire.sent[i].data[2] == 0x10) &&
                  !(r.wire.sent[i].data[1] == 0x3f && r.wire.sent[i].data[2] == 0x60));
        if (position)
            r.checkWithdrawn();
    }
}

void writeFailuresPreservePrefixes() {
    for (unsigned position = 0; position < 2; ++position)
        for (unsigned prefix = 0; prefix < (position ? 3u : 2u); ++prefix)
            for (unsigned failure = 0; failure < 4; ++failure) {
                Rig r;
                r.acquire(position ? 0 : 8);
                L::ActionContext action;
                r.prepare(action, position != 0);
                for (unsigned i = 0; i < prefix; ++i)
                    r.command(action, position ? 0x2302 : 0x2301,
                              position ? (i == 1 ? 1 : 0) : (i == 0 ? 1 : 0));
                const Saved<L::AlarmIncident> initial(action.incident);
                const auto work = r.next(action);
                const auto id = r.admit(work.transfer);
                const unsigned sent = r.wire.sentCount;
                // No-TX cancel is separate from every possible-TX terminal.
                if (failure != 0)
                    r.turn(r.now);
                if (failure == 2) {
                    r.wire.push(reply(work.transfer, 0x06090030, true), r.now + 1);
                    r.turn(r.now + 1, budget(8, 0, 8, 0));
                } else if (failure == 3) {
                    r.turn(r.now + 101, O::ServiceBudget());
                } else {
                    CHECK(r.owner.cancel(id, r.now) == O::Cancel::CANCELLED);
                }
                const C::SdoCompletion terminal = r.result(id);
                CHECK(L::advanceAction(action, r.reservation, r.current, terminal, r.now));
                CHECK(action.state == M::ActionState::FAILED &&
                      action.completion == M::ActionCompletion::NOT_OBSERVED);
                CHECK(action.commandEvidenceMask == uint8_t((1u << (prefix + 1)) - 1));
                for (unsigned i = 0; i < prefix; ++i)
                    CHECK(action.commandHistory[i].protocolAcknowledged);
                CHECK(action.commandHistory[prefix].outcome == terminal.outcome);
                CHECK(r.wire.sentCount == sent + (failure ? 1u : 0u));
                initial.unchanged(action.incident);
                if (position && (prefix > 1 || (prefix == 1 && failure)))
                    r.checkWithdrawn();
                else
                    CHECK(r.axis.originKnown && r.reference.nativeKnown &&
                          r.axis.binding.configurationGeneration == 7);
                if (position && prefix == 2)
                    CHECK(action.commandHistory[1].protocolAcknowledged &&
                          action.execution == M::ActionExecution::ACKNOWLEDGED);
                if (!position && prefix == 1)
                    CHECK(action.commandHistory[0].protocolAcknowledged &&
                          action.execution == M::ActionExecution::ACKNOWLEDGED);
                const Saved<L::ActionContext> failed(action);
                CHECK(!r.next(action).ready);
                r.wire.push(reply(work.transfer), r.now + 1); // Late reply cannot rewrite terminal.
                r.turn(r.now + 1, budget(8, 0, 8, 0));
                failed.unchanged(action);
                CHECK(r.wire.sentCount == sent + (failure ? 1u : 0u));
            }
}

void transmissionNoticeAndStaleAdmissionAreAtomic() {
    Rig r;
    r.acquire(0);
    L::ActionContext action;
    r.prepare(action, true);
    r.command(action, 0x2302, 0);
    const Saved<L::ActionContext> saved(action);
    const Saved<L::ReadTarget> targetSaved(r.current);
    const Saved<M::AxisReference> refSaved(r.reference);
    CHECK(!L::noteActionTransmission(action, r.reservation, r.current, r.now));
    saved.unchanged(action);
    CHECK(!L::noteActionTransmission(action, action.reservation, r.current, r.axis, r.reference,
                                     r.now));
    saved.unchanged(action);
    CHECK(!L::noteActionTransmission(action, r.reservation, action.reservation.target, r.axis,
                                     r.reference, r.now));
    saved.unchanged(action);
    for (unsigned fault = 0; fault < 4; ++fault) {
        M::AxisConfig stale = r.axis;
        if (fault == 0)
            ++stale.referenceGeneration;
        if (fault == 1)
            ++stale.binding.configurationGeneration;
        if (fault == 2)
            stale.exhausted = true;
        if (fault == 3)
            stale.referenceGeneration = UINT32_MAX;
        const Saved<M::AxisConfig> axisSaved(stale);
        CHECK(!L::noteActionTransmission(action, r.reservation, r.current, stale, r.reference,
                                         r.now));
        saved.unchanged(action);
        targetSaved.unchanged(r.current);
        refSaved.unchanged(r.reference);
        axisSaved.unchanged(stale);
    }
    r.command(action, 0x2302, 1);
    r.checkWithdrawn();
    const Saved<L::ActionContext> triggered(action);
    L::PreparedAction output;
    const Saved<L::PreparedAction> oldOutput(output);
    CHECK(!L::nextAction(action, r.reservation, target(), r.now, output));
    oldOutput.unchanged(output);
    triggered.unchanged(action);
    CHECK(L::advanceAction(action, r.reservation, r.current, M::ActionOutcome::CANCELLED, r.now));
    r.checkWithdrawn();
    CHECK(!r.next(action).ready);
}

void interruptedResultSurvivesClear() {
    Rig r;
    r.acquire(8);
    r.begin();
    L::ReadContext interrupted;
    CHECK(L::prepareState(interrupted, r.current, r.operation, r.now, r.deadline, options(),
                          uint8_t(1u << unsigned(L::StateBlock::STATUS))));
    L::PreparedRead read;
    CHECK(L::nextRead(interrupted, r.now, read) && read.ready);
    const auto id = r.admit(read.transfer);
    CHECK(r.owner.cancel(id, r.now) == O::Cancel::CANCELLED);
    const O::Completion* result = r.owner.result(id);
    CHECK(result && result->service.outcome == C::SdoOutcome::CANCELLED &&
          !result->service.transmissionPossible);
    const Saved<O::Completion> saved(*result);
    CHECK(L::advanceRead(interrupted, result->service, r.now));
    const Saved<L::ReadContext> failedRead(interrupted);
    L::ActionContext action;
    r.prepare(action, false);
    r.commands(action, false);
    r.cycle(action);
    CHECK(action.state == M::ActionState::SUCCEEDED);
    CHECK(r.owner.result(id));
    saved.unchanged(*r.owner.result(id));
    failedRead.unchanged(interrupted);
    CHECK(r.owner.release(id));
}

void deadlinesFaultsAndRebootKeepOriginalTerminals() {
    for (unsigned position = 0; position < 2; ++position)
        for (unsigned event = 0; event < 3; ++event) {
            Rig r;
            r.acquire(position ? 0 : 8);
            L::ActionContext action;
            r.prepare(action, position != 0);
            r.commands(action, position != 0);
            // Finish all but the final observation; newer fault/deadline/reset
            // wins even though the earlier monitor field reported no alarm.
            for (unsigned i = 0; i < 6; ++i) {
                const auto work = r.next(action);
                r.field(action, r.raw(work.transfer, 0, 1, 0xff05, 0));
            }
            const auto work = r.next(action);
            CHECK(work.ready && work.transfer.direction == C::SdoDirection::UPLOAD);
            if (event == 0)
                r.turn(r.deadline - 1, O::ServiceBudget());
            const auto id = r.admit(work.transfer);
            r.turn(r.now);
            const Saved<C::SdoCompletion> command(action.commandHistory[position ? 1 : 0]);
            if (event == 0) {
                r.wire.push(reply(work.transfer, position ? 0 : 1), r.deadline);
                r.turn(r.deadline, budget(8, 0, 8, 0));
                const auto terminal = r.result(id);
                CHECK(L::advanceAction(action, r.reservation, r.current, terminal, r.now));
                CHECK(action.outcome == M::ActionOutcome::DEADLINE);
            } else {
                M::Frame urgent = frame(event == 1 ? 0x81 : 0x701, event == 1 ? 8 : 1);
                if (event == 1) {
                    urgent.data[0] = 5;
                    urgent.data[1] = 0xff;
                    urgent.data[2] = 1;
                }
                r.wire.push(urgent, r.now + 1);
                r.turn(r.now + 1, budget(8, 0, 8, 0));
                if (event == 1) {
                    CHECK(r.services.node(0)->emcyCount == 1);
                    CHECK(r.owner.cancel(id, r.now) == O::Cancel::CANCELLED);
                }
                const auto terminal = r.result(id);
                CHECK(terminal.outcome ==
                      (event == 1 ? C::SdoOutcome::CANCELLED : C::SdoOutcome::REBOOT));
                CHECK(L::advanceAction(action, r.reservation, r.current,
                                       event == 1 ? M::ActionOutcome::DRIVE_FAULT
                                                  : M::ActionOutcome::INVALIDATED,
                                       r.now));
                CHECK(action.outcome ==
                      (event == 1 ? M::ActionOutcome::DRIVE_FAULT : M::ActionOutcome::INVALIDATED));
                const Saved<L::ActionContext> failed(action);
                const unsigned sent = r.wire.sentCount;
                r.wire.push(reply(work.transfer, position ? 0 : 1), r.now + 1);
                r.turn(r.now + 1, budget(8, 0, 8, 0));
                failed.unchanged(action);
                CHECK(r.wire.sentCount == sent);
            }
            CHECK(action.state == M::ActionState::FAILED &&
                  action.completion == M::ActionCompletion::NOT_OBSERVED && action.uncertain);
            command.unchanged(action.commandHistory[position ? 1 : 0]);
            if (position)
                CHECK(!r.axis.originKnown && !r.reference.nativeKnown &&
                      !M::validatePreparedTarget(r.prepared, r.axis, nullptr, r.now));
        }
}

void conservativeNoticeThenDefiniteNoTransmission() {
    Rig r;
    r.acquire(0);
    L::ActionContext action;
    r.prepare(action, true);
    r.command(action, 0x2302, 0);
    const auto work = r.next(action);
    // A conservative pre-TX withdrawal may precede an owner-side no-TX
    // cancellation. Its terminal refines execution without renewing coordinates.
    CHECK(L::noteActionTransmission(action, r.reservation, r.current, r.axis, r.reference, r.now));
    r.checkWithdrawn();
    const auto id = r.admit(work.transfer);
    CHECK(r.owner.cancel(id, r.now) == O::Cancel::CANCELLED);
    const auto terminal = r.result(id);
    CHECK(!terminal.transmissionPossible);
    CHECK(L::advanceAction(action, r.reservation, r.current, terminal, r.now));
    CHECK(action.state == M::ActionState::FAILED &&
          action.execution == M::ActionExecution::NOT_TRANSMITTED && action.coordinatesInvalidated);
    r.checkWithdrawn();
}

void failedCounterActionThenExplicitNewEpochRead() {
    Rig r;
    r.acquire(0);
    L::ActionContext action;
    r.prepare(action, true);
    r.command(action, 0x2302, 0);
    r.command(action, 0x2302, 1);
    const auto rearm = r.next(action);
    const auto id = r.admit(rearm.transfer);
    const unsigned sent = r.wire.sentCount;
    CHECK(r.owner.cancel(id, r.now) == O::Cancel::CANCELLED);
    const auto terminal = r.result(id, false);
    CHECK(!terminal.transmissionPossible && !r.services.unresolved(0));
    CHECK(L::advanceAction(action, r.reservation, r.current, terminal, r.now));
    CHECK(action.outcome == M::ActionOutcome::CANCELLED &&
          action.execution == M::ActionExecution::ACKNOWLEDGED && action.uncertain);
    const Saved<L::ActionContext> failed(action);
    const Saved<O::Completion> retained(*r.owner.result(id));
    r.checkWithdrawn();
    // Even a definite no-TX rearm failure keeps the trigger's coordinate loss.
    // An unread old-epoch result prevents synchronization, without erasing it.
    CHECK(!r.owner.advanceConfiguration(r.sequence, r.current.channel.binding, r.now));
    retained.unchanged(*r.owner.result(id));
    CHECK(r.owner.release(id));
    r.synchronize();
    r.begin();
    L::ReadContext read;
    CHECK(L::prepareState(read, r.current, r.operation, r.now, r.deadline, options(),
                          uint8_t(1u << unsigned(L::StateBlock::FEEDBACK))));
    L::PreparedRead fresh;
    CHECK(L::nextRead(read, r.now, fresh) && fresh.ready);
    O::BusRequest stale;
    stale.sequence = r.sequence;
    stale.transfer = fresh.transfer;
    stale.transfer.channel.binding = target().channel.binding;
    O::RequestId rejected;
    const Saved<O::RequestId> unchanged(rejected);
    CHECK(r.owner.admit(stale, r.now, rejected) == O::BusAdmission::INVALID);
    unchanged.unchanged(rejected);
    CHECK(r.wire.sentCount == sent);
    while (read.state == M::ReadState::ACTIVE) {
        CHECK(L::nextRead(read, r.now, fresh) && fresh.ready);
        const auto observed = r.exchange(fresh.transfer);
        CHECK(L::advanceRead(read, observed, r.now));
    }
    L::StateObservation feedback;
    CHECK(L::getStateBlock(read, L::StateBlock::FEEDBACK, r.current, r.now, 1000, feedback));
    CHECK(feedback.actualPosition == 0 && r.wire.sentCount == sent + 2);
    failed.unchanged(action); // A later zero observation cannot settle failed rearm.
    r.checkWithdrawn();       // Nor can it synthesize an origin or home witness.
}

// A matched absent-object abort settles its write and leaves the route open. It does not
// retire the workflow's history: the recorded prefix and the possible effect stay as they were.
void absentObjectAbortLeavesWorkflowHistoryOnAnOpenRoute() {
    Rig r;
    r.acquire(0);
    L::ActionContext action;
    r.prepare(action, true);
    r.command(action, 0x2302, 0);
    const auto write = r.next(action);
    const auto id = r.admit(write.transfer);
    const unsigned sent = r.wire.sentCount;
    r.turn(r.now);
    CHECK(r.wire.sentCount == sent + 1);
    r.wire.push(reply(write.transfer, 0x06020000, true), r.now + 1);
    r.turn(r.now + 1, budget(8, 0, 8, 0));
    const auto terminal = r.result(id);
    CHECK(terminal.outcome == C::SdoOutcome::SERVER_ABORT && terminal.abortCode == 0x06020000);
    CHECK(terminal.effectPossible && terminal.effectUncertain);
    CHECK(!r.services.unresolved(0) && r.owner.sequence(r.sequence));
    CHECK(L::advanceAction(action, r.reservation, r.current, terminal, r.now));
    CHECK(action.state == M::ActionState::FAILED);
    CHECK(action.commandHistory[0].protocolAcknowledged && action.commandHistory[0].effectPossible);
    CHECK(action.commandHistory[1].outcome == C::SdoOutcome::SERVER_ABORT &&
          action.commandHistory[1].effectUncertain);
    const Saved<L::ActionContext> failed(action);
    CHECK(!r.next(action).ready);
    r.turn(r.now + 1000, budget());
    failed.unchanged(action);
    CHECK(r.wire.sentCount == sent + 1); // Open route, and still nothing replays.
}

void uncertainCounterTriggerCannotReopenThroughEpochSync() {
    Rig r;
    r.acquire(0);
    L::ActionContext action;
    r.prepare(action, true);
    r.command(action, 0x2302, 0);
    const auto trigger = r.next(action);
    const auto id = r.admit(trigger.transfer);
    r.wire.autoReport = r.wire.retirement = false;
    const unsigned sent = r.wire.sentCount;
    r.turn(r.now);
    r.checkWithdrawn();
    CHECK(r.wire.pending[sent]);
    r.turn(r.now + 101, budget(0, 0, 8, 0));
    const auto terminal = r.result(id, false);
    CHECK(terminal.outcome == C::SdoOutcome::TIMEOUT && terminal.effectUncertain &&
          r.services.unresolved(0) && r.wire.retireCalls == 1);
    CHECK(L::advanceAction(action, r.reservation, r.current, terminal, r.now));
    CHECK(action.execution == M::ActionExecution::UNKNOWN && action.uncertain);
    const Saved<L::ActionContext> failed(action);
    const Saved<O::Completion> retained(*r.owner.result(id));
    CHECK(!r.owner.advanceConfiguration(r.sequence, r.current.channel.binding, r.now));
    CHECK(!r.owner.beginOperation(r.sequence, r.operation + 1, r.deadline, r.now));
    // Retiring the local controller frame and receiving a late ACK still do
    // not qualify the ambiguous remote SDO channel or replay the trailing zero.
    r.wire.report(sent);
    r.wire.push(reply(trigger.transfer), r.now + 1);
    r.turn(r.now + 1, budget(8, 0, 8, 0));
    retained.unchanged(*r.owner.result(id));
    CHECK(r.owner.release(id));
    CHECK(r.services.unresolved(0) && !r.owner.sequence(r.sequence));
    CHECK(!r.owner.advanceConfiguration(r.sequence, r.current.channel.binding, r.now));
    O::BusRequest request;
    request.sequence = r.sequence;
    request.transfer = trigger.transfer;
    ++request.transfer.step;
    O::RequestId rejected;
    for (unsigned epoch = 0; epoch < 2; ++epoch) {
        request.transfer.channel.binding =
            epoch ? r.current.channel.binding : target().channel.binding;
        CHECK(r.owner.admit(request, r.now, rejected) == O::BusAdmission::INVALID);
    }
    // Remove reservation occupancy as an alternative rejection cause. A new
    // local token still cannot establish a fresh remote transaction boundary.
    CHECK(r.owner.releaseSequence(r.sequence));
    CHECK(M::sameBinding(r.services.channel(0)->binding, target().channel.binding));
    O::SequenceId replacement;
    const Saved<O::SequenceId> unchanged(replacement);
    CHECK(!r.owner.beginSequence(target().channel.binding, 1, r.now, replacement));
    unchanged.unchanged(replacement);
    CHECK(r.services.unresolved(0));
    // Reading the trigger's own object back is the same pair, so it cannot be the boundary either.
    C::SdoTransfer readback = trigger.transfer;
    readback.channel = *r.services.channel(0);
    readback.direction = C::SdoDirection::UPLOAD;
    readback.readEffect = C::SdoReadEffect::ACCEPTED_UNQUALIFIED;
    readback.allowSegmentedUpload = false;
    readback.deadlineUs = r.now + 1000;
    C::SdoDrainPremises premises;
    premises.soleClient = premises.orderedServer = true;
    CHECK(r.owner.admitDrainMarker(readback, premises, r.now, rejected) == O::BusAdmission::INVALID);
    CHECK(r.services.unresolved(0) && r.services.requalifications(0) == 0);
    CHECK(r.wire.sentCount == sent + 1 && !r.wire.pending[sent] && r.invalidations == 0);
    failed.unchanged(action);
    r.checkWithdrawn();
}

void identityWidthsRefusedAtAdmission() {
    for (unsigned position = 0; position < 2; ++position) {
        Rig r;
        r.acquire(position ? 0 : 8);
        r.begin();
        L::ActionOptions measured = r.policy();
        measured.service.identityWidths = L::IdentityWidths::MEASURED_U8;
        L::ActionContext action;
        const Saved<L::ActionContext> empty(action);
        const M::Status refused =
            position ? L::prepareSetDevicePosition(action, r.reservation, r.state, 0, r.axis, r.operation,
                                                   r.now, r.deadline, measured)
                     : L::prepareClearAlarm(action, r.reservation, r.state, r.operation, r.now,
                                            r.deadline, measured);
        CHECK(!refused && refused.detail == int32_t(M::ActionError::ARGUMENT));
        empty.unchanged(action);
    }
}
// The cycle that clears the alarm must prepare the final ERRORS read. That is proved first.
void unpreparableFinalReadRefusesBeforeAnyMutation() {
    Rig r;
    r.acquire(8);
    L::ActionContext action;
    r.prepare(action, false);
    r.commands(action, false);
    const uint32_t values[] = {0, 0xa580, 0xf008, 0xffff, 1};
    for (uint32_t value : values) {
        const L::PreparedAction work = r.next(action);
        const C::SdoCompletion terminal = r.exchange(work.transfer, value);
        action.options.service.identityWidths = L::IdentityWidths::MEASURED_U8;
        const Saved<L::ActionContext> refused(action);
        const M::Status status = L::advanceAction(action, r.reservation, r.current, terminal, r.now);
        CHECK(!status && status.detail == int32_t(M::ReadError::ARGUMENT));
        refused.unchanged(action);
        action.options.service.identityWidths = L::IdentityWidths::DECLARED_U16;
        CHECK(L::advanceAction(action, r.reservation, r.current, terminal, r.now));
    }
    CHECK(action.finalErrorRead && action.state == M::ActionState::ACTIVE);
    // Before the final read the slot still holds the prerequisite, and the incident outlives it.
    CHECK(action.extraObservation.block == L::StateBlock::ERRORS &&
          action.extraObservation.errorCode == 0xff05 && action.incident.errorCode == 0xff05);
}
// Admission reads no more than it needs: a counter clear carries no alarm incident or ERRORS,
// only its admission FEEDBACK, which the baseline keeps after the cycles replace it.
void counterClearHoldsItsFeedbackOnly() {
    Rig r;
    r.acquire(0);
    L::ActionContext action;
    r.prepare(action, true);
    CHECK(!action.incident.present && action.incident.codeLink == L::AlarmCodeLink::NONE);
    CHECK(action.extraObservation.block == L::StateBlock::FEEDBACK &&
          action.extraObservation.actualPosition == 123 && action.baseline.present &&
          action.baseline.startPosition == 123 && action.baseline.startVelocity == 0);
    const Saved<L::CounterBaseline> baseline(action.baseline);
    r.commands(action, true);
    r.cycle(action);
    CHECK(action.state == M::ActionState::SUCCEEDED &&
          action.extraObservation.block == L::StateBlock::FEEDBACK &&
          action.extraObservation.actualPosition == 0);
    baseline.unchanged(action.baseline);
    CHECK(action.commandHistory[1].transfer.index == 0x2302 && action.commandHistory[1].transfer.data[0] == 1);
}
} // namespace
int main() {
    successfulActionsRetainEvidence();
    unsupportedValuesAndPoliciesAreAtomic();
    pendingObservationsAndDeadline();
    writeFailuresPreservePrefixes();
    transmissionNoticeAndStaleAdmissionAreAtomic();
    interruptedResultSurvivesClear();
    deadlinesFaultsAndRebootKeepOriginalTerminals();
    conservativeNoticeThenDefiniteNoTransmission();
    failedCounterActionThenExplicitNewEpochRead();
    uncertainCounterTriggerCannotReopenThroughEpochSync();
    absentObjectAbortLeavesWorkflowHistoryOnAnOpenRoute();
    identityWidthsRefusedAtAdmission();
    unpreparableFinalReadRefusesBeforeAnyMutation();
    counterClearHoldsItsFeedbackOnly();
    std::printf("reference actions: %u checks passed\n", checks);
}
