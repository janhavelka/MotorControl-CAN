/** @file multidrive_contract_test.cpp
 * @brief Production common-contract fixtures, never additional supported drivers.
 * Source review: docs/audit/PROMPT_46_CONTRAST_SOURCES.md and drive_contrasts.md.
 * N5/EPOS4 configuration and proprietary protocol values remain test-only.
 * SPDX-License-Identifier: MIT
 */
#include <MotorControlCAN/ActionOperation.h>
#include <MotorControlCAN/Axis.h>
#include <MotorControlCAN/Capabilities.h>
#include <MotorControlCAN/Discovery.h>
#include <MotorControlCAN/Native.h>
#include <MotorControlCAN/can/Frame.h>
#include <MotorControlCAN/canopen/Pdo.h>
#include <MotorControlCAN/canopen/Sdo.h>
#include <MotorControlCAN/profiles/lichuan_c/Objects.h>
#include <MotorControlCAN/profiles/lichuan_c/Pdo.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace {
namespace M = MotorControlCAN;
namespace C = MotorControlCAN::CANOPEN;
namespace L = MotorControlCAN::LICHUAN_C;

void check(bool condition, const char* expression, int line) {
    if (!condition) {
        std::fprintf(stderr, "fixture check failed at line %d: %s\n", line, expression);
        std::exit(EXIT_FAILURE);
    }
}
#define CHECK(expression) check(static_cast<bool>(expression), #expression, __LINE__)

template <typename T> struct Saved {
    unsigned char bytes[sizeof(T)];
    explicit Saved(const T& value) { std::memcpy(bytes, &value, sizeof value); }
    bool unchanged(const T& value) const { return std::memcmp(bytes, &value, sizeof value) == 0; }
};

M::Binding binding(uint32_t profile = 17) {
    M::Binding value;
    value.logicalAxisId = 91;
    value.endpoint.bus = 3;
    value.endpoint.profile = profile;
    value.endpoint.address = 5;
    value.bindingGeneration = 4;
    value.configurationGeneration = 8;
    return value;
}
M::UnitScale scale(uint32_t numerator, uint32_t denominator = 1) {
    return M::UnitScale(numerator, denominator, M::ScaleSource::DOCUMENTED);
}
M::AxisConfig axis(uint32_t profile = 17) {
    M::AxisConfig value;
    value.binding = binding(profile);
    value.originKnown = true;
    value.originSource = M::ScaleSource::ASSUMED;
    value.units.commandStepsPerMotorTurn = scale(3600);
    value.units.motorTurnsPerLoadTurn = scale(1);
    return value;
}
M::PositionRequest turns(const M::AxisConfig& configuration, int64_t numerator,
                         uint64_t denominator = 1) {
    M::PositionRequest request;
    request.value = M::Rational(numerator, denominator);
    request.unit = M::PositionUnit::TURNS;
    request.frame = M::CoordinateFrame::LOAD;
    request.configurationGeneration = configuration.binding.configurationGeneration;
    return request;
}

void motorKindsAndOptionalGeometry() {
    // N5 v3.3.0 pp38,297-298: p38 example values for the 3202h bitfield.
    // They are not profile IDs: bit3 is open-loop current reduction; bit6 BLDC;
    // bit0 is closed/open loop. Slow-speed bit7 requires closed loop. No writes.
    const uint32_t submodeExamples[] = {0x08, 0x40};
    const M::MotorKind kinds[] = {M::MotorKind::STEPPER, M::MotorKind::BLDC};
    for (unsigned i = 0; i < 2; ++i) {
        M::AxisConfig configuration = axis();
        M::Capabilities facts;
        facts.motor = kinds[i];
        facts.feedback = M::FeedbackKind::ENCODER;
        facts.position = true;
        facts.velocity = true;
        CHECK(M::requireCapability(facts, M::Capability::POSITION));
        CHECK(configuration.binding.endpoint.profile == 17);
        CHECK(configuration.binding.endpoint.profile != submodeExamples[i]);
        CHECK((submodeExamples[i] & 0x01u) == 0); // Both documented examples select open loop.
        CHECK(((submodeExamples[i] & 0x40u) != 0) == (i == 1));
        CHECK(((submodeExamples[i] & 0x08u) != 0) == (i == 0));
        CHECK(configuration.units.fullStepsPerMotorTurn.numerator == 0);
        const Saved<M::AxisConfig> original(configuration);
        M::PreparedTarget target;
        // N5 p64: 60A8=FF410000, one turn=3600 only with explicitly selected 1:1 gearing.
        CHECK(M::preparePosition(turns(configuration, 1), configuration, nullptr, target));
        CHECK(target.endpointNative == 3600);
        CHECK(original.unchanged(configuration));
        // Changing motor/feedback facts does not invent a command scale or mode.
        facts.feedback = M::FeedbackKind::NONE;
        CHECK(M::requireCapability(facts, M::Capability::POSITION));
        CHECK(M::preparePosition(turns(configuration, 1), configuration, nullptr, target));
        CHECK(target.endpointNative == 3600);
        // C1 uses the production coordinate API, with application-supplied
        // origin/reference facts. No N5 profile or native sequence is implied.
        M::AxisConfig angular;
        angular.binding = configuration.binding;
        angular.units = configuration.units;
        angular.supportedRelativeBases = 1; // Explicit ACTUAL-basis policy.
        angular.originKnown = true;
        angular.originNative = 17;
        angular.originSource = M::ScaleSource::ASSUMED;
        M::AxisReference reference;
        reference.binding = angular.binding;
        reference.referenceGeneration = angular.referenceGeneration;
        reference.nativeKnown = true;
        reference.nativePosition = 3517; // 350 degrees beyond the host origin.
        reference.source = M::ScaleSource::ASSUMED;
        reference.observedUs = 100;
        reference.nowUs = 110;
        reference.maximumAgeUs = 20;
        M::PositionRequest angle;
        angle.value = M::Rational(10);
        angle.unit = M::PositionUnit::DEGREES;
        angle.frame = M::CoordinateFrame::LOAD;
        angle.wrapped = true;
        angle.path = M::AnglePath::SHORTEST;
        angle.tie = M::HalfTurnTie::REJECT;
        angle.configurationGeneration = angular.binding.configurationGeneration;
        const Saved<M::AxisConfig> originalAngular(angular);
        const Saved<M::AxisReference> originalReference(reference);
        M::PreparedTarget prepared;
        CHECK(M::preparePosition(angle, angular, &reference, prepared));
        CHECK(prepared.endpointNative == 3717); // Positive wrap: +20 degrees.
        CHECK(prepared.requestedNative.integral == 3717 && prepared.requestedNative.numerator == 0);
        CHECK(prepared.requested.path == M::AnglePath::SHORTEST);
        CHECK(prepared.requested.frame == M::CoordinateFrame::LOAD);
        CHECK(prepared.referenceUsed && prepared.retainedReference.nativePosition == 3517);
        CHECK(prepared.quantization.errorNumerator == 0 && prepared.exactArithmetic);
        CHECK(M::validatePreparedTarget(prepared, angular, &reference, 115));
        angle.frame = M::CoordinateFrame::MOTOR;
        CHECK(M::preparePosition(angle, angular, &reference, prepared));
        CHECK(prepared.endpointNative == 3717); // Frames coincide only at 1:1.
        CHECK(originalAngular.unchanged(angular));
        CHECK(originalReference.unchanged(reference));

        angular.softLimitsKnown = true;
        angular.softMinimum = 17;
        angular.softMaximum = 3617;
        angle.path = M::AnglePath::POSITIVE;
        const Saved<M::PreparedTarget> selectedPath(prepared);
        CHECK(!M::preparePosition(angle, angular, &reference, prepared));
        CHECK(selectedPath.unchanged(prepared));
        angle.path = M::AnglePath::NEGATIVE;
        CHECK(M::preparePosition(angle, angular, &reference, prepared));
        CHECK(prepared.endpointNative == 117); // Explicit -340-degree path.
    }

    // EPOS4 rel13740 pp221,234: selected 8192 command increments/turn is abstract,
    // not a factory default. A servo needs no full-step geometry.
    M::AxisConfig servo = axis(23);
    servo.units.commandStepsPerMotorTurn = scale(8192);
    M::PreparedTarget target;
    CHECK(M::preparePosition(turns(servo, 3, 2), servo, nullptr, target));
    CHECK(target.endpointNative == 12288);
    CHECK(servo.units.fullStepsPerMotorTurn.numerator == 0);
    // Abstract servo C1: independent feedback, gearing and reversed command
    // polarity do not create full-step geometry or change angular intent.
    M::AxisConfig angular;
    angular.binding = servo.binding;
    angular.units = servo.units;
    angular.units.motorTurnsPerLoadTurn = scale(3, 2);
    angular.units.encoder.countsPerUnit = scale(2048);
    angular.units.encoder.countsPerUnit.source = M::ScaleSource::READBACK;
    angular.units.encoder.sourceId = 1;
    angular.units.commandPolarity = -1;
    angular.supportedRelativeBases = 1;
    angular.originKnown = true;
    angular.originNative = 17;
    angular.originSource = M::ScaleSource::ASSUMED;
    M::AxisReference reference;
    reference.binding = angular.binding;
    reference.referenceGeneration = angular.referenceGeneration;
    reference.nativeKnown = true;
    reference.nativePosition = 17;
    reference.source = M::ScaleSource::ASSUMED;
    reference.observedUs = 100;
    reference.nowUs = 110;
    reference.maximumAgeUs = 20;
    M::PositionRequest angle;
    angle.value = M::Rational(1, 10);
    angle.unit = M::PositionUnit::TURNS;
    angle.frame = M::CoordinateFrame::MOTOR;
    angle.wrapped = true;
    angle.path = M::AnglePath::POSITIVE;
    angle.tie = M::HalfTurnTie::REJECT;
    angle.rounding = M::Rounding::NEAREST;
    angle.maximumQuantizationError = M::Rational(1, 5);
    angle.configurationGeneration = angular.binding.configurationGeneration;
    M::PreparedTarget prepared;
    CHECK(M::preparePosition(angle, angular, &reference, prepared));
    CHECK(prepared.endpointNative == -802); // 17 - 819.2, rounded once.
    CHECK(prepared.requestedNative.integral == -802 && prepared.requestedNative.numerator == 1 &&
          prepared.requestedNative.denominator == 5 && prepared.requestedNative.negative);
    CHECK(prepared.quantization.errorNumerator == 1 &&
          prepared.quantization.errorDenominator == 5 && !prepared.quantization.errorNegative);
    CHECK(prepared.requested.path == M::AnglePath::POSITIVE && prepared.exactArithmetic);
    CHECK(angular.units.fullStepsPerMotorTurn.numerator == 0);
    CHECK(angular.units.encoder.countsPerUnit.numerator == 2048);
    angular.units.encoder.countsPerUnit = M::UnitScale();
    CHECK(M::preparePosition(angle, angular, &reference, prepared));
    CHECK(prepared.endpointNative == -802); // Command conversion needs no feedback scale.
    angle.frame = M::CoordinateFrame::LOAD;
    CHECK(M::preparePosition(angle, angular, &reference, prepared));
    CHECK(prepared.endpointNative == -1212); // 17 - (819.2 * 3/2).
    CHECK(prepared.quantization.errorNumerator == 1 &&
          prepared.quantization.errorDenominator == 5 && prepared.quantization.errorNegative);
    angle.maximumQuantizationError = M::Rational(1, 6);
    const Saved<M::PreparedTarget> quantized(prepared);
    CHECK(!M::preparePosition(angle, angular, &reference, prepared));
    CHECK(quantized.unchanged(prepared));
}

void independentScalesAndWideIntent() {
    // Abstract C1b: 1/2 load turn * 3/2 motor/load * 4096 = 3072 commands,
    // independently 1536 feedback counts at 2048 counts/motor turn.
    M::AxisConfig configuration = axis();
    configuration.units.commandStepsPerMotorTurn = scale(4096);
    configuration.units.motorTurnsPerLoadTurn = scale(3, 2);
    configuration.units.encoder.countsPerUnit = scale(2048);
    configuration.units.encoder.sourceId = 1;
    configuration.units.encoder.countsPerUnit.source = M::ScaleSource::READBACK;
    M::PreparedTarget target;
    CHECK(M::preparePosition(turns(configuration, 1, 2), configuration, nullptr, target));
    CHECK(target.endpointNative == 3072);
    M::Rational feedback;
    CHECK(M::convertDisplacement(M::Rational(1, 2), M::PositionUnit::TURNS,
        M::PositionUnit::ENCODER_COUNTS, configuration.units, feedback));
    CHECK(feedback.numerator == 1536 && feedback.denominator == 1);
    configuration.units.encoder.countsPerUnit = M::UnitScale();
    CHECK(M::preparePosition(turns(configuration, 1, 2), configuration, nullptr, target));
    CHECK(target.endpointNative == 3072);

    // C2: N5 p412 / EPOS4 p256 / Lichuan EDS 60FF are I32 storage.
    // Exact signed rates use production velocity preparation, not a position surrogate.
    configuration.nativeRateBasis = M::NativeRateBasis::MOTOR_TURNS;
    configuration.nativeVelocityTime = M::TimeUnit::MINUTE;
    M::VelocityRequest request;
    request.unit = M::VelocityUnit(M::PositionUnit::TURNS, M::TimeUnit::MINUTE);
    request.frame = M::CoordinateFrame::MOTOR;
    request.configurationGeneration = configuration.binding.configurationGeneration;
    for (int sign = -1; sign <= 1; sign += 2) {
        request.value = M::Rational(sign * 70000);
        M::PreparedVelocityTarget prepared;
        CHECK(M::prepareVelocityTarget(request, configuration, prepared));
        CHECK(prepared.effectiveNative == sign * 70000);
        CHECK(M::checkNativeInteger(prepared.effectiveNative, 4, true));
        // N5 p349 6042 is I16: width rejection, not an operating-rate claim.
        CHECK(M::checkNativeInteger(prepared.effectiveNative, 2, true).code == M::Err::ILLEGAL_VALUE);
    }
    CHECK(M::checkNativeInteger(-32768, 2, true));
    CHECK(M::checkNativeInteger(32767, 2, true));
    CHECK(!M::checkNativeInteger(-32769, 2, true));
    CHECK(!M::checkNativeInteger(32768, 2, true));
    CHECK(!M::checkNativeInteger(-1, 2, false));
    CHECK(M::checkNativeInteger(65535, 2, false));
    CHECK(!M::checkNativeInteger(65536, 2, false));
    CHECK(M::checkNativeInteger(INT64_MIN, 8, true));
    CHECK(M::checkNativeInteger(INT64_MAX, 8, true));
    CHECK(!M::checkNativeInteger(0, 3, true));
    // Independently stated scalar limits: signedness never becomes a rate capability.
    CHECK(M::checkNativeInteger(-128, 1, true));
    CHECK(M::checkNativeInteger(127, 1, true));
    CHECK(!M::checkNativeInteger(-129, 1, true));
    CHECK(!M::checkNativeInteger(128, 1, true));
    CHECK(M::checkNativeInteger(255, 1, false));
    CHECK(!M::checkNativeInteger(256, 1, false));
    CHECK(!M::checkNativeInteger(-1, 1, false));
    CHECK(M::checkNativeInteger(INT32_MIN, 4, true));
    CHECK(M::checkNativeInteger(INT32_MAX, 4, true));
    CHECK(!M::checkNativeInteger(int64_t(INT32_MIN) - 1, 4, true));
    CHECK(!M::checkNativeInteger(int64_t(INT32_MAX) + 1, 4, true));
    CHECK(M::checkNativeInteger(UINT32_MAX, 4, false));
    CHECK(!M::checkNativeInteger(int64_t(UINT32_MAX) + 1, 4, false));
    CHECK(!M::checkNativeInteger(-1, 4, false));
    CHECK(M::checkNativeInteger(0, 8, false));
    CHECK(M::checkNativeInteger(INT64_MAX, 8, false)); // Input domain is explicitly int64.
    CHECK(!M::checkNativeInteger(-1, 8, false));
    CHECK(!M::checkNativeInteger(INT64_MIN, 8, false));
    CHECK(!M::checkNativeInteger(0, 0, true));
    CHECK(!M::checkNativeInteger(0, 255, true));
}

void productionNumericMigration() {
    // Retain the original prototype-migration regressions through the actual Axis API.
    M::AxisConfig configuration = axis();
    configuration.nativeMinimum = INT32_MIN;
    configuration.nativeMaximum = INT32_MAX;
    configuration.units.commandStepsPerMotorTurn = scale(1);
    M::PreparedTarget output;
    CHECK(M::preparePosition(turns(configuration, 71), configuration, nullptr, output));
    const Saved<M::PreparedTarget> original(output);
    M::PositionRequest request = turns(configuration, INT64_MAX);
    request.frame = M::CoordinateFrame::MOTOR;
    configuration.originNative = 1;
    M::Status status = M::preparePosition(request, configuration, nullptr, output);
    CHECK(status.code == M::Err::ILLEGAL_VALUE);
    CHECK(status.detail == 100 + static_cast<int32_t>(M::UnitError::ARITHMETIC_OVERFLOW));
    CHECK(original.unchanged(output));
    request.value = M::Rational(INT64_MIN); configuration.originNative = -1;
    status = M::preparePosition(request, configuration, nullptr, output);
    CHECK(status.code == M::Err::ILLEGAL_VALUE);
    CHECK(status.detail == 100 + static_cast<int32_t>(M::UnitError::ARITHMETIC_OVERFLOW));
    CHECK(original.unchanged(output));
    request.value = M::Rational(static_cast<int64_t>(INT32_MAX) + 1);
    CHECK(M::preparePosition(request, configuration, nullptr, output));
    CHECK(output.endpointNative == INT32_MAX);
    request.value = M::Rational(static_cast<int64_t>(INT32_MIN) - 1);
    configuration.originNative = 1;
    CHECK(M::preparePosition(request, configuration, nullptr, output));
    CHECK(output.endpointNative == INT32_MIN);

    M::PositionRequest native;
    native.configurationGeneration = configuration.binding.configurationGeneration;
    native.value = M::Rational(1, 2);
    const Saved<M::PreparedTarget> boundary(output);
    status = M::preparePosition(native, configuration, nullptr, output);
    CHECK(status.code == M::Err::ILLEGAL_VALUE);
    CHECK(status.detail == 100 + static_cast<int32_t>(M::UnitError::FRACTIONAL_TARGET));
    CHECK(boundary.unchanged(output));
    native.value = M::Rational(1, 0);
    CHECK(!M::preparePosition(native, configuration, nullptr, output));
    CHECK(boundary.unchanged(output));
    native.value = M::Rational(1);
    configuration.units.fullStepsPerMotorTurn = scale(1, 0);
    status = M::preparePosition(native, configuration, nullptr, output);
    CHECK(status.code == M::Err::INVALID_CONFIG);
    CHECK(status.detail == 100 + static_cast<int32_t>(M::UnitError::INVALID_SCALE));
    CHECK(boundary.unchanged(output));
    configuration.units.fullStepsPerMotorTurn = M::UnitScale();
    configuration.units.motorTurnsPerLoadTurn = scale(1, 0);
    CHECK(!M::preparePosition(request, configuration, nullptr, output));
    CHECK(boundary.unchanged(output));
    configuration.originNative = 0;
    configuration.units.motorTurnsPerLoadTurn = scale(1);
    configuration.units.commandStepsPerMotorTurn = scale(2000000, 2000000);
    request.value = M::Rational(2000000, 2000000);
    CHECK(M::preparePosition(request, configuration, nullptr, output));
    CHECK(output.endpointNative == 1);
    native.value = M::Rational(INT32_MAX);
    CHECK(M::preparePosition(native, configuration, nullptr, output));
    CHECK(output.endpointNative == INT32_MAX);
}

void nativeFloatQuantization() {
    // C3: moteus float32 representation, not an executable moteus profile.
    M::FloatQuantization quantized;
    CHECK(M::quantizeFloat(M::Rational(1, 10), M::Rational(1, 671088640), quantized));
    uint32_t bits = 0;
    std::memcpy(&bits, &quantized.value, sizeof bits);
    CHECK(bits == 0x3dcccccdu); // Independent LE wire vector: cd cc cc 3d.
    CHECK(quantized.absoluteErrorBound >= 1.0 / 671088640.0);
    CHECK(quantized.absoluteErrorBound < 1.5e-9);
    const Saved<M::FloatQuantization> saved(quantized);
    CHECK(!M::quantizeFloat(M::Rational(1, 10), M::Rational(0), quantized));
    CHECK(saved.unchanged(quantized));
    M::AxisConfig configuration = axis(31);
    configuration.positionRepresentation = M::NativeRepresentation::BINARY32;
    M::PositionRequest request;
    request.configurationGeneration = configuration.binding.configurationGeneration;
    request.value = M::Rational(1, 10);
    request.maximumQuantizationError = M::Rational(1, 671088640);
    M::PreparedTarget target;
    CHECK(M::preparePosition(request, configuration, nullptr, target));
    CHECK(target.effectiveFloat == quantized.value);
}

void protocolNeutralBinding() {
    // C4 is an abstract multi-axis route, not a driver or mixed-protocol bus claim.
    M::Binding route = binding();
    route.endpoint.protocol = M::Protocol::PROPRIETARY;
    route.endpoint.address = 500;
    route.endpoint.channel = 9;
    route.endpoint.subaxis = 2;
    CHECK(M::validateBinding(route));
    M::Binding second = route;
    ++second.logicalAxisId;
    CHECK(!M::sameBinding(route, second));
    second = route; ++second.endpoint.subaxis;
    CHECK(!M::sameBinding(route, second));
    second = route; ++second.endpoint.channel;
    CHECK(!M::sameBinding(route, second));
    C::SdoChannel channel;
    channel.binding = route;
    channel.clientToServerId = 0x605;
    channel.serverToClientId = 0x585;
    CHECK(!C::validateSdoChannel(channel));
    M::AxisConfig configuration = axis();
    configuration.binding = route;
    M::PreparedTarget target;
    CHECK(M::preparePosition(turns(configuration, 1), configuration, nullptr, target));
    CHECK(target.endpointNative == 3600); // No NMT/statusword prerequisite for host arithmetic.
}

C::SdoExpectation canopenExpectation() {
    C::SdoExpectation expected;
    expected.channel.binding = binding();
    expected.channel.clientToServerId = 0x605;
    expected.channel.serverToClientId = 0x585;
    expected.index = 0x60ff;
    expected.width = 4;
    expected.direction = C::SdoDirection::DOWNLOAD;
    return expected;
}
// Test-only selected VESC route predicate. It is deliberately not an installed decoder.
bool vescRoute(const M::Frame& frame) {
    return M::validateFrame(frame) && frame.extended && !frame.rtr && frame.id == 0x117 &&
        frame.payloadLength == 4;
}
void vescExtendedData() {
    // C5: pinned VESC example, SET_CURRENT, ID23, signed BE scale1000.
    M::Frame frame;
    frame.extended = true; frame.id = 0x117;
    frame.dlc = frame.payloadLength = 4;
    const uint8_t expected[] = {0, 0, 0xc7, 0x38};
    std::memcpy(frame.data, expected, sizeof expected);
    CHECK(M::validateFrame(frame) && vescRoute(frame));
    CHECK(C::checkRequest(frame, canopenExpectation()).code == M::Err::UNSUPPORTED);
    CHECK(std::memcmp(frame.data, expected, sizeof expected) == 0);
    frame.extended = false;
    CHECK(M::validateFrame(frame)); // Common representation is not protocol routing policy.
    CHECK(!vescRoute(frame));
}
void odriveRemoteRequest() {
    // C6: ODrive0.6.12 command09, illustrative node5. RTR has no payload.
    M::Frame request;
    request.id = (5u << 5) | 0x09u;
    request.rtr = true; request.dlc = 8;
    CHECK(M::validateFrame(request));
    CHECK(C::checkRequest(request, canopenExpectation()).code == M::Err::UNSUPPORTED);
    request.payloadLength = 1;
    CHECK(M::validateFrame(request).code == M::Err::FRAME_ERROR);
    request.payloadLength = 0;
    request.rtr = false; request.dlc = 0;
    CHECK(M::validateFrame(request)); // Documented empty-data alternative, collision risk remains.
    CHECK(!C::checkRequest(request, canopenExpectation()));
    M::Frame response = request;
    response.dlc = response.payloadLength = 8;
    // Documentary response fields: 1.0 revolution, 2.0 revolutions/second, LE float32.
    const uint8_t bytes[] = {0, 0, 0x80, 0x3f, 0, 0, 0, 0x40};
    std::memcpy(response.data, bytes, sizeof bytes);
    CHECK(M::validateFrame(response));
    CHECK(!C::checkRequest(response, canopenExpectation()));
    CHECK(!vescRoute(response));
    // Only this explicitly selected fixture handler receives the response; no probing.
    unsigned handled = 0;
    if (!response.extended && !response.rtr && response.id == request.id && response.payloadLength == 8)
        ++handled;
    CHECK(handled == 1);
}
void moteusFdRequirement() {
    // C7: the documented 16-byte request is not representable in Frame::data[8].
    // Only the requirement flag is tested; no payload is copied or truncated.
    M::Frame requirement;
    requirement.id = 0x8001; requirement.extended = true; requirement.fd = true;
    CHECK(sizeof requirement.data == 8);
    CHECK(M::validateFrame(requirement).code == M::Err::UNSUPPORTED);
    CHECK(C::checkRequest(requirement, canopenExpectation()).code == M::Err::UNSUPPORTED);
    CHECK(!vescRoute(requirement));
}

void rampDimensionsAndCapabilityGuards() {
    // C10: LC EN86p11 time; N5p65 speed-unit/second with6097=1/1;
    // EPOS4pp230,236 rpm/s. Values are selected fixture inputs, never writes.
    M::NativeRamp lichuan;
    lichuan.kind = M::RampKind::TIME_MS;
    lichuan.value = M::Rational(100);
    M::NativeRamp epos;
    epos.kind = M::RampKind::ACCELERATION;
    epos.value = M::Rational(100);
    epos.accelerationUnit = M::AccelerationUnit(M::PositionUnit::TURNS,
        M::TimeUnit::MINUTE, M::TimeUnit::SECOND);
    M::UnitConfig units;
    units.commandStepsPerMotorTurn = scale(3600);
    units.motorTurnsPerLoadTurn = scale(1);
    const M::AccelerationUnit degreesPerSecondSquared(M::PositionUnit::DEGREES,
        M::TimeUnit::SECOND, M::TimeUnit::SECOND);
    M::Rational converted;
    CHECK(M::convertAcceleration(epos.value, epos.accelerationUnit,
        degreesPerSecondSquared, units, converted));
    CHECK(converted.numerator == 600 && converted.denominator == 1);
    M::NativeRamp n5;
    CHECK(M::convertNativeRamp(epos, M::RampKind::ACCELERATION,
        degreesPerSecondSquared, units, n5));
    CHECK(n5.value.numerator == 600 && n5.value.denominator == 1);
    // N5p65 defines6097 numerator01/denominator02. For selected3/2 and raw4,
    // the explicitly normalized quantity is6 rpm/s, hence36 degrees/s^2.
    // This is an independent fixture normalization, not a raw6097 decoder.
    const int64_t rawAcceleration = 4;
    const int64_t factorNumerator = 3;
    const uint64_t factorDenominator = 2;
    M::NativeRamp scaledN5 = epos;
    scaledN5.value = M::Rational(rawAcceleration * factorNumerator, factorDenominator);
    CHECK(M::convertAcceleration(scaledN5.value, scaledN5.accelerationUnit,
        degreesPerSecondSquared, units, converted));
    CHECK(converted.numerator == 36 && converted.denominator == 1);
    M::NativeRamp scaledOutput;
    CHECK(M::convertNativeRamp(scaledN5, M::RampKind::ACCELERATION,
        degreesPerSecondSquared, units, scaledOutput));
    CHECK(scaledOutput.value.numerator == 36 && scaledOutput.value.denominator == 1);
    const Saved<M::NativeRamp> saved(n5);
    CHECK(M::convertNativeRamp(lichuan, M::RampKind::ACCELERATION,
        degreesPerSecondSquared, units, n5).code == M::Err::UNSUPPORTED);
    CHECK(saved.unchanged(n5));
    CHECK(M::convertNativeRamp(epos, M::RampKind::TIME_MS,
        degreesPerSecondSquared, units, n5).code == M::Err::UNSUPPORTED);
    CHECK(saved.unchanged(n5));
    CHECK(M::convertNativeRamp(lichuan, M::RampKind::TIME_MS,
        degreesPerSecondSquared, units, n5));
    CHECK(n5.value.numerator == 100 && n5.kind == M::RampKind::TIME_MS);
    const Saved<M::NativeRamp> duration(n5);
    lichuan.value = M::Rational(-1);
    CHECK(!M::convertNativeRamp(lichuan, M::RampKind::TIME_MS,
        degreesPerSecondSquared, units, n5));
    CHECK(duration.unchanged(n5));
    lichuan.value = M::Rational(1, 0);
    CHECK(!M::convertNativeRamp(lichuan, M::RampKind::TIME_MS,
        degreesPerSecondSquared, units, n5));
    CHECK(duration.unchanged(n5));
    lichuan.value = M::Rational(1);
    lichuan.kind = static_cast<M::RampKind>(255);
    CHECK(!M::convertNativeRamp(lichuan, M::RampKind::TIME_MS,
        degreesPerSecondSquared, units, n5));
    CHECK(duration.unchanged(n5));
    CHECK(!M::convertNativeRamp(epos, static_cast<M::RampKind>(255),
        degreesPerSecondSquared, units, n5));
    CHECK(duration.unchanged(n5));
    epos.value = M::Rational(INT64_MAX);
    CHECK(!M::convertNativeRamp(epos, M::RampKind::ACCELERATION,
        degreesPerSecondSquared, units, n5)); // rpm/s -> degree/s^2 multiplies by6.
    CHECK(duration.unchanged(n5));
    epos.value = M::Rational(100);
    const Saved<M::NativeRamp> originalAcceleration(epos);
    CHECK(!M::convertNativeRamp(epos, M::RampKind::ACCELERATION,
        degreesPerSecondSquared, units, epos)); // Documented input/output alias refusal.
    CHECK(originalAcceleration.unchanged(epos));
    M::AccelerationUnit invalidUnit = degreesPerSecondSquared;
    invalidUnit.accelerationTime = static_cast<M::TimeUnit>(255);
    CHECK(!M::convertNativeRamp(epos, M::RampKind::ACCELERATION, invalidUnit, units, n5));
    CHECK(duration.unchanged(n5));

    // C11 abstract variants. Holding brakes are not VESC/moteus electrical braking.
    M::Capabilities variantA;
    variantA.motor = M::MotorKind::BLDC;
    variantA.feedback = M::FeedbackKind::HALL;
    variantA.position = true;
    const Saved<M::Capabilities> beforeA(variantA);
    CHECK(M::requireCapability(variantA, M::Capability::POSITION));
    CHECK(M::requireCapability(variantA, M::Capability::TORQUE).code == M::Err::UNSUPPORTED);
    CHECK(M::requireCapability(variantA, M::Capability::HOLDING_BRAKE).code == M::Err::UNSUPPORTED);
    CHECK(beforeA.unchanged(variantA));
    M::Capabilities variantB = variantA;
    variantB.position = false; variantB.torque = true; variantB.holdingBrake = true;
    variantB.calibrationRequired = true;
    const Saved<M::Capabilities> beforeB(variantB);
    CHECK(M::requireCapability(variantB, M::Capability::POSITION).code == M::Err::UNSUPPORTED);
    CHECK(M::requireCapability(variantB, M::Capability::TORQUE).code == M::Err::INVALID_CONFIG);
    CHECK(!M::requireCapability(variantB, M::Capability::HOLDING_BRAKE));
    CHECK(beforeB.unchanged(variantB));
    variantB.calibrationQualified = true;
    CHECK(M::requireCapability(variantB, M::Capability::TORQUE));
    CHECK(M::requireCapability(variantB, M::Capability::HOLDING_BRAKE));
    CHECK(!M::requireCapability(variantB, static_cast<M::Capability>(255)));
    variantB.motor = static_cast<M::MotorKind>(255);
    CHECK(!M::requireCapability(variantB, M::Capability::TORQUE));
    variantB.motor = M::MotorKind::UNKNOWN;
    variantB.feedback = static_cast<M::FeedbackKind>(255);
    CHECK(!M::requireCapability(variantB, M::Capability::TORQUE));
}

void nativeEvidenceWithoutTransmissionConfirmation() {
    // A qualified native reply need not wait for a controller TX confirmation callback.
    M::ServiceEvidence evidence;
    evidence.admitted = true;
    evidence.transmissionPossible = true;
    evidence.acknowledgement = M::AcknowledgementSupport::RESPONSE;
    evidence.acknowledged = true;
    M::ServiceOutcome outcome;
    CHECK(M::checkServiceEvidence(evidence, outcome));
    CHECK(outcome.execution == M::ActionExecution::ACKNOWLEDGED);
    CHECK(outcome.completion == M::ActionCompletion::NOT_OBSERVED);
    CHECK(!evidence.transmissionConfirmed);
    evidence.completionObserved = true;
    CHECK(M::checkServiceEvidence(evidence, outcome));
    CHECK(outcome.execution == M::ActionExecution::ACKNOWLEDGED);
    CHECK(outcome.completion == M::ActionCompletion::OBSERVED);
    CHECK(!evidence.transmissionConfirmed);

    // A no-ack command may also have a separately qualified completion report.
    evidence.acknowledgement = M::AcknowledgementSupport::NONE;
    evidence.acknowledged = false;
    CHECK(M::checkServiceEvidence(evidence, outcome));
    CHECK(outcome.execution == M::ActionExecution::UNKNOWN);
    CHECK(outcome.completion == M::ActionCompletion::OBSERVED);
    CHECK(!evidence.transmissionConfirmed);

    // Native evidence still cannot coexist with a claim that TX was impossible.
    const Saved<M::ServiceOutcome> retained(outcome);
    M::ServiceEvidence inconsistent;
    inconsistent.admitted = true;
    inconsistent.acknowledgement = M::AcknowledgementSupport::RESPONSE;
    inconsistent.acknowledged = true;
    CHECK(!M::checkServiceEvidence(inconsistent, outcome));
    CHECK(retained.unchanged(outcome));
    inconsistent.acknowledged = false;
    inconsistent.completionObserved = true;
    CHECK(!M::checkServiceEvidence(inconsistent, outcome));
    CHECK(retained.unchanged(outcome));
}

void absentAcknowledgementAndTrajectoryDone() {
    // C8 abstract setter. Native trajectory-done is backed by moteus register00b;
    // an explicit controller report can coexist with nonzero final velocity.
    M::ServiceEvidence evidence;
    M::ServiceOutcome outcome;
    CHECK(M::checkServiceEvidence(evidence, outcome));
    CHECK(outcome.execution == M::ActionExecution::NOT_TRANSMITTED);
    evidence.admitted = true;
    CHECK(M::checkServiceEvidence(evidence, outcome));
    CHECK(outcome.execution == M::ActionExecution::NOT_TRANSMITTED);
    evidence.transmissionPossible = evidence.transmissionConfirmed = true;
    CHECK(M::checkServiceEvidence(evidence, outcome)); // Unknown native acknowledgement contract.
    CHECK(outcome.execution == M::ActionExecution::UNKNOWN);
    CHECK(outcome.completion == M::ActionCompletion::NOT_OBSERVED);
    evidence.acknowledgement = M::AcknowledgementSupport::NONE;
    CHECK(M::checkServiceEvidence(evidence, outcome));
    CHECK(outcome.execution == M::ActionExecution::UNKNOWN);
    CHECK(outcome.completion == M::ActionCompletion::NOT_OBSERVED);
    evidence.completionObserved = true;
    const M::Rational terminalVelocity(2); // Explicit abstract two motor turns/second.
    const bool nativeStateKnown = false; // A protocol need not expose an axis state.
    CHECK(M::checkServiceEvidence(evidence, outcome));
    CHECK(outcome.execution == M::ActionExecution::UNKNOWN);
    CHECK(outcome.completion == M::ActionCompletion::OBSERVED);
    CHECK(terminalVelocity.numerator != 0 && !nativeStateKnown);
    evidence.completionObserved = false;
    evidence.acknowledgement = M::AcknowledgementSupport::RESPONSE;
    CHECK(M::checkServiceEvidence(evidence, outcome)); // Lost response is still unknown.
    CHECK(outcome.execution == M::ActionExecution::UNKNOWN);
    evidence.acknowledged = true;
    CHECK(M::checkServiceEvidence(evidence, outcome));
    CHECK(outcome.execution == M::ActionExecution::ACKNOWLEDGED);
    CHECK(outcome.completion == M::ActionCompletion::NOT_OBSERVED);
    const Saved<M::ServiceOutcome> saved(outcome);
    evidence.acknowledgement = M::AcknowledgementSupport::NONE;
    CHECK(!M::checkServiceEvidence(evidence, outcome));
    CHECK(saved.unchanged(outcome));
    evidence.acknowledgement = M::AcknowledgementSupport::UNKNOWN;
    CHECK(!M::checkServiceEvidence(evidence, outcome));
    CHECK(saved.unchanged(outcome));
    evidence.acknowledgement = M::AcknowledgementSupport::RESPONSE;
    evidence.rejected = true;
    CHECK(!M::checkServiceEvidence(evidence, outcome)); // Cannot both acknowledge and reject.
    CHECK(saved.unchanged(outcome));
    evidence.acknowledged = false;
    CHECK(M::checkServiceEvidence(evidence, outcome));
    CHECK(outcome.execution == M::ActionExecution::REJECTED);
    CHECK(outcome.completion == M::ActionCompletion::NOT_OBSERVED);
    const Saved<M::ServiceOutcome> rejection(outcome);
    evidence.completionObserved = true;
    CHECK(!M::checkServiceEvidence(evidence, outcome)); // Same command cannot be rejected and done.
    CHECK(rejection.unchanged(outcome));
    evidence.completionObserved = evidence.rejected = false;
    evidence.acknowledgement = static_cast<M::AcknowledgementSupport>(255);
    CHECK(!M::checkServiceEvidence(evidence, outcome));
    CHECK(rejection.unchanged(outcome));
    evidence.acknowledgement = M::AcknowledgementSupport::UNKNOWN;
    evidence.admitted = false;
    CHECK(!M::checkServiceEvidence(evidence, outcome));
    CHECK(rejection.unchanged(outcome));
    evidence.admitted = true; evidence.transmissionPossible = false;
    CHECK(!M::checkServiceEvidence(evidence, outcome)); // Confirmed cannot contradict possible.
    CHECK(rejection.unchanged(outcome));
    evidence.transmissionConfirmed = false; evidence.completionObserved = true;
    CHECK(!M::checkServiceEvidence(evidence, outcome));
    CHECK(rejection.unchanged(outcome));
}

void profileScopedObjectsAndCapacities() {
    // C9 documentary fixtures, excluded from Discovery and installation.
    // N5p283: NanoJ2300 U32; N5p127: eight PDO sets. LC EDS: four sets,
    // 2300 U16 parameter action; no1010/1011 signatures. EP rel13740 has no2300.
    CHECK(L::OBJECT_2300_00.type == L::ScalarType::U16);
    CHECK(L::OBJECT_2300_00.widthBytes == 2);
    L::ReadDescriptor read;
    CHECK(L::readDescriptor(0x2300, 0, read));
    CHECK(read.widthBytes == 2 && read.representation == L::ReadRepresentation::UNSIGNED);
    const Saved<L::ReadDescriptor> savedRead(read);
    CHECK(!L::readDescriptor(0x1010, 1, read));
    CHECK(savedRead.unchanged(read));
    CHECK(!L::readDescriptor(0x1011, 1, read));
    CHECK(savedRead.unchanged(read));
    const C::PdoObject n5Objects[] = {
        C::PdoObject(0x2300, 0, 4, C::PdoSign::UNSIGNED, C::PdoRole::RAW, true, false),
        C::PdoObject(0x60ff, 0, 4, C::PdoSign::SIGNED, C::PdoRole::COMMANDED, true, false)
    };
    C::PdoProfile n5;
    n5.profile = 17; n5.sets = 8; n5.slots = 8;
    n5.objects = n5Objects; n5.objectCount = 2;
    C::PdoMap map;
    map.direction = C::PdoDirection::RPDO; map.number = 8; map.count = 1;
    map.words[0] = 0x23000020;
    C::PdoLayout layout;
    CHECK(C::validatePdoMap(map, n5, layout));
    CHECK(layout.fields[0].widthBytes == 4 && layout.profile == 17);
    const Saved<C::PdoLayout> saved(layout);
    CHECK(!C::validatePdoMap(map, L::pdoProfile(41), layout));
    CHECK(saved.unchanged(layout));
    map.number = 1; map.words[0] = 0x23000010;
    CHECK(!C::validatePdoMap(map, n5, layout)); // Same index cannot substitute the LC width.
    CHECK(saved.unchanged(layout));
    map.words[0] = 0x23000020;
    C::PdoProfile epos = n5;
    epos.profile = 23; epos.sets = 4;
    epos.objects = n5Objects + 1; epos.objectCount = 1; // Reviewed60FF only; no2300.
    CHECK(!C::validatePdoMap(map, epos, layout));
    CHECK(saved.unchanged(layout));
    C::PdoRoute route;
    route.binding = binding(23); route.generation = 1; route.cobId = 0x205;
    route.layout = layout; // N5 layout cannot be relabelled as EPOS4 by changing binding.
    CHECK(!C::validatePdoRoute(route));
    route.binding = binding(17);
    CHECK(C::validatePdoRoute(route));
    map.number = 8; map.words[0] = 0x60ff0020;
    CHECK(C::validatePdoMap(map, n5, layout));
    CHECK(!C::validatePdoMap(map, L::pdoProfile(41), layout));

    // Explicit documentary storage signatures; never generic dispatch by index.
    const uint32_t saveSignature = 0x65766173u; // N5p172, EPp71.
    const uint32_t restoreSignature = 0x64616f6cu; // N5p176, EPp72.
    CHECK(!M::checkNativeInteger(saveSignature, L::OBJECT_2300_00.widthBytes, false));
    CHECK(!M::checkNativeInteger(restoreSignature, L::OBJECT_2300_00.widthBytes, false));
    CHECK(M::discoveryProfileCount() == 1);
    M::DiscoveryProfile installed;
    CHECK(M::getDiscoveryProfile(0, installed));
    CHECK(installed.profile == M::DriveProfile::LICHUAN_C);
    CHECK(std::strcmp(installed.profileId, "lichuan_c") == 0);
    CHECK(!M::getDiscoveryProfile(1, installed));
}
} // namespace

int main() {
    motorKindsAndOptionalGeometry();
    independentScalesAndWideIntent();
    productionNumericMigration();
    nativeFloatQuantization();
    protocolNeutralBinding();
    vescExtendedData();
    odriveRemoteRequest();
    moteusFdRequirement();
    rampDimensionsAndCapabilityGuards();
    nativeEvidenceWithoutTransmissionConfirmation();
    absentAcknowledgementAndTrajectoryDone();
    profileScopedObjectsAndCapacities();
    std::puts("production multidrive contract fixtures passed");
    return EXIT_SUCCESS;
}
