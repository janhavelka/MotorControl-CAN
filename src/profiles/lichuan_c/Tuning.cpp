// SPDX-License-Identifier: MIT
// Control and tuning reads. Control and DI writes share the settings transaction in Settings.cpp.
#include "ReadPlan.h"

namespace MotorControlCAN {
namespace LICHUAN_C {
using namespace Detail;

Status prepareControlRead(DriverContext& output, const ReadTarget& target, uint32_t operation,
                          uint64_t now, uint64_t deadline, const ReadOptions& options) noexcept {
    return prepareDriverRead(output, target, operation, now, deadline, options,
                             DriverGroup::CONTROL);
}
Status prepareTuningRead(DriverContext& output, const ReadTarget& target, uint32_t operation,
                         uint64_t now, uint64_t deadline, const ReadOptions& options,
                         TuningGroup group) noexcept {
    DriverGroup native;
    switch (group) {
    case TuningGroup::CURRENT_HOLDING:
        native = DriverGroup::TUNING_CURRENT_HOLDING;
        break;
    case TuningGroup::FILTER_POSITION:
        native = DriverGroup::TUNING_FILTER_POSITION;
        break;
    case TuningGroup::LOOP_GAINS:
        native = DriverGroup::TUNING_LOOP_GAINS;
        break;
    case TuningGroup::EDS_GAINS:
        native = DriverGroup::TUNING_EDS_GAINS;
        break;
    default:
        return readInvalid(ReadError::ARGUMENT, "unknown finite tuning group");
    }
    return prepareDriverRead(output, target, operation, now, deadline, options, native);
}
Status getControl(const DriverContext& c, const ReadTarget& target, uint64_t now, uint64_t age,
                  ControlObservation& output) noexcept {
    ControlObservation candidate;
    const Status s = beginGet(c.read, &output, sizeof output, target, now, age,
                              inFamily(c.read, GroupFamily::CONTROL), candidate.window);
    if (!s)
        return s;
    candidate.target = c.read.target;
    candidate.loopModeRaw = uint16_t(c.read.values[0].raw);
    if (candidate.loopModeRaw <= 2)
        candidate.loopMode = static_cast<LoopMode>(candidate.loopModeRaw);
    candidate.encoderResolutionRaw = uint16_t(c.read.values[1].raw);
    copyProvenance(c.read, candidate.fields);
    output = candidate;
    return Ok();
}
Status getTuning(const DriverContext& c, const ReadTarget& target, uint64_t now, uint64_t age,
                 TuningObservation& output) noexcept {
    TuningObservation candidate;
    const Status s = beginGet(c.read, &output, sizeof output, target, now, age,
                              inFamily(c.read, GroupFamily::TUNING), candidate.window);
    if (!s)
        return s;
    candidate.group = groupPlan(c.read.kind)->tuning;
    candidate.target = c.read.target;
    candidate.fieldCount = c.read.count;
    copyProvenance(c.read, candidate.fields);
    const ReadValue* v = c.read.values;
    switch (candidate.group) {
    case TuningGroup::CURRENT_HOLDING:
        candidate.maximumPeakCurrentMa = uint16_t(v[0].raw);
        candidate.closedLoopMaximumPercent = uint16_t(v[1].raw);
        candidate.closedLoopBasePercent = uint16_t(v[2].raw);
        candidate.openLoopMaximumPercent = uint16_t(v[3].raw);
        candidate.holdingCurrentPercent = uint16_t(v[4].raw);
        candidate.holdingTimeMs = uint16_t(v[5].raw);
        break;
    case TuningGroup::FILTER_POSITION:
        candidate.lowPassEnableRaw = uint16_t(v[0].raw);
        candidate.lowPassCoefficientRaw = uint16_t(v[1].raw);
        candidate.positionAlarmThresholdRaw = uint16_t(v[2].raw);
        candidate.inPositionThresholdRaw = uint16_t(v[3].raw);
        candidate.inPositionTimeRaw = uint16_t(v[4].raw);
        candidate.averagingFilterRaw = uint16_t(v[5].raw);
        break;
    case TuningGroup::LOOP_GAINS:
        candidate.currentProportionalFactorRaw = uint16_t(v[0].raw);
        candidate.currentKpRaw = uint16_t(v[1].raw);
        candidate.currentKiRaw = uint16_t(v[2].raw);
        candidate.currentKcRaw = uint16_t(v[3].raw);
        candidate.velocityKp1Raw = uint16_t(v[4].raw);
        candidate.velocityKv1Raw = uint16_t(v[5].raw);
        candidate.velocityNode1Raw = uint16_t(v[6].raw);
        candidate.velocityKp2Raw = uint16_t(v[7].raw);
        candidate.velocityKv2Raw = uint16_t(v[8].raw);
        candidate.velocityNode2Raw = uint16_t(v[9].raw);
        candidate.velocityFeedforwardRaw = uint16_t(v[10].raw);
        candidate.positionKiRaw = uint16_t(v[11].raw);
        break;
    case TuningGroup::EDS_GAINS:
        for (unsigned i = 0; i < 7; ++i)
            candidate.edsGainsRaw[i] = uint16_t(v[i].raw);
        break;
    }
    output = candidate;
    return Ok();
}
} // namespace LICHUAN_C
} // namespace MotorControlCAN
