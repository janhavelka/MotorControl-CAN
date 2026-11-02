// SPDX-License-Identifier: MIT
#include "DeviceEvidence.h"

namespace MotorControlCANExample {
using MotorControlCANOwner::Diagnostic;
namespace M = MotorControlCAN;
namespace L = MotorControlCAN::LICHUAN_C;
namespace {
template <size_t N> bool text(const char (&value)[N]) {
    if (!value[0])
        return false;
    for (size_t i = 1; i < N; ++i)
        if (!value[i])
            return true;
    return false;
}
bool revision(const char (&value)[41]) {
    if (value[40])
        return false;
    for (unsigned i = 0; i < 40; ++i)
        if (!((value[i] >= '0' && value[i] <= '9') || (value[i] >= 'a' && value[i] <= 'f') ||
              (value[i] >= 'A' && value[i] <= 'F')))
            return false;
    return true;
}
bool same(const L::ReadTarget& a, const L::ReadTarget& b) {
    return a.nodeGeneration == b.nodeGeneration &&
           M::sameBinding(a.channel.binding, b.channel.binding) &&
           a.channel.clientToServerId == b.channel.clientToServerId &&
           a.channel.serverToClientId == b.channel.serverToClientId;
}
bool overlaps(const void* a, size_t an, const void* b, size_t bn) {
    const uintptr_t x = reinterpret_cast<uintptr_t>(a), y = reinterpret_cast<uintptr_t>(b);
    return x >= y ? x - y < bn : y - x < an;
}
bool reviewed(const ReadExperiment& review) {
    return text(review.sourceReview) && text(review.independentReview) &&
           text(review.fixtureReview) && text(review.independentControl) &&
           text(review.failurePlan) && text(review.reconciliation);
}
M::Status prepare(const AcquisitionPlan& p, uint64_t now, L::ReadContext& candidate) {
    switch (p.kind) {
    case L::ReadKind::IDENTITY:
        if (!p.stateMask)
            return L::prepareIdentity(candidate, p.target, 1, now, p.deadlineUs, p.options);
        break;
    case L::ReadKind::CONFIG:
        if (!p.stateMask)
            return L::prepareConfig(candidate, p.target, 1, now, p.deadlineUs, p.options);
        break;
    case L::ReadKind::STATE:
        return L::prepareState(candidate, p.target, 1, now, p.deadlineUs, p.options, p.stateMask);
    default:
        break;
    }
    return M::Status(M::Err::INVALID_CONFIG, 0, "unsupported acquisition intent");
}
} // namespace
DeviceEvidence::DeviceEvidence(ProbeApp& app, CanBusOwner& owner,
                               EvidenceEnvironment environment) noexcept
    : app_(app), owner_(owner), environment_(environment) {}
EvidenceAdmission DeviceEvidence::request(const AcquisitionPlan& plan, uint64_t now) noexcept {
    return start(plan, nullptr, now);
}
EvidenceAdmission DeviceEvidence::requestExperiment(const ReadExperiment& review,
                                                    uint64_t now) noexcept {
    if (overlaps(&review, sizeof review, this, sizeof *this) || !reviewed(review))
        return EvidenceAdmission::INVALID;
    return start(review.plan, &review, now);
}
EvidenceAdmission DeviceEvidence::start(const AcquisitionPlan& plan, const ReadExperiment* review,
                                        uint64_t now) noexcept {
    if (occupied_ || app_.readContext().state != M::ReadState::EMPTY)
        return EvidenceAdmission::BUSY;
    if (overlaps(&plan, sizeof plan, this, sizeof *this) || !plan.id || plan.id <= lastId_ ||
        now < lastUs_ || now < owner_.lastTimeUs() || !text(plan.fixtureId) ||
        !revision(plan.softwareRevision) || !text(plan.firmware) || !text(plan.dialect) ||
        !text(plan.authorization) || !text(plan.observations) || !app_.selected() ||
        !same(plan.target, *app_.selected()) || environment_ > EvidenceEnvironment::HOST_FAKE)
        return EvidenceAdmission::INVALID;
    // No record is observable while unoccupied. Reuse its owned context for
    // preflight instead of stacking it above the application's own preparation.
    // A refusal leaves the application and any retained public record unchanged.
    L::ReadContext& candidate = record_.read;
    if (!prepare(plan, now, candidate))
        return EvidenceAdmission::INVALID;
    if (environment_ == EvidenceEnvironment::DEVICE && !review) {
        // The current generated vocabulary has no reviewed non-changing member.
        // Do not make unknown/future enum values permission. Q20 must be resolved
        // in the canonical ledger and typed policy before ordinary live acquisition.
        for (unsigned i = 0; i < candidate.count; ++i)
            if (candidate.values[i].object.readEffect == L::ReadEffect::UNQUALIFIED)
                return EvidenceAdmission::READ_EFFECT_UNQUALIFIED;
        return EvidenceAdmission::INVALID;
    }
    const bool accepted =
        plan.kind == L::ReadKind::IDENTITY
            ? app_.requestIdentity(now, plan.deadlineUs, plan.options)
        : plan.kind == L::ReadKind::CONFIG
            ? app_.requestConfig(now, plan.deadlineUs, plan.options)
            : app_.requestState(now, plan.deadlineUs, plan.options, plan.stateMask);
    if (!accepted)
        return EvidenceAdmission::INVALID;

    // Reset only valid prefixes. Avoid a large temporary record on a firmware stack.
    record_.plan = plan;
    record_.environment = environment_;
    record_.experimental = review != nullptr;
    if (review)
        record_.review = *review;
    record_.startedUs = now;
    record_.completedUs = 0;
    record_.terminal = record_.interrupted = record_.evidenceLost = false;
    record_.droppedTraffic = record_.terminalCount = record_.trafficCount = 0;
    record_.maxServiceGapUs = 0;
    record_.initialStats = record_.finalStats = owner_.stats();
    record_.initialResources = record_.finalResources = owner_.resources();
    record_.read = app_.readContext();
    record_.intendedCount = candidate.count;
    for (unsigned i = 0; i < candidate.count; ++i)
        record_.intended[i] = candidate.values[i].object;
    // Old diagnostic copies are not part of this acquisition. No authoritative RX
    // is consumed here; collection owns the diagnostic drain until release.
    Diagnostic old;
    for (unsigned i = 0; i < CanBusOwner::TRACE_CAPACITY && owner_.takeDiagnostic(old); ++i) {
    }
    traceOverwritten_ = owner_.stats().traceOverwritten;
    rxLost_ = owner_.stats().rxLost;
    occupied_ = true;
    lastId_ = plan.id;
    lastUs_ = now;
    if (traceOverwritten_ == UINT32_MAX || rxLost_ == UINT32_MAX)
        lost(1); // Saturated counter cannot certify absence of subsequent loss.
    return EvidenceAdmission::ACCEPTED;
}
void DeviceEvidence::lost(uint32_t count) noexcept {
    record_.evidenceLost = true;
    record_.droppedTraffic =
        UINT32_MAX - record_.droppedTraffic < count ? UINT32_MAX : record_.droppedTraffic + count;
}
void DeviceEvidence::capture() noexcept {
    const uint32_t current = owner_.stats().traceOverwritten;
    if (current < traceOverwritten_ || current == UINT32_MAX)
        lost(1);
    else if (current > traceOverwritten_)
        lost(current - traceOverwritten_);
    traceOverwritten_ = current;
    const uint32_t missing = owner_.stats().rxLost;
    if (missing < rxLost_ || missing == UINT32_MAX)
        lost(1);
    else if (missing > rxLost_)
        lost(missing - rxLost_);
    rxLost_ = missing;
    Diagnostic input;
    for (unsigned i = 0; i < CanBusOwner::TRACE_CAPACITY && owner_.takeDiagnostic(input); ++i) {
        if (input.kind == DiagnosticKind::TERMINAL)
            continue;
        if (record_.trafficCount == EVIDENCE_TRAFFIC) {
            lost(1);
            continue;
        }
        EvidenceTraffic& out = record_.traffic[record_.trafficCount++];
        out.kind = input.kind;
        out.frame = input.frame;
        out.txId = input.txId;
        out.controllerEpoch = input.controllerEpoch;
        out.processedUs = input.processedUs;
        out.receivedUs = input.receivedUs;
        out.receivedTimeKnown = input.receivedTimeKnown;
        out.receiveBoundsKnown = input.receiveBoundsKnown;
        out.receivedEarliestUs = input.receivedEarliestUs;
        out.receivedLatestUs = input.receivedLatestUs;
        out.accepted = input.accepted;
        out.controller = input.controller;
        out.recovery = input.recovery;
    }
}
bool DeviceEvidence::service(uint64_t now) noexcept {
    if (now < lastUs_ || now < owner_.lastTimeUs())
        return false;
    if (!occupied_ || record_.terminal)
        return true;
    if (now - lastUs_ > record_.maxServiceGapUs)
        record_.maxServiceGapUs = now - lastUs_;
    capture();
    if (record_.evidenceLost && app_.readContext().state == M::ReadState::ACTIVE) {
        record_.interrupted = true;
        if (!app_.cancelRead(now))
            return false;
    }
    const Completion* terminal = owner_.result(app_.pendingRequest());
    if (terminal) {
        if (record_.terminalCount == L::READ_FIELDS)
            return false; // Impossible for the admitted finite typed plan; retain owner evidence.
        record_.terminals[record_.terminalCount] = *terminal;
        if (!app_.advanceRead(now))
            return false;
        ++record_.terminalCount;
    }
    // A pending success wins its own cancellation race, but explicit interruption
    // still withdraws the unread remainder before the next application turn.
    if (record_.interrupted && app_.readContext().state == M::ReadState::ACTIVE &&
        !app_.cancelRead(now))
        return false;
    record_.read = app_.readContext();
    record_.finalStats = owner_.stats();
    record_.finalResources = owner_.resources();
    lastUs_ = now;
    if (record_.read.state != M::ReadState::ACTIVE) {
        record_.terminal = true;
        record_.completedUs = now;
    }
    return true;
}
bool DeviceEvidence::interrupt(uint64_t now) noexcept {
    if (!occupied_ || record_.terminal || now < lastUs_ || now < owner_.lastTimeUs())
        return false;
    if (app_.readContext().state == M::ReadState::ACTIVE && !app_.cancelRead(now))
        return false;
    record_.interrupted = true;
    return service(now);
}
bool DeviceEvidence::release() noexcept {
    if (!occupied_ || !record_.terminal || !app_.releaseRead())
        return false;
    occupied_ = false;
    return true;
}
} // namespace MotorControlCANExample
