// SPDX-License-Identifier: MIT
#include <MotorControlCAN/profiles/lichuan_c/State.h>
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

    // One upload of 1000:00 on the owner's accepted list. Acceptance is recorded risk, so the
    // terminal still reports a possible effect. These bytes are the same synthetic value as below.
    Reads::ReadContext probe;
    if (!checked(Reads::prepareProbe(probe, target, 1, 0, 1000, options), "probe preparation"))
        return 1;
    Reads::PreparedRead probeWork;
    if (!checked(Reads::nextRead(probe, 0, probeWork), "probe work") || !probeWork.ready ||
        probeWork.transfer.index != 0x1000 || probeWork.transfer.subindex != 0 ||
        probeWork.transfer.readEffect != Service::SdoReadEffect::ACCEPTED_UNQUALIFIED)
        return 2;
    Service::SdoCompletion probeTerminal;
    probeTerminal.transfer = probeWork.transfer;
    probeTerminal.outcome = Service::SdoOutcome::SUCCESS;
    probeTerminal.nodeGeneration = target.nodeGeneration;
    probeTerminal.completedUs = 3;
    probeTerminal.queued = probeTerminal.transmissionPossible = true;
    probeTerminal.protocolAcknowledged = probeTerminal.effectPossible = true;
    probeTerminal.size = identity[0].width;
    for (unsigned i = 0; i < identity[0].width; ++i)
        probeTerminal.data[i] = identity[0].bytes[i];
    if (!checked(Reads::advanceRead(probe, probeTerminal, 4), "probe completion") ||
        probe.state != Core::ReadState::SUCCEEDED || !probe.evidence.effectPossible)
        return 2;
    std::printf("Probe: 1000:00 raw U32=0x%08" PRIX32 "; effect possible=%s\n", probe.values[0].raw,
                probe.evidence.effectPossible ? "true" : "false");

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
        terminal.effectPossible = next.transfer.readEffect != Service::SdoReadEffect::REVIEWED_NON_CHANGING;
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
    if (context.state != Core::ReadState::SUCCEEDED || observed.modelMatched)
        return 2;

    Reads::ReadContext state;
    const uint8_t blocks = (1u << static_cast<unsigned>(Reads::StateBlock::STATUS)) |
                           (1u << static_cast<unsigned>(Reads::StateBlock::MONITORS));
    if (!checked(Reads::prepareState(state, target, 3, nowUs, 1000, options, blocks),
                 "state preparation"))
        return 1;
    // These independent bytes deliberately disagree between the selected EN86
    // alarm bit and monitor alarm bit. A document comparison selects no winner.
    for (unsigned i = 0; i < 5; ++i) {
        Reads::PreparedRead next;
        if (!checked(Reads::nextRead(state, nowUs, next), "next state field") || !next.ready)
            return 1;
        Service::SdoCompletion terminal;
        terminal.transfer = next.transfer;
        terminal.outcome = Service::SdoOutcome::SUCCESS;
        terminal.nodeGeneration = target.nodeGeneration;
        terminal.admittedUs = nowUs;
        terminal.completedUs = nowUs + 3;
        terminal.queued = terminal.transmissionPossible = terminal.protocolAcknowledged = true;
        terminal.effectPossible = true;
        terminal.size = next.transfer.size;
        if (next.transfer.index == 0x6041) { terminal.data[0] = 0x87; terminal.data[1] = 4; }
        if (next.transfer.index == 0x6061) terminal.data[0] = 1;
        if (!checked(Reads::advanceRead(state, terminal, nowUs + 4), "state completion"))
            return 1;
        nowUs += 10;
    }
    Reads::StateInterpretation interpreted;
    if (!checked(Reads::getStateInterpretation(state, target, nowUs, 1000,
                                              Reads::StatusLayout::EN86, interpreted),
                 "state interpretation"))
        return 1;
    std::printf("EN86 document comparison: statusword=0x%04X, monitor=0x%04X, "
                "alarm disagreement=%s, skew=%" PRIu64 " us\n",
                static_cast<unsigned>(interpreted.statusword),
                static_cast<unsigned>(interpreted.monitor),
                interpreted.alarmDisagreement ? "true" : "false", interpreted.window.skewUs);
    // The same meanings from the raw word alone, as a caller with only a STATUS block or a
    // qualified TPDO sample would decode them. Its route and freshness remain its own to prove.
    Reads::StatusIndications decoded;
    if (!checked(Reads::interpretStatusword(interpreted.statusword, Reads::StatusLayout::EN86, decoded),
                 "statusword decode"))
        return 1;
    std::printf("EN86 word alone: running=%s, unlisted=0x%04X\n",
                decoded.running == Reads::StateIndication::YES ? "yes" : "no",
                static_cast<unsigned>(decoded.unlistedStatusBits));
    std::puts("The comparison qualifies no firmware, action admission, torque or physical safety.");
    return interpreted.alarmDisagreement && interpreted.running == Reads::StateIndication::NO &&
                   interpreted.mode == Reads::ObservedMode::PROFILE_POSITION
               ? 0 : 2;
}
