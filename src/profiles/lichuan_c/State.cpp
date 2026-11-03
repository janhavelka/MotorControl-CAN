// SPDX-License-Identifier: MIT
#include "../../canopen/SdoEvidence.h"
#include <MotorControlCAN/profiles/lichuan_c/State.h>

namespace MotorControlCAN {
namespace LICHUAN_C {
namespace {
using CANOPEN::overlaps;
StateIndication indication(bool value) {
    return value ? StateIndication::YES : StateIndication::NO;
}
bool differs(StateIndication status, bool monitor) {
    return status != StateIndication::UNKNOWN && status != indication(monitor);
}
bool layoutKnown(StatusLayout layout) {
    return layout == StatusLayout::UNSPECIFIED || layout == StatusLayout::EN86 ||
           layout == StatusLayout::ZH86 || layout == StatusLayout::ZH57 ||
           layout == StatusLayout::OP86;
}
} // namespace

Status interpretStatusword(uint16_t word, StatusLayout layout, StatusIndications& output) noexcept {
    if (!layoutKnown(layout))
        return Status(Err::INVALID_CONFIG, int32_t(ReadError::ARGUMENT),
                      "unknown documentary status layout");
    StatusIndications value;
    value.layout = layout;
    value.statusword = word;
    value.lowStatusBits = uint8_t(word & 7u);
    value.unlistedStatusBits = word;
    if (layout != StatusLayout::UNSPECIFIED) {
        value.lowPatternListed = value.lowStatusBits == 0 || value.lowStatusBits == 1 ||
                                 value.lowStatusBits == 3 || value.lowStatusBits == 7;
        // These are the literal source tables, not generic CiA402 state masks.
        constexpr uint16_t commonMask = 0x8107;            // Low examples, home8, position15.
        constexpr uint16_t enMask = commonMask | 0x1480;   // Alarm7, zero10, effective12.
        constexpr uint16_t zh86Mask = commonMask | 0x4008; // Alarm3, run14.
        constexpr uint16_t zh57Mask = commonMask | 0x4080; // Alarm7, run14.
        uint16_t mask = zh57Mask;
        if (layout == StatusLayout::EN86) {
            mask = enMask;
            value.alarm = indication((word & 0x0080) != 0);
            value.running = indication((word & 0x0400) == 0);
            value.targetEffective = indication((word & 0x1000) != 0);
        } else {
            if (layout == StatusLayout::ZH86)
                mask = zh86Mask;
            value.alarm = indication((word & (layout == StatusLayout::ZH86 ? 0x0008 : 0x0080)) != 0);
            value.running = indication((word & 0x4000) != 0);
        }
        value.homingComplete = indication((word & 0x0100) != 0);
        value.positionComplete = indication((word & 0x8000) != 0);
        value.unlistedStatusBits = uint16_t(word & ~mask);
    }
    output = value;
    return Ok();
}

Status getStateInterpretation(const ReadContext& context, const ReadTarget& current, uint64_t now,
                              uint64_t age, StatusLayout layout,
                              StateInterpretation& output) noexcept {
    if (overlaps(&output, sizeof output, &context, sizeof context) ||
        overlaps(&output, sizeof output, &current, sizeof current))
        return Status(Err::INVALID_CONFIG, int32_t(ReadError::OVERLAP),
                      "state interpretation overlaps input");
    StatusIndications decoded;
    Status result = interpretStatusword(0, layout, decoded); // Refuses an unknown layout first.
    if (!result)
        return result;
    StateObservation status, monitors;
    result = getStateBlock(context, StateBlock::STATUS, current, now, age, status);
    if (!result)
        return result;
    result = getStateBlock(context, StateBlock::MONITORS, current, now, age, monitors);
    if (!result)
        return result;
    StateInterpretation candidate;
    candidate.target = status.target;
    candidate.layout = layout;
    candidate.statusword = status.statusword;
    candidate.modeRaw = status.modeRaw;
    candidate.monitor = monitors.monitor;
    candidate.logicalInputs = monitors.logicalInputs;
    candidate.logicalOutputs = monitors.logicalOutputs;
    if (status.modeRaw == 1 || status.modeRaw == 3 || status.modeRaw == 6)
        candidate.mode = static_cast<ObservedMode>(status.modeRaw);
    interpretStatusword(status.statusword, layout, decoded); // The layout was accepted above.
    candidate.lowStatusBits = decoded.lowStatusBits;
    candidate.lowPatternListed = decoded.lowPatternListed;
    candidate.unlistedStatusBits = decoded.unlistedStatusBits;
    candidate.alarm = decoded.alarm;
    candidate.running = decoded.running;
    candidate.homingComplete = decoded.homingComplete;
    candidate.positionComplete = decoded.positionComplete;
    candidate.targetEffective = decoded.targetEffective;
    const uint16_t monitor = monitors.monitor;
    candidate.monitorPositionComplete = (monitor & MONITOR_POSITION_COMPLETE) != 0;
    candidate.monitorHomingComplete = (monitor & MONITOR_HOMING_COMPLETE) != 0;
    candidate.monitorRunning = (monitor & MONITOR_RUNNING) != 0;
    candidate.monitorAlarm = (monitor & MONITOR_ALARM) != 0;
    candidate.monitorReleased = (monitor & MONITOR_RELEASED) != 0;
    candidate.positiveSoftwareLimit = (monitor & MONITOR_POSITIVE_SOFT_LIMIT) != 0;
    candidate.negativeSoftwareLimit = (monitor & MONITOR_NEGATIVE_SOFT_LIMIT) != 0;
    candidate.unlistedMonitorBits = uint16_t(monitor & ~MONITOR_DOCUMENTED_BITS);
    candidate.alarmDisagreement = differs(candidate.alarm, candidate.monitorAlarm);
    candidate.runningDisagreement = differs(candidate.running, candidate.monitorRunning);
    candidate.homingDisagreement =
        differs(candidate.homingComplete, candidate.monitorHomingComplete);
    candidate.positionDisagreement =
        differs(candidate.positionComplete, candidate.monitorPositionComplete);
    for (unsigned i = 0; i < 2; ++i)
        candidate.fields[i] = status.fields[i];
    for (unsigned i = 0; i < 3; ++i)
        candidate.fields[i + 2] = monitors.fields[i];
    candidate.window.earliestUs = status.window.earliestUs < monitors.window.earliestUs
                                      ? status.window.earliestUs
                                      : monitors.window.earliestUs;
    candidate.window.latestUs = status.window.latestUs > monitors.window.latestUs
                                    ? status.window.latestUs
                                    : monitors.window.latestUs;
    candidate.window.skewUs = candidate.window.latestUs - candidate.window.earliestUs;
    output = candidate;
    return Ok();
}
} // namespace LICHUAN_C
} // namespace MotorControlCAN
