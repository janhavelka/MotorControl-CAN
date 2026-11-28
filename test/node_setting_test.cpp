// SPDX-License-Identifier: MIT
// Prompt 61 node setting: refusals, the one 2203 write and its readback, and the two confirmation slots.
// Literal host vectors only; no hardware claims.
#include "CanBusOwner.h"
#include <MotorControlCAN/profiles/lichuan_c/Communication.h>
#include <MotorControlCAN/profiles/lichuan_c/State.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace M = MotorControlCAN;
namespace C = MotorControlCAN::CANOPEN;
namespace L = MotorControlCAN::LICHUAN_C;
namespace {
unsigned checks = 0;
#define CHECK(x)                                                                                   \
    do {                                                                                           \
        ++checks;                                                                                  \
        if (!(x)) {                                                                                \
            std::fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #x);                           \
            std::abort();                                                                          \
        }                                                                                          \
    } while (false)
template <class T> struct Saved {
    unsigned char bytes[sizeof(T)];
    explicit Saved(const T& value) { std::memcpy(bytes, &value, sizeof value); }
    void unchanged(const T& value) const { CHECK(!std::memcmp(bytes, &value, sizeof value)); }
};
} // namespace
#include "support/OwnerRig.h"
namespace {
// One terminal for transfer t at time at: an upload returns raw in the transfer's width.
C::SdoCompletion terminal(const C::SdoTransfer& t, uint64_t at, uint32_t raw = 0,
                          C::SdoOutcome result = C::SdoOutcome::SUCCESS, M::Generation node = 11) {
    C::SdoCompletion e;
    e.transfer = t;
    e.nodeGeneration = node;
    e.admittedUs = at;
    e.completedUs = at + 1;
    e.outcome = result;
    e.queued = e.transmissionPossible = e.transmissionConfirmed = e.effectPossible = true;
    e.protocolAcknowledged = result == C::SdoOutcome::SUCCESS;
    e.effectUncertain = !e.protocolAcknowledged;
    if (e.protocolAcknowledged && t.direction == C::SdoDirection::UPLOAD) {
        e.size = t.size;
        for (unsigned i = 0; i < 4; ++i)
            e.data[i] = uint8_t(raw >> (8 * i));
    }
    return e;
}
struct Fixture {
    L::AxisReservation reservation;
    L::NodeSettingReview review;
    L::NodeSettingRequest request;
    L::ReadOptions options;
    L::ReadContext state;
    L::NodeSettingContext context;
    M::Binding binding;
    uint64_t now = 100;
    explicit Fixture(uint16_t monitor = L::MONITOR_RELEASED) {
        reservation.target.channel = OwnerRig::route(1);
        reservation.target.nodeGeneration = 11;
        reservation.model = L::SettingsModel::CL86_C;
        reservation.owner = 73;
        reservation.sequence = 1;
        reservation.generation = 3;
        binding = reservation.target.channel.binding;
        review.target = reservation.target;
        review.dipAttestationRecord = 5;
        review.routeBackRecord = 6;
        review.configured.words[0] = 1u << 1 | 1u << 2; // Nodes 1 and 2 on this interface.
        request.node = 5;
        options.queueTimeoutUs = options.responseTimeoutUs = 100;
        CHECK(L::prepareState(state, reservation.target, 1, now, 5000, options,
                              uint8_t(1u << unsigned(L::StateBlock::MONITORS) | 1u << unsigned(L::StateBlock::STATUS))));
        for (;;) {
            L::PreparedRead p;
            CHECK(L::nextRead(state, now, p));
            if (!p.ready)
                break;
            const uint32_t raw = p.transfer.index == 0x2100 ? monitor : p.transfer.index == 0x6061 ? 1u : 0x0250u;
            CHECK(L::advanceRead(state, terminal(p.transfer, now, raw), now + 2));
            now += 2;
        }
        CHECK(state.state == M::ReadState::SUCCEEDED);
    }
    M::Status prepare(uint64_t age = 10000) {
        return L::prepareNodeSetting(context, reservation, review, request, state, 9, now, now + 3000, options, age);
    }
    L::PreparedRead next(const M::Binding& b) {
        L::PreparedRead p;
        CHECK(L::nextNodeSetting(context, reservation, b, now, p));
        return p;
    }
    // The download, its notice and a terminal with this outcome.
    C::SdoTransfer write(C::SdoOutcome outcome = C::SdoOutcome::SUCCESS) {
        const L::PreparedRead p = next(binding);
        CHECK(p.ready && p.transfer.direction == C::SdoDirection::DOWNLOAD && p.transfer.step == 1);
        CHECK(L::noteNodeSettingTransmission(context, reservation, binding, now));
        CHECK(L::advanceNodeSetting(context, reservation, terminal(p.transfer, now, 0, outcome), now + 2));
        now += 2;
        return p.transfer;
    }
    L::ReadTarget candidate(unsigned node, M::Generation generation = 12) const {
        L::ReadTarget t;
        t.channel = OwnerRig::route(node);
        t.channel.binding.bindingGeneration = 4; // The application's switched host route.
        t.nodeGeneration = generation;
        return t;
    }
    // One confirmation slot read to its terminal.
    void confirm(L::NodeSettingCandidate which, unsigned node, C::SdoOutcome outcome, uint32_t raw) {
        const L::ReadTarget t = candidate(node);
        CHECK(L::prepareNodeSettingConfirmation(context, t, which, now, now + 1000));
        const L::PreparedRead p = next(t.channel.binding);
        CHECK(p.ready && p.transfer.direction == C::SdoDirection::UPLOAD && p.transfer.index == 0x2203 &&
              p.transfer.step == 2u + context.confirmationsUsed && p.transfer.channel.clientToServerId == 0x600 + node);
        CHECK(L::advanceNodeSetting(context, reservation, terminal(p.transfer, now, raw, outcome, 12), now + 2));
        now += 2;
    }
};
void refusals() {
    // Each refusal leaves the context unchanged and prepares nothing.
    for (unsigned scenario = 0; scenario < 10; ++scenario) {
        Fixture f(scenario == 6 ? uint16_t(0) : scenario == 7 ? uint16_t(L::MONITOR_RELEASED | L::MONITOR_ALARM)
                                                              : uint16_t(L::MONITOR_RELEASED));
        uint64_t age = 10000;
        switch (scenario) {
        case 0: f.request.node = 0; break;    // Node zero.
        case 1: f.request.node = 128; break;  // Above 127.
        case 2: f.request.node = 1; break;    // The current node.
        case 3: f.request.node = 2; break;    // Configured on the interface.
        case 4: f.review.dipAttestationRecord = 0; break; // No DIP attestation.
        case 5: f.review.routeBackRecord = 0; break;      // No route back.
        case 6: break;                         // Enabled: 2100 does not report released.
        case 7: break;                         // An alarm.
        case 8: age = 5; break;                // A stale state read.
        default: f.reservation.target.nodeGeneration = 12; f.review.target = f.reservation.target; break; // Another boot.
        }
        const Saved<L::NodeSettingContext> saved(f.context);
        const M::Status s = f.prepare(age);
        CHECK(!s);
        CHECK(s.detail == int32_t(scenario <= 3 ? M::ReadError::VALUE
                                  : scenario <= 5 ? M::ReadError::ARGUMENT : M::ReadError::STATE));
        saved.unchanged(f.context);
    }
}
void writeReadbackAndConfirmation() {
    Fixture f;
    CHECK(f.prepare());
    L::NodeSettingResult result;
    CHECK(!L::getNodeSetting(f.context, result)); // Nothing terminal yet.
    // A confirmation before the terminal write is refused.
    CHECK(!L::prepareNodeSettingConfirmation(f.context, f.candidate(5), L::NodeSettingCandidate::REQUESTED, f.now,
                                             f.now + 1000));
    const M::Binding before = f.binding;
    const C::SdoTransfer w = f.write();
    // The one download: 2B 03 22 00 05 00, 2203:00 = 5, on the original route.
    CHECK(w.index == 0x2203 && w.subindex == 0 && w.size == 2 && w.data[0] == 5 && w.data[1] == 0 &&
          w.channel.clientToServerId == 0x601 && M::sameBinding(w.channel.binding, before));
    CHECK(f.binding.configurationGeneration == before.configurationGeneration + 1);
    // Acknowledged on the old route: not activation, the stored value still unconfirmed.
    CHECK(f.context.state == L::NodeSettingState::READING_BACK && f.context.writeAcknowledged && f.context.uncertain);
    CHECK(f.context.activation == L::CommunicationRequirement::UNKNOWN);
    L::PreparedRead stale;
    CHECK(!L::nextNodeSetting(f.context, f.reservation, before, f.now, stale)); // The old epoch is withdrawn.
    const L::PreparedRead p = f.next(f.binding);
    CHECK(p.ready && p.transfer.direction == C::SdoDirection::UPLOAD && p.transfer.index == 0x2203 &&
          p.transfer.step == 2 && p.transfer.channel.clientToServerId == 0x601 &&
          M::sameBinding(p.transfer.channel.binding, f.binding) && p.transfer.requireUploadReview);
    CHECK(L::advanceNodeSetting(f.context, f.reservation, terminal(p.transfer, f.now, 5), f.now + 2));
    f.now += 2;
    CHECK(f.context.state == L::NodeSettingState::STORED && !f.context.uncertain && f.context.readbackRaw == 5);
    CHECK(!f.next(f.binding).ready); // Nothing more is yielded on its own.
    CHECK(L::getNodeSetting(f.context, result));
    CHECK(result.stored && result.writeAcknowledged && !result.answeredOnRequested && result.beforeNode == 1 &&
          result.requestedNode == 5 && result.activation == L::CommunicationRequirement::UNKNOWN &&
          result.persistence == L::CommunicationRequirement::UNKNOWN && !result.confirmationsUsed);
    // A confirmation reads only an exact candidate: not node 4, not node 5 on other identifiers.
    L::ReadTarget wrong = f.candidate(4);
    CHECK(!L::prepareNodeSettingConfirmation(f.context, wrong, L::NodeSettingCandidate::REQUESTED, f.now, f.now + 1000));
    wrong = f.candidate(5);
    wrong.channel.clientToServerId = 0x611;
    CHECK(!L::prepareNodeSettingConfirmation(f.context, wrong, L::NodeSettingCandidate::REQUESTED, f.now, f.now + 1000));
    CHECK(!L::prepareNodeSettingConfirmation(f.context, f.candidate(5), L::NodeSettingCandidate::REQUESTED, f.now,
                                             f.now + 10)); // No exchange fits its own deadline.
    CHECK(!f.context.confirmationsUsed);
    // Slot 1: the requested node answers; slot 2: the old node does not. No third slot.
    f.confirm(L::NodeSettingCandidate::REQUESTED, 5, C::SdoOutcome::SUCCESS, 5);
    CHECK(L::prepareNodeSettingConfirmation(f.context, f.candidate(1), L::NodeSettingCandidate::BEFORE, f.now,
                                            f.now + 1000));
    CHECK(!L::prepareNodeSettingConfirmation(f.context, f.candidate(1), L::NodeSettingCandidate::BEFORE, f.now,
                                             f.now + 1000)); // One slot active at a time.
    const L::PreparedRead q = f.next(f.candidate(1).channel.binding);
    CHECK(q.ready && q.transfer.step == 4);
    CHECK(L::advanceNodeSetting(f.context, f.reservation, terminal(q.transfer, f.now, 0, C::SdoOutcome::TIMEOUT, 12),
                                f.now + 2));
    f.now += 2;
    CHECK(!L::prepareNodeSettingConfirmation(f.context, f.candidate(5), L::NodeSettingCandidate::REQUESTED, f.now,
                                             f.now + 1000));
    CHECK(f.context.confirmationsUsed == 2 && !f.next(f.binding).ready);
    CHECK(L::getNodeSetting(f.context, result));
    CHECK(result.requested == L::NodeSettingAnswer::ANSWERED && result.before == L::NodeSettingAnswer::NOT_ANSWERED &&
          result.answeredOnRequested && result.confirmationsUsed == 2 &&
          result.activation == L::CommunicationRequirement::UNKNOWN);
}
void lostReplyAndNeitherCandidate() {
    Fixture f;
    CHECK(f.prepare());
    f.write(C::SdoOutcome::TIMEOUT);
    // A lost reply: failed, uncertain, never resent, no readback.
    CHECK(f.context.state == L::NodeSettingState::FAILED && f.context.uncertain && !f.context.writeAcknowledged);
    CHECK(!f.next(f.binding).ready);
    L::NodeSettingResult result;
    CHECK(L::getNodeSetting(f.context, result) && !result.stored && result.uncertain);
    // Confirmation is still allowed after a possible download: on the old node it fails, on the requested node
    // the slot is withdrawn locally. Neither answered; nothing was sent again.
    f.confirm(L::NodeSettingCandidate::BEFORE, 1, C::SdoOutcome::TIMEOUT, 0);
    const L::ReadTarget t = f.candidate(5);
    CHECK(L::prepareNodeSettingConfirmation(f.context, t, L::NodeSettingCandidate::REQUESTED, f.now, f.now + 1000));
    CHECK(L::advanceNodeSetting(f.context, M::ReadOutcome::CANCELLED, f.now));
    CHECK(L::getNodeSetting(f.context, result));
    CHECK(result.before == L::NodeSettingAnswer::NOT_ANSWERED && result.requested == L::NodeSettingAnswer::NOT_ANSWERED &&
          !result.answeredOnRequested && result.uncertain && result.confirmationsUsed == 2);
    CHECK(!L::prepareNodeSettingConfirmation(f.context, t, L::NodeSettingCandidate::REQUESTED, f.now, f.now + 1000));
    // A withdrawal before any possible download leaves no effect and no confirmation.
    Fixture g;
    CHECK(g.prepare());
    CHECK(L::advanceNodeSetting(g.context, M::ReadOutcome::CANCELLED, g.now));
    CHECK(g.context.state == L::NodeSettingState::FAILED && !g.context.uncertain && !g.context.effectsPossible);
    CHECK(!L::prepareNodeSettingConfirmation(g.context, g.candidate(5), L::NodeSettingCandidate::REQUESTED, g.now,
                                             g.now + 1000));
    // A readback that differs fails the intent with its value kept.
    Fixture h;
    CHECK(h.prepare());
    h.write();
    const L::PreparedRead p = h.next(h.binding);
    CHECK(L::advanceNodeSetting(h.context, h.reservation, terminal(p.transfer, h.now, 0), h.now + 2));
    CHECK(h.context.state == L::NodeSettingState::FAILED && h.context.readbackKnown && h.context.readbackRaw == 0 &&
          h.context.uncertain);
}
} // namespace

int main() {
    refusals();
    writeReadbackAndConfirmation();
    lostReplyAndNeitherCandidate();
    std::printf("node setting: %u checks passed\n", checks);
}
