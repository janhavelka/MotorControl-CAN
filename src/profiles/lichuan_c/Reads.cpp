// SPDX-License-Identifier: MIT
#include <MotorControlCAN/profiles/lichuan_c/DriverSettings.h>
#include <MotorControlCAN/profiles/lichuan_c/IoSettings.h>
#include <MotorControlCAN/profiles/lichuan_c/Pdo.h>
#include <MotorControlCAN/profiles/lichuan_c/Reads.h>

namespace MotorControlCAN {
namespace LICHUAN_C {
namespace {
Status invalid(ReadError detail, const char* message) {
    return Status(Err::INVALID_CONFIG, static_cast<int32_t>(detail), message);
}
bool overlap(const void* a, size_t an, const void* b, size_t bn) {
    const uintptr_t x = reinterpret_cast<uintptr_t>(a), y = reinterpret_cast<uintptr_t>(b);
    return x >= y ? x - y < bn : y - x < an;
}
bool sameChannel(const CANOPEN::SdoChannel& a, const CANOPEN::SdoChannel& b) {
    return sameBinding(a.binding, b.binding) && a.clientToServerId == b.clientToServerId &&
           a.serverToClientId == b.serverToClientId;
}
bool sameTarget(const ReadTarget& a, const ReadTarget& b) {
    return a.nodeGeneration == b.nodeGeneration && sameChannel(a.channel, b.channel);
}
bool validTarget(const ReadTarget& target) {
    return target.nodeGeneration && CANOPEN::validateSdoChannel(target.channel).isOk();
}
bool sameDescriptor(const ReadDescriptor& a, const ReadDescriptor& b) {
    return a.index == b.index && a.subindex == b.subindex && a.widthBytes == b.widthBytes &&
           a.representation == b.representation && a.readEffect == b.readEffect &&
           a.scope == b.scope;
}
struct Address {
    uint16_t index;
    uint8_t subindex;
};
const Address identity[] = {{0x1000, 0}, {0x1009, 0}, {0x100a, 0}, {0x1018, 0}, {0x1018, 1},
                            {0x1018, 2}, {0x1018, 3}, {0x2000, 0}, {0x2001, 0}};
const Address configuration[] = {{0x1005, 0}, {0x1014, 0}, {0x1017, 0},
                                 {0x1200, 0}, {0x1200, 1}, {0x1200, 2}};
const Address state[] = {{0x1001, 0}, {0x603f, 0}, {0x1003, 0}, {0x1003, 1}, {0x1003, 2},
                         {0x1003, 3}, {0x1003, 4}, {0x2100, 0}, {0x2101, 0}, {0x2102, 0},
                         {0x6041, 0}, {0x6061, 0}, {0x6064, 0}, {0x606c, 0}, {0x60fd, 0}};
const uint8_t starts[STATE_BLOCKS] = {0, 2, 7, 10, 12, 14};
const uint8_t lengths[STATE_BLOCKS] = {2, 5, 3, 2, 2, 1};
const Address driver[] = {{0x2200, 0}, {0x2201, 0}};
const Address softLimits[] = {{0x2202, 0}, {0x607d, 0}, {0x607d, 1}, {0x607d, 2}};
const Address motionParameters[] = {{0x6081, 0}, {0x6083, 0}, {0x6084, 0}};
const Address ioInputs[] = {{0x2400, 0}, {0x2400, 1}, {0x2400, 2}, {0x2400, 3}, {0x2400, 4},
                            {0x2400, 5}, {0x2400, 6}, {0x2400, 7}, {0x2400, 8}};
const Address ioOutputs[] = {{0x2400, 0x0c}, {0x2400, 0x0d}, {0x2400, 0x0e}, {0x2400, 0x0f}};
const Address ioFilters[] = {{0x2401, 0}, {0x2402, 0}, {0x2403, 0}, {0x2404, 0},
                             {0x2405, 0}, {0x2406, 0}, {0x2407, 0}};
const Address ioCommand[] = {{0x60fe, 0}, {0x60fe, 1}, {0x60fe, 2}};

const Address* driverPlan(ReadKind kind, unsigned& count) {
    switch (kind) {
    case ReadKind::DRIVER:
        count = 2;
        return driver;
    case ReadKind::DRIVER_SOFT_LIMITS:
        count = 4;
        return softLimits;
    case ReadKind::DRIVER_MOTION_PARAMETERS:
        count = 3;
        return motionParameters;
    case ReadKind::IO_INPUT_CONFIGURATION:
        count = 9;
        return ioInputs;
    case ReadKind::IO_OUTPUT_CONFIGURATION:
        count = 4;
        return ioOutputs;
    case ReadKind::IO_INPUT_FILTERS:
        count = 7;
        return ioFilters;
    case ReadKind::IO_OUTPUT_COMMAND:
        count = 3;
        return ioCommand;
    default:
        count = 0;
        return nullptr;
    }
}
bool driverKind(ReadKind kind) {
    unsigned count;
    return driverPlan(kind, count) != nullptr;
}

bool add(ReadContext& c, Address address) {
    return c.count < READ_FIELDS &&
           readDescriptor(address.index, address.subindex, c.values[c.count++].object);
}
bool plan(ReadContext& c) {
    unsigned driverCount;
    const Address* driverFields = driverPlan(c.kind, driverCount);
    if (driverFields) {
        for (unsigned i = 0; i < driverCount; ++i)
            if (!add(c, driverFields[i]))
                return false;
    } else if (c.kind == ReadKind::IDENTITY) {
        for (const Address& address : identity)
            if (!add(c, address))
                return false;
    } else if (c.kind == ReadKind::CONFIG) {
        for (const Address& address : configuration)
            if (!add(c, address))
                return false;
    } else if (c.kind == ReadKind::PROBE) {
        if (!add(c, identity[0]))
            return false;
    } else if (c.kind == ReadKind::STATE && c.blockMask && !(c.blockMask & 0xc0u)) {
        for (unsigned b = 0; b < STATE_BLOCKS; ++b) {
            if (!(c.blockMask & (1u << b)))
                continue;
            c.blockStart[b] = c.count;
            c.blockLength[b] = lengths[b];
            for (unsigned i = 0; i < lengths[b]; ++i)
                if (!add(c, state[starts[b] + i]))
                    return false;
        }
    } else
        return false;
    return true;
}
bool shape(const ReadContext& c) {
    if (!validTarget(c.target) || !c.operation || !c.count || c.count > READ_FIELDS ||
        c.cursor > c.count || c.state == ReadState::EMPTY || c.state > ReadState::FAILED ||
        c.source > ReadSource::TPDO || c.deadlineUs <= c.startedUs || c.servicedUs < c.startedUs ||
        !c.options.queueTimeoutUs || !c.options.responseTimeoutUs ||
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
        } else if (c.kind == ReadKind::IDENTITY) {
            addresses = identity;
            count = 9;
        } else if (c.kind == ReadKind::CONFIG) {
            addresses = configuration;
            count = 6;
        } else if (c.kind == ReadKind::PROBE) {
            addresses = identity;
            count = 1;
        } else {
            addresses = driverPlan(c.kind, count);
            if (!addresses)
                return false;
        }
        for (unsigned i = 0; i < count; ++i) {
            if (cursor >= c.count)
                return false;
            ReadDescriptor expected;
            if (!readDescriptor(addresses[i].index, addresses[i].subindex, expected) ||
                !sameDescriptor(expected, c.values[cursor++].object))
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
    out.readEffect = object.readEffect == ReadEffect::UNQUALIFIED
                         ? CANOPEN::SdoReadEffect::UNQUALIFIED
                         : CANOPEN::SdoReadEffect::REVIEWED_NON_CHANGING;
    out.deadlineUs = c.deadlineUs;
    out.queueTimeoutUs = c.options.queueTimeoutUs;
    out.responseTimeoutUs = c.options.responseTimeoutUs;
    return out;
}
bool sameWork(const CANOPEN::SdoTransfer& a, const CANOPEN::SdoTransfer& b) {
    return sameChannel(a.channel, b.channel) && a.operation == b.operation &&
           a.request == b.request && a.step == b.step && a.index == b.index &&
           a.subindex == b.subindex && a.direction == b.direction && a.size == b.size &&
           a.readEffect == b.readEffect && a.allowSegmentedUpload == b.allowSegmentedUpload &&
           a.deadlineUs == b.deadlineUs && a.queueTimeoutUs == b.queueTimeoutUs &&
           a.responseTimeoutUs == b.responseTimeoutUs;
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
Status prepare(ReadContext& output, const ReadTarget& target, uint32_t operation, uint64_t now,
               uint64_t deadline, const ReadOptions& options, ReadKind kind, uint8_t mask,
               const CANOPEN::PdoRoute* route) {
    if (overlap(&output, sizeof output, &target, sizeof target) ||
        overlap(&output, sizeof output, &options, sizeof options) ||
        (route && overlap(&output, sizeof output, route, sizeof *route)))
        return invalid(ReadError::OVERLAP, "read preparation overlaps input");
    if (!validTarget(target))
        return invalid(ReadError::TARGET, "invalid read target or node epoch");
    if (!operation || deadline <= now || !options.queueTimeoutUs || !options.responseTimeoutUs)
        return invalid(ReadError::ARGUMENT, "invalid read identity or budgets");
    ReadContext candidate;
    candidate.target = target;
    candidate.operation = operation;
    candidate.kind = kind;
    candidate.state = ReadState::ACTIVE;
    candidate.startedUs = candidate.servicedUs = now;
    candidate.deadlineUs = deadline;
    candidate.options = options;
    candidate.blockMask = mask;
    if (!plan(candidate))
        return invalid(ReadError::ARGUMENT, "invalid read selection");
    if (kind == ReadKind::PROBE && candidate.values[0].object.readEffect == ReadEffect::UNQUALIFIED)
        return Status(Err::UNSUPPORTED, static_cast<int32_t>(ReadError::READ_EFFECT),
                      "Q20: non-changing probe is not established");
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
        for (unsigned i = 0; i < candidate.count; ++i) {
            const ReadDescriptor& object = candidate.values[i].object;
            bool found = false;
            for (unsigned j = 0; j < checked.mapping.count; ++j)
                if (checked.fields[j].index == object.index &&
                    checked.fields[j].subindex == object.subindex &&
                    checked.fields[j].widthBytes == object.widthBytes)
                    found = true;
            if (!found)
                return invalid(ReadError::ARGUMENT, "selected state fields are absent from TPDO");
        }
        candidate.source = ReadSource::TPDO;
        candidate.pdoRoute = *route;
    }
    output = candidate;
    return Ok();
}
void fail(ReadContext& c, ReadOutcome outcome, Status status, uint64_t now) {
    c.state = ReadState::FAILED;
    c.outcome = outcome;
    c.status = status;
    c.servicedUs = now;
}
ReadOutcome outcome(CANOPEN::SdoOutcome value) {
    switch (value) {
    case CANOPEN::SdoOutcome::SUCCESS:
        return ReadOutcome::SUCCESS;
    case CANOPEN::SdoOutcome::SERVER_ABORT:
        return ReadOutcome::PROTOCOL_REJECTED;
    case CANOPEN::SdoOutcome::CLIENT_ABORT:
    case CANOPEN::SdoOutcome::UNSUPPORTED:
        return ReadOutcome::REPLY_ERROR;
    case CANOPEN::SdoOutcome::TIMEOUT:
        return ReadOutcome::DEADLINE;
    case CANOPEN::SdoOutcome::CANCELLED:
        return ReadOutcome::CANCELLED;
    case CANOPEN::SdoOutcome::REBOOT:
    case CANOPEN::SdoOutcome::BINDING_INVALIDATED:
        return ReadOutcome::INVALIDATED;
    default:
        return ReadOutcome::TRANSPORT_ERROR;
    }
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
int32_t signed32(uint32_t raw) {
    return static_cast<int32_t>(raw <= INT32_MAX ? int64_t(raw)
                                                 : int64_t(raw) - INT64_C(4294967296));
}
} // namespace

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
Status nextRead(const ReadContext& c, uint64_t now, PreparedRead& output) noexcept {
    if (overlap(&c, sizeof c, &output, sizeof output))
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
    if (overlap(&c, sizeof c, &input, sizeof input))
        return invalid(ReadError::OVERLAP, "read terminal overlaps context");
    if (!shape(c) || c.state != ReadState::ACTIVE || c.source != ReadSource::SDO ||
        c.cursor >= c.count)
        return invalid(ReadError::STATE, "not an active service read");
    const CANOPEN::SdoCompletion evidence = input;
    const CANOPEN::SdoTransfer expected = work(c);
    if (!sameWork(evidence.transfer, expected) ||
        evidence.nodeGeneration != c.target.nodeGeneration)
        return invalid(ReadError::CORRELATION, "read terminal does not match request");
    if (evidence.admittedUs < c.servicedUs || evidence.admittedUs >= c.deadlineUs ||
        evidence.completedUs < evidence.admittedUs || evidence.completedUs > now ||
        now < c.servicedUs)
        return invalid(ReadError::TIME, "invalid read admission or completion time");
    const bool success = evidence.outcome == CANOPEN::SdoOutcome::SUCCESS;
    const bool possible =
        evidence.transmissionPossible && expected.readEffect == CANOPEN::SdoReadEffect::UNQUALIFIED;
    if (evidence.outcome > CANOPEN::SdoOutcome::UNSUPPORTED ||
        (evidence.transmissionPossible && !evidence.queued) ||
        (evidence.transmissionConfirmed && !evidence.transmissionPossible) ||
        evidence.effectPossible != possible || evidence.effectUncertain != (possible && !success) ||
        (success &&
         (!evidence.protocolAcknowledged || !evidence.queued || !evidence.transmissionPossible ||
          evidence.abortValid || evidence.size != expected.size)) ||
        (!success && (evidence.protocolAcknowledged || evidence.size)) ||
        (evidence.outcome == CANOPEN::SdoOutcome::SERVER_ABORT &&
         (!evidence.abortValid || !evidence.transmissionPossible)))
        return invalid(ReadError::CORRELATION, "inconsistent service terminal evidence");
    ReadContext candidate = c;
    candidate.evidence = evidence;
    candidate.evidencePresent = true;
    candidate.uncertain |= evidence.effectUncertain;
    candidate.invalidated = evidence.outcome == CANOPEN::SdoOutcome::REBOOT ||
                            evidence.outcome == CANOPEN::SdoOutcome::BINDING_INVALIDATED ||
                            evidence.outcome == CANOPEN::SdoOutcome::BUS_OFF;
    candidate.servicedUs = now;
    if (now >= c.deadlineUs || evidence.completedUs >= c.deadlineUs)
        fail(candidate, ReadOutcome::DEADLINE, invalid(ReadError::TIME, "read overall deadline"),
             now);
    else if (!success) {
        const ReadOutcome terminal = outcome(evidence.outcome);
        fail(candidate, terminal,
             terminal == ReadOutcome::PROTOCOL_REJECTED
                 ? Status(Err::PROTOCOL_ERROR, 0, "native SDO rejection")
                 : invalid(ReadError::STATE, "read service failed"),
             now);
    } else {
        ReadValue& value = candidate.values[c.cursor];
        uint32_t raw = 0;
        const Status decoded = CANOPEN::readWord(evidence.data, evidence.size, expected.size, raw);
        if (!decoded)
            return decoded;
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
            fail(candidate, ReadOutcome::REPLY_ERROR,
                 invalid(ReadError::VALUE, "error history exceeds four source entries"), now);
        else
            progress(candidate);
    }
    c = candidate;
    return Ok();
}
Status advanceRead(ReadContext& c, const CANOPEN::PdoRecord& record, uint64_t now) noexcept {
    if (overlap(&c, sizeof c, &record, sizeof record))
        return invalid(ReadError::OVERLAP, "PDO read overlaps context");
    if (!shape(c) || c.state != ReadState::ACTIVE || c.source != ReadSource::TPDO ||
        c.kind != ReadKind::STATE)
        return invalid(ReadError::STATE, "not an active passive read");
    if (now < c.servicedUs || now >= c.deadlineUs || !qualified(c, record, now))
        return invalid(ReadError::TARGET, "PDO evidence is stale, unqualified or expired");
    CANOPEN::PdoSample decoded;
    if (!CANOPEN::decodePdo(record.observation.sample.frame, c.pdoRoute, decoded))
        return invalid(ReadError::VALUE, "invalid complete PDO frame");
    ReadContext candidate = c;
    for (unsigned i = 0; i < c.count; ++i) {
        ReadValue& value = candidate.values[i];
        bool found = false;
        for (unsigned j = 0; j < decoded.route.layout.mapping.count; ++j) {
            const CANOPEN::PdoValue& source = decoded.values[j];
            if (source.object.index != value.object.index ||
                source.object.subindex != value.object.subindex ||
                source.object.widthBytes != value.object.widthBytes)
                continue;
            value.raw = source.raw;
            value.valid = true;
            found = true;
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
        if (!found)
            return invalid(ReadError::VALUE, "missing selected PDO field");
    }
    candidate.servicedUs = now;
    progress(candidate);
    c = candidate;
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
    c.invalidated = reason == ReadOutcome::INVALIDATED;
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
    if (overlap(&output, sizeof output, &c, sizeof c) ||
        overlap(&output, sizeof output, &target, sizeof target))
        return invalid(ReadError::OVERLAP, "identity output overlaps input");
    Status status = current(c, target, now, age);
    if (!status)
        return status;
    if (c.kind != ReadKind::IDENTITY || c.state != ReadState::SUCCEEDED)
        return invalid(ReadError::STATE, "identity incomplete or wrong context");
    IdentityObservation candidate;
    candidate.target = c.target;
    status = fields(c, 0, 9, now, age, candidate.window);
    if (!status)
        return status;
    candidate.deviceType = c.values[0].raw;
    candidate.hardwareVersion = uint16_t(c.values[1].raw);
    candidate.softwareVersion = uint16_t(c.values[2].raw);
    candidate.identityEntries = uint8_t(c.values[3].raw);
    candidate.vendorId = c.values[4].raw;
    candidate.productCode = c.values[5].raw;
    candidate.revision = c.values[6].raw;
    candidate.modelCode = uint16_t(c.values[7].raw);
    candidate.firmwareCode = uint16_t(c.values[8].raw);
    for (unsigned i = 0; i < 9; ++i)
        candidate.fields[i] = c.values[i].provenance;
    output = candidate;
    return Ok();
}
Status getConfig(const ReadContext& c, const ReadTarget& target, uint64_t now, uint64_t age,
                 ConfigObservation& output) noexcept {
    if (overlap(&output, sizeof output, &c, sizeof c) ||
        overlap(&output, sizeof output, &target, sizeof target))
        return invalid(ReadError::OVERLAP, "configuration output overlaps input");
    Status status = current(c, target, now, age);
    if (!status)
        return status;
    if (c.kind != ReadKind::CONFIG || c.state != ReadState::SUCCEEDED)
        return invalid(ReadError::STATE, "configuration incomplete or wrong context");
    ConfigObservation candidate;
    candidate.target = c.target;
    status = fields(c, 0, 6, now, age, candidate.window);
    if (!status)
        return status;
    candidate.syncCobId = c.values[0].raw;
    candidate.emcyCobId = c.values[1].raw;
    candidate.producerTimeMs = uint16_t(c.values[2].raw);
    candidate.sdoEntries = uint8_t(c.values[3].raw);
    candidate.clientToServer = c.values[4].raw;
    candidate.serverToClient = c.values[5].raw;
    for (unsigned i = 0; i < 6; ++i)
        candidate.fields[i] = c.values[i].provenance;
    output = candidate;
    return Ok();
}
Status getStateBlock(const ReadContext& c, StateBlock block, const ReadTarget& target, uint64_t now,
                     uint64_t age, StateObservation& output,
                     const CANOPEN::PdoRecord* pdo) noexcept {
    if (overlap(&output, sizeof output, &c, sizeof c) ||
        overlap(&output, sizeof output, &target, sizeof target) ||
        (pdo && overlap(&output, sizeof output, pdo, sizeof *pdo)))
        return invalid(ReadError::OVERLAP, "state output overlaps input");
    Status status = current(c, target, now, age);
    if (!status)
        return status;
    const unsigned b = static_cast<unsigned>(block);
    if (c.kind != ReadKind::STATE || b >= STATE_BLOCKS || !(c.blockMask & (1u << b)))
        return invalid(ReadError::STATE, "state block not selected");
    if ((c.source == ReadSource::TPDO && (!pdo || !qualified(c, *pdo, now))) ||
        (c.source == ReadSource::SDO && pdo))
        return invalid(ReadError::TARGET, "current PDO qualification unavailable");
    StateObservation candidate;
    candidate.target = c.target;
    candidate.block = block;
    const unsigned start = c.blockStart[b], length = c.blockLength[b];
    status = fields(c, start, length, now, age, candidate.window);
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
        candidate.fields[candidate.fieldCount++] = c.values[i].provenance;
    }
    const ReadValue* v = c.values + start;
    switch (block) {
    case StateBlock::ERRORS:
        candidate.errorRegister = uint8_t(v[0].raw);
        candidate.errorCode = uint16_t(v[1].raw);
        break;
    case StateBlock::ERROR_HISTORY:
        if (v[0].raw > 4)
            return invalid(ReadError::VALUE, "error history count out of range");
        candidate.errorCount = uint8_t(v[0].raw);
        for (unsigned i = 0; i < candidate.errorCount; ++i)
            candidate.errorHistory[i] = v[i + 1].raw;
        break;
    case StateBlock::MONITORS:
        candidate.monitor = uint16_t(v[0].raw);
        candidate.logicalInputs = uint16_t(v[1].raw);
        candidate.logicalOutputs = uint16_t(v[2].raw);
        break;
    case StateBlock::STATUS:
        candidate.statusword = uint16_t(v[0].raw);
        candidate.modeRaw = uint8_t(v[1].raw);
        break;
    case StateBlock::FEEDBACK:
        candidate.actualPosition = signed32(v[0].raw);
        candidate.actualVelocity = signed32(v[1].raw);
        break;
    case StateBlock::INPUTS:
        candidate.digitalInputs = v[0].raw;
        break;
    }
    output = candidate;
    return Ok();
}
Status prepareDriverRead(DriverContext& c, const ReadTarget& target, uint32_t operation,
                         uint64_t now, uint64_t deadline, const ReadOptions& options,
                         DriverGroup group) noexcept {
    ReadKind kind;
    switch (group) {
    case DriverGroup::DRIVER:
        kind = ReadKind::DRIVER;
        break;
    case DriverGroup::SOFT_LIMITS:
        kind = ReadKind::DRIVER_SOFT_LIMITS;
        break;
    case DriverGroup::MOTION_PARAMETERS:
        kind = ReadKind::DRIVER_MOTION_PARAMETERS;
        break;
    case DriverGroup::IO_INPUT_CONFIGURATION:
        kind = ReadKind::IO_INPUT_CONFIGURATION;
        break;
    case DriverGroup::IO_OUTPUT_CONFIGURATION:
        kind = ReadKind::IO_OUTPUT_CONFIGURATION;
        break;
    case DriverGroup::IO_INPUT_FILTERS:
        kind = ReadKind::IO_INPUT_FILTERS;
        break;
    case DriverGroup::IO_OUTPUT_COMMAND:
        kind = ReadKind::IO_OUTPUT_COMMAND;
        break;
    default:
        return invalid(ReadError::ARGUMENT, "unknown native driver group");
    }
    return prepare(c.read, target, operation, now, deadline, options, kind, 0, nullptr);
}
Status nextDriver(const DriverContext& c, uint64_t now, PreparedRead& output) noexcept {
    if (!driverKind(c.read.kind))
        return invalid(ReadError::STATE, "not a driver context");
    return nextRead(c.read, now, output);
}
Status advanceDriver(DriverContext& c, const CANOPEN::SdoCompletion& input, uint64_t now) noexcept {
    if (!driverKind(c.read.kind))
        return invalid(ReadError::STATE, "not a driver context");
    return advanceRead(c.read, input, now);
}
Status advanceDriver(DriverContext& c, ReadOutcome reason, uint64_t now) noexcept {
    if (!driverKind(c.read.kind))
        return invalid(ReadError::STATE, "not a driver context");
    return advanceRead(c.read, reason, now);
}
Status getDriver(const DriverContext& c, const ReadTarget& target, uint64_t now, uint64_t age,
                 DriverObservation& output) noexcept {
    if (overlap(&output, sizeof output, &c, sizeof c) ||
        overlap(&output, sizeof output, &target, sizeof target))
        return invalid(ReadError::OVERLAP, "driver output overlaps input");
    Status status = current(c.read, target, now, age);
    if (!status)
        return status;
    if ((c.read.kind != ReadKind::DRIVER && c.read.kind != ReadKind::DRIVER_SOFT_LIMITS &&
         c.read.kind != ReadKind::DRIVER_MOTION_PARAMETERS) ||
        c.read.state != ReadState::SUCCEEDED || c.read.source != ReadSource::SDO)
        return invalid(ReadError::STATE, "driver group incomplete or wrong context");
    DriverObservation candidate;
    candidate.target = c.read.target;
    status = fields(c.read, 0, c.read.count, now, age, candidate.window);
    if (!status)
        return status;
    candidate.fieldCount = c.read.count;
    for (unsigned i = 0; i < c.read.count; ++i)
        candidate.fields[i] = c.read.values[i].provenance;
    const ReadValue* v = c.read.values;
    switch (c.read.kind) {
    case ReadKind::DRIVER:
        candidate.directionRaw = uint16_t(v[0].raw);
        candidate.subdivisionRaw = uint16_t(v[1].raw);
        break;
    case ReadKind::DRIVER_SOFT_LIMITS:
        candidate.group = DriverGroup::SOFT_LIMITS;
        candidate.softLimitSelectorRaw = uint16_t(v[0].raw);
        candidate.softLimitEntries = uint8_t(v[1].raw);
        candidate.minimumNative = signed32(v[2].raw);
        candidate.maximumNative = signed32(v[3].raw);
        break;
    case ReadKind::DRIVER_MOTION_PARAMETERS:
        candidate.group = DriverGroup::MOTION_PARAMETERS;
        candidate.positionSpeedRpm = v[0].raw;
        candidate.rampUpTimeMs = v[1].raw;
        candidate.rampDownTimeMs = v[2].raw;
        break;
    default:
        return invalid(ReadError::STATE, "unknown driver observation");
    }
    output = candidate;
    return Ok();
}
Status getIo(const DriverContext& c, const ReadTarget& target, uint64_t now, uint64_t age,
             IoObservation& output) noexcept {
    if (overlap(&output, sizeof output, &c, sizeof c) ||
        overlap(&output, sizeof output, &target, sizeof target))
        return invalid(ReadError::OVERLAP, "I/O output overlaps input");
    Status status = current(c.read, target, now, age);
    if (!status)
        return status;
    IoObservation candidate;
    switch (c.read.kind) {
    case ReadKind::IO_INPUT_CONFIGURATION:
        candidate.group = DriverGroup::IO_INPUT_CONFIGURATION;
        break;
    case ReadKind::IO_OUTPUT_CONFIGURATION:
        candidate.group = DriverGroup::IO_OUTPUT_CONFIGURATION;
        break;
    case ReadKind::IO_INPUT_FILTERS:
        candidate.group = DriverGroup::IO_INPUT_FILTERS;
        break;
    case ReadKind::IO_OUTPUT_COMMAND:
        candidate.group = DriverGroup::IO_OUTPUT_COMMAND;
        break;
    default:
        return invalid(ReadError::STATE, "not an I/O context");
    }
    if (c.read.state != ReadState::SUCCEEDED || c.read.source != ReadSource::SDO)
        return invalid(ReadError::STATE, "I/O group incomplete or wrong source");
    status = fields(c.read, 0, c.read.count, now, age, candidate.window);
    if (!status)
        return status;
    candidate.target = c.read.target;
    candidate.fieldCount = c.read.count;
    for (unsigned i = 0; i < c.read.count; ++i)
        candidate.fields[i] = c.read.values[i].provenance;
    const ReadValue* v = c.read.values;
    switch (candidate.group) {
    case DriverGroup::IO_INPUT_CONFIGURATION:
        candidate.ioEntries = uint16_t(v[0].raw);
        candidate.inputPolarityRaw = uint16_t(v[1].raw);
        for (unsigned i = 0; i < 7; ++i) {
            candidate.inputInverted[i] = (candidate.inputPolarityRaw & (1u << i)) != 0;
            candidate.inputFunctionRaw[i] = uint16_t(v[i + 2].raw);
            candidate.inputFunctions[i] =
                candidate.inputFunctionRaw[i] <= 13
                    ? static_cast<InputFunction>(candidate.inputFunctionRaw[i])
                    : InputFunction::UNKNOWN;
        }
        break;
    case DriverGroup::IO_OUTPUT_CONFIGURATION:
        candidate.outputConfig0CRaw = uint16_t(v[0].raw);
        candidate.outputConfig0DRaw = uint16_t(v[1].raw);
        candidate.outputConfig0ERaw = uint16_t(v[2].raw);
        candidate.outputConfig0FRaw = uint16_t(v[3].raw);
        break;
    case DriverGroup::IO_INPUT_FILTERS:
        for (unsigned i = 0; i < 7; ++i)
            candidate.inputFilterRaw[i] = uint16_t(v[i].raw);
        break;
    case DriverGroup::IO_OUTPUT_COMMAND:
        candidate.outputEntries = uint8_t(v[0].raw);
        candidate.outputValueRaw = v[1].raw;
        candidate.outputMaskRaw = v[2].raw;
        break;
    default:
        return invalid(ReadError::STATE, "unknown I/O observation");
    }
    output = candidate;
    return Ok();
}
} // namespace LICHUAN_C
} // namespace MotorControlCAN
