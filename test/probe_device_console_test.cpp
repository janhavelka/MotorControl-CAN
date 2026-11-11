// SPDX-License-Identifier: MIT
// The shared console reaches the real application/owner with synthetic wire evidence only.
#include "FakeTransport.h"
#include "FirmwareProbe.h"
#include "ProbeConsole.h"
#include "ProbeDevice.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

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

void contains(const std::string& output, const char* wanted) {
    if (output.find(wanted) == std::string::npos)
        std::fprintf(stderr, "Missing [%s] in:\n%s\n", wanted, output.c_str());
    CHECK(output.find(wanted) != std::string::npos);
}
size_t occurrences(const std::string& output, const char* wanted) {
    size_t count = 0, offset = 0;
    while ((offset = output.find(wanted, offset)) != std::string::npos) {
        ++count;
        ++offset;
    }
    return count;
}
std::string recordWith(const std::string& output, const char* wanted) {
    const size_t at = output.find(wanted);
    CHECK(at != std::string::npos);
    const size_t previous = output.rfind('\n', at);
    const size_t first = previous == std::string::npos ? 0 : previous + 1;
    const size_t last = output.find('\n', at);
    CHECK(last != std::string::npos);
    return output.substr(first, last - first);
}
struct Bytes {
    std::string output;
    bool blocked = false;
    uint64_t now = 0;
    static bool read(void*, uint8_t&) { return false; }
    static bool write(void* context, uint8_t value) {
        Bytes& self = *static_cast<Bytes*>(context);
        if (self.blocked)
            return false;
        self.output += static_cast<char>(value);
        return true;
    }
    static bool clock(void* context, uint64_t& output) {
        output = static_cast<Bytes*>(context)->now;
        return true;
    }
    E::ProbeCallbacks callbacks() {
        E::ProbeCallbacks result;
        result.context = this;
        result.readByte = read;
        result.writeByte = write;
        result.nowUs = clock;
        return result;
    }
};
struct Wire {
    E::FakeTransport fake;
    unsigned nmtCount = 0, stateCount = 0;
    M::Frame lastNmt;
    M::Frame stateReply;
    O::TxReport nmtReport;
    bool reportPending = false, stateReplyPending = false;
    explicit Wire(E::FakeTransport::Mode mode) : fake(mode) {}
    static bool receive(void* p, O::ReceivedFrame& output) {
        Wire& self = *static_cast<Wire*>(p);
        if (self.stateReplyPending) {
            output = O::ReceivedFrame();
            output.frame = self.stateReply;
            output.receivedTimeKnown = true;
            output.receivedUs = self.fake.nowUs();
            self.stateReplyPending = false;
            return true;
        }
        auto& fake = self.fake;
        return fake.callbacks().receive(&fake, output);
    }
    static bool transmit(void* p, const M::Frame& frame, O::TransportTxId id, uint32_t epoch) {
        Wire& self = *static_cast<Wire*>(p);
        const uint16_t index = uint16_t(frame.data[1]) | uint16_t(frame.data[2]) << 8;
        const bool monitor = index >= 0x2100 && index <= 0x2102;
        if (frame.id == 0x605 && frame.payloadLength == 8 && frame.data[0] == 0x40 &&
            (monitor || index == 0x6041 || index == 0x6061) && !self.reportPending &&
            !self.stateReplyPending) {
            // Extra synthetic monitor words use the same ordinary owner path. They are raw bits,
            // deliberately including an unknown high bit; no drive interpretation is qualified.
            const uint16_t raw[] = {0x8001, 0x0041, 0x0005};
            const uint16_t value = monitor ? raw[index - 0x2100] : index == 0x6041 ? 0x8037 : 1;
            self.stateReply = M::Frame();
            self.stateReply.id = 0x585;
            self.stateReply.dlc = self.stateReply.payloadLength = 8;
            self.stateReply.data[0] = index == 0x6061 ? 0x4f : 0x4b;
            self.stateReply.data[1] = frame.data[1];
            self.stateReply.data[2] = frame.data[2];
            self.stateReply.data[3] = frame.data[3];
            self.stateReply.data[4] = uint8_t(value);
            self.stateReply.data[5] = uint8_t(value >> 8);
            self.stateReplyPending = true;
            self.nmtReport = O::TxReport();
            self.nmtReport.id = id;
            self.nmtReport.controllerEpoch = epoch;
            self.nmtReport.transmitted = true;
            self.reportPending = true;
            ++self.stateCount;
            return true;
        }
        if (frame.id == 0 && frame.payloadLength == 2 && !self.reportPending) {
            self.lastNmt = frame;
            ++self.nmtCount;
            self.nmtReport = O::TxReport();
            self.nmtReport.id = id;
            self.nmtReport.controllerEpoch = epoch;
            self.nmtReport.transmitted = true;
            self.reportPending = true;
            return true;
        }
        return self.fake.callbacks().transmit(&self.fake, frame, id, epoch);
    }
    static bool report(void* p, O::TxReport& output) {
        Wire& self = *static_cast<Wire*>(p);
        if (self.reportPending) {
            output = self.nmtReport;
            self.reportPending = false;
            return true;
        }
        return self.fake.callbacks().takeTxReport(&self.fake, output);
    }
    static void observe(void* p, const O::ReceivedFrame& input, uint64_t now) {
        auto& fake = static_cast<Wire*>(p)->fake;
        fake.callbacks().observe(&fake, input, now);
    }
    O::Transport callbacks() {
        O::Transport result;
        result.context = this;
        result.receive = receive;
        result.transmit = transmit;
        result.takeTxReport = report;
        result.observe = observe;
        return result;
    }
};
struct Rig {
    C::SdoClient services;
    Wire wire;
    Bytes bytes;
    E::ProbePlatform platform;
    O::CanBusOwner owner;
    E::ProbeApp app;
    E::DeviceEvidence evidence;
    E::ProbeDevice device;
    E::ProbeConsole console;
    uint64_t now = 0;
    explicit Rig(E::FakeTransport::Mode mode = E::FakeTransport::Mode::SUCCESS)
        : wire(mode), platform(bytes.callbacks()), owner(services, wire.callbacks(), 127),
          app(owner, platform), evidence(app, owner), device(app, owner, services, &evidence),
          console(app, owner, platform) {
        const auto target = E::FakeTransport::target();
        CHECK(services.configure(0, target.channel, true, true));
        C::NetworkRoute route;
        route.binding = target.channel.binding;
        route.generation = target.nodeGeneration;
        route.heartbeatId = 0x705;
        route.emcyId = 0x85;
        CHECK(services.configureNetwork(0, route, C::NetworkObserver(), 0));
        E::HostInterfaceFacts facts;
        facts.interfaceKnown = facts.bitrateKnown = true;
        facts.interfaceId = target.channel.binding.endpoint.bus;
        facts.bitrateBitsPerSecond = 250000;
        CHECK(app.setHostInterfaceFacts(facts));
        console.attachDevice(device);
        CHECK(!app.selected() && wire.fake.transmitCalls() == 0);
    }
    void line(const std::string& command) {
        for (char c : command)
            console.feed(static_cast<uint8_t>(c), now);
        console.feed('\n', now);
    }
    void turn(uint64_t increment = 1) {
        now += increment;
        bytes.now = now;
        CHECK(wire.fake.setTime(now));
        CHECK(app.service(now));
        CHECK(evidence.service(now));
        console.service(now);
    }
    void drain(unsigned count = 1000) {
        for (unsigned i = 0; i < count; ++i)
            turn();
    }
    std::string command(const std::string& command, unsigned turns = 1000) {
        const size_t offset = bytes.output.size();
        line(command);
        drain(turns);
        return bytes.output.substr(offset);
    }
    void select() { contains(command("@1 host 0"), "\"ok\":true"); }
    unsigned attempts() const {
        return wire.fake.transmitCalls() + wire.nmtCount + wire.stateCount;
    }
};

void selectionAndCachedRoutes() {
    Rig r;
    r.select();
    CHECK(r.app.selected()->channel.binding.endpoint.address == 5);
    contains(r.command("@2 useaddr 5 0"), "\"ok\":true");
    const char* bad[] = {"host 2",      "host -1",         "host 0 extra", "useaddr 0",
                         "useaddr 128", "useaddr 5 65536", "useaddr 6",    "useaddr 5 0 extra"};
    for (const char* command : bad) {
        contains(r.command(std::string("@3 ") + command), "\"ok\":false");
        CHECK(r.app.selected()->channel.binding.endpoint.address == 5);
    }
    const char* cached[] = {"network",   "heartbeat", "load",   "debug raw", "debug decoded",
                            "debug off", "status",    "health", "stats",     "drv"};
    for (const char* command : cached)
        contains(r.command(std::string("@4 ") + command), "\"ok\":true");
    contains(r.command("@4 pdo 0"), "\"ok\":false"); // No map is fabricated by inspection.
    CHECK(r.attempts() == 0);
}

void attachmentsRequireTheSameStationaryGraph() {
    Rig local, foreign;
    L::ReadTarget localTarget = E::FakeTransport::target();
    L::ReadTarget foreignTarget = E::FakeTransport::target();
    CHECK(M::sameBinding(localTarget.channel.binding, foreignTarget.channel.binding));
    E::AxisSession localSession(local.owner), foreignSession(foreign.owner);
    E::ProbeSettings localSettings(local.owner, local.services, localSession, localTarget);
    E::ProbeSettings foreignSettings(foreign.owner, foreign.services, foreignSession,
                                     foreignTarget);
    E::ProbeSettings wrongStoreSettings(local.owner, foreign.services, localSession, localTarget);
    E::ProbeSettings wrongSessionSettings(local.owner, local.services, foreignSession, localTarget);
    E::ProbeSettings replacementSettings(local.owner, local.services, localSession, localTarget);
    E::ProbeCommissioning localCommissioning(local.owner, local.services, localSession,
                                             localTarget);
    E::ProbeCommissioning foreignCommissioning(foreign.owner, foreign.services, foreignSession,
                                               foreignTarget);
    E::ProbeCommissioning wrongStoreCommissioning(local.owner, foreign.services, localSession,
                                                  localTarget);
    E::ProbeCommissioning wrongSessionCommissioning(local.owner, local.services, foreignSession,
                                                    localTarget);
    E::ProbeCommissioning replacementCommissioning(local.owner, local.services, localSession,
                                                   localTarget);
    CHECK(!local.console.attachDevice(foreign.device));
    CHECK(local.console.attachDevice(local.device));
    CHECK(!local.console.attachSettings(foreignSettings));
    CHECK(!local.console.attachSettings(wrongStoreSettings));
    CHECK(!local.console.attachSettings(wrongSessionSettings));
    CHECK(local.console.attachSettings(localSettings));
    CHECK(local.console.attachSettings(localSettings));
    CHECK(!local.console.attachSettings(replacementSettings));
    CHECK(!local.console.attachCommissioning(foreignCommissioning));
    CHECK(!local.console.attachCommissioning(wrongStoreCommissioning));
    CHECK(!local.console.attachCommissioning(wrongSessionCommissioning));
    CHECK(local.console.attachCommissioning(localCommissioning));
    CHECK(local.console.attachCommissioning(localCommissioning));
    CHECK(!local.console.attachCommissioning(replacementCommissioning));
    CHECK(local.attempts() == 0 && foreign.attempts() == 0);
    CHECK(!local.app.selected() && !foreign.app.selected());
}

void settingsRejectMismatchedSelectedTargetBeforePreparation() {
    for (unsigned mismatch = 0; mismatch < 3; ++mismatch) {
        Rig rig;
        rig.select();
        L::ReadTarget current = *rig.app.selected();
        if (mismatch == 0) {
            // The real observer advances the node epoch while selection retains its old copy.
            CHECK(rig.services.reboot(5, rig.now));
            current.nodeGeneration = rig.services.node(0)->route.generation;
            CHECK(current.nodeGeneration != rig.app.selected()->nodeGeneration);
        } else if (mismatch == 1) {
            current.channel.clientToServerId = 0x615;
        } else {
            current.channel.serverToClientId = 0x595;
        }
        CHECK(M::sameBinding(current.channel.binding, rig.app.selected()->channel.binding));
        E::AxisSession session(rig.owner);
        CHECK(session.open(current, L::SettingsModel::CL86_C, 1, rig.now));
        E::ProbeSettings settings(rig.owner, rig.services, session, current);
        CHECK(rig.console.attachSettings(settings));
        const uint32_t next = session.nextOperation();
        const std::string reply = rig.command("@701 driver limits 1");
        // This error belongs to routing, before native prerequisite preparation is attempted.
        contains(reply, "Q20_external_fresh_prerequisites_and_handoff_guard_required");
        CHECK(reply.find("\"result\":\"accepted\"") == std::string::npos);
        CHECK(session.nextOperation() == next && !session.busy() && !settings.retained());
        const auto resources = rig.owner.resources();
        CHECK(rig.attempts() == 0 && resources.queued == 0 && resources.active == 0 &&
              resources.retained == 0);
    }
}

void wholePlanReadGateAndArguments() {
    const char* gated[] = {"read config", "read state errors", "read state history",
                           "read state inputs"};
    for (const char* command : gated) {
        Rig r;
        r.select();
        contains(r.command(std::string("@2 ") + command), "\"ok\":false");
        CHECK(r.app.readContext().state == M::ReadState::EMPTY);
        CHECK(r.attempts() == 0 && !r.app.pendingRequest().owner);
    }
    Rig r;
    r.select();
    const char* invalid[] = {"probe 0",
                             "probe -1",
                             "probe 1.5",
                             "probe 1 extra",
                             "read identity nonsense",
                             "read state unknown",
                             "read config 0",
                             "read state status 0",
                             "read state status 1 extra"};
    for (const char* command : invalid)
        contains(r.command(std::string("@2 ") + command), "\"ok\":false");
    CHECK(r.attempts() == 0 && r.app.readContext().state == M::ReadState::EMPTY);
}

void identityValuesAndTerminal() {
    Rig r;
    r.select();
    const std::string response = r.command("@20 read identity");
    contains(response, "\"result\":\"accepted\"");
    contains(response, "\"type\":\"terminal\"");
    contains(response, "\"command_id\":20");
    contains(response, "\"outcome\":\"SUCCESS\"");
    CHECK(occurrences(response, "\"type\":\"terminal\"") == 1);
    CHECK(r.wire.fake.transmitted() == 9);
    L::IdentityObservation identity;
    CHECK(r.app.getIdentity(r.now, 1000000, identity));
    CHECK(identity.vendorId == 0xa1b2c3d4u && identity.revision == 0x99aabbccu);
    // These unsigned values exceed signed 32-bit; JSON must preserve the native values.
    contains(response, "2712847316");
    contains(response, "2578103244");
    CHECK(r.services.node(0)->heartbeat.present && r.services.node(0)->emcyCount == 9);
    const std::string retained = r.command("@21 result 1");
    contains(retained, "\"command_id\":20");
    contains(retained, "SUCCESS");
    contains(r.command("@22 release 1"), "\"ok\":true");
    CHECK(r.app.readContext().state == M::ReadState::EMPTY && r.wire.fake.transmitted() == 9);
}

void rawMonitorWordsRemainUninterpreted() {
    Rig r;
    r.select();
    const std::string output = r.command("@20 read state monitors");
    contains(output, "SUCCESS");
    contains(output, "\"raw\":\"32769\"");
    contains(output, "\"unit\":\"raw-bits\"");
    CHECK(output.find("\"unit\":\"rpm\"") == std::string::npos);
    CHECK(occurrences(output, "\"interpretation\":null") == 3);
    L::StateObservation monitors;
    CHECK(r.app.getStateBlock(L::StateBlock::MONITORS, r.now, 1000000, monitors));
    CHECK(monitors.monitor == 0x8001 && monitors.logicalInputs == 0x0041 &&
          monitors.logicalOutputs == 5);
    CHECK(r.wire.stateCount == 3 && r.wire.fake.transmitCalls() == 0);
}

void boundedMonitoringAndOff() {
    Rig r;
    r.select();
    const std::string output = r.command("@20 monitor 1 2 100", 4000);
    contains(output, "\"result\":\"accepted\"");
    contains(output, "\"type\":\"terminal\"");
    CHECK(!r.device.monitoring());
    CHECK(r.wire.stateCount == 4 && r.wire.fake.transmitted() == 0);
    CHECK(occurrences(output, "\"type\":\"terminal\"") == 1);
    contains(r.command("@21 release 1"), "\"ok\":true");

    Rig stopped;
    stopped.select();
    stopped.line("@20 monitor 1 2 100");
    for (unsigned i = 0; i < 20 && !stopped.wire.stateCount; ++i)
        stopped.turn();
    CHECK(stopped.wire.stateCount == 1);
    stopped.line("@21 monitor off");
    stopped.drain(4000);
    CHECK(!stopped.device.monitoring());
    CHECK(stopped.wire.stateCount == 1 && !stopped.services.unresolved(0));
    CHECK(occurrences(stopped.bytes.output, "\"type\":\"terminal\"") == 1);
}

void monitorOwnsSampleGaps() {
    Rig r;
    r.select();
    r.line("@20 monitor 1 2 100");
    for (unsigned i = 0; i < 100 && r.app.readContext().state != M::ReadState::EMPTY; ++i)
        r.turn();
    CHECK(r.device.monitoring() && r.app.readContext().state == M::ReadState::EMPTY);
    CHECK(r.wire.stateCount == 2);
    const char* competitors[] = {"@30 probe", "@31 host 0", "@32 discover probe 100 0"};
    for (const char* command : competitors) {
        contains(r.command(command, 50), "\"ok\":false");
        CHECK(r.app.readContext().state == M::ReadState::EMPTY);
        CHECK(!r.device.discovery().active());
    }
    CHECK(r.wire.stateCount == 2 && r.wire.fake.transmitted() == 0);
    r.drain(3000);
    CHECK(!r.device.monitoring() && r.wire.stateCount == 4);

    Rig foreign;
    foreign.select();
    foreign.line("@20 monitor 1 2 100");
    for (unsigned i = 0; i < 100 && foreign.app.readContext().state != M::ReadState::EMPTY; ++i)
        foreign.turn();
    CHECK(foreign.device.monitoring() && foreign.app.readContext().state == M::ReadState::EMPTY);
    L::ReadOptions options;
    options.queueTimeoutUs = options.responseTimeoutUs = 10000;
    CHECK(foreign.app.requestProbe(foreign.now, foreign.now + 20000, options));
    const uint32_t operation = foreign.app.readContext().operation;
    CHECK(foreign.device.stopMonitor(foreign.now));
    foreign.drain(100);
    CHECK(foreign.app.readContext().operation == operation);
    CHECK(foreign.app.readContext().state == M::ReadState::ACTIVE);
    CHECK(foreign.owner.result(foreign.app.pendingRequest()));
    CHECK(foreign.wire.fake.transmitted() == 1 && !foreign.services.unresolved(0));
    // The old monitor never consumes this other consumer's result, even while publishing its own.
    CHECK(foreign.app.advanceRead(foreign.now));
    CHECK(foreign.app.readContext().state == M::ReadState::SUCCEEDED);
}

void monitorDeadlineAndGapCancellationRemainDistinct() {
    Rig pressured;
    pressured.select();
    pressured.bytes.blocked = true;
    const uint8_t filler[E::ProbePlatform::OUTPUT_CAPACITY] = {};
    CHECK(pressured.platform.writeOutput(filler, sizeof filler));
    pressured.line("@20 monitor 1 2 5");
    pressured.drain(50);
    CHECK(pressured.app.readContext().state == M::ReadState::SUCCEEDED);
    CHECK(pressured.wire.stateCount == 2); // One sample, with its values waiting behind pressure.
    pressured.turn(10000);
    pressured.bytes.blocked = false;
    pressured.drain(3000);
    contains(pressured.bytes.output, "\"outcome\":\"DEADLINE\"");
    CHECK(pressured.wire.stateCount == 2 && !pressured.device.monitoring());

    Rig cancelled;
    cancelled.select();
    cancelled.line("@20 monitor 1 2 100");
    for (unsigned i = 0; i < 100 && cancelled.app.readContext().state != M::ReadState::EMPTY; ++i)
        cancelled.turn();
    CHECK(cancelled.device.monitoring() &&
          cancelled.app.readContext().state == M::ReadState::EMPTY);
    contains(cancelled.command("@21 monitor off"), "\"outcome\":\"CANCELLED\"");
    CHECK(cancelled.wire.stateCount == 2 && !cancelled.device.monitoring());
}

E::AcquisitionPlan capturePlan(const Rig& r, L::ReadKind kind) {
    E::AcquisitionPlan plan;
    plan.id = 1;
    std::strcpy(plan.fixtureId, "synthetic-console-fixture");
    std::strcpy(plan.softwareRevision, "0123456789012345678901234567890123456789");
    std::strcpy(plan.firmware, "synthetic-no-hardware-qualification");
    std::strcpy(plan.dialect, "raw-unknown");
    std::strcpy(plan.authorization, "finite-host-test");
    std::strcpy(plan.observations, "test-owned-synthetic-wire-events");
    plan.target = *r.app.selected();
    plan.kind = kind;
    plan.options.queueTimeoutUs = plan.options.responseTimeoutUs = 100000;
    plan.deadlineUs = r.now + 1000000;
    return plan;
}

void explicitCaptureUsesExistingCollector() {
    Rig r;
    r.select();
    CHECK(r.device.setCapturePlan(capturePlan(r, L::ReadKind::IDENTITY)));
    contains(r.command("@2 debug raw"), "\"ok\":true");
    const std::string output = r.command("@20 capture-read");
    contains(output, "\"type\":\"terminal\"");
    CHECK(occurrences(output, "\"type\":\"value\"") == 9);
    CHECK(output.find("\"type\":\"value\"") < output.find("\"type\":\"terminal\""));
    contains(output, "\"raw\":\"2712847316\"");
    contains(output, "\"interpretation\":null");
    const auto* record = r.evidence.record();
    CHECK(record && record->terminal && !record->experimental);
    CHECK(record->read.state == M::ReadState::SUCCEEDED && record->terminalCount == 9);
    CHECK(record->trafficCount != 0 && r.wire.fake.transmitted() == 9);
    const std::string terminal = recordWith(output, "\"type\":\"terminal\"");
    contains(terminal, "\"acquisition_id\":1");
    contains(terminal, record->evidenceLost ? "\"evidence_lost\":true" : "\"evidence_lost\":false");
    contains(terminal,
             (std::string("\"traffic_count\":") + std::to_string(record->trafficCount)).c_str());
    contains(
        terminal,
        (std::string("\"dropped_traffic\":") + std::to_string(record->droppedTraffic)).c_str());
    O::Diagnostic copy;
    CHECK(!r.device.diagnostic(copy)); // Capture owns this drain while its record is retained.
    contains(r.command("@21 release 1"), "\"ok\":true");
    CHECK(!r.evidence.record());

    Rig gated;
    gated.select();
    CHECK(gated.device.setCapturePlan(capturePlan(gated, L::ReadKind::CONFIG)));
    contains(gated.command("@20 capture-read"), "\"ok\":false");
    CHECK(gated.attempts() == 0 && !gated.evidence.record());
}

void failedReadKeepsNativeEvidence() {
    Rig abort(E::FakeTransport::Mode::ABORT);
    abort.select();
    const std::string response = abort.command("@10 probe");
    contains(response, "PROTOCOL_REJECTED");
    contains(response, "4275878552");
    CHECK(abort.app.readContext().evidence.abortValid);
    CHECK(abort.app.readContext().evidence.abortCode == 0xfedcba98u);
    CHECK(abort.wire.fake.transmitted() == 1);

    Rig timeout(E::FakeTransport::Mode::SILENT);
    timeout.select();
    timeout.line("@10 probe 10");
    timeout.drain(10);
    CHECK(timeout.wire.fake.transmitted() == 1);
    timeout.turn(20000);
    timeout.drain();
    contains(timeout.bytes.output, "\"type\":\"terminal\"");
    contains(timeout.bytes.output, "\"uncertain\":true");
    CHECK(timeout.app.readContext().state == M::ReadState::FAILED);
    CHECK(timeout.wire.fake.transmitted() == 1); // No retry after an unconfirmed upload.
}

void cancelSettlesInflightWithoutNextField() {
    Rig r;
    r.select();
    r.line("@10 read identity");
    for (unsigned i = 0; i < 20 && !r.wire.fake.transmitted(); ++i)
        r.turn();
    CHECK(r.wire.fake.transmitted() == 1 && r.app.pendingRequest().owner);
    r.line("@11 cancel 1");
    r.drain();
    contains(r.bytes.output, "CANCELLED");
    CHECK(r.app.readContext().state == M::ReadState::FAILED);
    CHECK(r.wire.fake.transmitted() == 1 && !r.services.unresolved(0));
    contains(r.command("@12 release 1"), "\"ok\":true");
    contains(r.command("@13 probe"), "SUCCESS");
    CHECK(r.wire.fake.transmitted() == 2);
}

void resultAndOutputPressureNeverOrphanReads() {
    Rig r;
    r.select();
    contains(r.command("@10 probe"), "SUCCESS");
    // Keep the first console correlation but release its already copied app result, as another
    // cooperative application consumer can. A passive scan then fills the second console slot.
    CHECK(r.app.releaseRead());
    contains(r.command("@11 discover passive 1 0", 2000), "\"type\":\"terminal\"");
    const unsigned before = r.attempts();
    contains(r.command("@12 probe"), "\"ok\":false");
    CHECK(r.app.readContext().state == M::ReadState::EMPTY && r.attempts() == before);

    Rig blocked;
    blocked.select();
    blocked.bytes.blocked = true;
    blocked.line("@20 status");
    blocked.line("@21 status");
    blocked.line("@22 probe");
    CHECK(blocked.app.readContext().state == M::ReadState::EMPTY);
    blocked.drain();
    CHECK(blocked.attempts() == 0);
    blocked.bytes.blocked = false;
    blocked.drain();
}

void staleSelectionCannotAdmit() {
    Rig r;
    r.select();
    CHECK(r.owner.reportController(C::ControllerState::RECOVERING, r.now));
    contains(r.command("@2 probe"), "\"ok\":false");
    CHECK(!r.device.node() && r.app.readContext().state == M::ReadState::EMPTY);
    CHECK(r.attempts() == 0);
}

void discoveryUsesOneOwnerAndCountsFields() {
    Rig passive;
    contains(passive.command("@10 discover passive 5 0", 10), "\"ok\":true");
    CHECK(passive.device.discovery().active() && passive.attempts() == 0);
    passive.turn(5000);
    passive.drain();
    CHECK(passive.device.discovery().complete());
    CHECK(passive.device.discovery().summary().servicesAdmitted == 0);
    CHECK(passive.attempts() == 0);
    contains(passive.command("@11 discover inspect"), "\"ok\":true");
    contains(passive.command("@12 release 1"), "\"ok\":true");

    Rig active;
    const std::string output = active.command("@10 discover identity 1000 0");
    contains(output, "\"ok\":true");
    CHECK(occurrences(output, "\"type\":\"finding\"") == 1);
    CHECK(output.find("\"type\":\"finding\"") < output.find("\"type\":\"terminal\""));
    CHECK(active.device.discovery().complete());
    CHECK(active.device.discovery().summary().servicesAdmitted == 9);
    CHECK(active.wire.fake.transmitted() == 9);
    const auto* found = active.device.discovery().result(0);
    CHECK(found && found->outcome == E::DiscoveryOutcome::GENERIC_IDENTITY);
    const std::string finding = recordWith(output, "\"type\":\"finding\"");
    contains(finding, "\"ordinal\":0");
    contains(finding, "\"address\":5");
    contains(finding, "\"binding_current\":true");
    contains(finding, "\"unresolved\":false");
    contains(finding, "2712847316");
    const std::string expectedOutcome =
        std::string("\"outcome_code\":") + std::to_string(static_cast<unsigned>(found->outcome));
    contains(finding, expectedOutcome.c_str());
    const std::string inspected = active.command("@11 discover inspect 0");
    contains(inspected, "\"address\":5");
    contains(inspected, expectedOutcome.c_str());
    contains(inspected, "\"raw_identity\":[");
    contains(active.command("@12 discover inspect 1"), "\"ok\":false");
    CHECK(active.wire.fake.transmitted() == 9);
}

void passiveDiscoveryNeedsNoSdoSession() {
    Rig r;
    C::NetworkRoute route;
    route.binding = E::FakeTransport::target().channel.binding;
    route.binding.logicalAxisId = 2;
    route.binding.endpoint.address = 6;
    route.generation = 1;
    route.heartbeatId = 0x706;
    route.emcyId = 0x86;
    CHECK(r.services.configureNetwork(1, route, C::NetworkObserver(), 0));
    CHECK(!r.services.channel(1));
    r.line("@10 discover passive 1 1");
    M::Frame heartbeat;
    heartbeat.id = 0x706;
    heartbeat.dlc = heartbeat.payloadLength = 1;
    heartbeat.data[0] = 127;
    CHECK(r.wire.fake.push(heartbeat));
    r.drain(2000);
    const auto* finding = r.device.discovery().result(0);
    CHECK(finding && finding->outcome == E::DiscoveryOutcome::PRESENCE);
    CHECK(finding->confidence == E::DiscoveryConfidence::RESPONDER_ONLY);
    CHECK(!r.services.channel(1) && !r.app.selected() && r.attempts() == 0);
    contains(r.command("@11 release 1"), "\"ok\":true");
    contains(r.command("@12 discover probe 10 1"), "\"ok\":false");
    CHECK(!r.services.channel(1) && r.attempts() == 0);
}

void discoveryBackedCapabilitiesFitWholeRecords() {
    Rig r;
    const std::string before = r.command("@20 profile list");
    contains(before, "\"selected_protocol\":null");
    contains(before, "\"hardware_qualified\":false");
    r.select();
    const std::string profile = r.command("@21 profile list");
    contains(profile, "\"selected_protocol\":\"canopen\"");
    contains(profile, "\"probe\":true");
    contains(profile, "\"identity\":true");
    contains(profile, "\"motor_kind\":null");
    contains(profile, "\"feedback\":null");
    const std::string caps = r.command("@22 caps", 2000);
    const std::string complete = recordWith(caps, "\"result\":\"complete\"");
    contains(complete, "\"endpoint_range\":[1,127]");
    contains(complete, "\"non_changing\":false");
    contains(complete, "\"hardware_qualified\":false");
    const std::string all = before + profile + caps;
    CHECK(all.find("record_capacity") == std::string::npos);
    size_t first = 0;
    while (first < all.size()) {
        const size_t last = all.find('\n', first);
        CHECK(last != std::string::npos && all[first] == '{' && all[last - 1] == '}');
        CHECK(last - first + 1 < E::ProbeConsole::RECORD_CAPACITY);
        first = last + 1;
    }
    CHECK(r.attempts() == 0);
}

void discoveryInterfaceChangePreventsHandoff() {
    for (unsigned turns = 1; turns <= 2; ++turns) {
        Rig r;
        r.line("@10 discover identity 1000 0");
        r.drain(turns);
        CHECK(r.device.discovery().active() && r.app.readContext().state == M::ReadState::ACTIVE);
        CHECK(r.attempts() == 0);
        if (turns == 2)
            CHECK(r.app.pendingRequest().owner); // Already admitted, but not handed to transport.
        auto changed = r.app.hostInterfaceFacts();
        changed.bitrateBitsPerSecond = 500000;
        CHECK(r.app.setHostInterfaceFacts(changed));
        r.drain(100);
        CHECK(r.attempts() == 0 && r.device.discovery().complete());
        CHECK(r.device.discovery().summary().interfaceChanged);
        const auto* found = r.device.discovery().result(0);
        CHECK(found && found->outcome == E::DiscoveryOutcome::INVALIDATED);
    }
}

void decodedDebugReportsOnlyCheckedEvents() {
    Rig r;
    r.select();
    contains(r.command("@2 debug decoded"), "\"ok\":true");
    M::Frame heartbeat;
    heartbeat.id = 0x705;
    heartbeat.dlc = heartbeat.payloadLength = 1;
    heartbeat.data[0] = 5;
    CHECK(r.wire.fake.push(heartbeat));
    M::Frame unknown = heartbeat;
    unknown.data[0] = 3; // Undocumented heartbeat state must stay raw.
    CHECK(r.wire.fake.push(unknown));
    M::Frame unrelated = heartbeat;
    unrelated.id = 0x706;
    CHECK(r.wire.fake.push(unrelated));
    M::Frame emergency;
    emergency.id = 0x85;
    emergency.dlc = emergency.payloadLength = 8;
    emergency.data[0] = 0x10;
    emergency.data[1] = 0x23;
    emergency.data[2] = 1;
    CHECK(r.wire.fake.push(emergency));
    const size_t offset = r.bytes.output.size();
    r.drain(100);
    const std::string output = r.bytes.output.substr(offset);
    const std::string decoded = recordWith(output, "\"event_kind\":1");
    contains(decoded, "\"decoded\":true");
    contains(decoded, "\"raw_nmt\":5");
    const std::string fault = recordWith(output, "\"event_kind\":2");
    contains(fault, "\"decoded\":true");
    contains(fault, "\"emcy_code\":8976");
    contains(fault, "\"error_register\":1");
    const std::string rejected = recordWith(output, "\"payload\":\"03\"");
    contains(rejected, "\"decoded\":false");
    CHECK(rejected.find("\"event_kind\"") == std::string::npos);
    contains(recordWith(output, "\"can_id\":1798"), "\"decoded\":false");
    CHECK(r.services.node(0)->nmt == C::NmtState::OPERATIONAL);
    CHECK(r.services.node(0)->emcyCount == 1 && r.wire.fake.observed() == 4);
    CHECK(r.attempts() == 0); // Diagnostics neither transmit nor steal authoritative receive.
}

void explicitNmtScope() {
    Rig addressed;
    addressed.select();
    contains(addressed.command("@10 nmt stop addressed"), "\"result\":\"accepted\"");
    CHECK(addressed.wire.nmtCount == 1 && addressed.wire.lastNmt.data[0] == 2);
    CHECK(addressed.wire.lastNmt.data[1] == 5 && addressed.wire.fake.transmitCalls() == 0);
    CHECK(!addressed.services.node(0)->nmtTime.present); // Controller TX is not remote state.
    Rig broadcast;
    broadcast.select();
    contains(broadcast.command("@10 nmt preop broadcast"), "\"result\":\"accepted\"");
    CHECK(broadcast.wire.nmtCount == 1 && broadcast.wire.lastNmt.data[0] == 0x80);
    CHECK(broadcast.wire.lastNmt.data[1] == 0);
    Rig invalid;
    invalid.select();
    const char* commands[] = {"nmt stop", "nmt stop broadcast 0", "nmt nonsense addressed",
                              "nmt reset addressed", "nmt stop addressed extra"};
    for (const char* command : commands)
        contains(invalid.command(std::string("@10 ") + command), "\"ok\":false");
    CHECK(invalid.attempts() == 0);
}

struct Driver {
    unsigned starts = 0, stops = 0, submissions = 0;
    static bool start(void* p, const E::TwaiConfig&, uint32_t) {
        ++static_cast<Driver*>(p)->starts;
        return true;
    }
    static bool stop(void* p) {
        ++static_cast<Driver*>(p)->stops;
        return true;
    }
    static E::TwaiSubmit submit(void* p, const M::Frame&, uint32_t) {
        ++static_cast<Driver*>(p)->submissions;
        return E::TwaiSubmit::FAILED_UNSENT;
    }
    static E::TwaiEvents events(void*) { return E::TwaiEvents(); }
    static bool receive(void*, O::ReceivedFrame&, uint64_t) { return false; }
    E::TwaiDriver callbacks() {
        E::TwaiDriver result;
        result.context = this;
        result.start = start;
        result.stop = stop;
        result.submit = submit;
        result.events = events;
        result.receive = receive;
        return result;
    }
};
void inactiveFirmwareRecoveryCannotStartController() {
    for (unsigned stopped = 0; stopped < 2; ++stopped) {
        C::SdoClient services;
        Driver driver;
        Bytes bytes;
        E::FirmwareProbe firmware(services, driver.callbacks(), bytes.callbacks());
        E::FirmwareConfig config;
        config.configured = true;
        config.controller.txPin = 4;
        config.controller.rxPin = 5;
        config.controller.bitrate = 250000;
        config.controller.sdkTxQueueDepth = 1;
        config.controller.rxQueueDepth = 16;
        CHECK(firmware.prepare(config));
        if (stopped)
            CHECK(firmware.start());
        O::RecoveryId id;
        CHECK(firmware.owner().recover(0, 1000, id));
        if (stopped)
            CHECK(firmware.stop());
        bytes.now = 1;
        CHECK(!firmware.service());
        const auto* terminal = firmware.owner().recoveryResult(id);
        CHECK(terminal && terminal->outcome == O::RecoveryOutcome::FAILED);
        CHECK(driver.starts == stopped && driver.submissions == 0);
        CHECK(!firmware.running());
    }
}
} // namespace
int main() {
    selectionAndCachedRoutes();
    attachmentsRequireTheSameStationaryGraph();
    settingsRejectMismatchedSelectedTargetBeforePreparation();
    wholePlanReadGateAndArguments();
    identityValuesAndTerminal();
    rawMonitorWordsRemainUninterpreted();
    boundedMonitoringAndOff();
    monitorOwnsSampleGaps();
    monitorDeadlineAndGapCancellationRemainDistinct();
    explicitCaptureUsesExistingCollector();
    failedReadKeepsNativeEvidence();
    cancelSettlesInflightWithoutNextField();
    resultAndOutputPressureNeverOrphanReads();
    staleSelectionCannotAdmit();
    discoveryUsesOneOwnerAndCountsFields();
    passiveDiscoveryNeedsNoSdoSession();
    discoveryBackedCapabilitiesFitWholeRecords();
    discoveryInterfaceChangePreventsHandoff();
    decodedDebugReportsOnlyCheckedEvents();
    explicitNmtScope();
    inactiveFirmwareRecoveryCannotStartController();
    std::printf("probe device console: %u checks\n", checks);
}
