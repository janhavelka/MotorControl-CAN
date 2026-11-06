// SPDX-License-Identifier: MIT
#include "PdoExperiment.h"
#include <cstring>

namespace MotorControlCANExample {
using MotorControlCANOwner::BusRequest;
using MotorControlCANOwner::DiagnosticKind;
using MotorControlCANOwner::Diagnostic;
using MotorControlCANOwner::Sequence;
namespace C = MotorControlCAN::CANOPEN;
namespace L = MotorControlCAN::LICHUAN_C;
namespace {
const uint16_t kIndex[9] = {0x1800, 0x1800, 0x1800, 0x1800, 0x1a00, 0x1a00, 0x1a00, 0x1a00, 0x1a00};
const uint8_t kSub[9] = {1, 2, 3, 5, 0, 1, 2, 3, 4};
const uint8_t kWidth[9] = {4, 1, 2, 2, 1, 4, 4, 4, 4};
template <size_t N> bool text(const char (&s)[N]) {
    if (!s[0])
        return false;
    for (size_t i = 1; i < N; ++i)
        if (!s[i])
            return true;
    return false;
}
bool revision(const char (&s)[41]) {
    if (s[40])
        return false;
    for (unsigned i = 0; i < 40; ++i)
        if (!((s[i] >= '0' && s[i] <= '9') || (s[i] >= 'a' && s[i] <= 'f') ||
              (s[i] >= 'A' && s[i] <= 'F')))
            return false;
    return true;
}
// The two plans this harness runs, each with the provenance its scope names. The map change is
// the 2026-11-02 node-1 experiment; the event timer is the subset measured on both nodes.
bool mapPlan(const L::Tpdo1ConfigurationRequest& q) {
    return q.transition == L::Tpdo1Transition::MAP3_TO_MAP2 &&
           q.eventTimer == L::Tpdo1EventTimerRaw::RAW_0;
}
bool timerPlan(const L::Tpdo1ConfigurationRequest& q) {
    return q.transition == L::Tpdo1Transition::EVENT_TIMER &&
           (q.eventTimer == L::Tpdo1EventTimerRaw::RAW_0 ||
            q.eventTimer == L::Tpdo1EventTimerRaw::RAW_100);
}
// The node's own TPDO1 route under this logical binding, at this or an earlier configuration
// epoch: a route installed before an earlier change of this lifecycle keeps that older epoch
// until a new map is installed. A route of another node or binding never matches. Withdrawing a
// match only revokes; any replacement still passes configurePdo's own checks.
bool ownTpdo1(const C::PdoRecord& pdo, const MotorControlCAN::Binding& target) {
    MotorControlCAN::Binding route = pdo.map.route.binding;
    if (route.configurationGeneration > target.configurationGeneration)
        return false;
    route.configurationGeneration = target.configurationGeneration;
    return MotorControlCAN::sameBinding(route, target) &&
           pdo.map.route.layout.mapping.direction == C::PdoDirection::TPDO &&
           pdo.map.route.layout.mapping.number == 1;
}
bool overlap(const void* a, size_t an, const void* b, size_t bn) {
    const uintptr_t x = reinterpret_cast<uintptr_t>(a), y = reinterpret_cast<uintptr_t>(b);
    return x >= y ? x - y < bn : y - x < an;
}
bool same(const C::SdoChannel& a, const C::SdoChannel& b) {
    return MotorControlCAN::sameBinding(a.binding, b.binding) &&
           a.clientToServerId == b.clientToServerId && a.serverToClientId == b.serverToClientId;
}
bool sameWork(const C::SdoTransfer& a, const C::SdoTransfer& b) {
    return same(a.channel, b.channel) && a.operation == b.operation && a.request == b.request &&
           a.step == b.step && a.index == b.index && a.subindex == b.subindex &&
           a.direction == b.direction && a.size == b.size && a.uploadSize == b.uploadSize &&
           a.readEffect == b.readEffect && a.requirePreOperational == b.requirePreOperational &&
           a.allowSegmentedUpload == b.allowSegmentedUpload && a.deadlineUs == b.deadlineUs &&
           a.queueTimeoutUs == b.queueTimeoutUs && a.responseTimeoutUs == b.responseTimeoutUs &&
           (a.direction != C::SdoDirection::DOWNLOAD || std::memcmp(a.data, b.data, a.size) == 0);
}
void copy(EvidenceTraffic& out, const Diagnostic& in) {
    out.kind = in.kind;
    out.frame = in.frame;
    out.txId = in.txId;
    out.controllerEpoch = in.controllerEpoch;
    out.processedUs = in.processedUs;
    out.receivedUs = in.receivedUs;
    out.receivedTimeKnown = in.receivedTimeKnown;
    out.receiveBoundsKnown = in.receiveBoundsKnown;
    out.receivedEarliestUs = in.receivedEarliestUs;
    out.receivedLatestUs = in.receivedLatestUs;
    out.accepted = in.accepted;
    out.controller = in.controller;
    out.recovery = in.recovery;
}
} // namespace
PdoExperiment::PdoExperiment(CanBusOwner& owner, C::SdoClient& services) noexcept
    : owner_(owner), services_(services) {}
bool PdoExperiment::current() const noexcept {
    const auto& p = record_.plan;
    const auto* route = services_.channel(p.slot);
    const auto* node = services_.node(p.slot);
    C::SdoChannel expected = p.review.plan.target.channel;
    if (record_.configurationMigrated)
        expected.binding = record_.current;
    return route && node && !node->exhausted && same(*route, expected) &&
           node->route.generation == p.review.plan.target.nodeGeneration &&
           owner_.sequence(sequence_);
}
bool PdoExperiment::currentPreOperational(uint64_t now, C::ObservationTime& out) const noexcept {
    const auto* node = services_.node(record_.plan.slot);
    C::ServiceEligibility eligibility;
    uint64_t first = 0, last = 0;
    if (!node || !C::serviceEligibility(*node, now, eligibility) || !eligibility.exactNmtKnown ||
        eligibility.state != C::NmtState::PRE_OPERATIONAL || node->nmtOrderingUncertain ||
        !C::observationTimeBounds(node->nmtTime, first, last) || last > now ||
        node->nmtTime.processedUs > now || first < record_.reservation.acquiredUs ||
        now - first > record_.plan.maximumAgeUs ||
        node->nmtTime.generation != record_.reservation.target.nodeGeneration)
        return false;
    out = node->nmtTime;
    return true;
}
EvidenceAdmission PdoExperiment::begin(const PdoExperimentPlan& p, uint64_t now) noexcept {
    if (occupied_)
        return EvidenceAdmission::BUSY;
    const auto& a = p.review.plan;
    const auto& r = p.review;
    const auto& q = p.configurationReview;
    const auto* route = services_.channel(p.slot);
    const auto* node = services_.node(p.slot);
    const bool map = mapPlan(p.request), timer = timerPlan(p.request);
    const uint32_t address = a.target.channel.binding.endpoint.address;
    // 9 baseline reads, the writes, 9 final reads: one exact grant each.
    const uint32_t services = map ? 24 : 19;
    if (overlap(&p, sizeof p, this, sizeof *this) || now < lastUs_ || now < owner_.lastTimeUs() ||
        !a.id || a.id == UINT32_MAX || a.id <= lastOperation_ || a.deadlineUs <= now ||
        (!map && !timer) || !p.firstAuthorizationId ||
        p.firstAuthorizationId > UINT32_MAX - (services - 1) || !p.maximumAgeUs ||
        a.options.queueTimeoutUs != 250000 || a.options.responseTimeoutUs != 250000 ||
        a.kind != L::ReadKind::IDENTITY || a.stateMask ||
        (map && (address != 1 || q.scope != L::PdoConfigurationScope::ISOLATED_EXPERIMENT)) ||
        (timer && ((address != 1 && address != 2) ||
                   q.scope != L::PdoConfigurationScope::FIXTURE_QUALIFIED)) ||
        a.target.channel.binding.configurationGeneration == UINT32_MAX || !text(a.fixtureId) ||
        !revision(a.softwareRevision) || !text(a.firmware) || !text(a.dialect) ||
        !text(a.authorization) || !text(a.observations) || !text(r.sourceReview) ||
        !text(r.independentReview) || !text(r.fixtureReview) || !text(r.independentControl) ||
        !text(r.failurePlan) || !text(r.reconciliation) || !q.fixtureRecord ||
        !q.firmwareRecord || !q.procedureRecord || !q.authorizationRecord ||
        !same(q.target.channel, a.target.channel) ||
        q.target.nodeGeneration != a.target.nodeGeneration || !route || !node || node->exhausted ||
        !node->maximumEvidenceAgeUs || node->maximumEvidenceAgeUs > p.maximumAgeUs ||
        !a.target.nodeGeneration || node->route.generation != a.target.nodeGeneration ||
        !same(*route, a.target.channel))
        return EvidenceAdmission::INVALID;
    SequenceId sequence;
    if (!owner_.beginSequence(a.target.channel.binding, 3, now, sequence))
        return EvidenceAdmission::BUSY;
    if (!owner_.beginOperation(sequence, a.id, a.deadlineUs, now)) {
        owner_.releaseSequence(sequence);
        return EvidenceAdmission::INVALID;
    }
    const Sequence* actual = owner_.sequence(sequence);
    if (!actual) {
        owner_.releaseSequence(sequence);
        return EvidenceAdmission::INVALID;
    }
    sequence_ = sequence;
    // Initialize counts/flags directly; do not create a record-sized stack temporary.
    record_.plan = p;
    record_.reservation.target = a.target;
    record_.reservation.model = L::SettingsModel::CL86_C;
    record_.reservation.owner = owner_.identity();
    record_.reservation.sequence = sequence.generation;
    record_.reservation.generation = sequence.generation;
    record_.reservation.acquiredUs = actual->acquiredUs;
    record_.baselinePreOperational = C::ObservationTime();
    record_.current = a.target.channel.binding;
    record_.configuration = L::Tpdo1ConfigurationContext();
    record_.configurationPrepared = record_.configurationMigrated = false;
    record_.terminal = record_.evidenceLost = record_.tailOverflow = false;
    record_.failure = PdoExperimentFailure::NONE;
    record_.admission = BusAdmission::ACCEPTED;
    record_.startedUs = now;
    record_.completedUs = record_.maxServiceGapUs = 0;
    record_.attemptedCount = record_.terminalCount = record_.trafficCount = record_.tailCount = 0;
    record_.droppedTraffic = 0;
    record_.initialStats = record_.finalStats = owner_.stats();
    record_.initialResources = record_.finalResources = owner_.resources();
    Diagnostic old;
    for (unsigned i = 0; i < CanBusOwner::TRACE_CAPACITY && owner_.takeDiagnostic(old); ++i) {
    }
    overwritten_ = owner_.stats().traceOverwritten;
    rxLost_ = owner_.stats().rxLost;
    occupied_ = true;
    pendingValid_ = readbackWait_ = false;
    readbackAfterUs_ = 0;
    requestedFailure_ = PdoExperimentFailure::NONE;
    lastUs_ = now;
    lastOperation_ = a.id + 1;
    if (overwritten_ == UINT32_MAX || rxLost_ == UINT32_MAX)
        lose(1, false);
    return EvidenceAdmission::ACCEPTED;
}
void PdoExperiment::lose(uint32_t count, bool tail) noexcept {
    if (tail) {
        record_.tailOverflow = true;
        return;
    }
    record_.evidenceLost = true;
    record_.droppedTraffic =
        UINT32_MAX - record_.droppedTraffic < count ? UINT32_MAX : record_.droppedTraffic + count;
}
void PdoExperiment::capture(bool tail) noexcept {
    const uint32_t overwritten = owner_.stats().traceOverwritten, lost = owner_.stats().rxLost;
    if (overwritten < overwritten_ || overwritten == UINT32_MAX)
        lose(1, tail);
    else if (overwritten > overwritten_)
        lose(overwritten - overwritten_, tail);
    if (lost < rxLost_ || lost == UINT32_MAX)
        lose(1, tail);
    else if (lost > rxLost_)
        lose(lost - rxLost_, tail);
    overwritten_ = overwritten;
    rxLost_ = lost;
    Diagnostic in;
    for (unsigned i = 0; i < CanBusOwner::TRACE_CAPACITY && owner_.takeDiagnostic(in); ++i) {
        if (in.kind == DiagnosticKind::TERMINAL)
            continue;
        if (tail) {
            if (record_.tailCount == PDO_EXPERIMENT_TAIL)
                lose(1, true);
            else
                copy(record_.tail[record_.tailCount++], in);
        } else {
            if (record_.trafficCount == PDO_EXPERIMENT_TRAFFIC)
                lose(1, false);
            else
                copy(record_.traffic[record_.trafficCount++], in);
        }
    }
}
void PdoExperiment::finish(PdoExperimentFailure why, uint64_t now) noexcept {
    auto& c = record_.configuration;
    if (record_.configurationPrepared && (c.state == L::PdoConfigurationState::WRITING ||
                                          c.state == L::PdoConfigurationState::READING_BACK))
        L::advancePdoConfiguration(c,
                                   why == PdoExperimentFailure::DEADLINE
                                       ? MotorControlCAN::ReadOutcome::DEADLINE
                                       : MotorControlCAN::ReadOutcome::INVALIDATED,
                                   now);
    record_.failure = why;
    record_.terminal = true;
    record_.completedUs = now;
    record_.finalStats = owner_.stats();
    record_.finalResources = owner_.resources();
    services_.clearUploadExperiment(record_.plan.slot);
}
bool PdoExperiment::service(uint64_t now) noexcept {
    if (now < lastUs_ || now < owner_.lastTimeUs())
        return false;
    if (!occupied_)
        return true;
    if (record_.terminal) {
        capture(true);
        lastUs_ = now;
        return true;
    }
    if (now - lastUs_ > record_.maxServiceGapUs)
        record_.maxServiceGapUs = now - lastUs_;
    lastUs_ = now;
    capture(false);
    const auto& plan = record_.plan.review.plan;
    C::ObservationTime livePreOp;
    if (record_.evidenceLost)
        requestedFailure_ = PdoExperimentFailure::EVIDENCE_LOSS;
    else if (now >= plan.deadlineUs)
        requestedFailure_ = PdoExperimentFailure::DEADLINE;
    // A completed failure can itself quarantine its route. Preserve that actual
    // native cause before interpreting the resulting unusable route as a new event.
    const Completion* available = pendingValid_ ? owner_.result(pending_) : nullptr;
    if (!available && requestedFailure_ == PdoExperimentFailure::NONE &&
        (!current() ||
         (record_.baselinePreOperational.present && !currentPreOperational(now, livePreOp))))
        requestedFailure_ = PdoExperimentFailure::INVALIDATED;
    // Losing a diagnostic copy is no reason to cancel a frame already on its way.
    if (requestedFailure_ != PdoExperimentFailure::NONE && pendingValid_ &&
        (requestedFailure_ != PdoExperimentFailure::EVIDENCE_LOSS ||
         !owner_.mayHaveTransmitted(pending_)))
        owner_.cancel(pending_, now);
    if (pendingValid_) {
        const Completion* done = owner_.result(pending_);
        if (!done)
            return true;
        const bool matched = done->sequence.owner == sequence_.owner &&
                             done->sequence.slot == sequence_.slot &&
                             done->sequence.generation == sequence_.generation &&
                             done->producer == 3 && sameWork(done->service.transfer, expected_) &&
                             done->service.nodeGeneration == plan.target.nodeGeneration;
        if (record_.terminalCount == PDO_EXPERIMENT_TERMINALS) {
            finish(PdoExperimentFailure::CORRELATION, now);
            return true;
        }
        record_.terminals[record_.terminalCount++] = *done;
        if (record_.configurationPrepared && matched && current()) {
            const auto status = L::advancePdoConfiguration(
                record_.configuration, record_.reservation, record_.current, done->service, now);
            if (!status && requestedFailure_ == PdoExperimentFailure::NONE)
                requestedFailure_ = PdoExperimentFailure::CONFIGURATION;
        } else if (matched && record_.terminalCount <= 9) {
            baseline_[record_.terminalCount - 1] = done->service;
        }
        if (!matched)
            requestedFailure_ = PdoExperimentFailure::CORRELATION;
        if (requestedFailure_ != PdoExperimentFailure::NONE ||
            done->service.outcome != C::SdoOutcome::SUCCESS) {
            finish(requestedFailure_ != PdoExperimentFailure::NONE ? requestedFailure_
                   : done->service.outcome == C::SdoOutcome::REBOOT ||
                           done->service.outcome == C::SdoOutcome::BINDING_INVALIDATED
                       ? PdoExperimentFailure::INVALIDATED
                       : PdoExperimentFailure::SERVICE,
                   now);
            return true;
        }
        if (!owner_.release(pending_))
            return false;
        pendingValid_ = false;
        if (record_.configurationPrepared &&
            record_.configuration.state == L::PdoConfigurationState::FAILED) {
            finish(PdoExperimentFailure::READBACK, now);
            return true;
        }
    }
    if (requestedFailure_ == PdoExperimentFailure::NONE &&
        (!current() ||
         (record_.baselinePreOperational.present && !currentPreOperational(now, livePreOp))))
        requestedFailure_ = PdoExperimentFailure::INVALIDATED;
    if (requestedFailure_ != PdoExperimentFailure::NONE) {
        finish(requestedFailure_, now);
        return true;
    }
    if (record_.configurationPrepared &&
        record_.configuration.state == L::PdoConfigurationState::SUCCEEDED) {
        finish(PdoExperimentFailure::NONE, now);
        return true;
    }
    if (!owner_.localTxRetired(record_.plan.slot))
        return true;
    if (!record_.baselinePreOperational.present) {
        if (!currentPreOperational(now, livePreOp))
            return true;
        record_.baselinePreOperational = livePreOp;
    }
    if (!currentPreOperational(now, livePreOp)) {
        finish(PdoExperimentFailure::INVALIDATED, now);
        return true;
    }
    if (record_.terminalCount == 9 && !record_.configurationPrepared) {
        // Every value, width and provenance is checked here, before the first write is admitted.
        if (!L::prepareTpdo1Configuration(
                record_.configuration, record_.reservation, record_.plan.configurationReview,
                record_.plan.request, baseline_, 9, C::NmtState::PRE_OPERATIONAL,
                record_.baselinePreOperational, plan.id + 1, now, plan.deadlineUs, plan.options,
                record_.plan.maximumAgeUs)) {
            finish(PdoExperimentFailure::CONFIGURATION, now);
            return true;
        }
        record_.configurationPrepared = true;
        if (!owner_.beginOperation(sequence_, plan.id + 1, plan.deadlineUs, now)) {
            finish(PdoExperimentFailure::CONFIGURATION, now);
            return true;
        }
    }
    if (record_.configurationPrepared &&
        record_.configuration.state == L::PdoConfigurationState::READING_BACK &&
        !record_.configurationMigrated) {
        if (!owner_.advanceConfiguration(sequence_, record_.current, now)) {
            finish(PdoExperimentFailure::CONFIGURATION, now);
            return true;
        }
        record_.configurationMigrated = true;
        // The advance revoked this node's routes again, which also ends their permission to take
        // replacement map evidence. Renew it for the withdrawn TPDO1 route, and admit the first
        // read only at a later time: the adapter refuses map evidence acquired at or before a
        // route's latest invalidation, and the read's adapter start, which dates the readback,
        // could otherwise fall at this boundary's own time if the owner serves again before the
        // clock advances.
        for (uint8_t slot = 0; slot < C::SdoClient::PDO_ROUTES; ++slot) {
            const C::PdoRecord* pdo = services_.pdo(slot);
            if (pdo && ownTpdo1(*pdo, plan.target.channel.binding)) {
                if (!services_.invalidatePdo(slot, true, now)) {
                    finish(PdoExperimentFailure::CONFIGURATION, now);
                    return true;
                }
                readbackWait_ = true;
                readbackAfterUs_ = now;
            }
        }
    }
    if (readbackWait_ && now <= readbackAfterUs_)
        return true;
    C::SdoTransfer work;
    if (record_.configurationPrepared) {
        L::PreparedRead prepared;
        if (!L::nextPdoConfiguration(record_.configuration, record_.reservation, record_.current,
                                     C::NmtState::PRE_OPERATIONAL, livePreOp, now, prepared) ||
            !prepared.ready) {
            finish(PdoExperimentFailure::CONFIGURATION, now);
            return true;
        }
        work = prepared.transfer;
    } else {
        const unsigned field = record_.terminalCount;
        work.channel = plan.target.channel;
        work.operation = plan.id;
        work.request = work.step = field + 1;
        work.requirePreOperational = true;
        work.index = kIndex[field];
        work.subindex = kSub[field];
        work.size = kWidth[field];
        work.deadlineUs = plan.deadlineUs;
        work.queueTimeoutUs = plan.options.queueTimeoutUs;
        work.responseTimeoutUs = plan.options.responseTimeoutUs;
    }
    const uint32_t authorization = record_.plan.firstAuthorizationId + record_.attemptedCount++;
    const bool granted =
        work.direction == C::SdoDirection::DOWNLOAD
            ? services_.authorizeDownloadExperiment(record_.plan.slot, work,
                                                    plan.target.nodeGeneration, authorization, now)
            : services_.authorizeUploadExperiment(record_.plan.slot, work,
                                                  plan.target.nodeGeneration, authorization, now);
    if (!granted) {
        finish(PdoExperimentFailure::AUTHORIZATION, now);
        return true;
    }
    BusRequest request;
    request.sequence = sequence_;
    request.transfer = work;
    record_.admission = owner_.admit(request, now, pending_);
    if (record_.admission != BusAdmission::ACCEPTED) {
        finish(PdoExperimentFailure::ADMISSION, now);
        return true;
    }
    expected_ = work;
    pendingValid_ = true;
    if (work.direction == C::SdoDirection::DOWNLOAD && !record_.configuration.effectsPossible) {
        // Withdraw the actual affected receiving route before any possible write, for the timer
        // change too: its communication facts change. Unrelated PDO layouts, other nodes'
        // included, remain usable. This harness never reinstalls maps or produces
        // sample-retirement proof afterwards; publication is the caller's explicit choice.
        for (uint8_t slot = 0; slot < C::SdoClient::PDO_ROUTES; ++slot) {
            const C::PdoRecord* pdo = services_.pdo(slot);
            if (pdo && ownTpdo1(*pdo, plan.target.channel.binding) &&
                !services_.invalidatePdo(slot, true, now)) {
                requestedFailure_ = PdoExperimentFailure::CONFIGURATION;
                owner_.cancel(pending_, now);
                return true;
            }
        }
    }
    if (work.direction == C::SdoDirection::DOWNLOAD &&
        !L::notePdoConfigurationTransmission(record_.configuration, record_.reservation,
                                             record_.current, C::NmtState::PRE_OPERATIONAL,
                                             livePreOp, now)) {
        requestedFailure_ = PdoExperimentFailure::CONFIGURATION;
        owner_.cancel(pending_, now);
    }
    return true;
}
EvidenceAdmission PdoExperiment::beginWithServiceAge(const PdoExperimentPlan& p,
                                                     uint64_t now) noexcept {
    const C::NodeSnapshot* node = services_.node(p.slot);
    if (!node || !p.maximumAgeUs || overlap(&p, sizeof p, this, sizeof *this))
        return EvidenceAdmission::INVALID;
    const uint64_t previous = node->maximumEvidenceAgeUs;
    if (!services_.setServiceAge(p.slot, p.maximumAgeUs, now))
        return EvidenceAdmission::INVALID;
    const EvidenceAdmission admitted = begin(p, now);
    // A refusal sends nothing and leaves the node's policy as it was. The same time is valid
    // again: setServiceAge just accepted it and begin changes no adapter time.
    if (admitted != EvidenceAdmission::ACCEPTED)
        services_.setServiceAge(p.slot, previous, now);
    return admitted;
}
bool PdoExperiment::release() noexcept {
    if (!occupied_ || !record_.terminal || !owner_.localTxRetired(record_.plan.slot))
        return false;
    if (pendingValid_) {
        if (!owner_.release(pending_))
            return false;
        pendingValid_ = false;
    }
    if (!owner_.releaseSequence(sequence_))
        return false;
    occupied_ = false;
    return true;
}
} // namespace MotorControlCANExample
