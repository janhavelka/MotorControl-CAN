// SPDX-License-Identifier: MIT
// Application intent only: independent unit vectors, no controller, queue or device defaults.
#include "MotionIntent.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>

#define CHECK(condition)                                                                           \
    do {                                                                                           \
        if (!(condition)) {                                                                        \
            std::fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #condition);                   \
            std::abort();                                                                          \
        }                                                                                          \
    } while (false)
namespace M = MotorControlCAN;
namespace L = MotorControlCAN::LICHUAN_C;
namespace E = MotorControlCANExample;
namespace {
template <typename T> struct Saved {
    unsigned char bytes[sizeof(T)];
    explicit Saved(const T& value) { std::memcpy(bytes, &value, sizeof value); }
    bool unchanged(const T& value) const { return std::memcmp(bytes, &value, sizeof value) == 0; }
};
M::AxisConfig axis() {
    M::AxisConfig c;
    c.binding.logicalAxisId = 17;
    c.binding.endpoint.profile = 3;
    c.binding.endpoint.address = 2;
    c.binding.bindingGeneration = 5;
    c.binding.configurationGeneration = 9;
    c.supportedRelativeBases = 1;
    c.nativeVelocityTime = M::TimeUnit::MINUTE;
    c.nativeRateBasis = M::NativeRateBasis::MOTOR_TURNS;
    c.units.commandStepsPerMotorTurn = M::UnitScale(3200, 1, M::ScaleSource::QUALIFIED);
    c.units.motorTurnsPerLoadTurn = M::UnitScale(5, 1, M::ScaleSource::QUALIFIED);
    return c;
}
M::VelocityRequest rpm(const M::AxisConfig& c, int64_t n, uint64_t d = 1) {
    M::VelocityRequest request;
    request.configurationGeneration = c.binding.configurationGeneration;
    request.value = M::Rational(n, d);
    request.unit = M::VelocityUnit(M::PositionUnit::TURNS, M::TimeUnit::MINUTE);
    return request;
}
M::PositionRequest relative(const M::AxisConfig& c) {
    M::PositionRequest p;
    p.configurationGeneration = c.binding.configurationGeneration;
    p.value = M::Rational(100);
    p.relative = true;
    p.basis = M::RelativeBasis::ACTUAL;
    return p;
}
E::SimpleMoveIntent intent(const M::AxisConfig& c) {
    E::SimpleMoveIntent i;
    CHECK(E::setSimpleSpeed(i, rpm(c, 60), c));
    CHECK(E::setSimpleRamp(i, true, M::Rational(100)));
    CHECK(E::setSimpleRamp(i, false, M::Rational(200)));
    return i;
}
void simpleCoordinatePolicy() {
    M::AxisConfig c = axis();
    c.originKnown = true; c.originNative = 100; c.originSource = M::ScaleSource::QUALIFIED;
    c.units.millimetresPerLoadTurn = M::UnitScale(10, 1, M::ScaleSource::QUALIFIED);
    M::PositionRequest p;
    M::PreparedTarget t;
    CHECK(E::makeSimplePosition(M::Rational(247, 2), M::PositionUnit::STEPS, false, 9, p));
    CHECK(p.frame == M::CoordinateFrame::MOTOR && p.rounding == M::Rounding::NEAREST);
    CHECK(M::preparePosition(p, c, nullptr, t));
    CHECK(t.effectiveNative == 224 && t.quantization.errorNumerator == 1 && !t.quantization.errorNegative);
    CHECK(E::makeSimplePosition(M::Rational(245, 2), M::PositionUnit::STEPS, false, 9, p));
    CHECK(M::preparePosition(p, c, nullptr, t) && t.effectiveNative == 222 && t.quantization.errorNegative);
    CHECK(E::makeSimplePosition(M::Rational(1), M::PositionUnit::TURNS, false, 9, p));
    CHECK(M::preparePosition(p, c, nullptr, t) && t.effectiveNative == 3300);
    CHECK(E::makeSimplePosition(M::Rational(5), M::PositionUnit::MILLIMETRES, false, 9, p));
    CHECK(p.frame == M::CoordinateFrame::LOAD);
    CHECK(M::preparePosition(p, c, nullptr, t) && t.effectiveNative == 8100);
    CHECK(E::makeSimplePosition(M::Rational(1), M::PositionUnit::STEPS, true, 9, p));
    CHECK(p.frame == M::CoordinateFrame::NATIVE && p.relative);
    const Saved<M::PositionRequest> saved(p);
    CHECK(!E::makeSimplePosition(M::Rational(1), M::PositionUnit::RADIANS, true, 9, p));
    CHECK(!E::makeSimplePosition(M::Rational(1, 0), M::PositionUnit::STEPS, true, 9, p));
    CHECK(!E::makeSimplePosition(M::Rational(1), M::PositionUnit::STEPS, true, 0, p));
    CHECK(saved.unchanged(p));
}
void missingFactsAndAtomicOutputs() {
    M::AxisConfig c = axis();
    E::SimpleMoveIntent i;
    M::MoveRequest out;
    out.position.value = M::Rational(321);
    L::MoveProfile profile;
    profile.rampUpTimeMs = 765;
    const Saved<M::MoveRequest> moveSaved(out);
    const Saved<L::MoveProfile> profileSaved(profile);
    const M::PositionRequest p = relative(c);
    CHECK(!E::buildSimpleMove(i, p, c, false, out, profile));
    CHECK(E::setSimpleSpeed(i, rpm(c, 60), c));
    CHECK(!E::buildSimpleMove(i, p, c, false, out, profile));
    CHECK(E::setSimpleRamp(i, true, M::Rational(100)));
    CHECK(!E::buildSimpleMove(i, p, c, false, out, profile));
    CHECK(E::setSimpleRamp(i, false, M::Rational(200)));
    c.units.commandStepsPerMotorTurn = M::UnitScale();
    CHECK(!E::buildSimpleMove(i, p, c, false, out, profile));
    CHECK(moveSaved.unchanged(out) && profileSaved.unchanged(profile));
    c = axis();
    CHECK(E::buildSimpleMove(i, p, c, false, out, profile));
    CHECK(out.position.value.numerator == 100 && out.position.relative);
    CHECK(out.speed.value.numerator == 60 && profile.rampUpTimeMs == 100 &&
          profile.rampDownTimeMs == 200);
    CHECK(!c.originKnown); // Relative intent does not invent a zero.
    M::PositionRequest absolute = p;
    absolute.relative = false;
    const Saved<M::MoveRequest> relativeSaved(out);
    CHECK(!E::buildSimpleMove(i, absolute, c, true, out, profile));
    CHECK(relativeSaved.unchanged(out));
    c.originKnown = true;
    c.originNative = 42;
    c.originSource = M::ScaleSource::QUALIFIED;
    CHECK(E::buildSimpleMove(i, absolute, c, true, out, profile));
    CHECK(!out.position.relative && c.originNative == 42);
    M::PositionRequest wrong = p;
    wrong.basis = M::RelativeBasis::COMMANDED;
    CHECK(!E::buildSimpleMove(i, wrong, c, false, out, profile));
    wrong = p;
    wrong.configurationGeneration++;
    CHECK(!E::buildSimpleMove(i, wrong, c, false, out, profile));
    wrong = p;
    wrong.wrapped = true;
    CHECK(!E::buildSimpleMove(i, wrong, c, false, out, profile));
    CHECK(!E::buildSimpleMove(i, p, c, true, out, profile));
    ++c.binding.configurationGeneration;
    absolute.configurationGeneration = c.binding.configurationGeneration;
    CHECK(!E::buildSimpleMove(i, absolute, c, true, out, profile)); // Speed epoch is stale too.
}
void setterBoundariesAndScale() {
    const M::AxisConfig c = axis();
    E::SimpleMoveIntent i = intent(c);
    CHECK(E::setSimpleRamp(i, true, M::Rational(4000, 2)));
    CHECK(i.rampUpMs == 2000);
    const Saved<E::SimpleMoveIntent> saved(i);
    CHECK(!E::setSimpleRamp(i, true, M::Rational(1, 2)));
    CHECK(!E::setSimpleRamp(i, false, M::Rational(-1)));
    CHECK(!E::setSimpleRamp(i, false, M::Rational(2001)));
    CHECK(!E::setSimpleRamp(i, false, M::Rational(1, 0)));
    CHECK(!E::setSimpleSpeed(i, rpm(c, -1), c));
    CHECK(!E::setSimpleSpeed(i, rpm(c, 3001), c));
    CHECK(!E::setSimpleSpeed(i, rpm(c, INT64_C(9007199254740993)), c));
    CHECK(!E::setSimpleSetup(i, static_cast<L::MoveSetup>(255)));
    CHECK(saved.unchanged(i));
    CHECK(E::setSimpleSpeed(i, rpm(c, 3000), c));
    CHECK(E::setSimpleSetup(i, L::MoveSetup::REUSE_STORED));
    M::MoveRequest out;
    L::MoveProfile profile;
    CHECK(E::buildSimpleMove(i, relative(c), c, false, out, profile));
    CHECK(profile.setup == L::MoveSetup::REUSE_STORED);
    CHECK(E::setSimpleSpeed(i, rpm(c, 0), c));
    CHECK(!E::buildSimpleMove(i, relative(c), c, false, out, profile));
    CHECK(E::setSimpleSpeed(i, rpm(c, 60), c));
    CHECK(E::setSimpleRamp(i, true, M::Rational(0)));
    CHECK(!E::buildSimpleMove(i, relative(c), c, false, out, profile));
    M::UnitScale scale(7, 3, M::ScaleSource::READBACK);
    const Saved<M::UnitScale> scaleSaved(scale);
    CHECK(!E::makeCommandScale(M::Rational(0), scale));
    CHECK(!E::makeCommandScale(M::Rational(-1), scale));
    CHECK(!E::makeCommandScale(M::Rational(1, 0), scale));
    CHECK(!E::makeCommandScale(M::Rational(int64_t(UINT32_MAX) + 1), scale));
    CHECK(!E::makeCommandScale(M::Rational(1, uint64_t(UINT32_MAX) + 1), scale));
    CHECK(scaleSaved.unchanged(scale));
    CHECK(E::makeCommandScale(M::Rational(6400, 2), scale));
    CHECK(scale.numerator == 3200 && scale.denominator == 1 &&
          scale.source == M::ScaleSource::ASSUMED);
    CHECK(E::makeCommandScale(M::Rational(UINT32_MAX), scale));
    CHECK(scale.numerator == UINT32_MAX);
}
void nativeDestinationAndSingleQuantization() {
    M::AxisConfig c = axis();
    M::VelocityRequest r = rpm(c, 1);
    r.frame = M::CoordinateFrame::LOAD;
    r.rounding = M::Rounding::NEAREST;
    r.maximumQuantizationError = M::Rational(1, 2);
    c.nativeVelocityTime = M::TimeUnit::SECOND;
    c.nativeRateBasis = M::NativeRateBasis::COMMAND_INCREMENTS;
    M::PreparedVelocityTarget arithmetic;
    CHECK(M::prepareVelocityTarget(r, c, arithmetic));
    CHECK(arithmetic.effectiveNative == 267); // 3200 * 5 / 60, in-range but NOT rpm.
    M::FiniteVelocityRequest run;
    run.durationUs = 4321;
    M::PreparedVelocityTarget preview;
    preview.effectiveNative = 8765;
    const Saved<M::FiniteVelocityRequest> runSaved(run);
    const Saved<M::PreparedVelocityTarget> previewSaved(preview);
    CHECK(!E::makeFiniteVelocity(r, 100, c, run, &preview));
    CHECK(runSaved.unchanged(run) && previewSaved.unchanged(preview));
    c = axis();
    r = rpm(c, 60);
    CHECK(E::makeFiniteVelocity(r, 100, c, run, &preview));
    CHECK(run.durationUs == 100000 && preview.effectiveNative == 60);
    c.nativeVelocityTime = M::TimeUnit::SECOND;
    CHECK(!E::makeFiniteVelocity(r, 100, c,
                                 run)); // rpm/native is invalid for a per-second native dimension.
    c = axis();
    c.velocityRepresentation = M::NativeRepresentation::BINARY32;
    CHECK(!E::makeFiniteVelocity(r, 100, c, run));
    c = axis();
    r = rpm(c, 5, 2);
    r.rounding = M::Rounding::NEAREST;
    r.maximumQuantizationError = M::Rational(1, 2);
    CHECK(E::makeFiniteVelocity(r, 50, c, run, &preview));
    CHECK(run.velocity.value.numerator == 5 && run.velocity.value.denominator == 2);
    CHECK(preview.effectiveNative == 2 && preview.quantization.errorNegative);
    CHECK(preview.quantization.errorNumerator == 1 && preview.quantization.errorDenominator == 2);
    CHECK(M::prepareVelocityTarget(run.velocity, c, arithmetic));
    CHECK(arithmetic.effectiveNative == 2 && arithmetic.quantization.errorNumerator == 1);
    const Saved<M::FiniteVelocityRequest> exactSaved(run);
    r.maximumQuantizationError = M::Rational(49, 100);
    CHECK(!E::makeFiniteVelocity(r, 50, c, run));
    CHECK(exactSaved.unchanged(run));
}
void finiteBoundsAndSignedIntent() {
    const M::AxisConfig c = axis();
    M::FiniteVelocityRequest run;
    M::PreparedVelocityTarget preview;
    CHECK(E::makeFiniteVelocity(rpm(c, -3000), 1, c, run, &preview));
    CHECK(preview.effectiveNative ==
          -3000); // Arithmetic only; reverse qualification is not supplied here.
    CHECK(!E::makeFiniteVelocity(rpm(c, -3001), 1, c, run));
    CHECK(!E::makeFiniteVelocity(rpm(c, 3001), 1, c, run));
    M::VelocityRequest outside = rpm(c, 6001, 2);
    outside.rounding = M::Rounding::NEAREST;
    outside.maximumQuantizationError = M::Rational(1, 2);
    CHECK(M::prepareVelocityTarget(outside, c, preview));
    CHECK(preview.effectiveNative == 3000);
    CHECK(!E::makeFiniteVelocity(outside, 1, c, run)); // In-range rounding cannot admit 3000.5 rpm.
    outside.value.numerator = -6001;
    CHECK(M::prepareVelocityTarget(outside, c, preview));
    CHECK(preview.effectiveNative == -3000);
    CHECK(!E::makeFiniteVelocity(outside, 1, c, run));
    CHECK(!E::makeFiniteVelocity(rpm(c, INT64_MIN), 1, c, run));
    CHECK(!E::makeFiniteVelocity(rpm(c, 0), 1, c, run));
    CHECK(!E::makeFiniteVelocity(rpm(c, 60), 0, c, run));
    CHECK(!E::makeFiniteVelocity(rpm(c, 60), UINT64_MAX, c, run));
    const uint64_t limit = uint64_t(INT64_MAX) / 1000u;
    CHECK(E::makeFiniteVelocity(rpm(c, 60), limit, c, run));
    CHECK(run.durationUs == limit * 1000u);
    CHECK(!E::makeFiniteVelocity(rpm(c, 60), limit + 1, c, run));
}
} // namespace
int main() {
    simpleCoordinatePolicy();
    missingFactsAndAtomicOutputs();
    setterBoundariesAndScale();
    nativeDestinationAndSingleQuantization();
    finiteBoundsAndSignedIntent();
    std::puts("motion_intent_test: passed");
}
