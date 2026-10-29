/** @file units_test.cpp
 * @brief Independent exact arithmetic, dimensional and quantization vectors.
 * The N5/EPOS4 scales below are test inputs, not implemented vendor profiles.
 * SPDX-License-Identifier: MIT
 */
#include <MotorControlCAN/Units.h>

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>

namespace M = MotorControlCAN;
namespace {
void check(bool condition, const char* expression, int line) {
    if (!condition) {
        std::fprintf(stderr, "units_test.cpp:%d: %s\n", line, expression);
        std::abort();
    }
}
#define CHECK(expression) check(static_cast<bool>(expression), #expression, __LINE__)

bool equal(const M::Rational& value, int64_t n, uint64_t d = 1) {
    return value.numerator == n && value.denominator == d;
}
template <typename T> struct Saved {
    unsigned char bytes[sizeof(T)];
    explicit Saved(const T& value) { std::memcpy(bytes, &value, sizeof value); }
    bool unchanged(const T& value) const { return std::memcmp(bytes, &value, sizeof value) == 0; }
};
M::UnitScale scale(uint32_t numerator, uint32_t denominator = 1) {
    return M::UnitScale(numerator, denominator, M::ScaleSource::DOCUMENTED);
}
M::UnitConfig configured() {
    M::UnitConfig config;
    config.commandStepsPerMotorTurn = scale(4096);
    config.motorTurnsPerLoadTurn = scale(3, 2);
    config.encoder.countsPerUnit = scale(2048);
    config.encoder.sourceId = 5;
    config.millimetresPerLoadTurn = scale(8);
    return config;
}
uint32_t floatBits(float value) {
    static_assert(sizeof(float) == sizeof(uint32_t), "IEEE binary32 fixture");
    uint32_t bits = 0;
    std::memcpy(&bits, &value, sizeof bits);
    return bits;
}

void parserAndReduction() {
    struct Vector { const char* text; int64_t numerator; uint64_t denominator; };
    const Vector vectors[] = {
        {"0", 0, 1}, {"-0", 0, 1}, {"+0000", 0, 1}, {"-0.000", 0, 1},
        {"001.2500", 5, 4}, {"-1.250", -5, 4}, {"123/246", 1, 2},
        {"0/18446744073709551615", 0, 1},
        {"1/18446744073709551615", 1, UINT64_MAX},
        {"18446744073709551615/18446744073709551615", 1, 1},
        {"-18446744073709551615/18446744073709551615", -1, 1},
        {"18446744073709551614/2", INT64_MAX, 1},
        {"18446744073709551615/3", INT64_C(6148914691236517205), 1},
        {"9223372036854775807.000", INT64_MAX, 1},
        {"-9223372036854775808", INT64_MIN, 1},
        {"-9223372036854775808.000", INT64_MIN, 1},
        {"0.0000000000000000001", 1, UINT64_C(10000000000000000000)},
        {"9007199254740991", INT64_C(9007199254740991), 1},
        {"9007199254740992", INT64_C(9007199254740992), 1},
        {"9007199254740993", INT64_C(9007199254740993), 1}
    };
    for (unsigned i = 0; i < sizeof vectors / sizeof vectors[0]; ++i) {
        M::Rational output(71, 3);
        CHECK(M::parseExactNumber(vectors[i].text, output));
        CHECK(equal(output, vectors[i].numerator, vectors[i].denominator));
    }
    const char* malformed[] = {
        nullptr, "", "+", "-", "1/-2", "1/+2", "1/", "/1", "1/0", "1/00",
        "1/2.0", "1.0/2", "1.00/2.000", "1e3", ".5", "1.", "1..0", "1.2.3",
        "--1", "+-1", " 1", "1 ", "1\n", "NaN", "inf", "0x10", "1_000",
        "9223372036854775808", "-9223372036854775809",
        "18446744073709551616/18446744073709551615", "1/18446744073709551616",
        "0.00000000000000000001", "18446744073709551615/2"
    };
    for (unsigned i = 0; i < sizeof malformed / sizeof malformed[0]; ++i) {
        M::Rational output(71, 3);
        CHECK(!M::parseExactNumber(malformed[i], output));
        CHECK(equal(output, 71, 3));
    }
    char boundary[129];
    std::memset(boundary, '0', 128);
    boundary[127] = '\0';
    M::Rational output(71, 3);
    CHECK(M::parseExactNumber(boundary, output));
    CHECK(equal(output, 0));
    boundary[127] = '0'; boundary[128] = '\0'; output = M::Rational(71, 3);
    CHECK(!M::parseExactNumber(boundary, output)); CHECK(equal(output, 71, 3));
    char unterminated[128]; std::memset(unterminated, '0', sizeof unterminated);
    CHECK(!M::parseExactNumber(unterminated, output)); CHECK(equal(output, 71, 3));

    CHECK(M::reduceRational(M::Rational(INT64_MIN, UINT64_C(9223372036854775808)), output));
    CHECK(equal(output, -1));
    CHECK(M::reduceRational(M::Rational(0, UINT64_MAX), output)); CHECK(equal(output, 0));
    CHECK(M::reduceRational(M::Rational(-14, 21), output)); CHECK(equal(output, -2, 3));
    CHECK(M::reduceRational(output, output)); CHECK(equal(output, -2, 3));
    output = M::Rational(71, 3);
    CHECK(!M::reduceRational(M::Rational(0, 0), output)); CHECK(equal(output, 71, 3));
}

void directedDouble() {
    double output = 71;
    CHECK(M::rationalToDouble(M::Rational(1, 2), output)); CHECK(output == 0.5);
    CHECK(M::rationalToDouble(M::Rational(1, 3), output)); CHECK(output == 1.0 / 3.0);
    CHECK(M::rationalToDouble(M::Rational(1, 10), output));
    CHECK(output == std::nextafter(0.1, 0.0));
    CHECK(M::rationalToDouble(M::Rational(-1, 10), output));
    CHECK(output == -std::nextafter(0.1, 0.0));
    CHECK(M::rationalToDouble(M::Rational(INT64_C(9007199254740993)), output));
    CHECK(output == 9007199254740992.0);
    CHECK(M::rationalToDouble(M::Rational(-INT64_C(9007199254740993)), output));
    CHECK(output == -9007199254740992.0);
    CHECK(M::rationalToDouble(M::Rational(INT64_MAX), output));
    CHECK(output == std::nextafter(9223372036854775808.0, 0.0));
    CHECK(M::rationalToDouble(M::Rational(INT64_MIN), output));
    CHECK(output == -9223372036854775808.0);
    CHECK(M::rationalToDouble(M::Rational(INT64_C(4730525896183341671),
        UINT64_C(13991455622818038268)), output));
    CHECK(output == std::nextafter(0.33810105422258846, 0.0));
    // Rounding each uint64 operand can put the first quotient below the nearest
    // toward-zero binary64 value; directed conversion must also check upward.
    CHECK(M::rationalToDouble(M::Rational(-INT64_C(640212508715691325),
        UINT64_C(12164937634629641363)), output));
    CHECK(output == -0.05262768523310908);
    CHECK(M::rationalToDouble(M::Rational(INT64_C(640212508715691325),
        UINT64_C(12164937634629641363)), output));
    CHECK(output == 0.05262768523310908);
    output = 71;
    CHECK(!M::rationalToDouble(M::Rational(1, 0), output)); CHECK(output == 71);
}

void dimensionsAndScaleDependencies() {
    M::UnitConfig config;
    M::Rational output(71, 3);
    CHECK(M::validateUnitConfig(config));
    CHECK(M::convertDisplacement(M::Rational(360), M::PositionUnit::DEGREES,
        M::PositionUnit::TURNS, config, output)); CHECK(equal(output, 1));
    CHECK(M::convertDisplacement(M::Rational(-720), M::PositionUnit::DEGREES,
        M::PositionUnit::TURNS, config, output)); CHECK(equal(output, -2));
    CHECK(M::convertDisplacement(M::Rational(1), M::PositionUnit::RADIANS,
        M::PositionUnit::RADIANS, config, output)); CHECK(equal(output, 1));
    CHECK(M::convertDisplacement(M::Rational(0), M::PositionUnit::RADIANS,
        M::PositionUnit::TURNS, config, output)); CHECK(equal(output, 0));
    output = M::Rational(71, 3);
    CHECK(!M::convertDisplacement(M::Rational(1), M::PositionUnit::RADIANS,
        M::PositionUnit::DEGREES, config, output)); CHECK(equal(output, 71, 3));
    CHECK(!M::convertDisplacement(M::Rational(1), M::PositionUnit::TURNS,
        M::PositionUnit::STEPS, config, output)); CHECK(equal(output, 71, 3));
    CHECK(!M::convertDisplacement(M::Rational(0), M::PositionUnit::TURNS,
        M::PositionUnit::STEPS, config, output)); CHECK(equal(output, 71, 3));
    CHECK(!M::convertDisplacement(M::Rational(1), M::PositionUnit::ENCODER_COUNTS,
        M::PositionUnit::ENCODER_COUNTS, config, output)); CHECK(equal(output, 71, 3));

    config = configured();
    CHECK(config.fullStepsPerMotorTurn.source == M::ScaleSource::UNKNOWN);
    CHECK(M::convertDisplacement(M::Rational(1, 2), M::PositionUnit::TURNS,
        M::PositionUnit::STEPS, config, output)); CHECK(equal(output, 3072));
    CHECK(M::convertDisplacement(M::Rational(1, 2), M::PositionUnit::TURNS,
        M::PositionUnit::ENCODER_COUNTS, config, output)); CHECK(equal(output, 1536));
    CHECK(M::convertDisplacement(M::Rational(1, 2), M::PositionUnit::TURNS,
        M::PositionUnit::MILLIMETRES, config, output)); CHECK(equal(output, 4));
    CHECK(M::convertDisplacement(M::Rational(2), M::PositionUnit::MILLIMETRES,
        M::PositionUnit::STEPS, config, output)); CHECK(equal(output, 1536));
    CHECK(M::convertDisplacement(M::Rational(3072), M::PositionUnit::STEPS,
        M::PositionUnit::DEGREES, config, output)); CHECK(equal(output, 180));

    // Both quantities have a motor basis: gear and lead are irrelevant.
    config.motorTurnsPerLoadTurn = M::UnitScale();
    config.millimetresPerLoadTurn = M::UnitScale();
    CHECK(M::convertDisplacement(M::Rational(4096), M::PositionUnit::STEPS,
        M::PositionUnit::ENCODER_COUNTS, config, output)); CHECK(equal(output, 2048));
    config.commandPolarity = -1;
    CHECK(M::convertDisplacement(M::Rational(4096), M::PositionUnit::STEPS,
        M::PositionUnit::ENCODER_COUNTS, config, output)); CHECK(equal(output, -2048));
    config.encoder.polarity = -1;
    CHECK(M::convertDisplacement(M::Rational(4096), M::PositionUnit::STEPS,
        M::PositionUnit::ENCODER_COUNTS, config, output)); CHECK(equal(output, 2048));
    config.fullStepsPerMotorTurn = scale(200);
    CHECK(M::convertDisplacement(M::Rational(200), M::PositionUnit::FULL_STEPS,
        M::PositionUnit::STEPS, config, output)); CHECK(equal(output, -4096));

    config = M::UnitConfig(); config.encoder.countsPerUnit = scale(4000);
    config.encoder.sourceId = 17; config.encoder.basis = M::EncoderBasis::LOAD_TURN;
    CHECK(M::convertDisplacement(M::Rational(4000), M::PositionUnit::ENCODER_COUNTS,
        M::PositionUnit::DEGREES, config, output)); CHECK(equal(output, 360));
    config.encoder.countsPerUnit = scale(100); config.encoder.basis = M::EncoderBasis::MILLIMETRE;
    CHECK(M::convertDisplacement(M::Rational(1000), M::PositionUnit::ENCODER_COUNTS,
        M::PositionUnit::MILLIMETRES, config, output)); CHECK(equal(output, 10));
    output = M::Rational(71, 3);
    CHECK(!M::convertDisplacement(M::Rational(1000), M::PositionUnit::ENCODER_COUNTS,
        M::PositionUnit::TURNS, config, output)); CHECK(equal(output, 71, 3));
    config.millimetresPerLoadTurn = scale(8); config.encoder.polarity = -1;
    CHECK(M::convertDisplacement(M::Rational(800), M::PositionUnit::ENCODER_COUNTS,
        M::PositionUnit::TURNS, config, output)); CHECK(equal(output, -1));

    // Documentary N5 tenth-degree scale and abstract EPOS4 servo scale, with no full steps.
    config = M::UnitConfig(); config.motorTurnsPerLoadTurn = scale(1);
    const uint32_t commandScales[] = {3600, 8192};
    const int64_t expected[] = {5400, 12288};
    for (unsigned i = 0; i < 2; ++i) {
        config.commandStepsPerMotorTurn = scale(commandScales[i]);
        CHECK(M::convertDisplacement(M::Rational(3, 2), M::PositionUnit::TURNS,
            M::PositionUnit::STEPS, config, output)); CHECK(equal(output, expected[i]));
    }
}

void exactExtremesAndInvalidFacts() {
    M::UnitConfig config;
    M::Rational output(71, 3);
    const int64_t integers[] = {INT64_MIN, INT64_MAX, INT64_C(9007199254740991),
        INT64_C(9007199254740992), INT64_C(9007199254740993)};
    for (unsigned i = 0; i < sizeof integers / sizeof integers[0]; ++i) {
        CHECK(M::convertDisplacement(M::Rational(integers[i]), M::PositionUnit::STEPS,
            M::PositionUnit::STEPS, config, output)); CHECK(equal(output, integers[i]));
    }
    config.commandStepsPerMotorTurn = scale(UINT32_MAX, UINT32_MAX - 1);
    config.encoder.countsPerUnit = config.commandStepsPerMotorTurn; config.encoder.sourceId = 1;
    CHECK(M::convertDisplacement(M::Rational(INT64_MAX), M::PositionUnit::STEPS,
        M::PositionUnit::ENCODER_COUNTS, config, output)); CHECK(equal(output, INT64_MAX));
    config = M::UnitConfig(); config.motorTurnsPerLoadTurn = scale(1);
    config.commandStepsPerMotorTurn = scale(UINT32_MAX);
    output = M::Rational(71, 3);
    CHECK(!M::convertDisplacement(M::Rational(INT64_MAX), M::PositionUnit::TURNS,
        M::PositionUnit::STEPS, config, output)); CHECK(equal(output, 71, 3));
    config.commandStepsPerMotorTurn = scale(1, 2);
    CHECK(!M::convertDisplacement(M::Rational(1, UINT64_MAX), M::PositionUnit::TURNS,
        M::PositionUnit::STEPS, config, output)); CHECK(equal(output, 71, 3));
    CHECK(M::convertDisplacement(M::Rational(2, UINT64_MAX), M::PositionUnit::TURNS,
        M::PositionUnit::STEPS, config, output)); CHECK(equal(output, 1, UINT64_MAX));
    // Two maximal uint32 scale denominators still have a valid uint64 product.
    config.commandStepsPerMotorTurn = scale(1, UINT32_MAX);
    config.motorTurnsPerLoadTurn = scale(1, UINT32_MAX);
    CHECK(M::convertDisplacement(M::Rational(1), M::PositionUnit::TURNS,
        M::PositionUnit::STEPS, config, output));
    CHECK(equal(output, 1, UINT64_C(18446744065119617025)));
    config.commandStepsPerMotorTurn = scale(UINT32_MAX);
    config.motorTurnsPerLoadTurn = scale(UINT32_MAX, UINT32_MAX - 1);
    CHECK(M::convertDisplacement(M::Rational(UINT32_MAX - 1, UINT32_MAX), M::PositionUnit::TURNS,
        M::PositionUnit::STEPS, config, output)); CHECK(equal(output, UINT32_MAX));

    config.commandStepsPerMotorTurn = scale(1); config.fullStepsPerMotorTurn = scale(1);
    config.commandPolarity = -1; output = M::Rational(71, 3);
    CHECK(!M::convertDisplacement(M::Rational(INT64_MIN), M::PositionUnit::STEPS,
        M::PositionUnit::FULL_STEPS, config, output)); CHECK(equal(output, 71, 3));
    CHECK(M::convertDisplacement(M::Rational(INT64_MIN, 2), M::PositionUnit::STEPS,
        M::PositionUnit::FULL_STEPS, config, output));
    CHECK(equal(output, INT64_C(4611686018427387904)));
    CHECK(M::convertDisplacement(M::Rational(0, UINT64_MAX), M::PositionUnit::STEPS,
        M::PositionUnit::FULL_STEPS, config, output)); CHECK(equal(output, 0));
    M::Rational aliased(INT64_MIN, 2);
    CHECK(M::convertDisplacement(aliased, M::PositionUnit::STEPS,
        M::PositionUnit::FULL_STEPS, config, aliased));
    CHECK(equal(aliased, INT64_C(4611686018427387904)));

    const M::UnitScale invalidScales[] = {M::UnitScale(0, 0), M::UnitScale(0, 2),
        M::UnitScale(1, 1, M::ScaleSource::UNKNOWN), scale(0), scale(1, 0),
        M::UnitScale(1, 1, static_cast<M::ScaleSource>(255))};
    for (unsigned i = 0; i < sizeof invalidScales / sizeof invalidScales[0]; ++i) {
        config = configured(); config.fullStepsPerMotorTurn = invalidScales[i];
        output = M::Rational(71, 3);
        CHECK(!M::validateUnitConfig(config));
        CHECK(!M::convertDisplacement(M::Rational(360), M::PositionUnit::DEGREES,
            M::PositionUnit::TURNS, config, output)); CHECK(equal(output, 71, 3));
    }
    config = configured(); config.encoder.sourceId = 0; CHECK(!M::validateUnitConfig(config));
    config = configured(); config.commandPolarity = 0; CHECK(!M::validateUnitConfig(config));
    config = configured(); config.encoder.polarity = 2; CHECK(!M::validateUnitConfig(config));
    config = configured(); config.encoder.basis = static_cast<M::EncoderBasis>(255);
    CHECK(!M::validateUnitConfig(config));
    config = configured(); output = M::Rational(71, 3);
    CHECK(!M::convertDisplacement(M::Rational(1, 0), M::PositionUnit::TURNS,
        M::PositionUnit::TURNS, config, output)); CHECK(equal(output, 71, 3));
    CHECK(!M::convertDisplacement(M::Rational(1), static_cast<M::PositionUnit>(255),
        M::PositionUnit::TURNS, config, output)); CHECK(equal(output, 71, 3));
    CHECK(!M::convertDisplacement(M::Rational(1), M::PositionUnit::TURNS,
        static_cast<M::PositionUnit>(255), config, output)); CHECK(equal(output, 71, 3));
}

void velocityAndAcceleration() {
    const M::UnitConfig config = configured(); M::Rational output(71, 3);
    CHECK(M::convertVelocity(M::Rational(-70000),
        M::VelocityUnit(M::PositionUnit::TURNS, M::TimeUnit::MINUTE),
        M::VelocityUnit(M::PositionUnit::DEGREES), config, output)); CHECK(equal(output, -420000));
    CHECK(M::convertVelocity(M::Rational(1),
        M::VelocityUnit(M::PositionUnit::STEPS, M::TimeUnit::MILLISECOND),
        M::VelocityUnit(M::PositionUnit::STEPS), M::UnitConfig(), output)); CHECK(equal(output, 1000));
    CHECK(M::convertVelocity(M::Rational(1), M::VelocityUnit(M::PositionUnit::STEPS),
        M::VelocityUnit(M::PositionUnit::STEPS, M::TimeUnit::MINUTE), config, output)); CHECK(equal(output, 60));
    CHECK(M::convertVelocity(M::Rational(1),
        M::VelocityUnit(M::PositionUnit::STEPS, M::TimeUnit::MINUTE),
        M::VelocityUnit(M::PositionUnit::STEPS, M::TimeUnit::MILLISECOND), config, output));
    CHECK(equal(output, 1, 60000));
    CHECK(M::convertAcceleration(M::Rational(60),
        M::AccelerationUnit(M::PositionUnit::TURNS, M::TimeUnit::MINUTE, M::TimeUnit::SECOND),
        M::AccelerationUnit(M::PositionUnit::TURNS), config, output)); CHECK(equal(output, 1));
    CHECK(M::convertAcceleration(M::Rational(3600),
        M::AccelerationUnit(M::PositionUnit::TURNS, M::TimeUnit::MINUTE, M::TimeUnit::MINUTE),
        M::AccelerationUnit(M::PositionUnit::TURNS), config, output)); CHECK(equal(output, 1));
    CHECK(M::convertAcceleration(M::Rational(-1, 1000),
        M::AccelerationUnit(M::PositionUnit::STEPS, M::TimeUnit::MILLISECOND, M::TimeUnit::MILLISECOND),
        M::AccelerationUnit(M::PositionUnit::STEPS), config, output)); CHECK(equal(output, -1000));
    output = M::Rational(71, 3);
    CHECK(!M::convertVelocity(M::Rational(INT64_MAX),
        M::VelocityUnit(M::PositionUnit::STEPS, M::TimeUnit::MILLISECOND),
        M::VelocityUnit(M::PositionUnit::STEPS), config, output)); CHECK(equal(output, 71, 3));
    CHECK(!M::convertVelocity(M::Rational(1),
        M::VelocityUnit(M::PositionUnit::TURNS, static_cast<M::TimeUnit>(255)),
        M::VelocityUnit(M::PositionUnit::TURNS), config, output)); CHECK(equal(output, 71, 3));
    CHECK(!M::convertAcceleration(M::Rational(1), M::AccelerationUnit(M::PositionUnit::TURNS),
        M::AccelerationUnit(M::PositionUnit::TURNS, M::TimeUnit::SECOND, static_cast<M::TimeUnit>(255)),
        config, output)); CHECK(equal(output, 71, 3));
}

void approximations() {
    const M::UnitConfig config; M::UnitConversion output;
    CHECK(M::convertDisplacement(M::Rational(1), M::PositionUnit::RADIANS,
        M::PositionUnit::DEGREES, config, 1e-10, output));
    CHECK(std::fabs(output.value - 57.29577951308232) <= output.absoluteErrorBound);
    CHECK(output.absoluteErrorBound > 0 && output.absoluteErrorBound <= 1e-10);
    CHECK(M::convertDisplacement(M::Rational(1), M::PositionUnit::TURNS,
        M::PositionUnit::RADIANS, config, 1e-10, output));
    CHECK(std::fabs(output.value - 6.283185307179586) <= output.absoluteErrorBound);
    CHECK(M::convertVelocity(M::Rational(60),
        M::VelocityUnit(M::PositionUnit::RADIANS, M::TimeUnit::MINUTE),
        M::VelocityUnit(M::PositionUnit::DEGREES), config, 1e-10, output));
    CHECK(std::fabs(output.value - 57.29577951308232) <= output.absoluteErrorBound);
    CHECK(M::convertAcceleration(M::Rational(3600),
        M::AccelerationUnit(M::PositionUnit::RADIANS, M::TimeUnit::MINUTE, M::TimeUnit::MINUTE),
        M::AccelerationUnit(M::PositionUnit::DEGREES), config, 1e-10, output));
    CHECK(std::fabs(output.value - 57.29577951308232) <= output.absoluteErrorBound);
    output.value = 71; output.absoluteErrorBound = 3;
    const Saved<M::UnitConversion> saved(output);
    const double badAllowances[] = {0, -1, std::numeric_limits<double>::infinity(),
        std::numeric_limits<double>::quiet_NaN(), 1e-30};
    for (unsigned i = 0; i < sizeof badAllowances / sizeof badAllowances[0]; ++i) {
        CHECK(!M::convertDisplacement(M::Rational(1), M::PositionUnit::RADIANS,
            M::PositionUnit::DEGREES, config, badAllowances[i], output)); CHECK(saved.unchanged(output));
    }
    CHECK(!M::convertDisplacement(M::Rational(1, 0), M::PositionUnit::RADIANS,
        M::PositionUnit::DEGREES, config, 1e-10, output)); CHECK(saved.unchanged(output));
    CHECK(!M::convertVelocity(M::Rational(1),
        M::VelocityUnit(M::PositionUnit::RADIANS, static_cast<M::TimeUnit>(255)),
        M::VelocityUnit(M::PositionUnit::TURNS), config, 1e-10, output)); CHECK(saved.unchanged(output));
}

void integerQuantization() {
    struct Vector { int64_t n; uint64_t d; M::Rounding rounding; int64_t expected;
        uint64_t error; uint64_t errorDenominator; bool errorNegative; };
    const Vector vectors[] = {
        {1, 2, M::Rounding::NEAREST, 0, 1, 2, true}, {-1, 2, M::Rounding::NEAREST, 0, 1, 2, false},
        {3, 2, M::Rounding::NEAREST, 2, 1, 2, false}, {-3, 2, M::Rounding::NEAREST, -2, 1, 2, true},
        {5, 2, M::Rounding::NEAREST, 2, 1, 2, true}, {-5, 2, M::Rounding::NEAREST, -2, 1, 2, false},
        {3, 2, M::Rounding::TOWARD_ZERO, 1, 1, 2, true}, {-3, 2, M::Rounding::TOWARD_ZERO, -1, 1, 2, false},
        {3, 2, M::Rounding::FLOOR, 1, 1, 2, true}, {-3, 2, M::Rounding::FLOOR, -2, 1, 2, true},
        {3, 2, M::Rounding::CEIL, 2, 1, 2, false}, {-3, 2, M::Rounding::CEIL, -1, 1, 2, false},
        {INT64_C(9007199254740993), UINT64_C(18014398509481984), M::Rounding::NEAREST,
            1, UINT64_C(9007199254740991), UINT64_C(18014398509481984), false},
        {INT64_C(9007199254740991), UINT64_C(18014398509481984), M::Rounding::NEAREST,
            0, UINT64_C(9007199254740991), UINT64_C(18014398509481984), true},
        {INT64_MIN, UINT64_MAX, M::Rounding::NEAREST,
            -1, UINT64_C(9223372036854775807), UINT64_MAX, true},
        {2, 6, M::Rounding::TOWARD_ZERO, 0, 1, 3, true},
        {0, UINT64_MAX, M::Rounding::EXACT, 0, 0, 1, false},
        {INT64_MIN, 1, M::Rounding::EXACT, INT64_MIN, 0, 1, false},
        {INT64_MAX, 1, M::Rounding::EXACT, INT64_MAX, 0, 1, false},
        {INT64_C(9007199254740993), 1, M::Rounding::EXACT, INT64_C(9007199254740993), 0, 1, false}
    };
    for (unsigned i = 0; i < sizeof vectors / sizeof vectors[0]; ++i) {
        M::IntegerQuantization output;
        CHECK(M::quantizeInteger(M::Rational(vectors[i].n, vectors[i].d), vectors[i].rounding,
            M::Rational(1), output));
        CHECK(output.value == vectors[i].expected && output.errorNumerator == vectors[i].error);
        CHECK(output.errorDenominator == vectors[i].errorDenominator && output.errorNegative == vectors[i].errorNegative);
    }
    M::IntegerQuantization output; output.value = 71; output.errorNumerator = 17;
    output.errorDenominator = 19; output.errorNegative = true;
    const Saved<M::IntegerQuantization> saved(output);
    CHECK(!M::quantizeInteger(M::Rational(1, 2), M::Rounding::EXACT, M::Rational(1), output));
    CHECK(saved.unchanged(output));
    CHECK(!M::quantizeInteger(M::Rational(-5, 2), M::Rounding::NEAREST, M::Rational(49, 100), output));
    CHECK(saved.unchanged(output));
    CHECK(!M::quantizeInteger(M::Rational(1, 3), M::Rounding::TOWARD_ZERO,
        M::Rational(INT64_C(333333333333333333), UINT64_C(1000000000000000000)), output));
    CHECK(saved.unchanged(output));
    CHECK(!M::quantizeInteger(M::Rational(1, 0), M::Rounding::NEAREST, M::Rational(1), output));
    CHECK(saved.unchanged(output));
    CHECK(!M::quantizeInteger(M::Rational(1), static_cast<M::Rounding>(255), M::Rational(1), output));
    CHECK(saved.unchanged(output));
    CHECK(!M::quantizeInteger(M::Rational(1), M::Rounding::EXACT, M::Rational(-1), output));
    CHECK(saved.unchanged(output));
    CHECK(!M::quantizeInteger(M::Rational(1), M::Rounding::EXACT, M::Rational(0, 0), output));
    CHECK(saved.unchanged(output));
    CHECK(M::quantizeInteger(M::Rational(1, 3), M::Rounding::TOWARD_ZERO, M::Rational(1, 3), output));
    CHECK(output.value == 0 && output.errorNumerator == 1 && output.errorDenominator == 3 && output.errorNegative);
}

void floatingQuantization() {
    struct Vector { int64_t n; uint64_t d; uint32_t bits; };
    const Vector vectors[] = {
        {0, 1, 0}, {1, 2, 0x3F000000}, {-1, 2, 0xBF000000},
        {16777217, 1, 0x4B800000}, {16777219, 1, 0x4B800002}, {-16777217, 1, 0xCB800000},
        {INT64_C(9007199254740991), 1, 0x5A000000}, {INT64_C(9007199254740993), 1, 0x5A000000},
        {INT64_MAX, 1, 0x5F000000}, {INT64_MIN, 1, 0xDF000000},
        {16777217, 16777216, 0x3F800000},
        {INT64_C(1152921573326323713), UINT64_C(1152921504606846976), 0x3F800001},
        {-INT64_C(1152921573326323713), UINT64_C(1152921504606846976), 0xBF800001}
    };
    for (unsigned i = 0; i < sizeof vectors / sizeof vectors[0]; ++i) {
        M::FloatQuantization output;
        CHECK(M::quantizeFloat(M::Rational(vectors[i].n, vectors[i].d), M::Rational(1), output));
        CHECK(floatBits(output.value) == vectors[i].bits);
        CHECK(output.absoluteErrorBound >= 0 && output.absoluteErrorBound <= 1);
    }
    M::FloatQuantization output;
    CHECK(M::quantizeFloat(M::Rational(1, 10), M::Rational(1, 671088640), output));
    CHECK(floatBits(output.value) == 0x3DCCCCCD);
    const double tenthError = 1.0 / 671088640.0;
    CHECK(output.absoluteErrorBound >= tenthError);
    CHECK(output.absoluteErrorBound <= std::nextafter(tenthError, std::numeric_limits<double>::infinity()));
    CHECK(M::quantizeFloat(M::Rational(-1, 10), M::Rational(1, 671088640), output));
    CHECK(floatBits(output.value) == 0xBDCCCCCD);
    CHECK(M::quantizeFloat(M::Rational(INT64_MAX), M::Rational(1), output));
    CHECK(output.absoluteErrorBound == 1); // Never subtract two already-rounded doubles.
    CHECK(M::quantizeFloat(M::Rational(INT64_MIN), M::Rational(0), output));
    CHECK(output.absoluteErrorBound == 0);
    CHECK(M::quantizeFloat(M::Rational(1, 2), M::Rational(0), output));
    CHECK(floatBits(output.value) == 0x3F000000 && output.absoluteErrorBound == 0);
    CHECK(M::quantizeFloat(M::Rational(1, UINT64_MAX), M::Rational(1, UINT64_MAX), output));
    CHECK(floatBits(output.value) == 0x1F800000 && output.absoluteErrorBound > 0);
    // Candidate is 2^-64: its denominator exceeds uint64, but the exact error still matters.

    output.value = 71; output.absoluteErrorBound = 3; const Saved<M::FloatQuantization> saved(output);
    CHECK(!M::quantizeFloat(M::Rational(1, 10), M::Rational(1, 671088641), output));
    CHECK(saved.unchanged(output));
    CHECK(!M::quantizeFloat(M::Rational(16777217), M::Rational(0), output)); CHECK(saved.unchanged(output));
    CHECK(!M::quantizeFloat(M::Rational(INT64_MAX), M::Rational(999, 1000), output)); CHECK(saved.unchanged(output));
    CHECK(!M::quantizeFloat(M::Rational(1, UINT64_MAX), M::Rational(0), output)); CHECK(saved.unchanged(output));
    CHECK(!M::quantizeFloat(M::Rational(1, 0), M::Rational(1), output)); CHECK(saved.unchanged(output));
    CHECK(!M::quantizeFloat(M::Rational(1), M::Rational(0, 0), output)); CHECK(saved.unchanged(output));
    CHECK(!M::quantizeFloat(M::Rational(1), M::Rational(-1), output)); CHECK(saved.unchanged(output));
}
} // namespace

int main() {
    parserAndReduction();
    directedDouble();
    dimensionsAndScaleDependencies();
    exactExtremesAndInvalidFacts();
    velocityAndAcceleration();
    approximations();
    integerQuantization();
    floatingQuantization();
}
