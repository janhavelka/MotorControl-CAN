// SPDX-License-Identifier: MIT
#include "DiscoveryScan.h"
#include "FakeTransport.h"
#include <cstdio>

namespace M = MotorControlCAN;
namespace C = MotorControlCAN::CANOPEN;
namespace E = MotorControlCANExample;
namespace O = MotorControlCANOwner;

int main() {
    C::SdoClient services;
    const M::LICHUAN_C::ReadTarget target = E::FakeTransport::target();
    C::NetworkRoute network;
    network.binding = target.channel.binding;
    network.generation = target.nodeGeneration;
    network.heartbeatId = 0x705;
    network.emcyId = 0x85;
    // Explicit synthetic fresh-session/exclusive-server premises. A real controller
    // application must establish its own route and old-response exclusion evidence.
    if (!services.configure(0, target.channel, true, true) ||
        !services.configureNetwork(0, network, C::NetworkObserver(), 0))
        return 1;
    E::FakeTransport fake;
    O::CanBusOwner owner(services, fake.callbacks(), 37);
    E::ProbePlatform platform{E::ProbeCallbacks()};
    E::ProbeApp app(owner, platform);
    E::DiscoveryScan scan(app, owner, services);
    E::DiscoveryInterface actual;
    actual.bitrate = 500000; // Fixture policy, never a detected or programmed bitrate.
    E::DiscoveryPlan plan;
    plan.count = 1;
    plan.targets[0].target = target;
    plan.interface = actual;
    plan.mode = E::DiscoveryMode::PROBE;
    plan.maximumServices = 1;
    plan.maximumAgeUs = 100;
    plan.options.queueTimeoutUs = plan.options.responseTimeoutUs = 20;
    plan.serviceBudget.rx = plan.serviceBudget.events = 8;
    plan.serviceBudget.tx = plan.serviceBudget.starts = 1;
    if (!scan.begin(plan, actual, 0, 100) || fake.transmitCalls())
        return 1;
    for (uint64_t now = 1; scan.active() && now <= 100; ++now)
        if (!fake.setTime(now) || !scan.service(actual, now))
            return 1;
    const E::DiscoveryFinding* found = scan.result(0);
    if (!scan.complete() || !found || found->outcome != E::DiscoveryOutcome::PRESENCE ||
        found->confidence != E::DiscoveryConfidence::RESPONDER_ONLY || fake.transmitted() != 1)
        return 1;
    std::printf("Synthetic node 5 responded: 1000:00 = 0x%08lx.\n",
                static_cast<unsigned long>(found->read.values[0].raw));
    std::printf("%u upload, %u occupied slot, %u free slot; no model or firmware match.\n",
                unsigned(scan.summary().servicesAdmitted),
                unsigned(scan.summary().occupiedSlotsAtStart),
                unsigned(scan.summary().freeSlotsAtStart));
    std::puts("FAKE ONLY: read effects remain unknown; no hardware qualification.");
    return scan.release() ? 0 : 1;
}
