// SPDX-License-Identifier: MIT
// Real service/owner integration with synthetic controller traffic only.
// No physical drive, firmware qualification or implicit enabling is represented.
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
    L::ReadTarget* current = nullptr;
    M::AxisConfig* axis = nullptr;
    M::AxisReference* reference = nullptr;
    static bool guard(void* context, O::SequenceId, const C::SdoTransfer& work, uint64_t now) {
        Wire& w = *static_cast<Wire*>(static_cast<OwnerRig::WireBase*>(context));
        if (!w.action || work.operation != w.action->operation)
            return true;
        CHECK(w.reservation && w.current && (work.index == 0x2301 || work.index == 0x2302));
        if (work.index == 0x2302) {
            CHECK(w.axis && w.reference);
            return bool(L::noteActionTransmission(*w.action, *w.reservation, *w.current, *w.axis,
                                                  *w.reference, now));
        }
        return bool(L::noteActionTransmission(*w.action, *w.reservation, *w.current, now));
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
    L::ReadTarget current = target();
    L::ReadContext state;
    M::AxisConfig axis;
    M::AxisReference reference;
    M::PreparedTarget prepared;
    unsigned invalidations = 0;
    uint64_t now = 0, deadline = 10000;
    uint32_t operation = 0;

    static void invalidated(void* context, const C::NetworkInvalidationEvent& event) {
        Rig& r = *static_cast<Rig*>(context);
        ++r.invalidations;
        if (event.cause == C::NetworkInvalidation::CONFIGURATION) {
            // This callback synchronizes a withdrawal already made at possible
            // trigger TX. A second invalidation would create the wrong epoch.
            CHECK(r.axis.binding.configurationGeneration ==
                  event.previous.binding.configurationGeneration + 1);
            CHECK(!r.axis.originKnown && !r.reference.nativeKnown);
        } else {
            CHECK(M::invalidateAxisReference(r.axis, r.reference));
            r.current.channel.binding = r.axis.binding;
            r.current.nodeGeneration = event.generation;
        }
    }
    explicit Rig(M::Generation configuration = 7, M::Generation confidence = 1)
        : owner(services, wire.callbacks(), 73) {
        current.channel.binding.configurationGeneration = configuration;
        axis.binding = current.channel.binding;
        axis.referenceGeneration = confidence;
        axis.units.commandStepsPerMotorTurn = M::UnitScale(3200, 1, M::ScaleSource::READBACK);
        axis.units.encoder.countsPerUnit = M::UnitScale(4000, 1, M::ScaleSource::READBACK);
        axis.units.encoder.sourceId = 6;
        axis.supportedRelativeBases = 1;
        axis.originKnown = axis.encoderOriginKnown = axis.softLimitsKnown = true;
        axis.originNative = 10;
        axis.encoderOriginNative = 20;
        axis.originSource = axis.encoderOriginSource = M::ScaleSource::READBACK;
        axis.softMinimum = -10000;
        axis.softMaximum = 10000;
        reference.binding = axis.binding;
        reference.referenceGeneration = axis.referenceGeneration;
        reference.nativeKnown = reference.idle = reference.stationary = true;
        reference.nativePosition = 123;
        reference.source = M::ScaleSource::QUALIFIED;
        reference.maximumAgeUs = 1000;
        M::PositionRequest request;
        request.configurationGeneration = axis.binding.configurationGeneration;
        request.value = M::Rational(10);
        CHECK(M::preparePosition(request, axis, nullptr, prepared));
        CHECK(M::validatePreparedTarget(prepared, axis, nullptr, now));
        CHECK(services.configure(0, current.channel, true, true));
        C::NetworkRoute network;
        network.binding = current.channel.binding;
        network.generation = 11;
        network.heartbeatId = 0x701;
        network.emcyId = 0x81;
        C::NetworkObserver observer;
        observer.context = this;
        observer.invalidate = invalidated;
        CHECK(services.configureNetwork(0, network, observer, 0));
        CHECK(owner.beginSequence(current.channel.binding, 1, now, sequence));
        reservation.target = current;
        reservation.model = L::SettingsModel::CL86_C;
        reservation.owner = owner.identity();
        reservation.sequence = uint32_t(sequence.slot + 1);
        reservation.generation = sequence.generation;
        reservation.acquiredUs = owner.sequence(sequence)->acquiredUs;
        wire.reservation = &reservation;
        wire.current = &current;
        wire.axis = &axis;
        wire.reference = &reference;
    }
    void begin() { CHECK(owner.beginOperation(sequence, ++operation, deadline, now)); }
    void turn(uint64_t time, const O::ServiceBudget& work = budget()) {
        now = time;
        CHECK(owner.service(now, work));
    }
    O::RequestId admit(const C::SdoTransfer& transfer) {
        O::BusRequest work;
        work.sequence = sequence;
        work.transfer = transfer;
        O::RequestId id;
        CHECK(owner.admit(work, now, id) == O::BusAdmission::ACCEPTED);
        return id;
    }
    C::SdoCompletion result(O::RequestId id, bool release = true) {
        const O::Completion* terminal = owner.result(id);
        CHECK(terminal);
        const C::SdoCompletion value = terminal->service;
        if (release)
            CHECK(owner.release(id));
        return value;
    }
    C::SdoCompletion exchange(const C::SdoTransfer& work, uint32_t raw = 0, bool abort = false) {
        const O::RequestId id = admit(work);
        turn(now);
        wire.push(reply(work, raw, abort), now + 1);
        turn(now + 1, budget(8, 0, 8, 0));
        return result(id);
    }
    uint32_t raw(const C::SdoTransfer& work, uint16_t monitor, uint8_t mode, uint16_t error,
                 int32_t position) const {
        switch (work.index) {
        case 0x1001:
            return error ? 1 : 0;
        case 0x1003:
            return work.subindex ? 0x12340000u + work.subindex : 2;
        case 0x2100:
            return monitor;
        case 0x2101:
            return 0xa580;
        case 0x2102:
            return 0xf008;
        case 0x603f:
            return error;
        case 0x6041:
            return 0xffff; // Conflicting statusword dialect never decides success.
        case 0x6061:
            return mode;
        case 0x6064:
            return uint32_t(position);
        case 0x606c:
            return 0;
        default:
            CHECK(false);
            return 0;
        }
    }
    void acquire(uint16_t monitor, uint8_t mode = 1, uint16_t error = 0xff05, uint8_t mask = 0x1f) {
        begin();
        CHECK(L::prepareState(state, current, operation, now, deadline, options(), mask));
        while (state.state == M::ReadState::ACTIVE) {
            L::PreparedRead work;
            CHECK(L::nextRead(state, now, work) && work.ready);
            const C::SdoCompletion terminal =
                exchange(work.transfer, raw(work.transfer, monitor, mode, error, 123));
            CHECK(L::advanceRead(state, terminal, now));
        }
        CHECK(state.state == M::ReadState::SUCCEEDED);
    }
    L::ActionOptions policy() const {
        L::ActionOptions value;
        value.service = options();
        value.observationIntervalUs = 10;
        value.maximumAgeUs = 1400; // A-20: at least 7 x (queue + response), for the counter clear.
        return value;
    }
    void prepare(L::ActionContext& action, bool position) {
        begin();
        if (position)
            CHECK(L::prepareSetDevicePosition(action, reservation, state, 0, axis, operation, now,
                                              deadline, policy()));
        else
            CHECK(L::prepareClearAlarm(action, reservation, state, operation, now, deadline,
                                       policy()));
        wire.action = &action;
    }
    L::PreparedAction next(const L::ActionContext& action) {
        L::PreparedAction work;
        CHECK(L::nextAction(action, reservation, current, now, work));
        return work;
    }
    void field(L::ActionContext& action, uint32_t rawValue = 0, bool abort = false) {
        const L::PreparedAction work = next(action);
        CHECK(work.ready);
        const C::SdoCompletion terminal = exchange(work.transfer, rawValue, abort);
        CHECK(L::advanceAction(action, reservation, current, terminal, now));
    }
    void command(L::ActionContext& action, uint16_t index, uint16_t value) {
        const auto work = next(action);
        CHECK(work.ready && work.transfer.index == index && work.transfer.subindex == 0 &&
              work.transfer.direction == C::SdoDirection::DOWNLOAD && work.transfer.size == 2 &&
              work.transfer.channel.binding.configurationGeneration == 7);
        CHECK(work.transfer.data[0] == uint8_t(value) &&
              work.transfer.data[1] == uint8_t(value >> 8));
        const unsigned sent = wire.sentCount;
        field(action);
        CHECK(wire.sentCount == sent + 1);
        const uint8_t expected[] = {
            0x2b, uint8_t(index), uint8_t(index >> 8), 0, uint8_t(value), uint8_t(value >> 8), 0,
            0};
        CHECK(wire.sent[sent].id == 0x601 && wire.sent[sent].payloadLength == 8 &&
              !std::memcmp(wire.sent[sent].data, expected, sizeof expected));
    }
    void synchronize() {
        CHECK(axis.binding.configurationGeneration == 8 && axis.referenceGeneration == 2);
        CHECK(owner.advanceConfiguration(sequence, current.channel.binding, now));
        CHECK(invalidations == 1 && axis.binding.configurationGeneration == 8 &&
              axis.referenceGeneration == 2);
    }
    void cycle(L::ActionContext& action, uint16_t monitor = 0, int32_t position = 0,
               uint16_t error = 0xff05, uint8_t mode = 1) {
        const uint32_t before = action.observations;
        unsigned fields = 0;
        do {
            const auto work = next(action);
            CHECK(work.ready && work.transfer.direction == C::SdoDirection::UPLOAD);
            CHECK(work.transfer.channel.binding.configurationGeneration ==
                  current.channel.binding.configurationGeneration);
            field(action, raw(work.transfer, monitor, mode, error, position));
            CHECK(++fields <= 7);
        } while (action.state == M::ActionState::ACTIVE &&
                 (action.observations == before || action.finalErrorRead));
    }
    void commands(L::ActionContext& action, bool position) {
        if (position) {
            command(action, 0x2302, 0);
            command(action, 0x2302, 1);
            command(action, 0x2302, 0);
            synchronize();
        } else {
            command(action, 0x2301, 1);
            command(action, 0x2301, 0);
        }
    }
    void checkWithdrawn() {
        CHECK(axis.binding.configurationGeneration == 8 && axis.referenceGeneration == 2 &&
              !axis.originKnown && !axis.encoderOriginKnown && !axis.softLimitsKnown &&
              !reference.nativeKnown);
        CHECK(!M::validatePreparedTarget(prepared, axis, nullptr, now));
        L::StateObservation stale;
        CHECK(!L::getStateBlock(state, L::StateBlock::FEEDBACK, current, now, 1000, stale));
        M::PositionRequest request;
        request.configurationGeneration = axis.binding.configurationGeneration;
        request.value = M::Rational(1);
        request.frame = M::CoordinateFrame::MOTOR;
        M::PreparedTarget output = prepared;
        const Saved<M::PreparedTarget> saved(output);
        CHECK(!M::preparePosition(request, axis, nullptr, output));
        saved.unchanged(output); // No new physical-coordinate target without an origin.
        request.frame = M::CoordinateFrame::NATIVE;
        request.relative = true;
        CHECK(!M::preparePosition(request, axis, &reference, output));
        saved.unchanged(output); // A zero report never restores a relative witness.
    }
};

void successfulActionsRetainEvidence() {
    for (unsigned position = 0; position < 2; ++position) {
        Rig r;
        if (position)
            r.reservation.model = L::SettingsModel::OL86_C;
        M::Frame emcy = frame(0x81, 8);
        emcy.data[0] = 5;
        emcy.data[1] = 0xff;
        emcy.data[2] = 1;
        emcy.data[7] = 0xa5;
        r.wire.push(emcy, r.now + 1);
        r.turn(r.now + 1);
        CHECK(r.services.node(0)->emcyCount == 1);
        const Saved<C::NetworkEvent> fault(r.services.node(0)->emcy);
        // Historical EMCY remains retained after a newer explicit state read.
        // A new fault after that read instead withdraws admission/completion.
        r.acquire(position ? 0 : 8);
        L::ActionContext action;
        const unsigned sent = r.wire.sentCount;
        r.prepare(action, position != 0);
        CHECK(action.incident.present == (position == 0));
        if (!position) {
            const L::AlarmIncident& incident = action.incident;
            CHECK(incident.readOperation == r.state.operation && incident.errorCode == 0xff05);
            CHECK(incident.errorCount == 2 && incident.errorHistory[0] == 0x12340001 &&
                  incident.errorHistory[1] == 0x12340002);
            CHECK(incident.earliestUs >= r.state.startedUs && incident.latestUs <= r.state.servicedUs);
            const L::StateBlock kept[] = {L::StateBlock::ERRORS, L::StateBlock::ERROR_HISTORY,
                                          L::StateBlock::MONITORS, L::StateBlock::STATUS};
            uint64_t earliest = UINT64_MAX, latest = 0;
            for (L::StateBlock block : kept) {
                const unsigned first = r.state.blockStart[unsigned(block)];
                for (unsigned i = first; i < first + r.state.blockLength[unsigned(block)]; ++i) {
                    const auto& value = r.state.values[i];
                    if (!value.valid)
                        continue;
                    earliest = value.provenance.earliestUs < earliest ? value.provenance.earliestUs : earliest;
                    latest = value.provenance.latestUs > latest ? value.provenance.latestUs : latest;
                }
            }
            CHECK(incident.earliestUs == earliest && incident.latestUs == latest);
            CHECK(incident.statusword == uint16_t(r.state.values[r.state.blockStart[unsigned(L::StateBlock::STATUS)]].raw));
            CHECK(incident.modeRaw == 1);
            const unsigned monitors = r.state.blockStart[unsigned(L::StateBlock::MONITORS)];
            CHECK(incident.monitor == uint16_t(r.state.values[monitors].raw) &&
                  incident.logicalInputs == uint16_t(r.state.values[monitors + 1].raw) &&
                  incident.logicalOutputs == uint16_t(r.state.values[monitors + 2].raw));
        }
        const Saved<L::AlarmIncident> original(action.incident);
        r.commands(action, position != 0);
        CHECK(action.execution == M::ActionExecution::ACKNOWLEDGED &&
              action.completion == M::ActionCompletion::NOT_OBSERVED);
        if (position)
            r.checkWithdrawn();
        const Saved<C::SdoCompletion> trigger(action.commandHistory[position ? 1 : 0]);
        r.cycle(action);
        CHECK(action.state == M::ActionState::SUCCEEDED &&
              action.outcome == M::ActionOutcome::OBSERVED && action.observations == 1 &&
              action.completion == M::ActionCompletion::OBSERVED);
        CHECK(r.wire.sentCount == sent + (position ? 10 : 9));
        trigger.unchanged(action.commandHistory[position ? 1 : 0]);
        original.unchanged(action.incident);
        fault.unchanged(r.services.node(0)->emcy);
        CHECK(r.services.node(0)->emcyCount == 1);
        if (position) {
            CHECK(action.extraObservation.actualPosition == 0);
            r.checkWithdrawn();
        } else {
            CHECK(action.extraObservation.errorCode == 0xff05 &&
                  action.monitorObservation.monitor == 0);
            CHECK(r.axis.originKnown && r.reference.nativeKnown &&
                  r.axis.binding.configurationGeneration == 7);
        }
        const Saved<L::ActionContext> terminal(action);
        CHECK(!r.next(action).ready);
        CHECK(!L::advanceAction(action, r.reservation, r.current, M::ActionOutcome::CANCELLED,
                                r.now));
        terminal.unchanged(action);
        for (unsigned i = sent; i < r.wire.sentCount; ++i) {
            const M::Frame& actual = r.wire.sent[i];
            CHECK(actual.id != 0 && !(actual.data[1] == 0x40 && actual.data[2] == 0x60));
        }
    }
}

void unsupportedValuesAndPoliciesAreAtomic() {
    Rig r;
    r.acquire(0);
    r.begin();
    L::ActionContext action;
    const Saved<L::ActionContext> before(action);
    const int64_t values[] = {INT64_MIN, -1, 1, INT64_MAX};
    for (int64_t value : values) {
        CHECK(L::prepareSetDevicePosition(action, r.reservation, r.state, value, r.axis,
                                          r.operation, r.now, r.deadline, r.policy())
                  .code == M::Err::UNSUPPORTED);
        before.unchanged(action);
    }
    for (unsigned position = 0; position < 2; ++position)
        for (unsigned scenario = 0; scenario < 9; ++scenario) {
            Rig bad;
            const uint16_t monitor = scenario == 0   ? 4
                                     : scenario == 1 ? (position ? 8 : 0)
                                     : position      ? 0
                                                     : 8;
            bad.acquire(monitor, scenario == 2 ? 3 : 1,
                        scenario == 3 && !position ? 0xff04 : 0xff05,
                        scenario == 4 && !position ? 0x1d : 0x1f);
            bad.begin();
            L::AxisReservation live = bad.reservation;
            M::AxisConfig axis = bad.axis;
            auto options = bad.policy();
            if (scenario == 5)
                live.model = L::SettingsModel::UNKNOWN;
            if (scenario == 6)
                options.maximumAgeUs = 0;
            if (scenario == 7)
                ++live.target.nodeGeneration;
            if (scenario == 8)
                live.acquiredUs = bad.now + 1;
            // Position admission deliberately does not require zero historical
            // 603F or an error-history block; those scenarios apply to clear only.
            if (position && (scenario == 3 || scenario == 4))
                continue;
            M::Status status =
                position
                    ? L::prepareSetDevicePosition(action, live, bad.state, 0, axis, bad.operation,
                                                  bad.now, bad.deadline, options)
                    : L::prepareClearAlarm(action, live, bad.state, bad.operation, bad.now,
                                           bad.deadline, options);
            CHECK(!status);
            before.unchanged(action);
        }
    for (unsigned which = 0; which < 2; ++which) {
        M::AxisConfig exhausted = r.axis;
        if (which)
            exhausted.referenceGeneration = UINT32_MAX;
        else
            exhausted.exhausted = true;
        CHECK(!L::prepareSetDevicePosition(action, r.reservation, r.state, 0, exhausted,
                                           r.operation, r.now, r.deadline, r.policy()));
        before.unchanged(action);
    }
    for (unsigned which = 0; which < 2; ++which) {
        Rig exhausted(which ? 7 : UINT32_MAX, which ? UINT32_MAX : 1);
        exhausted.acquire(0);
        exhausted.begin();
        const Saved<M::AxisConfig> axis(exhausted.axis);
        CHECK(!L::prepareSetDevicePosition(action, exhausted.reservation, exhausted.state, 0,
                                           exhausted.axis, exhausted.operation, exhausted.now,
                                           exhausted.deadline, exhausted.policy()));
        before.unchanged(action);
        axis.unchanged(exhausted.axis);
    }
}

void pendingObservationsAndDeadline() {
    for (unsigned position = 0; position < 2; ++position) {
        Rig r;
        r.acquire(position ? 0 : 8);
        L::ActionContext action;
        r.prepare(action, position != 0);
        r.commands(action, position != 0);
        r.cycle(action, position ? 0 : 8, position ? 123 : 0);
        CHECK(action.state == M::ActionState::ACTIVE && action.phase == L::ActionPhase::WAITING &&
              action.completion == M::ActionCompletion::NOT_OBSERVED);
        const unsigned sent = r.wire.sentCount;
        CHECK(!r.next(action).ready);
        r.turn(action.eligibleUs, O::ServiceBudget());
        r.cycle(action, position ? 0 : 8, position ? -123 : 0);
        CHECK(action.state == M::ActionState::ACTIVE && action.observations == 2);
        r.turn(r.deadline, O::ServiceBudget());
        CHECK(
            L::advanceAction(action, r.reservation, r.current, M::ActionOutcome::CANCELLED, r.now));
        CHECK(action.outcome == M::ActionOutcome::DEADLINE &&
              action.execution == M::ActionExecution::ACKNOWLEDGED && action.uncertain);
        // Alarm polling is MONITORS+STATUS only: ERRORS waits until the alarm has cleared.
        CHECK(r.wire.sentCount == sent + (position ? 7u : 5u));
        for (unsigned i = sent; i < r.wire.sentCount && !position; ++i)
            CHECK(!(r.wire.sent[i].data[1] == 0x01 && r.wire.sent[i].data[2] == 0x10) &&
                  !(r.wire.sent[i].data[1] == 0x3f && r.wire.sent[i].data[2] == 0x60));
        if (position)
            r.checkWithdrawn();
    }
}

void writeFailuresPreservePrefixes() {
    for (unsigned position = 0; position < 2; ++position)
        for (unsigned prefix = 0; prefix < (position ? 3u : 2u); ++prefix)
            for (unsigned failure = 0; failure < 4; ++failure) {
                Rig r;
                r.acquire(position ? 0 : 8);
                L::ActionContext action;
                r.prepare(action, position != 0);
                for (unsigned i = 0; i < prefix; ++i)
                    r.command(action, position ? 0x2302 : 0x2301,
                              position ? (i == 1 ? 1 : 0) : (i == 0 ? 1 : 0));
                const Saved<L::AlarmIncident> initial(action.incident);
                const auto work = r.next(action);
                const auto id = r.admit(work.transfer);
                const unsigned sent = r.wire.sentCount;
                // No-TX cancel is separate from every possible-TX terminal.
                if (failure != 0)
                    r.turn(r.now);
                if (failure == 2) {
                    r.wire.push(reply(work.transfer, 0x06090030, true), r.now + 1);
                    r.turn(r.now + 1, budget(8, 0, 8, 0));
                } else if (failure == 3) {
                    r.turn(r.now + 101, O::ServiceBudget());
                } else {
                    CHECK(r.owner.cancel(id, r.now) == O::Cancel::CANCELLED);
                }
                const C::SdoCompletion terminal = r.result(id);
                CHECK(L::advanceAction(action, r.reservation, r.current, terminal, r.now));
                CHECK(action.state == M::ActionState::FAILED &&
                      action.completion == M::ActionCompletion::NOT_OBSERVED);
                CHECK(action.commandEvidenceMask == uint8_t((1u << (prefix + 1)) - 1));
                for (unsigned i = 0; i < prefix; ++i)
                    CHECK(action.commandHistory[i].protocolAcknowledged);
                CHECK(action.commandHistory[prefix].outcome == terminal.outcome);
                CHECK(r.wire.sentCount == sent + (failure ? 1u : 0u));
                initial.unchanged(action.incident);
                if (position && (prefix > 1 || (prefix == 1 && failure)))
                    r.checkWithdrawn();
                else
                    CHECK(r.axis.originKnown && r.reference.nativeKnown &&
                          r.axis.binding.configurationGeneration == 7);
                if (position && prefix == 2)
                    CHECK(action.commandHistory[1].protocolAcknowledged &&
                          action.execution == M::ActionExecution::ACKNOWLEDGED);
                if (!position && prefix == 1)
                    CHECK(action.commandHistory[0].protocolAcknowledged &&
                          action.execution == M::ActionExecution::ACKNOWLEDGED);
                const Saved<L::ActionContext> failed(action);
                CHECK(!r.next(action).ready);
                r.wire.push(reply(work.transfer), r.now + 1); // Late reply cannot rewrite terminal.
                r.turn(r.now + 1, budget(8, 0, 8, 0));
                failed.unchanged(action);
                CHECK(r.wire.sentCount == sent + (failure ? 1u : 0u));
            }
}

void transmissionNoticeAndStaleAdmissionAreAtomic() {
    Rig r;
    r.acquire(0);
    L::ActionContext action;
    r.prepare(action, true);
    r.command(action, 0x2302, 0);
    const Saved<L::ActionContext> saved(action);
    const Saved<L::ReadTarget> targetSaved(r.current);
    const Saved<M::AxisReference> refSaved(r.reference);
    CHECK(!L::noteActionTransmission(action, r.reservation, r.current, r.now));
    saved.unchanged(action);
    CHECK(!L::noteActionTransmission(action, action.reservation, r.current, r.axis, r.reference,
                                     r.now));
    saved.unchanged(action);
    CHECK(!L::noteActionTransmission(action, r.reservation, action.reservation.target, r.axis,
                                     r.reference, r.now));
    saved.unchanged(action);
    for (unsigned fault = 0; fault < 4; ++fault) {
        M::AxisConfig stale = r.axis;
        if (fault == 0)
            ++stale.referenceGeneration;
        if (fault == 1)
            ++stale.binding.configurationGeneration;
        if (fault == 2)
            stale.exhausted = true;
        if (fault == 3)
            stale.referenceGeneration = UINT32_MAX;
        const Saved<M::AxisConfig> axisSaved(stale);
        CHECK(!L::noteActionTransmission(action, r.reservation, r.current, stale, r.reference,
                                         r.now));
        saved.unchanged(action);
        targetSaved.unchanged(r.current);
        refSaved.unchanged(r.reference);
        axisSaved.unchanged(stale);
    }
    r.command(action, 0x2302, 1);
    r.checkWithdrawn();
    const Saved<L::ActionContext> triggered(action);
    L::PreparedAction output;
    const Saved<L::PreparedAction> oldOutput(output);
    CHECK(!L::nextAction(action, r.reservation, target(), r.now, output));
    oldOutput.unchanged(output);
    triggered.unchanged(action);
    CHECK(L::advanceAction(action, r.reservation, r.current, M::ActionOutcome::CANCELLED, r.now));
    r.checkWithdrawn();
    CHECK(!r.next(action).ready);
}

void interruptedResultSurvivesClear() {
    Rig r;
    r.acquire(8);
    r.begin();
    L::ReadContext interrupted;
    CHECK(L::prepareState(interrupted, r.current, r.operation, r.now, r.deadline, options(),
                          uint8_t(1u << unsigned(L::StateBlock::STATUS))));
    L::PreparedRead read;
    CHECK(L::nextRead(interrupted, r.now, read) && read.ready);
    const auto id = r.admit(read.transfer);
    CHECK(r.owner.cancel(id, r.now) == O::Cancel::CANCELLED);
    const O::Completion* result = r.owner.result(id);
    CHECK(result && result->service.outcome == C::SdoOutcome::CANCELLED &&
          !result->service.transmissionPossible);
    const Saved<O::Completion> saved(*result);
    CHECK(L::advanceRead(interrupted, result->service, r.now));
    const Saved<L::ReadContext> failedRead(interrupted);
    L::ActionContext action;
    r.prepare(action, false);
    r.commands(action, false);
    r.cycle(action);
    CHECK(action.state == M::ActionState::SUCCEEDED);
    CHECK(r.owner.result(id));
    saved.unchanged(*r.owner.result(id));
    failedRead.unchanged(interrupted);
    CHECK(r.owner.release(id));
}

void deadlinesFaultsAndRebootKeepOriginalTerminals() {
    for (unsigned position = 0; position < 2; ++position)
        for (unsigned event = 0; event < 3; ++event) {
            Rig r;
            r.acquire(position ? 0 : 8);
            L::ActionContext action;
            r.prepare(action, position != 0);
            r.commands(action, position != 0);
            // Finish all but the final observation; newer fault/deadline/reset
            // wins even though the earlier monitor field reported no alarm.
            for (unsigned i = 0; i < 6; ++i) {
                const auto work = r.next(action);
                r.field(action, r.raw(work.transfer, 0, 1, 0xff05, 0));
            }
            const auto work = r.next(action);
            CHECK(work.ready && work.transfer.direction == C::SdoDirection::UPLOAD);
            if (event == 0)
                r.turn(r.deadline - 1, O::ServiceBudget());
            const auto id = r.admit(work.transfer);
            r.turn(r.now);
            const Saved<C::SdoCompletion> command(action.commandHistory[position ? 1 : 0]);
            if (event == 0) {
                r.wire.push(reply(work.transfer, position ? 0 : 1), r.deadline);
                r.turn(r.deadline, budget(8, 0, 8, 0));
                const auto terminal = r.result(id);
                CHECK(L::advanceAction(action, r.reservation, r.current, terminal, r.now));
                CHECK(action.outcome == M::ActionOutcome::DEADLINE);
            } else {
                M::Frame urgent = frame(event == 1 ? 0x81 : 0x701, event == 1 ? 8 : 1);
                if (event == 1) {
                    urgent.data[0] = 5;
                    urgent.data[1] = 0xff;
                    urgent.data[2] = 1;
                }
                r.wire.push(urgent, r.now + 1);
                r.turn(r.now + 1, budget(8, 0, 8, 0));
                if (event == 1) {
                    CHECK(r.services.node(0)->emcyCount == 1);
                    CHECK(r.owner.cancel(id, r.now) == O::Cancel::CANCELLED);
                }
                const auto terminal = r.result(id);
                CHECK(terminal.outcome ==
                      (event == 1 ? C::SdoOutcome::CANCELLED : C::SdoOutcome::REBOOT));
                CHECK(L::advanceAction(action, r.reservation, r.current,
                                       event == 1 ? M::ActionOutcome::DRIVE_FAULT
                                                  : M::ActionOutcome::INVALIDATED,
                                       r.now));
                CHECK(action.outcome ==
                      (event == 1 ? M::ActionOutcome::DRIVE_FAULT : M::ActionOutcome::INVALIDATED));
                const Saved<L::ActionContext> failed(action);
                const unsigned sent = r.wire.sentCount;
                r.wire.push(reply(work.transfer, position ? 0 : 1), r.now + 1);
                r.turn(r.now + 1, budget(8, 0, 8, 0));
                failed.unchanged(action);
                CHECK(r.wire.sentCount == sent);
            }
            CHECK(action.state == M::ActionState::FAILED &&
                  action.completion == M::ActionCompletion::NOT_OBSERVED && action.uncertain);
            command.unchanged(action.commandHistory[position ? 1 : 0]);
            if (position)
                CHECK(!r.axis.originKnown && !r.reference.nativeKnown &&
                      !M::validatePreparedTarget(r.prepared, r.axis, nullptr, r.now));
        }
}

void conservativeNoticeThenDefiniteNoTransmission() {
    Rig r;
    r.acquire(0);
    L::ActionContext action;
    r.prepare(action, true);
    r.command(action, 0x2302, 0);
    const auto work = r.next(action);
    // A conservative pre-TX withdrawal may precede an owner-side no-TX
    // cancellation. Its terminal refines execution without renewing coordinates.
    CHECK(L::noteActionTransmission(action, r.reservation, r.current, r.axis, r.reference, r.now));
    r.checkWithdrawn();
    const auto id = r.admit(work.transfer);
    CHECK(r.owner.cancel(id, r.now) == O::Cancel::CANCELLED);
    const auto terminal = r.result(id);
    CHECK(!terminal.transmissionPossible);
    CHECK(L::advanceAction(action, r.reservation, r.current, terminal, r.now));
    CHECK(action.state == M::ActionState::FAILED &&
          action.execution == M::ActionExecution::NOT_TRANSMITTED && action.coordinatesInvalidated);
    r.checkWithdrawn();
}

void failedCounterActionThenExplicitNewEpochRead() {
    Rig r;
    r.acquire(0);
    L::ActionContext action;
    r.prepare(action, true);
    r.command(action, 0x2302, 0);
    r.command(action, 0x2302, 1);
    const auto rearm = r.next(action);
    const auto id = r.admit(rearm.transfer);
    const unsigned sent = r.wire.sentCount;
    CHECK(r.owner.cancel(id, r.now) == O::Cancel::CANCELLED);
    const auto terminal = r.result(id, false);
    CHECK(!terminal.transmissionPossible && !r.services.unresolved(0));
    CHECK(L::advanceAction(action, r.reservation, r.current, terminal, r.now));
    CHECK(action.outcome == M::ActionOutcome::CANCELLED &&
          action.execution == M::ActionExecution::ACKNOWLEDGED && action.uncertain);
    const Saved<L::ActionContext> failed(action);
    const Saved<O::Completion> retained(*r.owner.result(id));
    r.checkWithdrawn();
    // Even a definite no-TX rearm failure keeps the trigger's coordinate loss.
    // An unread old-epoch result prevents synchronization, without erasing it.
    CHECK(!r.owner.advanceConfiguration(r.sequence, r.current.channel.binding, r.now));
    retained.unchanged(*r.owner.result(id));
    CHECK(r.owner.release(id));
    r.synchronize();
    r.begin();
    L::ReadContext read;
    CHECK(L::prepareState(read, r.current, r.operation, r.now, r.deadline, options(),
                          uint8_t(1u << unsigned(L::StateBlock::FEEDBACK))));
    L::PreparedRead fresh;
    CHECK(L::nextRead(read, r.now, fresh) && fresh.ready);
    O::BusRequest stale;
    stale.sequence = r.sequence;
    stale.transfer = fresh.transfer;
    stale.transfer.channel.binding = target().channel.binding;
    O::RequestId rejected;
    const Saved<O::RequestId> unchanged(rejected);
    CHECK(r.owner.admit(stale, r.now, rejected) == O::BusAdmission::INVALID);
    unchanged.unchanged(rejected);
    CHECK(r.wire.sentCount == sent);
    while (read.state == M::ReadState::ACTIVE) {
        CHECK(L::nextRead(read, r.now, fresh) && fresh.ready);
        const auto observed = r.exchange(fresh.transfer);
        CHECK(L::advanceRead(read, observed, r.now));
    }
    L::StateObservation feedback;
    CHECK(L::getStateBlock(read, L::StateBlock::FEEDBACK, r.current, r.now, 1000, feedback));
    CHECK(feedback.actualPosition == 0 && r.wire.sentCount == sent + 2);
    failed.unchanged(action); // A later zero observation cannot settle failed rearm.
    r.checkWithdrawn();       // Nor can it synthesize an origin or home witness.
}

// A matched absent-object abort settles its write and leaves the route open. It does not
// retire the workflow's history: the recorded prefix and the possible effect stay as they were.
void absentObjectAbortLeavesWorkflowHistoryOnAnOpenRoute() {
    Rig r;
    r.acquire(0);
    L::ActionContext action;
    r.prepare(action, true);
    r.command(action, 0x2302, 0);
    const auto write = r.next(action);
    const auto id = r.admit(write.transfer);
    const unsigned sent = r.wire.sentCount;
    r.turn(r.now);
    CHECK(r.wire.sentCount == sent + 1);
    r.wire.push(reply(write.transfer, 0x06020000, true), r.now + 1);
    r.turn(r.now + 1, budget(8, 0, 8, 0));
    const auto terminal = r.result(id);
    CHECK(terminal.outcome == C::SdoOutcome::SERVER_ABORT && terminal.abortCode == 0x06020000);
    CHECK(terminal.effectPossible && terminal.effectUncertain);
    CHECK(!r.services.unresolved(0) && r.owner.sequence(r.sequence));
    CHECK(L::advanceAction(action, r.reservation, r.current, terminal, r.now));
    CHECK(action.state == M::ActionState::FAILED);
    CHECK(action.commandHistory[0].protocolAcknowledged && action.commandHistory[0].effectPossible);
    CHECK(action.commandHistory[1].outcome == C::SdoOutcome::SERVER_ABORT &&
          action.commandHistory[1].effectUncertain);
    const Saved<L::ActionContext> failed(action);
    CHECK(!r.next(action).ready);
    r.turn(r.now + 1000, budget());
    failed.unchanged(action);
    CHECK(r.wire.sentCount == sent + 1); // Open route, and still nothing replays.
}

void uncertainCounterTriggerCannotReopenThroughEpochSync() {
    Rig r;
    r.acquire(0);
    L::ActionContext action;
    r.prepare(action, true);
    r.command(action, 0x2302, 0);
    const auto trigger = r.next(action);
    const auto id = r.admit(trigger.transfer);
    r.wire.autoReport = r.wire.retirement = false;
    const unsigned sent = r.wire.sentCount;
    r.turn(r.now);
    r.checkWithdrawn();
    CHECK(r.wire.pending[sent]);
    r.turn(r.now + 101, budget(0, 0, 8, 0));
    const auto terminal = r.result(id, false);
    CHECK(terminal.outcome == C::SdoOutcome::TIMEOUT && terminal.effectUncertain &&
          r.services.unresolved(0) && r.wire.retireCalls == 1);
    CHECK(L::advanceAction(action, r.reservation, r.current, terminal, r.now));
    CHECK(action.execution == M::ActionExecution::UNKNOWN && action.uncertain);
    const Saved<L::ActionContext> failed(action);
    const Saved<O::Completion> retained(*r.owner.result(id));
    CHECK(!r.owner.advanceConfiguration(r.sequence, r.current.channel.binding, r.now));
    CHECK(!r.owner.beginOperation(r.sequence, r.operation + 1, r.deadline, r.now));
    // Retiring the local controller frame and receiving a late ACK still do
    // not qualify the ambiguous remote SDO channel or replay the trailing zero.
    r.wire.report(sent);
    r.wire.push(reply(trigger.transfer), r.now + 1);
    r.turn(r.now + 1, budget(8, 0, 8, 0));
    retained.unchanged(*r.owner.result(id));
    CHECK(r.owner.release(id));
    CHECK(r.services.unresolved(0) && !r.owner.sequence(r.sequence));
    CHECK(!r.owner.advanceConfiguration(r.sequence, r.current.channel.binding, r.now));
    O::BusRequest request;
    request.sequence = r.sequence;
    request.transfer = trigger.transfer;
    ++request.transfer.step;
    O::RequestId rejected;
    for (unsigned epoch = 0; epoch < 2; ++epoch) {
        request.transfer.channel.binding =
            epoch ? r.current.channel.binding : target().channel.binding;
        CHECK(r.owner.admit(request, r.now, rejected) == O::BusAdmission::INVALID);
    }
    // Remove reservation occupancy as an alternative rejection cause. A new
    // local token still cannot establish a fresh remote transaction boundary.
    CHECK(r.owner.releaseSequence(r.sequence));
    CHECK(M::sameBinding(r.services.channel(0)->binding, target().channel.binding));
    O::SequenceId replacement;
    const Saved<O::SequenceId> unchanged(replacement);
    CHECK(!r.owner.beginSequence(target().channel.binding, 1, r.now, replacement));
    unchanged.unchanged(replacement);
    CHECK(r.services.unresolved(0));
    // Reading the trigger's own object back is the same pair, so it cannot be the boundary either.
    C::SdoTransfer readback = trigger.transfer;
    readback.channel = *r.services.channel(0);
    readback.direction = C::SdoDirection::UPLOAD;
    readback.readEffect = C::SdoReadEffect::ACCEPTED_UNQUALIFIED;
    readback.allowSegmentedUpload = false;
    readback.deadlineUs = r.now + 1000;
    C::SdoDrainPremises premises;
    premises.soleClient = premises.orderedServer = true;
    CHECK(r.owner.admitDrainMarker(readback, premises, r.now, rejected) == O::BusAdmission::INVALID);
    CHECK(r.services.unresolved(0) && r.services.requalifications(0) == 0);
    CHECK(r.wire.sentCount == sent + 1 && !r.wire.pending[sent] && r.invalidations == 0);
    failed.unchanged(action);
    r.checkWithdrawn();
}

void identityWidthsRefusedAtAdmission() {
    for (unsigned position = 0; position < 2; ++position) {
        Rig r;
        r.acquire(position ? 0 : 8);
        r.begin();
        L::ActionOptions measured = r.policy();
        measured.service.identityWidths = L::IdentityWidths::MEASURED_U8;
        L::ActionContext action;
        const Saved<L::ActionContext> empty(action);
        const M::Status refused =
            position ? L::prepareSetDevicePosition(action, r.reservation, r.state, 0, r.axis, r.operation,
                                                   r.now, r.deadline, measured)
                     : L::prepareClearAlarm(action, r.reservation, r.state, r.operation, r.now,
                                            r.deadline, measured);
        CHECK(!refused && refused.detail == int32_t(M::ActionError::ARGUMENT));
        empty.unchanged(action);
    }
}
// The cycle that clears the alarm must prepare the final ERRORS read. That is proved first.
void unpreparableFinalReadRefusesBeforeAnyMutation() {
    Rig r;
    r.acquire(8);
    L::ActionContext action;
    r.prepare(action, false);
    r.commands(action, false);
    const uint32_t values[] = {0, 0xa580, 0xf008, 0xffff, 1};
    for (uint32_t value : values) {
        const L::PreparedAction work = r.next(action);
        const C::SdoCompletion terminal = r.exchange(work.transfer, value);
        action.options.service.identityWidths = L::IdentityWidths::MEASURED_U8;
        const Saved<L::ActionContext> refused(action);
        const M::Status status = L::advanceAction(action, r.reservation, r.current, terminal, r.now);
        CHECK(!status && status.detail == int32_t(M::ReadError::ARGUMENT));
        refused.unchanged(action);
        action.options.service.identityWidths = L::IdentityWidths::DECLARED_U16;
        CHECK(L::advanceAction(action, r.reservation, r.current, terminal, r.now));
    }
    CHECK(action.finalErrorRead && action.state == M::ActionState::ACTIVE);
    // Before the final read the slot still holds the prerequisite, and the incident outlives it.
    CHECK(action.extraObservation.block == L::StateBlock::ERRORS &&
          action.extraObservation.errorCode == 0xff05 && action.incident.errorCode == 0xff05);
}
// Admission reads no more than it needs: a counter clear carries no alarm incident or ERRORS.
void counterClearHoldsItsFeedbackOnly() {
    Rig r;
    r.acquire(0);
    L::ActionContext action;
    r.prepare(action, true);
    CHECK(!action.incident.present && action.extraObservation.fieldCount == 0);
    r.commands(action, true);
    r.cycle(action);
    CHECK(action.state == M::ActionState::SUCCEEDED &&
          action.extraObservation.block == L::StateBlock::FEEDBACK);
    CHECK(action.commandHistory[1].transfer.index == 0x2302 && action.commandHistory[1].transfer.data[0] == 1);
}
} // namespace
int main() {
    successfulActionsRetainEvidence();
    unsupportedValuesAndPoliciesAreAtomic();
    pendingObservationsAndDeadline();
    writeFailuresPreservePrefixes();
    transmissionNoticeAndStaleAdmissionAreAtomic();
    interruptedResultSurvivesClear();
    deadlinesFaultsAndRebootKeepOriginalTerminals();
    conservativeNoticeThenDefiniteNoTransmission();
    failedCounterActionThenExplicitNewEpochRead();
    uncertainCounterTriggerCannotReopenThroughEpochSync();
    absentObjectAbortLeavesWorkflowHistoryOnAnOpenRoute();
    identityWidthsRefusedAtAdmission();
    unpreparableFinalReadRefusesBeforeAnyMutation();
    counterClearHoldsItsFeedbackOnly();
    std::printf("reference actions: %u checks passed\n", checks);
}
