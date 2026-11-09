// SPDX-License-Identifier: MIT
// The S2 bench's D line over the real ProbeApp, owner and service store, with a synthetic wire.
// The firmware turn is ProbeApp::service, as in FirmwareProbe; the bench record follows it.
#include "DiscoveryBench.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace M = MotorControlCAN;
namespace C = MotorControlCAN::CANOPEN;
namespace L = MotorControlCAN::LICHUAN_C;
namespace E = MotorControlCANExample;
namespace O = MotorControlCANOwner;
namespace {
unsigned checks = 0;
#define CHECK(condition)                                                                           \
    do {                                                                                           \
        ++checks;                                                                                  \
        if (!(condition)) {                                                                        \
            std::fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #condition);                   \
            std::abort();                                                                          \
        }                                                                                          \
    } while (false)
} // namespace
#include "support/OwnerRig.h"
namespace {
// The bench's own route shape (main.cpp target): profile 1, epochs 1, node epoch 1.
L::ReadTarget target(unsigned node) {
    L::ReadTarget t;
    t.channel.binding.logicalAxisId = node;
    t.channel.binding.endpoint.profile = 1;
    t.channel.binding.endpoint.address = node;
    t.channel.binding.bindingGeneration = t.channel.binding.configurationGeneration = 1;
    t.channel.clientToServerId = 0x600 + node;
    t.channel.serverToClientId = 0x580 + node;
    t.nodeGeneration = 1;
    return t;
}
// One upload reply of a retained bench capture as its RESULT line prints it: object=, size= and the
// little-endian data= bytes as a raw value.
struct Captured {
    uint16_t index;
    uint8_t sub;
    uint8_t size;
    uint32_t raw;
};
// Two identity dictionaries in the specimens' retained form; node 2 can differ on request.
struct Wire : OwnerRig::WireBase {
    uint64_t now = 0;
    bool silent[3] = {};
    // While set, each upload to that node must be the table's next object, in the captured order,
    // and takes its captured reply instead of the dictionary below.
    const Captured* script[3] = {};
    unsigned scriptLength[3] = {}, scriptAt[3] = {};
    uint16_t model[3] = {0, 0x0400, 0x0400};
    unsigned uploads = 0;
    static bool transmit(void* context, const M::Frame& f, O::TransportTxId id, uint32_t epoch) {
        Wire& w = *static_cast<Wire*>(static_cast<OwnerRig::WireBase*>(context));
        // Any download, NMT, drain marker or other identifier fails the test.
        CHECK(f.id == 0x601 || f.id == 0x602);
        CHECK(f.data[0] == 0x40 && f.payloadLength == 8);
        ++w.uploads;
        CHECK(OwnerRig::WireBase::transmit(context, f, id, epoch));
        const unsigned node = f.id - 0x600;
        if (w.silent[node])
            return true;
        const uint16_t index = uint16_t(f.data[1] | f.data[2] << 8);
        const uint8_t sub = f.data[3];
        uint8_t width = 0;
        uint32_t raw = 0;
        switch (index) {
        case 0x1000:
            width = 4;
            raw = 0x00040192;
            break;
        case 0x1009:
            width = 1; // One byte, as both specimens answer (MEASURED_U8).
            raw = 0x0c;
            break;
        case 0x100a:
            width = 1;
            raw = 0x20;
            break;
        case 0x1018:
            width = sub ? 4 : 1;
            raw = sub == 0 ? 3 : sub == 1 ? 0x1000 : sub == 2 ? 1 : 0;
            break;
        case 0x2000:
            width = 2;
            raw = w.model[node];
            break;
        case 0x2001:
            width = 2;
            raw = 0x0200;
            break;
        default:
            CHECK(false);
        }
        if (w.script[node]) {
            CHECK(w.scriptAt[node] < w.scriptLength[node]);
            const Captured& c = w.script[node][w.scriptAt[node]++];
            CHECK(c.index == index && c.sub == sub);
            width = c.size;
            raw = c.raw;
        }
        M::Frame reply = OwnerRig::frame(0x580 + node, 8);
        reply.data[0] = width == 1 ? 0x4f : width == 2 ? 0x4b : 0x43;
        reply.data[1] = f.data[1];
        reply.data[2] = f.data[2];
        reply.data[3] = sub;
        for (unsigned i = 0; i < 4; ++i)
            reply.data[4 + i] = uint8_t(raw >> (8 * i));
        w.push(reply, w.now, epoch);
        return true;
    }
    O::Transport callbacks() {
        O::Transport t = plumbing();
        t.transmit = transmit;
        return t;
    }
};
struct Rig {
    C::SdoClient services;
    Wire wire;
    E::ProbePlatform platform;
    O::CanBusOwner owner;
    E::ProbeApp app;
    E::DiscoveryBench bench;
    uint64_t now = 10;
    explicit Rig(unsigned routes = 2)
        : platform(E::ProbeCallbacks()), owner(services, wire.callbacks(), 28), app(owner, platform),
          bench(app, owner, services) {
        // The bench premise of main.cpp ensureRoute: exclusive, unconfirmed, network route bound.
        for (unsigned node = 1; node <= routes; ++node) {
            const L::ReadTarget t = target(node);
            CHECK(services.configure(uint8_t(node - 1), t.channel, false, true));
            C::NetworkRoute route;
            route.binding = t.channel.binding;
            route.generation = 1;
            route.heartbeatId = 0x700 + node;
            route.emcyId = 0x80 + node;
            CHECK(services.configureNetwork(uint8_t(node - 1), route, C::NetworkObserver(), 0));
        }
    }
    E::DiscoveryBenchRequest request(E::DiscoveryMode mode, uint32_t id = 1, uint8_t select = 0,
                                     bool expect = false, uint32_t seconds = 2) const {
        E::DiscoveryBenchRequest r;
        r.id = id;
        r.mode = mode;
        r.seconds = seconds;
        r.select = select;
        r.expect = expect;
        r.bitrate = 1000000;
        return r;
    }
    // One firmware turn (FirmwareProbe::service runs ProbeApp::service) and then the bench.
    void turn(uint64_t step = 100) {
        now += step;
        wire.now = now;
        CHECK(app.service(now));
        CHECK(bench.service(owner.lastTimeUs()));
    }
    void finish(unsigned limit = 40000) {
        for (unsigned i = 0; i < limit && bench.active(); ++i)
            turn();
        CHECK(bench.record() && bench.record()->terminal);
    }
    void heartbeat(unsigned node, uint8_t state) {
        M::Frame f = OwnerRig::frame(0x700 + node, 1);
        f.data[0] = state;
        wire.push(f, now);
    }
    // Every record line, in order; each must fit and parse as a whole line.
    unsigned lines(char (*out)[E::DISCOVERY_BENCH_LINE], unsigned capacity) const {
        unsigned n = 0;
        while (n < capacity && bench.formatLine(n, out[n], E::DISCOVERY_BENCH_LINE)) {
            CHECK(std::strstr(out[n], "OUTPUT_FORMAT_ERROR") == nullptr);
            CHECK(std::strlen(out[n]) > 0 && out[n][std::strlen(out[n]) - 1] == '\n');
            ++n;
        }
        return n;
    }
};
// A refusal word, null-safe: an admitted request (null) is never a refusal.
bool refused(const char* got, const char* word) {
    return got && std::strcmp(got, word) == 0;
}
bool starts(const char* line, const char* prefix) {
    return std::strncmp(line, prefix, std::strlen(prefix)) == 0;
}

void admissionRefusals() {
    Rig rig;
    // Invalid requests change nothing and send nothing.
    E::DiscoveryBenchRequest bad = rig.request(E::DiscoveryMode::PROBE);
    bad.id = 0;
    CHECK(refused(rig.bench.begin(bad, rig.now), "invalid"));
    bad = rig.request(E::DiscoveryMode::PROBE);
    bad.seconds = 0;
    CHECK(refused(rig.bench.begin(bad, rig.now), "invalid"));
    bad.seconds = 61;
    CHECK(refused(rig.bench.begin(bad, rig.now), "invalid"));
    bad = rig.request(E::DiscoveryMode::PROBE, 1, 3);
    CHECK(refused(rig.bench.begin(bad, rig.now), "invalid"));
    bad = rig.request(E::DiscoveryMode::PROBE, 1, 0, true); // An expectation is identity only.
    CHECK(refused(rig.bench.begin(bad, rig.now), "invalid"));
    bad = rig.request(E::DiscoveryMode::IDENTITY);
    bad.bitrate = 0;
    CHECK(refused(rig.bench.begin(bad, rig.now), "invalid"));
    CHECK(!rig.bench.record() && !rig.wire.sentCount);
    // A busy app read refuses the scan itself.
    CHECK(rig.app.select(target(1)));
    L::ReadOptions o;
    o.queueTimeoutUs = o.responseTimeoutUs = 250000;
    CHECK(rig.app.requestProbe(rig.now, rig.now + 1000000, o));
    CHECK(refused(rig.bench.begin(rig.request(E::DiscoveryMode::PROBE), rig.now), "scan"));
    CHECK(!rig.bench.record());
    // Only one configured slot: the fixed two-node plan refuses as a route fact.
    Rig one(1);
    CHECK(refused(one.bench.begin(one.request(E::DiscoveryMode::PASSIVE), one.now), "route"));
    // Admitted once; a second request while held is busy, and an older identity stays invalid later.
    Rig held;
    CHECK(held.bench.begin(held.request(E::DiscoveryMode::PROBE, 5), held.now) == nullptr);
    CHECK(refused(held.bench.begin(held.request(E::DiscoveryMode::PROBE, 6), held.now), "busy"));
    CHECK(!held.bench.release()); // Not while the scan runs.
    held.finish();
    CHECK(held.bench.release() && !held.bench.record());
    CHECK(refused(held.bench.begin(held.request(E::DiscoveryMode::PROBE, 5), held.now), "invalid"));
}

// The bench gives the scan a zero owner budget: without the firmware turn nothing is admitted.
void benchAddsNoOwnerWork() {
    Rig rig;
    CHECK(rig.bench.begin(rig.request(E::DiscoveryMode::PROBE), rig.now) == nullptr);
    for (unsigned i = 0; i < 50; ++i) {
        rig.now += 100;
        CHECK(rig.bench.service(rig.now));
    }
    CHECK(!rig.wire.sentCount && rig.bench.active());
    rig.finish();
    CHECK(rig.wire.uploads == 2);
}

void passiveObservation() {
    Rig rig;
    CHECK(rig.bench.begin(rig.request(E::DiscoveryMode::PASSIVE, 1, 1), rig.now) == nullptr);
    rig.turn();
    rig.heartbeat(1, 0x7f); // Node 2 stays silent: heartbeat disabled is a serviceable device too.
    rig.finish();
    CHECK(!rig.wire.sentCount);
    const E::DiscoveryFinding* first = rig.bench.scan().result(0);
    const E::DiscoveryFinding* second = rig.bench.scan().result(1);
    CHECK(first && first->outcome == E::DiscoveryOutcome::PRESENCE && first->heartbeat.present);
    CHECK(second && second->outcome == E::DiscoveryOutcome::NO_RESPONSE);
    const E::DiscoveryBenchRecord& r = *rig.bench.record();
    CHECK(r.admitted == 0 && r.terminalCount == 0 && rig.bench.scan().summary().deadlineExpired);
    // Host selection afterwards: cached facts only, nothing sent.
    CHECK(r.selection.attempted && r.selection.selected && r.selection.bindingCurrent);
    CHECK(r.selection.txBefore == r.selection.txAfter && rig.app.selected());
    char out[16][E::DISCOVERY_BENCH_LINE];
    const unsigned n = rig.lines(out, 16);
    CHECK(n == 5);
    CHECK(starts(out[0], "DISC id=1 mode=0 seconds=2 targets=2 attempted=2 completed=2 admitted=0 "
                         "terminals=0"));
    CHECK(starts(out[1], "DISC_T node=1 slot=0 outcome=1 confidence=1 ") &&
          std::strstr(out[1], " hb=1 ") && std::strstr(out[1], " current=1 "));
    CHECK(starts(out[2], "DISC_T node=2 slot=1 outcome=3 confidence=0 ") &&
          std::strstr(out[2], " hb=0 "));
    CHECK(starts(out[3], "DISC_SELECT node=1 attempted=1 selected=1 current=1 tx0=0 tx1=0"));
    CHECK(starts(out[4], "END discovery terminals=0 targets=2;"));
}

void probeAndSelection() {
    Rig rig;
    CHECK(rig.bench.begin(rig.request(E::DiscoveryMode::PROBE, 3, 2), rig.now) == nullptr);
    rig.finish();
    CHECK(rig.wire.uploads == 2 && rig.wire.sentCount == 2);
    for (unsigned i = 0; i < 2; ++i) {
        // The exact minimal probe frame, 1000:00 upload, on each node's own route.
        CHECK(rig.wire.sent[i].id == 0x601 + i && rig.wire.sent[i].data[1] == 0x00 &&
              rig.wire.sent[i].data[2] == 0x10 && rig.wire.sent[i].data[3] == 0);
        const E::DiscoveryFinding* f = rig.bench.scan().result(uint8_t(i));
        CHECK(f->outcome == E::DiscoveryOutcome::PRESENCE &&
              f->confidence == E::DiscoveryConfidence::RESPONDER_ONLY && f->bindingCurrent);
    }
    const E::DiscoveryBenchRecord& r = *rig.bench.record();
    CHECK(r.admitted == 2 && r.terminalCount == 2 && !r.terminalsLost);
    CHECK(r.selection.node == 2 && r.selection.selected);
    CHECK(rig.app.selected()->channel.binding.endpoint.address == 2);
    char out[16][E::DISCOVERY_BENCH_LINE];
    const unsigned n = rig.lines(out, 16);
    CHECK(n == 9);
    CHECK(starts(out[3], "DISC_PROBE node=1 type=00040192"));
    CHECK(starts(out[4], "DISC_PROBE node=2 type=00040192"));
    CHECK(starts(out[5], "DISC_SELECT node=2 attempted=1 selected=1 current=1"));
    for (unsigned i = 6; i < 8; ++i)
        CHECK(starts(out[i], "RESULT op=") && std::strstr(out[i], " object=1000:00 dir=0 outcome=0 ") &&
              std::strstr(out[i], " data=92010400 "));
    CHECK(starts(out[8], "END discovery terminals=2 targets=2;"));
}

void identityExpectations() {
    Rig rig;
    rig.wire.model[2] = 0x0401; // Node 2 contradicts the retained fixture identity.
    CHECK(rig.bench.begin(rig.request(E::DiscoveryMode::IDENTITY, 7, 0, true, 5), rig.now) == nullptr);
    rig.finish();
    CHECK(rig.wire.uploads == 18);
    const E::DiscoveryFinding* first = rig.bench.scan().result(0);
    const E::DiscoveryFinding* second = rig.bench.scan().result(1);
    CHECK(first->outcome == E::DiscoveryOutcome::GENERIC_IDENTITY &&
          first->confidence == E::DiscoveryConfidence::EXPECTATION_MATCH);
    CHECK(second->outcome == E::DiscoveryOutcome::CONTRADICTORY_IDENTITY &&
          second->confidence == E::DiscoveryConfidence::RAW_IDENTITY);
    CHECK(rig.bench.record()->terminalCount == 18 && rig.bench.record()->admitted == 18);
    CHECK(!rig.bench.record()->selection.attempted);
    char out[32][E::DISCOVERY_BENCH_LINE];
    const unsigned n = rig.lines(out, 32);
    CHECK(n == 1 + 2 + 2 + 1 + 18 + 1);
    CHECK(starts(out[3], "DISC_ID node=1 type=00040192 hw=c hww=1 sw=20 sww=1 entries=3 "
                         "vendor=00001000 product=00000001 revision=00000000 model=0400 "
                         "firmware=0200"));
    CHECK(starts(out[4], "DISC_ID node=2 ") && std::strstr(out[4], " model=0401 "));
    CHECK(starts(out[5], "DISC_SELECT node=0 attempted=0 selected=0"));
    // Without an expectation the same reads stay generic: no automatic model or firmware claim.
    Rig plain;
    CHECK(plain.bench.begin(plain.request(E::DiscoveryMode::IDENTITY, 1, 1), plain.now) == nullptr);
    plain.finish();
    CHECK(plain.bench.scan().result(0)->confidence == E::DiscoveryConfidence::RAW_IDENTITY);
    CHECK(plain.bench.scan().result(1)->outcome == E::DiscoveryOutcome::GENERIC_IDENTITY);
}

// A probe that gets no reply: finite, no retry, its route left unresolved and its uncertainty kept;
// the other node is still probed once, and its selection reflects only cached facts.
void silentNodeKeepsUncertainty() {
    Rig rig;
    rig.wire.silent[1] = true;
    CHECK(rig.bench.begin(rig.request(E::DiscoveryMode::PROBE, 1, 1), rig.now) == nullptr);
    rig.finish();
    CHECK(rig.wire.uploads == 2); // One probe each; the silent one is never repeated.
    const E::DiscoveryFinding* first = rig.bench.scan().result(0);
    CHECK(first->outcome == E::DiscoveryOutcome::NO_RESPONSE && first->unresolved &&
          first->read.uncertain);
    CHECK(rig.bench.scan().result(1)->outcome == E::DiscoveryOutcome::PRESENCE);
    CHECK(rig.bench.scan().summary().unresolvedSlots == 1);
    char out[16][E::DISCOVERY_BENCH_LINE];
    const unsigned n = rig.lines(out, 16);
    CHECK(n == 8); // Node 2's probe value line, then both retained terminals.
    CHECK(std::strstr(out[1], " outcome=3 ") && std::strstr(out[1], " uncertain=1 ") &&
          std::strstr(out[1], " unresolved=1 "));
    CHECK(std::strstr(out[5], " outcome=3 ")); // The retained timeout terminal of node 1.
    // Release keeps the quarantine: a new probe scan refuses the unresolved route.
    CHECK(rig.bench.release());
    CHECK(refused(rig.bench.begin(rig.request(E::DiscoveryMode::PROBE, 2), rig.now), "scan"));
}

// ------------------------------------------------------- 2026-11-09 capture, replayed literally
// A produced line against a retained captured line, token by token. A token whose key is listed in
// `skip` is compared by key only: S2 microseconds (completed, admitted, gap), the owner's request
// and TX counters of that boot (oreq, tx0, tx1) and the app's operation numbers (op, req; the
// readiness routes 901 and 902 had used the first six of that boot). Every other token, every value
// included, must be identical, and so must the token count.
bool listed(const char* list, const char* key, size_t size) {
    for (const char* at = list; *at;) {
        while (*at == ' ')
            ++at;
        const char* end = at;
        while (*end && *end != ' ')
            ++end;
        if (size_t(end - at) == size && !std::strncmp(at, key, size))
            return true;
        at = end;
    }
    return false;
}
bool sameLine(const char* got, const char* captured, const char* skip) {
    const char* a = got;
    const char* b = captured;
    for (;;) {
        while (*a == ' ')
            ++a;
        while (*b == ' ')
            ++b;
        const bool doneA = !*a || *a == '\n', doneB = !*b || *b == '\n';
        if (doneA || doneB) {
            if (doneA && doneB)
                return true;
            break;
        }
        const char* endA = a;
        while (*endA && *endA != ' ' && *endA != '\n')
            ++endA;
        const char* endB = b;
        while (*endB && *endB != ' ' && *endB != '\n')
            ++endB;
        const char* keyA = a;
        while (keyA < endA && *keyA != '=')
            ++keyA;
        const char* keyB = b;
        while (keyB < endB && *keyB != '=')
            ++keyB;
        const size_t key = size_t(keyA - a);
        if (key != size_t(keyB - b) || std::strncmp(a, b, key))
            break;
        if (!listed(skip, a, key) &&
            (endA - a != endB - b || std::strncmp(a, b, size_t(endA - a))))
            break;
        a = endA;
        b = endB;
    }
    std::fprintf(stderr, "produced: %s\ncaptured: %s\n", got, captured);
    return false;
}
// The skip list of each record line's tag.
const char* skipFor(const char* line) {
    return starts(line, "DISC_T ")        ? "completed"
           : starts(line, "DISC_SELECT ") ? "tx0 tx1"
           : starts(line, "RESULT ")      ? "op req oreq admitted completed"
           : starts(line, "DISC ")        ? "gap"
                                          : "";
}
// The probe and identity replies of 2026-11-09 11:19Z, from the RESULT lines of
// docs/bench/20261109T1119Z-r2-p37_discovery/rx.txt: the probe lines 16-17, the identity uploads
// of node 1 lines 27-35 and of node 2 lines 36-44. Both specimens answered 1009 and 100A in one
// byte and 1018:00 with 5, while the EDS names subindexes 1..3 only.
const Captured PROBE_REPLY[1] = {{0x1000, 0x00, 4, 0x00040192}};
const Captured IDENTITY_REPLIES[9] = {
    {0x1000, 0x00, 4, 0x00040192}, {0x1009, 0x00, 1, 0x0000000c}, {0x100a, 0x00, 1, 0x00000020},
    {0x1018, 0x00, 1, 0x00000005}, {0x1018, 0x01, 4, 0x00001000}, {0x1018, 0x02, 4, 0x00000001},
    {0x1018, 0x03, 4, 0x00000000}, {0x2000, 0x00, 2, 0x00000400}, {0x2001, 0x00, 2, 0x00000200}};
// The three D lines of that plan (Do,5,0,0, Dp,3,1,0 and Di,5,2,1) in one MCU boot (image
// 757f1cd), heartbeats disabled on both nodes (1017 = 0): the passive window finds no evidence and
// sends nothing; one accepted 1000:00 probe per node, both 00040192, then host selection of node 1;
// the nine identity uploads per node, both matching the retained 2026-11-04 identity (1018
// 1000/1/0, 2000 0400, 2001 0200), then selection of node 2. Selection sends nothing. Each record's
// lines are compared with the captured ones; the n listing afterwards (lines 47-48) has no
// observation of either node.
void replayDiscovery20261109() {
    struct Plan {
        E::DiscoveryMode mode;
        uint32_t id, seconds;
        uint8_t select;
        bool expect;
        const Captured* replies;
        unsigned replyCount;
        unsigned lineCount;
        const char* lines[25];
    };
    static const Plan plans[3] = {
        {E::DiscoveryMode::PASSIVE, 1, 5, 0, false, nullptr, 0, 5,
         {
            // line 3
            "DISC id=1 mode=0 seconds=5 targets=2 attempted=2 completed=2 admitted=0 terminals=0 "
            "occupied=2 free=0 unresolved=0 drops=0 cancelled=0 deadline=1 changed=0 failed=0 "
            "lost=0 gap=2000",
            // line 4
            "DISC_T node=1 slot=0 outcome=3 confidence=0 read=0 state=0 fields=0 uncertain=0 hb=0 "
            "hbp=0 boot=0 bootp=0 pdo=0 pdop=0 dup=0 unresolved=0 current=1 completed=70572979",
            // line 5
            "DISC_T node=2 slot=1 outcome=3 confidence=0 read=0 state=0 fields=0 uncertain=0 hb=0 "
            "hbp=0 boot=0 bootp=0 pdo=0 pdop=0 dup=0 unresolved=0 current=1 completed=70572979",
            // line 6
            "DISC_SELECT node=0 attempted=0 selected=0 current=0 tx0=0 tx1=0",
            // line 7
            "END discovery terminals=0 targets=2; selection sends nothing; r=release",
         }},
        {E::DiscoveryMode::PROBE, 2, 3, 1, false, PROBE_REPLY, 1, 9,
         {
            // line 10
            "DISC id=2 mode=1 seconds=3 targets=2 attempted=2 completed=2 admitted=2 terminals=2 "
            "occupied=2 free=0 unresolved=0 drops=0 cancelled=0 deadline=0 changed=0 failed=0 "
            "lost=0 gap=4100",
            // line 11
            "DISC_T node=1 slot=0 outcome=1 confidence=1 read=1 state=2 fields=1 uncertain=0 hb=0 "
            "hbp=0 boot=0 bootp=0 pdo=0 pdop=0 dup=0 unresolved=0 current=1 completed=70661981",
            // line 12
            "DISC_T node=2 slot=1 outcome=1 confidence=1 read=1 state=2 fields=1 uncertain=0 hb=0 "
            "hbp=0 boot=0 bootp=0 pdo=0 pdop=0 dup=0 unresolved=0 current=1 completed=70670993",
            // line 13
            "DISC_PROBE node=1 type=00040192",
            // line 14
            "DISC_PROBE node=2 type=00040192",
            // line 15
            "DISC_SELECT node=1 attempted=1 selected=1 current=1 tx0=46 tx1=46",
            // line 16
            "RESULT op=7 req=7 oreq=47 step=1 object=1000:00 dir=0 outcome=0 ack=1 possible=1 "
            "uncertain=0 abort=00000000 cleanup=0 size=4 data=92010400 admitted=70657981 "
            "completed=70661981 ne=1 reason=0 st=3f",
            // line 17
            "RESULT op=8 req=8 oreq=48 step=1 object=1000:00 dir=0 outcome=0 ack=1 possible=1 "
            "uncertain=0 abort=00000000 cleanup=0 size=4 data=92010400 admitted=70667981 "
            "completed=70670993 ne=1 reason=0 st=3f",
            // line 18
            "END discovery terminals=2 targets=2; selection sends nothing; r=release",
         }},
        {E::DiscoveryMode::IDENTITY, 3, 5, 2, true, IDENTITY_REPLIES, 9, 25,
         {
            // line 21
            "DISC id=3 mode=2 seconds=5 targets=2 attempted=2 completed=2 admitted=18 "
            "terminals=18 occupied=2 free=0 unresolved=0 drops=0 cancelled=0 deadline=0 changed=0 "
            "failed=0 lost=0 gap=4157",
            // line 22
            "DISC_T node=1 slot=0 outcome=2 confidence=3 read=1 state=2 fields=9 uncertain=0 hb=0 "
            "hbp=0 boot=0 bootp=0 pdo=0 pdop=0 dup=0 unresolved=0 current=1 completed=70892982",
            // line 23
            "DISC_T node=2 slot=1 outcome=2 confidence=3 read=1 state=2 fields=9 uncertain=0 hb=0 "
            "hbp=0 boot=0 bootp=0 pdo=0 pdop=0 dup=0 unresolved=0 current=1 completed=70974990",
            // line 24
            "DISC_ID node=1 type=00040192 hw=c hww=1 sw=20 sww=1 entries=5 vendor=00001000 "
            "product=00000001 revision=00000000 model=0400 firmware=0200",
            // line 25
            "DISC_ID node=2 type=00040192 hw=c hww=1 sw=20 sww=1 entries=5 vendor=00001000 "
            "product=00000001 revision=00000000 model=0400 firmware=0200",
            // line 26
            "DISC_SELECT node=2 attempted=1 selected=1 current=1 tx0=64 tx1=64",
            // line 27
            "RESULT op=9 req=9 oreq=49 step=1 object=1000:00 dir=0 outcome=0 ack=1 possible=1 "
            "uncertain=0 abort=00000000 cleanup=0 size=4 data=92010400 admitted=70816981 "
            "completed=70820981 ne=1 reason=0 st=3f",
            // line 28
            "RESULT op=9 req=9 oreq=50 step=2 object=1009:00 dir=0 outcome=0 ack=1 possible=1 "
            "uncertain=0 abort=00000000 cleanup=0 size=1 data=0c000000 admitted=70826981 "
            "completed=70829984 ne=1 reason=0 st=3f",
            // line 29
            "RESULT op=9 req=9 oreq=51 step=3 object=100a:00 dir=0 outcome=0 ack=1 possible=1 "
            "uncertain=0 abort=00000000 cleanup=0 size=1 data=20000000 admitted=70835981 "
            "completed=70838981 ne=1 reason=0 st=3f",
            // line 30
            "RESULT op=9 req=9 oreq=52 step=4 object=1018:00 dir=0 outcome=0 ack=1 possible=1 "
            "uncertain=0 abort=00000000 cleanup=0 size=1 data=05000000 admitted=70844981 "
            "completed=70847981 ne=1 reason=0 st=3f",
            // line 31
            "RESULT op=9 req=9 oreq=53 step=5 object=1018:01 dir=0 outcome=0 ack=1 possible=1 "
            "uncertain=0 abort=00000000 cleanup=0 size=4 data=00100000 admitted=70853981 "
            "completed=70856985 ne=1 reason=0 st=3f",
            // line 32
            "RESULT op=9 req=9 oreq=54 step=6 object=1018:02 dir=0 outcome=0 ack=1 possible=1 "
            "uncertain=0 abort=00000000 cleanup=0 size=4 data=01000000 admitted=70862981 "
            "completed=70865981 ne=1 reason=0 st=3f",
            // line 33
            "RESULT op=9 req=9 oreq=55 step=7 object=1018:03 dir=0 outcome=0 ack=1 possible=1 "
            "uncertain=0 abort=00000000 cleanup=0 size=4 data=00000000 admitted=70871981 "
            "completed=70874981 ne=1 reason=0 st=3f",
            // line 34
            "RESULT op=9 req=9 oreq=56 step=8 object=2000:00 dir=0 outcome=0 ack=1 possible=1 "
            "uncertain=0 abort=00000000 cleanup=0 size=2 data=00040000 admitted=70880981 "
            "completed=70883983 ne=1 reason=0 st=3f",
            // line 35
            "RESULT op=9 req=9 oreq=57 step=9 object=2001:00 dir=0 outcome=0 ack=1 possible=1 "
            "uncertain=0 abort=00000000 cleanup=0 size=2 data=00020000 admitted=70889981 "
            "completed=70892982 ne=1 reason=0 st=3f",
            // line 36
            "RESULT op=10 req=10 oreq=58 step=1 object=1000:00 dir=0 outcome=0 ack=1 possible=1 "
            "uncertain=0 abort=00000000 cleanup=0 size=4 data=92010400 admitted=70899981 "
            "completed=70902990 ne=1 reason=0 st=3f",
            // line 37
            "RESULT op=10 req=10 oreq=59 step=2 object=1009:00 dir=0 outcome=0 ack=1 possible=1 "
            "uncertain=0 abort=00000000 cleanup=0 size=1 data=0c000000 admitted=70908981 "
            "completed=70911982 ne=1 reason=0 st=3f",
            // line 38
            "RESULT op=10 req=10 oreq=60 step=3 object=100a:00 dir=0 outcome=0 ack=1 possible=1 "
            "uncertain=0 abort=00000000 cleanup=0 size=1 data=20000000 admitted=70917981 "
            "completed=70920983 ne=1 reason=0 st=3f",
            // line 39
            "RESULT op=10 req=10 oreq=61 step=4 object=1018:00 dir=0 outcome=0 ack=1 possible=1 "
            "uncertain=0 abort=00000000 cleanup=0 size=1 data=05000000 admitted=70926981 "
            "completed=70929982 ne=1 reason=0 st=3f",
            // line 40
            "RESULT op=10 req=10 oreq=62 step=5 object=1018:01 dir=0 outcome=0 ack=1 possible=1 "
            "uncertain=0 abort=00000000 cleanup=0 size=4 data=00100000 admitted=70935981 "
            "completed=70938982 ne=1 reason=0 st=3f",
            // line 41
            "RESULT op=10 req=10 oreq=63 step=6 object=1018:02 dir=0 outcome=0 ack=1 possible=1 "
            "uncertain=0 abort=00000000 cleanup=0 size=4 data=01000000 admitted=70944981 "
            "completed=70947989 ne=1 reason=0 st=3f",
            // line 42
            "RESULT op=10 req=10 oreq=64 step=7 object=1018:03 dir=0 outcome=0 ack=1 possible=1 "
            "uncertain=0 abort=00000000 cleanup=0 size=4 data=00000000 admitted=70953981 "
            "completed=70956988 ne=1 reason=0 st=3f",
            // line 43
            "RESULT op=10 req=10 oreq=65 step=8 object=2000:00 dir=0 outcome=0 ack=1 possible=1 "
            "uncertain=0 abort=00000000 cleanup=0 size=2 data=00040000 admitted=70962981 "
            "completed=70965988 ne=1 reason=0 st=3f",
            // line 44
            "RESULT op=10 req=10 oreq=66 step=9 object=2001:00 dir=0 outcome=0 ack=1 possible=1 "
            "uncertain=0 abort=00000000 cleanup=0 size=2 data=00020000 admitted=70971981 "
            "completed=70974990 ne=1 reason=0 st=3f",
            // line 45
            "END discovery terminals=18 targets=2; selection sends nothing; r=release",
         }},
    };
    Rig rig;
    for (const Plan& p : plans) {
        for (unsigned node = 1; node <= 2; ++node) {
            rig.wire.script[node] = p.replies;
            rig.wire.scriptLength[node] = p.replyCount;
            rig.wire.scriptAt[node] = 0;
        }
        const unsigned sent = rig.wire.sentCount;
        CHECK(rig.bench.begin(rig.request(p.mode, p.id, p.select, p.expect, p.seconds), rig.now) ==
              nullptr);
        rig.finish(80000);
        CHECK(rig.wire.scriptAt[1] == p.replyCount && rig.wire.scriptAt[2] == p.replyCount);
        CHECK(rig.wire.sentCount == sent + 2 * p.replyCount);
        char out[32][E::DISCOVERY_BENCH_LINE];
        CHECK(rig.lines(out, 32) == p.lineCount);
        for (unsigned i = 0; i < p.lineCount; ++i)
            CHECK(sameLine(out[i], p.lines[i], skipFor(p.lines[i])));
        const E::DiscoveryBenchRecord& r = *rig.bench.record();
        CHECK(r.selection.txBefore == r.selection.txAfter);
        CHECK(rig.bench.release());
    }
    for (unsigned slot = 0; slot < 2; ++slot) {
        const C::NodeSnapshot& node = *rig.services.node(uint8_t(slot));
        CHECK(!node.heartbeat.present && !node.nmtTime.present && !node.transitionPending);
    }
}
} // namespace
int main() {
    admissionRefusals();
    benchAddsNoOwnerWork();
    passiveObservation();
    probeAndSelection();
    identityExpectations();
    silentNodeKeepsUncertainty();
    replayDiscovery20261109();
    std::printf("discovery bench: %u checks passed\n", checks);
}
