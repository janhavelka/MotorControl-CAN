// SPDX-License-Identifier: MIT
// prepareMoveRelative through the real owner, the selected SDO engine and AxisSession, on the shared
// fake of support/MoveRig.h, and the combined interruption matrix of both moves. No relative base is
// measured on any specimen: every MoveEvidence::relativeBase here is this test's switch, opened only to
// exercise the lifecycle, and the fake's base is a switch too. Nothing here shows what a drive adds a
// displacement to, or that it accepts, moves or completes one. NMT stopped is never used as a drive
// state, and no free SDO channel is fabricated. Every scenario runs twice, the second time with
// heartbeat traffic delivered on every turn.
#include <cstdio>
#include <cstdlib>
#include <cstring>

static unsigned checks = 0;
static bool asyncTraffic = false;
static const char* scenario = "";
#define CHECK(condition)                                                                           \
    do {                                                                                           \
        ++checks;                                                                                  \
        if (!(condition)) {                                                                        \
            std::fprintf(stderr, "%s:%d: %s [%s, async %d]\n", __FILE__, __LINE__, #condition,     \
                         scenario, int(asyncTraffic));                                             \
            std::abort();                                                                          \
        }                                                                                          \
    } while (false)
#include "support/MoveRig.h"
namespace {
using namespace MoveRig;

L::MoveEvidence relEvidence(L::RelativeBase base = L::RelativeBase::ACTUAL,
                            L::CompletionRule rule = L::CompletionRule::AFTER_ABSENCE) {
    L::MoveEvidence value = evidence(rule);
    value.relativeBase = base;
    return value;
}
M::MoveRequest relRequest(int64_t displacement, M::RelativeBasis basis = M::RelativeBasis::ACTUAL) {
    M::MoveRequest value = moveRequest(displacement);
    value.position.relative = true;
    value.position.basis = basis;
    return value;
}
E::StopPolicy stopPolicy(const Rig& r) {
    E::StopPolicy policy;
    policy.options.service = reads();
    policy.options.observationIntervalUs = 1000;
    policy.options.maximumAgeUs = uint64_t(QUEUE + uint64_t(RESPONSE)) * 9;
    policy.deadlineUs = r.now + 200000;
    policy.bestEffortDeadlineUs = r.now + 300000;
    return policy;
}
const int32_t PARAMETER = int32_t(M::MoveError::PARAMETER_ERROR_BASE);
const int32_t AXIS = PARAMETER + int32_t(L::PositionParameterError::AXIS_ERROR_BASE);
// No absolute, immediate, mode, counter or alarm word or object ever comes from a relative move.
void onlyRelativeWords(const Rig& r, unsigned from, unsigned node = 1) {
    CHECK(!wordsFrom(r, from, 0x001f, node) && !wordsFrom(r, from, 0x003f, node) &&
          !wordsFrom(r, from, 0x007f, node));
    CHECK(!downloadsFrom(r, from, 0x6060, node) && !downloadsFrom(r, from, 0x6061, node) &&
          !downloadsFrom(r, from, 0x2301, node) && !downloadsFrom(r, from, 0x2302, node));
}
void finishAll(Rig& r) {
    r.finishMove();
    for (unsigned k = 0; k < 600 && (r.one.a.busy() || r.two.a.busy()); ++k)
        r.step();
    CHECK(!r.one.a.busy() && !r.two.a.busy());
}

// 1. The whole relative sequence, literal bytes in literal order, and its retained snapshot.
void golden() {
    scenario = "relative golden";
    Rig r;
    r.ready();
    r.drive.position = 1000; // The fake's actual position, which the witness reports.
    const M::AxisReference w = witness(r, 1000);
    CHECK(r.relative(w, relEvidence(), relRequest(2500)).isOk());
    CHECK(r.wire.sentCount == r.startFrame && r.a.moveActive());
    r.finishMove();
    const L::MoveContext& c = moveOf(r);
    CHECK(c.state == M::ActionState::SUCCEEDED && c.outcome == M::ActionOutcome::OBSERVED);
    CHECK(c.setup == M::ActionExecution::ACKNOWLEDGED && c.execution == M::ActionExecution::ACKNOWLEDGED);
    CHECK(c.rearm == M::ActionExecution::ACKNOWLEDGED && !c.uncertain);
    unsigned i = r.startFrame;
    const uint8_t displacement[8] = {0x23, 0x7a, 0x60, 0x00, 0xc4, 0x09, 0x00, 0x00}; // 2500, not 3500.
    CHECK(frameIs(r, i++, displacement));
    CHECK(downloadAt(r, i++, 0x6081, 4, 120) && downloadAt(r, i++, 0x6083, 4, 100));
    CHECK(downloadAt(r, i++, 0x6084, 4, 200));
    const uint8_t trigger[8] = {0x2b, 0x40, 0x60, 0x00, 0x5f, 0x00, 0x00, 0x00};
    CHECK(frameIs(r, i++, trigger));
    CHECK(cycleAt(r, i) && cycleAt(r, i + 7));
    i += 14;
    CHECK(wordAt(r, i++, 0x000f) && i == r.wire.sentCount);
    onlyRelativeWords(r, r.startFrame);
    CHECK(wordsFrom(r, r.startFrame, 0x005f) == 1 && r.drive.edges == 1);
    // Captured: displacement, quantization, host endpoint and the exact witness snapshot.
    CHECK(c.displacement == M::MoveDisplacement::KNOWN && c.displacementNative == 2500);
    const M::PreparedTarget& t = c.parameters.target;
    CHECK(c.parameters.targetNative == 2500 && t.effectiveNative == 2500 && t.endpointNative == 3500);
    CHECK(t.requested.relative && t.requested.basis == M::RelativeBasis::ACTUAL);
    CHECK(!t.quantization.errorNumerator && t.exactArithmetic && t.referenceUsed);
    CHECK(t.retainedReference.nativePosition == 1000 && t.retainedReference.observedUs == w.observedUs);
    CHECK(t.retainedReference.maximumAgeUs == w.maximumAgeUs && t.referenceGeneration == w.referenceGeneration);
    // The host arithmetic reference and the drive's native base are kept apart.
    CHECK(t.retainedReference.basis == M::RelativeBasis::ACTUAL && c.evidence.relativeBase == L::RelativeBase::ACTUAL);
    CHECK(c.trigger.transfer.data[0] == 0x5f && c.completed.position == 3500); // Raw fake report.
    CHECK(r.a.latestControl() && r.a.latestControl()->transfer.data[0] == 0x0f && !r.a.moveFenced());
    quiet(r);
}

// 2. Every capability, form, policy and witness refusal, before any write, then the same rig moves.
void gates() {
    scenario = "relative gates";
    Rig r;
    r.ready();
    r.one.coordinates.supportedRelativeBases = 7; // Host arithmetic may use every basis here.
    const M::AxisReference actual = witness(r, 0);
    refused(r, r.relative(actual, L::MoveEvidence(), relRequest(100)), M::Err::UNSUPPORTED,
            M::MoveError::CAPABILITY);
    refused(r, r.relative(actual, evidence(), relRequest(100)), M::Err::UNSUPPORTED,
            M::MoveError::RELATIVE_BASIS);
    // A queued base is refused whatever the evidence says, and so is a base the record did not measure.
    const M::AxisReference queued = witness(r, 0, M::RelativeBasis::QUEUED);
    for (unsigned base = 0; base < 3; ++base) {
        const M::Status s = r.relative(queued, relEvidence(L::RelativeBase(base)),
                                       relRequest(100, M::RelativeBasis::QUEUED));
        CHECK(s.msg && std::strstr(s.msg, "queue")); // The precise gate: a target queue, not a measurement.
        refused(r, s, M::Err::UNSUPPORTED, M::MoveError::RELATIVE_BASIS);
    }
    const M::AxisReference commanded = witness(r, 0, M::RelativeBasis::COMMANDED);
    refused(r, r.relative(commanded, relEvidence(L::RelativeBase::ACTUAL),
                          relRequest(100, M::RelativeBasis::COMMANDED)),
            M::Err::UNSUPPORTED, M::MoveError::RELATIVE_BASIS);
    refused(r, r.relative(actual, relEvidence(L::RelativeBase::COMMANDED), relRequest(100)),
            M::Err::UNSUPPORTED, M::MoveError::RELATIVE_BASIS);
    refused(r, r.relative(actual, relEvidence(L::RelativeBase(3)), relRequest(100)), M::Err::INVALID_CONFIG,
            M::MoveError::ARGUMENT);
    L::MoveProfile immediate = profile();
    immediate.immediateUpdate = true; // 007F.
    refused(r, r.relative(actual, relEvidence(), relRequest(100), 1, immediate), M::Err::UNSUPPORTED,
            M::MoveError::CAPABILITY);
    // Each preparation takes only its own form: nothing is converted.
    refused(r, r.relative(actual, relEvidence(), moveRequest(100)), M::Err::INVALID_CONFIG, M::MoveError::ARGUMENT);
    refused(r, r.request(relEvidence(), relRequest(100), profile(), &actual), M::Err::INVALID_CONFIG,
            M::MoveError::ARGUMENT);
    M::MoveRequest wrapped = relRequest(100);
    wrapped.position.wrapped = true;
    refused(r, r.relative(actual, relEvidence(), wrapped), M::Err::INVALID_CONFIG, M::MoveError::ARGUMENT);
    r.readState(r.state); // Refusals let time pass; keep the admission read fresh.
    // Host policy without the basis: the axis's own refusal, with its detail kept beneath the bases.
    r.one.coordinates.supportedRelativeBases = 2;
    M::Status s = r.relative(actual, relEvidence(), relRequest(100));
    CHECK(s.code == M::Err::UNSUPPORTED && s.detail == AXIS + int32_t(M::AxisError::UNSUPPORTED_BASIS));
    refused(r, s, s.code, M::MoveError(s.detail));
    r.one.coordinates.supportedRelativeBases = 7;
    // Witness rules.
    M::AxisReference w = witness(r, 0);
    w.idle = false;
    refused(r, r.relative(w, relEvidence(), relRequest(100)), M::Err::INVALID_CONFIG, M::MoveError::REFERENCE);
    w = witness(r, 0);
    w.stationary = false;
    refused(r, r.relative(w, relEvidence(), relRequest(100)), M::Err::INVALID_CONFIG, M::MoveError::REFERENCE);
    w = witness(r, 0, M::RelativeBasis::COMMANDED);
    refused(r, r.relative(w, relEvidence(), relRequest(100)), M::Err::INVALID_CONFIG, M::MoveError::REFERENCE);
    w = witness(r, 0);
    w.maximumAgeUs = 0; // No expiry is not freshness.
    refused(r, r.relative(w, relEvidence(), relRequest(100)), M::Err::INVALID_CONFIG, M::MoveError::REFERENCE);
    w = witness(r, 0);
    w.nativeKnown = false; // Unknown reference.
    s = r.relative(w, relEvidence(), relRequest(100));
    CHECK(s.detail == AXIS + int32_t(M::AxisError::MISSING_REFERENCE));
    refused(r, s, s.code, M::MoveError(s.detail));
    w = witness(r, 0);
    w.observedUs = 0;
    w.maximumAgeUs = 1;
    s = r.relative(w, relEvidence(), relRequest(100));
    CHECK(s.detail == AXIS + int32_t(M::AxisError::STALE_REFERENCE));
    refused(r, s, s.code, M::MoveError(s.detail));
    w = witness(r, 0);
    ++w.referenceGeneration;
    s = r.relative(w, relEvidence(), relRequest(100));
    CHECK(s.detail == AXIS + int32_t(M::AxisError::STALE_GENERATION));
    refused(r, s, s.code, M::MoveError(s.detail));
    M::MoveRequest old = relRequest(100);
    --old.position.configurationGeneration;
    s = r.relative(actual, relEvidence(), old);
    CHECK(s.detail == AXIS + int32_t(M::AxisError::STALE_GENERATION));
    refused(r, s, s.code, M::MoveError(s.detail));
    // A witness observed before the latest 000F describes an earlier drive.
    const M::AxisReference early = witness(r, 0);
    r.step();
    r.writeThrough(0x6040, 2, 0x000f);
    r.readState(r.state);
    refused(r, r.relative(early, relEvidence(), relRequest(100)), M::Err::INVALID_CONFIG, M::MoveError::REFERENCE);
    // Everything refused, then the same rig still moves: nothing above changed anything.
    r.readState(r.state);
    CHECK(r.relative(witness(r, 0), relEvidence(), relRequest(100)).isOk());
    r.finishMove();
    CHECK(moveOf(r).state == M::ActionState::SUCCEEDED && r.drive.edges == 1);
}

// 3. Native displacement boundaries, endpoint axis limits and exact quantization.
void boundaries() {
    scenario = "relative boundaries";
    const int32_t edges[] = {1000000, -1000000};
    for (int32_t value : edges) {
        Rig r;
        r.ready();
        CHECK(r.relative(witness(r, 0), relEvidence(), relRequest(value)).isOk());
        r.finishMove();
        CHECK(moveOf(r).state == M::ActionState::SUCCEEDED);
        CHECK(downloadAt(r, r.startFrame, 0x607a, 4, uint32_t(value)) && r.drive.position == value);
    }
    Rig r;
    r.ready();
    const int64_t outside[] = {1000001, -1000001, INT64_MAX, INT64_MIN};
    for (int64_t value : outside) {
        const M::Status s = r.relative(witness(r, 0), relEvidence(), relRequest(value));
        CHECK(!s && s.detail >= PARAMETER);
        refused(r, s, s.code, M::MoveError(s.detail));
    }
    // 1000000.5 and -1000000.5 round back to the edge under ties-to-even, but the requested
    // displacement is outside.
    M::Status s;
    const int64_t halves[] = {2000001, -2000001};
    for (int64_t numerator : halves) {
        M::MoveRequest half = relRequest(0);
        half.position.value = M::Rational(numerator, 2);
        half.position.rounding = M::Rounding::NEAREST;
        half.position.maximumQuantizationError = M::Rational(1, 2);
        s = r.relative(witness(r, 0), relEvidence(), half);
        CHECK(s.detail == PARAMETER + int32_t(L::PositionParameterError::RANGE));
        refused(r, s, M::Err::ILLEGAL_VALUE, M::MoveError(s.detail));
    }
    // The 607A range bounds the displacement, not the endpoint: 900000 + 200000 is admitted while the
    // axis limits allow 1100000, and refused once its own limits do not.
    r.readState(r.state);
    r.one.coordinates.nativeMinimum = -1500000;
    r.one.coordinates.nativeMaximum = 1500000;
    s = r.relative(witness(r, 1400000), relEvidence(), relRequest(200000));
    CHECK(s.detail == AXIS + int32_t(M::AxisError::LIMIT));
    refused(r, s, s.code, M::MoveError(s.detail));
    s = r.relative(witness(r, -1400000), relEvidence(), relRequest(-200000));
    CHECK(s.detail == AXIS + int32_t(M::AxisError::LIMIT));
    refused(r, s, s.code, M::MoveError(s.detail));
    r.one.coordinates.softLimitsKnown = true;
    r.one.coordinates.softMinimum = -1000000;
    r.one.coordinates.softMaximum = 1000000;
    s = r.relative(witness(r, 900000), relEvidence(), relRequest(200000));
    CHECK(s.detail == AXIS + int32_t(M::AxisError::LIMIT));
    refused(r, s, s.code, M::MoveError(s.detail));
    r.one.coordinates.softLimitsKnown = false;
    r.readState(r.state);
    r.drive.position = 900000;
    CHECK(r.relative(witness(r, 900000), relEvidence(), relRequest(200000)).isOk());
    CHECK(moveOf(r).parameters.targetNative == 200000 && moveOf(r).parameters.target.endpointNative == 1100000);
    r.finishMove();
    CHECK(moveOf(r).state == M::ActionState::SUCCEEDED && downloadAt(r, r.startFrame, 0x607a, 4, 200000));
    // A third of a motor turn each way at 3200 steps per turn: 1066 2/3 rounds to 1067, error +1/3,
    // quantized once on the displacement, before the witness is added.
    for (int sign = 1; sign >= -1; sign -= 2) {
        Rig m;
        m.ready();
        M::MoveRequest third = relRequest(0);
        third.position.value = M::Rational(sign, 3);
        third.position.unit = M::PositionUnit::TURNS;
        third.position.frame = M::CoordinateFrame::MOTOR;
        third.position.rounding = M::Rounding::NEAREST;
        third.position.maximumQuantizationError = M::Rational(1, 2);
        CHECK(m.relative(witness(m, 50), relEvidence(), third).isOk());
        const M::PreparedTarget& t = moveOf(m).parameters.target;
        CHECK(t.effectiveNative == sign * 1067 && t.endpointNative == 50 + sign * 1067);
        CHECK(t.quantization.errorNumerator == 1 && t.quantization.errorDenominator == 3 &&
              t.quantization.errorNegative == (sign < 0));
        m.finishMove();
        CHECK(downloadAt(m, m.startFrame, 0x607a, 4, uint32_t(sign * 1067)));
    }
    // A fifth of a load turn at gear 5 is exactly one motor turn: 3200, no error.
    Rig l;
    l.ready();
    M::MoveRequest load = relRequest(0);
    load.position.value = M::Rational(1, 5);
    load.position.unit = M::PositionUnit::TURNS;
    load.position.frame = M::CoordinateFrame::LOAD;
    CHECK(l.relative(witness(l, 0), relEvidence(), load).isOk());
    CHECK(moveOf(l).parameters.targetNative == 3200 && !moveOf(l).parameters.target.quantization.errorNumerator);
    l.finishMove();
}

// 4. R-08 under each basis: zero effective displacement is refused before any frame. A basis the
// drive's base is not evidenced for stops earlier, at its own gate.
void zeroDisplacement() {
    scenario = "relative zero";
    Rig r;
    r.ready();
    r.one.coordinates.supportedRelativeBases = 7;
    refused(r, r.relative(witness(r, 10), relEvidence(L::RelativeBase::ACTUAL), relRequest(0)),
            M::Err::ILLEGAL_VALUE, M::MoveError::ZERO_DISPLACEMENT);
    refused(r, r.relative(witness(r, 10, M::RelativeBasis::COMMANDED), relEvidence(L::RelativeBase::COMMANDED),
                          relRequest(0, M::RelativeBasis::COMMANDED)),
            M::Err::ILLEGAL_VALUE, M::MoveError::ZERO_DISPLACEMENT);
    refused(r, r.relative(witness(r, 10, M::RelativeBasis::QUEUED), relEvidence(L::RelativeBase::ACTUAL),
                          relRequest(0, M::RelativeBasis::QUEUED)),
            M::Err::UNSUPPORTED, M::MoveError::RELATIVE_BASIS);
    refused(r, r.relative(witness(r, 10), evidence(), relRequest(0)), M::Err::UNSUPPORTED,
            M::MoveError::RELATIVE_BASIS);
    // A third of a step quantizes to zero: also zero, also before any frame.
    M::MoveRequest tiny = relRequest(0);
    tiny.position.value = M::Rational(1, 3);
    tiny.position.rounding = M::Rounding::NEAREST;
    tiny.position.maximumQuantizationError = M::Rational(1, 2);
    refused(r, r.relative(witness(r, 10), relEvidence(), tiny), M::Err::ILLEGAL_VALUE,
            M::MoveError::ZERO_DISPLACEMENT);
    CHECK(!r.drive.edges && r.drive.target == 5000);
}

// 5. A measured COMMANDED base: the host witness and the drive's base are both commanded, and both
// differ from the fake's actual position.
void commandedBase() {
    scenario = "commanded base";
    Rig r;
    r.ready();
    r.one.coordinates.supportedRelativeBases = 2;
    r.drive.commandedBase = true;
    r.drive.commanded = 4000;
    r.drive.position = 3990; // The fake lags its own command; neither is measured.
    CHECK(r.relative(witness(r, 4000, M::RelativeBasis::COMMANDED), relEvidence(L::RelativeBase::COMMANDED),
                     relRequest(1000, M::RelativeBasis::COMMANDED))
              .isOk());
    r.finishMove();
    const L::MoveContext& c = moveOf(r);
    CHECK(c.state == M::ActionState::SUCCEEDED && c.parameters.target.endpointNative == 5000);
    CHECK(c.parameters.target.retainedReference.basis == M::RelativeBasis::COMMANDED);
    CHECK(c.evidence.relativeBase == L::RelativeBase::COMMANDED && r.drive.endpoint == 5000);
    CHECK(downloadAt(r, r.startFrame, 0x607a, 4, 1000) && wordsFrom(r, r.startFrame, 0x005f) == 1);
}

// 6. A completion never establishes the requested endpoint: not when the drive's base differed from
// the witness, and not after the reference changed during the move.
void completionWithDifferentReference() {
    scenario = "different reference";
    {
        Rig r;
        r.ready();
        r.drive.position = 1200; // The witness below says 1000.
        CHECK(r.relative(witness(r, 1000), relEvidence(), relRequest(500)).isOk());
        const Saved<M::AxisReference> borrowed(r.one.witness);
        r.finishMove();
        const L::MoveContext& c = moveOf(r);
        CHECK(c.state == M::ActionState::SUCCEEDED && c.completion == M::ActionCompletion::OBSERVED);
        CHECK(c.parameters.target.endpointNative == 1500 && c.completed.position == 1700);
        borrowed.unchanged(r.one.witness); // Nothing writes the completion back into a witness.
        r.readState(r.state);
        // The same witness predates the rearm 000F, so it cannot start the next move.
        refused(r, r.relative(r.one.witness, relEvidence(), relRequest(500)), M::Err::INVALID_CONFIG,
                M::MoveError::REFERENCE);
    }
    Rig r;
    r.ready();
    r.drive.travelReads = 3;
    CHECK(r.relative(witness(r, 0), relEvidence(), relRequest(800)).isOk());
    for (unsigned k = 0; k < 400 && moveOf(r).execution != M::ActionExecution::ACKNOWLEDGED; ++k)
        r.step();
    // After the trigger the application revokes position confidence. The motion continues on the
    // drive, so the move keeps observing; it cannot be resumed or replayed, only reported.
    M::AxisReference dropped = r.one.witness;
    CHECK(M::invalidateAxisReference(r.one.coordinates, dropped, M::ReferenceLoss::POSITION));
    r.finishMove();
    const L::MoveContext& c = moveOf(r);
    CHECK(c.state == M::ActionState::SUCCEEDED && c.completion == M::ActionCompletion::OBSERVED);
    CHECK(!M::validatePreparedTarget(c.parameters.target, r.one.coordinates, &r.one.witness, r.now));
    // Revalidation is explicit: a new witness in the new epoch admits a new move, never the old one.
    r.readState(r.state);
    M::AxisReference fresh = witness(r, 800);
    fresh.referenceGeneration = r.one.coordinates.referenceGeneration;
    const M::Status stale = r.relative(r.one.witness, relEvidence(), relRequest(800));
    CHECK(stale.detail == AXIS + int32_t(M::AxisError::STALE_GENERATION));
    refused(r, stale, stale.code, M::MoveError(stale.detail));
    CHECK(r.relative(fresh, relEvidence(), relRequest(800)).isOk());
    r.finishMove();
    CHECK(moveOf(r).state == M::ActionState::SUCCEEDED && wordsFrom(r, 0, 0x005f) == 2);
}

// 7. Actual admission rechecks what preparation checked: at each parameter and trigger notice, and,
// with the transport's preflight wired, again when the owner hands the frame over. The refusal's own
// reason stays readable.
enum class Change { WITNESS_AGES, WITNESS_REPLACED, WITNESS_MOVING, POSITION_LOSS, COORDINATES_LOSS, STATE_AGES };
int32_t refusalDetail(Change change) {
    switch (change) {
    case Change::WITNESS_AGES:
    case Change::WITNESS_REPLACED: return AXIS + int32_t(M::AxisError::STALE_REFERENCE);
    case Change::WITNESS_MOVING: return int32_t(M::MoveError::REFERENCE);
    case Change::STATE_AGES: return int32_t(M::MoveError::STATE);
    default: return AXIS + int32_t(M::AxisError::STALE_GENERATION);
    }
}
void admissionRechecks() {
    const Change changes[] = {Change::WITNESS_AGES, Change::WITNESS_REPLACED, Change::WITNESS_MOVING,
                              Change::POSITION_LOSS, Change::COORDINATES_LOSS, Change::STATE_AGES};
    const unsigned count = sizeof changes / sizeof changes[0];
    for (unsigned run = 0; run < 2 * count; ++run) {
        const bool wired = run >= count; // Each change without the transport hook, then with it.
        const Change change = changes[run % count];
        scenario = wired ? "admission rechecks, preflight" : "admission rechecks, no preflight";
        Rig r;
        r.wire.preflight = wired;
        r.ready();
        M::AxisReference w = witness(r, 0);
        if (change == Change::WITNESS_AGES)
            w.maximumAgeUs = 3000; // Fresh at preparation, aged by the trigger.
        CHECK(r.relative(w, relEvidence(), relRequest(600)).isOk());
        // Run until 607A and 6081 are acknowledged and 6083 is noted.
        for (unsigned k = 0; k < 400 && !(moveOf(r).setupCursor == 2 && moveOf(r).transmissionNoted); ++k)
            r.step();
        CHECK(moveOf(r).setupCursor == 2 && r.a.moveActive());
        if (change == Change::WITNESS_REPLACED)
            r.one.witness.observedUs = r.now; // A new observation, so not the move's witness.
        else if (change == Change::WITNESS_MOVING)
            r.one.witness.stationary = false;
        else if (change == Change::POSITION_LOSS || change == Change::COORDINATES_LOSS) {
            M::AxisReference scratch = r.one.witness;
            CHECK(M::invalidateAxisReference(r.one.coordinates, scratch,
                                             change == Change::POSITION_LOSS ? M::ReferenceLoss::POSITION
                                                                             : M::ReferenceLoss::COORDINATES));
        } else if (change == Change::STATE_AGES) {
            r.serviceSession = false;
            r.steps(80); // 40 ms against a 36 ms age cap.
            r.serviceSession = true;
        }
        const unsigned refusals = r.wire.preflightRefusals;
        r.finishMove();
        const L::MoveContext& c = moveOf(r);
        CHECK(c.state == M::ActionState::FAILED && c.outcome == M::ActionOutcome::INVALIDATED);
        CHECK(!wordsFrom(r, r.startFrame, 0x005f) && !r.drive.edges);
        CHECK(c.execution == M::ActionExecution::NOT_TRANSMITTED);
        CHECK(r.a.moveRefusal().detail == refusalDetail(change));
        // A change made before 6083 left stops 6083 itself at the handover. The witness ages and the
        // admission read expires only later, so their 6083 leaves either way.
        const bool atHandover = wired && change != Change::WITNESS_AGES && change != Change::STATE_AGES;
        CHECK(r.wire.preflightRefusals == refusals + (atHandover ? 1u : 0u));
        const unsigned last = atHandover ? 2u : 3u; // The download that never left.
        for (unsigned k = 0; k < last; ++k)
            CHECK(c.setupHistory[k].outcome == C::SdoOutcome::SUCCESS);
        // The refused download was closed in the owner unsent, and settled as evidence; nothing after
        // it was offered.
        CHECK(c.setupEvidenceMask == (atHandover ? 0x7 : 0xf) && !c.setupHistory[last].transmissionPossible);
        CHECK(c.setup == M::ActionExecution::UNKNOWN);
        CHECK(downloadsFrom(r, r.startFrame, 0x6083) == (atHandover ? 0u : 1u));
        CHECK(!downloadsFrom(r, r.startFrame, 0x6084));
        CHECK(!r.a.moveFenced() && !c.uncertain);
    }
    // The absolute contrast: POSITION loss leaves a target with no witness valid, so it completes.
    Rig a;
    a.ready();
    CHECK(a.request().isOk());
    for (unsigned k = 0; k < 400 && moveOf(a).setupCursor != 2; ++k)
        a.step();
    M::AxisReference scratch;
    CHECK(M::invalidateAxisReference(a.one.coordinates, scratch, M::ReferenceLoss::POSITION));
    a.finishMove();
    CHECK(moveOf(a).state == M::ActionState::SUCCEEDED);
}

// 8. The combined interruption matrix.
enum class Point { MODE, PARAM0, PARAM1, PARAM2, PARAM3, PRE_TRIGGER, POSSIBLE_TRIGGER, PROTOCOL, ACCEPTANCE,
                   COMPLETION, REARM };
enum class Cut { DEADLINE, CANCEL, FAULT, BOOT, REBIND, MISSING, STALE, STOP };
const char* const POINTS[] = {"mode", "607A", "6081", "6083", "6084", "pre-trigger", "possible trigger",
                              "protocol confirmation", "acceptance", "completion", "rearm"};
const char* const CUTS[] = {"deadline", "cancel", "fault", "boot", "rebind", "missing", "stale", "stop"};
const uint16_t SETUP[4] = {0x607a, 0x6081, 0x6083, 0x6084};
bool reached(const Rig& r, Point p) {
    const L::MoveContext& c = moveOf(r);
    switch (p) {
    case Point::MODE:
        return c.phase == L::MovePhase::PARAMETERS && !c.setupEvidenceMask && !c.transmissionNoted;
    case Point::POSSIBLE_TRIGGER:
    case Point::PROTOCOL:
        return c.phase == L::MovePhase::TRIGGER && c.transmissionNoted;
    case Point::ACCEPTANCE:
        return c.phase == L::MovePhase::OBSERVING && !c.observations && uploadsFrom(r, r.startFrame, 0x6041) == 1;
    case Point::COMPLETION:
        return c.phase == L::MovePhase::OBSERVING && c.observations == 2 &&
               uploadsFrom(r, r.startFrame, 0x6041) == 3;
    case Point::REARM:
        return c.phase == L::MovePhase::REARM && c.transmissionNoted;
    default: {
        const unsigned k = p == Point::PRE_TRIGGER ? 3u : unsigned(p) - unsigned(Point::PARAM0);
        return c.phase == L::MovePhase::PARAMETERS && c.setupCursor == k && c.transmissionNoted;
    }
    }
}
bool downloadPoint(Point p) {
    return (p >= Point::PARAM0 && p <= Point::PARAM3) || p == Point::POSSIBLE_TRIGGER || p == Point::REARM;
}
M::ActionOutcome expectedOutcome(Point p, Cut cut) {
    switch (cut) {
    case Cut::DEADLINE: return M::ActionOutcome::DEADLINE;
    case Cut::CANCEL:
    case Cut::STOP: return M::ActionOutcome::CANCELLED;
    case Cut::FAULT: return M::ActionOutcome::DRIVE_FAULT;
    case Cut::BOOT:
    case Cut::REBIND: return M::ActionOutcome::INVALIDATED;
    case Cut::MISSING: return M::ActionOutcome::SERVICE_TIMEOUT;
    default:
        // Stale: before the trigger the expired admission read refuses the next write. After it the
        // move's own fresh cycles carry on, unless a cycle in flight now holds old fields.
        return p <= Point::PRE_TRIGGER                            ? M::ActionOutcome::INVALIDATED
               : p == Point::ACCEPTANCE || p == Point::COMPLETION ? M::ActionOutcome::OBSERVATION_SCOPE
                                                                  : M::ActionOutcome::OBSERVED;
    }
}
void matrixCase(Point p, bool handedOff, Cut cut) {
    Rig r;
    r.drive.travelReads = 2; // Cycle 1 accepts, cycle 2 still moves, cycle 3 completes.
    r.ready();
    r.drive.position = 700;
    CHECK(r.relative(witness(r, 700), relEvidence(), relRequest(1000)).isOk());
    const unsigned start = r.startFrame;
    for (unsigned k = 0; k < 400 && r.a.moveActive() && !reached(r, p); ++k)
        r.step();
    CHECK(r.a.moveActive() && reached(r, p));
    // Owner-only turns: hand the queued download to the controller, or also receive its reply.
    const unsigned ownerTurns = p == Point::PRE_TRIGGER || p == Point::PROTOCOL ? 2u : handedOff ? 1u : 0u;
    r.serviceSession = false;
    r.steps(ownerTurns);
    r.serviceSession = true;
    const L::MoveContext& c = moveOf(r);
    const uint8_t setupBefore = c.setupEvidenceMask;
    const uint32_t observationsBefore = c.observations;
    const unsigned inFlight = r.wire.sentCount; // Frames before this one may have been answered.
    const unsigned at = r.wire.sentCount;
    const uint32_t moveOperation = c.operation;
    E::StopPolicy policy = stopPolicy(r);
    E::StopPath path = E::StopPath::NONE;
    switch (cut) {
    case Cut::DEADLINE: r.turn(c.deadlineUs); break;
    case Cut::CANCEL: CHECK(r.a.cancelMove(r.now)); break;
    case Cut::FAULT: CHECK(r.a.cancelMove(r.now, M::MoveInterruption::DRIVE_FAULT)); break;
    case Cut::BOOT: r.later(frame(0x701, 1)); break;
    case Cut::REBIND: CHECK(r.owner.invalidate(r.a.sequence(), r.now)); break;
    case Cut::MISSING: r.drive.mute = true; break;
    case Cut::STALE:
        r.serviceSession = false;
        r.steps(90); // 45 ms against a 36 ms age cap.
        r.serviceSession = true;
        break;
    case Cut::STOP:
        path = r.a.requestStop(L::StopKind::HALT, r.a.moveObservation(), policy, r.now);
        CHECK(path != E::StopPath::NONE);
        break;
    }
    finishAll(r);
    const M::ActionOutcome expected = expectedOutcome(p, cut);
    CHECK(c.state == (expected == M::ActionOutcome::OBSERVED ? M::ActionState::SUCCEEDED : M::ActionState::FAILED));
    CHECK(c.outcome == expected);
    if (cut == Cut::CANCEL || cut == Cut::FAULT || cut == Cut::STOP)
        CHECK(c.interruption == (cut == Cut::CANCEL  ? M::MoveInterruption::CANCELLED
                                 : cut == Cut::FAULT ? M::MoveInterruption::DRIVE_FAULT
                                                     : M::MoveInterruption::STOP));
    // Nothing converted, nothing replayed: at most one 005F, and it is the only edge the fake saw.
    onlyRelativeWords(r, start);
    const unsigned triggers = wordsFrom(r, start, 0x005f);
    CHECK(triggers <= 1 && triggers == r.drive.edges);
    const bool mayMove = c.execution == M::ActionExecution::ACKNOWLEDGED || c.execution == M::ActionExecution::UNKNOWN;
    CHECK(mayMove == (triggers == 1));
    CHECK(triggers == 1 || p <= Point::POSSIBLE_TRIGGER);
    for (unsigned k = 0; k < 4; ++k)
        CHECK(downloadsFrom(r, start, SETUP[k]) <= 1);
    // Earlier, possibly executed work stays in the record.
    for (unsigned k = 0; k < 4; ++k)
        if (setupBefore & (1u << k))
            CHECK((c.setupEvidenceMask & (1u << k)) && c.setupHistory[k].outcome == C::SdoOutcome::SUCCESS);
    if (p >= Point::PROTOCOL)
        CHECK(c.triggerPresent && c.trigger.outcome == C::SdoOutcome::SUCCESS &&
              c.execution == M::ActionExecution::ACKNOWLEDGED);
    CHECK(c.observations >= observationsBefore);
    if (p == Point::REARM)
        CHECK(c.completion == M::ActionCompletion::OBSERVED && c.completed.present);
    // The rearm comes only after an observed completion, as its own step, at most once.
    CHECK(!c.rearmPresent || c.completion == M::ActionCompletion::OBSERVED);
    CHECK(wordsFrom(r, start, 0x000f) == (c.rearm != M::ActionExecution::NOT_TRANSMITTED ? 1u : 0u));
    if (mayMove && c.completion != M::ActionCompletion::OBSERVED)
        CHECK(c.uncertain && r.a.moveFenced());
    else
        CHECK(!r.a.moveFenced());
    if (cut == Cut::STOP) {
        // Queued old targets never leave after a stop.
        if (downloadPoint(p) && !handedOff) {
            if (p == Point::REARM)
                CHECK(!wordsFrom(r, at, 0x000f));
            else if (p == Point::POSSIBLE_TRIGGER)
                CHECK(!triggers);
            else
                CHECK(!downloadsFrom(r, at, SETUP[unsigned(p) - unsigned(Point::PARAM0)]));
        }
        // The stop is its own operation with its own deadline, and never writes 000F first.
        unsigned stopFrame = r.wire.sentCount;
        for (unsigned i = at; i < r.wire.sentCount && stopFrame == r.wire.sentCount; ++i)
            if (wordAt(r, i, 0x010f) || wordAt(r, i, 0x000b))
                stopFrame = i;
        CHECK(stopFrame < r.wire.sentCount);
        CHECK(wordsFrom(r, at, 0x010f) + wordsFrom(r, at, 0x000b) == 1);
        if (p == Point::REARM && !handedOff)
            CHECK(wordsFrom(r, at, 0x010f) == 1); // From the 005F the unsent 000F left the latest.
        const L::ActionContext& halt = r.a.stopAction(L::StopKind::HALT);
        if (wordsFrom(r, at, 0x010f)) {
            CHECK(halt.operation > moveOperation && halt.deadlineUs == policy.deadlineUs);
            CHECK(halt.predecessorWord == 0x005f && !halt.predecessorDocumented);
            // Only there is a complete cycle after the held trigger. At the rearm, the stop cancelled
            // its 000F unsent, so the 005F is still the latest word and the completing cycle follows it.
            CHECK(p == Point::COMPLETION || (p == Point::REARM && !handedOff));
        } else {
            CHECK(r.a.bestEffort().operation > moveOperation);
            CHECK(r.a.bestEffort().deadlineUs == policy.deadlineUs ||
                  r.a.bestEffort().deadlineUs == policy.bestEffortDeadlineUs);
        }
        // A stop waits for the poll in flight: no stop frame leaves in the turn that poll did, and the
        // poll settled into the move as evidence.
        if (p == Point::ACCEPTANCE || p == Point::COMPLETION) {
            CHECK(uploadAt(r, inFlight - 1, 0x6041));
            CHECK(r.sentAt[stopFrame] >= r.sentAt[inFlight - 1] + TICK);
            CHECK(c.observationEvidencePresent && c.observationEvidence.transfer.index == 0x6041 &&
                  c.observationEvidence.outcome == C::SdoOutcome::SUCCESS);
        }
    }
    // Terminal stability.
    const Saved<L::MoveContext> terminal(c);
    quiet(r, 20);
    terminal.unchanged(c);
    // Attempted new motion. Once any download of the move may have reached the drive, the latest
    // controlword is gone and only a new explicit history can admit one. Before that nothing was
    // written: a download closed provably unsent leaves the same 000F the latest, so it and a fresh
    // read still describe the drive, and a new request is a new explicit edge.
    if (c.state == M::ActionState::FAILED) {
        const bool written = c.effectsPossible || cut == Cut::STOP;
        const unsigned before = r.wire.sentCount;
        const M::Status again = r.relative(witness(r, 1700), relEvidence(), relRequest(1000));
        if (written)
            CHECK(!again);
        else if (again)
            CHECK(r.a.cancelMove(r.now)); // Ended before any service: nothing of it can leave.
        r.steps(4);
        CHECK(r.wire.sentCount == before && r.drive.edges == triggers);
    }
}
void interruptionMatrix() {
    const Point points[] = {Point::MODE, Point::PARAM0, Point::PARAM1, Point::PARAM2, Point::PARAM3,
                            Point::PRE_TRIGGER, Point::POSSIBLE_TRIGGER, Point::PROTOCOL, Point::ACCEPTANCE,
                            Point::COMPLETION, Point::REARM};
    const Cut cuts[] = {Cut::DEADLINE, Cut::CANCEL, Cut::FAULT, Cut::BOOT, Cut::REBIND, Cut::MISSING,
                        Cut::STALE, Cut::STOP};
    static char name[96];
    for (Point p : points)
        for (unsigned handedOff = 0; handedOff < (downloadPoint(p) ? 2u : 1u); ++handedOff)
            for (Cut cut : cuts) {
                std::snprintf(name, sizeof name, "matrix %s%s / %s", POINTS[unsigned(p)],
                              handedOff ? " handed off" : "", CUTS[unsigned(cut)]);
                scenario = name;
                matrixCase(p, handedOff != 0, cut);
            }
}

// 9. Cancellation before and after the frame is handed to the controller.
void cancelAroundHandoff() {
    for (unsigned handedOff = 0; handedOff < 2; ++handedOff) {
        scenario = handedOff ? "cancel after handoff" : "cancel before handoff";
        Rig r;
        r.drive.travelReads = 1000000;
        r.ready();
        CHECK(r.relative(witness(r, 0), relEvidence(), relRequest(300)).isOk());
        for (unsigned k = 0; k < 400 && !reached(r, Point::POSSIBLE_TRIGGER); ++k)
            r.step();
        r.serviceSession = false;
        r.steps(handedOff);
        r.serviceSession = true;
        CHECK(r.a.cancelMove(r.now));
        finishAll(r);
        const L::MoveContext& c = moveOf(r);
        CHECK(c.outcome == M::ActionOutcome::CANCELLED && c.setup == M::ActionExecution::ACKNOWLEDGED);
        if (!handedOff) { // Closed in the owner: it never reached the controller.
            CHECK(!wordsFrom(r, r.startFrame, 0x005f) && !r.drive.edges);
            CHECK(c.execution == M::ActionExecution::NOT_TRANSMITTED && !c.uncertain && !r.a.moveFenced());
        } else { // It may have left, so it settles; its acknowledgement is kept and the fake moves.
            CHECK(wordsFrom(r, r.startFrame, 0x005f) == 1 && r.drive.edges == 1);
            CHECK(c.execution == M::ActionExecution::ACKNOWLEDGED && c.uncertain && r.a.moveFenced());
        }
        // A local cancel is not a stop.
        CHECK(!wordsFrom(r, r.startFrame, 0x010f) && !wordsFrom(r, r.startFrame, 0x000b));
    }
}

// 10. A mid-curve stop comes from the held 005F, with no 000F preamble.
void stopFromHeldTrigger() {
    scenario = "stop from held trigger";
    Rig r;
    r.drive.travelReads = 1000000;
    r.ready();
    CHECK(r.relative(witness(r, 0), relEvidence(), relRequest(-4000)).isOk());
    for (unsigned k = 0; k < 400 && moveOf(r).observations < 1; ++k)
        r.step();
    const unsigned at = r.wire.sentCount;
    const E::StopPolicy policy = stopPolicy(r);
    CHECK(r.a.requestStop(L::StopKind::HALT, r.a.moveObservation(), policy, r.now) == E::StopPath::CONFIRMED);
    finishAll(r);
    const L::ActionContext& s = r.a.stopAction(L::StopKind::HALT);
    CHECK(s.state == M::ActionState::SUCCEEDED && s.predecessorWord == 0x005f && !s.predecessorDocumented);
    CHECK(s.predecessorBasis == L::PredecessorBasis::PROJECT_DECISION);
    CHECK(wordsFrom(r, at, 0x010f) == 1 && !wordsFrom(r, at, 0x000f) && !wordsFrom(r, at, 0x000b));
    CHECK(moveOf(r).interruption == M::MoveInterruption::STOP && r.a.moveFenced());
}

// 11. Both nodes under urgent pressure: simultaneous stops, one node's results left unread.
void bothNodesUrgent() {
    scenario = "two nodes urgent";
    Rig r(true);
    r.one.drive.travelReads = r.two.drive.travelReads = 1000000;
    r.ready(1);
    r.ready(2);
    r.two.drive.position = -100;
    const unsigned start = r.wire.sentCount;
    CHECK(r.relative(witness(r, 0, M::RelativeBasis::ACTUAL, 1), relEvidence(), relRequest(5000), 1).isOk());
    CHECK(r.relative(witness(r, -100, M::RelativeBasis::ACTUAL, 2), relEvidence(), relRequest(-5000), 2).isOk());
    for (unsigned k = 0; k < 600 && (moveOf(r, 1).observations < 1 || moveOf(r, 2).observations < 1); ++k)
        r.step();
    CHECK(moveOf(r, 1).observations >= 1 && moveOf(r, 2).observations >= 1);
    const unsigned at = r.wire.sentCount;
    const E::StopPolicy policy = stopPolicy(r);
    CHECK(r.one.a.requestStop(L::StopKind::HALT, r.one.a.moveObservation(), policy, r.now) == E::StopPath::CONFIRMED);
    CHECK(r.two.a.requestStop(L::StopKind::QUICK_STOP, r.two.a.moveObservation(), policy, r.now) ==
          E::StopPath::CONFIRMED);
    // Node 2's terminals stay unread; node 1's stop is not blocked by them.
    r.serviceTwo = false;
    for (unsigned k = 0; k < 200 && r.one.a.stopActive(); ++k)
        r.step();
    CHECK(r.one.a.stopAction(L::StopKind::HALT).state == M::ActionState::SUCCEEDED);
    CHECK(r.two.a.stopActive());
    r.serviceTwo = true;
    finishAll(r);
    CHECK(r.two.a.stopAction(L::StopKind::QUICK_STOP).state == M::ActionState::SUCCEEDED);
    for (unsigned node = 1; node <= 2; ++node) {
        const L::ActionContext& s = r.axis(node).a.stopAction(node == 1 ? L::StopKind::HALT : L::StopKind::QUICK_STOP);
        CHECK(s.predecessorWord == 0x005f && !s.predecessorDocumented && s.operation > moveOf(r, node).operation);
        CHECK(moveOf(r, node).interruption == M::MoveInterruption::STOP && r.axis(node).a.moveFenced());
        CHECK(wordsFrom(r, start, 0x005f, node) == 1 && !wordsFrom(r, at, 0x000f, node));
        onlyRelativeWords(r, start, node);
    }
    CHECK(wordsFrom(r, at, 0x010f, 1) == 1 && !wordsFrom(r, at, 0x000b, 1));
    CHECK(wordsFrom(r, at, 0x000b, 2) == 1 && !wordsFrom(r, at, 0x010f, 2));
    CHECK(r.one.drive.edges == 1 && r.two.drive.edges == 1);
}

// 12. A lost trigger reply: unknown, fenced, best-effort stop only, and an identical late ACK changes
// nothing.
void ambiguousLateAck() {
    scenario = "ambiguous late ack";
    Rig r;
    r.drive.silentIndex = 0x6040;
    r.drive.silentWord = 0x005f;
    r.ready();
    CHECK(r.relative(witness(r, 0), relEvidence(), relRequest(900)).isOk());
    const unsigned first = r.startFrame;
    r.finishMove();
    const L::MoveContext& c = moveOf(r);
    CHECK(c.outcome == M::ActionOutcome::SERVICE_TIMEOUT && c.execution == M::ActionExecution::UNKNOWN);
    CHECK(c.trigger.transmissionPossible && c.uncertain && r.a.moveFenced() && !r.a.latestControl());
    CHECK(!r.relative(witness(r, 900), relEvidence(), relRequest(900)));
    static L::ReadContext none;
    const E::StopPath path = r.a.requestStop(L::StopKind::QUICK_STOP, none, stopPolicy(r), r.now);
    CHECK(path == E::StopPath::BEST_EFFORT || path == E::StopPath::HELD);
    finishAll(r);
    CHECK(r.a.bestEffort().outcome == M::ActionOutcome::STOP_SENT_UNCONFIRMED);
    const Saved<L::MoveContext> move(c);
    const Saved<L::BestEffortStop> stop(r.a.bestEffort());
    M::Frame late = frame(0x581, 8);
    const uint8_t reply[8] = {0x60, 0x40, 0x60, 0x00, 0, 0, 0, 0};
    std::memcpy(late.data, reply, sizeof reply);
    r.later(late);
    r.steps(10);
    move.unchanged(c);
    stop.unchanged(r.a.bestEffort());
    CHECK(wordsFrom(r, first, 0x005f) == 1 && wordsFrom(r, first, 0x000b) == 1);
    CHECK(!r.relative(witness(r, 900), relEvidence(), relRequest(900)));
}

// 13. Bus-off during a move, a held stop, one explicit recovery: one 000B, and nothing resumes.
void explicitRecovery() {
    scenario = "explicit recovery";
    Rig r;
    r.drive.travelReads = 1000000;
    r.ready();
    CHECK(r.relative(witness(r, 0), relEvidence(), relRequest(2000)).isOk());
    const unsigned start = r.startFrame;
    for (unsigned k = 0; k < 400 && moveOf(r).observations < 1; ++k)
        r.step();
    CHECK(r.owner.reportController(C::ControllerState::BUS_OFF, r.now));
    r.step();
    finishAll(r);
    const L::MoveContext& c = moveOf(r);
    CHECK(c.state == M::ActionState::FAILED && c.execution == M::ActionExecution::ACKNOWLEDGED && r.a.moveFenced());
    const Saved<L::MoveContext> ended(c);
    static L::ReadContext none;
    const unsigned at = r.wire.sentCount;
    CHECK(r.a.requestStop(L::StopKind::QUICK_STOP, none, stopPolicy(r), r.now) == E::StopPath::HELD);
    r.steps(3);
    CHECK(r.wire.sentCount == at && r.a.stopPath() == E::StopPath::HELD);
    O::RecoveryId recovery;
    CHECK(r.owner.recover(r.now, r.now + 4 * TICK, recovery));
    r.step();
    CHECK(r.owner.recoveryResult(recovery) && r.owner.recoveryResult(recovery)->outcome == O::RecoveryOutcome::RETIRED);
    finishAll(r);
    CHECK(r.a.bestEffort().outcome == M::ActionOutcome::STOP_SENT_UNCONFIRMED);
    CHECK(r.wire.sentCount == at + 1 && wordAt(r, at, 0x000b));
    ended.unchanged(c); // Recovery never resumes, rearms or replays the move.
    CHECK(wordsFrom(r, start, 0x005f) == 1 && !wordsFrom(r, start, 0x000f));
    CHECK(!r.relative(witness(r, 2000), relEvidence(), relRequest(2000)));
}

// 14. Identical values twice are two explicitly admitted edges, each with its own witness.
void repeatedValues() {
    scenario = "repeated values";
    Rig r;
    r.ready();
    const M::AxisReference w0 = witness(r, 0);
    CHECK(r.relative(w0, relEvidence(), relRequest(1000)).isOk());
    const unsigned first = r.startFrame;
    r.finishMove();
    CHECK(moveOf(r).state == M::ActionState::SUCCEEDED && r.drive.position == 1000);
    const uint32_t firstOperation = moveOf(r).operation;
    quiet(r); // No automatic second edge.
    CHECK(r.drive.edges == 1 && wordsFrom(r, first, 0x005f) == 1);
    r.readState(r.state);
    // The first witness predates the rearm: it cannot start the second move.
    refused(r, r.relative(w0, relEvidence(), relRequest(1000)), M::Err::INVALID_CONFIG, M::MoveError::REFERENCE);
    r.readState(r.state);
    CHECK(r.relative(witness(r, 1000), relEvidence(), relRequest(1000)).isOk());
    const unsigned second = r.startFrame;
    r.finishMove();
    CHECK(moveOf(r).state == M::ActionState::SUCCEEDED && moveOf(r).operation > firstOperation);
    CHECK(moveOf(r).parameters.target.endpointNative == 2000 && r.drive.position == 2000);
    CHECK(r.drive.edges == 2 && wordsFrom(r, first, 0x005f) == 2 && wordsFrom(r, second, 0x005f) == 1);
    CHECK(downloadAt(r, first, 0x607a, 4, 1000) && downloadAt(r, second, 0x607a, 4, 1000));
}

// 15. Partial setup then failure: never retried, nothing after it.
void partialSetup() {
    scenario = "partial setup";
    Rig r;
    r.ready();
    r.drive.abortIndex = 0x6083;
    CHECK(r.relative(witness(r, 0), relEvidence(), relRequest(1000)).isOk());
    r.finishMove();
    const L::MoveContext& c = moveOf(r);
    CHECK(c.outcome == M::ActionOutcome::PROTOCOL_REJECTED && c.setup == M::ActionExecution::UNKNOWN);
    CHECK(c.setupEvidenceMask == 7 && c.setupHistory[0].outcome == C::SdoOutcome::SUCCESS);
    CHECK(c.setupHistory[1].outcome == C::SdoOutcome::SUCCESS && c.setupHistory[2].abortValid);
    CHECK(downloadsFrom(r, r.startFrame, 0x6083) == 1 && !downloadsFrom(r, r.startFrame, 0x6084));
    CHECK(!wordsFrom(r, r.startFrame, 0x005f) && !r.drive.edges && !r.a.moveFenced());
    quiet(r);
}

// 16. Timeout after the trigger, then attempted new motion: fenced, whatever is supplied.
void timeoutThenNewMotion() {
    scenario = "timeout then new motion";
    Rig r;
    r.drive.travelReads = 1000000;
    r.ready();
    CHECK(r.relative(witness(r, 0), relEvidence(), relRequest(1500), 1, profile(), 60000).isOk());
    const unsigned first = r.startFrame;
    r.finishMove();
    CHECK(moveOf(r).outcome == M::ActionOutcome::DEADLINE && moveOf(r).execution == M::ActionExecution::ACKNOWLEDGED);
    CHECK(r.a.moveFenced() && r.a.latestControl() && r.a.latestControl()->transfer.data[0] == 0x5f);
    // The trigger is never restarted. Even an explicit 000F, a fresh idle read and a fresh witness are
    // refused: none shows the trajectory ended.
    r.drive.moving = false;
    r.drive.monitor = 1;
    r.writeThrough(0x6040, 2, 0x000f);
    r.readState(r.state);
    refused(r, r.relative(witness(r, 1500), relEvidence(), relRequest(1500)), M::Err::INVALID_CONFIG,
            M::MoveError::STATE);
    CHECK(wordsFrom(r, first, 0x005f) == 1 && r.drive.edges == 1);
}

// 17. An acknowledged trigger with a stale completion bit never completes.
void staleCompletionBit() {
    scenario = "stale bit 15";
    Rig r;
    r.drive.clearCompletion = false;
    r.drive.travelReads = 1000000;
    r.ready();
    CHECK(r.relative(witness(r, 0), relEvidence(), relRequest(100), 1, profile(), 60000).isOk());
    r.finishMove();
    const L::MoveContext& c = moveOf(r);
    CHECK(c.outcome == M::ActionOutcome::DEADLINE && c.completion == M::ActionCompletion::NOT_OBSERVED);
    CHECK(c.execution == M::ActionExecution::ACKNOWLEDGED && c.acceptance.present && !c.absence.present);
    CHECK((c.latest.statusword & COMPLETE_BIT) && !wordsFrom(r, r.startFrame, 0x000f) && r.a.moveFenced());
}

// 18. The rearm: an observed completion survives its failure, and it is never retried.
void rearmNoRetry() {
    scenario = "relative rearm";
    for (unsigned lost = 0; lost < 2; ++lost) {
        Rig r;
        r.ready();
        (lost ? r.drive.silentIndex : r.drive.abortIndex) = 0x6040;
        (lost ? r.drive.silentWord : r.drive.abortWord) = 0x000f;
        CHECK(r.relative(witness(r, 0), relEvidence(), relRequest(100)).isOk());
        r.finishMove();
        const L::MoveContext& c = moveOf(r);
        CHECK(c.completion == M::ActionCompletion::OBSERVED && c.execution == M::ActionExecution::ACKNOWLEDGED);
        CHECK(c.outcome == (lost ? M::ActionOutcome::SERVICE_TIMEOUT : M::ActionOutcome::PROTOCOL_REJECTED));
        CHECK(c.uncertain && !r.a.moveFenced() && wordsFrom(r, r.startFrame, 0x000f) == 1);
        quiet(r);
        CHECK(wordsFrom(r, r.startFrame, 0x000f) == 1);
    }
}

// 19. G-13: the completing cycle processed at the cutoff never completes.
void processingCutoff() {
    scenario = "relative cutoff";
    Rig r;
    r.ready();
    CHECK(r.relative(witness(r, 0), relEvidence(), relRequest(100)).isOk());
    const uint64_t deadline = moveOf(r).deadlineUs;
    for (unsigned k = 0; k < 400 && uploadsFrom(r, r.startFrame, 0x606c) < 2; ++k)
        r.step();
    CHECK(uploadAt(r, r.wire.sentCount - 1, 0x606c) && moveOf(r).observations == 1);
    r.serviceSession = false;
    r.steps(3);
    CHECK(r.now < deadline);
    r.serviceSession = true;
    r.turn(deadline);
    r.finishMove();
    const L::MoveContext& c = moveOf(r);
    CHECK(c.outcome == M::ActionOutcome::DEADLINE && c.completion == M::ActionCompletion::NOT_OBSERVED);
    CHECK(c.observationEvidencePresent && c.observationEvidence.completedUs < deadline);
    CHECK(!wordsFrom(r, r.startFrame, 0x000f) && r.a.moveFenced());
}

// The owner terminal of a successful download, by hand, for library calls run outside the session.
C::SdoCompletion acknowledged(const C::SdoTransfer& t, uint64_t at, M::Generation node) {
    C::SdoCompletion c;
    c.transfer = t;
    c.outcome = C::SdoOutcome::SUCCESS;
    c.nodeGeneration = node;
    c.admittedUs = c.completedUs = at;
    c.queued = c.transmissionPossible = c.protocolAcknowledged = c.effectPossible = true;
    return c;
}
// 20. The library calls directly: refusals and notice rechecks change nothing; validate equals prepare.
void atomicity() {
    scenario = "relative atomicity";
    Rig r;
    r.ready();
    static L::MoveContext c;
    static L::ReadContext observation;
    const L::AxisReservation live = r.reservation();
    const L::ReadTarget here = r.current();
    const uint32_t operation = r.a.nextOperation();
    CHECK(r.owner.beginOperation(r.a.sequence(), operation, r.now + LEAD, r.now));
    M::AxisConfig axis = axisConfig();
    M::AxisReference w = witness(r, 0);
    const C::SdoCompletion control = *r.a.latestControl();
    const L::MoveOptions o = options();
    struct Refusal {
        L::MoveEvidence evidence;
        M::MoveRequest request;
    };
    M::MoveRequest queued = relRequest(10, M::RelativeBasis::QUEUED);
    const Refusal refusals[] = {{L::MoveEvidence(), relRequest(10)}, {evidence(), relRequest(10)},
                                {relEvidence(), relRequest(0)},      {relEvidence(), moveRequest(10)},
                                {relEvidence(), queued}};
    const Saved<L::MoveContext> empty(c);
    for (const Refusal& k : refusals) {
        const M::Status checked = L::validateMoveRelative(c, live, here, r.state, control, r.identity, k.evidence,
                                                          k.request, profile(), nullptr, axis, w, operation,
                                                          r.now, r.now + LEAD, o);
        const M::Status prepared = L::prepareMoveRelative(c, live, here, r.state, control, r.identity, k.evidence,
                                                          k.request, profile(), nullptr, axis, w, operation,
                                                          r.now, r.now + LEAD, o);
        CHECK(!prepared && prepared.code == checked.code && prepared.detail == checked.detail);
        empty.unchanged(c);
    }
    CHECK(L::prepareMoveRelative(c, live, here, r.state, control, r.identity, relEvidence(), relRequest(10),
                                 profile(), nullptr, axis, w, operation, r.now, r.now + LEAD, o));
    CHECK(c.state == M::ActionState::ACTIVE && c.parameters.target.requested.relative);
    L::PreparedMove work;
    CHECK(L::nextMove(c, observation, live, here, r.now, work) && work.ready && work.transfer.index == 0x607a);
    // Each recheck refusal leaves the context byte for byte as it was.
    const Saved<L::MoveContext> active(c);
    M::AxisReference changed = w;
    ++changed.observedUs;
    CHECK(L::noteMoveTransmission(c, live, here, axis, &changed, nullptr, 0, r.now).detail ==
          AXIS + int32_t(M::AxisError::STALE_REFERENCE));
    CHECK(L::noteMoveTransmission(c, live, here, axis, nullptr, nullptr, 0, r.now).detail ==
          AXIS + int32_t(M::AxisError::STALE_GENERATION));
    changed = w;
    changed.idle = false;
    CHECK(L::noteMoveTransmission(c, live, here, axis, &changed, nullptr, 0, r.now).detail ==
          int32_t(M::MoveError::REFERENCE));
    M::AxisConfig moved = axis;
    ++moved.referenceGeneration;
    CHECK(L::noteMoveTransmission(c, live, here, moved, &w, nullptr, 0, r.now).detail ==
          AXIS + int32_t(M::AxisError::STALE_GENERATION));
    CHECK(L::noteMoveTransmission(c, live, here, axis, &w, nullptr, 0, r.now + o.maximumAgeUs + 1).detail ==
          int32_t(M::MoveError::STATE));
    CHECK(L::noteMoveTransmission(c, live, here, c.parameters.axis, &w, nullptr, 0, r.now).detail ==
          int32_t(M::MoveError::OVERLAP));
    CHECK(L::noteMoveTransmission(c, live, here, axis, &c.parameters.target.retainedReference, nullptr, 0, r.now)
              .detail == int32_t(M::MoveError::OVERLAP));
    active.unchanged(c);
    CHECK(L::noteMoveTransmission(c, live, here, axis, &w, nullptr, 0, r.now));
    C::SdoCompletion ack = acknowledged(work.transfer, r.now, here.nodeGeneration);
    CHECK(L::advanceMove(c, observation, live, here, ack, r.now));
    CHECK(c.setupCursor == 1 && c.setup == M::ActionExecution::UNKNOWN);
    CHECK(L::advanceMove(c, observation, live, here, M::MoveInterruption::CANCELLED, r.now));
    CHECK(c.state == M::ActionState::FAILED && c.setupHistory[0].outcome == C::SdoOutcome::SUCCESS);
}

// 21. A device counter zero advances the configuration epoch: the prepared relative move can only be
// ended, its frozen endpoint is no longer current, and the old witness cannot admit it again.
void counterZeroInvalidates() {
    scenario = "relative counter zero";
    Rig r;
    r.open();
    r.readIdentity();
    r.writeThrough(0x6040, 2, 0x000f);
    r.readState(r.state, uint8_t(STATE_MASK | bit(L::StateBlock::FEEDBACK)));
    static L::MoveContext move;
    static L::ReadContext observation;
    static L::ActionContext counter;
    const L::AxisReservation live = r.reservation();
    L::ReadTarget here = r.current();
    M::AxisConfig axis = axisConfig();
    const M::AxisReference frozen = witness(r, 0);
    M::AxisReference current = frozen;
    const C::SdoCompletion control = *r.a.latestControl();
    const uint32_t operation = r.a.nextOperation();
    CHECK(r.owner.beginOperation(r.a.sequence(), operation, r.now + LEAD, r.now));
    CHECK(L::prepareMoveRelative(move, live, here, r.state, control, r.identity, relEvidence(), relRequest(400),
                                 profile(), nullptr, axis, frozen, operation, r.now, r.now + LEAD, options()));
    L::ActionOptions budgets;
    budgets.service = reads();
    budgets.observationIntervalUs = 1000;
    budgets.maximumAgeUs = uint64_t(QUEUE + uint64_t(RESPONSE)) * 9;
    CHECK(L::prepareSetDevicePosition(counter, live, r.state, 0, axis, operation + 1, r.now, r.now + LEAD, budgets));
    for (unsigned word = 0; word < 2; ++word) {
        L::PreparedAction work;
        CHECK(L::nextAction(counter, live, here, r.now, work) && work.ready);
        CHECK(L::noteActionTransmission(counter, live, here, axis, current, r.now));
        if (!word)
            CHECK(L::advanceAction(counter, live, here,
                                   acknowledged(work.transfer, r.now, here.nodeGeneration), r.now));
    }
    CHECK(here.channel.binding.configurationGeneration == 8 && !current.nativeKnown);
    const Saved<L::MoveContext> before(move);
    L::PreparedMove next;
    CHECK(L::nextMove(move, observation, live, here, r.now, next).detail == int32_t(M::MoveError::TARGET));
    CHECK(L::noteMoveTransmission(move, live, here, axis, &frozen, nullptr, 0, r.now).detail ==
          int32_t(M::MoveError::TARGET));
    CHECK(!M::validatePreparedTarget(move.parameters.target, axis, &frozen, r.now));
    before.unchanged(move);
    CHECK(L::advanceMove(move, observation, live, here, M::MoveInterruption::INVALIDATED, r.now));
    CHECK(move.outcome == M::ActionOutcome::INVALIDATED && move.execution == M::ActionExecution::NOT_TRANSMITTED);
    static L::MoveContext again;
    L::AxisReservation renewed = live;
    renewed.target = here;
    CHECK(!L::validateMoveRelative(again, renewed, here, r.state, control, r.identity, relEvidence(),
                                   relRequest(400), profile(), nullptr, axis, frozen, operation + 2, r.now,
                                   r.now + LEAD, options()));
}

// The trigger noted at its admission, the frame still queued for the next owner turn.
void toNotedTrigger(Rig& r) {
    for (unsigned k = 0; k < 400 && r.a.moveActive() && !reached(r, Point::POSSIBLE_TRIGGER); ++k)
        r.step();
    CHECK(reached(r, Point::POSSIBLE_TRIGGER) && !wordsFrom(r, r.startFrame, 0x005f));
}

// 22. Packet 21 review D1: the owner's clock runs ahead of the session's, as after another axis's
// turn. A time behind it refuses before anything changes. A stop request still withdraws the move,
// at the owner's time, where the owner's cancel of the queued trigger wins: it never leaves after the
// withdrawal.
void ownerClockAhead() {
    for (unsigned stop = 0; stop < 2; ++stop) {
        scenario = stop ? "owner ahead, stop" : "owner ahead, cancel";
        Rig r;
        r.drive.travelReads = 1000000;
        r.ready();
        CHECK(r.relative(witness(r, 0), relEvidence(), relRequest(300)).isOk());
        toNotedTrigger(r);
        const uint64_t ahead = r.now + 200, behind = r.now + 100;
        CHECK(r.owner.service(ahead, budget(8, 0, 8, 0, 0))); // Transmits nothing.
        CHECK(r.owner.lastTimeUs() == ahead && !wordsFrom(r, r.startFrame, 0x005f));
        static L::ReadContext none;
        if (!stop) {
            const Saved<E::AxisSession> before(r.a);
            CHECK(!r.a.cancelMove(behind));
            CHECK(!r.a.cancelMove(behind, M::MoveInterruption::DRIVE_FAULT));
            r.a.service(behind); // A session turn behind the owner does nothing either.
            CHECK(!r.a.beginOperation(behind + LEAD, behind));
            before.unchanged(r.a);
            CHECK(r.a.cancelMove(ahead));
        } else {
            CHECK(r.a.requestStop(L::StopKind::HALT, none, stopPolicy(r), behind) == E::StopPath::NONE);
            CHECK(!r.a.stopActive());
        }
        r.now = ahead;
        finishAll(r);
        const L::MoveContext& c = moveOf(r);
        CHECK(c.state == M::ActionState::FAILED && c.outcome == M::ActionOutcome::CANCELLED);
        CHECK(c.interruption == (stop ? M::MoveInterruption::STOP : M::MoveInterruption::CANCELLED));
        CHECK(c.execution == M::ActionExecution::NOT_TRANSMITTED && c.triggerPresent &&
              !c.trigger.transmissionPossible && c.servicedUs >= ahead);
        CHECK(!wordsFrom(r, r.startFrame, 0x005f) && !r.drive.edges && !r.a.moveFenced());
        CHECK(!wordsFrom(r, r.startFrame, 0x010f) && !wordsFrom(r, r.startFrame, 0x000b));
        if (stop) {
            // The refused request sent nothing. One at a valid time goes out once, as 000B: the
            // acknowledged parameters left no latest word.
            const unsigned at = r.wire.sentCount;
            const E::StopPath path = r.a.requestStop(L::StopKind::HALT, none, stopPolicy(r), r.now);
            CHECK(path == E::StopPath::BEST_EFFORT || path == E::StopPath::HELD);
            finishAll(r);
            CHECK(wordsFrom(r, at, 0x000b) == 1 && r.wire.sentCount == at + 1);
            CHECK(!wordsFrom(r, r.startFrame, 0x005f) && !r.drive.edges);
        }
    }
}

// 23. Packet 21 review D2: the trigger was noted at its admission and waits for the next owner turn.
// With the transport's beforeTransmit calling preflight, a reference loss or a witness expiry in
// between stops it there, provably unsent, and the reason stays readable. Without the hook only the
// admission held, and the trigger leaves.
void recheckAtHandover() {
    const M::ReferenceLoss losses[] = {M::ReferenceLoss::COORDINATES, M::ReferenceLoss::POSITION};
    for (unsigned wired = 0; wired < 2; ++wired)
        for (M::ReferenceLoss loss : losses) {
            scenario = wired ? "handover loss, preflight" : "handover loss, no preflight";
            Rig r;
            r.wire.preflight = wired != 0;
            r.drive.travelReads = 2;
            r.ready();
            CHECK(r.relative(witness(r, 0), relEvidence(), relRequest(300)).isOk());
            toNotedTrigger(r);
            M::AxisReference scratch = r.one.witness;
            CHECK(M::invalidateAxisReference(r.one.coordinates, scratch, loss));
            finishAll(r);
            const L::MoveContext& c = moveOf(r);
            if (!wired) {
                CHECK(wordsFrom(r, r.startFrame, 0x005f) == 1 && r.drive.edges == 1);
                CHECK(c.execution == M::ActionExecution::ACKNOWLEDGED && r.a.moveRefusal().isOk());
                continue;
            }
            CHECK(c.state == M::ActionState::FAILED && c.outcome == M::ActionOutcome::INVALIDATED);
            CHECK(c.interruption == M::MoveInterruption::INVALIDATED);
            CHECK(c.setup == M::ActionExecution::ACKNOWLEDGED && c.execution == M::ActionExecution::NOT_TRANSMITTED);
            CHECK(c.triggerPresent && !c.trigger.transmissionPossible);
            CHECK(!wordsFrom(r, r.startFrame, 0x005f) && !r.drive.edges && !r.a.moveFenced());
            CHECK(r.wire.preflightRefusals == 1);
            CHECK(r.a.moveRefusal().detail == AXIS + int32_t(M::AxisError::STALE_GENERATION));
            onlyRelativeWords(r, r.startFrame);
        }
    // The witness expires exactly between the notice and the handover. Every rig here runs the same
    // schedule, so an identical one measures how long after preparation the trigger is noted.
    uint64_t delay = 0;
    {
        Rig probe;
        probe.ready();
        const uint64_t prepared = probe.now;
        CHECK(probe.relative(witness(probe, 0), relEvidence(), relRequest(300)).isOk());
        toNotedTrigger(probe);
        delay = probe.now - prepared;
    }
    scenario = "handover witness expiry";
    Rig r;
    r.ready();
    M::AxisReference w = witness(r, 0);
    w.maximumAgeUs = delay; // Inclusive: fresh at the notice, too old one turn later.
    CHECK(r.relative(w, relEvidence(), relRequest(300)).isOk());
    toNotedTrigger(r);
    CHECK(r.now - w.observedUs == delay);
    finishAll(r);
    const L::MoveContext& c = moveOf(r);
    CHECK(c.outcome == M::ActionOutcome::INVALIDATED && c.execution == M::ActionExecution::NOT_TRANSMITTED);
    CHECK(!wordsFrom(r, r.startFrame, 0x005f) && !r.drive.edges && !r.a.moveFenced());
    CHECK(r.a.moveRefusal().detail == AXIS + int32_t(M::AxisError::STALE_REFERENCE));
}

// 24. Cancel while the trigger is held in the controller's adapter: accepted for transmission, not yet
// reported, not yet on the bus. The session cannot prove it unsent, so withdrawMove does not cancel
// it. Its terminal settles as evidence, whether the adapter then sends it or reports a failure, and
// a trigger that may have left fences new motion.
void cancelWhileHeld() {
    for (unsigned sent = 0; sent < 2; ++sent) {
        scenario = sent ? "held trigger, then sent" : "held trigger, then failed";
        Rig r;
        r.drive.travelReads = 1000000;
        r.ready();
        CHECK(r.relative(witness(r, 0), relEvidence(), relRequest(300)).isOk());
        toNotedTrigger(r);
        r.wire.autoReport = false;
        r.holdTx = true;
        r.step(); // The owner hands the 005F over; the adapter holds it.
        const unsigned held = r.wire.sentCount - 1;
        CHECK(wordAt(r, held, 0x005f) && r.wire.pending[held] && !r.drive.edges);
        CHECK(r.a.cancelMove(r.now));
        CHECK(r.a.moveActive() && r.a.busy()); // Withdrawn, its terminal still to come.
        r.wire.report(held, sent != 0);
        if (!sent)
            r.served = held + 1; // The controller never put it on the bus.
        r.wire.autoReport = true;
        r.holdTx = false;
        finishAll(r);
        const L::MoveContext& c = moveOf(r);
        CHECK(c.state == M::ActionState::FAILED && c.outcome == M::ActionOutcome::CANCELLED);
        CHECK(c.interruption == M::MoveInterruption::CANCELLED && c.triggerPresent);
        CHECK(c.execution == (sent ? M::ActionExecution::ACKNOWLEDGED : M::ActionExecution::UNKNOWN));
        CHECK(c.trigger.transmissionPossible && c.uncertain && r.a.moveFenced());
        CHECK(r.drive.edges == sent && wordsFrom(r, r.startFrame, 0x005f) == 1);
        CHECK(!wordsFrom(r, r.startFrame, 0x010f) && !wordsFrom(r, r.startFrame, 0x000b));
    }
}

void runAll() {
    golden();
    gates();
    boundaries();
    zeroDisplacement();
    commandedBase();
    completionWithDifferentReference();
    admissionRechecks();
    interruptionMatrix();
    cancelAroundHandoff();
    stopFromHeldTrigger();
    bothNodesUrgent();
    ambiguousLateAck();
    explicitRecovery();
    repeatedValues();
    partialSetup();
    timeoutThenNewMotion();
    staleCompletionBit();
    rearmNoRetry();
    processingCutoff();
    atomicity();
    counterZeroInvalidates();
    ownerClockAhead();
    recheckAtHandover();
    cancelWhileHeld();
}
} // namespace

int main() {
    for (int async = 0; async < 2; ++async) {
        asyncTraffic = async != 0;
        runAll();
    }
    std::printf("move_relative: %u checks\n", checks);
    return 0;
}
