// SPDX-License-Identifier: MIT
// The attended bench routes through the real firmware assembly, owner, SDO engine, typed reads and
// AxisSession, against a synthetic one-frame driver and two synthetic drives. Every value, word
// response and movement here is manufactured by this fake: nothing shows what a CL86-C does.
#include "BenchRoute.h"
#include <MotorControlCAN/profiles/lichuan_c/State.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <string>
#include <vector>

namespace M = MotorControlCAN;
namespace C = M::CANOPEN;
namespace L = M::LICHUAN_C;
namespace E = MotorControlCANExample;
namespace O = MotorControlCANOwner;
namespace {
unsigned checks = 0;
const char* scenario = "";
#define CHECK(c)                                                                                   \
    do {                                                                                           \
        ++checks;                                                                                  \
        if (!(c)) {                                                                                \
            std::fprintf(stderr, "%s:%d: %s [%s]\n", __FILE__, __LINE__, #c, scenario);            \
            std::abort();                                                                          \
        }                                                                                          \
    } while (false)

} // namespace
#include "support/BenchDrive.h"
namespace {
using namespace BenchDrive;
struct Rig {
    C::SdoClient services;
    Console console;
    Driver driver;
    E::AcquisitionRecord acquisitionStorage;
    E::FirmwareProbe firmware;
    E::AxisSession one, two;
    E::RouteRecord routeStorage;
    E::BenchRoute route;
    E::RouteWork routeWork;
    const C::SdoTransfer* externalWord = nullptr; // Explicit additional test producer, scoped below.
    unsigned samples = 0;
    bool positionVelocity = true;
    static void sample(void* context, E::RouteResources& output) {
        Rig& r = *static_cast<Rig*>(context);
        ++r.samples;
        output.stackFree = 9000 - r.samples;
        output.heapFree = 60000;
        output.heapMinimum = 55000;
        output.txErrors = r.samples;
        output.rxErrors = 10 + r.samples;
    }
    static E::RouteCallbacks hooks(Rig* rig) {
        E::RouteCallbacks value;
        value.context = rig;
        value.sample = sample;
        return value;
    }
    // work: attach RouteWork, as the firmware does on the first route that needs it.
    explicit Rig(bool work = true)
        : firmware(services, driver.callbacks(), console.callbacks(), &acquisitionStorage), one(firmware.owner()),
          two(firmware.owner()), route(firmware, services, hooks(this), &routeStorage) {
        E::DownloadPreflight check;
        check.context = this;
        check.check = [](void* context, O::SequenceId sequence, const C::SdoTransfer& transfer,
                         uint64_t now) {
            Rig& r = *static_cast<Rig*>(context);
            if (r.externalWord && E::sameDownload(*r.externalWord, transfer) &&
                r.one.ownsDownload(sequence, transfer))
                return true;
            C::SdoTransfer other = transfer;
            other.data[0] ^= 1;
            CHECK(!r.route.preflight(sequence, other, now));
            // main.cpp's dispatcher: the routes, then the H record, the one other download producer here.
            return r.route.preflight(sequence, transfer, now);
        };
        firmware.setPreflight(check);
        driver.console = &console;
        driver.drives[0].node = 1;
        driver.drives[1].node = 2;
        E::FirmwareConfig config;
        config.configured = true;
        config.controller.txPin = 17;
        config.controller.rxPin = 16;
        config.controller.bitrate = 1000000;
        config.controller.sdkTxQueueDepth = 1;
        config.controller.rxQueueDepth = 16;
        CHECK(firmware.begin(config));
        for (unsigned node = 1; node <= 2; ++node) {
            C::SdoChannel channel;
            channel.binding.logicalAxisId = node;
            channel.binding.endpoint.profile = 1;
            channel.binding.endpoint.address = node;
            channel.binding.bindingGeneration = channel.binding.configurationGeneration = 1;
            channel.clientToServerId = 0x600 + node;
            channel.serverToClientId = 0x580 + node;
            // As the firmware's ensureRoute: an exclusive fresh session, not yet confirmed.
            CHECK(services.configure(uint8_t(node - 1), channel, false, true));
            C::NetworkRoute network;
            network.binding = channel.binding;
            network.generation = 1;
            network.heartbeatId = 0x700 + node;
            network.emcyId = 0x80 + node;
            CHECK(services.configureNetwork(uint8_t(node - 1), network, C::NetworkObserver(), console.now));
        }
        CHECK(route.attach(1, one) && route.attach(2, two));
        CHECK(!work || (route.attachWork(routeWork) && !route.attachWork(routeWork)));
        CHECK(!route.attach(1, two));
    }
    // main.cpp's order: the firmware turn, the K and H records, then the route.
    void turn() {
        console.now += 1000;
        Drive& d = driver.drives[0];
        if (d.mode == 1 && d.moveReads && positionVelocity)
            d.velocity = 5;
        else if (d.mode == 1 && (d.lastWord == 0x000b || d.lastWord == 0x010f))
            d.velocity = 0;
        driver.deliver();
        firmware.service();
        route.service(firmware.owner().lastTimeUs());
    }
    void turns(unsigned count) {
        for (unsigned i = 0; i < count; ++i)
            turn();
    }
    // A pre-operational heartbeat: the exact NMT state ordinary SDO writes need on this ingress.
    void heartbeat(unsigned node) {
        M::Frame beat = frame(0x700 + node, 1);
        beat.data[0] = 0x7f;
        driver.push(beat);
        turns(3);
    }
    const char* start(const std::string& text) {
        E::RoutePlan plan;
        CHECK(E::parseRouteCommand(text.data(), text.size(), plan));
        return route.begin(plan, console.now);
    }
    const E::RouteRecord& run(const std::string& text, unsigned limit = 80000) {
        const char* refusal = start(text);
        if (refusal)
            std::fprintf(stderr, "refused %s: %s\n", text.c_str(), refusal);
        CHECK(!refusal);
        for (unsigned i = 0; i < limit && route.active(); ++i)
            turn();
        CHECK(!route.active() && route.record() && route.record()->terminal);
        if (std::getenv("ROUTE_DEBUG"))
            std::fprintf(stderr, "%s: failure=%u why=%s step=%u cycles=%u terminals=%u\n%s", text.c_str(),
                         unsigned(route.record()->failure), route.record()->why, unsigned(route.record()->step),
                         unsigned(route.record()->cycles), unsigned(route.record()->terminalCount),
                         dump().c_str());
        return *route.record();
    }
    std::string dump() const {
        std::string out;
        char line[256];
        for (unsigned cursor = 0; route.formatLine(cursor, line, sizeof line); ++cursor) {
            const size_t size = std::strlen(line);
            CHECK(size > 1 && size < 250 && line[size - 1] == '\n');
            CHECK(std::strncmp(line, "OUTPUT_FORMAT_ERROR", 19) != 0);
            out += line;
        }
        CHECK(out.rfind("END trial ", std::string::npos) != std::string::npos);
        return out;
    }
    void release() {
        turns(5);
        CHECK(route.release());
        CHECK(!route.record());
    }
    E::AxisSession& session(unsigned node) { return node == 2 ? two : one; }
    unsigned downloadsFrom(size_t first) const {
        unsigned count = 0;
        for (size_t i = first; i < driver.requests.size(); ++i)
            count += driver.requests[i].data[0] != 0x40 && driver.requests[i].data[0] != 0x80;
        return count;
    }
};


std::string transcript;
void enabled(Rig& r) {
    CHECK(r.run("M1,1,1,1,100").failure == E::RouteFailure::NONE);
    r.release(); r.heartbeat(1);
    CHECK(r.run("E1,2,1,1,1,32823,5000,120,10000,800").failure == E::RouteFailure::NONE);
    r.release();
    r.driver.drives[0].travelReads = 100;
    r.driver.drives[0].releaseWords = true;
}
std::string line(unsigned selected, unsigned duration = 2000, unsigned cycles = 4, unsigned after = 1) {
    const unsigned value = selected == 1 || selected == 5 ? 20 : selected == 2 || selected == 3 ? 500 : 0;
    const unsigned target = selected >= 6 ? 7000 : 0;
    return "p1,3,1,0,5000,30,1000,1000," + std::to_string(selected) + "," + std::to_string(after) + "," +
           std::to_string(value) + "," + std::to_string(target) + "," + std::to_string(duration) + "," +
           std::to_string(cycles) + ",100,2,1500,1500,500";
}
std::vector<M::Frame> writes(const Rig& r, size_t from) {
    std::vector<M::Frame> result;
    for (size_t i = from; i < r.driver.requests.size(); ++i)
        if (r.driver.requests[i].data[0] != 0x40 && r.driver.requests[i].data[0] != 0x80)
            result.push_back(r.driver.requests[i]);
    return result;
}
uint32_t raw(const M::Frame& frame) {
    return uint32_t(frame.data[4]) | uint32_t(frame.data[5]) << 8 | uint32_t(frame.data[6]) << 16 |
           uint32_t(frame.data[7]) << 24;
}
void testEveryCase() {
    for (unsigned selected = 1; selected <= 8; ++selected) {
        scenario = "finite-position-change-case";
        std::unique_ptr<Rig> rig(new Rig);
        enabled(*rig);
        const size_t from = rig->driver.requests.size();
        const uint64_t begin = rig->console.now;
        const E::RouteRecord& record = rig->run(line(selected));

        CHECK(record.failure == E::RouteFailure::NONE && record.changeTriggerPossible && record.changeStarted);
        CHECK(record.stopPathRequested == uint8_t(E::StopPath::CONFIRMED));
        CHECK(record.stopRequested && record.changeStopDueUs == begin + 2000000 &&
              record.changeStopEndUs == begin + 3500000 && record.completedUs <= record.changeStopEndUs);
        CHECK(record.terminalCount <= E::ROUTE_TERMINALS && record.cycles == 4);
        const std::vector<M::Frame> output = writes(*rig, from);
        CHECK(output.size() >= 7 && indexOf(output[0]) == 0x607a && raw(output[4]) == 0x001f);
        CHECK(indexOf(output.back()) == 0x6040 && raw(output.back()) == 0x000b);
        const unsigned expected = selected <= 4 || selected == 8 ? 1 : selected == 5 ? 4 : 3;
        CHECK(record.changeWrites == expected && output.size() == 6 + expected);
        if (selected == 4 || selected == 5 || selected == 6 || selected == 7)
            CHECK(indexOf(output[5]) == 0x6040 && raw(output[5]) == 0x000f);
        if (selected == 5 || selected == 6 || selected == 7)
            CHECK(raw(output[output.size() - 2]) == (selected == 7 ? 0x001fu : 0x003fu));
        if (selected == 8) CHECK(indexOf(output[5]) == 0x607a && raw(output[5]) == 7000);
        CHECK(rig->dump().find("TRIAL_CHANGE possible=1 started=1") != std::string::npos);
        transcript += rig->dump();
    }
}
void testDumpEndSurvivesTailGrowth() {
    scenario = "end-marker-is-a-snapshot";
    std::unique_ptr<Rig> rig(new Rig); enabled(*rig);
    rig->run(line(1));
    char output[256];
    bool last = false;
    unsigned cursor = 0;
    while (rig->route.formatLine(cursor, output, sizeof output, &last) && !last)
        ++cursor;
    CHECK(last && std::strncmp(output, "END trial ", 10) == 0);
    // Backpressure leaves this cursor unchanged. Reformatting it is still the
    // same pending line until the output queue accepts it.
    bool repeatedLast = false;
    CHECK(rig->route.formatLine(cursor, output, sizeof output, &repeatedLast) && repeatedLast);
    const unsigned tail = rig->route.record()->tailCount;
    rig->heartbeat(2); // RX continues after the terminal and grows the retained tail.
    CHECK(rig->route.record()->tailCount == tail + 1);
    bool movedEnd = false;
    CHECK(rig->route.formatLine(cursor + 1, output, sizeof output, &movedEnd) && movedEnd);
    // This reproduces the old duplicate END: cursor alone is not completion.
    // The formatter's original flag remains true after RX; main stops only
    // after the corresponding complete output line has queued successfully.
    CHECK(last && repeatedLast);
    bool shortLine = true;
    CHECK(rig->route.formatLine(cursor + 1, output, 1, &shortLine) && !shortLine && output[0] == 0);
}
void testFailedChangeStillStops() {
    for (unsigned lost = 0; lost != 2; ++lost) {
        scenario = lost ? "unconfirmed-change-still-stops" : "aborted-change-still-stops";
        std::unique_ptr<Rig> rig(new Rig); enabled(*rig);
        const size_t from = rig->driver.requests.size();
        CHECK(!rig->start(line(1)));
        while (rig->route.active() && rig->route.record()->cycles == 0) rig->turn();
        CHECK(rig->route.record()->cycles == 1);
        Drive& d = rig->driver.drives[0];
        if (lost) d.silentIndex = 0x6081; else d.abortIndex = 0x6081;
        for (unsigned i = 0; i < 10000 && rig->route.active(); ++i) rig->turn();
        const E::RouteRecord& r = *rig->route.record();
        CHECK(r.terminal && r.failure == E::RouteFailure::SERVICE && r.changeStarted && r.changeWrites == 0);
        CHECK(r.stopRequested && r.completedUs <= r.changeStopEndUs &&
              r.stopPathRequested != uint8_t(E::StopPath::CONFIRMED));
        const std::vector<M::Frame> output = writes(*rig, from);
        CHECK(output.size() == 7 && indexOf(output[5]) == 0x6081 && raw(output.back()) == 0x000b);
    }
}

void testMismatchedParameterAckDoesNotRestore() {
    scenario = "mismatched-bare-parameter-ack-keeps-fallback";
    std::unique_ptr<Rig> rig(new Rig); enabled(*rig);
    CHECK(!rig->start(line(1)));
    while (rig->route.active() && rig->route.record()->cycles == 0) rig->turn();
    rig->driver.drives[0].mismatchIndex = 0x6081;
    for (unsigned i = 0; i < 10000 && rig->route.active(); ++i) rig->turn();
    const E::RouteRecord& r = *rig->route.record();
    CHECK(r.terminal && r.failure == E::RouteFailure::SERVICE && r.changeWrites == 0 &&
          r.stopRequested && r.stopPathRequested != uint8_t(E::StopPath::CONFIRMED));
}
void testParameterEpochChangeDoesNotRestore() {
    scenario = "bare-parameter-boot-invalidates-control";
    std::unique_ptr<Rig> rig(new Rig); enabled(*rig);
    CHECK(!rig->start(line(1)));
    while (rig->route.active() && !rig->route.record()->changeStarted) rig->turn();
    M::Frame boot = frame(0x701, 1); boot.data[0] = 0;
    rig->driver.push(boot);
    for (unsigned i = 0; i < 10000 && rig->route.active(); ++i) rig->turn();
    CHECK(rig->route.record()->terminal && rig->route.record()->failure != E::RouteFailure::NONE);
    CHECK(!rig->one.latestControl() && rig->route.record()->changeWrites == 0 &&
          rig->route.record()->stopPathRequested != uint8_t(E::StopPath::CONFIRMED));
}

void holdInitialTrigger(Rig& rig) {
    enabled(rig);
    CHECK(rig.run("P1,3,1,0,5000,30,1000,1000,1,100,0,0,0,0,0").failure == E::RouteFailure::NONE);
    rig.release();
    CHECK(rig.one.latestControl() && rig.one.latestControl()->transfer.data[0] == 0x001f && !rig.one.busy());
}
C::SdoTransfer parameter(Rig& rig) {
    C::SdoTransfer transfer;
    transfer.channel = *rig.services.channel(0);
    transfer.deadlineUs = rig.console.now + 5000000;
    transfer.operation = transfer.request = rig.one.beginOperation(transfer.deadlineUs, rig.console.now);
    CHECK(transfer.operation);
    transfer.step = 1; transfer.index = 0x6081; transfer.size = 4;
    transfer.direction = C::SdoDirection::DOWNLOAD;
    transfer.data[0] = 20;
    transfer.queueTimeoutUs = E::ROUTE_QUEUE_US; transfer.responseTimeoutUs = E::ROUTE_RESPONSE_US;
    uint32_t grant = 0, operation = 0;
    CHECK(rig.services.lastExperiment(0, grant, operation));
    CHECK(rig.services.authorizeDownloadExperiment(0, transfer, rig.services.node(0)->route.generation,
                                                   grant + 1, rig.console.now));
    return transfer;
}
void testDirectBareTicketBoundaries() {
    for (unsigned unknown = 0; unknown != 2; ++unknown) {
        scenario = unknown ? "unknown-parameter-cannot-be-resurrected" : "plain-parameter-ack-stays-conservative";
        std::unique_ptr<Rig> rig(new Rig); holdInitialTrigger(*rig);
        C::SdoTransfer transfer = parameter(*rig);
        rig->externalWord = &transfer;
        if (unknown) rig->driver.drives[0].silentIndex = 0x6081;
        CHECK((unknown ? rig->one.admitPositionChangeParameter(transfer, 4, rig->console.now)
                       : rig->one.admitPrepared(transfer, rig->console.now)) == O::BusAdmission::ACCEPTED);
        C::SdoCompletion done;
        bool settled = false;
        for (unsigned i = 0; i < 1000 && !settled; ++i) { rig->turn(); settled = rig->one.settle(done); }
        CHECK(settled && !rig->one.latestControl());
        CHECK(unknown ? done.effectUncertain : done.outcome == C::SdoOutcome::SUCCESS && done.protocolAcknowledged);
        const size_t before = rig->driver.requests.size();
        // Same current operation and otherwise exact shape: the missing prior acknowledged controlword
        // alone must prevent this later explicit call from resurrecting it.
        CHECK(rig->one.admitPositionChangeParameter(transfer, 5, rig->console.now) == O::BusAdmission::INVALID);
        CHECK(rig->driver.requests.size() == before && !rig->one.latestControl());
    }
    {
        scenario = "explicit-ticket-refuses-other-object-shapes";
        std::unique_ptr<Rig> rig(new Rig); holdInitialTrigger(*rig);
        C::SdoTransfer transfer = parameter(*rig);
        const uint32_t prior = rig->one.latestControl()->transfer.operation;
        for (uint16_t index : {uint16_t(0x6040), uint16_t(0x6060), uint16_t(0x60ff), uint16_t(0x2500)}) {
            C::SdoTransfer wrong = transfer; wrong.index = index;
            CHECK(rig->one.admitPositionChangeParameter(wrong, 4, rig->console.now) == O::BusAdmission::INVALID);
        }
        C::SdoTransfer wrong = transfer; wrong.size = 2;
        CHECK(rig->one.admitPositionChangeParameter(wrong, 4, rig->console.now) == O::BusAdmission::INVALID);
        wrong = transfer; wrong.subindex = 1;
        CHECK(rig->one.admitPositionChangeParameter(wrong, 4, rig->console.now) == O::BusAdmission::INVALID);
        wrong = transfer; wrong.direction = C::SdoDirection::UPLOAD;
        CHECK(rig->one.admitPositionChangeParameter(wrong, 4, rig->console.now) == O::BusAdmission::INVALID);
        CHECK(rig->one.admitPositionChangeParameter(transfer, 0, rig->console.now) == O::BusAdmission::INVALID);
        CHECK(rig->one.latestControl() && rig->one.latestControl()->transfer.operation == prior);
    }
    {
        scenario = "stop-supersedes-ticket-before-parameter-ack";
        std::unique_ptr<Rig> rig(new Rig); holdInitialTrigger(*rig);
        C::SdoTransfer transfer = parameter(*rig); rig->externalWord = &transfer;
        CHECK(rig->one.admitPositionChangeParameter(transfer, 4, rig->console.now) == O::BusAdmission::ACCEPTED);
        rig->turn(); // Parameter submitted; its ACK is delivered only after the stop request below.
        CHECK(rig->driver.active && indexOf(rig->driver.current) == 0x6081);
        E::StopPolicy policy;
        policy.deadlineUs = policy.bestEffortDeadlineUs = rig->console.now + 1500000;
        policy.options.service.queueTimeoutUs = E::ROUTE_QUEUE_US;
        policy.options.service.responseTimeoutUs = E::ROUTE_RESPONSE_US;
        policy.options.observationIntervalUs = 100000;
        policy.options.maximumAgeUs = 7 * (uint64_t(E::ROUTE_QUEUE_US) + E::ROUTE_RESPONSE_US);
        L::ReadContext absent;
        CHECK(rig->one.requestStop(L::StopKind::QUICK_STOP, absent, policy, rig->console.now) != E::StopPath::NONE);
        rig->turns(500);
        CHECK(!rig->one.latestControl());
    }
}
void testWindowAndDirectionRefusals() {
    scenario = "invalid-change-plan";
    E::RoutePlan p;
    CHECK(E::parseRouteCommand(line(6).data(), line(6).size(), p));
    p.target = -1; CHECK(!E::routePlanValid(p));
    p.target = 5000; p.replacementTarget = -1; CHECK(!E::routePlanValid(p));
    p.replacementTarget = 7000; p.bestEffortMs = 0; CHECK(!E::routePlanValid(p));
    CHECK(!E::parseRouteCommand(line(1, 2000, 2, 1).data(), line(1, 2000, 2, 1).size(), p));
    {
        scenario = "latched-running-is-not-motion";
        std::unique_ptr<Rig> rig(new Rig); enabled(*rig); rig->positionVelocity = false;
        const E::RouteRecord& r = rig->run(line(6));
        CHECK(r.failure == E::RouteFailure::PREREQUISITE && std::strcmp(r.why, "not-running") == 0 &&
              !r.changeStarted && r.stopRequested && r.last.position < r.plan.target);
    }
    {
        scenario = "curve-ended-before-change";
        std::unique_ptr<Rig> rig(new Rig); enabled(*rig); rig->driver.drives[0].travelReads = 1;
        const E::RouteRecord& r = rig->run(line(6));
        CHECK(r.failure == E::RouteFailure::PREREQUISITE && !r.changeStarted && r.stopRequested);
    }
    {
        scenario = "stop-due-before-change-cycle";
        std::unique_ptr<Rig> rig(new Rig); enabled(*rig);
        const uint64_t begin = rig->console.now;
        CHECK(!rig->start(line(6)));
        while (!rig->one.latestControl() || rig->one.latestControl()->transfer.data[0] != 0x1f)
            rig->turn();
        // The first observation may be pending; its bounded exchange settles before the fixed stop.
        rig->console.now = begin + 2000000;
        for (unsigned i = 0; i < 10000 && rig->route.active(); ++i) rig->turn();
        CHECK(rig->route.record()->terminal && !rig->route.record()->changeStarted &&
              rig->route.record()->stopRequested && rig->route.record()->changeStopDueUs == begin + 2000000);
    }
}

std::string velocityLine(unsigned replacement = 20, unsigned after = 500) {
    return "v1,2,1,10,200,200,12000,5000,16000,1,2,2," + std::to_string(DEVICE_TYPE) + "," +
        std::to_string(HARDWARE) + "," + std::to_string(SOFTWARE) + "," + std::to_string(VENDOR) + "," +
        std::to_string(PRODUCT) + "," + std::to_string(REVISION) + "," + std::to_string(MODEL) + "," +
        std::to_string(FIRMWARE) + ",0,0," + std::to_string(replacement) + "," + std::to_string(after);
}
void testNativeVelocityChangeRoute() {
    scenario = "native-finite-velocity-change";
    std::unique_ptr<Rig> rig(new Rig);
    Drive& d = rig->driver.drives[0];
    d.mode = 3; d.monitor = L::MONITOR_RELEASED; d.status = 0x8050;
    d.releaseWords = d.releasePattern = d.spins = true;
    CHECK(rig->run("A1,1").failure == E::RouteFailure::NONE);
    rig->release();
    const E::RouteRecord& r = rig->run(velocityLine());
    if (r.failure != E::RouteFailure::NONE) std::fprintf(stderr, "%s", rig->dump().c_str());
    CHECK(r.failure == E::RouteFailure::NONE && r.velocityChangeAttempted && r.velocityChangeAccepted);
    const L::VelocityContext& run = rig->one.velocityAction();
    CHECK(run.change.present && run.change.serviceEvidencePresent && !run.change.uncertain);
    CHECK(run.change.evidence.gate == L::VelocityGate::ATTENDED_EXPERIMENT &&
          !run.change.evidence.rateChangeQualified && run.change.rate.effectiveNative == 20);
    CHECK(run.stopDueUs == run.startedUs + 12000000 && run.deadlineUs == run.startedUs + 16000000);
    unsigned targets = 0;
    for (const M::Frame& f : rig->driver.requests)
        if (f.data[0] != 0x40 && f.data[0] != 0x80 && indexOf(f) == 0x60ff) {
            CHECK(raw(f) == (++targets == 1 ? 10u : 20u));
        }
    CHECK(targets == 2 && r.terminalCount <= E::ROUTE_TERMINALS);
    CHECK(rig->dump().find("TRIAL_VELOCITY_CHANGE rpm=20 after=500 attempted=1 admitted=1 present=1 terminal=1") !=
          std::string::npos);
    transcript += rig->dump();
    E::RoutePlan plan;
    CHECK(!E::parseRouteCommand(velocityLine(0).data(), velocityLine(0).size(), plan));
    CHECK(!E::parseRouteCommand(velocityLine(61).data(), velocityLine(61).size(), plan));
    CHECK(!E::parseRouteCommand(velocityLine(20, 12000).data(), velocityLine(20, 12000).size(), plan));
}


void testVelocityChangeWaitsForFreshRunningCycle() {
    scenario = "long-poll-and-delayed-enable-change-boundary";
    std::unique_ptr<Rig> rig(new Rig);
    Drive& d = rig->driver.drives[0];
    d.mode = 3; d.monitor = L::MONITOR_RELEASED; d.status = 0x8050;
    d.releaseWords = d.releasePattern = d.spins = true;
    d.staleWord = 0x000f; d.staleCycles = 2;
    CHECK(rig->run("A1,1").failure == E::RouteFailure::NONE); rig->release();
    CHECK(!rig->start(velocityLine(20, 5000)));
    while (rig->route.active() && !rig->route.record()->velocityRequested) rig->turn();
    const uint64_t requestedAt = rig->one.velocityAction().startedUs + 5000000;
    while (rig->route.active() && rig->console.now < requestedAt) rig->turn();
    const L::VelocityContext& waiting = rig->one.velocityAction();
    CHECK(waiting.runningObserved && waiting.enablePending == 2 && waiting.runningPolls == 0);
    CHECK(!rig->route.record()->velocityChangeAttempted); // The enable sample is now stale; no failed attempt.
    for (unsigned i = 0; i < 20000 && rig->route.active(); ++i) rig->turn();
    const E::RouteRecord& r = *rig->route.record();
    const L::VelocityContext& run = rig->one.velocityAction();
    CHECK(r.terminal && r.failure == E::RouteFailure::NONE && r.velocityChangeAttempted &&
          r.velocityChangeAccepted && !r.statusPresent);
    CHECK(run.change.preparedUs >= requestedAt &&
          run.change.preparedUs >= run.enableReport.latestUs + run.options.pollIntervalUs);
    CHECK(run.change.serviceEvidencePresent && !run.change.uncertain &&
          run.stopDueUs == run.startedUs + 12000000 && run.deadlineUs == run.startedUs + 16000000);
    unsigned targets = 0;
    for (const M::Frame& f : rig->driver.requests)
        if (f.data[0] != 0x40 && f.data[0] != 0x80 && indexOf(f) == 0x60ff) ++targets;
    CHECK(targets == 2);
}
void testUnconfirmedNativeVelocityChangeRoute() {
    scenario = "lost-native-velocity-change-still-stops";
    std::unique_ptr<Rig> rig(new Rig);
    Drive& d = rig->driver.drives[0];
    d.mode = 3; d.monitor = L::MONITOR_RELEASED; d.status = 0x8050;
    d.releaseWords = d.releasePattern = d.spins = true;
    CHECK(rig->run("A1,1").failure == E::RouteFailure::NONE); rig->release();
    CHECK(!rig->start(velocityLine()));
    while (rig->route.active() && !rig->route.record()->velocityChangeAttempted) rig->turn();
    CHECK(rig->route.record()->velocityChangeAccepted);
    d.silentIndex = 0x60ff; d.writesOnly = true;
    for (unsigned i = 0; i < 20000 && rig->route.active(); ++i) rig->turn();
    const E::RouteRecord& r = *rig->route.record();
    const L::VelocityContext& run = rig->one.velocityAction();
    CHECK(r.terminal && r.failure != E::RouteFailure::NONE && run.change.present && run.change.uncertain);
    CHECK(run.change.serviceEvidencePresent && run.change.serviceEvidence.effectUncertain);
    CHECK((run.stopExecution != M::ActionExecution::NOT_TRANSMITTED ||
           (r.stopRequested && rig->one.bestEffort().admitted)) && r.completedUs <= run.deadlineUs);
    unsigned targets = 0;
    for (const M::Frame& f : rig->driver.requests)
        if (f.data[0] != 0x40 && f.data[0] != 0x80 && indexOf(f) == 0x60ff) ++targets;
    CHECK(targets == 2);
}


void testFailedRetargetHaltFallsBack() {
    scenario = "unconfirmed-retarget-halt-keeps-quick-fallback";
    std::unique_ptr<Rig> rig(new Rig); enabled(*rig);
    std::string request = line(6);
    const size_t kind = request.rfind(",2,1500,1500,500");
    CHECK(kind != std::string::npos); request[kind + 1] = '1';
    const size_t first = rig->driver.requests.size();
    CHECK(!rig->start(request));
    while (rig->route.active() && !rig->route.record()->changeStarted) rig->turn();
    rig->driver.drives[0].silentIndex = 0x607a;
    rig->driver.drives[0].writesOnly = true;
    for (unsigned i = 0; i < 10000 && rig->route.active(); ++i) rig->turn();
    const E::RouteRecord& r = *rig->route.record();
    CHECK(r.terminal && r.failure == E::RouteFailure::SERVICE && r.stopRequested && r.stopRequestKind == 2);
    CHECK(r.changeWrites == 1 && r.completedUs <= r.changeStopEndUs);
    const std::vector<M::Frame> output = writes(*rig, first);
    CHECK(output.size() == 8 && raw(output[5]) == 0x000f && indexOf(output[6]) == 0x607a &&
          raw(output.back()) == 0x000b);
}
void testStaleQueuedPositionChange() {
    scenario = "stale-queued-change-refused-before-transmit";
    std::unique_ptr<Rig> rig(new Rig); enabled(*rig);
    const size_t first = rig->driver.requests.size();
    CHECK(!rig->start(line(6, 10000, 8)));
    while (rig->route.active() && !rig->route.record()->changeStarted) rig->turn();
    CHECK(rig->route.record()->changeStarted && rig->route.record()->changeWrites == 0);
    rig->console.now += 3000000; // Below stop due, beyond the explicit state age limit.
    for (unsigned i = 0; i < 10000 && rig->route.active(); ++i) rig->turn();
    const E::RouteRecord& r = *rig->route.record();
    CHECK(r.terminal && r.failure != E::RouteFailure::NONE && r.stopRequested && r.changeWrites == 0);
    const std::vector<M::Frame> output = writes(*rig, first);
    CHECK(output.size() == 6 && raw(output[4]) == 0x001f && raw(output.back()) == 0x000b);
}
void testLateFallbackDeadline() {
    for (unsigned changing = 0; changing != 2; ++changing)
        for (unsigned expired = 0; expired != 2; ++expired) {
            scenario = expired ? "no-fallback-after-finite-deadline" : "late-fallback-keeps-finite-deadline";
            std::unique_ptr<Rig> rig(new Rig);
            Drive& d = rig->driver.drives[0];
            d.mode = 3; d.monitor = L::MONITOR_RELEASED; d.status = 0x8050;
            d.releaseWords = d.releasePattern = d.spins = true;
            d.silentIndex = 0x6040; d.silentWord = 0x000f;
            CHECK(rig->run("A1,1").failure == E::RouteFailure::NONE); rig->release();
            std::string request = velocityLine();
            if (!changing) { request[0] = 'V'; request.resize(request.size() - std::strlen(",20,500")); }
            CHECK(!rig->start(request));
            for (;;) {
                rig->turn();
                bool finalEnable = false;
                for (const M::Frame& f : rig->driver.requests)
                    finalEnable |= f.data[0] != 0x40 && indexOf(f) == 0x6040 && raw(f) == 0x000f;
                if (finalEnable) break;
                CHECK(rig->route.active());
            }
            const uint64_t deadline = rig->one.velocityAction().deadlineUs;
            const size_t first = rig->driver.requests.size();
            rig->console.now = expired ? deadline + 1000 : deadline - 1000000;
            for (unsigned i = 0; i < 10000 && rig->route.active(); ++i) rig->turn();
            CHECK(rig->route.record()->terminal && rig->one.velocityAction().deadlineUs == deadline);
            const L::BestEffortStop& stop = rig->one.bestEffort();
            if (expired) {
                CHECK(!rig->route.record()->stopRequested && !stop.admitted && writes(*rig, first).empty());
            } else {
                CHECK(rig->route.record()->stopRequested && stop.admitted && stop.deadlineUs <= deadline);
                CHECK(writes(*rig, first).size() == 1 && raw(writes(*rig, first)[0]) == 0x000b);
            }
        }
}
} // namespace
int main(int argc, char** argv) {
    testDumpEndSurvivesTailGrowth();
    testEveryCase();
    testFailedChangeStillStops();
    testMismatchedParameterAckDoesNotRestore();
    testParameterEpochChangeDoesNotRestore();
    testDirectBareTicketBoundaries();
    testWindowAndDirectionRefusals();
    testNativeVelocityChangeRoute();
    testVelocityChangeWaitsForFreshRunningCycle();
    testUnconfirmedNativeVelocityChangeRoute();
    testStaleQueuedPositionChange();
    testLateFallbackDeadline();
    testFailedRetargetHaltFallsBack();
    if (argc == 3 && std::strcmp(argv[1], "--transcript") == 0) {
        std::FILE* file = std::fopen(argv[2], "wb");
        CHECK(file && std::fwrite(transcript.data(), 1, transcript.size(), file) == transcript.size());
        CHECK(std::fclose(file) == 0);
    } else {
        CHECK(argc == 1);
    }
    std::printf("position change: %u checks\n", checks);
}
