// SPDX-License-Identifier: MIT
// Synthetic protocol/controller timelines; no fixture claims vendor behavior.
#include "CanBusOwner.h"
#include "OwnerCounters.h"
#include <MotorControlCAN/profiles/lichuan_c/Pdo.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <new>

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
M::Frame frame(uint32_t id, uint8_t size = 8) {
    M::Frame out;
    out.id = id;
    out.dlc = out.payloadLength = size;
    return out;
}
M::Frame reply(unsigned node = 1, uint16_t index = 0x2345, bool download = false) {
    M::Frame out = frame(0x580 + node);
    out.data[0] = download ? 0x60 : 0x4b;
    out.data[1] = uint8_t(index);
    out.data[2] = uint8_t(index >> 8);
    out.data[4] = 0x34;
    out.data[5] = 0x12;
    return out;
}
M::Frame heartbeat(unsigned node = 1) {
    M::Frame out = frame(0x700 + node, 1);
    out.data[0] = 5;
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
    enum { CAPACITY = 96 };
    E::ReceivedFrame rx[CAPACITY];
    E::TxReport reports[CAPACITY];
    C::SdoTxId ids[CAPACITY];
    uint32_t epochs[CAPACITY] = {};
    bool reportsRetired[CAPACITY] = {};
    M::Frame sent[CAPACITY];
    unsigned rxRead = 0, rxWrite = 0, reportRead = 0, reportWrite = 0, sentCount = 0;
    unsigned receiveCalls = 0, reportCalls = 0, observed = 0, retireCalls = 0;
    unsigned beginCalls = 0, pollCalls = 0;
    bool accept = true, retireAccepted = false, rejectGuard = false;
    E::RecoveryProgress progress = E::RecoveryProgress::PENDING;
    static bool receive(void* p, E::ReceivedFrame& out) {
        Fake& f = *static_cast<Fake*>(p);
        ++f.receiveCalls;
        if (f.rxRead == f.rxWrite)
            return false;
        out = f.rx[f.rxRead++];
        return true;
    }
    static bool transmit(void* p, const M::Frame& value, C::SdoTxId id, uint32_t epoch) {
        Fake& f = *static_cast<Fake*>(p);
        CHECK(f.sentCount < CAPACITY);
        f.sent[f.sentCount] = value;
        f.ids[f.sentCount] = id;
        f.epochs[f.sentCount++] = epoch;
        return f.accept;
    }
    static bool report(void* p, E::TxReport& out) {
        Fake& f = *static_cast<Fake*>(p);
        ++f.reportCalls;
        if (f.reportRead == f.reportWrite)
            return false;
        out = f.reports[f.reportRead++];
        return true;
    }
    static void observe(void* p, const E::ReceivedFrame&, uint64_t) {
        ++static_cast<Fake*>(p)->observed;
    }
    static bool guard(void* p, E::SequenceId, const C::SdoTransfer&, uint64_t) {
        return !static_cast<Fake*>(p)->rejectGuard;
    }
    static bool same(C::SdoTxId a, C::SdoTxId b) {
        return a.request.owner == b.request.owner && a.request.channel == b.request.channel &&
               a.request.generation == b.request.generation && a.frame == b.frame;
    }
    static bool retire(void* p, C::SdoTxId id, uint32_t epoch) {
        Fake& f = *static_cast<Fake*>(p);
        ++f.retireCalls;
        if (!f.retireAccepted)
            return false;
        // This synchronous fake has no outstanding callbacks. A positive proof
        // removes exact queued reports and prevents future reports for that
        // handoff; reports for other nodes or epochs remain independently owned.
        unsigned write = f.reportRead;
        for (unsigned read = f.reportRead; read < f.reportWrite; ++read)
            if (f.reports[read].controllerEpoch != epoch || !same(f.reports[read].id, id))
                f.reports[write++] = f.reports[read];
        f.reportWrite = write;
        for (unsigned i = 0; i < f.sentCount; ++i)
            if (f.epochs[i] == epoch && same(f.ids[i], id))
                f.reportsRetired[i] = true;
        return true;
    }
    static bool begin(void* p, uint32_t) {
        ++static_cast<Fake*>(p)->beginCalls;
        return true;
    }
    static E::RecoveryProgress poll(void* p, uint32_t) {
        Fake& f = *static_cast<Fake*>(p);
        ++f.pollCalls;
        if (f.progress == E::RecoveryProgress::RETIRED) {
            // No asynchronous callbacks exist in this fake; retirement drops
            // every pending ingress/report before accepting the new epoch.
            f.rxRead = f.rxWrite;
            f.reportRead = f.reportWrite;
        }
        return f.progress;
    }
    E::Transport transport() {
        E::Transport out;
        out.context = this;
        out.receive = receive;
        out.transmit = transmit;
        out.takeTxReport = report;
        out.observe = observe;
        out.beforeTransmit = guard;
        out.retire = retire;
        out.beginRecovery = begin;
        out.pollRecovery = poll;
        return out;
    }
    void push(const M::Frame& value, uint64_t captured, bool known = true, uint32_t epoch = 1) {
        CHECK(rxWrite < CAPACITY);
        auto& out = rx[rxWrite++];
        out.frame = value;
        out.receivedUs = captured;
        out.receivedTimeKnown = known;
        out.controllerEpoch = epoch;
    }
    void reportTx(unsigned sentIndex, bool transmitted = true, uint32_t epoch = 1) {
        CHECK(sentIndex < sentCount && reportWrite < CAPACITY);
        CHECK(epoch != epochs[sentIndex] || !reportsRetired[sentIndex]);
        auto& out = reports[reportWrite++];
        out.id = ids[sentIndex];
        out.transmitted = transmitted;
        out.controllerEpoch = epoch;
    }
};
struct Rig {
    C::SdoClient services;
    Fake fake;
    E::CanBusOwner owner;
    uint64_t now = 0;
    Rig() : owner(services, fake.transport(), 73) { configure(11); }
    void configure(uint32_t nodeGeneration, uint32_t bindingGeneration = 3) {
        for (uint8_t i = 0; i < 2; ++i) {
            auto channel = route(i + 1);
            channel.binding.bindingGeneration = bindingGeneration;
            CHECK(services.configure(i, channel, true, true));
            C::NetworkRoute network;
            network.binding = channel.binding;
            network.generation = nodeGeneration;
            network.heartbeatId = 0x701 + i;
            network.emcyId = 0x81 + i;
            CHECK(services.configureNetwork(i, network, C::NetworkObserver(), now));
        }
    }
    void retireSyntheticServerAndReconstruct(uint32_t nodeGeneration,
                                             uint32_t bindingGeneration = 3) {
        CHECK(owner.resources().active == 0 && owner.resources().queued == 0);
        CHECK(fake.rxRead == fake.rxWrite && fake.reportRead == fake.reportWrite);
        // This test server has no delayed replies, queued remote work or other
        // producer. Destruction explicitly retires its entire session after
        // successful controller recovery; changing a host token alone cannot.
        CHECK(!owner.resources().inhibited);
        services.~SdoClient();
        new (&services) C::SdoClient;
        configure(nodeGeneration, bindingGeneration);
    }
    void turn(uint64_t time, E::ServiceBudget b = budget()) {
        now = time;
        CHECK(owner.service(time, b));
    }
    E::Health health(unsigned node = 1) const {
        E::Health out;
        CHECK(owner.health(route(node).binding, out));
        return out;
    }
    E::SequenceId sequence(unsigned node = 1) {
        E::SequenceId out;
        CHECK(owner.beginSequence(route(node).binding, uint8_t(node), now, out));
        CHECK(owner.beginOperation(out, 1, 10000, now));
        return out;
    }
    E::BusRequest request(E::SequenceId sequence, unsigned step = 1, bool download = false) {
        const E::Sequence* s = owner.sequence(sequence);
        CHECK(s);
        E::BusRequest out;
        out.sequence = sequence;
        auto& t = out.transfer;
        t.channel = route(s->binding.endpoint.address);
        t.channel.binding = s->binding;
        t.operation = t.request = s->operation;
        t.step = step;
        t.index = 0x2345;
        t.size = 2;
        t.direction = download ? C::SdoDirection::DOWNLOAD : C::SdoDirection::UPLOAD;
        t.readEffect = C::SdoReadEffect::REVIEWED_NON_CHANGING;
        t.queueTimeoutUs = t.responseTimeoutUs = 100;
        t.deadlineUs = s->deadlineUs;
        t.data[0] = 0x34;
        t.data[1] = 0x12;
        return out;
    }
    E::RequestId admit(const E::BusRequest& request, bool urgent = false) {
        E::RequestId out;
        CHECK((urgent ? owner.admitUrgent(request, now, out) : owner.admit(request, now, out)) ==
              E::BusAdmission::ACCEPTED);
        return out;
    }
    const E::Completion& done(E::RequestId id) const {
        const E::Completion* out = owner.result(id);
        CHECK(out);
        return *out;
    }
};

void cachedStateAndKnownZero() {
    Rig r;
    CHECK(r.health().state == E::DriverState::UNINIT);
    E::Health unknown;
    unknown.totalSuccess = 99;
    const Saved<E::Health> saved(unknown);
    M::Binding missing = route().binding;
    ++missing.logicalAxisId;
    CHECK(!r.owner.health(missing, unknown));
    saved.unchanged(unknown);
    E::DriverState state = E::DriverState::OFFLINE;
    CHECK(!r.owner.driverState(missing, state) && state == E::DriverState::OFFLINE);
    const auto sequence = r.sequence();
    const auto id = r.admit(r.request(sequence));
    r.turn(0);
    CHECK(r.owner.stats().txFrames == 0 && r.fake.sentCount == 1);
    r.fake.push(reply(), 0);
    r.turn(0, budget(1, 0, 0, 0));
    const auto h = r.health();
    CHECK(h.state == E::DriverState::READY && h.totalSuccess == 1 && h.totalFailures == 0);
    CHECK(h.lastOkKnown && h.lastOkUs == 0 && h.lastSeenKnown && h.lastSeenUs == 0);
    CHECK(!h.lastErrorKnown && h.consecutiveFailures == 0);
    CHECK(r.done(id).service.protocolAcknowledged && !r.done(id).service.transmissionConfirmed);
    const unsigned receives = r.fake.receiveCalls, reports = r.fake.reportCalls;
    const unsigned sent = r.fake.sentCount, observed = r.fake.observed;
    const Saved<E::Stats> stats(r.owner.stats());
    for (unsigned i = 0; i < 20; ++i) {
        CHECK(r.owner.driverState(route().binding, state) && state == E::DriverState::READY);
        CHECK(r.owner.health(route().binding, unknown));
        CHECK(r.owner.resources().retained == 1 && r.owner.result(id));
    }
    stats.unchanged(r.owner.stats());
    CHECK(receives == r.fake.receiveCalls && reports == r.fake.reportCalls &&
          sent == r.fake.sentCount && observed == r.fake.observed);
    // A released native/owner terminal cannot discard an outstanding final TX report.
    CHECK(r.owner.release(id));
    r.fake.reportTx(0, true, 2);
    r.fake.reportTx(0);
    r.fake.reportTx(0);
    r.fake.reportTx(0, false);
    r.turn(1, budget(0, 0, 8, 0));
    CHECK(r.owner.stats().txFrames == 1 && r.owner.stats().txRejected == 0);
    E::Diagnostic diagnostic;
    unsigned txCopies = 0, rxCopies = 0, reportsCopied = 0;
    while (r.owner.takeDiagnostic(diagnostic)) {
        if (diagnostic.kind == E::DiagnosticKind::TX) {
            ++txCopies;
            CHECK(diagnostic.accepted && diagnostic.txId.frame == r.fake.ids[0].frame &&
                  diagnostic.txId.request.generation == r.fake.ids[0].request.generation);
        } else if (diagnostic.kind == E::DiagnosticKind::RX) {
            ++rxCopies;
            CHECK(diagnostic.receivedTimeKnown && diagnostic.receivedUs == 0 &&
                  diagnostic.frame.id == 0x581 && diagnostic.frame.data[4] == 0x34);
        } else if (diagnostic.kind == E::DiagnosticKind::TX_REPORT) {
            ++reportsCopied;
            CHECK(diagnostic.accepted && diagnostic.txId.frame == r.fake.ids[0].frame);
        }
    }
    CHECK(txCopies == 1 && rxCopies == 1 && reportsCopied == 1);
}

void abortFailureThenIndependentRead() {
    Rig r;
    const auto sequence = r.sequence();
    const auto failed = r.admit(r.request(sequence));
    r.turn(0);
    r.fake.reportTx(0);
    M::Frame abort = reply();
    abort.data[0] = 0x80;
    abort.data[4] = 0x67;
    abort.data[5] = 0x45;
    abort.data[6] = 0x23;
    abort.data[7] = 0xf1;
    r.fake.push(abort, 0);
    r.turn(0, budget(1, 0, 8, 0));
    auto h = r.health();
    CHECK(h.totalSuccess == 0 && h.totalFailures == 1 && h.consecutiveFailures == 1);
    CHECK(h.lastErrorKnown && h.lastErrorUs == 0 && h.lastSeenKnown && h.lastSeenUs == 0);
    CHECK(h.state == E::DriverState::DEGRADED);
    CHECK(r.done(failed).service.abortValid && r.done(failed).service.abortCode == 0xf1234567u);
    const E::Completion failedOperation = r.done(failed);
    const Saved<E::Completion> terminal(failedOperation);
    const M::Status error = h.lastError;
    r.fake.push(heartbeat(), 1);
    r.turn(1, budget(1, 0, 0, 0));
    CHECK(r.health().state == E::DriverState::DEGRADED && r.health().totalSuccess == 0);
    CHECK(r.services.unresolved(0));
    E::Diagnostic diagnostic, failedHistory;
    bool historyFound = false;
    while (r.owner.takeDiagnostic(diagnostic))
        if (diagnostic.kind == E::DiagnosticKind::TERMINAL) {
            failedHistory = diagnostic;
            historyFound = true;
        }
    CHECK(historyFound && failedHistory.completion.service.abortCode == 0xf1234567u);
    // Retained results occupy their axis reservation. The application keeps its
    // failed operation copy while explicitly releasing only owner storage.
    CHECK(r.owner.release(failed) && r.owner.releaseSequence(sequence));
    E::RecoveryId recovery;
    CHECK(r.owner.recover(1, 10, recovery));
    r.fake.progress = E::RecoveryProgress::RETIRED;
    r.turn(2, budget(0, 0, 0, 0, 1));
    CHECK(r.owner.releaseRecovery(recovery));
    r.retireSyntheticServerAndReconstruct(12);
    CHECK(r.health().consecutiveFailures == 1);
    const auto freshSequence = r.sequence();
    const auto reconcile = r.admit(r.request(freshSequence));
    r.turn(2);
    r.fake.reportTx(1, true, r.owner.controllerEpoch());
    r.fake.push(reply(), 3, true, r.owner.controllerEpoch());
    r.turn(3, budget(1, 0, 8, 0));
    h = r.health();
    CHECK(h.totalSuccess == 1 && h.totalFailures == 1 && h.consecutiveFailures == 0);
    CHECK(h.state == E::DriverState::READY && h.lastOkKnown && h.lastOkUs == 3);
    CHECK(h.lastErrorKnown && h.lastErrorUs == 0 && h.lastError.code == error.code &&
          h.lastError.detail == error.detail && h.lastSeenUs == 3);
    terminal.unchanged(failedOperation);
    CHECK(r.done(reconcile).service.protocolAcknowledged);
    CHECK(failedHistory.completion.service.abortCode == 0xf1234567u &&
          failedHistory.completion.service.outcome == C::SdoOutcome::SERVER_ABORT);
    CHECK(r.owner.clearStats());
    h = r.health();
    CHECK(h.state == E::DriverState::READY && h.totalSuccess == 0 && h.totalFailures == 0);
    CHECK(h.lastOkKnown && h.lastOkUs == 3 && h.lastErrorKnown && h.lastErrorUs == 0 &&
          h.lastSeenKnown && h.lastSeenUs == 3 && h.lastError.detail == error.detail);
}

void freshHeartbeatPreservesServiceFailure() {
    Rig r;
    CHECK(r.services.setProducerInterval(0, r.services.node(0)->route, 1, 0));
    CHECK(r.owner.setHeartbeatMaximumAge(route().binding, 5, 0));
    const auto sequence = r.sequence();
    const auto id = r.admit(r.request(sequence));
    r.turn(0);
    r.fake.reportTx(0, false);
    r.fake.push(heartbeat(), 1);
    r.turn(1, budget(1, 0, 8, 0));
    CHECK(r.done(id).service.outcome == C::SdoOutcome::TRANSPORT_ERROR);
    CHECK(r.health().state == E::DriverState::DEGRADED && r.health().totalFailures == 1);
    r.turn(6, E::ServiceBudget());
    CHECK(r.health().state == E::DriverState::DEGRADED);
    r.turn(7, E::ServiceBudget());
    CHECK(r.health().state == E::DriverState::OFFLINE);
    r.fake.push(heartbeat(), 7);
    r.turn(7, budget(1, 0, 0, 0));
    CHECK(r.health().state == E::DriverState::DEGRADED && r.health().consecutiveFailures == 1 &&
          r.health().totalSuccess == 0 && r.health().lastErrorUs == 1);
    CHECK(r.services.unresolved(0));
}

void heartbeatPolicyAndControllerRecovery() {
    Rig r;
    CHECK(r.owner.setHeartbeatMaximumAge(route().binding, 5, 0));
    r.turn(50, E::ServiceBudget());
    CHECK(r.health().state ==
          E::DriverState::UNINIT); // Unknown producer, no invented startup timer.
    CHECK(r.services.setProducerInterval(0, r.services.node(0)->route, 1, 50));
    r.turn(60, E::ServiceBudget());
    CHECK(r.health().state == E::DriverState::UNINIT);
    r.fake.push(heartbeat(), 60);
    r.turn(60, budget(1, 0, 0, 0));
    CHECK(r.health().state == E::DriverState::READY);
    r.turn(65, E::ServiceBudget());
    CHECK(r.health().state == E::DriverState::READY);
    r.turn(66, E::ServiceBudget());
    CHECK(r.health().state == E::DriverState::OFFLINE);
    // Communication projection alone does not reject an explicit service.
    const auto sequence = r.sequence();
    const auto read = r.admit(r.request(sequence));
    CHECK(r.owner.cancel(read, 66) == E::Cancel::CANCELLED);
    r.fake.push(heartbeat(), 66);
    r.turn(66, budget(1, 0, 0, 0));
    CHECK(r.health().state == E::DriverState::READY);
    CHECK(r.services.setProducerInterval(0, r.services.node(0)->route, 0, 66));
    r.turn(80, E::ServiceBudget());
    CHECK(r.health().state == E::DriverState::READY);
    CHECK(r.services.setProducerInterval(0, r.services.node(0)->route, 1, 80));
    CHECK(r.owner.setHeartbeatMaximumAge(route().binding, 0, 80));
    r.turn(90, E::ServiceBudget());
    CHECK(r.health().state == E::DriverState::READY);
    CHECK(r.owner.reportController(C::ControllerState::BUS_OFF, 90));
    CHECK(r.health().state == E::DriverState::OFFLINE && r.owner.stats().busOffEvents == 1);
    CHECK(r.owner.reportController(C::ControllerState::BUS_OFF, 90));
    CHECK(r.owner.stats().busOffEvents == 1);
    CHECK(!r.owner.reportController(C::ControllerState::AVAILABLE, 90));
    CHECK(r.health().state == E::DriverState::OFFLINE && r.owner.resources().inhibited);
    E::RecoveryId recovery;
    CHECK(r.owner.recover(90, 100, recovery));
    r.fake.progress = E::RecoveryProgress::RETIRED;
    r.turn(91, budget(0, 0, 0, 0, 1));
    CHECK(r.owner.recoveryResult(recovery)->outcome == E::RecoveryOutcome::RETIRED);
    CHECK(r.health().state == E::DriverState::UNINIT && !r.owner.resources().inhibited);
    CHECK(r.owner.stats().recoveries == 1 && r.fake.beginCalls == 1 && r.fake.pollCalls == 1);
    CHECK(r.owner.releaseRecovery(recovery));
    r.fake.push(heartbeat(), 92, true, r.owner.controllerEpoch());
    r.turn(92, budget(1, 0, 0, 0));
    CHECK(r.health().state == E::DriverState::READY);
    CHECK(r.health().totalSuccess == 0 && r.health().totalFailures == 0);
}

void localCancellationAndQueueExpiryAreHealthNeutral() {
    Rig r;
    const auto sequence = r.sequence();
    const auto queued = r.admit(r.request(sequence));
    CHECK(r.owner.cancel(queued, 0) == E::Cancel::CANCELLED);
    CHECK(r.owner.cancel(queued, 0) == E::Cancel::ALREADY_TERMINAL);
    const auto expires = r.admit(r.request(sequence, 2));
    r.turn(100, E::ServiceBudget());
    CHECK(r.done(expires).service.outcome == C::SdoOutcome::TIMEOUT);
    CHECK(r.owner.stats().cancelled == 1 && r.owner.stats().timeouts == 1);
    CHECK(r.health().totalFailures == 0 && r.health().totalSuccess == 0 &&
          r.health().consecutiveFailures == 0 && !r.health().lastErrorKnown);
    CHECK(r.fake.sentCount == 0);
    const auto groupA = r.admit(r.request(sequence, 3));
    const auto groupB = r.admit(r.request(sequence, 4));
    CHECK(r.owner.invalidate(sequence, 100));
    CHECK(r.owner.result(groupA) && r.owner.result(groupB));
    CHECK(r.owner.stats().cancelled == 3 && r.health().totalFailures == 0);
    Rig guard;
    const auto gs = guard.sequence();
    const auto denied = guard.admit(guard.request(gs, 1, true));
    guard.fake.rejectGuard = true;
    guard.turn(0);
    CHECK(guard.done(denied).reason == E::CompletionReason::PREFLIGHT_REJECTED);
    CHECK(guard.owner.stats().cancelled == 1 && guard.health().totalFailures == 0);
    Rig active;
    const auto as = active.sequence();
    const auto write = active.admit(active.request(as, 1, true));
    active.turn(0);
    CHECK(active.owner.cancel(write, 1) == E::Cancel::CANCELLED);
    CHECK(active.done(write).service.effectUncertain && active.owner.stats().cancelled == 1);
    CHECK(active.health().totalFailures == 0 && active.health().totalSuccess == 0);
}

void segmentedServiceCountsOnce() {
    Rig r;
    const auto sequence = r.sequence();
    auto work = r.request(sequence, 1, true);
    work.transfer.size = 8;
    const auto id = r.admit(work);
    r.turn(0);
    CHECK(r.fake.sent[0].data[0] == 0x21 && r.health().totalSuccess == 0);
    r.fake.reportTx(0);
    r.fake.push(reply(1, 0x2345, true), 1);
    r.turn(1);
    CHECK(r.fake.sentCount == 2 && r.health().totalSuccess == 0);
    M::Frame ack = frame(0x581);
    ack.data[0] = 0x20;
    r.fake.reportTx(1);
    r.fake.push(ack, 2);
    r.turn(2);
    CHECK(r.fake.sentCount == 3 && r.health().totalSuccess == 0);
    ack.data[0] = 0x30;
    r.fake.reportTx(2);
    r.fake.push(ack, 3);
    r.turn(3);
    CHECK(r.done(id).service.protocolAcknowledged && r.health().totalSuccess == 1);
    CHECK(r.owner.stats().txFrames == 3 && r.owner.stats().rxFrames == 3);
    r.turn(4);
    CHECK(r.health().totalSuccess == 1 && r.health().totalFailures == 0);
}

void statisticsResetPreservesUncertaintyHistoryAndLiveWork() {
    Rig r;
    const auto sequence = r.sequence();
    const auto write = r.admit(r.request(sequence, 1, true));
    r.turn(0);
    r.fake.reportTx(0);
    r.turn(1, budget(0, 0, 8, 0));
    r.turn(101, E::ServiceBudget());
    CHECK(r.done(write).service.outcome == C::SdoOutcome::TIMEOUT &&
          r.done(write).service.effectUncertain);
    const Saved<E::Completion> retained(r.done(write));
    const E::Health before = r.health();
    const auto resources = r.owner.resources();
    CHECK(before.totalFailures == 1 && before.consecutiveFailures == 1);
    CHECK(r.owner.clearStats());
    auto h = r.health();
    CHECK(h.totalFailures == 0 && h.totalSuccess == 0 && h.consecutiveFailures == 1);
    CHECK(h.state == E::DriverState::DEGRADED && h.lastErrorKnown &&
          h.lastErrorUs == before.lastErrorUs);
    CHECK(h.lastError.code == before.lastError.code &&
          h.lastError.detail == before.lastError.detail);
    CHECK(r.owner.resources().retained == resources.retained &&
          r.owner.resources().traceUsed == resources.traceUsed && r.services.unresolved(0));
    retained.unchanged(r.done(write));
    CHECK(r.owner.stats().txFrames == 0 && r.owner.stats().timeouts == 0 &&
          r.owner.stats().maxServiceGapUs == 0);
    r.turn(151, E::ServiceBudget());
    CHECK(r.owner.stats().maxServiceGapUs == 50);
    CHECK(!r.owner.service(150, E::ServiceBudget()) && r.owner.stats().maxServiceGapUs == 50);
    E::Diagnostic diagnostic;
    bool historicalTimeout = false;
    while (r.owner.takeDiagnostic(diagnostic))
        historicalTimeout |= diagnostic.kind == E::DiagnosticKind::TERMINAL &&
                             diagnostic.completion.service.effectUncertain;
    CHECK(historicalTimeout);
    const auto second = r.sequence(2);
    const auto read = r.admit(r.request(second));
    r.turn(151);
    CHECK(r.owner.resources().active == 1);
    CHECK(r.owner.clearStats());
    CHECK(r.owner.resources().active == 1 && !r.owner.result(read));
    CHECK(r.owner.reportController(C::ControllerState::BUS_OFF, 152));
    const uint32_t epoch = r.owner.controllerEpoch();
    CHECK(r.owner.clearStats());
    CHECK(r.owner.resources().inhibited && r.owner.controllerEpoch() == epoch);
    CHECK(r.health().state == E::DriverState::OFFLINE);
    retained.unchanged(r.done(write));
}

void exactIngressEnqueueAndAdmissionBoundaries() {
    Rig r;
    M::Frame invalid = frame(0x123, 1);
    invalid.fd = true;
    r.fake.push(invalid, 0);
    r.fake.push(frame(0x123, 1), 1);
    r.fake.push(heartbeat(), 0, true, 2);
    r.fake.push(frame(0x456, 1), 0);
    r.fake.push(heartbeat(), 0, false);
    r.turn(0, budget(8, 0, 0, 0));
    CHECK(r.owner.stats().rxFrames == 5 && r.owner.stats().rxDropped == 3 &&
          r.owner.stats().unmatchedFrames == 1 && r.fake.observed == 5);
    CHECK(r.health().lastSeenKnown && r.health().state == E::DriverState::UNINIT);
    E::RequestId rejected;
    rejected.generation = 99;
    CHECK(r.owner.admit(E::BusRequest(), 0, rejected) == E::BusAdmission::INVALID);
    CHECK(rejected.generation == 99 && r.owner.stats().queueRejected == 1);
    CHECK(r.owner.admitUrgent(E::BusRequest(), 0, rejected) == E::BusAdmission::INVALID);
    CHECK(r.owner.stats().queueRejected == 2 && r.owner.stats().resultsRejected == 0);
    const auto sequence = r.sequence();
    for (unsigned i = 0; i < E::CanBusOwner::REQUESTS; ++i)
        r.admit(r.request(sequence, i + 1));
    CHECK(r.owner.admit(r.request(sequence, 7), 0, rejected) == E::BusAdmission::RESULTS_FULL);
    r.admit(r.request(sequence, 7), true);
    CHECK(r.owner.admitUrgent(r.request(sequence, 8), 0, rejected) ==
          E::BusAdmission::RESULTS_FULL);
    CHECK(r.owner.stats().resultsRejected == 2 && r.owner.stats().queueRejected == 4);
    CHECK(r.owner.receiveOverflow(0, 7));
    CHECK(r.owner.stats().rxDropped == 10 && r.owner.stats().cancelled == 0);
    Rig denied;
    denied.fake.accept = false;
    const auto ds = denied.sequence();
    const auto failed = denied.admit(denied.request(ds));
    denied.turn(0);
    CHECK(denied.done(failed).service.outcome == C::SdoOutcome::TRANSPORT_ERROR);
    CHECK(denied.owner.stats().txRejected == 1 && denied.owner.stats().txFrames == 0);
    denied.fake.reportTx(0, false);
    denied.fake.reportTx(0, false);
    denied.turn(1, budget(0, 0, 8, 0));
    CHECK(denied.owner.stats().txRejected == 1 && denied.health().totalFailures == 1);
    Rig negative;
    const auto ns = negative.sequence();
    const auto nf = negative.admit(negative.request(ns));
    negative.turn(0);
    negative.fake.reportTx(0, false);
    negative.fake.reportTx(0);
    negative.turn(1, budget(0, 0, 8, 0));
    CHECK(negative.done(nf).service.outcome == C::SdoOutcome::TRANSPORT_ERROR);
    CHECK(negative.owner.stats().txRejected == 1 && negative.owner.stats().txFrames == 0);
}

void fullTraceDoesNotDelayOrdinaryOrUrgentWork() {
    Rig r;
    const auto ordinary = r.sequence();
    const auto urgent = r.sequence(2);
    const auto a = r.admit(r.request(ordinary));
    const auto b = r.admit(r.request(urgent), true);
    for (unsigned i = 0; i < 20; ++i)
        r.fake.push(frame(0x400 + i, 1), 0);
    r.turn(0);
    CHECK(r.fake.sentCount == 2);
    CHECK(r.fake.sent[0].id == 0x602); // Reserved urgent work receives the first safe opportunity.
    r.fake.reportTx(0);
    r.fake.reportTx(1);
    r.fake.push(reply(), 1);
    r.fake.push(reply(2), 1);
    for (unsigned i = 1; i <= 3; ++i)
        r.turn(i);
    CHECK(r.done(a).service.protocolAcknowledged && r.done(b).service.protocolAcknowledged);
    CHECK(r.fake.observed == 22 && r.owner.stats().unmatchedFrames == 20);
    CHECK(r.owner.stats().rxFrames == 22 && r.owner.stats().txFrames == 2);
    CHECK(r.owner.stats().traceOverwritten == 20 && r.owner.resources().traceUsed == 8);
    CHECK(r.owner.resources().retained == 2 && r.owner.resources().active == 0 &&
          r.owner.resources().queued == 0 && r.owner.resources().reservations == 2);
    E::Diagnostic diagnostic;
    unsigned count = 0, terminals = 0;
    uint64_t previous = 0;
    while (r.owner.takeDiagnostic(diagnostic)) {
        ++count;
        CHECK(diagnostic.processedUs >= previous);
        previous = diagnostic.processedUs;
        terminals += diagnostic.kind == E::DiagnosticKind::TERMINAL ? 1 : 0;
    }
    CHECK(count == E::CanBusOwner::TRACE_CAPACITY && terminals == 2);
    const Saved<E::Diagnostic> unchanged(diagnostic);
    CHECK(!r.owner.takeDiagnostic(diagnostic));
    unchanged.unchanged(diagnostic);
    CHECK(r.owner.resources().traceUsed == 0 && r.owner.result(a) && r.owner.result(b));
}

void configurationHistoryAndBindingReset() {
    Rig r;
    const auto sequence = r.sequence();
    const auto read = r.admit(r.request(sequence));
    r.turn(0);
    r.fake.reportTx(0);
    r.fake.push(reply(), 1);
    r.turn(1, budget(1, 0, 8, 0));
    CHECK(r.owner.release(read));
    const E::Health before = r.health();
    M::Binding changed = route().binding;
    ++changed.configurationGeneration;
    CHECK(r.owner.advanceConfiguration(sequence, changed, 1));
    E::Health h;
    CHECK(!r.owner.health(route().binding, h));
    CHECK(r.owner.health(changed, h));
    CHECK(h.totalSuccess == before.totalSuccess && h.lastOkKnown && h.lastOkUs == before.lastOkUs);
    const auto failure = r.admit(r.request(sequence, 2));
    r.turn(1);
    r.fake.reportTx(1);
    M::Frame abort = reply();
    abort.data[0] = 0x80;
    abort.data[7] = 0xf1;
    r.fake.push(abort, 2);
    r.turn(2, budget(1, 0, 8, 0));
    CHECK(r.owner.health(changed, h) && h.totalSuccess == 1 && h.totalFailures == 1);
    CHECK(h.lastErrorKnown && h.consecutiveFailures == 1);
    const Saved<E::Completion> failed(r.done(failure));
    E::RecoveryId recovery;
    CHECK(r.owner.recover(2, 10, recovery));
    r.fake.progress = E::RecoveryProgress::RETIRED;
    r.turn(3, budget(0, 0, 0, 0, 1));
    CHECK(r.owner.releaseRecovery(recovery));
    const E::Stats interfaceBefore = r.owner.stats();
    r.retireSyntheticServerAndReconstruct(13, 4);
    M::Binding rebound = route().binding;
    rebound.bindingGeneration = 4;
    CHECK(!r.owner.health(changed, h) && r.owner.health(rebound, h));
    r.turn(3, E::ServiceBudget());
    CHECK(r.owner.health(rebound, h));
    CHECK(h.state == E::DriverState::UNINIT && !h.lastOkKnown && !h.lastErrorKnown &&
          !h.lastSeenKnown);
    CHECK(h.totalSuccess == 0 && h.totalFailures == 0 && h.consecutiveFailures == 0);
    CHECK(h.lastError.code == E::Health().lastError.code);
    CHECK(r.owner.stats().txFrames == interfaceBefore.txFrames &&
          r.owner.stats().rxFrames == interfaceBefore.rxFrames &&
          r.owner.stats().recoveries == interfaceBefore.recoveries);
    failed.unchanged(r.done(failure));
    // A lifecycle epoch change on the same binding withdraws current freshness
    // while preserving the binding's historical observation time.
    C::SdoClient services;
    Fake fake;
    E::CanBusOwner owner(services, fake.transport(), 74);
    C::NetworkRoute network;
    network.binding = route().binding;
    network.generation = 1;
    network.heartbeatId = 0x701;
    network.emcyId = 0x81;
    CHECK(services.configureNetwork(0, network, C::NetworkObserver(), 0));
    fake.push(heartbeat(), 0);
    CHECK(owner.service(0, budget(1, 0, 0, 0)));
    CHECK(owner.health(network.binding, h) && h.lastSeenKnown);
    CHECK(services.invalidateNetwork(0, C::NetworkInvalidation::BINDING, 1));
    CHECK(owner.service(1, E::ServiceBudget()));
    CHECK(owner.health(network.binding, h) && h.state == E::DriverState::UNINIT);
    CHECK(
        h.lastSeenKnown); // Same logical binding preserves historical times; epoch withdraws freshness.
}

void requalifiedPdoDoesNotPromoteRetainedSample() {
    Rig r;
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
    CHECK(r.services.pdo(0)->samplePresent && r.health().state == E::DriverState::READY);
    const C::PdoObservation old = r.services.pdo(0)->observation;
    CHECK(r.services.invalidatePdo(0, false, 2));
    r.turn(2, E::ServiceBudget());
    CHECK(r.health().state == E::DriverState::UNINIT);
    proof.evidenceId = 102;
    proof.establishedUs = 3;
    CHECK(r.services.qualifyPdo(0, proof, 3));
    r.turn(3, E::ServiceBudget());
    CHECK(r.services.pdo(0)->samplePresent && r.services.pdo(0)->qualified);
    CHECK(r.services.pdo(0)->observation.evidenceId == old.evidenceId);
    CHECK(r.health().state == E::DriverState::UNINIT && r.health().lastSeenUs == 1);
    r.fake.push(frame(0x281), 2);
    r.turn(3, budget(1, 0, 0, 0));
    CHECK(r.health().state == E::DriverState::UNINIT);
    r.fake.push(frame(0x281), 4);
    r.turn(4, budget(1, 0, 0, 0));
    CHECK(r.health().state == E::DriverState::READY && r.health().lastSeenUs == 4);
    CHECK(r.services.pdo(0)->observation.evidenceId == 102);
    CHECK(r.health().totalSuccess == 0 && r.health().totalFailures == 0);

    // A checked current-qualified sample can arrive too late for READY. Its
    // delivery remains historical evidence, independently of capture freshness.
    CHECK(r.services.setServiceAge(0, 5, 4));
    r.fake.push(frame(0x281), 5);
    r.turn(11, budget(1, 0, 0, 0));
    CHECK(r.services.pdo(0)->observation.time.receivedUs == 5 &&
          r.services.pdo(0)->observation.time.processedUs == 11);
    CHECK(r.health().lastSeenKnown && r.health().lastSeenUs == 11);
    CHECK(r.health().state == E::DriverState::UNINIT && r.health().totalSuccess == 0 &&
          r.health().totalFailures == 0);
    const uint32_t unmatched = r.owner.stats().unmatchedFrames;
    r.fake.push(frame(0x281, 7), 12); // Wrong mapped payload length.
    r.turn(12, budget(1, 0, 0, 0));
    r.fake.push(frame(0x281), 13, false); // Capture time cannot qualify this PDO.
    r.turn(13, budget(1, 0, 0, 0));
    CHECK(r.owner.stats().unmatchedFrames == unmatched + 2);
    CHECK(r.health().lastSeenKnown && r.health().lastSeenUs == 11 &&
          r.health().state == E::DriverState::UNINIT);
    r.fake.push(frame(0x281), 14);
    r.turn(14, budget(1, 0, 0, 0));
    CHECK(r.health().lastSeenUs == 14 && r.health().state == E::DriverState::READY);
    CHECK(r.health().totalSuccess == 0 && r.health().totalFailures == 0);
}

void recoveryTimeoutAndCounterSaturation() {
    Rig r;
    E::RecoveryId recovery;
    CHECK(r.owner.recover(0, 5, recovery));
    r.turn(5, E::ServiceBudget());
    CHECK(r.owner.recoveryResult(recovery)->outcome == E::RecoveryOutcome::TIMEOUT);
    CHECK(r.owner.stats().timeouts == 1 && r.owner.stats().recoveries == 0);
    CHECK(r.fake.pollCalls == 0 && r.health().totalFailures == 0);
    CHECK(r.owner.receiveOverflow(5, UINT32_MAX));
    CHECK(r.owner.stats().rxDropped == UINT32_MAX);
    CHECK(r.owner.receiveOverflow(5, UINT32_MAX) && r.owner.stats().rxDropped == UINT32_MAX);
    CHECK(r.owner.clearStats() && r.owner.stats().rxDropped == 0);
    CHECK(r.owner.resources().inhibited && r.owner.recoveryResult(recovery));
    uint32_t count = UINT32_MAX - 1;
    E::detail::add(count);
    CHECK(count == UINT32_MAX);
    E::detail::add(count);
    CHECK(count == UINT32_MAX);
    count = UINT32_MAX - 3;
    E::detail::add(count, 4);
    CHECK(count == UINT32_MAX);
    count = 0;
    E::detail::add(count, UINT32_MAX);
    CHECK(count == UINT32_MAX);
    E::detail::add(count, UINT32_MAX);
    CHECK(count == UINT32_MAX);
    uint8_t streak = UINT8_MAX - 1;
    E::detail::add(streak);
    CHECK(streak == UINT8_MAX);
    Rig gaps;
    gaps.turn(0, E::ServiceBudget());
    gaps.turn(UINT64_MAX, E::ServiceBudget());
    CHECK(gaps.owner.stats().maxServiceGapUs == UINT64_MAX);
    CHECK(gaps.owner.clearStats());
    gaps.turn(UINT64_MAX, E::ServiceBudget());
    CHECK(gaps.owner.stats().maxServiceGapUs == 0);
    E::detail::add(streak);
    CHECK(streak == UINT8_MAX);
}

void discoveredUnsupportedServiceIsHealthNeutral() {
    Rig r;
    const auto sequence = r.sequence();
    auto work = r.request(sequence);
    work.transfer.allowSegmentedUpload = false;
    const auto id = r.admit(work);
    r.turn(0);
    r.fake.reportTx(0);
    M::Frame initiation = reply();
    initiation.data[0] = 0x41;
    initiation.data[4] = 2;
    initiation.data[5] = initiation.data[6] = initiation.data[7] = 0;
    r.fake.push(initiation, 1);
    r.turn(1, budget(1, 0, 1, 0));
    CHECK(r.done(id).service.outcome == C::SdoOutcome::UNSUPPORTED);
    CHECK(r.done(id).service.transmissionPossible && r.done(id).service.clientAbortQueued);
    const auto h = r.health();
    CHECK(h.totalSuccess == 0 && h.totalFailures == 0 && h.consecutiveFailures == 0);
    CHECK(!h.lastOkKnown && !h.lastErrorKnown && h.lastSeenKnown && h.lastSeenUs == 1);
    CHECK(h.state == E::DriverState::READY && r.services.unresolved(0));
    CHECK(r.owner.stats().txFrames == 1 && r.owner.stats().timeouts == 0 &&
          r.owner.stats().cancelled == 0);
}

void malformedReplyAndCleanupTransmissionCountSeparately() {
    Rig r;
    const auto sequence = r.sequence();
    const auto id = r.admit(r.request(sequence));
    r.turn(0);
    r.fake.reportTx(0);
    M::Frame malformed = reply();
    malformed.dlc = malformed.payloadLength = 7;
    r.fake.push(malformed, 1);
    r.turn(1, budget(1, 1, 1, 0));
    CHECK(r.done(id).service.outcome == C::SdoOutcome::CLIENT_ABORT);
    CHECK(r.fake.sentCount == 2 && r.fake.sent[1].data[0] == 0x80);
    const Saved<E::Completion> terminal(r.done(id));
    const auto h = r.health();
    CHECK(h.totalSuccess == 0 && h.totalFailures == 1 && h.consecutiveFailures == 1);
    CHECK(h.lastErrorKnown && h.lastErrorUs == 1 && h.state == E::DriverState::DEGRADED);
    CHECK(r.owner.stats().txFrames == 1 && r.owner.stats().txRejected == 0);
    r.fake.reportTx(1);
    r.fake.reportTx(1);
    r.turn(2, budget(0, 0, 8, 0));
    CHECK(r.owner.stats().txFrames == 2 && r.owner.stats().txRejected == 0);
    terminal.unchanged(r.done(id));
    const auto after = r.health();
    CHECK(after.totalSuccess == 0 && after.totalFailures == 1 && after.consecutiveFailures == 1);
    CHECK(after.lastErrorKnown && after.lastErrorUs == h.lastErrorUs &&
          after.lastError.code == h.lastError.code && after.lastError.detail == h.lastError.detail);
    CHECK(after.state == E::DriverState::DEGRADED && !after.lastOkKnown);
}

void deadlineWinsCancellationAtSameCutoff() {
    for (unsigned transmitted = 0; transmitted < 2; ++transmitted) {
        Rig r;
        E::SequenceId sequence;
        CHECK(r.owner.beginSequence(route().binding, 1, 0, sequence));
        CHECK(r.owner.beginOperation(sequence, 1, 5, 0));
        const auto id = r.admit(r.request(sequence));
        if (transmitted)
            r.turn(0);
        CHECK(r.owner.cancel(id, 5) == E::Cancel::ALREADY_TERMINAL);
        CHECK(r.done(id).service.outcome == C::SdoOutcome::TIMEOUT);
        CHECK(r.owner.stats().timeouts == 1 && r.owner.stats().cancelled == 0);
        CHECK(r.health().totalSuccess == 0 && r.health().totalFailures == transmitted &&
              r.health().consecutiveFailures == transmitted);
        const Saved<E::Completion> terminal(r.done(id));
        CHECK(r.owner.cancel(id, 5) == E::Cancel::ALREADY_TERMINAL);
        CHECK(r.owner.stats().timeouts == 1 && r.owner.stats().cancelled == 0);
        terminal.unchanged(r.done(id));
    }
}

void exactRetirementAlsoRetiresFinalReports() {
    for (unsigned accepted = 0; accepted < 2; ++accepted) {
        Rig r;
        r.fake.retireAccepted = accepted != 0;
        const auto first = r.sequence();
        const auto other = r.sequence(2);
        const auto cancelled = r.admit(r.request(first));
        const auto unaffected = r.admit(r.request(other));
        r.turn(0);
        CHECK(r.fake.sentCount == 2 && r.owner.stats().txFrames == 0);
        CHECK(r.fake.ids[0].request.channel == 0 && r.fake.ids[1].request.channel == 1);
        r.fake.reportTx(0);
        r.fake.reportTx(1);
        CHECK(r.owner.cancel(cancelled, 1) == E::Cancel::CANCELLED);
        const Saved<E::Completion> terminal(r.done(cancelled));
        CHECK(r.fake.retireCalls == 1 && r.fake.reportWrite - r.fake.reportRead == 2 - accepted);
        CHECK(r.fake.reportsRetired[0] == bool(accepted) && !r.fake.reportsRetired[1]);
        if (accepted)
            CHECK(Fake::same(r.fake.reports[r.fake.reportRead].id, r.fake.ids[1]));
        // Refusing report retirement leaves the existing matching final report
        // deliverable. A positive retirement suppresses it even if TX happened
        // earlier: the owner counts delivered confirmation rather than guessing.
        r.turn(2, budget(0, 0, 8, 0));
        CHECK(r.owner.stats().txFrames == 2 - accepted && r.owner.stats().txRejected == 0);
        CHECK(r.fake.reportRead == r.fake.reportWrite && r.fake.sentCount == 2);
        CHECK(r.owner.stats().cancelled == 1 && r.health().totalFailures == 0);
        terminal.unchanged(r.done(cancelled));
        r.fake.push(reply(2), 3);
        r.turn(3, budget(1, 0, 8, 0));
        CHECK(r.done(unaffected).service.protocolAcknowledged && r.health(2).totalSuccess == 1);
        CHECK(r.owner.stats().txFrames == 2 - accepted && r.owner.stats().txRejected == 0);
        terminal.unchanged(r.done(cancelled));
    }
}
} // namespace
int main() {
    cachedStateAndKnownZero();
    abortFailureThenIndependentRead();
    freshHeartbeatPreservesServiceFailure();
    heartbeatPolicyAndControllerRecovery();
    localCancellationAndQueueExpiryAreHealthNeutral();
    segmentedServiceCountsOnce();
    statisticsResetPreservesUncertaintyHistoryAndLiveWork();
    exactIngressEnqueueAndAdmissionBoundaries();
    fullTraceDoesNotDelayOrdinaryOrUrgentWork();
    configurationHistoryAndBindingReset();
    requalifiedPdoDoesNotPromoteRetainedSample();
    recoveryTimeoutAndCounterSaturation();
    discoveredUnsupportedServiceIsHealthNeutral();
    malformedReplyAndCleanupTransmissionCountSeparately();
    deadlineWinsCancellationAtSameCutoff();
    exactRetirementAlsoRetiresFinalReports();
    std::printf("CAN owner health: %u checks passed\n", checks);
    std::printf(
        "host bytes: CanBusOwner=%zu Health=%zu Stats=%zu Diagnostic=%zu OwnerResources=%zu\n",
        sizeof(E::CanBusOwner), sizeof(E::Health), sizeof(E::Stats), sizeof(E::Diagnostic),
        sizeof(E::OwnerResources));
}
