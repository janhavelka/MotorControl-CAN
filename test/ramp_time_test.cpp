// SPDX-License-Identifier: MIT
// prepareRampTime against hand-computed tables; no controller, profile or device traffic.
// Every expected value below is worked out by hand in the comment beside it, never taken from
// the code under test.
#include <MotorControlCAN/Axis.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>

#define CHECK(condition) do { if (!(condition)) { std::fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #condition); std::abort(); } } while (false)

namespace M = MotorControlCAN;

namespace {
template <typename T> struct Saved {
    unsigned char bytes[sizeof(T)];
    explicit Saved(const T& value) { std::memcpy(bytes, &value, sizeof value); }
    bool unchanged(const T& value) const { return std::memcmp(bytes, &value, sizeof value) == 0; }
};

const int32_t unitBase = static_cast<int32_t>(M::AxisError::UNIT_ERROR_BASE);

bool failedWith(const M::Status& status, M::Err code, int32_t detail) {
    return !status && status.code == code && status.detail == detail;
}
bool failedWith(const M::Status& status, M::Err code, M::AxisError error) {
    return failedWith(status, code, static_cast<int32_t>(error));
}
bool failedWith(const M::Status& status, M::Err code, M::UnitError error) {
    return failedWith(status, code, unitBase + static_cast<int32_t>(error));
}
bool equal(const M::Rational& value, int64_t n, uint64_t d) {
    return value.numerator == n && value.denominator == d;
}

M::UnitScale scale(uint32_t n, uint32_t d = 1) { return M::UnitScale(n, d, M::ScaleSource::QUALIFIED); }

// A motor-rpm axis (native rate = motor turns per minute) behind a 5 mm lead screw, direct drive.
M::AxisConfig rpmAxis(uint32_t gear = 1) {
    M::AxisConfig c;
    c.binding.logicalAxisId = 7;
    c.binding.endpoint.profile = 17;
    c.binding.endpoint.address = 1;
    c.binding.bindingGeneration = 2;
    c.binding.configurationGeneration = 3;
    c.nativeRateBasis = M::NativeRateBasis::MOTOR_TURNS;
    c.nativeVelocityTime = M::TimeUnit::MINUTE;
    c.units.motorTurnsPerLoadTurn = scale(gear);
    c.units.millimetresPerLoadTurn = scale(5);
    return c;
}

M::PreparedVelocityTarget speed(const M::AxisConfig& c, int64_t n, uint64_t d = 1,
    M::VelocityUnit unit = M::VelocityUnit(M::PositionUnit::MILLIMETRES, M::TimeUnit::SECOND),
    M::CoordinateFrame frame = M::CoordinateFrame::LOAD) {
    M::VelocityRequest r;
    r.configurationGeneration = c.binding.configurationGeneration;
    r.value = M::Rational(n, d); r.unit = unit; r.frame = frame;
    M::PreparedVelocityTarget out;
    CHECK(M::prepareVelocityTarget(r, c, out));
    return out;
}

M::AccelerationRequest accel(const M::AxisConfig& c, int64_t n, uint64_t d = 1,
    M::AccelerationUnit unit = M::AccelerationUnit(M::PositionUnit::MILLIMETRES),
    M::CoordinateFrame frame = M::CoordinateFrame::LOAD, M::Rounding rounding = M::Rounding::EXACT) {
    M::AccelerationRequest r;
    r.configurationGeneration = c.binding.configurationGeneration;
    r.value = M::Rational(n, d); r.unit = unit; r.frame = frame; r.rounding = rounding;
    r.maximumAccelerationError = M::Rational(1000000);
    return r;
}

const M::AccelerationUnit rpmPerSecond(M::PositionUnit::TURNS, M::TimeUnit::MINUTE, M::TimeUnit::SECOND);
const M::AccelerationUnit mmPerSecond2(M::PositionUnit::MILLIMETRES, M::TimeUnit::SECOND, M::TimeUnit::SECOND);

// 5 mm lead: 50 mm/s is 10 turns/s = 600 rpm. 500 mm/s^2 is 100 turns/s^2 = 6000 rpm/s, so
// T = 600 x 1000 / 6000 = 100 ms.
void leadScrewTable() {
    const M::AxisConfig c = rpmAxis();
    const M::PreparedVelocityTarget v = speed(c, 50);
    CHECK(v.effectiveNative == 600);
    M::PreparedRampTime out;
    CHECK(M::prepareRampTime(accel(c, 500), v, c, 1, 2000, out));
    CHECK(equal(out.exactMs, 100, 1) && out.effectiveMs == 100 && out.quantization.errorNumerator == 0);
    CHECK(equal(out.effectiveAcceleration, 500, 1) && equal(out.accelerationError, 0, 1));
    CHECK(out.speedNative == 600 && out.rateBasis == M::NativeRateBasis::MOTOR_TURNS);
    CHECK(out.rateTime == M::TimeUnit::MINUTE && M::sameBinding(out.binding, c.binding));

    // 300 mm/s^2 = 60 turns/s^2 = 3600 rpm/s: T = 600000 / 3600 = 500/3 = 166.67 ms.
    const Saved<M::PreparedRampTime> saved(out);
    M::AccelerationRequest r = accel(c, 300);
    CHECK(failedWith(M::prepareRampTime(r, v, c, 1, 2000, out), M::Err::ILLEGAL_VALUE, M::UnitError::FRACTIONAL_TARGET));
    CHECK(saved.unchanged(out));
    // NEAREST and CEIL: 167 ms, +1/3 ms. Effective 300 x (500/3) / 167 = 50000/167 = 299.40 mm/s^2,
    // error 50000/167 - 300 = -100/167 mm/s^2.
    const M::Rounding up[] = {M::Rounding::NEAREST, M::Rounding::CEIL};
    for (unsigned i = 0; i < 2; ++i) {
        r.rounding = up[i];
        CHECK(M::prepareRampTime(r, v, c, 1, 2000, out));
        CHECK(equal(out.exactMs, 500, 3) && out.effectiveMs == 167);
        CHECK(out.quantization.errorNumerator == 1 && out.quantization.errorDenominator == 3 && !out.quantization.errorNegative);
        CHECK(equal(out.effectiveAcceleration, 50000, 167) && equal(out.accelerationError, -100, 167));
    }
    // FLOOR and TOWARD_ZERO: 166 ms, -2/3 ms. Effective 300 x (500/3) / 166 = 25000/83 = 301.20,
    // error 25000/83 - 300 = 100/83 mm/s^2.
    const M::Rounding down[] = {M::Rounding::FLOOR, M::Rounding::TOWARD_ZERO};
    for (unsigned i = 0; i < 2; ++i) {
        r.rounding = down[i];
        CHECK(M::prepareRampTime(r, v, c, 1, 2000, out));
        CHECK(out.effectiveMs == 166 && out.quantization.errorNumerator == 2 &&
              out.quantization.errorDenominator == 3 && out.quantization.errorNegative);
        CHECK(equal(out.effectiveAcceleration, 25000, 83) && equal(out.accelerationError, 100, 83));
    }
    // The allowance is inclusive and in mm/s^2: |-100/167| passes at 100/167, fails at 99/167.
    r.rounding = M::Rounding::NEAREST; r.maximumAccelerationError = M::Rational(100, 167);
    CHECK(M::prepareRampTime(r, v, c, 1, 2000, out) && out.effectiveMs == 167);
    const Saved<M::PreparedRampTime> nearest(out);
    r.maximumAccelerationError = M::Rational(99, 167);
    CHECK(failedWith(M::prepareRampTime(r, v, c, 1, 2000, out), M::Err::ILLEGAL_VALUE, M::UnitError::QUANTIZATION_ERROR));
    CHECK(nearest.unchanged(out));
    // The same boundary with an unreduced allowance whose cross products exceed 64 bits:
    // (100 x 2^55) / (167 x 2^55) passes, one less in the numerator fails.
    const uint64_t k = uint64_t(1) << 55;
    r.maximumAccelerationError = M::Rational(int64_t(100 * k), 167 * k);
    CHECK(M::prepareRampTime(r, v, c, 1, 2000, out) && out.effectiveMs == 167);
    r.maximumAccelerationError = M::Rational(int64_t(100 * k - 1), 167 * k);
    CHECK(failedWith(M::prepareRampTime(r, v, c, 1, 2000, out), M::Err::ILLEGAL_VALUE, M::UnitError::QUANTIZATION_ERROR));
    r.maximumAccelerationError = M::Rational(0);
    CHECK(failedWith(M::prepareRampTime(r, v, c, 1, 2000, out), M::Err::ILLEGAL_VALUE, M::UnitError::QUANTIZATION_ERROR));
    // A zero allowance still admits an exact time.
    r = accel(c, 500); r.maximumAccelerationError = M::Rational(0);
    CHECK(M::prepareRampTime(r, v, c, 1, 2000, out) && out.effectiveMs == 100);

    // 10 mm/s^2 = 2 turns/s^2 = 120 rpm/s: T = 600000 / 120 = 5000 ms, above 2000: refused, never
    // clamped. The range belongs to the caller: with 6000 ms the same request converts.
    const Saved<M::PreparedRampTime> exact(out);
    CHECK(failedWith(M::prepareRampTime(accel(c, 10), v, c, 1, 2000, out), M::Err::ILLEGAL_VALUE, M::AxisError::LIMIT));
    CHECK(exact.unchanged(out));
    CHECK(M::prepareRampTime(accel(c, 10), v, c, 1, 6000, out) && out.effectiveMs == 5000);

    // 30000 mm/s^2 = 6000 turns/s^2 = 360000 rpm/s: T = 5/3 ms, NEAREST 2 ms; effective
    // 30000 x (5/3) / 2 = 25000 mm/s^2, error -5000 (the guide's worked example).
    CHECK(M::prepareRampTime(accel(c, 30000, 1, mmPerSecond2, M::CoordinateFrame::LOAD, M::Rounding::NEAREST), v, c, 1, 2000, out));
    CHECK(equal(out.exactMs, 5, 3) && out.effectiveMs == 2);
    CHECK(equal(out.effectiveAcceleration, 25000, 1) && equal(out.accelerationError, -5000, 1));
    // 1.5 mm/s^2 = 0.3 turns/s^2 = 18 rpm/s: T = 600000 / 18 = 100000/3 ms; out of range either way.
    CHECK(failedWith(M::prepareRampTime(accel(c, 3, 2, mmPerSecond2, M::CoordinateFrame::LOAD, M::Rounding::NEAREST),
        v, c, 1, 2000, out), M::Err::ILLEGAL_VALUE, M::AxisError::LIMIT));
}

// The same 100 ms in every accepted spelling of 100 motor turns/s^2 at 600 rpm.
void unitForms() {
    const M::AxisConfig c = rpmAxis();
    const M::PreparedVelocityTarget v = speed(c, 600, 1, M::VelocityUnit(M::PositionUnit::TURNS, M::TimeUnit::MINUTE),
        M::CoordinateFrame::NATIVE);
    CHECK(v.effectiveNative == 600);
    struct Form { int64_t n; uint64_t d; M::AccelerationUnit unit; M::CoordinateFrame frame; };
    const Form forms[] = {
        {6000, 1, rpmPerSecond, M::CoordinateFrame::NATIVE}, // native rpm/s
        {6000, 1, rpmPerSecond, M::CoordinateFrame::MOTOR}, // motor rpm/s
        {100, 1, M::AccelerationUnit(M::PositionUnit::TURNS), M::CoordinateFrame::MOTOR}, // turns/s^2
        {36000, 1, M::AccelerationUnit(M::PositionUnit::DEGREES), M::CoordinateFrame::MOTOR}, // 100 x 360 deg/s^2
        {100, 1, M::AccelerationUnit(M::PositionUnit::TURNS), M::CoordinateFrame::LOAD}, // direct drive
        {500, 1, M::AccelerationUnit(M::PositionUnit::MILLIMETRES), M::CoordinateFrame::LOAD}, // 100 x 5 mm/s^2
        {360000, 1, M::AccelerationUnit(M::PositionUnit::TURNS, M::TimeUnit::MINUTE, M::TimeUnit::MINUTE),
            M::CoordinateFrame::MOTOR}, // 6000 rpm/s x 60 = turns/min^2
        {6, 1, M::AccelerationUnit(M::PositionUnit::TURNS, M::TimeUnit::MINUTE, M::TimeUnit::MILLISECOND),
            M::CoordinateFrame::NATIVE}, // 6000 rpm/s = 6 rpm/ms
        {2, 20000, M::AccelerationUnit(M::PositionUnit::TURNS, M::TimeUnit::MILLISECOND, M::TimeUnit::MILLISECOND),
            M::CoordinateFrame::MOTOR}, // 100 turns/s^2 = 1/10000 turns/ms^2, given unreduced
    };
    for (unsigned i = 0; i < sizeof forms / sizeof forms[0]; ++i) {
        M::AccelerationRequest r = accel(c, forms[i].n, forms[i].d, forms[i].unit, forms[i].frame);
        M::PreparedRampTime out;
        CHECK(M::prepareRampTime(r, v, c, 1, 2000, out));
        CHECK(equal(out.exactMs, 100, 1) && out.effectiveMs == 100);
        // An exact time leaves the acceleration as requested, reduced (2/20000 is 1/10000).
        M::Rational reduced; CHECK(M::reduceRational(r.value, reduced));
        CHECK(equal(out.effectiveAcceleration, reduced.numerator, reduced.denominator));
        CHECK(equal(out.accelerationError, 0, 1));
    }
}

// A 10:1 gearbox in front of the 5 mm screw: 5 mm/s is 1 load turn/s = 10 motor turns/s = 600 rpm;
// 50 mm/s^2 is 10 load turns/s^2 = 100 motor turns/s^2: 100 ms. The same 50 mm/s^2 named in the
// MOTOR frame is refused (linear travel needs the load frame); 100 motor turns/s^2 stays 100 ms and
// 100 load turns/s^2 (1000 motor turns/s^2) is 10 ms.
void gearRatios() {
    const M::AxisConfig c = rpmAxis(10);
    const M::PreparedVelocityTarget v = speed(c, 5);
    CHECK(v.effectiveNative == 600);
    M::PreparedRampTime out;
    CHECK(M::prepareRampTime(accel(c, 50), v, c, 1, 2000, out) && out.effectiveMs == 100);
    CHECK(failedWith(M::prepareRampTime(accel(c, 50, 1, mmPerSecond2, M::CoordinateFrame::MOTOR), v, c, 1, 2000, out),
        M::Err::ILLEGAL_VALUE, M::AxisError::INVALID_ARGUMENT));
    const M::AccelerationUnit turns(M::PositionUnit::TURNS);
    CHECK(M::prepareRampTime(accel(c, 100, 1, turns, M::CoordinateFrame::MOTOR), v, c, 1, 2000, out) && out.effectiveMs == 100);
    CHECK(M::prepareRampTime(accel(c, 100, 1, turns, M::CoordinateFrame::LOAD), v, c, 1, 2000, out) && out.effectiveMs == 10);
    // 3:2 gear, 1.5 motor turns per load turn: 600 rpm with 40 load turns/s^2 = 60 motor turns/s^2
    // = 3600 rpm/s gives 500/3 ms again; NEAREST 167 ms, effective 40 x (500/3) / 167 = 20000/501.
    M::AxisConfig g = rpmAxis(); g.units.motorTurnsPerLoadTurn = scale(3, 2);
    const M::PreparedVelocityTarget gv = speed(g, 600, 1, M::VelocityUnit(M::PositionUnit::TURNS, M::TimeUnit::MINUTE),
        M::CoordinateFrame::MOTOR);
    CHECK(gv.effectiveNative == 600);
    M::AccelerationRequest r = accel(g, 40, 1, turns, M::CoordinateFrame::LOAD, M::Rounding::NEAREST);
    CHECK(M::prepareRampTime(r, gv, g, 1, 2000, out));
    CHECK(equal(out.exactMs, 500, 3) && out.effectiveMs == 167 && equal(out.effectiveAcceleration, 20000, 501));
    CHECK(equal(out.accelerationError, -40, 501)); // 20000/501 - 40 = -40/501
    // A missing lead refuses millimetres with the unit's own reason.
    M::AxisConfig noLead = rpmAxis(); noLead.units.millimetresPerLoadTurn = M::UnitScale();
    const M::PreparedVelocityTarget nv = speed(noLead, 600, 1, M::VelocityUnit(M::PositionUnit::TURNS, M::TimeUnit::MINUTE),
        M::CoordinateFrame::NATIVE);
    CHECK(failedWith(M::prepareRampTime(accel(noLead, 500), nv, noLead, 1, 2000, out), M::Err::INVALID_CONFIG,
        M::UnitError::MISSING_SCALE));
}

// Direction is never in the acceleration: a negative speed or a reversed command polarity gives
// the same time, and the result keeps the signed speed it used.
void signsAndPolarity() {
    M::AxisConfig c = rpmAxis();
    const M::PreparedVelocityTarget reverse = speed(c, -50);
    CHECK(reverse.effectiveNative == -600);
    M::PreparedRampTime out;
    CHECK(M::prepareRampTime(accel(c, 500), reverse, c, 1, 2000, out));
    CHECK(out.effectiveMs == 100 && out.speedNative == -600 && equal(out.effectiveAcceleration, 500, 1));
    c.units.commandPolarity = -1;
    const M::PreparedVelocityTarget flipped = speed(c, 50);
    CHECK(flipped.effectiveNative == -600);
    CHECK(M::prepareRampTime(accel(c, 500), flipped, c, 1, 2000, out) && out.effectiveMs == 100);
}

// A command-increment axis: 1000 steps per motor turn, native steps/s. 600 rpm = 10000 steps/s;
// 100 turns/s^2 = 100000 steps/s^2, so T = 1000 x 10000 / 100000 = 100 ms. With reversed polarity
// the STEPS route through turns keeps the magnitude.
void commandIncrementAxis() {
    M::AxisConfig c = rpmAxis();
    c.nativeRateBasis = M::NativeRateBasis::COMMAND_INCREMENTS; c.nativeVelocityTime = M::TimeUnit::SECOND;
    c.units.commandStepsPerMotorTurn = scale(1000);
    const M::PreparedVelocityTarget v = speed(c, 600, 1, M::VelocityUnit(M::PositionUnit::TURNS, M::TimeUnit::MINUTE),
        M::CoordinateFrame::MOTOR);
    CHECK(v.effectiveNative == 10000);
    M::PreparedRampTime out;
    CHECK(M::prepareRampTime(accel(c, 100, 1, M::AccelerationUnit(M::PositionUnit::TURNS), M::CoordinateFrame::MOTOR),
        v, c, 1, 2000, out) && out.effectiveMs == 100);
    CHECK(M::prepareRampTime(accel(c, 100000, 1, M::AccelerationUnit(), M::CoordinateFrame::NATIVE), v, c, 1, 2000, out) &&
          out.effectiveMs == 100 && out.rateBasis == M::NativeRateBasis::COMMAND_INCREMENTS);
    c.units.commandPolarity = -1;
    const M::PreparedVelocityTarget r = speed(c, 600, 1, M::VelocityUnit(M::PositionUnit::TURNS, M::TimeUnit::MINUTE),
        M::CoordinateFrame::MOTOR);
    CHECK(r.effectiveNative == -10000);
    CHECK(M::prepareRampTime(accel(c, 100000, 1, M::AccelerationUnit(), M::CoordinateFrame::MOTOR), r, c, 1, 2000, out) &&
          out.effectiveMs == 100);
}

// NEAREST resolves half milliseconds to the even integer: 600 rpm at 240000 rpm/s is 2.5 ms -> 2,
// at 1200000/7 rpm/s 3.5 ms -> 4; 1200000 rpm/s is 0.5 ms -> 0 and is refused below 1 ms.
void tiesAndMinimum() {
    const M::AxisConfig c = rpmAxis();
    const M::PreparedVelocityTarget v = speed(c, 600, 1, M::VelocityUnit(M::PositionUnit::TURNS, M::TimeUnit::MINUTE),
        M::CoordinateFrame::NATIVE);
    M::PreparedRampTime out;
    M::AccelerationRequest r = accel(c, 240000, 1, rpmPerSecond, M::CoordinateFrame::NATIVE, M::Rounding::NEAREST);
    CHECK(M::prepareRampTime(r, v, c, 1, 2000, out) && equal(out.exactMs, 5, 2) && out.effectiveMs == 2);
    CHECK(equal(out.effectiveAcceleration, 300000, 1) && equal(out.accelerationError, 60000, 1));
    r.value = M::Rational(1200000, 7);
    CHECK(M::prepareRampTime(r, v, c, 1, 2000, out) && equal(out.exactMs, 7, 2) && out.effectiveMs == 4);
    CHECK(equal(out.effectiveAcceleration, 150000, 1)); // 600 rpm in 4 ms
    const Saved<M::PreparedRampTime> saved(out);
    r.value = M::Rational(1200000);
    CHECK(failedWith(M::prepareRampTime(r, v, c, 1, 2000, out), M::Err::ILLEGAL_VALUE, M::AxisError::LIMIT));
    r.rounding = M::Rounding::CEIL;
    CHECK(M::prepareRampTime(r, v, c, 1, 2000, out) && out.effectiveMs == 1);
    // Range edges are inclusive: 2000 ms exactly (600 rpm at 300 rpm/s) passes at 2000, fails at 1999.
    r = accel(c, 300, 1, rpmPerSecond, M::CoordinateFrame::NATIVE);
    CHECK(M::prepareRampTime(r, v, c, 1, 2000, out) && out.effectiveMs == 2000);
    CHECK(M::prepareRampTime(r, v, c, 2000, 2000, out));
    CHECK(failedWith(M::prepareRampTime(r, v, c, 1, 1999, out), M::Err::ILLEGAL_VALUE, M::AxisError::LIMIT));
    CHECK(failedWith(M::prepareRampTime(r, v, c, 2001, 3000, out), M::Err::ILLEGAL_VALUE, M::AxisError::LIMIT));
}

// Cancellation keeps reduced results that fit; storage refusals are explicit, never clamped.
void storageEdges() {
    M::AxisConfig c = rpmAxis();
    c.nativeRateBasis = M::NativeRateBasis::COMMAND_INCREMENTS; c.nativeVelocityTime = M::TimeUnit::SECOND;
    M::VelocityRequest vr; vr.configurationGeneration = c.binding.configurationGeneration;
    M::PreparedVelocityTarget v, extreme;
    vr.value = M::Rational(INT64_C(1) << 62); CHECK(M::prepareVelocityTarget(vr, c, v));
    vr.value = M::Rational(INT64_MIN); CHECK(M::prepareVelocityTarget(vr, c, extreme));
    M::PreparedRampTime out;
    // 2^62 steps/s at 2^62 steps/s^2 is 1000 ms although 1000 x 2^62 exceeds 64 bits.
    M::AccelerationRequest r = accel(c, INT64_C(1) << 62, 1, M::AccelerationUnit(), M::CoordinateFrame::NATIVE);
    CHECK(M::prepareRampTime(r, v, c, 1, 2000, out) && equal(out.exactMs, 1000, 1) && out.effectiveMs == 1000);
    // The widest speed, |INT64_MIN| = 2^63 steps/s, at 2^62 steps/s^2 is 2000 ms.
    CHECK(M::prepareRampTime(r, extreme, c, 1, 2000, out) && out.effectiveMs == 2000 && out.speedNative == INT64_MIN);
    const Saved<M::PreparedRampTime> saved(out);
    // 2^62 steps/s at 1/2^62 steps/s^2 is 1000 x 2^124 ms: refused as storage overflow.
    r.value = M::Rational(1, uint64_t(1) << 62);
    CHECK(failedWith(M::prepareRampTime(r, v, c, 1, 2000, out), M::Err::ILLEGAL_VALUE, M::AxisError::ARITHMETIC_OVERFLOW));
    CHECK(saved.unchanged(out));
    // 2^62 steps/s at 1 step/s^2 is 1000 x 2^62 ms, also beyond 64 bits.
    r.value = M::Rational(1);
    CHECK(failedWith(M::prepareRampTime(r, v, c, 1, 2000, out), M::Err::ILLEGAL_VALUE, M::AxisError::ARITHMETIC_OVERFLOW));
    // 1000 steps/s at 1 step/s^2 is 10^6 ms: inside storage, outside the range.
    M::PreparedVelocityTarget small;
    vr.value = M::Rational(1000); CHECK(M::prepareVelocityTarget(vr, c, small));
    CHECK(failedWith(M::prepareRampTime(r, small, c, 1, 2000, out), M::Err::ILLEGAL_VALUE, M::AxisError::LIMIT));
    CHECK(saved.unchanged(out));
}

// Every refusal leaves the output unchanged and reports its own reason.
void refusals() {
    const M::AxisConfig c = rpmAxis();
    const M::PreparedVelocityTarget v = speed(c, 50);
    M::PreparedRampTime out;
    CHECK(M::prepareRampTime(accel(c, 500), v, c, 1, 2000, out));
    const Saved<M::PreparedRampTime> saved(out);
    struct Case { M::AccelerationRequest request; M::Err code; int32_t detail; };
    const int32_t invalid = static_cast<int32_t>(M::AxisError::INVALID_ARGUMENT);
    const int32_t stale = static_cast<int32_t>(M::AxisError::STALE_GENERATION);
    Case cases[16];
    unsigned count = 0;
    M::AccelerationRequest r = accel(c, 0); cases[count++] = Case{r, M::Err::ILLEGAL_VALUE, invalid};
    r = accel(c, -500); cases[count++] = Case{r, M::Err::ILLEGAL_VALUE, invalid};
    r = accel(c, 500); r.value.denominator = 0; cases[count++] = Case{r, M::Err::ILLEGAL_VALUE, invalid};
    r = accel(c, 500); r.unit.position = static_cast<M::PositionUnit>(9); cases[count++] = Case{r, M::Err::ILLEGAL_VALUE, invalid};
    r = accel(c, 500); r.unit.accelerationTime = static_cast<M::TimeUnit>(3); cases[count++] = Case{r, M::Err::ILLEGAL_VALUE, invalid};
    r = accel(c, 500); r.rounding = static_cast<M::Rounding>(5); cases[count++] = Case{r, M::Err::ILLEGAL_VALUE, invalid};
    r = accel(c, 500); r.frame = static_cast<M::CoordinateFrame>(3); cases[count++] = Case{r, M::Err::ILLEGAL_VALUE, invalid};
    r = accel(c, 500); r.maximumAccelerationError = M::Rational(-1); cases[count++] = Case{r, M::Err::ILLEGAL_VALUE, invalid};
    r = accel(c, 500); r.maximumAccelerationError.denominator = 0; cases[count++] = Case{r, M::Err::ILLEGAL_VALUE, invalid};
    r = accel(c, 500); ++r.configurationGeneration; cases[count++] = Case{r, M::Err::INVALID_CONFIG, stale};
    r = accel(c, 500); r.configurationGeneration = 0; cases[count++] = Case{r, M::Err::INVALID_CONFIG, stale};
    // Radians need pi: the exact path refuses them.
    r = accel(c, 200, 1, M::AccelerationUnit(M::PositionUnit::RADIANS), M::CoordinateFrame::MOTOR);
    cases[count++] = Case{r, M::Err::UNSUPPORTED, unitBase + static_cast<int32_t>(M::UnitError::APPROXIMATION_REQUIRED)};
    // The native frame needs motor turns per nativeVelocityTime.
    r = accel(c, 100, 1, M::AccelerationUnit(M::PositionUnit::TURNS), M::CoordinateFrame::NATIVE);
    cases[count++] = Case{r, M::Err::ILLEGAL_VALUE, invalid};
    r = accel(c, 500, 1, mmPerSecond2, M::CoordinateFrame::NATIVE); cases[count++] = Case{r, M::Err::ILLEGAL_VALUE, invalid};
    for (unsigned i = 0; i < count; ++i) {
        CHECK(failedWith(M::prepareRampTime(cases[i].request, v, c, 1, 2000, out), cases[i].code, cases[i].detail));
        CHECK(saved.unchanged(out));
    }
    // The range itself: zero minimum and an inverted range.
    CHECK(failedWith(M::prepareRampTime(accel(c, 500), v, c, 0, 2000, out), M::Err::ILLEGAL_VALUE, M::AxisError::INVALID_ARGUMENT));
    CHECK(failedWith(M::prepareRampTime(accel(c, 500), v, c, 2000, 1000, out), M::Err::ILLEGAL_VALUE, M::AxisError::INVALID_ARGUMENT));
    // The speed: another configuration epoch, binary32, zero, another rate dimension.
    M::PreparedVelocityTarget other = v; ++other.binding.configurationGeneration;
    CHECK(failedWith(M::prepareRampTime(accel(c, 500), other, c, 1, 2000, out), M::Err::INVALID_CONFIG, M::AxisError::STALE_GENERATION));
    other = v; other.rateTime = M::TimeUnit::SECOND;
    CHECK(failedWith(M::prepareRampTime(accel(c, 500), other, c, 1, 2000, out), M::Err::INVALID_CONFIG, M::AxisError::STALE_GENERATION));
    other = v; other.representation = M::NativeRepresentation::BINARY32;
    CHECK(failedWith(M::prepareRampTime(accel(c, 500), other, c, 1, 2000, out), M::Err::ILLEGAL_VALUE, M::AxisError::INVALID_ARGUMENT));
    const M::PreparedVelocityTarget zero = speed(c, 0);
    CHECK(failedWith(M::prepareRampTime(accel(c, 500), zero, c, 1, 2000, out), M::Err::ILLEGAL_VALUE, M::AxisError::INVALID_ARGUMENT));
    // An invalid configuration fails first.
    M::AxisConfig broken = c; broken.binding.logicalAxisId = 0;
    CHECK(!M::prepareRampTime(accel(c, 500), v, broken, 1, 2000, out));
    CHECK(saved.unchanged(out));
}
} // namespace

int main() {
    leadScrewTable();
    unitForms();
    gearRatios();
    signsAndPolarity();
    commandIncrementAxis();
    tiesAndMinimum();
    storageEdges();
    refusals();
    std::puts("ramp time tests passed");
    return 0;
}
