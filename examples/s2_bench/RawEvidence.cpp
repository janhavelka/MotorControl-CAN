// SPDX-License-Identifier: MIT
#include "RawEvidence.h"
#include <cstring>

namespace MotorControlCANExample {
using MotorControlCANOwner::BusRequest;
using MotorControlCANOwner::DiagnosticKind;
using MotorControlCANOwner::Diagnostic;
namespace C = MotorControlCAN::CANOPEN;
namespace L = MotorControlCAN::LICHUAN_C;
namespace {
struct Field {
    uint16_t index;
    uint8_t subindex;
};
const Field fields[] = {
    {0x1000, 0}, {0x1009, 0}, {0x100a, 0}, {0x1018, 0}, {0x1018, 1}, {0x1018, 2}, {0x1018, 3},
    {0x2000, 0}, {0x2001, 0}, {0x1800, 1}, {0x1800, 2}, {0x1800, 3}, {0x1800, 5}, {0x1a00, 0},
    {0x1a00, 1}, {0x1a00, 2}, {0x1a00, 3}, {0x1a00, 4}, {0x6041, 0}, {0x6061, 0}, {0x1017, 0},
    {0x2100, 0}, {0x603f, 0}, {0x6064, 0}, {0x2201, 0}, {0x606c, 0}};
// Original EDS sections and EN86 pp11/16-19 establish these exact readable
// addresses/widths. Uploads retain actual indicated sizes; Q20 effects stay unknown.
const RawEvidenceField motionReadiness[] = {
    {0x1001, 0, 1}, {0x603f, 0, 2}, {0x6041, 0, 2}, {0x6061, 0, 1}, {0x2100, 0, 2}, {0x2101, 0, 2},
    {0x6064, 0, 4}, {0x606c, 0, 4}, {0x607a, 0, 4}, {0x6081, 0, 4}, {0x6083, 0, 4}, {0x6084, 0, 4},
    {0x605d, 0, 2}, {0x60ff, 0, 4}, {0x2200, 0, 2}, {0x2201, 0, 2}, {0x2202, 0, 2}, {0x607d, 1, 4},
    {0x607d, 2, 4}, {0x2500, 0, 2}, {0x2501, 0, 2}, {0x2400, 1, 2}, {0x2400, 2, 2}, {0x2400, 3, 2},
    {0x2400, 4, 2}, {0x2400, 5, 2}, {0x2400, 6, 2}, {0x2400, 7, 2}, {0x2400, 8, 2}, {0x60fd, 0, 4}};
static_assert(sizeof(motionReadiness) / sizeof(motionReadiness[0]) == RAW_EVIDENCE_FIELDS,
              "Motion readiness is exactly thirty uploads");
// LC_StepDriverCAN_V1.1.eds lists these subindexes in 1400..1403, 1800..1803
// (communication) and 1600..1603, 1A00..1A03 (mapping). Actual widths stay raw.
const uint8_t pdoRecordSubindexes[] = {1, 2, 3, 5, 0, 1, 2, 3, 4};
static_assert(sizeof(pdoRecordSubindexes) == 9, "PDO record is exactly nine uploads");
// The same EDS gives 1800..1803 sub1 U32, sub2 U8, sub3 and sub5 U16, all RW.
// Zero means the subindex is not an admitted PDO_COMM_WRITE entry.
unsigned pdoEntryWidth(uint8_t subindex) {
    return subindex == 1 ? 4 : subindex == 2 ? 1 : subindex == 3 || subindex == 5 ? 2 : 0;
}
// The bit and type ranges are generic CiA 301 values, not Lichuan-documented:
// a COB-ID change toggles only bit 31 (PDO not valid), never the identifier,
// bit 30 (RTR) or bit 29 (frame kind); a transmission type excludes reserved
// 241..251 and RTR-only 252/253. Whether the drive accepts it is what the
// experiment observes.
bool validPdoChange(const RawPdoSelection& s) {
    const unsigned width = pdoEntryWidth(s.subindex);
    if (!s.transmit || !width || s.value == s.baseline ||
        (width < 4 && (s.baseline | s.value) >> (8 * width)))
        return false;
    if (s.subindex == 1)
        return (s.baseline ^ s.value) == 0x80000000u;
    return s.subindex != 2 || s.value <= 240 || s.value >= 254;
}
// One exact read, write, read entry; values are unsigned little-endian bytes.
struct ExactEntry {
    uint16_t index;
    uint8_t subindex, width;
    uint32_t baseline, value;
};
bool exactEntry(const RawEvidencePlan& p, ExactEntry& out) {
    if (p.enableHeartbeat) {
        out = ExactEntry{0x1017, 0, 2, 0, 1000};
        return true;
    }
    if (p.inventory != RawEvidenceInventory::PDO_COMM_WRITE)
        return false;
    out = ExactEntry{uint16_t(0x1800 + p.pdo.set), p.pdo.subindex,
                     uint8_t(pdoEntryWidth(p.pdo.subindex)), p.pdo.baseline, p.pdo.value};
    return true;
}
uint32_t littleEndian(const uint8_t* bytes, unsigned width) {
    uint32_t value = 0;
    for (unsigned i = 0; i < width; ++i)
        value |= uint32_t(bytes[i]) << (8 * i);
    return value;
}
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
    out.receiveBoundsKnown = in.receiveBoundsKnown;
    out.receivedEarliestUs = in.receivedEarliestUs;
    out.receivedLatestUs = in.receivedLatestUs;
    out.accepted = in.accepted;
    out.controller = in.controller;
    out.recovery = in.recovery;
}
} // namespace
bool motionReadinessField(size_t ordinal, RawEvidenceField& output) noexcept {
    if (ordinal >= sizeof(motionReadiness) / sizeof(motionReadiness[0]))
        return false;
    output = motionReadiness[ordinal];
    return true;
}
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
    const bool motion = p.inventory == RawEvidenceInventory::MOTION_READINESS;
    const bool pdoRecord = p.inventory == RawEvidenceInventory::PDO_RECORD;
    const bool pdoWrite = p.inventory == RawEvidenceInventory::PDO_COMM_WRITE;
    const unsigned count = motion              ? 30
                           : pdoRecord         ? 9
                           : pdoWrite          ? 3
                           : p.enableHeartbeat ? 3
                           : p.includeNetwork  ? 26
                           : p.includeState    ? 20
                                               : 18;
    const auto* route = services_.channel(p.slot);
    const auto* node = services_.node(p.slot);
    if (p.inventory > RawEvidenceInventory::PDO_COMM_WRITE ||
        ((motion || pdoRecord || pdoWrite) &&
         (p.includeState || p.includeNetwork || p.enableHeartbeat)) ||
        ((pdoRecord || pdoWrite) && p.pdo.set > 3) || (pdoWrite && !validPdoChange(p.pdo)) ||
        (p.includeNetwork && !p.includeState) ||
        (p.enableHeartbeat && (p.includeNetwork || p.includeState)) ||
        overlap(&p, sizeof p, this, sizeof *this) || now < lastUs_ || now < owner_.lastTimeUs() ||
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
    ExactEntry exact{};
    const bool exactPlan = exactEntry(record_.plan, exact);
    capture(false);
    // A frame already on its way settles: cancelling it would quarantine the route.
    if (record_.evidenceLost && pendingValid_ && !owner_.mayHaveTransmitted(pending_))
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
            t.direction == expected_.direction && t.size == expected_.size &&
            t.uploadSize == expected_.uploadSize &&
            (t.direction != C::SdoDirection::DOWNLOAD ||
             std::memcmp(t.data, expected_.data, expected_.size) == 0) &&
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
        if (exactPlan && record_.terminals[record_.terminalCount - 1].service.transfer.direction ==
                             C::SdoDirection::UPLOAD) {
            // Readback confirms only the object bytes. Never infer motor state,
            // remote persistence, a host-loss response or a PDO sample from them.
            const C::SdoCompletion& result = record_.terminals[record_.terminalCount - 1].service;
            const uint32_t wanted = record_.terminalCount == 1 ? exact.baseline : exact.value;
            if (result.size != exact.width || littleEndian(result.data, exact.width) != wanted) {
                finish(RawEvidenceFailure::READBACK, now);
                return true;
            }
            if (record_.plan.enableHeartbeat && record_.terminalCount == 3) {
                const auto* node = services_.node(record_.plan.slot);
                if (!current() || !node ||
                    !services_.setProducerInterval(record_.plan.slot, node->route, 1000, now)) {
                    finish(RawEvidenceFailure::INVALIDATED, now);
                    return true;
                }
            }
        }
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
    work.size = 4;
    work.uploadSize = C::SdoUploadSize::SERVER_INDICATED;
    if (exactPlan) {
        work.index = exact.index;
        work.subindex = exact.subindex;
        work.size = exact.width;
        work.uploadSize = C::SdoUploadSize::EXACT;
        if (field == 1) {
            work.direction = C::SdoDirection::DOWNLOAD;
            for (unsigned i = 0; i < exact.width; ++i)
                work.data[i] = uint8_t(exact.value >> (8 * i));
        }
    } else if (record_.plan.inventory == RawEvidenceInventory::MOTION_READINESS) {
        work.index = motionReadiness[field].index;
        work.subindex = motionReadiness[field].subindex;
    } else if (record_.plan.inventory == RawEvidenceInventory::PDO_RECORD) {
        const RawPdoSelection& pdo = record_.plan.pdo;
        const bool communication = field < 4;
        work.index = uint16_t((pdo.transmit ? (communication ? 0x1800 : 0x1a00)
                                            : (communication ? 0x1400 : 0x1600)) +
                              pdo.set);
        work.subindex = pdoRecordSubindexes[field];
    } else {
        work.index = fields[field].index;
        work.subindex = fields[field].subindex;
    }
    work.readEffect = C::SdoReadEffect::UNQUALIFIED;
    work.deadlineUs = plan.deadlineUs;
    work.queueTimeoutUs = plan.options.queueTimeoutUs;
    work.responseTimeoutUs = plan.options.responseTimeoutUs;
    const uint32_t authorization = record_.plan.firstAuthorizationId + uint32_t(field);
    const bool granted =
        work.direction == C::SdoDirection::DOWNLOAD
            ? services_.authorizeDownloadExperiment(record_.plan.slot, work,
                                                    plan.target.nodeGeneration, authorization, now)
            : services_.authorizeUploadExperiment(record_.plan.slot, work,
                                                  plan.target.nodeGeneration, authorization, now);
    if (!granted) {
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
    // A PDO entry download is not 1017 and leaves the adapter's producer facts alone.
    if (record_.plan.enableHeartbeat && work.direction == C::SdoDirection::DOWNLOAD) {
        const auto* node = services_.node(record_.plan.slot);
        // Conservative first-possible boundary: accepted local work may leave at
        // the next owner turn. No existing producer fact may survive uncertainty.
        if (!node || !services_.invalidateProducerInterval(record_.plan.slot, node->route, now)) {
            owner_.cancel(pending_, now);
        }
    }
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
