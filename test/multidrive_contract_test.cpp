/** @file multidrive_contract_test.cpp
 * @brief Test-only documentary/common invariants for the prompt-6 prototype.
 * No N5/EPOS4 profile, native vendor sequence or hardware behavior is implemented.
 * Sources: docs/reference/drive_contrasts.md C1, C1b, C2, C4, C10 and C11.
 * N5 original: docs/reference/multidrive/vendor/nanotec_n5_canopen_v3.3.0.pdf
 * EPOS4 original (remote-only, rel13740):
 * https://www.maxongroup.com/medias/sys_master/root/9579215585310/EPOS4-Firmware-Specification-En.pdf
 * Lichuan original: docs/reference/vendor/86C_CANopen_user_manual_en.pdf
 * SPDX-License-Identifier: MIT
 */
#include "Operations.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace {
namespace M = MotorControlCAN;
namespace P = MotorControlCAN::ProofProfile;

void check(bool condition, const char* expression, int line) {
    if (!condition) {
        std::fprintf(stderr, "fixture check failed at line %d: %s\n", line, expression);
        std::exit(EXIT_FAILURE);
    }
}
#define CHECK(expression) check(static_cast<bool>(expression), #expression, __LINE__)

// Capture the original object representation to verify atomic failure publication.
template <typename T> struct Saved {
    unsigned char bytes[sizeof(T)];
    explicit Saved(const T& value) { std::memcpy(bytes, &value, sizeof value); }
    bool unchanged(const T& value) const { return std::memcmp(bytes, &value, sizeof value) == 0; }
};

M::OperationToken operation(uint32_t value = 7) {
    M::OperationToken token;
    token.value = value;
    return token;
}

M::Binding binding() {
    M::Binding value;
    value.logicalAxisId = 91;
    value.endpoint.bus = 3;
    value.endpoint.profile = 17;
    value.endpoint.address = 5;
    value.bindingGeneration = 4;
    value.configurationGeneration = 8;
    return value;
}

M::UnitScale scale(uint32_t numerator, uint32_t denominator = 1) {
    M::UnitScale value;
    value.numerator = numerator;
    value.denominator = denominator;
    value.source = M::ScaleSource::DOCUMENTED;
    return value;
}

M::ProofAxisConfig axis(M::MotorKind motor) {
    M::ProofAxisConfig value;
    value.binding = binding();
    value.capabilities.motor = motor;
    value.capabilities.feedback = M::FeedbackKind::ENCODER;
    value.capabilities.position = true;
    value.capabilities.velocity = true;
    value.originKnown = true;
    value.originSource = M::ScaleSource::ASSUMED;
    value.units.commandStepsPerMotorTurn = scale(3600);
    value.units.motorTurnsPerLoadTurn = scale(1);
    return value;
}

M::PositionRequest turns(const M::ProofAxisConfig& configuration, int64_t numerator,
                         uint64_t denominator = 1) {
    M::PositionRequest request;
    request.value = M::Rational(numerator, denominator);
    request.unit = M::PositionUnit::TURNS;
    request.frame = M::CoordinateFrame::LOAD;
    request.configurationGeneration = configuration.binding.configurationGeneration;
    return request;
}

M::ServiceEvidence acknowledgement(const M::Binding& route, M::OperationToken token,
                                  M::StepToken step) {
    M::ServiceEvidence value;
    value.binding = route;
    value.operation = token;
    value.step = step;
    value.kind = M::EventKind::ACKNOWLEDGED;
    value.observedUs = 20;
    value.queued = true;
    value.transmissionPossible = true;
    value.protocolAcknowledged = true;
    return value;
}

void motorKindsAndOptionalGeometry() {
    // N5 manual v3.3.0 pp38,64: 3202=0x08 stepper / 0x40 BLDC;
    // configured 60A8=0xFF410000 means tenths of a degree, 3600 per turn.
    // See docs/reference/multidrive/vendor/nanotec_n5_canopen_v3.3.0.pdf.
    const M::MotorKind kinds[] = {M::MotorKind::STEPPER, M::MotorKind::BLDC};
    const uint32_t documentedMotorCodes[] = {0x08, 0x40};
    for (unsigned i = 0; i < 2; ++i) {
        M::ProofAxisConfig configuration = axis(kinds[i]);
        configuration.binding.endpoint.profile = documentedMotorCodes[i];
        CHECK(configuration.units.fullStepsPerMotorTurn.numerator == 0);
        const Saved<M::ProofAxisConfig> original(configuration);
        P::MoveContext context;
        CHECK(P::prepareMoveAbsolute(turns(configuration, 1), configuration,
                                     operation(), 0, 100, context));
        CHECK(context.nativeTarget == 3600);
        CHECK(context.configuration.capabilities.motor == kinds[i]);
        CHECK(context.configuration.units.fullStepsPerMotorTurn.source == M::ScaleSource::UNKNOWN);
        CHECK(context.execution == M::ActionExecution::NOT_TRANSMITTED);
        CHECK(context.completion == M::ActionCompletion::NOT_OBSERVED);
        CHECK(original.unchanged(configuration));
    }

    // EPOS4 Firmware Specification rel13740 pp221,234: command and sensor
    // position representations need explicit interpretation, not step geometry.
    // This selected 8192 scale is abstract, not an EPOS4 factory default.
    M::ProofAxisConfig servo = axis(M::MotorKind::BRUSHED_DC);
    servo.binding.endpoint.profile = 23;
    servo.units.commandStepsPerMotorTurn = scale(8192);
    P::MoveContext context;
    CHECK(P::prepareMoveAbsolute(turns(servo, 3, 2), servo, operation(), 0, 100, context));
    CHECK(context.nativeTarget == 12288);
    CHECK(context.configuration.units.fullStepsPerMotorTurn.numerator == 0);
    CHECK(!context.configuration.capabilities.torque && !context.configuration.capabilities.brake);
}

void independentScalesAndWideIntent() {
    // C1b is explicitly abstract: 1/2 load turn * 3/2 motor/load * 4096 = 3072.
    // The same mechanical displacement is 1536 feedback counts at 2048/turn.
    M::ProofAxisConfig configuration = axis(M::MotorKind::BLDC);
    configuration.units.commandStepsPerMotorTurn = scale(4096);
    configuration.units.motorTurnsPerLoadTurn = scale(3, 2);
    configuration.units.encoder.countsPerUnit = scale(2048);
    configuration.units.encoder.sourceId = 1;
    configuration.units.encoder.countsPerUnit.source = M::ScaleSource::READBACK;
    P::MoveContext context;
    CHECK(P::prepareMoveAbsolute(turns(configuration, 1, 2), configuration,
                                 operation(), 0, 100, context));
    CHECK(context.nativeTarget == 3072);
    CHECK(context.configuration.units.encoder.countsPerUnit.numerator == 2048);
    CHECK(context.configuration.units.encoder.countsPerUnit.source == M::ScaleSource::READBACK);
    CHECK(context.configuration.units.commandStepsPerMotorTurn.source == M::ScaleSource::DOCUMENTED);
    configuration.units.encoder.countsPerUnit = M::UnitScale();
    P::MoveContext withoutFeedback;
    CHECK(P::prepareMoveAbsolute(turns(configuration, 1, 2), configuration,
                                 operation(8), 0, 100, withoutFeedback));
    CHECK(withoutFeedback.nativeTarget == 3072);

    // C2: N5/EPOS4 60FF is I32; +/-70000 must not narrow to RS I16 RPM.
    M::VelocityRequest velocity;
    CHECK(velocity.unit.position == M::PositionUnit::STEPS && velocity.unit.time == M::TimeUnit::SECOND);
    velocity.value = M::Rational(-70000);
    velocity.unit.position = M::PositionUnit::TURNS;
    velocity.unit.time = M::TimeUnit::MINUTE;
    CHECK(velocity.value.numerator == -70000 && velocity.value.denominator == 1);
    velocity.value = M::Rational(70000);
    CHECK(velocity.value.numerator == 70000);
    M::PositionRequest native;
    native.value = M::Rational(-70000);
    native.configurationGeneration = configuration.binding.configurationGeneration;
    CHECK(P::prepareMoveAbsolute(native, configuration, operation(9), 0, 100, context));
    CHECK(context.nativeTarget == -70000);
    M::ServiceWork work;
    bool ready = false;
    CHECK(P::nextMove(context, 0, work, ready) && ready);
    const uint8_t negative70000[] = {0x90, 0xEE, 0xFE, 0xFF};
    CHECK(work.payload.length == 4);
    CHECK(std::memcmp(work.payload.bytes, negative70000, sizeof negative70000) == 0);
}

void productionNumericMigration() {
    M::ProofAxisConfig configuration = axis(M::MotorKind::BLDC);
    configuration.units.commandStepsPerMotorTurn = scale(1);
    P::MoveContext context;
    CHECK(P::prepareMoveAbsolute(turns(configuration, 71), configuration,
        operation(), 0, 100, context));
    const Saved<P::MoveContext> original(context);
    M::PositionRequest request = turns(configuration, INT64_MAX);
    request.frame = M::CoordinateFrame::MOTOR;
    configuration.originNative = 1;
    M::Status status = P::prepareMoveAbsolute(request, configuration, operation(), 0, 100, context);
    CHECK(status.code == M::Err::ILLEGAL_VALUE);
    CHECK(status.detail == 100 + static_cast<int32_t>(M::UnitError::ARITHMETIC_OVERFLOW));
    CHECK(original.unchanged(context));
    request.value = M::Rational(INT64_MIN); configuration.originNative = -1;
    status = P::prepareMoveAbsolute(request, configuration, operation(), 0, 100, context);
    CHECK(status.code == M::Err::ILLEGAL_VALUE);
    CHECK(status.detail == 100 + static_cast<int32_t>(M::UnitError::ARITHMETIC_OVERFLOW));
    CHECK(original.unchanged(context));

    // Origin addition is checked in int64 before the final I32 profile bound.
    request.value = M::Rational(static_cast<int64_t>(INT32_MAX) + 1);
    CHECK(P::prepareMoveAbsolute(request, configuration, operation(), 0, 100, context));
    CHECK(context.nativeTarget == INT32_MAX);
    request.value = M::Rational(static_cast<int64_t>(INT32_MIN) - 1);
    configuration.originNative = 1;
    CHECK(P::prepareMoveAbsolute(request, configuration, operation(), 0, 100, context));
    CHECK(context.nativeTarget == INT32_MIN);

    M::PositionRequest native;
    native.configurationGeneration = configuration.binding.configurationGeneration;
    native.value = M::Rational(1, 2);
    const Saved<P::MoveContext> boundary(context);
    status = P::prepareMoveAbsolute(native, configuration, operation(), 0, 100, context);
    CHECK(status.code == M::Err::ILLEGAL_VALUE);
    CHECK(status.detail == 100 + static_cast<int32_t>(M::UnitError::FRACTIONAL_TARGET));
    CHECK(boundary.unchanged(context));
    native.value = M::Rational(1, 0);
    status = P::prepareMoveAbsolute(native, configuration, operation(), 0, 100, context);
    CHECK(status.code == M::Err::ILLEGAL_VALUE);
    CHECK(status.detail == static_cast<int32_t>(M::AxisError::INVALID_ARGUMENT));
    CHECK(boundary.unchanged(context));

    // A native request still validates malformed supplied optional facts.
    native.value = M::Rational(1);
    configuration.units.fullStepsPerMotorTurn = scale(1, 0);
    status = P::prepareMoveAbsolute(native, configuration, operation(), 0, 100, context);
    CHECK(status.code == M::Err::INVALID_CONFIG);
    CHECK(status.detail == 100 + static_cast<int32_t>(M::UnitError::INVALID_SCALE));
    CHECK(boundary.unchanged(context));
    configuration.units.fullStepsPerMotorTurn = M::UnitScale();
    configuration.units.motorTurnsPerLoadTurn = scale(1, 0);
    request.value = M::Rational(1);
    status = P::prepareMoveAbsolute(request, configuration, operation(), 0, 100, context);
    CHECK(status.code == M::Err::INVALID_CONFIG);
    CHECK(status.detail == 100 + static_cast<int32_t>(M::UnitError::INVALID_SCALE));
    CHECK(boundary.unchanged(context));

    // The proof now consumes production arithmetic rather than its old 1e6 cap.
    configuration.originNative = 0;
    configuration.units.motorTurnsPerLoadTurn = scale(1);
    configuration.units.commandStepsPerMotorTurn = scale(2000000, 2000000);
    request.value = M::Rational(2000000, 2000000);
    CHECK(P::prepareMoveAbsolute(request, configuration, operation(), 0, 100, context));
    CHECK(context.nativeTarget == 1);
    native.value = M::Rational(INT32_MAX);
    CHECK(P::prepareMoveAbsolute(native, configuration, operation(), 0, 100, context));
    CHECK(context.nativeTarget == INT32_MAX);
}

void protocolNeutralBindingAndFrames() {
    // C4 is an abstract route, not a claimed proprietary driver or shared bus.
    M::Binding proprietary = binding();
    proprietary.endpoint.protocol = M::Protocol::PROPRIETARY;
    proprietary.endpoint.address = 500;
    proprietary.endpoint.subaxis = 2;
    CHECK(M::validateBinding(proprietary));
    M::Binding anotherAxis = proprietary;
    ++anotherAxis.logicalAxisId;
    CHECK(!M::sameBinding(proprietary, anotherAxis));
    anotherAxis = proprietary;
    anotherAxis.endpoint.subaxis = 3;
    CHECK(!M::sameBinding(proprietary, anotherAxis));
    P::ReadContext read;
    const Saved<P::ReadContext> originalRead(read);
    CHECK(!P::prepareIdentity(proprietary, operation(), 0, 100, read));
    CHECK(originalRead.unchanged(read));
    M::ProofAxisConfig configuration = axis(M::MotorKind::BLDC);
    configuration.binding = proprietary;
    P::MoveContext move;
    const Saved<P::MoveContext> originalMove(move);
    CHECK(!P::prepareMoveAbsolute(turns(configuration, 1), configuration, operation(), 0, 100, move));
    CHECK(originalMove.unchanged(move));

    M::Frame frame;
    frame.extended = true;
    frame.id = 0x1FFFFFFF;
    CHECK(M::validateFrame(frame));
    frame.extended = false;
    CHECK(M::validateFrame(frame).code == M::Err::FRAME_ERROR);
    frame.id = 0x123;
    frame.rtr = true;
    frame.dlc = 8;
    CHECK(M::validateFrame(frame));
    frame.payloadLength = 1;
    CHECK(M::validateFrame(frame).code == M::Err::FRAME_ERROR);
    frame.payloadLength = 0;
    frame.fd = true;
    CHECK(M::validateFrame(frame).code == M::Err::UNSUPPORTED);
}

void rampDimensionsAndCapabilityGuards() {
    // C10 illustrative values: LC EN86 p11, 6083=100 ms; EPOS4 rel13740
    // pp230,236, 6083=100 rpm/s with 60AA=0x00C00300. No writes/conversion.
    M::NativeRamp lichuan;
    lichuan.kind = M::RampKind::TIME_MS;
    lichuan.value = 100;
    M::NativeRamp epos;
    epos.kind = M::RampKind::ACCELERATION_RPM_PER_SECOND;
    epos.value = 100;
    CHECK(lichuan.value == epos.value && lichuan.kind != epos.kind);

    // C11 variants are abstract. Brake/configuration distinctions are documented
    // by N5 p107 and EPOS4 rel13740 p193; these are not vendor capability tables.
    M::ProofAxisConfig variantA = axis(M::MotorKind::BLDC);
    const Saved<M::Capabilities> beforeA(variantA.capabilities);
    CHECK(M::requireCapability(variantA.capabilities, M::Capability::POSITION));
    CHECK(M::requireCapability(variantA.capabilities, M::Capability::TORQUE).code == M::Err::UNSUPPORTED);
    CHECK(M::requireCapability(variantA.capabilities, M::Capability::BRAKE).code == M::Err::UNSUPPORTED);
    CHECK(beforeA.unchanged(variantA.capabilities));
    M::ProofAxisConfig variantB = variantA;
    variantB.capabilities.position = false;
    variantB.capabilities.torque = true;
    variantB.capabilities.brake = true;
    variantB.capabilities.calibrationRequired = true;
    const Saved<M::Capabilities> beforeB(variantB.capabilities);
    CHECK(M::requireCapability(variantB.capabilities, M::Capability::POSITION).code == M::Err::UNSUPPORTED);
    const M::Status unqualified = M::requireCapability(variantB.capabilities, M::Capability::TORQUE);
    CHECK(unqualified.code == M::Err::INVALID_CONFIG);
    CHECK(unqualified.detail == static_cast<int32_t>(M::MoveError::CALIBRATION_UNQUALIFIED));
    CHECK(!M::requireCapability(variantB.capabilities, M::Capability::BRAKE));
    CHECK(beforeB.unchanged(variantB.capabilities));
    P::MoveContext output;
    const Saved<P::MoveContext> original(output);
    CHECK(!P::prepareMoveAbsolute(turns(variantB, 1), variantB, operation(), 0, 100, output));
    CHECK(original.unchanged(output));
    variantB.capabilities.position = true;
    CHECK(!P::prepareMoveAbsolute(turns(variantB, 1), variantB, operation(), 0, 100, output));
    CHECK(original.unchanged(output));
    CHECK(!variantB.capabilities.calibrationQualified);
    variantB.capabilities.calibrationQualified = true;
    CHECK(M::requireCapability(variantB.capabilities, M::Capability::POSITION));
}

void atomicPublicationAndTerminalEvidence() {
    const M::Binding route = binding();
    P::ReadContext read;
    CHECK(P::prepareIdentity(route, operation(), 0, 100, read));
    P::Identity identity;
    identity.value = 0xA5A5A5A5;
    const Saved<P::Identity> originalIdentity(identity);
    CHECK(!P::getIdentity(read, route, identity));
    CHECK(originalIdentity.unchanged(identity));
    M::ServiceEvidence event = acknowledgement(route, read.operation, read.step);
    event.payload.length = 4;
    event.payload.bytes[0] = 0x78;
    event.payload.bytes[1] = 0x56;
    event.payload.bytes[2] = 0x34;
    event.payload.bytes[3] = 0x12;
    ++event.step.value;
    const Saved<P::ReadContext> originalRead(read);
    CHECK(!P::advanceRead(read, event));
    CHECK(originalRead.unchanged(read));
    --event.step.value;
    CHECK(P::advanceRead(read, event));
    event.payload.bytes[0] = 0;
    CHECK(P::getIdentity(read, route, identity));
    CHECK(identity.value == 0x12345678 && identity.observedUs == 20);
    M::Binding stale = route;
    ++stale.configurationGeneration;
    const Saved<P::Identity> publishedIdentity(identity);
    CHECK(!P::getIdentity(read, stale, identity));
    CHECK(publishedIdentity.unchanged(identity));
    const Saved<P::ReadContext> terminalRead(read);
    CHECK(!P::prepareIdentity(route, operation(9), 100, 100, read));
    CHECK(terminalRead.unchanged(read));

    M::ProofAxisConfig configuration = axis(M::MotorKind::BLDC);
    P::MoveContext move;
    CHECK(P::prepareMoveAbsolute(turns(configuration, 1), configuration, operation(), 0, 100, move));
    M::ServiceEvidence rejected = acknowledgement(route, move.operation, move.step);
    rejected.kind = M::EventKind::PROTOCOL_REJECTED;
    rejected.protocolAcknowledged = false;
    rejected.abortValid = true;
    rejected.abortCode = 0xF1234567;
    CHECK(P::advanceMove(move, rejected));
    rejected.abortCode = 0;
    CHECK(move.state == M::ActionState::FAILED && move.status.code == M::Err::PROTOCOL_ERROR);
    CHECK(move.execution == M::ActionExecution::REJECTED && move.uncertain);
    CHECK(move.evidence.abortValid && move.evidence.abortCode == 0xF1234567);
    CHECK(move.completion == M::ActionCompletion::NOT_OBSERVED);
    M::ServiceWork output;
    output.index = 0xA555;
    const Saved<M::ServiceWork> originalWork(output);
    bool ready = true;
    CHECK(P::nextMove(move, 20, output, ready) && !ready);
    CHECK(originalWork.unchanged(output));
    const Saved<P::MoveContext> terminalMove(move);
    configuration.units.commandStepsPerMotorTurn = M::UnitScale();
    CHECK(!P::prepareMoveAbsolute(turns(configuration, 1), configuration, operation(9), 0, 100, move));
    CHECK(terminalMove.unchanged(move));
}
} // namespace

int main() {
    motorKindsAndOptionalGeometry();
    independentScalesAndWideIntent();
    productionNumericMigration();
    protocolNeutralBindingAndFrames();
    rampDimensionsAndCapabilityGuards();
    atomicPublicationAndTerminalEvidence();
    std::puts("prompt-6 test-only contract fixtures passed");
    return EXIT_SUCCESS;
}
