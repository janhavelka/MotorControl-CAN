// SPDX-License-Identifier: MIT
// The shared move types without any wire protocol: a test-only servo with no full-step geometry, a
// test-only non-CANopen drive on an invented serial frame, and common exact intent vectors compared
// with MotorControl-RS. Nothing here is a drive, a protocol or a measurement.
#include <MotorControlCAN/MoveOperation.h>
#include <cstdio>
#include <cstdlib>

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
// A stepper: 3200 command increments and 200 full steps per motor turn, gear 5.
M::AxisConfig stepper() {
    M::AxisConfig c = servo();
    c.binding = binding(M::Protocol::CANOPEN);
    c.units.commandStepsPerMotorTurn = M::UnitScale(3200, 1, M::ScaleSource::QUALIFIED);
    c.units.motorTurnsPerLoadTurn = M::UnitScale(5, 1, M::ScaleSource::QUALIFIED);
    c.units.fullStepsPerMotorTurn = M::UnitScale(200, 1, M::ScaleSource::QUALIFIED);
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

// Common intent vectors. Every field is set: the defaults differ between the libraries
// (PositionRequest.relative is true in RS and false here, the allowance is double there and
// Rational here). Expected values are RS's preparePosition at MotorControl-RS a13f767
// (src/Axis.cpp), computed by a scratch build for packet 21; no device frame is involved.
struct Vector {
    const char* name;
    bool servoAxis;
    int64_t numerator, denominator;
    M::PositionUnit unit;
    M::CoordinateFrame frame;
    bool relative;
    M::Rounding rounding;
    int64_t allowance; // Halves of a native increment.
    int64_t witness, effective, endpoint;
    int64_t errorThirds; // RS roundingError times 6, so thirds and halves are both exact.
};
void intentVectors() {
    const Vector vectors[] = {
        {"2500 steps", false, 2500, 1, M::PositionUnit::STEPS, M::CoordinateFrame::NATIVE, true,
          M::Rounding::EXACT, 0, 1000, 2500, 3500, 0},
        {"+1/3 motor turn", false, 1, 3, M::PositionUnit::TURNS, M::CoordinateFrame::MOTOR, true,
          M::Rounding::NEAREST, 1, 50, 1067, 1117, 2},
        {"-1/3 motor turn", false, -1, 3, M::PositionUnit::TURNS, M::CoordinateFrame::MOTOR, true,
          M::Rounding::NEAREST, 1, 50, -1067, -1017, -2},
        {"1/5 load turn", false, 1, 5, M::PositionUnit::TURNS, M::CoordinateFrame::LOAD, true,
          M::Rounding::EXACT, 0, 0, 3200, 3200, 0},
        {"90 load degrees", false, 90, 1, M::PositionUnit::DEGREES, M::CoordinateFrame::LOAD, true,
          M::Rounding::EXACT, 0, -100, 4000, 3900, 0},
        {"7/2 steps", false, 7, 2, M::PositionUnit::STEPS, M::CoordinateFrame::NATIVE, true,
          M::Rounding::NEAREST, 1, 0, 4, 4, 3},
        {"5/2 steps", false, 5, 2, M::PositionUnit::STEPS, M::CoordinateFrame::NATIVE, true,
          M::Rounding::NEAREST, 1, 0, 2, 2, -3},
        {"1 full step", false, 1, 1, M::PositionUnit::FULL_STEPS, M::CoordinateFrame::MOTOR, true,
          M::Rounding::EXACT, 0, 0, 16, 16, 0},
        {"servo 1/4 turn", true, 1, 4, M::PositionUnit::TURNS, M::CoordinateFrame::MOTOR, true,
          M::Rounding::EXACT, 0, 0, 2500, 2500, 0},
        {"servo -1/3 turn", true, -1, 3, M::PositionUnit::TURNS, M::CoordinateFrame::MOTOR, true,
          M::Rounding::NEAREST, 1, 0, -3333, -3333, 2},
        {"absolute 1/3 motor turn", false, 1, 3, M::PositionUnit::TURNS, M::CoordinateFrame::MOTOR, false,
          M::Rounding::NEAREST, 1, 0, 1067, 1067, 2},
    };
    for (const Vector& v : vectors) {
        const M::AxisConfig axis = v.servoAxis ? servo() : stepper();
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
        if (!M::preparePosition(r, axis, &w, t)) {
            std::fprintf(stderr, "vector %s refused\n", v.name);
            CHECK(false);
        }
        CHECK(t.effectiveNative == v.effective && t.endpointNative == v.endpoint);
        const int64_t magnitude = t.quantization.errorNumerator
                                      ? int64_t(t.quantization.errorNumerator) * 6 /
                                            int64_t(t.quantization.errorDenominator)
                                      : 0;
        CHECK((t.quantization.errorNegative ? -magnitude : magnitude) == v.errorThirds);
    }
    // RS also refuses a full-step request on the servo, which has no full-step geometry.
    M::PositionRequest r;
    r.value = M::Rational(1);
    r.unit = M::PositionUnit::FULL_STEPS;
    r.frame = M::CoordinateFrame::MOTOR;
    r.relative = true;
    r.basis = M::RelativeBasis::ACTUAL;
    r.configurationGeneration = 5;
    r.rounding = M::Rounding::EXACT;
    const M::AxisConfig axis = servo();
    const M::AxisReference w = witness(axis, 0);
    M::PreparedTarget t;
    CHECK(!M::preparePosition(r, axis, &w, t));
}
} // namespace

int main() {
    servoWithoutFullSteps();
    intentVectors();
    std::printf("move_common: %u checks\n", checks);
    return 0;
}
