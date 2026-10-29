// SPDX-License-Identifier: MIT
// Adapted from MotorControl-RS Axis.cpp, copyright (c) 2026 Jan Havelka.
// See LICENSE and docs/axis_contract.md for intentional CAN differences.
#include "MotorControlCAN/Axis.h"
#include <cmath>
#include <limits>

namespace MotorControlCAN {
namespace {
Status fail(AxisError e, const char* text, Err code = Err::ILLEGAL_VALUE) {
    return Status(code, static_cast<int32_t>(e), text);
}
Status unitStatus(Status s) {
    if (!s) s.detail += static_cast<int32_t>(AxisError::UNIT_ERROR_BASE);
    return s;
}
uint64_t magnitude(int64_t n) { return n < 0 ? uint64_t(-(n + 1)) + 1 : uint64_t(n); }
uint64_t gcd(uint64_t a, uint64_t b) {
    while (b) { const uint64_t r = a % b; a = b; b = r; }
    return a;
}
bool sourceKnown(ScaleSource s) { return s >= ScaleSource::ASSUMED && s <= ScaleSource::QUALIFIED; }
bool sameScale(const UnitScale& a, const UnitScale& b) {
    return a.numerator == b.numerator && a.denominator == b.denominator && a.source == b.source;
}
bool sameInterpretation(const AxisConfig& a, const AxisConfig& b) {
    const UnitConfig& x = a.units; const UnitConfig& y = b.units;
    return sameScale(x.commandStepsPerMotorTurn, y.commandStepsPerMotorTurn) &&
        sameScale(x.motorTurnsPerLoadTurn, y.motorTurnsPerLoadTurn) &&
        sameScale(x.fullStepsPerMotorTurn, y.fullStepsPerMotorTurn) &&
        sameScale(x.millimetresPerLoadTurn, y.millimetresPerLoadTurn) &&
        sameScale(x.encoder.countsPerUnit, y.encoder.countsPerUnit) &&
        x.encoder.basis == y.encoder.basis && x.encoder.sourceId == y.encoder.sourceId &&
        x.encoder.polarity == y.encoder.polarity && x.commandPolarity == y.commandPolarity &&
        a.positionRepresentation == b.positionRepresentation;
}
Status evidence(const AxisConfig& c, const AxisReference& r, bool stationary, bool native) {
    if (!sameBinding(c.binding, r.binding) || !r.referenceGeneration || c.referenceGeneration != r.referenceGeneration)
        return fail(AxisError::STALE_GENERATION, "reference binding/epoch mismatch", Err::INVALID_CONFIG);
    if (r.basis > RelativeBasis::QUEUED || !sourceKnown(r.source))
        return fail(AxisError::STALE_REFERENCE, "reference provenance absent", Err::INVALID_CONFIG);
    if (r.nowUs > uint64_t(INT64_MAX) || r.observedUs > uint64_t(INT64_MAX) || r.maximumAgeUs > uint64_t(INT64_MAX))
        return fail(AxisError::CLOCK_ERROR, "reference time exceeds monotonic domain", Err::INVALID_CONFIG);
    if (r.nowUs < r.observedUs || (r.maximumAgeUs && r.nowUs - r.observedUs > r.maximumAgeUs))
        return fail(AxisError::STALE_REFERENCE, "reference is future or stale", Err::INVALID_CONFIG);
    if (stationary && (!r.idle || !r.stationary))
        return fail(AxisError::NOT_STATIONARY, "idle stationary evidence required", Err::INVALID_CONFIG);
    if (native && !r.nativeKnown)
        return fail(AxisError::MISSING_REFERENCE, "native coordinate relation unknown", Err::INVALID_CONFIG);
    return Ok();
}
Status increment(AxisConfig& c) {
    if (c.exhausted || !c.referenceGeneration || c.referenceGeneration == UINT32_MAX ||
        !c.binding.configurationGeneration || c.binding.configurationGeneration == UINT32_MAX)
        return fail(AxisError::GENERATION_EXHAUSTED, "axis epochs exhausted", Err::INVALID_CONFIG);
    ++c.referenceGeneration; ++c.binding.configurationGeneration; return Ok();
}
bool inBounds(const NativeQuantity& q, int64_t low, int64_t high) {
    return q.integral >= low && q.integral <= high &&
        !(q.integral == low && q.numerator && q.negative) &&
        !(q.integral == high && q.numerator && !q.negative);
}
Status bounds(const NativeQuantity& q, const AxisConfig& c) {
    if (!inBounds(q, c.nativeMinimum, c.nativeMaximum) ||
        (c.softLimitsKnown && !inBounds(q, c.softMinimum, c.softMaximum)))
        return fail(AxisError::LIMIT, "endpoint outside inclusive limits");
    return Ok();
}
Status add(const NativeQuantity& q, int64_t n, NativeQuantity& out) {
    return unitStatus(addNativeInteger(q, n, out));
}
Status integerBounds(int64_t n, const AxisConfig& c) {
    NativeQuantity q; q.integral = n; return bounds(q, c);
}
Status conversionConfig(CoordinateFrame frame, PositionUnit unit, const AxisConfig& c, UnitConfig& out) {
    if (frame > CoordinateFrame::LOAD || unit > PositionUnit::MILLIMETRES)
        return fail(AxisError::INVALID_ARGUMENT, "invalid coordinate frame/unit");
    if (frame == CoordinateFrame::NATIVE && unit != PositionUnit::STEPS)
        return fail(AxisError::INVALID_ARGUMENT, "native frame requires command increments");
    if (frame == CoordinateFrame::MOTOR && unit == PositionUnit::MILLIMETRES)
        return fail(AxisError::INVALID_ARGUMENT, "linear travel requires load frame");
    out = c.units;
    // Motor angles have no load-gear dependency. Validate the original first.
    if (frame == CoordinateFrame::MOTOR && (unit == PositionUnit::TURNS ||
        unit == PositionUnit::DEGREES || unit == PositionUnit::RADIANS))
        out.motorTurnsPerLoadTurn = UnitScale(1, 1, ScaleSource::DOCUMENTED);
    return Ok();
}
Status displacement(const Rational& value, PositionUnit unit, CoordinateFrame frame,
    const AxisConfig& c, NativeQuantity& out) {
    UnitConfig units; Status s = conversionConfig(frame, unit, c, units); if (!s) return s;
    // Mechanical increments follow command polarity, unlike native increments.
    if (unit == PositionUnit::STEPS && frame != CoordinateFrame::NATIVE && units.commandPolarity < 0) {
        units.commandStepsPerMotorTurn = UnitScale(1, 1, ScaleSource::DOCUMENTED);
        units.motorTurnsPerLoadTurn = UnitScale(1, 1, ScaleSource::DOCUMENTED);
        unit = PositionUnit::TURNS;
    }
    return unitStatus(convertDisplacement(value, unit, PositionUnit::STEPS, units, out));
}
double errorDouble(const IntegerQuantization& q) {
    if (!q.errorNumerator) return 0;
    double bound = double(q.errorNumerator) / double(q.errorDenominator);
    // Four ulps cover the two uint64 conversions and division on binary64.
    for (unsigned i = 0; i < 4; ++i) bound = std::nextafter(bound, std::numeric_limits<double>::infinity());
    return bound;
}
Status quantize(const NativeQuantity& q, NativeRepresentation rep, Rounding rounding,
    const Rational& allowance, int64_t& integer, float& floating, IntegerQuantization& iq, double& error) {
    if (rep == NativeRepresentation::INTEGER) {
        Status s = unitStatus(quantizeInteger(q, rounding, allowance, iq)); if (!s) return s;
        integer = iq.value; error = errorDouble(iq); return Ok();
    }
    FloatQuantization fq; Status s = unitStatus(quantizeFloat(q, allowance, fq)); if (!s) return s;
    floating = fq.value; error = fq.absoluteErrorBound; return Ok();
}
// Compare an exact integer basis plus binary32 without rounding the basis.
// Only fractional sign matters when comparing to integral coordinate limits.
Status floatBounds(float value, int64_t base, const AxisConfig& c) {
    const double v = value;
    if (!std::isfinite(v) || v < -std::ldexp(1.0, 63) || v > std::ldexp(1.0, 63))
        return fail(AxisError::LIMIT, "native float outside coordinate storage");
    // Rounding an I64 displacement may produce +2^63. A negative basis can
    // still yield an in-range absolute endpoint; cancel before signed storage.
    if (v == std::ldexp(1.0, 63)) {
        if (base >= 0) return fail(AxisError::LIMIT, "float endpoint exceeds int64");
        NativeQuantity q; q.integral = INT64_MAX;
        Status s = add(q, base + 1, q); return s ? bounds(q, c) : s;
    }
    const int64_t integral = static_cast<int64_t>(std::trunc(v));
    NativeQuantity q; q.integral = integral;
    Status s = add(q, base, q); if (!s) return s;
    const double part = v - double(integral);
    if (part < 0 && q.integral > 0) {
        --q.integral; q.numerator = 1; q.denominator = 2;
    } else if (part > 0 && q.integral < 0) {
        ++q.integral; q.numerator = 1; q.denominator = 2; q.negative = true;
    } else if (part != 0) {
        q.numerator = 1; q.denominator = 2; q.negative = part < 0;
    }
    return bounds(q, c);
}
int compareFloatInteger(float value, int64_t n) {
    const double v = value;
    if (v >= std::ldexp(1.0, 63)) return 1;
    if (v < -std::ldexp(1.0, 63)) return -1;
    const int64_t integral = static_cast<int64_t>(std::trunc(v));
    if (integral != n) return integral < n ? -1 : 1;
    return v < double(integral) ? -1 : (v > double(integral) ? 1 : 0);
}
uint64_t modularAdd(uint64_t a, uint64_t b, uint64_t m) {
    return a >= m - b ? a - (m - b) : a + b;
}
uint64_t modularMultiply(uint64_t a, uint64_t b, uint64_t m) {
    uint64_t result = 0; a %= m;
    while (b) {
        if (b & 1u) result = modularAdd(result, a, m);
        b >>= 1; if (b) a = modularAdd(a, a, m);
    }
    return result;
}
uint64_t nativePhase(int64_t n, uint64_t denominator, uint64_t modulus) {
    const uint64_t phase = modularMultiply(magnitude(n), denominator, modulus);
    return n < 0 && phase ? modulus - phase : phase;
}
Status period(const PositionRequest& r, const AxisConfig& c, NativeQuantity& output) {
    AxisConfig positive = c; positive.units.commandPolarity = 1;
    return displacement(Rational(1), PositionUnit::TURNS, r.frame, positive, output);
}
Status wrapped(const PositionRequest& r, const AxisConfig& c, const AxisReference& ref,
    NativeQuantity& endpoint, bool& nativePositive) {
    Rational orientation = r.value;
    const uint64_t turn = r.unit == PositionUnit::DEGREES ? 360 : 1;
    if (r.unit != PositionUnit::RADIANS && orientation.denominator <= UINT64_MAX / turn) {
        const uint64_t rem = magnitude(orientation.numerator) % (orientation.denominator * turn);
        orientation.numerator = r.value.numerator < 0 ?
            (rem == uint64_t(INT64_MAX) + 1u ? INT64_MIN : -int64_t(rem)) : int64_t(rem);
    }
    NativeQuantity angle, p;
    Status s = displacement(orientation, r.unit, r.frame, c, angle); if (!s) return s;
    s = period(r, c, p); if (!s) return s;
    const uint64_t multiplier = p.denominator / gcd(angle.denominator, p.denominator);
    if (angle.denominator > UINT64_MAX / multiplier)
        return fail(AxisError::ARITHMETIC_OVERFLOW, "wrapped denominator exceeds storage");
    const uint64_t denominator = angle.denominator * multiplier;
    if (uint64_t(p.integral) > (UINT64_MAX - p.numerator) / p.denominator)
        return fail(AxisError::ARITHMETIC_OVERFLOW, "wrapped turn exceeds storage");
    const uint64_t pn = uint64_t(p.integral) * p.denominator + p.numerator;
    const uint64_t pm = denominator / p.denominator;
    if (!pn || pn > UINT64_MAX / pm) return fail(AxisError::ARITHMETIC_OVERFLOW, "wrapped modulus exceeds storage");
    const uint64_t modulus = pn * pm;
    uint64_t phase = nativePhase(angle.integral, denominator, modulus);
    const uint64_t fraction = modularMultiply(angle.numerator, denominator / angle.denominator, modulus);
    phase = modularAdd(phase, angle.negative && fraction ? modulus - fraction : fraction, modulus);
    phase = modularAdd(phase, nativePhase(c.originNative, denominator, modulus), modulus);
    const uint64_t current = nativePhase(ref.nativePosition, denominator, modulus);
    const uint64_t positive = phase >= current ? phase - current : modulus - (current - phase);
    const uint64_t negative = positive ? modulus - positive : 0;
    nativePositive = c.units.commandPolarity > 0;
    if (r.path == AnglePath::NEGATIVE) nativePositive = !nativePositive;
    if (r.path == AnglePath::SHORTEST) {
        if (positive == negative && positive) {
            if (r.tie == HalfTurnTie::REJECT) return fail(AxisError::AMBIGUOUS, "half-turn direction ambiguous");
            nativePositive = (r.tie == HalfTurnTie::POSITIVE) == (c.units.commandPolarity > 0);
        } else nativePositive = positive < negative;
    }
    const uint64_t distance = nativePositive ? positive : negative, integral = distance / denominator;
    if (integral > uint64_t(INT64_MAX) + (nativePositive ? 0u : 1u))
        return fail(AxisError::ARITHMETIC_OVERFLOW, "wrapped distance exceeds signed storage");
    NativeQuantity delta;
    delta.integral = nativePositive ? int64_t(integral) :
        (integral == uint64_t(INT64_MAX) + 1u ? INT64_MIN : -int64_t(integral));
    delta.numerator = distance % denominator; delta.denominator = denominator;
    delta.negative = !nativePositive && delta.numerator;
    return add(delta, ref.nativePosition, endpoint);
}
// Approximate decisions use outward intervals within binary64's exact-integer
// range. A midpoint never substitutes for the retained exact request.
struct Interval { double value = 0, error = 0; };
Status offsetInterval(Interval& q, int64_t base) {
    if (base < -INT64_C(9007199254740992) || base > INT64_C(9007199254740992) ||
        std::fabs(q.value) > 9007199254740992.0 || std::fabs(q.value + double(base)) > 9007199254740992.0)
        return fail(AxisError::ARITHMETIC_OVERFLOW, "approximate coordinate exceeds binary64 integer precision");
    q.value += double(base);
    q.error = std::nextafter(q.error + std::fabs(q.value) * (2 * std::numeric_limits<double>::epsilon()),
        std::numeric_limits<double>::infinity());
    return Ok();
}
void intervalEnds(const Interval& q, double& lo, double& hi) {
    lo = std::nextafter(q.value - q.error, -std::numeric_limits<double>::infinity());
    hi = std::nextafter(q.value + q.error, std::numeric_limits<double>::infinity());
}
Status intervalBounds(const Interval& q, const AxisConfig& c) {
    double lo, hi; intervalEnds(q, lo, hi);
    // More distant limits cannot constrain an interval in this domain.
    const int64_t lim = INT64_C(9007199254740992);
    if ((c.nativeMinimum >= -lim && lo < double(c.nativeMinimum)) ||
        (c.nativeMaximum <= lim && hi > double(c.nativeMaximum)) ||
        (c.softLimitsKnown && ((c.softMinimum >= -lim && lo < double(c.softMinimum)) ||
                              (c.softMaximum <= lim && hi > double(c.softMaximum)))))
        return fail(AxisError::LIMIT, "approximation interval crosses inclusive limit");
    return Ok();
}
double rounded(double x, Rounding r) {
    if (r == Rounding::FLOOR) return std::floor(x);
    if (r == Rounding::CEIL) return std::ceil(x);
    if (r == Rounding::TOWARD_ZERO) return std::trunc(x);
    const double low = std::floor(x), fraction = x - low;
    return fraction < .5 ? low : (fraction > .5 ? low + 1 :
        (std::fmod(std::fabs(low), 2.0) == 0 ? low : low + 1));
}
Status quantizeInterval(const Interval& q, NativeRepresentation rep, Rounding rounding,
    const Rational& allowance, int64_t& integer, float& floating, double& error) {
    double allowed; Status s = unitStatus(rationalToDouble(allowance, allowed)); if (!s) return s;
    double lo, hi; intervalEnds(q, lo, hi);
    if (std::fabs(lo) > 9007199254740992.0 || std::fabs(hi) > 9007199254740992.0)
        return fail(AxisError::ARITHMETIC_OVERFLOW, "approximate interval exceeds exact integer range");
    double effective;
    if (rep == NativeRepresentation::INTEGER) {
        if (rounding == Rounding::EXACT) return fail(AxisError::AMBIGUOUS, "nonzero radians cannot prove an exact integer");
        const double a = rounded(lo, rounding), b = rounded(hi, rounding);
        if (a != b) return fail(AxisError::AMBIGUOUS, "interval crosses integer rounding boundary");
        integer = static_cast<int64_t>(a); effective = a;
    } else {
        const float a = static_cast<float>(lo), b = static_cast<float>(hi);
        if (a != b) return fail(AxisError::AMBIGUOUS, "interval crosses binary32 rounding boundary");
        floating = a; effective = a;
    }
    error = std::nextafter(std::fmax(std::fabs(effective - lo), std::fabs(effective - hi)),
        std::numeric_limits<double>::infinity());
    if (error > allowed) return fail(AxisError::AMBIGUOUS, "total quantization error exceeds allowance");
    return Ok();
}
Status approximatePosition(const PositionRequest& r, const AxisConfig& c, const AxisReference* ref,
    int64_t origin, PreparedTarget& next) {
    if (!r.approximate || !std::isfinite(r.maximumApproximationError) || r.maximumApproximationError <= 0)
        return fail(AxisError::INVALID_ARGUMENT, "radians require explicit finite allowance");
    UnitConfig units; Status s = conversionConfig(r.frame, r.unit, c, units); if (!s) return s;
    UnitConversion v;
    s = unitStatus(convertDisplacement(r.value, r.unit, PositionUnit::STEPS, units, r.maximumApproximationError, v));
    if (!s) return s;
    Interval q; q.value = v.value; q.error = v.absoluteErrorBound;
    bool nativePositive = true;
    if (r.wrapped) {
        NativeQuantity exactPeriod; s = period(r, c, exactPeriod); if (!s) return s;
        const double p = double(exactPeriod.integral) + double(exactPeriod.numerator) / double(exactPeriod.denominator);
        if (!std::isfinite(p) || p <= 0 || p > 9007199254740992.0)
            return fail(AxisError::ARITHMETIC_OVERFLOW, "approximate period exceeds precision");
        s = offsetInterval(q, origin); if (!s) return s;
        if (ref->nativePosition < -INT64_C(9007199254740992) || ref->nativePosition > INT64_C(9007199254740992))
            return fail(AxisError::ARITHMETIC_OVERFLOW, "wrapped reference exceeds precision");
        s = offsetInterval(q, -ref->nativePosition); if (!s) return s;
        const double turns = std::fabs(q.value / p);
        q.error = std::nextafter(q.error + (std::fabs(q.value) + p * (turns + 2)) *
            (16 * std::numeric_limits<double>::epsilon()), std::numeric_limits<double>::infinity());
        double positive = std::fmod(q.value, p); if (positive < 0) positive += p;
        if (positive <= q.error || p - positive <= q.error)
            return fail(AxisError::AMBIGUOUS, "radian interval crosses orientation wrap");
        nativePositive = c.units.commandPolarity > 0;
        if (r.path == AnglePath::NEGATIVE) nativePositive = !nativePositive;
        if (r.path == AnglePath::SHORTEST) {
            if (std::fabs(positive - p / 2) <= q.error)
                return fail(AxisError::AMBIGUOUS, "radian interval crosses half-turn boundary");
            nativePositive = positive < p / 2;
        }
        q.value = nativePositive ? positive : positive - p;
        s = offsetInterval(q, ref->nativePosition); if (!s) return s;
    } else if (!r.relative) {
        s = offsetInterval(q, origin); if (!s) return s;
    }
    Interval endpoint = q;
    if (r.relative) { s = offsetInterval(endpoint, ref->nativePosition); if (!s) return s; }
    if (endpoint.error > r.maximumApproximationError)
        return fail(AxisError::AMBIGUOUS, "endpoint approximation exceeds allowance");
    s = intervalBounds(endpoint, c); if (!s) return s;
    s = quantizeInterval(q, c.positionRepresentation, r.rounding, r.maximumQuantizationError,
        next.effectiveNative, next.effectiveFloat, next.absoluteErrorBound); if (!s) return s;
    if (c.positionRepresentation == NativeRepresentation::INTEGER) {
        NativeQuantity n; n.integral = next.effectiveNative;
        if (r.relative) { s = add(n, ref->nativePosition, n); if (!s) return s; }
        next.endpointNative = n.integral;
        s = bounds(n, c); if (!s) return s;
        if (r.wrapped && ((next.endpointNative > ref->nativePosition && !nativePositive) ||
            (next.endpointNative < ref->nativePosition && nativePositive)))
            return fail(AxisError::AMBIGUOUS, "rounding reverses wrapped path");
    } else {
        s = floatBounds(next.effectiveFloat, r.relative ? ref->nativePosition : 0, c); if (!s) return s;
        next.endpointFloat = double(next.effectiveFloat) + (r.relative ? double(ref->nativePosition) : 0);
        if (r.wrapped && ((compareFloatInteger(next.effectiveFloat, ref->nativePosition) > 0 && !nativePositive) ||
            (compareFloatInteger(next.effectiveFloat, ref->nativePosition) < 0 && nativePositive)))
            return fail(AxisError::AMBIGUOUS, "float rounding reverses wrapped path");
    }
    next.exactArithmetic = false; next.approximateRequestedNative = endpoint.value;
    next.approximationErrorBound = endpoint.error; return Ok();
}
} // namespace

Status validateAxisConfig(const AxisConfig& c) {
    Status s = validateBinding(c.binding); if (!s) return fail(AxisError::INVALID_TARGET, "invalid common axis binding", Err::INVALID_CONFIG);
    if (c.exhausted || !c.referenceGeneration)
        return fail(AxisError::GENERATION_EXHAUSTED, "axis confidence epoch unusable", Err::INVALID_CONFIG);
    s = unitStatus(validateUnitConfig(c.units)); if (!s) return s;
    if (c.nativeMinimum > c.nativeMaximum || (c.supportedRelativeBases & ~7u) ||
        c.originSource > ScaleSource::QUALIFIED || c.encoderOriginSource > ScaleSource::QUALIFIED ||
        c.positionRepresentation > NativeRepresentation::BINARY32 || c.velocityRepresentation > NativeRepresentation::BINARY32 ||
        c.nativeVelocityTime > TimeUnit::MILLISECOND ||
        (c.originKnown && (!sourceKnown(c.originSource) || c.originNative < c.nativeMinimum || c.originNative > c.nativeMaximum)) ||
        (c.encoderOriginKnown && (!sourceKnown(c.encoderOriginSource) || !c.units.encoder.sourceId ||
            c.units.encoder.countsPerUnit.source == ScaleSource::UNKNOWN || c.encoderOriginNative < c.nativeMinimum || c.encoderOriginNative > c.nativeMaximum)) ||
        (c.softLimitsKnown && (c.softMinimum > c.softMaximum || c.softMinimum < c.nativeMinimum || c.softMaximum > c.nativeMaximum)))
        return fail(AxisError::INVALID_ARGUMENT, "invalid axis interpretation/limits", Err::INVALID_CONFIG);
    return Ok();
}
Status configureAxis(AxisConfig& current, const AxisConfig& candidate, const AxisReference& e,
    AxisReference* retainedReference) {
    Status s = validateAxisConfig(current); if (!s) return s;
    s = validateAxisConfig(candidate); if (!s) return s;
    s = evidence(current, e, true, false); if (!s) return s;
    if (!sameBinding(current.binding, candidate.binding) || current.referenceGeneration != candidate.referenceGeneration)
        return fail(AxisError::STALE_GENERATION, "candidate changes binding/epochs directly", Err::INVALID_CONFIG);
    if (candidate.originKnown != current.originKnown || candidate.originNative != current.originNative || candidate.originSource != current.originSource ||
        candidate.encoderOriginKnown != current.encoderOriginKnown || candidate.encoderOriginNative != current.encoderOriginNative || candidate.encoderOriginSource != current.encoderOriginSource)
        return fail(AxisError::MISSING_REFERENCE, "origins require explicit checked operation", Err::INVALID_CONFIG);
    if (candidate.softLimitsKnown && (!current.softLimitsKnown || candidate.softMinimum != current.softMinimum || candidate.softMaximum != current.softMaximum)) {
        s = evidence(current, e, true, true); if (!s) return s;
    }
    AxisConfig next = candidate;
    const bool unchanged = sameInterpretation(current, candidate);
    if (!unchanged) {
        next.originKnown = next.encoderOriginKnown = next.softLimitsKnown = false;
        next.originSource = next.encoderOriginSource = ScaleSource::UNKNOWN;
    }
    s = increment(next); if (!s) return s;
    AxisReference reference;
    if (unchanged) { reference = e; reference.binding = next.binding; reference.referenceGeneration = next.referenceGeneration; }
    current = next;
    if (retainedReference) *retainedReference = reference;
    return Ok();
}
Status setAxisOrigin(AxisConfig& c, int64_t origin, const AxisReference& e, AxisOrigin which) {
    Status s = validateAxisConfig(c); if (!s) return s;
    s = evidence(c, e, true, true); if (!s) return s;
    if (which > AxisOrigin::ENCODER) return fail(AxisError::INVALID_ARGUMENT, "unknown origin selector");
    if (which == AxisOrigin::ENCODER && (c.units.encoder.countsPerUnit.source == ScaleSource::UNKNOWN || !c.units.encoder.sourceId))
        return fail(AxisError::MISSING_ORIGIN, "encoder origin needs selected encoder scale", Err::INVALID_CONFIG);
    if (origin < c.nativeMinimum || origin > c.nativeMaximum) return fail(AxisError::LIMIT, "origin outside native limits");
    AxisConfig next = c; s = increment(next); if (!s) return s;
    if (which == AxisOrigin::MECHANICAL) { next.originKnown = true; next.originNative = origin; next.originSource = e.source; }
    else { next.encoderOriginKnown = true; next.encoderOriginNative = origin; next.encoderOriginSource = e.source; }
    next.softLimitsKnown = false; c = next; return Ok();
}
Status invalidateAxisReference(AxisConfig& c, AxisReference& ref, ReferenceLoss loss) {
    if (loss > ReferenceLoss::COORDINATES) return fail(AxisError::INVALID_ARGUMENT, "unknown reference loss");
    AxisConfig next = c;
    if (loss == ReferenceLoss::COORDINATES) {
        next.originKnown = next.encoderOriginKnown = next.softLimitsKnown = false;
        next.originSource = next.encoderOriginSource = ScaleSource::UNKNOWN;
    }
    const bool exhausted = next.exhausted || !next.referenceGeneration || next.referenceGeneration == UINT32_MAX ||
        (loss == ReferenceLoss::COORDINATES && (!next.binding.configurationGeneration || next.binding.configurationGeneration == UINT32_MAX));
    if (exhausted) next.exhausted = true;
    else {
        ++next.referenceGeneration;
        if (loss == ReferenceLoss::COORDINATES) ++next.binding.configurationGeneration;
    }
    c = next; ref = AxisReference();
    return exhausted ? fail(AxisError::GENERATION_EXHAUSTED, "confidence revoked; retire binding", Err::INVALID_CONFIG) : Ok();
}
Status preparePosition(const PositionRequest& r, const AxisConfig& c, const AxisReference* ref, PreparedTarget& output) {
    Status s = validateAxisConfig(c); if (!s) return s;
    if (!r.configurationGeneration || r.configurationGeneration != c.binding.configurationGeneration)
        return fail(AxisError::STALE_GENERATION, "position configuration epoch differs", Err::INVALID_CONFIG);
    if (!r.value.denominator || r.frame > CoordinateFrame::LOAD || r.unit > PositionUnit::MILLIMETRES ||
        r.basis > RelativeBasis::QUEUED || r.referencePolicy > ReferencePolicy::REQUIRED ||
        r.rounding > Rounding::CEIL || r.path > AnglePath::SHORTEST || r.tie > HalfTurnTie::NEGATIVE ||
        !r.maximumQuantizationError.denominator || r.maximumQuantizationError.numerator < 0 ||
        !std::isfinite(r.maximumApproximationError) || r.maximumApproximationError < 0 ||
        (r.approximate && r.unit != PositionUnit::RADIANS))
        return fail(AxisError::INVALID_ARGUMENT, "invalid position intent");
    if (r.wrapped && (r.relative || r.frame == CoordinateFrame::NATIVE ||
        (r.unit != PositionUnit::TURNS && r.unit != PositionUnit::DEGREES && r.unit != PositionUnit::RADIANS)))
        return fail(AxisError::INVALID_ARGUMENT, "wrapped orientation requires absolute motor/load angle");
    if (r.relative && !(c.supportedRelativeBases & (1u << unsigned(r.basis))))
        return fail(AxisError::UNSUPPORTED_BASIS, "relative basis unavailable", Err::UNSUPPORTED);
    if (!ref && (r.relative || r.wrapped || r.referencePolicy == ReferencePolicy::REQUIRED))
        return fail(AxisError::MISSING_REFERENCE, "selected native-position witness required", Err::INVALID_CONFIG);
    if (ref) {
        s = evidence(c, *ref, false, true); if (!s) return s;
        if (ref->basis != r.basis) return fail(AxisError::MISSING_REFERENCE, "reference basis differs", Err::INVALID_CONFIG);
        s = integerBounds(ref->nativePosition, c); if (!s) return s;
    }
    int64_t origin = 0;
    if (!r.relative && r.frame != CoordinateFrame::NATIVE) {
        if (r.unit == PositionUnit::ENCODER_COUNTS) {
            if (!c.encoderOriginKnown) return fail(AxisError::MISSING_ORIGIN, "encoder zero relation unavailable", Err::INVALID_CONFIG);
            origin = c.encoderOriginNative;
        } else {
            if (!c.originKnown) return fail(AxisError::MISSING_ORIGIN, "mechanical host zero unavailable", Err::INVALID_CONFIG);
            origin = c.originNative;
        }
    }
    PreparedTarget next; next.requested = r; next.binding = c.binding;
    next.referenceGeneration = c.referenceGeneration; next.representation = c.positionRepresentation;
    if (ref) { next.referenceUsed = true; next.retainedReference = *ref; }
    if (r.unit == PositionUnit::RADIANS && r.value.numerator) {
        s = approximatePosition(r, c, ref, origin, next); if (!s) return s;
        output = next; return Ok();
    }
    NativeQuantity quantity; bool nativePositive = true;
    if (r.wrapped) { s = wrapped(r, c, *ref, quantity, nativePositive); if (!s) return s; }
    else {
        s = displacement(r.value, r.unit, r.frame, c, quantity); if (!s) return s;
        if (!r.relative) { s = add(quantity, origin, quantity); if (!s) return s; }
    }
    NativeQuantity endpoint = quantity;
    if (r.relative) { s = add(quantity, ref->nativePosition, endpoint); if (!s) return s; }
    s = bounds(endpoint, c); if (!s) return s;
    next.requestedNative = endpoint;
    s = quantize(quantity, c.positionRepresentation, r.rounding, r.maximumQuantizationError,
        next.effectiveNative, next.effectiveFloat, next.quantization, next.absoluteErrorBound); if (!s) return s;
    if (c.positionRepresentation == NativeRepresentation::INTEGER) {
        NativeQuantity effective; effective.integral = next.effectiveNative;
        if (r.relative) { s = add(effective, ref->nativePosition, effective); if (!s) return s; }
        next.endpointNative = effective.integral;
        s = bounds(effective, c); if (!s) return s;
        if (r.wrapped && ((next.endpointNative > ref->nativePosition && !nativePositive) ||
            (next.endpointNative < ref->nativePosition && nativePositive)))
            return fail(AxisError::AMBIGUOUS, "rounding reverses wrapped direction");
    } else {
        s = floatBounds(next.effectiveFloat, r.relative ? ref->nativePosition : 0, c); if (!s) return s;
        next.endpointFloat = double(next.effectiveFloat) + (r.relative ? double(ref->nativePosition) : 0);
        if (r.wrapped && ((compareFloatInteger(next.effectiveFloat, ref->nativePosition) > 0 && !nativePositive) ||
            (compareFloatInteger(next.effectiveFloat, ref->nativePosition) < 0 && nativePositive)))
            return fail(AxisError::AMBIGUOUS, "float rounding reverses wrapped direction");
    }
    output = next; return Ok();
}
Status validatePreparedTarget(const PreparedTarget& p, const AxisConfig& c, const AxisReference* ref, uint64_t nowUs) {
    Status s = validateAxisConfig(c); if (!s) return s;
    if (!sameBinding(p.binding, c.binding) || p.representation != c.positionRepresentation)
        return fail(AxisError::STALE_GENERATION, "prepared interpretation stale", Err::INVALID_CONFIG);
    if (nowUs > uint64_t(INT64_MAX)) return fail(AxisError::CLOCK_ERROR, "admission time exceeds domain", Err::INVALID_CONFIG);
    if (!p.referenceUsed) return Ok();
    if (!ref || p.referenceGeneration != c.referenceGeneration)
        return fail(AxisError::STALE_GENERATION, "prepared reference epoch stale", Err::INVALID_CONFIG);
    const AxisReference& old = p.retainedReference;
    if (nowUs < old.nowUs || nowUs < ref->nowUs)
        return fail(AxisError::CLOCK_ERROR, "admission time moved backwards", Err::INVALID_CONFIG);
    if (ref->observedUs != old.observedUs || ref->nativePosition != old.nativePosition ||
        ref->basis != old.basis || ref->source != old.source || ref->maximumAgeUs != old.maximumAgeUs ||
        ref->nativeKnown != old.nativeKnown || ref->referenceGeneration != old.referenceGeneration ||
        !sameBinding(ref->binding, old.binding))
        return fail(AxisError::STALE_REFERENCE, "prepared witness changed; prepare again", Err::INVALID_CONFIG);
    AxisReference atAdmission = *ref; atAdmission.nowUs = nowUs;
    return evidence(c, atAdmission, false, true);
}
Status prepareVelocityTarget(const VelocityRequest& r, const AxisConfig& c, PreparedVelocityTarget& output) {
    Status s = validateAxisConfig(c); if (!s) return s;
    if (!r.configurationGeneration || r.configurationGeneration != c.binding.configurationGeneration)
        return fail(AxisError::STALE_GENERATION, "velocity configuration epoch differs", Err::INVALID_CONFIG);
    if (!r.value.denominator || r.unit.position > PositionUnit::MILLIMETRES || r.unit.time > TimeUnit::MILLISECOND ||
        r.rounding > Rounding::CEIL || !r.maximumQuantizationError.denominator || r.maximumQuantizationError.numerator < 0 ||
        !std::isfinite(r.maximumApproximationError) || r.maximumApproximationError < 0 ||
        (r.approximate && r.unit.position != PositionUnit::RADIANS))
        return fail(AxisError::INVALID_ARGUMENT, "invalid velocity intent");
    UnitConfig units; s = conversionConfig(r.frame, r.unit.position, c, units); if (!s) return s;
    if (r.frame == CoordinateFrame::NATIVE && r.unit.time != c.nativeVelocityTime)
        return fail(AxisError::INVALID_ARGUMENT, "native rate time dimension differs");
    PositionUnit from = r.unit.position;
    if (from == PositionUnit::STEPS && r.frame != CoordinateFrame::NATIVE && units.commandPolarity < 0) {
        units.commandStepsPerMotorTurn = UnitScale(1, 1, ScaleSource::DOCUMENTED);
        units.motorTurnsPerLoadTurn = UnitScale(1, 1, ScaleSource::DOCUMENTED); from = PositionUnit::TURNS;
    }
    PreparedVelocityTarget next; next.requested = r; next.binding = c.binding; next.representation = c.velocityRepresentation;
    const VelocityUnit destination(PositionUnit::STEPS, c.nativeVelocityTime);
    if (r.unit.position == PositionUnit::RADIANS && r.value.numerator) {
        if (!r.approximate || r.maximumApproximationError <= 0)
            return fail(AxisError::INVALID_ARGUMENT, "radian rate needs explicit allowance");
        UnitConversion v;
        s = unitStatus(convertVelocity(r.value, r.unit, destination, units, r.maximumApproximationError, v)); if (!s) return s;
        Interval q; q.value = v.value; q.error = v.absoluteErrorBound;
        s = quantizeInterval(q, c.velocityRepresentation, r.rounding, r.maximumQuantizationError,
            next.effectiveNative, next.effectiveFloat, next.absoluteErrorBound); if (!s) return s;
        next.exactArithmetic = false; next.approximateRequestedNative = q.value;
    } else {
        s = unitStatus(convertVelocity(r.value, VelocityUnit(from, r.unit.time), destination, units, next.requestedNative)); if (!s) return s;
        s = quantize(next.requestedNative, c.velocityRepresentation, r.rounding, r.maximumQuantizationError,
            next.effectiveNative, next.effectiveFloat, next.quantization, next.absoluteErrorBound); if (!s) return s;
    }
    output = next; return Ok();
}
} // namespace MotorControlCAN
