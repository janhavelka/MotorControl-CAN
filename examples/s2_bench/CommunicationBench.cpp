// SPDX-License-Identifier: MIT
#include "CommunicationBench.h"
#include "AxisSession.h"
#include <cstdio>
#include <cstring>
#include <new>

namespace MotorControlCANExample {
using MotorControlCANOwner::BusRequest;
using MotorControlCANOwner::Diagnostic;
using MotorControlCANOwner::DiagnosticKind;
using MotorControlCANOwner::Sequence;
namespace M = MotorControlCAN;
namespace C = MotorControlCAN::CANOPEN;
namespace L = MotorControlCAN::LICHUAN_C;
namespace {
// Host policies of both records, not device timing: queue/response budgets and evidence age.
const uint32_t BUDGET_US = 250000;
const uint64_t AGE_US = 3000000;
struct GroupStep {
    L::CommunicationGroup group;
    uint8_t pdo;
};
// The fixed K order: the six prompt 19 network fields, the stored node setting, then each PDO
// record with every child, inactive slots included (EDS 1400..1403/1600..1603/1800..1803/1A00..1A03).
const GroupStep GROUPS[COMMUNICATION_GROUPS] = {
    {L::CommunicationGroup::NETWORK, 0},            {L::CommunicationGroup::NODE_SETTING, 0},
    {L::CommunicationGroup::TPDO_COMMUNICATION, 1}, {L::CommunicationGroup::TPDO_COMMUNICATION, 2},
    {L::CommunicationGroup::TPDO_COMMUNICATION, 3}, {L::CommunicationGroup::TPDO_COMMUNICATION, 4},
    {L::CommunicationGroup::TPDO_MAPPING, 1},       {L::CommunicationGroup::TPDO_MAPPING, 2},
    {L::CommunicationGroup::TPDO_MAPPING, 3},       {L::CommunicationGroup::TPDO_MAPPING, 4},
    {L::CommunicationGroup::RPDO_COMMUNICATION, 1}, {L::CommunicationGroup::RPDO_COMMUNICATION, 2},
    {L::CommunicationGroup::RPDO_COMMUNICATION, 3}, {L::CommunicationGroup::RPDO_COMMUNICATION, 4},
    {L::CommunicationGroup::RPDO_MAPPING, 1},       {L::CommunicationGroup::RPDO_MAPPING, 2},
    {L::CommunicationGroup::RPDO_MAPPING, 3},       {L::CommunicationGroup::RPDO_MAPPING, 4}};
bool sameChannel(const C::SdoChannel& a, const C::SdoChannel& b) {
    return M::sameBinding(a.binding, b.binding) && a.clientToServerId == b.clientToServerId &&
           a.serverToClientId == b.serverToClientId;
}
// Complete work identity, policy flags and, for a download, its bytes.
bool sameWork(const C::SdoTransfer& a, const C::SdoTransfer& b) {
    return sameChannel(a.channel, b.channel) && a.operation == b.operation &&
           a.request == b.request && a.step == b.step && a.index == b.index &&
           a.subindex == b.subindex && a.direction == b.direction && a.size == b.size &&
           a.uploadSize == b.uploadSize && a.readEffect == b.readEffect &&
           a.allowSegmentedUpload == b.allowSegmentedUpload &&
           a.requirePreOperational == b.requirePreOperational &&
           a.requireUploadReview == b.requireUploadReview && a.deadlineUs == b.deadlineUs &&
           a.queueTimeoutUs == b.queueTimeoutUs && a.responseTimeoutUs == b.responseTimeoutUs &&
           (a.direction != C::SdoDirection::DOWNLOAD ||
            (a.size <= sizeof a.data && std::memcmp(a.data, b.data, a.size) == 0));
}
bool sameSequence(const SequenceId& a, const SequenceId& b) {
    return a.owner == b.owner && a.slot == b.slot && a.generation == b.generation;
}
// Why beginSequence refused: another holder keeps this axis's reservation (on the bench an open
// route AxisSession, which keeps it for the rest of the boot), or anything else.
const char* sequenceRefusal(const CanBusOwner& owner, const M::Binding& binding) {
    SequenceId held;
    return owner.findSequence(binding, held) ? "reserved" : "busy";
}
// The channel grants an experiment only above its last grant, in authorization and in operation
// identity, whoever drew it (SdoClient::authorizeUploadExperiment). A route's grants can be above
// main.cpp's counters, which therefore continue from the channel's last grant; a request that does
// not is refused before it reserves anything, rather than at its first upload.
const char* grantRefusal(const C::SdoClient& services, uint8_t slot, uint32_t operation, uint32_t grant) {
    uint32_t lastGrant = 0, lastOperation = 0;
    return services.lastExperiment(slot, lastGrant, lastOperation) && operation > lastOperation &&
                   grant > lastGrant
               ? nullptr
               : "grant";
}
char direction(L::CommunicationGroup g) {
    return g == L::CommunicationGroup::RPDO_COMMUNICATION || g == L::CommunicationGroup::RPDO_MAPPING
               ? 'r'
               : 't';
}
} // namespace

// ------------------------------------------------------------------- K: typed reads
CommunicationReads::CommunicationReads(CanBusOwner& owner, C::SdoClient& services) noexcept
    : owner_(owner), services_(services) {}

bool CommunicationReads::current() const noexcept {
    const C::SdoChannel* route = services_.channel(slot());
    const C::NodeSnapshot* node = services_.node(slot());
    return route && node && !node->exhausted && sameChannel(*route, target_.channel) &&
           node->route.generation == target_.nodeGeneration && owner_.sequence(sequence_);
}

const char* CommunicationReads::begin(const CommunicationBenchRequest& r, uint64_t now) noexcept {
    if (occupied_)
        return "busy";
    if (!r.id || r.id <= lastId_ || r.id > UINT32_MAX - (COMMUNICATION_GROUPS - 1) ||
        r.node < 1 || r.node > BENCH_NODES || !r.firstGrant ||
        r.firstGrant > UINT32_MAX - (COMMUNICATION_UPLOADS - 1) || r.deadlineUs <= now ||
        now < lastUs_ || now < owner_.lastTimeUs())
        return "invalid";
    const uint8_t s = uint8_t(r.node - 1);
    const C::SdoChannel* channel = services_.channel(s);
    const C::NodeSnapshot* node = services_.node(s);
    if (!channel || !node || node->exhausted || channel->binding.endpoint.address != r.node)
        return "route";
    // An unresolved route admits nothing; refuse before reserving rather than fail at the first read.
    if (services_.unresolved(s))
        return "unresolved";
    if (const char* refusal = grantRefusal(services_, s, r.id, r.firstGrant))
        return refusal;
    SequenceId sequence;
    if (!owner_.beginSequence(channel->binding, COMMUNICATION_PRODUCER, now, sequence))
        return sequenceRefusal(owner_, channel->binding);
    sequence_ = sequence;
    target_.channel = *channel;
    target_.nodeGeneration = node->route.generation;
    record_.request = r;
    record_.startedUs = now;
    record_.completedUs = record_.maxServiceGapUs = 0;
    record_.terminal = false;
    record_.failure = CommunicationBenchFailure::NONE;
    record_.admission = BusAdmission::ACCEPTED;
    record_.group = record_.published = record_.grants = record_.terminalCount = 0;
    record_.outcome = M::ReadOutcome::NONE;
    record_.uncertain = false;
    record_.status = M::Ok();
    L::resetRead(context_.read);
    occupied_ = true;
    pendingValid_ = prepared_ = false;
    lastUs_ = now;
    lastId_ = r.id + (COMMUNICATION_GROUPS - 1);
    return nullptr;
}

void CommunicationReads::finish(CommunicationBenchFailure why, uint64_t now) noexcept {
    // Settle a group still waiting between uploads; a held terminal already settled it.
    if (prepared_ && context_.read.state == M::ReadState::ACTIVE && !pendingValid_)
        L::advanceCommunication(context_,
                                why == CommunicationBenchFailure::DEADLINE ? M::ReadOutcome::DEADLINE
                                : why == CommunicationBenchFailure::INVALIDATED
                                    ? M::ReadOutcome::INVALIDATED
                                    : M::ReadOutcome::CANCELLED,
                                now);
    if (prepared_) {
        record_.outcome = context_.read.outcome;
        record_.uncertain = context_.read.uncertain;
    } else if (why == CommunicationBenchFailure::NONE) {
        record_.outcome = M::ReadOutcome::SUCCESS;
    }
    record_.failure = why;
    record_.terminal = true;
    record_.completedUs = now;
    services_.clearUploadExperiment(slot());
}

bool CommunicationReads::service(uint64_t now) noexcept {
    if (now < lastUs_ || now < owner_.lastTimeUs())
        return false;
    if (!occupied_ || record_.terminal) {
        lastUs_ = now;
        return true;
    }
    if (now - lastUs_ > record_.maxServiceGapUs)
        record_.maxServiceGapUs = now - lastUs_;
    lastUs_ = now;
    const uint64_t deadline = record_.request.deadlineUs;
    if (pendingValid_) {
        // The owner settles its own deadline and lifecycle invalidation; a frame on its way is
        // never cancelled here, as that would quarantine the route.
        const MotorControlCANOwner::Completion* done = owner_.result(pending_);
        if (!done)
            return true;
        const bool matched = sameSequence(done->sequence, sequence_) &&
                             done->producer == COMMUNICATION_PRODUCER &&
                             sameWork(done->service.transfer, expected_) &&
                             done->service.nodeGeneration == target_.nodeGeneration;
        if (record_.terminalCount == COMMUNICATION_UPLOADS || !matched) {
            finish(CommunicationBenchFailure::CORRELATION, now);
            return true; // The unmatched terminal stays held until release.
        }
        copyTerminal(record_.terminals[record_.terminalCount++], *done);
        const M::Status consumed = L::advanceCommunication(context_, done->service, now);
        if (!consumed) {
            record_.status = consumed;
            finish(CommunicationBenchFailure::CORRELATION, now);
            return true;
        }
        if (context_.read.state == M::ReadState::FAILED) {
            finish(context_.read.outcome == M::ReadOutcome::DEADLINE ? CommunicationBenchFailure::DEADLINE
                   : context_.read.outcome == M::ReadOutcome::INVALIDATED
                       ? CommunicationBenchFailure::INVALIDATED
                       : CommunicationBenchFailure::SERVICE,
                   now);
            return true; // The failed terminal stays held until release, as in every bench record.
        }
        if (!owner_.release(pending_))
            return false;
        pendingValid_ = false;
        if (context_.read.state == M::ReadState::SUCCEEDED) {
            const M::Status got = L::getCommunication(context_, target_, now, AGE_US, observation_);
            if (!got) {
                record_.status = got;
                finish(CommunicationBenchFailure::PUBLICATION, now);
                return true;
            }
            CommunicationGroupResult& g = record_.groups[record_.published++];
            g = CommunicationGroupResult();
            g.group = observation_.group;
            g.pdoNumber = observation_.pdoNumber;
            g.fieldCount = observation_.fieldCount;
            g.window = observation_.window;
            switch (observation_.group) {
            case L::CommunicationGroup::NETWORK: {
                const L::ConfigObservation& n = observation_.network;
                const uint32_t raw[6] = {n.syncCobId,  n.emcyCobId,      n.producerTimeMs,
                                         n.sdoEntries, n.clientToServer, n.serverToClient};
                std::memcpy(g.raw, raw, sizeof raw);
                break;
            }
            case L::CommunicationGroup::NODE_SETTING:
                g.raw[0] = observation_.nodeSettingRaw;
                break;
            case L::CommunicationGroup::RPDO_COMMUNICATION:
            case L::CommunicationGroup::TPDO_COMMUNICATION: {
                const L::PdoCommunicationObservation& c = observation_.communication;
                const uint32_t raw[6] = {c.entriesRaw,     c.cobIdRaw,         c.transmissionTypeRaw,
                                         c.inhibitTimeRaw, c.compatibilityRaw, c.eventTimerRaw};
                std::memcpy(g.raw, raw, sizeof raw);
                break;
            }
            case L::CommunicationGroup::RPDO_MAPPING:
            case L::CommunicationGroup::TPDO_MAPPING:
                g.raw[0] = observation_.mapping.countRaw;
                for (unsigned i = 0; i < 4; ++i)
                    g.raw[i + 1] = observation_.mapping.words[i];
                break;
            }
            prepared_ = false;
            if (++record_.group == COMMUNICATION_GROUPS) {
                finish(CommunicationBenchFailure::NONE, now);
                return true;
            }
        }
    }
    if (now >= deadline) {
        finish(CommunicationBenchFailure::DEADLINE, now);
        return true;
    }
    if (!current()) {
        finish(CommunicationBenchFailure::INVALIDATED, now);
        return true;
    }
    // A reply can precede the final TX report: the next upload waits for authoritative retirement.
    if (!owner_.localTxRetired(slot()))
        return true;
    if (!prepared_) {
        const GroupStep& step = GROUPS[record_.group];
        const uint32_t operation = record_.request.id + record_.group;
        L::ReadOptions options;
        options.queueTimeoutUs = options.responseTimeoutUs = BUDGET_US;
        options.pdoNumber = step.pdo;
        if (!owner_.beginOperation(sequence_, operation, deadline, now)) {
            finish(CommunicationBenchFailure::PREPARATION, now);
            return true;
        }
        const M::Status prepared =
            L::prepareCommunicationRead(context_, target_, operation, now, deadline, options, step.group);
        if (!prepared) {
            record_.status = prepared;
            finish(CommunicationBenchFailure::PREPARATION, now);
            return true;
        }
        prepared_ = true;
    }
    L::PreparedRead next;
    const M::Status yielded = L::nextCommunication(context_, now, next);
    // Defensive: the typed groups yield uploads only, and this record is never a download producer.
    if (!yielded || !next.ready || next.transfer.direction != C::SdoDirection::UPLOAD) {
        record_.status = yielded;
        finish(CommunicationBenchFailure::PREPARATION, now);
        return true;
    }
    // Q20: every unqualified upload is an explicit reviewed acquisition with its own exact grant,
    // even on a confirmed channel; an accepted (DEC-1) upload needs none.
    if (next.transfer.readEffect == C::SdoReadEffect::UNQUALIFIED) {
        if (record_.grants >= COMMUNICATION_UPLOADS ||
            !services_.authorizeUploadExperiment(slot(), next.transfer, target_.nodeGeneration,
                                                 record_.request.firstGrant + record_.grants, now)) {
            finish(CommunicationBenchFailure::AUTHORIZATION, now);
            return true;
        }
        ++record_.grants;
    }
    BusRequest request;
    request.sequence = sequence_;
    request.transfer = next.transfer;
    record_.admission = owner_.admit(request, now, pending_);
    if (record_.admission != BusAdmission::ACCEPTED) {
        finish(CommunicationBenchFailure::ADMISSION, now);
        return true;
    }
    expected_ = next.transfer;
    pendingValid_ = true;
    return true;
}

bool CommunicationReads::formatLine(unsigned cursor, char* out, size_t size) const noexcept {
    if (!occupied_ || !record_.terminal || !out || !size)
        return false;
    typedef unsigned long U32;
    typedef unsigned long long U64;
    const unsigned node = record_.request.node;
    const unsigned firstResult = 1u + record_.published;
    int count = 0;
    if (cursor == 0) {
        count = std::snprintf(
            out, size,
            "COMM id=%lu node=%u failure=%u groups=%u published=%u group=%u outcome=%u uncertain=%u "
            "status=%u detail=%ld terminals=%u grants=%u admission=%u gap=%llu\n",
            U32(record_.request.id), node, unsigned(record_.failure), unsigned(COMMUNICATION_GROUPS),
            unsigned(record_.published), unsigned(record_.group), unsigned(record_.outcome),
            unsigned(record_.uncertain), unsigned(record_.status.code), long(record_.status.detail),
            unsigned(record_.terminalCount), unsigned(record_.grants), unsigned(record_.admission),
            U64(record_.maxServiceGapUs));
    } else if (cursor < firstResult) {
        // Typed values from getCommunication, in raw native form: no unit, route or map adopted.
        const CommunicationGroupResult& g = record_.groups[cursor - 1];
        const uint32_t* v = g.raw;
        const U64 lo = U64(g.window.earliestUs), hi = U64(g.window.latestUs), skew = U64(g.window.skewUs);
        switch (g.group) {
        case L::CommunicationGroup::NETWORK:
            count = std::snprintf(out, size,
                                  "COMM_NETWORK node=%u sync=%08lx emcy=%08lx producer=%lu sdo=%lu "
                                  "cs=%08lx sc=%08lx lo=%llu hi=%llu skew=%llu\n",
                                  node, U32(v[0]), U32(v[1]), U32(v[2]), U32(v[3]), U32(v[4]),
                                  U32(v[5]), lo, hi, skew);
            break;
        case L::CommunicationGroup::NODE_SETTING:
            count = std::snprintf(out, size, "COMM_NODE node=%u raw=%lu lo=%llu hi=%llu skew=%llu\n",
                                  node, U32(v[0]), lo, hi, skew);
            break;
        case L::CommunicationGroup::RPDO_COMMUNICATION:
        case L::CommunicationGroup::TPDO_COMMUNICATION:
            count = std::snprintf(out, size,
                                  "COMM_PDO node=%u dir=%c pdo=%u entries=%lu cob=%08lx type=%lu "
                                  "inhibit=%lu compat=%lu event=%lu lo=%llu hi=%llu skew=%llu\n",
                                  node, direction(g.group), unsigned(g.pdoNumber), U32(v[0]),
                                  U32(v[1]), U32(v[2]), U32(v[3]), U32(v[4]), U32(v[5]), lo, hi, skew);
            break;
        case L::CommunicationGroup::RPDO_MAPPING:
        case L::CommunicationGroup::TPDO_MAPPING:
            count = std::snprintf(out, size,
                                  "COMM_MAP node=%u dir=%c pdo=%u count=%lu w1=%08lx w2=%08lx "
                                  "w3=%08lx w4=%08lx lo=%llu hi=%llu skew=%llu\n",
                                  node, direction(g.group), unsigned(g.pdoNumber), U32(v[0]),
                                  U32(v[1]), U32(v[2]), U32(v[3]), U32(v[4]), lo, hi, skew);
            break;
        }
    } else if (cursor < firstResult + record_.terminalCount) {
        count = formatTerminal(out, size, record_.terminals[cursor - firstResult]);
    } else if (cursor == firstResult + record_.terminalCount) {
        count = std::snprintf(out, size,
                              "END comm terminals=%u groups=%u; typed uploads only, nothing written; "
                              "r=release\n",
                              unsigned(record_.terminalCount), unsigned(record_.published));
    } else {
        return false;
    }
    return fitLine(count, out, size);
}

bool CommunicationReads::release() noexcept {
    if (!occupied_ || !record_.terminal || !owner_.localTxRetired(slot()))
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

// ------------------------------------------------------------------- H: heartbeat commissioning
HeartbeatCommission::HeartbeatCommission(CanBusOwner& owner, C::SdoClient& services) noexcept
    : owner_(owner), services_(services) {}

L::ReadTarget HeartbeatCommission::currentTarget() const noexcept {
    L::ReadTarget t = record_.reservation.target;
    t.channel.binding = record_.current;
    return t;
}

bool HeartbeatCommission::current() const noexcept {
    const C::SdoChannel* route = services_.channel(slot());
    const C::NodeSnapshot* node = services_.node(slot());
    C::SdoChannel expected = record_.reservation.target.channel;
    if (record_.migrated)
        expected.binding = record_.current;
    return route && node && !node->exhausted && sameChannel(*route, expected) &&
           node->route.generation == record_.reservation.target.nodeGeneration &&
           owner_.sequence(sequence_);
}

const char* HeartbeatCommission::migrationRefusal(uint8_t s, uint64_t now) const noexcept {
    // The adapter's own decision on the node premises its epoch migration checks after the write.
    // Ordinary service alone is not enough: an N line's pending transition refuses it too.
    const C::SdoChannel* channel = services_.channel(s);
    if (channel && services_.configurationEligible(s, channel->binding, now))
        return nullptr;
    // Only the console word below; the decision above is the adapter's.
    const C::NodeSnapshot* node = services_.node(s);
    C::ServiceEligibility eligibility;
    if (!node || !C::serviceEligibility(*node, now, eligibility) || !eligibility.ordinarySdo)
        return "service";
    return node->transitionPending ? "transition" : "configuration";
}

void HeartbeatCommission::currentNmt(uint64_t now, L::CommunicationNmt& out) const noexcept {
    // The owner's actual observation: known only for a current qualified heartbeat or boot-up.
    // Anything else, an uncomputable window included, is unknown and never Operational.
    out = L::CommunicationNmt();
    const C::NodeSnapshot* node = services_.node(slot());
    C::ServiceEligibility eligibility;
    if (node && C::serviceEligibility(*node, now, eligibility) && eligibility.exactNmtKnown) {
        out.known = true;
        out.state = eligibility.state;
        out.time = node->nmtTime;
    }
}

const char* HeartbeatCommission::begin(const HeartbeatBenchRequest& r, uint64_t now) noexcept {
    if (occupied_)
        return "busy";
    // Prompt 38 reviewed exactly raw 0 to raw 1000; there is no other value and no reverse.
    if (r.producerTimeRaw != 1000)
        return "value";
    if (!r.id || r.id <= lastId_ || r.id == UINT32_MAX || r.node < 1 || r.node > BENCH_NODES ||
        !r.firstGrant || r.firstGrant > UINT32_MAX - (HEARTBEAT_TERMINALS - 1) ||
        r.deadlineUs <= now || now < lastUs_ || now < owner_.lastTimeUs())
        return "invalid";
    const uint8_t s = uint8_t(r.node - 1);
    const C::SdoChannel* channel = services_.channel(s);
    const C::NodeSnapshot* node = services_.node(s);
    if (!channel || !node || node->exhausted || channel->binding.endpoint.address != r.node ||
        channel->binding.configurationGeneration == UINT32_MAX)
        return "route";
    if (services_.unresolved(s))
        return "unresolved";
    // Without its premises the migration after an acknowledged write would be refused.
    if (const char* refusal = migrationRefusal(s, now))
        return refusal;
    // After the bench premises the operator acts on: a grant refusal is the image's own numbering.
    if (const char* refusal = grantRefusal(services_, s, r.id, r.firstGrant))
        return refusal;
    SequenceId sequence;
    if (!owner_.beginSequence(channel->binding, COMMUNICATION_PRODUCER, now, sequence))
        return sequenceRefusal(owner_, channel->binding);
    const Sequence* held = owner_.sequence(sequence);
    if (!held || !owner_.beginOperation(sequence, r.id, r.deadlineUs, now)) {
        owner_.releaseSequence(sequence);
        return "invalid";
    }
    sequence_ = sequence;
    record_.request = r;
    // The continuous exclusion the baseline and every later step run under.
    record_.reservation.target.channel = *channel;
    record_.reservation.target.nodeGeneration = node->route.generation;
    record_.reservation.model = L::SettingsModel::CL86_C;
    record_.reservation.owner = owner_.identity();
    record_.reservation.sequence = uint32_t(sequence.slot) + 1;
    record_.reservation.generation = sequence.generation;
    record_.reservation.acquiredUs = held->acquiredUs;
    // Bench attestations: fixture OPTAK-S2-20261102-two-CL86, the 2026-11-04 identity record of
    // both specimens, the prompt 38 procedure and the 2026-11-02 standing authorization.
    new (&record_.review) L::CommunicationReview();
    record_.review.target = record_.reservation.target;
    record_.review.scope = L::CommunicationScope::ISOLATED_EXPERIMENT;
    record_.review.fixtureRecord = 20261102;
    record_.review.firmwareRecord = 20261104;
    record_.review.procedureRecord = 38;
    record_.review.authorizationRecord = 20261102;
    new (&record_.nmt) L::CommunicationNmt();
    new (&record_.context) L::CommunicationContext();
    new (&record_.result) L::CommunicationResult();
    record_.current = channel->binding;
    record_.status = M::Ok();
    record_.prepared = record_.migrated = record_.published = false;
    record_.terminal = record_.evidenceLost = record_.tailOverflow = false;
    record_.failure = HeartbeatBenchFailure::NONE;
    record_.admission = BusAdmission::ACCEPTED;
    record_.startedUs = now;
    record_.completedUs = record_.maxServiceGapUs = 0;
    record_.attempted = record_.terminalCount = record_.trafficCount = record_.tailCount = 0;
    record_.droppedTraffic = 0;
    Diagnostic old;
    for (unsigned i = 0; i < CanBusOwner::TRACE_CAPACITY && owner_.takeDiagnostic(old); ++i) {
    }
    overwritten_ = owner_.stats().traceOverwritten;
    rxLost_ = owner_.stats().rxLost;
    occupied_ = true;
    pendingValid_ = false;
    requested_ = HeartbeatBenchFailure::NONE;
    lastUs_ = now;
    lastId_ = r.id + 1;
    if (overwritten_ == UINT32_MAX || rxLost_ == UINT32_MAX)
        lose(1, false);
    return nullptr;
}

void HeartbeatCommission::lose(uint32_t count, bool tail) noexcept {
    if (tail) {
        record_.tailOverflow = true;
        return;
    }
    record_.evidenceLost = true;
    record_.droppedTraffic =
        UINT32_MAX - record_.droppedTraffic < count ? UINT32_MAX : record_.droppedTraffic + count;
}

void HeartbeatCommission::capture(bool tail) noexcept {
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
            if (record_.tailCount == HEARTBEAT_TAIL)
                lose(1, true);
            else
                copyTraffic(record_.tail[record_.tailCount++], in);
        } else if (record_.trafficCount == HEARTBEAT_TRAFFIC) {
            lose(1, false);
        } else {
            copyTraffic(record_.traffic[record_.trafficCount++], in);
        }
    }
}

void HeartbeatCommission::finish(HeartbeatBenchFailure why, uint64_t now) noexcept {
    L::CommunicationContext& c = record_.context;
    if (record_.prepared && (c.state == L::CommunicationState::WRITING ||
                             c.state == L::CommunicationState::AWAITING_CONFIRMATION ||
                             c.state == L::CommunicationState::READING_BACK))
        // Local withdrawal keeps every terminal and any possible effect; it is no motor action.
        L::advanceCommunication(c,
                                why == HeartbeatBenchFailure::DEADLINE ? M::ReadOutcome::DEADLINE
                                : why == HeartbeatBenchFailure::INVALIDATED
                                    ? M::ReadOutcome::INVALIDATED
                                    : M::ReadOutcome::CANCELLED,
                                now);
    record_.failure = why;
    record_.terminal = true;
    record_.completedUs = now;
    services_.clearUploadExperiment(slot());
}

bool HeartbeatCommission::withdraw(uint64_t now) noexcept {
    // The download may change producer timing, so no earlier producer fact may survive it, and
    // the node's PDO confidence goes too. Neither is reinstated: a readback is not active timing.
    const C::NodeSnapshot* node = services_.node(slot());
    if (!node || !services_.invalidateProducerInterval(slot(), node->route, now))
        return false;
    const M::Endpoint& endpoint = record_.reservation.target.channel.binding.endpoint;
    for (uint8_t i = 0; i < C::SdoClient::PDO_ROUTES; ++i) {
        const C::PdoRecord* pdo = services_.pdo(i);
        if (pdo && pdo->map.route.binding.endpoint.bus == endpoint.bus &&
            pdo->map.route.binding.endpoint.address == endpoint.address &&
            !services_.invalidatePdo(i, true, now))
            return false;
    }
    return true;
}

bool HeartbeatCommission::preflight(SequenceId sequence, const C::SdoTransfer& transfer,
                                    uint64_t now) const noexcept {
    if (!occupied_ || record_.terminal || record_.evidenceLost || !pendingValid_ ||
        requested_ != HeartbeatBenchFailure::NONE || !record_.prepared ||
        record_.context.state != L::CommunicationState::WRITING ||
        !record_.context.transmissionNoted || now < lastUs_ || now >= record_.request.deadlineUs ||
        !sameSequence(sequence, sequence_) || !sameDownload(expected_, transfer) || !current())
        return false;
    // The profile's own eligibility at this instant: same reservation, binding, budget and NMT.
    L::CommunicationNmt nmt;
    currentNmt(now, nmt);
    L::PreparedRead check;
    return L::nextCommunication(record_.context, record_.reservation, record_.current, nmt, now,
                                check) &&
           check.ready && sameDownload(check.transfer, transfer);
}

bool HeartbeatCommission::service(uint64_t now) noexcept {
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
    const uint64_t deadline = record_.request.deadlineUs;
    if (record_.evidenceLost)
        requested_ = HeartbeatBenchFailure::EVIDENCE_LOSS;
    else if (now >= deadline)
        requested_ = HeartbeatBenchFailure::DEADLINE;
    // A completed failure can itself quarantine the route: keep that native cause first.
    const MotorControlCANOwner::Completion* available = pendingValid_ ? owner_.result(pending_) : nullptr;
    if (!available && requested_ == HeartbeatBenchFailure::NONE && !current())
        requested_ = HeartbeatBenchFailure::INVALIDATED;
    // Losing a diagnostic copy is no reason to cancel a frame already on its way.
    if (requested_ != HeartbeatBenchFailure::NONE && pendingValid_ && !available &&
        (requested_ != HeartbeatBenchFailure::EVIDENCE_LOSS || !owner_.mayHaveTransmitted(pending_)))
        owner_.cancel(pending_, now);
    if (pendingValid_) {
        const MotorControlCANOwner::Completion* done = owner_.result(pending_);
        if (!done)
            return true;
        const bool matched = sameSequence(done->sequence, sequence_) &&
                             done->producer == COMMUNICATION_PRODUCER &&
                             sameWork(done->service.transfer, expected_) &&
                             done->service.nodeGeneration == record_.reservation.target.nodeGeneration;
        if (record_.terminalCount == HEARTBEAT_TERMINALS) {
            finish(HeartbeatBenchFailure::CORRELATION, now);
            return true;
        }
        copyTerminal(record_.terminals[record_.terminalCount++], *done);
        if (matched && record_.prepared) {
            const M::Status consumed = L::advanceCommunication(
                record_.context, record_.reservation, record_.current, done->service, now);
            if (!consumed) {
                record_.status = consumed;
                if (requested_ == HeartbeatBenchFailure::NONE)
                    requested_ = HeartbeatBenchFailure::CONFIGURATION;
            }
        } else if (matched && done->service.outcome == C::SdoOutcome::SUCCESS) {
            baseline_ = done->service; // The exact baseline terminal, checked by the profile next.
        }
        if (!matched)
            requested_ = HeartbeatBenchFailure::CORRELATION;
        if (requested_ != HeartbeatBenchFailure::NONE ||
            done->service.outcome != C::SdoOutcome::SUCCESS) {
            finish(requested_ != HeartbeatBenchFailure::NONE ? requested_
                   : done->service.outcome == C::SdoOutcome::REBOOT ||
                           done->service.outcome == C::SdoOutcome::BINDING_INVALIDATED
                       ? HeartbeatBenchFailure::INVALIDATED
                       : HeartbeatBenchFailure::SERVICE,
                   now);
            return true; // Retain the final owner result until explicit release.
        }
        if (!owner_.release(pending_))
            return false;
        pendingValid_ = false;
        if (record_.prepared && record_.context.state == L::CommunicationState::FAILED) {
            finish(HeartbeatBenchFailure::READBACK, now);
            return true;
        }
        if (record_.prepared && record_.context.state == L::CommunicationState::SUCCEEDED) {
            const M::Status got = L::getCommunication(record_.context, record_.reservation,
                                                      currentTarget(), now, record_.result);
            if (!got) {
                record_.status = got;
                finish(HeartbeatBenchFailure::PUBLICATION, now);
                return true;
            }
            record_.published = true;
            finish(HeartbeatBenchFailure::NONE, now);
            return true;
        }
    }
    if (requested_ == HeartbeatBenchFailure::NONE && !current())
        requested_ = HeartbeatBenchFailure::INVALIDATED;
    if (requested_ != HeartbeatBenchFailure::NONE) {
        finish(requested_, now);
        return true;
    }
    // A reply can precede the final TX report; the migration and every next step wait for it.
    if (!owner_.localTxRetired(slot()))
        return true;
    L::CommunicationNmt nmt;
    currentNmt(now, nmt);
    C::SdoTransfer work;
    if (record_.terminalCount == 0) {
        // A known current state other than Pre-operational refuses before anything is sent:
        // Stopped or Initializing never become serviceable by a read here.
        if (nmt.known && nmt.state != C::NmtState::PRE_OPERATIONAL) {
            record_.nmt = nmt;
            finish(HeartbeatBenchFailure::NMT, now);
            return true;
        }
        record_.nmt = nmt;
        if (migrationRefusal(slot(), now)) {
            finish(HeartbeatBenchFailure::ELIGIBILITY, now);
            return true;
        }
        work.channel = record_.reservation.target.channel;
        work.operation = record_.request.id;
        work.request = work.step = 1;
        work.index = 0x1017;
        work.subindex = 0;
        work.size = 2;
        work.uploadSize = C::SdoUploadSize::EXACT;
        work.readEffect = C::SdoReadEffect::UNQUALIFIED;
        work.requirePreOperational = nmt.known;
        work.deadlineUs = deadline;
        work.queueTimeoutUs = work.responseTimeoutUs = BUDGET_US;
    } else {
        if (!record_.prepared) {
            // Every candidate, epoch, baseline value and NMT premise is checked before any write.
            if (!owner_.beginOperation(sequence_, record_.request.id + 1, deadline, now)) {
                finish(HeartbeatBenchFailure::CONFIGURATION, now);
                return true;
            }
            L::CommunicationRequest request;
            request.producerTimeRaw = record_.request.producerTimeRaw;
            L::ReadOptions options;
            options.queueTimeoutUs = options.responseTimeoutUs = BUDGET_US;
            record_.nmt = nmt;
            const M::Status prepared = L::prepareCommunication(
                record_.context, record_.reservation, record_.review, request, baseline_, nmt,
                record_.request.id + 1, now, deadline, options, AGE_US);
            if (!prepared) {
                record_.status = prepared;
                finish(HeartbeatBenchFailure::BASELINE, now);
                return true;
            }
            record_.prepared = true;
        }
        if (record_.context.state == L::CommunicationState::AWAITING_CONFIRMATION) {
            // The acknowledged write moved the application epoch at its notice; the idle owner
            // follows only now, after the write's result is released and its TX retired.
            if (!record_.migrated) {
                if (!owner_.advanceConfiguration(sequence_, record_.current, now)) {
                    finish(HeartbeatBenchFailure::CONFIGURATION, now);
                    return true;
                }
                record_.migrated = true;
            }
            const M::Status armed = L::prepareCommunicationConfirmation(
                record_.context, record_.reservation, currentTarget(), nmt, now);
            if (!armed) {
                record_.status = armed;
                finish(HeartbeatBenchFailure::CONFIGURATION, now);
                return true;
            }
        }
        L::PreparedRead next;
        const M::Status yielded = L::nextCommunication(record_.context, record_.reservation,
                                                       record_.current, nmt, now, next);
        if (!yielded || !next.ready) {
            record_.status = yielded;
            finish(HeartbeatBenchFailure::CONFIGURATION, now);
            return true;
        }
        work = next.transfer;
        // The last point at which a refused migration can still be avoided: nothing written yet.
        if (work.direction == C::SdoDirection::DOWNLOAD && migrationRefusal(slot(), now)) {
            finish(HeartbeatBenchFailure::ELIGIBILITY, now);
            return true;
        }
    }
    const uint32_t grant = record_.request.firstGrant + record_.attempted++;
    const bool granted =
        work.direction == C::SdoDirection::DOWNLOAD
            ? services_.authorizeDownloadExperiment(slot(), work,
                                                    record_.reservation.target.nodeGeneration,
                                                    grant, now)
            : services_.authorizeUploadExperiment(slot(), work,
                                                  record_.reservation.target.nodeGeneration,
                                                  grant, now);
    if (!granted) {
        finish(HeartbeatBenchFailure::AUTHORIZATION, now);
        return true;
    }
    BusRequest request;
    request.sequence = sequence_;
    request.transfer = work;
    record_.admission = owner_.admit(request, now, pending_);
    if (record_.admission != BusAdmission::ACCEPTED) {
        finish(HeartbeatBenchFailure::ADMISSION, now);
        return true;
    }
    expected_ = work;
    pendingValid_ = true;
    // Before the next owner turn can hand the download off: withdraw dependent facts, then the
    // profile's notice advances the application epoch exactly once. A refusal cancels the request.
    if (work.direction == C::SdoDirection::DOWNLOAD &&
        (!withdraw(now) || !L::noteCommunicationTransmission(record_.context, record_.reservation,
                                                             record_.current, nmt, now))) {
        requested_ = HeartbeatBenchFailure::CONFIGURATION;
        owner_.cancel(pending_, now);
    }
    return true;
}

bool HeartbeatCommission::formatLine(unsigned cursor, char* out, size_t size) const noexcept {
    if (!occupied_ || !record_.terminal || !out || !size)
        return false;
    typedef unsigned long U32;
    typedef unsigned long long U64;
    const L::CommunicationContext& c = record_.context;
    const unsigned terminals = record_.terminalCount, traffic = record_.trafficCount;
    const unsigned tail = record_.tailCount;
    int count = 0;
    if (cursor == 0) {
        // The workflow result; the profile's own preparation facts follow on HB_PREP.
        count = std::snprintf(
            out, size,
            "HB id=%lu node=%u failure=%u lost=%u state=%u outcome=%u write=%u ack=%u effects=%u "
            "uncertain=%u known=%u readback=%u matched=%u published=%u raw=%u terminals=%u gap=%llu\n",
            U32(record_.request.id), unsigned(record_.request.node), unsigned(record_.failure),
            unsigned(record_.evidenceLost), unsigned(c.state), unsigned(c.outcome),
            unsigned(c.writeOutcome), unsigned(c.writeAcknowledged), unsigned(c.effectsPossible),
            unsigned(c.uncertain), unsigned(c.readbackKnown), unsigned(c.readbackRaw),
            unsigned(c.readbackMatched), unsigned(record_.published),
            unsigned(record_.published ? record_.result.producerTimeRaw : 0), terminals,
            U64(record_.maxServiceGapUs));
    } else if (cursor == 1) {
        // The NMT observation the profile was given, the epoch migration, the review scope and the
        // unknown restart/persistence requirements, and the last refused profile call.
        count = std::snprintf(
            out, size,
            "HB_PREP node=%u nmt=%u nmtState=%u prepared=%u migrated=%u cfg=%lu scope=%u restart=%u "
            "persistence=%u status=%u detail=%ld\n",
            unsigned(record_.request.node), unsigned(record_.nmt.known), unsigned(record_.nmt.state),
            unsigned(record_.prepared), unsigned(record_.migrated),
            U32(record_.current.configurationGeneration), unsigned(record_.review.scope),
            unsigned(c.restart), unsigned(c.persistence), unsigned(record_.status.code),
            long(record_.status.detail));
    } else if (cursor <= terminals + 1) {
        count = formatTerminal(out, size, record_.terminals[cursor - 2]);
    } else if (cursor <= terminals + traffic + tail + 1) {
        const unsigned index = cursor - terminals - 2;
        const bool isTail = index >= traffic;
        count = formatTraffic(out, size, isTail ? record_.tail[index - traffic] : record_.traffic[index],
                              isTail ? "TAIL" : "TRAFFIC");
    } else if (cursor == terminals + traffic + tail + 2) {
        count = std::snprintf(out, size,
                              "END heartbeat terminals=%u traffic=%u tail=%u tailLost=%u lost=%u; "
                              "no save, reset or restore; r=release\n",
                              terminals, traffic, tail, unsigned(record_.tailOverflow),
                              unsigned(record_.evidenceLost));
    } else {
        return false;
    }
    return fitLine(count, out, size);
}

bool HeartbeatCommission::release() noexcept {
    if (!occupied_ || !record_.terminal || !owner_.localTxRetired(slot()))
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
