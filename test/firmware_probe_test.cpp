// SPDX-License-Identifier: MIT
// Pure firmware assembly tests. Pins, times, peer freshness and replies are
// synthetic fixture facts, with no physical configuration or Q20 claim.
#include "FirmwareProbe.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace M = MotorControlCAN;
namespace C = M::CANOPEN;
namespace L = M::LICHUAN_C;
namespace E = MotorControlCANExample;
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

L::ReadTarget target() {
    L::ReadTarget value;
    value.channel.binding.logicalAxisId = 9;
    value.channel.binding.endpoint.profile = 1;
    value.channel.binding.endpoint.address = 5;
    value.channel.binding.bindingGeneration = 1;
    value.channel.binding.configurationGeneration = 1;
    value.channel.clientToServerId = 0x605;
    value.channel.serverToClientId = 0x585;
    value.nodeGeneration = 1;
    return value;
}
E::FirmwareConfig config() {
    E::FirmwareConfig value;
    value.configured = true;
    value.controller.txPin = 4;
    value.controller.rxPin = 5;
    value.controller.bitrate = 250000;
    value.controller.sdkTxQueueDepth = 1;
    value.controller.rxQueueDepth = 16;
    return value;
}
L::ReadOptions options() {
    L::ReadOptions value;
    value.queueTimeoutUs = 100;
    value.responseTimeoutUs = 100;
    return value;
}
M::Frame frame(uint32_t id, uint8_t count) {
    M::Frame value;
    value.id = id;
    value.dlc = value.payloadLength = count;
    return value;
}
struct Bytes {
    uint64_t now = 0;
    bool clockOk = true, blocked = false;
    unsigned clocks = 0, reads = 0, writes = 0;
    unsigned remaining = 0;
    static bool clock(void* context, uint64_t& output) {
        Bytes& b = *static_cast<Bytes*>(context);
        ++b.clocks;
        output = b.clockOk ? b.now : UINT64_MAX;
        return b.clockOk;
    }
    static bool read(void* context, uint8_t& output) {
        Bytes& b = *static_cast<Bytes*>(context);
        ++b.reads;
        if (!b.remaining)
            return false;
        output = static_cast<uint8_t>(--b.remaining);
        return true;
    }
    static bool write(void* context, uint8_t) {
        Bytes& b = *static_cast<Bytes*>(context);
        ++b.writes;
        return !b.blocked;
    }
    E::ProbeCallbacks callbacks() {
        E::ProbeCallbacks value;
        value.context = this;
        value.nowUs = clock;
        value.readByte = read;
        value.writeByte = write;
        return value;
    }
};
struct Driver {
    E::TwaiEvents pending;
    E::ReceivedFrame incoming[16];
    M::Frame sent[16];
    unsigned starts = 0, stops = 0, submits = 0, receives = 0, eventCalls = 0;
    unsigned head = 0, count = 0;
    uint32_t epoch = 0, serial = 0;
    bool startOk = true, stopOk = true, live = false, active = false;
    static bool start(void* context, const E::TwaiConfig&, uint32_t epoch) {
        Driver& d = *static_cast<Driver*>(context);
        ++d.starts;
        d.epoch = epoch;
        d.live = d.startOk;
        return d.startOk;
    }
    static bool stop(void* context) {
        Driver& d = *static_cast<Driver*>(context);
        ++d.stops;
        if (!d.stopOk)
            return false;
        d.live = d.active = false;
        d.count = d.head = 0;
        d.pending = E::TwaiEvents();
        return true;
    }
    static bool submit(void* context, const M::Frame& value, uint32_t serial) {
        Driver& d = *static_cast<Driver*>(context);
        CHECK(d.live && !d.active && d.submits < 16);
        d.sent[d.submits++] = value;
        d.active = true;
        d.serial = serial;
        return true;
    }
    static E::TwaiEvents events(void* context) {
        Driver& d = *static_cast<Driver*>(context);
        ++d.eventCalls;
        E::TwaiEvents copy = d.pending;
        copy.rxPending = d.count != 0;
        d.pending = E::TwaiEvents();
        return copy;
    }
    static bool receive(void* context, E::ReceivedFrame& output, uint64_t cutoffUs) {
        Driver& d = *static_cast<Driver*>(context);
        ++d.receives;
        if (!d.count || (d.incoming[d.head].receiveBoundsKnown &&
                         d.incoming[d.head].receivedLatestUs > cutoffUs))
            return false;
        output = d.incoming[d.head];
        d.head = (d.head + 1) % 16;
        --d.count;
        return true;
    }
    E::TwaiDriver callbacks() {
        E::TwaiDriver value;
        value.context = this;
        value.start = start;
        value.stop = stop;
        value.submit = submit;
        value.events = events;
        value.receive = receive;
        return value;
    }
    void push(const M::Frame& value) {
        CHECK(count < 16);
        E::ReceivedFrame& next = incoming[(head + count++) % 16];
        next.frame = value;
        next.controllerEpoch = epoch;
        next.receivedTimeKnown = false;
    }
    void passive() {
        M::Frame heartbeat = frame(0x705, 1);
        heartbeat.data[0] = 5;
        push(heartbeat);
        M::Frame emcy = frame(0x85, 8);
        emcy.data[0] = 0x10;
        emcy.data[1] = 0x23;
        emcy.data[2] = 1;
        push(emcy);
    }
    void reply() {
        CHECK(active && !pending.txDone);
        const M::Frame& request = sent[submits - 1];
        CHECK(request.id == 0x605 && request.data[0] == 0x40);
        const uint16_t index = uint16_t(request.data[1]) | uint16_t(request.data[2]) << 8;
        unsigned width = 4;
        uint32_t raw = 0;
        // Independent configuration vectors also used by the native probe fixture.
        if (index == 0x1005)
            raw = 0x80;
        else if (index == 0x1014)
            raw = 0x85;
        else if (index == 0x1017) {
            width = 2;
            raw = 250;
        } else {
            CHECK(index == 0x1200 && request.data[3] <= 2);
            if (request.data[3] == 0) {
                width = 1;
                raw = 2;
            } else
                raw = request.data[3] == 1 ? 0x605 : 0x585;
        }
        M::Frame response = frame(0x585, 8);
        response.data[0] = width == 1 ? 0x4f : width == 2 ? 0x4b : 0x43;
        std::memcpy(response.data + 1, request.data + 1, 3);
        for (unsigned i = 0; i < 4; ++i)
            response.data[4 + i] = static_cast<uint8_t>(raw >> (8 * i));
        passive();
        push(response);
        pending.txDone = true;
        pending.transmitted = true;
        pending.serial = serial;
        active = false;
    }
};
struct Rig {
    C::SdoClient services;
    Driver driver;
    Bytes bytes;
    E::FirmwareProbe firmware;
    Rig() : firmware(services, driver.callbacks(), bytes.callbacks()) {
        const L::ReadTarget selected = target();
        // Test-only fresh peer premise; startup never discovers or grants this.
        CHECK(services.configure(0, selected.channel, true, true));
        C::NetworkRoute network;
        network.binding = selected.channel.binding;
        network.generation = selected.nodeGeneration;
        network.heartbeatId = 0x705;
        network.emcyId = 0x85;
        CHECK(services.configureNetwork(0, network, C::NetworkObserver(), 0));
    }
    void turn(uint64_t now) {
        bytes.now = now;
        const unsigned reads = bytes.reads, writes = bytes.writes;
        CHECK(firmware.service());
        CHECK(bytes.reads - reads <= E::ProbePlatform::INPUT_BUDGET);
        CHECK(bytes.writes - writes <= E::ProbePlatform::OUTPUT_BUDGET);
    }
};
void startup() {
    Rig untouched;
    CHECK(untouched.driver.starts == 0 && untouched.driver.submits == 0);
    CHECK(untouched.bytes.clocks == 0 && !untouched.firmware.app().selected());
    CHECK(!untouched.firmware.service() && untouched.bytes.clocks == 0);
    CHECK(!untouched.firmware.begin(E::FirmwareConfig()));
    CHECK(untouched.driver.starts == 0 && !untouched.firmware.begin(config()));
    for (unsigned pin = 19; pin <= 20; ++pin) {
        for (unsigned tx = 0; tx < 2; ++tx) {
            Rig rejected;
            E::FirmwareConfig cfg = config();
            if (tx)
                cfg.controller.txPin = pin;
            else
                cfg.controller.rxPin = pin;
            CHECK(!rejected.firmware.begin(cfg));
            CHECK(rejected.driver.starts == 0 && rejected.driver.submits == 0);
        }
    }
    Rig failed;
    failed.driver.startOk = false;
    CHECK(!failed.firmware.begin(config()));
    CHECK(!failed.firmware.begin(config()) && failed.driver.starts == 1);
    CHECK(!failed.firmware.service() && failed.driver.submits == 0);
    Rig valid;
    CHECK(valid.firmware.begin(config()));
    CHECK(valid.firmware.app().select(target()));
    valid.turn(0);
    valid.turn(0);
    CHECK(valid.firmware.running() && !valid.firmware.clockFailed());
    CHECK(valid.driver.starts == 1 && valid.driver.submits == 0);
}
void clockBoundariesAndRetirement() {
    Rig wide;
    CHECK(wide.firmware.begin(config()));
    wide.turn(0);
    wide.turn(UINT32_MAX);
    wide.turn(UINT64_C(4294967296));
    wide.turn(UINT64_C(4294967296));
    CHECK(!wide.firmware.clockFailed() && wide.driver.submits == 0);
    for (unsigned variant = 0; variant < 4; ++variant) {
        Rig r;
        CHECK(r.firmware.begin(config()) && r.firmware.app().select(target()));
        CHECK(r.firmware.app().requestConfig(0, 1000, options()));
        r.turn(0);
        r.turn(1);
        CHECK(r.driver.submits == 1 && r.driver.active);
        const E::RequestId id = r.firmware.app().pendingRequest();
        r.driver.stopOk = variant < 2;
        r.bytes.clockOk = (variant % 2) == 0;
        r.bytes.now = 0;
        CHECK(!r.firmware.service());
        CHECK(!r.firmware.running() && r.firmware.clockFailed());
        CHECK(r.firmware.owner().resources().inhibited && r.driver.stops == 1);
        CHECK(r.firmware.faultRetired() == r.driver.stopOk);
        CHECK(r.firmware.transport().diagnostics().inhibited);
        CHECK(r.driver.active == !r.driver.stopOk);
        const E::Completion* terminal = r.firmware.owner().result(id);
        CHECK(terminal && terminal->service.outcome != C::SdoOutcome::SUCCESS);
        const unsigned clocks = r.bytes.clocks, receives = r.driver.receives;
        r.bytes.clockOk = true;
        r.bytes.now = 2;
        for (unsigned i = 0; i < 4; ++i)
            CHECK(!r.firmware.service());
        CHECK(r.driver.submits == 1 && r.driver.stops == 1);
        CHECK(r.bytes.clocks == clocks && r.driver.receives == receives);
        CHECK(r.firmware.owner().result(id) == terminal);
        CHECK(!r.firmware.begin(config()));
    }
    // An explicit public owner call may advance beyond the last firmware turn.
    // A failing clock must invalidate at that actual latest boundary.
    Rig advanced;
    CHECK(advanced.firmware.begin(config()));
    advanced.turn(100);
    CHECK(advanced.firmware.owner().reportController(C::ControllerState::AVAILABLE, 200));
    CHECK(advanced.services.controllerState() == C::ControllerState::AVAILABLE);
    const uint32_t generation = advanced.services.node(0)->route.generation;
    advanced.bytes.clockOk = false;
    CHECK(!advanced.firmware.service());
    CHECK(advanced.firmware.owner().lastTimeUs() == 200);
    CHECK(advanced.services.controllerState() == C::ControllerState::RECOVERING);
    CHECK(advanced.services.node(0)->route.generation > generation);
    CHECK(advanced.services.node(0)->boundaryUs == 200);
    CHECK(advanced.firmware.clockFailed() && advanced.firmware.faultRetired());
    CHECK(advanced.firmware.owner().resources().inhibited && advanced.driver.stops == 1);
    CHECK(advanced.driver.submits == 0);
}
void typedReadUnderPressure() {
    Rig r;
    CHECK(r.firmware.begin(config()) && r.firmware.app().select(target()));
    r.bytes.blocked = true;
    r.bytes.remaining = 160;
    uint8_t output[E::ProbePlatform::OUTPUT_CAPACITY] = {};
    CHECK(r.firmware.platform().writeOutput(output, sizeof output));
    CHECK(!r.firmware.platform().writeOutput(output, 1));
    CHECK(r.firmware.app().requestConfig(0, 1000, options()));
    unsigned answered = 0, advanced = 0;
    uint64_t now = 0;
    while (r.firmware.app().readContext().state == M::ReadState::ACTIVE && now < 100) {
        r.turn(now++);
        if (r.driver.submits != answered) {
            CHECK(r.driver.submits == answered + 1);
            r.driver.reply();
            ++answered;
        }
        const E::RequestId id = r.firmware.app().pendingRequest();
        const E::Completion* terminal = r.firmware.owner().result(id);
        if (!terminal)
            continue;
        CHECK(terminal->service.outcome == C::SdoOutcome::SUCCESS);
        unsigned char copy[sizeof(E::Completion)];
        std::memcpy(copy, terminal, sizeof copy);
        CHECK(!r.firmware.app().releaseRead());
        const unsigned submitted = r.driver.submits;
        r.turn(now++);
        CHECK(r.firmware.owner().result(id) == terminal);
        CHECK(std::memcmp(copy, terminal, sizeof copy) == 0);
        CHECK(r.driver.submits == submitted);
        CHECK(r.firmware.app().advanceRead(now - 1));
        CHECK(!r.firmware.owner().result(id));
        ++advanced;
    }
    CHECK(answered == 6 && advanced == 6);
    CHECK(r.firmware.app().readContext().state == M::ReadState::SUCCEEDED);
    L::ConfigObservation observed;
    CHECK(r.firmware.app().getConfig(now, 1000, observed));
    CHECK(observed.syncCobId == 0x80 && observed.emcyCobId == 0x85 &&
          observed.producerTimeMs == 250 && observed.sdoEntries == 2 &&
          observed.clientToServer == 0x605 && observed.serverToClient == 0x585);
    CHECK(r.services.node(0)->heartbeat.present && r.services.node(0)->emcyCount == 6);
    CHECK(r.services.node(0)->emcy.errorCode == 0x2310);
    CHECK(r.firmware.owner().stats().rxFrames == 18);
    const E::ProbeIoStats pressure = r.firmware.platform().stats();
    CHECK(pressure.inputUsed == E::ProbePlatform::INPUT_CAPACITY && pressure.inputDropped > 0);
    CHECK(pressure.outputUsed == sizeof output && pressure.outputRejected == 1);
    CHECK(r.firmware.owner().resources().retained == 0);
    CHECK(r.firmware.app().releaseRead());
    CHECK(!r.firmware.app().getConfig(now, 1000, observed));
    CHECK(observed.producerTimeMs == 250);
}
void saturationAndExplicitRecovery() {
    Rig r;
    CHECK(r.firmware.begin(config()));
    E::SequenceId sequence;
    CHECK(r.firmware.owner().beginSequence(target().channel.binding, 1, 0, sequence));
    CHECK(r.firmware.owner().beginOperation(sequence, 1, 1000, 0));
    E::BusRequest work;
    work.sequence = sequence;
    work.transfer.channel = target().channel;
    work.transfer.operation = 1;
    work.transfer.index = 0x1005;
    work.transfer.size = 4;
    work.transfer.readEffect = C::SdoReadEffect::REVIEWED_NON_CHANGING;
    work.transfer.queueTimeoutUs = work.transfer.responseTimeoutUs = 100;
    work.transfer.deadlineUs = 1000;
    E::RequestId ids[E::CanBusOwner::REQUESTS];
    for (unsigned i = 0; i < E::CanBusOwner::REQUESTS; ++i) {
        work.transfer.request = work.transfer.step = i + 1;
        CHECK(r.firmware.owner().admit(work, 0, ids[i]) == E::BusAdmission::ACCEPTED);
    }
    E::RequestId rejected;
    ++work.transfer.request;
    ++work.transfer.step;
    CHECK(r.firmware.owner().admit(work, 0, rejected) == E::BusAdmission::RESULTS_FULL);
    CHECK(!rejected.owner && r.driver.submits == 0);
    r.turn(0);
    CHECK(r.driver.submits == 1);
    r.driver.pending.busOff = true;
    r.turn(1);
    CHECK(r.firmware.owner().resources().inhibited);
    CHECK(r.firmware.owner().resources().retained == E::CanBusOwner::REQUESTS);
    for (unsigned i = 0; i < E::CanBusOwner::REQUESTS; ++i)
        CHECK(r.firmware.owner().result(ids[i]));
    r.turn(2);
    CHECK(r.driver.starts == 1 && r.driver.submits == 1);
    E::RecoveryId recovery;
    CHECK(r.firmware.owner().recover(3, 100, recovery));
    r.turn(3);
    CHECK(r.driver.stops == 1 && !r.firmware.owner().recoveryResult(recovery));
    r.turn(4);
    const E::RecoveryCompletion* result = r.firmware.owner().recoveryResult(recovery);
    CHECK(result && result->outcome == E::RecoveryOutcome::RETIRED);
    CHECK(r.driver.starts == 2 && r.driver.epoch == 2);
    r.turn(5);
    CHECK(r.driver.submits == 1 && !r.driver.active);
    CHECK(!r.firmware.transport().diagnostics().inhibited);
    for (unsigned i = 0; i < E::CanBusOwner::REQUESTS; ++i) {
        CHECK(r.firmware.owner().result(ids[i]));
        CHECK(r.firmware.owner().release(ids[i]));
    }
    CHECK(r.firmware.owner().releaseRecovery(recovery));
}
E::ReadExperiment syntheticEvidenceReview() {
    E::ReadExperiment review;
    review.plan.id = 1;
    std::strcpy(review.plan.fixtureId, "firmware-host-driver-fake-only");
    std::strcpy(review.plan.softwareRevision, "0123456789abcdef0123456789abcdef01234567");
    std::strcpy(review.plan.firmware, "synthetic test replies; no running device");
    std::strcpy(review.plan.dialect, "raw unresolved");
    std::strcpy(review.plan.authorization, "host test only; no actual bench permission");
    std::strcpy(review.plan.observations,
                "scripted frames and retained terminals; no physical observations");
    review.plan.target = target();
    review.plan.kind = L::ReadKind::CONFIG;
    review.plan.options = options();
    review.plan.deadlineUs = 1000;
    std::strcpy(review.sourceReview, "synthetic configuration words only; Q20 remains unresolved");
    std::strcpy(review.independentReview,
                "host test fixture assertions; no physical review claimed");
    std::strcpy(review.fixtureReview, "Driver and Bytes fakes; no physical controller");
    std::strcpy(review.independentControl,
                "no device; finite supplied clock and bounded caller loop");
    std::strcpy(review.failurePlan, "retain terminal and raw prefix; no retries or new work");
    std::strcpy(review.reconciliation,
                "inspect unknown effects; no physical reconciliation claimed");
    return review;
}
void firmwareEvidenceAdmissionAndCollection() {
    Rig r;
    CHECK(!r.firmware.evidence().record() && r.driver.submits == 0);
    CHECK(r.firmware.begin(config()) && r.firmware.app().select(target()));
    r.turn(0);
    const E::ReadExperiment review = syntheticEvidenceReview();
    CHECK(r.firmware.evidence().request(review.plan, 0) ==
          E::EvidenceAdmission::READ_EFFECT_UNQUALIFIED);
    CHECK(!r.firmware.evidence().record() && r.driver.submits == 0);
    CHECK(r.firmware.app().readContext().state == M::ReadState::EMPTY);
    CHECK(r.firmware.evidence().requestExperiment(review, 0) == E::EvidenceAdmission::ACCEPTED);
    CHECK(r.driver.submits == 0); // Admission is not transmission.
    unsigned answered = 0;
    for (uint64_t now = 1; now < 100 && !r.firmware.evidence().record()->terminal; ++now) {
        r.turn(now); // Firmware alone services the owner and collects each terminal.
        if (r.driver.submits != answered) {
            CHECK(r.driver.submits == answered + 1);
            r.driver.reply();
            ++answered;
        }
    }
    const E::AcquisitionRecord& record = *r.firmware.evidence().record();
    CHECK(record.environment == E::EvidenceEnvironment::DEVICE && record.experimental);
    CHECK(record.terminal && record.read.state == M::ReadState::SUCCEEDED);
    CHECK(record.intendedCount == 6 && record.terminalCount == 6 && answered == 6);
    CHECK(!record.evidenceLost && record.trafficCount > 0);
    CHECK(record.initialStats.txFrames == 0 && record.finalStats.txFrames > 0 &&
          record.finalStats.txFrames <= answered);
    // The last adapter report may follow SDO delivery; counters record only the
    // reports actually consumed by the owner before this terminal snapshot.
    CHECK(record.initialStats.rxFrames == 0 && record.finalStats.rxFrames == 18);
    CHECK(record.initialResources.reservations == 1 && record.finalResources.retained == 0);
    for (unsigned i = 0; i < record.terminalCount; ++i) {
        CHECK(record.terminals[i].service.outcome == C::SdoOutcome::SUCCESS);
        CHECK(record.terminals[i].service.replyValid);
        CHECK(record.terminals[i].service.effectPossible);
    }
    CHECK(r.firmware.owner().resources().retained == 0);
    CHECK(r.firmware.evidence().release());
}
void firmwareEvidenceFailuresNeverReplay() {
    for (unsigned variant = 0; variant < 2; ++variant) {
        Rig r;
        CHECK(r.firmware.begin(config()) && r.firmware.app().select(target()));
        const E::ReadExperiment review = syntheticEvidenceReview();
        CHECK(r.firmware.evidence().requestExperiment(review, 0) == E::EvidenceAdmission::ACCEPTED);
        r.turn(0);
        r.turn(1);
        CHECK(r.driver.submits == 1 && r.driver.active);
        if (variant == 0) {
            // Confirm only controller TX; no SDO response is synthesized.
            r.driver.pending.txDone = r.driver.pending.transmitted = true;
            r.driver.pending.serial = r.driver.serial;
            r.driver.active = false;
            for (uint64_t now = 2; now < 150 && !r.firmware.evidence().record()->terminal; ++now)
                r.turn(now);
        } else {
            r.bytes.clockOk = false;
            CHECK(!r.firmware.service());
            CHECK(r.firmware.clockFailed() && r.firmware.faultRetired());
        }
        const E::AcquisitionRecord& record = *r.firmware.evidence().record();
        CHECK(record.environment == E::EvidenceEnvironment::DEVICE && record.experimental);
        CHECK(record.terminal && record.read.state == M::ReadState::FAILED &&
              record.read.uncertain);
        CHECK(record.terminalCount == 1 && record.terminals[0].service.effectUncertain);
        CHECK(!record.terminals[0].service.protocolAcknowledged && !record.evidenceLost);
        if (variant == 0)
            CHECK(record.terminals[0].service.outcome == C::SdoOutcome::TIMEOUT);
        else {
            CHECK(record.interrupted);
            bool controllerRecorded = false;
            for (unsigned i = 0; i < record.trafficCount; ++i)
                controllerRecorded |=
                    record.traffic[i].kind == E::DiagnosticKind::CONTROLLER &&
                    record.traffic[i].controller == C::ControllerState::RECOVERING;
            CHECK(controllerRecorded);
        }
        const uint64_t completedUs = record.completedUs;
        for (uint64_t now = 200; now < 204; ++now) {
            r.bytes.now = now;
            CHECK(r.firmware.service() == (variant == 0));
        }
        CHECK(r.driver.submits == 1 && r.firmware.evidence().record()->terminalCount == 1);
        CHECK(r.firmware.evidence().record()->completedUs == completedUs);
        CHECK(r.firmware.evidence().record()->read.uncertain);
        CHECK(r.firmware.evidence().release());
    }
}
} // namespace
int main() {
    startup();
    clockBoundariesAndRetirement();
    typedReadUnderPressure();
    saturationAndExplicitRecovery();
    firmwareEvidenceAdmissionAndCollection();
    firmwareEvidenceFailuresNeverReplay();
    std::printf("firmware_probe_test: %u checks; sizeof FirmwareProbe=%zu, CanBusOwner=%zu, "
                "TwaiAdapter=%zu, ProbeApp=%zu, ProbePlatform=%zu, SdoClient=%zu\n",
                checks, sizeof(E::FirmwareProbe), sizeof(E::CanBusOwner), sizeof(E::TwaiAdapter),
                sizeof(E::ProbeApp), sizeof(E::ProbePlatform), sizeof(C::SdoClient));
}
