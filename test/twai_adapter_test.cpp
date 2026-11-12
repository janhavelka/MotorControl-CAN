// SPDX-License-Identifier: MIT
// Synthetic controller events exercise the real service store and owner.
// Fixture pins, rates, times and routes are not hardware configuration evidence.
#include "ProbeApp.h"
#include "TwaiAdapter.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>

namespace M = MotorControlCAN;
namespace C = M::CANOPEN;
namespace E = MotorControlCANExample;
namespace O = MotorControlCANOwner;
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
// The rig pairs node 1 on slot 0 with node 2 on slot 1. Scenarios that need node 2 run when the
// capacity set has two channels; a one-channel set configures node 1 alone and runs the others.
// Channels beyond two stay unconfigured and must not change any outcome.
constexpr bool PAIRED_CHANNELS = C::SdoClient::CHANNELS >= 2;
constexpr unsigned FIXTURE_NODES = PAIRED_CHANNELS ? 2 : 1;

M::Frame frame(uint32_t id, uint8_t count = 8) {
    M::Frame value;
    value.id = id;
    value.dlc = value.payloadLength = count;
    return value;
}
E::TwaiConfig config() {
    E::TwaiConfig value;
    value.txPin = 4;
    value.rxPin = 5;
    value.bitrate = 250000;
    value.sdkTxQueueDepth = 1;
    value.rxQueueDepth = 16;
    return value;
}
struct FakeDriver {
    enum { RX_CAPACITY = 16, HISTORY = 16, ATTEMPTS = 64 };
    E::TwaiEvents pending;
    O::ReceivedFrame rx[RX_CAPACITY];
    M::Frame sent[HISTORY];
    uint32_t serials[HISTORY] = {};
    M::Frame attempted[ATTEMPTS]; // Every submit call, including those with no SDK contact.
    uint32_t attemptSerials[ATTEMPTS] = {};
    unsigned attempts = 0, deferrals = 0;
    unsigned starts = 0, stops = 0, submits = 0, eventCalls = 0, receiveCalls = 0;
    unsigned rxRead = 0, rxWrite = 0;
    uint32_t epoch = 0, activeSerial = 0;
    E::TwaiSubmit submitResult = E::TwaiSubmit::SENT;
    bool startOk = true, stopOk = true, immediate = false;
    bool live = false, active = false;
    static bool start(void* p, const E::TwaiConfig& cfg, uint32_t epoch) {
        FakeDriver& f = *static_cast<FakeDriver*>(p);
        ++f.starts;
        CHECK(cfg.sdkTxQueueDepth == 1 && epoch != 0);
        f.epoch = epoch;
        f.live = f.startOk;
        return f.startOk;
    }
    static bool stop(void* p) {
        FakeDriver& f = *static_cast<FakeDriver*>(p);
        ++f.stops;
        if (!f.stopOk)
            return false;
        f.live = f.active = false;
        f.activeSerial = 0;
        f.pending = E::TwaiEvents();
        f.rxRead = f.rxWrite = 0;
        return true;
    }
    static E::TwaiSubmit submit(void* p, const M::Frame& value, uint32_t serial) {
        FakeDriver& f = *static_cast<FakeDriver*>(p);
        CHECK(f.live && !f.active && f.submits < HISTORY && f.attempts < ATTEMPTS);
        f.attempted[f.attempts] = value;
        f.attemptSerials[f.attempts++] = serial;
        // Same guard order as IdfTwaiDriver::submit. An unread controller fault proves
        // no acceptance; captured RX proves no SDK contact and the frame stays queued.
        if (f.pending.busOff || f.pending.fault || f.pending.rxDropped)
            return E::TwaiSubmit::FAILED_UNSENT;
        if (f.rxRead != f.rxWrite) {
            ++f.deferrals;
            return E::TwaiSubmit::DEFERRED;
        }
        // Rejection retains neither the call-only input nor a future callback.
        if (f.submitResult != E::TwaiSubmit::SENT)
            return f.submitResult;
        f.sent[f.submits] = value;
        f.serials[f.submits++] = serial;
        f.active = true;
        f.activeSerial = serial;
        if (f.immediate)
            f.complete(true);
        return E::TwaiSubmit::SENT;
    }
    static E::TwaiEvents events(void* p) {
        FakeDriver& f = *static_cast<FakeDriver*>(p);
        ++f.eventCalls;
        E::TwaiEvents copy = f.pending;
        copy.rxPending = f.rxRead != f.rxWrite;
        f.pending = E::TwaiEvents();
        return copy;
    }
    static bool receive(void* p, O::ReceivedFrame& output, uint64_t cutoffUs) {
        FakeDriver& f = *static_cast<FakeDriver*>(p);
        ++f.receiveCalls;
        if (f.rxRead == f.rxWrite || f.pending.busOff || f.pending.fault || f.pending.rxDropped)
            return false;
        // Same head rule as IdfTwaiDriver::receive: a newer ingress waits for a later turn.
        if (f.rx[f.rxRead % RX_CAPACITY].receivedLatestUs > cutoffUs)
            return false;
        output = f.rx[f.rxRead++ % RX_CAPACITY];
        return true;
    }
    E::TwaiDriver driver() {
        E::TwaiDriver value;
        value.context = this;
        value.start = start;
        value.stop = stop;
        value.submit = submit;
        value.events = events;
        value.receive = receive;
        return value;
    }
    void complete(bool transmitted, uint32_t serial = 0) {
        CHECK(!pending.txDone);
        pending.txDone = true;
        pending.transmitted = transmitted;
        pending.serial = serial ? serial : activeSerial;
        active = false;
    }
    void push(const M::Frame& value, bool captureKnown = false, uint64_t captured = 0) {
        if (rxWrite - rxRead == RX_CAPACITY) {
            ++pending.rxDropped;
            pending.fault = true;
            return;
        }
        O::ReceivedFrame& entry = rx[rxWrite++ % RX_CAPACITY];
        entry.frame = value;
        entry.controllerEpoch = epoch;
        entry.receivedTimeKnown = captureKnown;
        entry.receivedUs = captured;
        entry.receiveBoundsKnown = false;
        entry.receivedEarliestUs = entry.receivedLatestUs = 0;
    }
    void pushBounded(const M::Frame& value, uint64_t earliest, uint64_t latest) {
        CHECK(rxWrite - rxRead < RX_CAPACITY);
        O::ReceivedFrame& entry = rx[rxWrite++ % RX_CAPACITY];
        entry.frame = value;
        entry.controllerEpoch = epoch;
        entry.receivedTimeKnown = false;
        entry.receivedUs = 0;
        entry.receiveBoundsKnown = true;
        entry.receivedEarliestUs = earliest;
        entry.receivedLatestUs = latest;
    }
};
// Copies every final report the real owner consumes, so timelines can assert the
// exact report and not only its effect. Everything else forwards to the adapter.
struct ReportTap {
    enum { CAPACITY = 16 };
    O::Transport inner;
    O::TxReport log[CAPACITY];
    unsigned count = 0;
    static ReportTap& of(void* p) { return *static_cast<ReportTap*>(p); }
    O::Transport wrap(const O::Transport& adapter) {
        inner = adapter;
        O::Transport value = adapter;
        value.context = this;
        value.receive = [](void* p, O::ReceivedFrame& frame) {
            return of(p).inner.receive(of(p).inner.context, frame);
        };
        value.transmit = [](void* p, const M::Frame& frame, O::TransportTxId id, uint32_t epoch) {
            return of(p).inner.transmit(of(p).inner.context, frame, id, epoch);
        };
        value.takeTxReport = [](void* p, O::TxReport& report) {
            ReportTap& tap = of(p);
            if (!tap.inner.takeTxReport(tap.inner.context, report))
                return false;
            CHECK(tap.count < CAPACITY);
            tap.log[tap.count++] = report;
            return true;
        };
        value.retire = [](void* p, O::TransportTxId id, uint32_t epoch) {
            return of(p).inner.retire(of(p).inner.context, id, epoch);
        };
        value.beginRecovery = [](void* p, uint32_t epoch) {
            return of(p).inner.beginRecovery(of(p).inner.context, epoch);
        };
        value.pollRecovery = [](void* p, uint32_t epoch) {
            return of(p).inner.pollRecovery(of(p).inner.context, epoch);
        };
        return value;
    }
};
struct Rig {
    C::SdoClient services;
    FakeDriver sdk;
    E::TwaiAdapter adapter;
    ReportTap tap;
    O::CanBusOwner owner;
    uint64_t now = 0;
    O::SequenceId lastSequence; // Most recent reservation from a request helper.
    explicit Rig(bool start = true)
        : adapter(services, sdk.driver()), owner(services, tap.wrap(adapter.transport()), 127) {
        for (uint8_t i = 0; i < FIXTURE_NODES; ++i) {
            CHECK(services.configure(i, route(i + 1), true, true));
            C::NetworkRoute network;
            network.binding = route(i + 1).binding;
            network.generation = 11;
            network.heartbeatId = 0x701 + i;
            network.emcyId = 0x81 + i;
            CHECK(services.configureNetwork(i, network, C::NetworkObserver(), 0));
        }
        if (start)
            CHECK(adapter.start(config()));
    }
    void turn(uint64_t time, O::ServiceBudget b = budget()) {
        now = time;
        adapter.pollEvents(owner, now);
        CHECK(owner.service(now, b));
        adapter.promote(owner, now);
    }
    // Half turn: a test can act between the owner and the SDK boundary.
    void stage(uint64_t time, O::ServiceBudget b = budget()) {
        now = time;
        adapter.pollEvents(owner, now);
        CHECK(owner.service(now, b));
    }
    // The old standalone adapter.service.
    void adapterOnly(uint64_t time) {
        adapter.pollEvents(owner, time);
        adapter.promote(owner, time);
    }
    O::RequestId request(unsigned node = 1, bool download = false, uint64_t deadline = 1000,
                         uint32_t queueUs = 100, bool requirePreOperational = false) {
        O::SequenceId sequence;
        CHECK(owner.beginSequence(route(node).binding, 1, now, sequence));
        CHECK(owner.beginOperation(sequence, 1, deadline, now));
        lastSequence = sequence;
        O::BusRequest work;
        work.sequence = sequence;
        work.transfer.channel = route(node);
        work.transfer.operation = work.transfer.request = work.transfer.step = 1;
        work.transfer.index = download ? 0x6040 : 0x6041;
        work.transfer.size = 2;
        work.transfer.direction = download ? C::SdoDirection::DOWNLOAD : C::SdoDirection::UPLOAD;
        work.transfer.readEffect = C::SdoReadEffect::REVIEWED_NON_CHANGING;
        work.transfer.requirePreOperational = requirePreOperational;
        work.transfer.queueTimeoutUs = queueUs;
        work.transfer.responseTimeoutUs = 100;
        work.transfer.deadlineUs = deadline;
        work.transfer.data[0] = 0x34;
        work.transfer.data[1] = 0x12;
        O::RequestId id;
        CHECK(owner.admit(work, now, id) == O::BusAdmission::ACCEPTED);
        return id;
    }
    // The 000B stop bytes as the next step of an existing reservation.
    O::BusRequest stopWork(O::SequenceId sequence, unsigned node, uint32_t step, uint64_t deadline,
                           uint32_t queueUs) {
        O::BusRequest work;
        work.sequence = sequence;
        work.transfer.channel = route(node);
        work.transfer.operation = 1;
        work.transfer.request = work.transfer.step = step;
        work.transfer.index = 0x6040;
        work.transfer.size = 2;
        work.transfer.direction = C::SdoDirection::DOWNLOAD;
        work.transfer.queueTimeoutUs = queueUs;
        work.transfer.responseTimeoutUs = 100;
        work.transfer.deadlineUs = deadline;
        work.transfer.data[0] = 0x0b;
        return work;
    }
    O::RequestId urgentStop(unsigned node, uint64_t deadline, uint32_t queueUs) {
        O::SequenceId sequence;
        CHECK(owner.beginSequence(route(node).binding, 1, now, sequence));
        CHECK(owner.beginOperation(sequence, 1, deadline, now));
        lastSequence = sequence;
        O::RequestId id;
        CHECK(owner.admitUrgent(stopWork(sequence, node, 1, deadline, queueUs), now, id) ==
              O::BusAdmission::ACCEPTED);
        return id;
    }
    // Eight bytes 1..8: one initiate frame, then segments. Needs SDO segmented transfer.
    O::RequestId requestLarge(unsigned node, uint64_t deadline, uint32_t queueUs) {
        O::SequenceId sequence;
        CHECK(owner.beginSequence(route(node).binding, 1, now, sequence));
        CHECK(owner.beginOperation(sequence, 1, deadline, now));
        lastSequence = sequence;
        O::BusRequest work = stopWork(sequence, node, 1, deadline, queueUs);
        work.transfer.size = 8;
        for (uint8_t i = 0; i < 8; ++i)
            work.transfer.data[i] = uint8_t(i + 1);
        O::RequestId id;
        CHECK(owner.admit(work, now, id) == O::BusAdmission::ACCEPTED);
        return id;
    }
    const O::Completion& done(O::RequestId id) {
        const O::Completion* result = owner.result(id);
        CHECK(result);
        return *result;
    }
};
O::TransportTxId arbitraryId(C::SdoClient& services, uint32_t number) {
    C::SdoTxId id;
    id.request.owner = &services;
    id.request.generation = number;
    id.frame = 1;
    return O::TransportTxId::fromSdo(id);
}

void checkNeverSent(const O::Completion& c);

void startupValidationAndOwnedCapacity() {
    Rig r(false);
    CHECK(r.sdk.starts == 0 && r.sdk.submits == 0);
    E::TwaiConfig invalid = config();
    invalid.rxPin = invalid.txPin;
    CHECK(!r.adapter.start(invalid));
    CHECK(r.sdk.starts == 0);
    CHECK(!r.adapter.start(config(), 0));
    CHECK(r.adapter.start(config()));
    CHECK(!r.adapter.start(config()));
    r.turn(0);
    CHECK(r.sdk.submits == 0 && !r.adapter.diagnostics().inhibited);
    O::Transport t = r.adapter.transport();
    M::Frame original = frame(0x123, 2);
    original.data[0] = 0x12;
    CHECK(t.transmit(t.context, original, arbitraryId(r.services, 1), 1));
    original.data[0] = 0xff;
    CHECK(t.transmit(t.context, original, arbitraryId(r.services, 2), 1));
    // Capacity is one slot per SDO channel plus the one NMT frame, the most the
    // owner can have outstanding, so ordinary work is never refused for capacity.
    CHECK(E::TwaiAdapter::TX_CAPACITY == C::SdoClient::CHANNELS + 1);
    for (uint32_t n = 3; n <= E::TwaiAdapter::TX_CAPACITY; ++n)
        CHECK(t.transmit(t.context, original, arbitraryId(r.services, n), 1));
    const uint32_t beyond = E::TwaiAdapter::TX_CAPACITY + 1;
    CHECK(!t.transmit(t.context, original, arbitraryId(r.services, beyond), 1));
    // Unqualified direct IDs reach negative reports, never the SDK. Reports
    // retain the same capacity slots until explicitly consumed.
    CHECK(r.owner.service(1, O::ServiceBudget()));
    r.adapterOnly(1);
    CHECK(!t.transmit(t.context, original, arbitraryId(r.services, beyond), 1));
    O::TxReport report;
    CHECK(t.takeTxReport(t.context, report) && !report.transmitted && report.provedUnsent);
    CHECK(report.id.sdo.request.generation == 1);
    CHECK(t.transmit(t.context, original, arbitraryId(r.services, beyond), 1));
    CHECK(r.sdk.submits == 0);
    CHECK(r.adapter.shutdown());
}

void delayedImmediateAndFailedCompletion() {
    for (unsigned immediate = 0; immediate < 2; ++immediate) {
        Rig r;
        r.sdk.immediate = immediate != 0;
        const O::RequestId id = r.request(1, true);
        r.turn(0);
        // Enqueue and submission alone never increment txFrames.
        CHECK(r.sdk.submits == 1 && !r.owner.result(id) && r.owner.stats().txFrames == 0);
        r.turn(1);
        // pollEvents precedes the owner, so an immediate completion is consumed here.
        CHECK(r.sdk.submits == 1 && r.owner.stats().txFrames == immediate);
        CHECK(r.sdk.sent[0].data[4] == 0x34 && r.sdk.sent[0].data[5] == 0x12);
        if (!immediate) {
            r.turn(2);
            CHECK(r.owner.stats().txFrames == 0 && !r.owner.result(id));
            r.sdk.complete(true);
        }
        r.turn(3);
        CHECK(r.owner.stats().txFrames == 1 && !r.owner.result(id));
        r.turn(4);
        CHECK(r.owner.stats().txFrames == 1 && !r.owner.result(id));
        M::Frame reply = frame(0x581);
        reply.data[0] = 0x60;
        reply.data[1] = 0x40;
        reply.data[2] = 0x60;
        r.sdk.push(reply);
        r.turn(5);
        CHECK(r.done(id).service.outcome == C::SdoOutcome::SUCCESS);
        CHECK(r.done(id).service.protocolAcknowledged && r.sdk.submits == 1);
    }
    Rig failed;
    const auto write = failed.request(1, true);
    failed.turn(0);
    failed.turn(1);
    // A completion after this turn's poll still waits one owner turn for its report.
    failed.stage(2);
    failed.sdk.complete(false);
    failed.adapter.promote(failed.owner, 2);
    CHECK(!failed.owner.result(write));
    failed.turn(3);
    CHECK(failed.done(write).service.outcome == C::SdoOutcome::TRANSPORT_ERROR);
    CHECK(failed.done(write).service.effectUncertain &&
          !failed.done(write).service.protocolAcknowledged);
    CHECK(failed.owner.stats().txFrames == 0 && failed.owner.stats().txRejected == 1);
    failed.turn(4);
    CHECK(failed.sdk.submits == 1);
}

void copiedPayloadAndWireFormatsReachTheDriver() {
    for (unsigned shape = 0; shape < 3; ++shape) {
        Rig r;
        C::SdoTransfer work;
        work.channel = route();
        work.operation = work.request = work.step = 1;
        work.index = 0x6040;
        work.direction = C::SdoDirection::DOWNLOAD;
        work.size = 2;
        work.data[0] = 0x34;
        work.data[1] = 0x12;
        work.deadlineUs = 100;
        work.queueTimeoutUs = work.responseTimeoutUs = 50;
        C::SdoRequestId request;
        CHECK(r.services.admit(0, work, 0, request));
        M::Frame wire;
        C::SdoTxId id;
        CHECK(r.services.nextTx(0, 0, wire, id));
        if (shape) {
            wire.extended = true;
            wire.id = 0x1fffffff;
        }
        if (shape == 2) {
            wire.rtr = true;
            wire.payloadLength = 0;
        }
        const M::Frame expected = wire;
        O::Transport transport = r.adapter.transport();
        CHECK(transport.transmit(transport.context, wire, O::TransportTxId::fromSdo(id), 1));
        wire.id = 0;
        std::memset(wire.data, 0xff, sizeof wire.data);
        std::memset(work.data, 0xee, sizeof work.data);
        CHECK(r.sdk.submits == 0);
        CHECK(r.owner.service(1, O::ServiceBudget()));
        r.adapterOnly(1);
        CHECK(r.sdk.submits == 1 && r.sdk.sent[0].id == expected.id);
        CHECK(r.sdk.sent[0].extended == expected.extended && r.sdk.sent[0].rtr == expected.rtr);
        CHECK(r.sdk.sent[0].dlc == 8 && r.sdk.sent[0].payloadLength == expected.payloadLength);
        if (!expected.rtr)
            CHECK(!std::memcmp(r.sdk.sent[0].data, expected.data, 8));
    }
}

void rejectionUnavailableAndIdentityFault() {
    // A false return that cannot prove nontransmission: effects stay possible and
    // the route quarantines. This is the old false-submit contract, unchanged.
    Rig rejected;
    rejected.sdk.submitResult = E::TwaiSubmit::FAILED_UNCERTAIN;
    const auto id = rejected.request(1, true);
    rejected.turn(0);
    rejected.turn(1);
    CHECK(rejected.sdk.submits == 0 && !rejected.sdk.active && rejected.sdk.attempts == 1);
    CHECK(rejected.done(id).service.outcome == C::SdoOutcome::TRANSPORT_ERROR);
    CHECK(rejected.done(id).service.transmissionPossible);
    CHECK(rejected.done(id).service.effectUncertain);
    CHECK(rejected.tap.count == 1 && !rejected.tap.log[0].transmitted);
    CHECK(!rejected.tap.log[0].provedUnsent);
    CHECK(rejected.services.unresolved(0));
    rejected.turn(2);
    CHECK(rejected.sdk.submits == 0 && rejected.sdk.attempts == 1);

    // A rejection that proves no acceptance: same outcome, but no possible effect
    // and no quarantine. Nothing is resent.
    Rig unsent;
    unsent.sdk.submitResult = E::TwaiSubmit::FAILED_UNSENT;
    const auto never = unsent.request(1, true);
    unsent.turn(0);
    unsent.turn(1);
    CHECK(unsent.sdk.submits == 0 && !unsent.sdk.active && unsent.sdk.attempts == 1);
    CHECK(unsent.done(never).service.outcome == C::SdoOutcome::TRANSPORT_ERROR);
    CHECK(!unsent.done(never).service.transmissionPossible);
    CHECK(!unsent.done(never).service.effectPossible);
    CHECK(!unsent.done(never).service.effectUncertain);
    CHECK(unsent.tap.count == 1 && !unsent.tap.log[0].transmitted);
    CHECK(unsent.tap.log[0].provedUnsent);
    CHECK(!unsent.services.unresolved(0) && !unsent.adapter.diagnostics().inhibited);
    unsent.turn(2);
    CHECK(unsent.sdk.submits == 0 && unsent.sdk.attempts == 1);

    Rig silent;
    const auto uncertain = silent.request(1, true, 10);
    silent.turn(0);
    silent.turn(1);
    silent.turn(10);
    CHECK(silent.done(uncertain).service.outcome == C::SdoOutcome::TIMEOUT);
    CHECK(silent.done(uncertain).service.effectUncertain && silent.sdk.submits == 1);
    silent.turn(20);
    CHECK(silent.sdk.submits == 1);

    Rig corrupt;
    const auto active = corrupt.request();
    corrupt.turn(0);
    corrupt.turn(1);
    corrupt.sdk.complete(true, corrupt.sdk.activeSerial + 1);
    corrupt.turn(2);
    CHECK(corrupt.adapter.diagnostics().inhibited);
    CHECK(corrupt.owner.resources().inhibited && corrupt.owner.stats().txFrames == 0);
    CHECK(corrupt.owner.result(active));
    corrupt.turn(3);
    CHECK(corrupt.sdk.submits == 1);
}

void activeAndQueuedRetirement() {
    Rig queued;
    const auto before = queued.request(1, true);
    // Pause between the production owner and adapter calls to cancel a queued
    // handoff before the controller is allowed to receive it.
    CHECK(queued.owner.service(0, budget()));
    CHECK(queued.owner.cancel(before, 0) == O::Cancel::CANCELLED);
    queued.turn(1);
    queued.turn(2);
    CHECK(queued.sdk.submits == 0);
    CHECK(queued.done(before).service.outcome == C::SdoOutcome::CANCELLED);

    Rig active;
    const auto sent = active.request(1, true);
    active.turn(0);
    active.turn(1);
    CHECK(active.owner.cancel(sent, 1) == O::Cancel::CANCELLED);
    CHECK(active.sdk.active && active.sdk.submits == 1);
    active.turn(2);
    CHECK(active.sdk.submits == 1);
    active.sdk.complete(false);
    active.turn(3);
    CHECK(active.done(sent).service.effectUncertain);
    CHECK(active.done(sent).service.outcome == C::SdoOutcome::CANCELLED);

    Rig expired;
    const auto deadline = expired.request(1, true, 5);
    CHECK(expired.owner.service(0, budget()));
    expired.turn(5);
    CHECK(expired.sdk.submits == 0);
    CHECK(expired.done(deadline).service.outcome == C::SdoOutcome::TIMEOUT);
    expired.turn(6);
    CHECK(expired.sdk.submits == 0);
}

void faultsRecoveryAndFailureOwnership() {
    for (unsigned kind = 0; kind < 3; ++kind) {
        Rig r;
        const auto first = r.request(1, true);
        if (PAIRED_CHANNELS)
            r.request(2, true); // Queued behind the first; one channel has the active one only.
        r.turn(0);
        r.turn(1);
        CHECK(r.sdk.submits == 1);
        if (kind == 0)
            r.sdk.pending.busOff = true;
        if (kind == 1)
            r.sdk.pending.fault = true;
        if (kind == 2)
            r.sdk.pending.rxDropped = 3;
        r.turn(2);
        CHECK(r.adapter.diagnostics().inhibited && r.owner.resources().inhibited);
        CHECK(r.sdk.submits == 1 && r.owner.result(first));
        if (kind == 0)
            CHECK(r.owner.stats().busOffEvents == 1);
        if (kind == 2)
            CHECK(r.owner.stats().rxDropped == 3);
        // Completion and RX from the old controller are retired by destruction.
        r.sdk.complete(true);
        r.sdk.push(frame(0x123, 1));
        O::RecoveryId recovery;
        CHECK(r.owner.recover(3, 100, recovery));
        r.turn(3, budget(0, 0, 0, 0, 1));
        CHECK(r.sdk.stops == 1 && r.sdk.starts == 1 && !r.sdk.active);
        CHECK(!r.owner.recoveryResult(recovery));
        r.turn(4, budget(0, 0, 0, 0, 1));
        CHECK(r.owner.recoveryResult(recovery));
        CHECK(r.owner.recoveryResult(recovery)->outcome == O::RecoveryOutcome::RETIRED);
        CHECK(r.sdk.starts == 2 && r.sdk.epoch == 2 && r.sdk.submits == 1);
        CHECK(!r.adapter.diagnostics().inhibited);
        r.turn(5);
        CHECK(r.sdk.submits == 1 && r.owner.stats().txFrames == 0 && r.owner.stats().rxFrames == 0);
    }
    for (unsigned failure = 0; failure < 2; ++failure) {
        Rig r;
        r.request(1, true);
        r.turn(0);
        r.turn(1);
        r.sdk.stopOk = failure != 0;
        r.sdk.startOk = failure == 0;
        O::RecoveryId recovery;
        CHECK(r.owner.recover(2, 100, recovery));
        r.turn(2, budget(0, 0, 0, 0, 1));
        if (failure)
            r.turn(3, budget(0, 0, 0, 0, 1));
        CHECK(r.owner.recoveryResult(recovery));
        CHECK(r.owner.recoveryResult(recovery)->outcome == O::RecoveryOutcome::FAILED);
        CHECK(r.adapter.diagnostics().inhibited && r.owner.resources().inhibited);
        CHECK(r.sdk.submits == 1);
        if (!failure)
            CHECK(r.sdk.active); // Failed retirement cannot release SDK storage.
        r.turn(4);
        CHECK(r.sdk.submits == 1);
    }
    Rig failedStart(false);
    failedStart.sdk.startOk = false;
    CHECK(!failedStart.adapter.start(config()));
    CHECK(!failedStart.adapter.start(config()) && failedStart.sdk.starts == 1);
    failedStart.sdk.stopOk = false;
    CHECK(!failedStart.adapter.shutdown());
    failedStart.sdk.stopOk = true;
    CHECK(failedStart.adapter.shutdown());
}

void frameShapesAndBoundedReceive() {
    Rig r;
    O::Transport t = r.adapter.transport();
    M::Frame value = frame(0x1fffffff, 0);
    value.extended = true;
    value.rtr = true;
    value.dlc = 8;
    CHECK(t.transmit(t.context, value, arbitraryId(r.services, 1), 1));
    CHECK(t.retire(t.context, arbitraryId(r.services, 1), 1));
    value.fd = true;
    CHECK(!t.transmit(t.context, value, arbitraryId(r.services, 2), 1));
    value.fd = false;
    value.brs = true;
    CHECK(!t.transmit(t.context, value, arbitraryId(r.services, 2), 1));
    value.brs = false;
    value.extended = false;
    CHECK(!t.transmit(t.context, value, arbitraryId(r.services, 2), 1));
    value = frame(0x123, 1);
    CHECK(!t.transmit(t.context, value, arbitraryId(r.services, 2), 2));
    M::Frame remote = frame(0x1fffffff, 0);
    remote.extended = remote.rtr = true;
    remote.dlc = 8;
    r.sdk.push(remote);
    O::ReceivedFrame copied;
    CHECK(t.receive(t.context, copied));
    CHECK(copied.frame.extended && copied.frame.rtr && copied.frame.dlc == 8);
    CHECK(copied.frame.payloadLength == 0 && !copied.receivedTimeKnown &&
          copied.controllerEpoch == 1);
    for (unsigned i = 0; i < 12; ++i)
        r.sdk.push(frame(0x123 + i, 1));
    const unsigned calls = r.sdk.receiveCalls;
    r.turn(0, budget(3, 0, 0, 0));
    CHECK(r.sdk.receiveCalls - calls == 3 && r.owner.stats().rxFrames == 3);
    for (unsigned i = 0; i < 17; ++i)
        r.sdk.push(frame(0x321, 1));
    r.turn(1);
    CHECK(r.adapter.diagnostics().inhibited && r.adapter.diagnostics().rxDropped > 0);
}

void unreadBootupPreventsAnOldHandoffFromTransmitting() {
    Rig r;
    const auto id = r.request(1, true);
    CHECK(r.owner.service(0, budget(0))); // Stage old work without SDK submission.
    CHECK(r.sdk.submits == 0);
    r.sdk.push(frame(0x123, 1));
    M::Frame boot = frame(0x701, 1);
    boot.data[0] = 0;
    r.sdk.push(boot);
    CHECK(r.owner.service(1, budget(1, 0, 0, 0)));
    CHECK(r.sdk.rxWrite - r.sdk.rxRead == 1);
    r.adapterOnly(1);
    CHECK(r.sdk.submits == 0 && !r.owner.result(id));
    CHECK(r.owner.service(2, budget(1, 0, 8, 0)));
    r.adapterOnly(2);
    CHECK(r.sdk.submits == 0 && r.owner.result(id));
    CHECK(r.done(id).service.outcome == C::SdoOutcome::REBOOT);
    CHECK(r.owner.service(3, budget()));
    r.adapterOnly(3);
    CHECK(r.sdk.submits == 0);
}

void rawErrorsRemainDiagnosticAndSaturate() {
    Rig r;
    r.sdk.pending.busErrors = 2;
    r.sdk.pending.errorFlags = 0x02;
    r.turn(0);
    r.sdk.pending.busErrors = 3;
    r.sdk.pending.errorFlags = 0x10;
    r.turn(1);
    CHECK(r.adapter.diagnostics().busErrors == 5 && r.adapter.diagnostics().errorFlags == 0x12);
    CHECK(!r.adapter.diagnostics().inhibited && r.owner.stats().txFrames == 0);
    r.sdk.pending.busErrors = UINT32_MAX;
    r.turn(2);
    CHECK(r.adapter.diagnostics().busErrors == UINT32_MAX);
    r.sdk.pending.busErrors = 1;
    r.turn(3);
    CHECK(r.adapter.diagnostics().busErrors == UINT32_MAX && r.sdk.submits == 0);
}

void backwardClockInvalidatesAtLatestOwnerTime() {
    Rig r;
    M::Frame heartbeat = frame(0x701, 1);
    heartbeat.data[0] = 5;
    r.sdk.push(heartbeat);
    r.turn(100);
    const auto request = r.request(1, true, 1000, 1000);
    r.turn(101);
    CHECK(r.sdk.submits == 1 && r.sdk.active);
    // A public owner action can accept a later time between adapter turns.
    CHECK(r.owner.reportController(C::ControllerState::AVAILABLE, 200));
    CHECK(r.services.node(0)->heartbeat.present);
    const uint32_t generation = r.services.node(0)->route.generation;
    r.adapterOnly(99);
    CHECK(r.adapter.diagnostics().inhibited && r.owner.resources().inhibited);
    CHECK(r.services.controllerState() == C::ControllerState::RECOVERING);
    CHECK(r.owner.lastTimeUs() == 200 && r.services.node(0)->boundaryUs == 200);
    CHECK(r.services.node(0)->route.generation > generation);
    CHECK(!r.services.node(0)->heartbeat.present);
    const O::Completion* terminal = r.owner.result(request);
    CHECK(terminal && terminal->service.outcome == C::SdoOutcome::BINDING_INVALIDATED);
    CHECK(terminal->service.completedUs == 200 && terminal->service.transfer.deadlineUs == 1000);
    CHECK(terminal->service.transmissionPossible && terminal->service.effectPossible &&
          terminal->service.effectUncertain && !terminal->service.protocolAcknowledged);
    // Local inhibition cannot prove an already submitted write was retired.
    CHECK(r.sdk.active && r.sdk.live && r.sdk.stops == 0 && r.sdk.starts == 1);
    uint8_t retained[sizeof(O::Completion)];
    std::memcpy(retained, terminal, sizeof retained);
    const unsigned events = r.sdk.eventCalls;
    for (uint64_t bad = 98; bad > 95; --bad) {
        r.adapterOnly(bad);
        CHECK(r.owner.result(request) == terminal);
        CHECK(!std::memcmp(retained, terminal, sizeof retained));
        CHECK(r.owner.lastTimeUs() == 200 && r.services.node(0)->boundaryUs == 200);
        CHECK(r.sdk.active && r.sdk.submits == 1 && r.sdk.stops == 0 && r.sdk.starts == 1);
        CHECK(r.sdk.eventCalls == events);
    }
}

struct BlockedConsole {
    unsigned reads = 0, writes = 0;
    static bool read(void* p, uint8_t& out) {
        ++static_cast<BlockedConsole*>(p)->reads;
        out = 'x';
        return true;
    }
    static bool write(void* p, uint8_t) {
        ++static_cast<BlockedConsole*>(p)->writes;
        return false;
    }
};
void sharedApplicationKeepsServingUnderPressure() {
    Rig r;
    BlockedConsole console;
    E::ProbeCallbacks callbacks;
    callbacks.context = &console;
    callbacks.readByte = BlockedConsole::read;
    callbacks.writeByte = BlockedConsole::write;
    E::ProbePlatform platform(callbacks);
    E::ProbeApp app(r.owner, platform);
    M::LICHUAN_C::ReadTarget selected;
    selected.channel = route();
    selected.nodeGeneration = 11;
    CHECK(app.select(selected));
    CHECK(platform.writeOutput(reinterpret_cast<const uint8_t*>("blocked"), 7));
    r.adapter.pollEvents(r.owner, 0);
    CHECK(app.service(0));
    r.adapter.promote(r.owner, 0);
    CHECK(r.sdk.submits == 0 && console.reads <= E::ProbePlatform::INPUT_BUDGET);
    M::LICHUAN_C::ReadOptions options;
    options.queueTimeoutUs = options.responseTimeoutUs = 100;
    CHECK(app.requestIdentity(0, 1000, options));
    for (uint64_t now = 1; now <= 4; ++now) {
        M::Frame heartbeat = frame(0x701, 1);
        heartbeat.data[0] = 5;
        r.sdk.push(heartbeat);
        const unsigned before = console.reads;
        r.adapter.pollEvents(r.owner, now);
        CHECK(app.service(now));
        r.adapter.promote(r.owner, now);
        CHECK(console.reads - before <= E::ProbePlatform::INPUT_BUDGET);
    }
    CHECK(r.sdk.submits == 1 && r.owner.stats().rxFrames == 4);
    CHECK(r.services.node(0)->nmt == C::NmtState::OPERATIONAL);
    CHECK(platform.stats().outputUsed == 7 && console.writes > 0);
}
void requiredPreOperationalIsRecheckedBeforeSdkSubmit() {
    Rig r;
    M::Frame heartbeat = frame(0x701, 1);
    heartbeat.data[0] = 0x7f;
    r.sdk.push(heartbeat, true, 0);
    r.turn(0);
    const auto id = r.request(1, true, 1000, 100, true);
    CHECK(r.owner.service(1, budget())); // Copied into TWAI queue, not submitted to SDK.
    CHECK(r.sdk.submits == 0);
    heartbeat.data[0] = 5;
    r.sdk.push(heartbeat, true, 2);
    r.adapterOnly(1); // Captured unread RX defers the SDK boundary.
    CHECK(r.sdk.submits == 0 && r.sdk.deferrals == 1);
    r.turn(2); // Sole owner now observes Operational; native handoff becomes ineligible.
    CHECK(r.services.node(0)->nmt == C::NmtState::OPERATIONAL && r.sdk.submits == 0);
    r.turn(3);
    CHECK(r.sdk.submits == 0);
    r.turn(101); // Original response deadline remains finite; no replay/requalification.
    CHECK(r.owner.result(id) && r.owner.result(id)->service.outcome == C::SdoOutcome::TIMEOUT);
    // Contract change: the frame never reached the SDK, so no effect is possible.
    checkNeverSent(r.done(id));
    CHECK(r.sdk.submits == 0 && !r.services.unresolved(0));
}
void requiredPreOperationalAgeIsRecheckedBeforeSdkSubmit() {
    Rig r;
    CHECK(r.services.setServiceAge(0, 10, 0));
    M::Frame heartbeat = frame(0x701, 1);
    heartbeat.data[0] = 0x7f;
    r.sdk.push(heartbeat, true, 0);
    r.turn(0);
    const auto id = r.request(1, true, 1000, 100, true);
    CHECK(r.owner.service(9, budget())); // Current at handoff, just inside inclusive age cap.
    CHECK(r.sdk.submits == 0);
    r.adapterOnly(11); // SDK boundary is later; stored NMT premise has expired.
    CHECK(r.sdk.submits == 0);
    CHECK(r.sdk.attempts == 0); // Refused by the recheck, before any submit call.
    r.turn(12);
    CHECK(r.sdk.submits == 0);
    // Contract change: promotion refused it, so the owner gets a proved-unsent report
    // now. It no longer waits for a response timeout and claims an uncertain effect.
    CHECK(r.tap.count == 1 && r.tap.log[0].provedUnsent && !r.tap.log[0].transmitted);
    CHECK(r.done(id).service.outcome == C::SdoOutcome::TRANSPORT_ERROR);
    CHECK(r.done(id).service.completedUs == 12);
    checkNeverSent(r.done(id));
    CHECK(!r.services.unresolved(0));
    r.turn(109);
    CHECK(r.sdk.submits == 0 && r.sdk.attempts == 0);
}
C::NmtTransfer nmtWork(Rig& r, C::NmtCommand command = C::NmtCommand::START,
                         uint64_t deadline = 100) {
    C::NmtTransfer work;
    work.command = command;
    work.target = r.services.node(0)->route;
    work.request = 99;
    work.deadlineUs = deadline;
    work.queueTimeoutUs = 20;
    return work;
}
void explicitNmtUsesTheSameTransportRetirement() {
    {
        Rig r;
        C::NmtRequestId id;
        CHECK(r.owner.admitNmt(nmtWork(r), 0, id));
        CHECK(r.owner.service(1, budget())); // Adapter copied, SDK has not borrowed it.
        CHECK(r.sdk.submits == 0 && r.owner.cancelNmt(id, 2));
        r.adapterOnly(2);
        CHECK(r.sdk.submits == 0 && r.owner.nmtResult(id)->outcome == C::NmtOutcome::CANCELLED);
        CHECK(!r.owner.nmtResult(id)->transmissionPossible);
        CHECK(r.owner.releaseNmt(id));
        r.turn(3);
        CHECK(r.sdk.submits == 0);
    }
    {
        Rig r;
        C::NmtRequestId id;
        CHECK(r.owner.admitNmt(nmtWork(r), 0, id));
        r.turn(1);
        CHECK(r.sdk.submits == 1 && r.sdk.sent[0].id == 0 && r.sdk.sent[0].data[1] == 1);
        CHECK(!r.owner.nmtResult(id) && r.owner.stats().txFrames == 0);
        CHECK(r.owner.cancelNmt(id, 2)); // Active SDK storage cannot yet be retired.
        CHECK(r.owner.nmtResult(id)->transmissionPossible && r.owner.releaseNmt(id));
        C::NmtRequestId next;
        CHECK(!r.owner.admitNmt(nmtWork(r), 3, next));
        r.sdk.complete(true);
        r.turn(3);
        r.turn(4); // Exact old report retires transport; it never revives a released result.
        CHECK(r.sdk.submits == 1 && r.owner.stats().txFrames == 1);
        CHECK(!r.owner.nmtResult(id));
    }
    {
        Rig r;
        r.sdk.submitResult = E::TwaiSubmit::FAILED_UNCERTAIN;
        C::NmtRequestId id;
        CHECK(r.owner.admitNmt(nmtWork(r), 0, id));
        r.turn(1);
        CHECK(!r.owner.nmtResult(id));
        r.turn(2);
        CHECK(r.owner.nmtResult(id)->outcome == C::NmtOutcome::TRANSPORT_ERROR &&
              r.owner.nmtResult(id)->transmissionPossible);
        CHECK(r.sdk.submits == 0 && r.owner.stats().txRejected == 1);
        r.turn(3);
        CHECK(r.sdk.submits == 0); // Failed submit is never replayed.
    }
    {
        Rig r;
        C::NmtRequestId id;
        CHECK(r.owner.admitNmt(nmtWork(r, C::NmtCommand::RESET_NODE), 0, id));
        r.turn(1);
        O::RecoveryId recovery;
        CHECK(r.owner.recover(2, 20, recovery));
        CHECK(r.owner.nmtResult(id)->outcome == C::NmtOutcome::INVALIDATED &&
              r.owner.nmtResult(id)->transmissionPossible);
        r.turn(3, budget(8, 2, 8, 2, 1));
        r.turn(4, budget(8, 2, 8, 2, 1));
        CHECK(r.owner.recoveryResult(recovery)->outcome == O::RecoveryOutcome::RETIRED);
        CHECK(r.sdk.stops == 1 && r.sdk.starts == 2 && r.sdk.submits == 1);
        CHECK(r.owner.nmtResult(id)->outcome == C::NmtOutcome::INVALIDATED);
        r.turn(5);
        CHECK(r.sdk.submits == 1 && r.services.node(0)->route.generation > 11);
    }
}

// ---------------------------------------------------------------------------
// Adversarial timelines through the real owner and SdoClient. FakeDriver is a
// scripted fake: nothing here measures controller, arbitration or bus behavior.
// ---------------------------------------------------------------------------
M::Frame wire(uint32_t id, uint8_t b0, uint8_t b1, uint8_t b2, uint8_t b3, uint8_t b4, uint8_t b5,
              uint8_t b6, uint8_t b7) {
    M::Frame value = frame(id);
    const uint8_t bytes[8] = {b0, b1, b2, b3, b4, b5, b6, b7};
    std::memcpy(value.data, bytes, 8);
    return value;
}
bool sameWire(const M::Frame& a, const M::Frame& b) {
    return a.id == b.id && a.extended == b.extended && a.rtr == b.rtr && a.fd == b.fd &&
           a.dlc == b.dlc && a.payloadLength == b.payloadLength && !std::memcmp(a.data, b.data, 8);
}
M::Frame writeRequest(unsigned node = 1) {
    return wire(0x600 + node, 0x2b, 0x40, 0x60, 0, 0x34, 0x12, 0, 0);
}
M::Frame stopRequest(unsigned node) {
    return wire(0x600 + node, 0x2b, 0x40, 0x60, 0, 0x0b, 0, 0, 0);
}
M::Frame writeAck(unsigned node = 1) {
    return wire(0x580 + node, 0x60, 0x40, 0x60, 0, 0, 0, 0, 0);
}
M::Frame heartbeatOf(unsigned node, uint8_t state = 5) {
    M::Frame value = frame(0x700 + node, 1);
    value.data[0] = state;
    return value;
}
struct Retained {
    const O::Completion* pointer = nullptr;
    uint8_t bytes[sizeof(O::Completion)];
    void take(const O::Completion& terminal) {
        pointer = &terminal;
        std::memcpy(bytes, &terminal, sizeof bytes);
    }
    bool unchanged(Rig& r, O::RequestId id) const {
        return r.owner.result(id) == pointer && !std::memcmp(bytes, pointer, sizeof bytes);
    }
};
// Never left the SDK boundary: no possible effect and no quarantine, whatever the outcome.
void checkNeverSent(const O::Completion& c) {
    CHECK(!c.service.transmissionPossible && !c.service.transmissionConfirmed);
    CHECK(!c.service.effectPossible && !c.service.effectUncertain);
    CHECK(!c.service.protocolAcknowledged);
}

// Arbitration loss is modelled only as what DEC-4 expects the SDK to show: a raw
// error callback with no TX-done, then a later success. This is a fake, not a
// measurement of arbitration behavior; W11 counts failed TX on a real bench.
void arbitrationLossThenSuccessIsOneSubmission() {
    Rig r;
    const auto id = r.request(1, true);
    r.turn(0);
    CHECK(r.sdk.submits == 1 && sameWire(r.sdk.sent[0], writeRequest()));
    for (uint64_t now = 1; now <= 3; ++now) {
        r.sdk.pending.busErrors = 1;
        r.sdk.pending.errorFlags = 0x02;
        r.turn(now);
        CHECK(!r.owner.result(id) && !r.services.unresolved(0));
        CHECK(!r.adapter.diagnostics().inhibited && !r.owner.resources().inhibited);
        CHECK(r.sdk.active && r.sdk.submits == 1 && r.sdk.attempts == 1);
    }
    CHECK(r.adapter.diagnostics().busErrors == 3 && r.adapter.diagnostics().errorFlags == 0x02);
    CHECK(r.tap.count == 0 && r.owner.stats().txRejected == 0);
    r.sdk.complete(true);
    r.turn(4);
    CHECK(r.tap.count == 1 && r.tap.log[0].transmitted && !r.tap.log[0].provedUnsent);
    CHECK(r.owner.stats().txFrames == 1 && !r.owner.result(id));
    r.sdk.push(writeAck());
    r.turn(5);
    CHECK(r.done(id).service.outcome == C::SdoOutcome::SUCCESS);
    CHECK(r.done(id).service.transmissionConfirmed && r.done(id).service.protocolAcknowledged);
    CHECK(!r.done(id).service.effectUncertain && !r.services.unresolved(0));
    r.turn(6);
    CHECK(r.sdk.submits == 1 && r.sdk.attempts == 1 && r.owner.stats().txRejected == 0);
}

// No ACK: hardware retransmission keeps the frame ACTIVE. The application resends
// nothing, the transfer ends unresolved, and only recovery retires the frames.
void noAckStaysActiveUntilRecoveryRetiresBothSlots() {
    Rig r;
    const auto stuck = r.request(1, true, 1000, 100);
    r.turn(0);
    CHECK(sameWire(r.sdk.sent[0], writeRequest()));
    const auto queued = r.request(2, true, 1000, 500);
    r.turn(1);
    CHECK(r.sdk.submits == 1 && r.sdk.attempts == 1);
    r.turn(99);
    CHECK(!r.owner.result(stuck) && !r.owner.result(queued));
    r.turn(100);
    const O::Completion& terminal = r.done(stuck);
    CHECK(terminal.service.outcome == C::SdoOutcome::TIMEOUT && terminal.service.completedUs == 100);
    CHECK(terminal.service.transmissionPossible && terminal.service.effectPossible);
    CHECK(terminal.service.effectUncertain && !terminal.service.transmissionConfirmed);
    CHECK(!terminal.service.protocolAcknowledged && r.services.unresolved(0));
    Retained first;
    first.take(terminal);
    // The stuck frame stalls the interface; node 2 waits and nothing is resent.
    for (uint64_t now = 101; now <= 200; now += 33) {
        r.turn(now);
        CHECK(r.sdk.active && r.sdk.submits == 1 && r.sdk.attempts == 1);
        CHECK(!r.owner.result(queued) && !r.owner.localTxRetired(0) && !r.owner.localTxRetired(1));
    }
    CHECK(first.unchanged(r, stuck));
    CHECK(r.sdk.stops == 0); // No automatic recovery.
    O::RecoveryId recovery;
    CHECK(r.owner.recover(300, 400, recovery));
    CHECK(r.owner.result(queued));
    checkNeverSent(r.done(queued));
    Retained second;
    second.take(r.done(queued));
    r.turn(301, budget(0, 0, 0, 0, 1));
    CHECK(r.sdk.stops == 1 && !r.sdk.active && !r.owner.recoveryResult(recovery));
    r.turn(302, budget(0, 0, 0, 0, 1));
    CHECK(r.owner.recoveryResult(recovery));
    CHECK(r.owner.recoveryResult(recovery)->outcome == O::RecoveryOutcome::RETIRED);
    CHECK(r.sdk.starts == 2 && r.sdk.epoch == 2 && r.owner.controllerEpoch() == 2);
    // Retirement proves neither frame can leave later. It clears no uncertainty.
    CHECK(first.unchanged(r, stuck) && second.unchanged(r, queued));
    CHECK(r.done(stuck).service.effectUncertain);
    r.turn(303);
    CHECK(r.sdk.submits == 1 && r.sdk.attempts == 1 && r.tap.count == 0);
    CHECK(r.owner.localTxRetired(0) && r.owner.localTxRetired(1));
    // All adapter slots are free again, with no old report behind them.
    O::Transport t = r.adapter.transport();
    O::TxReport report;
    CHECK(!t.takeTxReport(t.context, report));
    for (uint32_t n = 0; n < E::TwaiAdapter::TX_CAPACITY; ++n)
        CHECK(t.transmit(t.context, frame(0x123 + n), arbitraryId(r.services, 91 + n), 2));
    const uint32_t beyond = E::TwaiAdapter::TX_CAPACITY;
    CHECK(!t.transmit(t.context, frame(0x123 + beyond), arbitraryId(r.services, 91 + beyond), 2));
    for (uint32_t n = 0; n < E::TwaiAdapter::TX_CAPACITY; ++n)
        CHECK(t.retire(t.context, arbitraryId(r.services, 91 + n), 2));
}

// C-04: the interrupt lands between the turn's poll and the submit attempt.
void rxInterruptDuringPromotionDefersWithoutQuarantine() {
    Rig r;
    const auto id = r.request(1, true);
    r.stage(0);
    r.sdk.push(heartbeatOf(1));
    r.adapter.promote(r.owner, 0);
    CHECK(r.sdk.attempts == 1 && r.sdk.deferrals == 1 && r.sdk.submits == 0 && !r.sdk.active);
    CHECK(!r.owner.result(id) && !r.services.unresolved(0) && r.tap.count == 0);
    CHECK(!r.adapter.diagnostics().inhibited && r.owner.stats().txRejected == 0);
    const M::Frame deferred = r.sdk.attempted[0];
    const uint32_t serial = r.sdk.attemptSerials[0];
    CHECK(sameWire(deferred, writeRequest()));
    r.turn(1); // The owner drains RX first, then the same queued bytes reach the SDK.
    CHECK(r.owner.stats().rxFrames == 1 && r.services.node(0)->nmt == C::NmtState::OPERATIONAL);
    CHECK(r.sdk.attempts == 2 && r.sdk.submits == 1 && r.sdk.deferrals == 1);
    CHECK(sameWire(r.sdk.sent[0], deferred) && r.sdk.serials[0] == serial);
    r.turn(2);
    CHECK(r.sdk.attempts == 2 && r.sdk.submits == 1);
    r.sdk.complete(true);
    r.sdk.push(writeAck());
    r.turn(3);
    CHECK(r.done(id).service.outcome == C::SdoOutcome::SUCCESS);
    CHECK(r.done(id).service.transmissionConfirmed && !r.services.unresolved(0));

    // A deferred frame has no outstanding request, so a same-route frame is not its
    // reply. The transfer ends lost, as any unsolicited frame does, and never submits.
    Rig early;
    const auto never = early.request(1, true);
    early.stage(0);
    early.sdk.push(writeAck());
    early.adapter.promote(early.owner, 0);
    CHECK(early.sdk.deferrals == 1 && early.sdk.submits == 0);
    early.turn(1);
    CHECK(early.done(never).service.outcome == C::SdoOutcome::CORRELATION_LOST);
    checkNeverSent(early.done(never));
    CHECK(early.services.unresolved(0));
    CHECK(early.sdk.attempts == 1 && early.sdk.submits == 0);
    early.turn(2);
    CHECK(early.sdk.attempts == 1 && early.sdk.submits == 0);
}

// C-07: the split narrows the race and does not close it. A frame that completes
// after the poll has its reply routed before its report.
void sameTickReplyOvertakesTxDoneAndBlocksUrgentForOneTurn() {
    Rig r;
    const auto id = r.request(1, true);
    const O::SequenceId sequence = r.lastSequence;
    r.turn(0);
    CHECK(r.sdk.submits == 1 && r.sdk.active);
    r.adapter.pollEvents(r.owner, 1);
    r.sdk.complete(true);
    r.sdk.push(writeAck());
    CHECK(r.owner.service(1, budget()));
    // The reply settled the transfer; the frame's report is still in the mailbox.
    const O::Completion& terminal = r.done(id);
    CHECK(terminal.service.outcome == C::SdoOutcome::SUCCESS);
    CHECK(terminal.service.protocolAcknowledged && !terminal.service.transmissionConfirmed);
    CHECK(r.owner.stats().txFrames == 0 && r.tap.count == 0);
    Retained held;
    held.take(terminal);
    // Until that report, the local frame is not retired: urgent work is refused.
    const O::BusRequest stop = r.stopWork(sequence, 1, 2, 1000, 100);
    O::RequestId urgent;
    CHECK(r.owner.admitUrgent(stop, 1, urgent) == O::BusAdmission::INHIBITED);
    CHECK(!r.owner.localTxRetired(0));
    r.adapter.promote(r.owner, 1);
    CHECK(r.sdk.submits == 1);
    // The next turn settles it. The report records TX and cannot rewrite the terminal.
    r.turn(2);
    CHECK(r.tap.count == 1 && r.tap.log[0].transmitted && !r.tap.log[0].provedUnsent);
    CHECK(r.owner.stats().txFrames == 1 && r.owner.localTxRetired(0));
    CHECK(held.unchanged(r, id));
    CHECK(r.owner.admitUrgent(stop, 2, urgent) == O::BusAdmission::ACCEPTED);
    r.turn(3);
    CHECK(r.sdk.submits == 2 && sameWire(r.sdk.sent[1], stopRequest(1)));
    CHECK(!r.services.unresolved(0));

    // A completion before the poll takes the ordinary order: report, then reply.
    Rig ordered;
    const auto normal = ordered.request(1, true);
    const O::SequenceId orderedSequence = ordered.lastSequence;
    ordered.turn(0);
    ordered.sdk.complete(true);
    ordered.sdk.push(writeAck());
    ordered.stage(1);
    CHECK(ordered.done(normal).service.outcome == C::SdoOutcome::SUCCESS);
    CHECK(ordered.done(normal).service.transmissionConfirmed);
    CHECK(ordered.owner.stats().txFrames == 1 && ordered.owner.localTxRetired(0));
    CHECK(ordered.owner.admitUrgent(ordered.stopWork(orderedSequence, 1, 2, 1000, 100), 1, urgent) ==
          O::BusAdmission::ACCEPTED);
}

void controllerFailureWhileQueuedAndActive() {
    for (unsigned kind = 0; kind < 3; ++kind) {
        Rig r;
        const auto active = r.request(1, true);
        r.turn(0);
        const auto queued = r.request(2, true);
        r.turn(1);
        CHECK(r.sdk.submits == 1 && r.sdk.attempts == 1);
        if (kind == 0)
            r.sdk.pending.busOff = true;
        else if (kind == 1)
            r.sdk.pending.fault = true;
        else
            r.sdk.pending.rxDropped = 1;
        r.turn(2);
        CHECK(r.adapter.diagnostics().inhibited && r.owner.resources().inhibited);
        const C::SdoOutcome expected =
            kind == 0 ? C::SdoOutcome::BUS_OFF : C::SdoOutcome::BINDING_INVALIDATED;
        // The active frame may have left; the queued frame provably has not.
        CHECK(r.done(active).service.outcome == expected);
        CHECK(r.done(active).service.transmissionPossible && r.done(active).service.effectUncertain);
        CHECK(r.done(queued).service.outcome == expected);
        checkNeverSent(r.done(queued));
        Retained one, two;
        one.take(r.done(active));
        two.take(r.done(queued));
        r.turn(3);
        CHECK(r.sdk.submits == 1 && r.sdk.attempts == 1 && r.sdk.active);
        O::RecoveryId recovery;
        CHECK(r.owner.recover(4, 100, recovery));
        r.turn(5, budget(0, 0, 0, 0, 1));
        r.turn(6, budget(0, 0, 0, 0, 1));
        CHECK(r.owner.recoveryResult(recovery)->outcome == O::RecoveryOutcome::RETIRED);
        CHECK(one.unchanged(r, active) && two.unchanged(r, queued));
        r.turn(7);
        CHECK(r.sdk.submits == 1 && r.sdk.attempts == 1 && r.sdk.stops == 1);
    }
    // Bus-off between this turn's poll and the submit: no acceptance, then the
    // controller fact arrives on the next poll and decides the terminal.
    Rig late;
    const auto id = late.request(1, true);
    late.stage(0);
    late.sdk.pending.busOff = true;
    late.adapter.promote(late.owner, 0);
    CHECK(late.sdk.attempts == 1 && late.sdk.submits == 0 && !late.sdk.active);
    late.turn(1);
    CHECK(late.done(id).service.outcome == C::SdoOutcome::BUS_OFF);
    checkNeverSent(late.done(id));
    CHECK(late.adapter.diagnostics().inhibited);
    late.turn(2);
    CHECK(late.sdk.attempts == 1 && late.sdk.submits == 0);
}

// The SDK source does not establish what is_tx_success reads for a frame that ends
// with bus-off, so a final "transmitted" report there must not settle anything.
void busOffWithAnActiveFrameAndAFinalReport() {
    // Both facts in one snapshot, either value of the flag.
    for (unsigned transmitted = 0; transmitted < 2; ++transmitted) {
        Rig r;
        const auto id = r.request(1, true);
        r.turn(0);
        r.sdk.complete(transmitted != 0);
        r.sdk.pending.busOff = true;
        r.turn(1);
        const O::Completion& c = r.done(id);
        CHECK(c.service.outcome == C::SdoOutcome::BUS_OFF);
        CHECK(c.service.transmissionPossible && c.service.effectPossible);
        CHECK(c.service.effectUncertain && !c.service.protocolAcknowledged);
        CHECK(!c.service.transmissionConfirmed && r.services.unresolved(0));
        CHECK(r.adapter.diagnostics().inhibited && r.owner.resources().inhibited);
        CHECK(r.owner.stats().busOffEvents == 1);
        CHECK(r.sdk.submits == 1 && r.sdk.attempts == 1);
        Retained held;
        held.take(c);
        r.turn(2);
        CHECK(held.unchanged(r, id));
        CHECK(r.sdk.submits == 1 && r.sdk.attempts == 1);
    }
    // Bus-off first, the report on a later poll: the terminal already stands.
    {
        Rig r;
        const auto id = r.request(1, true);
        r.turn(0);
        r.sdk.pending.busOff = true;
        r.turn(1);
        Retained held;
        held.take(r.done(id));
        CHECK(!r.done(id).service.transmissionConfirmed && r.done(id).service.effectUncertain);
        r.sdk.complete(true);
        r.turn(2);
        CHECK(held.unchanged(r, id));
        CHECK(!r.done(id).service.transmissionConfirmed && r.done(id).service.effectUncertain);
        CHECK(r.owner.localTxRetired(0) && r.adapter.diagnostics().inhibited);
        r.turn(3);
        CHECK(r.sdk.submits == 1 && r.sdk.attempts == 1);
    }
    // Report first, bus-off on a later poll. The adapter cannot tell this report from
    // a success, so "confirmed" survives, but no protocol acknowledgement is invented
    // and the effect stays uncertain.
    {
        Rig r;
        const auto id = r.request(1, true);
        r.turn(0);
        r.sdk.complete(true);
        r.turn(1);
        CHECK(r.tap.count == 1 && r.tap.log[0].transmitted && !r.owner.result(id));
        r.sdk.pending.busOff = true;
        r.turn(2);
        const O::Completion& c = r.done(id);
        CHECK(c.service.outcome == C::SdoOutcome::BUS_OFF && !c.service.protocolAcknowledged);
        CHECK(c.service.transmissionPossible && c.service.effectUncertain);
        CHECK(r.services.unresolved(0));
        r.turn(3);
        CHECK(r.sdk.submits == 1 && r.sdk.attempts == 1);
    }
}

// SDK review: a frame completing just before stop has its report discarded. The
// frame may be on the wire, so retirement says nothing about delivery either way.
void retirementNeverImpliesNotDelivered() {
    // Through recovery.
    {
        Rig r;
        const auto id = r.request(1, true);
        r.turn(0);
        r.sdk.complete(true); // Delivered, but no poll reads it before the stop.
        O::RecoveryId recovery;
        CHECK(r.owner.recover(1, 100, recovery));
        r.turn(2, budget(0, 0, 0, 0, 1));
        r.turn(3, budget(0, 0, 0, 0, 1));
        CHECK(r.owner.recoveryResult(recovery)->outcome == O::RecoveryOutcome::RETIRED);
        CHECK(r.sdk.stops == 1 && r.tap.count == 0 && r.owner.stats().txFrames == 0);
        const O::Completion& c = r.done(id);
        CHECK(c.service.transmissionPossible && c.service.effectPossible);
        CHECK(c.service.effectUncertain && !c.service.transmissionConfirmed);
        // Recovery cancels local work. CANCELLED here is no claim about the wire.
        CHECK(c.service.outcome == C::SdoOutcome::CANCELLED);
        CHECK(c.reason == O::CompletionReason::RECOVERY);
        r.turn(4);
        CHECK(r.sdk.submits == 1 && r.tap.count == 0);
        CHECK(r.done(id).service.effectUncertain);
    }
    // Through local shutdown, as FirmwareProbe's failure path does.
    {
        Rig r;
        const auto id = r.request(1, true);
        r.turn(0);
        r.sdk.complete(true);
        CHECK(r.owner.reportController(C::ControllerState::RECOVERING, 1));
        CHECK(r.adapter.shutdown());
        CHECK(r.sdk.stops == 1 && !r.sdk.live && r.tap.count == 0);
        CHECK(r.done(id).service.transmissionPossible && r.done(id).service.effectUncertain);
        CHECK(!r.done(id).service.transmissionConfirmed);
    }
}

// A deferred frame ends before it ever reaches the SDK. Unread RX with a zero RX
// budget keeps the fake deferring without the owner draining it.
void deferredWithdrawalNeverTouchesTheSdk() {
    // Queue cutoff.
    {
        Rig r;
        r.sdk.push(heartbeatOf(1));
        const auto id = r.request(1, true, 1000, 50);
        const O::SequenceId sequence = r.lastSequence;
        for (uint64_t now = 0; now < 50; now += 7) {
            r.turn(now, budget(0));
            CHECK(!r.owner.result(id) && r.sdk.submits == 0);
        }
        r.turn(49, budget(0));
        CHECK(!r.owner.result(id) && r.sdk.submits == 0 && r.sdk.deferrals == r.sdk.attempts);
        CHECK(r.sdk.deferrals > 1);
        const unsigned attempts = r.sdk.attempts;
        r.turn(50, budget(0)); // now >= cutoff expires.
        CHECK(r.done(id).service.outcome == C::SdoOutcome::TIMEOUT);
        CHECK(r.done(id).service.completedUs == 50);
        checkNeverSent(r.done(id));
        CHECK(!r.services.unresolved(0) && r.sdk.submits == 0 && r.sdk.attempts == attempts);
        CHECK(r.owner.stats().txRejected == 0 && r.owner.localTxRetired(0));
        CHECK(r.owner.sequence(sequence));
        // No quarantine: the same reservation closes and the route takes new work.
        CHECK(r.owner.release(id) && r.owner.releaseSequence(sequence));
        r.turn(51);
        CHECK(r.sdk.submits == 0 && r.sdk.attempts == attempts);
        const auto next = r.request(1, true, 1000, 50);
        r.turn(52);
        CHECK(r.sdk.submits == 1 && sameWire(r.sdk.sent[0], writeRequest()));
        CHECK(!r.owner.result(next));
    }
    // Overall deadline shorter than the queue budget.
    {
        Rig r;
        r.sdk.push(heartbeatOf(1));
        const auto id = r.request(1, true, 30, 100);
        r.turn(0, budget(0));
        r.turn(29, budget(0));
        CHECK(!r.owner.result(id) && r.sdk.deferrals == 2);
        r.turn(30, budget(0));
        CHECK(r.done(id).service.outcome == C::SdoOutcome::TIMEOUT);
        CHECK(r.done(id).service.completedUs == 30);
        checkNeverSent(r.done(id));
        CHECK(!r.services.unresolved(0) && r.sdk.submits == 0);
    }
    // Cancel.
    {
        Rig r;
        r.sdk.push(heartbeatOf(1));
        const auto id = r.request(1, true);
        r.turn(0, budget(0));
        CHECK(r.sdk.deferrals == 1);
        CHECK(r.owner.cancel(id, 1) == O::Cancel::CANCELLED);
        CHECK(r.done(id).service.outcome == C::SdoOutcome::CANCELLED);
        checkNeverSent(r.done(id));
        CHECK(!r.services.unresolved(0) && r.owner.localTxRetired(0));
        r.turn(2);
        CHECK(r.sdk.submits == 0 && r.sdk.deferrals == 1);
    }
}

// The response clock starts at SDK submission. Handoff here is t=0.
void responseClockStartsAtSdkSubmission() {
    // Deferred to t=30, so the cutoff is 130, not 100. now >= cutoff expires.
    {
        Rig r;
        r.sdk.push(heartbeatOf(1));
        const auto id = r.request(1, true, 1000, 100);
        r.turn(0, budget(0));
        r.turn(29, budget(0));
        CHECK(r.sdk.submits == 0);
        r.turn(30);
        CHECK(r.sdk.submits == 1 && !r.owner.result(id));
        r.turn(100); // Handoff clock and queue cutoff would both have ended it here.
        r.turn(129);
        CHECK(!r.owner.result(id));
        r.turn(130);
        CHECK(r.done(id).service.outcome == C::SdoOutcome::TIMEOUT);
        CHECK(r.done(id).service.completedUs == 130);
        CHECK(r.done(id).service.transmissionPossible && r.done(id).service.effectUncertain);
        CHECK(r.services.unresolved(0) && r.sdk.submits == 1);
    }
    // A reply in the last open microsecond still succeeds.
    {
        Rig r;
        r.sdk.push(heartbeatOf(1));
        const auto id = r.request(1, true, 1000, 100);
        r.turn(0, budget(0));
        r.turn(30);
        r.sdk.complete(true);
        r.sdk.push(writeAck());
        r.turn(129);
        CHECK(r.done(id).service.outcome == C::SdoOutcome::SUCCESS);
        CHECK(!r.services.unresolved(0));
    }
    // The same reply at the cutoff is too late.
    {
        Rig r;
        r.sdk.push(heartbeatOf(1));
        const auto id = r.request(1, true, 1000, 100);
        r.turn(0, budget(0));
        r.turn(30);
        r.sdk.push(writeAck());
        r.turn(130);
        CHECK(r.done(id).service.outcome == C::SdoOutcome::TIMEOUT);
        CHECK(r.done(id).service.effectUncertain && r.services.unresolved(0));
    }
    // Queue edge: the last permitted submission, with a full response window after it.
    {
        Rig r;
        r.sdk.push(heartbeatOf(1));
        const auto id = r.request(1, true, 1000, 100);
        r.turn(0, budget(0));
        r.turn(99);
        CHECK(r.sdk.submits == 1);
        r.turn(198);
        CHECK(!r.owner.result(id));
        r.turn(199);
        CHECK(r.done(id).service.outcome == C::SdoOutcome::TIMEOUT);
        CHECK(r.done(id).service.completedUs == 199);
    }
    // One microsecond later the queue budget has gone and nothing reaches the SDK.
    {
        Rig r;
        r.sdk.push(heartbeatOf(1));
        const auto id = r.request(1, true, 1000, 100);
        r.turn(0, budget(0));
        r.turn(100);
        CHECK(r.sdk.submits == 0);
        CHECK(r.done(id).service.outcome == C::SdoOutcome::TIMEOUT);
        checkNeverSent(r.done(id));
        CHECK(!r.services.unresolved(0));
    }
    // The overall deadline caps the response window.
    {
        Rig r;
        const auto id = r.request(1, true, 50, 100);
        r.turn(10);
        CHECK(r.sdk.submits == 1);
        r.turn(49);
        CHECK(!r.owner.result(id));
        r.turn(50);
        CHECK(r.done(id).service.outcome == C::SdoOutcome::TIMEOUT);
        CHECK(r.done(id).service.completedUs == 50);
        CHECK(r.done(id).service.effectUncertain);
    }
}

// C-20: node 1's pending frame cannot make node 2's never-submitted stop look
// transmitted or quarantine node 2.
void pendingFrameDoesNotQuarantineAHealthyAxis() {
    {
        Rig r;
        const auto pending = r.request(1, true);
        r.turn(0);
        CHECK(r.sdk.active && sameWire(r.sdk.sent[0], writeRequest()));
        r.now = 1;
        const auto stop = r.urgentStop(2, 1000, 50);
        const O::SequenceId stopSequence = r.lastSequence;
        r.turn(1);
        CHECK(r.sdk.submits == 1 && r.sdk.attempts == 1 && !r.owner.result(stop));
        r.turn(50);
        CHECK(!r.owner.result(stop));
        r.turn(51); // Admitted at 1 with a 50 microsecond queue budget.
        const O::Completion& terminal = r.done(stop);
        CHECK(terminal.service.outcome == C::SdoOutcome::TIMEOUT);
        CHECK(terminal.service.completedUs == 51);
        checkNeverSent(terminal);
        CHECK(!r.services.unresolved(1) && r.owner.sequence(stopSequence));
        CHECK(r.owner.localTxRetired(1) && !r.owner.localTxRetired(0));
        // The stop's bytes never reached the SDK boundary at all.
        CHECK(r.sdk.submits == 1 && r.sdk.attempts == 1 && r.sdk.attempted[0].id == 0x601);
        Retained held;
        held.take(terminal);
        // Node 1 runs to its own uncertain end; node 2's retained result does not move.
        r.turn(100);
        CHECK(r.done(pending).service.outcome == C::SdoOutcome::TIMEOUT);
        CHECK(r.done(pending).service.transmissionPossible && r.services.unresolved(0));
        CHECK(held.unchanged(r, stop) && !r.services.unresolved(1));
        r.sdk.complete(true);
        r.turn(101);
        CHECK(held.unchanged(r, stop) && r.sdk.attempts == 1);
        r.sdk.pending.busOff = true;
        r.turn(102);
        CHECK(held.unchanged(r, stop));
        O::RecoveryId recovery;
        CHECK(r.owner.recover(103, 200, recovery));
        r.turn(104, budget(0, 0, 0, 0, 1));
        r.turn(105, budget(0, 0, 0, 0, 1));
        CHECK(r.owner.recoveryResult(recovery)->outcome == O::RecoveryOutcome::RETIRED);
        CHECK(held.unchanged(r, stop) && r.sdk.attempts == 1);
        checkNeverSent(r.done(stop));
    }
    // With no event budget the owner cannot retire the frame directly, so the same
    // fact arrives as the frame's final report, exactly and proved unsent.
    {
        Rig r;
        const auto pending = r.request(1, true);
        r.turn(0);
        r.now = 1;
        const auto stop = r.urgentStop(2, 1000, 50);
        r.turn(1);
        const O::ServiceBudget noEvents = budget(8, 2, 0, 2);
        r.turn(51, noEvents);
        checkNeverSent(r.done(stop));
        CHECK(!r.owner.localTxRetired(1));
        Retained held;
        held.take(r.done(stop));
        r.sdk.complete(true);
        r.turn(52, noEvents); // Promotion reaches the expired frame and refuses it.
        CHECK(r.sdk.attempts == 1 && r.tap.count == 0);
        r.turn(53);
        CHECK(r.tap.count == 2);
        const O::TxReport& first = r.tap.log[0];
        const O::TxReport& second = r.tap.log[1];
        CHECK(first.id.sdo.request.channel == 0 && first.transmitted && !first.provedUnsent);
        CHECK(second.id.sdo.request.channel == 1 && !second.transmitted && second.provedUnsent);
        CHECK(held.unchanged(r, stop) && !r.services.unresolved(1));
        CHECK(r.owner.localTxRetired(0) && r.owner.localTxRetired(1));
        CHECK(!r.owner.result(pending));
        CHECK(r.sdk.submits == 1 && r.sdk.attempts == 1);
    }
}

// A later frame proved unsent removes only its own possibility. The earlier frame
// was accepted and may have acted, so the transfer still quarantines.
void laterFrameProvedUnsentKeepsEarlierEffects() {
    const M::Frame initiate = wire(0x601, 0x21, 0x40, 0x60, 0, 8, 0, 0, 0);
    const M::Frame segment = wire(0x601, 0x00, 1, 2, 3, 4, 5, 6, 7);
    // Rejected at the SDK boundary.
    {
        Rig r;
        const auto id = r.requestLarge(1, 1000, 100);
        r.turn(0);
        CHECK(r.sdk.submits == 1 && sameWire(r.sdk.sent[0], initiate));
        r.sdk.complete(true);
        r.turn(1);
        CHECK(r.owner.stats().txFrames == 1);
        r.sdk.push(writeAck());
        r.sdk.submitResult = E::TwaiSubmit::FAILED_UNSENT;
        r.turn(2);
        CHECK(r.sdk.attempts == 2 && r.sdk.submits == 1 && sameWire(r.sdk.attempted[1], segment));
        r.turn(3);
        CHECK(r.tap.count == 2 && r.tap.log[1].provedUnsent && !r.tap.log[1].transmitted);
        const O::Completion& c = r.done(id);
        CHECK(c.service.outcome == C::SdoOutcome::TRANSPORT_ERROR);
        CHECK(c.service.transmissionPossible && c.service.transmissionConfirmed);
        CHECK(c.service.effectPossible && c.service.effectUncertain);
        CHECK(!c.service.protocolAcknowledged && r.services.unresolved(0));
        r.turn(4);
        CHECK(r.sdk.attempts == 2 && r.sdk.submits == 1);
    }
    // Deferred past its queue budget.
    {
        Rig r;
        const auto id = r.requestLarge(1, 1000, 100);
        r.turn(0);
        r.sdk.complete(true);
        r.turn(1);
        r.sdk.push(writeAck());
        r.stage(2);
        r.sdk.push(heartbeatOf(1));
        r.adapter.promote(r.owner, 2);
        CHECK(r.sdk.deferrals == 1 && r.sdk.submits == 1);
        CHECK(sameWire(r.sdk.attempted[1], segment));
        r.turn(101, budget(0));
        CHECK(!r.owner.result(id));
        r.turn(102, budget(0)); // Staged at 2 with a 100 microsecond queue budget.
        const O::Completion& c = r.done(id);
        CHECK(c.service.outcome == C::SdoOutcome::TIMEOUT && c.service.completedUs == 102);
        CHECK(c.service.transmissionPossible && c.service.effectUncertain);
        CHECK(r.services.unresolved(0) && r.sdk.submits == 1);
    }
}

void nmtProvedUnsentEndsWithNoPossibleTransmission() {
    Rig r;
    r.sdk.submitResult = E::TwaiSubmit::FAILED_UNSENT;
    C::NmtRequestId id;
    CHECK(r.owner.admitNmt(nmtWork(r), 0, id));
    r.turn(1);
    CHECK(r.sdk.attempts == 1 && r.sdk.submits == 0);
    CHECK(sameWire(r.sdk.attempted[0], [] {
        M::Frame value = frame(0, 2);
        value.data[0] = 1;
        value.data[1] = 1;
        return value;
    }()));
    CHECK(!r.owner.nmtResult(id));
    r.turn(2);
    CHECK(r.tap.count == 1 && r.tap.log[0].provedUnsent && !r.tap.log[0].transmitted);
    CHECK(r.owner.nmtResult(id)->outcome == C::NmtOutcome::TRANSPORT_ERROR);
    CHECK(!r.owner.nmtResult(id)->transmissionPossible);
    CHECK(!r.owner.nmtResult(id)->transmissionConfirmed);
    r.turn(3);
    CHECK(r.sdk.attempts == 1 && r.sdk.submits == 0); // Never replayed.
    // Slot and frame are retired; nmtProvedUnsentLeavesNoIntent covers what follows.
    CHECK(r.owner.releaseNmt(id));
}

// Conservative ingress bounds pass through unchanged, and a frame stamped after this
// turn's owner cutoff stays queued. It also holds a queued TX back until routed.
void receiveBoundsStayConservativeAndDeferLaterFrames() {
    Rig r;
    const auto id = r.request(1, true, 1000, 100);
    r.sdk.pushBounded(heartbeatOf(1), 10, 40);
    r.turn(39);
    CHECK(r.owner.stats().rxFrames == 0 && !r.services.node(0)->heartbeat.present);
    CHECK(r.sdk.rxWrite - r.sdk.rxRead == 1 && r.sdk.deferrals == 1 && r.sdk.submits == 0);
    CHECK(!r.owner.result(id));
    r.turn(45); // Processed later than its upper bound; no relabeling to 45.
    CHECK(r.owner.stats().rxFrames == 1 && r.sdk.rxRead == r.sdk.rxWrite);
    const C::ObservationTime& seen = r.services.node(0)->nmtTime;
    CHECK(seen.receiveBoundsKnown && !seen.receivedTimeKnown && seen.receivedUs == 0);
    CHECK(seen.receivedEarliestUs == 10 && seen.receivedLatestUs == 40 && seen.processedUs == 45);
    CHECK(r.sdk.submits == 1 && sameWire(r.sdk.sent[0], writeRequest()));
    O::Diagnostic diagnostic;
    bool traced = false;
    while (r.owner.takeDiagnostic(diagnostic))
        if (diagnostic.kind == O::DiagnosticKind::RX) {
            traced = true;
            CHECK(diagnostic.receiveBoundsKnown && !diagnostic.receivedTimeKnown);
            CHECK(diagnostic.receivedEarliestUs == 10 && diagnostic.receivedLatestUs == 40);
            CHECK(diagnostic.processedUs == 45);
        }
    CHECK(traced);
    // Exact cutoff: an upper bound equal to the owner time is routed in that turn.
    Rig exact;
    exact.sdk.pushBounded(heartbeatOf(1), 5, 20);
    exact.turn(19);
    CHECK(exact.owner.stats().rxFrames == 0);
    exact.turn(20);
    CHECK(exact.owner.stats().rxFrames == 1);
    CHECK(exact.services.node(0)->nmtTime.processedUs == 20);
}

void checkNoIntent(const Rig& r, uint8_t slot = 0) {
    const C::NodeSnapshot& node = *r.services.node(slot);
    CHECK(!node.intentPresent && !node.transitionPending && !node.transitionObserved);
    CHECK(!node.transitionExpired && !node.transitionConflict);
}

// D1: a command that provably never left leaves no claim that it did.
void nmtProvedUnsentLeavesNoIntent() {
    // A later matching heartbeat is an ordinary observation, never a confirmation.
    {
        Rig r;
        r.sdk.submitResult = E::TwaiSubmit::FAILED_UNSENT;
        C::NmtRequestId id;
        CHECK(r.owner.admitNmt(nmtWork(r), 0, id));
        r.turn(1);
        checkNoIntent(r);
        r.turn(2);
        CHECK(r.owner.nmtResult(id)->outcome == C::NmtOutcome::TRANSPORT_ERROR);
        CHECK(!r.owner.nmtResult(id)->transmissionPossible);
        checkNoIntent(r);
        r.sdk.push(heartbeatOf(1), true, 3);
        r.turn(3);
        CHECK(r.services.node(0)->nmt == C::NmtState::OPERATIONAL);
        checkNoIntent(r);
        // Its old deadline passes with nothing to expire.
        r.turn(100);
        checkNoIntent(r);
        CHECK(r.sdk.attempts == 1 && r.sdk.submits == 0);
    }
    // New NMT to the node is admitted at once, well before the old deadline of 100.
    {
        Rig r;
        r.sdk.submitResult = E::TwaiSubmit::FAILED_UNSENT;
        C::NmtRequestId id;
        CHECK(r.owner.admitNmt(nmtWork(r), 0, id));
        r.turn(1);
        r.turn(2);
        CHECK(r.owner.nmtResult(id));
        CHECK(r.owner.releaseNmt(id));
        r.sdk.submitResult = E::TwaiSubmit::SENT;
        C::NmtRequestId next;
        CHECK(r.owner.admitNmt(nmtWork(r), 2, next));
        r.turn(3);
        CHECK(r.sdk.submits == 1 && r.sdk.sent[0].id == 0 && r.sdk.attempts == 2);
        CHECK(r.services.node(0)->intentPresent && r.services.node(0)->transitionUs == 3);
    }
    // Broadcast: no node keeps the claim.
    {
        Rig r;
        r.sdk.submitResult = E::TwaiSubmit::FAILED_UNSENT;
        C::NmtTransfer work = nmtWork(r);
        work.broadcast = true;
        C::NmtRequestId id;
        CHECK(r.owner.admitNmt(work, 0, id));
        r.turn(1);
        r.turn(2);
        CHECK(r.sdk.attempts == 1 && r.sdk.attempted[0].data[1] == 0);
        CHECK(!r.owner.nmtResult(id)->transmissionPossible);
        for (uint8_t slot = 0; slot < FIXTURE_NODES; ++slot)
            checkNoIntent(r, slot);
    }
}

// D2: NMT behind an ACTIVE frame has its own hold, like SDO. A frame on node 2 stalls
// the interface; the command targets node 1, so its handoff cannot end that transfer.
struct NmtBehindActive {
    Rig r;
    O::RequestId active;
    C::NmtRequestId id;
    uint32_t generation = 0;
    explicit NmtBehindActive(C::NmtCommand command, uint64_t deadline = 100,
                             uint32_t queueUs = 20) {
        r.sdk.push(heartbeatOf(1), true, 0);
        r.turn(0);
        CHECK(r.services.node(0)->heartbeat.present);
        generation = r.services.node(0)->route.generation;
        r.now = 1;
        active = r.request(2, true);
        r.turn(1);
        CHECK(r.sdk.active && sameWire(r.sdk.sent[0], writeRequest(2)));
        C::NmtTransfer work = nmtWork(r, command, deadline);
        work.queueTimeoutUs = queueUs;
        CHECK(r.owner.admitNmt(work, 2, id));
        r.turn(2);
        // Handed to the adapter, held short of the SDK.
        CHECK(r.sdk.attempts == 1 && !r.owner.nmtResult(id));
        checkNoIntent(r);
    }
};
void nmtQueuedBehindAnActiveFrameNeverClaimsTransmission() {
    // Queue cutoff, then a heartbeat that must not confirm anything.
    {
        NmtBehindActive t(C::NmtCommand::START);
        t.r.turn(21);
        CHECK(!t.r.owner.nmtResult(t.id));
        t.r.turn(22); // Admitted at 2 with a 20 microsecond queue budget.
        const C::NmtCompletion& c = *t.r.owner.nmtResult(t.id);
        CHECK(c.outcome == C::NmtOutcome::TIMEOUT && c.completedUs == 22);
        CHECK(!c.transmissionPossible && !c.transmissionConfirmed);
        checkNoIntent(t.r);
        t.r.sdk.push(heartbeatOf(1), true, 23);
        t.r.turn(23);
        checkNoIntent(t.r);
        CHECK(t.r.sdk.attempts == 1 && t.r.sdk.submits == 1);
    }
    // Overall deadline shorter than the queue budget, at equality.
    {
        NmtBehindActive t(C::NmtCommand::START, 30, 1000);
        t.r.turn(29);
        CHECK(!t.r.owner.nmtResult(t.id));
        t.r.turn(30);
        const C::NmtCompletion& c = *t.r.owner.nmtResult(t.id);
        CHECK(c.outcome == C::NmtOutcome::TIMEOUT && c.completedUs == 30);
        CHECK(!c.transmissionPossible && !c.transmissionConfirmed);
        checkNoIntent(t.r);
        CHECK(t.r.sdk.attempts == 1 && t.r.sdk.submits == 1);
    }
    // Cancel: the frame is retired at once and the slot is immediately reusable.
    {
        NmtBehindActive t(C::NmtCommand::START);
        CHECK(t.r.owner.cancelNmt(t.id, 5));
        const C::NmtCompletion& c = *t.r.owner.nmtResult(t.id);
        CHECK(c.outcome == C::NmtOutcome::CANCELLED);
        CHECK(!c.transmissionPossible && !c.transmissionConfirmed);
        checkNoIntent(t.r);
        CHECK(t.r.owner.releaseNmt(t.id));
        C::NmtRequestId next;
        CHECK(t.r.owner.admitNmt(nmtWork(t.r), 5, next));
        t.r.sdk.complete(true);
        t.r.turn(6);
        CHECK(t.r.sdk.submits == 2 && t.r.sdk.sent[1].id == 0);
        CHECK(t.r.services.node(0)->intentPresent);
    }
    // Evidence withdrawn at handoff stays withdrawn: holding took back the claim of a
    // command, not the host's caution. Reset is the visible case.
    for (unsigned useCancel = 0; useCancel < 2; ++useCancel) {
        NmtBehindActive t(C::NmtCommand::RESET_NODE);
        CHECK(t.r.services.node(0)->route.generation > t.generation);
        CHECK(!t.r.services.node(0)->heartbeat.present);
        const uint32_t withdrawn = t.r.services.node(0)->route.generation;
        if (useCancel)
            CHECK(t.r.owner.cancelNmt(t.id, 21));
        else
            t.r.turn(22);
        CHECK(!t.r.owner.nmtResult(t.id)->transmissionPossible);
        checkNoIntent(t.r);
        CHECK(t.r.services.node(0)->route.generation == withdrawn);
        CHECK(!t.r.services.node(0)->heartbeat.present);
        t.r.turn(23);
        CHECK(t.r.services.node(0)->route.generation == withdrawn);
        CHECK(!t.r.services.node(0)->heartbeat.present);
        CHECK(t.r.sdk.attempts == 1 && t.r.sdk.submits == 1);
    }
    // No event budget at expiry: the same fact arrives later as a proved-unsent report
    // and cannot rewrite the terminal or revive the intent.
    {
        NmtBehindActive t(C::NmtCommand::START);
        const O::ServiceBudget noEvents = budget(8, 2, 0, 2);
        t.r.turn(22, noEvents);
        const C::NmtCompletion& c = *t.r.owner.nmtResult(t.id);
        CHECK(c.outcome == C::NmtOutcome::TIMEOUT && !c.transmissionPossible);
        t.r.sdk.complete(true);
        t.r.turn(23, noEvents);
        CHECK(t.r.sdk.attempts == 1);
        t.r.turn(24);
        CHECK(t.r.tap.count == 2);
        CHECK(t.r.tap.log[0].transmitted && !t.r.tap.log[1].transmitted);
        CHECK(t.r.tap.log[1].provedUnsent);
        CHECK(t.r.tap.log[1].id.kind == O::TransportTxId::Kind::NMT);
        CHECK(c.outcome == C::NmtOutcome::TIMEOUT && c.completedUs == 22);
        CHECK(!c.transmissionPossible);
        checkNoIntent(t.r);
    }
}
// Once the SDK has it, the intent exists, timed from submission and not from handoff.
void nmtIntentReturnsAtSdkSubmission() {
    NmtBehindActive t(C::NmtCommand::START);
    Rig& r = t.r;
    // While held, a matching heartbeat confirms nothing.
    r.sdk.push(heartbeatOf(1), true, 5);
    r.turn(5);
    checkNoIntent(r);
    r.sdk.complete(true);
    r.turn(10); // Node 2's frame finishes; the held NMT frame goes at this time.
    CHECK(r.sdk.submits == 2 && r.sdk.attempts == 2);
    CHECK(r.sdk.sent[1].id == 0 && r.sdk.sent[1].payloadLength == 2);
    CHECK(r.sdk.sent[1].data[0] == 1 && r.sdk.sent[1].data[1] == 1);
    {
        const C::NodeSnapshot& node = *r.services.node(0);
        CHECK(node.intentPresent && node.transitionPending && !node.transitionObserved);
        CHECK(node.requestedCommand == C::NmtCommand::START && node.transitionUs == 10);
        CHECK(node.transitionDeadlineUs == 100);
    }
    CHECK(!r.owner.nmtResult(t.id));
    r.sdk.complete(true);
    // A heartbeat captured no later than submission cannot show the command's effect.
    r.sdk.push(heartbeatOf(1), true, 10);
    r.turn(11);
    CHECK(r.owner.nmtResult(t.id)->outcome == C::NmtOutcome::TRANSMITTED);
    CHECK(r.owner.nmtResult(t.id)->transmissionPossible);
    CHECK(r.owner.nmtResult(t.id)->transmissionConfirmed);
    CHECK(r.services.node(0)->transitionPending && !r.services.node(0)->transitionObserved);
    r.sdk.push(heartbeatOf(1), true, 12);
    r.turn(12);
    CHECK(r.services.node(0)->transitionObserved && !r.services.node(0)->transitionPending);
}
// SDK may have accepted it: the intent stays and a heartbeat can still show it.
void uncertainNmtSubmissionKeepsTheIntent() {
    Rig uncertain;
    uncertain.sdk.submitResult = E::TwaiSubmit::FAILED_UNCERTAIN;
    C::NmtRequestId id;
    CHECK(uncertain.owner.admitNmt(nmtWork(uncertain), 0, id));
    uncertain.turn(1);
    CHECK(uncertain.services.node(0)->intentPresent);
    CHECK(uncertain.services.node(0)->transitionUs == 1);
    uncertain.turn(2);
    CHECK(uncertain.owner.nmtResult(id)->outcome == C::NmtOutcome::TRANSPORT_ERROR);
    CHECK(uncertain.owner.nmtResult(id)->transmissionPossible);
    CHECK(uncertain.services.node(0)->transitionPending);
    uncertain.sdk.push(heartbeatOf(1), true, 3);
    uncertain.turn(3);
    CHECK(uncertain.services.node(0)->transitionObserved);
}

// D3: the hold does not depend on the adapter having seen this turn's time.
void holdNeedsNoPriorPollInTheTurn() {
    // Proved unsent: no possible effect and no quarantine.
    {
        Rig r;
        r.turn(0);
        const auto id = r.request(1, true, 1000, 100);
        CHECK(r.owner.service(5, budget())); // Adapter time is still 0.
        CHECK(r.sdk.attempts == 0);
        r.sdk.submitResult = E::TwaiSubmit::FAILED_UNSENT;
        r.adapter.promote(r.owner, 5);
        CHECK(r.sdk.attempts == 1 && r.sdk.submits == 0);
        r.turn(6);
        CHECK(r.tap.count == 1 && r.tap.log[0].provedUnsent && !r.tap.log[0].transmitted);
        CHECK(r.done(id).service.outcome == C::SdoOutcome::TRANSPORT_ERROR);
        CHECK(r.done(id).service.completedUs == 6);
        checkNeverSent(r.done(id));
        CHECK(!r.services.unresolved(0));
    }
    // Sent later: the response clock still starts at submission.
    {
        Rig r;
        r.turn(0);
        const auto id = r.request(1, true, 1000, 100);
        CHECK(r.owner.service(5, budget()));
        r.adapter.promote(r.owner, 8);
        CHECK(r.sdk.submits == 1 && sameWire(r.sdk.sent[0], writeRequest()));
        r.turn(107);
        CHECK(!r.owner.result(id));
        r.turn(108);
        CHECK(r.done(id).service.outcome == C::SdoOutcome::TIMEOUT);
        CHECK(r.done(id).service.completedUs == 108);
        CHECK(r.done(id).service.effectUncertain && r.services.unresolved(0));
    }
    // The same for NMT.
    {
        Rig r;
        r.turn(0);
        C::NmtRequestId id;
        CHECK(r.owner.admitNmt(nmtWork(r), 1, id));
        CHECK(r.owner.service(5, budget()));
        checkNoIntent(r);
        r.sdk.submitResult = E::TwaiSubmit::FAILED_UNSENT;
        r.adapter.promote(r.owner, 5);
        r.turn(6);
        CHECK(r.owner.nmtResult(id)->outcome == C::NmtOutcome::TRANSPORT_ERROR);
        CHECK(!r.owner.nmtResult(id)->transmissionPossible);
        checkNoIntent(r);
    }
}
} // namespace

int main() {
    startupValidationAndOwnedCapacity();
    delayedImmediateAndFailedCompletion();
    copiedPayloadAndWireFormatsReachTheDriver();
    rejectionUnavailableAndIdentityFault();
    activeAndQueuedRetirement();
    faultsRecoveryAndFailureOwnership();
    frameShapesAndBoundedReceive();
    unreadBootupPreventsAnOldHandoffFromTransmitting();
    rawErrorsRemainDiagnosticAndSaturate();
    backwardClockInvalidatesAtLatestOwnerTime();
    sharedApplicationKeepsServingUnderPressure();
    explicitNmtUsesTheSameTransportRetirement();
    requiredPreOperationalIsRecheckedBeforeSdkSubmit();
    requiredPreOperationalAgeIsRecheckedBeforeSdkSubmit();
    arbitrationLossThenSuccessIsOneSubmission();
    if (PAIRED_CHANNELS)
        noAckStaysActiveUntilRecoveryRetiresBothSlots();
    rxInterruptDuringPromotionDefersWithoutQuarantine();
    sameTickReplyOvertakesTxDoneAndBlocksUrgentForOneTurn();
    if (PAIRED_CHANNELS)
        controllerFailureWhileQueuedAndActive();
    busOffWithAnActiveFrameAndAFinalReport();
    retirementNeverImpliesNotDelivered();
    deferredWithdrawalNeverTouchesTheSdk();
    responseClockStartsAtSdkSubmission();
    if (PAIRED_CHANNELS)
        pendingFrameDoesNotQuarantineAHealthyAxis();
    laterFrameProvedUnsentKeepsEarlierEffects();
    nmtProvedUnsentEndsWithNoPossibleTransmission();
    receiveBoundsStayConservativeAndDeferLaterFrames();
    nmtProvedUnsentLeavesNoIntent();
    if (PAIRED_CHANNELS) {
        nmtQueuedBehindAnActiveFrameNeverClaimsTransmission();
        nmtIntentReturnsAtSdkSubmission();
    }
    uncertainNmtSubmissionKeepsTheIntent();
    holdNeedsNoPriorPollInTheTurn();
    std::printf("TWAI adapter: %u checks; host sizeof(TwaiAdapter)=%zu\n", checks,
                sizeof(E::TwaiAdapter));
}
