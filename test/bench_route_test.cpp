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

M::Frame frame(uint32_t id, uint8_t count) {
    M::Frame value;
    value.id = id;
    value.dlc = value.payloadLength = count;
    return value;
}
uint16_t indexOf(const M::Frame& f) { return uint16_t(f.data[1] | uint16_t(f.data[2]) << 8); }
struct Console {
    uint64_t now = 1000000;
    std::string output;
    static bool clock(void* context, uint64_t& out) {
        out = static_cast<Console*>(context)->now;
        return true;
    }
    static bool write(void* context, uint8_t byte) {
        static_cast<Console*>(context)->output.push_back(char(byte));
        return true;
    }
    static bool read(void*, uint8_t&) { return false; }
    E::ProbeCallbacks callbacks() {
        E::ProbeCallbacks value;
        value.context = this;
        value.nowUs = clock;
        value.readByte = read;
        value.writeByte = write;
        return value;
    }
};
// One synthetic drive. Widths follow the EDS declarations the typed reads expect.
struct Drive {
    uint8_t errorRegister = 0;
    uint16_t errorCode = 0, monitor = 1, status = 0x8037, lastWord = 0;
    uint8_t mode = 0;
    int32_t position = 0, velocity = 0, positionTarget = 5000, velocityTarget = 120;
    uint32_t rpm = 7, rampUp = 8, rampDown = 9;
    unsigned moveReads = 0;   // 2100 reads that still report running after a trigger.
    uint16_t silentIndex = 0; // No reply to this object (any direction).
    uint16_t silentWord = 0;  // With 6040: only this word goes unanswered.
    uint16_t abortIndex = 0;  // Abort 08000020 for this object.
    uint16_t alarmAfterMode = 0; // 2100 bits set once 6060 has been written.
    bool followWords = true;
    unsigned downloads = 0;
    unsigned width(uint16_t index, uint8_t sub) const {
        switch (index) {
        case 0x1001: case 0x6061:
            return 1;
        case 0x60fe:
            return sub ? 4 : 1;
        case 0x6064: case 0x606c: case 0x607a: case 0x60ff: case 0x6081: case 0x6083: case 0x6084:
            return 4;
        default:
            return 2;
        }
    }
    uint32_t value(uint16_t index, uint8_t sub) {
        switch (index) {
        case 0x1001: return errorRegister;
        case 0x603f: return errorCode;
        case 0x2100:
            if (moveReads) {
                if (!--moveReads)
                    position = positionTarget;
                return uint16_t(monitor | L::MONITOR_RUNNING) & ~L::MONITOR_POSITION_COMPLETE;
            }
            return monitor;
        case 0x2101: return 0xa580;
        case 0x2102: return 0xf008;
        case 0x6041: return status;
        case 0x6061: return mode;
        case 0x6064: return uint32_t(position);
        case 0x606c: return uint32_t(velocity);
        case 0x607a: return uint32_t(positionTarget);
        case 0x60ff: return uint32_t(velocityTarget);
        case 0x6081: return rpm;
        case 0x6083: return rampUp;
        case 0x6084: return rampDown;
        case 0x1017: return 0;
        case 0x2400: return sub == 0 ? 0x0f : sub >= 2 && sub <= 4 ? sub - 1u : 0x100u + sub;
        case 0x60fe: return sub == 0 ? 2 : 0;
        default: return index >= 0x2401 && index <= 0x2520 ? uint32_t(index & 0xff) : 0;
        }
    }
    // Returns false for no reply.
    bool answer(const M::Frame& request, M::Frame& reply) {
        const uint16_t index = indexOf(request);
        const uint8_t sub = request.data[3];
        const bool upload = request.data[0] == 0x40;
        const uint16_t word = uint16_t(request.data[4] | uint16_t(request.data[5]) << 8);
        if (index == silentIndex && (index != 0x6040 || !silentWord || word == silentWord))
            return false;
        std::memcpy(reply.data + 1, request.data + 1, 3);
        if (index == abortIndex) {
            reply.data[0] = 0x80;
            reply.data[4] = 0x20;
            reply.data[7] = 0x08;
            return true;
        }
        if (!upload) {
            ++downloads;
            const int32_t signedValue = int32_t(uint32_t(request.data[4]) | uint32_t(request.data[5]) << 8 |
                                                uint32_t(request.data[6]) << 16 | uint32_t(request.data[7]) << 24);
            if (index == 0x6060) {
                mode = request.data[4];
                monitor = uint16_t(monitor | alarmAfterMode);
            } else if (index == 0x6040) {
                lastWord = word;
                const uint16_t low = word == 6 ? 1 : word == 7 ? 3 : word == 15 ? 7 : uint16_t(status & 7u);
                if (followWords)
                    status = uint16_t((status & ~L::STATUS_LOW_BITS) | low);
                if (word == 0x001f)
                    moveReads = 3;
                if (word == 0x010f || word == 0x000b)
                    moveReads = 0;
            } else if (index == 0x607a) {
                positionTarget = signedValue;
            } else if (index == 0x6081) {
                rpm = uint32_t(signedValue);
            } else if (index == 0x6083) {
                rampUp = uint32_t(signedValue);
            } else if (index == 0x6084) {
                rampDown = uint32_t(signedValue);
            }
            reply.data[0] = 0x60;
            return true;
        }
        const unsigned w = width(index, sub);
        const uint32_t v = value(index, sub);
        reply.data[0] = uint8_t(0x43u | (4u - w) << 2);
        for (unsigned i = 0; i < w; ++i)
            reply.data[4 + i] = uint8_t(v >> (8 * i));
        return true;
    }
};
// One frame at a time, as the real shim presents the SDK: TX report, then the reply.
struct Driver {
    Console* console = nullptr;
    E::TwaiEvents pending;
    O::ReceivedFrame incoming[16];
    std::vector<M::Frame> requests;
    Drive drives[2];
    unsigned head = 0, count = 0;
    uint32_t epoch = 0, serial = 0;
    bool live = false, active = false;
    M::Frame current;
    static bool start(void* context, const E::TwaiConfig&, uint32_t epoch) {
        Driver& d = *static_cast<Driver*>(context);
        d.epoch = epoch;
        d.live = true;
        return true;
    }
    static bool stop(void* context) {
        static_cast<Driver*>(context)->live = false;
        return true;
    }
    static E::TwaiSubmit submit(void* context, const M::Frame& value, uint32_t serial) {
        Driver& d = *static_cast<Driver*>(context);
        CHECK(d.live && !d.active);
        if (d.count)
            return E::TwaiSubmit::DEFERRED;
        CHECK(value.id == 0x601 || value.id == 0x602); // Only SDO requests: no NMT, PDO or other.
        CHECK(value.dlc == 8 && !value.extended && !value.rtr);
        d.requests.push_back(value);
        d.current = value;
        d.active = true;
        d.serial = serial;
        return E::TwaiSubmit::SENT;
    }
    static E::TwaiEvents events(void* context) {
        Driver& d = *static_cast<Driver*>(context);
        E::TwaiEvents copy = d.pending;
        copy.rxPending = d.count != 0;
        d.pending = E::TwaiEvents();
        return copy;
    }
    static bool receive(void* context, O::ReceivedFrame& output, uint64_t cutoffUs) {
        Driver& d = *static_cast<Driver*>(context);
        if (!d.count || d.incoming[d.head].receivedLatestUs > cutoffUs)
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
        next.receiveBoundsKnown = true;
        next.receivedEarliestUs = console->now - 400;
        next.receivedLatestUs = console->now - 100;
    }
    void deliver() {
        if (!active)
            return;
        pending.txDone = true;
        pending.transmitted = true;
        pending.serial = serial;
        active = false;
        if (current.data[0] == 0x80)
            return; // A client abort is local cleanup.
        const unsigned node = unsigned(current.id - 0x600);
        M::Frame reply = frame(0x580 + node, 8);
        if (drives[node - 1].answer(current, reply))
            push(reply);
    }
};
struct Rig {
    C::SdoClient services;
    Console console;
    Driver driver;
    E::FirmwareProbe firmware;
    E::AxisSession one, two;
    E::BenchRoute route;
    unsigned samples = 0;
    static void sample(void* context, E::RouteResources& output) {
        Rig& r = *static_cast<Rig*>(context);
        ++r.samples;
        output.stackFree = 9000 - r.samples;
        output.heapFree = 60000;
        output.heapMinimum = 55000;
    }
    static E::RouteCallbacks hooks(Rig* rig) {
        E::RouteCallbacks value;
        value.context = rig;
        value.sample = sample;
        return value;
    }
    Rig()
        : firmware(services, driver.callbacks(), console.callbacks()), one(firmware.owner()),
          two(firmware.owner()), route(firmware, services, hooks(this)) {
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
        CHECK(!route.attach(1, two));
    }
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
    unsigned downloadsFrom(size_t first) const {
        unsigned count = 0;
        for (size_t i = first; i < driver.requests.size(); ++i)
            count += driver.requests[i].data[0] != 0x40 && driver.requests[i].data[0] != 0x80;
        return count;
    }
};
bool upload(const M::Frame& f, unsigned node, uint16_t index, uint8_t sub = 0) {
    const uint8_t expected[8] = {0x40, uint8_t(index), uint8_t(index >> 8), sub, 0, 0, 0, 0};
    return f.id == 0x600 + node && !std::memcmp(f.data, expected, 8);
}
bool download(const M::Frame& f, unsigned node, const uint8_t (&expected)[8]) {
    return f.id == 0x600 + node && !std::memcmp(f.data, expected, 8);
}
struct Address {
    uint16_t index;
    uint8_t sub;
};
const Address IMAGE[] = {{0x2100, 0}, {0x2101, 0}, {0x2102, 0}, {0x6041, 0}, {0x6061, 0}, {0x6064, 0}, {0x606c, 0}};
const Address FULL[] = {{0x1001, 0}, {0x603f, 0}, {0x2100, 0}, {0x2101, 0}, {0x2102, 0},
                        {0x6041, 0}, {0x6061, 0}, {0x6064, 0}, {0x606c, 0}};
// Compare the next requests with a fixed list, advancing the cursor.
void expectUploads(const Rig& r, size_t& at, unsigned node, const Address* list, unsigned count) {
    for (unsigned i = 0; i < count; ++i, ++at) {
        CHECK(at < r.driver.requests.size());
        CHECK(upload(r.driver.requests[at], node, list[i].index, list[i].sub));
    }
}
bool has(const std::string& text, const char* part) { return text.find(part) != std::string::npos; }

void testParse() {
    scenario = "parse";
    E::RoutePlan p;
    CHECK(E::parseRouteCommand("A1,5", 4, p) && p.kind == E::RouteKind::READ_ACTIONS && p.node == 1 && p.trial == 5);
    CHECK(E::parseRouteCommand("I2,4294967295", 13, p) && p.kind == E::RouteKind::READ_IO && p.trial == UINT32_MAX);
    CHECK(E::parseRouteCommand("U1,9", 4, p) && p.kind == E::RouteKind::READ_TUNING);
    const std::string mode = "M1,6,1,3,250";
    CHECK(E::parseRouteCommand(mode.data(), mode.size(), p) && p.attended && p.cycles == 3 && p.intervalMs == 250);
    const std::string enable = "E2,7,1,1,2,32823,-5000,120,10000,800";
    CHECK(E::parseRouteCommand(enable.data(), enable.size(), p));
    CHECK(p.kind == E::RouteKind::ENABLE && p.node == 2 && p.shaft == 2 && p.reviewedStatus == 0x8037 &&
          p.reviewedPositionTarget == -5000 && p.reviewedVelocityTarget == 120 && p.deadlineMs == 10000 &&
          p.intervalMs == 800 && p.start == 1);
    const std::string stop = "S1,8,1,2,2000,3000,100";
    CHECK(E::parseRouteCommand(stop.data(), stop.size(), p) && p.stopKind == 2 && p.bestEffortMs == 3000);
    const std::string position = "P1,9,1,-3,16,1,1000,1000,8,250,1,4,2000,0,100";
    CHECK(E::parseRouteCommand(position.data(), position.size(), p));
    CHECK(p.startPosition == -3 && p.target == 16 && p.rpm == 1 && p.rampUpMs == 1000 && p.cycles == 8 &&
          p.stopKind == 1 && p.stopAfter == 4 && p.stopDeadlineMs == 2000 && p.stopIntervalMs == 100);
    const std::string quiet = "P1,9,1,0,16,1,1000,1000,8,250,0,0,0,0,0";
    CHECK(E::parseRouteCommand(quiet.data(), quiet.size(), p) && !p.stopKind);
    const char* const bad[] = {"", "A", "A1", "A1,", "A0,5", "A3,5", "A1,0", "A1,5,", "A1,5,1", "X1,5",
                               "a1,5", "A1, 5", "A+1,5", "A1,-5", "A1,99999999999", "A1,4294967296",
                               "M1,6,1,0,250", "M1,6,1,11,250", "M1,6,1,3,99", "M1,6,2,3,250", "M1,6,1,3",
                               "E1,7,1,0,1,0,0,0,10000,800", "E1,7,1,1,0,0,0,0,10000,800",
                               "E1,7,1,1,3,0,0,0,10000,800", "E1,7,1,1,1,65536,0,0,10000,800",
                               "E1,7,1,1,1,0,0,0,55001,800", "E1,7,1,1,1,0,0,0,10000,9",
                               "E1,7,1,1,1,0,2147483648,0,10000,800", "E1,7,1,1,1,-1,0,0,10000,800",
                               "S1,8,1,0,2000,0,100", "S1,8,1,3,2000,0,100", "S1,8,1,1,2000,1999,100",
                               "S1,8,1,1,0,0,100", "P1,9,1,0,1000001,1,1000,1000,8,250,0,0,0,0,0",
                               "P1,9,1,0,16,3001,1000,1000,8,250,0,0,0,0,0",
                               "P1,9,1,0,16,1,2001,1000,8,250,0,0,0,0,0",
                               "P1,9,1,0,16,1,1000,1000,17,250,0,0,0,0,0",
                               "P1,9,1,0,16,1,1000,1000,8,250,1,0,2000,0,100",
                               "P1,9,1,0,16,1,1000,1000,8,250,1,9,2000,0,100",
                               "P1,9,1,0,16,1,1000,1000,8,250,1,4,0,0,100",
                               "P1,9,1,0,16,1,1000,1000,8,250,1,4,2000,1000,100",
                               "P1,9,1,0,16,1,1000,1000,8,250,0,4,0,0,0",
                               "P1,9,1,0,16,1,1000,1000,8,250,0,0,0,0,100"};
    for (const char* text : bad) {
        E::RoutePlan untouched;
        if (E::parseRouteCommand(text, std::strlen(text), untouched))
            std::fprintf(stderr, "accepted: %s\n", text);
        CHECK(!E::parseRouteCommand(text, std::strlen(text), untouched));
        CHECK(untouched.kind == E::RouteKind::NONE && untouched.trial == 0);
    }
    CHECK(!E::parseRouteCommand(nullptr, 4, p));
    CHECK(std::strcmp(E::routeName(E::RouteKind::POSITION), "position") == 0);
}
// Nothing at construction or service, and refusals change nothing.
void testPassiveAndRefusals() {
    scenario = "passive";
    std::unique_ptr<Rig> r(new Rig);
    r->turns(200);
    CHECK(r->driver.requests.empty() && r->console.output.empty() && !r->route.record());
    CHECK(std::strcmp(r->start("M1,1,0,3,250"), "unattended") == 0);
    CHECK(std::strcmp(r->start("E1,1,0,1,1,32823,5000,120,10000,800"), "unattended") == 0);
    CHECK(std::strcmp(r->start("S1,1,0,1,2000,0,100"), "unattended") == 0);
    CHECK(std::strcmp(r->start("P1,1,1,0,16,1,1000,1000,8,250,0,0,0,0,0"), "control") == 0);
    // A deadline whose polls would not fit the record is refused before anything is sent.
    CHECK(std::strcmp(r->start("E1,1,1,1,1,32823,5000,120,55000,100"), "capacity") == 0);
    CHECK(std::strcmp(r->start("S1,1,1,1,55000,0,10"), "capacity") == 0);
    CHECK(std::strcmp(r->start("P1,1,1,0,16,1,1000,1000,16,500,0,0,0,0,0"), "capacity") == 0);
    CHECK(std::strcmp(r->start("P1,1,1,0,16,1,1000,1000,12,5000,0,0,0,0,0"), "duration") == 0);
    r->turns(50);
    CHECK(r->driver.requests.empty() && !r->route.record());
    CHECK(r->route.motionAdmissions() == 0);
    CHECK(std::strcmp(r->route.abort(r->console.now), "none") == 0);
    CHECK(!r->route.release());
}
// Packet 15 P1: the fixed read set, granted where unqualified, and no write.
void testActionsReads() {
    scenario = "actions";
    std::unique_ptr<Rig> r(new Rig);
    const E::RouteRecord& rec = r->run("A1,5");
    CHECK(rec.failure == E::RouteFailure::NONE && !rec.evidenceLost && rec.terminalCount == 22);
    size_t at = 0;
    expectUploads(*r, at, 1, FULL, 9);
    const Address rest[] = {{0x6081, 0}, {0x6083, 0}, {0x6084, 0}, {0x1017, 0}, {0x607a, 0}, {0x60ff, 0}};
    expectUploads(*r, at, 1, rest, 6);
    expectUploads(*r, at, 1, IMAGE, 7);
    CHECK(at == r->driver.requests.size() && r->downloadsFrom(0) == 0);
    for (unsigned i = 0; i < rec.terminalCount; ++i)
        CHECK(rec.terminals[i].outcome == uint8_t(C::SdoOutcome::SUCCESS) && rec.terminals[i].step >= 1);
    CHECK(rec.before.stackFree == 8999 && rec.after.stackFree == 8998 && r->samples == 2);
    CHECK(rec.last.blocks == 0x1c && rec.last.statusword == 0x8037 && rec.last.monitor == 1);
    const std::string text = r->dump();
    CHECK(has(text, "TRIAL id=5 node=1 route=actions failure=0 why=none lost=0 terminals=22"));
    CHECK(has(text, "TRIAL_PLAN route=actions node=1 trial=5 attended=0\n"));
    CHECK(has(text, "TRIAL_RES stack0=8999 stack1=8998"));
    CHECK(has(text, "RESULT op=") && has(text, "object=607a:00 dir=0 outcome=0 ack=1"));
    CHECK(has(text, "TRAFFIC k=1 ") && has(text, "TRAFFIC k=0 "));
    // Each trial identity once, rising, and one route at a time.
    CHECK(std::strcmp(r->start("A1,6"), "busy") == 0);
    r->release();
    CHECK(std::strcmp(r->start("A1,5"), "trial") == 0);
    CHECK(std::strcmp(r->start("A1,4"), "trial") == 0);
    const size_t before = r->driver.requests.size();
    r->run("A2,6");
    CHECK(r->driver.requests.size() == before + 22 && r->driver.requests.back().id == 0x602);
}
// Packet 13 P1 and packet 14 T0: images around the typed groups; never 2400:0B.
void testIoAndTuningReads() {
    scenario = "io";
    std::unique_ptr<Rig> r(new Rig);
    const E::RouteRecord& io = r->run("I1,1");
    CHECK(io.failure == E::RouteFailure::NONE && io.terminalCount == 37);
    size_t at = 0;
    expectUploads(*r, at, 1, IMAGE, 7);
    for (uint8_t sub = 0; sub <= 8; ++sub, ++at)
        CHECK(upload(r->driver.requests[at], 1, 0x2400, sub));
    for (uint8_t sub = 0x0c; sub <= 0x0f; ++sub, ++at)
        CHECK(upload(r->driver.requests[at], 1, 0x2400, sub));
    for (uint16_t index = 0x2401; index <= 0x2407; ++index, ++at)
        CHECK(upload(r->driver.requests[at], 1, index));
    for (uint8_t sub = 0; sub <= 2; ++sub, ++at)
        CHECK(upload(r->driver.requests[at], 1, 0x60fe, sub));
    expectUploads(*r, at, 1, IMAGE, 7);
    CHECK(at == r->driver.requests.size() && r->downloadsFrom(0) == 0);
    for (const M::Frame& f : r->driver.requests)
        CHECK(!(indexOf(f) == 0x2400 && f.data[3] == 0x0b));
    r->dump();
    r->release();
    scenario = "tuning";
    const size_t first = r->driver.requests.size();
    const E::RouteRecord& tuning = r->run("U1,2");
    CHECK(tuning.failure == E::RouteFailure::NONE && tuning.terminalCount == 45);
    at = first;
    expectUploads(*r, at, 1, IMAGE, 7);
    for (uint16_t index = 0x2502; index <= 0x2520; ++index, ++at)
        CHECK(upload(r->driver.requests[at], 1, index));
    expectUploads(*r, at, 1, IMAGE, 7);
    CHECK(at == r->driver.requests.size() && r->downloadsFrom(first) == 0);
    r->dump();
}
// Packet 15 P2 then P3 then P4 on one power-up of the fake.
void testModeEnableStop() {
    scenario = "mode";
    std::unique_ptr<Rig> r(new Rig);
    const E::RouteRecord& mode = r->run("M1,10,1,3,200");
    CHECK(mode.failure == E::RouteFailure::NONE && mode.cycles == 3 && mode.checkPresent && mode.checkPassed);
    size_t at = 0;
    expectUploads(*r, at, 1, FULL, 9);
    const Address rest[] = {{0x6081, 0}, {0x6083, 0}, {0x6084, 0}, {0x1017, 0}, {0x607a, 0}, {0x60ff, 0}};
    expectUploads(*r, at, 1, rest, 6);
    const uint8_t modeOne[8] = {0x2f, 0x60, 0x60, 0, 1, 0, 0, 0};
    CHECK(download(r->driver.requests[at++], 1, modeOne));
    expectUploads(*r, at, 1, FULL, 9); // The first cycle reads 1001 and 603F once.
    expectUploads(*r, at, 1, IMAGE, 7);
    expectUploads(*r, at, 1, IMAGE, 7);
    CHECK(at == r->driver.requests.size() && r->downloadsFrom(0) == 1 && r->driver.drives[0].mode == 1);
    CHECK(mode.motionTotal == 1 && mode.caseCount == 1 && !r->one.motionHistoryWritten());
    std::string text = r->dump();
    CHECK(has(text, "TRIAL_CHECK passed=1 blocks=1d 1001=00 603f=0000 2100=0001 6041=8037 6061=00"));
    r->release();
    CHECK(std::strcmp(r->start("M1,11,1,3,200"), "history") == 0);
    // Bounded ingress alone qualifies no ordinary write, so the enable waits for a heartbeat.
    const size_t quiet = r->driver.requests.size();
    CHECK(std::strcmp(r->start("E1,12,1,1,1,32823,5000,120,10000,800"), "eligibility") == 0);
    r->turns(20);
    CHECK(r->driver.requests.size() == quiet && r->route.motionAdmissions() == 1);
    r->heartbeat(1);

    scenario = "enable";
    const size_t enableStart = r->driver.requests.size();
    const E::RouteRecord& enable = r->run("E1,12,1,1,1,32823,5000,120,10000,800");
    CHECK(enable.failure == E::RouteFailure::NONE && enable.enableRequested);
    const L::ActionContext& c = r->one.enableAction();
    CHECK(c.state == M::ActionState::SUCCEEDED && c.outcome == M::ActionOutcome::OBSERVED);
    CHECK(c.enable.evidence.trial == 12 && c.enable.evidence.powerUpShaft == L::ShaftObservation::FREE);
    at = enableStart;
    expectUploads(*r, at, 1, FULL, 9);
    CHECK(upload(r->driver.requests[at++], 1, 0x607a) && upload(r->driver.requests[at++], 1, 0x60ff));
    const uint16_t words[] = {6, 7, 15};
    const Address cycle[] = {{0x2100, 0}, {0x2101, 0}, {0x2102, 0}, {0x6041, 0}, {0x6061, 0}, {0x6064, 0}, {0x606c, 0}};
    for (uint16_t w : words) {
        const uint8_t bytes[8] = {0x2b, 0x40, 0x60, 0, uint8_t(w), 0, 0, 0};
        CHECK(download(r->driver.requests[at++], 1, bytes));
        expectUploads(*r, at, 1, cycle, 7);
    }
    CHECK(at == r->driver.requests.size());
    CHECK(r->one.latestControl() && r->one.latestControl()->transfer.data[0] == 0x0f);
    text = r->dump();
    CHECK(has(text, "TRIAL_PLAN route=enable node=1 trial=12 attended=1 start=1 shaft=1 status=8037 pt=5000 vt=120"));
    CHECK(has(text, "TRIAL_ENABLE own=1 state=2"));
    CHECK(has(text, "TRIAL_ENABLE_STEP word=15 present=1 status=8037"));
    r->release();

    scenario = "stop";
    const size_t stopStart = r->driver.requests.size();
    const E::RouteRecord& stop = r->run("S1,13,1,1,2000,0,100");
    CHECK(stop.failure == E::RouteFailure::NONE && stop.stopRequested &&
          stop.stopPathRequested == uint8_t(E::StopPath::CONFIRMED));
    at = stopStart;
    const Address stopState[] = {{0x2100, 0}, {0x2101, 0}, {0x2102, 0}, {0x6041, 0}, {0x6061, 0}};
    expectUploads(*r, at, 1, stopState, 5);
    const uint8_t halt[8] = {0x2b, 0x40, 0x60, 0, 0x0f, 0x01, 0, 0};
    CHECK(download(r->driver.requests[at++], 1, halt));
    CHECK(r->driver.drives[0].lastWord == 0x010f && r->downloadsFrom(stopStart) == 1);
    text = r->dump();
    CHECK(has(text, "TRIAL_STOP kind=1 path=1 final=4 own=1 state=2"));
    CHECK(has(text, "TRIAL_BEST_EFFORT which=0 own=0"));
    r->release();
    // The 010F does not establish a 000F again, so a position experiment is refused.
    CHECK(std::strcmp(r->start("P1,14,1,0,16,1,1000,1000,4,250,0,0,0,0,0"), "control") == 0);
}
// Packet 19: the reviewed native writes in order, one trigger, fresh cycles and a named stop.
void testPosition() {
    scenario = "position";
    std::unique_ptr<Rig> r(new Rig);
    r->run("M1,1,1,1,200");
    r->release();
    r->heartbeat(1);
    r->run("E1,2,1,1,1,32823,5000,120,10000,800");
    r->release();
    const size_t first = r->driver.requests.size();
    const E::RouteRecord& p = r->run("P1,3,1,0,16,1,1000,1000,6,100,1,2,2000,0,200");
    CHECK(p.failure == E::RouteFailure::NONE && p.checkPassed && p.cycles == 2 && p.stopRequested);
    CHECK(p.motionTotal == 4 && p.caseCount == 1);
    size_t at = first;
    expectUploads(*r, at, 1, FULL, 9);
    const Address before[] = {{0x6081, 0}, {0x6083, 0}, {0x6084, 0}, {0x607a, 0}};
    expectUploads(*r, at, 1, before, 4);
    const uint8_t writes[5][8] = {{0x23, 0x7a, 0x60, 0, 16, 0, 0, 0},
                                  {0x23, 0x81, 0x60, 0, 1, 0, 0, 0},
                                  {0x23, 0x83, 0x60, 0, 0xe8, 3, 0, 0},
                                  {0x23, 0x84, 0x60, 0, 0xe8, 3, 0, 0},
                                  {0x2b, 0x40, 0x60, 0, 0x1f, 0, 0, 0}};
    for (const auto& bytes : writes)
        CHECK(download(r->driver.requests[at++], 1, bytes));
    expectUploads(*r, at, 1, IMAGE, 7);
    expectUploads(*r, at, 1, IMAGE, 7);
    const uint8_t halt[8] = {0x2b, 0x40, 0x60, 0, 0x0f, 0x01, 0, 0};
    CHECK(download(r->driver.requests[at++], 1, halt));
    CHECK(r->downloadsFrom(first) == 6);
    CHECK(r->one.stopAction(L::StopKind::HALT).predecessorWord == 0x001f);
    const std::string text = r->dump();
    CHECK(has(text, "TRIAL_PLAN route=position node=1 trial=3 attended=1 sp=0 target=16 rpm=1 up=1000 down=1000"));
    CHECK(has(text, "TRIAL_CHECK passed=1 blocks=1d 1001=00 603f=0000 2100=0001 6041=8037 6061=01 6064=0 606c=0"));
    CHECK(has(text, "TRIAL_LAST passed=1 blocks=1c 1001=00 603f=0000 2100=0004")); // Running, as reported.
    r->release();
    // The session wrote motion: no new enable on it, and no second experiment without a new 000F.
    CHECK(std::strcmp(r->start("E1,4,1,1,1,32823,16,120,10000,800"), "history") == 0);
    CHECK(std::strcmp(r->start("P1,4,1,16,16,1,1000,1000,4,250,0,0,0,0,0"), "control") == 0);
}
// Prerequisites refuse before any write; a lost reply ends the route with nothing after it.
void testPrerequisitesAndLostReplies() {
    scenario = "mode-prerequisite";
    std::unique_ptr<Rig> r(new Rig);
    r->driver.drives[0].mode = 1;
    const E::RouteRecord& m = r->run("M1,1,1,3,200");
    CHECK(m.failure == E::RouteFailure::PREREQUISITE && std::strcmp(m.why, "mode") == 0);
    CHECK(r->downloadsFrom(0) == 0 && m.terminalCount == 15);
    r->release();
    r->driver.drives[0].mode = 0;
    r->driver.drives[0].velocity = 2;
    CHECK(r->run("M1,2,1,3,200").failure == E::RouteFailure::PREREQUISITE);
    CHECK(std::strcmp(r->route.record()->why, "velocity") == 0 && r->downloadsFrom(0) == 0);
    r->release();
    r->driver.drives[0].velocity = 0;

    scenario = "mode-alarm";
    r->driver.drives[0].alarmAfterMode = L::MONITOR_ALARM;
    const E::RouteRecord& alarm = r->run("M1,3,1,5,200");
    CHECK(alarm.failure == E::RouteFailure::OBSERVATION && std::strcmp(alarm.why, "alarm") == 0);
    CHECK(alarm.cycles == 1 && r->downloadsFrom(0) == 1);
    r->release();

    scenario = "mode-lost";
    std::unique_ptr<Rig> lost(new Rig);
    lost->driver.drives[1].silentIndex = 0x6060;
    const E::RouteRecord& l = lost->run("M2,1,1,3,200");
    CHECK(l.failure == E::RouteFailure::SERVICE && std::strcmp(l.why, "uncertain") == 0);
    CHECK(lost->downloadsFrom(0) == 1 && lost->driver.requests.back().data[0] == 0x2f);
    lost->turns(2000);
    CHECK(lost->downloadsFrom(0) == 1); // Never replayed.
    lost->release();
    // The mode may have changed unacknowledged: the session's record refuses any enable, and the
    // route stays unresolved, so nothing reopens it for another route either.
    CHECK(lost->two.motionHistoryWritten());
    CHECK(lost->start("E2,2,1,1,1,32823,5000,120,10000,800") != nullptr);
    CHECK(lost->downloadsFrom(0) == 1);

    scenario = "position-prerequisite";
    std::unique_ptr<Rig> p(new Rig);
    p->run("M1,1,1,1,200");
    p->release();
    p->heartbeat(1);
    p->run("E1,2,1,1,1,32823,5000,120,10000,800");
    p->release();
    const size_t first = p->driver.requests.size();
    const E::RouteRecord& wrong = p->run("P1,3,1,5,16,1,1000,1000,4,100,0,0,0,0,0");
    CHECK(wrong.failure == E::RouteFailure::PREREQUISITE && std::strcmp(wrong.why, "position") == 0);
    CHECK(p->downloadsFrom(first) == 0);
    p->release();

    scenario = "position-trigger-lost";
    p->driver.drives[0].silentIndex = 0x6040;
    p->driver.drives[0].silentWord = 0x001f;
    const size_t second = p->driver.requests.size();
    const E::RouteRecord& trigger = p->run("P1,4,1,0,16,1,1000,1000,4,100,1,2,2000,0,200");
    CHECK(trigger.failure == E::RouteFailure::SERVICE && std::strcmp(trigger.why, "uncertain") == 0);
    CHECK(p->downloadsFrom(second) == 5 && !trigger.stopRequested); // No stop follows by itself.
    p->turns(500);
    CHECK(p->downloadsFrom(second) == 5);
    p->dump();
}
// Enable refusals and withdrawals: reviewed mismatch, EMCY and a local abort.
void testEnableWithdrawals() {
    scenario = "enable-mismatch";
    std::unique_ptr<Rig> r(new Rig);
    r->run("M1,1,1,1,200");
    r->release();
    r->heartbeat(1);
    const size_t first = r->driver.requests.size();
    const E::RouteRecord& mismatch = r->run("E1,2,1,1,1,32823,5001,120,10000,800");
    CHECK(mismatch.failure == E::RouteFailure::ACTION && r->downloadsFrom(first) == 0);
    CHECK(has(r->dump(), "TRIAL_ENABLE own=1 state=3"));
    r->release();

    scenario = "enable-refused";
    // Without the attendant's shaft observation the library refuses the power-up enabled pattern.
    // The parser never yields shaft 0, so a held drive with a different reviewed word refuses instead.
    const E::RouteRecord& refused = r->run("E1,3,1,1,1,32822,5000,120,10000,800");
    CHECK(refused.failure == E::RouteFailure::REFUSED && refused.statusPresent);
    CHECK(has(r->dump(), "TRIAL_STATUS code="));
    r->release();

    scenario = "enable-emcy";
    std::unique_ptr<Rig> e(new Rig);
    e->run("M1,1,1,1,200");
    e->release();
    e->heartbeat(1);
    const size_t start = e->driver.requests.size();
    CHECK(!e->start("E1,2,1,1,1,32823,5000,120,10000,800"));
    for (unsigned i = 0; i < 4000 && e->downloadsFrom(start) < 1; ++i)
        e->turn();
    M::Frame emcy = frame(0x81, 8);
    emcy.data[0] = 0x10;
    emcy.data[1] = 0xff;
    e->driver.push(emcy);
    for (unsigned i = 0; i < 80000 && e->route.active(); ++i)
        e->turn();
    const E::RouteRecord& fault = *e->route.record();
    CHECK(fault.failure == E::RouteFailure::OBSERVATION && std::strcmp(fault.why, "emcy") == 0 && fault.emcy);
    CHECK(e->downloadsFrom(start) == 1); // Word 6 only: no word follows a routed fault.
    CHECK(e->one.enableAction().outcome == M::ActionOutcome::DRIVE_FAULT);
    CHECK(has(e->dump(), "TRIAL_EMCY data=10ff"));
    e->release();

    scenario = "enable-abort";
    std::unique_ptr<Rig> a(new Rig);
    a->run("M1,1,1,1,200");
    a->release();
    a->heartbeat(1);
    const size_t mark = a->driver.requests.size();
    CHECK(!a->start("E1,2,1,1,1,32823,5000,120,10000,800"));
    for (unsigned i = 0; i < 4000 && a->downloadsFrom(mark) < 1; ++i)
        a->turn();
    CHECK(!a->route.abort(a->console.now));
    CHECK(std::strcmp(a->route.abort(a->console.now), "already") == 0);
    for (unsigned i = 0; i < 80000 && a->route.active(); ++i)
        a->turn();
    CHECK(a->route.record()->failure == E::RouteFailure::ABORTED);
    CHECK(a->downloadsFrom(mark) == 1);
    CHECK(a->one.enableAction().outcome == M::ActionOutcome::CANCELLED);
    a->turns(500);
    CHECK(a->downloadsFrom(mark) == 1); // No stop was sent.
}
// The stop's policy decides: refused without history, best effort only when allowed.
void testStopPolicy() {
    scenario = "stop-refused";
    std::unique_ptr<Rig> r(new Rig);
    const E::RouteRecord& refused = r->run("S1,1,1,2,2000,0,100");
    CHECK(refused.failure == E::RouteFailure::REFUSED && std::strcmp(refused.why, "stop") == 0);
    CHECK(r->downloadsFrom(0) == 0 && refused.stopPathRequested == uint8_t(E::StopPath::NONE));
    r->release();
    scenario = "stop-best-effort";
    // A fresh idle report in mode 0 refuses even the fallback; nothing says what 000B does there.
    CHECK(r->run("S1,2,1,1,2000,3000,100").failure == E::RouteFailure::REFUSED && r->downloadsFrom(0) == 0);
    r->release();
    // In mode 1 with no latest controlword, only the policy's best-effort branch remains.
    r->driver.drives[0].mode = 1;
    const E::RouteRecord& fallback = r->run("S1,3,1,1,2000,3000,100");
    CHECK(fallback.failure == E::RouteFailure::ACTION && std::strcmp(fallback.why, "best-effort") == 0);
    const uint8_t quick[8] = {0x2b, 0x40, 0x60, 0, 0x0b, 0, 0, 0};
    CHECK(download(r->driver.requests.back(), 1, quick) && r->downloadsFrom(0) == 1);
    const std::string text = r->dump();
    CHECK(has(text, "TRIAL_BEST_EFFORT which=0 own=1"));
    r->release();
    // A best-effort stop leaves its route unresolved, and nothing here reopens it.
    const size_t sent = r->driver.requests.size();
    CHECK(std::strcmp(r->start("A1,4"), "session") == 0);
    r->turns(100);
    CHECK(r->driver.requests.size() == sent && r->route.motionAdmissions() == 3);
}
// Three per route and node, twelve in all, counted at admission whatever the outcome.
void testMotionBudget() {
    scenario = "budget";
    std::unique_ptr<Rig> r(new Rig);
    uint32_t trial = 0;
    const auto id = [&trial]() { return std::to_string(++trial); };
    for (unsigned node = 1; node <= 2; ++node)
        for (unsigned i = 0; i < 3; ++i) {
            // No latest controlword and no best-effort branch: refused by the session, sends nothing.
            CHECK(r->run("S" + std::to_string(node) + "," + id() + ",1,1,100,0,10").failure ==
                  E::RouteFailure::REFUSED);
            r->release();
        }
    CHECK(std::strcmp(r->start("S1," + id() + ",1,2,100,0,10"), "budget") == 0);
    CHECK(r->route.motionAdmissions() == 6);
    for (unsigned node = 1; node <= 2; ++node) {
        r->run("M" + std::to_string(node) + "," + id() + ",1,1,200");
        r->release();
        r->heartbeat(node);
    }
    // A reviewed 607A that does not match ends each enable before any word, so none writes.
    for (unsigned i = 0; i < 3; ++i) {
        CHECK(r->run("E1," + id() + ",1,1,1,32823,5001,120,10000,800").failure == E::RouteFailure::ACTION);
        r->release();
    }
    CHECK(std::strcmp(r->start("E1," + id() + ",1,1,1,32823,5001,120,10000,800"), "budget") == 0);
    CHECK(r->run("E2," + id() + ",1,1,1,32823,5001,120,10000,800").failure == E::RouteFailure::ACTION);
    r->release();
    CHECK(r->route.motionAdmissions() == 12);
    CHECK(std::strcmp(r->start("E2," + id() + ",1,1,1,32823,5001,120,10000,800"), "budget") == 0);
    CHECK(r->downloadsFrom(0) == 2); // The two 6060 writes.
    // Reads are not motion admissions.
    CHECK(r->run("A1," + id()).failure == E::RouteFailure::NONE);
}
// A read abort ends after the in-flight upload settles, with nothing after it.
void testReadAbort() {
    scenario = "read-abort";
    std::unique_ptr<Rig> r(new Rig);
    CHECK(!r->start("U1,1"));
    r->turns(20);
    CHECK(!r->route.abort(r->console.now));
    for (unsigned i = 0; i < 1000 && r->route.active(); ++i)
        r->turn();
    const size_t sent = r->driver.requests.size();
    CHECK(r->route.record()->failure == E::RouteFailure::ABORTED && sent < 45);
    r->turns(200);
    CHECK(r->driver.requests.size() == sent);
    CHECK(has(r->dump(), "why=abort"));
}
// A full set of records for the parser's grammar check in s2_evidence_test.py.
void writeTranscript(const char* path) {
    std::unique_ptr<Rig> r(new Rig);
    std::string out;
    const char* const lines[] = {"A1,1", "I1,2", "U1,3", "M1,4,1,2,200", "E1,5,1,1,1,32823,5000,120,10000,800",
                                 "P1,6,1,0,16,1,1000,1000,4,100,1,2,2000,0,200"};
    const char* const stop = "S1,7,1,1,2000,0,100";
    for (unsigned i = 0; i < 7; ++i) {
        const char* text = i < 6 ? lines[i] : stop;
        if (i == 6)
            r->driver.drives[0].silentIndex = 0x2101; // A lost reply: failed evidence, nothing after.
        if (i == 4)
            r->heartbeat(1);
        r->run(text);
        char admitted[160];
        std::snprintf(admitted, sizeof admitted,
                      "TRIAL admitted route=%s node=%u trial=%lu motion=%u case=%u; finite typed route, no retry\n",
                      E::routeName(r->route.record()->plan.kind), unsigned(r->route.record()->plan.node),
                      static_cast<unsigned long>(r->route.record()->plan.trial),
                      unsigned(r->route.record()->motionTotal), unsigned(r->route.record()->caseCount));
        out += std::string("=== ") + text + "\n" + admitted + r->dump();
        r->turns(5);
        r->route.release();
    }
    std::FILE* file = std::fopen(path, "wb");
    CHECK(file);
    CHECK(std::fwrite(out.data(), 1, out.size(), file) == out.size());
    std::fclose(file);
}
} // namespace

int main(int argc, char** argv) {
    if (argc == 3 && std::strcmp(argv[1], "--transcript") == 0) {
        writeTranscript(argv[2]);
        return 0;
    }
    testParse();
    testPassiveAndRefusals();
    testActionsReads();
    testIoAndTuningReads();
    testModeEnableStop();
    testPosition();
    testPrerequisitesAndLostReplies();
    testEnableWithdrawals();
    testStopPolicy();
    testMotionBudget();
    testReadAbort();
    std::printf("bench_route: %u checks\n", checks);
    return 0;
}
