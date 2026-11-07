// SPDX-License-Identifier: MIT
// Synthetic mode6 action traffic through the sole real owner; no hardware qualification.
#include "CanBusOwner.h"
#include <MotorControlCAN/profiles/lichuan_c/Actions.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <initializer_list>

namespace M = MotorControlCAN;
namespace C = MotorControlCAN::CANOPEN;
namespace L = MotorControlCAN::LICHUAN_C;
namespace O = MotorControlCANOwner;
static unsigned checks = 0;
#define CHECK(condition)                                                                           \
    do {                                                                                           \
        ++checks;                                                                                  \
        if (!(condition)) {                                                                        \
            std::fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #condition);                   \
            std::abort();                                                                          \
        }                                                                                          \
    } while (false)
#include "support/OwnerRig.h"
namespace {
template <class T> struct Saved {
    unsigned char bytes[sizeof(T)];
    explicit Saved(const T& value) { std::memcpy(bytes, &value, sizeof value); }
    void unchanged(const T& value) const { CHECK(!std::memcmp(bytes, &value, sizeof value)); }
};
uint8_t bit(L::StateBlock block) {
    return uint8_t(1u << unsigned(block));
}
const uint8_t fullMask = uint8_t(bit(L::StateBlock::ERRORS) | bit(L::StateBlock::MONITORS) |
                                 bit(L::StateBlock::STATUS) | bit(L::StateBlock::FEEDBACK));
L::ReadTarget target() {
    L::ReadTarget t;
    t.channel = OwnerRig::route();
    t.nodeGeneration = 11;
    return t;
}
L::ActionOptions options() {
    L::ActionOptions o;
    o.service.queueTimeoutUs = o.service.responseTimeoutUs = 100;
    o.observationIntervalUs = 10;
    o.maximumAgeUs = 100000;
    o.commandIntervalUs = L::ENABLE_WORD_INTERVAL_US;
    return o;
}
struct Wire : OwnerRig::WireBase {
    L::ActionContext* action = nullptr;
    const L::AxisReservation* live = nullptr;
    static bool guard(void* context, O::SequenceId, const C::SdoTransfer& transfer, uint64_t now) {
        Wire& w = *static_cast<Wire*>(static_cast<OwnerRig::WireBase*>(context));
        if (!w.action || transfer.direction == C::SdoDirection::UPLOAD)
            return true;
        CHECK(transfer.index == 0x6040 && w.live);
        return bool(L::noteActionTransmission(*w.action, *w.live, target(), now));
    }
    O::Transport callbacks() {
        O::Transport t = plumbing();
        t.beforeTransmit = guard;
        return t;
    }
};
struct Rig {
    C::SdoClient services;
    Wire wire;
    O::CanBusOwner owner;
    O::SequenceId sequence;
    L::AxisReservation reservation;
    L::ReadContext state;
    uint64_t now = 0;
    const uint64_t deadline = 1000000;
    uint32_t operation = 0;
    bool heartbeat;
    uint16_t status = 1, monitor = 0x11;
    uint8_t mode = 6;
    int32_t position = 123, velocity = 0;
    explicit Rig(bool async = false) : owner(services, wire.callbacks(), 73), heartbeat(async) {
        CHECK(services.configure(0, target().channel, true, true));
        C::NetworkRoute network;
        network.binding = target().channel.binding;
        network.generation = 11;
        network.heartbeatId = 0x701;
        network.emcyId = 0x81;
        CHECK(services.configureNetwork(0, network, C::NetworkObserver(), 0));
        CHECK(owner.beginSequence(target().channel.binding, 1, 0, sequence));
        reservation.target = target();
        reservation.model = L::SettingsModel::CL86_C;
        reservation.owner = owner.identity();
        reservation.sequence = uint32_t(sequence.slot + 1);
        reservation.generation = sequence.generation;
        reservation.acquiredUs = owner.sequence(sequence)->acquiredUs;
        wire.live = &reservation;
    }
    void begin() { CHECK(owner.beginOperation(sequence, ++operation, deadline, now)); }
    void turn(uint64_t time, const O::ServiceBudget& budget = OwnerRig::budget()) {
        now = time;
        if (heartbeat) {
            M::Frame f = OwnerRig::frame(0x701, 1);
            f.data[0] = 5;
            wire.push(f, now, owner.controllerEpoch());
        }
        CHECK(owner.service(now, budget));
    }
    uint32_t raw(const C::SdoTransfer& work) const {
        switch (work.index) {
        case 0x2100:
            return monitor;
        case 0x6041:
            return status;
        case 0x6061:
            return mode;
        case 0x6064:
            return uint32_t(position);
        case 0x606c:
            return uint32_t(velocity);
        case 0x607a:
            return 5000;
        case 0x60ff:
            return 120;
        default:
            return 0;
        }
    }
    C::SdoCompletion exchange(const C::SdoTransfer& work, bool urgent = false, bool abort = false,
                              bool silent = false) {
        O::BusRequest request;
        request.sequence = sequence;
        request.transfer = work;
        O::RequestId id;
        CHECK((urgent ? owner.admitUrgent(request, now, id) : owner.admit(request, now, id)) ==
              O::BusAdmission::ACCEPTED);
        turn(now);
        const M::Frame& sent = wire.sent[wire.sentCount - 1];
        CHECK(sent.id == 0x601 && sent.dlc == 8 && sent.payloadLength == 8 &&
              sent.data[1] == uint8_t(work.index) && sent.data[2] == uint8_t(work.index >> 8) &&
              sent.data[3] == work.subindex);
        if (work.direction == C::SdoDirection::DOWNLOAD) {
            CHECK(work.index == 0x6040 && work.size == 2 && sent.data[0] == 0x2b &&
                  sent.data[4] == work.data[0] && sent.data[5] == work.data[1]);
        } else
            CHECK(sent.data[0] == 0x40);
        M::Frame reply = OwnerRig::frame(0x581, 8);
        reply.data[0] = abort                                         ? 0x80
                        : work.direction == C::SdoDirection::DOWNLOAD ? 0x60
                        : work.size == 1                              ? 0x4f
                        : work.size == 2                              ? 0x4b
                                                                      : 0x43;
        reply.data[1] = sent.data[1];
        reply.data[2] = sent.data[2];
        reply.data[3] = sent.data[3];
        const uint32_t value = abort ? 0x08000023u : raw(work);
        if (abort || work.direction == C::SdoDirection::UPLOAD)
            for (unsigned i = 0; i < 4; ++i)
                reply.data[4 + i] = uint8_t(value >> (8 * i));
        if (silent) {
            turn(now + 1, OwnerRig::budget(8, 0, 8, 0));
            turn(now + 101, OwnerRig::budget(8, 0, 8, 0));
        } else {
            wire.push(reply, now + 1, owner.controllerEpoch());
            turn(now + 1, OwnerRig::budget(8, 0, 8, 0));
        }
        const O::Completion* terminal = owner.result(id);
        CHECK(terminal && terminal->service.protocolAcknowledged == !(abort || silent));
        const C::SdoCompletion result = terminal->service;
        CHECK(owner.release(id));
        return result;
    }
    void acquire(uint8_t mask = fullMask) {
        wire.action = nullptr;
        begin();
        CHECK(L::prepareState(state, target(), operation, now, deadline, options().service, mask));
        while (state.state == M::ReadState::ACTIVE) {
            L::PreparedRead work;
            CHECK(L::nextRead(state, now, work) && work.ready);
            const C::SdoCompletion terminal = exchange(work.transfer);
            CHECK(L::advanceRead(state, terminal, now));
        }
        CHECK(state.state == M::ReadState::SUCCEEDED);
    }
    C::SdoCompletion predecessor(uint16_t word) {
        wire.action = nullptr;
        begin();
        C::SdoTransfer work;
        work.channel = target().channel;
        work.operation = work.request = operation;
        work.step = 1;
        work.index = 0x6040;
        work.size = 2;
        work.direction = C::SdoDirection::DOWNLOAD;
        work.data[0] = uint8_t(word);
        work.data[1] = uint8_t(word >> 8);
        work.queueTimeoutUs = work.responseTimeoutUs = 100;
        work.deadlineUs = deadline;
        return exchange(work);
    }
    L::EnableEvidence evidence() const {
        L::EnableEvidence e;
        e.modeRaw = 6;
        e.homingEnableQualified = true;
        e.gate = L::EnableGate::ATTENDED_EXPERIMENT;
        e.trial = 1;
        e.reservation = reservation;
        e.operation = operation;
        e.expiresUs = deadline;
        e.start = L::EnableStart::NO_MOTION_WRITE_RECORDED;
        e.reviewed = true;
        e.reviewedStatusword = status;
        e.reviewedPositionTarget = 5000;
        e.reviewedVelocityTarget = 120;
        return e;
    }
    L::PreparedAction next(const L::ActionContext& action) {
        L::PreparedAction out;
        CHECK(L::nextAction(action, reservation, target(), now, out));
        if (!out.ready && action.state == M::ActionState::ACTIVE && out.eligibleUs > now) {
            turn(out.eligibleUs);
            CHECK(L::nextAction(action, reservation, target(), now, out));
        }
        return out;
    }
    void consume(L::ActionContext& action, const L::PreparedAction& work, bool abort = false) {
        const C::SdoCompletion terminal =
            exchange(work.transfer, action.actionKind == M::ActionKind::STOP, abort);
        CHECK(L::advanceAction(action, reservation, target(), terminal, now));
    }
};

void explicitEnable(bool heartbeat) {
    Rig r(heartbeat);
    r.acquire();
    r.begin();
    L::ActionContext action;
    CHECK(L::prepareEnable(action, r.reservation, r.state, r.evidence(), r.operation, r.now,
                           r.deadline, options()));
    r.wire.action = &action;
    const unsigned start = r.wire.sentCount;
    uint16_t words[3] = {};
    uint64_t wordAt[3] = {};
    unsigned downloads = 0, targets = 0, fields = 0;
    while (action.state == M::ActionState::ACTIVE) {
        const L::PreparedAction work = r.next(action);
        CHECK(work.ready);
        if (work.transfer.direction == C::SdoDirection::DOWNLOAD) {
            CHECK(downloads < 3 && work.transfer.index == 0x6040 && !work.transfer.data[1]);
            const uint16_t expected[] = {6, 7, 15};
            words[downloads] = work.transfer.data[0];
            wordAt[downloads] = r.now;
            CHECK(words[downloads] == expected[downloads]);
            r.status = uint16_t(downloads == 0 ? 1 : downloads == 1 ? 3 : 7);
            r.monitor = uint16_t(downloads == 2 ? 1 : 0x11);
            ++downloads;
        } else if (work.transfer.index == 0x607a || work.transfer.index == 0x60ff) {
            CHECK(downloads == 0 && targets < 2);
            ++targets;
        } else {
            CHECK(downloads > 0);
            ++fields;
        }
        r.consume(action, work);
    }
    CHECK(action.state == M::ActionState::SUCCEEDED &&
          action.outcome == M::ActionOutcome::OBSERVED);
    CHECK(downloads == 3 && targets == 2 && fields == 21 && r.wire.sentCount == start + 26);
    CHECK(wordAt[1] >= wordAt[0] + L::ENABLE_WORD_INTERVAL_US &&
          wordAt[2] >= wordAt[1] + L::ENABLE_WORD_INTERVAL_US);
    CHECK(action.commandEvidenceMask == 7 && action.enable.evidence.modeRaw == 6);
    CHECK(action.enable.steps[2].present && action.extraObservation.actualVelocity == 0);
}

void enableRefusals() {
    for (unsigned bad = 0; bad < 8; ++bad) {
        Rig r;
        if (bad == 3)
            r.mode = 1;
        if (bad == 4) {
            r.status = 7;
            r.monitor = 1;
        }
        if (bad == 5)
            r.monitor = 0x15; // Running cannot be made an enable prerequisite.
        if (bad == 6)
            r.velocity = 1;
        r.acquire();
        r.begin();
        L::EnableEvidence e = r.evidence();
        if (bad == 0)
            e.gate = L::EnableGate::CLOSED;
        if (bad == 1)
            e.homingEnableQualified = false;
        if (bad == 2)
            e.modeRaw = 3;
        if (bad == 7) {
            e.modeRaw = 1;
            e.homingEnableQualified = false;
        }
        L::ActionContext action;
        action.operation = 0xaabb;
        const Saved<L::ActionContext> saved(action);
        const unsigned sent = r.wire.sentCount;
        CHECK(!L::prepareEnable(action, r.reservation, r.state, e, r.operation, r.now, r.deadline,
                                options()));
        saved.unchanged(action);
        r.turn(r.now + 1);
        CHECK(r.wire.sentCount == sent);
    }
}

void homeInterrupt(bool heartbeat, bool abort, bool movingFeedback) {
    Rig r(heartbeat);
    C::SdoCompletion control = r.predecessor(0x001f);
    r.status = 7;
    r.monitor = 4;
    r.acquire(uint8_t(bit(L::StateBlock::MONITORS) | bit(L::StateBlock::STATUS)));
    r.begin();
    L::ActionContext action;
    CHECK(L::prepareStop(action, r.reservation, r.state, control, L::StopKind::HALT, r.operation,
                         r.now, r.deadline, options()));
    CHECK(action.stopModeRaw == 6 && !action.predecessorDocumented &&
          action.predecessorBasis == L::PredecessorBasis::PROJECT_DECISION);
    r.wire.action = &action;
    const unsigned start = r.wire.sentCount;
    L::PreparedAction work = r.next(action);
    CHECK(work.ready && work.transfer.index == 0x6040 && work.transfer.size == 2 &&
          work.transfer.data[0] == 0x0f && work.transfer.data[1] == 0);
    r.consume(action, work, abort);
    CHECK(r.wire.sentCount == start + 1); // The falling edge itself is the stop, no preamble.
    if (abort) {
        CHECK(action.state == M::ActionState::FAILED &&
              action.outcome == M::ActionOutcome::PROTOCOL_REJECTED);
        CHECK(action.commandHistory[0].abortValid &&
              action.commandHistory[0].abortCode == 0x08000023);
        CHECK(!action.observationKnown && action.uncertain);
        return;
    }
    r.monitor = 0;
    r.velocity = movingFeedback ? 1 : 0;
    const uint16_t indices[] = {0x2100, 0x2101, 0x2102, 0x6041, 0x6061, 0x6064, 0x606c};
    for (uint16_t index : indices) {
        work = r.next(action);
        CHECK(work.ready && work.transfer.direction == C::SdoDirection::UPLOAD &&
              work.transfer.index == index);
        r.consume(action, work);
    }
    CHECK(r.wire.sentCount == start + 8 && action.observationKnown && action.observations == 1);
    CHECK(action.extraObservation.fieldCount == 2 &&
          action.extraObservation.actualVelocity == r.velocity);
    if (movingFeedback) {
        CHECK(action.state == M::ActionState::ACTIVE && action.outcome == M::ActionOutcome::NONE);
        CHECK(
            L::advanceAction(action, r.reservation, target(), M::ActionOutcome::CANCELLED, r.now));
        CHECK(action.state == M::ActionState::FAILED &&
              action.outcome == M::ActionOutcome::CANCELLED);
    } else
        CHECK(action.state == M::ActionState::SUCCEEDED &&
              action.outcome == M::ActionOutcome::OBSERVED &&
              action.execution == M::ActionExecution::ACKNOWLEDGED);
}

void stopRefusals() {
    for (unsigned bad = 0; bad < 6; ++bad) {
        Rig r;
        C::SdoCompletion control = r.predecessor(bad == 1 ? 0x000f : 0x001f);
        r.status = 7;
        r.monitor = 4;
        if (bad == 2)
            r.mode = 3;
        r.acquire(uint8_t(bit(L::StateBlock::MONITORS) | bit(L::StateBlock::STATUS)));
        if (bad == 3)
            control.protocolAcknowledged = false;
        if (bad == 4)
            ++control.transfer.channel.binding.configurationGeneration;
        if (bad == 5)
            r.now += options().maximumAgeUs + 1;
        r.begin();
        L::ActionContext action;
        const Saved<L::ActionContext> before(action);
        const unsigned sent = r.wire.sentCount;
        CHECK(!L::prepareStop(action, r.reservation, r.state, control,
                              bad == 0 ? L::StopKind::QUICK_STOP : L::StopKind::HALT, r.operation,
                              r.now, r.deadline, options()));
        before.unchanged(action);
        r.turn(r.now + 1);
        CHECK(r.wire.sentCount == sent);
    }
}
void lostInterruptReply(bool heartbeat) {
    Rig r(heartbeat);
    const C::SdoCompletion control = r.predecessor(0x001f);
    r.status = 7;
    r.monitor = 4;
    r.acquire(uint8_t(bit(L::StateBlock::MONITORS) | bit(L::StateBlock::STATUS)));
    r.begin();
    L::ActionContext action;
    CHECK(L::prepareStop(action, r.reservation, r.state, control, L::StopKind::HALT, r.operation,
                         r.now, r.deadline, options()));
    r.wire.action = &action;
    const L::PreparedAction work = r.next(action);
    const unsigned start = r.wire.sentCount;
    const C::SdoCompletion timeout = r.exchange(work.transfer, true, false, true);
    CHECK(timeout.outcome == C::SdoOutcome::TIMEOUT && timeout.transmissionPossible &&
          timeout.effectPossible && timeout.effectUncertain && !timeout.protocolAcknowledged);
    CHECK(L::advanceAction(action, r.reservation, target(), timeout, r.now));
    CHECK(action.state == M::ActionState::FAILED &&
          action.outcome == M::ActionOutcome::SERVICE_TIMEOUT && action.uncertain &&
          action.execution == M::ActionExecution::UNKNOWN && !action.observationKnown);
    CHECK(action.commandHistory[0].outcome == C::SdoOutcome::TIMEOUT);
    const Saved<L::ActionContext> history(action);
    CHECK(!L::advanceAction(action, r.reservation, target(), timeout, r.now));
    history.unchanged(action);
    const L::PreparedAction done = r.next(action);
    CHECK(!done.ready && r.wire.sentCount == start + 1);
}
} // namespace
int main() {
    explicitEnable(false);
    explicitEnable(true);
    enableRefusals();
    for (bool heartbeat : {false, true}) {
        homeInterrupt(heartbeat, false, false);
        homeInterrupt(heartbeat, true, false);
        homeInterrupt(heartbeat, false, true);
        lostInterruptReply(heartbeat);
    }
    stopRefusals();
    std::printf("homing actions: %u checks passed\n", checks);
}
