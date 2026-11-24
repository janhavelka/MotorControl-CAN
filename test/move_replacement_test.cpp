// SPDX-License-Identifier: MIT
// Every drive report and capability here is manufactured. No fake opens a firmware capability.
#include <cstdio>
#include <cstdlib>
#include <cstring>
static unsigned checks = 0;
static bool asyncTraffic = false;
static const char* scenario = "replacement";
#define CHECK(condition)                                                                           \
    do {                                                                                           \
        ++checks;                                                                                  \
        if (!(condition)) {                                                                        \
            std::fprintf(stderr, "%s:%d: %s [%s]\n", __FILE__, __LINE__, #condition, scenario);    \
            std::abort();                                                                          \
        }                                                                                          \
    } while (false)
#include "support/MoveRig.h"

namespace {
using namespace MoveRig;
struct Fixture {
    Rig rig;
    L::MoveContext original, changed;
    L::ReadContext observation;
    L::StoredPositionQualification stored;
    L::AxisReservation live;
    L::ReadTarget current;
    L::MoveReplacementEvidence evidence;
    L::MoveReplacementRequest request;
    // flagged: the original's record names another firmware (1009), so the original runs firmware unqualified.
    explicit Fixture(bool flagged = false) {
        rig.ready();
        rig.drive.travelReads = 100;
        rig.drive.velocity = 30;
        L::MoveEvidence first = MoveRig::evidence();
        if (flagged)
            ++first.firmware.hardwareVersion;
        CHECK(rig.request(first, moveRequest(TARGET, 30)));
        for (unsigned i = 0; i < 300 && !rig.a.moveAction().latest.present; ++i)
            rig.step();
        CHECK(rig.a.moveAction().phase == L::MovePhase::WAITING &&
              rig.a.moveAction().acceptance.present);
        original = rig.a.moveAction();
        live = rig.reservation();
        current = rig.current();
        CHECK(L::prepareStoredPositionQualification(stored, original, rig.now));
        evidence.gate = L::MoveChangeGate::ATTENDED_EXPERIMENT;
        evidence.record = 52;
        evidence.model = original.evidence.model;
        evidence.firmware = original.evidence.firmware;
        request.request = moveRequest(TARGET, 60);
        request.profile = profile();
        request.fields = 2;
        request.mode = L::MoveReplacementMode::BARE;
    }
    M::Status prepare(const L::MoveFeedbackRelation* feedback = nullptr) {
        const M::Status s = L::prepareMoveReplacement(changed, original, stored, live, current,
                                                      evidence, request, rig.one.coordinates,
                                                      feedback, original.operation + 1, rig.now);
        CHECK(!s || changed.retryClass == M::RetryClass::MOTION_LIKE); // A retrigger is M too.
        return s;
    }
    void handoff() {
        // This is a core consumer on the real owner. Production AxisSession handoff is tested
        // independently; its old observer is disabled here before another owner turn.
        rig.serviceSession = false;
        rig.wire.sessions[0] = nullptr;
        CHECK(L::advanceMove(original, observation, live, current, M::MoveInterruption::REPLACED,
                             rig.now));
        CHECK(rig.owner.beginOperation(rig.a.sequence(), changed.operation, changed.deadlineUs,
                                       rig.now));
    }
    void service() {
        L::PreparedMove work;
        if (rig.now < changed.eligibleUs)
            rig.turn(changed.eligibleUs);
        CHECK(L::nextMove(changed, observation, live, current, rig.now, work));
        if (work.observe) {
            CHECK(L::beginMoveObservation(changed, observation, live, current, rig.now));
            CHECK(L::nextMove(changed, observation, live, current, rig.now, work));
        }
        CHECK(work.ready);
        O::BusRequest request;
        request.sequence = rig.a.sequence();
        request.transfer = work.transfer;
        O::RequestId id;
        CHECK(rig.owner.admit(request, rig.now, id) == O::BusAdmission::ACCEPTED);
        if (work.transfer.direction == C::SdoDirection::DOWNLOAD) {
            CHECK(L::noteMoveTransmission(changed, live, current, rig.one.coordinates, nullptr,
                                          nullptr, 0, rig.now));
            if (changed.phase != L::MovePhase::REARM)
                CHECK(L::validateMoveAdmission(changed, rig.one.coordinates, nullptr, rig.now));
            L::PreparedMove again;
            CHECK(L::nextMove(changed, observation, live, current, rig.now, again) &&
                  !again.ready && !again.observe);
        }
        for (unsigned i = 0; i < 50 && !rig.owner.result(id); ++i)
            rig.step();
        CHECK(rig.owner.result(id));
        const C::SdoCompletion terminal = rig.owner.result(id)->service;
        CHECK(rig.owner.release(id));
        CHECK(L::advanceMove(changed, observation, live, current, terminal, rig.now));
    }
};
void experimentModes() {
    for (unsigned mode = 0; mode < 4; ++mode) {
        Fixture f;
        f.request.mode = static_cast<L::MoveReplacementMode>(mode);
        if (mode == 1) {
            f.request.fields = 0;
            f.request.request = moveRequest(TARGET, 30);
        }
        const Saved<L::MoveContext> old(f.original);
        CHECK(f.prepare());
        old.unchanged(f.original);
        CHECK(f.changed.deadlineUs == f.original.deadlineUs);
        CHECK(f.changed.relation.originalOperationId == f.original.operation);
        CHECK(f.changed.relation.replacementOperationId == f.changed.operation);
        CHECK(f.changed.uncertain && !f.changed.triggerPresent && !f.changed.acceptance.present);
        const unsigned first = f.rig.wire.sentCount;
        f.handoff();
        for (unsigned i = 0; i < 25 && f.changed.observations < 2; ++i)
            f.service();
        CHECK(f.changed.state == M::ActionState::ACTIVE);
        CHECK(f.changed.completion == M::ActionCompletion::NOT_OBSERVED && !f.changed.rearmPresent);
        CHECK(!f.changed.acceptance.present && !f.changed.completed.present);
        CHECK(!downloadsFrom(f.rig, first, 0x6060));
        CHECK(wordsFrom(f.rig, first, 6) == 0 && wordsFrom(f.rig, first, 7) == 0);
        CHECK(wordsFrom(f.rig, first, 0x000f) == (mode ? 1u : 0u));
        CHECK(wordsFrom(f.rig, first, 0x003f) == (mode == 2 ? 1u : 0u));
        CHECK(wordsFrom(f.rig, first, 0x001f) == (mode == 3 ? 1u : 0u));
        CHECK(f.changed.clearPresent == (mode != 0));
        CHECK(L::advanceMove(f.changed, f.observation, f.live, f.current, M::MoveInterruption::STOP,
                             f.rig.now));
        CHECK(f.changed.uncertain && f.changed.interruption == M::MoveInterruption::STOP);
    }
}
void refusals() {
    for (unsigned variant = 0; variant < 13; ++variant) {
        Fixture f;
        const Saved<L::MoveContext> unchanged(f.changed);
        switch (variant) {
        case 0:
            f.evidence.gate = L::MoveChangeGate::CLOSED;
            break;
        case 1:
            f.original.withdrawal = M::MoveInterruption::STOP;
            break;
        case 2:
            f.original.setupHistory[1].effectUncertain = true;
            break;
        case 3:
            ++f.stored.values[1];
            break;
        case 4:
            f.request.request.position.relative = true;
            break;
        case 5:
            f.request.request.speed.value = M::Rational(0);
            break;
        case 6:
            f.request.request.speed.value = M::Rational(-30);
            break;
        case 7:
            f.request.profile.rampUpTimeMs = 0;
            break;
        case 8:
            f.request.request = moveRequest(TARGET + 100, 60);
            f.request.fields = 3;
            break;
        case 9:
            f.evidence.immediateQualified = true;
            break;
        case 10:
            f.rig.now = f.original.deadlineUs;
            break;
        case 11:
            f.request.fields = 4;
            break; // Unselected speed changed.
        default:
            f.request.request.speed.value = M::Rational(241, 4);
            f.request.request.speed.rounding = M::Rounding::FLOOR;
            f.request.request.speed.maximumQuantizationError = M::Rational(1);
            break; // Quantized 60 rpm does not admit a request above the experiment cap.
        }
        CHECK(!f.prepare());
        unchanged.unchanged(f.changed);
    }
}
void changedTarget() {
    Fixture f;
    f.request.request = moveRequest(TARGET + 100, 60);
    f.request.fields = 3;
    L::MoveFeedbackRelation relation;
    relation.record = 53;
    relation.binding = f.rig.one.coordinates.binding;
    relation.nodeGeneration = f.current.nodeGeneration;
    relation.referenceGeneration = f.rig.one.coordinates.referenceGeneration;
    relation.sameIncrement = true;
    CHECK(f.prepare(&relation));
    CHECK(f.changed.targetChanged && f.changed.feedbackRelation.record == 53);
    ++relation.referenceGeneration;
    CHECK(!f.prepare(&relation));
    --relation.referenceGeneration;
    ++relation.nodeGeneration;
    CHECK(!f.prepare(&relation));
    --relation.nodeGeneration;
    f.request.request = moveRequest(TARGET - 100, 60);
    CHECK(!f.prepare(&relation));
    f.request.request = moveRequest(TARGET + 100, 60);
    relation.commandMinusFeedback = INT64_MAX;
    CHECK(!f.prepare(&relation));
}
void failedWriteAndNoReplay() {
    Fixture f;
    f.request.fields = 6;
    f.request.profile.rampUpTimeMs = 150;
    CHECK(f.prepare());
    f.handoff();
    f.service();
    CHECK(f.changed.setupEvidenceMask == 2 && f.changed.setup == M::ActionExecution::UNKNOWN);
    f.rig.drive.silentIndex = 0x6083;
    f.service();
    CHECK(f.changed.state == M::ActionState::FAILED && f.changed.uncertain);
    CHECK(f.changed.setupEvidenceMask == 6 &&
          f.changed.setupHistory[1].outcome == C::SdoOutcome::SUCCESS);
    CHECK(f.changed.setupHistory[2].effectUncertain);
    const unsigned before = f.rig.wire.sentCount;
    L::PreparedMove work;
    CHECK(L::nextMove(f.changed, f.observation, f.live, f.current, f.rig.now, work) && !work.ready);
    CHECK(before == f.rig.wire.sentCount);
}
void measuredBuffered() {
    Fixture f;
    f.request.mode = L::MoveReplacementMode::BUFFERED;
    f.evidence.gate = L::MoveChangeGate::MEASURED;
    f.evidence.parameterMask = 2;
    f.evidence.fallingEdgeQualified = f.evidence.bufferedQualified = true;
    f.evidence.completionQualified = f.evidence.stopQualified = true;
    CHECK(f.prepare());
    f.handoff();
    for (unsigned i = 0; i < 50 && !f.changed.triggerPresent; ++i)
        f.service();
    CHECK(f.changed.cleared.present && f.changed.triggerPresent && !f.changed.acceptance.present);
    CHECK(f.changed.cleared.earliestUs >= f.changed.clearEvidence.completedUs);
    f.rig.drive.remaining = 1;
    for (unsigned i = 0; i < 40 && f.changed.state == M::ActionState::ACTIVE; ++i)
        f.service();
    CHECK(f.changed.state == M::ActionState::SUCCEEDED &&
          f.changed.completion == M::ActionCompletion::OBSERVED);
    CHECK(f.changed.acceptance.earliestUs >= f.changed.trigger.completedUs &&
          f.changed.rearmPresent);
}
// Fix round c, item 6: a replacement of a move that runs firmware unqualified stays flagged, even when its own
// record names the drive's firmware; the original's own flag is unchanged.
void replacementKeepsTheFirmwareFlag() {
    Fixture f(true);
    CHECK(f.original.identity.firmwareUnqualified && f.original.identity.expectedMatch);
    const L::IdentityValues& observed = f.original.identity.observed;
    f.evidence.firmware.hardwareVersion = observed.hardwareVersion; // The replacement record is the drive's own.
    f.evidence.firmware.softwareVersion = observed.softwareVersion;
    f.evidence.firmware.revision = observed.revision;
    f.evidence.firmware.firmwareCode = observed.firmwareCode;
    CHECK(f.prepare());
    CHECK(f.changed.identity.outcome == L::IdentityOutcome::CONFIRMED && f.changed.identity.expectedMatch &&
          f.changed.identity.firmwareUnqualified);
    // Without the original's flag the same record is qualified.
    Fixture g;
    CHECK(!g.original.identity.firmwareUnqualified);
    CHECK(g.prepare() && !g.changed.identity.firmwareUnqualified);
}
void qualificationAndValidationAreAtomic() {
    Fixture f;
    L::StoredPositionQualification qualification;
    CHECK(L::prepareStoredPositionQualification(qualification, f.original, f.rig.now));
    CHECK(qualification.originalOperationId == f.original.operation);
    const Saved<L::StoredPositionQualification> retained(qualification);
    f.original.setupHistory[1].effectUncertain = true;
    CHECK(!L::prepareStoredPositionQualification(qualification, f.original, f.rig.now));
    retained.unchanged(qualification);
    f.original.setupHistory[1].effectUncertain = false;
    const Saved<L::MoveContext> output(f.changed);
    CHECK(L::validateMoveReplacement(f.changed, f.original, qualification, f.live, f.current,
                                     f.evidence, f.request, f.rig.one.coordinates, nullptr,
                                     f.original.operation + 1, f.rig.now));
    output.unchanged(f.changed);
    ++f.current.nodeGeneration;
    CHECK(!L::validateMoveReplacement(f.changed, f.original, qualification, f.live, f.current,
                                      f.evidence, f.request, f.rig.one.coordinates, nullptr,
                                      f.original.operation + 1, f.rig.now));
    output.unchanged(f.changed);
}
} // namespace
int main() {
    qualificationAndValidationAreAtomic();
    experimentModes();
    refusals();
    changedTarget();
    failedWriteAndNoReplay();
    measuredBuffered();
    replacementKeepsTheFirmwareFlag();
    std::printf("move replacement: %u checks\n", checks);
}
