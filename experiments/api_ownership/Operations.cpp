/** @file Operations.cpp
 * @brief Explicit one-step proof operations; no transport engine or vendor logic.
 * SPDX-License-Identifier: MIT
 */
#include "Operations.h"

namespace MotorControlCAN {

Status requireCapability(const Capabilities& facts, Capability requested) noexcept {
    bool available = false;
    switch (requested) {
    case Capability::POSITION: available = facts.position; break;
    case Capability::VELOCITY: available = facts.velocity; break;
    case Capability::TORQUE: available = facts.torque; break;
    case Capability::BRAKE: available = facts.brake; break;
    }
    if (!available) {
        return Status(Err::UNSUPPORTED, static_cast<int32_t>(MoveError::UNSUPPORTED_CAPABILITY),
                      "requested capability unavailable");
    }
    if (facts.calibrationRequired && !facts.calibrationQualified) {
        return Status(Err::INVALID_CONFIG, static_cast<int32_t>(MoveError::CALIBRATION_UNQUALIFIED),
                      "required calibration unqualified");
    }
    return Ok();
}

namespace ProofProfile {
namespace {

Status readError(ReadError reason, const char* message) {
    return Status(Err::INVALID_CONFIG, static_cast<int32_t>(reason), message);
}

Status moveError(MoveError reason, const char* message, Err code = Err::INVALID_CONFIG) {
    return Status(code, static_cast<int32_t>(reason), message);
}

bool correlated(const Binding& binding, OperationToken operation, StepToken step,
                const ServiceEvidence& evidence) {
    return sameBinding(binding, evidence.binding) && operation.value == evidence.operation.value &&
        step.value == evidence.step.value;
}

bool eventShapeValid(const ServiceEvidence& evidence) {
    if (evidence.payload.length > sizeof evidence.payload.bytes) return false;
    switch (evidence.kind) {
    case EventKind::ACKNOWLEDGED:
        return evidence.protocolAcknowledged && evidence.transmissionPossible &&
            evidence.queued && !evidence.abortValid;
    case EventKind::PROTOCOL_REJECTED:
        return !evidence.protocolAcknowledged && evidence.abortValid &&
            evidence.transmissionPossible && evidence.queued;
    case EventKind::REPLY_ERROR:
        return !evidence.protocolAcknowledged && evidence.transmissionPossible && evidence.queued;
    case EventKind::TRANSPORT_FAILURE:
    case EventKind::CANCEL:
    case EventKind::DEADLINE:
        return !evidence.protocolAcknowledged && !evidence.abortValid &&
            (!evidence.transmissionPossible || evidence.queued);
    }
    return false;
}

bool eventTimeValid(uint64_t startedUs, uint64_t servicedUs, uint64_t deadlineUs,
                    const ServiceEvidence& evidence) {
    if (evidence.observedUs < startedUs || evidence.observedUs < servicedUs) return false;
    return evidence.kind == EventKind::DEADLINE ? evidence.observedUs >= deadlineUs :
        evidence.observedUs < deadlineUs;
}

ServiceWork workFor(const Binding& binding, OperationToken operation, StepToken step,
                    uint64_t deadlineUs, WorkKind kind, uint16_t index, bool mayChange) {
    ServiceWork work;
    work.binding = binding;
    work.operation = operation;
    work.step = step;
    work.deadlineUs = deadlineUs;
    work.kind = kind;
    work.index = index;
    work.mayChange = mayChange;
    return work;
}

uint32_t decodeU32(const Payload& payload) {
    return static_cast<uint32_t>(payload.bytes[0]) |
        (static_cast<uint32_t>(payload.bytes[1]) << 8) |
        (static_cast<uint32_t>(payload.bytes[2]) << 16) |
        (static_cast<uint32_t>(payload.bytes[3]) << 24);
}

void encodeI32(int32_t value, Payload& payload) {
    const uint32_t bits = static_cast<uint32_t>(value);
    payload.length = 4;
    for (uint8_t i = 0; i < 4; ++i) payload.bytes[i] = static_cast<uint8_t>(bits >> (8 * i));
}

// Small prototype bounds make every admitted multiplication auditable. Prompts
// 9-10 replace this limited conversion; no general rational engine is implied.
constexpr uint64_t INPUT_LIMIT = 1000000ULL;
constexpr uint64_t PRODUCT_LIMIT = 1000000000000000000ULL;

bool multiplyBounded(uint64_t& value, uint64_t factor) {
    if (!factor || value > PRODUCT_LIMIT / factor) return false;
    value *= factor;
    return true;
}

bool scaleUsable(const UnitScale& scale) {
    return scale.numerator > 0 && scale.numerator <= INPUT_LIMIT &&
        scale.denominator > 0 && scale.denominator <= INPUT_LIMIT &&
        scale.source != ScaleSource::UNKNOWN &&
        static_cast<uint8_t>(scale.source) <= static_cast<uint8_t>(ScaleSource::QUALIFIED);
}

Status convertTarget(const PositionRequest& request, const AxisConfig& config, int32_t& output) {
    if (request.configurationGeneration != config.binding.configurationGeneration)
        return moveError(MoveError::STALE_CONFIGURATION, "stale coordinate generation");
    if (request.rounding != Rounding::EXACT || request.maximumQuantizationError.numerator != 0 ||
        request.maximumQuantizationError.denominator == 0)
        return moveError(MoveError::INVALID_REQUEST, "prototype requires exact integer quantization", Err::UNSUPPORTED);
    if (request.value.denominator == 0 || request.value.denominator > INPUT_LIMIT ||
        request.value.numerator < -static_cast<int64_t>(INPUT_LIMIT) ||
        request.value.numerator > static_cast<int64_t>(INPUT_LIMIT))
        return moveError(MoveError::OUT_OF_RANGE, "prototype input bound exceeded", Err::ILLEGAL_VALUE);
    if (config.nativeMinimum > config.nativeMaximum)
        return moveError(MoveError::INVALID_REQUEST, "invalid inclusive native limits");

    const bool negative = request.value.numerator < 0;
    uint64_t numerator = static_cast<uint64_t>(negative ? -request.value.numerator : request.value.numerator);
    uint64_t denominator = request.value.denominator;
    if (request.frame == CoordinateFrame::NATIVE) {
        if (request.unit != PositionUnit::NATIVE_INCREMENTS)
            return moveError(MoveError::INVALID_REQUEST, "native frame requires native increments", Err::UNSUPPORTED);
    } else if (request.frame == CoordinateFrame::MOTOR || request.frame == CoordinateFrame::LOAD) {
        if (request.unit != PositionUnit::TURNS && request.unit != PositionUnit::DEGREES)
            return moveError(MoveError::INVALID_REQUEST, "angular frame requires turns or degrees", Err::UNSUPPORTED);
        if (!config.originKnown) return moveError(MoveError::MISSING_ORIGIN, "coordinate origin unavailable");
        if (!scaleUsable(config.units.commandIncrementsPerMotorTurn))
            return moveError(MoveError::UNRESOLVED_UNITS, "command scale unavailable");
        if (!multiplyBounded(numerator, config.units.commandIncrementsPerMotorTurn.numerator) ||
            !multiplyBounded(denominator, config.units.commandIncrementsPerMotorTurn.denominator))
            return moveError(MoveError::OUT_OF_RANGE, "prototype product bound exceeded", Err::ILLEGAL_VALUE);
        if (request.frame == CoordinateFrame::LOAD) {
            if (!scaleUsable(config.units.motorTurnsPerLoadTurn))
                return moveError(MoveError::UNRESOLVED_UNITS, "gear scale unavailable");
            if (!multiplyBounded(numerator, config.units.motorTurnsPerLoadTurn.numerator) ||
                !multiplyBounded(denominator, config.units.motorTurnsPerLoadTurn.denominator))
                return moveError(MoveError::OUT_OF_RANGE, "prototype product bound exceeded", Err::ILLEGAL_VALUE);
        }
        if (request.unit == PositionUnit::DEGREES && !multiplyBounded(denominator, 360))
            return moveError(MoveError::OUT_OF_RANGE, "prototype degree denominator bound exceeded", Err::ILLEGAL_VALUE);
    } else {
        return moveError(MoveError::INVALID_REQUEST, "unknown coordinate frame");
    }
    if (numerator % denominator)
        return moveError(MoveError::FRACTIONAL_TARGET, "target is not exact native integer", Err::ILLEGAL_VALUE);
    const int64_t magnitude = static_cast<int64_t>(numerator / denominator);
    int64_t native = negative ? -magnitude : magnitude;
    if (request.frame != CoordinateFrame::NATIVE) native += config.originNative;
    if (native < INT32_MIN || native > INT32_MAX ||
        native < config.nativeMinimum || native > config.nativeMaximum)
        return moveError(MoveError::OUT_OF_RANGE, "native target outside inclusive limits", Err::ILLEGAL_VALUE);
    output = static_cast<int32_t>(native);
    return Ok();
}

} // namespace

Status prepareIdentity(const Binding& binding, OperationToken operation,
                       uint64_t nowUs, uint64_t deadlineUs, ReadContext& output) noexcept {
    if (!validateBinding(binding)) return readError(ReadError::INVALID_TARGET, "invalid common binding");
    if (binding.endpoint.protocol != Protocol::CANOPEN)
        return Status(Err::UNSUPPORTED, static_cast<int32_t>(ReadError::INVALID_TARGET),
                      "synthetic profile requires CANopen");
    if (!operation.value) return readError(ReadError::INVALID_OPERATION, "zero operation token");
    if (deadlineUs <= nowUs) return readError(ReadError::INVALID_DEADLINE, "deadline must follow preparation");
    ReadContext candidate;
    candidate.binding = binding;
    candidate.operation = operation;
    candidate.step.value = 1;
    candidate.state = ReadState::ACTIVE;
    candidate.startedUs = candidate.servicedUs = nowUs;
    candidate.deadlineUs = deadlineUs;
    output = candidate;
    return Ok();
}

Status nextRead(const ReadContext& context, uint64_t nowUs,
                ServiceWork& output, bool& ready) noexcept {
    if (context.state == ReadState::EMPTY) return readError(ReadError::INVALID_STATE, "empty read context");
    if (nowUs < context.servicedUs) return readError(ReadError::CLOCK_ERROR, "backward supplied time");
    if (context.state != ReadState::ACTIVE) {
        if (context.state != ReadState::SUCCEEDED && context.state != ReadState::FAILED)
            return readError(ReadError::INVALID_STATE, "unknown read state");
        ready = false;
        return Ok();
    }
    if (nowUs >= context.deadlineUs) return readError(ReadError::DEADLINE_EXPIRED, "read deadline reached");
    output = workFor(context.binding, context.operation, context.step, context.deadlineUs,
                     WorkKind::SDO_UPLOAD, IDENTITY_OBJECT, false);
    ready = true;
    return Ok();
}

Status advanceRead(ReadContext& context, const ServiceEvidence& evidence) noexcept {
    if (context.state != ReadState::ACTIVE) return readError(ReadError::INVALID_STATE, "read is not active");
    if (!correlated(context.binding, context.operation, context.step, evidence))
        return readError(ReadError::WRONG_CORRELATION, "read evidence provenance mismatch");
    if (!eventShapeValid(evidence)) return readError(ReadError::INVALID_EVENT, "invalid service evidence");
    if (!eventTimeValid(context.startedUs, context.servicedUs, context.deadlineUs, evidence))
        return readError(ReadError::CLOCK_ERROR, "invalid event time or deadline kind");
    ReadContext candidate = context;
    candidate.evidence = evidence;
    candidate.servicedUs = evidence.observedUs;
    candidate.state = ReadState::FAILED;
    switch (evidence.kind) {
    case EventKind::ACKNOWLEDGED:
        if (evidence.payload.length == 4) {
            candidate.identity.value = decodeU32(evidence.payload);
            candidate.identity.binding = candidate.binding;
            candidate.identity.observedUs = evidence.observedUs;
            candidate.state = ReadState::SUCCEEDED;
            candidate.outcome = ReadOutcome::SUCCESS;
        } else {
            candidate.outcome = ReadOutcome::REPLY_ERROR;
            candidate.status = Status(Err::FRAME_ERROR, 0, "synthetic identity requires four bytes");
        }
        break;
    case EventKind::PROTOCOL_REJECTED:
        candidate.outcome = ReadOutcome::PROTOCOL_REJECTED;
        candidate.status = Status(Err::PROTOCOL_ERROR, 0, "native service rejected");
        break;
    case EventKind::REPLY_ERROR:
        candidate.outcome = ReadOutcome::REPLY_ERROR;
        candidate.status = Status(Err::FRAME_ERROR, 0, "malformed native service reply");
        break;
    case EventKind::TRANSPORT_FAILURE:
        candidate.outcome = ReadOutcome::TRANSPORT_ERROR;
        candidate.status = readError(ReadError::TRANSPORT_FAILURE, "service transport failure");
        break;
    case EventKind::CANCEL:
        candidate.outcome = ReadOutcome::CANCELLED;
        candidate.status = readError(ReadError::CANCELLED, "local read cancellation");
        break;
    case EventKind::DEADLINE:
        candidate.outcome = ReadOutcome::DEADLINE;
        candidate.status = readError(ReadError::DEADLINE_EXPIRED, "read deadline reached");
        break;
    }
    context = candidate;
    return Ok();
}

Status getIdentity(const ReadContext& context, const Binding& currentBinding,
                   Identity& output) noexcept {
    if (context.kind != ReadKind::IDENTITY) return readError(ReadError::WRONG_KIND, "not an identity read");
    if (context.state != ReadState::SUCCEEDED || context.outcome != ReadOutcome::SUCCESS)
        return readError(ReadError::NOT_COMPLETE, "identity read not successful");
    if (!sameBinding(context.binding, currentBinding))
        return readError(ReadError::STALE_GENERATION, "identity provenance is stale");
    output = context.identity;
    return Ok();
}

Status prepareMoveAbsolute(const PositionRequest& request, const AxisConfig& configuration,
                           OperationToken operation, uint64_t nowUs, uint64_t deadlineUs,
                           MoveContext& output) noexcept {
    if (!validateBinding(configuration.binding)) return moveError(MoveError::INVALID_TARGET, "invalid common binding");
    if (configuration.binding.endpoint.protocol != Protocol::CANOPEN)
        return moveError(MoveError::INVALID_TARGET, "synthetic profile requires CANopen", Err::UNSUPPORTED);
    if (!operation.value) return moveError(MoveError::INVALID_OPERATION, "zero operation token");
    if (deadlineUs <= nowUs) return moveError(MoveError::INVALID_DEADLINE, "deadline must follow preparation");
    const Status capability = requireCapability(configuration.capabilities, Capability::POSITION);
    if (!capability) return capability;
    MoveContext candidate;
    const Status converted = convertTarget(request, configuration, candidate.nativeTarget);
    if (!converted) return converted;
    candidate.binding = configuration.binding;
    candidate.operation = operation;
    candidate.step.value = 1;
    candidate.request = request;
    candidate.configuration = configuration;
    candidate.state = ActionState::ACTIVE;
    candidate.startedUs = candidate.servicedUs = nowUs;
    candidate.deadlineUs = deadlineUs;
    output = candidate;
    return Ok();
}

Status nextMove(const MoveContext& context, uint64_t nowUs,
                ServiceWork& output, bool& ready) noexcept {
    if (context.state == ActionState::EMPTY) return moveError(MoveError::INVALID_STATE, "empty move context");
    if (nowUs < context.servicedUs) return moveError(MoveError::CLOCK_ERROR, "backward supplied time");
    if (context.state != ActionState::ACTIVE) {
        if (context.state != ActionState::SUCCEEDED && context.state != ActionState::FAILED)
            return moveError(MoveError::INVALID_STATE, "unknown move state");
        ready = false;
        return Ok();
    }
    if (nowUs >= context.deadlineUs) return moveError(MoveError::DEADLINE_EXPIRED, "move deadline reached");
    ServiceWork candidate = workFor(context.binding, context.operation, context.step, context.deadlineUs,
                                    WorkKind::SDO_DOWNLOAD, TARGET_OBJECT, true);
    encodeI32(context.nativeTarget, candidate.payload);
    output = candidate;
    ready = true;
    return Ok();
}

Status advanceMove(MoveContext& context, const ServiceEvidence& evidence) noexcept {
    if (context.state != ActionState::ACTIVE) return moveError(MoveError::INVALID_STATE, "move is not active");
    if (!correlated(context.binding, context.operation, context.step, evidence))
        return moveError(MoveError::WRONG_CORRELATION, "move evidence provenance mismatch");
    if (!eventShapeValid(evidence)) return moveError(MoveError::INVALID_EVENT, "invalid service evidence");
    if (!eventTimeValid(context.startedUs, context.servicedUs, context.deadlineUs, evidence))
        return moveError(MoveError::CLOCK_ERROR, "invalid event time or deadline kind");
    MoveContext candidate = context;
    candidate.evidence = evidence;
    candidate.servicedUs = evidence.observedUs;
    candidate.state = ActionState::FAILED;
    candidate.execution = evidence.transmissionPossible ? ActionExecution::UNKNOWN : ActionExecution::NOT_TRANSMITTED;
    candidate.uncertain = evidence.transmissionPossible;
    switch (evidence.kind) {
    case EventKind::ACKNOWLEDGED:
        if (evidence.payload.length != 0) {
            candidate.outcome = ActionOutcome::REPLY_ERROR;
            candidate.status = Status(Err::FRAME_ERROR, 0, "synthetic write ACK has no payload");
        } else {
            candidate.state = ActionState::SUCCEEDED;
            candidate.outcome = ActionOutcome::ACKNOWLEDGED;
            candidate.execution = ActionExecution::ACKNOWLEDGED;
            candidate.uncertain = false;
        }
        break;
    case EventKind::PROTOCOL_REJECTED:
        candidate.outcome = ActionOutcome::PROTOCOL_REJECTED;
        candidate.execution = ActionExecution::REJECTED;
        // A service rejection does not establish that no earlier effect occurred.
        candidate.status = Status(Err::PROTOCOL_ERROR, 0, "native service rejected");
        break;
    case EventKind::REPLY_ERROR:
        candidate.outcome = ActionOutcome::REPLY_ERROR;
        candidate.status = Status(Err::FRAME_ERROR, 0, "malformed native service reply");
        break;
    case EventKind::TRANSPORT_FAILURE:
        candidate.outcome = ActionOutcome::TRANSPORT_ERROR;
        candidate.status = moveError(MoveError::TRANSPORT_FAILURE, "service transport failure");
        break;
    case EventKind::CANCEL:
        candidate.outcome = ActionOutcome::CANCELLED;
        candidate.status = moveError(MoveError::CANCELLED, "local cancellation is not a motor stop");
        break;
    case EventKind::DEADLINE:
        candidate.outcome = ActionOutcome::DEADLINE;
        candidate.status = moveError(MoveError::DEADLINE_EXPIRED, "move deadline reached");
        break;
    }
    context = candidate;
    return Ok();
}

} // namespace ProofProfile
} // namespace MotorControlCAN
