// SPDX-License-Identifier: MIT
// Integration uses the ordinary owner/service receive path and synthetic wire traffic.
#include "FakeTransport.h"
#include "ProbeApp.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace M = MotorControlCAN;
namespace C = MotorControlCAN::CANOPEN;
namespace L = MotorControlCAN::LICHUAN_C;
namespace E = MotorControlCANExample;
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
template <class T> struct Saved {
    unsigned char bytes[sizeof(T)];
    explicit Saved(const T& value) { std::memcpy(bytes, &value, sizeof value); }
    void unchanged(const T& value) const { CHECK(!std::memcmp(bytes, &value, sizeof value)); }
};
struct Bytes {
    uint8_t input[256] = {}, output[512] = {};
    unsigned inputCount = 0, inputRead = 0, outputCount = 0;
    unsigned readCalls = 0, writeCalls = 0, idleCalls = 0, clockCalls = 0;
    uint64_t now = 0;
    bool blocked = false, clockValid = true;
    static bool clock(void* context, uint64_t& output) {
        Bytes& bytes = *static_cast<Bytes*>(context);
        ++bytes.clockCalls;
        if (!bytes.clockValid) {
            output = 12345; // A failed platform clock cannot publish callback scratch.
            return false;
        }
        output = bytes.now;
        return true;
    }
    static bool read(void* context, uint8_t& output) {
        Bytes& bytes = *static_cast<Bytes*>(context);
        ++bytes.readCalls;
        if (bytes.inputRead == bytes.inputCount)
            return false;
        output = bytes.input[bytes.inputRead++];
        return true;
    }
    static bool write(void* context, uint8_t value) {
        Bytes& bytes = *static_cast<Bytes*>(context);
        ++bytes.writeCalls;
        if (bytes.blocked || bytes.outputCount == sizeof bytes.output)
            return false;
        bytes.output[bytes.outputCount++] = value;
        return true;
    }
    static void idle(void* context) { ++static_cast<Bytes*>(context)->idleCalls; }
    E::ProbeCallbacks callbacks() {
        E::ProbeCallbacks result;
        result.context = this;
        result.nowUs = clock;
        result.readByte = read;
        result.writeByte = write;
        result.idle = idle;
        return result;
    }
};
L::ReadOptions options() {
    L::ReadOptions value;
    value.queueTimeoutUs = 50;
    value.responseTimeoutUs = 20;
    return value;
}
M::Frame heartbeat(uint8_t state) {
    M::Frame value;
    value.id = 0x705;
    value.dlc = value.payloadLength = 1;
    value.data[0] = state;
    return value;
}
struct Rig {
    C::SdoClient services;
    E::FakeTransport fake;
    Bytes bytes;
    E::ProbePlatform platform;
    E::CanBusOwner owner;
    E::ProbeApp app;
    uint64_t now = 0;
    explicit Rig(E::FakeTransport::Mode mode = E::FakeTransport::Mode::SUCCESS)
        : fake(mode), platform(bytes.callbacks()), owner(services, fake.callbacks(), 126),
          app(owner, platform) {
        const L::ReadTarget target = E::FakeTransport::target();
        // Explicit fixture premise, not bootstrap evidence from the real drive.
        CHECK(services.configure(0, target.channel, true, true));
        C::NetworkRoute network;
        network.binding = target.channel.binding;
        network.generation = target.nodeGeneration;
        network.heartbeatId = 0x705;
        network.emcyId = 0x85;
        CHECK(services.configureNetwork(0, network, C::NetworkObserver(), 0));
    }
    void select() { CHECK(app.select(E::FakeTransport::target())); }
    void turn(uint64_t time) {
        CHECK(fake.setTime(time));
        now = bytes.now = time;
        const unsigned reads = bytes.readCalls, writes = bytes.writeCalls;
        CHECK(app.service(time));
        CHECK(bytes.readCalls - reads <= E::ProbePlatform::INPUT_BUDGET);
        CHECK(bytes.writeCalls - writes <= E::ProbePlatform::OUTPUT_BUDGET);
    }
    void finish(bool inspectLifetime = false) {
        for (unsigned i = 0; i < 100 && app.readContext().state == M::ReadState::ACTIVE; ++i) {
            turn(now + 1);
            const E::RequestId id = app.pendingRequest();
            const E::Completion* terminal = owner.result(id);
            if (!terminal)
                continue;
            const Saved<E::Completion> retained(*terminal);
            CHECK(owner.result(id) == terminal);
            if (inspectLifetime) {
                const unsigned transmitted = fake.transmitted();
                CHECK(!app.releaseRead());
                CHECK(!app.select(E::FakeTransport::target()));
                CHECK(!app.requestIdentity(now, now + 1000, options()));
                turn(now + 1);
                CHECK(owner.result(id) == terminal && fake.transmitted() == transmitted);
                retained.unchanged(*terminal);
            }
            CHECK(app.advanceRead(now));
            CHECK(!owner.result(id) && !owner.release(id));
        }
        CHECK(app.readContext().state != M::ReadState::ACTIVE);
    }
};
void startupAndSuccess() {
    Rig rig;
    CHECK(!rig.app.selected() && !rig.fake.transmitCalls() && !rig.fake.receiveCalls());
    CHECK(!rig.app.requestIdentity(0, 1000, options()));
    rig.select();
    E::Health health;
    CHECK(rig.app.health(health) && health.state == E::DriverState::UNINIT);
    L::IdentityObservation identity;
    identity.deviceType = 0xffffffff;
    const Saved<L::IdentityObservation> noIdentity(identity);
    CHECK(!rig.app.getIdentity(0, 1000, identity));
    noIdentity.unchanged(identity);
    CHECK(!rig.fake.transmitCalls() && !rig.fake.receiveCalls());
    rig.turn(0);
    CHECK(!rig.fake.transmitCalls()); // Startup service alone has no work.
    const L::ReadOptions invalid;
    CHECK(!rig.app.requestIdentity(0, 1000, invalid));
    CHECK(!rig.app.requestIdentity(0, 0, options()));
    CHECK(rig.app.requestIdentity(0, 1000, options()));
    CHECK(!rig.fake.transmitCalls()); // Preparation does not hand off CAN.
    rig.finish(true);
    CHECK(rig.app.readContext().state == M::ReadState::SUCCEEDED);
    CHECK(rig.fake.transmitted() == 9 && rig.owner.resources().retained == 0);
    CHECK(rig.app.getIdentity(rig.now, 1000, identity));
    CHECK(identity.deviceType == 0x12345678 && identity.hardwareVersion == 0x1122 &&
          identity.softwareVersion == 0x3344 && identity.identityEntries == 3 &&
          identity.vendorId == 0xa1b2c3d4 && identity.productCode == 0x55667788 &&
          identity.revision == 0x99aabbcc && identity.modelCode == 0x4567 &&
          identity.firmwareCode == 0x6789 && !identity.modelMatched);
    CHECK(rig.services.node(0)->heartbeat.present && rig.services.node(0)->emcyCount == 9);
    CHECK(rig.services.node(0)->emcy.errorCode == 0x2310);
    CHECK(rig.owner.stats().rxFrames == 36 && rig.owner.stats().unmatchedFrames == 9);
    CHECK(rig.fake.observed() == 36);
    CHECK(rig.app.health(health) && health.totalSuccess == 9 && health.totalFailures == 0);
    const unsigned transmitted = rig.fake.transmitCalls();
    CHECK(!rig.app.requestConfig(rig.now, rig.now + 1000, options()));
    L::IdentityObservation copied;
    CHECK(rig.app.getIdentity(rig.now, 1000, copied));
    CHECK(rig.fake.transmitCalls() == transmitted);
    CHECK(rig.app.releaseRead());
    CHECK(!rig.app.getIdentity(rig.now, 1000, copied));
    CHECK(identity.deviceType == 0x12345678); // Owned copy survives context release.
    CHECK(rig.app.requestConfig(rig.now, rig.now + 1000, options()));
    rig.finish();
    L::ConfigObservation config;
    CHECK(rig.app.getConfig(rig.now, 1000, config));
    CHECK(config.syncCobId == 0x80 && config.emcyCobId == 0x85 && config.producerTimeMs == 250 &&
          config.sdoEntries == 2 && config.clientToServer == 0x605 &&
          config.serverToClient == 0x585);
    const Saved<L::ConfigObservation> kept(config);
    CHECK(!rig.app.getConfig(rig.now + 1001, 1000, config));
    kept.unchanged(config);
    CHECK(rig.fake.transmitted() == 15 && rig.app.releaseRead());
}
void failureAndTimeout() {
    Rig rejected(E::FakeTransport::Mode::ABORT);
    rejected.select();
    CHECK(rejected.app.requestIdentity(0, 1000, options()));
    rejected.finish(true);
    const L::ReadContext& read = rejected.app.readContext();
    CHECK(read.state == M::ReadState::FAILED && read.outcome == M::ReadOutcome::PROTOCOL_REJECTED);
    CHECK(read.evidencePresent && read.evidence.abortValid &&
          read.evidence.abortCode == 0xfedcba98u);
    CHECK(read.evidence.outcome == C::SdoOutcome::SERVER_ABORT && !read.values[0].valid);
    CHECK(rejected.fake.transmitted() == 1 && rejected.app.releaseRead());
    Rig silent(E::FakeTransport::Mode::SILENT);
    silent.select();
    CHECK(silent.app.requestIdentity(0, 1000, options()));
    silent.finish();
    CHECK(silent.app.readContext().state == M::ReadState::FAILED);
    CHECK(silent.app.readContext().evidence.outcome == C::SdoOutcome::TIMEOUT);
    CHECK(silent.app.readContext().uncertain && silent.fake.transmitted() == 1);
    CHECK(silent.owner.stats().timeouts == 1 && silent.services.node(0)->heartbeat.present);
    CHECK(silent.app.releaseRead());
}
void pressureAndBytes() {
    Rig rig;
    rig.select();
    rig.bytes.blocked = true;
    rig.bytes.inputCount = 128;
    for (unsigned i = 0; i < rig.bytes.inputCount; ++i)
        rig.bytes.input[i] = static_cast<uint8_t>(i);
    uint8_t output[E::ProbePlatform::OUTPUT_CAPACITY];
    for (unsigned i = 0; i < sizeof output; ++i)
        output[i] = static_cast<uint8_t>(i);
    CHECK(rig.platform.writeOutput(output, sizeof output));
    CHECK(!rig.platform.writeOutput(output, 1));
    CHECK(!rig.platform.writeOutput(nullptr, 1));
    CHECK(rig.platform.writeOutput(nullptr, 0));
    CHECK(rig.app.requestIdentity(0, 1000, options()));
    rig.finish();
    CHECK(rig.app.readContext().state == M::ReadState::SUCCEEDED && rig.fake.transmitted() == 9);
    CHECK(rig.platform.stats().inputUsed == 64 && rig.platform.stats().inputDropped == 64);
    CHECK(rig.platform.stats().outputUsed == 256 && rig.platform.stats().outputRejected == 1);
    CHECK(rig.bytes.outputCount == 0 && rig.bytes.idleCalls > 0);
    // Full input drops newest; embedded zero and FIFO byte ownership survive.
    for (unsigned i = 0; i < 64; ++i) {
        uint8_t value = 255;
        CHECK(rig.platform.takeInput(value) && value == i);
    }
    uint8_t empty = 123;
    CHECK(!rig.platform.takeInput(empty) && empty == 123);
    rig.bytes.blocked = false;
    rig.turn(rig.now + 1);
    CHECK(rig.bytes.outputCount == 16 && rig.platform.stats().outputUsed == 240);
    // Original copied submission cannot change when its input is overwritten.
    std::memset(output, 0xee, sizeof output);
    while (rig.platform.stats().outputUsed)
        rig.turn(rig.now + 1);
    for (unsigned i = 0; i < 256; ++i)
        CHECK(rig.bytes.output[i] == static_cast<uint8_t>(i));
    CHECK(rig.fake.transmitted() == 9 && rig.app.releaseRead());
}
void timeAndCallbacks() {
    E::ProbePlatform missing{E::ProbeCallbacks()};
    uint64_t time = 77;
    CHECK(!missing.nowUs(time) && time == 77);
    missing.service();
    uint8_t empty = 9;
    CHECK(!missing.takeInput(empty) && empty == 9);
    Rig rig;
    CHECK(rig.platform.nowUs(time) && time == 0);
    rig.bytes.clockValid = false;
    time = 77;
    CHECK(!rig.platform.nowUs(time) && time == 77);
    rig.select();
    rig.turn(10);
    const unsigned receives = rig.fake.receiveCalls(), idle = rig.bytes.idleCalls;
    CHECK(!rig.app.service(9));
    CHECK(rig.fake.receiveCalls() == receives && rig.bytes.idleCalls == idle);
    CHECK(!rig.fake.setTime(9) && rig.fake.nowUs() == 10);
    CHECK(!rig.app.requestConfig(9, 1000, options()));
}
void cancellationAndQueueCutoff() {
    Rig rig;
    rig.select();
    CHECK(rig.app.requestIdentity(0, 1000, options()));
    rig.turn(0); // Admit after owner service; request is queued and has not crossed TX.
    const E::RequestId id = rig.app.pendingRequest();
    CHECK(id.owner && !rig.fake.transmitted());
    CHECK(rig.owner.cancel(id, 10) == E::Cancel::CANCELLED);
    const E::Completion* terminal = rig.owner.result(id);
    CHECK(terminal && terminal->reason == E::CompletionReason::CANCELLED);
    const Saved<E::Completion> retained(*terminal);
    CHECK(!rig.app.advanceRead(0) && rig.owner.result(id) == terminal);
    retained.unchanged(*terminal);
    CHECK(rig.app.advanceRead(10) && !rig.owner.result(id));
    CHECK(rig.app.readContext().outcome == M::ReadOutcome::CANCELLED);
    CHECK(!rig.app.readContext().uncertain && rig.app.releaseRead());
    Rig delayed;
    delayed.select();
    CHECK(delayed.app.requestConfig(0, 1000, options()));
    delayed.turn(0);
    const E::RequestId queued = delayed.app.pendingRequest();
    delayed.turn(50); // Exact initial queue cutoff, before the overall cutoff.
    CHECK(delayed.fake.transmitted() == 0);
    CHECK(delayed.owner.result(queued)->reason == E::CompletionReason::QUEUE_EXPIRED);
    CHECK(delayed.app.advanceRead(50));
    CHECK(delayed.app.readContext().evidence.outcome == C::SdoOutcome::TIMEOUT);
    CHECK(!delayed.app.readContext().uncertain && delayed.app.releaseRead());
}
void resetInvalidatesCurrentEvidence() {
    Rig complete;
    complete.select();
    CHECK(complete.app.requestIdentity(0, 1000, options()));
    complete.finish();
    L::IdentityObservation identity;
    CHECK(complete.app.getIdentity(complete.now, 1000, identity));
    const Saved<L::IdentityObservation> kept(identity);
    CHECK(complete.fake.push(heartbeat(0)));
    complete.turn(complete.now + 1);
    CHECK(!complete.app.getIdentity(complete.now, 1000, identity));
    kept.unchanged(identity);
    CHECK(complete.app.readContext().values[0].valid); // Retained history is not currentness.
    CHECK(complete.app.releaseRead());
    CHECK(!complete.app.requestIdentity(complete.now, complete.now + 1000, options()));
    CHECK(complete.fake.transmitted() == 9); // Never requalify/retry automatically.
    Rig pending;
    pending.select();
    CHECK(pending.app.requestConfig(0, 1000, options()));
    pending.turn(0);
    const E::RequestId id = pending.app.pendingRequest();
    CHECK(pending.fake.push(heartbeat(0)));
    pending.turn(1);
    CHECK(pending.owner.result(id) && !pending.fake.transmitted());
    CHECK(pending.app.advanceRead(1));
    CHECK(pending.app.readContext().state == M::ReadState::FAILED && pending.app.releaseRead());
}
void fakeBoundsAndAtomicRefusals() {
    E::FakeTransport fake;
    E::Transport callbacks = fake.callbacks();
    M::Frame request;
    request.id = 0x605;
    request.dlc = request.payloadLength = 8;
    request.data[0] = 0x40;
    request.data[2] = 0x10;
    C::SdoTxId id;
    // Malformed/unknown uploads and all writes are refused before queue mutation.
    request.data[0] = 0x2b;
    CHECK(!callbacks.transmit(&fake, request, E::TransportTxId::fromSdo(id), 1));
    CHECK(!fake.ingressUsed() && !fake.reportsUsed() && !fake.transmitted());
    request.data[0] = 0x40;
    request.data[1] = 0xff;
    CHECK(!callbacks.transmit(&fake, request, E::TransportTxId::fromSdo(id), 1));
    request.data[1] = 0;
    for (unsigned i = 0; i < E::FakeTransport::INGRESS_CAPACITY - 3; ++i)
        CHECK(fake.push(heartbeat(5)));
    const unsigned ingress = fake.ingressUsed();
    CHECK(!callbacks.transmit(&fake, request, E::TransportTxId::fromSdo(id), 1));
    CHECK(fake.ingressUsed() == ingress && !fake.reportsUsed() && !fake.transmitted());
    while (fake.ingressUsed()) {
        E::ReceivedFrame value;
        CHECK(callbacks.receive(&fake, value));
    }
    E::ReceivedFrame empty;
    const Saved<E::ReceivedFrame> kept(empty);
    CHECK(!callbacks.receive(&fake, empty));
    kept.unchanged(empty);
    // Fill report storage while explicitly draining only synthetic ingress.
    for (unsigned i = 0; i < E::FakeTransport::REPORT_CAPACITY; ++i) {
        CHECK(callbacks.transmit(&fake, request, E::TransportTxId::fromSdo(id), 1));
        E::ReceivedFrame value;
        while (fake.ingressUsed())
            CHECK(callbacks.receive(&fake, value));
    }
    CHECK(!callbacks.transmit(&fake, request, E::TransportTxId::fromSdo(id), 1));
    CHECK(!fake.ingressUsed() && fake.reportsUsed() == E::FakeTransport::REPORT_CAPACITY);
    for (unsigned i = 0; i < E::FakeTransport::REPORT_CAPACITY; ++i) {
        E::TxReport report;
        CHECK(callbacks.takeTxReport(&fake, report) && report.transmitted);
    }
    E::TxReport emptyReport;
    const Saved<E::TxReport> retained(emptyReport);
    CHECK(!callbacks.takeTxReport(&fake, emptyReport));
    retained.unchanged(emptyReport);
    for (unsigned i = 0; i < E::FakeTransport::INGRESS_CAPACITY; ++i)
        CHECK(fake.push(heartbeat(5)));
    CHECK(!fake.push(heartbeat(5)) && fake.ingressUsed() == E::FakeTransport::INGRESS_CAPACITY);
}
} // namespace
int main() {
    startupAndSuccess();
    failureAndTimeout();
    pressureAndBytes();
    timeAndCallbacks();
    cancellationAndQueueCutoff();
    resetInvalidatesCurrentEvidence();
    fakeBoundsAndAtomicRefusals();
    std::printf("probe_app: %u checks passed (synthetic only)\n", checks);
}
