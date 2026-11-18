// SPDX-License-Identifier: MIT
#include "LinkWatch.h"
#include <cstdio>

namespace MotorControlCANExample {
namespace C = MotorControlCAN::CANOPEN;
using MotorControlCANOwner::Health;
using MotorControlCANOwner::SdoReuse;
using MotorControlCANOwner::SdoReusePolicy;
using MotorControlCANOwner::Stats;
namespace {
const char* policyName(SdoReusePolicy policy) noexcept {
    return policy == SdoReusePolicy::NONE                       ? "none"
           : policy == SdoReusePolicy::ACCEPTED_RISK_WITH_MARKER ? "marker"
           : policy == SdoReusePolicy::ACCEPTED_RISK_WITHOUT_MARKER ? "nomarker"
                                                                     : "unknown";
}
const char* const EVENT_NAMES[] = {"expired", "restart", "restart-refused", "recovery", "listener-end",
                                   "resynced", "marker", "marker-result"};
// The watch's explicit markers: host-only identities far above every application read.
const uint32_t MARKER_IDENTITY_BASE = 0xD0000000u;
bool put(size_t size, int written) noexcept {
    return written > 0 && size_t(written) < size;
}
} // namespace

bool formatLink(const FirmwareProbe& firmware, const C::SdoClient& services, unsigned index, char* out,
                size_t size) noexcept {
    // FirmwareProbe exposes the owner only through its non-const accessor; nothing here changes it.
    MotorControlCANOwner::CanBusOwner& owner = const_cast<FirmwareProbe&>(firmware).owner();
    if (!out || !size)
        return false;
    if (index == 0) {
        const Stats& s = owner.stats();
        const MotorControlCANOwner::OwnerResources r = owner.resources();
        return put(size,
                   std::snprintf(out, size,
                                 "LINK interface lifetime=%llu noListener=%u expiredPending=%u txExpired=%lu "
                                 "markers=%lu reuseStopped=%lu recoveries=%lu inhibited=%u t=%llu\n",
                                 static_cast<unsigned long long>(owner.txLifetimeUs()), unsigned(owner.noListener()),
                                 unsigned(r.txExpired), static_cast<unsigned long>(s.txExpired),
                                 static_cast<unsigned long>(s.markers), static_cast<unsigned long>(s.reuseStopped),
                                 static_cast<unsigned long>(s.recoveries), unsigned(r.inhibited),
                                 static_cast<unsigned long long>(owner.lastTimeUs())));
    }
    if (index > BENCH_NODES)
        return false;
    const uint8_t slot = uint8_t(index - 1);
    const C::SdoChannel* channel = services.channel(slot);
    SdoReuse reuse;
    Health health;
    if (!channel || !owner.sdoReuse(channel->binding, reuse) || !owner.health(channel->binding, health))
        return put(size, std::snprintf(out, size, "LINK node=%u configured=0\n", index));
    return put(size,
               std::snprintf(out, size,
                             "LINK node=%u reuse=%s unresolved=%u stopped=%u pending=%u accepted=%u responded=%u "
                             "lastResponse=%llu heartbeat=%u lastHeartbeat=%llu bootUps=%lu noResponse=%u\n",
                             index, policyName(reuse.policy), unsigned(services.unresolved(slot)),
                             unsigned(health.reuseStopped), unsigned(health.resyncPending),
                             unsigned(health.acceptedCorrelation), unsigned(health.lastResponseKnown),
                             static_cast<unsigned long long>(health.lastResponseUs),
                             unsigned(health.lastHeartbeatKnown),
                             static_cast<unsigned long long>(health.lastHeartbeatUs),
                             static_cast<unsigned long>(health.bootUps), unsigned(health.consecutiveNoResponse)));
}

MotorControlCANOwner::BusAdmission admitExplicitMarker(FirmwareProbe& firmware, const C::SdoClient& services,
                                                       unsigned slot, uint64_t now, uint32_t identity,
                                                       MotorControlCANOwner::RequestId& output,
                                                       uint32_t& object) noexcept {
    const C::SdoChannel* channel = services.channel(uint8_t(slot));
    SdoReuse reuse;
    if (!channel || !firmware.owner().sdoReuse(channel->binding, reuse))
        return MotorControlCANOwner::BusAdmission::INVALID;
    C::SdoTransfer marker;
    marker.channel = *channel;
    marker.direction = C::SdoDirection::UPLOAD;
    marker.readEffect = C::SdoReadEffect::ACCEPTED_UNQUALIFIED;
    marker.uploadSize = C::SdoUploadSize::EXACT;
    marker.allowSegmentedUpload = false;
    marker.size = 4;
    marker.queueTimeoutUs = marker.responseTimeoutUs = 250000; // Host policy, the bench routes' own.
    marker.deadlineUs = now + 500000;
    marker.operation = marker.request = marker.step = identity;
    // The binding's marker list in order; the owner refuses an object that is retained on the route or
    // its last completed pair, and the next one is tried.
    MotorControlCANOwner::BusAdmission admitted = MotorControlCANOwner::BusAdmission::INVALID;
    for (unsigned i = 0; i < reuse.markerCount && i < SdoReuse::MARKER_OBJECTS; ++i) {
        marker.index = reuse.markers[i].index;
        marker.subindex = reuse.markers[i].subindex;
        marker.size = reuse.markers[i].size;
        admitted = firmware.owner().admitDrainMarker(marker, now, output);
        if (admitted == MotorControlCANOwner::BusAdmission::ACCEPTED)
            object = uint32_t(marker.index) << 8 | marker.subindex;
        if (admitted != MotorControlCANOwner::BusAdmission::INVALID)
            return admitted; // Accepted, or a no-TX refusal the operator can ask again after.
    }
    return admitted;
}

MarkerCommand::MarkerCommand(FirmwareProbe& firmware, C::SdoClient& services) noexcept
    : firmware_(firmware), services_(services) {}

bool MarkerCommand::begin(unsigned node, uint64_t now, char* out, size_t size) noexcept {
    const char* const admissions[] = {"accepted", "invalid", "expired", "results_full", "ids_exhausted",
                                      "inhibited", "no_listener"};
    if (!out || !size)
        return false;
    if (active_ || node < 1 || node > BENCH_NODES || !services_.unresolved(uint8_t(node - 1))) {
        std::snprintf(out, size, "MARKER refused node=%u reason=%s; nothing sent\n", node,
                      active_ ? "active" : node < 1 || node > BENCH_NODES ? "node" : "resolved");
        return false;
    }
    uint32_t object = 0;
    const MotorControlCANOwner::BusAdmission admitted =
        admitExplicitMarker(firmware_, services_, node - 1, now, 0xC0000000u + ++identity_, request_, object);
    if (admitted != MotorControlCANOwner::BusAdmission::ACCEPTED) {
        std::snprintf(out, size, "MARKER refused node=%u reason=%s; nothing sent\n", node,
                      unsigned(admitted) < 7 ? admissions[unsigned(admitted)] : "unknown");
        return false;
    }
    slot_ = uint8_t(node - 1);
    object_ = object;
    active_ = true;
    std::snprintf(out, size, "MARKER admitted node=%u object=%04lx:%02lx; one explicit read, policy premises accepted\n",
                  node, static_cast<unsigned long>(object >> 8), static_cast<unsigned long>(object & 0xff));
    return true;
}

bool MarkerCommand::service(uint64_t now, char* out, size_t size) noexcept {
    if (!active_ || !out || !size)
        return false;
    MotorControlCANOwner::CanBusOwner& owner = firmware_.owner();
    const MotorControlCANOwner::Completion* done = owner.result(request_);
    if (!done)
        return false;
    // Reopened only after the marker's own frame retired; wait for that before reporting the route.
    if (done->service.outcome == C::SdoOutcome::SUCCESS && services_.drainSettled(slot_))
        return false;
    const unsigned outcome = unsigned(done->service.outcome);
    const unsigned possible = unsigned(done->service.transmissionPossible);
    if (!owner.release(request_))
        return false;
    active_ = false;
    std::snprintf(out, size, "MARKER node=%u object=%04lx:%02lx outcome=%u possible=%u reopened=%u t=%llu\n",
                  unsigned(slot_) + 1, static_cast<unsigned long>(object_ >> 8),
                  static_cast<unsigned long>(object_ & 0xff), outcome, possible,
                  unsigned(!services_.unresolved(slot_)), static_cast<unsigned long long>(now));
    return true;
}

LinkWatch::LinkWatch(FirmwareProbe& firmware, C::SdoClient& services) noexcept
    : firmware_(firmware), services_(services) {}

const char* LinkWatch::begin(uint32_t seconds, uint64_t now) noexcept {
    if (active())
        return "active";
    if (!seconds || seconds > LINK_WATCH_MAXIMUM_SECONDS || now < firmware_.owner().lastTimeUs())
        return "invalid";
    if (!firmware_.running())
        return "state";
    const Stats& s = firmware_.owner().stats();
    startUs_ = now;
    endUs_ = now + uint64_t(seconds) * 1000000u;
    expired0_ = s.txExpired;
    unmatched0_ = s.unmatchedFrames;
    refused0_ = s.reuseStopped;
    markers0_ = s.markers;
    unmatchedRestart_ = refusedRestart_ = dropped_ = 0;
    head_ = count_ = restarts_ = causes_ = 0;
    outcome_ = 0xff;
    listenerUs_ = 0;
    restartTried_ = recoveryPending_ = recovered_ = listenerEnded_ = endPending_ = false;
    noListener_ = firmware_.owner().noListener();
    for (unsigned slot = 0; slot < BENCH_NODES; ++slot) {
        resynced_[slot] = markerActive_[slot] = false;
        probes_[slot] = 0;
        nextProbeUs_[slot] = 0;
    }
    explicit_ = explicitOk_ = 0;
    recovery_ = MotorControlCANOwner::RecoveryId();
    armed_ = armedLine_ = true;
    return nullptr;
}

void LinkWatch::push(LinkEventKind kind, unsigned node, uint32_t value, uint64_t now) noexcept {
    if (count_ == LINK_WATCH_EVENTS) {
        ++dropped_;
        return;
    }
    Event& e = events_[(head_ + count_++) % LINK_WATCH_EVENTS];
    e.timeUs = now;
    e.kind = uint8_t(kind);
    e.node = uint8_t(node);
    e.value = value;
}

void LinkWatch::service(uint64_t now) noexcept {
    if (!armed_)
        return;
    MotorControlCANOwner::CanBusOwner& owner = firmware_.owner();
    const Stats& s = owner.stats();
    const bool noListener = owner.noListener();
    if (noListener_ && !noListener) {
        listenerEnded_ = true;
        listenerUs_ = now;
        push(LinkEventKind::LISTENER_END, 0, 0, now);
    }
    noListener_ = noListener;
    // The one restart: the application's own decision on an expired TX lifetime, called once.
    if (!restartTried_ && (s.txExpired > expired0_ || noListener)) {
        restartTried_ = true;
        push(LinkEventKind::EXPIRED, 0, s.txExpired, now);
        if (owner.recover(now, now + LINK_WATCH_RECOVERY_US, recovery_)) {
            restarts_ = 1;
            recoveryPending_ = true;
            unmatchedRestart_ = s.unmatchedFrames;
            refusedRestart_ = s.reuseStopped;
            push(LinkEventKind::RESTART, 0, recovery_.generation, now);
        } else {
            push(LinkEventKind::RESTART_REFUSED, 0, 0, now);
        }
    }
    if (recoveryPending_) {
        const MotorControlCANOwner::RecoveryCompletion* done = owner.recoveryResult(recovery_);
        if (done) {
            outcome_ = uint8_t(done->outcome);
            causes_ = uint8_t(unsigned(done->causes.busOff) | unsigned(done->causes.receiveLost) << 1 |
                              unsigned(done->causes.txExpired) << 2);
            push(LinkEventKind::RECOVERY, causes_, outcome_, now);
            recovered_ = done->outcome == MotorControlCANOwner::RecoveryOutcome::RETIRED;
            // Released at once: the result is copied above, and a held one would refuse every later recovery.
            recoveryPending_ = !owner.releaseRecovery(recovery_);
        }
    }
    if (recovered_)
        for (unsigned slot = 0; slot < BENCH_NODES; ++slot) {
            repair(slot, now);
            if (!resynced_[slot] && services_.channel(uint8_t(slot)) && !services_.unresolved(uint8_t(slot))) {
                resynced_[slot] = true;
                push(LinkEventKind::RESYNCED, slot + 1, s.markers - markers0_, now);
            }
        }
    bool markersPending = false;
    for (bool active : markerActive_)
        markersPending |= active;
    if (now >= endUs_ && !recoveryPending_ && !markersPending) {
        armed_ = false;
        endPending_ = true;
    }
}

// After the restart, a node whose route is still unresolved is probed with an explicit marker, without
// waiting for NO_LISTENER to end: no automatic step runs during it, and the marker's own ACK ends it. A
// failed probe is followed by another at the probe interval, only once its frame is locally retired,
// at most LINK_WATCH_PROBES per node. Each result is released as soon as it is read. Nothing else is sent.
void LinkWatch::repair(unsigned slot, uint64_t now) noexcept {
    MotorControlCANOwner::CanBusOwner& owner = firmware_.owner();
    if (markerActive_[slot]) {
        const MotorControlCANOwner::Completion* done = owner.result(marker_[slot]);
        if (done) {
            if (done->service.outcome == C::SdoOutcome::SUCCESS)
                ++explicitOk_;
            push(LinkEventKind::MARKER_RESULT, slot + 1,
                 uint32_t(done->service.outcome) << 8 | uint32_t(done->service.transmissionPossible), now);
            markerActive_[slot] = !owner.release(marker_[slot]);
        }
        return;
    }
    const C::SdoChannel* channel = services_.channel(uint8_t(slot));
    Health health;
    SdoReuse reuse;
    if (probes_[slot] >= LINK_WATCH_PROBES || now < nextProbeUs_[slot] || !channel ||
        !services_.unresolved(uint8_t(slot)) || services_.drainSettled(uint8_t(slot)) ||
        !owner.health(channel->binding, health) || health.reuseStopped ||
        !owner.sdoReuse(channel->binding, reuse) || reuse.policy == SdoReusePolicy::NONE ||
        !owner.localTxRetired(uint8_t(slot)) || owner.resources().inhibited)
        return;
    uint32_t object = 0;
    const MotorControlCANOwner::BusAdmission admitted =
        admitExplicitMarker(firmware_, services_, slot, now, MARKER_IDENTITY_BASE + ++markerIdentity_, marker_[slot],
                            object);
    if (admitted == MotorControlCANOwner::BusAdmission::ACCEPTED) {
        markerActive_[slot] = true;
        ++probes_[slot];
        nextProbeUs_[slot] = now + LINK_WATCH_PROBE_INTERVAL_US;
        ++explicit_;
        push(LinkEventKind::MARKER, slot + 1, object, now);
        return;
    }
    if (admitted == MotorControlCANOwner::BusAdmission::INVALID)
        probes_[slot] = LINK_WATCH_PROBES; // No admissible object is left: the route stays unresolved.
}

bool LinkWatch::takeLine(char* out, size_t size) noexcept {
    if (!out || !size)
        return false;
    if (armedLine_) {
        MotorControlCANOwner::CanBusOwner& owner = firmware_.owner();
        SdoReuse reuse[BENCH_NODES];
        for (unsigned slot = 0; slot < BENCH_NODES; ++slot) {
            const C::SdoChannel* channel = services_.channel(uint8_t(slot));
            if (!channel || !owner.sdoReuse(channel->binding, reuse[slot]))
                reuse[slot].policy = SdoReusePolicy(0xff);
        }
        if (!put(size,
                 std::snprintf(out, size,
                               "LINK_WATCH armed seconds=%lu lifetime=%llu reuse1=%s reuse2=%s t=%llu; one restart "
                               "on TX lifetime expiry, nothing sent\n",
                               static_cast<unsigned long>((endUs_ - startUs_) / 1000000u),
                               static_cast<unsigned long long>(owner.txLifetimeUs()), policyName(reuse[0].policy),
                               policyName(BENCH_NODES > 1 ? reuse[BENCH_NODES - 1].policy : reuse[0].policy),
                               static_cast<unsigned long long>(startUs_))))
            return false;
        armedLine_ = false;
        return true;
    }
    if (count_) {
        const Event& e = events_[head_];
        if (!put(size,
                 std::snprintf(out, size, "LINK_EVENT t=%llu kind=%s node=%u value=%lx\n",
                               static_cast<unsigned long long>(e.timeUs), EVENT_NAMES[e.kind], unsigned(e.node),
                               static_cast<unsigned long>(e.value))))
            return false;
        head_ = uint8_t((head_ + 1) % LINK_WATCH_EVENTS);
        --count_;
        return true;
    }
    if (!endPending_)
        return false;
    const Stats& s = firmware_.owner().stats();
    const uint32_t unmatchedAfter = restarts_ ? s.unmatchedFrames - unmatchedRestart_ : 0;
    const uint32_t refusedAfter = restarts_ ? s.reuseStopped - refusedRestart_ : 0;
    if (!put(size,
             std::snprintf(out, size,
                           "LINK_WATCH_END restarts=%u expired=%lu recovery=%d causes=%x listenerEnd=%u "
                           "listenerUs=%llu resynced=%u,%u unmatchedAfter=%lu refusedAfter=%lu markers=%lu "
                           "explicit=%lu explicitOk=%lu noListener=%u dropped=%lu\n",
                           unsigned(restarts_), static_cast<unsigned long>(s.txExpired - expired0_),
                           outcome_ == 0xff ? -1 : int(outcome_), unsigned(causes_), unsigned(listenerEnded_),
                           static_cast<unsigned long long>(listenerEnded_ ? listenerUs_ - startUs_ : 0),
                           unsigned(resynced_[0]), unsigned(BENCH_NODES > 1 ? resynced_[BENCH_NODES - 1] : 0),
                           static_cast<unsigned long>(unmatchedAfter), static_cast<unsigned long>(refusedAfter),
                           static_cast<unsigned long>(s.markers - markers0_), static_cast<unsigned long>(explicit_),
                           static_cast<unsigned long>(explicitOk_), unsigned(firmware_.owner().noListener()),
                           static_cast<unsigned long>(dropped_))))
        return false;
    endPending_ = false;
    return true;
}
} // namespace MotorControlCANExample
