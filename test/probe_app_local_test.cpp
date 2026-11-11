// SPDX-License-Identifier: MIT
// Offline application tests use the same public arithmetic and owner as firmware.
#include "FakeTransport.h"
#include "ProbeApp.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace M = MotorControlCAN;
namespace E = MotorControlCANExample;
namespace O = MotorControlCANOwner;
namespace {
unsigned checks = 0;
#define CHECK(condition)                                                                           \
    do {                                                                                           \
        ++checks;                                                                                  \
        if (!(condition)) {                                                                        \
            std::fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #condition);                   \
            std::abort();                                                                          \
        }                                                                                          \
    } while (false)
template <class T> struct Saved {
    unsigned char bytes[sizeof(T)];
    explicit Saved(const T& value) { std::memcpy(bytes, &value, sizeof value); }
    void unchanged(const T& value) const { CHECK(!std::memcmp(bytes, &value, sizeof value)); }
};
struct Rig {
    M::CANOPEN::SdoClient services;
    E::FakeTransport fake;
    E::ProbePlatform platform;
    O::CanBusOwner owner;
    E::ProbeApp app;
    Rig()
        : platform(E::ProbeCallbacks()), owner(services, fake.callbacks(), 177),
          app(owner, platform) {}
    void noTraffic() const { CHECK(!fake.transmitCalls() && !fake.receiveCalls()); }
};
E::HostAxisCommand field(E::HostAxisField which, int64_t numerator, uint64_t denominator = 1) {
    E::HostAxisCommand command;
    command.kind = E::HostAxisCommandKind::CONFIGURE;
    command.field = which;
    command.value = M::Rational(numerator, denominator);
    return command;
}
M::AxisReference witness(const E::ProbeApp& app) {
    M::AxisReference evidence;
    evidence.binding = app.axisConfig()->binding;
    evidence.referenceGeneration = app.axisConfig()->referenceGeneration;
    evidence.idle = evidence.stationary = evidence.nativeKnown = true;
    evidence.nativePosition = 1000;
    evidence.source = M::ScaleSource::QUALIFIED;
    evidence.observedUs = evidence.nowUs = 10;
    evidence.maximumAgeUs = 5;
    return evidence;
}
void initializationAndAtomicFailure() {
    Rig rig;
    CHECK(!rig.app.axisConfig() && !rig.app.axisReference());
    E::HostAxisView view;
    E::HostAxisCommand query;
    CHECK(rig.app.executeHostAxis(query, 0, view) && !view.configured && view.offline);
    const Saved<E::HostAxisView> oldView(view);
    auto bad = field(E::HostAxisField::COMMAND_SCALE, -1);
    CHECK(!rig.app.executeHostAxis(bad, 0, view));
    oldView.unchanged(view);
    CHECK(!rig.app.axisConfig());
    CHECK(rig.app.initializeOfflineAxis());
    CHECK(!rig.app.initializeOfflineAxis());
    CHECK(!rig.app.axisConfig()->units.commandStepsPerMotorTurn.numerator);
    CHECK(!rig.app.axisConfig()->originKnown && !rig.app.axisReference());
    const Saved<M::AxisConfig> oldConfig(*rig.app.axisConfig());
    CHECK(!rig.app.executeHostAxis(bad, 0, view));
    oldConfig.unchanged(*rig.app.axisConfig());
    E::HostAxisCommand origin;
    origin.kind = E::HostAxisCommandKind::ORIGIN;
    origin.value = M::Rational(0, 1);
    CHECK(!rig.app.executeHostAxis(origin, 0, view));
    oldConfig.unchanged(*rig.app.axisConfig());
    M::PreparedTarget prepared;
    prepared.effectiveNative = 97;
    const Saved<M::PreparedTarget> oldPrepared(prepared);
    M::PositionRequest request;
    request.relative = true;
    request.configurationGeneration = rig.app.axisConfig()->binding.configurationGeneration;
    CHECK(!rig.app.previewPosition(request, 0, prepared));
    oldPrepared.unchanged(prepared);
    request.relative = false;
    request.value = M::Rational(INT64_C(9007199254740993), 1);
    CHECK(rig.app.previewPosition(request, 0, prepared));
    CHECK(prepared.effectiveNative == INT64_C(9007199254740993));
    rig.noTraffic();
}
void offlineFacts() {
    Rig rig;
    const auto selected = E::FakeTransport::target();
    CHECK(rig.services.configure(0, selected.channel, true, true));
    M::CANOPEN::NetworkRoute network;
    network.binding = selected.channel.binding;
    network.generation = selected.nodeGeneration;
    network.heartbeatId = 0x705;
    network.emcyId = 0x85;
    CHECK(rig.services.configureNetwork(0, network, M::CANOPEN::NetworkObserver(), 0));
    CHECK(rig.app.select(selected));
    const Saved<M::LICHUAN_C::ReadTarget> originalSelection(*rig.app.selected());
    E::HostAxisView view;
    CHECK(rig.app.executeHostAxis(field(E::HostAxisField::COMMAND_SCALE, 2000), 0, view));
    CHECK(view.configured && view.offline && !view.referenceKnown);
    CHECK(view.configuration.binding.endpoint.protocol == M::Protocol::PROPRIETARY);
    CHECK(view.configuration.units.commandStepsPerMotorTurn.source == M::ScaleSource::ASSUMED);
    const E::HostAxisField scales[] = {E::HostAxisField::GEAR, E::HostAxisField::FULL_STEP_SCALE,
                                       E::HostAxisField::LEAD};
    for (auto which : scales)
        CHECK(rig.app.executeHostAxis(field(which, 2), 0, view));
    CHECK(rig.app.executeHostAxis(field(E::HostAxisField::ENCODER_ID, 42), 0, view));
    CHECK(rig.app.executeHostAxis(field(E::HostAxisField::ENCODER_SCALE, 4000), 0, view));
    E::HostAxisCommand basis = field(E::HostAxisField::ENCODER_BASIS, 0);
    basis.encoderBasis = M::EncoderBasis::LOAD_TURN;
    CHECK(rig.app.executeHostAxis(basis, 0, view));
    CHECK(rig.app.executeHostAxis(field(E::HostAxisField::ENCODER_POLARITY, -1), 0, view));
    CHECK(rig.app.executeHostAxis(field(E::HostAxisField::POLARITY, -1), 0, view));
    CHECK(rig.app.executeHostAxis(field(E::HostAxisField::RELATIVE_BASES, 7), 0, view));
    E::HostAxisCommand limits = field(E::HostAxisField::NATIVE_LIMITS, -2000);
    limits.secondValue = M::Rational(2000, 1);
    CHECK(rig.app.executeHostAxis(limits, 0, view));
    limits.field = E::HostAxisField::SOFT_LIMITS;
    limits.value = M::Rational(-100, 1);
    limits.secondValue = M::Rational(100, 1);
    CHECK(rig.app.executeHostAxis(limits, 0, view) && view.configuration.softLimitsKnown);
    auto reference = witness(rig.app); // Outside soft limits remains a valid position witness.
    CHECK(rig.app.setHostAxisReference(reference));
    auto clear = field(E::HostAxisField::GEAR, 0);
    clear.clear = true;
    const uint32_t generation = view.configuration.binding.configurationGeneration;
    CHECK(rig.app.executeHostAxis(clear, 10, view));
    CHECK(view.configuration.binding.configurationGeneration == generation + 1);
    CHECK(!view.configuration.softLimitsKnown && !view.configuration.originKnown);
    CHECK(!view.referenceKnown && !rig.app.axisReference());
    CHECK(!rig.app.setHostAxisReference(reference)); // Old epochs cannot be reused.
    const Saved<M::AxisConfig> old(*rig.app.axisConfig());
    auto invalid = field(E::HostAxisField::RELATIVE_BASES, 8);
    CHECK(!rig.app.executeHostAxis(invalid, 10, view));
    old.unchanged(*rig.app.axisConfig());
    originalSelection.unchanged(*rig.app.selected());
    CHECK(!M::sameBinding(rig.app.axisConfig()->binding, rig.app.selected()->channel.binding));
    rig.noTraffic();
}
void checkedEvidenceAndPreview() {
    Rig rig;
    E::HostAxisView view;
    CHECK(rig.app.executeHostAxis(field(E::HostAxisField::COMMAND_SCALE, 2000), 0, view));
    CHECK(rig.app.executeHostAxis(field(E::HostAxisField::RELATIVE_BASES, 1), 0, view));
    M::AxisReference reference = witness(rig.app);
    CHECK(rig.app.setHostAxisReference(reference));
    M::AxisConfig candidate = *rig.app.axisConfig();
    M::AxisReference moving = reference;
    moving.stationary = false;
    const Saved<M::AxisConfig> old(candidate);
    CHECK(!rig.app.configureHostAxis(candidate, moving));
    old.unchanged(*rig.app.axisConfig());
    CHECK(!rig.app.setHostAxisOrigin(1000, moving));
    E::HostAxisCommand origin;
    origin.kind = E::HostAxisCommandKind::ORIGIN;
    origin.value = M::Rational(1000, 1);
    CHECK(rig.app.executeHostAxis(origin, 10, view));
    CHECK(view.configuration.originKnown);
    CHECK(rig.app.axisReference()->observedUs == 10);
    E::HostAxisCommand prepare;
    prepare.kind = E::HostAxisCommandKind::PREPARE;
    prepare.position.value = M::Rational(1, 2);
    prepare.position.unit = M::PositionUnit::TURNS;
    prepare.position.frame = M::CoordinateFrame::MOTOR;
    CHECK(rig.app.executeHostAxis(prepare, 10, view)); // Absolute arithmetic needs no witness.
    CHECK(view.prepared.effectiveNative == 2000 && !view.prepared.referenceUsed);
    prepare.position.relative = true;
    CHECK(rig.app.executeHostAxis(prepare, 15, view)); // Inclusive age boundary.
    CHECK(view.prepared.endpointNative == 2000 && view.prepared.referenceUsed);
    const Saved<E::HostAxisView> oldView(view);
    CHECK(!rig.app.executeHostAxis(prepare, 16, view));
    oldView.unchanged(view);
    CHECK(rig.app.axisReference()->observedUs == 10 && rig.app.axisReference()->nowUs == 10);
    prepare.position.configurationGeneration =
        rig.app.axisConfig()->binding.configurationGeneration;
    M::PreparedTarget target;
    CHECK(!rig.app.previewPosition(prepare.position, 9, target));
    reference = *rig.app.axisReference();
    candidate = *rig.app.axisConfig();
    CHECK(rig.app.configureHostAxis(candidate, reference));
    CHECK(rig.app.axisReference()->observedUs == 10);
    rig.noTraffic();
}
void hostFactsAndEpochExhaustion() {
    Rig rig;
    E::HostInterfaceFacts facts;
    CHECK(!rig.app.hostInterfaceFacts().interfaceKnown);
    facts.interfaceKnown = facts.bitrateKnown = true;
    facts.interfaceId = 2;
    CHECK(!rig.app.setHostInterfaceFacts(facts));
    facts.bitrateBitsPerSecond = 500000;
    CHECK(rig.app.setHostInterfaceFacts(facts));
    CHECK(rig.app.hostInterfaceFacts().bitrateBitsPerSecond == 500000);
    E::HostWiringUse use = E::HostWiringUse::CONNECTED;
    CHECK(!rig.app.hostWiring(10, use) && use == E::HostWiringUse::CONNECTED);
    for (uint8_t terminal = 0; terminal < 10; ++terminal) {
        CHECK(rig.app.hostWiring(terminal, use) && use == E::HostWiringUse::UNKNOWN);
        CHECK(rig.app.setHostWiring(terminal, E::HostWiringUse::UNUSED));
        CHECK(rig.app.hostWiring(terminal, use) && use == E::HostWiringUse::UNUSED);
    }
    CHECK(!rig.app.setHostWiring(10, E::HostWiringUse::CONNECTED));
    CHECK(!rig.app.setHostWiring(0, static_cast<E::HostWiringUse>(3)));
    M::AxisConfig config;
    config.binding.logicalAxisId = config.binding.endpoint.profile = 1;
    config.binding.bindingGeneration = 1;
    config.binding.configurationGeneration = UINT32_MAX;
    CHECK(rig.app.establishHostAxis(config));
    const Saved<M::AxisConfig> old(*rig.app.axisConfig());
    E::HostAxisView view;
    CHECK(!rig.app.executeHostAxis(field(E::HostAxisField::COMMAND_SCALE, 2000), 0, view));
    old.unchanged(*rig.app.axisConfig());
    rig.noTraffic();
}
} // namespace
int main() {
    initializationAndAtomicFailure();
    offlineFacts();
    checkedEvidenceAndPreview();
    hostFactsAndEpochExhaustion();
}
