// SPDX-License-Identifier: MIT
// Real AxisSession, owner and SDO engine with an explicitly synthetic velocity server.
// Its identity, mode-3 start/stop, reverse and status dialect are test inputs, not CL86 qualification.
#pragma once
#ifndef CHECK
#error "Define the suite's checks counter and CHECK before including VelocityRig.h"
#endif
#include "AxisSession.h"
#include "OwnerRig.h"
#include <cstring>

namespace VelocityRig {
namespace M = MotorControlCAN;
namespace C = MotorControlCAN::CANOPEN;
namespace L = MotorControlCAN::LICHUAN_C;
namespace E = MotorControlCANExample;
namespace O = MotorControlCANOwner;
// The rig binds node 1 on slot 0 and node 2 (the `other` drive) on slot 1. A one-channel capacity
// set binds node 1 alone; scenarios that use node 2 then run only with two channels. Channels
// beyond two stay unconfigured.
constexpr bool PAIRED_CHANNELS = C::SdoClient::CHANNELS >= 2;
constexpr unsigned RIG_NODES = PAIRED_CHANNELS ? 2 : 1;
using OwnerRig::budget;
using OwnerRig::frame;
const uint64_t TICK = 500, DURATION = 120000, DEADLINE = 300000;
const uint32_t QUEUE = 1000, RESPONSE = 1000;
const uint32_t DEVICE_TYPE = 0x40192, VENDOR = 0x1234, PRODUCT = 0x5432, REVISION = 0x55;
const uint16_t HARDWARE = 0x101, SOFTWARE = 0x202, MODEL = 0x86, FIRMWARE = 0x303;
template <class T> struct Saved {
    unsigned char bytes[sizeof(T)];
    explicit Saved(const T& value) { std::memcpy(bytes, &value, sizeof value); }
    void unchanged(const T& value) const { CHECK(!std::memcmp(bytes, &value, sizeof value)); }
};
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
inline L::VelocityOptions options(L::StopKind kind = L::StopKind::HALT) {
    L::VelocityOptions value;
    value.service = reads();
    value.pollIntervalUs = 10000;
    value.maxPolls = 20;
    value.maximumAgeUs = DEADLINE;
    value.requiredStopBudgetUs = 40000;
    value.commandIntervalUs = 10000;
    value.stopKind = kind;
    return value;
}
inline M::AxisConfig axisConfig() {
    M::AxisConfig value;
    value.binding = target().channel.binding;
    value.units.commandStepsPerMotorTurn = M::UnitScale(3200, 1, M::ScaleSource::QUALIFIED);
    value.units.motorTurnsPerLoadTurn = M::UnitScale(1, 1, M::ScaleSource::QUALIFIED);
    value.nativeRateBasis = M::NativeRateBasis::MOTOR_TURNS;
    value.nativeVelocityTime = M::TimeUnit::MINUTE;
    return value;
}
inline M::FiniteVelocityRequest request(int64_t rpm = 120, uint64_t duration = DURATION) {
    M::FiniteVelocityRequest value;
    value.velocity.value = M::Rational(rpm);
    value.velocity.unit = M::VelocityUnit(M::PositionUnit::TURNS, M::TimeUnit::MINUTE);
    value.velocity.configurationGeneration = target().channel.binding.configurationGeneration;
    value.durationUs = duration;
    return value;
}
inline L::VelocityProfile profile() {
    L::VelocityProfile value;
    value.rampUpTimeMs = 100;
    value.rampDownTimeMs = 200;
    return value;
}
inline L::VelocityEvidence evidence(bool reverse = false, L::StopKind kind = L::StopKind::HALT) {
    L::VelocityEvidence value;
    value.gate = L::VelocityGate::MEASURED;
    value.record = 351;
    value.model = L::SettingsModel::CL86_C;
    value.firmware.deviceType = DEVICE_TYPE;
    value.firmware.hardwareVersion = HARDWARE;
    value.firmware.softwareVersion = SOFTWARE;
    value.firmware.vendorId = VENDOR;
    value.firmware.productCode = PRODUCT;
    value.firmware.revision = REVISION;
    value.firmware.modelCode = MODEL;
    value.firmware.firmwareCode = FIRMWARE;
    value.layout = L::StatusLayout::ZH86;
    value.startQualified = value.stopQualified = value.modeChangeQualified = true;
    value.stopKind = kind;
    value.modeChangeFromRaw = 1;
    value.reverseQualified = reverse;
    value.maximumForwardRpm = 300;
    value.maximumReverseRpm = reverse ? 300 : 0;
    return value;
}
inline uint16_t indexOf(const M::Frame& value) {
    return uint16_t(value.data[1] | uint16_t(value.data[2]) << 8);
}
inline uint32_t valueOf(const M::Frame& value) {
    return uint32_t(value.data[4]) | uint32_t(value.data[5]) << 8 | uint32_t(value.data[6]) << 16 |
           uint32_t(value.data[7]) << 24;
}
// No trajectory or position simulation. The fake reports only the chosen mode/enable/running rules.
// mute suppresses replies after applying writes, preserving genuine unknown-write scenarios.
struct Drive {
    uint8_t mode = 3;
    uint16_t status = 1, monitor = L::MONITOR_RELEASED;
    int32_t targetRpm = 777, velocity = 0;
    uint32_t rampUp = 50, rampDown = 50;
    uint16_t muteIndex = 0, muteWord = 0, abortIndex = 0;
    bool stopWorks = true, releaseOnStop = false;
    bool respond(const M::Frame& sent, M::Frame& out) {
        const uint16_t index = indexOf(sent);
        const uint8_t sub = sent.data[3];
        const bool download = sent.data[0] != 0x40;
        const uint32_t value = valueOf(sent);
        out = frame(sent.id - 0x600 + 0x580, 8);
        out.data[1] = sent.data[1];
        out.data[2] = sent.data[2];
        out.data[3] = sub;
        if (abortIndex && index == abortIndex) {
            out.data[0] = 0x80;
            out.data[4] = 0x23;
            out.data[7] = 0x08;
            return true;
        }
        if (download) {
            if (index == 0x6060)
                mode = uint8_t(value);
            else if (index == 0x60ff)
                targetRpm = int32_t(value);
            else if (index == 0x6083)
                rampUp = value;
            else if (index == 0x6084)
                rampDown = value;
            else if (index == 0x6040) {
                if (value == 6) {
                    status = 1;
                    monitor = L::MONITOR_RELEASED;
                } else if (value == 7) {
                    status = 3;
                    monitor = 0;
                } else if (value == 15) {
                    status = 0x4007;
                    monitor = L::MONITOR_RUNNING;
                    velocity = targetRpm;
                } else if ((value == 0x010f || value == 0x000b) && stopWorks) {
                    status = releaseOnStop ? 0 : 7;
                    monitor = releaseOnStop ? L::MONITOR_RELEASED : 0;
                    velocity = 0;
                } else {
                    CHECK(value == 0x010f || value == 0x000b);
                }
            } else {
                CHECK(false); // An unexpected write is a fixture failure, never silently accepted.
            }
            if (muteIndex == index && (!muteWord || (index == 0x6040 && value == muteWord)))
                return false;
            out.data[0] = 0x60;
            return true;
        }
        if (muteIndex && muteIndex == index)
            return false;
        uint32_t raw = 0;
        unsigned size = 2;
        switch (index) {
        case 0x1000:
            raw = DEVICE_TYPE;
            size = 4;
            break;
        case 0x1009:
            raw = HARDWARE;
            break;
        case 0x100a:
            raw = SOFTWARE;
            break;
        case 0x1018:
            size = sub ? 4 : 1;
            raw = !sub ? 3 : sub == 1 ? VENDOR : sub == 2 ? PRODUCT : REVISION;
            break;
        case 0x2000:
            raw = MODEL;
            break;
        case 0x2001:
            raw = FIRMWARE;
            break;
        case 0x1001:
            size = 1;
            break;
        case 0x603f:
            break;
        case 0x2100:
            raw = monitor;
            break;
        case 0x2101:
            break;
        case 0x2102:
            break;
        case 0x6041:
            raw = status;
            break;
        case 0x6061:
            raw = mode;
            size = 1;
            break;
        case 0x6064:
            size = 4;
            break;
        case 0x606c:
            raw = uint32_t(velocity);
            size = 4;
            break;
        case 0x607a:
            size = 4;
            break;
        case 0x60ff:
            raw = uint32_t(targetRpm);
            size = 4;
            break;
        case 0x6081:
            raw = 120;
            size = 4;
            break;
        case 0x6083:
            raw = rampUp;
            size = 4;
            break;
        case 0x6084:
            raw = rampDown;
            size = 4;
            break;
        default:
            CHECK(false);
        }
        out.data[0] = size == 1 ? 0x4f : size == 2 ? 0x4b : 0x43;
        for (unsigned i = 0; i < 4; ++i)
            out.data[4 + i] = uint8_t(raw >> (8 * i));
        return true;
    }
};
struct Wire : OwnerRig::WireBase {
    E::AxisSession* session = nullptr;
    C::SdoClient* services = nullptr;
    bool driveFault = false;
    uint16_t provedUnsentWord =
        0; // A controller proof that this exact 6040 word never reached its SDK.
    unsigned preflightRefusals = 0;
    static bool transmitWithProof(void* context, const M::Frame& value, O::TransportTxId id,
                                  uint32_t epoch) {
        Wire& w = *static_cast<Wire*>(static_cast<OwnerRig::WireBase*>(context));
        const bool accepted = OwnerRig::WireBase::transmit(context, value, id, epoch);
        if (w.provedUnsentWord && indexOf(value) == 0x6040 && value.data[0] != 0x40 &&
            valueOf(value) == w.provedUnsentWord) {
            // Match TwaiAdapter's application queue contract: hold before SDK submission, then
            // report this held frame provably unsent. A plain negative TX report is insufficient.
            CHECK(w.services && w.services->holdHandoff(id.sdo));
            CHECK(w.autoReport && w.reportWrite);
            O::TxReport& report = w.reports[w.reportWrite - 1];
            report.transmitted = false;
            report.provedUnsent = true;
        }
        return accepted;
    }
    static bool guard(void* context, O::SequenceId sequence, const C::SdoTransfer& work,
                      uint64_t now) {
        Wire& w = *static_cast<Wire*>(static_cast<OwnerRig::WireBase*>(context));
        const O::SequenceId held = w.session->sequence();
        if (held.owner != sequence.owner || held.slot != sequence.slot ||
            held.generation != sequence.generation)
            return true;
        if (w.session->preflight(sequence, work, now, w.driveFault))
            return true;
        ++w.preflightRefusals;
        return false;
    }
    O::Transport callbacks() {
        O::Transport out = plumbing();
        out.beforeTransmit = guard;
        out.transmit = transmitWithProof;
        return out;
    }
};
struct Rig {
    C::SdoClient services;
    Wire wire;
    O::CanBusOwner owner;
    E::AxisSession session;
    Drive drive, other;
    L::ReadContext state, identityRead;
    L::IdentityObservation identity;
    M::AxisConfig axis = axisConfig();
    uint64_t now = 0;
    unsigned served = 0, startFrame = 0, stamped = 0;
    uint64_t sentAt[OwnerRig::WireBase::CAPACITY] = {};
    bool asynchronous = false, serviceSession = true;
    Rig() : owner(services, wire.callbacks(), 73), session(owner) {
        wire.session = &session;
        wire.services = &services;
        for (unsigned node = 1; node <= RIG_NODES; ++node) {
            CHECK(services.configure(uint8_t(node - 1), target(node).channel, true, true));
            C::NetworkRoute network;
            network.binding = target(node).channel.binding;
            network.generation = 11;
            network.heartbeatId = 0x700 + node;
            network.emcyId = 0x80 + node;
            CHECK(services.configureNetwork(uint8_t(node - 1), network, C::NetworkObserver(), 0));
        }
    }
    void turn(uint64_t at, O::ServiceBudget policy = budget()) {
        now = at;
        if (wire.rxRead == wire.rxWrite)
            wire.rxRead = wire.rxWrite = 0;
        for (; served < wire.sentCount; ++served) {
            if (wire.pending[served])
                break;
            bool provedUnsent = false;
            for (unsigned report = 0; report < wire.reportWrite; ++report)
                if (OwnerRig::same(wire.reports[report].id, wire.ids[served]))
                    provedUnsent |= wire.reports[report].provedUnsent;
            if (provedUnsent)
                continue; // A queued frame is not a physical server request.
            const M::Frame& sent = wire.sent[served];
            CHECK(sent.id == 0x601 || sent.id == 0x602);
            M::Frame reply;
            if ((sent.id == 0x601 ? drive : other).respond(sent, reply))
                wire.push(reply, now, owner.controllerEpoch());
        }
        if (asynchronous) {
            M::Frame heartbeat = frame(0x702, 1);
            heartbeat.data[0] = 5;
            wire.push(heartbeat, now, owner.controllerEpoch());
        }
        CHECK(owner.service(now, policy));
        for (; stamped < wire.sentCount; ++stamped)
            sentAt[stamped] = now;
        if (serviceSession)
            session.service(now);
    }
    void step() { turn(now + TICK); }
    void finish(unsigned bound = 1000) {
        for (unsigned i = 0; i < bound && session.busy(); ++i)
            step();
        CHECK(!session.busy());
    }
    void read(L::ReadContext& into) {
        for (unsigned guard = 0; into.state == M::ReadState::ACTIVE && guard < 30; ++guard) {
            L::PreparedRead work;
            CHECK(L::nextRead(into, now, work) && work.ready);
            CHECK(session.admitPrepared(work.transfer, now) == O::BusAdmission::ACCEPTED);
            step();
            step();
            C::SdoCompletion terminal;
            CHECK(session.settle(terminal));
            CHECK(L::advanceRead(into, terminal, now));
        }
        CHECK(into.state == M::ReadState::SUCCEEDED);
    }
    void readState() {
        const uint32_t operation = session.beginOperation(now + DEADLINE, now);
        CHECK(operation);
        const uint8_t mask = uint8_t((1u << unsigned(L::StateBlock::MONITORS)) |
                                     (1u << unsigned(L::StateBlock::STATUS)) |
                                     (1u << unsigned(L::StateBlock::FEEDBACK)));
        CHECK(L::prepareState(state, target(), operation, now, now + DEADLINE, reads(), mask));
        read(state);
    }
    void ready() {
        CHECK(session.open(target(), L::SettingsModel::CL86_C, 1, now));
        const uint32_t operation = session.beginOperation(now + DEADLINE, now);
        CHECK(operation);
        CHECK(L::prepareIdentity(identityRead, target(), operation, now, now + DEADLINE, reads()));
        read(identityRead);
        CHECK(L::getIdentity(identityRead, target(), now, DEADLINE, identity));
        readState();
    }
    void readProfile(L::DriverContext& into) {
        const uint32_t operation = session.beginOperation(now + DEADLINE, now);
        CHECK(operation);
        CHECK(L::prepareDriverRead(into, target(), operation, now, now + DEADLINE, reads(),
                                   L::DriverGroup::MOTION_PARAMETERS));
        for (unsigned i = 0; into.read.state == M::ReadState::ACTIVE && i < 10; ++i) {
            L::PreparedRead work;
            CHECK(L::nextDriver(into, now, work) && work.ready);
            CHECK(session.admitPrepared(work.transfer, now) == O::BusAdmission::ACCEPTED);
            step();
            step();
            C::SdoCompletion terminal;
            CHECK(session.settle(terminal) && L::advanceDriver(into, terminal, now));
        }
        CHECK(into.read.state == M::ReadState::SUCCEEDED);
    }
    M::Status start(const L::VelocityEvidence& proof = evidence(),
                    const M::FiniteVelocityRequest& intent = request(),
                    const L::VelocityOptions& policy = options(), uint64_t lead = DEADLINE) {
        startFrame = wire.sentCount;
        return session.requestVelocity(state, identity, proof, intent, profile(), axis, policy,
                                       now + lead, now);
    }
    unsigned writes(uint16_t index, uint32_t value, bool matchValue = true) const {
        unsigned count = 0;
        for (unsigned i = startFrame; i < wire.sentCount; ++i)
            if (wire.sent[i].id == 0x601 && wire.sent[i].data[0] != 0x40 &&
                indexOf(wire.sent[i]) == index && (!matchValue || valueOf(wire.sent[i]) == value))
                ++count;
        return count;
    }
    void running() {
        for (unsigned i = 0;
             i < 300 && session.velocityActive() && !session.velocityAction().runningObserved; ++i)
            step();
        CHECK(session.velocityActive() && session.velocityAction().runningObserved);
    }
};
} // namespace VelocityRig
