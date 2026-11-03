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

// Binary32 spacing is 4 near 2^25, so a small positive wrapped move can round
// below its witness. Exact and approximate preparation share this rejection.
void wrappedFloatRoundingReversal() {
    M::AxisConfig c = config();
    c.positionRepresentation = M::NativeRepresentation::BINARY32;
    c.units.commandStepsPerMotorTurn = scale(1000);
    c.originKnown = true; c.originSource = M::ScaleSource::QUALIFIED;
    const M::AxisReference ref = reference(c, 33554433); // 433 steps into its turn.
    M::PositionRequest r = position(c, 4332, 10000); // Turn 0.4332 is 0.2 steps ahead.
    r.frame = M::CoordinateFrame::MOTOR; r.unit = M::PositionUnit::TURNS;
    r.wrapped = true; r.path = M::AnglePath::POSITIVE; r.maximumQuantizationError = M::Rational(2);
    M::PreparedTarget out;
    const Saved<M::PreparedTarget> saved(out);
    M::Status status = M::preparePosition(r, c, &ref, out);
    CHECK(!status && status.detail == static_cast<int32_t>(M::AxisError::AMBIGUOUS));
    CHECK(saved.unchanged(out));
    r.value = M::Rational(4360, 10000); // Three steps ahead lands exactly on a binary32 value.
    CHECK(M::preparePosition(r, c, &ref, out));
    CHECK(out.effectiveFloat == 33554436.0f && out.endpointFloat == 33554436.0);

    M::PreparedTarget approximate;
    const Saved<M::PreparedTarget> untouched(approximate);
    r.unit = M::PositionUnit::RADIANS; r.approximate = true; r.maximumApproximationError = 1e-6;
    r.value = M::Rational(INT64_C(2721875875), UINT64_C(1000000000)); // Same 0.2-step move.
    status = M::preparePosition(r, c, &ref, approximate);
    CHECK(!status && status.detail == static_cast<int32_t>(M::AxisError::AMBIGUOUS));
    CHECK(untouched.unchanged(approximate));
    r.value = M::Rational(INT64_C(2739468793), UINT64_C(1000000000)); // Turn 0.436 again.
    CHECK(M::preparePosition(r, c, &ref, approximate));
    CHECK(approximate.effectiveFloat == 33554436.0f && !approximate.exactArithmetic);
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

bool failedWith(const M::Status& status, M::AxisError error) {
    return !status && status.detail == static_cast<int32_t>(error);
}

M::AxisConfig motorRpmAxis() {
    M::AxisConfig c = config();
    c.nativeRateBasis = M::NativeRateBasis::MOTOR_TURNS;
    c.nativeVelocityTime = M::TimeUnit::MINUTE;
    return c;
}

M::VelocityRequest rate(const M::AxisConfig& c, int64_t n, uint64_t d = 1,
    M::PositionUnit unit = M::PositionUnit::TURNS, M::TimeUnit time = M::TimeUnit::MINUTE,
    M::CoordinateFrame frame = M::CoordinateFrame::MOTOR) {
    M::VelocityRequest r;
    r.configurationGeneration = c.binding.configurationGeneration;
    r.value = M::Rational(n, d);
    r.unit = M::VelocityUnit(unit, time);
    r.frame = frame;
    return r;
}

// 3,200 increments/motor turn with gear 5: one load turn/minute is 5 motor rpm,
// and only an explicit increment destination yields 16,000.
void nativeRateDestination() {
    M::AxisConfig c = motorRpmAxis();
    c.units.commandStepsPerMotorTurn = scale(3200); c.units.motorTurnsPerLoadTurn = scale(5);
    M::PreparedVelocityTarget out;
    M::VelocityRequest r = rate(c, 1); r.frame = M::CoordinateFrame::LOAD;
    CHECK(M::prepareVelocityTarget(r, c, out));
    CHECK(out.effectiveNative == 5 && out.requestedNative.integral == 5 && out.absoluteErrorBound == 0);
    CHECK(out.rateBasis == M::NativeRateBasis::MOTOR_TURNS && out.rateTime == M::TimeUnit::MINUTE);
    r.frame = M::CoordinateFrame::MOTOR;
    CHECK(M::prepareVelocityTarget(r, c, out)); CHECK(out.effectiveNative == 1);
    M::AxisConfig increments = c; increments.nativeRateBasis = M::NativeRateBasis::COMMAND_INCREMENTS;
    r.frame = M::CoordinateFrame::LOAD;
    CHECK(M::prepareVelocityTarget(r, increments, out));
    CHECK(out.effectiveNative == 16000 && out.rateBasis == M::NativeRateBasis::COMMAND_INCREMENTS);

    // Increments have no gear dependency, in either mechanical frame.
    const M::CoordinateFrame frames[] = {M::CoordinateFrame::MOTOR, M::CoordinateFrame::LOAD};
    for (unsigned i = 0; i < 2; ++i) {
        r = rate(c, 3200, 1, M::PositionUnit::STEPS, M::TimeUnit::MINUTE, frames[i]);
        CHECK(M::prepareVelocityTarget(r, c, out)); CHECK(out.effectiveNative == 1);
        c.units.motorTurnsPerLoadTurn = scale(1);
        CHECK(M::prepareVelocityTarget(r, c, out)); CHECK(out.effectiveNative == 1);
        c.units.motorTurnsPerLoadTurn = M::UnitScale(); // Unknown gear is not consulted either.
        CHECK(M::prepareVelocityTarget(r, c, out)); CHECK(out.effectiveNative == 1);
        c.units.motorTurnsPerLoadTurn = scale(5);
        CHECK(M::prepareVelocityTarget(r, increments, out)); CHECK(out.effectiveNative == 3200);
    }

    // A caller's 1/1 command scale would misconvert increments; the real scale is required.
    M::AxisConfig missing = c; missing.units.commandStepsPerMotorTurn = M::UnitScale();
    r = rate(missing, 3200, 1, M::PositionUnit::STEPS);
    const Saved<M::PreparedVelocityTarget> saved(out);
    M::Status status = M::prepareVelocityTarget(r, missing, out);
    CHECK(!status && status.code == M::Err::INVALID_CONFIG); CHECK(saved.unchanged(out));
    r = rate(missing, 60);
    CHECK(M::prepareVelocityTarget(r, missing, out)); CHECK(out.effectiveNative == 60);

    // NATIVE is the destination dimension, identity only, without polarity.
    c.units.commandPolarity = -1;
    r = rate(c, 60, 1, M::PositionUnit::TURNS, M::TimeUnit::MINUTE, M::CoordinateFrame::NATIVE);
    CHECK(M::prepareVelocityTarget(r, c, out)); CHECK(out.effectiveNative == 60);
    r.frame = M::CoordinateFrame::MOTOR;
    CHECK(M::prepareVelocityTarget(r, c, out)); CHECK(out.effectiveNative == -60);
    r.frame = M::CoordinateFrame::LOAD;
    CHECK(M::prepareVelocityTarget(r, c, out)); CHECK(out.effectiveNative == -300);
    r = rate(c, 6400, 1, M::PositionUnit::STEPS);
    CHECK(M::prepareVelocityTarget(r, c, out)); CHECK(out.effectiveNative == -2);
    CHECK(M::prepareVelocityTarget(r, increments, out)); CHECK(out.effectiveNative == 6400);
    c.units.commandPolarity = 1;
    const Saved<M::PreparedVelocityTarget> dimension(out);
    r = rate(c, 3200, 1, M::PositionUnit::STEPS, M::TimeUnit::MINUTE, M::CoordinateFrame::NATIVE);
    CHECK(failedWith(M::prepareVelocityTarget(r, c, out), M::AxisError::INVALID_ARGUMENT));
    r = rate(c, 1, 1, M::PositionUnit::TURNS, M::TimeUnit::SECOND, M::CoordinateFrame::NATIVE);
    CHECK(failedWith(M::prepareVelocityTarget(r, c, out), M::AxisError::INVALID_ARGUMENT));
    r = rate(c, 1, 1, M::PositionUnit::TURNS, M::TimeUnit::MINUTE, M::CoordinateFrame::NATIVE);
    CHECK(failedWith(M::prepareVelocityTarget(r, increments, out), M::AxisError::INVALID_ARGUMENT));
    r = rate(c, 1, 1, M::PositionUnit::MILLIMETRES, M::TimeUnit::MINUTE);
    CHECK(failedWith(M::prepareVelocityTarget(r, c, out), M::AxisError::INVALID_ARGUMENT));
    CHECK(dimension.unchanged(out));

    // Position coordinates keep their increment meaning under an rpm rate basis.
    c.originKnown = true; c.originSource = M::ScaleSource::QUALIFIED;
    M::PositionRequest p = position(c, 1); p.unit = M::PositionUnit::TURNS; p.frame = M::CoordinateFrame::MOTOR;
    M::PreparedTarget target;
    CHECK(M::preparePosition(p, c, nullptr, target)); CHECK(target.endpointNative == 3200);
    p.frame = M::CoordinateFrame::LOAD;
    CHECK(M::preparePosition(p, c, nullptr, target)); CHECK(target.endpointNative == 16000);
    p = position(c, 3200);
    CHECK(M::preparePosition(p, c, nullptr, target)); CHECK(target.endpointNative == 3200);
    const Saved<M::PreparedTarget> nativePosition(target);
    p.unit = M::PositionUnit::TURNS;
    CHECK(failedWith(M::preparePosition(p, c, nullptr, target), M::AxisError::INVALID_ARGUMENT));
    CHECK(nativePosition.unchanged(target));
    c.nativeRateBasis = static_cast<M::NativeRateBasis>(2);
    CHECK(!M::validateAxisConfig(c));
    CHECK(!M::prepareVelocityTarget(rate(motorRpmAxis(), 1), c, out));
}

// Quantize once, in the destination: allowance and error are rpm, not increments.
void nativeRateQuantization() {
    M::AxisConfig c = motorRpmAxis(); c.units.commandStepsPerMotorTurn = scale(3200);
    M::PreparedVelocityTarget out;
    // Quantizing increments first would round 4799.6 to 4800, 1.5 rpm, then to 2.
    M::VelocityRequest r = rate(c, 47996, 10, M::PositionUnit::STEPS);
    r.rounding = M::Rounding::NEAREST; r.maximumQuantizationError = M::Rational(1, 2);
    CHECK(M::prepareVelocityTarget(r, c, out));
    CHECK(out.effectiveNative == 1 && out.requestedNative.integral == 1);
    CHECK(out.requestedNative.numerator == 3999 && out.requestedNative.denominator == 8000);
    CHECK(out.quantization.errorNumerator == 3999 && out.quantization.errorDenominator == 8000);
    CHECK(out.quantization.errorNegative && out.absoluteErrorBound > 0.49 && out.absoluteErrorBound < 0.5);
    const Saved<M::PreparedVelocityTarget> saved(out);
    r.maximumQuantizationError = M::Rational(3998, 8000);
    CHECK(!M::prepareVelocityTarget(r, c, out)); CHECK(saved.unchanged(out));
    r.maximumQuantizationError = M::Rational(1, 2); r.rounding = M::Rounding::EXACT;
    CHECK(!M::prepareVelocityTarget(r, c, out)); CHECK(saved.unchanged(out));

    // A half rpm crosses the rounding boundary while being 1,600 whole increments.
    r = rate(c, 1600, 1, M::PositionUnit::STEPS);
    r.rounding = M::Rounding::NEAREST; r.maximumQuantizationError = M::Rational(1, 2);
    CHECK(M::prepareVelocityTarget(r, c, out)); CHECK(out.effectiveNative == 0);
    r.value = M::Rational(4800);
    CHECK(M::prepareVelocityTarget(r, c, out)); CHECK(out.effectiveNative == 2); // Ties to even.
    r.value = M::Rational(1601); r.maximumQuantizationError = M::Rational(1);
    CHECK(M::prepareVelocityTarget(r, c, out)); CHECK(out.effectiveNative == 1);

    c.velocityRepresentation = M::NativeRepresentation::BINARY32;
    r = rate(c, 1, 10); r.maximumQuantizationError = M::Rational(1, 671088640);
    CHECK(M::prepareVelocityTarget(r, c, out));
    CHECK(floatBits(out.effectiveFloat) == 0x3DCCCCCD && out.representation == M::NativeRepresentation::BINARY32);
    const Saved<M::PreparedVelocityTarget> floating(out);
    r.maximumQuantizationError = M::Rational(1, 671088641);
    CHECK(!M::prepareVelocityTarget(r, c, out)); CHECK(floating.unchanged(out));
    r = rate(c, 320, 1, M::PositionUnit::STEPS); r.maximumQuantizationError = M::Rational(1, 671088640);
    CHECK(M::prepareVelocityTarget(r, c, out)); CHECK(floatBits(out.effectiveFloat) == 0x3DCCCCCD);

    c = motorRpmAxis(); c.units.motorTurnsPerLoadTurn = scale(5);
    r = rate(c, 6283185, 1000000, M::PositionUnit::RADIANS, M::TimeUnit::SECOND);
    const Saved<M::PreparedVelocityTarget> before(out);
    CHECK(failedWith(M::prepareVelocityTarget(r, c, out), M::AxisError::INVALID_ARGUMENT));
    CHECK(before.unchanged(out)); // Radians need explicit approximation.
    r.approximate = true; r.maximumApproximationError = 1e-6;
    r.rounding = M::Rounding::NEAREST; r.maximumQuantizationError = M::Rational(1, 1000);
    CHECK(M::prepareVelocityTarget(r, c, out));
    CHECK(out.effectiveNative == 60 && !out.exactArithmetic && out.absoluteErrorBound < 0.001);
    CHECK(std::fabs(out.approximateRequestedNative - 60) < 1e-5);
    r.frame = M::CoordinateFrame::LOAD;
    CHECK(M::prepareVelocityTarget(r, c, out)); CHECK(out.effectiveNative == 300);
    c.units.commandPolarity = -1;
    CHECK(M::prepareVelocityTarget(r, c, out)); CHECK(out.effectiveNative == -300);
    c.units.commandPolarity = 1; r.frame = M::CoordinateFrame::MOTOR;
    r.value = M::Rational(1570796, 1000000); r.rounding = M::Rounding::EXACT;
    const Saved<M::PreparedVelocityTarget> inexact(out);
    CHECK(failedWith(M::prepareVelocityTarget(r, c, out), M::AxisError::AMBIGUOUS)); CHECK(inexact.unchanged(out));
    r.frame = M::CoordinateFrame::NATIVE;
    CHECK(failedWith(M::prepareVelocityTarget(r, c, out), M::AxisError::INVALID_ARGUMENT)); CHECK(inexact.unchanged(out));
    r = rate(c, 0, 1, M::PositionUnit::RADIANS, M::TimeUnit::SECOND);
    CHECK(M::prepareVelocityTarget(r, c, out)); CHECK(out.effectiveNative == 0 && out.exactArithmetic);
    c.velocityRepresentation = M::NativeRepresentation::BINARY32;
    r = rate(c, 6283185, 1000000, M::PositionUnit::RADIANS, M::TimeUnit::SECOND);
    r.approximate = true; r.maximumApproximationError = 1e-6; r.maximumQuantizationError = M::Rational(1, 1000);
    CHECK(M::prepareVelocityTarget(r, c, out)); // 59.99999707 rpm; the binary32 below 60 is nearer.
    CHECK(out.effectiveFloat == 59.99999618530273f && !out.exactArithmetic);
}

void nativeRateBoundaries() {
    M::AxisConfig c = motorRpmAxis(); M::PreparedVelocityTarget out;
    M::VelocityRequest r = rate(c, INT64_MIN, 1, M::PositionUnit::TURNS, M::TimeUnit::MINUTE, M::CoordinateFrame::NATIVE);
    CHECK(M::prepareVelocityTarget(r, c, out)); CHECK(out.effectiveNative == INT64_MIN);
    r.value = M::Rational(INT64_MAX);
    CHECK(M::prepareVelocityTarget(r, c, out)); CHECK(out.effectiveNative == INT64_MAX);
    const Saved<M::PreparedVelocityTarget> saved(out);
    // Polarity cannot negate INT64_MIN, and time scaling cannot exceed fixed storage.
    c.units.commandPolarity = -1; r = rate(c, INT64_MIN);
    CHECK(!M::prepareVelocityTarget(r, c, out)); CHECK(saved.unchanged(out));
    r.value = M::Rational(INT64_MAX);
    CHECK(M::prepareVelocityTarget(r, c, out)); CHECK(out.effectiveNative == -INT64_MAX);
    c.units.commandPolarity = 1;
    const Saved<M::PreparedVelocityTarget> extreme(out);
    r = rate(c, INT64_MAX, 1, M::PositionUnit::TURNS, M::TimeUnit::SECOND);
    CHECK(!M::prepareVelocityTarget(r, c, out)); CHECK(extreme.unchanged(out));
    r = rate(c, INT64_MAX, 1, M::PositionUnit::TURNS, M::TimeUnit::MILLISECOND);
    CHECK(!M::prepareVelocityTarget(r, c, out)); CHECK(extreme.unchanged(out));
    c.units.motorTurnsPerLoadTurn = scale(UINT32_MAX);
    r = rate(c, INT64_MAX); r.frame = M::CoordinateFrame::LOAD;
    CHECK(!M::prepareVelocityTarget(r, c, out)); CHECK(extreme.unchanged(out));
    // Large factors cancel exactly before storage checks.
    c.units.commandStepsPerMotorTurn = scale(UINT32_MAX);
    r = rate(c, INT64_MAX, 1, M::PositionUnit::STEPS);
    r.rounding = M::Rounding::TOWARD_ZERO; r.maximumQuantizationError = M::Rational(1);
    CHECK(M::prepareVelocityTarget(r, c, out)); CHECK(out.effectiveNative == INT64_MAX / UINT32_MAX);
    r = rate(c, INT64_MAX, uint64_t(INT64_MAX));
    CHECK(M::prepareVelocityTarget(r, c, out)); CHECK(out.effectiveNative == 1);

    // Zero and nonfinite inputs, with the allowance in the destination.
    const Saved<M::PreparedVelocityTarget> before(out);
    r = rate(c, 1, 0);
    CHECK(failedWith(M::prepareVelocityTarget(r, c, out), M::AxisError::INVALID_ARGUMENT));
    r = rate(c, 1); r.maximumQuantizationError = M::Rational(1, 0);
    CHECK(failedWith(M::prepareVelocityTarget(r, c, out), M::AxisError::INVALID_ARGUMENT));
    r = rate(c, 1); r.maximumQuantizationError = M::Rational(-1);
    CHECK(failedWith(M::prepareVelocityTarget(r, c, out), M::AxisError::INVALID_ARGUMENT));
    const double invalid[] = {std::numeric_limits<double>::quiet_NaN(),
        std::numeric_limits<double>::infinity(), -std::numeric_limits<double>::infinity(), -1};
    for (unsigned i = 0; i < 4; ++i) {
        r = rate(c, 1, 1, M::PositionUnit::RADIANS, M::TimeUnit::SECOND);
        r.approximate = true; r.maximumApproximationError = invalid[i];
        CHECK(failedWith(M::prepareVelocityTarget(r, c, out), M::AxisError::INVALID_ARGUMENT));
    }
    r = rate(c, 1); r.frame = static_cast<M::CoordinateFrame>(255);
    CHECK(failedWith(M::prepareVelocityTarget(r, c, out), M::AxisError::INVALID_ARGUMENT));
    CHECK(before.unchanged(out));

    // The basis is interpretation: changing it needs a new epoch, not a stale request.
    c = motorRpmAxis(); M::AxisReference ref = reference(c, 9);
    M::VelocityRequest current = rate(c, 60);
    CHECK(M::prepareVelocityTarget(current, c, out));
    const M::PreparedVelocityTarget frozen = out;
    M::AxisConfig candidate = c; candidate.nativeRateBasis = M::NativeRateBasis::COMMAND_INCREMENTS;
    candidate.units.commandStepsPerMotorTurn = scale(1000);
    c.originKnown = false;
    CHECK(M::configureAxis(c, candidate, ref, &ref));
    CHECK(c.nativeRateBasis == M::NativeRateBasis::COMMAND_INCREMENTS);
    CHECK(!M::sameBinding(frozen.binding, c.binding));
    CHECK(failedWith(M::prepareVelocityTarget(current, c, out), M::AxisError::STALE_GENERATION));
    current.configurationGeneration = c.binding.configurationGeneration;
    CHECK(M::prepareVelocityTarget(current, c, out));
    CHECK(out.effectiveNative == 60000 && out.rateBasis == M::NativeRateBasis::COMMAND_INCREMENTS);
    // A rate-only change leaves position interpretation and its evidence alone.
    c = motorRpmAxis(); c.originKnown = c.softLimitsKnown = true; c.originSource = M::ScaleSource::QUALIFIED;
    c.softMinimum = -5; c.softMaximum = 5;
    ref = reference(c, 1); candidate = c; candidate.nativeVelocityTime = M::TimeUnit::SECOND;
    CHECK(M::configureAxis(c, candidate, ref, &ref));
    CHECK(c.originKnown && c.softLimitsKnown && ref.nativeKnown && c.nativeVelocityTime == M::TimeUnit::SECOND);
}

// Same exact common intent as MotorControl-RS test/velocity_test.cpp
// testSharedNumericPreparation, without its signed 16-bit limit or zero rejection.
void commonRateIntentVectors() {
    M::AxisConfig a = motorRpmAxis(); M::PreparedVelocityTarget out;
    const int64_t signedRpm[] = {-32769, -32768, -1, 0, 1, 32767, 32768};
    M::VelocityRequest r = rate(a, 60, 1, M::PositionUnit::TURNS, M::TimeUnit::MINUTE, M::CoordinateFrame::NATIVE);
    for (unsigned i = 0; i < sizeof signedRpm / sizeof signedRpm[0]; ++i) {
        r.value = M::Rational(signedRpm[i]);
        CHECK(M::prepareVelocityTarget(r, a, out)); CHECK(out.effectiveNative == signedRpm[i]);
    }
    const M::Rounding modes[] = {M::Rounding::NEAREST, M::Rounding::TOWARD_ZERO, M::Rounding::FLOOR, M::Rounding::CEIL};
    const int positive[] = {2, 1, 1, 2}, negative[] = {-2, -1, -2, -1};
    for (unsigned i = 0; i < 4; ++i) for (int sign = -1; sign <= 1; sign += 2) {
        r.value = M::Rational(sign * 3, 2); r.rounding = modes[i]; r.maximumQuantizationError = M::Rational(1, 2);
        CHECK(M::prepareVelocityTarget(r, a, out));
        CHECK(out.effectiveNative == (sign < 0 ? negative[i] : positive[i]) && out.absoluteErrorBound >= 0.5);
    }
    r.value = M::Rational(5, 2); r.rounding = M::Rounding::NEAREST;
    CHECK(M::prepareVelocityTarget(r, a, out)); CHECK(out.effectiveNative == 2);
    r.rounding = M::Rounding::EXACT; CHECK(!M::prepareVelocityTarget(r, a, out));

    r = rate(a, 60); // Motor rpm needs no command scale, gear or origin.
    CHECK(M::prepareVelocityTarget(r, a, out)); CHECK(out.effectiveNative == 60);
    r = rate(a, 360, 1, M::PositionUnit::DEGREES, M::TimeUnit::SECOND);
    CHECK(M::prepareVelocityTarget(r, a, out)); CHECK(out.effectiveNative == 60);
    r = rate(a, 200, 1, M::PositionUnit::FULL_STEPS, M::TimeUnit::SECOND);
    CHECK(!M::prepareVelocityTarget(r, a, out)); // Full steps are optional geometry, never inferred.
    a.units.fullStepsPerMotorTurn = M::UnitScale(200, 1, M::ScaleSource::DOCUMENTED);
    CHECK(M::prepareVelocityTarget(r, a, out)); CHECK(out.effectiveNative == 60);
    r = rate(a, 1000, 1, M::PositionUnit::STEPS, M::TimeUnit::SECOND);
    CHECK(!M::prepareVelocityTarget(r, a, out));
    a.units.commandStepsPerMotorTurn = scale(1000);
    CHECK(M::prepareVelocityTarget(r, a, out)); CHECK(out.effectiveNative == 60);
    a.units.commandPolarity = -1;
    CHECK(M::prepareVelocityTarget(r, a, out)); CHECK(out.effectiveNative == -60);
    a.units.commandPolarity = 1;
    r = rate(a, 60); r.frame = M::CoordinateFrame::LOAD;
    CHECK(!M::prepareVelocityTarget(r, a, out));
    a.units.motorTurnsPerLoadTurn = scale(3, 2);
    CHECK(M::prepareVelocityTarget(r, a, out)); CHECK(out.effectiveNative == 90);
    r = rate(a, 10, 1, M::PositionUnit::MILLIMETRES, M::TimeUnit::SECOND, M::CoordinateFrame::LOAD);
    CHECK(!M::prepareVelocityTarget(r, a, out));
    a.units.millimetresPerLoadTurn = scale(10);
    CHECK(M::prepareVelocityTarget(r, a, out)); CHECK(out.effectiveNative == 90);
    r = rate(a, 4000, 1, M::PositionUnit::ENCODER_COUNTS, M::TimeUnit::SECOND, M::CoordinateFrame::LOAD);
    a.units.encoder.countsPerUnit = scale(4000); a.units.encoder.sourceId = 11;
    CHECK(M::prepareVelocityTarget(r, a, out)); CHECK(out.effectiveNative == 60);
    a.units.encoder.polarity = -1;
    CHECK(M::prepareVelocityTarget(r, a, out)); CHECK(out.effectiveNative == -60);
    a.units.encoder.polarity = 1; a.units.encoder.sourceId = 0;
    CHECK(!M::prepareVelocityTarget(r, a, out)); a.units.encoder.sourceId = 11;
    a.units.encoder.basis = M::EncoderBasis::MILLIMETRE; a.units.encoder.countsPerUnit = scale(1000);
    r.value = M::Rational(10000);
    CHECK(M::prepareVelocityTarget(r, a, out)); CHECK(out.effectiveNative == 90);
    a.units.encoder.basis = M::EncoderBasis::LOAD_TURN; a.units.encoder.countsPerUnit = scale(4000);
    r.value = M::Rational(4000);
    CHECK(M::prepareVelocityTarget(r, a, out)); CHECK(out.effectiveNative == 90);

    // Abstract contrasts: a servo with no step geometry, and a non-CANopen float
    // revolutions-per-second rate. Neither is a vendor capability claim.
    M::AxisConfig servo = motorRpmAxis();
    servo.units.encoder.countsPerUnit = scale(4096); servo.units.encoder.sourceId = 5;
    CHECK(servo.units.fullStepsPerMotorTurn.source == M::ScaleSource::UNKNOWN);
    r = rate(servo, 4096, 1, M::PositionUnit::ENCODER_COUNTS);
    CHECK(M::prepareVelocityTarget(r, servo, out)); CHECK(out.effectiveNative == 1);
    M::AxisConfig floatRevolutions = config();
    floatRevolutions.nativeRateBasis = M::NativeRateBasis::MOTOR_TURNS;
    floatRevolutions.velocityRepresentation = M::NativeRepresentation::BINARY32;
    r = rate(floatRevolutions, 6, 1); r.maximumQuantizationError = M::Rational(1, 671088640);
    CHECK(M::prepareVelocityTarget(r, floatRevolutions, out));
    CHECK(floatBits(out.effectiveFloat) == 0x3DCCCCCD && out.rateTime == M::TimeUnit::SECOND);
}

// A-05: wrapped requests consume a chosen basis exactly like relative ones.
void wrappedRequestsHonorBasisPolicy() {
    M::AxisConfig c = config(); c.supportedRelativeBases = 1; // ACTUAL only.
    c.units.commandStepsPerMotorTurn = scale(360);
    c.originKnown = true; c.originSource = M::ScaleSource::QUALIFIED;
    M::AxisReference ref = reference(c, 20); ref.basis = M::RelativeBasis::QUEUED;
    M::PositionRequest r = position(c, 90); r.unit = M::PositionUnit::DEGREES;
    r.frame = M::CoordinateFrame::MOTOR; r.wrapped = true; r.basis = M::RelativeBasis::QUEUED;
    M::PreparedTarget out;
    const Saved<M::PreparedTarget> saved(out);
    M::Status status = M::preparePosition(r, c, &ref, out);
    CHECK(failedWith(status, M::AxisError::UNSUPPORTED_BASIS) && status.code == M::Err::UNSUPPORTED);
    CHECK(saved.unchanged(out));
    M::PositionRequest relative = position(c, 1); relative.relative = true; relative.basis = M::RelativeBasis::QUEUED;
    CHECK(failedWith(M::preparePosition(relative, c, &ref, out), M::AxisError::UNSUPPORTED_BASIS));
    CHECK(saved.unchanged(out));
    r.value = M::Rational(0); r.unit = M::PositionUnit::RADIANS; // Exact zero takes the exact path.
    CHECK(failedWith(M::preparePosition(r, c, &ref, out), M::AxisError::UNSUPPORTED_BASIS));
    r.value = M::Rational(1); r.approximate = true; r.maximumApproximationError = 1e-6;
    r.rounding = M::Rounding::NEAREST; r.maximumQuantizationError = M::Rational(1);
    CHECK(failedWith(M::preparePosition(r, c, &ref, out), M::AxisError::UNSUPPORTED_BASIS));
    CHECK(saved.unchanged(out));
    CHECK(failedWith(M::preparePosition(r, c, nullptr, out), M::AxisError::UNSUPPORTED_BASIS));

    r = position(c, 90); r.unit = M::PositionUnit::DEGREES; r.frame = M::CoordinateFrame::MOTOR; r.wrapped = true;
    ref.basis = M::RelativeBasis::ACTUAL;
    CHECK(M::preparePosition(r, c, &ref, out)); CHECK(out.endpointNative == 90);
    const Saved<M::PreparedTarget> actual(out);
    c.supportedRelativeBases = 6; // Only the other bases.
    CHECK(failedWith(M::preparePosition(r, c, &ref, out), M::AxisError::UNSUPPORTED_BASIS));
    c.supportedRelativeBases = 0;
    CHECK(failedWith(M::preparePosition(r, c, &ref, out), M::AxisError::UNSUPPORTED_BASIS));
    c.supportedRelativeBases = 4; ref.basis = M::RelativeBasis::QUEUED; r.basis = M::RelativeBasis::QUEUED;
    CHECK(M::preparePosition(r, c, &ref, out)); CHECK(out.retainedReference.basis == M::RelativeBasis::QUEUED);
    const M::RelativeBasis others[] = {M::RelativeBasis::ACTUAL, M::RelativeBasis::COMMANDED};
    const Saved<M::PreparedTarget> queued(out);
    for (unsigned i = 0; i < 2; ++i) { // A policy-admitted request cannot use a mismatched witness.
        ref.basis = others[i];
        CHECK(failedWith(M::preparePosition(r, c, &ref, out), M::AxisError::MISSING_REFERENCE));
    }
    CHECK(queued.unchanged(out));

    // The policy governs displacement bases; an unwrapped absolute witness is only checked.
    c.supportedRelativeBases = 1; ref.basis = M::RelativeBasis::QUEUED;
    M::PositionRequest absolute = position(c, 5); absolute.basis = M::RelativeBasis::QUEUED;
    absolute.referencePolicy = M::ReferencePolicy::REQUIRED;
    CHECK(M::preparePosition(absolute, c, &ref, out)); CHECK(out.endpointNative == 5);
}

// DEC-6: 0..1000 soft inside -100..1100 native.
M::AxisConfig softLimited() {
    M::AxisConfig c = config();
    c.nativeMinimum = -100; c.nativeMaximum = 1100;
    c.softLimitsKnown = true; c.softMinimum = 0; c.softMaximum = 1000;
    return c;
}

void softLimitRecovery() {
    M::AxisConfig c = softLimited(); M::PreparedTarget out;
    struct Case { int64_t witness, value, endpoint; bool accepted; };
    const Case relative[] = {
        {1005, -10, 995, true}, {1005, -5, 1000, true}, {1005, -1005, 0, true}, {-5, 10, 5, true}, {-5, 5, 0, true},
        {-5, 1005, 1000, true}, {1100, -100, 1000, true}, {-100, 100, 0, true},
        {1005, -4, 1001, false}, {1005, 0, 1005, false}, {1005, 1, 1006, false}, {1005, -1006, -1, false},
        {-5, 4, -1, false}, {-5, 0, -5, false}, {-5, -1, -6, false}, {-5, 1006, 1001, false},
        {500, 501, 1001, false}, {500, -501, -1, false}, // A witness inside gains nothing.
        {1101, -200, 901, false}, {-101, 200, 99, false}}; // Outside native limits.
    for (unsigned i = 0; i < sizeof relative / sizeof relative[0]; ++i) {
        const Case& t = relative[i];
        const M::AxisReference ref = reference(c, t.witness);
        M::PositionRequest r = position(c, t.value); r.relative = true;
        const Saved<M::PreparedTarget> saved(out);
        const M::Status status = M::preparePosition(r, c, &ref, out);
        if (t.accepted) {
            CHECK(status); CHECK(out.effectiveNative == t.value && out.endpointNative == t.endpoint);
        } else {
            CHECK(failedWith(status, M::AxisError::LIMIT)); CHECK(saved.unchanged(out));
        }
        // An absolute request to the same endpoint reaches the same decision.
        r = position(c, t.endpoint);
        const Saved<M::PreparedTarget> absolute(out);
        const M::Status direct = M::preparePosition(r, c, &ref, out);
        if (t.accepted) {
            CHECK(direct); CHECK(out.effectiveNative == t.endpoint && out.referenceUsed);
        } else {
            CHECK(failedWith(direct, M::AxisError::LIMIT)); CHECK(absolute.unchanged(out));
        }
    }
    CHECK(failedWith(M::preparePosition(position(c, 1001), c, nullptr, out), M::AxisError::LIMIT));
    CHECK(M::preparePosition(position(c, 1000), c, nullptr, out));

    // Without soft limits a witness still has to be inside native limits.
    M::AxisConfig nativeOnly = c; nativeOnly.softLimitsKnown = false;
    M::AxisReference ref = reference(nativeOnly, 1005);
    M::PositionRequest r = position(nativeOnly, 10); r.relative = true;
    CHECK(M::preparePosition(r, nativeOnly, &ref, out)); CHECK(out.endpointNative == 1015);
    ref = reference(nativeOnly, 1101);
    r.value = M::Rational(-10);
    CHECK(failedWith(M::preparePosition(r, nativeOnly, &ref, out), M::AxisError::LIMIT));

    // Unusable references never admit recovery.
    r = position(c, -10); r.relative = true;
    ref = reference(c, 1005); ref.nativeKnown = false;
    const Saved<M::PreparedTarget> saved(out);
    CHECK(failedWith(M::preparePosition(r, c, &ref, out), M::AxisError::MISSING_REFERENCE));
    M::PositionRequest absolute = position(c, 995); absolute.referencePolicy = M::ReferencePolicy::REQUIRED;
    CHECK(failedWith(M::preparePosition(absolute, c, &ref, out), M::AxisError::MISSING_REFERENCE));
    ref = reference(c, 1005); ref.nowUs = 121;
    CHECK(failedWith(M::preparePosition(r, c, &ref, out), M::AxisError::STALE_REFERENCE));
    ref = reference(c, 1005); ref.observedUs = 111;
    CHECK(failedWith(M::preparePosition(r, c, &ref, out), M::AxisError::STALE_REFERENCE));
    ref = reference(c, 1005); ++ref.referenceGeneration;
    CHECK(failedWith(M::preparePosition(r, c, &ref, out), M::AxisError::STALE_GENERATION));
    ref = reference(c, 1005); ++ref.binding.configurationGeneration;
    CHECK(failedWith(M::preparePosition(r, c, &ref, out), M::AxisError::STALE_GENERATION));
    ref = reference(c, 1005); ref.source = M::ScaleSource::UNKNOWN;
    CHECK(failedWith(M::preparePosition(r, c, &ref, out), M::AxisError::STALE_REFERENCE));
    ref = reference(c, 1005); ref.basis = M::RelativeBasis::QUEUED;
    CHECK(failedWith(M::preparePosition(r, c, &ref, out), M::AxisError::MISSING_REFERENCE));
    M::PositionRequest old = r; ++old.configurationGeneration; ref = reference(c, 1005);
    CHECK(failedWith(M::preparePosition(old, c, &ref, out), M::AxisError::STALE_GENERATION));
    CHECK(saved.unchanged(out));

    // A recovered target keeps ordinary admission dependencies.
    CHECK(M::preparePosition(r, c, &ref, out)); CHECK(out.endpointNative == 995);
    CHECK(M::validatePreparedTarget(out, c, &ref, 120));
    CHECK(failedWith(M::validatePreparedTarget(out, c, &ref, 121), M::AxisError::STALE_REFERENCE));
    M::AxisReference moved = ref; moved.nativePosition = 1006;
    CHECK(failedWith(M::validatePreparedTarget(out, c, &moved, 110), M::AxisError::STALE_REFERENCE));
    M::AxisConfig next = c; M::AxisReference idle = ref; idle.idle = idle.stationary = true;
    CHECK(M::configureAxis(next, c, idle, &idle));
    CHECK(failedWith(M::validatePreparedTarget(out, next, &idle, 110), M::AxisError::STALE_GENERATION));
    r.configurationGeneration = next.binding.configurationGeneration; // Retained witness in the new epoch.
    CHECK(M::preparePosition(r, next, &idle, out)); CHECK(out.endpointNative == 995);
}

// Requested and quantized endpoints are each checked, through every arithmetic path.
void softLimitRecoveryEndpoints() {
    M::AxisConfig c = softLimited(); M::PreparedTarget out;
    M::AxisReference ref = reference(c, 1005);
    M::PositionRequest r = position(c, -9, 2); r.relative = true; // Requested 1000.5.
    r.rounding = M::Rounding::FLOOR; r.maximumQuantizationError = M::Rational(1, 2);
    const Saved<M::PreparedTarget> saved(out);
    CHECK(failedWith(M::preparePosition(r, c, &ref, out), M::AxisError::LIMIT)); // Floor reaches 1000.
    CHECK(saved.unchanged(out));
    r.value = M::Rational(-11, 2); r.rounding = M::Rounding::CEIL; // Requested 999.5.
    CHECK(M::preparePosition(r, c, &ref, out));
    CHECK(out.effectiveNative == -5 && out.endpointNative == 1000 && out.requestedNative.integral == 999);
    ref = reference(c, -5);
    r.value = M::Rational(9, 2); r.rounding = M::Rounding::CEIL; // Requested -0.5, ceil reaches 0.
    const Saved<M::PreparedTarget> below(out);
    CHECK(failedWith(M::preparePosition(r, c, &ref, out), M::AxisError::LIMIT)); CHECK(below.unchanged(out));
    r.value = M::Rational(11, 2); r.rounding = M::Rounding::FLOOR; // Requested 0.5, floor reaches 0.
    CHECK(M::preparePosition(r, c, &ref, out)); CHECK(out.endpointNative == 0);

    // Polarity: the same mechanical sign travels inward from one side only.
    c.units.commandPolarity = -1; c.units.commandStepsPerMotorTurn = scale(10);
    c.originKnown = true; c.originSource = M::ScaleSource::QUALIFIED;
    ref = reference(c, 1005);
    r = position(c, 10); r.relative = true; r.unit = M::PositionUnit::STEPS; r.frame = M::CoordinateFrame::MOTOR;
    CHECK(M::preparePosition(r, c, &ref, out)); CHECK(out.effectiveNative == -10 && out.endpointNative == 995);
    r.value = M::Rational(-10);
    const Saved<M::PreparedTarget> outward(out);
    CHECK(failedWith(M::preparePosition(r, c, &ref, out), M::AxisError::LIMIT)); CHECK(outward.unchanged(out));
    r.unit = M::PositionUnit::TURNS; r.value = M::Rational(1);
    CHECK(M::preparePosition(r, c, &ref, out)); CHECK(out.endpointNative == 995);
    r.frame = M::CoordinateFrame::LOAD; c.units.motorTurnsPerLoadTurn = scale(2);
    CHECK(M::preparePosition(r, c, &ref, out)); CHECK(out.endpointNative == 985);
    r.frame = M::CoordinateFrame::MOTOR; r.value = M::Rational(-1);
    CHECK(failedWith(M::preparePosition(r, c, &ref, out), M::AxisError::LIMIT));
    ref = reference(c, -5); r.value = M::Rational(-1);
    CHECK(M::preparePosition(r, c, &ref, out)); CHECK(out.effectiveNative == 10 && out.endpointNative == 5);
    r.value = M::Rational(1);
    CHECK(failedWith(M::preparePosition(r, c, &ref, out), M::AxisError::LIMIT));
    // Absolute mechanical coordinates with negative polarity and an origin.
    c.originNative = 1000; ref = reference(c, 1005);
    M::PositionRequest absolute = position(c, 1); absolute.unit = M::PositionUnit::TURNS; absolute.frame = M::CoordinateFrame::MOTOR;
    CHECK(M::preparePosition(absolute, c, &ref, out)); CHECK(out.endpointNative == 990);
    absolute.value = M::Rational(-1);
    CHECK(failedWith(M::preparePosition(absolute, c, &ref, out), M::AxisError::LIMIT));

    // Binary32: an inside request whose encoded value lands outside still rejects.
    c = config(); c.positionRepresentation = M::NativeRepresentation::BINARY32;
    c.softLimitsKnown = true; c.softMinimum = 0; c.softMaximum = 33554435;
    ref = reference(c, 33554440);
    r = position(c, 33554435); r.maximumQuantizationError = M::Rational(2); // Encodes as 33554436.
    const Saved<M::PreparedTarget> encoded(out);
    CHECK(failedWith(M::preparePosition(r, c, &ref, out), M::AxisError::LIMIT)); CHECK(encoded.unchanged(out));
    r.value = M::Rational(33554432);
    CHECK(M::preparePosition(r, c, &ref, out)); CHECK(out.effectiveFloat == 33554432.0f);
    r = position(c, -8); r.relative = true; r.maximumQuantizationError = M::Rational(0);
    CHECK(M::preparePosition(r, c, &ref, out)); CHECK(out.endpointFloat == 33554432.0);
    r.value = M::Rational(-4);
    CHECK(failedWith(M::preparePosition(r, c, &ref, out), M::AxisError::LIMIT));

    // Approximate radians: the whole interval must be inside.
    c = softLimited(); c.units.commandStepsPerMotorTurn = scale(1000); ref = reference(c, 1005);
    r = position(c, -1); r.unit = M::PositionUnit::RADIANS; r.frame = M::CoordinateFrame::MOTOR; r.relative = true;
    r.approximate = true; r.maximumApproximationError = 1e-8;
    r.rounding = M::Rounding::NEAREST; r.maximumQuantizationError = M::Rational(1);
    CHECK(M::preparePosition(r, c, &ref, out));
    CHECK(out.effectiveNative == -159 && out.endpointNative == 846 && !out.exactArithmetic);
    r.value = M::Rational(1);
    CHECK(failedWith(M::preparePosition(r, c, &ref, out), M::AxisError::LIMIT));
    r.value = M::Rational(-1, 1000); // About -0.16 increments: still outside.
    CHECK(failedWith(M::preparePosition(r, c, &ref, out), M::AxisError::LIMIT));
    r.value = M::Rational(-3, 100); // Requested about 1000.23 would round in to 1000.
    CHECK(failedWith(M::preparePosition(r, c, &ref, out), M::AxisError::LIMIT));
}

void softLimitRecoveryWrapped() {
    M::AxisConfig c = softLimited(); c.units.commandStepsPerMotorTurn = scale(360);
    c.originKnown = true; c.originSource = M::ScaleSource::QUALIFIED;
    M::AxisReference ref = reference(c, 1005); // 285 degrees into its turn.
    M::PositionRequest r = position(c, 270); r.unit = M::PositionUnit::DEGREES;
    r.frame = M::CoordinateFrame::MOTOR; r.wrapped = true;
    M::PreparedTarget out;
    CHECK(M::preparePosition(r, c, &ref, out)); CHECK(out.endpointNative == 990); // Shortest is inward.
    r.value = M::Rational(0); r.path = M::AnglePath::NEGATIVE;
    CHECK(M::preparePosition(r, c, &ref, out)); CHECK(out.endpointNative == 720);
    const Saved<M::PreparedTarget> saved(out);
    r.path = M::AnglePath::POSITIVE;
    CHECK(failedWith(M::preparePosition(r, c, &ref, out), M::AxisError::LIMIT)); // 1080: outward.
    r.path = M::AnglePath::SHORTEST;
    CHECK(failedWith(M::preparePosition(r, c, &ref, out), M::AxisError::LIMIT)); // No other turn is chosen.
    r.value = M::Rational(285);
    CHECK(failedWith(M::preparePosition(r, c, &ref, out), M::AxisError::LIMIT)); // Equal orientation stays outside.
    CHECK(saved.unchanged(out));

    // Negative polarity reverses which physical path is inward.
    c.units.commandPolarity = -1;
    r.value = M::Rational(90); r.path = M::AnglePath::POSITIVE; // Phase 270: 15 increments down.
    CHECK(M::preparePosition(r, c, &ref, out)); CHECK(out.endpointNative == 990);
    r.path = M::AnglePath::NEGATIVE;
    CHECK(failedWith(M::preparePosition(r, c, &ref, out), M::AxisError::LIMIT));
    c.units.commandPolarity = 1;

    // Below the range, and through the approximate path.
    ref = reference(c, -5); r.value = M::Rational(5); r.path = M::AnglePath::SHORTEST;
    CHECK(M::preparePosition(r, c, &ref, out)); CHECK(out.endpointNative == 5);
    r.path = M::AnglePath::NEGATIVE;
    CHECK(failedWith(M::preparePosition(r, c, &ref, out), M::AxisError::LIMIT));
    ref = reference(c, 1005);
    r.path = M::AnglePath::SHORTEST; r.unit = M::PositionUnit::RADIANS; r.approximate = true;
    r.value = M::Rational(INT64_C(4712388980), UINT64_C(1000000000)); // 270 degrees.
    r.maximumApproximationError = 1e-6; r.rounding = M::Rounding::NEAREST; r.maximumQuantizationError = M::Rational(1, 2);
    CHECK(M::preparePosition(r, c, &ref, out)); CHECK(out.endpointNative == 990 && !out.exactArithmetic);
    r.path = M::AnglePath::POSITIVE;
    CHECK(failedWith(M::preparePosition(r, c, &ref, out), M::AxisError::LIMIT));
    ref = reference(c, 1101); r.path = M::AnglePath::NEGATIVE;
    CHECK(failedWith(M::preparePosition(r, c, &ref, out), M::AxisError::LIMIT));
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
    wrappedFloatRoundingReversal();
    signedWideVelocity();
    nativeRateDestination();
    nativeRateQuantization();
    nativeRateBoundaries();
    commonRateIntentVectors();
    wrappedRequestsHonorBasisPolicy();
    softLimitRecovery();
    softLimitRecoveryEndpoints();
    softLimitRecoveryWrapped();
    std::puts("pure axis coordinate/reference contracts passed");
}
