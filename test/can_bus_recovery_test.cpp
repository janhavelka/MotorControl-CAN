// SPDX-License-Identifier: MIT
// Synthetic server/controller timelines prove local ownership, never a drive stop.
#include "CanBusOwner.h"
#include <MotorControlCAN/profiles/lichuan_c/Pdo.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>

namespace M = MotorControlCAN;
namespace C = MotorControlCAN::CANOPEN;
namespace E = MotorControlCANExample;
namespace L = MotorControlCAN::LICHUAN_C;
namespace {
unsigned checks = 0;
#define CHECK(c)                                                                                   \
    do {                                                                                           \
        ++checks;                                                                                  \
        if (!(c)) {                                                                                \
            std::fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #c);                           \
            std::abort();                                                                          \
        }                                                                                          \
    } while (false)
template <class T> struct Saved {
    unsigned char bytes[sizeof(T)];
    explicit Saved(const T& value) { std::memcpy(bytes, &value, sizeof value); }
    void unchanged(const T& value) const { CHECK(!std::memcmp(bytes, &value, sizeof value)); }
};
C::SdoChannel route(unsigned node = 1) {
    C::SdoChannel out;
    out.binding.logicalAxisId = 100 + node;
    out.binding.endpoint.profile = 41;
    out.binding.endpoint.address = node;
    out.binding.bindingGeneration = 3;
    out.binding.configurationGeneration = 7;
    out.clientToServerId = 0x600 + node;
    out.serverToClientId = 0x580 + node;
    return out;
}
M::Frame frame(uint32_t id, uint8_t length = 8) {
    M::Frame out;
    out.id = id;
    out.dlc = out.payloadLength = length;
    return out;
}
M::Frame reply(unsigned node = 1, bool download = false, uint16_t index = 0x2345) {
    M::Frame out = frame(0x580 + node);
    out.data[0] = download ? 0x60 : 0x4b;
    out.data[1] = uint8_t(index);
    out.data[2] = uint8_t(index >> 8);
    out.data[4] = 0x34;
    out.data[5] = 0x12;
    return out;
}
E::ServiceBudget budget(uint8_t rx = 8, uint8_t tx = 2, uint8_t events = 8, uint8_t starts = 2,
                        uint8_t recovery = 0) {
    E::ServiceBudget out;
    out.rx = rx;
    out.tx = tx;
    out.events = events;
    out.starts = starts;
    out.recovery = recovery;
    return out;
}
struct Fake {
    enum { CAPACITY = 64 };
    E::ReceivedFrame incoming[CAPACITY];
    E::TxReport reports[CAPACITY];
    M::Frame sent[CAPACITY];
    E::TransportTxId ids[CAPACITY];
    uint32_t epochs[CAPACITY] = {};
    bool pending[CAPACITY] = {};      // Controller queue ownership, separate from enqueue history.
    unsigned departed[CAPACITY] = {}; // Enqueue indices which actually left the fake controller.
    unsigned departedCount = 0;
    unsigned rxRead = 0, rxWrite = 0, reportRead = 0, reportWrite = 0, sentCount = 0;
    unsigned receives = 0, reportCalls = 0, observations = 0, retireCalls = 0;
    unsigned begins = 0, polls = 0;
    uint32_t recoveryEpoch = 0;
    bool accept = true, retirement = false, startRecovery = true;
    E::RecoveryProgress progress = E::RecoveryProgress::PENDING;
    E::TransportTxId retired;
    uint32_t retiredEpoch = 0;
    static bool receive(void* p, E::ReceivedFrame& output) {
        Fake& f = *static_cast<Fake*>(p);
        ++f.receives;
        if (f.rxRead == f.rxWrite)
            return false;
        output = f.incoming[f.rxRead++];
        return true;
    }
    static bool transmit(void* p, const M::Frame& value, E::TransportTxId id, uint32_t epoch) {
        Fake& f = *static_cast<Fake*>(p);
        CHECK(f.sentCount < CAPACITY);
        f.sent[f.sentCount] = value;
        f.ids[f.sentCount] = id;
        // A false enqueue return deliberately retains possible queue acceptance.
        f.pending[f.sentCount] = true;
        f.epochs[f.sentCount++] = epoch;
        return f.accept;
    }
    static bool takeReport(void* p, E::TxReport& output) {
        Fake& f = *static_cast<Fake*>(p);
        ++f.reportCalls;
        if (f.reportRead == f.reportWrite)
            return false;
        output = f.reports[f.reportRead++];
        return true;
    }
    static void observe(void* p, const E::ReceivedFrame&, uint64_t) {
        ++static_cast<Fake*>(p)->observations;
    }
    static bool retire(void* p, E::TransportTxId id, uint32_t epoch) {
        Fake& f = *static_cast<Fake*>(p);
        ++f.retireCalls;
        f.retired = id;
        f.retiredEpoch = epoch;
        if (f.retirement) {
            for (unsigned i = 0; i < f.sentCount; ++i)
                if (f.epochs[i] == epoch && f.ids[i].sdo.request.owner == id.sdo.request.owner &&
                    f.ids[i].sdo.request.channel == id.sdo.request.channel &&
                    f.ids[i].sdo.request.generation == id.sdo.request.generation &&
                    f.ids[i].sdo.frame == id.sdo.frame)
                    f.pending[i] = false;
            unsigned write = f.reportRead;
            for (unsigned i = f.reportRead; i < f.reportWrite; ++i) {
                const E::TxReport& report = f.reports[i];
                if (report.controllerEpoch == epoch &&
                    report.id.sdo.request.owner == id.sdo.request.owner &&
                    report.id.sdo.request.channel == id.sdo.request.channel &&
                    report.id.sdo.request.generation == id.sdo.request.generation &&
                    report.id.sdo.frame == id.sdo.frame)
                    continue;
                f.reports[write++] = report;
            }
            f.reportWrite = write;
        }
        return f.retirement;
    }
    static bool begin(void* p, uint32_t epoch) {
        Fake& f = *static_cast<Fake*>(p);
        ++f.begins;
        f.recoveryEpoch = epoch;
        return f.startRecovery;
    }
    static E::RecoveryProgress poll(void* p, uint32_t epoch) {
        Fake& f = *static_cast<Fake*>(p);
        CHECK(epoch == f.recoveryEpoch);
        ++f.polls;
        if (f.progress == E::RecoveryProgress::RETIRED) {
            // This fake has no asynchronous callbacks. Empty every local queue
            // before claiming readiness for the new epoch; nothing is replayed.
            for (unsigned i = 0; i < f.sentCount; ++i)
                f.pending[i] = false;
            f.rxRead = f.rxWrite;
            f.reportRead = f.reportWrite;
        }
        return f.progress;
    }
    E::Transport transport(bool withRetire = true) {
        E::Transport out;
        out.context = this;
        out.receive = receive;
        out.transmit = transmit;
        out.takeTxReport = takeReport;
        out.observe = observe;
        out.retire = withRetire ? retire : nullptr;
        out.beginRecovery = begin;
        out.pollRecovery = poll;
        return out;
    }
    void push(const M::Frame& value, uint64_t captured, uint32_t epoch = 1) {
        CHECK(rxWrite < CAPACITY);
        E::ReceivedFrame& f = incoming[rxWrite++];
        f.frame = value;
        f.receivedTimeKnown = true;
        f.receivedUs = captured;
        f.controllerEpoch = epoch;
    }
    void queueReport(unsigned sentIndex, bool transmitted, uint32_t epoch) {
        CHECK(sentIndex < sentCount && reportWrite < CAPACITY);
        E::TxReport& r = reports[reportWrite++];
        r.id = ids[sentIndex];
        r.transmitted = transmitted;
        r.controllerEpoch = epoch;
    }
    bool physicalDrain(unsigned sentIndex, bool reportReady = true) {
        CHECK(sentIndex < sentCount);
        if (!pending[sentIndex])
            return false;
        CHECK(departedCount < CAPACITY);
        departed[departedCount++] = sentIndex;
        pending[sentIndex] = false;
        if (reportReady)
            queueReport(sentIndex, true, epochs[sentIndex]);
        return true;
    }
    unsigned pendingCount() const {
        unsigned count = 0;
        for (unsigned i = 0; i < sentCount; ++i)
            count += pending[i] ? 1 : 0;
        return count;
    }
    void report(unsigned sentIndex, bool transmitted = true, uint32_t epoch = 1) {
        CHECK(sentIndex < sentCount);
        if (epoch == epochs[sentIndex]) {
            if (transmitted)
                physicalDrain(sentIndex, false);
            else
                pending[sentIndex] = false;
        }
        // Wrong epochs deliberately inject a stale report; they cannot remove
        // the actual queued frame. A repeated report never causes physical TX.
        queueReport(sentIndex, transmitted, epoch);
    }
};
struct Rig {
    C::SdoClient services;
    Fake fake;
    E::CanBusOwner owner;
    uint64_t now = 0;
    explicit Rig(uint32_t serial = 0, uint32_t epoch = 1, bool retirement = true)
        : owner(services, fake.transport(retirement), 73, serial, epoch) {
        for (uint8_t i = 0; i < 2; ++i) {
            CHECK(services.configure(i, route(i + 1), true, true));
            C::NetworkRoute n;
            n.binding = route(i + 1).binding;
            n.generation = 11;
            n.heartbeatId = 0x701 + i;
            n.emcyId = 0x81 + i;
            CHECK(services.configureNetwork(i, n, C::NetworkObserver(), 0));
        }
    }
    void turn(uint64_t time, E::ServiceBudget b = budget()) {
        now = time;
        CHECK(owner.service(time, b));
    }
    E::SequenceId sequence(unsigned node = 1, uint8_t producer = 1, uint64_t deadline = 10000) {
        E::SequenceId out;
        CHECK(owner.beginSequence(route(node).binding, producer, now, out));
        CHECK(owner.beginOperation(out, 1, deadline, now));
        return out;
    }
    E::BusRequest request(E::SequenceId sequence, uint32_t step = 1, bool download = false,
                          uint16_t index = 0x2345) {
        const E::Sequence* s = owner.sequence(sequence);
        CHECK(s);
        E::BusRequest out;
        out.sequence = sequence;
        out.transfer.channel = route(s->binding.endpoint.address);
        out.transfer.channel.binding = s->binding;
        out.transfer.operation = out.transfer.request = s->operation;
        out.transfer.step = step;
        out.transfer.index = index;
        out.transfer.size = 2;
        out.transfer.direction = download ? C::SdoDirection::DOWNLOAD : C::SdoDirection::UPLOAD;
        out.transfer.readEffect = C::SdoReadEffect::REVIEWED_NON_CHANGING;
        out.transfer.queueTimeoutUs = out.transfer.responseTimeoutUs = 1000;
        out.transfer.deadlineUs = s->deadlineUs;
        out.transfer.data[0] = 0xa5;
        out.transfer.data[1] = 0x5a;
        return out;
    }
    E::RequestId admit(const E::BusRequest& input, bool urgent = false) {
        E::RequestId out;
        CHECK((urgent ? owner.admitUrgent(input, now, out) : owner.admit(input, now, out)) ==
              E::BusAdmission::ACCEPTED);
        return out;
    }
    const E::Completion& done(E::RequestId id) {
        const E::Completion* out = owner.result(id);
        CHECK(out);
        return *out;
    }
    void complete(E::RequestId id, unsigned node = 1, bool download = false,
                  uint16_t index = 0x2345) {
        fake.report(fake.sentCount - 1);
        fake.push(reply(node, download, index), now + 1);
        turn(now + 1, budget(8, 0, 8, 0));
        CHECK(done(id).service.outcome == C::SdoOutcome::SUCCESS);
    }
};

void urgentCapacityAndPriority() {
    Rig r;
    const auto ordinary = r.sequence(1, 1), urgent = r.sequence(2, 4);
    E::RequestId pending[E::CanBusOwner::REQUESTS];
    for (unsigned i = 0; i < E::CanBusOwner::REQUESTS; ++i)
        pending[i] = r.admit(r.request(ordinary, i + 1));
    E::RequestId rejected;
    rejected.generation = 987;
    const Saved<E::RequestId> unchanged(rejected);
    CHECK(r.owner.admit(r.request(ordinary, 7), 0, rejected) == E::BusAdmission::RESULTS_FULL);
    unchanged.unchanged(rejected);
    const auto reserved = r.admit(r.request(urgent), true);
    CHECK(reserved.slot >= E::CanBusOwner::REQUESTS);
    CHECK(r.owner.admitUrgent(r.request(urgent, 2), 0, rejected) == E::BusAdmission::RESULTS_FULL);
    unchanged.unchanged(rejected);
    r.turn(0, budget(0, 1, 0, 1));
    CHECK(r.fake.sentCount == 1 && r.fake.sent[0].id == 0x602);
    r.complete(reserved, 2);
    CHECK(r.owner.beginOperation(urgent, 2, 10000, r.now));
    CHECK(r.owner.admitUrgent(r.request(urgent), r.now, rejected) == E::BusAdmission::RESULTS_FULL);
    CHECK(r.owner.release(reserved));
    const auto reused = r.admit(r.request(urgent), true);
    CHECK(reused.slot == reserved.slot && reused.generation != reserved.generation);
    CHECK(!r.owner.result(reserved));
    CHECK(r.owner.cancel(reused, r.now) == E::Cancel::CANCELLED);
    for (const auto id : pending)
        CHECK(r.owner.cancel(id, r.now) == E::Cancel::CANCELLED);
    CHECK(r.fake.sentCount == 1);

    // Six unread ordinary successes occupy all ordinary slots. New operation
    // admission still has a separate urgent terminal reservation.
    Rig full;
    const auto sequence = full.sequence();
    E::RequestId results[E::CanBusOwner::REQUESTS];
    for (unsigned i = 0; i < E::CanBusOwner::REQUESTS; ++i) {
        results[i] = full.admit(full.request(sequence, i + 1));
        full.turn(full.now, budget(0, 1, 0, 1));
        full.complete(results[i]);
    }
    const Saved<E::Completion> first(full.done(results[0]));
    CHECK(full.owner.beginOperation(sequence, 2, 10000, full.now));
    CHECK(full.owner.admit(full.request(sequence), full.now, rejected) ==
          E::BusAdmission::RESULTS_FULL);
    const auto extra = full.admit(full.request(sequence), true);
    full.turn(full.now, budget(0, 1, 0, 1));
    full.complete(extra);
    first.unchanged(full.done(results[0]));
    CHECK(full.fake.sentCount == E::CanBusOwner::TOTAL_REQUESTS);
}

void urgentKeepsSequenceOrder() {
    Rig r;
    const auto sequence = r.sequence();
    const auto first = r.admit(r.request(sequence, 1, false, 0x2345));
    const auto urgent = r.admit(r.request(sequence, 2, false, 0x2346), true);
    const auto last = r.admit(r.request(sequence, 3, false, 0x2347));
    r.turn(0, budget(0, 1, 0, 1));
    CHECK(r.fake.sent[0].data[1] == 0x45 && !r.owner.result(urgent));
    CHECK(!r.owner.beginOperation(sequence, 2, 10000, 0));
    r.complete(first);
    r.turn(r.now, budget(0, 1, 0, 1));
    CHECK(r.fake.sentCount == 2 && r.fake.sent[1].data[1] == 0x46);
    r.complete(urgent, 1, false, 0x2346);
    r.turn(r.now, budget(0, 1, 0, 1));
    CHECK(r.fake.sentCount == 3 && r.fake.sent[2].data[1] == 0x47);
    r.complete(last, 1, false, 0x2347);
}

void cancellationPhasesAndTerminalPrecedence() {
    for (unsigned phase = 0; phase < 4; ++phase) {
        Rig r;
        r.fake.retirement = true;
        const auto sequence = r.sequence();
        const auto id = r.admit(r.request(sequence, 1, true));
        if (phase) {
            r.turn(0, budget(0, 1, 0, 1));
            if (phase >= 2) {
                r.fake.report(0);
                r.turn(1, budget(0, 0, 1, 0));
            }
            if (phase == 3) {
                r.fake.push(reply(1, true), 2);
                r.turn(2, budget(1, 0, 0, 0));
            }
        }
        if (phase == 3) {
            const Saved<E::Completion> prior(r.done(id));
            CHECK(r.owner.cancel(id, r.now) == E::Cancel::ALREADY_TERMINAL);
            prior.unchanged(r.done(id));
            CHECK(r.done(id).service.protocolAcknowledged);
        } else {
            CHECK(r.owner.cancel(id, r.now) == E::Cancel::CANCELLED);
            const auto& terminal = r.done(id);
            CHECK(terminal.reason == E::CompletionReason::CANCELLED);
            CHECK(terminal.service.outcome == C::SdoOutcome::CANCELLED);
            CHECK(terminal.service.transmissionPossible == (phase != 0));
            CHECK(terminal.service.transmissionConfirmed == (phase == 2));
            CHECK(terminal.service.effectUncertain == (phase != 0));
            CHECK(!terminal.service.protocolAcknowledged);
            const Saved<E::Completion> saved(terminal);
            r.fake.push(reply(1, true), r.now + 1);
            r.turn(r.now + 1, budget(1, 0, 8, 0));
            saved.unchanged(r.done(id));
            CHECK(r.owner.cancel(id, r.now) == E::Cancel::ALREADY_TERMINAL);
        }
        CHECK(r.fake.sentCount == unsigned(phase != 0));
        CHECK(r.fake.retireCalls == unsigned(phase == 1));
    }
    Rig deadline;
    const auto sequence = deadline.sequence(1, 1, 10);
    const auto id = deadline.admit(deadline.request(sequence));
    CHECK(deadline.owner.cancel(id, 10) == E::Cancel::ALREADY_TERMINAL);
    CHECK(deadline.done(id).reason == E::CompletionReason::DEADLINE);
    CHECK(!deadline.done(id).service.transmissionPossible && deadline.fake.sentCount == 0);

    // The adapter has staged a segment after initiation, but TX budget is zero.
    // Cancellation retires it before any later turn can hand it to the controller.
    Rig staged;
    const auto s = staged.sequence();
    auto request = staged.request(s, 1, true);
    request.transfer.size = 8;
    const auto segment = staged.admit(request);
    staged.turn(0, budget(0, 1, 0, 1));
    staged.fake.report(0);
    staged.fake.push(reply(1, true), 1);
    staged.turn(1, budget(1, 0, 1, 0));
    CHECK(staged.services.port(0)->queueOccupied);
    CHECK(staged.owner.cancel(segment, 1) == E::Cancel::CANCELLED);
    staged.turn(2, budget(0, 2, 8, 2));
    CHECK(staged.fake.sentCount == 1);
    for (unsigned i = 1; i < staged.fake.sentCount; ++i)
        CHECK(staged.fake.sent[i].data[0] == 0x80); // Only optional native abort cleanup.
    CHECK(staged.done(segment).service.effectUncertain);
}

void cleanupHandoffNeedsRetirement() {
    Rig r;
    const auto sequence = r.sequence();
    const auto id = r.admit(r.request(sequence));
    r.turn(0, budget(0, 1, 0, 1));
    r.fake.report(0);
    M::Frame malformed = reply();
    malformed.dlc = malformed.payloadLength = 7;
    r.fake.push(malformed, 1);
    r.turn(1, budget(1, 1, 1, 0));
    CHECK(r.done(id).service.outcome == C::SdoOutcome::CLIENT_ABORT);
    CHECK(r.fake.sentCount == 2 && r.fake.sent[1].data[0] == 0x80);
    const Saved<E::Completion> saved(r.done(id));
    // Protocol abort cleanup has the same external queue ownership as work.
    CHECK(r.owner.reportController(C::ControllerState::BUS_OFF, 2));
    CHECK(r.fake.retireCalls == 1 && r.fake.retired.sdo.frame == r.fake.ids[1].sdo.frame);
    r.turn(3, budget());
    CHECK(r.fake.sentCount == 2);
    saved.unchanged(r.done(id));
}

void captureTimeCannotRewriteAnExpiredTerminal() {
    for (unsigned capturedEarly = 0; capturedEarly < 2; ++capturedEarly) {
        Rig r;
        const auto sequence = r.sequence(1, 1, 10);
        const auto id = r.admit(r.request(sequence, 1, true));
        r.turn(0, budget(0, 1, 0, 1));
        r.fake.report(0);
        r.turn(1, budget(0, 0, 1, 0));
        r.fake.push(reply(1, true), capturedEarly ? 9 : 10);
        r.turn(10, budget(1, 0, 0, 0));
        CHECK(r.done(id).service.outcome == C::SdoOutcome::TIMEOUT);
        CHECK(!r.done(id).service.protocolAcknowledged && r.done(id).service.effectUncertain);
        const Saved<E::Completion> saved(r.done(id));
        CHECK(r.owner.cancel(id, 10) == E::Cancel::ALREADY_TERMINAL);
        CHECK(r.owner.cancel(id, 9) == E::Cancel::INVALID);
        r.fake.push(reply(1, true), 11);
        r.turn(11, budget(1, 0, 0, 0));
        saved.unchanged(r.done(id));
        CHECK(r.fake.sentCount == 1);
    }
}

void unprovedRetirementBlocksOnlyItsChannel() {
    for (unsigned callback = 0; callback < 2; ++callback) {
        Rig r(0, 1, callback != 0);
        const auto a = r.sequence(1, 1), b = r.sequence(2, 2);
        const auto first = r.admit(r.request(a, 1, true));
        const auto urgent = r.admit(r.request(a, 2, true, 0x2346), true);
        const auto other = r.admit(r.request(b));
        r.turn(0, budget(0, 1, 0, 1));
        CHECK(r.fake.sentCount == 1 && r.fake.sent[0].id == 0x601);
        CHECK(r.owner.cancel(first, 1) == E::Cancel::CANCELLED);
        CHECK(r.done(first).service.effectUncertain);
        M::Frame heartbeat = frame(0x702, 1);
        heartbeat.data[0] = 5;
        r.fake.push(heartbeat, 2);
        r.turn(2, budget(1, 1, 8, 1));
        CHECK(r.done(urgent).reason == E::CompletionReason::INVALIDATED);
        CHECK(r.fake.sentCount == 2 && r.fake.sent[1].id == 0x602);
        CHECK(r.services.node(1)->heartbeat.present &&
              r.services.node(1)->nmt == C::NmtState::OPERATIONAL);
        r.complete(other, 2);
        r.turn(999, budget());
        CHECK(r.fake.retireCalls == callback && r.services.unresolved(0));
        for (unsigned i = 1; i < r.fake.sentCount; ++i)
            CHECK(r.fake.sent[i].id != 0x601);
        CHECK(r.done(first).service.effectUncertain);
    }
}

void groupInvalidationDoesNotCancelOtherProducer() {
    for (unsigned group = 0; group < 2; ++group) {
        Rig r;
        const auto a = r.sequence(1, 1), b = r.sequence(2, 2);
        const auto first = r.admit(r.request(a));
        const auto next = r.admit(r.request(a, 2), true);
        const auto other = r.admit(r.request(b));
        CHECK(group ? r.owner.invalidateProducer(1, 0) : r.owner.invalidate(a, 0));
        CHECK(!r.owner.sequence(a) && r.owner.sequence(b));
        CHECK(r.done(first).reason == E::CompletionReason::INVALIDATED);
        CHECK(r.done(next).reason == E::CompletionReason::INVALIDATED);
        r.turn(0, budget(0, 1, 0, 1));
        CHECK(r.fake.sentCount == 1 && r.fake.sent[0].id == 0x602);
        r.complete(other, 2);
    }
}

void staleQueuedFramesAfterBoundaries() {
    for (unsigned boundary = 0; boundary < 3; ++boundary) {
        Rig r;
        const auto sequence = r.sequence();
        const auto first = r.admit(r.request(sequence, 1, true));
        const auto queued = r.admit(r.request(sequence, 2, true, 0x2346));
        r.turn(0, budget(0, 1, 0, 1));
        if (boundary == 0)
            CHECK(r.owner.reportController(C::ControllerState::BUS_OFF, 1));
        else if (boundary == 1) {
            M::Frame boot = frame(0x701, 1);
            r.fake.push(boot, 1);
            r.turn(1, budget(1, 0, 8, 0));
        } else {
            CHECK(r.services.invalidateNetwork(0, C::NetworkInvalidation::BINDING, 1));
            r.turn(1, budget(0, 0, 8, 0));
        }
        CHECK(r.owner.result(first) && r.owner.result(queued));
        CHECK(r.done(first).service.effectUncertain &&
              !r.done(queued).service.transmissionPossible);
        CHECK(r.fake.retireCalls == 1 && r.fake.retiredEpoch == 1);
        CHECK(r.fake.retired.sdo.frame == r.fake.ids[0].sdo.frame);
        const Saved<E::Completion> interrupted(r.done(first));
        r.fake.report(0);
        r.fake.push(reply(1, true), 2);
        r.turn(2, budget(1, 2, 8, 2));
        r.turn(5000, budget());
        interrupted.unchanged(r.done(first));
        CHECK(r.fake.sentCount == 1 && r.services.unresolved(0));
        C::SdoChannel replacement = route();
        ++replacement.binding.bindingGeneration;
        CHECK(!r.services.configure(0, replacement, true, true));
    }
}

void refusedEnqueueStillNeedsRetirement() {
    Rig r;
    r.fake.accept = false;
    const auto sequence = r.sequence();
    const auto id = r.admit(r.request(sequence, 1, true));
    r.turn(0, budget(0, 1, 0, 1));
    CHECK(r.done(id).service.outcome == C::SdoOutcome::TRANSPORT_ERROR);
    CHECK(!r.done(id).controllerAccepted && r.done(id).service.effectUncertain);
    r.turn(1, budget(0, 2, 8, 2));
    r.turn(2, budget());
    CHECK(r.fake.sentCount == 1 && r.fake.retireCalls == 1 && r.services.unresolved(0));
    const Saved<E::Completion> retained(r.done(id));
    r.fake.report(0, false);
    r.turn(3, budget(0, 2, 8, 2));
    retained.unchanged(r.done(id));
    CHECK(r.fake.sentCount == 1);
}

void exactRetirementControlsPhysicalDeparture() {
    for (unsigned positive = 0; positive < 2; ++positive) {
        Rig r;
        r.fake.retirement = positive != 0;
        const auto sequence = r.sequence();
        const auto original = r.admit(r.request(sequence, 1, true));
        const auto later = r.admit(r.request(sequence, 2, true, 0x2346), true);
        r.turn(0, budget(0, 1, 0, 1));
        CHECK(r.fake.pendingCount() == 1 && r.fake.departedCount == 0);
        CHECK(r.owner.cancel(original, 1) == E::Cancel::CANCELLED);
        CHECK(r.fake.retireCalls == 1 && r.fake.pendingCount() == unsigned(!positive));
        r.turn(1, budget());
        CHECK(r.done(later).reason == E::CompletionReason::INVALIDATED);
        CHECK(r.fake.sentCount == 1);
        const Saved<E::Completion> saved(r.done(original));
        CHECK(r.fake.physicalDrain(0) == !positive);
        CHECK(r.fake.departedCount == unsigned(!positive));
        if (!positive) {
            // The unretired original can still leave; it is never new intent.
            CHECK(r.fake.departed[0] == 0 && r.fake.sent[0].data[1] == 0x45);
            r.turn(2, budget(0, 2, 8, 2));
            CHECK(r.services.unresolved(0) && r.fake.sentCount == 1);
        } else {
            // Another channel can admit fresh work after exact retirement.
            const auto other = r.sequence(2, 2);
            const auto fresh = r.admit(r.request(other, 1, true, 0x2347));
            r.turn(2, budget(0, 1, 0, 1));
            CHECK(r.fake.sentCount == 2 && r.fake.pendingCount() == 1);
            CHECK(!r.fake.physicalDrain(0) && r.fake.physicalDrain(1));
            CHECK(r.fake.departedCount == 1 && r.fake.departed[0] == 1);
            CHECK(r.fake.sent[1].id == 0x602 && r.fake.sent[1].data[1] == 0x47);
            r.fake.push(reply(2, true, 0x2347), 3);
            r.turn(3, budget(1, 0, 8, 0));
            CHECK(r.done(fresh).service.protocolAcknowledged);
        }
        saved.unchanged(r.done(original));
        CHECK(r.done(original).service.effectUncertain);
    }

    // Exact retirement must preserve a different channel's pending frame.
    Rig parallel;
    parallel.fake.retirement = true;
    const auto a = parallel.sequence(1, 1), b = parallel.sequence(2, 2);
    const auto cancelled = parallel.admit(parallel.request(a, 1, true));
    const auto unaffected = parallel.admit(parallel.request(b, 1, true));
    parallel.turn(0, budget(0, 2, 0, 2));
    CHECK(parallel.fake.pendingCount() == 2);
    CHECK(parallel.owner.cancel(cancelled, 1) == E::Cancel::CANCELLED);
    CHECK(parallel.fake.pendingCount() == 1 && !parallel.fake.physicalDrain(0));
    CHECK(parallel.fake.physicalDrain(1));
    CHECK(parallel.fake.departedCount == 1 && parallel.fake.departed[0] == 1);
    parallel.fake.push(reply(2, true), 2);
    parallel.turn(2, budget(1, 0, 8, 0));
    CHECK(parallel.done(unaffected).service.protocolAcknowledged);
    CHECK(parallel.done(cancelled).service.effectUncertain);
}

void recoveryActuallyRetiresLocalQueues() {
    for (unsigned outcome = 0; outcome < 4; ++outcome) {
        Rig r;
        const auto a = r.sequence(1, 1), b = r.sequence(2, 2);
        const auto first = r.admit(r.request(a, 1, true));
        const E::BusRequest original = r.request(b, 1, true, 0x2346);
        const auto second = r.admit(original);
        r.turn(0, budget(0, 2, 0, 2));
        CHECK(r.fake.pendingCount() == 2 && r.fake.departedCount == 0);
        // One frame leaves before the boundary; its report remains queued.
        CHECK(r.fake.physicalDrain(0));
        M::Frame heartbeat = frame(0x702, 1);
        heartbeat.data[0] = 5;
        r.fake.push(heartbeat, 0);
        CHECK(r.fake.pendingCount() == 1 && r.fake.reportWrite == 1 && r.fake.rxWrite == 1);
        r.fake.startRecovery = outcome != 1;
        r.fake.progress = outcome == 0   ? E::RecoveryProgress::RETIRED
                          : outcome == 2 ? E::RecoveryProgress::FAILED
                                         : E::RecoveryProgress::PENDING;
        E::RecoveryId recovery;
        CHECK(r.owner.recover(1, 3, recovery));
        const Saved<E::Completion> firstResult(r.done(first));
        const Saved<E::Completion> secondResult(r.done(second));
        r.turn(outcome == 3 ? 3 : 2, budget(0, 0, 0, 0, 1));
        CHECK(r.owner.recoveryResult(recovery));
        if (outcome == 0) {
            CHECK(r.owner.recoveryResult(recovery)->outcome == E::RecoveryOutcome::RETIRED);
            CHECK(r.fake.pendingCount() == 0 && r.fake.rxRead == r.fake.rxWrite);
            CHECK(r.fake.reportRead == r.fake.reportWrite);
            CHECK(!r.fake.physicalDrain(1) && r.fake.departedCount == 1);
            CHECK(r.services.controllerState() == C::ControllerState::AVAILABLE);
        } else {
            CHECK(r.owner.recoveryResult(recovery)->outcome ==
                  (outcome == 3 ? E::RecoveryOutcome::TIMEOUT : E::RecoveryOutcome::FAILED));
            CHECK(r.fake.pendingCount() == 1 && r.fake.rxRead == 0 && r.fake.reportRead == 0);
            CHECK(!r.owner.reportController(C::ControllerState::AVAILABLE, r.now));
            E::RequestId rejected;
            CHECK(r.owner.admit(original, r.now, rejected) == E::BusAdmission::INHIBITED);
            r.turn(4, budget(0, 2, 0, 2));
            CHECK(r.fake.pendingCount() == 1 && r.fake.sentCount == 2);
            // Failed/timed-out host work cannot claim that the old queue vanished.
            CHECK(r.fake.physicalDrain(1));
            CHECK(r.fake.departedCount == 2 && r.fake.departed[1] == 1);
            CHECK(r.fake.sent[1].data[1] == 0x46);
        }
        r.turn(5, budget(8, 2, 8, 2, 1));
        CHECK(r.fake.sentCount == 2 && r.services.unresolved(0) && r.services.unresolved(1));
        CHECK(!r.services.node(1)->heartbeat.present);
        firstResult.unchanged(r.done(first));
        secondResult.unchanged(r.done(second));
    }
}

void repliesBeforeFinalTxReportDoNotLoseRetirement() {
    // Remote protocol evidence may arrive before the controller's final report.
    // Until that report arrives there must still be only one external handoff.
    Rig r;
    const auto sequence = r.sequence();
    auto request = r.request(sequence, 1, true);
    request.transfer.size = 8;
    const auto segmented = r.admit(request);
    r.turn(0, budget(0, 1, 0, 1));
    CHECK(r.fake.physicalDrain(0, false));
    r.fake.push(reply(1, true), 1);
    r.turn(1, budget(1, 2, 0, 0));
    CHECK(r.fake.sentCount == 1 && !r.owner.result(segmented));
    r.turn(2, budget(0, 2, 8, 0));
    CHECK(r.fake.sentCount == 1 && r.fake.retireCalls == 1);
    r.fake.report(0);
    r.turn(3, budget(0, 1, 1, 0));
    CHECK(r.fake.sentCount == 2 && r.fake.ids[0].sdo.frame != r.fake.ids[1].sdo.frame);
    CHECK(r.fake.physicalDrain(1, false));
    M::Frame segmentAck = frame(0x581);
    segmentAck.data[0] = 0x20;
    r.fake.push(segmentAck, 4);
    r.turn(4, budget(1, 2, 0, 0));
    CHECK(r.fake.sentCount == 2 && !r.owner.result(segmented));
    r.fake.report(1);
    r.turn(5, budget(0, 1, 1, 0));
    CHECK(r.fake.sentCount == 3);
    CHECK(r.fake.physicalDrain(2, false));
    segmentAck.data[0] = 0x30;
    r.fake.push(segmentAck, 6);
    r.turn(6, budget(1, 0, 0, 0));
    CHECK(r.done(segmented).service.protocolAcknowledged);
    const Saved<E::Completion> saved(r.done(segmented));
    E::RequestId rejected;
    CHECK(r.owner.admit(r.request(sequence, 2), 6, rejected) == E::BusAdmission::INHIBITED);
    r.turn(7, budget(0, 1, 0, 1));
    CHECK(r.fake.sentCount == 3);
    r.fake.report(2);
    r.turn(8, budget(0, 1, 1, 1));
    const auto next = r.admit(r.request(sequence, 2));
    r.turn(8, budget(0, 1, 0, 1));
    CHECK(r.fake.sentCount == 4 && r.fake.sent[3].data[0] == 0x40);
    r.complete(next);
    saved.unchanged(r.done(segmented));
}

void recoveryHasSeparateCapacityAndFiniteBudget() {
    Rig r;
    const auto sequence = r.sequence();
    E::RequestId pending[E::CanBusOwner::TOTAL_REQUESTS];
    for (unsigned i = 0; i < E::CanBusOwner::TOTAL_REQUESTS; ++i)
        pending[i] = r.admit(r.request(sequence, i + 1, true), i == E::CanBusOwner::REQUESTS);
    r.turn(0, budget(0, 1, 0, 1));
    E::RecoveryId recovery;
    CHECK(r.owner.recover(0, 10, recovery));
    CHECK(r.fake.begins == 1 && r.fake.recoveryEpoch == 2 && !r.owner.recoveryResult(recovery));
    CHECK(!r.owner.releaseRecovery(recovery));
    for (const auto id : pending)
        CHECK(r.done(id).reason == E::CompletionReason::RECOVERY);
    const Saved<E::Completion> interrupted(r.done(pending[0]));
    E::RecoveryId unchanged;
    unchanged.generation = 765;
    const Saved<E::RecoveryId> rejected(unchanged);
    CHECK(!r.owner.recover(0, 20, unchanged));
    rejected.unchanged(unchanged);
    r.turn(0, budget(0, 0, 0, 0, 0));
    CHECK(r.fake.polls == 0);
    r.turn(1, budget(0, 0, 0, 0, 255));
    CHECK(r.fake.polls == 1 && !r.owner.recoveryResult(recovery));
    r.turn(10, budget(0, 0, 0, 0, 255));
    CHECK(r.fake.polls == 1);
    const E::RecoveryCompletion* terminal = r.owner.recoveryResult(recovery);
    CHECK(terminal && terminal->outcome == E::RecoveryOutcome::TIMEOUT);
    CHECK(terminal->startedUs == 0 && terminal->deadlineUs == 10 && terminal->completedUs == 10);
    const Saved<E::RecoveryCompletion> saved(*terminal);
    r.fake.progress = E::RecoveryProgress::RETIRED;
    r.turn(11, budget(8, 2, 8, 2, 255));
    saved.unchanged(*r.owner.recoveryResult(recovery));
    interrupted.unchanged(r.done(pending[0]));
    CHECK(r.fake.polls == 1 && r.fake.sentCount == 1);
    CHECK(!r.owner.reportController(C::ControllerState::AVAILABLE, 11));
    CHECK(r.owner.releaseRecovery(recovery) && !r.owner.releaseRecovery(recovery));
    CHECK(!r.owner.recoveryResult(recovery));
    CHECK(r.owner.recover(11, 20, unchanged));
    CHECK(unchanged.generation != recovery.generation && r.fake.recoveryEpoch == 3);
    r.turn(12, budget(0, 0, 0, 0, 1));
    CHECK(r.owner.recoveryResult(unchanged)->outcome == E::RecoveryOutcome::RETIRED);
    CHECK(r.services.controllerState() == C::ControllerState::AVAILABLE);
    CHECK(r.services.unresolved(0) && r.services.unresolved(1));
    CHECK(r.fake.sentCount == 1 && !r.owner.sequence(sequence));
}

void missingRecoveryHooksRetainFailure() {
    for (unsigned missing = 1; missing <= 3; ++missing) {
        C::SdoClient services;
        Fake fake;
        E::Transport transport = fake.transport();
        if (missing & 1)
            transport.beginRecovery = nullptr;
        if (missing & 2)
            transport.pollRecovery = nullptr;
        E::CanBusOwner owner(services, transport, 73);
        E::RecoveryId id;
        CHECK(owner.recover(0, 10, id));
        const E::RecoveryCompletion* terminal = owner.recoveryResult(id);
        CHECK(terminal && terminal->outcome == E::RecoveryOutcome::FAILED);
        CHECK(terminal->completedUs == 0 && terminal->controllerEpoch == 2);
        const Saved<E::RecoveryCompletion> saved(*terminal);
        CHECK(owner.service(1, budget(8, 2, 8, 2, 1)));
        saved.unchanged(*owner.recoveryResult(id));
        CHECK(fake.begins == 0 && fake.polls == 0 && fake.sentCount == 0);
        E::RecoveryId rejected;
        CHECK(!owner.recover(1, 20, rejected));
        CHECK(owner.releaseRecovery(id) && !owner.recoveryResult(id));
        CHECK(!owner.reportController(C::ControllerState::AVAILABLE, 1));
        E::RequestId work;
        CHECK(owner.admit(E::BusRequest(), 1, work) == E::BusAdmission::INHIBITED);
        CHECK(owner.recover(1, 20, rejected));
        CHECK(rejected.generation != id.generation);
        CHECK(owner.recoveryResult(rejected)->outcome == E::RecoveryOutcome::FAILED);
        CHECK(!owner.recoveryResult(id));
    }
}

void twoVendorBindingsShareRecoveryWithoutInventingEvidence() {
    // Abstract Lichuan C / Nanotec N5 CANopen bindings from Prompt 4's contrast.
    // Profile tags and object bytes are local fixtures, not installed vendor operations.
    C::SdoClient services;
    Fake fake;
    E::CanBusOwner owner(services, fake.transport(), 73);
    E::RequestId ids[2];
    C::SdoChannel bindings[2];
    for (uint8_t channel = 0; channel < 2; ++channel) {
        C::SdoChannel& binding = bindings[channel];
        binding = route(channel + 1);
        binding.binding.endpoint.profile = 41 + channel;
        CHECK(services.configure(channel, binding, true, true));
        C::NetworkRoute network;
        network.binding = binding.binding;
        network.generation = 11;
        network.heartbeatId = 0x701 + channel;
        network.emcyId = 0x81 + channel;
        CHECK(services.configureNetwork(channel, network, C::NetworkObserver(), 0));
    }
    for (uint8_t channel = 0; channel < 2; ++channel) {
        const C::SdoChannel& binding = bindings[channel];
        E::SequenceId sequence;
        CHECK(owner.beginSequence(binding.binding, channel + 1, 0, sequence));
        CHECK(owner.beginOperation(sequence, 1, 100, 0));
        E::BusRequest work;
        work.sequence = sequence;
        work.transfer.channel = binding;
        work.transfer.request = work.transfer.operation = work.transfer.step = 1;
        work.transfer.direction = C::SdoDirection::DOWNLOAD;
        work.transfer.index = 0x2345;
        work.transfer.size = 2;
        work.transfer.data[0] = uint8_t(10 + channel);
        work.transfer.queueTimeoutUs = work.transfer.responseTimeoutUs = 50;
        work.transfer.deadlineUs = 100;
        CHECK(owner.admit(work, 0, ids[channel]) == E::BusAdmission::ACCEPTED);
    }
    CHECK(owner.service(0, budget(0, 2, 0, 2)));
    CHECK(fake.sentCount == 2 && fake.pendingCount() == 2);
    CHECK(fake.ids[0].sdo.request.owner == &services && fake.ids[1].sdo.request.owner == &services);
    CHECK(fake.physicalDrain(0));
    fake.push(reply(1, true), 1);
    CHECK(owner.service(1, budget(1, 0, 8, 0)));
    CHECK(owner.result(ids[0]) && owner.result(ids[0])->service.protocolAcknowledged);
    CHECK(!owner.result(ids[1]));
    const Saved<E::Completion> confirmed(*owner.result(ids[0]));
    E::RecoveryId recovery;
    CHECK(owner.recover(2, 10, recovery));
    const E::Completion* uncertain = owner.result(ids[1]);
    CHECK(uncertain && uncertain->service.effectUncertain);
    CHECK(!uncertain->service.protocolAcknowledged && !uncertain->service.transmissionConfirmed);
    const Saved<E::Completion> interrupted(*uncertain);
    fake.progress = E::RecoveryProgress::RETIRED;
    CHECK(owner.service(3, budget(0, 0, 0, 0, 1)));
    CHECK(owner.recoveryResult(recovery)->outcome == E::RecoveryOutcome::RETIRED);
    CHECK(fake.pendingCount() == 0 && !fake.physicalDrain(1));
    for (uint8_t channel = 0; channel < 2; ++channel) {
        CHECK(owner.result(ids[channel])->service.transfer.channel.binding.endpoint.profile ==
              unsigned(41 + channel));
        CHECK(services.unresolved(channel));
    }
    confirmed.unchanged(*owner.result(ids[0]));
    interrupted.unchanged(*owner.result(ids[1]));
    CHECK(fake.sentCount == 2); // Local recovery sent no write or invented acknowledgement.
}

void wrongEpochIngressAndReportsRemainDiagnostic() {
    Rig unconfirmed;
    const auto unconfirmedSequence = unconfirmed.sequence();
    const auto unconfirmedId = unconfirmed.admit(unconfirmed.request(unconfirmedSequence, 1, true));
    unconfirmed.turn(0, budget(0, 1, 0, 1));
    unconfirmed.fake.report(0, true, 2);
    unconfirmed.turn(1, budget(0, 0, 1, 0));
    CHECK(unconfirmed.owner.cancel(unconfirmedId, 1) == E::Cancel::CANCELLED);
    CHECK(!unconfirmed.done(unconfirmedId).service.transmissionConfirmed);
    CHECK(unconfirmed.fake.retireCalls == 1);

    Rig r;
    const auto sequence = r.sequence();
    const auto id = r.admit(r.request(sequence, 1, true));
    r.turn(0, budget(0, 1, 0, 1));
    r.fake.report(0, true, 2);
    r.fake.push(reply(1, true), 1, 2);
    M::Frame heartbeat = frame(0x702, 1);
    heartbeat.data[0] = 5;
    r.fake.push(heartbeat, 1, 2);
    r.turn(1, budget(8, 0, 8, 0));
    CHECK(!r.owner.result(id) && !r.services.node(1)->heartbeat.present);
    CHECK(r.fake.observations == 2);
    r.fake.report(0, true, 1);
    r.fake.push(reply(1, true), 2, 1);
    r.turn(2, budget(8, 0, 8, 0));
    CHECK(r.done(id).service.transmissionConfirmed && r.done(id).service.protocolAcknowledged);
    const Saved<E::Completion> saved(r.done(id));
    E::RecoveryId recovery;
    CHECK(r.owner.recover(3, 10, recovery));
    r.fake.progress = E::RecoveryProgress::RETIRED;
    r.turn(4, budget(0, 0, 0, 0, 1));
    // Deliberate adapter-contract violation: recovery retired these old local
    // queues, but inject stale epoch-one input afterward to test owner rejection.
    r.fake.report(0, false, 1);
    r.fake.push(heartbeat, 5, 1);
    r.turn(5, budget(8, 2, 8, 2));
    CHECK(!r.services.node(1)->heartbeat.present && r.fake.sentCount == 1);
    saved.unchanged(r.done(id));
    // A new host ingress stamp still cannot establish a new remote SDO session.
    r.fake.push(reply(1, true), 6, 2);
    r.turn(6, budget(8, 2, 8, 2));
    CHECK(r.services.unresolved(0) && r.fake.sentCount == 1);
    saved.unchanged(r.done(id));
}

void recoveryCannotRequalifyPdo() {
    Rig r;
    M::Frame heartbeat = frame(0x701, 1);
    heartbeat.data[0] = 5;
    r.fake.push(heartbeat, 0);
    r.turn(0, budget(1, 0, 0, 0));
    C::PdoMap map;
    CHECK(L::pdoTemplate(C::PdoDirection::TPDO, 2, map));
    C::PdoMapEvidence evidence;
    evidence.route.binding = route().binding;
    evidence.route.generation = 1;
    evidence.route.cobId = 0x281;
    CHECK(C::validatePdoMap(map, L::pdoProfile(41), evidence.route.layout));
    evidence.nodeGeneration = 11;
    evidence.source = C::PdoMapSource::REVIEWED_CONFIGURATION;
    evidence.evidenceId = 100;
    evidence.communicationKnown = true;
    evidence.cobIdParameter = 0x281;
    evidence.transmissionType = 255;
    CHECK(r.services.configurePdo(0, 0, evidence, L::pdoProfile(41), C::PdoObserver(), 0));
    C::PdoQualification proof;
    proof.route = evidence.route;
    proof.nodeGeneration = 11;
    proof.basis = C::PdoSampleBasis::PRODUCER_AND_INGRESS_RETIRED;
    proof.evidenceId = 101;
    CHECK(r.services.qualifyPdo(0, proof, 0));
    r.fake.push(frame(0x281), 1);
    r.turn(1, budget(1, 0, 0, 0));
    CHECK(r.services.pdo(0)->qualified && r.services.pdo(0)->observation.time.present);
    E::RecoveryId recovery;
    CHECK(r.owner.recover(2, 10, recovery));
    CHECK(!r.services.pdo(0)->qualified && !r.services.pdo(0)->confirmed);
    r.fake.progress = E::RecoveryProgress::RETIRED;
    r.turn(3, budget(0, 0, 0, 0, 1));
    r.fake.push(frame(0x281), 4, 2);
    r.turn(4, budget(1, 2, 8, 2));
    r.turn(5000, budget());
    CHECK(!r.services.pdo(0)->qualified && !r.services.pdo(0)->confirmed);
    CHECK(!r.services.qualifyPdo(0, proof, 5000));
    CHECK(r.services.unresolved(0) && r.fake.sentCount == 0);
}

void configurationWaitsForExternalRetirement() {
    Rig r;
    const auto sequence = r.sequence();
    const auto id = r.admit(r.request(sequence, 1, true));
    r.turn(0, budget(0, 1, 0, 1));
    CHECK(r.fake.physicalDrain(0, false));
    r.fake.push(reply(1, true), 1);
    r.turn(1, budget(1, 0, 0, 0));
    CHECK(r.done(id).service.protocolAcknowledged);
    CHECK(r.owner.release(id));
    M::Binding changed = route().binding;
    ++changed.configurationGeneration;
    CHECK(!r.owner.advanceConfiguration(sequence, changed, 1));
    CHECK(r.owner.sequence(sequence)->binding.configurationGeneration == 7);
    r.turn(2, budget(0, 0, 8, 0));
    CHECK(r.fake.retireCalls == 1 && !r.owner.advanceConfiguration(sequence, changed, 2));
    r.fake.report(0);
    r.turn(3, budget(0, 0, 1, 0));
    CHECK(r.owner.advanceConfiguration(sequence, changed, 3));
    CHECK(r.owner.sequence(sequence)->binding.configurationGeneration == 8);
    CHECK(r.fake.sentCount == 1);
}

void unknownControllerWithdrawsFreshness() {
    Rig r;
    CHECK(r.owner.reportController(C::ControllerState::AVAILABLE, 0));
    M::Frame heartbeat = frame(0x701, 1);
    heartbeat.data[0] = 5;
    M::Frame emcy = frame(0x81);
    emcy.data[0] = 0x10;
    emcy.data[1] = 0x23;
    emcy.data[2] = 0x80;
    r.fake.push(heartbeat, 0);
    r.fake.push(emcy, 0);
    r.turn(0, budget(2, 0, 0, 0));
    CHECK(r.services.node(0)->heartbeat.present && r.services.node(0)->nmtTime.present);
    CHECK(r.services.node(0)->serviceQualification.present && r.services.node(0)->emcyCount == 1);
    const uint32_t generation = r.services.node(0)->route.generation;
    const auto sequence = r.sequence();
    const auto first = r.admit(r.request(sequence, 1, true));
    const auto next = r.admit(r.request(sequence, 2, true), true);
    r.turn(0, budget(0, 1, 0, 1));
    CHECK(r.fake.pendingCount() == 1);
    r.fake.retirement = true;
    CHECK(r.owner.reportController(C::ControllerState::UNKNOWN, 1));
    CHECK(r.services.controllerState() == C::ControllerState::UNKNOWN);
    const C::NodeSnapshot& node = *r.services.node(0);
    CHECK(node.route.generation == generation + 1);
    CHECK(!node.heartbeat.present && !node.nmtTime.present && !node.serviceQualification.present);
    CHECK(node.nmt == C::NmtState::OPERATIONAL && node.emcy.errorCode == 0x2310 &&
          node.emcyCount == 1);
    CHECK(r.services.unresolved(0) && r.services.unresolved(1));
    CHECK(r.done(first).service.effectUncertain && r.owner.result(next));
    CHECK(!r.owner.sequence(sequence) &&
          !r.owner.reportController(C::ControllerState::AVAILABLE, 1));
    CHECK(r.fake.pendingCount() == 0 && !r.fake.physicalDrain(0));
    const Saved<E::Completion> retained(r.done(first));
    r.turn(5000, budget());
    CHECK(r.fake.sentCount == 1 && r.fake.departedCount == 0);
    retained.unchanged(r.done(first));
}

void exhaustionClockBoundariesAndOverflow() {
    for (unsigned kind = 0; kind < 3; ++kind) {
        Rig r(kind == 0 ? UINT32_MAX : 0, kind == 1 ? UINT32_MAX : kind == 2 ? 0 : 1);
        E::RecoveryId output;
        output.generation = 123;
        const Saved<E::RecoveryId> unchanged(output);
        CHECK(!r.owner.recover(0, 1, output));
        unchanged.unchanged(output);
        CHECK(r.fake.begins == 0);
    }
    Rig last(UINT32_MAX - 1);
    E::RecoveryId final;
    CHECK(last.owner.recover(0, 1, final));
    CHECK(final.generation == UINT32_MAX);
    last.fake.progress = E::RecoveryProgress::RETIRED;
    last.turn(0, budget(0, 0, 0, 0, 1));
    CHECK(last.owner.releaseRecovery(final));
    CHECK(!last.owner.recover(0, 1, final));
    CHECK(last.fake.begins == 1);

    Rig epoch(0, UINT32_MAX - 1);
    E::RecoveryId finalEpoch;
    CHECK(epoch.owner.recover(0, 1, finalEpoch));
    CHECK(epoch.fake.recoveryEpoch == UINT32_MAX);
    epoch.fake.progress = E::RecoveryProgress::RETIRED;
    epoch.turn(0, budget(0, 0, 0, 0, 1));
    CHECK(epoch.owner.releaseRecovery(finalEpoch));
    const Saved<E::RecoveryId> unchangedEpoch(finalEpoch);
    CHECK(!epoch.owner.recover(0, 1, finalEpoch));
    unchangedEpoch.unchanged(finalEpoch);
    CHECK(epoch.fake.begins == 1);

    Rig requests(UINT32_MAX - 2);
    const auto lastSequence = requests.sequence();
    const auto lastRequest = requests.admit(requests.request(lastSequence));
    CHECK(lastRequest.generation == UINT32_MAX);
    E::RequestId exhausted;
    exhausted.generation = 543;
    const Saved<E::RequestId> unchangedRequest(exhausted);
    CHECK(requests.owner.admitUrgent(requests.request(lastSequence, 2), 0, exhausted) ==
          E::BusAdmission::IDS_EXHAUSTED);
    unchangedRequest.unchanged(exhausted);

    Rig clock;
    E::RecoveryId recovery;
    CHECK(!clock.owner.recover(0, 0, recovery));
    CHECK(clock.owner.recover(0, std::numeric_limits<uint64_t>::max(), recovery));
    clock.turn(1, E::ServiceBudget());
    CHECK(!clock.owner.service(0, budget(8, 2, 8, 2, 1)));
    CHECK(!clock.owner.receiveOverflow(0));
    CHECK(clock.fake.polls == 0);
    clock.turn(std::numeric_limits<uint64_t>::max(), E::ServiceBudget());
    CHECK(clock.owner.recoveryResult(recovery)->outcome == E::RecoveryOutcome::TIMEOUT);

    Rig overflow;
    const auto sequence = overflow.sequence();
    const auto first = overflow.admit(overflow.request(sequence, 1, true));
    const auto queued = overflow.admit(overflow.request(sequence, 2, true));
    overflow.turn(0, budget(0, 1, 0, 1));
    CHECK(overflow.owner.receiveOverflow(0));
    CHECK(overflow.services.controllerState() == C::ControllerState::RECOVERING);
    CHECK(overflow.done(first).service.effectUncertain && overflow.owner.result(queued));
    CHECK(!overflow.owner.sequence(sequence));
    CHECK(!overflow.owner.reportController(C::ControllerState::AVAILABLE, 0));
    overflow.turn(1000, budget());
    CHECK(overflow.fake.sentCount == 1 && overflow.services.unresolved(0));

    for (unsigned refused = 0; refused < 2; ++refused) {
        Rig failure;
        failure.fake.startRecovery = !refused;
        failure.fake.progress = E::RecoveryProgress::FAILED;
        E::RecoveryId id;
        CHECK(failure.owner.recover(0, 10, id));
        failure.turn(0, budget(0, 0, 0, 0, 1));
        CHECK(failure.owner.recoveryResult(id)->outcome == E::RecoveryOutcome::FAILED);
        CHECK(!failure.owner.reportController(C::ControllerState::AVAILABLE, 0));
        CHECK(failure.fake.polls == unsigned(!refused) && failure.fake.sentCount == 0);
    }
}
} // namespace
int main() {
    urgentCapacityAndPriority();
    urgentKeepsSequenceOrder();
    cancellationPhasesAndTerminalPrecedence();
    cleanupHandoffNeedsRetirement();
    captureTimeCannotRewriteAnExpiredTerminal();
    unprovedRetirementBlocksOnlyItsChannel();
    groupInvalidationDoesNotCancelOtherProducer();
    staleQueuedFramesAfterBoundaries();
    refusedEnqueueStillNeedsRetirement();
    exactRetirementControlsPhysicalDeparture();
    recoveryActuallyRetiresLocalQueues();
    repliesBeforeFinalTxReportDoNotLoseRetirement();
    recoveryHasSeparateCapacityAndFiniteBudget();
    missingRecoveryHooksRetainFailure();
    twoVendorBindingsShareRecoveryWithoutInventingEvidence();
    wrongEpochIngressAndReportsRemainDiagnostic();
    recoveryCannotRequalifyPdo();
    configurationWaitsForExternalRetirement();
    unknownControllerWithdrawsFreshness();
    exhaustionClockBoundariesAndOverflow();
    std::printf("can_bus_recovery_test: %u checks\n", checks);
    return 0;
}
