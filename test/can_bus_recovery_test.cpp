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
namespace O = MotorControlCANOwner;
namespace L = MotorControlCAN::LICHUAN_C;
static unsigned checks = 0;
#define CHECK(c)                                                                                   \
    do {                                                                                           \
        ++checks;                                                                                  \
        if (!(c)) {                                                                                \
            std::fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #c);                           \
            std::abort();                                                                          \
        }                                                                                          \
    } while (false)
#include "support/OwnerRig.h"
namespace {
using OwnerRig::budget;
using OwnerRig::route;
template <class T> struct Saved {
    unsigned char bytes[sizeof(T)];
    explicit Saved(const T& value) { std::memcpy(bytes, &value, sizeof value); }
    void unchanged(const T& value) const { CHECK(!std::memcmp(bytes, &value, sizeof value)); }
};
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
struct Fake {
    enum { CAPACITY = 64 };
    O::ReceivedFrame incoming[CAPACITY];
    O::TxReport reports[CAPACITY];
    M::Frame sent[CAPACITY];
    O::TransportTxId ids[CAPACITY];
    uint32_t epochs[CAPACITY] = {};
    bool pending[CAPACITY] = {};      // Controller queue ownership, separate from enqueue history.
    unsigned departed[CAPACITY] = {}; // Enqueue indices which actually left the fake controller.
    unsigned departedCount = 0;
    unsigned rxRead = 0, rxWrite = 0, reportRead = 0, reportWrite = 0, sentCount = 0;
    unsigned receives = 0, reportCalls = 0, observations = 0, retireCalls = 0;
    unsigned begins = 0, polls = 0;
    uint32_t recoveryEpoch = 0;
    bool accept = true, retirement = false, startRecovery = true;
    C::SdoClient* holder = nullptr; // Acts like an adapter holding a frame it has not submitted.
    O::RecoveryProgress progress = O::RecoveryProgress::PENDING;
    O::TransportTxId retired;
    uint32_t retiredEpoch = 0;
    static bool receive(void* p, O::ReceivedFrame& output) {
        Fake& f = *static_cast<Fake*>(p);
        ++f.receives;
        if (f.rxRead == f.rxWrite)
            return false;
        output = f.incoming[f.rxRead++];
        return true;
    }
    static bool transmit(void* p, const M::Frame& value, O::TransportTxId id, uint32_t epoch) {
        Fake& f = *static_cast<Fake*>(p);
        CHECK(f.sentCount < CAPACITY);
        f.sent[f.sentCount] = value;
        f.ids[f.sentCount] = id;
        // A false enqueue return deliberately retains possible queue acceptance.
        f.pending[f.sentCount] = true;
        f.epochs[f.sentCount++] = epoch;
        if (f.holder)
            CHECK(f.holder->holdHandoff(id.sdo));
        return f.accept;
    }
    static bool takeReport(void* p, O::TxReport& output) {
        Fake& f = *static_cast<Fake*>(p);
        ++f.reportCalls;
        if (f.reportRead == f.reportWrite)
            return false;
        output = f.reports[f.reportRead++];
        return true;
    }
    static void observe(void* p, const O::ReceivedFrame&, uint64_t) {
        ++static_cast<Fake*>(p)->observations;
    }
    static bool retire(void* p, O::TransportTxId id, uint32_t epoch) {
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
                const O::TxReport& report = f.reports[i];
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
    static O::RecoveryProgress poll(void* p, uint32_t epoch) {
        Fake& f = *static_cast<Fake*>(p);
        CHECK(epoch == f.recoveryEpoch);
        ++f.polls;
        if (f.progress == O::RecoveryProgress::RETIRED) {
            // This fake has no asynchronous callbacks. Empty every local queue
            // before claiming readiness for the new epoch; nothing is replayed.
            for (unsigned i = 0; i < f.sentCount; ++i)
                f.pending[i] = false;
            f.rxRead = f.rxWrite;
            f.reportRead = f.reportWrite;
        }
        return f.progress;
    }
    O::Transport transport(bool withRetire = true) {
        O::Transport out;
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
        O::ReceivedFrame& f = incoming[rxWrite++];
        f.frame = value;
        f.receivedTimeKnown = true;
        f.receivedUs = captured;
        f.controllerEpoch = epoch;
    }
    void queueReport(unsigned sentIndex, bool transmitted, uint32_t epoch) {
        CHECK(sentIndex < sentCount && reportWrite < CAPACITY);
        O::TxReport& r = reports[reportWrite++];
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
    O::CanBusOwner owner;
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
    void turn(uint64_t time, O::ServiceBudget b = budget()) {
        now = time;
        CHECK(owner.service(time, b));
    }
    O::SequenceId sequence(unsigned node = 1, uint8_t producer = 1, uint64_t deadline = 10000) {
        O::SequenceId out;
        CHECK(owner.beginSequence(route(node).binding, producer, now, out));
        CHECK(owner.beginOperation(out, 1, deadline, now));
        return out;
    }
    O::BusRequest request(O::SequenceId sequence, uint32_t step = 1, bool download = false,
                          uint16_t index = 0x2345) {
        const O::Sequence* s = owner.sequence(sequence);
        CHECK(s);
        O::BusRequest out;
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
    O::RequestId admit(const O::BusRequest& input, bool urgent = false) {
        O::RequestId out;
        CHECK((urgent ? owner.admitUrgent(input, now, out) : owner.admit(input, now, out)) ==
              O::BusAdmission::ACCEPTED);
        return out;
    }
    const O::Completion& done(O::RequestId id) {
        const O::Completion* out = owner.result(id);
        CHECK(out);
        return *out;
    }
    void complete(O::RequestId id, unsigned node = 1, bool download = false,
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
    O::RequestId pending[O::CanBusOwner::REQUESTS];
    for (unsigned i = 0; i < O::CanBusOwner::REQUESTS; ++i)
        pending[i] = r.admit(r.request(ordinary, i + 1));
    O::RequestId rejected;
    rejected.generation = 987;
    const Saved<O::RequestId> unchanged(rejected);
    CHECK(r.owner.admit(r.request(ordinary, 7), 0, rejected) == O::BusAdmission::RESULTS_FULL);
    unchanged.unchanged(rejected);
    const auto reserved = r.admit(r.request(urgent), true);
    CHECK(reserved.slot >= O::CanBusOwner::REQUESTS);
    CHECK(r.owner.admitUrgent(r.request(urgent, 2), 0, rejected) == O::BusAdmission::RESULTS_FULL);
    unchanged.unchanged(rejected);
    r.turn(0, budget(0, 1, 0, 1));
    CHECK(r.fake.sentCount == 1 && r.fake.sent[0].id == 0x602);
    r.complete(reserved, 2);
    CHECK(r.owner.beginOperation(urgent, 2, 10000, r.now));
    CHECK(r.owner.admitUrgent(r.request(urgent), r.now, rejected) == O::BusAdmission::RESULTS_FULL);
    CHECK(r.owner.release(reserved));
    const auto reused = r.admit(r.request(urgent), true);
    CHECK(reused.slot == reserved.slot && reused.generation != reserved.generation);
    CHECK(!r.owner.result(reserved));
    CHECK(r.owner.cancel(reused, r.now) == O::Cancel::CANCELLED);
    for (const auto id : pending)
        CHECK(r.owner.cancel(id, r.now) == O::Cancel::CANCELLED);
    CHECK(r.fake.sentCount == 1);

    // Six unread ordinary successes occupy all ordinary slots. New operation
    // admission still has a separate urgent terminal reservation.
    Rig full;
    const auto sequence = full.sequence();
    O::RequestId results[O::CanBusOwner::REQUESTS];
    for (unsigned i = 0; i < O::CanBusOwner::REQUESTS; ++i) {
        results[i] = full.admit(full.request(sequence, i + 1));
        full.turn(full.now, budget(0, 1, 0, 1));
        full.complete(results[i]);
    }
    const Saved<O::Completion> first(full.done(results[0]));
    CHECK(full.owner.beginOperation(sequence, 2, 10000, full.now));
    CHECK(full.owner.admit(full.request(sequence), full.now, rejected) ==
          O::BusAdmission::RESULTS_FULL);
    const auto extra = full.admit(full.request(sequence), true);
    full.turn(full.now, budget(0, 1, 0, 1));
    full.complete(extra);
    first.unchanged(full.done(results[0]));
    CHECK(full.fake.sentCount == O::CanBusOwner::TOTAL_REQUESTS);
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
            const Saved<O::Completion> prior(r.done(id));
            CHECK(r.owner.cancel(id, r.now) == O::Cancel::ALREADY_TERMINAL);
            prior.unchanged(r.done(id));
            CHECK(r.done(id).service.protocolAcknowledged);
        } else {
            CHECK(r.owner.cancel(id, r.now) == O::Cancel::CANCELLED);
            const auto& terminal = r.done(id);
            CHECK(terminal.reason == O::CompletionReason::CANCELLED);
            CHECK(terminal.service.outcome == C::SdoOutcome::CANCELLED);
            CHECK(terminal.service.transmissionPossible == (phase != 0));
            CHECK(terminal.service.transmissionConfirmed == (phase == 2));
            CHECK(terminal.service.effectUncertain == (phase != 0));
            CHECK(!terminal.service.protocolAcknowledged);
            const Saved<O::Completion> saved(terminal);
            r.fake.push(reply(1, true), r.now + 1);
            r.turn(r.now + 1, budget(1, 0, 8, 0));
            saved.unchanged(r.done(id));
            CHECK(r.owner.cancel(id, r.now) == O::Cancel::ALREADY_TERMINAL);
        }
        CHECK(r.fake.sentCount == unsigned(phase != 0));
        CHECK(r.fake.retireCalls == unsigned(phase == 1));
    }
    Rig deadline;
    const auto sequence = deadline.sequence(1, 1, 10);
    const auto id = deadline.admit(deadline.request(sequence));
    CHECK(deadline.owner.cancel(id, 10) == O::Cancel::ALREADY_TERMINAL);
    CHECK(deadline.done(id).reason == O::CompletionReason::DEADLINE);
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
    CHECK(staged.owner.cancel(segment, 1) == O::Cancel::CANCELLED);
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
    const Saved<O::Completion> saved(r.done(id));
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
        const Saved<O::Completion> saved(r.done(id));
        CHECK(r.owner.cancel(id, 10) == O::Cancel::ALREADY_TERMINAL);
        CHECK(r.owner.cancel(id, 9) == O::Cancel::INVALID);
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
        CHECK(r.owner.cancel(first, 1) == O::Cancel::CANCELLED);
        CHECK(r.done(first).service.effectUncertain);
        M::Frame heartbeat = frame(0x702, 1);
        heartbeat.data[0] = 5;
        r.fake.push(heartbeat, 2);
        r.turn(2, budget(1, 1, 8, 1));
        CHECK(r.done(urgent).reason == O::CompletionReason::INVALIDATED);
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
        CHECK(r.done(first).reason == O::CompletionReason::INVALIDATED);
        CHECK(r.done(next).reason == O::CompletionReason::INVALIDATED);
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
        const Saved<O::Completion> interrupted(r.done(first));
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
    const Saved<O::Completion> retained(r.done(id));
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
        CHECK(r.owner.cancel(original, 1) == O::Cancel::CANCELLED);
        CHECK(r.fake.retireCalls == 1 && r.fake.pendingCount() == unsigned(!positive));
        r.turn(1, budget());
        CHECK(r.done(later).reason == O::CompletionReason::INVALIDATED);
        CHECK(r.fake.sentCount == 1);
        const Saved<O::Completion> saved(r.done(original));
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
    CHECK(parallel.owner.cancel(cancelled, 1) == O::Cancel::CANCELLED);
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
        const O::BusRequest original = r.request(b, 1, true, 0x2346);
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
        r.fake.progress = outcome == 0   ? O::RecoveryProgress::RETIRED
                          : outcome == 2 ? O::RecoveryProgress::FAILED
                                         : O::RecoveryProgress::PENDING;
        O::RecoveryId recovery;
        CHECK(r.owner.recover(1, 3, recovery));
        const Saved<O::Completion> firstResult(r.done(first));
        const Saved<O::Completion> secondResult(r.done(second));
        r.turn(outcome == 3 ? 3 : 2, budget(0, 0, 0, 0, 1));
        CHECK(r.owner.recoveryResult(recovery));
        if (outcome == 0) {
            CHECK(r.owner.recoveryResult(recovery)->outcome == O::RecoveryOutcome::RETIRED);
            CHECK(r.fake.pendingCount() == 0 && r.fake.rxRead == r.fake.rxWrite);
            CHECK(r.fake.reportRead == r.fake.reportWrite);
            CHECK(!r.fake.physicalDrain(1) && r.fake.departedCount == 1);
            CHECK(r.services.controllerState() == C::ControllerState::AVAILABLE);
        } else {
            CHECK(r.owner.recoveryResult(recovery)->outcome ==
                  (outcome == 3 ? O::RecoveryOutcome::TIMEOUT : O::RecoveryOutcome::FAILED));
            CHECK(r.fake.pendingCount() == 1 && r.fake.rxRead == 0 && r.fake.reportRead == 0);
            CHECK(!r.owner.reportController(C::ControllerState::AVAILABLE, r.now));
            O::RequestId rejected;
            CHECK(r.owner.admit(original, r.now, rejected) == O::BusAdmission::INHIBITED);
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
    const Saved<O::Completion> saved(r.done(segmented));
    O::RequestId rejected;
    CHECK(r.owner.admit(r.request(sequence, 2), 6, rejected) == O::BusAdmission::INHIBITED);
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
    O::RequestId pending[O::CanBusOwner::TOTAL_REQUESTS];
    for (unsigned i = 0; i < O::CanBusOwner::TOTAL_REQUESTS; ++i)
        pending[i] = r.admit(r.request(sequence, i + 1, true), i == O::CanBusOwner::REQUESTS);
    r.turn(0, budget(0, 1, 0, 1));
    O::RecoveryId recovery;
    CHECK(r.owner.recover(0, 10, recovery));
    CHECK(r.fake.begins == 1 && r.fake.recoveryEpoch == 2 && !r.owner.recoveryResult(recovery));
    CHECK(!r.owner.releaseRecovery(recovery));
    for (const auto id : pending)
        CHECK(r.done(id).reason == O::CompletionReason::RECOVERY);
    const Saved<O::Completion> interrupted(r.done(pending[0]));
    O::RecoveryId unchanged;
    unchanged.generation = 765;
    const Saved<O::RecoveryId> rejected(unchanged);
    CHECK(!r.owner.recover(0, 20, unchanged));
    rejected.unchanged(unchanged);
    r.turn(0, budget(0, 0, 0, 0, 0));
    CHECK(r.fake.polls == 0);
    r.turn(1, budget(0, 0, 0, 0, 255));
    CHECK(r.fake.polls == 1 && !r.owner.recoveryResult(recovery));
    r.turn(10, budget(0, 0, 0, 0, 255));
    CHECK(r.fake.polls == 1);
    const O::RecoveryCompletion* terminal = r.owner.recoveryResult(recovery);
    CHECK(terminal && terminal->outcome == O::RecoveryOutcome::TIMEOUT);
    CHECK(terminal->startedUs == 0 && terminal->deadlineUs == 10 && terminal->completedUs == 10);
    const Saved<O::RecoveryCompletion> saved(*terminal);
    r.fake.progress = O::RecoveryProgress::RETIRED;
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
    CHECK(r.owner.recoveryResult(unchanged)->outcome == O::RecoveryOutcome::RETIRED);
    CHECK(r.services.controllerState() == C::ControllerState::AVAILABLE);
    // Contract change (W5): recovery quarantines the route with a frame out, not the idle one.
    CHECK(r.services.unresolved(0) && !r.services.unresolved(1));
    CHECK(r.fake.sentCount == 1 && !r.owner.sequence(sequence));
}

void missingRecoveryHooksRetainFailure() {
    for (unsigned missing = 1; missing <= 3; ++missing) {
        C::SdoClient services;
        Fake fake;
        O::Transport transport = fake.transport();
        if (missing & 1)
            transport.beginRecovery = nullptr;
        if (missing & 2)
            transport.pollRecovery = nullptr;
        O::CanBusOwner owner(services, transport, 73);
        O::RecoveryId id;
        CHECK(owner.recover(0, 10, id));
        const O::RecoveryCompletion* terminal = owner.recoveryResult(id);
        CHECK(terminal && terminal->outcome == O::RecoveryOutcome::FAILED);
        CHECK(terminal->completedUs == 0 && terminal->controllerEpoch == 2);
        const Saved<O::RecoveryCompletion> saved(*terminal);
        CHECK(owner.service(1, budget(8, 2, 8, 2, 1)));
        saved.unchanged(*owner.recoveryResult(id));
        CHECK(fake.begins == 0 && fake.polls == 0 && fake.sentCount == 0);
        O::RecoveryId rejected;
        CHECK(!owner.recover(1, 20, rejected));
        CHECK(owner.releaseRecovery(id) && !owner.recoveryResult(id));
        CHECK(!owner.reportController(C::ControllerState::AVAILABLE, 1));
        O::RequestId work;
        CHECK(owner.admit(O::BusRequest(), 1, work) == O::BusAdmission::INHIBITED);
        CHECK(owner.recover(1, 20, rejected));
        CHECK(rejected.generation != id.generation);
        CHECK(owner.recoveryResult(rejected)->outcome == O::RecoveryOutcome::FAILED);
        CHECK(!owner.recoveryResult(id));
    }
}

void twoVendorBindingsShareRecoveryWithoutInventingEvidence() {
    // Abstract Lichuan C / Nanotec N5 CANopen bindings from Prompt 4's contrast.
    // Profile tags and object bytes are local fixtures, not installed vendor operations.
    C::SdoClient services;
    Fake fake;
    O::CanBusOwner owner(services, fake.transport(), 73);
    O::RequestId ids[2];
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
        O::SequenceId sequence;
        CHECK(owner.beginSequence(binding.binding, channel + 1, 0, sequence));
        CHECK(owner.beginOperation(sequence, 1, 100, 0));
        O::BusRequest work;
        work.sequence = sequence;
        work.transfer.channel = binding;
        work.transfer.request = work.transfer.operation = work.transfer.step = 1;
        work.transfer.direction = C::SdoDirection::DOWNLOAD;
        work.transfer.index = 0x2345;
        work.transfer.size = 2;
        work.transfer.data[0] = uint8_t(10 + channel);
        work.transfer.queueTimeoutUs = work.transfer.responseTimeoutUs = 50;
        work.transfer.deadlineUs = 100;
        CHECK(owner.admit(work, 0, ids[channel]) == O::BusAdmission::ACCEPTED);
    }
    CHECK(owner.service(0, budget(0, 2, 0, 2)));
    CHECK(fake.sentCount == 2 && fake.pendingCount() == 2);
    CHECK(fake.ids[0].sdo.request.owner == &services && fake.ids[1].sdo.request.owner == &services);
    CHECK(fake.physicalDrain(0));
    fake.push(reply(1, true), 1);
    CHECK(owner.service(1, budget(1, 0, 8, 0)));
    CHECK(owner.result(ids[0]) && owner.result(ids[0])->service.protocolAcknowledged);
    CHECK(!owner.result(ids[1]));
    const Saved<O::Completion> confirmed(*owner.result(ids[0]));
    O::RecoveryId recovery;
    CHECK(owner.recover(2, 10, recovery));
    const O::Completion* uncertain = owner.result(ids[1]);
    CHECK(uncertain && uncertain->service.effectUncertain);
    CHECK(!uncertain->service.protocolAcknowledged && !uncertain->service.transmissionConfirmed);
    const Saved<O::Completion> interrupted(*uncertain);
    fake.progress = O::RecoveryProgress::RETIRED;
    CHECK(owner.service(3, budget(0, 0, 0, 0, 1)));
    CHECK(owner.recoveryResult(recovery)->outcome == O::RecoveryOutcome::RETIRED);
    CHECK(fake.pendingCount() == 0 && !fake.physicalDrain(1));
    for (uint8_t channel = 0; channel < 2; ++channel) {
        CHECK(owner.result(ids[channel])->service.transfer.channel.binding.endpoint.profile ==
              unsigned(41 + channel));
    }
    // Contract change (W5): the settled write on the first binding leaves nothing that
    // could still answer; the second write is uncertain and only its route is quarantined.
    CHECK(!services.unresolved(0) && services.unresolved(1));
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
    CHECK(unconfirmed.owner.cancel(unconfirmedId, 1) == O::Cancel::CANCELLED);
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
    const Saved<O::Completion> saved(r.done(id));
    O::RecoveryId recovery;
    CHECK(r.owner.recover(3, 10, recovery));
    r.fake.progress = O::RecoveryProgress::RETIRED;
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
    O::RecoveryId recovery;
    CHECK(r.owner.recover(2, 10, recovery));
    CHECK(!r.services.pdo(0)->qualified && !r.services.pdo(0)->confirmed);
    r.fake.progress = O::RecoveryProgress::RETIRED;
    r.turn(3, budget(0, 0, 0, 0, 1));
    r.fake.push(frame(0x281), 4, 2);
    r.turn(4, budget(1, 2, 8, 2));
    r.turn(5000, budget());
    CHECK(!r.services.pdo(0)->qualified && !r.services.pdo(0)->confirmed);
    CHECK(!r.services.qualifyPdo(0, proof, 5000));
    // Contract change (W5): no SDO left this route, so it stays open. The PDO loss above stands.
    CHECK(!r.services.unresolved(0) && r.fake.sentCount == 0);
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
    // Contract change (W5): UNKNOWN acts through RECOVERING, so only the route with work is held.
    CHECK(r.services.unresolved(0) && !r.services.unresolved(1));
    CHECK(r.done(first).service.effectUncertain && r.owner.result(next));
    CHECK(!r.owner.sequence(sequence) &&
          !r.owner.reportController(C::ControllerState::AVAILABLE, 1));
    CHECK(r.fake.pendingCount() == 0 && !r.fake.physicalDrain(0));
    const Saved<O::Completion> retained(r.done(first));
    r.turn(5000, budget());
    CHECK(r.fake.sentCount == 1 && r.fake.departedCount == 0);
    retained.unchanged(r.done(first));
}

void exhaustionClockBoundariesAndOverflow() {
    for (unsigned kind = 0; kind < 3; ++kind) {
        Rig r(kind == 0 ? UINT32_MAX : 0, kind == 1 ? UINT32_MAX : kind == 2 ? 0 : 1);
        O::RecoveryId output;
        output.generation = 123;
        const Saved<O::RecoveryId> unchanged(output);
        CHECK(!r.owner.recover(0, 1, output));
        unchanged.unchanged(output);
        CHECK(r.fake.begins == 0);
    }
    Rig last(UINT32_MAX - 1);
    O::RecoveryId final;
    CHECK(last.owner.recover(0, 1, final));
    CHECK(final.generation == UINT32_MAX);
    last.fake.progress = O::RecoveryProgress::RETIRED;
    last.turn(0, budget(0, 0, 0, 0, 1));
    CHECK(last.owner.releaseRecovery(final));
    CHECK(!last.owner.recover(0, 1, final));
    CHECK(last.fake.begins == 1);

    Rig epoch(0, UINT32_MAX - 1);
    O::RecoveryId finalEpoch;
    CHECK(epoch.owner.recover(0, 1, finalEpoch));
    CHECK(epoch.fake.recoveryEpoch == UINT32_MAX);
    epoch.fake.progress = O::RecoveryProgress::RETIRED;
    epoch.turn(0, budget(0, 0, 0, 0, 1));
    CHECK(epoch.owner.releaseRecovery(finalEpoch));
    const Saved<O::RecoveryId> unchangedEpoch(finalEpoch);
    CHECK(!epoch.owner.recover(0, 1, finalEpoch));
    unchangedEpoch.unchanged(finalEpoch);
    CHECK(epoch.fake.begins == 1);

    Rig requests(UINT32_MAX - 2);
    const auto lastSequence = requests.sequence();
    const auto lastRequest = requests.admit(requests.request(lastSequence));
    CHECK(lastRequest.generation == UINT32_MAX);
    O::RequestId exhausted;
    exhausted.generation = 543;
    const Saved<O::RequestId> unchangedRequest(exhausted);
    CHECK(requests.owner.admitUrgent(requests.request(lastSequence, 2), 0, exhausted) ==
          O::BusAdmission::IDS_EXHAUSTED);
    unchangedRequest.unchanged(exhausted);

    Rig clock;
    O::RecoveryId recovery;
    CHECK(!clock.owner.recover(0, 0, recovery));
    CHECK(clock.owner.recover(0, std::numeric_limits<uint64_t>::max(), recovery));
    clock.turn(1, O::ServiceBudget());
    CHECK(!clock.owner.service(0, budget(8, 2, 8, 2, 1)));
    CHECK(!clock.owner.receiveOverflow(0));
    CHECK(clock.fake.polls == 0);
    clock.turn(std::numeric_limits<uint64_t>::max(), O::ServiceBudget());
    CHECK(clock.owner.recoveryResult(recovery)->outcome == O::RecoveryOutcome::TIMEOUT);

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
        failure.fake.progress = O::RecoveryProgress::FAILED;
        O::RecoveryId id;
        CHECK(failure.owner.recover(0, 10, id));
        failure.turn(0, budget(0, 0, 0, 0, 1));
        CHECK(failure.owner.recoveryResult(id)->outcome == O::RecoveryOutcome::FAILED);
        CHECK(!failure.owner.reportController(C::ControllerState::AVAILABLE, 0));
        CHECK(failure.fake.polls == unsigned(!refused) && failure.fake.sentCount == 0);
    }
}
// Explicit SDO settlement (DEC-3, W5). Every marker positive below states the sole-client and
// ordered-server premises through this fake. It proves this host's logic, never that a real
// server answers in order: Q18 stays open until packet 11 measures it.
C::SdoDrainPremises fakePremises(bool soleClient = true, bool orderedServer = true) {
    C::SdoDrainPremises out;
    out.soleClient = soleClient;
    out.orderedServer = orderedServer;
    return out;
}
const uint16_t LOST_INDEX = 0x2345; // Rig::request's default object, retained by lose().
const uint16_t MARKER_INDEX = 0x2000;
C::SdoTransfer markerWork(uint64_t now, unsigned node = 1, uint16_t index = MARKER_INDEX,
                          uint8_t subindex = 0) {
    C::SdoTransfer out;
    out.channel = route(node);
    out.operation = out.request = out.step = 900;
    out.index = index;
    out.subindex = subindex;
    out.size = 2;
    out.direction = C::SdoDirection::UPLOAD;
    out.readEffect = C::SdoReadEffect::ACCEPTED_UNQUALIFIED;
    out.allowSegmentedUpload = false; // A marker is one expedited exchange.
    out.queueTimeoutUs = out.responseTimeoutUs = 1000;
    out.deadlineUs = now + 5000;
    return out;
}
M::Frame serverFrame(unsigned node, uint8_t command, uint16_t index, uint8_t subindex = 0) {
    M::Frame out = frame(0x580 + node);
    out.data[0] = command;
    out.data[1] = uint8_t(index);
    out.data[2] = uint8_t(index >> 8);
    out.data[3] = subindex;
    return out;
}
M::Frame uploaded(unsigned node = 1, uint16_t index = MARKER_INDEX, uint8_t subindex = 0) {
    M::Frame out = serverFrame(node, 0x4b, index, subindex);
    out.data[4] = 0x34;
    out.data[5] = 0x12;
    return out;
}
M::Frame serverAbort(unsigned node, uint16_t index, uint32_t code, uint8_t subindex = 0) {
    M::Frame out = serverFrame(node, 0x80, index, subindex);
    for (unsigned i = 0; i < 4; ++i)
        out.data[4 + i] = uint8_t(code >> (8 * i));
    return out;
}
M::Frame networkState(unsigned node, uint8_t state) {
    M::Frame out = frame(0x700 + node, 1);
    out.data[0] = state;
    return out;
}
void expectUpload(const M::Frame& sent, unsigned node, uint16_t index, uint8_t subindex = 0) {
    CHECK(sent.id == 0x600 + node && sent.dlc == 8 && sent.payloadLength == 8);
    CHECK(!sent.extended && !sent.rtr && !sent.fd);
    const uint8_t literal[8] = {0x40, uint8_t(index), uint8_t(index >> 8), subindex, 0, 0, 0, 0};
    CHECK(!std::memcmp(sent.data, literal, sizeof literal));
}
void expectWrite(const M::Frame& sent, unsigned node, uint16_t index) {
    CHECK(sent.id == 0x600 + node && sent.dlc == 8 && sent.payloadLength == 8);
    const uint8_t literal[8] = {0x2b, uint8_t(index), uint8_t(index >> 8), 0, 0xa5, 0x5a, 0, 0};
    CHECK(!std::memcmp(sent.data, literal, sizeof literal));
}
struct Lost {
    O::SequenceId sequence;
    O::RequestId request;
};
// One sent write that never gets a reply: a possible effect, and LOST_INDEX:00 retained.
Lost lose(Rig& r, unsigned node = 1) {
    Lost out;
    out.sequence = r.sequence(node, uint8_t(node));
    out.request = r.admit(r.request(out.sequence, 1, true));
    r.turn(r.now, budget(0, 1, 0, 1));
    expectWrite(r.fake.sent[r.fake.sentCount - 1], node, LOST_INDEX);
    r.fake.report(r.fake.sentCount - 1);
    r.turn(r.now + 1000, budget(0, 0, 8, 0));
    const C::SdoCompletion& result = r.done(out.request).service;
    CHECK(result.outcome == C::SdoOutcome::TIMEOUT && result.effectPossible &&
          result.effectUncertain && r.services.unresolved(uint8_t(node - 1)));
    return out;
}
O::RequestId admitMarker(Rig& r, const C::SdoTransfer& work) {
    O::RequestId out;
    CHECK(r.owner.admitDrainMarker(work, fakePremises(), r.now, out) == O::BusAdmission::ACCEPTED);
    CHECK(out.slot >= O::CanBusOwner::REQUESTS); // The urgent slot, outside ordinary pressure.
    return out;
}
// Sends the marker and proves its exact frame. Reports it as left, so it is awaiting a reply.
O::RequestId sendMarker(Rig& r, const C::SdoTransfer& work, unsigned node = 1) {
    const O::RequestId out = admitMarker(r, work);
    const unsigned sent = r.fake.sentCount;
    r.turn(r.now, budget(0, 1, 0, 1));
    CHECK(r.fake.sentCount == sent + 1 && !r.owner.result(out));
    expectUpload(r.fake.sent[sent], node, work.index, work.subindex);
    r.fake.report(sent);
    r.turn(r.now, budget(0, 0, 8, 0));
    CHECK(!r.owner.result(out));
    return out;
}
void markerRefused(Rig& r, const C::SdoTransfer& work, O::BusAdmission expected,
                   const C::SdoDrainPremises& premises = fakePremises()) {
    O::RequestId output;
    output.generation = 987;
    const Saved<O::RequestId> unchanged(output);
    const unsigned sent = r.fake.sentCount;
    const uint32_t requalified = r.services.requalifications(uint8_t(work.channel.binding.endpoint.address - 1));
    CHECK(r.owner.admitDrainMarker(work, premises, r.now, output) == expected);
    unchanged.unchanged(output);
    CHECK(r.fake.sentCount == sent);
    CHECK(r.services.requalifications(uint8_t(work.channel.binding.endpoint.address - 1)) == requalified);
}
void stray(Rig& r, const M::Frame& value) {
    r.fake.push(value, r.now + 1);
    r.turn(r.now + 1, budget(8, 0, 8, 0));
}

void absentObjectAbortLeavesRouteOpen() {
    Rig r;
    const auto sequence = r.sequence();
    const auto first = r.admit(r.request(sequence, 1, true));
    r.turn(0, budget(0, 1, 0, 1));
    r.complete(first, 1, true);
    const Saved<O::Completion> firstResult(r.done(first));
    const auto second = r.admit(r.request(sequence, 2, true, 0x2346));
    r.turn(r.now, budget(0, 1, 0, 1));
    expectWrite(r.fake.sent[1], 1, 0x2346);
    r.fake.report(1);
    const M::Frame absent = serverAbort(1, 0x2346, 0x06020000u);
    stray(r, absent);
    const C::SdoCompletion& rejected = r.done(second).service;
    CHECK(rejected.outcome == C::SdoOutcome::SERVER_ABORT && rejected.abortValid &&
          rejected.abortCode == 0x06020000u && rejected.replyValid);
    CHECK(rejected.reply.id == 0x581 && !std::memcmp(rejected.reply.data, absent.data, 8));
    CHECK(rejected.transmissionPossible && rejected.effectPossible && rejected.effectUncertain);
    CHECK(!rejected.protocolAcknowledged && !rejected.clientAbortQueued);
    // The rejection settles this transaction only. Nothing rewrites either recorded write.
    firstResult.unchanged(r.done(first));
    CHECK(r.done(first).service.protocolAcknowledged && r.done(first).service.effectPossible);
    CHECK(!r.services.unresolved(0) && !r.services.drainSettled(0));
    CHECK(r.services.requalifications(0) == 0);
    // No marker applies to an open route, and the same reservation carries the next step.
    markerRefused(r, markerWork(r.now), O::BusAdmission::INVALID);
    CHECK(r.owner.sequence(sequence));
    const auto next = r.admit(r.request(sequence, 3, false, 0x2347));
    r.turn(r.now, budget(0, 1, 0, 1));
    CHECK(r.fake.sentCount == 3); // No replay of the aborted write.
    expectUpload(r.fake.sent[2], 1, 0x2347);
    r.fake.report(2);
    stray(r, uploaded(1, 0x2347));
    CHECK(r.done(next).service.outcome == C::SdoOutcome::SUCCESS);
    // Before an initial handoff, the same abort frame is not a matched abort.
    Rig early;
    const auto queued = early.sequence();
    const auto unsent = early.admit(early.request(queued, 1, true));
    early.fake.push(absent, 0);
    early.turn(0, budget(8, 0, 0, 0));
    CHECK(early.services.unresolved(0) && early.fake.sentCount == 0);
    CHECK(early.done(unsent).reason == O::CompletionReason::INVALIDATED);
}

enum class ControllerEvent { RECOVERY, BUS_OFF };
void raise(Rig& r, ControllerEvent event, O::RecoveryId& recovery) {
    if (event == ControllerEvent::BUS_OFF)
        CHECK(r.owner.reportController(C::ControllerState::BUS_OFF, r.now));
    CHECK(r.owner.recover(r.now, r.now + 100, recovery));
}
void retireRecovery(Rig& r, O::RecoveryId recovery) {
    r.fake.progress = O::RecoveryProgress::RETIRED;
    r.turn(r.now + 1, budget(0, 0, 0, 0, 1));
    CHECK(r.owner.recoveryResult(recovery)->outcome == O::RecoveryOutcome::RETIRED);
    CHECK(r.owner.releaseRecovery(recovery));
}
void controllerEventsQuarantineOnlyPossibleWork() {
    const ControllerEvent events[] = {ControllerEvent::RECOVERY, ControllerEvent::BUS_OFF};
    for (ControllerEvent event : events) {
        // Sent on node 1, untouched node 2.
        {
            Rig r;
            const auto sequence = r.sequence();
            const auto sent = r.admit(r.request(sequence, 1, true));
            r.turn(0, budget(0, 1, 0, 1));
            const uint32_t generation = r.services.node(1)->route.generation;
            O::RecoveryId recovery;
            raise(r, event, recovery);
            CHECK(r.services.unresolved(0) && !r.services.unresolved(1));
            const C::SdoCompletion& result = r.done(sent).service;
            CHECK(result.transmissionPossible && result.effectUncertain && !result.protocolAcknowledged);
            CHECK(result.outcome == (event == ControllerEvent::BUS_OFF ? C::SdoOutcome::BUS_OFF
                                                                       : C::SdoOutcome::CANCELLED));
            retireRecovery(r, recovery);
            CHECK(r.services.unresolved(0) && !r.services.unresolved(1));
            CHECK(r.fake.sentCount == 1); // Recovery sent nothing and replayed nothing.
            // Clean is not current: the untouched axis lost its evidence with its generation.
            const C::NodeSnapshot& node = *r.services.node(1);
            CHECK(node.route.generation > generation && !node.serviceQualification.present &&
                  !node.heartbeat.present && !node.nmtTime.present);
            O::SequenceId refused;
            CHECK(!r.owner.beginSequence(route(1).binding, 1, r.now, refused));
            const auto other = r.sequence(2, 2);
            O::BusRequest write = r.request(other, 1, true);
            O::RequestId output;
            CHECK(r.owner.admit(write, r.now, output) == O::BusAdmission::INVALID);
            CHECK(!r.services.checkAdmission(1, write.transfer, r.now));
            r.fake.push(networkState(2, 5), r.now + 1, r.owner.controllerEpoch());
            r.turn(r.now + 1, budget(1, 0, 0, 0));
            CHECK(r.services.node(1)->nmtTime.present && r.services.checkAdmission(1, write.transfer, r.now));
            write = r.request(other, 2, true);
            CHECK(r.owner.admit(write, r.now, output) == O::BusAdmission::ACCEPTED);
            r.turn(r.now, budget(0, 1, 0, 1));
            CHECK(r.fake.sentCount == 2);
            expectWrite(r.fake.sent[1], 2, LOST_INDEX);
        }
        // Service evidence qualifies the untouched axis as well as NMT does.
        {
            Rig r;
            O::RecoveryId recovery;
            raise(r, event, recovery);
            CHECK(!r.services.unresolved(0) && !r.services.unresolved(1));
            retireRecovery(r, recovery);
            const auto sequence = r.sequence();
            O::RequestId output;
            CHECK(r.owner.admit(r.request(sequence, 1, true), r.now, output) == O::BusAdmission::INVALID);
            const auto read = r.admit(r.request(sequence, 2));
            r.turn(r.now, budget(0, 1, 0, 1));
            expectUpload(r.fake.sent[0], 1, LOST_INDEX);
            r.fake.report(0, true, r.owner.controllerEpoch());
            r.fake.push(reply(), r.now + 1, r.owner.controllerEpoch());
            r.turn(r.now + 1, budget(8, 0, 8, 0));
            CHECK(r.done(read).service.outcome == C::SdoOutcome::SUCCESS);
            CHECK(r.services.node(0)->serviceQualification.present);
            CHECK(r.owner.admit(r.request(sequence, 3, true), r.now, output) == O::BusAdmission::ACCEPTED);
        }
        // Admitted, staged and held work had no frame on the bus.
        for (unsigned phase = 0; phase < 3; ++phase) {
            Rig r;
            const auto sequence = r.sequence();
            const auto work = r.admit(r.request(sequence, 1, true));
            if (phase == 1) {
                // Stage the frame in the client, with no transmit allowed in this owner turn.
                r.turn(0, budget(0, 0, 0, 1));
            } else if (phase == 2) {
                r.fake.holder = &r.services;
                r.turn(0, budget(0, 1, 0, 1));
                CHECK(r.fake.sentCount == 1); // Reached the transport, which held it unsubmitted.
            }
            CHECK(!r.owner.result(work));
            O::RecoveryId recovery;
            raise(r, event, recovery);
            CHECK(!r.services.unresolved(0) && !r.services.unresolved(1));
            const C::SdoCompletion& result = r.done(work).service;
            CHECK(!result.transmissionPossible && !result.effectPossible && !result.effectUncertain);
        }
    }
    // The node-scoped cause behaves the same as the interface event.
    for (unsigned cause = 0; cause < 2; ++cause) {
        const C::NetworkInvalidation value =
            cause ? C::NetworkInvalidation::BUS_OFF : C::NetworkInvalidation::RECOVERY;
        Rig idle;
        CHECK(idle.services.invalidateNetwork(0, value, 0));
        CHECK(!idle.services.unresolved(0) && !idle.services.node(0)->serviceQualification.present);
        Rig busy;
        const auto sequence = busy.sequence();
        const auto id = busy.admit(busy.request(sequence, 1, true));
        busy.turn(0, budget(0, 1, 0, 1));
        CHECK(busy.services.invalidateNetwork(0, value, 1));
        CHECK(busy.services.unresolved(0) && !busy.services.unresolved(1));
        busy.turn(1, budget(0, 0, 8, 0));
        CHECK(busy.done(id).service.effectUncertain);
    }
}

void bootAndBindingStillQuarantineIdleRoutes() {
    Rig boot;
    boot.fake.push(networkState(1, 0), 0);
    boot.turn(0, budget(1, 0, 0, 0));
    CHECK(boot.services.unresolved(0) && !boot.services.unresolved(1));
    // Nothing was sent, so the boot retained no pair; any distinct object can drain it.
    boot.fake.push(networkState(1, 0x7f), 1);
    boot.turn(1, budget(1, 0, 0, 0));
    CHECK(boot.services.checkDrain(0, markerWork(1, 1, LOST_INDEX), fakePremises(), 1));
    Rig binding;
    CHECK(binding.services.invalidateNetwork(0, C::NetworkInvalidation::BINDING, 0));
    CHECK(binding.services.unresolved(0) && !binding.services.unresolved(1));
    O::SequenceId refused;
    CHECK(!binding.owner.beginSequence(route(1).binding, 1, 0, refused));
    const auto other = binding.sequence(2, 2);
    CHECK(binding.owner.sequence(other));
}

void markerRefusals() {
    {   // Only an unresolved route takes a marker.
        Rig open;
        markerRefused(open, markerWork(0), O::BusAdmission::INVALID);
        CHECK(!open.services.checkDrain(0, markerWork(0), fakePremises(), 0));
    }
    {   // Each premise is the application's, and neither defaults to true.
        Rig r;
        lose(r);
        const C::SdoTransfer work = markerWork(r.now);
        markerRefused(r, work, O::BusAdmission::INVALID, C::SdoDrainPremises());
        markerRefused(r, work, O::BusAdmission::INVALID, fakePremises(false, true));
        markerRefused(r, work, O::BusAdmission::INVALID, fakePremises(true, false));
        markerRefused(r, work, O::BusAdmission::INVALID, fakePremises(false, false));
        CHECK(r.services.checkDrain(0, work, fakePremises(), r.now));
    }
    {   // The same pair as a retained request; the same index with another subindex differs.
        Rig r;
        lose(r);
        markerRefused(r, markerWork(r.now, 1, LOST_INDEX), O::BusAdmission::INVALID);
        CHECK(r.services.checkDrain(0, markerWork(r.now, 1, LOST_INDEX, 1), fakePremises(), r.now));
    }
    {   // Only an exact accepted upload qualifies.
        Rig r;
        lose(r);
        C::SdoTransfer work = markerWork(r.now);
        work.direction = C::SdoDirection::DOWNLOAD;
        markerRefused(r, work, O::BusAdmission::INVALID);
        work = markerWork(r.now);
        work.readEffect = C::SdoReadEffect::UNQUALIFIED;
        markerRefused(r, work, O::BusAdmission::INVALID);
        work = markerWork(r.now);
        work.readEffect = C::SdoReadEffect::REVIEWED_NON_CHANGING;
        markerRefused(r, work, O::BusAdmission::INVALID);
        work = markerWork(r.now);
        work.uploadSize = C::SdoUploadSize::SERVER_INDICATED;
        markerRefused(r, work, O::BusAdmission::INVALID);
        work = markerWork(r.now);
        work.deadlineUs = r.now;
        markerRefused(r, work, O::BusAdmission::INVALID);
        // A marker is one expedited exchange: no segmentation, and nothing that requires it.
        work = markerWork(r.now);
        work.allowSegmentedUpload = true;
        markerRefused(r, work, O::BusAdmission::INVALID);
        work = markerWork(r.now);
        work.size = 5;
        markerRefused(r, work, O::BusAdmission::INVALID);
        work = markerWork(r.now);
        work.size = 4;
        CHECK(r.services.checkDrain(0, work, fakePremises(), r.now));
    }
    // The last completed pair could still answer twice, whether it succeeded or cleanly aborted.
    for (unsigned rejected = 0; rejected < 2; ++rejected) {
        Rig r;
        const auto sequence = r.sequence();
        const auto done = r.admit(r.request(sequence, 1, false, 0x2346));
        r.turn(0, budget(0, 1, 0, 1));
        r.fake.report(0);
        stray(r, rejected ? serverAbort(1, 0x2346, 0x06020000u) : uploaded(1, 0x2346));
        CHECK(r.done(done).service.outcome ==
              (rejected ? C::SdoOutcome::SERVER_ABORT : C::SdoOutcome::SUCCESS));
        CHECK(!r.services.unresolved(0));
        const auto lost = r.admit(r.request(sequence, 2, true));
        r.turn(r.now, budget(0, 1, 0, 1));
        r.fake.report(1);
        r.turn(r.now + 1000, budget(0, 0, 8, 0));
        CHECK(r.done(lost).service.outcome == C::SdoOutcome::TIMEOUT && r.services.unresolved(0));
        markerRefused(r, markerWork(r.now, 1, 0x2346), O::BusAdmission::INVALID);
        CHECK(r.services.checkDrain(0, markerWork(r.now), fakePremises(), r.now));
    }
    {   // A stale binding or route is not this channel any more.
        Rig r;
        lose(r);
        C::SdoTransfer work = markerWork(r.now);
        ++work.channel.binding.bindingGeneration;
        markerRefused(r, work, O::BusAdmission::INVALID);
        work = markerWork(r.now);
        ++work.channel.binding.configurationGeneration;
        markerRefused(r, work, O::BusAdmission::INVALID);
        work = markerWork(r.now);
        ++work.channel.serverToClientId;
        markerRefused(r, work, O::BusAdmission::INVALID);
    }
    {   // History holds four pairs, repeats cost nothing, and the fifth refuses from then on.
        Rig r;
        lose(r);
        stray(r, uploaded(1, 0x3001));
        stray(r, uploaded(1, 0x3001));
        stray(r, uploaded(1, 0x3002));
        stray(r, serverFrame(1, 0x60, 0x3003));
        CHECK(r.services.checkDrain(0, markerWork(r.now), fakePremises(), r.now));
        markerRefused(r, markerWork(r.now, 1, 0x3003), O::BusAdmission::INVALID);
        stray(r, serverAbort(1, 0x3004, 0x06020000u));
        markerRefused(r, markerWork(r.now), O::BusAdmission::INVALID);
        r.turn(r.now + 9000, budget());
        markerRefused(r, markerWork(r.now), O::BusAdmission::INVALID);
        CHECK(r.services.unresolved(0));
    }
    {   // A frame naming no object refuses, whether segment-shaped or short.
        for (unsigned shape = 0; shape < 3; ++shape) {
            Rig r;
            lose(r);
            CHECK(r.services.checkDrain(0, markerWork(r.now), fakePremises(), r.now));
            stray(r, shape == 0   ? serverFrame(1, 0x00, 0x1111)
                     : shape == 1 ? serverFrame(1, 0x20, 0x1111)
                                  : frame(0x581, 7));
            markerRefused(r, markerWork(r.now), O::BusAdmission::INVALID);
        }
    }
    {   // An unreported handoff holds the channel, and only its final report releases it.
        Rig r;
        const auto sequence = r.sequence();
        const auto id = r.admit(r.request(sequence, 1, true));
        r.turn(0, budget(0, 1, 0, 1));
        r.turn(1000, budget(0, 0, 8, 0));
        CHECK(r.done(id).service.outcome == C::SdoOutcome::TIMEOUT && r.fake.retireCalls == 1);
        CHECK(!r.owner.localTxRetired(0));
        markerRefused(r, markerWork(r.now), O::BusAdmission::INHIBITED);
        r.fake.report(0);
        r.turn(r.now, budget(0, 0, 8, 0));
        CHECK(r.owner.localTxRetired(0));
        CHECK(r.services.checkDrain(0, markerWork(r.now), fakePremises(), r.now));
    }
    {   // An NMT frame still owned by the controller holds this node's channel as well.
        Rig r;
        lose(r);
        C::NmtTransfer nmt;
        nmt.command = C::NmtCommand::START;
        nmt.target = r.services.node(0)->route;
        nmt.request = 123;
        nmt.deadlineUs = r.now + 100;
        nmt.queueTimeoutUs = 20;
        C::NmtRequestId id;
        CHECK(r.owner.admitNmt(nmt, r.now, id));
        r.turn(r.now, budget(0, 1, 0, 0));
        CHECK(r.fake.sent[r.fake.sentCount - 1].id == 0);
        const unsigned sent = r.fake.sentCount;
        markerRefused(r, markerWork(r.now), O::BusAdmission::INHIBITED);
        CHECK(r.fake.sentCount == sent && r.services.unresolved(0));
    }
    {   // A marker in flight holds its channel against a second one.
        Rig r;
        lose(r);
        const auto first = sendMarker(r, markerWork(r.now));
        markerRefused(r, markerWork(r.now, 1, 0x2001), O::BusAdmission::INHIBITED);
        r.turn(r.now + 1000, budget(0, 0, 8, 0));
        CHECK(r.done(first).service.outcome == C::SdoOutcome::TIMEOUT);
        // Its retained result occupies the only urgent slot until released.
        markerRefused(r, markerWork(r.now, 1, 0x2001), O::BusAdmission::RESULTS_FULL);
        CHECK(r.owner.release(first));
        CHECK(r.services.checkDrain(0, markerWork(r.now, 1, 0x2001), fakePremises(), r.now));
    }
    {   // A staged cleanup abort is local work on this route.
        Rig r;
        const auto sequence = r.sequence();
        O::BusRequest read = r.request(sequence);
        read.transfer.allowSegmentedUpload = false;
        const auto id = r.admit(read);
        r.turn(0, budget(0, 1, 0, 1));
        r.fake.report(0);
        M::Frame initial = serverFrame(1, 0x41, LOST_INDEX);
        initial.data[4] = 2;
        r.fake.push(initial, 1);
        r.turn(1, budget(8, 0, 8, 0));
        CHECK(r.done(id).service.outcome == C::SdoOutcome::UNSUPPORTED);
        CHECK(r.done(id).service.clientAbortQueued && r.services.port(0)->queueOccupied);
        CHECK(r.services.unresolved(0));
        markerRefused(r, markerWork(r.now), O::BusAdmission::INVALID);
        r.turn(r.now, budget(0, 1, 0, 0));
        CHECK(r.fake.sentCount == 2 && r.fake.sent[1].data[0] == 0x80);
        markerRefused(r, markerWork(r.now), O::BusAdmission::INHIBITED); // Abort not reported yet.
    }
    {   // An inhibited owner refuses first, and the controller's own state refuses underneath.
        Rig r;
        lose(r);
        CHECK(r.owner.reportController(C::ControllerState::BUS_OFF, r.now));
        markerRefused(r, markerWork(r.now), O::BusAdmission::INHIBITED);
        CHECK(!r.services.checkDrain(0, markerWork(r.now), fakePremises(), r.now));
        O::RecoveryId recovery;
        CHECK(r.owner.recover(r.now, r.now + 100, recovery));
        markerRefused(r, markerWork(r.now), O::BusAdmission::INHIBITED);
        CHECK(!r.services.checkDrain(0, markerWork(r.now), fakePremises(), r.now));
        retireRecovery(r, recovery);
        CHECK(r.services.unresolved(0)); // Recovery itself reopens nothing.
        CHECK(r.services.checkDrain(0, markerWork(r.now), fakePremises(), r.now));
    }
    {   // Known STOPPED and INITIALIZING refuse; a later state report restores eligibility.
        for (unsigned state = 0; state < 2; ++state) {
            Rig r;
            lose(r);
            stray(r, networkState(1, state ? 4 : 0));
            markerRefused(r, markerWork(r.now), O::BusAdmission::INVALID);
            stray(r, networkState(1, 0x7f));
            CHECK(r.services.checkDrain(0, markerWork(r.now), fakePremises(), r.now));
        }
    }
    {   // An exhausted node has no generation left to invalidate.
        C::SdoClient services;
        Fake fake;
        O::CanBusOwner owner(services, fake.transport(), 73);
        CHECK(services.configure(0, route(), true, true));
        C::NetworkRoute network;
        network.binding = route().binding;
        network.generation = UINT32_MAX;
        network.heartbeatId = 0x701;
        network.emcyId = 0x81;
        CHECK(services.configureNetwork(0, network, C::NetworkObserver(), 0));
        CHECK(services.invalidateNetwork(0, C::NetworkInvalidation::BINDING, 0));
        CHECK(services.node(0)->exhausted && services.unresolved(0));
        O::RequestId output;
        CHECK(owner.admitDrainMarker(markerWork(0), fakePremises(), 0, output) == O::BusAdmission::INVALID);
    }
    {   // No request identity is left to name the marker.
        Rig r(UINT32_MAX);
        stray(r, networkState(1, 0));
        CHECK(r.services.unresolved(0));
        stray(r, networkState(1, 0x7f));
        markerRefused(r, markerWork(r.now), O::BusAdmission::IDS_EXHAUSTED);
    }
}

void pendingMarkerTraffic() {
    {   // An old reply for a retained pair is ignored, in any of its forms, and the marker settles.
        Rig r;
        lose(r);
        const auto id = sendMarker(r, markerWork(r.now));
        const uint32_t unmatched = r.owner.stats().unmatchedFrames;
        stray(r, serverFrame(1, 0x60, LOST_INDEX));
        stray(r, serverAbort(1, LOST_INDEX, 0x06020000u));
        CHECK(!r.owner.result(id) && r.services.unresolved(0));
        CHECK(r.owner.stats().unmatchedFrames == unmatched + 2);
        // Extended and RTR frames are not this service and leave the marker alone.
        M::Frame other = uploaded(1, 0x3333);
        other.extended = true;
        stray(r, other);
        CHECK(!r.owner.result(id));
        stray(r, uploaded());
        CHECK(r.done(id).service.outcome == C::SdoOutcome::SUCCESS);
        CHECK(!r.services.unresolved(0) && r.services.requalifications(0) == 1);
    }
    {   // A marker forbids segmentation, so a segmented answer fails it at once: no segment
        // request, one cleanup abort, and its own pair is retained.
        Rig r;
        lose(r);
        const auto id = sendMarker(r, markerWork(r.now));
        const unsigned sent = r.fake.sentCount;
        M::Frame initial = serverFrame(1, 0x41, MARKER_INDEX);
        initial.data[4] = 2;
        stray(r, initial);
        CHECK(r.done(id).service.outcome == C::SdoOutcome::UNSUPPORTED);
        CHECK(r.done(id).service.clientAbortQueued && r.services.unresolved(0));
        CHECK(!r.services.drainSettled(0) && r.services.requalifications(0) == 0);
        r.turn(r.now, budget(0, 1, 0, 0));
        CHECK(r.fake.sentCount == sent + 1 && r.fake.sent[sent].data[0] == 0x80);
        r.fake.report(sent);
        r.turn(r.now, budget(0, 0, 8, 0));
        CHECK(r.owner.release(id));
        markerRefused(r, markerWork(r.now), O::BusAdmission::INVALID);
        CHECK(r.services.checkDrain(0, markerWork(r.now, 1, 0x2001), fakePremises(), r.now));
    }
    {   // A frame after a reopening disproves the premise that reopening rested on. The route
        // quarantines again and no later marker is accepted, even on a fresh object.
        Rig r;
        lose(r);
        const auto id = sendMarker(r, markerWork(r.now));
        stray(r, uploaded());
        CHECK(r.done(id).service.outcome == C::SdoOutcome::SUCCESS);
        CHECK(!r.services.unresolved(0) && r.services.requalifications(0) == 1);
        const Saved<O::Completion> settled(r.done(id));
        stray(r, serverFrame(1, 0x60, LOST_INDEX));
        CHECK(r.services.unresolved(0) && r.services.requalifications(0) == 1);
        settled.unchanged(r.done(id));
        CHECK(r.owner.release(id));
        markerRefused(r, markerWork(r.now, 1, LOST_INDEX), O::BusAdmission::INVALID);
        markerRefused(r, markerWork(r.now, 1, 0x2001), O::BusAdmission::INVALID);
        r.turn(r.now + 9000, budget());
        markerRefused(r, markerWork(r.now, 1, 0x2001), O::BusAdmission::INVALID);
        CHECK(r.services.unresolved(0));
    }
    {   // So does a wrong-object frame on later live work. It does not end that work or
        // close the route, but the next loss cannot be drained.
        Rig r;
        const Lost lost = lose(r);
        const auto id = sendMarker(r, markerWork(r.now));
        stray(r, uploaded());
        CHECK(r.services.requalifications(0) == 1);
        CHECK(r.owner.release(id) && r.owner.release(lost.request));
        CHECK(r.owner.releaseSequence(lost.sequence));
        const auto sequence = r.sequence();
        const auto read = r.admit(r.request(sequence, 1, false, 0x2347));
        r.turn(r.now, budget(0, 1, 0, 1));
        r.fake.report(r.fake.sentCount - 1);
        stray(r, uploaded(1, 0x3000));
        CHECK(!r.owner.result(read) && !r.services.unresolved(0));
        stray(r, uploaded(1, 0x2347));
        CHECK(r.done(read).service.outcome == C::SdoOutcome::SUCCESS && !r.services.unresolved(0));
        const auto write = r.admit(r.request(sequence, 2, true));
        r.turn(r.now, budget(0, 1, 0, 1));
        r.fake.report(r.fake.sentCount - 1);
        r.turn(r.now + 1000, budget(0, 0, 8, 0));
        CHECK(r.done(write).service.outcome == C::SdoOutcome::TIMEOUT && r.services.unresolved(0));
        markerRefused(r, markerWork(r.now, 1, 0x2001), O::BusAdmission::INVALID);
    }
    {   // An unrecorded stray fails the marker. Both it and the marker are now retained.
        Rig r;
        lose(r);
        const auto id = sendMarker(r, markerWork(r.now));
        const unsigned sent = r.fake.sentCount;
        stray(r, uploaded(1, 0x3333));
        const C::SdoCompletion& failed = r.done(id).service;
        CHECK(failed.outcome == C::SdoOutcome::CORRELATION_LOST && failed.size == 0 &&
              !failed.protocolAcknowledged);
        CHECK(r.services.unresolved(0) && !r.services.drainSettled(0));
        CHECK(r.services.requalifications(0) == 0);
        CHECK(r.owner.release(id));
        markerRefused(r, markerWork(r.now, 1, 0x3333), O::BusAdmission::INVALID);
        markerRefused(r, markerWork(r.now), O::BusAdmission::INVALID);
        markerRefused(r, markerWork(r.now, 1, LOST_INDEX), O::BusAdmission::INVALID);
        r.turn(r.now + 9000, budget());
        CHECK(r.fake.sentCount == sent && r.services.unresolved(0)); // Nothing retried by itself.
        // A further attempt is a new explicit request with a different object.
        const auto second = sendMarker(r, markerWork(r.now, 1, 0x2001));
        CHECK(second.generation != id.generation);
        stray(r, uploaded(1, 0x2001));
        CHECK(r.done(second).service.outcome == C::SdoOutcome::SUCCESS);
        CHECK(!r.services.unresolved(0) && r.services.requalifications(0) == 1);
    }
    {   // A malformed or segment frame fails it and refuses every later marker.
        for (unsigned shape = 0; shape < 3; ++shape) {
            Rig r;
            lose(r);
            const auto id = sendMarker(r, markerWork(r.now));
            stray(r, shape == 0   ? frame(0x581, 7)
                     : shape == 1 ? serverFrame(1, 0x00, MARKER_INDEX)
                                  : serverFrame(1, 0x20, MARKER_INDEX));
            CHECK(r.done(id).service.outcome == C::SdoOutcome::CORRELATION_LOST);
            CHECK(r.owner.release(id));
            markerRefused(r, markerWork(r.now, 1, 0x2001), O::BusAdmission::INVALID);
            r.turn(r.now + 9000, budget());
            markerRefused(r, markerWork(r.now, 1, 0x2001), O::BusAdmission::INVALID);
            CHECK(r.services.unresolved(0));
        }
    }
    {   // A server abort on the marker's own pair settles the marker and reopens nothing.
        Rig r;
        lose(r);
        const auto id = sendMarker(r, markerWork(r.now));
        stray(r, serverAbort(1, MARKER_INDEX, 0x06020000u));
        const C::SdoCompletion& failed = r.done(id).service;
        CHECK(failed.outcome == C::SdoOutcome::SERVER_ABORT && failed.abortCode == 0x06020000u);
        CHECK(r.services.unresolved(0) && !r.services.drainSettled(0));
        CHECK(r.services.requalifications(0) == 0);
        CHECK(r.owner.release(id));
        markerRefused(r, markerWork(r.now), O::BusAdmission::INVALID);
        CHECK(r.services.checkDrain(0, markerWork(r.now, 1, 0x2001), fakePremises(), r.now));
    }
    {   // A marker timeout stays unresolved, retains its own pair and is not sent twice.
        Rig r;
        lose(r);
        const auto id = sendMarker(r, markerWork(r.now));
        const unsigned sent = r.fake.sentCount;
        r.turn(r.now + 1000, budget());
        CHECK(r.done(id).service.outcome == C::SdoOutcome::TIMEOUT);
        CHECK(r.services.unresolved(0) && r.fake.sentCount == sent);
        CHECK(r.owner.release(id));
        markerRefused(r, markerWork(r.now), O::BusAdmission::INVALID);
        CHECK(r.services.checkDrain(0, markerWork(r.now, 1, 0x2001), fakePremises(), r.now));
    }
    {   // A reply stamped before the marker's handoff cannot be the marker's reply.
        Rig r;
        lose(r);
        const uint64_t before = r.now;
        r.now += 5;
        const auto id = sendMarker(r, markerWork(r.now));
        r.fake.push(uploaded(), before);
        r.turn(r.now, budget(8, 0, 8, 0));
        CHECK(r.done(id).service.outcome == C::SdoOutcome::CORRELATION_LOST);
        CHECK(r.services.unresolved(0) && r.services.requalifications(0) == 0);
    }
    {   // Cancelling an unsent marker retains nothing; cancelling a sent one retains its pair.
        Rig r;
        lose(r);
        const auto unsent = admitMarker(r, markerWork(r.now));
        CHECK(r.owner.cancel(unsent, r.now) == O::Cancel::CANCELLED);
        CHECK(r.owner.release(unsent) && r.services.unresolved(0) && r.fake.sentCount == 1);
        const auto sent = sendMarker(r, markerWork(r.now));
        CHECK(r.owner.cancel(sent, r.now) == O::Cancel::CANCELLED);
        CHECK(r.owner.release(sent) && r.services.unresolved(0));
        markerRefused(r, markerWork(r.now), O::BusAdmission::INVALID);
    }
    {   // An observed frame before the marker starts removes its object from eligibility.
        Rig r;
        lose(r);
        const auto id = admitMarker(r, markerWork(r.now));
        stray(r, uploaded());
        r.turn(r.now, budget(0, 2, 0, 2));
        CHECK(r.fake.sentCount == 1 && !r.owner.result(id));
        r.turn(r.now + 1000, budget());
        CHECK(r.done(id).service.outcome == C::SdoOutcome::TIMEOUT && r.fake.sentCount == 1);
        CHECK(r.services.unresolved(0));
    }
    {   // A marker queued before a boot belongs to the old session and never starts in the new one.
        Rig r;
        lose(r);
        const auto id = admitMarker(r, markerWork(r.now));
        const uint32_t generation = r.services.node(0)->route.generation;
        stray(r, networkState(1, 0));
        CHECK(r.services.node(0)->route.generation == generation + 1);
        CHECK(r.done(id).reason == O::CompletionReason::INVALIDATED);
        CHECK(!r.done(id).service.transmissionPossible && r.fake.sentCount == 1);
        CHECK(r.services.unresolved(0));
        CHECK(r.owner.release(id));
        // The same object remains available to a new explicit marker in the new session.
        stray(r, networkState(1, 0x7f));
        const auto next = sendMarker(r, markerWork(r.now));
        stray(r, uploaded());
        CHECK(r.done(next).service.outcome == C::SdoOutcome::SUCCESS && !r.services.unresolved(0));
    }
    {   // Recovery ends a pending marker: it stays unresolved and keeps the pair.
        Rig r;
        lose(r);
        const auto id = sendMarker(r, markerWork(r.now));
        O::RecoveryId recovery;
        CHECK(r.owner.recover(r.now, r.now + 100, recovery));
        CHECK(r.done(id).reason == O::CompletionReason::RECOVERY);
        retireRecovery(r, recovery);
        CHECK(r.services.unresolved(0) && r.services.requalifications(0) == 0);
        CHECK(r.owner.release(id));
        markerRefused(r, markerWork(r.now), O::BusAdmission::INVALID);
    }
}

void markerSuccessThroughOwner() {
    Rig r;
    // Node 1: an acknowledged write, then a lost one. Node 2: a live reservation.
    const auto sequence = r.sequence(1, 1);
    const auto acknowledged = r.admit(r.request(sequence, 1, true));
    r.turn(0, budget(0, 1, 0, 1));
    r.complete(acknowledged, 1, true);
    const auto lost = r.admit(r.request(sequence, 2, true));
    r.turn(r.now, budget(0, 1, 0, 1));
    r.fake.report(1);
    r.turn(r.now + 1000, budget(0, 0, 8, 0));
    const auto other = r.sequence(2, 2);
    const uint32_t otherGeneration = r.owner.sequence(other)->nodeGeneration;
    CHECK(r.done(lost).service.effectUncertain && r.services.unresolved(0));
    CHECK(!r.owner.sequence(sequence) && r.owner.sequence(other));
    const Saved<O::Completion> ackResult(r.done(acknowledged)), lostResult(r.done(lost));
    const uint32_t generation = r.services.node(0)->route.generation;
    const auto id = sendMarker(r, markerWork(r.now));
    CHECK(r.fake.sentCount == 3 && !r.services.drainSettled(0));
    CHECK(r.services.requalifications(0) == 0 && r.services.unresolved(0));
    CHECK(!r.owner.sequence(sequence)); // The pending marker lends the old reservation nothing.
    stray(r, uploaded());
    const O::Completion marker = r.done(id);
    CHECK(marker.reason == O::CompletionReason::SERVICE && !marker.sequence.owner);
    CHECK(marker.service.outcome == C::SdoOutcome::SUCCESS && marker.service.protocolAcknowledged);
    CHECK(marker.service.size == 2 && marker.service.data[0] == 0x34 && marker.service.data[1] == 0x12);
    // The route is open, the settle flag is consumed and the count is retained.
    CHECK(!r.services.unresolved(0) && !r.services.drainSettled(0));
    CHECK(r.services.requalifications(0) == 1 && r.services.requalifications(1) == 0);
    CHECK(!r.services.reopenAfterDrain(0));
    CHECK(r.services.node(0)->route.generation == generation);
    // Earlier uncertain results are unchanged and nothing replays.
    ackResult.unchanged(r.done(acknowledged));
    lostResult.unchanged(r.done(lost));
    CHECK(r.done(lost).service.effectUncertain && !r.done(lost).service.protocolAcknowledged);
    r.turn(r.now + 1, budget());
    CHECK(r.fake.sentCount == 3);
    // Every reservation on the node is invalid, including the one holding the old 000F history.
    CHECK(!r.owner.sequence(sequence));
    CHECK(!r.owner.beginOperation(sequence, 2, r.now + 1000, r.now));
    M::Binding changed = route().binding;
    ++changed.configurationGeneration;
    CHECK(!r.owner.advanceConfiguration(sequence, changed, r.now));
    O::BusRequest oldWork = r.request(other);
    oldWork.sequence = sequence;
    oldWork.transfer.channel = route(1);
    oldWork.transfer.operation = oldWork.transfer.request = 1;
    oldWork.transfer.step = 3;
    O::RequestId output;
    CHECK(r.owner.admit(oldWork, r.now, output) == O::BusAdmission::INVALID);
    CHECK(r.owner.admitUrgent(oldWork, r.now, output) == O::BusAdmission::INVALID);
    // The other node's reservation is untouched.
    const O::Sequence* live = r.owner.sequence(other);
    CHECK(live && live->nodeGeneration == otherGeneration);
    const auto independent = r.admit(r.request(other));
    r.turn(r.now, budget(0, 1, 0, 1));
    CHECK(r.fake.sentCount == 4);
    expectUpload(r.fake.sent[3], 2, LOST_INDEX);
    r.complete(independent, 2);
    // New work needs a new reservation; the invalid one must be released first.
    O::SequenceId fresh;
    CHECK(!r.owner.beginSequence(route(1).binding, 1, r.now, fresh));
    CHECK(r.owner.release(acknowledged) && r.owner.release(lost) && r.owner.releaseSequence(sequence));
    CHECK(r.owner.beginSequence(route(1).binding, 1, r.now, fresh));
    CHECK(fresh.generation != sequence.generation && r.owner.beginOperation(fresh, 1, r.now + 1000, r.now));
    const auto after = r.admit(r.request(fresh, 1, false, 0x2347));
    r.turn(r.now, budget(0, 1, 0, 1));
    CHECK(r.fake.sentCount == 5);
    expectUpload(r.fake.sent[4], 1, 0x2347);
    r.complete(after, 1, false, 0x2347);
    CHECK(r.done(after).service.outcome == C::SdoOutcome::SUCCESS);
    // Reopening consumed the history. A later loss starts it again: only that request's pair is
    // retained, so the object lost before the first marker can serve as the next marker. With
    // nothing unexpected in between, a second clean reopening is allowed and counted.
    const auto again = r.admit(r.request(fresh, 2, true, 0x2346));
    r.turn(r.now, budget(0, 1, 0, 1));
    r.fake.report(r.fake.sentCount - 1);
    r.turn(r.now + 1000, budget(0, 0, 8, 0));
    CHECK(r.done(again).service.outcome == C::SdoOutcome::TIMEOUT && r.services.unresolved(0));
    markerRefused(r, markerWork(r.now, 1, 0x2346), O::BusAdmission::INVALID);
    CHECK(r.owner.release(id));
    const auto second = sendMarker(r, markerWork(r.now, 1, LOST_INDEX));
    stray(r, uploaded(1, LOST_INDEX));
    CHECK(r.done(second).service.outcome == C::SdoOutcome::SUCCESS);
    CHECK(!r.services.unresolved(0) && r.services.requalifications(0) == 2);
    CHECK(!r.owner.sequence(fresh));
}

// A reply can overtake the marker's TX report. The route opens only once that frame is retired,
// or a duplicate reply could complete work admitted in between.
void markerReopensOnlyAfterItsTxRetires() {
    for (unsigned proved = 0; proved < 2; ++proved) {
        Rig r;
        const Lost lost = lose(r);
        const auto id = admitMarker(r, markerWork(r.now));
        r.turn(r.now, budget(0, 1, 0, 1));
        const unsigned sent = r.fake.sentCount - 1;
        expectUpload(r.fake.sent[sent], 1, MARKER_INDEX);
        r.fake.push(uploaded(), r.now + 1);
        r.turn(r.now + 1, budget(8, 0, 0, 0)); // No report budget, so no retirement callback.
        CHECK(r.done(id).service.outcome == C::SdoOutcome::SUCCESS);
        CHECK(r.services.drainSettled(0) && r.services.unresolved(0));
        CHECK(r.services.requalifications(0) == 0 && !r.owner.localTxRetired(0));
        O::SequenceId refused;
        CHECK(!r.owner.beginSequence(route(1).binding, 3, r.now, refused));
        CHECK(!r.services.checkAdmission(0, r.done(id).service.transfer, r.now));
        CHECK(!r.owner.sequence(lost.sequence));
        if (proved) {
            r.fake.retirement = true;
            r.turn(r.now, budget(0, 0, 8, 0));
            CHECK(r.fake.retireCalls >= 1);
        } else {
            r.turn(r.now + 1, budget(0, 0, 8, 0));
            CHECK(r.services.drainSettled(0) && r.services.unresolved(0));
            r.fake.report(sent);
            r.turn(r.now, budget(0, 0, 8, 0));
        }
        CHECK(r.owner.localTxRetired(0));
        CHECK(!r.services.unresolved(0) && !r.services.drainSettled(0));
        CHECK(r.services.requalifications(0) == 1 && !r.owner.sequence(lost.sequence));
    }
    // A duplicate before retirement is contrary evidence: the settlement is withdrawn.
    Rig r;
    lose(r);
    const auto id = admitMarker(r, markerWork(r.now));
    r.turn(r.now, budget(0, 1, 0, 1));
    const unsigned sent = r.fake.sentCount - 1;
    r.fake.push(uploaded(), r.now + 1);
    r.turn(r.now + 1, budget(8, 0, 0, 0));
    CHECK(r.services.drainSettled(0));
    const Saved<O::Completion> settled(r.done(id));
    r.fake.push(uploaded(), r.now + 1);
    r.turn(r.now + 1, budget(8, 0, 0, 0));
    CHECK(!r.services.drainSettled(0) && r.services.unresolved(0));
    settled.unchanged(r.done(id));
    r.fake.report(sent);
    r.turn(r.now, budget(0, 0, 8, 0));
    CHECK(r.owner.localTxRetired(0) && r.services.unresolved(0));
    CHECK(r.services.requalifications(0) == 0 && r.fake.sentCount == sent + 1);
    CHECK(r.owner.release(id));
    markerRefused(r, markerWork(r.now), O::BusAdmission::INVALID);
    O::SequenceId refused;
    CHECK(!r.owner.beginSequence(route(1).binding, 3, r.now, refused));
}

// A marker settles the channel, not the server: neither its abort nor its success is evidence.
void markerTerminalDoesNotQualifyService() {
    {
        Rig r;
        CHECK(r.services.invalidateNetwork(0, C::NetworkInvalidation::BINDING, 0));
        CHECK(r.services.unresolved(0) && !r.services.node(0)->serviceQualification.present);
        const auto failed = sendMarker(r, markerWork(r.now));
        stray(r, serverAbort(1, MARKER_INDEX, 0x06020000u));
        CHECK(r.done(failed).service.outcome == C::SdoOutcome::SERVER_ABORT);
        CHECK(!r.services.node(0)->serviceQualification.present && !r.services.node(0)->sdo.present);
        CHECK(r.owner.release(failed));
        const auto id = sendMarker(r, markerWork(r.now, 1, 0x2001));
        stray(r, uploaded(1, 0x2001));
        CHECK(r.done(id).service.outcome == C::SdoOutcome::SUCCESS && !r.services.unresolved(0));
        CHECK(!r.services.node(0)->serviceQualification.present && !r.services.node(0)->sdo.present);
        C::ServiceEligibility eligibility;
        CHECK(C::serviceEligibility(*r.services.node(0), r.now, eligibility));
        CHECK(!eligibility.ordinarySdo && eligibility.probeUpload);
        // Ordinary work needs fresh evidence. An accepted upload supplies it on the open route.
        const auto sequence = r.sequence();
        O::RequestId output;
        CHECK(r.owner.admit(r.request(sequence, 1, true), r.now, output) == O::BusAdmission::INVALID);
        O::BusRequest accepted = r.request(sequence, 2);
        accepted.transfer.readEffect = C::SdoReadEffect::ACCEPTED_UNQUALIFIED;
        const auto read = r.admit(accepted);
        r.turn(r.now, budget(0, 1, 0, 1));
        expectUpload(r.fake.sent[r.fake.sentCount - 1], 1, LOST_INDEX);
        r.fake.report(r.fake.sentCount - 1);
        stray(r, uploaded(1, LOST_INDEX));
        CHECK(r.done(read).service.outcome == C::SdoOutcome::SUCCESS);
        CHECK(r.services.node(0)->serviceQualification.present);
        CHECK(r.owner.admit(r.request(sequence, 3, true), r.now, output) == O::BusAdmission::ACCEPTED);
    }
}

// A matched abort reports rejection in every case. It leaves the route open only when it
// answers the one confirmed outstanding frame, the same premise a success has.
void abortOnLiveFrameQuarantines() {
    Rig r;
    const auto sequence = r.sequence();
    const auto id = r.admit(r.request(sequence, 1, true));
    r.turn(0, budget(0, 1, 0, 1));
    stray(r, serverAbort(1, LOST_INDEX, 0x06020000u)); // No TX report has arrived.
    const C::SdoCompletion& rejected = r.done(id).service;
    CHECK(rejected.outcome == C::SdoOutcome::SERVER_ABORT && rejected.abortCode == 0x06020000u);
    CHECK(rejected.effectPossible && rejected.effectUncertain);
    CHECK(r.services.unresolved(0) && !r.services.unresolved(1) && !r.owner.sequence(sequence));
    CHECK(!r.services.node(0)->sdo.present);
    markerRefused(r, markerWork(r.now), O::BusAdmission::INHIBITED);
    r.fake.report(0);
    r.turn(r.now, budget(0, 0, 8, 0));
    // The late report retires the frame. It does not rewrite the decision.
    CHECK(r.owner.localTxRetired(0) && r.services.unresolved(0));
    markerRefused(r, markerWork(r.now, 1, LOST_INDEX), O::BusAdmission::INVALID);
    CHECK(r.services.checkDrain(0, markerWork(r.now), fakePremises(), r.now));
}

// A rejected step ends what was queued behind it. The reservation stays for new explicit steps.
void rejectedStepEndsQueuedSteps() {
    Rig r;
    const auto sequence = r.sequence(1, 1), other = r.sequence(2, 2);
    const auto first = r.admit(r.request(sequence, 1, true));
    const auto second = r.admit(r.request(sequence, 2, true, 0x2346));
    const auto independent = r.admit(r.request(other));
    r.turn(0, budget(0, 1, 0, 1));
    CHECK(r.fake.sentCount == 1);
    expectWrite(r.fake.sent[0], 1, LOST_INDEX);
    r.fake.report(0);
    stray(r, serverAbort(1, LOST_INDEX, 0x06020000u));
    CHECK(r.done(first).service.outcome == C::SdoOutcome::SERVER_ABORT);
    const O::Completion& ended = r.done(second);
    CHECK(ended.reason == O::CompletionReason::INVALIDATED && !ended.service.transmissionPossible);
    CHECK(!ended.service.effectPossible && !ended.service.effectUncertain);
    CHECK(!r.owner.result(independent)); // Another reservation's step does not depend on it.
    CHECK(!r.services.unresolved(0) && r.owner.sequence(sequence));
    r.turn(r.now, budget(0, 2, 0, 2));
    CHECK(r.fake.sentCount == 2);
    expectUpload(r.fake.sent[1], 2, LOST_INDEX);
    r.turn(r.now + 10, budget());
    CHECK(r.fake.sentCount == 2); // Step 2 never left, and never will.
    const auto next = r.admit(r.request(sequence, 3, true, 0x2347));
    r.turn(r.now, budget(0, 1, 0, 1));
    CHECK(r.fake.sentCount == 3);
    expectWrite(r.fake.sent[2], 1, 0x2347);
    CHECK(!r.owner.result(next));
}

// A reply can overtake its TX report. The frame is still live, so a controller event then
// quarantines, though the settled result is not rewritten. A report first leaves it clean.
void successOvertakingItsReportStaysPossible() {
    const ControllerEvent events[] = {ControllerEvent::RECOVERY, ControllerEvent::BUS_OFF};
    for (ControllerEvent event : events)
        for (unsigned reported = 0; reported < 2; ++reported) {
            Rig r;
            const auto sequence = r.sequence();
            const auto id = r.admit(r.request(sequence));
            r.turn(0, budget(0, 1, 0, 1));
            if (reported)
                r.fake.report(0);
            stray(r, uploaded(1, LOST_INDEX));
            CHECK(r.done(id).service.outcome == C::SdoOutcome::SUCCESS && !r.services.unresolved(0));
            const Saved<O::Completion> settled(r.done(id));
            O::RecoveryId recovery;
            raise(r, event, recovery);
            CHECK(r.services.unresolved(0) == !reported && !r.services.unresolved(1));
            settled.unchanged(r.done(id));
            CHECK(!r.done(id).service.effectUncertain);
            retireRecovery(r, recovery);
            CHECK(r.services.unresolved(0) == !reported);
            if (!reported) {
                // Nothing was retained, but that object's reply could still answer twice.
                markerRefused(r, markerWork(r.now, 1, LOST_INDEX), O::BusAdmission::INVALID);
                CHECK(r.services.checkDrain(0, markerWork(r.now), fakePremises(), r.now));
            }
        }
}


// A cleanup abort proved retired must not hold the route forever. Each proof releases it:
// a transmitted report, a failed report, and an accepted retirement callback.
void retiredCleanupAbortReleasesTheMarker() {
    for (unsigned proof = 0; proof < 3; ++proof) {
        Rig r;
        const auto sequence = r.sequence();
        O::BusRequest read = r.request(sequence);
        read.transfer.allowSegmentedUpload = false;
        const auto id = r.admit(read);
        r.turn(0, budget(0, 1, 0, 1));
        r.fake.report(0);
        M::Frame initial = serverFrame(1, 0x41, LOST_INDEX);
        initial.data[4] = 2;
        r.fake.push(initial, 1);
        r.turn(1, budget(8, 1, 8, 0));
        CHECK(r.done(id).service.outcome == C::SdoOutcome::UNSUPPORTED);
        CHECK(r.fake.sentCount == 2 && r.fake.sent[1].data[0] == 0x80);
        CHECK(!r.owner.localTxRetired(0));
        markerRefused(r, markerWork(r.now), O::BusAdmission::INHIBITED);
        if (proof == 2) {
            r.fake.retirement = true;
            r.turn(r.now + 1000, budget(0, 0, 8, 0)); // Cleanup cutoff passes, so the owner asks.
            CHECK(r.fake.retireCalls == 1);
        } else {
            r.fake.report(1, proof == 0);
            r.turn(r.now, budget(0, 0, 8, 0));
        }
        CHECK(r.owner.localTxRetired(0) && r.services.unresolved(0));
        markerRefused(r, markerWork(r.now, 1, LOST_INDEX), O::BusAdmission::INVALID);
        const auto marker = sendMarker(r, markerWork(r.now));
        stray(r, uploaded());
        CHECK(r.done(marker).service.outcome == C::SdoOutcome::SUCCESS);
        CHECK(!r.services.unresolved(0) && r.services.requalifications(0) == 1);
    }
}

// None of these is the required boundary. They leave the route blocked and send nothing.
void quietTimeHeartbeatEpochAndReadbackCannotReopen() {
    Rig r;
    const Lost lost = lose(r);
    const Saved<O::Completion> result(r.done(lost.request));
    C::SdoTransfer readback = markerWork(r.now, 1, LOST_INDEX);
    const auto blocked = [&](uint64_t now) {
        C::SdoTransfer work = markerWork(now);
        work.readEffect = C::SdoReadEffect::REVIEWED_NON_CHANGING;
        CHECK(r.services.unresolved(0) && !r.services.checkAdmission(0, work, now));
        CHECK(!r.owner.sequence(lost.sequence));
        O::SequenceId refused;
        CHECK(!r.owner.beginSequence(route(1).binding, 3, now, refused));
        CHECK(r.services.requalifications(0) == 0 && r.fake.sentCount == 1);
        result.unchanged(r.done(lost.request));
    };
    // Quiet time, longer than every timeout in this rig.
    r.turn(r.now + 1000000, budget());
    blocked(r.now);
    // A fresh heartbeat proves the node is on the bus, not that old replies are gone.
    stray(r, networkState(1, 5));
    CHECK(r.services.node(0)->heartbeat.present);
    C::ServiceEligibility eligibility;
    CHECK(C::serviceEligibility(*r.services.node(0), r.now, eligibility) && eligibility.ordinarySdo);
    blocked(r.now);
    // A same-object readback cannot tell a new reply from the old one. As a marker it is
    // refused; as a frame it is the same old pair again.
    readback = markerWork(r.now, 1, LOST_INDEX);
    markerRefused(r, readback, O::BusAdmission::INVALID);
    stray(r, serverFrame(1, 0x60, LOST_INDEX));
    blocked(r.now);
    // A new interface epoch and the application's own requalification are no boundary.
    O::RecoveryId recovery;
    const uint32_t epoch = r.owner.controllerEpoch();
    CHECK(r.owner.recover(r.now, r.now + 100, recovery));
    retireRecovery(r, recovery);
    CHECK(r.owner.controllerEpoch() == epoch + 1);
    blocked(r.now);
    M::Binding changed = route().binding;
    ++changed.configurationGeneration;
    CHECK(!r.owner.advanceConfiguration(lost.sequence, changed, r.now));
    // A reboot gives the node a new generation and the route still stays blocked.
    const uint32_t generation = r.services.node(0)->route.generation;
    r.fake.push(networkState(1, 0), r.now + 1, r.owner.controllerEpoch());
    r.turn(r.now + 1, budget(1, 0, 0, 0));
    CHECK(r.services.node(0)->route.generation == generation + 1);
    blocked(r.now);
    // A live client refuses another configuration of this route.
    CHECK(!r.services.configure(0, route(1), true, true));
    blocked(r.now);
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
    absentObjectAbortLeavesRouteOpen();
    controllerEventsQuarantineOnlyPossibleWork();
    bootAndBindingStillQuarantineIdleRoutes();
    markerRefusals();
    pendingMarkerTraffic();
    markerSuccessThroughOwner();
    quietTimeHeartbeatEpochAndReadbackCannotReopen();
    retiredCleanupAbortReleasesTheMarker();
    markerReopensOnlyAfterItsTxRetires();
    markerTerminalDoesNotQualifyService();
    abortOnLiveFrameQuarantines();
    rejectedStepEndsQueuedSteps();
    successOvertakingItsReportStaysPossible();
    std::printf("can_bus_recovery_test: %u checks\n", checks);
    return 0;
}
