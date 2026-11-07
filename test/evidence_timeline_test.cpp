// SPDX-License-Identifier: MIT
// Timelines of retained service evidence. Budget timeouts come from the real SDO client, with the
// budget ending one microsecond before, at and after the overall cutoff. The rest covers what must
// stay published, what must be withdrawn and which evidence each action may rest on. Oracles are
// literal, transcribed from the source notes in State.h and Actions.h rather than derived from the
// code under test. All traffic is synthetic; nothing here qualifies a drive.
#include "SdoClient.h"
#include "canopen/SdoEvidence.h"
#include <MotorControlCAN/Axis.h>
#include <MotorControlCAN/profiles/lichuan_c/Actions.h>
#include <MotorControlCAN/profiles/lichuan_c/Pdo.h>
#include <MotorControlCAN/profiles/lichuan_c/PdoConfiguration.h>
#include <MotorControlCAN/profiles/lichuan_c/Position.h>
#include <MotorControlCAN/profiles/lichuan_c/State.h>
#include <MotorControlCAN/profiles/lichuan_c/Tuning.h>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <initializer_list>
#include <new>

namespace M = MotorControlCAN;
namespace C = MotorControlCAN::CANOPEN;
namespace L = MotorControlCAN::LICHUAN_C;
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
template <typename T> struct Saved {
    unsigned char bytes[sizeof(T)];
    explicit Saved(const T& value) { std::memcpy(bytes, &value, sizeof value); }
    bool same(const T& value) const { return std::memcmp(bytes, &value, sizeof value) == 0; }
};
bool refused(const M::Status& status) {
    return !status && status.code != M::Err::OK && status.msg && *status.msg;
}
bool refusedAs(const M::Status& status, M::ReadError detail) {
    return refused(status) && status.detail == int32_t(detail);
}

L::ReadTarget target() {
    L::ReadTarget t;
    t.channel.binding.logicalAxisId = 17;
    t.channel.binding.endpoint.profile = 41;
    t.channel.binding.endpoint.address = 5;
    t.channel.binding.endpoint.bus = 2;
    t.channel.binding.bindingGeneration = 3;
    t.channel.binding.configurationGeneration = 7;
    t.channel.clientToServerId = 0x625;
    t.channel.serverToClientId = 0x5a5;
    t.nodeGeneration = 11;
    return t;
}
L::ReadTarget withBinding(const M::Binding& binding) {
    L::ReadTarget t = target();
    t.channel.binding = binding;
    return t;
}
L::ReadOptions options(uint32_t queue = 100, uint32_t response = 100) {
    L::ReadOptions o;
    o.queueTimeoutUs = queue;
    o.responseTimeoutUs = response;
    return o;
}
const uint64_t ACQUIRED = 1000, STEP_AT = 5000, SPAN = 1000, CUTOFF = STEP_AT + SPAN;
L::AxisReservation reservation() {
    L::AxisReservation r;
    r.target = target();
    r.model = L::SettingsModel::CL86_C;
    r.owner = 3;
    r.sequence = 5;
    r.generation = 9;
    r.acquiredUs = ACQUIRED;
    return r;
}
uint8_t mask(std::initializer_list<L::StateBlock> blocks) {
    unsigned out = 0;
    for (L::StateBlock block : blocks)
        out |= 1u << unsigned(block);
    return uint8_t(out);
}
const uint8_t PREREQUISITE = uint8_t((1u << unsigned(L::StateBlock::MONITORS)) |
                                     (1u << unsigned(L::StateBlock::STATUS)));

C::SdoCompletion acknowledged(const C::SdoTransfer& work, uint64_t admitted, uint64_t completed,
                              uint32_t raw = 0) {
    C::SdoCompletion e;
    e.transfer = work;
    e.outcome = C::SdoOutcome::SUCCESS;
    e.nodeGeneration = target().nodeGeneration;
    e.admittedUs = admitted;
    e.completedUs = completed;
    e.queued = e.transmissionPossible = e.transmissionConfirmed = true;
    e.protocolAcknowledged = e.effectPossible = true;
    if (work.direction == C::SdoDirection::UPLOAD) {
        e.size = work.size;
        for (unsigned i = 0; i < e.size; ++i)
            e.data[i] = uint8_t(raw >> (8u * i));
    }
    return e;
}
C::SdoCompletion failed(const C::SdoTransfer& work, uint64_t admitted, uint64_t completed,
                        C::SdoOutcome outcome, bool possible = true) {
    C::SdoCompletion e;
    e.transfer = work;
    e.outcome = outcome;
    e.nodeGeneration = target().nodeGeneration;
    e.admittedUs = admitted;
    e.completedUs = completed;
    e.queued = e.transmissionPossible = e.effectPossible = e.effectUncertain = possible;
    if (outcome == C::SdoOutcome::SERVER_ABORT) {
        e.abortValid = true;
        e.abortCode = 0xf1234567u;
    }
    return e;
}
// Field by field, because padding is not part of what a context retains.
bool sameTerminal(const C::SdoCompletion& a, const C::SdoCompletion& b) {
    return C::sameSdoWork(a.transfer, b.transfer) && a.outcome == b.outcome &&
           a.completedUs == b.completedUs && a.admittedUs == b.admittedUs &&
           a.nodeGeneration == b.nodeGeneration && a.queued == b.queued &&
           a.transmissionPossible == b.transmissionPossible &&
           a.transmissionConfirmed == b.transmissionConfirmed &&
           a.protocolAcknowledged == b.protocolAcknowledged &&
           a.effectPossible == b.effectPossible && a.effectUncertain == b.effectUncertain &&
           a.abortValid == b.abortValid && a.abortCode == b.abortCode && a.size == b.size &&
           !std::memcmp(a.data, b.data, sizeof a.data);
}

// How far a real client carried the request before its budget ran out.
enum class Reach { UNSERVED, STAGED, HANDED_OFF, CONFIRMED };
const Reach reaches[] = {Reach::UNSERVED, Reach::STAGED, Reach::HANDED_OFF, Reach::CONFIRMED};
bool mayHaveLeft(Reach reach) {
    return reach == Reach::HANDED_OFF || reach == Reach::CONFIRMED;
}
// The queue budget governs until handoff and the response budget from then on.
L::ReadOptions budgetEndingAt(Reach reach, int offset) {
    const uint32_t ending = uint32_t(int64_t(SPAN) + offset);
    return mayHaveLeft(reach) ? options(50000, ending) : options(ending, 50000);
}
// The genuine terminal: the client must stay silent until the budget's own end, clamped to cutoff.
C::SdoCompletion realTimeout(const C::SdoTransfer& work, Reach reach, uint64_t ending,
                             bool bestEffort = false) {
    C::SdoClient client;
    CHECK(client.configure(0, work.channel, true, true));
    C::NetworkRoute route;
    route.binding = work.channel.binding;
    route.generation = target().nodeGeneration;
    route.heartbeatId = 0x705;
    route.emcyId = 0x85;
    CHECK(client.configureNetwork(0, route, C::NetworkObserver(), STEP_AT));
    C::SdoRequestId id;
    if (bestEffort)
        CHECK(client.admitBestEffortStop(0, work, STEP_AT, id, UINT64_MAX));
    else
        CHECK(client.admit(0, work, STEP_AT, id));
    if (reach != Reach::UNSERVED)
        CHECK(client.service(STEP_AT));
    if (mayHaveLeft(reach)) {
        M::Frame frame;
        C::SdoTxId tx;
        CHECK(client.nextTx(0, STEP_AT, frame, tx));
        CHECK(client.canTransmit(tx, STEP_AT));
        if (reach == Reach::CONFIRMED)
            CHECK(client.confirmTx(tx, STEP_AT));
    }
    CHECK(client.service(ending - 1));
    CHECK(!client.result(id));
    CHECK(client.service(ending));
    const C::SdoCompletion* terminal = client.result(id);
    CHECK(terminal && terminal->outcome == C::SdoOutcome::TIMEOUT && terminal->completedUs == ending);
    CHECK(terminal->admittedUs == STEP_AT && terminal->nodeGeneration == target().nodeGeneration);
    CHECK(terminal->transmissionPossible == mayHaveLeft(reach));
    CHECK(terminal->transmissionConfirmed == (reach == Reach::CONFIRMED));
    CHECK(terminal->effectUncertain == mayHaveLeft(reach));
    return *terminal;
}

// A finished read delivering the given word for each listed object, in 1 us steps up to `now`.
struct Word {
    uint16_t index;
    uint32_t raw;
};
void finishRead(L::ReadContext& c, uint64_t& now, std::initializer_list<Word> words) {
    while (c.state == M::ReadState::ACTIVE) {
        L::PreparedRead work;
        CHECK(L::nextRead(c, now, work) && work.ready);
        uint32_t raw = 1;
        for (const Word& word : words)
            if (word.index == work.transfer.index)
                raw = word.raw;
        ++now;
        CHECK(L::advanceRead(c, acknowledged(work.transfer, now, now, raw), now));
    }
    CHECK(c.state == M::ReadState::SUCCEEDED);
}
C::SdoCompletion controlWord(uint16_t word, uint64_t admitted, uint64_t completed) {
    C::SdoTransfer work;
    work.channel = target().channel;
    work.operation = work.request = 31;
    work.step = 1;
    work.index = 0x6040;
    work.direction = C::SdoDirection::DOWNLOAD;
    work.size = 2;
    work.data[0] = uint8_t(word);
    work.data[1] = uint8_t(word >> 8);
    work.deadlineUs = completed + 1000000;
    work.queueTimeoutUs = work.responseTimeoutUs = 100;
    return acknowledged(work, admitted, completed);
}
L::ActionOptions actionOptions(const L::ReadOptions& service = options(), uint64_t age = 1000000) {
    L::ActionOptions o;
    o.service = service;
    o.observationIntervalUs = 10;
    o.maximumAgeUs = age;
    return o;
}

// Each rig reaches one live service step at exactly STEP_AT, with deadline CUTOFF and the budgets
// under test. `download` tells the driver below whether a possible-TX notice applies.
struct ReadRig {
    L::ReadContext c;
    C::SdoTransfer work;
    bool download = false;
    ReadRig(unsigned, const L::ReadOptions& o) {
        uint64_t now = STEP_AT - 100;
        CHECK(L::prepareState(c, target(), 1, now, CUTOFF, o,
                              mask({L::StateBlock::ERRORS, L::StateBlock::STATUS})));
        for (unsigned i = 0; i < 2; ++i) {
            L::PreparedRead p;
            CHECK(L::nextRead(c, now, p));
            ++now;
            CHECK(L::advanceRead(c, acknowledged(p.transfer, now, now, 0x21), now));
        }
        L::PreparedRead p;
        CHECK(L::nextRead(c, STEP_AT, p));
        work = p.transfer;
    }
    void notice() {}
    M::Status consume(const C::SdoCompletion& e, uint64_t now) { return L::advanceRead(c, e, now); }
    void settled(M::ReadOutcome outcome, const C::SdoCompletion& e, bool, bool possible, uint64_t now) {
        CHECK(c.state == M::ReadState::FAILED && c.outcome == outcome);
        CHECK(c.status.code == M::Err::INVALID_CONFIG && c.status.detail == int32_t(M::ReadError::TIME));
        CHECK(c.uncertain == possible && !c.invalidated);
        CHECK(c.evidencePresent && sameTerminal(c.evidence, e));
        CHECK(!c.values[2].valid);
        // A budget says nothing against earlier replies, so the finished block stays published,
        // and the unfinished block is refused.
        L::StateObservation block;
        CHECK(L::getStateBlock(c, L::StateBlock::ERRORS, target(), now, 100000, block));
        CHECK(block.errorRegister == 0x21 && block.errorCode == 0x21);
        CHECK(refused(L::getStateBlock(c, L::StateBlock::STATUS, target(), now, 100000, block)));
        const Saved<L::ReadContext> closed(c);
        CHECK(refused(L::advanceRead(c, e, now + 1)));
        CHECK(refused(L::advanceRead(c, M::ReadOutcome::CANCELLED, now + 1)));
        CHECK(refused(L::advanceRead(c, M::ReadOutcome::DEADLINE, CUTOFF + 50)));
        CHECK(refused(L::advanceRead(c, M::ReadOutcome::INVALIDATED, now + 1)));
        L::PreparedRead later;
        CHECK(L::nextRead(c, CUTOFF + 5000, later) && !later.ready);
        CHECK(closed.same(c));
    }
};
struct TuningRig {
    L::DriverContext c;
    C::SdoTransfer work;
    bool download = false;
    TuningRig(unsigned, const L::ReadOptions& o) {
        CHECK(L::prepareTuningRead(c, target(), 1, STEP_AT, CUTOFF, o, L::TuningGroup::CURRENT_HOLDING));
        L::PreparedRead p;
        CHECK(L::nextDriver(c, STEP_AT, p));
        work = p.transfer;
    }
    void notice() {}
    M::Status consume(const C::SdoCompletion& e, uint64_t now) { return L::advanceDriver(c, e, now); }
    void settled(M::ReadOutcome outcome, const C::SdoCompletion& e, bool, bool possible, uint64_t now) {
        CHECK(c.read.state == M::ReadState::FAILED && c.read.outcome == outcome);
        CHECK(c.read.status.detail == int32_t(M::ReadError::TIME));
        CHECK(c.read.uncertain == possible && !c.read.invalidated);
        CHECK(sameTerminal(c.read.evidence, e));
        L::TuningObservation out;
        const Saved<L::TuningObservation> blank(out);
        CHECK(refused(L::getTuning(c, target(), now, 100000, out)) && blank.same(out));
        const Saved<L::DriverContext> closed(c);
        CHECK(refused(L::advanceDriver(c, e, now + 1)));
        CHECK(refused(L::advanceDriver(c, M::ReadOutcome::CANCELLED, now + 1)));
        CHECK(closed.same(c));
    }
};
// variant 0 motion, 1 I/O, 2 control, each on its first download.
struct SettingsWriteRig {
    L::DriverSettingsContext c;
    L::AxisReservation live = reservation();
    M::Binding binding = target().channel.binding;
    C::SdoTransfer work;
    bool download = true;
    SettingsWriteRig(unsigned variant, const L::ReadOptions& o) {
        uint64_t now = ACQUIRED;
        L::DriverContext prior;
        if (variant == 0) {
            const C::SdoCompletion control = controlWord(0x000f, now + 1, now + 2);
            now += 2;
            L::ReadContext state;
            CHECK(L::prepareState(state, target(), 2, now, CUTOFF, options(), PREREQUISITE));
            finishRead(state, now, {{0x2100, 0}, {0x6041, 7}, {0x6061, 1}});
            CHECK(L::prepareDriverRead(prior, target(), 3, now, CUTOFF, options(),
                                       L::DriverGroup::MOTION_PARAMETERS));
            finishRead(prior.read, now, {});
            L::DriverSettings candidate;
            candidate.positionSpeedRpm = candidate.rampUpTimeMs = candidate.rampDownTimeMs = 2;
            CHECK(L::prepareDriverSettings(c, live, live.target, prior, state, control, candidate, 41, STEP_AT,
                                           CUTOFF, o, 100000));
        } else if (variant == 1) {
            L::ReadContext state;
            CHECK(L::prepareState(state, target(), 2, now, CUTOFF, options(), PREREQUISITE));
            finishRead(state, now, {{0x2100, 0}, {0x6041, 7}, {0x6061, 1}});
            CHECK(L::prepareDriverRead(prior, target(), 3, now, CUTOFF, options(),
                                       L::DriverGroup::IO_INPUT_CONFIGURATION));
            finishRead(prior.read, now, {});
            L::IoSettings candidate;
            candidate.invertedInputs = 0x55;
            for (L::InputFunction& function : candidate.functions)
                function = L::InputFunction::STOP;
            CHECK(L::prepareIoSettings(c, live, live.target, prior, state, candidate, 41, STEP_AT, CUTOFF, o,
                                       100000));
        } else {
            L::ReadContext state;
            CHECK(L::prepareState(state, target(), 2, now, CUTOFF, options(), PREREQUISITE));
            finishRead(state, now, {{0x2100, 0}, {0x6041, 7}, {0x6061, 1}});
            CHECK(L::prepareControlRead(prior, target(), 3, now, CUTOFF, options()));
            finishRead(prior.read, now, {});
            L::ControlSettings candidate;
            candidate.loopMode = L::LoopMode::CLOSED_LOOP;
            CHECK(L::prepareControlSettings(c, live, live.target, prior, state, candidate, 41, STEP_AT, CUTOFF, o, 100000));
        }
        L::PreparedRead p;
        CHECK(L::nextDriver(c, live, binding, STEP_AT, p) && p.ready);
        work = p.transfer;
    }
    void notice() { CHECK(L::noteDriverTransmission(c, live, binding, STEP_AT)); }
    M::Status consume(const C::SdoCompletion& e, uint64_t now) {
        return L::advanceDriver(c, live, binding, e, now);
    }
    void settled(M::ReadOutcome outcome, const C::SdoCompletion& e, bool noted, bool, uint64_t now) {
        CHECK(c.state == L::SettingsState::FAILED && c.outcome == outcome);
        CHECK(c.status.detail == int32_t(M::ReadError::TIME));
        CHECK(c.uncertain == noted && c.effectsPossible == noted && !c.confirmed);
        CHECK(binding.configurationGeneration == (noted ? 8u : 7u));
        CHECK(c.evidencePresent && sameTerminal(c.evidence, e));
        L::DriverObservation out;
        const Saved<L::DriverObservation> blank(out);
        CHECK(refused(L::getDriver(c, live, withBinding(binding), now, 100000, out)) && blank.same(out));
        const Saved<L::DriverSettingsContext> closed(c);
        CHECK(refused(L::advanceDriver(c, live, binding, e, now + 1)));
        CHECK(refused(L::advanceDriver(c, M::ReadOutcome::CANCELLED, now + 1)));
        CHECK(refused(L::advanceDriver(c, M::ReadOutcome::DEADLINE, CUTOFF + 50)));
        M::Binding kept = binding;
        CHECK(refused(L::noteDriverTransmission(c, live, kept, now + 1)));
        L::PreparedRead later;
        later.transfer.index = 0xbeef;
        CHECK(L::nextDriver(c, live, binding, CUTOFF + 5000, later) && !later.ready);
        CHECK(closed.same(c) && M::sameBinding(kept, binding));
    }
};
// variant 0 motion, 1 control: every write acknowledged, then the first readback upload.
struct SettingsReadbackRig {
    L::DriverSettingsContext c;
    L::AxisReservation live = reservation();
    M::Binding binding = target().channel.binding;
    C::SdoTransfer work;
    bool download = false;
    SettingsReadbackRig(unsigned variant, const L::ReadOptions& o) {
        uint64_t now = ACQUIRED;
        L::DriverContext prior;
        if (variant == 0) {
            const C::SdoCompletion control = controlWord(0x000f, now + 1, now + 2);
            now += 2;
            L::ReadContext state;
            CHECK(L::prepareState(state, target(), 2, now, CUTOFF, options(), PREREQUISITE));
            finishRead(state, now, {{0x2100, 0}, {0x6041, 7}, {0x6061, 1}});
            CHECK(L::prepareDriverRead(prior, target(), 3, now, CUTOFF, options(),
                                       L::DriverGroup::MOTION_PARAMETERS));
            finishRead(prior.read, now, {});
            L::DriverSettings candidate;
            candidate.positionSpeedRpm = candidate.rampUpTimeMs = candidate.rampDownTimeMs = 2;
            CHECK(L::prepareDriverSettings(c, live, live.target, prior, state, control, candidate, 41, now, CUTOFF,
                                           o, 100000));
        } else {
            L::ReadContext state;
            CHECK(L::prepareState(state, target(), 2, now, CUTOFF, options(), PREREQUISITE));
            finishRead(state, now, {{0x2100, 0}, {0x6041, 7}, {0x6061, 1}});
            CHECK(L::prepareControlRead(prior, target(), 3, now, CUTOFF, options()));
            finishRead(prior.read, now, {});
            L::ControlSettings candidate;
            candidate.loopMode = L::LoopMode::CLOSED_LOOP;
            CHECK(L::prepareControlSettings(c, live, live.target, prior, state, candidate, 41, now, CUTOFF, o, 100000));
        }
        while (c.state == L::SettingsState::WRITING) {
            L::PreparedRead p;
            CHECK(L::nextDriver(c, live, binding, now, p));
            CHECK(L::noteDriverTransmission(c, live, binding, now));
            ++now;
            CHECK(L::advanceDriver(c, live, binding, acknowledged(p.transfer, now, now), now));
        }
        CHECK(c.state == L::SettingsState::READING_BACK);
        L::PreparedRead p;
        CHECK(L::nextDriver(c, live, binding, STEP_AT, p) && p.ready);
        work = p.transfer;
    }
    void notice() {}
    M::Status consume(const C::SdoCompletion& e, uint64_t now) {
        return L::advanceDriver(c, live, binding, e, now);
    }
    void settled(M::ReadOutcome outcome, const C::SdoCompletion& e, bool, bool possible, uint64_t now) {
        CHECK(c.state == L::SettingsState::FAILED && c.outcome == outcome);
        CHECK(c.status.detail == int32_t(M::ReadError::TIME));
        CHECK(c.uncertain == possible && c.effectsPossible && c.confirmed == c.changed);
        CHECK(sameTerminal(c.evidence, e) && sameTerminal(c.readback.read.evidence, e));
        CHECK(c.readback.read.outcome == outcome && c.readback.read.state == M::ReadState::FAILED);
        CHECK(!c.readback.read.invalidated);
        const Saved<L::DriverSettingsContext> closed(c);
        CHECK(refused(L::advanceDriver(c, live, binding, e, now + 1)));
        CHECK(refused(L::advanceDriver(c, M::ReadOutcome::INVALIDATED, now + 1)));
        CHECK(closed.same(c));
    }
};
// Action on a given download slot. Variants: 0 HALT, 1 QUICK_STOP, 2/3 alarm clear slots 0/1,
// 4/5/6 counter clear slots 0/1/2.
struct ActionCommandRig {
    L::ActionContext c;
    L::AxisReservation live = reservation();
    L::ReadTarget current = target();
    M::AxisConfig axis;
    M::AxisReference reference;
    C::SdoTransfer work;
    bool download = true;
    unsigned variant;
    bool prefix = false;
    bool counter() const { return variant >= 4; }
    void noticeAt(uint64_t at) {
        if (counter())
            CHECK(L::noteActionTransmission(c, live, current, axis, reference, at));
        else
            CHECK(L::noteActionTransmission(c, live, current, at));
    }
    ActionCommandRig(unsigned which, const L::ReadOptions& o) : variant(which) {
        axis.binding = current.channel.binding;
        uint64_t now = ACQUIRED;
        const C::SdoCompletion control = controlWord(0x000f, now + 1, now + 2);
        now += 2;
        L::ReadContext state;
        const bool clearing = which == 2 || which == 3;
        CHECK(L::prepareState(state, target(), 2, now, CUTOFF, options(),
                              clearing    ? mask({L::StateBlock::ERRORS, L::StateBlock::ERROR_HISTORY,
                                                  L::StateBlock::MONITORS, L::StateBlock::STATUS})
                              : counter() ? mask({L::StateBlock::MONITORS, L::StateBlock::STATUS,
                                                  L::StateBlock::FEEDBACK})
                                          : PREREQUISITE));
        if (clearing)
            finishRead(state, now, {{0x2100, L::MONITOR_ALARM}, {0x6041, 7}, {0x6061, 1}, {0x603f, 0xff05}, {0x1003, 0}});
        else // A counter clear also needs 606C zero; a stop does not read it.
            finishRead(state, now,
                       {{0x2100, which < 2 ? uint32_t(L::MONITOR_RUNNING) : 0u}, {0x6041, 7}, {0x6061, 1}, {0x606c, 0}});
        const L::ActionOptions policy = actionOptions(o);
        if (which < 2)
            CHECK(L::prepareStop(c, live, state, control, which ? L::StopKind::QUICK_STOP : L::StopKind::HALT,
                                 41, now, CUTOFF, policy));
        else if (clearing)
            CHECK(L::prepareClearAlarm(c, live, state, 41, now, CUTOFF, policy));
        else
            CHECK(L::prepareSetDevicePosition(c, live, state, 0, axis, 41, now, CUTOFF, policy));
        const unsigned slot = which == 3 ? 1 : which >= 4 ? which - 4 : 0;
        for (unsigned i = 0; i < slot; ++i) {
            L::PreparedAction p;
            CHECK(L::nextAction(c, live, current, now, p) && p.ready);
            noticeAt(now);
            ++now;
            CHECK(L::advanceAction(c, live, current, acknowledged(p.transfer, now, now), now));
            prefix = true;
        }
        L::PreparedAction p;
        CHECK(L::nextAction(c, live, current, STEP_AT, p) && p.ready);
        work = p.transfer;
    }
    void notice() { noticeAt(STEP_AT); }
    M::Status consume(const C::SdoCompletion& e, uint64_t now) {
        return L::advanceAction(c, live, current, e, now);
    }
    void settled(M::ActionOutcome outcome, const C::SdoCompletion& e, bool noted, bool possible, uint64_t now) {
        CHECK(c.state == M::ActionState::FAILED && c.outcome == outcome);
        CHECK(c.status.detail == int32_t(M::ActionError::TIME) && c.status.msg && *c.status.msg);
        // Settled evidence refines the notice, and an earlier acknowledged prefix is never undone.
        const unsigned slot = variant == 3 ? 1 : variant >= 4 ? variant - 4 : 0;
        const bool effecting = slot == (counter() ? 1u : 0u);
        const M::ActionExecution execution =
            effecting ? (possible ? M::ActionExecution::UNKNOWN : M::ActionExecution::NOT_TRANSMITTED)
            : slot > (counter() ? 1u : 0u) ? M::ActionExecution::ACKNOWLEDGED
                                           : M::ActionExecution::NOT_TRANSMITTED;
        CHECK(c.execution == execution);
        CHECK(c.effectsPossible == (possible || prefix));
        CHECK(c.uncertain == (possible || prefix));
        CHECK(c.completion == M::ActionCompletion::NOT_OBSERVED);
        CHECK(sameTerminal(c.commandHistory[slot], e));
        CHECK(c.commandEvidenceMask == uint8_t((2u << slot) - 1u));
        if (counter()) // The coordinate withdrawal happens at the notice and is never rolled back.
            CHECK(c.coordinatesInvalidated == (slot == 2 || (slot == 1 && noted)));
        L::PreparedAction later;
        later.eligibleUs = 12345;
        CHECK(L::nextAction(c, live, current, CUTOFF + 5000, later) && !later.ready);
        const Saved<L::ActionContext> closed(c);
        CHECK(refused(L::advanceAction(c, live, current, e, now + 1)));
        CHECK(refused(L::advanceAction(c, live, current, M::ActionOutcome::CANCELLED, now + 1)));
        CHECK(refused(L::advanceAction(c, live, current, M::ActionOutcome::DEADLINE, CUTOFF + 50)));
        CHECK(refused(L::advanceAction(c, live, current, M::ActionOutcome::INVALIDATED, now + 1)));
        if (!counter())
            CHECK(refused(L::noteActionTransmission(c, live, current, now + 1)));
        CHECK(closed.same(c));
    }
};
// An acknowledged stop or clear, then an observation upload: 0 first field, 1 a later field of the
// same cycle, 2 the clear's one final ERRORS read.
struct ActionObserveRig {
    L::ActionContext c;
    L::AxisReservation live = reservation();
    L::ReadTarget current = target();
    C::SdoTransfer work;
    bool download = false;
    uint64_t latest = 0;
    ActionObserveRig(unsigned variant, const L::ReadOptions& o) {
        uint64_t now = ACQUIRED;
        const C::SdoCompletion control = controlWord(0x000f, now + 1, now + 2);
        now += 2;
        L::ReadContext state;
        const bool clearing = variant == 2;
        CHECK(L::prepareState(state, target(), 2, now, CUTOFF, options(),
                              clearing ? mask({L::StateBlock::ERRORS, L::StateBlock::ERROR_HISTORY,
                                               L::StateBlock::MONITORS, L::StateBlock::STATUS})
                                       : PREREQUISITE));
        if (clearing) {
            finishRead(state, now, {{0x2100, L::MONITOR_ALARM}, {0x6041, 7}, {0x6061, 1}, {0x603f, 0xff05}, {0x1003, 0}});
            CHECK(L::prepareClearAlarm(c, live, state, 41, now, CUTOFF, actionOptions(o)));
        } else {
            finishRead(state, now, {{0x2100, L::MONITOR_RUNNING}, {0x6041, 7}, {0x6061, 1}});
            CHECK(L::prepareStop(c, live, state, control, L::StopKind::HALT, 41, now, CUTOFF, actionOptions(o)));
        }
        while (c.phase == L::ActionPhase::COMMAND) {
            L::PreparedAction p;
            CHECK(L::nextAction(c, live, current, now, p) && p.ready);
            CHECK(L::noteActionTransmission(c, live, current, now));
            ++now;
            CHECK(L::advanceAction(c, live, current, acknowledged(p.transfer, now, now), now));
        }
        // Reach the wanted field. Reports here show no alarm, so the clear goes to its final read.
        const unsigned fields = variant == 0 ? 0 : variant == 1 ? 3 : 5;
        for (unsigned i = 0; i < fields; ++i) {
            L::PreparedAction p;
            CHECK(L::nextAction(c, live, current, now, p) && p.ready);
            const uint32_t raw = p.transfer.index == 0x6041 ? 7 : p.transfer.index == 0x6061 ? 1 : 0;
            ++now;
            CHECK(L::advanceAction(c, live, current, acknowledged(p.transfer, now, now, raw), now));
        }
        CHECK(c.finalErrorRead == clearing);
        L::PreparedAction p;
        CHECK(L::nextAction(c, live, current, STEP_AT, p) && p.ready);
        work = p.transfer;
    }
    void notice() {}
    M::Status consume(const C::SdoCompletion& e, uint64_t now) {
        return L::advanceAction(c, live, current, e, now);
    }
    void settled(M::ActionOutcome outcome, const C::SdoCompletion& e, bool, bool, uint64_t now) {
        CHECK(c.state == M::ActionState::FAILED && c.outcome == outcome);
        CHECK(c.status.detail == int32_t(M::ActionError::TIME));
        // The acknowledged command survives; only its observation failed.
        CHECK(c.execution == M::ActionExecution::ACKNOWLEDGED && c.commandEvidenceMask);
        CHECK(c.effectsPossible && c.uncertain && c.completion == M::ActionCompletion::NOT_OBSERVED);
        CHECK(c.observationEvidencePresent && sameTerminal(c.observationEvidence, e));
        CHECK(sameTerminal(c.read.evidence, e));
        const Saved<L::ActionContext> closed(c);
        CHECK(refused(L::advanceAction(c, live, current, e, now + 1)));
        CHECK(refused(L::advanceAction(c, live, current, M::ActionOutcome::CANCELLED, now + 1)));
        CHECK(closed.same(c));
    }
};

template <class Rig, class Outcome>
void budgetTimeline(std::initializer_list<unsigned> variants, Outcome early, Outcome late) {
    for (unsigned variant : variants)
        for (Reach reach : reaches)
            for (int offset : {-1, 0, 1})
                for (uint64_t lag : {uint64_t(0), uint64_t(1)})
                    for (bool noted : {false, true}) {
                        const bool possible = mayHaveLeft(reach);
                        Rig rig(variant, budgetEndingAt(reach, offset));
                        // An upload has no notice, and a possible download always carries one.
                        if (rig.download ? (possible && !noted) : noted)
                            continue;
                        if (noted)
                            rig.notice();
                        const uint64_t ending = offset < 0 ? CUTOFF - 1 : CUTOFF;
                        const C::SdoCompletion e = realTimeout(rig.work, reach, ending);
                        const uint64_t now = ending + lag;
                        const Saved<Rig> pending(rig);
                        // A terminal consumed before it could have happened changes nothing.
                        CHECK(refused(rig.consume(e, ending - 1)));
                        CHECK(pending.same(rig));
                        CHECK(rig.consume(e, now));
                        rig.settled(ending < CUTOFF && now < CUTOFF ? early : late, e, noted, possible, now);
                    }
}
void bestEffortBudgetTimeline() {
    // Confirmed TX of a best-effort stop finishes as UNCONFIRMED_SENT and never reaches a budget.
    for (Reach reach : {Reach::UNSERVED, Reach::STAGED, Reach::HANDED_OFF})
        for (int offset : {-1, 0, 1})
            for (uint64_t lag : {uint64_t(0), uint64_t(1)}) {
                L::BestEffortStop c;
                CHECK(L::prepareBestEffortStop(c, target(), 41, STEP_AT, CUTOFF, budgetEndingAt(reach, offset)));
                L::PreparedAction p;
                CHECK(L::nextBestEffortStop(c, target(), STEP_AT, p) && p.ready);
                CHECK(L::noteBestEffortAdmission(c, STEP_AT));
                const uint64_t ending = offset < 0 ? CUTOFF - 1 : CUTOFF;
                const C::SdoCompletion e = realTimeout(p.transfer, reach, ending, true);
                const uint64_t now = ending + lag;
                CHECK(L::advanceBestEffortStop(c, e, now));
                CHECK(c.state == M::ActionState::FAILED);
                // Only a budget that ended, and was processed, before the cutoff is a service timeout.
                CHECK(c.outcome == (ending < CUTOFF && now < CUTOFF ? M::ActionOutcome::SERVICE_TIMEOUT
                                                                    : M::ActionOutcome::DEADLINE));
                CHECK(c.execution == (mayHaveLeft(reach) ? M::ActionExecution::UNKNOWN
                                                         : M::ActionExecution::NOT_TRANSMITTED));
                CHECK(c.effectsPossible == mayHaveLeft(reach) && c.commandEvidencePresent);
                const Saved<L::BestEffortStop> closed(c);
                CHECK(refused(L::advanceBestEffortStop(c, e, now + 1)));
                CHECK(refused(L::advanceBestEffortStop(c, M::ActionOutcome::CANCELLED, now + 1)));
                CHECK(refused(L::noteBestEffortAdmission(c, now + 1)));
                CHECK(closed.same(c));
            }
}
// An action observation still open at the cutoff could leave its channel unresolved (trial 213),
// so none is handed out once one exchange's budgets no longer fit, and no budget can end against
// the cutoff for one. The rigs above keep that timeline for every other kind of work.
void actionObservationHeldBack() {
    for (unsigned variant : {0u, 1u, 2u}) {
        ActionObserveRig rig(variant, options()); // 100 + 100 us.
        L::PreparedAction p;
        CHECK(L::nextAction(rig.c, rig.live, rig.current, CUTOFF - 200, p) && p.ready &&
              C::sameSdoWork(p.transfer, rig.work));
        const Saved<L::ActionContext> before(rig.c);
        for (uint64_t at : {CUTOFF - 199, CUTOFF - 1}) {
            L::PreparedAction late;
            CHECK(L::nextAction(rig.c, rig.live, rig.current, at, late) && !late.ready);
        }
        CHECK(before.same(rig.c));
        // A terminal for the work handed out before the span still settles as before.
        CHECK(rig.consume(acknowledged(rig.work, CUTOFF - 200, CUTOFF - 1), CUTOFF - 1));
    }
}
void budgetTimelines() {
    budgetTimeline<ReadRig>({0}, M::ReadOutcome::SERVICE_TIMEOUT, M::ReadOutcome::DEADLINE);
    budgetTimeline<TuningRig>({0}, M::ReadOutcome::SERVICE_TIMEOUT, M::ReadOutcome::DEADLINE);
    budgetTimeline<SettingsWriteRig>({0, 1, 2}, M::ReadOutcome::SERVICE_TIMEOUT, M::ReadOutcome::DEADLINE);
    budgetTimeline<SettingsReadbackRig>({0, 1}, M::ReadOutcome::SERVICE_TIMEOUT, M::ReadOutcome::DEADLINE);
    budgetTimeline<ActionCommandRig>({0, 1, 2, 3, 4, 5, 6}, M::ActionOutcome::SERVICE_TIMEOUT,
                                     M::ActionOutcome::DEADLINE);
    actionObservationHeldBack();
    bestEffortBudgetTimeline();
}

// Possible TX with no earlier notice cannot be believed, whichever budget ended.
void timeoutWithoutNoticeIsRefused() {
    SettingsWriteRig settings(0, budgetEndingAt(Reach::CONFIRMED, -1));
    const C::SdoCompletion e = realTimeout(settings.work, Reach::CONFIRMED, CUTOFF - 1);
    const Saved<L::DriverSettingsContext> before(settings.c);
    CHECK(refusedAs(settings.consume(e, CUTOFF - 1), M::ReadError::CORRELATION));
    CHECK(before.same(settings.c));
    ActionCommandRig action(0, budgetEndingAt(Reach::CONFIRMED, -1));
    const C::SdoCompletion a = realTimeout(action.work, Reach::CONFIRMED, CUTOFF - 1);
    const Saved<L::ActionContext> beforeAction(action.c);
    CHECK(refused(action.consume(a, CUTOFF - 1)));
    CHECK(beforeAction.same(action.c));
    // An outcome beyond the vocabulary cannot pass for a retained action terminal.
    action.c.outcome = static_cast<M::ActionOutcome>(int(M::ActionOutcome::SERVICE_TIMEOUT) + 1);
    L::PreparedAction later;
    later.eligibleUs = 12345;
    const Saved<L::PreparedAction> blank(later);
    CHECK(refused(L::nextAction(action.c, action.live, action.current, STEP_AT, later)) && blank.same(later));
}

// The other service failures keep their own causes before the cutoff, and all yield to it.
void otherOutcomesAgainstCutoff() {
    struct Row {
        C::SdoOutcome service;
        M::ReadOutcome read;
        M::ActionOutcome action;
        bool invalidates;
    };
    const Row rows[] = {
        {C::SdoOutcome::SERVER_ABORT, M::ReadOutcome::PROTOCOL_REJECTED, M::ActionOutcome::PROTOCOL_REJECTED, false},
        {C::SdoOutcome::CLIENT_ABORT, M::ReadOutcome::REPLY_ERROR, M::ActionOutcome::REPLY_ERROR, false},
        {C::SdoOutcome::UNSUPPORTED, M::ReadOutcome::REPLY_ERROR, M::ActionOutcome::REPLY_ERROR, false},
        {C::SdoOutcome::CANCELLED, M::ReadOutcome::CANCELLED, M::ActionOutcome::CANCELLED, false},
        {C::SdoOutcome::TRANSPORT_ERROR, M::ReadOutcome::TRANSPORT_ERROR, M::ActionOutcome::TRANSPORT_ERROR, false},
        {C::SdoOutcome::BUS_OFF, M::ReadOutcome::TRANSPORT_ERROR, M::ActionOutcome::TRANSPORT_ERROR, true},
        {C::SdoOutcome::CORRELATION_LOST, M::ReadOutcome::TRANSPORT_ERROR, M::ActionOutcome::TRANSPORT_ERROR, true},
        {C::SdoOutcome::REBOOT, M::ReadOutcome::INVALIDATED, M::ActionOutcome::INVALIDATED, true},
        {C::SdoOutcome::BINDING_INVALIDATED, M::ReadOutcome::INVALIDATED, M::ActionOutcome::INVALIDATED, true},
    };
    for (const Row& row : rows)
        for (uint64_t at : {CUTOFF - 1, CUTOFF}) {
            const bool early = at < CUTOFF;
            ReadRig read(0, options());
            CHECK(read.consume(failed(read.work, STEP_AT, at, row.service), at));
            CHECK(read.c.outcome == (early ? row.read : M::ReadOutcome::DEADLINE));
            CHECK(read.c.invalidated == row.invalidates); // The cutoff does not erase a withdrawal.

            SettingsWriteRig write(0, options());
            write.notice();
            CHECK(write.consume(failed(write.work, STEP_AT, at, row.service), at));
            CHECK(write.c.outcome == (early ? row.read : M::ReadOutcome::DEADLINE));

            SettingsReadbackRig back(0, options());
            CHECK(back.consume(failed(back.work, STEP_AT, at, row.service), at));
            CHECK(back.c.outcome == (early ? row.read : M::ReadOutcome::DEADLINE));
            CHECK(back.c.readback.read.invalidated == row.invalidates);

            ActionCommandRig command(0, options());
            command.notice();
            CHECK(command.consume(failed(command.work, STEP_AT, at, row.service), at));
            CHECK(command.c.outcome == (early ? row.action : M::ActionOutcome::DEADLINE));

            ActionObserveRig observe(1, options());
            CHECK(observe.consume(failed(observe.work, STEP_AT, at, row.service), at));
            CHECK(observe.c.outcome == (early ? row.action : M::ActionOutcome::DEADLINE));
            CHECK(observe.c.read.invalidated == row.invalidates);
        }
    // A success arriving or processed at the cutoff publishes nothing, whichever way it reached it.
    for (uint64_t completed : {CUTOFF - 1, CUTOFF})
        for (uint64_t now : {completed, completed + 1}) {
            const bool early = completed < CUTOFF && now < CUTOFF;
            ReadRig read(0, options());
            CHECK(read.consume(acknowledged(read.work, STEP_AT, completed, 7), now));
            CHECK(read.c.values[2].valid == early);
            if (!early)
                CHECK(read.c.outcome == M::ReadOutcome::DEADLINE);
            SettingsReadbackRig back(0, options());
            CHECK(back.consume(acknowledged(back.work, STEP_AT, completed, 2), now));
            CHECK(back.c.readback.read.values[0].valid == early);
            if (!early)
                CHECK(back.c.state == L::SettingsState::FAILED && back.c.outcome == M::ReadOutcome::DEADLINE);
            ActionCommandRig command(0, options());
            command.notice();
            CHECK(command.consume(acknowledged(command.work, STEP_AT, completed), now));
            if (!early)
                CHECK(command.c.state == M::ActionState::FAILED &&
                      command.c.outcome == M::ActionOutcome::DEADLINE &&
                      command.c.execution == M::ActionExecution::ACKNOWLEDGED);
        }
}

// Success stays success: nothing later moves it, not even a request to invalidate or a later time.
void publishedSuccessIsStable() {
    uint64_t now = ACQUIRED;
    L::ReadContext read;
    CHECK(L::prepareState(read, target(), 1, now, CUTOFF, options(), PREREQUISITE));
    finishRead(read, now, {{0x6041, 0x8107}, {0x6061, 1}});
    C::SdoCompletion last = read.evidence;
    const Saved<L::ReadContext> done(read);
    CHECK(refused(L::advanceRead(read, last, now + 1)));
    CHECK(refused(L::advanceRead(read, M::ReadOutcome::INVALIDATED, now + 1)));
    CHECK(refused(L::advanceRead(read, M::ReadOutcome::CANCELLED, now + 1)));
    CHECK(refused(L::advanceRead(read, M::ReadOutcome::DEADLINE, CUTOFF + 1)));
    L::PreparedRead later;
    CHECK(L::nextRead(read, CUTOFF + 10, later) && !later.ready);
    CHECK(done.same(read) && read.outcome == M::ReadOutcome::SUCCESS);
    // Age is a publication question and never rewrites the outcome.
    L::StateObservation block;
    const Saved<L::StateObservation> blank(block);
    const M::Status expired = L::getStateBlock(read, L::StateBlock::STATUS, target(), CUTOFF + 10, 10, block);
    CHECK(refusedAs(expired, M::ReadError::TIME) && blank.same(block) && done.same(read));
    CHECK(L::getStateBlock(read, L::StateBlock::STATUS, target(), CUTOFF + 10, CUTOFF, block));
    CHECK(block.statusword == 0x8107);
}

// CORRELATION_LOST withdraws the earlier blocks, keeps their bytes as history, and only a new
// preparation clears either.
void correlationLossWithdrawsEarlierBlocks() {
    const L::StateBlock all[] = {L::StateBlock::ERRORS, L::StateBlock::ERROR_HISTORY,
                                 L::StateBlock::MONITORS, L::StateBlock::STATUS,
                                 L::StateBlock::FEEDBACK, L::StateBlock::INPUTS};
    // Lose the correlation at each field of a read selecting every block.
    for (unsigned lostAt = 1; lostAt < 15; ++lostAt) {
        L::ReadContext c;
        uint64_t now = ACQUIRED;
        CHECK(L::prepareState(c, target(), 1, now, CUTOFF, options(), 0x3f));
        for (unsigned i = 0; i < lostAt; ++i) {
            L::PreparedRead p;
            CHECK(L::nextRead(c, now, p));
            ++now;
            const uint32_t raw = p.transfer.index == 0x1003 && !p.transfer.subindex ? 4 : 0x40 + i;
            CHECK(L::advanceRead(c, acknowledged(p.transfer, now, now, raw), now));
        }
        // A control copy times out at the same field, which withdraws nothing.
        L::ReadContext timedOut = c;
        L::PreparedRead p;
        CHECK(L::nextRead(c, now, p));
        ++now;
        const C::SdoCompletion lost = failed(p.transfer, now, now, C::SdoOutcome::CORRELATION_LOST);
        L::ReadContext history;
        std::memcpy(static_cast<void*>(&history), &c, sizeof c);
        CHECK(L::advanceRead(c, lost, now));
        CHECK(c.state == M::ReadState::FAILED && c.invalidated && c.uncertain);
        CHECK(c.outcome == M::ReadOutcome::TRANSPORT_ERROR);
        CHECK(c.status.detail == int32_t(M::ReadError::STATE));
        CHECK(sameTerminal(c.evidence, lost));
        // Historical values and provenance are untouched, byte for byte.
        CHECK(!std::memcmp(c.values, history.values, sizeof c.values));
        CHECK(c.count == history.count && c.cursor == history.cursor);
        const C::SdoCompletion budget = failed(p.transfer, now, now, C::SdoOutcome::TIMEOUT);
        CHECK(L::advanceRead(timedOut, budget, now));
        CHECK(!timedOut.invalidated && timedOut.outcome == M::ReadOutcome::SERVICE_TIMEOUT);
        unsigned published = 0;
        for (L::StateBlock block : all) {
            L::StateObservation out;
            L::ReadWindow window;
            window.skewUs = 77;
            const Saved<L::StateObservation> blankOut(out);
            const Saved<L::ReadWindow> blankWindow(window);
            CHECK(refusedAs(L::getStateBlock(c, block, target(), now, 100000, out), M::ReadError::TARGET));
            CHECK(refusedAs(L::validateStateBlock(c, block, target(), now, 100000, window), M::ReadError::TARGET));
            CHECK(blankOut.same(out) && blankWindow.same(window));
            published += L::getStateBlock(timedOut, block, target(), now, 100000, out).isOk();
        }
        // What a budget timeout keeps is exactly the completed blocks.
        CHECK(published == (lostAt >= 15 ? 6u : lostAt >= 14 ? 5u : lostAt >= 12 ? 4u : lostAt >= 10 ? 3u : lostAt >= 7 ? 2u : lostAt >= 2 ? 1u : 0u));
        L::StateInterpretation interpretation;
        const Saved<L::StateInterpretation> blankInterpretation(interpretation);
        CHECK(refused(L::getStateInterpretation(c, target(), now, 100000, L::StatusLayout::EN86, interpretation)));
        CHECK(blankInterpretation.same(interpretation));

        // Withdrawn state cannot support prerequisites either.
        L::DriverContext prior;
        uint64_t later = now;
        CHECK(L::prepareDriverRead(prior, target(), 2, later, CUTOFF, options(), L::DriverGroup::MOTION_PARAMETERS));
        finishRead(prior.read, later, {});
        const C::SdoCompletion control = controlWord(0x000f, ACQUIRED, ACQUIRED);
        L::DriverSettingsContext settings;
        const Saved<L::DriverSettingsContext> blankSettings(settings);
        L::DriverSettings candidate;
        candidate.positionSpeedRpm = candidate.rampUpTimeMs = candidate.rampDownTimeMs = 2;
        CHECK(refusedAs(L::prepareDriverSettings(settings, reservation(), reservation().target, prior, c, control, candidate, 41, later,
                                                 CUTOFF, options(), 100000),
                        M::ReadError::STATE));
        CHECK(blankSettings.same(settings));
        L::ActionContext action;
        const Saved<L::ActionContext> blankAction(action);
        CHECK(refused(L::prepareStop(action, reservation(), c, control, L::StopKind::HALT, 41, later, CUTOFF, actionOptions())));
        CHECK(refused(L::prepareClearAlarm(action, reservation(), c, 41, later, CUTOFF, actionOptions())));
        CHECK(blankAction.same(action));

        // A refused preparation changes neither the withdrawal nor the history.
        const Saved<L::ReadContext> closed(c);
        CHECK(refused(L::prepareState(c, target(), 0, later, CUTOFF, options(), 0x3f)));
        CHECK(refused(L::prepareState(c, target(), 3, later, later, options(), 0x3f)));
        CHECK(refused(L::prepareState(c, target(), 3, later, CUTOFF, options(), 0)));
        CHECK(closed.same(c));
        // Only a new preparation resets it, leaving none of the old bytes in force.
        CHECK(L::prepareState(c, target(), 3, later, CUTOFF, options(), PREREQUISITE));
        CHECK(!c.invalidated && !c.uncertain && !c.evidencePresent && c.state == M::ReadState::ACTIVE);
        CHECK(c.outcome == M::ReadOutcome::NONE);
        for (const L::ReadValue& value : c.values)
            CHECK(!value.valid && !value.raw);
        L::StateObservation stillEmpty;
        const Saved<L::StateObservation> blankEmpty(stillEmpty);
        CHECK(refused(L::getStateBlock(c, L::StateBlock::MONITORS, target(), later, 100000, stillEmpty)));
        CHECK(blankEmpty.same(stillEmpty));
        finishRead(c, later, {{0x6041, 7}});
        CHECK(L::getStateBlock(c, L::StateBlock::STATUS, target(), later, 100000, stillEmpty));
        CHECK(stillEmpty.statusword == 7);
        L::resetRead(c);
        CHECK(c.state == M::ReadState::EMPTY && !c.invalidated);
    }
    // A local withdrawal invalidates; a local cancellation of the same read does not.
    for (M::ReadOutcome reason : {M::ReadOutcome::CANCELLED, M::ReadOutcome::INVALIDATED}) {
        L::ReadContext c;
        uint64_t now = ACQUIRED;
        CHECK(L::prepareState(c, target(), 1, now, CUTOFF, options(), PREREQUISITE));
        for (unsigned i = 0; i < 3; ++i) {
            L::PreparedRead p;
            CHECK(L::nextRead(c, now, p));
            ++now;
            CHECK(L::advanceRead(c, acknowledged(p.transfer, now, now, 9), now));
        }
        CHECK(L::advanceRead(c, reason, now));
        L::StateObservation out;
        const M::Status status = L::getStateBlock(c, L::StateBlock::MONITORS, target(), now, 1000, out);
        CHECK(status.isOk() == (reason == M::ReadOutcome::CANCELLED));
    }
}

// A move's own write, as a future move owner would admit it.
C::SdoTransfer moveWork(const L::ReadTarget& route, uint16_t index, uint8_t subindex = 0, uint8_t size = 4,
                        C::SdoDirection direction = C::SdoDirection::DOWNLOAD) {
    C::SdoTransfer t;
    t.channel = route.channel;
    t.operation = 51;
    t.request = 52;
    t.step = 2;
    t.index = index;
    t.subindex = subindex;
    t.direction = direction;
    t.size = size;
    t.deadlineUs = 5000;
    t.queueTimeoutUs = t.responseTimeoutUs = 100;
    return t;
}
// The operation-owned writes at their EDS widths, as Settings.h states them for prompts 35 and 36.
// 607A alone changes no field in this group. Velocity setup conservatively retires it at 60FF.
struct MoveRow {
    uint16_t index;
    uint8_t subindex, size;
};
const MoveRow retiringWrites[] = {{0x60ff, 0, 4}, {0x6081, 0, 4}, {0x6083, 0, 4}, {0x6084, 0, 4}, {0x6098, 0, 1},
                                  {0x6099, 1, 4}, {0x6099, 2, 4}, {0x609a, 0, 4}, {0x607c, 0, 4}};
const MoveRow targetWrites[] = {{0x607a, 0, 4}};
C::SdoTransfer moveWork(const L::ReadTarget& route, const MoveRow& row) {
    return moveWork(route, row.index, row.subindex, row.size);
}
bool listedWrite(uint32_t index, unsigned subindex, unsigned size) {
    for (const MoveRow& row : retiringWrites)
        if (row.index == index && row.subindex == subindex && row.size == size)
            return true;
    for (const MoveRow& row : targetWrites)
        if (row.index == index && row.subindex == subindex && row.size == size)
            return true;
    return false;
}
const uint32_t OLD_SPEED = 100, OLD_UP = 200, OLD_DOWN = 300;
L::DriverSettings retainedValues() {
    L::DriverSettings s;
    s.positionSpeedRpm = OLD_SPEED;
    s.rampUpTimeMs = OLD_UP;
    s.rampDownTimeMs = OLD_DOWN;
    return s;
}
struct RetirementRig {
    L::AxisReservation live = reservation();
    C::SdoCompletion control;
    L::ReadContext state;
    L::DriverContext prior;
    uint64_t now = ACQUIRED;
    RetirementRig() {
        control = controlWord(0x000f, now + 1, now + 2);
        now += 2;
        CHECK(L::prepareState(state, target(), 2, now, 1000000, options(), PREREQUISITE));
        finishRead(state, now, {{0x2100, 0}, {0x6041, 7}, {0x6061, 1}});
        prior = motionRead(OLD_SPEED);
    }
    L::DriverContext motionRead(uint32_t speed) {
        L::DriverContext read;
        CHECK(L::prepareDriverRead(read, target(), 3, now, 1000000, options(), L::DriverGroup::MOTION_PARAMETERS));
        finishRead(read.read, now, {{0x6081, speed}, {0x6083, OLD_UP}, {0x6084, OLD_DOWN}});
        return read;
    }
    M::Status prepareNoOp(L::DriverSettingsContext& out, const L::DriverContext& from) {
        return L::prepareDriverSettings(out, live, live.target, from, state, control, retainedValues(), 41, now, 1000000,
                                        options(), 1000000);
    }
};
// The retirement sets exactly one flag: everything else, including padding, is history.
void retiredExactly(const L::DriverContext& retained, const Saved<L::DriverContext>& before) {
    L::DriverContext expected;
    std::memcpy(static_cast<void*>(&expected), before.bytes, sizeof expected);
    expected.read.invalidated = true;
    CHECK(!std::memcmp(&expected, &retained, sizeof expected));
}
void moveWritesRetireMotionParameters() {
    const L::ReadTarget route = target();
    // Each retiring write withdraws the retained observation, and the epochs do not move.
    for (const MoveRow& row : retiringWrites) {
        RetirementRig r;
        L::DriverSettingsContext noOp;
        CHECK(r.prepareNoOp(noOp, r.prior)); // The hazard: retained values let this succeed with no TX.
        CHECK(noOp.state == L::SettingsState::SUCCEEDED && !noOp.changed);
        const M::Binding binding = route.channel.binding;
        const Saved<L::DriverContext> before(r.prior);
        CHECK(L::retireMotionParameters(r.prior, route, moveWork(route, row)));
        retiredExactly(r.prior, before);
        CHECK(M::sameBinding(binding, route.channel.binding));
        // Getter and later no-op refuse, atomically.
        L::DriverObservation observed;
        const Saved<L::DriverObservation> blank(observed);
        CHECK(refusedAs(L::getDriver(r.prior, route, r.now, 1000000, observed), M::ReadError::TARGET));
        CHECK(blank.same(observed));
        L::DriverSettingsContext refusedNoOp;
        refusedNoOp.operation = 0xbeef;
        const Saved<L::DriverSettingsContext> blankSettings(refusedNoOp);
        CHECK(refusedAs(r.prepareNoOp(refusedNoOp, r.prior), M::ReadError::STATE));
        CHECK(blankSettings.same(refusedNoOp));
        // Repeated retirement is the same.
        const Saved<L::DriverContext> once(r.prior);
        CHECK(L::retireMotionParameters(r.prior, route, moveWork(route, row)));
        CHECK(once.same(r.prior));
        // A fresh read replaces it. If the drive still holds the old values the no-op is honest.
        L::DriverContext same = r.motionRead(OLD_SPEED);
        CHECK(!same.read.invalidated);
        CHECK(r.prepareNoOp(noOp, same) && !noOp.changed && noOp.state == L::SettingsState::SUCCEEDED);
        // If it holds the move's value, the same candidate is real work.
        L::DriverContext moved = r.motionRead(777);
        L::DriverSettingsContext work;
        CHECK(r.prepareNoOp(work, moved) && work.state == L::SettingsState::WRITING && work.changed == 1);
    }
    // A target write has nothing in this group to retire: accepted, every byte unchanged.
    for (const MoveRow& row : targetWrites) {
        RetirementRig r;
        const Saved<L::DriverContext> before(r.prior);
        CHECK(L::retireMotionParameters(r.prior, route, moveWork(route, row)));
        CHECK(before.same(r.prior));
        L::DriverObservation observed;
        CHECK(L::getDriver(r.prior, route, r.now, 1000000, observed) && observed.positionSpeedRpm == OLD_SPEED);
        L::DriverSettingsContext noOp;
        CHECK(r.prepareNoOp(noOp, r.prior) && noOp.state == L::SettingsState::SUCCEEDED);
    }
    // The accepted set is exactly the table: every object, at every plausible subindex and width.
    {
        L::DriverContext empty;
        const Saved<L::DriverContext> blank(empty);
        unsigned accepted = 0;
        for (uint32_t index = 0; index <= 0xffff; ++index)
            for (unsigned subindex : {0u, 1u, 2u, 3u})
                for (unsigned size : {1u, 2u, 4u}) {
                    const M::Status status = L::retireMotionParameters(
                        empty, route, moveWork(route, uint16_t(index), uint8_t(subindex), uint8_t(size)));
                    const bool listed = listedWrite(index, subindex, size);
                    if (status.isOk() != listed || (!listed && !refusedAs(status, M::ReadError::ARGUMENT))) {
                        std::fprintf(stderr, "write %04x:%02x width %u\n", unsigned(index), subindex, size);
                        std::abort();
                    }
                    accepted += listed;
                }
        CHECK(accepted == 10 && blank.same(empty));
    }
    // A refused write changes nothing in a context that a valid one would retire.
    {
        RetirementRig r;
        const Saved<L::DriverContext> before(r.prior);
        const auto refuse = [&](const C::SdoTransfer& work, M::ReadError detail) {
            CHECK(refusedAs(L::retireMotionParameters(r.prior, route, work), detail));
            CHECK(before.same(r.prior));
        };
        const auto wrongShapes = [&](const MoveRow& row) {
            refuse(moveWork(route, row.index, uint8_t(row.subindex + 3), row.size), M::ReadError::ARGUMENT);
            refuse(moveWork(route, row.index, row.subindex, uint8_t(row.size + 1)), M::ReadError::ARGUMENT);
            refuse(moveWork(route, row.index, row.subindex, uint8_t(row.size - 1)), M::ReadError::ARGUMENT);
            refuse(moveWork(route, row.index, row.subindex, row.size, C::SdoDirection::UPLOAD), M::ReadError::ARGUMENT);
            for (unsigned which = 0; which < 3; ++which) {
                C::SdoTransfer unidentified = moveWork(route, row);
                if (which == 0)
                    unidentified.operation = 0;
                if (which == 1)
                    unidentified.request = 0;
                if (which == 2)
                    unidentified.step = 0;
                refuse(unidentified, M::ReadError::ARGUMENT);
            }
            for (unsigned which = 0; which < 5; ++which) {
                C::SdoTransfer elsewhere = moveWork(route, row);
                if (which == 0)
                    ++elsewhere.channel.clientToServerId;
                if (which == 1)
                    ++elsewhere.channel.serverToClientId;
                if (which == 2)
                    ++elsewhere.channel.binding.logicalAxisId;
                if (which == 3)
                    --elsewhere.channel.binding.configurationGeneration; // An older route's write.
                if (which == 4)
                    --elsewhere.channel.binding.bindingGeneration;
                refuse(elsewhere, M::ReadError::TARGET);
            }
        };
        for (const MoveRow& row : retiringWrites)
            wrongShapes(row);
        for (const MoveRow& row : targetWrites)
            wrongShapes(row);
        refuse(moveWork(route, 0x6099, 0, 1), M::ReadError::ARGUMENT); // The count entry is no phase.
        refuse(moveWork(route, 0x6040, 0, 2), M::ReadError::ARGUMENT);
        refuse(moveWork(route, 0x6060, 0, 1), M::ReadError::ARGUMENT);
        refuse(moveWork(route, 0x607d, 1, 4), M::ReadError::ARGUMENT);
        // An unusable current route.
        L::ReadTarget unusable = route;
        unusable.nodeGeneration = 0;
        CHECK(refusedAs(L::retireMotionParameters(r.prior, unusable, moveWork(unusable, 0x6081)), M::ReadError::TARGET));
        unusable = route;
        unusable.channel.serverToClientId = unusable.channel.clientToServerId;
        CHECK(refusedAs(L::retireMotionParameters(r.prior, unusable, moveWork(unusable, 0x6081)), M::ReadError::TARGET));
        // Aliases into the retained context, at its real evidence and target members.
        C::SdoTransfer& insideWork = r.prior.read.evidence.transfer;
        const L::ReadTarget& insideRoute = r.prior.read.target;
        CHECK(refusedAs(L::retireMotionParameters(r.prior, route, insideWork), M::ReadError::OVERLAP));
        CHECK(refusedAs(L::retireMotionParameters(r.prior, insideRoute, moveWork(route, 0x6081)), M::ReadError::OVERLAP));
        CHECK(before.same(r.prior));
    }
    // Another axis is refused: each identity field in turn, in a context of the retiring group.
    for (unsigned which = 0; which < 9; ++which) {
        RetirementRig r;
        L::ReadTarget other = route;
        M::Binding& binding = r.prior.read.target.channel.binding;
        L::ReadTarget& retained = r.prior.read.target;
        switch (which) {
        case 0: ++binding.logicalAxisId; break;
        case 1: ++binding.endpoint.bus; break;
        case 2: ++binding.endpoint.profile; break;
        case 3: ++binding.endpoint.address; break;
        case 4: ++binding.endpoint.channel; break;
        case 5: ++binding.endpoint.subaxis; break;
        case 6: binding.endpoint.protocol = static_cast<M::Protocol>(200); break;
        case 7: ++retained.channel.clientToServerId; break;
        default: ++retained.channel.serverToClientId; break;
        }
        const Saved<L::DriverContext> before(r.prior);
        CHECK(refusedAs(L::retireMotionParameters(r.prior, other, moveWork(other, 0x6083)), M::ReadError::TARGET));
        CHECK(before.same(r.prior));
    }
    // Older and newer epochs of this axis are just as stale, whichever epoch moved.
    for (unsigned which = 0; which < 6; ++which) {
        RetirementRig r;
        L::ReadTarget current = route;
        switch (which) {
        case 0: ++current.channel.binding.configurationGeneration; break;
        case 1: --current.channel.binding.configurationGeneration; break;
        case 2: ++current.channel.binding.bindingGeneration; break;
        case 3: --current.channel.binding.bindingGeneration; break;
        case 4: ++current.nodeGeneration; break;
        default: --current.nodeGeneration; break;
        }
        const Saved<L::DriverContext> before(r.prior);
        CHECK(L::retireMotionParameters(r.prior, current, moveWork(current, 0x6081)));
        retiredExactly(r.prior, before);
    }
    // Other groups, and nothing retained, are unchanged success on any axis.
    {
        RetirementRig r;
        const L::DriverGroup others[] = {
            L::DriverGroup::DRIVER, L::DriverGroup::SOFT_LIMITS, L::DriverGroup::IO_INPUT_CONFIGURATION,
            L::DriverGroup::IO_OUTPUT_CONFIGURATION, L::DriverGroup::IO_INPUT_FILTERS, L::DriverGroup::IO_OUTPUT_COMMAND,
            L::DriverGroup::CONTROL, L::DriverGroup::TUNING_CURRENT_HOLDING, L::DriverGroup::TUNING_FILTER_POSITION,
            L::DriverGroup::TUNING_LOOP_GAINS, L::DriverGroup::TUNING_EDS_GAINS};
        L::ReadTarget elsewhere = route;
        ++elsewhere.channel.binding.logicalAxisId;
        for (L::DriverGroup group : others) {
            L::DriverContext read;
            CHECK(L::prepareDriverRead(read, route, 7, r.now, 1000000, options(), group));
            finishRead(read.read, r.now, {});
            const Saved<L::DriverContext> before(read);
            CHECK(L::retireMotionParameters(read, route, moveWork(route, 0x6081)));
            CHECK(L::retireMotionParameters(read, elsewhere, moveWork(elsewhere, 0x6081)));
            CHECK(before.same(read));
        }
        L::DriverContext state;
        CHECK(L::prepareState(state.read, route, 7, r.now, 1000000, options(), PREREQUISITE));
        finishRead(state.read, r.now, {});
        const Saved<L::DriverContext> stateBefore(state);
        CHECK(L::retireMotionParameters(state, route, moveWork(route, 0x6081)));
        CHECK(stateBefore.same(state));
        L::DriverContext empty;
        const Saved<L::DriverContext> blank(empty);
        CHECK(L::retireMotionParameters(empty, route, moveWork(route, 0x6084)));
        CHECK(L::retireMotionParameters(empty, elsewhere, moveWork(elsewhere, 0x6084)));
        CHECK(blank.same(empty));
    }
    // An active read cannot finish into current evidence, and a failed one stays history.
    {
        RetirementRig r;
        L::DriverContext active;
        CHECK(L::prepareDriverRead(active, route, 8, r.now, 1000000, options(), L::DriverGroup::MOTION_PARAMETERS));
        L::PreparedRead first;
        CHECK(L::nextDriver(active, r.now, first) && first.ready);
        ++r.now;
        CHECK(L::advanceDriver(active, acknowledged(first.transfer, r.now, r.now, OLD_SPEED), r.now));
        const Saved<L::DriverContext> before(active);
        CHECK(L::retireMotionParameters(active, route, moveWork(route, 0x6084)));
        retiredExactly(active, before);
        CHECK(active.read.state == M::ReadState::ACTIVE);
        finishRead(active.read, r.now, {{0x6083, OLD_UP}, {0x6084, OLD_DOWN}});
        CHECK(active.read.state == M::ReadState::SUCCEEDED && active.read.invalidated);
        CHECK(active.read.values[0].raw == OLD_SPEED && active.read.values[2].raw == OLD_DOWN);
        L::DriverObservation observed;
        const Saved<L::DriverObservation> blank(observed);
        CHECK(refusedAs(L::getDriver(active, route, r.now, 1000000, observed), M::ReadError::TARGET));
        CHECK(blank.same(observed));
        L::DriverSettingsContext out;
        CHECK(refusedAs(r.prepareNoOp(out, active), M::ReadError::STATE));
        // A new preparation resets the withdrawal. That read began after the write, by construction.
        CHECK(L::prepareDriverRead(active, route, 9, r.now, 1000000, options(), L::DriverGroup::MOTION_PARAMETERS));
        CHECK(!active.read.invalidated);
        // Failed: the terminal and any partial values remain, and no current read existed anyway.
        L::DriverContext broken;
        CHECK(L::prepareDriverRead(broken, route, 8, r.now, 1000000, options(), L::DriverGroup::MOTION_PARAMETERS));
        L::PreparedRead p;
        CHECK(L::nextDriver(broken, r.now, p));
        const C::SdoCompletion lost = failed(p.transfer, r.now, r.now, C::SdoOutcome::TIMEOUT);
        CHECK(L::advanceDriver(broken, lost, r.now));
        const Saved<L::DriverContext> brokenBefore(broken);
        CHECK(L::retireMotionParameters(broken, route, moveWork(route, 0x6081)));
        retiredExactly(broken, brokenBefore);
        CHECK(sameTerminal(broken.read.evidence, lost));
    }
}
// Runs a motion settings transaction to the wanted end, with a chosen readback for 6081.
void transact(RetirementRig& r, L::DriverSettingsContext& c, M::Binding& binding, uint32_t speedReadback) {
    L::DriverSettings candidate;
    candidate.positionSpeedRpm = 500;
    candidate.rampUpTimeMs = 600;
    candidate.rampDownTimeMs = 700;
    CHECK(L::prepareDriverSettings(c, r.live, r.live.target, r.prior, r.state, r.control, candidate, 41, r.now, 1000000,
                                   options(), 1000000));
    while (c.state == L::SettingsState::WRITING || c.state == L::SettingsState::READING_BACK) {
        const bool writing = c.state == L::SettingsState::WRITING;
        L::PreparedRead p;
        CHECK(L::nextDriver(c, r.live, binding, r.now, p) && p.ready);
        if (writing)
            CHECK(L::noteDriverTransmission(c, r.live, binding, r.now));
        ++r.now;
        const uint32_t raw = writing                         ? 0
                             : p.transfer.index == 0x6081    ? speedReadback
                             : p.transfer.index == 0x6083    ? 600
                                                             : 700;
        CHECK(L::advanceDriver(c, r.live, binding, acknowledged(p.transfer, r.now, r.now, raw), r.now));
    }
}
void settingsContextsAreRetiredToo() {
    // The finished context's readback: the transaction stays a success, its result is withdrawn.
    {
        RetirementRig r;
        M::Binding binding = r.live.target.channel.binding;
        L::DriverSettingsContext c;
        transact(r, c, binding, 500);
        CHECK(c.state == L::SettingsState::SUCCEEDED);
        const L::ReadTarget route = withBinding(binding);
        L::DriverObservation observed;
        CHECK(L::getDriver(c, r.live, route, r.now, 1000000, observed) && observed.positionSpeedRpm == 500);
        const Saved<L::DriverContext> readback(c.readback);
        const Saved<L::DriverObservation> kept(observed);
        CHECK(L::retireMotionParameters(c.readback, route, moveWork(route, 0x6083)));
        retiredExactly(c.readback, readback);
        CHECK(c.state == L::SettingsState::SUCCEEDED && c.outcome == M::ReadOutcome::SUCCESS);
        CHECK(refused(L::getDriver(c, r.live, route, r.now, 1000000, observed)) && kept.same(observed));
        // And it cannot be the next call's prior.
        L::DriverSettingsContext next;
        L::AxisReservation newer = r.live;
        newer.target = route;
        const Saved<L::DriverSettingsContext> blank(next);
        CHECK(refused(L::prepareDriverSettings(next, newer, newer.target, c.readback, r.state, r.control, retainedValues(), 42,
                                               r.now, 1000000, options(), 1000000)));
        CHECK(blank.same(next));
    }
    // A no-op context's copied readback.
    {
        RetirementRig r;
        L::DriverSettingsContext noOp;
        CHECK(r.prepareNoOp(noOp, r.prior) && noOp.state == L::SettingsState::SUCCEEDED);
        L::DriverObservation observed;
        CHECK(L::getDriver(noOp, r.live, target(), r.now, 1000000, observed));
        CHECK(L::retireMotionParameters(noOp.readback, target(), moveWork(target(), 0x6084)));
        CHECK(refused(L::getDriver(noOp, r.live, target(), r.now, 1000000, observed)));
    }
    // Contradictory readback: the raw contradiction stays visible as history, and is not current.
    {
        RetirementRig r;
        M::Binding binding = r.live.target.channel.binding;
        L::DriverSettingsContext c;
        transact(r, c, binding, 501);
        CHECK(c.state == L::SettingsState::FAILED && c.outcome == M::ReadOutcome::REPLY_ERROR);
        const L::ReadTarget route = withBinding(binding);
        CHECK(c.readback.read.values[0].valid && c.readback.read.values[0].raw == 501);
        const Saved<L::DriverContext> readback(c.readback);
        CHECK(L::retireMotionParameters(c.readback, route, moveWork(route, 0x6081)));
        retiredExactly(c.readback, readback);
        CHECK(c.readback.read.values[0].raw == 501 && c.confirmed == 7);
        L::DriverObservation observed;
        const Saved<L::DriverObservation> blank(observed);
        CHECK(refused(L::getDriver(c, r.live, route, r.now, 1000000, observed)) && blank.same(observed));
    }
    // A contradictory move readback in a fresh read: a current read that disagrees is real work,
    // and the configuration epoch still moves only through explicit settings.
    {
        RetirementRig r;
        M::AxisConfig axis;
        axis.binding = target().channel.binding;
        M::PositionRequest request;
        request.configurationGeneration = axis.binding.configurationGeneration;
        request.value = M::Rational(10);
        L::DriverSettings parameters = retainedValues();
        L::PreparedPositionParameters prepared;
        CHECK(L::preparePositionParameters(prepared, L::SettingsModel::CL86_C, request, axis, nullptr, parameters, r.now));
        M::Binding binding = target().channel.binding;
        const L::ReadTarget route = withBinding(binding);
        // No operation-owned write moves an epoch, so none can stale that operation's own target.
        for (const MoveRow& row : retiringWrites) {
            CHECK(L::retireMotionParameters(r.prior, route, moveWork(route, row)));
            CHECK(M::validatePreparedTarget(prepared.target, axis, nullptr, r.now));
        }
        for (const MoveRow& row : targetWrites) {
            CHECK(L::retireMotionParameters(r.prior, route, moveWork(route, row)));
            CHECK(M::validatePreparedTarget(prepared.target, axis, nullptr, r.now));
        }
        L::DriverContext contradiction = r.motionRead(999);
        L::DriverSettingsContext settings;
        CHECK(r.prepareNoOp(settings, contradiction) && settings.state == L::SettingsState::WRITING);
        CHECK(settings.changed == 1 && settings.requested[0] == OLD_SPEED);
        L::PreparedRead p;
        CHECK(L::nextDriver(settings, r.live, binding, r.now, p) && p.ready);
        // An explicit settings write, unlike the move's own phases, withdraws the target.
        CHECK(L::noteDriverTransmission(settings, r.live, binding, r.now));
        axis.binding = binding;
        const M::Status stale = M::validatePreparedTarget(prepared.target, axis, nullptr, r.now);
        CHECK(refused(stale));
    }
}

// Literal per-document table for the exhaustive decode. EN86 inverts zero-speed into running.
struct Layout {
    L::StatusLayout layout;
    uint16_t alarm, running, listed;
    bool runningInverted, effective;
};
const Layout documents[] = {
    {L::StatusLayout::EN86, 0x0080, 0x0400, 0x9587, true, true},
    {L::StatusLayout::ZH86, 0x0008, 0x4000, 0xC10F, false, false},
    {L::StatusLayout::ZH57, 0x0080, 0x4000, 0xC187, false, false},
    {L::StatusLayout::OP86, 0x0080, 0x4000, 0xC187, false, false},
};
L::StateIndication yesNo(bool value) {
    return value ? L::StateIndication::YES : L::StateIndication::NO;
}
void statuswordDecodeIsExhaustive() {
    for (uint32_t word = 0; word <= 0xffff; ++word) {
        const uint16_t w = uint16_t(word);
        L::StatusIndications out;
        CHECK(L::interpretStatusword(w, L::StatusLayout::UNSPECIFIED, out));
        CHECK(out.layout == L::StatusLayout::UNSPECIFIED && out.statusword == w &&
              out.lowStatusBits == (w & 7) && !out.lowPatternListed && out.unlistedStatusBits == w);
        CHECK(out.alarm == L::StateIndication::UNKNOWN && out.running == L::StateIndication::UNKNOWN &&
              out.homingComplete == L::StateIndication::UNKNOWN &&
              out.positionComplete == L::StateIndication::UNKNOWN &&
              out.targetEffective == L::StateIndication::UNKNOWN);
        for (const Layout& document : documents) {
            CHECK(L::interpretStatusword(w, document.layout, out));
            const unsigned low = w & 7u;
            CHECK(out.layout == document.layout && out.statusword == w && out.lowStatusBits == low);
            CHECK(out.lowPatternListed == (low == 0 || low == 1 || low == 3 || low == 7));
            CHECK(out.unlistedStatusBits == uint16_t(w & ~document.listed));
            CHECK(out.alarm == yesNo((w & document.alarm) != 0));
            CHECK(out.running == yesNo(((w & document.running) != 0) != document.runningInverted));
            CHECK(out.homingComplete == yesNo((w & 0x0100) != 0));
            CHECK(out.positionComplete == yesNo((w & 0x8000) != 0));
            CHECK(out.targetEffective == (document.effective ? yesNo((w & 0x1000) != 0)
                                                             : L::StateIndication::UNKNOWN));
        }
    }
    // Unknown documents refuse with nothing changed, across the whole enumeration.
    for (unsigned value = 5; value < 256; ++value) {
        L::StatusIndications out;
        out.statusword = 0xbeef;
        const Saved<L::StatusIndications> blank(out);
        CHECK(refusedAs(L::interpretStatusword(7, static_cast<L::StatusLayout>(value), out), M::ReadError::ARGUMENT));
        CHECK(blank.same(out));
    }
    CHECK(L::MONITOR_POSITION_COMPLETE == 0x01 && L::MONITOR_HOMING_COMPLETE == 0x02 &&
          L::MONITOR_RUNNING == 0x04 && L::MONITOR_ALARM == 0x08 && L::MONITOR_RELEASED == 0x10 &&
          L::MONITOR_POSITIVE_SOFT_LIMIT == 0x20 && L::MONITOR_NEGATIVE_SOFT_LIMIT == 0x40 &&
          L::MONITOR_DOCUMENTED_BITS == 0x7f);
}
L::ReadContext statusAndMonitors(uint16_t statusword, uint16_t monitor, uint64_t& now, uint8_t mode = 1) {
    L::ReadContext c;
    CHECK(L::prepareState(c, target(), 1, now, 1000000, options(), PREREQUISITE));
    finishRead(c, now, {{0x2100, monitor}, {0x6041, statusword}, {0x6061, mode}});
    return c;
}
// A status-only SDO read decodes through the public value function, with no MONITORS block needed.
void statusOnlyReadDecodes() {
    const uint16_t words[] = {0x0000, 0x0007, 0x0087, 0x0408, 0x1000, 0x4000, 0x8107, 0xffff, 0x0200, 0x2000};
    uint64_t now = ACQUIRED;
    for (uint16_t word : words) {
        L::ReadContext c;
        CHECK(L::prepareState(c, target(), 1, now, 1000000, options(), mask({L::StateBlock::STATUS})));
        finishRead(c, now, {{0x6041, word}, {0x6061, 1}});
        L::StateObservation block;
        CHECK(L::getStateBlock(c, L::StateBlock::STATUS, target(), now, 1000000, block));
        CHECK(block.statusword == word && !block.interpreted);
        // The combined getter still needs both blocks; the value function does not.
        L::StateInterpretation combined;
        const Saved<L::StateInterpretation> blank(combined);
        CHECK(refusedAs(L::getStateInterpretation(c, target(), now, 1000000, L::StatusLayout::EN86, combined),
                        M::ReadError::STATE));
        CHECK(blank.same(combined));
        L::StatusIndications decoded;
        CHECK(L::interpretStatusword(block.statusword, L::StatusLayout::EN86, decoded));
        CHECK(decoded.statusword == word && decoded.alarm == yesNo((word & 0x80) != 0));
    }
}
// The contradiction report compares only what the selected document defines, with no winner.
void contradictionReportingAcrossLayouts() {
    const uint16_t words[] = {0x0007, 0x0087, 0x000f, 0x0407, 0x4007, 0x4087, 0x8107, 0x8007, 0x0107, 0x1007, 0x0407 | 0x8000};
    uint64_t now = ACQUIRED;
    for (uint16_t word : words)
        for (unsigned monitor = 0; monitor < 128; ++monitor)
            for (unsigned unlisted : {0u, 0x0080u, 0xff00u}) {
                const uint16_t monitorWord = uint16_t(monitor | unlisted);
                L::ReadContext c = statusAndMonitors(word, monitorWord, now);
                const bool monitorAlarm = (monitor & 8) != 0, monitorRunning = (monitor & 4) != 0;
                const bool monitorHome = (monitor & 2) != 0, monitorPosition = (monitor & 1) != 0;
                L::StateInterpretation out;
                CHECK(L::getStateInterpretation(c, target(), now, 1000000, L::StatusLayout::UNSPECIFIED, out));
                CHECK(!out.alarmDisagreement && !out.runningDisagreement && !out.homingDisagreement &&
                      !out.positionDisagreement);
                CHECK(out.unlistedStatusBits == word && out.unlistedMonitorBits == unlisted);
                for (const Layout& document : documents) {
                    CHECK(L::getStateInterpretation(c, target(), now, 1000000, document.layout, out));
                    const bool alarm = (word & document.alarm) != 0;
                    const bool running = ((word & document.running) != 0) != document.runningInverted;
                    CHECK(out.alarmDisagreement == (alarm != monitorAlarm));
                    CHECK(out.runningDisagreement == (running != monitorRunning));
                    CHECK(out.homingDisagreement == (((word & 0x100) != 0) != monitorHome));
                    CHECK(out.positionDisagreement == (((word & 0x8000) != 0) != monitorPosition));
                    CHECK(out.unlistedStatusBits == uint16_t(word & ~document.listed));
                    CHECK(out.unlistedMonitorBits == unlisted);
                    CHECK(out.monitorReleased == ((monitor & 0x10) != 0));
                    CHECK(out.positiveSoftwareLimit == ((monitor & 0x20) != 0));
                    CHECK(out.negativeSoftwareLimit == ((monitor & 0x40) != 0));
                    CHECK(out.statusword == word && out.monitor == monitorWord);
                }
            }
    // Unknown mode bytes stay raw and do not disturb the decode.
    for (unsigned mode : {0u, 2u, 4u, 0x80u, 0xffu}) {
        L::ReadContext c = statusAndMonitors(7, 0, now, uint8_t(mode));
        L::StateInterpretation out;
        CHECK(L::getStateInterpretation(c, target(), now, 1000000, L::StatusLayout::EN86, out));
        CHECK(out.modeRaw == mode && out.mode == L::ObservedMode::UNKNOWN);
    }
}
// A qualified TPDO1 sample of 6041 decodes through the same function. The decoder is not qualification:
// losing the PDO proof withdraws the block, whatever the bytes still say.
void pdoRawDecode() {
    C::SdoClient owner;
    CHECK(owner.configure(0, target().channel, true, true));
    C::NetworkRoute network;
    network.binding = target().channel.binding;
    network.generation = target().nodeGeneration;
    network.heartbeatId = 0x705;
    network.emcyId = 0x85;
    CHECK(owner.configureNetwork(0, network, C::NetworkObserver(), 0));
    C::PdoMapEvidence map;
    map.route.binding = target().channel.binding;
    map.route.generation = 2;
    map.route.cobId = 0x185;
    C::PdoMap template1;
    CHECK(L::pdoTemplate(C::PdoDirection::TPDO, 1, template1));
    CHECK(C::validatePdoMap(template1, L::pdoProfile(41), map.route.layout));
    map.nodeGeneration = target().nodeGeneration;
    map.source = C::PdoMapSource::REVIEWED_CONFIGURATION;
    map.communicationKnown = true;
    map.cobIdParameter = 0x185;
    map.transmissionType = 255;
    map.evidenceId = 22;
    CHECK(owner.configurePdo(0, 0, map, L::pdoProfile(41), C::PdoObserver(), 0));
    C::PdoQualification q;
    q.route = map.route;
    q.nodeGeneration = target().nodeGeneration;
    q.evidenceId = 33;
    q.basis = C::PdoSampleBasis::PRODUCER_AND_INGRESS_RETIRED;
    q.establishedUs = 1;
    CHECK(owner.qualifyPdo(0, q, 1));

    const uint16_t words[] = {0x0000, 0x0007, 0x0087, 0x0408, 0x1407, 0x4007, 0x8107, 0xffff, 0x0200};
    uint64_t now = 2;
    for (uint16_t word : words) {
        M::Frame sample;
        sample.id = 0x185;
        sample.dlc = sample.payloadLength = 7;
        sample.data[0] = uint8_t(word);
        sample.data[1] = uint8_t(word >> 8);
        sample.data[6] = 1; // 6061 mode.
        ++now;
        CHECK(owner.receive(sample, now, now));
        const C::PdoRecord& record = *owner.pdo(0);
        L::ReadContext c;
        CHECK(L::prepareState(c, target(), 1, now - 1, 1000, options(), mask({L::StateBlock::STATUS}), &map.route));
        CHECK(L::advanceRead(c, record, now));
        CHECK(c.state == M::ReadState::SUCCEEDED && c.source == L::ReadSource::TPDO);
        L::StateObservation block;
        CHECK(L::getStateBlock(c, L::StateBlock::STATUS, target(), now, 100, block, &record));
        CHECK(block.statusword == word && block.modeRaw == 1);
        // The typed block, the raw frame bytes and an SDO read of the same word all decode alike.
        const uint16_t raw = uint16_t(record.observation.sample.frame.data[0] |
                                      (uint16_t(record.observation.sample.frame.data[1]) << 8));
        CHECK(raw == word);
        uint64_t sdoTime = now;
        L::ReadContext service;
        CHECK(L::prepareState(service, target(), 2, sdoTime, 1000000, options(), mask({L::StateBlock::STATUS})));
        finishRead(service, sdoTime, {{0x6041, word}, {0x6061, 1}});
        L::StateObservation serviceBlock;
        CHECK(L::getStateBlock(service, L::StateBlock::STATUS, target(), sdoTime, 1000000, serviceBlock));
        for (const Layout& document : documents) {
            L::StatusIndications fromPdo, fromFrame, fromSdo;
            CHECK(L::interpretStatusword(block.statusword, document.layout, fromPdo));
            CHECK(L::interpretStatusword(raw, document.layout, fromFrame));
            CHECK(L::interpretStatusword(serviceBlock.statusword, document.layout, fromSdo));
            CHECK(fromPdo.alarm == fromFrame.alarm && fromPdo.alarm == fromSdo.alarm);
            CHECK(fromPdo.running == fromFrame.running && fromPdo.running == fromSdo.running);
            CHECK(fromPdo.unlistedStatusBits == fromFrame.unlistedStatusBits &&
                  fromPdo.unlistedStatusBits == fromSdo.unlistedStatusBits);
            CHECK(fromPdo.lowPatternListed == fromSdo.lowPatternListed);
        }
        // The SDO-only combined getter cannot read a TPDO context, and sources never mix.
        L::StateInterpretation combined;
        const Saved<L::StateInterpretation> blank(combined);
        CHECK(refused(L::getStateInterpretation(c, target(), now, 100, L::StatusLayout::EN86, combined)));
        CHECK(blank.same(combined));
        L::StateObservation out;
        const Saved<L::StateObservation> blankOut(out);
        CHECK(refusedAs(L::getStateBlock(c, L::StateBlock::STATUS, target(), now, 100, out), M::ReadError::TARGET));
        CHECK(refusedAs(L::getStateBlock(service, L::StateBlock::STATUS, target(), sdoTime, 100, out, &record),
                        M::ReadError::TARGET));
        CHECK(blankOut.same(out));
        // A decodable word on a record without its proof is refused. The caller's route check stays.
        C::PdoRecord withdrawn = record;
        withdrawn.qualified = false;
        CHECK(refusedAs(L::getStateBlock(c, L::StateBlock::STATUS, target(), now, 100, out, &withdrawn), M::ReadError::TARGET));
        withdrawn = record;
        withdrawn.map.nodeGeneration = 12;
        CHECK(refusedAs(L::getStateBlock(c, L::StateBlock::STATUS, target(), now, 100, out, &withdrawn), M::ReadError::TARGET));
        CHECK(blankOut.same(out));
        // A decode says nothing a stop or settings call can use: both still need an SDO block.
        L::ActionContext action;
        const Saved<L::ActionContext> blankAction(action);
        const C::SdoCompletion control = controlWord(0x000f, ACQUIRED, ACQUIRED);
        L::AxisReservation live = reservation();
        live.acquiredUs = 0;
        CHECK(refused(L::prepareStop(action, live, c, control, L::StopKind::HALT, 41, now, 1000, actionOptions())));
        CHECK(blankAction.same(action));
    }
}
// pdoTemplate's refusal carries static text for every out-of-range argument, and output stays put.
void pdoTemplateRefusalsHaveText() {
    C::PdoMap output;
    output.number = 0x7777;
    output.words[0] = 0x12345678;
    const Saved<C::PdoMap> blank(output);
    for (uint32_t number = 0; number <= 0xffff; ++number) {
        if (number >= 1 && number <= 4)
            continue;
        for (C::PdoDirection direction : {C::PdoDirection::RPDO, C::PdoDirection::TPDO}) {
            const M::Status status = L::pdoTemplate(direction, uint16_t(number), output);
            CHECK(refused(status) && status.code == M::Err::INVALID_CONFIG && !status.detail);
            CHECK(std::strlen(status.msg) > 0);
            CHECK(blank.same(output));
        }
    }
    for (unsigned direction = 2; direction < 256; ++direction)
        for (uint16_t number : {uint16_t(0), uint16_t(1), uint16_t(4), uint16_t(5)}) {
            const M::Status status = L::pdoTemplate(static_cast<C::PdoDirection>(direction), number, output);
            CHECK(refused(status) && std::strlen(status.msg) > 0);
            CHECK(blank.same(output));
        }
    for (C::PdoDirection direction : {C::PdoDirection::RPDO, C::PdoDirection::TPDO})
        for (uint16_t number = 1; number <= 4; ++number) {
            C::PdoMap good;
            CHECK(L::pdoTemplate(direction, number, good) && good.number == number);
        }
}

// The same evidence on both predicates. Settings wants nonrunning, 000F and a bounded age; a stop
// must work on a running drive, accepts DEC-8 predecessors and lets an acknowledged word age.
const uint16_t HALT_PREDECESSORS[] = {0x000f, 0x001f, 0x005f, 0x003f, 0x007f};
bool haltAllowed(uint16_t word) {
    for (uint16_t allowed : HALT_PREDECESSORS)
        if (word == allowed)
            return true;
    return false;
}
struct Predicate {
    L::AxisReservation live = reservation();
    C::SdoCompletion control;
    L::ReadContext state;
    L::DriverContext prior;
    uint64_t now;
    // Control acknowledged at `completedAt`; reads follow at `readsAt`.
    Predicate(uint16_t word, uint16_t monitor, uint16_t statusword, uint8_t mode, uint64_t completedAt,
              uint64_t readsAt) {
        control = controlWord(word, completedAt, completedAt);
        now = readsAt;
        CHECK(L::prepareState(state, target(), 1, now, UINT64_MAX / 2, options(), PREREQUISITE));
        finishRead(state, now, {{0x2100, monitor}, {0x6041, statusword}, {0x6061, mode}});
        CHECK(L::prepareDriverRead(prior, target(), 2, now, UINT64_MAX / 2, options(), L::DriverGroup::MOTION_PARAMETERS));
        finishRead(prior.read, now, {});
    }
    M::Status settings(L::DriverSettingsContext& out, uint64_t age, uint64_t at) const {
        L::DriverSettings candidate;
        candidate.positionSpeedRpm = candidate.rampUpTimeMs = candidate.rampDownTimeMs = 2;
        return L::prepareDriverSettings(out, live, live.target, prior, state, control, candidate, 41, at, at + 100000, options(), age);
    }
    M::Status stop(L::ActionContext& out, L::StopKind kind, uint64_t age, uint64_t at) const {
        return L::prepareStop(out, live, state, control, kind, 41, at, at + 100000, actionOptions(options(), age));
    }
};
void predicatesStaySeparate() {
    const uint16_t words[] = {0x000f, 0x001f, 0x005f, 0x003f, 0x007f, 0x010f, 0x000b, 0x0006, 0x0007,
                              0x000d, 0x0000, 0x0080, 0x010b, 0x011f, 0xffff};
    const uint16_t monitors[] = {0, L::MONITOR_RUNNING, L::MONITOR_ALARM, L::MONITOR_RELEASED,
                                 L::MONITOR_RUNNING | L::MONITOR_ALARM, L::MONITOR_RUNNING | L::MONITOR_RELEASED,
                                 L::MONITOR_POSITION_COMPLETE, L::MONITOR_HOMING_COMPLETE,
                                 L::MONITOR_RUNNING | L::MONITOR_POSITIVE_SOFT_LIMIT, 0x0080, 0xff00};
    const uint16_t statuses[] = {0x0007, 0x0003, 0x0006, 0x0107, 0x8007, 0x0087, 0xffff};
    const uint8_t modes[] = {1, 3, 6, 0xff};
    for (uint16_t word : words)
        for (uint16_t monitor : monitors)
            for (uint16_t statusword : statuses)
                for (uint8_t mode : modes) {
                    Predicate p(word, monitor, statusword, mode, ACQUIRED + 1, ACQUIRED + 2);
                    const bool operating = (statusword & 7) == 7 && mode == 1;
                    const bool badForAny = (monitor & (L::MONITOR_ALARM | L::MONITOR_RELEASED)) != 0;
                    const bool running = (monitor & L::MONITOR_RUNNING) != 0;
                    L::DriverSettingsContext settings;
                    const Saved<L::DriverSettingsContext> blankSettings(settings);
                    const bool settingsOk = word == 0x000f && operating && !badForAny && !running;
                    const M::Status s = p.settings(settings, 100000, p.now);
                    CHECK(s.isOk() == settingsOk);
                    if (!settingsOk)
                        CHECK(refusedAs(s, M::ReadError::STATE) && blankSettings.same(settings));
                    for (L::StopKind kind : {L::StopKind::HALT, L::StopKind::QUICK_STOP}) {
                        L::ActionContext stop;
                        const Saved<L::ActionContext> blankStop(stop);
                        const bool allowed = haltAllowed(word) || (kind == L::StopKind::QUICK_STOP && word == 0x010f);
                        const bool velocityOperating = mode == 3 && word == 0x000f &&
                                                       (statusword & 7) == 7 && running;
                        // Mode 6 has only the bit-4 falling edge from the real held start word.
                        const bool homingOperating = mode == 6 && word == 0x001f &&
                                                     kind == L::StopKind::HALT && (statusword & 7) == 7;
                        const bool stopOk = allowed && (operating || velocityOperating || homingOperating) && !badForAny;
                        CHECK(L::stopPredecessorAllowed(word, kind) == allowed);
                        const M::Status validated = L::validateStop(stop, p.live, p.state, p.control, kind, 41, p.now,
                                                                    p.now + 100000, actionOptions());
                        CHECK(validated.isOk() == stopOk);
                        const M::Status t = p.stop(stop, kind, 200000, p.now);
                        CHECK(t.isOk() == stopOk);
                        if (!stopOk)
                            CHECK(refused(t) && blankStop.same(stop));
                        else {
                            CHECK(stop.predecessorWord == word);
                            CHECK(stop.predecessorDocumented == (word == 0x000f && mode == 1));
                            CHECK(stop.predecessorBasis ==
                                  (word != 0x000f || mode == 3      ? L::PredecessorBasis::PROJECT_DECISION
                                   : kind == L::StopKind::QUICK_STOP ? L::PredecessorBasis::EN86_ZH86_OP86
                                                                     : L::PredecessorBasis::ZH86_OP86));
                        }
                    }
                }
    // The whole word space of the DEC-8 table, against the literal list.
    for (uint32_t word = 0; word <= 0xffff; ++word) {
        CHECK(L::stopPredecessorAllowed(uint16_t(word), L::StopKind::HALT) == haltAllowed(uint16_t(word)));
        CHECK(L::stopPredecessorAllowed(uint16_t(word), L::StopKind::QUICK_STOP) ==
              (haltAllowed(uint16_t(word)) || word == 0x010f));
    }
    // A prior 000B, or a prior 010F for HALT, is rejected rather than reinterpreted.
    {
        Predicate p(0x010f, L::MONITOR_RUNNING, 7, 1, ACQUIRED + 1, ACQUIRED + 2);
        L::ActionContext stop;
        CHECK(refused(p.stop(stop, L::StopKind::HALT, 200000, p.now)));
        CHECK(p.stop(stop, L::StopKind::QUICK_STOP, 200000, p.now));
        CHECK(!stop.predecessorDocumented && stop.predecessorBasis == L::PredecessorBasis::PROJECT_DECISION);
    }
    // DEC-8 words stay refused for settings, with otherwise perfect evidence and a nonrunning drive.
    for (uint16_t word : {uint16_t(0x001f), uint16_t(0x005f), uint16_t(0x003f), uint16_t(0x007f), uint16_t(0x010f)}) {
        Predicate p(word, 0, 7, 1, ACQUIRED + 1, ACQUIRED + 2);
        L::DriverSettingsContext settings;
        const Saved<L::DriverSettingsContext> blank(settings);
        CHECK(refusedAs(p.settings(settings, 100000, p.now), M::ReadError::STATE) && blank.same(settings));
        L::ActionContext stop;
        CHECK(p.stop(stop, L::StopKind::QUICK_STOP, 200000, p.now));
    }
    // Age: the settings bound counts from admission and includes equality, and is not on the stop.
    {
        const uint64_t age = 2000; // Longer than the budgets' cycle, for the stop's own check.
        const uint64_t acknowledgedAt = ACQUIRED + 1;
        const uint64_t readsAt = acknowledgedAt + 100;
        Predicate p(0x000f, 0, 7, 1, acknowledgedAt, readsAt);
        // The reads stay fresh under this age until the control word has already aged out.
        const uint64_t edge = acknowledgedAt + age;
        CHECK(edge > p.now && edge - readsAt <= age);
        L::DriverSettingsContext settings;
        CHECK(p.settings(settings, age, edge));
        settings = L::DriverSettingsContext();
        const Saved<L::DriverSettingsContext> blank(settings);
        CHECK(refusedAs(p.settings(settings, age, edge + 1), M::ReadError::STATE) && blank.same(settings));
        L::ActionContext stop;
        CHECK(p.stop(stop, L::StopKind::HALT, age, edge + 1)); // Same evidence, same time, same age.
        CHECK(stop.controlEvidence.completedUs == acknowledgedAt);
    }
    // A stop accepts an arbitrarily old word when its read blocks are fresh; the blocks still expire.
    {
        const uint64_t age = 2000;
        Predicate p(0x000f, L::MONITOR_RUNNING, 7, 1, ACQUIRED, 1000000);
        L::ActionContext stop;
        const Saved<L::ActionContext> blank(stop);
        CHECK(L::validateStop(stop, p.live, p.state, p.control, L::StopKind::HALT, 41, p.now, p.now + 1000,
                              actionOptions(options(), age)));
        // Earliest STATUS or MONITORS admission is one step after the reads began.
        const uint64_t firstRead = 1000001;
        CHECK(L::validateStop(stop, p.live, p.state, p.control, L::StopKind::HALT, 41, firstRead + age, firstRead + age + 1000,
                              actionOptions(options(), age)));
        CHECK(refused(L::validateStop(stop, p.live, p.state, p.control, L::StopKind::HALT, 41, firstRead + age + 1,
                                      firstRead + age + 1001, actionOptions(options(), age))));
        CHECK(blank.same(stop));
        // The same old word cannot admit settings, though it is not running here.
        Predicate q(0x000f, 0, 7, 1, ACQUIRED, 1000000);
        L::DriverSettingsContext settings;
        const Saved<L::DriverSettingsContext> blankSettings(settings);
        CHECK(refused(q.settings(settings, age, q.now)) && blankSettings.same(settings));
    }
    // A read that began before the word completed cannot support either.
    {
        Predicate p(0x000f, 0, 7, 1, ACQUIRED, ACQUIRED + 10);
        p.control = controlWord(0x000f, ACQUIRED + 5, p.now + 1); // Completed after the reads.
        const uint64_t at = p.now + 2;
        L::DriverSettingsContext settings;
        CHECK(refusedAs(p.settings(settings, 100000, at), M::ReadError::STATE));
        L::ActionContext stop;
        CHECK(refused(p.stop(stop, L::StopKind::HALT, 200000, at)));
    }
    // Order to the microsecond, for each block alone. A block admitted exactly at the word's
    // completion qualifies and one earlier does not, on both predicates.
    const auto bothAgree = [](const L::ReadContext& state, const C::SdoCompletion& control,
                              const L::DriverContext& prior, uint64_t at, bool expected) {
        L::DriverSettings candidate;
        candidate.positionSpeedRpm = candidate.rampUpTimeMs = candidate.rampDownTimeMs = 2;
        static L::DriverSettingsContext settings;
        settings = L::DriverSettingsContext();
        const Saved<L::DriverSettingsContext> blank(settings);
        const M::Status s = L::prepareDriverSettings(settings, reservation(), reservation().target, prior, state, control, candidate,
                                                     41, at, at + 100000, options(), 1000000);
        CHECK(s.isOk() == expected && (expected || blank.same(settings)));
        static L::ActionContext stop;
        L::resetAction(stop);
        CHECK(L::validateStop(stop, reservation(), state, control, L::StopKind::HALT, 41, at, at + 100000,
                              actionOptions()).isOk() == expected);
    };
    const uint64_t readStart = 3000;
    const auto freshPrior = [](uint64_t& now) {
        L::DriverContext prior;
        CHECK(L::prepareDriverRead(prior, target(), 2, now, 1000000, options(), L::DriverGroup::MOTION_PARAMETERS));
        finishRead(prior.read, now, {});
        return prior;
    };
    for (bool late : {false, true}) {
        // MONITORS is read first, admitted at readStart + 1. The word completes then or just after.
        L::ReadContext state;
        uint64_t now = readStart;
        CHECK(L::prepareState(state, target(), 1, now, 1000000, options(), PREREQUISITE));
        finishRead(state, now, {{0x2100, 0}, {0x6041, 7}, {0x6061, 1}});
        const L::DriverContext prior = freshPrior(now);
        bothAgree(state, controlWord(0x000f, ACQUIRED, readStart + (late ? 2 : 1)), prior, now, !late);
    }
    for (bool late : {false, true}) {
        // STATUS alone: its earliest admission is readStart + 4, with MONITORS moved past the word,
        // since a real read always takes MONITORS first.
        L::ReadContext state;
        uint64_t now = readStart;
        CHECK(L::prepareState(state, target(), 1, now, 1000000, options(), PREREQUISITE));
        finishRead(state, now, {{0x2100, 0}, {0x6041, 7}, {0x6061, 1}});
        for (unsigned slot = 0; slot < 3; ++slot) {
            L::ReadProvenance& moved = state.values[slot].provenance;
            moved.earliestUs = moved.latestUs = moved.deliveredUs = readStart + 5;
        }
        const L::DriverContext prior = freshPrior(now);
        bothAgree(state, controlWord(0x000f, ACQUIRED, readStart + (late ? 5 : 4)), prior, now, !late);
    }
    for (bool late : {false, true}) {
        // The prior group alone, which only settings consumes. It may begin exactly at completion.
        uint64_t now = readStart;
        const L::DriverContext prior = freshPrior(now);
        const C::SdoCompletion control = controlWord(0x000f, ACQUIRED, readStart + (late ? 1 : 0));
        uint64_t later = readStart + 100;
        L::ReadContext state;
        CHECK(L::prepareState(state, target(), 1, later, 1000000, options(), PREREQUISITE));
        finishRead(state, later, {{0x2100, 0}, {0x6041, 7}, {0x6061, 1}});
        L::DriverSettings candidate;
        candidate.positionSpeedRpm = candidate.rampUpTimeMs = candidate.rampDownTimeMs = 2;
        L::DriverSettingsContext settings;
        CHECK(L::prepareDriverSettings(settings, reservation(), reservation().target, prior, state, control, candidate, 41, later,
                                       later + 100000, options(), 1000000).isOk() == !late);
        L::ActionContext stop;
        CHECK(L::validateStop(stop, reservation(), state, control, L::StopKind::HALT, 41, later, later + 100000,
                              actionOptions()));
    }
}

// Shared overlap behavior through real entries: zero-size ranges share nothing, and any byte shared
// with a real input is refused before the zero-size or other-argument checks can write output.
void overlapAtRealCallSites() {
    alignas(16) unsigned char storage[256] = {};
    for (unsigned i = 0; i < sizeof storage; ++i)
        storage[i] = uint8_t(i);
    uint32_t& inside = *reinterpret_cast<uint32_t*>(storage + 8);
    const Saved<decltype(storage)> untouched(storage);
    // Output overlapping any part of the span, including bytes past the selected width.
    CHECK(refused(C::readWord(storage + 8, 4, 2, inside)));
    CHECK(refused(C::readWord(storage + 4, 8, 2, inside)));
    CHECK(refused(C::readWord(storage + 4, 5, 1, inside))); // One shared byte.
    CHECK(untouched.same(storage));
    uint32_t out = 0;
    int32_t signedOut = 0;
    // Exactly adjacent on either side is legal, and a zero span is capacity rather than overlap.
    CHECK(refused(C::readWord(storage + 8, 0, 1, out)) && !out);
    CHECK(refused(C::readSignedWord(storage + 8, 0, 1, signedOut)) && !signedOut);
    CHECK(refused(C::readWord(nullptr, 4, 1, out)));
    uint32_t adjacent[2] = {0x11223344u, 0};
    CHECK(C::readWord(reinterpret_cast<const uint8_t*>(&adjacent[0]), sizeof adjacent[0], 4, adjacent[1]));
    CHECK(adjacent[1] == 0x11223344u);
    CHECK(C::readWord(reinterpret_cast<const uint8_t*>(&adjacent[1]), sizeof adjacent[1], 4, adjacent[0]));

    // Getter output overlapping its context by one alignment unit, then exactly past its end.
    struct Space {
        alignas(16) unsigned char bytes[sizeof(L::ReadContext) + sizeof(L::StateObservation) + 32];
    };
    static Space space;
    L::ReadContext& read = *new (static_cast<void*>(space.bytes)) L::ReadContext();
    uint64_t now = ACQUIRED;
    CHECK(L::prepareState(read, target(), 1, now, 1000000, options(), PREREQUISITE));
    finishRead(read, now, {{0x6041, 7}, {0x6061, 1}});
    const size_t unit = alignof(L::StateObservation);
    const size_t past = (sizeof(L::ReadContext) + unit - 1) / unit * unit;
    L::StateObservation& sharing = *reinterpret_cast<L::StateObservation*>(space.bytes + past - unit);
    const Saved<L::ReadContext> kept(read);
    CHECK(refusedAs(L::getStateBlock(read, L::StateBlock::STATUS, target(), now, 1000000, sharing), M::ReadError::OVERLAP));
    CHECK(kept.same(read));
    L::StateObservation& clear = *new (static_cast<void*>(space.bytes + past)) L::StateObservation();
    CHECK(L::getStateBlock(read, L::StateBlock::STATUS, target(), now, 1000000, clear) && clear.statusword == 7);

    // Self-aliased work and terminals, at the fields a caller could plausibly pass.
    {
        L::ReadContext c;
        CHECK(L::prepareState(c, target(), 1, ACQUIRED, 1000000, options(), PREREQUISITE));
        const Saved<L::ReadContext> before(c);
        CHECK(refusedAs(L::advanceRead(c, c.evidence, ACQUIRED + 1), M::ReadError::OVERLAP));
        CHECK(refusedAs(L::prepareState(c, c.target, 2, ACQUIRED, 1000000, options(), PREREQUISITE), M::ReadError::OVERLAP));
        CHECK(refusedAs(L::prepareState(c, target(), 2, ACQUIRED, 1000000, c.options, PREREQUISITE), M::ReadError::OVERLAP));
        CHECK(before.same(c));
    }
    {
        const L::AxisReservation live = reservation();
        ActionCommandRig rig(0, options());
        const Saved<L::ActionContext> before(rig.c);
        CHECK(refused(L::advanceAction(rig.c, live, target(), rig.c.observationEvidence, STEP_AT)));
        CHECK(refused(L::advanceAction(rig.c, rig.c.reservation, target(), rig.c.controlEvidence, STEP_AT)));
        CHECK(refused(L::advanceAction(rig.c, live, rig.c.read.target, rig.c.controlEvidence, STEP_AT)));
        CHECK(before.same(rig.c));
        L::BestEffortStop stop;
        CHECK(L::prepareBestEffortStop(stop, target(), 1, STEP_AT, CUTOFF, options()));
        CHECK(L::noteBestEffortAdmission(stop, STEP_AT));
        const Saved<L::BestEffortStop> pending(stop);
        CHECK(refused(L::advanceBestEffortStop(stop, stop.commandEvidence, STEP_AT)));
        CHECK(refused(L::prepareBestEffortStop(stop, stop.target, 2, STEP_AT, CUTOFF, options())));
        CHECK(refused(L::prepareBestEffortStop(stop, target(), 2, STEP_AT, CUTOFF, stop.service)));
        CHECK(pending.same(stop));
    }
    // An empty range inside the output is not an alias, so the entry's own count check answers.
    {
        L::Tpdo1ConfigurationContext output;
        L::Tpdo1ConfigurationReview review;
        C::ObservationTime preOp;
        const C::SdoCompletion* insideOutput = reinterpret_cast<const C::SdoCompletion*>(&output);
        const Saved<L::Tpdo1ConfigurationContext> blank(output);
        const M::Status empty = L::prepareTpdo1Configuration(output, reservation(), review, insideOutput, 0,
                                                             C::NmtState::PRE_OPERATIONAL, preOp, 1, 0, 100, options(), 10);
        CHECK(refusedAs(empty, M::ReadError::ARGUMENT));
        const M::Status aliased = L::prepareTpdo1Configuration(output, reservation(), review, insideOutput, 9,
                                                               C::NmtState::PRE_OPERATIONAL, preOp, 1, 0, 100, options(), 10);
        CHECK(refusedAs(aliased, M::ReadError::OVERLAP));
        CHECK(blank.same(output));
    }
    // Frame-sized and span inputs of the shared PDO and NMT checks.
    {
        C::PdoMapEvidence map;
        map.route.binding = target().channel.binding;
        map.route.generation = 2;
        map.route.cobId = 0x185;
        C::PdoMap template1;
        CHECK(L::pdoTemplate(C::PdoDirection::TPDO, 1, template1));
        CHECK(C::validatePdoMap(template1, L::pdoProfile(41), map.route.layout));
        static C::PdoMapEvidence output;
        const Saved<C::PdoMapEvidence> blank(output);
        const C::SdoCompletion* inside = reinterpret_cast<const C::SdoCompletion*>(&output);
        // Zero results is an invalid count, reported as such, even when the pointer is inside output.
        CHECK(refused(C::pdoMapFromReadback(inside, 0, map.route, 1, 1, output)));
        CHECK(refused(C::pdoMapFromReadback(nullptr, 6, map.route, 1, 1, output)));
        CHECK(refused(C::pdoMapFromReadback(inside, 6, map.route, 1, 1, output)));
        CHECK(blank.same(output));
        static C::PdoSample sample;
        const Saved<C::PdoSample> blankSample(sample);
        M::Frame frame;
        frame.id = 0x185;
        frame.dlc = frame.payloadLength = 7;
        const M::Frame& frameInside = reinterpret_cast<const M::Frame&>(sample);
        CHECK(refused(C::decodePdo(frameInside, map.route, sample)));
        CHECK(refused(C::decodePdo(frame, sample.route, sample)));
        CHECK(blankSample.same(sample));
        CHECK(C::decodePdo(frame, map.route, sample));
    }
}

// Terminal consistency through the callers' own view of the budget: a real client's terminal is
// accepted by the private check exactly when the public entry accepts it.
void privateChecksAgreeWithRealTerminals() {
    // Every real caller passes a nonzero size, so an empty range is pinned here, in both orders and
    // pointing inside the other range. The boundary is pinned on both sides as well.
    unsigned char bytes[32] = {};
    CHECK(!C::overlaps(bytes + 8, 0, bytes + 8, 0) && !C::overlaps(bytes + 8, 0, bytes, 32));
    CHECK(!C::overlaps(bytes, 32, bytes + 8, 0));
    CHECK(C::overlaps(bytes, 9, bytes + 8, 1) && C::overlaps(bytes + 8, 1, bytes, 9));
    CHECK(!C::overlaps(bytes, 8, bytes + 8, 1) && !C::overlaps(bytes + 8, 1, bytes, 8));
    for (Reach reach : reaches) {
        ReadRig read(0, budgetEndingAt(reach, -1));
        const C::SdoCompletion e = realTimeout(read.work, reach, CUTOFF - 1);
        CHECK(C::terminalConsistent(e) && C::checkUploadTerminal(e, read.work.size));
        CHECK(C::sameSdoWork(e.transfer, read.work));
        SettingsWriteRig write(1, budgetEndingAt(reach, -1));
        const C::SdoCompletion d = realTimeout(write.work, reach, CUTOFF - 1);
        CHECK(C::checkDownloadTerminal(d, mayHaveLeft(reach)));
        CHECK(C::checkDownloadTerminal(d, true));
        CHECK(C::checkDownloadTerminal(d, false) == !mayHaveLeft(reach));
    }
}
} // namespace

int main() {
    budgetTimelines();
    timeoutWithoutNoticeIsRefused();
    otherOutcomesAgainstCutoff();
    publishedSuccessIsStable();
    correlationLossWithdrawsEarlierBlocks();
    moveWritesRetireMotionParameters();
    settingsContextsAreRetiredToo();
    statuswordDecodeIsExhaustive();
    statusOnlyReadDecodes();
    contradictionReportingAcrossLayouts();
    pdoRawDecode();
    pdoTemplateRefusalsHaveText();
    predicatesStaySeparate();
    overlapAtRealCallSites();
    privateChecksAgreeWithRealTerminals();
    std::printf("evidence_timeline: %u checks passed\n", checks);
    return 0;
}
