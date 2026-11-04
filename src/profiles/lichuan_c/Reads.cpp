// SPDX-License-Identifier: MIT
#include "ReadPlan.h"
#include <MotorControlCAN/profiles/lichuan_c/Pdo.h>
#include <new>

namespace MotorControlCAN {
namespace LICHUAN_C {
using namespace Detail;
namespace {
Status invalid(ReadError detail, const char* message) {
    return readInvalid(detail, message);
}
bool validTarget(const ReadTarget& target) {
    return target.nodeGeneration && CANOPEN::validateSdoChannel(target.channel).isOk();
}
bool sameDescriptor(const ReadDescriptor& a, const ReadDescriptor& b) {
    return a.index == b.index && a.subindex == b.subindex && a.widthBytes == b.widthBytes &&
           a.representation == b.representation && a.readEffect == b.readEffect &&
           a.scope == b.scope;
}
const Address identity[] = {{0x1000, 0}, {0x1009, 0}, {0x100a, 0}, {0x1018, 0}, {0x1018, 1},
                            {0x1018, 2}, {0x1018, 3}, {0x2000, 0}, {0x2001, 0}};
const Address configuration[] = {{0x1005, 0}, {0x1014, 0}, {0x1017, 0},
                                 {0x1200, 0}, {0x1200, 1}, {0x1200, 2}};
const Address state[] = {{0x1001, 0}, {0x603f, 0}, {0x1003, 0}, {0x1003, 1}, {0x1003, 2},
                         {0x1003, 3}, {0x1003, 4}, {0x2100, 0}, {0x2101, 0}, {0x2102, 0},
                         {0x6041, 0}, {0x6061, 0}, {0x6064, 0}, {0x606c, 0}, {0x60fd, 0}};
const uint8_t starts[STATE_BLOCKS] = {0, 2, 7, 10, 12, 14};
const uint8_t lengths[STATE_BLOCKS] = {2, 5, 3, 2, 2, 1};
// The fixed address list of every kind but STATE, whose blocks are selected by mask.
const Address* fixedPlan(ReadKind kind, unsigned& count) {
    if (const GroupPlan* plan = groupPlan(kind)) {
        count = plan->count;
        return plan->fields;
    }
    switch (kind) {
    case ReadKind::IDENTITY:
        count = 9;
        return identity;
    case ReadKind::CONFIG:
        count = 6;
        return configuration;
    case ReadKind::PROBE:
        count = 1;
        return identity;
    default:
        count = 0;
        return nullptr;
    }
}

// Visits the selected plan in slot order without touching any context.
template <class Visit> bool visitPlan(ReadKind kind, uint8_t mask, Visit visit) {
    if (kind == ReadKind::STATE) {
        if (!mask || (mask & 0xc0u))
            return false;
        for (unsigned b = 0; b < STATE_BLOCKS; ++b) {
            if (!(mask & (1u << b)))
                continue;
            for (unsigned i = 0; i < lengths[b]; ++i) {
                ReadDescriptor object;
                const Address address = state[starts[b] + i];
                if (!readDescriptor(address.index, address.subindex, object) ||
                    !visit(object, b, i))
                    return false;
            }
        }
        return true;
    }
    unsigned count;
    const Address* fields = fixedPlan(kind, count);
    if (!fields)
        return false;
    for (unsigned i = 0; i < count; ++i) {
        ReadDescriptor object;
        if (!readDescriptor(fields[i].index, fields[i].subindex, object) ||
            !visit(object, unsigned(STATE_BLOCKS), i))
            return false;
    }
    return true;
}
struct PlanWriter {
    ReadContext& c;
    bool operator()(const ReadDescriptor& object, unsigned block, unsigned slot) const {
        if (block < STATE_BLOCKS && !slot) {
            c.blockStart[block] = c.count;
            c.blockLength[block] = lengths[block];
        }
        c.values[c.count++].object = object;
        return true;
    }
};
struct PlanNoop {
    bool operator()(const ReadDescriptor&, unsigned, unsigned) const { return true; }
};
struct RouteFields {
    const CANOPEN::PdoLayout& checked;
    bool operator()(const ReadDescriptor& object, unsigned, unsigned) const {
        for (unsigned j = 0; j < checked.mapping.count; ++j)
            if (checked.fields[j].index == object.index &&
                checked.fields[j].subindex == object.subindex &&
                checked.fields[j].widthBytes == object.widthBytes)
                return true;
        return false;
    }
};
// Each width option is a known enumerator, and a measured width only on the kind it concerns.
bool widthsFit(ReadKind kind, const ReadOptions& o) {
    return o.identityWidths <= IdentityWidths::MEASURED_U8 &&
           (o.identityWidths == IdentityWidths::DECLARED_U16 || kind == ReadKind::IDENTITY) &&
           o.countWidths <= CountWidths::MEASURED_U16 &&
           (o.countWidths == CountWidths::DECLARED_U8 || kind == ReadKind::DRIVER_SOFT_LIMITS ||
            kind == ReadKind::IO_OUTPUT_COMMAND);
}
bool shape(const ReadContext& c) {
    if (!validTarget(c.target) || !c.operation || !c.count || c.count > READ_FIELDS ||
        c.cursor > c.count || c.state == ReadState::EMPTY || c.state > ReadState::FAILED ||
        c.source > ReadSource::TPDO || c.deadlineUs <= c.startedUs || c.servicedUs < c.startedUs ||
        !c.options.queueTimeoutUs || !c.options.responseTimeoutUs || !widthsFit(c.kind, c.options) ||
        (c.kind != ReadKind::STATE && c.blockMask))
        return false;
    unsigned cursor = 0;
    for (unsigned b = 0; b < (c.kind == ReadKind::STATE ? STATE_BLOCKS : 1); ++b) {
        const Address* addresses = nullptr;
        unsigned count = 0;
        if (c.kind == ReadKind::STATE) {
            if (!c.blockMask || (c.blockMask & 0xc0u))
                return false;
            if (!(c.blockMask & (1u << b))) {
                if (c.blockLength[b])
                    return false;
                continue;
            }
            if (c.blockStart[b] != cursor || c.blockLength[b] != lengths[b])
                return false;
            addresses = state + starts[b];
            count = lengths[b];
        } else {
            addresses = fixedPlan(c.kind, count);
            if (!addresses)
                return false;
        }
        for (unsigned i = 0; i < count; ++i) {
            if (cursor >= c.count)
                return false;
            ReadDescriptor expected;
            if (!readDescriptor(addresses[i].index, addresses[i].subindex, expected))
                return false;
            if (const uint8_t width =
                    measuredWidth(c.kind, addresses[i].index, addresses[i].subindex, c.options))
                expected.widthBytes = width;
            if (!sameDescriptor(expected, c.values[cursor++].object))
                return false;
        }
    }
    return cursor == c.count;
}
bool required(const ReadContext& c, unsigned slot) {
    if (c.kind != ReadKind::STATE ||
        !(c.blockMask & (1u << static_cast<unsigned>(StateBlock::ERROR_HISTORY))))
        return true;
    const unsigned start = c.blockStart[static_cast<unsigned>(StateBlock::ERROR_HISTORY)];
    return slot <= start || slot >= start + 5 || !c.values[start].valid ||
           slot - start <= c.values[start].raw;
}
void progress(ReadContext& c) {
    while (c.cursor < c.count && (c.values[c.cursor].valid || !required(c, c.cursor)))
        ++c.cursor;
    if (c.cursor == c.count) {
        c.state = ReadState::SUCCEEDED;
        c.outcome = ReadOutcome::SUCCESS;
        c.status = Ok();
    }
}
CANOPEN::SdoTransfer work(const ReadContext& c) {
    CANOPEN::SdoTransfer out;
    out.channel = c.target.channel;
    out.operation = out.request = c.operation;
    out.step = c.cursor + 1;
    const ReadDescriptor& object = c.values[c.cursor].object;
    out.index = object.index;
    out.subindex = object.subindex;
    out.size = object.widthBytes;
    // One to one: no descriptor value may ever claim the absence of an effect.
    out.readEffect = object.readEffect == ReadEffect::ACCEPTED_UNQUALIFIED
                         ? CANOPEN::SdoReadEffect::ACCEPTED_UNQUALIFIED
                         : CANOPEN::SdoReadEffect::UNQUALIFIED;
    out.deadlineUs = c.deadlineUs;
    out.queueTimeoutUs = c.options.queueTimeoutUs;
    out.responseTimeoutUs = c.options.responseTimeoutUs;
    return out;
}
bool qualified(const ReadContext& c, const CANOPEN::PdoRecord& record, uint64_t now) {
    CANOPEN::PdoFreshness freshness;
    const CANOPEN::PdoQualification& q = record.qualification;
    const CANOPEN::PdoObservation& sample = record.observation;
    return record.confirmed && record.qualified && record.samplePresent &&
           record.map.communicationKnown &&
           (record.map.source == CANOPEN::PdoMapSource::REVIEWED_CONFIGURATION ||
            record.map.source == CANOPEN::PdoMapSource::SDO_READBACK) &&
           record.map.evidenceId && q.evidenceId &&
           (record.map.cobIdParameter & 0xbffff800u) == 0 &&
           (record.map.cobIdParameter & 0x7ffu) == c.pdoRoute.cobId &&
           (record.map.transmissionType <= 240 || record.map.transmissionType >= 254) &&
           record.map.startedUs <= record.map.observedUs &&
           record.map.observedUs <= record.boundaryUs && record.boundaryUs <= q.establishedUs &&
           record.map.observedUs <= now && record.map.nodeGeneration == c.target.nodeGeneration &&
           q.nodeGeneration == c.target.nodeGeneration &&
           (q.basis == CANOPEN::PdoSampleBasis::PRODUCER_AND_INGRESS_RETIRED ||
            q.basis == CANOPEN::PdoSampleBasis::DISTINCT_VERIFIED_ROUTE) &&
           CANOPEN::samePdoRoute(c.pdoRoute, record.map.route) &&
           CANOPEN::samePdoRoute(c.pdoRoute, q.route) &&
           CANOPEN::samePdoRoute(c.pdoRoute, sample.sample.route) && sample.time.present &&
           sample.time.generation == c.pdoRoute.generation && sample.time.receivedTimeKnown &&
           sample.time.receivedUs >= c.startedUs &&
           sample.time.receivedUs <= sample.time.processedUs && sample.evidenceId == q.evidenceId &&
           sample.time.receivedUs > record.boundaryUs && sample.time.receivedUs > q.establishedUs &&
           CANOPEN::pdoFreshness(record, now, UINT64_MAX, freshness) &&
           freshness == CANOPEN::PdoFreshness::FRESH;
}
// Everything preparation can refuse, with no output to touch.
Status validateInputs(const ReadTarget& target, uint32_t operation, uint64_t now, uint64_t deadline,
                      const ReadOptions& options, ReadKind kind, uint8_t mask,
                      const CANOPEN::PdoRoute* route) {
    if (!validTarget(target))
        return invalid(ReadError::TARGET, "invalid read target or node epoch");
    if (!operation || deadline <= now || !options.queueTimeoutUs || !options.responseTimeoutUs ||
        !widthsFit(kind, options))
        return invalid(ReadError::ARGUMENT, "invalid read identity or budgets");
    if (!visitPlan(kind, mask, PlanNoop()))
        return invalid(ReadError::ARGUMENT, "invalid read selection");
    if (kind == ReadKind::PROBE) {
        ReadDescriptor object;
        if (!readDescriptor(identity[0].index, identity[0].subindex, object) ||
            object.readEffect != ReadEffect::ACCEPTED_UNQUALIFIED)
            return Status(Err::UNSUPPORTED, static_cast<int32_t>(ReadError::READ_EFFECT),
                          "Q20: probe object is not on the accepted read list");
    }
    if (route) {
        CANOPEN::PdoLayout checked;
        if (kind != ReadKind::STATE || !CANOPEN::validatePdoRoute(*route) ||
            route->layout.mapping.direction != CANOPEN::PdoDirection::TPDO ||
            !sameBinding(route->binding, target.channel.binding) ||
            !CANOPEN::validatePdoMap(route->layout.mapping,
                                     pdoProfile(route->binding.endpoint.profile), checked))
            return invalid(ReadError::TARGET, "invalid reviewed TPDO source");
        CANOPEN::PdoRoute rebuilt = *route;
        rebuilt.layout = checked;
        if (!CANOPEN::samePdoRoute(rebuilt, *route))
            return invalid(ReadError::TARGET, "TPDO field facts differ from reviewed profile");
        if (!visitPlan(kind, mask, RouteFields{checked}))
            return invalid(ReadError::ARGUMENT, "selected state fields are absent from TPDO");
    }
    return Ok();
}
Status prepare(ReadContext& output, const ReadTarget& target, uint32_t operation, uint64_t now,
               uint64_t deadline, const ReadOptions& options, ReadKind kind, uint8_t mask,
               const CANOPEN::PdoRoute* route) {
    if (overlaps(&output, sizeof output, &target, sizeof target) ||
        overlaps(&output, sizeof output, &options, sizeof options) ||
        (route && overlaps(&output, sizeof output, route, sizeof *route)))
        return invalid(ReadError::OVERLAP, "read preparation overlaps input");
    const Status checked = validateInputs(target, operation, now, deadline, options, kind, mask, route);
    if (!checked)
        return checked;
    // Nothing below can fail, and the overlap check above keeps every input readable.
    new (static_cast<void*>(&output)) ReadContext();
    output.target = target;
    output.operation = operation;
    output.kind = kind;
    output.state = ReadState::ACTIVE;
    output.startedUs = output.servicedUs = now;
    output.deadlineUs = deadline;
    output.options = options;
    output.blockMask = mask;
    visitPlan(kind, mask, PlanWriter{output});
    for (unsigned i = 0; i < output.count; ++i) {
        ReadDescriptor& object = output.values[i].object;
        if (const uint8_t width = measuredWidth(kind, object.index, object.subindex, options))
            object.widthBytes = width;
    }
    if (route) {
        output.source = ReadSource::TPDO;
        output.pdoRoute = *route;
    }
    return Ok();
}
void fail(ReadContext& c, ReadOutcome outcome, Status status, uint64_t now) {
    c.state = ReadState::FAILED;
    c.outcome = outcome;
    c.status = status;
    c.servicedUs = now;
}
Status current(const ReadContext& c, const ReadTarget& target, uint64_t now, uint64_t age) {
    if (!shape(c) || !validTarget(target) || !sameTarget(c.target, target))
        return invalid(ReadError::TARGET, "stale read target");
    if (c.invalidated)
        return invalid(ReadError::TARGET, "read lifecycle evidence was invalidated");
    if (!age || now < c.servicedUs)
        return invalid(ReadError::TIME, "invalid read publication time");
    return Ok();
}
Status fields(const ReadContext& c, unsigned start, unsigned length, uint64_t now, uint64_t age,
              ReadWindow& window) {
    bool first = true;
    for (unsigned i = start; i < start + length; ++i) {
        if (!required(c, i))
            continue;
        const ReadValue& value = c.values[i];
        const ReadProvenance& p = value.provenance;
        if (!value.valid || !sameBinding(p.binding, c.target.channel.binding) ||
            p.nodeGeneration != c.target.nodeGeneration || p.index != value.object.index ||
            p.subindex != value.object.subindex || p.source != c.source)
            return invalid(ReadError::STATE, "read field incomplete or wrong provenance");
        if (p.earliestUs > p.latestUs || p.latestUs > p.deliveredUs || p.deliveredUs > now ||
            now - p.earliestUs > age)
            return invalid(ReadError::TIME, "read field is future or expired");
        if (first || p.earliestUs < window.earliestUs)
            window.earliestUs = p.earliestUs;
        if (first || p.latestUs > window.latestUs)
            window.latestUs = p.latestUs;
        first = false;
    }
    window.skewUs = window.latestUs - window.earliestUs;
    return Ok();
}
} // namespace

namespace Detail {
uint8_t measuredWidth(ReadKind kind, uint16_t index, uint8_t subindex, const ReadOptions& o) noexcept {
    if (subindex)
        return 0;
    if (kind == ReadKind::IDENTITY && o.identityWidths == IdentityWidths::MEASURED_U8 &&
        (index == 0x1009 || index == 0x100a))
        return 1;
    if (o.countWidths == CountWidths::MEASURED_U16 &&
        ((kind == ReadKind::DRIVER_SOFT_LIMITS && index == 0x607d) ||
         (kind == ReadKind::IO_OUTPUT_COMMAND && index == 0x60fe)))
        return 2;
    return 0;
}
Status prepareRead(ReadContext& output, const ReadTarget& target, uint32_t operation, uint64_t now,
                   uint64_t deadline, const ReadOptions& options, ReadKind kind, uint8_t mask,
                   const CANOPEN::PdoRoute* route) noexcept {
    return prepare(output, target, operation, now, deadline, options, kind, mask, route);
}
Status beginGet(const ReadContext& c, const void* output, size_t outputSize,
                const ReadTarget& target, uint64_t now, uint64_t age, bool kindMatches,
                ReadWindow& window) noexcept {
    if (overlaps(output, outputSize, &c, sizeof c) ||
        overlaps(output, outputSize, &target, sizeof target))
        return invalid(ReadError::OVERLAP, "read output overlaps input");
    const Status status = current(c, target, now, age);
    if (!status)
        return status;
    if (!kindMatches || c.state != ReadState::SUCCEEDED || c.source != ReadSource::SDO)
        return invalid(ReadError::STATE, "read group incomplete or wrong context");
    return fields(c, 0, c.count, now, age, window);
}
} // namespace Detail

Status prepareProbe(ReadContext& c, const ReadTarget& t, uint32_t operation, uint64_t now,
                    uint64_t deadline, const ReadOptions& options) noexcept {
    return prepare(c, t, operation, now, deadline, options, ReadKind::PROBE, 0, nullptr);
}
Status prepareIdentity(ReadContext& c, const ReadTarget& t, uint32_t operation, uint64_t now,
                       uint64_t deadline, const ReadOptions& options) noexcept {
    return prepare(c, t, operation, now, deadline, options, ReadKind::IDENTITY, 0, nullptr);
}
Status prepareState(ReadContext& c, const ReadTarget& t, uint32_t operation, uint64_t now,
                    uint64_t deadline, const ReadOptions& options, uint8_t mask,
                    const CANOPEN::PdoRoute* route) noexcept {
    return prepare(c, t, operation, now, deadline, options, ReadKind::STATE, mask, route);
}
Status prepareConfig(ReadContext& c, const ReadTarget& t, uint32_t operation, uint64_t now,
                     uint64_t deadline, const ReadOptions& options) noexcept {
    return prepare(c, t, operation, now, deadline, options, ReadKind::CONFIG, 0, nullptr);
}
Status validateRead(ReadKind kind, const ReadTarget& target, uint32_t operation, uint64_t now,
                 uint64_t deadline, const ReadOptions& options, uint8_t mask) noexcept {
    return validateInputs(target, operation, now, deadline, options, kind, mask, nullptr);
}
void resetRead(ReadContext& c) noexcept {
    new (static_cast<void*>(&c)) ReadContext();
}
Status nextRead(const ReadContext& c, uint64_t now, PreparedRead& output) noexcept {
    if (overlaps(&c, sizeof c, &output, sizeof output))
        return invalid(ReadError::OVERLAP, "read work overlaps context");
    if (!shape(c))
        return invalid(ReadError::STATE, "invalid read context");
    if (now < c.servicedUs || (c.state == ReadState::ACTIVE && now >= c.deadlineUs))
        return invalid(ReadError::TIME, "read deadline or backward time");
    if (c.state != ReadState::ACTIVE || c.source == ReadSource::TPDO) {
        output.ready = false;
        return Ok();
    }
    if (c.cursor >= c.count)
        return invalid(ReadError::STATE, "invalid active read cursor");
    PreparedRead candidate;
    candidate.ready = true;
    candidate.transfer = work(c);
    output = candidate;
    return Ok();
}
Status advanceRead(ReadContext& c, const CANOPEN::SdoCompletion& input, uint64_t now) noexcept {
    if (overlaps(&c, sizeof c, &input, sizeof input))
        return invalid(ReadError::OVERLAP, "read terminal overlaps context");
    if (!shape(c) || c.state != ReadState::ACTIVE || c.source != ReadSource::SDO ||
        c.cursor >= c.count)
        return invalid(ReadError::STATE, "not an active service read");
    const CANOPEN::SdoCompletion& evidence = input; // The overlap check makes a copy unnecessary.
    const CANOPEN::SdoTransfer expected = work(c);
    if (!CANOPEN::sameSdoWork(evidence.transfer, expected) ||
        evidence.nodeGeneration != c.target.nodeGeneration)
        return invalid(ReadError::CORRELATION, "read terminal does not match request");
    if (evidence.admittedUs < c.servicedUs || evidence.admittedUs >= c.deadlineUs ||
        evidence.completedUs < evidence.admittedUs || evidence.completedUs > now ||
        now < c.servicedUs)
        return invalid(ReadError::TIME, "invalid read admission or completion time");
    const bool success = evidence.outcome == CANOPEN::SdoOutcome::SUCCESS;
    if (!CANOPEN::checkUploadTerminal(evidence, expected.size))
        return invalid(ReadError::CORRELATION, "inconsistent service terminal evidence");
    const bool expired = now >= c.deadlineUs || evidence.completedUs >= c.deadlineUs;
    uint32_t raw = 0;
    if (success && !expired) {
        // The one refusal left, taken before the first mutation.
        const Status decoded = CANOPEN::readWord(evidence.data, evidence.size, expected.size, raw);
        if (!decoded)
            return decoded;
    }
    const unsigned slot = c.cursor;
    c.evidence = evidence;
    c.evidencePresent = true;
    c.uncertain |= evidence.effectUncertain;
    // Each of these means earlier replies on this route can no longer be assumed fresh. The values
    // and this terminal stay retained as history; only current publication is withdrawn. Never
    // cleared here: a retired observation stays retired, whatever its active read later reports.
    c.invalidated |= evidence.outcome == CANOPEN::SdoOutcome::REBOOT ||
                     evidence.outcome == CANOPEN::SdoOutcome::BINDING_INVALIDATED ||
                     evidence.outcome == CANOPEN::SdoOutcome::BUS_OFF ||
                     evidence.outcome == CANOPEN::SdoOutcome::CORRELATION_LOST;
    c.servicedUs = now;
    if (expired)
        fail(c, ReadOutcome::DEADLINE, invalid(ReadError::TIME, "read overall deadline"), now);
    else if (!success) {
        const ReadOutcome terminal = readOutcome(evidence.outcome);
        fail(c, terminal,
             terminal == ReadOutcome::PROTOCOL_REJECTED
                 ? Status(Err::PROTOCOL_ERROR, 0, "native SDO rejection")
             : terminal == ReadOutcome::SERVICE_TIMEOUT
                 ? invalid(ReadError::TIME, "read queue or response budget expired")
                 : invalid(ReadError::STATE, "read service failed"),
             now);
    } else {
        ReadValue& value = c.values[slot];
        value.raw = raw;
        value.valid = true;
        value.provenance.binding = c.target.channel.binding;
        value.provenance.nodeGeneration = c.target.nodeGeneration;
        value.provenance.index = expected.index;
        value.provenance.subindex = expected.subindex;
        value.provenance.earliestUs = evidence.admittedUs;
        value.provenance.latestUs = evidence.completedUs;
        value.provenance.deliveredUs = now;
        if (expected.index == 0x1003 && expected.subindex == 0 && raw > 4)
            fail(c, ReadOutcome::REPLY_ERROR,
                 invalid(ReadError::VALUE, "error history exceeds four source entries"), now);
        else
            progress(c);
    }
    return Ok();
}
Status advanceRead(ReadContext& c, const CANOPEN::PdoRecord& record, uint64_t now) noexcept {
    if (overlaps(&c, sizeof c, &record, sizeof record))
        return invalid(ReadError::OVERLAP, "PDO read overlaps context");
    if (!shape(c) || c.state != ReadState::ACTIVE || c.source != ReadSource::TPDO ||
        c.kind != ReadKind::STATE)
        return invalid(ReadError::STATE, "not an active passive read");
    if (now < c.servicedUs || now >= c.deadlineUs || !qualified(c, record, now))
        return invalid(ReadError::TARGET, "PDO evidence is stale, unqualified or expired");
    CANOPEN::PdoSample decoded;
    if (!CANOPEN::decodePdo(record.observation.sample.frame, c.pdoRoute, decoded))
        return invalid(ReadError::VALUE, "invalid complete PDO frame");
    // First pass refuses; the second cannot fail.
    for (unsigned i = 0; i < c.count; ++i) {
        const ReadValue& value = c.values[i];
        bool found = false;
        for (unsigned j = 0; j < decoded.route.layout.mapping.count; ++j) {
            const CANOPEN::PdoValue& source = decoded.values[j];
            found |= source.object.index == value.object.index &&
                     source.object.subindex == value.object.subindex &&
                     source.object.widthBytes == value.object.widthBytes;
        }
        if (!found)
            return invalid(ReadError::VALUE, "missing selected PDO field");
    }
    for (unsigned i = 0; i < c.count; ++i) {
        ReadValue& value = c.values[i];
        for (unsigned j = 0; j < decoded.route.layout.mapping.count; ++j) {
            const CANOPEN::PdoValue& source = decoded.values[j];
            if (source.object.index != value.object.index ||
                source.object.subindex != value.object.subindex ||
                source.object.widthBytes != value.object.widthBytes)
                continue;
            value.raw = source.raw;
            value.valid = true;
            value.provenance.binding = c.target.channel.binding;
            value.provenance.nodeGeneration = c.target.nodeGeneration;
            value.provenance.mappingGeneration = c.pdoRoute.generation;
            value.provenance.source = ReadSource::TPDO;
            value.provenance.index = value.object.index;
            value.provenance.subindex = value.object.subindex;
            value.provenance.earliestUs = value.provenance.latestUs =
                record.observation.time.receivedUs;
            value.provenance.deliveredUs = now;
            value.provenance.captureKnown = true;
            value.provenance.evidenceId = record.qualification.evidenceId;
        }
    }
    c.servicedUs = now;
    progress(c);
    return Ok();
}
Status advanceRead(ReadContext& c, ReadOutcome reason, uint64_t now) noexcept {
    if (!shape(c) || c.state != ReadState::ACTIVE)
        return invalid(ReadError::STATE, "read is not active");
    if (now < c.servicedUs || (reason == ReadOutcome::DEADLINE && now < c.deadlineUs))
        return invalid(ReadError::TIME, "invalid local read termination time");
    if (reason != ReadOutcome::DEADLINE && reason != ReadOutcome::CANCELLED &&
        reason != ReadOutcome::INVALIDATED)
        return invalid(ReadError::ARGUMENT, "invalid local read termination");
    c.invalidated |= reason == ReadOutcome::INVALIDATED;
    if (now >= c.deadlineUs)
        reason = ReadOutcome::DEADLINE;
    c.uncertain |= c.source == ReadSource::SDO;
    fail(c, reason,
         invalid(reason == ReadOutcome::DEADLINE ? ReadError::TIME : ReadError::STATE,
                 "local read termination"),
         now);
    return Ok();
}

Status getIdentity(const ReadContext& c, const ReadTarget& target, uint64_t now, uint64_t age,
                   IdentityObservation& output) noexcept {
    IdentityObservation candidate;
    const Status status = beginGet(c, &output, sizeof output, target, now, age,
                                   c.kind == ReadKind::IDENTITY, candidate.window);
    if (!status)
        return status;
    candidate.target = c.target;
    candidate.deviceType = c.values[0].raw;
    candidate.hardwareVersion = uint16_t(c.values[1].raw);
    candidate.hardwareVersionWidth = c.values[1].object.widthBytes;
    candidate.softwareVersion = uint16_t(c.values[2].raw);
    candidate.softwareVersionWidth = c.values[2].object.widthBytes;
    candidate.identityEntries = uint8_t(c.values[3].raw);
    candidate.vendorId = c.values[4].raw;
    candidate.productCode = c.values[5].raw;
    candidate.revision = c.values[6].raw;
    candidate.modelCode = uint16_t(c.values[7].raw);
    candidate.firmwareCode = uint16_t(c.values[8].raw);
    copyProvenance(c, candidate.fields);
    output = candidate;
    return Ok();
}
Status getConfig(const ReadContext& c, const ReadTarget& target, uint64_t now, uint64_t age,
                 ConfigObservation& output) noexcept {
    ConfigObservation candidate;
    const Status status = beginGet(c, &output, sizeof output, target, now, age,
                                   c.kind == ReadKind::CONFIG, candidate.window);
    if (!status)
        return status;
    candidate.target = c.target;
    candidate.syncCobId = c.values[0].raw;
    candidate.emcyCobId = c.values[1].raw;
    candidate.producerTimeMs = uint16_t(c.values[2].raw);
    candidate.sdoEntries = uint8_t(c.values[3].raw);
    candidate.clientToServer = c.values[4].raw;
    candidate.serverToClient = c.values[5].raw;
    copyProvenance(c, candidate.fields);
    output = candidate;
    return Ok();
}
Status validateStateBlock(const ReadContext& c, StateBlock block, const ReadTarget& target,
                       uint64_t now, uint64_t age, ReadWindow& window,
                       const CANOPEN::PdoRecord* pdo) noexcept {
    if (overlaps(&window, sizeof window, &c, sizeof c) ||
        overlaps(&window, sizeof window, &target, sizeof target) ||
        (pdo && overlaps(&window, sizeof window, pdo, sizeof *pdo)))
        return invalid(ReadError::OVERLAP, "state window overlaps input");
    Status status = current(c, target, now, age);
    if (!status)
        return status;
    const unsigned b = static_cast<unsigned>(block);
    if (c.kind != ReadKind::STATE || b >= STATE_BLOCKS || !(c.blockMask & (1u << b)))
        return invalid(ReadError::STATE, "state block not selected");
    if ((c.source == ReadSource::TPDO && (!pdo || !qualified(c, *pdo, now))) ||
        (c.source == ReadSource::SDO && pdo))
        return invalid(ReadError::TARGET, "current PDO qualification unavailable");
    const unsigned start = c.blockStart[b], length = c.blockLength[b];
    ReadWindow bounds;
    status = fields(c, start, length, now, age, bounds);
    if (!status)
        return status;
    for (unsigned i = start; i < start + length; ++i) {
        if (!required(c, i))
            continue;
        if (c.source == ReadSource::TPDO &&
            (c.values[i].provenance.mappingGeneration != c.pdoRoute.generation ||
             c.values[i].provenance.evidenceId != pdo->qualification.evidenceId ||
             c.values[i].provenance.earliestUs <= pdo->qualification.establishedUs ||
             c.values[i].provenance.earliestUs <= pdo->boundaryUs))
            return invalid(ReadError::TARGET, "withdrawn PDO sample provenance");
    }
    if (block == StateBlock::ERROR_HISTORY && c.values[start].raw > 4)
        return invalid(ReadError::VALUE, "error history count out of range");
    window = bounds;
    return Ok();
}
Status getStateBlock(const ReadContext& c, StateBlock block, const ReadTarget& target, uint64_t now,
                     uint64_t age, StateObservation& output,
                     const CANOPEN::PdoRecord* pdo) noexcept {
    if (overlaps(&output, sizeof output, &c, sizeof c) ||
        overlaps(&output, sizeof output, &target, sizeof target) ||
        (pdo && overlaps(&output, sizeof output, pdo, sizeof *pdo)))
        return invalid(ReadError::OVERLAP, "state output overlaps input");
    ReadWindow window;
    const Status status = validateStateBlock(c, block, target, now, age, window, pdo);
    if (!status)
        return status;
    new (static_cast<void*>(&output)) StateObservation();
    output.target = c.target;
    output.block = block;
    output.window = window;
    const unsigned b = static_cast<unsigned>(block);
    const unsigned start = c.blockStart[b], length = c.blockLength[b];
    for (unsigned i = start; i < start + length; ++i)
        if (required(c, i))
            output.fields[output.fieldCount++] = c.values[i].provenance;
    const ReadValue* v = c.values + start;
    switch (block) {
    case StateBlock::ERRORS:
        output.errorRegister = uint8_t(v[0].raw);
        output.errorCode = uint16_t(v[1].raw);
        break;
    case StateBlock::ERROR_HISTORY:
        output.errorCount = uint8_t(v[0].raw);
        for (unsigned i = 0; i < output.errorCount; ++i)
            output.errorHistory[i] = v[i + 1].raw;
        break;
    case StateBlock::MONITORS:
        output.monitor = uint16_t(v[0].raw);
        output.logicalInputs = uint16_t(v[1].raw);
        output.logicalOutputs = uint16_t(v[2].raw);
        break;
    case StateBlock::STATUS:
        output.statusword = uint16_t(v[0].raw);
        output.modeRaw = uint8_t(v[1].raw);
        break;
    case StateBlock::FEEDBACK:
        output.actualPosition = signed32(v[0].raw);
        output.actualVelocity = signed32(v[1].raw);
        break;
    case StateBlock::INPUTS:
        output.digitalInputs = v[0].raw;
        break;
    }
    return Ok();
}
} // namespace LICHUAN_C
} // namespace MotorControlCAN
