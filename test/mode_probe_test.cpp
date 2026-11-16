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
    E::FirmwareProbe firmware;
    E::AxisSession one, two;
    E::BenchRoute route;
    E::RouteWork routeWork;
    const C::SdoTransfer* externalWord = nullptr; // Explicit additional test producer, scoped below.
    unsigned samples = 0;
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
        : firmware(services, driver.callbacks(), console.callbacks()), one(firmware.owner()),
          two(firmware.owner()), route(firmware, services, hooks(this)) {
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

std::string line(unsigned mode, unsigned trial = 1) {
    return "m1," + std::to_string(trial) + ",1," + std::to_string(mode) + ",2,100";
}
void released(Rig& rig) {
    Drive& d = rig.driver.drives[0];
    d.mode = 1;
    d.monitor = L::MONITOR_RELEASED | L::MONITOR_RUNNING;
    d.status = 0xc050;
    d.position = 42;
    d.velocity = 0;
}
void checkOnlyModeWrite(const Rig& rig, unsigned expected, unsigned count = 1) {
    unsigned writes = 0;
    for (const M::Frame& f : rig.driver.requests) {
        if (f.data[0] == 0x40 || f.data[0] == 0x80)
            continue;
        ++writes;
        CHECK(indexOf(f) == 0x6060 && f.data[0] == 0x2f && f.data[3] == 0 && f.data[4] == expected);
        CHECK(f.data[5] == 0 && f.data[6] == 0 && f.data[7] == 0);
    }
    CHECK(writes == count);
}
void testCandidatesAndRestoration() {
    scenario = "candidates-restoration";
    std::unique_ptr<Rig> rig(new Rig);
    released(*rig);
    unsigned trial = 0;
    const unsigned candidates[] = {2, 4, 7, 8, 9, 10, 1, 3};
    for (unsigned mode : candidates) {
        const size_t prior = rig->driver.requests.size();
        const E::RouteRecord& r = rig->run(line(mode, ++trial));
        CHECK(r.failure == E::RouteFailure::NONE && r.checkPassed && r.firstPresent);
        CHECK(r.plan.modeRaw == mode && r.cycles == 2 && r.motionTotal == 0 && r.caseCount == 1);
        CHECK(r.terminalCount == 18 + 1 + 9 + 7 && r.last.mode == mode && r.last.position == 42);
        CHECK(rig->downloadsFrom(prior) == 1 && !rig->one.latestControl());
        const std::string dump = rig->dump();
        CHECK(dump.find("route=mode-probe") != std::string::npos);
        CHECK(dump.find("mode=" + std::to_string(mode)) != std::string::npos);
        CHECK(dump.find("object=6060:00 dir=1") != std::string::npos);
        rig->release();
    }
}
void testIgnoredAndFailedWrites() {
    scenario = "ignored-mode-is-evidence";
    {
        std::unique_ptr<Rig> rig(new Rig);
        released(*rig);
        rig->driver.drives[0].ignoreMode = true;
        const E::RouteRecord& r = rig->run(line(9));
        CHECK(r.failure == E::RouteFailure::NONE && r.last.mode == 1 && r.cycles == 2);
        checkOnlyModeWrite(*rig, 9);
    }
    for (unsigned lost = 0; lost != 2; ++lost) {
        scenario = lost ? "lost-mode-write" : "aborted-mode-write";
        std::unique_ptr<Rig> rig(new Rig);
        released(*rig);
        Drive& d = rig->driver.drives[0];
        if (lost)
            d.silentIndex = 0x6060;
        else
            d.abortIndex = 0x6060;
        const E::RouteRecord& r = rig->run(line(4));
        CHECK(r.failure == E::RouteFailure::SERVICE && r.cycles == 0 && r.terminalCount == 19);
        checkOnlyModeWrite(*rig, 4);
        const E::RouteTerminal& t = r.terminals[18];
        CHECK(t.index == 0x6060 && t.direction == unsigned(C::SdoDirection::DOWNLOAD));
        CHECK(lost ? (t.flags & 4u) != 0 : t.abortCode == d.abortCode);
    }
}
void testRefusals() {
    scenario = "candidate-validation";
    E::RoutePlan p;
    for (unsigned mode : {0u, 5u, 6u, 11u, 255u})
        CHECK(!E::parseRouteCommand(line(mode).data(), line(mode).size(), p));
    {
        std::unique_ptr<Rig> rig(new Rig);
        CHECK(rig->start("m1,1,0,2,2,100") && rig->driver.requests.empty());
    }
    for (unsigned invalid = 0; invalid != 5; ++invalid) {
        scenario = "disabled-stationary-admission";
        std::unique_ptr<Rig> rig(new Rig);
        released(*rig);
        Drive& d = rig->driver.drives[0];
        if (invalid == 0) d.monitor &= uint16_t(~L::MONITOR_RELEASED);
        if (invalid == 1) d.status = 0x8037;
        if (invalid == 2) d.velocity = 1;
        if (invalid == 3) d.drift = 1;
        if (invalid == 4) d.mode = 6;
        CHECK(rig->run(line(2)).failure == E::RouteFailure::PREREQUISITE);
        CHECK(rig->downloadsFrom(0) == 0);
    }
}
void testEpochAndDeadlineBeforeWrite() {
    for (unsigned epoch = 0; epoch != 2; ++epoch) {
        scenario = epoch ? "boot-before-write" : "cutoff-before-write";
        std::unique_ptr<Rig> rig(new Rig);
        released(*rig);
        CHECK(!rig->start(line(2)));
        while (rig->route.active() && !rig->route.record()->checkPassed)
            rig->turn();
        CHECK(rig->route.active() && rig->route.record()->checkPassed && rig->downloadsFrom(0) == 0);
        if (epoch) {
            M::Frame boot = frame(0x701, 1);
            boot.data[0] = 0;
            rig->driver.push(boot);
        } else {
            rig->console.now += uint64_t(E::ROUTE_CUTOFF_MS) * 1000;
        }
        for (unsigned i = 0; i < 20 && rig->route.active(); ++i)
            rig->turn();
        CHECK(!rig->route.active() && rig->route.record()->failure != E::RouteFailure::NONE);
        CHECK(rig->downloadsFrom(0) == 0);
    }
}
void testPostWriteMovementEndsObservation() {
    scenario = "unexpected-movement";
    std::unique_ptr<Rig> rig(new Rig);
    released(*rig);
    rig->driver.drives[0].stepAfterMode = 1;
    const E::RouteRecord& r = rig->run(line(7));
    CHECK(r.failure == E::RouteFailure::OBSERVATION && std::strcmp(r.why, "position") == 0);
    checkOnlyModeWrite(*rig, 7);
}
} // namespace
int main() {
    testCandidatesAndRestoration();
    testIgnoredAndFailedWrites();
    testRefusals();
    testEpochAndDeadlineBeforeWrite();
    testPostWriteMovementEndsObservation();
    std::printf("mode probe: %u checks\n", checks);
}
