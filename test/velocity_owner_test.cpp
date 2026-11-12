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
        CHECK(downloads == 7 && (!PAIRED_CHANNELS || r.services.node(1)->heartbeat.present));
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
// The attended experiment through the session: the same frames, its own stop at due, and a context
// that still names the experiment after success. A qualification claim inside it sends nothing.
L::VelocityEvidence experimentEvidence() {
    L::VelocityEvidence value = evidence();
    value.gate = L::VelocityGate::ATTENDED_EXPERIMENT;
    value.startQualified = value.stopQualified = value.modeChangeQualified = false;
    value.modeChangeFromRaw = 0;
    value.maximumForwardRpm = L::VELOCITY_EXPERIMENT_MAXIMUM_RPM;
    return value;
}
void attendedExperimentThroughSession() {
    scenario = "attended experiment scope through the session";
    {
        Rig r;
        r.asynchronous = true;
        r.ready();
        CHECK(r.start(experimentEvidence(), request(60)));
        r.finish();
        const L::VelocityContext& c = r.session.velocityAction();
        CHECK(c.state == M::ActionState::SUCCEEDED && c.outcome == M::ActionOutcome::OBSERVED &&
              c.completion == M::ActionCompletion::OBSERVED && !c.needsStop);
        CHECK(c.evidence.gate == L::VelocityGate::ATTENDED_EXPERIMENT && !c.evidence.startQualified &&
              !c.evidence.stopQualified && !c.evidence.modeChangeQualified &&
              !c.evidence.reverseQualified && c.evidence.maximumForwardRpm == 60);
        CHECK(r.writes(0x60ff, 60) == 1 && r.writes(0x6040, 15) == 1 && r.writes(0x6040, 0x010f) == 1 &&
              !r.writes(0x6060, 0, false));
    }
    for (unsigned item = 0; item < 5; ++item) {
        Rig r;
        if (item == 4)
            r.drive.mode = 1; // Released in mode 1: the experiment claims no mode change.
        r.ready();
        L::VelocityEvidence proof = experimentEvidence();
        M::FiniteVelocityRequest intent = request(60);
        if (item == 0)
            proof.startQualified = true;
        if (item == 1)
            proof.stopQualified = true;
        if (item == 2)
            intent = request(61);
        if (item == 3)
            proof.maximumForwardRpm = 61;
        const Saved<L::VelocityContext> empty(r.session.velocityAction());
        const unsigned sent = r.wire.sentCount;
        CHECK(!r.start(proof, intent));
        empty.unchanged(r.session.velocityAction());
        CHECK(r.wire.sentCount == sent && !r.session.velocityActive());
        r.step();
        CHECK(r.wire.sentCount == sent);
    }
}
void exactPollBudgetCoversSetupAndTheWholeDuration() {
    scenario = "setup confirmations do not consume the exact running poll budget";
    for (unsigned variant = 0; variant < 8; ++variant) {
        const unsigned modeChange = (variant >> 2) & 1u;
        const unsigned quick = (variant >> 1) & 1u;
        Rig r;
        if (modeChange)
            r.drive.mode = 1;
        r.ready();
        const L::StopKind kind = quick ? L::StopKind::QUICK_STOP : L::StopKind::HALT;
        L::VelocityOptions policy = options(kind);
        policy.maxPolls = (variant & 1u) ? 4u : 1u;
        policy.pollIntervalUs = DURATION / policy.maxPolls;
        CHECK(r.start(evidence(false, kind), request(), policy));
        const uint64_t due = r.session.velocityAction().stopDueUs;
        for (unsigned i = 0; i < 1000 && r.session.busy(); ++i) {
            r.step();
            const L::VelocityContext& c = r.session.velocityAction();
            CHECK(c.firstFailure == M::ActionOutcome::NONE);
            if (r.now < due)
                CHECK(r.session.velocityActive() && !c.stopRequested && !r.writes(0x6040, 0x010f) &&
                      !r.writes(0x6040, 0x000b));
        }
        const L::VelocityContext& c = r.session.velocityAction();
        CHECK(!r.session.busy() && c.state == M::ActionState::SUCCEEDED &&
              c.outcome == M::ActionOutcome::OBSERVED);
        CHECK(c.stopRequestedUs >= due && c.stopRequestedUs <= due + policy.pollIntervalUs);
        CHECK(c.observations == c.runningPolls + (modeChange ? 4u : 3u) &&
              c.runningPolls <= policy.maxPolls);
        CHECK(c.latest.cycle == c.observations && c.runningObserved && !c.needsStop);
        CHECK(r.writes(0x6040, quick ? 0x000b : 0x010f) == 1 && r.writes(0x6060, 3) == modeChange);
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
// Trial 1009 (2026-11-09), replayed through the real owner and AxisSession. Every reply value below is
// the retained capture's, in its order: docs/bench/20261109T1446Z-p3536-velocity-node1-1009/rx.txt,
// RESULT lines 16 to 24 (identity, op 35), 25 to 33 (admission read, op 36) and 34 to 60 (the run, op
// 37), and the later read-only route 1019 (docs/bench/20261109T1446Z-p3536-p1-node1-1019/rx.txt,
// RESULT lines 9 to 15 and 22 to 28: 2100 0004, 6041 C437, 6064 3595 then 3604, 606C 5). The budgets are
// BenchRoute::startVelocity's for that line (100 ms queue, 250 ms response, 2 s interval, six polls,
// nine-field age, eight-service stop reserve, 10 ms between words, 12 s run, 18 s deadline). Host turns
// replace the bench's clock, so only the order and the relative times are the bench's. What the drive
// reports after a stop word was never measured: each case states its own assumption.
namespace Replay1009 {
struct Report {
    uint16_t monitor, status;
};
const Report ADMISSION = {0x0011, 0x8050}; // Lines 27 and 30.
const Report AFTER_6 = {0x0011, 0x8031};   // Lines 38 and 41.
const Report AFTER_7 = {0x0001, 0x8033};   // Lines 46 and 49.
const Report AFTER_15 = {0x0001, 0x8033};  // Lines 54 and 57: still word 7's report.
const Report LATER = {0x0004, 0xc437};     // Route 1019, about 37 s later: running.
const uint32_t QUEUE_US = 100000, RESPONSE_US = 250000;
const uint64_t SERVICE_US = uint64_t(QUEUE_US) + RESPONSE_US, TICK_US = 1000;
// The drive as captured. A stop word changes its report to afterStop, an assumption of each case.
struct Drive {
    Report report = ADMISSION;
    Report afterStop = LATER;
    bool later = false;           // Reporting LATER, with 606C 5 and 6064 rising as in 1019.
    uint16_t muteWord = 0;        // This 6040 word is applied but its reply is lost.
    uint16_t abortWord = 0;       // This 6040 word is refused with an SDO abort and not applied.
    // Backlog S17: after word 15, report AFTER_15 for this many cycles, then LATER. 0 keeps AFTER_15.
    unsigned enableAfterCycles = 0, pendingCycles = 0;
    const Report* afterEnable = &LATER;
    int32_t position = 0;
    uint32_t positionReads = 0;
    void become(const Report& r) {
        report = r;
        later = r.status == LATER.status && r.monitor == LATER.monitor;
    }
    bool respond(const M::Frame& sent, M::Frame& out) {
        const uint16_t index = indexOf(sent);
        const uint8_t sub = sent.data[3];
        out = frame(0x581, 8);
        out.data[1] = sent.data[1];
        out.data[2] = sent.data[2];
        out.data[3] = sub;
        if (sent.data[0] != 0x40) {
            const uint32_t value = valueOf(sent);
            CHECK(index == 0x60ff || index == 0x6083 || index == 0x6084 || index == 0x6040);
            if (index == 0x6040) {
                CHECK(value == 6 || value == 7 || value == 15 || value == 0x010f || value == 0x000b);
                if (value == abortWord) {
                    out.data[0] = 0x80;
                    out.data[4] = 0x22; // 08000022: data cannot be transferred in the present state.
                    out.data[7] = 0x08;
                    return true;
                }
                become(value == 6 ? AFTER_6 : value == 7 ? AFTER_7 : value == 15 ? AFTER_15 : afterStop);
                pendingCycles = value == 15 ? enableAfterCycles : 0;
                if (value == muteWord)
                    return false;
            }
            out.data[0] = 0x60;
            return true;
        }
        uint32_t raw = 0;
        unsigned size = 2;
        switch (index) {
        case 0x1000: raw = 0x00040192; size = 4; break;               // Line 16.
        case 0x1009: raw = 0x0c; size = 1; break;                     // Line 17.
        case 0x100a: raw = 0x20; size = 1; break;                     // Line 18.
        case 0x1018:                                                  // Lines 19 to 22.
            raw = !sub ? 5u : sub == 1 ? 0x1000u : sub == 2 ? 1u : 0u;
            size = sub ? 4 : 1;
            break;
        case 0x2000: raw = 0x0400; break;                             // Line 23.
        case 0x2001: raw = 0x0200; break;                             // Line 24.
        case 0x1001: size = 1; break;                                 // Line 25.
        case 0x603f: break;                                           // Line 26.
        case 0x2100: raw = report.monitor; break;
        case 0x2101: case 0x2102: break;
        case 0x6041: raw = report.status; break;
        case 0x6061: raw = 3; size = 1; break;
        case 0x6064:
            if (later)
                position = 3595 + 9 * int32_t(positionReads++);
            raw = uint32_t(position);
            size = 4;
            break;
        case 0x606c:
            raw = uint32_t(later ? 5 : 0);
            size = 4;
            if (pendingCycles && !--pendingCycles)
                become(*afterEnable);
            break;
        default: CHECK(false);
        }
        out.data[0] = size == 1 ? 0x4f : size == 2 ? 0x4b : 0x43;
        for (unsigned i = 0; i < 4; ++i)
            out.data[4 + i] = uint8_t(raw >> (8 * i));
        return true;
    }
};
// The drive type is a parameter so that the S18 replays can script their own reports.
template <class D> struct BenchOf {
    C::SdoClient services;
    Wire wire;
    O::CanBusOwner owner;
    E::AxisSession session;
    D drive;
    L::ReadContext identityRead, state, check;
    L::IdentityObservation identity;
    M::AxisConfig axis;
    uint64_t now = 0;
    unsigned served = 0, startFrame = 0;
    // Backlog S17: BenchRoute's VELOCITY options with its enable report window (2000 ms, cycles 300 ms
    // apart) and the plan that fits the record with it: 12 s at 5 s intervals, three polls, 15.5 s.
    bool windowed = false;
    // Backlog S18: the record, stop kind and rate of the line, and the 4000 ms / 17000 ms plan when set.
    uint32_t record = 1009;
    L::StopKind kind = L::StopKind::HALT;
    int64_t rpm = 6;
    uint64_t pollUs = 0, deadlineAfterUs = 0;
    BenchOf() : owner(services, wire.callbacks(), 73), session(owner) {
        wire.session = &session;
        wire.services = &services;
        CHECK(services.configure(0, target().channel, true, true));
        C::NetworkRoute network;
        network.binding = target().channel.binding;
        network.generation = 11;
        network.heartbeatId = 0x701;
        network.emcyId = 0x81;
        CHECK(services.configureNetwork(0, network, C::NetworkObserver(), 0));
    }
    void turn(uint64_t at) {
        now = at;
        if (wire.rxRead == wire.rxWrite)
            wire.rxRead = wire.rxWrite = 0;
        for (; served < wire.sentCount; ++served) {
            CHECK(!wire.pending[served] && wire.sent[served].id == 0x601);
            M::Frame reply;
            if (drive.respond(wire.sent[served], reply))
                wire.push(reply, now, owner.controllerEpoch());
        }
        CHECK(owner.service(now, budget()));
        session.service(now);
    }
    void step() { turn(now + TICK_US); }
    // Empties the fake wire between the phases of one long session (S18); nothing may be outstanding.
    void rewind() {
        for (unsigned i = 0; i < wire.sentCount; ++i)
            CHECK(!wire.pending[i]);
        CHECK(served == wire.sentCount && wire.reportRead == wire.reportWrite && wire.rxRead == wire.rxWrite);
        wire.sentCount = wire.reportRead = wire.reportWrite = wire.rxRead = wire.rxWrite = 0;
        served = startFrame = 0;
    }
    L::ReadOptions service() const {
        L::ReadOptions value;
        value.queueTimeoutUs = QUEUE_US;
        value.responseTimeoutUs = RESPONSE_US;
        return value;
    }
    void read(L::ReadContext& into) {
        for (unsigned guard = 0; into.state == M::ReadState::ACTIVE && guard < 30; ++guard) {
            L::PreparedRead work;
            CHECK(L::nextRead(into, now, work) && work.ready);
            CHECK(session.admitPrepared(work.transfer, now) == O::BusAdmission::ACCEPTED);
            step();
            step();
            C::SdoCompletion terminal;
            CHECK(session.settle(terminal));
            CHECK(L::advanceRead(into, terminal, now));
        }
        CHECK(into.state == M::ReadState::SUCCEEDED);
    }
    // A full read, as the route's STEP_STATE FULL: ERRORS, MONITORS, STATUS and FEEDBACK (blocks 1d).
    void readState(L::ReadContext& into) {
        const uint32_t operation = session.beginOperation(now + 20 * SERVICE_US, now);
        CHECK(operation);
        const uint8_t mask = uint8_t((1u << unsigned(L::StateBlock::ERRORS)) |
                                     (1u << unsigned(L::StateBlock::MONITORS)) |
                                     (1u << unsigned(L::StateBlock::STATUS)) |
                                     (1u << unsigned(L::StateBlock::FEEDBACK)));
        CHECK(L::prepareState(into, target(), operation, now, now + 20 * SERVICE_US, service(), mask));
        read(into);
    }
    // Ops 35 and 36: identity with the specimens' one-byte 1009 and 100A, then the admission read.
    void ready() {
        CHECK(session.open(target(), L::SettingsModel::CL86_C, 1, now));
        const uint32_t operation = session.beginOperation(now + 20 * SERVICE_US, now);
        CHECK(operation);
        L::ReadOptions widths = service();
        widths.identityWidths = L::IdentityWidths::MEASURED_U8;
        CHECK(L::prepareIdentity(identityRead, target(), operation, now, now + 20 * SERVICE_US, widths));
        read(identityRead);
        CHECK(L::getIdentity(identityRead, target(), now, 20 * SERVICE_US, identity));
        readState(state);
    }
    M::Status start() {
        axis.binding = session.binding();
        axis.nativeRateBasis = M::NativeRateBasis::MOTOR_TURNS;
        axis.nativeVelocityTime = M::TimeUnit::MINUTE;
        L::VelocityEvidence evidence; // TRIAL_VELOCITY_EVIDENCE, line 8.
        evidence.gate = L::VelocityGate::ATTENDED_EXPERIMENT;
        evidence.record = record;
        evidence.model = L::SettingsModel::CL86_C;
        evidence.firmware.deviceType = 0x00040192;
        evidence.firmware.hardwareVersion = 0x0c;
        evidence.firmware.softwareVersion = 0x20;
        evidence.firmware.vendorId = 0x1000;
        evidence.firmware.productCode = 1;
        evidence.firmware.revision = 0;
        evidence.firmware.modelCode = 0x0400;
        evidence.firmware.firmwareCode = 0x0200;
        evidence.layout = L::StatusLayout::ZH86;
        evidence.stopKind = kind;
        evidence.maximumForwardRpm = uint32_t(rpm);
        M::FiniteVelocityRequest request; // TRIAL_PLAN, line 4.
        request.velocity.value = M::Rational(rpm);
        request.velocity.unit = M::VelocityUnit(M::PositionUnit::TURNS, M::TimeUnit::MINUTE);
        request.velocity.configurationGeneration = axis.binding.configurationGeneration;
        request.durationUs = 12000000;
        L::VelocityProfile profile;
        profile.rampUpTimeMs = profile.rampDownTimeMs = 200;
        L::VelocityOptions options;
        options.service = service();
        options.pollIntervalUs = 2000000;
        options.maxPolls = 6;
        options.maximumAgeUs = 9 * SERVICE_US;
        options.requiredStopBudgetUs = 8 * SERVICE_US;
        options.commandIntervalUs = L::ENABLE_WORD_INTERVAL_US;
        options.stopKind = kind;
        if (windowed) {
            options.pollIntervalUs = pollUs ? pollUs : 5000000;
            options.maxPolls = 3;
            options.enableWindowUs = 2000000;
            options.enableSpacingUs = 300000;
        }
        startFrame = wire.sentCount;
        return session.requestVelocity(state, identity, evidence, request, profile, axis, options,
                                       now + (deadlineAfterUs ? deadlineAfterUs : windowed ? 15500000 : 18000000),
                                       now);
    }
    // The run's downloads and cycles end once the session records a velocity terminal.
    void runToTerminal() {
        for (unsigned i = 0; i < 400000 && session.busy(); ++i)
            step();
        CHECK(!session.busy());
    }
    bool downloadAt(unsigned i, uint16_t index, uint32_t value, uint8_t command) const {
        const M::Frame& f = wire.sent[i];
        return i < wire.sentCount && f.id == 0x601 && f.dlc == 8 && f.data[0] == command &&
               indexOf(f) == index && f.data[3] == 0 && valueOf(f) == value;
    }
    bool cycleAt(unsigned i) const {
        const uint16_t order[] = {0x2100, 0x2101, 0x2102, 0x6041, 0x6061, 0x6064, 0x606c};
        for (unsigned k = 0; k < 7; ++k)
            if (i + k >= wire.sentCount || wire.sent[i + k].data[0] != 0x40 ||
                indexOf(wire.sent[i + k]) != order[k])
                return false;
        return true;
    }
};
using Bench = BenchOf<Drive>;
// An explicit application stop with the route's budgets; the DEC-2 branch only when asked for.
template <class B> E::StopPolicy stopPolicy(const B& b, bool bestEffort) {
    E::StopPolicy policy;
    policy.options.service = b.service();
    policy.options.observationIntervalUs = 2000000;
    policy.options.maximumAgeUs = 9 * SERVICE_US;
    policy.deadlineUs = b.now + 6000000;
    policy.bestEffortDeadlineUs = bestEffort ? policy.deadlineUs : 0;
    return policy;
}
// The captured run up to its terminal, as TX lines 115 to 193 sent it: 60FF 6, 6083 200, 6084 200,
// then 6, 7 and 15, each followed by one seven-field cycle.
void checkCapturedRun(const Bench& b) {
    unsigned i = b.startFrame;
    CHECK(b.downloadAt(i, 0x60ff, 6, 0x23) && b.downloadAt(i + 1, 0x6083, 200, 0x23) &&
          b.downloadAt(i + 2, 0x6084, 200, 0x23));
    i += 3;
    const uint16_t words[] = {6, 7, 15};
    for (uint16_t word : words) {
        CHECK(b.downloadAt(i, 0x6040, word, 0x2b) && b.cycleAt(i + 1));
        i += 8;
    }
    const L::VelocityContext& c = b.session.velocityAction();
    // TRIAL_VELOCITY line 9 and TRIAL_VELOCITY_SAMPLE lines 11 and 12.
    CHECK(c.evidence.gate == L::VelocityGate::ATTENDED_EXPERIMENT && c.evidence.record == 1009);
    CHECK(c.firstFailure == M::ActionOutcome::OBSERVATION_SCOPE &&
          c.outcome == M::ActionOutcome::OBSERVATION_SCOPE);
    CHECK(c.setup == M::ActionExecution::ACKNOWLEDGED && c.execution == M::ActionExecution::ACKNOWLEDGED);
    CHECK(c.commandEvidenceMask == 7 && c.control.transfer.data[0] == 15 && !c.runningObserved);
    CHECK(c.uncertain && c.effectsPossible && c.needsStop && c.observations == 3 && !c.runningPolls);
    CHECK(c.status.code == M::Err::INVALID_CONFIG && c.status.detail == int32_t(M::VelocityError::STATE));
    CHECK(c.rate.effectiveNative == 6 && c.stopDueUs == c.startedUs + 12000000 &&
          c.deadlineUs == c.startedUs + 18000000);
    CHECK(c.admission.present && !c.admission.cycle && c.admission.statusword == 0x8050 &&
          c.admission.monitor == 0x0011 && c.admission.modeRaw == 3 && !c.admission.position &&
          !c.admission.velocity);
    CHECK(c.latest.present && c.latest.cycle == 3 && c.latest.statusword == 0x8033 &&
          c.latest.monitor == 0x0001 && c.latest.modeRaw == 3 && !c.latest.position &&
          !c.latest.velocity);
    // The cycle that ended the run is outside every reviewed stop state: 6041 low bits 011, not
    // 111, and 2100 running clear in mode 3. prepareStop refuses to follow the run's own acknowledged
    // 000F from it, for either kind.
    L::ActionOptions stop;
    stop.service = b.service();
    stop.observationIntervalUs = 2000000;
    stop.maximumAgeUs = 9 * SERVICE_US;
    static L::ActionContext probe;
    const L::StopKind kinds[] = {L::StopKind::HALT, L::StopKind::QUICK_STOP};
    for (L::StopKind kind : kinds)
        CHECK(!L::validateStop(probe, c.reservation, b.session.velocityObservation(), c.control, kind,
                               c.operation + 10, c.stopRequestedUs, c.deadlineUs, stop));
}
void trial1009StopsAfterTheLaggingEnable() {
    // 0: the drive then reports what 1019 found 37 s later, running (the worst case: the HALT is never
    // seen to act). 1: it keeps word 7's report after the HALT, as it did when the run failed.
    for (unsigned variant = 0; variant < 2; ++variant) {
        scenario = variant ? "trial 1009 replay, HALT then 8033/0001" : "trial 1009 replay, HALT then C437/0004";
        Bench b;
        b.drive.afterStop = variant ? AFTER_15 : LATER;
        b.ready();
        CHECK(b.start());
        b.runToTerminal();
        checkCapturedRun(b);
        const L::VelocityContext& c = b.session.velocityAction();
        if (c.stopExecution == M::ActionExecution::NOT_TRANSMITTED)
            std::fprintf(stderr,
                         "1009 replay: state=%u phase=%u first=%u int=%u stop=%u sexec=%u needs=%u "
                         "fenced=%u path=%u sent=%u\n",
                         unsigned(c.state), unsigned(c.phase), unsigned(c.firstFailure),
                         unsigned(c.interruption), unsigned(c.stopOutcome), unsigned(c.stopExecution),
                         unsigned(c.needsStop), unsigned(b.session.velocityNeedsStop()),
                         unsigned(b.session.stopPath()), b.wire.sentCount - b.startFrame);
        // The run's own HALT follows its acknowledged 000F at once: 2B 40 60 00 0F 01 00 00.
        const unsigned halt = b.startFrame + 27;
        CHECK(b.downloadAt(halt, 0x6040, 0x010f, 0x2b));
        const uint8_t bytes[] = {0x2b, 0x40, 0x60, 0x00, 0x0f, 0x01, 0x00, 0x00};
        for (unsigned k = 0; k < 8; ++k)
            CHECK(b.wire.sent[halt].data[k] == bytes[k]);
        // One stop word, then only the stop's MONITORS and STATUS cycles; nothing is resent.
        unsigned downloads = 0, cycles = 0;
        for (unsigned i = halt + 1; i < b.wire.sentCount; ++i) {
            downloads += b.wire.sent[i].data[0] != 0x40;
            cycles += indexOf(b.wire.sent[i]) == 0x2100;
        }
        CHECK(!downloads && cycles >= 1);
        const L::ActionContext& s = b.session.stopAction(L::StopKind::HALT);
        CHECK(s.operation == c.stopOperation && s.operation > c.operation && s.stopModeRaw == 3);
        CHECK(s.predecessorWord == 0x000f && !s.predecessorDocumented &&
              s.predecessorBasis == L::PredecessorBasis::PROJECT_DECISION);
        CHECK(s.startedUs == c.stopRequestedUs && s.deadlineUs == c.deadlineUs);
        CHECK(s.execution == M::ActionExecution::ACKNOWLEDGED && s.observations == cycles);
        // The record: the first failure kept, the stop sent and acknowledged, nothing settled.
        CHECK(c.state == M::ActionState::FAILED && c.phase == L::VelocityPhase::STOPPING &&
              c.stopRequested && c.interruption == M::VelocityInterruption::NONE);
        CHECK(c.stopExecution == M::ActionExecution::ACKNOWLEDGED &&
              c.completion == M::ActionCompletion::NOT_OBSERVED && c.needsStop && c.uncertain);
        CHECK(b.session.velocityNeedsStop() && b.session.stopPath() == E::StopPath::FINISHED);
        if (!variant) {
            // Running throughout, so the HALT's observation never completes before the deadline.
            CHECK(s.state == M::ActionState::FAILED && s.outcome == M::ActionOutcome::DEADLINE &&
                  s.completion == M::ActionCompletion::NOT_OBSERVED);
            CHECK(c.stopOutcome == M::ActionOutcome::DEADLINE && b.now >= c.deadlineUs);
        } else {
            // A not-running report from a drive that never reported running is no evidence that the
            // HALT stopped anything: the stop's own record says what it saw, the run stays unsettled.
            CHECK(s.state == M::ActionState::SUCCEEDED && s.outcome == M::ActionOutcome::OBSERVED &&
                  s.observations == 1 && s.monitorObservation.monitor == 0x0001 &&
                  s.statusObservation.statusword == 0x8033);
            CHECK(c.stopOutcome == M::ActionOutcome::OBSERVED && b.now < c.deadlineUs);
        }
        // The fence holds: no new run, move or enable on this session.
        b.readState(b.check);
        CHECK(!b.start());
        // The latest word is now the HALT's 010F, not the run's 000F: a second HALT has no
        // predecessor and is refused, so the stop word is never sent twice.
        const unsigned sent = b.wire.sentCount;
        CHECK(b.session.latestControl() && b.session.latestControl()->transfer.data[0] == 0x0f &&
              b.session.latestControl()->transfer.data[1] == 0x01);
        CHECK(b.session.requestStop(L::StopKind::HALT, b.check, stopPolicy(b, false), b.now) ==
              E::StopPath::NONE);
        for (unsigned i = 0; i < 100; ++i)
            b.step();
        CHECK(b.wire.sentCount == sent && b.session.velocityNeedsStop());
    }
}
// Local cancel is not a stop. Cancelled right after the acknowledged 000F, before its cycle finished,
// the run ends CANCELLED with nothing more sent and the fence up. The application's own HALT then
// follows that 000F even though a fresh read reports word 7's state (the stop prepareStop refuses),
// and its not-running observation leaves the fence up: it followed no running report.
void trial1009CancelledThenStoppedByTheApplication() {
    scenario = "trial 1009 replay cancelled after word 15, then an explicit HALT";
    Bench b;
    b.drive.afterStop = AFTER_15;
    b.ready();
    CHECK(b.start());
    for (unsigned i = 0; i < 100000 && b.session.velocityAction().commandEvidenceMask != 7; ++i)
        b.step();
    CHECK(b.session.velocityAction().commandEvidenceMask == 7 &&
          b.session.velocityAction().commandHistory[2].protocolAcknowledged);
    CHECK(b.session.cancelVelocity(b.now));
    b.runToTerminal();
    const L::VelocityContext& c = b.session.velocityAction();
    CHECK(c.state == M::ActionState::FAILED && c.interruption == M::VelocityInterruption::CANCELLED &&
          c.needsStop && b.session.velocityNeedsStop() && !c.stopRequested &&
          c.stopExecution == M::ActionExecution::NOT_TRANSMITTED);
    for (unsigned i = b.startFrame; i < b.wire.sentCount; ++i)
        CHECK(!b.downloadAt(i, 0x6040, 0x010f, 0x2b) && !b.downloadAt(i, 0x6040, 0x000b, 0x2b));
    const Saved<L::VelocityContext> cancelled(c);
    b.readState(b.check);
    const unsigned before = b.wire.sentCount;
    CHECK(b.session.requestStop(L::StopKind::HALT, b.check, stopPolicy(b, false), b.now) ==
          E::StopPath::CONFIRMED);
    b.runToTerminal();
    CHECK(b.downloadAt(before, 0x6040, 0x010f, 0x2b));
    const L::ActionContext& s = b.session.stopAction(L::StopKind::HALT);
    CHECK(s.state == M::ActionState::SUCCEEDED && s.completion == M::ActionCompletion::OBSERVED &&
          s.predecessorWord == 0x000f && s.stopModeRaw == 3 && s.statusObservation.statusword == 0x8033);
    CHECK(b.session.velocityNeedsStop());
    cancelled.unchanged(b.session.velocityAction()); // A separate stop never rewrites the run.
}
// The same run with word 15's reply lost: the latest acknowledged word is 7, so a HALT (010F) would be
// the first enable-operation word, and HALT allows no 000B substitute. No stop can be composed; the
// run ends with its stop recorded as not composed, the fence up and nothing sent.
void trial1009WithoutAcknowledgedFinalWordSignalsTheMissingStop() {
    scenario = "trial 1009 replay with word 15's reply lost";
    Bench b;
    b.drive.muteWord = 15;
    b.ready();
    CHECK(b.start());
    b.runToTerminal();
    const L::VelocityContext& c = b.session.velocityAction();
    const unsigned last = b.startFrame + 19;
    CHECK(b.downloadAt(last, 0x6040, 15, 0x2b) && b.wire.sentCount == last + 1);
    CHECK(c.state == M::ActionState::FAILED && c.firstFailure == M::ActionOutcome::SERVICE_TIMEOUT &&
          c.execution == M::ActionExecution::UNKNOWN && c.commandEvidenceMask == 7);
    CHECK(c.phase == L::VelocityPhase::STOPPING && c.stopRequested && !c.stopOperation &&
          c.interruption == M::VelocityInterruption::NONE);
    CHECK(c.stopOutcome == M::ActionOutcome::INVALIDATED &&
          c.stopExecution == M::ActionExecution::NOT_TRANSMITTED &&
          c.completion == M::ActionCompletion::NOT_OBSERVED);
    CHECK(c.needsStop && b.session.velocityNeedsStop() && !b.session.latestControl());
    CHECK(b.session.stopPath() != E::StopPath::CONFIRMED && b.session.stopAction(L::StopKind::HALT).state ==
                                                              M::ActionState::EMPTY);
    // The unanswered 15 leaves the route unresolved, so only the DEC-2 branch remains: the application
    // acts on the signal with an explicit QUICK_STOP of its own, never a HALT substitute.
    const unsigned before = b.wire.sentCount;
    const E::StopPath path =
        b.session.requestStop(L::StopKind::QUICK_STOP, b.session.velocityObservation(), stopPolicy(b, true), b.now);
    CHECK(path == E::StopPath::BEST_EFFORT || path == E::StopPath::HELD);
    b.runToTerminal();
    CHECK(b.wire.sentCount == before + 1 && b.downloadAt(before, 0x6040, 0x000b, 0x2b));
    CHECK(b.session.bestEffort().execution == M::ActionExecution::UNKNOWN && b.session.velocityNeedsStop());
}
// The same run with word 15 refused by an SDO abort: the route stays usable and the latest acknowledged
// word is 7, so again no stop can be composed. A fresh read then reports word 7's state in mode 3; while
// the fence stands that idle report no longer refuses the application's explicit DEC-2 000B.
void trial1009WithRejectedFinalWordLetsTheApplicationStop() {
    scenario = "trial 1009 replay with word 15 rejected";
    Bench b;
    b.drive.abortWord = 15;
    b.ready();
    CHECK(b.start());
    b.runToTerminal();
    const L::VelocityContext& c = b.session.velocityAction();
    CHECK(c.state == M::ActionState::FAILED && c.firstFailure == M::ActionOutcome::PROTOCOL_REJECTED &&
          c.execution == M::ActionExecution::REJECTED && c.needsStop && b.session.velocityNeedsStop());
    CHECK(c.phase == L::VelocityPhase::STOPPING && c.stopRequested && !c.stopOperation &&
          c.stopOutcome == M::ActionOutcome::INVALIDATED &&
          c.stopExecution == M::ActionExecution::NOT_TRANSMITTED);
    CHECK(b.wire.sentCount == b.startFrame + 20 && !b.session.latestControl());
    b.readState(b.check);
    L::StateObservation monitors, status;
    CHECK(L::getStateBlock(b.check, L::StateBlock::MONITORS, target(), b.now, 9 * SERVICE_US, monitors) &&
          monitors.monitor == 0x0001);
    CHECK(L::getStateBlock(b.check, L::StateBlock::STATUS, target(), b.now, 9 * SERVICE_US, status) &&
          status.statusword == 0x8033 && status.modeRaw == 3);
    const unsigned before = b.wire.sentCount;
    CHECK(b.session.requestStop(L::StopKind::HALT, b.check, stopPolicy(b, false), b.now) == E::StopPath::NONE);
    const E::StopPath path = b.session.requestStop(L::StopKind::QUICK_STOP, b.check, stopPolicy(b, true), b.now);
    CHECK(path == E::StopPath::BEST_EFFORT);
    b.runToTerminal();
    CHECK(b.wire.sentCount == before + 1 && b.downloadAt(before, 0x6040, 0x000b, 0x2b));
    CHECK(b.session.bestEffort().execution == M::ActionExecution::UNKNOWN && b.session.velocityNeedsStop());
}
// Backlog S17 through the owner and AxisSession, with BenchRoute's window. The lag below is an
// assumption of each case: no record has measured when this firmware reports the enable in mode 3.
const Report HALTED = {0x0001, 0x9437}; // Routes 1309 and 1310 after trial 1308: enabled, halted.
// The words, then word 15's cycle and the window's further cycles; returns the frame after them.
unsigned checkWindowFrames(const Bench& b, uint32_t windowCycles) {
    unsigned i = b.startFrame;
    CHECK(b.downloadAt(i, 0x60ff, 6, 0x23) && b.downloadAt(i + 1, 0x6083, 200, 0x23) &&
          b.downloadAt(i + 2, 0x6084, 200, 0x23));
    i += 3;
    const uint16_t words[] = {6, 7, 15};
    for (uint16_t word : words) {
        CHECK(b.downloadAt(i, 0x6040, word, 0x2b) && b.cycleAt(i + 1));
        i += 8;
    }
    for (uint32_t k = 0; k < windowCycles; ++k, i += 7)
        CHECK(b.cycleAt(i));
    return i;
}
// 1308's 8033/0001 for three cycles after word 15, then C437/0004 with 606C 5: the run waits inside
// the window, runs, sends its HALT at due and settles it, because a running report preceded it.
void windowWaitsForTheEnableReport() {
    scenario = "S17 window: 8033/0001 for three cycles, then C437/0004";
    Bench b;
    b.windowed = true;
    b.drive.enableAfterCycles = 3;
    b.drive.afterStop = HALTED;
    b.ready();
    CHECK(b.start());
    b.runToTerminal();
    const L::VelocityContext& c = b.session.velocityAction();
    CHECK(c.state == M::ActionState::SUCCEEDED && c.outcome == M::ActionOutcome::OBSERVED &&
          c.completion == M::ActionCompletion::OBSERVED && c.firstFailure == M::ActionOutcome::NONE);
    CHECK(c.runningObserved && !c.needsStop && !b.session.velocityNeedsStop());
    CHECK(c.enablePending == 3 && c.pendingFirst.cycle == 3 && c.pendingFirst.statusword == 0x8033 &&
          c.pendingLast.cycle == 5 && c.pendingLast.monitor == 0x0001 && c.enableReport.cycle == 6 &&
          c.enableReport.statusword == 0xc437 && c.enableReport.monitor == 0x0004 && c.enableReport.velocity == 5);
    const uint64_t ack = c.commandHistory[2].completedUs;
    CHECK(c.pendingLast.earliestUs >= c.pendingFirst.latestUs + 2 * 300000 &&
          c.enableReport.earliestUs >= c.pendingLast.latestUs + 300000 && c.enableReport.earliestUs - ack <= 2000000);
    // The words; after word 15's cycle two more still pending and the enable report's; the running
    // cycles; then one HALT at or after due.
    unsigned i = checkWindowFrames(b, 3);
    unsigned running = 0;
    for (; b.cycleAt(i); i += 7)
        ++running;
    CHECK(running == c.runningPolls && c.runningPolls >= 1 && c.runningPolls <= 3);
    CHECK(b.downloadAt(i, 0x6040, 0x010f, 0x2b));
    const L::ActionContext& s = b.session.stopAction(L::StopKind::HALT);
    CHECK(s.operation == c.stopOperation && s.state == M::ActionState::SUCCEEDED &&
          s.startedUs >= c.stopDueUs && s.deadlineUs == c.deadlineUs);
    unsigned downloads = 0;
    for (unsigned k = i + 1; k < b.wire.sentCount; ++k)
        downloads += b.wire.sent[k].data[0] != 0x40;
    CHECK(!downloads);
}
// 8033/0001 to the end of the window: one HALT, at the cycle after which no further one fits; it
// settles nothing, as in trial 1308, and the fence holds.
void windowExpiresWithOneHalt() {
    scenario = "S17 window: 8033/0001 to the end of the window";
    Bench b;
    b.windowed = true;
    b.drive.afterStop = AFTER_15;
    b.ready();
    CHECK(b.start());
    b.runToTerminal();
    const L::VelocityContext& c = b.session.velocityAction();
    CHECK(c.state == M::ActionState::FAILED && c.firstFailure == M::ActionOutcome::OBSERVATION_SCOPE &&
          std::strcmp(c.status.msg, "final enable not reported within its window") == 0);
    CHECK(c.enablePending >= 2 && c.enablePending <= 2000000 / 300000 + 1 && !c.enableReport.present &&
          c.observations == 2 + c.enablePending && !c.runningPolls);
    const uint64_t ack = c.commandHistory[2].completedUs;
    CHECK(c.pendingLast.earliestUs - ack <= 2000000 + SERVICE_US && c.stopRequestedUs + 300000 - ack > 2000000);
    const unsigned halt = checkWindowFrames(b, c.enablePending - 1);
    CHECK(b.downloadAt(halt, 0x6040, 0x010f, 0x2b));
    unsigned downloads = 0;
    for (unsigned k = halt + 1; k < b.wire.sentCount; ++k)
        downloads += b.wire.sent[k].data[0] != 0x40;
    CHECK(!downloads);
    CHECK(c.stopExecution == M::ActionExecution::ACKNOWLEDGED && c.stopOutcome == M::ActionOutcome::OBSERVED &&
          c.completion == M::ActionCompletion::NOT_OBSERVED && c.needsStop && b.session.velocityNeedsStop());
    std::fprintf(stderr, "S17 window expiry: %u window cycles, HALT %llu us after word 15's acknowledgement\n",
                 unsigned(c.enablePending), static_cast<unsigned long long>(c.stopRequestedUs - ack));
}
// Backlog S18 through the owner and AxisSession: trials 1513 (node 2, 30 rpm) and 1607 (node 1, 60 rpm) on
// 2026-11-10 (docs/bench/20261110T1038Z-s18-velocity-node2-1513 and 20261110T1043Z-s19-velocity-node1-1607,
// with the later reads 1514 and 1608), replayed on this fixture's node 1 route with BenchRoute's plan for
// those lines: 12 s, interval 4000 ms, three polls, deadline 17000 ms, the S17 window, QUICK_STOP. Each value
// below is the capture's, except where marked: the stop's 6064 and 606C, which that stop did not read, and
// every report of a later run or stop, which no record holds.
struct Cycle {
    uint16_t monitor, status;
    uint8_t mode;
    int32_t position, velocity;
};
// Every cycle begins with its 2100 read and takes one Cycle, chosen by the latest 6040 word and the cycles
// read since it; a list repeats its last entry.
struct ScriptDrive {
    Cycle start = {0x0011, 0x8050, 3, 0, 0};  // TRIAL_CHECK of both trials.
    Cycle after6 = {0x0011, 0x8031, 3, 0, 0}; // The cycle after word 6.
    Cycle after7 = {0x0001, 0x8033, 3, 0, 0}; // After word 7.
    Cycle pending = {0x0001, 0x8033, 3, 0, 0}; // After word 15, for pendingCycles cycles.
    unsigned pendingCycles = 2;
    const Cycle* running = nullptr; // After 15 and the pending cycles.
    unsigned runningCount = 0;
    const Cycle* quick = nullptr; // After 000B.
    unsigned quickCount = 0;
    const Cycle* halted = nullptr; // After 010F.
    unsigned haltedCount = 0;
    uint16_t word = 0;
    unsigned cycles = 0;
    Cycle current = start;
    static Cycle pick(const Cycle* list, unsigned count, unsigned at) {
        CHECK(list && count);
        return list[at < count ? at : count - 1];
    }
    Cycle select() const {
        switch (word) {
        case 0: return start;
        case 6: return after6;
        case 7: return after7;
        case 15: return cycles < pendingCycles ? pending : pick(running, runningCount, cycles - pendingCycles);
        case 0x000b: return pick(quick, quickCount, cycles);
        default: CHECK(word == 0x010f); return pick(halted, haltedCount, cycles);
        }
    }
    bool respond(const M::Frame& sent, M::Frame& out) {
        const uint16_t index = indexOf(sent);
        const uint8_t sub = sent.data[3];
        out = frame(0x581, 8);
        out.data[1] = sent.data[1];
        out.data[2] = sent.data[2];
        out.data[3] = sub;
        if (sent.data[0] != 0x40) {
            CHECK(index == 0x60ff || index == 0x6083 || index == 0x6084 || index == 0x6040);
            if (index == 0x6040) {
                word = uint16_t(valueOf(sent));
                cycles = 0;
            }
            out.data[0] = 0x60;
            return true;
        }
        uint32_t raw = 0;
        unsigned size = 2;
        switch (index) {
        case 0x1000: raw = 0x00040192; size = 4; break; // TRIAL_VELOCITY_EVIDENCE of both trials.
        case 0x1009: raw = 0x0c; size = 1; break;
        case 0x100a: raw = 0x20; size = 1; break;
        case 0x1018:
            raw = !sub ? 5u : sub == 1 ? 0x1000u : sub == 2 ? 1u : 0u;
            size = sub ? 4 : 1;
            break;
        case 0x2000: raw = 0x0400; break;
        case 0x2001: raw = 0x0200; break;
        case 0x1001: size = 1; break;
        case 0x603f: break;
        case 0x2100:
            current = select();
            ++cycles;
            raw = current.monitor;
            break;
        case 0x2101: case 0x2102: break;
        case 0x6041: raw = current.status; break;
        case 0x6061: raw = current.mode; size = 1; break;
        case 0x6064: raw = uint32_t(current.position); size = 4; break;
        case 0x606c: raw = uint32_t(current.velocity); size = 4; break;
        default: CHECK(false);
        }
        out.data[0] = size == 1 ? 0x4f : size == 2 ? 0x4b : 0x43;
        for (unsigned i = 0; i < 4; ++i)
            out.data[4 + i] = uint8_t(raw >> (8 * i));
        return true;
    }
};
typedef BenchOf<ScriptDrive> ScriptBench;
// The running cycles after word 15's two pending cycles: the enable report, then the two polls.
const Cycle RUN_1513[3] = {{0x0004, 0xc437, 3, 88, 30}, {0x0004, 0xc437, 3, 2097, 30}, {0x0004, 0xc437, 3, 4105, 30}};
const Cycle RUN_1607[3] = {{0x0004, 0xc437, 3, 179, 60}, {0x0004, 0xc437, 3, 4196, 60}, {0x0004, 0xc437, 3, 8214, 58}};
// The stop: 2100 0014, 6041 C050, 6061 3 in its first cycle (op 30). Its 6064 and 606C are the later read's
// (1514: 5608, 1608: 11219, both 606C 0), which also gives the second cycle (2100 0015, 6041 C050).
const Cycle STOP_1513[2] = {{0x0014, 0xc050, 3, 5608, 0}, {0x0015, 0xc050, 3, 5608, 0}};
const Cycle STOP_1607[2] = {{0x0014, 0xc050, 3, 11219, 0}, {0x0015, 0xc050, 3, 11219, 0}};
void planS18(ScriptBench& b, uint32_t record, int64_t rpm, const Cycle* running, const Cycle* stop, unsigned stops) {
    b.windowed = true;
    b.record = record;
    b.rpm = rpm;
    b.kind = L::StopKind::QUICK_STOP;
    b.pollUs = 4000000;
    b.deadlineAfterUs = 17000000;
    b.drive.running = running;
    b.drive.runningCount = 3;
    b.drive.quick = stop;
    b.drive.quickCount = stops;
}
unsigned wordsSent(const ScriptBench& b, unsigned from, uint32_t word) {
    unsigned count = 0;
    for (unsigned i = from; i < b.wire.sentCount; ++i)
        count += b.downloadAt(i, 0x6040, word, 0x2b);
    return count;
}
// After the 000B: two seven-field cycles four seconds apart, then SUCCEEDED and no fence. The same power-up
// then runs again from the latched release, its HALT leaves the drive halted, and the attendant's
// experiment 000B from that halted state releases it. Those later reports are assumptions.
void trials1513And1607SettleOnReleasedStandstill() {
    for (unsigned trial = 0; trial < 2; ++trial) {
        scenario = trial ? "S18 replay 1607" : "S18 replay 1513";
        ScriptBench b;
        planS18(b, trial ? 1607 : 1513, trial ? 60 : 30, trial ? RUN_1607 : RUN_1513, trial ? STOP_1607 : STOP_1513, 2);
        b.ready();
        CHECK(b.start());
        b.runToTerminal();
        const L::VelocityContext& c = b.session.velocityAction();
        const L::ActionContext& s = b.session.stopAction(L::StopKind::QUICK_STOP);
        CHECK(c.state == M::ActionState::SUCCEEDED && c.outcome == M::ActionOutcome::OBSERVED &&
              c.completion == M::ActionCompletion::OBSERVED && c.firstFailure == M::ActionOutcome::NONE);
        CHECK(!c.needsStop && !b.session.velocityNeedsStop() && c.runningObserved && c.enablePending == 2 &&
              c.enableReport.present && c.enableReport.statusword == 0xc437 && c.runningPolls == 2);
        CHECK(s.operation == c.stopOperation && s.state == M::ActionState::SUCCEEDED &&
              s.outcome == M::ActionOutcome::OBSERVED && s.observations == 2 && s.standstillObserving &&
              s.predecessorWord == 0x000f && s.stopModeRaw == 3 && s.monitorObservation.monitor == 0x0015 &&
              s.statusObservation.statusword == 0xc050 && s.extraObservation.actualPosition == (trial ? 11219 : 5608));
        CHECK(s.startedUs >= c.stopDueUs && s.deadlineUs == c.deadlineUs);
        // The 000B once, then exactly two seven-field cycles, the second at least 4 s after the first.
        unsigned at = b.startFrame;
        while (at < b.wire.sentCount && !b.downloadAt(at, 0x6040, 0x000b, 0x2b))
            ++at;
        CHECK(at < b.wire.sentCount && b.cycleAt(at + 1) && b.cycleAt(at + 8) && b.wire.sentCount == at + 15);
        CHECK(wordsSent(b, b.startFrame, 0x000b) == 1 && !wordsSent(b, b.startFrame, 0x010f));
        CHECK(s.extraObservation.window.earliestUs >= s.commandHistory[0].completedUs + 4000000);
        // The same power-up runs again from 2100 0015, 6041 C050. Assumed: word 6's cycle keeps the latch,
        // word 7's clears it (as word 7 did in mode 1, trial 308); the run then halts at due.
        const int32_t at0 = trial ? 11219 : 5608;
        const Cycle second[1] = {{0x0004, 0xc437, 3, at0 + 100, 30}};
        const Cycle halted[1] = {{0x0001, 0x9437, 3, at0 + 300, 0}};
        const Cycle after6 = {0x0015, 0x8031, 3, at0, 0}, after7 = {0x0001, 0x8033, 3, at0, 0};
        b.drive.after6 = after6;
        b.drive.after7 = b.drive.pending = after7;
        b.drive.running = second;
        b.drive.runningCount = 1;
        b.drive.halted = halted;
        b.drive.haltedCount = 1;
        b.kind = L::StopKind::HALT;
        b.rewind();
        b.readState(b.state);
        CHECK(b.start());
        b.runToTerminal();
        const L::VelocityContext& v = b.session.velocityAction();
        CHECK(v.state == M::ActionState::SUCCEEDED && v.admission.monitor == 0x0015 &&
              v.admission.statusword == 0xc050 && v.admission.position == at0 && !b.session.velocityNeedsStop());
        // Halted and enabled in mode 3 after the run's own HALT. HALT and an unflagged QUICK_STOP stay
        // refused by the idle report; the attendant's flagged QUICK_STOP sends one 000B, released here.
        b.rewind();
        b.readState(b.check);
        const unsigned before = b.wire.sentCount;
        CHECK(b.session.requestStop(L::StopKind::QUICK_STOP, b.check, stopPolicy(b, false), b.now) ==
              E::StopPath::NONE);
        E::StopPolicy flagged = stopPolicy(b, false);
        flagged.attendedExperiment = true;
        CHECK(b.session.requestStop(L::StopKind::HALT, b.check, flagged, b.now) == E::StopPath::NONE);
        CHECK(b.wire.sentCount == before);
        const Cycle released[2] = {{0x0010, 0x8050, 3, at0 + 300, 0}, {0x0011, 0x8050, 3, at0 + 300, 0}};
        b.drive.quick = released;
        b.drive.quickCount = 2;
        CHECK(b.session.requestStop(L::StopKind::QUICK_STOP, b.check, flagged, b.now) == E::StopPath::CONFIRMED);
        b.runToTerminal();
        const L::ActionContext& q = b.session.stopAction(L::StopKind::QUICK_STOP);
        CHECK(q.state == M::ActionState::SUCCEEDED && q.predecessorWord == 0x010f && q.standstillObserving &&
              q.observations == 2 && wordsSent(b, before, 0x000b) == 1 && b.wire.sentCount == before + 15);
        CHECK(!b.session.velocityNeedsStop() && b.session.velocityAction().state == M::ActionState::SUCCEEDED);
    }
}
// The same run with a stop that shows no released standstill in its two cycles: the run stays fenced, and
// nothing follows the one 000B.
void quickStopWithoutStandstillStaysFenced() {
    const Cycle cases[4][2] = {
        {{0x0014, 0xc050, 3, 5600, 0}, {0x0015, 0xc050, 3, 5608, 0}}, // 6064 moved between the cycles.
        {{0x0014, 0xc050, 3, 5608, 2}, {0x0015, 0xc050, 3, 5608, 0}}, // 606C nonzero.
        {{0x0014, 0xc050, 3, 5608, 0}, {0x001d, 0xc050, 3, 5608, 0}}, // Alarm.
        {{0x0014, 0xc050, 3, 5608, 0}, {0x0015, 0xc050, 1, 5608, 0}}, // Mode 1.
    };
    for (unsigned k = 0; k < 4; ++k) {
        scenario = "S18 replay 1513 without a standstill";
        ScriptBench b;
        planS18(b, 1513, 30, RUN_1513, cases[k], 2);
        b.ready();
        CHECK(b.start());
        b.runToTerminal();
        const L::VelocityContext& c = b.session.velocityAction();
        const L::ActionContext& s = b.session.stopAction(L::StopKind::QUICK_STOP);
        CHECK(s.state == M::ActionState::FAILED && s.observations == 2 &&
              s.outcome == (k < 2 ? M::ActionOutcome::DEADLINE : M::ActionOutcome::OBSERVATION_SCOPE));
        CHECK(c.state == M::ActionState::FAILED && c.needsStop && b.session.velocityNeedsStop() &&
              c.completion == M::ActionCompletion::NOT_OBSERVED && c.stopExecution == M::ActionExecution::ACKNOWLEDGED);
        CHECK(wordsSent(b, b.startFrame, 0x000b) == 1);
    }
}
// An explicit QUICK_STOP while velocityNeedsStop holds: the run is cancelled locally once running, and a
// fresh read reports it running after its 000F, so the ordinary mode 3 admission takes the stop. The stop's
// released standstill then clears the fence; the cancelled run keeps its own record.
void explicitQuickStopAfterARunningReportClearsTheFence() {
    scenario = "S18: explicit quick stop after a running report";
    ScriptBench b;
    planS18(b, 1513, 30, RUN_1513, STOP_1513, 2);
    b.ready();
    CHECK(b.start());
    for (unsigned i = 0; i < 100000 && !b.session.velocityAction().runningObserved; ++i)
        b.step();
    CHECK(b.session.cancelVelocity(b.now));
    b.runToTerminal();
    CHECK(b.session.velocityNeedsStop() && b.session.velocityAction().state == M::ActionState::FAILED &&
          !wordsSent(b, b.startFrame, 0x000b));
    const Saved<L::VelocityContext> cancelled(b.session.velocityAction());
    b.readState(b.check);
    const unsigned before = b.wire.sentCount;
    CHECK(b.session.requestStop(L::StopKind::QUICK_STOP, b.check, stopPolicy(b, false), b.now) ==
          E::StopPath::CONFIRMED);
    b.runToTerminal();
    const L::ActionContext& s = b.session.stopAction(L::StopKind::QUICK_STOP);
    CHECK(s.state == M::ActionState::SUCCEEDED && s.standstillObserving && s.observations == 2 &&
          s.predecessorWord == 0x000f && wordsSent(b, before, 0x000b) == 1 && b.wire.sentCount == before + 15);
    CHECK(!b.session.velocityNeedsStop());
    cancelled.unchanged(b.session.velocityAction());
}
// After an MCU restart node 1 still reads halted (trial 1707's report: 6041 9437, 2100 0001, mode 3, 606C
// 0) with no controlword history in this boot. Only the attendant's flagged QUICK_STOP with a best-effort
// deadline takes one DEC-2 000B; it never succeeds, clears nothing and is not repeated. Everything else is
// refused with nothing sent.
void haltedAfterRestartTakesOneBestEffortStop() {
    scenario = "S18: halted after an MCU restart";
    const Cycle halted = {0x0001, 0x9437, 3, 1131, 0};
    // A running report, and an enabled one in mode 1, keep their own best-effort path (stateRefuses), so
    // they are not among these.
    const Cycle others[] = {{0x0009, 0x9437, 3, 1131, 0}, {0x0011, 0x9437, 3, 1131, 0},
                            {0x0001, 0x9033, 3, 1131, 0}, {0x0001, 0x9437, 3, 1131, 2}};
    for (const Cycle& k : others) {
        ScriptBench b;
        b.drive.start = k;
        b.ready();
        E::StopPolicy flagged = stopPolicy(b, true);
        flagged.attendedExperiment = true;
        const unsigned before = b.wire.sentCount;
        CHECK(b.session.requestStop(L::StopKind::QUICK_STOP, b.state, flagged, b.now) == E::StopPath::NONE);
        CHECK(b.wire.sentCount == before);
    }
    ScriptBench b;
    b.drive.start = halted;
    b.ready();
    CHECK(!b.session.latestControl() && !b.session.motionHistoryWritten());
    E::StopPolicy flagged = stopPolicy(b, true);
    flagged.attendedExperiment = true;
    E::StopPolicy noBestEffort = stopPolicy(b, false);
    noBestEffort.attendedExperiment = true;
    const unsigned before = b.wire.sentCount;
    CHECK(b.session.requestStop(L::StopKind::QUICK_STOP, b.state, stopPolicy(b, true), b.now) == E::StopPath::NONE);
    CHECK(b.session.requestStop(L::StopKind::HALT, b.state, flagged, b.now) == E::StopPath::NONE);
    CHECK(b.session.requestStop(L::StopKind::QUICK_STOP, b.state, noBestEffort, b.now) == E::StopPath::NONE);
    CHECK(b.wire.sentCount == before);
    const E::StopPath path = b.session.requestStop(L::StopKind::QUICK_STOP, b.state, flagged, b.now);
    CHECK(path == E::StopPath::BEST_EFFORT || path == E::StopPath::HELD);
    b.runToTerminal();
    CHECK(b.wire.sentCount == before + 1 && b.downloadAt(before, 0x6040, 0x000b, 0x2b));
    CHECK(b.session.bestEffort().state == M::ActionState::FAILED &&
          b.session.bestEffort().execution == M::ActionExecution::UNKNOWN && !b.session.velocityNeedsStop() &&
          b.session.stopAction(L::StopKind::QUICK_STOP).state == M::ActionState::EMPTY);
    // Its own write ends the exception: the same fresh halted report now refuses a second 000B.
    E::StopPolicy again = stopPolicy(b, true);
    again.attendedExperiment = true;
    CHECK(b.session.motionHistoryWritten() &&
          b.session.requestStop(L::StopKind::QUICK_STOP, b.state, again, b.now) == E::StopPath::NONE);
    CHECK(b.wire.sentCount == before + 1);
    // Any write of this object in this boot ends the exception too, with no controlword and a fresh
    // halted report after that write.
    ScriptBench w;
    w.drive.start = halted;
    w.ready();
    C::SdoTransfer t;
    t.channel = target().channel;
    t.operation = t.request = w.session.beginOperation(w.now + 20 * SERVICE_US, w.now);
    t.step = 1;
    t.direction = C::SdoDirection::DOWNLOAD;
    t.index = 0x6083;
    t.size = 4;
    t.data[0] = 200;
    t.deadlineUs = w.now + 20 * SERVICE_US;
    t.queueTimeoutUs = QUEUE_US;
    t.responseTimeoutUs = RESPONSE_US;
    CHECK(t.operation && w.session.admitPrepared(t, w.now) == O::BusAdmission::ACCEPTED);
    w.step();
    w.step();
    C::SdoCompletion written;
    CHECK(w.session.settle(written) && written.outcome == C::SdoOutcome::SUCCESS);
    CHECK(w.session.motionHistoryWritten() && !w.session.latestControl());
    w.readState(w.check);
    E::StopPolicy late = stopPolicy(w, true);
    late.attendedExperiment = true;
    const unsigned sent = w.wire.sentCount;
    CHECK(w.session.requestStop(L::StopKind::QUICK_STOP, w.check, late, w.now) == E::StopPath::NONE &&
          w.wire.sentCount == sent);
}
// Trials 1742 (from 2100 0015, 6041 C050 at 8412) and 1762 (from 0011/8050 at 8419), node 2 on 2026-11-10
// (docs/bench/20261110T1134Z-s20-c3-velocity-node2-1742, 20261110T1135Z-s20-c4-velocity-node2-1762), as
// captured up to word 15's first cycle: word 6's cycle repeated the admission 6064 (1742 with the latch
// still set, C031), word 7's read 0001/8033 at 8424 and word 15's first cycle 8033 at 8424, 606C 0. Both
// failed there against the admission and sent their own 000B. From word 15's second cycle on (one more
// pending cycle, the enable report, running, the stop) the reports are assumptions. A third variant moves
// 6064 after word 7 and must still fail with the run's own stop, which the drive answers as in 1742.
void trials1742And1762PassTheWindowAfterWord7sPull() {
    const Cycle starts[2][3] = {
        {{0x0015, 0xc050, 3, 8412, 0}, {0x0015, 0xc031, 3, 8412, 0}, {0x0001, 0x8033, 3, 8424, 0}},
        {{0x0011, 0x8050, 3, 8419, 0}, {0x0011, 0x8031, 3, 8419, 0}, {0x0001, 0x8033, 3, 8424, 0}}};
    const Cycle running[3] = {{0x0004, 0xc437, 3, 8500, 30}, {0x0004, 0xc437, 3, 10500, 30},
                              {0x0004, 0xc437, 3, 12500, 30}};
    const Cycle stop[2] = {{0x0014, 0xc050, 3, 14000, 0}, {0x0015, 0xc050, 3, 14000, 0}};
    const Cycle answered[1] = {{0x0001, 0x8050, 3, 8425, 0}}; // 1742's stop cycle: 2100 0001, 6041 8050.
    for (unsigned k = 0; k < 3; ++k) {
        scenario = k == 0 ? "S18 window: trial 1742" : k == 1 ? "S18 window: trial 1762" : "S18 window: moved after 7";
        const Cycle* start = starts[k == 0 ? 0 : 1];
        ScriptBench b;
        planS18(b, k == 0 ? 1742 : 1762, 30, running, k == 2 ? answered : stop, k == 2 ? 1 : 2);
        b.drive.start = start[0];
        b.drive.after6 = start[1];
        b.drive.after7 = start[2];
        b.drive.pending = start[2];
        if (k == 2)
            b.drive.pending.position = 8425;
        b.ready();
        CHECK(b.start());
        b.runToTerminal();
        const L::VelocityContext& c = b.session.velocityAction();
        CHECK(c.admission.position == start[0].position && c.commandEvidenceMask == 7);
        if (k < 2) {
            CHECK(c.state == M::ActionState::SUCCEEDED && c.enablePending == 2 && c.pendingFirst.position == 8424 &&
                  c.enableReport.present && c.runningObserved && !b.session.velocityNeedsStop());
        } else {
            CHECK(c.state == M::ActionState::FAILED && !c.enablePending && c.latest.position == 8425 &&
                  c.firstFailure == M::ActionOutcome::OBSERVATION_SCOPE &&
                  std::strcmp(c.status.msg, "final enable state is not confirmed") == 0);
            CHECK(c.stopExecution == M::ActionExecution::ACKNOWLEDGED && wordsSent(b, b.startFrame, 0x000b) == 1 &&
                  c.needsStop && b.session.velocityNeedsStop());
        }
    }
}
// S16's stop on the 000F alone: word 15 never reported within the window, so no running report came before
// the run's QUICK_STOP. A released report then ends the stop outside its scope and the fence holds.
void unqualifiedQuickStopKeepsTheFence() {
    scenario = "S18: quick stop on the 000F alone";
    ScriptBench b;
    const Cycle released[1] = {{0x0011, 0x8050, 3, 0, 0}};
    planS18(b, 1513, 30, RUN_1513, released, 1);
    b.drive.pendingCycles = 100;
    b.ready();
    CHECK(b.start());
    b.runToTerminal();
    const L::VelocityContext& c = b.session.velocityAction();
    const L::ActionContext& s = b.session.stopAction(L::StopKind::QUICK_STOP);
    CHECK(std::strcmp(c.status.msg, "final enable not reported within its window") == 0 && !c.runningObserved);
    CHECK(!s.standstillObserving && s.read.count == 5 && s.outcome == M::ActionOutcome::OBSERVATION_SCOPE &&
          s.observations == 1 && wordsSent(b, b.startFrame, 0x000b) == 1);
    CHECK(c.state == M::ActionState::FAILED && c.needsStop && b.session.velocityNeedsStop());
}
} // namespace Replay1009
} // namespace
int main() {
    completeRunsKeepPreparationAnchor();
    attendedExperimentThroughSession();
    exactPollBudgetCoversSetupAndTheWholeDuration();
    unsupportedStartsPublishNothing();
    releasedModeChangeAndQualifiedReverse();
    completedStopCannotReuseOldReleasedAdmission();
    staleGenerationAtControllerHandover();
    if (PAIRED_CHANNELS) { // The ordinary pressure comes from node 2's reservation.
        ordinaryResultsPressureCannotRestartDuration();
        urgentStopSurvivesOrdinaryPressure();
    }
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
    Replay1009::trial1009StopsAfterTheLaggingEnable();
    Replay1009::windowWaitsForTheEnableReport();
    Replay1009::windowExpiresWithOneHalt();
    Replay1009::trial1009WithoutAcknowledgedFinalWordSignalsTheMissingStop();
    Replay1009::trial1009WithRejectedFinalWordLetsTheApplicationStop();
    Replay1009::trial1009CancelledThenStoppedByTheApplication();
    Replay1009::trials1513And1607SettleOnReleasedStandstill();
    Replay1009::quickStopWithoutStandstillStaysFenced();
    Replay1009::unqualifiedQuickStopKeepsTheFence();
    Replay1009::explicitQuickStopAfterARunningReportClearsTheFence();
    Replay1009::haltedAfterRestartTakesOneBestEffortStop();
    Replay1009::trials1742And1762PassTheWindowAfterWord7sPull();
    std::printf("velocity owner: %u checks\n", checks);
}
