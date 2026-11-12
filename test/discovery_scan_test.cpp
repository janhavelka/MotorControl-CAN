// SPDX-License-Identifier: MIT
// Synthetic integration through the existing ProbeApp, owner and native SDO store.
#include "DiscoveryScan.h"
#include <MotorControlCAN/profiles/lichuan_c/Pdo.h>
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
template <class T> struct Saved {
    unsigned char bytes[sizeof(T)];
    explicit Saved(const T& value) { std::memcpy(bytes, &value, sizeof value); }
    void unchanged(const T& value) const { CHECK(!std::memcmp(bytes, &value, sizeof value)); }
};
} // namespace
#include "support/OwnerRig.h"
namespace {
// The fixture pairs node 1 on slot 0 with node 2 on slot 1. Scenarios that configure both, or scan
// both in one plan, need two channels and two plan targets; a one-channel capacity set runs the
// single-target scenarios with slot 0 alone. Further channels stay free and change no outcome.
constexpr bool PAIRED_CHANNELS = C::SdoClient::CHANNELS >= 2;
constexpr bool PAIRED_TARGETS = E::DiscoveryPlan::CAPACITY >= 2;
constexpr unsigned FIXTURE_CHANNELS = PAIRED_CHANNELS ? 2 : 1;
L::ReadTarget target(unsigned node = 1) {
    L::ReadTarget value;
    value.channel = OwnerRig::route(node);
    value.nodeGeneration = 11;
    return value;
}
M::Frame heartbeat(unsigned node, uint8_t state) {
    M::Frame value = OwnerRig::frame(0x700 + node, 1);
    value.data[0] = state;
    return value;
}
struct Wire : OwnerRig::WireBase {
    uint64_t now = 0;
    bool silent[2] = {false, false};
    bool abortReply = false, malformed = false;
    bool bootAfterReply = false;
    unsigned uploads = 0, clientAborts = 0;
    static bool transmit(void* context, const M::Frame& request, O::TransportTxId id,
                         uint32_t epoch) {
        Wire& w = *static_cast<Wire*>(static_cast<OwnerRig::WireBase*>(context));
        // Any download, NMT, drain marker or guessed route is a test failure.
        CHECK(request.id == 0x601 || request.id == 0x602);
        CHECK(!request.extended && !request.rtr && request.payloadLength == 8);
        if (request.data[0] == 0x80) {
            // The real stack terminates malformed replies with a protocol abort.
            // This is no upload retry, download, or application mutation intent.
            CHECK(w.malformed && w.uploads == 1 && !w.clientAborts);
            CHECK(request.data[1] == 0 && request.data[2] == 0x10 && request.data[3] == 0);
            ++w.clientAborts;
            return OwnerRig::WireBase::transmit(context, request, id, epoch);
        }
        CHECK(request.data[0] == 0x40);
        ++w.uploads;
        CHECK(OwnerRig::WireBase::transmit(context, request, id, epoch));
        const unsigned node = request.id - 0x600;
        if (w.silent[node - 1])
            return true;
        const uint16_t index = uint16_t(request.data[1]) | uint16_t(request.data[2]) << 8;
        const uint8_t subindex = request.data[3];
        uint8_t width = 0;
        uint32_t raw = 0;
        // Deliberately unrelated fixture data, never identification rules or EDS defaults.
        switch (index) {
        case 0x1000:
            width = 4;
            raw = 0x12345678;
            break;
        case 0x1009:
            width = 2;
            raw = 0x1122;
            break;
        case 0x100a:
            width = 2;
            raw = 0x3344;
            break;
        case 0x1018:
            CHECK(subindex <= 3);
            width = subindex ? 4 : 1;
            raw = subindex == 0   ? 3
                  : subindex == 1 ? 0xa1b2c3d4u
                  : subindex == 2 ? 0x55667788u
                                  : 0x99aabbccu;
            break;
        case 0x2000:
            width = 2;
            raw = 0x4567;
            break;
        case 0x2001:
            width = 2;
            raw = 0x6789;
            break;
        default:
            CHECK(false);
        }
        CHECK(index == 0x1018 || !subindex);
        M::Frame reply = OwnerRig::frame(0x580 + node, 8);
        reply.data[0] = w.abortReply ? 0x80 : width == 1 ? 0x4f : width == 2 ? 0x4b : 0x43;
        reply.data[1] = request.data[1];
        reply.data[2] = request.data[2];
        reply.data[3] = subindex;
        if (w.malformed)
            reply.data[0] = 0x46; // Invalid reserved expedited-upload command bits.
        if (w.abortReply)
            raw = 0xfedcba98u;
        for (unsigned i = 0; i < 4; ++i)
            reply.data[4 + i] = static_cast<uint8_t>(raw >> (8 * i));
        w.push(reply, w.now, epoch);
        if (w.bootAfterReply)
            w.push(heartbeat(node, 0), w.now, epoch);
        return true;
    }
    O::Transport callbacks() {
        O::Transport value = plumbing();
        value.transmit = transmit;
        return value;
    }
};
struct Rig {
    C::SdoClient services;
    Wire wire;
    E::ProbePlatform platform;
    O::CanBusOwner owner;
    E::ProbeApp app;
    E::DiscoveryScan scan;
    E::DiscoveryInterface actual;
    uint64_t now = 0;
    explicit Rig(unsigned channels = FIXTURE_CHANNELS, bool networkOnly = false)
        : platform(E::ProbeCallbacks()), owner(services, wire.callbacks(), 137),
          app(owner, platform), scan(app, owner, services) {
        actual.bitrate = 500000;
        for (unsigned i = 0; i < channels; ++i) {
            const L::ReadTarget t = target(i + 1);
            if (!networkOnly)
                CHECK(services.configure(static_cast<uint8_t>(i), t.channel, true, true));
            C::NetworkRoute network;
            network.binding = t.channel.binding;
            network.generation = t.nodeGeneration;
            network.heartbeatId = 0x701 + i;
            network.emcyId = 0x81 + i;
            CHECK(services.configureNetwork(static_cast<uint8_t>(i), network, C::NetworkObserver(),
                                            0));
        }
    }
    E::DiscoveryPlan plan(E::DiscoveryMode mode, unsigned count = 2) const {
        E::DiscoveryPlan p;
        p.count = static_cast<uint8_t>(count);
        for (unsigned i = 0; i < count && i < E::DiscoveryPlan::CAPACITY; ++i)
            p.targets[i].target = target(i + 1);
        p.interface = actual;
        p.mode = mode;
        p.options.queueTimeoutUs = 10;
        p.options.responseTimeoutUs = 5;
        p.maximumServices = static_cast<uint8_t>(mode == E::DiscoveryMode::IDENTITY ? 9 * count
                                                 : mode == E::DiscoveryMode::PROBE  ? count
                                                                                    : 0);
        p.maximumAgeUs = 100;
        p.serviceBudget = OwnerRig::budget();
        return p;
    }
    void turn(uint64_t time) {
        now = wire.now = time;
        const uint8_t admitted = scan.summary().servicesAdmitted;
        CHECK(scan.service(actual, time));
        CHECK(scan.summary().servicesAdmitted <= admitted + 1);
    }
    void finish() {
        for (unsigned i = 0; i < 200 && scan.active(); ++i)
            turn(now + 1);
        CHECK(scan.complete());
        CHECK(scan.summary().completed == scan.summary().targets);
    }
};
void activeReadsAndRetention() {
    for (unsigned identity = 0; identity < 2; ++identity) {
        Rig rig;
        for (uint8_t i = 0; i < 2; ++i)
            CHECK(rig.services.setProducerInterval(i, rig.services.node(i)->route, 0, 0));
        E::DiscoveryPlan p =
            rig.plan(identity ? E::DiscoveryMode::IDENTITY : E::DiscoveryMode::PROBE);
        CHECK(rig.scan.begin(p, rig.actual, 0, 100));
        CHECK(!rig.wire.sentCount && !rig.scan.result(0) && !rig.scan.release());
        p.count = 0;
        p.maximumServices = 0;
        p.serviceBudget = O::ServiceBudget();
        p.targets[0].target = target(3); // Caller input is borrowed only during begin.
        rig.finish();
        const unsigned count = identity ? 18 : 2;
        CHECK(rig.wire.sentCount == count && rig.scan.summary().servicesAdmitted == count);
        CHECK(rig.scan.summary().attempted == 2 && rig.scan.summary().occupiedSlotsAtStart == 2 &&
              rig.scan.summary().freeSlotsAtStart == C::SdoClient::CHANNELS - 2 &&
              !rig.scan.summary().unresolvedSlots);
        for (uint8_t i = 0; i < 2; ++i) {
            const E::DiscoveryFinding* f = rig.scan.result(i);
            CHECK(f && f->slot == i && !f->heartbeat.present && !f->bootup.present);
            CHECK(f->outcome == (identity ? E::DiscoveryOutcome::GENERIC_IDENTITY
                                          : E::DiscoveryOutcome::PRESENCE));
            CHECK(f->confidence == (identity ? E::DiscoveryConfidence::RAW_IDENTITY
                                             : E::DiscoveryConfidence::RESPONDER_ONLY));
            CHECK(f->read.state == M::ReadState::SUCCEEDED && f->read.evidencePresent);
            CHECK(!f->unresolved && f->read.values[0].raw == 0x12345678);
            // DEC-1 admits unknown read effects; success cannot turn them into NON_CHANGING.
            CHECK(f->read.evidence.effectPossible && !f->read.evidence.effectUncertain);
        }
        CHECK(!rig.scan.result(2) && !rig.scan.begin(p, rig.actual, rig.now, 200));
        const E::DiscoveryFinding* f = rig.scan.result(0);
        const Saved<E::DiscoveryFinding> retained(*f);
        const Saved<E::DiscoverySummary> summary(rig.scan.summary());
        rig.turn(rig.now + 1);
        retained.unchanged(*f);
        summary.unchanged(rig.scan.summary());
        CHECK(rig.scan.release() && !rig.scan.result(0) && !rig.scan.release());
        CHECK(!rig.services.node(0)->heartbeat.present); // A disabled producer does not bar SDO.
    }
}
E::DiscoveryExpectation expectation() {
    E::DiscoveryExpectation value;
    value.identityKnown = true;
    value.vendorId = 0xa1b2c3d4;
    value.productCode = 0x55667788;
    value.modelCode = 0x4567;
    value.firmwareKnown = true;
    value.revision = 0x99aabbcc;
    value.firmwareCode = 0x6789;
    return value;
}
void explicitInterpretation() {
    for (unsigned scenario = 0; scenario < 3; ++scenario) {
        Rig rig;
        E::DiscoveryPlan p = rig.plan(E::DiscoveryMode::IDENTITY, 1);
        p.targets[0].expected = expectation();
        if (scenario == 1)
            ++p.targets[0].expected.vendorId;
        if (scenario == 2)
            ++p.targets[0].expected.firmwareCode;
        CHECK(rig.scan.begin(p, rig.actual, 0, 100));
        rig.finish();
        const E::DiscoveryFinding& f = *rig.scan.result(0);
        CHECK(f.outcome == (scenario == 1   ? E::DiscoveryOutcome::CONTRADICTORY_IDENTITY
                            : scenario == 2 ? E::DiscoveryOutcome::UNSUPPORTED_FIRMWARE
                                            : E::DiscoveryOutcome::GENERIC_IDENTITY));
        if (!scenario)
            CHECK(f.confidence == E::DiscoveryConfidence::EXPECTATION_MATCH);
        CHECK(f.read.state == M::ReadState::SUCCEEDED && rig.wire.sentCount == 9);
    }
    Rig abort;
    abort.wire.abortReply = true;
    CHECK(abort.scan.begin(abort.plan(E::DiscoveryMode::PROBE, 1), abort.actual, 0, 100));
    abort.finish();
    const E::DiscoveryFinding& f = *abort.scan.result(0);
    CHECK(f.outcome == E::DiscoveryOutcome::PROTOCOL_REJECTED);
    CHECK(f.read.evidence.abortValid && f.read.evidence.abortCode == 0xfedcba98u);
    CHECK(abort.wire.sentCount == 1);
    Rig malformed;
    malformed.wire.malformed = true;
    CHECK(
        malformed.scan.begin(malformed.plan(E::DiscoveryMode::PROBE, 1), malformed.actual, 0, 100));
    malformed.finish();
    const E::DiscoveryFinding& bad = *malformed.scan.result(0);
    CHECK(bad.outcome == E::DiscoveryOutcome::SERVICE_ERROR && bad.unresolved &&
          bad.read.uncertain);
    CHECK(bad.read.evidence.outcome == C::SdoOutcome::CLIENT_ABORT && malformed.wire.uploads == 1);
    CHECK(malformed.wire.clientAborts == 1 && malformed.wire.sentCount == 2);
    CHECK(malformed.scan.release());
    CHECK(!malformed.scan.begin(malformed.plan(E::DiscoveryMode::PROBE, 1), malformed.actual,
                                malformed.now, 200));
}
void permanentSlotsAndSilence() {
    Rig rig;
    rig.wire.silent[0] = rig.wire.silent[1] = true;
    CHECK(rig.scan.begin(rig.plan(E::DiscoveryMode::PROBE), rig.actual, 0, 100));
    rig.finish();
    CHECK(rig.wire.sentCount == 2 && rig.scan.summary().unresolvedSlots == 2);
    for (uint8_t i = 0; i < 2; ++i) {
        const E::DiscoveryFinding& f = *rig.scan.result(i);
        CHECK(f.outcome == E::DiscoveryOutcome::NO_RESPONSE && f.unresolved && f.read.uncertain);
        CHECK(f.read.evidencePresent && f.read.evidence.outcome == C::SdoOutcome::TIMEOUT);
    }
    CHECK(rig.scan.release());
    CHECK(!rig.services.configure(0, target(3).channel, true, true));
    CHECK(!rig.services.configure(1, target(3).channel, true, true));
    CHECK(!rig.scan.begin(rig.plan(E::DiscoveryMode::PROBE), rig.actual, rig.now, 200));
    CHECK(rig.wire.sentCount == 2); // No automatic marker, retry, fresh adapter or rebind.
}
void atomicPlanRefusal() {
    Rig rig(1);
    const E::DiscoveryPlan valid = rig.plan(E::DiscoveryMode::PROBE, 1);
    for (unsigned scenario = 0; scenario < 12; ++scenario) {
        E::DiscoveryPlan p = valid;
        E::DiscoveryInterface actual = rig.actual;
        switch (scenario) {
        case 0:
            ++actual.bitrate;
            break;
        case 1:
            p.interface.bitrate = actual.bitrate = 0;
            break;
        case 2:
            p.count = 0;
            break;
        case 3:
            p.count = E::DiscoveryPlan::CAPACITY + 1;
            break;
        case 4: // A second target on a slot this rig never configured; needs room for two.
            if (!PAIRED_TARGETS)
                continue;
            p.count = 2;
            p.targets[1].target = target(2);
            p.maximumServices = 2;
            break;
        case 5:
            ++p.targets[0].target.nodeGeneration;
            break;
        case 6:
            ++p.targets[0].target.channel.binding.bindingGeneration;
            break;
        case 7:
            ++p.targets[0].target.channel.clientToServerId;
            break;
        case 8:
            p.maximumServices = 0;
            break;
        case 9:
            p.mode = E::DiscoveryMode::IDENTITY;
            p.maximumServices = 8;
            break;
        case 10:
            p.maximumAgeUs = 0;
            break;
        case 11:
            p.options.responseTimeoutUs = 0;
            break;
        }
        const Saved<O::OwnerResources> resources(rig.owner.resources());
        CHECK(!rig.scan.begin(p, actual, 0, 100));
        CHECK(!rig.scan.active() && !rig.scan.complete() && !rig.scan.result(0));
        CHECK(!rig.app.selected() && rig.app.readContext().state == M::ReadState::EMPTY);
        resources.unchanged(rig.owner.resources());
        CHECK(!rig.wire.sentCount);
    }
    CHECK(rig.scan.begin(valid, rig.actual, 0, 100));
    CHECK(!rig.scan.service(rig.actual, UINT64_MAX));
    CHECK(!rig.scan.cancel(UINT64_MAX));
    rig.finish();
    CHECK(rig.scan.summary().occupiedSlotsAtStart == 1 &&
          rig.scan.summary().freeSlotsAtStart == C::SdoClient::CHANNELS - 1);
}
void cancellationAndDeadline() {
    if (!PAIRED_TARGETS)
        return; // Every case here scans both fixture nodes; deadlineCutoff scans one.
    for (unsigned sent = 0; sent < 2; ++sent) {
        Rig rig;
        rig.wire.silent[0] = true;
        CHECK(rig.scan.begin(rig.plan(E::DiscoveryMode::PROBE), rig.actual, 0, 100));
        for (unsigned i = 0;
             i < 10 && (!rig.app.pendingRequest().owner || (sent && !rig.wire.sentCount)); ++i)
            rig.turn(rig.now + 1);
        CHECK(rig.app.pendingRequest().owner && rig.wire.sentCount == sent);
        CHECK(rig.scan.cancel(rig.now) && !rig.scan.result(0));
        rig.finish();
        CHECK(rig.scan.summary().cancelled && rig.wire.sentCount == sent);
        CHECK(rig.scan.result(0)->outcome == E::DiscoveryOutcome::CANCELLED);
        CHECK(rig.scan.result(0)->unresolved == bool(sent));
        CHECK(rig.scan.result(0)->read.uncertain == bool(sent));
        CHECK(rig.scan.result(1)->outcome == E::DiscoveryOutcome::CANCELLED);
    }
    Rig zero;
    E::DiscoveryPlan p = zero.plan(E::DiscoveryMode::PROBE);
    p.serviceBudget = O::ServiceBudget();
    CHECK(zero.scan.begin(p, zero.actual, 0, 10));
    zero.turn(1);
    zero.turn(2);
    CHECK(!zero.owner.resources().queued && !zero.owner.resources().active &&
          !zero.owner.resources().retained && !zero.app.pendingRequest().owner);
    CHECK(!zero.scan.summary().servicesAdmitted);
    zero.turn(10); // Equality expires even when owner RX/TX/event/start budgets are all zero.
    zero.finish();
    CHECK(zero.scan.summary().deadlineExpired && !zero.wire.sentCount);
    CHECK(zero.scan.result(0)->outcome == E::DiscoveryOutcome::DEADLINE);
    CHECK(!zero.scan.result(0)->unresolved);
    Rig changed;
    CHECK(changed.scan.begin(changed.plan(E::DiscoveryMode::PROBE), changed.actual, 0, 100));
    ++changed.actual.bitrate;
    changed.turn(1);
    changed.finish();
    CHECK(changed.scan.summary().interfaceChanged && !changed.wire.sentCount);
    CHECK(changed.scan.result(0)->outcome == E::DiscoveryOutcome::INVALIDATED);
    Rig changedAfterTx;
    changedAfterTx.wire.silent[0] = true;
    CHECK(changedAfterTx.scan.begin(changedAfterTx.plan(E::DiscoveryMode::PROBE),
                                    changedAfterTx.actual, 0, 100));
    while (!changedAfterTx.wire.sentCount)
        changedAfterTx.turn(changedAfterTx.now + 1);
    ++changedAfterTx.actual.bitrate;
    changedAfterTx.turn(changedAfterTx.now + 1);
    changedAfterTx.finish();
    CHECK(changedAfterTx.wire.sentCount == 1);
    CHECK(changedAfterTx.scan.result(0)->outcome == E::DiscoveryOutcome::INVALIDATED &&
          changedAfterTx.scan.result(0)->read.uncertain &&
          changedAfterTx.scan.result(0)->unresolved);
}
void deadlineCutoff() {
    Rig cutoff;
    CHECK(cutoff.scan.begin(cutoff.plan(E::DiscoveryMode::PROBE, 1), cutoff.actual, 0, 10));
    while (!cutoff.wire.sentCount)
        cutoff.turn(cutoff.now + 1);
    CHECK(cutoff.wire.rxWrite == 1); // Successful earlier-captured reply is still queued.
    cutoff.turn(10);
    cutoff.finish();
    CHECK(cutoff.scan.result(0)->outcome == E::DiscoveryOutcome::DEADLINE);
    CHECK(cutoff.scan.result(0)->unresolved && cutoff.scan.result(0)->read.uncertain);
}
void passiveObservations() {
    for (unsigned late = 0; late < 2; ++late) {
        Rig cutoff;
        CHECK(cutoff.scan.begin(cutoff.plan(E::DiscoveryMode::PASSIVE, 1), cutoff.actual, 0, 10));
        cutoff.wire.push(heartbeat(1, 5), 9);
        cutoff.turn(late ? 10 : 9);
        cutoff.finish();
        const E::DiscoveryFinding& f = *cutoff.scan.result(0);
        CHECK(cutoff.services.node(0)->heartbeat.present); // Ordinary dispatch still proceeds.
        CHECK(f.heartbeat.present == !late);
        CHECK(f.outcome ==
              (late ? E::DiscoveryOutcome::NO_RESPONSE : E::DiscoveryOutcome::PRESENCE));
        CHECK(!cutoff.wire.sentCount);
    }
    if (PAIRED_TARGETS) { // Heartbeat and boot-up of the two fixture nodes in one plan.
        Rig rig;
        E::DiscoveryPlan p = rig.plan(E::DiscoveryMode::PASSIVE);
        CHECK(rig.scan.begin(p, rig.actual, 0, 10));
        rig.wire.push(heartbeat(1, 5), 1);
        rig.wire.push(heartbeat(2, 0), 1);
        rig.turn(1);
        rig.finish();
        CHECK(!rig.wire.sentCount && !rig.scan.summary().servicesAdmitted);
        const E::DiscoveryFinding& hb = *rig.scan.result(0);
        const E::DiscoveryFinding& boot = *rig.scan.result(1);
        CHECK(hb.heartbeat.present && hb.heartbeat.receivedUs == 1);
        CHECK(hb.outcome == E::DiscoveryOutcome::PRESENCE);
        CHECK(boot.bootup.present && !boot.heartbeat.present && boot.bootup.receivedUs == 1);
        CHECK(boot.outcome == E::DiscoveryOutcome::PRESENCE);
    }
    Rig duplicate;
    CHECK(duplicate.scan.begin(duplicate.plan(E::DiscoveryMode::PASSIVE, 1), duplicate.actual, 0,
                               50));
    // Contradictory overlapping capture intervals cannot establish exact NMT ordering.
    CHECK(duplicate.services.receive(heartbeat(1, 5), 10, 20, 20));
    CHECK(duplicate.services.receive(heartbeat(1, 4), 15, 25, 25));
    CHECK(duplicate.services.node(0)->nmtOrderingUncertain);
    duplicate.turn(25);
    duplicate.finish();
    CHECK(duplicate.scan.result(0)->suspectedDuplicate && !duplicate.wire.sentCount);
    Rig unknown;
    CHECK(unknown.scan.begin(unknown.plan(E::DiscoveryMode::PASSIVE, 1), unknown.actual, 0, 5));
    unknown.wire.push(heartbeat(3, 5), 1);
    unknown.finish();
    CHECK(!unknown.scan.result(0)->heartbeat.present && !unknown.scan.result(0)->bootup.present);
    CHECK(!unknown.wire.sentCount);
    Rig sequence;
    CHECK(sequence.scan.begin(sequence.plan(E::DiscoveryMode::PASSIVE, 1), sequence.actual, 0, 10));
    sequence.wire.push(heartbeat(1, 0), 1);
    sequence.wire.push(heartbeat(1, 5), 1);
    sequence.turn(1);
    CHECK(!sequence.services.node(0)->heartbeat.present && sequence.wire.observed == 2);
    sequence.wire.push(heartbeat(1, 5), 2);
    sequence.turn(2);
    sequence.finish();
    const E::DiscoveryFinding& pair = *sequence.scan.result(0);
    CHECK(pair.bootup.present && pair.bootup.receivedUs == 1);
    CHECK(pair.heartbeat.present && pair.heartbeat.receivedUs == 2);
    CHECK(pair.outcome == E::DiscoveryOutcome::PRESENCE && !sequence.wire.sentCount);
    CHECK(sequence.wire.observed == 3);
    Rig overflow;
    CHECK(overflow.scan.begin(overflow.plan(E::DiscoveryMode::PASSIVE, 1), overflow.actual, 0, 10));
    for (unsigned i = 0; i < 8; ++i)
        overflow.wire.push(heartbeat(1, 5), 1);
    overflow.turn(1);
    overflow.finish();
    CHECK(overflow.scan.result(0)->heartbeat.present && !overflow.wire.sentCount);
    CHECK(overflow.scan.summary().passiveHistoryDrops == 8 - C::SdoClient::NETWORK_HISTORY);
    CHECK(overflow.wire.observed == 8 && overflow.services.node(0)->heartbeat.present);
}
void selectionAndInvalidation() {
    Rig rig;
    CHECK(rig.app.select(target()));
    const Saved<L::ReadTarget> selection(*rig.app.selected());
    for (unsigned i = 0; i < 3; ++i) {
        L::ReadTarget stale = target();
        if (!i)
            ++stale.nodeGeneration;
        else if (i == 1)
            ++stale.channel.binding.configurationGeneration;
        else
            ++stale.channel.serverToClientId;
        CHECK(!rig.app.select(stale));
        selection.unchanged(*rig.app.selected());
    }
    CHECK(rig.app.select(target(2)) && !rig.wire.sentCount);
    CHECK(rig.services.node(0)->route.generation == target().nodeGeneration);
    CHECK(rig.scan.begin(rig.plan(E::DiscoveryMode::PROBE, 1), rig.actual, 0, 100));
    rig.turn(1);
    rig.wire.push(heartbeat(1, 0), 2);
    rig.turn(2);
    rig.finish();
    CHECK(rig.scan.result(0)->outcome == E::DiscoveryOutcome::INVALIDATED);
    CHECK(rig.services.node(1)->route.generation == target(2).nodeGeneration);
    CHECK(!rig.wire.sentCount);
}
void qualifiedPdoOnly() {
    for (unsigned qualified = 0; qualified < 2; ++qualified) {
        Rig rig;
        C::PdoMap map;
        CHECK(L::pdoTemplate(C::PdoDirection::TPDO, 2, map));
        C::PdoMapEvidence actual;
        actual.route.binding = target().channel.binding;
        actual.route.generation = 7;
        actual.route.cobId = 0x281;
        CHECK(C::validatePdoMap(map, L::pdoProfile(41), actual.route.layout));
        actual.nodeGeneration = target().nodeGeneration;
        actual.source = C::PdoMapSource::REVIEWED_CONFIGURATION;
        actual.evidenceId = 501;
        actual.communicationKnown = true;
        actual.cobIdParameter = 0x281;
        actual.transmissionType = 255;
        CHECK(rig.services.configurePdo(0, 0, actual, L::pdoProfile(41), C::PdoObserver(), 0));
        if (qualified) {
            C::PdoQualification proof;
            proof.route = actual.route;
            proof.nodeGeneration = target().nodeGeneration;
            proof.basis = C::PdoSampleBasis::PRODUCER_AND_INGRESS_RETIRED;
            proof.evidenceId = 502;
            CHECK(rig.services.qualifyPdo(0, proof, 0));
        }
        CHECK(rig.scan.begin(rig.plan(E::DiscoveryMode::PASSIVE, 1), rig.actual, 0, 10));
        rig.wire.push(OwnerRig::frame(0x281, 8), 1);
        rig.turn(1);
        rig.finish();
        const E::DiscoveryFinding& f = *rig.scan.result(0);
        CHECK(f.pdo.present == bool(qualified));
        if (qualified) {
            CHECK(f.mappingGeneration == 7 && f.pdo.receivedUs == 1);
            CHECK(f.outcome == E::DiscoveryOutcome::PRESENCE);
        } else
            CHECK(f.outcome == E::DiscoveryOutcome::NO_RESPONSE && !f.mappingGeneration);
        CHECK(!f.heartbeat.present && !f.bootup.present && !rig.wire.sentCount);
    }
}
void lifecycleAtPublication() {
    Rig sameTurn;
    sameTurn.wire.bootAfterReply = true;
    CHECK(sameTurn.scan.begin(sameTurn.plan(E::DiscoveryMode::PROBE, 1), sameTurn.actual, 0, 100));
    sameTurn.finish();
    const E::DiscoveryFinding& f = *sameTurn.scan.result(0);
    CHECK(f.outcome == E::DiscoveryOutcome::INVALIDATED &&
          f.confidence == E::DiscoveryConfidence::NONE);
    CHECK(f.read.state == M::ReadState::SUCCEEDED && f.read.values[0].valid &&
          f.read.values[0].raw == 0x12345678);
    CHECK(f.read.evidence.outcome == C::SdoOutcome::SUCCESS);
    CHECK(sameTurn.services.node(0)->route.generation > target().nodeGeneration);
    CHECK(sameTurn.wire.sentCount == 1 && sameTurn.wire.observed == 2);

    Rig earlier;
    earlier.wire.push(heartbeat(1, 0), 1);
    CHECK(earlier.app.service(1)); // Normal dispatch leaves its diagnostic copy queued.
    E::DiscoveryPlan oldBoot = earlier.plan(E::DiscoveryMode::PASSIVE, 1);
    oldBoot.targets[0].target.nodeGeneration = earlier.services.node(0)->route.generation;
    CHECK(oldBoot.targets[0].target.nodeGeneration > target().nodeGeneration);
    CHECK(earlier.scan.begin(oldBoot, earlier.actual, 2, 10));
    earlier.turn(3);
    earlier.finish();
    CHECK(!earlier.scan.result(0)->bootup.present && !earlier.scan.result(0)->heartbeat.present);
    CHECK(earlier.scan.result(0)->outcome == E::DiscoveryOutcome::NO_RESPONSE);
    CHECK(earlier.wire.observed == 1 && !earlier.wire.sentCount);

    Rig cached;
    cached.wire.push(heartbeat(1, 5), 1);
    CHECK(cached.app.service(1));
    CHECK(cached.scan.begin(cached.plan(E::DiscoveryMode::PASSIVE, 1), cached.actual, 2, 10));
    cached.turn(3);
    cached.finish();
    CHECK(cached.scan.result(0)->heartbeat.present &&
          cached.scan.result(0)->heartbeat.receivedUs == 1);
    CHECK(cached.scan.result(0)->heartbeat.processedUs == 1);
    CHECK(cached.scan.result(0)->outcome == E::DiscoveryOutcome::PRESENCE);

    Rig retiredHeartbeat;
    retiredHeartbeat.wire.push(heartbeat(1, 5), 1);
    CHECK(retiredHeartbeat.app.service(1));
    CHECK(retiredHeartbeat.services.node(0)->heartbeat.present);
    retiredHeartbeat.wire.push(heartbeat(1, 0), 2);
    CHECK(retiredHeartbeat.app.service(2));
    CHECK(!retiredHeartbeat.services.node(0)->heartbeat.present);
    E::DiscoveryPlan current = retiredHeartbeat.plan(E::DiscoveryMode::PASSIVE, 1);
    current.targets[0].target.nodeGeneration = retiredHeartbeat.services.node(0)->route.generation;
    CHECK(current.targets[0].target.nodeGeneration > target().nodeGeneration);
    CHECK(retiredHeartbeat.scan.begin(current, retiredHeartbeat.actual, 3, 10));
    retiredHeartbeat.turn(4);
    retiredHeartbeat.finish();
    const E::DiscoveryFinding& retired = *retiredHeartbeat.scan.result(0);
    CHECK(!retired.heartbeat.present && !retired.bootup.present && retired.bindingCurrent);
    CHECK(retired.outcome == E::DiscoveryOutcome::NO_RESPONSE);
    CHECK(retiredHeartbeat.wire.observed == 2 && !retiredHeartbeat.wire.sentCount);
    if (!PAIRED_TARGETS)
        return; // The batch case scans both fixture nodes.

    Rig batch;
    batch.wire.silent[1] = true;
    CHECK(batch.scan.begin(batch.plan(E::DiscoveryMode::PROBE), batch.actual, 0, 100));
    for (unsigned i = 0; i < 30 && batch.wire.sentCount < 2; ++i)
        batch.turn(batch.now + 1);
    CHECK(batch.wire.sentCount == 2 && batch.scan.active());
    CHECK(batch.scan.summary().completed == 1 && !batch.scan.result(0));
    batch.wire.push(heartbeat(1, 0), batch.now + 1);
    batch.turn(batch.now + 1);
    batch.finish();
    const E::DiscoveryFinding& first = *batch.scan.result(0);
    const E::DiscoveryFinding& second = *batch.scan.result(1);
    CHECK(first.outcome == E::DiscoveryOutcome::INVALIDATED &&
          first.confidence == E::DiscoveryConfidence::NONE);
    CHECK(first.read.state == M::ReadState::SUCCEEDED && first.read.values[0].valid &&
          first.read.values[0].raw == 0x12345678);
    CHECK(second.outcome == E::DiscoveryOutcome::NO_RESPONSE && second.read.uncertain);
    CHECK(first.unresolved == batch.services.unresolved(0));
    CHECK(second.unresolved == batch.services.unresolved(1));
    CHECK(batch.scan.summary().unresolvedSlots ==
          unsigned(batch.services.unresolved(0)) + unsigned(batch.services.unresolved(1)));
    CHECK(batch.scan.summary().unresolvedSlots == 2 && batch.wire.sentCount == 2);
    CHECK(!first.bindingCurrent && second.bindingCurrent);
}
void negativeIdentitySurvivesInvalidation() {
    for (unsigned firmware = 0; firmware < 2; ++firmware) {
        Rig rig;
        rig.wire.silent[1] = true;
        E::DiscoveryPlan p = rig.plan(E::DiscoveryMode::IDENTITY);
        p.targets[0].expected = expectation();
        if (firmware)
            ++p.targets[0].expected.firmwareCode;
        else
            ++p.targets[0].expected.vendorId;
        CHECK(rig.scan.begin(p, rig.actual, 0, 100));
        for (unsigned i = 0; i < 80 && rig.wire.sentCount < 10; ++i)
            rig.turn(rig.now + 1);
        CHECK(rig.wire.sentCount == 10 && rig.scan.active() && rig.scan.summary().completed == 1);
        rig.wire.push(heartbeat(1, 0), rig.now + 1);
        rig.turn(rig.now + 1);
        rig.finish();
        const E::DiscoveryFinding& first = *rig.scan.result(0);
        CHECK(first.outcome == (firmware ? E::DiscoveryOutcome::UNSUPPORTED_FIRMWARE
                                         : E::DiscoveryOutcome::CONTRADICTORY_IDENTITY));
        CHECK(!first.bindingCurrent && first.read.state == M::ReadState::SUCCEEDED);
        CHECK(first.read.values[4].valid && first.read.values[4].raw == 0xa1b2c3d4u);
        CHECK(first.read.values[8].valid && first.read.values[8].raw == 0x6789);
        CHECK(rig.scan.result(1)->outcome == E::DiscoveryOutcome::NO_RESPONSE &&
              rig.scan.result(1)->bindingCurrent);
        CHECK(rig.scan.summary().unresolvedSlots == 2 && rig.wire.sentCount == 10);
    }
}
void passiveWithoutServicePremises() {
    for (unsigned bootup = 0; bootup < 2; ++bootup) {
        Rig rig(1, true);
        CHECK(!rig.services.channel(0) && rig.services.node(0));
        E::DiscoveryPlan p = rig.plan(E::DiscoveryMode::PASSIVE, 1);
        p.targets[0].target.channel.clientToServerId = 0;
        p.targets[0].target.channel.serverToClientId = 0;
        E::DiscoveryPlan active = p;
        active.mode = E::DiscoveryMode::PROBE;
        active.maximumServices = 1;
        CHECK(!rig.scan.begin(active, rig.actual, 0, 10));
        CHECK(!rig.scan.active() && !rig.app.selected() && !rig.wire.sentCount);
        CHECK(!rig.owner.resources().reservations && !rig.owner.resources().queued);
        CHECK(rig.scan.begin(p, rig.actual, 0, 10));
        rig.wire.push(heartbeat(1, bootup ? 0 : 5), 1);
        rig.turn(1);
        rig.finish();
        const E::DiscoveryFinding& f = *rig.scan.result(0);
        CHECK(f.outcome == E::DiscoveryOutcome::PRESENCE &&
              f.confidence == E::DiscoveryConfidence::RESPONDER_ONLY);
        CHECK(f.bindingCurrent == !bootup);
        CHECK(f.bootup.present == bool(bootup) && f.heartbeat.present == !bootup);
        CHECK(rig.scan.summary().occupiedSlotsAtStart == 1 &&
              rig.scan.summary().freeSlotsAtStart == C::SdoClient::CHANNELS - 1);
        CHECK(!rig.services.channel(0) && !rig.wire.sentCount &&
              !rig.scan.summary().servicesAdmitted);
    }
}
} // namespace
int main() {
    if (PAIRED_TARGETS)
        activeReadsAndRetention();
    explicitInterpretation();
    atomicPlanRefusal();
    cancellationAndDeadline();
    deadlineCutoff();
    passiveObservations();
    if (PAIRED_CHANNELS)
        selectionAndInvalidation();
    qualifiedPdoOnly();
    if (PAIRED_TARGETS)
        permanentSlotsAndSilence();
    lifecycleAtPublication();
    if (PAIRED_TARGETS)
        negativeIdentitySurvivesInvalidation();
    passiveWithoutServicePremises();
    std::printf("discovery_scan: %u checks passed (synthetic only)\n", checks);
}
