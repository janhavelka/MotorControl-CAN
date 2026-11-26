// SPDX-License-Identifier: MIT
// The library evidence a measured record builds, and the checks of a request against it (prompt 58
// slice b2). A check runs before the library's own preparation and computes the native values with
// the same pure functions and inputs that preparation uses, so it judges exactly what would be
// written; the preparation then applies every library limit as before.
#include "../../canopen/SdoEvidence.h"
#include <MotorControlCAN/profiles/lichuan_c/Records.h>

namespace MotorControlCAN {
namespace LICHUAN_C {
namespace {
using CANOPEN::overlaps;

Status refuse(RecordCheckError detail, const char* message, Err code = Err::INVALID_CONFIG) {
    return Status(code, int32_t(detail), message);
}
Status based(Status s, RecordCheckError base) {
    s.detail += int32_t(base);
    return s;
}
bool measured(RecordBasis basis) { return basis == RecordBasis::MEASURED; }
// A valid record of the wanted kind, and its id.
Status recordOf(const MeasuredRecord& record, RecordKind kind, uint32_t& id) {
    const Status s = recordId(id, record);
    if (!s)
        return Status(s.code, int32_t(RecordCheckError::RECORD), s.msg);
    if (record.kind != kind)
        return refuse(RecordCheckError::KIND, "the record covers another operation");
    return Ok();
}
bool inRange(uint64_t value, const RecordRange& range) {
    return value >= range.minimum && value <= range.maximum;
}
bool sameIdentity(const IdentityValues& a, const IdentityValues& b) {
    return a.deviceType == b.deviceType && a.hardwareVersion == b.hardwareVersion &&
           a.softwareVersion == b.softwareVersion && a.vendorId == b.vendorId &&
           a.productCode == b.productCode && a.revision == b.revision &&
           a.modelCode == b.modelCode && a.firmwareCode == b.firmwareCode;
}
template <class Firmware> void copyIdentity(Firmware& f, const IdentityValues& i) {
    f.deviceType = i.deviceType;
    f.hardwareVersion = i.hardwareVersion;
    f.softwareVersion = i.softwareVersion;
    f.vendorId = i.vendorId;
    f.productCode = i.productCode;
    f.revision = i.revision;
    f.modelCode = i.modelCode;
    f.firmwareCode = i.firmwareCode;
}
template <class Firmware> bool sameFirmware(const Firmware& a, const Firmware& b) {
    return a.deviceType == b.deviceType && a.hardwareVersion == b.hardwareVersion &&
           a.softwareVersion == b.softwareVersion && a.vendorId == b.vendorId &&
           a.productCode == b.productCode && a.revision == b.revision &&
           a.modelCode == b.modelCode && a.firmwareCode == b.firmwareCode;
}
bool sameSignal(const MoveSignal& a, const MoveSignal& b) {
    return a.source == b.source && a.mask == b.mask && a.value == b.value;
}
uint64_t magnitude(int64_t value) {
    return value < 0 ? uint64_t(0) - uint64_t(value) : uint64_t(value);
}
// The integer rpm a 6081 or 60FF write carries, or a refusal.
Status rpmOf(const VelocityRequest& request, const AxisConfig& axis, int64_t& rpm) {
    PreparedVelocityTarget rate;
    const Status s = prepareVelocityTarget(request, axis, rate);
    if (!s)
        return based(s, RecordCheckError::RATE_ERROR_BASE);
    if (rate.rateBasis != NativeRateBasis::MOTOR_TURNS || rate.rateTime != TimeUnit::MINUTE ||
        rate.representation != NativeRepresentation::INTEGER)
        return refuse(RecordCheckError::RATE_BASIS, "the rate is not integer motor turns per minute");
    rpm = rate.effectiveNative;
    return Ok();
}
bool rampsWithin(uint32_t up, uint32_t down, const RecordRange& range) {
    return inRange(up, range) && inRange(down, range);
}
// The fresh 6041/2100 pair and 6061 of a state read, as the preparations read them.
Status startReport(const ReadContext& state, const ReadTarget& current, uint64_t now, uint64_t age,
                   StartReport& report, uint8_t& mode) {
    // One observation reused for both blocks: it is about 0.5 KB, and these checks run on console stacks.
    StateObservation block;
    if (!getStateBlock(state, StateBlock::STATUS, current, now, age, block))
        return refuse(RecordCheckError::STATE, "the state read has no current STATUS and MONITORS");
    const uint16_t statusword = block.statusword;
    const uint8_t modeRaw = block.modeRaw;
    if (!getStateBlock(state, StateBlock::MONITORS, current, now, age, block))
        return refuse(RecordCheckError::STATE, "the state read has no current STATUS and MONITORS");
    report.statusword = statusword;
    report.monitor = block.monitor;
    mode = modeRaw;
    return Ok();
}
bool admitted(const RecordStarts& starts, const StartReport& report) {
    for (unsigned i = 0; i < starts.count; ++i)
        if (starts.reports[i].statusword == report.statusword &&
            starts.reports[i].monitor == report.monitor)
            return true;
    return false;
}
const RecordStop& stopOf(const VelocityScope& v, StopKind kind) {
    return kind == StopKind::QUICK_STOP ? v.quickStop : v.halt;
}
} // namespace

Status makeMoveEvidence(MoveEvidence& output, const MeasuredRecord& record) noexcept {
    if (overlaps(&output, sizeof output, &record, sizeof record))
        return refuse(RecordCheckError::OVERLAP, "move evidence overlaps its record");
    uint32_t id = 0;
    const Status s = recordOf(record, RecordKind::POSITION, id);
    if (!s)
        return s;
    const PositionScope& p = record.position;
    const bool relative = p.move == RecordMoveKind::RELATIVE;
    if (!measured(p.rpm.basis) || !measured(p.rampMs.basis) || !measured(p.distanceBasis) ||
        !measured(p.signalsBasis) || (relative && !measured(p.baseBasis)))
        return refuse(RecordCheckError::BASIS,
                      "a measured move needs measured rpm, ramp, distance, signals and base",
                      Err::UNSUPPORTED);
    MoveEvidence e;
    e.gate = MoveGate::MEASURED;
    e.record = id;
    e.model = record.model;
    copyIdentity(e.firmware, record.identity);
    e.acceptance = p.acceptance;
    e.completion = p.completion;
    e.rule = p.rule;
    e.relativeBase = relative ? p.relativeBase : RelativeBase::UNMEASURED;
    output = e;
    return Ok();
}

Status makeVelocityEvidence(VelocityEvidence& output, const MeasuredRecord& record,
                            StopKind stopKind) noexcept {
    if (overlaps(&output, sizeof output, &record, sizeof record))
        return refuse(RecordCheckError::OVERLAP, "velocity evidence overlaps its record");
    uint32_t id = 0;
    const Status s = recordOf(record, RecordKind::VELOCITY, id);
    if (!s)
        return s;
    const VelocityScope& v = record.velocity;
    if (stopKind != StopKind::HALT && stopKind != StopKind::QUICK_STOP)
        return refuse(RecordCheckError::STOP_KIND, "unknown stop kind", Err::UNSUPPORTED);
    const RecordStop& stop = stopOf(v, stopKind);
    if (!stop.admitted)
        return refuse(RecordCheckError::STOP_KIND, "the record does not establish this stop kind",
                      Err::UNSUPPORTED);
    if (!measured(v.rpm.basis) || !measured(v.directionBasis) || !measured(v.rampMs.basis) ||
        !measured(v.durationBasis) || !measured(v.scaleBasis) || !measured(stop.rpm.basis) ||
        !measured(v.starts.basis))
        return refuse(RecordCheckError::BASIS,
                      "a measured velocity run needs measured rpm, direction, ramp, duration, 2201, "
                      "stop and start facts",
                      Err::UNSUPPORTED);
    // VelocityEvidence always has a forward range (maximumForwardRpm 1..3000), so a reverse-only
    // record would be widened by it; the record check would still refuse forward, but the evidence
    // must not claim more than the record.
    if (v.direction == RecordDirection::REVERSE)
        return refuse(RecordCheckError::DIRECTION,
                      "velocity evidence needs a forward range; a reverse-only record has none",
                      Err::UNSUPPORTED);
    const bool reverse = v.direction == RecordDirection::BOTH;
    VelocityEvidence e;
    e.gate = VelocityGate::MEASURED;
    e.record = id;
    e.model = record.model;
    copyIdentity(e.firmware, record.identity);
    e.layout = v.layout;
    e.startQualified = e.stopQualified = true;
    e.stopKind = stopKind;
    e.reverseQualified = reverse;
    e.maximumForwardRpm = v.rpm.maximum;
    e.maximumReverseRpm = reverse ? v.rpm.maximum : 0;
    e.maximumEnableWindowUs = v.maximumWindowUs;
    output = e;
    return Ok();
}

Status makeEnableEvidence(EnableEvidence& evidence, IdentityReference& reference,
                          const MeasuredRecord& record, uint32_t operation) noexcept {
    if (overlaps(&evidence, sizeof evidence, &record, sizeof record) ||
        overlaps(&reference, sizeof reference, &record, sizeof record) ||
        overlaps(&evidence, sizeof evidence, &reference, sizeof reference))
        return refuse(RecordCheckError::OVERLAP, "enable evidence overlaps its record");
    uint32_t id = 0;
    const Status s = recordOf(record, RecordKind::ENABLE, id);
    if (!s)
        return s;
    if (!measured(record.enable.modeBasis) || !measured(record.enable.starts.basis))
        return refuse(RecordCheckError::BASIS, "a measured enable needs a measured mode and starts",
                      Err::UNSUPPORTED);
    IdentityReference i;
    i.present = true;
    i.values = record.identity;
    EnableEvidence e;
    e.gate = EnableGate::MEASURED;
    e.modeRaw = record.enable.modeRaw;
    e.operation = operation;
    e.start = EnableStart::UNKNOWN;
    e.recordIdentity = i;
    evidence = e;
    reference = i;
    return Ok();
}

Status checkRecordMove(const MeasuredRecord& record, const MoveEvidence& evidence,
                       const MoveRequest& request, const MoveProfile& profile,
                       const AxisConfig& axis, const AxisReference* reference,
                       uint64_t nowUs) noexcept {
    MoveEvidence expected;
    Status s = makeMoveEvidence(expected, record);
    if (!s)
        return s;
    if (evidence.gate != expected.gate || evidence.record != expected.record ||
        evidence.model != expected.model || !sameFirmware(evidence.firmware, expected.firmware))
        return refuse(RecordCheckError::EVIDENCE, "the move evidence is not the record's");
    if (!sameSignal(evidence.acceptance, expected.acceptance) ||
        !sameSignal(evidence.completion, expected.completion))
        return refuse(RecordCheckError::SIGNALS, "the move signals are not the record's",
                      Err::UNSUPPORTED);
    if (evidence.rule != expected.rule)
        return refuse(RecordCheckError::RULE, "the completion rule is not the record's",
                      Err::UNSUPPORTED);
    if (evidence.relativeBase != expected.relativeBase)
        return refuse(RecordCheckError::BASE, "the relative base is not the record's",
                      Err::UNSUPPORTED);
    const PositionScope& p = record.position;
    const bool relative = p.move == RecordMoveKind::RELATIVE;
    if (request.position.relative != relative)
        return refuse(RecordCheckError::MOVE_KIND,
                      relative ? "the record covers relative moves only"
                               : "the record covers absolute moves only",
                      Err::UNSUPPORTED);
    const RelativeBasis base =
        p.relativeBase == RelativeBase::COMMANDED ? RelativeBasis::COMMANDED : RelativeBasis::ACTUAL;
    if (relative && request.position.basis != base)
        return refuse(RecordCheckError::BASE, "the request's relative basis is not the record's base",
                      Err::UNSUPPORTED);
    if (!rampsWithin(profile.rampUpTimeMs, profile.rampDownTimeMs, p.rampMs))
        return refuse(RecordCheckError::RAMP, "6083 or 6084 outside the record's ramp range",
                      Err::ILLEGAL_VALUE);
    int64_t rpm = 0;
    s = rpmOf(request.speed, axis, rpm);
    if (!s)
        return s;
    if (rpm < 1 || !inRange(uint64_t(rpm), p.rpm))
        return refuse(RecordCheckError::SPEED, "6081 outside the record's rpm range",
                      Err::ILLEGAL_VALUE);
    // 607A as the move preparation computes it, from the same request, axis, witness and settings.
    DriverSettings settings;
    settings.positionSpeedRpm = uint32_t(rpm);
    settings.rampUpTimeMs = profile.rampUpTimeMs;
    settings.rampDownTimeMs = profile.rampDownTimeMs;
    PreparedPositionParameters parameters;
    s = preparePositionParameters(parameters, record.model, request.position, axis, reference,
                                  settings, nowUs);
    if (!s)
        return based(s, RecordCheckError::PARAMETER_ERROR_BASE);
    uint64_t distance = 0;
    if (relative) {
        distance = magnitude(parameters.targetNative);
    } else {
        const PreparedTarget& t = parameters.target;
        if (!t.referenceUsed || !t.retainedReference.nativeKnown)
            return refuse(RecordCheckError::DISTANCE_UNKNOWN,
                          "an absolute move under a record needs a known witness for its travel",
                          Err::UNSUPPORTED);
        // Both lie within int64, so the unsigned difference of their two's complements is exact.
        const uint64_t endpoint = uint64_t(t.endpointNative);
        const uint64_t witness = uint64_t(t.retainedReference.nativePosition);
        distance = t.endpointNative >= t.retainedReference.nativePosition ? endpoint - witness
                                                                          : witness - endpoint;
    }
    if (distance > p.maximumDistance)
        return refuse(RecordCheckError::DISTANCE, "travel beyond the record's distance bound",
                      Err::ILLEGAL_VALUE);
    return Ok();
}

Status checkRecordVelocity(const MeasuredRecord& record, const VelocityEvidence& evidence,
                           const FiniteVelocityRequest& request, const VelocityProfile& profile,
                           const AxisConfig& axis, const ReadContext& state,
                           const ReadTarget& current, uint64_t nowUs,
                           const VelocityOptions& options) noexcept {
    VelocityEvidence expected;
    Status s = makeVelocityEvidence(expected, record, options.stopKind);
    if (!s)
        return s;
    const VelocityEvidence& e = evidence;
    if (e.gate != expected.gate || e.record != expected.record || e.model != expected.model ||
        !sameFirmware(e.firmware, expected.firmware) || e.layout != expected.layout ||
        e.startQualified != expected.startQualified || e.stopQualified != expected.stopQualified ||
        e.stopKind != expected.stopKind || e.modeChangeQualified != expected.modeChangeQualified ||
        e.modeChangeFromRaw != expected.modeChangeFromRaw ||
        e.reverseQualified != expected.reverseQualified || e.rangeProbe != expected.rangeProbe ||
        e.reverseProbe != expected.reverseProbe || e.maximumForwardRpm != expected.maximumForwardRpm ||
        e.maximumReverseRpm != expected.maximumReverseRpm ||
        e.maximumEnableWindowUs != expected.maximumEnableWindowUs)
        return refuse(RecordCheckError::EVIDENCE, "the velocity evidence is not the record's");
    const VelocityScope& v = record.velocity;
    if (!rampsWithin(profile.rampUpTimeMs, profile.rampDownTimeMs, v.rampMs))
        return refuse(RecordCheckError::RAMP, "6083 or 6084 outside the record's ramp range",
                      Err::ILLEGAL_VALUE);
    if (request.durationUs > v.maximumDurationUs)
        return refuse(RecordCheckError::DURATION, "duration beyond the record's longest run",
                      Err::ILLEGAL_VALUE);
    const UnitScale& scale = axis.units.commandStepsPerMotorTurn;
    if (!scale.denominator || scale.numerator % scale.denominator ||
        scale.numerator / scale.denominator != v.scale2201)
        return refuse(RecordCheckError::SCALE,
                      "the axis command scale is not the 2201 subdivision the record was made at",
                      Err::UNSUPPORTED);
    int64_t rpm = 0;
    s = rpmOf(request.velocity, axis, rpm);
    if (!s)
        return s;
    const bool forward = v.direction == RecordDirection::FORWARD || v.direction == RecordDirection::BOTH;
    const bool reverse = v.direction == RecordDirection::REVERSE || v.direction == RecordDirection::BOTH;
    if ((rpm > 0 && !forward) || (rpm < 0 && !reverse))
        return refuse(RecordCheckError::DIRECTION, "the record does not cover this 60FF sign",
                      Err::UNSUPPORTED);
    const uint64_t speed = magnitude(rpm);
    if (!speed || !inRange(speed, v.rpm))
        return refuse(RecordCheckError::SPEED, "60FF outside the record's rpm range",
                      Err::ILLEGAL_VALUE);
    if (!inRange(speed, stopOf(v, options.stopKind).rpm))
        return refuse(RecordCheckError::STOP_SPEED,
                      "the rate is outside the range the selected stop was established at",
                      Err::ILLEGAL_VALUE);
    if (options.enableWindowUs > v.maximumWindowUs)
        return refuse(RecordCheckError::WINDOW, "enable report window beyond the record's bound",
                      Err::ILLEGAL_VALUE);
    StartReport report;
    uint8_t mode = 0;
    s = startReport(state, current, nowUs, options.maximumAgeUs, report, mode);
    if (!s)
        return s;
    if (!admitted(v.starts, report))
        return refuse(RecordCheckError::START, "the start report is not one the record admits",
                      Err::UNSUPPORTED);
    return Ok();
}

Status checkRecordEnable(const MeasuredRecord& record, const EnableEvidence& evidence,
                         const ActionOptions& options, const ReadContext& state,
                         const IdentityObservation& identity, const ReadTarget& current,
                         uint64_t nowUs) noexcept {
    EnableEvidence expected;
    IdentityReference reference;
    Status s = makeEnableEvidence(expected, reference, record, evidence.operation);
    if (!s)
        return s;
    if (evidence.gate != expected.gate || evidence.modeRaw != expected.modeRaw ||
        !evidence.recordIdentity.present || !sameIdentity(evidence.recordIdentity.values, reference.values) ||
        !options.identityReference.present || !sameIdentity(options.identityReference.values, reference.values))
        return refuse(RecordCheckError::EVIDENCE,
                      "the enable evidence or identity reference is not the record's");
    // The prompt 57 policy against the record's own identity, not against measuredIdentity(): its product
    // fields must match; a firmware difference only flags, and prepareEnable records that flag.
    IdentityCheck check;
    if (!checkIdentity(check, identity, current, record.identity, IdentityBasis::MEASURED, nowUs))
        return refuse(RecordCheckError::IDENTITY, "the enable needs a current identity read of this target");
    if (!check.expectedMatch)
        return refuse(RecordCheckError::IDENTITY,
                      "identity product fields (1000, 1018:01, 1018:02, 2000) differ from the record's",
                      Err::UNSUPPORTED);
    StartReport report;
    uint8_t mode = 0;
    s = startReport(state, current, nowUs, options.maximumAgeUs, report, mode);
    if (!s)
        return s;
    if (mode != record.enable.modeRaw)
        return refuse(RecordCheckError::MODE, "the drive reports another mode than the record's",
                      Err::UNSUPPORTED);
    if (!admitted(record.enable.starts, report))
        return refuse(RecordCheckError::START, "the start report is not one the record admits",
                      Err::UNSUPPORTED);
    return Ok();
}
} // namespace LICHUAN_C
} // namespace MotorControlCAN
