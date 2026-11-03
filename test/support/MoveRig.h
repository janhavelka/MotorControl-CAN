// SPDX-License-Identifier: MIT
// The move suites' shared rig: the real owner, the selected SDO engine, AxisSession and a fake server
// on node 1, with an optional second node. Every word, status, monitor, mode, identity, target and
// feedback value here is manufactured by this fake, and so are the acceptance and completion masks:
// 6041 bit 12 and bit 15 are documentary candidates whose behavior on any specimen is unmeasured, and
// the fake's relative base is a switch, not a measurement. Nothing here shows that a drive accepts,
// moves or completes a target. Define `checks`, `asyncTraffic`, `scenario` and CHECK before including.
#pragma once
#ifndef CHECK
#error "Define the suite's checks counter and CHECK macro before including MoveRig.h"
#endif
#include "AxisSession.h"
#include <MotorControlCAN/profiles/lichuan_c/State.h>
#include <cstring>
#include "OwnerRig.h"

namespace MoveRig {
namespace M = MotorControlCAN;
namespace C = MotorControlCAN::CANOPEN;
namespace L = MotorControlCAN::LICHUAN_C;
namespace E = MotorControlCANExample;
namespace O = MotorControlCANOwner;
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
inline uint8_t bit(L::StateBlock block) { return uint8_t(1u << unsigned(block)); }
const uint8_t STATE_MASK = uint8_t(bit(L::StateBlock::MONITORS) | bit(L::StateBlock::STATUS));
const uint8_t ENABLE_MASK = uint8_t(bit(L::StateBlock::ERRORS) | bit(L::StateBlock::MONITORS) |
                                    bit(L::StateBlock::STATUS) | bit(L::StateBlock::FEEDBACK));
inline L::ReadTarget target(unsigned node = 1) {
    L::ReadTarget value;
    value.channel = OwnerRig::route(node);
    value.nodeGeneration = 11;
    return value;
}
inline L::ReadOptions reads() {
    L::ReadOptions value;
    value.queueTimeoutUs = QUEUE;
    value.responseTimeoutUs = RESPONSE;
    return value;
}
inline L::MoveOptions options() {
    L::MoveOptions value;
    value.service = reads();
    value.observationIntervalUs = 1000;
    value.maximumAgeUs = uint64_t(QUEUE + uint64_t(RESPONSE)) * 9;
    return value;
}
inline M::AxisConfig axisConfig(unsigned node = 1) {
    M::AxisConfig c;
    c.binding = OwnerRig::route(node).binding;
    c.units.commandStepsPerMotorTurn = M::UnitScale(3200, 1, M::ScaleSource::QUALIFIED);
    c.units.motorTurnsPerLoadTurn = M::UnitScale(5, 1, M::ScaleSource::QUALIFIED);
    c.nativeRateBasis = M::NativeRateBasis::MOTOR_TURNS;
    c.nativeVelocityTime = M::TimeUnit::MINUTE;
    c.supportedRelativeBases = 1;
    return c;
}
inline M::MoveRequest moveRequest(int64_t targetSteps = TARGET, int64_t rpm = 120) {
    M::MoveRequest value;
    value.position.value = M::Rational(targetSteps);
    value.position.configurationGeneration = OwnerRig::route(1).binding.configurationGeneration;
    value.speed.value = M::Rational(rpm);
    value.speed.unit = M::VelocityUnit(M::PositionUnit::TURNS, M::TimeUnit::MINUTE);
    value.speed.configurationGeneration = value.position.configurationGeneration;
    return value;
}
inline L::MoveProfile profile(uint32_t up = 100, uint32_t down = 200) {
    L::MoveProfile value;
    value.rampUpTimeMs = up;
    value.rampDownTimeMs = down;
    return value;
}
// The fake identity. Raw codes only; nothing here is a real model or firmware.
const uint32_t DEVICE_TYPE = 0x00040192, VENDOR = 0x00000300, PRODUCT = 0x00008605, REVISION = 0x00010001;
const uint16_t HARDWARE = 0x0102, SOFTWARE = 0x0203, MODEL = 0x0086, FIRMWARE = 0x0123;
inline L::MoveEvidence evidence(L::CompletionRule rule = L::CompletionRule::AFTER_ABSENCE) {
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
// A fake server with a crude motion model. Only its wire shape means anything. target is the raw
// 607A value; endpoint is where the fake moves to. A 001F edge moves to target. A 005F edge adds
// target to the fake's own base: its actual position, or with commandedBase its last endpoint.
struct FakeDrive {
    uint16_t status = IDLE_STATUS, monitor = 1;
    uint8_t mode = 1;
    int32_t target = 5000, position = 0, velocity = 0, endpoint = 0, commanded = 0;
    uint32_t speed = 60, rampUp = 100, rampDown = 100;
    bool answer = true, acceptOnTrigger = true, clearCompletion = true, followEnable = false;
    bool commandedBase = false;
    bool mute = false; // Acts on every frame but its replies are lost: missing feedback.
    unsigned travelReads = 1; // 6041 reads reporting motion after a trigger, before arrival.
    unsigned remaining = 0, statusReads = 0, words = 0, alarmAtRead = 0, edges = 0;
    bool moving = false;
    uint16_t lastWord = 0xffff;
    uint16_t silentIndex = 0, abortIndex = 0, silentWord = 0, abortWord = 0;
    void arrive() {
        moving = false;
        status = uint16_t(status | COMPLETE_BIT);
        monitor = uint16_t((monitor | L::MONITOR_POSITION_COMPLETE) & ~L::MONITOR_RUNNING);
        position = endpoint;
    }
    void word(uint16_t w) {
        if ((w == 0x001f || w == 0x005f) && lastWord == 0x000f) {
            endpoint = w == 0x001f ? target : (commandedBase ? commanded : position) + target;
            commanded = endpoint;
            ++edges;
            if (acceptOnTrigger)
                status = uint16_t(status | ACCEPT_BIT);
            if (clearCompletion)
                status = uint16_t(status & ~COMPLETE_BIT);
            moving = true;
            remaining = travelReads;
            monitor = uint16_t((monitor & ~L::MONITOR_POSITION_COMPLETE) | L::MONITOR_RUNNING);
        } else if (w == 0x000f && (lastWord == 0x001f || lastWord == 0x005f)) {
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
inline uint16_t frameIndex(const M::Frame& f) { return uint16_t(f.data[1] | (uint16_t(f.data[2]) << 8)); }
inline uint32_t frameValue(const M::Frame& f) {
    return uint32_t(f.data[4]) | (uint32_t(f.data[5]) << 8) | (uint32_t(f.data[6]) << 16) |
           (uint32_t(f.data[7]) << 24);
}
inline bool replyTo(const M::Frame& sent, FakeDrive& drive, M::Frame& out) {
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
    // The application's transmit guard, as AxisSession documents it: the session holding the sequence
    // judges the download.
    E::AxisSession* sessions[2] = {nullptr, nullptr};
    bool preflight = true; // False stands for an application that does not wire the hook.
    unsigned preflightCalls = 0, preflightRefusals = 0;
    static Wire& of(void* context) { return *static_cast<Wire*>(static_cast<OwnerRig::WireBase*>(context)); }
    static bool beginRecovery(void*, uint32_t) { return true; }
    static O::RecoveryProgress pollRecovery(void* context, uint32_t) { return of(context).progress; }
    static bool guard(void* context, O::SequenceId sequence, const C::SdoTransfer& transfer, uint64_t now) {
        Wire& w = of(context);
        if (!w.preflight)
            return true;
        ++w.preflightCalls;
        for (E::AxisSession* s : w.sessions) {
            const O::SequenceId held = s ? s->sequence() : O::SequenceId();
            if (!held.owner || held.owner != sequence.owner || held.slot != sequence.slot ||
                held.generation != sequence.generation)
                continue;
            if (s->preflight(sequence, transfer, now))
                return true;
            ++w.preflightRefusals;
            return false;
        }
        return true;
    }
    O::Transport callbacks() {
        O::Transport value = plumbing();
        value.beginRecovery = beginRecovery;
        value.pollRecovery = pollRecovery;
        value.beforeTransmit = guard;
        return value;
    }
};
// One axis of the rig: its session, fake drive, latest reads, and the coordinate configuration and
// witness its moves borrow until they are terminal.
struct Axis {
    E::AxisSession a;
    FakeDrive drive;
    L::ReadContext state;
    L::IdentityObservation identity;
    M::AxisConfig coordinates;
    M::AxisReference witness;
    explicit Axis(O::CanBusOwner& owner) : a(owner) {}
};
struct Rig {
    C::SdoClient services;
    Wire wire;
    O::CanBusOwner owner;
    Axis one, two;
    // The first axis under its historical names, so single-node scenarios read as before.
    E::AxisSession& a;
    FakeDrive& drive;
    L::ReadContext& state;
    L::IdentityObservation& identity;
    uint64_t now = 0;
    unsigned served = 0, startFrame = 0;
    uint64_t sentAt[OwnerRig::WireBase::CAPACITY] = {};
    unsigned stamped = 0;
    bool serviceSession = true, serviceTwo = true;
    // With wire.autoReport false: a frame the adapter accepted but has not reported stays in the
    // adapter, off the bus, so the fake does not see it until reported.
    bool holdTx = false;
    unsigned nodes = 1;
    explicit Rig(bool twoNodes = false)
        : owner(services, wire.callbacks(), 73), one(owner), two(owner), a(one.a), drive(one.drive),
          state(one.state), identity(one.identity), nodes(twoNodes ? 2u : 1u) {
        wire.sessions[0] = &one.a;
        wire.sessions[1] = &two.a;
        for (unsigned node = 1; node <= nodes; ++node) {
            const L::ReadTarget t = target(node);
            CHECK(services.configure(uint8_t(node - 1), t.channel, true, true));
            C::NetworkRoute network;
            network.binding = t.channel.binding;
            network.generation = 11;
            network.heartbeatId = 0x700 + node;
            network.emcyId = 0x80 + node;
            CHECK(services.configureNetwork(uint8_t(node - 1), network, C::NetworkObserver(), 0));
        }
        one.coordinates = axisConfig(1);
        two.coordinates = axisConfig(2);
    }
    Rig(const Rig&) = delete;
    Rig& operator=(const Rig&) = delete;
    Axis& axis(unsigned node) { return node == 2 ? two : one; }
    L::ReadTarget current(unsigned node = 1) const {
        L::ReadTarget value = target(node);
        value.nodeGeneration = services.node(uint8_t(node - 1))->route.generation;
        return value;
    }
    void later(const M::Frame& value) { wire.push(value, now + 1, owner.controllerEpoch()); }
    void turn(uint64_t time) {
        now = time;
        if (wire.rxRead == wire.rxWrite)
            wire.rxRead = wire.rxWrite = 0;
        for (; served < wire.sentCount; ++served) {
            if (holdTx && wire.pending[served])
                break; // Still in the adapter; later frames wait behind it.
            const M::Frame& sent = wire.sent[served];
            if (sent.id != 0x601 && sent.id != 0x602)
                continue;
            FakeDrive& d = sent.id == 0x601 ? one.drive : two.drive;
            M::Frame reply;
            if (d.answer && replyTo(sent, d, reply) && !d.mute)
                wire.push(reply, now, owner.controllerEpoch());
        }
        if (asyncTraffic)
            for (unsigned node = 1; node <= nodes; ++node) {
                M::Frame heartbeat = frame(0x700 + node, 1);
                heartbeat.data[0] = 5;
                wire.push(heartbeat, now, owner.controllerEpoch());
            }
        CHECK(owner.service(now, budget(8, 2, 8, 2, 1)));
        for (; stamped < wire.sentCount; ++stamped)
            sentAt[stamped] = now;
        if (serviceSession)
            one.a.service(now);
        if (serviceTwo)
            two.a.service(now);
    }
    void step() { turn(now + TICK); }
    void steps(unsigned count) {
        for (unsigned i = 0; i < count; ++i)
            step();
    }
    void finishMove(unsigned limit = 2000, unsigned node = 1) {
        E::AxisSession& s = axis(node).a;
        for (unsigned i = 0; i < limit && (s.moveActive() || s.busy()); ++i)
            step();
        CHECK(!s.moveActive());
    }
    void open(unsigned node = 1) {
        CHECK(axis(node).a.open(current(node), L::SettingsModel::CL86_C, uint8_t(node), now));
    }
    C::SdoCompletion settled(unsigned node = 1) {
        C::SdoCompletion output;
        CHECK(axis(node).a.settle(output));
        return output;
    }
    void readState(L::ReadContext& into, uint8_t mask = STATE_MASK, unsigned node = 1) {
        E::AxisSession& s = axis(node).a;
        const uint64_t deadline = now + LEAD;
        const uint32_t operation = s.beginOperation(deadline, now);
        CHECK(operation);
        CHECK(L::prepareState(into, current(node), operation, now, deadline, reads(), mask));
        while (into.state == M::ReadState::ACTIVE) {
            L::PreparedRead work;
            CHECK(L::nextRead(into, now, work) && work.ready);
            CHECK(s.admitPrepared(work.transfer, now) == O::BusAdmission::ACCEPTED);
            steps(2);
            const C::SdoCompletion terminal = settled(node);
            CHECK(L::advanceRead(into, terminal, now));
        }
        CHECK(into.state == M::ReadState::SUCCEEDED);
    }
    void readIdentity(unsigned node = 1) {
        static L::ReadContext read;
        E::AxisSession& s = axis(node).a;
        const uint64_t deadline = now + LEAD;
        const uint32_t operation = s.beginOperation(deadline, now);
        CHECK(operation);
        CHECK(L::prepareIdentity(read, current(node), operation, now, deadline, reads()));
        while (read.state == M::ReadState::ACTIVE) {
            L::PreparedRead work;
            CHECK(L::nextRead(read, now, work) && work.ready);
            CHECK(s.admitPrepared(work.transfer, now) == O::BusAdmission::ACCEPTED);
            steps(2);
            const C::SdoCompletion terminal = settled(node);
            CHECK(L::advanceRead(read, terminal, now));
        }
        CHECK(read.state == M::ReadState::SUCCEEDED);
        CHECK(L::getIdentity(read, current(node), now, LEAD, axis(node).identity));
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
    void writeThrough(uint16_t index, uint8_t size, uint32_t value, unsigned node = 1) {
        E::AxisSession& s = axis(node).a;
        const uint64_t deadline = now + LEAD;
        const uint32_t operation = s.beginOperation(deadline, now);
        CHECK(operation);
        C::SdoTransfer t;
        t.channel = target(node).channel;
        t.channel.binding = current(node).channel.binding;
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
        CHECK(s.admitPrepared(t, now) == O::BusAdmission::ACCEPTED);
        steps(2);
        settled(node);
    }
    // The explicit enable is stood in for by an acknowledged 000F here; enable_action_test and the
    // absolute suite's enable chain run the real one.
    void ready(unsigned node = 1) {
        open(node);
        readIdentity(node);
        writeThrough(0x6040, 2, 0x000f, node);
        readState(axis(node).state, STATE_MASK, node);
    }
    L::AxisReservation reservation(unsigned node = 1) const {
        const E::AxisSession& s = node == 2 ? two.a : one.a;
        L::AxisReservation value;
        const O::SequenceId id = s.sequence();
        const O::Sequence* live = owner.sequence(id);
        CHECK(live);
        value.target = current(node);
        value.model = L::SettingsModel::CL86_C;
        value.owner = owner.identity();
        value.sequence = uint32_t(id.slot) + 1;
        value.generation = id.generation;
        value.acquiredUs = live->acquiredUs;
        return value;
    }
    // A move borrows its axis and witness until it is terminal, so they are copied into the axis's
    // own storage, and only while no move of that axis is active.
    M::Status request(const L::MoveEvidence& e = evidence(), const M::MoveRequest& r = moveRequest(),
                      const L::MoveProfile& p = profile(), const M::AxisReference* reference = nullptr,
                      const M::AxisConfig& coordinates = axisConfig(), uint64_t lead = LEAD,
                      const L::MoveOptions& o = options(), const L::DriverContext* stored = nullptr) {
        startFrame = wire.sentCount;
        if (!a.moveActive()) {
            one.coordinates = coordinates;
            if (reference)
                one.witness = *reference;
        }
        return a.requestMove(state, identity, e, r, p, stored, one.coordinates,
                             reference ? &one.witness : nullptr, o, now + lead, now);
    }
    M::Status relative(const M::AxisReference& reference, const L::MoveEvidence& e,
                       const M::MoveRequest& r, unsigned node = 1, const L::MoveProfile& p = profile(),
                       uint64_t lead = LEAD, const L::MoveOptions& o = options(),
                       const L::DriverContext* stored = nullptr) {
        Axis& x = axis(node);
        startFrame = wire.sentCount;
        if (!x.a.moveActive())
            x.witness = reference;
        return x.a.requestMoveRelative(x.state, x.identity, e, r, p, stored, x.coordinates, x.witness,
                                       o, now + lead, now);
    }
};
inline bool frameIs(const Rig& r, unsigned i, const uint8_t (&expected)[8], unsigned node = 1) {
    const M::Frame& f = r.wire.sent[i];
    return i < r.wire.sentCount && f.id == 0x600 + node && f.dlc == 8 && f.payloadLength == 8 &&
           !f.extended && !f.rtr && !std::memcmp(f.data, expected, sizeof expected);
}
inline bool downloadAt(const Rig& r, unsigned i, uint16_t index, uint8_t size, uint32_t value,
                       unsigned node = 1) {
    const uint8_t command = size == 4 ? 0x23 : size == 2 ? 0x2b : 0x2f;
    uint8_t expected[8] = {command, uint8_t(index), uint8_t(index >> 8), 0, 0, 0, 0, 0};
    for (unsigned b = 0; b < size; ++b)
        expected[4 + b] = uint8_t(value >> (8 * b));
    return frameIs(r, i, expected, node);
}
inline bool wordAt(const Rig& r, unsigned i, uint16_t word, unsigned node = 1) {
    return downloadAt(r, i, 0x6040, 2, word, node);
}
inline bool uploadAt(const Rig& r, unsigned i, uint16_t index, uint8_t sub = 0, unsigned node = 1) {
    const uint8_t expected[8] = {0x40, uint8_t(index), uint8_t(index >> 8), sub, 0, 0, 0, 0};
    return frameIs(r, i, expected, node);
}
inline unsigned wordsFrom(const Rig& r, unsigned from, uint16_t word, unsigned node = 1) {
    unsigned count = 0;
    for (unsigned i = from; i < r.wire.sentCount; ++i)
        count += wordAt(r, i, word, node);
    return count;
}
inline unsigned downloadsFrom(const Rig& r, unsigned from, uint16_t index, unsigned node = 1) {
    unsigned count = 0;
    for (unsigned i = from; i < r.wire.sentCount; ++i)
        count += r.wire.sent[i].id == 0x600 + node && r.wire.sent[i].data[0] != 0x40 &&
                 frameIndex(r.wire.sent[i]) == index;
    return count;
}
inline unsigned uploadsFrom(const Rig& r, unsigned from, uint16_t index, unsigned node = 1) {
    unsigned count = 0;
    for (unsigned i = from; i < r.wire.sentCount; ++i)
        count += uploadAt(r, i, index, 0, node);
    return count;
}
inline void quiet(Rig& r, unsigned ticks = 40) {
    const unsigned sent = r.wire.sentCount;
    r.steps(ticks);
    CHECK(r.wire.sentCount == sent);
}
inline bool cycleAt(const Rig& r, unsigned i) {
    const uint16_t order[] = {0x2100, 0x2101, 0x2102, 0x6041, 0x6061, 0x6064, 0x606c};
    for (unsigned k = 0; k < 7; ++k)
        if (!uploadAt(r, i + k, order[k]))
            return false;
    return true;
}
inline const L::MoveContext& moveOf(const Rig& r, unsigned node = 1) {
    return node == 2 ? r.two.a.moveAction() : r.one.a.moveAction();
}
// Refused at the session: no frame, no move, nothing busy.
inline void refused(Rig& r, const M::Status& s, M::Err code, M::MoveError detail) {
    CHECK(!s);
    CHECK(s.code == code);
    CHECK(s.detail == int32_t(detail));
    CHECK(!r.a.moveActive() && !r.a.busy());
    quiet(r, 4);
}
inline M::AxisReference witness(const Rig& r, int64_t position,
                                M::RelativeBasis basis = M::RelativeBasis::ACTUAL, unsigned node = 1) {
    M::AxisReference w;
    w.binding = axisConfig(node).binding;
    w.referenceGeneration = axisConfig(node).referenceGeneration;
    w.idle = w.stationary = w.nativeKnown = true;
    w.nativePosition = position;
    w.basis = basis;
    w.source = M::ScaleSource::QUALIFIED;
    w.observedUs = w.nowUs = r.now;
    w.maximumAgeUs = 1000000;
    return w;
}
} // namespace MoveRig
