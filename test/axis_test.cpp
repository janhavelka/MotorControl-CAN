// SPDX-License-Identifier: MIT
// Independent coordinate vectors; no controller, vendor driver or device traffic.
#include <MotorControlCAN/Axis.h>

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>

#define CHECK(condition) do { if (!(condition)) { std::fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #condition); std::abort(); } } while (false)

namespace M = MotorControlCAN;

namespace {
template <typename T> struct Saved {
    unsigned char bytes[sizeof(T)];
    explicit Saved(const T& value) { std::memcpy(bytes, &value, sizeof value); }
    bool unchanged(const T& value) const { return std::memcmp(bytes, &value, sizeof value) == 0; }
};

M::AxisConfig config() {
    M::AxisConfig c;
    c.binding.logicalAxisId = 91;
    c.binding.endpoint.profile = 17;
    c.binding.endpoint.address = 1000;
    c.binding.endpoint.subaxis = 2;
    c.binding.bindingGeneration = 3;
    c.binding.configurationGeneration = 4;
    c.supportedRelativeBases = 7;
    return c;
}

M::UnitScale scale(uint32_t n, uint32_t d = 1) {
    return M::UnitScale(n, d, M::ScaleSource::QUALIFIED);
}

M::AxisReference reference(const M::AxisConfig& c, int64_t position = 0) {
    M::AxisReference r;
    r.binding = c.binding;
    r.referenceGeneration = c.referenceGeneration;
    r.nativeKnown = r.idle = r.stationary = true;
    r.nativePosition = position;
    r.source = M::ScaleSource::QUALIFIED;
    r.observedUs = 100;
    r.nowUs = 110;
    r.maximumAgeUs = 20;
    return r;
}

M::PositionRequest position(const M::AxisConfig& c, int64_t n, uint64_t d = 1) {
    M::PositionRequest r;
    r.configurationGeneration = c.binding.configurationGeneration;
    r.value = M::Rational(n, d);
    return r;
}

uint32_t floatBits(float value) {
    uint32_t bits = 0;
    std::memcpy(&bits, &value, sizeof bits);
    return bits;
}

void nativeExactAndAtomicFailures() {
    M::AxisConfig c = config();
    CHECK(M::validateAxisConfig(c)); // Generic address/subaxis need no CANopen range or step geometry.
    CHECK(c.units.fullStepsPerMotorTurn.source == M::ScaleSource::UNKNOWN);
    M::PreparedTarget out;
    const int64_t values[] = {INT64_MIN, -1, 0, 1, INT64_C(9007199254740992),
        INT64_C(9007199254740993), INT64_MAX};
    for (unsigned i = 0; i < sizeof values / sizeof values[0]; ++i) {
        CHECK(M::preparePosition(position(c, values[i]), c, nullptr, out));
        CHECK(out.endpointNative == values[i] && out.effectiveNative == values[i]);
        CHECK(out.requestedNative.integral == values[i] && out.requestedNative.numerator == 0);
        CHECK(out.exactArithmetic && !out.referenceUsed);
        CHECK(M::sameBinding(out.binding, c.binding));
    }
    const Saved<M::PreparedTarget> saved(out);
    M::PositionRequest r = position(c, 1, 0);
    CHECK(!M::preparePosition(r, c, nullptr, out)); CHECK(saved.unchanged(out));
    r = position(c, 1); ++r.configurationGeneration;
    CHECK(!M::preparePosition(r, c, nullptr, out)); CHECK(saved.unchanged(out));
    r = position(c, 1); r.frame = static_cast<M::CoordinateFrame>(255);
    CHECK(!M::preparePosition(r, c, nullptr, out)); CHECK(saved.unchanged(out));
    r = position(c, 1); r.unit = static_cast<M::PositionUnit>(255);
    CHECK(!M::preparePosition(r, c, nullptr, out)); CHECK(saved.unchanged(out));
    r = position(c, 1); r.rounding = static_cast<M::Rounding>(255);
    CHECK(!M::preparePosition(r, c, nullptr, out)); CHECK(saved.unchanged(out));
    r = position(c, 1); r.referencePolicy = static_cast<M::ReferencePolicy>(255);
    CHECK(!M::preparePosition(r, c, nullptr, out)); CHECK(saved.unchanged(out));
    r = position(c, 1); r.basis = static_cast<M::RelativeBasis>(255);
    CHECK(!M::preparePosition(r, c, nullptr, out)); CHECK(saved.unchanged(out));
    r = position(c, 1); r.maximumQuantizationError = M::Rational(-1);
    CHECK(!M::preparePosition(r, c, nullptr, out)); CHECK(saved.unchanged(out));
    c.units.fullStepsPerMotorTurn = scale(1, 0);
    CHECK(!M::preparePosition(position(c, 1), c, nullptr, out)); CHECK(saved.unchanged(out));
    c = config(); c.nativeMinimum = 2; c.nativeMaximum = 1;
    CHECK(!M::validateAxisConfig(c));
    c = config(); c.supportedRelativeBases = 8; CHECK(!M::validateAxisConfig(c));
    c = config(); c.referenceGeneration = 0; CHECK(!M::validateAxisConfig(c));
    c = config(); c.positionRepresentation = static_cast<M::NativeRepresentation>(255);
    CHECK(!M::validateAxisConfig(c));
}

void independentScalesAndOrigins() {
    M::AxisConfig c = config();
    c.units.commandStepsPerMotorTurn = scale(3200);
    c.units.motorTurnsPerLoadTurn = scale(5, 2);
    c.units.millimetresPerLoadTurn = scale(8);
    c.originKnown = true; c.originNative = 17; c.originSource = M::ScaleSource::QUALIFIED;
    M::PositionRequest r = position(c, 90);
    r.unit = M::PositionUnit::DEGREES; r.frame = M::CoordinateFrame::MOTOR;
    M::PreparedTarget out;
    CHECK(M::preparePosition(r, c, nullptr, out)); CHECK(out.endpointNative == 817);
    r.frame = M::CoordinateFrame::LOAD;
    CHECK(M::preparePosition(r, c, nullptr, out)); CHECK(out.endpointNative == 2017);
    r.unit = M::PositionUnit::MILLIMETRES; r.value = M::Rational(1);
    CHECK(M::preparePosition(r, c, nullptr, out)); CHECK(out.endpointNative == 1017);
    r.frame = M::CoordinateFrame::MOTOR;
    const Saved<M::PreparedTarget> saved(out);
    CHECK(!M::preparePosition(r, c, nullptr, out)); CHECK(saved.unchanged(out));

    c.units.encoder.countsPerUnit = scale(100);
    c.units.encoder.basis = M::EncoderBasis::MILLIMETRE;
    c.units.encoder.sourceId = 7;
    c.encoderOriginKnown = true; c.encoderOriginNative = 10;
    c.encoderOriginSource = M::ScaleSource::READBACK;
    r.unit = M::PositionUnit::ENCODER_COUNTS; r.frame = M::CoordinateFrame::LOAD;
    r.value = M::Rational(100);
    CHECK(M::preparePosition(r, c, nullptr, out)); CHECK(out.endpointNative == 1010);
    c.units.encoder.polarity = -1;
    CHECK(M::preparePosition(r, c, nullptr, out)); CHECK(out.endpointNative == -990);
    c.encoderOriginKnown = false;
    const Saved<M::PreparedTarget> feedbackSaved(out);
    CHECK(!M::preparePosition(r, c, nullptr, out)); CHECK(feedbackSaved.unchanged(out));
    // A mechanical host zero cannot substitute for the selected encoder's zero.
    CHECK(c.originKnown);

    // Prompt-4 C1b: abstract servo, 1/2 load turn -> 3/4 motor turn.
    c = config(); c.units.commandStepsPerMotorTurn = scale(4096);
    c.units.motorTurnsPerLoadTurn = scale(3, 2);
    c.units.encoder.countsPerUnit = scale(2048); c.units.encoder.sourceId = 3;
    c.originKnown = c.encoderOriginKnown = true;
    c.originSource = c.encoderOriginSource = M::ScaleSource::QUALIFIED;
    r = position(c, 1, 2); r.unit = M::PositionUnit::TURNS; r.frame = M::CoordinateFrame::LOAD;
    CHECK(M::preparePosition(r, c, nullptr, out)); CHECK(out.endpointNative == 3072);
    r.unit = M::PositionUnit::ENCODER_COUNTS; r.value = M::Rational(1536);
    CHECK(M::preparePosition(r, c, nullptr, out)); CHECK(out.endpointNative == 3072);
    CHECK(c.units.fullStepsPerMotorTurn.source == M::ScaleSource::UNKNOWN);
    c.units.encoder.countsPerUnit = M::UnitScale(); c.units.encoder.sourceId = 0;
    c.encoderOriginKnown = false; c.encoderOriginSource = M::ScaleSource::UNKNOWN;
    r.unit = M::PositionUnit::TURNS; r.value = M::Rational(1, 2);
    CHECK(M::preparePosition(r, c, nullptr, out)); CHECK(out.endpointNative == 3072);
    c.units.motorTurnsPerLoadTurn = M::UnitScale();
    const Saved<M::PreparedTarget> missingScale(out);
    CHECK(!M::preparePosition(r, c, nullptr, out)); CHECK(missingScale.unchanged(out));
    r.frame = M::CoordinateFrame::MOTOR;
    CHECK(M::preparePosition(r, c, nullptr, out)); CHECK(out.endpointNative == 2048);
}

void roundingOriginsAndOverflow() {
    M::AxisConfig c = config(); M::PreparedTarget out;
    c.originKnown = true; c.originSource = M::ScaleSource::QUALIFIED;
    M::PositionRequest r = position(c, 1, 2); r.frame = M::CoordinateFrame::MOTOR;
    r.rounding = M::Rounding::NEAREST; r.maximumQuantizationError = M::Rational(1, 2);
    c.originNative = 1;
    CHECK(M::preparePosition(r, c, nullptr, out)); CHECK(out.effectiveNative == 2);
    CHECK(out.requestedNative.integral == 1 && out.requestedNative.numerator == 1);
    M::AxisReference ref = reference(c, 1); r.relative = true;
    CHECK(M::preparePosition(r, c, &ref, out));
    CHECK(out.effectiveNative == 0 && out.endpointNative == 1);
    r.relative = false; r.value.numerator = -1; r.rounding = M::Rounding::TOWARD_ZERO;
    c.originNative = 2;
    CHECK(M::preparePosition(r, c, nullptr, out)); CHECK(out.effectiveNative == 1);
    c.originNative = INT64_MAX; r.rounding = M::Rounding::NEAREST;
    CHECK(M::preparePosition(r, c, nullptr, out)); CHECK(out.effectiveNative == INT64_MAX - 1);
    c.originNative = INT64_MIN; r.value.numerator = 1;
    CHECK(M::preparePosition(r, c, nullptr, out)); CHECK(out.effectiveNative == INT64_MIN);
    const Saved<M::PreparedTarget> saved(out);
    r.value = M::Rational(-1);
    CHECK(!M::preparePosition(r, c, nullptr, out)); CHECK(saved.unchanged(out));
    c.originNative = INT64_MAX; r.value = M::Rational(1);
    CHECK(!M::preparePosition(r, c, nullptr, out)); CHECK(saved.unchanged(out));

    c = config(); r = position(c, -5, 2); r.maximumQuantizationError = M::Rational(1);
    const M::Rounding modes[] = {M::Rounding::NEAREST, M::Rounding::TOWARD_ZERO,
        M::Rounding::FLOOR, M::Rounding::CEIL};
    const int64_t expected[] = {-2, -2, -3, -2};
    for (unsigned i = 0; i < 4; ++i) {
        r.rounding = modes[i]; CHECK(M::preparePosition(r, c, nullptr, out));
        CHECK(out.effectiveNative == expected[i]);
    }
    r.rounding = M::Rounding::EXACT;
    const Saved<M::PreparedTarget> fractional(out);
    CHECK(!M::preparePosition(r, c, nullptr, out)); CHECK(fractional.unchanged(out));
    r.rounding = M::Rounding::NEAREST; r.maximumQuantizationError = M::Rational(49, 100);
    CHECK(!M::preparePosition(r, c, nullptr, out)); CHECK(fractional.unchanged(out));
    c.units.commandStepsPerMotorTurn = scale(UINT32_MAX);
    r = position(c, INT64_MAX); r.unit = M::PositionUnit::TURNS;
    r.frame = M::CoordinateFrame::MOTOR; r.relative = true; ref = reference(c);
    CHECK(!M::preparePosition(r, c, &ref, out)); CHECK(fractional.unchanged(out));
    c = config(); c.units.commandPolarity = -1;
    r = position(c, INT64_MIN); r.frame = M::CoordinateFrame::MOTOR; r.relative = true;
    ref = reference(c);
    CHECK(!M::preparePosition(r, c, &ref, out)); CHECK(fractional.unchanged(out));
    r = position(c, INT64_MIN);
    CHECK(M::preparePosition(r, c, nullptr, out)); CHECK(out.endpointNative == INT64_MIN);

    // Large scale products cancel exactly before storage/range checks.
    c = config(); c.units.commandStepsPerMotorTurn = scale(UINT32_MAX, UINT32_MAX - 1);
    c.units.motorTurnsPerLoadTurn = scale(UINT32_MAX - 1, UINT32_MAX);
    c.originKnown = true; c.originSource = M::ScaleSource::QUALIFIED;
    r = position(c, INT64_MAX); r.unit = M::PositionUnit::TURNS; r.frame = M::CoordinateFrame::LOAD;
    CHECK(M::preparePosition(r, c, nullptr, out)); CHECK(out.endpointNative == INT64_MAX);
}

void inclusiveLimitsAndChosenBasis() {
    M::AxisConfig c = config(); c.nativeMinimum = 0; c.nativeMaximum = 10;
    c.softLimitsKnown = true; c.softMinimum = 0; c.softMaximum = 10;
    M::PreparedTarget out;
    CHECK(M::preparePosition(position(c, 0), c, nullptr, out)); CHECK(out.endpointNative == 0);
    CHECK(M::preparePosition(position(c, 10), c, nullptr, out)); CHECK(out.endpointNative == 10);
    M::AxisReference ref = reference(c, 9);
    M::PositionRequest r = position(c, -9); r.relative = true;
    CHECK(M::preparePosition(r, c, &ref, out));
    CHECK(out.effectiveNative == -9 && out.endpointNative == 0); // Displacement is not an absolute limit.
    r.value = M::Rational(1); CHECK(M::preparePosition(r, c, &ref, out)); CHECK(out.endpointNative == 10);
    const Saved<M::PreparedTarget> saved(out);
    r.value = M::Rational(3, 2); r.rounding = M::Rounding::TOWARD_ZERO;
    r.maximumQuantizationError = M::Rational(1, 2);
    CHECK(!M::preparePosition(r, c, &ref, out)); CHECK(saved.unchanged(out));
    r.value = M::Rational(1); ref.nativePosition = 11;
    CHECK(!M::preparePosition(r, c, &ref, out)); CHECK(saved.unchanged(out));
    ref = reference(c, 9); ref.basis = M::RelativeBasis::COMMANDED;
    CHECK(!M::preparePosition(r, c, &ref, out)); CHECK(saved.unchanged(out));
    r.basis = M::RelativeBasis::COMMANDED;
    CHECK(M::preparePosition(r, c, &ref, out)); CHECK(out.endpointNative == 10);
    const Saved<M::PreparedTarget> matched(out);
    CHECK(!M::preparePosition(r, c, nullptr, out)); CHECK(matched.unchanged(out));
    ref.nativeKnown = false;
    CHECK(!M::preparePosition(r, c, &ref, out)); CHECK(matched.unchanged(out));
    ref = reference(c, 9); r.basis = M::RelativeBasis::ACTUAL; c.supportedRelativeBases = 0;
    CHECK(!M::preparePosition(r, c, &ref, out)); CHECK(matched.unchanged(out));
}

void freshnessAndPreparedDependencies() {
    M::AxisConfig c = config(); M::PreparedTarget out;
    M::PositionRequest r = position(c, 1);
    CHECK(M::preparePosition(r, c, nullptr, out));
    CHECK(M::validatePreparedTarget(out, c, nullptr, 0));
    CHECK(M::validatePreparedTarget(out, c, nullptr, INT64_MAX));
    CHECK(!M::validatePreparedTarget(out, c, nullptr, UINT64_MAX));
    r.referencePolicy = M::ReferencePolicy::REQUIRED;
    const Saved<M::PreparedTarget> saved(out);
    CHECK(!M::preparePosition(r, c, nullptr, out)); CHECK(saved.unchanged(out));
    M::AxisReference ref = reference(c);
    CHECK(M::preparePosition(r, c, &ref, out)); CHECK(out.referenceUsed);
    CHECK(M::validatePreparedTarget(out, c, &ref, 120));
    CHECK(!M::validatePreparedTarget(out, c, &ref, 121));
    CHECK(!M::validatePreparedTarget(out, c, &ref, 109)); // Admission cannot precede preparation.
    CHECK(!M::validatePreparedTarget(out, c, nullptr, 110));
    M::AxisReference different = ref; ++different.nativePosition;
    CHECK(!M::validatePreparedTarget(out, c, &different, 110));
    different = ref; different.basis = M::RelativeBasis::QUEUED;
    CHECK(!M::validatePreparedTarget(out, c, &different, 110));
    different = ref; ++different.binding.endpoint.subaxis;
    CHECK(!M::validatePreparedTarget(out, c, &different, 110));
    M::AxisConfig changed = c; ++changed.referenceGeneration;
    CHECK(!M::validatePreparedTarget(out, changed, &ref, 110));
    changed = c; ++changed.binding.bindingGeneration;
    CHECK(!M::validatePreparedTarget(out, changed, &ref, 110));
    changed = c; ++changed.binding.configurationGeneration;
    CHECK(!M::validatePreparedTarget(out, changed, &ref, 110));
    ref.nowUs = 120;
    CHECK(M::preparePosition(r, c, &ref, out));
    const Saved<M::PreparedTarget> inclusive(out);
    ref.nowUs = 121;
    CHECK(!M::preparePosition(r, c, &ref, out)); CHECK(inclusive.unchanged(out));
    ref.maximumAgeUs = 0;
    CHECK(M::preparePosition(r, c, &ref, out));
    ref.nowUs = 99;
    const Saved<M::PreparedTarget> noExpiry(out);
    CHECK(!M::preparePosition(r, c, &ref, out)); CHECK(noExpiry.unchanged(out));
    ref.nowUs = UINT64_MAX;
    CHECK(!M::preparePosition(r, c, &ref, out)); CHECK(noExpiry.unchanged(out));
    ref = reference(c); ref.observedUs = ref.nowUs = INT64_MAX;
    CHECK(M::preparePosition(r, c, &ref, out));
    ref.observedUs = ref.nowUs = UINT64_MAX;
    const Saved<M::PreparedTarget> timeLimit(out);
    CHECK(!M::preparePosition(r, c, &ref, out)); CHECK(timeLimit.unchanged(out));
    ref = reference(c); ref.source = M::ScaleSource::UNKNOWN;
    CHECK(!M::preparePosition(r, c, &ref, out)); CHECK(timeLimit.unchanged(out));
    ref = reference(c); ++ref.referenceGeneration;
    CHECK(!M::preparePosition(r, c, &ref, out)); CHECK(timeLimit.unchanged(out));
    ref = reference(c); ++ref.binding.configurationGeneration;
    CHECK(!M::preparePosition(r, c, &ref, out)); CHECK(timeLimit.unchanged(out));
    ref = reference(c); ref.observedUs = ref.nowUs = ref.maximumAgeUs = 0;
    CHECK(M::preparePosition(r, c, &ref, out)); // Zero is a valid monotonic timestamp.
    const Saved<M::PreparedTarget> timeZero(out);
    ref.observedUs = 1;
    CHECK(!M::preparePosition(r, c, &ref, out)); CHECK(timeZero.unchanged(out));
}

void configurationAndReferenceEpochs() {
    M::AxisConfig c = config();
    c.originKnown = c.encoderOriginKnown = c.softLimitsKnown = true;
    c.originNative = 17; c.encoderOriginNative = 10;
    c.originSource = c.encoderOriginSource = M::ScaleSource::QUALIFIED;
    c.units.encoder.sourceId = 7; c.units.encoder.countsPerUnit = scale(100);
    c.softMinimum = -100; c.softMaximum = 100;
    M::AxisReference ref = reference(c, 20);
    M::AxisConfig candidate = c; candidate.nativeVelocityTime = M::TimeUnit::MINUTE;
    ref.stationary = false;
    const Saved<M::AxisConfig> saved(c); const Saved<M::AxisReference> savedRef(ref);
    CHECK(!M::configureAxis(c, candidate, ref, &ref));
    CHECK(saved.unchanged(c) && savedRef.unchanged(ref));
    ref.stationary = true; candidate = c; candidate.softMinimum = -90;
    const M::Generation oldGeneration = c.binding.configurationGeneration;
    const uint64_t originalObserved = ref.observedUs;
    CHECK(M::configureAxis(c, candidate, ref, &ref));
    CHECK(c.binding.configurationGeneration == oldGeneration + 1);
    CHECK(c.originKnown && c.encoderOriginKnown && ref.nativeKnown);
    CHECK(ref.observedUs == originalObserved && ref.nowUs == 110 && ref.maximumAgeUs == 20);
    M::PositionRequest r = position(c, 1); r.relative = true;
    M::PreparedTarget out; CHECK(M::preparePosition(r, c, &ref, out));
    ref.nowUs = 121;
    const Saved<M::PreparedTarget> stale(out);
    CHECK(!M::preparePosition(r, c, &ref, out)); CHECK(stale.unchanged(out));
    ref.nowUs = 110;
    CHECK(M::configureAxis(c, c, ref, &ref)); // Both documented aliases are supported.
    CHECK(c.originKnown && ref.observedUs == originalObserved);
    candidate = c; candidate.units.commandPolarity = -1;
    CHECK(M::configureAxis(c, candidate, ref, &ref));
    CHECK(!c.originKnown && !c.encoderOriginKnown && !c.softLimitsKnown && !ref.nativeKnown);

    c = config(); ref = reference(c, 9); candidate = c;
    candidate.originKnown = true; candidate.originSource = M::ScaleSource::QUALIFIED;
    const Saved<M::AxisConfig> cannotInject(c);
    CHECK(!M::configureAxis(c, candidate, ref)); CHECK(cannotInject.unchanged(c));
    CHECK(!M::setAxisOrigin(c, 10, ref, M::AxisOrigin::ENCODER)); CHECK(cannotInject.unchanged(c));
    c.units.encoder.countsPerUnit = scale(100); c.units.encoder.sourceId = 7;
    ref = reference(c, 9);
    CHECK(M::setAxisOrigin(c, 10, ref, M::AxisOrigin::ENCODER));
    CHECK(c.encoderOriginKnown && c.encoderOriginNative == 10 && !c.originKnown);
    ref = reference(c, 9);
    const Saved<M::AxisConfig> selectedOrigin(c);
    CHECK(!M::setAxisOrigin(c, 0, ref, static_cast<M::AxisOrigin>(255)));
    CHECK(selectedOrigin.unchanged(c));
    c.softLimitsKnown = true; c.softMinimum = -100; c.softMaximum = 100;
    ref = reference(c, 9);
    CHECK(M::setAxisOrigin(c, 9, ref));
    CHECK(c.originKnown && c.originNative == 9 && !c.softLimitsKnown);
    CHECK(c.encoderOriginKnown && c.encoderOriginNative == 10);
    CHECK(!M::setAxisOrigin(c, 9, ref)); // Old witness cannot be recycled after the epoch change.
    ref = reference(c, 9);
    CHECK(M::setAxisOrigin(c, 11, ref, M::AxisOrigin::ENCODER));
    CHECK(c.encoderOriginNative == 11 && c.originKnown && c.originNative == 9);

    ref = reference(c, 9); r = position(c, 3);
    CHECK(M::preparePosition(r, c, nullptr, out));
    r.referencePolicy = M::ReferencePolicy::REQUIRED;
    M::PreparedTarget dependent;
    CHECK(M::preparePosition(r, c, &ref, dependent));
    const M::Generation configurationGeneration = c.binding.configurationGeneration;
    CHECK(M::invalidateAxisReference(c, ref, M::ReferenceLoss::POSITION));
    CHECK(c.originKnown && c.encoderOriginKnown && !ref.nativeKnown);
    CHECK(c.binding.configurationGeneration == configurationGeneration);
    CHECK(M::validatePreparedTarget(out, c, nullptr, 110)); // Absolute arithmetic has no position-witness dependency.
    CHECK(!M::validatePreparedTarget(dependent, c, &ref, 110));
    ref = reference(c, 9);
    CHECK(M::invalidateAxisReference(c, ref, M::ReferenceLoss::COORDINATES));
    CHECK(!c.originKnown && !c.encoderOriginKnown && !c.softLimitsKnown && !ref.nativeKnown);
    CHECK(c.binding.configurationGeneration == configurationGeneration + 1);
    CHECK(!M::validatePreparedTarget(out, c, nullptr, 110));

    c = config(); c.referenceGeneration = UINT32_MAX; ref = reference(c);
    r = position(c, 1); CHECK(M::preparePosition(r, c, nullptr, out));
    CHECK(!M::invalidateAxisReference(c, ref, M::ReferenceLoss::POSITION));
    CHECK(c.exhausted && !ref.nativeKnown);
    CHECK(!M::validatePreparedTarget(out, c, nullptr, 110));
    CHECK(!M::invalidateAxisReference(c, ref)); CHECK(c.exhausted);
    c = config(); c.binding.configurationGeneration = UINT32_MAX; ref = reference(c);
    candidate = c;
    const Saved<M::AxisConfig> exhaustedConfig(c); const Saved<M::AxisReference> exhaustedRef(ref);
    CHECK(!M::configureAxis(c, candidate, ref, &ref));
    CHECK(exhaustedConfig.unchanged(c) && exhaustedRef.unchanged(ref));
    CHECK(!M::setAxisOrigin(c, 0, ref)); CHECK(exhaustedConfig.unchanged(c));
    CHECK(!M::invalidateAxisReference(c, ref)); CHECK(c.exhausted && !ref.nativeKnown);

    c = config(); c.binding.configurationGeneration = UINT32_MAX - 1; ref = reference(c);
    CHECK(M::configureAxis(c, c, ref, &ref));
    CHECK(c.binding.configurationGeneration == UINT32_MAX && !c.exhausted);
    candidate = c;
    const Saved<M::AxisConfig> lastEpoch(c); const Saved<M::AxisReference> lastReference(ref);
    CHECK(!M::configureAxis(c, candidate, ref, &ref));
    CHECK(lastEpoch.unchanged(c) && lastReference.unchanged(ref));
}

void wrappedAngles() {
    M::AxisConfig c = config(); c.units.commandStepsPerMotorTurn = scale(360);
    c.originKnown = true; c.originSource = M::ScaleSource::QUALIFIED;
    M::PositionRequest r = position(c, 0); r.unit = M::PositionUnit::DEGREES;
    r.frame = M::CoordinateFrame::MOTOR; r.wrapped = true;
    M::PreparedTarget out;
    for (int polarity = -1; polarity <= 1; polarity += 2) {
        c.units.commandPolarity = static_cast<int8_t>(polarity);
        const int origins[] = {-17, 0, 17};
        for (unsigned oi = 0; oi < 3; ++oi) {
            c.originNative = origins[oi];
            for (int current = -730; current <= 730; current += 73) {
                const M::AxisReference ref = reference(c, current);
                for (int angle = -720; angle <= 720; angle += 45) {
                    r.value = M::Rational(angle);
                    int positive = (origins[oi] + polarity * angle - current) % 360;
                    if (positive < 0) positive += 360;
                    const int negative = positive ? positive - 360 : 0;
                    const M::AnglePath paths[] = {M::AnglePath::POSITIVE,
                        M::AnglePath::NEGATIVE, M::AnglePath::SHORTEST};
                    for (unsigned pi = 0; pi < 3; ++pi) {
                        r.path = paths[pi]; r.tie = M::HalfTurnTie::REJECT;
                        if (r.path == M::AnglePath::SHORTEST && positive == 180) {
                            const Saved<M::PreparedTarget> saved(out);
                            CHECK(!M::preparePosition(r, c, &ref, out)); CHECK(saved.unchanged(out));
                            continue;
                        }
                        const int delta = r.path == M::AnglePath::SHORTEST ?
                            (positive < 180 ? positive : negative) :
                            (((r.path == M::AnglePath::POSITIVE) == (polarity > 0)) ? positive : negative);
                        CHECK(M::preparePosition(r, c, &ref, out));
                        CHECK(out.endpointNative == current + delta);
                    }
                }
            }
        }
    }
    c.originNative = 0; c.units.commandPolarity = -1;
    M::AxisReference ref = reference(c, 180); r.value = M::Rational(0);
    r.path = M::AnglePath::SHORTEST; r.tie = M::HalfTurnTie::POSITIVE;
    CHECK(M::preparePosition(r, c, &ref, out)); CHECK(out.endpointNative == 0);
    r.tie = M::HalfTurnTie::NEGATIVE;
    CHECK(M::preparePosition(r, c, &ref, out)); CHECK(out.endpointNative == 360);
    c.units.commandPolarity = 1; ref = reference(c, 20);
    c.softLimitsKnown = true; c.softMinimum = 0; c.softMaximum = 100;
    r.path = M::AnglePath::POSITIVE;
    const Saved<M::PreparedTarget> saved(out);
    CHECK(!M::preparePosition(r, c, &ref, out)); CHECK(saved.unchanged(out));
    // A legal negative path must not silently replace the selected positive one.
    c.softLimitsKnown = false; c.units.commandStepsPerMotorTurn = scale(1000, 3);
    r.value = M::Rational(90); r.path = M::AnglePath::SHORTEST;
    r.rounding = M::Rounding::NEAREST; r.maximumQuantizationError = M::Rational(1, 3);
    ref = reference(c);
    CHECK(M::preparePosition(r, c, &ref, out)); CHECK(out.endpointNative == 83);
    CHECK(out.requestedNative.integral == 83 && out.requestedNative.numerator == 1 &&
        out.requestedNative.denominator == 3);
    c.units.commandStepsPerMotorTurn = scale(1000);
    r.value = M::Rational(252); r.path = M::AnglePath::NEGATIVE; ref = reference(c, INT64_MAX);
    CHECK(M::preparePosition(r, c, &ref, out)); CHECK(out.endpointNative == INT64_MAX - 107);
    r.path = M::AnglePath::POSITIVE; ref = reference(c, INT64_MIN);
    CHECK(M::preparePosition(r, c, &ref, out)); CHECK(out.endpointNative == INT64_MIN + 508);
}

void floatingAndApproximateBoundaries() {
    M::AxisConfig c = config(); c.positionRepresentation = M::NativeRepresentation::BINARY32;
    M::PositionRequest r = position(c, 1, 10);
    r.maximumQuantizationError = M::Rational(1, 671088640);
    M::PreparedTarget out;
    CHECK(M::preparePosition(r, c, nullptr, out));
    CHECK(floatBits(out.effectiveFloat) == 0x3DCCCCCD && out.absoluteErrorBound > 0);
    const Saved<M::PreparedTarget> saved(out);
    r.maximumQuantizationError = M::Rational(1, 671088641);
    CHECK(!M::preparePosition(r, c, nullptr, out)); CHECK(saved.unchanged(out));
    r = position(c, 16777217); r.maximumQuantizationError = M::Rational(1);
    CHECK(M::preparePosition(r, c, nullptr, out)); CHECK(out.effectiveFloat == 16777216.0f);
    r.maximumQuantizationError = M::Rational(0);
    const Saved<M::PreparedTarget> integerPrecision(out);
    CHECK(!M::preparePosition(r, c, nullptr, out)); CHECK(integerPrecision.unchanged(out));
    c.nativeMinimum = 0; c.nativeMaximum = 16777219;
    r.value = M::Rational(16777219); r.maximumQuantizationError = M::Rational(1);
    CHECK(!M::preparePosition(r, c, nullptr, out)); CHECK(integerPrecision.unchanged(out));

    // A binary32 displacement can exceed int64 while its exact endpoint is legal.
    c = config(); c.positionRepresentation = M::NativeRepresentation::BINARY32;
    r = position(c, INT64_MAX); r.relative = true; r.maximumQuantizationError = M::Rational(1);
    M::AxisReference floatRef = reference(c, -1);
    CHECK(M::preparePosition(r, c, &floatRef, out));
    CHECK(floatBits(out.effectiveFloat) == 0x5F000000);
    CHECK(out.requestedNative.integral == INT64_MAX - 1);
    floatRef = reference(c, INT64_MIN);
    CHECK(M::preparePosition(r, c, &floatRef, out));
    CHECK(out.requestedNative.integral == -1 && out.endpointFloat == 0);

    // Binary64 comparison would merge the reference with the lower float here.
    c.units.commandStepsPerMotorTurn = scale(360);
    c.originKnown = true; c.originSource = M::ScaleSource::QUALIFIED;
    r = position(c, 34, 360); r.unit = M::PositionUnit::TURNS;
    r.frame = M::CoordinateFrame::MOTOR; r.wrapped = true; r.path = M::AnglePath::POSITIVE;
    r.maximumQuantizationError = M::Rational(2);
    floatRef = reference(c, INT64_C(9007199254740993));
    const Saved<M::PreparedTarget> reversed(out);
    CHECK(!M::preparePosition(r, c, &floatRef, out)); CHECK(reversed.unchanged(out));

    c = config(); c.units.commandStepsPerMotorTurn = scale(1000);
    r = position(c, 1); r.unit = M::PositionUnit::RADIANS;
    r.frame = M::CoordinateFrame::MOTOR; r.relative = true;
    M::AxisReference ref = reference(c);
    CHECK(!M::preparePosition(r, c, &ref, out)); CHECK(reversed.unchanged(out));
    r.approximate = true; r.maximumApproximationError = 1e-8;
    r.rounding = M::Rounding::NEAREST; r.maximumQuantizationError = M::Rational(1);
    CHECK(M::preparePosition(r, c, &ref, out));
    CHECK(out.effectiveNative == 159 && !out.exactArithmetic && out.approximationErrorBound > 0);
    const Saved<M::PreparedTarget> approximate(out);
    r.maximumApproximationError = 1e-30;
    CHECK(!M::preparePosition(r, c, &ref, out)); CHECK(approximate.unchanged(out));
    r.maximumApproximationError = std::numeric_limits<double>::infinity();
    CHECK(!M::preparePosition(r, c, &ref, out)); CHECK(approximate.unchanged(out));
    r.approximate = false; r.maximumApproximationError = 0; r.value = M::Rational(0);
    r.rounding = M::Rounding::EXACT; r.maximumQuantizationError = M::Rational(0);
    ref = reference(c, INT64_MAX);
    CHECK(M::preparePosition(r, c, &ref, out)); CHECK(out.endpointNative == INT64_MAX && out.exactArithmetic);
}

void wrappedRadianIntervals() {
    M::AxisConfig c = config(); c.units.commandStepsPerMotorTurn = scale(360);
    c.originKnown = true; c.originNative = 17; c.originSource = M::ScaleSource::QUALIFIED;
    M::AxisReference ref = reference(c, 20);
    M::PositionRequest r = position(c, 1); r.unit = M::PositionUnit::RADIANS;
    r.frame = M::CoordinateFrame::MOTOR; r.wrapped = true; r.approximate = true;
    r.rounding = M::Rounding::NEAREST; r.maximumQuantizationError = M::Rational(1, 2);
    r.maximumApproximationError = 1e-8;
    M::PreparedTarget out;
    CHECK(M::preparePosition(r, c, &ref, out)); CHECK(out.endpointNative == 74);
    CHECK(!out.exactArithmetic && out.approximationErrorBound > 0);

    c.units.commandStepsPerMotorTurn = scale(1); c.originNative = 0; ref = reference(c);
    r.value = M::Rational(INT64_C(3141592653589793), UINT64_C(1000000000000000));
    r.tie = M::HalfTurnTie::POSITIVE;
    const Saved<M::PreparedTarget> saved(out);
    M::Status status = M::preparePosition(r, c, &ref, out);
    CHECK(!status && status.detail == static_cast<int32_t>(M::AxisError::AMBIGUOUS));
    CHECK(saved.unchanged(out)); // Explicit tie policy cannot resolve an approximate half-turn interval.
    r.value = M::Rational(INT64_C(6283185307179586), UINT64_C(1000000000000000));
    status = M::preparePosition(r, c, &ref, out);
    CHECK(!status && status.detail == static_cast<int32_t>(M::AxisError::AMBIGUOUS));
    CHECK(saved.unchanged(out)); // Approximate orientation crosses the full-turn boundary.
}

void signedWideVelocity() {
    M::AxisConfig c = config(); c.nativeMinimum = 0; c.nativeMaximum = 1;
    M::VelocityRequest r; r.configurationGeneration = c.binding.configurationGeneration;
    M::PreparedVelocityTarget out;
    const int64_t values[] = {INT64_MIN, -70000, 0, 70000, INT64_MAX};
    for (unsigned i = 0; i < sizeof values / sizeof values[0]; ++i) {
        r.value = M::Rational(values[i]); CHECK(M::prepareVelocityTarget(r, c, out));
        CHECK(out.effectiveNative == values[i]);
    }
    c.units.commandStepsPerMotorTurn = scale(4096); c.units.motorTurnsPerLoadTurn = scale(3, 2);
    r.frame = M::CoordinateFrame::LOAD; r.unit = M::VelocityUnit(M::PositionUnit::TURNS);
    r.value = M::Rational(-1, 2);
    CHECK(M::prepareVelocityTarget(r, c, out)); CHECK(out.effectiveNative == -3072);
    r.frame = M::CoordinateFrame::MOTOR;
    CHECK(M::prepareVelocityTarget(r, c, out)); CHECK(out.effectiveNative == -2048);
    c.units.commandStepsPerMotorTurn = scale(60);
    r.unit = M::VelocityUnit(M::PositionUnit::TURNS, M::TimeUnit::MINUTE);
    r.value = M::Rational(-70000);
    CHECK(M::prepareVelocityTarget(r, c, out)); CHECK(out.effectiveNative == -70000);
    r.value = M::Rational(0);
    CHECK(M::prepareVelocityTarget(r, c, out)); CHECK(out.effectiveNative == 0);
    const Saved<M::PreparedVelocityTarget> saved(out);
    r.value = M::Rational(INT64_MAX); r.unit.time = M::TimeUnit::MILLISECOND;
    CHECK(!M::prepareVelocityTarget(r, c, out)); CHECK(saved.unchanged(out));
    r.frame = M::CoordinateFrame::NATIVE; r.value = M::Rational(1);
    CHECK(!M::prepareVelocityTarget(r, c, out)); CHECK(saved.unchanged(out));
    r.unit = M::VelocityUnit(); ++r.configurationGeneration;
    CHECK(!M::prepareVelocityTarget(r, c, out)); CHECK(saved.unchanged(out));
    r.configurationGeneration = c.binding.configurationGeneration;
    c.velocityRepresentation = M::NativeRepresentation::BINARY32;
    r.value = M::Rational(1, 10); r.maximumQuantizationError = M::Rational(1, 671088640);
    CHECK(M::prepareVelocityTarget(r, c, out)); CHECK(floatBits(out.effectiveFloat) == 0x3DCCCCCD);
    const Saved<M::PreparedVelocityTarget> floating(out);
    r.maximumQuantizationError = M::Rational(1, 671088641);
    CHECK(!M::prepareVelocityTarget(r, c, out)); CHECK(floating.unchanged(out));
}
} // namespace

int main() {
    nativeExactAndAtomicFailures();
    independentScalesAndOrigins();
    roundingOriginsAndOverflow();
    inclusiveLimitsAndChosenBasis();
    freshnessAndPreparedDependencies();
    configurationAndReferenceEpochs();
    wrappedAngles();
    floatingAndApproximateBoundaries();
    wrappedRadianIntervals();
    signedWideVelocity();
    std::puts("pure axis coordinate/reference contracts passed");
}
