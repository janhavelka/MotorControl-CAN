// SPDX-License-Identifier: MIT
// prepareAccelerationRamp (prompt 60, slice b3): hand-computed ramp times in both modes, every refusal,
// a native 6083/6084 written from an acceleration producing the same traffic as the same native
// milliseconds, and records and library limits never widened by the unit form. The rigs' drives are
// the synthetic fakes of MoveRig.h and VelocityRig.h, not device behavior.
#include <MotorControlCAN/profiles/lichuan_c/Records.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
static unsigned checks = 0;
static bool asyncTraffic = false;
static const char* scenario = "acceleration ramp";
#define CHECK(condition)                                                                           \
    do {                                                                                           \
        ++checks;                                                                                  \
        if (!(condition)) {                                                                        \
            std::fprintf(stderr, "%s:%d: %s [%s]\n", __FILE__, __LINE__, #condition, scenario);    \
            std::abort();                                                                          \
        }                                                                                          \
    } while (false)
#include "support/MoveRig.h"
#include "support/VelocityRig.h"

namespace {
namespace M = MotorControlCAN;
namespace L = MotorControlCAN::LICHUAN_C;

template <class T> struct Snapshot {
    unsigned char bytes[sizeof(T)];
    explicit Snapshot(const T& value) { std::memcpy(bytes, static_cast<const void*>(&value), sizeof value); }
    bool unchanged(const T& value) const { return !std::memcmp(bytes, static_cast<const void*>(&value), sizeof value); }
};
const int32_t AXIS_BASE = static_cast<int32_t>(L::RampError::AXIS_ERROR_BASE);
bool refused(const M::Status& s, M::Err code, int32_t detail) {
    if (s.isOk() || s.code != code || s.detail != detail)
        std::fprintf(stderr, "status %d/%d: %s\n", int(s.code), int(s.detail), s.msg);
    return !s && s.code == code && s.detail == detail;
}
bool refused(const M::Status& s, M::Err code, L::RampError detail) { return refused(s, code, int32_t(detail)); }
bool equal(const M::Rational& value, int64_t n, uint64_t d) { return value.numerator == n && value.denominator == d; }

const M::AccelerationUnit RPM_PER_S(M::PositionUnit::TURNS, M::TimeUnit::MINUTE, M::TimeUnit::SECOND);
const M::AccelerationUnit TURNS_PER_S2(M::PositionUnit::TURNS);

// MoveRig's axis: 3200 command steps per motor turn and a 5:1 gear, native rate motor rpm.
M::AxisConfig moveAxis() { return MoveRig::axisConfig(); }
M::VelocityRequest rpm(const M::AxisConfig& axis, int64_t n, uint64_t d = 1) {
    M::VelocityRequest r;
    r.configurationGeneration = axis.binding.configurationGeneration;
    r.value = M::Rational(n, d);
    r.unit = M::VelocityUnit(M::PositionUnit::TURNS, M::TimeUnit::MINUTE);
    r.frame = M::CoordinateFrame::NATIVE;
    return r;
}
M::AccelerationRequest accel(const M::AxisConfig& axis, int64_t n, uint64_t d = 1,
                             M::AccelerationUnit unit = RPM_PER_S,
                             M::CoordinateFrame frame = M::CoordinateFrame::NATIVE,
                             M::Rounding rounding = M::Rounding::EXACT) {
    M::AccelerationRequest r;
    r.configurationGeneration = axis.binding.configurationGeneration;
    r.value = M::Rational(n, d);
    r.unit = unit;
    r.frame = frame;
    r.rounding = rounding;
    r.maximumAccelerationError = M::Rational(1000000);
    return r;
}

// Hand-computed: T [ms] = rpm x 1000 / a [rpm/s].
void moveStarts() {
    scenario = "move starts";
    const M::AxisConfig axis = moveAxis();
    M::PreparedRampTime out;
    // 120 rpm at 1200 rpm/s: 100 ms.
    CHECK(L::prepareAccelerationRamp(out, L::RampUse::MOVE_START, accel(axis, 1200), rpm(axis, 120), axis));
    CHECK(out.effectiveMs == 100 && equal(out.exactMs, 100, 1) && out.speedNative == 120);
    // The same as 4 load turns/s^2: x5 gear = 20 motor turns/s^2 = 1200 rpm/s.
    CHECK(L::prepareAccelerationRamp(out, L::RampUse::MOVE_START, accel(axis, 4, 1, TURNS_PER_S2, M::CoordinateFrame::LOAD),
                                     rpm(axis, 120), axis));
    CHECK(out.effectiveMs == 100 && equal(out.effectiveAcceleration, 4, 1));
    // 300 rpm at 1800 rpm/s: 500/3 ms; NEAREST 167, effective 1800 x (500/3) / 167 = 300000/167 rpm/s.
    CHECK(L::prepareAccelerationRamp(out, L::RampUse::MOVE_START,
                                     accel(axis, 1800, 1, RPM_PER_S, M::CoordinateFrame::NATIVE, M::Rounding::NEAREST),
                                     rpm(axis, 300), axis));
    CHECK(out.effectiveMs == 167 && equal(out.exactMs, 500, 3) && equal(out.effectiveAcceleration, 300000, 167));
    // The measured box, on the effective rpm: 2 and 300 convert, 1 and 301 refuse though 6081 admits them.
    CHECK(L::prepareAccelerationRamp(out, L::RampUse::MOVE_START, accel(axis, 20), rpm(axis, 2), axis) &&
          out.effectiveMs == 100);
    const Snapshot<M::PreparedRampTime> kept(out);
    CHECK(refused(L::prepareAccelerationRamp(out, L::RampUse::MOVE_START, accel(axis, 10), rpm(axis, 1), axis),
                  M::Err::UNSUPPORTED, L::RampError::OUTSIDE_LAW));
    CHECK(refused(L::prepareAccelerationRamp(out, L::RampUse::MOVE_START, accel(axis, 3010), rpm(axis, 301), axis),
                  M::Err::UNSUPPORTED, L::RampError::OUTSIDE_LAW));
    CHECK(kept.unchanged(out));
    // 300.4 rpm rounds to 300 (inside), 300.6 to 301 (outside): the box applies to what is written.
    M::VelocityRequest fractional = rpm(axis, 1502, 5);
    fractional.rounding = M::Rounding::NEAREST;
    fractional.maximumQuantizationError = M::Rational(1, 2);
    CHECK(L::prepareAccelerationRamp(out, L::RampUse::MOVE_START, accel(axis, 3000), fractional, axis) &&
          out.speedNative == 300 && out.effectiveMs == 100);
    fractional.value = M::Rational(1503, 5);
    CHECK(refused(L::prepareAccelerationRamp(out, L::RampUse::MOVE_START, accel(axis, 3000), fractional, axis),
                  M::Err::UNSUPPORTED, L::RampError::OUTSIDE_LAW));
    // Only the measured 100..2000 ms after rounding: 300 rpm at 3000 rpm/s is 100 ms (admitted), at
    // 100000/33 rpm/s exactly 99 ms (refused); at 150 rpm/s 2000 ms, at 149 rpm/s 2013.4 ms; at
    // 600000 rpm/s 0.5 ms, NEAREST 0.
    const int32_t limit = AXIS_BASE + int32_t(M::AxisError::LIMIT);
    CHECK(L::prepareAccelerationRamp(out, L::RampUse::MOVE_START, accel(axis, 3000), rpm(axis, 300), axis) &&
          out.effectiveMs == L::MOVE_RAMP_LAW_MINIMUM_MS);
    CHECK(refused(L::prepareAccelerationRamp(out, L::RampUse::MOVE_START, accel(axis, 100000, 33), rpm(axis, 300), axis),
                  M::Err::ILLEGAL_VALUE, limit));
    // 99.97 ms rounds to 100 under NEAREST and is admitted; FLOOR gives 99 and refuses.
    CHECK(L::prepareAccelerationRamp(out, L::RampUse::MOVE_START,
                                     accel(axis, 3001, 1, RPM_PER_S, M::CoordinateFrame::NATIVE, M::Rounding::NEAREST),
                                     rpm(axis, 300), axis) && out.effectiveMs == 100);
    CHECK(refused(L::prepareAccelerationRamp(out, L::RampUse::MOVE_START,
                                             accel(axis, 3001, 1, RPM_PER_S, M::CoordinateFrame::NATIVE, M::Rounding::FLOOR),
                                             rpm(axis, 300), axis),
                  M::Err::ILLEGAL_VALUE, limit));
    CHECK(L::prepareAccelerationRamp(out, L::RampUse::MOVE_START, accel(axis, 150), rpm(axis, 300), axis) &&
          out.effectiveMs == L::RAMP_LAW_MAXIMUM_MS);
    CHECK(refused(L::prepareAccelerationRamp(out, L::RampUse::MOVE_START,
                                             accel(axis, 149, 1, RPM_PER_S, M::CoordinateFrame::NATIVE, M::Rounding::NEAREST),
                                             rpm(axis, 300), axis),
                  M::Err::ILLEGAL_VALUE, limit));
    CHECK(refused(L::prepareAccelerationRamp(out, L::RampUse::MOVE_START,
                                             accel(axis, 600000, 1, RPM_PER_S, M::CoordinateFrame::NATIVE, M::Rounding::NEAREST),
                                             rpm(axis, 300), axis),
                  M::Err::ILLEGAL_VALUE, limit));
    // EXACT and a fractional time: prepareRampTime's FRACTIONAL_TARGET, with the base added.
    CHECK(refused(L::prepareAccelerationRamp(out, L::RampUse::MOVE_START, accel(axis, 1800), rpm(axis, 300), axis),
                  M::Err::ILLEGAL_VALUE,
                  AXIS_BASE + int32_t(M::AxisError::UNIT_ERROR_BASE) + int32_t(M::UnitError::FRACTIONAL_TARGET)));
}

void velocityStarts() {
    scenario = "velocity starts";
    const M::AxisConfig axis = VelocityRig::axisConfig(); // Direct drive, motor rpm.
    M::PreparedRampTime out;
    // 120 rpm at 600 rpm/s: 200 ms, the measured minimum; the same magnitude for -120 rpm.
    CHECK(L::prepareAccelerationRamp(out, L::RampUse::VELOCITY_START, accel(axis, 600), rpm(axis, 120), axis));
    CHECK(out.effectiveMs == 200 && out.speedNative == 120);
    CHECK(L::prepareAccelerationRamp(out, L::RampUse::VELOCITY_START, accel(axis, 600), rpm(axis, -120), axis));
    CHECK(out.effectiveMs == 200 && out.speedNative == -120);
    // 120 rpm at 120000/199 rpm/s is exactly 199 ms, below the measured 200: refused; 2000 ms passes,
    // 2001 ms (120 rpm at 120000/2001 rpm/s) refuses.
    const int32_t limit = AXIS_BASE + int32_t(M::AxisError::LIMIT);
    CHECK(refused(L::prepareAccelerationRamp(out, L::RampUse::VELOCITY_START, accel(axis, 120000, 199), rpm(axis, 120), axis),
                  M::Err::ILLEGAL_VALUE, limit));
    CHECK(L::prepareAccelerationRamp(out, L::RampUse::VELOCITY_START, accel(axis, 60), rpm(axis, 120), axis) &&
          out.effectiveMs == L::RAMP_LAW_MAXIMUM_MS);
    CHECK(refused(L::prepareAccelerationRamp(out, L::RampUse::VELOCITY_START, accel(axis, 120000, 2001), rpm(axis, 120), axis),
                  M::Err::ILLEGAL_VALUE, limit));
    // A 100 ms velocity ramp is inside the move range but below the velocity one.
    CHECK(refused(L::prepareAccelerationRamp(out, L::RampUse::VELOCITY_START, accel(axis, 1200), rpm(axis, 120), axis),
                  M::Err::ILLEGAL_VALUE, limit));
    // 10 motor turns/s^2 = 600 rpm/s in the MOTOR frame.
    CHECK(L::prepareAccelerationRamp(out, L::RampUse::VELOCITY_START, accel(axis, 10, 1, TURNS_PER_S2, M::CoordinateFrame::MOTOR),
                                     rpm(axis, 120), axis) && out.effectiveMs == 200);
    // Box 6..4500 rpm: 6 at 3 rpm/s is 2000 ms; 4500 at 2250 rpm/s is 2000 ms; 5 and 4501 refuse.
    CHECK(L::prepareAccelerationRamp(out, L::RampUse::VELOCITY_START, accel(axis, 3), rpm(axis, 6), axis) &&
          out.effectiveMs == 2000);
    CHECK(L::prepareAccelerationRamp(out, L::RampUse::VELOCITY_START, accel(axis, 2250), rpm(axis, -4500), axis) &&
          out.effectiveMs == 2000);
    const Snapshot<M::PreparedRampTime> kept(out);
    CHECK(refused(L::prepareAccelerationRamp(out, L::RampUse::VELOCITY_START, accel(axis, 50), rpm(axis, 5), axis),
                  M::Err::UNSUPPORTED, L::RampError::OUTSIDE_LAW));
    CHECK(refused(L::prepareAccelerationRamp(out, L::RampUse::VELOCITY_START, accel(axis, 45010), rpm(axis, 4501), axis),
                  M::Err::UNSUPPORTED, L::RampError::OUTSIDE_LAW));
    CHECK(kept.unchanged(out));
    // 2 rpm is inside the move law but outside the velocity law.
    CHECK(refused(L::prepareAccelerationRamp(out, L::RampUse::VELOCITY_START, accel(axis, 20), rpm(axis, 2), axis),
                  M::Err::UNSUPPORTED, L::RampError::OUTSIDE_LAW));
    // 3000 rpm is inside the velocity law but outside the move law.
    CHECK(L::prepareAccelerationRamp(out, L::RampUse::VELOCITY_START, accel(axis, 3000), rpm(axis, 3000), axis) &&
          out.effectiveMs == 1000);
    CHECK(refused(L::prepareAccelerationRamp(out, L::RampUse::MOVE_START, accel(axis, 3000), rpm(axis, 3000), axis),
                  M::Err::UNSUPPORTED, L::RampError::OUTSIDE_LAW));
}

// Every unmeasured use refuses before anything is computed, even with a speed that cannot be
// prepared, and leaves the output unchanged.
void refusals() {
    scenario = "refusals";
    const M::AxisConfig axis = moveAxis();
    M::PreparedRampTime out;
    CHECK(L::prepareAccelerationRamp(out, L::RampUse::MOVE_START, accel(axis, 1200), rpm(axis, 120), axis));
    const Snapshot<M::PreparedRampTime> kept(out);
    M::VelocityRequest broken = rpm(axis, 120);
    broken.configurationGeneration = 0;
    const L::RampUse refusedUses[] = {L::RampUse::MOVE_DECELERATION, L::RampUse::VELOCITY_DECELERATION,
                                      L::RampUse::VELOCITY_CHANGE, L::RampUse::MOVE_REPLACEMENT, L::RampUse::HOMING};
    for (unsigned i = 0; i < sizeof refusedUses / sizeof refusedUses[0]; ++i) {
        CHECK(refused(L::prepareAccelerationRamp(out, refusedUses[i], accel(axis, 1200), rpm(axis, 120), axis),
                      M::Err::UNSUPPORTED, L::RampError::UNMEASURED));
        CHECK(refused(L::prepareAccelerationRamp(out, refusedUses[i], accel(axis, 0), broken, axis),
                      M::Err::UNSUPPORTED, L::RampError::UNMEASURED));
        CHECK(kept.unchanged(out));
    }
    CHECK(refused(L::prepareAccelerationRamp(out, static_cast<L::RampUse>(7), accel(axis, 1200), rpm(axis, 120), axis),
                  M::Err::ILLEGAL_VALUE, L::RampError::ARGUMENT));
    // The speed's own preparation, with the base added and its Err kept.
    CHECK(refused(L::prepareAccelerationRamp(out, L::RampUse::MOVE_START, accel(axis, 1200), broken, axis),
                  M::Err::INVALID_CONFIG, AXIS_BASE + int32_t(M::AxisError::STALE_GENERATION)));
    // A speed that is not motor rpm.
    M::AxisConfig steps = axis;
    steps.nativeRateBasis = M::NativeRateBasis::COMMAND_INCREMENTS;
    steps.nativeVelocityTime = M::TimeUnit::SECOND;
    M::VelocityRequest motorRpm = rpm(steps, 120);
    motorRpm.frame = M::CoordinateFrame::MOTOR;
    CHECK(refused(L::prepareAccelerationRamp(out, L::RampUse::MOVE_START, accel(steps, 1200, 1, RPM_PER_S, M::CoordinateFrame::MOTOR),
                                             motorRpm, steps),
                  M::Err::INVALID_CONFIG, L::RampError::RATE_BASIS));
    M::AxisConfig binary = axis;
    binary.velocityRepresentation = M::NativeRepresentation::BINARY32;
    CHECK(refused(L::prepareAccelerationRamp(out, L::RampUse::MOVE_START, accel(binary, 1200), rpm(binary, 120), binary),
                  M::Err::INVALID_CONFIG, L::RampError::RATE_BASIS));
    // prepareRampTime's refusals: zero and negative accelerations, radians, a stale acceleration.
    const int32_t invalid = AXIS_BASE + int32_t(M::AxisError::INVALID_ARGUMENT);
    CHECK(refused(L::prepareAccelerationRamp(out, L::RampUse::MOVE_START, accel(axis, 0), rpm(axis, 120), axis),
                  M::Err::ILLEGAL_VALUE, invalid));
    CHECK(refused(L::prepareAccelerationRamp(out, L::RampUse::MOVE_START, accel(axis, -1200), rpm(axis, 120), axis),
                  M::Err::ILLEGAL_VALUE, invalid));
    CHECK(refused(L::prepareAccelerationRamp(out, L::RampUse::VELOCITY_START,
                                             accel(axis, 20, 1, M::AccelerationUnit(M::PositionUnit::RADIANS), M::CoordinateFrame::MOTOR),
                                             rpm(axis, 120), axis),
                  M::Err::UNSUPPORTED,
                  AXIS_BASE + int32_t(M::AxisError::UNIT_ERROR_BASE) + int32_t(M::UnitError::APPROXIMATION_REQUIRED)));
    M::AccelerationRequest stale = accel(axis, 1200);
    ++stale.configurationGeneration;
    CHECK(refused(L::prepareAccelerationRamp(out, L::RampUse::MOVE_START, stale, rpm(axis, 120), axis),
                  M::Err::INVALID_CONFIG, AXIS_BASE + int32_t(M::AxisError::STALE_GENERATION)));
    CHECK(kept.unchanged(out));
}

// Records narrow a converted ramp or speed exactly as they narrow native ones: the profile goes
// through checkRecordMove and checkRecordVelocity unchanged.
L::RecordRange range(uint32_t minimum, uint32_t maximum) {
    L::RecordRange r;
    r.minimum = minimum;
    r.maximum = maximum;
    r.basis = L::RecordBasis::MEASURED;
    return r;
}
void setEvidence(L::MeasuredRecord& r, const char* text) {
    std::memset(r.evidence, 0, sizeof r.evidence);
    std::strncpy(r.evidence, text, sizeof r.evidence - 1);
}
L::MeasuredRecord positionRecord() {
    L::MeasuredRecord r;
    r.kind = L::RecordKind::POSITION;
    setEvidence(r, "p60-position-fixture");
    r.model = L::SettingsModel::CL86_C;
    r.identity = L::measuredIdentity();
    L::PositionScope& p = r.position;
    p.move = L::RecordMoveKind::ABSOLUTE;
    p.rpm = range(2, 60);
    p.rampMs = range(200, 2000);
    p.maximumDistance = 2000;
    p.distanceBasis = L::RecordBasis::MEASURED;
    p.acceptance.source = L::MoveSignalSource::STATUSWORD;
    p.acceptance.mask = p.acceptance.value = 0x1000;
    p.completion.source = L::MoveSignalSource::MONITOR;
    p.completion.mask = p.completion.value = 0x0001;
    p.rule = L::CompletionRule::AFTER_ABSENCE;
    p.signalsBasis = L::RecordBasis::MEASURED;
    return r;
}
L::MeasuredRecord velocityRecord() {
    L::MeasuredRecord r;
    r.kind = L::RecordKind::VELOCITY;
    setEvidence(r, "p60-velocity-fixture");
    r.model = L::SettingsModel::CL86_C;
    r.identity = L::measuredIdentity();
    L::VelocityScope& v = r.velocity;
    v.layout = L::StatusLayout::ZH86;
    v.layoutBasis = L::RecordBasis::DOCUMENTED;
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
    v.maximumWindowUs = 800000;
    v.windowBasis = L::RecordBasis::INFERRED;
    v.starts.reports[0].statusword = 0x8050;
    v.starts.reports[0].monitor = 0x0011;
    v.starts.count = 1;
    v.starts.basis = L::RecordBasis::MEASURED;
    return r;
}
void recordsNeverWidened() {
    scenario = "records";
    const M::AxisConfig axis = moveAxis();
    const L::MeasuredRecord record = positionRecord();
    L::MoveEvidence evidence;
    CHECK(L::makeMoveEvidence(evidence, record));
    M::MoveRequest move = MoveRig::moveRequest(1000, 60);
    M::PreparedRampTime ramp;
    // 60 rpm at 600 rpm/s: 100 ms, inside the law and 1..2000 ms, below the record's 200 ms.
    CHECK(L::prepareAccelerationRamp(ramp, L::RampUse::MOVE_START, accel(axis, 600), move.speed, axis) &&
          ramp.effectiveMs == 100);
    L::MoveProfile profile = MoveRig::profile(ramp.effectiveMs, 500);
    CHECK(refused(L::checkRecordMove(record, evidence, move, profile, axis, nullptr, 0), M::Err::ILLEGAL_VALUE,
                  int32_t(L::RecordCheckError::RAMP)));
    // 120 rpm is inside the law's 2..300 rpm but above the record's 60: the record still refuses.
    move = MoveRig::moveRequest(1000, 120);
    CHECK(L::prepareAccelerationRamp(ramp, L::RampUse::MOVE_START, accel(axis, 600), move.speed, axis) &&
          ramp.effectiveMs == 200);
    profile = MoveRig::profile(ramp.effectiveMs, 500);
    CHECK(refused(L::checkRecordMove(record, evidence, move, profile, axis, nullptr, 0), M::Err::ILLEGAL_VALUE,
                  int32_t(L::RecordCheckError::SPEED)));

    const M::AxisConfig vAxis = VelocityRig::axisConfig();
    const L::MeasuredRecord vRecord = velocityRecord();
    L::VelocityOptions options = VelocityRig::options();
    L::VelocityEvidence vEvidence;
    CHECK(L::makeVelocityEvidence(vEvidence, vRecord, options.stopKind));
    M::FiniteVelocityRequest run = VelocityRig::request(30);
    // 30 rpm at 100 rpm/s: 300 ms for both ramps, inside the law, outside the record's 200..200 ms.
    CHECK(L::prepareAccelerationRamp(ramp, L::RampUse::VELOCITY_START, accel(vAxis, 100), run.velocity, vAxis) &&
          ramp.effectiveMs == 300);
    L::VelocityProfile vProfile;
    vProfile.rampUpTimeMs = vProfile.rampDownTimeMs = ramp.effectiveMs;
    L::ReadContext state;
    CHECK(refused(L::checkRecordVelocity(vRecord, vEvidence, run, vProfile, vAxis, state, VelocityRig::target(), 0,
                                         options),
                  M::Err::ILLEGAL_VALUE, int32_t(L::RecordCheckError::RAMP)));
}

// A move whose 6083 comes from an acceleration sends exactly the frames of the same move given the
// same milliseconds natively.
M::Frame recorded[OwnerRig::WireBase::CAPACITY];
unsigned recordedCount = 0;
void runMove(const L::MoveProfile& profile, bool compare) {
    MoveRig::Rig r;
    r.ready();
    CHECK(r.request(MoveRig::evidence(), MoveRig::moveRequest(), profile).isOk());
    r.finishMove();
    CHECK(MoveRig::moveOf(r).state == M::ActionState::SUCCEEDED);
    CHECK(MoveRig::downloadsFrom(r, r.startFrame, 0x6083) == 1);
    const unsigned count = r.wire.sentCount - r.startFrame;
    if (!compare) {
        recordedCount = count;
        for (unsigned i = 0; i < count; ++i)
            recorded[i] = r.wire.sent[r.startFrame + i];
        return;
    }
    CHECK(count == recordedCount);
    for (unsigned i = 0; i < count; ++i) {
        const M::Frame& a = recorded[i];
        const M::Frame& b = r.wire.sent[r.startFrame + i];
        CHECK(a.id == b.id && a.dlc == b.dlc && a.payloadLength == b.payloadLength &&
              !std::memcmp(a.data, b.data, sizeof a.data));
    }
}
void moveTraffic() {
    scenario = "move traffic";
    runMove(MoveRig::profile(100, 200), false);
    // 120 rpm (moveRequest's speed) at 4 load turns/s^2 = 1200 rpm/s: 100 ms, written to 6083 only.
    const M::AxisConfig axis = moveAxis();
    M::PreparedRampTime ramp;
    CHECK(L::prepareAccelerationRamp(ramp, L::RampUse::MOVE_START,
                                     accel(axis, 4, 1, TURNS_PER_S2, M::CoordinateFrame::LOAD),
                                     MoveRig::moveRequest().speed, axis));
    L::MoveProfile profile = MoveRig::profile(0, 200);
    profile.rampUpTimeMs = ramp.effectiveMs;
    runMove(profile, true);
}
void runVelocity(const L::VelocityProfile& profile, bool compare, const M::FiniteVelocityRequest& run) {
    VelocityRig::Rig r;
    r.ready();
    r.startFrame = r.wire.sentCount;
    CHECK(r.session.requestVelocity(r.state, r.identity, VelocityRig::evidence(), run, profile, r.axis,
                                    VelocityRig::options(), r.now + VelocityRig::DEADLINE, r.now).isOk());
    r.finish();
    CHECK(r.writes(0x6083, profile.rampUpTimeMs) == 1 && r.writes(0x6084, profile.rampDownTimeMs) == 1);
    const unsigned count = r.wire.sentCount - r.startFrame;
    if (!compare) {
        recordedCount = count;
        for (unsigned i = 0; i < count; ++i)
            recorded[i] = r.wire.sent[r.startFrame + i];
        return;
    }
    CHECK(count == recordedCount);
    for (unsigned i = 0; i < count; ++i) {
        const M::Frame& a = recorded[i];
        const M::Frame& b = r.wire.sent[r.startFrame + i];
        CHECK(a.id == b.id && a.dlc == b.dlc && a.payloadLength == b.payloadLength &&
              !std::memcmp(a.data, b.data, sizeof a.data));
    }
}
void velocityTraffic() {
    scenario = "velocity traffic";
    const M::FiniteVelocityRequest run = VelocityRig::request(); // 120 rpm.
    L::VelocityProfile native;
    native.rampUpTimeMs = native.rampDownTimeMs = 200;
    runVelocity(native, false, run);
    // 120 rpm at 600 rpm/s: 200 ms, written to both 6083 and 6084.
    const M::AxisConfig axis = VelocityRig::axisConfig();
    M::PreparedRampTime ramp;
    CHECK(L::prepareAccelerationRamp(ramp, L::RampUse::VELOCITY_START, accel(axis, 600), run.velocity, axis));
    L::VelocityProfile converted;
    converted.rampUpTimeMs = converted.rampDownTimeMs = ramp.effectiveMs;
    runVelocity(converted, true, run);
    // The law reaches 4500 rpm; the run's own evidence (300 rpm) still refuses 3500 rpm before any frame.
    VelocityRig::Rig r;
    r.ready();
    const M::FiniteVelocityRequest fast = VelocityRig::request(3500);
    CHECK(L::prepareAccelerationRamp(ramp, L::RampUse::VELOCITY_START, accel(axis, 3500), fast.velocity, axis) &&
          ramp.effectiveMs == 1000);
    L::VelocityProfile wide;
    wide.rampUpTimeMs = wide.rampDownTimeMs = ramp.effectiveMs;
    const unsigned before = r.wire.sentCount;
    CHECK(!r.session.requestVelocity(r.state, r.identity, VelocityRig::evidence(), fast, wide, r.axis,
                                     VelocityRig::options(), r.now + VelocityRig::DEADLINE, r.now));
    CHECK(r.wire.sentCount == before);
}
} // namespace

int main() {
    moveStarts();
    velocityStarts();
    refusals();
    recordsNeverWidened();
    moveTraffic();
    velocityTraffic();
    std::printf("acceleration ramp tests passed: %u checks\n", checks);
    return 0;
}
