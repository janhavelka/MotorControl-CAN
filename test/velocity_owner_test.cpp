// SPDX-License-Identifier: MIT
// End-to-end finite velocity through the real owner. All firmware qualification is synthetic.
#include <cstdio>
#include <cstdlib>
static unsigned checks = 0;
static const char* scenario = "";
#define CHECK(condition)                                                                           \
    do {                                                                                           \
        ++checks;                                                                                  \
        if (!(condition)) {                                                                        \
            std::fprintf(stderr, "%s:%d: %s [%s]\n", __FILE__, __LINE__, #condition, scenario);    \
            std::abort();                                                                          \
        }                                                                                          \
    } while (false)
#include "support/VelocityRig.h"
namespace {
using namespace VelocityRig;

void completeRunsKeepPreparationAnchor() {
    scenario = "complete velocity and explicit stop";
    for (unsigned kind = 0; kind < 2; ++kind) {
        Rig r;
        r.asynchronous = true;
        r.ready();
        const uint64_t prepared = r.now;
        const L::StopKind stop = kind ? L::StopKind::QUICK_STOP : L::StopKind::HALT;
        CHECK(r.start(evidence(false, stop), request(), options(stop)));
        CHECK(r.wire.sentCount == r.startFrame && r.session.velocityActive());
        r.finish();
        const L::VelocityContext& c = r.session.velocityAction();
        if (c.state != M::ActionState::SUCCEEDED)
            std::fprintf(stderr,
                         "velocity terminal phase=%u outcome=%u first=%u step=%u command=%u: %s\n",
                         unsigned(c.phase), unsigned(c.outcome), unsigned(c.firstFailure), c.step,
                         c.commandCursor, c.status.msg);
        CHECK(c.state == M::ActionState::SUCCEEDED && c.outcome == M::ActionOutcome::OBSERVED);
        CHECK(c.startedUs == prepared && c.stopDueUs == prepared + DURATION &&
              c.deadlineUs == prepared + DEADLINE);
        CHECK(c.runningObserved && !c.needsStop && !r.session.velocityNeedsStop());
        CHECK(c.execution == M::ActionExecution::ACKNOWLEDGED &&
              c.stopExecution == M::ActionExecution::ACKNOWLEDGED &&
              c.completion == M::ActionCompletion::OBSERVED);
        CHECK(r.writes(0x60ff, 120) == 1 && r.writes(0x6083, 100) == 1 &&
              r.writes(0x6084, 200) == 1 && r.writes(0x6040, 6) == 1 && r.writes(0x6040, 7) == 1 &&
              r.writes(0x6040, 15) == 1 && r.writes(0x6040, kind ? 0x000b : 0x010f) == 1);
        CHECK(!r.writes(0x6060, 0, false) && !r.writes(0x607a, 0, false));
        unsigned downloads = 0;
        const uint16_t indexes[] = {0x60ff, 0x6083, 0x6084, 0x6040, 0x6040, 0x6040, 0x6040};
        for (unsigned i = r.startFrame; i < r.wire.sentCount; ++i) {
            const M::Frame& f = r.wire.sent[i];
            if (f.data[0] == 0x40)
                continue;
            CHECK(downloads < 7 && indexOf(f) == indexes[downloads++]);
            if (indexOf(f) == 0x6040 && valueOf(f) == (kind ? 0x000bu : 0x010fu))
                CHECK(r.sentAt[i] >= c.stopDueUs && r.sentAt[i] < c.deadlineUs);
        }
        CHECK(downloads == 7 && r.services.node(1)->heartbeat.present);
        const Saved<L::VelocityContext> terminal(c);
        const unsigned sent = r.wire.sentCount;
        r.step();
        terminal.unchanged(r.session.velocityAction());
        CHECK(r.wire.sentCount == sent);
    }
}
void unsupportedStartsPublishNothing() {
    scenario = "qualification, rate destination and enabled mode guards";
    for (unsigned item = 0; item < 7; ++item) {
        Rig r;
        if (item == 3) {
            r.drive.mode = 1;
            r.drive.status = 7;
            r.drive.monitor = 0;
        }
        r.ready();
        L::VelocityEvidence proof = evidence();
        M::FiniteVelocityRequest intent = request();
        if (!item)
            proof.gate = L::VelocityGate::CLOSED;
        if (item == 1)
            intent.velocity.value = M::Rational(-120);
        if (item == 2)
            proof.startQualified = false;
        if (item == 4)
            r.axis.nativeRateBasis = M::NativeRateBasis::COMMAND_INCREMENTS;
        if (item == 5)
            intent.velocity.configurationGeneration++;
        if (item == 6)
            intent.velocity.value = M::Rational(0);
        const Saved<L::VelocityContext> empty(r.session.velocityAction());
        const unsigned sent = r.wire.sentCount;
        CHECK(!r.start(proof, intent));
        empty.unchanged(r.session.velocityAction());
        CHECK(r.wire.sentCount == sent && !r.session.velocityActive());
        r.step();
        CHECK(r.wire.sentCount == sent);
    }
}
void releasedModeChangeAndQualifiedReverse() {
    scenario = "explicit mode write and separately qualified reverse";
    Rig r;
    r.drive.mode = 1;
    r.ready();
    CHECK(r.start(evidence(true), request(-120)));
    r.finish();
    CHECK(r.session.velocityAction().state == M::ActionState::SUCCEEDED);
    CHECK(r.writes(0x6060, 3) == 1 && r.writes(0x60ff, uint32_t(-120)) == 1);
    CHECK(indexOf(r.wire.sent[r.startFrame]) == 0x6060);
    CHECK(r.session.velocityAction().rate.effectiveNative == -120);
}
void completedStopCannotReuseOldReleasedAdmission() {
    scenario = "old released snapshot cannot restart an enabled drive after HALT";
    Rig r;
    r.ready();
    CHECK(r.start());
    r.finish();
    CHECK(r.session.velocityAction().state == M::ActionState::SUCCEEDED && r.drive.monitor == 0 &&
          r.drive.status == 7);
    const Saved<L::VelocityContext> completed(r.session.velocityAction());
    unsigned sent = r.wire.sentCount;
    CHECK(!r.start()); // Same pre-run released snapshot, still within its numerical age policy.
    completed.unchanged(r.session.velocityAction());
    CHECK(r.wire.sentCount == sent);
    r.readState();
    sent = r.wire.sentCount;
    CHECK(!r.start()); // A fresh enabled report also cannot be a released start.
    completed.unchanged(r.session.velocityAction());
    CHECK(r.wire.sentCount == sent);
    // An explicit external release is a new fake fixture fact, never a library-generated command.
    r.drive.monitor = L::MONITOR_RELEASED;
    r.drive.status = 1;
    r.readState();
    CHECK(r.start());
    CHECK(r.session.cancelVelocity(r.now));
    r.finish();
}
void staleGenerationAtControllerHandover() {
    scenario = "generation changes after local admission";
    Rig r;
    r.ready();
    CHECK(r.start());
    r.session.service(r.now); // Accepted locally, but owner has not handed it over.
    ++r.axis.binding.configurationGeneration;
    r.finish();
    const L::VelocityContext& c = r.session.velocityAction();
    CHECK(c.state == M::ActionState::FAILED &&
          c.interruption == M::VelocityInterruption::INVALIDATED);
    CHECK(r.wire.preflightRefusals == 1 && !r.writes(0x60ff, 0, false));
    CHECK(!r.session.velocityRefusal().isOk() &&
          c.execution == M::ActionExecution::NOT_TRANSMITTED);
}
void fillOrdinary(Rig& r, O::SequenceId& other, O::RequestId (&ids)[O::CanBusOwner::REQUESTS]) {
    CHECK(r.owner.beginSequence(target(2).channel.binding, 2, r.now, other));
    const uint64_t deadline = r.now + DEADLINE;
    CHECK(r.owner.beginOperation(other, 1, deadline, r.now));
    for (unsigned i = 0; i < O::CanBusOwner::REQUESTS; ++i) {
        O::BusRequest queued;
        queued.sequence = other;
        queued.transfer.channel = target(2).channel;
        queued.transfer.operation = queued.transfer.request = 1;
        queued.transfer.step = i + 1;
        queued.transfer.index = 0x2100;
        queued.transfer.size = 2;
        queued.transfer.deadlineUs = deadline;
        queued.transfer.queueTimeoutUs = 50000;
        queued.transfer.responseTimeoutUs = RESPONSE;
        CHECK(r.owner.admit(queued, r.now, ids[i]) == O::BusAdmission::ACCEPTED);
    }
}
void releaseOrdinary(Rig& r, O::SequenceId other,
                     const O::RequestId (&ids)[O::CanBusOwner::REQUESTS]) {
    for (unsigned i = 0; i < O::CanBusOwner::REQUESTS; ++i) {
        CHECK(r.owner.result(ids[i]) && r.owner.result(ids[i])->service.protocolAcknowledged);
        CHECK(r.owner.release(ids[i]));
    }
    CHECK(r.owner.releaseSequence(other));
}
void ordinaryResultsPressureCannotRestartDuration() {
    scenario = "full ordinary/results pressure expires preparation";
    Rig r;
    r.ready();
    r.asynchronous = true;
    O::SequenceId other;
    O::RequestId ids[O::CanBusOwner::REQUESTS];
    fillOrdinary(r, other, ids);
    const uint64_t prepared = r.now;
    CHECK(r.start());
    r.finish();
    CHECK(r.session.velocityAction().state == M::ActionState::FAILED &&
          r.session.velocityAction().stopDueUs == prepared + DURATION);
    CHECK(!r.writes(0x60ff, 0, false) && !r.writes(0x6040, 0, false));
    CHECK(r.services.node(1)->heartbeat.present);
    releaseOrdinary(r, other, ids);
    const unsigned sent = r.wire.sentCount;
    r.step();
    CHECK(r.wire.sentCount == sent); // Freed storage cannot restart the expired run.
}
void urgentStopSurvivesOrdinaryPressure() {
    scenario = "full ordinary results preserve finite stop capacity";
    Rig r;
    r.asynchronous = true;
    r.ready();
    CHECK(r.start());
    r.running();
    O::SequenceId other;
    O::RequestId ids[O::CanBusOwner::REQUESTS];
    fillOrdinary(r, other, ids);
    r.finish();
    CHECK(r.session.velocityAction().state == M::ActionState::SUCCEEDED);
    CHECK(r.writes(0x6040, 0x010f) == 1 && r.services.node(1)->heartbeat.present);
    releaseOrdinary(r, other, ids);
}
void cancellationNeverAddsAStop() {
    scenario = "cancel before admission, queued and after running";
    for (unsigned phase = 0; phase < 3; ++phase) {
        Rig r;
        r.ready();
        CHECK(r.start());
        if (phase == 1)
            r.session.service(r.now);
        if (phase == 2)
            r.running();
        CHECK(r.session.cancelVelocity(r.now));
        r.finish();
        const L::VelocityContext& c = r.session.velocityAction();
        CHECK(c.state == M::ActionState::FAILED &&
              c.interruption == M::VelocityInterruption::CANCELLED);
        CHECK(!r.writes(0x6040, 0x010f) && !r.writes(0x6040, 0x000b));
        CHECK(c.needsStop == (phase == 2) && r.session.velocityNeedsStop() == (phase == 2));
        if (phase != 2)
            CHECK(r.wire.sentCount == r.startFrame);
        const Saved<L::VelocityContext> terminal(c);
        r.step();
        terminal.unchanged(r.session.velocityAction());
    }
}
void cancellationAfterHandoverRetainsEveryPossibleWord() {
    scenario = "cancel after controller handover before native acknowledgement";
    const uint16_t words[] = {0, 6, 7, 15};
    for (uint16_t word : words) {
        Rig r;
        r.ready();
        CHECK(r.start());
        for (unsigned i = 0; i < 300 && !(word ? r.writes(0x6040, word) : r.writes(0x60ff, 120));
             ++i)
            r.step();
        CHECK(word ? r.writes(0x6040, word) == 1 : r.writes(0x60ff, 120) == 1);
        const unsigned sent = r.wire.sentCount;
        CHECK(
            r.session.cancelVelocity(r.now)); // The owner's accepted write still settles normally.
        r.finish();
        const L::VelocityContext& c = r.session.velocityAction();
        CHECK(c.state == M::ActionState::FAILED &&
              c.interruption == M::VelocityInterruption::CANCELLED);
        CHECK(r.wire.sentCount == sent && !r.writes(0x6040, 0x010f) && !r.writes(0x6040, 0x000b));
        CHECK(c.effectsPossible && c.needsStop == (word != 0));
        if (word) {
            const unsigned slot = word == 6 ? 0 : word == 7 ? 1 : 2;
            CHECK(c.commandEvidenceMask & (1u << slot));
            CHECK(c.commandHistory[slot].protocolAcknowledged);
        } else {
            CHECK(c.setupEvidenceMask == 1 && c.setupHistory[0].protocolAcknowledged);
        }
    }
}
void faultAndBootInvalidateQueuedMotion() {
    scenario = "fault guard or boot-up before motion handover";
    for (unsigned cause = 0; cause < 2; ++cause) {
        Rig r;
        r.ready();
        CHECK(r.start());
        r.session.service(r.now);
        if (!cause)
            r.wire.driveFault = true;
        else {
            M::Frame boot = frame(0x701, 1);
            boot.data[0] = 0;
            r.wire.push(boot, r.now + TICK, r.owner.controllerEpoch());
        }
        r.finish();
        const L::VelocityContext& c = r.session.velocityAction();
        CHECK(c.state == M::ActionState::FAILED && !r.writes(0x60ff, 0, false) &&
              !r.writes(0x6040, 0, false));
        CHECK(c.outcome == (cause ? M::ActionOutcome::INVALIDATED : M::ActionOutcome::DRIVE_FAULT));
        if (cause)
            CHECK(r.services.node(0)->route.generation != target().nodeGeneration);
        else
            CHECK(r.session.faultBlocked() && r.wire.preflightRefusals == 1);
    }
}
void provenUnsentFirstEnableClearsTentativeFence() {
    scenario = "queued or controller-proved unsent first 6 cannot leave a motion fence";
    for (unsigned controllerProof = 0; controllerProof < 2; ++controllerProof) {
        Rig r;
        r.ready();
        if (controllerProof)
            r.wire.provedUnsentWord = 6;
        CHECK(r.start());
        if (!controllerProof) {
            for (unsigned i = 0;
                 i < 50 && !(r.session.velocityAction().phase == L::VelocityPhase::ENABLE &&
                             r.session.velocityAction().transmissionNoted);
                 ++i)
                r.step();
            CHECK(r.session.velocityAction().phase == L::VelocityPhase::ENABLE &&
                  r.session.velocityAction().commandCursor == 0 &&
                  r.session.velocityAction().transmissionNoted && !r.writes(0x6040, 0, false));
            CHECK(r.session.cancelVelocity(r.now));
        }
        r.finish();
        const L::VelocityContext& c = r.session.velocityAction();
        if (c.needsStop || r.session.velocityNeedsStop() || c.commandEvidenceMask != 1)
            std::fprintf(stderr, "proof=%u state=%u mask=%u tx=%u need=%u fence=%u outcome=%u\n",
                         controllerProof, unsigned(c.state), c.commandEvidenceMask,
                         c.commandHistory[0].transmissionPossible, c.needsStop,
                         r.session.velocityNeedsStop(), unsigned(c.outcome));
        CHECK(c.state == M::ActionState::FAILED && c.commandEvidenceMask == 1 &&
              !c.commandHistory[0].transmissionPossible && !c.needsStop &&
              !r.session.velocityNeedsStop());
        CHECK(!r.writes(0x6040, 7) && !r.writes(0x6040, 15) && !r.writes(0x6040, 0x010f) &&
              !r.writes(0x6040, 0x000b));
        CHECK(r.drive.monitor == L::MONITOR_RELEASED && r.drive.status == 1);
    }
}
void waitBeforeFinalEnable(Rig& r) {
    for (unsigned i = 0;
         i < 200 && !(r.session.velocityAction().phase == L::VelocityPhase::ENABLE &&
                      r.session.velocityAction().commandCursor == 2 &&
                      !r.session.velocityAction().transmissionNoted);
         ++i)
        r.step();
    const L::VelocityContext& c = r.session.velocityAction();
    CHECK(c.phase == L::VelocityPhase::ENABLE && c.commandCursor == 2 && !c.transmissionNoted &&
          c.commandEvidenceMask == 3 && c.commandHistory[0].protocolAcknowledged &&
          c.commandHistory[1].protocolAcknowledged && !r.writes(0x6040, 15));
}
void finalEnableLosesBudgetBeforeHandover() {
    scenario = "queued final 15 loses one microsecond of its full service budget";
    Rig r;
    r.ready();
    CHECK(r.start());
    waitBeforeFinalEnable(r);
    const uint64_t due = r.session.velocityAction().stopDueUs;
    const uint64_t interval = uint64_t(QUEUE) + RESPONSE;
    r.turn(due - interval); // Exactly enough at admission; no controller handover in this turn.
    CHECK(r.session.velocityAction().transmissionNoted && !r.writes(0x6040, 15));
    r.turn(due -
           (interval - 1)); // Still inside the queue budget, but too late for the whole service.
    r.finish();
    const L::VelocityContext& c = r.session.velocityAction();
    CHECK(c.state == M::ActionState::FAILED && c.commandEvidenceMask == 7 &&
          c.execution == M::ActionExecution::NOT_TRANSMITTED &&
          !c.commandHistory[2].transmissionPossible);
    CHECK(c.commandHistory[2].transfer.deadlineUs == due && c.deadlineUs > due);
    CHECK(c.commandHistory[0].transmissionPossible && c.commandHistory[1].transmissionPossible &&
          c.needsStop && r.session.velocityNeedsStop());
    CHECK(r.wire.preflightRefusals == 1 && !r.writes(0x6040, 15));
    const Saved<L::VelocityContext> terminal(c);
    r.turn(due + 1);
    terminal.unchanged(r.session.velocityAction());
    CHECK(!r.writes(0x6040, 15));
}
void unretiredFinalEnableRetainsUncertaintyAcrossDue() {
    scenario = "controller-held final 15 crosses due; late TX cannot become a new run";
    Rig r;
    r.ready();
    CHECK(r.start());
    waitBeforeFinalEnable(r);
    const uint64_t due = r.session.velocityAction().stopDueUs;
    const uint64_t interval = uint64_t(QUEUE) + RESPONSE;
    r.turn(due - interval - 1); // Queue locally with enough budget for the next handover.
    r.wire.autoReport = false;
    r.wire.retirement = false;
    r.turn(due - interval);
    CHECK(r.writes(0x6040, 15) == 1);
    const unsigned held = r.wire.sentCount - 1;
    CHECK(r.wire.pending[held] && indexOf(r.wire.sent[held]) == 0x6040 &&
          valueOf(r.wire.sent[held]) == 15 && r.drive.status == 3);
    r.turn(due); // No physical report; this adapter cannot prove that its queued frame retired.
    const L::VelocityContext& interrupted = r.session.velocityAction();
    CHECK(interrupted.commandEvidenceMask == 7 &&
          interrupted.commandHistory[2].transfer.deadlineUs == due &&
          interrupted.execution == M::ActionExecution::UNKNOWN && interrupted.needsStop);
    CHECK(interrupted.firstFailure == M::ActionOutcome::SERVICE_TIMEOUT && r.wire.pending[held]);
    const Saved<C::SdoCompletion> failedWord(interrupted.commandHistory[2]);
    // A later real TX report describes the old frame; it cannot reopen or replay the run.
    r.wire.report(held, true);
    r.wire.autoReport = true;
    r.step();
    r.finish();
    const L::VelocityContext& c = r.session.velocityAction();
    failedWord.unchanged(c.commandHistory[2]);
    CHECK(
        c.state == M::ActionState::FAILED && c.firstFailure == M::ActionOutcome::SERVICE_TIMEOUT &&
        c.execution == M::ActionExecution::UNKNOWN && c.needsStop && r.session.velocityNeedsStop());
    CHECK(r.writes(0x6040, 15) == 1 && r.writes(0x6040, 6) == 1 && r.writes(0x6040, 7) == 1);
    CHECK(!r.writes(0x6040, 0x000b)); // A HALT policy never substitutes an unrequested QUICK_STOP.
}
E::StopPolicy explicitStop(uint64_t now) {
    E::StopPolicy value;
    value.options.service = reads();
    value.options.observationIntervalUs = 10000;
    value.options.maximumAgeUs = DEADLINE;
    value.deadlineUs = value.bestEffortDeadlineUs = now + 40000;
    return value;
}
void missedServiceKeepsFirstFailureAfterObservedStop() {
    scenario = "missed service retains failure after checked stop";
    Rig r;
    r.ready();
    CHECK(r.start());
    r.running();
    const uint64_t due = r.session.velocityAction().stopDueUs;
    const uint64_t deadline = r.session.velocityAction().deadlineUs;
    r.turn(due + options().pollIntervalUs + 1); // No admitted work when service was missed.
    r.finish();
    const L::VelocityContext& c = r.session.velocityAction();
    CHECK(c.state == M::ActionState::FAILED &&
          c.firstFailure == M::ActionOutcome::SERVICE_TIMEOUT &&
          c.outcome == M::ActionOutcome::SERVICE_TIMEOUT && c.deadlineUs == deadline &&
          c.stopDueUs == due);
    CHECK(c.stopOutcome == M::ActionOutcome::OBSERVED &&
          c.completion == M::ActionCompletion::OBSERVED && !c.needsStop &&
          !r.session.velocityNeedsStop());
    CHECK(r.writes(0x6040, 0x010f) == 1);
}
void expiredRunNeedsSeparatelyAdmittedStop() {
    scenario = "expired run and later independent stop keep separate outcomes";
    Rig r;
    r.ready();
    CHECK(r.start());
    r.running();
    r.turn(r.session.velocityAction().deadlineUs);
    CHECK(!r.session.velocityActive() && r.session.velocityNeedsStop());
    const L::VelocityContext& c = r.session.velocityAction();
    CHECK(c.outcome == M::ActionOutcome::DEADLINE && c.needsStop && !r.writes(0x6040, 0x010f) &&
          !r.writes(0x6040, 0x000b));
    const Saved<L::VelocityContext> terminal(c);
    const E::StopPath path = r.session.requestStop(
        L::StopKind::HALT, r.session.velocityObservation(), explicitStop(r.now), r.now);
    CHECK(path == E::StopPath::CONFIRMED);
    r.finish();
    CHECK(r.session.stopAction(L::StopKind::HALT).outcome == M::ActionOutcome::OBSERVED &&
          !r.session.velocityNeedsStop());
    terminal.unchanged(r.session.velocityAction());
}
void externalStopInterruptsScheduledStop() {
    scenario = "external stop interrupts velocity while its scheduled stop is active";
    Rig r;
    r.ready();
    CHECK(r.start());
    r.running();
    r.drive.stopWorks = false; // The first HALT is acknowledged but keeps reporting running.
    r.turn(r.session.velocityAction().stopDueUs);
    for (unsigned i = 0; i < 40 && !r.session.stopAction(L::StopKind::HALT).observationKnown; ++i)
        r.step();
    CHECK(r.session.stopActive() && r.session.velocityActive() &&
          r.session.stopAction(L::StopKind::HALT).observationKnown);
    const E::StopPath path = r.session.requestStop(
        L::StopKind::QUICK_STOP, r.session.velocityObservation(), explicitStop(r.now), r.now);
    CHECK(path == E::StopPath::CONFIRMED);
    CHECK(r.session.velocityAction().interruption == M::VelocityInterruption::STOP &&
          r.session.velocityAction().state == M::ActionState::FAILED);
    const Saved<L::VelocityContext> terminal(r.session.velocityAction());
    r.drive.stopWorks = true;
    r.finish();
    CHECK(r.writes(0x6040, 0x010f) == 1 && r.writes(0x6040, 0x000b) == 1);
    CHECK(r.session.stopAction(L::StopKind::QUICK_STOP).completion ==
          M::ActionCompletion::OBSERVED);
    terminal.unchanged(r.session.velocityAction());
}
void unknownStartSurvivesBestEffortStop() {
    scenario = "lost final enable reply, explicit DEC-2 stop and stale ACK";
    for (unsigned quick = 0; quick < 2; ++quick) {
        Rig r;
        r.ready();
        r.drive.muteIndex = 0x6040;
        r.drive.muteWord = 15;
        const L::StopKind kind = quick ? L::StopKind::QUICK_STOP : L::StopKind::HALT;
        CHECK(r.start(evidence(false, kind), request(), options(kind)));
        r.finish();
        const L::VelocityContext& c = r.session.velocityAction();
        CHECK(c.state == M::ActionState::FAILED && c.execution == M::ActionExecution::UNKNOWN &&
              c.needsStop && r.session.velocityNeedsStop());
        // Only an explicitly selected QUICK_STOP permits policy DEC-2. HALT cannot substitute it.
        CHECK(r.writes(0x6040, 15) == 1 && r.writes(0x6040, 0x000b) == quick &&
              !r.writes(0x6040, 0x010f));
        CHECK(c.stopExecution ==
                  (quick ? M::ActionExecution::UNKNOWN : M::ActionExecution::NOT_TRANSMITTED) &&
              c.completion == M::ActionCompletion::NOT_OBSERVED);
        const Saved<L::VelocityContext> terminal(c);
        // The retained word-7 observation predates the uncertain start. It must not refuse DEC-2.
        const E::StopPath path = r.session.requestStop(
            L::StopKind::QUICK_STOP, r.session.velocityObservation(), explicitStop(r.now), r.now);
        CHECK(path == E::StopPath::BEST_EFFORT || path == E::StopPath::HELD);
        r.drive.muteIndex = 0;
        r.finish();
        CHECK(r.writes(0x6040, 0x000b) == quick + 1 && r.session.velocityNeedsStop());
        CHECK(r.session.bestEffort().execution == M::ActionExecution::UNKNOWN);
        terminal.unchanged(r.session.velocityAction());
        M::Frame stale = frame(0x581, 8);
        stale.data[0] = 0x60;
        stale.data[1] = 0x40;
        stale.data[2] = 0x60;
        r.wire.push(stale, r.now + TICK, r.owner.controllerEpoch());
        r.step();
        terminal.unchanged(r.session.velocityAction());
    }
}
void lostStopAcknowledgementStaysUnknown() {
    scenario = "one stop transmission and lost acknowledgement";
    Rig r;
    r.ready();
    r.drive.muteIndex = 0x6040;
    r.drive.muteWord = 0x010f;
    CHECK(r.start());
    r.finish();
    const L::VelocityContext& c = r.session.velocityAction();
    CHECK(c.state == M::ActionState::FAILED && c.stopExecution == M::ActionExecution::UNKNOWN &&
          c.needsStop && r.session.velocityNeedsStop());
    CHECK(r.writes(0x6040, 0x010f) == 1 && !r.writes(0x6040, 0x000b));
    CHECK(c.execution == M::ActionExecution::ACKNOWLEDGED &&
          c.completion == M::ActionCompletion::NOT_OBSERVED);
}
void earlyStopReplyProcessedAtDeadlineCannotSucceed() {
    scenario = "earlier received stop ACK processed at fixed deadline";
    Rig r;
    r.ready();
    r.drive.muteIndex = 0x6040;
    r.drive.muteWord = 0x010f;
    CHECK(r.start());
    r.running();
    r.turn(r.session.velocityAction().stopDueUs);
    for (unsigned i = 0; i < 20 && !r.writes(0x6040, 0x010f); ++i)
        r.step();
    CHECK(r.writes(0x6040, 0x010f) == 1);
    M::Frame ack = frame(0x581, 8);
    ack.data[0] = 0x60;
    ack.data[1] = 0x40;
    ack.data[2] = 0x60;
    r.wire.push(ack, r.now + 1, r.owner.controllerEpoch());
    r.turn(r.session.velocityAction().deadlineUs);
    r.finish();
    const L::VelocityContext& c = r.session.velocityAction();
    CHECK(c.state == M::ActionState::FAILED && c.outcome == M::ActionOutcome::DEADLINE &&
          c.completion == M::ActionCompletion::NOT_OBSERVED && r.session.velocityNeedsStop());
    CHECK(r.writes(0x6040, 0x010f) == 1 && !r.writes(0x6040, 0x000b));
}
void settingsSnapshotRetiresWithoutChangingGeneration() {
    scenario = "velocity parameter writes retire position snapshot";
    Rig r;
    r.ready();
    L::DriverContext retained;
    r.readProfile(retained);
    r.readState();
    L::DriverContext* rows[] = {&retained};
    CHECK(r.session.retainSettings(rows, 1));
    const M::Generation generation = r.axis.binding.configurationGeneration;
    CHECK(r.start());
    r.session.service(r.now);
    CHECK(retained.read.invalidated); // The first 60FF possible-TX notice retires it.
    CHECK(r.axis.binding.configurationGeneration == generation);
    r.finish();
    CHECK(r.session.velocityAction().state == M::ActionState::SUCCEEDED);
    CHECK(r.axis.binding.configurationGeneration == generation);
}
} // namespace
int main() {
    completeRunsKeepPreparationAnchor();
    unsupportedStartsPublishNothing();
    releasedModeChangeAndQualifiedReverse();
    completedStopCannotReuseOldReleasedAdmission();
    staleGenerationAtControllerHandover();
    ordinaryResultsPressureCannotRestartDuration();
    urgentStopSurvivesOrdinaryPressure();
    cancellationNeverAddsAStop();
    cancellationAfterHandoverRetainsEveryPossibleWord();
    faultAndBootInvalidateQueuedMotion();
    provenUnsentFirstEnableClearsTentativeFence();
    finalEnableLosesBudgetBeforeHandover();
    unretiredFinalEnableRetainsUncertaintyAcrossDue();
    missedServiceKeepsFirstFailureAfterObservedStop();
    expiredRunNeedsSeparatelyAdmittedStop();
    externalStopInterruptsScheduledStop();
    unknownStartSurvivesBestEffortStop();
    lostStopAcknowledgementStaysUnknown();
    earlyStopReplyProcessedAtDeadlineCannotSucceed();
    settingsSnapshotRetiresWithoutChangingGeneration();
    std::printf("velocity owner: %u checks\n", checks);
}
