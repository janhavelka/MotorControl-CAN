// SPDX-License-Identifier: MIT
#include <MotorControlCAN/profiles/lichuan_c/IoSettings.h>
#include <MotorControlCAN/profiles/lichuan_c/Settings.h>
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
// Deliberately undefined/origin/unlisted functions and an unexplained polarity bit.
const SyntheticValue values[] = {{0x2400, 0, 2, 65535}, {0x2400, 1, 2, 0x8001}, {0x2400, 2, 2, 0},
                                 {0x2400, 3, 2, 1},     {0x2400, 4, 2, 99},     {0x2400, 5, 2, 2},
                                 {0x2400, 6, 2, 3},     {0x2400, 7, 2, 5},      {0x2400, 8, 2, 6}};
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
    std::puts("Offline synthetic I/O configuration read; no controller, stack or clock is opened.");
    std::puts("Route, generations, values, supplied times and successful service terminals are "
              "fixtures.");
    const L::ReadTarget target = syntheticTarget();
    L::ReadOptions options;
    options.queueTimeoutUs = options.responseTimeoutUs = 100;
    L::DriverContext context;
    if (!checked(L::prepareDriverRead(context, target, 1, 0, 1000, options,
                                      L::DriverGroup::IO_INPUT_CONFIGURATION),
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
        for (unsigned i = 0; i < 4 && i < value.width; ++i)
            terminal.data[i] = uint8_t(value.raw >> (8 * i));
        if (!checked(L::advanceDriver(context, terminal, now + 4), "advanceDriver"))
            return 1;
        now += 10;
    }
    L::PreparedRead done;
    if (!checked(L::nextDriver(context, now, done), "terminal nextDriver") || done.ready)
        return 2;
    L::IoObservation observed;
    if (!checked(L::getIo(context, target, now, 1000, observed), "getIo"))
        return 1;
    std::printf("Raw count=%u, polarity=0x%04x; DI0=%u undefined, DI1=%u origin, DI2=%u unknown\n",
                unsigned(observed.ioEntries), unsigned(observed.inputPolarityRaw),
                unsigned(observed.inputFunctionRaw[0]), unsigned(observed.inputFunctionRaw[1]),
                unsigned(observed.inputFunctionRaw[2]));
    std::printf("Acquisition interval=[%" PRIu64 ", %" PRIu64 "] us; skew=%" PRIu64 " us\n",
                observed.window.earliestUs, observed.window.latestUs, observed.window.skewUs);
    std::puts("Assignment and polarity establish neither wiring nor electrical levels or method "
              "prerequisites.");
    std::puts("This example performs no configuration writes, output actions, enabling or motion.");
    // Explicit illustrative nameplate/exclusion premises, not identity detection.
    L::SettingsReservation reservation;
    reservation.target = target;
    reservation.model = L::SettingsModel::CL86_C;
    reservation.owner = reservation.sequence = reservation.generation = 1;
    L::IoSettings candidate;
    candidate.invertedInputs = observed.inputPolarityRaw & L::INPUT_POLARITY_BITS;
    for (unsigned i = 0; i < 7; ++i)
        candidate.functions[i] = observed.inputFunctions[i];
    candidate.functions[2] = L::InputFunction::USER2; // Explicit replacement for raw unknown99.
    L::DriverSettingsContext settings;
    if (!checked(L::prepareIoSettings(settings, reservation, context, candidate, 2, now, now + 1000,
                                      options, 1000),
                 "prepareIoSettings") ||
        !checked(L::nextDriver(settings, reservation, target.channel.binding, now, done),
                 "settings nextDriver") ||
        !done.ready || done.transfer.index != 0x2400 || done.transfer.subindex != 4 ||
        done.transfer.direction != M::CANOPEN::SdoDirection::DOWNLOAD)
        return 3;
    std::puts("Preview only: explicit DI2 assignment prepared; withdrawal before admission leaves "
              "configuration unchanged.");
    if (!checked(L::advanceDriver(settings, M::ReadOutcome::CANCELLED, now), "withdraw settings") ||
        settings.effectsPossible || settings.uncertain)
        return 4;
    return context.read.state == M::ReadState::SUCCEEDED &&
                   observed.inputFunctions[0] == L::InputFunction::UNDEFINED &&
                   observed.inputFunctions[1] == L::InputFunction::ORIGIN &&
                   observed.inputFunctions[2] == L::InputFunction::UNKNOWN &&
                   !observed.wiringKnown && !observed.electricalLevelsKnown
               ? 0
               : 2;
}
