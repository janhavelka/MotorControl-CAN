// SPDX-License-Identifier: MIT
// Adapted from MotorControl-RS Units.cpp, UnitFactors.h and Axis.cpp
// Copyright (c) 2026 Jan Havelka. See LICENSE and docs/units_status.md.
#include "MotorControlCAN/Units.h"
#include <cmath>
#include <cstring>
#include <limits>

namespace MotorControlCAN {
namespace {
static_assert(std::numeric_limits<double>::is_iec559 && std::numeric_limits<double>::digits == 53,
              "Exact floating comparisons require IEEE binary64");
static_assert(std::numeric_limits<float>::is_iec559 && std::numeric_limits<float>::digits == 24 &&
              sizeof(float) == sizeof(uint32_t), "Native float quantization requires IEEE binary32");
Status fail(UnitError reason, const char* text, Err code = Err::ILLEGAL_VALUE) {
    return Status(code, static_cast<int32_t>(reason), text);
}
uint64_t magnitude(int64_t n) { return n < 0 ? uint64_t(-(n + 1)) + 1 : uint64_t(n); }
uint64_t gcd(uint64_t a, uint64_t b) {
    while (b) { const uint64_t r = a % b; a = b; b = r; }
    return a;
}
bool signedMagnitude(uint64_t n, bool negative, int64_t& out) {
    const uint64_t limit = uint64_t(INT64_MAX) + (negative ? 1u : 0u);
    if (n > limit) return false;
    out = negative ? (n == uint64_t(INT64_MAX) + 1u ? INT64_MIN : -int64_t(n)) : int64_t(n);
    return true;
}
Status fraction(uint64_t n, uint64_t d, bool negative, Rational& out) {
    if (!d) return fail(UnitError::INVALID_ARGUMENT, "zero denominator");
    const uint64_t common = gcd(n, d);
    n /= common; d /= common;
    int64_t signedValue;
    if (!signedMagnitude(n, negative, signedValue))
        return fail(UnitError::ARITHMETIC_OVERFLOW, "reduced numerator exceeds int64");
    out = Rational(signedValue, d);
    return Ok();
}
// Exact unsigned ratio vs nonnegative binary64. Quotient/fraction expansion
// avoids rounding uint64 operands and never doubles a remainder unsafely.
int compareRatio(uint64_t n, uint64_t d, double value) {
    if (value >= 18446744073709551616.0) return -1;
    const uint64_t integer = static_cast<uint64_t>(std::floor(value));
    const uint64_t quotient = n / d;
    if (quotient != integer) return quotient < integer ? -1 : 1;
    uint64_t remainder = n % d;
    const double part = value - static_cast<double>(integer);
    if (!remainder) return part == 0 ? 0 : -1;
    if (part == 0 || part < std::ldexp(1.0, -64)) return 1;
    int exponent = 0;
    const uint64_t mantissa = static_cast<uint64_t>(std::ldexp(std::frexp(part, &exponent), 53));
    const int last = 53 - exponent;
    for (int i = 1; i <= last; ++i) {
        const bool ratioBit = remainder >= d - remainder;
        remainder = ratioBit ? remainder - (d - remainder) : remainder + remainder;
        const int bit = last - i;
        const bool floatBit = bit < 53 && ((mantissa >> bit) & 1u);
        if (ratioBit != floatBit) return ratioBit ? 1 : -1;
    }
    return remainder ? 1 : 0;
}
// Quantization allowance proofs need wider products. Binary32 has denominator <=2^87,
// error denominator <=151 bits, tolerance comparison <=215 bits. The 256-bit
// stack value also accommodates the binary64 error-bound dyadic comparisons.
// This is private unsigned storage, not a public arbitrary precision facility.
struct Wide {
    uint32_t words[8] = {};
    explicit Wide(uint64_t n = 0) { words[0] = uint32_t(n); words[1] = uint32_t(n >> 32); }
    int compare(const Wide& b) const {
        for (unsigned i = 8; i-- > 0; )
            if (words[i] != b.words[i]) return words[i] < b.words[i] ? -1 : 1;
        return 0;
    }
    bool add(const Wide& b) {
        uint64_t carry = 0;
        for (unsigned i = 0; i < 8; ++i) {
            const uint64_t sum = uint64_t(words[i]) + b.words[i] + carry;
            words[i] = uint32_t(sum); carry = sum >> 32;
        }
        return carry == 0;
    }
    void subtract(const Wide& b) { // Caller establishes *this >= b.
        uint64_t borrow = 0;
        for (unsigned i = 0; i < 8; ++i) {
            const uint64_t rhs = uint64_t(b.words[i]) + borrow;
            borrow = uint64_t(words[i]) < rhs;
            words[i] = uint32_t(uint64_t(words[i]) - rhs);
        }
    }
    bool shift(unsigned count) {
        for (unsigned bit = 0; bit < count; ++bit) {
            if (words[7] & 0x80000000u) return false;
            for (unsigned i = 7; i > 0; --i) words[i] = (words[i] << 1) | (words[i-1] >> 31);
            words[0] <<= 1;
        }
        return true;
    }
    bool multiply(uint64_t factor) {
        Wide sum, term = *this;
        while (factor) {
            if ((factor & 1u) && !sum.add(term)) return false;
            factor >>= 1;
            if (factor && !term.shift(1)) return false;
        }
        *this = sum;
        return true;
    }
    double approximate() const {
        double result = 0;
        for (unsigned i = 8; i-- > 0; ) result = std::ldexp(result, 32) + words[i];
        return result;
    }
};
bool compareWideDouble(const Wide& n, const Wide& d, double value, int& comparison) {
    if (value == 0) { comparison = n.compare(Wide()); return true; }
    int exponent = 0;
    uint64_t mantissa = static_cast<uint64_t>(std::ldexp(std::frexp(value, &exponent), 53));
    exponent -= 53;
    while (!(mantissa & 1u)) { mantissa >>= 1; ++exponent; }
    Wide left = n, right = d;
    if (!right.multiply(mantissa)) return false;
    if (exponent < 0 ? !left.shift(unsigned(-exponent)) : !right.shift(unsigned(exponent))) return false;
    comparison = left.compare(right);
    return true;
}
Status errorBound(const Wide& n, const Wide& d, double& out) {
    if (n.compare(Wide()) == 0) { out = 0; return Ok(); }
    double bound = n.approximate() / d.approximate();
    int comparison = 0;
    // At most two bounded integer conversions and one division precede this.
    for (unsigned i = 0; i < 8; ++i) {
        if (!compareWideDouble(n, d, bound, comparison)) break;
        if (comparison <= 0) { out = bound; return Ok(); }
        bound = std::nextafter(bound, std::numeric_limits<double>::infinity());
    }
    return fail(UnitError::PRECISION_LIMIT, "cannot bound native float error");
}
struct Factors {
    uint64_t n[12] = {}, d[12] = {};
    unsigned count = 0;
    int sign = 1, tauPower = 0;
    bool add(uint64_t a, uint64_t b, bool inverse = false) {
        if (count == 12 || !a || !b) return false;
        n[count] = inverse ? b : a; d[count++] = inverse ? a : b;
        return true;
    }
};
bool positionValid(PositionUnit unit) { return unit <= PositionUnit::MILLIMETRES; }
bool timeValid(TimeUnit unit) { return unit <= TimeUnit::MILLISECOND; }
Status addScale(const UnitScale& scale, Factors& f, bool inverse) {
    if (scale.source == ScaleSource::UNKNOWN)
        return fail(UnitError::MISSING_SCALE, "required scale is unknown", Err::INVALID_CONFIG);
    return f.add(scale.numerator, scale.denominator, inverse) ? Ok() :
        fail(UnitError::ARITHMETIC_OVERFLOW, "factor capacity");
}
Status spatial(PositionUnit u, const UnitConfig& c, Factors& f, EncoderBasis& basis, bool inverse) {
    basis = EncoderBasis::LOAD_TURN;
    switch (u) {
    case PositionUnit::TURNS: return Ok();
    case PositionUnit::DEGREES: return f.add(1, 360, inverse) ? Ok() : fail(UnitError::ARITHMETIC_OVERFLOW, "factor capacity");
    case PositionUnit::RADIANS: f.tauPower += inverse ? 1 : -1; return Ok();
    case PositionUnit::MILLIMETRES: basis = EncoderBasis::MILLIMETRE; return Ok();
    case PositionUnit::STEPS:
        basis = EncoderBasis::MOTOR_TURN; f.sign *= c.commandPolarity;
        return addScale(c.commandStepsPerMotorTurn, f, !inverse);
    case PositionUnit::FULL_STEPS:
        basis = EncoderBasis::MOTOR_TURN;
        return addScale(c.fullStepsPerMotorTurn, f, !inverse);
    case PositionUnit::ENCODER_COUNTS:
        basis = c.encoder.basis; f.sign *= c.encoder.polarity;
        return addScale(c.encoder.countsPerUnit, f, !inverse);
    }
    return fail(UnitError::INVALID_UNIT, "invalid spatial unit");
}
Status selectFactors(PositionUnit from, PositionUnit to, const UnitConfig& c, Factors& f) {
    Status status = validateUnitConfig(c);
    if (!status) return status;
    if (!positionValid(from) || !positionValid(to)) return fail(UnitError::INVALID_UNIT, "invalid spatial unit");
    if (from == PositionUnit::ENCODER_COUNTS || to == PositionUnit::ENCODER_COUNTS) {
        if (c.encoder.countsPerUnit.source == ScaleSource::UNKNOWN)
            return fail(UnitError::MISSING_SCALE, "selected encoder scale is absent", Err::INVALID_CONFIG);
    }
    if (from == to) return Ok();
    EncoderBasis a, b;
    status = spatial(from, c, f, a, false); if (!status) return status;
    status = spatial(to, c, f, b, true); if (!status) return status;
    if (a == b) return Ok();
    if (a != EncoderBasis::LOAD_TURN) {
        status = addScale(a == EncoderBasis::MOTOR_TURN ? c.motorTurnsPerLoadTurn : c.millimetresPerLoadTurn, f, true);
        if (!status) return status;
    }
    if (b != EncoderBasis::LOAD_TURN)
        return addScale(b == EncoderBasis::MOTOR_TURN ? c.motorTurnsPerLoadTurn : c.millimetresPerLoadTurn, f, false);
    return Ok();
}
Status addTime(TimeUnit from, TimeUnit to, Factors& f) {
    if (!timeValid(from) || !timeValid(to)) return fail(UnitError::INVALID_UNIT, "invalid time unit");
    const uint64_t n[] = {1, 60, 1}, d[] = {1, 1, 1000};
    return f.add(n[unsigned(to)], d[unsigned(to)]) && f.add(d[unsigned(from)], n[unsigned(from)]) ? Ok() :
        fail(UnitError::ARITHMETIC_OVERFLOW, "factor capacity");
}
Status exact(const Rational& value, Factors f, Rational& output) {
    if (!value.denominator) return fail(UnitError::INVALID_ARGUMENT, "zero denominator");
    if (!value.numerator) { output = Rational(); return Ok(); }
    if (f.tauPower) return fail(UnitError::APPROXIMATION_REQUIRED, "radians need explicit approximation", Err::UNSUPPORTED);
    if (!f.add(magnitude(value.numerator), value.denominator)) return fail(UnitError::ARITHMETIC_OVERFLOW, "factor capacity");
    for (unsigned i = 0; i < f.count; ++i) for (unsigned j = 0; j < f.count; ++j) {
        const uint64_t common = gcd(f.n[i], f.d[j]);
        f.n[i] /= common; f.d[j] /= common;
    }
    uint64_t n = 1, d = 1;
    for (unsigned i = 0; i < f.count; ++i) {
        if (n > UINT64_MAX / f.n[i] || d > UINT64_MAX / f.d[i])
            return fail(UnitError::ARITHMETIC_OVERFLOW, "reduced fraction exceeds fixed storage");
        n *= f.n[i]; d *= f.d[i];
    }
    return fraction(n, d, (value.numerator < 0) != (f.sign < 0), output);
}
Status approximate(const Rational& value, const Factors& f, double allowance, UnitConversion& output) {
    if (!std::isfinite(allowance) || allowance <= 0)
        return fail(UnitError::INVALID_ARGUMENT, "approximation allowance must be positive and finite");
    double result;
    Status status = rationalToDouble(value, result); if (!status) return status;
    for (unsigned i = 0; i < f.count; ++i) result *= double(f.n[i]) / double(f.d[i]);
    const double tau = 6.283185307179586476925286766559;
    if (f.tauPower < 0) result /= tau;
    if (f.tauPower > 0) result *= tau;
    result *= f.sign;
    // <=32 rounded operations, including directed input approximation and tau.
    // 64 epsilon is conservative on binary64; no long-double precision assumed.
    double bound = std::fabs(result) * (64 * std::numeric_limits<double>::epsilon());
    if (bound) bound = std::nextafter(bound, std::numeric_limits<double>::infinity());
    if (!std::isfinite(result) || !std::isfinite(bound) || bound > allowance ||
        (value.numerator && (result == 0 || std::fpclassify(result) != FP_NORMAL || bound == 0)))
        return fail(UnitError::PRECISION_LIMIT, "approximation cannot meet error allowance");
    UnitConversion candidate; candidate.value = result; candidate.absoluteErrorBound = bound;
    output = candidate; return Ok();
}
} // namespace

Status reduceRational(const Rational& value, Rational& output) {
    return fraction(magnitude(value.numerator), value.denominator, value.numerator < 0, output);
}
Status parseExactNumber(const char* text, Rational& output) {
    if (!text) return fail(UnitError::INVALID_ARGUMENT, "missing exact number");
    unsigned length = 0;
    while (length < 128 && text[length]) ++length;
    if (length == 128) return fail(UnitError::CAPACITY, "exact text exceeds 127 characters");
    if (!length) return fail(UnitError::INVALID_TEXT, "empty exact number");
    unsigned i = 0; bool negative = false;
    if (text[i] == '-' || text[i] == '+') { negative = text[i++] == '-'; if (i == length) return fail(UnitError::INVALID_TEXT, "missing digits"); }
    if (text[length-1] == '.') return fail(UnitError::INVALID_TEXT, "missing decimal digits");
    unsigned point = length;
    for (unsigned j = i; j < length; ++j) if (text[j] == '.') { point = j; break; }
    if (point < length) {
        for (unsigned j = i; j < length; ++j) if (text[j] == '/') return fail(UnitError::INVALID_TEXT, "fraction operands must be integers");
        while (length > point + 1 && text[length-1] == '0') --length;
        if (length == point + 1) length = point;
    }
    uint64_t n = 0, d = 1; bool digits = false, decimal = false, part = false;
    for (; i < length; ++i) {
        const char ch = text[i];
        if (ch == '.' && !decimal && !part && digits) { decimal = true; continue; }
        if (ch == '/' && !decimal && !part && digits) { part = true; d = 0; digits = false; continue; }
        if (ch < '0' || ch > '9') return fail(UnitError::INVALID_TEXT, "invalid exact number syntax");
        digits = true; const unsigned digit = unsigned(ch - '0');
        uint64_t& component = part ? d : n;
        if (component > (UINT64_MAX - digit) / 10) return fail(UnitError::CAPACITY, "exact component exceeds uint64");
        component = component * 10 + digit;
        if (decimal) {
            if (d > UINT64_MAX / 10) return fail(UnitError::CAPACITY, "decimal denominator exceeds uint64");
            d *= 10;
        }
    }
    if (!digits || !d || text[length-1] == '.') return fail(UnitError::INVALID_TEXT, "missing denominator or digits");
    return fraction(n, d, negative, output);
}
Status rationalToDouble(const Rational& value, double& output) {
    if (!value.denominator) return fail(UnitError::INVALID_ARGUMENT, "zero denominator");
    double absolute = double(magnitude(value.numerator)) / double(value.denominator);
    for (unsigned corrections = 0; corrections < 8; ++corrections) {
        if (compareRatio(magnitude(value.numerator), value.denominator, absolute) >= 0) {
            // Rounded operands may also start below the directed result. Find
            // the closest representable magnitude without exceeding the ratio.
            for (unsigned up = 0; up < 8; ++up) {
                const double next = std::nextafter(absolute, std::numeric_limits<double>::infinity());
                if (compareRatio(magnitude(value.numerator), value.denominator, next) < 0) {
                    output = value.numerator < 0 ? -absolute : absolute; return Ok();
                }
                absolute = next;
            }
            break;
        }
        absolute = std::nextafter(absolute, 0.0);
    }
    return fail(UnitError::PRECISION_LIMIT, "cannot establish directed binary64 conversion");
}
Status validateUnitConfig(const UnitConfig& c) {
    const UnitScale* scales[] = {&c.commandStepsPerMotorTurn, &c.motorTurnsPerLoadTurn,
        &c.fullStepsPerMotorTurn, &c.millimetresPerLoadTurn, &c.encoder.countsPerUnit};
    for (const UnitScale* s : scales) {
        if (s->source > ScaleSource::QUALIFIED || !s->denominator ||
            (s->source == ScaleSource::UNKNOWN ? (s->numerator != 0 || s->denominator != 1) : s->numerator == 0))
            return fail(UnitError::INVALID_SCALE, "malformed scale", Err::INVALID_CONFIG);
    }
    if ((c.commandPolarity != 1 && c.commandPolarity != -1) || (c.encoder.polarity != 1 && c.encoder.polarity != -1))
        return fail(UnitError::INVALID_POLARITY, "polarity must be +1/-1", Err::INVALID_CONFIG);
    if (c.encoder.basis > EncoderBasis::MILLIMETRE) return fail(UnitError::INVALID_UNIT, "invalid encoder basis", Err::INVALID_CONFIG);
    if (c.encoder.countsPerUnit.source != ScaleSource::UNKNOWN && !c.encoder.sourceId)
        return fail(UnitError::MISSING_SCALE, "known encoder requires source identity", Err::INVALID_CONFIG);
    return Ok();
}
Status convertDisplacement(const Rational& value, PositionUnit from, PositionUnit to, const UnitConfig& config, Rational& output) {
    Factors f; Status status = selectFactors(from, to, config, f);
    return status ? exact(value, f, output) : status;
}
Status convertVelocity(const Rational& value, VelocityUnit from, VelocityUnit to, const UnitConfig& config, Rational& output) {
    Factors f; Status status = selectFactors(from.position, to.position, config, f);
    if (status) status = addTime(from.time, to.time, f);
    return status ? exact(value, f, output) : status;
}
Status convertAcceleration(const Rational& value, AccelerationUnit from, AccelerationUnit to, const UnitConfig& config, Rational& output) {
    Factors f; Status status = selectFactors(from.position, to.position, config, f);
    if (status) status = addTime(from.velocityTime, to.velocityTime, f);
    if (status) status = addTime(from.accelerationTime, to.accelerationTime, f);
    return status ? exact(value, f, output) : status;
}
Status convertDisplacement(const Rational& value, PositionUnit from, PositionUnit to, const UnitConfig& config, double maximumError, UnitConversion& output) {
    Factors f; Status status = selectFactors(from, to, config, f);
    return status ? approximate(value, f, maximumError, output) : status;
}
Status convertVelocity(const Rational& value, VelocityUnit from, VelocityUnit to, const UnitConfig& config, double maximumError, UnitConversion& output) {
    Factors f; Status status = selectFactors(from.position, to.position, config, f);
    if (status) status = addTime(from.time, to.time, f);
    return status ? approximate(value, f, maximumError, output) : status;
}
Status convertAcceleration(const Rational& value, AccelerationUnit from, AccelerationUnit to, const UnitConfig& config, double maximumError, UnitConversion& output) {
    Factors f; Status status = selectFactors(from.position, to.position, config, f);
    if (status) status = addTime(from.velocityTime, to.velocityTime, f);
    if (status) status = addTime(from.accelerationTime, to.accelerationTime, f);
    return status ? approximate(value, f, maximumError, output) : status;
}
Status quantizeInteger(const Rational& value, Rounding rounding, const Rational& maximumError, IntegerQuantization& output) {
    if (!value.denominator || !maximumError.denominator || maximumError.numerator < 0)
        return fail(UnitError::INVALID_ARGUMENT, "invalid quantity/allowance");
    if (rounding > Rounding::CEIL) return fail(UnitError::INVALID_UNIT, "invalid rounding policy");
    const bool negative = value.numerator < 0;
    const uint64_t n = magnitude(value.numerator), d = value.denominator;
    uint64_t integral = n / d, remainder = n % d;
    if (remainder && rounding == Rounding::EXACT) return fail(UnitError::FRACTIONAL_TARGET, "not an exact integer");
    const uint64_t other = d - remainder;
    const bool away = remainder && ((rounding == Rounding::FLOOR && negative) ||
        (rounding == Rounding::CEIL && !negative) || (rounding == Rounding::NEAREST &&
        (remainder > other || (remainder == other && integral % 2))));
    if (away) ++integral; // quotient <=2^63, and remainder prevents the extreme case.
    IntegerQuantization candidate;
    if (!signedMagnitude(integral, negative, candidate.value)) return fail(UnitError::ARITHMETIC_OVERFLOW, "rounded integer overflow");
    candidate.errorNumerator = remainder ? (away ? other : remainder) : 0;
    candidate.errorDenominator = d;
    const uint64_t common = gcd(candidate.errorNumerator, d);
    candidate.errorNumerator /= common; candidate.errorDenominator /= common;
    candidate.errorNegative = candidate.errorNumerator && (away == negative);
    Wide error(candidate.errorNumerator), allowance(magnitude(maximumError.numerator));
    if (!error.multiply(maximumError.denominator) || !allowance.multiply(candidate.errorDenominator))
        return fail(UnitError::ARITHMETIC_OVERFLOW, "allowance product overflow");
    if (error.compare(allowance) > 0) return fail(UnitError::QUANTIZATION_ERROR, "integer error exceeds allowance");
    output = candidate; return Ok();
}
Status quantizeFloat(const Rational& value, const Rational& maximumError, FloatQuantization& output) {
    if (!value.denominator || !maximumError.denominator || maximumError.numerator < 0)
        return fail(UnitError::INVALID_ARGUMENT, "invalid quantity/allowance");
    double initial;
    Status status = rationalToDouble(value, initial); if (!status) return status;
    const uint64_t n = magnitude(value.numerator), d = value.denominator;
    float candidate = static_cast<float>(std::fabs(initial));
    bool established = n == 0;
    for (unsigned i = 0; !established && i < 4; ++i) {
        uint32_t bits; std::memcpy(&bits, &candidate, sizeof bits);
        const float below = std::nextafter(candidate, 0.0f);
        const float above = std::nextafter(candidate, std::numeric_limits<float>::infinity());
        const int low = compareRatio(n, d, (double(candidate) + below) / 2);
        const int high = compareRatio(n, d, (double(candidate) + above) / 2);
        if (low < 0 || (low == 0 && (bits & 1u))) candidate = below;
        else if (high > 0 || (high == 0 && (bits & 1u))) candidate = above;
        else established = true;
    }
    if (!established) return fail(UnitError::PRECISION_LIMIT, "cannot establish nearest binary32");
    int exponent = 0;
    uint64_t mantissa = candidate ? uint64_t(std::ldexp(std::frexp(double(candidate), &exponent), 24)) : 0;
    exponent -= 24;
    while (mantissa && !(mantissa & 1u)) { mantissa >>= 1; ++exponent; }
    Wide floatN(mantissa), floatD(1);
    if (exponent < 0 ? !floatD.shift(unsigned(-exponent)) : !floatN.shift(unsigned(exponent)))
        return fail(UnitError::ARITHMETIC_OVERFLOW, "float dyadic exceeds storage");
    Wide a = floatN, b = floatD, denominator = floatD;
    if (!a.multiply(d) || !b.multiply(n) || !denominator.multiply(d)) return fail(UnitError::ARITHMETIC_OVERFLOW, "float error product overflow");
    Wide error = a.compare(b) >= 0 ? a : b;
    error.subtract(a.compare(b) >= 0 ? b : a);
    Wide left = error, right = denominator;
    if (!left.multiply(maximumError.denominator) || !right.multiply(magnitude(maximumError.numerator)))
        return fail(UnitError::ARITHMETIC_OVERFLOW, "float allowance product overflow");
    if (left.compare(right) > 0) return fail(UnitError::QUANTIZATION_ERROR, "binary32 error exceeds allowance");
    FloatQuantization result; result.value = value.numerator < 0 ? -candidate : candidate;
    status = errorBound(error, denominator, result.absoluteErrorBound); if (!status) return status;
    output = result; return Ok();
}
} // namespace MotorControlCAN
