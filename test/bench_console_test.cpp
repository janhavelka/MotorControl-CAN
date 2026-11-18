// SPDX-License-Identifier: MIT
// The shared console on the S2 bench image (BenchConsole) through the real firmware assembly, owner,
// SDO engine, application, BenchRoute, AxisSession and ProbeMotion, against the bench route suite's
// synthetic one-frame driver and two synthetic drives (support/BenchDrive.h). Every value, word response
// and movement here is manufactured by that fake: nothing shows what a CL86-C does.
#include "BenchConsole.h"
#include "BenchRoute.h"
#include <algorithm>
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

bool has(const std::string& text, const char* part) {
    return text.find(part) != std::string::npos;
}
size_t count(const std::string& text, const char* part) {
    size_t found = 0;
    for (size_t at = text.find(part); at != std::string::npos; at = text.find(part, at + 1))
        ++found;
    return found;
}
// main.cpp's objects in its order: firmware, the route and its two sessions, then the console, its two
// blocks and one motion attachment per node.
struct Rig {
    C::SdoClient services;
    Console uart;
    Driver driver;
    E::AcquisitionRecord evidenceRecord;
    E::RouteRecord routeRecord;
    E::FirmwareProbe firmware;
    E::AxisSession one, two;
    E::BenchRoute route;
    E::RouteWork work;
    struct Blocks {
        alignas(E::ProbeDevice) unsigned char device[sizeof(E::ProbeDevice)];
        alignas(E::ProbeConsole) unsigned char console[sizeof(E::ProbeConsole)];
    } blocks;
    E::BenchConsole console;
    E::BenchAxis axis1, axis2;
    E::BenchActivity activity;
    E::BenchConsoleStorage storage() {
        E::BenchConsoleStorage value;
        value.device = blocks.device;
        value.console = blocks.console;
        return value;
    }
    Rig()
        : firmware(services, driver.callbacks(), uart.callbacks(), &evidenceRecord), one(firmware.owner()), two(firmware.owner()),
          route(firmware, services, E::RouteCallbacks(), &routeRecord), console(firmware, services, storage()),
          axis1(firmware, one), axis2(firmware, two) {
        driver.console = &uart;
        driver.drives[0].node = 1;
        driver.drives[1].node = 2;
        // main.cpp's dispatcher: the console's own motion downloads first and never further, then the route.
        E::DownloadPreflight check;
        check.context = this;
        check.check = [](void* context, O::SequenceId sequence, const C::SdoTransfer& transfer, uint64_t now) {
            Rig& r = *static_cast<Rig*>(context);
            if (r.console.ownsDownload(sequence, transfer))
                return r.console.preflight(sequence, transfer, now);
            return r.route.preflight(sequence, transfer, now);
        };
        firmware.setPreflight(check);
        E::FirmwareConfig config;
        config.configured = true;
        config.controller.txPin = 17;
        config.controller.rxPin = 16;
        config.controller.bitrate = 1000000;
        config.controller.sdkTxQueueDepth = 1;
        config.controller.rxQueueDepth = 16;
        CHECK(firmware.begin(config));
        CHECK(route.attach(1, one) && route.attach(2, two) && route.attachWork(work));
        route.setCaps(E::RouteCaps::campaign()); // The optional guard, which testArmingAndMove exercises.
        CHECK(console.attachRoute(route) && console.attachAxis(1, axis1) && console.attachAxis(2, axis2));
        CHECK(!console.attachAxis(1, axis2) && console.hasAxis(1) && console.hasAxis(2));
    }
    // The bench's `o`: main.cpp's ensureRoute for both nodes, then the console's host facts.
    void open() {
        for (unsigned node = 1; node <= 2; ++node) {
            C::SdoChannel channel;
            channel.binding.logicalAxisId = node;
            channel.binding.endpoint.profile = 1;
            channel.binding.endpoint.address = node;
            channel.binding.bindingGeneration = channel.binding.configurationGeneration = 1;
            channel.clientToServerId = 0x600 + node;
            channel.serverToClientId = 0x580 + node;
            CHECK(services.configure(uint8_t(node - 1), channel, false, true));
            C::NetworkRoute network;
            network.binding = channel.binding;
            network.generation = 1;
            network.heartbeatId = 0x700 + node;
            network.emcyId = 0x80 + node;
            CHECK(services.configureNetwork(uint8_t(node - 1), network, C::NetworkObserver(), uart.now));
        }
        CHECK(console.open(1000000));
    }
    L::ReadTarget target(unsigned node) const {
        L::ReadTarget value;
        value.channel = *services.channel(uint8_t(node - 1));
        value.nodeGeneration = services.node(uint8_t(node - 1))->route.generation;
        return value;
    }
    // main.cpp's activity, through its own policy (BenchConsole.h), for the bench state this rig can hold: a
    // route record, and the dump and retained response that Image models.
    E::BenchHolds bench;
    void derive() {
        bench.trial = route.record();
        bench.controller = true;
        activity = E::benchActivity(bench);
    }
    // main.cpp's turn: firmware, route, then the console and its notice.
    std::string notices;
    void turn() {
        uart.now += 1000;
        driver.deliver();
        firmware.service();
        route.service(firmware.owner().lastTimeUs());
        derive();
        console.service(uart.now, activity);
        if (console.notice()) {
            notices += console.notice();
            uart.output += console.notice(); // main.cpp's one-shot response path.
            console.clearNotice();
        }
    }
    void turns(unsigned n) {
        for (unsigned i = 0; i < n; ++i)
            turn();
    }
    // Every byte of one line as main.cpp hands it over, with its CR.
    E::ConsoleInput send(const std::string& text) {
        derive();
        E::ConsoleInput result = E::ConsoleInput::COLLECTING;
        for (char c : text) {
            result = console.take(uint8_t(c), uart.now, activity);
            CHECK(result == E::ConsoleInput::COLLECTING);
        }
        result = console.take('\r', uart.now, activity);
        CHECK(result != E::ConsoleInput::COLLECTING);
        if (result == E::ConsoleInput::REFUSED)
            uart.output += console.reply(); // main.cpp's one-shot response path.
        return result;
    }
    // Send one line and service until the console is quiet; returns what it printed.
    std::string command(const std::string& text, unsigned limit = 400) {
        const size_t mark = uart.output.size();
        send(text);
        for (unsigned i = 0; i < limit; ++i)
            turn();
        return uart.output.substr(mark);
    }
    unsigned uploads() const {
        unsigned n = 0;
        for (const M::Frame& f : driver.requests)
            n += f.data[0] == 0x40;
        return n;
    }
    unsigned downloadsFrom(size_t first) const {
        unsigned n = 0;
        for (size_t i = first; i < driver.requests.size(); ++i)
            n += driver.requests[i].data[0] != 0x40 && driver.requests[i].data[0] != 0x80;
        return n;
    }
    // The bench route suite's helpers: a route through BenchRoute, its release, a pre-operational heartbeat.
    const char* start(const std::string& text) {
        E::RoutePlan plan;
        CHECK(E::parseRouteCommand(text.data(), text.size(), plan));
        return route.begin(plan, uart.now);
    }
    const E::RouteRecord& run(const std::string& text) {
        const char* refusal = start(text);
        if (refusal)
            std::fprintf(stderr, "refused %s: %s\n", text.c_str(), refusal);
        CHECK(!refusal);
        for (unsigned i = 0; i < 80000 && route.active(); ++i)
            turn();
        CHECK(!route.active() && route.record() && route.record()->terminal);
        return *route.record();
    }
    void release() {
        turns(5);
        CHECK(route.release());
    }
    void heartbeat(unsigned node) {
        M::Frame beat = frame(0x700 + node, 1);
        beat.data[0] = 0x7f;
        driver.push(beat);
        turns(3);
    }
};
// Every printed line ends in a newline and is either one whole console record or one bench line.
void wholeLines(const std::string& output) {
    size_t at = 0;
    while (at < output.size()) {
        const size_t end = output.find('\n', at);
        CHECK(end != std::string::npos);
        const std::string line = output.substr(at, end - at);
        if (line[0] == '{')
            CHECK(line.compare(0, 37, "{\"schema\":\"motorcontrolcan.console/1\"") == 0 && line.back() == '}' &&
                  count(line, "{\"schema\"") == 1);
        else
            CHECK(line.find('{') == std::string::npos);
        at = end + 1;
    }
}
std::string id(uint32_t& trial) {
    return std::to_string(++trial);
}
// P2 and P3 on one node, as the bench route suite runs them: the latest acknowledged word is then 000F.
void enabled(Rig& r, uint32_t& trial, unsigned node = 1) {
    const std::string n = std::to_string(node);
    CHECK(r.run("M" + n + "," + id(trial) + ",1,1,200").failure == E::RouteFailure::NONE);
    r.release();
    r.heartbeat(node);
    CHECK(r.run("E" + n + "," + id(trial) + ",1,1,1,32823,5000,120,10000,800").failure == E::RouteFailure::NONE);
    r.release();
    const C::SdoCompletion* control = (node == 1 ? r.one : r.two).latestControl();
    CHECK(control && control->transfer.data[0] == 0x0f);
}
// The host's arming line. Every evidence value here is this fake's, never a measurement.
struct ArmSpec {
    unsigned attended = 1;
    long sp = 0;
    unsigned interval = 100;
    unsigned long record = 19;
    unsigned as = 1, am = ACCEPT_BIT, av = ACCEPT_BIT, cs = 1, cm = COMPLETE_BIT, cv = COMPLETE_BIT, rule = 1;
    unsigned long dt = DEVICE_TYPE, hw = HARDWARE, sw = SOFTWARE, vid = VENDOR, pc = PRODUCT, rev = REVISION;
    unsigned long mc = MODEL, fc = FIRMWARE;
    unsigned rel = 1; // 0 absolute, 1 relative ACTUAL, 2 relative COMMANDED.
    long base = 0;
    unsigned sd = 2000, be = 3000, si = 100;
};
std::string arm(unsigned node, uint32_t& trial, const ArmSpec& a = ArmSpec()) {
    char text[300];
    std::snprintf(text, sizeof text,
                  "Y%u,%s,%u,%ld,%u,%lu,%u,%u,%u,%u,%u,%u,%u,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%u,%ld,%u,%u,%u", node,
                  id(trial).c_str(), a.attended, a.sp, a.interval, a.record, a.as, a.am, a.av, a.cs, a.cm, a.cv,
                  a.rule, a.dt, a.hw, a.sw, a.vid, a.pc, a.rev, a.mc, a.fc, a.rel, a.base, a.sd, a.be, a.si);
    return text;
}

void testGate() {
    scenario = "gate";
    struct Row {
        const char* text;
        E::ConsoleGate gate;
    };
    const Row rows[] = {{"@1 version", E::ConsoleGate::ALWAYS},
                        {"@1 help", E::ConsoleGate::ALWAYS},
                        {"@1 status", E::ConsoleGate::ALWAYS},
                        {"@1 health", E::ConsoleGate::ALWAYS},
                        {"@1 stats", E::ConsoleGate::ALWAYS},
                        {"@1 stats reset", E::ConsoleGate::IDLE_BENCH},
                        {"@1 stats times", E::ConsoleGate::ALWAYS},
                        {"@1 drv bus", E::ConsoleGate::ALWAYS},
                        {"@1 health counts", E::ConsoleGate::ALWAYS},
                        {"@1 load", E::ConsoleGate::ALWAYS},
                        {"@1 network", E::ConsoleGate::ALWAYS},
                        {"@1 heartbeat", E::ConsoleGate::ALWAYS},
                        {"@1 pdo 0", E::ConsoleGate::ALWAYS},
                        {"@1 pdo apply timer 100", E::ConsoleGate::IDLE_BENCH},
                        {"@1 debug", E::ConsoleGate::ALWAYS},
                        {"@1 debug off", E::ConsoleGate::ALWAYS},
                        {"@1 debug raw", E::ConsoleGate::IDLE_BENCH},
                        {"@1 result", E::ConsoleGate::ALWAYS},
                        {"@1 release 4", E::ConsoleGate::ALWAYS},
                        {"@1 cancel", E::ConsoleGate::ALWAYS},
                        {"@1 host 0", E::ConsoleGate::IDLE_BENCH},
                        {"@1 useaddr 2", E::ConsoleGate::IDLE_BENCH},
                        {"@1 probe", E::ConsoleGate::IDLE_BENCH},
                        {"@1 read state", E::ConsoleGate::IDLE_BENCH},
                        {"@1 monitor 100 3", E::ConsoleGate::IDLE_BENCH},
                        {"@1 monitor off", E::ConsoleGate::ALWAYS},
                        {"@1 nmt preop addressed", E::ConsoleGate::IDLE_BENCH},
                        {"@1 discover passive 500 0 1", E::ConsoleGate::IDLE_LISTING},
                        {"@1 discover inspect", E::ConsoleGate::ALWAYS},
                        {"@1 discover cancel", E::ConsoleGate::ALWAYS},
                        {"@1 recover", E::ConsoleGate::NEVER},
                        {"@1 stop fast", E::ConsoleGate::IDLE_BENCH},
                        {"@1 moveby 10", E::ConsoleGate::IDLE_BENCH},
                        {"@1 velocity read", E::ConsoleGate::IDLE_BENCH},
                        {"@1 home methods", E::ConsoleGate::ALWAYS},
                        {"@1 home read", E::ConsoleGate::IDLE_BENCH},
                        {"@1 axis config", E::ConsoleGate::ALWAYS},
                        {"@1 axis origin 0", E::ConsoleGate::IDLE_BENCH},
                        {"@1 stepsperturn 1600", E::ConsoleGate::IDLE_BENCH},
                        {"@1 speed", E::ConsoleGate::ALWAYS},
                        {"@1 speed 30", E::ConsoleGate::ALWAYS},
                        {"@1 motion-profile inspect", E::ConsoleGate::ALWAYS},
                        {"@1 motion-profile read", E::ConsoleGate::IDLE_BENCH},
                        {"@1 frobnicate", E::ConsoleGate::ALWAYS}};
    for (const Row& row : rows) {
        E::ConsoleLine line;
        CHECK(E::parseConsoleLine(row.text, std::strlen(row.text), line));
        if (E::benchConsoleGate(line) != row.gate)
            std::fprintf(stderr, "gate of %s\n", row.text);
        CHECK(E::benchConsoleGate(line) == row.gate);
    }
}
// Inspection without a route, collection rules, and whole records through the UART queue.
void testInputAndOutput() {
    scenario = "io";
    std::unique_ptr<Rig> r(new Rig);
    r->turns(20);
    CHECK(r->uart.output.empty() && r->driver.requests.empty() && !r->console.holds());
    const std::string version = r->command("@7 version");
    CHECK(has(version, "\"type\":\"reply\",\"id\":7,\"command\":\"version\",\"ok\":true"));
    wholeLines(version);
    // Without the premise no slot exists: the console refuses the selection itself, sends nothing.
    const std::string early = r->command("@8 host 0");
    CHECK(has(early, "\"id\":8") && has(early, "\"ok\":false"));
    // An overlong line and an incomplete one reach nothing.
    std::string longLine = "@9 help ";
    longLine.append(300, 'x');
    CHECK(r->send(longLine) == E::ConsoleInput::REFUSED);
    CHECK(std::strcmp(r->console.reply(), "CONSOLE_REFUSED reason=length; no work\n") == 0);
    CHECK(r->console.take('@', r->uart.now, r->activity) == E::ConsoleInput::COLLECTING);
    CHECK(r->console.take('1', r->uart.now, r->activity) == E::ConsoleInput::COLLECTING);
    CHECK(!r->console.expire(r->uart.now + E::BenchConsole::COLLECT_TIMEOUT_US));
    CHECK(r->console.expire(r->uart.now + E::BenchConsole::COLLECT_TIMEOUT_US + 1) && !r->console.collecting());
    // A line that does not parse goes to the console, which refuses it with its own record.
    const std::string bad = r->command("@0 version");
    CHECK(has(bad, "rejected (invalid_line)") || has(bad, "invalid_line"));
    // Held output: a console reply waits behind a bench record dump, then appears whole after it.
    r->bench.dumping = true;
    const size_t mark = r->uart.output.size();
    r->send("@10 caps");
    CHECK(r->activity.holdOutput);
    for (unsigned i = 0; i < 50; ++i) {
        r->uart.now += 1000;
        r->firmware.service();
        r->console.service(r->uart.now, r->activity);
        if (i % 5 == 0) {
            static const char bench[] = "RESULT op=1 req=1 bench line written whole\n";
            r->firmware.platform().writeOutput(reinterpret_cast<const uint8_t*>(bench), sizeof bench - 1);
        }
    }
    CHECK(r->uart.output.find("{\"schema\"", mark) == std::string::npos);
    r->bench.dumping = false;
    r->derive();
    CHECK(!r->activity.holdOutput);
    for (unsigned i = 0; i < 2000; ++i) {
        r->uart.now += 1000;
        r->firmware.service();
        r->console.service(r->uart.now, r->activity);
        if (i % 20 == 0) { // Room for both: a bench stream that fills the queue alone starves the console.
            static const char bench[] = "TRAFFIC k=1 p=2 id=581 another whole bench line\n";
            r->firmware.platform().writeOutput(reinterpret_cast<const uint8_t*>(bench), sizeof bench - 1);
        }
    }
    const std::string listing = r->uart.output.substr(mark);
    if (std::getenv("BENCH_CONSOLE_DEBUG"))
        std::fprintf(stderr, "%s\n", listing.c_str());
    CHECK(count(listing, "\"type\":\"capability\"") > 10 && has(listing, "\"result\":\"complete\""));
    wholeLines(r->uart.output);
    CHECK(r->driver.requests.empty());
}
// The premise step, the console's device routes on a bench slot, and its retention fence.
// An input loss during a console line (main.cpp after ProbePlatform::takeInputLoss): the line is dropped
// unseen, with the console's own refusal correlated by its @ID when that prefix arrived whole.
void testInputLoss() {
    scenario = "input-loss";
    std::unique_ptr<Rig> r(new Rig);
    r->open();
    const char partial[] = "@12 velocity 6 r";
    for (const char* c = partial; *c; ++c)
        CHECK(r->console.take(uint8_t(*c), r->uart.now, r->activity) == E::ConsoleInput::COLLECTING);
    const std::string lost = r->console.abandon();
    CHECK(!r->console.collecting() && r->console.console().stats().lines == 0);
    CHECK(has(lost, "\"type\":\"reply\",\"id\":12,\"command\":\"input\",\"ok\":false") &&
          has(lost, "\"error\":\"input_lost\"") && lost.back() == '\n');
    wholeLines(lost);
    // Without its whole prefix the line has no ID to answer: the bench's own refusal line.
    for (const char* c = "@1"; *c; ++c)
        r->console.take(uint8_t(*c), r->uart.now, r->activity);
    CHECK(std::string(r->console.abandon()) == "CONSOLE_REFUSED reason=input-loss; no work\n");
    // The widest ID the console accepts is still correlated; one past it is not an ID.
    for (const char* c = "@4294967295 read st"; *c; ++c)
        r->console.take(uint8_t(*c), r->uart.now, r->activity);
    CHECK(has(r->console.abandon(), "\"id\":4294967295,\"command\":\"input\",\"ok\":false"));
    for (const char* c = "@4294967296 read st"; *c; ++c)
        r->console.take(uint8_t(*c), r->uart.now, r->activity);
    CHECK(std::string(r->console.abandon()) == "CONSOLE_REFUSED reason=input-loss; no work\n");
    // The next whole line is the console's as before.
    CHECK(has(r->command("@13 version", 100), "\"id\":13,\"command\":\"version\",\"ok\":true"));
    CHECK(r->console.console().stats().lines == 1);
}
void testConsoleReads() {
    scenario = "reads";
    std::unique_ptr<Rig> r(new Rig);
    r->open();
    CHECK(has(r->command("@1 host 0"), "\"wire_work\":false"));
    CHECK(r->driver.requests.empty());
    const std::string identity = r->command("@2 read identity", 600);
    CHECK(has(identity, "\"result\":\"accepted\",\"operation_id\":1"));
    CHECK(has(identity, "\"type\":\"terminal\"") && has(identity, "\"outcome\":\"SUCCESS\""));
    // The measured one-byte 1009 and 100A widths came from the bench's policy, not detection.
    CHECK(has(identity, "\"index\":4105,\"subindex\":0,\"width\":1,\"raw\":\"18\""));
    CHECK(has(identity, "\"index\":4106,\"subindex\":0,\"width\":1,\"raw\":\"52\""));
    CHECK(r->driver.requests.size() == 9 && r->driver.drives[0].downloads == 0);
    for (const M::Frame& f : r->driver.requests)
        CHECK(f.id == 0x601 && f.data[0] == 0x40);
    wholeLines(r->uart.output);
    // Retained until released: the bench admits no owner work meanwhile, and its route refuses.
    CHECK(r->console.holds());
    E::RoutePlan plan;
    CHECK(E::parseRouteCommand("A1,5", 4, plan));
    CHECK(std::strcmp(r->route.begin(plan, r->uart.now), "state") == 0);
    CHECK(has(r->command("@3 release 1"), "\"id\":3,\"command\":\"release\",\"ok\":true"));
    CHECK(!r->console.holds());
    // A state read, the inspection routes, and a finite monitor on the same slot.
    r->driver.drives[0].position = 1234;
    const std::string state = r->command("@4 read state feedback", 600);
    CHECK(has(state, "\"index\":24676") && has(state, "\"native_signed\":\"1234\"") &&
          has(state, "\"outcome\":\"SUCCESS\""));
    CHECK(has(r->command("@5 release 2"), "\"ok\":true"));
    CHECK(has(r->command("@6 network"), "\"id\":6,\"command\":\"network\",\"ok\":true"));
    CHECK(has(r->command("@7 heartbeat"), "\"heartbeat_seen\":false"));
    CHECK(has(r->command("@8 health"), "\"known\":true"));
    CHECK(has(r->command("@9 load"), "\"rx_frames\""));
    CHECK(has(r->command("@10 stats"), "\"counters\":["));
    CHECK(has(r->command("@11 pdo 0"), "no_configured_pdo_map"));
    CHECK(has(r->command("@12 config"), "\"bitrate_bits_per_second\":1000000,\"selected_node\":1"));
    const size_t before = r->driver.requests.size();
    const std::string monitor = r->command("@13 monitor 20 3", 1200);
    CHECK(count(monitor, "\"type\":\"value\"") == 6 && has(monitor, "\"outcome\":\"SUCCESS\""));
    CHECK(r->driver.requests.size() == before + 6);
    CHECK(has(r->command("@14 release 3"), "\"ok\":true") && !r->console.holds());
    // useaddr finds the other node's existing slot; discover passive sends nothing.
    CHECK(has(r->command("@15 useaddr 2"), "\"wire_work\":false"));
    const size_t quiet = r->driver.requests.size();
    const std::string passive = r->command("@16 discover passive 200 0 1", 600);
    CHECK(has(passive, "\"result\":\"accepted\"") && has(passive, "DISCOVERY_FINISHED"));
    CHECK(r->driver.requests.size() == quiet);
    CHECK(has(r->command("@17 release 4"), "\"ok\":true") && !r->console.holds());
    // Tracing makes the console a second trace drain: the bench refuses its own work meanwhile.
    CHECK(has(r->command("@18 debug raw"), "\"mode\":1"));
    CHECK(r->console.holds());
    CHECK(has(r->command("@19 debug off"), "\"mode\":0") && !r->console.holds());
    wholeLines(r->uart.output);
}
// Mutual exclusion with a running bench route, host recovery, and the selection guard.
void testExclusion() {
    scenario = "exclusion";
    std::unique_ptr<Rig> r(new Rig);
    r->open();
    CHECK(has(r->command("@1 host 0"), "\"ok\":true"));
    E::RoutePlan plan;
    CHECK(E::parseRouteCommand("A1,5", 4, plan));
    CHECK(!r->route.begin(plan, r->uart.now));
    r->turns(2);
    CHECK(r->route.active());
    const unsigned routeUploads = 22; // READ_ACTIONS: nine, six, then the seven-field image.
    for (const char* text : {"@2 read state", "@3 probe", "@4 host 1", "@5 monitor 50 2", "@6 stop fast",
                             "@7 stats reset", "@8 debug raw", "@9 discover passive 200 0", "@10 nmt preop addressed"}) {
        const size_t mark = r->uart.output.size();
        CHECK(r->send(text) == E::ConsoleInput::REFUSED);
        CHECK(has(r->console.reply(), "\"ok\":false,\"status\":\"INVALID_CONFIG\",\"detail\":0,"
                                      "\"error\":\"bench_wire_work_active\"}\n"));
        CHECK(r->uart.output.size() > mark);
    }
    CHECK(!r->console.holds());
    // Inspection still answers during the route.
    CHECK(has(r->command("@11 status", 100), "\"id\":11,\"command\":\"status\",\"ok\":true"));
    CHECK(has(r->command("@12 network", 100), "\"ok\":true"));
    for (unsigned i = 0; i < 20000 && r->route.active(); ++i)
        r->turn();
    CHECK(r->route.record() && r->route.record()->terminal);
    CHECK(r->route.record()->failure == E::RouteFailure::NONE && r->uploads() == routeUploads);
    // Still held until released.
    CHECK(r->send("@13 read state") == E::ConsoleInput::REFUSED);
    r->turns(5);
    CHECK(r->route.release());
    const std::string state = r->command("@14 read state", 600);
    CHECK(has(state, "\"outcome\":\"SUCCESS\"") && r->uploads() == routeUploads + 2);
    CHECK(has(r->command("@15 release 1"), "\"ok\":true"));
    // Host recovery is never forwarded on this image.
    CHECK(r->send("@16 recover") == E::ConsoleInput::REFUSED);
    CHECK(has(r->console.reply(), "bench_image_has_no_host_recovery"));
    // A bench identity read selects its own node: the console then refuses until it selects again.
    CHECK(r->firmware.app().select(r->target(2)));
    CHECK(r->send("@17 read state") == E::ConsoleInput::REFUSED);
    CHECK(has(r->console.reply(), "selection_changed_outside_console_select_again"));
    // Inspection is not refused and adopts nothing.
    CHECK(has(r->command("@18 config"), "\"selected_node\":2"));
    CHECK(r->send("@19 read state") == E::ConsoleInput::REFUSED);
    CHECK(has(r->command("@20 host 0"), "\"ok\":true"));
    CHECK(has(r->command("@21 read state", 600), "\"outcome\":\"SUCCESS\""));
    CHECK(has(r->command("@22 release 2"), "\"ok\":true"));
    // During a bench network listing a scan would drain the same observations.
    r->activity.listing = true;
    E::BenchActivity listing = r->activity;
    for (char c : std::string("@23 discover passive 200 0"))
        r->console.take(uint8_t(c), r->uart.now, listing);
    CHECK(r->console.take('\n', r->uart.now, listing) == E::ConsoleInput::REFUSED);
    CHECK(has(r->console.reply(), "bench_network_listing_active"));
    wholeLines(r->uart.output);
}
// Increment 2: the arming route, then the console's moves on the armed node through the bench's own session.
const uint8_t MOVE_WRITES[5][8] = {{0x23, 0x7a, 0x60, 0, 16, 0, 0, 0},
                                   {0x23, 0x81, 0x60, 0, 1, 0, 0, 0},
                                   {0x23, 0x83, 0x60, 0, 0xe8, 3, 0, 0},
                                   {0x23, 0x84, 0x60, 0, 0xe8, 3, 0, 0},
                                   {0x2b, 0x40, 0x60, 0, 0x5f, 0, 0, 0}};
bool sameFrame(const M::Frame& f, unsigned node, const uint8_t (&bytes)[8]) {
    return f.id == 0x600 + node && !std::memcmp(f.data, bytes, 8);
}
// Arm node 1 at the drive's present 6064 (the host's reviewed start position and ACTUAL base); the arming
// record stays held, as on the bench.
void armed(Rig& r, uint32_t& trial, ArmSpec spec = ArmSpec()) {
    spec.sp = spec.base = r.driver.drives[0].position;
    const size_t first = r.driver.requests.size();
    const E::RouteRecord& a = r.run(arm(1, trial, spec));
    if (a.failure != E::RouteFailure::NONE)
        std::fprintf(stderr, "arming failed why=%s first=%ld check=%ld sp=%ld\n", a.why, long(a.first.position),
                     long(a.check.position), long(a.plan.startPosition));
    CHECK(a.failure == E::RouteFailure::NONE && a.checkPassed);
    CHECK(r.downloadsFrom(first) == 0); // Reads only.
    r.turns(2);
}
void testArmingParse() {
    scenario = "arm-parse";
    uint32_t trial = 0;
    E::RoutePlan p;
    const std::string line = arm(1, trial);
    CHECK(E::parseRouteCommand(line.data(), line.size(), p) && p.kind == E::RouteKind::ARM_MOVE && p.attended &&
          p.relativeBase == 1 && p.stopDeadlineMs == 2000 && p.bestEffortMs == 3000 && p.stopIntervalMs == 100 &&
          p.firmware.firmwareCode == FIRMWARE && p.intervalMs == 100);
    CHECK(std::strcmp(E::routeName(E::RouteKind::ARM_MOVE), "arm-move") == 0 &&
          std::strcmp(E::routeName(E::RouteKind::ARM_VELOCITY), "arm-velocity") == 0);
    // MOVE's evidence rules: a value outside its mask, an ACTUAL base other than the start position, a
    // stop budget the stop could not use whole.
    ArmSpec bad = ArmSpec();
    bad.av = 0x0001;
    ArmSpec base = ArmSpec();
    base.base = 5;
    ArmSpec budget = ArmSpec();
    budget.be = 1000;
    ArmSpec interval = ArmSpec();
    interval.si = 5;
    for (const ArmSpec* s : {&bad, &base, &budget, &interval}) {
        const std::string text = arm(1, trial, *s);
        E::RoutePlan untouched;
        CHECK(!E::parseRouteCommand(text.data(), text.size(), untouched) && untouched.kind == E::RouteKind::NONE);
    }
    const std::string velocity = "B2,9,1,30,1000,1000,500,7,1,2," + std::to_string(DEVICE_TYPE) + "," +
                                 std::to_string(HARDWARE) + "," + std::to_string(SOFTWARE) + "," +
                                 std::to_string(VENDOR) + "," + std::to_string(PRODUCT) + "," +
                                 std::to_string(REVISION) + "," + std::to_string(MODEL) + "," +
                                 std::to_string(FIRMWARE) + ",1500,1500,500";
    CHECK(E::parseRouteCommand(velocity.data(), velocity.size(), p) && p.kind == E::RouteKind::ARM_VELOCITY &&
          p.node == 2 && p.rpm == 30 && p.layout == 1 && p.stopKind == 2 && p.intervalMs == 500);
    const std::string zh57 = "B2,9,1,30,1000,1000,500,7,3,2,1,1,1,1,1,1,1,1,1500,1500,500";
    CHECK(!E::parseRouteCommand(zh57.data(), zh57.size(), p));
}
void testArmingAndMove() {
    scenario = "arm-move";
    std::unique_ptr<Rig> r(new Rig);
    r->open();
    Drive& d = r->driver.drives[0];
    d.statusBits = true;
    uint32_t trial = 0;
    // No arming yet: the console has no motion attachment and refuses motion itself; nothing is sent.
    CHECK(has(r->command("@1 host 0"), "\"ok\":true"));
    CHECK(has(r->command("@2 move relative 16 steps native 1 configured"), "attach_actual_axis"));
    // Before the enable the arming refuses as MOVE does: no ordinary SDO qualification, no 000F yet.
    const size_t early = r->driver.requests.size();
    const char* refusal = r->start(arm(1, trial));
    CHECK(refusal && (std::strcmp(refusal, "eligibility") == 0 || std::strcmp(refusal, "control") == 0));
    CHECK(r->driver.requests.size() == early && !r->route.record());
    enabled(*r, trial);
    ArmSpec unattended;
    unattended.attended = 0;
    CHECK(std::strcmp(r->start(arm(1, trial, unattended)), "unattended") == 0);
    const uint8_t routes = r->route.motionAdmissions(); // MODE_ONE and ENABLE.
    size_t first = r->driver.requests.size();
    armed(*r, trial);
    CHECK(r->driver.requests.size() - first == 27); // Identity and two full reads, as a relative MOVE.
    CHECK(has(r->notices, "CONSOLE armed node=1 trial=") &&
          has(r->notices, "route=arm-move relative=1 witness=1 scale=0 origin=0 changes=0 changeRecord=0; ") &&
          has(r->notices, "prerequisites named, nothing sent"));
    CHECK(r->console.armedNode() == 1 && r->route.motionAdmissions() == routes && r->axis1.reference.nativeKnown);
    // Host intent, then the move while the settled arming record is still held for its dump.
    CHECK(has(r->command("@3 accel 1000", 100), "\"ok\":true"));
    CHECK(has(r->command("@4 decel 1000", 100), "\"ok\":true"));
    first = r->driver.requests.size();
    const std::string moved = r->command("@5 move relative 16 steps native 1 configured", 8000);
    CHECK(has(moved, "\"id\":5,\"command\":\"move\",\"ok\":true,\"status\":\"OK\",\"result\":\"accepted\""));
    CHECK(has(moved, "\"type\":\"terminal\",\"id\":5,\"command\":\"move\",\"ok\":true"));
    size_t at = first;
    for (const auto& bytes : MOVE_WRITES) {
        while (at < r->driver.requests.size() && r->driver.requests[at].data[0] == 0x40)
            ++at;
        CHECK(at < r->driver.requests.size() && sameFrame(r->driver.requests[at++], 1, bytes));
    }
    const uint8_t rearm[8] = {0x2b, 0x40, 0x60, 0, 0x0f, 0, 0, 0};
    CHECK(sameFrame(r->driver.requests.back(), 1, rearm) && r->downloadsFrom(first) == 6 && d.position == 16);
    const L::MoveContext& c = r->one.moveAction();
    CHECK(c.state == M::ActionState::SUCCEEDED && c.parameters.target.requested.relative &&
          c.parameters.target.referenceUsed && c.evidence.record == 19);
    // Counted with the bench routes, and retained until released: the bench admits nothing meanwhile.
    CHECK(r->route.motionAdmissions() == routes + 1 && r->console.holds());
    CHECK(has(r->command("@6 release 1", 100), "\"ok\":true") && !r->console.holds());
    // The admission consumed the borrowed prerequisites: a second move needs a new arming.
    CHECK(has(r->command("@7 move relative 16 steps native 1 configured", 100), "prerequisites"));
    CHECK(r->route.motionAdmissions() == routes + 1);
    // Route scratch reuse no longer destroys the owned arming. Its already-consumed
    // motion prerequisites still refuse through the public helper, without new work.
    r->release();
    CHECK(r->run("A1," + id(trial)).failure == E::RouteFailure::NONE);
    E::RouteArming gone;
    CHECK(!r->route.armed(gone));
    r->release();
    CHECK(has(r->command("@8 move relative 16 steps native 1 configured", 100), "prerequisites"));
    // Armed again: the per-case cap counts the console's moves with MOVE routes, three per node per boot.
    for (unsigned n = 0; n < 2; ++n) {
        armed(*r, trial);
        const std::string again = r->command("@" + std::to_string(20 + n) + " move relative 16 steps native 1 configured",
                                             8000);
        CHECK(has(again, "\"type\":\"terminal\"") && has(again, "\"command\":\"move\",\"ok\":true"));
        CHECK(has(r->command("@" + std::to_string(30 + n) + " release " + std::to_string(2 + n), 100), "\"ok\":true"));
        r->release();
    }
    CHECK(r->route.motionAdmissions() == routes + 3);
    armed(*r, trial);
    CHECK(r->send("@40 move relative 16 steps native 1 configured") == E::ConsoleInput::REFUSED);
    CHECK(has(r->console.reply(), "bench_motion_budget_spent"));
    // A stop is never refused for the caps.
    CHECK(has(r->command("@41 stop normal", 2000), "\"command\":\"stop\""));
    wholeLines(r->uart.output);
}
// A stop during a console move: its prerequisites are named at the request, from the move's own cycle.
void testConsoleStop() {
    scenario = "stop";
    std::unique_ptr<Rig> r(new Rig);
    r->open();
    Drive& d = r->driver.drives[0];
    d.statusBits = true;
    d.travelReads = 100000; // This fake's drive keeps reporting travel until a stop word.
    uint32_t trial = 0;
    enabled(*r, trial);
    CHECK(has(r->command("@1 host 0", 100), "\"ok\":true"));
    armed(*r, trial);
    CHECK(has(r->command("@2 accel 1000", 100), "\"ok\":true") && has(r->command("@3 decel 1000", 100), "\"ok\":true"));
    const uint8_t before = r->route.motionAdmissions();
    CHECK(has(r->command("@4 move relative 16 steps native 1 configured", 1500), "\"result\":\"accepted\""));
    CHECK(r->one.moveActive());
    const size_t first = r->driver.requests.size();
    const std::string stopped = r->command("@5 stop normal", 8000);
    if (std::getenv("BENCH_CONSOLE_DEBUG"))
        std::fputs(stopped.c_str(), stderr);
    CHECK(has(stopped, "\"id\":5,\"command\":\"stop\",\"ok\":true,\"status\":\"OK\",\"result\":\"accepted\""));
    CHECK(has(stopped, "\"type\":\"terminal\",\"id\":5,\"command\":\"stop\""));
    const uint8_t halt[8] = {0x2b, 0x40, 0x60, 0, 0x0f, 0x01, 0, 0};
    bool sent = false;
    for (size_t i = first; i < r->driver.requests.size(); ++i)
        sent = sent || sameFrame(r->driver.requests[i], 1, halt);
    CHECK(sent && d.lastWord == 0x010f);
    CHECK(r->route.motionAdmissions() == before + 2); // The move and its stop.
    wholeLines(r->uart.output);
}
// The simple routes: a host command scale through ProbeMotion with the arming's witness, which ends the
// arming's prerequisites and advances the binding's configuration epoch, then moveby after a new arming.
void testScaleAndMoveby() {
    scenario = "moveby";
    std::unique_ptr<Rig> r(new Rig);
    r->open();
    Drive& d = r->driver.drives[0];
    d.statusBits = true;
    uint32_t trial = 0;
    // MODE_ONE and a heartbeat first, as before an enable: the owner's configuration advance needs ordinary
    // SDO service, and the witness arming requires it.
    CHECK(r->run("M1," + id(trial) + ",1,1,200").failure == E::RouteFailure::NONE);
    r->release();
    r->heartbeat(1);
    CHECK(has(r->command("@1 host 0", 100), "\"ok\":true"));
    // Without a witness the scale is refused by the core's stationary-evidence rule; nothing is sent.
    CHECK(has(r->command("@2 stepsperturn 1600", 100), "attach_actual_axis"));
    const std::string witness = "X1," + id(trial) + ",1," + std::to_string(d.position);
    size_t first = r->driver.requests.size();
    CHECK(r->run(witness).failure == E::RouteFailure::NONE && r->downloadsFrom(first) == 0);
    CHECK(r->driver.requests.size() - first == 18);
    r->turns(2);
    CHECK(has(r->notices, "route=arm-witness relative=0 witness=1 scale=0 origin=0"));
    const uint32_t epoch = r->target(1).channel.binding.configurationGeneration;
    CHECK(has(r->command("@3 stepsperturn 1600", 100), "\"id\":3,\"command\":\"stepsperturn\",\"ok\":true"));
    CHECK(r->axis1.axis.units.commandStepsPerMotorTurn.source == M::ScaleSource::ASSUMED &&
          r->axis1.axis.units.commandStepsPerMotorTurn.numerator == 1600);
    CHECK(r->target(1).channel.binding.configurationGeneration == epoch + 1 &&
          M::sameBinding(r->one.binding(), r->target(1).channel.binding));
    // The scale consumed the witness: an origin needs a fresh one.
    CHECK(has(r->command("@4 axis origin 0", 100), "\"ok\":false"));
    r->release();
    CHECK(r->run("X1," + id(trial) + ",1," + std::to_string(d.position)).failure == E::RouteFailure::NONE);
    r->turns(2);
    CHECK(has(r->command("@5 axis origin 0", 100), "\"id\":5,\"command\":\"axis\",\"ok\":true"));
    CHECK(r->axis1.axis.originKnown && r->axis1.axis.originNative == 0);
    r->release();
    // The enable after the host coordinates: their configuration advances dropped no word that mattered.
    CHECK(r->run("E1," + id(trial) + ",1,1,1,32823,5000,120,10000,800").failure == E::RouteFailure::NONE);
    r->release();
    CHECK(has(r->command("@6 speed 1", 100), "\"ok\":true") && has(r->command("@7 accel 1000", 100), "\"ok\":true") &&
          has(r->command("@8 decel 1000", 100), "\"ok\":true"));
    r->notices.clear();
    armed(*r, trial);
    CHECK(has(r->notices, "relative=1 witness=1 scale=1 origin=1"));
    first = r->driver.requests.size();
    const std::string moved = r->command("@9 moveby 16", 8000);
    CHECK(has(moved, "\"type\":\"terminal\",\"id\":9,\"command\":\"moveby\",\"ok\":true"));
    CHECK(r->downloadsFrom(first) == 6 && d.position == 16 && r->one.moveAction().state == M::ActionState::SUCCEEDED);
    CHECK(has(r->command("@10 release 1", 100), "\"ok\":true"));
    r->release();
    // An absolute target in steps of the host's motor frame, from its explicit origin.
    armed(*r, trial);
    first = r->driver.requests.size();
    const std::string to = r->command("@11 moveto 0", 8000);
    if (!has(to, "\"type\":\"terminal\",\"id\":11,\"command\":\"moveto\",\"ok\":true"))
        std::fprintf(stderr, "%s\n", to.c_str());
    CHECK(has(to, "\"type\":\"terminal\",\"id\":11,\"command\":\"moveto\",\"ok\":true") && d.position == 0);
    wholeLines(r->uart.output);
}
// This fake's released mode 3 standstill, as the bench route suite's VELOCITY starts from, with node 1 selected.
void velocityRig(Rig& r, uint32_t& trial) {
    r.open();
    Drive& d = r.driver.drives[0];
    d.monitor = 0x0011; // As the bench route suite's released mode 3 drive: released, then spins on 0007-000F.
    d.status = 0x8050;
    d.mode = 3;
    d.releaseWords = true;
    d.spins = true;
    CHECK(r.run("A1," + id(trial)).failure == E::RouteFailure::NONE); // Service qualification, as before VELOCITY.
    r.release();
    r.heartbeat(1);
    CHECK(has(r.command("@1 host 0", 100), "\"ok\":true"));
}
// The velocity arming line: HALT (1) or QUICK_STOP (2) as the run's stop kind, then the stop budgets.
std::string velocityArming(uint32_t& trial, unsigned stopKind = 1, const char* budgets = "1500,1500,500",
                           unsigned maximumRpm = 6) {
    return "B1," + id(trial) + ",1," + std::to_string(maximumRpm) + ",200,200,5000,351,2," + std::to_string(stopKind) + "," +
           std::to_string(DEVICE_TYPE) + "," + std::to_string(HARDWARE) + "," + std::to_string(SOFTWARE) + "," +
           std::to_string(VENDOR) + "," + std::to_string(PRODUCT) + "," + std::to_string(REVISION) + "," +
           std::to_string(MODEL) + "," + std::to_string(FIRMWARE) + "," + budgets;
}
const uint8_t HALT_WORD[8] = {0x2b, 0x40, 0x60, 0, 0x0f, 0x01, 0, 0};
const uint8_t QUICK_WORD[8] = {0x2b, 0x40, 0x60, 0, 0x0b, 0, 0, 0};
unsigned sentFrom(const Rig& r, size_t first, const uint8_t (&bytes)[8]) {
    unsigned n = 0;
    for (size_t i = first; i < r.driver.requests.size(); ++i)
        n += sameFrame(r.driver.requests[i], 1, bytes);
    return n;
}
// A console velocity of 12 s, admitted and turning at the end; returns the number of requests before it.
size_t velocityRunning(Rig& r, uint32_t& trial, unsigned stopKind = 1, const char* budgets = "1500,1500,500") {
    CHECK(r.run(velocityArming(trial, stopKind, budgets)).failure == E::RouteFailure::NONE);
    r.turns(2);
    const size_t first = r.driver.requests.size();
    const std::string kind = stopKind == 1 ? "normal" : "fast";
    CHECK(has(r.command("@3 velocity 6 rpm native 12000 configured " + kind, 1500), "\"result\":\"accepted\""));
    CHECK(r.one.velocityActive() && r.one.velocityNeedsStop() && r.driver.drives[0].velocity != 0);
    CHECK(!sentFrom(r, first, HALT_WORD) && !sentFrom(r, first, QUICK_WORD));
    return first;
}
// Service until the run's own due time has passed and every stop has settled.
void settle(Rig& r) {
    for (unsigned i = 0; i < 30000 && (r.one.velocityActive() || r.one.stopActive()); ++i)
        r.turn();
    r.turns(200);
    CHECK(!r.one.velocityActive() && !r.one.stopActive());
}
void testExplicitVelocityChangeArming() {
    for (unsigned variant = 0; variant != 3; ++variant) {
        const bool allow = variant != 0, exhausted = variant == 2;
        const bool accepted = allow && !exhausted;
        scenario = exhausted ? "velocity-change-target-budget" :
                   allow ? "velocity-change-explicit-arm" : "velocity-change-closed-arm";
        std::unique_ptr<Rig> r(new Rig);
        uint32_t trial = 0;
        velocityRig(*r, trial);
        // Give the other axis a witness arming, or unused velocity capacity for the cap case.
        r->driver.drives[1].statusBits = true;
        CHECK(r->run("M2," + id(trial) + ",1,1,200").failure == E::RouteFailure::NONE);
        r->release();
        r->heartbeat(2);
        if (exhausted) {
            Drive& other = r->driver.drives[1];
            other.mode = 3;
            other.monitor = 0x0011;
            other.status = 0x8050;
            other.releaseWords = other.spins = true;
            std::string otherArming = velocityArming(trial, 2, "1500,1500,500", 30);
            otherArming[1] = '2';
            CHECK(r->run(otherArming).failure == E::RouteFailure::NONE);
        } else {
            CHECK(r->run("X2," + id(trial) + ",1," + std::to_string(r->driver.drives[1].position)).failure ==
                  E::RouteFailure::NONE);
        }
        r->turns(2);
        r->release();
        std::string line = velocityArming(trial, 2, "1500,1500,500", 30);
        if (allow)
            line += ",1";
        CHECK(r->run(line).failure == E::RouteFailure::NONE);
        r->turns(2);
        CHECK(has(r->notices, allow ? "changes=1 changeRecord=" : "changes=0 changeRecord=0"));
        CHECK(has(r->command("@20 velocity 6 rpm native 12000 configured fast", 100), "\"result\":\"accepted\""));
        for (unsigned n = 0; n < 10000 &&
             !(r->one.velocityAction().phase == L::VelocityPhase::WAITING && r->one.velocityAction().runningPolls); ++n)
            r->turn();
        const L::VelocityContext& run = r->one.velocityAction();
        CHECK(r->one.velocityActive() && run.runningPolls && run.phase == L::VelocityPhase::WAITING);
        const uint64_t due = run.stopDueUs, deadline = run.deadlineUs;
        // The operation ID still names node 1 after selection moves to node 2.
        CHECK(has(r->command("@25 useaddr 2", 20), "\"ok\":true"));
        if (exhausted) {
            // Model retained campaign pressure on this axis; the other still has capacity.
            r->route.countConsoleMotion(1, E::RouteKind::VELOCITY);
            r->route.countConsoleMotion(1, E::RouteKind::VELOCITY);
            CHECK(!r->route.consoleMotionAllowed(1, E::RouteKind::VELOCITY));
            CHECK(r->route.consoleMotionAllowed(2, E::RouteKind::VELOCITY));
        }
        const size_t first = r->driver.requests.size();
        const std::string changed = r->command("@21 velocity change 1 12 rpm native configured", 100);
        CHECK(has(changed, accepted ? "\"result\":\"accepted\"" : "\"ok\":false"));
        CHECK(run.change.present == accepted && run.stopDueUs == due && run.deadlineUs == deadline &&
              run.rate.effectiveNative == 6);
        if (accepted) {
            CHECK(run.change.evidence.gate == L::VelocityGate::ATTENDED_EXPERIMENT &&
                  run.change.evidence.record == trial && !run.change.evidence.rateChangeQualified);
            CHECK(run.change.rate.effectiveNative == 12 && run.change.serviceEvidencePresent);
            CHECK(r->downloadsFrom(first) == 1);
        } else
            CHECK(r->downloadsFrom(first) == 0);
        settle(*r);
        CHECK(!r->one.velocityNeedsStop());
        CHECK(r->firmware.app().selected()->channel.binding.logicalAxisId == 2);
        CHECK(has(r->command("@26 useaddr 1", 20), "\"ok\":true"));
        if (accepted) {
            CHECK(has(r->command("@22 release 1", 100), "\"ok\":true"));
            r->release();
            CHECK(r->run(velocityArming(trial, 2, "1500,1500,500", 30)).failure == E::RouteFailure::NONE);
            r->turns(2);
            CHECK(has(r->command("@23 velocity 6 rpm native 12000 configured fast", 100), "\"result\":\"accepted\""));
            for (unsigned n = 0; n < 10000 &&
                 !(r->one.velocityAction().phase == L::VelocityPhase::WAITING && r->one.velocityAction().runningPolls); ++n)
                r->turn();
            const size_t before = r->driver.requests.size();
            CHECK(has(r->command("@24 velocity change 2 12 rpm native configured", 100), "\"ok\":false"));
            CHECK(!r->one.velocityAction().change.present && r->downloadsFrom(before) == 0);
            settle(*r);
        }
    }
}
void testExplicitMoveChangeArming() {
    for (unsigned allow = 0; allow != 2; ++allow) {
        scenario = allow ? "move-change-explicit-arm" : "move-change-ordinary-rearm-closes";
        std::unique_ptr<Rig> r(new Rig);
        r->open();
        Drive& d = r->driver.drives[0];
        d.statusBits = true;
        d.travelReads = 100000; // Manufactured continuing travel, no completion law.
        uint32_t trial = 0;
        enabled(*r, trial);
        CHECK(has(r->command("@1 host 0", 50), "\"ok\":true"));
        ArmSpec spec;
        spec.rel = 0;
        CHECK(r->run(arm(1, trial, spec) + ",1").failure == E::RouteFailure::NONE);
        r->turns(2);
        if (!allow) {
            r->release();
            CHECK(r->run(arm(1, trial, spec)).failure == E::RouteFailure::NONE);
            r->turns(2); // A successful ordinary arming must withdraw earlier change permission.
        }
        CHECK(has(r->command("@2 accel 500", 50), "\"ok\":true"));
        CHECK(has(r->command("@3 decel 500", 50), "\"ok\":true"));
        d.velocity = 30; // Only the fake's post-trigger feedback reports this native running rate.
        CHECK(r->send("@4 move absolute 800 steps native 30 configured") == E::ConsoleInput::FORWARDED);
        for (unsigned n = 0; n < 2000 &&
             !(r->one.moveAction().latest.present && r->one.moveAction().phase == L::MovePhase::WAITING); ++n)
            r->turn();
        CHECK(r->one.moveActive() && r->one.moveAction().latest.present);
        const uint64_t deadline = r->one.moveAction().deadlineUs;
        size_t first = r->driver.requests.size();
        const std::string retarget = r->command("@5 move retarget 1 absolute 1600 steps native 30 configured", 30);
        CHECK(has(retarget, "\"ok\":false") && r->downloadsFrom(first) == 0);
        for (unsigned n = 0; n < 2000 && r->one.moveAction().phase != L::MovePhase::WAITING; ++n)
            r->turn();
        first = r->driver.requests.size();
        const std::string changed = r->command("@6 move speed 1 60 immediate", 100);
        CHECK(has(changed, allow ? "\"result\":\"accepted\"" : "\"ok\":false"));
        CHECK(r->one.moveAction().deadlineUs == deadline);
        if (allow) {
            const L::MoveContext& native = r->one.moveAction();
            CHECK(native.replacementEvidence.gate == L::MoveChangeGate::ATTENDED_EXPERIMENT &&
                  native.replacementEvidence.record == trial && !native.replacementEvidence.parameterMask &&
                  !native.replacementEvidence.immediateQualified && !native.replacementEvidence.completionQualified &&
                  !native.replacementEvidence.stopQualified);
            CHECK(native.parameters.targetNative == 800 && native.speed.effectiveNative == 60);
            CHECK(r->one.originalMoveAction().interruption == M::MoveInterruption::REPLACED &&
                  r->one.originalMoveAction().speed.effectiveNative == 30);
            CHECK(r->axis1.motion.normalResult(1) && r->axis1.motion.normalResult().id != 1);
            CHECK(r->downloadsFrom(first) > 0);
            CHECK(has(r->uart.output, "\"physical_interruption_observed\":false"));
        } else {
            CHECK(!r->one.moveAction().relation.originalOperationId && r->downloadsFrom(first) == 0);
        }
    }
}
// The velocity arming from this fake's released mode 3 standstill, then the console's finite velocity run with
// its own stop at the due time, counted as a VELOCITY route and its stop.
void testVelocityArming() {
    scenario = "velocity";
    std::unique_ptr<Rig> r(new Rig);
    Drive& d = r->driver.drives[0];
    uint32_t trial = 0;
    velocityRig(*r, trial);
    const uint8_t before = r->route.motionAdmissions();
    const std::string line = velocityArming(trial);
    size_t first = r->driver.requests.size();
    const E::RouteRecord& a = r->run(line);
    CHECK(a.failure == E::RouteFailure::NONE && a.checkPassed && r->downloadsFrom(first) == 0);
    CHECK(r->driver.requests.size() - first == 18);
    r->turns(2);
    CHECK(has(r->notices, "route=arm-velocity relative=0 witness=0"));
    // The run's stop kind is the arming line's, HALT: the library refuses the other.
    CHECK(has(r->command("@2 velocity 6 rpm native 12000 configured fast", 100), "\"ok\":false"));
    CHECK(r->route.motionAdmissions() == before);
    first = r->driver.requests.size();
    const std::string run = r->command("@3 velocity 6 rpm native 12000 configured normal", 30000);
    CHECK(has(run, "\"id\":3,\"command\":\"velocity\",\"ok\":true,\"status\":\"OK\",\"result\":\"accepted\""));
    CHECK(has(run, "\"type\":\"terminal\",\"id\":3,\"command\":\"velocity\",\"ok\":true"));
    const L::VelocityContext& c = r->one.velocityAction();
    CHECK(c.state == M::ActionState::SUCCEEDED && c.rate.effectiveNative == 6 && !c.needsStop);
    const uint8_t halt[8] = {0x2b, 0x40, 0x60, 0, 0x0f, 0x01, 0, 0};
    bool stopped = false;
    for (size_t i = first; i < r->driver.requests.size(); ++i)
        stopped = stopped || sameFrame(r->driver.requests[i], 1, halt);
    CHECK(stopped && d.velocity == 0);
    CHECK(r->route.motionAdmissions() == before + 2); // The run and its own stop, as VELOCITY counts them.
    wholeLines(r->uart.output);
}
// A local cancel of a running console velocity would withdraw its own stop at the due time, its only software
// bound: ProbeMotion refuses it, by request ID and without one, and the run's stop still leaves at its due time.
void testVelocityCancelRefused() {
    scenario = "velocity-cancel";
    std::unique_ptr<Rig> r(new Rig);
    Drive& d = r->driver.drives[0];
    uint32_t trial = 0;
    velocityRig(*r, trial);
    const size_t first = velocityRunning(*r, trial);
    const std::string reply = r->uart.output.substr(r->uart.output.rfind("\"id\":3,\"command\":\"velocity\""));
    const size_t at = reply.find("\"operation_id\":");
    CHECK(at != std::string::npos);
    const std::string operation = std::to_string(std::strtoul(reply.c_str() + at + 15, nullptr, 10));
    const std::string bare = r->command("@4 cancel", 100);
    const std::string named = r->command("@5 cancel " + operation, 100);
    CHECK(has(bare, "\"id\":4,\"command\":\"cancel\",\"ok\":false"));
    CHECK(has(named, "\"id\":5,\"command\":\"cancel\",\"ok\":false") && has(named, "not_cancellable"));
    const L::VelocityContext& c = r->one.velocityAction();
    CHECK(r->one.velocityActive() && c.interruption == M::VelocityInterruption::NONE && d.velocity != 0);
    CHECK(!sentFrom(*r, first, HALT_WORD) && !sentFrom(*r, first, QUICK_WORD));
    settle(*r);
    CHECK(has(r->uart.output, "\"type\":\"terminal\",\"id\":3,\"command\":\"velocity\",\"ok\":true"));
    CHECK(sentFrom(*r, first, HALT_WORD) == 1 && d.velocity == 0 && d.lastWord == 0x010f);
    CHECK(c.state == M::ActionState::SUCCEEDED && !r->one.velocityNeedsStop());
    wholeLines(r->uart.output);
}
// An explicit console stop during a running console velocity: AxisSession::requestStop withdraws the run, and
// with it its own stop at the due time, before it validates the explicit one. Whatever that stop's outcome,
// accepted, refused or never valid, a stop word still leaves for the turning drive.
void testVelocityExplicitStops() {
    // stop normal, the run's own kind: the explicit HALT is the stop.
    {
        scenario = "velocity-stop-normal";
        std::unique_ptr<Rig> r(new Rig);
        uint32_t trial = 0;
        velocityRig(*r, trial);
        const size_t first = velocityRunning(*r, trial);
        const std::string stopped = r->command("@4 stop normal", 4000);
        CHECK(has(stopped, "\"id\":4,\"command\":\"stop\",\"ok\":true,\"status\":\"OK\",\"result\":\"accepted\""));
        CHECK(r->one.velocityAction().interruption == M::VelocityInterruption::STOP);
        settle(*r);
        CHECK(sentFrom(*r, first, HALT_WORD) == 1 && r->driver.drives[0].velocity == 0 &&
              r->driver.drives[0].lastWord == 0x010f);
        CHECK(has(r->uart.output, "\"type\":\"terminal\",\"id\":4,\"command\":\"stop\""));
        wholeLines(r->uart.output);
    }
    // stop fast on a run whose own stop is HALT: QUICK_STOP leaves (sequenced or best effort, 000B either way).
    for (const char* budgets : {"1500,1500,500", "1500,0,500"}) {
        scenario = budgets[5] == '0' ? "velocity-stop-fast-no-best-effort" : "velocity-stop-fast";
        std::unique_ptr<Rig> r(new Rig);
        uint32_t trial = 0;
        velocityRig(*r, trial);
        const size_t first = velocityRunning(*r, trial, 1, budgets);
        const std::string stopped = r->command("@4 stop fast", 4000);
        if (std::getenv("BENCH_CONSOLE_DEBUG"))
            std::fputs(stopped.c_str(), stderr);
        CHECK(has(stopped, "\"id\":4,\"command\":\"stop\",\"ok\":true,\"status\":\"OK\",\"result\":\"accepted\""));
        settle(*r);
        CHECK(sentFrom(*r, first, QUICK_WORD) == 1 && !sentFrom(*r, first, HALT_WORD));
        CHECK(r->driver.drives[0].velocity == 0 && r->driver.drives[0].lastWord == 0x000b);
        wholeLines(r->uart.output);
    }
    // A refused explicit stop: stop normal after stop fast. The fast stop already left; the refusal sends nothing.
    {
        scenario = "velocity-stop-refused";
        std::unique_ptr<Rig> r(new Rig);
        uint32_t trial = 0;
        velocityRig(*r, trial);
        const size_t first = velocityRunning(*r, trial, 2);
        CHECK(has(r->command("@4 stop fast", 4000), "\"id\":4,\"command\":\"stop\",\"ok\":true"));
        const size_t before = r->driver.requests.size();
        CHECK(has(r->command("@5 stop normal", 100), "\"id\":5,\"command\":\"stop\",\"ok\":false"));
        CHECK(!sentFrom(*r, before, HALT_WORD));
        settle(*r);
        CHECK(sentFrom(*r, first, QUICK_WORD) >= 1 && r->driver.drives[0].velocity == 0);
        wholeLines(r->uart.output);
    }
    // An explicit stop refused after the withdrawal: stop normal while the run's own due-time HALT is the active
    // stop. AxisSession withdraws the run, then refuses a second HALT; the run's own stop still leaves.
    {
        scenario = "velocity-stop-during-own-stop";
        std::unique_ptr<Rig> r(new Rig);
        uint32_t trial = 0;
        velocityRig(*r, trial);
        const size_t first = velocityRunning(*r, trial);
        for (unsigned i = 0; i < 30000 && !r->one.stopActive(); ++i)
            r->turn();
        CHECK(r->one.stopActive() && r->one.velocityActive());
        const std::string refused = r->command("@4 stop normal", 200);
        if (std::getenv("BENCH_CONSOLE_DEBUG"))
            std::fputs(refused.c_str(), stderr);
        CHECK(has(refused, "\"id\":4,\"command\":\"stop\",\"ok\":false"));
        settle(*r);
        CHECK(sentFrom(*r, first, HALT_WORD) == 1 && r->driver.drives[0].velocity == 0);
        wholeLines(r->uart.output);
    }
    // A stop line the console never admits: the run is untouched and its own stop leaves at the due time.
    {
        scenario = "velocity-stop-invalid";
        std::unique_ptr<Rig> r(new Rig);
        uint32_t trial = 0;
        velocityRig(*r, trial);
        const size_t first = velocityRunning(*r, trial);
        CHECK(has(r->command("@4 stop sideways", 100), "\"id\":4,\"command\":\"stop\",\"ok\":false"));
        CHECK(r->one.velocityActive() && r->one.velocityAction().interruption == M::VelocityInterruption::NONE);
        settle(*r);
        CHECK(sentFrom(*r, first, HALT_WORD) == 1 && r->driver.drives[0].velocity == 0 && !r->one.velocityNeedsStop());
        wholeLines(r->uart.output);
    }
}
// A refused arming applies nothing: its reads are not current, so the console's motion line is refused before
// the console sees it. main.cpp refuses a T line while the console retains an operation; this rig starts the
// route directly so that BenchConsole::arm itself meets the refusal.
void testRefusedArming() {
    scenario = "arming-refused";
    std::unique_ptr<Rig> r(new Rig);
    r->open();
    r->driver.drives[0].statusBits = true;
    uint32_t trial = 0;
    enabled(*r, trial);
    CHECK(has(r->command("@1 host 0", 100), "\"ok\":true"));
    armed(*r, trial);
    CHECK(has(r->command("@2 accel 1000", 100), "\"ok\":true") && has(r->command("@3 decel 1000", 100), "\"ok\":true"));
    const std::string moved = r->command("@4 move relative 16 steps native 1 configured", 8000);
    CHECK(has(moved, "\"type\":\"terminal\",\"id\":4,\"command\":\"move\",\"ok\":true") && r->console.holds());
    r->release();
    // The retained move result refuses the new prerequisites (ProbeMotion): the arming is refused whole.
    r->notices.clear();
    ArmSpec spec;
    spec.sp = spec.base = r->driver.drives[0].position;
    CHECK(r->run(arm(1, trial, spec)).failure == E::RouteFailure::NONE);
    r->turns(2);
    CHECK(has(r->notices, ("CONSOLE arm refused node=1 trial=" + std::to_string(trial)).c_str()));
    CHECK(has(r->command("@5 release 1", 100), "\"ok\":true") && !r->console.holds());
    // The refused arming did not replenish consumed prerequisites; nothing is counted or sent.
    const size_t first = r->driver.requests.size();
    const uint8_t admissions = r->route.motionAdmissions();
    CHECK(has(r->command("@6 move relative 16 steps native 1 configured", 100), "prerequisites"));
    r->turns(20);
    CHECK(r->downloadsFrom(first) == 0 && r->route.motionAdmissions() == admissions);
    wholeLines(r->uart.output);
}
// One node's stop budgets never name another node's stop: a witness arming of node 2 names none, so a console
// stop there has no prerequisites and sends nothing, and node 1's own budgets come back with its next arming.
void testBudgetsPerNode() {
    scenario = "budgets-per-node";
    std::unique_ptr<Rig> r(new Rig);
    r->open();
    r->driver.drives[0].statusBits = true;
    r->driver.drives[1].statusBits = true;
    uint32_t trial = 0;
    enabled(*r, trial);
    CHECK(has(r->command("@1 host 0", 100), "\"ok\":true"));
    ArmSpec spec;
    spec.sd = 2500;
    armed(*r, trial, spec);
    r->release();
    CHECK(r->run("M2," + id(trial) + ",1,1,200").failure == E::RouteFailure::NONE);
    r->release();
    r->heartbeat(2);
    r->notices.clear();
    CHECK(r->run("X2," + id(trial) + ",1," + std::to_string(r->driver.drives[1].position)).failure ==
          E::RouteFailure::NONE);
    r->turns(2);
    CHECK(has(r->notices, "CONSOLE armed node=2") && r->console.armedNode() == 2);
    CHECK(has(r->command("@2 host 1", 100), "\"ok\":true"));
    size_t first = r->driver.requests.size();
    CHECK(has(r->command("@3 stop normal", 400), "\"id\":3,\"command\":\"stop\",\"ok\":false"));
    for (size_t i = first; i < r->driver.requests.size(); ++i)
        CHECK(r->driver.requests[i].id != 0x602 || r->driver.requests[i].data[0] == 0x40);
    // Node 1 armed again: its stop is named from its own arming line's budgets.
    r->release();
    CHECK(has(r->command("@4 host 0", 100), "\"ok\":true"));
    armed(*r, trial, spec);
    CHECK(r->console.armedNode() == 1);
    first = r->driver.requests.size();
    const uint64_t at = r->uart.now;
    CHECK(has(r->command("@5 stop normal", 4000), "\"id\":5,\"command\":\"stop\",\"ok\":true"));
    CHECK(sentFrom(*r, first, HALT_WORD) == 1);
    const L::ActionContext& halt = r->one.stopAction(L::StopKind::HALT);
    CHECK(halt.deadlineUs >= at + 2500000ull && halt.deadlineUs <= at + 2500000ull + 10000);
    wholeLines(r->uart.output);
}
// A drive fault reported after the arming holds back the move's first download unsent.
void testFaultHoldsBack() {
    scenario = "fault";
    std::unique_ptr<Rig> r(new Rig);
    r->open();
    r->driver.drives[0].statusBits = true;
    uint32_t trial = 0;
    enabled(*r, trial);
    CHECK(has(r->command("@1 host 0", 100), "\"ok\":true"));
    armed(*r, trial);
    CHECK(has(r->command("@2 accel 1000", 100), "\"ok\":true") && has(r->command("@3 decel 1000", 100), "\"ok\":true"));
    M::Frame emcy = frame(0x81, 8);
    emcy.data[0] = 0x10; // A nonzero error code from this fake, not a CL86-C code.
    emcy.data[1] = 0x23;
    r->driver.push(emcy);
    r->turns(3);
    CHECK(r->services.node(0)->emcyCount == 1);
    const size_t first = r->driver.requests.size();
    const std::string moved = r->command("@4 move relative 16 steps native 1 configured", 3000);
    CHECK(has(moved, "\"result\":\"accepted\"") && has(moved, "\"type\":\"terminal\",\"id\":4,\"command\":\"move\",\"ok\":false"));
    CHECK(r->downloadsFrom(first) == 0 && r->one.faultBlocked());
}
void testTwoArmedAxes() {
    scenario = "two-owned-armings-independent-stop";
    std::unique_ptr<Rig> r(new Rig);
    r->open();
    for (auto& drive : r->driver.drives) {
        drive.statusBits = true;
        drive.travelReads = 100000;
    }
    uint32_t trial = 0;
    enabled(*r, trial, 1);
    enabled(*r, trial, 2);
    CHECK(has(r->command("@1 host 0", 20), "\"ok\":true"));
    ArmSpec spec;
    CHECK(r->run(arm(1, trial, spec)).failure == E::RouteFailure::NONE);
    r->turns(2);
    const uint64_t firstReadUs = r->axis1.armedState.servicedUs;
    const uint32_t firstTrial = r->axis1.armedTrial;
    r->release();
    CHECK(r->run(arm(2, trial, spec)).failure == E::RouteFailure::NONE);
    r->turns(2);
    CHECK(r->axis1.armedTrial == firstTrial && r->axis1.armedState.servicedUs == firstReadUs);
    CHECK(r->axis2.armedTrial != firstTrial);
    CHECK(r->console.console().motionForAxis(1) == &r->axis1.motion);
    CHECK(r->console.console().motionForAxis(2) == &r->axis2.motion);
    CHECK(has(r->command("@2 useaddr 1", 20), "\"ok\":true"));
    CHECK(has(r->command("@3 accel 1000", 20), "\"ok\":true"));
    CHECK(has(r->command("@4 decel 1000", 20), "\"ok\":true"));
    const unsigned admissions = r->route.motionAdmissions();
    CHECK(has(r->command("@5 move relative 16 steps native 1 configured", 250), "\"result\":\"accepted\""));
    CHECK(r->one.moveActive());
    CHECK(has(r->command("@6 useaddr 2", 20), "\"ok\":true"));
    CHECK(has(r->command("@7 accel 1000", 20), "\"ok\":true"));
    CHECK(has(r->command("@8 decel 1000", 20), "\"ok\":true"));
    CHECK(has(r->command("@9 move relative 32 steps native 1 configured", 250), "\"result\":\"accepted\""));
    CHECK(r->one.moveActive() && r->two.moveActive());
    CHECK(r->route.motionAdmissions() == admissions + 2);
    // Select the first axis, then address the second stop explicitly. Selection is unchanged.
    CHECK(has(r->command("@10 useaddr 1", 20), "\"ok\":true"));
    const size_t start = r->driver.requests.size();
    const std::string stopped = r->command("@11 stop 2 fast", 500);
    CHECK(has(stopped, "\"result\":\"accepted\""));
    CHECK(r->one.moveActive() && !r->two.moveActive());
    bool secondStop = false;
    for (size_t i = start; i < r->driver.requests.size(); ++i) {
        const M::Frame& frame = r->driver.requests[i];
        if (frame.data[0] == 0x2b && frame.data[1] == 0x40 && frame.data[2] == 0x60) {
            CHECK(frame.id == 0x602); // No first-axis controlword caused by the second-axis stop.
            secondStop = true;
        }
    }
    CHECK(secondStop && r->route.motionAdmissions() == admissions + 3);
    CHECK(r->firmware.app().selected()->channel.binding.logicalAxisId == 1);
    // The second axis retains its earlier fast stop, so it refuses another. That
    // refusal must not prevent the group from stopping the first axis.
    const std::string group = r->command("@12 stop all fast", 1000);
    CHECK(has(group, "\"type\":\"group_stop_axis\""));
    CHECK(!r->one.moveActive() && r->route.motionAdmissions() == admissions + 4);
    wholeLines(r->uart.output);
}
// main.cpp's loop around the rig, in main.cpp's order and through the image's own policy (benchActivity,
// trialLineRefusal, dumpRefusal in BenchConsole.h): its T lines, record report and dump (dumpTrial), its
// retained one-shot response (line, flushResponse) and the console's turn, over a UART that takes
// Console::rate bytes per 1 ms turn. The pair summary is a short stand-in for main.cpp's TRIAL_PAIR text.
struct Image {
    Rig& r;
    bool dumping = false, reported = true, pairUndumped = false;
    unsigned cursor = 0, lastTurns = 0;
    std::string retained;
    explicit Image(Rig& rig) : r(rig) {}
    E::BenchHolds holds() const {
        E::BenchHolds h;
        h.trial = r.route.record();
        h.dumping = dumping;
        h.retained = !retained.empty();
        h.console = r.console.holds();
        h.controller = true;
        return h;
    }
    bool written(const std::string& text) {
        return r.firmware.platform().writeOutput(reinterpret_cast<const uint8_t*>(text.data()), text.size());
    }
    // main.cpp's line(): written, else retained when nothing is, else dropped.
    void line(const std::string& text) {
        if (retained.empty() && written(text))
            return;
        if (retained.empty() && text.size() < 320)
            retained = text;
    }
    bool dumpTrial() {
        char out[256];
        bool last = false;
        if (!r.route.formatLine(cursor, out, sizeof out, &last))
            return false;
        if (written(out)) {
            ++cursor;
            return !last;
        }
        return true;
    }
    void turn() {
        r.uart.now += 1000;
        r.uart.room = r.uart.rate;
        r.driver.deliver();
        r.firmware.service();
        r.route.service(r.firmware.owner().lastTimeUs());
        const E::RouteRecord* t = r.route.record();
        const bool pair = t && E::isArmingPair(t->plan.kind);
        if (t && t->terminal && !reported && !(pair && !retained.empty())) {
            reported = true;
            r.services.clearUploadExperiment(0);
            r.services.clearUploadExperiment(1);
            if (pair) {
                pairUndumped = true;
                line("TRIAL_PAIR id=" + std::to_string(t->plan.trial) + " route=" + E::routeName(t->plan.kind) +
                     " failure=" + std::to_string(unsigned(t->failure)) + "\n");
            } else {
                dumping = true;
                cursor = 0;
            }
        }
        if (dumping)
            dumping = dumpTrial();
        if (!retained.empty() && written(retained))
            retained.clear();
        r.bench = holds();
        r.derive();
        r.console.service(r.uart.now, r.activity);
        if (r.console.notice() && retained.empty()) {
            r.notices += r.console.notice();
            line(r.console.notice());
            r.console.clearNotice();
        }
    }
    void turns(unsigned n) {
        for (unsigned i = 0; i < n; ++i)
            turn();
    }
    // main.cpp takes no input while a response is retained.
    void room() {
        for (unsigned i = 0; i < 20000 && !retained.empty(); ++i)
            turn();
        CHECK(retained.empty());
    }
    // A T line without its T, as main.cpp's input path takes it. @return What the bench printed for it.
    std::string trial(const std::string& text) {
        room();
        const size_t mark = r.uart.output.size();
        const char* refusal = E::trialLineRefusal(holds());
        if (!refusal) {
            E::RoutePlan plan;
            refusal = E::parseRouteCommand(text.data(), text.size(), plan) ? r.route.begin(plan, r.uart.now) : "syntax";
        }
        if (refusal)
            line(std::string("TRIAL_REFUSED reason=") + refusal + "; no work\n");
        else {
            reported = false;
            line(std::string("TRIAL admitted route=") + E::routeName(r.route.record()->plan.kind) +
                 " node=" + std::to_string(r.route.record()->plan.node) + "\n");
        }
        turns(40);
        return r.uart.output.substr(mark);
    }
    // main.cpp's `d` and `r` for a route record.
    std::string key(char c) {
        room();
        const size_t mark = r.uart.output.size();
        const E::RouteRecord* t = r.route.record();
        if (c == 'd') {
            const char* refusal = E::dumpRefusal(holds(), t && t->terminal);
            if (!refusal) {
                dumping = true;
                cursor = 0;
                pairUndumped = false;
            } else
                line(std::strcmp(refusal, "console") == 0
                         ? "DUMP refused reason=console; release the console's results first\n"
                         : "DUMP unavailable\n");
        } else if (c == 'r' && t) {
            line(pairUndumped               ? "TRIAL release refused reason=undumped; d first\n"
                 : !dumping && r.route.release() ? "TRIAL record released; sessions and uncertainty retained\n"
                                                 : "TRIAL release refused\n");
        }
        // Its first whole line, at the UART's rate: the reply, or a dump's first line.
        for (unsigned i = 0; i < 400 && r.uart.output.find('\n', mark) == std::string::npos; ++i)
            turn();
        return r.uart.output.substr(mark);
    }
    // One console line, then turns until `until` is printed or `limit` turns have passed.
    // @return What was printed since the line; lastTurns counts the turns taken.
    std::string console(const std::string& text, const std::string& until, unsigned limit = 2000) {
        room();
        const size_t mark = r.uart.output.size();
        r.bench = holds();
        r.derive();
        E::ConsoleInput taken = E::ConsoleInput::COLLECTING;
        for (char c : text)
            CHECK((taken = r.console.take(uint8_t(c), r.uart.now, r.activity)) == E::ConsoleInput::COLLECTING);
        taken = r.console.take('\r', r.uart.now, r.activity);
        CHECK(taken != E::ConsoleInput::COLLECTING);
        if (taken == E::ConsoleInput::REFUSED)
            line(r.console.reply());
        lastTurns = 0;
        // Until the line holding `until` has arrived whole, at the UART's rate.
        const auto arrived = [&]() {
            const size_t at = r.uart.output.find(until, mark);
            return at != std::string::npos && r.uart.output.find('\n', at) != std::string::npos;
        };
        while (lastTurns < limit && !arrived()) {
            turn();
            ++lastTurns;
        }
        if (std::getenv("BENCH_CONSOLE_DEBUG"))
            std::fprintf(stderr, "[%s] %u turns:\n%s\n", text.c_str(), lastTurns, r.uart.output.substr(mark).c_str());
        return r.uart.output.substr(mark);
    }
};
// The host's paired arming lines: ARM_MOVE's fields and the other node's reviewed 6064 (Ta), or the velocity
// arming's fields for both nodes (Te).
std::string pairMove(uint32_t& trial, const ArmSpec& spec, long otherPosition) {
    std::string line = arm(1, trial, spec) + "," + std::to_string(otherPosition);
    line[0] = 'a';
    return line;
}
// Every operation ID the console printed, once each, for `release`.
std::vector<unsigned long> operations(const std::string& output) {
    std::vector<unsigned long> ids;
    for (size_t at = output.find("\"operation_id\":"); at != std::string::npos;
         at = output.find("\"operation_id\":", at + 1)) {
        const unsigned long id = std::strtoul(output.c_str() + at + 15, nullptr, 10);
        bool seen = !id;
        for (unsigned long known : ids)
            seen = seen || known == id;
        if (!seen)
            ids.push_back(id);
    }
    return ids;
}
void releaseAll(Image& m, unsigned& request) {
    for (unsigned long id : operations(m.r.uart.output)) {
        const std::string tag = "\"id\":" + std::to_string(++request) + ",";
        m.console("@" + std::to_string(request) + " release " + std::to_string(id), tag, 400);
    }
    CHECK(!m.r.console.holds());
}
// The order that failed on the bench (2026-11-16 HM1, 115200 baud): one axis armed, then its dump, which every
// console reply waits behind, then its move; the other axis's arming line then meets the image's guard,
// because the console retains the first move. Nothing of that line is parsed or sent.
void testSingleArmingGuard() {
    scenario = "single-arming-guard";
    std::unique_ptr<Rig> r(new Rig);
    r->open();
    for (auto& drive : r->driver.drives) {
        drive.statusBits = true;
        drive.travelReads = 100000;
    }
    uint32_t trial = 0;
    enabled(*r, trial, 1);
    enabled(*r, trial, 2);
    Image m(*r);
    r->uart.rate = 11; // 115200 baud 8N1: 11.52 bytes per 1 ms turn.
    unsigned request = 0;
    CHECK(has(m.console("@" + std::to_string(++request) + " host 0", "\"id\":1,"), "\"ok\":true"));
    ArmSpec spec;
    spec.rel = 0;
    spec.sp = r->driver.drives[0].position;
    CHECK(has(m.trial(arm(1, trial, spec)), "TRIAL admitted route=arm-move node=1"));
    for (unsigned i = 0; i < 5000 && !m.dumping; ++i)
        m.turn();
    CHECK(m.dumping && r->axis1.armedTrial == trial);
    // The console's reply is admitted at once but printed only after the whole dump.
    const std::string accel = m.console("@" + std::to_string(++request) + " accel 1000", "\"id\":2,", 30000);
    CHECK(has(accel, "\"ok\":true") && !m.dumping);
    CHECK(has(accel, "TRIAL_PLAN") && accel.rfind("TRIAL_") < accel.find("\"id\":2,") && m.lastTurns > 100);
    CHECK(has(m.key('r'), "TRIAL record released"));
    CHECK(has(m.console("@" + std::to_string(++request) + " decel 1000", "\"id\":3,"), "\"ok\":true"));
    CHECK(has(m.console("@" + std::to_string(++request) + " move absolute 800 steps native 30 configured", "\"id\":4,"),
              "\"result\":\"accepted\""));
    CHECK(r->one.moveActive() && r->console.holds());
    const size_t requests = r->driver.requests.size();
    spec.sp = r->driver.drives[1].position;
    CHECK(has(m.trial(arm(2, trial, spec)), "TRIAL_REFUSED reason=console; no work"));
    CHECK(!r->route.record() && r->axis2.armedTrial == 0);
    for (size_t i = requests; i < r->driver.requests.size(); ++i)
        CHECK(r->driver.requests[i].id != 0x602);
    CHECK(has(m.console("@" + std::to_string(++request) + " stop 1 normal", "\"id\":5,"), "\"result\":\"accepted\""));
    m.turns(3000);
    CHECK(!r->one.moveActive());
    releaseAll(m, request);
    wholeLines(r->uart.output);
}
// The correction: Ta prepares both axes in one record and prints one summary line instead of its dump, so
// both axes are armed, and both moves admitted and acknowledged, with no dump or arming line between them, at
// 115200 baud and within each arming's age cap. The stops are per axis and then the group, through the
// console as before. The record is dumped whole once the console's results are released.
void testPairedArming() {
    scenario = "pair-arming";
    std::unique_ptr<Rig> r(new Rig);
    r->open();
    for (auto& drive : r->driver.drives) {
        drive.statusBits = true;
        drive.travelReads = 100000; // This fake's drive keeps reporting travel until a stop word.
    }
    uint32_t trial = 0;
    enabled(*r, trial, 1);
    enabled(*r, trial, 2);
    Image m(*r);
    r->uart.rate = 11;
    unsigned request = 0;
    CHECK(has(m.console("@" + std::to_string(++request) + " host 0", "\"id\":1,"), "\"ok\":true"));
    ArmSpec spec;
    spec.rel = 0;
    spec.sp = r->driver.drives[0].position;
    const size_t requests = r->driver.requests.size();
    const uint8_t admissions = r->route.motionAdmissions();
    CHECK(has(m.trial(pairMove(trial, spec, r->driver.drives[1].position)), "TRIAL admitted route=arm-move-pair node=1"));
    for (unsigned i = 0; i < 5000 && !has(r->notices, "CONSOLE "); ++i)
        m.turn();
    CHECK(has(r->notices, ("CONSOLE armed pair nodes=1,2 trial=" + std::to_string(trial) + " route=arm-move-pair; ").c_str()));
    // Printed after the summary, which went first: both whole within a few dozen milliseconds at 115200 baud.
    unsigned printed = 0;
    while (printed < 100 && !has(r->uart.output, "nothing sent\n")) {
        m.turn();
        ++printed;
    }
    CHECK(printed < 100 && r->uart.output.find("TRIAL_PAIR id=") < r->uart.output.find("CONSOLE armed pair"));
    CHECK(r->downloadsFrom(requests) == 0 && r->route.motionAdmissions() == admissions); // Reads only.
    CHECK(has(r->uart.output, ("TRIAL_PAIR id=" + std::to_string(trial)).c_str()) && !has(r->uart.output, "TRIAL_PLAN"));
    CHECK(!m.dumping && m.pairUndumped && r->axis1.armedTrial == trial && r->axis2.armedTrial == trial);
    // Each axis's record of its own reads: node 1's were taken first, node 2's after them.
    CHECK(r->axis1.armedState.servicedUs && r->axis2.armedState.servicedUs > r->axis1.armedState.servicedUs);
    const uint64_t armedUs = r->uart.now;
    const char* lines[] = {"useaddr 1", "accel 1000", "decel 1000", "move absolute 800 steps native 30 configured",
                           "useaddr 2", "accel 1000", "decel 1000", "move absolute 1600 steps native 30 configured"};
    for (const char* text : lines) {
        const std::string tag = "\"id\":" + std::to_string(++request) + ",";
        const std::string out = m.console("@" + std::to_string(request) + " " + text, tag, 400);
        CHECK(has(out, "\"ok\":true") && m.lastTurns < 100 && !has(out, "TRIAL_"));
    }
    CHECK(r->one.moveActive() && r->two.moveActive() && r->route.motionAdmissions() == admissions + 2);
    CHECK(r->uart.now - armedUs < 9 * (uint64_t(E::ROUTE_QUEUE_US) + E::ROUTE_RESPONSE_US));
    // The full record waits for the console's results; `r` waits for `d`.
    CHECK(has(m.key('d'), "DUMP refused reason=console"));
    CHECK(has(m.key('r'), "TRIAL release refused reason=undumped; d first") && r->route.record());
    // One axis stopped by address while the UART takes nothing: the stop is admitted and sent at once, the
    // other axis keeps moving, and the reply follows when output resumes.
    size_t first = r->driver.requests.size();
    std::string tag = "\"id\":" + std::to_string(++request) + ",";
    r->uart.blocked = true;
    CHECK(!has(m.console("@" + std::to_string(request) + " stop 2 normal", tag, 200), "\"id\""));
    bool halted = false;
    for (size_t i = first; i < r->driver.requests.size(); ++i)
        halted = halted || (r->driver.requests[i].id == 0x602 &&
                            !std::memcmp(r->driver.requests[i].data, HALT_WORD, 8));
    CHECK(halted && r->driver.drives[1].lastWord == 0x010f);
    m.turns(1300);
    CHECK(r->one.moveActive() && !r->two.moveActive());
    r->uart.blocked = false;
    m.turns(400);
    CHECK(has(r->uart.output, (tag + "\"command\":\"stop\",\"ok\":true,\"status\":\"OK\",\"result\":\"accepted\"").c_str()));
    for (size_t i = first; i < r->driver.requests.size(); ++i)
        CHECK(!(r->driver.requests[i].id == 0x601 && r->driver.requests[i].data[0] == 0x2b &&
                r->driver.requests[i].data[1] == 0x40 && r->driver.requests[i].data[2] == 0x60));
    // The group stop reaches the other axis.
    first = r->driver.requests.size();
    tag = "\"id\":" + std::to_string(++request) + ",";
    const size_t group = r->uart.output.size();
    CHECK(has(m.console("@" + std::to_string(request) + " stop all fast", tag, 400), "\"axis_count\":2") &&
          m.lastTurns < 100);
    m.turns(3000);
    CHECK(!r->one.moveActive() && !r->two.moveActive());
    CHECK(count(r->uart.output.substr(group), "\"type\":\"group_stop_axis\"") == 2);
    bool quick = false;
    for (size_t i = first; i < r->driver.requests.size(); ++i)
        quick = quick || sameFrame(r->driver.requests[i], 1, QUICK_WORD);
    CHECK(quick);
    releaseAll(m, request);
    // Exported after the motion: the plan, both checks, then `r`.
    const size_t mark = r->uart.output.size();
    CHECK(!has(m.key('d'), "DUMP"));
    for (unsigned i = 0; i < 60000 && m.dumping; ++i)
        m.turn();
    m.turns(100); // The ring's last queued lines, at the UART's rate.
    const std::string dump = r->uart.output.substr(mark);
    if (const char* path = std::getenv("BENCH_CONSOLE_PAIR_DUMP")) {
        // For scripts/s2_evidence.py: the packer counts the summary lines against the header's summaries=.
        if (FILE* file = std::fopen(path, "wb")) {
            std::fwrite(dump.data(), 1, dump.size(), file);
            std::fclose(file);
        }
    }
    CHECK(!m.dumping && has(dump, "TRIAL_PLAN route=arm-move-pair node=1 pair=2") &&
          has(dump, "TRIAL_PAIR_CHECK node=1 passed=1") && has(dump, "END trial "));
    // Node 2's reads name their node; node 1's are only its pair check.
    for (const char* tag : {"\nTRIAL_CHECK passed=1 ", "\nTRIAL_LAST passed=1 "}) {
        const size_t at = dump.find(tag);
        CHECK(at != std::string::npos);
        const std::string line = dump.substr(at + 1, dump.find('\n', at + 1) - at - 1);
        CHECK(line.size() > 7 && line.compare(line.size() - 7, 7, " node=2") == 0);
    }
    CHECK(!has(dump, "TRIAL_FIRST"));
    CHECK(has(m.key('r'), "TRIAL record released") && !r->route.record());
    wholeLines(r->uart.output);
}
// HM3's order (2026-11-16 bench, image b7033c12): a group HALT during both moves, its result released, then a
// group quick stop 2.3 s later. The quick stop is sequenced on each HALT's own last cycle (AxisSession picks the
// freshest report, 2026-11-17), never the unconfirmed 000B branch, and both checked stops leave no move fence.
void testGroupQuickStopAfterGroupHalt() {
    scenario = "group-quick-after-halt";
    std::unique_ptr<Rig> r(new Rig);
    r->open();
    for (auto& drive : r->driver.drives) {
        drive.statusBits = true;
        drive.travelReads = 100000;
    }
    uint32_t trial = 0;
    enabled(*r, trial, 1);
    enabled(*r, trial, 2);
    Image m(*r);
    unsigned request = 0;
    CHECK(has(m.console("@" + std::to_string(++request) + " host 0", "\"id\":1,"), "\"ok\":true"));
    ArmSpec spec;
    spec.rel = 0;
    spec.sp = r->driver.drives[0].position;
    CHECK(has(m.trial(pairMove(trial, spec, r->driver.drives[1].position)), "TRIAL admitted route=arm-move-pair"));
    for (unsigned i = 0; i < 5000 && !has(r->notices, "CONSOLE "); ++i)
        m.turn();
    CHECK(has(r->notices, "CONSOLE armed pair nodes=1,2"));
    const char* lines[] = {"useaddr 1", "accel 1000", "decel 1000", "move absolute 800 steps native 30 configured",
                           "useaddr 2", "accel 1000", "decel 1000", "move absolute 1600 steps native 30 configured"};
    for (const char* text : lines) {
        const std::string tag = "\"id\":" + std::to_string(++request) + ",";
        CHECK(has(m.console("@" + std::to_string(request) + " " + text, tag, 400), "\"ok\":true"));
    }
    CHECK(r->one.moveActive() && r->two.moveActive());
    // As on the bench (3 s in): both moves have a complete observation cycle before the group HALT.
    for (unsigned i = 0; i < 5000 && (r->one.moveAction().observations < 1 || r->two.moveAction().observations < 1); ++i)
        m.turn();
    CHECK(r->one.moveAction().observations >= 1 && r->two.moveAction().observations >= 1);
    std::string tag = "\"id\":" + std::to_string(++request) + ",";
    CHECK(has(m.console("@" + std::to_string(request) + " stop all normal", tag, 400), "\"axis_count\":2"));
    m.turns(2000);
    CHECK(!r->one.moveActive() && !r->two.moveActive());
    CHECK(r->one.stopAction(L::StopKind::HALT).state == M::ActionState::SUCCEEDED &&
          r->two.stopAction(L::StopKind::HALT).state == M::ActionState::SUCCEEDED);
    CHECK(!r->one.moveFenced() && !r->two.moveFenced()); // Checked stops leave no fence.
    const unsigned long groupHalt = operations(r->uart.output).back();
    tag = "\"id\":" + std::to_string(++request) + ",";
    CHECK(has(m.console("@" + std::to_string(request) + " release " + std::to_string(groupHalt), tag, 400),
              "\"ok\":true"));
    // Just past the stop age cap of each move's last cycle (BenchConsole::nameStop: seven fields at the route
    // budgets) and still inside it for each HALT's last cycle, as on the bench: only the HALT's report is fresh.
    const uint64_t age = 7 * (uint64_t(E::ROUTE_QUEUE_US) + E::ROUTE_RESPONSE_US);
    uint64_t moveLast = 0, haltLast = UINT64_MAX;
    for (E::AxisSession* s : {&r->one, &r->two}) {
        moveLast = std::max(moveLast, s->moveObservation().servicedUs);
        haltLast = std::min(haltLast, s->stopAction(L::StopKind::HALT).read.servicedUs);
    }
    while (r->uart.now <= moveLast + age + 2000)
        m.turn();
    // The console admits the stop when the line is fed, at this time: each HALT's report is still fresh then.
    CHECK(haltLast > moveLast && r->uart.now < haltLast + age);
    const size_t first = r->driver.requests.size(), mark = r->uart.output.size();
    tag = "\"id\":" + std::to_string(++request) + ",";
    CHECK(has(m.console("@" + std::to_string(request) + " stop all fast", tag, 400), "\"axis_count\":2"));
    m.turns(3000);
    const std::string group = r->uart.output.substr(mark);
    CHECK(count(group, "\"type\":\"group_stop_axis\"") == 2 && !has(group, "STOP_SENT_UNCONFIRMED"));
    for (unsigned node = 1; node <= 2; ++node) {
        E::AxisSession& s = node == 1 ? r->one : r->two;
        CHECK(s.stopAction(L::StopKind::QUICK_STOP).predecessorWord == 0x010f &&
              s.bestEffort().outcome == M::ActionOutcome::NONE);
        unsigned quick = 0;
        for (size_t i = first; i < r->driver.requests.size(); ++i)
            quick += r->driver.requests[i].id == 0x600 + node && !std::memcmp(r->driver.requests[i].data, QUICK_WORD, 8);
        CHECK(quick == 1);
    }
    wholeLines(r->uart.output);
}
// The pair's own refusals: grammar, the other node's readiness, and a retained operation on either axis,
// which leaves both axes as they were. Nothing is sent for any of them.
void testPairRefusals() {
    scenario = "pair-refusals";
    uint32_t trial = 0;
    ArmSpec spec;
    spec.rel = 0;
    E::RoutePlan p;
    std::string line = pairMove(trial, spec, 7);
    CHECK(E::parseRouteCommand(line.data(), line.size(), p) && p.kind == E::RouteKind::ARM_MOVE_PAIR && p.node == 1 &&
          p.pairStartPosition == 7 && p.relativeBase == 0 && E::isArming(p.kind) && E::isArmingPair(p.kind));
    CHECK(std::strcmp(E::routeName(E::RouteKind::ARM_MOVE_PAIR), "arm-move-pair") == 0 &&
          std::strcmp(E::routeName(E::RouteKind::ARM_VELOCITY_PAIR), "arm-velocity-pair") == 0);
    ArmSpec relative;
    for (const std::string& bad : {pairMove(trial, relative, 7), pairMove(trial, spec, 7) + ",1",
                                   arm(1, trial, spec).replace(0, 1, "a")}) {
        E::RoutePlan untouched;
        CHECK(!E::parseRouteCommand(bad.data(), bad.size(), untouched) && untouched.kind == E::RouteKind::NONE);
    }
    std::string velocity = velocityArming(trial);
    velocity[0] = 'e';
    CHECK(E::parseRouteCommand(velocity.data(), velocity.size(), p) && p.kind == E::RouteKind::ARM_VELOCITY_PAIR);
    CHECK(!E::parseRouteCommand((velocity + ",1").data(), velocity.size() + 2, p));
    // Node 2 not enabled: the pair is refused before any read.
    std::unique_ptr<Rig> r(new Rig);
    r->open();
    for (auto& drive : r->driver.drives)
        drive.statusBits = true;
    enabled(*r, trial, 1);
    size_t first = r->driver.requests.size();
    const char* refusal = r->start(pairMove(trial, spec, 0));
    CHECK(refusal && (std::strcmp(refusal, "pair-eligibility") == 0 || std::strcmp(refusal, "pair-control") == 0));
    CHECK(r->driver.requests.size() == first && !r->route.record());
    // A retained console move result on node 1 refuses both: node 2 stays unarmed. main.cpp would refuse the
    // T line itself (testSingleArmingGuard); the rig starts the route directly, as testRefusedArming does, so
    // that the console meets the pair.
    enabled(*r, trial, 2);
    CHECK(has(r->command("@1 host 0", 100), "\"ok\":true"));
    armed(*r, trial);
    CHECK(has(r->command("@2 accel 1000", 100), "\"ok\":true") && has(r->command("@3 decel 1000", 100), "\"ok\":true"));
    CHECK(has(r->command("@4 move relative 16 steps native 1 configured", 8000), "\"type\":\"terminal\",\"id\":4"));
    CHECK(r->console.holds());
    r->release();
    const uint32_t before = r->axis1.armedTrial;
    r->notices.clear();
    spec.sp = r->driver.drives[0].position;
    CHECK(r->run(pairMove(trial, spec, r->driver.drives[1].position)).failure == E::RouteFailure::NONE);
    r->turns(2);
    CHECK(has(r->notices, ("CONSOLE arm refused node=1 trial=" + std::to_string(trial) +
                           " reason=console_axis_operation_retained; pair: this trial armed neither node")
                              .c_str()));
    CHECK(r->axis1.armedTrial == before && r->axis2.armedTrial == 0);
    first = r->driver.requests.size();
    CHECK(has(r->command("@6 useaddr 2", 50), "\"ok\":true"));
    CHECK(has(r->command("@7 move absolute 16 steps native 1 configured", 100), "\"ok\":false"));
    CHECK(r->downloadsFrom(first) == 0);
    wholeLines(r->uart.output);
}
// Te: both axes' velocity armings in one record, then two finite console velocity runs, each stopping at its
// own due time. Paired preparation claims nothing about simultaneity: each run is admitted on its own line.
void testPairedVelocity() {
    scenario = "pair-velocity";
    std::unique_ptr<Rig> r(new Rig);
    uint32_t trial = 0;
    velocityRig(*r, trial);
    Drive& other = r->driver.drives[1];
    other.monitor = 0x0011;
    other.status = 0x8050;
    other.mode = 3;
    other.releaseWords = other.spins = true;
    CHECK(r->run("A2," + id(trial)).failure == E::RouteFailure::NONE);
    r->release();
    r->heartbeat(2);
    Image m(*r);
    r->uart.rate = 11;
    unsigned request = 10;
    std::string line = velocityArming(trial);
    line[0] = 'e';
    const size_t requests = r->driver.requests.size();
    CHECK(has(m.trial(line), "TRIAL admitted route=arm-velocity-pair node=1"));
    for (unsigned i = 0; i < 5000 && !has(r->notices, "CONSOLE "); ++i)
        m.turn();
    CHECK(has(r->notices, "CONSOLE armed pair nodes=1,2") && r->downloadsFrom(requests) == 0);
    CHECK(r->driver.requests.size() - requests == 36); // Identity and two full reads per node, as ARM_VELOCITY.
    for (unsigned node = 1; node <= 2; ++node) {
        std::string tag = "\"id\":" + std::to_string(++request) + ",";
        CHECK(has(m.console("@" + std::to_string(request) + " useaddr " + std::to_string(node), tag, 400), "\"ok\":true"));
        tag = "\"id\":" + std::to_string(++request) + ",";
        CHECK(has(m.console("@" + std::to_string(request) + " velocity 6 rpm native 12000 configured normal", tag, 400),
                  "\"result\":\"accepted\"") &&
              m.lastTurns < 100);
    }
    CHECK(r->one.velocityActive() && r->two.velocityActive());
    for (unsigned i = 0; i < 30000 && (r->one.velocityActive() || r->two.velocityActive() || r->one.stopActive() ||
                                       r->two.stopActive());
         ++i)
        m.turn();
    m.turns(200);
    CHECK(!r->one.velocityNeedsStop() && !r->two.velocityNeedsStop());
    CHECK(r->driver.drives[0].velocity == 0 && r->driver.drives[1].velocity == 0);
    releaseAll(m, request);
    CHECK(!has(m.key('d'), "DUMP"));
    for (unsigned i = 0; i < 60000 && m.dumping; ++i)
        m.turn();
    m.turns(100); // The ring's last queued lines, at the UART's rate.
    CHECK(has(r->uart.output, "TRIAL_PLAN route=arm-velocity-pair node=1 pair=2"));
    CHECK(has(m.key('r'), "TRIAL record released"));
    wholeLines(r->uart.output);
}
} // namespace

int main() {
    testTwoArmedAxes();
    testGate();
    testInputAndOutput();
    testInputLoss();
    testConsoleReads();
    testExclusion();
    testArmingParse();
    testArmingAndMove();
    testConsoleStop();
    testScaleAndMoveby();
    testVelocityArming();
    testExplicitVelocityChangeArming();
    testExplicitMoveChangeArming();
    testVelocityCancelRefused();
    testVelocityExplicitStops();
    testRefusedArming();
    testBudgetsPerNode();
    testFaultHoldsBack();
    testSingleArmingGuard();
    testPairedArming();
    testGroupQuickStopAfterGroupHalt();
    testPairRefusals();
    testPairedVelocity();
    std::printf("bench_console: %u checks; BenchConsole %u, ProbeDevice %u, ProbeConsole %u, BenchAxis %u bytes\n",
                checks, unsigned(sizeof(E::BenchConsole)), unsigned(sizeof(E::ProbeDevice)),
                unsigned(sizeof(E::ProbeConsole)), unsigned(sizeof(E::BenchAxis)));
    return 0;
}
