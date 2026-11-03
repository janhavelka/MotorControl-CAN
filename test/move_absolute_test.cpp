// SPDX-License-Identifier: MIT
// prepareMoveAbsolute through the real owner, the selected SDO engine and AxisSession.
// Every word, status, monitor, mode, identity, target and feedback value here is manufactured by
// this fake, and so are the acceptance and completion masks: 6041 bit 12 and bit 15 are documentary
// candidates whose behavior on any specimen is unmeasured, and MoveEvidence is opened here only to
// exercise the lifecycle. Nothing here shows that a drive accepts, moves or completes a target.
// Every scenario runs twice, the second time with heartbeat traffic delivered on every turn.
#include "AxisSession.h"
#include <MotorControlCAN/profiles/lichuan_c/State.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace M = MotorControlCAN;
namespace C = MotorControlCAN::CANOPEN;
namespace L = MotorControlCAN::LICHUAN_C;
namespace E = MotorControlCANExample;
namespace O = MotorControlCANOwner;
static unsigned checks = 0;
static bool asyncTraffic = false;
static const char* scenario = "";
#define CHECK(condition)                                                                           \
    do {                                                                                           \
        ++checks;                                                                                  \
        if (!(condition)) {                                                                        \
            std::fprintf(stderr, "%s:%d: %s [%s, async %d]\n", __FILE__, __LINE__, #condition,     \
                         scenario, int(asyncTraffic));                                             \
            std::abort();                                                                          \
        }                                                                                          \
    } while (false)
#include "support/OwnerRig.h"
namespace {
using OwnerRig::budget;
using OwnerRig::frame;
// Same object, its own snapshot: padding differs between compilers, so never compare two objects.
template <class T> struct Saved {
    unsigned char bytes[sizeof(T)];
    explicit Saved(const T& value) { std::memcpy(bytes, static_cast<const void*>(&value), sizeof value); }
    void unchanged(const T& value) const {
        CHECK(!std::memcmp(bytes, static_cast<const void*>(&value), sizeof value));
    }
};
const uint64_t TICK = 500;
const uint32_t QUEUE = 2000, RESPONSE = 2000;
const uint64_t LEAD = 400000;
const uint16_t IDLE_STATUS = 0x8037; // Manufactured: the specimens' power-up word, bit 15 set.
const uint16_t ACCEPT_BIT = 0x1000, COMPLETE_BIT = 0x8000; // This test's choice, not a dialect.
const int32_t TARGET = 25000;
uint8_t bit(L::StateBlock block) { return uint8_t(1u << unsigned(block)); }
const uint8_t STATE_MASK = uint8_t(bit(L::StateBlock::MONITORS) | bit(L::StateBlock::STATUS));
const uint8_t ENABLE_MASK = uint8_t(bit(L::StateBlock::ERRORS) | bit(L::StateBlock::MONITORS) |
                                    bit(L::StateBlock::STATUS) | bit(L::StateBlock::FEEDBACK));
L::ReadTarget target() {
    L::ReadTarget value;
    value.channel = OwnerRig::route(1);
    value.nodeGeneration = 11;
    return value;
}
L::ReadOptions reads() {
    L::ReadOptions value;
    value.queueTimeoutUs = QUEUE;
    value.responseTimeoutUs = RESPONSE;
    return value;
}
L::MoveOptions options() {
    L::MoveOptions value;
    value.service = reads();
    value.observationIntervalUs = 1000;
    value.maximumAgeUs = uint64_t(QUEUE + uint64_t(RESPONSE)) * 9;
    return value;
}
M::AxisConfig axisConfig() {
    M::AxisConfig c;
    c.binding = OwnerRig::route(1).binding;
    c.units.commandStepsPerMotorTurn = M::UnitScale(3200, 1, M::ScaleSource::QUALIFIED);
    c.units.motorTurnsPerLoadTurn = M::UnitScale(5, 1, M::ScaleSource::QUALIFIED);
    c.nativeRateBasis = M::NativeRateBasis::MOTOR_TURNS;
    c.nativeVelocityTime = M::TimeUnit::MINUTE;
    c.supportedRelativeBases = 1;
    return c;
}
M::MoveRequest moveRequest(int64_t targetSteps = TARGET, int64_t rpm = 120) {
    M::MoveRequest value;
    value.position.value = M::Rational(targetSteps);
    value.position.configurationGeneration = OwnerRig::route(1).binding.configurationGeneration;
    value.speed.value = M::Rational(rpm);
    value.speed.unit = M::VelocityUnit(M::PositionUnit::TURNS, M::TimeUnit::MINUTE);
    value.speed.configurationGeneration = value.position.configurationGeneration;
    return value;
}
L::MoveProfile profile(uint32_t up = 100, uint32_t down = 200) {
    L::MoveProfile value;
    value.rampUpTimeMs = up;
    value.rampDownTimeMs = down;
    return value;
}
// The fake identity. Raw codes only; nothing here is a real model or firmware.
const uint32_t DEVICE_TYPE = 0x00040192, VENDOR = 0x00000300, PRODUCT = 0x00008605, REVISION = 0x00010001;
const uint16_t HARDWARE = 0x0102, SOFTWARE = 0x0203, MODEL = 0x0086, FIRMWARE = 0x0123;
L::MoveEvidence evidence(L::CompletionRule rule = L::CompletionRule::AFTER_ABSENCE) {
    L::MoveEvidence value;
    value.gate = L::MoveGate::MEASURED;
    value.record = 19;
    value.model = L::SettingsModel::CL86_C;
    value.firmware.deviceType = DEVICE_TYPE;
    value.firmware.hardwareVersion = HARDWARE;
    value.firmware.softwareVersion = SOFTWARE;
    value.firmware.vendorId = VENDOR;
    value.firmware.productCode = PRODUCT;
    value.firmware.revision = REVISION;
    value.firmware.modelCode = MODEL;
    value.firmware.firmwareCode = FIRMWARE;
    value.acceptance.source = L::MoveSignalSource::STATUSWORD;
    value.acceptance.mask = value.acceptance.value = ACCEPT_BIT;
    value.completion.source = L::MoveSignalSource::STATUSWORD;
    value.completion.mask = value.completion.value = COMPLETE_BIT;
    value.rule = rule;
    return value;
}
// A fake server with a crude motion model. Only its wire shape means anything.
struct FakeDrive {
    uint16_t status = IDLE_STATUS, monitor = 1;
    uint8_t mode = 1;
    int32_t target = 5000, position = 0, velocity = 0;
    uint32_t speed = 60, rampUp = 100, rampDown = 100;
    bool answer = true, acceptOnTrigger = true, clearCompletion = true, followEnable = false;
    unsigned travelReads = 1; // 6041 reads reporting motion after a trigger, before arrival.
    unsigned remaining = 0, statusReads = 0, words = 0, alarmAtRead = 0;
    bool moving = false;
    uint16_t lastWord = 0xffff;
    uint16_t silentIndex = 0, abortIndex = 0, silentWord = 0, abortWord = 0;
    void arrive() {
        moving = false;
        status = uint16_t(status | COMPLETE_BIT);
        monitor = uint16_t((monitor | L::MONITOR_POSITION_COMPLETE) & ~L::MONITOR_RUNNING);
        position = target;
    }
    void word(uint16_t w) {
        if (w == 0x001f && lastWord == 0x000f) {
            if (acceptOnTrigger)
                status = uint16_t(status | ACCEPT_BIT);
            if (clearCompletion)
                status = uint16_t(status & ~COMPLETE_BIT);
            moving = true;
            remaining = travelReads;
            monitor = uint16_t((monitor & ~L::MONITOR_POSITION_COMPLETE) | L::MONITOR_RUNNING);
        } else if (w == 0x000f && lastWord == 0x001f) {
            status = uint16_t(status & ~ACCEPT_BIT);
        } else if (w == 0x010f || w == 0x000b) {
            moving = false;
            monitor = uint16_t(monitor & ~L::MONITOR_RUNNING);
        }
        if (followEnable && (w == 6 || w == 7 || w == 15)) {
            const uint16_t low = w == 6 ? 1 : w == 7 ? 3 : 7;
            status = uint16_t((status & ~L::STATUS_LOW_BITS) | low);
        }
        lastWord = w;
        ++words;
    }
};
uint16_t frameIndex(const M::Frame& f) { return uint16_t(f.data[1] | (uint16_t(f.data[2]) << 8)); }
uint32_t frameValue(const M::Frame& f) {
    return uint32_t(f.data[4]) | (uint32_t(f.data[5]) << 8) | (uint32_t(f.data[6]) << 16) |
           (uint32_t(f.data[7]) << 24);
}
bool replyTo(const M::Frame& sent, FakeDrive& drive, M::Frame& out) {
    out = frame(sent.id - 0x600 + 0x580, 8);
    const uint16_t index = frameIndex(sent);
    const uint8_t sub = sent.data[3];
    out.data[1] = sent.data[1];
    out.data[2] = sent.data[2];
    out.data[3] = sent.data[3];
    const bool download = sent.data[0] != 0x40;
    const bool isWord = index == 0x6040 && download;
    const uint16_t word = isWord ? uint16_t(frameValue(sent)) : 0;
    if (index == drive.silentIndex && index && (!isWord || !drive.silentWord || word == drive.silentWord))
        return false;
    if (index == drive.abortIndex && index && (!isWord || !drive.abortWord || word == drive.abortWord)) {
        out.data[0] = 0x80;
        out.data[4] = 0x23;
        out.data[7] = 0x08;
        return true;
    }
    if (download) {
        const uint32_t value = frameValue(sent);
        if (index == 0x607a)
            drive.target = int32_t(value);
        else if (index == 0x6081)
            drive.speed = value;
        else if (index == 0x6083)
            drive.rampUp = value;
        else if (index == 0x6084)
            drive.rampDown = value;
        else if (isWord)
            drive.word(word);
        out.data[0] = 0x60;
        return true;
    }
    uint32_t raw = 0;
    unsigned size = 2;
    switch (index) {
    case 0x1000: raw = DEVICE_TYPE; size = 4; break;
    case 0x1009: raw = HARDWARE; break;
    case 0x100a: raw = SOFTWARE; break;
    case 0x1018:
        size = sub ? 4 : 1;
        raw = sub == 0 ? 3 : sub == 1 ? VENDOR : sub == 2 ? PRODUCT : REVISION;
        break;
    case 0x2000: raw = MODEL; break;
    case 0x2001: raw = FIRMWARE; break;
    case 0x1001: raw = 0; size = 1; break;
    case 0x603f: raw = 0; break;
    case 0x2100: raw = drive.monitor; break;
    case 0x2101: raw = 0xa580; break;
    case 0x2102: raw = 0xf008; break;
    case 0x6041:
        ++drive.statusReads;
        if (drive.alarmAtRead && drive.statusReads == drive.alarmAtRead)
            drive.monitor = uint16_t(drive.monitor | L::MONITOR_ALARM);
        if (drive.moving) {
            if (!drive.remaining)
                drive.arrive();
            else
                --drive.remaining;
        }
        raw = drive.status;
        break;
    case 0x6061: raw = drive.mode; size = 1; break;
    case 0x6064: raw = uint32_t(drive.position); size = 4; break;
    case 0x606c: raw = uint32_t(drive.velocity); size = 4; break;
    case 0x607a: raw = uint32_t(drive.target); size = 4; break;
    case 0x60ff: raw = 120; size = 4; break;
    case 0x6081: raw = drive.speed; size = 4; break;
    case 0x6083: raw = drive.rampUp; size = 4; break;
    case 0x6084: raw = drive.rampDown; size = 4; break;
    default: break;
    }
    out.data[0] = size == 1 ? 0x4f : size == 2 ? 0x4b : 0x43;
    for (unsigned i = 0; i < 4; ++i)
        out.data[4 + i] = uint8_t(raw >> (8 * i));
    return true;
}
struct Wire : OwnerRig::WireBase {
    O::RecoveryProgress progress = O::RecoveryProgress::RETIRED;
    static bool beginRecovery(void*, uint32_t) { return true; }
    static O::RecoveryProgress pollRecovery(void* context, uint32_t) {
        return static_cast<Wire*>(static_cast<OwnerRig::WireBase*>(context))->progress;
    }
    O::Transport callbacks() {
        O::Transport value = plumbing();
        value.beginRecovery = beginRecovery;
        value.pollRecovery = pollRecovery;
        return value;
    }
};
struct Rig {
    C::SdoClient services;
    Wire wire;
    O::CanBusOwner owner;
    E::AxisSession a;
    FakeDrive drive;
    uint64_t now = 0;
    unsigned served = 0, startFrame = 0;
    uint64_t sentAt[OwnerRig::WireBase::CAPACITY] = {};
    unsigned stamped = 0;
    bool serviceSession = true;
    L::ReadContext state;
    L::IdentityObservation identity;
    Rig() : owner(services, wire.callbacks(), 73), a(owner) {
        const L::ReadTarget t = target();
        CHECK(services.configure(0, t.channel, true, true));
        C::NetworkRoute network;
        network.binding = t.channel.binding;
        network.generation = 11;
        network.heartbeatId = 0x701;
        network.emcyId = 0x81;
        CHECK(services.configureNetwork(0, network, C::NetworkObserver(), 0));
    }
    L::ReadTarget current() const {
        L::ReadTarget value = target();
        value.nodeGeneration = services.node(0)->route.generation;
        return value;
    }
    void later(const M::Frame& value) { wire.push(value, now + 1, owner.controllerEpoch()); }
    void turn(uint64_t time) {
        now = time;
        if (wire.rxRead == wire.rxWrite)
            wire.rxRead = wire.rxWrite = 0;
        for (; served < wire.sentCount; ++served) {
            M::Frame reply;
            if (wire.sent[served].id == 0x601 && drive.answer && replyTo(wire.sent[served], drive, reply))
                wire.push(reply, now, owner.controllerEpoch());
        }
        if (asyncTraffic) {
            M::Frame heartbeat = frame(0x701, 1);
            heartbeat.data[0] = 5;
            wire.push(heartbeat, now, owner.controllerEpoch());
        }
        CHECK(owner.service(now, budget(8, 2, 8, 2, 1)));
        for (; stamped < wire.sentCount; ++stamped)
            sentAt[stamped] = now;
        if (serviceSession)
            a.service(now);
    }
    void step() { turn(now + TICK); }
    void steps(unsigned count) {
        for (unsigned i = 0; i < count; ++i)
            step();
    }
    void finishMove(unsigned limit = 2000) {
        for (unsigned i = 0; i < limit && (a.moveActive() || a.busy()); ++i)
            step();
        CHECK(!a.moveActive());
    }
    void open() { CHECK(a.open(current(), L::SettingsModel::CL86_C, 1, now)); }
    C::SdoCompletion settled() {
        C::SdoCompletion output;
        CHECK(a.settle(output));
        return output;
    }
    void readState(L::ReadContext& into, uint8_t mask = STATE_MASK) {
        const uint64_t deadline = now + LEAD;
        const uint32_t operation = a.beginOperation(deadline, now);
        CHECK(operation);
        CHECK(L::prepareState(into, current(), operation, now, deadline, reads(), mask));
        while (into.state == M::ReadState::ACTIVE) {
            L::PreparedRead work;
            CHECK(L::nextRead(into, now, work) && work.ready);
            CHECK(a.admitPrepared(work.transfer, now) == O::BusAdmission::ACCEPTED);
            steps(2);
            const C::SdoCompletion terminal = settled();
            CHECK(L::advanceRead(into, terminal, now));
        }
        CHECK(into.state == M::ReadState::SUCCEEDED);
    }
    void readIdentity() {
        static L::ReadContext read;
        const uint64_t deadline = now + LEAD;
        const uint32_t operation = a.beginOperation(deadline, now);
        CHECK(operation);
        CHECK(L::prepareIdentity(read, current(), operation, now, deadline, reads()));
        while (read.state == M::ReadState::ACTIVE) {
            L::PreparedRead work;
            CHECK(L::nextRead(read, now, work) && work.ready);
            CHECK(a.admitPrepared(work.transfer, now) == O::BusAdmission::ACCEPTED);
            steps(2);
            const C::SdoCompletion terminal = settled();
            CHECK(L::advanceRead(read, terminal, now));
        }
        CHECK(read.state == M::ReadState::SUCCEEDED);
        CHECK(L::getIdentity(read, current(), now, LEAD, identity));
    }
    void readProfile(L::DriverContext& into) {
        const uint64_t deadline = now + LEAD;
        const uint32_t operation = a.beginOperation(deadline, now);
        CHECK(operation);
        CHECK(L::prepareDriverRead(into, current(), operation, now, deadline, reads(),
                                   L::DriverGroup::MOTION_PARAMETERS));
        while (into.read.state == M::ReadState::ACTIVE) {
            L::PreparedRead work;
            CHECK(L::nextDriver(into, now, work) && work.ready);
            CHECK(a.admitPrepared(work.transfer, now) == O::BusAdmission::ACCEPTED);
            steps(2);
            const C::SdoCompletion terminal = settled();
            CHECK(L::advanceDriver(into, terminal, now));
        }
        CHECK(into.read.state == M::ReadState::SUCCEEDED);
    }
    // An explicit caller-issued write through the session's one path.
    void writeThrough(uint16_t index, uint8_t size, uint32_t value) {
        const uint64_t deadline = now + LEAD;
        const uint32_t operation = a.beginOperation(deadline, now);
        CHECK(operation);
        C::SdoTransfer t;
        t.channel = target().channel;
        t.channel.binding = current().channel.binding;
        t.operation = t.request = operation;
        t.step = 1;
        t.index = index;
        t.size = size;
        t.direction = C::SdoDirection::DOWNLOAD;
        for (unsigned i = 0; i < size; ++i)
            t.data[i] = uint8_t(value >> (8 * i));
        t.deadlineUs = deadline;
        t.queueTimeoutUs = QUEUE;
        t.responseTimeoutUs = RESPONSE;
        CHECK(a.admitPrepared(t, now) == O::BusAdmission::ACCEPTED);
        steps(2);
        settled();
    }
    // The explicit enable is stood in for by an acknowledged 000F here; enableChain() runs the real one.
    void ready() {
        open();
        readIdentity();
        writeThrough(0x6040, 2, 0x000f);
        readState(state);
    }
    L::AxisReservation reservation() const {
        L::AxisReservation value;
        const O::SequenceId id = a.sequence();
        const O::Sequence* live = owner.sequence(id);
        CHECK(live);
        value.target = current();
        value.model = L::SettingsModel::CL86_C;
        value.owner = owner.identity();
        value.sequence = uint32_t(id.slot) + 1;
        value.generation = id.generation;
        value.acquiredUs = live->acquiredUs;
        return value;
    }
    M::Status request(const L::MoveEvidence& e = evidence(), const M::MoveRequest& r = moveRequest(),
                      const L::MoveProfile& p = profile(), const M::AxisReference* reference = nullptr,
                      const M::AxisConfig& axis = axisConfig(), uint64_t lead = LEAD,
                      const L::MoveOptions& o = options(), const L::DriverContext* stored = nullptr) {
        startFrame = wire.sentCount;
        return a.requestMove(state, identity, e, r, p, stored, axis, reference, o, now + lead, now);
    }
};
bool frameIs(const Rig& r, unsigned i, const uint8_t (&expected)[8]) {
    const M::Frame& f = r.wire.sent[i];
    return i < r.wire.sentCount && f.id == 0x601 && f.dlc == 8 && f.payloadLength == 8 &&
           !f.extended && !f.rtr && !std::memcmp(f.data, expected, sizeof expected);
}
bool downloadAt(const Rig& r, unsigned i, uint16_t index, uint8_t size, uint32_t value) {
    const uint8_t command = size == 4 ? 0x23 : size == 2 ? 0x2b : 0x2f;
    uint8_t expected[8] = {command, uint8_t(index), uint8_t(index >> 8), 0, 0, 0, 0, 0};
    for (unsigned b = 0; b < size; ++b)
        expected[4 + b] = uint8_t(value >> (8 * b));
    return frameIs(r, i, expected);
}
bool wordAt(const Rig& r, unsigned i, uint16_t word) { return downloadAt(r, i, 0x6040, 2, word); }
bool uploadAt(const Rig& r, unsigned i, uint16_t index, uint8_t sub = 0) {
    const uint8_t expected[8] = {0x40, uint8_t(index), uint8_t(index >> 8), sub, 0, 0, 0, 0};
    return frameIs(r, i, expected);
}
unsigned wordsFrom(const Rig& r, unsigned from, uint16_t word) {
    unsigned count = 0;
    for (unsigned i = from; i < r.wire.sentCount; ++i)
        count += wordAt(r, i, word);
    return count;
}
unsigned downloadsFrom(const Rig& r, unsigned from, uint16_t index) {
    unsigned count = 0;
    for (unsigned i = from; i < r.wire.sentCount; ++i)
        count += r.wire.sent[i].data[0] != 0x40 && frameIndex(r.wire.sent[i]) == index;
    return count;
}
unsigned uploadsFrom(const Rig& r, unsigned from, uint16_t index) {
    unsigned count = 0;
    for (unsigned i = from; i < r.wire.sentCount; ++i)
        count += uploadAt(r, i, index);
    return count;
}
void quiet(Rig& r, unsigned ticks = 40) {
    const unsigned sent = r.wire.sentCount;
    r.steps(ticks);
    CHECK(r.wire.sentCount == sent);
}
bool cycleAt(const Rig& r, unsigned i) {
    const uint16_t order[] = {0x2100, 0x2101, 0x2102, 0x6041, 0x6061, 0x6064, 0x606c};
    for (unsigned k = 0; k < 7; ++k)
        if (!uploadAt(r, i + k, order[k]))
            return false;
    return true;
}
const L::MoveContext& moveOf(const Rig& r) { return r.a.moveAction(); }
// Refused at the session: no frame, no move, nothing busy.
void refused(Rig& r, const M::Status& s, M::Err code, M::MoveError detail) {
    CHECK(!s);
    CHECK(s.code == code);
    CHECK(s.detail == int32_t(detail));
    CHECK(!r.a.moveActive() && !r.a.busy());
    quiet(r, 4);
}

// 1. The whole sequence, literal bytes in literal order, and its retained history.
void golden() {
    scenario = "golden";
    Rig r;
    r.ready();
    const C::SdoCompletion predecessor = *r.a.latestControl();
    CHECK(r.request().isOk());
    CHECK(r.wire.sentCount == r.startFrame); // Preparation sends nothing.
    CHECK(r.a.moveActive() && r.a.busy() && !r.a.close());
    r.finishMove();
    const L::MoveContext& c = moveOf(r);
    CHECK(c.state == M::ActionState::SUCCEEDED && c.outcome == M::ActionOutcome::OBSERVED);
    CHECK(c.completion == M::ActionCompletion::OBSERVED);
    CHECK(c.setup == M::ActionExecution::ACKNOWLEDGED && c.execution == M::ActionExecution::ACKNOWLEDGED);
    CHECK(c.rearm == M::ActionExecution::ACKNOWLEDGED && c.interruption == M::MoveInterruption::NONE);
    CHECK(!c.uncertain && c.effectsPossible && c.status.isOk());
    CHECK(c.displacement == M::MoveDisplacement::UNKNOWN && !c.displacementNative);
    unsigned i = r.startFrame;
    CHECK(downloadAt(r, i++, 0x607a, 4, uint32_t(TARGET)));
    const uint8_t target[8] = {0x23, 0x7a, 0x60, 0x00, 0xa8, 0x61, 0x00, 0x00};
    CHECK(frameIs(r, i - 1, target));
    CHECK(downloadAt(r, i++, 0x6081, 4, 120));
    CHECK(downloadAt(r, i++, 0x6083, 4, 100));
    CHECK(downloadAt(r, i++, 0x6084, 4, 200));
    const uint8_t trigger[8] = {0x2b, 0x40, 0x60, 0x00, 0x1f, 0x00, 0x00, 0x00};
    CHECK(frameIs(r, i++, trigger));
    CHECK(cycleAt(r, i)); // Cycle 1: accepted, completion absent.
    i += 7;
    CHECK(cycleAt(r, i)); // Cycle 2: completion reported.
    i += 7;
    const uint8_t rearm[8] = {0x2b, 0x40, 0x60, 0x00, 0x0f, 0x00, 0x00, 0x00};
    CHECK(frameIs(r, i++, rearm));
    CHECK(i == r.wire.sentCount);
    quiet(r);
    // Bit 4 was held from the trigger until the move was terminal: no 6040 between them.
    CHECK(wordsFrom(r, r.startFrame, 0x001f) == 1 && wordsFrom(r, r.startFrame, 0x000f) == 1);
    CHECK(!downloadsFrom(r, r.startFrame, 0x6060) && !downloadsFrom(r, r.startFrame, 0x6061));
    // History, each terminal under the move's one operation.
    CHECK(c.setupMask == 0xf && c.setupEvidenceMask == 0xf && c.triggerPresent && c.rearmPresent);
    for (unsigned k = 0; k < 4; ++k)
        CHECK(c.setupHistory[k].outcome == C::SdoOutcome::SUCCESS &&
              c.setupHistory[k].transfer.operation == c.operation);
    CHECK(c.trigger.transfer.data[0] == 0x1f && c.trigger.transfer.operation == c.operation);
    CHECK(c.rearmEvidence.transfer.data[0] == 0x0f && c.rearmEvidence.transfer.operation == c.operation);
    CHECK(c.setupHistory[3].completedUs <= c.trigger.admittedUs);
    CHECK(c.control.completedUs == predecessor.completedUs && c.control.transfer.data[0] == 0x0f);
    CHECK(c.observations == 2 && c.acceptance.present && c.acceptance.cycle == 1);
    CHECK(c.absence.present && c.absence.cycle == 1 && c.completed.present && c.completed.cycle == 2);
    CHECK(c.acceptance.earliestUs >= c.trigger.completedUs);
    CHECK(c.completed.statusword == (IDLE_STATUS | ACCEPT_BIT) && c.completed.position == TARGET);
    CHECK(c.admission.present && !c.admission.cycle && c.admission.statusword == IDLE_STATUS);
    CHECK(c.parameters.targetNative == TARGET && c.parameters.settings.positionSpeedRpm == 120);
    CHECK(c.speed.rateBasis == M::NativeRateBasis::MOTOR_TURNS && c.speed.rateTime == M::TimeUnit::MINUTE);
    CHECK(c.evidence.record == 19 && c.parameters.target.binding.configurationGeneration == 7);
    // The rearm acknowledgement is the latest word, and the last complete cycle stays readable.
    CHECK(r.a.latestControl() && r.a.latestControl()->transfer.data[0] == 0x0f);
    CHECK(r.a.moveObservation().state == M::ReadState::SUCCEEDED && !r.a.moveFenced());
    CHECK(r.a.moveObservation().startedUs == c.observationStartedUs);
    CHECK(r.a.close());
}

// 2. The chain with the real gated enable: its acknowledged 000F is the move's predecessor.
void enableChain() {
    scenario = "enable chain";
    Rig r;
    r.drive.followEnable = true;
    r.open();
    r.readIdentity();
    L::ReadContext enableState;
    r.readState(enableState, ENABLE_MASK);
    L::EnableEvidence e;
    e.gate = L::EnableGate::ATTENDED_EXPERIMENT;
    e.trial = 1;
    e.start = L::EnableStart::NO_MOTION_WRITE_RECORDED;
    e.powerUpShaft = L::ShaftObservation::FREE;
    e.reviewed = true;
    e.reviewedStatusword = IDLE_STATUS;
    e.reviewedPositionTarget = 5000;
    e.reviewedVelocityTarget = 120;
    e.reservation = r.reservation();
    e.operation = r.a.nextOperation();
    e.expiresUs = r.now + 1000000;
    L::ActionOptions budgets;
    budgets.service = reads();
    budgets.observationIntervalUs = 1000;
    budgets.maximumAgeUs = uint64_t(QUEUE + uint64_t(RESPONSE)) * 9;
    budgets.commandIntervalUs = L::ENABLE_WORD_INTERVAL_US;
    CHECK(r.a.requestEnable(enableState, e, budgets, r.now + LEAD, r.now).isOk());
    for (unsigned k = 0; k < 600 && r.a.enableActive(); ++k)
        r.step();
    CHECK(r.a.enableAction().state == M::ActionState::SUCCEEDED);
    CHECK(r.a.latestControl() && r.a.latestControl()->transfer.data[0] == 0x0f);
    r.readState(r.state);
    CHECK(r.request().isOk());
    r.finishMove();
    CHECK(moveOf(r).state == M::ActionState::SUCCEEDED);
    CHECK(moveOf(r).control.transfer.operation == r.a.enableAction().operation);
    CHECK(r.a.motionHistoryWritten()); // A later enable is refused by the session's own record.
}

// 3. Native endpoints, requested and quantized bounds, overflow and fractional error.
void targetsAndQuantization() {
    scenario = "targets";
    {
        Rig r;
        r.ready();
        CHECK(r.request(evidence(), moveRequest(-1000000)).isOk());
        r.finishMove();
        CHECK(moveOf(r).state == M::ActionState::SUCCEEDED);
        const uint8_t negative[8] = {0x23, 0x7a, 0x60, 0x00, 0xc0, 0xbd, 0xf0, 0xff};
        CHECK(frameIs(r, r.startFrame, negative));
    }
    {
        Rig r;
        r.ready();
        CHECK(r.request(evidence(), moveRequest(1000000)).isOk());
        r.finishMove();
        const uint8_t positive[8] = {0x23, 0x7a, 0x60, 0x00, 0x40, 0x42, 0x0f, 0x00};
        CHECK(frameIs(r, r.startFrame, positive));
    }
    Rig r;
    r.ready();
    const int64_t outside[] = {1000001, -1000001, INT64_MAX, INT64_MIN};
    for (int64_t value : outside) {
        const M::Status s = r.request(evidence(), moveRequest(value));
        CHECK(!s && s.detail >= int32_t(M::MoveError::PARAMETER_ERROR_BASE));
        refused(r, s, s.code, M::MoveError(s.detail));
    }
    // 1000000.5 rounds back to 1000000 under ties-to-even, but the request itself is outside.
    M::MoveRequest half = moveRequest();
    half.position.value = M::Rational(2000001, 2);
    half.position.rounding = M::Rounding::NEAREST;
    half.position.maximumQuantizationError = M::Rational(1, 2);
    const M::Status s = r.request(evidence(), half);
    CHECK(s.detail == int32_t(M::MoveError::PARAMETER_ERROR_BASE) + int32_t(L::PositionParameterError::RANGE));
    refused(r, s, M::Err::ILLEGAL_VALUE, M::MoveError(s.detail));
    // 999999.5 is inside and rounds to the even 1000000: the error is kept, quantized once.
    half.position.value = M::Rational(1999999, 2);
    CHECK(r.request(evidence(), half).isOk());
    const L::MoveContext& c = moveOf(r);
    CHECK(c.parameters.targetNative == 1000000);
    CHECK(c.parameters.target.quantization.errorNumerator == 1 &&
          c.parameters.target.quantization.errorDenominator == 2 &&
          !c.parameters.target.quantization.errorNegative);
    r.finishMove();
    CHECK(c.state == M::ActionState::SUCCEEDED);
    // A motor-frame third of a turn at 3200 steps per turn: 1066 2/3, nearest 1067, error +1/3.
    Rig m;
    m.ready();
    M::AxisConfig axis = axisConfig();
    axis.originKnown = true;
    axis.originSource = M::ScaleSource::QUALIFIED;
    M::MoveRequest third = moveRequest();
    third.position.value = M::Rational(1, 3);
    third.position.unit = M::PositionUnit::TURNS;
    third.position.frame = M::CoordinateFrame::MOTOR;
    third.position.rounding = M::Rounding::NEAREST;
    third.position.maximumQuantizationError = M::Rational(1, 2);
    CHECK(m.request(evidence(), third, profile(), nullptr, axis).isOk());
    CHECK(moveOf(m).parameters.targetNative == 1067);
    CHECK(moveOf(m).parameters.target.quantization.errorNumerator == 1 &&
          moveOf(m).parameters.target.quantization.errorDenominator == 3);
    m.finishMove();
    CHECK(downloadAt(m, m.startFrame, 0x607a, 4, 1067));
}

M::AxisReference witness(const Rig& r, int64_t position) {
    M::AxisReference w;
    w.binding = axisConfig().binding;
    w.referenceGeneration = axisConfig().referenceGeneration;
    w.idle = w.stationary = w.nativeKnown = true;
    w.nativePosition = position;
    w.basis = M::RelativeBasis::ACTUAL;
    w.source = M::ScaleSource::QUALIFIED;
    w.observedUs = w.nowUs = r.now;
    w.maximumAgeUs = 1000000;
    return w;
}
// 4. Optional witness, zero distance (R-08) and the reference rules.
void referenceAndZero() {
    scenario = "reference";
    Rig r;
    r.ready();
    // An absolute zero with no witness is an ordinary target: displacement stays unknown.
    CHECK(r.request(evidence(), moveRequest(0)).isOk());
    CHECK(moveOf(r).displacement == M::MoveDisplacement::UNKNOWN);
    r.finishMove();
    CHECK(moveOf(r).state == M::ActionState::SUCCEEDED && downloadAt(r, r.startFrame, 0x607a, 4, 0));
    Rig z;
    z.ready();
    M::AxisReference w = witness(z, 0);
    refused(z, z.request(evidence(), moveRequest(0), profile(), &w), M::Err::ILLEGAL_VALUE,
            M::MoveError::ZERO_DISPLACEMENT);
    w = witness(z, TARGET);
    refused(z, z.request(evidence(), moveRequest(TARGET), profile(), &w), M::Err::ILLEGAL_VALUE,
            M::MoveError::ZERO_DISPLACEMENT);
    w = witness(z, 100);
    w.stationary = false;
    refused(z, z.request(evidence(), moveRequest(), profile(), &w), M::Err::INVALID_CONFIG,
            M::MoveError::REFERENCE);
    w = witness(z, 100);
    w.idle = false;
    refused(z, z.request(evidence(), moveRequest(), profile(), &w), M::Err::INVALID_CONFIG,
            M::MoveError::REFERENCE);
    w = witness(z, 100);
    w.basis = M::RelativeBasis::COMMANDED;
    refused(z, z.request(evidence(), moveRequest(), profile(), &w), M::Err::INVALID_CONFIG,
            M::MoveError::REFERENCE);
    // A stale or foreign witness: the axis rules keep their own detail beneath the base.
    w = witness(z, 100);
    w.observedUs = 0;
    w.maximumAgeUs = 1;
    M::Status s = z.request(evidence(), moveRequest(), profile(), &w);
    CHECK(s.detail == int32_t(M::MoveError::PARAMETER_ERROR_BASE) +
                          int32_t(L::PositionParameterError::AXIS_ERROR_BASE) +
                          int32_t(M::AxisError::STALE_REFERENCE));
    refused(z, s, s.code, M::MoveError(s.detail));
    w = witness(z, 100);
    ++w.referenceGeneration;
    s = z.request(evidence(), moveRequest(), profile(), &w);
    CHECK(!s && s.detail > int32_t(M::MoveError::PARAMETER_ERROR_BASE));
    refused(z, s, s.code, M::MoveError(s.detail));
    // A fresh idle stationary witness fixes the displacement.
    w = witness(z, 100);
    CHECK(z.request(evidence(), moveRequest(), profile(), &w).isOk());
    CHECK(moveOf(z).displacement == M::MoveDisplacement::KNOWN && moveOf(z).displacementNative == TARGET - 100);
    CHECK(moveOf(z).parameters.target.referenceUsed);
    z.finishMove();
    CHECK(moveOf(z).state == M::ActionState::SUCCEEDED);
}

// 5. The rate basis: 6081 is rpm, so only integer motor turns per minute may reach it.
void speedBasis() {
    scenario = "speed";
    Rig r;
    r.ready();
    // 1 load turn/min at 3200 steps per turn and gear 5, under the defaults COMMAND_INCREMENTS per
    // SECOND, is 267: inside 0..3000 and wrong by a large factor. Refused before any write.
    M::AxisConfig increments = axisConfig();
    increments.nativeRateBasis = M::NativeRateBasis::COMMAND_INCREMENTS;
    increments.nativeVelocityTime = M::TimeUnit::SECOND;
    M::MoveRequest load = moveRequest();
    load.speed.value = M::Rational(1);
    load.speed.frame = M::CoordinateFrame::LOAD;
    load.speed.rounding = M::Rounding::NEAREST;
    load.speed.maximumQuantizationError = M::Rational(1, 2);
    M::PreparedVelocityTarget pinned;
    CHECK(M::prepareVelocityTarget(load.speed, increments, pinned) && pinned.effectiveNative == 267);
    refused(r, r.request(evidence(), load, profile(), nullptr, increments), M::Err::INVALID_CONFIG,
            M::MoveError::RATE_BASIS);
    M::AxisConfig perSecond = axisConfig();
    perSecond.nativeVelocityTime = M::TimeUnit::SECOND;
    M::MoveRequest native = moveRequest();
    native.speed.unit = M::VelocityUnit(M::PositionUnit::TURNS, M::TimeUnit::SECOND);
    refused(r, r.request(evidence(), native, profile(), nullptr, perSecond), M::Err::INVALID_CONFIG,
            M::MoveError::RATE_BASIS);
    M::AxisConfig floating = axisConfig();
    floating.velocityRepresentation = M::NativeRepresentation::BINARY32;
    refused(r, r.request(evidence(), moveRequest(), profile(), nullptr, floating),
            M::Err::INVALID_CONFIG, M::MoveError::RATE_BASIS);
    // Range: zero, negative, above 3000, and a request above 3000 that rounds back inside.
    const int64_t rpm[] = {0, -1, 3001};
    for (int64_t value : rpm)
        refused(r, r.request(evidence(), moveRequest(TARGET, value)), M::Err::ILLEGAL_VALUE,
                M::MoveError::RANGE);
    M::MoveRequest above = moveRequest();
    above.speed.value = M::Rational(6001, 2);
    above.speed.rounding = M::Rounding::NEAREST;
    above.speed.maximumQuantizationError = M::Rational(1, 2);
    refused(r, r.request(evidence(), above), M::Err::ILLEGAL_VALUE, M::MoveError::RANGE);
    M::MoveRequest tiny = moveRequest();
    tiny.speed.value = M::Rational(1, 3);
    tiny.speed.rounding = M::Rounding::NEAREST;
    tiny.speed.maximumQuantizationError = M::Rational(1, 2);
    refused(r, r.request(evidence(), tiny), M::Err::ILLEGAL_VALUE, M::MoveError::RANGE);
    // The same load turn per minute under MOTOR_TURNS per MINUTE is 5 rpm.
    load.speed.rounding = M::Rounding::EXACT;
    CHECK(r.request(evidence(), load).isOk());
    CHECK(moveOf(r).speed.effectiveNative == 5 && moveOf(r).parameters.settings.positionSpeedRpm == 5);
    r.finishMove();
    CHECK(downloadAt(r, r.startFrame + 1, 0x6081, 4, 5));
    // 5/2 rpm rounds to 2 under ties-to-even, quantized once, error -1/2 rpm; 3000 itself is admitted.
    Rig h;
    h.ready();
    M::MoveRequest fraction = moveRequest();
    fraction.speed.value = M::Rational(5, 2);
    fraction.speed.rounding = M::Rounding::NEAREST;
    fraction.speed.maximumQuantizationError = M::Rational(1, 2);
    CHECK(h.request(evidence(), fraction).isOk());
    CHECK(moveOf(h).speed.effectiveNative == 2 && moveOf(h).speed.quantization.errorNegative &&
          moveOf(h).speed.quantization.errorNumerator == 1 && moveOf(h).speed.quantization.errorDenominator == 2);
    h.finishMove();
    Rig top;
    top.ready();
    CHECK(top.request(evidence(), moveRequest(TARGET, 3000)).isOk());
    top.finishMove();
    CHECK(downloadAt(top, top.startFrame + 1, 0x6081, 4, 3000));
}

// 6. Capability gate, firmware binding, budgets and the unsupported forms.
void gates() {
    scenario = "gates";
    Rig r;
    r.ready();
    refused(r, r.request(L::MoveEvidence()), M::Err::UNSUPPORTED, M::MoveError::CAPABILITY);
    L::MoveEvidence e = evidence();
    e.firmware.firmwareCode = uint16_t(FIRMWARE + 1);
    refused(r, r.request(e), M::Err::UNSUPPORTED, M::MoveError::CAPABILITY);
    e = evidence();
    e.firmware.softwareVersion = 0;
    refused(r, r.request(e), M::Err::UNSUPPORTED, M::MoveError::CAPABILITY);
    e = evidence();
    e.model = L::SettingsModel::OL86_C;
    refused(r, r.request(e), M::Err::UNSUPPORTED, M::MoveError::CAPABILITY);
    L::MoveProfile immediate = profile();
    immediate.immediateUpdate = true;
    refused(r, r.request(evidence(), moveRequest(), immediate), M::Err::UNSUPPORTED, M::MoveError::CAPABILITY);
    e = evidence();
    e.record = 0;
    refused(r, r.request(e), M::Err::INVALID_CONFIG, M::MoveError::ARGUMENT);
    e = evidence();
    e.rule = L::CompletionRule::NONE;
    refused(r, r.request(e), M::Err::INVALID_CONFIG, M::MoveError::ARGUMENT);
    e = evidence();
    e.completion.mask = 0;
    refused(r, r.request(e), M::Err::INVALID_CONFIG, M::MoveError::ARGUMENT);
    e = evidence();
    e.acceptance.value = 0x2000; // Outside its mask.
    refused(r, r.request(e), M::Err::INVALID_CONFIG, M::MoveError::ARGUMENT);
    e = evidence();
    e.acceptance.source = L::MoveSignalSource::NONE;
    refused(r, r.request(e), M::Err::INVALID_CONFIG, M::MoveError::ARGUMENT);
    r.readState(r.state); // Each refusal above let time pass; keep the admission read fresh.
    // Ramps: zero is a documentary value with no evidenced meaning, so a move refuses it.
    refused(r, r.request(evidence(), moveRequest(), profile(0, 200)), M::Err::ILLEGAL_VALUE, M::MoveError::RANGE);
    refused(r, r.request(evidence(), moveRequest(), profile(100, 0)), M::Err::ILLEGAL_VALUE, M::MoveError::RANGE);
    refused(r, r.request(evidence(), moveRequest(), profile(2001, 200)), M::Err::ILLEGAL_VALUE, M::MoveError::RANGE);
    // Relative and wrapped intents are not this call's.
    M::MoveRequest relative = moveRequest();
    relative.position.relative = true;
    refused(r, r.request(evidence(), relative), M::Err::INVALID_CONFIG, M::MoveError::ARGUMENT);
    M::MoveRequest wrapped = moveRequest();
    wrapped.position.wrapped = true;
    refused(r, r.request(evidence(), wrapped), M::Err::INVALID_CONFIG, M::MoveError::ARGUMENT);
    // Budgets: a deadline that cannot cover the schedule, an age cap below one cycle, other widths.
    const uint64_t schedule = L::MOVE_SERVICE_STEPS * (uint64_t(QUEUE) + RESPONSE);
    refused(r, r.request(evidence(), moveRequest(), profile(), nullptr, axisConfig(), schedule),
            M::Err::INVALID_CONFIG, M::MoveError::TIME);
    L::MoveOptions o = options();
    o.maximumAgeUs = 7 * (uint64_t(QUEUE) + RESPONSE) - 1;
    refused(r, r.request(evidence(), moveRequest(), profile(), nullptr, axisConfig(), LEAD, o),
            M::Err::INVALID_CONFIG, M::MoveError::ARGUMENT);
    o = options();
    o.service.identityWidths = L::IdentityWidths::MEASURED_U8;
    refused(r, r.request(evidence(), moveRequest(), profile(), nullptr, axisConfig(), LEAD, o),
            M::Err::INVALID_CONFIG, M::MoveError::ARGUMENT);
    o = options();
    o.observationIntervalUs = 0;
    refused(r, r.request(evidence(), moveRequest(), profile(), nullptr, axisConfig(), LEAD, o),
            M::Err::INVALID_CONFIG, M::MoveError::ARGUMENT);
    r.readState(r.state);
    // An axis of another binding, and a request prepared under an older configuration epoch.
    M::AxisConfig other = axisConfig();
    ++other.binding.logicalAxisId;
    refused(r, r.request(evidence(), moveRequest(), profile(), nullptr, other), M::Err::INVALID_CONFIG,
            M::MoveError::TARGET);
    M::MoveRequest old = moveRequest();
    --old.position.configurationGeneration;
    M::Status s = r.request(evidence(), old);
    CHECK(s.detail == int32_t(M::MoveError::PARAMETER_ERROR_BASE) +
                          int32_t(L::PositionParameterError::AXIS_ERROR_BASE) +
                          int32_t(M::AxisError::STALE_GENERATION));
    refused(r, s, s.code, M::MoveError(s.detail));
    old = moveRequest();
    --old.speed.configurationGeneration;
    s = r.request(evidence(), old);
    CHECK(s.detail == int32_t(M::MoveError::RATE_ERROR_BASE) + int32_t(M::AxisError::STALE_GENERATION));
    refused(r, s, s.code, M::MoveError(s.detail));
    r.readState(r.state);
    // Everything refused, then the same rig still moves: nothing above changed anything.
    CHECK(r.request().isOk());
    r.finishMove();
    CHECK(moveOf(r).state == M::ActionState::SUCCEEDED);
}

// 7. Drive-state and history prerequisites, each before any write.
void stateGates() {
    scenario = "state";
    struct Case {
        uint16_t status, monitor;
        uint8_t mode;
        M::MoveError detail;
    };
    const Case cases[] = {
        {IDLE_STATUS, 1, 3, M::MoveError::STATE},           // Mode 3 on an enabled drive (D-02).
        {IDLE_STATUS, 1, 0, M::MoveError::STATE},           // Mode 0, the specimens' power-up mode.
        {0x8033, 1, 1, M::MoveError::STATE},                // Low bits 011.
        {IDLE_STATUS, 0x11, 1, M::MoveError::STATE},        // Released.
        {IDLE_STATUS, 0x09, 1, M::MoveError::STATE},        // Alarm.
        {IDLE_STATUS, 0x05, 1, M::MoveError::STATE},        // Running.
        {IDLE_STATUS, 0x21, 1, M::MoveError::STATE},        // Positive soft limit.
        {IDLE_STATUS, 0x81, 1, M::MoveError::STATE},        // Undocumented bit 7.
        {IDLE_STATUS | ACCEPT_BIT, 1, 1, M::MoveError::ACCEPTANCE}, // Acceptance already reported.
    };
    for (const Case& k : cases) {
        Rig r;
        r.drive.status = k.status;
        r.drive.monitor = k.monitor;
        r.drive.mode = k.mode;
        r.ready();
        refused(r, r.request(), M::Err::INVALID_CONFIG, k.detail);
    }
    {   // A state read taken before the 000F does not follow it.
        Rig r;
        r.open();
        r.readIdentity();
        r.readState(r.state);
        r.writeThrough(0x6040, 2, 0x000f);
        refused(r, r.request(), M::Err::INVALID_CONFIG, M::MoveError::STATE);
    }
    {   // The latest word is a trigger: no 000F, so no move, and no 000F is sent to make one.
        Rig r;
        r.open();
        r.readIdentity();
        r.writeThrough(0x6040, 2, 0x001f);
        r.readState(r.state);
        refused(r, r.request(), M::Err::INVALID_CONFIG, M::MoveError::STATE);
        CHECK(!wordsFrom(r, 0, 0x000f));
    }
    {   // Any later download withdraws the word: the session has no latest controlword.
        Rig r;
        r.ready();
        r.writeThrough(0x6081, 4, 60);
        r.readState(r.state);
        refused(r, r.request(), M::Err::INVALID_CONFIG, M::MoveError::STATE);
    }
    {   // Busy and closed sessions refuse; the reservation must be usable.
        Rig r;
        r.ready();
        CHECK(r.request().isOk());
        const M::Status s = r.request();
        CHECK(!s && s.detail == int32_t(M::MoveError::STATE));
        r.finishMove();
        CHECK(r.a.close());
        const M::Status closed = r.request();
        CHECK(!closed && closed.detail == int32_t(M::MoveError::TARGET));
    }
}

// 8. Profile reuse and contradiction, and the packet 10 retirement hook.
void profileReuse() {
    scenario = "reuse";
    {   // Equal stored values: only 607A is written.
        Rig r;
        r.drive.speed = 120;
        r.drive.rampUp = 100;
        r.drive.rampDown = 200;
        static L::DriverContext stored;
        r.ready();
        r.readProfile(stored);
        r.readState(r.state);
        L::MoveProfile p = profile();
        p.setup = L::MoveSetup::REUSE_STORED;
        // A read the session would not retire on a move write cannot be reused (review F1).
        refused(r, r.request(evidence(), moveRequest(), p, nullptr, axisConfig(), LEAD, options(), &stored),
                M::Err::INVALID_CONFIG, M::MoveError::STATE);
        L::DriverContext* list[] = {&stored};
        CHECK(r.a.retainSettings(list, 1));
        CHECK(r.request(evidence(), moveRequest(), p, nullptr, axisConfig(), LEAD, options(), &stored).isOk());
        CHECK(moveOf(r).setupMask == 1);
        r.finishMove();
        CHECK(moveOf(r).state == M::ActionState::SUCCEEDED && moveOf(r).setup == M::ActionExecution::ACKNOWLEDGED);
        CHECK(downloadAt(r, r.startFrame, 0x607a, 4, uint32_t(TARGET)) && wordAt(r, r.startFrame + 1, 0x001f));
        CHECK(!downloadsFrom(r, r.startFrame, 0x6081) && !downloadsFrom(r, r.startFrame, 0x6083));
    }
    {   // One mismatch: only that one is written, in its place in the order.
        Rig r;
        r.drive.speed = 60;
        r.drive.rampUp = 100;
        r.drive.rampDown = 200;
        static L::DriverContext stored;
        r.ready();
        r.readProfile(stored);
        r.readState(r.state);
        L::DriverContext* list[] = {&stored};
        CHECK(r.a.retainSettings(list, 1));
        L::MoveProfile p = profile();
        p.setup = L::MoveSetup::REUSE_STORED;
        CHECK(r.request(evidence(), moveRequest(), p, nullptr, axisConfig(), LEAD, options(), &stored).isOk());
        CHECK(moveOf(r).setupMask == 3);
        r.finishMove();
        CHECK(downloadAt(r, r.startFrame + 1, 0x6081, 4, 120) && wordAt(r, r.startFrame + 2, 0x001f));
        // That 6081 write retired the read, so the same read cannot be reused for the next move.
        CHECK(stored.read.invalidated);
        r.readState(r.state);
        refused(r, r.request(evidence(), moveRequest(), p, nullptr, axisConfig(), LEAD, options(), &stored),
                M::Err::INVALID_CONFIG, M::MoveError::STATE);
    }
    {   // WRITE_ALL with the context retained: the move retires it, keeps its own target valid, and a
        // later reuse of that context is refused until a fresh read replaces it.
        Rig r;
        static L::DriverContext stored;
        r.ready();
        r.readProfile(stored);
        L::DriverContext* list[] = {&stored};
        CHECK(r.a.retainSettings(list, 1));
        r.readState(r.state);
        CHECK(r.request().isOk());
        r.finishMove();
        CHECK(moveOf(r).state == M::ActionState::SUCCEEDED);
        CHECK(stored.read.invalidated);
        L::DriverObservation observed;
        CHECK(!L::getDriver(stored, r.current(), r.now, LEAD, observed));
        CHECK(M::validatePreparedTarget(moveOf(r).parameters.target, axisConfig(), nullptr, r.now));
        CHECK(r.current().channel.binding.configurationGeneration == 7); // The epoch did not move.
        r.readState(r.state);
        L::MoveProfile p = profile();
        p.setup = L::MoveSetup::REUSE_STORED;
        refused(r, r.request(evidence(), moveRequest(), p, nullptr, axisConfig(), LEAD, options(), &stored),
                M::Err::INVALID_CONFIG, M::MoveError::STATE);
        // A later settings call needs a fresh read too.
        static L::DriverSettingsContext settings;
        L::DriverSettings values;
        values.positionSpeedRpm = 120;
        values.rampUpTimeMs = 100;
        values.rampDownTimeMs = 200;
        CHECK(!L::prepareDriverSettings(settings, r.reservation(), r.current(), stored, r.state,
                                        *r.a.latestControl(), values, r.a.nextOperation(), r.now,
                                        r.now + LEAD, reads(), options().maximumAgeUs));
        // Missing and stale stored reads are refused as well.
        refused(r, r.request(evidence(), moveRequest(), p), M::Err::INVALID_CONFIG, M::MoveError::STATE);
    }
    {   // A retained context of another axis: the hook refuses, so the download is cancelled unsent.
        Rig r;
        r.ready();
        static L::DriverContext foreign;
        L::ReadTarget elsewhere = r.current();
        elsewhere.channel = OwnerRig::route(2);
        CHECK(L::prepareDriverRead(foreign, elsewhere, 1, r.now, r.now + LEAD, reads(),
                                   L::DriverGroup::MOTION_PARAMETERS));
        L::DriverContext* list[] = {&foreign};
        CHECK(r.a.retainSettings(list, 1));
        CHECK(r.request().isOk());
        r.finishMove();
        const L::MoveContext& c = moveOf(r);
        CHECK(c.state == M::ActionState::FAILED && c.outcome == M::ActionOutcome::INVALIDATED);
        CHECK(c.setup == M::ActionExecution::NOT_TRANSMITTED && !c.effectsPossible);
        CHECK(r.wire.sentCount == r.startFrame); // 607A never left.
    }
}

// 9. New versus stale acceptance and completion, under each rule.
void staleReports() {
    scenario = "stale";
    {   // Completion never clears: under AFTER_ABSENCE the old bit never finishes the move.
        Rig r;
        r.drive.clearCompletion = false;
        r.drive.travelReads = 1000000;
        r.ready();
        CHECK(r.request(evidence(), moveRequest(), profile(), nullptr, axisConfig(), 60000).isOk());
        r.finishMove();
        const L::MoveContext& c = moveOf(r);
        CHECK(c.state == M::ActionState::FAILED && c.outcome == M::ActionOutcome::DEADLINE);
        CHECK(c.completion == M::ActionCompletion::NOT_OBSERVED && c.execution == M::ActionExecution::ACKNOWLEDGED);
        CHECK(c.acceptance.present && !c.absence.present && c.observations >= 2 && c.uncertain);
        CHECK(!wordsFrom(r, r.startFrame, 0x000f) && r.a.moveFenced()); // Bit 4 stays held.
        CHECK(r.a.latestControl() && r.a.latestControl()->transfer.data[0] == 0x1f);
    }
    {   // The same reports under AFTER_ACCEPTANCE: a later cycle after acceptance qualifies.
        Rig r;
        r.drive.clearCompletion = false;
        r.drive.travelReads = 1000000;
        r.ready();
        CHECK(r.request(evidence(L::CompletionRule::AFTER_ACCEPTANCE)).isOk());
        r.finishMove();
        const L::MoveContext& c = moveOf(r);
        CHECK(c.state == M::ActionState::SUCCEEDED && c.acceptance.cycle == 1 && c.completed.cycle == 2);
    }
    {   // No acceptance: completion alone, even after absence, finishes nothing.
        Rig r;
        r.drive.acceptOnTrigger = false;
        r.drive.travelReads = 2;
        r.ready();
        CHECK(r.request(evidence(), moveRequest(), profile(), nullptr, axisConfig(), 60000).isOk());
        r.finishMove();
        const L::MoveContext& c = moveOf(r);
        CHECK(c.outcome == M::ActionOutcome::DEADLINE && c.completion == M::ActionCompletion::NOT_OBSERVED);
        CHECK(!c.acceptance.present && c.absence.present && r.a.moveFenced());
    }
    {   // An alarm reported mid-move ends the observation scope; its raw cycle is kept.
        Rig r;
        r.drive.travelReads = 5;
        r.ready();
        r.drive.alarmAtRead = r.drive.statusReads + 1; // Raised by cycle 1, reported by cycle 2.
        CHECK(r.request().isOk());
        r.finishMove();
        const L::MoveContext& c = moveOf(r);
        CHECK(c.outcome == M::ActionOutcome::OBSERVATION_SCOPE && (c.latest.monitor & L::MONITOR_ALARM));
        CHECK(c.uncertain && !wordsFrom(r, r.startFrame, 0x000f));
    }
}

// 10. Each phase interrupted by cancel, fault, deadline, reboot and a stop.
enum class Phase { PARAMETERS, TRIGGER, OBSERVING, WAITING, REARM };
enum class Cut { CANCEL, FAULT, DEADLINE, REBOOT, STOP };
bool atPhase(const L::MoveContext& c, Phase p) {
    switch (p) {
    case Phase::PARAMETERS:
        return c.phase == L::MovePhase::PARAMETERS && c.setupEvidenceMask == 1 && c.transmissionNoted;
    case Phase::TRIGGER:
        return c.phase == L::MovePhase::TRIGGER && c.transmissionNoted;
    case Phase::OBSERVING:
        return c.phase == L::MovePhase::OBSERVING && c.observations == 0;
    case Phase::WAITING:
        return c.phase == L::MovePhase::WAITING && c.observations == 1;
    default:
        return c.phase == L::MovePhase::REARM && c.transmissionNoted;
    }
}
void interruptions() {
    const Phase phases[] = {Phase::PARAMETERS, Phase::TRIGGER, Phase::OBSERVING, Phase::WAITING, Phase::REARM};
    const Cut cuts[] = {Cut::CANCEL, Cut::FAULT, Cut::DEADLINE, Cut::REBOOT, Cut::STOP};
    for (Phase phase : phases)
        for (Cut cut : cuts) {
            scenario = "interruption";
            Rig r;
            r.drive.travelReads = 1;
            r.ready();
            L::MoveOptions o = options();
            o.observationIntervalUs = 4000; // Long enough to stand in WAITING.
            CHECK(r.request(evidence(), moveRequest(), profile(), nullptr, axisConfig(), LEAD, o).isOk());
            for (unsigned k = 0; k < 400 && r.a.moveActive() && !atPhase(moveOf(r), phase); ++k)
                r.step();
            CHECK(r.a.moveActive() && atPhase(moveOf(r), phase));
            const unsigned at = r.wire.sentCount;
            E::StopPath path = E::StopPath::NONE;
            switch (cut) {
            case Cut::CANCEL:
                CHECK(r.a.cancelMove(r.now));
                break;
            case Cut::FAULT:
                CHECK(r.a.cancelMove(r.now, M::MoveInterruption::DRIVE_FAULT));
                break;
            case Cut::DEADLINE:
                r.turn(moveOf(r).deadlineUs);
                break;
            case Cut::REBOOT:
                r.later(frame(0x701, 1));
                break;
            case Cut::STOP: {
                E::StopPolicy policy;
                policy.options.service = reads();
                policy.options.observationIntervalUs = 1000;
                policy.options.maximumAgeUs = uint64_t(QUEUE + uint64_t(RESPONSE)) * 9;
                policy.deadlineUs = r.now + 200000;
                policy.bestEffortDeadlineUs = r.now + 300000;
                path = r.a.requestStop(L::StopKind::HALT, r.a.moveObservation(), policy, r.now);
                CHECK(path != E::StopPath::NONE);
                break;
            }
            }
            r.finishMove();
            for (unsigned k = 0; k < 400 && r.a.busy(); ++k)
                r.step();
            const L::MoveContext& c = moveOf(r);
            CHECK(c.state == M::ActionState::FAILED);
            const M::ActionOutcome expected = cut == Cut::FAULT      ? M::ActionOutcome::DRIVE_FAULT
                                              : cut == Cut::DEADLINE ? M::ActionOutcome::DEADLINE
                                              : cut == Cut::REBOOT   ? M::ActionOutcome::INVALIDATED
                                                                     : M::ActionOutcome::CANCELLED;
            CHECK(c.outcome == expected);
            if (cut == Cut::STOP)
                CHECK(c.interruption == M::MoveInterruption::STOP);
            // Nothing replayed: at most the one trigger, and no rearm after the cut unless that rearm
            // was itself the download already admitted.
            const unsigned triggers = wordsFrom(r, r.startFrame, 0x001f);
            CHECK(triggers <= 1 && (phase == Phase::PARAMETERS ? !triggers : true));
            CHECK(triggers == 1 || phase == Phase::PARAMETERS || phase == Phase::TRIGGER);
            CHECK(wordsFrom(r, at, 0x000f) <= (phase == Phase::REARM ? 1u : 0u));
            CHECK(c.rearmPresent == (phase == Phase::REARM));
            CHECK(c.completion == (phase == Phase::REARM ? M::ActionCompletion::OBSERVED
                                                         : M::ActionCompletion::NOT_OBSERVED));
            const bool mayMove = c.execution == M::ActionExecution::ACKNOWLEDGED ||
                                 c.execution == M::ActionExecution::UNKNOWN;
            CHECK(mayMove == (triggers == 1));
            if (mayMove && c.completion != M::ActionCompletion::OBSERVED)
                CHECK(c.uncertain && r.a.moveFenced());
            else
                CHECK(!r.a.moveFenced());
            if (phase == Phase::PARAMETERS)
                CHECK(!c.triggerPresent && c.execution == M::ActionExecution::NOT_TRANSMITTED);
            if (cut == Cut::STOP) {
                // A stop never writes 000F first. From the held trigger with a complete cycle after it,
                // the stop is sequenced, and its predecessor is that word.
                const L::ActionContext& s = r.a.stopAction(L::StopKind::HALT);
                if (phase == Phase::WAITING) {
                    CHECK(path == E::StopPath::CONFIRMED);
                    CHECK(s.predecessorWord == 0x001f && !s.predecessorDocumented);
                    CHECK(s.predecessorBasis == L::PredecessorBasis::PROJECT_DECISION);
                    CHECK(wordsFrom(r, at, 0x010f) == 1 && !wordsFrom(r, at, 0x000b));
                    CHECK(s.state == M::ActionState::SUCCEEDED);
                } else {
                    CHECK(path == E::StopPath::BEST_EFFORT || path == E::StopPath::FINISHED ||
                          path == E::StopPath::HELD || path == E::StopPath::CONFIRMED);
                }
            }
            const unsigned sent = r.wire.sentCount;
            r.steps(20);
            CHECK(r.wire.sentCount == sent);
        }
}

// 11. A trigger whose reply is lost: unknown, fenced, and a stop after it stays unqualified.
void lostTrigger() {
    scenario = "lost trigger";
    Rig r;
    r.drive.silentIndex = 0x6040;
    r.drive.silentWord = 0x001f;
    r.ready();
    CHECK(r.request().isOk());
    r.finishMove();
    const L::MoveContext& c = moveOf(r);
    CHECK(c.state == M::ActionState::FAILED && c.outcome == M::ActionOutcome::SERVICE_TIMEOUT);
    CHECK(c.execution == M::ActionExecution::UNKNOWN && c.trigger.transmissionPossible);
    CHECK(c.uncertain && c.effectsPossible && r.a.moveFenced() && !r.a.latestControl());
    const unsigned first = r.startFrame; // request() below moves startFrame.
    CHECK(wordsFrom(r, first, 0x001f) == 1);
    // New motion stays fenced, whatever is supplied.
    const M::Status again = r.request();
    CHECK(!again && (again.detail == int32_t(M::MoveError::TARGET) || again.detail == int32_t(M::MoveError::STATE)));
    // Only the best-effort stop remains, and an identical late reply cannot qualify it.
    E::StopPolicy policy;
    policy.options.service = reads();
    policy.options.observationIntervalUs = 1000;
    policy.options.maximumAgeUs = uint64_t(QUEUE + uint64_t(RESPONSE)) * 9;
    policy.deadlineUs = r.now + 20000;
    policy.bestEffortDeadlineUs = r.now + 40000;
    static L::ReadContext none;
    const E::StopPath path = r.a.requestStop(L::StopKind::QUICK_STOP, none, policy, r.now);
    CHECK(path == E::StopPath::BEST_EFFORT || path == E::StopPath::HELD);
    for (unsigned k = 0; k < 200 && r.a.stopActive(); ++k)
        r.step();
    CHECK(r.a.bestEffort().state == M::ActionState::FAILED);
    CHECK(r.a.bestEffort().outcome == M::ActionOutcome::STOP_SENT_UNCONFIRMED);
    const Saved<L::MoveContext> move(c);
    const Saved<L::BestEffortStop> stop(r.a.bestEffort());
    M::Frame late = frame(0x581, 8);
    const uint8_t reply[8] = {0x60, 0x40, 0x60, 0x00, 0, 0, 0, 0};
    std::memcpy(late.data, reply, sizeof reply);
    r.later(late);
    r.steps(10);
    move.unchanged(c);
    stop.unchanged(r.a.bestEffort());
    CHECK(wordsFrom(r, first, 0x001f) == 1);
    CHECK(!r.request());
}

// 12. Acknowledged trigger, missing completion: the route stays resolved and new motion is fenced.
void missingCompletion() {
    scenario = "missing completion";
    Rig r;
    r.drive.travelReads = 1000000;
    r.ready();
    CHECK(r.request(evidence(), moveRequest(), profile(), nullptr, axisConfig(), 60000).isOk());
    r.finishMove();
    CHECK(moveOf(r).outcome == M::ActionOutcome::DEADLINE && moveOf(r).execution == M::ActionExecution::ACKNOWLEDGED);
    CHECK(r.a.moveFenced() && r.a.latestControl() && r.a.latestControl()->transfer.data[0] == 0x1f);
    // Even an idle report and an explicit 000F written by the caller do not lift the fence: neither
    // shows that the trajectory ended. Without the fence this admission would pass.
    r.drive.moving = false;
    r.drive.monitor = 1;
    r.writeThrough(0x6040, 2, 0x000f);
    r.readState(r.state);
    refused(r, r.request(), M::Err::INVALID_CONFIG, M::MoveError::STATE);
}

// 12b. A trigger the drive answered with an abort reached it, and an abort does not show it had no
// effect: the move is uncertain and new motion is fenced, even after an explicit 000F (review F2).
void abortedTrigger() {
    scenario = "aborted trigger";
    Rig r;
    r.ready();
    r.drive.abortIndex = 0x6040;
    r.drive.abortWord = 0x001f;
    CHECK(r.request().isOk());
    r.finishMove();
    const L::MoveContext& c = moveOf(r);
    CHECK(c.outcome == M::ActionOutcome::PROTOCOL_REJECTED && c.execution == M::ActionExecution::REJECTED);
    CHECK(c.trigger.abortValid && c.uncertain && r.a.moveFenced());
    CHECK(wordsFrom(r, r.startFrame, 0x001f) == 1 && !wordsFrom(r, r.startFrame, 0x000f));
    r.drive.abortIndex = 0;
    r.writeThrough(0x6040, 2, 0x000f);
    r.readState(r.state);
    refused(r, r.request(), M::Err::INVALID_CONFIG, M::MoveError::STATE);
}

// 13. The visible rearm: its failure keeps the observed completion and is never retried.
void rearmFailure() {
    scenario = "rearm";
    for (unsigned lost = 0; lost < 2; ++lost) {
        Rig r;
        r.ready(); // Its stand-in 000F goes before the fault is armed.
        (lost ? r.drive.silentIndex : r.drive.abortIndex) = 0x6040;
        (lost ? r.drive.silentWord : r.drive.abortWord) = 0x000f;
        CHECK(r.request().isOk());
        r.finishMove();
        const L::MoveContext& c = moveOf(r);
        CHECK(c.state == M::ActionState::FAILED && c.completion == M::ActionCompletion::OBSERVED);
        CHECK(c.execution == M::ActionExecution::ACKNOWLEDGED && c.completed.present);
        CHECK(c.outcome == (lost ? M::ActionOutcome::SERVICE_TIMEOUT : M::ActionOutcome::PROTOCOL_REJECTED));
        CHECK(c.rearm == (lost ? M::ActionExecution::UNKNOWN : M::ActionExecution::REJECTED));
        // A rejected or lost 000F may still have changed the drive: uncertain, never resent.
        CHECK(c.uncertain && !r.a.moveFenced());
        CHECK(wordsFrom(r, r.startFrame, 0x000f) == 1 && !r.a.latestControl());
        quiet(r);
        if (!lost) // A lost reply leaves the route unresolved, so no read can follow.
            r.readState(r.state);
        CHECK(!r.request());
    }
}

// 14. G-13: the cycle that would complete is processed at the cutoff, so it never completes.
void processingCutoff() {
    scenario = "cutoff";
    Rig r;
    r.ready();
    CHECK(r.request().isOk());
    const uint64_t deadline = moveOf(r).deadlineUs;
    // Run to the completing cycle's last upload, then let only the owner receive its reply.
    for (unsigned k = 0; k < 400 && uploadsFrom(r, r.startFrame, 0x606c) < 2; ++k)
        r.step();
    CHECK(uploadAt(r, r.wire.sentCount - 1, 0x606c) && moveOf(r).observations == 1);
    r.serviceSession = false;
    r.steps(3); // The reply is received and its terminal published, before the cutoff.
    CHECK(r.now < deadline);
    r.serviceSession = true;
    r.turn(deadline);
    r.finishMove();
    const L::MoveContext& c = moveOf(r);
    CHECK(c.outcome == M::ActionOutcome::DEADLINE && c.completion == M::ActionCompletion::NOT_OBSERVED);
    CHECK(c.observationEvidencePresent && c.observationEvidence.completedUs < deadline);
    CHECK(!wordsFrom(r, r.startFrame, 0x000f) && r.a.moveFenced());
}

// 15. The library calls directly: every refusal leaves its outputs byte for byte unchanged.
void atomicity() {
    scenario = "atomicity";
    Rig r;
    r.ready();
    static L::MoveContext c;
    static L::ReadContext observation;
    const L::AxisReservation live = r.reservation();
    const L::ReadTarget here = r.current();
    const uint32_t operation = r.a.nextOperation();
    CHECK(r.owner.beginOperation(r.a.sequence(), operation, r.now + LEAD, r.now));
    const M::MoveRequest request = moveRequest();
    const L::MoveProfile p = profile();
    const M::AxisConfig axis = axisConfig();
    const L::MoveOptions o = options();
    const L::MoveEvidence e = evidence();
    const C::SdoCompletion control = *r.a.latestControl();
    {
        const Saved<L::MoveContext> before(c);
        CHECK(!L::prepareMoveAbsolute(c, live, here, r.state, control, r.identity, L::MoveEvidence(), request, p,
                                      nullptr, axis, nullptr, operation, r.now, r.now + LEAD, o));
        before.unchanged(c);
        L::ReadTarget stale = here;
        ++stale.nodeGeneration;
        CHECK(!L::prepareMoveAbsolute(c, live, stale, r.state, control, r.identity, e, request, p, nullptr,
                                      axis, nullptr, operation, r.now, r.now + LEAD, o));
        before.unchanged(c);
        CHECK(L::validateMoveAbsolute(c, live, here, r.state, control, r.identity, e, request, p, nullptr,
                                      axis, nullptr, operation, r.now, r.now + LEAD, o));
        // The identity read belongs to the boot it was taken in: another node epoch is not this firmware.
        static L::IdentityObservation rebooted;
        rebooted = r.identity;
        ++rebooted.target.nodeGeneration;
        const M::Status other = L::validateMoveAbsolute(c, live, here, r.state, control, rebooted, e, request,
                                                        p, nullptr, axis, nullptr, operation, r.now,
                                                        r.now + LEAD, o);
        CHECK(other.code == M::Err::UNSUPPORTED && other.detail == int32_t(M::MoveError::CAPABILITY));
        before.unchanged(c);
    }
    CHECK(L::prepareMoveAbsolute(c, live, here, r.state, control, r.identity, e, request, p, nullptr, axis,
                                 nullptr, operation, r.now, r.now + LEAD, o));
    CHECK(c.state == M::ActionState::ACTIVE && c.phase == L::MovePhase::PARAMETERS);
    const Saved<L::MoveContext> active(c);
    L::PreparedMove work;
    const Saved<L::PreparedMove> blank(work);
    L::ReadTarget stale = here;
    ++stale.nodeGeneration;
    CHECK(L::nextMove(c, observation, live, stale, r.now, work).detail == int32_t(M::MoveError::TARGET));
    L::ReadTarget reconfigured = here;
    ++reconfigured.channel.binding.configurationGeneration; // An explicit settings call elsewhere.
    CHECK(L::nextMove(c, observation, live, reconfigured, r.now, work).detail == int32_t(M::MoveError::TARGET));
    L::AxisReservation other = live;
    ++other.generation;
    CHECK(L::nextMove(c, observation, other, here, r.now, work).detail == int32_t(M::MoveError::TARGET));
    CHECK(L::nextMove(c, observation, live, here, r.now - 1, work).detail == int32_t(M::MoveError::STATE));
    blank.unchanged(work);
    CHECK(L::beginMoveObservation(c, observation, live, here, r.now).detail == int32_t(M::MoveError::STATE));
    CHECK(L::noteMoveWithdrawal(c, live, here, M::MoveInterruption::NONE, r.now).detail ==
          int32_t(M::MoveError::STATE));
    CHECK(L::noteMoveWithdrawal(c, live, here, M::MoveInterruption::DEADLINE, r.now).detail ==
          int32_t(M::MoveError::STATE));
    CHECK(L::noteMoveTransmission(c, live, here, nullptr, 1, r.now).detail == int32_t(M::MoveError::ARGUMENT));
    CHECK(L::advanceMove(c, observation, live, here, M::MoveInterruption::DEADLINE, r.now).detail ==
          int32_t(M::MoveError::STATE));
    // A terminal for other work, and one before the notice that a frame may have left.
    CHECK(L::nextMove(c, observation, live, here, r.now, work) && work.ready);
    C::SdoCompletion wrong;
    wrong.transfer = work.transfer;
    wrong.transfer.data[0] ^= 1;
    wrong.outcome = C::SdoOutcome::SUCCESS;
    wrong.nodeGeneration = here.nodeGeneration;
    wrong.admittedUs = wrong.completedUs = r.now;
    wrong.queued = wrong.transmissionPossible = wrong.protocolAcknowledged = wrong.effectPossible = true;
    CHECK(L::advanceMove(c, observation, live, here, wrong, r.now).detail == int32_t(M::MoveError::CORRELATION));
    wrong.transfer = work.transfer;
    CHECK(L::advanceMove(c, observation, live, here, wrong, r.now).detail == int32_t(M::MoveError::CORRELATION));
    // Overlap: an output inside an input.
    CHECK(L::advanceMove(c, observation, live, here, c.control, r.now).detail == int32_t(M::MoveError::OVERLAP));
    active.unchanged(c);
    // A withdrawal is then accepted once, and the ended move refuses everything after.
    CHECK(L::advanceMove(c, observation, live, here, M::MoveInterruption::CANCELLED, r.now));
    CHECK(c.state == M::ActionState::FAILED && c.outcome == M::ActionOutcome::CANCELLED && !c.uncertain);
    const Saved<L::MoveContext> ended(c);
    CHECK(!L::advanceMove(c, observation, live, here, M::MoveInterruption::CANCELLED, r.now));
    CHECK(L::nextMove(c, observation, live, here, r.now, work) && !work.ready && !work.observe);
    ended.unchanged(c);
    L::resetMove(c);
    CHECK(c.state == M::ActionState::EMPTY);
    // A withdrawal noted while a download is in flight: its acknowledgement is kept as evidence and
    // ends the move with the withdrawal's reason at once. It never advances to the next parameter.
    const uint32_t second = operation + 1;
    CHECK(r.owner.beginOperation(r.a.sequence(), second, r.now + LEAD, r.now));
    CHECK(L::prepareMoveAbsolute(c, live, here, r.state, control, r.identity, e, request, p, nullptr, axis,
                                 nullptr, second, r.now, r.now + LEAD, o));
    CHECK(L::nextMove(c, observation, live, here, r.now, work) && work.ready && work.transfer.index == 0x607a);
    C::SdoCompletion ack = wrong;
    ack.transfer = work.transfer; // The download in flight, as the owner would retain it.
    CHECK(L::noteMoveTransmission(c, live, here, nullptr, 0, r.now));
    CHECK(c.setup == M::ActionExecution::UNKNOWN && c.uncertain && c.effectsPossible);
    // Noted, so in flight: nextMove never offers it again (review F4).
    CHECK(L::nextMove(c, observation, live, here, r.now, work) && !work.ready && !work.observe);
    CHECK(L::noteMoveWithdrawal(c, live, here, M::MoveInterruption::STOP, r.now));
    CHECK(!L::noteMoveWithdrawal(c, live, here, M::MoveInterruption::CANCELLED, r.now)); // First wins.
    CHECK(L::nextMove(c, observation, live, here, r.now, work) && !work.ready); // Nothing new.
    CHECK(L::advanceMove(c, observation, live, here, ack, r.now));
    CHECK(c.state == M::ActionState::FAILED && c.outcome == M::ActionOutcome::CANCELLED);
    CHECK(c.interruption == M::MoveInterruption::STOP && c.setupEvidenceMask == 1);
    CHECK(c.setupHistory[0].outcome == C::SdoOutcome::SUCCESS && c.setupCursor == 0);
    CHECK(c.setup == M::ActionExecution::UNKNOWN && !c.triggerPresent && !c.uncertain);
    // A partial setup: 607A acknowledged, then 6081 spent its queue budget unsent. The summary stays
    // UNKNOWN, since 607A reached the drive, and nothing is uncertain (review F3).
    L::resetMove(c);
    const uint32_t third = second + 1;
    CHECK(r.owner.beginOperation(r.a.sequence(), third, r.now + LEAD, r.now));
    CHECK(L::prepareMoveAbsolute(c, live, here, r.state, control, r.identity, e, request, p, nullptr, axis,
                                 nullptr, third, r.now, r.now + LEAD, o));
    CHECK(L::nextMove(c, observation, live, here, r.now, work) && work.ready);
    ack.transfer = work.transfer;
    CHECK(L::noteMoveTransmission(c, live, here, nullptr, 0, r.now));
    CHECK(L::advanceMove(c, observation, live, here, ack, r.now));
    CHECK(c.state == M::ActionState::ACTIVE && c.setupCursor == 1 && c.setup == M::ActionExecution::UNKNOWN);
    CHECK(L::nextMove(c, observation, live, here, r.now, work) && work.ready && work.transfer.index == 0x6081);
    C::SdoCompletion unsent;
    unsent.transfer = work.transfer;
    unsent.outcome = C::SdoOutcome::TIMEOUT;
    unsent.nodeGeneration = here.nodeGeneration;
    unsent.admittedUs = unsent.completedUs = r.now;
    CHECK(L::advanceMove(c, observation, live, here, unsent, r.now));
    CHECK(c.state == M::ActionState::FAILED && c.outcome == M::ActionOutcome::SERVICE_TIMEOUT);
    CHECK(c.setup == M::ActionExecution::UNKNOWN && c.setupEvidenceMask == 3 && c.effectsPossible);
    CHECK(c.setupHistory[1].outcome == C::SdoOutcome::TIMEOUT && !c.uncertain);
}

// The owner terminal of a successful download, by hand, for library calls run outside the session.
C::SdoCompletion acknowledged(const C::SdoTransfer& t, uint64_t at, M::Generation node) {
    C::SdoCompletion c;
    c.transfer = t;
    c.outcome = C::SdoOutcome::SUCCESS;
    c.nodeGeneration = node;
    c.admittedUs = c.completedUs = at;
    c.queued = c.transmissionPossible = c.protocolAcknowledged = c.effectPossible = true;
    return c;
}
// 16. A device counter zero, through the real prepareSetDevicePosition trigger, advances the
// configuration epoch: a move prepared before it can neither run nor be admitted again.
void counterZeroInvalidates() {
    scenario = "counter zero";
    Rig r;
    r.open();
    r.readIdentity();
    r.writeThrough(0x6040, 2, 0x000f);
    r.readState(r.state, uint8_t(STATE_MASK | bit(L::StateBlock::FEEDBACK))); // The counter needs FEEDBACK.
    static L::MoveContext move;
    static L::ReadContext observation;
    static L::ActionContext counter;
    const L::AxisReservation live = r.reservation();
    L::ReadTarget here = r.current();
    M::AxisConfig axis = axisConfig();
    M::AxisReference reference;
    const C::SdoCompletion control = *r.a.latestControl();
    const uint32_t operation = r.a.nextOperation();
    CHECK(r.owner.beginOperation(r.a.sequence(), operation, r.now + LEAD, r.now));
    const M::MoveRequest request = moveRequest();
    CHECK(L::prepareMoveAbsolute(move, live, here, r.state, control, r.identity, evidence(), request,
                                 profile(), nullptr, axis, nullptr, operation, r.now, r.now + LEAD, options()));
    L::ActionOptions budgets;
    budgets.service = reads();
    budgets.observationIntervalUs = 1000;
    budgets.maximumAgeUs = uint64_t(QUEUE + uint64_t(RESPONSE)) * 9;
    CHECK(L::prepareSetDevicePosition(counter, live, r.state, 0, axis, operation + 1, r.now,
                                      r.now + LEAD, budgets));
    // Value 0, acknowledged, then the value 1 trigger: its notice withdraws coordinates.
    for (unsigned word = 0; word < 2; ++word) {
        L::PreparedAction work;
        CHECK(L::nextAction(counter, live, here, r.now, work) && work.ready);
        CHECK(work.transfer.index == 0x2302 && work.transfer.data[0] == word);
        CHECK(L::noteActionTransmission(counter, live, here, axis, reference, r.now));
        if (!word)
            CHECK(L::advanceAction(counter, live, here, acknowledged(work.transfer, r.now, here.nodeGeneration),
                                   r.now));
    }
    CHECK(here.channel.binding.configurationGeneration == 8 && axis.binding.configurationGeneration == 8);
    // The move froze epoch 7: every call against the new target refuses, and it can only be ended.
    const Saved<L::MoveContext> before(move);
    L::PreparedMove next;
    CHECK(L::nextMove(move, observation, live, here, r.now, next).detail == int32_t(M::MoveError::TARGET));
    CHECK(L::noteMoveTransmission(move, live, here, nullptr, 0, r.now).detail == int32_t(M::MoveError::TARGET));
    CHECK(!M::validatePreparedTarget(move.parameters.target, axis, nullptr, r.now));
    before.unchanged(move);
    CHECK(L::advanceMove(move, observation, live, here, M::MoveInterruption::INVALIDATED, r.now));
    CHECK(move.state == M::ActionState::FAILED && move.outcome == M::ActionOutcome::INVALIDATED);
    CHECK(move.setup == M::ActionExecution::NOT_TRANSMITTED && !move.effectsPossible);
    // The same intent cannot be admitted again: the old reservation's target and the request's
    // configuration epoch are both stale.
    static L::MoveContext again;
    CHECK(L::validateMoveAbsolute(again, live, here, r.state, control, r.identity, evidence(), request,
                                  profile(), nullptr, axis, nullptr, operation + 2, r.now, r.now + LEAD,
                                  options())
              .detail == int32_t(M::MoveError::TARGET));
    L::AxisReservation renewed = live;
    renewed.target = here;
    const M::Status stale = L::validateMoveAbsolute(again, renewed, here, r.state, control, r.identity,
                                                    evidence(), request, profile(), nullptr, axis, nullptr,
                                                    operation + 2, r.now, r.now + LEAD, options());
    CHECK(!stale);
}

void runAll() {
    golden();
    enableChain();
    targetsAndQuantization();
    referenceAndZero();
    speedBasis();
    gates();
    stateGates();
    profileReuse();
    staleReports();
    interruptions();
    lostTrigger();
    missingCompletion();
    abortedTrigger();
    rearmFailure();
    processingCutoff();
    atomicity();
    counterZeroInvalidates();
}
} // namespace

int main() {
    for (int async = 0; async < 2; ++async) {
        asyncTraffic = async != 0;
        runAll();
    }
    std::printf("move_absolute: %u checks\n", checks);
    return 0;
}
