// SPDX-License-Identifier: MIT
#include <MotorControlCAN/profiles/lichuan_c/State.h>

namespace MotorControlCAN {
namespace LICHUAN_C {
namespace {
bool overlap(const void* a, size_t an, const void* b, size_t bn) {
    const uintptr_t x = reinterpret_cast<uintptr_t>(a), y = reinterpret_cast<uintptr_t>(b);
    return x >= y ? x - y < bn : y - x < an;
}
StateIndication indication(bool value) {
    return value ? StateIndication::YES : StateIndication::NO;
}
bool differs(StateIndication status, bool monitor) {
    return status != StateIndication::UNKNOWN && status != indication(monitor);
}
void interpretStatus(StateInterpretation& value) {
    const uint16_t word = value.statusword;
    value.lowStatusBits = uint8_t(word & 7u);
    value.unlistedStatusBits = word;
    if (value.layout == StatusLayout::UNSPECIFIED)
        return;
    value.lowPatternListed = value.lowStatusBits == 0 || value.lowStatusBits == 1 ||
                             value.lowStatusBits == 3 || value.lowStatusBits == 7;
    // These are the literal source tables, not generic CiA402 state masks.
    constexpr uint16_t commonMask = 0x8107;            // Low examples, home8, position15.
    constexpr uint16_t enMask = commonMask | 0x1480;   // Alarm7, zero10, effective12.
    constexpr uint16_t zh86Mask = commonMask | 0x4008; // Alarm3, run14.
    constexpr uint16_t zh57Mask = commonMask | 0x4080; // Alarm7, run14.
    uint16_t mask = zh57Mask;
    if (value.layout == StatusLayout::EN86) {
        mask = enMask;
        value.alarm = indication((word & 0x0080) != 0);
        value.running = indication((word & 0x0400) == 0);
        value.targetEffective = indication((word & 0x1000) != 0);
    } else {
        if (value.layout == StatusLayout::ZH86)
            mask = zh86Mask;
        value.alarm =
            indication((word & (value.layout == StatusLayout::ZH86 ? 0x0008 : 0x0080)) != 0);
        value.running = indication((word & 0x4000) != 0);
    }
    value.homingComplete = indication((word & 0x0100) != 0);
    value.positionComplete = indication((word & 0x8000) != 0);
    value.unlistedStatusBits = uint16_t(word & ~mask);
}
} // namespace

Status getStateInterpretation(const ReadContext& context, const ReadTarget& current, uint64_t now,
                              uint64_t age, StatusLayout layout,
                              StateInterpretation& output) noexcept {
    if (overlap(&output, sizeof output, &context, sizeof context) ||
        overlap(&output, sizeof output, &current, sizeof current))
        return Status(Err::INVALID_CONFIG, int32_t(ReadError::OVERLAP),
                      "state interpretation overlaps input");
    if (layout != StatusLayout::UNSPECIFIED && layout != StatusLayout::EN86 &&
        layout != StatusLayout::ZH86 && layout != StatusLayout::ZH57 &&
        layout != StatusLayout::OP86)
        return Status(Err::INVALID_CONFIG, int32_t(ReadError::ARGUMENT),
                      "unknown documentary status layout");
    StateObservation status, monitors;
    Status result = getStateBlock(context, StateBlock::STATUS, current, now, age, status);
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
    interpretStatus(candidate);
    const uint16_t monitor = monitors.monitor;
    candidate.monitorPositionComplete = (monitor & 0x0001) != 0;
    candidate.monitorHomingComplete = (monitor & 0x0002) != 0;
    candidate.monitorRunning = (monitor & 0x0004) != 0;
    candidate.monitorAlarm = (monitor & 0x0008) != 0;
    candidate.monitorReleased = (monitor & 0x0010) != 0;
    candidate.positiveSoftwareLimit = (monitor & 0x0020) != 0;
    candidate.negativeSoftwareLimit = (monitor & 0x0040) != 0;
    candidate.unlistedMonitorBits = uint16_t(monitor & ~uint16_t(0x007f));
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
