// SPDX-License-Identifier: MIT
// Actual AxisSession/owner integration with manufactured drive reports, never firmware evidence.
#include <cstdio>
#include <cstdlib>
#include <cstring>
static unsigned checks = 0;
static bool asyncTraffic = false;
static const char* scenario = "axis change";
#define CHECK(condition)                                                                           \
    do {                                                                                           \
        ++checks;                                                                                  \
        if (!(condition)) {                                                                        \
            std::fprintf(stderr, "%s:%d: %s [%s]\n", __FILE__, __LINE__, #condition, scenario);    \
            std::abort();                                                                          \
        }                                                                                          \
    } while (false)
#include "support/MoveRig.h"
#include "support/VelocityRig.h"
namespace {
using namespace MoveRig;
struct Fixture {
    Rig r;
    L::MoveReplacementRequest request;
    L::MoveReplacementEvidence evidence;
    Fixture() {
        r.ready();
        r.drive.travelReads = 100;
        r.drive.velocity = 30;
        CHECK(r.request(MoveRig::evidence(), moveRequest(TARGET, 30)));
        for (unsigned i = 0; i < 300 && !r.a.moveAction().latest.present; ++i)
            r.step();
        CHECK(r.a.moveAction().phase == L::MovePhase::WAITING &&
              r.a.moveAction().acceptance.present);
        request.request = moveRequest(TARGET, 60);
        request.profile = profile();
        request.fields = 2;
        request.mode = L::MoveReplacementMode::IMMEDIATE;
        evidence.gate = L::MoveChangeGate::ATTENDED_EXPERIMENT;
        evidence.record = 52;
        evidence.model = r.a.moveAction().evidence.model;
        evidence.firmware = r.a.moveAction().evidence.firmware;
    }
    M::Status replace() { return r.a.requestMoveReplacement(request, evidence, nullptr, r.now); }
    E::StopPolicy policy() {
        E::StopPolicy p;
        p.options.service = reads();
        p.options.observationIntervalUs = 1000;
        p.options.maximumAgeUs = uint64_t(QUEUE + uint64_t(RESPONSE)) * 9;
        p.deadlineUs = r.now + 200000;
        p.bestEffortDeadlineUs = r.now + 300000;
        return p;
    }
    void finish() {
        for (unsigned i = 0; i < 800 && r.a.busy(); ++i)
            r.step();
        CHECK(!r.a.busy());
    }
};
void relationsAndStops() {
    for (unsigned boundary = 0; boundary < 5; ++boundary) {
        Fixture f;
        const uint32_t original = f.r.a.moveAction().operation;
        const uint64_t cutoff = f.r.a.moveAction().deadlineUs;
        const O::SequenceId sequence = f.r.a.sequence();
        const auto setup = f.r.a.moveAction().setupHistory[0];
        const auto trigger = f.r.a.moveAction().trigger;
        const unsigned first = f.r.wire.sentCount;
        CHECK(f.replace());
        CHECK(f.r.a.sequence().generation == sequence.generation);
        CHECK(f.r.a.moveAction().deadlineUs == cutoff);
        CHECK(f.r.a.originalMoveAction().operation == original);
        CHECK(f.r.a.originalMoveAction().interruption == M::MoveInterruption::REPLACED);
        CHECK(f.r.a.originalMoveAction().relation.observationSuperseded);
        CHECK(f.r.a.moveAction().relation.observationSuperseded);
        CHECK(f.r.a.originalMoveAction().setupHistory[0].completedUs == setup.completedUs);
        CHECK(f.r.a.originalMoveAction().trigger.completedUs == trigger.completedUs);
        L::PreparedMove retainedWork;
        const auto live = f.r.reservation();
        const auto current = f.r.current();
        CHECK(L::nextMove(f.r.a.originalMoveAction(), f.r.a.moveObservation(), live, current,
                          f.r.now, retainedWork));
        CHECK(!retainedWork.ready && !retainedWork.observe);
        CHECK(f.r.a.moveObservation().state ==
              M::ReadState::SUCCEEDED); // Stop reserve includes actual prior evidence.
        CHECK(!f.replace()); // No second outstanding/replacement result overwriting this pair.
        if (boundary == 1)
            f.r.a.service(f.r.now); // Queued, not transmitted.
        if (boundary == 2) {
            f.r.a.service(f.r.now);
            f.r.step(); // 6081 handed to transport, no reply consumed yet.
        }
        if (boundary >= 3) {
            for (unsigned i = 0; i < 300 && !(boundary == 3 ? f.r.a.moveAction().clearPresent
                                                            : f.r.a.moveAction().triggerPresent);
                 ++i)
                f.r.step();
            CHECK(boundary == 3 ? f.r.a.moveAction().clearPresent
                                : f.r.a.moveAction().triggerPresent);
        }
        CHECK(f.r.a.requestStop(L::StopKind::QUICK_STOP, f.r.a.moveObservation(), f.policy(),
                                f.r.now) != E::StopPath::NONE);
        CHECK(!f.replace());
        f.finish();
        CHECK(f.r.a.moveAction().interruption == M::MoveInterruption::STOP);
        CHECK(f.r.a.moveAction().uncertain && f.r.a.moveFenced());
        CHECK(wordsFrom(f.r, first, 0x000b) == 1);
        CHECK(!wordsFrom(f.r, first, 6) && !wordsFrom(f.r, first, 7));
        if (boundary <= 1)
            CHECK(downloadsFrom(f.r, first, 0x6081) == 0);
        CHECK(f.r.a.originalMoveAction().trigger.completedUs == trigger.completedUs);
    }
}
void refusedStopIsSticky() {
    Fixture f;
    const unsigned before = f.r.wire.sentCount;
    E::StopPolicy invalid;
    CHECK(f.r.a.requestStop(L::StopKind::HALT, f.r.a.moveObservation(), invalid, f.r.now - 1) ==
          E::StopPath::NONE);
    CHECK(!f.replace());
    f.r.steps(4);
    CHECK(f.r.wire.sentCount == before && !f.r.a.moveActive());
}
void malformedRelationsAreRefused() {
    Fixture f;
    CHECK(f.replace());
    const auto live = f.r.reservation();
    const auto current = f.r.current();
    for (unsigned role = 0; role != 2; ++role) {
        for (unsigned variant = 0; variant != 5; ++variant) {
            L::MoveContext invalid = role ? f.r.a.moveAction() : f.r.a.originalMoveAction();
            switch (variant) {
            case 0:
                invalid.relation.originalOperationId = 0;
                break;
            case 1:
                invalid.relation.replacementOperationId = 0;
                break;
            case 2:
                invalid.relation.originalOperationId = invalid.relation.replacementOperationId;
                break;
            case 3:
                invalid.relation.originalOperationId = invalid.relation.replacementOperationId + 1;
                break;
            default:
                invalid.operation = invalid.relation.replacementOperationId + 1;
                break;
            }
            L::PreparedMove output;
            const Saved<L::PreparedMove> unchanged(output);
            CHECK(!L::nextMove(invalid, f.r.a.moveObservation(), live, current, f.r.now, output));
            unchanged.unchanged(output);
        }
    }
    L::MoveContext invalid = f.r.a.originalMoveAction();
    invalid.relation.observationSuperseded = false;
    L::PreparedMove output;
    CHECK(!L::nextMove(invalid, f.r.a.moveObservation(), live, current, f.r.now, output));
}
void lateOriginalCompletionRefusesHandoff() {
    Fixture f;
    const uint32_t original = f.r.a.moveAction().operation;
    f.r.drive.remaining = 1;
    for (unsigned i = 0;
         i < 200 && f.r.a.moveAction().completion == M::ActionCompletion::NOT_OBSERVED; ++i)
        f.r.step();
    CHECK(f.r.a.moveAction().completion == M::ActionCompletion::OBSERVED);
    const unsigned before = f.r.wire.sentCount;
    CHECK(!f.replace());
    CHECK(f.r.a.moveAction().operation == original &&
          !f.r.a.moveAction().relation.originalOperationId);
    CHECK(f.r.wire.sentCount == before);
    f.finish();
    CHECK(f.r.a.moveAction().state == M::ActionState::SUCCEEDED);
    CHECK(!f.replace());
    CHECK(f.r.a.moveAction().operation == original);
}
void fullOrdinaryRetainsUrgent() {
    Fixture f;
    O::RequestId ids[O::CanBusOwner::REQUESTS];
    const auto& original = f.r.a.moveAction();
    for (unsigned i = 0; i < O::CanBusOwner::REQUESTS; ++i) {
        O::BusRequest request;
        request.sequence = f.r.a.sequence();
        request.transfer.channel = f.r.current().channel;
        request.transfer.operation = request.transfer.request = original.operation;
        request.transfer.step = original.step + i + 1;
        request.transfer.index = 0x2100;
        request.transfer.direction = C::SdoDirection::UPLOAD;
        request.transfer.size = 2;
        request.transfer.deadlineUs = original.deadlineUs;
        request.transfer.queueTimeoutUs = QUEUE;
        request.transfer.responseTimeoutUs = RESPONSE;
        CHECK(f.r.owner.admit(request, f.r.now, ids[i]) == O::BusAdmission::ACCEPTED);
        CHECK(f.r.owner.cancel(ids[i], f.r.now) == O::Cancel::CANCELLED);
    }
    const unsigned first = f.r.wire.sentCount;
    CHECK(f.replace());
    f.r.steps(2);
    CHECK(f.r.wire.sentCount == first && !f.r.a.moveAction().transmissionNoted);
    CHECK(f.r.owner.resources().retained == O::CanBusOwner::REQUESTS);
    CHECK(f.r.a.requestStop(L::StopKind::QUICK_STOP, f.r.a.moveObservation(), f.policy(),
                            f.r.now) != E::StopPath::NONE);
    f.finish();
    CHECK(wordsFrom(f.r, first, 0x000b) == 1 && !downloadsFrom(f.r, first, 0x6081));
    for (unsigned i = 0; i < O::CanBusOwner::REQUESTS; ++i)
        CHECK(f.r.owner.release(ids[i]));
    const unsigned after = f.r.wire.sentCount;
    f.r.steps(4);
    CHECK(f.r.wire.sentCount == after && f.r.a.moveAction().uncertain && f.r.a.moveFenced());
}
void unresolvedAndDeadline() {
    {
        Fixture f;
        CHECK(f.replace());
        f.r.drive.silentIndex = 0x6081;
        for (unsigned i = 0; i < 100 && f.r.a.moveActive(); ++i)
            f.r.step();
        CHECK(!f.r.a.moveActive() && f.r.a.moveAction().uncertain && f.r.a.moveFenced());
        const unsigned before = f.r.wire.sentCount;
        CHECK(!f.replace());
        f.r.steps(4);
        CHECK(f.r.wire.sentCount == before);
    }
    {
        Fixture f;
        const uint64_t deadline = f.r.a.moveAction().deadlineUs;
        CHECK(f.replace());
        const unsigned before = f.r.wire.sentCount;
        f.r.turn(deadline);
        CHECK(f.r.wire.sentCount == before &&
              f.r.a.moveAction().outcome == M::ActionOutcome::DEADLINE);
        CHECK(f.r.a.moveAction().uncertain && f.r.a.moveFenced());
    }
}
void velocityChangePreservesOwnHalt() {
    VelocityRig::Rig r;
    r.ready();
    CHECK(r.start());
    r.running();
    const auto& run = r.session.velocityAction();
    const uint64_t due = run.stopDueUs, deadline = run.deadlineUs;
    CHECK(r.session.latestControl());
    const auto predecessor = *r.session.latestControl();
    L::VelocityChangeEvidence evidence;
    evidence.gate = L::VelocityGate::MEASURED;
    evidence.record = 52;
    evidence.model = run.evidence.model;
    evidence.firmware = run.evidence.firmware;
    evidence.rateChangeQualified = true;
    evidence.maximumForwardRpm = 300;
    const auto change = VelocityRig::request(180).velocity;
    CHECK(r.session.requestVelocityChange(change, evidence, r.now));
    for (unsigned i = 0; i < 30 && !run.change.serviceEvidencePresent; ++i)
        r.step();
    CHECK(run.change.execution == M::ActionExecution::ACKNOWLEDGED);
    CHECK(r.session.latestControl() &&
          r.session.latestControl()->completedUs == predecessor.completedUs);
    CHECK(run.stopDueUs == due && run.deadlineUs == deadline);
    r.finish();
    CHECK(run.state == M::ActionState::SUCCEEDED &&
          run.completion == M::ActionCompletion::OBSERVED);
    CHECK(!run.needsStop && run.stopExecution == M::ActionExecution::ACKNOWLEDGED);
    CHECK(r.writes(0x6040, 0x010f) == 1 && r.writes(0x6040, 0x000b) == 0);
    CHECK(r.writes(0x60ff, 180) == 1 && r.writes(0x6040, 15) == 1);
}
void locallyExpiredVelocityChangePreservesStop() {
    for (unsigned variant = 0; variant < 8; ++variant) {
        scenario = "unsent-change-time-refusal-retains-run-stop";
        VelocityRig::Rig r;
        r.ready();
        auto options = VelocityRig::options();
        const bool stale = variant == 2 || variant == 3;
        if (stale)
            options.maximumAgeUs = 7 * (VelocityRig::QUEUE + VelocityRig::RESPONSE);
        CHECK(r.start(VelocityRig::evidence(), VelocityRig::request(), options));
        r.running();
        const auto& run = r.session.velocityAction();
        const uint64_t due = run.stopDueUs, deadline = run.deadlineUs;
        L::VelocityChangeEvidence evidence;
        evidence.gate = L::VelocityGate::MEASURED;
        evidence.record = 52;
        evidence.model = run.evidence.model;
        evidence.firmware = run.evidence.firmware;
        evidence.rateChangeQualified = true;
        evidence.maximumForwardRpm = 300;
        r.now = stale ? run.latest.earliestUs + options.maximumAgeUs - 1
                      : due - 8 * (VelocityRig::QUEUE + VelocityRig::RESPONSE) - 1;
        CHECK(r.session.requestVelocityChange(VelocityRig::request(180).velocity, evidence, r.now));
        // Variant 1 expires before admission; 0 and 2–5 lose their window while queued.
        // Variants 6–7 also exhaust the queue timeout. None changes the original route.
        if (variant == 1)
            r.now += VelocityRig::TICK;
        r.session.service(r.now);
        if (variant == 3)
            ++r.axis.referenceGeneration; // A simultaneous provenance change must still invalidate.
        if (variant == 4 || variant == 5) {
            r.serviceSession = false;
            r.step(); // Owner rejects now, but the session consumes its terminal at/after stop due.
            r.serviceSession = true;
            r.turn(due + (variant == 5 ? VelocityRig::TICK : 0));
        } else if (variant >= 6) {
            // No owner turn occurs within the queue budget, so no preflight callback runs.
            r.turn(variant == 6 ? r.now + VelocityRig::QUEUE + 1 : due + VelocityRig::TICK);
        }
        r.finish();
        CHECK(run.state == M::ActionState::FAILED && run.change.serviceEvidencePresent);
        CHECK(run.change.execution == M::ActionExecution::NOT_TRANSMITTED &&
              !run.change.serviceEvidence.transmissionPossible && !r.writes(0x60ff, 180));
        CHECK(run.stopDueUs == due && run.deadlineUs == deadline);
        CHECK(r.wire.preflightRefusals == (variant == 1 || variant >= 6 ? 0u : 1u));
        if (variant >= 6)
            CHECK(run.change.serviceEvidence.outcome == C::SdoOutcome::TIMEOUT &&
                  run.firstFailure == M::ActionOutcome::SERVICE_TIMEOUT);
        if (variant == 3) {
            CHECK(run.interruption == M::VelocityInterruption::INVALIDATED && run.needsStop &&
                  !run.stopRequested && !r.writes(0x6040, 0x010f));
        } else {
            CHECK(run.stopRequested && run.stopOperation && !run.needsStop);
            CHECK(run.stopExecution == M::ActionExecution::ACKNOWLEDGED &&
                  run.completion == M::ActionCompletion::OBSERVED);
            CHECK(r.writes(0x6040, 0x010f) == 1 && !r.writes(0x6040, 0x000b));
            CHECK(run.change.firstFailure != M::ActionOutcome::NONE);
        }
    }
}
} // namespace
int main() {
    relationsAndStops();
    refusedStopIsSticky();
    malformedRelationsAreRefused();
    lateOriginalCompletionRefusesHandoff();
    fullOrdinaryRetainsUrgent();
    unresolvedAndDeadline();
    velocityChangePreservesOwnHalt();
    locallyExpiredVelocityChangePreservesStop();
    std::printf("axis change: %u checks\n", checks);
}
