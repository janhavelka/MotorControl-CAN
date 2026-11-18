// SPDX-License-Identifier: MIT
// Prompt 55's unplug check (LinkWatch, the `U` line) and the link lines (`L`) through the real
// firmware assembly, owner, SDO engine and typed reads, against BenchDrive's synthetic one-frame
// driver. Unplugging is a test input: nothing ACKs the controller's frame. Nothing here shows what a
// CL86-C, a TJA1051 or the TWAI controller does when a connector is pulled.
#include "LinkWatch.h"
#include "AxisSession.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <string>
#include <vector>

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
    E::LinkWatch watch;
    std::string lines;
    Rig() : firmware(services, driver.callbacks(), console.callbacks(), &acquisitionStorage), watch(firmware, services) {
        driver.console = &console;
        E::FirmwareConfig config;
        config.configured = true;
        config.controller.txPin = 17;
        config.controller.rxPin = 16;
        config.controller.bitrate = 1000000;
        config.controller.sdkTxQueueDepth = 1;
        config.controller.rxQueueDepth = 16;
        CHECK(firmware.begin(config));
        for (unsigned node = 1; node <= 2; ++node) {
            CHECK(services.configure(uint8_t(node - 1), channel(node), false, true));
            C::NetworkRoute network;
            network.binding = channel(node).binding;
            network.generation = 1;
            network.heartbeatId = 0x700 + node;
            network.emcyId = 0x80 + node;
            CHECK(services.configureNetwork(uint8_t(node - 1), network, C::NetworkObserver(), console.now));
        }
    }
    static C::SdoChannel channel(unsigned node) {
        C::SdoChannel value;
        value.binding.logicalAxisId = node;
        value.binding.endpoint.profile = 1;
        value.binding.endpoint.address = node;
        value.binding.bindingGeneration = value.binding.configurationGeneration = 1;
        value.clientToServerId = 0x600 + node;
        value.serverToClientId = 0x580 + node;
        return value;
    }
    void turn() {
        console.now += 1000;
        driver.deliver();
        firmware.service();
        watch.service(firmware.owner().lastTimeUs());
        char line[320];
        while (watch.takeLine(line, sizeof line))
            lines += line;
        if (firmware.app().pendingRequest().owner && firmware.owner().result(firmware.app().pendingRequest()))
            firmware.app().advanceRead(firmware.owner().lastTimeUs());
    }
    void turns(unsigned count) {
        for (unsigned i = 0; i < count; ++i)
            turn();
    }
    void probe(unsigned node) {
        L::ReadTarget target;
        target.channel = *services.channel(uint8_t(node - 1));
        target.nodeGeneration = services.node(uint8_t(node - 1))->route.generation;
        CHECK(firmware.app().select(target));
        L::ReadOptions options;
        options.queueTimeoutUs = options.responseTimeoutUs = 250000;
        CHECK(firmware.app().requestProbe(console.now, console.now + 2000000, options));
    }
    std::string link(unsigned index) {
        char line[320];
        CHECK(E::formatLink(firmware, services, index, line, sizeof line));
        return line;
    }
    bool has(const char* text) const { return lines.find(text) != std::string::npos; }
};
void linkLines() {
    scenario = "link";
    std::unique_ptr<Rig> r(new Rig);
    r->turns(2);
    const std::string interface = r->link(0);
    CHECK(interface.rfind("LINK interface lifetime=100000 noListener=0 expiredPending=0 txExpired=0 markers=0 "
                          "reuseStopped=0 recoveries=0 inhibited=0 ", 0) == 0);
    CHECK(interface.find(" state=available offlineSince=0 receiveLost=0\n") != std::string::npos);
    CHECK(r->link(1).rfind("LINK node=1 reuse=marker unresolved=0 stopped=0 pending=0 accepted=0 responded=0 ", 0) == 0);
    CHECK(r->link(1).find(" answered=0 errors=0 misses=0 notObserved=0\n") != std::string::npos);
    CHECK(r->link(2).rfind("LINK node=2 reuse=marker ", 0) == 0);
    // The controller's error counters, bus evidence: without a status read only the callback counts.
    const unsigned long long t = static_cast<unsigned long long>(r->firmware.owner().lastTimeUs());
    char expected[200];
    std::snprintf(expected, sizeof expected,
                  "LINK bus read=1 t=%llu known=0 state=unknown tec=0 rec=0 busErrors=0 arbLost=0 prev=0 prevT=0 "
                  "dBusErrors=0 dArbLost=0 deltas=0 prevState=unknown dTec=0 dRec=0\n", t);
    CHECK(r->link(3) == expected);
    CHECK(r->link(4) == "LINK counters busOff=0 recoveriesStarted=0 recoveries=0 recoveriesFailed=0 rxOverflows=0 "
                        "abortsSent=0 abortsReceived=0 resyncs=0\n");
    char line[320];
    CHECK(!E::formatLink(r->firmware, r->services, E::LINK_LINES, line, sizeof line));
    CHECK(!E::formatLink(r->firmware, r->services, 0, line, 8)); // Too small: refused, never cut.
    CHECK(r->driver.requests.empty()); // Formatting sent nothing.
    r->probe(1);
    r->turns(5);
    CHECK(r->link(1).find(" responded=1 ") != std::string::npos);
    CHECK(r->link(1).find(" answered=1 errors=0 misses=0 notObserved=0\n") != std::string::npos);
    // A status read: state and counters, then each later reading's change since the one before.
    r->driver.statusAvailable = true;
    r->driver.controllerStatus.state = E::TwaiErrorState::PASSIVE;
    r->driver.controllerStatus.transmitErrors = 130;
    r->driver.controllerStatus.receiveErrors = 3;
    const std::string first = r->link(3);
    CHECK(first.find(" known=1 state=passive tec=130 rec=3 busErrors=0 arbLost=0 prev=1 ") != std::string::npos &&
          first.find(" deltas=0 prevState=unknown dTec=0 dRec=0\n") != std::string::npos);
    r->driver.controllerStatus.state = E::TwaiErrorState::WARNING;
    r->driver.controllerStatus.transmitErrors = 100;
    r->turns(1);
    const std::string second = r->link(3);
    CHECK(second.find(" known=1 state=warning tec=100 rec=3 ") != std::string::npos &&
          second.find(" deltas=1 prevState=passive dTec=-30 dRec=0\n") != std::string::npos);
    CHECK(r->driver.requests.size() == 1); // The probe; the reads sent nothing.
}
void refusals() {
    scenario = "refusals";
    std::unique_ptr<Rig> r(new Rig);
    CHECK(!std::strcmp(r->watch.begin(0, r->console.now), "invalid"));
    CHECK(!std::strcmp(r->watch.begin(E::LINK_WATCH_MAXIMUM_SECONDS + 1, r->console.now), "invalid"));
    CHECK(!r->watch.begin(5, r->console.now));
    CHECK(!std::strcmp(r->watch.begin(5, r->console.now), "active"));
    // A window with no expiry: no restart, and the end line says so.
    r->turns(5200);
    CHECK(!r->watch.active() && !r->watch.restarts() && r->firmware.owner().stats().recoveries == 0);
    CHECK(r->has("LINK_WATCH armed seconds=5 lifetime=100000 reuse1=marker reuse2=marker "));
    CHECK(r->has("LINK_WATCH_END restarts=0 expired=0 recovery=-1 causes=0 listenerEnd=0 "));
}
// The connector pulled with the drives released: a read's frame is never ACKed. Within the TX lifetime
// the owner reports it expired and enters NO_LISTENER; the watch calls recover() itself, once. The owner
// starts no resync step during NO_LISTENER, so the watch probes node 1 at once with an explicit marker
// (1018:01: the expired 1000:00 is retained). Unplugged, that probe gets no ACK and times out; after the
// replug its frame leaves (its late reply is the one unmatched frame), NO_LISTENER ends, and the next probe
// a probe interval later (1018:02) reopens the route. Bounded: one restart, at most LINK_WATCH_PROBES.
void unplugAndReplug() {
    scenario = "unplug";
    std::unique_ptr<Rig> r(new Rig);
    r->turns(2);
    CHECK(!r->watch.begin(60, r->firmware.owner().lastTimeUs()));
    r->driver.unplugged = true;
    r->probe(1);
    r->turns(99);
    CHECK(!r->firmware.owner().noListener() && !r->watch.restarts()); // Not before the lifetime.
    r->turns(5);
    CHECK(r->firmware.owner().stats().txExpired >= 1 && r->watch.restarts() == 1);
    CHECK(r->has("LINK_EVENT ") && r->has(" kind=expired node=0 value=1\n") && r->has(" kind=restart node=0 "));
    CHECK(r->has(" kind=recovery node=4 value=0\n")); // RETIRED, cause: TX expired.
    // NO_LISTENER is a bus-level fault: the interface is offline and the rows are not observed.
    CHECK(r->link(0).find(" state=offline ") != std::string::npos &&
          r->link(1).find(" notObserved=1\n") != std::string::npos &&
          r->link(2).find(" notObserved=1\n") != std::string::npos);
    CHECK(r->has(" kind=marker node=1 value=101801\n")); // Probed at once, NO_LISTENER or not.
    r->turns(300); // Still unplugged: whatever expires again, the watch never restarts twice.
    CHECK(r->watch.restarts() == 1 && r->firmware.owner().stats().recoveries == 1);
    CHECK(r->has(" kind=marker-result node=1 value=301\n")); // TIMEOUT, transmission possible.
    CHECK(r->firmware.owner().noListener() && r->services.unresolved(0));
    CHECK(!r->firmware.owner().stats().markers); // No automatic resync step during NO_LISTENER.
    const size_t beforeReplug = r->driver.requests.size();
    CHECK(beforeReplug == 2); // The deleted probe and the watch's first marker, both unACKed.
    r->driver.unplugged = false;
    r->turns(1200); // Past the probe interval.
    CHECK(!r->firmware.owner().noListener() && !r->services.unresolved(0) && !r->services.unresolved(1));
    CHECK(r->has(" kind=listener-end node=0 ") && r->has(" kind=marker node=1 value=101802\n") &&
          r->has(" kind=marker-result node=1 value=1\n"));
    CHECK(r->has(" kind=resynced node=1 ") && r->has(" kind=resynced node=2 "));
    CHECK(r->link(0).find(" state=available offlineSince=0 ") != std::string::npos &&
          r->link(1).find(" notObserved=0\n") != std::string::npos);
    // After the replug the first probe's frame, held unACKed in the controller since 1.106 s, leaves; its
    // late reply lands in the same turn as its TX report, so no cleanup abort follows it. Then only the
    // second probe leaves; the 1000:00 read the restart deleted is never resent.
    const uint8_t second[8] = {0x40, 0x18, 0x10, 0x02, 0, 0, 0, 0};
    CHECK(r->driver.requests.size() == beforeReplug + 1);
    CHECK(!std::memcmp(r->driver.requests[beforeReplug].data, second, 8));
    r->turns(60000);
    CHECK(!r->watch.active());
    // Two expiries (the read, then the first probe while unplugged) and one restart. The one unmatched
    // frame after it is the late reply to that probe, which timed out before the replug: the counter
    // cannot tell it from a stale frame, so the record's traffic must.
    CHECK(r->has("LINK_WATCH_END restarts=1 expired=2 recovery=0 causes=4 listenerEnd=1 ") &&
          r->has(" resynced=1,1 unmatchedAfter=1 refusedAfter=0 ") &&
          r->has(" markers=0 explicit=2 explicitOk=1 noListener=0 dropped=0\n"));
    CHECK(r->driver.requests.size() == beforeReplug + 1); // No further probe once reopened.
}
// The probes are bounded: a drive that answers no marker object gets one probe per probe interval, each
// after its earlier probe frame retired and was aborted (05040000), until no object of the marker list is
// admissible (1000:00 is retained from the deleted read, each timed-out probe retains its own pair). Three
// probes here, below LINK_WATCH_PROBES; the route stays unresolved and nothing else leaves.
void probesAreBounded() {
    scenario = "bounded-probes";
    std::unique_ptr<Rig> r(new Rig);
    r->turns(2);
    CHECK(!r->watch.begin(60, r->firmware.owner().lastTimeUs()));
    r->driver.unplugged = true;
    r->probe(1);
    r->turns(120);
    CHECK(r->watch.restarts() == 1);
    r->driver.unplugged = false;
    r->driver.drives[0].silentIndex = 0x1018; // The drive answers no marker object.
    r->turns(62000);
    CHECK(!r->watch.active());
    CHECK(r->has(" kind=marker node=1 value=101801\n") && r->has(" kind=marker node=1 value=101802\n") &&
          r->has(" kind=marker node=1 value=101803\n") && !r->has(" kind=marker node=1 value=100000\n"));
    const uint8_t expected[7][8] = {{0x40, 0x00, 0x10, 0x00, 0, 0, 0, 0},
                                    {0x40, 0x18, 0x10, 0x01, 0, 0, 0, 0},
                                    {0x80, 0x18, 0x10, 0x01, 0x00, 0x00, 0x04, 0x05},
                                    {0x40, 0x18, 0x10, 0x02, 0, 0, 0, 0},
                                    {0x80, 0x18, 0x10, 0x02, 0x00, 0x00, 0x04, 0x05},
                                    {0x40, 0x18, 0x10, 0x03, 0, 0, 0, 0},
                                    {0x80, 0x18, 0x10, 0x03, 0x00, 0x00, 0x04, 0x05}};
    CHECK(r->driver.requests.size() == 7);
    for (unsigned i = 0; i < 7 && i < r->driver.requests.size(); ++i)
        CHECK(!std::memcmp(r->driver.requests[i].data, expected[i], 8));
    CHECK(r->has(" markers=0 explicit=3 explicitOk=0 ") && 3 < E::LINK_WATCH_PROBES + 0u);
    CHECK(r->watch.restarts() == 1 && r->firmware.owner().stats().recoveries == 1 && r->services.unresolved(0));
}
// Prompt 55's 2026-11-04 cases without an MCU restart (bench item b): a best-effort stop leaves its route
// unresolved and starts no owner resync (reopening after a stop is explicit intent), as trial 503 did. The
// bench's explicit marker (`y`) then reopens it; a resolved route refuses one; nothing else is sent.
void explicitMarkerAfterBestEffortStop() {
    scenario = "explicit-marker";
    std::unique_ptr<Rig> r(new Rig);
    std::unique_ptr<E::AxisSession> session(new E::AxisSession(r->firmware.owner()));
    E::MarkerCommand marker(r->firmware, r->services);
    r->turns(2);
    char line[320];
    CHECK(!marker.begin(1, r->console.now, line, sizeof line));
    CHECK(std::string(line) == "MARKER refused node=1 reason=resolved; nothing sent\n");
    CHECK(!marker.begin(3, r->console.now, line, sizeof line) && std::strstr(line, "reason=node"));
    L::ReadTarget target;
    target.channel = *r->services.channel(0);
    target.nodeGeneration = r->services.node(0)->route.generation;
    CHECK(r->firmware.app().openAxis(*session, target, L::SettingsModel::CL86_C, r->console.now));
    E::StopPolicy policy;
    policy.options.service.queueTimeoutUs = policy.options.service.responseTimeoutUs = 250000;
    policy.options.observationIntervalUs = 100000;
    policy.options.maximumAgeUs = 2500000;
    policy.deadlineUs = r->console.now + 2000000;
    policy.bestEffortDeadlineUs = r->console.now + 3000000;
    L::ReadContext none;
    CHECK(session->requestStop(L::StopKind::QUICK_STOP, none, policy, r->console.now) == E::StopPath::BEST_EFFORT);
    for (unsigned i = 0; i < 50; ++i) {
        r->turn();
        session->service(r->firmware.owner().lastTimeUs());
    }
    CHECK(session->stopPath() == E::StopPath::FINISHED && r->services.unresolved(0));
    r->turns(400);
    CHECK(r->services.unresolved(0) && !r->firmware.owner().stats().markers); // No owner resync after a stop.
    CHECK(session->close());
    const size_t before = r->driver.requests.size();
    CHECK(marker.begin(1, r->console.now, line, sizeof line) && marker.active());
    CHECK(std::string(line) == "MARKER admitted node=1 object=1000:00; one explicit read, policy premises accepted\n");
    CHECK(!marker.begin(2, r->console.now, line, sizeof line) && std::strstr(line, "reason=active"));
    bool reported = false;
    for (unsigned i = 0; i < 20 && !reported; ++i) {
        r->turn();
        reported = marker.service(r->firmware.owner().lastTimeUs(), line, sizeof line);
    }
    CHECK(reported && !marker.active() && std::strstr(line, "MARKER node=1 object=1000:00 outcome=0 possible=1 reopened=1 "));
    CHECK(!r->services.unresolved(0) && r->firmware.owner().resources().retained == 0);
    CHECK(r->driver.requests.size() == before + 1 && indexOf(r->driver.requests[before]) == 0x1000 &&
          r->driver.requests[before].data[0] == 0x40);
}
} // namespace
int main() {
    linkLines();
    refusals();
    unplugAndReplug();
    probesAreBounded();
    explicitMarkerAfterBestEffortStop();
    std::printf("link_watch_test: %u checks\n", checks);
    return 0;
}
