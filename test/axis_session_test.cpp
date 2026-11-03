// SPDX-License-Identifier: MIT
// AxisSession and the core stop contexts through the real owner and selected SDO engine.
// Every controlword, status, monitor and error value here is manufactured by this fake. None
// is qualified drive behavior, and nothing here shows that any word stops or enables a motor.
// NMT stopped is never used as a drive state. Every scenario runs twice, the second time with
// heartbeat and EMCY traffic delivered on every turn.
#include "AxisSession.h"
#include "ProbeApp.h"
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
            std::fprintf(stderr, "%s:%d: %s [%s, async %d]\n", __FILE__, __LINE__, #condition,      \
                         scenario, int(asyncTraffic));                                             \
            std::abort();                                                                          \
        }                                                                                          \
    } while (false)
#include "support/OwnerRig.h"
namespace {
using OwnerRig::budget;
using OwnerRig::frame;
template <class T> struct Saved {
    unsigned char bytes[sizeof(T)];
    explicit Saved(const T& value) { std::memcpy(bytes, static_cast<const void*>(&value), sizeof value); }
    void unchanged(const T& value) const {
        CHECK(!std::memcmp(bytes, static_cast<const void*>(&value), sizeof value));
    }
};
const uint16_t WORD_ENABLE = 0x000f, WORD_TRIGGER = 0x001f, WORD_HALT = 0x010f, WORD_QUICK = 0x000b;
uint16_t stopWord(L::StopKind kind) { return kind == L::StopKind::HALT ? WORD_HALT : WORD_QUICK; }
uint8_t bit(L::StateBlock block) { return uint8_t(1u << unsigned(block)); }
const uint8_t STATE_MASK = uint8_t((1u << unsigned(L::StateBlock::MONITORS)) |
                                   (1u << unsigned(L::StateBlock::STATUS)));
L::ReadTarget target(unsigned node = 1) {
    L::ReadTarget value;
    value.channel = OwnerRig::route(node);
    value.nodeGeneration = 11;
    return value;
}
L::ReadOptions reads(uint32_t queue = 100, uint32_t response = 100) {
    L::ReadOptions value;
    value.queueTimeoutUs = queue;
    value.responseTimeoutUs = response;
    return value;
}
E::StopPolicy stopPolicy(uint64_t now, uint64_t lead = 5000, uint64_t bestEffortLead = 6000,
                         uint32_t queue = 100, uint32_t response = 100, uint64_t interval = 10) {
    E::StopPolicy value;
    value.options.service = reads(queue, response);
    value.options.observationIntervalUs = interval;
    value.options.maximumAgeUs = uint64_t(queue + uint64_t(response)) * 5;
    value.deadlineUs = now + lead;
    value.bestEffortDeadlineUs = bestEffortLead ? now + bestEffortLead : 0;
    return value;
}
// A fake server. Its replies are this fixture's, and only its wire shape is meaningful.
struct FakeDrive {
    bool answer = true;
    uint16_t monitor = 4, status = 7, errorCode = 0xff05, abortIndex = 0;
    uint8_t mode = 1;
};
uint16_t frameIndex(const M::Frame& f) { return uint16_t(f.data[1] | (uint16_t(f.data[2]) << 8)); }
M::Frame replyTo(const M::Frame& sent, const FakeDrive& drive) {
    M::Frame out = frame(sent.id - 0x600 + 0x580, 8);
    const uint16_t index = frameIndex(sent);
    out.data[1] = sent.data[1];
    out.data[2] = sent.data[2];
    out.data[3] = sent.data[3];
    if (index == drive.abortIndex && index) {
        out.data[0] = 0x80;
        out.data[4] = 0x23;
        out.data[7] = 0x08;
        return out;
    }
    if (sent.data[0] != 0x40) {
        out.data[0] = 0x60;
        return out;
    }
    uint32_t raw = 0;
    unsigned size = 2;
    switch (index) {
    case 0x1001:
        raw = 1;
        size = 1;
        break;
    case 0x1003:
        raw = sent.data[3] ? 0x12340000u + sent.data[3] : 2;
        size = sent.data[3] ? 4 : 1;
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
    case 0x603f:
        raw = drive.errorCode;
        break;
    case 0x6041:
        raw = drive.status;
        break;
    case 0x6061:
        raw = drive.mode;
        size = 1;
        break;
    case 0x6064:
    case 0x606c:
        size = 4;
        break;
    default:
        break;
    }
    out.data[0] = size == 1 ? 0x4f : size == 2 ? 0x4b : 0x43;
    for (unsigned i = 0; i < 4; ++i)
        out.data[4 + i] = uint8_t(raw >> (8 * i));
    return out;
}
struct Wire : OwnerRig::WireBase {
    O::RecoveryProgress progress = O::RecoveryProgress::RETIRED;
    unsigned recoveries = 0;
    static bool beginRecovery(void* context, uint32_t) {
        ++static_cast<Wire*>(static_cast<OwnerRig::WireBase*>(context))->recoveries;
        return true;
    }
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
    E::AxisSession a, b;
    E::ProbeApp* app = nullptr;
    FakeDrive drive[2];
    uint64_t now = 0, opDeadline = 0, window = 4000;
    unsigned served = 0;
    bool serviceA = true;
    Rig() : owner(services, wire.callbacks(), 73), a(owner), b(owner) {
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
    }
    L::ReadTarget current(unsigned node = 1) const {
        L::ReadTarget value = target(node);
        value.nodeGeneration = services.node(uint8_t(node - 1))->route.generation;
        return value;
    }
    void push(const M::Frame& value) { wire.push(value, now, owner.controllerEpoch()); }
    // Captured after anything already sent, for the next turn.
    void later(const M::Frame& value) { wire.push(value, now + 1, owner.controllerEpoch()); }
    void answer(unsigned i) {
        later(replyTo(wire.sent[i], drive[wire.sent[i].id - 0x601]));
        if (served <= i)
            served = i + 1; // The drive answers a frame once.
    }
    // Answers what the last turn sent, except from a silent drive. Those stay unanswered.
    void turn(uint64_t time, const O::ServiceBudget& work = budget(8, 2, 8, 2, 1)) {
        now = time;
        if (wire.rxRead == wire.rxWrite)
            wire.rxRead = wire.rxWrite = 0;
        for (; served < wire.sentCount; ++served) {
            const M::Frame& f = wire.sent[served];
            if (f.id >= 0x601 && f.id <= 0x602 && drive[f.id - 0x601].answer)
                push(replyTo(f, drive[f.id - 0x601]));
        }
        uint32_t emcy[2] = {};
        const M::Generation epoch[2] = {services.node(0)->route.generation,
                                        services.node(1)->route.generation};
        if (asyncTraffic)
            for (unsigned node = 1; node <= 2; ++node) {
                M::Frame heartbeat = frame(0x700 + node, 1);
                heartbeat.data[0] = 5;
                push(heartbeat);
                M::Frame fault = frame(0x80 + node, 8);
                fault.data[0] = 0x23;
                fault.data[1] = 0x81;
                fault.data[2] = 0x11;
                fault.data[7] = uint8_t(node);
                push(fault);
                emcy[node - 1] = services.node(uint8_t(node - 1))->emcyCount;
            }
        if (app) {
            CHECK(app->service(now));
        } else {
            CHECK(owner.service(now, work));
            if (serviceA)
                a.service(now);
            b.service(now);
        }
        if (asyncTraffic && work.rx)
            for (unsigned node = 1; node <= 2; ++node) {
                const C::NodeSnapshot* state = services.node(uint8_t(node - 1));
                // A frame at the same instant as an invalidation boundary is not evidence.
                if (state->route.generation != epoch[node - 1])
                    continue;
                CHECK(state->emcyCount == emcy[node - 1] + 1 &&
                      state->emcy.errorCode == 0x8123 && state->emcy.manufacturer[4] == node);
            }
    }
    void step() { turn(now + 1); }
    void steps(unsigned count) {
        for (unsigned i = 0; i < count; ++i)
            step();
    }
};
bool isWord(const Rig& r, unsigned i, uint16_t word, unsigned node = 1) {
    const uint8_t expected[] = {0x2b, 0x40, 0x60, 0, uint8_t(word), uint8_t(word >> 8), 0, 0};
    const M::Frame& f = r.wire.sent[i];
    return i < r.wire.sentCount && f.id == 0x600 + node && f.dlc == 8 && f.payloadLength == 8 &&
           !f.extended && !f.rtr && !std::memcmp(f.data, expected, sizeof expected);
}
unsigned writes6040(const Rig& r, unsigned from, unsigned node = 1) {
    unsigned count = 0;
    for (unsigned i = from; i < r.wire.sentCount; ++i)
        count += r.wire.sent[i].id == 0x600 + node && r.wire.sent[i].data[0] == 0x2b &&
                 frameIndex(r.wire.sent[i]) == 0x6040;
    return count;
}
unsigned uploads(const Rig& r, unsigned from, uint16_t index) {
    unsigned count = 0;
    for (unsigned i = from; i < r.wire.sentCount; ++i)
        count += r.wire.sent[i].data[0] == 0x40 && frameIndex(r.wire.sent[i]) == index;
    return count;
}
void openAxis(Rig& r, E::AxisSession& s, unsigned node = 1, uint8_t producer = 1) {
    CHECK(s.open(r.current(node), L::SettingsModel::CL86_C, producer, r.now));
}
uint32_t begin(Rig& r, E::AxisSession& s) {
    r.opDeadline = r.now + r.window;
    const uint32_t operation = s.beginOperation(r.opDeadline, r.now);
    CHECK(operation);
    return operation;
}
C::SdoTransfer service(const Rig& r, unsigned node, uint32_t operation, uint32_t step, uint16_t index,
                       bool download = false, uint16_t value = 0, const L::ReadOptions& options = reads()) {
    C::SdoTransfer t;
    t.channel = target(node).channel;
    t.operation = t.request = operation;
    t.step = step;
    t.index = index;
    t.size = 2;
    t.direction = download ? C::SdoDirection::DOWNLOAD : C::SdoDirection::UPLOAD;
    if (download) {
        t.data[0] = uint8_t(value);
        t.data[1] = uint8_t(value >> 8);
    }
    t.deadlineUs = r.opDeadline;
    t.queueTimeoutUs = options.queueTimeoutUs;
    t.responseTimeoutUs = options.responseTimeoutUs;
    return t;
}
C::SdoCompletion settled(E::AxisSession& s) {
    C::SdoCompletion output;
    CHECK(s.settle(output));
    return output;
}
void write(Rig& r, E::AxisSession& s, uint16_t index, uint16_t value, unsigned node = 1) {
    const uint32_t operation = begin(r, s);
    CHECK(s.admitPrepared(service(r, node, operation, 1, index, true, value), r.now) ==
          O::BusAdmission::ACCEPTED);
    r.steps(2);
    CHECK(settled(s).outcome == C::SdoOutcome::SUCCESS);
}
void readState(Rig& r, E::AxisSession& s, L::ReadContext& state, unsigned node = 1,
               uint8_t mask = STATE_MASK) {
    const uint32_t operation = begin(r, s);
    CHECK(L::prepareState(state, r.current(node), operation, r.now, r.opDeadline, reads(), mask));
    while (state.state == M::ReadState::ACTIVE) {
        L::PreparedRead work;
        CHECK(L::nextRead(state, r.now, work) && work.ready);
        CHECK(s.admitPrepared(work.transfer, r.now) == O::BusAdmission::ACCEPTED);
        r.steps(2);
        const C::SdoCompletion terminal = settled(s);
        CHECK(L::advanceRead(state, terminal, r.now));
    }
    CHECK(state.state == M::ReadState::SUCCEEDED);
}
// The latest word is this fixture's manufactured predecessor, followed by a fresh state read.
void history(Rig& r, E::AxisSession& s, L::ReadContext& state, uint16_t word = WORD_ENABLE,
             unsigned node = 1) {
    write(r, s, 0x6040, WORD_ENABLE, node);
    if (word != WORD_ENABLE)
        write(r, s, 0x6040, word, node);
    CHECK(s.latestControl() && s.latestControl()->transfer.data[0] == uint8_t(word) &&
          s.latestControl()->transfer.data[1] == uint8_t(word >> 8));
    readState(r, s, state, node);
}
void finish(Rig& r, const E::AxisSession& s) {
    for (unsigned i = 0; i < 200 && s.stopActive(); ++i)
        r.step();
    CHECK(!s.stopActive());
}
void quiet(Rig& r, unsigned from = UINT32_MAX) {
    const unsigned sent = from == UINT32_MAX ? r.wire.sentCount : from;
    r.steps(4);
    CHECK(r.wire.sentCount == sent);
}
L::AxisReservation reservationOf(const Rig& r, const E::AxisSession& s, unsigned node = 1) {
    L::AxisReservation value;
    const O::SequenceId id = s.sequence();
    const O::Sequence* live = r.owner.sequence(id);
    CHECK(live);
    value.target = r.current(node);
    value.model = L::SettingsModel::CL86_C;
    value.owner = r.owner.identity();
    value.sequence = uint32_t(id.slot) + 1;
    value.generation = id.generation;
    value.acquiredUs = live->acquiredUs;
    return value;
}
void checkEnded(const L::BestEffortStop& stop, M::ActionOutcome outcome, M::ActionExecution execution) {
    CHECK(stop.state == M::ActionState::FAILED && stop.outcome == outcome && stop.execution == execution);
    CHECK(stop.effectsPossible == (execution == M::ActionExecution::UNKNOWN));
}

// 1. Enable fixture, optional trigger, a poll in flight, then a stop.
void triggerPollThenStop() {
    const uint16_t words[] = {WORD_ENABLE, WORD_TRIGGER, 0x005f, 0x003f, 0x007f};
    for (uint16_t word : words)
        for (unsigned kind = 0; kind < 2; ++kind) {
            const L::StopKind selected = kind ? L::StopKind::QUICK_STOP : L::StopKind::HALT;
            Rig r;
            openAxis(r, r.a);
            L::ReadContext state;
            history(r, r.a, state, word);
            const uint32_t pollOperation = begin(r, r.a);
            r.drive[0].answer = false;
            CHECK(r.a.admitPrepared(service(r, 1, pollOperation, 1, 0x2100), r.now) ==
                  O::BusAdmission::ACCEPTED);
            r.step();
            const unsigned pollFrame = r.wire.sentCount - 1;
            CHECK(r.wire.sent[pollFrame].data[0] == 0x40 && frameIndex(r.wire.sent[pollFrame]) == 0x2100);
            CHECK(r.a.requestStop(selected, state, stopPolicy(r.now), r.now) == E::StopPath::CONFIRMED);
            CHECK(!r.a.latestControl());
            r.steps(3);
            CHECK(r.wire.sentCount == pollFrame + 1); // Nothing leaves past a poll in flight.
            CHECK(r.a.stopAction(selected).state == M::ActionState::ACTIVE);
            // The poll settles, and only then does the stop leave, exactly once.
            r.drive[0].monitor = 0x0004;
            r.answer(pollFrame);
            r.step();
            CHECK(r.wire.sentCount == pollFrame + 2 && isWord(r, pollFrame + 1, stopWord(selected)));
            const C::SdoCompletion poll = settled(r.a);
            CHECK(poll.outcome == C::SdoOutcome::SUCCESS && poll.transfer.index == 0x2100 &&
                  poll.transfer.operation == pollOperation && poll.size == 2 && poll.data[0] == 4);
            CHECK(!r.a.latestControl()); // An upload settling here restores nothing.
            r.drive[0].answer = true;
            r.drive[0].monitor = 0;
            r.answer(pollFrame + 1);
            finish(r, r.a);
            const L::ActionContext& stop = r.a.stopAction(selected);
            CHECK(stop.state == M::ActionState::SUCCEEDED && stop.outcome == M::ActionOutcome::OBSERVED &&
                  stop.execution == M::ActionExecution::ACKNOWLEDGED);
            CHECK(stop.predecessorWord == word && stop.predecessorDocumented == (word == WORD_ENABLE));
            CHECK(stop.predecessorBasis == (word != WORD_ENABLE ? L::PredecessorBasis::PROJECT_DECISION
                                            : kind              ? L::PredecessorBasis::EN86_ZH86_OP86
                                                                : L::PredecessorBasis::ZH86_OP86));
            CHECK(stop.operation != pollOperation && stop.commandHistory[0].transfer.operation == stop.operation);
            CHECK(r.a.stopPath() == E::StopPath::FINISHED);
            CHECK(writes6040(r, pollFrame) == 1);
            CHECK(r.a.latestControl() && r.a.latestControl()->transfer.data[1] == (kind ? 0 : 1));
        }
}

// 2. Lost trigger reply, best-effort stop, identical late reply.
void lostTriggerThenBestEffort() {
    for (unsigned lateFirst = 0; lateFirst < 2; ++lateFirst)
        for (unsigned settleFirst = 0; settleFirst < 2; ++settleFirst) {
            Rig r;
            openAxis(r, r.a);
            L::ReadContext state;
            history(r, r.a, state);
            const uint32_t triggerOperation = begin(r, r.a);
            r.drive[0].answer = false;
            CHECK(r.a.admitPrepared(service(r, 1, triggerOperation, 1, 0x6040, true, WORD_TRIGGER), r.now) ==
                  O::BusAdmission::ACCEPTED);
            CHECK(!r.a.latestControl()); // Withdrawn at admission, before any report.
            r.step();
            const unsigned triggerFrame = r.wire.sentCount - 1;
            CHECK(isWord(r, triggerFrame, WORD_TRIGGER));
            r.turn(r.now + 100);
            CHECK(r.services.unresolved(0));
            C::SdoCompletion trigger;
            if (settleFirst) {
                trigger = settled(r.a);
                CHECK(trigger.outcome == C::SdoOutcome::TIMEOUT && trigger.effectUncertain);
            }
            r.wire.autoReport = !lateFirst;
            CHECK(r.a.requestStop(L::StopKind::HALT, state, stopPolicy(r.now), r.now) ==
                  E::StopPath::BEST_EFFORT);
            r.step();
            CHECK(r.wire.sentCount == triggerFrame + 2 && isWord(r, triggerFrame + 1, WORD_QUICK));
            M::Frame late = frame(0x581, 8);
            const uint8_t identical[] = {0x60, 0x40, 0x60, 0, 0, 0, 0, 0};
            std::memcpy(late.data, identical, sizeof identical);
            if (lateFirst) {
                r.later(late);
                r.step();
                CHECK(r.a.stopPath() == E::StopPath::BEST_EFFORT); // No reply settles or withdraws it.
                CHECK(r.a.bestEffort().state == M::ActionState::ACTIVE);
                r.wire.report(triggerFrame + 1);
                r.step();
            } else {
                r.step();
                CHECK(r.a.stopPath() == E::StopPath::FINISHED);
                r.later(late);
                r.step();
            }
            const L::BestEffortStop& stop = r.a.bestEffort();
            checkEnded(stop, M::ActionOutcome::STOP_SENT_UNCONFIRMED, M::ActionExecution::UNKNOWN);
            CHECK(stop.commandEvidencePresent && stop.commandEvidence.transmissionConfirmed &&
                  !stop.commandEvidence.protocolAcknowledged &&
                  stop.commandEvidence.outcome == C::SdoOutcome::UNCONFIRMED_SENT);
            CHECK(r.a.stopPath() == E::StopPath::FINISHED);
            CHECK(r.services.unresolved(0) && r.services.requalifications(0) == 0);
            O::SequenceId none;
            CHECK(!r.owner.findSequence(target().channel.binding, none));
            CHECK(!r.a.latestControl());
            if (!settleFirst) {
                trigger = settled(r.a);
                CHECK(!r.a.latestControl());
            }
            // The original outcome keeps its own identity and stays uncertain.
            CHECK(trigger.outcome == C::SdoOutcome::TIMEOUT && trigger.transfer.operation == triggerOperation &&
                  trigger.transfer.data[0] == 0x1f && trigger.effectUncertain && !trigger.protocolAcknowledged);
            quiet(r);
        }
}

// 3. A poll reply is lost while the stop waits.
void lostPollWhileStopWaits() {
    for (unsigned kind = 0; kind < 2; ++kind)
        for (unsigned longQueue = 0; longQueue < 2; ++longQueue)
            for (unsigned allowed = 0; allowed < 2; ++allowed) {
                const L::StopKind selected = kind ? L::StopKind::QUICK_STOP : L::StopKind::HALT;
                Rig r;
                openAxis(r, r.a);
                L::ReadContext state;
                history(r, r.a, state);
                const uint32_t pollOperation = begin(r, r.a);
                r.drive[0].answer = false;
                CHECK(r.a.admitPrepared(service(r, 1, pollOperation, 1, 0x2100), r.now) ==
                      O::BusAdmission::ACCEPTED);
                r.step();
                const unsigned from = r.wire.sentCount;
                const E::StopPolicy policy =
                    stopPolicy(r.now, 5000, allowed ? 6000 : 0, longQueue ? 500 : 100, 100);
                CHECK(r.a.requestStop(selected, state, policy, r.now) == E::StopPath::CONFIRMED);
                r.turn(r.now + 99);
                CHECK(r.wire.sentCount == from && r.a.stopAction(selected).state == M::ActionState::ACTIVE);
                r.step();
                CHECK(r.services.unresolved(0));
                const L::ActionContext& first = r.a.stopAction(selected);
                CHECK(first.state == M::ActionState::FAILED &&
                      first.execution == M::ActionExecution::NOT_TRANSMITTED && !first.effectsPossible);
                const C::SdoCompletion poll = settled(r.a);
                CHECK(poll.outcome == C::SdoOutcome::TIMEOUT && poll.transfer.operation == pollOperation);
                if (!allowed) {
                    CHECK(r.a.stopPath() == E::StopPath::FINISHED);
                    quiet(r, from);
                    continue;
                }
                CHECK(r.a.stopPath() == E::StopPath::BEST_EFFORT);
                CHECK(r.a.bestEffort().deadlineUs == policy.bestEffortDeadlineUs);
                r.step();
                CHECK(r.wire.sentCount == from + 1 && isWord(r, from, WORD_QUICK)); // Never 010F here.
                finish(r, r.a);
                checkEnded(r.a.bestEffort(), M::ActionOutcome::STOP_SENT_UNCONFIRMED,
                           M::ActionExecution::UNKNOWN);
                quiet(r, from + 1);
            }
}

// 4. Expiry at every boundary.
void queuedStopAtExactQueueCutoff() {
    for (unsigned allowed = 0; allowed < 2; ++allowed) {
        Rig r;
        openAxis(r, r.a);
        L::ReadContext state;
        history(r, r.a, state);
        const uint32_t pollOperation = begin(r, r.a);
        r.drive[0].answer = false;
        CHECK(r.a.admitPrepared(service(r, 1, pollOperation, 1, 0x2100, false, 0, reads(50, 1000)), r.now) ==
              O::BusAdmission::ACCEPTED);
        r.step();
        const unsigned pollFrame = r.wire.sentCount - 1;
        const uint64_t start = r.now;
        const E::StopPolicy policy = stopPolicy(start, 5000, allowed ? 6000 : 0, 50, 1000);
        CHECK(r.a.requestStop(L::StopKind::HALT, state, policy, start) == E::StopPath::CONFIRMED);
        r.turn(start + 49);
        CHECK(r.a.stopAction(L::StopKind::HALT).state == M::ActionState::ACTIVE);
        r.turn(start + 50);
        const L::ActionContext& stop = r.a.stopAction(L::StopKind::HALT);
        CHECK(stop.state == M::ActionState::FAILED && stop.outcome == M::ActionOutcome::SERVICE_TIMEOUT &&
              stop.execution == M::ActionExecution::NOT_TRANSMITTED && !stop.uncertain);
        if (allowed) {
            CHECK(r.a.stopPath() == E::StopPath::BEST_EFFORT);
            r.turn(start + 99);
            CHECK(r.a.stopPath() == E::StopPath::BEST_EFFORT);
            r.turn(start + 100);
            checkEnded(r.a.bestEffort(), M::ActionOutcome::SERVICE_TIMEOUT, M::ActionExecution::NOT_TRANSMITTED);
        }
        CHECK(r.a.stopPath() == E::StopPath::FINISHED);
        CHECK(r.wire.sentCount == pollFrame + 1);
        // The poll was never disturbed.
        r.answer(pollFrame);
        r.step();
        CHECK(settled(r.a).outcome == C::SdoOutcome::SUCCESS);
    }
}
void sequencedOverallDeadlineEquality() {
    // Queued behind a poll, nothing possible.
    {
        Rig r;
        openAxis(r, r.a);
        L::ReadContext state;
        history(r, r.a, state);
        const uint32_t pollOperation = begin(r, r.a);
        r.drive[0].answer = false;
        CHECK(r.a.admitPrepared(service(r, 1, pollOperation, 1, 0x2100, false, 0, reads(50, 1000)), r.now) ==
              O::BusAdmission::ACCEPTED);
        r.step();
        const unsigned sent = r.wire.sentCount;
        const E::StopPolicy policy = stopPolicy(r.now, 30, 0, 50, 1000);
        CHECK(r.a.requestStop(L::StopKind::QUICK_STOP, state, policy, r.now) == E::StopPath::CONFIRMED);
        r.turn(policy.deadlineUs - 1);
        CHECK(r.a.stopAction(L::StopKind::QUICK_STOP).state == M::ActionState::ACTIVE);
        r.turn(policy.deadlineUs);
        const L::ActionContext& stop = r.a.stopAction(L::StopKind::QUICK_STOP);
        CHECK(stop.outcome == M::ActionOutcome::DEADLINE &&
              stop.execution == M::ActionExecution::NOT_TRANSMITTED && r.wire.sentCount == sent);
    }
    // On the wire with no controller report and no reply. A reply captured earlier but only
    // processed at the exact cutoff loses. The frame is never sent again.
    for (unsigned reported = 0; reported < 2; ++reported)
        for (unsigned bestEffort = 0; bestEffort < 2; ++bestEffort) {
            Rig r;
            openAxis(r, r.a);
            L::ReadContext state;
            history(r, r.a, state);
            r.drive[0].answer = false;
            r.wire.autoReport = reported != 0;
            const E::StopPolicy policy = stopPolicy(r.now, 30, bestEffort ? 60 : 0, 50, 1000);
            CHECK(r.a.requestStop(L::StopKind::HALT, state, policy, r.now) == E::StopPath::CONFIRMED);
            const unsigned from = r.wire.sentCount;
            r.step();
            CHECK(r.wire.sentCount == from + 1 && isWord(r, from, WORD_HALT));
            r.turn(policy.deadlineUs - 1);
            CHECK(r.a.stopAction(L::StopKind::HALT).state == M::ActionState::ACTIVE);
            r.wire.push(replyTo(r.wire.sent[from], r.drive[0]), policy.deadlineUs - 1, r.owner.controllerEpoch());
            r.turn(policy.deadlineUs);
            const L::ActionContext& stop = r.a.stopAction(L::StopKind::HALT);
            CHECK(stop.state == M::ActionState::FAILED && stop.outcome == M::ActionOutcome::DEADLINE &&
                  stop.execution == M::ActionExecution::UNKNOWN && stop.uncertain && stop.effectsPossible);
            CHECK(r.a.stopPath() == E::StopPath::FINISHED && !r.a.latestControl()); // No branch after possible TX.
            r.turn(policy.deadlineUs + 100);
            CHECK(r.wire.sentCount == from + 1);
        }
}
void bestEffortDeadlineEquality() {
    // Requests at or past their cutoffs are refused with nothing changed.
    {
        Rig r;
        openAxis(r, r.a);
        L::ReadContext state;
        r.step();
        const Saved<E::AxisSession> before(r.a);
        E::StopPolicy policy = stopPolicy(r.now);
        policy.deadlineUs = r.now;
        CHECK(r.a.requestStop(L::StopKind::QUICK_STOP, state, policy, r.now) == E::StopPath::NONE);
        policy = stopPolicy(r.now);
        policy.bestEffortDeadlineUs = policy.deadlineUs - 1;
        CHECK(r.a.requestStop(L::StopKind::QUICK_STOP, state, policy, r.now) == E::StopPath::NONE);
        policy = stopPolicy(r.now);
        CHECK(r.a.requestStop(L::StopKind::QUICK_STOP, state, policy, r.now - 1) == E::StopPath::NONE);
        before.unchanged(r.a);
    }
    // Waiting behind local work, nothing possible.
    {
        Rig r;
        openAxis(r, r.a);
        const uint32_t pollOperation = begin(r, r.a);
        r.drive[0].answer = false;
        CHECK(r.a.admitPrepared(service(r, 1, pollOperation, 1, 0x2100, false, 0, reads(50, 1000)), r.now) ==
              O::BusAdmission::ACCEPTED);
        r.step();
        const unsigned sent = r.wire.sentCount;
        L::ReadContext state;
        const E::StopPolicy policy = stopPolicy(r.now, 20, 30, 50, 1000);
        CHECK(r.a.requestStop(L::StopKind::HALT, state, policy, r.now) == E::StopPath::BEST_EFFORT);
        r.turn(policy.bestEffortDeadlineUs - 1);
        CHECK(r.a.stopPath() == E::StopPath::BEST_EFFORT);
        r.turn(policy.bestEffortDeadlineUs);
        checkEnded(r.a.bestEffort(), M::ActionOutcome::DEADLINE, M::ActionExecution::NOT_TRANSMITTED);
        CHECK(r.wire.sentCount == sent);
        CHECK(!r.a.bestEffort().commandEvidence.transmissionPossible);
    }
    // On the wire with no report and no reply: possible TX stays UNKNOWN.
    {
        Rig r;
        openAxis(r, r.a);
        r.wire.autoReport = r.wire.retirement = false;
        r.drive[0].answer = false;
        L::ReadContext state;
        const E::StopPolicy policy = stopPolicy(r.now, 20, 30, 50, 1000);
        CHECK(r.a.requestStop(L::StopKind::HALT, state, policy, r.now) == E::StopPath::BEST_EFFORT);
        const unsigned from = r.wire.sentCount;
        r.step();
        CHECK(r.wire.sentCount == from + 1 && isWord(r, from, WORD_QUICK));
        r.turn(policy.bestEffortDeadlineUs - 1);
        CHECK(r.a.bestEffort().state == M::ActionState::ACTIVE && r.a.bestEffort().admitted);
        r.turn(policy.bestEffortDeadlineUs);
        checkEnded(r.a.bestEffort(), M::ActionOutcome::DEADLINE, M::ActionExecution::UNKNOWN);
        const Saved<L::BestEffortStop> closed(r.a.bestEffort());
        // A late final report cannot change the deadline, or say more than the terminal did.
        r.wire.report(from);
        r.step();
        closed.unchanged(r.a.bestEffort());
        CHECK(r.services.unresolved(0) && r.wire.sentCount == from + 1);
    }
    // A response budget ending first gives the same evidence.
    {
        Rig r;
        openAxis(r, r.a);
        r.wire.autoReport = false;
        r.drive[0].answer = false;
        L::ReadContext state;
        const E::StopPolicy policy = stopPolicy(r.now, 5000, 6000);
        CHECK(r.a.requestStop(L::StopKind::QUICK_STOP, state, policy, r.now) == E::StopPath::BEST_EFFORT);
        r.step();
        r.turn(r.now + 100);
        checkEnded(r.a.bestEffort(), M::ActionOutcome::SERVICE_TIMEOUT, M::ActionExecution::UNKNOWN);
    }
    // A refused enqueue is not proof that nothing left.
    {
        Rig r;
        openAxis(r, r.a);
        r.wire.accept = r.wire.autoReport = r.wire.retirement = false;
        r.drive[0].answer = false;
        L::ReadContext state;
        CHECK(r.a.requestStop(L::StopKind::QUICK_STOP, state, stopPolicy(r.now), r.now) ==
              E::StopPath::BEST_EFFORT);
        r.step();
        finish(r, r.a);
        checkEnded(r.a.bestEffort(), M::ActionOutcome::TRANSPORT_ERROR, M::ActionExecution::UNKNOWN);
    }
    // Held by a fault until the exact cutoff.
    {
        Rig r;
        openAxis(r, r.a);
        L::ReadContext state;
        history(r, r.a, state);
        const E::StopPolicy policy = stopPolicy(r.now, 300, 400);
        CHECK(r.a.requestStop(L::StopKind::HALT, state, policy, r.now) == E::StopPath::CONFIRMED);
        const unsigned sent = r.wire.sentCount;
        CHECK(r.owner.reportController(C::ControllerState::BUS_OFF, r.now));
        r.step();
        CHECK(r.a.stopPath() == E::StopPath::HELD);
        CHECK(r.a.bestEffort().deadlineUs == policy.deadlineUs);
        r.turn(policy.deadlineUs - 1);
        CHECK(r.a.stopPath() == E::StopPath::HELD);
        r.turn(policy.deadlineUs);
        checkEnded(r.a.bestEffort(), M::ActionOutcome::DEADLINE, M::ActionExecution::NOT_TRANSMITTED);
        CHECK(r.a.stopPath() == E::StopPath::FINISHED && r.wire.sentCount == sent);
    }
}

// 5. Fault and reboot.
void recoverOnce(Rig& r, O::RecoveryId& id) {
    CHECK(r.owner.recover(r.now, r.now + 200, id));
    r.step();
    CHECK(r.owner.recoveryResult(id));
}
void busOffThenRecoveryThenOnce000B() {
    // With a poll on the wire the route also ends quarantined. Without one it stays clean.
    for (unsigned poll = 0; poll < 2; ++poll)
        for (unsigned kind = 0; kind < 2; ++kind) {
            const L::StopKind selected = kind ? L::StopKind::QUICK_STOP : L::StopKind::HALT;
            Rig r;
            openAxis(r, r.a);
            L::ReadContext state;
            history(r, r.a, state);
            if (poll) {
                const uint32_t pollOperation = begin(r, r.a);
                r.drive[0].answer = false;
                CHECK(r.a.admitPrepared(service(r, 1, pollOperation, 1, 0x2100), r.now) ==
                      O::BusAdmission::ACCEPTED);
                r.step();
            }
            const unsigned from = r.wire.sentCount;
            const E::StopPolicy policy = stopPolicy(r.now);
            CHECK(r.a.requestStop(selected, state, policy, r.now) == E::StopPath::CONFIRMED);
            CHECK(r.owner.reportController(C::ControllerState::BUS_OFF, r.now));
            r.step();
            CHECK(r.a.stopPath() == E::StopPath::HELD && r.wire.sentCount == from);
            const L::ActionContext& first = r.a.stopAction(selected);
            CHECK(first.state == M::ActionState::FAILED &&
                  first.execution == M::ActionExecution::NOT_TRANSMITTED);
            r.steps(3);
            CHECK(r.a.stopPath() == E::StopPath::HELD && r.wire.sentCount == from);
            CHECK(r.a.bestEffort().deadlineUs == policy.deadlineUs);
            O::RecoveryId recovery;
            recoverOnce(r, recovery);
            CHECK(r.owner.recoveryResult(recovery)->outcome == O::RecoveryOutcome::RETIRED);
            CHECK(r.a.bestEffort().deadlineUs == policy.deadlineUs); // Recovery never extends it.
            CHECK(r.a.firstBestEffort().state == M::ActionState::EMPTY); // No earlier attempt here.
            CHECK(r.a.stopPath() == E::StopPath::BEST_EFFORT);
            r.step();
            CHECK(r.wire.sentCount == from + 1 && isWord(r, from, WORD_QUICK)); // 000B whatever was asked.
            finish(r, r.a);
            checkEnded(r.a.bestEffort(), M::ActionOutcome::STOP_SENT_UNCONFIRMED, M::ActionExecution::UNKNOWN);
            quiet(r, from + 1);
            CHECK(r.services.unresolved(0));
        }
}
void recoveryInterruptingQueuedStop() {
    // recover() itself retires the queued stop. That recovery is the one it may wait across.
    Rig r;
    openAxis(r, r.a);
    L::ReadContext state;
    history(r, r.a, state);
    const unsigned from = r.wire.sentCount;
    const E::StopPolicy policy = stopPolicy(r.now);
    CHECK(r.a.requestStop(L::StopKind::HALT, state, policy, r.now) == E::StopPath::CONFIRMED);
    O::RecoveryId first;
    r.wire.progress = O::RecoveryProgress::PENDING;
    CHECK(r.owner.recover(r.now, r.now + 200, first));
    r.step();
    CHECK(r.a.stopPath() == E::StopPath::HELD && r.wire.sentCount == from);
    r.wire.progress = O::RecoveryProgress::RETIRED;
    r.steps(2);
    CHECK(r.owner.recoveryResult(first)->outcome == O::RecoveryOutcome::RETIRED);
    r.step();
    CHECK(r.wire.sentCount == from + 1 && isWord(r, from, WORD_QUICK));
    finish(r, r.a);
    checkEnded(r.a.bestEffort(), M::ActionOutcome::STOP_SENT_UNCONFIRMED, M::ActionExecution::UNKNOWN);
}
void failedOrSecondRecoveryEndsNotTransmitted() {
    // A failed recovery leaves the stop held, and the fixed deadline ends it.
    {
        Rig r;
        openAxis(r, r.a);
        L::ReadContext state;
        history(r, r.a, state);
        const unsigned from = r.wire.sentCount;
        const E::StopPolicy policy = stopPolicy(r.now, 300, 400);
        CHECK(r.a.requestStop(L::StopKind::HALT, state, policy, r.now) == E::StopPath::CONFIRMED);
        CHECK(r.owner.reportController(C::ControllerState::BUS_OFF, r.now));
        r.step();
        r.wire.progress = O::RecoveryProgress::FAILED;
        O::RecoveryId id;
        recoverOnce(r, id);
        CHECK(r.owner.recoveryResult(id)->outcome == O::RecoveryOutcome::FAILED);
        CHECK(r.a.stopPath() == E::StopPath::HELD);
        r.turn(policy.deadlineUs);
        checkEnded(r.a.bestEffort(), M::ActionOutcome::DEADLINE, M::ActionExecution::NOT_TRANSMITTED);
        CHECK(r.wire.sentCount == from);
    }
    // A second recovery ends it, even a successful one.
    for (unsigned failedFirst = 0; failedFirst < 2; ++failedFirst) {
        Rig r;
        openAxis(r, r.a);
        L::ReadContext state;
        history(r, r.a, state);
        const unsigned from = r.wire.sentCount;
        CHECK(r.a.requestStop(L::StopKind::HALT, state, stopPolicy(r.now), r.now) == E::StopPath::CONFIRMED);
        CHECK(r.owner.reportController(C::ControllerState::BUS_OFF, r.now));
        r.step();
        r.wire.progress = failedFirst ? O::RecoveryProgress::FAILED : O::RecoveryProgress::PENDING;
        O::RecoveryId id;
        CHECK(r.owner.recover(r.now, r.now + (failedFirst ? 200 : 5), id));
        r.turn(r.now + 5);
        CHECK(r.owner.recoveryResult(id));
        CHECK(r.a.stopPath() == E::StopPath::HELD);
        CHECK(r.owner.releaseRecovery(id));
        r.wire.progress = O::RecoveryProgress::RETIRED;
        O::RecoveryId second;
        CHECK(r.owner.recover(r.now, r.now + 200, second));
        r.step();
        CHECK(r.a.stopPath() == E::StopPath::FINISHED);
        checkEnded(r.a.bestEffort(), M::ActionOutcome::TRANSPORT_ERROR, M::ActionExecution::NOT_TRANSMITTED);
        r.steps(3);
        CHECK(r.owner.recoveryResult(second)->outcome == O::RecoveryOutcome::RETIRED);
        CHECK(r.wire.sentCount == from);
    }
    // Interrupted by a recovery, then by a second one.
    {
        Rig r;
        openAxis(r, r.a);
        L::ReadContext state;
        history(r, r.a, state);
        const unsigned from = r.wire.sentCount;
        CHECK(r.a.requestStop(L::StopKind::HALT, state, stopPolicy(r.now), r.now) == E::StopPath::CONFIRMED);
        r.wire.progress = O::RecoveryProgress::FAILED;
        O::RecoveryId id;
        recoverOnce(r, id);
        CHECK(r.a.stopPath() == E::StopPath::HELD);
        CHECK(r.owner.releaseRecovery(id));
        CHECK(r.owner.recover(r.now, r.now + 200, id));
        r.step();
        checkEnded(r.a.bestEffort(), M::ActionOutcome::TRANSPORT_ERROR, M::ActionExecution::NOT_TRANSMITTED);
        CHECK(r.wire.sentCount == from);
    }
}
void possibleWriteIsNeverResent() {
    // A sequenced stop on the wire, then bus-off, then recovery.
    for (unsigned acknowledged = 0; acknowledged < 2; ++acknowledged) {
        Rig r;
        openAxis(r, r.a);
        L::ReadContext state;
        history(r, r.a, state);
        r.drive[0].answer = false;
        r.wire.autoReport = false;
        const unsigned from = r.wire.sentCount;
        CHECK(r.a.requestStop(L::StopKind::HALT, state, stopPolicy(r.now), r.now) == E::StopPath::CONFIRMED);
        r.step();
        CHECK(isWord(r, from, WORD_HALT));
        if (acknowledged) {
            r.wire.report(from);
            r.step();
        }
        CHECK(r.owner.reportController(C::ControllerState::BUS_OFF, r.now));
        r.step();
        CHECK(r.a.stopPath() == E::StopPath::FINISHED);
        CHECK(r.a.stopAction(L::StopKind::HALT).execution == M::ActionExecution::UNKNOWN);
        CHECK(r.a.bestEffort().state == M::ActionState::EMPTY);
        O::RecoveryId id;
        recoverOnce(r, id);
        r.steps(4);
        CHECK(r.wire.sentCount == from + 1);
    }
    // The best-effort frame on the wire, then bus-off, then recovery.
    {
        Rig r;
        openAxis(r, r.a);
        r.wire.autoReport = false;
        r.drive[0].answer = false;
        L::ReadContext state;
        const unsigned from = r.wire.sentCount;
        CHECK(r.a.requestStop(L::StopKind::HALT, state, stopPolicy(r.now), r.now) == E::StopPath::BEST_EFFORT);
        r.step();
        CHECK(r.wire.sentCount == from + 1 && isWord(r, from, WORD_QUICK));
        CHECK(r.owner.reportController(C::ControllerState::BUS_OFF, r.now));
        r.step();
        CHECK(r.a.stopPath() == E::StopPath::FINISHED);
        checkEnded(r.a.bestEffort(), M::ActionOutcome::TRANSPORT_ERROR, M::ActionExecution::UNKNOWN);
        O::RecoveryId id;
        recoverOnce(r, id);
        r.steps(4);
        CHECK(r.wire.sentCount == from + 1);
    }
}
void bootEndsStopWithNoTransmission() {
    M::Frame boot = frame(0x701, 1);
    // Queued sequenced stop.
    for (unsigned allowed = 0; allowed < 2; ++allowed) {
        Rig r;
        openAxis(r, r.a);
        L::ReadContext state;
        history(r, r.a, state);
        const unsigned from = r.wire.sentCount;
        const uint32_t pollOperation = begin(r, r.a);
        r.drive[0].answer = false;
        CHECK(r.a.admitPrepared(service(r, 1, pollOperation, 1, 0x2100), r.now) == O::BusAdmission::ACCEPTED);
        r.step();
        CHECK(r.a.requestStop(L::StopKind::HALT, state, stopPolicy(r.now, 5000, allowed ? 6000 : 0), r.now) ==
              E::StopPath::CONFIRMED);
        r.later(boot);
        r.step();
        CHECK(r.a.stopPath() == E::StopPath::FINISHED);
        const L::ActionContext& stop = r.a.stopAction(L::StopKind::HALT);
        CHECK(stop.outcome == M::ActionOutcome::INVALIDATED &&
              stop.execution == M::ActionExecution::NOT_TRANSMITTED && !stop.effectsPossible);
        CHECK(r.a.bestEffort().state == M::ActionState::EMPTY); // A new server session needs new intent.
        CHECK(r.wire.sentCount == from + 1);
        CHECK(settled(r.a).outcome == C::SdoOutcome::REBOOT);
        CHECK(r.a.close());
    }
    // Queued best-effort stop.
    {
        Rig r;
        openAxis(r, r.a);
        L::ReadContext state;
        const unsigned from = r.wire.sentCount;
        CHECK(r.a.requestStop(L::StopKind::QUICK_STOP, state, stopPolicy(r.now), r.now) ==
              E::StopPath::BEST_EFFORT);
        r.later(boot);
        r.step();
        checkEnded(r.a.bestEffort(), M::ActionOutcome::INVALIDATED, M::ActionExecution::NOT_TRANSMITTED);
        CHECK(r.wire.sentCount == from);
    }
    // Held across a fault.
    {
        Rig r;
        openAxis(r, r.a);
        L::ReadContext state;
        history(r, r.a, state);
        const unsigned from = r.wire.sentCount;
        CHECK(r.a.requestStop(L::StopKind::HALT, state, stopPolicy(r.now), r.now) == E::StopPath::CONFIRMED);
        CHECK(r.owner.reportController(C::ControllerState::BUS_OFF, r.now));
        r.step();
        CHECK(r.a.stopPath() == E::StopPath::HELD);
        r.later(boot);
        r.step();
        checkEnded(r.a.bestEffort(), M::ActionOutcome::INVALIDATED, M::ActionExecution::NOT_TRANSMITTED);
        O::RecoveryId id;
        recoverOnce(r, id);
        quiet(r, from);
    }
    // A new request after the boot needs a session opened on the new server session.
    {
        Rig r;
        openAxis(r, r.a);
        L::ReadContext state;
        r.later(boot);
        r.step();
        const Saved<E::AxisSession> before(r.a);
        const unsigned from = r.wire.sentCount;
        CHECK(r.a.requestStop(L::StopKind::QUICK_STOP, state, stopPolicy(r.now), r.now) == E::StopPath::NONE);
        before.unchanged(r.a);
        CHECK(r.a.close());
        L::ReadTarget stale = target();
        CHECK(!r.a.open(stale, L::SettingsModel::CL86_C, 1, r.now));
        quiet(r, from);
    }
}

// 6. Old-history replay.
enum class Withdrawal {
    TARGET,
    MODE,
    RPDO,
    CONFIGURATION,
    CONTROL,
    NMT_RESET,
    BOOT,
    REBIND,
    RECOVERY,
    BUS_OFF,
    UNCERTAIN_DOWNLOAD,
    UNCERTAIN_UPLOAD,
    QUARANTINE
};
void withdrawnHistoryStaysWithdrawn() {
    const Withdrawal events[] = {Withdrawal::TARGET,     Withdrawal::MODE,    Withdrawal::RPDO,
                                 Withdrawal::CONFIGURATION, Withdrawal::CONTROL, Withdrawal::NMT_RESET,
                                 Withdrawal::BOOT,       Withdrawal::REBIND,  Withdrawal::RECOVERY,
                                 Withdrawal::BUS_OFF,    Withdrawal::UNCERTAIN_DOWNLOAD,
                                 Withdrawal::UNCERTAIN_UPLOAD, Withdrawal::QUARANTINE};
    for (Withdrawal event : events) {
        Rig r;
        openAxis(r, r.a);
        L::ReadContext state;
        history(r, r.a, state);
        const C::SdoCompletion copied = *r.a.latestControl();
        uint16_t index = 0;
        switch (event) {
        case Withdrawal::TARGET:
            index = 0x607a;
            break;
        case Withdrawal::MODE:
            index = 0x6060;
            break;
        case Withdrawal::RPDO:
            index = 0x1400;
            break;
        case Withdrawal::CONFIGURATION:
            index = 0x6081;
            break;
        case Withdrawal::CONTROL:
            index = 0x6040;
            break;
        default:
            break;
        }
        if (index) {
            // At admission, before any report, because it may reach the drive from here on.
            const uint32_t operation = begin(r, r.a);
            CHECK(r.a.admitPrepared(service(r, 1, operation, 1, index, true, WORD_ENABLE), r.now) ==
                  O::BusAdmission::ACCEPTED);
            CHECK(!r.a.latestControl());
            r.steps(2);
            CHECK(settled(r.a).outcome == C::SdoOutcome::SUCCESS);
            if (event == Withdrawal::CONTROL) {
                // Only this write's own acknowledgement becomes the new latest word.
                CHECK(r.a.latestControl() && r.a.latestControl()->completedUs > copied.completedUs);
                continue;
            }
        } else if (event == Withdrawal::NMT_RESET) {
            C::NmtTransfer reset;
            reset.command = C::NmtCommand::RESET_NODE;
            reset.target = r.services.node(0)->route;
            reset.request = 123;
            reset.deadlineUs = r.now + 100;
            reset.queueTimeoutUs = 20;
            C::NmtRequestId id;
            CHECK(r.owner.admitNmt(reset, r.now, id));
            r.steps(2);
            CHECK(r.owner.nmtResult(id));
        } else if (event == Withdrawal::BOOT) {
            r.later(frame(0x701, 1));
            r.step();
        } else if (event == Withdrawal::REBIND) {
            CHECK(r.services.invalidateNetwork(0, C::NetworkInvalidation::BINDING, r.now));
            r.step();
        } else if (event == Withdrawal::RECOVERY) {
            O::RecoveryId id;
            recoverOnce(r, id);
            CHECK(r.owner.recoveryResult(id)->outcome == O::RecoveryOutcome::RETIRED);
        } else if (event == Withdrawal::BUS_OFF) {
            CHECK(r.owner.reportController(C::ControllerState::BUS_OFF, r.now));
            r.step();
        } else if (event == Withdrawal::UNCERTAIN_DOWNLOAD || event == Withdrawal::UNCERTAIN_UPLOAD) {
            const bool download = event == Withdrawal::UNCERTAIN_DOWNLOAD;
            const uint32_t operation = begin(r, r.a);
            r.drive[0].answer = false;
            CHECK(r.a.admitPrepared(service(r, 1, operation, 1, download ? 0x607a : 0x2100, download), r.now) ==
                  O::BusAdmission::ACCEPTED);
            CHECK(bool(r.a.latestControl()) == !download);
            r.step();
            r.turn(r.now + 100);
            CHECK(settled(r.a).outcome == C::SdoOutcome::TIMEOUT);
        } else if (event == Withdrawal::QUARANTINE) {
            // Cancelling work that may have left quarantines the route.
            const uint32_t operation = begin(r, r.a);
            r.drive[0].answer = false;
            O::BusRequest raw;
            raw.sequence = r.a.sequence();
            raw.transfer = service(r, 1, operation, 1, 0x2100);
            O::RequestId id;
            CHECK(r.owner.admit(raw, r.now, id) == O::BusAdmission::ACCEPTED);
            r.step();
            CHECK(r.owner.cancel(id, r.now) == O::Cancel::CANCELLED && r.services.unresolved(0));
            r.step();
            CHECK(r.owner.release(id));
        }
        CHECK(!r.a.latestControl());
        // No later turn, read or request brings it back, and nothing can stop on it.
        r.steps(2);
        CHECK(!r.a.latestControl());
        const unsigned sent = r.wire.sentCount;
        const Saved<O::OwnerResources> resources(r.owner.resources());
        CHECK(r.a.requestStop(L::StopKind::HALT, state, stopPolicy(r.now, 5000, 0), r.now) == E::StopPath::NONE);
        resources.unchanged(r.owner.resources());
        CHECK(r.wire.sentCount == sent);
        // Reopening does not restore it, and neither does a copy of the old terminal.
        CHECK(r.a.close());
        openAxis(r, r.a);
        CHECK(!r.a.latestControl());
        if (r.owner.sequence(r.a.sequence())) {
            L::ReadContext fresh;
            readState(r, r.a, fresh);
            L::ActionContext replay;
            const Saved<L::ActionContext> empty(replay);
            CHECK(!L::prepareStop(replay, reservationOf(r, r.a), fresh, copied, L::StopKind::HALT,
                                  UINT32_MAX - 1, r.now, r.now + 1000, stopPolicy(r.now).options));
            empty.unchanged(replay);
            CHECK(r.a.requestStop(L::StopKind::HALT, fresh, stopPolicy(r.now, 5000, 0), r.now) == E::StopPath::NONE);
        }
    }
    // An upload leaves the word in place, and a plain reopen drops it.
    Rig r;
    openAxis(r, r.a);
    L::ReadContext state;
    history(r, r.a, state);
    const C::SdoCompletion copied = *r.a.latestControl();
    readState(r, r.a, state);
    CHECK(r.a.latestControl());
    CHECK(r.a.close());
    openAxis(r, r.a);
    CHECK(!r.a.latestControl());
    readState(r, r.a, state);
    L::ActionContext replay;
    const Saved<L::ActionContext> empty(replay);
    CHECK(!L::prepareStop(replay, reservationOf(r, r.a), state, copied, L::StopKind::QUICK_STOP, UINT32_MAX - 1,
                          r.now, r.now + 1000, stopPolicy(r.now).options));
    empty.unchanged(replay);
    // A forged word in a copy of the true latest terminal is still not this session's latest.
    C::SdoCompletion forged = copied;
    forged.transfer.data[0] = 0x1f;
    CHECK(!L::prepareStop(replay, reservationOf(r, r.a), state, forged, L::StopKind::QUICK_STOP, UINT32_MAX - 1,
                          r.now, r.now + 1000, stopPolicy(r.now).options));
    empty.unchanged(replay);
}
void writeSettlingAfterStopDoesNotRestore() {
    for (unsigned word = 0; word < 2; ++word) {
        Rig r;
        openAxis(r, r.a);
        L::ReadContext state;
        history(r, r.a, state);
        const uint32_t operation = begin(r, r.a);
        r.drive[0].answer = false;
        CHECK(r.a.admitPrepared(service(r, 1, operation, 1, 0x6040, true, word ? WORD_TRIGGER : WORD_ENABLE), r.now) ==
              O::BusAdmission::ACCEPTED);
        r.step();
        const unsigned writeFrame = r.wire.sentCount - 1;
        CHECK(r.a.requestStop(L::StopKind::HALT, state, stopPolicy(r.now), r.now) == E::StopPath::BEST_EFFORT);
        CHECK(!r.a.latestControl());
        r.answer(writeFrame);
        r.step(); // The write is acknowledged, the stop is admitted behind it.
        const C::SdoCompletion settledWrite = settled(r.a);
        CHECK(settledWrite.outcome == C::SdoOutcome::SUCCESS);
        CHECK(!r.a.latestControl());
        finish(r, r.a);
        CHECK(!r.a.latestControl());
        CHECK(isWord(r, writeFrame + 1, WORD_QUICK) && r.wire.sentCount == writeFrame + 2);
    }
    // The same when the write settles only after the stop has ended, with its route clean.
    Rig r;
    openAxis(r, r.a);
    L::ReadContext state;
    history(r, r.a, state);
    const uint32_t operation = begin(r, r.a);
    r.drive[0].answer = false;
    CHECK(r.a.admitPrepared(service(r, 1, operation, 1, 0x6040, true, WORD_ENABLE, reads(50, 1000)), r.now) ==
          O::BusAdmission::ACCEPTED);
    r.step();
    const unsigned writeFrame = r.wire.sentCount - 1;
    CHECK(r.a.requestStop(L::StopKind::HALT, state, stopPolicy(r.now, 20, 30, 50, 1000), r.now) == E::StopPath::BEST_EFFORT);
    r.turn(r.now + 30);
    CHECK(r.a.stopPath() == E::StopPath::FINISHED);
    checkEnded(r.a.bestEffort(), M::ActionOutcome::DEADLINE, M::ActionExecution::NOT_TRANSMITTED);
    r.answer(writeFrame);
    r.step();
    CHECK(settled(r.a).outcome == C::SdoOutcome::SUCCESS && !r.services.unresolved(0));
    CHECK(!r.a.latestControl());
}
void idleAtEnableStillStops() {
    for (unsigned kind = 0; kind < 2; ++kind) {
        const L::StopKind selected = kind ? L::StopKind::QUICK_STOP : L::StopKind::HALT;
        Rig r;
        openAxis(r, r.a);
        L::ReadContext state;
        history(r, r.a, state);
        const E::StopPolicy policy = stopPolicy(r.now);
        const uint64_t acknowledged = r.a.latestControl()->completedUs;
        r.turn(acknowledged + 3 * policy.options.maximumAgeUs);
        // The state read has expired. It takes the 000B branch, or is refused without it.
        const Saved<E::AxisSession> before(r.a);
        CHECK(r.a.requestStop(selected, state, stopPolicy(r.now, 5000, 0), r.now) == E::StopPath::NONE);
        before.unchanged(r.a);
        readState(r, r.a, state);
        CHECK(r.now - acknowledged > policy.options.maximumAgeUs);
        const unsigned from = r.wire.sentCount;
        CHECK(r.a.requestStop(selected, state, stopPolicy(r.now), r.now) == E::StopPath::CONFIRMED);
        r.step();
        CHECK(isWord(r, from, stopWord(selected)) && r.wire.sentCount == from + 1);
        CHECK(r.a.stopAction(selected).predecessorDocumented);
    }
    Rig r;
    openAxis(r, r.a);
    L::ReadContext state;
    history(r, r.a, state);
    const E::StopPolicy policy = stopPolicy(r.now);
    r.turn(r.now + 2 * policy.options.maximumAgeUs);
    const unsigned from = r.wire.sentCount;
    const E::StopPolicy second = stopPolicy(r.now);
    CHECK(r.a.latestControl());
    CHECK(r.a.requestStop(L::StopKind::HALT, state, second, r.now) == E::StopPath::BEST_EFFORT);
    CHECK(!r.a.latestControl()); // Its admission withdraws the word.
    CHECK(r.owner.sequence(r.a.sequence())->deadlineUs == second.bestEffortDeadlineUs);
    r.step();
    CHECK(isWord(r, from, WORD_QUICK));
}

// 7. Two nodes under ordinary pressure.
void twoNodePressure() {
    for (unsigned hog = 0; hog < 2; ++hog) {
        Rig r;
        openAxis(r, r.a, 1);
        openAxis(r, r.b, 2);
        L::ReadContext stateA, stateB;
        history(r, r.a, stateA, WORD_ENABLE, 1);
        history(r, r.b, stateB, WORD_ENABLE, 2);
        readState(r, r.a, stateA, 1);
        // Fill every shared ordinary slot, from both axes or from one. Unsent and unread.
        const uint32_t opA = begin(r, r.a);
        const uint64_t deadlineA = r.opDeadline;
        const uint32_t opB = begin(r, r.b);
        const unsigned fromA = hog ? O::CanBusOwner::REQUESTS : 3;
        for (unsigned i = 0; i < O::CanBusOwner::REQUESTS; ++i) {
            const bool first = i < fromA;
            O::BusRequest work;
            work.sequence = first ? r.a.sequence() : r.b.sequence();
            work.transfer = service(r, first ? 1 : 2, first ? opA : opB, i + 1, 0x2100);
            if (first)
                work.transfer.deadlineUs = deadlineA;
            O::RequestId id;
            CHECK(r.owner.admit(work, r.now, id) == O::BusAdmission::ACCEPTED);
        }
        O::BusRequest more;
        more.sequence = r.a.sequence();
        more.transfer = service(r, 1, opA, 9, 0x2100);
        more.transfer.deadlineUs = deadlineA;
        O::RequestId refused;
        CHECK(r.owner.admit(more, r.now, refused) == O::BusAdmission::RESULTS_FULL);
        more.sequence = r.b.sequence();
        more.transfer = service(r, 2, opB, 9, 0x2100);
        CHECK(r.owner.admit(more, r.now, refused) == O::BusAdmission::RESULTS_FULL);
        // A stop on axis 1 runs with its terminal left unread.
        const unsigned from = r.wire.sentCount;
        r.drive[0].monitor = r.drive[1].monitor = 0;
        CHECK(r.a.requestStop(L::StopKind::QUICK_STOP, stateA, stopPolicy(r.now), r.now) == E::StopPath::CONFIRMED);
        r.serviceA = false;
        r.steps(2);
        CHECK(r.a.stopAction(L::StopKind::QUICK_STOP).state == M::ActionState::ACTIVE);
        CHECK(writes6040(r, from, 1) == 1 && writes6040(r, from, 2) == 0);
        // Axis 2 is not blocked by that unread terminal, nor by the ordinary pressure.
        L::ReadContext stale = stateB;
        CHECK(r.b.requestStop(L::StopKind::HALT, stale, stopPolicy(r.now), r.now) == E::StopPath::CONFIRMED);
        finish(r, r.b);
        CHECK(r.b.stopAction(L::StopKind::HALT).state == M::ActionState::SUCCEEDED);
        CHECK(writes6040(r, from, 2) == 1 && writes6040(r, from, 1) == 1);
        bool sentHalt = false;
        for (unsigned i = from; i < r.wire.sentCount; ++i)
            sentHalt |= isWord(r, i, WORD_HALT, 2);
        CHECK(sentHalt);
        // Axis 1 still holds its result, and then completes.
        CHECK(r.a.stopAction(L::StopKind::QUICK_STOP).state == M::ActionState::ACTIVE);
        r.serviceA = true;
        finish(r, r.a);
        CHECK(r.a.stopAction(L::StopKind::QUICK_STOP).state == M::ActionState::SUCCEEDED);
        CHECK(writes6040(r, from, 1) == 1);
        unsigned retained = r.owner.resources().retained;
        CHECK(retained >= O::CanBusOwner::REQUESTS); // Retired work keeps its explicit results.
    }
}

// 8. Escalation.
void escalationFromQueuedHalt() {
    // Without an in-flight poll, then behind one.
    for (unsigned behindPoll = 0; behindPoll < 2; ++behindPoll) {
        Rig r;
        openAxis(r, r.a);
        L::ReadContext state;
        history(r, r.a, state);
        if (behindPoll) {
            const uint32_t pollOperation = begin(r, r.a);
            r.drive[0].answer = false;
            CHECK(r.a.admitPrepared(service(r, 1, pollOperation, 1, 0x2100), r.now) == O::BusAdmission::ACCEPTED);
            r.step();
        }
        const unsigned from = r.wire.sentCount;
        CHECK(r.a.requestStop(L::StopKind::HALT, state, stopPolicy(r.now), r.now) == E::StopPath::CONFIRMED);
        // An escalation with no best-effort branch is refused, and the normal stop carries on.
        const Saved<O::OwnerResources> resources(r.owner.resources());
        CHECK(r.a.requestStop(L::StopKind::QUICK_STOP, state, stopPolicy(r.now, 5000, 0), r.now) == E::StopPath::NONE);
        resources.unchanged(r.owner.resources());
        CHECK(r.a.stopPath() == E::StopPath::CONFIRMED && r.a.stopAction(L::StopKind::HALT).state == M::ActionState::ACTIVE);
        const E::StopPath path = r.a.requestStop(L::StopKind::QUICK_STOP, state, stopPolicy(r.now), r.now);
        // The retired HALT holds the urgent slot until the session's next turn reads it.
        CHECK(path == E::StopPath::HELD);
        r.step();
        CHECK(r.a.stopPath() == E::StopPath::BEST_EFFORT);
        const L::ActionContext& halt = r.a.stopAction(L::StopKind::HALT);
        CHECK(halt.state == M::ActionState::FAILED && halt.outcome == M::ActionOutcome::CANCELLED &&
              halt.execution == M::ActionExecution::NOT_TRANSMITTED);
        CHECK(r.a.stopAction(L::StopKind::QUICK_STOP).state == M::ActionState::EMPTY);
        if (behindPoll) {
            quiet(r, from);
            r.answer(from - 1);
        }
        r.step();
        CHECK(r.wire.sentCount == from + 1 && isWord(r, from, WORD_QUICK));
        finish(r, r.a);
        checkEnded(r.a.bestEffort(), M::ActionOutcome::STOP_SENT_UNCONFIRMED, M::ActionExecution::UNKNOWN);
        CHECK(writes6040(r, from) == 1);
    }
}
void escalationFromInFlightHalt() {
    for (unsigned lost = 0; lost < 2; ++lost) {
        Rig r;
        openAxis(r, r.a);
        L::ReadContext state;
        history(r, r.a, state);
        r.drive[0].answer = false;
        const unsigned from = r.wire.sentCount;
        CHECK(r.a.requestStop(L::StopKind::HALT, state, stopPolicy(r.now), r.now) == E::StopPath::CONFIRMED);
        r.step();
        CHECK(isWord(r, from, WORD_HALT));
        CHECK(r.a.requestStop(L::StopKind::QUICK_STOP, state, stopPolicy(r.now), r.now) == E::StopPath::HELD);
        r.steps(3);
        CHECK(r.wire.sentCount == from + 1); // 010F settles first.
        if (lost)
            r.turn(r.now + 100);
        else {
            r.answer(from);
            r.step();
        }
        const L::ActionContext& halt = r.a.stopAction(L::StopKind::HALT);
        CHECK(halt.state == M::ActionState::FAILED && (halt.commandEvidenceMask & 1u) &&
              halt.commandHistory[0].transfer.data[0] == 0x0f && halt.commandHistory[0].transfer.data[1] == 1 &&
              halt.commandHistory[0].transfer.operation == halt.operation);
        CHECK(halt.execution == (lost ? M::ActionExecution::UNKNOWN : M::ActionExecution::ACKNOWLEDGED));
        CHECK(halt.outcome == (lost ? M::ActionOutcome::SERVICE_TIMEOUT : M::ActionOutcome::CANCELLED));
        CHECK(r.a.stopAction(L::StopKind::QUICK_STOP).state == M::ActionState::EMPTY);
        CHECK(!r.a.latestControl()); // A superseded terminal is older intent, even a success.
        const Saved<L::ActionContext> old(halt);
        r.step();
        CHECK(r.wire.sentCount == from + 2 && isWord(r, from + 1, WORD_QUICK));
        finish(r, r.a);
        const L::BestEffortStop& stop = r.a.bestEffort();
        checkEnded(stop, M::ActionOutcome::STOP_SENT_UNCONFIRMED, M::ActionExecution::UNKNOWN);
        CHECK(stop.commandEvidence.transfer.data[0] == 0x0b && stop.operation != halt.operation);
        old.unchanged(halt);
        CHECK(writes6040(r, from) == 2);
    }
}
void escalationFromObservation() {
    // The running HALT's acknowledgement and latest complete cycle are the fresh evidence:
    // between cycles, and with a field of the next cycle in flight.
    for (unsigned inFlight = 0; inFlight < 2; ++inFlight) {
        Rig r;
        openAxis(r, r.a);
        L::ReadContext state;
        history(r, r.a, state);
        const unsigned from = r.wire.sentCount;
        CHECK(r.a.requestStop(L::StopKind::HALT, state, stopPolicy(r.now, 3000, 3100, 100, 100, 200), r.now) ==
              E::StopPath::CONFIRMED);
        r.steps(14);
        const L::ActionContext& halt = r.a.stopAction(L::StopKind::HALT);
        CHECK(halt.observations == 1 && halt.phase == L::ActionPhase::WAITING);
        CHECK(r.a.latestControl() && r.a.latestControl()->transfer.data[1] == 1);
        const Saved<C::SdoCompletion> command(halt.commandHistory[0]);
        unsigned field = 0;
        if (inFlight) {
            r.drive[0].answer = false;
            r.turn(halt.eligibleUs);
            r.step();
            field = r.wire.sentCount - 1;
            CHECK(r.wire.sent[field].data[0] == 0x40 && frameIndex(r.wire.sent[field]) == 0x2100);
        }
        // The state argument plays no part: nothing can read while the stop runs.
        L::ReadContext none;
        const E::StopPolicy second = stopPolicy(r.now, 2000, 0);
        CHECK(r.a.requestStop(L::StopKind::QUICK_STOP, none, second, r.now) == E::StopPath::CONFIRMED);
        const L::ActionContext& quick = r.a.stopAction(L::StopKind::QUICK_STOP);
        CHECK(quick.predecessorWord == WORD_HALT && !quick.predecessorDocumented &&
              quick.predecessorBasis == L::PredecessorBasis::PROJECT_DECISION);
        CHECK(quick.controlEvidence.transfer.data[0] == 0x0f && quick.controlEvidence.transfer.data[1] == 1);
        CHECK(quick.operation > halt.operation && quick.deadlineUs == second.deadlineUs);
        if (inFlight) {
            r.steps(2);
            CHECK(r.wire.sentCount == field + 1); // The field settles first, in its own context.
            CHECK(halt.state == M::ActionState::ACTIVE);
            r.answer(field);
            r.step();
            CHECK(halt.observationEvidence.transfer.index == 0x2100);
            r.drive[0].answer = true;
        }
        CHECK(halt.state == M::ActionState::FAILED && halt.outcome == M::ActionOutcome::CANCELLED &&
              halt.execution == M::ActionExecution::ACKNOWLEDGED);
        command.unchanged(halt.commandHistory[0]);
        r.drive[0].monitor = 0;
        finish(r, r.a);
        CHECK(quick.state == M::ActionState::SUCCEEDED && quick.commandHistory[0].transfer.data[0] == 0x0b);
        CHECK(quick.commandHistory[0].transfer.operation == quick.operation);
        CHECK(r.a.bestEffort().state == M::ActionState::EMPTY);
        command.unchanged(halt.commandHistory[0]);
        unsigned halts = 0, quicks = 0;
        for (unsigned i = from; i < r.wire.sentCount; ++i) {
            halts += isWord(r, i, WORD_HALT);
            quicks += isWord(r, i, WORD_QUICK);
        }
        CHECK(halts == 1 && quicks == 1); // Never a second 010F.
    }
    // With a stale cycle, or none yet after the acknowledgement: the best-effort branch.
    for (unsigned stale = 0; stale < 2; ++stale) {
        Rig r;
        openAxis(r, r.a);
        L::ReadContext state;
        history(r, r.a, state);
        const unsigned from = r.wire.sentCount;
        const E::StopPolicy first = stopPolicy(r.now, 3000, 3100, 100, 100, 2000);
        CHECK(r.a.requestStop(L::StopKind::HALT, state, first, r.now) == E::StopPath::CONFIRMED);
        const L::ActionContext& halt = r.a.stopAction(L::StopKind::HALT);
        unsigned field = 0;
        if (stale) {
            r.steps(14);
            CHECK(halt.observations == 1);
            r.turn(r.now + first.options.maximumAgeUs + 1);
        } else {
            r.steps(2);
            CHECK(halt.observations == 0 && halt.execution == M::ActionExecution::ACKNOWLEDGED);
            r.drive[0].answer = false;
            r.step();
            field = r.wire.sentCount - 1;
            CHECK(r.wire.sent[field].data[0] == 0x40 && frameIndex(r.wire.sent[field]) == 0x2100);
        }
        const Saved<C::SdoCompletion> command(halt.commandHistory[0]);
        // Without a best-effort branch that evidence gap refuses it, and HALT carries on.
        const Saved<O::OwnerResources> resources(r.owner.resources());
        CHECK(r.a.requestStop(L::StopKind::QUICK_STOP, state, stopPolicy(r.now, 900, 0), r.now) == E::StopPath::NONE);
        resources.unchanged(r.owner.resources());
        CHECK(halt.state == M::ActionState::ACTIVE && r.a.stopPath() == E::StopPath::CONFIRMED);
        const E::StopPath path = r.a.requestStop(L::StopKind::QUICK_STOP, state, stopPolicy(r.now, 900, 1000), r.now);
        CHECK(path == (stale ? E::StopPath::BEST_EFFORT : E::StopPath::HELD));
        if (!stale) {
            r.steps(2);
            CHECK(r.wire.sentCount == field + 1 && halt.state == M::ActionState::ACTIVE);
            r.answer(field);
            r.step();
            CHECK(halt.observationEvidencePresent && halt.observationEvidence.transfer.index == 0x2100);
        }
        CHECK(halt.state == M::ActionState::FAILED && halt.outcome == M::ActionOutcome::CANCELLED &&
              halt.execution == M::ActionExecution::ACKNOWLEDGED);
        command.unchanged(halt.commandHistory[0]);
        CHECK(r.a.stopAction(L::StopKind::QUICK_STOP).state == M::ActionState::EMPTY);
        finish(r, r.a);
        CHECK(writes6040(r, from) == 2 && isWord(r, from, WORD_HALT));
        checkEnded(r.a.bestEffort(), M::ActionOutcome::STOP_SENT_UNCONFIRMED, M::ActionExecution::UNKNOWN);
        CHECK(r.a.bestEffort().commandEvidence.transfer.data[0] == 0x0b);
    }
}
// Core prepareEscalation, on a real running HALT's context.
void prepareEscalationUnits() {
    Rig r;
    openAxis(r, r.a);
    L::ReadContext state;
    history(r, r.a, state);
    const E::StopPolicy policy = stopPolicy(r.now, 3000, 3100, 100, 100, 2000);
    CHECK(r.a.requestStop(L::StopKind::HALT, state, policy, r.now) == E::StopPath::CONFIRMED);
    const L::ActionContext& halt = r.a.stopAction(L::StopKind::HALT);
    r.steps(14);
    CHECK(halt.observations == 1 && halt.observationKnown);
    const L::ActionOptions options = policy.options;
    uint64_t oldest = UINT64_MAX;
    for (unsigned i = 0; i < halt.monitorObservation.fieldCount; ++i)
        if (halt.monitorObservation.fields[i].earliestUs < oldest)
            oldest = halt.monitorObservation.fields[i].earliestUs;
    const uint64_t limit = oldest + options.maximumAgeUs;
    L::ActionContext output;
    CHECK(L::prepareEscalation(output, halt, 77, limit, limit + 100, options));
    CHECK(output.kind == L::StopKind::QUICK_STOP && output.state == M::ActionState::ACTIVE &&
          output.operation == 77 && output.predecessorWord == WORD_HALT && !output.predecessorDocumented &&
          output.predecessorBasis == L::PredecessorBasis::PROJECT_DECISION &&
          output.execution == M::ActionExecution::NOT_TRANSMITTED && output.deadlineUs == limit + 100);
    L::PreparedAction work;
    CHECK(L::nextAction(output, halt.reservation, halt.reservation.target, limit, work) && work.ready);
    CHECK(work.transfer.data[0] == 0x0b && work.transfer.data[1] == 0);
    {
        // A later read withdrawn after correlation loss takes the earlier cycle with it.
        L::ActionContext withdrawn = halt;
        withdrawn.read.invalidated = true;
        L::ActionContext refused;
        CHECK(!L::prepareEscalation(refused, withdrawn, 77, limit, limit + 100, options));
        CHECK(refused.state == M::ActionState::EMPTY);
    }
    for (unsigned wrong = 0; wrong < 26; ++wrong) {
        L::ActionContext running = halt;
        L::ActionOptions budget = options;
        uint32_t operation = 77;
        uint64_t now = limit, deadline = limit + 100;
        switch (wrong) {
        case 0:
            now = limit + 1; // Stale by one microsecond.
            deadline = limit + 101;
            break;
        case 1:
            running.commandEvidenceMask = 0;
            break;
        case 2:
            running.execution = M::ActionExecution::UNKNOWN;
            break;
        case 3:
            running.execution = M::ActionExecution::NOT_TRANSMITTED;
            break;
        case 4:
            running.observationKnown = false;
            break;
        case 5:
            running.kind = L::StopKind::QUICK_STOP;
            break;
        case 6:
            running.actionKind = M::ActionKind::CLEAR_ALARM;
            break;
        case 7:
            running.commandHistory[0].transfer.data[1] = 0; // 000F, not the HALT's word.
            break;
        case 8:
            running.commandHistory[0].transfer.data[0] = 0x0b;
            running.commandHistory[0].transfer.data[1] = 0;
            break;
        case 9:
            running.commandHistory[0].transfer.data[0] = 0x1f;
            running.commandHistory[0].transfer.data[1] = 1;
            break;
        case 10:
            running.commandHistory[0].protocolAcknowledged = false;
            break;
        case 11:
            running.commandHistory[0].outcome = C::SdoOutcome::TIMEOUT;
            break;
        case 12:
            running.commandHistory[0].completedUs = oldest + 1; // The cycle began before the ACK.
            break;
        case 13:
            running.statusObservation.modeRaw = 3;
            break;
        case 14:
            running.statusObservation.statusword = 3;
            break;
        case 15:
            running.monitorObservation.monitor = 8;
            break;
        case 16:
            running.monitorObservation.monitor = 16;
            break;
        case 17:
            running.statusObservation.fieldCount = 0;
            break;
        case 18:
            running.monitorObservation.fieldCount = 0;
            break;
        case 19:
            running.reservation.model = L::SettingsModel::UNKNOWN;
            break;
        case 20:
            operation = 0;
            break;
        case 21:
            deadline = now;
            break;
        case 22:
            budget.observationIntervalUs = 0;
            break;
        case 23:
            budget.maximumAgeUs = 999;
            break;
        case 24:
            running.commandHistory[0].nodeGeneration = 99;
            break;
        case 25:
            running.statusObservation.fields[0].earliestUs = running.commandHistory[0].completedUs - 1;
            now = limit - 50; // Fresh, so only its order against the acknowledgement can reject it.
            deadline = limit;
            break;
        }
        L::ActionContext rejected;
        rejected.operation = 555;
        const Saved<L::ActionContext> unchanged(rejected);
        CHECK(!L::prepareEscalation(rejected, running, operation, now, deadline, budget));
        unchanged.unchanged(rejected);
    }
    L::ActionContext copy = halt;
    const Saved<L::ActionContext> before(copy);
    CHECK(!L::prepareEscalation(copy, copy, 77, limit, limit + 100, options));
    before.unchanged(copy);
    CHECK(!L::prepareEscalation(copy, halt, 77, limit, limit + 100, copy.options));
    before.unchanged(copy);
    // The EMPTY context of a stop that never ran.
    L::ActionContext rejected;
    const L::ActionContext never;
    CHECK(!L::prepareEscalation(rejected, never, 77, limit, limit + 100, options));
}
void refusedEscalationsChangeNothing() {
    // Sequenced QUICK: HALT and QUICK are both refused.
    {
        Rig r;
        openAxis(r, r.a);
        L::ReadContext state;
        history(r, r.a, state);
        CHECK(r.a.requestStop(L::StopKind::QUICK_STOP, state, stopPolicy(r.now), r.now) == E::StopPath::CONFIRMED);
        for (unsigned i = 0; i < 2; ++i) {
            const Saved<E::AxisSession> session(r.a);
            const Saved<O::OwnerResources> resources(r.owner.resources());
            const unsigned sent = r.wire.sentCount;
            CHECK(r.a.requestStop(i ? L::StopKind::QUICK_STOP : L::StopKind::HALT, state, stopPolicy(r.now), r.now) ==
                  E::StopPath::NONE);
            session.unchanged(r.a);
            resources.unchanged(r.owner.resources());
            CHECK(r.wire.sentCount == sent);
        }
        CHECK(r.a.stopAction(L::StopKind::HALT).state == M::ActionState::EMPTY);
    }
    // HALT on HALT.
    {
        Rig r;
        openAxis(r, r.a);
        L::ReadContext state;
        history(r, r.a, state);
        CHECK(r.a.requestStop(L::StopKind::HALT, state, stopPolicy(r.now), r.now) == E::StopPath::CONFIRMED);
        const Saved<E::AxisSession> session(r.a);
        CHECK(r.a.requestStop(L::StopKind::HALT, state, stopPolicy(r.now), r.now) == E::StopPath::NONE);
        session.unchanged(r.a);
    }
    // Nothing outranks the best-effort branch, admitted or held.
    for (unsigned held = 0; held < 2; ++held)
        for (unsigned kind = 0; kind < 2; ++kind) {
            Rig r;
            openAxis(r, r.a);
            L::ReadContext state;
            if (held) {
                history(r, r.a, state);
                CHECK(r.a.requestStop(L::StopKind::HALT, state, stopPolicy(r.now), r.now) == E::StopPath::CONFIRMED);
                CHECK(r.owner.reportController(C::ControllerState::BUS_OFF, r.now));
                r.step();
                CHECK(r.a.stopPath() == E::StopPath::HELD);
            } else {
                CHECK(r.a.requestStop(L::StopKind::HALT, state, stopPolicy(r.now), r.now) == E::StopPath::BEST_EFFORT);
            }
            const Saved<E::AxisSession> session(r.a);
            const Saved<O::OwnerResources> resources(r.owner.resources());
            CHECK(r.a.requestStop(kind ? L::StopKind::QUICK_STOP : L::StopKind::HALT, state, stopPolicy(r.now), r.now) ==
                  E::StopPath::NONE);
            session.unchanged(r.a);
            resources.unchanged(r.owner.resources());
        }
    // A finished stop is history: the DEC-8 table decides. 010F allows only 000B, and 000B nothing.
    {
        Rig r;
        openAxis(r, r.a);
        L::ReadContext state;
        history(r, r.a, state);
        r.drive[0].monitor = 0;
        CHECK(r.a.requestStop(L::StopKind::HALT, state, stopPolicy(r.now), r.now) == E::StopPath::CONFIRMED);
        finish(r, r.a);
        CHECK(r.a.stopAction(L::StopKind::HALT).state == M::ActionState::SUCCEEDED);
        readState(r, r.a, state);
        CHECK(r.a.requestStop(L::StopKind::HALT, state, stopPolicy(r.now), r.now) == E::StopPath::NONE);
        const unsigned from = r.wire.sentCount;
        CHECK(r.a.requestStop(L::StopKind::QUICK_STOP, state, stopPolicy(r.now), r.now) == E::StopPath::CONFIRMED);
        finish(r, r.a);
        CHECK(isWord(r, from, WORD_QUICK) && r.a.latestControl()->transfer.data[0] == 0x0b);
        readState(r, r.a, state);
        for (unsigned kind = 0; kind < 2; ++kind)
            CHECK(r.a.requestStop(kind ? L::StopKind::QUICK_STOP : L::StopKind::HALT, state, stopPolicy(r.now), r.now) ==
                  E::StopPath::NONE);
    }
}

// 9. Stopped and released.
void stoppedAndReleasedRefuse() {
    const uint16_t refusedWords[] = {WORD_QUICK, 0x0006, 0x0007, 0x0000};
    for (uint16_t word : refusedWords)
        for (unsigned kind = 0; kind < 2; ++kind) {
            Rig r;
            openAxis(r, r.a);
            L::ReadContext state;
            history(r, r.a, state, word);
            const Saved<E::AxisSession> session(r.a);
            const Saved<O::OwnerResources> resources(r.owner.resources());
            const uint32_t rejected = r.owner.stats().queueRejected;
            const unsigned sent = r.wire.sentCount;
            CHECK(r.a.requestStop(kind ? L::StopKind::QUICK_STOP : L::StopKind::HALT, state, stopPolicy(r.now), r.now) ==
                  E::StopPath::NONE);
            session.unchanged(r.a);
            resources.unchanged(r.owner.resources());
            CHECK(r.owner.stats().queueRejected == rejected);
            quiet(r, sent);
        }
    // A prior 010F refuses HALT, and allows QUICK_STOP.
    {
        Rig r;
        openAxis(r, r.a);
        L::ReadContext state;
        history(r, r.a, state, WORD_HALT);
        CHECK(r.a.requestStop(L::StopKind::HALT, state, stopPolicy(r.now), r.now) == E::StopPath::NONE);
        CHECK(r.a.requestStop(L::StopKind::QUICK_STOP, state, stopPolicy(r.now), r.now) == E::StopPath::CONFIRMED);
    }
    // A fresh released or alarm report from an idle drive refuses, with or without history,
    // whichever branch is open.
    const uint16_t reports[] = {0x0010, 0x0008, 0x0018};
    for (uint16_t report : reports)
        for (unsigned withHistory = 0; withHistory < 2; ++withHistory)
            for (unsigned allowed = 0; allowed < 2; ++allowed) {
                Rig r;
                openAxis(r, r.a);
                L::ReadContext state;
                r.drive[0].monitor = report;
                if (withHistory)
                    history(r, r.a, state);
                else
                    readState(r, r.a, state);
                const Saved<E::AxisSession> session(r.a);
                const Saved<O::OwnerResources> resources(r.owner.resources());
                const unsigned sent = r.wire.sentCount;
                CHECK(r.a.requestStop(L::StopKind::QUICK_STOP, state, stopPolicy(r.now, 5000, allowed ? 6000 : 0), r.now) ==
                      E::StopPath::NONE);
                session.unchanged(r.a);
                resources.unchanged(r.owner.resources());
                quiet(r, sent);
                // An expired report says nothing, so the 000B branch opens again.
                if (allowed) {
                    r.turn(r.now + 2000);
                    CHECK(r.a.requestStop(L::StopKind::QUICK_STOP, state, stopPolicy(r.now), r.now) ==
                          E::StopPath::BEST_EFFORT);
                }
            }
}
void zeroBestEffortRefusalKeepsQueuedWork() {
    // Missing history with no branch: refused, and work queued on the axis still goes out.
    for (unsigned queuedDownload = 0; queuedDownload < 2; ++queuedDownload) {
        Rig r;
        openAxis(r, r.a);
        L::ReadContext state;
        const uint32_t operation = begin(r, r.a);
        CHECK(r.a.admitPrepared(service(r, 1, operation, 1, queuedDownload ? 0x607a : 0x2100, queuedDownload != 0), r.now) ==
              O::BusAdmission::ACCEPTED);
        const unsigned sent = r.wire.sentCount;
        const Saved<O::OwnerResources> resources(r.owner.resources());
        const Saved<O::Sequence> sequence(*r.owner.sequence(r.a.sequence()));
        const Saved<E::AxisSession> session(r.a);
        CHECK(r.a.requestStop(L::StopKind::HALT, state, stopPolicy(r.now, 5000, 0), r.now) == E::StopPath::NONE);
        session.unchanged(r.a);
        resources.unchanged(r.owner.resources());
        sequence.unchanged(*r.owner.sequence(r.a.sequence()));
        r.steps(2);
        CHECK(r.wire.sentCount == sent + 1);
        const C::SdoCompletion result = settled(r.a);
        CHECK(result.outcome == C::SdoOutcome::SUCCESS && result.transfer.operation == operation);
    }
    // Also on an unresolved route.
    Rig r;
    openAxis(r, r.a);
    L::ReadContext state;
    const uint32_t operation = begin(r, r.a);
    r.drive[0].answer = false;
    CHECK(r.a.admitPrepared(service(r, 1, operation, 1, 0x2100), r.now) == O::BusAdmission::ACCEPTED);
    r.step();
    r.turn(r.now + 100);
    CHECK(r.services.unresolved(0));
    const unsigned sent = r.wire.sentCount;
    CHECK(r.a.requestStop(L::StopKind::HALT, state, stopPolicy(r.now, 5000, 0), r.now) == E::StopPath::NONE);
    CHECK(r.a.stopPath() == E::StopPath::NONE && r.a.bestEffort().state == M::ActionState::EMPTY);
    quiet(r, sent);
}

// 10. ProbeApp.
struct Console {
    static bool clock(void*, uint64_t& output) {
        output = 0;
        return true;
    }
    static bool read(void*, uint8_t&) { return false; }
    static bool write(void*, uint8_t) { return true; }
    static E::ProbeCallbacks callbacks() {
        E::ProbeCallbacks value;
        value.nowUs = clock;
        value.readByte = read;
        value.writeByte = write;
        return value;
    }
};
struct AppRig : Rig {
    E::ProbePlatform platform;
    E::ProbeApp probe;
    AppRig() : platform(Console::callbacks()), probe(owner, platform) { app = &probe; }
    void completeRead() {
        for (unsigned i = 0; i < 40 && probe.readContext().state == M::ReadState::ACTIVE; ++i) {
            step();
            if (owner.result(probe.pendingRequest()))
                CHECK(probe.advanceRead(now));
        }
        CHECK(probe.readContext().state != M::ReadState::ACTIVE);
    }
};
void probeReservationPassesToSession() {
    AppRig r;
    CHECK(r.probe.select(target()));
    CHECK(r.probe.requestState(r.now, r.now + 2000, reads(), STATE_MASK));
    r.steps(2);
    CHECK(r.probe.readContext().state == M::ReadState::ACTIVE);
    E::AxisSession* session = r.probe.openAxis(r.a, r.current(), L::SettingsModel::CL86_C, r.now);
    CHECK(session && session->isOpen() && r.owner.sequence(session->sequence()));
    CHECK(!r.probe.openAxis(r.a, r.current(), L::SettingsModel::CL86_C, r.now));
    CHECK(r.probe.axis(target().channel.binding) == session);
    r.completeRead();
    CHECK(r.probe.readContext().state == M::ReadState::SUCCEEDED);
    // releaseRead works, and the reservation stays with the session.
    const O::SequenceId held = session->sequence();
    CHECK(r.probe.releaseRead());
    CHECK(r.probe.readContext().state == M::ReadState::EMPTY && r.owner.sequence(held));
    // A later read uses the session's reservation, and its own release leaves it held.
    CHECK(r.probe.requestState(r.now, r.now + 2000, reads(), STATE_MASK));
    r.completeRead();
    CHECK(r.probe.readContext().state == M::ReadState::SUCCEEDED);
    CHECK(r.probe.releaseRead() && r.owner.sequence(held));
    O::SequenceId found;
    CHECK(r.owner.findSequence(target().channel.binding, found) && found.generation == held.generation);
    CHECK(session->close());
    CHECK(!r.owner.sequence(held));
    CHECK(!r.probe.axis(target().channel.binding));
    O::SequenceId fresh;
    CHECK(r.owner.beginSequence(target().channel.binding, 2, r.now, fresh));
}
void stopInterruptsProbeRead() {
    // A field on the wire settles, and the read then ends INVALIDATED. A field still queued
    // is retired with an explicit no-TX result. Either way the stop goes out once.
    for (unsigned onWire = 0; onWire < 2; ++onWire) {
        AppRig r;
        CHECK(r.probe.select(target()));
        E::AxisSession* session = r.probe.openAxis(r.a, r.current(), L::SettingsModel::CL86_C, r.now);
        CHECK(session);
        write(r, *session, 0x6040, WORD_ENABLE);
        CHECK(r.probe.requestState(r.now, r.now + 2000, reads(), STATE_MASK));
        r.completeRead();
        const L::ReadContext state = r.probe.readContext();
        CHECK(state.state == M::ReadState::SUCCEEDED && r.probe.releaseRead());
        // The next read.
        CHECK(r.probe.requestState(r.now, r.now + 2000, reads(), STATE_MASK));
        r.drive[0].answer = false;
        r.step(); // The first field is admitted, not yet sent.
        unsigned field = r.wire.sentCount;
        if (onWire) {
            r.step();
            CHECK(r.wire.sentCount == field + 1 && frameIndex(r.wire.sent[field]) == 0x2100);
        }
        const unsigned from = r.wire.sentCount;
        CHECK(session->requestStop(L::StopKind::QUICK_STOP, state, stopPolicy(r.now), r.now) == E::StopPath::CONFIRMED);
        CHECK(session->stopActive());
        CHECK(!r.probe.requestIdentity(r.now, r.now + 1000, reads())); // The read is still retained.
        if (onWire) {
            r.steps(2);
            CHECK(r.wire.sentCount == from); // The field in flight settles first.
            r.answer(field);
        }
        r.step();
        CHECK(r.owner.result(r.probe.pendingRequest()));
        CHECK(r.probe.advanceRead(r.now));
        r.step();
        CHECK(r.probe.readContext().state == M::ReadState::FAILED);
        CHECK(r.probe.readContext().outcome ==
              (onWire ? M::ReadOutcome::INVALIDATED : M::ReadOutcome::CANCELLED));
        CHECK(r.wire.sentCount >= from + 1 && isWord(r, from, WORD_QUICK));
        CHECK(writes6040(r, from) == 1);
        // Reads are refused while the stop is active, and allowed after it.
        CHECK(r.probe.releaseRead());
        CHECK(session->stopActive());
        CHECK(!r.probe.requestState(r.now, r.now + 1000, reads(), STATE_MASK));
        CHECK(r.probe.readContext().state == M::ReadState::EMPTY);
        r.drive[0].answer = true;
        r.drive[0].monitor = 0;
        r.answer(from);
        finish(r, *session);
        CHECK(session->stopAction(L::StopKind::QUICK_STOP).state == M::ActionState::SUCCEEDED);
        CHECK(r.probe.requestState(r.now, r.now + 1000, reads(), STATE_MASK));
    }
}
void stopFromAnotherProducerAdopts() {
    Rig r;
    O::SequenceId held;
    CHECK(r.owner.beginSequence(target().channel.binding, 3, r.now, held));
    // It adopts the reservation another producer holds, and does not take ownership of it.
    openAxis(r, r.a, 1, 2);
    CHECK(r.a.sequence().generation == held.generation && r.a.sequence().slot == held.slot);
    L::ReadContext state;
    history(r, r.a, state);
    const unsigned from = r.wire.sentCount;
    CHECK(r.a.requestStop(L::StopKind::QUICK_STOP, state, stopPolicy(r.now), r.now) == E::StopPath::CONFIRMED);
    r.drive[0].monitor = 0;
    finish(r, r.a);
    CHECK(r.a.stopAction(L::StopKind::QUICK_STOP).state == M::ActionState::SUCCEEDED && isWord(r, from, WORD_QUICK));
    CHECK(r.a.stopAction(L::StopKind::QUICK_STOP).reservation.generation == held.generation);
    CHECK(r.a.close());
    CHECK(r.owner.sequence(held)); // Close released only what this session took.
    openAxis(r, r.a, 1, 2);
    const uint32_t adoptedOperation = begin(r, r.a);
    CHECK(r.a.admitPrepared(service(r, 1, adoptedOperation, 1, 0x2100), r.now) == O::BusAdmission::ACCEPTED);
    CHECK(r.a.busy() && !r.a.close());
    r.steps(2);
    CHECK(r.a.busy() && !r.a.close()); // Its terminal is unread.
    CHECK(settled(r.a).outcome == C::SdoOutcome::SUCCESS && r.a.close());
    openAxis(r, r.a, 1, 2);
    CHECK(r.a.requestStop(L::StopKind::QUICK_STOP, state, stopPolicy(r.now), r.now) != E::StopPath::NONE);
    CHECK(r.a.stopActive() && !r.a.close());
    finish(r, r.a);
    CHECK(r.a.close());
    CHECK(r.services.unresolved(0)); // That best-effort stop ended its route, as it must.
    // adoptReservation does take it over.
    Rig other;
    CHECK(other.owner.beginSequence(target().channel.binding, 3, other.now, held));
    openAxis(other, other.a, 1, 2);
    other.a.adoptReservation();
    CHECK(other.a.close() && !other.owner.sequence(held));
}
void noReservationStillSendsBestEffort() {
    // An ordinary session that lost its route, then one opened fresh on that route.
    for (unsigned useApp = 0; useApp < 2; ++useApp) {
        AppRig r;
        E::AxisSession* first = r.probe.openAxis(r.a, r.current(), L::SettingsModel::CL86_C, r.now);
        CHECK(first);
        const uint32_t operation = begin(r, *first);
        r.drive[0].answer = false;
        CHECK(first->admitPrepared(service(r, 1, operation, 1, 0x2100), r.now) == O::BusAdmission::ACCEPTED);
        r.step();
        r.turn(r.now + 100);
        CHECK(r.services.unresolved(0));
        CHECK(settled(*first).outcome == C::SdoOutcome::TIMEOUT);
        CHECK(first->close());
        E::AxisSession* session = useApp ? r.probe.openAxis(r.a, r.current(), L::SettingsModel::CL86_C, r.now) : &r.a;
        if (!useApp)
            openAxis(r, r.a);
        CHECK(session && session->isOpen() && !session->sequence().owner);
        CHECK(!session->beginOperation(r.now + 100, r.now));
        C::SdoTransfer refused = service(r, 1, 1, 1, 0x2100);
        refused.deadlineUs = r.now + 100;
        CHECK(session->admitPrepared(refused, r.now) == O::BusAdmission::INVALID);
        const unsigned from = r.wire.sentCount;
        L::ReadContext none;
        CHECK(session->requestStop(L::StopKind::HALT, none, stopPolicy(r.now), r.now) == E::StopPath::BEST_EFFORT);
        for (unsigned i = 0; i < 4; ++i) {
            r.step();
            session->service(r.now);
        }
        CHECK(r.wire.sentCount == from + 1 && isWord(r, from, WORD_QUICK));
        checkEnded(session->bestEffort(), M::ActionOutcome::STOP_SENT_UNCONFIRMED, M::ActionExecution::UNKNOWN);
        CHECK(r.services.unresolved(0) && session->close());
    }
}

// 11. Core BestEffortStop units, with no owner.
C::SdoCompletion stopTerminal(const L::BestEffortStop& stop, C::SdoOutcome outcome, bool possible, uint64_t at) {
    C::SdoCompletion value;
    L::PreparedAction work;
    L::BestEffortStop open = stop;
    open.admitted = false;
    CHECK(L::nextBestEffortStop(open, stop.target, open.servicedUs, work) && work.ready);
    value.transfer = work.transfer;
    value.outcome = outcome;
    value.admittedUs = stop.startedUs;
    value.completedUs = at;
    value.nodeGeneration = stop.target.nodeGeneration;
    value.queued = possible;
    value.transmissionPossible = value.effectPossible = possible;
    value.transmissionConfirmed = outcome == C::SdoOutcome::UNCONFIRMED_SENT;
    value.effectUncertain = possible;
    return value;
}
void coreBestEffortUnits() {
    const L::ReadTarget t = target();
    // Preparation.
    {
        L::BestEffortStop stop;
        stop.operation = 777;
        const Saved<L::BestEffortStop> before(stop);
        CHECK(!L::prepareBestEffortStop(stop, t, 0, 10, 20, reads()));
        CHECK(!L::prepareBestEffortStop(stop, t, 1, 10, 10, reads()));
        CHECK(!L::prepareBestEffortStop(stop, t, 1, 10, 9, reads()));
        CHECK(!L::prepareBestEffortStop(stop, t, 1, 10, 20, reads(0, 100)));
        CHECK(!L::prepareBestEffortStop(stop, t, 1, 10, 20, reads(100, 0)));
        L::ReadTarget bad = t;
        bad.nodeGeneration = 0;
        CHECK(!L::prepareBestEffortStop(stop, bad, 1, 10, 20, reads()));
        bad = t;
        bad.channel.clientToServerId = 0;
        CHECK(!L::prepareBestEffortStop(stop, bad, 1, 10, 20, reads()));
        CHECK(!L::prepareBestEffortStop(stop, stop.target, 1, 10, 20, reads()));
        CHECK(!L::prepareBestEffortStop(stop, t, 1, 10, 20, stop.service));
        before.unchanged(stop);
        CHECK(L::prepareBestEffortStop(stop, t, 5, 10, 20, reads()));
        CHECK(stop.state == M::ActionState::ACTIVE && stop.startedUs == 10 && stop.servicedUs == 10 &&
              stop.deadlineUs == 20 && stop.operation == 5 && !stop.admitted && !stop.commandEvidencePresent &&
              stop.execution == M::ActionExecution::NOT_TRANSMITTED && stop.outcome == M::ActionOutcome::NONE);
    }
    // The yielded bytes, and every time and target rejection.
    {
        L::BestEffortStop stop;
        CHECK(L::prepareBestEffortStop(stop, t, 5, 10, 20, reads()));
        L::PreparedAction work;
        CHECK(L::nextBestEffortStop(stop, t, 10, work) && work.ready);
        CHECK(work.transfer.index == 0x6040 && work.transfer.subindex == 0 &&
              work.transfer.direction == C::SdoDirection::DOWNLOAD && work.transfer.size == 2 &&
              work.transfer.data[0] == 0x0b && work.transfer.data[1] == 0 && work.transfer.deadlineUs == 20 &&
              work.transfer.operation == 5 && work.transfer.step == 1);
        L::PreparedAction unchanged;
        unchanged.eligibleUs = 99;
        const Saved<L::PreparedAction> output(unchanged);
        CHECK(!L::nextBestEffortStop(stop, t, 9, unchanged));
        CHECK(!L::nextBestEffortStop(stop, t, 20, unchanged));
        L::ReadTarget moved = t;
        ++moved.nodeGeneration;
        CHECK(!L::nextBestEffortStop(stop, moved, 11, unchanged));
        moved = t;
        ++moved.channel.binding.configurationGeneration;
        CHECK(!L::nextBestEffortStop(stop, moved, 11, unchanged));
        output.unchanged(unchanged);
        const Saved<L::BestEffortStop> before(stop);
        CHECK(!L::noteBestEffortAdmission(stop, 9));
        CHECK(!L::noteBestEffortAdmission(stop, 20));
        before.unchanged(stop);
        CHECK(L::noteBestEffortAdmission(stop, 11));
        CHECK(stop.admitted && stop.servicedUs == 11 && stop.state == M::ActionState::ACTIVE);
        CHECK(!L::noteBestEffortAdmission(stop, 12));
        CHECK(L::nextBestEffortStop(stop, t, 12, work) && !work.ready); // Admitted work is never yielded again.
        // Withdrawal is refused once admitted, for every reason.
        const M::ActionOutcome reasons[] = {M::ActionOutcome::CANCELLED, M::ActionOutcome::DEADLINE,
                                            M::ActionOutcome::INVALIDATED, M::ActionOutcome::TRANSPORT_ERROR};
        const Saved<L::BestEffortStop> admitted(stop);
        for (M::ActionOutcome reason : reasons)
            CHECK(!L::advanceBestEffortStop(stop, reason, 25));
        admitted.unchanged(stop);
    }
    // Rejected terminals, all atomic.
    for (unsigned wrong = 0; wrong < 22; ++wrong) {
        L::BestEffortStop stop;
        CHECK(L::prepareBestEffortStop(stop, t, 5, 10, 20, reads()));
        CHECK(L::noteBestEffortAdmission(stop, 11));
        C::SdoCompletion terminal = stopTerminal(stop, C::SdoOutcome::UNCONFIRMED_SENT, true, 12);
        uint64_t now = 13;
        switch (wrong) {
        case 0:
            terminal.outcome = C::SdoOutcome::SUCCESS;
            break;
        case 1:
            terminal.outcome = C::SdoOutcome::SERVER_ABORT;
            terminal.abortValid = false;
            terminal.transmissionConfirmed = false;
            break;
        case 2:
            terminal.protocolAcknowledged = true;
            break;
        case 3:
            terminal.transfer.data[0] = 0x0f;
            break;
        case 4:
            terminal.transfer.data[1] = 1;
            break;
        case 5:
            ++terminal.transfer.operation;
            break;
        case 6:
            terminal.transfer.index = 0x6041;
            break;
        case 7:
            terminal.transfer.subindex = 1;
            break;
        case 8:
            terminal.transfer.size = 1;
            break;
        case 9:
            terminal.transfer.direction = C::SdoDirection::UPLOAD;
            break;
        case 10:
            ++terminal.transfer.deadlineUs;
            break;
        case 11:
            terminal.transfer.channel.clientToServerId = 0x602;
            break;
        case 12:
            terminal.transfer.channel.serverToClientId = 0x582;
            break;
        case 13:
            terminal.transfer.channel.binding.logicalAxisId = 102;
            break;
        case 14:
            terminal.admittedUs = 9;
            break;
        case 15:
            terminal.admittedUs = 13; // After its own completion.
            break;
        case 16:
            now = 11;
            break;
        case 17:
            terminal.size = 2;
            break;
        case 18:
            terminal.abortValid = true;
            break;
        case 19:
            terminal.transmissionConfirmed = false; // UNCONFIRMED_SENT says the controller reported it.
            break;
        case 20:
            terminal.effectPossible = false;
            break;
        case 21:
            terminal.outcome = static_cast<C::SdoOutcome>(200);
            break;
        }
        const Saved<L::BestEffortStop> before(stop);
        CHECK(!L::advanceBestEffortStop(stop, terminal, now));
        before.unchanged(stop);
    }
    {
        L::BestEffortStop stop;
        CHECK(L::prepareBestEffortStop(stop, t, 5, 10, 20, reads()));
        const Saved<L::BestEffortStop> before(stop);
        // Nothing can settle a stop that was not admitted, and overlap is refused.
        CHECK(!L::advanceBestEffortStop(stop, stopTerminal(stop, C::SdoOutcome::UNCONFIRMED_SENT, true, 11), 12));
        before.unchanged(stop);
        CHECK(L::noteBestEffortAdmission(stop, 11));
        const Saved<L::BestEffortStop> admitted(stop);
        CHECK(!L::advanceBestEffortStop(stop, stop.commandEvidence, 12));
        admitted.unchanged(stop);
        // Confirmed without possible.
        C::SdoCompletion terminal = stopTerminal(stop, C::SdoOutcome::TIMEOUT, false, 12);
        terminal.transmissionConfirmed = true;
        CHECK(!L::advanceBestEffortStop(stop, terminal, 13));
        admitted.unchanged(stop);
    }
    // Terminal mapping for every remaining outcome.
    struct Case {
        C::SdoOutcome input;
        M::ActionOutcome output;
    };
    const Case cases[] = {{C::SdoOutcome::CLIENT_ABORT, M::ActionOutcome::REPLY_ERROR},
                          {C::SdoOutcome::TIMEOUT, M::ActionOutcome::SERVICE_TIMEOUT},
                          {C::SdoOutcome::CANCELLED, M::ActionOutcome::CANCELLED},
                          {C::SdoOutcome::BUS_OFF, M::ActionOutcome::TRANSPORT_ERROR},
                          {C::SdoOutcome::REBOOT, M::ActionOutcome::INVALIDATED},
                          {C::SdoOutcome::BINDING_INVALIDATED, M::ActionOutcome::INVALIDATED},
                          {C::SdoOutcome::CORRELATION_LOST, M::ActionOutcome::TRANSPORT_ERROR},
                          {C::SdoOutcome::TRANSPORT_ERROR, M::ActionOutcome::TRANSPORT_ERROR},
                          {C::SdoOutcome::UNCONFIRMED_SENT, M::ActionOutcome::STOP_SENT_UNCONFIRMED}};
    // A timeout at the cutoff is the deadline, while the table's earlier one is a spent budget.
    for (unsigned possible = 0; possible < 2; ++possible) {
        L::BestEffortStop stop;
        CHECK(L::prepareBestEffortStop(stop, t, 5, 10, 20, reads()));
        CHECK(L::noteBestEffortAdmission(stop, 11));
        CHECK(L::advanceBestEffortStop(stop, stopTerminal(stop, C::SdoOutcome::TIMEOUT, possible != 0, 20), 20));
        checkEnded(stop, M::ActionOutcome::DEADLINE,
                   possible ? M::ActionExecution::UNKNOWN : M::ActionExecution::NOT_TRANSMITTED);
    }
    for (const Case& item : cases)
        for (unsigned possible = 0; possible < 2; ++possible) {
            if (item.input == C::SdoOutcome::UNCONFIRMED_SENT && !possible)
                continue;
            L::BestEffortStop stop;
            CHECK(L::prepareBestEffortStop(stop, t, 5, 10, 20, reads()));
            CHECK(L::noteBestEffortAdmission(stop, 11));
            const C::SdoCompletion terminal = stopTerminal(stop, item.input, possible != 0, 12);
            CHECK(L::advanceBestEffortStop(stop, terminal, 13));
            checkEnded(stop, item.output, possible ? M::ActionExecution::UNKNOWN : M::ActionExecution::NOT_TRANSMITTED);
            CHECK(stop.commandEvidencePresent && stop.commandEvidence.outcome == item.input && stop.servicedUs == 13);
            // The first terminal is immutable.
            const Saved<L::BestEffortStop> closed(stop);
            CHECK(!L::advanceBestEffortStop(stop, terminal, 14));
            CHECK(!L::advanceBestEffortStop(stop, M::ActionOutcome::CANCELLED, 14));
            CHECK(!L::noteBestEffortAdmission(stop, 14));
            L::PreparedAction work;
            CHECK(L::nextBestEffortStop(stop, t, 14, work) && !work.ready);
            closed.unchanged(stop);
        }
    // Ending a stop that was never admitted.
    {
        const M::ActionOutcome reasons[] = {M::ActionOutcome::CANCELLED, M::ActionOutcome::INVALIDATED,
                                            M::ActionOutcome::TRANSPORT_ERROR};
        for (M::ActionOutcome reason : reasons) {
            L::BestEffortStop stop;
            CHECK(L::prepareBestEffortStop(stop, t, 5, 10, 20, reads()));
            CHECK(L::advanceBestEffortStop(stop, reason, 19));
            checkEnded(stop, reason, M::ActionExecution::NOT_TRANSMITTED);
            // An inclusive deadline wins over the local reason.
            L::BestEffortStop late;
            CHECK(L::prepareBestEffortStop(late, t, 5, 10, 20, reads()));
            CHECK(L::advanceBestEffortStop(late, reason, 20));
            checkEnded(late, M::ActionOutcome::DEADLINE, M::ActionExecution::NOT_TRANSMITTED);
        }
        L::BestEffortStop stop;
        CHECK(L::prepareBestEffortStop(stop, t, 5, 10, 20, reads()));
        const Saved<L::BestEffortStop> before(stop);
        CHECK(!L::advanceBestEffortStop(stop, M::ActionOutcome::DEADLINE, 19));
        CHECK(!L::advanceBestEffortStop(stop, M::ActionOutcome::CANCELLED, 9));
        CHECK(!L::advanceBestEffortStop(stop, M::ActionOutcome::OBSERVED, 15));
        CHECK(!L::advanceBestEffortStop(stop, M::ActionOutcome::STOP_SENT_UNCONFIRMED, 15));
        CHECK(!L::advanceBestEffortStop(stop, M::ActionOutcome::NONE, 15));
        before.unchanged(stop);
        CHECK(L::advanceBestEffortStop(stop, M::ActionOutcome::DEADLINE, 20));
        CHECK(stop.outcome == M::ActionOutcome::DEADLINE);
    }
}

// 12. A-20: an impossible freshness budget, and a real service gap.
M::AxisConfig axisFor(const L::ReadTarget& t) {
    M::AxisConfig axis;
    axis.binding = t.channel.binding;
    axis.referenceGeneration = 1;
    axis.units.commandStepsPerMotorTurn = M::UnitScale(3200, 1, M::ScaleSource::READBACK);
    axis.units.encoder.countsPerUnit = M::UnitScale(4000, 1, M::ScaleSource::READBACK);
    axis.units.encoder.sourceId = 6;
    axis.supportedRelativeBases = 1;
    return axis;
}
void freshnessBudgetBoundaries() {
    struct Budget {
        uint32_t queue, response;
    };
    const Budget budgets[] = {{100, 100},
                              {UINT32_MAX, 1},
                              {1, UINT32_MAX},
                              {UINT32_MAX, UINT32_MAX},
                              {0x80000000u, 0x80000000u}};
    for (const Budget& item : budgets)
        for (unsigned which = 0; which < 3; ++which) {
            Rig r;
            openAxis(r, r.a);
            L::ReadContext state;
            if (which == 0) {
                history(r, r.a, state);
            } else if (which == 1) {
                r.drive[0].monitor = 8;
                readState(r, r.a, state, 1,
                          uint8_t(bit(L::StateBlock::ERRORS) | bit(L::StateBlock::ERROR_HISTORY) | STATE_MASK));
            } else {
                r.drive[0].monitor = 0;
                readState(r, r.a, state);
            }
            const L::AxisReservation reservation = reservationOf(r, r.a);
            const M::AxisConfig axis = axisFor(r.current());
            const unsigned fields = which == 2 ? 7 : 5;
            L::ActionOptions options;
            options.service = reads(item.queue, item.response);
            options.observationIntervalUs = 10;
            options.maximumAgeUs = fields * (uint64_t(item.queue) + item.response);
            const uint32_t operation = begin(r, r.a);
            const auto attempt = [&](L::ActionContext& output, const L::ActionOptions& policy) -> M::Status {
                if (which == 0)
                    return L::prepareStop(output, reservation, state, *r.a.latestControl(), L::StopKind::HALT,
                                          operation, r.now, r.opDeadline, policy);
                if (which == 1)
                    return L::prepareClearAlarm(output, reservation, state, operation, r.now, r.opDeadline, policy);
                return L::prepareSetDevicePosition(output, reservation, state, 0, axis, operation, r.now,
                                                   r.opDeadline, policy);
            };
            L::ActionContext accepted;
            CHECK(attempt(accepted, options));
            CHECK(accepted.state == M::ActionState::ACTIVE);
            options.maximumAgeUs -= 1;
            L::ActionContext rejected;
            rejected.operation = 777;
            const Saved<L::ActionContext> unchanged(rejected);
            const M::Status status = attempt(rejected, options);
            CHECK(!status && status.detail == int32_t(M::ActionError::ARGUMENT));
            unchanged.unchanged(rejected);
            // A product computed in 32 bits would wrap to something these pass.
            options.maximumAgeUs = (fields * (uint64_t(item.queue) + item.response)) & 0xffffffffu;
            if (options.maximumAgeUs < fields * (uint64_t(item.queue) + item.response)) {
                CHECK(!attempt(rejected, options));
                unchanged.unchanged(rejected);
            }
            options.maximumAgeUs = 1;
            CHECK(!attempt(rejected, options));
            unchanged.unchanged(rejected);
        }
    // Through the session: an impossible budget is not a sequenced stop.
    Rig r;
    openAxis(r, r.a);
    L::ReadContext state;
    history(r, r.a, state);
    E::StopPolicy policy = stopPolicy(r.now, 5000, 0);
    policy.options.maximumAgeUs = 999;
    const Saved<E::AxisSession> session(r.a);
    CHECK(r.a.requestStop(L::StopKind::HALT, state, policy, r.now) == E::StopPath::NONE);
    session.unchanged(r.a);
    policy.options.maximumAgeUs = 1000;
    CHECK(r.a.requestStop(L::StopKind::HALT, state, policy, r.now) == E::StopPath::CONFIRMED);
}
void realServiceGapStillExpires() {
    // Valid budgets, and every field within them. The application just stops servicing the
    // session for a while, so the sequential cycle outlives the freshness cap.
    for (unsigned over = 0; over < 2; ++over) {
        Rig r;
        openAxis(r, r.a);
        L::ReadContext state;
        history(r, r.a, state);
        const unsigned from = r.wire.sentCount;
        const E::StopPolicy policy = stopPolicy(r.now);
        CHECK(r.a.requestStop(L::StopKind::HALT, state, policy, r.now) == E::StopPath::CONFIRMED);
        const L::ActionContext& stop = r.a.stopAction(L::StopKind::HALT);
        r.steps(2);
        CHECK(stop.phase == L::ActionPhase::OBSERVING && (stop.commandEvidenceMask & 1u));
        r.steps(7); // Fields one to three settle, and field four goes out.
        CHECK(r.wire.sentCount == from + 5 && frameIndex(r.wire.sent[from + 4]) == 0x6041);
        const uint64_t limit = stop.read.values[0].provenance.earliestUs + policy.options.maximumAgeUs;
        r.serviceA = false;
        r.step(); // The drive answers field four, and the session does not read it for a long time.
        r.serviceA = true;
        r.turn(limit - 2); // Field five is admitted here.
        r.step();
        CHECK(r.wire.sentCount == from + 6 && frameIndex(r.wire.sent[from + 5]) == 0x6061);
        CHECK(stop.state == M::ActionState::ACTIVE && stop.observations == 0);
        r.turn(limit + over);
        CHECK(stop.execution == M::ActionExecution::ACKNOWLEDGED);
        CHECK(stop.completion == M::ActionCompletion::NOT_OBSERVED);
        if (over) {
            CHECK(stop.state == M::ActionState::FAILED && stop.outcome == M::ActionOutcome::OBSERVATION_SCOPE);
            CHECK(stop.observations == 0 && !stop.observationKnown && stop.uncertain);
            CHECK(r.a.stopPath() == E::StopPath::FINISHED);
            quiet(r, from + 6);
            CHECK(writes6040(r, from) == 1);
        } else {
            CHECK(stop.state == M::ActionState::ACTIVE && stop.observations == 1 && stop.observationKnown);
        }
    }
}

// 13. Alarm clear reads ERRORS once, after the alarm clears.
struct AlarmRun {
    Rig r;
    L::ReadContext state;
    L::ActionContext action;
    L::AxisReservation reservation;
    unsigned from = 0;
    AlarmRun() {
        openAxis(r, r.a);
        r.drive[0].monitor = 8;
        readState(r, r.a, state, 1,
                  uint8_t(bit(L::StateBlock::ERRORS) | bit(L::StateBlock::ERROR_HISTORY) | STATE_MASK));
        reservation = reservationOf(r, r.a);
        const uint32_t operation = begin(r, r.a);
        L::ActionOptions options;
        options.service = reads();
        options.observationIntervalUs = 10;
        options.maximumAgeUs = 1000;
        CHECK(L::prepareClearAlarm(action, reservation, state, operation, r.now, r.opDeadline, options));
        from = r.wire.sentCount;
    }
    // One service through the session, and back into the action.
    void field() {
        L::PreparedAction work;
        CHECK(L::nextAction(action, reservation, r.current(), r.now, work));
        if (!work.ready) {
            r.turn(work.eligibleUs);
            return;
        }
        if (work.transfer.direction == C::SdoDirection::DOWNLOAD)
            CHECK(L::noteActionTransmission(action, reservation, r.current(), r.now));
        CHECK(r.a.admitPrepared(work.transfer, r.now) == O::BusAdmission::ACCEPTED);
        r.steps(2);
        const C::SdoCompletion terminal = settled(r.a);
        CHECK(L::advanceAction(action, reservation, r.current(), terminal, r.now));
    }
};
void alarmErrorsReadOnce() {
    for (unsigned cycles = 1; cycles < 4; ++cycles) {
        AlarmRun run;
        Rig& r = run.r;
        run.field();
        run.field();
        CHECK(r.wire.sentCount == run.from + 2);
        const uint8_t on[] = {0x2b, 0x01, 0x23, 0, 1, 0, 0, 0}, off[] = {0x2b, 0x01, 0x23, 0, 0, 0, 0, 0};
        CHECK(!std::memcmp(r.wire.sent[run.from].data, on, 8) && !std::memcmp(r.wire.sent[run.from + 1].data, off, 8));
        // The alarm stays set for these cycles. Polling touches no consuming object.
        for (unsigned cycle = 0; cycle < cycles; ++cycle) {
            while (run.action.observations == cycle)
                run.field();
            CHECK(run.action.state == M::ActionState::ACTIVE && run.action.phase == L::ActionPhase::WAITING);
            CHECK(uploads(r, run.from, 0x1001) == 0 && uploads(r, run.from, 0x603f) == 0);
            CHECK(!run.action.finalErrorRead);
        }
        CHECK(r.wire.sentCount == run.from + 2 + cycles * 5);
        r.drive[0].monitor = 0;
        while (run.action.observations == cycles)
            run.field();
        CHECK(run.action.finalErrorRead && run.action.state == M::ActionState::ACTIVE);
        CHECK(uploads(r, run.from, 0x1001) == 0 && uploads(r, run.from, 0x603f) == 0);
        const unsigned monitors = r.wire.sentCount;
        r.drive[0].errorCode = uint16_t(0x1000 + cycles); // Not the value the prerequisite read.
        while (run.action.state == M::ActionState::ACTIVE)
            run.field();
        CHECK(run.action.state == M::ActionState::SUCCEEDED && run.action.outcome == M::ActionOutcome::OBSERVED);
        CHECK(r.wire.sentCount == monitors + 2);
        CHECK(uploads(r, run.from, 0x1001) == 1 && uploads(r, run.from, 0x603f) == 1);
        CHECK(uploads(r, monitors, 0x1001) == 1 && uploads(r, monitors, 0x603f) == 1);
        CHECK(uploads(r, monitors, 0x2100) == 0);
        CHECK(run.action.extraObservation.errorCode == 0x1000 + cycles &&
              run.action.extraObservation.errorRegister == 1);
        CHECK(run.action.monitorObservation.monitor == 0 && run.action.observations == cycles + 1);
        CHECK(run.action.completion == M::ActionCompletion::OBSERVED);
        quiet(r);
    }
}
void alarmErrorsFailureKeepsMonitorObservation() {
    const uint16_t objects[] = {0x1001, 0x603f};
    for (uint16_t object : objects)
        for (unsigned lost = 0; lost < 2; ++lost) {
            AlarmRun run;
            Rig& r = run.r;
            run.field();
            run.field();
            r.drive[0].monitor = 0;
            while (!run.action.finalErrorRead)
                run.field();
            const L::StateObservation monitors = run.action.monitorObservation;
            CHECK(monitors.monitor == 0 && monitors.fieldCount);
            const Saved<L::StateObservation> retained(run.action.monitorObservation);
            const unsigned before = r.wire.sentCount;
            if (object == 0x603f)
                run.field();
            if (lost) {
                r.drive[0].answer = false;
                L::PreparedAction work;
                CHECK(L::nextAction(run.action, run.reservation, r.current(), r.now, work) && work.ready);
                CHECK(r.a.admitPrepared(work.transfer, r.now) == O::BusAdmission::ACCEPTED);
                r.step();
                r.turn(r.now + 100);
                const C::SdoCompletion terminal = settled(r.a);
                CHECK(terminal.outcome == C::SdoOutcome::TIMEOUT);
                CHECK(L::advanceAction(run.action, run.reservation, r.current(), terminal, r.now));
                CHECK(run.action.outcome == M::ActionOutcome::SERVICE_TIMEOUT);
            } else {
                r.drive[0].abortIndex = object;
                run.field();
                CHECK(run.action.outcome == M::ActionOutcome::PROTOCOL_REJECTED);
                CHECK(run.action.observationEvidence.abortValid &&
                      run.action.observationEvidence.abortCode == 0x08000023);
            }
            CHECK(run.action.state == M::ActionState::FAILED);
            CHECK(run.action.execution == M::ActionExecution::ACKNOWLEDGED);
            CHECK(run.action.completion == M::ActionCompletion::NOT_OBSERVED && run.action.uncertain);
            CHECK(run.action.observationKnown && run.action.observations == 1);
            retained.unchanged(run.action.monitorObservation);
            CHECK(uploads(r, run.from, object) == 1); // Never tried again.
            CHECK(r.wire.sentCount == before + (object == 0x603f ? 2u : 1u));
            quiet(r);
        }
}

// Session rules the numbered scenarios lean on.
void sessionLifecycle() {
    Rig r;
    L::ReadTarget wrong = target();
    wrong.nodeGeneration = 12;
    CHECK(!r.a.open(wrong, L::SettingsModel::CL86_C, 1, r.now)); // Not the current epoch.
    wrong = target();
    wrong.nodeGeneration = 0;
    CHECK(!r.a.open(wrong, L::SettingsModel::CL86_C, 1, r.now));
    wrong = target();
    wrong.channel.clientToServerId = 0;
    CHECK(!r.a.open(wrong, L::SettingsModel::CL86_C, 1, r.now));
    wrong = target();
    wrong.channel.binding.logicalAxisId = 999;
    CHECK(!r.a.open(wrong, L::SettingsModel::CL86_C, 1, r.now));
    CHECK(!r.a.isOpen() && !r.a.close());
    L::ReadContext state;
    C::SdoCompletion output;
    output.size = 9;
    CHECK(!r.a.settle(output) && output.size == 9);
    CHECK(!r.a.beginOperation(r.now + 100, r.now));
    CHECK(r.a.requestStop(L::StopKind::QUICK_STOP, state, stopPolicy(r.now), r.now) == E::StopPath::NONE);
    openAxis(r, r.a);
    CHECK(r.a.isOpen() && r.owner.sequence(r.a.sequence()));
    CHECK(!r.a.open(target(), L::SettingsModel::CL86_C, 1, r.now)); // Already open.
    CHECK(M::sameBinding(r.a.binding(), target().channel.binding));
    CHECK(!r.a.latestControl() && r.a.stopPath() == E::StopPath::NONE && !r.a.stopActive());
    CHECK(r.a.stopAction(L::StopKind::HALT).state == M::ActionState::EMPTY &&
          r.a.bestEffort().state == M::ActionState::EMPTY);
    r.step();
    // Time cannot go backward.
    CHECK(!r.a.beginOperation(r.now + 100, r.now - 1));
    CHECK(!r.a.beginOperation(r.now, r.now)); // A deadline must be in the future.
    const uint32_t first = begin(r, r.a);
    C::SdoTransfer poll = service(r, 1, first, 1, 0x2100);
    CHECK(r.a.admitPrepared(poll, r.now - 1) == O::BusAdmission::INVALID);
    CHECK(r.a.admitPrepared(poll, r.now) == O::BusAdmission::ACCEPTED);
    poll.step = 2;
    CHECK(r.a.admitPrepared(poll, r.now) == O::BusAdmission::INVALID); // One command at a time.
    CHECK(!r.a.beginOperation(r.now + 100, r.now));
    CHECK(!r.a.close());
    r.steps(2);
    CHECK(!r.a.close()); // Its result is unread.
    CHECK(settled(r.a).transfer.operation == first);
    CHECK(!r.a.settle(output));
    // A session never reuses an operation identity, and a stop takes the next ones.
    history(r, r.a, state);
    r.drive[0].monitor = 0;
    CHECK(r.a.requestStop(L::StopKind::HALT, state, stopPolicy(r.now), r.now) == E::StopPath::CONFIRMED);
    const uint32_t stopOperation = r.a.stopAction(L::StopKind::HALT).operation;
    CHECK(stopOperation > first);
    // Everything else is refused while it runs.
    CHECK(!r.a.beginOperation(r.now + 100, r.now));
    poll.operation = poll.request = stopOperation;
    poll.step = 50;
    poll.deadlineUs = r.a.stopAction(L::StopKind::HALT).deadlineUs;
    CHECK(r.a.admitPrepared(poll, r.now) == O::BusAdmission::INVALID);
    CHECK(!r.a.close());
    finish(r, r.a);
    const uint32_t after = begin(r, r.a);
    CHECK(after > stopOperation);
    // A reservation this session took is released on close, with the history.
    const O::SequenceId held = r.a.sequence();
    CHECK(r.a.close() && !r.a.isOpen() && !r.a.latestControl() && !r.owner.sequence(held));
    CHECK(r.a.stopPath() == E::StopPath::NONE);
    // Reopening starts with empty contexts.
    openAxis(r, r.a);
    CHECK(r.a.stopAction(L::StopKind::HALT).state == M::ActionState::EMPTY);
}
void onlyFreshCurrentStateQualifies() {
    // Another axis's state, a missing read or a running drive out of the sequenced scope is
    // 000B-only. These are this session's policy: nothing shows what the drive does.
    for (unsigned variant = 0; variant < 6; ++variant)
        for (unsigned allowed = 0; allowed < 2; ++allowed) {
            Rig r;
            openAxis(r, r.a, 1);
            openAxis(r, r.b, 2);
            L::ReadContext state;
            if (variant == 1)
                r.drive[0].mode = 3;
            if (variant == 2)
                r.drive[0].status = 3;
            if (variant == 4)
                r.drive[0].monitor = 0x000c; // Alarm and running.
            if (variant == 5)
                r.drive[0].monitor = 0x0014; // Released and running.
            history(r, r.a, state);
            if (variant == 0)
                readState(r, r.b, state, 2);
            if (variant == 3)
                state = L::ReadContext();
            const unsigned from = r.wire.sentCount;
            const Saved<O::OwnerResources> resources(r.owner.resources());
            const E::StopPath path =
                r.a.requestStop(L::StopKind::HALT, state, stopPolicy(r.now, 5000, allowed ? 6000 : 0), r.now);
            if (!allowed) {
                CHECK(path == E::StopPath::NONE);
                resources.unchanged(r.owner.resources());
                CHECK(r.a.latestControl());
                continue;
            }
            CHECK(path == E::StopPath::BEST_EFFORT);
            finish(r, r.a);
            CHECK(r.wire.sentCount == from + 1 && isWord(r, from, WORD_QUICK));
        }
}
// A fresh idle report outside the stop's states refuses both paths with no traffic.
void idleOutOfScopeRefuses() {
    struct Report {
        uint16_t monitor, status;
        uint8_t mode;
    };
    const Report reports[] = {{0x0008, 7, 1}, {0x0010, 7, 1}, {0x0018, 7, 1}, {0x0000, 7, 3},
                              {0x0000, 3, 1}, {0x0000, 6, 1}, {0x0000, 0, 0}, {0x0008, 3, 3}};
    for (const Report& report : reports)
        for (unsigned withHistory = 0; withHistory < 2; ++withHistory)
            for (unsigned kind = 0; kind < 2; ++kind) {
                const L::StopKind selected = kind ? L::StopKind::QUICK_STOP : L::StopKind::HALT;
                Rig r;
                openAxis(r, r.a);
                L::ReadContext state;
                r.drive[0].monitor = report.monitor;
                r.drive[0].status = report.status;
                r.drive[0].mode = report.mode;
                if (withHistory)
                    history(r, r.a, state);
                else
                    readState(r, r.a, state);
                const Saved<E::AxisSession> session(r.a);
                const Saved<O::OwnerResources> resources(r.owner.resources());
                const unsigned sent = r.wire.sentCount;
                CHECK(r.a.requestStop(selected, state, stopPolicy(r.now), r.now) == E::StopPath::NONE);
                session.unchanged(r.a);
                resources.unchanged(r.owner.resources());
                quiet(r, sent);
                // Once that report has expired it says nothing, and the 000B branch is open.
                r.turn(state.values[0].provenance.earliestUs + 1001);
                CHECK(r.a.requestStop(selected, state, stopPolicy(r.now), r.now) == E::StopPath::BEST_EFFORT);
            }
    // Up to the exact age limit the report still refuses.
    Rig r;
    openAxis(r, r.a);
    L::ReadContext state;
    r.drive[0].monitor = 0x0010;
    readState(r, r.a, state);
    r.turn(state.values[0].provenance.earliestUs + 1000);
    CHECK(r.a.requestStop(L::StopKind::QUICK_STOP, state, stopPolicy(r.now), r.now) == E::StopPath::NONE);
    // An idle drive with a clean report keeps the sequenced path.
    Rig clean;
    openAxis(clean, clean.a);
    clean.drive[0].monitor = 0;
    history(clean, clean.a, state);
    CHECK(clean.a.requestStop(L::StopKind::QUICK_STOP, state, stopPolicy(clean.now), clean.now) == E::StopPath::CONFIRMED);
}
void stopRequestedDuringAFault() {
    // A session opened on a dead controller holds the request across its one recovery.
    {
        Rig r;
        CHECK(r.owner.reportController(C::ControllerState::BUS_OFF, r.now));
        r.step();
        openAxis(r, r.a);
        CHECK(!r.a.sequence().owner);
        L::ReadContext state;
        const E::StopPolicy policy = stopPolicy(r.now);
        CHECK(r.a.requestStop(L::StopKind::HALT, state, policy, r.now) == E::StopPath::HELD);
        r.steps(2);
        CHECK(r.a.stopPath() == E::StopPath::HELD && r.wire.sentCount == 0);
        O::RecoveryId id;
        recoverOnce(r, id);
        r.step();
        CHECK(r.wire.sentCount == 1 && isWord(r, 0, WORD_QUICK));
        finish(r, r.a);
        CHECK(r.a.bestEffort().deadlineUs == policy.bestEffortDeadlineUs);
        checkEnded(r.a.bestEffort(), M::ActionOutcome::STOP_SENT_UNCONFIRMED, M::ActionExecution::UNKNOWN);
    }
    // A second fault after the recovery, before the held stop leaves, ends it not transmitted.
    {
        Rig r;
        openAxis(r, r.a);
        L::ReadContext state;
        history(r, r.a, state);
        const unsigned from = r.wire.sentCount;
        CHECK(r.a.requestStop(L::StopKind::HALT, state, stopPolicy(r.now), r.now) == E::StopPath::CONFIRMED);
        CHECK(r.owner.reportController(C::ControllerState::BUS_OFF, r.now));
        r.step();
        O::RecoveryId id;
        recoverOnce(r, id);
        CHECK(r.a.stopPath() == E::StopPath::BEST_EFFORT && r.wire.sentCount == from);
        CHECK(r.owner.reportController(C::ControllerState::BUS_OFF, r.now));
        r.step();
        CHECK(r.a.stopPath() == E::StopPath::FINISHED);
        CHECK(r.a.bestEffort().state == M::ActionState::FAILED &&
              r.a.bestEffort().execution == M::ActionExecution::NOT_TRANSMITTED && !r.a.bestEffort().effectsPossible);
        CHECK(r.owner.releaseRecovery(id));
        CHECK(r.owner.recover(r.now, r.now + 200, id));
        r.steps(3);
        CHECK(r.wire.sentCount == from);
    }
}

// Regressions for the contract the independent review settled.
// Item 1: an escalated HALT's terminal lands in its own context on every path.
void supersededHaltSettlesAfterFinished() {
    for (unsigned lost = 0; lost < 2; ++lost)
        for (unsigned second = 0; second < 2; ++second) {
            Rig r;
            openAxis(r, r.a);
            L::ReadContext state;
            history(r, r.a, state);
            r.drive[0].answer = false;
            const unsigned from = r.wire.sentCount;
            CHECK(r.a.requestStop(L::StopKind::HALT, state, stopPolicy(r.now, 5000, 6000, 100, 1000), r.now) ==
                  E::StopPath::CONFIRMED);
            r.step();
            CHECK(isWord(r, from, WORD_HALT));
            const L::ActionContext& halt = r.a.stopAction(L::StopKind::HALT);
            const E::StopPolicy policy = stopPolicy(r.now, 20, 30, 100, 1000);
            CHECK(r.a.requestStop(L::StopKind::QUICK_STOP, state, policy, r.now) == E::StopPath::HELD);
            r.turn(policy.bestEffortDeadlineUs - 1);
            CHECK(r.a.stopPath() == E::StopPath::HELD && r.wire.sentCount == from + 1);
            r.turn(policy.bestEffortDeadlineUs);
            checkEnded(r.a.bestEffort(), M::ActionOutcome::DEADLINE, M::ActionExecution::NOT_TRANSMITTED);
            CHECK(r.a.stopPath() == E::StopPath::FINISHED);
            // Its request is still in flight, so the session is busy and cannot close.
            CHECK(halt.state == M::ActionState::ACTIVE && r.a.busy() && !r.a.stopActive());
            CHECK(!r.a.close() && !r.a.beginOperation(r.now + 100, r.now));
            // Nothing but QUICK_STOP may start, and HALT's context is not reused.
            CHECK(r.a.requestStop(L::StopKind::HALT, state, stopPolicy(r.now), r.now) == E::StopPath::NONE);
            unsigned sent = r.wire.sentCount;
            if (second) {
                // A later stop waits, and cannot take that terminal as its own.
                CHECK(r.a.requestStop(L::StopKind::QUICK_STOP, state, stopPolicy(r.now), r.now) == E::StopPath::HELD);
                r.steps(2);
                CHECK(r.wire.sentCount == sent && r.a.bestEffort().state == M::ActionState::ACTIVE &&
                      !r.a.bestEffort().admitted);
            }
            if (lost) {
                r.turn(r.now + 1000);
            } else {
                r.answer(from);
                r.step();
            }
            CHECK(halt.state == M::ActionState::FAILED && (halt.commandEvidenceMask & 1u));
            CHECK(halt.commandHistory[0].transfer.data[0] == 0x0f && halt.commandHistory[0].transfer.data[1] == 1);
            CHECK(halt.execution == (lost ? M::ActionExecution::UNKNOWN : M::ActionExecution::ACKNOWLEDGED));
            CHECK(halt.outcome == (lost ? M::ActionOutcome::SERVICE_TIMEOUT : M::ActionOutcome::CANCELLED));
            CHECK(!r.a.latestControl()); // Never from a superseded terminal, even a success.
            if (second) {
                r.step();
                CHECK(r.wire.sentCount == sent + 1 && isWord(r, sent, WORD_QUICK));
                finish(r, r.a);
                checkEnded(r.a.bestEffort(), M::ActionOutcome::STOP_SENT_UNCONFIRMED, M::ActionExecution::UNKNOWN);
                CHECK(r.a.bestEffort().commandEvidence.transfer.data[0] == 0x0b);
                CHECK(r.a.close());
                continue;
            }
            // close then succeeds, and a new stop sends.
            CHECK(!r.a.busy() && r.a.close());
            openAxis(r, r.a);
            CHECK(!r.a.latestControl());
            sent = r.wire.sentCount;
            CHECK(r.a.requestStop(L::StopKind::QUICK_STOP, state, stopPolicy(r.now), r.now) == E::StopPath::BEST_EFFORT);
            r.step();
            CHECK(r.wire.sentCount == sent + 1 && isWord(r, sent, WORD_QUICK));
            finish(r, r.a);
            checkEnded(r.a.bestEffort(), M::ActionOutcome::STOP_SENT_UNCONFIRMED, M::ActionExecution::UNKNOWN);
        }
}
// Item 2: invalid options refuse at the request, whichever path it would have taken.
void invalidOptionsRefuseAtRequest() {
    L::ActionOptions good;
    good.service = reads();
    good.observationIntervalUs = 10;
    good.maximumAgeUs = 1000;
    CHECK(L::stopOptionsValid(good));
    L::ActionOptions bad = good;
    bad.service.queueTimeoutUs = 0;
    CHECK(!L::stopOptionsValid(bad));
    bad = good;
    bad.service.responseTimeoutUs = 0;
    CHECK(!L::stopOptionsValid(bad));
    bad = good;
    bad.observationIntervalUs = 0;
    CHECK(!L::stopOptionsValid(bad));
    bad = good;
    bad.maximumAgeUs = 0;
    CHECK(!L::stopOptionsValid(bad));
    bad = good;
    bad.maximumAgeUs = 999;
    CHECK(!L::stopOptionsValid(bad));
    bad.service = reads(UINT32_MAX, UINT32_MAX);
    bad.maximumAgeUs = 5 * (uint64_t(UINT32_MAX) * 2);
    CHECK(L::stopOptionsValid(bad));
    --bad.maximumAgeUs;
    CHECK(!L::stopOptionsValid(bad));
    bad.maximumAgeUs = 5 * (uint64_t(UINT32_MAX) * 2) & 0xffffffffu;
    CHECK(!L::stopOptionsValid(bad));
    for (unsigned wrong = 0; wrong < 5; ++wrong)
        for (unsigned route = 0; route < 4; ++route) {
            Rig r;
            openAxis(r, r.a);
            L::ReadContext state;
            if (route != 1)
                history(r, r.a, state); // Route 1 has no history: only the 000B branch could run.
            if (route == 3) {
                CHECK(r.a.requestStop(L::StopKind::HALT, state, stopPolicy(r.now), r.now) == E::StopPath::CONFIRMED);
            } else if (route == 2) {
                // An unresolved route, where also only that branch could run.
                const uint32_t operation = begin(r, r.a);
                r.drive[0].answer = false;
                CHECK(r.a.admitPrepared(service(r, 1, operation, 1, 0x2100), r.now) == O::BusAdmission::ACCEPTED);
                r.step();
                r.turn(r.now + 100);
                CHECK(settled(r.a).outcome == C::SdoOutcome::TIMEOUT && r.services.unresolved(0));
            } else {
                // Queued work that a stop would retire.
                const uint32_t operation = begin(r, r.a);
                CHECK(r.a.admitPrepared(service(r, 1, operation, 1, 0x2100), r.now) == O::BusAdmission::ACCEPTED);
            }
            E::StopPolicy policy = stopPolicy(r.now);
            if (wrong == 0)
                policy.options.service.queueTimeoutUs = 0;
            if (wrong == 1)
                policy.options.service.responseTimeoutUs = 0;
            if (wrong == 2)
                policy.options.observationIntervalUs = 0;
            if (wrong == 3)
                policy.options.maximumAgeUs = 999;
            if (wrong == 4)
                policy.options.maximumAgeUs = 0;
            const unsigned sent = r.wire.sentCount;
            const Saved<E::AxisSession> session(r.a);
            const Saved<O::OwnerResources> resources(r.owner.resources());
            const L::StopKind selected = L::StopKind::QUICK_STOP;
            CHECK(r.a.requestStop(selected, state, policy, r.now) == E::StopPath::NONE);
            session.unchanged(r.a);
            resources.unchanged(r.owner.resources());
            CHECK(r.wire.sentCount == sent);
            CHECK(r.a.bestEffort().state == M::ActionState::EMPTY);
            if (route < 2) {
                r.steps(2);
                CHECK(r.wire.sentCount == sent + 1 && !isWord(r, sent, WORD_QUICK));
                CHECK(settled(r.a).outcome == C::SdoOutcome::SUCCESS); // The queued work was not retired.
            } else if (route == 3) {
                r.step();
                CHECK(isWord(r, sent, WORD_HALT) && writes6040(r, sent) == 1); // The HALT carries on alone.
            } else {
                quiet(r, sent);
            }
        }
}
// Items 3 and 6: a best-effort stop proved never sent by a local fault waits for one recovery.
void bestEffortHeldByFault() {
    enum { BUS_OFF, OVERFLOW, RECOVERY };
    for (unsigned fault = BUS_OFF; fault <= RECOVERY; ++fault)
        for (unsigned behindRead = 0; behindRead < 2; ++behindRead) {
            Rig r;
            openAxis(r, r.a);
            L::ReadContext state;
            if (behindRead) {
                const uint32_t operation = begin(r, r.a);
                r.drive[0].answer = false;
                CHECK(r.a.admitPrepared(service(r, 1, operation, 1, 0x2100), r.now) == O::BusAdmission::ACCEPTED);
                r.step();
            }
            const unsigned from = r.wire.sentCount;
            const E::StopPolicy policy = stopPolicy(r.now, 800, 900);
            CHECK(r.a.requestStop(L::StopKind::HALT, state, policy, r.now) == E::StopPath::BEST_EFFORT);
            const uint32_t firstOperation = r.a.bestEffort().operation;
            O::RecoveryId id;
            if (fault == BUS_OFF) {
                CHECK(r.owner.reportController(C::ControllerState::BUS_OFF, r.now));
            } else if (fault == OVERFLOW) {
                CHECK(r.owner.receiveOverflow(r.now));
            } else {
                r.wire.progress = O::RecoveryProgress::PENDING;
                CHECK(r.owner.recover(r.now, r.now + 200, id));
            }
            r.step();
            CHECK(r.a.stopPath() == E::StopPath::HELD && r.wire.sentCount == from);
            // The attempt the fault ended stays visible, and the waiting one keeps its deadline.
            const L::BestEffortStop& ended = r.a.firstBestEffort();
            CHECK(ended.state == M::ActionState::FAILED && ended.operation == firstOperation &&
                  ended.execution == M::ActionExecution::NOT_TRANSMITTED && !ended.effectsPossible &&
                  ended.commandEvidencePresent && !ended.commandEvidence.transmissionPossible);
            const Saved<L::BestEffortStop> retained(ended);
            CHECK(r.a.bestEffort().state == M::ActionState::ACTIVE && !r.a.bestEffort().admitted);
            CHECK(r.a.bestEffort().deadlineUs == policy.bestEffortDeadlineUs &&
                  r.a.bestEffort().operation > firstOperation);
            r.steps(2);
            CHECK(r.a.stopPath() == E::StopPath::HELD && r.wire.sentCount == from);
            if (fault == RECOVERY) {
                r.wire.progress = O::RecoveryProgress::RETIRED;
                r.step();
            } else {
                recoverOnce(r, id);
            }
            CHECK(r.a.stopPath() == E::StopPath::BEST_EFFORT);
            r.step();
            CHECK(r.wire.sentCount == from + 1 && isWord(r, from, WORD_QUICK));
            finish(r, r.a);
            checkEnded(r.a.bestEffort(), M::ActionOutcome::STOP_SENT_UNCONFIRMED, M::ActionExecution::UNKNOWN);
            retained.unchanged(r.a.firstBestEffort());
            quiet(r, from + 1);
            if (!behindRead) {
                CHECK(r.a.close());
                openAxis(r, r.a);
                CHECK(r.a.firstBestEffort().state == M::ActionState::EMPTY &&
                      r.a.bestEffort().state == M::ActionState::EMPTY);
            }
        }
    // Held, admitted after the recovery, then a second fault: nothing leaves, ever.
    {
        Rig r;
        openAxis(r, r.a);
        L::ReadContext state;
        CHECK(r.a.requestStop(L::StopKind::HALT, state, stopPolicy(r.now), r.now) == E::StopPath::BEST_EFFORT);
        CHECK(r.owner.reportController(C::ControllerState::BUS_OFF, r.now));
        r.step();
        O::RecoveryId id;
        recoverOnce(r, id);
        CHECK(r.a.stopPath() == E::StopPath::BEST_EFFORT);
        CHECK(r.owner.reportController(C::ControllerState::BUS_OFF, r.now));
        r.step();
        CHECK(r.a.stopPath() == E::StopPath::FINISHED);
        CHECK(r.a.bestEffort().state == M::ActionState::FAILED &&
              r.a.bestEffort().execution == M::ActionExecution::NOT_TRANSMITTED);
        CHECK(r.a.firstBestEffort().state == M::ActionState::FAILED);
        CHECK(r.owner.releaseRecovery(id));
        CHECK(r.owner.recover(r.now, r.now + 200, id));
        r.steps(3);
        CHECK(r.wire.sentCount == 0);
    }
    // A sequenced stop with an earlier fallback: requested while faulted, recovered, faulted
    // again. It may wait for that second fault, but the second recovery still ends it.
    {
        Rig r;
        CHECK(r.owner.reportController(C::ControllerState::BUS_OFF, r.now));
        r.step();
        openAxis(r, r.a);
        L::ReadContext state;
        CHECK(r.a.requestStop(L::StopKind::HALT, state, stopPolicy(r.now), r.now) == E::StopPath::HELD);
        O::RecoveryId id;
        recoverOnce(r, id);
        CHECK(r.a.stopPath() == E::StopPath::BEST_EFFORT);
        CHECK(r.owner.reportController(C::ControllerState::BUS_OFF, r.now));
        r.step();
        CHECK(r.a.bestEffort().execution == M::ActionExecution::NOT_TRANSMITTED);
        CHECK(r.owner.releaseRecovery(id));
        CHECK(r.owner.recover(r.now, r.now + 200, id));
        r.steps(3);
        CHECK(r.a.stopPath() == E::StopPath::FINISHED);
        CHECK(r.a.bestEffort().state == M::ActionState::FAILED &&
              r.a.bestEffort().execution == M::ActionExecution::NOT_TRANSMITTED);
        CHECK(r.wire.sentCount == 0);
    }
}
// Item 4: a held stop keeps the deadline it already had, and nothing widens it.
void heldStopKeepsItsDeadline() {
    const uint64_t laters[] = {0, 400, 5000};
    for (uint64_t later : laters) {
        // No recovery: the sequenced deadline ends it, even with a later or no branch deadline.
        {
            Rig r;
            openAxis(r, r.a);
            L::ReadContext state;
            history(r, r.a, state);
            const unsigned from = r.wire.sentCount;
            const E::StopPolicy policy = stopPolicy(r.now, 300, later);
            CHECK(r.a.requestStop(L::StopKind::HALT, state, policy, r.now) == E::StopPath::CONFIRMED);
            CHECK(r.owner.reportController(C::ControllerState::BUS_OFF, r.now));
            r.step();
            CHECK(r.a.stopPath() == E::StopPath::HELD && r.a.bestEffort().deadlineUs == policy.deadlineUs);
            r.turn(policy.deadlineUs - 1);
            CHECK(r.a.stopPath() == E::StopPath::HELD);
            r.turn(policy.deadlineUs);
            checkEnded(r.a.bestEffort(), M::ActionOutcome::DEADLINE, M::ActionExecution::NOT_TRANSMITTED);
            O::RecoveryId id;
            recoverOnce(r, id);
            quiet(r, from);
        }
        // With a recovery: 000B once, also when the branch was refused for other causes.
        {
            Rig r;
            openAxis(r, r.a);
            L::ReadContext state;
            history(r, r.a, state);
            const unsigned from = r.wire.sentCount;
            CHECK(r.a.requestStop(L::StopKind::HALT, state, stopPolicy(r.now, 300, later), r.now) ==
                  E::StopPath::CONFIRMED);
            CHECK(r.owner.reportController(C::ControllerState::BUS_OFF, r.now));
            r.step();
            O::RecoveryId id;
            recoverOnce(r, id);
            r.step();
            CHECK(r.wire.sentCount == from + 1 && isWord(r, from, WORD_QUICK));
            finish(r, r.a);
            checkEnded(r.a.bestEffort(), M::ActionOutcome::STOP_SENT_UNCONFIRMED, M::ActionExecution::UNKNOWN);
        }
    }
    // A best-effort stop keeps its own, later, deadline.
    Rig r;
    openAxis(r, r.a);
    L::ReadContext state;
    const E::StopPolicy policy = stopPolicy(r.now, 300, 400);
    CHECK(r.a.requestStop(L::StopKind::HALT, state, policy, r.now) == E::StopPath::BEST_EFFORT);
    CHECK(r.owner.reportController(C::ControllerState::BUS_OFF, r.now));
    r.step();
    CHECK(r.a.stopPath() == E::StopPath::HELD && r.a.bestEffort().deadlineUs == policy.bestEffortDeadlineUs);
    r.turn(policy.deadlineUs);
    CHECK(r.a.stopPath() == E::StopPath::HELD);
    r.turn(policy.bestEffortDeadlineUs - 1);
    CHECK(r.a.stopPath() == E::StopPath::HELD);
    r.turn(policy.bestEffortDeadlineUs);
    checkEnded(r.a.bestEffort(), M::ActionOutcome::DEADLINE, M::ActionExecution::NOT_TRANSMITTED);
    CHECK(r.wire.sentCount == 0);
    // Sequenced queue expiry with no branch is not a fault, and does not wait.
    Rig expired;
    openAxis(expired, expired.a);
    history(expired, expired.a, state);
    const uint32_t operation = begin(expired, expired.a);
    expired.drive[0].answer = false;
    CHECK(expired.a.admitPrepared(service(expired, 1, operation, 1, 0x2100, false, 0, reads(50, 1000)), expired.now) ==
          O::BusAdmission::ACCEPTED);
    expired.step();
    const uint64_t start = expired.now;
    CHECK(expired.a.requestStop(L::StopKind::HALT, state, stopPolicy(start, 5000, 0, 50, 1000), start) ==
          E::StopPath::CONFIRMED);
    expired.turn(start + 50);
    CHECK(expired.a.stopPath() == E::StopPath::FINISHED && expired.a.bestEffort().state == M::ActionState::EMPTY);
}
// Item 5: one recovery, counted from the request.
void oneRecoveryFromTheRequest() {
    // Bus-off and a failed recovery before the session's next turn. The second recovery ends it.
    {
        Rig r;
        openAxis(r, r.a);
        L::ReadContext state;
        history(r, r.a, state);
        const unsigned from = r.wire.sentCount;
        CHECK(r.a.requestStop(L::StopKind::HALT, state, stopPolicy(r.now), r.now) == E::StopPath::CONFIRMED);
        CHECK(r.owner.reportController(C::ControllerState::BUS_OFF, r.now));
        r.wire.progress = O::RecoveryProgress::FAILED;
        O::RecoveryId id;
        CHECK(r.owner.recover(r.now, r.now + 200, id));
        r.serviceA = false;
        r.step();
        CHECK(r.owner.recoveryResult(id)->outcome == O::RecoveryOutcome::FAILED);
        r.serviceA = true;
        r.step();
        CHECK(r.a.stopPath() == E::StopPath::HELD);
        CHECK(r.owner.releaseRecovery(id));
        r.wire.progress = O::RecoveryProgress::RETIRED;
        CHECK(r.owner.recover(r.now, r.now + 200, id));
        r.steps(4);
        checkEnded(r.a.bestEffort(), M::ActionOutcome::TRANSPORT_ERROR, M::ActionExecution::NOT_TRANSMITTED);
        CHECK(r.wire.sentCount == from);
    }
    // Requested while a recovery runs: that recovery is the one, whatever its result.
    for (unsigned succeeds = 0; succeeds < 2; ++succeeds) {
        Rig r;
        openAxis(r, r.a);
        r.step();
        r.wire.progress = O::RecoveryProgress::PENDING;
        O::RecoveryId id;
        CHECK(r.owner.recover(r.now, r.now + 200, id));
        r.step();
        CHECK(r.owner.recoveryActive());
        L::ReadContext state;
        const unsigned from = r.wire.sentCount;
        const E::StopPolicy policy = stopPolicy(r.now);
        CHECK(r.a.requestStop(L::StopKind::HALT, state, policy, r.now) == E::StopPath::HELD);
        r.steps(2);
        CHECK(r.a.stopPath() == E::StopPath::HELD && r.wire.sentCount == from);
        r.wire.progress = succeeds ? O::RecoveryProgress::RETIRED : O::RecoveryProgress::FAILED;
        r.step();
        CHECK(!r.owner.recoveryActive());
        if (succeeds) {
            r.step();
            CHECK(r.wire.sentCount == from + 1 && isWord(r, from, WORD_QUICK));
            finish(r, r.a);
            checkEnded(r.a.bestEffort(), M::ActionOutcome::STOP_SENT_UNCONFIRMED, M::ActionExecution::UNKNOWN);
        } else {
            CHECK(r.a.stopPath() == E::StopPath::HELD);
            CHECK(r.owner.releaseRecovery(id));
            r.wire.progress = O::RecoveryProgress::RETIRED;
            CHECK(r.owner.recover(r.now, r.now + 200, id));
            r.steps(3);
            checkEnded(r.a.bestEffort(), M::ActionOutcome::TRANSPORT_ERROR, M::ActionExecution::NOT_TRANSMITTED);
            CHECK(r.wire.sentCount == from);
        }
    }
    // Requested after a recovery failed and ended: the next one is the one.
    {
        Rig r;
        openAxis(r, r.a);
        r.wire.progress = O::RecoveryProgress::FAILED;
        O::RecoveryId id;
        recoverOnce(r, id);
        CHECK(!r.owner.recoveryActive());
        L::ReadContext state;
        const unsigned from = r.wire.sentCount;
        CHECK(r.a.requestStop(L::StopKind::QUICK_STOP, state, stopPolicy(r.now), r.now) == E::StopPath::HELD);
        CHECK(r.owner.releaseRecovery(id));
        r.wire.progress = O::RecoveryProgress::RETIRED;
        recoverOnce(r, id);
        r.step();
        CHECK(r.wire.sentCount == from + 1 && isWord(r, from, WORD_QUICK));
    }
    // An NMT handoff blocking the route just waits, and uses up nothing.
    for (unsigned recovered = 0; recovered < 2; ++recovered) {
        Rig r;
        r.wire.autoReport = false;
        C::NmtTransfer start;
        start.command = C::NmtCommand::START;
        start.target = r.services.node(0)->route;
        start.request = 123;
        start.deadlineUs = 5000;
        start.queueTimeoutUs = 20;
        C::NmtRequestId nmt;
        CHECK(r.owner.admitNmt(start, r.now, nmt));
        r.step();
        const unsigned nmtFrame = r.wire.sentCount - 1;
        CHECK(r.wire.sent[nmtFrame].id == 0 && r.wire.pending[nmtFrame]);
        openAxis(r, r.a);
        L::ReadContext state;
        const E::StopPolicy policy = stopPolicy(r.now);
        CHECK(r.a.requestStop(L::StopKind::QUICK_STOP, state, policy, r.now) == E::StopPath::HELD);
        r.steps(5);
        CHECK(r.a.stopPath() == E::StopPath::HELD && r.wire.sentCount == nmtFrame + 1);
        r.wire.autoReport = true;
        if (recovered) {
            O::RecoveryId id;
            recoverOnce(r, id); // Inhibited turns did not count as this recovery.
            CHECK(r.a.bestEffort().target.nodeGeneration == r.current().nodeGeneration);
        } else {
            r.wire.report(nmtFrame);
            r.step();
        }
        CHECK(r.a.stopPath() == E::StopPath::BEST_EFFORT);
        r.step();
        CHECK(r.wire.sentCount == nmtFrame + 2 && isWord(r, nmtFrame + 1, WORD_QUICK));
        finish(r, r.a);
        checkEnded(r.a.bestEffort(), M::ActionOutcome::STOP_SENT_UNCONFIRMED, M::ActionExecution::UNKNOWN);
        CHECK(r.a.bestEffort().deadlineUs == policy.bestEffortDeadlineUs);
    }
    // So does an external frame with no proof of retirement.
    {
        Rig r;
        openAxis(r, r.a);
        const uint32_t operation = begin(r, r.a);
        r.drive[0].answer = false;
        r.wire.autoReport = r.wire.retirement = false;
        CHECK(r.a.admitPrepared(service(r, 1, operation, 1, 0x2100), r.now) == O::BusAdmission::ACCEPTED);
        r.step();
        const unsigned poll = r.wire.sentCount - 1;
        r.turn(r.now + 100);
        CHECK(settled(r.a).outcome == C::SdoOutcome::TIMEOUT && r.wire.pending[poll]);
        L::ReadContext state;
        CHECK(r.a.requestStop(L::StopKind::QUICK_STOP, state, stopPolicy(r.now), r.now) == E::StopPath::HELD);
        r.steps(5);
        CHECK(r.a.stopPath() == E::StopPath::HELD && r.wire.sentCount == poll + 1);
        r.wire.autoReport = true;
        r.wire.report(poll);
        r.step();
        CHECK(r.a.stopPath() == E::StopPath::BEST_EFFORT);
        r.step();
        CHECK(r.wire.sentCount == poll + 2 && isWord(r, poll + 1, WORD_QUICK));
    }
}
// Item 9: operation identities refuse early enough for every stop a request can need.
void operationIdentityBoundary() {
    for (unsigned sequenced = 0; sequenced < 2; ++sequenced) {
        // The last value that still has room: sequenced, its fallback and a held attempt.
        {
            Rig r;
            openAxis(r, r.a);
            L::ReadContext state;
            if (sequenced)
                history(r, r.a, state);
            r.opDeadline = r.now + r.window;
            CHECK(r.owner.beginOperation(r.a.sequence(), UINT32_MAX - 3, r.opDeadline, r.now));
            const unsigned from = r.wire.sentCount;
            CHECK(r.a.requestStop(L::StopKind::QUICK_STOP, state, stopPolicy(r.now), r.now) ==
                  (sequenced ? E::StopPath::CONFIRMED : E::StopPath::BEST_EFFORT));
            CHECK(r.owner.reportController(C::ControllerState::BUS_OFF, r.now));
            r.step();
            CHECK(r.a.stopPath() == E::StopPath::HELD);
            O::RecoveryId id;
            recoverOnce(r, id);
            r.step();
            CHECK(r.wire.sentCount == from + 1 && isWord(r, from, WORD_QUICK));
            CHECK(r.a.bestEffort().operation > UINT32_MAX - 3);
        }
        // One above: refused, with queued work left alone.
        const uint32_t limits[] = {UINT32_MAX - 2, UINT32_MAX - 1, UINT32_MAX};
        for (uint32_t limit : limits) {
            Rig r;
            openAxis(r, r.a);
            L::ReadContext state;
            if (sequenced)
                history(r, r.a, state);
            r.opDeadline = r.now + r.window;
            CHECK(r.owner.beginOperation(r.a.sequence(), limit, r.opDeadline, r.now));
            O::BusRequest raw;
            raw.sequence = r.a.sequence();
            raw.transfer = service(r, 1, limit, 1, 0x2100);
            O::RequestId id;
            CHECK(r.owner.admit(raw, r.now, id) == O::BusAdmission::ACCEPTED);
            const Saved<E::AxisSession> session(r.a);
            const Saved<O::OwnerResources> resources(r.owner.resources());
            const unsigned sent = r.wire.sentCount;
            CHECK(r.a.requestStop(L::StopKind::QUICK_STOP, state, stopPolicy(r.now), r.now) == E::StopPath::NONE);
            session.unchanged(r.a);
            resources.unchanged(r.owner.resources());
            CHECK(!r.owner.result(id) && r.owner.sequence(r.a.sequence())->operation == limit);
            r.steps(2);
            CHECK(r.wire.sentCount == sent + 1 && !isWord(r, sent, WORD_QUICK));
        }
    }
}
// Item 10: reads and operations refuse while a terminal is unread, so its history survives.
void busyExcludesReadsAndOperations() {
    AppRig r;
    CHECK(r.probe.select(target()));
    E::AxisSession* session = r.probe.openAxis(r.a, r.current(), L::SettingsModel::CL86_C, r.now);
    CHECK(session);
    const uint32_t operation = begin(r, *session);
    CHECK(session->admitPrepared(service(r, 1, operation, 1, 0x6040, true, WORD_ENABLE), r.now) ==
          O::BusAdmission::ACCEPTED);
    CHECK(session->busy() && !session->stopActive());
    r.steps(2); // Acknowledged by the fake, and not read.
    CHECK(session->busy() && !session->latestControl());
    const Saved<O::OwnerResources> resources(r.owner.resources());
    CHECK(!r.probe.requestState(r.now, r.now + 1000, reads(), STATE_MASK));
    CHECK(!r.probe.requestIdentity(r.now, r.now + 1000, reads()));
    CHECK(!r.probe.requestConfig(r.now, r.now + 1000, reads()));
    CHECK(r.probe.readContext().state == M::ReadState::EMPTY);
    CHECK(!session->beginOperation(r.now + 1000, r.now));
    C::SdoTransfer other = service(r, 1, operation, 2, 0x2100);
    CHECK(session->admitPrepared(other, r.now) == O::BusAdmission::INVALID);
    CHECK(!session->close());
    resources.unchanged(r.owner.resources());
    // The unread 000F still becomes the latest word, because nothing took its operation.
    CHECK(settled(*session).outcome == C::SdoOutcome::SUCCESS);
    CHECK(session->latestControl() && !session->busy());
    CHECK(session->latestControl()->transfer.data[0] == 0x0f);
    CHECK(r.probe.requestState(r.now, r.now + 1000, reads(), STATE_MASK));
    r.completeRead();
    const L::ReadContext state = r.probe.readContext();
    CHECK(state.state == M::ReadState::SUCCEEDED && r.probe.releaseRead());
    CHECK(session->latestControl());
    CHECK(session->requestStop(L::StopKind::HALT, state, stopPolicy(r.now), r.now) == E::StopPath::CONFIRMED);
    CHECK(session->busy() && !r.probe.requestState(r.now, r.now + 1000, reads(), STATE_MASK));
    // A superseded request keeps it busy after the path has finished.
    r.drive[0].answer = false;
    r.step();
    const unsigned frame = r.wire.sentCount - 1;
    CHECK(isWord(r, frame, WORD_HALT));
    const E::StopPolicy policy = stopPolicy(r.now, 2, 3);
    CHECK(session->requestStop(L::StopKind::QUICK_STOP, state, policy, r.now) == E::StopPath::HELD);
    r.turn(policy.bestEffortDeadlineUs);
    CHECK(!session->stopActive() && session->busy());
    CHECK(!r.probe.requestState(r.now, r.now + 1000, reads(), STATE_MASK));
    r.answer(frame);
    r.step();
    CHECK(!session->busy());
    CHECK(r.probe.requestState(r.now, r.now + 1000, reads(), STATE_MASK));
}
// A monitors-only read is not fresh STATUS and MONITORS, so it refuses nothing.
void partialStateDecidesWhatItCan() {
    // A monitor report alone can say idle and alarmed or released, and that refuses.
    const uint16_t refused[] = {0x0008, 0x0010};
    for (uint16_t report : refused) {
        Rig r;
        openAxis(r, r.a);
        L::ReadContext state;
        r.drive[0].monitor = report;
        readState(r, r.a, state, 1, bit(L::StateBlock::MONITORS));
        const unsigned from = r.wire.sentCount;
        CHECK(r.a.requestStop(L::StopKind::HALT, state, stopPolicy(r.now), r.now) == E::StopPath::NONE);
        r.step();
        CHECK(r.wire.sentCount == from);
    }
    // An idle report with no status block establishes nothing else, so the stop stays available.
    Rig r;
    openAxis(r, r.a);
    L::ReadContext state;
    r.drive[0].monitor = 0;
    readState(r, r.a, state, 1, bit(L::StateBlock::MONITORS));
    const unsigned from = r.wire.sentCount;
    CHECK(r.a.requestStop(L::StopKind::HALT, state, stopPolicy(r.now), r.now) == E::StopPath::BEST_EFFORT);
    r.step();
    CHECK(r.wire.sentCount == from + 1 && isWord(r, from, WORD_QUICK));
}
// A running stop between cycles ends on its own deadline, or at once when its route goes.
void waitingStopEnds() {
    for (unsigned event = 0; event < 3; ++event) {
        Rig r;
        openAxis(r, r.a);
        L::ReadContext state;
        history(r, r.a, state);
        const unsigned from = r.wire.sentCount;
        const E::StopPolicy policy = stopPolicy(r.now, 300, 400, 100, 100, 1000);
        CHECK(r.a.requestStop(L::StopKind::HALT, state, policy, r.now) == E::StopPath::CONFIRMED);
        const L::ActionContext& stop = r.a.stopAction(L::StopKind::HALT);
        r.steps(14);
        CHECK(stop.observations == 1 && stop.phase == L::ActionPhase::WAITING);
        const Saved<C::SdoCompletion> command(stop.commandHistory[0]);
        const unsigned sent = r.wire.sentCount;
        CHECK(sent == from + 6);
        if (event == 0) {
            r.turn(policy.deadlineUs - 1);
            CHECK(stop.state == M::ActionState::ACTIVE);
            r.turn(policy.deadlineUs);
            CHECK(stop.outcome == M::ActionOutcome::DEADLINE);
        } else {
            if (event == 1)
                r.later(frame(0x701, 1));
            else
                CHECK(r.owner.reportController(C::ControllerState::BUS_OFF, r.now));
            r.step();
            CHECK(stop.outcome == M::ActionOutcome::INVALIDATED);
            CHECK(!r.a.latestControl());
        }
        CHECK(stop.state == M::ActionState::FAILED && stop.execution == M::ActionExecution::ACKNOWLEDGED &&
              stop.completion == M::ActionCompletion::NOT_OBSERVED && stop.uncertain);
        command.unchanged(stop.commandHistory[0]);
        CHECK(r.a.stopPath() == E::StopPath::FINISHED && r.a.bestEffort().state == M::ActionState::EMPTY);
        quiet(r, sent);
    }
}
// A service reply is never this path's terminal, UNSUPPORTED included.
void unsupportedTerminalIsRejected() {
    L::BestEffortStop stop;
    CHECK(L::prepareBestEffortStop(stop, target(), 5, 10, 20, reads()));
    CHECK(L::noteBestEffortAdmission(stop, 11));
    const C::SdoCompletion terminal = stopTerminal(stop, C::SdoOutcome::UNSUPPORTED, true, 12);
    const Saved<L::BestEffortStop> before(stop);
    CHECK(!L::advanceBestEffortStop(stop, terminal, 13));
    before.unchanged(stop);
}

// Options shared with identity reads cannot refuse a stop.
void identityWidthCannotRefuseStop() {
    for (unsigned withHistory = 0; withHistory < 2; ++withHistory) {
        Rig r;
        openAxis(r, r.a);
        L::ReadContext state;
        if (withHistory)
            history(r, r.a, state);
        E::StopPolicy policy = stopPolicy(r.now);
        policy.options.service.identityWidths = L::IdentityWidths::MEASURED_U8;
        const unsigned from = r.wire.sentCount;
        CHECK(r.a.requestStop(L::StopKind::QUICK_STOP, state, policy, r.now) != E::StopPath::NONE);
        r.step();
        CHECK(r.wire.sentCount == from + 1 && isWord(r, from, WORD_QUICK));
    }
}
void invalidKindRefuses() {
    Rig r;
    openAxis(r, r.a);
    L::ReadContext state;
    const unsigned from = r.wire.sentCount;
    CHECK(r.a.requestStop(static_cast<L::StopKind>(255), state, stopPolicy(r.now), r.now) == E::StopPath::NONE);
    r.step();
    CHECK(r.wire.sentCount == from && r.a.stopPath() == E::StopPath::NONE);
}

#define RUN(name) (scenario = #name, name())
// A state read held inside the slot this request overwrites counts as missing. The retained
// stop stays whole and the request takes the unsequenced branch.
void stateHeldInTheSlotCountsAsMissing() {
    Rig r;
    openAxis(r, r.a);
    r.drive[0].monitor = 0;
    L::ReadContext state;
    history(r, r.a, state);
    CHECK(r.a.requestStop(L::StopKind::QUICK_STOP, state, stopPolicy(r.now), r.now) == E::StopPath::CONFIRMED);
    finish(r, r.a);
    write(r, r.a, 0x6040, WORD_ENABLE);
    const L::ActionContext& slot = r.a.stopAction(L::StopKind::QUICK_STOP);
    CHECK(slot.read.state == M::ReadState::SUCCEEDED);
    const Saved<L::ActionContext> retained(slot);
    const E::StopPath path = r.a.requestStop(L::StopKind::QUICK_STOP, slot.read, stopPolicy(r.now), r.now);
    CHECK(path == E::StopPath::BEST_EFFORT || path == E::StopPath::HELD);
    retained.unchanged(slot);
    finish(r, r.a);
    retained.unchanged(slot);
}
// A sequenced stop the owner refuses changes nothing, including an earlier stop's history.
void ownerRefusalKeepsRetainedStop() {
    Rig r;
    openAxis(r, r.a);
    r.drive[0].monitor = 0;
    L::ReadContext state;
    history(r, r.a, state);
    CHECK(r.a.requestStop(L::StopKind::QUICK_STOP, state, stopPolicy(r.now), r.now) == E::StopPath::CONFIRMED);
    finish(r, r.a);
    write(r, r.a, 0x6040, WORD_ENABLE);
    readState(r, r.a, state);
    const uint64_t at = r.now;
    // The owner has seen a later time than the session asks with, so it refuses the operation.
    CHECK(r.owner.service(at + 5, budget()));
    const L::ActionContext& slot = r.a.stopAction(L::StopKind::QUICK_STOP);
    CHECK(slot.state == M::ActionState::SUCCEEDED);
    const Saved<E::AxisSession> session(r.a);
    const unsigned sent = r.wire.sentCount;
    CHECK(r.a.requestStop(L::StopKind::QUICK_STOP, state, stopPolicy(at + 1), at + 1) == E::StopPath::NONE);
    session.unchanged(r.a);
    CHECK(r.wire.sentCount == sent);
}
void allScenarios() {
    RUN(triggerPollThenStop);
    RUN(lostTriggerThenBestEffort);
    RUN(lostPollWhileStopWaits);
    RUN(queuedStopAtExactQueueCutoff);
    RUN(sequencedOverallDeadlineEquality);
    RUN(bestEffortDeadlineEquality);
    RUN(busOffThenRecoveryThenOnce000B);
    RUN(recoveryInterruptingQueuedStop);
    RUN(failedOrSecondRecoveryEndsNotTransmitted);
    RUN(possibleWriteIsNeverResent);
    RUN(bootEndsStopWithNoTransmission);
    RUN(withdrawnHistoryStaysWithdrawn);
    RUN(writeSettlingAfterStopDoesNotRestore);
    RUN(idleAtEnableStillStops);
    RUN(twoNodePressure);
    RUN(escalationFromQueuedHalt);
    RUN(escalationFromInFlightHalt);
    RUN(escalationFromObservation);
    RUN(prepareEscalationUnits);
    RUN(refusedEscalationsChangeNothing);
    RUN(stoppedAndReleasedRefuse);
    RUN(zeroBestEffortRefusalKeepsQueuedWork);
    RUN(probeReservationPassesToSession);
    RUN(stopInterruptsProbeRead);
    RUN(stopFromAnotherProducerAdopts);
    RUN(noReservationStillSendsBestEffort);
    RUN(coreBestEffortUnits);
    RUN(freshnessBudgetBoundaries);
    RUN(realServiceGapStillExpires);
    RUN(alarmErrorsReadOnce);
    RUN(alarmErrorsFailureKeepsMonitorObservation);
    RUN(sessionLifecycle);
    RUN(onlyFreshCurrentStateQualifies);
    RUN(stopRequestedDuringAFault);
    RUN(idleOutOfScopeRefuses);
    RUN(supersededHaltSettlesAfterFinished);
    RUN(invalidOptionsRefuseAtRequest);
    RUN(bestEffortHeldByFault);
    RUN(heldStopKeepsItsDeadline);
    RUN(oneRecoveryFromTheRequest);
    RUN(operationIdentityBoundary);
    RUN(busyExcludesReadsAndOperations);
    RUN(partialStateDecidesWhatItCan);
    RUN(identityWidthCannotRefuseStop);
    RUN(invalidKindRefuses);
    RUN(unsupportedTerminalIsRejected);
    RUN(stateHeldInTheSlotCountsAsMissing);
    RUN(ownerRefusalKeepsRetainedStop);
    RUN(waitingStopEnds);
}
} // namespace

int main() {
    allScenarios();
    asyncTraffic = true;
    allScenarios();
    std::printf("axis session: %u checks\n", checks);
    return 0;
}
