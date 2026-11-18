// SPDX-License-Identifier: MIT
// Prompt 55 owner primitives: the TX lifetime and NO_LISTENER, the one recovery entry point and
// its causes, presence observations, boot-up invalidation, and stops before the owner's own
// resync marker. A synthetic controller and nodes; no drive, firmware or bus behavior is claimed.
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace {
unsigned checks = 0;
} // namespace
#define CHECK(condition)                                                                           \
    do {                                                                                           \
        ++checks;                                                                                  \
        if (!(condition)) {                                                                        \
            std::fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #condition);                   \
            std::abort();                                                                          \
        }                                                                                          \
    } while (false)
#include "support/OwnerRig.h"

namespace {
namespace M = MotorControlCAN;
namespace C = MotorControlCAN::CANOPEN;
namespace O = MotorControlCANOwner;
using OwnerRig::budget;
using OwnerRig::frame;
using OwnerRig::route;
constexpr bool PAIRED_CHANNELS = C::SdoClient::CHANNELS >= 2;
constexpr unsigned NODES = PAIRED_CHANNELS ? 2 : 1;
const uint64_t LIFETIME = O::CanBusOwner::DEFAULT_TX_LIFETIME_US;

// Adds explicit recovery callbacks to the shared scripted wire.
struct Wire : OwnerRig::WireBase {
    unsigned begins = 0, polls = 0;
    O::RecoveryProgress progress = O::RecoveryProgress::PENDING;
    C::SdoClient* holder = nullptr; // Holds each SDO handoff short of the SDK, as an adapter queue does.
    O::TransportTxId lastHeld;
    static bool send(void* context, const M::Frame& value, O::TransportTxId id, uint32_t epoch) {
        Wire& w = of(context);
        const bool accepted = WireBase::transmit(context, value, id, epoch);
        if (w.holder && id.kind == O::TransportTxId::Kind::SDO) {
            CHECK(w.holder->holdHandoff(id.sdo));
            w.lastHeld = id;
        }
        return accepted;
    }
    static Wire& of(void* context) { return *static_cast<Wire*>(static_cast<OwnerRig::WireBase*>(context)); }
    static bool begin(void* context, uint32_t) {
        ++of(context).begins;
        return true;
    }
    static O::RecoveryProgress poll(void* context, uint32_t) {
        Wire& w = of(context);
        ++w.polls;
        if (w.progress == O::RecoveryProgress::RETIRED) {
            // The fake deletes its controller: every local queue is empty, nothing replays.
            for (unsigned i = 0; i < w.sentCount; ++i)
                w.pending[i] = false;
            w.rxRead = w.rxWrite;
            w.reportRead = w.reportWrite;
        }
        return w.progress;
    }
    O::Transport transport() {
        O::Transport out = plumbing();
        out.transmit = send;
        out.beginRecovery = begin;
        out.pollRecovery = poll;
        return out;
    }
};
struct Rig {
    C::SdoClient services;
    Wire wire;
    O::CanBusOwner owner;
    uint64_t now = 0;
    unsigned kinds[9] = {}, steps[7] = {};
    O::RecoveryCauses lastCauses;
    explicit Rig(bool reported = true, uint32_t seed = 0) : owner(services, wire.transport(), 56, seed) {
        // Without automatic reports the fake holds each frame as an ACTIVE SDK frame does:
        // a stuck frame cannot be withdrawn, only a final report or recovery ends it.
        wire.autoReport = reported;
        wire.retirement = reported;
        for (unsigned node = 1; node <= NODES; ++node) {
            CHECK(services.configure(uint8_t(node - 1), route(node), true, true));
            C::NetworkRoute n;
            n.binding = route(node).binding;
            n.generation = 11;
            n.heartbeatId = 0x700 + node;
            n.emcyId = 0x80 + node;
            CHECK(services.configureNetwork(uint8_t(node - 1), n, C::NetworkObserver(), 0));
        }
    }
    void turn(uint64_t time, O::ServiceBudget b = budget()) {
        now = time;
        CHECK(owner.service(time, b));
        O::Diagnostic d;
        while (owner.takeDiagnostic(d)) {
            ++kinds[unsigned(d.kind)];
            if (d.kind == O::DiagnosticKind::RESYNC)
                ++steps[unsigned(d.resync)];
            if (d.kind == O::DiagnosticKind::RECOVERY)
                lastCauses = d.recoveryCauses;
        }
        CHECK(owner.stats().traceOverwritten == 0);
    }
    O::SequenceId reserve(unsigned node, uint32_t operation = 1) {
        O::SequenceId id;
        CHECK(owner.beginSequence(services.channel(uint8_t(node - 1))->binding, uint8_t(node), now, id));
        CHECK(owner.beginOperation(id, operation, now + 10000000, now));
        return id;
    }
    O::BusRequest read(O::SequenceId s, unsigned node, uint16_t index = 0x6041) const {
        const O::Sequence* q = owner.sequence(s);
        CHECK(q);
        O::BusRequest r;
        r.sequence = s;
        r.transfer.channel = *services.channel(uint8_t(node - 1));
        r.transfer.operation = r.transfer.request = q->operation;
        r.transfer.step = 1;
        r.transfer.index = index;
        r.transfer.size = 2;
        r.transfer.direction = C::SdoDirection::UPLOAD;
        r.transfer.readEffect = C::SdoReadEffect::ACCEPTED_UNQUALIFIED;
        r.transfer.queueTimeoutUs = r.transfer.responseTimeoutUs = 1000;
        r.transfer.deadlineUs = q->deadlineUs;
        return r;
    }
    O::RequestId admit(const O::BusRequest& r, bool urgent = false) {
        O::RequestId id;
        CHECK((urgent ? owner.admitUrgent(r, now, id) : owner.admit(r, now, id)) ==
              O::BusAdmission::ACCEPTED);
        return id;
    }
    const O::Completion& done(O::RequestId id) const {
        const O::Completion* c = owner.result(id);
        CHECK(c);
        return *c;
    }
    O::Health health(unsigned node) const {
        O::Health h;
        CHECK(owner.health(services.channel(uint8_t(node - 1))->binding, h));
        return h;
    }
    M::Frame upload(unsigned node, uint16_t index, uint32_t value, uint8_t width = 2) const {
        M::Frame f = frame(0x580 + node, 8);
        f.data[0] = uint8_t(0x43 | ((4 - width) << 2));
        f.data[1] = uint8_t(index);
        f.data[2] = uint8_t(index >> 8);
        for (unsigned i = 0; i < width; ++i)
            f.data[4 + i] = uint8_t(value >> (8 * i));
        return f;
    }
    void push(const M::Frame& f) { wire.push(f, now, owner.controllerEpoch()); }
    M::Frame heartbeat(unsigned node, uint8_t state) const {
        M::Frame f = frame(0x700 + node, 1);
        f.data[0] = state;
        return f;
    }
    C::SdoTransfer stop(unsigned node) const {
        C::SdoTransfer out;
        out.channel = *services.channel(uint8_t(node - 1));
        out.operation = out.request = out.step = 901;
        out.index = 0x6040;
        out.size = 2;
        out.direction = C::SdoDirection::DOWNLOAD;
        out.data[0] = 0x0b;
        out.queueTimeoutUs = out.responseTimeoutUs = 1000;
        out.deadlineUs = now + 5000;
        return out;
    }
    C::SdoTransfer marker(unsigned node) const {
        C::SdoTransfer out;
        out.channel = *services.channel(uint8_t(node - 1));
        out.operation = out.request = out.step = 902;
        out.index = 0x1000;
        out.size = 4;
        out.direction = C::SdoDirection::UPLOAD;
        out.readEffect = C::SdoReadEffect::ACCEPTED_UNQUALIFIED;
        out.allowSegmentedUpload = false;
        out.queueTimeoutUs = out.responseTimeoutUs = 1000;
        out.deadlineUs = now + 5000;
        return out;
    }
};
bool frameIs(const M::Frame& f, uint32_t id, uint8_t command, uint16_t index, uint8_t subindex = 0) {
    return f.id == id && f.dlc == 8 && f.data[0] == command && f.data[1] == uint8_t(index) &&
           f.data[2] == uint8_t(index >> 8) && f.data[3] == subindex;
}
// One read on node 1 sent at time 0 and never reported or answered: a stuck frame.
O::RequestId stuck(Rig& r) {
    const O::RequestId id = r.admit(r.read(r.reserve(1), 1));
    r.turn(0);
    CHECK(r.wire.sentCount == 1 && r.wire.pending[0]);
    return id;
}

void lifetimeExpiresWithoutResendOrRestart() {
    Rig r(false);
    CHECK(r.owner.txLifetimeUs() == LIFETIME && LIFETIME == 100000);
    const O::RequestId id = stuck(r);
    r.turn(LIFETIME - 1);
    // Its request timed out at its own response cutoff long before; that is not an expiry.
    CHECK(r.done(id).service.outcome == C::SdoOutcome::TIMEOUT && r.done(id).service.transmissionPossible);
    CHECK(!r.owner.noListener() && r.owner.stats().txExpired == 0 && r.owner.resources().txExpired == 0);
    r.turn(LIFETIME);
    CHECK(r.owner.noListener() && r.owner.resources().noListener && r.health(1).noListener);
    CHECK(r.owner.stats().txExpired == 1 && r.owner.resources().txExpired == 1);
    CHECK(r.kinds[unsigned(O::DiagnosticKind::TX_EXPIRED)] == 1);
    CHECK(r.kinds[unsigned(O::DiagnosticKind::LISTENER)] == 1);
    // It stays possibly sent and keeps its channel: no resend, no restart, reported once.
    for (unsigned i = 0; i < 20; ++i)
        r.turn(r.now + LIFETIME);
    CHECK(r.wire.sentCount == 1 && r.wire.begins == 0 && r.owner.stats().txExpired == 1);
    CHECK(!r.owner.localTxRetired(0) && r.services.unresolved(0) && r.owner.noListener());
    CHECK(r.owner.stats().recoveries == 0 && !r.owner.recoveryActive());
    // Ordinary work fails fast with nothing sent; urgent work stays admissible, and one ACK of
    // another frame ends NO_LISTENER. That is controller evidence only.
    if (PAIRED_CHANNELS) {
        const O::SequenceId other = r.reserve(2);
        O::RequestId refused;
        refused.generation = 99;
        const uint32_t rejected = r.owner.stats().queueRejected;
        CHECK(r.owner.admit(r.read(other, 2), r.now, refused) == O::BusAdmission::NO_LISTENER);
        CHECK(refused.generation == 99 && r.owner.stats().queueRejected == rejected + 1);
        const O::RequestId urgent = r.admit(r.read(other, 2), true);
        r.turn(r.now + 1);
        CHECK(r.wire.sentCount == 2 && frameIs(r.wire.sent[1], 0x602, 0x40, 0x6041));
        CHECK(r.owner.noListener());
        r.wire.report(1);
        r.turn(r.now + 1);
        CHECK(!r.owner.noListener() && r.kinds[unsigned(O::DiagnosticKind::LISTENER)] == 2);
        CHECK(r.services.unresolved(0) && r.owner.resources().txExpired == 1); // Node 1 unchanged.
        r.push(r.upload(2, 0x6041, 0x0237));
        r.turn(r.now + 1);
        CHECK(r.done(urgent).service.outcome == C::SdoOutcome::SUCCESS);
        O::BusRequest next = r.read(other, 2);
        next.transfer.step = 2;
        CHECK(r.owner.admit(next, r.now, refused) == O::BusAdmission::ACCEPTED); // Ordinary again.
    }
}

void lifetimeSettingAndReportRaces() {
    {   // Zero disables the check; a smaller lifetime reports earlier.
        Rig r(false);
        CHECK(r.owner.setTxLifetime(0, 0) && r.owner.txLifetimeUs() == 0);
        stuck(r);
        r.turn(10 * LIFETIME);
        CHECK(!r.owner.noListener() && r.owner.stats().txExpired == 0);
        CHECK(r.owner.setTxLifetime(500, r.now));
        r.turn(r.now + 1);
        CHECK(r.owner.noListener() && r.owner.stats().txExpired == 1); // Its handoff was long ago.
        CHECK(!r.owner.setTxLifetime(1, r.now - 1)); // Backward time changes nothing.
        CHECK(r.owner.txLifetimeUs() == 500);
    }
    {   // A report taken in the same turn as the lifetime boundary prevents the expiry.
        Rig r(false);
        stuck(r);
        r.wire.report(0);
        r.turn(LIFETIME);
        CHECK(!r.owner.noListener() && r.owner.stats().txExpired == 0);
        // The request's own response timeout staged its abort, which left once the channel freed.
        CHECK(r.wire.sentCount == 2 && frameIs(r.wire.sent[1], 0x601, 0x80, 0x6041));
    }
    {   // A late report after expiry retires the frame and ends NO_LISTENER; the count stays.
        Rig r(false);
        stuck(r);
        r.turn(LIFETIME);
        CHECK(r.owner.noListener());
        r.wire.report(0);
        r.turn(r.now + 1);
        CHECK(!r.owner.noListener() && r.owner.resources().txExpired == 0 && r.owner.stats().txExpired == 1);
        // Never the read again; only its abort, still within the abort's own queue budget.
        CHECK(r.wire.sentCount == 2 && frameIs(r.wire.sent[1], 0x601, 0x80, 0x6041));
        CHECK(r.services.unresolved(0));
    }
    {   // A received frame ends it too, but only one whose capture follows its start (M2).
        Rig r(false);
        stuck(r);
        r.turn(LIFETIME);
        r.wire.push(r.heartbeat(1, 0x7f), 50, r.owner.controllerEpoch()); // Long before.
        r.wire.push(r.heartbeat(1, 0x7f), LIFETIME, r.owner.controllerEpoch()); // Not after.
        r.turn(r.now + 1);
        CHECK(r.owner.noListener());
        r.wire.push(r.heartbeat(1, 0x7f), r.now, r.owner.controllerEpoch());
        r.turn(r.now + 1);
        CHECK(!r.owner.noListener() && r.owner.resources().txExpired == 1);
    }
    if (PAIRED_CHANNELS) {
        // M3: ordinary work queued before NO_LISTENER does not start during it; its own queue
        // cutoff still runs, and urgent work still goes.
        Rig r(false);
        stuck(r);
        const O::SequenceId other = r.reserve(2);
        O::BusRequest queuedRead = r.read(other, 2);
        queuedRead.transfer.queueTimeoutUs = 200000;
        const O::RequestId queued = r.admit(queuedRead);
        r.turn(LIFETIME, budget(8, 2, 8, 0)); // The expiry turn, with no start budget.
        CHECK(r.owner.noListener() && r.wire.sentCount == 1);
        for (unsigned i = 0; i < 5; ++i)
            r.turn(r.now + 1000);
        CHECK(r.wire.sentCount == 1 && !r.owner.result(queued) && r.owner.resources().queued == 1);
        r.turn(200000);
        CHECK(r.done(queued).reason == O::CompletionReason::QUEUE_EXPIRED && r.wire.sentCount == 1);
        CHECK(!r.done(queued).service.transmissionPossible);
    }
    if (PAIRED_CHANNELS) {
        // TX-done racing a stop: the stuck frame's channel is inhibited, the other node's stop goes
        // once, and both reports in one turn end NO_LISTENER without any resend.
        Rig r(false);
        stuck(r);
        r.turn(LIFETIME);
        O::RequestId blocked;
        CHECK(r.owner.admitBestEffortStop(r.stop(1), r.now, blocked) == O::BusAdmission::INHIBITED);
        O::RequestId stop;
        CHECK(r.owner.admitBestEffortStop(r.stop(2), r.now, stop) == O::BusAdmission::ACCEPTED);
        r.turn(r.now + 1);
        CHECK(r.wire.sentCount == 2 && frameIs(r.wire.sent[1], 0x602, 0x2b, 0x6040));
        r.wire.report(0);
        r.wire.report(1);
        r.turn(r.now + 1);
        CHECK(!r.owner.noListener() && r.done(stop).service.outcome == C::SdoOutcome::UNCONFIRMED_SENT);
        for (unsigned i = 0; i < 5; ++i)
            r.turn(r.now + 100);
        // The freed channel sends only the read's staged abort; the stop is never resent.
        CHECK(r.wire.sentCount == 3 && frameIs(r.wire.sent[2], 0x601, 0x80, 0x6041));
        CHECK(r.owner.admitBestEffortStop(r.stop(1), r.now, blocked) == O::BusAdmission::ACCEPTED);
    }
}

// R1: a frame an adapter holds behind another has not started its lifetime; the lifetime runs
// from the SDK submission the adapter reports.
void lifetimeStartsAtSubmission() {
    {   // Held until its own queue cutoff ended it: it never reached the SDK, so never expires.
        Rig r(false);
        r.wire.holder = &r.services;
        const O::RequestId id = r.admit(r.read(r.reserve(1), 1));
        r.turn(0);
        r.turn(1000);
        CHECK(r.done(id).service.outcome == C::SdoOutcome::TIMEOUT && !r.done(id).service.transmissionPossible);
        r.turn(10 * LIFETIME);
        CHECK(!r.owner.noListener() && r.owner.stats().txExpired == 0);
    }
    Rig r(false);
    r.wire.holder = &r.services;
    O::BusRequest held = r.read(r.reserve(1), 1);
    held.transfer.queueTimeoutUs = 200000; // It may wait in the adapter's queue that long.
    const O::RequestId id = r.admit(held);
    r.turn(0);
    CHECK(r.wire.sentCount == 1);
    r.turn(LIFETIME);
    CHECK(!r.owner.noListener() && r.owner.stats().txExpired == 0); // Held: not started.
    const uint64_t submitted = LIFETIME + 10;
    CHECK(r.services.noteSubmitted(r.wire.lastHeld.sdo, submitted));
    r.turn(submitted + LIFETIME - 1);
    CHECK(!r.owner.noListener() && r.owner.stats().txExpired == 0);
    r.turn(submitted + LIFETIME);
    CHECK(r.owner.noListener() && r.owner.stats().txExpired == 1);
    CHECK(r.done(id).service.transmissionPossible);
}

// The explicit recovery is the one restart: single flight, one begin per call, causes kept.
void recoveryIsTheOneEntryPoint() {
    {   // An expired frame: NO_LISTENER survives the restart, the frame is retired as possibly
        // sent, and a drain marker stays admissible so that its ACK can end NO_LISTENER.
        Rig r(false);
        const O::RequestId id = stuck(r);
        r.turn(LIFETIME);
        O::RecoveryId recovery, second;
        CHECK(r.owner.recover(r.now, r.now + 1000, recovery) && r.wire.begins == 1);
        CHECK(!r.owner.recover(r.now, r.now + 1000, second) && r.wire.begins == 1);
        r.wire.progress = O::RecoveryProgress::RETIRED;
        r.turn(r.now + 1, budget(0, 0, 0, 0, 1));
        const O::RecoveryCompletion* closed = r.owner.recoveryResult(recovery);
        CHECK(closed && closed->outcome == O::RecoveryOutcome::RETIRED);
        CHECK(closed->causes.txExpired && !closed->causes.busOff && !closed->causes.receiveLost);
        CHECK(r.lastCauses.txExpired && r.kinds[unsigned(O::DiagnosticKind::RECOVERY)] == 1);
        CHECK(r.owner.resources().txExpired == 0 && r.owner.localTxRetired(0));
        CHECK(r.done(id).service.transmissionPossible && r.services.unresolved(0));
        CHECK(r.owner.noListener() && r.wire.sentCount == 1);
        CHECK(!r.owner.recover(r.now, r.now + 1000, second)); // Unread result: still single flight.
        CHECK(r.owner.releaseRecovery(recovery));
        if (PAIRED_CHANNELS) {
            O::RequestId refused;
            CHECK(r.owner.admit(r.read(r.reserve(2), 2), r.now, refused) == O::BusAdmission::NO_LISTENER);
        }
        O::RequestId marker;
        CHECK(r.owner.admitDrainMarker(r.marker(1), r.now, marker) == O::BusAdmission::ACCEPTED);
        r.turn(r.now + 1);
        CHECK(r.wire.sentCount == 2 && frameIs(r.wire.sent[1], 0x601, 0x40, 0x1000));
        r.wire.report(1);
        r.turn(r.now + 1);
        CHECK(!r.owner.noListener() && r.services.unresolved(0)); // No identity or route freshness.
        r.push(r.upload(1, 0x1000, 0x00020192, 4));
        r.turn(r.now + 1);
        CHECK(r.done(marker).service.outcome == C::SdoOutcome::SUCCESS && !r.services.unresolved(0));
        // The next recovery starts from no cause: the retired one cleared them.
        CHECK(r.owner.release(marker) && r.owner.recover(r.now, r.now + 1000, second));
        r.turn(r.now + 1, budget(0, 0, 0, 0, 1));
        const O::RecoveryCauses none = r.owner.recoveryResult(second)->causes;
        CHECK(!none.txExpired && !none.busOff && !none.receiveLost && r.wire.begins == 2);
    }
    for (unsigned overflow = 0; overflow < 2; ++overflow) {
        // Bus-off and RX overflow report through the same owner and keep their own causes and
        // counters. Neither restarts anything by itself.
        Rig r;
        if (overflow)
            CHECK(r.owner.receiveOverflow(r.now, 3) && r.owner.stats().rxLost == 3);
        else
            CHECK(r.owner.reportController(C::ControllerState::BUS_OFF, r.now) &&
                  r.owner.stats().busOffEvents == 1);
        CHECK(r.owner.resources().inhibited);
        for (unsigned i = 0; i < 10; ++i)
            r.turn(r.now + LIFETIME);
        CHECK(r.wire.begins == 0 && r.wire.sentCount == 0);
        O::RecoveryId recovery;
        CHECK(r.owner.recover(r.now, r.now + 1000, recovery) && r.wire.begins == 1);
        r.wire.progress = O::RecoveryProgress::RETIRED;
        r.turn(r.now + 1, budget(0, 0, 0, 0, 1));
        const O::RecoveryCompletion* closed = r.owner.recoveryResult(recovery);
        CHECK(closed && closed->outcome == O::RecoveryOutcome::RETIRED && !r.owner.resources().inhibited);
        CHECK(closed->causes.busOff == !overflow && closed->causes.receiveLost == bool(overflow));
        CHECK(!closed->causes.txExpired && r.lastCauses.busOff == !overflow);
        CHECK(r.owner.stats().recoveries == 1 && r.owner.stats().rxLost == (overflow ? 3u : 0u));
        // Deliver item 5: each counter moves on its own event only.
        const O::Stats& s = r.owner.stats();
        CHECK(s.rxOverflows == overflow && s.busOffEvents == !overflow && s.txExpired == 0);
        CHECK(s.recoveriesStarted == 1 && s.recoveriesFailed == 0 && s.resyncs == 0 &&
              s.abortsSent == 0 && s.abortsReceived == 0);
        CHECK(!r.owner.resources().receiveLost && !r.health(1).notObserved);
    }
}

// R1: a restart before the request's own response timeout closes it CANCELLED; the retired
// recovery still gives the route its one resync step, so the restarted interface resyncs.
void restartBeforeResponseTimeoutResyncs() {
    Rig r(false);
    O::BusRequest slow = r.read(r.reserve(1), 1);
    slow.transfer.responseTimeoutUs = 200000; // Longer than the TX lifetime.
    const O::RequestId id = r.admit(slow);
    r.turn(0);
    r.turn(LIFETIME);
    CHECK(r.owner.noListener() && !r.owner.result(id));
    O::RecoveryId recovery;
    CHECK(r.owner.recover(r.now, r.now + 1000, recovery));
    r.wire.progress = O::RecoveryProgress::RETIRED;
    r.turn(r.now + 1, budget(0, 0, 0, 0, 1));
    CHECK(r.owner.releaseRecovery(recovery));
    CHECK(r.done(id).service.outcome == C::SdoOutcome::CANCELLED && r.done(id).reason == O::CompletionReason::RECOVERY);
    CHECK(r.services.unresolved(0) && r.health(1).resyncPending);
    r.wire.autoReport = r.wire.retirement = true;
    // NO_LISTENER outlived the restart: no automatic marker becomes a new unacknowledged frame.
    for (unsigned i = 0; i < 5; ++i)
        r.turn(r.now + 1000);
    CHECK(r.owner.noListener() && r.wire.sentCount == 1 && r.health(1).resyncPending);
    // A drive's heartbeat ends it; the step goes in the next turn with budget.
    r.wire.push(r.heartbeat(1, 0x7f), r.now + 1, r.owner.controllerEpoch());
    r.turn(r.now + 1, budget(8, 0, 8, 0));
    CHECK(!r.owner.noListener() && r.wire.sentCount == 1);
    r.turn(r.now + 1);
    CHECK(r.wire.sentCount == 2 && frameIs(r.wire.sent[1], 0x601, 0x40, 0x1000));
    CHECK(r.owner.stats().markers == 1 && !r.health(1).resyncPending);
    // A receive overflow closes the work BINDING_INVALIDATED, which arms nothing by itself; the
    // retired recovery still gives the route it left unresolved with possible TX its step.
    Rig o(false);
    const O::RequestId lost = o.admit(o.read(o.reserve(1), 1));
    o.turn(0);
    CHECK(o.owner.receiveOverflow(1, 1));
    CHECK(o.done(lost).service.outcome == C::SdoOutcome::BINDING_INVALIDATED);
    CHECK(o.done(lost).service.transmissionPossible && o.services.unresolved(0) && !o.health(1).resyncPending);
    CHECK(o.owner.recover(2, 1000, recovery));
    o.wire.progress = O::RecoveryProgress::RETIRED;
    o.turn(3, budget(0, 0, 0, 0, 1));
    CHECK(o.owner.recoveryResult(recovery)->causes.receiveLost && o.owner.releaseRecovery(recovery));
    o.wire.autoReport = o.wire.retirement = true;
    o.turn(4);
    CHECK(o.wire.sentCount == 2 && frameIs(o.wire.sent[1], 0x601, 0x40, 0x1000));
}

// One-turn grace: no resync step starts in the turn whose poll retired a recovery, so a stop the
// application holds through the recovery can be admitted first; without one, the marker goes next.
void markerWaitsOneTurnAfterRecovery() {
    for (unsigned withStop = 0; withStop < 2; ++withStop) {
        Rig r;
        const O::RequestId lost = r.admit(r.read(r.reserve(1), 1));
        r.turn(0);
        CHECK(r.wire.sentCount == 1);
        CHECK(r.owner.reportController(C::ControllerState::BUS_OFF, 1));
        CHECK(r.done(lost).service.outcome == C::SdoOutcome::BUS_OFF && r.services.unresolved(0));
        O::RecoveryId recovery;
        CHECK(r.owner.recover(2, 1000, recovery));
        r.wire.progress = O::RecoveryProgress::RETIRED;
        r.turn(3, budget(8, 2, 8, 2, 1)); // The closure turn, with TX and start budget.
        CHECK(r.owner.recoveryResult(recovery)->outcome == O::RecoveryOutcome::RETIRED);
        CHECK(r.wire.sentCount == 1 && r.owner.stats().markers == 0 && r.health(1).resyncPending);
        CHECK(r.owner.releaseRecovery(recovery));
        if (withStop) {
            O::RequestId stop;
            CHECK(r.owner.admitBestEffortStop(r.stop(1), r.now, stop) == O::BusAdmission::ACCEPTED);
            r.turn(4);
            CHECK(r.wire.sentCount == 2 && frameIs(r.wire.sent[1], 0x601, 0x2b, 0x6040));
            for (unsigned i = 0; i < 5; ++i)
                r.turn(r.now + 1000);
            CHECK(r.wire.sentCount == 2 && r.owner.stats().markers == 0); // The stop ended the step.
        } else {
            r.turn(4);
            CHECK(r.wire.sentCount == 2 && frameIs(r.wire.sent[1], 0x601, 0x40, 0x1000));
            CHECK(r.owner.stats().markers == 1 && !r.health(1).resyncPending);
        }
    }
}

// No automatic resync step starts during NO_LISTENER: a timed-out route on node 2 keeps its armed
// step while node 1's frame is stuck, and an explicit marker on node 2 stays admissible.
void noResyncDuringNoListener() {
    if (!PAIRED_CHANNELS)
        return;
    for (unsigned explicitMarker = 0; explicitMarker < 2; ++explicitMarker) {
        Rig r(false);
        stuck(r); // Node 1's frame, never reported.
        const O::RequestId lost = r.admit(r.read(r.reserve(2), 2));
        r.turn(1);
        CHECK(r.wire.sentCount == 2);
        r.wire.report(1); // Node 2's read left; its reply never comes.
        r.turn(1001);
        CHECK(r.done(lost).service.outcome == C::SdoOutcome::TIMEOUT && r.health(2).resyncPending);
        CHECK(r.wire.sentCount == 3 && frameIs(r.wire.sent[2], 0x602, 0x80, 0x6041));
        r.wire.report(2); // Its abort left too: local TX on node 2's route is retired.
        r.turn(LIFETIME); // Node 1's frame outlives the lifetime first.
        CHECK(r.owner.noListener() && r.owner.localTxRetired(1));
        CHECK(r.wire.sentCount == 3 && r.owner.stats().markers == 0 && r.health(2).resyncPending);
        if (explicitMarker) {
            O::RequestId probe;
            CHECK(r.owner.admitDrainMarker(r.marker(2), r.now, probe) == O::BusAdmission::ACCEPTED);
            r.turn(r.now + 1);
            CHECK(r.wire.sentCount == 4 && frameIs(r.wire.sent[3], 0x602, 0x40, 0x1000));
            r.wire.report(3); // Acknowledged: NO_LISTENER ends.
            r.turn(r.now + 1);
            CHECK(!r.owner.noListener() && r.owner.stats().markers == 0);
            continue;
        }
        for (unsigned i = 0; i < 5; ++i)
            r.turn(r.now + 1000);
        CHECK(r.wire.sentCount == 3 && r.owner.stats().markers == 0);
        r.wire.push(r.heartbeat(2, 0x7f), r.now + 1, r.owner.controllerEpoch());
        r.turn(r.now + 1, budget(8, 0, 8, 0)); // Ends NO_LISTENER; no start budget this turn.
        CHECK(!r.owner.noListener() && r.wire.sentCount == 3);
        r.turn(r.now + 1);
        CHECK(r.wire.sentCount == 4 && frameIs(r.wire.sent[3], 0x602, 0x40, 0x1000));
        CHECK(r.owner.stats().markers == 1);
    }
}

// M5: a pending recovery reports no expiry, and a cause reported while it runs is kept for the
// next recovery rather than cleared by this one.
void causesDuringRecovery() {
    Rig r(false);
    stuck(r);
    O::RecoveryId recovery;
    CHECK(r.owner.recover(10, LIFETIME * 3, recovery));
    r.turn(LIFETIME + 10);
    CHECK(!r.owner.noListener() && r.owner.stats().txExpired == 0);
    CHECK(r.owner.receiveOverflow(r.now, 2) && r.owner.stats().rxLost == 2);
    r.wire.progress = O::RecoveryProgress::RETIRED;
    r.turn(r.now + 1, budget(0, 0, 0, 0, 1));
    const O::RecoveryCompletion* first = r.owner.recoveryResult(recovery);
    CHECK(first && first->outcome == O::RecoveryOutcome::RETIRED);
    CHECK(!first->causes.receiveLost && !first->causes.txExpired); // Both arose during it.
    CHECK(r.owner.releaseRecovery(recovery));
    O::RecoveryId second;
    CHECK(r.owner.recover(r.now, r.now + 1000, second));
    r.turn(r.now + 1, budget(0, 0, 0, 0, 1));
    CHECK(r.owner.recoveryResult(second)->causes.receiveLost && r.lastCauses.receiveLost);
    // A failed recovery hands its causes on too.
    CHECK(r.owner.releaseRecovery(second) && r.owner.reportController(C::ControllerState::BUS_OFF, r.now));
    r.wire.progress = O::RecoveryProgress::FAILED;
    O::RecoveryId failed, next;
    CHECK(r.owner.recover(r.now, r.now + 1000, failed));
    r.turn(r.now + 1, budget(0, 0, 0, 0, 1));
    CHECK(r.owner.recoveryResult(failed)->outcome == O::RecoveryOutcome::FAILED);
    CHECK(r.owner.recoveryResult(failed)->causes.busOff && r.owner.releaseRecovery(failed));
    r.wire.progress = O::RecoveryProgress::RETIRED;
    CHECK(r.owner.recover(r.now, r.now + 1000, next));
    r.turn(r.now + 1, budget(0, 0, 0, 0, 1));
    CHECK(r.owner.recoveryResult(next)->causes.busOff);
}

// After a restart retired a stuck frame, the no-marker setting reopens the timed-out route with
// nothing sent: the recovery's retirement reaches the adapter like a final report would.
void noMarkerReopensAfterRestart() {
    Rig r(false);
    O::SdoReuse reuse;
    reuse.policy = O::SdoReusePolicy::ACCEPTED_RISK_WITHOUT_MARKER;
    CHECK(r.owner.setSdoReuse(r.services.channel(0)->binding, reuse, 0));
    const O::RequestId id = stuck(r);
    r.turn(LIFETIME);
    CHECK(r.done(id).service.outcome == C::SdoOutcome::TIMEOUT && r.health(1).resyncPending);
    O::RecoveryId recovery;
    CHECK(r.owner.recover(r.now, r.now + 1000, recovery));
    r.wire.progress = O::RecoveryProgress::RETIRED;
    r.turn(r.now + 1, budget(0, 0, 0, 0, 1));
    CHECK(r.owner.recoveryResult(recovery)->outcome == O::RecoveryOutcome::RETIRED);
    CHECK(r.services.unresolved(0) && r.owner.releaseRecovery(recovery));
    r.turn(r.now + 1);
    CHECK(r.owner.noListener() && r.services.unresolved(0)); // No step during NO_LISTENER.
    r.wire.push(r.heartbeat(1, 0x7f), r.now + 1, r.owner.controllerEpoch());
    r.turn(r.now + 1);
    CHECK(!r.owner.noListener());
    r.turn(r.now + 1);
    CHECK(!r.services.unresolved(0) && r.services.requalifications(0) == 1 && r.wire.sentCount == 1);
    CHECK(r.steps[unsigned(O::ResyncStep::REOPENED)] == 1 && r.health(1).acceptedCorrelation);
    CHECK(r.health(1).reusePolicy == O::SdoReusePolicy::ACCEPTED_RISK_WITHOUT_MARKER);
}

// Observations only, per node: last response, last heartbeat, boot-ups and the no-response streak.
void presenceOfTwoNodes() {
    Rig r;
    const O::SequenceId one = r.reserve(1);
    const O::RequestId answered = r.admit(r.read(one, 1));
    O::Health fresh = r.health(1);
    CHECK(!fresh.lastResponseKnown && !fresh.lastHeartbeatKnown && !fresh.lastBootUpKnown);
    CHECK(fresh.bootUps == 0 && fresh.consecutiveNoResponse == 0);
    r.turn(0);
    r.push(r.upload(1, 0x6041, 0x0237));
    r.turn(5);
    CHECK(r.done(answered).service.outcome == C::SdoOutcome::SUCCESS);
    r.push(r.heartbeat(1, 0x7f));
    r.turn(7);
    O::Health h1 = r.health(1);
    CHECK(h1.lastResponseKnown && h1.lastResponseUs == 5 && h1.lastHeartbeatKnown && h1.lastHeartbeatUs == 7);
    CHECK(h1.consecutiveNoResponse == 0 && h1.bootUps == 0);
    if (PAIRED_CHANNELS) {
        // Node 2 is silent. Its read and then the owner's one resync marker each time out. Each
        // TX report is taken before the response timeout, as on a live bus (a report taken in
        // the timeout's own turn comes too late: the request is then a local end, rule C3).
        const O::RequestId silent = r.admit(r.read(r.reserve(2), 2));
        r.turn(10);
        r.turn(11);
        r.turn(1010);
        CHECK(r.done(silent).service.outcome == C::SdoOutcome::TIMEOUT);
        CHECK(r.done(silent).evidence == O::EvidenceClass::NO_ANSWER);
        CHECK(r.health(2).consecutiveNoResponse == 1 && !r.health(2).lastResponseKnown);
        r.turn(1011); // The abort's report: then the marker.
        CHECK(r.owner.stats().markers == 1);
        r.turn(1012);
        r.turn(2011);
        r.turn(2012);
        O::Health h2 = r.health(2);
        CHECK(h2.consecutiveNoResponse == 2 && !h2.lastResponseKnown && !h2.lastHeartbeatKnown);
        CHECK(h2.consecutiveFailures == 2 && r.wire.sentCount == 5); // Read, abort, marker, abort.
        CHECK(h2.totalMisses == 2 && h2.totalErrors == 0);
        // Both cleanup aborts were confirmed: node 2's, never node 1's.
        CHECK(h2.abortsSent == 2 && h2.lastAbortSentUs == 2012 && r.owner.stats().abortsSent == 2);
        CHECK(r.health(1).abortsSent == 0 && h2.abortsReceived == 0);
        // Node 1 is unaffected throughout.
        const O::Health same = r.health(1);
        CHECK(same.consecutiveNoResponse == 0 && same.lastResponseUs == 5 && same.lastHeartbeatUs == 7);
        // Any frame on node 2's response identifier ends its streak, even a late one.
        r.push(r.upload(2, 0x6041, 1));
        r.turn(3000);
        h2 = r.health(2);
        CHECK(h2.consecutiveNoResponse == 0 && h2.lastResponseKnown && h2.lastResponseUs == 3000);
        CHECK(r.services.unresolved(1)); // An observation, not a reopening.
    }
}

// Boot-up still invalidates the binding, its reservations and the accepted-correlation mark.
void bootUpInvalidation() {
    Rig r;
    const O::SequenceId first = r.reserve(1);
    const O::RequestId lost = r.admit(r.read(first, 1));
    r.turn(0);
    r.turn(1000);
    CHECK(r.done(lost).service.outcome == C::SdoOutcome::TIMEOUT && r.wire.sentCount == 2);
    r.turn(1001);
    CHECK(r.wire.sentCount == 3 && frameIs(r.wire.sent[2], 0x601, 0x40, 0x1000));
    r.push(r.upload(1, 0x1000, 0x00020192, 4));
    r.turn(1002);
    CHECK(!r.services.unresolved(0) && r.health(1).acceptedCorrelation);
    const O::SequenceId second = r.reserve(1, 2);
    const O::RequestId marked = r.admit(r.read(second, 1));
    r.turn(1003);
    r.push(r.upload(1, 0x6041, 3));
    r.turn(1004);
    CHECK(r.done(marked).service.outcome == C::SdoOutcome::SUCCESS && r.done(marked).acceptedCorrelation);
    const M::Generation generation = r.services.node(0)->route.generation;
    const unsigned sent = r.wire.sentCount;
    r.push(r.heartbeat(1, 0x00));
    r.turn(1005);
    const O::Health h = r.health(1);
    CHECK(h.bootUps == 1 && h.lastBootUpKnown && h.lastBootUpUs == 1005 && !h.acceptedCorrelation);
    CHECK(r.services.node(0)->route.generation == generation + 1 && r.services.unresolved(0));
    CHECK(!r.owner.sequence(second) && !h.resyncPending);
    for (unsigned i = 0; i < 5; ++i)
        r.turn(r.now + 1000);
    CHECK(r.wire.sentCount == sent && r.services.unresolved(0)); // Nothing reopens by itself.
}

// D1: a best-effort stop ends the node's pending resync step under every policy, and none follows
// it by itself, not even after its result is released. C1: it drops an unsent cleanup abort.
void stopEndsResync() {
    for (unsigned without = 0; without < 2; ++without) {
        Rig r;
        O::SdoReuse reuse;
        if (without)
            reuse.policy = O::SdoReusePolicy::ACCEPTED_RISK_WITHOUT_MARKER;
        CHECK(r.owner.setSdoReuse(r.services.channel(0)->binding, reuse, 0));
        const O::RequestId lost = r.admit(r.read(r.reserve(1), 1));
        r.turn(0);
        r.turn(1000, budget(8, 0, 8, 0)); // Timeout; its abort is staged, no TX budget.
        CHECK(r.done(lost).service.clientAbortQueued && r.health(1).resyncPending);
        O::RequestId stop;
        CHECK(r.owner.admitBestEffortStop(r.stop(1), r.now, stop) == O::BusAdmission::ACCEPTED);
        r.turn(1001);
        // The stop goes first and the unsent abort is dropped (C1); the step has ended (D1).
        CHECK(r.wire.sentCount == 2 && frameIs(r.wire.sent[1], 0x601, 0x2b, 0x6040));
        CHECK(!r.health(1).resyncPending);
        CHECK(!r.services.port(0)->queueOccupied);
        r.turn(1002);
        CHECK(r.done(stop).service.outcome == C::SdoOutcome::UNCONFIRMED_SENT);
        // The stop's own ACK arrives; nothing reopens the route by itself (D1).
        M::Frame ack = frame(0x581, 8);
        ack.data[0] = 0x60;
        ack.data[1] = 0x40;
        ack.data[2] = 0x60;
        r.push(ack);
        r.turn(1003);
        CHECK(r.owner.release(stop));
        for (unsigned i = 0; i < 5; ++i)
            r.turn(r.now + 1000);
        CHECK(r.wire.sentCount == 2 && r.services.unresolved(0) && r.owner.stats().markers == 0);
        CHECK(r.steps[unsigned(O::ResyncStep::REOPENED)] == 0 && !r.health(1).resyncPending);
        O::SequenceId refused;
        CHECK(!r.owner.beginSequence(r.services.channel(0)->binding, 3, r.now, refused));
        // An explicit marker is the application's intent and reopens it.
        O::RequestId marker;
        CHECK(r.owner.admitDrainMarker(r.marker(1), r.now, marker) == O::BusAdmission::ACCEPTED);
        r.turn(r.now + 1);
        r.push(r.upload(1, 0x1000, 0x00020192, 4));
        r.turn(r.now + 1);
        CHECK(!r.services.unresolved(0) && r.owner.release(marker));
    }
}

// M6: the stop never cancels the owner's marker when its own placement would fail.
void stopKeepsMarkerWhenPlacementFails() {
    Rig r(true, UINT32_MAX - 3); // The sequence, the read and the marker take the last identities.
    const O::RequestId lost = r.admit(r.read(r.reserve(1), 1));
    r.turn(0);
    r.turn(1000);
    CHECK(r.done(lost).service.outcome == C::SdoOutcome::TIMEOUT);
    r.turn(1001);
    CHECK(r.owner.stats().markers == 1 && r.wire.sentCount == 3);
    O::RequestId stop;
    CHECK(r.owner.admitBestEffortStop(r.stop(1), r.now, stop) == O::BusAdmission::IDS_EXHAUSTED);
    r.push(r.upload(1, 0x1000, 0x00020192, 4));
    r.turn(1002);
    CHECK(r.steps[unsigned(O::ResyncStep::MARKER_FAILED)] == 0 && !r.services.unresolved(0));
}

// M7: every NMT command is class M, refused under any other class, and its result says so.
void nmtIsMotionLike() {
    Rig r;
    C::NmtTransfer nmt;
    CHECK(nmt.retryClass == M::RetryClass::MOTION_LIKE);
    nmt.command = C::NmtCommand::START;
    nmt.bus = r.services.node(0)->route.binding.endpoint.bus;
    nmt.target = r.services.node(0)->route;
    nmt.request = 7;
    nmt.deadlineUs = 5000;
    nmt.queueTimeoutUs = 1000;
    C::NmtTransfer other = nmt;
    other.retryClass = M::RetryClass::NOT_REPEATABLE;
    C::NmtRequestId id;
    CHECK(!r.owner.admitNmt(other, 0, id));
    CHECK(r.owner.admitNmt(nmt, 0, id));
    r.turn(1);
    r.turn(2);
    const C::NmtCompletion* done = r.owner.nmtResult(id);
    CHECK(done && done->transfer.retryClass == M::RetryClass::MOTION_LIKE);
}

// Stops keep priority over every repair step: a best-effort stop cancels the owner's own marker.
void stopsGoBeforeTheOwnersMarker() {
    Rig r;
    const O::RequestId lost = r.admit(r.read(r.reserve(1), 1));
    r.turn(0);
    r.turn(1000);
    CHECK(r.done(lost).service.outcome == C::SdoOutcome::TIMEOUT);
    r.turn(1001);
    CHECK(r.wire.sentCount == 3 && r.owner.stats().markers == 1); // Read, abort, marker.
    O::RequestId stop;
    CHECK(r.owner.admitBestEffortStop(r.stop(1), r.now, stop) == O::BusAdmission::ACCEPTED);
    r.turn(1002); // The marker's frame reports, then the stop leaves.
    CHECK(r.steps[unsigned(O::ResyncStep::MARKER_FAILED)] == 1);
    CHECK(r.wire.sentCount == 4 && frameIs(r.wire.sent[3], 0x601, 0x2b, 0x6040));
    r.turn(1003);
    CHECK(r.done(stop).service.outcome == C::SdoOutcome::UNCONFIRMED_SENT);
    // A late marker reply is retained by the stop's route; no marker follows by itself.
    r.push(r.upload(1, 0x1000, 0x00020192, 4));
    for (unsigned i = 0; i < 5; ++i)
        r.turn(r.now + 1000);
    CHECK(r.wire.sentCount == 4 && r.services.unresolved(0) && r.owner.stats().markers == 1);
}
} // namespace

// Prompt 55, rule C2: NO_LISTENER is a bus-level fault. A confirmed read on node 2 that times out
// while it lasts is a local end: no miss, and the node rows keep their state, marked not
// observed. Without the fault (the lifetime check off) the same timeout is a miss.
void noListenerTimeoutIsNoMiss() {
    if (!PAIRED_CHANNELS)
        return;
    for (unsigned fault = 0; fault < 2; ++fault) {
        Rig r(false);
        if (!fault)
            CHECK(r.owner.setTxLifetime(0, 0));
        stuck(r); // Node 1's frame, never reported.
        const O::SequenceId two = r.reserve(2);
        const O::RequestId answered = r.admit(r.read(two, 2));
        r.turn(1);
        r.wire.report(1);
        r.push(r.upload(2, 0x6041, 1)); // An earlier answer of node 2: READY before the fault.
        r.turn(2);
        CHECK(r.done(answered).evidence == O::EvidenceClass::ANSWERED && r.owner.release(answered));
        CHECK(r.owner.beginOperation(two, 2, r.now + 10000000, r.now));
        O::BusRequest work = r.read(two, 2);
        work.transfer.responseTimeoutUs = 2 * LIFETIME;
        const O::RequestId id = r.admit(work);
        r.turn(3);
        CHECK(r.wire.sentCount == 3);
        r.wire.report(2); // Node 2's read was ACKed.
        r.turn(4);
        CHECK(r.health(2).state == O::DriverState::READY);
        r.turn(LIFETIME);
        CHECK(r.owner.noListener() == bool(fault));
        const O::OwnerResources during = r.owner.resources();
        CHECK(during.interfaceState ==
              (fault ? O::InterfaceState::OFFLINE : O::InterfaceState::AVAILABLE));
        CHECK(!fault || (during.offlineSinceUs == LIFETIME && r.health(2).notObserved &&
                         r.health(2).notObservedSinceUs == LIFETIME));
        r.turn(2 * LIFETIME + 3);
        CHECK(r.done(id).service.outcome == C::SdoOutcome::TIMEOUT &&
              r.done(id).service.transmissionConfirmed);
        CHECK(r.done(id).evidence ==
              (fault ? O::EvidenceClass::LOCAL_END : O::EvidenceClass::NO_ANSWER));
        const O::Health h = r.health(2);
        CHECK(h.consecutiveNoResponse == !fault && h.totalMisses == !fault &&
              h.totalFailures == !fault);
        // The bus fault marks no binding OFFLINE: node 2 keeps its last projection.
        CHECK(h.state == (fault ? O::DriverState::READY : O::DriverState::DEGRADED));
        CHECK(h.notObserved == bool(fault) && r.health(1).notObserved == bool(fault));
        if (!fault)
            continue;
        // A received frame ends NO_LISTENER: the rows are observed again.
        r.wire.push(r.heartbeat(1, 0x7f), r.now + 1, r.owner.controllerEpoch());
        r.turn(r.now + 1);
        CHECK(!r.owner.noListener() && !r.health(2).notObserved && !r.health(1).notObserved);
        CHECK(r.owner.resources().interfaceState == O::InterfaceState::AVAILABLE &&
              r.owner.resources().offlineSinceUs == 0);
    }
}

// Prompt 55, rule C2: bus-off ends a confirmed wait as a local end and freezes the rows; an RX
// overflow does the same, invalidates what the lost frames may have carried and lasts until a
// recovery retires.
void busOffAndOverflowFreezeRows() {
    for (unsigned overflow = 0; overflow < 2; ++overflow) {
        Rig r;
        const O::SequenceId one = r.reserve(1);
        const O::RequestId first = r.admit(r.read(one, 1));
        r.turn(0);
        r.push(r.upload(1, 0x6041, 0x0237));
        r.turn(1);
        CHECK(r.done(first).evidence == O::EvidenceClass::ANSWERED && r.owner.release(first));
        r.push(r.heartbeat(1, 0x7f));
        r.turn(2);
        CHECK(r.services.node(0)->heartbeat.present);
        CHECK(r.owner.beginOperation(one, 2, r.now + 10000000, r.now));
        const O::RequestId waiting = r.admit(r.read(one, 1));
        r.turn(3);
        r.turn(4); // Its TX report: the read waits, confirmed.
        CHECK(r.health(1).state == O::DriverState::READY && r.health(1).totalSuccess == 1);
        if (overflow)
            CHECK(r.owner.receiveOverflow(5, 2));
        else
            CHECK(r.owner.reportController(C::ControllerState::BUS_OFF, 5));
        const O::Completion& ended = r.done(waiting);
        CHECK(ended.service.outcome ==
              (overflow ? C::SdoOutcome::BINDING_INVALIDATED : C::SdoOutcome::BUS_OFF));
        CHECK(ended.evidence == O::EvidenceClass::LOCAL_END);
        // Evidence the lost frames could have carried is withdrawn: here the heartbeat.
        CHECK(!r.services.node(0)->heartbeat.present || !overflow);
        const O::Health h = r.health(1);
        CHECK(h.state == O::DriverState::READY && h.notObserved && h.notObservedSinceUs == 5);
        CHECK(h.totalFailures == 0 && h.consecutiveFailures == 0 && h.consecutiveNoResponse == 0);
        O::OwnerResources resources = r.owner.resources();
        CHECK(resources.interfaceState == O::InterfaceState::OFFLINE && resources.offlineSinceUs == 5);
        CHECK(resources.receiveLost == bool(overflow));
        CHECK(r.owner.stats().rxOverflows == overflow && r.owner.stats().busOffEvents == !overflow);
        CHECK(r.owner.stats().lastRxOverflowUs == (overflow ? 5u : 0u) &&
              r.owner.stats().lastBusOffUs == (overflow ? 0u : 5u));
        // A failed restart keeps the fault and the episode; a retired one ends both.
        O::RecoveryId failed, retired;
        r.wire.progress = O::RecoveryProgress::FAILED;
        CHECK(r.owner.recover(6, 1000, failed));
        r.turn(7, budget(0, 0, 0, 0, 1));
        CHECK(r.owner.recoveryResult(failed)->outcome == O::RecoveryOutcome::FAILED);
        CHECK(r.owner.stats().recoveriesFailed == 1 && r.owner.stats().lastRecoveryFailedUs == 7);
        CHECK(r.owner.resources().receiveLost == bool(overflow) && r.health(1).notObserved &&
              r.health(1).notObservedSinceUs == 5);
        CHECK(r.owner.releaseRecovery(failed));
        r.wire.progress = O::RecoveryProgress::RETIRED;
        CHECK(r.owner.recover(8, 1000, retired));
        r.turn(9, budget(0, 0, 0, 0, 1));
        CHECK(r.owner.recoveryResult(retired)->outcome == O::RecoveryOutcome::RETIRED);
        resources = r.owner.resources();
        CHECK(resources.interfaceState == O::InterfaceState::AVAILABLE && !resources.receiveLost);
        CHECK(!r.health(1).notObserved && r.health(1).state == O::DriverState::UNINIT);
        CHECK(r.owner.stats().recoveriesStarted == 2 && r.owner.stats().recoveries == 1 &&
              r.owner.stats().lastRecoveryUs == 9 && r.owner.stats().lastRecoveryStartedUs == 8);
        CHECK(r.health(1).totalSuccess == 1 && r.health(1).totalFailures == 0);
    }
}

// Deliver item 5: resyncs, TX expiries and aborts received count on their events, with times, in
// the interface and per node; clearStats clears them and nothing else.
void countersMoveOnTheirEvents() {
    Rig r;
    const O::RequestId lost = r.admit(r.read(r.reserve(1), 1));
    r.turn(0);
    r.turn(1);
    r.turn(1000);
    CHECK(r.done(lost).evidence == O::EvidenceClass::NO_ANSWER && r.owner.stats().resyncs == 0);
    r.turn(1001); // The abort's report, then the marker.
    CHECK(r.owner.stats().markers == 1 && r.owner.stats().abortsSent == 1);
    r.push(r.upload(1, 0x1000, 0x00020192, 4));
    r.turn(1002);
    r.turn(1003);
    CHECK(!r.services.unresolved(0) && r.steps[unsigned(O::ResyncStep::REOPENED)] == 1);
    const O::Stats& s = r.owner.stats();
    CHECK(s.resyncs == 1 && s.lastResyncUs >= 1002 && s.lastResyncUs <= 1003);
    O::Health h = r.health(1);
    CHECK(h.resyncs == 1 && h.lastResyncUs == s.lastResyncUs && h.abortsSent == 1);
    CHECK(s.txExpired == 0 && s.busOffEvents == 0 && s.rxOverflows == 0 && s.recoveriesStarted == 0);
    if (PAIRED_CHANNELS)
        CHECK(r.health(2).resyncs == 0 && r.health(2).abortsSent == 0);
    // A late abort on an idle route: received and counted, not a terminal.
    M::Frame abort = r.upload(1, 0x6041, 0);
    abort.data[0] = 0x80;
    r.push(abort);
    r.turn(1004);
    CHECK(s.abortsReceived == 1 && s.lastAbortReceivedUs == 1004 &&
          r.health(1).abortsReceived == 1 && r.health(1).lastAbortReceivedUs == 1004);
    // A TX expiry carries its time; clearStats clears every counter and node count, no streak.
    Rig e(false);
    stuck(e);
    e.turn(LIFETIME);
    CHECK(e.owner.stats().txExpired == 1 && e.owner.stats().lastTxExpiredUs == LIFETIME);
    CHECK(r.owner.clearStats() && s.resyncs == 0 && s.lastResyncUs == 0 && s.abortsSent == 0 &&
          s.abortsReceived == 0 && s.lastAbortReceivedUs == 0);
    h = r.health(1);
    CHECK(h.resyncs == 0 && h.abortsSent == 0 && h.abortsReceived == 0 && h.lastResyncUs == 0 &&
          h.consecutiveNoResponse == 0 && h.lastResponseKnown);
}

int main() {
    lifetimeExpiresWithoutResendOrRestart();
    lifetimeSettingAndReportRaces();
    lifetimeStartsAtSubmission();
    recoveryIsTheOneEntryPoint();
    restartBeforeResponseTimeoutResyncs();
    markerWaitsOneTurnAfterRecovery();
    noResyncDuringNoListener();
    causesDuringRecovery();
    noMarkerReopensAfterRestart();
    presenceOfTwoNodes();
    bootUpInvalidation();
    stopsGoBeforeTheOwnersMarker();
    stopEndsResync();
    stopKeepsMarkerWhenPlacementFails();
    nmtIsMotionLike();
    noListenerTimeoutIsNoMiss();
    busOffAndOverflowFreezeRows();
    countersMoveOnTheirEvents();
    std::printf("owner self-repair: %u checks\n", checks);
    return 0;
}
