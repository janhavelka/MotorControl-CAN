// SPDX-License-Identifier: MIT
#include "FirmwareSmoke.h"
#include <cstdarg>
#include <cstdio>

namespace MotorControlCANExample {
namespace C = MotorControlCAN::CANOPEN;
namespace L = MotorControlCAN::LICHUAN_C;
namespace {
const char* transceiverName(TransceiverState state) {
    switch (state) {
    case TransceiverState::NOT_CONTROLLED:
        return "uncontrolled";
    case TransceiverState::SILENT_REQUESTED:
        return "silent-requested";
    case TransceiverState::NORMAL_REQUESTED:
        return "normal-requested";
    case TransceiverState::FAILED:
        return "failed";
    }
    return "unknown";
}
// The fixture premise: CiA 301 predefined connection set for the configured node.
L::ReadTarget smokeTarget(uint8_t node) {
    L::ReadTarget target;
    target.channel.binding.logicalAxisId = node;
    target.channel.binding.endpoint.profile = 1;
    target.channel.binding.endpoint.address = node;
    target.channel.binding.bindingGeneration = 1;
    target.channel.binding.configurationGeneration = 1;
    target.channel.clientToServerId = 0x600u + node;
    target.channel.serverToClientId = 0x580u + node;
    target.nodeGeneration = 1;
    return target;
}
} // namespace
FirmwareSmoke::FirmwareSmoke(const SmokeConfig& config) noexcept : config_(config) {}
FirmwareTurnHook FirmwareSmoke::hook() noexcept {
    FirmwareTurnHook result;
    result.context = this;
    result.turn = [](void* p, FirmwareProbe& firmware) {
        static_cast<FirmwareSmoke*>(p)->turn(firmware);
    };
    return result;
}
bool FirmwareSmoke::valid() const noexcept {
    const uint8_t a = config_.nodes[0], b = config_.nodes[1];
    return a >= 1 && a <= 127 && b >= 1 && b <= 127 && a != b && config_.readWindowUs &&
           config_.options.queueTimeoutUs && config_.options.responseTimeoutUs &&
           config_.identity;
}
void FirmwareSmoke::print(const char* format, ...) noexcept {
    va_list arguments;
    va_start(arguments, format);
    const int count = std::vsnprintf(line_, sizeof line_, format, arguments);
    va_end(arguments);
    // A truncated record is failed evidence, never a plausible shortened line.
    static const char failure[] = "SMOKE output-format-error\n";
    if (count < 0 || static_cast<size_t>(count) >= sizeof line_) {
        for (size_t i = 0; i < sizeof failure; ++i)
            line_[i] = failure[i];
        lineSize_ = sizeof failure - 1;
    } else {
        lineSize_ = static_cast<size_t>(count);
    }
}
void FirmwareSmoke::turn(FirmwareProbe& firmware) noexcept {
    // One retained line at a time: a full console delays the next step, never CAN service.
    if (lineSize_) {
        if (!firmware.platform().writeOutput(reinterpret_cast<const uint8_t*>(line_), lineSize_))
            return;
        lineSize_ = 0;
    }
    if (!announced_) {
        // Once per boot, before any input is examined: the identity lets a runner check the
        // image before it sends the trigger. Passive output only.
        announced_ = true;
        print("SMOKE ready %s\n", config_.identity ? config_.identity : "identity=unconfigured");
        return;
    }
    uint8_t byte = 0;
    if (step_ == SmokeStep::WAITING) {
        bool triggered = false;
        while (!triggered && firmware.platform().takeInput(byte)) {
            triggered = byte == TRIGGER;
            if (!triggered && ignored_ != UINT32_MAX)
                ++ignored_;
        }
        if (!triggered)
            return;
        if (!valid()) {
            step_ = SmokeStep::DONE;
            print("SMOKE refused: fixture nodes/budgets/identity not configured; no start\n");
            return;
        }
        const bool started = firmware.start();
        step_ = started ? SmokeStep::READING : SmokeStep::DONE;
        print("SMOKE image %s start=%s running=%u transceiver=%s check=%u\n", config_.identity,
              started ? "ok" : "failed", unsigned(firmware.running()),
              transceiverName(firmware.transceiver()), unsigned(firmware.lastCheck()));
        return;
    }
    while (firmware.platform().takeInput(byte))
        if (ignored_ != UINT32_MAX)
            ++ignored_;
    if (step_ == SmokeStep::READING)
        read(firmware);
    else if (step_ == SmokeStep::FINISH) {
        step_ = SmokeStep::DONE;
        print("SMOKE done; no further requests; ignored=%lu\n",
              static_cast<unsigned long>(ignored_));
    }
}
void FirmwareSmoke::read(FirmwareProbe& firmware) noexcept {
    ProbeApp& app = firmware.app();
    const uint64_t now = firmware.owner().lastTimeUs();
    const uint8_t node = config_.nodes[node_];
    if (!firmware.running()) {
        // A progression failure retired the controller: report and admit nothing more.
        step_ = SmokeStep::DONE;
        print("SMOKE node=%u stopped: controller not running failure=%u\n", unsigned(node),
              unsigned(firmware.failure()));
        return;
    }
    if (!requested_) {
        const L::ReadTarget target = smokeTarget(node);
        C::NetworkRoute route;
        route.binding = target.channel.binding;
        route.generation = target.nodeGeneration;
        route.heartbeatId = 0x700u + node;
        route.emcyId = 0x80u + node;
        // Explicit premise: an exclusive, initially unused server session. Not confirmation.
        const bool admitted =
            firmware.services().configure(node_, target.channel, false, true) &&
            firmware.services().configureNetwork(node_, route, C::NetworkObserver(), now) &&
            app.select(target) &&
            app.requestIdentity(now, now + config_.readWindowUs, config_.options);
        if (!admitted) {
            print("SMOKE node=%u identity=refused; no request\n", unsigned(node));
            if (node_ < 1)
                ++node_;
            else
                step_ = SmokeStep::FINISH;
            return;
        }
        requested_ = true;
        return;
    }
    // ProbeApp admits each field during service; this consumes the retained terminal.
    if (app.pendingRequest().owner && firmware.owner().result(app.pendingRequest()))
        app.advanceRead(now);
    const L::ReadContext& context = app.readContext();
    if (context.state == MotorControlCAN::ReadState::ACTIVE)
        return;
    L::IdentityObservation identity;
    if (context.state == MotorControlCAN::ReadState::SUCCEEDED &&
        app.getIdentity(now, config_.readWindowUs, identity))
        print("SMOKE node=%u identity=ok device=%08lx hw=%04x/%u sw=%04x/%u entries=%u "
              "vendor=%08lx product=%08lx revision=%08lx model=%04x firmware=%04x skew=%llu\n",
              unsigned(node), static_cast<unsigned long>(identity.deviceType),
              unsigned(identity.hardwareVersion), unsigned(identity.hardwareVersionWidth),
              unsigned(identity.softwareVersion), unsigned(identity.softwareVersionWidth),
              unsigned(identity.identityEntries), static_cast<unsigned long>(identity.vendorId),
              static_cast<unsigned long>(identity.productCode),
              static_cast<unsigned long>(identity.revision), unsigned(identity.modelCode),
              unsigned(identity.firmwareCode),
              static_cast<unsigned long long>(identity.window.skewUs));
    else
        print("SMOKE node=%u identity=failed state=%u outcome=%u uncertain=%u\n", unsigned(node),
              unsigned(context.state), unsigned(context.outcome), unsigned(context.uncertain));
    // Releases local history only; uncertainty and quarantine stay with the services.
    app.releaseRead();
    requested_ = false;
    if (node_ < 1)
        ++node_;
    else
        step_ = SmokeStep::FINISH;
}
} // namespace MotorControlCANExample
