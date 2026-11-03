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
using OwnerRig::budget;
using OwnerRig::frame;
template <class T> struct Saved {
    unsigned char bytes[sizeof(T)];
    explicit Saved(const T& value) { std::memcpy(bytes, &value, sizeof value); }
    // Report the caller and the first differing byte: a padding difference shows only here.
    void unchanged(const T& value, int line = __builtin_LINE()) const {
        ++checks;
        const unsigned char* now = reinterpret_cast<const unsigned char*>(&value);
        for (size_t i = 0; i < sizeof value; ++i)
            if (bytes[i] != now[i]) {
                std::fprintf(stderr, "%s:%d: byte %u of %u changed\n", __FILE__, line, unsigned(i), unsigned(sizeof value));
                std::abort();
            }
    }
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
struct Wire : OwnerRig::WireBase {
    L::ActionContext* action = nullptr;
    const L::AxisReservation* reservation = nullptr;
    C::SdoCompletion* latestControl = nullptr;
    static bool guard(void* context, O::SequenceId, const C::SdoTransfer& work, uint64_t now) {
        Wire& w = *static_cast<Wire*>(static_cast<OwnerRig::WireBase*>(context));
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
    O::Transport callbacks() {
        O::Transport value = plumbing();
        value.beforeTransmit = guard;
        return value;
    }
};
struct Rig {
    C::SdoClient services;
    Wire wire;
    O::CanBusOwner owner;
    O::SequenceId sequence;
    L::AxisReservation reservation;
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
    void turn(uint64_t time, const O::ServiceBudget& work = budget()) {
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
    O::RequestId admit(const C::SdoTransfer& transfer, bool urgent = false) {
        O::BusRequest work;
        work.sequence = sequence;
        work.transfer = transfer;
        O::RequestId id;
        CHECK((urgent ? owner.admitUrgent(work, now, id) : owner.admit(work, now, id)) ==
              O::BusAdmission::ACCEPTED);
        return id;
    }
    const O::Completion& done(O::RequestId id) const {
        const O::Completion* value = owner.result(id);
        CHECK(value);
        return *value;
    }
    C::SdoCompletion complete(O::RequestId id, const C::SdoTransfer& work, uint32_t raw = 0,
                              bool abort = false, bool release = true) {
        wire.push(reply(work, raw, abort), now + 1);
        turn(now + 1, budget(8, 0, 8, 0));
        const C::SdoCompletion value = done(id).service;
        if (release)
            CHECK(owner.release(id));
        return value;
    }
    C::SdoCompletion exchange(const C::SdoTransfer& work, uint32_t raw = 0) {
        const O::RequestId id = admit(work);
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
        const O::RequestId id = admit(work.transfer, true);
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
            const Saved<C::SdoCompletion> command(action.commandHistory[0]);
            // High status bits, including conflicting source dialect flags, do
            // not decide this narrow monitor-based completion contract.
            r.cycle(action, 0xff80, 0xffff);
            CHECK(action.state == M::ActionState::SUCCEEDED &&
                  action.outcome == M::ActionOutcome::OBSERVED &&
                  action.completion == M::ActionCompletion::OBSERVED && action.observationKnown &&
                  action.observations == 1);
            CHECK(r.wire.sentCount == before + 6);
            command.unchanged(action.commandHistory[0]);
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
    r.turn(eligible - 1, O::ServiceBudget());
    CHECK(!r.next(action).ready && r.wire.sentCount == before);
    r.turn(eligible, O::ServiceBudget());
    CHECK(r.next(action).ready && r.next(action).transfer.step == 7);
    r.cycle(action, 4);
    CHECK(action.observations == 2 && action.state == M::ActionState::ACTIVE);
    r.turn(r.deadline, O::ServiceBudget());
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
        L::AxisReservation reservation = r.reservation;
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
    // DEC-8 table, whole words only. These fixture words are this fake's, not qualified behavior.
    const uint16_t words[] = {0x000f, 0x001f, 0x005f, 0x003f, 0x007f, 0x010f, 0x000b, 0x0007,
                              0x0006, 0x0000, 0x011f, 0x002f, 0x004f, 0x000e, 0x8000};
    for (uint16_t word : words)
        for (unsigned kind = 0; kind < 2; ++kind) {
            const L::StopKind selected = kind ? L::StopKind::QUICK_STOP : L::StopKind::HALT;
            const bool trigger = word == 0x001f || word == 0x005f || word == 0x003f || word == 0x007f;
            const bool allowed =
                word == 0x000f || trigger || (word == 0x010f && selected == L::StopKind::QUICK_STOP);
            CHECK(L::stopPredecessorAllowed(word, selected) == allowed);
            Rig r;
            r.prerequisites();
            r.begin();
            C::SdoCompletion control = r.control;
            control.transfer.data[0] = uint8_t(word);
            control.transfer.data[1] = uint8_t(word >> 8);
            L::ActionContext action;
            action.operation = 777;
            const Saved<L::ActionContext> before(action);
            const M::Status status =
                L::prepareStop(action, r.reservation, r.state, control, selected, r.operation, r.now,
                               r.deadline, r.actionOptions());
            CHECK(bool(status) == allowed);
            if (allowed)
                CHECK(action.predecessorWord == word &&
                      action.predecessorDocumented == (word == 0x000f) &&
                      action.predecessorBasis ==
                          (word != 0x000f ? L::PredecessorBasis::PROJECT_DECISION
                           : kind         ? L::PredecessorBasis::EN86_ZH86_OP86
                                          : L::PredecessorBasis::ZH86_OP86) &&
                      action.state == M::ActionState::ACTIVE);
            else
                before.unchanged(action);
        }
}
void contradictoryObservationsAndAbort() {
    for (unsigned scenario = 0; scenario < 4; ++scenario) {
        Rig r;
        r.prerequisites();
        L::ActionContext action;
        r.prepare(action, L::StopKind::QUICK_STOP);
        r.field(action, 0x6040);
        const Saved<C::SdoCompletion> command(action.commandHistory[0]);
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
        command.unchanged(action.commandHistory[0]);
    }
    Rig rejected;
    rejected.prerequisites();
    L::ActionContext action;
    rejected.prepare(action);
    rejected.field(action, 0x6040, 0x08000022, true);
    CHECK(action.state == M::ActionState::FAILED &&
          action.outcome == M::ActionOutcome::PROTOCOL_REJECTED &&
          action.execution == M::ActionExecution::REJECTED && action.commandHistory[0].abortValid &&
          action.commandHistory[0].abortCode == 0x08000022);
    CHECK(rejected.wire.sentCount == 7);
}

void cancellationAndLostReplies() {
    for (unsigned phase = 0; phase < 4; ++phase) {
        Rig r;
        r.prerequisites();
        L::ActionContext action;
        r.prepare(action);
        const L::PreparedAction work = r.next(action);
        const O::RequestId id = r.admit(work.transfer, true);
        const unsigned before = r.wire.sentCount;
        if (phase) {
            r.wire.autoReport = phase > 1;
            r.turn(r.now);
            if (phase == 2)
                r.turn(r.now + 1, budget(0, 0, 8, 0));
        }
        if (phase == 3) {
            r.turn(r.now + 100, O::ServiceBudget());
            CHECK(r.done(id).service.outcome == C::SdoOutcome::TIMEOUT);
        } else {
            CHECK(r.owner.cancel(id, r.now) == O::Cancel::CANCELLED);
        }
        const C::SdoCompletion original = r.done(id).service;
        CHECK(original.transmissionPossible == (phase != 0));
        CHECK(L::advanceAction(action, r.reservation, target(), original, r.now));
        r.control = original;
        CHECK(action.state == M::ActionState::FAILED &&
              action.execution ==
                  (phase ? M::ActionExecution::UNKNOWN : M::ActionExecution::NOT_TRANSMITTED));
        CHECK(action.uncertain == (phase != 0));
        const Saved<O::Completion> retained(r.done(id));
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
    const O::RequestId first = r.admit(oldA), second = r.admit(oldB);
    CHECK(!r.owner.beginOperation(r.sequence, r.operation + 1, r.deadline, r.now));
    CHECK(r.owner.cancel(first, r.now) == O::Cancel::CANCELLED);
    CHECK(r.owner.cancel(second, r.now) == O::Cancel::CANCELLED);
    const Saved<O::Completion> savedA(r.done(first)), savedB(r.done(second));
    L::ActionContext stop;
    r.prepare(stop);
    O::BusRequest stale;
    stale.sequence = r.sequence;
    stale.transfer = oldA;
    O::RequestId rejected;
    rejected.generation = 999;
    const Saved<O::RequestId> unchanged(rejected);
    CHECK(r.owner.admit(stale, r.now, rejected) == O::BusAdmission::INVALID);
    unchanged.unchanged(rejected);
    const unsigned before = r.wire.sentCount;
    r.field(stop, 0x6040);
    r.cycle(stop);
    CHECK(stop.state == M::ActionState::SUCCEEDED && r.wire.sentCount == before + 6);
    savedA.unchanged(r.done(first));
    savedB.unchanged(r.done(second));
    CHECK(r.owner.admit(stale, r.now, rejected) == O::BusAdmission::INVALID);
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
    O::RequestId original[O::CanBusOwner::REQUESTS];
    for (unsigned i = 0; i < O::CanBusOwner::REQUESTS; ++i) {
        const C::SdoTransfer work = r.transfer(0x2101, i + 1);
        original[i] = r.admit(work);
        CHECK(r.owner.cancel(original[i], r.now) == O::Cancel::CANCELLED);
    }
    L::ActionContext action;
    r.prepare(action);
    const L::PreparedAction work = r.next(action);
    O::BusRequest request;
    request.sequence = r.sequence;
    request.transfer = work.transfer;
    O::RequestId id;
    CHECK(r.owner.admit(request, r.now, id) == O::BusAdmission::RESULTS_FULL);
    id = r.admit(work.transfer, true);
    request.transfer.step = 2;
    O::RequestId refused;
    CHECK(r.owner.admitUrgent(request, r.now, refused) == O::BusAdmission::RESULTS_FULL);
    r.turn(r.now);
    const C::SdoCompletion ack = r.complete(id, work.transfer, 0, false, false);
    CHECK(L::advanceAction(action, r.reservation, target(), ack, r.now));
    const Saved<O::Completion> retained(r.done(id));
    request.transfer = r.next(action).transfer;
    CHECK(r.owner.admitUrgent(request, r.now, refused) == O::BusAdmission::RESULTS_FULL);
    retained.unchanged(r.done(id));
    CHECK(r.owner.release(id));
    r.cycle(action);
    CHECK(action.state == M::ActionState::SUCCEEDED);
    for (O::RequestId old : original)
        CHECK(r.done(old).reason == O::CompletionReason::CANCELLED &&
              !r.done(old).service.transmissionPossible);
}

void quarantineAndOtherNodeProgress() {
    for (unsigned retired = 0; retired < 2; ++retired) {
        Rig r;
        r.prerequisites();
        r.begin();
        const C::SdoTransfer oldWork = r.transfer(0x2101);
        const O::RequestId old = r.admit(oldWork);
        r.wire.autoReport = false;
        r.wire.retirement = retired != 0;
        const unsigned last = r.wire.sentCount;
        r.turn(r.now);
        CHECK(r.wire.pending[last]);
        CHECK(r.owner.cancel(old, r.now) == O::Cancel::CANCELLED);
        const Saved<O::Completion> previous(r.done(old));
        CHECK(r.done(old).service.effectUncertain && r.services.unresolved(0));
        CHECK(r.wire.pending[last] == !retired && r.wire.retireCalls == 1);
        CHECK(!r.owner.sequence(r.sequence));
        CHECK(!r.owner.beginOperation(r.sequence, r.operation + 1, r.deadline, r.now));
        O::BusRequest blocked;
        blocked.sequence = r.sequence;
        blocked.transfer = oldWork;
        ++blocked.transfer.step;
        O::RequestId refused;
        CHECK(r.owner.admitUrgent(blocked, r.now, refused) == O::BusAdmission::INVALID);
        // The old channel cannot interpret an identical late reply as fresh
        // stop confirmation, regardless of local retirement success.
        r.wire.push(reply(oldWork, 0), r.now + 1);
        r.turn(r.now + 1, budget(8, 0, 8, 0));
        previous.unchanged(r.done(old));
        CHECK(r.services.unresolved(0));

        O::SequenceId second;
        CHECK(r.owner.beginSequence(target(2).channel.binding, 2, r.now, second));
        CHECK(r.owner.beginOperation(second, 50, r.deadline, r.now));
        C::SdoTransfer fresh = oldWork;
        fresh.channel = target(2).channel;
        fresh.operation = fresh.request = 50;
        O::BusRequest request;
        request.sequence = second;
        request.transfer = fresh;
        O::RequestId other;
        r.wire.autoReport = true;
        CHECK(r.owner.admitUrgent(request, r.now, other) == O::BusAdmission::ACCEPTED);
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
        L::AxisReservation live = r.reservation;
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
        const Saved<C::SdoCompletion> command(action.commandHistory[0]);
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
        command.unchanged(action.commandHistory[0]);
        CHECK(r.wire.sentCount == 7);
    }
    Rig r;
    r.prerequisites();
    L::ActionContext action;
    r.prepare(action);
    const L::PreparedAction work = r.next(action);
    const O::RequestId id = r.admit(work.transfer, true);
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
        const O::RequestId id = r.admit(work.transfer, true);
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
        // R-14 escalation: only an acknowledged 010F, and only for 000B. A prior 000B admits neither.
        L::ActionContext escalated;
        const Saved<L::ActionContext> untouched(escalated);
        const M::Status status = L::prepareStop(escalated, r.reservation, first.read, r.control,
                                                L::StopKind::QUICK_STOP, r.operation + 1, r.now,
                                                r.deadline, r.actionOptions());
        CHECK(bool(status) == (kind == 0));
        if (kind == 0)
            CHECK(escalated.predecessorWord == 0x010f && !escalated.predecessorDocumented);
        else
            untouched.unchanged(escalated);
        CHECK(r.wire.sentCount == sent);
    }
}
// DEC-3: a marker reopens the route, not the reservation or its 000F. The ordered server here is
// a fake premise: this proves the host's logic, not a real server's ordering (Q18).
void drainMarkerConsumesOld000F() {
    Rig r;
    r.prerequisites();
    const C::SdoCompletion old000F = r.control;
    CHECK(old000F.protocolAcknowledged && old000F.transfer.index == 0x6040);
    r.begin();
    const O::RequestId lostId = r.admit(r.transfer(0x2200, 1, true));
    r.turn(r.now);
    r.turn(r.now + 100, budget(0, 0, 8, 0));
    const C::SdoCompletion lost = r.done(lostId).service;
    CHECK(lost.outcome == C::SdoOutcome::TIMEOUT && lost.effectUncertain);
    CHECK(r.services.unresolved(0) && !r.owner.sequence(r.sequence));
    CHECK(r.owner.release(lostId));
    const unsigned sent = r.wire.sentCount;
    C::SdoTransfer marker = r.transfer(0x2000);
    marker.readEffect = C::SdoReadEffect::ACCEPTED_UNQUALIFIED;
    marker.allowSegmentedUpload = false;
    C::SdoDrainPremises premises;
    premises.soleClient = premises.orderedServer = true;
    O::RequestId markerId;
    CHECK(r.owner.admitDrainMarker(marker, premises, r.now, markerId) == O::BusAdmission::ACCEPTED);
    r.turn(r.now);
    CHECK(r.wire.sentCount == sent + 1 && r.wire.sent[sent].data[1] == 0x00 &&
          r.wire.sent[sent].data[2] == 0x20);
    r.wire.push(reply(marker, 0x1234), r.now + 1);
    r.turn(r.now + 1, budget(8, 0, 8, 0));
    CHECK(r.done(markerId).service.outcome == C::SdoOutcome::SUCCESS);
    CHECK(!r.services.unresolved(0) && r.services.requalifications(0) == 1);
    // The reservation holding that 000F stays dead. Its copied values alone admit nothing.
    CHECK(!r.owner.sequence(r.sequence));
    CHECK(!r.owner.beginOperation(r.sequence, r.operation + 1, r.deadline, r.now));
    O::BusRequest stop;
    stop.sequence = r.sequence;
    stop.transfer = r.transfer(0x6040, 1, true);
    stop.transfer.data[0] = 0x0f;
    stop.transfer.data[1] = 1;
    O::RequestId refused;
    CHECK(r.owner.admitUrgent(stop, r.now, refused) == O::BusAdmission::INVALID);
    CHECK(r.wire.sentCount == sent + 1);
    // A new reservation cannot adopt the 000F acknowledged before it was acquired.
    CHECK(r.owner.release(markerId) && r.owner.releaseSequence(r.sequence));
    r.turn(r.now + 1, O::ServiceBudget());
    CHECK(r.owner.beginSequence(target().channel.binding, 1, r.now, r.sequence));
    r.reservation.sequence = uint32_t(r.sequence.slot + 1);
    r.reservation.generation = r.sequence.generation;
    r.reservation.acquiredUs = r.owner.sequence(r.sequence)->acquiredUs;
    r.acquire();
    r.begin();
    L::ActionContext action;
    const Saved<L::ActionContext> unchanged(action);
    CHECK(!L::prepareStop(action, r.reservation, r.state, old000F, L::StopKind::HALT, r.operation,
                          r.now, r.deadline, r.actionOptions()));
    unchanged.unchanged(action);
    // Only a 000F acknowledged under the new reservation qualifies it again.
    r.prerequisites();
    r.begin();
    CHECK(L::prepareStop(action, r.reservation, r.state, r.control, L::StopKind::HALT, r.operation,
                         r.now, r.deadline, r.actionOptions()));
    CHECK(r.control.completedUs > old000F.completedUs);
}
void freshnessAndStepExhaustion() {
    // An acknowledged word does not expire while the drive idles. Fresh reads still do.
    Rig fresh;
    fresh.prerequisites();
    const L::ActionOptions policy = fresh.actionOptions();
    fresh.turn(fresh.control.admittedUs + policy.maximumAgeUs + 5000, O::ServiceBudget());
    fresh.acquire();
    fresh.begin();
    CHECK(fresh.now - fresh.control.admittedUs > policy.maximumAgeUs);
    L::ActionContext action;
    CHECK(L::prepareStop(action, fresh.reservation, fresh.state, fresh.control, L::StopKind::HALT,
                         fresh.operation, fresh.now, fresh.deadline, policy));
    const uint64_t earliest = fresh.state.values[0].provenance.earliestUs;
    fresh.turn(earliest + policy.maximumAgeUs, O::ServiceBudget());
    CHECK(L::prepareStop(action, fresh.reservation, fresh.state, fresh.control, L::StopKind::HALT,
                         fresh.operation, fresh.now, fresh.deadline, policy));
    fresh.turn(fresh.now + 1, O::ServiceBudget());
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
            const Saved<C::SdoCompletion> command(action.commandHistory[0]);
            for (unsigned i = 0; i < failed; ++i)
                r.field(action, indices[i], values[i]);
            const L::PreparedAction work = r.next(action);
            CHECK(work.ready && work.transfer.index == indices[failed]);
            const O::RequestId id = r.admit(work.transfer, true);
            r.turn(r.now);
            C::SdoCompletion terminal;
            if (abort)
                terminal = r.complete(id, work.transfer, 0x08000023, true, false);
            else {
                CHECK(r.owner.cancel(id, r.now) == O::Cancel::CANCELLED);
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
            command.unchanged(action.commandHistory[0]);
            const Saved<L::ActionContext> closed(action);
            const Saved<O::Completion> retained(r.done(id));
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
    const Saved<C::SdoCompletion> command(action.commandHistory[0]);
    const Saved<C::SdoCompletion> observation(action.observationEvidence);
    const Saved<L::ReadContext> unadmittedRead(action.read);
    const unsigned count = r.wire.sentCount;
    CHECK(
        L::advanceAction(action, r.reservation, target(), M::ActionOutcome::CANCELLED, r.now + 1));
    CHECK(action.state == M::ActionState::FAILED && action.outcome == M::ActionOutcome::CANCELLED &&
          action.execution == M::ActionExecution::ACKNOWLEDGED && action.uncertain &&
          action.completion == M::ActionCompletion::NOT_OBSERVED);
    command.unchanged(action.commandHistory[0]);
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
    CHECK(!L::advanceAction(action, r.reservation, target(), action.commandHistory[0], r.now));
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
    r.turn(UINT64_MAX - 1, O::ServiceBudget());
    CHECK(!r.next(action).ready);
    CHECK(
        L::advanceAction(action, r.reservation, target(), M::ActionOutcome::CANCELLED, UINT64_MAX));
    CHECK(action.state == M::ActionState::FAILED && action.outcome == M::ActionOutcome::DEADLINE &&
          action.execution == M::ActionExecution::ACKNOWLEDGED && action.uncertain);
    r.turn(UINT64_MAX, O::ServiceBudget());
    CHECK(!r.next(action).ready && r.wire.sentCount == count);
}

void uncertainEnqueueRefusalAndObservedEmcy() {
    Rig refused;
    refused.prerequisites();
    L::ActionContext failure;
    refused.prepare(failure);
    const L::PreparedAction work = refused.next(failure);
    const O::RequestId id = refused.admit(work.transfer, true);
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
    const Saved<O::Completion> retained(refused.done(id));
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
    const Saved<C::SdoCompletion> command(action.commandHistory[0]);
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
    command.unchanged(action.commandHistory[0]);
    CHECK(fault.wire.sentCount == 7 && fault.services.node(0)->emcy.errorCode == 0x8123);
}
void identityWidthsRefusedAtAdmission() {
    Rig r;
    r.prerequisites();
    L::ActionOptions measured = r.actionOptions();
    measured.service.identityWidths = L::IdentityWidths::MEASURED_U8;
    CHECK(L::stopOptionsValid(r.actionOptions()) && !L::stopOptionsValid(measured));
    L::ActionContext action;
    const Saved<L::ActionContext> empty(action);
    r.begin();
    const M::Status refused = L::prepareStop(action, r.reservation, r.state, r.control,
                                             L::StopKind::HALT, r.operation, r.now, r.deadline, measured);
    CHECK(!refused && refused.detail == int32_t(M::ActionError::ARGUMENT));
    empty.unchanged(action);
    CHECK(!L::validateStop(action, r.reservation, r.state, r.control, L::StopKind::HALT, r.operation,
                           r.now, r.deadline, measured));
    L::ActionContext running;
    CHECK(!L::prepareEscalation(action, running, r.operation, r.now, r.deadline, measured));
    empty.unchanged(action);
}
void validationMatchesPreparationAndChangesNothing() {
    Rig r;
    r.prerequisites();
    L::ActionContext action;
    const Saved<L::ActionContext> empty(action);
    const Saved<L::ReadContext> state(r.state);
    r.begin();
    CHECK(L::validateStop(action, r.reservation, r.state, r.control, L::StopKind::HALT, r.operation,
                          r.now, r.deadline, r.actionOptions()));
    empty.unchanged(action);
    state.unchanged(r.state);
    CHECK(L::prepareStop(action, r.reservation, r.state, r.control, L::StopKind::HALT, r.operation,
                         r.now, r.deadline, r.actionOptions()));
    CHECK(!action.incident.present && action.extraObservation.fieldCount == 0);
    CHECK(action.statusObservation.statusword == 7 && action.monitorObservation.monitor == 4);
    // Output overlap is part of validation, so a caller learns it before anything moves.
    const Saved<L::ActionContext> prepared(action);
    const M::Status overlap = L::validateStop(action, r.reservation, action.read, r.control,
                                              L::StopKind::HALT, r.operation, r.now, r.deadline,
                                              r.actionOptions());
    CHECK(!overlap && overlap.detail == int32_t(M::ActionError::OVERLAP));
    prepared.unchanged(action);
    const M::Status self = L::validateEscalation(action, action, r.operation, r.now, r.deadline,
                                                 r.actionOptions());
    CHECK(!self && self.detail == int32_t(M::ActionError::OVERLAP));
    L::resetAction(action);
    CHECK(action.state == M::ActionState::EMPTY && !action.operation && !action.incident.present &&
          action.read.state == M::ReadState::EMPTY && !action.statusObservation.fieldCount);
}
// A forged setting makes the next cycle unpreparable. The refusal must come before the read
// or anything else moves: at the last command, at every field and at a cycle's last field.
void unpreparableCycleRefusesBeforeAnyMutation() {
    Rig r;
    r.prerequisites();
    L::ActionContext action;
    r.prepare(action);
    const auto refuseThenAccept = [&](const C::SdoCompletion& terminal) {
        action.options.service.identityWidths = L::IdentityWidths::MEASURED_U8;
        const Saved<L::ActionContext> refused(action);
        const M::Status status = L::advanceAction(action, r.reservation, target(), terminal, r.now);
        CHECK(!status && status.detail == int32_t(M::ReadError::ARGUMENT));
        refused.unchanged(action);
        action.options.service.identityWidths = L::IdentityWidths::DECLARED_U16;
        CHECK(L::advanceAction(action, r.reservation, target(), terminal, r.now));
    };
    L::PreparedAction work = r.next(action);
    O::RequestId id = r.admit(work.transfer, true);
    r.turn(r.now);
    refuseThenAccept(r.complete(id, work.transfer));
    CHECK(action.phase == L::ActionPhase::OBSERVING && action.read.state == M::ReadState::ACTIVE);
    const uint32_t values[] = {4, 0xa580, 0xf008, 7, 1};
    for (uint32_t value : values) {
        work = r.next(action);
        id = r.admit(work.transfer, true);
        r.turn(r.now);
        refuseThenAccept(r.complete(id, work.transfer, value));
    }
    CHECK(action.phase == L::ActionPhase::WAITING && action.observations == 1);
}
// The read refuses its own withdrawal, before the action has moved.
void refusedReadWithdrawalLeavesActionUnchanged() {
    Rig r;
    r.prerequisites();
    L::ActionContext action;
    r.prepare(action);
    r.field(action, 0x6040);
    CHECK(action.phase == L::ActionPhase::OBSERVING);
    ++action.read.values[0].object.index;
    const Saved<L::ActionContext> refused(action);
    CHECK(!L::advanceAction(action, r.reservation, target(), M::ActionOutcome::CANCELLED, r.now));
    refused.unchanged(action);
    --action.read.values[0].object.index;
    CHECK(L::advanceAction(action, r.reservation, target(), M::ActionOutcome::CANCELLED, r.now));
    CHECK(action.state == M::ActionState::FAILED && action.outcome == M::ActionOutcome::CANCELLED);
}
} // namespace

// A budget timeout before the cutoff is its own outcome. Execution comes from possible TX alone.
void commandServiceTimeoutIsNotTheDeadline() {
    struct Case {
        bool possible, atCutoff;
        M::ActionOutcome outcome;
        M::ActionExecution execution;
    };
    const Case cases[] = {
        {false, false, M::ActionOutcome::SERVICE_TIMEOUT, M::ActionExecution::NOT_TRANSMITTED},
        {true, false, M::ActionOutcome::SERVICE_TIMEOUT, M::ActionExecution::UNKNOWN},
        {true, true, M::ActionOutcome::DEADLINE, M::ActionExecution::UNKNOWN},
    };
    for (const Case& item : cases) {
        Rig r;
        r.prerequisites();
        r.deadline = r.now + 1000;
        L::ActionContext action;
        r.prepare(action);
        L::PreparedAction work;
        CHECK(L::nextAction(action, r.reservation, target(), r.now, work) && work.ready);
        if (item.possible)
            CHECK(L::noteActionTransmission(action, r.reservation, target(), r.now));
        const uint64_t at = item.atCutoff ? r.deadline : r.now + 10;
        C::SdoCompletion e;
        e.transfer = work.transfer;
        e.outcome = C::SdoOutcome::TIMEOUT;
        e.nodeGeneration = target().nodeGeneration;
        e.admittedUs = r.now;
        e.completedUs = at;
        e.queued = e.transmissionPossible = e.effectPossible = e.effectUncertain = item.possible;
        CHECK(L::advanceAction(action, r.reservation, target(), e, at));
        CHECK(action.state == M::ActionState::FAILED && action.outcome == item.outcome &&
              action.execution == item.execution && action.uncertain == item.possible);
        CHECK(action.status.detail == int32_t(M::ActionError::TIME));
        // The terminal stays a well-formed context for later calls.
        L::PreparedAction after;
        CHECK(L::nextAction(action, r.reservation, target(), at, after) && !after.ready);
        const Saved<L::ActionContext> closed(action);
        CHECK(!L::advanceAction(action, r.reservation, target(), e, at + 1));
        closed.unchanged(action);
    }
}

int main() {
    commandServiceTimeoutIsNotTheDeadline();
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
    drainMarkerConsumesOld000F();
    freshnessAndStepExhaustion();
    everyObservationFieldCanFailIndependently();
    cancelBetweenObservationCycles();
    overlappingInputsAndInvalidPoliciesAreAtomic();
    nearMaximumTimeAndBackwardCalls();
    uncertainEnqueueRefusalAndObservedEmcy();
    identityWidthsRefusedAtAdmission();
    validationMatchesPreparationAndChangesNothing();
    unpreparableCycleRefusesBeforeAnyMutation();
    refusedReadWithdrawalLeavesActionUnchanged();
    std::printf("stop action: %u checks\n", checks);
}
