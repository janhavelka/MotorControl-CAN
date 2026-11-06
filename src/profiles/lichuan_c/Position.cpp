// SPDX-License-Identifier: MIT
#include "ReadPlan.h"
#include <MotorControlCAN/profiles/lichuan_c/Position.h>
#include <MotorControlCAN/profiles/lichuan_c/State.h>
#include <cmath>
#include <new>

namespace MotorControlCAN {
namespace LICHUAN_C {
using namespace Detail;
namespace {
constexpr int64_t TARGET_MINIMUM = -1000000;
constexpr int64_t TARGET_MAXIMUM = 1000000;
constexpr uint32_t MAXIMUM_POSITION_SPEED_RPM = 3000;
constexpr uint32_t MAXIMUM_RAMP_TIME_MS = 2000;

Status error(PositionParameterError detail, const char* message, Err code = Err::INVALID_CONFIG) {
    return Status(code, static_cast<int32_t>(detail), message);
}

Status axisStatus(Status status) {
    if (!status)
        status.detail += static_cast<int32_t>(PositionParameterError::AXIS_ERROR_BASE);
    return status;
}

bool inRange(const NativeQuantity& value) {
    return value.integral >= TARGET_MINIMUM && value.integral <= TARGET_MAXIMUM &&
           !(value.integral == TARGET_MINIMUM && value.numerator && value.negative) &&
           !(value.integral == TARGET_MAXIMUM && value.numerator && !value.negative);
}

Status requestedRange(const PreparedTarget& target) {
    if (!target.exactArithmetic) {
        const int64_t lowerDistance = target.effectiveNative - TARGET_MINIMUM;
        const int64_t upperDistance = TARGET_MAXIMUM - target.effectiveNative;
        const int64_t distance = lowerDistance < upperDistance ? lowerDistance : upperDistance;
        // Comparing the bound with exact small distances avoids rounding a tiny
        // outward uncertainty away when the effective integer is at a limit.
        if (!std::isfinite(target.absoluteErrorBound) || target.absoluteErrorBound < 0 ||
            target.absoluteErrorBound > static_cast<double>(distance))
            return error(PositionParameterError::RANGE,
                         "approximate requested position may exceed the native target range",
                         Err::ILLEGAL_VALUE);
        return Ok();
    }

    // Quantization records effective-requested for the encoded value, before
    // adding a relative basis. Reconstructing from it avoids subtracting a
    // possibly INT64_MIN reference or duplicating coordinate conversion.
    NativeQuantity requested;
    requested.numerator = target.quantization.errorNumerator;
    requested.denominator = target.quantization.errorDenominator;
    requested.negative = requested.numerator && !target.quantization.errorNegative;
    Status status = addNativeInteger(requested, target.effectiveNative, requested);
    if (!status) {
        status.detail += static_cast<int32_t>(AxisError::UNIT_ERROR_BASE);
        return axisStatus(status);
    }
    if (!inRange(requested))
        return error(PositionParameterError::RANGE,
                     "requested position exceeds the native target range", Err::ILLEGAL_VALUE);
    return Ok();
}
} // namespace

Status preparePositionParameters(PreparedPositionParameters& output, SettingsModel model,
                                 const PositionRequest& request, const AxisConfig& axis,
                                 const AxisReference* reference, const DriverSettings& settings,
                                 uint64_t nowUs) noexcept {
    if (overlaps(&output, sizeof output, &request, sizeof request) ||
        overlaps(&output, sizeof output, &axis, sizeof axis) ||
        overlaps(&output, sizeof output, &settings, sizeof settings) ||
        (reference && overlaps(&output, sizeof output, reference, sizeof *reference)))
        return error(PositionParameterError::OVERLAP, "position parameters overlap input");
    if (model != SettingsModel::CL86_C && model != SettingsModel::OL86_C)
        return error(PositionParameterError::MODEL, "explicit documented 86C model required");
    if (request.wrapped || nowUs > uint64_t(INT64_MAX))
        return error(PositionParameterError::ARGUMENT,
                     "position parameters require unwrapped intent and a valid current time");
    if (axis.positionRepresentation != NativeRepresentation::INTEGER)
        return error(PositionParameterError::REPRESENTATION,
                     "86C position target requires integer native representation",
                     Err::UNSUPPORTED);
    if (settings.positionSpeedRpm > MAXIMUM_POSITION_SPEED_RPM ||
        settings.rampUpTimeMs > MAXIMUM_RAMP_TIME_MS ||
        settings.rampDownTimeMs > MAXIMUM_RAMP_TIME_MS)
        return error(PositionParameterError::RANGE, "position profile exceeds native rpm/ms ranges",
                     Err::ILLEGAL_VALUE);

    AxisReference currentReference;
    if (reference) {
        if (reference->nowUs > nowUs)
            return axisStatus(Status(Err::INVALID_CONFIG,
                                     static_cast<int32_t>(AxisError::CLOCK_ERROR),
                                     "position preparation time precedes reference time"));
        currentReference = *reference;
        currentReference.nowUs = nowUs;
        reference = &currentReference;
    }

    PreparedPositionParameters candidate;
    Status status = preparePosition(request, axis, reference, candidate.target);
    if (!status)
        return axisStatus(status);
    if (candidate.target.effectiveNative < TARGET_MINIMUM ||
        candidate.target.effectiveNative > TARGET_MAXIMUM)
        return error(PositionParameterError::RANGE,
                     "effective position exceeds the native target range", Err::ILLEGAL_VALUE);
    status = requestedRange(candidate.target);
    if (!status)
        return status;

    candidate.model = model;
    candidate.axis = axis;
    candidate.settings = settings;
    candidate.targetNative = static_cast<int32_t>(candidate.target.effectiveNative);
    candidate.preparedUs = nowUs;
    output = candidate;
    return Ok();
}

namespace {
// The download widths and directions come from the reviewed ledger. A ledger change that alters
// them stops this compiling instead of silently changing the bytes.
static_assert(OBJECT_607A_00.widthBytes == 4 && OBJECT_6081_00.widthBytes == 4 &&
                  OBJECT_6083_00.widthBytes == 4 && OBJECT_6084_00.widthBytes == 4 &&
                  OBJECT_6040_00.widthBytes == 2,
              "move downloads use the EDS widths");
static_assert(OBJECT_607A_00.sdoAccess == SdoAccess::READ_WRITE &&
                  OBJECT_6081_00.sdoAccess == SdoAccess::READ_WRITE &&
                  OBJECT_6083_00.sdoAccess == SdoAccess::READ_WRITE &&
                  OBJECT_6084_00.sdoAccess == SdoAccess::READ_WRITE &&
                  OBJECT_6040_00.sdoAccess == SdoAccess::WRITE_ONLY,
              "move downloads need writable objects");
// Parameter order: target position, profile speed, acceleration time, deceleration time, then the
// edge (EN86/ZH86 p11-12, OP86 p9-10). The bit of each object in setupMask is its position here.
const ObjectDescriptor* const SETUP[4] = {&OBJECT_607A_00, &OBJECT_6081_00, &OBJECT_6083_00,
                                          &OBJECT_6084_00};
constexpr uint16_t TRIGGER_WORD = 0x001f; // 15 to 31: bit 4 rising, bit 6 clear (absolute).
// 15 to 95: bit 4 rising, bit 6 set (relative), EN86/ZH86 p12 and OP86 p10. Bit 5 stays clear.
constexpr uint16_t RELATIVE_TRIGGER_WORD = 0x005f;
constexpr uint16_t REARM_WORD = 0x000f;   // Project policy after completion (DEC-8).
constexpr uint8_t OBSERVATION_MASK =
    uint8_t((1u << unsigned(StateBlock::MONITORS)) | (1u << unsigned(StateBlock::STATUS)) |
            (1u << unsigned(StateBlock::FEEDBACK)));
// 2100 bits a move tolerates at admission: in place and homed are idle reports.
constexpr uint16_t IDLE_MONITOR_BITS = MONITOR_POSITION_COMPLETE | MONITOR_HOMING_COMPLETE;
// 2100 bits that end a running move's observation scope: alarm, released and both soft limits.
constexpr uint16_t SCOPE_MONITOR_BITS =
    MONITOR_ALARM | MONITOR_RELEASED | MONITOR_POSITIVE_SOFT_LIMIT | MONITOR_NEGATIVE_SOFT_LIMIT;

Status moveError(MoveError detail, const char* message, Err code = Err::INVALID_CONFIG) {
    return Status(code, static_cast<int32_t>(detail), message);
}
Status based(Status status, MoveError base) {
    if (!status)
        status.detail += static_cast<int32_t>(base);
    return status;
}
// Each term is at most 2^33 and the count is single digit, so this cannot wrap.
uint64_t serviceBudget(const MoveOptions& o, uint64_t services) {
    return services * (uint64_t(o.service.queueTimeoutUs) + o.service.responseTimeoutUs);
}
bool optionsValid(const MoveOptions& o) {
    return o.service.queueTimeoutUs && o.service.responseTimeoutUs &&
           o.service.identityWidths == IdentityWidths::DECLARED_U16 &&
           o.service.countWidths == CountWidths::DECLARED_U8 && o.observationIntervalUs &&
           o.maximumAgeUs && o.maximumAgeUs >= serviceBudget(o, MOVE_OBSERVATION_FIELDS);
}
bool signalValid(const MoveSignal& s) {
    return (s.source == MoveSignalSource::STATUSWORD || s.source == MoveSignalSource::MONITOR) &&
           s.mask && !(s.value & uint16_t(~s.mask));
}
bool reported(const MoveSignal& s, const MoveSample& sample) {
    const uint16_t word = s.source == MoveSignalSource::STATUSWORD ? sample.statusword : sample.monitor;
    return (word & s.mask) == s.value;
}
bool sameFirmware(const MoveFirmware& f, const IdentityObservation& i) {
    return f.deviceType == i.deviceType && f.hardwareVersion == i.hardwareVersion &&
           f.softwareVersion == i.softwareVersion && f.vendorId == i.vendorId &&
           f.productCode == i.productCode && f.revision == i.revision &&
           f.modelCode == i.modelCode && f.firmwareCode == i.firmwareCode;
}
uint32_t raw(const ReadContext& c, StateBlock block, unsigned field) {
    return c.values[c.blockStart[static_cast<unsigned>(block)] + field].raw;
}
void bound(MoveSample& s, const ReadWindow& w) {
    if (w.earliestUs < s.earliestUs)
        s.earliestUs = w.earliestUs;
    if (w.latestUs > s.latestUs)
        s.latestUs = w.latestUs;
}
// The admission read: fresh, complete, after the 000F and under the reservation, of an idle drive.
Status checkState(const ReadContext& state, const AxisReservation& r,
                  const CANOPEN::SdoCompletion& control, uint64_t now, uint64_t age,
                  MoveSample& sample) {
    ReadWindow status, monitors;
    if (state.source != ReadSource::SDO || state.state != ReadState::SUCCEEDED || state.uncertain ||
        state.startedUs < r.acquiredUs ||
        !validateStateBlock(state, StateBlock::STATUS, r.target, now, age, status) ||
        !validateStateBlock(state, StateBlock::MONITORS, r.target, now, age, monitors) ||
        status.earliestUs < control.completedUs || monitors.earliestUs < control.completedUs ||
        status.earliestUs < r.acquiredUs || monitors.earliestUs < r.acquiredUs)
        return moveError(MoveError::STATE,
                         "move needs a fresh complete SDO state read after the latest 000F");
    MoveSample s;
    s.present = true;
    s.statusword = uint16_t(raw(state, StateBlock::STATUS, 0));
    s.modeRaw = uint8_t(raw(state, StateBlock::STATUS, 1));
    s.monitor = uint16_t(raw(state, StateBlock::MONITORS, 0));
    s.earliestUs = status.earliestUs;
    s.latestUs = status.latestUs;
    bound(s, monitors);
    if (s.modeRaw != 1)
        return moveError(MoveError::STATE,
                         "move needs reported mode 1; no sequence changes mode on an enabled drive");
    if ((s.statusword & STATUS_LOW_BITS) != STATUS_LOW_BITS)
        return moveError(MoveError::STATE, "move needs 6041 low bits 111 after the 000F");
    if (s.monitor & uint16_t(~IDLE_MONITOR_BITS))
        return moveError(MoveError::STATE,
                         "move needs 2100 with no running, alarm, released, limit or unknown bit");
    sample = s;
    return Ok();
}
// The reused profile: a fresh, complete, unretired read of this axis under the reservation. Bits of
// setupMask whose stored value already equals the request are cleared; 607A is always written.
Status checkStored(const DriverContext* stored, const AxisReservation& r, uint64_t now,
                   uint64_t age, const DriverSettings& settings, uint8_t& mask) {
    DriverObservation observed;
    if (!stored || stored->read.kind != ReadKind::DRIVER_MOTION_PARAMETERS ||
        stored->read.startedUs < r.acquiredUs ||
        !getDriver(*stored, r.target, now, age, observed) ||
        observed.group != DriverGroup::MOTION_PARAMETERS || observed.window.earliestUs < r.acquiredUs)
        return moveError(MoveError::STATE,
                         "profile reuse needs a fresh unretired MOTION_PARAMETERS read on this route");
    uint8_t m = 0xf;
    if (observed.positionSpeedRpm == settings.positionSpeedRpm)
        m &= uint8_t(~2u);
    if (observed.rampUpTimeMs == settings.rampUpTimeMs)
        m &= uint8_t(~4u);
    if (observed.rampDownTimeMs == settings.rampDownTimeMs)
        m &= uint8_t(~8u);
    mask = m;
    return Ok();
}
// 6081 is U32 rpm. Only a destination of motor turns per minute, quantized to an integer, is rpm.
Status checkSpeed(const PreparedVelocityTarget& v) {
    if (v.rateBasis != NativeRateBasis::MOTOR_TURNS || v.rateTime != TimeUnit::MINUTE ||
        v.representation != NativeRepresentation::INTEGER)
        return moveError(MoveError::RATE_BASIS,
                         "6081 is rpm: the speed must be prepared as integer motor turns per minute");
    const int64_t effective = v.effectiveNative;
    if (effective < 1 || effective > int64_t(MAXIMUM_POSITION_SPEED_RPM))
        return moveError(MoveError::RANGE, "quantized speed outside 1..3000 rpm", Err::ILLEGAL_VALUE);
    if (!v.exactArithmetic) {
        const double e = v.absoluteErrorBound;
        if (!std::isfinite(e) || e < 0 || e >= double(effective) ||
            e > double(int64_t(MAXIMUM_POSITION_SPEED_RPM) - effective))
            return moveError(MoveError::RANGE, "approximate speed may leave 1..3000 rpm",
                             Err::ILLEGAL_VALUE);
        return Ok();
    }
    const NativeQuantity& q = v.requestedNative;
    const bool positive = q.integral > 0 || (q.integral == 0 && q.numerator && !q.negative);
    const bool withinMaximum = q.integral < int64_t(MAXIMUM_POSITION_SPEED_RPM) ||
                               (q.integral == int64_t(MAXIMUM_POSITION_SPEED_RPM) &&
                                (!q.numerator || q.negative));
    if (!positive || !withinMaximum)
        return moveError(MoveError::RANGE, "requested speed outside 0..3000 rpm, or not positive",
                         Err::ILLEGAL_VALUE);
    return Ok();
}
struct Checked {
    PreparedPositionParameters parameters;
    PreparedVelocityTarget speed;
    MoveSample admission;
    bool known = false;
    int64_t displacement = 0;
    uint8_t mask = 0xf;
};
// The relative capability gate: the request's host basis must be the drive's measured native base.
// A queued base would need an edge in a running curve, which the idle admission excludes, and a
// target queue no source documents, so no evidence value can admit it.
Status checkBasis(const MoveEvidence& e, RelativeBasis basis) {
    if (e.relativeBase > RelativeBase::COMMANDED)
        return moveError(MoveError::ARGUMENT, "invalid relative base evidence");
    if (basis == RelativeBasis::QUEUED)
        return moveError(MoveError::RELATIVE_BASIS,
                         "a queued relative base needs a target queue; moves start only from idle",
                         Err::UNSUPPORTED);
    if (e.relativeBase == RelativeBase::UNMEASURED)
        return moveError(MoveError::RELATIVE_BASIS,
                         "the drive's relative base is not measured for this firmware",
                         Err::UNSUPPORTED);
    const RelativeBasis measured =
        e.relativeBase == RelativeBase::ACTUAL ? RelativeBasis::ACTUAL : RelativeBasis::COMMANDED;
    if (basis != measured)
        return moveError(MoveError::RELATIVE_BASIS,
                         "the requested basis is not the drive's measured relative base",
                         Err::UNSUPPORTED);
    return Ok();
}
// The witness rules before arithmetic. An absolute move needs none, and refuses a supplied known one
// that is not idle, stationary ACTUAL. A relative move needs one of its own basis with an age cap,
// because the drive's base at the edge must still be what it says.
Status checkWitness(const AxisReference* reference, bool relative, RelativeBasis basis) {
    if (!relative)
        return reference && reference->nativeKnown &&
                       (!reference->idle || !reference->stationary ||
                        reference->basis != RelativeBasis::ACTUAL)
                   ? moveError(MoveError::REFERENCE,
                               "a supplied witness must be idle, stationary actual command coordinates")
                   : Ok();
    if (!reference->idle || !reference->stationary || reference->basis != basis ||
        !reference->maximumAgeUs)
        return moveError(MoveError::REFERENCE,
                         "a relative witness must be idle, stationary, of the request's basis and "
                         "with an age cap");
    return Ok();
}
// Every refusal of both move preparations, in one place, writing only the caller's local result.
Status checkMove(const MoveContext& output, const AxisReservation& r, const ReadTarget& current,
                 const ReadContext& state, const CANOPEN::SdoCompletion& control,
                 const IdentityObservation& identity, const MoveEvidence& e,
                 const MoveRequest& request, const MoveProfile& profile,
                 const DriverContext* stored, const AxisConfig& axis,
                 const AxisReference* reference, bool relative, uint32_t operation, uint64_t now,
                 uint64_t deadline, const MoveOptions& options, Checked& result) {
    const void* inputs[] = {&r, &current, &state, &control, &identity, &e, &request,
                            &profile, &axis, &options, &result};
    const size_t sizes[] = {sizeof r, sizeof current, sizeof state, sizeof control,
                            sizeof identity, sizeof e, sizeof request, sizeof profile,
                            sizeof axis, sizeof options, sizeof result};
    for (unsigned i = 0; i < sizeof sizes / sizeof sizes[0]; ++i)
        if (overlaps(&output, sizeof output, inputs[i], sizes[i]))
            return moveError(MoveError::OVERLAP, "move preparation overlaps input");
    if ((stored && overlaps(&output, sizeof output, stored, sizeof *stored)) ||
        (reference && overlaps(&output, sizeof output, reference, sizeof *reference)))
        return moveError(MoveError::OVERLAP, "move preparation overlaps input");
    if (e.gate == MoveGate::CLOSED)
        return moveError(MoveError::CAPABILITY,
                         "move is gated: acceptance and completion are not established",
                         Err::UNSUPPORTED);
    if (e.gate > MoveGate::MEASURED || !e.record || !signalValid(e.acceptance) ||
        !signalValid(e.completion) || e.rule == CompletionRule::NONE ||
        e.rule > CompletionRule::AFTER_ACCEPTANCE || profile.setup > MoveSetup::REUSE_STORED)
        return moveError(MoveError::ARGUMENT, "invalid move evidence record, signals or setup");
    // Each preparation takes only its own form: a relative intent is never run as an absolute one.
    if (request.position.relative != relative || request.position.wrapped ||
        request.position.basis > RelativeBasis::QUEUED)
        return moveError(MoveError::ARGUMENT,
                         relative ? "relative move needs an unwrapped relative target"
                                  : "absolute move needs an unwrapped absolute target");
    if (profile.immediateUpdate)
        return moveError(MoveError::CAPABILITY,
                         "immediate update (003F, 007F) puts a new edge in a curve: unsupported",
                         Err::UNSUPPORTED);
    if (relative) {
        const Status basis = checkBasis(e, request.position.basis);
        if (!basis)
            return basis;
    }
    if (!validReservation(r) || !operation || deadline <= now || now > uint64_t(INT64_MAX) ||
        r.acquiredUs > now || !optionsValid(options))
        return moveError(MoveError::ARGUMENT, "invalid move reservation, identity, time or budgets");
    if (deadline - now <= serviceBudget(options, MOVE_SERVICE_STEPS))
        return moveError(MoveError::TIME,
                         "move deadline does not cover every scheduled service at its budgets");
    if (!sameTarget(r.target, current) || !sameBinding(axis.binding, r.target.channel.binding))
        return moveError(MoveError::TARGET, "move reservation, route or axis is not current");
    if (e.model != r.model || !sameTarget(identity.target, r.target) ||
        !sameFirmware(e.firmware, identity))
        return moveError(MoveError::CAPABILITY,
                         "no measured move evidence for this model, firmware and boot",
                         Err::UNSUPPORTED);
    if (!acknowledgedControlWrite(control, r, now) || controlWord(control.transfer) != REARM_WORD)
        return moveError(MoveError::STATE,
                         "move needs the actual latest acknowledged 000F under this reservation");
    Status s = checkState(state, r, control, now, options.maximumAgeUs, result.admission);
    if (!s)
        return s;
    if (reported(e.acceptance, result.admission))
        return moveError(MoveError::ACCEPTANCE,
                         "acceptance is reported before the trigger, so it cannot show this one");
    if (!profile.rampUpTimeMs || profile.rampUpTimeMs > MAXIMUM_RAMP_TIME_MS ||
        !profile.rampDownTimeMs || profile.rampDownTimeMs > MAXIMUM_RAMP_TIME_MS)
        return moveError(MoveError::RANGE, "ramp times must be 1..2000 ms; zero is unqualified",
                         Err::ILLEGAL_VALUE);
    s = checkWitness(reference, relative, request.position.basis);
    if (!s)
        return s;
    s = prepareVelocityTarget(request.speed, axis, result.speed);
    if (!s)
        return based(s, MoveError::RATE_ERROR_BASE);
    if (!sameBinding(result.speed.binding, r.target.channel.binding))
        return moveError(MoveError::TARGET, "speed was prepared for another binding");
    s = checkSpeed(result.speed);
    if (!s)
        return s;
    DriverSettings settings;
    settings.positionSpeedRpm = uint32_t(result.speed.effectiveNative);
    settings.rampUpTimeMs = profile.rampUpTimeMs;
    settings.rampDownTimeMs = profile.rampDownTimeMs;
    s = preparePositionParameters(result.parameters, r.model, request.position, axis, reference,
                                  settings, now);
    if (!s)
        return based(s, MoveError::PARAMETER_ERROR_BASE);
    const PreparedTarget& target = result.parameters.target;
    // Like the state read, a witness must describe the drive after the 000F it follows: an earlier
    // one may predate an enable or an earlier move, and its value says nothing about the base now.
    if (target.referenceUsed && target.retainedReference.observedUs < control.completedUs)
        return moveError(MoveError::REFERENCE,
                         "the witness was observed before the latest acknowledged 000F");
    result.known = target.referenceUsed && target.retainedReference.nativeKnown;
    if (result.known) {
        // R-08. For a relative move this difference is the quantized displacement itself. For an
        // absolute one the endpoint is inside the documented range and the witness inside int64,
        // so only a witness near either end of int64 can make it unrepresentable.
        const int64_t endpoint = target.endpointNative;
        const int64_t witness = target.retainedReference.nativePosition;
        if ((witness > 0 && endpoint < INT64_MIN + witness) ||
            (witness < 0 && endpoint > INT64_MAX + witness))
            return moveError(MoveError::RANGE, "displacement from the witness exceeds int64",
                             Err::ILLEGAL_VALUE);
        result.displacement = endpoint - witness;
        if (!result.displacement)
            return moveError(MoveError::ZERO_DISPLACEMENT,
                             "effective displacement is zero; nothing is written",
                             Err::ILLEGAL_VALUE);
    }
    result.mask = 0xf;
    if (profile.setup == MoveSetup::REUSE_STORED) {
        s = checkStored(stored, r, now, options.maximumAgeUs, settings, result.mask);
        if (!s)
            return s;
    }
    return Ok();
}

uint8_t nextSetupBit(uint8_t mask, uint8_t after) {
    for (uint8_t bit = uint8_t(after + 1); bit < 4; ++bit)
        if (mask & (1u << bit))
            return bit;
    return 4;
}
bool observationMatches(const MoveContext& c, const ReadContext& read) {
    return read.operation == c.operation && read.kind == ReadKind::STATE &&
           read.source == ReadSource::SDO && sameTarget(read.target, c.reservation.target) &&
           read.blockMask == OBSERVATION_MASK && read.count == MOVE_OBSERVATION_FIELDS &&
           read.deadlineUs == c.deadlineUs && read.startedUs == c.observationStartedUs &&
           read.options.queueTimeoutUs == c.options.service.queueTimeoutUs &&
           read.options.responseTimeoutUs == c.options.service.responseTimeoutUs;
}
bool waitingPhase(MovePhase p) {
    return p == MovePhase::WAITING || p == MovePhase::OBSERVING || p == MovePhase::REARM;
}
bool downloading(const MoveContext& c) {
    return c.phase == MovePhase::PARAMETERS || c.phase == MovePhase::TRIGGER ||
           c.phase == MovePhase::REARM;
}
Status shape(const MoveContext& c, uint64_t now) {
    const MoveOptions& o = c.options;
    if (!validReservation(c.reservation) || !c.operation || !c.step ||
        c.state == ActionState::EMPTY || c.state > ActionState::FAILED ||
        c.phase > MovePhase::REARM || c.outcome == ActionOutcome::STOP_SENT_UNCONFIRMED ||
        c.outcome > ActionOutcome::SERVICE_TIMEOUT || c.interruption > MoveInterruption::DEADLINE ||
        c.withdrawal > MoveInterruption::DEADLINE || c.evidence.gate != MoveGate::MEASURED ||
        c.setup > ActionExecution::UNKNOWN || c.execution > ActionExecution::UNKNOWN ||
        c.rearm > ActionExecution::UNKNOWN || c.completion > ActionCompletion::OBSERVED ||
        !o.service.queueTimeoutUs || !o.service.responseTimeoutUs || !o.observationIntervalUs ||
        !o.maximumAgeUs || c.deadlineUs <= c.startedUs || c.servicedUs < c.startedUs ||
        now < c.servicedUs || !(c.setupMask & 1u) || (c.setupMask & ~0xfu) || c.setupCursor > 3 ||
        !(c.setupMask & (1u << c.setupCursor)) || (c.setupEvidenceMask & ~c.setupMask) ||
        (c.parameters.target.requested.relative &&
         (c.evidence.relativeBase == RelativeBase::UNMEASURED ||
          c.evidence.relativeBase > RelativeBase::COMMANDED || !c.parameters.target.referenceUsed)) ||
        (c.phase == MovePhase::PARAMETERS && c.triggerPresent) ||
        (waitingPhase(c.phase) && (!c.triggerPresent || c.execution != ActionExecution::ACKNOWLEDGED)) ||
        (c.phase == MovePhase::REARM && c.completion != ActionCompletion::OBSERVED))
        return moveError(MoveError::STATE, "invalid move context or time");
    return Ok();
}
Status currentMove(const MoveContext& c, const AxisReservation& live, const ReadTarget& target,
                   uint64_t now) {
    const Status s = shape(c, now);
    if (!s)
        return s;
    if (!sameReservation(c.reservation, live) || !sameTarget(c.reservation.target, target))
        return moveError(MoveError::TARGET, "move reservation or target changed");
    return Ok();
}
CANOPEN::SdoTransfer download(const MoveContext& c) {
    CANOPEN::SdoTransfer t;
    t.channel = c.reservation.target.channel;
    t.operation = t.request = c.operation;
    t.step = c.step;
    t.direction = CANOPEN::SdoDirection::DOWNLOAD;
    t.deadlineUs = c.deadlineUs;
    t.queueTimeoutUs = c.options.service.queueTimeoutUs;
    t.responseTimeoutUs = c.options.service.responseTimeoutUs;
    const ObjectDescriptor* object = &OBJECT_6040_00;
    const uint16_t trigger =
        c.parameters.target.requested.relative ? RELATIVE_TRIGGER_WORD : TRIGGER_WORD;
    uint32_t value = c.phase == MovePhase::TRIGGER ? trigger : REARM_WORD;
    if (c.phase == MovePhase::PARAMETERS) {
        const DriverSettings& s = c.parameters.settings;
        object = SETUP[c.setupCursor];
        value = c.setupCursor == 0   ? uint32_t(c.parameters.targetNative)
                : c.setupCursor == 1 ? s.positionSpeedRpm
                : c.setupCursor == 2 ? s.rampUpTimeMs
                                     : s.rampDownTimeMs;
    }
    t.index = object->index;
    t.subindex = object->subindex;
    t.size = object->widthBytes;
    for (unsigned i = 0; i < t.size; ++i)
        t.data[i] = uint8_t(value >> (8 * i));
    return t;
}
ActionOutcome outcomeOf(MoveInterruption reason) {
    switch (reason) {
    case MoveInterruption::DRIVE_FAULT:
        return ActionOutcome::DRIVE_FAULT;
    case MoveInterruption::INVALIDATED:
        return ActionOutcome::INVALIDATED;
    case MoveInterruption::DEADLINE:
        return ActionOutcome::DEADLINE;
    default:
        return ActionOutcome::CANCELLED; // CANCELLED and STOP: a stop is its own operation.
    }
}
ActionOutcome outcomeOf(CANOPEN::SdoOutcome o) {
    switch (o) {
    case CANOPEN::SdoOutcome::SERVER_ABORT:
        return ActionOutcome::PROTOCOL_REJECTED;
    case CANOPEN::SdoOutcome::TIMEOUT:
        // Every caller settles the overall cutoff first, so this is a queue or response budget.
        return ActionOutcome::SERVICE_TIMEOUT;
    case CANOPEN::SdoOutcome::CANCELLED:
        return ActionOutcome::CANCELLED;
    case CANOPEN::SdoOutcome::REBOOT:
    case CANOPEN::SdoOutcome::BINDING_INVALIDATED:
        return ActionOutcome::INVALIDATED;
    case CANOPEN::SdoOutcome::CLIENT_ABORT:
    case CANOPEN::SdoOutcome::UNSUPPORTED:
        return ActionOutcome::REPLY_ERROR;
    default:
        return ActionOutcome::TRANSPORT_ERROR;
    }
}
ActionOutcome outcomeOf(ReadOutcome o) {
    switch (o) {
    case ReadOutcome::PROTOCOL_REJECTED:
        return ActionOutcome::PROTOCOL_REJECTED;
    case ReadOutcome::REPLY_ERROR:
        return ActionOutcome::REPLY_ERROR;
    case ReadOutcome::CANCELLED:
        return ActionOutcome::CANCELLED;
    case ReadOutcome::DEADLINE:
        return ActionOutcome::DEADLINE;
    case ReadOutcome::SERVICE_TIMEOUT:
        return ActionOutcome::SERVICE_TIMEOUT;
    case ReadOutcome::INVALIDATED:
        return ActionOutcome::INVALIDATED;
    default:
        return ActionOutcome::TRANSPORT_ERROR;
    }
}
// Possible and uncertain effects, recomputed from every retained download terminal so each terminal
// refines its own possible-TX notice. A possibly sent trigger without an observed completion means
// the drive may still be moving, which no later local event can settle.
void settleEffects(MoveContext& c) {
    bool possible = c.transmissionNoted, uncertain = c.transmissionNoted;
    for (unsigned i = 0; i < 4; ++i)
        if (c.setupEvidenceMask & (1u << i)) {
            possible |= c.setupHistory[i].effectPossible;
            uncertain |= c.setupHistory[i].effectUncertain;
        }
    const CANOPEN::SdoCompletion* ends[] = {c.triggerPresent ? &c.trigger : nullptr,
                                            c.rearmPresent ? &c.rearmEvidence : nullptr};
    for (const CANOPEN::SdoCompletion* e : ends)
        if (e) {
            possible |= e->effectPossible;
            uncertain |= e->effectUncertain;
        }
    const bool triggerPossible =
        c.execution == ActionExecution::UNKNOWN || c.execution == ActionExecution::ACKNOWLEDGED;
    c.effectsPossible = possible;
    c.uncertain = uncertain || c.readUncertain ||
                  (triggerPossible && c.completion != ActionCompletion::OBSERVED);
}
void fail(MoveContext& c, ActionOutcome reason, uint64_t now, const char* message) {
    c.state = ActionState::FAILED;
    c.outcome = reason;
    c.status = reason == ActionOutcome::PROTOCOL_REJECTED
                   ? Status(Err::PROTOCOL_ERROR, 0, message)
                   : moveError(reason == ActionOutcome::DEADLINE ||
                                       reason == ActionOutcome::SERVICE_TIMEOUT
                                   ? MoveError::TIME
                                   : MoveError::STATE,
                               message);
    c.servicedUs = now;
    settleEffects(c);
}
// A finished cycle: freshness after the trigger, then scope, then the frozen evidence's rule.
void finishCycle(MoveContext& c, const ReadContext& read, uint64_t now, bool anotherStep) {
    const ReadTarget& target = c.reservation.target;
    const uint64_t age = c.options.maximumAgeUs;
    const uint64_t after = c.trigger.completedUs;
    ReadWindow monitors, status, feedback;
    if (!validateStateBlock(read, StateBlock::MONITORS, target, now, age, monitors) ||
        !validateStateBlock(read, StateBlock::STATUS, target, now, age, status) ||
        !validateStateBlock(read, StateBlock::FEEDBACK, target, now, age, feedback) ||
        monitors.earliestUs < after || status.earliestUs < after || feedback.earliestUs < after) {
        fail(c, ActionOutcome::OBSERVATION_SCOPE, now, "move observation freshness unavailable");
        return;
    }
    MoveSample s;
    s.present = s.feedback = true;
    s.cycle = ++c.observations; // Distinct identities per cycle bound this below UINT32_MAX.
    s.statusword = uint16_t(raw(read, StateBlock::STATUS, 0));
    s.modeRaw = uint8_t(raw(read, StateBlock::STATUS, 1));
    s.monitor = uint16_t(raw(read, StateBlock::MONITORS, 0));
    s.position = signed32(raw(read, StateBlock::FEEDBACK, 0));
    s.velocity = signed32(raw(read, StateBlock::FEEDBACK, 1));
    s.earliestUs = monitors.earliestUs;
    s.latestUs = monitors.latestUs;
    bound(s, status);
    bound(s, feedback);
    c.latest = s;
    if (s.modeRaw != 1 || (s.statusword & STATUS_LOW_BITS) != STATUS_LOW_BITS ||
        (s.monitor & SCOPE_MONITOR_BITS) || (s.monitor & uint16_t(~MONITOR_DOCUMENTED_BITS))) {
        fail(c, ActionOutcome::OBSERVATION_SCOPE, now,
             "drive report outside the move's supported observation scope");
        return;
    }
    const bool accepted = reported(c.evidence.acceptance, s);
    const bool complete = reported(c.evidence.completion, s);
    if (accepted && !c.acceptance.present)
        c.acceptance = s;
    if (!complete && !c.absence.present)
        c.absence = s;
    const bool fresh = c.evidence.rule == CompletionRule::AFTER_ABSENCE
                           ? c.absence.present && c.absence.cycle < s.cycle
                           : c.acceptance.cycle < s.cycle;
    if (complete && c.acceptance.present && fresh) {
        c.completion = ActionCompletion::OBSERVED;
        c.completed = s;
        if (!anotherStep) {
            fail(c, ActionOutcome::ITERATION_EXHAUSTED, now,
                 "completion observed; no identity is left for the 000F step");
            return;
        }
        c.phase = MovePhase::REARM;
        c.transmissionNoted = false;
        c.eligibleUs = c.stepStartedUs = now;
        return;
    }
    if (!anotherStep) {
        fail(c, ActionOutcome::ITERATION_EXHAUSTED, now, "move service identity exhausted");
        return;
    }
    const uint64_t remaining = c.deadlineUs - now;
    c.eligibleUs = c.options.observationIntervalUs >= remaining ? c.deadlineUs
                                                                : now + c.options.observationIntervalUs;
    c.stepStartedUs = c.eligibleUs;
    c.phase = MovePhase::WAITING;
}
} // namespace

namespace {
// Both preparations after checkMove passed. Nothing here can fail, and output is disjoint from
// every input.
void publishMove(MoveContext& output, const Checked& result, const AxisReservation& reservation,
                 const CANOPEN::SdoCompletion& control, const MoveEvidence& evidence,
                 const MoveProfile& profile, uint32_t operation, uint64_t nowUs,
                 uint64_t deadlineUs, const MoveOptions& options) {
    new (static_cast<void*>(&output)) MoveContext();
    MoveContext& c = output;
    c.reservation = reservation;
    c.evidence = evidence;
    c.profile = profile;
    c.options = options;
    c.parameters = result.parameters;
    c.speed = result.speed;
    c.displacement = result.known ? MoveDisplacement::KNOWN : MoveDisplacement::UNKNOWN;
    c.displacementNative = result.displacement;
    c.state = ActionState::ACTIVE;
    c.phase = MovePhase::PARAMETERS;
    c.operation = operation;
    c.startedUs = c.servicedUs = c.eligibleUs = c.stepStartedUs = nowUs;
    c.deadlineUs = deadlineUs;
    c.setupMask = result.mask;
    c.control = control;
    c.admission = result.admission;
}
} // namespace

Status validateMoveAbsolute(const MoveContext& output, const AxisReservation& reservation,
                            const ReadTarget& current, const ReadContext& state,
                            const CANOPEN::SdoCompletion& control,
                            const IdentityObservation& identity, const MoveEvidence& evidence,
                            const MoveRequest& request, const MoveProfile& profile,
                            const DriverContext* stored, const AxisConfig& axis,
                            const AxisReference* reference, uint32_t operation, uint64_t nowUs,
                            uint64_t deadlineUs, const MoveOptions& options) noexcept {
    Checked result;
    return checkMove(output, reservation, current, state, control, identity, evidence, request,
                     profile, stored, axis, reference, false, operation, nowUs, deadlineUs, options,
                     result);
}

Status prepareMoveAbsolute(MoveContext& output, const AxisReservation& reservation,
                           const ReadTarget& current, const ReadContext& state,
                           const CANOPEN::SdoCompletion& control, const IdentityObservation& identity,
                           const MoveEvidence& evidence, const MoveRequest& request,
                           const MoveProfile& profile, const DriverContext* stored,
                           const AxisConfig& axis, const AxisReference* reference,
                           uint32_t operation, uint64_t nowUs, uint64_t deadlineUs,
                           const MoveOptions& options) noexcept {
    Checked result;
    const Status s = checkMove(output, reservation, current, state, control, identity, evidence,
                               request, profile, stored, axis, reference, false, operation, nowUs,
                               deadlineUs, options, result);
    if (s)
        publishMove(output, result, reservation, control, evidence, profile, operation, nowUs,
                    deadlineUs, options);
    return s;
}

Status validateMoveRelative(const MoveContext& output, const AxisReservation& reservation,
                            const ReadTarget& current, const ReadContext& state,
                            const CANOPEN::SdoCompletion& control,
                            const IdentityObservation& identity, const MoveEvidence& evidence,
                            const MoveRequest& request, const MoveProfile& profile,
                            const DriverContext* stored, const AxisConfig& axis,
                            const AxisReference& reference, uint32_t operation, uint64_t nowUs,
                            uint64_t deadlineUs, const MoveOptions& options) noexcept {
    Checked result;
    return checkMove(output, reservation, current, state, control, identity, evidence, request,
                     profile, stored, axis, &reference, true, operation, nowUs, deadlineUs, options,
                     result);
}

Status prepareMoveRelative(MoveContext& output, const AxisReservation& reservation,
                           const ReadTarget& current, const ReadContext& state,
                           const CANOPEN::SdoCompletion& control, const IdentityObservation& identity,
                           const MoveEvidence& evidence, const MoveRequest& request,
                           const MoveProfile& profile, const DriverContext* stored,
                           const AxisConfig& axis, const AxisReference& reference,
                           uint32_t operation, uint64_t nowUs, uint64_t deadlineUs,
                           const MoveOptions& options) noexcept {
    Checked result;
    const Status s = checkMove(output, reservation, current, state, control, identity, evidence,
                               request, profile, stored, axis, &reference, true, operation, nowUs,
                               deadlineUs, options, result);
    if (s)
        publishMove(output, result, reservation, control, evidence, profile, operation, nowUs,
                    deadlineUs, options);
    return s;
}

void resetMove(MoveContext& c) noexcept {
    new (static_cast<void*>(&c)) MoveContext();
}

// The current work of an active move. A withdrawn one, or a download already noted, still has work
// in flight: nextMove yields nothing new for it, while advanceMove needs the identity to correlate
// its terminal.
static Status yieldMove(const MoveContext& c, const ReadContext& observation,
                        const AxisReservation& live, const ReadTarget& target, uint64_t now,
                        PreparedMove& output, bool correlating) noexcept {
    if (overlaps(&output, sizeof output, &c, sizeof c) ||
        overlaps(&output, sizeof output, &observation, sizeof observation) ||
        overlaps(&output, sizeof output, &live, sizeof live) ||
        overlaps(&output, sizeof output, &target, sizeof target))
        return moveError(MoveError::OVERLAP, "move work overlaps input");
    const Status s = currentMove(c, live, target, now);
    if (!s)
        return s;
    if (c.state == ActionState::ACTIVE && now >= c.deadlineUs)
        return moveError(MoveError::TIME, "move overall deadline");
    PreparedMove next;
    next.eligibleUs = c.eligibleUs;
    // No new observation cycle or read once its budget no longer fits: the move then ends at its
    // deadline with nothing in flight. Downloads keep their own cutoff, and correlation rebuilds work
    // already handed out, so neither is affected.
    const bool late = !correlating && !downloading(c) &&
                      !exchangeFits(c.deadlineUs, now, c.options.service.queueTimeoutUs,
                                    c.options.service.responseTimeoutUs);
    if (c.state != ActionState::ACTIVE || now < c.eligibleUs || late ||
        (!correlating && (c.withdrawal != MoveInterruption::NONE ||
                          (downloading(c) && c.transmissionNoted)))) {
        output = next;
        return Ok();
    }
    if (downloading(c)) {
        next.ready = true;
        next.transfer = download(c);
    } else if (c.phase == MovePhase::WAITING) {
        next.observe = true;
    } else {
        if (!observationMatches(c, observation))
            return moveError(MoveError::CORRELATION, "observation read is not this move's cycle");
        PreparedRead read;
        const Status readStatus = nextRead(observation, now, read);
        if (!readStatus)
            return readStatus;
        if (!read.ready)
            return moveError(MoveError::STATE, "move observation has no pending field");
        next.ready = true;
        next.transfer = read.transfer;
        next.transfer.step = c.step;
    }
    output = next;
    return Ok();
}

Status nextMove(const MoveContext& c, const ReadContext& observation, const AxisReservation& live,
                const ReadTarget& current, uint64_t now, PreparedMove& output) noexcept {
    return yieldMove(c, observation, live, current, now, output, false);
}

Status beginMoveObservation(MoveContext& c, ReadContext& observation, const AxisReservation& live,
                            const ReadTarget& current, uint64_t now) noexcept {
    if (overlaps(&observation, sizeof observation, &c, sizeof c) ||
        overlaps(&observation, sizeof observation, &live, sizeof live) ||
        overlaps(&observation, sizeof observation, &current, sizeof current) ||
        overlaps(&c, sizeof c, &live, sizeof live) || overlaps(&c, sizeof c, &current, sizeof current))
        return moveError(MoveError::OVERLAP, "move observation overlaps input");
    Status s = currentMove(c, live, current, now);
    if (!s)
        return s;
    if (c.state != ActionState::ACTIVE || c.phase != MovePhase::WAITING ||
        c.withdrawal != MoveInterruption::NONE || now < c.eligibleUs)
        return moveError(MoveError::STATE, "no due move observation");
    if (now >= c.deadlineUs)
        return moveError(MoveError::TIME, "move overall deadline");
    // Atomic on the read, and the only refusal left.
    s = prepareState(observation, c.reservation.target, c.operation, now, c.deadlineUs,
                     c.options.service, OBSERVATION_MASK);
    if (!s)
        return s;
    c.phase = MovePhase::OBSERVING;
    c.observationStartedUs = c.stepStartedUs = c.servicedUs = now;
    return Ok();
}

namespace {
// The recheck itself, for a context already known to be an active parameter or trigger download.
// What preparation checked must still hold at the move's actual admission, not only when the move
// was prepared, and again when the frame is handed over.
Status recheckAdmission(const MoveContext& c, const AxisConfig& axis,
                        const AxisReference* reference, uint64_t now) {
    Status s = validatePreparedTarget(c.parameters.target, axis, reference, now);
    if (!s) {
        s.detail += int32_t(PositionParameterError::AXIS_ERROR_BASE) +
                    int32_t(MoveError::PARAMETER_ERROR_BASE);
        return s;
    }
    // validatePreparedTarget has refused a used witness that is missing.
    if (c.parameters.target.referenceUsed && (!reference->idle || !reference->stationary))
        return moveError(MoveError::REFERENCE,
                         "the move's witness is no longer idle and stationary");
    if (now - c.admission.earliestUs > c.options.maximumAgeUs)
        return moveError(MoveError::STATE, "the admission state read is older than its age cap");
    return Ok();
}
} // namespace

Status validateMoveAdmission(const MoveContext& c, const AxisConfig& axis,
                             const AxisReference* reference, uint64_t now) noexcept {
    if (overlaps(&c, sizeof c, &axis, sizeof axis) ||
        (reference && overlaps(&c, sizeof c, reference, sizeof *reference)))
        return moveError(MoveError::OVERLAP, "move admission recheck overlaps input");
    const Status s = shape(c, now);
    if (!s)
        return s;
    if (c.state != ActionState::ACTIVE ||
        (c.phase != MovePhase::PARAMETERS && c.phase != MovePhase::TRIGGER))
        return moveError(MoveError::STATE, "no parameter or trigger download to recheck");
    return recheckAdmission(c, axis, reference, now);
}

Status noteMoveTransmission(MoveContext& c, const AxisReservation& live, const ReadTarget& current,
                            const AxisConfig& axis, const AxisReference* reference,
                            DriverContext* const* retained, uint8_t count, uint64_t now) noexcept {
    if (overlaps(&c, sizeof c, &live, sizeof live) || overlaps(&c, sizeof c, &current, sizeof current) ||
        overlaps(&c, sizeof c, &axis, sizeof axis) ||
        (reference && overlaps(&c, sizeof c, reference, sizeof *reference)))
        return moveError(MoveError::OVERLAP, "move TX notice overlaps input");
    if (count && !retained)
        return moveError(MoveError::ARGUMENT, "retained settings list missing");
    for (unsigned i = 0; i < count; ++i)
        if (!retained[i] || overlaps(retained[i], sizeof *retained[i], &c, sizeof c) ||
            overlaps(retained[i], sizeof *retained[i], &live, sizeof live))
            return moveError(MoveError::ARGUMENT, "retained settings entry missing or aliased");
    Status s = currentMove(c, live, current, now);
    if (!s)
        return s;
    if (c.state != ActionState::ACTIVE || !downloading(c) || c.transmissionNoted ||
        now >= c.deadlineUs || now < c.eligibleUs || c.withdrawal != MoveInterruption::NONE)
        return moveError(MoveError::STATE, "no new live move download");
    if (c.phase != MovePhase::REARM) {
        // The actual admission of a parameter or trigger write. The rearm follows an observed
        // completion and takes no recheck.
        s = recheckAdmission(c, axis, reference, now);
        if (!s)
            return s;
    }
    if (c.phase == MovePhase::PARAMETERS) {
        // The packet 10 hook: an operation's own write never moves the configuration epoch, so the
        // move's target stays valid, but every retained MOTION_PARAMETERS observation is withdrawn.
        const CANOPEN::SdoTransfer work = download(c);
        for (unsigned i = 0; i < count; ++i) {
            s = retireMotionParameters(*retained[i], current, work);
            if (!s)
                return s;
        }
    }
    c.transmissionNoted = c.effectsPossible = c.uncertain = true;
    (c.phase == MovePhase::PARAMETERS ? c.setup
     : c.phase == MovePhase::TRIGGER  ? c.execution
                                      : c.rearm) = ActionExecution::UNKNOWN;
    c.servicedUs = now;
    return Ok();
}

Status noteMoveWithdrawal(MoveContext& c, const AxisReservation& live, const ReadTarget& current,
                          MoveInterruption reason, uint64_t now) noexcept {
    if (overlaps(&c, sizeof c, &live, sizeof live) || overlaps(&c, sizeof c, &current, sizeof current))
        return moveError(MoveError::OVERLAP, "move withdrawal overlaps input");
    const Status s =
        reason == MoveInterruption::INVALIDATED ? shape(c, now) : currentMove(c, live, current, now);
    if (!s)
        return s;
    if (c.state != ActionState::ACTIVE || c.withdrawal != MoveInterruption::NONE ||
        reason == MoveInterruption::NONE || reason >= MoveInterruption::DEADLINE)
        return moveError(MoveError::STATE, "invalid move withdrawal");
    // servicedUs stays: the terminal in flight may have completed before this call.
    c.withdrawal = now >= c.deadlineUs ? MoveInterruption::DEADLINE : reason;
    return Ok();
}

Status advanceMove(MoveContext& c, ReadContext& observation, const AxisReservation& live,
                   const ReadTarget& current, const CANOPEN::SdoCompletion& input,
                   uint64_t now) noexcept {
    const void* inputs[] = {&live, &current, &input};
    const size_t sizes[] = {sizeof live, sizeof current, sizeof input};
    for (unsigned i = 0; i < 3; ++i)
        if (overlaps(&c, sizeof c, inputs[i], sizes[i]) ||
            overlaps(&observation, sizeof observation, inputs[i], sizes[i]))
            return moveError(MoveError::OVERLAP, "move terminal overlaps input");
    if (overlaps(&c, sizeof c, &observation, sizeof observation))
        return moveError(MoveError::OVERLAP, "move terminal overlaps input");
    Status s = currentMove(c, live, current, now);
    if (!s)
        return s;
    if (c.state != ActionState::ACTIVE)
        return moveError(MoveError::STATE, "move already terminal");
    PreparedMove work;
    // As with ordinary reads, validate correlation before consuming a deadline.
    s = yieldMove(c, observation, live, current, c.servicedUs < c.eligibleUs ? c.eligibleUs : c.servicedUs,
                  work, true);
    if (!s)
        return s;
    if (!work.ready || !CANOPEN::sameSdoWork(input.transfer, work.transfer) ||
        input.nodeGeneration != c.reservation.target.nodeGeneration ||
        input.admittedUs < c.stepStartedUs || input.admittedUs >= c.deadlineUs ||
        input.completedUs < input.admittedUs || input.completedUs > now ||
        input.completedUs < c.servicedUs)
        return moveError(MoveError::CORRELATION, "move terminal does not match current service");
    // A withdrawn move keeps this terminal as evidence, and nothing in it can advance the move.
    const bool withdrawn = c.withdrawal != MoveInterruption::NONE;
    if (c.phase == MovePhase::OBSERVING) {
        CANOPEN::SdoCompletion local = input;
        local.transfer.step = uint32_t(observation.cursor) + 1;
        s = advanceRead(observation, local, now); // Atomic on refusal; nothing else has moved.
        if (!s)
            return s;
        c.readUncertain |= observation.uncertain;
        c.observationEvidence = input;
        c.observationEvidencePresent = true;
        if (withdrawn) {
        } else if (observation.state == ReadState::FAILED) {
            fail(c, outcomeOf(observation.outcome), now, "move observation service failed");
        } else if (c.step == UINT32_MAX) {
            // The last representable identity may still close a cycle; only later work is exhausted.
            if (observation.state == ReadState::SUCCEEDED)
                finishCycle(c, observation, now, false);
            else
                fail(c, ActionOutcome::ITERATION_EXHAUSTED, now, "move service identity exhausted");
        } else {
            ++c.step;
            c.stepStartedUs = now;
            if (observation.state == ReadState::SUCCEEDED)
                finishCycle(c, observation, now, true);
        }
        settleEffects(c);
    } else {
        if (!CANOPEN::checkDownloadTerminal(input, c.transmissionNoted))
            return moveError(MoveError::CORRELATION, "inconsistent move download terminal");
        const bool success = input.outcome == CANOPEN::SdoOutcome::SUCCESS;
        const bool expired = now >= c.deadlineUs || input.completedUs >= c.deadlineUs;
        const ActionExecution execution =
            success ? ActionExecution::ACKNOWLEDGED
            : input.outcome == CANOPEN::SdoOutcome::SERVER_ABORT ? ActionExecution::REJECTED
            : input.transmissionPossible ? ActionExecution::UNKNOWN
                                         : ActionExecution::NOT_TRANSMITTED;
        const uint8_t following =
            c.phase == MovePhase::PARAMETERS ? nextSetupBit(c.setupMask, c.setupCursor) : 4;
        if (c.phase == MovePhase::PARAMETERS) {
            bool earlier = false; // An earlier parameter may have reached the drive.
            for (unsigned i = 0; i < c.setupCursor; ++i)
                earlier |= (c.setupEvidenceMask & (1u << i)) && c.setupHistory[i].effectPossible;
            c.setupHistory[c.setupCursor] = input;
            c.setupEvidenceMask = uint8_t(c.setupEvidenceMask | (1u << c.setupCursor));
            // ACKNOWLEDGED only once every planned download was; a partial setup stays UNKNOWN,
            // and setupHistory says which parameters were acknowledged.
            c.setup = success ? (following < 4 ? ActionExecution::UNKNOWN : execution)
                      : earlier ? ActionExecution::UNKNOWN
                                : execution;
        } else if (c.phase == MovePhase::TRIGGER) {
            c.trigger = input;
            c.triggerPresent = true;
            c.execution = execution;
        } else {
            c.rearmEvidence = input;
            c.rearmPresent = true;
            c.rearm = execution;
        }
        c.transmissionNoted = false;
        if (withdrawn) {
        } else if (expired)
            fail(c, ActionOutcome::DEADLINE, now, "move overall deadline");
        else if (!success)
            fail(c, outcomeOf(input.outcome), now, "move download failed; never retried");
        else if (c.phase == MovePhase::REARM) {
            c.state = ActionState::SUCCEEDED;
            c.outcome = ActionOutcome::OBSERVED;
            c.status = Ok();
        } else if (c.step == UINT32_MAX)
            fail(c, ActionOutcome::ITERATION_EXHAUSTED, now, "move service identity exhausted");
        else {
            if (c.phase == MovePhase::TRIGGER)
                c.phase = MovePhase::WAITING; // The first cycle is due at once.
            else if (following < 4)
                c.setupCursor = following;
            else
                c.phase = MovePhase::TRIGGER;
            ++c.step;
            c.eligibleUs = c.stepStartedUs = now;
        }
        settleEffects(c);
    }
    if (withdrawn) {
        c.interruption = c.withdrawal;
        fail(c, outcomeOf(c.withdrawal), now, "move withdrawn; the terminal in flight is kept as evidence");
    }
    c.servicedUs = now;
    return Ok();
}

Status advanceMove(MoveContext& c, ReadContext& observation, const AxisReservation& live,
                   const ReadTarget& current, MoveInterruption reason, uint64_t now) noexcept {
    if (overlaps(&c, sizeof c, &live, sizeof live) || overlaps(&c, sizeof c, &current, sizeof current) ||
        overlaps(&c, sizeof c, &observation, sizeof observation) ||
        overlaps(&observation, sizeof observation, &live, sizeof live) ||
        overlaps(&observation, sizeof observation, &current, sizeof current))
        return moveError(MoveError::OVERLAP, "move withdrawal overlaps input");
    Status s =
        reason == MoveInterruption::INVALIDATED ? shape(c, now) : currentMove(c, live, current, now);
    if (!s)
        return s;
    if (c.state != ActionState::ACTIVE || reason == MoveInterruption::NONE ||
        reason > MoveInterruption::DEADLINE ||
        (reason == MoveInterruption::DEADLINE && now < c.deadlineUs))
        return moveError(MoveError::STATE, "invalid local move termination");
    const bool matches = observationMatches(c, observation);
    if (c.phase == MovePhase::OBSERVING && !matches && reason != MoveInterruption::INVALIDATED)
        return moveError(MoveError::CORRELATION, "observation read is not this move's cycle");
    if (c.phase == MovePhase::OBSERVING && matches && observation.state == ReadState::ACTIVE &&
        now >= c.eligibleUs) {
        const ReadOutcome readReason = reason == MoveInterruption::INVALIDATED ? ReadOutcome::INVALIDATED
                                       : reason == MoveInterruption::DEADLINE  ? ReadOutcome::DEADLINE
                                                                               : ReadOutcome::CANCELLED;
        // Atomic on the read, and nothing else has moved yet.
        s = advanceRead(observation, readReason, now);
        if (!s)
            return s;
        c.readUncertain |= observation.uncertain;
    }
    if (now >= c.deadlineUs)
        reason = MoveInterruption::DEADLINE;
    c.interruption = reason;
    fail(c, outcomeOf(reason), now, "move ended locally; nothing is replayed, rearmed or stopped");
    return Ok();
}
} // namespace LICHUAN_C
} // namespace MotorControlCAN
