// SPDX-License-Identifier: MIT
// The native read group table, and the driver and I/O group reads built on it.
#include "ReadPlan.h"

namespace MotorControlCAN {
namespace LICHUAN_C {
using namespace Detail;
namespace {
const Address driver[] = {{0x2200, 0}, {0x2201, 0}};
const Address softLimits[] = {{0x2202, 0}, {0x607d, 0}, {0x607d, 1}, {0x607d, 2}};
const Address motionParameters[] = {{0x6081, 0}, {0x6083, 0}, {0x6084, 0}};
const Address velocityTarget[] = {{0x60ff, 0}};
const Address persistence[] = {{0x2300, 0}};
const Address homing[] = {{0x607c, 0}, {0x6098, 0}, {0x6099, 0},
                          {0x6099, 1}, {0x6099, 2}, {0x609a, 0}};
const Address ioInputs[] = {{0x2400, 0}, {0x2400, 1}, {0x2400, 2}, {0x2400, 3}, {0x2400, 4},
                            {0x2400, 5}, {0x2400, 6}, {0x2400, 7}, {0x2400, 8}};
const Address ioOutputs[] = {{0x2400, 0x0c}, {0x2400, 0x0d}, {0x2400, 0x0e}, {0x2400, 0x0f}};
const Address ioFilters[] = {{0x2401, 0}, {0x2402, 0}, {0x2403, 0}, {0x2404, 0},
                             {0x2405, 0}, {0x2406, 0}, {0x2407, 0}};
const Address ioCommand[] = {{0x60fe, 0}, {0x60fe, 1}, {0x60fe, 2}};
const Address control[] = {{0x2500, 0}, {0x2501, 0}};
const Address currentHolding[] = {{0x2502, 0}, {0x2503, 0}, {0x2504, 0},
                                  {0x2505, 0}, {0x2506, 0}, {0x2507, 0}};
const Address filterPosition[] = {{0x2508, 0}, {0x2509, 0}, {0x250a, 0},
                                  {0x250b, 0}, {0x250c, 0}, {0x250d, 0}};
const Address loopGains[] = {{0x250e, 0}, {0x250f, 0}, {0x2510, 0}, {0x2511, 0},
                             {0x2512, 0}, {0x2513, 0}, {0x2514, 0}, {0x2515, 0},
                             {0x2516, 0}, {0x2517, 0}, {0x2518, 0}, {0x2519, 0}};
const Address edsGains[] = {{0x251a, 0}, {0x251b, 0}, {0x251c, 0}, {0x251d, 0},
                            {0x251e, 0}, {0x251f, 0}, {0x2520, 0}};
template <size_t N> constexpr uint8_t length(const Address (&)[N]) {
    return uint8_t(N);
}
const TuningGroup noTuning = TuningGroup::CURRENT_HOLDING; // Unread outside the TUNING family.
const GroupPlan groups[] = {
    {DriverGroup::DRIVER, ReadKind::DRIVER, GroupFamily::DRIVER, noTuning, driver, length(driver)},
    {DriverGroup::SOFT_LIMITS, ReadKind::DRIVER_SOFT_LIMITS, GroupFamily::DRIVER, noTuning,
     softLimits, length(softLimits)},
    {DriverGroup::MOTION_PARAMETERS, ReadKind::DRIVER_MOTION_PARAMETERS, GroupFamily::DRIVER,
     noTuning, motionParameters, length(motionParameters)},
    {DriverGroup::VELOCITY_TARGET, ReadKind::VELOCITY_TARGET, GroupFamily::DRIVER, noTuning,
     velocityTarget, length(velocityTarget)},
    {DriverGroup::HOMING, ReadKind::HOMING, GroupFamily::DRIVER, noTuning, homing, length(homing)},
    {DriverGroup::PERSISTENCE, ReadKind::PERSISTENCE, GroupFamily::DRIVER, noTuning, persistence,
     length(persistence)},
    {DriverGroup::IO_INPUT_CONFIGURATION, ReadKind::IO_INPUT_CONFIGURATION, GroupFamily::IO,
     noTuning, ioInputs, length(ioInputs)},
    {DriverGroup::IO_OUTPUT_CONFIGURATION, ReadKind::IO_OUTPUT_CONFIGURATION, GroupFamily::IO,
     noTuning, ioOutputs, length(ioOutputs)},
    {DriverGroup::IO_INPUT_FILTERS, ReadKind::IO_INPUT_FILTERS, GroupFamily::IO, noTuning,
     ioFilters, length(ioFilters)},
    {DriverGroup::IO_OUTPUT_COMMAND, ReadKind::IO_OUTPUT_COMMAND, GroupFamily::IO, noTuning,
     ioCommand, length(ioCommand)},
    {DriverGroup::CONTROL, ReadKind::CONTROL, GroupFamily::CONTROL, noTuning, control,
     length(control)},
    {DriverGroup::TUNING_CURRENT_HOLDING, ReadKind::TUNING_CURRENT_HOLDING, GroupFamily::TUNING,
     TuningGroup::CURRENT_HOLDING, currentHolding, length(currentHolding)},
    {DriverGroup::TUNING_FILTER_POSITION, ReadKind::TUNING_FILTER_POSITION, GroupFamily::TUNING,
     TuningGroup::FILTER_POSITION, filterPosition, length(filterPosition)},
    {DriverGroup::TUNING_LOOP_GAINS, ReadKind::TUNING_LOOP_GAINS, GroupFamily::TUNING,
     TuningGroup::LOOP_GAINS, loopGains, length(loopGains)},
    {DriverGroup::TUNING_EDS_GAINS, ReadKind::TUNING_EDS_GAINS, GroupFamily::TUNING,
     TuningGroup::EDS_GAINS, edsGains, length(edsGains)},
};
} // namespace

namespace Detail {
const GroupPlan* groupPlan(DriverGroup group) noexcept {
    for (const GroupPlan& plan : groups)
        if (plan.group == group)
            return &plan;
    return nullptr;
}
const GroupPlan* groupPlan(ReadKind kind) noexcept {
    for (const GroupPlan& plan : groups)
        if (plan.kind == kind)
            return &plan;
    return nullptr;
}
} // namespace Detail

Status prepareDriverRead(DriverContext& c, const ReadTarget& target, uint32_t operation,
                         uint64_t now, uint64_t deadline, const ReadOptions& options,
                         DriverGroup group) noexcept {
    const GroupPlan* plan = groupPlan(group);
    if (!plan)
        return readInvalid(ReadError::ARGUMENT, "unknown native driver group");
    return prepareRead(c.read, target, operation, now, deadline, options, plan->kind, 0, nullptr);
}
Status nextDriver(const DriverContext& c, uint64_t now, PreparedRead& output) noexcept {
    if (!groupPlan(c.read.kind))
        return readInvalid(ReadError::STATE, "not a driver context");
    return nextRead(c.read, now, output);
}
Status advanceDriver(DriverContext& c, const CANOPEN::SdoCompletion& input, uint64_t now) noexcept {
    if (!groupPlan(c.read.kind))
        return readInvalid(ReadError::STATE, "not a driver context");
    return advanceRead(c.read, input, now);
}
Status advanceDriver(DriverContext& c, ReadOutcome reason, uint64_t now) noexcept {
    if (!groupPlan(c.read.kind))
        return readInvalid(ReadError::STATE, "not a driver context");
    return advanceRead(c.read, reason, now);
}
Status getDriver(const DriverContext& c, const ReadTarget& target, uint64_t now, uint64_t age,
                 DriverObservation& output) noexcept {
    DriverObservation candidate;
    const Status status = beginGet(c.read, &output, sizeof output, target, now, age,
                                   inFamily(c.read, GroupFamily::DRIVER), candidate.window);
    if (!status)
        return status;
    candidate.target = c.read.target;
    candidate.group = groupPlan(c.read.kind)->group;
    candidate.fieldCount = c.read.count;
    copyProvenance(c.read, candidate.fields);
    const ReadValue* v = c.read.values;
    switch (candidate.group) {
    case DriverGroup::DRIVER:
        candidate.directionRaw = uint16_t(v[0].raw);
        candidate.subdivisionRaw = uint16_t(v[1].raw);
        break;
    case DriverGroup::SOFT_LIMITS:
        candidate.softLimitSelectorRaw = uint16_t(v[0].raw);
        candidate.softLimitEntries = uint16_t(v[1].raw);
        candidate.softLimitEntriesWidth = v[1].object.widthBytes;
        candidate.minimumNative = signed32(v[2].raw);
        candidate.maximumNative = signed32(v[3].raw);
        break;
    case DriverGroup::MOTION_PARAMETERS:
        candidate.positionSpeedRpm = v[0].raw;
        candidate.rampUpTimeMs = v[1].raw;
        candidate.rampDownTimeMs = v[2].raw;
        break;
    case DriverGroup::VELOCITY_TARGET:
        candidate.velocityTargetRpm = signed32(v[0].raw);
        break;
    case DriverGroup::PERSISTENCE:
        candidate.persistenceRaw = uint16_t(v[0].raw);
        break;
    case DriverGroup::HOMING:
        candidate.homeOffsetRaw = signed32(v[0].raw);
        candidate.homeMethodRaw = uint8_t(v[1].raw);
        candidate.homeSpeedEntries = uint8_t(v[2].raw);
        candidate.homeSearchSpeedRpm = v[3].raw;
        candidate.homeCreepSpeedRpm = v[4].raw;
        candidate.homeRampTimeMs = v[5].raw;
        break;
    default:
        return readInvalid(ReadError::STATE, "unknown driver observation");
    }
    output = candidate;
    return Ok();
}
Status getIo(const DriverContext& c, const ReadTarget& target, uint64_t now, uint64_t age,
             IoObservation& output) noexcept {
    IoObservation candidate;
    const Status status = beginGet(c.read, &output, sizeof output, target, now, age,
                                   inFamily(c.read, GroupFamily::IO), candidate.window);
    if (!status)
        return status;
    candidate.group = groupPlan(c.read.kind)->group;
    candidate.target = c.read.target;
    candidate.fieldCount = c.read.count;
    copyProvenance(c.read, candidate.fields);
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
        candidate.outputEntries = uint16_t(v[0].raw);
        candidate.outputEntriesWidth = v[0].object.widthBytes;
        candidate.outputValueRaw = v[1].raw;
        candidate.outputMaskRaw = v[2].raw;
        break;
    default:
        return readInvalid(ReadError::STATE, "unknown I/O observation");
    }
    output = candidate;
    return Ok();
}
} // namespace LICHUAN_C
} // namespace MotorControlCAN
