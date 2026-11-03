// SPDX-License-Identifier: MIT
// Bounds-only ingress through the real owner, the selected SDO engine and AxisSession: the
// ESP32-S2 bench shape, with an unconfirmed channel, a passive node route, no heartbeat and
// no exact receive time. Contract change (bounds qualification, decided from the 2026-11-03 S2
// soaks): a reply whose receive interval starts strictly after its handoff qualifies ordinary
// SDO service, so the session admits writes, enable words and a sequenced stop. Unknown time
// and an interval that reaches back to its handoff still qualify nothing. Every word, status
// and value is manufactured by this fake: nothing here shows what a drive does.
#include "AxisSession.h"
#include <MotorControlCAN/profiles/lichuan_c/State.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace M = MotorControlCAN;
namespace C = MotorControlCAN::CANOPEN;
namespace L = MotorControlCAN::LICHUAN_C;
namespace E = MotorControlCANExample;
namespace O = MotorControlCANOwner;
static unsigned checks = 0;
static const char* scenario = "";
#define CHECK(condition)                                                                           \
    do {                                                                                           \
        ++checks;                                                                                  \
        if (!(condition)) {                                                                        \
            std::fprintf(stderr, "%s:%d: %s [%s]\n", __FILE__, __LINE__, #condition, scenario);    \
            std::abort();                                                                          \
        }                                                                                          \
    } while (false)
#include "support/OwnerRig.h"
namespace {
using OwnerRig::budget;
using OwnerRig::frame;
const uint64_t TICK = 500;
const uint64_t SERVICE_AGE = 50000; // Application policy for this fake, not a device timeout.
const uint32_t QUEUE = 2000, RESPONSE = 2000;
const uint16_t POWER_UP_STATUS = 0x8037; // The measured raw word, used here only as bytes.
uint8_t bit(L::StateBlock block) { return uint8_t(1u << unsigned(block)); }
const uint8_t FULL_MASK = uint8_t(bit(L::StateBlock::ERRORS) | bit(L::StateBlock::MONITORS) |
                                  bit(L::StateBlock::STATUS) | bit(L::StateBlock::FEEDBACK));
// Only accepted reads (2100-2102, 6041, 6061, 6064, 606C): what an unqualified route can send.
// ERRORS reads 1001 and 603F, UNQUALIFIED uploads that need ordinary service like any write.
const uint8_t ACCEPTED_MASK = uint8_t(FULL_MASK & ~bit(L::StateBlock::ERRORS));
// How the fake platform reports each reply's receive time.
enum class Ingress {
    BOUNDS,     ///< Interval starting strictly after the request's handoff, as S2 measured.
    AT_HANDOFF, ///< Interval whose lower bound equals the handoff.
    UNKNOWN     ///< Neither exact capture nor bounds.
};
const char* const NAMES[] = {"bounds", "at-handoff", "unknown"};
L::ReadTarget target() {
    L::ReadTarget value;
    value.channel = OwnerRig::route(1);
    value.nodeGeneration = 11;
    return value;
}
L::ActionOptions options() {
    L::ActionOptions value;
    value.service.queueTimeoutUs = QUEUE;
    value.service.responseTimeoutUs = RESPONSE;
    value.observationIntervalUs = 1000;
    value.maximumAgeUs = uint64_t(QUEUE + uint64_t(RESPONSE)) * 9;
    value.commandIntervalUs = L::ENABLE_WORD_INTERVAL_US;
    return value;
}
L::EnableEvidence evidence() {
    L::EnableEvidence value;
    value.gate = L::EnableGate::ATTENDED_EXPERIMENT;
    value.start = L::EnableStart::NO_MOTION_WRITE_RECORDED;
    value.powerUpShaft = L::ShaftObservation::FREE;
    value.reviewed = true;
    value.reviewedStatusword = POWER_UP_STATUS;
    value.reviewedPositionTarget = 5000;
    value.reviewedVelocityTarget = 120;
    return value;
}
// A fake server that follows the documented low status bits for words 6, 7 and 15.
struct FakeDrive {
    uint16_t monitor = 1, status = POWER_UP_STATUS;
    uint8_t mode = 1;
    int32_t positionTarget = 5000, velocityTarget = 120;
};
uint16_t frameIndex(const M::Frame& f) { return uint16_t(f.data[1] | (uint16_t(f.data[2]) << 8)); }
void replyTo(const M::Frame& sent, FakeDrive& drive, M::Frame& out) {
    out = frame(sent.id - 0x600 + 0x580, 8);
    out.data[1] = sent.data[1];
    out.data[2] = sent.data[2];
    out.data[3] = sent.data[3];
    const uint16_t index = frameIndex(sent);
    if (sent.data[0] != 0x40) {
        if (index == 0x6040 && sent.data[0] == 0x2b) {
            const uint16_t word = uint16_t(sent.data[4] | (uint16_t(sent.data[5]) << 8));
            const uint16_t low = word == 6 ? 1 : word == 7 ? 3 : word == 15 ? 7 : 0;
            if (low)
                drive.status = uint16_t((drive.status & ~L::STATUS_LOW_BITS) | low);
            if (word == 15)
                drive.monitor = uint16_t(drive.monitor & ~L::MONITOR_RELEASED);
        }
        if (index == 0x6060)
            drive.mode = sent.data[4];
        out.data[0] = 0x60;
        return;
    }
    uint32_t raw = 0;
    unsigned size = 2;
    switch (index) {
    case 0x1001: size = 1; break;
    case 0x2100: raw = drive.monitor; break;
    case 0x2101: raw = 0xa580; break;
    case 0x2102: raw = 0xf008; break;
    case 0x6041: raw = drive.status; break;
    case 0x6061: raw = drive.mode; size = 1; break;
    case 0x6064: case 0x606c: size = 4; break;
    case 0x607a: raw = uint32_t(drive.positionTarget); size = 4; break;
    case 0x60ff: raw = uint32_t(drive.velocityTarget); size = 4; break;
    default: break;
    }
    out.data[0] = size == 1 ? 0x4f : size == 2 ? 0x4b : 0x43;
    for (unsigned i = 0; i < 4; ++i)
        out.data[4 + i] = uint8_t(raw >> (8 * i));
}
struct Wire : OwnerRig::WireBase {
    static bool beginRecovery(void*, uint32_t) { return true; }
    static O::RecoveryProgress pollRecovery(void*, uint32_t) { return O::RecoveryProgress::RETIRED; }
    O::Transport callbacks() {
        O::Transport value = plumbing();
        value.beginRecovery = beginRecovery;
        value.pollRecovery = pollRecovery;
        return value;
    }
};
struct Rig {
    C::SdoClient services;
    Wire wire;
    O::CanBusOwner owner;
    E::AxisSession a;
    FakeDrive drive;
    Ingress ingress;
    uint64_t now = 0;
    unsigned served = 0, startFrame = 0, trials = 0, stamped = 0;
    uint64_t sentAt[OwnerRig::WireBase::CAPACITY] = {};
    explicit Rig(Ingress form) : owner(services, wire.callbacks(), 73), a(owner), ingress(form) {
        scenario = NAMES[unsigned(form)];
        const L::ReadTarget t = target();
        CHECK(services.configure(0, t.channel, false, true)); // No external confirmation.
        C::NetworkRoute network;
        network.binding = t.channel.binding;
        network.generation = 11;
        network.heartbeatId = 0x701;
        network.emcyId = 0x81;
        CHECK(services.configureNetwork(0, network, C::NetworkObserver(), 0));
        CHECK(services.setServiceAge(0, SERVICE_AGE, 0));
    }
    L::ReadTarget current() const {
        L::ReadTarget value = target();
        value.nodeGeneration = services.node(0)->route.generation;
        return value;
    }
    // A reply as the platform delivers it, one turn after its request was handed off.
    void deliver(const M::Frame& value, uint64_t handoff) {
        CHECK(wire.rxWrite < OwnerRig::WireBase::CAPACITY);
        O::ReceivedFrame& input = wire.incoming[wire.rxWrite++];
        input = O::ReceivedFrame();
        input.frame = value;
        input.controllerEpoch = owner.controllerEpoch();
        if (ingress != Ingress::UNKNOWN) {
            input.receiveBoundsKnown = true;
            input.receivedEarliestUs = ingress == Ingress::BOUNDS ? handoff + 1 : handoff;
            input.receivedLatestUs = now;
        }
    }
    void turn(uint64_t time) {
        now = time;
        if (wire.rxRead == wire.rxWrite)
            wire.rxRead = wire.rxWrite = 0;
        for (; served < wire.sentCount; ++served) {
            M::Frame reply;
            if (wire.sent[served].id == 0x601) {
                replyTo(wire.sent[served], drive, reply);
                deliver(reply, sentAt[served]);
            }
        }
        CHECK(owner.service(now, budget(8, 2, 8, 2, 1)));
        for (; stamped < wire.sentCount; ++stamped)
            sentAt[stamped] = now; // The owner hands frames off at this processing time.
        a.service(now);
    }
    void step() { turn(now + TICK); }
    void steps(unsigned count) {
        for (unsigned i = 0; i < count; ++i)
            step();
    }
    C::SdoCompletion settled() {
        C::SdoCompletion output;
        CHECK(a.settle(output));
        return output;
    }
    void readState(L::ReadContext& state, uint8_t mask = FULL_MASK) {
        const uint64_t deadline = now + 400000;
        const uint32_t operation = a.beginOperation(deadline, now);
        CHECK(operation);
        L::ReadOptions reads;
        reads.queueTimeoutUs = QUEUE;
        reads.responseTimeoutUs = RESPONSE;
        CHECK(L::prepareState(state, current(), operation, now, deadline, reads, mask));
        while (state.state == M::ReadState::ACTIVE) {
            L::PreparedRead work;
            CHECK(L::nextRead(state, now, work) && work.ready);
            CHECK(a.admitPrepared(work.transfer, now) == O::BusAdmission::ACCEPTED);
            steps(2);
            CHECK(L::advanceRead(state, settled(), now));
        }
        CHECK(state.state == M::ReadState::SUCCEEDED);
    }
    C::SdoTransfer download(uint16_t index, uint8_t size, uint32_t value) {
        const uint64_t deadline = now + 400000;
        C::SdoTransfer t;
        t.channel = target().channel;
        t.channel.binding = current().channel.binding;
        t.operation = t.request = a.beginOperation(deadline, now);
        CHECK(t.operation);
        t.step = 1;
        t.index = index;
        t.size = size;
        t.direction = C::SdoDirection::DOWNLOAD;
        for (unsigned i = 0; i < size; ++i)
            t.data[i] = uint8_t(value >> (8 * i));
        t.deadlineUs = deadline;
        t.queueTimeoutUs = QUEUE;
        t.responseTimeoutUs = RESPONSE;
        return t;
    }
    // One caller write through the session's single command path, then its settlement.
    O::BusAdmission write(uint16_t index, uint8_t size, uint32_t value) {
        const O::BusAdmission admission = a.admitPrepared(download(index, size, value), now);
        if (admission == O::BusAdmission::ACCEPTED) {
            steps(2);
            CHECK(settled().outcome == C::SdoOutcome::SUCCESS);
        }
        return admission;
    }
    L::EnableEvidence bound() {
        L::EnableEvidence given = evidence();
        given.trial = ++trials;
        const O::SequenceId id = a.sequence();
        const O::Sequence* live = owner.sequence(id);
        CHECK(live);
        given.reservation.target = current();
        given.reservation.model = L::SettingsModel::CL86_C;
        given.reservation.owner = owner.identity();
        given.reservation.sequence = uint32_t(id.slot) + 1;
        given.reservation.generation = id.generation;
        given.reservation.acquiredUs = live->acquiredUs;
        given.operation = a.nextOperation();
        given.expiresUs = now + 1000000;
        return given;
    }
};
bool wordAt(const Rig& r, unsigned i, uint16_t word) {
    const uint8_t expected[] = {0x2b, 0x40, 0x60, 0, uint8_t(word), uint8_t(word >> 8), 0, 0};
    return i < r.wire.sentCount && r.wire.sent[i].id == 0x601 &&
           !std::memcmp(r.wire.sent[i].data, expected, sizeof expected);
}
unsigned downloadsFrom(const Rig& r, unsigned from) {
    unsigned count = 0;
    for (unsigned i = from; i < r.wire.sentCount; ++i)
        count += r.wire.sent[i].data[0] != 0x40;
    return count;
}

// 1. On bounds-only ingress a settings write, the enable words and a sequenced stop are admitted,
// and a target write as a move would issue it. The proof ages from each reply's lower bound.
void boundsQualifyTheSessionsWrites() {
    Rig r(Ingress::BOUNDS);
    CHECK(r.a.open(r.current(), L::SettingsModel::CL86_C, 1, r.now));
    // Before any reply nothing qualifies ordinary work, and nothing goes out.
    CHECK(r.a.admitPrepared(r.download(0x6060, 1, 1), r.now) == O::BusAdmission::INVALID);
    CHECK(r.wire.sentCount == 0);
    L::ReadContext state;
    r.readState(state, ACCEPTED_MASK); // Their bounded replies are the service proof.
    const C::NodeSnapshot& node = *r.services.node(0);
    CHECK(node.serviceQualification.receiveBoundsKnown && node.serviceFromSdo);
    CHECK(!node.heartbeat.present && !node.nmtTime.present);
    C::ServiceEligibility service;
    CHECK(C::serviceEligibility(node, r.now, service) && service.ordinarySdo && !service.exactNmtKnown);
    r.readState(state); // Now the full read, 1001 and 603F included, is ordinary work too.
    // The one setting the enable allows before it: 6060=1, acknowledged.
    const unsigned settings = r.wire.sentCount;
    CHECK(r.write(0x6060, 1, 1) == O::BusAdmission::ACCEPTED);
    CHECK(downloadsFrom(r, settings) == 1 && r.drive.mode == 1);
    r.readState(state);

    r.startFrame = r.wire.sentCount;
    CHECK(r.a.requestEnable(state, r.bound(), options(), r.now + 400000, r.now).isOk());
    for (unsigned i = 0; i < 400 && r.a.enableActive(); ++i)
        r.step();
    const L::ActionContext& enable = r.a.enableAction();
    CHECK(enable.state == M::ActionState::SUCCEEDED && enable.outcome == M::ActionOutcome::OBSERVED);
    const uint16_t words[] = {6, 7, 15};
    for (unsigned w = 0; w < 3; ++w)
        CHECK(enable.commandHistory[w].outcome == C::SdoOutcome::SUCCESS &&
              enable.commandHistory[w].transfer.data[0] == words[w]);
    CHECK(downloadsFrom(r, r.startFrame) == 3);

    E::StopPolicy policy;
    policy.options = options();
    policy.deadlineUs = r.now + 20000;
    policy.bestEffortDeadlineUs = r.now + 30000;
    const unsigned stopFrom = r.wire.sentCount;
    CHECK(r.a.requestStop(L::StopKind::HALT, enable.read, policy, r.now) == E::StopPath::CONFIRMED);
    r.steps(2);
    CHECK(wordAt(r, stopFrom, 0x010f)); // Sequenced, not the 000B best-effort word.
    for (unsigned i = 0; i < 200 && r.a.stopActive(); ++i)
        r.step();
    CHECK(r.a.stopAction(L::StopKind::HALT).commandHistory[0].outcome == C::SdoOutcome::SUCCESS);
    CHECK(downloadsFrom(r, stopFrom) == 1);
    C::SdoCompletion ignored;
    while (r.a.settle(ignored)) {
    }

    // A target write, as the move's first native write would go, is admitted on the same proof.
    const unsigned moveFrom = r.wire.sentCount;
    CHECK(r.write(0x607a, 4, 16) == O::BusAdmission::ACCEPTED);
    CHECK(downloadsFrom(r, moveFrom) == 1);
    // Quiet past the age policy: the lower bound is older, so writes wait for a new read.
    const uint64_t proof = node.serviceQualification.receivedEarliestUs;
    r.turn(proof + SERVICE_AGE + 1);
    CHECK(C::serviceEligibility(node, r.now, service) && !service.ordinarySdo);
    CHECK(r.a.admitPrepared(r.download(0x607a, 4, 16), r.now) == O::BusAdmission::INVALID);
    r.readState(state, ACCEPTED_MASK);
    CHECK(r.write(0x607a, 4, 16) == O::BusAdmission::ACCEPTED);
}

// 2. Nothing else is relaxed: unknown time, or an interval reaching back to the handoff, still
// qualifies no ordinary work, so the same session admits no write and no enable word.
void otherIngressStillRefuses() {
    for (unsigned form = unsigned(Ingress::AT_HANDOFF); form <= unsigned(Ingress::UNKNOWN); ++form) {
        Rig r(static_cast<Ingress>(form));
        CHECK(r.a.open(r.current(), L::SettingsModel::CL86_C, 1, r.now));
        L::ReadContext state;
        r.readState(state, ACCEPTED_MASK); // Accepted reads still work and settle.
        const C::ObservationTime& proof = r.services.node(0)->serviceQualification;
        if (form == unsigned(Ingress::AT_HANDOFF))
            CHECK(!proof.present && !r.services.node(0)->sdo.present);
        else // Unchanged: unknown time leaves a timeless record that never qualifies.
            CHECK(proof.present && !proof.receivedTimeKnown && !proof.receiveBoundsKnown);
        C::ServiceEligibility service;
        CHECK(C::serviceEligibility(*r.services.node(0), r.now, service) && !service.ordinarySdo);
        const unsigned from = r.wire.sentCount;
        CHECK(r.a.admitPrepared(r.download(0x6060, 1, 1), r.now) == O::BusAdmission::INVALID);
        {   // An UNQUALIFIED upload (1001, the full read's first) is ordinary work and refuses too.
            const uint64_t deadline = r.now + 400000;
            const uint32_t operation = r.a.beginOperation(deadline, r.now);
            CHECK(operation);
            L::ReadOptions reads;
            reads.queueTimeoutUs = QUEUE;
            reads.responseTimeoutUs = RESPONSE;
            static L::ReadContext full;
            CHECK(L::prepareState(full, r.current(), operation, r.now, deadline, reads, FULL_MASK));
            L::PreparedRead work;
            CHECK(L::nextRead(full, r.now, work) && work.ready && work.transfer.index == 0x1001);
            CHECK(r.a.admitPrepared(work.transfer, r.now) == O::BusAdmission::INVALID);
        }
        r.startFrame = r.wire.sentCount;
        const M::Status requested = r.a.requestEnable(state, r.bound(), options(), r.now + 400000, r.now);
        for (unsigned i = 0; i < 400 && r.a.enableActive(); ++i)
            r.step();
        CHECK(!requested.isOk() || r.a.enableAction().state == M::ActionState::FAILED);
        CHECK(downloadsFrom(r, from) == 0);
    }
}
} // namespace

int main() {
    boundsQualifyTheSessionsWrites();
    otherIngressStillRefuses();
    std::printf("bounded_service: %u checks\n", checks);
    return 0;
}
