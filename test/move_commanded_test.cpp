// SPDX-License-Identifier: MIT
// Adversarial contract tests of the COMMANDED relative base beyond its one success in
// move_relative_test: stale and wrong-basis witnesses, generation changes, endpoint and displacement
// bounds, quantization to zero, a borrowed witness changed before handoff, and an observation after
// acceptance that contradicts the commanded base. Synthetic only. No specimen record names COMMANDED:
// node 1 measured ACTUAL on 2026-11-04, and nothing else is measured. Every RelativeBase::COMMANDED
// here is this test's switch, opened to exercise the contract, and the fake's commanded base
// (support/MoveRig.h) is a switch too. Nothing here shows what any drive adds a displacement to, and
// nothing here enables or recommends COMMANDED for a specimen. Session scenarios run through the real
// owner, the selected SDO engine and AxisSession. Direct library calls run with no time passing, so
// each refusal is judged on inputs that are otherwise admitted. Every scenario runs twice, the second
// time with heartbeat traffic delivered on every turn.
#include <cstdint>
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

const int32_t PARAMETER = int32_t(M::MoveError::PARAMETER_ERROR_BASE);
const int32_t AXIS = PARAMETER + int32_t(L::PositionParameterError::AXIS_ERROR_BASE);
const int32_t UNIT = AXIS + int32_t(M::AxisError::UNIT_ERROR_BASE);
const int32_t RATE = int32_t(M::MoveError::RATE_ERROR_BASE);
int32_t moveDetail(M::MoveError e) { return int32_t(e); }
int32_t axisDetail(M::AxisError e) { return AXIS + int32_t(e); }
int32_t unitDetail(M::UnitError e) { return UNIT + int32_t(e); }
const int64_t BASE = 4000; // The fake's commanded position, and the host witness of it.
const int64_t STEP = 1000;
const int32_t FAKE_TARGET = 5000; // FakeDrive's 607A before any write.
const uint16_t SETUP[4] = {0x607a, 0x6081, 0x6083, 0x6084};

L::MoveEvidence commandedEvidence(L::RelativeBase base = L::RelativeBase::COMMANDED) {
    L::MoveEvidence value = evidence();
    value.relativeBase = base;
    return value;
}
M::MoveRequest commandedRequest(int64_t displacement, M::RelativeBasis basis = M::RelativeBasis::COMMANDED) {
    M::MoveRequest value = moveRequest(displacement);
    value.position.relative = true;
    value.position.basis = basis;
    return value;
}
// numerator/denominator command steps under an explicit rounding and allowance.
M::MoveRequest fractional(int64_t numerator, uint64_t denominator, M::Rounding rounding,
                          M::Rational allowance = M::Rational(1)) {
    M::MoveRequest value = commandedRequest(0);
    value.position.value = M::Rational(numerator, denominator);
    value.position.rounding = rounding;
    value.position.maximumQuantizationError = allowance;
    return value;
}
M::AxisReference commanded(const Rig& r, int64_t position = BASE) {
    return witness(r, position, M::RelativeBasis::COMMANDED);
}
// The fake adds a displacement to its last commanded endpoint, which differs from its actual position
// by ten steps here. The host axis policy admits the COMMANDED basis only.
void commandedRig(Rig& r) {
    r.ready();
    r.one.coordinates.supportedRelativeBases = 2;
    r.drive.commandedBase = true;
    r.drive.commanded = int32_t(BASE);
    r.drive.position = int32_t(BASE - 10);
}
void finishAll(Rig& r) {
    r.finishMove();
    for (unsigned k = 0; k < 600 && r.a.busy(); ++k)
        r.step();
    CHECK(!r.a.busy());
}
// The trigger noted at its admission, the frame still queued for the next owner turn.
void toNotedTrigger(Rig& r) {
    for (unsigned k = 0; k < 400 && r.a.moveActive() &&
                         !(moveOf(r).phase == L::MovePhase::TRIGGER && moveOf(r).transmissionNoted);
         ++k)
        r.step();
    CHECK(moveOf(r).phase == L::MovePhase::TRIGGER && moveOf(r).transmissionNoted);
    CHECK(!wordsFrom(r, r.startFrame, 0x005f));
}
// Refused at the session with exactly this status. Nothing is transmitted, no move starts, the fake's
// 607A and edges are untouched, and the latest acknowledged controlword and the retained move result
// stay byte for byte as they were.
void refusedHere(Rig& r, const M::AxisReference& w, const L::MoveEvidence& e, const M::MoveRequest& q,
                 M::Err code, int32_t detail) {
    CHECK(r.a.latestControl());
    const Saved<C::SdoCompletion> control(*r.a.latestControl());
    const Saved<L::MoveContext> retained(moveOf(r));
    const int32_t target = r.drive.target;
    const unsigned edges = r.drive.edges, sent = r.wire.sentCount;
    const M::Status s = r.relative(w, e, q);
    CHECK(!s && s.code == code && s.detail == detail);
    refused(r, s, code, M::MoveError(detail));
    CHECK(r.wire.sentCount == sent && r.drive.target == target && r.drive.edges == edges);
    CHECK(r.a.latestControl());
    control.unchanged(*r.a.latestControl());
    retained.unchanged(moveOf(r));
    CHECK(!r.a.moveFenced());
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

// ---- Direct library calls -------------------------------------------------------------------

// One rig, with no time passing between calls: every input but the one a vector changes stays
// admissible, so each refusal is the vector's own.
struct Direct {
    Rig r;
    L::AxisReservation live;
    L::ReadTarget here;
    C::SdoCompletion control;
    uint32_t operation = 0;
    Direct() {
        r.ready();
        live = r.reservation();
        here = r.current();
        control = *r.a.latestControl();
        operation = r.a.nextOperation();
        CHECK(r.owner.beginOperation(r.a.sequence(), operation, r.now + LEAD, r.now));
    }
};
struct Inputs {
    L::ReadTarget current;
    const L::ReadContext* state = nullptr;
    C::SdoCompletion control;
    L::IdentityObservation identity;
    L::MoveEvidence evidence;
    M::MoveRequest request;
    M::AxisConfig axis;
    M::AxisReference witness;
};
Inputs inputs(const Direct& d, int64_t displacement = STEP) {
    Inputs in;
    in.current = d.here;
    in.state = &d.r.state;
    in.control = d.control;
    in.identity = d.r.identity;
    in.evidence = commandedEvidence();
    in.request = commandedRequest(displacement);
    in.axis = axisConfig();
    in.axis.supportedRelativeBases = 2;
    in.witness = commanded(d.r);
    return in;
}
// The output every direct preparation writes. It holds a retained terminal result between vectors, so
// "unchanged" is a real claim.
L::MoveContext& output() {
    static L::MoveContext value;
    return value;
}
M::Status check(const Direct& d, const Inputs& in, bool publish) {
    return publish ? L::prepareMoveRelative(output(), d.live, in.current, *in.state, in.control, in.identity,
                                            in.evidence, in.request, profile(), nullptr, in.axis, in.witness,
                                            d.operation, d.r.now, d.r.now + LEAD, options())
                   : L::validateMoveRelative(output(), d.live, in.current, *in.state, in.control, in.identity,
                                             in.evidence, in.request, profile(), nullptr, in.axis, in.witness,
                                             d.operation, d.r.now, d.r.now + LEAD, options());
}
void refusedDirect(const Direct& d, const Inputs& in, M::Err code, int32_t detail) {
    const Saved<L::MoveContext> before(output());
    const M::Status checked = check(d, in, false);
    const M::Status prepared = check(d, in, true);
    CHECK(!checked && checked.code == code && checked.detail == detail);
    CHECK(!prepared && prepared.code == code && prepared.detail == detail);
    before.unchanged(output());
}
const L::MoveContext& admittedDirect(const Direct& d, const Inputs& in) {
    CHECK(check(d, in, false).isOk());
    CHECK(check(d, in, true).isOk());
    const L::MoveContext& c = output();
    CHECK(c.state == M::ActionState::ACTIVE && c.phase == L::MovePhase::PARAMETERS && c.operation == d.operation);
    CHECK(c.parameters.target.requested.relative && c.displacement == M::MoveDisplacement::KNOWN);
    CHECK(c.displacementNative == c.parameters.targetNative && c.parameters.targetNative != 0);
    CHECK(c.parameters.target.endpointNative == in.witness.nativePosition + c.parameters.targetNative);
    CHECK(c.parameters.target.retainedReference.nativePosition == in.witness.nativePosition);
    CHECK(c.parameters.target.retainedReference.observedUs == in.witness.observedUs);
    return c;
}
// End a direct move locally, so the output again holds a retained terminal result.
void endDirect(const Direct& d) {
    static L::ReadContext unused;
    CHECK(L::advanceMove(output(), unused, d.live, d.here, M::MoveInterruption::CANCELLED, d.r.now));
    CHECK(output().state == M::ActionState::FAILED && output().execution == M::ActionExecution::NOT_TRANSMITTED);
}

// 1. Wrong basis: every combination of request basis, witness basis and the drive's measured base.
// Only a request whose basis the measured base names, with a witness of that same basis, is admitted.
void directBasis(const Direct& d) {
    scenario = "direct basis matrix";
    const M::RelativeBasis bases[] = {M::RelativeBasis::ACTUAL, M::RelativeBasis::COMMANDED,
                                      M::RelativeBasis::QUEUED};
    const L::RelativeBase drives[] = {L::RelativeBase::UNMEASURED, L::RelativeBase::ACTUAL,
                                      L::RelativeBase::COMMANDED};
    for (M::RelativeBasis request : bases)
        for (M::RelativeBasis held : bases)
            for (L::RelativeBase drive : drives) {
                Inputs in = inputs(d);
                in.axis.supportedRelativeBases = 7; // Host arithmetic may use every basis here.
                in.evidence = commandedEvidence(drive);
                in.request = commandedRequest(STEP, request);
                in.witness = witness(d.r, BASE, held);
                const bool measured =
                    (drive == L::RelativeBase::ACTUAL && request == M::RelativeBasis::ACTUAL) ||
                    (drive == L::RelativeBase::COMMANDED && request == M::RelativeBasis::COMMANDED);
                if (!measured)
                    refusedDirect(d, in, M::Err::UNSUPPORTED, moveDetail(M::MoveError::RELATIVE_BASIS));
                else if (held != request)
                    refusedDirect(d, in, M::Err::INVALID_CONFIG, moveDetail(M::MoveError::REFERENCE));
                else {
                    const L::MoveContext& c = admittedDirect(d, in);
                    CHECK(c.parameters.target.retainedReference.basis == request && c.evidence.relativeBase == drive);
                    CHECK(c.parameters.target.requested.basis == request);
                    endDirect(d);
                }
            }
    // Out-of-range values never select a base.
    Inputs in = inputs(d);
    in.witness.basis = M::RelativeBasis(3);
    refusedDirect(d, in, M::Err::INVALID_CONFIG, moveDetail(M::MoveError::REFERENCE));
    in = inputs(d);
    in.request.position.basis = M::RelativeBasis(3);
    refusedDirect(d, in, M::Err::INVALID_CONFIG, moveDetail(M::MoveError::ARGUMENT));
    in = inputs(d);
    in.evidence.relativeBase = L::RelativeBase(3);
    refusedDirect(d, in, M::Err::INVALID_CONFIG, moveDetail(M::MoveError::ARGUMENT));
    // A closed gate is refused before any basis.
    in = inputs(d);
    in.evidence.gate = L::MoveGate::CLOSED;
    refusedDirect(d, in, M::Err::UNSUPPORTED, moveDetail(M::MoveError::CAPABILITY));
    // The host policy must admit COMMANDED too: measured evidence does not open host arithmetic.
    for (unsigned bits = 0; bits < 8; ++bits) {
        in = inputs(d);
        in.axis.supportedRelativeBases = uint8_t(bits);
        if (bits & 2u) {
            admittedDirect(d, in);
            endDirect(d);
        } else
            refusedDirect(d, in, M::Err::UNSUPPORTED, axisDetail(M::AxisError::UNSUPPORTED_BASIS));
    }
}

// 2. Stale witness: its age cap, its order against the latest acknowledged 000F, future times, and
// the evidence a COMMANDED witness must still carry.
void directStale(const Direct& d) {
    scenario = "direct stale witness";
    const uint64_t since = d.r.now - d.control.completedUs;
    CHECK(since >= 2);
    Inputs in = inputs(d);
    in.witness.observedUs = d.control.completedUs; // Observed exactly when the 000F completed.
    in.witness.maximumAgeUs = since;               // Inclusive: exactly as old as its cap.
    admittedDirect(d, in);
    endDirect(d);
    in.witness.maximumAgeUs = since - 1;
    refusedDirect(d, in, M::Err::INVALID_CONFIG, axisDetail(M::AxisError::STALE_REFERENCE));
    in = inputs(d);
    in.witness.observedUs = d.control.completedUs - 1; // Describes the drive before that 000F.
    refusedDirect(d, in, M::Err::INVALID_CONFIG, moveDetail(M::MoveError::REFERENCE));
    in = inputs(d);
    in.witness.observedUs = d.r.now + 1; // A future observation.
    refusedDirect(d, in, M::Err::INVALID_CONFIG, axisDetail(M::AxisError::STALE_REFERENCE));
    in = inputs(d);
    in.witness.nowUs = d.r.now + 1; // A witness clock ahead of the preparation.
    refusedDirect(d, in, M::Err::INVALID_CONFIG, axisDetail(M::AxisError::CLOCK_ERROR));
    in = inputs(d);
    in.witness.maximumAgeUs = 0; // No expiry is not freshness.
    refusedDirect(d, in, M::Err::INVALID_CONFIG, moveDetail(M::MoveError::REFERENCE));
    in = inputs(d);
    in.witness.nativeKnown = false;
    refusedDirect(d, in, M::Err::INVALID_CONFIG, axisDetail(M::AxisError::MISSING_REFERENCE));
    in = inputs(d);
    in.witness.source = M::ScaleSource::UNKNOWN;
    refusedDirect(d, in, M::Err::INVALID_CONFIG, axisDetail(M::AxisError::STALE_REFERENCE));
    in = inputs(d);
    in.witness.idle = false;
    refusedDirect(d, in, M::Err::INVALID_CONFIG, moveDetail(M::MoveError::REFERENCE));
    in = inputs(d);
    in.witness.stationary = false;
    refusedDirect(d, in, M::Err::INVALID_CONFIG, moveDetail(M::MoveError::REFERENCE));
}

// 3. Generation change before preparation: witness, request, axis, route, node, identity, control
// and state epochs. A COMMANDED witness from another epoch says nothing about the drive's base now.
void directGenerations(const Direct& d) {
    scenario = "direct generations";
    const M::Status stale(M::Err::INVALID_CONFIG, axisDetail(M::AxisError::STALE_GENERATION), "");
    Inputs in = inputs(d);
    ++in.witness.referenceGeneration;
    refusedDirect(d, in, stale.code, stale.detail);
    in = inputs(d);
    in.witness.referenceGeneration = 0;
    refusedDirect(d, in, stale.code, stale.detail);
    in = inputs(d);
    ++in.witness.binding.configurationGeneration;
    refusedDirect(d, in, stale.code, stale.detail);
    in = inputs(d);
    ++in.witness.binding.bindingGeneration;
    refusedDirect(d, in, stale.code, stale.detail);
    in = inputs(d);
    ++in.witness.binding.logicalAxisId; // Another axis's witness.
    refusedDirect(d, in, stale.code, stale.detail);
    in = inputs(d);
    --in.request.position.configurationGeneration;
    refusedDirect(d, in, stale.code, stale.detail);
    in = inputs(d);
    --in.request.speed.configurationGeneration;
    refusedDirect(d, in, M::Err::INVALID_CONFIG, RATE + int32_t(M::AxisError::STALE_GENERATION));
    in = inputs(d);
    ++in.axis.referenceGeneration; // Confidence revoked after the witness was taken.
    refusedDirect(d, in, stale.code, stale.detail);
    // An axis, request and witness that agree with each other but not with the reservation's binding.
    in = inputs(d);
    ++in.axis.binding.configurationGeneration;
    in.request.position.configurationGeneration = in.request.speed.configurationGeneration =
        in.axis.binding.configurationGeneration;
    in.witness.binding = in.axis.binding;
    refusedDirect(d, in, M::Err::INVALID_CONFIG, moveDetail(M::MoveError::TARGET));
    in = inputs(d);
    ++in.current.nodeGeneration; // A boot-up since the reservation.
    refusedDirect(d, in, M::Err::INVALID_CONFIG, moveDetail(M::MoveError::TARGET));
    in = inputs(d);
    ++in.current.channel.binding.configurationGeneration;
    refusedDirect(d, in, M::Err::INVALID_CONFIG, moveDetail(M::MoveError::TARGET));
    in = inputs(d);
    ++in.identity.target.nodeGeneration;
    refusedDirect(d, in, M::Err::UNSUPPORTED, moveDetail(M::MoveError::CAPABILITY));
    in = inputs(d);
    ++in.control.nodeGeneration;
    refusedDirect(d, in, M::Err::INVALID_CONFIG, moveDetail(M::MoveError::STATE));
    static L::ReadContext otherEpoch;
    otherEpoch = d.r.state;
    ++otherEpoch.target.nodeGeneration;
    in = inputs(d);
    in.state = &otherEpoch;
    refusedDirect(d, in, M::Err::INVALID_CONFIG, moveDetail(M::MoveError::STATE));
    // After preparation a new node epoch reaches no yield, notice or terminal of the old move.
    in = inputs(d);
    const L::MoveContext& c = admittedDirect(d, in);
    L::ReadTarget rebooted = d.here;
    ++rebooted.nodeGeneration;
    static L::ReadContext observation;
    L::PreparedMove work;
    const Saved<L::MoveContext> active(c);
    CHECK(L::nextMove(c, observation, d.live, rebooted, d.r.now, work).detail == moveDetail(M::MoveError::TARGET));
    CHECK(L::noteMoveTransmission(output(), d.live, rebooted, in.axis, &in.witness, nullptr, 0, d.r.now).detail ==
          moveDetail(M::MoveError::TARGET));
    active.unchanged(c);
    CHECK(c.setup == M::ActionExecution::NOT_TRANSMITTED && !c.transmissionNoted);
    endDirect(d);
}

// 4. Endpoint and displacement bounds: 607A's documented range and I32 width bound the displacement,
// the axis limits bound both endpoints, and int64 arithmetic is checked, not wrapped.
void directBounds(const Direct& d) {
    scenario = "direct bounds";
    const M::Err illegal = M::Err::ILLEGAL_VALUE;
    const int64_t edges[] = {1000000, -1000000};
    for (int64_t value : edges) {
        const Inputs in = inputs(d, value);
        const L::MoveContext& c = admittedDirect(d, in);
        CHECK(c.parameters.targetNative == value && c.parameters.target.endpointNative == BASE + value);
        endDirect(d);
    }
    const int64_t outside[] = {1000001, -1000001, INT32_MAX, INT32_MIN, int64_t(INT32_MAX) + 1,
                               int64_t(INT32_MIN) - 1, INT64_MAX - BASE};
    for (int64_t value : outside)
        refusedDirect(d, inputs(d, value), illegal, PARAMETER + int32_t(L::PositionParameterError::RANGE));
    // Requested just outside, effective back on the edge under ties-to-even: the request decides.
    const int64_t halves[] = {2000001, -2000001};
    for (int64_t numerator : halves) {
        Inputs in = inputs(d);
        in.request = fractional(numerator, 2, M::Rounding::NEAREST, M::Rational(1, 2));
        refusedDirect(d, in, illegal, PARAMETER + int32_t(L::PositionParameterError::RANGE));
    }
    // Checked int64 endpoint arithmetic near both ends.
    const int32_t overflow = unitDetail(M::UnitError::ARITHMETIC_OVERFLOW);
    Inputs in = inputs(d, 10);
    in.witness = commanded(d.r, INT64_MAX - 5);
    refusedDirect(d, in, illegal, overflow);
    in = inputs(d, -10);
    in.witness = commanded(d.r, INT64_MIN + 5);
    refusedDirect(d, in, illegal, overflow);
    refusedDirect(d, inputs(d, INT64_MAX), illegal, overflow);
    // I32 axis limits on both endpoints, requested and effective.
    const int32_t limit = axisDetail(M::AxisError::LIMIT);
    in = inputs(d, STEP);
    in.axis.nativeMinimum = INT32_MIN;
    in.axis.nativeMaximum = INT32_MAX;
    in.witness = commanded(d.r, int64_t(INT32_MAX) - 1000);
    CHECK(admittedDirect(d, in).parameters.target.endpointNative == INT32_MAX);
    endDirect(d);
    in.request = commandedRequest(1001);
    refusedDirect(d, in, illegal, limit);
    in.request = fractional(2001, 2, M::Rounding::NEAREST, M::Rational(1, 2)); // 1000.5 rounds to 1000.
    refusedDirect(d, in, illegal, limit);
    in.witness = commanded(d.r, int64_t(INT32_MIN) + 1000);
    in.request = commandedRequest(-1000);
    CHECK(admittedDirect(d, in).parameters.target.endpointNative == INT32_MIN);
    endDirect(d);
    in.request = commandedRequest(-1001);
    refusedDirect(d, in, illegal, limit);
    in.request = fractional(-2001, 2, M::Rounding::NEAREST, M::Rational(1, 2));
    refusedDirect(d, in, illegal, limit);
    in.witness = commanded(d.r, int64_t(INT32_MAX) + 1); // The witness itself outside native limits.
    in.request = commandedRequest(-10);
    refusedDirect(d, in, illegal, limit);
    // Soft limits: a witness outside them admits only inward travel that ends inside.
    in = inputs(d);
    in.axis.softLimitsKnown = true;
    in.axis.softMinimum = -10000;
    in.axis.softMaximum = 10000;
    in.witness = commanded(d.r, 12000);
    in.request = commandedRequest(-5000);
    CHECK(admittedDirect(d, in).parameters.target.endpointNative == 7000);
    endDirect(d);
    in.request = commandedRequest(-1000);
    refusedDirect(d, in, illegal, limit);
}

// 5. Quantization to zero: a nonzero request whose quantized displacement is zero is refused with
// ZERO_DISPLACEMENT before any write, under every rounding and frame. Pure arithmetic reports it.
void directQuantization(const Direct& d) {
    scenario = "direct quantization";
    struct Zero {
        int64_t numerator;
        uint64_t denominator;
        M::Rounding rounding;
    };
    const Zero zeros[] = {{1, 3, M::Rounding::NEAREST},      {-1, 3, M::Rounding::NEAREST},
                          {1, 2, M::Rounding::NEAREST},      {-1, 2, M::Rounding::NEAREST},
                          {9, 10, M::Rounding::TOWARD_ZERO}, {-9, 10, M::Rounding::TOWARD_ZERO},
                          {9, 10, M::Rounding::FLOOR},       {-9, 10, M::Rounding::CEIL}};
    const int32_t zero = moveDetail(M::MoveError::ZERO_DISPLACEMENT);
    for (const Zero& z : zeros) {
        Inputs in = inputs(d);
        in.request = fractional(z.numerator, z.denominator, z.rounding);
        refusedDirect(d, in, M::Err::ILLEGAL_VALUE, zero);
    }
    // Half a step through the motor and load frames: 1/6400 motor turn, 1/32000 load turn at gear 5.
    Inputs in = inputs(d);
    in.request = fractional(1, 6400, M::Rounding::NEAREST, M::Rational(1, 2));
    in.request.position.unit = M::PositionUnit::TURNS;
    in.request.position.frame = M::CoordinateFrame::MOTOR;
    refusedDirect(d, in, M::Err::ILLEGAL_VALUE, zero);
    in.request.position.value = M::Rational(3, 6400); // 1.5 steps: 2 under ties-to-even.
    CHECK(admittedDirect(d, in).parameters.targetNative == 2);
    CHECK(output().parameters.target.quantization.errorNumerator == 1 &&
          output().parameters.target.quantization.errorDenominator == 2 &&
          !output().parameters.target.quantization.errorNegative);
    endDirect(d);
    in.request.position.value = M::Rational(1, 32000);
    in.request.position.frame = M::CoordinateFrame::LOAD;
    refusedDirect(d, in, M::Err::ILLEGAL_VALUE, zero);
    // The nonzero neighbours are admitted, quantized once on the displacement, error kept.
    in = inputs(d);
    in.request = fractional(1, 10, M::Rounding::CEIL);
    const M::PreparedTarget& t = admittedDirect(d, in).parameters.target;
    CHECK(t.effectiveNative == 1 && t.endpointNative == BASE + 1 && t.quantization.errorNumerator == 9 &&
          t.quantization.errorDenominator == 10 && !t.quantization.errorNegative);
    endDirect(d);
    in.request = fractional(-1, 10, M::Rounding::FLOOR);
    admittedDirect(d, in);
    CHECK(t.effectiveNative == -1 && t.endpointNative == BASE - 1 && t.quantization.errorNumerator == 9 &&
          t.quantization.errorNegative);
    endDirect(d);
    // No quantization permitted, or an allowance below the error: refused, and neither is a zero.
    in.request = fractional(1, 3, M::Rounding::EXACT);
    refusedDirect(d, in, M::Err::ILLEGAL_VALUE, unitDetail(M::UnitError::FRACTIONAL_TARGET));
    in.request = fractional(1, 3, M::Rounding::NEAREST, M::Rational(1, 4));
    refusedDirect(d, in, M::Err::ILLEGAL_VALUE, unitDetail(M::UnitError::QUANTIZATION_ERROR));
    // The pure parameter arithmetic is not the gate: it reports the zero displacement.
    static L::PreparedPositionParameters pure;
    L::DriverSettings settings;
    settings.positionSpeedRpm = 120;
    settings.rampUpTimeMs = 100;
    settings.rampDownTimeMs = 200;
    in = inputs(d);
    in.request = fractional(1, 3, M::Rounding::NEAREST, M::Rational(1, 2));
    CHECK(L::preparePositionParameters(pure, L::SettingsModel::CL86_C, in.request.position, in.axis, &in.witness,
                                       settings, d.r.now));
    CHECK(pure.targetNative == 0 && pure.target.endpointNative == BASE && pure.target.referenceUsed);
}

// The changes a borrowed witness can undergo. `admission` is the time of the next admission check: a
// nowUs at it is a legitimate refresh, one microsecond past it is a clock running ahead.
enum class Mutation {
    NONE, POSITION, BASIS, OBSERVED, AGE_CAP, SOURCE, UNKNOWN_NATIVE, REFERENCE_EPOCH, BINDING, IDLE,
    STATIONARY, CLOCK_AHEAD, CLOCK_REFRESH
};
const char* const MUTATIONS[] = {"none", "position", "basis", "observed", "age cap", "source", "unknown native",
                                 "reference epoch", "binding", "idle", "stationary", "clock ahead",
                                 "clock refresh"};
const Mutation ALL_MUTATIONS[] = {Mutation::NONE, Mutation::POSITION, Mutation::BASIS, Mutation::OBSERVED,
                                  Mutation::AGE_CAP, Mutation::SOURCE, Mutation::UNKNOWN_NATIVE,
                                  Mutation::REFERENCE_EPOCH, Mutation::BINDING, Mutation::IDLE,
                                  Mutation::STATIONARY, Mutation::CLOCK_AHEAD, Mutation::CLOCK_REFRESH};
void mutate(M::AxisReference& w, Mutation m, uint64_t admission) {
    switch (m) {
    case Mutation::NONE: break;
    case Mutation::POSITION: ++w.nativePosition; break; // The host now believes another commanded base.
    case Mutation::BASIS: w.basis = M::RelativeBasis::ACTUAL; break;
    case Mutation::OBSERVED: w.observedUs = w.nowUs = admission; break; // A new observation.
    case Mutation::AGE_CAP: ++w.maximumAgeUs; break;
    case Mutation::SOURCE: w.source = M::ScaleSource::READBACK; break;
    case Mutation::UNKNOWN_NATIVE: w.nativeKnown = false; break;
    case Mutation::REFERENCE_EPOCH: ++w.referenceGeneration; break;
    case Mutation::BINDING: ++w.binding.bindingGeneration; break;
    case Mutation::IDLE: w.idle = false; break;
    case Mutation::STATIONARY: w.stationary = false; break;
    case Mutation::CLOCK_AHEAD: w.nowUs = admission + 1; break;
    case Mutation::CLOCK_REFRESH: w.nowUs = admission; break;
    }
}
// Zero when the change is not one: the same observation, with only its supplied time advanced.
int32_t mutationDetail(Mutation m) {
    switch (m) {
    case Mutation::NONE:
    case Mutation::CLOCK_REFRESH: return 0;
    case Mutation::IDLE:
    case Mutation::STATIONARY: return moveDetail(M::MoveError::REFERENCE);
    case Mutation::CLOCK_AHEAD: return axisDetail(M::AxisError::CLOCK_ERROR);
    default: return axisDetail(M::AxisError::STALE_REFERENCE);
    }
}

// 6. The notice rechecks the caller's current witness, not the frozen copy. Each refused notice
// leaves the context byte for byte, and the frozen snapshot is the context's own data.
void directNotice(const Direct& d) {
    scenario = "direct notice";
    Inputs in = inputs(d);
    const L::MoveContext& c = admittedDirect(d, in);
    static L::ReadContext observation;
    const uint64_t at = d.r.now + TICK;
    L::PreparedMove work;
    CHECK(L::nextMove(c, observation, d.live, d.here, at, work) && work.ready && work.transfer.index == 0x607a);
    const Saved<L::MoveContext> active(c);
    for (Mutation m : ALL_MUTATIONS) {
        if (!mutationDetail(m))
            continue;
        M::AxisReference changed = in.witness;
        mutate(changed, m, at);
        const M::Status s = L::noteMoveTransmission(output(), d.live, d.here, in.axis, &changed, nullptr, 0, at);
        CHECK(!s && s.code == M::Err::INVALID_CONFIG && s.detail == mutationDetail(m));
        active.unchanged(c);
    }
    CHECK(L::noteMoveTransmission(output(), d.live, d.here, in.axis, nullptr, nullptr, 0, at).detail ==
          axisDetail(M::AxisError::STALE_GENERATION));
    const M::ReferenceLoss losses[] = {M::ReferenceLoss::POSITION, M::ReferenceLoss::COORDINATES};
    for (M::ReferenceLoss loss : losses) {
        M::AxisConfig lost = in.axis;
        M::AxisReference scratch = in.witness;
        CHECK(M::invalidateAxisReference(lost, scratch, loss));
        CHECK(L::noteMoveTransmission(output(), d.live, d.here, lost, &in.witness, nullptr, 0, at).detail ==
              axisDetail(M::AxisError::STALE_GENERATION));
    }
    active.unchanged(c);
    // Changing the caller's witness after preparation leaves the frozen copy as it was.
    const M::AxisReference original = in.witness;
    in.witness.nativePosition += 7;
    in.witness.basis = M::RelativeBasis::ACTUAL;
    CHECK(c.parameters.target.retainedReference.nativePosition == BASE &&
          c.parameters.target.retainedReference.basis == M::RelativeBasis::COMMANDED);
    CHECK(L::noteMoveTransmission(output(), d.live, d.here, in.axis, &in.witness, nullptr, 0, at).detail ==
          axisDetail(M::AxisError::STALE_REFERENCE));
    active.unchanged(c);
    // The same observation with its supplied time refreshed to the admission is admitted.
    in.witness = original;
    mutate(in.witness, Mutation::CLOCK_REFRESH, at);
    CHECK(L::noteMoveTransmission(output(), d.live, d.here, in.axis, &in.witness, nullptr, 0, at));
    CHECK(c.transmissionNoted && c.setup == M::ActionExecution::UNKNOWN && c.effectsPossible);
    // The right work's terminal from another node epoch does not correlate.
    C::SdoCompletion ack = acknowledged(work.transfer, at, d.here.nodeGeneration + 1);
    const Saved<L::MoveContext> noted(c);
    CHECK(L::advanceMove(output(), observation, d.live, d.here, ack, at).detail ==
          moveDetail(M::MoveError::CORRELATION));
    noted.unchanged(c);
    ack.nodeGeneration = d.here.nodeGeneration;
    CHECK(L::advanceMove(output(), observation, d.live, d.here, ack, at));
    CHECK(c.setupCursor == 1 && c.setupHistory[0].outcome == C::SdoOutcome::SUCCESS);
    CHECK(L::advanceMove(output(), observation, d.live, d.here, M::MoveInterruption::CANCELLED, at));
    CHECK(c.state == M::ActionState::FAILED && c.setupEvidenceMask == 1 &&
          c.execution == M::ActionExecution::NOT_TRANSMITTED);
}

void directCalls() {
    Direct d;
    // The output starts as a retained terminal result, not an empty context.
    admittedDirect(d, inputs(d));
    endDirect(d);
    directBasis(d);
    directStale(d);
    directGenerations(d);
    directBounds(d);
    directQuantization(d);
    directNotice(d);
    // No direct call transmitted anything.
    quiet(d.r, 4);
}

// ---- Through the session ---------------------------------------------------------------------

// 7. A sample of each refusal through the session: nothing transmitted, nothing consumed, then the
// same rig moves from the fake's commanded base. A witness from before that move's rearm cannot
// start the next one; one observed exactly at the rearm's completion can.
void sessionRefusals() {
    scenario = "session refusals";
    Rig r;
    commandedRig(r);
    const L::MoveEvidence e = commandedEvidence();
    const M::MoveRequest q = commandedRequest(STEP);
    refusedHere(r, witness(r, BASE), e, q, M::Err::INVALID_CONFIG, moveDetail(M::MoveError::REFERENCE));
    refusedHere(r, commanded(r), commandedEvidence(L::RelativeBase::ACTUAL), q, M::Err::UNSUPPORTED,
                moveDetail(M::MoveError::RELATIVE_BASIS));
    refusedHere(r, commanded(r), evidence(), q, M::Err::UNSUPPORTED, moveDetail(M::MoveError::RELATIVE_BASIS));
    M::AxisReference aged = commanded(r);
    aged.observedUs = r.now - 2;
    aged.maximumAgeUs = 1;
    refusedHere(r, aged, e, q, M::Err::INVALID_CONFIG, axisDetail(M::AxisError::STALE_REFERENCE));
    M::AxisReference early = commanded(r);
    early.observedUs = r.a.latestControl()->completedUs - 1;
    refusedHere(r, early, e, q, M::Err::INVALID_CONFIG, moveDetail(M::MoveError::REFERENCE));
    M::AxisReference epoch = commanded(r);
    ++epoch.referenceGeneration;
    refusedHere(r, epoch, e, q, M::Err::INVALID_CONFIG, axisDetail(M::AxisError::STALE_GENERATION));
    refusedHere(r, commanded(r), e, commandedRequest(1000001), M::Err::ILLEGAL_VALUE,
                PARAMETER + int32_t(L::PositionParameterError::RANGE));
    refusedHere(r, commanded(r, INT64_MAX - 5), e, commandedRequest(10), M::Err::ILLEGAL_VALUE,
                unitDetail(M::UnitError::ARITHMETIC_OVERFLOW));
    refusedHere(r, commanded(r), e, fractional(1, 3, M::Rounding::NEAREST, M::Rational(1, 2)),
                M::Err::ILLEGAL_VALUE, moveDetail(M::MoveError::ZERO_DISPLACEMENT));
    r.one.coordinates.supportedRelativeBases = 1;
    refusedHere(r, commanded(r), e, q, M::Err::UNSUPPORTED, axisDetail(M::AxisError::UNSUPPORTED_BASIS));
    r.one.coordinates.supportedRelativeBases = 2;
    CHECK(!r.drive.edges && r.drive.target == FAKE_TARGET && r.drive.words == 1); // Only ready()'s 000F.
    r.readState(r.state); // Refusals let time pass; keep the admission read fresh.
    CHECK(r.relative(commanded(r), e, q).isOk());
    const unsigned first = r.startFrame;
    r.finishMove();
    const L::MoveContext& c = moveOf(r);
    CHECK(c.state == M::ActionState::SUCCEEDED && c.outcome == M::ActionOutcome::OBSERVED);
    CHECK(r.drive.endpoint == BASE + STEP && r.drive.edges == 1 && c.parameters.target.endpointNative == BASE + STEP);
    CHECK(downloadAt(r, first, 0x607a, 4, uint32_t(STEP)) && wordsFrom(r, first, 0x005f) == 1 &&
          wordsFrom(r, first, 0x000f) == 1);
    // The latest acknowledged controlword is now the move's own rearm.
    CHECK(c.rearmPresent && r.a.latestControl() && r.a.latestControl()->completedUs == c.rearmEvidence.completedUs);
    const uint64_t rearmed = c.rearmEvidence.completedUs;
    r.readState(r.state);
    refusedHere(r, r.one.witness, e, q, M::Err::INVALID_CONFIG, moveDetail(M::MoveError::REFERENCE));
    M::AxisReference before = commanded(r, BASE + STEP);
    before.observedUs = rearmed - 1;
    refusedHere(r, before, e, q, M::Err::INVALID_CONFIG, moveDetail(M::MoveError::REFERENCE));
    M::AxisReference at = commanded(r, BASE + STEP);
    at.observedUs = rearmed;
    CHECK(r.relative(at, e, q).isOk());
    const unsigned second = r.startFrame;
    r.finishMove();
    CHECK(moveOf(r).state == M::ActionState::SUCCEEDED && moveOf(r).parameters.target.endpointNative == BASE + 2 * STEP);
    CHECK(r.drive.endpoint == BASE + 2 * STEP && r.drive.edges == 2 && downloadAt(r, second, 0x607a, 4, uint32_t(STEP)));
}

// 8. A witness that ages between preparation and the trigger: the next notice refuses, nothing after
// it leaves, the acknowledged parameters stay in the record, and the drive holds a 607A with no edge.
void agesBeforeTrigger() {
    scenario = "witness ages before the trigger";
    Rig r;
    commandedRig(r);
    M::AxisReference w = commanded(r);
    w.maximumAgeUs = 3000; // Fresh at preparation, aged before the trigger.
    CHECK(r.relative(w, commandedEvidence(), commandedRequest(STEP)).isOk());
    r.finishMove();
    const L::MoveContext& c = moveOf(r);
    CHECK(c.state == M::ActionState::FAILED && c.outcome == M::ActionOutcome::INVALIDATED);
    CHECK(c.interruption == M::MoveInterruption::INVALIDATED);
    CHECK(r.a.moveRefusal().code == M::Err::INVALID_CONFIG &&
          r.a.moveRefusal().detail == axisDetail(M::AxisError::STALE_REFERENCE));
    CHECK(!wordsFrom(r, r.startFrame, 0x005f) && !r.drive.edges && c.execution == M::ActionExecution::NOT_TRANSMITTED);
    unsigned k = 0; // The refused download: everything before it acknowledged, nothing after it.
    while (k < 4 && (c.setupEvidenceMask & (1u << k)) && c.setupHistory[k].outcome == C::SdoOutcome::SUCCESS)
        ++k;
    CHECK(k >= 1 && k < 4); // 607A left at once; the witness aged before the last parameter.
    CHECK(c.setupEvidenceMask == uint8_t((1u << (k + 1)) - 1) && !c.setupHistory[k].transmissionPossible);
    for (unsigned i = 0; i < 4; ++i)
        CHECK(downloadsFrom(r, r.startFrame, SETUP[i]) == (i < k ? 1u : 0u));
    CHECK(!c.triggerPresent && c.setup == M::ActionExecution::UNKNOWN && c.effectsPossible);
    CHECK(!c.uncertain && !r.a.moveFenced() && r.drive.target == int32_t(STEP));
    // A download of the move may have reached the drive, so the 000F before it no longer describes the
    // drive: a fresh witness alone does not admit a new move.
    const unsigned sent = r.wire.sentCount;
    r.readState(r.state);
    const M::Status again = r.relative(commanded(r), commandedEvidence(), commandedRequest(STEP));
    CHECK(!again && again.detail == moveDetail(M::MoveError::STATE) && !r.a.latestControl());
    quiet(r, 4);
    CHECK(r.wire.sentCount == sent + 5 && !r.drive.edges); // Only the state read.
}

// 9. Generation change after preparation: a reference loss on the borrowed axis, or a node boot-up,
// before the first parameter or with the trigger queued. Nothing leaves after the change. After a
// boot-up the old COMMANDED witness describes the drive before it and cannot start a move.
enum class Epoch { POSITION, COORDINATES, BOOT };
void epochCase(Epoch epoch, bool atHandover) {
    static char name[64];
    const char* const epochs[] = {"position loss", "coordinates loss", "boot-up"};
    std::snprintf(name, sizeof name, "epoch %s%s", epochs[unsigned(epoch)], atHandover ? " at handover" : "");
    scenario = name;
    Rig r;
    commandedRig(r);
    r.drive.travelReads = 2;
    const M::AxisReference w = commanded(r);
    CHECK(r.relative(w, commandedEvidence(), commandedRequest(STEP)).isOk());
    if (atHandover)
        toNotedTrigger(r);
    const M::Generation node = r.current().nodeGeneration;
    const unsigned at = r.wire.sentCount;
    if (epoch == Epoch::BOOT) {
        M::Frame boot = frame(0x701, 1);
        boot.data[0] = 0;
        r.later(boot);
    } else {
        M::AxisReference scratch = r.one.witness; // The borrowed witness itself is left as it was.
        CHECK(M::invalidateAxisReference(r.one.coordinates, scratch,
                                         epoch == Epoch::POSITION ? M::ReferenceLoss::POSITION
                                                                  : M::ReferenceLoss::COORDINATES));
    }
    finishAll(r);
    const L::MoveContext& c = moveOf(r);
    CHECK(c.state == M::ActionState::FAILED && c.outcome == M::ActionOutcome::INVALIDATED);
    CHECK(r.wire.sentCount == at && !wordsFrom(r, r.startFrame, 0x005f) && !r.drive.edges);
    CHECK(c.execution == M::ActionExecution::NOT_TRANSMITTED && !r.a.moveFenced());
    if (epoch != Epoch::BOOT)
        CHECK(r.a.moveRefusal().detail == axisDetail(M::AxisError::STALE_GENERATION));
    if (atHandover)
        CHECK(c.setup == M::ActionExecution::ACKNOWLEDGED && c.triggerPresent && !c.trigger.transmissionPossible &&
              r.drive.target == int32_t(STEP));
    else
        CHECK(r.drive.target == FAKE_TARGET && !c.effectsPossible && c.setup == M::ActionExecution::NOT_TRANSMITTED);
    if (epoch != Epoch::BOOT)
        return;
    CHECK(r.current().nodeGeneration != node);
    // The reservation describes the node before its boot-up: no move starts on it, from the old
    // witness or a fresh one. The library half, a witness older than the latest 000F, is in
    // directStale; a reopen after boot-up is outside this rig.
    const M::AxisReference witnesses[] = {w, commanded(r)};
    for (const M::AxisReference& held : witnesses) {
        const unsigned sent = r.wire.sentCount;
        const M::Status again = r.relative(held, commandedEvidence(), commandedRequest(STEP));
        CHECK(!again && again.code == M::Err::INVALID_CONFIG && again.detail == moveDetail(M::MoveError::TARGET));
        CHECK(!r.a.moveActive() && !r.a.busy());
        quiet(r, 4);
        CHECK(r.wire.sentCount == sent && !r.drive.edges && moveOf(r).outcome == M::ActionOutcome::INVALIDATED);
    }
}
void generationAfterPreparation() {
    const Epoch epochs[] = {Epoch::POSITION, Epoch::COORDINATES, Epoch::BOOT};
    for (Epoch epoch : epochs)
        for (unsigned handover = 0; handover < 2; ++handover)
            epochCase(epoch, handover != 0);
}

// 10. The borrowed witness changed after preparation and before handoff: before the first parameter,
// while 607A is in flight (so the 6081 notice judges it), and with the trigger queued for handover.
// After the trigger's acknowledgement nothing rechecks it, and the move completes.
enum class Window { BEFORE_FIRST, MID_SETUP, AT_HANDOVER, AFTER_ACK };
void borrowedCase(Mutation m, Window when) {
    static char name[96];
    const char* const windows[] = {"before 607A", "607A in flight", "trigger queued", "after trigger ack"};
    std::snprintf(name, sizeof name, "borrowed %s, %s", MUTATIONS[unsigned(m)], windows[unsigned(when)]);
    scenario = name;
    Rig r;
    commandedRig(r);
    r.drive.travelReads = 2;
    CHECK(r.relative(commanded(r), commandedEvidence(), commandedRequest(STEP)).isOk());
    CHECK(r.a.latestControl());
    const Saved<C::SdoCompletion> control(*r.a.latestControl());
    const L::MoveContext& c = moveOf(r);
    const Saved<M::AxisReference> frozen(c.parameters.target.retainedReference);
    switch (when) {
    case Window::BEFORE_FIRST: break;
    case Window::MID_SETUP:
        for (unsigned k = 0; k < 400 && !downloadsFrom(r, r.startFrame, 0x607a); ++k)
            r.step();
        CHECK(downloadsFrom(r, r.startFrame, 0x607a) == 1 && c.setupCursor == 0 && c.transmissionNoted);
        break;
    case Window::AT_HANDOVER: toNotedTrigger(r); break;
    case Window::AFTER_ACK:
        for (unsigned k = 0; k < 400 && c.execution != M::ActionExecution::ACKNOWLEDGED; ++k)
            r.step();
        CHECK(c.execution == M::ActionExecution::ACKNOWLEDGED && c.state == M::ActionState::ACTIVE);
        break;
    }
    const unsigned refusals = r.wire.preflightRefusals;
    M::AxisReference expected = r.one.witness;
    mutate(r.one.witness, m, r.now + TICK);
    mutate(expected, m, r.now + TICK);
    finishAll(r);
    // Nothing writes into the borrowed witness, and the frozen snapshot is the move's own copy.
    const Saved<M::AxisReference> mutated(expected);
    mutated.unchanged(r.one.witness);
    frozen.unchanged(c.parameters.target.retainedReference);
    CHECK(c.parameters.target.retainedReference.basis == M::RelativeBasis::COMMANDED &&
          c.parameters.target.retainedReference.nativePosition == BASE);
    const int32_t detail = mutationDetail(m);
    if (!detail || when == Window::AFTER_ACK) {
        CHECK(c.state == M::ActionState::SUCCEEDED && c.outcome == M::ActionOutcome::OBSERVED);
        CHECK(r.a.moveRefusal().isOk() && r.wire.preflightRefusals == refusals);
        CHECK(wordsFrom(r, r.startFrame, 0x005f) == 1 && r.drive.edges == 1 && r.drive.endpoint == BASE + STEP);
        return;
    }
    CHECK(c.state == M::ActionState::FAILED && c.outcome == M::ActionOutcome::INVALIDATED);
    CHECK(c.interruption == M::MoveInterruption::INVALIDATED);
    CHECK(r.a.moveRefusal().code == M::Err::INVALID_CONFIG && r.a.moveRefusal().detail == detail);
    CHECK(!wordsFrom(r, r.startFrame, 0x005f) && !r.drive.edges && c.execution == M::ActionExecution::NOT_TRANSMITTED);
    CHECK(!r.a.moveFenced() && !c.uncertain);
    // The trigger is judged at the handover by preflight; every earlier download at its own notice.
    CHECK(r.wire.preflightRefusals == refusals + (when == Window::AT_HANDOVER ? 1u : 0u));
    switch (when) {
    case Window::BEFORE_FIRST:
        // Nothing at all left, so the 000F before the move is still the latest and still true.
        CHECK(r.wire.sentCount == r.startFrame && r.drive.target == FAKE_TARGET);
        CHECK(c.setupEvidenceMask == 1 && !c.setupHistory[0].transmissionPossible);
        CHECK(c.setup == M::ActionExecution::NOT_TRANSMITTED && !c.effectsPossible);
        CHECK(r.a.latestControl());
        control.unchanged(*r.a.latestControl());
        break;
    case Window::MID_SETUP:
        CHECK(c.setupEvidenceMask == 3 && c.setupHistory[0].outcome == C::SdoOutcome::SUCCESS &&
              !c.setupHistory[1].transmissionPossible);
        for (unsigned i = 0; i < 4; ++i)
            CHECK(downloadsFrom(r, r.startFrame, SETUP[i]) == (i ? 0u : 1u));
        CHECK(c.setup == M::ActionExecution::UNKNOWN && c.effectsPossible && r.drive.target == int32_t(STEP));
        break;
    default: // AT_HANDOVER
        CHECK(c.setup == M::ActionExecution::ACKNOWLEDGED && c.setupEvidenceMask == 0xf);
        CHECK(c.triggerPresent && !c.trigger.transmissionPossible && r.drive.target == int32_t(STEP));
        for (unsigned i = 0; i < 4; ++i)
            CHECK(downloadsFrom(r, r.startFrame, SETUP[i]) == 1);
        break;
    }
}
void borrowedMutation() {
    const Window windows[] = {Window::BEFORE_FIRST, Window::MID_SETUP, Window::AT_HANDOVER, Window::AFTER_ACK};
    for (Window when : windows)
        for (Mutation m : ALL_MUTATIONS)
            borrowedCase(m, when);
}
// Without the transport's preflight only the trigger's admission held: a change after its notice is
// not seen, and the trigger leaves (packet 21 D2). Kept as the documented contrast, not a defect.
void unwiredHandover() {
    scenario = "borrowed position, trigger queued, no preflight";
    Rig r;
    r.wire.preflight = false;
    commandedRig(r);
    CHECK(r.relative(commanded(r), commandedEvidence(), commandedRequest(STEP)).isOk());
    toNotedTrigger(r);
    ++r.one.witness.nativePosition;
    finishAll(r);
    const L::MoveContext& c = moveOf(r);
    CHECK(c.state == M::ActionState::SUCCEEDED && c.execution == M::ActionExecution::ACKNOWLEDGED);
    CHECK(r.a.moveRefusal().isOk() && wordsFrom(r, r.startFrame, 0x005f) == 1 && r.drive.edges == 1);
    CHECK(c.parameters.target.retainedReference.nativePosition == BASE && r.one.witness.nativePosition == BASE + 1);
}

// 11. An observation after acceptance that contradicts the commanded base: the drive's own commanded
// position differed from the witness, the drive used its actual position instead, or it reported a
// position far from both while moving. The move reports the controller's completion under its frozen
// evidence and keeps the raw samples; it claims no endpoint, corrects nothing, writes no second edge
// and writes nothing back into the witness. The contradiction is visible only in retained evidence.
enum class Contradiction { COMMANDED_ELSEWHERE, ACTUAL_BASE, POSITION_JUMP };
void contradictionCase(Contradiction k) {
    const char* const names[] = {"drive commanded elsewhere", "drive used actual base", "position jump"};
    scenario = names[unsigned(k)];
    Rig r;
    commandedRig(r);
    r.drive.travelReads = 2; // Cycle 1 accepts, cycle 2 still moves, cycle 3 completes.
    int64_t driveEnd = BASE + STEP;
    if (k == Contradiction::COMMANDED_ELSEWHERE) {
        r.drive.commanded = int32_t(BASE + 100);
        driveEnd = BASE + 100 + STEP;
    } else if (k == Contradiction::ACTUAL_BASE) {
        r.drive.commandedBase = false;
        driveEnd = BASE - 10 + STEP;
    }
    CHECK(r.relative(commanded(r), commandedEvidence(), commandedRequest(STEP)).isOk());
    const L::MoveContext& c = moveOf(r);
    const Saved<L::PreparedPositionParameters> parameters(c.parameters);
    const Saved<L::MoveEvidence> frozen(c.evidence);
    const Saved<M::AxisReference> borrowed(r.one.witness);
    if (k == Contradiction::POSITION_JUMP) {
        for (unsigned i = 0; i < 400 && c.execution != M::ActionExecution::ACKNOWLEDGED; ++i)
            r.step();
        CHECK(c.execution == M::ActionExecution::ACKNOWLEDGED && !c.observations);
        r.drive.position = -999999;
    }
    r.finishMove();
    CHECK(c.state == M::ActionState::SUCCEEDED && c.outcome == M::ActionOutcome::OBSERVED);
    CHECK(c.completion == M::ActionCompletion::OBSERVED && c.setup == M::ActionExecution::ACKNOWLEDGED);
    CHECK(c.execution == M::ActionExecution::ACKNOWLEDGED && c.rearm == M::ActionExecution::ACKNOWLEDGED);
    CHECK(!c.uncertain && !r.a.moveFenced() && r.a.moveRefusal().isOk());
    CHECK(c.acceptance.present && c.completed.present && c.completed.cycle > c.acceptance.cycle);
    CHECK(c.completed.position == driveEnd && r.drive.endpoint == driveEnd);
    if (k == Contradiction::POSITION_JUMP)
        CHECK(c.acceptance.position == -999999 && c.completed.position == c.parameters.target.endpointNative);
    else
        CHECK(c.completed.position != c.parameters.target.endpointNative);
    parameters.unchanged(c.parameters);
    frozen.unchanged(c.evidence);
    borrowed.unchanged(r.one.witness);
    CHECK(c.displacementNative == STEP && c.parameters.target.endpointNative == BASE + STEP);
    CHECK(c.evidence.relativeBase == L::RelativeBase::COMMANDED);
    const unsigned s = r.startFrame;
    CHECK(downloadsFrom(r, s, 0x607a) == 1 && downloadsFrom(r, s, 0x6040) == 2);
    CHECK(wordsFrom(r, s, 0x005f) == 1 && wordsFrom(r, s, 0x000f) == 1 && r.drive.edges == 1);
    CHECK(!downloadsFrom(r, s, 0x6060) && !downloadsFrom(r, s, 0x2301) && !downloadsFrom(r, s, 0x2302));
    quiet(r);
    // The witness predates the rearm, whatever the observation said: a new move needs a new one.
    r.readState(r.state);
    refusedHere(r, r.one.witness, commandedEvidence(), commandedRequest(STEP), M::Err::INVALID_CONFIG,
                moveDetail(M::MoveError::REFERENCE));
}
void contradiction() {
    const Contradiction kinds[] = {Contradiction::COMMANDED_ELSEWHERE, Contradiction::ACTUAL_BASE,
                                   Contradiction::POSITION_JUMP};
    for (Contradiction k : kinds)
        contradictionCase(k);
}

// 12. The admitted bounds on the wire: literal 607A bytes at the documented range's edges, a one-step
// FLOOR displacement, and an endpoint exactly at an I32 axis limit.
void boundsOnWire() {
    scenario = "bounds on the wire";
    const int32_t values[] = {1000000, -1000000};
    const uint8_t bytes[2][8] = {{0x23, 0x7a, 0x60, 0x00, 0x40, 0x42, 0x0f, 0x00},
                                 {0x23, 0x7a, 0x60, 0x00, 0xc0, 0xbd, 0xf0, 0xff}};
    for (unsigned i = 0; i < 2; ++i) {
        Rig r;
        commandedRig(r);
        CHECK(r.relative(commanded(r), commandedEvidence(), commandedRequest(values[i])).isOk());
        r.finishMove();
        const L::MoveContext& c = moveOf(r);
        CHECK(c.state == M::ActionState::SUCCEEDED && c.parameters.targetNative == values[i]);
        CHECK(c.parameters.target.endpointNative == BASE + values[i] && r.drive.endpoint == BASE + values[i]);
        CHECK(frameIs(r, r.startFrame, bytes[i]));
    }
    Rig r;
    commandedRig(r);
    CHECK(r.relative(commanded(r), commandedEvidence(), fractional(-1, 10, M::Rounding::FLOOR)).isOk());
    const M::PreparedTarget& t = moveOf(r).parameters.target;
    CHECK(t.effectiveNative == -1 && t.endpointNative == BASE - 1 && t.quantization.errorNumerator == 9 &&
          t.quantization.errorDenominator == 10 && t.quantization.errorNegative);
    r.finishMove();
    const uint8_t minusOne[8] = {0x23, 0x7a, 0x60, 0x00, 0xff, 0xff, 0xff, 0xff};
    CHECK(moveOf(r).state == M::ActionState::SUCCEEDED && frameIs(r, r.startFrame, minusOne));
    CHECK(r.drive.endpoint == BASE - 1);
    Rig e;
    commandedRig(e);
    e.one.coordinates.nativeMinimum = INT32_MIN;
    e.one.coordinates.nativeMaximum = INT32_MAX;
    e.drive.commanded = INT32_MAX - 1000;
    CHECK(e.relative(commanded(e, int64_t(INT32_MAX) - 1000), commandedEvidence(), commandedRequest(1000)).isOk());
    e.finishMove();
    CHECK(moveOf(e).state == M::ActionState::SUCCEEDED && moveOf(e).parameters.target.endpointNative == INT32_MAX);
    CHECK(e.drive.endpoint == INT32_MAX && downloadAt(e, e.startFrame, 0x607a, 4, 1000));
}

void runAll() {
    directCalls();
    sessionRefusals();
    agesBeforeTrigger();
    generationAfterPreparation();
    borrowedMutation();
    unwiredHandover();
    contradiction();
    boundsOnWire();
}
} // namespace

int main() {
    for (int async = 0; async < 2; ++async) {
        asyncTraffic = async != 0;
        runAll();
    }
    std::printf("move_commanded: %u checks\n", checks);
    return 0;
}
