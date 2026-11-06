// SPDX-License-Identifier: MIT
// Optional audit producer for test/vectors/rs_common_intent.inc. It is compiled only by
// scripts/rs_move_vectors.py, against MotorControl-RS sources extracted read-only, and never by
// the MotorControl-CAN build. It reads the retained inputs, calls the real
// MotorControlRS::preparePosition, and prints one row per input in the retained row syntax with
// RS's own outputs. The shared fixture mirrors the header of the retained file and
// test/move_common_test.cpp.
#include "MotorControlRS/Axis.h"
#include <cmath>
#include <cstdio>
#include <cstring>

namespace RS = MotorControlRS;

namespace {
struct AxisRow {
    const char* name;
    uint32_t steps, stepsDenominator, gear, gearDenominator, fullSteps, fullStepsDenominator;
    int64_t origin;
};
const AxisRow axes[] = {
#define RS_MOVE_AXIS(name, steps, stepsDen, gear, gearDen, full, fullDen, origin)                 \
    {name, steps, stepsDen, gear, gearDen, full, fullDen, origin},
#include "rs_common_intent.inc"
};

bool findAxis(const char* name, RS::AxisConfig& c) {
    for (const AxisRow& a : axes) {
        if (std::strcmp(a.name, name))
            continue;
        c = RS::AxisConfig();
        c.target.id = 31;
        c.target.address = 4;
        c.target.generation = 2;
        c.generation = 5;
        c.units.commandStepsPerMotorTurn = RS::UnitScale(a.steps, a.stepsDenominator, RS::ScaleSource::QUALIFIED);
        c.units.motorTurnsPerLoadTurn = RS::UnitScale(a.gear, a.gearDenominator, RS::ScaleSource::QUALIFIED);
        if (a.fullSteps)
            c.units.fullStepsPerMotorTurn =
                RS::UnitScale(a.fullSteps, a.fullStepsDenominator, RS::ScaleSource::QUALIFIED);
        c.supportedRelativeBases = 1;
        c.originKnown = true;
        c.originNative = a.origin;
        c.originSource = RS::ScaleSource::QUALIFIED;
        return true;
    }
    return false;
}

RS::AxisReference witness(const RS::AxisConfig& c, int64_t position) {
    RS::AxisReference w;
    w.target = c.target;
    w.configurationGeneration = c.generation;
    w.idle = w.stationary = w.nativeKnown = true;
    w.nativePosition = position;
    w.basis = RS::RelativeBasis::ACTUAL;
    w.source = RS::ScaleSource::QUALIFIED;
    w.observedUs = w.nowUs = 1000;
    w.maximumAgeUs = 1000;
    return w;
}

RS::PositionRequest request(const RS::AxisConfig& c, int64_t numerator, uint64_t denominator,
                            RS::PositionUnit unit, RS::CoordinateFrame frame, bool relative,
                            RS::Rounding rounding, int64_t allowanceHalves) {
    RS::PositionRequest r;
    r.value = RS::Rational(numerator, denominator);
    r.unit = unit;
    r.frame = frame;
    r.relative = relative;
    r.wrapped = false;
    r.path = RS::AnglePath::SHORTEST;
    r.tie = RS::HalfTurnTie::REJECT;
    r.basis = RS::RelativeBasis::ACTUAL;
    r.configurationGeneration = c.generation;
    r.rounding = rounding;
    r.maximumQuantizationError = double(allowanceHalves) / 2; // Exact in binary64.
    r.approximate = false;
    r.rationalRadians = true;
    r.radians = 0;
    r.maximumApproximationError = 0;
    return r;
}

int failures = 0;

// One retained vector. A refusal or an error that is not a whole number of sixths is printed as its
// own row kind, so the comparison names it instead of rounding it into the table.
void printVector(const char* name, const char* axis, int64_t numerator, uint64_t denominator,
            RS::PositionUnit unit, const char* unitName, RS::CoordinateFrame frame, const char* frameName,
            bool relative, RS::Rounding rounding, const char* roundingName, int64_t allowance,
            int64_t position) {
    RS::AxisConfig c;
    if (!findAxis(axis, c)) {
        std::printf("RS_MOVE_ERROR(\"%s\", \"no RS_MOVE_AXIS %s\")\n", name, axis);
        ++failures;
        return;
    }
    const RS::PositionRequest r = request(c, numerator, denominator, unit, frame, relative, rounding, allowance);
    const RS::AxisReference w = witness(c, position);
    RS::PreparedTarget t;
    const RS::Status s = RS::preparePosition(r, c, &w, t);
    if (!s) {
        std::printf("RS_MOVE_VECTOR_REFUSED(\"%s\", %s, %ld)\n", name, RS::errToString(s.code), long(s.detail));
        return;
    }
    const double sixths = t.roundingError * 6;
    const double whole = std::floor(sixths + 0.5);
    if (std::fabs(sixths - whole) > 1e-9) {
        std::printf("RS_MOVE_ERROR(\"%s\", \"rounding error %.17g is not a whole number of sixths\")\n", name,
                    t.roundingError);
        ++failures;
        return;
    }
    if (!t.endpointKnown) {
        std::printf("RS_MOVE_ERROR(\"%s\", \"RS endpoint is unknown\")\n", name);
        ++failures;
        return;
    }
    std::printf("RS_MOVE_VECTOR(\"%s\", \"%s\", %lld, %llu, %s, %s, %s, %s, %lld, %lld, %lld, %lld, %lld)\n", name,
                axis, (long long)numerator, (unsigned long long)denominator, unitName, frameName,
                relative ? "true" : "false", roundingName, (long long)allowance, (long long)position,
                (long long)t.effectiveNative, (long long)t.endpointNative, (long long)whole);
}

void printRefusal(const char* name, const char* axis, int64_t numerator, uint64_t denominator,
             RS::PositionUnit unit, const char* unitName, RS::CoordinateFrame frame, const char* frameName,
             bool relative, RS::Rounding rounding, const char* roundingName, int64_t allowance,
             int64_t position) {
    RS::AxisConfig c;
    if (!findAxis(axis, c)) {
        std::printf("RS_MOVE_ERROR(\"%s\", \"no RS_MOVE_AXIS %s\")\n", name, axis);
        ++failures;
        return;
    }
    const RS::PositionRequest r = request(c, numerator, denominator, unit, frame, relative, rounding, allowance);
    const RS::AxisReference w = witness(c, position);
    RS::PreparedTarget t;
    const RS::Status s = RS::preparePosition(r, c, &w, t);
    std::printf("RS_MOVE_REFUSAL(\"%s\", \"%s\", %lld, %llu, %s, %s, %s, %s, %lld, %lld, %s, %ld)\n", name, axis,
                (long long)numerator, (unsigned long long)denominator, unitName, frameName,
                relative ? "true" : "false", roundingName, (long long)allowance, (long long)position,
                RS::errToString(s.code), long(s.detail));
}
} // namespace

int main() {
// Only the inputs are passed on; the retained outputs are what the comparison checks.
#define RS_MOVE_VECTOR(name, axis, n, d, unit, frame, relative, rounding, allowance, position, effective,     \
                       endpoint, sixths)                                                                  \
    printVector(name, axis, n, d, RS::PositionUnit::unit, #unit, RS::CoordinateFrame::frame, #frame,        \
                relative, RS::Rounding::rounding, #rounding, allowance, position);
#define RS_MOVE_REFUSAL(name, axis, n, d, unit, frame, relative, rounding, allowance, position, code, detail) \
    printRefusal(name, axis, n, d, RS::PositionUnit::unit, #unit, RS::CoordinateFrame::frame, #frame,       \
                 relative, RS::Rounding::rounding, #rounding, allowance, position);
#include "rs_common_intent.inc"
    return failures ? 3 : 0;
}
