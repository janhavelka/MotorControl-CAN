// SPDX-License-Identifier: MIT
// Pure firmware assembly tests. Pins, times, peer freshness and replies are
// synthetic fixture facts, with no physical configuration or Q20 claim.
#include "FirmwareProbe.h"
#include "FirmwareSmoke.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace M = MotorControlCAN;
namespace C = M::CANOPEN;
namespace L = M::LICHUAN_C;
namespace E = MotorControlCANExample;
namespace O = MotorControlCANOwner;
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
    const char* script = nullptr; // Scripted console input, consumed after `remaining`.
    char out[4096] = {};          // Accepted console output, NUL-terminated.
    size_t used = 0;
    static bool clock(void* context, uint64_t& output) {
        Bytes& b = *static_cast<Bytes*>(context);
        ++b.clocks;
        output = b.clockOk ? b.now : UINT64_MAX;
        return b.clockOk;
    }
    static bool read(void* context, uint8_t& output) {
        Bytes& b = *static_cast<Bytes*>(context);
        ++b.reads;
        if (b.remaining) {
            output = static_cast<uint8_t>(--b.remaining);
            return true;
        }
        if (!b.script || !*b.script)
            return false;
        output = static_cast<uint8_t>(*b.script++);
        return true;
    }
    static bool write(void* context, uint8_t byte) {
        Bytes& b = *static_cast<Bytes*>(context);
        ++b.writes;
        if (b.blocked)
            return false;
        if (b.used + 1 < sizeof b.out)
            b.out[b.used++] = static_cast<char>(byte);
        return true;
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
    O::ReceivedFrame incoming[16];
    M::Frame sent[32];
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
    static E::TwaiSubmit submit(void* context, const M::Frame& value, uint32_t serial) {
        Driver& d = *static_cast<Driver*>(context);
        CHECK(d.live && !d.active && d.submits < 32);
        // Same rule as IdfTwaiDriver::submit: captured RX defers with no SDK contact.
        if (d.count)
            return E::TwaiSubmit::DEFERRED;
        d.sent[d.submits++] = value;
        d.active = true;
        d.serial = serial;
        return E::TwaiSubmit::SENT;
    }
    static E::TwaiEvents events(void* context) {
        Driver& d = *static_cast<Driver*>(context);
        ++d.eventCalls;
        E::TwaiEvents copy = d.pending;
        copy.rxPending = d.count != 0;
        d.pending = E::TwaiEvents();
        return copy;
    }
    static bool receive(void* context, O::ReceivedFrame& output, uint64_t cutoffUs) {
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
        O::ReceivedFrame& next = incoming[(head + count++) % 16];
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
// Application GPIO fake. Each call records pin, level and the controller calls before it,
// so tests prove ordering against the driver (silence before stop, start before normal).
struct Pin {
    struct Call {
        int gpio;
        bool high;
        unsigned starts, stops;
    } calls[16];
    unsigned count = 0, failAt = 0; // failAt: 1-based call number that the SDK refuses.
    unsigned failMask = 0;          // Bit n-1 set: call n is refused too.
    const Driver* driver = nullptr;
    static bool drive(void* context, int gpio, bool high) {
        Pin& p = *static_cast<Pin*>(context);
        CHECK(p.count < 16);
        p.calls[p.count] = {gpio, high, p.driver->starts, p.driver->stops};
        const unsigned call = ++p.count;
        return call != p.failAt && ((p.failMask >> (call - 1)) & 1u) == 0;
    }
    E::SilentPin callbacks() {
        E::SilentPin value;
        value.context = this;
        value.drive = drive;
        return value;
    }
};
struct Rig {
    C::SdoClient services;
    Driver driver;
    Bytes bytes;
    Pin pin;
    E::FirmwareProbe firmware;
    explicit Rig(bool silentPin = false, bool route = true)
        : firmware(services, driver.callbacks(), bytes.callbacks(),
                   silentPin ? pin.callbacks() : E::SilentPin()) {
        pin.driver = &driver;
        if (!route)
            return;
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
    // Each target reserves only its own console: S3 USB GPIO19/20, S2 UART0 GPIO43/44.
    // Both reserve the MSPI flash bus GPIO26..32; S3 also its ROM boot-print U0TXD GPIO43.
    // An unspecified target reserves nothing (the s2_bench application fixes its pins).
    using K = E::FirmwareConfigCheck;
    using T = E::FirmwareTarget;
    const struct {
        T target;
        int pin;
        K expected;
    } policy[] = {{T::ESP32S3, 19, K::CONSOLE_PIN}, {T::ESP32S3, 20, K::CONSOLE_PIN},
                  {T::ESP32S3, 43, K::SYSTEM_PIN},  {T::ESP32S3, 44, K::OK},
                  {T::ESP32S3, 26, K::SYSTEM_PIN},  {T::ESP32S3, 32, K::SYSTEM_PIN},
                  {T::ESP32S3, 33, K::OK},          {T::ESP32S3, 21, K::OK},
                  {T::ESP32S2, 43, K::CONSOLE_PIN}, {T::ESP32S2, 44, K::CONSOLE_PIN},
                  {T::ESP32S2, 26, K::SYSTEM_PIN},  {T::ESP32S2, 29, K::SYSTEM_PIN},
                  {T::ESP32S2, 30, K::SYSTEM_PIN},  {T::ESP32S2, 32, K::SYSTEM_PIN},
                  {T::ESP32S2, 33, K::OK},          {T::ESP32S2, 21, K::OK},
                  {T::ESP32S2, 19, K::OK},          {T::ESP32S2, 20, K::OK},
                  {T::UNSPECIFIED, 19, K::OK},      {T::UNSPECIFIED, 43, K::OK},
                  {T::UNSPECIFIED, 30, K::OK}};
    for (const auto& row : policy) {
        for (unsigned tx = 0; tx < 2; ++tx) {
            Rig rig;
            E::FirmwareConfig cfg = config();
            cfg.target = row.target;
            if (tx)
                cfg.controller.txPin = row.pin;
            else
                cfg.controller.rxPin = row.pin;
            CHECK(E::checkFirmwareConfig(cfg, false) == row.expected);
            CHECK(rig.firmware.begin(cfg) == (row.expected == K::OK));
            CHECK(rig.firmware.lastCheck() == row.expected);
            // A refused pin is refused before any driver call, and nothing is sent.
            CHECK(rig.driver.starts == (row.expected == K::OK ? 1u : 0u) && rig.driver.submits == 0);
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
    CHECK(valid.firmware.running() && !valid.firmware.progressionFailed());
    CHECK(valid.driver.starts == 1 && valid.driver.submits == 0);
}
void clockBoundariesAndRetirement() {
    Rig wide;
    CHECK(wide.firmware.begin(config()));
    wide.turn(0);
    wide.turn(UINT32_MAX);
    wide.turn(UINT64_C(4294967296));
    wide.turn(UINT64_C(4294967296));
    CHECK(!wide.firmware.progressionFailed() && wide.driver.submits == 0);
    for (unsigned variant = 0; variant < 4; ++variant) {
        Rig r;
        CHECK(r.firmware.begin(config()) && r.firmware.app().select(target()));
        CHECK(r.firmware.app().requestConfig(0, 1000, options()));
        r.turn(0);
        r.turn(1);
        CHECK(r.driver.submits == 1 && r.driver.active);
        const O::RequestId id = r.firmware.app().pendingRequest();
        r.driver.stopOk = variant < 2;
        r.bytes.clockOk = (variant % 2) == 0;
        r.bytes.now = 0;
        CHECK(!r.firmware.service());
        CHECK(!r.firmware.running() && r.firmware.progressionFailed());
        CHECK(r.firmware.owner().resources().inhibited && r.driver.stops == 1);
        CHECK(r.firmware.faultRetired() == r.driver.stopOk);
        CHECK(r.firmware.transport().diagnostics().inhibited);
        CHECK(r.driver.active == !r.driver.stopOk);
        const O::Completion* terminal = r.firmware.owner().result(id);
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
    CHECK(advanced.firmware.progressionFailed() && advanced.firmware.faultRetired());
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
        const O::RequestId id = r.firmware.app().pendingRequest();
        const O::Completion* terminal = r.firmware.owner().result(id);
        if (!terminal)
            continue;
        CHECK(terminal->service.outcome == C::SdoOutcome::SUCCESS);
        unsigned char copy[sizeof(O::Completion)];
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
    O::SequenceId sequence;
    CHECK(r.firmware.owner().beginSequence(target().channel.binding, 1, 0, sequence));
    CHECK(r.firmware.owner().beginOperation(sequence, 1, 1000, 0));
    O::BusRequest work;
    work.sequence = sequence;
    work.transfer.channel = target().channel;
    work.transfer.operation = 1;
    work.transfer.index = 0x1005;
    work.transfer.size = 4;
    work.transfer.readEffect = C::SdoReadEffect::REVIEWED_NON_CHANGING;
    work.transfer.queueTimeoutUs = work.transfer.responseTimeoutUs = 100;
    work.transfer.deadlineUs = 1000;
    O::RequestId ids[O::CanBusOwner::REQUESTS];
    for (unsigned i = 0; i < O::CanBusOwner::REQUESTS; ++i) {
        work.transfer.request = work.transfer.step = i + 1;
        CHECK(r.firmware.owner().admit(work, 0, ids[i]) == O::BusAdmission::ACCEPTED);
    }
    O::RequestId rejected;
    ++work.transfer.request;
    ++work.transfer.step;
    CHECK(r.firmware.owner().admit(work, 0, rejected) == O::BusAdmission::RESULTS_FULL);
    CHECK(!rejected.owner && r.driver.submits == 0);
    r.turn(0);
    CHECK(r.driver.submits == 1);
    r.driver.pending.busOff = true;
    r.turn(1);
    CHECK(r.firmware.owner().resources().inhibited);
    CHECK(r.firmware.owner().resources().retained == O::CanBusOwner::REQUESTS);
    for (unsigned i = 0; i < O::CanBusOwner::REQUESTS; ++i)
        CHECK(r.firmware.owner().result(ids[i]));
    r.turn(2);
    CHECK(r.driver.starts == 1 && r.driver.submits == 1);
    O::RecoveryId recovery;
    CHECK(r.firmware.owner().recover(3, 100, recovery));
    r.turn(3);
    CHECK(r.driver.stops == 1 && !r.firmware.owner().recoveryResult(recovery));
    r.turn(4);
    const O::RecoveryCompletion* result = r.firmware.owner().recoveryResult(recovery);
    CHECK(result && result->outcome == O::RecoveryOutcome::RETIRED);
    CHECK(r.driver.starts == 2 && r.driver.epoch == 2);
    r.turn(5);
    CHECK(r.driver.submits == 1 && !r.driver.active);
    CHECK(!r.firmware.transport().diagnostics().inhibited);
    for (unsigned i = 0; i < O::CanBusOwner::REQUESTS; ++i) {
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
    // Each scripted report is in the mailbox before its reply, and pollEvents precedes
    // the owner, so every report is consumed before this terminal snapshot.
    CHECK(record.initialStats.txFrames == 0 && record.finalStats.txFrames == answered);
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
            CHECK(r.firmware.progressionFailed() && r.firmware.faultRetired());
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
                    record.traffic[i].kind == O::DiagnosticKind::CONTROLLER &&
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
// A firmware failure names the step that latched it, never a blanket clock fault (C-13).
void failureReportsTheRealReason() {
    const auto retiredAndLatched = [](Rig& r, E::FirmwareFailure reason) {
        CHECK(!r.firmware.running() && r.firmware.progressionFailed());
        CHECK(r.firmware.failure() == reason);
        CHECK(r.firmware.faultRetired() && r.driver.stops == 1);
        CHECK(r.firmware.owner().resources().inhibited);
        CHECK(r.firmware.transport().diagnostics().inhibited);
        const unsigned clocks = r.bytes.clocks, submits = r.driver.submits;
        r.bytes.clockOk = true;
        r.bytes.now = UINT64_C(1000000);
        for (unsigned i = 0; i < 3; ++i)
            CHECK(!r.firmware.service());
        CHECK(r.firmware.failure() == reason); // A later turn cannot rewrite the first cause.
        CHECK(r.bytes.clocks == clocks && r.driver.submits == submits && r.driver.stops == 1);
    };
    {
        Rig r;
        CHECK(r.firmware.begin(config()));
        CHECK(!r.firmware.progressionFailed());
        CHECK(r.firmware.failure() == E::FirmwareFailure::NONE);
        r.turn(10);
        CHECK(r.firmware.failure() == E::FirmwareFailure::NONE);
        r.bytes.clockOk = false;
        CHECK(!r.firmware.service());
        retiredAndLatched(r, E::FirmwareFailure::CLOCK);
    }
    {
        Rig r; // Supplied time moved backward; the clock read itself succeeded.
        CHECK(r.firmware.begin(config()));
        r.turn(10);
        r.bytes.now = 9;
        CHECK(!r.firmware.service());
        retiredAndLatched(r, E::FirmwareFailure::CLOCK);
    }
    {
        Rig r; // Clock is valid and monotonic; the owner refuses a time behind its own.
        CHECK(r.firmware.begin(config()));
        r.turn(100);
        CHECK(r.firmware.owner().reportController(C::ControllerState::AVAILABLE, 200));
        r.bytes.now = 150;
        CHECK(!r.firmware.service());
        CHECK(r.firmware.owner().lastTimeUs() == 200);
        retiredAndLatched(r, E::FirmwareFailure::APPLICATION);
    }
    {
        Rig r; // Clock, owner and application all progress; only evidence refuses this time.
        CHECK(r.firmware.begin(config()) && r.firmware.app().select(target()));
        r.turn(0);
        const E::ReadExperiment review = syntheticEvidenceReview();
        CHECK(r.firmware.evidence().requestExperiment(review, 0) == E::EvidenceAdmission::ACCEPTED);
        CHECK(r.firmware.evidence().service(50)); // A public call ahead of the firmware turn.
        r.bytes.now = 40;
        CHECK(!r.firmware.service());
        CHECK(r.firmware.owner().lastTimeUs() == 40);
        retiredAndLatched(r, E::FirmwareFailure::EVIDENCE);
    }
}
} // namespace
// The firmware hands the owner a transport whose beforeTransmit reaches the registered preflight,
// as the bench routes register AxisSession::preflight. A refusal closes the download unsent and
// nothing reaches the controller; with the check removed the same download is handed over.
struct Gate {
    unsigned calls = 0;
    bool allow = false;
    static bool check(void* context, O::SequenceId, const C::SdoTransfer& transfer, uint64_t) {
        Gate& g = *static_cast<Gate*>(context);
        ++g.calls;
        CHECK(transfer.direction == C::SdoDirection::DOWNLOAD && transfer.index == 0x2500);
        return g.allow;
    }
};
O::RequestId download(Rig& r, uint32_t operation, uint64_t now, O::SequenceId& sequence) {
    O::CanBusOwner& owner = r.firmware.owner();
    CHECK(owner.beginSequence(target().channel.binding, 1, now, sequence));
    CHECK(owner.beginOperation(sequence, operation, now + 1000, now));
    O::BusRequest request;
    request.sequence = sequence;
    request.transfer.channel = target().channel;
    request.transfer.operation = request.transfer.request = operation;
    request.transfer.step = 1;
    request.transfer.index = 0x2500;
    request.transfer.size = 2;
    request.transfer.direction = C::SdoDirection::DOWNLOAD;
    request.transfer.queueTimeoutUs = request.transfer.responseTimeoutUs = 100;
    request.transfer.deadlineUs = now + 1000;
    O::RequestId id;
    CHECK(owner.admit(request, now, id) == O::BusAdmission::ACCEPTED);
    return id;
}
void preflightReachesTheOwner() {
    Rig r;
    CHECK(r.firmware.begin(config()));
    M::Frame heartbeat = frame(0x705, 1);
    heartbeat.data[0] = 5;
    r.driver.push(heartbeat); // A current NMT state, so an ordinary download is admissible.
    r.turn(0);
    Gate gate;
    E::DownloadPreflight preflight;
    preflight.context = &gate;
    preflight.check = Gate::check;
    r.firmware.setPreflight(preflight);
    O::SequenceId sequence;
    const O::RequestId refused = download(r, 1, 1, sequence);
    r.turn(2);
    const O::Completion* c = r.firmware.owner().result(refused);
    CHECK(gate.calls == 1 && c && c->reason == O::CompletionReason::PREFLIGHT_REJECTED);
    CHECK(!c->controllerAccepted && !c->service.transmissionPossible && r.driver.submits == 0);
    CHECK(r.firmware.owner().release(refused) && r.firmware.owner().releaseSequence(sequence));
    r.firmware.setPreflight(E::DownloadPreflight());
    download(r, 2, 3, sequence);
    r.turn(4);
    CHECK(gate.calls == 1 && r.driver.submits == 1 && r.driver.sent[0].id == 0x605 &&
          r.driver.sent[0].data[0] == 0x2b && r.driver.sent[0].data[1] == 0x00 &&
          r.driver.sent[0].data[2] == 0x25);
}

// Board checks name the first failing fact; no check drives a GPIO or the controller.
void boardChecks() {
    static_assert(E::firmwareConsolePin(E::FirmwareTarget::ESP32S3, 19) &&
                      E::firmwareConsolePin(E::FirmwareTarget::ESP32S3, 20) &&
                      !E::firmwareConsolePin(E::FirmwareTarget::ESP32S3, 43),
                  "S3 reserves its USB Serial/JTAG pins only");
    static_assert(E::firmwareConsolePin(E::FirmwareTarget::ESP32S2, 43) &&
                      E::firmwareConsolePin(E::FirmwareTarget::ESP32S2, 44) &&
                      !E::firmwareConsolePin(E::FirmwareTarget::ESP32S2, 19) &&
                      !E::firmwareConsolePin(E::FirmwareTarget::ESP32S2, -1),
                  "S2 reserves its UART0 console pins only");
    // System pins at run time, so a policy change fails here with its line, not at compile.
    for (int gpio = -2; gpio < 50; ++gpio) {
        const bool flash = gpio >= 26 && gpio <= 32;
        CHECK(E::firmwareSystemPin(E::FirmwareTarget::ESP32S2, gpio) == flash);
        CHECK(E::firmwareSystemPin(E::FirmwareTarget::ESP32S3, gpio) == (flash || gpio == 43));
        CHECK(!E::firmwareSystemPin(E::FirmwareTarget::UNSPECIFIED, gpio));
    }
    using K = E::FirmwareConfigCheck;
    E::FirmwareConfig cfg;
    CHECK(E::checkFirmwareConfig(cfg, true) == K::NOT_CONFIGURED);
    cfg = config();
    cfg.target = E::FirmwareTarget::ESP32S2;
    CHECK(E::checkFirmwareConfig(cfg, false) == K::OK);
    cfg.controller.txPin = -1;
    CHECK(E::checkFirmwareConfig(cfg, true) == K::MISSING_PIN);
    cfg = config();
    cfg.target = E::FirmwareTarget::ESP32S2;
    cfg.controller.rxPin = cfg.controller.txPin;
    CHECK(E::checkFirmwareConfig(cfg, true) == K::PIN_CONFLICT);
    const struct {
        int pin;
        E::SilentLevel level;
        bool driver;
        K expected;
    } silent[] = {{18, E::SilentLevel::HIGH, true, K::OK},
                  {18, E::SilentLevel::LOW, true, K::OK},
                  {18, E::SilentLevel::HIGH, false, K::SILENT_INCOMPLETE},
                  {-1, E::SilentLevel::HIGH, true, K::SILENT_INCOMPLETE},
                  {18, E::SilentLevel::NONE, true, K::SILENT_INCOMPLETE},
                  {4, E::SilentLevel::HIGH, true, K::PIN_CONFLICT},
                  {5, E::SilentLevel::LOW, true, K::PIN_CONFLICT},
                  {43, E::SilentLevel::HIGH, true, K::CONSOLE_PIN},
                  {43, E::SilentLevel::NONE, true, K::CONSOLE_PIN},
                  {30, E::SilentLevel::HIGH, true, K::SYSTEM_PIN},
                  {26, E::SilentLevel::NONE, false, K::SYSTEM_PIN},
                  {4, E::SilentLevel::NONE, true, K::PIN_CONFLICT},
                  {19, E::SilentLevel::HIGH, true, K::OK},
                  {-1, E::SilentLevel::NONE, false, K::OK}};
    for (const auto& row : silent) {
        cfg = config();
        cfg.target = E::FirmwareTarget::ESP32S2;
        cfg.transceiver.silentPin = row.pin;
        cfg.transceiver.silentLevel = row.level;
        CHECK(E::checkFirmwareConfig(cfg, row.driver) == row.expected);
        Rig rig(row.driver);
        CHECK(rig.firmware.prepare(cfg) == (row.expected == K::OK));
        CHECK(rig.firmware.lastCheck() == row.expected);
        // A refused board touches neither the GPIO nor the controller.
        const bool drives = row.expected == K::OK && row.level != E::SilentLevel::NONE;
        CHECK(rig.pin.count == (drives ? 1u : 0u) && rig.driver.starts == 0);
        CHECK(!rig.firmware.prepare(cfg) && rig.pin.count == (drives ? 1u : 0u));
        CHECK(rig.firmware.start() == (row.expected == K::OK));
        CHECK(rig.driver.submits == 0);
    }
    const struct {
        E::FirmwareTarget target;
        int pin;
        K expected;
    } targets[] = {{E::FirmwareTarget::ESP32S3, 19, K::CONSOLE_PIN},
                   {E::FirmwareTarget::ESP32S3, 20, K::CONSOLE_PIN},
                   {E::FirmwareTarget::ESP32S3, 43, K::SYSTEM_PIN},
                   {E::FirmwareTarget::ESP32S3, 44, K::OK},
                   {E::FirmwareTarget::ESP32S3, 31, K::SYSTEM_PIN},
                   {E::FirmwareTarget::UNSPECIFIED, 43, K::OK},
                   {E::FirmwareTarget::UNSPECIFIED, 19, K::OK}};
    for (const auto& row : targets) {
        cfg = config();
        cfg.target = row.target;
        cfg.transceiver.silentPin = row.pin;
        cfg.transceiver.silentLevel = E::SilentLevel::HIGH;
        CHECK(E::checkFirmwareConfig(cfg, true) == row.expected);
        Rig rig(true); // A refused silent pin is never driven, not even to its silent level.
        CHECK(rig.firmware.prepare(cfg) == (row.expected == K::OK));
        CHECK(rig.pin.count == (row.expected == K::OK ? 1u : 0u) && rig.driver.starts == 0);
    }
    // Declaration order names the refusal: console, then system, then conflict.
    cfg = config();
    cfg.target = E::FirmwareTarget::ESP32S3;
    cfg.controller.txPin = cfg.controller.rxPin = 43;
    CHECK(E::checkFirmwareConfig(cfg, false) == K::SYSTEM_PIN);
    cfg.controller.rxPin = 19;
    CHECK(E::checkFirmwareConfig(cfg, false) == K::CONSOLE_PIN);
}
E::FirmwareConfig silentConfig(E::SilentLevel level) {
    E::FirmwareConfig cfg = config();
    cfg.target = E::FirmwareTarget::ESP32S2;
    cfg.transceiver.silentPin = 18;
    cfg.transceiver.silentLevel = level;
    return cfg;
}
// Silence is requested first and kept until an explicit start has opened the controller;
// every failure path ends with the controller closed and silence requested where possible.
void silentTransceiver() {
    for (unsigned high = 0; high < 2; ++high) {
        const E::SilentLevel level = high ? E::SilentLevel::HIGH : E::SilentLevel::LOW;
        Rig r(true);
        CHECK(r.firmware.transceiver() == E::TransceiverState::NOT_CONTROLLED);
        CHECK(r.firmware.prepare(silentConfig(level)));
        CHECK(r.pin.count == 1 && r.pin.calls[0].gpio == 18 && r.pin.calls[0].high == (high == 1));
        CHECK(r.pin.calls[0].starts == 0 && r.driver.starts == 0);
        CHECK(r.firmware.transceiver() == E::TransceiverState::SILENT_REQUESTED);
        // Passive: turns before the explicit start open nothing and send nothing.
        for (uint64_t now = 0; now < 20; ++now) {
            r.bytes.now = now;
            CHECK(!r.firmware.service());
        }
        CHECK(r.driver.starts == 0 && r.driver.submits == 0 && r.pin.count == 1);
        CHECK(r.firmware.start());
        CHECK(r.pin.count == 2 && r.pin.calls[1].high == (high == 0) && r.pin.calls[1].starts == 1);
        CHECK(r.firmware.transceiver() == E::TransceiverState::NORMAL_REQUESTED);
        CHECK(r.firmware.running() && r.driver.submits == 0 && !r.firmware.start());
        r.turn(20);
        CHECK(r.driver.submits == 0); // Startup and start admit no work.
        // Explicit stop: silence before the controller is retired.
        CHECK(r.firmware.stop());
        CHECK(r.pin.count == 3 && r.pin.calls[2].high == (high == 1) && r.pin.calls[2].stops == 0);
        CHECK(r.driver.stops == 1 && !r.driver.live && !r.firmware.running());
        CHECK(r.firmware.owner().resources().inhibited && r.firmware.stopped());
        CHECK(r.firmware.transceiver() == E::TransceiverState::SILENT_REQUESTED);
        CHECK(!r.firmware.stop() && !r.firmware.start() && r.driver.starts == 1);
        CHECK(!r.firmware.progressionFailed() && r.pin.count == 3);
    }
    {
        Rig r(true); // The SDK refuses the silent level: nothing may start afterwards.
        r.pin.failAt = 1;
        CHECK(!r.firmware.prepare(silentConfig(E::SilentLevel::HIGH)));
        CHECK(r.firmware.transceiver() == E::TransceiverState::FAILED);
        CHECK(r.firmware.lastCheck() == E::FirmwareConfigCheck::OK);
        CHECK(!r.firmware.start() && !r.firmware.stop() && r.driver.starts == 0);
    }
    {
        Rig r(true); // Controller start fails: the silent level is never released.
        r.driver.startOk = false;
        CHECK(r.firmware.prepare(silentConfig(E::SilentLevel::HIGH)));
        CHECK(!r.firmware.start() && r.driver.starts == 1 && r.pin.count == 1);
        CHECK(r.firmware.transceiver() == E::TransceiverState::SILENT_REQUESTED);
        CHECK(r.firmware.started() && !r.firmware.running());
        // The refused start retires at once: a partial SDK node is deleted now, not at stop.
        CHECK(r.driver.stops == 1 && r.firmware.retired());
        // Stop repeats silence only; the confirmed shutdown is not repeated.
        CHECK(r.firmware.stop() && r.driver.stops == 1 && r.pin.count == 2);
    }
    {
        Rig r(true); // Normal level refused after start: close the controller, ask silence again.
        r.pin.failAt = 2;
        CHECK(r.firmware.prepare(silentConfig(E::SilentLevel::HIGH)));
        CHECK(!r.firmware.start());
        CHECK(r.driver.starts == 1 && r.driver.stops == 1 && !r.firmware.running());
        // Silence first, then the controller shutdown, as on every failure path.
        CHECK(r.pin.count == 3 && r.pin.calls[2].high && r.pin.calls[2].stops == 0);
        CHECK(r.firmware.transceiver() == E::TransceiverState::SILENT_REQUESTED);
        r.bytes.now = 1;
        CHECK(!r.firmware.service() && r.driver.submits == 0);
    }
    {
        Rig r(true); // A refused stop shutdown is reported, and silence is still requested.
        CHECK(r.firmware.prepare(silentConfig(E::SilentLevel::HIGH)) && r.firmware.start());
        r.driver.stopOk = false;
        CHECK(!r.firmware.stop() && r.driver.stops == 1 && !r.firmware.running());
        CHECK(r.pin.count == 3 && r.pin.calls[2].high && r.pin.calls[2].stops == 0);
        CHECK(r.firmware.transceiver() == E::TransceiverState::SILENT_REQUESTED);
        CHECK(r.firmware.transport().diagnostics().inhibited);
        CHECK(r.firmware.stopped() && !r.firmware.stopConfirmed());
        CHECK(E::firmwareStatus(r.firmware, true) == E::FirmwareBootStatus::STOP_FAILED);
    }
    {
        Rig r(true); // Silence refused at stop: the level is unknown, the controller still retires.
        CHECK(r.firmware.prepare(silentConfig(E::SilentLevel::HIGH)) && r.firmware.start());
        r.pin.failAt = 3;
        CHECK(!r.firmware.stop() && r.driver.stops == 1 && !r.driver.live);
        CHECK(r.firmware.transceiver() == E::TransceiverState::FAILED);
        CHECK(E::firmwareStatus(r.firmware, true) == E::FirmwareBootStatus::STOP_FAILED);
    }
    {
        Rig r(true); // Silence refused at a progression failure: retirement still proceeds.
        CHECK(r.firmware.prepare(silentConfig(E::SilentLevel::HIGH)) && r.firmware.start());
        r.turn(5);
        r.pin.failAt = 3;
        r.bytes.clockOk = false;
        CHECK(!r.firmware.service() && r.firmware.progressionFailed());
        CHECK(r.firmware.transceiver() == E::TransceiverState::FAILED);
        CHECK(r.driver.stops == 1 && r.firmware.faultRetired());
        CHECK(E::firmwareStatus(r.firmware, true) == E::FirmwareBootStatus::PROGRESSION_FAILED);
    }
    {
        Rig r(true); // A progression failure silences before shutting the controller down.
        CHECK(r.firmware.prepare(silentConfig(E::SilentLevel::HIGH)) && r.firmware.start());
        r.turn(5);
        r.bytes.clockOk = false;
        CHECK(!r.firmware.service() && r.firmware.progressionFailed());
        CHECK(r.pin.count == 3 && r.pin.calls[2].high && r.pin.calls[2].stops == 0);
        CHECK(r.driver.stops == 1 && r.firmware.faultRetired());
        CHECK(r.firmware.transceiver() == E::TransceiverState::SILENT_REQUESTED);
        // Stop after the failure repeats only the silence; the controller is already retired.
        CHECK(r.firmware.stop() && r.driver.stops == 1 && r.pin.count == 4);
    }
    {
        Rig r(true); // No silent input: prepare drives nothing; start alone opens the controller.
        CHECK(r.firmware.prepare(config()) && r.pin.count == 0 && r.driver.starts == 0);
        CHECK(r.firmware.transceiver() == E::TransceiverState::NOT_CONTROLLED);
        CHECK(r.firmware.start() && r.driver.starts == 1 && r.pin.count == 0);
        CHECK(r.firmware.stop() && r.driver.stops == 1 && r.pin.count == 0);
    }
}
// The shared task's status projection: explicit outcomes win over startup facts.
void bootStatus() {
    using S = E::FirmwareBootStatus;
    {
        Rig r(true);
        CHECK(r.firmware.prepare(E::FirmwareConfig()) == false);
        CHECK(E::firmwareStatus(r.firmware, true) == S::CONFIGURATION_REQUIRED);
        CHECK(E::firmwareStatus(r.firmware, false) == S::PLATFORM_FAILED);
    }
    {
        Rig r(true);
        E::FirmwareConfig cfg = silentConfig(E::SilentLevel::HIGH);
        cfg.controller.txPin = 43;
        CHECK(!r.firmware.prepare(cfg));
        CHECK(E::firmwareStatus(r.firmware, true) == S::CONFIGURATION_REJECTED);
    }
    {
        Rig r(true);
        r.pin.failAt = 1;
        CHECK(!r.firmware.prepare(silentConfig(E::SilentLevel::HIGH)));
        CHECK(E::firmwareStatus(r.firmware, true) == S::TRANSCEIVER_FAILED);
        // The unknown pin level is not hidden behind a console that also failed.
        CHECK(E::firmwareStatus(r.firmware, false) == S::TRANSCEIVER_FAILED);
    }
    {
        Rig r(true); // A refused board is benign: a console failure is the reported fact.
        E::FirmwareConfig cfg = silentConfig(E::SilentLevel::HIGH);
        cfg.controller.txPin = 30;
        CHECK(!r.firmware.prepare(cfg) && r.firmware.lastCheck() == E::FirmwareConfigCheck::SYSTEM_PIN);
        CHECK(E::firmwareStatus(r.firmware, true) == S::CONFIGURATION_REJECTED);
        CHECK(E::firmwareStatus(r.firmware, false) == S::PLATFORM_FAILED);
    }
    {
        Rig r(true);
        CHECK(r.firmware.prepare(silentConfig(E::SilentLevel::HIGH)));
        CHECK(E::firmwareStatus(r.firmware, true) == S::INACTIVE);
        CHECK(E::firmwareStatus(r.firmware, false) == S::PLATFORM_FAILED);
        CHECK(r.firmware.start());
        // A hook may start after a console failure: report what happened, not the console.
        CHECK(E::firmwareStatus(r.firmware, false) == S::RUNNING);
        CHECK(r.firmware.stop() && E::firmwareStatus(r.firmware, true) == S::STOPPED);
    }
    {
        Rig r(true);
        r.driver.startOk = false;
        CHECK(r.firmware.prepare(silentConfig(E::SilentLevel::HIGH)) && !r.firmware.start());
        CHECK(r.firmware.retired() && E::firmwareStatus(r.firmware, true) == S::CONTROLLER_FAILED);
    }
    {
        C::SdoClient services; // Platform defect: idle must be null.
        Driver driver;
        Bytes bytes;
        E::ProbeCallbacks callbacks = bytes.callbacks();
        callbacks.idle = [](void*) {};
        E::FirmwareProbe firmware(services, driver.callbacks(), callbacks);
        CHECK(!firmware.prepare(config()) && driver.starts == 0);
        CHECK(firmware.lastCheck() == E::FirmwareConfigCheck::INVALID_PLATFORM);
        CHECK(E::firmwareStatus(firmware, true) == S::PLATFORM_FAILED);
    }
}
// The controller stays closed for the rest of the boot, so the owner must refuse new work
// rather than admit a read that no service turn will ever run.
void refusesLaterWork(Rig& r) {
    CHECK(r.firmware.owner().resources().inhibited);
    CHECK(r.services.controllerState() == C::ControllerState::RECOVERING);
    const unsigned submits = r.driver.submits, starts = r.driver.starts;
    const uint64_t now = r.firmware.owner().lastTimeUs();
    L::ReadTarget current = target();
    const C::NodeSnapshot* node = r.services.node(0);
    CHECK(node);
    current.nodeGeneration = node->route.generation;
    current.channel.binding = node->route.binding;
    // Selection itself must be current. This test isolates controller inhibition rather than
    // relying on the old selection bug admitting the pre-invalidation node epoch.
    CHECK(r.firmware.app().select(current));
    CHECK(r.firmware.owner().matchesChannel(current.channel, current.nodeGeneration));
    CHECK(!r.firmware.app().requestIdentity(now, now + 2000, options()));
    CHECK(r.firmware.app().readContext().state == M::ReadState::EMPTY);
    for (uint64_t t = now; t < now + 50; ++t) {
        r.bytes.now = t;
        CHECK(!r.firmware.service());
    }
    CHECK(!r.firmware.start() && r.driver.submits == submits && r.driver.starts == starts);
}
// Review R-D1/R-D2: every path that leaves the controller closed invalidates the owner once,
// and a start-path shutdown that the driver refused is reported as such, not as a clean refusal.
void closedPathsInvalidateTheOwner() {
    using S = E::FirmwareBootStatus;
    const auto status = [](const Rig& r) { return E::firmwareStatus(r.firmware, true); };
    {
        Rig r(true); // Control: while INACTIVE the same request is admitted and waits for start.
        CHECK(r.firmware.prepare(silentConfig(E::SilentLevel::HIGH)));
        CHECK(!r.firmware.owner().resources().inhibited);
        CHECK(r.firmware.app().select(target()) && r.firmware.app().requestIdentity(0, 2000, options()));
        CHECK(r.firmware.app().readContext().state == M::ReadState::ACTIVE);
        // The controller start is refused: the held read was never handed over, and only an
        // explicit cancel ends it; the owner then refuses everything else.
        r.driver.startOk = false;
        CHECK(!r.firmware.start() && status(r) == S::CONTROLLER_FAILED);
        CHECK(r.driver.starts == 1 && r.driver.stops == 1 && r.firmware.retired());
        CHECK(r.firmware.app().readContext().state == M::ReadState::ACTIVE);
        CHECK(r.firmware.app().cancelRead(0));
        CHECK(r.firmware.app().readContext().state != M::ReadState::ACTIVE);
        CHECK(r.firmware.app().releaseRead());
        refusesLaterWork(r);
        CHECK(r.driver.submits == 0);
    }
    {
        Rig r(true); // Normal level refused after a successful controller start.
        r.pin.failAt = 2;
        CHECK(r.firmware.prepare(silentConfig(E::SilentLevel::HIGH)) && !r.firmware.start());
        CHECK(status(r) == S::CONTROLLER_FAILED && r.firmware.retired() && !r.driver.live);
        refusesLaterWork(r);
    }
    {
        Rig r(true); // Stop before any start: nothing to shut down, the owner still invalidated.
        CHECK(r.firmware.prepare(silentConfig(E::SilentLevel::HIGH)) && r.firmware.stop());
        CHECK(status(r) == S::STOPPED && r.firmware.stopConfirmed() && r.firmware.retired());
        CHECK(r.driver.starts == 0 && r.driver.stops == 0 && r.pin.count == 2 && r.pin.calls[1].high);
        refusesLaterWork(r);
    }
    {
        Rig r(true); // Ordinary stop while running, through the same retirement.
        CHECK(r.firmware.prepare(silentConfig(E::SilentLevel::HIGH)) && r.firmware.start());
        r.turn(3);
        CHECK(r.firmware.stop() && status(r) == S::STOPPED);
        refusesLaterWork(r);
    }
    // Refused prepares: the compile fixture, console and system pins, a refused silent level.
    for (unsigned variant = 0; variant < 4; ++variant) {
        Rig r(true);
        E::FirmwareConfig cfg = silentConfig(E::SilentLevel::HIGH);
        if (variant == 0)
            cfg = E::FirmwareConfig();
        else if (variant == 1)
            cfg.controller.txPin = 43;
        else if (variant == 2)
            cfg.controller.rxPin = 31;
        else
            r.pin.failAt = 1;
        CHECK(!r.firmware.prepare(cfg) && r.driver.starts == 0);
        CHECK(status(r) == (variant == 0   ? S::CONFIGURATION_REQUIRED
                            : variant == 3 ? S::TRANSCEIVER_FAILED
                                           : S::CONFIGURATION_REJECTED));
        CHECK(!r.firmware.stop()); // Stop is valid after a successful prepare only.
        refusesLaterWork(r);
    }
    {
        C::SdoClient services; // Platform defect: the owner is invalidated as well.
        Driver driver;
        Bytes bytes;
        E::ProbeCallbacks callbacks = bytes.callbacks();
        callbacks.idle = [](void*) {};
        E::FirmwareProbe firmware(services, driver.callbacks(), callbacks);
        CHECK(!firmware.prepare(config()) && firmware.owner().resources().inhibited);
    }
    {
        Rig r(true); // Normal level refused and its shutdown refused: the node may remain.
        r.pin.failAt = 2;
        r.driver.stopOk = false;
        CHECK(r.firmware.prepare(silentConfig(E::SilentLevel::HIGH)) && !r.firmware.start());
        CHECK(!r.firmware.running() && !r.firmware.retired() && r.driver.live);
        CHECK(r.firmware.transceiver() == E::TransceiverState::SILENT_REQUESTED);
        CHECK(r.firmware.transport().diagnostics().inhibited);
        CHECK(status(r) == S::CONTROLLER_UNRETIRED);
        CHECK(E::firmwareStatus(r.firmware, false) == S::CONTROLLER_UNRETIRED);
        refusesLaterWork(r);
        // A later explicit stop repeats only the unconfirmed shutdown, after silence.
        r.driver.stopOk = true;
        CHECK(r.firmware.stop() && r.driver.stops == 2 && !r.driver.live && r.firmware.retired());
        CHECK(r.pin.count == 4 && r.pin.calls[3].high && r.pin.calls[3].stops == 1);
        CHECK(status(r) == S::STOPPED);
    }
    {
        Rig r(true); // Refused start and refused shutdown: a partial SDK node may remain.
        r.driver.startOk = false;
        r.driver.stopOk = false;
        CHECK(r.firmware.prepare(silentConfig(E::SilentLevel::HIGH)) && !r.firmware.start());
        CHECK(!r.firmware.retired() && r.driver.stops == 1 && status(r) == S::CONTROLLER_UNRETIRED);
        CHECK(!r.firmware.stop() && r.driver.stops == 2 && status(r) == S::STOP_FAILED);
    }
    {
        Rig r(true); // Normal level and the silence after it both refused: the pin level is
        r.pin.failMask = 0x6u; // unknown, but the controller shutdown was confirmed.
        CHECK(r.firmware.prepare(silentConfig(E::SilentLevel::HIGH)) && !r.firmware.start());
        CHECK(r.pin.count == 3 && r.firmware.transceiver() == E::TransceiverState::FAILED);
        CHECK(r.firmware.retired() && r.driver.stops == 1 && !r.driver.live);
        CHECK(status(r) == S::CONTROLLER_FAILED);
        refusesLaterWork(r);
        // Stop asks for silence once more and does not shut the controller down again.
        CHECK(r.firmware.stop() && r.pin.count == 4 && r.driver.stops == 1);
        CHECK(r.firmware.transceiver() == E::TransceiverState::SILENT_REQUESTED);
        CHECK(status(r) == S::STOPPED);
    }
    {
        Rig r(true); // Progression failure whose shutdown is refused, then an explicit stop.
        CHECK(r.firmware.prepare(silentConfig(E::SilentLevel::HIGH)) && r.firmware.start());
        r.turn(5);
        r.driver.stopOk = false;
        r.bytes.clockOk = false;
        CHECK(!r.firmware.service() && r.firmware.progressionFailed());
        CHECK(!r.firmware.faultRetired() && !r.firmware.retired() && r.driver.live);
        CHECK(status(r) == S::PROGRESSION_FAILED);
        refusesLaterWork(r);
        r.driver.stopOk = true;
        CHECK(r.firmware.stop() && r.firmware.stopConfirmed() && r.firmware.retired());
        CHECK(r.driver.stops == 2 && !r.driver.live);
        // The first cause stays the status, and the failure path's own retirement stays
        // unconfirmed: TX between the failure and the stop cannot be excluded.
        CHECK(status(r) == S::PROGRESSION_FAILED && !r.firmware.faultRetired());
    }
}
// Answer the active synthetic identity upload of node 1 or 2 with fixed raw values.
void answerIdentity(Driver& d) {
    CHECK(d.active && !d.pending.txDone);
    const M::Frame& request = d.sent[d.submits - 1];
    CHECK(request.data[0] == 0x40 && (request.id == 0x601 || request.id == 0x602));
    const uint16_t index = uint16_t(request.data[1]) | uint16_t(request.data[2]) << 8;
    const uint8_t sub = request.data[3];
    unsigned width = 4;
    uint32_t raw = 0x1000u + index + sub;
    if (index == 0x1009 || index == 0x100a || (index == 0x1018 && sub == 0)) {
        width = 1; // Measured one-byte answers, as configured for this fixture.
        raw = index == 0x1018 ? 4 : 0x0c;
    } else if (index == 0x2000 || index == 0x2001) {
        width = 2;
    } else {
        CHECK(index == 0x1000 || index == 0x1018);
    }
    M::Frame response = frame(request.id - 0x80, 8);
    response.data[0] = width == 1 ? 0x4f : width == 2 ? 0x4b : 0x43;
    std::memcpy(response.data + 1, request.data + 1, 3);
    for (unsigned i = 0; i < 4; ++i)
        response.data[4 + i] = static_cast<uint8_t>(raw >> (8 * i));
    d.push(response);
    d.pending.txDone = d.pending.transmitted = true;
    d.pending.serial = d.serial;
    d.active = false;
}
E::SmokeConfig smokeConfig() {
    E::SmokeConfig value;
    value.nodes[0] = 1;
    value.nodes[1] = 2;
    value.options.queueTimeoutUs = value.options.responseTimeoutUs = 100;
    value.options.identityWidths = L::IdentityWidths::MEASURED_U8;
    value.readWindowUs = 2000;
    value.identity = "synthetic-image";
    return value;
}
// The finite shared-image check: passive until the one trigger, then exactly one start and
// one identity read per node, a final marker, and nothing more for the rest of the boot.
void smokeCheck() {
    for (unsigned variant = 0; variant < 2; ++variant) {
        Rig r(true, false);
        CHECK(r.firmware.prepare(silentConfig(E::SilentLevel::HIGH)));
        E::FirmwareSmoke smoke(smokeConfig());
        const E::FirmwareTurnHook hook = smoke.hook();
        uint64_t now = 0;
        const auto step = [&]() {
            r.bytes.now = now++;
            r.firmware.service();
            hook.turn(hook.context, r.firmware);
        };
        r.bytes.script = "xg\r\n";
        for (unsigned i = 0; i < 20; ++i)
            step();
        CHECK(smoke.step() == E::SmokeStep::WAITING && smoke.ignoredBytes() == 4);
        CHECK(r.driver.starts == 0 && r.driver.submits == 0 && r.pin.count == 1);
        // While passive the image prints exactly one line: its identity.
        CHECK(std::strcmp(r.bytes.out, "SMOKE ready synthetic-image\n") == 0);
        r.bytes.script = "G";
        unsigned answered = 0;
        while (smoke.step() != E::SmokeStep::DONE && now < 5000) {
            step();
            // Variant 1: node 2 never answers its first field; its budget ends the read.
            if (r.driver.active && !r.driver.pending.txDone && (variant == 0 || answered < 9)) {
                answerIdentity(r.driver);
                ++answered;
            }
        }
        for (unsigned i = 0; i < 64; ++i)
            step(); // Drain the console queue.
        CHECK(smoke.step() == E::SmokeStep::DONE);
        CHECK(r.driver.starts == 1 && r.pin.count == 2 && !r.pin.calls[1].high);
        CHECK(r.driver.submits == (variant == 0 ? 18u : 10u) && answered == (variant ? 9u : 18u));
        CHECK(std::strstr(r.bytes.out, "SMOKE image synthetic-image start=ok running=1 "
                                       "transceiver=normal-requested check=0\n"));
        CHECK(std::strstr(r.bytes.out, "SMOKE node=1 identity=ok device=00002000 hw=000c/1 "
                                       "sw=000c/1 entries=4 vendor=00002019"));
        if (variant == 0)
            CHECK(std::strstr(r.bytes.out, "SMOKE node=2 identity=ok"));
        else
            CHECK(std::strstr(r.bytes.out, "SMOKE node=2 identity=failed state=3") &&
                  std::strstr(r.bytes.out, "uncertain=1"));
        CHECK(std::strstr(r.bytes.out, "SMOKE done; no further requests; ignored=4\n"));
        // A later trigger finds the check finished: no restart, request or retry.
        const unsigned submits = r.driver.submits;
        r.bytes.script = "GG";
        for (unsigned i = 0; i < 200; ++i)
            step();
        CHECK(r.driver.starts == 1 && r.driver.submits == submits && smoke.ignoredBytes() == 6);
    }
    {
        Rig r(true, false); // An unconfigured check refuses before any start.
        CHECK(r.firmware.prepare(silentConfig(E::SilentLevel::HIGH)));
        E::FirmwareSmoke smoke{E::SmokeConfig()};
        const E::FirmwareTurnHook hook = smoke.hook();
        r.bytes.script = "G";
        for (uint64_t now = 0; now < 64; ++now) {
            r.bytes.now = now;
            r.firmware.service();
            hook.turn(hook.context, r.firmware);
        }
        CHECK(smoke.step() == E::SmokeStep::DONE && r.driver.starts == 0 && r.pin.count == 1);
        CHECK(std::strcmp(r.bytes.out, "SMOKE ready identity=unconfigured\n"
                                       "SMOKE refused: fixture nodes/budgets/identity not "
                                       "configured; no start\n") == 0);
    }
}
// The smoke check's failure paths: a refused start, a progression failure mid-read and a
// console without room. None retries, and a full console never delays CAN service.
void smokeFailures() {
    const auto turn = [](Rig& r, const E::FirmwareTurnHook& hook, uint64_t now) {
        r.bytes.now = now;
        const bool serviced = r.firmware.service();
        hook.turn(hook.context, r.firmware);
        return serviced;
    };
    {
        Rig r(true, false); // The controller start is refused.
        r.driver.startOk = false;
        CHECK(r.firmware.prepare(silentConfig(E::SilentLevel::HIGH)));
        E::FirmwareSmoke smoke(smokeConfig());
        const E::FirmwareTurnHook hook = smoke.hook();
        r.bytes.script = "G";
        for (uint64_t now = 0; now < 64; ++now)
            turn(r, hook, now);
        CHECK(smoke.step() == E::SmokeStep::DONE && r.driver.starts == 1 && r.driver.submits == 0);
        CHECK(std::strcmp(r.bytes.out, "SMOKE ready synthetic-image\n"
                                       "SMOKE image synthetic-image start=failed running=0 "
                                       "transceiver=silent-requested check=0\n") == 0);
        CHECK(r.firmware.owner().resources().inhibited);
        r.bytes.script = "G"; // A second trigger finds the check finished.
        for (uint64_t now = 64; now < 128; ++now)
            turn(r, hook, now);
        CHECK(r.driver.starts == 1 && r.driver.submits == 0 && smoke.ignoredBytes() == 1);
    }
    {
        Rig r(true, false); // The clock fails while node 1's first field is in flight.
        CHECK(r.firmware.prepare(silentConfig(E::SilentLevel::HIGH)));
        E::FirmwareSmoke smoke(smokeConfig());
        const E::FirmwareTurnHook hook = smoke.hook();
        r.bytes.script = "G";
        uint64_t now = 0;
        while (r.driver.submits == 0 && now < 64)
            turn(r, hook, now++);
        CHECK(r.driver.submits == 1 && smoke.step() == E::SmokeStep::READING && smoke.node() == 0);
        r.bytes.clockOk = false;
        for (unsigned i = 0; i < 40; ++i)
            CHECK(!turn(r, hook, now++));
        CHECK(smoke.step() == E::SmokeStep::DONE && r.driver.submits == 1 && !r.firmware.running());
        CHECK(r.firmware.failure() == E::FirmwareFailure::CLOCK && r.firmware.faultRetired());
        CHECK(std::strstr(r.bytes.out, "SMOKE node=1 stopped: controller not running failure=1\n"));
        CHECK(!std::strstr(r.bytes.out, "SMOKE node=2") && !std::strstr(r.bytes.out, "SMOKE done"));
    }
    {
        Rig r(true, false); // The console has no room: the check waits, CAN service does not.
        CHECK(r.firmware.prepare(silentConfig(E::SilentLevel::HIGH)));
        E::FirmwareSmoke smoke(smokeConfig());
        const E::FirmwareTurnHook hook = smoke.hook();
        uint8_t filler[E::ProbePlatform::OUTPUT_CAPACITY];
        std::memset(filler, '.', sizeof filler);
        const auto fill = [&]() {
            const size_t used = r.firmware.platform().stats().outputUsed;
            CHECK(r.firmware.platform().writeOutput(filler, sizeof filler - used));
        };
        r.bytes.blocked = true;
        fill();
        r.bytes.script = "G";
        uint64_t now = 0;
        for (; now < 50; ++now)
            turn(r, hook, now);
        // The ready line is still waiting, so the trigger has not even been looked at.
        CHECK(smoke.step() == E::SmokeStep::WAITING && r.driver.starts == 0 && smoke.ignoredBytes() == 0);
        r.bytes.blocked = false;
        unsigned answered = 0;
        bool refilled = false, held = false;
        while (smoke.step() != E::SmokeStep::DONE && now < 5000) {
            turn(r, hook, now++);
            if (r.driver.active && !r.driver.pending.txDone) {
                answerIdentity(r.driver);
                ++answered;
            }
            if (!refilled && r.driver.submits == 1) {
                // Node 1 is reading: take the console away until its result line is pending.
                refilled = true;
                r.bytes.blocked = true;
                fill();
            }
            if (refilled && r.bytes.blocked && smoke.node() == 1) {
                // Node 1 finished while the console was full; node 2 is not admitted until
                // its line is written, but every turn still services the owner.
                for (unsigned i = 0; i < 200; ++i)
                    CHECK(turn(r, hook, now++));
                CHECK(r.driver.submits == 9 && answered == 9 && smoke.step() == E::SmokeStep::READING);
                held = true;
                r.bytes.blocked = false;
            }
        }
        for (unsigned i = 0; i < 128; ++i)
            turn(r, hook, now++); // Drain the console queue.
        CHECK(refilled && held && smoke.step() == E::SmokeStep::DONE);
        CHECK(r.driver.starts == 1 && r.driver.submits == 18 && answered == 18);
        CHECK(std::strstr(r.bytes.out, "SMOKE node=1 identity=ok") &&
              std::strstr(r.bytes.out, "SMOKE node=2 identity=ok") &&
              std::strstr(r.bytes.out, "SMOKE done; no further requests; ignored=0\n"));
    }
}
int main() {
    startup();
    clockBoundariesAndRetirement();
    typedReadUnderPressure();
    saturationAndExplicitRecovery();
    firmwareEvidenceAdmissionAndCollection();
    firmwareEvidenceFailuresNeverReplay();
    failureReportsTheRealReason();
    preflightReachesTheOwner();
    boardChecks();
    silentTransceiver();
    smokeCheck();
    smokeFailures();
    bootStatus();
    closedPathsInvalidateTheOwner();
    std::printf("firmware_probe_test: %u checks; sizeof FirmwareProbe=%zu, CanBusOwner=%zu, "
                "TwaiAdapter=%zu, ProbeApp=%zu, ProbePlatform=%zu, SdoClient=%zu\n",
                checks, sizeof(E::FirmwareProbe), sizeof(O::CanBusOwner), sizeof(E::TwaiAdapter),
                sizeof(E::ProbeApp), sizeof(E::ProbePlatform), sizeof(C::SdoClient));
}
