// SPDX-License-Identifier: MIT
#include <MotorControlCAN/profiles/lichuan_c/DriverSettings.h>
#include <cinttypes>
#include <cstdio>

namespace M = MotorControlCAN;
namespace C = MotorControlCAN::CANOPEN;
namespace L = MotorControlCAN::LICHUAN_C;
namespace {
struct SyntheticValue {
    uint16_t index;
    uint8_t subindex, width;
    uint32_t raw;
};
// Fixture values demonstrate U32 ramp times, including an unqualified raw value.
const SyntheticValue values[] = {{0x6081, 0, 4, 1234}, {0x6083, 0, 4, 250}, {0x6084, 0, 4, 2001}};
L::ReadTarget syntheticTarget() {
    L::ReadTarget t;
    t.channel.binding.logicalAxisId = 9;
    t.channel.binding.endpoint.profile = 1;
    t.channel.binding.endpoint.address = 17;
    t.channel.binding.bindingGeneration = 2;
    t.channel.binding.configurationGeneration = 3;
    t.channel.clientToServerId = 0x651;
    t.channel.serverToClientId = 0x5d1;
    t.nodeGeneration = 4;
    return t;
}
bool checked(M::Status status, const char* operation) {
    if (status)
        return true;
    std::fprintf(stderr, "%s: %s (%" PRId32 "): %s\n", operation, M::errToString(status.code),
                 status.detail, status.msg);
    return false;
}
} // namespace
int main() {
    std::puts("Offline synthetic native settings read; no controller, stack or clock is opened.");
    std::puts("Route, generations, values, supplied times and qualified terminals are fixtures.");
    const L::ReadTarget target = syntheticTarget();
    L::ReadOptions options;
    options.queueTimeoutUs = options.responseTimeoutUs = 100;
    L::DriverContext context;
    if (!checked(L::prepareDriverRead(context, target, 1, 0, 1000, options,
                                      L::DriverGroup::MOTION_PARAMETERS),
                 "prepareDriverRead"))
        return 1;
    uint64_t now = 0;
    for (const SyntheticValue& value : values) {
        L::PreparedRead work;
        if (!checked(L::nextDriver(context, now, work), "nextDriver"))
            return 1;
        if (!work.ready || work.transfer.index != value.index ||
            work.transfer.subindex != value.subindex || work.transfer.size != value.width ||
            work.transfer.direction != C::SdoDirection::UPLOAD)
            return 2;
        // Production callers obtain this retained terminal from their sole service owner.
        C::SdoCompletion terminal;
        terminal.transfer = work.transfer;
        terminal.nodeGeneration = target.nodeGeneration;
        terminal.admittedUs = now;
        terminal.completedUs = now + 3;
        terminal.outcome = C::SdoOutcome::SUCCESS;
        terminal.queued = terminal.transmissionPossible = terminal.protocolAcknowledged = true;
        terminal.effectPossible = true;
        terminal.size = value.width;
        for (unsigned i = 0; i < value.width; ++i)
            terminal.data[i] = uint8_t(value.raw >> (8 * i));
        if (!checked(L::advanceDriver(context, terminal, now + 4), "advanceDriver"))
            return 1;
        now += 10;
    }
    L::PreparedRead done;
    if (!checked(L::nextDriver(context, now, done), "terminal nextDriver") || done.ready)
        return 2;
    L::DriverObservation observed;
    if (!checked(L::getDriver(context, target, now, 1000, observed), "getDriver"))
        return 1;
    std::printf("Raw position speed=%" PRIu32 " rpm, ramp up=%" PRIu32 " ms, ramp down=%" PRIu32
                " ms\n",
                observed.positionSpeedRpm, observed.rampUpTimeMs, observed.rampDownTimeMs);
    std::printf("Acquisition interval=[%" PRIu64 ", %" PRIu64 "] us; skew=%" PRIu64 " us\n",
                observed.window.earliestUs, observed.window.latestUs, observed.window.skewUs);
    std::puts("Raw ramp-down 2001 ms remains observable; this read grants no write permission.");
    std::puts(
        "Upload effects, activation, coordinate scale and firmware applicability remain unknown.");
    return context.read.state == M::ReadState::SUCCEEDED && !observed.interpreted ? 0 : 2;
}
