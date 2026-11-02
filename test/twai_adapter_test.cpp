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
    enum { RX_CAPACITY = 16, HISTORY = 16 };
    E::TwaiEvents pending;
    O::ReceivedFrame rx[RX_CAPACITY];
    M::Frame sent[HISTORY];
    uint32_t serials[HISTORY] = {};
    unsigned starts = 0, stops = 0, submits = 0, eventCalls = 0, receiveCalls = 0;
    unsigned rxRead = 0, rxWrite = 0;
    uint32_t epoch = 0, activeSerial = 0;
    bool startOk = true, stopOk = true, submitOk = true, immediate = false;
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
    static bool submit(void* p, const M::Frame& value, uint32_t serial) {
        FakeDriver& f = *static_cast<FakeDriver*>(p);
        CHECK(f.live && !f.active && f.submits < HISTORY);
        // Rejection retains neither the call-only input nor a future callback.
        if (!f.submitOk)
            return false;
        f.sent[f.submits] = value;
        f.serials[f.submits++] = serial;
        f.active = true;
        f.activeSerial = serial;
        if (f.immediate)
            f.complete(true);
        return true;
    }
    static E::TwaiEvents events(void* p) {
        FakeDriver& f = *static_cast<FakeDriver*>(p);
        ++f.eventCalls;
        E::TwaiEvents copy = f.pending;
        copy.rxPending = f.rxRead != f.rxWrite;
        f.pending = E::TwaiEvents();
        return copy;
    }
    static bool receive(void* p, O::ReceivedFrame& output, uint64_t) {
        FakeDriver& f = *static_cast<FakeDriver*>(p);
        ++f.receiveCalls;
        if (f.rxRead == f.rxWrite || f.pending.busOff || f.pending.fault || f.pending.rxDropped)
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
    }
};
struct Rig {
    C::SdoClient services;
    FakeDriver sdk;
    E::TwaiAdapter adapter;
    O::CanBusOwner owner;
    uint64_t now = 0;
    explicit Rig(bool start = true)
        : adapter(services, sdk.driver()), owner(services, adapter.transport(), 127) {
        for (uint8_t i = 0; i < 2; ++i) {
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
        CHECK(owner.service(now, b));
        adapter.service(owner, now);
    }
    O::RequestId request(unsigned node = 1, bool download = false, uint64_t deadline = 1000,
                         uint32_t queueUs = 100, bool requirePreOperational = false) {
        O::SequenceId sequence;
        CHECK(owner.beginSequence(route(node).binding, 1, now, sequence));
        CHECK(owner.beginOperation(sequence, 1, deadline, now));
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
    CHECK(!t.transmit(t.context, original, arbitraryId(r.services, 3), 1));
    // Unqualified direct IDs reach negative reports, never the SDK. Reports
    // retain the same two capacity slots until explicitly consumed.
    CHECK(r.owner.service(1, O::ServiceBudget()));
    r.adapter.service(r.owner, 1);
    CHECK(!t.transmit(t.context, original, arbitraryId(r.services, 3), 1));
    O::TxReport report;
    CHECK(t.takeTxReport(t.context, report) && !report.transmitted);
    CHECK(report.id.sdo.request.generation == 1);
    CHECK(t.transmit(t.context, original, arbitraryId(r.services, 3), 1));
    CHECK(r.sdk.submits == 0);
    CHECK(r.adapter.shutdown());
}

void delayedImmediateAndFailedCompletion() {
    for (unsigned immediate = 0; immediate < 2; ++immediate) {
        Rig r;
        r.sdk.immediate = immediate != 0;
        const O::RequestId id = r.request(1, true);
        r.turn(0);
        CHECK(r.sdk.submits == 1 && !r.owner.result(id));
        r.turn(1);
        CHECK(r.sdk.submits == 1 && r.owner.stats().txFrames == 0);
        CHECK(r.sdk.sent[0].data[4] == 0x34 && r.sdk.sent[0].data[5] == 0x12);
        if (!immediate) {
            r.turn(2);
            CHECK(r.owner.stats().txFrames == 0 && !r.owner.result(id));
            r.sdk.complete(true);
        }
        r.turn(3);
        // The adapter copied the callback after the owner turn. The next owner
        // turn consumes that report; enqueue alone never increments txFrames.
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
    failed.sdk.complete(false);
    failed.turn(2);
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
        r.adapter.service(r.owner, 1);
        CHECK(r.sdk.submits == 1 && r.sdk.sent[0].id == expected.id);
        CHECK(r.sdk.sent[0].extended == expected.extended && r.sdk.sent[0].rtr == expected.rtr);
        CHECK(r.sdk.sent[0].dlc == 8 && r.sdk.sent[0].payloadLength == expected.payloadLength);
        if (!expected.rtr)
            CHECK(!std::memcmp(r.sdk.sent[0].data, expected.data, 8));
    }
}

void rejectionUnavailableAndIdentityFault() {
    Rig rejected;
    rejected.sdk.submitOk = false;
    const auto id = rejected.request(1, true);
    rejected.turn(0);
    rejected.turn(1);
    CHECK(rejected.sdk.submits == 0 && !rejected.sdk.active);
    CHECK(rejected.done(id).service.outcome == C::SdoOutcome::TRANSPORT_ERROR);
    rejected.turn(2);
    CHECK(rejected.sdk.submits == 0);

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
        r.request(2, true);
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
    r.adapter.service(r.owner, 1);
    CHECK(r.sdk.submits == 0 && !r.owner.result(id));
    CHECK(r.owner.service(2, budget(1, 0, 8, 0)));
    r.adapter.service(r.owner, 2);
    CHECK(r.sdk.submits == 0 && r.owner.result(id));
    CHECK(r.done(id).service.outcome == C::SdoOutcome::REBOOT);
    CHECK(r.owner.service(3, budget()));
    r.adapter.service(r.owner, 3);
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
    r.adapter.service(r.owner, 99);
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
        r.adapter.service(r.owner, bad);
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
    CHECK(app.service(0));
    r.adapter.service(r.owner, 0);
    CHECK(r.sdk.submits == 0 && console.reads <= E::ProbePlatform::INPUT_BUDGET);
    M::LICHUAN_C::ReadOptions options;
    options.queueTimeoutUs = options.responseTimeoutUs = 100;
    CHECK(app.requestIdentity(0, 1000, options));
    for (uint64_t now = 1; now <= 4; ++now) {
        M::Frame heartbeat = frame(0x701, 1);
        heartbeat.data[0] = 5;
        r.sdk.push(heartbeat);
        const unsigned before = console.reads;
        CHECK(app.service(now));
        r.adapter.service(r.owner, now);
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
    r.adapter.service(r.owner, 1); // Captured unread RX defers the SDK boundary.
    CHECK(r.sdk.submits == 0);
    r.turn(2); // Sole owner now observes Operational; native handoff becomes ineligible.
    CHECK(r.services.node(0)->nmt == C::NmtState::OPERATIONAL && r.sdk.submits == 0);
    r.turn(3);
    CHECK(r.sdk.submits == 0);
    r.turn(101); // Original response deadline remains finite; no replay/requalification.
    CHECK(r.owner.result(id) && r.owner.result(id)->service.outcome == C::SdoOutcome::TIMEOUT);
    CHECK(r.owner.result(id)->service.effectUncertain && r.sdk.submits == 0);
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
    r.adapter.service(r.owner, 11); // SDK boundary is later; stored NMT premise has expired.
    CHECK(r.sdk.submits == 0);
    r.turn(12);
    CHECK(r.sdk.submits == 0);
    r.turn(109);
    CHECK(r.owner.result(id) && r.owner.result(id)->service.outcome == C::SdoOutcome::TIMEOUT);
    CHECK(r.owner.result(id)->service.effectUncertain && r.sdk.submits == 0);
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
        r.adapter.service(r.owner, 2);
        CHECK(r.sdk.submits == 0 && r.owner.nmtResult(id)->outcome == C::NmtOutcome::CANCELLED);
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
        r.sdk.submitOk = false;
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
    std::printf("TWAI adapter: %u checks; host sizeof(TwaiAdapter)=%zu\n", checks,
                sizeof(E::TwaiAdapter));
}
