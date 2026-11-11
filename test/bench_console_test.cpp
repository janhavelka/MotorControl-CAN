// SPDX-License-Identifier: MIT
// The shared console on the S2 bench image (BenchConsole) through the real firmware assembly, owner,
// SDO engine, application, BenchRoute, AxisSession and ProbeMotion, against the bench route suite's
// synthetic one-frame driver and two synthetic drives (support/BenchDrive.h). Every value, word response
// and movement here is manufactured by that fake: nothing shows what a CL86-C does.
#include "BenchConsole.h"
#include "BenchRoute.h"
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
        : firmware(services, driver.callbacks(), uart.callbacks()), one(firmware.owner()), two(firmware.owner()),
          route(firmware, services, E::RouteCallbacks()), console(firmware, services, storage()),
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
    // main.cpp's activity for the bench state this rig can hold: a route record, except a terminal settled
    // arming record, which holds only its dump.
    void derive() {
        const E::RouteRecord* r = route.record();
        const bool arming = r && r->terminal && !r->unsettled && E::isArming(r->plan.kind);
        activity.wire = r && !arming;
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
    r->activity.holdOutput = true;
    const size_t mark = r->uart.output.size();
    r->send("@10 caps");
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
    r->activity.holdOutput = false;
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
          has(r->notices, "route=arm-move relative=1 witness=1 scale=0 origin=0; prerequisites named, nothing sent"));
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
    // A later route replaces the arming's reads: refused here before the console sees the line.
    r->release();
    CHECK(r->run("A1," + id(trial)).failure == E::RouteFailure::NONE);
    E::RouteArming gone;
    CHECK(!r->route.armed(gone));
    r->release();
    CHECK(r->send("@8 move relative 16 steps native 1 configured") == E::ConsoleInput::REFUSED);
    CHECK(has(r->console.reply(), "arming_reads_replaced_arm_again"));
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
// The velocity arming from this fake's released mode 3 standstill, then the console's finite velocity run with
// its own stop at the due time, counted as a VELOCITY route and its stop.
void testVelocityArming() {
    scenario = "velocity";
    std::unique_ptr<Rig> r(new Rig);
    r->open();
    Drive& d = r->driver.drives[0];
    d.monitor = 0x0011; // As the bench route suite's released mode 3 drive: released, then spins on 0007-000F.
    d.status = 0x8050;
    d.mode = 3;
    d.releaseWords = true;
    d.spins = true;
    uint32_t trial = 0;
    CHECK(r->run("A1," + id(trial)).failure == E::RouteFailure::NONE); // Service qualification, as before VELOCITY.
    r->release();
    r->heartbeat(1);
    CHECK(has(r->command("@1 host 0", 100), "\"ok\":true"));
    const uint8_t before = r->route.motionAdmissions();
    const std::string line = "B1," + id(trial) + ",1,6,200,200,5000,351,2,1," + std::to_string(DEVICE_TYPE) + "," +
                             std::to_string(HARDWARE) + "," + std::to_string(SOFTWARE) + "," + std::to_string(VENDOR) +
                             "," + std::to_string(PRODUCT) + "," + std::to_string(REVISION) + "," +
                             std::to_string(MODEL) + "," + std::to_string(FIRMWARE) + ",1500,1500,500";
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
} // namespace

int main() {
    testGate();
    testInputAndOutput();
    testConsoleReads();
    testExclusion();
    testArmingParse();
    testArmingAndMove();
    testConsoleStop();
    testScaleAndMoveby();
    testVelocityArming();
    testFaultHoldsBack();
    std::printf("bench_console: %u checks; BenchConsole %u, ProbeDevice %u, ProbeConsole %u, BenchAxis %u bytes\n",
                checks, unsigned(sizeof(E::BenchConsole)), unsigned(sizeof(E::ProbeDevice)),
                unsigned(sizeof(E::ProbeConsole)), unsigned(sizeof(E::BenchAxis)));
    return 0;
}
