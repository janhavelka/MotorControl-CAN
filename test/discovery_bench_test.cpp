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
// Two identity dictionaries in the specimens' retained form; node 2 can differ on request.
struct Wire : OwnerRig::WireBase {
    uint64_t now = 0;
    bool silent[3] = {};
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
} // namespace
int main() {
    admissionRefusals();
    benchAddsNoOwnerWork();
    passiveObservation();
    probeAndSelection();
    identityExpectations();
    silentNodeKeepsUncertainty();
    std::printf("discovery bench: %u checks passed\n", checks);
}
