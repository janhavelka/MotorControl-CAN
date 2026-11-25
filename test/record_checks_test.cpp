// SPDX-License-Identifier: MIT
// Record checks and evidence builders (prompt 58, slice b2): the record id from the CRC, the library
// evidence a record builds, every refusal of a request against its record, and records that state
// more than the library allows staying limited by the library. The records and state reads are test
// inputs in the shape of the stage (a) evidence, not the reviewed records or a device's behavior.
#include <MotorControlCAN/profiles/lichuan_c/Records.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <new>

namespace M = MotorControlCAN;
namespace C = MotorControlCAN::CANOPEN;
namespace L = MotorControlCAN::LICHUAN_C;
using E = L::RecordCheckError;

namespace {
void check(bool valid, int line) {
    if (!valid) {
        std::fprintf(stderr, "record_checks_test.cpp:%d\n", line);
        std::abort();
    }
}
#define CHECK(value) check(static_cast<bool>(value), __LINE__)
template <class T> struct Saved {
    unsigned char bytes[sizeof(T)];
    explicit Saved(const T& value) { std::memcpy(bytes, &value, sizeof value); }
    bool unchanged(const T& value) const { return !std::memcmp(bytes, &value, sizeof value); }
};
void refused(const M::Status& s, E detail, M::Err code) {
    if (s.detail != int32_t(detail) || s.code != code)
        std::fprintf(stderr, "status %d/%d: %s\n", int(s.code), int(s.detail), s.msg);
    CHECK(!s && s.detail == int32_t(detail) && s.code == code);
}

L::RecordRange range(uint32_t minimum, uint32_t maximum,
                     L::RecordBasis basis = L::RecordBasis::MEASURED) {
    L::RecordRange r;
    r.minimum = minimum;
    r.maximum = maximum;
    r.basis = basis;
    return r;
}
void setEvidence(L::MeasuredRecord& r, const char* text) {
    std::memset(r.evidence, 0, sizeof r.evidence);
    std::strncpy(r.evidence, text, sizeof r.evidence - 1);
}
void addStart(L::RecordStarts& starts, uint16_t statusword, uint16_t monitor) {
    starts.reports[starts.count].statusword = statusword;
    starts.reports[starts.count].monitor = monitor;
    ++starts.count;
}
L::MeasuredRecord enableRecord() {
    L::MeasuredRecord r;
    r.kind = L::RecordKind::ENABLE;
    setEvidence(r, "p58-enable-fixture");
    r.model = L::SettingsModel::CL86_C;
    r.identity = L::measuredIdentity();
    r.enable.modeRaw = 1;
    r.enable.modeBasis = L::RecordBasis::MEASURED;
    addStart(r.enable.starts, 0x8437, 0x0001);
    addStart(r.enable.starts, 0x4050, 0x0014);
    addStart(r.enable.starts, 0x8050, 0x0011);
    r.enable.starts.basis = L::RecordBasis::MEASURED;
    return r;
}
L::MeasuredRecord positionRecord() {
    L::MeasuredRecord r;
    r.kind = L::RecordKind::POSITION;
    setEvidence(r, "p58-position-fixture");
    r.model = L::SettingsModel::CL86_C;
    r.identity = L::measuredIdentity();
    L::PositionScope& p = r.position;
    p.move = L::RecordMoveKind::RELATIVE;
    p.relativeBase = L::RelativeBase::ACTUAL;
    p.baseBasis = L::RecordBasis::MEASURED;
    p.rpm = range(2, 300);
    p.rampMs = range(100, 2000);
    p.maximumDistance = 169410;
    p.distanceBasis = L::RecordBasis::MEASURED;
    p.acceptance.source = L::MoveSignalSource::STATUSWORD;
    p.acceptance.mask = p.acceptance.value = 0x1000;
    p.completion.source = L::MoveSignalSource::MONITOR;
    p.completion.mask = p.completion.value = 0x0001;
    p.rule = L::CompletionRule::AFTER_ABSENCE;
    p.signalsBasis = L::RecordBasis::MEASURED;
    return r;
}
L::MeasuredRecord absoluteRecord() {
    L::MeasuredRecord r = positionRecord();
    setEvidence(r, "p58-absolute-fixture");
    r.position.move = L::RecordMoveKind::ABSOLUTE;
    r.position.relativeBase = L::RelativeBase::UNMEASURED;
    r.position.baseBasis = L::RecordBasis::NONE;
    r.position.rpm = range(2, 60);
    r.position.maximumDistance = 2000;
    return r;
}
L::MeasuredRecord velocityRecord() {
    L::MeasuredRecord r;
    r.kind = L::RecordKind::VELOCITY;
    setEvidence(r, "p58-velocity-fixture");
    r.model = L::SettingsModel::CL86_C;
    r.identity = L::measuredIdentity();
    L::VelocityScope& v = r.velocity;
    v.layout = L::StatusLayout::ZH86;
    v.layoutBasis = L::RecordBasis::DOCUMENTED; // Layout and window may rest on any basis.
    v.rpm = range(6, 60);
    v.direction = L::RecordDirection::FORWARD;
    v.directionBasis = L::RecordBasis::MEASURED;
    v.rampMs = range(200, 200);
    v.maximumDurationUs = 12000000;
    v.durationBasis = L::RecordBasis::MEASURED;
    v.scale2201 = 1000;
    v.scaleBasis = L::RecordBasis::MEASURED;
    v.halt.admitted = true;
    v.halt.rpm = range(6, 60);
    v.quickStop.admitted = true;
    v.quickStop.rpm = range(6, 30);
    v.maximumWindowUs = 800000;
    v.windowBasis = L::RecordBasis::INFERRED;
    addStart(v.starts, 0x8050, 0x0011);
    addStart(v.starts, 0x0001, L::MONITOR_RELEASED);
    v.starts.basis = L::RecordBasis::MEASURED;
    return r;
}

// One route, reservation and identity, as the library's own tests build them.
struct Fixture {
    L::ReadTarget target;
    L::AxisReservation live;
    L::IdentityObservation identity;
    M::AxisConfig axis;
    Fixture() {
        target.channel.binding.logicalAxisId = 17;
        target.channel.binding.endpoint.profile = 41;
        target.channel.binding.endpoint.address = 5;
        target.channel.binding.endpoint.bus = 2;
        target.channel.binding.bindingGeneration = 3;
        target.channel.binding.configurationGeneration = 7;
        target.channel.clientToServerId = 0x605;
        target.channel.serverToClientId = 0x585;
        target.nodeGeneration = 11;
        live.target = target;
        live.model = L::SettingsModel::CL86_C;
        live.owner = 41;
        live.sequence = 3;
        live.generation = 2;
        live.acquiredUs = 100;
        const L::IdentityValues i = L::measuredIdentity();
        identity.target = target;
        identity.deviceType = i.deviceType;
        identity.hardwareVersion = i.hardwareVersion;
        identity.softwareVersion = i.softwareVersion;
        identity.vendorId = i.vendorId;
        identity.productCode = i.productCode;
        identity.revision = i.revision;
        identity.modelCode = i.modelCode;
        identity.firmwareCode = i.firmwareCode;
        axis.binding = target.channel.binding;
        axis.units.commandStepsPerMotorTurn = M::UnitScale(1000, 1, M::ScaleSource::QUALIFIED);
        axis.units.motorTurnsPerLoadTurn = M::UnitScale(1, 1, M::ScaleSource::QUALIFIED);
        axis.nativeRateBasis = M::NativeRateBasis::MOTOR_TURNS;
        axis.nativeVelocityTime = M::TimeUnit::MINUTE;
        axis.supportedRelativeBases = 1;
    }
    // A complete fresh ERRORS, MONITORS, STATUS and FEEDBACK read reporting these words.
    void readState(L::ReadContext& state, uint16_t statusword, uint16_t monitor, uint8_t mode,
                   uint64_t at = 200) const {
        const uint8_t blocks = uint8_t((1u << unsigned(L::StateBlock::ERRORS)) |
                                       (1u << unsigned(L::StateBlock::MONITORS)) |
                                       (1u << unsigned(L::StateBlock::STATUS)) |
                                       (1u << unsigned(L::StateBlock::FEEDBACK)));
        L::ReadOptions options;
        options.queueTimeoutUs = options.responseTimeoutUs = 100;
        CHECK(L::prepareState(state, target, 1, at, at + 100000, options, blocks));
        for (unsigned guard = 0; state.state == M::ReadState::ACTIVE && guard < 20; ++guard) {
            L::PreparedRead work;
            CHECK(L::nextRead(state, at, work) && work.ready);
            const uint16_t index = work.transfer.index;
            const uint32_t value = index == 0x6041 ? statusword
                                   : index == 0x2100 ? monitor
                                   : index == 0x6061 ? mode
                                                     : 0u;
            C::SdoCompletion e;
            e.transfer = work.transfer;
            e.nodeGeneration = 11;
            e.admittedUs = e.completedUs = at;
            e.outcome = C::SdoOutcome::SUCCESS;
            e.queued = e.transmissionPossible = e.protocolAcknowledged = e.effectPossible = true;
            e.size = work.transfer.size;
            for (unsigned i = 0; i < e.size; ++i)
                e.data[i] = uint8_t(value >> (8u * i));
            CHECK(L::advanceRead(state, e, at));
            ++at;
        }
        CHECK(state.state == M::ReadState::SUCCEEDED);
    }
    M::MoveRequest move(int64_t steps, int64_t rpm, bool relative) const {
        M::MoveRequest r;
        r.position.value = M::Rational(steps);
        r.position.relative = relative;
        r.position.configurationGeneration = axis.binding.configurationGeneration;
        r.speed.value = M::Rational(rpm);
        r.speed.unit = M::VelocityUnit(M::PositionUnit::TURNS, M::TimeUnit::MINUTE);
        r.speed.configurationGeneration = axis.binding.configurationGeneration;
        return r;
    }
    M::AxisReference witness(int64_t position, uint64_t at = 300) const {
        M::AxisReference w;
        w.binding = axis.binding;
        w.referenceGeneration = axis.referenceGeneration;
        w.idle = w.stationary = w.nativeKnown = true;
        w.nativePosition = position;
        w.basis = M::RelativeBasis::ACTUAL;
        w.source = M::ScaleSource::QUALIFIED;
        w.observedUs = w.nowUs = at;
        w.maximumAgeUs = 1000000;
        return w;
    }
    M::FiniteVelocityRequest velocity(int64_t rpm, uint64_t durationUs = 500000) const {
        M::FiniteVelocityRequest r;
        r.velocity.value = M::Rational(rpm);
        r.velocity.unit = M::VelocityUnit(M::PositionUnit::TURNS, M::TimeUnit::MINUTE);
        r.velocity.configurationGeneration = axis.binding.configurationGeneration;
        r.durationUs = durationUs;
        return r;
    }
};
L::MoveProfile moveProfile(uint32_t up = 100, uint32_t down = 200) {
    L::MoveProfile p;
    p.rampUpTimeMs = up;
    p.rampDownTimeMs = down;
    return p;
}
L::VelocityProfile velocityProfile(uint32_t up = 200, uint32_t down = 200) {
    L::VelocityProfile p;
    p.rampUpTimeMs = up;
    p.rampDownTimeMs = down;
    return p;
}
L::VelocityOptions velocityOptions(L::StopKind kind = L::StopKind::HALT) {
    L::VelocityOptions o;
    o.service.queueTimeoutUs = o.service.responseTimeoutUs = 100;
    o.pollIntervalUs = 10000;
    o.maxPolls = 100;
    o.maximumAgeUs = 1000000;
    o.requiredStopBudgetUs = 100000;
    o.commandIntervalUs = 10000;
    o.stopKind = kind;
    return o;
}
uint32_t lineCrc(const L::MeasuredRecord& r) {
    char text[L::RECORD_TEXT_CAPACITY + 1];
    size_t length = 0;
    L::RecordRefusal refusal;
    CHECK(L::encodeRecord(text, sizeof text, length, refusal, r).isOk());
    return uint32_t(std::strtoul(text + length - 8, nullptr, 16));
}

void recordIds() {
    const L::MeasuredRecord records[] = {enableRecord(), positionRecord(), velocityRecord()};
    for (const L::MeasuredRecord& r : records) {
        uint32_t id = 0;
        CHECK(L::recordId(id, r).isOk() && id && id == lineCrc(r));
    }
    // Another evidence reference is another record and another id.
    L::MeasuredRecord other = enableRecord();
    setEvidence(other, "p58-enable-other");
    uint32_t a = 0, b = 0;
    CHECK(L::recordId(a, enableRecord()) && L::recordId(b, other) && a != b);
    // A record whose canonical CRC is 0 has no id: refused everywhere, output unchanged. The evidence
    // reference was solved over GF(2) with zlib.crc32 for exactly this record.
    L::MeasuredRecord zero = enableRecord();
    zero.enable.starts.count = 0;
    addStart(zero.enable.starts, 0x8437, 0x0001);
    addStart(zero.enable.starts, 0x4050, 0x0014);
    addStart(zero.enable.starts, 0x8050, 0x0011);
    addStart(zero.enable.starts, 0xC050, 0x0015);
    addStart(zero.enable.starts, 0x0050, 0x0010);
    setEvidence(zero, "p58-zerccaacaccacaaccccacacaaacccaaaaaaaaaaaaaa");
    uint32_t id = 77;
    M::Status s = L::recordId(id, zero);
    CHECK(!s && s.detail == int32_t(L::RecordError::ZERO_CRC) && id == 77);
    L::RecordRefusal refusal;
    s = L::validateRecord(refusal, zero);
    CHECK(!s && refusal.reason == L::RecordError::ZERO_CRC && std::strcmp(refusal.key, "crc") == 0);
    char text[L::RECORD_TEXT_CAPACITY + 1];
    size_t length = 5;
    CHECK(L::encodeRecord(text, sizeof text, length, refusal, zero).detail ==
          int32_t(L::RecordError::ZERO_CRC));
    CHECK(length == 5);
    // Its own line, with the matching crc=00000000, decodes to a refusal too.
    const char* body = "v=1 kind=enable evidence=p58-zerccaacaccacaaccccacacaaacccaaaaaaaaaaaaaa "
                       "model=cl86_c identity=00040192.000C.0020.00001000.00000001.00000000.0400.0200 "
                       "mode=1 mode_basis=measured starts=8437/0001,4050/0014,8050/0011,C050/0015,"
                       "0050/0010 starts_basis=measured";
    CHECK(L::crc32Ieee(0, body, std::strlen(body)) == 0);
    std::snprintf(text, sizeof text, "%s crc=00000000", body);
    L::MeasuredRecord decoded = enableRecord();
    const Saved<L::MeasuredRecord> before(decoded);
    s = L::decodeRecord(decoded, refusal, text, std::strlen(text));
    CHECK(!s && refusal.reason == L::RecordError::ZERO_CRC && before.unchanged(decoded));
    L::EnableEvidence evidence;
    L::IdentityReference reference;
    refused(L::makeEnableEvidence(evidence, reference, zero, 9), E::RECORD, M::Err::INVALID_CONFIG);
}

void moveEvidence() {
    L::MoveEvidence e;
    CHECK(L::makeMoveEvidence(e, positionRecord()));
    uint32_t id = 0;
    CHECK(L::recordId(id, positionRecord()));
    const L::IdentityValues i = L::measuredIdentity();
    CHECK(e.gate == L::MoveGate::MEASURED && e.record == id && e.model == L::SettingsModel::CL86_C);
    CHECK(e.firmware.deviceType == i.deviceType && e.firmware.hardwareVersion == i.hardwareVersion &&
          e.firmware.softwareVersion == i.softwareVersion && e.firmware.vendorId == i.vendorId &&
          e.firmware.productCode == i.productCode && e.firmware.revision == i.revision &&
          e.firmware.modelCode == i.modelCode && e.firmware.firmwareCode == i.firmwareCode);
    CHECK(e.acceptance.source == L::MoveSignalSource::STATUSWORD && e.acceptance.mask == 0x1000 &&
          e.completion.source == L::MoveSignalSource::MONITOR && e.completion.value == 0x0001 &&
          e.rule == L::CompletionRule::AFTER_ABSENCE && e.relativeBase == L::RelativeBase::ACTUAL);
    CHECK(L::makeMoveEvidence(e, absoluteRecord()) && e.relativeBase == L::RelativeBase::UNMEASURED);
    // Refusals leave the output unchanged.
    const Saved<L::MoveEvidence> before(e);
    L::MeasuredRecord r = positionRecord();
    r.position.signalsBasis = L::RecordBasis::INFERRED;
    refused(L::makeMoveEvidence(e, r), E::BASIS, M::Err::UNSUPPORTED);
    r = positionRecord();
    r.position.rpm.basis = L::RecordBasis::DOCUMENTED;
    refused(L::makeMoveEvidence(e, r), E::BASIS, M::Err::UNSUPPORTED);
    r = positionRecord();
    r.position.baseBasis = L::RecordBasis::INFERRED;
    refused(L::makeMoveEvidence(e, r), E::BASIS, M::Err::UNSUPPORTED);
    refused(L::makeMoveEvidence(e, velocityRecord()), E::KIND, M::Err::INVALID_CONFIG);
    r = positionRecord();
    r.position.rpm = range(300, 2);
    refused(L::makeMoveEvidence(e, r), E::RECORD, M::Err::ILLEGAL_VALUE);
    CHECK(before.unchanged(e));
}

void velocityEvidence() {
    L::VelocityEvidence e;
    CHECK(L::makeVelocityEvidence(e, velocityRecord(), L::StopKind::QUICK_STOP));
    uint32_t id = 0;
    CHECK(L::recordId(id, velocityRecord()));
    CHECK(e.gate == L::VelocityGate::MEASURED && e.record == id && e.layout == L::StatusLayout::ZH86 &&
          e.startQualified && e.stopQualified && e.stopKind == L::StopKind::QUICK_STOP &&
          !e.modeChangeQualified && !e.reverseQualified && !e.rangeProbe && !e.reverseProbe &&
          e.maximumForwardRpm == 60 && e.maximumReverseRpm == 0 && e.maximumEnableWindowUs == 800000);
    CHECK(e.firmware.modelCode == L::measuredIdentity().modelCode);
    L::MeasuredRecord r = velocityRecord();
    r.velocity.direction = L::RecordDirection::BOTH;
    CHECK(L::makeVelocityEvidence(e, r, L::StopKind::HALT) && e.reverseQualified &&
          e.maximumForwardRpm == 60 && e.maximumReverseRpm == 60 && e.stopKind == L::StopKind::HALT);
    const Saved<L::VelocityEvidence> before(e);
    r = velocityRecord();
    r.velocity.quickStop = L::RecordStop();
    refused(L::makeVelocityEvidence(e, r, L::StopKind::QUICK_STOP), E::STOP_KIND, M::Err::UNSUPPORTED);
    refused(L::makeVelocityEvidence(e, r, L::StopKind(9)), E::STOP_KIND, M::Err::UNSUPPORTED);
    r = velocityRecord();
    r.velocity.direction = L::RecordDirection::REVERSE;
    refused(L::makeVelocityEvidence(e, r, L::StopKind::HALT), E::DIRECTION, M::Err::UNSUPPORTED);
    // The basis of the selected stop counts; the other stop's does not.
    r = velocityRecord();
    r.velocity.quickStop.rpm.basis = L::RecordBasis::INFERRED;
    refused(L::makeVelocityEvidence(e, r, L::StopKind::QUICK_STOP), E::BASIS, M::Err::UNSUPPORTED);
    L::VelocityEvidence halt;
    CHECK(L::makeVelocityEvidence(halt, r, L::StopKind::HALT));
    L::RecordBasis L::VelocityScope::*const bases[] = {
        &L::VelocityScope::directionBasis, &L::VelocityScope::durationBasis, &L::VelocityScope::scaleBasis};
    for (auto basis : bases) {
        r = velocityRecord();
        r.velocity.*basis = L::RecordBasis::DOCUMENTED;
        refused(L::makeVelocityEvidence(e, r, L::StopKind::HALT), E::BASIS, M::Err::UNSUPPORTED);
    }
    r = velocityRecord();
    r.velocity.starts.basis = L::RecordBasis::INFERRED;
    refused(L::makeVelocityEvidence(e, r, L::StopKind::HALT), E::BASIS, M::Err::UNSUPPORTED);
    refused(L::makeVelocityEvidence(e, enableRecord(), L::StopKind::HALT), E::KIND, M::Err::INVALID_CONFIG);
    CHECK(before.unchanged(e));
}

void enableEvidence() {
    L::EnableEvidence e;
    L::IdentityReference reference;
    CHECK(L::makeEnableEvidence(e, reference, enableRecord(), 42));
    CHECK(e.gate == L::EnableGate::MEASURED && e.modeRaw == 1 && e.operation == 42 &&
          e.start == L::EnableStart::UNKNOWN && reference.present &&
          reference.values.vendorId == L::measuredIdentity().vendorId);
    const Saved<L::EnableEvidence> before(e);
    const Saved<L::IdentityReference> referenceBefore(reference);
    L::MeasuredRecord r = enableRecord();
    r.enable.starts.basis = L::RecordBasis::INFERRED;
    refused(L::makeEnableEvidence(e, reference, r, 43), E::BASIS, M::Err::UNSUPPORTED);
    r = enableRecord();
    r.enable.modeBasis = L::RecordBasis::DOCUMENTED;
    refused(L::makeEnableEvidence(e, reference, r, 43), E::BASIS, M::Err::UNSUPPORTED);
    refused(L::makeEnableEvidence(e, reference, positionRecord(), 43), E::KIND, M::Err::INVALID_CONFIG);
    CHECK(before.unchanged(e) && referenceBefore.unchanged(reference));
    // Outputs that overlap: two objects in one buffer, used by address only.
    alignas(L::EnableEvidence) unsigned char storage[sizeof(L::EnableEvidence) + 8];
    L::EnableEvidence* shared = new (storage) L::EnableEvidence();
    L::IdentityReference* inside = reinterpret_cast<L::IdentityReference*>(storage + 4);
    refused(L::makeEnableEvidence(*shared, *inside, enableRecord(), 43), E::OVERLAP, M::Err::INVALID_CONFIG);
}

void moveChecks() {
    const Fixture f;
    const L::MeasuredRecord record = positionRecord();
    L::MoveEvidence evidence;
    CHECK(L::makeMoveEvidence(evidence, record));
    const M::AxisReference w = f.witness(1000);
    const uint64_t now = 400;
    // Admitted: relative +169410 steps at 300 rpm with ramps 100 and 2000 ms, each at its bound.
    CHECK(L::checkRecordMove(record, evidence, f.move(169410, 300, true), moveProfile(100, 2000), f.axis, &w, now));
    CHECK(L::checkRecordMove(record, evidence, f.move(-169410, 2, true), moveProfile(), f.axis, &w, now));
    // Travel, speed and ramps just past the record.
    refused(L::checkRecordMove(record, evidence, f.move(169411, 300, true), moveProfile(), f.axis, &w, now),
            E::DISTANCE, M::Err::ILLEGAL_VALUE);
    refused(L::checkRecordMove(record, evidence, f.move(-169411, 300, true), moveProfile(), f.axis, &w, now),
            E::DISTANCE, M::Err::ILLEGAL_VALUE);
    refused(L::checkRecordMove(record, evidence, f.move(1000, 301, true), moveProfile(), f.axis, &w, now),
            E::SPEED, M::Err::ILLEGAL_VALUE);
    refused(L::checkRecordMove(record, evidence, f.move(1000, 1, true), moveProfile(), f.axis, &w, now),
            E::SPEED, M::Err::ILLEGAL_VALUE);
    refused(L::checkRecordMove(record, evidence, f.move(1000, 100, true), moveProfile(99, 200), f.axis, &w, now),
            E::RAMP, M::Err::ILLEGAL_VALUE);
    refused(L::checkRecordMove(record, evidence, f.move(1000, 100, true), moveProfile(100, 2001), f.axis, &w, now),
            E::RAMP, M::Err::ILLEGAL_VALUE);
    // Kind and basis.
    refused(L::checkRecordMove(record, evidence, f.move(1000, 100, false), moveProfile(), f.axis, &w, now),
            E::MOVE_KIND, M::Err::UNSUPPORTED);
    M::MoveRequest commanded = f.move(1000, 100, true);
    commanded.position.basis = M::RelativeBasis::COMMANDED;
    refused(L::checkRecordMove(record, evidence, commanded, moveProfile(), f.axis, &w, now), E::BASE,
            M::Err::UNSUPPORTED);
    // Evidence that is not the record's.
    L::MoveEvidence other = evidence;
    other.record ^= 1;
    refused(L::checkRecordMove(record, other, f.move(1000, 100, true), moveProfile(), f.axis, &w, now),
            E::EVIDENCE, M::Err::INVALID_CONFIG);
    other = evidence;
    other.firmware.softwareVersion ^= 1;
    refused(L::checkRecordMove(record, other, f.move(1000, 100, true), moveProfile(), f.axis, &w, now),
            E::EVIDENCE, M::Err::INVALID_CONFIG);
    other = evidence;
    other.completion.mask = 0x0003;
    refused(L::checkRecordMove(record, other, f.move(1000, 100, true), moveProfile(), f.axis, &w, now),
            E::SIGNALS, M::Err::UNSUPPORTED);
    other = evidence;
    other.rule = L::CompletionRule::AFTER_ACCEPTANCE;
    refused(L::checkRecordMove(record, other, f.move(1000, 100, true), moveProfile(), f.axis, &w, now),
            E::RULE, M::Err::UNSUPPORTED);
    other = evidence;
    other.relativeBase = L::RelativeBase::COMMANDED;
    refused(L::checkRecordMove(record, other, f.move(1000, 100, true), moveProfile(), f.axis, &w, now),
            E::BASE, M::Err::UNSUPPORTED);
    refused(L::checkRecordMove(velocityRecord(), evidence, f.move(1000, 100, true), moveProfile(), f.axis, &w,
                               now),
            E::KIND, M::Err::INVALID_CONFIG);
    // Speed conversions: a stale generation keeps the conversion's own error, and a destination that
    // is not rpm is refused before any comparison.
    M::MoveRequest stale = f.move(1000, 100, true);
    stale.speed.configurationGeneration = 99;
    const M::Status s = L::checkRecordMove(record, evidence, stale, moveProfile(), f.axis, &w, now);
    CHECK(!s && s.detail > int32_t(E::RATE_ERROR_BASE) && s.detail < int32_t(E::PARAMETER_ERROR_BASE));
    M::AxisConfig steps = f.axis;
    steps.nativeRateBasis = M::NativeRateBasis::COMMAND_INCREMENTS;
    steps.nativeVelocityTime = M::TimeUnit::SECOND;
    M::MoveRequest perSecond = f.move(1000, 100, true);
    perSecond.speed.unit = M::VelocityUnit(M::PositionUnit::STEPS, M::TimeUnit::SECOND);
    refused(L::checkRecordMove(record, evidence, perSecond, moveProfile(), steps, &w, now), E::RATE_BASIS,
            M::Err::INVALID_CONFIG);

    // Absolute: the travel is the endpoint minus a known witness; without one it is unknown.
    const L::MeasuredRecord absolute = absoluteRecord();
    L::MoveEvidence absoluteEvidence;
    CHECK(L::makeMoveEvidence(absoluteEvidence, absolute));
    CHECK(L::checkRecordMove(absolute, absoluteEvidence, f.move(3000, 60, false), moveProfile(), f.axis, &w, now));
    CHECK(L::checkRecordMove(absolute, absoluteEvidence, f.move(-1000, 60, false), moveProfile(), f.axis, &w, now));
    refused(L::checkRecordMove(absolute, absoluteEvidence, f.move(3001, 60, false), moveProfile(), f.axis, &w,
                               now),
            E::DISTANCE, M::Err::ILLEGAL_VALUE);
    refused(L::checkRecordMove(absolute, absoluteEvidence, f.move(1500, 60, false), moveProfile(), f.axis,
                               nullptr, now),
            E::DISTANCE_UNKNOWN, M::Err::UNSUPPORTED);
    refused(L::checkRecordMove(absolute, absoluteEvidence, f.move(1000, 60, true), moveProfile(), f.axis, &w, now),
            E::MOVE_KIND, M::Err::UNSUPPORTED);
    // A record that states more travel than 607A holds stays limited by the library: the target is
    // refused by the same preparation the move uses, with its own range error.
    L::MeasuredRecord wide = absoluteRecord();
    wide.position.maximumDistance = L::RECORD_MAXIMUM_ABSOLUTE_DISTANCE;
    L::MoveEvidence wideEvidence;
    CHECK(L::makeMoveEvidence(wideEvidence, wide));
    const M::AxisReference zero = f.witness(0);
    const M::Status limited =
        L::checkRecordMove(wide, wideEvidence, f.move(1500000, 60, false), moveProfile(), f.axis, &zero, now);
    CHECK(!limited && limited.code == M::Err::ILLEGAL_VALUE &&
          limited.detail == int32_t(E::PARAMETER_ERROR_BASE) + int32_t(L::PositionParameterError::RANGE));
    CHECK(L::checkRecordMove(wide, wideEvidence, f.move(1000000, 60, false), moveProfile(), f.axis, &zero, now));
}

void velocityChecks() {
    const Fixture f;
    const L::MeasuredRecord record = velocityRecord();
    L::VelocityEvidence evidence;
    CHECK(L::makeVelocityEvidence(evidence, record, L::StopKind::HALT));
    L::ReadContext state;
    f.readState(state, 0x0001, L::MONITOR_RELEASED, 3);
    const uint64_t now = 400;
    const L::VelocityOptions halt = velocityOptions();
    const L::VelocityProfile ramps = velocityProfile();
    CHECK(L::checkRecordVelocity(record, evidence, f.velocity(60, 12000000), ramps, f.axis, state, f.target, now,
                                 halt));
    CHECK(L::checkRecordVelocity(record, evidence, f.velocity(6), ramps, f.axis, state, f.target, now, halt));
    refused(L::checkRecordVelocity(record, evidence, f.velocity(61), ramps, f.axis, state, f.target, now, halt),
            E::SPEED, M::Err::ILLEGAL_VALUE);
    refused(L::checkRecordVelocity(record, evidence, f.velocity(5), ramps, f.axis, state, f.target, now, halt),
            E::SPEED, M::Err::ILLEGAL_VALUE);
    refused(L::checkRecordVelocity(record, evidence, f.velocity(-30), ramps, f.axis, state, f.target, now, halt),
            E::DIRECTION, M::Err::UNSUPPORTED);
    refused(L::checkRecordVelocity(record, evidence, f.velocity(30, 12000001), ramps, f.axis, state, f.target,
                                   now, halt),
            E::DURATION, M::Err::ILLEGAL_VALUE);
    refused(L::checkRecordVelocity(record, evidence, f.velocity(30), velocityProfile(201, 200), f.axis, state,
                                   f.target, now, halt),
            E::RAMP, M::Err::ILLEGAL_VALUE);
    M::AxisConfig scale = f.axis;
    scale.units.commandStepsPerMotorTurn = M::UnitScale(51200, 1, M::ScaleSource::QUALIFIED);
    refused(L::checkRecordVelocity(record, evidence, f.velocity(30), ramps, scale, state, f.target, now, halt),
            E::SCALE, M::Err::UNSUPPORTED);
    scale.units.commandStepsPerMotorTurn = M::UnitScale(2000, 2, M::ScaleSource::QUALIFIED); // Exactly 1000.
    CHECK(L::checkRecordVelocity(record, evidence, f.velocity(30), ramps, scale, state, f.target, now, halt));
    scale.units.commandStepsPerMotorTurn = M::UnitScale();
    refused(L::checkRecordVelocity(record, evidence, f.velocity(30), ramps, scale, state, f.target, now, halt),
            E::SCALE, M::Err::UNSUPPORTED);
    // The window bound, then the start report.
    L::VelocityOptions window = halt;
    window.enableWindowUs = 800000;
    window.enableSpacingUs = 50000;
    CHECK(L::checkRecordVelocity(record, evidence, f.velocity(30), ramps, f.axis, state, f.target, now, window));
    window.enableWindowUs = 800001;
    refused(L::checkRecordVelocity(record, evidence, f.velocity(30), ramps, f.axis, state, f.target, now, window),
            E::WINDOW, M::Err::ILLEGAL_VALUE);
    L::ReadContext latched;
    f.readState(latched, 0xC050, 0x0015, 3);
    refused(L::checkRecordVelocity(record, evidence, f.velocity(30), ramps, f.axis, latched, f.target, now, halt),
            E::START, M::Err::UNSUPPORTED);
    L::ReadContext other;
    f.readState(other, 0x8050, 0x0011, 3);
    CHECK(L::checkRecordVelocity(record, evidence, f.velocity(30), ramps, f.axis, other, f.target, now, halt));
    // A read older than the age cap, or of another route, is no start report.
    L::VelocityOptions young = halt;
    young.maximumAgeUs = 10;
    refused(L::checkRecordVelocity(record, evidence, f.velocity(30), ramps, f.axis, state, f.target, 5000, young),
            E::STATE, M::Err::INVALID_CONFIG);
    L::ReadTarget moved = f.target;
    moved.nodeGeneration = 12;
    refused(L::checkRecordVelocity(record, evidence, f.velocity(30), ramps, f.axis, state, moved, now, halt),
            E::STATE, M::Err::INVALID_CONFIG);
    // The stop kind and its rate range: QUICK_STOP was established at 6..30 rpm only.
    L::VelocityEvidence quick;
    CHECK(L::makeVelocityEvidence(quick, record, L::StopKind::QUICK_STOP));
    const L::VelocityOptions quickOptions = velocityOptions(L::StopKind::QUICK_STOP);
    CHECK(L::checkRecordVelocity(record, quick, f.velocity(30), ramps, f.axis, state, f.target, now, quickOptions));
    refused(L::checkRecordVelocity(record, quick, f.velocity(31), ramps, f.axis, state, f.target, now, quickOptions),
            E::STOP_SPEED, M::Err::ILLEGAL_VALUE);
    // Evidence built for another stop kind, or changed, is not this run's.
    refused(L::checkRecordVelocity(record, evidence, f.velocity(30), ramps, f.axis, state, f.target, now,
                                   quickOptions),
            E::EVIDENCE, M::Err::INVALID_CONFIG);
    L::VelocityEvidence changed = evidence;
    changed.maximumForwardRpm = 3000;
    refused(L::checkRecordVelocity(record, changed, f.velocity(30), ramps, f.axis, state, f.target, now, halt),
            E::EVIDENCE, M::Err::INVALID_CONFIG);
    changed = evidence;
    changed.modeChangeQualified = true;
    refused(L::checkRecordVelocity(record, changed, f.velocity(30), ramps, f.axis, state, f.target, now, halt),
            E::EVIDENCE, M::Err::INVALID_CONFIG);
    L::MeasuredRecord haltOnly = record;
    haltOnly.velocity.quickStop = L::RecordStop();
    refused(L::checkRecordVelocity(haltOnly, quick, f.velocity(30), ramps, f.axis, state, f.target, now,
                                   quickOptions),
            E::STOP_KIND, M::Err::UNSUPPORTED);
    // Both directions: a negative rate is admitted at the same magnitudes.
    L::MeasuredRecord both = record;
    both.velocity.direction = L::RecordDirection::BOTH;
    L::VelocityEvidence bothEvidence;
    CHECK(L::makeVelocityEvidence(bothEvidence, both, L::StopKind::HALT));
    CHECK(L::checkRecordVelocity(both, bothEvidence, f.velocity(-60), ramps, f.axis, state, f.target, now, halt));
    refused(L::checkRecordVelocity(both, bothEvidence, f.velocity(-61), ramps, f.axis, state, f.target, now, halt),
            E::SPEED, M::Err::ILLEGAL_VALUE);
}

void enableChecks() {
    const Fixture f;
    const L::MeasuredRecord record = enableRecord();
    L::EnableEvidence evidence;
    L::ActionOptions options;
    options.service.queueTimeoutUs = options.service.responseTimeoutUs = 100;
    options.observationIntervalUs = 1000;
    options.maximumAgeUs = 1000000;
    options.commandIntervalUs = L::ENABLE_WORD_INTERVAL_US;
    CHECK(L::makeEnableEvidence(evidence, options.identityReference, record, 42));
    L::ReadContext powerUp, latched, enabled;
    f.readState(powerUp, 0x8437, 0x0001, 1);
    f.readState(latched, 0x4050, 0x0014, 1);
    f.readState(enabled, 0x0437, 0x0001, 1);
    const uint64_t now = 400;
    CHECK(L::checkRecordEnable(record, evidence, options, powerUp, f.target, now));
    CHECK(L::checkRecordEnable(record, evidence, options, latched, f.target, now));
    refused(L::checkRecordEnable(record, evidence, options, enabled, f.target, now), E::START,
            M::Err::UNSUPPORTED);
    L::ReadContext mode3;
    f.readState(mode3, 0x8437, 0x0001, 3);
    refused(L::checkRecordEnable(record, evidence, options, mode3, f.target, now), E::MODE, M::Err::UNSUPPORTED);
    L::ActionOptions bare = options;
    bare.identityReference = L::IdentityReference();
    refused(L::checkRecordEnable(record, evidence, bare, powerUp, f.target, now), E::EVIDENCE,
            M::Err::INVALID_CONFIG);
    L::ActionOptions otherSpecimen = options;
    otherSpecimen.identityReference.values.revision = 7;
    refused(L::checkRecordEnable(record, evidence, otherSpecimen, powerUp, f.target, now), E::EVIDENCE,
            M::Err::INVALID_CONFIG);
    L::EnableEvidence attended = evidence;
    attended.gate = L::EnableGate::ATTENDED_EXPERIMENT;
    refused(L::checkRecordEnable(record, attended, options, powerUp, f.target, now), E::EVIDENCE,
            M::Err::INVALID_CONFIG);
    L::ActionOptions young = options;
    young.maximumAgeUs = 10;
    refused(L::checkRecordEnable(record, evidence, young, powerUp, f.target, 5000), E::STATE,
            M::Err::INVALID_CONFIG);
    refused(L::checkRecordEnable(velocityRecord(), evidence, options, powerUp, f.target, now), E::KIND,
            M::Err::INVALID_CONFIG);

    // A record stating mode 6 passes its own check, and prepareEnable still refuses a MEASURED enable
    // outside mode 1: the record does not widen the library's gate.
    L::MeasuredRecord homing = enableRecord();
    homing.enable.modeRaw = 6;
    L::EnableEvidence mode6;
    L::ActionOptions mode6Options = options;
    CHECK(L::makeEnableEvidence(mode6, mode6Options.identityReference, homing, 42));
    L::ReadContext inMode6;
    f.readState(inMode6, 0x8437, 0x0001, 6);
    CHECK(L::checkRecordEnable(homing, mode6, mode6Options, inMode6, f.target, now));
    static L::ActionContext output;
    const M::Status s = L::validateEnable(output, f.live, inMode6, f.identity, mode6, 42, now,
                                          now + 10000000, mode6Options);
    CHECK(!s && s.code == M::Err::UNSUPPORTED && s.detail == int32_t(M::ActionError::STATE));
    CHECK(std::strstr(s.msg, "mode 1 only"));
    // The mode 1 record's own evidence passes that same library check's gate rules.
    CHECK(L::validateEnable(output, f.live, powerUp, f.identity, evidence, 42, now, now + 10000000, options));
}
} // namespace

int main() {
    recordIds();
    moveEvidence();
    velocityEvidence();
    enableEvidence();
    moveChecks();
    velocityChecks();
    enableChecks();
    std::puts("record_checks_test passed");
    return 0;
}
