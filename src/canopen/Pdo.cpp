// SPDX-License-Identifier: MIT
#include <MotorControlCAN/canopen/PdoState.h>

namespace MotorControlCAN {
namespace CANOPEN {
namespace {
Status invalid(const char* message) { return Status(Err::INVALID_CONFIG, 0, message); }
Status illegal(const char* message) { return Status(Err::ILLEGAL_VALUE, 0, message); }
bool span(const void* value, size_t length) {
    return length <= UINTPTR_MAX - reinterpret_cast<uintptr_t>(value);
}
bool overlap(const void* a, size_t aSize, const void* b, size_t bSize) {
    const uintptr_t first = reinterpret_cast<uintptr_t>(a);
    const uintptr_t second = reinterpret_cast<uintptr_t>(b);
    return first <= second ? second - first < aSize : first - second < bSize;
}
bool directionValid(PdoDirection direction) { return direction <= PdoDirection::TPDO; }
bool representationValid(const PdoObject& object) {
    return object.widthBytes && object.widthBytes <= 4 && object.sign <= PdoSign::RAW &&
           object.role <= PdoRole::ESTIMATED;
}
bool eligible(const PdoObject& object, PdoDirection direction) {
    return direction == PdoDirection::RPDO ? object.rpdo : object.tpdo;
}
bool sameAddress(const PdoObject& a, const PdoObject& b) {
    return a.index == b.index && a.subindex == b.subindex;
}
bool matches(uint32_t word, const PdoObject& object) {
    return (word >> 16) == object.index && uint8_t(word >> 8) == object.subindex &&
           uint8_t(word) == uint8_t(object.widthBytes * 8);
}
uint32_t scalar(const uint8_t* data, uint8_t count) {
    uint32_t value = 0;
    for (uint8_t i = 0; i < count; ++i)
        value |= uint32_t(data[i]) << (8u * i);
    return value;
}
bool communication(uint32_t parameter, const PdoRoute& route) {
    if (parameter & 0x80000000u)
        return false;
    if (((parameter & 0x20000000u) != 0) != route.extended)
        return false;
    // Bit 30 describes RTR policy; these codecs accept data frames only.
    if (!route.extended && (parameter & 0x1ffff800u))
        return false;
    return (parameter & (route.extended ? 0x1fffffffu : 0x7ffu)) == route.cobId;
}
} // namespace

Status validatePdoMap(const PdoMap& map, const PdoProfile& profile,
                      PdoLayout& output) noexcept {
    if (overlap(&map, sizeof(map), &output, sizeof(output)) ||
        overlap(&profile, sizeof(profile), &output, sizeof(output)))
        return illegal("PDO layout output overlaps input");
    if (!profile.profile || !profile.sets || profile.sets > 512 || !profile.slots ||
        profile.slots > PDO_ENTRIES || !profile.objects || !profile.objectCount || profile.objectCount > 256)
        return invalid("invalid bounded PDO profile");
    const size_t objectBytes = profile.objectCount * sizeof(PdoObject);
    if (!span(profile.objects, objectBytes))
        return invalid("invalid PDO object span");
    if (overlap(profile.objects, objectBytes, &output, sizeof(output)))
        return illegal("PDO layout output overlaps object facts");
    if (!directionValid(map.direction))
        return illegal("invalid PDO direction");
    if (!map.number || map.number > profile.sets || map.count > profile.slots)
        return invalid("PDO set or entry count exceeds profile");
    for (size_t i = 0; i < profile.objectCount; ++i) {
        const PdoObject& object = profile.objects[i];
        if (!object.index)
            return invalid("invalid PDO object address");
        if (!representationValid(object))
            return illegal("invalid native PDO representation");
        for (size_t j = 0; j < i; ++j)
            if (sameAddress(object, profile.objects[j]))
                return invalid("duplicate PDO object facts");
    }
    PdoLayout candidate;
    candidate.profile = profile.profile;
    candidate.mapping = map;
    for (uint8_t i = 0; i < PDO_ENTRIES; ++i) {
        if (i >= map.count) {
            if (map.words[i])
                return invalid("nonzero inactive PDO mapping entry");
            continue;
        }
        const uint8_t bits = uint8_t(map.words[i]);
        if (!bits || bits > 32 || bits % 8)
            return illegal("unsupported PDO field width");
        const PdoObject* selected = nullptr;
        for (size_t j = 0; j < profile.objectCount; ++j)
            if ((map.words[i] >> 16) == profile.objects[j].index &&
                uint8_t(map.words[i] >> 8) == profile.objects[j].subindex)
                selected = &profile.objects[j];
        if (!selected || !eligible(*selected, map.direction))
            return invalid("PDO object is not eligible");
        if (!matches(map.words[i], *selected))
            return illegal("PDO mapping width differs from native width");
        for (uint8_t j = 0; j < i; ++j)
            if (sameAddress(*selected, candidate.fields[j]))
                return invalid("duplicate mapped PDO object");
        if (candidate.payloadLength + selected->widthBytes > 8)
            return illegal("PDO payload exceeds Classical CAN");
        candidate.fields[i] = *selected;
        candidate.payloadLength = uint8_t(candidate.payloadLength + selected->widthBytes);
    }
    output = candidate;
    return Ok();
}

Status validatePdoRoute(const PdoRoute& route) noexcept {
    if (!validateBinding(route.binding) || route.binding.endpoint.protocol != Protocol::CANOPEN ||
        !route.binding.endpoint.address || route.binding.endpoint.address > 127 || !route.generation ||
        route.layout.profile != route.binding.endpoint.profile ||
        !route.cobId || route.extended || route.cobId > 0x7ffu)
        return invalid("invalid PDO route binding or identifier");
    const PdoMap& map = route.layout.mapping;
    if (!directionValid(map.direction) || !map.number || map.number > 512 || !map.count ||
        map.count > PDO_ENTRIES)
        return invalid("invalid active PDO layout");
    uint8_t length = 0;
    for (uint8_t i = 0; i < PDO_ENTRIES; ++i) {
        if (i >= map.count) {
            if (map.words[i])
                return invalid("nonzero inactive PDO mapping entry");
            continue;
        }
        const PdoObject& field = route.layout.fields[i];
        if (!field.index || !representationValid(field) || !eligible(field, map.direction) ||
            !matches(map.words[i], field))
            return invalid("PDO layout differs from copied field facts");
        for (uint8_t j = 0; j < i; ++j)
            if (sameAddress(field, route.layout.fields[j]))
                return invalid("duplicate active PDO object");
        if (length + field.widthBytes > 8)
            return invalid("PDO layout exceeds Classical CAN");
        length = uint8_t(length + field.widthBytes);
    }
    if (length != route.layout.payloadLength)
        return invalid("inconsistent PDO payload length");
    return Ok();
}

bool samePdoRoute(const PdoRoute& a, const PdoRoute& b) noexcept {
    if (!sameBinding(a.binding,b.binding) || a.generation != b.generation || a.cobId != b.cobId || a.extended != b.extended ||
        a.layout.profile != b.layout.profile || a.layout.payloadLength != b.layout.payloadLength ||
        a.layout.mapping.direction != b.layout.mapping.direction || a.layout.mapping.number != b.layout.mapping.number ||
        a.layout.mapping.count != b.layout.mapping.count || a.layout.mapping.count > PDO_ENTRIES) return false;
    for (unsigned i = 0; i < PDO_ENTRIES; ++i) {
        if (a.layout.mapping.words[i] != b.layout.mapping.words[i]) return false;
        if (i >= a.layout.mapping.count) continue;
        const PdoObject& x = a.layout.fields[i]; const PdoObject& y = b.layout.fields[i];
        if (x.index != y.index || x.subindex != y.subindex || x.widthBytes != y.widthBytes ||
            x.sign != y.sign || x.role != y.role || x.rpdo != y.rpdo || x.tpdo != y.tpdo) return false;
    }
    return true;
}

Status decodePdo(const Frame& frame, const PdoRoute& route, PdoSample& output) noexcept {
    if (overlap(&frame, sizeof(frame), &output, sizeof(output)) ||
        overlap(&route, sizeof(route), &output, sizeof(output)))
        return illegal("PDO sample output overlaps input");
    Status status = validatePdoRoute(route);
    if (!status)
        return status;
    status = validateFrame(frame);
    if (!status)
        return status;
    if (frame.rtr || frame.extended != route.extended || frame.id != route.cobId ||
        frame.payloadLength != route.layout.payloadLength)
        return Status(Err::FRAME_ERROR, 0, "PDO envelope does not match active route");
    PdoSample candidate;
    candidate.route = route;
    candidate.frame = frame;
    uint8_t offset = 0;
    for (uint8_t i = 0; i < route.layout.mapping.count; ++i) {
        PdoValue& value = candidate.values[i];
        value.object = route.layout.fields[i];
        value.raw = scalar(frame.data + offset, value.object.widthBytes);
        value.numericKnown = value.object.sign != PdoSign::RAW;
        if (value.numericKnown) {
            value.native = int64_t(value.raw);
            if (value.object.sign == PdoSign::SIGNED &&
                (value.raw & (uint32_t(1) << (value.object.widthBytes * 8u - 1u))))
                value.native -= int64_t(1) << (value.object.widthBytes * 8u);
        }
        offset = uint8_t(offset + value.object.widthBytes);
    }
    output = candidate;
    return Ok();
}

Status pdoMapFromReadback(const SdoCompletion* results, size_t count, const PdoRoute& expected,
                         Generation nodeGeneration, uint32_t evidenceId, PdoMapEvidence& output) noexcept {
    if (overlap(&expected, sizeof(expected), &output, sizeof(output)))
        return illegal("PDO evidence output overlaps route");
    if (!validatePdoRoute(expected) || !nodeGeneration || !evidenceId || !results ||
        count != size_t(3 + expected.layout.mapping.count))
        return invalid("invalid PDO readback evidence bounds");
    const size_t resultBytes = count * sizeof(SdoCompletion);
    if (!span(results, resultBytes))
        return invalid("invalid PDO completion span");
    if (overlap(results, resultBytes, &output, sizeof(output)))
        return illegal("PDO evidence output overlaps completions");
    PdoMapEvidence candidate;
    candidate.route = expected;
    candidate.nodeGeneration = nodeGeneration;
    candidate.evidenceId = evidenceId;
    candidate.source = PdoMapSource::SDO_READBACK;
    const bool transmit = expected.layout.mapping.direction == PdoDirection::TPDO;
    const uint16_t communicationIndex =
        uint16_t((transmit ? 0x1800 : 0x1400) + expected.layout.mapping.number - 1);
    const uint16_t mappingIndex = uint16_t((transmit ? 0x1a00 : 0x1600) + expected.layout.mapping.number - 1);
    for (size_t i = 0; i < count; ++i) {
        const SdoCompletion& result = results[i];
        const SdoTransfer& transfer = result.transfer;
        const uint16_t index = i < 2 ? communicationIndex : mappingIndex;
        const uint8_t subindex = uint8_t(i < 2 ? i + 1 : i - 2);
        const uint8_t width = uint8_t(i == 1 || i == 2 ? 1 : 4);
        if (!validateSdoChannel(transfer.channel) || !sameBinding(transfer.channel.binding, expected.binding) ||
            transfer.channel.clientToServerId != results[0].transfer.channel.clientToServerId ||
            transfer.channel.serverToClientId != results[0].transfer.channel.serverToClientId ||
            transfer.index != index || transfer.subindex != subindex || transfer.size != width ||
            transfer.direction != SdoDirection::UPLOAD || result.outcome != SdoOutcome::SUCCESS ||
            !result.protocolAcknowledged || result.size != width || result.abortValid ||
            result.nodeGeneration != nodeGeneration || result.admittedUs > result.completedUs)
            return invalid("PDO readback lacks matching successful uploads");
        const uint32_t value = scalar(result.data, width);
        if (i == 0) {
            if (!communication(value, expected))
                return invalid("PDO COB-ID readback differs from route");
            candidate.cobIdParameter = value;
        }
        if (i == 1) {
            if (value > 240 && value != 254 && value != 255)
                return invalid("reserved PDO transmission type");
            candidate.transmissionType = uint8_t(value);
        }
        if (i == 2 && value != expected.layout.mapping.count)
            return invalid("PDO mapping count readback differs");
        if (i >= 3 && value != expected.layout.mapping.words[i - 3])
            return invalid("PDO mapping entry readback differs");
        if (i == 0 || result.admittedUs < candidate.startedUs)
            candidate.startedUs = result.admittedUs;
        if (result.completedUs > candidate.observedUs)
            candidate.observedUs = result.completedUs;
    }
    candidate.communicationKnown = true;
    output = candidate;
    return Ok();
}
bool pdoFreshness(const PdoRecord& record, uint64_t now, uint64_t age, PdoFreshness& output) {
    if (!age || now < record.boundaryUs || (record.confirmed && now < record.map.observedUs) ||
        (record.qualified && now < record.qualification.establishedUs) ||
        (record.samplePresent && (now < record.observation.time.processedUs ||
         (record.observation.time.receivedTimeKnown && now < record.observation.time.receivedUs))) ||
        overlap(&record, sizeof record, &output, sizeof output)) return false;
    PdoFreshness result;
    if (!record.confirmed) result = PdoFreshness::NO_MAP;
    else if (!record.qualified) result = PdoFreshness::UNQUALIFIED;
    else if (!record.samplePresent || !record.observation.time.receivedTimeKnown ||
        !samePdoRoute(record.observation.sample.route, record.map.route) ||
        record.observation.evidenceId != record.qualification.evidenceId ||
        record.observation.time.receivedUs <= record.qualification.establishedUs) result = PdoFreshness::NEVER_OBSERVED;
    else result = now - record.observation.time.receivedUs <= age ? PdoFreshness::FRESH : PdoFreshness::EXPIRED;
    output = result;
    return true;
}
} // namespace CANOPEN
} // namespace MotorControlCAN
