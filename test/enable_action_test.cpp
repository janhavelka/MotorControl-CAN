// SPDX-License-Identifier: MIT
// prepareEnable through the real owner, the selected SDO engine and AxisSession.
// Every word, status, monitor, mode, target and feedback value here is manufactured by this fake.
// No specimen has run this sequence, and nothing here shows that any word enables a motor, that a
// retained target is harmless or that a report means torque. Every scenario runs twice, the second
// time with heartbeat traffic delivered on every turn.
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
const uint64_t INTERVAL = L::ENABLE_WORD_INTERVAL_US;
const uint32_t QUEUE = 2000, RESPONSE = 2000;
const uint16_t POWER_UP_STATUS = 0x8037; // The measured raw word, used here only as bytes.
uint8_t bit(L::StateBlock block) { return uint8_t(1u << unsigned(block)); }
const uint8_t FULL_MASK = uint8_t(bit(L::StateBlock::ERRORS) | bit(L::StateBlock::MONITORS) |
                                  bit(L::StateBlock::STATUS) | bit(L::StateBlock::FEEDBACK));
L::ReadTarget target() {
    L::ReadTarget value;
    value.channel = OwnerRig::route(1);
    value.nodeGeneration = 11;
    return value;
}
L::ActionOptions options(uint32_t queue = QUEUE, uint32_t response = RESPONSE) {
    L::ActionOptions value;
    value.service.queueTimeoutUs = queue;
    value.service.responseTimeoutUs = response;
    value.observationIntervalUs = 1000;
    value.maximumAgeUs = uint64_t(queue + uint64_t(response)) * 9;
    value.commandIntervalUs = INTERVAL;
    return value;
}
// Shortest deadline lead prepareEnable admits for these budgets.
uint64_t minimumLead(const L::ActionOptions& o) {
    return 3 * o.commandIntervalUs +
           L::ENABLE_SERVICE_STEPS * (uint64_t(o.service.queueTimeoutUs) + o.service.responseTimeoutUs) + 1;
}
L::EnableEvidence evidence(uint16_t status = POWER_UP_STATUS, int32_t position = 5000, int32_t velocity = 120) {
    L::EnableEvidence value;
    value.gate = L::EnableGate::ATTENDED_EXPERIMENT;
    value.start = L::EnableStart::NO_MOTION_WRITE_RECORDED;
    value.powerUpShaft = L::ShaftObservation::FREE; // Needed: 0x8037 with 2100=1 reads as enabled.
    value.reviewed = true;
    value.reviewedStatusword = status;
    value.reviewedPositionTarget = position;
    value.reviewedVelocityTarget = velocity;
    return value;
}
// A fake server following the documented table for words 6, 7 and 15. Only its wire shape means
// anything: the table is what prepareEnable is checked against, not what a drive does.
struct FakeDrive {
    uint16_t monitor = 1, status = POWER_UP_STATUS, errorCode = 0;
    uint8_t mode = 1, errorRegister = 0;
    int32_t positionTarget = 5000, velocityTarget = 120, position = 0, velocity = 0;
    bool answer = true, followWords = true;
    unsigned lagReads = 0;      // 6041 reads to answer with the old low bits after each word.
    uint16_t silentIndex = 0, abortIndex = 0, silentWord = 0, abortWord = 0;
    uint16_t lastWord = 0xffff;
    // The bytes node 1 gave on 2026-11-03: word 6 released the motor (2100 bit 4) and cleared 6041 bit
    // 10. Words 7 and 15 here follow the hypothesis that energizing sets it again; not yet measured.
    bool measuredRelease = false;
    // The bytes of a release measured on both specimens (trial 1782, mode 3, one firmware): word 6 moved
    // 6041 bits 0..6 from 0x50 to 0x31 with 2100 still released, word 7 gave 0x33 and cleared 2100
    // released, word 15 gave 0x37. Mode 1 is this fake's assumption, not a measurement.
    bool releasedPattern = false;
    // This word clears the latch a moving quick stop left, 2100 running and 6041 bit 14, as word 7 did in
    // trials 308 (4031/0014 to 0033/0000) and 1742 (C031/0015 to 8033/0001); zero keeps it. With
    // unlatchStatus false only 2100 running clears.
    uint16_t unlatchWord = 0;
    bool unlatchStatus = true;
    unsigned words = 0, targetReads = 0;
    // Once this many enable words have arrived, the driftRead-th 6064 read after that (1 the first) adds
    // driftChange first: a position report that moves on a later cycle, as word 7's did in trial 308.
    unsigned driftWord = 0, driftRead = 0, driftReads = 0;
    int32_t driftChange = 0;
    // Applied when this many enable words have arrived; zero means never.
    unsigned changeAfterWord = 0;
    uint16_t monitorChange = 0, statusChange = 0;
    int32_t positionChange = 0, velocityChange = 0;
    int modeChange = -1;
    void apply() {
        monitor = uint16_t(monitor ^ monitorChange);
        status = uint16_t(status ^ statusChange);
        position += positionChange;
        velocity += velocityChange;
        if (modeChange >= 0)
            mode = uint8_t(modeChange);
        changeAfterWord = 0;
    }
};
uint16_t frameIndex(const M::Frame& f) { return uint16_t(f.data[1] | (uint16_t(f.data[2]) << 8)); }
uint16_t frameWord(const M::Frame& f) { return uint16_t(f.data[4] | (uint16_t(f.data[5]) << 8)); }
bool replyTo(const M::Frame& sent, FakeDrive& drive, M::Frame& out) {
    out = frame(sent.id - 0x600 + 0x580, 8);
    const uint16_t index = frameIndex(sent);
    out.data[1] = sent.data[1];
    out.data[2] = sent.data[2];
    out.data[3] = sent.data[3];
    const bool isWord = index == 0x6040 && sent.data[0] == 0x2b;
    const uint16_t word = isWord ? frameWord(sent) : 0;
    if ((index == drive.silentIndex && index && (!isWord || !drive.silentWord || word == drive.silentWord)))
        return false;
    if (index == drive.abortIndex && index && (!isWord || !drive.abortWord || word == drive.abortWord)) {
        out.data[0] = 0x80;
        out.data[4] = 0x23;
        out.data[7] = 0x08;
        return true;
    }
    if (sent.data[0] != 0x40) {
        if (isWord) {
            drive.lastWord = word;
            ++drive.words;
            if (drive.followWords) {
                const uint16_t low = word == 6 ? 1 : word == 7 ? 3 : word == 15 ? 7 : 0;
                drive.status = uint16_t((drive.status & ~L::STATUS_LOW_BITS) | low);
                if (word == 15)
                    drive.monitor = uint16_t(drive.monitor & ~L::MONITOR_RELEASED);
                if (drive.measuredRelease && word == 6) {
                    drive.status = uint16_t(drive.status & ~L::STATUS_EN86_ZERO_SPEED);
                    drive.monitor = uint16_t(drive.monitor | L::MONITOR_RELEASED);
                } else if (drive.measuredRelease && word == 15) {
                    drive.status = uint16_t(drive.status | L::STATUS_EN86_ZERO_SPEED);
                }
                if (drive.releasedPattern && (word == 6 || word == 7 || word == 15))
                    drive.status = uint16_t((drive.status & ~0x0060u) | 0x0020u);
                if (drive.releasedPattern && word == 7)
                    drive.monitor = uint16_t(drive.monitor & ~L::MONITOR_RELEASED);
                if (drive.unlatchWord && word == drive.unlatchWord) {
                    drive.monitor = uint16_t(drive.monitor & ~L::MONITOR_RUNNING);
                    if (drive.unlatchStatus)
                        drive.status = uint16_t(drive.status & ~0x4000u);
                }
            }
            if (drive.changeAfterWord && drive.words == drive.changeAfterWord)
                drive.apply();
        }
        if (index == 0x6060)
            drive.mode = sent.data[4];
        out.data[0] = 0x60;
        return true;
    }
    uint32_t raw = 0;
    unsigned size = 2;
    switch (index) {
    case 0x1001:
        raw = drive.errorRegister;
        size = 1;
        break;
    case 0x603f:
        raw = drive.errorCode;
        break;
    case 0x2100:
        raw = drive.monitor;
        break;
    case 0x2101:
        raw = 0xa580;
        break;
    case 0x2102:
        raw = 0xf008;
        break;
    case 0x6041:
        raw = drive.status;
        if (drive.lagReads) {
            --drive.lagReads;
            raw = uint16_t((raw & ~L::STATUS_LOW_BITS) | 5u);
        }
        break;
    case 0x6061:
        raw = drive.mode;
        size = 1;
        break;
    case 0x6064:
        if (drive.driftWord && drive.words >= drive.driftWord && ++drive.driftReads == drive.driftRead)
            drive.position += drive.driftChange;
        raw = uint32_t(drive.position);
        size = 4;
        break;
    case 0x606c:
        raw = uint32_t(drive.velocity);
        size = 4;
        break;
    case 0x607a:
        raw = uint32_t(drive.positionTarget);
        size = 4;
        ++drive.targetReads;
        break;
    case 0x60ff:
        raw = uint32_t(drive.velocityTarget);
        size = 4;
        ++drive.targetReads;
        break;
    default:
        break;
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
    unsigned stamped = 0, trials = 0;
    bool serviceSession = true;
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
    void finishEnable(unsigned limit = 400) {
        for (unsigned i = 0; i < limit && a.enableActive(); ++i)
            step();
        CHECK(!a.enableActive());
    }
    void open() { CHECK(a.open(current(), L::SettingsModel::CL86_C, 1, now)); }
    C::SdoCompletion settled() {
        C::SdoCompletion output;
        CHECK(a.settle(output));
        return output;
    }
    void readState(L::ReadContext& state, uint8_t mask = FULL_MASK) {
        const uint64_t deadline = now + 400000;
        const uint32_t operation = a.beginOperation(deadline, now);
        CHECK(operation);
        L::ReadOptions reads;
        reads.queueTimeoutUs = QUEUE;
        reads.responseTimeoutUs = RESPONSE;
        CHECK(L::prepareState(state, current(), operation, now, deadline, reads, mask));
        while (state.state == M::ReadState::ACTIVE) {
            L::PreparedRead work;
            CHECK(L::nextRead(state, now, work) && work.ready);
            CHECK(a.admitPrepared(work.transfer, now) == O::BusAdmission::ACCEPTED);
            steps(2);
            const C::SdoCompletion terminal = settled();
            CHECK(L::advanceRead(state, terminal, now));
        }
        CHECK(state.state == M::ReadState::SUCCEEDED);
    }
    // An explicit caller-issued write, as a non-enable command through the session's one path.
    void writeThrough(uint16_t index, uint8_t size, uint16_t value, bool settle = true) {
        const uint64_t deadline = now + 400000;
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
        t.data[0] = uint8_t(value);
        t.data[1] = uint8_t(value >> 8);
        t.deadlineUs = deadline;
        t.queueTimeoutUs = QUEUE;
        t.responseTimeoutUs = RESPONSE;
        CHECK(a.admitPrepared(t, now) == O::BusAdmission::ACCEPTED);
        if (settle) {
            steps(2);
            settled();
        }
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
    // The application's attended trial record, bound to this reservation and the next operation.
    L::EnableEvidence bound(L::EnableEvidence given) {
        if (!given.trial)
            given.trial = ++trials;
        if (owner.sequence(a.sequence())) // None after a lost route: the request then refuses.
            given.reservation = reservation();
        given.operation = a.nextOperation();
        if (!given.expiresUs)
            given.expiresUs = now + 1000000;
        return given;
    }
    M::Status request(const L::ReadContext& state, const L::EnableEvidence& given = evidence(),
                      uint64_t leadUs = 400000, const L::ActionOptions& budgets = options()) {
        startFrame = wire.sentCount;
        return a.requestEnable(state, bound(given), budgets, now + leadUs, now);
    }
};
L::AxisReservation reservationOf(const Rig& r) { return r.reservation(); }
bool wordAt(const Rig& r, unsigned i, uint16_t word) {
    const uint8_t expected[] = {0x2b, 0x40, 0x60, 0, uint8_t(word), uint8_t(word >> 8), 0, 0};
    const M::Frame& f = r.wire.sent[i];
    return i < r.wire.sentCount && f.id == 0x601 && f.dlc == 8 && f.payloadLength == 8 && !f.extended &&
           !f.rtr && !std::memcmp(f.data, expected, sizeof expected);
}
bool uploadAt(const Rig& r, unsigned i, uint16_t index, uint8_t sub = 0) {
    const uint8_t expected[] = {0x40, uint8_t(index), uint8_t(index >> 8), sub, 0, 0, 0, 0};
    const M::Frame& f = r.wire.sent[i];
    return i < r.wire.sentCount && f.id == 0x601 && f.dlc == 8 && !std::memcmp(f.data, expected, sizeof expected);
}
unsigned wordsFrom(const Rig& r, unsigned from) {
    unsigned count = 0;
    for (unsigned i = from; i < r.wire.sentCount; ++i)
        count += r.wire.sent[i].data[0] == 0x2b && frameIndex(r.wire.sent[i]) == 0x6040;
    return count;
}
unsigned downloadsFrom(const Rig& r, unsigned from) {
    unsigned count = 0;
    for (unsigned i = from; i < r.wire.sentCount; ++i)
        count += r.wire.sent[i].data[0] != 0x40;
    return count;
}
void quiet(Rig& r, unsigned ticks = 40) {
    const unsigned sent = r.wire.sentCount;
    r.steps(ticks);
    CHECK(r.wire.sentCount == sent);
}
// Run until the session has admitted this many words or the action has ended.
void runToWords(Rig& r, unsigned count, unsigned limit = 300) {
    for (unsigned i = 0; i < limit && r.a.enableActive() && wordsFrom(r, r.startFrame) < count; ++i)
        r.step();
    CHECK(wordsFrom(r, r.startFrame) == count);
}
void admittedRig(Rig& r, L::ReadContext& state) {
    r.open();
    r.readState(state);
}

// 1. The whole documented sequence, literal bytes in literal order, and its reported history.
void goldenSequence() {
    Rig r;
    L::ReadContext state;
    admittedRig(r, state);
    CHECK(r.request(state).isOk());
    CHECK(r.wire.sentCount == r.startFrame); // Preparation sends nothing.
    r.finishEnable();
    const L::ActionContext& c = r.a.enableAction();
    CHECK(c.state == M::ActionState::SUCCEEDED && c.outcome == M::ActionOutcome::OBSERVED);
    CHECK(c.completion == M::ActionCompletion::OBSERVED && c.execution == M::ActionExecution::ACKNOWLEDGED);
    CHECK(!c.uncertain && c.effectsPossible && c.actionKind == M::ActionKind::ENABLE);
    unsigned i = r.startFrame;
    CHECK(uploadAt(r, i++, 0x607a));
    CHECK(uploadAt(r, i++, 0x60ff));
    const uint16_t words[] = {6, 7, 15};
    const uint16_t low[] = {1, 3, 7};
    uint64_t lastWordAt = 0;
    for (unsigned w = 0; w < 3; ++w) {
        if (w)
            CHECK(r.sentAt[i] - lastWordAt >= INTERVAL); // Spacing honored even with a fast drive.
        lastWordAt = r.sentAt[i];
        CHECK(wordAt(r, i++, words[w]));
        CHECK(uploadAt(r, i++, 0x2100));
        CHECK(uploadAt(r, i++, 0x2101));
        CHECK(uploadAt(r, i++, 0x2102));
        CHECK(uploadAt(r, i++, 0x6041));
        CHECK(uploadAt(r, i++, 0x6061));
        CHECK(uploadAt(r, i++, 0x6064));
        CHECK(uploadAt(r, i++, 0x606c));
        CHECK(c.commandHistory[w].outcome == C::SdoOutcome::SUCCESS);
        CHECK(c.enable.steps[w].present && (c.enable.steps[w].statusword & 7u) == low[w]);
        CHECK(c.enable.steps[w].statusword == ((POWER_UP_STATUS & 0xfff8u) | low[w]));
        CHECK(c.enable.steps[w].monitor == 1);
    }
    CHECK(i == r.wire.sentCount);
    CHECK(c.commandEvidenceMask == 7 && c.commandCursor == 2);
    CHECK(c.enable.positionTargetKnown && c.enable.positionTarget == 5000);
    CHECK(c.enable.velocityTargetKnown && c.enable.velocityTarget == 120);
    CHECK(c.enable.startStatusword == POWER_UP_STATUS && c.enable.startMonitor == 1);
    // The session recorded each step, so a stop follows the acknowledged 000F.
    CHECK(r.a.latestControl() && r.a.latestControl()->transfer.data[0] == 0x0f &&
          r.a.latestControl()->transfer.data[1] == 0);
    // Not one mode, target or fault write, and no later frame.
    CHECK(downloadsFrom(r, r.startFrame) == 3 && r.drive.targetReads == 2 && r.drive.mode == 1);
    quiet(r);
}

// 1b. The shortest admitted deadline lets a prompt drive finish, and one microsecond less refuses.
void minimumDeadline() {
    const uint64_t intervals[] = {INTERVAL, 50000};
    for (uint64_t interval : intervals) {
        Rig r;
        L::ReadContext state;
        admittedRig(r, state);
        L::ActionOptions budgets = options();
        budgets.commandIntervalUs = interval;
        const uint64_t lead = minimumLead(budgets);
        const M::Status shortStatus = r.request(state, evidence(), lead - 1, budgets);
        CHECK(shortStatus.detail == int32_t(M::ActionError::ARGUMENT));
        CHECK(r.request(state, evidence(), lead, budgets).isOk());
        r.finishEnable();
        CHECK(r.a.enableAction().state == M::ActionState::SUCCEEDED);
        for (unsigned w = 1; w < 3; ++w)
            CHECK(r.a.enableAction().commandHistory[w].admittedUs >=
                  r.a.enableAction().commandHistory[w - 1].completedUs + interval);
    }
}

// 2. Everything that refuses, atomically, with no traffic and no change to the context.
void refusalsAreAtomic() {
    Rig r;
    L::ReadContext state;
    admittedRig(r, state);
    const L::AxisReservation good = reservationOf(r);
    const uint32_t operation = 900;
    const uint64_t now = r.now, deadline = now + 400000;
    L::ActionContext output;
    Saved<L::ActionContext> blank(output);
    const unsigned sent = r.wire.sentCount;
    const auto refuses = [&](const L::AxisReservation& reservation, const L::ReadContext& read,
                             const L::EnableEvidence& given, uint32_t op, uint64_t time,
                             uint64_t cutoff, const L::ActionOptions& budgets, M::Err code,
                             M::ActionError detail) {
        Saved<L::ReadContext> savedRead(read);
        const M::Status checked = L::validateEnable(output, reservation, read, given, op, time, cutoff, budgets);
        const M::Status actual = L::prepareEnable(output, reservation, read, given, op, time, cutoff, budgets);
        CHECK(actual.code == code && actual.detail == int32_t(detail));
        CHECK(checked.code == code && checked.detail == int32_t(detail));
        blank.unchanged(output);
        savedRead.unchanged(read);
    };
    const M::Err bad = M::Err::INVALID_CONFIG;
    const L::ActionOptions budgets = options();
    L::EnableEvidence fine = evidence();
    fine.trial = 1;
    fine.reservation = good;
    fine.operation = operation;
    fine.expiresUs = now + 1000000;
    CHECK(L::validateEnable(output, good, state, fine, operation, now, deadline, budgets).isOk());
    // The gate is first: no budget or state matters while it is closed.
    L::EnableEvidence given;
    refuses(good, state, given, operation, now, deadline, budgets, M::Err::UNSUPPORTED, M::ActionError::STATE);
    given = fine;
    given.gate = L::EnableGate::CLOSED;
    refuses(good, state, given, operation, now, deadline, budgets, M::Err::UNSUPPORTED, M::ActionError::STATE);
    given = fine;
    given.gate = static_cast<L::EnableGate>(2);
    refuses(good, state, given, operation, now, deadline, budgets, bad, M::ActionError::ARGUMENT);
    given = fine;
    given.start = L::EnableStart::UNKNOWN;
    refuses(good, state, given, operation, now, deadline, budgets, bad, M::ActionError::STATE);
    given = fine;
    given.start = static_cast<L::EnableStart>(3); // The first value past RELEASED_BY_RECORDED_STOP.
    refuses(good, state, given, operation, now, deadline, budgets, bad, M::ActionError::ARGUMENT);
    given = fine; // A released start against this enabled-looking power-up report.
    given.start = L::EnableStart::RELEASED_BY_RECORDED_STOP;
    refuses(good, state, given, operation, now, deadline, budgets, bad, M::ActionError::STATE);
    given = fine;
    given.reviewed = false;
    refuses(good, state, given, operation, now, deadline, budgets, bad, M::ActionError::STATE);
    for (uint16_t flip = 1; flip; flip = uint16_t(flip << 1)) {
        given = fine; // Every status bit counts: disputed ones are compared, never interpreted.
        given.reviewedStatusword = uint16_t(POWER_UP_STATUS ^ flip);
        refuses(good, state, given, operation, now, deadline, budgets, bad, M::ActionError::STATE);
    }
    // Arguments and budgets.
    refuses(good, state, fine, 0, now, deadline, budgets, bad, M::ActionError::ARGUMENT);
    refuses(good, state, fine, operation, now, now, budgets, bad, M::ActionError::ARGUMENT);
    // Three intervals and 26 full service budgets: exactly that much is refused, one more is not.
    const uint64_t lead = minimumLead(budgets);
    CHECK(lead == 3 * INTERVAL + 26 * uint64_t(QUEUE + RESPONSE) + 1);
    refuses(good, state, fine, operation, now, now + lead - 1, budgets, bad, M::ActionError::ARGUMENT);
    CHECK(L::validateEnable(output, good, state, fine, operation, now, now + lead, budgets).isOk());
    L::ActionOptions huge = budgets;
    huge.commandIntervalUs = UINT64_MAX / 2; // Cannot wrap.
    refuses(good, state, fine, operation, now, UINT64_MAX, huge, bad, M::ActionError::ARGUMENT);
    // The evidence is one trial's: nonzero identity, this reservation, this operation, unexpired.
    given = fine;
    given.trial = 0;
    refuses(good, state, given, operation, now, deadline, budgets, bad, M::ActionError::ARGUMENT);
    for (unsigned field = 0; field < 4; ++field) {
        given = fine;
        if (field == 0)
            ++given.reservation.generation;
        else if (field == 1)
            ++given.reservation.acquiredUs;
        else if (field == 2)
            ++given.reservation.target.channel.binding.bindingGeneration;
        else
            given.reservation.owner ^= 1;
        refuses(good, state, given, operation, now, deadline, budgets, bad, M::ActionError::TARGET);
    }
    given = fine;
    given.expiresUs = now;
    refuses(good, state, given, operation, now, deadline, budgets, bad, M::ActionError::TIME);
    given.expiresUs = now + 1;
    CHECK(L::validateEnable(output, good, state, given, operation, now, deadline, budgets).isOk());
    given = fine;
    given.operation = operation + 1;
    refuses(good, state, given, operation, now, deadline, budgets, bad, M::ActionError::STATE);
    // 0x8037 with 2100=1 is the manual's enabled pattern: only an attended observation admits it.
    given = fine;
    given.powerUpShaft = L::ShaftObservation::NONE;
    refuses(good, state, given, operation, now, deadline, budgets, bad, M::ActionError::STATE);
    given.powerUpShaft = L::ShaftObservation::HOLDING;
    CHECK(L::validateEnable(output, good, state, given, operation, now, deadline, budgets).isOk());
    given.powerUpShaft = static_cast<L::ShaftObservation>(3);
    refuses(good, state, given, operation, now, deadline, budgets, bad, M::ActionError::ARGUMENT);
    L::ActionOptions changed = budgets;
    changed.commandIntervalUs = 0;
    refuses(good, state, fine, operation, now, deadline, changed, bad, M::ActionError::ARGUMENT);
    changed.commandIntervalUs = INTERVAL - 1;
    refuses(good, state, fine, operation, now, deadline, changed, bad, M::ActionError::ARGUMENT);
    changed = budgets;
    changed.maximumAgeUs = uint64_t(QUEUE + uint64_t(RESPONSE)) * 9 - 1;
    refuses(good, state, fine, operation, now, deadline, changed, bad, M::ActionError::ARGUMENT);
    changed = budgets;
    changed.observationIntervalUs = 0;
    refuses(good, state, fine, operation, now, deadline, changed, bad, M::ActionError::ARGUMENT);
    changed = budgets;
    changed.service.queueTimeoutUs = 0;
    refuses(good, state, fine, operation, now, deadline, changed, bad, M::ActionError::ARGUMENT);
    changed = budgets;
    changed.service.identityWidths = L::IdentityWidths::MEASURED_U8;
    refuses(good, state, fine, operation, now, deadline, changed, bad, M::ActionError::ARGUMENT);
    changed = budgets;
    changed.service.countWidths = L::CountWidths::MEASURED_U16;
    refuses(good, state, fine, operation, now, deadline, changed, bad, M::ActionError::ARGUMENT);
    // Reservation.
    L::AxisReservation reservation = good;
    reservation.model = static_cast<L::SettingsModel>(0xff);
    refuses(reservation, state, fine, operation, now, deadline, budgets, bad, M::ActionError::ARGUMENT);
    reservation = good;
    reservation.owner = 0;
    refuses(reservation, state, fine, operation, now, deadline, budgets, bad, M::ActionError::ARGUMENT);
    reservation = good;
    reservation.acquiredUs = now + 1;
    refuses(reservation, state, fine, operation, now, deadline, budgets, bad, M::ActionError::ARGUMENT);
    // A state read older than the reservation is not evidence under it.
    reservation = good;
    reservation.acquiredUs = state.startedUs + 1;
    given = fine; // Evidence reviewed for that reservation, so only the read can refuse.
    given.reservation = reservation;
    refuses(reservation, state, given, operation, now, deadline, budgets, bad, M::ActionError::STATE);
    // A changed node epoch makes the read stale.
    reservation = good;
    ++reservation.target.nodeGeneration;
    given.reservation = reservation;
    refuses(reservation, state, given, operation, now, deadline, budgets, bad, M::ActionError::STATE);
    // A read that did not finish cleanly, whichever way, is not this evidence.
    static L::ReadContext altered;
    altered = state;
    altered.state = M::ReadState::FAILED;
    refuses(good, altered, fine, operation, now, deadline, budgets, bad, M::ActionError::STATE);
    altered = state;
    altered.uncertain = true;
    refuses(good, altered, fine, operation, now, deadline, budgets, bad, M::ActionError::STATE);
    altered = state;
    altered.source = L::ReadSource::TPDO;
    refuses(good, altered, fine, operation, now, deadline, budgets, bad, M::ActionError::STATE);
    altered = state;
    altered.invalidated = true;
    refuses(good, altered, fine, operation, now, deadline, budgets, bad, M::ActionError::STATE);
    // Age: exactly at the bound is accepted, one past it refused.
    L::ReadWindow window;
    CHECK(L::validateStateBlock(state, L::StateBlock::ERRORS, r.current(), now, budgets.maximumAgeUs, window));
    const uint64_t expire = window.earliestUs + budgets.maximumAgeUs;
    CHECK(L::validateEnable(output, good, state, fine, operation, expire, expire + 400000, budgets).isOk());
    refuses(good, state, fine, operation, expire + 1, expire + 400001, budgets, bad, M::ActionError::STATE);
    CHECK(r.wire.sentCount == sent);
    // Overlap with each borrowed input.
    static L::ActionContext aliasing;
    Saved<L::ActionContext> aliasingSaved(aliasing);
    aliasing.reservation = good;
    aliasingSaved = Saved<L::ActionContext>(aliasing);
    CHECK(L::prepareEnable(aliasing, aliasing.reservation, state, fine, operation, now, deadline, budgets).detail ==
          int32_t(M::ActionError::OVERLAP));
    CHECK(L::prepareEnable(aliasing, good, aliasing.read, fine, operation, now, deadline, budgets).detail ==
          int32_t(M::ActionError::OVERLAP));
    CHECK(L::prepareEnable(aliasing, good, state, aliasing.enable.evidence, operation, now, deadline, budgets).detail ==
          int32_t(M::ActionError::OVERLAP));
    CHECK(L::prepareEnable(aliasing, good, state, fine, operation, now, deadline, aliasing.options).detail ==
          int32_t(M::ActionError::OVERLAP));
    aliasingSaved.unchanged(aliasing);
}

// 3. Wrong starting reports, each refused by a real read through the real owner.
void wrongStartRefuses() {
    struct Case {
        uint16_t monitor;
        uint8_t mode;
        uint16_t status;
        uint8_t errorRegister;
        uint16_t errorCode;
        int32_t velocity;
        bool ok;
    };
    const Case cases[] = {
        {1, 1, POWER_UP_STATUS, 0, 0, 0, true},
        {1 | L::MONITOR_RELEASED, 1, POWER_UP_STATUS, 0, 0, 0, true},  // Released either way.
        {1 | L::MONITOR_HOMING_COMPLETE, 1, POWER_UP_STATUS, 0, 0, 0, true},
        {1, 0, POWER_UP_STATUS, 0, 0, 0, false}, // The measured power-up mode byte.
        {1, 3, POWER_UP_STATUS, 0, 0, 0, false}, // ZH86 p13: the motor starts at the end of 6,7,15.
        {1, 6, POWER_UP_STATUS, 0, 0, 0, false},
        {1, 0x81, POWER_UP_STATUS, 0, 0, 0, false},
        {0, 1, POWER_UP_STATUS, 0, 0, 0, false}, // Not in place: maybe an interrupted curve.
        {1 | L::MONITOR_RUNNING, 1, POWER_UP_STATUS, 0, 0, 0, false},
        {1 | L::MONITOR_ALARM, 1, POWER_UP_STATUS, 0, 0, 0, false},
        {1 | L::MONITOR_POSITIVE_SOFT_LIMIT, 1, POWER_UP_STATUS, 0, 0, 0, false},
        {1 | L::MONITOR_NEGATIVE_SOFT_LIMIT, 1, POWER_UP_STATUS, 0, 0, 0, false},
        {1 | 0x0080, 1, POWER_UP_STATUS, 0, 0, 0, false}, // Unknown 2100 bits.
        {1 | 0x8000, 1, POWER_UP_STATUS, 0, 0, 0, false},
        {1, 1, POWER_UP_STATUS, 1, 0, 0, false},
        {1, 1, POWER_UP_STATUS, 0, 0xff05, 0, false}, // 603F may keep an old alarm; still refused.
        {1, 1, POWER_UP_STATUS, 0, 0, 1, false},
    };
    for (const Case& k : cases) {
        Rig r;
        r.drive.monitor = k.monitor;
        r.drive.mode = k.mode;
        r.drive.status = k.status;
        r.drive.errorRegister = k.errorRegister;
        r.drive.errorCode = k.errorCode;
        r.drive.velocity = k.velocity;
        L::ReadContext state;
        admittedRig(r, state);
        const M::Status status = r.request(state);
        CHECK(status.isOk() == k.ok);
        CHECK(!status.isOk() || r.a.enableActive());
        if (!k.ok) {
            CHECK(status.detail == int32_t(M::ActionError::STATE));
            CHECK(r.a.enableAction().state == M::ActionState::EMPTY);
            CHECK(!r.a.motionHistoryWritten());
            quiet(r, 4);
        }
    }
    // A read of fewer blocks is not enough, whichever one is missing.
    const L::StateBlock blocks[] = {L::StateBlock::ERRORS, L::StateBlock::MONITORS, L::StateBlock::STATUS,
                                    L::StateBlock::FEEDBACK};
    for (L::StateBlock missing : blocks) {
        Rig r;
        r.open();
        L::ReadContext state;
        r.readState(state, uint8_t(FULL_MASK & ~bit(missing)));
        CHECK(r.request(state).detail == int32_t(M::ActionError::STATE));
    }
    // Disputed status bits with a clean 2100: bit 3 and bit 7 are each an alarm in some manual, and
    // bit 14 running. No union: an operator who reviewed the word may proceed, and any other word
    // is refused. 2100 is what says alarm or running, as its two manuals agree.
    const uint16_t disputed[] = {0x0008, 0x0080, 0x4000, 0x1000, 0x0400};
    for (uint16_t flag : disputed) {
        Rig r;
        r.drive.status = uint16_t(POWER_UP_STATUS | flag);
        L::ReadContext state;
        admittedRig(r, state);
        CHECK(r.request(state).detail == int32_t(M::ActionError::STATE));
        CHECK(r.request(state, evidence(uint16_t(POWER_UP_STATUS | flag))).isOk());
    }
    // The enabled pattern is refused without an attended power-up observation; others need none.
    struct Pattern {
        uint16_t status, monitor;
        bool enabledPattern;
    };
    const Pattern patterns[] = {{POWER_UP_STATUS, 1, true},
                                {POWER_UP_STATUS, 1 | L::MONITOR_RELEASED, false},
                                {uint16_t(POWER_UP_STATUS & ~1u), 1, false},
                                {uint16_t(POWER_UP_STATUS & ~4u), 1, false}};
    for (const Pattern& p : patterns)
        for (unsigned observed = 0; observed < 3; ++observed) {
            Rig r;
            r.drive.status = p.status;
            r.drive.monitor = p.monitor;
            L::ReadContext state;
            admittedRig(r, state);
            L::EnableEvidence given = evidence(p.status);
            given.powerUpShaft = static_cast<L::ShaftObservation>(observed);
            const M::Status status = r.request(state, given);
            CHECK(status.isOk() == (!p.enabledPattern || observed != 0));
            if (!status.isOk())
                CHECK(status.detail == int32_t(M::ActionError::STATE) && r.wire.sentCount == r.startFrame);
        }
    // The same measured word under two manuals is why decoding cannot admit an enable.
    L::StatusIndications en, zh;
    CHECK(L::interpretStatusword(POWER_UP_STATUS, L::StatusLayout::EN86, en));
    CHECK(L::interpretStatusword(POWER_UP_STATUS, L::StatusLayout::ZH86, zh));
    CHECK(en.running == L::StateIndication::YES && zh.running == L::StateIndication::NO);
}

// 4. Targets decide before any word: never zeroed, written or interpreted.
void targetsDecideBeforeAnyWord() {
    struct Case {
        int32_t position, velocity, reviewedPosition, reviewedVelocity;
        bool ok;
    };
    const Case cases[] = {
        {5000, 120, 5000, 120, true},
        {0, 0, 0, 0, true},           // Zero is one more value that needs review.
        {-5000, -120, -5000, -120, true},
        {5000, 120, 5000, 0, false},  // Nonzero velocity target of unknown origin.
        {5000, 121, 5000, 120, false},
        {5001, 120, 5000, 120, false},
        {0, 0, 5000, 120, false},     // A review of other values does not cover zero either.
        {5000, 120, 0, 0, false},
        {-1, 120, 1, 120, false},     // Signed bytes.
    };
    for (const Case& k : cases) {
        Rig r;
        r.drive.positionTarget = k.position;
        r.drive.velocityTarget = k.velocity;
        L::ReadContext state;
        admittedRig(r, state);
        CHECK(r.request(state, evidence(POWER_UP_STATUS, k.reviewedPosition, k.reviewedVelocity)).isOk());
        r.finishEnable();
        const L::ActionContext& c = r.a.enableAction();
        CHECK(c.enable.positionTargetKnown && c.enable.positionTarget == k.position);
        CHECK(c.enable.velocityTargetKnown && c.enable.velocityTarget == k.velocity);
        if (k.ok) {
            CHECK(c.state == M::ActionState::SUCCEEDED && wordsFrom(r, r.startFrame) == 3);
        } else {
            CHECK(c.state == M::ActionState::FAILED && c.outcome == M::ActionOutcome::OBSERVATION_SCOPE);
            CHECK(c.execution == M::ActionExecution::NOT_TRANSMITTED && !c.effectsPossible);
            CHECK(!c.uncertain && !c.commandEvidenceMask);
            CHECK(r.wire.sentCount == r.startFrame + 2 && downloadsFrom(r, r.startFrame) == 0);
            CHECK(!r.a.latestControl());
            quiet(r);
        }
        CHECK(r.drive.positionTarget == k.position && r.drive.velocityTarget == k.velocity);
    }
    // A target read that fails sends no word, and a lost reply leaves its effect unknown.
    for (unsigned which = 0; which < 2; ++which)
        for (unsigned silent = 0; silent < 2; ++silent) {
            Rig r;
            L::ReadContext state;
            admittedRig(r, state);
            const uint16_t index = which ? 0x60ff : 0x607a;
            (silent ? r.drive.silentIndex : r.drive.abortIndex) = index;
            CHECK(r.request(state).isOk());
            r.finishEnable();
            const L::ActionContext& c = r.a.enableAction();
            CHECK(c.state == M::ActionState::FAILED);
            CHECK(c.outcome == (silent ? M::ActionOutcome::SERVICE_TIMEOUT : M::ActionOutcome::PROTOCOL_REJECTED));
            CHECK(c.uncertain && c.observationEvidencePresent); // An upload's effect stays unqualified.
            CHECK(c.execution == M::ActionExecution::NOT_TRANSMITTED && wordsFrom(r, r.startFrame) == 0);
            quiet(r);
        }
}

// 4b. Node 1's measured bytes: from mode 1 at 0x8437 with the shaft holding, word 6 gave 0x8031 and
// 2100=0x0011. Bit 10 follows the enable state, so it never ends the sequence; any other bit still
// does, and so does a nonzero 606C.
void zeroSpeedBitFollowsTheEnable() {
    const uint16_t modeOne = 0x8437;
    {
        Rig r;
        r.drive.status = modeOne;
        r.drive.measuredRelease = true;
        L::ReadContext state;
        admittedRig(r, state);
        L::EnableEvidence e = evidence(modeOne);
        e.powerUpShaft = L::ShaftObservation::HOLDING;
        CHECK(r.request(state, e).isOk());
        r.finishEnable();
        const L::ActionContext& c = r.a.enableAction();
        CHECK(c.state == M::ActionState::SUCCEEDED && c.outcome == M::ActionOutcome::OBSERVED);
        CHECK(wordsFrom(r, r.startFrame) == 3);
        CHECK(c.enable.steps[0].statusword == 0x8031 && c.enable.steps[0].monitor == 0x0011);
        CHECK(c.enable.steps[1].statusword == 0x8033);
        CHECK(c.enable.steps[2].statusword == modeOne && c.enable.steps[2].monitor == 0x0001);
    }
    // The same release with another bit changing, or the shaft creeping, still ends at that word.
    const uint16_t others[] = {0x0200, 0x0800, 0x1000, 0x4000};
    for (uint16_t bit : others) {
        Rig r;
        r.drive.status = modeOne;
        r.drive.measuredRelease = true;
        r.drive.changeAfterWord = 1;
        r.drive.statusChange = bit;
        L::ReadContext state;
        admittedRig(r, state);
        CHECK(r.request(state, evidence(modeOne)).isOk());
        r.finishEnable();
        const L::ActionContext& c = r.a.enableAction();
        CHECK(c.state == M::ActionState::FAILED && c.outcome == M::ActionOutcome::OBSERVATION_SCOPE);
        CHECK(wordsFrom(r, r.startFrame) == 1);
    }
    {
        Rig r;
        r.drive.status = modeOne;
        r.drive.measuredRelease = true;
        r.drive.changeAfterWord = 1;
        r.drive.velocityChange = 1;
        L::ReadContext state;
        admittedRig(r, state);
        CHECK(r.request(state, evidence(modeOne)).isOk());
        r.finishEnable();
        CHECK(r.a.enableAction().outcome == M::ActionOutcome::OBSERVATION_SCOPE);
        CHECK(wordsFrom(r, r.startFrame) == 1);
    }
}

// 5. A report at each step that leaves the supported scope ends the sequence there.
void outOfScopeAtEachStep() {
    struct Case {
        uint16_t monitorChange, statusChange;
        int32_t positionChange, velocityChange;
        int mode;
    };
    const Case cases[] = {
        {L::MONITOR_ALARM, 0, 0, 0, -1},  // Fault at each step.
        {L::MONITOR_RUNNING, 0, 0, 0, -1},
        {L::MONITOR_POSITION_COMPLETE, 0, 0, 0, -1},
        {L::MONITOR_HOMING_COMPLETE, 0, 0, 0, -1},
        {L::MONITOR_POSITIVE_SOFT_LIMIT, 0, 0, 0, -1},
        {0x0200, 0, 0, 0, -1},            // Unknown 2100 bit.
        {0, 0x0008, 0, 0, -1},            // Status alarm in one manual, monitor quiet: disagreement.
        {0, 0x0080, 0, 0, -1},            // Status alarm in others, monitor quiet.
        {0, 0x4000, 0, 0, -1},            // Status running, monitor quiet.
        {0, 0x0200, 0, 0, -1},            // Unlisted status bit.
        {0, 0x0010, 0, 0, -1},            // Bit 4, set in 0x8037, clears.
        {0, 0x0020, 0, 0, -1},            // Bit 5, set in 0x8037, clears.
        {0, 0x8000, 0, 0, -1},            // Position-complete flag dropped.
        {0, 0, 1, 0, -1},                 // Raw position moved.
        {0, 0, 0, 1, -1},                 // Raw velocity nonzero.
        {0, 0, 0, 0, 3},                  // Mode moved to velocity: refused, never corrected.
        {0, 0, 0, 0, 0},
    };
    for (unsigned at = 1; at <= 3; ++at)
        for (const Case& k : cases) {
            Rig r;
            L::ReadContext state;
            admittedRig(r, state);
            r.drive.changeAfterWord = at;
            r.drive.monitorChange = k.monitorChange;
            r.drive.statusChange = k.statusChange;
            r.drive.positionChange = k.positionChange;
            r.drive.velocityChange = k.velocityChange;
            r.drive.modeChange = k.mode;
            CHECK(r.request(state).isOk());
            r.finishEnable();
            const L::ActionContext& c = r.a.enableAction();
            CHECK(c.state == M::ActionState::FAILED && c.outcome == M::ActionOutcome::OBSERVATION_SCOPE);
            CHECK(c.execution == M::ActionExecution::ACKNOWLEDGED && c.effectsPossible && c.uncertain);
            CHECK(c.completion == M::ActionCompletion::NOT_OBSERVED);
            CHECK(c.commandEvidenceMask == uint8_t((1u << at) - 1u));
            CHECK(wordsFrom(r, r.startFrame) == at); // Prefix retained, and nothing follows.
            for (unsigned s = 0; s < 3; ++s)
                CHECK(c.enable.steps[s].present == (s + 1 < at));
            CHECK(downloadsFrom(r, r.startFrame) == at); // No fault clear, mode fix or stop.
            CHECK(c.observationKnown); // The disagreeing words stay inspectable.
            const uint16_t word = at == 1 ? 6 : at == 2 ? 7 : 15;
            CHECK(r.a.latestControl() && r.a.latestControl()->transfer.data[0] == word);
            quiet(r);
        }
}

// 6. Word terminals at each step: no replay, and an unknown or rejected word stays that.
void wordFailuresAtEachStep() {
    const uint16_t words[] = {6, 7, 15};
    for (unsigned at = 0; at < 3; ++at) {
        {
            Rig r;
            L::ReadContext state;
            admittedRig(r, state);
            r.drive.abortIndex = 0x6040;
            r.drive.abortWord = words[at];
            CHECK(r.request(state).isOk());
            r.finishEnable();
            const L::ActionContext& c = r.a.enableAction();
            CHECK(c.state == M::ActionState::FAILED && c.outcome == M::ActionOutcome::PROTOCOL_REJECTED);
            CHECK(c.execution == M::ActionExecution::REJECTED);
            CHECK(c.commandHistory[at].outcome == C::SdoOutcome::SERVER_ABORT);
            CHECK(wordsFrom(r, r.startFrame) == at + 1);
            CHECK(c.effectsPossible == true);
            quiet(r);
        }
        {
            Rig r;
            L::ReadContext state;
            admittedRig(r, state);
            r.drive.silentIndex = 0x6040;
            r.drive.silentWord = words[at];
            CHECK(r.request(state).isOk());
            r.finishEnable();
            const L::ActionContext& c = r.a.enableAction();
            CHECK(c.state == M::ActionState::FAILED && c.outcome == M::ActionOutcome::SERVICE_TIMEOUT);
            CHECK(c.execution == M::ActionExecution::UNKNOWN && c.uncertain && c.effectsPossible);
            CHECK(wordsFrom(r, r.startFrame) == at + 1); // A lost reply is never replayed.
            CHECK(!r.a.latestControl()); // A possible word withdrew the history.
            quiet(r);
            CHECK(!r.a.requestEnable(state, r.bound(evidence()), options(), r.now + 400000, r.now).isOk());
        }
    }
}

// 7. Slow replies and slow reports: the interval never shortens and polling stays finite.
void delaysAndPolling() {
    {   // A late report polls in WAITING, then the sequence continues.
        Rig r;
        L::ReadContext state;
        admittedRig(r, state);
        r.drive.lagReads = 2;
        CHECK(r.request(state).isOk());
        r.finishEnable();
        const L::ActionContext& c = r.a.enableAction();
        CHECK(c.state == M::ActionState::SUCCEEDED && c.observations == 5);
        CHECK(wordsFrom(r, r.startFrame) == 3);
    }
    {   // Low bits that never follow: finite, DEADLINE, no later word and no fallback.
        Rig r;
        L::ReadContext state;
        admittedRig(r, state);
        r.drive.followWords = false;
        const L::ActionOptions quick = options(1000, 1000);
        CHECK(r.request(state, evidence(), minimumLead(quick), quick).isOk());
        r.finishEnable();
        const L::ActionContext& c = r.a.enableAction();
        CHECK(c.state == M::ActionState::FAILED && c.outcome == M::ActionOutcome::DEADLINE);
        CHECK(c.uncertain && c.execution == M::ActionExecution::ACKNOWLEDGED);
        CHECK(wordsFrom(r, r.startFrame) == 1 && c.observations >= 2);
        quiet(r);
    }
    {   // Released never clears after 15: low bits alone never complete.
        Rig r;
        r.drive.monitor = 1 | L::MONITOR_RELEASED;
        r.drive.followWords = true;
        L::ReadContext state;
        admittedRig(r, state);
        const L::ActionOptions quick = options(1000, 1000);
        CHECK(r.request(state, evidence(POWER_UP_STATUS), minimumLead(quick), quick).isOk());
        runToWords(r, 3);
        r.drive.monitor = 1 | L::MONITOR_RELEASED; // Fake reports released on after 15.
        for (unsigned i = 0; i < 400 && r.a.enableActive(); ++i) {
            r.drive.monitor = 1 | L::MONITOR_RELEASED;
            r.step();
        }
        const L::ActionContext& c = r.a.enableAction();
        CHECK(c.state == M::ActionState::FAILED && c.outcome == M::ActionOutcome::DEADLINE);
        CHECK(!c.enable.steps[2].present && c.enable.steps[1].present);
    }
    {   // A slow drive reply moves the next word later, as the interval runs from the reply.
        Rig r;
        L::ReadContext state;
        admittedRig(r, state);
        CHECK(r.request(state).isOk());
        runToWords(r, 1);
        const unsigned first = r.startFrame + 2;
        const unsigned sentBefore = r.served;
        r.drive.answer = false;
        r.steps(2);
        r.drive.answer = true;
        r.served = sentBefore;
        r.finishEnable();
        const L::ActionContext& c = r.a.enableAction();
        CHECK(c.state == M::ActionState::SUCCEEDED);
        const uint64_t ack = c.commandHistory[0].completedUs;
        CHECK(ack > r.sentAt[first] + TICK);
        const unsigned second = first + 8;
        CHECK(wordAt(r, second, 7) && r.sentAt[second] >= ack + INTERVAL);
    }
    {   // A deadline that cannot fit the third word's slot is refused before any traffic.
        Rig r;
        L::ReadContext state;
        admittedRig(r, state);
        L::ActionOptions slow = options();
        slow.commandIntervalUs = 50000;
        CHECK(r.request(state, evidence(), 100004, slow).detail == int32_t(M::ActionError::ARGUMENT));
        CHECK(r.wire.sentCount == r.startFrame && r.a.enableAction().state == M::ActionState::EMPTY);
    }
}

// 8. Local withdrawal at every point, and precedence at the cutoff.
void cancellationAndDeadline() {
    {   // Before any upload left.
        Rig r;
        L::ReadContext state;
        admittedRig(r, state);
        CHECK(r.request(state).isOk());
        CHECK(r.a.cancelEnable(r.now));
        const L::ActionContext& c = r.a.enableAction();
        CHECK(c.state == M::ActionState::FAILED && c.outcome == M::ActionOutcome::CANCELLED);
        CHECK(!c.effectsPossible && !c.uncertain);
        CHECK(!r.a.cancelEnable(r.now));
        quiet(r);
        CHECK(r.wire.sentCount == r.startFrame);
    }
    for (unsigned words = 1; words <= 3; ++words) {
        {   // While the word is unanswered: its terminal settles into the context, then no more.
            Rig r;
            L::ReadContext state;
            admittedRig(r, state);
            CHECK(r.request(state).isOk());
            runToWords(r, words);
            const unsigned sent = r.wire.sentCount;
            CHECK(r.a.cancelEnable(r.now));
            CHECK(r.a.enableActive()); // In flight: only its terminal can settle it.
            r.finishEnable();
            const L::ActionContext& c = r.a.enableAction();
            CHECK(c.state == M::ActionState::FAILED && c.outcome == M::ActionOutcome::CANCELLED);
            CHECK(c.commandEvidenceMask == uint8_t((1u << words) - 1u));
            CHECK(c.execution == M::ActionExecution::ACKNOWLEDGED);
            CHECK(c.uncertain && c.effectsPossible); // Local cancel is not a stop or an undo.
            CHECK(r.wire.sentCount == sent && wordsFrom(r, r.startFrame) == words);
            CHECK(r.a.latestControl() && downloadsFrom(r, r.startFrame) == words);
            quiet(r);
        }
        if (words < 3) { // Waiting between words.
            Rig r;
            L::ReadContext state;
            admittedRig(r, state);
            CHECK(r.request(state).isOk());
            runToWords(r, words);
            for (unsigned i = 0; i < 100 && r.a.enableAction().commandCursor != words; ++i)
                r.step();
            CHECK(r.a.enableAction().phase == L::ActionPhase::COMMAND);
            const unsigned sent = r.wire.sentCount;
            CHECK(r.a.cancelEnable(r.now, M::ActionOutcome::DRIVE_FAULT));
            const L::ActionContext& c = r.a.enableAction();
            CHECK(c.state == M::ActionState::FAILED && c.outcome == M::ActionOutcome::DRIVE_FAULT);
            quiet(r);
            CHECK(r.wire.sentCount == sent && wordsFrom(r, r.startFrame) == words);
        }
    }
    for (unsigned reason = 0; reason < 2; ++reason) {
        // The last field of the last cycle in flight: its terminal is evidence, not completion.
        Rig r;
        L::ReadContext state;
        admittedRig(r, state);
        CHECK(r.request(state).isOk());
        runToWords(r, 3);
        bool inFlight = false;
        for (unsigned i = 0; i < 200 && !inFlight; ++i) {
            r.step();
            const unsigned last = r.wire.sentCount - 1;
            inFlight = wordsFrom(r, r.startFrame) == 3 && uploadAt(r, last, 0x606c) && r.served <= last &&
                       r.a.enableAction().commandEvidenceMask == 7;
        }
        CHECK(inFlight && r.a.enableActive());
        const M::ActionOutcome why = reason ? M::ActionOutcome::DRIVE_FAULT : M::ActionOutcome::CANCELLED;
        CHECK(r.a.cancelEnable(r.now, why));
        CHECK(r.a.enableActive() && r.a.enableAction().withdrawal == why);
        r.finishEnable();
        const L::ActionContext& c = r.a.enableAction();
        CHECK(c.state == M::ActionState::FAILED && c.outcome == why);
        CHECK(c.completion == M::ActionCompletion::NOT_OBSERVED && !c.enable.steps[2].present);
        CHECK(c.observationEvidencePresent && c.observationEvidence.transfer.index == 0x606c &&
              c.observationEvidence.outcome == C::SdoOutcome::SUCCESS);
        CHECK(c.read.state == M::ReadState::SUCCEEDED); // Kept as evidence, never interpreted.
        CHECK(c.uncertain && wordsFrom(r, r.startFrame) == 3);
        quiet(r);
    }
    {   // A target upload in flight: its value is kept, and no word follows.
        Rig r;
        L::ReadContext state;
        admittedRig(r, state);
        CHECK(r.request(state).isOk());
        r.step();
        CHECK(uploadAt(r, r.wire.sentCount - 1, 0x607a) || r.a.enableAction().enable.targetCursor == 0);
        CHECK(r.a.cancelEnable(r.now));
        r.finishEnable();
        const L::ActionContext& c = r.a.enableAction();
        CHECK(c.outcome == M::ActionOutcome::CANCELLED && c.enable.positionTargetKnown &&
              !c.enable.velocityTargetKnown && wordsFrom(r, r.startFrame) == 0);
    }
    {   // Only local reasons are accepted.
        Rig r;
        L::ReadContext state;
        admittedRig(r, state);
        CHECK(r.request(state).isOk());
        CHECK(!r.a.cancelEnable(r.now, M::ActionOutcome::OBSERVED));
        CHECK(!r.a.cancelEnable(r.now - 1));
        CHECK(r.a.enableActive());
    }
    {   // At the cutoff a cancel reports DEADLINE, and a request in flight waits for its terminal.
        Rig r;
        L::ReadContext state;
        admittedRig(r, state);
        const L::ActionOptions quick = options(1000, 1000);
        const uint64_t cutoff = r.now + minimumLead(quick);
        CHECK(r.request(state, evidence(), minimumLead(quick), quick).isOk());
        r.drive.followWords = false;
        while (r.now < cutoff && r.a.enableActive())
            r.turn(r.now + TICK > cutoff ? cutoff : r.now + TICK);
        for (unsigned i = 0; i < 20 && r.a.enableActive(); ++i)
            r.step();
        CHECK(r.a.enableAction().outcome == M::ActionOutcome::DEADLINE);
    }
    {   // Requests refused while an enable runs, and a terminal the session owns.
        Rig r;
        L::ReadContext state;
        admittedRig(r, state);
        CHECK(r.request(state).isOk());
        CHECK(r.a.busy() && r.a.beginOperation(r.now + 1000, r.now) == 0);
        CHECK(!r.a.close());
        r.step();
        // A finished request of the enable cannot be taken by a caller.
        r.serviceSession = false;
        r.steps(3);
        C::SdoCompletion stolen;
        CHECK(!r.a.settle(stolen));
        r.serviceSession = true;
        CHECK(r.request(state).detail == int32_t(M::ActionError::STATE));
        r.finishEnable();
        CHECK(!r.a.busy() && r.a.close());
    }
}

// 9. Boot, controller loss and a stale epoch at every point. Nothing follows, nothing is replayed.
void lifecycleLoss() {
    for (unsigned words = 0; words <= 3; ++words) {
        for (unsigned kind = 0; kind < 2; ++kind) {
            Rig r;
            L::ReadContext state;
            admittedRig(r, state);
            CHECK(r.request(state).isOk());
            if (words)
                runToWords(r, words);
            else
                r.steps(1);
            if (kind) {
                CHECK(r.owner.reportController(C::ControllerState::BUS_OFF, r.now));
            } else {
                r.later(frame(0x701, 1));
            }
            r.finishEnable();
            const unsigned sent = r.wire.sentCount;
            const L::ActionContext& c = r.a.enableAction();
            CHECK(c.state == M::ActionState::FAILED);
            CHECK(c.outcome == M::ActionOutcome::INVALIDATED || c.outcome == M::ActionOutcome::TRANSPORT_ERROR);
            CHECK(wordsFrom(r, r.startFrame) == words);
            CHECK(!r.a.latestControl());
            if (words)
                CHECK(c.uncertain && c.effectsPossible);
            r.steps(10);
            CHECK(r.wire.sentCount == sent);
            // The session keeps its record of those words whatever route comes next.
            CHECK(r.a.motionHistoryWritten() == (words != 0));
        }
    }
    // Stale generation and wrong owner evidence, directly on the public calls: all atomic.
    Rig r;
    L::ReadContext state;
    admittedRig(r, state);
    CHECK(r.request(state).isOk());
    runToWords(r, 1);
    const L::AxisReservation live = reservationOf(r);
    const L::ActionContext& c = r.a.enableAction();
    Saved<L::ActionContext> saved(c);
    L::PreparedAction work;
    L::ReadTarget stale = live.target;
    ++stale.nodeGeneration;
    CHECK(L::nextAction(c, live, stale, r.now, work).detail == int32_t(M::ActionError::TARGET));
    L::AxisReservation other = live;
    ++other.generation;
    CHECK(L::nextAction(c, other, live.target, r.now, work).detail == int32_t(M::ActionError::TARGET));
    saved.unchanged(c);
}

// 10. A stop after each step: the session's own history decides, never a copied word.
void stopAfterEachStep() {
    E::StopPolicy policy;
    policy.options = options();
    for (unsigned words = 1; words <= 2; ++words)
        for (unsigned fresh = 0; fresh < 2; ++fresh) {
            // Acknowledged 6 or 7, with no stop arrow. The enable's own finished cycle is a fresh idle
            // report, which refuses. With no fresh report the best-effort word goes instead.
            Rig r;
            L::ReadContext state;
            admittedRig(r, state);
            CHECK(r.request(state).isOk());
            runToWords(r, words);
            for (unsigned i = 0; i < 200 && r.a.enableAction().commandCursor != words; ++i)
                r.step();
            CHECK(r.a.enableAction().phase == L::ActionPhase::COMMAND &&
                  r.a.enableAction().read.state == M::ReadState::SUCCEEDED);
            policy.deadlineUs = r.now + 20000;
            policy.bestEffortDeadlineUs = r.now + 30000;
            const uint16_t word = words == 1 ? 6 : 7;
            CHECK(r.a.latestControl() && r.a.latestControl()->transfer.data[0] == word);
            const unsigned admitted = wordsFrom(r, r.startFrame);
            static L::ReadContext none;
            const E::StopPath path = r.a.requestStop(L::StopKind::QUICK_STOP,
                                                     fresh ? r.a.enableAction().read : none, policy, r.now);
            // Refused or not, the request ended the enable: no further enable word goes out.
            r.finishEnable();
            const L::ActionContext& c = r.a.enableAction();
            CHECK(c.state == M::ActionState::FAILED && c.outcome == M::ActionOutcome::CANCELLED);
            CHECK(c.uncertain);
            r.steps(40);
            if (fresh) {
                CHECK(path == E::StopPath::NONE && wordsFrom(r, r.startFrame) == admitted);
            } else {
                CHECK(path != E::StopPath::NONE && wordsFrom(r, r.startFrame) == admitted + 1);
                CHECK(wordAt(r, r.wire.sentCount - 1, 0x000b) || r.a.bestEffort().commandEvidencePresent);
            }
        }
    // Both orderings agree. After a known word 6, or with that word still in flight, a fresh running
    // report gets 000B and a fresh idle report that is not enabled is refused.
    for (unsigned running = 0; running < 2; ++running)
        for (unsigned known = 0; known < 2; ++known) {
            Rig r;
            r.open();
            L::ReadContext state;
            if (known) {
                r.writeThrough(0x6040, 2, 6);
                CHECK(r.a.latestControl() && r.a.latestControl()->transfer.data[0] == 6);
            }
            r.drive.monitor = uint16_t(running ? 1 | L::MONITOR_RUNNING : 1);
            r.drive.status = uint16_t((POWER_UP_STATUS & ~7u) | 1u); // What the table pairs with 6.
            r.readState(state);
            if (!known)
                r.writeThrough(0x6040, 2, 6, false); // Admitted, not yet answered.
            policy.deadlineUs = r.now + 20000;
            policy.bestEffortDeadlineUs = r.now + 30000;
            const unsigned from = r.wire.sentCount;
            const E::StopPath path = r.a.requestStop(L::StopKind::QUICK_STOP, state, policy, r.now);
            CHECK((path != E::StopPath::NONE) == bool(running));
            for (unsigned i = 0; i < 20; ++i) {
                r.step();
                C::SdoCompletion ignored;
                r.a.settle(ignored); // The caller's own word settles, as it would in an application.
            }
            unsigned stops = 0;
            for (unsigned i = from; i < r.wire.sentCount; ++i)
                stops += wordAt(r, i, 0x000b);
            CHECK(stops == running);
        }
    {   // A time that moved backward refuses the stop but still ends the enable.
        Rig r;
        L::ReadContext state;
        admittedRig(r, state);
        CHECK(r.request(state).isOk());
        runToWords(r, 1);
        for (unsigned i = 0; i < 200 && r.a.enableAction().commandCursor != 1; ++i)
            r.step();
        policy.deadlineUs = r.now + 20000;
        policy.bestEffortDeadlineUs = r.now + 30000;
        const unsigned admitted = wordsFrom(r, r.startFrame);
        CHECK(r.a.requestStop(L::StopKind::QUICK_STOP, state, policy, r.now - 1) == E::StopPath::NONE);
        CHECK(!r.a.enableActive());
        const L::ActionContext& c = r.a.enableAction();
        CHECK(c.outcome == M::ActionOutcome::CANCELLED && c.servicedUs == r.now);
        r.steps(40);
        CHECK(wordsFrom(r, r.startFrame) == admitted);
    }
    {   // 15 acknowledged, its cycle unfinished: no fresh state yet, so the best-effort word goes.
        Rig r;
        L::ReadContext state;
        admittedRig(r, state);
        CHECK(r.request(state).isOk());
        runToWords(r, 3);
        for (unsigned i = 0; i < 100 && !(r.a.enableAction().commandEvidenceMask & 4); ++i)
            r.step();
        CHECK(r.a.latestControl() && r.a.latestControl()->transfer.data[0] == 0x0f);
        CHECK(r.a.enableActive());
        policy.deadlineUs = r.now + 20000;
        policy.bestEffortDeadlineUs = r.now + 30000;
        const unsigned from = r.wire.sentCount;
        // Its own later state is not yet complete; an earlier state read predates the word.
        CHECK(r.a.requestStop(L::StopKind::HALT, state, policy, r.now) == E::StopPath::HELD ||
              r.a.stopPath() == E::StopPath::BEST_EFFORT);
        r.steps(10);
        CHECK(wordsFrom(r, from) == 1);
        unsigned word = from;
        while (r.wire.sent[word].data[0] != 0x2b)
            ++word;
        CHECK(wordAt(r, word, 0x000b));
        CHECK(r.a.enableAction().outcome == M::ActionOutcome::CANCELLED);
    }
    for (unsigned kind = 0; kind < 2; ++kind) {
        // Completed enable: its last cycle is the fresh state the sequenced stop needs.
        Rig r;
        L::ReadContext state;
        admittedRig(r, state);
        CHECK(r.request(state).isOk());
        r.finishEnable();
        CHECK(r.a.enableAction().state == M::ActionState::SUCCEEDED);
        const L::StopKind selected = kind ? L::StopKind::QUICK_STOP : L::StopKind::HALT;
        policy.deadlineUs = r.now + 20000;
        policy.bestEffortDeadlineUs = r.now + 30000;
        const unsigned from = r.wire.sentCount;
        CHECK(r.a.requestStop(selected, r.a.enableAction().read, policy, r.now) == E::StopPath::CONFIRMED);
        r.steps(2);
        CHECK(wordAt(r, from, kind ? 0x000b : 0x010f));
        const L::ActionContext& stop = r.a.stopAction(selected);
        CHECK(stop.predecessorWord == 0x000f && stop.predecessorDocumented);
        CHECK(stop.predecessorBasis == (kind ? L::PredecessorBasis::EN86_ZH86_OP86 : L::PredecessorBasis::ZH86_OP86));
    }
    {   // A word in flight: withdrawal is immediate, and a stop waits for that frame to settle.
        Rig r;
        L::ReadContext state;
        admittedRig(r, state);
        CHECK(r.request(state).isOk());
        runToWords(r, 1);
        CHECK(!r.a.latestControl()); // Admission withdrew any earlier word.
        policy.deadlineUs = r.now + 20000;
        policy.bestEffortDeadlineUs = r.now + 30000;
        // Fresh pre-enable state reads low bits 7 and a clean monitor, so the stop's refusal test
        // passes, and with no history this takes the best-effort path.
        const E::StopPath path = r.a.requestStop(L::StopKind::QUICK_STOP, state, policy, r.now);
        CHECK(path == E::StopPath::HELD || path == E::StopPath::BEST_EFFORT);
        for (unsigned i = 0; i < 20; ++i)
            r.step();
        CHECK(r.a.enableAction().state == M::ActionState::FAILED);
        CHECK(r.a.enableAction().commandHistory[0].outcome == C::SdoOutcome::SUCCESS);
        CHECK(wordsFrom(r, r.startFrame) == 2); // Word 6, then 000B, and never word 7.
    }
    {   // A copied acknowledgement does not transfer to another reservation.
        Rig r;
        L::ReadContext state;
        admittedRig(r, state);
        CHECK(r.request(state).isOk());
        r.finishEnable();
        const C::SdoCompletion copied = *r.a.latestControl();
        L::AxisReservation fresher = reservationOf(r);
        fresher.acquiredUs = copied.admittedUs + 1;
        static L::ActionContext stop;
        Saved<L::ActionContext> saved(stop);
        CHECK(L::prepareStop(stop, fresher, r.a.enableAction().read, copied, L::StopKind::QUICK_STOP, 900,
                             r.now, r.now + 20000, options())
                  .detail == int32_t(M::ActionError::STATE));
        saved.unchanged(stop);
    }
}

// 11. The session's own history: any earlier write refuses, and reopening restores nothing.
void sessionHistory() {
    {   // Fresh word from this session.
        Rig r;
        L::ReadContext state;
        r.open();
        r.writeThrough(0x6040, 2, 0x000f);
        CHECK(r.a.latestControl());
        r.readState(state);
        CHECK(r.request(state).detail == int32_t(M::ActionError::STATE));
        CHECK(r.a.enableAction().state == M::ActionState::EMPTY);
        // Reopening keeps the record, even with a current state read.
        CHECK(r.a.close());
        r.open();
        CHECK(!r.a.latestControl());
        CHECK(r.a.motionHistoryWritten());
        r.readState(state);
        CHECK(r.request(state).detail == int32_t(M::ActionError::STATE));
        quiet(r, 4);
    }
    {   // A target, a settings word or a different mode each refuse.
        const uint16_t indexes[] = {0x607a, 0x60ff, 0x6081, 0x2200, 0x6060};
        for (uint16_t index : indexes) {
            Rig r;
            L::ReadContext state;
            r.open();
            r.writeThrough(index, index == 0x6060 ? 1 : 2, index == 0x6060 ? 3 : 1);
            r.drive.mode = 1;
            r.readState(state);
            CHECK(r.a.motionHistoryWritten());
            CHECK(r.request(state).detail == int32_t(M::ActionError::STATE));
        }
    }
    {   // An acknowledged 6060=1 is the one write that leaves the history clean.
        Rig r;
        L::ReadContext state;
        r.drive.mode = 0;
        r.open();
        const unsigned before = r.wire.sentCount;
        r.writeThrough(0x6060, 1, 1);
        CHECK(r.drive.mode == 1 && !r.a.motionHistoryWritten() && !r.a.latestControl());
        const uint8_t modeOne[] = {0x2f, 0x60, 0x60, 0x00, 0x01, 0x00, 0x00, 0x00};
        CHECK(r.wire.sentCount == before + 1 && r.wire.sent[before].id == 0x601 &&
              !std::memcmp(r.wire.sent[before].data, modeOne, sizeof modeOne));
        r.readState(state);
        CHECK(r.request(state).isOk());
        r.finishEnable();
        CHECK(r.a.enableAction().state == M::ActionState::SUCCEEDED);
    }
    {   // A 6060=1 with a lost reply leaves its effect unknown.
        Rig r;
        r.open();
        r.drive.silentIndex = 0x6060;
        r.writeThrough(0x6060, 1, 1, false);
        for (unsigned i = 0; i < 40 && !r.a.enableActive(); ++i) {
            r.step();
            C::SdoCompletion terminal;
            if (r.a.settle(terminal)) {
                CHECK(terminal.outcome == C::SdoOutcome::TIMEOUT);
                break;
            }
        }
        CHECK(r.a.motionHistoryWritten());
    }
    {   // No second enable, whether the first succeeded or failed: never replayed.
        for (unsigned fail = 0; fail < 2; ++fail) {
            Rig r;
            L::ReadContext state;
            admittedRig(r, state);
            if (fail)
                r.drive.abortIndex = 0x6040;
            CHECK(r.request(state).isOk());
            r.finishEnable();
            CHECK(r.a.motionHistoryWritten());
            const unsigned sent = r.wire.sentCount;
            CHECK(r.a.requestEnable(state, r.bound(evidence()), options(), r.now + 400000, r.now).detail ==
                  int32_t(M::ActionError::STATE));
            quiet(r);
            CHECK(r.wire.sentCount == sent);
        }
    }
    {   // A failed target review wrote nothing, so the record stays clean, but its trial is spent.
        Rig r;
        L::ReadContext state;
        admittedRig(r, state);
        L::EnableEvidence first = evidence(POWER_UP_STATUS, 5000, 0);
        first.trial = 7;
        CHECK(r.request(state, first).isOk());
        r.finishEnable();
        CHECK(!r.a.motionHistoryWritten());
        r.readState(state);
        L::EnableEvidence again = evidence();
        again.trial = 7;
        CHECK(r.request(state, again).detail == int32_t(M::ActionError::STATE));
        again.trial = 6;
        CHECK(r.request(state, again).detail == int32_t(M::ActionError::STATE));
        again.trial = 8;
        CHECK(r.request(state, again).isOk());
        r.finishEnable();
        CHECK(r.a.enableAction().state == M::ActionState::SUCCEEDED);
    }
    {   // Copied evidence cannot open another enable: not by a second session on the same
        // reservation, whose operations have moved on, and not after a restart's new reservation.
        Rig r;
        L::ReadContext state;
        admittedRig(r, state);
        const L::EnableEvidence used = r.bound(evidence());
        CHECK(r.a.requestEnable(state, used, options(), r.now + 400000, r.now));
        r.finishEnable();
        E::AxisSession other(r.owner); // Adopts the reservation r.a still holds.
        CHECK(other.open(r.current(), L::SettingsModel::CL86_C, 2, r.now));
        CHECK(other.nextOperation() > used.operation);
        CHECK(other.requestEnable(r.a.enableAction().read, used, options(), r.now + 400000, r.now).detail ==
              int32_t(M::ActionError::STATE));
        CHECK(!other.enableActive());
        Rig restarted;
        restarted.steps(3);
        restarted.open();
        L::ReadContext later;
        restarted.readState(later);
        CHECK(restarted.a.requestEnable(later, used, options(), restarted.now + 400000, restarted.now).detail ==
              int32_t(M::ActionError::TARGET));
    }
    {   // A stop admitted at any time ends the session's clean history.
        Rig r;
        L::ReadContext state;
        admittedRig(r, state);
        E::StopPolicy policy;
        policy.options = options();
        policy.deadlineUs = r.now + 20000;
        policy.bestEffortDeadlineUs = r.now + 30000;
        CHECK(r.a.requestStop(L::StopKind::QUICK_STOP, state, policy, r.now) != E::StopPath::NONE);
        CHECK(r.a.motionHistoryWritten());
    }
    {   // A new session object knows nothing: only the caller's evidence speaks for earlier time.
        Rig r;
        L::ReadContext state;
        admittedRig(r, state);
        L::EnableEvidence unknown = evidence();
        unknown.start = L::EnableStart::UNKNOWN;
        CHECK(r.request(state, unknown).detail == int32_t(M::ActionError::STATE));
        CHECK(r.a.enableAction().state == M::ActionState::EMPTY);
    }
    {   // A state read held inside the enable's own context is refused as overlap.
        Rig r;
        L::ReadContext state;
        admittedRig(r, state);
        CHECK(r.request(state, evidence(POWER_UP_STATUS, 5000, 0)).isOk());
        r.finishEnable();
        CHECK(r.a.requestEnable(r.a.enableAction().read, r.bound(evidence()), options(), r.now + 400000, r.now).detail ==
              int32_t(M::ActionError::OVERLAP));
    }
}

// 12. The core calls directly with owner terminals, independent of the session, and identity limits.
struct Direct {
    Rig r;
    L::ReadContext state;
    L::AxisReservation live;
    static L::ActionContext c;
    uint64_t lead = 400000;
    void start() {
        r.open();
        r.readState(state);
        live = reservationOf(r);
        const uint32_t operation = r.a.beginOperation(r.now + lead, r.now);
        CHECK(operation);
        L::resetAction(c);
        L::EnableEvidence given = evidence();
        given.trial = 1;
        given.reservation = live;
        given.operation = operation;
        given.expiresUs = r.now + 1000000;
        CHECK(L::prepareEnable(c, live, state, given, operation, r.now, r.now + lead, options()));
    }
    // One service: yield, admit, a transmission notice for a word, then the owner terminal.
    bool once() {
        L::PreparedAction work;
        for (unsigned i = 0; i < 400; ++i) {
            CHECK(L::nextAction(c, live, live.target, r.now, work));
            if (c.state != M::ActionState::ACTIVE || work.ready)
                break;
            r.step();
        }
        if (!work.ready)
            return false;
        const bool word = c.phase == L::ActionPhase::COMMAND;
        // One upload each, as the ledger's accepted action prerequisite, and never a poll.
        if (c.phase == L::ActionPhase::TARGETS)
            CHECK(work.transfer.readEffect == C::SdoReadEffect::ACCEPTED_UNQUALIFIED &&
                  work.transfer.index == (c.enable.targetCursor ? 0x60ff : 0x607a));
        O::BusRequest request;
        request.sequence = r.a.sequence();
        request.transfer = work.transfer;
        O::RequestId id;
        CHECK(r.owner.admit(request, r.now, id) == O::BusAdmission::ACCEPTED);
        if (word)
            CHECK(L::noteActionTransmission(c, live, live.target, r.now));
        r.steps(2);
        const O::Completion* terminal = r.owner.result(id);
        CHECK(terminal);
        const C::SdoCompletion copy = terminal->service;
        CHECK(r.owner.release(id));
        CHECK(L::advanceAction(c, live, live.target, copy, r.now));
        return true;
    }
};
L::ActionContext Direct::c;
void directLifecycle() {
    {
        Direct d;
        d.start();
        while (d.once()) {
        }
        CHECK(Direct::c.state == M::ActionState::SUCCEEDED);
        CHECK(d.r.drive.status == ((POWER_UP_STATUS & 0xfff8u) | 7u));
    }
    {   // Terminals that do not match are refused atomically, in every phase.
        Direct d;
        d.start();
        const auto refused = [&]() {
            Saved<L::ActionContext> saved(Direct::c);
            C::SdoCompletion wrong;
            wrong.transfer.index = 0x1234;
            CHECK(L::advanceAction(Direct::c, d.live, d.live.target, wrong, d.r.now).detail ==
                  int32_t(M::ActionError::CORRELATION));
            L::PreparedAction work;
            CHECK(L::nextAction(Direct::c, d.live, d.live.target, d.r.now - 1, work).detail ==
                  int32_t(M::ActionError::STATE));
            CHECK(L::advanceAction(Direct::c, d.live, d.live.target, M::ActionOutcome::OBSERVED, d.r.now).detail ==
                  int32_t(M::ActionError::STATE));
            CHECK(L::advanceAction(Direct::c, d.live, d.live.target, M::ActionOutcome::DEADLINE, d.r.now).detail ==
                  int32_t(M::ActionError::STATE));
            saved.unchanged(Direct::c);
        };
        refused();
        CHECK(d.once());
        refused();
        CHECK(d.once());
        refused(); // Now at word 6.
        CHECK(d.once());
        refused(); // Now observing word 6.
        // A second transmission notice for the same word is refused.
        while (Direct::c.phase != L::ActionPhase::COMMAND)
            CHECK(d.once());
        while (d.r.now < Direct::c.eligibleUs)
            d.r.step();
        CHECK(L::noteActionTransmission(Direct::c, d.live, d.live.target, d.r.now));
        Saved<L::ActionContext> saved(Direct::c);
        CHECK(L::noteActionTransmission(Direct::c, d.live, d.live.target, d.r.now).detail ==
              int32_t(M::ActionError::STATE));
        saved.unchanged(Direct::c);
    }
    {   // A notice before eligibility is refused: the interval cannot be skipped by calling early.
        Direct d;
        d.start();
        while (d.once() && !(Direct::c.phase == L::ActionPhase::COMMAND && Direct::c.commandCursor == 1)) {
        }
        CHECK(Direct::c.eligibleUs > d.r.now);
        L::PreparedAction work;
        CHECK(L::nextAction(Direct::c, d.live, d.live.target, d.r.now, work));
        CHECK(!work.ready && work.eligibleUs == Direct::c.eligibleUs);
        CHECK(L::noteActionTransmission(Direct::c, d.live, d.live.target, d.r.now).detail ==
              int32_t(M::ActionError::STATE));
    }
    {   // Identity limit: work needing another identity fails, and no later word goes out.
        Direct d;
        d.start();
        while (d.once() && Direct::c.commandEvidenceMask != 1) {
        }
        Direct::c.step = UINT32_MAX;
        while (Direct::c.state == M::ActionState::ACTIVE && d.once()) {
        }
        CHECK(Direct::c.state == M::ActionState::FAILED);
        CHECK(Direct::c.outcome == M::ActionOutcome::ITERATION_EXHAUSTED);
        CHECK(wordsFrom(d.r, 0) == 1);
    }
    {   // On-time receipt processed late, and first receipt after expiry: both are DEADLINE.
        for (unsigned onTime = 0; onTime < 2; ++onTime) {
            Direct d;
            d.lead = 140000;
            d.start();
            CHECK(d.once());
            CHECK(d.once());
            L::PreparedAction work;
            CHECK(L::nextAction(Direct::c, d.live, d.live.target, d.r.now, work) && work.ready);
            O::BusRequest request;
            request.sequence = d.r.a.sequence();
            request.transfer = work.transfer;
            O::RequestId id;
            CHECK(d.r.owner.admit(request, d.r.now, id) == O::BusAdmission::ACCEPTED);
            CHECK(L::noteActionTransmission(Direct::c, d.live, d.live.target, d.r.now));
            if (onTime)
                d.r.steps(2);
            else
                d.r.drive.answer = false;
            while (!d.r.owner.result(id))
                d.r.step();
            const C::SdoCompletion copy = d.r.owner.result(id)->service;
            const uint64_t after = Direct::c.deadlineUs + 7 > d.r.now ? Direct::c.deadlineUs + 7 : d.r.now;
            CHECK(L::advanceAction(Direct::c, d.live, d.live.target, copy, after));
            CHECK(Direct::c.outcome == M::ActionOutcome::DEADLINE);
            CHECK(Direct::c.effectsPossible);
            if (onTime) // The on-time acknowledgement is kept even though the action failed.
                CHECK(Direct::c.execution == M::ActionExecution::ACKNOWLEDGED);
        }
    }
    {   // A local reason at the cutoff becomes DEADLINE, and an earlier terminal never moves.
        Direct d;
        d.start();
        CHECK(L::advanceAction(Direct::c, d.live, d.live.target, M::ActionOutcome::CANCELLED, Direct::c.deadlineUs));
        CHECK(Direct::c.outcome == M::ActionOutcome::DEADLINE);
        Saved<L::ActionContext> saved(Direct::c);
        CHECK(L::advanceAction(Direct::c, d.live, d.live.target, M::ActionOutcome::CANCELLED, d.r.now + 500000).detail ==
              int32_t(M::ActionError::STATE));
        saved.unchanged(Direct::c);
        L::PreparedAction work;
        CHECK(L::nextAction(Direct::c, d.live, d.live.target, d.r.now + 500000, work) && !work.ready);
    }
    {   // An independently routed fault is DRIVE_FAULT, and a changed route is INVALIDATED.
        for (unsigned route = 0; route < 2; ++route) {
            Direct d;
            d.start();
            CHECK(d.once());
            CHECK(d.once());
            CHECK(d.once());
            L::ReadTarget stale = d.live.target;
            ++stale.nodeGeneration;
            if (route)
                CHECK(L::advanceAction(Direct::c, d.live, stale, M::ActionOutcome::INVALIDATED, d.r.now));
            else
                CHECK(L::advanceAction(Direct::c, d.live, d.live.target, M::ActionOutcome::DRIVE_FAULT, d.r.now));
            CHECK(Direct::c.outcome == (route ? M::ActionOutcome::INVALIDATED : M::ActionOutcome::DRIVE_FAULT));
            CHECK(Direct::c.uncertain && Direct::c.execution == M::ActionExecution::ACKNOWLEDGED);
        }
    }
    {   // A withdrawal recorded with a word in flight: nothing new, and the terminal ends it.
        Direct d;
        d.start();
        CHECK(d.once());
        CHECK(d.once());
        L::PreparedAction work;
        CHECK(L::nextAction(Direct::c, d.live, d.live.target, d.r.now, work) && work.ready);
        O::BusRequest request;
        request.sequence = d.r.a.sequence();
        request.transfer = work.transfer;
        O::RequestId id;
        CHECK(d.r.owner.admit(request, d.r.now, id) == O::BusAdmission::ACCEPTED);
        CHECK(L::noteActionTransmission(Direct::c, d.live, d.live.target, d.r.now));
        Saved<L::ActionContext> before(Direct::c);
        CHECK(L::noteActionWithdrawal(Direct::c, d.live, d.live.target, M::ActionOutcome::OBSERVED, d.r.now)
                  .detail == int32_t(M::ActionError::STATE));
        before.unchanged(Direct::c);
        CHECK(L::noteActionWithdrawal(Direct::c, d.live, d.live.target, M::ActionOutcome::DRIVE_FAULT, d.r.now));
        CHECK(L::noteActionWithdrawal(Direct::c, d.live, d.live.target, M::ActionOutcome::CANCELLED, d.r.now)
                  .detail == int32_t(M::ActionError::STATE)); // The first reason stays.
        CHECK(L::nextAction(Direct::c, d.live, d.live.target, d.r.now, work) && !work.ready);
        d.r.steps(2);
        const C::SdoCompletion copy = d.r.owner.result(id)->service;
        CHECK(d.r.owner.release(id));
        CHECK(L::advanceAction(Direct::c, d.live, d.live.target, copy, d.r.now));
        CHECK(Direct::c.state == M::ActionState::FAILED && Direct::c.outcome == M::ActionOutcome::DRIVE_FAULT);
        CHECK(Direct::c.commandHistory[0].outcome == C::SdoOutcome::SUCCESS &&
              Direct::c.execution == M::ActionExecution::ACKNOWLEDGED && Direct::c.uncertain);
        CHECK(Direct::c.phase == L::ActionPhase::COMMAND); // Acknowledged, never observed.
    }
    {   // A context never prepared yields nothing and accepts nothing.
        Rig r;
        r.open();
        L::ReadContext state;
        r.readState(state);
        const L::AxisReservation live = reservationOf(r);
        L::resetAction(Direct::c);
        L::PreparedAction work;
        CHECK(L::nextAction(Direct::c, live, live.target, r.now, work).detail == int32_t(M::ActionError::STATE));
    }
}

// 12. EnableStart::RELEASED_BY_RECORDED_STOP: the released standstill in mode 1 a recorded stop left, with
// the drive kept powered. The bytes follow the release measured on one firmware (8050 with 2100=0011,
// then 8031, 8033 with 2100 released clear and the position report moved, 8037); the mode is the fake's.
const uint16_t RELEASED_STATUS = 0x8050;
const uint16_t RELEASED_MONITOR = uint16_t(L::MONITOR_POSITION_COMPLETE | L::MONITOR_RELEASED);
void releasedDrive(FakeDrive& d, int32_t position = 15256) {
    d.status = RELEASED_STATUS;
    d.monitor = RELEASED_MONITOR;
    d.mode = 1;
    d.position = position;
    d.velocityTarget = 6;
    d.releasedPattern = true;
}
L::EnableEvidence releasedEvidence(uint16_t status = RELEASED_STATUS) {
    L::EnableEvidence e = evidence(status, 5000, 6);
    e.start = L::EnableStart::RELEASED_BY_RECORDED_STOP;
    e.powerUpShaft = L::ShaftObservation::NONE; // Not consulted: the start report reads released.
    return e;
}
void releasedByRecordedStop() {
    {   // The whole sequence: word 7 moves the position report by +5 and the enable still succeeds.
        Rig r;
        releasedDrive(r.drive);
        r.drive.changeAfterWord = 2;
        r.drive.positionChange = 5;
        L::ReadContext state;
        admittedRig(r, state);
        CHECK(r.request(state, releasedEvidence()).isOk());
        r.finishEnable();
        const L::ActionContext& c = r.a.enableAction();
        CHECK(c.state == M::ActionState::SUCCEEDED && c.outcome == M::ActionOutcome::OBSERVED);
        CHECK(c.execution == M::ActionExecution::ACKNOWLEDGED && !c.uncertain && c.commandEvidenceMask == 7);
        CHECK(c.enable.evidence.start == L::EnableStart::RELEASED_BY_RECORDED_STOP);
        CHECK(c.enable.startStatusword == RELEASED_STATUS && c.enable.startMonitor == RELEASED_MONITOR);
        CHECK(c.enable.startPosition == 15256); // Before word 6.
        CHECK(c.enable.steps[0].statusword == 0x8031 && c.enable.steps[0].monitor == RELEASED_MONITOR &&
              c.enable.steps[0].position == 15256);
        CHECK(c.enable.steps[1].statusword == 0x8033 && c.enable.steps[1].monitor == 1 &&
              c.enable.steps[1].position == 15261); // After word 7: kept as reported, not judged.
        CHECK(c.enable.steps[2].statusword == 0x8037 && c.enable.steps[2].monitor == 1 &&
              c.enable.steps[2].position == 15261 && c.enable.steps[2].velocity == 0);
        unsigned i = r.startFrame;
        CHECK(uploadAt(r, i, 0x607a) && uploadAt(r, i + 1, 0x60ff) && wordAt(r, i + 2, 6));
        CHECK(wordsFrom(r, r.startFrame) == 3 && downloadsFrom(r, r.startFrame) == 3 && r.drive.mode == 1);
        CHECK(r.a.latestControl() && r.a.latestControl()->transfer.data[0] == 0x0f);
        quiet(r);
    }
    {   // Word 7's report that moves on: word 15 waits until two cycles in a row repeat one 6064, and word
        // 7's step keeps the second of them (+5 at the word, +2 more on its second cycle).
        Rig r;
        releasedDrive(r.drive, -40);
        r.drive.changeAfterWord = 2;
        r.drive.positionChange = 5;
        r.drive.driftWord = 2;
        r.drive.driftRead = 2;
        r.drive.driftChange = 2;
        L::ReadContext state;
        admittedRig(r, state);
        CHECK(r.request(state, releasedEvidence()).isOk());
        r.finishEnable();
        const L::ActionContext& c = r.a.enableAction();
        CHECK(c.state == M::ActionState::SUCCEEDED && c.enable.startPosition == -40 &&
              c.enable.steps[0].position == -40 && c.enable.steps[1].position == -33 &&
              c.enable.steps[2].position == -33);
        // Word 6's cycle, word 7's three (-35, then -33 twice, the accepted one at least two intervals after
        // the word) and word 15's.
        CHECK(c.observations == 5 &&
              c.enable.steps[1].earliestUs >= c.commandHistory[1].completedUs + 2 * options().observationIntervalUs);
        quiet(r);
    }
    {   // A position report that moves after word 15 ends the sequence, which tells the caller to stop:
        // the drive may be enabled and moving to its retained target. The acknowledged 000F is the latest
        // word, and the enable's own last cycle (low bits 111, not released) admits the QUICK_STOP.
        const int32_t changes[][2] = {{-3, 0}, {1, 0}, {0, 30}, {0, -1}};
        for (const auto& k : changes) {
            Rig r;
            releasedDrive(r.drive, -40);
            r.drive.changeAfterWord = 3;
            r.drive.positionChange = k[0];
            r.drive.velocityChange = k[1];
            L::ReadContext state;
            admittedRig(r, state);
            CHECK(r.request(state, releasedEvidence()).isOk());
            r.finishEnable();
            const L::ActionContext& c = r.a.enableAction();
            CHECK(c.state == M::ActionState::FAILED && c.outcome == M::ActionOutcome::OBSERVATION_SCOPE);
            CHECK(std::strstr(c.status.msg, "after word 15") && std::strstr(c.status.msg, "stop it"));
            CHECK(c.commandEvidenceMask == 7 && c.enable.steps[1].present && !c.enable.steps[2].present);
            CHECK(c.extraObservation.actualPosition == -40 + k[0] && c.extraObservation.actualVelocity == k[1]);
            CHECK(r.a.latestControl() && r.a.latestControl()->transfer.data[0] == 0x0f);
            E::StopPolicy policy;
            policy.options = options();
            policy.deadlineUs = r.now + 20000;
            const unsigned from = r.wire.sentCount;
            CHECK(r.a.requestStop(L::StopKind::QUICK_STOP, c.read, policy, r.now) == E::StopPath::CONFIRMED);
            r.steps(4);
            CHECK(r.wire.sentCount > from && wordAt(r, from, 0x000b));
        }
    }
    {   // NO_MOTION_WRITE_RECORDED keeps its rules on the same start: bits 5 and 6 end it at word 6.
        Rig r;
        releasedDrive(r.drive);
        L::ReadContext state;
        admittedRig(r, state);
        L::EnableEvidence e = releasedEvidence();
        e.start = L::EnableStart::NO_MOTION_WRITE_RECORDED;
        CHECK(r.request(state, e).isOk());
        r.finishEnable();
        const L::ActionContext& c = r.a.enableAction();
        CHECK(c.state == M::ActionState::FAILED && c.outcome == M::ActionOutcome::OBSERVATION_SCOPE);
        CHECK(wordsFrom(r, r.startFrame) == 1 && c.enable.steps[0].present == false);
    }
    {   // And its position rule: the same word 7 move ends a power-up start at word 7.
        Rig r;
        r.drive.changeAfterWord = 2;
        r.drive.positionChange = 5;
        L::ReadContext state;
        admittedRig(r, state);
        CHECK(r.request(state).isOk());
        r.finishEnable();
        const L::ActionContext& c = r.a.enableAction();
        CHECK(c.state == M::ActionState::FAILED && c.outcome == M::ActionOutcome::OBSERVATION_SCOPE);
        CHECK(wordsFrom(r, r.startFrame) == 2 && c.enable.steps[0].present && !c.enable.steps[1].present);
    }
    // Word 6's cycle is still released, so its 6064 must repeat the admission; every other rule holds at
    // each word: other 6041 bits, 2100 bits, 606C, mode.
    struct Change {
        unsigned word;
        uint16_t monitor, status;
        int32_t position, velocity;
        int mode;
    };
    const Change changes[] = {
        {1, 0, 0, 1, 0, -1},                    // Position moved while released.
        {1, 0, 0, -1, 0, -1},
        {1, 0, 0x0010, 0, 0, -1},               // 6041 bit 4.
        {2, 0, 0x0008, 0, 0, -1},               // Bit 3, an alarm in ZH86.
        {2, 0, 0x4000, 0, 0, -1},               // Bit 14, running in ZH86 and OP86.
        {3, 0, 0x8000, 0, 0, -1},               // Bit 15.
        {2, L::MONITOR_RUNNING, 0, 0, 0, -1},   // Running while the report moves.
        {2, 0, 0, 0, 1, -1},                    // 606C nonzero after word 7.
        {3, 0, 0, 0, -1, -1},
        {2, L::MONITOR_ALARM, 0, 0, 0, -1},
        {3, L::MONITOR_POSITION_COMPLETE, 0, 0, 0, -1},
        {2, 0, 0, 0, 0, 3},                     // Mode.
    };
    for (const Change& k : changes) {
        Rig r;
        releasedDrive(r.drive);
        r.drive.changeAfterWord = k.word;
        r.drive.monitorChange = k.monitor;
        r.drive.statusChange = k.status;
        r.drive.positionChange = k.position;
        r.drive.velocityChange = k.velocity;
        r.drive.modeChange = k.mode;
        L::ReadContext state;
        admittedRig(r, state);
        CHECK(r.request(state, releasedEvidence()).isOk());
        r.finishEnable();
        const L::ActionContext& c = r.a.enableAction();
        CHECK(c.state == M::ActionState::FAILED && c.outcome == M::ActionOutcome::OBSERVATION_SCOPE);
        CHECK(wordsFrom(r, r.startFrame) == k.word && c.commandEvidenceMask == uint8_t((1u << k.word) - 1u));
        quiet(r);
    }
    // Refused before any traffic: the gate, and every start report but the released standstill in mode 1.
    struct Start {
        uint16_t monitor, status;
        uint8_t mode;
        int32_t velocity;
        uint16_t errorCode;
    };
    const uint16_t latched = uint16_t(L::MONITOR_RELEASED | L::MONITOR_RUNNING); // 0014, trial 2109.
    const Start starts[] = {
        {1, POWER_UP_STATUS, 1, 0, 0},                                            // Enabled report.
        {1, RELEASED_STATUS, 1, 0, 0},                                            // 2100 not released.
        {uint16_t(1 | L::MONITOR_RUNNING), 0xc050, 1, 0, 0},                      // Running, not released.
        {RELEASED_MONITOR, 0x8051, 1, 0, 0},                                      // Low bits 001.
        {RELEASED_MONITOR, 0x8053, 1, 0, 0},
        {RELEASED_MONITOR, 0x8057, 1, 0, 0},
        {RELEASED_MONITOR, 0x8040, 1, 0, 0},                                      // Not the 0x50 pattern.
        {latched, 0x4070, 1, 0, 0},
        {latched, 0x4058, 1, 0, 0},                                               // Bit 3, an alarm in ZH86.
        {uint16_t(RELEASED_MONITOR | L::MONITOR_ALARM), RELEASED_STATUS, 1, 0, 0}, // Alarm.
        {uint16_t(latched | L::MONITOR_ALARM), 0x4050, 1, 0, 0},
        {uint16_t(latched | L::MONITOR_POSITIVE_SOFT_LIMIT), 0x4050, 1, 0, 0},     // Soft limits.
        {uint16_t(RELEASED_MONITOR | L::MONITOR_NEGATIVE_SOFT_LIMIT), RELEASED_STATUS, 1, 0, 0},
        {uint16_t(latched | 0x0080), 0x4050, 1, 0, 0},                            // Undocumented bit.
        {RELEASED_MONITOR, RELEASED_STATUS, 1, 1, 0},                             // Moving.
        {RELEASED_MONITOR, RELEASED_STATUS, 1, -1, 0},
        {latched, 0x4050, 1, -30, 0},                                             // 606C nonzero, latched.
        {RELEASED_MONITOR, RELEASED_STATUS, 3, 0, 0},                             // Mode 3.
        {uint16_t(latched | 1), 0xc050, 3, 0, 0},                                 // 1924's report, mode 3.
        {RELEASED_MONITOR, RELEASED_STATUS, 0, 0, 0},
        {RELEASED_MONITOR, RELEASED_STATUS, 1, 0, 0xff05},
    };
    for (const Start& k : starts) {
        Rig r;
        releasedDrive(r.drive);
        r.drive.monitor = k.monitor;
        r.drive.status = k.status;
        r.drive.mode = k.mode;
        r.drive.velocity = k.velocity;
        r.drive.errorCode = k.errorCode;
        L::ReadContext state;
        admittedRig(r, state);
        L::EnableEvidence e = releasedEvidence(k.status);
        e.powerUpShaft = L::ShaftObservation::HOLDING; // No observation admits a start outside the rule.
        const M::Status status = r.request(state, e);
        CHECK(status.code == M::Err::INVALID_CONFIG && status.detail == int32_t(M::ActionError::STATE));
        CHECK(r.a.enableAction().state == M::ActionState::EMPTY && r.wire.sentCount == r.startFrame);
        quiet(r, 4);
    }
    {   // A closed gate refuses first; mode 6 refuses even with its own homing evidence.
        Rig r;
        releasedDrive(r.drive);
        L::ReadContext state;
        admittedRig(r, state);
        L::EnableEvidence closed = releasedEvidence();
        closed.gate = L::EnableGate::CLOSED;
        CHECK(r.request(state, closed).code == M::Err::UNSUPPORTED);
        Rig h;
        releasedDrive(h.drive);
        h.drive.mode = 6;
        h.drive.monitor = L::MONITOR_RELEASED;
        L::ReadContext homing;
        admittedRig(h, homing);
        L::EnableEvidence six = releasedEvidence();
        six.modeRaw = 6;
        six.homingEnableQualified = true;
        CHECK(h.request(homing, six).detail == int32_t(M::ActionError::STATE));
        six.start = L::EnableStart::NO_MOTION_WRITE_RECORDED; // The mode 6 start this report does admit.
        CHECK(h.request(homing, six).isOk());
    }
    {   // The session's own record still decides: a write of this object refuses, a 6060=1 does not.
        Rig r;
        releasedDrive(r.drive);
        r.drive.mode = 3;
        L::ReadContext state;
        r.open();
        r.writeThrough(0x6060, 1, 1);
        CHECK(r.drive.mode == 1 && !r.a.motionHistoryWritten());
        r.readState(state);
        CHECK(r.request(state, releasedEvidence()).isOk());
        r.finishEnable();
        CHECK(r.a.enableAction().state == M::ActionState::SUCCEEDED);
        Rig w;
        releasedDrive(w.drive);
        w.open();
        w.writeThrough(0x607a, 2, 1);
        L::ReadContext after;
        w.readState(after);
        CHECK(w.a.motionHistoryWritten() && w.request(after, releasedEvidence()).detail == int32_t(M::ActionError::STATE));
        CHECK(w.a.enableAction().state == M::ActionState::EMPTY);
    }
}

// 12b. The latched release a moving quick stop leaves, from the captured reports: 2109 (node 1 after the
// quick stop of move 2108: 2100=0014, 6041=4050, mode 1, 6064 -2017, 606C 0; 607A -2000, the move's relative
// displacement from -1522, so its internal target was -3522; 60FF 45) and 1924
// (node 2 after a halted-state quick stop: 0015, C050, mode 3, 33743). What the words then do is this
// fake's, after trials 308 and 1742; the +11 word 7 pull is this test's number.
const uint16_t LATCHED_STATUS = 0x4050, LATCHED_MONITOR = uint16_t(L::MONITOR_RELEASED | L::MONITOR_RUNNING);
void latchedDrive(FakeDrive& d) {
    d.status = LATCHED_STATUS;
    d.monitor = LATCHED_MONITOR;
    d.mode = 1;
    d.position = -2017;
    d.positionTarget = -2000;
    d.velocityTarget = 45;
    d.releasedPattern = true;
    d.unlatchWord = 7;
    d.changeAfterWord = 2; // Word 7's pull.
    d.positionChange = 11;
}
L::EnableEvidence latchedEvidence(uint16_t status = LATCHED_STATUS) {
    L::EnableEvidence e = evidence(status, -2000, 45);
    e.start = L::EnableStart::RELEASED_BY_RECORDED_STOP;
    e.powerUpShaft = L::ShaftObservation::NONE;
    return e;
}
void latchedRelease() {
    {   // 2109's report: word 7 clears the latch, its report holds still, and word 15 completes the enable.
        Rig r;
        latchedDrive(r.drive);
        L::ReadContext state;
        admittedRig(r, state);
        CHECK(r.request(state, latchedEvidence()).isOk());
        r.finishEnable();
        const L::ActionContext& c = r.a.enableAction();
        CHECK(c.state == M::ActionState::SUCCEEDED && c.outcome == M::ActionOutcome::OBSERVED && !c.uncertain);
        CHECK(c.enable.startStatusword == 0x4050 && c.enable.startMonitor == 0x0014 &&
              c.enable.startPosition == -2017 && c.enable.positionTarget == -2000);
        // Word 6: the latch persists, still released, at the start's 6064.
        CHECK(c.enable.steps[0].statusword == 0x4031 && c.enable.steps[0].monitor == 0x0014 &&
              c.enable.steps[0].position == -2017);
        // Word 7: 0033/0000 as in trial 308, the latch gone, the report pulled and held for two cycles.
        CHECK(c.enable.steps[1].statusword == 0x0033 && c.enable.steps[1].monitor == 0 &&
              c.enable.steps[1].position == -2006);
        CHECK(c.enable.steps[2].statusword == 0x0037 && c.enable.steps[2].monitor == 0 &&
              c.enable.steps[2].position == -2006 && c.enable.steps[2].velocity == 0);
        CHECK(wordsFrom(r, r.startFrame) == 3 && downloadsFrom(r, r.startFrame) == 3 && r.drive.mode == 1);
        CHECK(r.drive.positionTarget == -2000); // Read and compared, never written.
        quiet(r);
    }
    {   // The latch may also clear in word 6's cycle.
        Rig r;
        latchedDrive(r.drive);
        r.drive.unlatchWord = 6;
        L::ReadContext state;
        admittedRig(r, state);
        CHECK(r.request(state, latchedEvidence()).isOk());
        r.finishEnable();
        const L::ActionContext& c = r.a.enableAction();
        CHECK(c.state == M::ActionState::SUCCEEDED && c.enable.steps[0].monitor == L::MONITOR_RELEASED &&
              c.enable.steps[0].statusword == 0x0031);
    }
    {   // 1924's report in mode 3 is refused for the enable, before any traffic; after the one 6060=1 the
        // same latched release (in place too) is admitted, and word 7 gives 8033/0001 as in trial 1742.
        Rig r;
        latchedDrive(r.drive);
        r.drive.status = 0xc050;
        r.drive.monitor = uint16_t(LATCHED_MONITOR | L::MONITOR_POSITION_COMPLETE);
        r.drive.mode = 3;
        r.drive.position = 33743;
        r.drive.changeAfterWord = 0;
        L::ReadContext state;
        admittedRig(r, state);
        L::EnableEvidence three = latchedEvidence(0xc050);
        three.modeRaw = 3;
        CHECK(r.request(state, three).detail == int32_t(M::ActionError::ARGUMENT)); // Mode 3: no enable here.
        const M::Status reviewed = r.request(state, latchedEvidence(0xc050)); // Mode 1 reviewed, 3 reported.
        CHECK(reviewed.code == M::Err::INVALID_CONFIG && reviewed.detail == int32_t(M::ActionError::STATE));
        CHECK(r.a.enableAction().state == M::ActionState::EMPTY && r.wire.sentCount == r.startFrame);
        r.writeThrough(0x6060, 1, 1);
        CHECK(r.drive.mode == 1 && !r.a.motionHistoryWritten());
        r.readState(state);
        CHECK(r.request(state, latchedEvidence(0xc050)).isOk());
        r.finishEnable();
        const L::ActionContext& c = r.a.enableAction();
        CHECK(c.state == M::ActionState::SUCCEEDED && c.enable.startMonitor == 0x0015);
        CHECK(c.enable.steps[0].statusword == 0xc031 && c.enable.steps[0].monitor == 0x0015);
        CHECK(c.enable.steps[1].statusword == 0x8033 && c.enable.steps[1].monitor == 0x0001 &&
              c.enable.steps[1].position == 33743);
        CHECK(c.enable.steps[2].statusword == 0x8037 && c.enable.steps[2].monitor == 0x0001);
    }
    {   // The latch kept at word 7: the sequence ends in word 7's cycle, word 15 unsent, with that cycle kept.
        // The drive is then switched on, reporting running. Nothing in the library stops it; a stop request
        // after word 7 has no sequenced arrow and takes the best-effort 000B only when the policy allows it.
        for (unsigned statusOnly = 0; statusOnly < 2; ++statusOnly) {
            Rig r;
            latchedDrive(r.drive);
            if (statusOnly)
                r.drive.unlatchStatus = false; // 2100 running clears, 6041 bit 14 stays.
            else
                r.drive.unlatchWord = 0;
            L::ReadContext state;
            admittedRig(r, state);
            CHECK(r.request(state, latchedEvidence()).isOk());
            r.finishEnable();
            const L::ActionContext& c = r.a.enableAction();
            CHECK(c.state == M::ActionState::FAILED && c.outcome == M::ActionOutcome::OBSERVATION_SCOPE);
            CHECK(!std::strstr(c.status.msg, "after word 15"));
            CHECK(wordsFrom(r, r.startFrame) == 2 && c.commandEvidenceMask == 3 && c.enable.steps[0].present &&
                  !c.enable.steps[1].present);
            CHECK(c.statusObservation.statusword == 0x4033 &&
                  c.monitorObservation.monitor == (statusOnly ? 0 : L::MONITOR_RUNNING) &&
                  c.extraObservation.actualPosition == -2006);
            if (statusOnly)
                continue;
            CHECK(r.a.latestControl() && r.a.latestControl()->transfer.data[0] == 7);
            E::StopPolicy policy;
            policy.options = options();
            policy.deadlineUs = r.now + 20000;
            CHECK(r.a.requestStop(L::StopKind::QUICK_STOP, c.read, policy, r.now) == E::StopPath::NONE);
            quiet(r, 4);
            policy.deadlineUs = r.now + 20000;
            policy.bestEffortDeadlineUs = r.now + 30000;
            const unsigned from = r.wire.sentCount;
            CHECK(r.a.requestStop(L::StopKind::QUICK_STOP, c.read, policy, r.now) == E::StopPath::BEST_EFFORT);
            r.steps(4);
            CHECK(r.wire.sentCount == from + 1 && wordAt(r, from, 0x000b));
        }
    }
    {   // Motion after word 15 toward the interrupted move's -3522: the enable fails and tells the caller to stop.
        Rig r;
        latchedDrive(r.drive);
        r.drive.driftWord = 3;
        r.drive.driftRead = 1;
        r.drive.driftChange = -40;
        L::ReadContext state;
        admittedRig(r, state);
        CHECK(r.request(state, latchedEvidence()).isOk());
        r.finishEnable();
        const L::ActionContext& c = r.a.enableAction();
        CHECK(c.state == M::ActionState::FAILED && c.outcome == M::ActionOutcome::OBSERVATION_SCOPE);
        CHECK(std::strstr(c.status.msg, "after word 15") && c.enable.steps[1].position == -2006 &&
              c.extraObservation.actualPosition == -2046 && c.commandEvidenceMask == 7);
        E::StopPolicy policy;
        policy.options = options();
        policy.deadlineUs = r.now + 20000;
        const unsigned from = r.wire.sentCount;
        CHECK(r.a.requestStop(L::StopKind::QUICK_STOP, c.read, policy, r.now) == E::StopPath::CONFIRMED);
        r.steps(4);
        CHECK(wordAt(r, from, 0x000b));
    }
    // Every other change still ends a latched start, each at its word: 6064 moving or 606C nonzero in
    // word 6's cycle, the in-place bit appearing, an alarm, a soft limit, a latch that comes back.
    struct Change {
        unsigned word;
        uint16_t monitor, status;
        int32_t position, velocity;
    };
    const Change changes[] = {
        {1, 0, 0, 1, 0},
        {1, 0, 0, -1, 0},
        {1, 0, 0, 0, 1},
        {2, 0, 0, 0, -1},                                   // 606C during word 7's pull.
        {2, L::MONITOR_POSITION_COMPLETE, 0, 0, 0},
        {3, L::MONITOR_POSITION_COMPLETE, 0, 0, 0},         // In place after word 15: a target reached.
        {2, L::MONITOR_ALARM, 0, 0, 0},
        {1, L::MONITOR_NEGATIVE_SOFT_LIMIT, 0, 0, 0},
        {3, L::MONITOR_RUNNING, 0, 0, 0},                   // Running again after word 15.
        {3, 0, 0x4000, 0, 0},
        {1, 0, 0x8000, 0, 0},
    };
    for (const Change& k : changes) {
        Rig r;
        latchedDrive(r.drive);
        r.drive.changeAfterWord = k.word;
        r.drive.monitorChange = k.monitor;
        r.drive.statusChange = k.status;
        r.drive.positionChange = k.position;
        r.drive.velocityChange = k.velocity;
        L::ReadContext state;
        admittedRig(r, state);
        CHECK(r.request(state, latchedEvidence()).isOk());
        r.finishEnable();
        const L::ActionContext& c = r.a.enableAction();
        CHECK(c.state == M::ActionState::FAILED && c.outcome == M::ActionOutcome::OBSERVATION_SCOPE);
        CHECK(wordsFrom(r, r.startFrame) == k.word && c.commandEvidenceMask == uint8_t((1u << k.word) - 1u));
        quiet(r);
    }
}

#define RUN(name)                                                                                  \
    do {                                                                                           \
        scenario = #name;                                                                          \
        name();                                                                                    \
    } while (false)
void allScenarios() {
    RUN(goldenSequence);
    RUN(minimumDeadline);
    RUN(refusalsAreAtomic);
    RUN(wrongStartRefuses);
    RUN(targetsDecideBeforeAnyWord);
    RUN(zeroSpeedBitFollowsTheEnable);
    RUN(outOfScopeAtEachStep);
    RUN(wordFailuresAtEachStep);
    RUN(delaysAndPolling);
    RUN(cancellationAndDeadline);
    RUN(lifecycleLoss);
    RUN(stopAfterEachStep);
    RUN(sessionHistory);
    RUN(releasedByRecordedStop);
    RUN(latchedRelease);
    RUN(directLifecycle);
}
} // namespace

int main() {
    allScenarios();
    asyncTraffic = true;
    allScenarios();
    std::printf("enable action: %u checks\n", checks);
    return 0;
}
