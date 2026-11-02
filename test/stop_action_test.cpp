// SPDX-License-Identifier: MIT
// Real service/owner integration with synthetic controller traffic only.
// The retained 6040=15 prerequisite is manufactured by this fake fixture;
// no stop preparation is permitted to send an enabling preamble.
#include "CanBusOwner.h"
#include <MotorControlCAN/profiles/lichuan_c/Actions.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace M = MotorControlCAN;
namespace C = MotorControlCAN::CANOPEN;
namespace L = MotorControlCAN::LICHUAN_C;
namespace E = MotorControlCANExample;
namespace {
unsigned checks = 0;
#define CHECK(condition)                                                                           \
    do {                                                                                           \
        ++checks;                                                                                  \
        if (!(condition)) {                                                                        \
            std::fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #condition);                   \
            std::abort();                                                                          \
        }                                                                                          \
    } while (false)
template <class T> struct Saved {
    unsigned char bytes[sizeof(T)];
    explicit Saved(const T& value) { std::memcpy(bytes, &value, sizeof value); }
    void unchanged(const T& value) const { CHECK(!std::memcmp(bytes, &value, sizeof value)); }
};
L::ReadTarget target(unsigned node = 1) {
    L::ReadTarget value;
    value.channel.binding.logicalAxisId = 100 + node;
    value.channel.binding.endpoint.profile = 41;
    value.channel.binding.endpoint.address = node;
    value.channel.binding.bindingGeneration = 3;
    value.channel.binding.configurationGeneration = 7;
    value.channel.clientToServerId = 0x600 + node;
    value.channel.serverToClientId = 0x580 + node;
    value.nodeGeneration = 11;
    return value;
}
L::ReadOptions options() {
    L::ReadOptions value;
    value.queueTimeoutUs = value.responseTimeoutUs = 100;
    return value;
}
uint8_t blocks() {
    return uint8_t((1u << unsigned(L::StateBlock::STATUS)) |
                   (1u << unsigned(L::StateBlock::MONITORS)));
}
M::Frame frame(uint32_t id, uint8_t size) {
    M::Frame value;
    value.id = id;
    value.dlc = value.payloadLength = size;
    return value;
}
M::Frame reply(const C::SdoTransfer& request, uint32_t raw = 0, bool abort = false) {
    M::Frame value = frame(request.channel.serverToClientId, 8);
    value.data[0] = abort                                            ? 0x80
                    : request.direction == C::SdoDirection::DOWNLOAD ? 0x60
                    : request.size == 1                              ? 0x4f
                    : request.size == 2                              ? 0x4b
                                                                     : 0x43;
    value.data[1] = uint8_t(request.index);
    value.data[2] = uint8_t(request.index >> 8);
    value.data[3] = request.subindex;
    if (abort || request.direction == C::SdoDirection::UPLOAD)
        for (unsigned i = 0; i < 4; ++i)
            value.data[4 + i] = uint8_t(raw >> (8 * i));
    return value;
}
bool same(E::TransportTxId a, E::TransportTxId b) {
        return E::sameTransportTxId(a, b);
    }
E::ServiceBudget budget(uint8_t rx = 8, uint8_t tx = 2, uint8_t events = 8, uint8_t starts = 2) {
    E::ServiceBudget value;
    value.rx = rx;
    value.tx = tx;
    value.events = events;
    value.starts = starts;
    return value;
}
struct Wire {
    enum { CAPACITY = 128 };
    E::ReceivedFrame incoming[CAPACITY];
    E::TxReport reports[CAPACITY];
    M::Frame sent[CAPACITY];
    E::TransportTxId ids[CAPACITY];
    uint32_t epochs[CAPACITY] = {};
    bool pending[CAPACITY] = {};
    unsigned rxRead = 0, rxWrite = 0, reportRead = 0, reportWrite = 0;
    unsigned sentCount = 0, retireCalls = 0, observed = 0;
    bool autoReport = true, retirement = true, accept = true;
    L::ActionContext* action = nullptr;
    const L::SettingsReservation* reservation = nullptr;
    C::SdoCompletion* latestControl = nullptr;
    static bool guard(void* context, E::SequenceId, const C::SdoTransfer& work, uint64_t now) {
        Wire& w = *static_cast<Wire*>(context);
        if (!w.action || work.operation != w.action->operation)
            return true;
        CHECK(w.reservation && work.index == 0x6040 && work.step == 1);
        if (!L::noteActionTransmission(*w.action, *w.reservation, w.reservation->target, now))
            return false;
        CHECK(w.latestControl);
        // The application owns last-control history. Withdraw prior000F
        // eligibility before the stop might leave, even if no reply arrives.
        // The action retains the original proof and possible effect separately.
        *w.latestControl = C::SdoCompletion();
        return true;
    }
    static bool receive(void* context, E::ReceivedFrame& output) {
        Wire& w = *static_cast<Wire*>(context);
        if (w.rxRead == w.rxWrite)
            return false;
        output = w.incoming[w.rxRead++];
        return true;
    }
    static bool transmit(void* context, const M::Frame& value, E::TransportTxId id, uint32_t epoch) {
        Wire& w = *static_cast<Wire*>(context);
        CHECK(w.sentCount < CAPACITY);
        const unsigned i = w.sentCount++;
        w.sent[i] = value;
        w.ids[i] = id;
        w.epochs[i] = epoch;
        w.pending[i] = true;
        if (w.autoReport)
            w.report(i, w.accept);
        return w.accept;
    }
    static bool takeReport(void* context, E::TxReport& output) {
        Wire& w = *static_cast<Wire*>(context);
        if (w.reportRead == w.reportWrite)
            return false;
        output = w.reports[w.reportRead++];
        return true;
    }
    static bool retire(void* context, E::TransportTxId id, uint32_t epoch) {
        Wire& w = *static_cast<Wire*>(context);
        ++w.retireCalls;
        if (!w.retirement)
            return false;
        for (unsigned i = 0; i < w.sentCount; ++i)
            if (same(w.ids[i], id) && w.epochs[i] == epoch)
                w.pending[i] = false;
        unsigned kept = w.reportRead;
        for (unsigned i = w.reportRead; i < w.reportWrite; ++i)
            if (!same(w.reports[i].id, id) || w.reports[i].controllerEpoch != epoch)
                w.reports[kept++] = w.reports[i];
        w.reportWrite = kept;
        return true;
    }
    static void observe(void* context, const E::ReceivedFrame&, uint64_t) {
        ++static_cast<Wire*>(context)->observed;
    }
    E::Transport callbacks() {
        E::Transport value;
        value.context = this;
        value.receive = receive;
        value.transmit = transmit;
        value.takeTxReport = takeReport;
        value.retire = retire;
        value.observe = observe;
        value.beforeTransmit = guard;
        return value;
    }
    void report(unsigned i, bool transmitted = true) {
        CHECK(i < sentCount && reportWrite < CAPACITY && pending[i]);
        E::TxReport& value = reports[reportWrite++];
        value.id = ids[i];
        value.controllerEpoch = epochs[i];
        value.transmitted = transmitted;
        pending[i] = false;
    }
    void push(const M::Frame& value, uint64_t now, uint32_t epoch = 1) {
        CHECK(rxWrite < CAPACITY);
        E::ReceivedFrame& input = incoming[rxWrite++];
        input.frame = value;
        input.receivedTimeKnown = true;
        input.receivedUs = now;
        input.controllerEpoch = epoch;
    }
};
struct Rig {
    C::SdoClient services;
    Wire wire;
    E::CanBusOwner owner;
    E::SequenceId sequence;
    L::SettingsReservation reservation;
    L::ReadContext state;
    C::SdoCompletion control;
    uint64_t now = 0;
    uint32_t operation = 0;
    unsigned selectedNode = 1;
    uint64_t deadline = 10000;
    L::ReadTarget selectedTarget() const { return target(selectedNode); }
    Rig() : owner(services, wire.callbacks(), 73) {
        for (unsigned node = 1; node <= 2; ++node) {
            const L::ReadTarget t = target(node);
            CHECK(services.configure(uint8_t(node - 1), t.channel, true, true));
            C::NetworkRoute network;
            network.binding = t.channel.binding;
            network.generation = 11;
            network.heartbeatId = 0x700 + node;
            network.emcyId = 0x80 + node;
            CHECK(services.configureNetwork(uint8_t(node - 1), network, C::NetworkObserver(), 0));
        }
        CHECK(owner.beginSequence(target().channel.binding, 1, now, sequence));
        reservation.target = target();
        reservation.model = L::SettingsModel::CL86_C;
        reservation.owner = owner.identity();
        reservation.sequence = uint32_t(sequence.slot + 1);
        reservation.generation = sequence.generation;
        reservation.acquiredUs = owner.sequence(sequence)->acquiredUs;
    }
    void begin() { CHECK(owner.beginOperation(sequence, ++operation, deadline, now)); }
    void turn(uint64_t time, const E::ServiceBudget& work = budget()) {
        now = time;
        CHECK(owner.service(now, work));
    }
    C::SdoTransfer transfer(uint16_t index, uint32_t step = 1, bool download = false) const {
        C::SdoTransfer value;
        value.channel = selectedTarget().channel;
        value.operation = operation;
        value.request = operation;
        value.step = step;
        value.index = index;
        value.size = 2;
        value.direction = download ? C::SdoDirection::DOWNLOAD : C::SdoDirection::UPLOAD;
        value.deadlineUs = deadline;
        value.queueTimeoutUs = value.responseTimeoutUs = 100;
        return value;
    }
    E::RequestId admit(const C::SdoTransfer& transfer, bool urgent = false) {
        E::BusRequest work;
        work.sequence = sequence;
        work.transfer = transfer;
        E::RequestId id;
        CHECK((urgent ? owner.admitUrgent(work, now, id) : owner.admit(work, now, id)) ==
              E::BusAdmission::ACCEPTED);
        return id;
    }
    const E::Completion& done(E::RequestId id) const {
        const E::Completion* value = owner.result(id);
        CHECK(value);
        return *value;
    }
    C::SdoCompletion complete(E::RequestId id, const C::SdoTransfer& work, uint32_t raw = 0,
                              bool abort = false, bool release = true) {
        wire.push(reply(work, raw, abort), now + 1);
        turn(now + 1, budget(8, 0, 8, 0));
        const C::SdoCompletion value = done(id).service;
        if (release)
            CHECK(owner.release(id));
        return value;
    }
    C::SdoCompletion exchange(const C::SdoTransfer& work, uint32_t raw = 0) {
        const E::RequestId id = admit(work);
        turn(now);
        return complete(id, work, raw);
    }
    void acquire(uint16_t status = 7, uint16_t monitor = 4, uint8_t mode = 1) {
        begin();
        CHECK(L::prepareState(state, selectedTarget(), operation, now, deadline, options(),
                              blocks()));
        const uint16_t indices[] = {0x2100, 0x2101, 0x2102, 0x6041, 0x6061};
        const uint32_t values[] = {monitor, 0xa580, 0xf008, status, mode};
        for (unsigned i = 0; i < 5; ++i) {
            L::PreparedRead work;
            CHECK(L::nextRead(state, now, work) && work.ready);
            CHECK(work.transfer.index == indices[i]);
            const C::SdoCompletion result = exchange(work.transfer, values[i]);
            CHECK(L::advanceRead(state, result, now));
        }
        CHECK(state.state == M::ReadState::SUCCEEDED);
    }
    void prerequisites(uint16_t status = 7, uint16_t monitor = 4, uint8_t mode = 1) {
        const unsigned before = wire.sentCount;
        begin();
        C::SdoTransfer fakeEnable = transfer(0x6040, 1, true);
        fakeEnable.data[0] = 15;
        control = exchange(fakeEnable);
        CHECK(control.protocolAcknowledged && control.transmissionConfirmed);
        acquire(status, monitor, mode);
        CHECK(wire.sentCount == before + 6);
    }
    L::ActionOptions actionOptions() const {
        L::ActionOptions value;
        value.service = options();
        value.observationIntervalUs = 10;
        value.maximumAgeUs = 1000;
        return value;
    }
    void prepare(L::ActionContext& action, L::StopKind kind = L::StopKind::HALT,
                 uint64_t observationInterval = 10) {
        begin();
        L::ActionOptions policy = actionOptions();
        policy.observationIntervalUs = observationInterval;
        CHECK(L::prepareStop(action, reservation, state, control, kind, operation, now, deadline,
                             policy));
        wire.action = &action;
        wire.reservation = &reservation;
        wire.latestControl = &control;
    }
    L::PreparedAction next(const L::ActionContext& action) {
        L::PreparedAction work;
        CHECK(L::nextAction(action, reservation, selectedTarget(), now, work));
        return work;
    }
    C::SdoCompletion field(L::ActionContext& action, uint16_t index, uint32_t raw = 0,
                           bool abort = false) {
        const L::PreparedAction work = next(action);
        CHECK(work.ready && work.transfer.index == index && work.transfer.subindex == 0);
        const E::RequestId id = admit(work.transfer, true);
        turn(now);
        const M::Frame& sent = wire.sent[wire.sentCount - 1];
        CHECK(sent.id == 0x600 + selectedNode && sent.payloadLength == 8 &&
              sent.data[1] == uint8_t(index) && sent.data[2] == uint8_t(index >> 8));
        if (index == 0x6040) {
            const uint8_t literal[] = {0x2b,
                                       0x40,
                                       0x60,
                                       0,
                                       uint8_t(action.kind == L::StopKind::HALT ? 0x0f : 0x0b),
                                       uint8_t(action.kind == L::StopKind::HALT ? 1 : 0),
                                       0,
                                       0};
            CHECK(!std::memcmp(sent.data, literal, sizeof literal));
        } else {
            CHECK(sent.data[0] == 0x40);
            for (unsigned i = 4; i < 8; ++i)
                CHECK(sent.data[i] == 0);
        }
        const C::SdoCompletion result = complete(id, work.transfer, raw, abort);
        CHECK(L::advanceAction(action, reservation, selectedTarget(), result, now));
        if (index == 0x6040)
            control = result;
        return result;
    }
    void cycle(L::ActionContext& action, uint16_t monitor = 0, uint16_t status = 7,
               uint8_t mode = 1) {
        field(action, 0x2100, monitor);
        field(action, 0x2101, 0xa580);
        field(action, 0x2102, 0xf008);
        field(action, 0x6041, status);
        field(action, 0x6061, mode);
    }
};

void exactCommandsAndObservedReport() {
    for (unsigned kind = 0; kind < 2; ++kind)
        for (unsigned moving = 0; moving < 2; ++moving) {
            Rig r;
            r.prerequisites(7, moving ? 4 : 0);
            if (moving)
                r.reservation.model = L::SettingsModel::OL86_C;
            L::ActionContext action;
            const unsigned before = r.wire.sentCount;
            r.prepare(action, kind ? L::StopKind::QUICK_STOP : L::StopKind::HALT);
            CHECK(r.wire.sentCount == before && action.state == M::ActionState::ACTIVE &&
                  action.execution == M::ActionExecution::NOT_TRANSMITTED &&
                  action.completion == M::ActionCompletion::NOT_OBSERVED);
            r.field(action, 0x6040);
            CHECK(action.execution == M::ActionExecution::ACKNOWLEDGED &&
                  action.state == M::ActionState::ACTIVE &&
                  action.completion == M::ActionCompletion::NOT_OBSERVED);
            const Saved<C::SdoCompletion> command(action.commandEvidence);
            // High status bits, including conflicting source dialect flags, do
            // not decide this narrow monitor-based completion contract.
            r.cycle(action, 0xff80, 0xffff);
            CHECK(action.state == M::ActionState::SUCCEEDED &&
                  action.outcome == M::ActionOutcome::OBSERVED &&
                  action.completion == M::ActionCompletion::OBSERVED && action.observationKnown &&
                  action.observations == 1);
            CHECK(r.wire.sentCount == before + 6);
            command.unchanged(action.commandEvidence);
            const Saved<L::ActionContext> terminal(action);
            CHECK(!r.next(action).ready);
            CHECK(!L::advanceAction(action, r.reservation, target(), M::ActionOutcome::CANCELLED,
                                    r.now));
            terminal.unchanged(action);
        }
}
void runningPollingAndDeadline() {
    Rig r;
    r.prerequisites();
    r.deadline = 60;
    L::ActionContext action;
    r.prepare(action);
    r.field(action, 0x6040);
    r.cycle(action, 4, 0x8007); // Old position-complete bit cannot finish a running stop.
    CHECK(action.state == M::ActionState::ACTIVE && action.phase == L::ActionPhase::WAITING &&
          action.completion == M::ActionCompletion::NOT_OBSERVED && action.observations == 1);
    const uint64_t eligible = action.eligibleUs;
    const unsigned before = r.wire.sentCount;
    CHECK(eligible == r.now + 10 && !r.next(action).ready);
    r.turn(eligible - 1, E::ServiceBudget());
    CHECK(!r.next(action).ready && r.wire.sentCount == before);
    r.turn(eligible, E::ServiceBudget());
    CHECK(r.next(action).ready && r.next(action).transfer.step == 7);
    r.cycle(action, 4);
    CHECK(action.observations == 2 && action.state == M::ActionState::ACTIVE);
    r.turn(r.deadline, E::ServiceBudget());
    CHECK(L::advanceAction(action, r.reservation, target(), M::ActionOutcome::CANCELLED, r.now));
    CHECK(action.outcome == M::ActionOutcome::DEADLINE && action.uncertain &&
          action.execution == M::ActionExecution::ACKNOWLEDGED &&
          action.completion == M::ActionCompletion::NOT_OBSERVED);
    CHECK(r.wire.sentCount == before + 5); // No repeated command, including on deadline.
}
void unsupportedPrerequisitesAreAtomic() {
    for (unsigned scenario = 0; scenario < 12; ++scenario) {
        Rig r;
        r.prerequisites(scenario == 0 ? 3 : 7,
                        scenario == 1   ? 8
                        : scenario == 2 ? 16
                                        : 4,
                        scenario == 3 ? 3 : 1);
        r.begin();
        L::SettingsReservation reservation = r.reservation;
        C::SdoCompletion control = r.control;
        L::ActionOptions policy = r.actionOptions();
        L::StopKind kind = L::StopKind::HALT;
        if (scenario == 4)
            reservation.model = L::SettingsModel::UNKNOWN;
        if (scenario == 5)
            control.transfer.data[0] = 11; // Prior quick-stop is not prior000F.
        if (scenario == 6)
            control.transfer.data[1] = 1; // Prior halt is not prior000F.
        if (scenario == 7)
            control.protocolAcknowledged = false;
        if (scenario == 8)
            reservation.acquiredUs = r.now;
        if (scenario == 9)
            policy.maximumAgeUs = 1;
        if (scenario == 10)
            policy.observationIntervalUs = 0;
        if (scenario == 11)
            kind = static_cast<L::StopKind>(255);
        L::ActionContext action;
        action.operation = 777;
        const Saved<L::ActionContext> before(action);
        const unsigned sent = r.wire.sentCount;
        CHECK(!L::prepareStop(action, reservation, r.state, control, kind, r.operation, r.now,
                              r.deadline, policy));
        before.unchanged(action);
        CHECK(r.wire.sentCount == sent);
    }
}
void contradictoryObservationsAndAbort() {
    for (unsigned scenario = 0; scenario < 4; ++scenario) {
        Rig r;
        r.prerequisites();
        L::ActionContext action;
        r.prepare(action, L::StopKind::QUICK_STOP);
        r.field(action, 0x6040);
        const Saved<C::SdoCompletion> command(action.commandEvidence);
        if (scenario < 3) {
            r.cycle(action, scenario == 0 ? 8 : scenario == 1 ? 16 : 0, 7, scenario == 2 ? 6 : 1);
            CHECK(action.outcome == M::ActionOutcome::OBSERVATION_SCOPE);
        } else {
            r.field(action, 0x2100, 0x08000023, true);
            CHECK(action.observationEvidence.abortValid &&
                  action.observationEvidence.abortCode == 0x08000023);
        }
        CHECK(action.state == M::ActionState::FAILED && action.uncertain &&
              action.execution == M::ActionExecution::ACKNOWLEDGED &&
              action.completion == M::ActionCompletion::NOT_OBSERVED);
        command.unchanged(action.commandEvidence);
    }
    Rig rejected;
    rejected.prerequisites();
    L::ActionContext action;
    rejected.prepare(action);
    rejected.field(action, 0x6040, 0x08000022, true);
    CHECK(action.state == M::ActionState::FAILED &&
          action.outcome == M::ActionOutcome::PROTOCOL_REJECTED &&
          action.execution == M::ActionExecution::REJECTED && action.commandEvidence.abortValid &&
          action.commandEvidence.abortCode == 0x08000022);
    CHECK(rejected.wire.sentCount == 7);
}

void cancellationAndLostReplies() {
    for (unsigned phase = 0; phase < 4; ++phase) {
        Rig r;
        r.prerequisites();
        L::ActionContext action;
        r.prepare(action);
        const L::PreparedAction work = r.next(action);
        const E::RequestId id = r.admit(work.transfer, true);
        const unsigned before = r.wire.sentCount;
        if (phase) {
            r.wire.autoReport = phase > 1;
            r.turn(r.now);
            if (phase == 2)
                r.turn(r.now + 1, budget(0, 0, 8, 0));
        }
        if (phase == 3) {
            r.turn(r.now + 100, E::ServiceBudget());
            CHECK(r.done(id).service.outcome == C::SdoOutcome::TIMEOUT);
        } else {
            CHECK(r.owner.cancel(id, r.now) == E::Cancel::CANCELLED);
        }
        const C::SdoCompletion original = r.done(id).service;
        CHECK(original.transmissionPossible == (phase != 0));
        CHECK(L::advanceAction(action, r.reservation, target(), original, r.now));
        r.control = original;
        CHECK(action.state == M::ActionState::FAILED &&
              action.execution ==
                  (phase ? M::ActionExecution::UNKNOWN : M::ActionExecution::NOT_TRANSMITTED));
        CHECK(action.uncertain == (phase != 0));
        const Saved<E::Completion> retained(r.done(id));
        const Saved<L::ActionContext> terminal(action);
        r.wire.push(reply(work.transfer), r.now + 1);
        r.turn(r.now + 1, budget(8, 2, 8, 2));
        retained.unchanged(r.done(id));
        terminal.unchanged(action);
        CHECK(r.wire.sentCount == before + unsigned(phase != 0));
        CHECK(!r.next(action).ready);
        if (phase) {
            L::ActionContext repeated;
            const Saved<L::ActionContext> unchanged(repeated);
            CHECK(!L::prepareStop(repeated, r.reservation, r.state, r.control, action.kind,
                                  r.operation + 1, r.now, r.deadline, r.actionOptions()));
            unchanged.unchanged(repeated);
        }
    }
}

void interruptQueuedWorkPreservesResultsAndFencesOldOperations() {
    Rig r;
    r.prerequisites();
    r.begin();
    const C::SdoTransfer oldA = r.transfer(0x2101, 1);
    const C::SdoTransfer oldB = r.transfer(0x2102, 2);
    const E::RequestId first = r.admit(oldA), second = r.admit(oldB);
    CHECK(!r.owner.beginOperation(r.sequence, r.operation + 1, r.deadline, r.now));
    CHECK(r.owner.cancel(first, r.now) == E::Cancel::CANCELLED);
    CHECK(r.owner.cancel(second, r.now) == E::Cancel::CANCELLED);
    const Saved<E::Completion> savedA(r.done(first)), savedB(r.done(second));
    L::ActionContext stop;
    r.prepare(stop);
    E::BusRequest stale;
    stale.sequence = r.sequence;
    stale.transfer = oldA;
    E::RequestId rejected;
    rejected.generation = 999;
    const Saved<E::RequestId> unchanged(rejected);
    CHECK(r.owner.admit(stale, r.now, rejected) == E::BusAdmission::INVALID);
    unchanged.unchanged(rejected);
    const unsigned before = r.wire.sentCount;
    r.field(stop, 0x6040);
    r.cycle(stop);
    CHECK(stop.state == M::ActionState::SUCCEEDED && r.wire.sentCount == before + 6);
    savedA.unchanged(r.done(first));
    savedB.unchanged(r.done(second));
    CHECK(r.owner.admit(stale, r.now, rejected) == E::BusAdmission::INVALID);
    CHECK(!r.done(first).service.transmissionPossible &&
          !r.done(second).service.transmissionPossible);
    // Explicitly newer post-stop work has a different operation, while old
    // completions remain retained. This read is not resumed motion.
    r.begin();
    r.wire.action = nullptr;
    CHECK(r.exchange(r.transfer(0x2101)).outcome == C::SdoOutcome::SUCCESS);
    savedA.unchanged(r.done(first));
    savedB.unchanged(r.done(second));
}

void reservedUrgentCapacity() {
    Rig r;
    r.prerequisites();
    r.begin();
    E::RequestId original[E::CanBusOwner::REQUESTS];
    for (unsigned i = 0; i < E::CanBusOwner::REQUESTS; ++i) {
        const C::SdoTransfer work = r.transfer(0x2101, i + 1);
        original[i] = r.admit(work);
        CHECK(r.owner.cancel(original[i], r.now) == E::Cancel::CANCELLED);
    }
    L::ActionContext action;
    r.prepare(action);
    const L::PreparedAction work = r.next(action);
    E::BusRequest request;
    request.sequence = r.sequence;
    request.transfer = work.transfer;
    E::RequestId id;
    CHECK(r.owner.admit(request, r.now, id) == E::BusAdmission::RESULTS_FULL);
    id = r.admit(work.transfer, true);
    request.transfer.step = 2;
    E::RequestId refused;
    CHECK(r.owner.admitUrgent(request, r.now, refused) == E::BusAdmission::RESULTS_FULL);
    r.turn(r.now);
    const C::SdoCompletion ack = r.complete(id, work.transfer, 0, false, false);
    CHECK(L::advanceAction(action, r.reservation, target(), ack, r.now));
    const Saved<E::Completion> retained(r.done(id));
    request.transfer = r.next(action).transfer;
    CHECK(r.owner.admitUrgent(request, r.now, refused) == E::BusAdmission::RESULTS_FULL);
    retained.unchanged(r.done(id));
    CHECK(r.owner.release(id));
    r.cycle(action);
    CHECK(action.state == M::ActionState::SUCCEEDED);
    for (E::RequestId old : original)
        CHECK(r.done(old).reason == E::CompletionReason::CANCELLED &&
              !r.done(old).service.transmissionPossible);
}

void quarantineAndOtherNodeProgress() {
    for (unsigned retired = 0; retired < 2; ++retired) {
        Rig r;
        r.prerequisites();
        r.begin();
        const C::SdoTransfer oldWork = r.transfer(0x2101);
        const E::RequestId old = r.admit(oldWork);
        r.wire.autoReport = false;
        r.wire.retirement = retired != 0;
        const unsigned last = r.wire.sentCount;
        r.turn(r.now);
        CHECK(r.wire.pending[last]);
        CHECK(r.owner.cancel(old, r.now) == E::Cancel::CANCELLED);
        const Saved<E::Completion> previous(r.done(old));
        CHECK(r.done(old).service.effectUncertain && r.services.unresolved(0));
        CHECK(r.wire.pending[last] == !retired && r.wire.retireCalls == 1);
        CHECK(!r.owner.sequence(r.sequence));
        CHECK(!r.owner.beginOperation(r.sequence, r.operation + 1, r.deadline, r.now));
        E::BusRequest blocked;
        blocked.sequence = r.sequence;
        blocked.transfer = oldWork;
        ++blocked.transfer.step;
        E::RequestId refused;
        CHECK(r.owner.admitUrgent(blocked, r.now, refused) == E::BusAdmission::INVALID);
        // The old channel cannot interpret an identical late reply as fresh
        // stop confirmation, regardless of local retirement success.
        r.wire.push(reply(oldWork, 0), r.now + 1);
        r.turn(r.now + 1, budget(8, 0, 8, 0));
        previous.unchanged(r.done(old));
        CHECK(r.services.unresolved(0));

        E::SequenceId second;
        CHECK(r.owner.beginSequence(target(2).channel.binding, 2, r.now, second));
        CHECK(r.owner.beginOperation(second, 50, r.deadline, r.now));
        C::SdoTransfer fresh = oldWork;
        fresh.channel = target(2).channel;
        fresh.operation = fresh.request = 50;
        E::BusRequest request;
        request.sequence = second;
        request.transfer = fresh;
        E::RequestId other;
        r.wire.autoReport = true;
        CHECK(r.owner.admitUrgent(request, r.now, other) == E::BusAdmission::ACCEPTED);
        r.turn(r.now);
        M::Frame heartbeat = frame(0x702, 1);
        heartbeat.data[0] = 5;
        r.wire.push(heartbeat, r.now + 1);
        r.turn(r.now + 1, budget(8, 0, 8, 0));
        CHECK(r.services.node(1)->nmt == C::NmtState::OPERATIONAL && !r.owner.result(other));
        r.wire.push(reply(fresh, 0), r.now + 1);
        r.turn(r.now + 1, budget(8, 0, 8, 0));
        CHECK(r.done(other).service.outcome == C::SdoOutcome::SUCCESS);
        previous.unchanged(r.done(old));
        CHECK(r.wire.sentCount == last + 2);
        CHECK(r.owner.release(other));
        // A real stop on the other node uses this same owner while the first
        // node stays quarantined and its uncertain original outcome survives.
        r.selectedNode = 2;
        r.sequence = second;
        r.operation = 50;
        r.reservation.target = target(2);
        r.reservation.sequence = uint32_t(second.slot + 1);
        r.reservation.generation = second.generation;
        r.reservation.acquiredUs = r.owner.sequence(second)->acquiredUs;
        r.prerequisites();
        L::ActionContext stop;
        r.prepare(stop, L::StopKind::QUICK_STOP);
        r.field(stop, 0x6040);
        r.wire.push(heartbeat, r.now + 1);
        r.turn(r.now + 1, budget(8, 0, 8, 0));
        r.cycle(stop);
        CHECK(stop.outcome == M::ActionOutcome::OBSERVED && r.services.unresolved(0));
        previous.unchanged(r.done(old));
    }
}

void generationsRebootFaultAndCorrelation() {
    for (unsigned change = 0; change < 4; ++change) {
        Rig r;
        r.prerequisites();
        L::ActionContext action;
        r.prepare(action);
        L::SettingsReservation live = r.reservation;
        L::ReadTarget current = target();
        if (change == 0)
            ++live.generation;
        if (change == 1)
            ++current.nodeGeneration;
        if (change == 2)
            ++current.channel.binding.bindingGeneration;
        if (change == 3)
            ++current.channel.binding.configurationGeneration;
        const Saved<L::ActionContext> before(action);
        L::PreparedAction output;
        output.eligibleUs = 999;
        const Saved<L::PreparedAction> saved(output);
        CHECK(!L::nextAction(action, live, current, r.now, output));
        CHECK(!L::noteActionTransmission(action, live, current, r.now));
        saved.unchanged(output);
        before.unchanged(action);
        CHECK(L::advanceAction(action, live, current, M::ActionOutcome::INVALIDATED, r.now));
        CHECK(action.state == M::ActionState::FAILED &&
              action.execution == M::ActionExecution::NOT_TRANSMITTED && !action.uncertain);
        CHECK(r.wire.sentCount == 6);
    }
    for (unsigned reboot = 0; reboot < 2; ++reboot) {
        Rig r;
        r.prerequisites();
        L::ActionContext action;
        r.prepare(action);
        r.field(action, 0x6040);
        const Saved<C::SdoCompletion> command(action.commandEvidence);
        if (reboot) {
            M::Frame boot = frame(0x701, 1);
            r.wire.push(boot, r.now + 1);
            r.turn(r.now + 1, budget(8, 0, 8, 0));
            CHECK(!r.owner.sequence(r.sequence));
            L::ReadTarget current = target();
            current.nodeGeneration = r.services.node(0)->route.generation;
            CHECK(L::advanceAction(action, r.reservation, current, M::ActionOutcome::INVALIDATED,
                                   r.now));
        } else {
            CHECK(L::advanceAction(action, r.reservation, target(), M::ActionOutcome::DRIVE_FAULT,
                                   r.now));
        }
        CHECK(action.state == M::ActionState::FAILED && action.uncertain &&
              action.execution == M::ActionExecution::ACKNOWLEDGED);
        command.unchanged(action.commandEvidence);
        CHECK(r.wire.sentCount == 7);
    }
    Rig r;
    r.prerequisites();
    L::ActionContext action;
    r.prepare(action);
    const L::PreparedAction work = r.next(action);
    const E::RequestId id = r.admit(work.transfer, true);
    r.turn(r.now);
    const C::SdoCompletion actual = r.complete(id, work.transfer);
    for (unsigned wrong = 0; wrong < 4; ++wrong) {
        C::SdoCompletion changed = actual;
        if (wrong == 0)
            ++changed.transfer.operation;
        if (wrong == 1)
            ++changed.transfer.step;
        if (wrong == 2)
            ++changed.nodeGeneration;
        if (wrong == 3)
            changed.protocolAcknowledged = false;
        const Saved<L::ActionContext> previous(action);
        CHECK(!L::advanceAction(action, r.reservation, target(), changed, r.now));
        previous.unchanged(action);
    }
    CHECK(L::advanceAction(action, r.reservation, target(), actual, r.now));
    const Saved<L::ActionContext> after(action);
    CHECK(!L::advanceAction(action, r.reservation, target(), actual, r.now));
    after.unchanged(action);
}

void exactDeadlineAndRepeatedStopRejection() {
    for (unsigned late = 0; late < 2; ++late) {
        Rig r;
        r.prerequisites();
        r.deadline = r.now + 10;
        L::ActionContext action;
        r.prepare(action);
        const L::PreparedAction work = r.next(action);
        const E::RequestId id = r.admit(work.transfer, true);
        r.turn(r.now);
        r.wire.push(reply(work.transfer), r.deadline - 1);
        r.turn(r.deadline - (late ? 0 : 1), budget(8, 0, 8, 0));
        const C::SdoCompletion terminal = r.done(id).service;
        CHECK(terminal.outcome == (late ? C::SdoOutcome::TIMEOUT : C::SdoOutcome::SUCCESS));
        // Even an earlier published ACK advanced at the exact action cutoff
        // cannot claim a completed observation or extend the operation.
        CHECK(L::advanceAction(action, r.reservation, target(), terminal, r.deadline));
        CHECK(action.state == M::ActionState::FAILED &&
              action.outcome == M::ActionOutcome::DEADLINE && action.uncertain &&
              action.completion == M::ActionCompletion::NOT_OBSERVED);
        CHECK(action.execution ==
              (late ? M::ActionExecution::UNKNOWN : M::ActionExecution::ACKNOWLEDGED));
        CHECK(r.wire.sentCount == 7);
    }
    for (unsigned kind = 0; kind < 2; ++kind) {
        Rig r;
        r.prerequisites();
        L::ActionContext first;
        r.prepare(first, kind ? L::StopKind::QUICK_STOP : L::StopKind::HALT);
        r.field(first, 0x6040);
        r.cycle(first);
        const unsigned sent = r.wire.sentCount;
        // Real latest control evidence is now the stop itself. Passing an old
        // discarded000F instead would violate the continuous-history contract.
        L::ActionContext repeated;
        const Saved<L::ActionContext> unchanged(repeated);
        CHECK(r.control.transfer.data[0] == (kind ? 11 : 15) &&
              r.control.transfer.data[1] == (kind ? 0 : 1));
        CHECK(!L::prepareStop(repeated, r.reservation, first.read, r.control, first.kind,
                              r.operation + 1, r.now, r.deadline, r.actionOptions()));
        unchanged.unchanged(repeated);
        CHECK(r.wire.sentCount == sent);
    }
}
void freshnessAndStepExhaustion() {
    Rig fresh;
    fresh.prerequisites();
    fresh.begin();
    L::ActionOptions policy = fresh.actionOptions();
    policy.maximumAgeUs = fresh.now - fresh.control.admittedUs;
    L::ActionContext action;
    CHECK(L::prepareStop(action, fresh.reservation, fresh.state, fresh.control, L::StopKind::HALT,
                         fresh.operation, fresh.now, fresh.deadline, policy));
    --policy.maximumAgeUs;
    L::ActionContext rejected;
    const Saved<L::ActionContext> unchanged(rejected);
    CHECK(!L::prepareStop(rejected, fresh.reservation, fresh.state, fresh.control,
                          L::StopKind::HALT, fresh.operation, fresh.now, fresh.deadline, policy));
    unchanged.unchanged(rejected);

    for (unsigned running = 0; running < 2; ++running) {
        Rig r;
        r.prerequisites();
        L::ActionContext boundary;
        r.prepare(boundary);
        r.field(boundary, 0x6040);
        // Synthetic host arithmetic boundary, not billions of bus operations.
        // Each following frame still passes real owner/service correlation.
        boundary.step = UINT32_MAX - 4;
        r.cycle(boundary, running ? 4 : 0);
        CHECK(boundary.step == UINT32_MAX && boundary.state != M::ActionState::ACTIVE);
        if (running) {
            CHECK(boundary.outcome == M::ActionOutcome::ITERATION_EXHAUSTED && boundary.uncertain &&
                  boundary.completion == M::ActionCompletion::NOT_OBSERVED);
        } else {
            CHECK(boundary.outcome == M::ActionOutcome::OBSERVED &&
                  boundary.completion == M::ActionCompletion::OBSERVED);
        }
        CHECK(boundary.execution == M::ActionExecution::ACKNOWLEDGED && r.wire.sentCount == 12);
        CHECK(!r.next(boundary).ready);
    }
}
void everyObservationFieldCanFailIndependently() {
    const uint16_t indices[] = {0x2100, 0x2101, 0x2102, 0x6041, 0x6061};
    const uint32_t values[] = {0, 0xa580, 0xf008, 7, 1};
    for (unsigned failed = 0; failed < 5; ++failed)
        for (unsigned abort = 0; abort < 2; ++abort) {
            Rig r;
            r.prerequisites();
            L::ActionContext action;
            r.prepare(action);
            r.field(action, 0x6040);
            const Saved<C::SdoCompletion> command(action.commandEvidence);
            for (unsigned i = 0; i < failed; ++i)
                r.field(action, indices[i], values[i]);
            const L::PreparedAction work = r.next(action);
            CHECK(work.ready && work.transfer.index == indices[failed]);
            const E::RequestId id = r.admit(work.transfer, true);
            r.turn(r.now);
            C::SdoCompletion terminal;
            if (abort)
                terminal = r.complete(id, work.transfer, 0x08000023, true, false);
            else {
                CHECK(r.owner.cancel(id, r.now) == E::Cancel::CANCELLED);
                terminal = r.done(id).service;
            }
            CHECK(L::advanceAction(action, r.reservation, target(), terminal, r.now));
            CHECK(action.state == M::ActionState::FAILED && action.uncertain &&
                  action.execution == M::ActionExecution::ACKNOWLEDGED &&
                  action.completion == M::ActionCompletion::NOT_OBSERVED &&
                  action.outcome ==
                      (abort ? M::ActionOutcome::PROTOCOL_REJECTED : M::ActionOutcome::CANCELLED));
            CHECK(action.observationEvidencePresent &&
                  action.observationEvidence.transfer.index == indices[failed] &&
                  action.observationEvidence.transfer.step == failed + 2 &&
                  action.observationEvidence.abortValid == bool(abort));
            if (abort)
                CHECK(action.observationEvidence.abortCode == 0x08000023);
            command.unchanged(action.commandEvidence);
            const Saved<L::ActionContext> closed(action);
            const Saved<E::Completion> retained(r.done(id));
            const unsigned count = r.wire.sentCount;
            r.wire.push(reply(work.transfer, values[failed]), r.now + 1);
            r.turn(r.now + 1);
            closed.unchanged(action);
            retained.unchanged(r.done(id));
            CHECK(!r.next(action).ready && r.wire.sentCount == count);
        }
}

void cancelBetweenObservationCycles() {
    Rig r;
    r.prerequisites();
    L::ActionContext action;
    r.prepare(action);
    r.field(action, 0x6040);
    r.cycle(action, 4);
    CHECK(action.phase == L::ActionPhase::WAITING && action.eligibleUs > r.now + 1);
    const Saved<C::SdoCompletion> command(action.commandEvidence);
    const Saved<C::SdoCompletion> observation(action.observationEvidence);
    const Saved<L::ReadContext> unadmittedRead(action.read);
    const unsigned count = r.wire.sentCount;
    CHECK(
        L::advanceAction(action, r.reservation, target(), M::ActionOutcome::CANCELLED, r.now + 1));
    CHECK(action.state == M::ActionState::FAILED && action.outcome == M::ActionOutcome::CANCELLED &&
          action.execution == M::ActionExecution::ACKNOWLEDGED && action.uncertain &&
          action.completion == M::ActionCompletion::NOT_OBSERVED);
    command.unchanged(action.commandEvidence);
    observation.unchanged(action.observationEvidence);
    unadmittedRead.unchanged(action.read); // No fabricated cancellation/effect of an unsent read.
    r.turn(action.eligibleUs);
    CHECK(!r.next(action).ready && r.wire.sentCount == count);
}

void overlappingInputsAndInvalidPoliciesAreAtomic() {
    Rig r;
    r.prerequisites();
    L::ActionContext action;
    r.prepare(action);
    const Saved<L::ActionContext> initial(action);
    // Actual declared subobjects deliberately alias the mutable whole object;
    // these cases require neither type punning nor misaligned storage.
    CHECK(!L::prepareStop(action, action.reservation, r.state, r.control, action.kind, r.operation,
                          r.now, r.deadline, r.actionOptions()));
    initial.unchanged(action);
    CHECK(!L::prepareStop(action, r.reservation, action.read, r.control, action.kind, r.operation,
                          r.now, r.deadline, r.actionOptions()));
    initial.unchanged(action);
    CHECK(!L::prepareStop(action, r.reservation, r.state, action.controlEvidence, action.kind,
                          r.operation, r.now, r.deadline, r.actionOptions()));
    initial.unchanged(action);
    CHECK(!L::prepareStop(action, r.reservation, r.state, r.control, action.kind, r.operation,
                          r.now, r.deadline, action.options));
    initial.unchanged(action);
    CHECK(!L::noteActionTransmission(action, action.reservation, target(), r.now));
    initial.unchanged(action);
    CHECK(!L::noteActionTransmission(action, r.reservation, action.reservation.target, r.now));
    initial.unchanged(action);
    CHECK(!L::advanceAction(action, r.reservation, target(), action.commandEvidence, r.now));
    initial.unchanged(action);
    CHECK(!L::advanceAction(action, action.reservation, target(), M::ActionOutcome::CANCELLED,
                            r.now));
    initial.unchanged(action);
    CHECK(!L::advanceAction(action, r.reservation, action.reservation.target,
                            M::ActionOutcome::CANCELLED, r.now));
    initial.unchanged(action);
    for (unsigned invalid = 0; invalid < 5; ++invalid) {
        L::ActionOptions policy = r.actionOptions();
        uint32_t operation = r.operation;
        uint64_t deadline = r.deadline;
        if (invalid == 0)
            policy.service.queueTimeoutUs = 0;
        if (invalid == 1)
            policy.service.responseTimeoutUs = 0;
        if (invalid == 2)
            policy.maximumAgeUs = 0;
        if (invalid == 3)
            operation = 0;
        if (invalid == 4)
            deadline = r.now;
        CHECK(!L::prepareStop(action, r.reservation, r.state, r.control, action.kind, operation,
                              r.now, deadline, policy));
        initial.unchanged(action);
    }
    CHECK(!L::advanceAction(action, r.reservation, target(), M::ActionOutcome::OBSERVED, r.now));
    initial.unchanged(action);
    CHECK(!L::advanceAction(action, r.reservation, target(), M::ActionOutcome::DEADLINE, r.now));
    initial.unchanged(action);
    CHECK(r.wire.sentCount == 6);
}

void nearMaximumTimeAndBackwardCalls() {
    Rig r;
    r.now = UINT64_MAX - 30;
    r.deadline = UINT64_MAX;
    r.prerequisites();
    L::ActionContext action;
    r.prepare(action, L::StopKind::QUICK_STOP, UINT64_MAX);
    const Saved<L::ActionContext> initial(action);
    L::PreparedAction work;
    work.eligibleUs = 123;
    const Saved<L::PreparedAction> unchanged(work);
    CHECK(!L::nextAction(action, r.reservation, target(), r.now - 1, work));
    CHECK(!L::noteActionTransmission(action, r.reservation, target(), r.now - 1));
    CHECK(
        !L::advanceAction(action, r.reservation, target(), M::ActionOutcome::CANCELLED, r.now - 1));
    unchanged.unchanged(work);
    initial.unchanged(action);
    r.field(action, 0x6040);
    r.cycle(action, 4);
    CHECK(action.phase == L::ActionPhase::WAITING && action.eligibleUs == UINT64_MAX &&
          !r.next(action).ready);
    const unsigned count = r.wire.sentCount;
    r.turn(UINT64_MAX - 1, E::ServiceBudget());
    CHECK(!r.next(action).ready);
    CHECK(
        L::advanceAction(action, r.reservation, target(), M::ActionOutcome::CANCELLED, UINT64_MAX));
    CHECK(action.state == M::ActionState::FAILED && action.outcome == M::ActionOutcome::DEADLINE &&
          action.execution == M::ActionExecution::ACKNOWLEDGED && action.uncertain);
    r.turn(UINT64_MAX, E::ServiceBudget());
    CHECK(!r.next(action).ready && r.wire.sentCount == count);
}

void uncertainEnqueueRefusalAndObservedEmcy() {
    Rig refused;
    refused.prerequisites();
    L::ActionContext failure;
    refused.prepare(failure);
    const L::PreparedAction work = refused.next(failure);
    const E::RequestId id = refused.admit(work.transfer, true);
    refused.wire.accept = refused.wire.autoReport = refused.wire.retirement = false;
    const unsigned pending = refused.wire.sentCount;
    refused.turn(refused.now);
    const C::SdoCompletion result = refused.done(id).service;
    CHECK(!refused.done(id).controllerAccepted && refused.wire.pending[pending] &&
          result.outcome == C::SdoOutcome::TRANSPORT_ERROR && result.effectUncertain);
    CHECK(L::advanceAction(failure, refused.reservation, target(), result, refused.now));
    CHECK(failure.outcome == M::ActionOutcome::TRANSPORT_ERROR &&
          failure.execution == M::ActionExecution::UNKNOWN && failure.uncertain);
    const Saved<L::ActionContext> closed(failure);
    const Saved<E::Completion> retained(refused.done(id));
    refused.turn(refused.now + 1, budget(0, 0, 8, 0));
    CHECK(refused.wire.retireCalls == 1 && refused.wire.pending[pending]);
    // False from enqueue did not prove the controller rejected the frame.
    // Its later positive report still cannot rewrite the failed operation.
    refused.wire.report(pending, true);
    refused.turn(refused.now + 1, budget(0, 0, 8, 0));
    closed.unchanged(failure);
    retained.unchanged(refused.done(id));
    CHECK(refused.services.unresolved(0));

    Rig fault;
    fault.prerequisites();
    L::ActionContext action;
    fault.prepare(action);
    fault.field(action, 0x6040);
    const Saved<C::SdoCompletion> command(action.commandEvidence);
    const Saved<L::ActionContext> beforeEvent(action);
    M::Frame emcy = frame(0x81, 8);
    emcy.data[0] = 0x23;
    emcy.data[1] = 0x81;
    emcy.data[2] = 0x11;
    emcy.data[7] = 0xa5;
    fault.wire.push(emcy, fault.now + 1);
    fault.turn(fault.now + 1, budget(8, 0, 8, 0));
    const auto* node = fault.services.node(0);
    CHECK(node && node->emcyCount == 1 && node->emcy.errorCode == 0x8123 &&
          node->emcy.errorRegister == 0x11 && node->emcy.manufacturer[4] == 0xa5);
    beforeEvent.unchanged(action); // Owner caches the event; application decides interruption.
    CHECK(L::advanceAction(action, fault.reservation, target(), M::ActionOutcome::DRIVE_FAULT,
                           fault.now));
    CHECK(action.state == M::ActionState::FAILED &&
          action.outcome == M::ActionOutcome::DRIVE_FAULT &&
          action.execution == M::ActionExecution::ACKNOWLEDGED && action.uncertain);
    command.unchanged(action.commandEvidence);
    CHECK(fault.wire.sentCount == 7 && fault.services.node(0)->emcy.errorCode == 0x8123);
}
} // namespace

int main() {
    exactCommandsAndObservedReport();
    runningPollingAndDeadline();
    unsupportedPrerequisitesAreAtomic();
    contradictoryObservationsAndAbort();
    cancellationAndLostReplies();
    interruptQueuedWorkPreservesResultsAndFencesOldOperations();
    reservedUrgentCapacity();
    quarantineAndOtherNodeProgress();
    generationsRebootFaultAndCorrelation();
    exactDeadlineAndRepeatedStopRejection();
    freshnessAndStepExhaustion();
    everyObservationFieldCanFailIndependently();
    cancelBetweenObservationCycles();
    overlappingInputsAndInvalidPoliciesAreAtomic();
    nearMaximumTimeAndBackwardCalls();
    uncertainEnqueueRefusalAndObservedEmcy();
    std::printf("stop action: %u checks\n", checks);
}
