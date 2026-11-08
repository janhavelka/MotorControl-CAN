// SPDX-License-Identifier: MIT
// Literal identity objects and frames are host fixtures, not drive qualification.
#include "SdoClient.h"
#include <MotorControlCAN/Discovery.h>
#include <MotorControlCAN/profiles/lichuan_c/Reads.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <new>

namespace M = MotorControlCAN;
namespace C = MotorControlCAN::CANOPEN;
namespace L = MotorControlCAN::LICHUAN_C;
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
template <typename T> struct Saved {
    unsigned char bytes[sizeof(T)];
    explicit Saved(const T& value) { std::memcpy(bytes, &value, sizeof value); }
    void unchanged(const T& value) const { CHECK(std::memcmp(bytes, &value, sizeof value) == 0); }
};
L::ReadTarget target() {
    L::ReadTarget result;
    result.channel.binding.logicalAxisId = 17;
    result.channel.binding.endpoint.bus = 2;
    result.channel.binding.endpoint.profile = 41;
    result.channel.binding.endpoint.address = 5;
    result.channel.binding.bindingGeneration = 3;
    result.channel.binding.configurationGeneration = 7;
    result.channel.clientToServerId = 0x625;
    result.channel.serverToClientId = 0x5a5;
    result.nodeGeneration = 11;
    return result;
}
L::ReadOptions options() {
    L::ReadOptions result;
    result.queueTimeoutUs = 100;
    result.responseTimeoutUs = 100;
    return result;
}
L::ReadContext probe() {
    L::ReadContext result;
    CHECK(L::prepareProbe(result, target(), 23, 0, 10000, options()));
    return result;
}
void inventory() {
    CHECK(M::discoveryProfileCount() == 1);
    M::DiscoveryProfile profile;
    CHECK(M::getDiscoveryProfile(0, profile));
    CHECK(profile.manufacturer == M::Manufacturer::LICHUAN);
    CHECK(profile.profile == M::DriveProfile::LICHUAN_C);
    CHECK(profile.protocol == M::Protocol::CANOPEN);
    CHECK(!std::strcmp(profile.manufacturerId, "lichuan"));
    CHECK(!std::strcmp(profile.manufacturerName, "Lichuan"));
    CHECK(!std::strcmp(profile.profileId, "lichuan_c"));
    CHECK(!std::strcmp(profile.profileName, "Lichuan C"));
    const Saved<M::DiscoveryProfile> saved(profile);
    CHECK(M::getDiscoveryProfile(1, profile).code == M::Err::UNSUPPORTED);
    saved.unchanged(profile);
    CHECK(M::getDiscoveryProfile(std::numeric_limits<std::size_t>::max(), profile).code ==
          M::Err::UNSUPPORTED);
    saved.unchanged(profile);
    M::DiscoveryCapabilities caps;
    CHECK(M::getDiscoveryCapabilities(M::DriveProfile::LICHUAN_C, caps));
    CHECK(caps.probe && caps.identity);
    CHECK(!caps.nonChanging && !caps.exactModel && !caps.firmware);
    CHECK(caps.minimumAddress == 1 && caps.maximumAddress == 127);
    CHECK(caps.probeIndex == 0x1000 && caps.probeSubindex == 0 && caps.probeWidthBytes == 4);
    const Saved<M::DiscoveryCapabilities> savedCaps(caps);
    CHECK(M::getDiscoveryCapabilities(static_cast<M::DriveProfile>(255), caps).code ==
          M::Err::UNSUPPORTED);
    savedCaps.unchanged(caps);
    // The application-assigned wire binding profile is independent of catalogue selectors.
    CHECK(target().channel.binding.endpoint.profile == 41);
}

struct Field {
    uint16_t index;
    uint8_t subindex, width;
    uint32_t value;
};
const Field minimal = {0x1000, 0, 4, 0x80040192};
const Field identity[] = {{0x1000, 0, 4, 0x40192}, {0x1009, 0, 2, 0x2608}, {0x100a, 0, 2, 1},
                          {0x1018, 0, 1, 3},       {0x1018, 1, 4, 0x1000}, {0x1018, 2, 4, 1},
                          {0x1018, 3, 4, 0},       {0x2000, 0, 2, 1},      {0x2001, 0, 2, 0}};
void configure(C::SdoClient& owner) {
    const L::ReadTarget current = target();
    CHECK(owner.configure(0, current.channel, true, true));
    C::NetworkRoute route;
    route.binding = current.channel.binding;
    route.generation = current.nodeGeneration;
    route.heartbeatId = 0x705;
    route.emcyId = 0x85;
    CHECK(owner.configureNetwork(0, route, C::NetworkObserver(), 0));
}
// Exercise the same service admission, CAN frames and retained terminal as ordinary reads.
void exchange(C::SdoClient& owner, L::ReadContext& context, const Field& expected,
              uint64_t admitted, bool abortReply = false) {
    L::PreparedRead work;
    CHECK(L::nextRead(context, admitted, work) && work.ready);
    CHECK(work.transfer.direction == C::SdoDirection::UPLOAD);
    CHECK(work.transfer.index == expected.index && work.transfer.subindex == expected.subindex);
    CHECK(expected.width == 1 || expected.width == 2 || expected.width == 4);
    CHECK(work.transfer.size == expected.width);
    CHECK(work.transfer.readEffect == C::SdoReadEffect::ACCEPTED_UNQUALIFIED);
    CHECK(work.transfer.channel.binding.endpoint.profile == 41);
    C::SdoRequestId request;
    CHECK(owner.admit(0, work.transfer, admitted, request));
    CHECK(owner.service(admitted));
    M::Frame tx;
    C::SdoTxId txId;
    CHECK(owner.nextTx(0, admitted, tx, txId));
    CHECK(tx.id == 0x625 && tx.dlc == 8 && tx.payloadLength == 8);
    CHECK(tx.data[0] == 0x40 && tx.data[1] == uint8_t(expected.index));
    CHECK(tx.data[2] == uint8_t(expected.index >> 8) && tx.data[3] == expected.subindex);
    for (unsigned i = 4; i < 8; ++i)
        CHECK(tx.data[i] == 0);
    CHECK(owner.canTransmit(txId, admitted));
    CHECK(owner.confirmTx(txId, admitted));
    M::Frame reply;
    reply.id = 0x5a5;
    reply.dlc = reply.payloadLength = 8;
    reply.data[0] = abortReply            ? 0x80
                    : expected.width == 1 ? 0x4f
                    : expected.width == 2 ? 0x4b
                                          : 0x43;
    reply.data[1] = uint8_t(expected.index);
    reply.data[2] = uint8_t(expected.index >> 8);
    reply.data[3] = expected.subindex;
    const uint32_t raw = abortReply ? 0x80000042 : expected.value;
    for (unsigned i = 0; i < (abortReply ? 4 : expected.width) && i < 4; ++i)
        reply.data[i + 4] = uint8_t(raw >> (8 * i));
    CHECK(owner.receive(reply, admitted + 1, admitted + 1));
    const C::SdoCompletion* terminal = owner.result(request);
    CHECK(terminal && terminal->effectPossible);
    CHECK(terminal->outcome == (abortReply ? C::SdoOutcome::SERVER_ABORT : C::SdoOutcome::SUCCESS));
    CHECK(L::advanceRead(context, *terminal, admitted + 1));
    CHECK(owner.release(request));
}
L::ReadContext completed() {
    L::ReadContext context = probe();
    C::SdoClient owner;
    configure(owner);
    exchange(owner, context, minimal, 0);
    return context;
}
void successfulProbe() {
    const L::ReadContext context = completed();
    const Saved<L::ReadContext> saved(context);
    L::ProbeObservation observation;
    CHECK(L::checkProbe(context, target(), 10, 10, observation));
    CHECK(observation.deviceType == 0x80040192);
    CHECK(M::sameBinding(observation.target.channel.binding, target().channel.binding));
    CHECK(observation.target.nodeGeneration == 11);
    CHECK(observation.provenance.index == 0x1000 && observation.provenance.subindex == 0);
    CHECK(observation.provenance.source == L::ReadSource::SDO);
    CHECK(observation.provenance.nodeGeneration == 11 && !observation.provenance.captureKnown);
    CHECK(observation.provenance.earliestUs == 0 && observation.provenance.latestUs == 1);
    CHECK(observation.provenance.deliveredUs == 1);
    CHECK(observation.window.earliestUs == 0 && observation.window.latestUs == 1 &&
          observation.window.skewUs == 1);
    CHECK(context.evidence.effectPossible && context.evidence.size == 4);
    saved.unchanged(context);
}
void rejectedPublication() {
    const L::ReadContext context = completed();
    L::ProbeObservation output;
    output.deviceType = 0xdeadbeef;
    const Saved<L::ProbeObservation> saved(output);
    for (unsigned variation = 0; variation < 6; ++variation) {
        L::ReadTarget changed = target();
        switch (variation) {
        case 0:
            ++changed.channel.binding.bindingGeneration;
            break;
        case 1:
            ++changed.channel.binding.configurationGeneration;
            break;
        case 2:
            ++changed.channel.clientToServerId;
            break;
        case 3:
            ++changed.channel.serverToClientId;
            break;
        case 4:
            ++changed.nodeGeneration;
            break;
        case 5:
            ++changed.channel.binding.endpoint.address;
            break;
        }
        CHECK(!L::checkProbe(context, changed, 10, 10, output));
        saved.unchanged(output);
    }
    CHECK(!L::checkProbe(context, target(), 10, 9, output));
    saved.unchanged(output);
    CHECK(!L::checkProbe(context, target(), 1, 0, output));
    saved.unchanged(output);
    CHECK(!L::checkProbe(context, target(), 0, 10, output));
    saved.unchanged(output);
    const L::ReadContext incomplete = probe();
    CHECK(!L::checkProbe(incomplete, target(), 1, 10, output));
    saved.unchanged(output);
    L::ReadContext invalidated = context;
    invalidated.invalidated = true;
    CHECK(!L::checkProbe(invalidated, target(), 10, 10, output));
    saved.unchanged(output);
    L::ReadContext withdrawn = probe();
    CHECK(L::advanceRead(withdrawn, M::ReadOutcome::INVALIDATED, 1));
    CHECK(withdrawn.invalidated && withdrawn.outcome == M::ReadOutcome::INVALIDATED);
    CHECK(!L::checkProbe(withdrawn, target(), 1, 10, output));
    saved.unchanged(output);
    L::ReadContext identityContext;
    CHECK(L::prepareIdentity(identityContext, target(), 24, 0, 10000, options()));
    C::SdoClient owner;
    configure(owner);
    for (unsigned i = 0; i < 9; ++i)
        exchange(owner, identityContext, identity[i], 2 * i);
    CHECK(identityContext.state == M::ReadState::SUCCEEDED);
    CHECK(!L::checkProbe(identityContext, target(), 20, 20, output));
    saved.unchanged(output);
    L::ReadContext failed = probe();
    CHECK(L::advanceRead(failed, M::ReadOutcome::CANCELLED, 1));
    CHECK(!L::checkProbe(failed, target(), 1, 10, output));
    saved.unchanged(output);
}
void retainedAbortAndOverlap() {
    L::ReadContext failed = probe();
    C::SdoClient owner;
    configure(owner);
    exchange(owner, failed, minimal, 0, true);
    CHECK(failed.outcome == M::ReadOutcome::PROTOCOL_REJECTED);
    CHECK(failed.evidence.abortCode == 0x80000042);
    const Saved<L::ReadContext> savedFailure(failed);
    L::ProbeObservation output;
    output.deviceType = 0xdeadbeef;
    const Saved<L::ProbeObservation> savedOutput(output);
    CHECK(!L::checkProbe(failed, target(), 1, 10, output));
    savedOutput.unchanged(output);
    savedFailure.unchanged(failed);

    // Construct the larger enclosing object last; the aliased output is refused before access.
    alignas(L::ReadContext) unsigned char storage[sizeof(L::ReadContext)];
    L::ProbeObservation* overlapping = new (storage) L::ProbeObservation();
    L::ReadContext* context = new (storage) L::ReadContext(completed());
    const Saved<L::ReadContext> savedContext(*context);
    CHECK(!L::checkProbe(*context, target(), 10, 10, *overlapping));
    savedContext.unchanged(*context);

    alignas(L::ProbeObservation) unsigned char targetStorage[sizeof(L::ProbeObservation)];
    L::ProbeObservation* targetOutput = new (targetStorage) L::ProbeObservation();
    L::ReadTarget* current = new (targetStorage) L::ReadTarget(target());
    unsigned char original[sizeof targetStorage];
    std::memcpy(original, targetStorage, sizeof original);
    CHECK(!L::checkProbe(*context, *current, 10, 10, *targetOutput));
    CHECK(!std::memcmp(original, targetStorage, sizeof original));
}
} // namespace
int main() {
    inventory();
    successfulProbe();
    rejectedPublication();
    retainedAbortAndOverlap();
    std::printf("discovery: %u checks\n", checks);
}
