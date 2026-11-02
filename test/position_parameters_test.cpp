// SPDX-License-Identifier: MIT
// Pure host vectors; no simulated drive, service engine or firmware capability.
#include <MotorControlCAN/profiles/lichuan_c/Position.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace M = MotorControlCAN;
namespace L = MotorControlCAN::LICHUAN_C;

namespace {
unsigned checks = 0;
#define CHECK(condition)                                                                           \
    do {                                                                                           \
        ++checks;                                                                                  \
        if (!(condition)) {                                                                        \
            std::fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #condition);                   \
            std::abort();                                                                          \
        }                                                                                          \
    } while (false)

template <typename T> struct Saved {
    unsigned char bytes[sizeof(T)];
    explicit Saved(const T& value) { std::memcpy(bytes, &value, sizeof value); }
    void unchanged(const T& value) const { CHECK(std::memcmp(bytes, &value, sizeof value) == 0); }
};

M::AxisConfig axis() {
    M::AxisConfig c;
    c.binding.logicalAxisId = 11;
    c.binding.endpoint.profile = 3;
    c.binding.endpoint.address = 9;
    c.binding.bindingGeneration = 2;
    c.binding.configurationGeneration = 4;
    c.supportedRelativeBases = 7;
    return c;
}
M::PositionRequest request(const M::AxisConfig& c, int64_t value, uint64_t denominator = 1) {
    M::PositionRequest r;
    r.configurationGeneration = c.binding.configurationGeneration;
    r.value = M::Rational(value, denominator);
    return r;
}
L::DriverSettings settings() {
    L::DriverSettings s;
    s.positionSpeedRpm = 120;
    s.rampUpTimeMs = 100;
    s.rampDownTimeMs = 200;
    return s;
}
M::AxisReference reference(const M::AxisConfig& c, int64_t position = 0) {
    M::AxisReference r;
    r.binding = c.binding;
    r.referenceGeneration = c.referenceGeneration;
    r.nativeKnown = true;
    r.nativePosition = position;
    r.source = M::ScaleSource::QUALIFIED;
    r.observedUs = 100;
    r.nowUs = 110;
    r.maximumAgeUs = 20;
    return r;
}
M::Status prepare(L::PreparedPositionParameters& out, const M::PositionRequest& r,
                  const M::AxisConfig& c, const M::AxisReference* ref = nullptr,
                  uint64_t now = 110) {
    const L::DriverSettings s = settings();
    return L::preparePositionParameters(out, L::SettingsModel::CL86_C, r, c, ref, s, now);
}
void rejectedUnchanged(L::PreparedPositionParameters& out, const M::PositionRequest& r,
                       const M::AxisConfig& c, const M::AxisReference* ref = nullptr,
                       uint64_t now = 110) {
    const Saved<L::PreparedPositionParameters> before(out);
    CHECK(!prepare(out, r, c, ref, now));
    before.unchanged(out);
}

void nativeRangesAndQuantization() {
    M::AxisConfig c = axis();
    L::PreparedPositionParameters out;
    const int64_t accepted[] = {-1000000, -1, 0, 1, 1000000};
    for (int64_t value : accepted) {
        CHECK(prepare(out, request(c, value), c));
        CHECK(out.targetNative == value && out.target.endpointNative == value);
        CHECK(out.target.exactArithmetic && out.target.quantization.errorNumerator == 0);
    }
    const int64_t rejected[] = {INT64_MIN, -1000001, 1000001, INT64_MAX};
    for (int64_t value : rejected)
        rejectedUnchanged(out, request(c, value), c);

    M::PositionRequest r = request(c, 2000001, 2);
    r.rounding = M::Rounding::TOWARD_ZERO;
    r.maximumQuantizationError = M::Rational(1, 2);
    rejectedUnchanged(out, r, c); // Effective 1000000 cannot hide requested 1000000.5.
    r.value = M::Rational(-2000001, 2);
    rejectedUnchanged(out, r, c);
    r.value = M::Rational(1999999, 2);
    r.rounding = M::Rounding::NEAREST;
    CHECK(prepare(out, r, c));
    CHECK(out.targetNative == 1000000 && out.target.quantization.errorNumerator == 1 &&
          out.target.quantization.errorDenominator == 2 && !out.target.quantization.errorNegative);
    r.value = M::Rational(-1999999, 2);
    CHECK(prepare(out, r, c));
    CHECK(out.targetNative == -1000000 && out.target.quantization.errorNegative);
    r.maximumQuantizationError = M::Rational(49, 100);
    rejectedUnchanged(out, r, c);
}

void relativeHostBasesAndEndpointLimits() {
    M::AxisConfig c = axis();
    L::PreparedPositionParameters out;
    const M::RelativeBasis bases[] = {M::RelativeBasis::ACTUAL, M::RelativeBasis::COMMANDED,
                                      M::RelativeBasis::QUEUED};
    for (M::RelativeBasis basis : bases) {
        M::AxisReference ref = reference(c, 900000);
        ref.basis = basis;
        M::PositionRequest r = request(c, 200000);
        r.relative = true;
        r.basis = basis;
        CHECK(prepare(out, r, c, &ref));
        CHECK(out.targetNative == 200000 && out.target.endpointNative == 1100000);
        CHECK(out.target.retainedReference.basis == basis && out.target.referenceUsed);
        ref.nativePosition = -900000;
        r.value = M::Rational(-200000);
        CHECK(prepare(out, r, c, &ref));
        CHECK(out.targetNative == -200000 && out.target.endpointNative == -1100000);
        r.value = M::Rational(1000001);
        rejectedUnchanged(out, r, c, &ref);
        c.supportedRelativeBases = 0;
        rejectedUnchanged(out, r, c, &ref);
        c.supportedRelativeBases = 7;
    }
    c.nativeMinimum = 0;
    c.nativeMaximum = 10;
    M::AxisReference ref = reference(c, 9);
    M::PositionRequest r = request(c, -9);
    r.relative = true;
    CHECK(prepare(out, r, c, &ref));
    CHECK(out.targetNative == -9 && out.target.endpointNative == 0);
    r.value = M::Rational(3, 2);
    r.rounding = M::Rounding::TOWARD_ZERO;
    r.maximumQuantizationError = M::Rational(1, 2);
    rejectedUnchanged(out, r, c, &ref); // Requested endpoint 10.5, effective endpoint 10.
    c.nativeMinimum = -100;
    c.nativeMaximum = 100;
    c.softLimitsKnown = true;
    c.softMinimum = 0;
    c.softMaximum = 10;
    rejectedUnchanged(out, r, c, &ref); // The same endpoint restriction applies to soft limits.
    c = axis();
    ref = reference(c, 1100000);
    r = request(c, 1000000);
    r.relative = true;
    CHECK(prepare(out, r, c, &ref));
    CHECK(out.targetNative == 1000000 && out.target.endpointNative == 2100000);
    ref = reference(c, INT64_MIN);
    r = request(c, 1);
    r.relative = true;
    CHECK(prepare(out, r, c, &ref));
    CHECK(out.targetNative == 1 && out.target.endpointNative == INT64_MIN + 1);
    ref.nativePosition = INT64_MAX;
    rejectedUnchanged(out, r, c, &ref);
}

void commonIntentVectorsAndOriginParity() {
    // Same host intent vectors as live MotorControl-RS test/axis_test.cpp
    // testRounding and testFactorsAndOrigins; no RS frames/native profile bounds.
    M::AxisConfig c = axis();
    c.units.commandStepsPerMotorTurn = M::UnitScale(3200, 1, M::ScaleSource::DOCUMENTED);
    c.units.motorTurnsPerLoadTurn = M::UnitScale(5, 2, M::ScaleSource::DOCUMENTED);
    c.originKnown = true;
    c.originSource = M::ScaleSource::QUALIFIED;
    M::PositionRequest r = request(c, 90);
    r.unit = M::PositionUnit::DEGREES;
    r.frame = M::CoordinateFrame::MOTOR;
    L::PreparedPositionParameters out;
    CHECK(prepare(out, r, c));
    CHECK(out.targetNative == 800);
    r.frame = M::CoordinateFrame::LOAD;
    CHECK(prepare(out, r, c));
    CHECK(out.targetNative == 2000);
    c.units.commandPolarity = -1;
    CHECK(prepare(out, r, c));
    CHECK(out.targetNative == -2000);

    c = axis();
    c.originKnown = true;
    c.originNative = 1;
    c.originSource = M::ScaleSource::QUALIFIED;
    r = request(c, 1, 2);
    r.frame = M::CoordinateFrame::MOTOR;
    r.rounding = M::Rounding::NEAREST;
    r.maximumQuantizationError = M::Rational(1, 2);
    CHECK(prepare(out, r, c));
    CHECK(out.targetNative == 2); // Absolute 1 + 0.5 rounds to even 2.
    M::AxisReference ref = reference(c, 1);
    r.relative = true;
    CHECK(prepare(out, r, c, &ref));
    CHECK(out.targetNative == 0 && out.target.endpointNative == 1); // Delta 0.5 rounds to 0.
    CHECK(out.target.quantization.errorNegative);
    // Opposite-sign intent and origin must be combined before the profile bound.
    r.relative = false;
    r.value = M::Rational(-1, 2);
    c.originNative = 1000000;
    CHECK(prepare(out, r, c));
    CHECK(out.targetNative == 1000000 && !out.target.quantization.errorNegative);
    c.originNative = -1000000;
    r.value = M::Rational(1, 2);
    CHECK(prepare(out, r, c));
    CHECK(out.targetNative == -1000000 && out.target.quantization.errorNegative);
}

void settingsAndModelValidation() {
    const M::AxisConfig c = axis();
    const M::PositionRequest r = request(c, 123);
    L::PreparedPositionParameters out;
    const L::SettingsModel models[] = {L::SettingsModel::CL86_C, L::SettingsModel::OL86_C};
    for (L::SettingsModel model : models) {
        L::DriverSettings s = settings();
        s.positionSpeedRpm = s.rampUpTimeMs = s.rampDownTimeMs = 0;
        CHECK(L::preparePositionParameters(out, model, r, c, nullptr, s, 0));
        CHECK(out.model == model && out.preparedUs == 0);
        s.positionSpeedRpm = 3000;
        s.rampUpTimeMs = s.rampDownTimeMs = 2000;
        CHECK(L::preparePositionParameters(out, model, r, c, nullptr, s, 110));
    }
    const Saved<L::PreparedPositionParameters> before(out);
    for (unsigned scenario = 0; scenario < 6; ++scenario) {
        L::DriverSettings s = settings();
        L::SettingsModel model = L::SettingsModel::CL86_C;
        if (scenario == 0)
            s = L::DriverSettings();
        if (scenario == 1)
            s.positionSpeedRpm = 3001;
        if (scenario == 2)
            s.rampUpTimeMs = 2001;
        if (scenario == 3)
            s.rampDownTimeMs = 2001;
        if (scenario == 4)
            model = L::SettingsModel::UNKNOWN;
        if (scenario == 5)
            model = static_cast<L::SettingsModel>(255);
        CHECK(!L::preparePositionParameters(out, model, r, c, nullptr, s, 110));
        before.unchanged(out);
    }
}

void witnessTimeAndEpochs() {
    M::AxisConfig c = axis();
    M::AxisReference ref = reference(c, 5);
    M::PositionRequest r = request(c, 0);
    r.relative = true;
    L::PreparedPositionParameters out;
    CHECK(prepare(out, r, c, &ref, 120)); // Inclusive age limit, no time renewal.
    CHECK(out.preparedUs == 120 && out.target.retainedReference.observedUs == 100 &&
          out.target.retainedReference.nowUs == 120 && ref.nowUs == 110);
    rejectedUnchanged(out, r, c, &ref, 121);
    rejectedUnchanged(out, r, c, &ref, 109);
    const M::Status rollback = prepare(out, r, c, &ref, 109);
    CHECK(rollback.code == M::Err::INVALID_CONFIG &&
          rollback.detail == static_cast<int32_t>(L::PositionParameterError::AXIS_ERROR_BASE) +
                                 static_cast<int32_t>(M::AxisError::CLOCK_ERROR));
    rejectedUnchanged(out, r, c, &ref, UINT64_MAX);
    rejectedUnchanged(out, r, c, nullptr);
    for (unsigned scenario = 0; scenario < 6; ++scenario) {
        M::AxisReference changed = ref;
        if (scenario == 0)
            ++changed.binding.bindingGeneration;
        if (scenario == 1)
            ++changed.binding.configurationGeneration;
        if (scenario == 2)
            ++changed.referenceGeneration;
        if (scenario == 3)
            changed.observedUs = 111;
        if (scenario == 4)
            changed.basis = M::RelativeBasis::QUEUED;
        if (scenario == 5)
            changed.nativeKnown = false;
        rejectedUnchanged(out, r, c, &changed);
    }
    ++r.configurationGeneration;
    rejectedUnchanged(out, r, c, &ref);
    r = request(c, 0);
    CHECK(prepare(out, r, c));
    CHECK(out.targetNative == 0 && !out.target.referenceUsed &&
          !out.target.retainedReference.stationary && !out.target.retainedReference.nativeKnown);
    ref.nowUs = 111;
    rejectedUnchanged(out, r, c, &ref); // Even an optional supplied witness is checked.
    r.referencePolicy = M::ReferencePolicy::REQUIRED;
    rejectedUnchanged(out, r, c);
}

void representationAndApproximation() {
    M::AxisConfig c = axis();
    M::PositionRequest r = request(c, 1);
    L::PreparedPositionParameters out;
    CHECK(prepare(out, r, c));
    c.positionRepresentation = M::NativeRepresentation::BINARY32;
    rejectedUnchanged(out, r, c);
    c = axis();
    r.wrapped = true;
    rejectedUnchanged(out, r, c);
    r.wrapped = false;
    c.units.commandStepsPerMotorTurn = M::UnitScale(1000, 1, M::ScaleSource::DOCUMENTED);
    r.unit = M::PositionUnit::RADIANS;
    r.frame = M::CoordinateFrame::MOTOR;
    r.relative = true;
    M::AxisReference ref = reference(c);
    rejectedUnchanged(out, r, c, &ref);
    r.approximate = true;
    r.maximumApproximationError = 1e-8;
    r.rounding = M::Rounding::NEAREST;
    r.maximumQuantizationError = M::Rational(1);
    CHECK(prepare(out, r, c, &ref));
    CHECK(out.targetNative == 159 && !out.target.exactArithmetic &&
          out.target.absoluteErrorBound > 0 && out.target.approximationErrorBound > 0);
    r.relative = false;
    c.originKnown = true;
    c.originSource = M::ScaleSource::QUALIFIED;
    c.originNative = 999841;
    rejectedUnchanged(out, r, c); // Rounded edge 1000000 has no error-bound margin.
    c.originNative = -999841;
    r.value = M::Rational(-1);
    rejectedUnchanged(out, r, c);
    c.originNative = 0;
    r.value = M::Rational(0);
    r.approximate = false;
    r.maximumApproximationError = 0;
    r.maximumQuantizationError = M::Rational(0);
    r.rounding = M::Rounding::EXACT;
    CHECK(prepare(out, r, c));
    CHECK(out.targetNative == 0 && out.target.exactArithmetic);
}

void ownedInputsAndAliasRejection() {
    M::AxisConfig c = axis();
    M::PositionRequest r = request(c, 17);
    r.relative = true;
    M::AxisReference ref = reference(c, 20);
    L::DriverSettings s = settings();
    L::PreparedPositionParameters out;
    CHECK(L::preparePositionParameters(out, L::SettingsModel::CL86_C, r, c, &ref, s, 110));
    const Saved<L::PreparedPositionParameters> before(out);
    CHECK(M::validatePreparedTarget(out.target, c, &ref, 110));
    M::AxisReference changedReference = ref;
    ++changedReference.nativePosition;
    CHECK(!M::validatePreparedTarget(out.target, c, &changedReference, 110));
    r.value = M::Rational(99);
    ++c.binding.configurationGeneration;
    CHECK(!M::validatePreparedTarget(out.target, c, &ref, 110));
    ref.nativePosition = -80;
    s.positionSpeedRpm = 999;
    before.unchanged(out);
    CHECK(out.target.requested.value.numerator == 17 && out.target.endpointNative == 37 &&
          out.target.retainedReference.nativePosition == 20 &&
          out.settings.positionSpeedRpm == 120);
    const L::PreparedPositionParameters copy = out;
    CHECK(!L::preparePositionParameters(out, copy.model, out.target.requested, copy.axis,
                                        &copy.target.retainedReference, copy.settings, 110));
    before.unchanged(out);
    CHECK(!L::preparePositionParameters(out, copy.model, copy.target.requested, out.axis,
                                        &copy.target.retainedReference, copy.settings, 110));
    before.unchanged(out);
    CHECK(!L::preparePositionParameters(out, copy.model, copy.target.requested, copy.axis,
                                        &out.target.retainedReference, copy.settings, 110));
    before.unchanged(out);
    CHECK(!L::preparePositionParameters(out, copy.model, copy.target.requested, copy.axis,
                                        &copy.target.retainedReference, out.settings, 110));
    before.unchanged(out);
}

void retainedTargetsFollowAxisInvalidation() {
    M::AxisConfig c = axis();
    M::AxisReference ref = reference(c, 10);
    M::PositionRequest relative = request(c, 1);
    relative.relative = true;
    L::PreparedPositionParameters displacement, absolute;
    CHECK(prepare(displacement, relative, c, &ref));
    CHECK(prepare(absolute, request(c, 11), c));
    CHECK(M::invalidateAxisReference(c, ref, M::ReferenceLoss::POSITION));
    CHECK(!M::validatePreparedTarget(displacement.target, c, &ref, 110));
    CHECK(M::validatePreparedTarget(absolute.target, c, nullptr, 110));
    CHECK(M::invalidateAxisReference(c, ref, M::ReferenceLoss::COORDINATES));
    CHECK(!M::validatePreparedTarget(absolute.target, c, nullptr, 110));
    const M::PositionRequest oldRequest = absolute.target.requested;
    rejectedUnchanged(absolute, oldRequest, c);
    CHECK(absolute.targetNative == 11 && displacement.targetNative == 1);
}
} // namespace

int main() {
    nativeRangesAndQuantization();
    relativeHostBasesAndEndpointLimits();
    commonIntentVectorsAndOriginParity();
    settingsAndModelValidation();
    witnessTimeAndEpochs();
    representationAndApproximation();
    ownedInputsAndAliasRejection();
    retainedTargetsFollowAxisInvalidation();
    std::printf("position parameter checks passed: %u\n", checks);
    return 0;
}
