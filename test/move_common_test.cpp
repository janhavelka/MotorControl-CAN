// SPDX-License-Identifier: MIT
// The shared move types without any wire protocol: a test-only servo with no full-step geometry, a
// test-only non-CANopen drive on an invented serial frame, and common exact intent vectors retained
// from MotorControl-RS in vectors/rs_common_intent.inc. Nothing here is a drive, a protocol or a
// measurement.
#include <MotorControlCAN/MoveOperation.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>

// Type neutrality at compile time: MoveOperation.h must declare no CANopen or Lichuan name. If any
// header it includes ever did, these internal-linkage sentinels would collide and this file would
// not compile.
namespace MotorControlCAN {
static const int CANOPEN = 0;
static const int LICHUAN_C = 0;
static_assert(CANOPEN == 0 && LICHUAN_C == 0, "sentinels for a protocol-free MoveOperation.h");
} // namespace MotorControlCAN

namespace M = MotorControlCAN;
static unsigned checks = 0;
#define CHECK(condition)                                                                           \
    do {                                                                                           \
        ++checks;                                                                                  \
        if (!(condition)) {                                                                        \
            std::fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #condition);                    \
            std::abort();                                                                          \
        }                                                                                          \
    } while (false)

namespace {
M::Binding binding(M::Protocol protocol) {
    M::Binding b;
    b.logicalAxisId = 31;
    b.endpoint.protocol = protocol;
    b.endpoint.profile = 77;
    b.endpoint.address = 4;
    b.bindingGeneration = 2;
    b.configurationGeneration = 5;
    return b;
}
// A servo: 10000 command increments per motor turn, direct drive, and no full-step geometry at all.
// Its native rate is command increments per second.
M::AxisConfig servo() {
    M::AxisConfig c;
    c.binding = binding(M::Protocol::PROPRIETARY);
    c.units.commandStepsPerMotorTurn = M::UnitScale(10000, 1, M::ScaleSource::QUALIFIED);
    c.units.motorTurnsPerLoadTurn = M::UnitScale(1, 1, M::ScaleSource::QUALIFIED);
    c.supportedRelativeBases = 1;
    c.originKnown = true;
    c.originSource = M::ScaleSource::QUALIFIED;
    return c;
}
M::AxisReference witness(const M::AxisConfig& c, int64_t position) {
    M::AxisReference w;
    w.binding = c.binding;
    w.referenceGeneration = c.referenceGeneration;
    w.idle = w.stationary = w.nativeKnown = true;
    w.nativePosition = position;
    w.basis = M::RelativeBasis::ACTUAL;
    w.source = M::ScaleSource::QUALIFIED;
    w.observedUs = w.nowUs = 1000;
    w.maximumAgeUs = 1000;
    return w;
}

// A test-only drive on an invented serial protocol with no NMT, SDO or statusword: one frame 'R' and
// a little-endian int32 displacement, acknowledged by one byte. It uses the shared types unchanged,
// and its own relative base is a switch, as unmeasured as the Lichuan one.
struct SerialMove {
    M::MoveRequest intent;
    M::PreparedTarget target;
    M::PreparedVelocityTarget speed;
    M::MoveDisplacement displacement = M::MoveDisplacement::UNKNOWN;
    int64_t displacementNative = 0;
    M::ActionState state = M::ActionState::EMPTY;
    M::ActionExecution execution = M::ActionExecution::NOT_TRANSMITTED;
    M::ActionCompletion completion = M::ActionCompletion::NOT_OBSERVED;
    M::ActionOutcome outcome = M::ActionOutcome::NONE;
    M::MoveInterruption interruption = M::MoveInterruption::NONE;
    uint8_t frame[5] = {};
};
M::Status prepareSerialMove(SerialMove& output, const M::MoveRequest& request, const M::AxisConfig& axis,
                            const M::AxisReference& reference, bool baseMeasured) {
    if (!request.position.relative || request.position.wrapped)
        return M::Status(M::Err::INVALID_CONFIG, int32_t(M::MoveError::ARGUMENT), "relative intent only");
    if (!baseMeasured)
        return M::Status(M::Err::UNSUPPORTED, int32_t(M::MoveError::RELATIVE_BASIS), "base unmeasured");
    SerialMove candidate;
    M::Status s = M::preparePosition(request.position, axis, &reference, candidate.target);
    if (!s) {
        s.detail += int32_t(M::MoveError::PARAMETER_ERROR_BASE);
        return s;
    }
    s = M::prepareVelocityTarget(request.speed, axis, candidate.speed);
    if (!s) {
        s.detail += int32_t(M::MoveError::RATE_ERROR_BASE);
        return s;
    }
    if (!candidate.target.effectiveNative)
        return M::Status(M::Err::ILLEGAL_VALUE, int32_t(M::MoveError::ZERO_DISPLACEMENT), "zero");
    if (candidate.target.effectiveNative < INT32_MIN || candidate.target.effectiveNative > INT32_MAX)
        return M::Status(M::Err::ILLEGAL_VALUE, int32_t(M::MoveError::RANGE), "frame range");
    candidate.intent = request;
    candidate.displacement = M::MoveDisplacement::KNOWN;
    candidate.displacementNative = candidate.target.effectiveNative;
    candidate.state = M::ActionState::ACTIVE;
    const uint32_t raw = uint32_t(int32_t(candidate.target.effectiveNative));
    candidate.frame[0] = 'R';
    for (unsigned i = 0; i < 4; ++i)
        candidate.frame[1 + i] = uint8_t(raw >> (8 * i));
    output = candidate;
    return M::Ok();
}
void interruptSerialMove(SerialMove& m, M::MoveInterruption reason) {
    m.interruption = reason;
    m.state = M::ActionState::FAILED;
    m.outcome = reason == M::MoveInterruption::DRIVE_FAULT   ? M::ActionOutcome::DRIVE_FAULT
                : reason == M::MoveInterruption::INVALIDATED ? M::ActionOutcome::INVALIDATED
                : reason == M::MoveInterruption::DEADLINE    ? M::ActionOutcome::DEADLINE
                                                             : M::ActionOutcome::CANCELLED;
}

void servoWithoutFullSteps() {
    const M::AxisConfig axis = servo();
    CHECK(M::validateAxisConfig(axis));
    CHECK(axis.units.fullStepsPerMotorTurn.source == M::ScaleSource::UNKNOWN);
    M::MoveRequest request;
    request.position.value = M::Rational(1, 4);
    request.position.unit = M::PositionUnit::TURNS;
    request.position.frame = M::CoordinateFrame::MOTOR;
    request.position.relative = true;
    request.position.basis = M::RelativeBasis::ACTUAL;
    request.position.configurationGeneration = 5;
    request.position.rounding = M::Rounding::EXACT;
    request.speed.value = M::Rational(60);
    request.speed.unit = M::VelocityUnit(M::PositionUnit::TURNS, M::TimeUnit::MINUTE);
    request.speed.frame = M::CoordinateFrame::MOTOR;
    request.speed.configurationGeneration = 5;
    SerialMove move;
    CHECK(!prepareSerialMove(move, request, axis, witness(axis, 100), false).isOk());
    CHECK(move.state == M::ActionState::EMPTY);
    CHECK(prepareSerialMove(move, request, axis, witness(axis, 100), true));
    CHECK(move.displacement == M::MoveDisplacement::KNOWN && move.displacementNative == 2500);
    CHECK(move.target.endpointNative == 2600 && move.speed.effectiveNative == 10000);
    CHECK(move.speed.rateBasis == M::NativeRateBasis::COMMAND_INCREMENTS && move.speed.rateTime == M::TimeUnit::SECOND);
    const uint8_t frame[5] = {'R', 0xc4, 0x09, 0x00, 0x00};
    for (unsigned i = 0; i < 5; ++i)
        CHECK(move.frame[i] == frame[i]);
    // A full-step request needs full-step geometry, which this servo does not have.
    SerialMove other;
    M::MoveRequest steps = request;
    steps.position.value = M::Rational(1);
    steps.position.unit = M::PositionUnit::FULL_STEPS;
    const M::Status s = prepareSerialMove(other, steps, axis, witness(axis, 100), true);
    CHECK(!s && s.detail > int32_t(M::MoveError::PARAMETER_ERROR_BASE) && other.state == M::ActionState::EMPTY);
    // Zero displacement means the same in any protocol.
    M::MoveRequest zero = request;
    zero.position.value = M::Rational(0);
    CHECK(prepareSerialMove(other, zero, axis, witness(axis, 100), true).detail ==
          int32_t(M::MoveError::ZERO_DISPLACEMENT));
    // The shared interruption reasons end it with their shared outcomes, and none is a motor command.
    interruptSerialMove(move, M::MoveInterruption::STOP);
    CHECK(move.outcome == M::ActionOutcome::CANCELLED && move.interruption == M::MoveInterruption::STOP);
    CHECK(move.execution == M::ActionExecution::NOT_TRANSMITTED);
}

// Common intent vectors, retained with their exact inputs in vectors/rs_common_intent.inc. Its
// expected values are what MotorControl-RS preparePosition (src/Axis.cpp) returns at the revision
// and source blobs that file records; scripts/rs_move_vectors.py recompiles that RS source read-only
// to check them. This test needs neither the sibling checkout nor the network, and no device frame
// is involved. Every request field is set: the defaults differ between the libraries
// (PositionRequest.relative is true in RS and false here, the allowance is double there and
// Rational here).
struct VectorAxis {
    const char* name;
    uint32_t steps, stepsDenominator, gear, gearDenominator, fullSteps, fullStepsDenominator;
    int64_t origin;
};
const VectorAxis vectorAxes[] = {
#define RS_MOVE_AXIS(name, steps, stepsDen, gear, gearDen, full, fullDen, origin)                       \
    {name, steps, stepsDen, gear, gearDen, full, fullDen, origin},
#include "vectors/rs_common_intent.inc"
};
// The retained scales and origin on the shared fixture's binding, generation, origin provenance
// and ACTUAL relative basis; the protocol in the binding does not enter this arithmetic.
bool vectorAxis(const char* name, M::AxisConfig& c) {
    for (const VectorAxis& a : vectorAxes) {
        if (std::strcmp(a.name, name))
            continue;
        c = servo();
        c.units.commandStepsPerMotorTurn = M::UnitScale(a.steps, a.stepsDenominator, M::ScaleSource::QUALIFIED);
        c.units.motorTurnsPerLoadTurn = M::UnitScale(a.gear, a.gearDenominator, M::ScaleSource::QUALIFIED);
        c.units.fullStepsPerMotorTurn =
            a.fullSteps ? M::UnitScale(a.fullSteps, a.fullStepsDenominator, M::ScaleSource::QUALIFIED) : M::UnitScale();
        c.originNative = a.origin;
        return true;
    }
    return false;
}
struct Vector {
    const char* name;
    const char* axis;
    int64_t numerator;
    uint64_t denominator;
    M::PositionUnit unit;
    M::CoordinateFrame frame;
    bool relative;
    M::Rounding rounding;
    int64_t allowance; // Halves of a native increment.
    int64_t witness;
    bool refused; // RS refuses it; then effective, endpoint and error are unused.
    int64_t effective, endpoint;
    int64_t errorSixths; // RS roundingError times 6, so thirds and halves are both exact.
};
const Vector vectors[] = {
#define RS_MOVE_VECTOR(name, axis, n, d, unit, frame, relative, rounding, allowance, witness, effective,     \
                       endpoint, sixths)                                                                  \
    {name, axis, n, d, M::PositionUnit::unit, M::CoordinateFrame::frame, relative, M::Rounding::rounding,    \
     allowance, witness, false, effective, endpoint, sixths},
#define RS_MOVE_REFUSAL(name, axis, n, d, unit, frame, relative, rounding, allowance, witness, code, detail) \
    {name, axis, n, d, M::PositionUnit::unit, M::CoordinateFrame::frame, relative, M::Rounding::rounding,    \
     allowance, witness, true, 0, 0, 0},
#include "vectors/rs_common_intent.inc"
};
void intentVectors() {
    for (const Vector& v : vectors) {
        M::AxisConfig axis;
        CHECK(vectorAxis(v.axis, axis));
        M::PositionRequest r;
        r.value = M::Rational(v.numerator, v.denominator);
        r.unit = v.unit;
        r.frame = v.frame;
        r.relative = v.relative;
        r.wrapped = false;
        r.path = M::AnglePath::SHORTEST;
        r.tie = M::HalfTurnTie::REJECT;
        r.basis = M::RelativeBasis::ACTUAL;
        r.referencePolicy = M::ReferencePolicy::OPTIONAL;
        r.configurationGeneration = axis.binding.configurationGeneration;
        r.rounding = v.rounding;
        r.maximumQuantizationError = M::Rational(v.allowance, 2);
        r.approximate = false;
        r.maximumApproximationError = 0;
        const M::AxisReference w = witness(axis, v.witness);
        M::PreparedTarget t;
        const M::Status s = M::preparePosition(r, axis, &w, t);
        if (s.isOk() == v.refused)
            std::fprintf(stderr, "vector %s: RS %s it, CAN %s it\n", v.name, v.refused ? "refuses" : "accepts",
                         s.isOk() ? "accepts" : "refuses");
        CHECK(s.isOk() != v.refused);
        if (v.refused)
            continue;
        CHECK(t.effectiveNative == v.effective && t.endpointNative == v.endpoint);
        CHECK(t.quantization.errorNumerator * 6 % t.quantization.errorDenominator == 0);
        const int64_t magnitude = t.quantization.errorNumerator
                                      ? int64_t(t.quantization.errorNumerator) * 6 /
                                            int64_t(t.quantization.errorDenominator)
                                      : 0;
        CHECK((t.quantization.errorNegative ? -magnitude : magnitude) == v.errorSixths);
    }
}
} // namespace

int main() {
    servoWithoutFullSteps();
    intentVectors();
    std::printf("move_common: %u checks\n", checks);
    return 0;
}
