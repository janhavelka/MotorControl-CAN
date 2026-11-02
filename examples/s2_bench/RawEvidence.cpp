// SPDX-License-Identifier: MIT
#include "RawEvidence.h"

namespace MotorControlCANExample {
namespace C = MotorControlCAN::CANOPEN;
namespace L = MotorControlCAN::LICHUAN_C;
namespace {
struct Field {
    uint16_t index;
    uint8_t subindex;
};
const Field fields[RAW_EVIDENCE_FIELDS] = {
    {0x1000, 0}, {0x1009, 0}, {0x100a, 0}, {0x1018, 0}, {0x1018, 1}, {0x1018, 2}, {0x1018, 3},
    {0x2000, 0}, {0x2001, 0}, {0x1800, 1}, {0x1800, 2}, {0x1800, 3}, {0x1800, 5}, {0x1a00, 0},
    {0x1a00, 1}, {0x1a00, 2}, {0x1a00, 3}, {0x1a00, 4}, {0x6041, 0}, {0x6061, 0}};
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
bool same(const C::SdoChannel& a, const C::SdoChannel& b) {
    return MotorControlCAN::sameBinding(a.binding, b.binding) &&
           a.clientToServerId == b.clientToServerId && a.serverToClientId == b.serverToClientId;
}
bool overlap(const void* a, size_t an, const void* b, size_t bn) {
    const uintptr_t x = reinterpret_cast<uintptr_t>(a), y = reinterpret_cast<uintptr_t>(b);
    return x >= y ? x - y < bn : y - x < an;
}
void copyTraffic(EvidenceTraffic& out, const Diagnostic& in) {
    out.kind = in.kind;
    out.frame = in.frame;
    out.txId = in.txId;
    out.controllerEpoch = in.controllerEpoch;
    out.processedUs = in.processedUs;
    out.receivedUs = in.receivedUs;
    out.receivedTimeKnown = in.receivedTimeKnown;
    out.accepted = in.accepted;
    out.controller = in.controller;
    out.recovery = in.recovery;
}
} // namespace
RawEvidence::RawEvidence(CanBusOwner& owner, C::SdoClient& services) noexcept
    : owner_(owner), services_(services) {}
bool RawEvidence::current() const noexcept {
    const auto& p = record_.plan;
    const auto* route = services_.channel(p.slot);
    const auto* node = services_.node(p.slot);
    return route && node && !node->exhausted && same(*route, p.review.plan.target.channel) &&
           node->route.generation == p.review.plan.target.nodeGeneration &&
           owner_.sequence(sequence_) != nullptr;
}
EvidenceAdmission RawEvidence::begin(const RawEvidencePlan& p, uint64_t now) noexcept {
    if (occupied_)
        return EvidenceAdmission::BUSY;
    const auto& r = p.review;
    const auto& a = r.plan;
    const unsigned count = p.includeState ? 20 : 18;
    const auto* route = services_.channel(p.slot);
    const auto* node = services_.node(p.slot);
    if (overlap(&p, sizeof p, this, sizeof *this) || now < lastUs_ || now < owner_.lastTimeUs() ||
        !a.id || a.id <= lastOperation_ || a.deadlineUs <= now || !p.firstAuthorizationId ||
        p.firstAuthorizationId > UINT32_MAX - (count - 1) || a.options.queueTimeoutUs != 250000 ||
        a.options.responseTimeoutUs != 250000 || a.kind != L::ReadKind::IDENTITY || a.stateMask ||
        !text(a.fixtureId) || !revision(a.softwareRevision) || !text(a.firmware) ||
        !text(a.dialect) || !text(a.authorization) || !text(a.observations) ||
        !text(r.sourceReview) || !text(r.independentReview) || !text(r.fixtureReview) ||
        !text(r.independentControl) || !text(r.failurePlan) || !text(r.reconciliation) || !route ||
        !node || node->exhausted || !a.target.nodeGeneration ||
        node->route.generation != a.target.nodeGeneration || !same(*route, a.target.channel))
        return EvidenceAdmission::INVALID;
    SequenceId sequence;
    if (!owner_.beginSequence(a.target.channel.binding, 2, now, sequence))
        return EvidenceAdmission::BUSY;
    if (!owner_.beginOperation(sequence, a.id, a.deadlineUs, now)) {
        owner_.releaseSequence(sequence);
        return EvidenceAdmission::INVALID;
    }
    sequence_ = sequence;
    record_.plan = p;
    record_.startedUs = now;
    record_.completedUs = record_.maxServiceGapUs = 0;
    record_.terminal = record_.evidenceLost = record_.tailOverflow = false;
    record_.failure = RawEvidenceFailure::NONE;
    record_.admission = BusAdmission::ACCEPTED;
    record_.intendedCount = uint8_t(count);
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
    pendingValid_ = false;
    lastUs_ = now;
    lastOperation_ = a.id;
    if (overwritten_ == UINT32_MAX || rxLost_ == UINT32_MAX)
        lose(1, false);
    return EvidenceAdmission::ACCEPTED;
}
void RawEvidence::lose(uint32_t count, bool tail) noexcept {
    if (tail) {
        record_.tailOverflow = true;
        return;
    }
    record_.evidenceLost = true;
    record_.droppedTraffic =
        UINT32_MAX - record_.droppedTraffic < count ? UINT32_MAX : record_.droppedTraffic + count;
}
void RawEvidence::capture(bool tail) noexcept {
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
            if (record_.tailCount == RAW_EVIDENCE_TAIL)
                lose(1, true);
            else
                copyTraffic(record_.tail[record_.tailCount++], in);
        } else {
            if (record_.trafficCount == RAW_EVIDENCE_TRAFFIC)
                lose(1, false);
            else
                copyTraffic(record_.traffic[record_.trafficCount++], in);
        }
    }
}
void RawEvidence::finish(RawEvidenceFailure why, uint64_t now) noexcept {
    record_.failure = why;
    record_.terminal = true;
    record_.completedUs = now;
    record_.finalStats = owner_.stats();
    record_.finalResources = owner_.resources();
    services_.clearUploadExperiment(record_.plan.slot);
}
bool RawEvidence::service(uint64_t now) noexcept {
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
    if (record_.evidenceLost && pendingValid_)
        owner_.cancel(pending_, now);
    if (pendingValid_) {
        const Completion* done = owner_.result(pending_);
        if (!done)
            return true;
        const auto& t = done->service.transfer;
        const bool matched =
            done->sequence.owner == sequence_.owner && done->sequence.slot == sequence_.slot &&
            done->sequence.generation == sequence_.generation && done->producer == 2 &&
            same(t.channel, expected_.channel) && t.operation == expected_.operation &&
            t.request == expected_.request && t.step == expected_.step &&
            t.index == expected_.index && t.subindex == expected_.subindex &&
            t.direction == C::SdoDirection::UPLOAD && t.size == 4 &&
            t.uploadSize == C::SdoUploadSize::SERVER_INDICATED &&
            t.readEffect == C::SdoReadEffect::UNQUALIFIED &&
            done->service.nodeGeneration == record_.plan.review.plan.target.nodeGeneration;
        record_.terminals[record_.terminalCount++] = *done;
        if (!matched || record_.evidenceLost || done->service.outcome != C::SdoOutcome::SUCCESS) {
            finish(!matched               ? RawEvidenceFailure::CORRELATION
                   : record_.evidenceLost ? RawEvidenceFailure::EVIDENCE_LOSS
                                          : RawEvidenceFailure::SERVICE,
                   now);
            return true; // Retain final owner result until explicit release.
        }
        if (!owner_.release(pending_))
            return false;
        pendingValid_ = false;
    }
    if (record_.evidenceLost) {
        finish(RawEvidenceFailure::EVIDENCE_LOSS, now);
        return true;
    }
    if (record_.terminalCount == record_.intendedCount) {
        finish(RawEvidenceFailure::NONE, now);
        return true;
    }
    if (now >= record_.plan.review.plan.deadlineUs) {
        finish(RawEvidenceFailure::DEADLINE, now);
        return true;
    }
    if (!current()) {
        finish(RawEvidenceFailure::INVALIDATED, now);
        return true;
    }
    // A reply can precede the final TX report. Only authoritative owner state
    // can establish its retirement: diagnostic copies can be lost, and exact
    // controller retirement may deliberately remove all pending reports.
    if (!owner_.localTxRetired(record_.plan.slot))
        return true;
    const unsigned field = record_.attemptedCount++;
    const auto& plan = record_.plan.review.plan;
    C::SdoTransfer work;
    work.channel = plan.target.channel;
    work.operation = plan.id;
    work.request = work.step = uint32_t(field + 1);
    work.index = fields[field].index;
    work.subindex = fields[field].subindex;
    work.size = 4;
    work.uploadSize = C::SdoUploadSize::SERVER_INDICATED;
    work.readEffect = C::SdoReadEffect::UNQUALIFIED;
    work.deadlineUs = plan.deadlineUs;
    work.queueTimeoutUs = plan.options.queueTimeoutUs;
    work.responseTimeoutUs = plan.options.responseTimeoutUs;
    if (!services_.authorizeUploadExperiment(record_.plan.slot, work, plan.target.nodeGeneration,
                                             record_.plan.firstAuthorizationId + uint32_t(field),
                                             now)) {
        finish(RawEvidenceFailure::AUTHORIZATION, now);
        return true;
    }
    BusRequest request;
    request.sequence = sequence_;
    request.transfer = work;
    record_.admission = owner_.admit(request, now, pending_);
    if (record_.admission != BusAdmission::ACCEPTED) {
        finish(RawEvidenceFailure::ADMISSION, now);
        return true;
    }
    expected_ = work;
    pendingValid_ = true;
    return true;
}
bool RawEvidence::release() noexcept {
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
