// SPDX-License-Identifier: MIT
// prepareMoveRelative through the real owner, the selected SDO engine and AxisSession, on the shared
// fake of support/MoveRig.h, and the combined interruption matrix of both moves. In tests 1 to 30 every
// MoveEvidence::relativeBase is this test's switch, opened only to exercise the lifecycle, and the
// fake's base is a switch too; nothing there shows what a drive adds a displacement to, or that it
// accepts, moves or completes one. Tests 31 to 33 replay retained captures of trials that measured
// those facts on one firmware, as drive reports; the replays check the library's reading of the
// captured values, not the drive. NMT stopped is never used as a drive state, and no free SDO channel
// is fabricated. Every scenario runs twice, the second time with heartbeat traffic delivered on every
// turn.
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
// Scenarios on Rig(true) bind node 2 on slot 1 beside node 1; they need a capacity set with two
// channels. A one-channel set runs the others, which bind node 1 alone.
constexpr bool PAIRED_CHANNELS = C::SdoClient::CHANNELS >= 2;

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
    // Attempted new motion, with the admission read taken before the move. A drive fault fences the
    // session (faultBlocked), whatever is read. Otherwise, once any download of the move may have
    // reached the drive, that read no longer follows the latest acknowledged controlword: the 000F it
    // followed is withdrawn, or the move's own acknowledged rearm has replaced it. It is refused, and
    // only a fresh read after the latest word can admit a new move. After a cancel at a handed-off
    // rearm that latest word is the rearm itself, as after a completed move, so a fresh read admits
    // a new move: a new explicit edge, not a resumption. Before any download may have left nothing was
    // written: a download closed provably unsent leaves the same 000F the latest, so it and a fresh
    // read still describe the drive, and a new request is a new explicit edge.
    if (c.state == M::ActionState::FAILED) {
        const bool written = c.effectsPossible || cut == Cut::STOP;
        const unsigned before = r.wire.sentCount;
        const M::Status again = r.relative(witness(r, 1700), relEvidence(), relRequest(1000));
        CHECK(r.a.faultBlocked() == (cut == Cut::FAULT));
        if (cut == Cut::FAULT)
            CHECK(!again && again.msg && std::strstr(again.msg, "drive fault"));
        else if (written)
            CHECK(!again);
        else if (again)
            CHECK(r.a.cancelMove(r.now)); // Ended before any service: nothing of it can leave.
        unsigned reads = 0; // The fresh read's uploads below.
        if (p == Point::REARM && handedOff && cut == Cut::CANCEL) {
            CHECK(r.a.latestControl() && r.a.latestControl()->transfer.operation == moveOperation);
            r.readState(r.state);
            reads = r.wire.sentCount - before;
            CHECK(reads == 5 && !downloadsFrom(r, before, 0x6040));
            CHECK(r.relative(witness(r, 1700), relEvidence(), relRequest(1000)).isOk());
            CHECK(r.a.cancelMove(r.now)); // Ended before any service: nothing of it can leave.
        }
        r.steps(4);
        CHECK(r.wire.sentCount == before + reads);
        CHECK(r.drive.edges == triggers);
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

// 10. A mid-curve stop comes from the held 005F, with no 000F preamble. The fake's HALT reports as the
// drives did (832 with haltReads 0, 828 with 1): running stays set in the first cycle or two after
// 010F, with 6041 bit 12 already clear, and the HALT completes only in the cycle that reads it clear.
void stopFromHeldTrigger() {
    scenario = "stop from held trigger";
    for (unsigned haltReads = 0; haltReads < 2; ++haltReads) {
        Rig r;
        r.drive.travelReads = 1000000;
        r.drive.haltReads = haltReads;
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
        CHECK(s.observations == haltReads + 2 && !(s.monitorObservation.monitor & L::MONITOR_RUNNING));
        CHECK(!(s.statusObservation.statusword & ACCEPT_BIT) && !(s.monitorObservation.monitor & L::MONITOR_POSITION_COMPLETE));
        CHECK(wordsFrom(r, at, 0x010f) == 1 && !wordsFrom(r, at, 0x000f) && !wordsFrom(r, at, 0x000b));
        CHECK(moveOf(r).interruption == M::MoveInterruption::STOP && r.a.moveFenced());
    }
}

// A quick stop during a move completes through the standstill rule on the captured 000B report:
// released with 6041 low bits 000 and running still latched (2100 0014, 6041 4050), then two cycles
// with FEEDBACK whose 606C reads 0 in both and whose 6064 did not change. A running bit that clears is
// not this path: the fake keeps it latched, as every capture did.
void quickStopStandstill(const L::ActionContext& s, const FakeDrive& d) {
    CHECK(s.state == M::ActionState::SUCCEEDED && s.outcome == M::ActionOutcome::OBSERVED);
    CHECK(s.standstillObserving && s.monitorObservation.monitor == 0x0014 &&
          s.statusObservation.statusword == 0x4050);
    CHECK((s.monitorObservation.monitor & L::MONITOR_RUNNING) && d.velocity == 0);
    CHECK(s.extraObservation.fieldCount == 2 && s.extraObservation.actualVelocity == 0 &&
          s.extraObservation.actualPosition == d.position);
    CHECK(s.observations >= 3); // The release, then two cycles with FEEDBACK.
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
    quickStopStandstill(r.two.a.stopAction(L::StopKind::QUICK_STOP), r.two.drive);
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

// 11b. One node's urgent stop while the other node's parameter or trigger download is queued in the
// owner, held in the adapter or already handed off, both ways round. The stop retires only its own
// axis's work and waits only for its own in-flight poll. The other move keeps every write exactly once,
// in order, and completes, or, when the adapter reports its held frame unsent, fails on that frame
// alone with nothing after it. Neither node receives the other's words, heartbeats keep arriving, and
// nothing is replayed after either terminal.
enum class Pending { PARAMETER, TRIGGER };
enum class Held { QUEUED, ADAPTER, ADAPTER_FAILED, HANDED_OFF };
const char* const PENDINGS[] = {"6081", "trigger"};
const char* const HELDS[] = {"queued", "in the adapter", "in the adapter, then failed", "handed off"};
// The other node's download admitted, noted and queued in the owner, not yet started.
bool pendingAt(const Rig& r, unsigned node, Pending p) {
    const L::MoveContext& c = moveOf(r, node);
    return c.state == M::ActionState::ACTIVE && c.transmissionNoted &&
           (p == Pending::TRIGGER ? c.phase == L::MovePhase::TRIGGER
                                  : c.phase == L::MovePhase::PARAMETERS && c.setupCursor == 1);
}
bool isDownloadFrame(const Rig& r, unsigned i, unsigned node) {
    return r.wire.sent[i].id == 0x600 + node && r.wire.sent[i].data[0] != 0x40;
}
void urgentBesideOtherWork(unsigned stopping, L::StopKind kind, Pending pending, Held held) {
    static char name[96];
    std::snprintf(name, sizeof name, "node %u %s beside node %u %s %s", stopping,
                  kind == L::StopKind::HALT ? "halt" : "quick stop", 3 - stopping,
                  PENDINGS[unsigned(pending)], HELDS[unsigned(held)]);
    scenario = name;
    const unsigned other = 3 - stopping;
    Rig r(true);
    Axis& s = r.axis(stopping);
    Axis& o = r.axis(other);
    s.drive.travelReads = 1000000;
    o.drive.travelReads = 1;
    r.ready(1);
    r.ready(2);
    const unsigned start = r.wire.sentCount;
    CHECK(r.relative(witness(r, 0, M::RelativeBasis::ACTUAL, stopping), relEvidence(), relRequest(5000),
                     stopping)
              .isOk());
    for (unsigned k = 0; k < 600 && moveOf(r, stopping).observations < 1; ++k)
        r.step();
    CHECK(moveOf(r, stopping).observations >= 1);
    // A fresh admission read: the one from ready() has aged while the stopping move ran.
    r.readState(o.state, STATE_MASK, other);
    const unsigned otherStart = r.wire.sentCount;
    CHECK(r.relative(witness(r, 0, M::RelativeBasis::ACTUAL, other), relEvidence(), relRequest(-3000), other)
              .isOk());
    for (unsigned k = 0; k < 600 && o.a.moveActive() && !pendingAt(r, other, pending); ++k)
        r.step();
    CHECK(pendingAt(r, other, pending) && s.a.moveActive());
    const uint16_t pendingIndex = pending == Pending::TRIGGER ? 0x6040 : 0x6081;
    const unsigned queuedAt = r.wire.sentCount;
    unsigned handed = r.wire.sentCount; // The other node's pending download, once on the wire.
    if (held != Held::QUEUED) {
        if (held == Held::ADAPTER || held == Held::ADAPTER_FAILED)
            r.holdAdapter();
        r.serviceSession = r.serviceTwo = false;
        r.step(); // An owner turn alone: it hands the download to the controller.
        r.serviceSession = r.serviceTwo = true;
        for (unsigned i = queuedAt; i < r.wire.sentCount && handed == r.wire.sentCount; ++i)
            if (isDownloadFrame(r, i, other))
                handed = i;
        CHECK(handed < r.wire.sentCount && frameIndex(r.wire.sent[handed]) == pendingIndex);
        CHECK(r.wire.pending[handed] == (held != Held::HANDED_OFF));
    } else {
        CHECK(!downloadsFrom(r, otherStart, pendingIndex, other)); // Not yet on the wire.
    }
    const uint32_t otherOperation = moveOf(r, other).operation;
    const uint32_t stoppingMove = moveOf(r, stopping).operation;
    const unsigned at = r.wire.sentCount;
    const E::StopPolicy policy = stopPolicy(r);
    CHECK(s.a.requestStop(kind, s.a.moveObservation(), policy, r.now) == E::StopPath::CONFIRMED);
    // The stop retired nothing of the other axis: its download is still live in the owner.
    CHECK(o.a.moveActive() && o.a.busy() && moveOf(r, other).withdrawal == M::MoveInterruption::NONE);
    const bool failed = held == Held::ADAPTER_FAILED;
    if (held == Held::ADAPTER || failed) {
        r.step(); // Within the frames' response budgets: the adapter then reports them in order.
        // Still off the bus: the other drive has not seen it.
        CHECK(r.wire.pending[handed] && (pending == Pending::TRIGGER ? !o.drive.edges : o.drive.speed != 120));
        r.releaseAdapter(handed, !failed);
    }
    finishAll(r);

    // The stopping node: its own urgent operation, from the held 005F, with no 000F before it.
    const L::ActionContext& stop = s.a.stopAction(kind);
    const uint16_t stopWord = kind == L::StopKind::HALT ? 0x010f : 0x000b;
    CHECK(stop.operation > stoppingMove && stop.deadlineUs == policy.deadlineUs);
    CHECK(stop.predecessorWord == 0x005f && !stop.predecessorDocumented);
    if (kind == L::StopKind::HALT)
        CHECK(stop.state == M::ActionState::SUCCEEDED && !(stop.monitorObservation.monitor & L::MONITOR_RUNNING));
    else
        quickStopStandstill(stop, s.drive);
    const L::MoveContext& stopped = moveOf(r, stopping);
    CHECK(stopped.state == M::ActionState::FAILED && stopped.outcome == M::ActionOutcome::CANCELLED);
    CHECK(stopped.interruption == M::MoveInterruption::STOP && stopped.execution == M::ActionExecution::ACKNOWLEDGED);
    CHECK(stopped.uncertain && s.a.moveFenced());
    CHECK(wordsFrom(r, start, 0x005f, stopping) == 1 && s.drive.edges == 1);
    CHECK(wordsFrom(r, at, stopWord, stopping) == 1);
    CHECK(wordsFrom(r, start, 0x010f, stopping) + wordsFrom(r, start, 0x000b, stopping) == 1);
    CHECK(!wordsFrom(r, start, 0x000f, stopping));
    for (unsigned i = at; i < r.wire.sentCount; ++i)
        if (isDownloadFrame(r, i, stopping))
            CHECK(wordAt(r, i, stopWord, stopping)); // Nothing of the move after the stop.
    onlyRelativeWords(r, start, stopping);

    // The other node: every write once, in order, and its own completion and rearm.
    const L::MoveContext& c = moveOf(r, other);
    CHECK(c.operation == otherOperation && c.interruption == M::MoveInterruption::NONE);
    unsigned writes[8], count = 0;
    for (unsigned i = otherStart; i < r.wire.sentCount; ++i)
        if (isDownloadFrame(r, i, other)) {
            CHECK(count < 8);
            writes[count++] = i;
        }
    const uint32_t expected[6] = {uint32_t(-3000), 120, 100, 200, 0x005f, 0x000f};
    const uint16_t objects[6] = {0x607a, 0x6081, 0x6083, 0x6084, 0x6040, 0x6040};
    const unsigned planned = failed ? (pending == Pending::TRIGGER ? 5u : 2u) : 6u;
    CHECK(count == planned);
    for (unsigned k = 0; k < count && k < 6; ++k)
        CHECK(downloadAt(r, writes[k], objects[k], k < 4 ? 4 : 2, expected[k], other));
    if (held != Held::QUEUED)
        CHECK(writes[pending == Pending::TRIGGER ? 4 : 1] == handed);
    CHECK(!wordsFrom(r, start, 0x010f, other) && !wordsFrom(r, start, 0x000b, other));
    onlyRelativeWords(r, otherStart, other);
    if (!failed) {
        CHECK(c.state == M::ActionState::SUCCEEDED && c.outcome == M::ActionOutcome::OBSERVED);
        CHECK(c.setup == M::ActionExecution::ACKNOWLEDGED && c.execution == M::ActionExecution::ACKNOWLEDGED);
        CHECK(c.rearm == M::ActionExecution::ACKNOWLEDGED && !c.uncertain && !o.a.moveFenced());
        CHECK(o.drive.edges == 1 && o.drive.position == -3000 && c.completed.present);
    } else {
        // Never on the bus, yet not proven unsent: its effect stays possible, and nothing follows it.
        CHECK(c.state == M::ActionState::FAILED && c.outcome == M::ActionOutcome::TRANSPORT_ERROR);
        CHECK(c.uncertain && c.effectsPossible && !o.drive.edges);
        if (pending == Pending::TRIGGER)
            CHECK(c.setup == M::ActionExecution::ACKNOWLEDGED && c.execution == M::ActionExecution::UNKNOWN &&
                  c.trigger.transmissionPossible && o.a.moveFenced());
        else
            CHECK(c.setup == M::ActionExecution::UNKNOWN && c.execution == M::ActionExecution::NOT_TRANSMITTED &&
                  !c.triggerPresent && c.setupHistory[1].transmissionPossible && !o.a.moveFenced() &&
                  o.drive.speed != 120);
    }

    // Passive traffic was never consumed or blocked by either service.
    if (asyncTraffic)
        for (unsigned node = 0; node < 2; ++node)
            CHECK(r.services.node(uint8_t(node))->heartbeat.present &&
                  r.services.node(uint8_t(node))->heartbeat.processedUs == r.now);
    CHECK(!r.owner.stats().rxDropped && !r.owner.stats().unmatchedFrames);

    // Both retained outcomes stay, and nothing follows either.
    const Saved<L::MoveContext> one(moveOf(r, 1)), two(moveOf(r, 2));
    const Saved<L::ActionContext> kept(stop);
    quiet(r, 20);
    one.unchanged(moveOf(r, 1));
    two.unchanged(moveOf(r, 2));
    kept.unchanged(stop);
}
void urgentBesideOtherNode() {
    const Pending pendings[] = {Pending::PARAMETER, Pending::TRIGGER};
    const Held helds[] = {Held::QUEUED, Held::ADAPTER, Held::ADAPTER_FAILED, Held::HANDED_OFF};
    const L::StopKind kinds[] = {L::StopKind::HALT, L::StopKind::QUICK_STOP};
    for (unsigned stopping = 1; stopping <= 2; ++stopping)
        for (L::StopKind kind : kinds)
            for (Pending pending : pendings)
                for (Held held : helds)
                    urgentBesideOtherWork(stopping, kind, pending, held);
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
    // validateMoveAdmission is that recheck alone: each refusal has the notice's status and detail,
    // and nothing changes.
    struct Recheck {
        const M::AxisConfig* axis;
        const M::AxisReference* witness;
        uint64_t at;
    };
    M::AxisReference replaced = w, busy = w;
    ++replaced.observedUs;
    busy.idle = false;
    const Recheck rechecks[] = {{&axis, &replaced, r.now},
                                {&axis, nullptr, r.now},
                                {&axis, &busy, r.now},
                                {&moved, &w, r.now},
                                {&axis, &w, r.now + o.maximumAgeUs + 1},
                                {&c.parameters.axis, &w, r.now},
                                {&axis, &c.parameters.target.retainedReference, r.now}};
    for (const Recheck& k : rechecks) {
        const M::Status noted = L::noteMoveTransmission(c, live, here, *k.axis, k.witness, nullptr, 0, k.at);
        const M::Status checked = L::validateMoveAdmission(c, *k.axis, k.witness, k.at);
        CHECK(!noted && !checked && checked.code == noted.code && checked.detail == noted.detail);
    }
    active.unchanged(c);
    CHECK(L::validateMoveAdmission(c, axis, &w, r.now));
    active.unchanged(c);
    CHECK(L::noteMoveTransmission(c, live, here, axis, &w, nullptr, 0, r.now));
    // After the notice, as at the handover, the recheck still runs and still refuses a change.
    CHECK(L::validateMoveAdmission(c, axis, &w, r.now));
    CHECK(L::validateMoveAdmission(c, axis, &replaced, r.now).detail == AXIS + int32_t(M::AxisError::STALE_REFERENCE));
    static L::MoveContext none;
    CHECK(L::validateMoveAdmission(none, axis, &w, r.now).detail == int32_t(M::MoveError::STATE));
    C::SdoCompletion ack = acknowledged(work.transfer, r.now, here.nodeGeneration);
    CHECK(L::advanceMove(c, observation, live, here, ack, r.now));
    CHECK(c.setupCursor == 1 && c.setup == M::ActionExecution::UNKNOWN);
    CHECK(L::advanceMove(c, observation, live, here, M::MoveInterruption::CANCELLED, r.now));
    CHECK(c.state == M::ActionState::FAILED && c.setupHistory[0].outcome == C::SdoOutcome::SUCCESS);
    CHECK(L::validateMoveAdmission(c, axis, &w, r.now).detail == int32_t(M::MoveError::STATE));
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

// 25. A fault reported through the move's own observation cycle, not a local fault cancel. The fake's
// alarm and its 6041 are manufactured; no capture of an alarm during a move exists. The cycle is
// sequential, not atomic: a report counts in the field it was read in. The scope check precedes the
// acceptance and completion rule, so a faulted cycle records neither, even when it carries both. The
// trigger may have moved the drive and no completion was observed: the move is uncertain and fenced,
// the held 005F stays the latest word, and no 000F follows it.
enum class Fault { BEFORE_ACCEPTANCE, AFTER_ACCEPTANCE, SAME_CYCLE, UNFINISHED_CYCLE };
const char* const FAULTS[] = {"before acceptance", "after acceptance", "mid-cycle, same cycle",
                              "mid-cycle, unfinished"};
void faultInCycle(Fault fault) {
    static char name[64];
    std::snprintf(name, sizeof name, "cycle fault %s", FAULTS[unsigned(fault)]);
    scenario = name;
    Rig r;
    r.drive.travelReads = fault == Fault::BEFORE_ACCEPTANCE ? 1000000 : 1; // Else cycle 2 completes.
    r.ready();
    if (fault == Fault::SAME_CYCLE) {
        // Raised by cycle 2's 6041 read, after its 2100 read was clean, and shown in that 6041.
        r.drive.alarmAtRead = r.drive.statusReads + 2;
        r.drive.alarmDisables = true;
    }
    CHECK(r.relative(witness(r, 0), relEvidence(), relRequest(800)).isOk());
    const unsigned start = r.startFrame;
    const L::MoveContext& c = moveOf(r);
    switch (fault) {
    case Fault::BEFORE_ACCEPTANCE:
        // Acknowledged; cycle 1's 2100 upload is admitted but not yet answered.
        for (unsigned k = 0; k < 400 && c.execution != M::ActionExecution::ACKNOWLEDGED; ++k)
            r.step();
        CHECK(c.execution == M::ActionExecution::ACKNOWLEDGED && !uploadsFrom(r, start, 0x2100));
        r.drive.monitor = uint16_t(r.drive.monitor | L::MONITOR_ALARM);
        break;
    case Fault::AFTER_ACCEPTANCE:
        // Between cycles 1 and 2: cycle 1 has accepted.
        for (unsigned k = 0; k < 400 && !(c.observations == 1 && c.phase == L::MovePhase::WAITING); ++k)
            r.step();
        CHECK(c.observations == 1 && c.acceptance.present && c.phase == L::MovePhase::WAITING);
        r.drive.monitor = uint16_t(r.drive.monitor | L::MONITOR_ALARM);
        break;
    case Fault::UNFINISHED_CYCLE:
        // Cycle 2's 2100 upload on the wire, unanswered: it reads the alarm, and the faulted drive
        // then aborts 2101, so cycle 2 never completes.
        for (unsigned k = 0; k < 400 && !(uploadsFrom(r, start, 0x2100) == 2 && r.served < r.wire.sentCount); ++k)
            r.step();
        CHECK(c.observations == 1 && uploadAt(r, r.wire.sentCount - 1, 0x2100));
        r.drive.monitor = uint16_t(r.drive.monitor | L::MONITOR_ALARM);
        r.drive.abortIndex = 0x2101;
        break;
    default:
        break;
    }
    finishAll(r);
    CHECK(c.state == M::ActionState::FAILED && c.interruption == M::MoveInterruption::NONE);
    CHECK(c.execution == M::ActionExecution::ACKNOWLEDGED && c.completion == M::ActionCompletion::NOT_OBSERVED);
    CHECK(!c.completed.present && c.uncertain && r.a.moveFenced());
    switch (fault) {
    case Fault::BEFORE_ACCEPTANCE:
        // Cycle 1 carried the acceptance report too; the alarm in the same cycle wins.
        CHECK(c.outcome == M::ActionOutcome::OBSERVATION_SCOPE && c.observations == 1);
        CHECK((c.latest.monitor & L::MONITOR_ALARM) && (c.latest.statusword & ACCEPT_BIT));
        CHECK(!c.acceptance.present && !c.absence.present);
        break;
    case Fault::AFTER_ACCEPTANCE:
        // Cycle 2 reports the alarm and completion together; completion does not count.
        CHECK(c.outcome == M::ActionOutcome::OBSERVATION_SCOPE && c.observations == 2);
        CHECK(c.acceptance.cycle == 1 && c.absence.cycle == 1 && c.latest.cycle == 2);
        CHECK((c.latest.monitor & L::MONITOR_ALARM) && (c.latest.statusword & COMPLETE_BIT));
        break;
    case Fault::SAME_CYCLE:
        // Cycle 2's clean 2100 was read before the fault, its 6041 after: that 6041 ends the cycle,
        // though it also reports completion.
        CHECK(c.outcome == M::ActionOutcome::OBSERVATION_SCOPE && c.observations == 2);
        CHECK(c.acceptance.cycle == 1 && c.latest.cycle == 2 && !(c.latest.monitor & L::MONITOR_ALARM));
        CHECK(!(c.latest.statusword & L::STATUS_LOW_BITS) && (c.latest.statusword & COMPLETE_BIT));
        break;
    case Fault::UNFINISHED_CYCLE:
        // The read failed on its third field, so the alarm read in its first never became a cycle.
        // The last complete cycle, which a stop may use, is still cycle 1.
        CHECK(c.outcome == M::ActionOutcome::PROTOCOL_REJECTED && c.status.code == M::Err::PROTOCOL_ERROR);
        CHECK(c.observations == 1 && c.latest.cycle == 1 && !(c.latest.monitor & L::MONITOR_ALARM));
        CHECK(c.observationEvidencePresent && c.observationEvidence.transfer.index == 0x2101 &&
              c.observationEvidence.abortValid);
        CHECK(r.a.moveObservation().state == M::ReadState::SUCCEEDED &&
              r.a.moveObservation().startedUs < c.observationStartedUs);
        CHECK(uploadsFrom(r, start, 0x2101) == 2 && uploadsFrom(r, start, 0x2102) == 1);
        break;
    }
    // No trailing 000F, no stop and no second edge: only the trigger and the cycles' uploads.
    CHECK(wordsFrom(r, start, 0x005f) == 1 && r.drive.edges == 1 && !wordsFrom(r, start, 0x000f));
    CHECK(!wordsFrom(r, start, 0x010f) && !wordsFrom(r, start, 0x000b));
    CHECK(r.a.latestControl() && r.a.latestControl()->transfer.data[0] == 0x5f);
    for (unsigned i = start + 5; i < r.wire.sentCount; ++i)
        CHECK(r.wire.sent[i].data[0] == 0x40);
    onlyRelativeWords(r, start);
    const Saved<L::MoveContext> ended(c);
    quiet(r);
    ended.unchanged(c);
    // The admission fence: even a fresh read and a fresh witness admit no new move.
    r.drive.abortIndex = 0;
    r.readState(r.state);
    const M::Status again = r.relative(witness(r, 800), relEvidence(), relRequest(800));
    CHECK(again.msg && std::strstr(again.msg, "fenced"));
    refused(r, again, M::Err::INVALID_CONFIG, M::MoveError::STATE);
    ended.unchanged(c);
}
void cycleFaults() {
    const Fault faults[] = {Fault::BEFORE_ACCEPTANCE, Fault::AFTER_ACCEPTANCE, Fault::SAME_CYCLE,
                            Fault::UNFINISHED_CYCLE};
    for (Fault fault : faults)
        faultInCycle(fault);
}

// 26. A drive fault reported by EMCY while a move's download waits. REARM has no read cycle, and a
// parameter or trigger write has none either, so the EMCY is the report. It reaches the owner's one
// receive path in an ordinary owner turn, which receives before it transmits: the application's
// transmit guard passes its unhandled-fault fact to preflight, so the queued download is refused there,
// provably unsent, and the move ends DRIVE_FAULT. No word trails the EMCY. A frame already handed over
// is not recalled: the application's own handling, after its sessions' turn, withdraws the move and
// the frame's terminal settles as evidence, acknowledged or lost. Every case fences the session
// (faultBlocked): a clean fresh read later still admits no move. Without the guard's fact, the contrast,
// the queued 000F leaves in the very turn that received the EMCY. The EMCY code is manufactured.
enum class EmcyAt { PARAMETER, TRIGGER, REARM, REARM_HANDED_OFF, REARM_LOST, REARM_UNGUARDED };
const char* const EMCYS[] = {"6081 queued", "trigger queued", "rearm queued", "rearm handed off",
                             "rearm handed off, lost", "rearm queued, no fault fact"};
void emcyBeforeHandover(EmcyAt at) {
    static char name[64];
    std::snprintf(name, sizeof name, "emcy, %s", EMCYS[unsigned(at)]);
    scenario = name;
    const bool rearm = at >= EmcyAt::REARM, handedOff = at == EmcyAt::REARM_HANDED_OFF || at == EmcyAt::REARM_LOST;
    const bool unguarded = at == EmcyAt::REARM_UNGUARDED;
    Rig r;
    r.drive.travelReads = 1;
    r.ready();
    if (at == EmcyAt::REARM_LOST) { // The rearm's reply is lost; ready()'s own 000F was answered.
        r.drive.silentIndex = 0x6040;
        r.drive.silentWord = 0x000f;
    }
    r.wire.faultHook = !unguarded;
    CHECK(r.relative(witness(r, 0), relEvidence(), relRequest(600)).isOk());
    const unsigned start = r.startFrame;
    const L::MoveContext& c = moveOf(r);
    const Point point = at == EmcyAt::PARAMETER ? Point::PARAM1 : at == EmcyAt::TRIGGER ? Point::POSSIBLE_TRIGGER
                                                                                      : Point::REARM;
    for (unsigned k = 0; k < 400 && r.a.moveActive() && !reached(r, point); ++k)
        r.step();
    CHECK(reached(r, point)); // Admitted and noted; still queued in the owner.
    const uint16_t queuedIndex = at == EmcyAt::PARAMETER ? 0x6081 : 0x6040;
    const unsigned queuedAt = r.wire.sentCount;
    unsigned held = r.wire.sentCount;
    if (handedOff) {
        r.holdAdapter();
        r.serviceSession = false;
        r.step(); // The owner hands the 000F over; it waits in the adapter, unanswered.
        r.serviceSession = true;
        held = r.wire.sentCount - 1;
        CHECK(wordAt(r, held, 0x000f) && r.wire.pending[held]);
    }
    const unsigned beforeFault = r.wire.sentCount;
    const unsigned refusals = r.wire.preflightRefusals;
    const uint32_t emcys = r.services.node(0)->emcyCount;
    r.drive.monitor = uint16_t(r.drive.monitor | L::MONITOR_ALARM); // The fake's own report of it.
    r.later(emcyFrame(1, 0xff05, 0x5a));
    r.step(); // An ordinary turn: it receives the EMCY, then would transmit.
    const uint64_t emcyTurn = r.now;
    CHECK(r.services.node(0)->emcyCount == emcys + 1 && r.faultsHandled == 1);
    CHECK(r.handled[0].kind == C::NetworkEventKind::EMCY && r.handled[0].errorCode == 0xff05 &&
          r.handled[0].manufacturer[4] == 0x5a); // The application's own copy.
    if (handedOff)
        r.releaseAdapter(held, true);
    finishAll(r);
    CHECK(c.state == M::ActionState::FAILED && c.outcome == M::ActionOutcome::DRIVE_FAULT);
    CHECK(c.interruption == M::MoveInterruption::DRIVE_FAULT && r.a.faultBlocked());
    CHECK(!wordsFrom(r, start, 0x010f) && !wordsFrom(r, start, 0x000b));
    if (rearm) {
        // The completion observed before the fault stays.
        CHECK(c.completion == M::ActionCompletion::OBSERVED && c.completed.present && c.rearmPresent);
        CHECK(c.execution == M::ActionExecution::ACKNOWLEDGED && !r.a.moveFenced());
        CHECK(wordsFrom(r, start, 0x005f) == 1 && r.drive.edges == 1);
    }
    switch (at) {
    case EmcyAt::PARAMETER:
    case EmcyAt::TRIGGER:
    case EmcyAt::REARM:
        // Refused at the handover in the EMCY's own turn: never on the wire, closed provably unsent.
        CHECK(r.wire.preflightRefusals == refusals + 1 && r.wire.sentCount >= queuedAt);
        CHECK(!downloadsFrom(r, queuedAt, queuedIndex) && !downloadsFrom(r, beforeFault, queuedIndex));
        if (at == EmcyAt::PARAMETER) {
            CHECK(c.setup == M::ActionExecution::UNKNOWN && c.setupEvidenceMask == 3 &&
                  !c.setupHistory[1].transmissionPossible && r.drive.speed != 120);
            CHECK(c.execution == M::ActionExecution::NOT_TRANSMITTED && !c.triggerPresent);
        } else if (at == EmcyAt::TRIGGER) {
            CHECK(c.setup == M::ActionExecution::ACKNOWLEDGED && c.execution == M::ActionExecution::NOT_TRANSMITTED);
            CHECK(c.triggerPresent && !c.trigger.transmissionPossible && !r.drive.edges);
            // The acknowledged parameter writes already withdrew the 000F, and the unsent trigger
            // restores nothing: no latest word remains.
            CHECK(!r.a.latestControl());
        } else {
            CHECK(c.rearm == M::ActionExecution::NOT_TRANSMITTED && !c.rearmEvidence.transmissionPossible);
            CHECK(r.a.latestControl() && r.a.latestControl()->transfer.data[0] == 0x5f);
        }
        CHECK(!wordsFrom(r, start, 0x000f) && !r.a.moveFenced() && c.effectsPossible);
        break;
    case EmcyAt::REARM_HANDED_OFF:
        CHECK(wordsFrom(r, start, 0x000f) == 1 && held < beforeFault && c.rearm == M::ActionExecution::ACKNOWLEDGED);
        CHECK(c.rearmEvidence.outcome == C::SdoOutcome::SUCCESS && c.rearmEvidence.transmissionPossible);
        CHECK(!c.uncertain && r.a.latestControl() && r.a.latestControl()->transfer.data[0] == 0x0f);
        break;
    case EmcyAt::REARM_LOST:
        CHECK(wordsFrom(r, start, 0x000f) == 1 && held < beforeFault && c.rearm == M::ActionExecution::UNKNOWN);
        CHECK(c.rearmEvidence.transmissionPossible && c.rearmEvidence.effectUncertain && c.uncertain);
        CHECK(!r.a.latestControl() && r.services.unresolved(0));
        break;
    case EmcyAt::REARM_UNGUARDED:
        // The defect the guard closes: the 000F left in the EMCY's own turn, after its receipt.
        CHECK(r.wire.preflightRefusals == refusals && wordsFrom(r, beforeFault, 0x000f) == 1);
        for (unsigned i = beforeFault; i < r.wire.sentCount; ++i)
            if (wordAt(r, i, 0x000f))
                CHECK(r.sentAt[i] == emcyTurn);
        CHECK(c.rearm == M::ActionExecution::ACKNOWLEDGED);
        break;
    }
    if (at != EmcyAt::REARM_UNGUARDED)
        CHECK(!wordsFrom(r, beforeFault, 0x000f)); // Nothing trails the fault.
    // The owner's store still holds that EMCY, and no later frame replaced it.
    CHECK(r.services.node(0)->emcyCount == emcys + 1 && r.services.node(0)->emcy.manufacturer[4] == 0x5a);
    const Saved<L::MoveContext> ended(c);
    quiet(r);
    // The fence: the admission read from before the fault, and then a clean fresh read after the alarm
    // has gone, admit no new move.
    const M::Status stale = r.relative(witness(r, 600), relEvidence(), relRequest(600));
    if (at == EmcyAt::REARM_LOST) {
        refused(r, stale, M::Err::INVALID_CONFIG, M::MoveError::TARGET); // No usable reservation.
    } else {
        CHECK(stale.msg && std::strstr(stale.msg, "drive fault"));
        refused(r, stale, M::Err::INVALID_CONFIG, M::MoveError::STATE);
        r.drive.monitor = uint16_t(r.drive.monitor & ~L::MONITOR_ALARM);
        r.readState(r.state);
        const M::Status clean = r.relative(witness(r, 600), relEvidence(), relRequest(600));
        CHECK(clean.msg && std::strstr(clean.msg, "drive fault"));
        refused(r, clean, M::Err::INVALID_CONFIG, M::MoveError::STATE);
    }
    CHECK(wordsFrom(r, start, 0x000f) == (handedOff || unguarded ? 1u : 0u) && r.drive.edges == (rearm ? 1u : 0u));
    ended.unchanged(c);
}
void emcyDuringMove() {
    const EmcyAt cases[] = {EmcyAt::PARAMETER, EmcyAt::TRIGGER, EmcyAt::REARM, EmcyAt::REARM_HANDED_OFF,
                            EmcyAt::REARM_LOST, EmcyAt::REARM_UNGUARDED};
    for (EmcyAt at : cases)
        emcyBeforeHandover(at);
}

// 27. The fault fence (faultBlocked). Once a move or an enable is withdrawn for a reported drive fault,
// this session object admits no move, enable or counter clear, across result release, close and open,
// even after a clean fresh read that would otherwise admit one: today's 000F and an alarm-free 2100.
// A stop and an explicit alarm clear stay admissible, and the clear's success does not lift the fence.
// The move fault comes through cancelMove before any download; the enable fault through the guard's
// fact, with its first word queued when the EMCY arrives.
void expectFenced(Rig& r) {
    r.readState(r.state);
    const M::Status relative = r.relative(witness(r, 0), relEvidence(), relRequest(600));
    CHECK(relative.msg && std::strstr(relative.msg, "drive fault"));
    refused(r, relative, M::Err::INVALID_CONFIG, M::MoveError::STATE);
    const M::Status absolute = r.request();
    CHECK(absolute.msg && std::strstr(absolute.msg, "drive fault"));
    refused(r, absolute, M::Err::INVALID_CONFIG, M::MoveError::STATE);
    L::ReadContext enableState;
    r.readState(enableState, ENABLE_MASK);
    L::ActionOptions budgets;
    budgets.service = reads();
    budgets.observationIntervalUs = 1000;
    budgets.maximumAgeUs = uint64_t(QUEUE + uint64_t(RESPONSE)) * 9;
    budgets.commandIntervalUs = L::ENABLE_WORD_INTERVAL_US;
    const M::Status enable = r.a.requestEnable(enableState, L::EnableEvidence(), budgets, r.now + LEAD, r.now);
    CHECK(!enable && enable.code == M::Err::INVALID_CONFIG && enable.detail == int32_t(M::ActionError::STATE));
    CHECK(enable.msg && std::strstr(enable.msg, "drive fault"));
    L::ReadContext feedback;
    r.readState(feedback, ENABLE_MASK);
    M::AxisConfig axis = axisConfig();
    M::AxisReference reference = witness(r, 0);
    const M::Status zero = r.a.requestCounterZero(feedback, 0, axis, reference, budgets, r.now + LEAD, r.now);
    CHECK(!zero && zero.code == M::Err::INVALID_CONFIG && zero.detail == int32_t(M::ActionError::STATE));
    CHECK(zero.msg && std::strstr(zero.msg, "drive fault"));
    CHECK(!r.a.busy() && r.a.faultBlocked());
    quiet(r, 4);
}
void faultFence() {
    for (unsigned viaEnable = 0; viaEnable < 2; ++viaEnable) {
        scenario = viaEnable ? "fault fence, enable" : "fault fence, move";
        Rig r;
        const unsigned start = r.wire.sentCount;
        if (!viaEnable) {
            r.ready();
            CHECK(r.relative(witness(r, 0), relEvidence(), relRequest(600)).isOk());
            CHECK(!r.a.faultBlocked());
            CHECK(r.a.cancelMove(r.now, M::MoveInterruption::DRIVE_FAULT));
            finishAll(r);
            const L::MoveContext& c = moveOf(r);
            CHECK(c.outcome == M::ActionOutcome::DRIVE_FAULT && c.execution == M::ActionExecution::NOT_TRANSMITTED);
            CHECK(r.wire.sentCount == r.startFrame && !r.a.moveFenced());
            CHECK(r.a.latestControl() && r.a.latestControl()->transfer.data[0] == 0x0f);
        } else {
            r.drive.followEnable = true;
            r.open();
            r.readIdentity();
            L::ReadContext enableState;
            r.readState(enableState, ENABLE_MASK);
            L::EnableEvidence e;
            e.gate = L::EnableGate::ATTENDED_EXPERIMENT;
            e.trial = 1;
            e.start = L::EnableStart::NO_MOTION_WRITE_RECORDED;
            e.powerUpShaft = L::ShaftObservation::FREE;
            e.reviewed = true;
            e.reviewedStatusword = IDLE_STATUS;
            e.reviewedPositionTarget = 5000;
            e.reviewedVelocityTarget = 120;
            e.reservation = r.reservation();
            e.operation = r.a.nextOperation();
            e.expiresUs = r.now + 1000000;
            L::ActionOptions budgets;
            budgets.service = reads();
            budgets.observationIntervalUs = 1000;
            budgets.maximumAgeUs = uint64_t(QUEUE + uint64_t(RESPONSE)) * 9;
            budgets.commandIntervalUs = L::ENABLE_WORD_INTERVAL_US;
            CHECK(r.a.requestEnable(enableState, e, budgets, r.now + LEAD, r.now).isOk());
            const L::ActionContext& enable = r.a.enableAction();
            for (unsigned k = 0; k < 600 && r.a.enableActive() &&
                                 !(enable.phase == L::ActionPhase::COMMAND && enable.transmissionNoted);
                 ++k)
                r.step();
            CHECK(enable.phase == L::ActionPhase::COMMAND && enable.transmissionNoted && !r.drive.words);
            const unsigned refusals = r.wire.preflightRefusals;
            r.later(emcyFrame(1, 0xff05));
            r.step(); // Receives the EMCY, then refuses the queued word 0006.
            for (unsigned k = 0; k < 600 && r.a.enableActive(); ++k)
                r.step();
            CHECK(enable.state == M::ActionState::FAILED && enable.outcome == M::ActionOutcome::DRIVE_FAULT);
            CHECK(enable.execution == M::ActionExecution::NOT_TRANSMITTED && r.wire.preflightRefusals == refusals + 1);
            CHECK(!downloadsFrom(r, start, 0x6040) && !r.drive.words && !r.a.latestControl());
        }
        CHECK(r.a.faultBlocked() && !r.a.busy());
        expectFenced(r);
        if (!viaEnable) {
            // A stop is admitted, sequenced from the 000F that is still the latest word.
            const unsigned stopAt = r.wire.sentCount;
            r.readState(r.state);
            CHECK(r.a.requestStop(L::StopKind::HALT, r.state, stopPolicy(r), r.now) == E::StopPath::CONFIRMED);
            finishAll(r);
            CHECK(r.a.stopAction(L::StopKind::HALT).state == M::ActionState::SUCCEEDED);
            CHECK(wordsFrom(r, stopAt, 0x010f) == 1 && r.wire.sentCount > stopAt);
            CHECK(r.a.faultBlocked());
        }
        // An explicit alarm clear is admitted and succeeds; the fence stays.
        r.drive.monitor = uint16_t((r.drive.monitor | L::MONITOR_ALARM) & ~L::MONITOR_RUNNING);
        r.drive.errorCode = 0xff05;
        L::ReadContext alarm;
        r.readState(alarm, uint8_t(bit(L::StateBlock::ERRORS) | bit(L::StateBlock::ERROR_HISTORY) |
                                   bit(L::StateBlock::MONITORS) | bit(L::StateBlock::STATUS)));
        L::ActionOptions budgets;
        budgets.service = reads();
        budgets.observationIntervalUs = 1000;
        budgets.maximumAgeUs = uint64_t(QUEUE + uint64_t(RESPONSE)) * 9;
        const unsigned clearAt = r.wire.sentCount;
        CHECK(r.a.requestClearAlarm(alarm, budgets, r.now + LEAD, r.now).isOk());
        finishAll(r);
        CHECK(r.a.clearAction().state == M::ActionState::SUCCEEDED && !(r.drive.monitor & L::MONITOR_ALARM));
        CHECK(downloadsFrom(r, clearAt, 0x2301) == 2 && !r.a.clearBlocked());
        CHECK(r.a.faultBlocked());
        expectFenced(r);
        // Release, close and open change nothing.
        CHECK(r.a.close());
        r.open();
        CHECK(r.a.faultBlocked());
        expectFenced(r);
        // A stop is still admitted: reopening left no latest word, so it is the 000B branch.
        const unsigned lastStop = r.wire.sentCount;
        const E::StopPath path = r.a.requestStop(L::StopKind::HALT, r.state, stopPolicy(r), r.now);
        CHECK(path == E::StopPath::BEST_EFFORT || path == E::StopPath::HELD);
        finishAll(r);
        CHECK(wordsFrom(r, lastStop, 0x000b) == 1 && r.wire.sentCount == lastStop + 1);
        CHECK(!wordsFrom(r, start, 0x005f) && !wordsFrom(r, start, 0x001f) && !r.drive.edges);
    }
}

// Packet 8 item 5, host timelines only: what a lost trigger reply, a stop from an acknowledged 005F
// beside an observed node, and a counter zero after a move leave for later work. Every report is the
// fake's; none of this is a drive measurement.

// 28. A 005F whose reply is lost: the drive either never acted on it (silent) or acted and only its
// reply was lost (acted). The move ends UNKNOWN and fences new moves, whatever is supplied. A stop is
// still admissible from the retained admission read or the move's last complete cycle, but with no
// latest word only the best-effort 000B exists, whatever kind was asked for; with a zero best-effort
// deadline it is refused with nothing sent. Nothing is replayed: one 005F, no 000F, no 010F.
void lostTriggerCase(bool acted, L::StopKind kind, bool admissionRead) {
    static char name[96];
    std::snprintf(name, sizeof name, "lost 005F reply, %s, %s from %s", acted ? "acted" : "silent",
                  kind == L::StopKind::HALT ? "halt" : "quick stop",
                  admissionRead ? "the admission read" : "the move's last cycle");
    scenario = name;
    Rig r;
    r.drive.travelReads = 1000000;
    if (!acted) {
        r.drive.silentIndex = 0x6040;
        r.drive.silentWord = 0x005f;
    }
    r.ready();
    CHECK(r.relative(witness(r, 0), relEvidence(), relRequest(-900)).isOk());
    const unsigned start = r.startFrame;
    if (acted) {
        toNotedTrigger(r);
        r.drive.mute = true;
        r.steps(2); // The owner hands the trigger over, then the fake acts on it; its reply is lost.
        r.drive.mute = false;
        CHECK(wordsFrom(r, start, 0x005f) == 1 && r.drive.edges == 1 && r.drive.moving);
    }
    r.finishMove();
    const L::MoveContext& c = moveOf(r);
    CHECK(c.state == M::ActionState::FAILED && c.outcome == M::ActionOutcome::SERVICE_TIMEOUT);
    CHECK(c.execution == M::ActionExecution::UNKNOWN && c.trigger.transmissionPossible && c.uncertain);
    CHECK(c.completion == M::ActionCompletion::NOT_OBSERVED && !c.observations);
    CHECK(r.a.moveFenced() && !r.a.latestControl() && r.drive.edges == (acted ? 1u : 0u));
    // The next move admission is refused, with nothing sent, by two independent facts: the lost reply
    // left the route unresolved, so the session has no usable reservation, which it reports first; and
    // the fence stays for this session object's life, should the route ever resolve. A witness of the
    // drive's present position changes neither.
    const unsigned before = r.wire.sentCount;
    const M::Status again = r.relative(witness(r, r.drive.position), relEvidence(), relRequest(-900));
    CHECK(!again && again.code == M::Err::INVALID_CONFIG && again.detail == int32_t(M::MoveError::TARGET));
    CHECK(again.msg && std::strstr(again.msg, "no usable reservation"));
    CHECK(r.services.unresolved(0) && r.a.moveFenced());
    CHECK(!r.a.moveActive() && !r.a.busy() && r.wire.sentCount == before);
    const Saved<L::MoveContext> ended(c);
    const L::ReadContext& source = admissionRead ? r.state : r.a.moveObservation();
    // An application that allows no best-effort branch gets no stop at all here.
    E::StopPolicy strict = stopPolicy(r);
    strict.bestEffortDeadlineUs = 0;
    CHECK(r.a.requestStop(kind, source, strict, r.now) == E::StopPath::NONE);
    quiet(r, 4);
    const E::StopPath path = r.a.requestStop(kind, source, stopPolicy(r), r.now);
    CHECK(path == E::StopPath::BEST_EFFORT || path == E::StopPath::HELD);
    finishAll(r);
    const L::BestEffortStop& stop = r.a.bestEffort();
    CHECK(stop.outcome == M::ActionOutcome::STOP_SENT_UNCONFIRMED && stop.execution == M::ActionExecution::UNKNOWN);
    CHECK(stop.operation > c.operation);
    // Exactly one frame after the lost trigger: the 000B, never a 010F, a 000F or a second 005F.
    CHECK(r.wire.sentCount == before + 1 && wordAt(r, before, 0x000b));
    CHECK(wordsFrom(r, start, 0x005f) == 1 && !wordsFrom(r, start, 0x010f) && !wordsFrom(r, start, 0x000f));
    CHECK(r.drive.edges == (acted ? 1u : 0u));
    if (acted) // The fake took the word: running latched and released, as the drives reported after 000B.
        CHECK(!r.drive.moving && (r.drive.monitor & L::MONITOR_RELEASED));
    onlyRelativeWords(r, start);
    ended.unchanged(c); // The stop is its own operation; the move's record is not rewritten.
    // Still fenced afterwards, and nothing follows.
    CHECK(r.a.moveFenced() && !r.relative(witness(r, r.drive.position), relEvidence(), relRequest(-900)));
    quiet(r);
    ended.unchanged(c);
}
void lostTriggerThenStop() {
    const L::StopKind kinds[] = {L::StopKind::HALT, L::StopKind::QUICK_STOP};
    for (unsigned acted = 0; acted < 2; ++acted)
        for (L::StopKind kind : kinds)
            for (unsigned admissionRead = 0; admissionRead < 2; ++admissionRead)
                lostTriggerCase(acted != 0, kind, admissionRead != 0);
}

// 29. HALT and quick stop from an acknowledged 005F, each as its own finite request on node 1, while
// node 2 only receives passive state reads through its own session, as the bench observer does: one
// upload of node 2 is queued in the owner when the stop is requested, and node 2 keeps reading through
// the stop. The stop is sequenced from the held 005F with no 000F before it; the interrupted relative
// move and the stop keep separate records that later observer traffic does not change; node 2 receives
// no download, keeps its own latest word and reads stationary throughout.
const uint8_t OBSERVE = uint8_t(STATE_MASK | bit(L::StateBlock::FEEDBACK));
void stopBesideObserver(L::StopKind kind) {
    scenario = kind == L::StopKind::HALT ? "halt from 005F beside an observer" : "quick stop from 005F beside an observer";
    Rig r(true);
    r.one.drive.travelReads = 1000000;
    r.ready(1);
    r.ready(2);
    r.two.drive.position = 821; // Manufactured: a stationary observer away from zero.
    const unsigned observerStart = r.wire.sentCount;
    const unsigned observerWords = r.two.drive.words;
    const C::SdoCompletion observerControl = *r.two.a.latestControl();
    CHECK(r.relative(witness(r, 0, M::RelativeBasis::ACTUAL, 1), relEvidence(), relRequest(-4000), 1).isOk());
    const unsigned start = r.startFrame;
    // The observer reads while the move runs, until the move has a complete cycle after its 005F.
    unsigned observed = 0;
    for (unsigned k = 0; k < 40 && moveOf(r, 1).observations < 1; ++k, ++observed)
        r.readState(r.two.state, OBSERVE, 2);
    CHECK(moveOf(r, 1).observations >= 1 && observed >= 1);
    // One observer upload admitted and still queued in the owner when the stop is requested.
    L::ReadContext& watch = r.two.state;
    const uint64_t watchDeadline = r.now + LEAD;
    const uint32_t watchOperation = r.two.a.beginOperation(watchDeadline, r.now);
    CHECK(watchOperation);
    CHECK(L::prepareState(watch, r.current(2), watchOperation, r.now, watchDeadline, reads(), OBSERVE));
    L::PreparedRead work;
    CHECK(L::nextRead(watch, r.now, work) && work.ready);
    CHECK(r.two.a.admitPrepared(work.transfer, r.now) == O::BusAdmission::ACCEPTED);
    const unsigned at = r.wire.sentCount;
    const uint32_t moveOperation = moveOf(r, 1).operation;
    const E::StopPolicy policy = stopPolicy(r);
    CHECK(r.one.a.requestStop(kind, r.one.a.moveObservation(), policy, r.now) == E::StopPath::CONFIRMED);
    CHECK(r.wire.sentCount == at); // The observer's upload was still queued, unsent, at the request.
    // The stop retired nothing of node 2: its queued upload completes, and so does the rest of that read.
    while (watch.state == M::ReadState::ACTIVE) {
        C::SdoCompletion terminal;
        for (unsigned k = 0; k < 40 && !r.two.a.settle(terminal); ++k)
            r.step();
        CHECK(terminal.outcome == C::SdoOutcome::SUCCESS && terminal.transfer.operation == watchOperation);
        CHECK(L::advanceRead(watch, terminal, r.now));
        if (watch.state != M::ReadState::ACTIVE)
            break;
        CHECK(L::nextRead(watch, r.now, work) && work.ready);
        CHECK(r.two.a.admitPrepared(work.transfer, r.now) == O::BusAdmission::ACCEPTED);
    }
    CHECK(watch.state == M::ReadState::SUCCEEDED);
    // Node 2 keeps reading while the stop runs, then the stop and both sessions finish.
    for (unsigned k = 0; k < 40 && r.one.a.stopActive(); ++k)
        r.readState(watch, OBSERVE, 2);
    finishAll(r);

    const uint16_t stopWord = kind == L::StopKind::HALT ? 0x010f : 0x000b;
    const L::ActionContext& stop = r.one.a.stopAction(kind);
    CHECK(stop.operation > moveOperation && stop.deadlineUs == policy.deadlineUs);
    CHECK(stop.predecessorWord == 0x005f && !stop.predecessorDocumented &&
          stop.predecessorBasis == L::PredecessorBasis::PROJECT_DECISION);
    if (kind == L::StopKind::HALT)
        CHECK(stop.state == M::ActionState::SUCCEEDED && !(stop.monitorObservation.monitor & L::MONITOR_RUNNING));
    else
        quickStopStandstill(stop, r.one.drive);
    const L::MoveContext& move = moveOf(r, 1);
    CHECK(move.state == M::ActionState::FAILED && move.outcome == M::ActionOutcome::CANCELLED);
    CHECK(move.interruption == M::MoveInterruption::STOP && move.execution == M::ActionExecution::ACKNOWLEDGED);
    CHECK(move.uncertain && r.one.a.moveFenced() && move.operation == moveOperation);
    // Node 1's words: one 005F, then the one stop word, with nothing of the move after it and no 000F.
    CHECK(wordsFrom(r, start, 0x005f, 1) == 1 && r.one.drive.edges == 1 && !wordsFrom(r, start, 0x000f, 1));
    CHECK(wordsFrom(r, at, stopWord, 1) == 1 && wordsFrom(r, start, 0x010f, 1) + wordsFrom(r, start, 0x000b, 1) == 1);
    unsigned stopFrame = r.wire.sentCount, lastStop = 0;
    for (unsigned i = at; i < r.wire.sentCount; ++i) {
        if (r.wire.sent[i].id != 0x601)
            continue;
        if (isDownloadFrame(r, i, 1))
            CHECK(wordAt(r, i, stopWord, 1));
        if (stopFrame == r.wire.sentCount && wordAt(r, i, stopWord, 1))
            stopFrame = i;
        lastStop = i;
    }
    onlyRelativeWords(r, start, 1);
    // Node 2's uploads shared the bus with the stop: one lies between its word and its last read.
    unsigned between = 0;
    for (unsigned i = stopFrame + 1; i < lastStop; ++i)
        between += r.wire.sent[i].id == 0x602;
    CHECK(stopFrame < lastStop && between >= 1);
    // Node 2: no download since its readiness, its own latest word, no move, and stationary reads.
    for (unsigned i = observerStart; i < r.wire.sentCount; ++i)
        CHECK(!isDownloadFrame(r, i, 2));
    CHECK(r.two.drive.words == observerWords && !r.two.drive.edges);
    CHECK(!r.two.a.moveActive() && !r.two.a.moveFenced() && !r.two.a.busy() && !r.two.a.stopActive());
    const C::SdoCompletion* control = r.two.a.latestControl();
    CHECK(control && control->transfer.operation == observerControl.transfer.operation &&
          control->completedUs == observerControl.completedUs && control->transfer.data[0] == 0x0f);
    L::StateObservation feedback;
    CHECK(L::getStateBlock(watch, L::StateBlock::FEEDBACK, r.current(2), r.now, UINT64_MAX, feedback));
    CHECK(feedback.actualPosition == 821 && !feedback.actualVelocity);
    if (asyncTraffic)
        for (unsigned node = 0; node < 2; ++node)
            CHECK(r.services.node(uint8_t(node))->heartbeat.present &&
                  r.services.node(uint8_t(node))->heartbeat.processedUs == r.now);
    CHECK(!r.owner.stats().rxDropped && !r.owner.stats().unmatchedFrames);
    // The move and the stop are retained independently: more observer traffic changes neither.
    const Saved<L::MoveContext> keptMove(move);
    const Saved<L::ActionContext> keptStop(stop);
    r.readState(watch, OBSERVE, 2);
    r.readState(watch, OBSERVE, 2);
    quiet(r, 20);
    keptMove.unchanged(move);
    keptStop.unchanged(stop);
}
void stopsBesideObserver() {
    stopBesideObserver(L::StopKind::HALT);
    stopBesideObserver(L::StopKind::QUICK_STOP);
}

// 30. A counter zero after a completed relative move, through AxisSession (the S2 counter-zero route's
// path). Before it, an alarm clear on the healthy drive is refused unsent even though 603F still holds
// a historic FF05. The zero writes exactly 2302 0, 1, 0, each acknowledged and kept in its own slot;
// it withdraws the axis and the borrowed witness once, publishes no origin, home or witness, rewrites
// nothing in the completed move, and leaves no latest word. Later, on the new epoch and after an
// explicit 000F, a relative move from the old witness, even re-dated, is refused; only a witness of
// the new epoch admits one, as a new explicit edge.
L::ReadTarget onSession(Rig& r) {
    L::ReadTarget value = r.current();
    value.channel.binding = r.a.binding();
    return value;
}
void readOnSession(Rig& r, L::ReadContext& into, uint8_t mask) {
    const L::ReadTarget here = onSession(r);
    const uint64_t deadline = r.now + LEAD;
    const uint32_t operation = r.a.beginOperation(deadline, r.now);
    CHECK(operation);
    CHECK(L::prepareState(into, here, operation, r.now, deadline, reads(), mask));
    while (into.state == M::ReadState::ACTIVE) {
        L::PreparedRead work;
        CHECK(L::nextRead(into, r.now, work) && work.ready);
        CHECK(r.a.admitPrepared(work.transfer, r.now) == O::BusAdmission::ACCEPTED);
        r.steps(2);
        CHECK(L::advanceRead(into, r.settled(), r.now));
    }
    CHECK(into.state == M::ReadState::SUCCEEDED);
}
void counterZeroAfterMove() {
    scenario = "counter zero after a relative move";
    Rig r;
    r.ready();
    r.drive.position = 1000;
    CHECK(r.relative(witness(r, 1000), relEvidence(), relRequest(2500)).isOk());
    r.finishMove();
    const L::MoveContext& c = moveOf(r);
    CHECK(c.state == M::ActionState::SUCCEEDED && c.rearm == M::ActionExecution::ACKNOWLEDGED && !r.a.moveFenced());
    CHECK(r.drive.position == 3500 && c.completed.position == 3500 && c.parameters.target.endpointNative == 3500);
    const Saved<L::MoveContext> moved(c);
    L::ActionOptions budgets;
    budgets.service = reads();
    budgets.observationIntervalUs = 1000;
    budgets.maximumAgeUs = uint64_t(QUEUE + uint64_t(RESPONSE)) * 9;
    // A healthy drive whose 603F still holds a historic FF05: no alarm is reported, so no clear is tried.
    r.drive.errorCode = 0xff05;
    L::ReadContext health;
    r.readState(health, uint8_t(bit(L::StateBlock::ERRORS) | bit(L::StateBlock::ERROR_HISTORY) |
                                bit(L::StateBlock::MONITORS) | bit(L::StateBlock::STATUS)));
    {
        const unsigned before = r.wire.sentCount;
        const Saved<E::AxisSession> session(r.a);
        const M::Status clear = r.a.requestClearAlarm(health, budgets, r.now + LEAD, r.now);
        CHECK(!clear && clear.detail == int32_t(M::ActionError::STATE));
        session.unchanged(r.a);
        quiet(r, 4);
        CHECK(r.wire.sentCount == before && !r.a.clearActive());
    }
    // The application's witness of the stationary endpoint after the rearm, and a second copy it kept.
    L::ReadContext feedback;
    r.readState(feedback, OBSERVE);
    r.one.witness = witness(r, 3500);
    const M::AxisReference old = r.one.witness;
    const uint32_t referenceBefore = r.one.coordinates.referenceGeneration;
    const M::Generation configurationBefore = r.one.coordinates.binding.configurationGeneration;
    const unsigned from = r.wire.sentCount;
    CHECK(r.a.requestCounterZero(feedback, 0, r.one.coordinates, r.one.witness, budgets, r.now + LEAD, r.now).isOk());
    for (unsigned k = 0; k < 600 && (r.a.clearActive() || r.a.busy()); ++k)
        r.step();
    const L::ActionContext& zero = r.a.clearAction();
    CHECK(zero.actionKind == M::ActionKind::SET_DEVICE_POSITION && zero.state == M::ActionState::SUCCEEDED);
    CHECK(zero.outcome == M::ActionOutcome::OBSERVED && zero.execution == M::ActionExecution::ACKNOWLEDGED);
    CHECK(zero.coordinatesInvalidated && !zero.valueOneMayRemain && !r.a.clearBlocked());
    CHECK(zero.baseline.present && zero.baseline.startPosition == 3500 && zero.extraObservation.actualPosition == 0);
    // The 2302 edge history: 0, 1, 0, each acknowledged and kept in its own slot, and nothing else written.
    CHECK(zero.commandEvidenceMask == 7);
    for (unsigned k = 0; k < 3; ++k)
        CHECK(zero.commandHistory[k].outcome == C::SdoOutcome::SUCCESS && zero.commandHistory[k].protocolAcknowledged &&
              zero.commandHistory[k].transfer.index == 0x2302 && zero.commandHistory[k].transfer.data[0] == (k == 1 ? 1 : 0));
    unsigned downloads = 0;
    for (unsigned i = from; i < r.wire.sentCount; ++i)
        if (isDownloadFrame(r, i, 1)) {
            CHECK(downloads < 3 && downloadAt(r, i, 0x2302, 2, downloads == 1 ? 1u : 0u));
            ++downloads;
        }
    CHECK(downloads == 3 && !downloadsFrom(r, from, 0x2301) && !downloadsFrom(r, from, 0x6040));
    // Withdrawn once, with no origin, home, witness or rebase in its place.
    CHECK(r.one.coordinates.referenceGeneration == referenceBefore + 1 &&
          r.one.coordinates.binding.configurationGeneration == configurationBefore + 1);
    CHECK(!r.one.coordinates.originKnown && !r.one.coordinates.encoderOriginKnown && !r.one.coordinates.softLimitsKnown);
    CHECK(!r.one.witness.nativeKnown && !r.one.witness.idle && !r.one.witness.stationary);
    CHECK(r.a.binding().configurationGeneration == configurationBefore + 1 && !r.a.latestControl());
    moved.unchanged(c);
    CHECK(r.drive.position == 0 && r.drive.edges == 1);
    const Saved<L::ActionContext> edges(zero);
    // The new epoch, after an explicit 000F: fresh identity and state, as any later move needs.
    {
        const uint64_t deadline = r.now + LEAD;
        const uint32_t operation = r.a.beginOperation(deadline, r.now);
        CHECK(operation);
        C::SdoTransfer t;
        t.channel = onSession(r).channel;
        t.operation = t.request = operation;
        t.step = 1;
        t.index = 0x6040;
        t.size = 2;
        t.direction = C::SdoDirection::DOWNLOAD;
        t.data[0] = 0x0f;
        t.deadlineUs = deadline;
        t.queueTimeoutUs = QUEUE;
        t.responseTimeoutUs = RESPONSE;
        CHECK(r.a.admitPrepared(t, r.now) == O::BusAdmission::ACCEPTED);
        r.steps(2);
        CHECK(r.settled().outcome == C::SdoOutcome::SUCCESS && r.a.latestControl());
    }
    {
        static L::ReadContext read;
        const uint64_t deadline = r.now + LEAD;
        const uint32_t operation = r.a.beginOperation(deadline, r.now);
        CHECK(operation);
        CHECK(L::prepareIdentity(read, onSession(r), operation, r.now, deadline, reads()));
        while (read.state == M::ReadState::ACTIVE) {
            L::PreparedRead work;
            CHECK(L::nextRead(read, r.now, work) && work.ready);
            CHECK(r.a.admitPrepared(work.transfer, r.now) == O::BusAdmission::ACCEPTED);
            r.steps(2);
            CHECK(L::advanceRead(read, r.settled(), r.now));
        }
        CHECK(L::getIdentity(read, onSession(r), r.now, LEAD, r.identity));
    }
    readOnSession(r, r.state, STATE_MASK);
    M::MoveRequest request = relRequest(400);
    request.position.configurationGeneration = request.speed.configurationGeneration = r.a.binding().configurationGeneration;
    // The old witness as kept, and re-dated as if observed now: both describe withdrawn coordinates.
    M::AxisReference redated = old;
    redated.observedUs = redated.nowUs = r.now;
    const M::AxisReference* stale[] = {&old, &redated};
    for (const M::AxisReference* w : stale) {
        const unsigned before = r.wire.sentCount;
        const M::Status refused = r.relative(*w, relEvidence(), request);
        CHECK(!refused && refused.code == M::Err::INVALID_CONFIG && !r.a.moveActive() && !r.a.busy());
        CHECK(w == &old || refused.detail == AXIS + int32_t(M::AxisError::STALE_GENERATION));
        quiet(r, 4);
        CHECK(r.wire.sentCount == before);
    }
    // Only a witness of the new epoch admits a new move; it is ended here before any of it leaves.
    M::AxisReference fresh = redated;
    fresh.binding = r.one.coordinates.binding;
    fresh.referenceGeneration = r.one.coordinates.referenceGeneration;
    fresh.nativePosition = 0;
    const unsigned before = r.wire.sentCount;
    CHECK(r.relative(fresh, relEvidence(), request).isOk());
    CHECK(r.a.cancelMove(r.now));
    finishAll(r);
    CHECK(r.wire.sentCount == before && r.drive.edges == 1 && r.drive.position == 0);
    CHECK(moveOf(r).execution == M::ActionExecution::NOT_TRANSMITTED && !r.a.moveFenced());
    edges.unchanged(zero);
}

// 31 to 33: the 2026-11-09 motion trials replayed. Every 2100, 6041, 6064 and 606C value in these
// tables is the retained capture's, in its order; the cited lines are the capture's RESULT lines (the
// TX lines for frames sent). Each test drives the real owner and AxisSession with those values and
// checks what the library concluded on the bench, so a change in how it reads these exact values fails
// here. The fake identity stands in for the identity reads (one firmware: 1000=00040192, 2000=0400,
// 2001=0200), and observation intervals are host ticks, not the bench's seconds. Drive reports only:
// no shaft angle or direction was measured in any of these trials.
//
// The trials' measured signals and base, from each capture's TRIAL_EVIDENCE line: acceptance 6041 bit
// 12, completion 2100 bit 0 after its absence, base ACTUAL, under the given record.
L::MoveEvidence benchEvidence(uint32_t record) {
    L::MoveEvidence value = relEvidence(L::RelativeBase::ACTUAL);
    value.record = record;
    value.completion.source = L::MoveSignalSource::MONITOR;
    value.completion.mask = value.completion.value = L::MONITOR_POSITION_COMPLETE;
    return value;
}
M::MoveRequest benchRequest(int32_t displacement) {
    M::MoveRequest value = moveRequest(displacement, 6); // 6 rpm: 6081 = 6.
    value.position.relative = true;
    value.position.basis = M::RelativeBasis::ACTUAL;
    return value;
}
// BenchRoute::startStop's budgets: an age cap of seven fields, which offers a quick stop the
// standstill rule, and no best-effort branch (--stop-best-effort-ms 0).
E::StopPolicy benchStopPolicy(const Rig& r) {
    E::StopPolicy policy;
    policy.options.service = reads();
    policy.options.observationIntervalUs = 1000;
    policy.options.maximumAgeUs = 7 * (uint64_t(QUEUE) + RESPONSE);
    policy.deadlineUs = r.now + 200000;
    return policy;
}
bool sampleIs(const L::MoveSample& s, uint32_t cycle, uint16_t status, uint16_t monitor, bool feedback,
              int32_t position, int32_t velocity) {
    return s.present && s.cycle == cycle && s.statusword == status && s.monitor == monitor && s.modeRaw == 1 &&
           s.feedback == feedback && s.position == position && s.velocity == velocity;
}
bool cycleOn(const Rig& r, unsigned i, unsigned node) {
    const uint16_t order[] = {0x2100, 0x2101, 0x2102, 0x6041, 0x6061, 0x6064, 0x606c};
    for (unsigned k = 0; k < 7; ++k)
        if (!uploadAt(r, i + k, order[k], 0, node))
            return false;
    return true;
}
// The trials' setup on the wire, byte for byte: 607A, 6081 = 6, 6083, 6084, then the 005F trigger.
bool benchSetupAt(const Rig& r, unsigned i, int32_t displacement, uint32_t ramp, unsigned node) {
    return downloadAt(r, i, 0x607a, 4, uint32_t(displacement), node) && downloadAt(r, i + 1, 0x6081, 4, 6, node) &&
           downloadAt(r, i + 2, 0x6083, 4, ramp, node) && downloadAt(r, i + 3, 0x6084, 4, ramp, node) &&
           wordAt(r, i + 4, 0x005f, node);
}
// The typed reads of one state read: what the bench's routes and observer recorded.
struct Seen {
    uint16_t monitor, status;
    int32_t position, velocity;
};
Seen seen(const Rig& r, const L::ReadContext& read, unsigned node) {
    L::StateObservation m, s, f;
    CHECK(L::getStateBlock(read, L::StateBlock::MONITORS, r.current(node), r.now, LEAD, m));
    CHECK(L::getStateBlock(read, L::StateBlock::STATUS, r.current(node), r.now, LEAD, s) && s.modeRaw == 1);
    CHECK(L::getStateBlock(read, L::StateBlock::FEEDBACK, r.current(node), r.now, LEAD, f));
    const Seen value = {m.monitor, s.statusword, f.actualPosition, f.actualVelocity};
    return value;
}
bool seenIs(const Seen& v, uint16_t monitor, uint16_t status, int32_t position, int32_t velocity) {
    return v.monitor == monitor && v.status == status && v.position == position && v.velocity == velocity;
}

// 31. Trials 825 and 1110: a production relative move from an ACTUAL witness at 0, accepted in cycle 1
// (6041 5037, bit 12; 2100 0004, bit 0 absent) and complete in cycle 2 (2100 0001, 6041 9437) at the
// written displacement, then rearmed with 000F. Node 1's -100 ran under its record 409 (attended, "a
// small turn", direction not given); node 2's +200 was its first production relative move, under its
// own record 830 (unattended). The later reads 831 and 1112 found 6041 8437: bit 12 clear after the rearm.
struct CompletedReplay {
    const char* name;
    unsigned node;
    uint32_t record;
    int32_t displacement;
    Report cycles[2];
};
const CompletedReplay COMPLETED[] = {
    // docs/bench/20261109T1033Z-p8-moverel-neg-node1-825/rx.txt: admission lines 37, 40 and 42 (2100
    // 0001, 6041 8437, 6064 0); setup TX lines 145 to 157; cycle 1 lines 49 to 55, cycle 2 lines 56 to
    // 62; rearm line 63 (TX 202); TRIAL_MOVE line 11. Later read: 20261109T1039Z-p8-p1-node1-831 line 6.
    {"trial 825", 1, 409, -100, {{0x0004, 0x5037, 0, 0}, {0x0001, 0x9437, -100, 0}}},
    // docs/bench/20261109T1252Z-r4b-moverel-node2-1110/rx.txt: admission lines 37, 40 and 42; setup TX
    // lines 145 to 157; cycle 1 lines 49 to 55, cycle 2 lines 56 to 62; rearm line 63 (TX 208);
    // TRIAL_MOVE line 11. Later read: 20261109T1252Z-r4b-p1-node2-1112 line 6.
    {"trial 1110", 2, 830, 200, {{0x0004, 0x5037, 0, 0}, {0x0001, 0x9437, 200, 0}}},
};
void replayCompleted(const CompletedReplay& t) {
    scenario = t.name;
    Rig r(t.node == 2);
    Axis& x = r.axis(t.node);
    x.drive.status = 0x8437;
    x.drive.monitor = 0x0001;
    x.drive.position = 0;
    x.drive.script = t.cycles;
    x.drive.scriptLength = 2;
    r.ready(t.node);
    const M::AxisReference start = witness(r, 0, M::RelativeBasis::ACTUAL, t.node);
    CHECK(r.relative(start, benchEvidence(t.record), benchRequest(t.displacement), t.node, profile(200, 200)).isOk());
    r.finishMove(2000, t.node);
    for (unsigned k = 0; k < 600 && x.a.busy(); ++k)
        r.step();
    // TRIAL_MOVE: state 2, phase 4, outcome 1, int 0, setup, exec, completion and rearm 1, uncertain 0,
    // readUnc 0, effects 1, cycles 2, mask f, ev f, trigger 1, rearmed 1, fenced 0, code 0.
    const L::MoveContext& c = moveOf(r, t.node);
    CHECK(c.state == M::ActionState::SUCCEEDED && c.phase == L::MovePhase::REARM);
    CHECK(c.outcome == M::ActionOutcome::OBSERVED && c.interruption == M::MoveInterruption::NONE);
    CHECK(c.setup == M::ActionExecution::ACKNOWLEDGED && c.execution == M::ActionExecution::ACKNOWLEDGED);
    CHECK(c.completion == M::ActionCompletion::OBSERVED && c.rearm == M::ActionExecution::ACKNOWLEDGED);
    CHECK(!c.uncertain && !c.readUncertain && c.effectsPossible && c.observations == 2);
    CHECK(c.setupMask == 0xf && c.setupEvidenceMask == 0xf && c.triggerPresent && c.rearmPresent);
    CHECK(c.status.code == M::Err::OK && !c.status.detail && !x.a.moveFenced());
    CHECK(c.parameters.targetNative == t.displacement && c.evidence.record == t.record);
    // TRIAL_MOVE_SAMPLE lines 12 to 16: admission, latest, acceptance, absence, completed.
    CHECK(sampleIs(c.admission, 0, 0x8437, 0x0001, false, 0, 0));
    CHECK(sampleIs(c.latest, 2, 0x9437, 0x0001, true, t.displacement, 0));
    CHECK(sampleIs(c.acceptance, 1, 0x5037, 0x0004, true, 0, 0));
    CHECK(sampleIs(c.absence, 1, 0x5037, 0x0004, true, 0, 0));
    CHECK(sampleIs(c.completed, 2, 0x9437, 0x0001, true, t.displacement, 0));
    // The host endpoint from the ACTUAL witness, and the drive's report at it. The report does not
    // establish the endpoint; here they agree.
    const M::PreparedTarget& target = c.parameters.target;
    CHECK(c.displacement == M::MoveDisplacement::KNOWN && c.displacementNative == t.displacement);
    CHECK(target.endpointNative == t.displacement && target.retainedReference.nativePosition == 0);
    CHECK(target.retainedReference.basis == M::RelativeBasis::ACTUAL && c.evidence.relativeBase == L::RelativeBase::ACTUAL);
    // The wire, as the capture's: setup, the trigger, two seven-field cycles, the rearm, nothing more.
    unsigned i = r.startFrame;
    CHECK(benchSetupAt(r, i, t.displacement, 200, t.node));
    i += 5;
    CHECK(cycleOn(r, i, t.node) && cycleOn(r, i + 7, t.node));
    i += 14;
    CHECK(wordAt(r, i++, 0x000f, t.node) && i == r.wire.sentCount && x.drive.edges == 1);
    const C::SdoCompletion* control = x.a.latestControl();
    CHECK(control && control->transfer.data[0] == 0x0f && control->transfer.data[1] == 0 &&
          control->completedUs == c.rearmEvidence.completedUs);
    // The later read: the fake's rearm cleared bit 12 of the last report, as the drive's did.
    const Saved<L::MoveContext> done(c);
    static L::ReadContext later;
    r.readState(later, OBSERVE, t.node);
    CHECK(seenIs(seen(r, later, t.node), 0x0001, 0x8437, t.displacement, 0));
    done.unchanged(c);
}
void completedReplays() {
    for (const CompletedReplay& t : COMPLETED)
        if (t.node == 1 || PAIRED_CHANNELS) // Node 2's trial binds slot 1.
            replayCompleted(t);
}

// 32. Trials 832 and 1109: node 1's production relative move +600 at 6 rpm from an ACTUAL witness, node
// 2 observing through its own session, and the planned stop from the held 005F after the second cycle
// that reported no completion. The move ends interrupted and kept (outcome CANCELLED, interruption
// STOP, trajectory uncertain, fenced); the stop completes as its own record. 832's HALT completed when
// 2100 running cleared, in its second cycle: the first after 010F still read running (0004) with 6041
// 4037. 1109's quick stop read released with running latched (2100 0014, 6041 4050) in every cycle;
// it completed on standstill, 6064 259 with 606C 0 in two FEEDBACK cycles in a row, 3 pulses past the
// last move cycle's 256. In both the observer read the same position and 606C 0 throughout and received
// no write. 832's last move cycle read 146 with 606C 5, before the 010F; the HALT's cycles read no
// 6064, so the halted position is not in this capture. In the stop rows, 6064 and 606C of a cycle that
// did not read them repeat the previous values and are never read.
struct StoppedReplay {
    const char* name;
    L::StopKind kind;
    int32_t start;
    uint16_t observerStatus;
    int32_t observerPosition;
    Report cycles[5]; // Two move cycles, then the stop's.
    unsigned length, stopObservations;
    uint16_t stopMonitor, stopStatus;
    unsigned stopUploads; // Node 1's uploads after the stop word: five per cycle, seven with FEEDBACK.
};
const StoppedReplay STOPPED[] = {
    // docs/bench/20261109T1039Z-p8-moverel-halt-observed-node1-832/rx.txt: admission lines 53, 56 and 58
    // (2100 0001, 6041 8437, 6064 -100); observer node 2 lines 35 to 41, 61 to 78 and 79 to 85 (2100
    // 0001, 6041 9437, 6064 304, 606C 0); setup TX lines 212 to 230; cycle 1 lines 68 to 77, cycle 2
    // lines 86 to 92; 010F line 93 (TX 311); stop cycles lines 94 to 98 and 99 to 103; TRIAL_MOVE line
    // 15, TRIAL_STOP line 21, TRIAL_OBSERVER line 10.
    {"trial 832", L::StopKind::HALT, -100, 0x9437, 304,
     {{0x0004, 0x5037, -100, 0}, {0x0004, 0x5037, 146, 5}, {0x0004, 0x4037, 146, 5}, {0x0000, 0x0437, 146, 5}},
     4, 2, 0x0000, 0x0437, 10},
    // docs/bench/20261109T1251Z-r4b-moverel-quick-observed-node1-1109/rx.txt: admission lines 53, 56 and
    // 58 (2100 0001, 6041 8437, 6064 0); observer node 2 lines 35 to 41, 61 to 78 and 79 to 85 (2100
    // 0001, 6041 8437, 6064 0, 606C 0); setup TX lines 223 to 241; cycle 1 lines 68 to 77, cycle 2 lines
    // 86 to 92; 000B line 93 (TX 326); stop cycles lines 94 to 98, 99 to 105 and 106 to 112; TRIAL_MOVE
    // line 15, TRIAL_STOP line 21, TRIAL_OBSERVER line 10.
    {"trial 1109", L::StopKind::QUICK_STOP, 0, 0x8437, 0,
     {{0x0004, 0x5037, 0, 0}, {0x0004, 0x5037, 256, 5}, {0x0014, 0x4050, 256, 5}, {0x0014, 0x4050, 259, 0},
      {0x0014, 0x4050, 259, 0}},
     5, 3, 0x0014, 0x4050, 19},
};
// TRIAL_OBSERVER: the observer's positions and 606C over its reads, and its 2100 bits together.
struct Watch {
    int32_t lowest = INT32_MAX, highest = INT32_MIN, fastest = 0;
    uint16_t monitors = 0;
    unsigned reads = 0;
};
void watch(Rig& r, Watch& w) {
    r.readState(r.two.state, OBSERVE, 2);
    const Seen v = seen(r, r.two.state, 2);
    w.lowest = v.position < w.lowest ? v.position : w.lowest;
    w.highest = v.position > w.highest ? v.position : w.highest;
    const int32_t speed = v.velocity < 0 ? -v.velocity : v.velocity;
    w.fastest = speed > w.fastest ? speed : w.fastest;
    w.monitors = uint16_t(w.monitors | v.monitor);
    ++w.reads;
}
void replayStopped(const StoppedReplay& t) {
    scenario = t.name;
    Rig r(true);
    r.one.drive.status = 0x8437;
    r.one.drive.monitor = 0x0001;
    r.one.drive.position = t.start;
    r.one.drive.script = t.cycles;
    r.one.drive.scriptLength = t.length;
    r.two.drive.status = t.observerStatus;
    r.two.drive.monitor = 0x0001;
    r.two.drive.position = t.observerPosition;
    r.ready(1);
    r.ready(2);
    const unsigned observerStart = r.wire.sentCount;
    const unsigned observerWords = r.two.drive.words;
    const C::SdoCompletion observerControl = *r.two.a.latestControl();
    Watch w;
    watch(r, w);
    L::MoveOptions o = options();
    o.observationIntervalUs = 20000; // Room for an observer read between cycles, as on the bench.
    CHECK(r.relative(witness(r, t.start, M::RelativeBasis::ACTUAL, 1), benchEvidence(409), benchRequest(600), 1,
                     profile(200, 200), LEAD, o)
              .isOk());
    const unsigned start = r.startFrame;
    for (unsigned k = 0; k < 400 && moveOf(r).observations < 1; ++k)
        r.step();
    watch(r, w);
    CHECK(moveOf(r).observations == 1);
    for (unsigned k = 0; k < 400 && moveOf(r).observations < 2; ++k)
        r.step();
    // BenchRoute::serviceMove: after the second cycle without completion, while the move waits.
    CHECK(moveOf(r).observations == 2 && moveOf(r).phase == L::MovePhase::WAITING);
    CHECK(moveOf(r).completion == M::ActionCompletion::NOT_OBSERVED);
    const unsigned at = r.wire.sentCount;
    const E::StopPolicy policy = benchStopPolicy(r);
    CHECK(r.one.a.requestStop(t.kind, r.one.a.moveObservation(), policy, r.now) == E::StopPath::CONFIRMED);
    finishAll(r);
    watch(r, w);

    // TRIAL_MOVE: state 3, phase 2, outcome 5, int 2, setup and exec 1, completion 0, rearm 0, uncertain
    // 1, readUnc 0, effects 1, cycles 2, target 600, mask f, ev f, trigger 1, rearmed 0, fenced 1, code 1,
    // detail 3.
    const L::MoveContext& c = moveOf(r);
    CHECK(c.state == M::ActionState::FAILED && c.phase == L::MovePhase::WAITING);
    CHECK(c.outcome == M::ActionOutcome::CANCELLED && c.interruption == M::MoveInterruption::STOP);
    CHECK(c.setup == M::ActionExecution::ACKNOWLEDGED && c.execution == M::ActionExecution::ACKNOWLEDGED);
    CHECK(c.completion == M::ActionCompletion::NOT_OBSERVED && c.rearm == M::ActionExecution::NOT_TRANSMITTED);
    CHECK(c.uncertain && !c.readUncertain && c.effectsPossible && c.observations == 2);
    CHECK(c.setupMask == 0xf && c.setupEvidenceMask == 0xf && c.triggerPresent && !c.rearmPresent);
    CHECK(c.status.code == M::Err::INVALID_CONFIG && c.status.detail == int32_t(M::MoveError::STATE));
    CHECK(r.one.a.moveFenced() && c.parameters.targetNative == 600);
    CHECK(c.parameters.target.endpointNative == t.start + 600 && c.parameters.target.retainedReference.nativePosition == t.start);
    // TRIAL_MOVE_SAMPLE lines 16 to 20: admission, latest, acceptance, absence; nothing completed.
    CHECK(sampleIs(c.admission, 0, 0x8437, 0x0001, false, 0, 0));
    CHECK(sampleIs(c.latest, 2, 0x5037, 0x0004, true, t.cycles[1].position, 5));
    CHECK(sampleIs(c.acceptance, 1, 0x5037, 0x0004, true, t.start, 0));
    CHECK(sampleIs(c.absence, 1, 0x5037, 0x0004, true, t.start, 0) && !c.completed.present);
    // TRIAL_STOP: path 1, final 4, state 2, outcome 1, exec 1, completion 1, uncertain 0, effects 1, pred
    // 005f, documented 0, basis 3; no best-effort stop.
    const L::ActionContext& s = r.one.a.stopAction(t.kind);
    CHECK(s.kind == t.kind && s.stopModeRaw == 1 && s.operation > c.operation);
    CHECK(s.state == M::ActionState::SUCCEEDED && s.outcome == M::ActionOutcome::OBSERVED);
    CHECK(s.execution == M::ActionExecution::ACKNOWLEDGED && s.completion == M::ActionCompletion::OBSERVED);
    CHECK(!s.uncertain && s.effectsPossible && s.observations == t.stopObservations);
    CHECK(s.predecessorWord == 0x005f && !s.predecessorDocumented &&
          s.predecessorBasis == L::PredecessorBasis::PROJECT_DECISION);
    CHECK(r.one.a.stopPath() == E::StopPath::FINISHED && r.one.a.bestEffort().state == M::ActionState::EMPTY);
    CHECK(s.monitorObservation.monitor == t.stopMonitor && s.statusObservation.statusword == t.stopStatus);
    if (t.kind == L::StopKind::HALT) {
        // Complete on running clear; FEEDBACK never read.
        CHECK(!s.standstillObserving && !s.extraObservation.fieldCount);
    } else {
        // Complete on standstill: released, low bits 000, running latched, then 6064 unchanged at 606C 0.
        CHECK(s.standstillObserving && s.extraObservation.fieldCount == 2);
        CHECK(s.extraObservation.actualPosition == 259 && !s.extraObservation.actualVelocity);
    }
    // Node 1's wire: setup, the 005F, two seven-field cycles, the one stop word, then only uploads.
    CHECK(benchSetupAt(r, start, 600, 200, 1) && wordsFrom(r, start, 0x005f, 1) == 1);
    CHECK(uploadsFrom(r, start, 0x2100, 1) == 2 + t.stopObservations && r.one.drive.edges == 1);
    unsigned stopFrame = r.wire.sentCount, after = 0, downloads = 0;
    for (unsigned i = at; i < r.wire.sentCount; ++i) {
        if (r.wire.sent[i].id != 0x601)
            continue;
        if (r.wire.sent[i].data[0] != 0x40) {
            ++downloads;
            CHECK(wordAt(r, i, t.kind == L::StopKind::HALT ? 0x010f : 0x000b, 1));
            stopFrame = i;
        } else if (stopFrame < i) {
            ++after;
        }
    }
    CHECK(downloads == 1 && after == t.stopUploads && !wordsFrom(r, start, 0x000f, 1));
    // TRIAL_OBSERVER: checked, never moved, 606C max 0, 2100 0001 in every read, no write to it.
    CHECK(w.reads == 3 && w.lowest == t.observerPosition && w.highest == t.observerPosition);
    CHECK(!w.fastest && w.monitors == 0x0001);
    for (unsigned i = observerStart; i < r.wire.sentCount; ++i)
        CHECK(r.wire.sent[i].id != 0x602 || r.wire.sent[i].data[0] == 0x40);
    CHECK(r.two.drive.words == observerWords && !r.two.drive.edges && !r.two.a.moveActive());
    const C::SdoCompletion* control = r.two.a.latestControl();
    CHECK(control && control->transfer.operation == observerControl.transfer.operation &&
          control->completedUs == observerControl.completedUs);
}
void stoppedReplays() {
    for (const StoppedReplay& t : STOPPED)
        replayStopped(t);
}

// 33. Trials 828, 829 and 830 on node 2, as the bench's experiment routes ran them through the
// session: each write as its own explicit download, each cycle a typed MONITORS, STATUS and FEEDBACK
// read. 828: 607A 600, 6081 6, 6083 500, 6084 500, then 001F from 6064 0; after two cycles, the second at
// 6064 75 with 606C 5 and running, a HALT from the acknowledged 001F completed when 2100 running
// cleared, in its third cycle (0004, 0004, then 0000; 6041 4037, 4037, 0437). 829: 000F after the 010F,
// and 6064 stayed 104 in the read before it and in all eight cycles after it, 6041 0437 then 0037, so
// the drive did not resume the halted target. 830: 607A 200 and 005F from 6064 104 ended at 304, the
// actual position plus the displacement; the commanded 600 plus 200 would be 800. That comparison is
// the measurement of node 2's base (attended: "seems to me that exactly that happened but cant confirm
// precise motion, position etc"); the library does not infer a base, and this test checks only the
// arithmetic on the values it read.
//
// docs/bench/20261109T1034Z-p8-position-halt-node2-828/rx.txt: read before lines 11 to 19 (2100 0001,
// 6041 8437, 6064 0); writes TX lines 98 to 110; cycles lines 29 to 35 and 36 to 42; 010F line 43 (TX
// 155); stop cycles lines 44 to 48, 49 to 53 and 54 to 58 (no 6064 read); TRIAL_STOP line 8. Then
// docs/bench/20261109T1034Z-p8-rearm-halt-node2-829/rx.txt: read lines 9 to 17, 000F line 18 (TX 102),
// cycles lines 19 to 74.
const Report HALTED_828[] = {{0x0004, 0x5037, 0, 0},   {0x0004, 0x5037, 75, 5},  {0x0004, 0x4037, 75, 5},
                             {0x0004, 0x4037, 75, 5},  {0x0000, 0x0437, 75, 5},  {0x0000, 0x0437, 104, 0},
                             {0x0000, 0x0037, 104, 0}, {0x0000, 0x0037, 104, 0}, {0x0000, 0x0037, 104, 0},
                             {0x0000, 0x0037, 104, 0}, {0x0000, 0x0037, 104, 0}, {0x0000, 0x0037, 104, 0},
                             {0x0000, 0x0037, 104, 0}, {0x0000, 0x0037, 104, 0}};
// docs/bench/20261109T1035Z-p8-relative-node2-830/rx.txt: read before lines 9 to 17 (6064 104), 6081,
// 6083, 6084 lines 18 to 20 (6, 500, 500), 607A line 21 (600); writes TX lines 136 to 148; ten cycles
// lines 27 to 96; TRIAL_BASE line 8.
const Report RELATIVE_830[] = {{0x0004, 0x5037, 104, 0}, {0x0004, 0x5037, 179, 5}, {0x0004, 0x5037, 278, 5},
                               {0x0001, 0x9437, 304, 0}, {0x0001, 0x9437, 304, 0}, {0x0001, 0x9437, 304, 0},
                               {0x0001, 0x9437, 304, 0}, {0x0001, 0x9437, 304, 0}, {0x0001, 0x9437, 304, 0},
                               {0x0001, 0x9437, 304, 0}};
void node2Base() {
    scenario = "trials 828 to 830";
    Rig r(true);
    Axis& x = r.two;
    x.drive.status = 0x8437;
    x.drive.monitor = 0x0001;
    x.drive.position = 0;
    x.drive.script = HALTED_828;
    x.drive.scriptLength = sizeof HALTED_828 / sizeof HALTED_828[0];
    r.ready(2);
    const uint8_t FULL = uint8_t(OBSERVE | bit(L::StateBlock::ERRORS));
    // 828, POSITION: the writes and the 001F, then two cycles.
    const unsigned from = r.wire.sentCount;
    r.writeThrough(0x607a, 4, 600, 2);
    r.writeThrough(0x6081, 4, 6, 2);
    r.writeThrough(0x6083, 4, 500, 2);
    r.writeThrough(0x6084, 4, 500, 2);
    r.writeThrough(0x6040, 2, 0x001f, 2);
    CHECK(downloadAt(r, from, 0x607a, 4, 600, 2) && wordsFrom(r, from, 0x001f, 2) == 1 && x.drive.edges == 1);
    r.readState(x.state, OBSERVE, 2);
    CHECK(seenIs(seen(r, x.state, 2), 0x0004, 0x5037, 0, 0));
    r.readState(x.state, OBSERVE, 2);
    CHECK(seenIs(seen(r, x.state, 2), 0x0004, 0x5037, 75, 5));
    // The planned HALT from that last cycle (TRIAL_STOP kind 1 path 1 final 4 state 2 outcome 1 exec 1
    // completion 1 uncertain 0 effects 1 pred 001f documented 0 basis 3 observations 3).
    const unsigned at = r.wire.sentCount;
    CHECK(x.a.requestStop(L::StopKind::HALT, x.state, benchStopPolicy(r), r.now) == E::StopPath::CONFIRMED);
    for (unsigned k = 0; k < 400 && (x.a.stopActive() || x.a.busy()); ++k)
        r.step();
    const L::ActionContext& s = x.a.stopAction(L::StopKind::HALT);
    CHECK(s.state == M::ActionState::SUCCEEDED && s.outcome == M::ActionOutcome::OBSERVED);
    CHECK(s.execution == M::ActionExecution::ACKNOWLEDGED && s.completion == M::ActionCompletion::OBSERVED);
    CHECK(!s.uncertain && s.effectsPossible && s.observations == 3 && x.a.stopPath() == E::StopPath::FINISHED);
    CHECK(s.predecessorWord == 0x001f && !s.predecessorDocumented &&
          s.predecessorBasis == L::PredecessorBasis::PROJECT_DECISION);
    CHECK(s.monitorObservation.monitor == 0x0000 && s.statusObservation.statusword == 0x0437);
    CHECK(!s.standstillObserving && !s.extraObservation.fieldCount);
    CHECK(wordsFrom(r, at, 0x010f, 2) == 1 && downloadsFrom(r, at, 0x6040, 2) == 1);
    CHECK(uploadsFrom(r, at, 0x2100, 2) == 3 && !uploadsFrom(r, at, 0x6064, 2));
    // The session's latest word is the stop's own 010F, the one a later rearm follows.
    const C::SdoCompletion* control = x.a.latestControl();
    CHECK(control && control->transfer.data[0] == 0x0f && control->transfer.data[1] == 0x01 &&
          control->completedUs == s.commandHistory[0].completedUs);
    const Saved<L::ActionContext> halt(s);
    r.compact(); // Each trial's frames fit the rig's log; the drive and session keep their state.
    // 829, REARM: one 000F after the 010F; no new edge, and no read shows the halted target resumed.
    r.readState(x.state, FULL, 2);
    CHECK(seenIs(seen(r, x.state, 2), 0x0000, 0x0437, 104, 0));
    const unsigned rearm = r.wire.sentCount;
    r.writeThrough(0x6040, 2, 0x000f, 2);
    CHECK(wordAt(r, rearm, 0x000f, 2) && rearm + 1 == r.wire.sentCount);
    control = x.a.latestControl();
    CHECK(control && control->transfer.data[0] == 0x0f && control->transfer.data[1] == 0);
    for (unsigned k = 0; k < 8; ++k) {
        r.readState(x.state, OBSERVE, 2);
        CHECK(seenIs(seen(r, x.state, 2), 0x0000, 0x0037, 104, 0));
    }
    CHECK(x.drive.edges == 1 && !x.a.stopActive() && !x.a.moveActive());
    CHECK(!downloadsFrom(r, rearm + 1, 0x6040, 2) && !downloadsFrom(r, rearm + 1, 0x607a, 2));
    halt.unchanged(s);
    r.compact();
    // 830, RELATIVE: the read before, then 607A 200, 6081 6, 6083 500, 6084 500 and 005F.
    r.readState(x.state, FULL, 2);
    const Seen before = seen(r, x.state, 2);
    CHECK(seenIs(before, 0x0000, 0x0037, 104, 0));
    const int32_t commanded = 600; // 828's acknowledged 607A, read back as 600 on line 21.
    CHECK(x.drive.target == commanded);
    x.drive.script = RELATIVE_830;
    x.drive.scriptLength = sizeof RELATIVE_830 / sizeof RELATIVE_830[0];
    const unsigned relative = r.wire.sentCount;
    r.writeThrough(0x607a, 4, 200, 2);
    r.writeThrough(0x6081, 4, 6, 2);
    r.writeThrough(0x6083, 4, 500, 2);
    r.writeThrough(0x6084, 4, 500, 2);
    r.writeThrough(0x6040, 2, 0x005f, 2);
    CHECK(downloadAt(r, relative, 0x607a, 4, 200, 2) && wordAt(r, relative + 4, 0x005f, 2) && x.drive.edges == 2);
    Seen last = before;
    for (unsigned k = 0; k < 10; ++k) {
        r.readState(x.state, OBSERVE, 2);
        last = seen(r, x.state, 2);
        const Report& expected = RELATIVE_830[k];
        CHECK(seenIs(last, expected.monitor, expected.status, expected.position, expected.velocity));
    }
    // ACTUAL: 104 + 200 = 304, the reported end. COMMANDED: 600 + 200 = 800.
    CHECK(seenIs(last, 0x0001, 0x9437, 304, 0));
    CHECK(before.position + 200 == last.position && commanded + 200 != last.position);
    halt.unchanged(s);
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
    if (PAIRED_CHANNELS) {
        bothNodesUrgent();
        urgentBesideOtherNode();
    }
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
    cycleFaults();
    emcyDuringMove();
    faultFence();
    lostTriggerThenStop();
    if (PAIRED_CHANNELS)
        stopsBesideObserver();
    counterZeroAfterMove();
    completedReplays();
    if (PAIRED_CHANNELS) {
        stoppedReplays();
        node2Base();
    }
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
