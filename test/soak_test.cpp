// SPDX-License-Identifier: MIT
// The soak through the real firmware assembly, owner, SDO engine and typed reads, against a
// synthetic driver and two synthetic drives. Times, values and faults are test inputs, with no
// physical claim. The driver refuses anything but an upload of an accepted object, so every
// scenario here also checks that the soak sends nothing else.
#include "Soak.h"
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
#define CHECK(c)                                                                                   \
    do {                                                                                           \
        ++checks;                                                                                  \
        if (!(c)) {                                                                                \
            std::fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #c);                           \
            std::abort();                                                                          \
        }                                                                                          \
    } while (false)

// DEC-1 accepted list, written here independently of the implementation.
bool accepted(uint16_t index, uint8_t subindex) {
    switch (index) {
    case 0x1000: case 0x1009: case 0x100a: case 0x2000: case 0x2001: case 0x2100: case 0x2101:
    case 0x2102: case 0x6041: case 0x6061: case 0x6064: case 0x606c:
        return subindex == 0;
    case 0x1018:
        return subindex <= 3;
    default:
        return false;
    }
}
unsigned widthOf(uint16_t index, uint8_t subindex) {
    switch (index) {
    case 0x1009: case 0x100a: case 0x6061:
        return 1;
    case 0x1018:
        return subindex ? 4 : 1;
    case 0x2000: case 0x2001: case 0x2100: case 0x2101: case 0x2102: case 0x6041:
        return 2;
    default:
        return 4;
    }
}
M::Frame frame(uint32_t id, uint8_t count) {
    M::Frame value;
    value.id = id;
    value.dlc = value.payloadLength = count;
    return value;
}
struct Console {
    uint64_t now = 1000000;
    bool blocked = false;
    std::string output;
    static bool clock(void* context, uint64_t& out) {
        out = static_cast<Console*>(context)->now;
        return true;
    }
    static bool write(void* context, uint8_t byte) {
        Console& c = *static_cast<Console*>(context);
        if (c.blocked)
            return false;
        c.output.push_back(char(byte));
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
struct Sent {
    unsigned node;
    uint16_t index;
    uint8_t subindex;
};
// Two synthetic drives behind a one-frame controller, as the real shim presents them.
struct Driver {
    Console* console = nullptr;
    E::TwaiEvents pending;
    O::ReceivedFrame incoming[16];
    std::vector<Sent> sent;
    std::vector<M::Frame> requests;
    uint32_t values[2][4] = {{0x8037, 0, 0x0100, 0}, {0x8037, 0, 0x0100, 0}};
    unsigned head = 0, count = 0;
    uint32_t epoch = 0, serial = 0;
    bool live = false, active = false;
    bool silent[2] = {false, false};
    uint16_t abortIndex = 0;
    bool reportLate = false;
    bool reportHeld = false;
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
        CHECK(value.id == 0x601 || value.id == 0x602); // No NMT, PDO or other identifier.
        CHECK(value.dlc == 8);
        if (value.data[0] != 0x80) { // A client abort is local cleanup; nothing else but uploads.
            CHECK(value.data[0] == 0x40);
            const uint16_t index = uint16_t(value.data[1]) | uint16_t(value.data[2]) << 8;
            CHECK(accepted(index, value.data[3]));
            d.sent.push_back(Sent{unsigned(value.id - 0x600), index, value.data[3]});
        }
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
    uint32_t raw(unsigned node, uint16_t index, uint8_t subindex) const {
        const uint32_t* v = values[node - 1];
        switch (index) {
        case 0x6041: return v[0];
        case 0x6061: return v[1];
        case 0x2100: return v[2];
        case 0x2101: return v[3];
        case 0x1000: return 0x00040192;
        case 0x1009: return 0x0c;
        case 0x100a: return 0x20;
        case 0x1018: return subindex == 0 ? 5 : subindex == 1 ? 0x1000 : subindex == 2 ? 1 : 0;
        case 0x2000: return 0x0400;
        case 0x2001: return 0x0200;
        default: return 0;
        }
    }
    // One completed exchange: the final TX report, then the reply one service turn later at most.
    void deliver() {
        if (reportHeld) {
            pending.txDone = true;
            pending.transmitted = true;
            pending.serial = serial;
            reportHeld = false;
            active = false;
        }
        if (!active)
            return;
        if (reportLate) {
            reportHeld = true;
        } else {
            pending.txDone = true;
            pending.transmitted = true;
            pending.serial = serial;
            active = false;
        }
        const M::Frame request = current;
        if (request.data[0] == 0x80)
            return;
        const unsigned node = unsigned(request.id - 0x600);
        if (silent[node - 1])
            return;
        const uint16_t index = uint16_t(request.data[1]) | uint16_t(request.data[2]) << 8;
        M::Frame reply = frame(0x580 + node, 8);
        std::memcpy(reply.data + 1, request.data + 1, 3);
        if (index == abortIndex) {
            reply.data[0] = 0x80;
            reply.data[4] = 0x00;
            reply.data[5] = 0x00;
            reply.data[6] = 0x02;
            reply.data[7] = 0x06; // 06020000, object does not exist.
        } else {
            const unsigned width = widthOf(index, request.data[3]);
            const uint32_t value = raw(node, index, request.data[3]);
            reply.data[0] = uint8_t(0x43u | (4u - width) << 2);
            for (unsigned i = 0; i < width; ++i)
                reply.data[4 + i] = uint8_t(value >> (8 * i));
        }
        push(reply);
    }
};
struct Rig {
    C::SdoClient services;
    Console console;
    Driver driver;
    E::SoakTwaiTap tap;
    E::FirmwareProbe firmware;
    E::Soak soak;
    unsigned samples = 0;
    static void sample(void* context, E::SoakResources& output) {
        ++static_cast<Rig*>(context)->samples;
        output.stackFree = 4321;
        output.heapFree = 70000;
        output.heapMinimum = 65000;
    }
    E::SoakCallbacks soakCallbacks() {
        E::SoakCallbacks value;
        value.context = this;
        value.sample = sample;
        return value;
    }
    Rig()
        : tap(driver.callbacks()), firmware(services, tap.driver(), console.callbacks()),
          soak(firmware, services, tap, soakCallbacks(), "0123456789012345678901234567890123456789") {
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
            CHECK(services.configure(uint8_t(node - 1), channel, false, true));
            C::NetworkRoute route;
            route.binding = channel.binding;
            route.generation = 1;
            route.heartbeatId = 0x700 + node;
            route.emcyId = 0x80 + node;
            CHECK(services.configureNetwork(uint8_t(node - 1), route, C::NetworkObserver(), console.now));
        }
    }
    void turns(unsigned count) {
        for (unsigned i = 0; i < count; ++i) {
            console.now += 1000;
            driver.deliver();
            firmware.service();
            soak.service(console.now);
        }
    }
    unsigned count(const char* text) const {
        unsigned found = 0;
        for (size_t at = console.output.find(text); at != std::string::npos;
             at = console.output.find(text, at + 1))
            ++found;
        return found;
    }
    bool has(const std::string& text) const { return console.output.find(text) != std::string::npos; }
    // Every SOAK record carries its line number; a gap means lost output.
    void linesContiguous() const {
        long expected = -1;
        size_t begin = 0;
        while (begin < console.output.size()) {
            const size_t end = console.output.find('\n', begin);
            if (end == std::string::npos)
                break; // The console is still sending this one.
            const std::string line = console.output.substr(begin, end - begin);
            CHECK(line.size() < 416);
            CHECK(line.rfind("OUTPUT_FORMAT_ERROR", 0) != 0);
            begin = end + 1;
            const size_t mark = line.find(" n=");
            CHECK(line.rfind("SOAK_", 0) == 0 && mark != std::string::npos);
            const long number = std::strtol(line.c_str() + mark + 3, nullptr, 10);
            if (expected >= 0)
                CHECK(number == expected);
            expected = number + 1;
        }
    }
};
E::SoakPlan plan(uint32_t trial = 1, uint32_t duration = 3600, uint32_t interval = 20, bool monitors = false,
                 bool identity = false) {
    E::SoakPlan value;
    value.trial = trial;
    value.durationS = duration;
    value.intervalMs = interval;
    value.summaryS = 3600;
    value.monitors = monitors;
    value.identity = identity;
    return value;
}
void testParse() {
    E::SoakPlan out;
    const char good[] = "7,120,250,60,3";
    CHECK(E::parseSoakCommand(good, sizeof good - 1, out));
    CHECK(out.trial == 7 && out.durationS == 120 && out.intervalMs == 250 && out.summaryS == 60);
    CHECK(out.monitors && out.identity);
    E::SoakPlan untouched;
    const char* const bad[] = {"", "7", "7,120,250,60", "0,120,250,60,0", "7,0,250,60,0",
                               "7,172801,250,60,0", "7,120,19,60,0", "7,120,60001,60,0",
                               "7,120,250,0,0", "7,120,250,3601,0", "7,120,250,60,4",
                               "7,120,250,60,0,1", "7,120,250,60,x", "-7,120,250,60,0",
                               "99999999999,120,250,60,0", "7, 120,250,60,0"};
    for (const char* text : bad) {
        CHECK(!E::parseSoakCommand(text, std::strlen(text), untouched));
        CHECK(untouched.trial == 0);
    }
    CHECK(!E::parseSoakCommand(nullptr, 3, untouched));
}
void testPassiveAndAlternating() {
    std::unique_ptr<Rig> rig(new Rig);
    rig->turns(200);
    CHECK(rig->driver.sent.empty() && rig->console.output.empty()); // Nothing before the command.
    CHECK(rig->soak.begin(plan(), rig->console.now) == E::SoakAdmission::ACCEPTED);
    CHECK(rig->driver.sent.empty()); // Admission itself sends nothing.
    CHECK(rig->soak.begin(plan(2), rig->console.now) == E::SoakAdmission::BUSY);
    rig->turns(1000);
    const std::vector<Sent>& sent = rig->driver.sent;
    CHECK(sent.size() >= 16);
    // Only 6041, 6061, 6064, 606C here: 1001, 603F and 1003 are never reached by a poll.
    const uint16_t order[4] = {0x6041, 0x6061, 0x6064, 0x606c};
    for (size_t i = 0; i < sent.size(); ++i) {
        CHECK(sent[i].index == order[i % 4]);
        CHECK(sent[i].node == 1 + (i / 4) % 2); // One whole operation per node, alternating.
    }
    CHECK(rig->has("SOAK_BEGIN"));
    CHECK(rig->has("trial=1 dur=3600 interval=20 summary=3600 monitors=0 identity=0"));
    CHECK(rig->has("sig=6041 from=0 to=8037 first=1"));
    CHECK(rig->has("sig=health"));
    const E::SoakNodeStats& one = rig->soak.nodeStats(1);
    CHECK(one.operationsOk >= 2 && one.operationsOk == one.operations - (one.operations - one.operationsOk));
    CHECK(one.stepsOk == one.steps && one.latencyCount == one.steps && one.latencyMaxUs >= one.latencyMinUs);
    CHECK(!one.stopped && !rig->soak.nodeStats(2).stopped);
    rig->linesContiguous();
}
// The reply overtakes the final TX report by one turn: the route stays open, the next request
// waits for retirement, and the ordering is counted.
void testReplyBeforeTxReport() {
    std::unique_ptr<Rig> rig(new Rig);
    rig->driver.reportLate = true;
    CHECK(rig->soak.begin(plan(), rig->console.now) == E::SoakAdmission::ACCEPTED);
    rig->turns(1000);
    const E::SoakNodeStats& one = rig->soak.nodeStats(1);
    CHECK(one.stepsOk == one.steps && one.steps >= 8);
    CHECK(one.earlyReplies == one.stepsOk);
    CHECK(one.retireMaxUs >= 1000);
    CHECK(!one.stopped && !rig->services.unresolved(0));
    rig->linesContiguous();
}
void testIdentityAndMonitorsPlan() {
    std::unique_ptr<Rig> rig(new Rig);
    rig->driver.values[0][3] = 0x11;
    CHECK(rig->soak.begin(plan(1, 3600, 20, true, true), rig->console.now) == E::SoakAdmission::ACCEPTED);
    rig->turns(2000);
    bool sawIdentity[2] = {}, sawMonitors = false;
    unsigned identities[2] = {};
    for (const Sent& item : rig->driver.sent) {
        if (item.index == 0x1000) {
            sawIdentity[item.node - 1] = true;
            ++identities[item.node - 1];
        }
        sawMonitors |= item.index == 0x2102;
    }
    CHECK(sawIdentity[0] && sawIdentity[1] && sawMonitors);
    CHECK(identities[0] == 1 && identities[1] == 1); // Once per soak, not a poll.
    CHECK(rig->has("sig=1009 from=0 to=c first=1"));
    CHECK(rig->has("sig=100a from=0 to=20 first=1"));
    CHECK(rig->has("sig=1000 from=0 to=40192 first=1"));
    CHECK(rig->has("sig=2100 from=0 to=100 first=1"));
    CHECK(rig->has("sig=2101 from=0 to=11 first=1"));
    CHECK(rig->soak.nodeStats(1).stepsOk == rig->soak.nodeStats(1).steps);
    rig->linesContiguous();
}
void testValueChange() {
    std::unique_ptr<Rig> rig(new Rig);
    CHECK(rig->soak.begin(plan(), rig->console.now) == E::SoakAdmission::ACCEPTED);
    rig->turns(500);
    CHECK(!rig->has("SOAK_ANOM"));
    rig->driver.values[1][0] = 0x8237;
    rig->turns(2000);
    CHECK(rig->has("node=2 sig=6041 from=8037 to=8237 first=0"));
    CHECK(rig->has("kind=value node=2 dump=1"));
    CHECK(rig->has("SOAK_T "));
    CHECK(rig->has("abandoned=0"));
    CHECK(rig->count("sig=6041") == 3); // Two first observations and one change.
    rig->linesContiguous();
}
void testAbortRetainedNotRetried() {
    std::unique_ptr<Rig> rig(new Rig);
    rig->driver.abortIndex = 0x2102;
    CHECK(rig->soak.begin(plan(1, 3600, 20, true, false), rig->console.now) == E::SoakAdmission::ACCEPTED);
    rig->turns(5000);
    CHECK(rig->has("local=0 step=3 object=2102:00 outcome=1 "));
    CHECK(rig->has("abort=06020000"));
    const E::SoakNodeStats& one = rig->soak.nodeStats(1);
    CHECK(one.outcomes[1] >= 1);
    CHECK(one.abortCodes == 1 && one.abortCode[0] == 0x06020000 && one.abortCount[0] == one.outcomes[1]);
    // A clean exchange's abort leaves the route open, and each repeat is a new scheduled operation.
    CHECK(!one.stopped);
    CHECK(one.operationsOk >= 1 && one.operationsFailed >= 1);
    unsigned aborted = 0;
    for (const Sent& item : rig->driver.sent)
        aborted += item.index == 0x2102 && item.node == 1;
    CHECK(aborted == one.outcomes[1]); // One frame per failed operation: nothing resent.
    rig->linesContiguous();
}
void testTimeoutStopsOnlyThatNode() {
    std::unique_ptr<Rig> rig(new Rig);
    CHECK(rig->soak.begin(plan(), rig->console.now) == E::SoakAdmission::ACCEPTED);
    rig->turns(300);
    rig->driver.silent[1] = true;
    const size_t before = rig->driver.sent.size();
    rig->turns(2000);
    CHECK(rig->has("SOAK_NODE_STOP"));
    CHECK(rig->has("node=2 reason=unresolved"));
    CHECK(rig->has("outcome=3 "));
    CHECK(rig->has("node=2 sig=unresolved from=0 to=1 first=0"));
    CHECK(rig->soak.nodeStats(2).stopped && !rig->soak.nodeStats(1).stopped);
    size_t lastNode2 = 0, node1After = 0;
    for (size_t i = before; i < rig->driver.sent.size(); ++i)
        if (rig->driver.sent[i].node == 2)
            lastNode2 = i;
    for (size_t i = lastNode2 + 1; i < rig->driver.sent.size(); ++i) {
        CHECK(rig->driver.sent[i].node == 1); // Node 2 is never touched again.
        ++node1After;
    }
    CHECK(node1After >= 8);
    CHECK(rig->services.unresolved(1)); // Nothing reopened it.
    CHECK(rig->firmware.owner().stats().recoveries == 0);
    rig->linesContiguous();
}
void testBootAndEmcy() {
    std::unique_ptr<Rig> rig(new Rig);
    CHECK(rig->soak.begin(plan(), rig->console.now) == E::SoakAdmission::ACCEPTED);
    rig->turns(500);
    M::Frame emcy = frame(0x82, 8);
    const uint8_t bytes[8] = {0x10, 0x23, 0x01, 0xaa, 0xbb, 0xcc, 0xdd, 0xee};
    std::memcpy(emcy.data, bytes, 8);
    rig->driver.push(emcy);
    rig->turns(1500);
    CHECK(rig->has("SOAK_EMCY"));
    CHECK(rig->has("node=2 dlc=8 data=102301aabbccddee disposition=0"));
    CHECK(rig->has("kind=emcy node=2 dump=1"));
    CHECK(!rig->soak.nodeStats(2).stopped); // An emergency is evidence, not a route event.
    M::Frame boot = frame(0x701, 1);
    rig->driver.push(boot);
    rig->turns(2000);
    CHECK(rig->has("SOAK_BOOT"));
    CHECK(rig->has("kind=boot node=1"));
    CHECK(rig->has("node=1 sig=nodeEpoch from=1 to=2 first=0"));
    CHECK(rig->has("node=1 sig=serverSessions from=0 to=1 first=0"));
    CHECK(rig->has("node=1 reason=unresolved"));
    CHECK(rig->soak.nodeStats(1).stopped && !rig->soak.nodeStats(2).stopped);
    rig->linesContiguous();
}
void testHeartbeatGapsAndNmt() {
    std::unique_ptr<Rig> rig(new Rig);
    CHECK(rig->soak.begin(plan(), rig->console.now) == E::SoakAdmission::ACCEPTED);
    rig->turns(100);
    M::Frame beat = frame(0x701, 1);
    beat.data[0] = 0x7f;
    for (unsigned i = 0; i < 3; ++i) {
        rig->driver.push(beat);
        rig->turns(i == 1 ? 700 : 300);
    }
    CHECK(rig->soak.nodeStats(1).heartbeats == 3);
    CHECK(rig->soak.nodeStats(1).heartbeatGapMaxUs >= 700000 && rig->soak.nodeStats(1).heartbeatGapMaxUs < 720000);
    CHECK(rig->has("node=1 sig=nmt from=ff to=7f first=0"));
    rig->linesContiguous();
}
// 2026-11-06: a soak begun 40 s after the previous one reported its first heartbeat gap as 38 to
// 39 s, because the owner's observation copies from between the soaks were still queued.
void testObservationsBeforeBeginBelongToNoSoak() {
    std::unique_ptr<Rig> rig(new Rig);
    rig->turns(2000); // Past the route boundary, so the heartbeat below is accepted, as on the bench.
    M::Frame beat = frame(0x701, 1);
    beat.data[0] = 0x7f;
    rig->driver.push(beat);
    rig->turns(100);
    M::Frame emcy = frame(0x81, 8); // An EMCY before the soak is not one of its events.
    emcy.data[0] = 0x10;
    rig->driver.push(emcy);
    rig->turns(5000);
    CHECK(rig->soak.begin(plan(), rig->console.now) == E::SoakAdmission::ACCEPTED);
    rig->turns(100);
    for (unsigned i = 0; i < 2; ++i) {
        rig->driver.push(beat);
        rig->turns(300);
    }
    rig->turns(3000);
    CHECK(rig->soak.nodeStats(1).heartbeats == 2);
    CHECK(rig->soak.nodeStats(1).heartbeatGapMaxUs >= 300000 && rig->soak.nodeStats(1).heartbeatGapMaxUs < 320000);
    CHECK(!rig->has("SOAK_EMCY") && !rig->has("kind=emcy"));
    CHECK(rig->has(" stale=2 "));
    rig->linesContiguous();
}
void testUnmatchedFrame() {
    std::unique_ptr<Rig> rig(new Rig);
    CHECK(rig->soak.begin(plan(), rig->console.now) == E::SoakAdmission::ACCEPTED);
    rig->turns(500);
    rig->driver.push(frame(0x123, 2));
    rig->turns(1500);
    CHECK(rig->has("sig=unmatched from=0 to=1 first=0"));
    CHECK(rig->has("kind=unmatched node=0 dump=1 a=123"));
    CHECK(rig->has("id=123 "));
    rig->linesContiguous();
}
void testDumpBudget() {
    std::unique_ptr<Rig> rig(new Rig);
    CHECK(rig->soak.begin(plan(), rig->console.now) == E::SoakAdmission::ACCEPTED);
    rig->turns(100);
    for (unsigned i = 0; i < 6; ++i) {
        M::Frame emcy = frame(0x81, 8);
        emcy.data[0] = uint8_t(i);
        rig->driver.push(emcy);
        rig->turns(4000);
    }
    CHECK(rig->count("kind=emcy") == 6);        // Each one is recorded.
    CHECK(rig->count("kind=emcy node=1 dump=1") == E::SOAK_DUMPS_PER_KIND); // The first ones in full.
    CHECK(rig->soak.pressure().dumpsSkipped == 6 - E::SOAK_DUMPS_PER_KIND);
    CHECK(rig->soak.pressure().dumps == E::SOAK_DUMPS_PER_KIND);
    CHECK(rig->soak.pressure().dumpUs > 0);
    rig->linesContiguous();
}
void testConsoleBackpressure() {
    std::unique_ptr<Rig> rig(new Rig);
    CHECK(rig->soak.begin(plan(), rig->console.now) == E::SoakAdmission::ACCEPTED);
    rig->turns(300);
    rig->console.blocked = true;
    const size_t before = rig->driver.sent.size();
    const uint64_t rxBefore = rig->firmware.owner().stats().rxFrames;
    // Records pile up against a stalled console while the reads keep to their schedule.
    M::Frame emcy = frame(0x81, 8);
    rig->driver.push(emcy);
    rig->driver.values[0][0] = 0x8237;
    for (unsigned i = 0; i < 400; ++i) {
        rig->driver.values[0][1] = i & 0xff; // Value records well past the rate limit.
        rig->turns(50);
    }
    const E::SoakPressure& pressure = rig->soak.pressure();
    CHECK(rig->driver.sent.size() > before + 500); // Service never waited for the console.
    CHECK(rig->firmware.owner().stats().rxFrames > rxBefore + 500);
    CHECK(rig->firmware.owner().stats().cancelled == 0); // Output pressure cancelled nothing.
    CHECK(pressure.blockedTurns > 1000);
    CHECK(pressure.eventsLimited + pressure.eventsFull > 0); // Counted, not hidden.
    CHECK(pressure.dumpsAbandoned >= 1 && pressure.traceLost > 0); // The deadline freed the reads.
    CHECK(pressure.deferDump > 0);
    CHECK(rig->soak.nodeStats(1).stepsOk == rig->soak.nodeStats(1).steps);
    CHECK(rig->firmware.platform().stats().outputRejected == 0); // Room is measured first.
    rig->console.blocked = false;
    const size_t resumed = rig->console.output.size();
    rig->turns(6000);
    CHECK(rig->console.output.size() > resumed);
    CHECK(rig->has("abandoned=1"));
    CHECK(rig->has("sig=6041 from=8037 to=8237") || pressure.eventsFull + pressure.eventsLimited > 0);
    rig->linesContiguous();
}
void testHeldOutputLetsAResponseThrough() {
    std::unique_ptr<Rig> rig(new Rig);
    CHECK(rig->soak.begin(plan(), rig->console.now) == E::SoakAdmission::ACCEPTED);
    rig->turns(10); // The first observations are waiting to print.
    const uint32_t lines = rig->soak.pressure().lines;
    for (unsigned i = 0; i < 100; ++i) {
        rig->console.now += 1000;
        rig->driver.deliver();
        rig->firmware.service();
        rig->soak.service(rig->console.now, true);
    }
    CHECK(rig->soak.pressure().lines == lines);
    CHECK(rig->firmware.platform().stats().outputUsed == 0); // A 256-byte line now fits.
    CHECK(rig->soak.nodeStats(1).operations > 0);           // Reads did not wait.
    rig->turns(100);
    CHECK(rig->soak.pressure().lines > lines);
    rig->linesContiguous();
}
void testStopSettlesStartedRead() {
    std::unique_ptr<Rig> rig(new Rig);
    CHECK(!rig->soak.requestStop());
    CHECK(rig->soak.begin(plan(), rig->console.now) == E::SoakAdmission::ACCEPTED);
    // Run to the middle of an operation.
    size_t started = rig->driver.sent.size();
    while (rig->driver.sent.size() == started)
        rig->turns(1);
    CHECK(rig->soak.requestStop());
    CHECK(rig->soak.active());
    rig->turns(3000);
    CHECK(!rig->soak.active());
    CHECK(rig->soak.ended() == E::SoakEnd::STOPPED);
    CHECK(rig->has("SOAK_END"));
    CHECK(rig->has("reason=stopped"));
    CHECK(rig->has("settled=1"));
    CHECK(rig->driver.sent.size() - started == 4); // The started operation finished; no other began.
    CHECK(rig->firmware.owner().stats().cancelled == 0);
    CHECK(rig->firmware.app().readContext().state == M::ReadState::EMPTY);
    CHECK(rig->firmware.owner().resources().reservations == 0);
    CHECK(rig->has("SOAK_SUM"));
    CHECK(rig->has("stack=4321 heap=70000 heapMin=65000"));
    rig->linesContiguous();
}
void testDurationAndTrialIdentity() {
    std::unique_ptr<Rig> rig(new Rig);
    E::SoakPlan shortPlan = plan(5, 2);
    shortPlan.summaryS = 1;
    CHECK(rig->soak.begin(shortPlan, rig->console.now) == E::SoakAdmission::ACCEPTED);
    rig->turns(1500);
    CHECK(rig->count("SOAK_SUM") == 1);
    CHECK(rig->samples >= 2);
    rig->turns(4000);
    CHECK(!rig->soak.active());
    CHECK(rig->soak.ended() == E::SoakEnd::DURATION);
    CHECK(rig->has("reason=duration"));
    const size_t after = rig->driver.sent.size();
    rig->turns(1000);
    CHECK(rig->driver.sent.size() == after); // Finite: nothing runs past the stated duration.
    // A repeated identity is refused; a later trial is explicit and new.
    CHECK(rig->soak.begin(plan(5), rig->console.now) == E::SoakAdmission::INVALID);
    CHECK(rig->soak.begin(plan(6), rig->console.now) == E::SoakAdmission::ACCEPTED);
    E::SoakPlan invalid = plan(7);
    invalid.intervalMs = 5;
    CHECK(rig->soak.begin(invalid, rig->console.now) == E::SoakAdmission::BUSY);
}
void testRefusalsLeaveNothing() {
    std::unique_ptr<Rig> rig(new Rig);
    E::SoakPlan invalid = plan();
    invalid.intervalMs = 5;
    CHECK(rig->soak.begin(invalid, rig->console.now) == E::SoakAdmission::INVALID);
    invalid = plan();
    invalid.trial = 0;
    CHECK(rig->soak.begin(invalid, rig->console.now) == E::SoakAdmission::INVALID);
    invalid = plan();
    invalid.durationS = 0;
    CHECK(rig->soak.begin(invalid, rig->console.now) == E::SoakAdmission::INVALID);
    CHECK(!rig->soak.active());
    rig->turns(50);
    CHECK(rig->driver.sent.empty() && rig->console.output.empty());
}
void testBusOffNoRecovery() {
    std::unique_ptr<Rig> rig(new Rig);
    CHECK(rig->soak.begin(plan(), rig->console.now) == E::SoakAdmission::ACCEPTED);
    rig->turns(500);
    rig->driver.pending.busOff = true;
    rig->driver.pending.busErrors = 7;
    rig->driver.pending.errorFlags = 0x40;
    rig->turns(100);
    const size_t after = rig->driver.sent.size();
    rig->turns(3000);
    CHECK(rig->driver.sent.size() <= after + 1);
    CHECK(rig->has("sig=controller from=0 to=2 first=0"));
    CHECK(rig->has("sig=inhibited from=0 to=1 first=0"));
    CHECK(rig->has("kind=controller"));
    CHECK(rig->has("sig=twaiErrors from=0 to=7 first=0"));
    CHECK(rig->has("sig=twaiFlags from=0 to=40 first=0"));
    CHECK(rig->soak.pressure().deferInhibited > 0 || rig->soak.nodeStats(1).stopped);
    CHECK(rig->firmware.owner().stats().recoveries == 0);
    CHECK(!rig->firmware.owner().recoveryActive());
    CHECK(rig->soak.active()); // Still observing to its stated end.
    rig->linesContiguous();
}
// A lasting bus fault, as the missing ACK of 2026-11-03: the error counter changes on every pass.
// One line per change once kept the event queue full, so no summary printed for 3 h and the first
// anomaly's dump was never closed.
void testLastingBusFaultKeepsSummaries() {
    std::unique_ptr<Rig> rig(new Rig);
    E::SoakPlan p = plan();
    p.summaryS = 1;
    CHECK(rig->soak.begin(p, rig->console.now) == E::SoakAdmission::ACCEPTED);
    rig->turns(1500);
    const unsigned summaries = rig->count("SOAK_SUM");
    for (unsigned i = 0; i < 4000; ++i) {
        rig->driver.pending.busErrors = 1;
        rig->turns(1);
    }
    rig->turns(1500); // Past the next summary.
    const unsigned windows = rig->count("SOAK_SUM") - summaries;
    CHECK(windows >= 4);
    const unsigned bus = rig->count("kind=bus");
    CHECK(bus >= 2 && bus <= windows + 1); // The first change, then at most one per window.
    CHECK(rig->has("sig=twaiErrors from=0 to=1 first=0"));
    CHECK(rig->has("twaiErr=4000 "));      // The summary still counts every error.
    CHECK(rig->count(" dump=1 ") == rig->count("SOAK_TEND"));
    CHECK(rig->soak.pressure().eventsFull == 0);
    CHECK(rig->soak.nodeStats(1).stepsOk == rig->soak.nodeStats(1).steps);
    rig->linesContiguous();
}
// A scenario with every record kind, for the parser's grammar check in s2_soak_test.py.
void writeTranscript(const char* path) {
    std::unique_ptr<Rig> rig(new Rig);
    E::SoakPlan full = plan(9, 30, 20, true, true);
    full.summaryS = 2;
    CHECK(rig->soak.begin(full, rig->console.now) == E::SoakAdmission::ACCEPTED);
    rig->turns(1000);
    rig->driver.values[0][0] = 0x8237;
    rig->turns(2000);
    rig->driver.abortIndex = 0x2102;
    rig->turns(2000);
    rig->driver.abortIndex = 0;
    M::Frame emcy = frame(0x82, 8);
    emcy.data[0] = 0x10;
    rig->driver.push(emcy);
    rig->turns(2000);
    rig->driver.push(frame(0x123, 2));
    rig->turns(2000);
    M::Frame beat = frame(0x702, 1);
    beat.data[0] = 0x7f;
    rig->driver.push(beat);
    rig->turns(1000);
    rig->driver.silent[1] = true;
    rig->turns(3000);
    rig->driver.push(frame(0x701, 1));
    rig->turns(3000);
    rig->console.blocked = true;
    rig->driver.push(emcy);
    rig->turns(6000);
    rig->console.blocked = false;
    CHECK(rig->soak.requestStop());
    rig->turns(6000);
    CHECK(!rig->soak.active());
    rig->linesContiguous();
    std::FILE* file = std::fopen(path, "wb");
    CHECK(file);
    CHECK(std::fwrite(rig->console.output.data(), 1, rig->console.output.size(), file) ==
          rig->console.output.size());
    std::fclose(file);
}
} // namespace

int main(int argc, char** argv) {
    if (argc == 3 && std::strcmp(argv[1], "--transcript") == 0) {
        writeTranscript(argv[2]);
        return 0;
    }
    testParse();
    testRefusalsLeaveNothing();
    testPassiveAndAlternating();
    testReplyBeforeTxReport();
    testIdentityAndMonitorsPlan();
    testValueChange();
    testAbortRetainedNotRetried();
    testTimeoutStopsOnlyThatNode();
    testBootAndEmcy();
    testHeartbeatGapsAndNmt();
    testObservationsBeforeBeginBelongToNoSoak();
    testUnmatchedFrame();
    testDumpBudget();
    testConsoleBackpressure();
    testHeldOutputLetsAResponseThrough();
    testStopSettlesStartedRead();
    testDurationAndTrialIdentity();
    testBusOffNoRecovery();
    testLastingBusFaultKeepsSummaries();
    std::printf("soak: %u checks\n", checks);
    return 0;
}
