// SPDX-License-Identifier: MIT
// Synthetic owner/controller reports only; no Lichuan synchronous behavior is qualified.
#include "ProbeConsole.h"
#include "ProbeSync.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

namespace M = MotorControlCAN;
namespace C = M::CANOPEN;
namespace L = M::LICHUAN_C;
namespace O = MotorControlCANOwner;
namespace E = MotorControlCANExample;
namespace {
#define CHECK(condition)                                                                           \
    do {                                                                                           \
        if (!(condition)) {                                                                        \
            std::fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #condition);                     \
            std::abort();                                                                          \
        }                                                                                          \
    } while (false)

struct Wire {
    unsigned transmitted = 0;
    bool reportPending = false, holdReport = false, ingressPending = false;
    O::TxReport report;
    static bool receive(void* context, O::ReceivedFrame& output) {
        auto& self = *static_cast<Wire*>(context);
        if (!self.ingressPending)
            return false;
        self.ingressPending = false;
        output = O::ReceivedFrame();
        output.frame.id = 0x80;
        return true;
    }
    static bool transmit(void* context, const M::Frame& frame, O::TransportTxId id,
                         uint32_t epoch) {
        auto& self = *static_cast<Wire*>(context);
        CHECK(id.kind == O::TransportTxId::Kind::SYNC);
        CHECK(frame.id == 0x80 && frame.dlc == 0 && frame.payloadLength == 0);
        CHECK(!self.reportPending);
        ++self.transmitted;
        self.report = O::TxReport();
        self.report.id = id;
        self.report.controllerEpoch = epoch;
        self.report.transmitted = true;
        self.reportPending = true;
        return true;
    }
    static bool takeReport(void* context, O::TxReport& output) {
        auto& self = *static_cast<Wire*>(context);
        if (!self.reportPending || self.holdReport)
            return false;
        output = self.report;
        self.reportPending = false;
        return true;
    }
    O::Transport callbacks() {
        O::Transport value;
        value.context = this;
        value.receive = receive;
        value.transmit = transmit;
        value.takeTxReport = takeReport;
        return value;
    }
};
struct Bytes {
    std::string output;
    bool blocked = false;
    static bool write(void* context, uint8_t byte) {
        auto& self = *static_cast<Bytes*>(context);
        if (self.blocked)
            return false;
        self.output += static_cast<char>(byte);
        return true;
    }
    E::ProbeCallbacks callbacks() {
        E::ProbeCallbacks value;
        value.context = this;
        value.writeByte = write;
        return value;
    }
};
struct Rig {
    C::SdoClient services;
    Wire wire;
    Bytes bytes;
    E::ProbePlatform platform;
    O::CanBusOwner owner;
    E::ProbeApp app;
    E::ProbeSync sync;
    E::ProbeConsole console;
    uint64_t now = 0;
    Rig() : platform(bytes.callbacks()), owner(services, wire.callbacks(), 193),
            app(owner, platform), sync(owner), console(app, owner, platform) {
        CHECK(console.attachSync(sync));
        L::ReadTarget target;
        target.channel.binding.logicalAxisId = 9;
        target.channel.binding.endpoint.profile = 1;
        target.channel.binding.endpoint.address = 5;
        target.channel.binding.bindingGeneration = 1;
        target.channel.binding.configurationGeneration = 1;
        target.channel.clientToServerId = 0x605;
        target.channel.serverToClientId = 0x585;
        target.nodeGeneration = 1;
        CHECK(services.configure(0, target.channel, true, true));
        C::NetworkRoute route;
        route.binding = target.channel.binding;
        route.generation = 1;
        route.heartbeatId = 0x705;
        route.emcyId = 0x85;
        CHECK(services.configureNetwork(0, route, C::NetworkObserver(), 0));
        CHECK(app.select(target));
        E::HostInterfaceFacts facts;
        facts.interfaceKnown = facts.bitrateKnown = true;
        facts.bitrateBitsPerSecond = 1000000;
        CHECK(app.setHostInterfaceFacts(facts));
    }
    void line(const char* input) {
        for (; *input; ++input)
            console.feed(static_cast<uint8_t>(*input), now);
        console.feed('\n', now);
    }
    void turn(uint64_t elapsed = 1) {
        now += elapsed;
        CHECK(app.service(now));
        console.service(now);
        platform.service();
    }
    void drain(unsigned turns = 100) {
        for (unsigned i = 0; i < turns; ++i)
            turn();
    }
    std::string command(const char* input) {
        const size_t begin = bytes.output.size();
        line(input);
        drain();
        return bytes.output.substr(begin);
    }
};

void contains(const std::string& text, const char* expected) {
    if (text.find(expected) == std::string::npos)
        std::fprintf(stderr, "Missing [%s] in %s\n", expected, text.c_str());
    CHECK(text.find(expected) != std::string::npos);
}
void passiveAndGrammar() {
    Rig r;
    r.drain();
    CHECK(r.wire.transmitted == 0 && !r.owner.syncArmed() && !r.sync.retained());
    contains(r.command("@1 sync inspect"), "\"armed\":false");
    contains(r.command("@2 sync once"), "\"ok\":false");
    CHECK(!r.sync.retained());
    contains(r.command("@3 sync arm"), "\"ok\":true");
    const char* invalid[] = {"@4 sync", "@4 sync once extra", "@4 sync period", "@4 sync period 10",
                             "@4 sync period 0 1", "@4 sync period 10 0", "@4 sync period -1 2",
                             "@4 sync period 1 4294967296", "@4 sync period 4294967296 1",
                             "@4 sync period 1 1 extra", "@4 sync arm 128", "@4 sync resolve 0"};
    for (const char* input : invalid)
        contains(r.command(input), "\"ok\":false");
    CHECK(!r.sync.retained() && r.wire.transmitted == 0);
}
void finiteAndRetained() {
    Rig r;
    contains(r.command("@1 sync arm"), "\"ok\":true");
    contains(r.command("@2 sync period 10 3"), "\"operation_id\":1");
    CHECK(r.wire.transmitted == 1 && r.sync.active());
    r.turn(9900);
    r.drain();
    CHECK(r.wire.transmitted == 2);
    r.turn(9900);
    r.drain();
    CHECK(r.wire.transmitted == 3 && !r.sync.active());
    const std::string result = r.command("@3 result 1");
    contains(result, "\"outcome\":\"LOCAL_TX\"");
    contains(result, "\"local_tx_count\":3");
    contains(result, "\"drive_effect_known\":false");
    contains(r.command("@4 sync once"), "\"ok\":false");
    contains(r.command("@5 release 1"), "\"ok\":true");
    CHECK(!r.sync.retained());
    contains(r.command("@6 sync once"), "\"result\":\"accepted\"");
    CHECK(r.wire.transmitted == 4 && !r.sync.active());
}
void deadlineAndCancel() {
    Rig r;
    contains(r.command("@1 sync arm"), "\"ok\":true");
    r.line("@2 sync period 10 2");
    r.turn(10000); // First application slot was never yielded; no catch-up TX.
    r.drain();
    CHECK(r.wire.transmitted == 0 && !r.sync.active());
    contains(r.command("@3 result 1"), "MISSED_DEADLINE");
    contains(r.command("@4 release 1"), "\"ok\":true");
    r.line("@5 sync period 10 2");
    r.line("@6 cancel 2");
    r.drain();
    CHECK(r.wire.transmitted == 0);
    contains(r.command("@7 result 2"), "CANCELLED");
    contains(r.command("@8 release 2"), "\"ok\":true");
    r.line("@9 sync period 10 2");
    r.turn();
    r.turn();
    contains(r.command("@10 sync disarm"), "\"motor_stop\":false");
    CHECK(!r.owner.syncArmed() && !r.sync.active());
    contains(r.command("@11 result 3"), "CANCELLED");
}
void pressureAndConflict() {
    Rig r;
    contains(r.command("@1 sync arm"), "\"ok\":true");
    r.bytes.blocked = true;
    r.line("@2 sync once");
    r.drain(300);
    CHECK(r.wire.transmitted == 1 && !r.sync.active());
    CHECK(r.sync.retained() && r.console.retainsOperation());
    r.line("@3 release 1"); // Terminal delivery is required before result storage release.
    CHECK(r.sync.retained());
    r.line("@99 sync disarm"); // Reserved control capacity still permits local disarm under pressure.
    CHECK(!r.owner.syncArmed());
    r.bytes.blocked = false;
    r.drain(500);
    contains(r.command("@4 result 1"), "LOCAL_TX");
    contains(r.command("@5 release 1"), "\"ok\":true");
    r.wire.ingressPending = true;
    r.turn();
    CHECK(r.owner.syncConflict() && !r.owner.syncArmed());
    contains(r.command("@6 sync arm"), "\"ok\":false");
    contains(r.command("@7 sync once"), "\"ok\":false");
    contains(r.command("@8 sync resolve 12"), "\"armed\":false");
    CHECK(!r.owner.syncConflict() && !r.owner.syncArmed());
    CHECK(r.wire.transmitted == 1); // Resolving never silently arms or sends.
}
void lateReportAndBusOff() {
    Rig r;
    contains(r.command("@1 sync arm"), "\"ok\":true");
    r.wire.holdReport = true;
    r.line("@2 sync once");
    r.drain();
    CHECK(r.wire.transmitted == 1 && r.sync.active() && r.owner.resources().syncTxPending);
    r.turn(10000);
    r.drain();
    CHECK(!r.sync.active() && !r.owner.syncArmed());
    contains(r.command("@3 result 1"), "MISSED_DEADLINE");
    CHECK(r.sync.context().transmissionPossible && r.sync.context().transmittedCount == 0);
    contains(r.command("@4 release 1"), "\"ok\":true");
    CHECK(r.owner.resources().syncTxPending); // Releasing result storage never retires controller TX.
    contains(r.command("@5 sync arm"), "\"ok\":false");
    r.wire.holdReport = false;
    r.drain();
    CHECK(!r.owner.resources().syncTxPending && !r.owner.syncArmed());
    CHECK(r.sync.context().outcome == C::SyncOutcome::MISSED_DEADLINE);
    CHECK(r.sync.context().transmittedCount == 0 && r.wire.transmitted == 1);
    contains(r.command("@6 sync arm"), "\"ok\":true");
    r.line("@7 sync period 10 2");
    r.turn();
    r.turn();
    CHECK(r.owner.reportController(C::ControllerState::BUS_OFF, r.now));
    r.drain();
    CHECK(!r.owner.syncArmed() && !r.sync.active());
    contains(r.command("@8 result 2"), "BUS_OFF");
    CHECK(r.wire.transmitted == 2);
}
void betweenSlotLifecycle() {
    for (unsigned scenario = 0; scenario < 4; ++scenario) {
        Rig r;
        contains(r.command("@1 sync arm"), "\"ok\":true");
        r.command("@2 sync period 10 2");
        CHECK(r.wire.transmitted == 1 && r.sync.active());
        CHECK(r.sync.context().state == C::SyncState::ACTIVE);
        C::SyncOutcome expected = C::SyncOutcome::BUS_OFF;
        if (scenario == 0) {
            CHECK(r.owner.reportController(C::ControllerState::BUS_OFF, r.now));
        } else if (scenario == 1) {
            const uint32_t oldArm = r.owner.syncArmGeneration();
            CHECK(r.owner.disarmSync(r.now));
            CHECK(r.owner.armSync(0x80, r.now));
            CHECK(r.owner.syncArmGeneration() > oldArm);
            expected = C::SyncOutcome::INVALIDATED;
        } else if (scenario == 2) {
            r.wire.ingressPending = true;
            expected = C::SyncOutcome::PRODUCER_CONFLICT;
        } else {
            C::SyncWork external;
            external.available = true;
            external.generation = 999;
            external.dueUs = r.now;
            external.expiresUs = r.now + 1;
            external.frame.id = 0x80;
            O::SyncRequestId id;
            CHECK(r.owner.admitSync(external, r.now, id) == O::BusAdmission::ACCEPTED);
            // This other request expires before any transport budget is used.
            O::ServiceBudget noWork;
            CHECK(r.owner.service(r.now + 1, noWork));
            const auto* terminal = r.owner.syncResult(id);
            CHECK(terminal && terminal->service.outcome == C::SyncOutcome::MISSED_DEADLINE);
            CHECK(r.owner.releaseSync(id));
            expected = C::SyncOutcome::INVALIDATED;
        }
        r.turn();
        CHECK(!r.sync.active() && r.sync.context().outcome == expected);
        CHECK(r.sync.context().transmittedCount == 1 && r.sync.context().transmissionPossible);
        r.turn(10000);
        CHECK(r.wire.transmitted == 1); // A later explicit arm cannot resume the old schedule.
    }
}
void ownerTerminalBeforeCancel() {
    for (uint32_t count = 1; count <= 2; ++count) {
        Rig r;
        contains(r.command("@1 sync arm"), "\"ok\":true");
        r.line(count == 1 ? "@2 sync once" : "@2 sync period 10 2");
        r.turn(); // Core yields; owner admits.
        r.turn(); // Owner hands off.
        ++r.now;
        CHECK(r.app.service(r.now)); // Actual TX terminal exists, helper has not consumed it.
        CHECK(r.sync.context().transmittedCount == 0 && r.wire.transmitted == 1);
        r.line("@3 cancel 1");
        CHECK(r.sync.context().transmittedCount == 1);
        CHECK(r.sync.context().outcome == (count == 1 ? C::SyncOutcome::LOCAL_TX
                                                     : C::SyncOutcome::CANCELLED));
        r.drain();
        CHECK(r.wire.transmitted == 1 && !r.sync.active());
        if (count == 1)
            contains(r.bytes.output, "not_cancellable");
    }
}
void helperTimeBoundaries() {
    {
        Rig r;
        CHECK(r.owner.armSync(0x80, 0));
        CHECK(r.sync.request(10000, 1, 0));
        CHECK(r.sync.service(0)); // Queued, with no controller handoff yet.
        O::ServiceBudget noWork;
        CHECK(r.owner.service(1, noWork));
        CHECK(!r.sync.cancel(0));
        CHECK(r.sync.context().state == C::SyncState::WAITING_TX);
        CHECK(r.sync.context().outcome == C::SyncOutcome::NONE);
        CHECK(r.sync.context().awaitingCompletion && r.sync.context().lastNowUs == 0);
        // A timely retry cancels the actual queued request before it can transmit.
        CHECK(r.sync.cancel(1));
        CHECK(r.sync.service(1));
        O::ServiceBudget send;
        send.tx = 1;
        CHECK(r.owner.service(2, send));
        CHECK(r.wire.transmitted == 0 && !r.sync.active());
        CHECK(r.sync.context().outcome == C::SyncOutcome::CANCELLED);
        CHECK(!r.sync.context().awaitingCompletion && r.sync.release());
    }
    {
        Rig r;
        CHECK(r.owner.armSync(0x80, 0));
        CHECK(r.sync.request(10000, 1, 0));
        CHECK(r.sync.service(0));
        O::ServiceBudget send;
        send.tx = 1;
        CHECK(r.owner.service(1, send));
        O::ServiceBudget report;
        report.events = 1;
        CHECK(r.owner.service(2, report)); // A terminal exists at a later owner time.
        CHECK(r.wire.transmitted == 1 && r.sync.context().transmittedCount == 0);
        CHECK(r.sync.service(1).code == M::Err::INVALID_CONFIG);
        CHECK(r.sync.context().state == C::SyncState::WAITING_TX);
        CHECK(r.sync.context().awaitingCompletion && r.sync.context().lastNowUs == 0);
        CHECK(r.sync.context().transmittedCount == 0);
        CHECK(r.sync.service(2));
        CHECK(r.sync.context().state == C::SyncState::SUCCEEDED);
        CHECK(r.sync.context().transmittedCount == 1 && !r.sync.active());
        CHECK(r.sync.release());
    }
}
} // namespace
int main() {
    passiveAndGrammar();
    finiteAndRetained();
    deadlineAndCancel();
    pressureAndConflict();
    lateReportAndBusOff();
    betweenSlotLifecycle();
    ownerTerminalBeforeCancel();
    helperTimeBoundaries();
    std::puts("Finite SYNC shared console passed");
}
