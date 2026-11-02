// SPDX-License-Identifier: MIT
#include "FakeTransport.h"
#include "ProbeApp.h"
#include <cstdio>
#include <cstring>

namespace M = MotorControlCAN;
namespace C = MotorControlCAN::CANOPEN;
namespace L = MotorControlCAN::LICHUAN_C;
namespace E = MotorControlCANExample;
namespace O = MotorControlCANOwner;
namespace {
bool clockUs(void* context, uint64_t& output) {
    output = static_cast<E::FakeTransport*>(context)->nowUs();
    return true;
}
// The application explicitly inspects each retained owner terminal twice,
// advances the concrete typed context, then releases the owner result slot.
bool finishRead(E::ProbeApp& app, O::CanBusOwner& owner, E::FakeTransport& fake,
                E::ProbePlatform& platform) {
    for (unsigned turn = 0; turn < 100 && app.readContext().state == M::ReadState::ACTIVE; ++turn) {
        if (!fake.setTime(fake.nowUs() + 1))
            return false;
        uint64_t now = 0;
        if (!platform.nowUs(now) || !app.service(now))
            return false;
        const O::RequestId pending = app.pendingRequest();
        const O::Completion* terminal = owner.result(pending);
        if (!terminal)
            continue;
        if (terminal != owner.result(pending) || terminal->reason != O::CompletionReason::SERVICE ||
            terminal->service.outcome != C::SdoOutcome::SUCCESS || !app.advanceRead(now) ||
            owner.result(pending))
            return false;
    }
    return app.readContext().state == M::ReadState::SUCCEEDED;
}
} // namespace
int main(int argc, char** argv) {
    if (argc != 2 || std::strcmp(argv[1], "--fake")) {
        std::fputs("Usage: native_probe --fake (synthetic only)\n", stderr);
        return 2;
    }
    const L::ReadTarget target = E::FakeTransport::target();
    C::NetworkRoute network;
    network.binding = target.channel.binding;
    network.generation = target.nodeGeneration;
    network.heartbeatId = 0x705;
    network.emcyId = 0x85;
    C::SdoClient services;
    // These true flags are explicit synthetic fixture premises, never a startup
    // qualification procedure for hardware. Unknown read effects/Q20 remain open.
    if (!services.configure(0, target.channel, true, true) ||
        !services.configureNetwork(0, network, C::NetworkObserver(), 0))
        return 1;
    E::FakeTransport fake;
    O::CanBusOwner owner(services, fake.callbacks(), 26);
    E::ProbeCallbacks callbacks;
    callbacks.context = &fake;
    callbacks.nowUs = clockUs;
    E::ProbePlatform platform(callbacks);
    E::ProbeApp app(owner, platform);
    O::Health health;
    if (!app.select(target) || !app.health(health) || fake.transmitCalls())
        return 1;
    L::ReadOptions options;
    options.queueTimeoutUs = options.responseTimeoutUs = 100;
    if (!app.requestIdentity(0, 1000, options) || fake.transmitCalls() ||
        !finishRead(app, owner, fake, platform))
        return 1;
    L::IdentityObservation identity;
    if (!app.getIdentity(fake.nowUs(), 1000, identity) || identity.modelMatched ||
        !app.releaseRead())
        return 1;
    if (!app.requestConfig(fake.nowUs(), fake.nowUs() + 1000, options) ||
        !finishRead(app, owner, fake, platform))
        return 1;
    L::ConfigObservation config;
    if (!app.getConfig(fake.nowUs(), 1000, config) || !app.releaseRead() ||
        fake.transmitted() != 15 || services.node(0)->emcyCount != 15 ||
        !services.node(0)->heartbeat.present || !owner.stats().unmatchedFrames)
        return 1;
    std::puts("FAKE ONLY: externally fresh/usable session premise; Q20 remains unresolved.");
    std::printf("Synthetic identity: 9 fields, device type 0x%08lx, modelMatched=false.\n",
                static_cast<unsigned long>(identity.deviceType));
    std::printf("Synthetic config: 6 fields, producer %u ms; routes observed only.\n",
                unsigned(config.producerTimeMs));
    std::printf("15 explicit uploads; heartbeat/EMCY interleaved; %lu unrelated frames.\n",
                static_cast<unsigned long>(owner.stats().unmatchedFrames));
    std::printf("Native storage: fake %lu, service %lu, owner %lu, app %lu, platform %lu bytes.\n",
                static_cast<unsigned long>(sizeof fake),
                static_cast<unsigned long>(sizeof services),
                static_cast<unsigned long>(sizeof owner), static_cast<unsigned long>(sizeof app),
                static_cast<unsigned long>(sizeof platform));
    std::puts("All owner terminals inspected and released; no hardware qualification.");
    return 0;
}
