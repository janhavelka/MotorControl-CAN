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
// The fake's identity: raw codes chosen here, not any real model or firmware. 1009 and 100A answer in
// one byte, as the route reads them.
const uint32_t DEVICE_TYPE = 0x00040192, VENDOR = 0x00000300, PRODUCT = 0x00008605, REVISION = 0x00010001;
const uint16_t HARDWARE = 0x12, SOFTWARE = 0x34, MODEL = 0x0086, FIRMWARE = 0x0123;
// This test's acceptance and completion bits for the fake's 6041. Not a dialect: no specimen has shown
// what either bit does.
const uint16_t ACCEPT_BIT = 0x1000, COMPLETE_BIT = 0x8000;
// One synthetic drive. Widths follow the EDS declarations the typed reads expect.
struct Drive {
    uint8_t errorRegister = 0;
    uint16_t errorCode = 0, monitor = 1, status = 0x8037, lastWord = 0;
    uint8_t mode = 0;
    int32_t position = 0, velocity = 0, positionTarget = 5000, velocityTarget = 120;
    uint32_t rpm = 7, rampUp = 8, rampDown = 9;
    unsigned moveReads = 0;   // 2100 reads that still report running after a trigger.
    unsigned travelReads = 3; // moveReads set by a 001F.
    uint16_t silentIndex = 0; // No reply to this object (any direction).
    uint16_t silentWord = 0;  // With 6040: only this word goes unanswered.
    uint16_t abortIndex = 0;  // Abort 08000020 for this object.
    uint16_t lateIndex = 0;   // The first reply to this object is held in `late` for the test to send.
    uint16_t mismatchIndex = 0; // Replies to this object name the next subindex.
    uint16_t alarmAfterMode = 0; // 2100 bits set once 6060 has been written.
    uint16_t monitorAfterTrigger = 0; // 2100 bits set by a 001F.
    bool followWords = true;
    bool writesOnly = false;    // silentIndex, abortIndex, lateIndex and mismatchIndex spare uploads.
    bool ignoreTrigger = false; // A 001F changes nothing the fake reports: no acceptance, no travel.
    // A 001F after 000F sets ACCEPT_BIT and clears COMPLETE_BIT; arrival sets COMPLETE_BIT; a 000F after
    // 001F clears ACCEPT_BIT. keepComplete leaves an old COMPLETE_BIT set through the trigger.
    bool statusBits = false, keepComplete = false;
    bool lateHeld = false;
    M::Frame late;
    unsigned downloads = 0;
    unsigned width(uint16_t index, uint8_t sub) const {
        switch (index) {
        case 0x1001: case 0x6061: case 0x1009: case 0x100a:
            return 1;
        case 0x1018:
            return sub ? 4 : 1;
        case 0x60fe:
            return sub ? 4 : 1;
        case 0x1000: case 0x6064: case 0x606c: case 0x607a: case 0x60ff: case 0x6081: case 0x6083: case 0x6084:
            return 4;
        default:
            return 2;
        }
    }
    uint32_t value(uint16_t index, uint8_t sub) {
        switch (index) {
        case 0x1000: return DEVICE_TYPE;
        case 0x1009: return HARDWARE;
        case 0x100a: return SOFTWARE;
        case 0x1018: return sub == 0 ? 3 : sub == 1 ? VENDOR : sub == 2 ? PRODUCT : REVISION;
        case 0x2000: return MODEL;
        case 0x2001: return FIRMWARE;
        case 0x1001: return errorRegister;
        case 0x603f: return errorCode;
        case 0x2100:
            if (moveReads) {
                if (!--moveReads) {
                    position = positionTarget;
                    if (statusBits)
                        status = uint16_t(status | COMPLETE_BIT);
                }
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
        const uint16_t word = uint16_t(request.data[4] | uint16_t(request.data[5]) << 8);
        if (writesOnly && request.data[0] == 0x40)
            return answer2(request, reply);
        if (index == silentIndex && (index != 0x6040 || !silentWord || word == silentWord))
            return false;
        if (index && index == lateIndex && !lateHeld) {
            // Acts on the request, but its reply waits for the test: a late reply.
            lateIndex = 0;
            late = reply;
            lateHeld = answer(request, late);
            return false;
        }
        if (index && index == mismatchIndex) {
            const bool sent = answer2(request, reply);
            reply.data[3] = uint8_t(sub + 1);
            return sent;
        }
        return answer2(request, reply);
    }
    bool answer2(const M::Frame& request, M::Frame& reply) {
        const uint16_t index = indexOf(request);
        const uint8_t sub = request.data[3];
        const bool upload = request.data[0] == 0x40;
        const uint16_t word = uint16_t(request.data[4] | uint16_t(request.data[5]) << 8);
        std::memcpy(reply.data + 1, request.data + 1, 3);
        if (index == abortIndex && !(writesOnly && upload)) {
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
                const uint16_t previous = lastWord;
                lastWord = word;
                const uint16_t low = word == 6 ? 1 : word == 7 ? 3 : word == 15 ? 7 : uint16_t(status & 7u);
                if (followWords)
                    status = uint16_t((status & ~L::STATUS_LOW_BITS) | low);
                if (word == 0x001f)
                    monitor = uint16_t(monitor | monitorAfterTrigger);
                if (word == 0x001f && !ignoreTrigger) {
                    moveReads = travelReads;
                    if (statusBits && previous == 0x000f)
                        status = uint16_t((status | ACCEPT_BIT) & (keepComplete ? 0xffffu : ~COMPLETE_BIT));
                }
                if (word == 0x000f && previous == 0x001f && statusBits)
                    status = uint16_t(status & ~ACCEPT_BIT);
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
    E::AxisSession& session(unsigned node) { return node == 2 ? two : one; }
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
    // A bounded reply after its handoff qualifies service, so no heartbeat is needed first.

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
// ------------------------------------------------------------------ packet 19 and 20 closing audit
// Turn until the active route completed `count` observation cycles, or ended.
void untilCycles(Rig& r, unsigned count) {
    for (unsigned i = 0; i < 80000 && r.route.active() && r.route.record()->cycles < count; ++i)
        r.turn();
}
void finishRoute(Rig& r) {
    for (unsigned i = 0; i < 80000 && r.route.active(); ++i)
        r.turn();
    CHECK(!r.route.active() && r.route.record() && r.route.record()->terminal);
}
unsigned wordsFrom(const Rig& r, size_t first, unsigned node, uint16_t word) {
    const uint8_t bytes[8] = {0x2b, 0x40, 0x60, 0, uint8_t(word), uint8_t(word >> 8), 0, 0};
    unsigned count = 0;
    for (size_t i = first; i < r.driver.requests.size(); ++i)
        count += download(r.driver.requests[i], node, bytes);
    return count;
}
std::string id(uint32_t& trial) { return std::to_string(++trial); }
// P2 and P3 on one node, as the earlier suites run them: the latest acknowledged word is then 000F.
void enabled(Rig& r, uint32_t& trial, unsigned node = 1) {
    const std::string n = std::to_string(node);
    CHECK(r.run("M" + n + "," + id(trial) + ",1,1,200").failure == E::RouteFailure::NONE);
    r.release();
    r.heartbeat(node);
    CHECK(r.run("E" + n + "," + id(trial) + ",1,1,1,32823,5000,120,10000,800").failure == E::RouteFailure::NONE);
    r.release();
    CHECK(r.session(node).latestControl() && r.session(node).latestControl()->transfer.data[0] == 0x0f);
}
std::string position(unsigned node, uint32_t& trial, int sp, int target, unsigned cycles = 4, unsigned interval = 100,
                     const char* stop = "0,0,0,0,0") {
    return "P" + std::to_string(node) + "," + id(trial) + ",1," + std::to_string(sp) + "," + std::to_string(target) +
           ",1,1000,1000," + std::to_string(cycles) + "," + std::to_string(interval) + "," + stop;
}
std::string rearm(unsigned node, uint32_t& trial, unsigned cycles = 3, unsigned interval = 100, unsigned attended = 1) {
    return "R" + std::to_string(node) + "," + id(trial) + "," + std::to_string(attended) + "," +
           std::to_string(cycles) + "," + std::to_string(interval);
}
// The host's line for MOVE. Every evidence value here is this fake's, never a measurement.
struct MoveSpec {
    unsigned attended = 1;
    long sp = 0, target = 16;
    unsigned rpm = 1, up = 1000, down = 1000, deadline = 10000, interval = 1000;
    unsigned long record = 19;
    unsigned as = 1, am = ACCEPT_BIT, av = ACCEPT_BIT, cs = 1, cm = COMPLETE_BIT, cv = COMPLETE_BIT, rule = 1;
    unsigned long dt = DEVICE_TYPE, hw = HARDWARE, sw = SOFTWARE, vid = VENDOR, pc = PRODUCT, rev = REVISION;
    unsigned long mc = MODEL, fc = FIRMWARE;
    const char* stop = "0,0,0,0,0";
};
std::string move(unsigned node, uint32_t& trial, const MoveSpec& m = MoveSpec()) {
    char text[300];
    std::snprintf(text, sizeof text,
                  "G%u,%s,%u,%ld,%ld,%u,%u,%u,%u,%u,%lu,%u,%u,%u,%u,%u,%u,%u,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%s", node,
                  id(trial).c_str(), m.attended, m.sp, m.target, m.rpm, m.up, m.down, m.deadline, m.interval, m.record,
                  m.as, m.am, m.av, m.cs, m.cm, m.cv, m.rule, m.dt, m.hw, m.sw, m.vid, m.pc, m.rev, m.mc, m.fc, m.stop);
    return text;
}
const Address IDENTITY[] = {{0x1000, 0}, {0x1009, 0}, {0x100a, 0}, {0x1018, 0}, {0x1018, 1},
                            {0x1018, 2}, {0x1018, 3}, {0x2000, 0}, {0x2001, 0}};

void testParseRearmMove() {
    scenario = "parse-rearm-move";
    E::RoutePlan p;
    const std::string r = "R2,9,1,16,5000";
    CHECK(E::parseRouteCommand(r.data(), r.size(), p) && p.kind == E::RouteKind::REARM && p.node == 2 && p.attended &&
          p.cycles == 16 && p.intervalMs == 5000);
    uint32_t trial = 40;
    MoveSpec spec;
    spec.sp = -3;
    spec.stop = "2,4,2000,3000,200";
    const std::string g = move(1, trial, spec);
    CHECK(E::parseRouteCommand(g.data(), g.size(), p) && p.kind == E::RouteKind::MOVE && p.trial == 41);
    CHECK(p.startPosition == -3 && p.target == 16 && p.rpm == 1 && p.rampUpMs == 1000 && p.deadlineMs == 10000 &&
          p.intervalMs == 1000 && p.record == 19 && p.acceptSource == 1 && p.acceptMask == ACCEPT_BIT &&
          p.completeValue == COMPLETE_BIT && p.rule == 1 && p.firmware.deviceType == DEVICE_TYPE &&
          p.firmware.hardwareVersion == HARDWARE && p.firmware.revision == REVISION &&
          p.firmware.firmwareCode == FIRMWARE && p.stopKind == 2 && p.stopAfter == 4 && p.bestEffortMs == 3000);
    // The longest line the host can build fits ROUTE_LINE with its integrity field; one byte more is
    // refused whole.
    MoveSpec widest;
    widest.sp = INT32_MIN;
    widest.target = -1000000;
    widest.rpm = 3000;
    widest.deadline = 55000;
    widest.interval = 5000;
    widest.record = widest.dt = widest.vid = widest.pc = widest.rev = UINT32_MAX;
    widest.am = widest.av = widest.cm = widest.cv = 65535;
    widest.hw = widest.sw = widest.mc = widest.fc = 65535;
    widest.stop = "2,16,55000,55000,10000";
    uint32_t big = UINT32_MAX - 1;
    const std::string longest = move(1, big, widest);
    CHECK(longest.size() + 10 <= E::ROUTE_LINE && E::parseRouteCommand(longest.data(), longest.size(), p));
    std::string padded = "R1,1,1,1,100";
    padded.insert(1, std::string(E::ROUTE_LINE, '0'));
    CHECK(!E::parseRouteCommand(padded.data(), padded.size(), p));
    struct Bad {
        const char* why;
        MoveSpec spec;
    } bad[12];
    bad[0].spec.rpm = 0;          // Zero speed is refused before any library call.
    bad[1].spec.up = 0;           // Zero ramps are unqualified (Q11).
    bad[2].spec.target = 1000001; // Outside the documentary 607A range.
    bad[3].spec.am = 0;           // A signal with no mask.
    bad[4].spec.av = 0x1001;      // A value outside its own mask.
    bad[5].spec.cs = 3;           // No such signal source.
    bad[6].spec.rule = 0;         // No completion rule.
    bad[7].spec.record = 0;       // The record identity is nonzero.
    bad[8].spec.stop = "1,0,2000,0,200";   // A stop with no cycle.
    bad[9].spec.stop = "0,2,0,0,0";        // No stop naming a cycle.
    bad[10].spec.stop = "1,2,2000,1000,200"; // Best effort before the stop deadline.
    bad[11].spec.interval = 19;   // Under the cycle spacing.
    for (const Bad& b : bad) {
        const std::string text = move(1, trial, b.spec);
        E::RoutePlan untouched;
        CHECK(!E::parseRouteCommand(text.data(), text.size(), untouched) && untouched.kind == E::RouteKind::NONE);
    }
    const char* const badRearm[] = {"R1,1,1,0,100", "R1,1,1,17,100", "R1,1,1,4,19", "R1,1,2,4,100", "R1,1,1,4"};
    for (const char* text : badRearm)
        CHECK(!E::parseRouteCommand(text, std::strlen(text), p));
    CHECK(std::strcmp(E::routeName(E::RouteKind::REARM), "rearm") == 0 &&
          std::strcmp(E::routeName(E::RouteKind::MOVE), "move") == 0);
}
// Packet 19: every prefix write aborted or silent ends the route at that write, with nothing after it,
// and nothing that needs a 000F follows: no second experiment, rearm or move.
void testPositionPrefixFailures() {
    const uint16_t indices[] = {0x607a, 0x6081, 0x6083, 0x6084, 0x6040};
    for (unsigned silent = 0; silent < 2; ++silent)
        for (unsigned k = 0; k < 5; ++k) {
            scenario = silent ? "position-silent" : "position-abort";
            std::unique_ptr<Rig> r(new Rig);
            uint32_t trial = 0;
            enabled(*r, trial);
            Drive& d = r->driver.drives[0];
            d.writesOnly = true; // 607A, 6081, 6083 and 6084 are read for the record first.
            if (silent) {
                d.silentIndex = indices[k];
                d.silentWord = indices[k] == 0x6040 ? 0x001f : 0;
            } else {
                d.abortIndex = indices[k];
            }
            const size_t first = r->driver.requests.size();
            const E::RouteRecord& p = r->run(position(1, trial, 0, 16, 4, 100, "1,2,2000,0,200"));
            CHECK(p.failure == E::RouteFailure::SERVICE && !p.stopRequested);
            // A server abort of a write leaves its effect uncertain too: it does not show that nothing changed.
            const E::RouteTerminal& last = p.terminals[p.terminalCount - 1];
            CHECK(std::strcmp(p.why, "uncertain") == 0 && last.index == indices[k] && (last.flags & 4u));
            CHECK(silent ? last.outcome != uint8_t(C::SdoOutcome::SUCCESS)
                         : last.outcome == uint8_t(C::SdoOutcome::SERVER_ABORT) && last.abortCode == 0x08000020);
            CHECK(r->downloadsFrom(first) == k + 1 && wordsFrom(*r, first, 1, 0x010f) == 0);
            CHECK(!r->one.latestControl());
            r->turns(500);
            CHECK(r->downloadsFrom(first) == k + 1);
            r->dump();
            r->release();
            CHECK(r->start(position(1, trial, 0, 16)) != nullptr);
            CHECK(r->start(rearm(1, trial)) != nullptr);
            CHECK(r->start(move(1, trial)) != nullptr);
            r->turns(200);
            CHECK(r->downloadsFrom(first) == k + 1 && wordsFrom(*r, first, 1, 0x001f) == (k == 4 ? 1u : 0u));
        }
}
// A reply that names another object, and one that arrives after its budget: neither is taken as the
// acknowledgement, the route ends there, and the late frame is only kept in the tail.
void testPositionLateAndMismatchedReplies() {
    scenario = "position-mismatch";
    std::unique_ptr<Rig> r(new Rig);
    uint32_t trial = 0;
    enabled(*r, trial);
    r->driver.drives[0].writesOnly = true;
    r->driver.drives[0].mismatchIndex = 0x6083;
    size_t first = r->driver.requests.size();
    const E::RouteRecord& m = r->run(position(1, trial, 0, 16));
    CHECK(m.failure == E::RouteFailure::SERVICE && r->downloadsFrom(first) == 3);
    CHECK(m.terminals[m.terminalCount - 1].index == 0x6083 &&
          m.terminals[m.terminalCount - 1].outcome != uint8_t(C::SdoOutcome::SUCCESS));
    r->turns(500);
    CHECK(r->downloadsFrom(first) == 3);
    r->dump();

    scenario = "position-late";
    std::unique_ptr<Rig> l(new Rig);
    trial = 0;
    enabled(*l, trial);
    Drive& d = l->driver.drives[0];
    d.writesOnly = true;
    d.lateIndex = 0x6084;
    first = l->driver.requests.size();
    const E::RouteRecord& late = l->run(position(1, trial, 0, 16));
    CHECK(late.failure == E::RouteFailure::SERVICE && std::strcmp(late.why, "uncertain") == 0);
    CHECK(l->downloadsFrom(first) == 4 && d.lateHeld);
    const unsigned terminals = late.terminalCount;
    l->driver.push(d.late); // The acknowledgement, after its budget was spent.
    l->turns(50);
    CHECK(late.terminalCount == terminals && late.failure == E::RouteFailure::SERVICE);
    CHECK(has(l->dump(), "id=581 ext=0 rtr=0 dlc=8 data=6084600000000000"));
    l->turns(500);
    CHECK(l->downloadsFrom(first) == 4 && wordsFrom(*l, first, 1, 0x001f) == 0);
    l->release();
    CHECK(l->start(position(1, trial, 0, 16)) != nullptr && l->start(rearm(1, trial)) != nullptr);
    CHECK(l->downloadsFrom(first) == 4);
}
// A boot-up, the route cutoff and an EMCY during the cycles each end the route; the trigger is never
// replayed and no stop follows by itself.
void testPositionRebootCutoffEmcy() {
    scenario = "position-reboot";
    std::unique_ptr<Rig> r(new Rig);
    uint32_t trial = 0;
    enabled(*r, trial);
    size_t first = r->driver.requests.size();
    CHECK(!r->start(position(1, trial, 0, 16, 6, 300, "1,4,2000,0,200")));
    untilCycles(*r, 1);
    M::Frame boot = frame(0x701, 1);
    boot.data[0] = 0;
    r->driver.push(boot);
    finishRoute(*r);
    const E::RouteRecord& b = *r->route.record();
    // Between cycles here: the boot-up withdrew the reservation, so the next cycle has no operation.
    CHECK(b.failure == E::RouteFailure::INVALIDATED && std::strcmp(b.why, "session") == 0 && b.cycles == 1 &&
          !b.stopRequested);
    CHECK(r->downloadsFrom(first) == 5 && wordsFrom(*r, first, 1, 0x001f) == 1);
    r->turns(500);
    CHECK(r->downloadsFrom(first) == 5);
    r->dump();
    r->release();
    CHECK(r->start(position(1, trial, 0, 16)) != nullptr && r->start(rearm(1, trial)) != nullptr &&
          r->start(move(1, trial)) != nullptr);
    CHECK(r->downloadsFrom(first) == 5);

    scenario = "position-cutoff";
    std::unique_ptr<Rig> c(new Rig);
    trial = 0;
    enabled(*c, trial);
    first = c->driver.requests.size();
    CHECK(!c->start(position(1, trial, 0, 16, 2, 5000)));
    untilCycles(*c, 1);
    CHECK(c->route.active() && c->route.record()->cycles == 1);
    c->console.now += uint64_t(E::ROUTE_CUTOFF_MS) * 1000; // Between cycles, with nothing in flight.
    finishRoute(*c);
    const E::RouteRecord& cut = *c->route.record();
    CHECK(cut.failure == E::RouteFailure::DEADLINE && std::strcmp(cut.why, "cutoff") == 0 && cut.cycles == 1);
    CHECK(c->downloadsFrom(first) == 5);
    c->dump();

    scenario = "position-emcy";
    std::unique_ptr<Rig> e(new Rig);
    trial = 0;
    enabled(*e, trial);
    first = e->driver.requests.size();
    CHECK(!e->start(position(1, trial, 0, 16, 6, 300, "1,4,2000,0,200")));
    untilCycles(*e, 1);
    M::Frame emcy = frame(0x81, 8);
    emcy.data[0] = 0x10;
    emcy.data[1] = 0xff;
    e->driver.push(emcy);
    finishRoute(*e);
    const E::RouteRecord& f = *e->route.record();
    CHECK(f.failure == E::RouteFailure::OBSERVATION && std::strcmp(f.why, "emcy") == 0 && !f.stopRequested);
    CHECK(e->downloadsFrom(first) == 5 && wordsFrom(*e, first, 1, 0x010f) == 0);
    CHECK(has(e->dump(), "TRIAL_EMCY data=10ff"));
}
// No new report after the trigger, and old completion candidates already set before it: the route
// keeps every value and claims neither acceptance nor completion. Its NONE means its steps ran.
void testPositionNoClaims() {
    scenario = "position-no-claims";
    std::unique_ptr<Rig> r(new Rig);
    uint32_t trial = 0;
    enabled(*r, trial);
    r->driver.drives[0].ignoreTrigger = true;
    const size_t first = r->driver.requests.size();
    const E::RouteRecord& p = r->run(position(1, trial, 0, 16, 3));
    CHECK(p.failure == E::RouteFailure::NONE && p.cycles == 3 && r->downloadsFrom(first) == 5);
    const std::string text = r->dump();
    // 6041 bit 15 and 2100 bit 0 read set before the trigger and unchanged after it.
    CHECK(has(text, "TRIAL_CHECK passed=1 blocks=1d 1001=00 603f=0000 2100=0001 6041=8037 6061=01 6064=0 606c=0"));
    CHECK(has(text, "TRIAL_LAST passed=1 blocks=1c 1001=00 603f=0000 2100=0001 6041=8037 6061=01 6064=0 606c=0"));
    CHECK(!has(text, "TRIAL_MOVE") && !has(text, "accept"));
}
// Packet 19's rearm: one 000F through the session after a finished POSITION, then fresh cycles.
void testRearm() {
    scenario = "rearm";
    std::unique_ptr<Rig> r(new Rig);
    uint32_t trial = 0;
    CHECK(std::strcmp(r->start(rearm(1, trial, 3, 100, 0)), "unattended") == 0);
    CHECK(std::strcmp(r->start(rearm(1, trial)), "control") == 0); // No controlword at all.
    enabled(*r, trial);
    CHECK(std::strcmp(r->start(rearm(1, trial)), "control") == 0); // The enable's 000F.
    CHECK(r->route.motionAdmissions() == 2);
    const E::RouteRecord& p = r->run(position(1, trial, 0, 16));
    CHECK(p.failure == E::RouteFailure::NONE);
    const uint32_t triggerOp = r->one.latestControl()->transfer.operation;
    r->release();
    size_t first = r->driver.requests.size();
    const E::RouteRecord& a = r->run(rearm(1, trial, 3));
    CHECK(a.failure == E::RouteFailure::NONE && a.checkPassed && a.cycles == 3 && a.rearmFollows == triggerOp);
    size_t at = first;
    expectUploads(*r, at, 1, FULL, 9);
    const uint8_t word[8] = {0x2b, 0x40, 0x60, 0, 0x0f, 0, 0, 0};
    CHECK(download(r->driver.requests[at++], 1, word));
    for (unsigned i = 0; i < 3; ++i)
        expectUploads(*r, at, 1, IMAGE, 7);
    CHECK(at == r->driver.requests.size() && r->downloadsFrom(first) == 1);
    CHECK(r->one.latestControl() && r->one.latestControl()->transfer.data[0] == 0x0f);
    std::string text = r->dump();
    CHECK(has(text, "TRIAL_PLAN route=rearm node=1 trial="));
    CHECK(has(text, "TRIAL_CHECK passed=1 blocks=1d 1001=00 603f=0000 2100=0001 6041=8037 6061=01 6064=16 606c=0"));
    CHECK(has(text, ("TRIAL_REARM predop=" + std::to_string(triggerOp)).c_str()) && has(text, " latest=1 word=000f latestop="));
    r->release();
    // Once: its own 000F is now the latest word.
    CHECK(std::strcmp(r->start(rearm(1, trial)), "control") == 0);
    // A second experiment from the new position, and its own rearm, then a third.
    CHECK(r->run(position(1, trial, 16, 32)).failure == E::RouteFailure::NONE);
    r->release();
    CHECK(r->run(rearm(1, trial)).failure == E::RouteFailure::NONE);
    r->release();
    CHECK(r->run(position(1, trial, 32, 48)).failure == E::RouteFailure::NONE);
    r->release();
    CHECK(r->run(rearm(1, trial)).failure == E::RouteFailure::NONE);
    r->release();
    CHECK(r->route.motionAdmissions() == 8);
    first = r->driver.requests.size();
    CHECK(std::strcmp(r->start(rearm(1, trial)), "budget") == 0);
    CHECK(std::strcmp(r->start(position(1, trial, 48, 64)), "budget") == 0);
    r->turns(100);
    CHECK(r->driver.requests.size() == first);

    scenario = "rearm-after-stop";
    std::unique_ptr<Rig> s(new Rig);
    trial = 0;
    enabled(*s, trial);
    CHECK(s->run(position(1, trial, 0, 16, 4, 100, "1,2,2000,0,200")).failure == E::RouteFailure::NONE);
    s->release();
    CHECK(std::strcmp(s->start(rearm(1, trial)), "control") == 0); // The stop's 010F is the latest.

    scenario = "rearm-running";
    std::unique_ptr<Rig> g(new Rig);
    trial = 0;
    enabled(*g, trial);
    g->driver.drives[0].travelReads = 10;
    CHECK(g->run(position(1, trial, 0, 16, 1)).failure == E::RouteFailure::NONE);
    g->release();
    first = g->driver.requests.size();
    const E::RouteRecord& running = g->run(rearm(1, trial));
    CHECK(running.failure == E::RouteFailure::PREREQUISITE && std::strcmp(running.why, "running") == 0);
    CHECK(g->downloadsFrom(first) == 0 && running.checkPresent && !running.checkPassed);
    g->dump();

    scenario = "rearm-lost";
    std::unique_ptr<Rig> l(new Rig);
    trial = 0;
    enabled(*l, trial);
    CHECK(l->run(position(1, trial, 0, 16)).failure == E::RouteFailure::NONE);
    l->release();
    l->driver.drives[0].silentIndex = 0x6040;
    l->driver.drives[0].silentWord = 0x000f;
    first = l->driver.requests.size();
    const E::RouteRecord& lost = l->run(rearm(1, trial));
    CHECK(lost.failure == E::RouteFailure::SERVICE && std::strcmp(lost.why, "uncertain") == 0);
    CHECK(l->downloadsFrom(first) == 1 && !l->one.latestControl());
    text = l->dump();
    CHECK(has(text, " latest=0 word=0000 latestop=0"));
    l->turns(500);
    CHECK(l->downloadsFrom(first) == 1);
    l->release();
    CHECK(l->start(rearm(1, trial)) != nullptr && l->start(position(1, trial, 16, 32)) != nullptr &&
          l->start(move(1, trial)) != nullptr);
    l->turns(200);
    CHECK(l->downloadsFrom(first) == 1);

    scenario = "rearm-cutoff";
    std::unique_ptr<Rig> c(new Rig);
    trial = 0;
    enabled(*c, trial);
    CHECK(c->run(position(1, trial, 0, 16)).failure == E::RouteFailure::NONE);
    c->release();
    first = c->driver.requests.size();
    CHECK(!c->start(rearm(1, trial, 2, 5000)));
    untilCycles(*c, 1);
    c->console.now += uint64_t(E::ROUTE_CUTOFF_MS) * 1000;
    finishRoute(*c);
    CHECK(c->route.record()->failure == E::RouteFailure::DEADLINE && std::strcmp(c->route.record()->why, "cutoff") == 0);
    CHECK(c->downloadsFrom(first) == 1);
    c->dump();
}
// Packet 20 through AxisSession::requestMove, with the host's evidence. The fake's bits are this test's.
void testMove() {
    scenario = "move";
    std::unique_ptr<Rig> r(new Rig);
    r->driver.drives[0].statusBits = true;
    uint32_t trial = 0;
    MoveSpec unattended;
    unattended.attended = 0;
    CHECK(std::strcmp(r->start(move(1, trial, unattended)), "unattended") == 0);
    enabled(*r, trial);
    MoveSpec wide;
    wide.deadline = 55000;
    wide.interval = 20;
    CHECK(std::strcmp(r->start(move(1, trial, wide)), "capacity") == 0);
    MoveSpec longer;
    longer.deadline = 55000;
    longer.interval = 5000;
    longer.stop = "1,1,5000,0,10000";
    CHECK(std::strcmp(r->start(move(1, trial, longer)), "duration") == 0);
    size_t first = r->driver.requests.size();
    const E::RouteRecord& m = r->run(move(1, trial));
    CHECK(m.failure == E::RouteFailure::NONE && m.moveRequested && m.checkPassed && m.cycles >= 2);
    const L::MoveContext& c = r->one.moveAction();
    CHECK(c.state == M::ActionState::SUCCEEDED && c.completion == M::ActionCompletion::OBSERVED &&
          c.rearm == M::ActionExecution::ACKNOWLEDGED && c.execution == M::ActionExecution::ACKNOWLEDGED);
    CHECK(!r->one.moveFenced() && c.evidence.record == 19 && c.evidence.firmware.firmwareCode == FIRMWARE);
    size_t at = first;
    expectUploads(*r, at, 1, IDENTITY, 9);
    expectUploads(*r, at, 1, FULL, 9);
    const uint8_t writes[5][8] = {{0x23, 0x7a, 0x60, 0, 16, 0, 0, 0},
                                  {0x23, 0x81, 0x60, 0, 1, 0, 0, 0},
                                  {0x23, 0x83, 0x60, 0, 0xe8, 3, 0, 0},
                                  {0x23, 0x84, 0x60, 0, 0xe8, 3, 0, 0},
                                  {0x2b, 0x40, 0x60, 0, 0x1f, 0, 0, 0}};
    for (const auto& bytes : writes)
        CHECK(download(r->driver.requests[at++], 1, bytes));
    const uint8_t rearmWord[8] = {0x2b, 0x40, 0x60, 0, 0x0f, 0, 0, 0};
    CHECK(download(r->driver.requests.back(), 1, rearmWord) && r->downloadsFrom(first) == 6);
    CHECK(r->one.latestControl() && r->one.latestControl()->transfer.data[0] == 0x0f);
    std::string text = r->dump();
    CHECK(has(text, "TRIAL_PLAN route=move node=1 trial="));
    CHECK(has(text, "TRIAL_EVIDENCE record=19 as=1 am=1000 av=1000 cs=1 cm=8000 cv=8000 rule=1 dt=00040192 hw=0012 "
                    "sw=0034 vid=00000300 pc=00008605 rev=00010001 mc=0086 fc=0123"));
    CHECK(has(text, "TRIAL_MOVE own=1 state=2 ") && has(text, " completion=1 rearm=1 ") && has(text, " fenced=0 "));
    CHECK(has(text, "TRIAL_MOVE_SAMPLE which=2 present=1 ") && has(text, "TRIAL_MOVE_SAMPLE which=4 present=1 "));
    r->release();
    // The move's own rearm leaves a 000F, so a second and third move may follow; a fourth is over the cap.
    MoveSpec next;
    next.sp = 16;
    next.target = 32;
    CHECK(r->run(move(1, trial, next)).failure == E::RouteFailure::NONE);
    r->release();
    next.sp = 32;
    next.target = 48;
    CHECK(r->run(move(1, trial, next)).failure == E::RouteFailure::NONE);
    r->release();
    next.sp = 48;
    next.target = 64;
    first = r->driver.requests.size();
    CHECK(std::strcmp(r->start(move(1, trial, next)), "budget") == 0);
    r->turns(100);
    CHECK(r->driver.requests.size() == first && r->route.motionAdmissions() == 5);
}
// Refusals before any write: the session's or the library's, each recorded.
void testMoveRefusals() {
    scenario = "move-refusals";
    std::unique_ptr<Rig> r(new Rig);
    r->driver.drives[0].statusBits = true;
    uint32_t trial = 0;
    CHECK(r->run("M1," + id(trial) + ",1,1,200").failure == E::RouteFailure::NONE);
    r->release();
    r->heartbeat(1);
    CHECK(std::strcmp(r->start(move(1, trial)), "control") == 0); // No enable: no 000F.
    CHECK(r->run("E1," + id(trial) + ",1,1,1,32823,5000,120,10000,800").failure == E::RouteFailure::NONE);
    r->release();
    const size_t first = r->driver.requests.size();
    MoveSpec firmware;
    firmware.fc = FIRMWARE + 1; // The record names other firmware than the fresh read.
    const E::RouteRecord& f = r->run(move(1, trial, firmware));
    CHECK(f.failure == E::RouteFailure::REFUSED && std::strcmp(f.why, "move") == 0 && f.statusPresent &&
          f.statusCode == uint8_t(M::Err::UNSUPPORTED));
    CHECK(has(r->dump(), "TRIAL_STATUS code="));
    r->release();
    MoveSpec early;
    early.am = early.av = 0x8000; // Reported before any trigger, so it cannot show this one.
    const E::RouteRecord& a = r->run(move(1, trial, early));
    CHECK(a.failure == E::RouteFailure::REFUSED && a.statusDetail == int32_t(M::MoveError::ACCEPTANCE));
    r->release();
    MoveSpec elsewhere;
    elsewhere.sp = 5;
    const E::RouteRecord& w = r->run(move(1, trial, elsewhere));
    CHECK(w.failure == E::RouteFailure::PREREQUISITE && std::strcmp(w.why, "position") == 0);
    r->release();
    CHECK(r->downloadsFrom(first) == 0 && !r->one.moveFenced() && r->one.latestControl());
    // Each was a motion admission whatever the library then said: the fourth is over the cap.
    CHECK(std::strcmp(r->start(move(1, trial)), "budget") == 0 && r->route.motionAdmissions() == 5);

    scenario = "move-cutoff";
    // A move whose deadline would pass the route cutoff is refused before any write.
    std::unique_ptr<Rig> c(new Rig);
    trial = 0;
    enabled(*c, trial);
    MoveSpec late;
    late.deadline = 50000;
    late.interval = 5000;
    const size_t mark = c->driver.requests.size();
    CHECK(!c->start(move(1, trial, late)));
    c->console.now += 11000000; // Before anything is in flight.
    finishRoute(*c);
    CHECK(c->route.record()->failure == E::RouteFailure::REFUSED &&
          std::strcmp(c->route.record()->why, "deadline") == 0);
    CHECK(c->downloadsFrom(mark) == 0 && !c->route.record()->moveRequested);
    c->dump();
}
// Lost replies, no acceptance and a stale completion: each ends as the move decides, uncertain where a
// trigger may have reached the drive, with no write after it and new moves fenced.
void testMoveFailures() {
    scenario = "move-lost-parameter";
    std::unique_ptr<Rig> r(new Rig);
    r->driver.drives[0].statusBits = true;
    uint32_t trial = 0;
    enabled(*r, trial);
    r->driver.drives[0].writesOnly = true;
    r->driver.drives[0].silentIndex = 0x6083;
    size_t first = r->driver.requests.size();
    const E::RouteRecord& p = r->run(move(1, trial));
    CHECK(p.failure == E::RouteFailure::ACTION && std::strcmp(p.why, "move") == 0 && r->downloadsFrom(first) == 3);
    CHECK(r->one.moveAction().setup == M::ActionExecution::UNKNOWN &&
          r->one.moveAction().execution == M::ActionExecution::NOT_TRANSMITTED && !r->one.moveFenced());
    r->turns(500);
    CHECK(r->downloadsFrom(first) == 3);
    r->dump();
    r->release();
    CHECK(r->start(move(1, trial)) != nullptr && r->downloadsFrom(first) == 3);

    scenario = "move-lost-trigger";
    std::unique_ptr<Rig> t(new Rig);
    t->driver.drives[0].statusBits = true;
    trial = 0;
    enabled(*t, trial);
    t->driver.drives[0].silentIndex = 0x6040;
    t->driver.drives[0].silentWord = 0x001f;
    first = t->driver.requests.size();
    const E::RouteRecord& lost = t->run(move(1, trial));
    CHECK(lost.failure == E::RouteFailure::ACTION && t->downloadsFrom(first) == 5);
    CHECK(t->one.moveFenced() && t->one.moveAction().uncertain &&
          t->one.moveAction().execution == M::ActionExecution::UNKNOWN);
    CHECK(has(t->dump(), " fenced=1 "));
    t->turns(500);
    t->release();
    CHECK(t->start(move(1, trial)) != nullptr && t->start(position(1, trial, 0, 16)) != nullptr &&
          t->start(rearm(1, trial)) != nullptr);
    t->turns(200);
    CHECK(t->downloadsFrom(first) == 5 && wordsFrom(*t, first, 1, 0x001f) == 1);

    const char* const names[] = {"move-no-acceptance", "move-stale-completion"};
    for (unsigned k = 0; k < 2; ++k) {
        scenario = names[k];
        std::unique_ptr<Rig> s(new Rig);
        Drive& d = s->driver.drives[0];
        d.statusBits = true;
        trial = 0;
        enabled(*s, trial);
        if (k)
            d.keepComplete = true; // Bit 15 stays set from before the trigger: never a new completion.
        else
            d.ignoreTrigger = true; // No new bit at all.
        MoveSpec spec;
        spec.deadline = 6000;
        first = s->driver.requests.size();
        const E::RouteRecord& m = s->run(move(1, trial, spec));
        const L::MoveContext& c = s->one.moveAction();
        CHECK(m.failure == E::RouteFailure::ACTION && c.outcome == M::ActionOutcome::DEADLINE &&
              c.completion == M::ActionCompletion::NOT_OBSERVED && s->one.moveFenced());
        CHECK(c.acceptance.present == (k == 1) && !c.completed.present);
        CHECK(s->downloadsFrom(first) == 5 && wordsFrom(*s, first, 1, 0x000f) == 0); // No rearm.
        s->dump();
    }
}
// A named stop through STOP's path after a stated cycle, a local abort and an EMCY during a move.
void testMoveInterruptions() {
    scenario = "move-stop";
    std::unique_ptr<Rig> r(new Rig);
    Drive& d = r->driver.drives[0];
    d.statusBits = true;
    d.travelReads = 40;
    uint32_t trial = 0;
    enabled(*r, trial);
    MoveSpec spec;
    spec.deadline = 6000; // The stop's polls must fit the record beside the move's own.
    spec.stop = "1,1,2000,0,500";
    size_t first = r->driver.requests.size();
    const E::RouteRecord& s = r->run(move(1, trial, spec));
    CHECK(s.failure == E::RouteFailure::NONE && s.stopRequested && s.stopPathRequested == uint8_t(E::StopPath::CONFIRMED));
    CHECK(r->one.moveAction().interruption == M::MoveInterruption::STOP && r->one.moveFenced());
    CHECK(r->one.stopAction(L::StopKind::HALT).predecessorWord == 0x001f);
    CHECK(wordsFrom(*r, first, 1, 0x010f) == 1 && wordsFrom(*r, first, 1, 0x000f) == 0 && r->route.motionAdmissions() == 4);
    std::string text = r->dump();
    CHECK(has(text, "TRIAL_STOP kind=1 path=1 ") && has(text, "TRIAL_MOVE own=1 state=3 ") && has(text, " int=2 "));

    const char* const names[] = {"move-abort", "move-emcy"};
    for (unsigned k = 0; k < 2; ++k) {
        scenario = names[k];
        std::unique_ptr<Rig> a(new Rig);
        a->driver.drives[0].statusBits = true;
        a->driver.drives[0].travelReads = 40;
        trial = 0;
        enabled(*a, trial);
        first = a->driver.requests.size();
        CHECK(!a->start(move(1, trial)));
        for (unsigned i = 0; i < 20000 && a->downloadsFrom(first) < 5; ++i)
            a->turn();
        a->turns(5);
        if (k) {
            M::Frame emcy = frame(0x81, 8);
            emcy.data[0] = 0x10;
            emcy.data[1] = 0xff;
            a->driver.push(emcy);
        } else {
            CHECK(!a->route.abort(a->console.now));
        }
        finishRoute(*a);
        const E::RouteRecord& x = *a->route.record();
        CHECK(x.failure == (k ? E::RouteFailure::OBSERVATION : E::RouteFailure::ABORTED) && !x.stopRequested);
        CHECK(a->one.moveAction().interruption ==
              (k ? M::MoveInterruption::DRIVE_FAULT : M::MoveInterruption::CANCELLED));
        CHECK(a->one.moveFenced() && a->downloadsFrom(first) == 5);
        a->turns(500);
        CHECK(a->downloadsFrom(first) == 5); // No stop and no rearm was sent.
        a->dump();
    }
}
// ------------------------------------------------------------------- review fixes, 2026-11-03
M::Frame beat(unsigned node) {
    M::Frame value = frame(0x700 + node, 1);
    value.data[0] = 0x7f;
    return value;
}
// Turn until the route ends, flooding the bus with heartbeats of `node` once `from` holds, until the
// route's copied traffic overflows. Returns the drive's downloads after the turn that lost it.
unsigned floodUntilEnd(Rig& r, size_t first, unsigned node, bool (*from)(const Rig&, size_t)) {
    unsigned atLoss = UINT32_MAX;
    for (unsigned i = 0; i < 200000 && r.route.active(); ++i) {
        for (unsigned k = 0; k < 12 && atLoss == UINT32_MAX && r.driver.count < 12 && from(r, first); ++k)
            r.driver.push(beat(node));
        r.turn();
        if (atLoss == UINT32_MAX && r.route.record()->evidenceLost)
            atLoss = r.downloadsFrom(first);
    }
    CHECK(!r.route.active());
    return atLoss;
}
bool triggerSent(const Rig& r, size_t first) {
    const uint8_t bytes[8] = {0x2b, 0x40, 0x60, 0, 0x1f, 0, 0, 0};
    for (size_t i = first; i < r.driver.requests.size(); ++i)
        if (download(r.driver.requests[i], 1, bytes))
            return true;
    return false;
}
bool wordSent(const Rig& r, size_t first) { return r.downloadsFrom(first) != 0; }
// Lost evidence ends an action route EVIDENCE_LOSS, never NONE, and withdraws its move or enable locally:
// the lost span may have held an EMCY. Nothing at all is sent because of the loss.
void testLostEvidence() {
    scenario = "lost-move";
    std::unique_ptr<Rig> r(new Rig);
    r->driver.drives[0].statusBits = true;
    r->driver.drives[0].travelReads = 40;
    uint32_t trial = 0;
    enabled(*r, trial);
    size_t first = r->driver.requests.size();
    CHECK(!r->start(move(1, trial)));
    unsigned atLoss = floodUntilEnd(*r, first, 1, triggerSent);
    const E::RouteRecord& m = *r->route.record();
    CHECK(m.failure == E::RouteFailure::EVIDENCE_LOSS && std::strcmp(m.why, "lost") == 0 && m.evidenceLost &&
          m.lossCancel && m.moveRequested && !m.stopRequested);
    CHECK(r->one.moveAction().interruption == M::MoveInterruption::CANCELLED && r->one.moveFenced());
    CHECK(atLoss == 5 && r->downloadsFrom(first) == 5); // The parameters and trigger; no stop, no rearm.
    r->turns(500);
    CHECK(r->downloadsFrom(first) == 5);
    std::string text = r->dump();
    CHECK(has(text, "route=move failure=5 why=lost lost=1 ") && has(text, " lossCancel=1\n"));
    CHECK(has(text, "TRIAL_MOVE own=1 state=3 ") && has(text, " int=1 "));

    scenario = "lost-enable";
    std::unique_ptr<Rig> e(new Rig);
    trial = 0;
    CHECK(e->run("M1," + id(trial) + ",1,1,200").failure == E::RouteFailure::NONE);
    e->release();
    e->heartbeat(1);
    first = e->driver.requests.size();
    CHECK(!e->start("E1," + id(trial) + ",1,1,1,32823,5000,120,10000,800"));
    atLoss = floodUntilEnd(*e, first, 1, wordSent);
    const E::RouteRecord& n = *e->route.record();
    CHECK(n.failure == E::RouteFailure::EVIDENCE_LOSS && n.lossCancel && n.enableRequested);
    CHECK(e->one.enableAction().outcome == M::ActionOutcome::CANCELLED);
    CHECK(atLoss >= 1 && atLoss < 3 && e->downloadsFrom(first) == atLoss); // Never word 15 after the loss.
    e->turns(500);
    CHECK(e->downloadsFrom(first) == atLoss);
    CHECK(has(e->dump(), " lossCancel=1\n"));

    scenario = "lost-position";
    std::unique_ptr<Rig> p(new Rig);
    trial = 0;
    enabled(*p, trial);
    p->driver.drives[0].travelReads = 40;
    first = p->driver.requests.size();
    CHECK(!p->start(position(1, trial, 0, 16, 6, 1000, "1,4,2000,0,500")));
    floodUntilEnd(*p, first, 1, triggerSent);
    const E::RouteRecord& q = *p->route.record();
    // Nothing to withdraw, and the planned stop is not asked for once evidence is lost.
    CHECK(q.failure == E::RouteFailure::EVIDENCE_LOSS && !q.lossCancel && !q.stopRequested);
    CHECK(p->downloadsFrom(first) == 5 && wordsFrom(*p, first, 1, 0x010f) == 0);
    p->dump();
}
// A planned stop must still fit the route cutoff when it is asked for at its latest: otherwise the
// route refuses before its first write, so a refused stop can never leave a trigger running.
void testPlannedStopFits() {
    scenario = "move-stop-fits";
    std::unique_ptr<Rig> r(new Rig);
    r->driver.drives[0].statusBits = true;
    r->driver.drives[0].travelReads = 400;
    uint32_t trial = 0;
    enabled(*r, trial);
    MoveSpec spec;
    spec.deadline = 40000;
    spec.interval = 5000;
    spec.stop = "1,7,10000,15000,5000";
    size_t first = r->driver.requests.size();
    CHECK(!r->start(move(1, trial, spec)));
    r->console.now += 12000000; // As if the identity and full reads had taken twelve seconds.
    finishRoute(*r);
    const E::RouteRecord& m = *r->route.record();
    CHECK(m.failure == E::RouteFailure::REFUSED && std::strcmp(m.why, "deadline") == 0 && !m.moveRequested);
    CHECK(r->downloadsFrom(first) == 0);
    r->dump();
    r->release();

    scenario = "position-stop-fits";
    first = r->driver.requests.size();
    CHECK(!r->start(position(1, trial, 0, 16, 6, 100, "1,4,2000,0,200")));
    r->console.now += 35000000; // Thirty-five seconds before the first write.
    finishRoute(*r);
    const E::RouteRecord& p = *r->route.record();
    CHECK(p.failure == E::RouteFailure::REFUSED && std::strcmp(p.why, "deadline") == 0 && !p.stopRequested);
    CHECK(p.checkPassed && r->downloadsFrom(first) == 0);
    r->dump();
    r->release();
    // Ten cycles of up to five seconds each before the stop can never fit: refused at admission.
    CHECK(std::strcmp(r->start(position(1, trial, 0, 16, 12, 100, "1,10,2000,0,1000")), "duration") == 0);
    CHECK(r->downloadsFrom(first) == 0);
}
// The test's own acknowledged 6040 write through the session, settled before the route's next turn:
// another writer between the route's check and its first write.
void ownWord(Rig& r, uint16_t word) {
    E::AxisSession& s = r.one;
    const uint64_t now = r.firmware.owner().lastTimeUs();
    const uint32_t operation = s.beginOperation(now + 2000000, now);
    CHECK(operation != 0);
    C::SdoTransfer t;
    t.channel = *r.services.channel(0);
    t.operation = t.request = operation;
    t.step = 1;
    t.index = 0x6040;
    t.size = 2;
    t.data[0] = uint8_t(word);
    t.data[1] = uint8_t(word >> 8);
    t.direction = C::SdoDirection::DOWNLOAD;
    t.deadlineUs = now + 2000000;
    t.queueTimeoutUs = E::ROUTE_QUEUE_US;
    t.responseTimeoutUs = E::ROUTE_RESPONSE_US;
    CHECK(s.admitPrepared(t, now) == O::BusAdmission::ACCEPTED);
    C::SdoCompletion done;
    bool settled = false;
    for (unsigned i = 0; i < 1000 && !settled; ++i) {
        r.console.now += 1000;
        r.driver.deliver();
        r.firmware.service();
        settled = s.settle(done);
    }
    CHECK(settled && done.outcome == C::SdoOutcome::SUCCESS);
}
// The predecessor the check step judged is judged again when the first write is offered.
void testPredecessorAtAdmission() {
    scenario = "rearm-predecessor";
    std::unique_ptr<Rig> r(new Rig);
    uint32_t trial = 0;
    enabled(*r, trial);
    CHECK(r->run(position(1, trial, 0, 16)).failure == E::RouteFailure::NONE);
    r->release();
    size_t first = r->driver.requests.size();
    CHECK(!r->start(rearm(1, trial)));
    for (unsigned i = 0; i < 20000 && r->route.active() && r->route.record()->step < 2; ++i)
        r->turn();
    CHECK(r->route.active() && r->route.record()->checkPassed && r->downloadsFrom(first) == 0);
    ownWord(*r, 0x000f); // Now the latest word is not the trigger any more.
    finishRoute(*r);
    const E::RouteRecord& a = *r->route.record();
    CHECK(a.failure == E::RouteFailure::PREREQUISITE && std::strcmp(a.why, "control") == 0);
    CHECK(wordsFrom(*r, first, 1, 0x000f) == 1 && r->downloadsFrom(first) == 1); // Only the other writer's.
    r->dump();

    scenario = "mode-predecessor";
    std::unique_ptr<Rig> m(new Rig);
    first = m->driver.requests.size();
    CHECK(!m->start("M1,1,1,1,200"));
    for (unsigned i = 0; i < 20000 && m->route.active() && m->route.record()->step < 6; ++i)
        m->turn();
    CHECK(m->route.active() && m->route.record()->checkPassed && m->downloadsFrom(first) == 0);
    ownWord(*m, 0x0006);
    finishRoute(*m);
    CHECK(m->route.record()->failure == E::RouteFailure::PREREQUISITE &&
          std::strcmp(m->route.record()->why, "history") == 0);
    CHECK(m->downloadsFrom(first) == 1 && m->driver.drives[0].mode == 0); // No 6060 write.
}
// begin() judges a plan built in code as the parser judges a line, before any bound divides by it.
void testPlanValidation() {
    scenario = "plan";
    std::unique_ptr<Rig> r(new Rig);
    E::RoutePlan enable, stop, move, mode;
    const std::string e = "E1,1,1,1,1,32823,5000,120,10000,800", s = "S1,1,1,1,2000,0,100", g = "G1,1,1,0,16,1,1000,"
                          "1000,10000,1000,19,1,4096,4096,1,32768,32768,1,262546,18,52,768,34309,65537,134,291,1,2,2000,0,"
                          "500";
    CHECK(E::parseRouteCommand(e.data(), e.size(), enable) && E::routePlanValid(enable));
    CHECK(E::parseRouteCommand(s.data(), s.size(), stop) && E::routePlanValid(stop));
    CHECK(E::parseRouteCommand(g.data(), g.size(), move) && E::routePlanValid(move));
    CHECK(E::parseRouteCommand("M1,1,1,1,200", 12, mode) && E::routePlanValid(mode));
    std::vector<E::RoutePlan> bad(12, enable);
    bad[0].intervalMs = 0; // terminalBound divides by it.
    bad[1] = stop;
    bad[1].intervalMs = 0;
    bad[2] = move;
    bad[2].intervalMs = 0;
    bad[3] = move;
    bad[3].stopIntervalMs = 0; // The planned stop's bound divides by it.
    bad[4] = mode;
    bad[4].node = 3;
    bad[5].kind = E::RouteKind::NONE;
    bad[6].kind = static_cast<E::RouteKind>(42);
    bad[7].deadlineMs = 55001;
    bad[8] = move;
    bad[8].acceptValue = 0x1001; // Outside its mask.
    bad[9] = move;
    bad[9].stopAfter = 0; // A stop naming no cycle.
    bad[10] = stop;
    bad[10].bestEffortMs = 1000; // Best effort before the deadline.
    bad[11].shaft = 0;
    for (const E::RoutePlan& plan : bad) {
        CHECK(!E::routePlanValid(plan));
        const char* refusal = r->route.begin(plan, r->console.now);
        CHECK(refusal && std::strcmp(refusal, "plan") == 0);
    }
    r->turns(50);
    CHECK(r->driver.requests.empty() && !r->route.record() && r->route.motionAdmissions() == 0);
    // The same plans as built by the parser are admitted.
    CHECK(!r->route.begin(stop, r->console.now));
}
std::string signLine(const std::string& text) {
    char field[16];
    std::snprintf(field, sizeof field, ",K%08lx", static_cast<unsigned long>(E::routeLineCrc(text.data(), text.size())));
    return text + field;
}
// The host's CRC-32 field: zlib's values, checked before any parse, and every single-digit change that
// still parses as another valid plan is refused.
void testIntegrity() {
    scenario = "integrity";
    CHECK(E::routeLineCrc("", 0) == 0xbe047a60u);                // zlib.crc32(b"T")
    CHECK(E::routeLineCrc("A1,5", 4) == 0x62328164u);            // zlib.crc32(b"TA1,5")
    CHECK(E::routeLineCrc("R2,9,1,16,5000", 14) == 0x3c9f15dbu); // zlib.crc32(b"TR2,9,1,16,5000")
    size_t body = 99;
    const std::string good = "A1,5,K62328164";
    CHECK(E::checkRouteIntegrity(good.data(), good.size(), body) && body == 4);
    const char* const bad[] = {"A1,5",           "A1,5,K6232816",  "A1,5,K623281640", "A1,5,K62328165",
                               "A1,5,K6232816A", "A1,6,K62328164", "A1,5K62328164",   "A1,5,k62328164",
                               "A1,5,K62328164 ", ",K62328164",    "A1,5,K6232816g",  "A1,5,,K62328164"};
    for (const char* text : bad) {
        size_t untouched = 99;
        CHECK(!E::checkRouteIntegrity(text, std::strlen(text), untouched) && untouched == 99);
    }
    CHECK(!E::checkRouteIntegrity(nullptr, 14, body));
    uint32_t trial = 40;
    const std::string lines[] = {move(1, trial), position(1, trial, 0, 16, 4, 100, "1,2,2000,0,200"),
                                 "E1,7,1,1,2,32823,-5000,120,10000,800", rearm(2, trial)};
    unsigned parsed = 0;
    for (const std::string& line : lines) {
        const std::string sent = signLine(line);
        CHECK(E::checkRouteIntegrity(sent.data(), sent.size(), body) && body == line.size());
        E::RoutePlan p;
        CHECK(E::parseRouteCommand(sent.data(), body, p));
        for (size_t at = 0; at < line.size(); ++at) {
            if (line[at] < '0' || line[at] > '9')
                continue;
            for (char digit = '0'; digit <= '9'; ++digit) {
                if (digit == line[at])
                    continue;
                std::string damaged = sent;
                damaged[at] = digit;
                E::RoutePlan other;
                parsed += E::parseRouteCommand(damaged.data(), line.size(), other);
                size_t untouched = 99;
                CHECK(!E::checkRouteIntegrity(damaged.data(), damaged.size(), untouched) && untouched == 99);
            }
        }
    }
    CHECK(parsed > 100); // Many of these changes are other valid plans: only the field refuses them.
}
// A planned stop the session refuses (path NONE) during a move: the move is withdrawn by the request
// itself, nothing of the stop is sent and later moves are fenced.
void testMoveStopRefused() {
    scenario = "move-stop-none";
    std::unique_ptr<Rig> r(new Rig);
    r->driver.drives[0].statusBits = true;
    r->driver.drives[0].travelReads = 400;
    uint32_t trial = 0;
    enabled(*r, trial);
    MoveSpec spec;
    spec.deadline = 30000;
    spec.interval = 5000;
    spec.stop = "1,1,2000,0,500";
    const size_t first = r->driver.requests.size();
    CHECK(!r->start(move(1, trial, spec)));
    bool aged = false;
    for (unsigned i = 0; i < 200000 && r->route.active(); ++i) {
        r->console.now += 1000;
        r->driver.deliver();
        r->firmware.service();
        const L::MoveContext& m = r->one.moveAction();
        if (!aged && m.observations >= 1 && m.phase == L::MovePhase::WAITING) {
            // The last complete cycle ages past the stop's age cap before the route's turn asks for it.
            aged = true;
            r->console.now += 2000000;
            r->firmware.service();
        }
        r->route.service(r->firmware.owner().lastTimeUs());
    }
    const E::RouteRecord& x = *r->route.record();
    CHECK(aged && x.failure == E::RouteFailure::REFUSED && std::strcmp(x.why, "stop") == 0);
    CHECK(!x.stopRequested && x.stopPathRequested == uint8_t(E::StopPath::NONE) && x.moveRequested);
    CHECK(r->one.moveAction().interruption == M::MoveInterruption::STOP && r->one.moveFenced());
    r->turns(500);
    CHECK(r->downloadsFrom(first) == 5 && wordsFrom(*r, first, 1, 0x010f) == 0 &&
          wordsFrom(*r, first, 1, 0x000b) == 0 && wordsFrom(*r, first, 1, 0x000f) == 0);
    r->dump();
}
// Abort or EMCY while a parameter is on its way, and abort with the trigger admitted but unsent: no
// later download leaves, the trigger never does, and nothing is fenced.
void testMoveEarlyWithdrawals() {
    const char* const names[] = {"move-abort-parameters", "move-emcy-parameters", "move-abort-queued-trigger"};
    for (unsigned k = 0; k < 3; ++k) {
        scenario = names[k];
        std::unique_ptr<Rig> r(new Rig);
        r->driver.drives[0].statusBits = true;
        r->driver.drives[0].travelReads = 40;
        uint32_t trial = 0;
        enabled(*r, trial);
        const size_t first = r->driver.requests.size();
        CHECK(!r->start(move(1, trial)));
        if (k < 2) {
            for (unsigned i = 0; i < 20000 && r->downloadsFrom(first) < 1; ++i)
                r->turn();
        } else {
            for (unsigned i = 0; i < 20000 && r->one.moveAction().phase != L::MovePhase::TRIGGER; ++i)
                r->turn();
            CHECK(r->downloadsFrom(first) == 4 && !triggerSent(*r, first)); // Admitted, not yet handed over.
        }
        if (k == 1) {
            M::Frame emcy = frame(0x81, 8);
            emcy.data[0] = 0x10;
            emcy.data[1] = 0xff;
            r->driver.push(emcy);
        } else {
            CHECK(!r->route.abort(r->console.now));
        }
        finishRoute(*r);
        r->turns(500);
        const E::RouteRecord& x = *r->route.record();
        CHECK(x.failure == (k == 1 ? E::RouteFailure::OBSERVATION : E::RouteFailure::ABORTED) && !x.stopRequested);
        CHECK(r->one.moveAction().interruption ==
              (k == 1 ? M::MoveInterruption::DRIVE_FAULT : M::MoveInterruption::CANCELLED));
        CHECK(!triggerSent(*r, first) && !r->one.moveFenced() && r->downloadsFrom(first) == (k < 2 ? 1u : 4u));
        r->dump();
    }
}
// A trigger acknowledgement that arrives after its budget, and a boot-up during the cycles: the move
// ends without its rearm, stays fenced, and the late frame is only kept in the tail.
void testMoveLateTriggerAndBoot() {
    scenario = "move-late-trigger";
    std::unique_ptr<Rig> r(new Rig);
    Drive& d = r->driver.drives[0];
    d.statusBits = true;
    uint32_t trial = 0;
    enabled(*r, trial);
    d.lateIndex = 0x6040; // The next 6040 is the move's trigger.
    size_t first = r->driver.requests.size();
    const E::RouteRecord& late = r->run(move(1, trial));
    CHECK(late.failure == E::RouteFailure::ACTION && d.lateHeld && r->downloadsFrom(first) == 5);
    CHECK(r->one.moveFenced() && r->one.moveAction().uncertain &&
          r->one.moveAction().execution == M::ActionExecution::UNKNOWN);
    const unsigned terminals = late.terminalCount;
    r->driver.push(d.late);
    r->turns(50);
    CHECK(late.terminalCount == terminals && late.failure == E::RouteFailure::ACTION);
    CHECK(has(r->dump(), "TAIL k=0 ") && has(r->dump(), "id=581 ext=0 rtr=0 dlc=8 data=6040600000000000"));
    r->turns(500);
    CHECK(r->downloadsFrom(first) == 5 && wordsFrom(*r, first, 1, 0x000f) == 0);
    r->release();
    CHECK(r->start(move(1, trial)) != nullptr);

    scenario = "move-boot";
    std::unique_ptr<Rig> b(new Rig);
    b->driver.drives[0].statusBits = true;
    b->driver.drives[0].travelReads = 40;
    trial = 0;
    enabled(*b, trial);
    first = b->driver.requests.size();
    CHECK(!b->start(move(1, trial)));
    for (unsigned i = 0; i < 40000 && b->route.active() && b->one.moveAction().observations < 1; ++i)
        b->turn();
    M::Frame boot = frame(0x701, 1);
    boot.data[0] = 0;
    b->driver.push(boot);
    finishRoute(*b);
    b->turns(500);
    const E::RouteRecord& x = *b->route.record();
    CHECK(x.failure == E::RouteFailure::ACTION && b->one.moveFenced() && b->downloadsFrom(first) == 5);
    CHECK(b->one.moveAction().interruption == M::MoveInterruption::INVALIDATED);
    CHECK(wordsFrom(*b, first, 1, 0x000f) == 0 && wordsFrom(*b, first, 1, 0x010f) == 0);
    b->dump();
    b->release();
    CHECK(b->start(move(1, trial)) != nullptr);
}
// One node's fence and lost trigger do not reach the other node, and the other node's routes do not
// unfence it.
void testFencedNodeIsolation() {
    scenario = "fenced-node-isolation";
    std::unique_ptr<Rig> r(new Rig);
    r->driver.drives[0].statusBits = r->driver.drives[1].statusBits = true;
    uint32_t trial = 0;
    enabled(*r, trial, 1);
    r->driver.drives[0].silentIndex = 0x6040;
    r->driver.drives[0].silentWord = 0x001f;
    CHECK(r->run(move(1, trial)).failure == E::RouteFailure::ACTION && r->one.moveFenced());
    r->release();
    enabled(*r, trial, 2);
    const size_t first = r->driver.requests.size();
    CHECK(r->run(move(2, trial)).failure == E::RouteFailure::NONE && !r->two.moveFenced() && r->one.moveFenced());
    r->release();
    CHECK(r->run(position(2, trial, 16, 32)).failure == E::RouteFailure::NONE);
    r->release();
    CHECK(r->run(rearm(2, trial)).failure == E::RouteFailure::NONE);
    r->release();
    for (size_t i = first; i < r->driver.requests.size(); ++i)
        CHECK(r->driver.requests[i].id == 0x602);
    CHECK(r->one.moveFenced() && r->start(move(1, trial)) != nullptr && r->start(rearm(1, trial)) != nullptr);
}
// A POSITION whose planned stop the session refused: REARM checks the drive afresh, refusing while it
// reports released and admitting its one 000F once the report is idle again.
void testRearmAfterRefusedStop() {
    scenario = "rearm-after-refused-stop";
    std::unique_ptr<Rig> r(new Rig);
    uint32_t trial = 0;
    enabled(*r, trial);
    Drive& d = r->driver.drives[0];
    d.monitorAfterTrigger = L::MONITOR_RELEASED;
    size_t first = r->driver.requests.size();
    const E::RouteRecord& p = r->run(position(1, trial, 0, 16, 6, 100, "1,4,2000,0,200"));
    CHECK(p.failure == E::RouteFailure::REFUSED && std::strcmp(p.why, "stop") == 0 && !p.stopRequested &&
          p.stopPathRequested == uint8_t(E::StopPath::NONE) && p.cycles == 4);
    CHECK(r->downloadsFrom(first) == 5 && wordsFrom(*r, first, 1, 0x010f) == 0 && wordsFrom(*r, first, 1, 0x000b) == 0);
    r->release();
    first = r->driver.requests.size();
    const E::RouteRecord& a = r->run(rearm(1, trial));
    CHECK(a.failure == E::RouteFailure::PREREQUISITE && std::strcmp(a.why, "released") == 0);
    CHECK(r->downloadsFrom(first) == 0);
    r->release();
    d.monitor = uint16_t(d.monitor & ~L::MONITOR_RELEASED);
    CHECK(r->run(rearm(1, trial)).failure == E::RouteFailure::NONE && wordsFrom(*r, first, 1, 0x000f) == 1 &&
          r->downloadsFrom(first) == 1);
}
// The session's handover recheck through FirmwareProbe's preflight: a parameter admitted while the
// admission read was within its age cap, handed over once it was not, never leaves.
void testMovePreflightAtHandover() {
    scenario = "move-preflight";
    std::unique_ptr<Rig> r(new Rig);
    r->driver.drives[0].statusBits = true;
    uint32_t trial = 0;
    enabled(*r, trial);
    const size_t first = r->driver.requests.size();
    CHECK(!r->start(move(1, trial)));
    for (unsigned i = 0; i < 20000 && r->route.active() && !r->route.record()->moveRequested; ++i)
        r->turn();
    CHECK(r->route.record()->moveRequested && r->downloadsFrom(first) == 0);
    // The route's age cap for the move's admission read; the next turn admits 607A at the cap and the
    // turn after that hands it over just past it.
    const uint64_t cap = 9 * (uint64_t(E::ROUTE_QUEUE_US) + E::ROUTE_RESPONSE_US);
    const uint64_t admission = r->one.moveAction().admission.earliestUs;
    CHECK(r->console.now < admission + cap - 1500);
    r->console.now = admission + cap - 1500;
    finishRoute(*r);
    r->turns(200);
    CHECK(r->downloadsFrom(first) == 0 && !r->one.moveFenced());
    CHECK(r->one.moveAction().interruption == M::MoveInterruption::INVALIDATED &&
          r->one.moveRefusal().detail == int32_t(M::MoveError::STATE));
    CHECK(r->route.record()->failure == E::RouteFailure::ACTION);
}
// A full set of records for the parser's grammar check in s2_evidence_test.py: every route, the
// rearm and the move on node 2 after the lost reply ended node 1's evidence, and a second move on node
// 2 whose copied traffic overflowed.
void writeTranscript(const char* path) {
    std::unique_ptr<Rig> r(new Rig);
    r->driver.drives[1].statusBits = true;
    std::string out;
    uint32_t moveTrial = 11;
    const auto append = [&out, &r](const std::string& text) {
        char admitted[160];
        std::snprintf(admitted, sizeof admitted,
                      "TRIAL admitted route=%s node=%u trial=%lu motion=%u case=%u; finite typed route, no retry\n",
                      E::routeName(r->route.record()->plan.kind), unsigned(r->route.record()->plan.node),
                      static_cast<unsigned long>(r->route.record()->plan.trial),
                      unsigned(r->route.record()->motionTotal), unsigned(r->route.record()->caseCount));
        out += std::string("=== ") + text + "\n" + admitted + r->dump();
        r->turns(5);
        r->route.release();
    };
    const std::string lines[] = {"A1,1", "I1,2", "U1,3", "M1,4,1,2,200", "E1,5,1,1,1,32823,5000,120,10000,800",
                                 "P1,6,1,0,16,1,1000,1000,4,100,1,2,2000,0,200", "S1,7,1,1,2000,0,100",
                                 "M2,8,1,1,200", "E2,9,1,1,1,32823,5000,120,10000,800",
                                 "P2,10,1,0,16,1,1000,1000,4,100,0,0,0,0,0", "R2,11,1,3,100", ""};
    const unsigned count = sizeof lines / sizeof lines[0];
    for (unsigned i = 0; i < count; ++i) {
        MoveSpec spec;
        spec.sp = 16;
        spec.target = 32;
        const std::string text = i + 1 < count ? lines[i] : move(2, moveTrial, spec);
        if (i == 6)
            r->driver.drives[0].silentIndex = 0x2101; // A lost reply: failed evidence, nothing after.
        if (i == 4)
            r->heartbeat(1);
        if (i == 8)
            r->heartbeat(2);
        CHECK(r->run(text).failure == (i == 6 ? E::RouteFailure::SERVICE : E::RouteFailure::NONE));
        append(text);
    }
    MoveSpec spec;
    spec.sp = 32;
    spec.target = 48;
    r->driver.drives[1].travelReads = 40;
    const std::string lost = move(2, moveTrial, spec);
    const size_t first = r->driver.requests.size();
    CHECK(!r->start(lost));
    for (unsigned i = 0; i < 200000 && r->route.active(); ++i) {
        if (!r->route.record()->evidenceLost && i % 3 == 0 && r->driver.count < 8 &&
            wordsFrom(*r, first, 2, 0x001f))
            r->driver.push(beat(2));
        r->turn();
    }
    CHECK(r->route.record()->failure == E::RouteFailure::EVIDENCE_LOSS && r->route.record()->lossCancel);
    append(lost);
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
    testParseRearmMove();
    testPositionPrefixFailures();
    testPositionLateAndMismatchedReplies();
    testPositionRebootCutoffEmcy();
    testPositionNoClaims();
    testRearm();
    testMove();
    testMoveRefusals();
    testMoveFailures();
    testMoveInterruptions();
    testLostEvidence();
    testPlannedStopFits();
    testPredecessorAtAdmission();
    testPlanValidation();
    testIntegrity();
    testMoveStopRefused();
    testMoveEarlyWithdrawals();
    testMoveLateTriggerAndBoot();
    testFencedNodeIsolation();
    testRearmAfterRefusedStop();
    testMovePreflightAtHandover();
    std::printf("bench_route: %u checks\n", checks);
    return 0;
}
