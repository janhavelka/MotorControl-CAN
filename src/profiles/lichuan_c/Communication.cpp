// SPDX-License-Identifier: MIT
#include "ReadPlan.h"
#include <MotorControlCAN/profiles/lichuan_c/Communication.h>

namespace MotorControlCAN {
namespace LICHUAN_C {
using namespace Detail;
namespace {
bool readKind(CommunicationGroup group, ReadKind& kind) {
    switch (group) {
    case CommunicationGroup::NETWORK:
        kind = ReadKind::CONFIG;
        return true;
    case CommunicationGroup::NODE_SETTING:
        kind = ReadKind::COMMUNICATION_NODE;
        return true;
    case CommunicationGroup::RPDO_COMMUNICATION:
        kind = ReadKind::RPDO_COMMUNICATION;
        return true;
    case CommunicationGroup::TPDO_COMMUNICATION:
        kind = ReadKind::TPDO_COMMUNICATION;
        return true;
    case CommunicationGroup::RPDO_MAPPING:
        kind = ReadKind::RPDO_MAPPING;
        return true;
    case CommunicationGroup::TPDO_MAPPING:
        kind = ReadKind::TPDO_MAPPING;
        return true;
    }
    return false;
}
bool communicationGroup(ReadKind kind, CommunicationGroup& group) {
    switch (kind) {
    case ReadKind::CONFIG:
        group = CommunicationGroup::NETWORK;
        return true;
    case ReadKind::COMMUNICATION_NODE:
        group = CommunicationGroup::NODE_SETTING;
        return true;
    case ReadKind::RPDO_COMMUNICATION:
        group = CommunicationGroup::RPDO_COMMUNICATION;
        return true;
    case ReadKind::TPDO_COMMUNICATION:
        group = CommunicationGroup::TPDO_COMMUNICATION;
        return true;
    case ReadKind::RPDO_MAPPING:
        group = CommunicationGroup::RPDO_MAPPING;
        return true;
    case ReadKind::TPDO_MAPPING:
        group = CommunicationGroup::TPDO_MAPPING;
        return true;
    default:
        return false;
    }
}
bool communicationContext(const CommunicationReadContext& context) {
    CommunicationGroup group;
    return communicationGroup(context.read.kind, group);
}
} // namespace

Status prepareCommunicationRead(CommunicationReadContext& output, const ReadTarget& target,
                                uint32_t operation, uint64_t now, uint64_t deadline,
                                const ReadOptions& options, CommunicationGroup group) noexcept {
    ReadKind kind;
    if (!readKind(group, kind))
        return readInvalid(ReadError::ARGUMENT, "unknown communication read group");
    return prepareRead(output.read, target, operation, now, deadline, options, kind, 0, nullptr);
}

Status nextCommunication(const CommunicationReadContext& context, uint64_t now,
                         PreparedRead& output) noexcept {
    if (!communicationContext(context))
        return readInvalid(ReadError::STATE, "not a communication read context");
    return nextRead(context.read, now, output);
}

Status advanceCommunication(CommunicationReadContext& context, const CANOPEN::SdoCompletion& input,
                            uint64_t now) noexcept {
    if (!communicationContext(context))
        return readInvalid(ReadError::STATE, "not a communication read context");
    return advanceRead(context.read, input, now);
}

Status advanceCommunication(CommunicationReadContext& context, ReadOutcome reason,
                            uint64_t now) noexcept {
    if (!communicationContext(context))
        return readInvalid(ReadError::STATE, "not a communication read context");
    return advanceRead(context.read, reason, now);
}

Status getCommunication(const CommunicationReadContext& context, const ReadTarget& target,
                        uint64_t now, uint64_t age, CommunicationObservation& output) noexcept {
    CommunicationObservation candidate;
    const bool kindMatches = communicationGroup(context.read.kind, candidate.group);
    const Status status = beginGet(context.read, &output, sizeof output, target, now, age,
                                   kindMatches, candidate.window);
    if (!status)
        return status;
    candidate.target = context.read.target;
    candidate.pdoNumber = context.read.options.pdoNumber;
    candidate.fieldCount = context.read.count;
    copyProvenance(context.read, candidate.fields);
    const ReadValue* values = context.read.values;
    switch (candidate.group) {
    case CommunicationGroup::NETWORK: {
        const Status projected = getConfig(context.read, target, now, age, candidate.network);
        if (!projected)
            return projected;
        break;
    }
    case CommunicationGroup::NODE_SETTING:
        candidate.nodeSettingRaw = uint16_t(values[0].raw);
        break;
    case CommunicationGroup::RPDO_COMMUNICATION:
    case CommunicationGroup::TPDO_COMMUNICATION:
        candidate.communication.entriesRaw = uint8_t(values[0].raw);
        candidate.communication.cobIdRaw = values[1].raw;
        candidate.communication.transmissionTypeRaw = uint8_t(values[2].raw);
        candidate.communication.inhibitTimeRaw = uint16_t(values[3].raw);
        candidate.communication.compatibilityRaw = uint8_t(values[4].raw);
        candidate.communication.eventTimerRaw = uint16_t(values[5].raw);
        break;
    case CommunicationGroup::RPDO_MAPPING:
    case CommunicationGroup::TPDO_MAPPING:
        candidate.mapping.countRaw = uint8_t(values[0].raw);
        for (unsigned i = 0; i < 4; ++i)
            candidate.mapping.words[i] = values[i + 1].raw;
        break;
    }
    output = candidate;
    return Ok();
}
} // namespace LICHUAN_C
} // namespace MotorControlCAN
