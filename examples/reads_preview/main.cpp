// SPDX-License-Identifier: MIT
#include <MotorControlCAN/profiles/lichuan_c/Reads.h>
#include <cinttypes>
#include <cstdio>

namespace Core = MotorControlCAN;
namespace Reads = MotorControlCAN::LICHUAN_C;
namespace Service = MotorControlCAN::CANOPEN;

namespace {
bool checked(Core::Status status, const char* operation) {
    if (status)
        return true;
    std::fprintf(stderr, "%s failed: %s (%" PRId32 "): %s\n", operation,
                 Core::errToString(status.code), status.detail, status.msg);
    return false;
}

struct SyntheticValue {
    uint16_t index;
    uint8_t subindex;
    uint8_t width;
    uint8_t bytes[4];
};

// Independent fixture bytes, not captured traffic or manufacturer identity values.
const SyntheticValue identity[] = {
    {0x1000, 0, 4, {0x78, 0x56, 0x34, 0x12}}, {0x1009, 0, 2, {0x34, 0x12, 0, 0}},
    {0x100a, 0, 2, {0xcd, 0xab, 0, 0}},       {0x1018, 0, 1, {3, 0, 0, 0}},
    {0x1018, 1, 4, {0x44, 0x33, 0x22, 0x11}}, {0x1018, 2, 4, {0x88, 0x77, 0x66, 0x55}},
    {0x1018, 3, 4, {0xcc, 0xbb, 0xaa, 0x99}}, {0x2000, 0, 2, {0x57, 0x13, 0, 0}},
    {0x2001, 0, 2, {0x68, 0x24, 0, 0}},
};

Reads::ReadTarget syntheticTarget() {
    Reads::ReadTarget target;
    auto& binding = target.channel.binding;
    binding.logicalAxisId = 9;
    binding.endpoint.protocol = Core::Protocol::CANOPEN;
    binding.endpoint.profile = 1;
    binding.endpoint.address = 17;
    binding.bindingGeneration = 2;
    binding.configurationGeneration = 3;
    target.channel.clientToServerId = 0x651;
    target.channel.serverToClientId = 0x5d1;
    target.nodeGeneration = 4;
    return target;
}
} // namespace

int main() {
    std::puts("Offline synthetic read lifecycle; no controller, stack or clock is opened.");
    std::puts(
        "Route, epochs, time, values and qualified service terminals below are fixture premises.");
    const Reads::ReadTarget target = syntheticTarget();
    Reads::ReadOptions options;
    options.queueTimeoutUs = 100;
    options.responseTimeoutUs = 100;

    Reads::ReadContext probe;
    const Core::Status probeStatus = Reads::prepareProbe(probe, target, 1, 0, 1000, options);
    if (probeStatus.code != Core::Err::UNSUPPORTED ||
        probeStatus.detail != static_cast<int32_t>(Core::ReadError::READ_EFFECT))
        return 2;
    std::printf("Probe: %s (%" PRId32 "): %s\n", Core::errToString(probeStatus.code),
                probeStatus.detail, probeStatus.msg);

    Reads::ReadContext context;
    if (!checked(Reads::prepareIdentity(context, target, 2, 0, 1000, options),
                 "identity preparation"))
        return 1;

    uint64_t nowUs = 0;
    for (const auto& value : identity) {
        Reads::PreparedRead next;
        if (!checked(Reads::nextRead(context, nowUs, next), "next identity field"))
            return 1;
        if (!next.ready || next.transfer.index != value.index ||
            next.transfer.subindex != value.subindex || next.transfer.size != value.width ||
            next.transfer.direction != Service::SdoDirection::UPLOAD)
            return 2;

        // A real application receives this terminal from its sole qualified service owner.
        Service::SdoCompletion terminal;
        terminal.transfer = next.transfer;
        terminal.outcome = Service::SdoOutcome::SUCCESS;
        terminal.nodeGeneration = target.nodeGeneration;
        terminal.admittedUs = nowUs;
        terminal.completedUs = nowUs + 3;
        terminal.queued = true;
        terminal.transmissionPossible = true;
        terminal.protocolAcknowledged = true;
        terminal.effectPossible = next.transfer.readEffect == Service::SdoReadEffect::UNQUALIFIED;
        terminal.effectUncertain = false;
        terminal.size = value.width;
        for (unsigned i = 0; i < value.width; ++i)
            terminal.data[i] = value.bytes[i];
        if (!checked(Reads::advanceRead(context, terminal, nowUs + 4), "synthetic completion"))
            return 1;
        nowUs += 10;
    }

    Reads::PreparedRead finished;
    if (!checked(Reads::nextRead(context, nowUs, finished), "terminal read") || finished.ready)
        return 2;
    Reads::IdentityObservation observed;
    if (!checked(Reads::getIdentity(context, target, nowUs, 1000, observed),
                 "identity publication"))
        return 1;

    std::printf("Nine uploads: raw hardware U16=0x%04X, software U16=0x%04X, "
                "model U16=0x%04X, firmware U16=0x%04X\n",
                static_cast<unsigned>(observed.hardwareVersion),
                static_cast<unsigned>(observed.softwareVersion),
                static_cast<unsigned>(observed.modelCode),
                static_cast<unsigned>(observed.firmwareCode));
    std::printf("Conservative supplied interval: [%" PRIu64 ", %" PRIu64 "] us; skew=%" PRIu64
                " us; modelMatched=%s\n",
                observed.window.earliestUs, observed.window.latestUs, observed.window.skewUs,
                observed.modelMatched ? "true" : "false");
    std::puts("Raw success does not establish model applicability, an atomic snapshot or harmless "
              "read effects.");
    return context.state == Core::ReadState::SUCCEEDED && !observed.modelMatched ? 0 : 2;
}
