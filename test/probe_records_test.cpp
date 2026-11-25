// SPDX-License-Identifier: MIT
// HOST_FAKE: measured records through ProbeMotion (prompt 58, slice b3) over the actual owner, SDO engine
// and AxisSession: load validates the whole record, use binds it to the axis's generations and sends
// nothing, a bound record supplies the evidence and narrows requests, rebinding, recovery and restart
// invalidate or forget it, drop returns the setters' gates. The records and the fake drive are test
// inputs; none qualifies a drive.
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
static unsigned checks = 0;
static const char* scenario = "";
#define CHECK(condition)                                                                           \
    do {                                                                                           \
        ++checks;                                                                                  \
        if (!(condition)) {                                                                        \
            std::fprintf(stderr, "%s:%d: %s [%s]\n", __FILE__, __LINE__, #condition, scenario);    \
            std::abort();                                                                          \
        }                                                                                          \
    } while (false)
#include "ProbeMotion.h"
#include "support/VelocityRig.h"

namespace {
using namespace VelocityRig;
bool discard(void*, uint8_t) { return true; }
E::ProbeCallbacks quiet() {
    E::ProbeCallbacks c;
    c.writeByte = discard;
    return c;
}
struct RecordRig : Rig {
    E::ProbePlatform platform;
    E::ProbeApp app;
    L::ReadTarget live;
    M::AxisReference reference;
    E::ProbeMotion motion;
    E::AxisRecords records;
    RecordRig() : platform(quiet()), app(owner, platform), live(target()), motion(app, owner, session, live, axis, reference) {
        serviceSession = false;
        CHECK(app.select(live));
        CHECK(app.openAxis(session, live, L::SettingsModel::CL86_C, now) == &session);
        CHECK(wire.sentCount == 0);
    }
    void tick() {
        now += TICK;
        if (wire.rxRead == wire.rxWrite)
            wire.rxRead = wire.rxWrite = 0;
        for (; served < wire.sentCount; ++served) {
            if (wire.pending[served])
                break;
            M::Frame reply;
            if (drive.respond(wire.sent[served], reply))
                wire.push(reply, now, owner.controllerEpoch());
        }
        CHECK(app.service(now));
        CHECK(motion.service(now));
    }
    void acquire(L::ReadContext& into) {
        for (unsigned i = 0; into.state == M::ReadState::ACTIVE && i < 64; ++i) {
            L::PreparedRead work;
            CHECK(L::nextRead(into, now, work) && work.ready);
            CHECK(session.admitPrepared(work.transfer, now) == O::BusAdmission::ACCEPTED);
            tick();
            tick();
            C::SdoCompletion terminal;
            CHECK(session.settle(terminal) && L::advanceRead(into, terminal, now));
        }
        CHECK(into.state == M::ReadState::SUCCEEDED);
    }
    // A fresh identity and a full ERRORS, MONITORS, STATUS and FEEDBACK read of the live target.
    void acquireIdentityAndState() {
        uint32_t operation = session.beginOperation(now + DEADLINE, now);
        CHECK(operation && L::prepareIdentity(identityRead, live, operation, now, now + DEADLINE, reads()));
        acquire(identityRead);
        CHECK(L::getIdentity(identityRead, live, now, DEADLINE, identity));
        operation = session.beginOperation(now + DEADLINE, now);
        const uint8_t mask = uint8_t((1u << unsigned(L::StateBlock::ERRORS)) |
                                     (1u << unsigned(L::StateBlock::MONITORS)) |
                                     (1u << unsigned(L::StateBlock::STATUS)) |
                                     (1u << unsigned(L::StateBlock::FEEDBACK)));
        CHECK(operation && L::prepareState(state, live, operation, now, now + DEADLINE, reads(), mask));
        acquire(state);
    }
    void finish() {
        for (unsigned i = 0; session.busy() && i < 1500; ++i)
            tick();
        CHECK(!session.busy());
        for (unsigned i = 0; i < 50; ++i)
            tick();
    }
    // The fake drive's HALT leaves it enabled and halted; the next run starts from a released standstill,
    // as a release between runs would leave it. Test fixture state only.
    void released() {
        drive.status = 1;
        drive.monitor = L::MONITOR_RELEASED;
    }
    // A finite velocity request on the axis's current configuration generation.
    M::FiniteVelocityRequest run(int64_t rpm) const {
        M::FiniteVelocityRequest value = request(rpm);
        value.velocity.configurationGeneration = axis.binding.configurationGeneration;
        return value;
    }
    // Velocity prerequisites with CLOSED evidence: only a bound record can open the gate.
    void armVelocity() {
        acquireIdentityAndState();
        CHECK(motion.setVelocityPrerequisites(state, identity, L::VelocityEvidence(), profile(), options()));
        startFrame = wire.sentCount;
    }
};
L::IdentityValues rigIdentity() {
    L::IdentityValues i;
    i.deviceType = DEVICE_TYPE;
    i.hardwareVersion = HARDWARE;
    i.softwareVersion = SOFTWARE;
    i.vendorId = VENDOR;
    i.productCode = PRODUCT;
    i.revision = REVISION;
    i.modelCode = MODEL;
    i.firmwareCode = FIRMWARE;
    return i;
}
L::RecordRange range(uint32_t minimum, uint32_t maximum) {
    L::RecordRange r;
    r.minimum = minimum;
    r.maximum = maximum;
    r.basis = L::RecordBasis::MEASURED;
    return r;
}
// A record of the rig's fake drive: its identity, 2201 (the axis's 3200), released start and ramps.
L::MeasuredRecord velocityRecord(uint32_t maximumRpm = 60) {
    L::MeasuredRecord r;
    r.kind = L::RecordKind::VELOCITY;
    std::strncpy(r.evidence, "probe-records-velocity", sizeof r.evidence - 1);
    r.model = L::SettingsModel::CL86_C;
    r.identity = rigIdentity();
    L::VelocityScope& v = r.velocity;
    v.layout = L::StatusLayout::ZH86;
    v.layoutBasis = L::RecordBasis::DOCUMENTED;
    v.rpm = range(6, maximumRpm);
    v.direction = L::RecordDirection::FORWARD;
    v.directionBasis = v.durationBasis = v.scaleBasis = L::RecordBasis::MEASURED;
    v.rampMs = range(100, 200);
    v.maximumDurationUs = DURATION;
    v.scale2201 = 3200;
    v.halt.admitted = true;
    v.halt.rpm = range(6, maximumRpm);
    v.windowBasis = L::RecordBasis::INFERRED;
    v.starts.count = 1;
    v.starts.reports[0].statusword = 0x0001;
    v.starts.reports[0].monitor = L::MONITOR_RELEASED;
    v.starts.basis = L::RecordBasis::MEASURED;
    return r;
}
L::MeasuredRecord enableRecord(uint16_t statusword) {
    L::MeasuredRecord r;
    r.kind = L::RecordKind::ENABLE;
    std::strncpy(r.evidence, "probe-records-enable", sizeof r.evidence - 1);
    r.model = L::SettingsModel::CL86_C;
    r.identity = rigIdentity();
    r.enable.modeRaw = 1;
    r.enable.modeBasis = L::RecordBasis::MEASURED;
    r.enable.starts.count = 1;
    r.enable.starts.reports[0].statusword = statusword;
    r.enable.starts.reports[0].monitor = L::MONITOR_RELEASED;
    r.enable.starts.basis = L::RecordBasis::MEASURED;
    return r;
}
L::MeasuredRecord absoluteRecord() {
    L::MeasuredRecord r;
    r.kind = L::RecordKind::POSITION;
    std::strncpy(r.evidence, "probe-records-absolute", sizeof r.evidence - 1);
    r.model = L::SettingsModel::CL86_C;
    r.identity = rigIdentity();
    L::PositionScope& p = r.position;
    p.move = L::RecordMoveKind::ABSOLUTE;
    p.rpm = range(2, 300);
    p.rampMs = range(100, 2000);
    p.maximumDistance = 2000;
    p.distanceBasis = p.signalsBasis = L::RecordBasis::MEASURED;
    p.acceptance.source = L::MoveSignalSource::STATUSWORD;
    p.acceptance.mask = p.acceptance.value = 0x1000;
    p.completion.source = L::MoveSignalSource::MONITOR;
    p.completion.mask = p.completion.value = 0x0001;
    p.rule = L::CompletionRule::AFTER_ABSENCE;
    return r;
}
struct Line {
    char text[L::RECORD_TEXT_CAPACITY + 1];
    size_t size = 0;
};
Line line(const L::MeasuredRecord& record) {
    Line l;
    L::RecordRefusal refusal;
    CHECK(L::encodeRecord(l.text, sizeof l.text, l.size, refusal, record));
    return l;
}
// Load and use through the line, with nothing sent and no operation started.
void loadAndUse(RecordRig& r, const L::MeasuredRecord& record, E::RecordSlot expected) {
    const unsigned sent = r.wire.sentCount;
    const Line l = line(record);
    L::RecordRefusal refusal;
    E::RecordSlot slot = E::RecordSlot::ENABLE;
    CHECK(r.motion.loadRecord(l.text, l.size, refusal, slot) && slot == expected);
    CHECK(refusal.reason == L::RecordError::NONE);
    CHECK(r.motion.useRecord(slot, r.now));
    CHECK(r.wire.sentCount == sent && !r.session.busy() && !r.motion.retained());
}

void loadValidatesTheWholeRecord() {
    scenario = "load validates the whole record before it takes effect";
    RecordRig r;
    L::RecordRefusal refusal;
    E::RecordSlot slot = E::RecordSlot::ENABLE;
    Line good = line(velocityRecord());
    // No storage: refused, nothing kept.
    CHECK(!r.motion.loadRecord(good.text, good.size, refusal, slot));
    r.motion.attachRecords(r.records);
    E::RecordView view;
    for (unsigned s = 0; s < E::RECORD_SLOTS; ++s)
        CHECK(!r.motion.showRecord(E::RecordSlot(s), view));
    // A corrupted line, a partial one and an out-of-range one are refused whole: the slot stays empty.
    Line bad = good;
    bad.text[30] = bad.text[30] == 'a' ? 'b' : 'a';
    M::Status s = r.motion.loadRecord(bad.text, bad.size, refusal, slot);
    CHECK(!s && refusal.reason == L::RecordError::CRC && std::strcmp(refusal.key, "crc") == 0);
    s = r.motion.loadRecord(good.text, good.size - 9, refusal, slot);
    CHECK(!s && refusal.reason != L::RecordError::NONE);
    L::MeasuredRecord wide = velocityRecord();
    wide.velocity.rpm.maximum = 4000;
    s = r.motion.loadRecord(wide, refusal, slot);
    CHECK(!s && refusal.reason == L::RecordError::LIMIT && std::strcmp(refusal.key, "rpm") == 0);
    CHECK(!r.motion.showRecord(E::RecordSlot::VELOCITY, view));
    // A valid line fills its slot, LOADED, with its id; a second record of that slot waits for a drop.
    slot = E::RecordSlot::ENABLE;
    CHECK(r.motion.loadRecord(good.text, good.size, refusal, slot) && slot == E::RecordSlot::VELOCITY);
    uint32_t id = 0;
    CHECK(L::recordId(id, velocityRecord()));
    CHECK(r.motion.showRecord(E::RecordSlot::VELOCITY, view) && view.state == E::RecordState::LOADED &&
          view.id == id && view.slot == E::RecordSlot::VELOCITY && view.record.velocity.rpm.maximum == 60 &&
          !view.binding.binding.logicalAxisId);
    s = r.motion.loadRecord(velocityRecord(30), refusal, slot);
    CHECK(!s && s.detail == int32_t(M::ActionError::STATE));
    CHECK(r.motion.showRecord(E::RecordSlot::VELOCITY, view) && view.record.velocity.rpm.maximum == 60);
    // Each kind and move has its own slot.
    CHECK(r.motion.loadRecord(enableRecord(0x0001), refusal, slot) && slot == E::RecordSlot::ENABLE);
    CHECK(r.motion.loadRecord(absoluteRecord(), refusal, slot) && slot == E::RecordSlot::POSITION_ABSOLUTE);
    CHECK(!r.motion.showRecord(E::RecordSlot::POSITION_RELATIVE, view));
    CHECK(r.wire.sentCount == 0 && !r.session.busy() && !r.motion.retained());
}

void useBindsAndSendsNothing() {
    scenario = "use binds to the axis generations and never enables or moves";
    RecordRig r;
    r.motion.attachRecords(r.records);
    CHECK(!r.motion.useRecord(E::RecordSlot::VELOCITY, r.now)); // Nothing loaded.
    loadAndUse(r, velocityRecord(), E::RecordSlot::VELOCITY);
    E::RecordView view;
    CHECK(r.motion.showRecord(E::RecordSlot::VELOCITY, view) && view.state == E::RecordState::BOUND);
    CHECK(M::sameBinding(view.binding.binding, r.axis.binding) &&
          view.binding.nodeGeneration == r.live.nodeGeneration &&
          view.binding.controllerEpoch == r.owner.controllerEpoch() && view.binding.boundUs == r.now);
    CHECK(r.drive.status == 1 && r.drive.monitor == L::MONITOR_RELEASED && r.drive.velocity == 0);
    // A record of another model is never bound to this CL86-C session.
    L::MeasuredRecord other = enableRecord(0x0001);
    other.model = L::SettingsModel::OL86_C;
    L::RecordRefusal refusal;
    E::RecordSlot slot = E::RecordSlot::VELOCITY;
    CHECK(r.motion.loadRecord(other, refusal, slot) && slot == E::RecordSlot::ENABLE);
    const M::Status s = r.motion.useRecord(E::RecordSlot::ENABLE, r.now);
    CHECK(!s && s.detail == int32_t(M::ActionError::TARGET));
    CHECK(r.motion.showRecord(E::RecordSlot::ENABLE, view) && view.state == E::RecordState::LOADED);
    // A backward clock is refused.
    r.tick();
    CHECK(!r.motion.useRecord(E::RecordSlot::VELOCITY, r.now - 1));
    CHECK(r.wire.sentCount == 0);
}

void boundRecordLimitsAndOpensVelocity() {
    scenario = "a bound velocity record supplies the evidence and narrows the run";
    RecordRig r;
    r.motion.attachRecords(r.records);
    r.armVelocity();
    // Loaded but not bound: the setter's CLOSED evidence still governs and refuses.
    L::RecordRefusal refusal;
    E::RecordSlot slot = E::RecordSlot::ENABLE;
    CHECK(r.motion.loadRecord(velocityRecord(), refusal, slot));
    unsigned sent = r.wire.sentCount;
    M::Status s = r.motion.requestVelocity(r.run(60), L::StopKind::HALT, r.now, r.now + DEADLINE);
    CHECK(!s && s.code == M::Err::UNSUPPORTED && r.wire.sentCount == sent && !r.motion.retained());
    CHECK(r.motion.useRecord(slot, r.now));
    // Bound: a rate above the record's range, or a stop kind it does not admit, is refused before any TX.
    s = r.motion.requestVelocity(r.run(120), L::StopKind::HALT, r.now, r.now + DEADLINE);
    CHECK(!s && s.detail == int32_t(L::RecordCheckError::SPEED) && r.wire.sentCount == sent);
    s = r.motion.requestVelocity(r.run(60), L::StopKind::QUICK_STOP, r.now, r.now + DEADLINE);
    CHECK(!s && s.detail == int32_t(L::RecordCheckError::STOP_KIND) && r.wire.sentCount == sent);
    CHECK(!r.motion.retained());
    // Within the record: the run uses the record's evidence, id included, and completes on the fake.
    CHECK(r.motion.requestVelocity(r.run(60), L::StopKind::HALT, r.now, r.now + DEADLINE));
    const L::VelocityContext* native = r.motion.nativeVelocity();
    uint32_t id = 0;
    CHECK(native && L::recordId(id, velocityRecord()) && native->evidence.record == id &&
          native->evidence.gate == L::VelocityGate::MEASURED && native->evidence.maximumForwardRpm == 60);
    CHECK(r.motion.normalResult().record == id && !r.motion.normalResult().attendedExperiment);
    r.finish();
    CHECK(r.motion.normalResult().kind == E::MotionKind::VELOCITY &&
          r.motion.normalResult().state == M::ActionState::SUCCEEDED);
    CHECK(r.writes(0x60ff, 60) == 1);
    CHECK(r.motion.releaseNormal());
    r.released();
    // Dropped: the setter's evidence governs again.
    CHECK(r.motion.dropRecord(E::RecordSlot::VELOCITY) && !r.motion.dropRecord(E::RecordSlot::VELOCITY));
    E::RecordView view;
    CHECK(!r.motion.showRecord(E::RecordSlot::VELOCITY, view));
    r.armVelocity();
    sent = r.wire.sentCount;
    s = r.motion.requestVelocity(r.run(60), L::StopKind::HALT, r.now, r.now + DEADLINE);
    CHECK(!s && s.code == M::Err::UNSUPPORTED && r.wire.sentCount == sent);
    // With the rig's own explicit evidence and no bound record, the existing gate admits the faster run
    // (a fresh rig: the fake wire's capture capacity holds one run per rig).
    RecordRig fresh;
    fresh.motion.attachRecords(fresh.records);
    loadAndUse(fresh, velocityRecord(), E::RecordSlot::VELOCITY);
    CHECK(fresh.motion.dropRecord(E::RecordSlot::VELOCITY));
    fresh.acquireIdentityAndState();
    CHECK(fresh.motion.setVelocityPrerequisites(fresh.state, fresh.identity, evidence(), profile(), options()));
    CHECK(fresh.motion.requestVelocity(fresh.run(120), L::StopKind::HALT, fresh.now, fresh.now + DEADLINE));
    fresh.finish();
    CHECK(fresh.motion.normalResult().state == M::ActionState::SUCCEEDED);
}

void configurationAndRecoveryInvalidate() {
    scenario = "a configuration change and a controller recovery invalidate a bound record";
    {
        RecordRig r;
        r.motion.attachRecords(r.records);
        loadAndUse(r, velocityRecord(), E::RecordSlot::VELOCITY);
        // An idle host configuration change advances the configuration generation.
        r.reference.binding = r.axis.binding;
        r.reference.referenceGeneration = r.axis.referenceGeneration;
        r.reference.idle = r.reference.stationary = true;
        r.reference.source = M::ScaleSource::QUALIFIED;
        r.reference.observedUs = r.reference.nowUs = r.now;
        const M::Generation configuration = r.axis.binding.configurationGeneration;
        CHECK(r.motion.configureCommandScale(M::UnitScale(3200, 1, M::ScaleSource::QUALIFIED), r.now));
        CHECK(r.axis.binding.configurationGeneration == configuration + 1 && r.wire.sentCount == 0);
        E::RecordView view;
        CHECK(r.motion.showRecord(E::RecordSlot::VELOCITY, view) && view.state == E::RecordState::INVALIDATED);
        // Requests of that slot are refused, never run on other evidence, until it is used again.
        r.armVelocity();
        CHECK(r.motion.setVelocityPrerequisites(r.state, r.identity, evidence(), profile(), options()));
        const unsigned sent = r.wire.sentCount;
        const M::Status s = r.motion.requestVelocity(r.run(60), L::StopKind::HALT, r.now, r.now + DEADLINE);
        CHECK(!s && s.detail == int32_t(M::ActionError::STATE) && r.wire.sentCount == sent);
        CHECK(r.motion.useRecord(E::RecordSlot::VELOCITY, r.now));
        CHECK(r.motion.showRecord(E::RecordSlot::VELOCITY, view) && view.state == E::RecordState::BOUND &&
              M::sameBinding(view.binding.binding, r.axis.binding));
        CHECK(r.motion.requestVelocity(r.run(60), L::StopKind::HALT, r.now, r.now + DEADLINE));
        r.finish();
        CHECK(r.motion.normalResult().state == M::ActionState::SUCCEEDED);
    }
    {
        RecordRig r;
        r.motion.attachRecords(r.records);
        loadAndUse(r, velocityRecord(), E::RecordSlot::VELOCITY);
        const uint32_t epoch = r.owner.controllerEpoch();
        O::RecoveryId recovery;
        CHECK(r.owner.recover(r.now, r.now + 1000, recovery) && r.owner.controllerEpoch() == epoch + 1);
        E::RecordView view;
        CHECK(r.motion.showRecord(E::RecordSlot::VELOCITY, view) && view.state == E::RecordState::INVALIDATED &&
              view.binding.controllerEpoch == epoch);
    }
}

void restartForgets() {
    scenario = "records live in RAM only: a restart starts empty and nothing is restored";
    E::RecordView view;
    {
        RecordRig r;
        r.motion.attachRecords(r.records);
        loadAndUse(r, velocityRecord(), E::RecordSlot::VELOCITY);
        CHECK(r.motion.showRecord(E::RecordSlot::VELOCITY, view));
    }
    RecordRig restarted; // New storage, as after an MCU restart: no record, no binding, no traffic.
    restarted.motion.attachRecords(restarted.records);
    for (unsigned s = 0; s < E::RECORD_SLOTS; ++s)
        CHECK(!restarted.motion.showRecord(E::RecordSlot(s), view));
    restarted.armVelocity();
    const unsigned sent = restarted.wire.sentCount;
    CHECK(!restarted.motion.requestVelocity(restarted.run(60), L::StopKind::HALT, restarted.now,
                                            restarted.now + DEADLINE));
    CHECK(restarted.wire.sentCount == sent);
}

void boundEnableAndMoveRecordsNarrow() {
    scenario = "bound enable and position records refuse before any TX";
    RecordRig r;
    r.motion.attachRecords(r.records);
    r.drive.mode = 1;
    r.acquireIdentityAndState();
    L::ActionOptions options;
    options.service = reads();
    options.observationIntervalUs = 1000;
    options.maximumAgeUs = DEADLINE;
    options.commandIntervalUs = L::ENABLE_WORD_INTERVAL_US;
    // The record admits 0437/0010 only; the drive reports 0001/0010: refused by the record.
    loadAndUse(r, enableRecord(0x0437), E::RecordSlot::ENABLE);
    CHECK(r.motion.setActionPrerequisites(r.state, r.identity, nullptr, options));
    unsigned sent = r.wire.sentCount;
    M::Status s = r.motion.requestEnable(r.now, r.now + DEADLINE);
    CHECK(!s && s.detail == int32_t(L::RecordCheckError::START) && r.wire.sentCount == sent);
    // A record that admits the drive's report passes its own check, and the library still refuses a
    // report outside its measured starts: the record does not widen the library's enable rule.
    CHECK(r.motion.dropRecord(E::RecordSlot::ENABLE));
    loadAndUse(r, enableRecord(0x0001), E::RecordSlot::ENABLE);
    s = r.motion.requestEnable(r.now, r.now + DEADLINE);
    CHECK(!s && s.detail != int32_t(L::RecordCheckError::START) && r.wire.sentCount == sent);
    CHECK(std::strcmp(s.msg, "the start report is not one the record admits") != 0);
    CHECK(!r.motion.retained() && r.drive.status == 1);
    // Position: a bound absolute record needs a known witness for the travel, checked before the session's
    // own gates; the relative slot is empty, so a relative request meets those gates as before.
    loadAndUse(r, absoluteRecord(), E::RecordSlot::POSITION_ABSOLUTE);
    L::MoveOptions moveOptions;
    moveOptions.service = reads();
    moveOptions.observationIntervalUs = 1000;
    moveOptions.maximumAgeUs = DEADLINE;
    L::MoveProfile moveProfile;
    moveProfile.rampUpTimeMs = moveProfile.rampDownTimeMs = 100;
    CHECK(r.motion.setMovePrerequisites(r.state, r.identity, L::MoveEvidence(), moveProfile, nullptr, moveOptions));
    M::MoveRequest move;
    move.position.value = M::Rational(1000);
    move.position.configurationGeneration = r.axis.binding.configurationGeneration;
    move.speed.value = M::Rational(60);
    move.speed.unit = M::VelocityUnit(M::PositionUnit::TURNS, M::TimeUnit::MINUTE);
    move.speed.configurationGeneration = move.position.configurationGeneration;
    s = r.motion.requestMove(move, r.now, r.now + DEADLINE);
    CHECK(!s && s.detail == int32_t(L::RecordCheckError::DISTANCE_UNKNOWN) && r.wire.sentCount == sent);
    move.position.relative = true;
    s = r.motion.requestMove(move, r.now, r.now + DEADLINE);
    // The session's own gate (no acknowledged 000F) answers, as it does without any record.
    CHECK(!s && std::strcmp(s.msg, "no latest acknowledged controlword") == 0);
    CHECK(r.wire.sentCount == sent && !r.motion.retained());
}
// The rig's identity under an attended experiment, as velocity_test's experiment(): every
// qualification false, the experiment's rate cap.
L::VelocityEvidence attended() {
    L::VelocityEvidence e = evidence();
    e.gate = L::VelocityGate::ATTENDED_EXPERIMENT;
    e.record = 901;
    e.startQualified = e.stopQualified = e.modeChangeQualified = e.reverseQualified = false;
    e.maximumForwardRpm = L::VELOCITY_EXPERIMENT_MAXIMUM_RPM;
    e.maximumReverseRpm = 0;
    return e;
}
// Prompt 58: an attended trial the caller named for an operation takes precedence over a bound record of
// that slot, in either order; the bound record governs the regular MEASURED path.
void attendedTrialTakesPrecedence() {
    scenario = "an explicit attended velocity experiment takes precedence over a bound record";
    for (unsigned order = 0; order < 2; ++order) {
        RecordRig r;
        r.motion.attachRecords(r.records);
        r.acquireIdentityAndState();
        if (order == 1)
            CHECK(r.motion.setVelocityPrerequisites(r.state, r.identity, attended(), profile(), options()));
        // The record admits 6..20 rpm: under it a 30 rpm run would be refused (SPEED).
        loadAndUse(r, velocityRecord(20), E::RecordSlot::VELOCITY);
        if (order == 0)
            CHECK(r.motion.setVelocityPrerequisites(r.state, r.identity, attended(), profile(), options()));
        r.startFrame = r.wire.sentCount;
        CHECK(r.motion.requestVelocity(r.run(30), L::StopKind::HALT, r.now, r.now + DEADLINE));
        const L::VelocityContext* native = r.motion.nativeVelocity();
        CHECK(native && native->evidence.gate == L::VelocityGate::ATTENDED_EXPERIMENT && native->evidence.record == 901);
        CHECK(r.motion.normalResult().attendedExperiment && !r.motion.normalResult().record);
        r.finish();
        CHECK(r.writes(0x60ff, 30) == 1 && r.motion.normalResult().attendedExperiment);
        E::RecordView view;
        CHECK(r.motion.showRecord(E::RecordSlot::VELOCITY, view) && view.state == E::RecordState::BOUND);
    }
    {
        // Explicit MEASURED evidence is the regular path: the bound record replaces it and narrows the run.
        RecordRig r;
        r.motion.attachRecords(r.records);
        r.acquireIdentityAndState();
        CHECK(r.motion.setVelocityPrerequisites(r.state, r.identity, evidence(), profile(), options()));
        loadAndUse(r, velocityRecord(20), E::RecordSlot::VELOCITY);
        const unsigned sent = r.wire.sentCount;
        const M::Status s = r.motion.requestVelocity(r.run(30), L::StopKind::HALT, r.now, r.now + DEADLINE);
        CHECK(!s && s.detail == int32_t(L::RecordCheckError::SPEED) && r.wire.sentCount == sent);
        CHECK(r.motion.requestVelocity(r.run(20), L::StopKind::HALT, r.now, r.now + DEADLINE));
        uint32_t id = 0;
        CHECK(L::recordId(id, velocityRecord(20)));
        CHECK(r.motion.nativeVelocity()->evidence.gate == L::VelocityGate::MEASURED &&
              r.motion.nativeVelocity()->evidence.record == id && r.motion.normalResult().record == id &&
              !r.motion.normalResult().attendedExperiment);
        r.finish();
    }
    scenario = "an explicit attended enable trial takes precedence over a bound record";
    for (unsigned order = 0; order < 2; ++order) {
        RecordRig r;
        r.motion.attachRecords(r.records);
        r.drive.mode = 1;
        r.acquireIdentityAndState();
        L::ActionOptions options;
        options.service = reads();
        options.observationIntervalUs = 1000;
        options.maximumAgeUs = DEADLINE;
        options.commandIntervalUs = L::ENABLE_WORD_INTERVAL_US;
        // An attended trial without its trial identity: the attended path's own refusal shows which gate
        // judged it. Under the record (it admits 0437/0010 only) the refusal would be START.
        L::EnableEvidence trial;
        trial.gate = L::EnableGate::ATTENDED_EXPERIMENT;
        trial.modeRaw = 1;
        if (order == 1)
            CHECK(r.motion.setEnablePrerequisites(r.state, r.identity, trial, options));
        loadAndUse(r, enableRecord(0x0437), E::RecordSlot::ENABLE);
        if (order == 0)
            CHECK(r.motion.setEnablePrerequisites(r.state, r.identity, trial, options));
        const unsigned sent = r.wire.sentCount;
        M::Status s = r.motion.requestEnable(r.now, r.now + DEADLINE);
        CHECK(!s && s.detail == int32_t(M::ActionError::ARGUMENT) &&
              std::strcmp(s.msg, "invalid enable trial, start, reservation or budgets") == 0);
        // The regular MEASURED enable on the same report meets the bound record.
        CHECK(r.motion.setActionPrerequisites(r.state, r.identity, nullptr, options));
        L::EnableEvidence measured;
        measured.gate = L::EnableGate::MEASURED;
        CHECK(r.motion.setEnablePrerequisites(r.state, r.identity, measured, options));
        s = r.motion.requestEnable(r.now, r.now + DEADLINE);
        CHECK(!s && s.detail == int32_t(L::RecordCheckError::START));
        CHECK(r.wire.sentCount == sent && !r.motion.retained());
    }
}
// A boot-up the owner observes advances the node generation: a bound record is INVALIDATED at once, before
// any application follows the boot-up into its live target, and the next request of its slot is refused
// with nothing sent and nothing retained, never run on the evidence the setter named before the boot-up.
// (A reopen on the new generation needs the route's post-boot drain, which this fake does not model; the
// record's STATE refusal on a current axis is shown after a configuration change above.)
void nodeRebootInvalidates() {
    scenario = "a node boot-up invalidates a bound record without fallback";
    RecordRig r;
    r.motion.attachRecords(r.records);
    r.acquireIdentityAndState();
    CHECK(r.motion.setVelocityPrerequisites(r.state, r.identity, evidence(), profile(), options()));
    loadAndUse(r, velocityRecord(), E::RecordSlot::VELOCITY);
    M::Generation before = 0, after = 0;
    uint32_t sessions = 0;
    CHECK(r.owner.nodeEpochs(r.axis.binding, before, sessions) && before == r.live.nodeGeneration);
    M::Frame boot = frame(0x701, 1);
    boot.data[0] = 0; // CANopen boot-up.
    r.wire.push(boot, r.now, r.owner.controllerEpoch());
    r.tick();
    CHECK(r.owner.nodeEpochs(r.axis.binding, after, sessions) && after != before);
    CHECK(r.live.nodeGeneration == before); // The application has not followed it.
    E::RecordView view;
    CHECK(r.motion.showRecord(E::RecordSlot::VELOCITY, view) && view.state == E::RecordState::INVALIDATED &&
          view.binding.nodeGeneration == before);
    const unsigned sent = r.wire.sentCount;
    const M::Status s = r.motion.requestVelocity(r.run(60), L::StopKind::HALT, r.now, r.now + DEADLINE);
    CHECK(!s && r.wire.sentCount == sent && !r.motion.retained() && !r.session.busy());
    // useRecord cannot rebind it to the old generation either.
    CHECK(!r.motion.useRecord(E::RecordSlot::VELOCITY, r.now));
    CHECK(r.motion.showRecord(E::RecordSlot::VELOCITY, view) && view.state == E::RecordState::INVALIDATED);
}
} // namespace

int main() {
    loadValidatesTheWholeRecord();
    useBindsAndSendsNothing();
    boundRecordLimitsAndOpensVelocity();
    configurationAndRecoveryInvalidate();
    restartForgets();
    boundEnableAndMoveRecordsNarrow();
    attendedTrialTakesPrecedence();
    nodeRebootInvalidates();
    std::printf("probe_records_test: %u checks passed\n", checks);
    return 0;
}
