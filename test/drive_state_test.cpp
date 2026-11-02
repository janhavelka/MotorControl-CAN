// SPDX-License-Identifier: MIT
// Literal documentary flags pass through the ordinary owner and raw read API.
// Synthetic replies test software interpretation, never firmware applicability.
#include "CanBusOwner.h"
#include <MotorControlCAN/profiles/lichuan_c/State.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace M = MotorControlCAN;
namespace C = MotorControlCAN::CANOPEN;
namespace L = MotorControlCAN::LICHUAN_C;
namespace E = MotorControlCANExample;
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
L::ReadTarget target() {
    L::ReadTarget value;
    value.channel.binding.logicalAxisId = 17;
    value.channel.binding.endpoint.profile = 41;
    value.channel.binding.endpoint.address = 5;
    value.channel.binding.bindingGeneration = 3;
    value.channel.binding.configurationGeneration = 7;
    value.channel.clientToServerId = 0x605;
    value.channel.serverToClientId = 0x585;
    value.nodeGeneration = 11;
    return value;
}
L::ReadOptions options() {
    L::ReadOptions value;
    value.queueTimeoutUs = value.responseTimeoutUs = 100;
    return value;
}
uint8_t blocks(bool feedback = false) {
    return uint8_t((1u << unsigned(L::StateBlock::STATUS)) |
                   (1u << unsigned(L::StateBlock::MONITORS)) |
                   (feedback ? 1u << unsigned(L::StateBlock::FEEDBACK) : 0));
}
M::Frame frame(uint32_t id, uint8_t width) {
    M::Frame value;
    value.id = id;
    value.dlc = value.payloadLength = width;
    return value;
}
// One staged frame and TX report suffice: the fixture explicitly dispatches
// heartbeat and SDO separately, through the same real receive owner.
struct Wire {
    E::ReceivedFrame incoming;
    E::TxReport report;
    M::Frame sent;
    bool hasIncoming = false, hasReport = false;
    unsigned transmitted = 0, observed = 0;
    static bool receive(void* context, E::ReceivedFrame& value) {
        Wire& wire = *static_cast<Wire*>(context);
        if (!wire.hasIncoming)
            return false;
        value = wire.incoming;
        wire.hasIncoming = false;
        return true;
    }
    static bool transmit(void* context, const M::Frame& value, C::SdoTxId id, uint32_t epoch) {
        Wire& wire = *static_cast<Wire*>(context);
        CHECK(!wire.hasReport && epoch == 1);
        CHECK(value.id == 0x605 && value.payloadLength == 8 && value.data[0] == 0x40);
        for (unsigned i = 4; i < 8; ++i)
            CHECK(value.data[i] == 0);
        wire.sent = value;
        wire.report.id = id;
        wire.report.controllerEpoch = epoch;
        wire.report.transmitted = true;
        wire.hasReport = true;
        ++wire.transmitted;
        return true;
    }
    static bool takeReport(void* context, E::TxReport& value) {
        Wire& wire = *static_cast<Wire*>(context);
        if (!wire.hasReport)
            return false;
        value = wire.report;
        wire.hasReport = false;
        return true;
    }
    static void observe(void* context, const E::ReceivedFrame&, uint64_t) {
        ++static_cast<Wire*>(context)->observed;
    }
    E::Transport callbacks() {
        E::Transport value;
        value.context = this;
        value.receive = receive;
        value.transmit = transmit;
        value.takeTxReport = takeReport;
        value.observe = observe;
        return value;
    }
    void push(const M::Frame& value, uint64_t now) {
        CHECK(!hasIncoming);
        incoming.frame = value;
        incoming.receivedTimeKnown = true;
        incoming.receivedUs = now;
        hasIncoming = true;
    }
};
struct Rig {
    C::SdoClient services;
    Wire wire;
    E::CanBusOwner owner;
    E::SequenceId sequence;
    L::ReadContext read;
    uint64_t now = 0;
    explicit Rig(bool feedback = false, uint8_t selection = 0)
        : owner(services, wire.callbacks(), 73) {
        CHECK(services.configure(0, target().channel, true, true));
        C::NetworkRoute network;
        network.binding = target().channel.binding;
        network.generation = 11;
        network.heartbeatId = 0x705;
        network.emcyId = 0x85;
        CHECK(services.configureNetwork(0, network, C::NetworkObserver(), 0));
        CHECK(owner.beginSequence(target().channel.binding, 1, 0, sequence));
        CHECK(owner.beginOperation(sequence, 1, 10000, 0));
        CHECK(L::prepareState(read, target(), 1, 0, 10000, options(),
                              selection ? selection : blocks(feedback)));
    }
    void turn(uint64_t time) {
        now = time;
        E::ServiceBudget budget;
        budget.rx = budget.tx = budget.events = budget.starts = 1;
        CHECK(owner.service(now, budget));
    }
    void field(uint16_t index, uint8_t width, uint32_t raw, bool heartbeat = false,
               bool abort = false) {
        L::PreparedRead work;
        CHECK(L::nextRead(read, now, work) && work.ready);
        CHECK(work.transfer.index == index && work.transfer.subindex == 0 &&
              work.transfer.size == width && work.transfer.direction == C::SdoDirection::UPLOAD);
        CHECK(work.transfer.readEffect == C::SdoReadEffect::UNQUALIFIED);
        E::BusRequest request;
        request.sequence = sequence;
        request.transfer = work.transfer;
        E::RequestId id;
        CHECK(owner.admit(request, now, id) == E::BusAdmission::ACCEPTED);
        turn(now);
        CHECK(wire.sent.data[1] == uint8_t(index) && wire.sent.data[2] == uint8_t(index >> 8));
        if (heartbeat) {
            M::Frame beat = frame(0x705, 1);
            beat.data[0] = 5;
            wire.push(beat, now + 1);
            turn(now + 1);
            CHECK(services.node(0)->nmt == C::NmtState::OPERATIONAL);
            CHECK(!owner.result(id));
        }
        M::Frame reply = frame(0x585, 8);
        reply.data[0] = abort ? 0x80 : width == 1 ? 0x4f : width == 2 ? 0x4b : 0x43;
        reply.data[1] = uint8_t(index);
        reply.data[2] = uint8_t(index >> 8);
        for (unsigned i = 0; i < 4; ++i)
            reply.data[4 + i] = uint8_t(raw >> (8 * i));
        wire.push(reply, now + 1);
        turn(now + 1);
        const E::Completion* result = owner.result(id);
        CHECK(result && result->reason == E::CompletionReason::SERVICE);
        const C::SdoCompletion copied = result->service;
        CHECK(copied.outcome == (abort ? C::SdoOutcome::SERVER_ABORT : C::SdoOutcome::SUCCESS));
        CHECK(L::advanceRead(read, copied, now));
        const Saved<L::ReadContext> retained(read);
        CHECK(owner.release(id));
        retained.unchanged(read);
    }
    void acquire(uint16_t status = 0, uint16_t monitor = 0, uint8_t mode = 1,
                 bool heartbeat = false) {
        field(0x2100, 2, monitor, heartbeat);
        field(0x2101, 2, 0xa580);
        field(0x2102, 2, 0xf008);
        field(0x6041, 2, status);
        field(0x6061, 1, mode);
        CHECK(wire.transmitted == 5);
    }
    L::StateInterpretation get(L::StatusLayout layout) {
        L::StateInterpretation value;
        CHECK(L::getStateInterpretation(read, target(), now, 1000, layout, value));
        return value;
    }
};
const L::StatusLayout layouts[] = {L::StatusLayout::UNSPECIFIED, L::StatusLayout::EN86,
                                   L::StatusLayout::ZH86, L::StatusLayout::ZH57,
                                   L::StatusLayout::OP86};
L::StateIndication indicated(bool value) {
    return value ? L::StateIndication::YES : L::StateIndication::NO;
}
void documentaryBits() {
    for (unsigned bit = 0; bit < 16; ++bit) {
        const uint16_t raw = uint16_t(1u << bit);
        Rig rig;
        rig.acquire(raw);
        for (L::StatusLayout layout : layouts) {
            const L::StateInterpretation value = rig.get(layout);
            CHECK(value.statusword == raw && value.modeRaw == 1);
            CHECK(value.lowStatusBits == (raw & 7));
            if (layout == L::StatusLayout::UNSPECIFIED) {
                CHECK(value.alarm == L::StateIndication::UNKNOWN &&
                      value.running == L::StateIndication::UNKNOWN &&
                      value.homingComplete == L::StateIndication::UNKNOWN &&
                      value.positionComplete == L::StateIndication::UNKNOWN &&
                      value.targetEffective == L::StateIndication::UNKNOWN);
                CHECK(!value.lowPatternListed && value.unlistedStatusBits == raw);
                CHECK(!value.alarmDisagreement && !value.runningDisagreement &&
                      !value.homingDisagreement && !value.positionDisagreement);
                continue;
            }
            const bool english = layout == L::StatusLayout::EN86;
            CHECK(value.alarm == indicated(bit == (layout == L::StatusLayout::ZH86 ? 3u : 7u)));
            CHECK(value.running == indicated(english ? bit != 10 : bit == 14));
            CHECK(value.homingComplete == indicated(bit == 8));
            CHECK(value.positionComplete == indicated(bit == 15));
            CHECK(value.targetEffective ==
                  (english ? indicated(bit == 12) : L::StateIndication::UNKNOWN));
            // Independent literal list of documented bit positions, rather than
            // importing the implementation's aggregate mask.
            const bool listed =
                bit <= 2 || bit == 8 || bit == 15 ||
                (english ? bit == 7 || bit == 10 || bit == 12
                         : bit == 14 || bit == (layout == L::StatusLayout::ZH86 ? 3u : 7u));
            CHECK(value.unlistedStatusBits == (listed ? 0 : raw));
        }
    }
    for (unsigned low = 0; low < 8; ++low) {
        Rig rig;
        rig.acquire(uint16_t(low));
        for (L::StatusLayout layout : layouts) {
            const L::StateInterpretation value = rig.get(layout);
            CHECK(value.lowStatusBits == low);
            CHECK(value.lowPatternListed == (layout != L::StatusLayout::UNSPECIFIED &&
                                             (low == 0 || low == 1 || low == 3 || low == 7)));
        }
    }
    for (unsigned bit = 0; bit < 16; ++bit) {
        Rig rig;
        rig.acquire(0, uint16_t(1u << bit));
        const L::StateInterpretation value = rig.get(L::StatusLayout::UNSPECIFIED);
        CHECK(value.monitorPositionComplete == (bit == 0));
        CHECK(value.monitorHomingComplete == (bit == 1));
        CHECK(value.monitorRunning == (bit == 2));
        CHECK(value.monitorAlarm == (bit == 3));
        CHECK(value.monitorReleased == (bit == 4));
        CHECK(value.positiveSoftwareLimit == (bit == 5));
        CHECK(value.negativeSoftwareLimit == (bit == 6));
        CHECK(value.unlistedMonitorBits == (bit > 6 ? uint16_t(1u << bit) : 0));
    }
    Rig enabled;
    enabled.acquire();
    CHECK(!enabled.get(L::StatusLayout::UNSPECIFIED).monitorReleased);
}
void rawModesDisagreementAndCopies() {
    const uint8_t modes[] = {0, 1, 3, 6, 2, 0x7f, 0x80, 0xff};
    for (uint8_t mode : modes) {
        Rig rig;
        rig.acquire(0xffff, 0xff80, mode, true);
        L::StateInterpretation value = rig.get(L::StatusLayout::EN86);
        const L::ObservedMode expectedMode = mode == 1   ? L::ObservedMode::PROFILE_POSITION
                                             : mode == 3 ? L::ObservedMode::PROFILE_VELOCITY
                                             : mode == 6 ? L::ObservedMode::HOMING
                                                         : L::ObservedMode::UNKNOWN;
        CHECK(value.modeRaw == mode && value.mode == expectedMode);
        CHECK(value.statusword == 0xffff && value.monitor == 0xff80 &&
              value.logicalInputs == 0xa580 && value.logicalOutputs == 0xf008);
        CHECK(value.alarmDisagreement && !value.runningDisagreement && value.homingDisagreement &&
              value.positionDisagreement);
        CHECK(value.unlistedMonitorBits == 0xff80 && rig.wire.observed == 6);
        CHECK(value.window.earliestUs == 0 && value.window.latestUs == rig.now &&
              value.window.skewUs == rig.now);
        const uint16_t indices[] = {0x6041, 0x6061, 0x2100, 0x2101, 0x2102};
        for (unsigned i = 0; i < 5; ++i) {
            CHECK(value.fields[i].index == indices[i] && value.fields[i].subindex == 0);
            CHECK(value.fields[i].source == L::ReadSource::SDO &&
                  value.fields[i].nodeGeneration == 11 && !value.fields[i].captureKnown);
            CHECK(value.fields[i].earliestUs <= value.fields[i].latestUs &&
                  value.fields[i].latestUs <= value.fields[i].deliveredUs);
        }
        CHECK(value.fields[2].earliestUs < value.fields[0].earliestUs);
        const Saved<L::StateInterpretation> copy(value);
        rig.read.values[0].raw = 0;
        rig.read.values[0].provenance.binding.configurationGeneration = 999;
        copy.unchanged(value);
    }
    Rig mismatched;
    mismatched.acquire(0x0407, 0x0004);
    const L::StateInterpretation value = mismatched.get(L::StatusLayout::EN86);
    CHECK(value.running == L::StateIndication::NO && value.monitorRunning &&
          value.runningDisagreement && value.window.skewUs > 0);
    Rig agrees;
    agrees.acquire(0x8187, 0x000f);
    const L::StateInterpretation matching = agrees.get(L::StatusLayout::OP86);
    CHECK(!matching.alarmDisagreement && matching.runningDisagreement &&
          !matching.homingDisagreement && !matching.positionDisagreement);
}
void rejectionAndPartialHistory() {
    Rig rig;
    rig.acquire(0x4007, 0x0004);
    L::StateInterpretation value = rig.get(L::StatusLayout::ZH86);
    const Saved<L::ReadContext> original(rig.read);
    CHECK(L::getStateInterpretation(rig.read, target(), 100, 100, L::StatusLayout::ZH86, value));
    const Saved<L::StateInterpretation> retained(value);
    CHECK(!L::getStateInterpretation(rig.read, target(), 101, 100, L::StatusLayout::ZH86, value));
    retained.unchanged(value);
    for (unsigned invalid = 0; invalid < 13; ++invalid) {
        L::ReadTarget current = target();
        L::ReadContext read = rig.read;
        uint64_t now = rig.now, age = 1000;
        L::StatusLayout layout = L::StatusLayout::EN86;
        switch (invalid) {
        case 0:
            ++current.channel.binding.bindingGeneration;
            break;
        case 1:
            ++current.channel.binding.configurationGeneration;
            break;
        case 2:
            ++current.nodeGeneration;
            break;
        case 3:
            ++current.channel.serverToClientId;
            break;
        case 4:
            ++current.channel.binding.endpoint.address;
            break;
        case 5:
            read.invalidated = true;
            break;
        case 6:
            age = 0;
            break;
        case 7:
            now = rig.now - 1;
            break;
        case 8:
            layout = static_cast<L::StatusLayout>(255);
            break;
        case 9:
            read.values[0].valid = false;
            break;
        case 10:
            ++read.values[0].provenance.nodeGeneration;
            break;
        case 11:
            read.kind = L::ReadKind::IDENTITY;
            break;
        case 12:
            read.values[0].provenance.earliestUs = rig.now + 1;
            break;
        }
        const Saved<L::ReadContext> before(read);
        CHECK(!L::getStateInterpretation(read, current, now, age, layout, value));
        retained.unchanged(value);
        before.unchanged(read);
    }
    original.unchanged(rig.read);
    Rig partial;
    partial.field(0x2100, 2, 0x10);
    partial.field(0x2101, 2, 0);
    partial.field(0x2102, 2, 0);
    partial.field(0x6041, 2, 7);
    CHECK(!L::getStateInterpretation(partial.read, target(), partial.now, 1000,
                                     L::StatusLayout::EN86, value));
    retained.unchanged(value);
    partial.field(0x6061, 1, 0x80000000, false, true);
    CHECK(partial.read.outcome == M::ReadOutcome::PROTOCOL_REJECTED);
    CHECK(!L::getStateInterpretation(partial.read, target(), partial.now, 1000,
                                     L::StatusLayout::EN86, value));
    retained.unchanged(value);
    Rig later(true);
    later.acquire(0x4007, 0x04);
    CHECK(later.read.state == M::ReadState::ACTIVE);
    CHECK(L::getStateInterpretation(later.read, target(), later.now, 1000, L::StatusLayout::ZH86,
                                    value));
    const L::StateInterpretation complete = value;
    later.field(0x6064, 4, 0xfedcba98, false, true);
    CHECK(later.read.outcome == M::ReadOutcome::PROTOCOL_REJECTED &&
          later.read.evidence.abortCode == 0xfedcba98);
    CHECK(L::getStateInterpretation(later.read, target(), later.now, 1000, L::StatusLayout::ZH86,
                                    value));
    CHECK(value.statusword == complete.statusword && value.monitor == complete.monitor &&
          value.mode == complete.mode && value.running == complete.running);
    CHECK(value.window.earliestUs == complete.window.earliestUs &&
          value.window.latestUs == complete.window.latestUs);
    for (unsigned i = 0; i < 5; ++i)
        CHECK(value.fields[i].index == complete.fields[i].index &&
              value.fields[i].earliestUs == complete.fields[i].earliestUs &&
              value.fields[i].latestUs == complete.fields[i].latestUs);
    CHECK(later.read.uncertain && later.read.values[0].valid);
    const Saved<L::StateInterpretation> afterFailure(value);
    Rig missing(false, uint8_t(1u << unsigned(L::StateBlock::STATUS)));
    missing.field(0x6041, 2, 7);
    missing.field(0x6061, 1, 1);
    CHECK(!L::getStateInterpretation(missing.read, target(), missing.now, 1000,
                                     L::StatusLayout::EN86, value));
    afterFailure.unchanged(value);
    Rig withdrawn(true);
    withdrawn.acquire(0x4007, 0x04);
    CHECK(L::advanceRead(withdrawn.read, M::ReadOutcome::INVALIDATED, withdrawn.now));
    CHECK(withdrawn.read.invalidated && withdrawn.read.values[0].valid &&
          withdrawn.read.values[3].raw == 0x4007);
    CHECK(!L::getStateInterpretation(withdrawn.read, target(), withdrawn.now, 1000,
                                     L::StatusLayout::ZH86, value));
    afterFailure.unchanged(value);
}
} // namespace
int main() {
    documentaryBits();
    rawModesDisagreementAndCopies();
    rejectionAndPartialHistory();
    std::printf("drive_state: %u checks passed\n", checks);
    return 0;
}
