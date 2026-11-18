// SPDX-License-Identifier: MIT
// Synthetic peer/controller cases; no CL86-C object or recovery qualification.
// Independent reply bytes follow the pinned CO_SDOserver.h wire tables.
#include "SdoClient.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>

namespace M = MotorControlCAN;
namespace C = MotorControlCAN::CANOPEN;
namespace {
unsigned checks = 0;
} // namespace
#define CHECK(condition) do { ++checks; if (!(condition)) { std::fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #condition); std::abort(); } } while (false)
#include "support/OwnerRig.h"
namespace O = MotorControlCANOwner;
namespace {
// Cases on slot 1 (node 2, or a second channel of node 1) need a capacity set with two channels;
// a one-channel set runs the rest on slot 0. Further channels stay unconfigured.
constexpr bool PAIRED_CHANNELS = C::SdoClient::CHANNELS >= 2;

C::SdoChannel route(uint8_t node = 1) {
    C::SdoChannel c;
    c.binding.logicalAxisId = 100u + node;
    c.binding.endpoint.profile = 1;
    c.binding.endpoint.address = node;
    c.binding.bindingGeneration = 2;
    c.binding.configurationGeneration = 3;
    c.clientToServerId = 0x600u + node;
    c.serverToClientId = 0x580u + node;
    return c;
}
C::SdoTransfer work(bool download = true, uint8_t size = 2) {
    C::SdoTransfer w;
    w.channel = route();
    w.operation = 17; w.request = 23; w.step = 1;
    w.index = 0x2200; w.size = size;
    w.direction = download ? C::SdoDirection::DOWNLOAD : C::SdoDirection::UPLOAD;
    w.deadlineUs = 100000; w.queueTimeoutUs = 1000; w.responseTimeoutUs = 1000;
    for (unsigned i = 0; i < sizeof w.data; ++i) w.data[i] = static_cast<uint8_t>(i + 1);
    return w;
}
M::Frame reply(uint8_t command, const C::SdoChannel& c = route()) {
    M::Frame f;
    f.id = c.serverToClientId; f.dlc = f.payloadLength = 8;
    f.data[0] = command; f.data[2] = 0x22;
    return f;
}
bool sameFrame(const M::Frame& a, const M::Frame& b) {
    return a.id == b.id && a.dlc == b.dlc && a.payloadLength == b.payloadLength &&
        a.extended == b.extended && a.rtr == b.rtr && a.fd == b.fd && a.brs == b.brs &&
        a.esi == b.esi && std::memcmp(a.data, b.data, sizeof a.data) == 0;
}
bool sameRequest(C::SdoRequestId a, C::SdoRequestId b) {
    return a.owner == b.owner && a.channel == b.channel && a.generation == b.generation;
}
C::SdoRequestId admit(C::SdoClient& c, const C::SdoTransfer& w, uint64_t now = 0, uint8_t slot = 0) {
    C::SdoRequestId id;
    CHECK(c.admit(slot, w, now, id));
    return id;
}
M::Frame take(C::SdoClient& c, uint64_t now = 0, uint8_t slot = 0, C::SdoTxId* saved = nullptr) {
    M::Frame f; C::SdoTxId tx;
    CHECK(c.nextTx(slot, now, f, tx));
    CHECK(M::validateFrame(f));
    CHECK(c.canTransmit(tx, now));
    if (saved) *saved = tx;
    return f;
}
const C::SdoCompletion& terminal(const C::SdoClient& c, C::SdoRequestId id, C::SdoOutcome outcome) {
    const C::SdoCompletion* result = c.result(id);
    CHECK(result && result->outcome == outcome);
    return *result;
}
void unchanged(const C::SdoCompletion& now, const C::SdoCompletion& before) {
    CHECK(now.outcome == before.outcome && now.completedUs == before.completedUs);
    CHECK(now.queued == before.queued && now.transmissionPossible == before.transmissionPossible);
    CHECK(now.transmissionConfirmed == before.transmissionConfirmed && now.protocolAcknowledged == before.protocolAcknowledged);
    CHECK(now.effectPossible == before.effectPossible && now.effectUncertain == before.effectUncertain);
    CHECK(now.abortValid == before.abortValid && now.abortCode == before.abortCode && now.stackReturn == before.stackReturn);
    CHECK(now.size == before.size && std::memcmp(now.data, before.data, sizeof now.data) == 0);
    CHECK(now.replyValid == before.replyValid && sameFrame(now.reply, before.reply));
    CHECK(now.clientAbortQueued == before.clientAbortQueued);
    CHECK(now.transfer.operation == before.transfer.operation && now.transfer.request == before.transfer.request);
    CHECK(now.transfer.step == before.transfer.step && now.transfer.index == before.transfer.index);
    CHECK(now.transfer.subindex == before.transfer.subindex && now.transfer.size == before.transfer.size);
    CHECK(std::memcmp(now.transfer.data, before.transfer.data, sizeof now.transfer.data) == 0);
    CHECK(M::sameBinding(now.transfer.channel.binding, before.transfer.channel.binding));
}
void blocked(C::SdoClient& c, C::SdoTransfer w, uint64_t now) {
    C::SdoRequestId id; id.channel = 91; id.generation = 123;
    const C::SdoRequestId saved = id;
    CHECK(!c.admit(0, w, now, id));
    CHECK(sameRequest(id, saved));
    CHECK(c.unresolved(0));
}

void cancelEveryTxBoundary() {
    // Each prefix ends at one externally meaningful boundary of a 10-byte
    // transfer: admitted, staged initiation, handed-off initiation, staged
    // segment, handed-off segment, staged final segment, handed-off final.
    // Downloads and both upload effect classes share retirement boundaries;
    // upload progress also owns already received data that must not be published.
    for (unsigned form = 0; form < 3; ++form) {
        for (unsigned phase = 0; phase < 7; ++phase) {
            const bool download = form == 0, reviewed = form == 2;
            C::SdoClient c; CHECK(c.configure(0, route(), true, true));
            C::SdoTransfer w = work(download, 10);
            if (reviewed) w.readEffect = C::SdoReadEffect::REVIEWED_NON_CHANGING;
            const C::SdoRequestId id = admit(c, w);
            C::SdoTxId latest;
            if (phase >= 1) CHECK(c.service(0));
            if (phase >= 2) CHECK(take(c, 1, 0, &latest).data[0] == (download ? 0x21 : 0x40));
            if (phase >= 3) {
                M::Frame initiation = reply(download ? 0x60 : 0x41);
                if (!download) initiation.data[4] = 10;
                CHECK(c.receive(initiation, 2));
            }
            if (phase >= 4) CHECK(take(c, 3, 0, &latest).data[0] == (download ? 0x00 : 0x60));
            if (phase >= 5) {
                M::Frame segment = reply(download ? 0x20 : 0x00);
                if (!download) for (unsigned i = 1; i < 8; ++i) segment.data[i] = static_cast<uint8_t>(i);
                CHECK(c.receive(segment, 4));
            }
            if (phase >= 6) CHECK(take(c, 5, 0, &latest).data[0] == (download ? 0x19 : 0x70));
            const uint32_t handed = c.port(0)->handedOff;
            CHECK(c.cancel(id, 6));
            const C::SdoCompletion saved = terminal(c, id, C::SdoOutcome::CANCELLED);
            CHECK(saved.transmissionPossible == (phase >= 2));
            CHECK(saved.effectPossible == (phase >= 2 && !reviewed));
            CHECK(saved.effectUncertain == saved.effectPossible);
            CHECK(saved.transfer.size == 10 && saved.transfer.data[9] == 10);
            CHECK(saved.size == 0); // Partial upload data never becomes a completed value.
            CHECK(!c.port(0)->queueOccupied);
            CHECK(!c.canTransmit(latest, 7));
            CHECK(!c.confirmTx(latest, 7));
            if (phase >= 2) {
                const uint8_t late = download ? (phase >= 6 ? 0x30 : phase >= 4 ? 0x20 : 0x60)
                                              : (phase >= 6 ? 0x19 : phase >= 4 ? 0x00 : 0x41);
                CHECK(!c.receive(reply(late), 8));
            }
            M::Frame sentinel = reply(0xa5); C::SdoTxId tx; tx.frame = 99;
            const M::Frame previous = sentinel;
            CHECK(!c.nextTx(0, 9, sentinel, tx));
            CHECK(sameFrame(sentinel, previous) && tx.frame == 99);
            CHECK(c.service(10000));
            CHECK(c.port(0)->handedOff == handed);
            unchanged(*c.result(id), saved);
            CHECK(c.release(id));
            C::SdoRequestId fresh;
            CHECK(c.admit(0, work(), 10000, fresh) == (phase < 2));
            CHECK(c.unresolved(0) == (phase >= 2));
        }
    }
}

void failedPremisesCannotReopen() {
    for (unsigned changing = 0; changing < 2; ++changing) {
        C::SdoClient c; CHECK(c.configure(0, route(), true, true));
        C::SdoTransfer original = work(changing != 0);
        original.readEffect = C::SdoReadEffect::REVIEWED_NON_CHANGING;
        original.responseTimeoutUs = 5;
        const C::SdoRequestId id = admit(c, original);
        take(c);
        CHECK(c.service(5));
        const C::SdoCompletion saved = terminal(c, id, C::SdoOutcome::TIMEOUT);
        CHECK(saved.effectPossible == (changing != 0));
        CHECK(saved.effectUncertain == (changing != 0));
        CHECK(c.release(id));
        CHECK(!c.receive(reply(changing ? 0x60 : 0x4b), 6));
        blocked(c, work(), 6);
        C::SdoTransfer probe = work(false);
        probe.readEffect = C::SdoReadEffect::REVIEWED_NON_CHANGING;
        blocked(c, probe, 6);
        probe.operation++; probe.request++; probe.step++;
        blocked(c, probe, 6);
        ++probe.channel.binding.bindingGeneration;
        ++probe.channel.binding.configurationGeneration;
        blocked(c, probe, 6);
        CHECK(!c.configure(0, probe.channel, true, true));
        CHECK(!c.configure(1, probe.channel, true, true));
        M::Frame heartbeat; heartbeat.id = 0x701; heartbeat.dlc = heartbeat.payloadLength = 1;
        heartbeat.data[0] = 5;
        CHECK(!c.receive(heartbeat, 7));
        CHECK(c.service(90000)); // Quiet time is no server/controller retirement proof.
        C::SdoTransfer later = work(); later.deadlineUs = 200000;
        blocked(c, later, 90000);
        CHECK(!c.result(id));
        CHECK(c.port(0)->handedOff == 1 && !c.port(0)->queueOccupied);
    }
    // Even a selected-engine client abort accepted and handed off is no peer ACK.
    C::SdoClient c; CHECK(c.configure(0, route(), true, true));
    C::SdoTransfer w = work(false); w.allowSegmentedUpload = false; w.queueTimeoutUs = 3;
    const C::SdoRequestId id = admit(c, w); take(c);
    M::Frame initiation = reply(0x41); initiation.data[4] = 2;
    CHECK(c.receive(initiation, 1));
    CHECK(terminal(c, id, C::SdoOutcome::UNSUPPORTED).clientAbortQueued);
    CHECK(c.release(id));
    C::SdoTxId cleanup;
    CHECK(take(c, 2, 0, &cleanup).data[0] == 0x80);
    CHECK(c.canTransmit(cleanup, 3));
    blocked(c, work(), 3);
    CHECK(!c.canTransmit(cleanup, 4)); // Cleanup staging cutoff remains fixed after release/handoff.
    CHECK(c.service(4)); CHECK(!c.canTransmit(cleanup, 4));
    CHECK(c.service(10000)); blocked(c, work(), 10000);
}

void cleanupEligibilityAfterReleaseAndInvalidation() {
    for (unsigned event = 0; event < 3; ++event) {
        C::SdoClient c; CHECK(c.configure(0, route(), true, true));
        C::SdoTransfer w = work(false); w.allowSegmentedUpload = false;
        const C::SdoRequestId id = admit(c, w); take(c);
        M::Frame initiation = reply(0x41); initiation.data[4] = 2;
        CHECK(c.receive(initiation, 1));
        const C::SdoCompletion saved = terminal(c, id, C::SdoOutcome::UNSUPPORTED);
        CHECK(saved.clientAbortQueued && c.release(id));
        C::SdoTxId cleanup; CHECK(take(c, 2, 0, &cleanup).data[0] == 0x80);
        if (event == 0) CHECK(c.busOff(3));
        else if (event == 1) CHECK(c.reboot(1, 3));
        else CHECK(c.invalidate(0, 3));
        CHECK(!c.canTransmit(cleanup, 3));
        CHECK(!c.result(id) && !c.port(0)->queueOccupied);
        blocked(c, work(), 3);
    }
}

void controllerCacheSurvivesLocalInvalidation() {
    for (unsigned cause = 0; cause < 3; ++cause) {
        C::SdoClient c; CHECK(c.configure(0, route(), true, true));
        const C::SdoRequestId id = admit(c, work());
        C::SdoTxId oldTx;
        const M::Frame controllerCache = take(c, 0, 0, &oldTx);
        CHECK(controllerCache.data[4] == 1 && controllerCache.data[5] == 2);
        if (cause == 0) CHECK(c.cancel(id, 1));
        else if (cause == 1) CHECK(c.busOff(1));
        else CHECK(c.reboot(1, 1));
        const C::SdoOutcome expected = cause == 0 ? C::SdoOutcome::CANCELLED :
            cause == 1 ? C::SdoOutcome::BUS_OFF : C::SdoOutcome::REBOOT;
        const C::SdoCompletion saved = terminal(c, id, expected);
        CHECK(saved.effectPossible && saved.effectUncertain);
        CHECK(!c.confirmTx(oldTx, 2));
        CHECK(c.release(id));
        C::SdoTransfer different = work(); different.data[0] = 91; ++different.request;
        blocked(c, different, 2);
        CHECK(!c.configure(0, route(), true, true));
        CHECK(!c.receive(reply(0x60), 3));
        // The external copy deliberately remains a valid old wire request.
        // The adapter cannot revoke controller-owned bytes, so it refuses reuse.
        CHECK(controllerCache.id == 0x601 && controllerCache.data[4] == 1);
        CHECK(c.service(10000) && c.unresolved(0));
        CHECK(!c.port(0)->queueOccupied && c.port(0)->handedOff == 1);
    }
}

void distinctRouteAndUnrelatedEvents() {
    C::SdoClient c; CHECK(c.configure(0, route(), true, true));
    C::SdoChannel alternate = route();
    alternate.binding.logicalAxisId = 501; alternate.binding.endpoint.channel = 1;
    alternate.clientToServerId = 0x321; alternate.serverToClientId = 0x456;
    CHECK(c.configure(1, alternate, true, true));
    const C::SdoRequestId first = admit(c, work()); take(c);
    CHECK(c.cancel(first, 1)); CHECK(c.release(first));
    C::SdoTransfer other = work(); other.channel = alternate;
    const C::SdoRequestId second = admit(c, other, 2, 1);
    CHECK(take(c, 2, 1).id == 0x321);
    const uint32_t identifiers[] = {0x701, 0x181, 0x81, 0x582};
    for (unsigned i = 0; i < sizeof identifiers / sizeof identifiers[0]; ++i) {
        M::Frame event = reply(0x60); event.id = identifiers[i];
        CHECK(!c.receive(event, 3)); CHECK(!c.result(second));
        CHECK(c.unresolved(0) && !c.unresolved(1));
    }
    M::Frame wrongObject = reply(0x60, alternate); wrongObject.data[1] = 1;
    CHECK(!c.receive(wrongObject, 4)); CHECK(!c.result(second));
    CHECK(c.receive(reply(0x60, alternate), 5));
    CHECK(terminal(c, second, C::SdoOutcome::SUCCESS).protocolAcknowledged);
    CHECK(c.observation(1) && !c.observation(1)->receivedTimeKnown);
    CHECK(c.observation(1)->processedUs == 5 && sameFrame(c.observation(1)->frame, reply(0x60, alternate)));
    CHECK(c.unresolved(0) && !c.unresolved(1));
    CHECK(c.reboot(1, 6)); CHECK(c.unresolved(0) && c.unresolved(1));
    CHECK(c.result(second)->outcome == C::SdoOutcome::SUCCESS);
}

void partialWorkflowAndReadEffects() {
    C::SdoClient c; CHECK(c.configure(0, route(), true, true));
    C::SdoTransfer first = work();
    const C::SdoRequestId firstId = admit(c, first); take(c);
    CHECK(c.receive(reply(0x60), 1));
    const C::SdoCompletion firstResult = terminal(c, firstId, C::SdoOutcome::SUCCESS);
    CHECK(firstResult.effectPossible && !firstResult.effectUncertain);
    CHECK(c.release(firstId));
    C::SdoTransfer second = work(); second.step = 2; ++second.request; second.data[0] = 91;
    const C::SdoRequestId secondId = admit(c, second, 2); take(c, 2);
    M::Frame abort = reply(0x80);
    abort.data[4] = 0x20; abort.data[7] = 0x08;
    CHECK(c.receive(abort, 3));
    const C::SdoCompletion result = terminal(c, secondId, C::SdoOutcome::SERVER_ABORT);
    CHECK(result.abortValid && result.abortCode == 0x08000020u);
    CHECK(result.effectPossible && result.effectUncertain && !result.protocolAcknowledged);
    CHECK(firstResult.outcome == C::SdoOutcome::SUCCESS && firstResult.effectPossible);
    CHECK(firstResult.transfer.step == 1 && result.transfer.step == 2);
    CHECK(firstResult.transfer.operation == result.transfer.operation);
    CHECK(firstResult.transfer.data[0] == 1 && result.transfer.data[0] == 91);
    for (unsigned reviewed = 0; reviewed < 2; ++reviewed) {
        C::SdoClient reader; CHECK(reader.configure(0, route(), true, true));
        C::SdoTransfer read = work(false);
        read.readEffect = reviewed ? C::SdoReadEffect::REVIEWED_NON_CHANGING : C::SdoReadEffect::UNQUALIFIED;
        const C::SdoRequestId id = admit(reader, read); take(reader);
        CHECK(reader.cancel(id, 1));
        const C::SdoCompletion& outcome = terminal(reader, id, C::SdoOutcome::CANCELLED);
        CHECK(outcome.effectPossible == !reviewed && outcome.effectUncertain == !reviewed);
        CHECK(reader.unresolved(0)); // Read-effect evidence cannot identify a late reply.
    }
}

void duplicateSegmentsAndSameObjectPayloads() {
    for (unsigned stage = 0; stage < 2; ++stage) {
        C::SdoClient c; CHECK(c.configure(0, route(), true, true));
        const C::SdoRequestId id = admit(c, work(true, 10));
        take(c);
        CHECK(c.receive(reply(0x60), 1));
        take(c, 2);
        if (stage) {
            CHECK(c.receive(reply(0x20), 3));
            take(c, 4);
        }
        // A duplicate initiation command is invalid in a segment exchange;
        // a repeated preceding-segment acknowledgement has the wrong toggle.
        CHECK(c.receive(reply(stage ? 0x20 : 0x60), 5));
        const C::SdoCompletion& result = terminal(c, id, C::SdoOutcome::CLIENT_ABORT);
        CHECK(!result.protocolAcknowledged && result.effectUncertain && c.unresolved(0));
    }
    C::SdoClient c; CHECK(c.configure(0, route(), true, true));
    C::SdoTransfer first = work(); first.responseTimeoutUs = 5;
    const C::SdoRequestId id = admit(c, first);
    const M::Frame oldRequest = take(c);
    CHECK(c.service(5));
    CHECK(terminal(c, id, C::SdoOutcome::TIMEOUT).effectUncertain);
    CHECK(c.release(id));
    C::SdoTransfer second = first;
    second.data[0] = 0xa5; second.data[1] = 0x5a;
    ++second.operation; ++second.request; ++second.step;
    blocked(c, second, 6);
    CHECK(oldRequest.data[4] == 1 && oldRequest.data[5] == 2);
    M::Frame oldAcknowledgement = reply(0x60);
    // Neither the written value nor the local operation/request/step appears
    // in an ordinary download acknowledgement. A changed payload is no escape.
    for (unsigned i = 4; i < 8; ++i) CHECK(oldAcknowledgement.data[i] == 0);
    CHECK(!c.receive(oldAcknowledgement, 7));
    blocked(c, second, 7);
}

void toggleWrapAmbiguityLimitation() {
    // This counterexample deliberately violates the external no-duplicate
    // session premise. Ordinary segment ACKs have only one toggle bit: after
    // it wraps, an old ACK can match a later segment. Host tokens cannot help.
    for (unsigned evidence = 0; evidence < 3; ++evidence) {
        C::SdoClient c; CHECK(c.configure(0, route(), true, true));
        const C::SdoRequestId id = admit(c, work(true, 21));
        take(c); CHECK(c.receive(reply(0x60), 1));
        CHECK(take(c, 2).data[0] == 0x00);
        const M::Frame firstAck = reply(0x20);
        CHECK(c.receive(firstAck, 3));
        CHECK(take(c, 4).data[0] == 0x10);
        CHECK(c.receive(reply(0x30), 5));
        C::SdoTxId finalTx;
        CHECK(take(c, 6, 0, &finalTx).data[0] == 0x01);
        if (evidence == 0) {
            // A delayed copy with unknown capture time is indistinguishable
            // from a fresh final ACK. Success here demonstrates the limitation;
            // it does not qualify any real peer/controller freshness premise.
            CHECK(c.receive(firstAck, 7));
        } else if (evidence == 1) {
            CHECK(!c.receive(firstAck, 3, 7)); // Original capture proves it is old.
        } else {
            CHECK(c.receive(reply(0x20), 7, 7)); // Independently fresh final ACK.
        }
        const C::SdoCompletion& result = terminal(c, id,
            evidence == 1 ? C::SdoOutcome::CORRELATION_LOST : C::SdoOutcome::SUCCESS);
        CHECK(result.transmissionPossible && result.effectPossible);
        CHECK(result.effectUncertain == (evidence == 1));
        CHECK(result.protocolAcknowledged == (evidence != 1));
        CHECK(c.unresolved(0) == (evidence == 1));
        CHECK(!c.canTransmit(finalTx, 7));
        CHECK(c.observation(0)->receivedTimeKnown == (evidence != 0));
        if (evidence == 1) {
            CHECK(c.observation(0)->disposition == C::SdoRxDisposition::UNRESOLVED);
            CHECK(c.observation(0)->receivedUs == 3 && c.observation(0)->processedUs == 7);
        }
    }
}

void capturedAndProcessingTime() {
    const uint64_t received[] = {4, 5, 6, 4};
    const uint64_t processed[] = {4, 5, 6, 6};
    for (unsigned overall = 0; overall < 2; ++overall) {
        for (unsigned i = 0; i < sizeof received / sizeof received[0]; ++i) {
            C::SdoClient c; CHECK(c.configure(0, route(), true, true));
            C::SdoTransfer w = work();
            if (overall) w.deadlineUs = 5;
            else w.responseTimeoutUs = 5;
            const C::SdoRequestId id = admit(c, w); take(c);
            CHECK(c.receive(reply(0x60), received[i], processed[i]) == (i == 0));
            const C::SdoCompletion& result = terminal(c, id,
                i == 0 ? C::SdoOutcome::SUCCESS : C::SdoOutcome::TIMEOUT);
            CHECK(result.completedUs == processed[i]);
            CHECK(result.protocolAcknowledged == (i == 0));
            const C::SdoRxObservation* observation = c.observation(0);
            CHECK(observation && observation->receivedTimeKnown);
            CHECK(observation->receivedUs == received[i] && observation->processedUs == processed[i]);
            CHECK(observation->disposition == (i == 0 ? C::SdoRxDisposition::PROCESSED : C::SdoRxDisposition::UNRESOLVED));
        }
    }
    // Prior service closure cannot be rolled back by subsequently dispatched
    // history, even when that history carries an earlier capture timestamp.
    C::SdoClient c; CHECK(c.configure(0, route(), true, true));
    C::SdoTransfer w = work(); w.responseTimeoutUs = 5;
    const C::SdoRequestId id = admit(c, w); take(c);
    CHECK(c.service(5));
    const C::SdoCompletion saved = terminal(c, id, C::SdoOutcome::TIMEOUT);
    CHECK(!c.receive(reply(0x60), 4, 6));
    unchanged(*c.result(id), saved);
    CHECK(c.observation(0)->receivedUs == 4 && c.observation(0)->processedUs == 6);
    const C::SdoRxObservation oldObservation = *c.observation(0);
    CHECK(!c.receive(reply(0x60), 8, 7)); // Future capture time rejects atomically.
    CHECK(!c.receive(reply(0x60), 3, 5)); // Processing time cannot run backwards.
    CHECK(c.observation(0)->receivedUs == oldObservation.receivedUs);
    CHECK(c.observation(0)->processedUs == oldObservation.processedUs);
    unchanged(*c.result(id), saved);
    CHECK(c.release(id));
    CHECK(!c.receive(reply(0x60), 7, 8));
    CHECK(!c.result(id) && c.observation(0)->processedUs == 8 && c.unresolved(0));
}

void terminalPrecedenceAndLaterObservations() {
    for (unsigned event = 0; event < 4; ++event) {
        C::SdoClient c; CHECK(c.configure(0, route(), true, true));
        const C::SdoRequestId id = admit(c, work()); take(c);
        if (event == 0) CHECK(c.cancel(id, 1));
        else if (event == 1) CHECK(c.busOff(1));
        else if (event == 2) CHECK(c.reboot(1, 1));
        else CHECK(c.invalidate(0, 1));
        const C::SdoOutcome expected[] = {C::SdoOutcome::CANCELLED, C::SdoOutcome::BUS_OFF,
            C::SdoOutcome::REBOOT, C::SdoOutcome::BINDING_INVALIDATED};
        const C::SdoCompletion saved = terminal(c, id, expected[event]);
        CHECK(!c.receive(reply(0x60), 0, 2));
        unchanged(*c.result(id), saved);
        CHECK(c.result(id)->effectUncertain && !c.result(id)->protocolAcknowledged);
        CHECK(c.observation(0)->receivedTimeKnown && c.observation(0)->receivedUs == 0);
        CHECK(c.observation(0)->processedUs == 2 && c.observation(0)->disposition == C::SdoRxDisposition::UNRESOLVED);
    }
    // The same events at the exact response cutoff cannot replace timeout.
    for (unsigned event = 0; event < 4; ++event) {
        C::SdoClient c; CHECK(c.configure(0, route(), true, true));
        C::SdoTransfer w = work(); w.responseTimeoutUs = 5;
        const C::SdoRequestId id = admit(c, w); take(c);
        if (event == 0) CHECK(!c.cancel(id, 5));
        else if (event == 1) CHECK(c.busOff(5));
        else if (event == 2) CHECK(c.reboot(1, 5));
        else CHECK(c.invalidate(0, 5));
        CHECK(terminal(c, id, C::SdoOutcome::TIMEOUT).completedUs == 5);
    }
}

void sameTimeTerminalOrdering() {
    for (unsigned cancelFirst = 0; cancelFirst < 2; ++cancelFirst) {
        C::SdoClient c; CHECK(c.configure(0, route(), true, true));
        const C::SdoRequestId id = admit(c, work()); take(c);
        if (cancelFirst) {
            CHECK(c.cancel(id, 1));
            const C::SdoCompletion saved = terminal(c, id, C::SdoOutcome::CANCELLED);
            CHECK(!c.receive(reply(0x60), 1, 1));
            unchanged(*c.result(id), saved);
            CHECK(c.observation(0)->disposition == C::SdoRxDisposition::UNRESOLVED);
        } else {
            CHECK(c.receive(reply(0x60), 1, 1));
            const C::SdoCompletion saved = terminal(c, id, C::SdoOutcome::SUCCESS);
            CHECK(!c.cancel(id, 1));
            unchanged(*c.result(id), saved);
            CHECK(!c.unresolved(0));
        }
    }
}

void staleBeforeHandoffAndObservedDuplicates() {
    for (unsigned released = 0; released < 2; ++released) {
        C::SdoClient c; CHECK(c.configure(0, route(), true, true));
        const C::SdoRequestId id = admit(c, work()); take(c);
        CHECK(c.receive(reply(0x60), 1, 1));
        const C::SdoCompletion saved = terminal(c, id, C::SdoOutcome::SUCCESS);
        if (released) CHECK(c.release(id));
        CHECK(!c.receive(reply(0x60), 2, 2));
        CHECK(c.unresolved(0));
        CHECK(c.observation(0)->disposition == C::SdoRxDisposition::UNRESOLVED);
        if (!released) {
            unchanged(*c.result(id), saved);
            CHECK(c.release(id));
        }
        blocked(c, work(), 3);
    }
    C::SdoClient c; CHECK(c.configure(0, route(), true, true));
    const C::SdoRequestId first = admit(c, work());
    take(c, 1); CHECK(c.receive(reply(0x60), 2, 2)); CHECK(c.release(first));
    C::SdoTransfer different = work(); different.data[0] = 91; ++different.request; ++different.step;
    const C::SdoRequestId second = admit(c, different, 3);
    C::SdoTxId tx; const M::Frame freshRequest = take(c, 10, 0, &tx);
    CHECK(freshRequest.data[4] == 91 && c.canTransmit(tx, 10));
    CHECK(!c.receive(reply(0x60), 9, 11)); // Captured before this handoff.
    const C::SdoCompletion& result = terminal(c, second, C::SdoOutcome::CORRELATION_LOST);
    CHECK(result.effectUncertain && !result.protocolAcknowledged);
    CHECK(c.unresolved(0) && !c.canTransmit(tx, 11));
    CHECK(c.observation(0)->receivedUs == 9 && c.observation(0)->processedUs == 11);
}

void segmentCaptureAndBorrowedObservation() {
    C::SdoClient c; CHECK(c.configure(0, route(), true, true));
    const C::SdoRequestId id = admit(c, work(true, 10)); take(c);
    CHECK(c.receive(reply(0x60), 1, 1));
    C::SdoTxId segment; take(c, 3, 0, &segment);
    // Being newer than initiation is insufficient: capture must also follow
    // the latest segment handoff whose response is now awaited.
    CHECK(!c.receive(reply(0x20), 2, 4));
    const C::SdoCompletion saved = terminal(c, id, C::SdoOutcome::CORRELATION_LOST);
    CHECK(saved.effectUncertain && !c.canTransmit(segment, 4));
    const C::SdoRxObservation* observed = c.observation(0);
    CHECK(observed && observed->disposition == C::SdoRxDisposition::UNRESOLVED);
    const M::Frame copied = observed->frame;
    CHECK(!c.receive(observed->frame, 5, 6)); // Input aliases the observation being replaced.
    CHECK(sameFrame(c.observation(0)->frame, copied));
    CHECK(c.observation(0)->receivedUs == 5 && c.observation(0)->processedUs == 6);
    unchanged(*c.result(id), saved);
}

void extendedObservationPreservesLiveRequest() {
    C::SdoClient c; CHECK(c.configure(0, route(), true, true));
    const C::SdoRequestId id = admit(c, work());
    C::SdoTxId tx; take(c, 0, 0, &tx);
    M::Frame extended = reply(0x60); extended.extended = true;
    CHECK(!c.receive(extended, 1, 1));
    CHECK(!c.result(id) && !c.unresolved(0) && c.canTransmit(tx, 1));
    const C::SdoRxObservation* observed = c.observation(0);
    CHECK(observed && observed->disposition == C::SdoRxDisposition::REJECTED);
    CHECK(sameFrame(observed->frame, extended));
    CHECK(c.receive(reply(0x60), 2, 2));
    CHECK(terminal(c, id, C::SdoOutcome::SUCCESS).protocolAcknowledged);
    CHECK(c.observation(0)->disposition == C::SdoRxDisposition::PROCESSED);
}

void transmitEligibilityAndBindingInvalidation() {
    C::SdoClient c; CHECK(c.configure(0, route(), true, true));
    CHECK(c.configure(1, route(2), true, true));
    const C::SdoRequestId first = admit(c, work());
    C::SdoTransfer otherWork = work(); otherWork.channel = route(2);
    const C::SdoRequestId second = admit(c, otherWork, 0, 1);
    C::SdoTxId firstTx, secondTx;
    const M::Frame cached = take(c, 0, 0, &firstTx); take(c, 0, 1, &secondTx);
    CHECK(c.canTransmit(firstTx, 0) && c.canTransmit(secondTx, 0));
    CHECK(c.invalidate(0, 1));
    const C::SdoCompletion& result = terminal(c, first, C::SdoOutcome::BINDING_INVALIDATED);
    CHECK(result.effectUncertain && !c.canTransmit(firstTx, 1));
    CHECK(c.canTransmit(secondTx, 1) && !c.unresolved(1));
    CHECK(c.receive(reply(0x60, route(2)), 2));
    CHECK(terminal(c, second, C::SdoOutcome::SUCCESS).protocolAcknowledged);
    CHECK(!c.canTransmit(secondTx, 2));
    CHECK(cached.data[4] == 1 && cached.data[5] == 2); // External storage is still owned externally.
    CHECK(c.release(first));
    C::SdoChannel replacement = route(); ++replacement.binding.bindingGeneration;
    CHECK(!c.configure(0, replacement, true, true));
    CHECK(!c.configure(1, replacement, true, true));
    C::SdoTransfer w = work(); w.channel = replacement; blocked(c, w, 3);
    CHECK(!c.invalidate(2, 3));
    CHECK(!c.invalidate(0, 0));
}

void oneHandoffCannotBecomeNewIntent() {
    C::SdoClient c; CHECK(c.configure(0, route(), true, true));
    C::SdoChannel relabeled = route();
    relabeled.clientToServerId = 0x321; relabeled.serverToClientId = 0x456;
    ++relabeled.binding.bindingGeneration;
    CHECK(!c.configure(1, relabeled, true, true)); // Same physical node/channel is no independent route.
    const C::SdoRequestId id = admit(c, work(true, 10));
    C::SdoTxId first, second, third;
    take(c, 0, 0, &first);
    C::SdoClient foreign;
    CHECK(!foreign.canTransmit(first, 0));
    C::SdoTxId invalid = first; invalid.request.owner = &foreign;
    CHECK(!c.canTransmit(invalid, 0));
    invalid = first; invalid.request.channel = C::SdoClient::CHANNELS;
    CHECK(!c.canTransmit(invalid, 0));
    invalid = first; ++invalid.request.generation;
    CHECK(!c.canTransmit(invalid, 0));
    invalid = first; invalid.frame = 0;
    CHECK(!c.canTransmit(invalid, 0));
    CHECK(c.canTransmit(first, 0));
    CHECK(c.canTransmit(first, 1));
    CHECK(c.receive(reply(0x60), 2));
    CHECK(c.confirmTx(first, 2) && !c.canTransmit(first, 2));
    take(c, 3, 0, &second);
    CHECK(!c.canTransmit(second, 2));
    CHECK(!c.canTransmit(first, 3) && !c.confirmTx(first, 3));
    CHECK(c.confirmTx(second, 3) && !c.canTransmit(second, 3));
    CHECK(c.receive(reply(0x20), 4));
    CHECK(!c.canTransmit(second, 4));
    take(c, 5, 0, &third);
    CHECK(!c.canTransmit(second, 5));
    CHECK(c.cancel(id, 6));
    CHECK(!c.canTransmit(third, 6));
    CHECK(terminal(c, id, C::SdoOutcome::CANCELLED).effectUncertain);
}

void nonwrappingTimeAndZeroCapture() {
    {
        C::SdoClient c; CHECK(c.configure(0, route(), true, true));
        const C::SdoRequestId id = admit(c, work()); take(c);
        CHECK(c.receive(reply(0x60), 0, 0));
        CHECK(terminal(c, id, C::SdoOutcome::SUCCESS).completedUs == 0);
        CHECK(c.observation(0)->receivedTimeKnown && c.observation(0)->receivedUs == 0);
    }
    {
        C::SdoClient c; CHECK(c.configure(0, route(), true, true));
        const uint64_t maximum = std::numeric_limits<uint64_t>::max();
        C::SdoTransfer w = work(); w.deadlineUs = maximum;
        const C::SdoRequestId id = admit(c, w, maximum - 3);
        C::SdoTxId tx; take(c, maximum - 2, 0, &tx);
        CHECK(c.canTransmit(tx, maximum - 1));
        CHECK(!c.canTransmit(tx, maximum));
        CHECK(!c.receive(reply(0x60), maximum - 1, maximum));
        CHECK(terminal(c, id, C::SdoOutcome::TIMEOUT).completedUs == maximum);
        CHECK(c.observation(0)->receivedUs == maximum - 1 && c.observation(0)->processedUs == maximum);
        CHECK(!c.service(0) && !c.receive(reply(0x60), 0, 0));
        CHECK(c.observation(0)->processedUs == maximum); // A wrapped hardware clock is not monotonic.
    }
}

// Drain marker at the client boundary (DEC-3). Each premise below is stated by this fake. A
// positive proves this client's logic, never that a real server answers in order (Q18).
C::SdoDrainPremises fakePremises() {
    C::SdoDrainPremises out;
    out.soleClient = out.orderedServer = true;
    return out;
}
C::SdoTransfer drainMarker(uint16_t index = 0x2000, uint8_t subindex = 0) {
    C::SdoTransfer out = work(false);
    out.operation = 40; out.request = 41; out.step = 42;
    out.index = index; out.subindex = subindex;
    out.readEffect = C::SdoReadEffect::ACCEPTED_UNQUALIFIED;
    out.allowSegmentedUpload = false;
    return out;
}
M::Frame named(uint8_t command, uint16_t index, uint8_t subindex = 0) {
    M::Frame f = reply(command);
    f.data[1] = static_cast<uint8_t>(index); f.data[2] = static_cast<uint8_t>(index >> 8);
    f.data[3] = subindex;
    return f;
}
// The one cleanup abort a response timeout after possible TX stages (prompt 55): CiA 301
// "SDO protocol timed out" 0x05040000 with the request's own object, handed off once and then
// reported locally retired. It settles nothing.
void retireAbort(C::SdoClient& c, uint64_t now, uint16_t index = 0x2200, uint8_t subindex = 0,
                 uint8_t slot = 0) {
    C::SdoTxId tx;
    const M::Frame abort = take(c, now, slot, &tx);
    const uint8_t literal[8] = {0x80, uint8_t(index), uint8_t(index >> 8), subindex, 0, 0, 0x04, 0x05};
    CHECK(abort.id == c.channel(slot)->clientToServerId && abort.dlc == 8 && !abort.extended && !abort.rtr);
    CHECK(std::memcmp(abort.data, literal, sizeof literal) == 0);
    M::Frame again; C::SdoTxId second;
    CHECK(!c.nextTx(slot, now, again, second)); // Handed off at most once.
    CHECK(c.noteTxRetired(tx) && c.unresolved(slot));
}
// A released timed-out write: the route is unresolved and retains 0x2200:00. Its abort is
// handed off and retired, so local TX on the route is retired.
void quarantine(C::SdoClient& c) {
    CHECK(c.configure(0, route(), true, true));
    C::SdoTransfer lost = work(); lost.responseTimeoutUs = 5;
    const C::SdoRequestId id = admit(c, lost); take(c);
    CHECK(c.service(5)); CHECK(terminal(c, id, C::SdoOutcome::TIMEOUT).clientAbortQueued);
    CHECK(c.release(id) && c.unresolved(0));
    retireAbort(c, 5);
}

void drainMarkerClientBoundary() {
    C::SdoClient c; quarantine(c);
    C::SdoTransfer marker = drainMarker();
    C::SdoRequestId id; id.channel = 91; id.generation = 123;
    const C::SdoRequestId saved = id;
    C::SdoDrainPremises premises = fakePremises();
    // Bad slots, a spent queue budget and each false premise publish nothing.
    CHECK(!c.checkDrain(C::SdoClient::CHANNELS, marker, premises, 5));
    CHECK(!c.drainSettled(C::SdoClient::CHANNELS) && !c.reopenAfterDrain(C::SdoClient::CHANNELS));
    CHECK(c.requalifications(C::SdoClient::CHANNELS) == 0);
    CHECK(!c.admitDrain(0, marker, premises, 5, id, 5));
    premises.soleClient = false;
    CHECK(!c.admitDrain(0, marker, premises, 5, id, 1000));
    premises = fakePremises(); premises.orderedServer = false;
    CHECK(!c.admitDrain(0, marker, premises, 5, id, 1000));
    CHECK(!c.admitDrain(0, marker, C::SdoDrainPremises(), 5, id, 1000));
    CHECK(sameRequest(id, saved) && !c.drainSettled(0) && !c.reopenAfterDrain(0));
    // Nothing the client itself offers bypasses ordinary admission meanwhile.
    CHECK(!c.checkAdmission(0, marker, 5) && !c.admit(0, marker, 5, id));
    premises = fakePremises();
    CHECK(c.admitDrain(0, marker, premises, 5, id, 1000));
    CHECK(id.owner == &c && id.channel == 0 && id.generation != saved.generation);
    C::SdoRequestId other;
    CHECK(!c.admitDrain(0, drainMarker(0x2001), premises, 5, other, 1000)); // One at a time.
    M::Frame sent = take(c, 5);
    const uint8_t literal[8] = {0x40, 0x00, 0x20, 0, 0, 0, 0, 0};
    CHECK(sent.id == 0x601 && std::memcmp(sent.data, literal, 8) == 0);
    M::Frame answer = named(0x4b, 0x2000); answer.data[4] = 0x34; answer.data[5] = 0x12;
    CHECK(c.receive(answer, 6));
    const C::SdoCompletion result = terminal(c, id, C::SdoOutcome::SUCCESS);
    CHECK(result.size == 2 && result.data[0] == 0x34 && result.data[1] == 0x12);
    // The marker settles the channel. The route opens only on the explicit reopening.
    CHECK(c.drainSettled(0) && c.unresolved(0) && c.requalifications(0) == 0);
    CHECK(c.release(id));
    CHECK(c.drainSettled(0) && c.unresolved(0));
    CHECK(!c.checkAdmission(0, work(), 6) && !c.checkDrain(0, drainMarker(0x2001), premises, 6));
    CHECK(c.reopenAfterDrain(0));
    CHECK(!c.drainSettled(0) && !c.unresolved(0) && c.requalifications(0) == 1);
    CHECK(!c.reopenAfterDrain(0) && c.requalifications(0) == 1);
    CHECK(c.checkAdmission(0, work(), 6) && !c.checkDrain(0, drainMarker(0x2001), premises, 6));
    // A controller state refuses a marker below the owner too, without touching the route.
    C::SdoClient d; quarantine(d);
    const C::ControllerState refused[] = {C::ControllerState::BUS_OFF, C::ControllerState::RECOVERING};
    uint64_t now = 6;
    for (const C::ControllerState state : refused) {
        CHECK(d.reportController(state, now));
        CHECK(!d.checkDrain(0, drainMarker(), premises, now));
        CHECK(d.reportController(C::ControllerState::AVAILABLE, ++now));
        CHECK(d.checkDrain(0, drainMarker(), premises, now));
    }
}

// Quarantine follows what may have left the host, sampled before retirement clears it.
void controllerEventsSamplePossibleWork() {
    const C::ControllerState events[] = {C::ControllerState::BUS_OFF, C::ControllerState::RECOVERING};
    for (const C::ControllerState event : events) {
        // Admitted and staged frames never left.
        for (unsigned staged = 0; staged < 2; ++staged) {
            C::SdoClient c; CHECK(c.configure(0, route(), true, true));
            const C::SdoRequestId id = admit(c, work());
            if (staged) CHECK(c.service(0) && c.port(0)->queueOccupied);
            CHECK(c.reportController(event, 1));
            CHECK(!c.unresolved(0) && !c.result(id)->transmissionPossible && !c.port(0)->queueOccupied);
        }
        // A first frame held unsubmitted never reached the SDK, so its handoff is proved unsent.
        {
            C::SdoClient c; CHECK(c.configure(0, route(), true, true));
            const C::SdoRequestId id = admit(c, work());
            C::SdoTxId tx; take(c, 0, 0, &tx);
            CHECK(c.holdHandoff(tx));
            CHECK(c.reportController(event, 1));
            CHECK(!c.unresolved(0) && !c.result(id)->transmissionPossible && !c.result(id)->effectUncertain);
        }
        // A later frame held unsubmitted follows one that did leave.
        {
            C::SdoClient c; CHECK(c.configure(0, route(), true, true));
            const C::SdoRequestId id = admit(c, work(true, 8));
            take(c, 0);
            CHECK(c.receive(reply(0x60), 1));
            C::SdoTxId tx; take(c, 2, 0, &tx);
            CHECK(c.holdHandoff(tx));
            CHECK(c.reportController(event, 3));
            CHECK(c.unresolved(0) && c.result(id)->transmissionPossible && c.result(id)->effectUncertain);
        }
        // A live handoff may be on the bus.
        {
            C::SdoClient c; CHECK(c.configure(0, route(), true, true));
            const C::SdoRequestId id = admit(c, work());
            take(c);
            CHECK(c.reportController(event, 1));
            CHECK(c.unresolved(0) && c.result(id)->effectUncertain);
        }
        // A reply can overtake its TX report: that frame stays possible until it is reported.
        for (unsigned reported = 0; reported < 2; ++reported) {
            C::SdoClient c; CHECK(c.configure(0, route(), true, true));
            const C::SdoRequestId id = admit(c, work());
            C::SdoTxId tx; take(c, 0, 0, &tx);
            CHECK(c.receive(reply(0x60), 1));
            if (reported) CHECK(c.noteTxRetired(tx));
            CHECK(c.reportController(event, 2));
            CHECK(c.unresolved(0) == !reported && c.result(id)->outcome == C::SdoOutcome::SUCCESS);
            CHECK(!c.result(id)->effectUncertain);
        }
        // A cleanup abort staged after a failed exchange stays quarantined.
        {
            C::SdoClient c; CHECK(c.configure(0, route(), true, true));
            C::SdoTransfer w = work(false); w.allowSegmentedUpload = false;
            const C::SdoRequestId id = admit(c, w); take(c);
            M::Frame initiation = reply(0x41); initiation.data[4] = 2;
            CHECK(c.receive(initiation, 1));
            CHECK(terminal(c, id, C::SdoOutcome::UNSUPPORTED).clientAbortQueued);
            CHECK(c.release(id));
            CHECK(c.reportController(event, 2));
            CHECK(c.unresolved(0) && !c.port(0)->queueOccupied);
        }
    }
}

// A grant armed before a loss is not revived by the marker that ends that loss. Reopening needs
// its own new authorization. The ordered server here is a fake premise.
void reopeningClearsArmedExperimentGrants() {
    for (unsigned download = 0; download < 2; ++download) {
        C::SdoClient c;
        CHECK(c.configure(0, route(), false, true));
        C::NetworkRoute network;
        network.binding = route().binding;
        network.generation = 5;
        network.heartbeatId = 0x701;
        network.emcyId = 0x81;
        CHECK(c.configureNetwork(0, network, C::NetworkObserver(), 0));
        C::SdoTransfer grant = work(download != 0);
        grant.operation = 60; grant.request = 61; grant.step = 62; grant.index = 0x2100;
        grant.readEffect = C::SdoReadEffect::UNQUALIFIED;
        CHECK(download ? c.authorizeDownloadExperiment(0, grant, 5, 77, 0)
                       : c.authorizeUploadExperiment(0, grant, 5, 77, 0));
        CHECK(c.checkAdmission(0, grant, 0)); // The grant is what admits it, with no evidence yet.
        C::SdoTransfer lost = drainMarker(0x2200);
        lost.responseTimeoutUs = 5;
        const C::SdoRequestId lostId = admit(c, lost); take(c);
        CHECK(c.service(5)); terminal(c, lostId, C::SdoOutcome::TIMEOUT);
        CHECK(c.release(lostId) && c.unresolved(0) && !c.checkAdmission(0, grant, 5));
        retireAbort(c, 5);
        C::SdoRequestId id;
        CHECK(c.admitDrain(0, drainMarker(), fakePremises(), 5, id, 1000));
        take(c, 5);
        M::Frame answer = named(0x4b, 0x2000);
        CHECK(c.receive(answer, 6));
        terminal(c, id, C::SdoOutcome::SUCCESS);
        CHECK(c.release(id) && c.reopenAfterDrain(0) && !c.unresolved(0));
        CHECK(!c.checkAdmission(0, grant, 6));
        C::SdoRequestId refused;
        CHECK(!c.admit(0, grant, 6, refused));
    }
}

// A wrong-object frame on a live transfer does not end it, but it is retained: a later marker
// cannot use that object and be settled by a reply that was already on its way.
void strayDuringOrdinaryTransferIsRetained() {
    C::SdoClient c; CHECK(c.configure(0, route(), true, true));
    C::SdoTransfer w = work(); w.responseTimeoutUs = 5;
    const C::SdoRequestId id = admit(c, w); take(c);
    CHECK(!c.receive(named(0x4b, 0x3000), 1));
    CHECK(!c.result(id));
    CHECK(c.service(5)); terminal(c, id, C::SdoOutcome::TIMEOUT);
    CHECK(c.release(id) && c.unresolved(0));
    CHECK(!c.checkDrain(0, drainMarker(), fakePremises(), 5)); // Its abort is not retired yet.
    retireAbort(c, 5);
    CHECK(c.checkDrain(0, drainMarker(), fakePremises(), 5));
    CHECK(!c.checkDrain(0, drainMarker(0x3000), fakePremises(), 5));
}

// Prompt 55: a response timeout after possible TX stages one abort through the existing
// client-abort path. Nothing when TX was never possible, when no budget remains, or for a
// best-effort stop. The abort is cleanup only and settles nothing.
void timeoutAbortIsCleanupOnly() {
    for (unsigned download = 0; download < 2; ++download) {
        C::SdoClient c; CHECK(c.configure(0, route(), true, true));
        if (PAIRED_CHANNELS) CHECK(c.configure(1, route(2), true, true));
        C::SdoTransfer lost = work(download != 0); lost.subindex = 3; lost.responseTimeoutUs = 5;
        if (!download) lost.readEffect = C::SdoReadEffect::ACCEPTED_UNQUALIFIED;
        const C::SdoRequestId id = admit(c, lost); take(c);
        CHECK(c.service(4) && !c.result(id) && !c.port(0)->queueOccupied);
        CHECK(c.service(5));
        const C::SdoCompletion& timedOut = terminal(c, id, C::SdoOutcome::TIMEOUT);
        // TIMEOUT stays the cause and carries no abort code; the staged abort is cleanup.
        CHECK(timedOut.clientAbortQueued && !timedOut.abortValid && timedOut.abortCode == 0);
        CHECK(timedOut.transmissionPossible && timedOut.effectUncertain && !timedOut.protocolAcknowledged);
        CHECK(c.unresolved(0) && c.port(0)->queueOccupied);
        if (PAIRED_CHANNELS) {
            // The other channel runs its own exchange meanwhile.
            C::SdoTransfer other = work(); other.channel = route(2);
            const C::SdoRequestId otherId = admit(c, other, 5, 1); take(c, 5, 1);
            CHECK(c.receive(reply(0x60, route(2)), 5));
            CHECK(terminal(c, otherId, C::SdoOutcome::SUCCESS).protocolAcknowledged && !c.unresolved(1));
        }
        CHECK(c.release(id));
        retireAbort(c, 6, 0x2200, 3);
        CHECK(c.port(0)->handedOff == 2);
        blocked(c, work(), 7); // It settled nothing.
        CHECK(c.service(10000) && c.port(0)->handedOff == 2 && c.unresolved(0));
    }
    {   // Not handed off within its own queue budget: retired and never sent later.
        C::SdoClient c; CHECK(c.configure(0, route(), true, true));
        C::SdoTransfer lost = work(); lost.responseTimeoutUs = 5;
        const C::SdoRequestId id = admit(c, lost); take(c);
        CHECK(c.service(5) && terminal(c, id, C::SdoOutcome::TIMEOUT).clientAbortQueued);
        CHECK(c.service(1005) && !c.port(0)->queueOccupied);
        M::Frame f; C::SdoTxId tx;
        CHECK(!c.nextTx(0, 1006, f, tx) && c.port(0)->handedOff == 1 && c.unresolved(0));
    }
    {   // Queue expiry before any handoff: no transmission was possible, so no abort.
        C::SdoClient c; CHECK(c.configure(0, route(), true, true));
        const C::SdoRequestId id = admit(c, work());
        CHECK(c.service(0) && c.port(0)->queueOccupied && c.service(1000));
        const C::SdoCompletion& expired = terminal(c, id, C::SdoOutcome::TIMEOUT);
        CHECK(!expired.transmissionPossible && !expired.clientAbortQueued);
        CHECK(!c.port(0)->queueOccupied && c.port(0)->handedOff == 0 && !c.unresolved(0));
    }
    {   // A handoff an adapter held short of its SDK never left either.
        C::SdoClient c; CHECK(c.configure(0, route(), true, true));
        const C::SdoRequestId id = admit(c, work());
        C::SdoTxId tx; take(c, 0, 0, &tx);
        CHECK(c.holdHandoff(tx) && c.service(1000));
        const C::SdoCompletion& expired = terminal(c, id, C::SdoOutcome::TIMEOUT);
        CHECK(!expired.transmissionPossible && !expired.clientAbortQueued && !c.port(0)->queueOccupied);
        CHECK(!c.unresolved(0));
    }
    {   // At the overall deadline no budget remains for cleanup.
        C::SdoClient c; CHECK(c.configure(0, route(), true, true));
        C::SdoTransfer w = work(); w.deadlineUs = 5;
        const C::SdoRequestId id = admit(c, w); take(c);
        CHECK(c.service(5));
        const C::SdoCompletion& expired = terminal(c, id, C::SdoOutcome::TIMEOUT);
        CHECK(expired.transmissionPossible && !expired.clientAbortQueued);
        CHECK(!c.port(0)->queueOccupied && c.unresolved(0));
    }
    {   // A best-effort stop sends exactly one frame (DEC-2), even when it times out.
        C::SdoClient c; CHECK(c.configure(0, route(), true, true));
        C::SdoTransfer stop = work(); stop.index = 0x6040; stop.subindex = 0;
        stop.data[0] = 0x0b; stop.data[1] = 0; stop.responseTimeoutUs = 5;
        C::SdoRequestId id;
        CHECK(c.admitBestEffortStop(0, stop, 0, id, 1000));
        take(c);
        CHECK(c.service(5));
        CHECK(!terminal(c, id, C::SdoOutcome::TIMEOUT).clientAbortQueued && !c.port(0)->queueOccupied);
        CHECK(c.port(0)->handedOff == 1);
    }
}

// The accepted no-marker setting settles after local retirement alone, through the one
// reopening. Its premises stay stated, never established.
void settlementWithoutMarker() {
    C::SdoClient c; CHECK(c.configure(0, route(), true, true));
    C::SdoTransfer lost = work(false); lost.readEffect = C::SdoReadEffect::ACCEPTED_UNQUALIFIED;
    lost.responseTimeoutUs = 5;
    const C::SdoRequestId id = admit(c, lost); take(c);
    CHECK(c.service(5)); CHECK(!terminal(c, id, C::SdoOutcome::TIMEOUT).acceptedCorrelation);
    CHECK(c.release(id));
    CHECK(!c.settleWithoutMarker(0, C::SdoDrainPremises(), 5)); // Unstated premises (NONE).
    CHECK(!c.settleWithoutMarker(0, fakePremises(), 5));        // Its abort is still staged.
    C::SdoTxId abortTx; take(c, 5, 0, &abortTx);
    CHECK(!c.settleWithoutMarker(0, fakePremises(), 5));        // Handed off, not yet retired.
    CHECK(!c.settleWithoutMarker(C::SdoClient::CHANNELS, fakePremises(), 5));
    CHECK(c.noteTxRetired(abortTx));
    CHECK(c.settleWithoutMarker(0, fakePremises(), 6));
    CHECK(c.drainSettled(0) && c.unresolved(0) && !c.settleWithoutMarker(0, fakePremises(), 6));
    CHECK(c.reopenAfterDrain(0) && !c.unresolved(0) && c.requalifications(0) == 1);
    CHECK(c.acceptedCorrelation(0) && !c.acceptedCorrelation(C::SdoClient::CHANNELS));
    // D2: the timed-out request's own late reply, idle, is absorbed after a no-marker reopening.
    CHECK(!c.receive(reply(0x4b), 7) && !c.unresolved(0) && !c.reuseRefused(0));
    // A(x) again: the old A(x) reply completes it, and no client check can tell.
    C::SdoTransfer again = lost; ++again.request; ++again.step; again.responseTimeoutUs = 1000;
    const C::SdoRequestId next = admit(c, again, 7);
    C::SdoTxId nextTx; take(c, 7, 0, &nextTx);
    M::Frame old = reply(0x4b); old.data[4] = 0x11; old.data[5] = 0x22;
    CHECK(c.receive(old, 8));
    const C::SdoCompletion& result = terminal(c, next, C::SdoOutcome::SUCCESS);
    CHECK(result.protocolAcknowledged && result.size == 2 && result.data[0] == 0x11 && !c.unresolved(0));
    CHECK(result.acceptedCorrelation); // M8: the client's own result carries the mark.
    CHECK(c.release(next) && c.noteTxRetired(nextTx));
    // An unexpected frame after the reopening refuses reuse; an explicit re-acceptance resumes it.
    CHECK(!c.receive(named(0x4b, 0x3000), 9));
    CHECK(c.unresolved(0) && c.reuseRefused(0));
    CHECK(!c.settleWithoutMarker(0, fakePremises(), 9) && !c.checkDrain(0, drainMarker(), fakePremises(), 9));
    CHECK(!c.reuseRefused(C::SdoClient::CHANNELS) && !c.reacceptReuse(C::SdoClient::CHANNELS));
    CHECK(c.reacceptReuse(0) && !c.reuseRefused(0) && !c.reacceptReuse(0) && c.unresolved(0));
    CHECK(c.settleWithoutMarker(0, fakePremises(), 10) && c.reopenAfterDrain(0));
    CHECK(c.requalifications(0) == 2 && !c.unresolved(0));
    // A controller event refuses the settlement below the owner too.
    C::SdoClient d; quarantine(d);
    CHECK(d.reportController(C::ControllerState::RECOVERING, 6) && !d.settleWithoutMarker(0, fakePremises(), 6));
    CHECK(d.reportController(C::ControllerState::AVAILABLE, 7) && d.settleWithoutMarker(0, fakePremises(), 7));
}

// The owner-level reuse policy (prompt 55). Every premise is stated by this fake; a positive
// proves the host's logic and the accepted risk, never a real server's ordering (Q18).
constexpr unsigned BENCH_NODES = PAIRED_CHANNELS ? 2 : 1;
const uint32_t DEVICE_TYPE = 0x00020192; // A synthetic 1000:00 value, no real drive's.
struct Bench {
    C::SdoClient services;
    OwnerRig::WireBase wire;
    O::CanBusOwner owner;
    uint64_t now = 0;
    unsigned steps[7] = {}, kinds[9] = {};
    explicit Bench(O::SdoReusePolicy policy, bool expect = false, uint32_t expected = DEVICE_TYPE)
        : owner(services, wire.plumbing(), 55) {
        for (unsigned node = 1; node <= BENCH_NODES; ++node) {
            CHECK(services.configure(uint8_t(node - 1), OwnerRig::route(node), true, true));
            C::NetworkRoute n;
            n.binding = OwnerRig::route(node).binding;
            n.generation = 11;
            n.heartbeatId = 0x700 + node;
            n.emcyId = 0x80 + node;
            CHECK(services.configureNetwork(uint8_t(node - 1), n, C::NetworkObserver(), 0));
            O::SdoReuse reuse;
            CHECK(reuse.policy == O::SdoReusePolicy::ACCEPTED_RISK_WITH_MARKER); // The default.
            // The default list: 1000:00, then 1018:01..03, every one U32, and inherited budgets.
            CHECK(reuse.markerCount == O::SdoReuse::MARKER_OBJECTS && O::SdoReuse::MARKER_OBJECTS == 4);
            CHECK(reuse.markers[0].index == 0x1000 && reuse.markers[0].subindex == 0);
            for (unsigned i = 1; i < 4; ++i)
                CHECK(reuse.markers[i].index == 0x1018 && reuse.markers[i].subindex == i);
            for (unsigned i = 0; i < 4; ++i)
                CHECK(reuse.markers[i].size == 4 && !reuse.markers[i].expectedKnown);
            CHECK(reuse.markerQueueTimeoutUs == 0 && reuse.markerResponseTimeoutUs == 0);
            reuse.policy = policy;
            reuse.markers[0].expectedKnown = expect;
            reuse.markers[0].expectedValue = expected;
            CHECK(owner.setSdoReuse(OwnerRig::route(node).binding, reuse, 0));
        }
    }
    const M::Binding& binding(unsigned node = 1) const { return services.channel(uint8_t(node - 1))->binding; }
    void turn(uint64_t time, O::ServiceBudget b = OwnerRig::budget()) {
        now = time;
        CHECK(owner.service(time, b));
        O::Diagnostic d;
        while (owner.takeDiagnostic(d)) {
            ++kinds[unsigned(d.kind)];
            if (d.kind == O::DiagnosticKind::RESYNC) ++steps[unsigned(d.resync)];
        }
        CHECK(owner.stats().traceOverwritten == 0);
    }
    O::SequenceId reserve(unsigned node, uint32_t operation) {
        O::SequenceId id;
        CHECK(owner.beginSequence(binding(node), uint8_t(node), now, id));
        CHECK(owner.beginOperation(id, operation, now + 100000, now));
        return id;
    }
    O::RequestId read(O::SequenceId s, uint16_t index, unsigned node = 1, uint32_t responseUs = 1000) {
        const O::Sequence* q = owner.sequence(s);
        CHECK(q);
        O::BusRequest r;
        r.sequence = s;
        r.transfer.channel = *services.channel(uint8_t(node - 1));
        r.transfer.operation = r.transfer.request = q->operation;
        r.transfer.step = 1;
        r.transfer.index = index;
        r.transfer.size = 4;
        r.transfer.direction = C::SdoDirection::UPLOAD;
        r.transfer.readEffect = C::SdoReadEffect::ACCEPTED_UNQUALIFIED;
        r.transfer.queueTimeoutUs = 1000;
        r.transfer.responseTimeoutUs = responseUs;
        r.transfer.deadlineUs = q->deadlineUs;
        O::RequestId id;
        CHECK(owner.admit(r, now, id) == O::BusAdmission::ACCEPTED);
        return id;
    }
    C::SdoTransfer marker1018(O::SequenceId s) const {
        const O::Sequence* q = owner.sequence(s);
        CHECK(q);
        C::SdoTransfer t;
        t.channel = *services.channel(0);
        t.operation = t.request = q->operation;
        t.step = 1;
        t.index = 0x1018; t.subindex = 1; t.size = 4;
        t.direction = C::SdoDirection::UPLOAD;
        t.readEffect = C::SdoReadEffect::ACCEPTED_UNQUALIFIED;
        t.queueTimeoutUs = t.responseTimeoutUs = 1000;
        t.deadlineUs = q->deadlineUs;
        return t;
    }
    M::Frame upload(unsigned node, uint16_t index, uint32_t value, uint8_t subindex = 0) const {
        M::Frame f = OwnerRig::frame(0x580 + node, 8);
        f.data[0] = 0x43;
        f.data[1] = uint8_t(index); f.data[2] = uint8_t(index >> 8); f.data[3] = subindex;
        for (unsigned i = 0; i < 4; ++i) f.data[4 + i] = uint8_t(value >> (8 * i));
        return f;
    }
    void answer(unsigned node, uint16_t index, uint32_t value, uint8_t subindex = 0) {
        wire.push(upload(node, index, value, subindex), now + 1);
        turn(now + 1);
    }
    const O::Completion& done(O::RequestId id) const {
        const O::Completion* c = owner.result(id);
        CHECK(c);
        return *c;
    }
    O::Health health(unsigned node = 1) const {
        O::Health h;
        CHECK(owner.health(binding(node), h));
        return h;
    }
    void expectSent(unsigned index, const uint8_t (&bytes)[8], unsigned node = 1) const {
        CHECK(index < wire.sentCount && wire.sent[index].id == 0x600 + node);
        CHECK(std::memcmp(wire.sent[index].data, bytes, 8) == 0);
    }
};
const uint8_t READ_6064[8] = {0x40, 0x64, 0x60, 0, 0, 0, 0, 0};
const uint8_t ABORT_6064[8] = {0x80, 0x64, 0x60, 0, 0, 0, 0x04, 0x05};
const uint8_t MARKER_1000[8] = {0x40, 0x00, 0x10, 0, 0, 0, 0, 0};
const uint8_t MARKER_1018[8] = {0x40, 0x18, 0x10, 1, 0, 0, 0, 0};

// A(x) times out; its abort leaves once. Returns after the abort's report is consumed.
O::SequenceId timeOut(Bench& b, O::RequestId& lost) {
    const O::SequenceId s = b.reserve(1, 1);
    lost = b.read(s, 0x6064);
    b.turn(0);
    CHECK(b.wire.sentCount == 1);
    b.expectSent(0, READ_6064);
    b.turn(999);
    CHECK(!b.owner.result(lost) && b.wire.sentCount == 1);
    b.turn(1000);
    const O::Completion& timedOut = b.done(lost);
    CHECK(timedOut.service.outcome == C::SdoOutcome::TIMEOUT && timedOut.service.clientAbortQueued);
    CHECK(timedOut.service.transmissionPossible && !timedOut.acceptedCorrelation);
    CHECK(b.wire.sentCount == 2 && b.services.unresolved(0));
    b.expectSent(1, ABORT_6064);
    CHECK(b.health().consecutiveNoResponse == 1 && b.health().resyncPending ==
          (timedOut.reusePolicy != O::SdoReusePolicy::NONE));
    CHECK(b.owner.release(lost));
    return s;
}
// New A(x) on the reopened route; the old A(x) reply completes it and carries the mark.
void lateSameObjectReply(Bench& b, O::SdoReusePolicy policy) {
    CHECK(!b.services.unresolved(0) && b.health().acceptedCorrelation);
    const unsigned sent = b.wire.sentCount;
    const O::SequenceId s = b.reserve(1, 2);
    const O::RequestId again = b.read(s, 0x6064);
    b.turn(b.now + 1);
    CHECK(b.wire.sentCount == sent + 1);
    b.expectSent(sent, READ_6064);
    b.answer(1, 0x6064, 0x1111); // The reply to the first, lost A(x), arriving late.
    const O::Completion& result = b.done(again);
    CHECK(result.service.outcome == C::SdoOutcome::SUCCESS && result.service.protocolAcknowledged);
    CHECK(result.service.data[0] == 0x11 && result.service.data[1] == 0x11);
    // The result says what its correlation rests on, and claims no detection. The client's
    // own SdoCompletion carries the same mark, so profile contexts that copy it keep it.
    CHECK(result.acceptedCorrelation && result.service.acceptedCorrelation && result.reusePolicy == policy);
    CHECK(!b.services.unresolved(0) && !b.services.reuseRefused(0) && b.owner.stats().reuseStopped == 0);
    CHECK(b.owner.release(again) && b.owner.releaseSequence(s));
}
// An unexpected frame after a reopening stops reuse, idle or while a request waits (that request
// ends CORRELATION_LOST); the explicit re-acceptance resumes it once.
void contradictionStopsReuse(Bench& b, bool marker, bool waiting, uint16_t contradicting) {
    const uint32_t requalified = b.services.requalifications(0);
    const uint32_t stopped = b.owner.stats().reuseStopped;
    const unsigned stoppedSteps = b.steps[unsigned(O::ResyncStep::REUSE_STOPPED)];
    const unsigned acceptedSteps = b.steps[unsigned(O::ResyncStep::REUSE_ACCEPTED)];
    O::SequenceId held;
    O::RequestId pending;
    if (waiting) {
        held = b.reserve(1, 3);
        pending = b.read(held, 0x6041);
        b.turn(b.now + 1);
        CHECK(!b.owner.result(pending) && !b.services.unresolved(0));
    }
    const unsigned sent = b.wire.sentCount;
    b.wire.push(b.upload(1, contradicting, 7), b.now + 1);
    b.turn(b.now + 1);
    CHECK(b.services.unresolved(0) && b.services.reuseRefused(0));
    CHECK(b.owner.stats().reuseStopped == stopped + 1);
    CHECK(b.steps[unsigned(O::ResyncStep::REUSE_STOPPED)] == stoppedSteps + 1);
    CHECK(b.health().reuseStopped && !b.health().resyncPending);
    if (waiting) {
        // The waiting request cannot stand on the contradicted premise, not even with its own
        // reply arriving afterwards.
        CHECK(b.done(pending).service.outcome == C::SdoOutcome::CORRELATION_LOST);
        CHECK(b.done(pending).acceptedCorrelation && !b.done(pending).service.protocolAcknowledged);
        b.answer(1, 0x6041, 5);
        CHECK(b.done(pending).service.outcome == C::SdoOutcome::CORRELATION_LOST && b.services.unresolved(0));
        CHECK(b.owner.release(pending) && b.owner.releaseSequence(held));
    }
    C::SdoTransfer explicitMarker;
    explicitMarker.channel = *b.services.channel(0);
    explicitMarker.operation = explicitMarker.request = explicitMarker.step = 77;
    explicitMarker.index = 0x1018; explicitMarker.subindex = 1; explicitMarker.size = 4;
    explicitMarker.direction = C::SdoDirection::UPLOAD;
    explicitMarker.readEffect = C::SdoReadEffect::ACCEPTED_UNQUALIFIED;
    explicitMarker.allowSegmentedUpload = false; // Otherwise a valid marker in every other way.
    explicitMarker.queueTimeoutUs = explicitMarker.responseTimeoutUs = 1000;
    explicitMarker.deadlineUs = b.now + 5000;
    O::RequestId refused;
    CHECK(b.owner.admitDrainMarker(explicitMarker, b.now, refused) == O::BusAdmission::INVALID);
    for (unsigned i = 0; i < 5; ++i) b.turn(b.now + 1000);
    CHECK(b.wire.sentCount == sent && b.services.unresolved(0)); // Nothing by itself.
    CHECK(b.owner.reacceptReuse(b.binding(), b.now) && !b.owner.reacceptReuse(b.binding(), b.now));
    CHECK(b.steps[unsigned(O::ResyncStep::REUSE_ACCEPTED)] == acceptedSteps); // Not yet drained.
    b.turn(b.now + 1);
    CHECK(b.steps[unsigned(O::ResyncStep::REUSE_ACCEPTED)] == acceptedSteps + 1 && !b.health().reuseStopped);
    if (marker) {
        // 1000:00, or 1018:01 when 1000:00 completed last (an earlier marker).
        CHECK(b.wire.sentCount == sent + 1);
        const bool alternate = b.wire.sent[sent].data[1] == 0x18;
        b.expectSent(sent, alternate ? MARKER_1018 : MARKER_1000);
        b.answer(1, alternate ? 0x1018 : 0x1000, DEVICE_TYPE, alternate ? 1 : 0);
    }
    CHECK(b.wire.sentCount == sent + (marker ? 1 : 0));
    CHECK(!b.services.unresolved(0) && b.services.requalifications(0) == requalified + 1);
}

void reusePolicyAfterTimeout() {
    {   // ACCEPTED_RISK_WITH_MARKER, the default: the owner issues one marker of another object.
        Bench b(O::SdoReusePolicy::ACCEPTED_RISK_WITH_MARKER, true);
        O::RequestId lost;
        const O::SequenceId first = timeOut(b, lost);
        O::RequestId other;
        if (PAIRED_CHANNELS) other = b.read(b.reserve(2, 1), 0x6064, 2);
        b.turn(1001); // The abort is retired: the marker goes, and node 2 is served alongside.
        CHECK(b.wire.sentCount == (PAIRED_CHANNELS ? 4u : 3u));
        b.expectSent(2, MARKER_1000);
        CHECK(b.owner.stats().markers == 1 && b.steps[unsigned(O::ResyncStep::MARKER_ADMITTED)] == 1);
        CHECK(!b.health().resyncPending && b.services.unresolved(0));
        if (PAIRED_CHANNELS) {
            b.expectSent(3, READ_6064, 2);
            b.answer(2, 0x6064, 5);
            CHECK(b.done(other).service.outcome == C::SdoOutcome::SUCCESS && !b.done(other).acceptedCorrelation);
            CHECK(!b.services.unresolved(1) && b.services.requalifications(1) == 0);
        }
        b.answer(1, 0x6064, 0x1111); // The old A(x) reply during the marker is a retained pair: absorbed.
        CHECK(b.services.unresolved(0) && b.services.requalifications(0) == 0);
        b.answer(1, 0x1000, DEVICE_TYPE);
        CHECK(b.services.requalifications(0) == 1 && b.steps[unsigned(O::ResyncStep::REOPENED)] == 1);
        CHECK(!b.owner.sequence(first)); // Reservations on the node were invalidated.
        CHECK(b.owner.resources().retained == (PAIRED_CHANNELS ? 1 : 0)); // The marker slot is free.
        lateSameObjectReply(b, O::SdoReusePolicy::ACCEPTED_RISK_WITH_MARKER);
        contradictionStopsReuse(b, true, false, 0x2000);
        contradictionStopsReuse(b, true, true, 0x2001);
    }
    {   // ACCEPTED_RISK_WITHOUT_MARKER: local retirement alone reopens; nothing else is sent.
        Bench b(O::SdoReusePolicy::ACCEPTED_RISK_WITHOUT_MARKER);
        O::RequestId lost;
        timeOut(b, lost);
        b.turn(1001);
        CHECK(b.wire.sentCount == 2 && b.owner.stats().markers == 0);
        CHECK(b.services.requalifications(0) == 1 && b.steps[unsigned(O::ResyncStep::REOPENED)] == 1);
        lateSameObjectReply(b, O::SdoReusePolicy::ACCEPTED_RISK_WITHOUT_MARKER);
        contradictionStopsReuse(b, false, false, 0x2000);
        // 2000:00 is now a pair kept from before the no-marker reopening: another object.
        contradictionStopsReuse(b, false, true, 0x2001);
    }
    {   // NONE: the abort still leaves, then the route stays quarantined and refuses markers.
        Bench b(O::SdoReusePolicy::NONE);
        O::RequestId lost;
        timeOut(b, lost);
        for (unsigned i = 0; i < 5; ++i) b.turn(b.now + 1000);
        CHECK(b.wire.sentCount == 2 && b.services.unresolved(0) && b.services.requalifications(0) == 0);
        CHECK(b.health().reusePolicy == O::SdoReusePolicy::NONE && !b.health().resyncPending);
        C::SdoTransfer marker;
        marker.channel = *b.services.channel(0);
        marker.operation = marker.request = marker.step = 9;
        marker.index = 0x1000; marker.size = 4;
        marker.direction = C::SdoDirection::UPLOAD;
        marker.readEffect = C::SdoReadEffect::ACCEPTED_UNQUALIFIED;
        marker.allowSegmentedUpload = false;
        marker.queueTimeoutUs = marker.responseTimeoutUs = 1000;
        marker.deadlineUs = b.now + 5000;
        O::RequestId refused;
        CHECK(b.owner.admitDrainMarker(marker, b.now, refused) == O::BusAdmission::INVALID);
        C::SdoDrainPremises stated; stated.soleClient = stated.orderedServer = true;
        CHECK(b.owner.admitDrainMarker(marker, stated, b.now, refused) == O::BusAdmission::INVALID);
        CHECK(!b.owner.reacceptReuse(b.binding(), b.now) && b.wire.sentCount == 2);
        // Setting an accepted policy later reopens nothing by itself: the step was never armed.
        O::SdoReuse reuse;
        CHECK(b.owner.setSdoReuse(b.binding(), reuse, b.now));
        b.turn(b.now + 1);
        CHECK(b.wire.sentCount == 2 && b.services.unresolved(0));
        // An explicit marker is the application's own: its result is retained until release.
        O::RequestId explicitMarker;
        CHECK(b.owner.admitDrainMarker(marker, b.now, explicitMarker) == O::BusAdmission::ACCEPTED);
        CHECK(explicitMarker.slot >= O::CanBusOwner::REQUESTS && b.owner.stats().markers == 0);
        b.turn(b.now + 1);
        CHECK(b.wire.sentCount == 3);
        b.expectSent(2, MARKER_1000);
        b.answer(1, 0x1000, DEVICE_TYPE);
        const O::Completion& settled = b.done(explicitMarker);
        CHECK(settled.service.outcome == C::SdoOutcome::SUCCESS && !settled.automaticMarker);
        CHECK(!b.services.unresolved(0) && b.services.requalifications(0) == 1);
        CHECK(b.owner.release(explicitMarker));
    }
}

// D2: after a no-marker reopening the timed-out request's own late reply is the accepted risk
// until the route's first success: absorbed when idle or while another object is awaited.
void noMarkerKeepsRetainedPairs() {
    Bench b(O::SdoReusePolicy::ACCEPTED_RISK_WITHOUT_MARKER);
    O::RequestId lost;
    timeOut(b, lost);
    b.turn(1001);
    CHECK(!b.services.unresolved(0) && b.services.requalifications(0) == 1);
    b.answer(1, 0x6064, 0x1111); // Idle: absorbed.
    CHECK(!b.services.unresolved(0) && !b.services.reuseRefused(0) && b.owner.stats().reuseStopped == 0);
    const O::SequenceId s = b.reserve(1, 2);
    const O::RequestId other = b.read(s, 0x6041);
    b.turn(b.now + 1);
    b.answer(1, 0x6064, 0x2222); // While 6041:00 is awaited: absorbed, that request waits on.
    CHECK(!b.owner.result(other) && !b.services.unresolved(0) && !b.services.reuseRefused(0));
    b.answer(1, 0x6041, 0x0237);
    const O::Completion& done = b.done(other);
    CHECK(done.service.outcome == C::SdoOutcome::SUCCESS && done.acceptedCorrelation &&
          done.service.acceptedCorrelation);
    // That first success ends the kept pairs: the same late reply now contradicts the premise.
    b.answer(1, 0x6064, 0x3333);
    CHECK(b.services.unresolved(0) && b.services.reuseRefused(0) && b.owner.stats().reuseStopped == 1);
}

// The marker's checks are observable: another object's frame fails it, a configured value that
// differs invalidates the binding, and the alternate serves when the first object is retained.
void markerChecks() {
    {   // A late frame naming another object fails the marker; no further marker follows.
        Bench b(O::SdoReusePolicy::ACCEPTED_RISK_WITH_MARKER);
        O::RequestId lost;
        timeOut(b, lost);
        b.turn(1001);
        b.expectSent(2, MARKER_1000);
        b.answer(1, 0x2000, 3);
        CHECK(b.services.unresolved(0) && b.services.requalifications(0) == 0);
        CHECK(b.steps[unsigned(O::ResyncStep::MARKER_FAILED)] == 1);
        CHECK(b.owner.resources().retained == 0); // The owner's own result is not retained.
        for (unsigned i = 0; i < 5; ++i) b.turn(b.now + 1000);
        CHECK(b.wire.sentCount == 3 && b.services.unresolved(0) && !b.health().resyncPending);
    }
    {   // A value other than the commissioned one: another drive answers. Invalidate, not reopen.
        Bench b(O::SdoReusePolicy::ACCEPTED_RISK_WITH_MARKER, true);
        O::RequestId lost;
        timeOut(b, lost);
        const M::Generation generation = b.services.node(0)->route.generation;
        b.turn(1001);
        b.answer(1, 0x1000, DEVICE_TYPE + 1);
        CHECK(b.steps[unsigned(O::ResyncStep::MARKER_MISMATCH)] == 1);
        CHECK(b.steps[unsigned(O::ResyncStep::REOPENED)] == 0 && b.services.requalifications(0) == 0);
        CHECK(b.services.unresolved(0) && !b.services.drainSettled(0));
        CHECK(b.services.node(0)->route.generation != generation);
        b.turn(b.now + 10);
        CHECK(b.services.unresolved(0) && b.wire.sentCount == 3);
    }
    {   // The first object equals the last completed pair, so the alternate 1018:01 is read.
        Bench b(O::SdoReusePolicy::ACCEPTED_RISK_WITH_MARKER);
        const O::SequenceId s = b.reserve(1, 1);
        const O::RequestId first = b.read(s, 0x1000);
        b.turn(0);
        b.answer(1, 0x1000, DEVICE_TYPE);
        CHECK(b.done(first).service.outcome == C::SdoOutcome::SUCCESS && b.owner.release(first));
        CHECK(b.owner.beginOperation(s, 2, b.now + 100000, b.now));
        const O::RequestId lost = b.read(s, 0x6064);
        b.turn(10);
        b.turn(1010);
        CHECK(b.done(lost).service.outcome == C::SdoOutcome::TIMEOUT && b.owner.release(lost));
        b.turn(1011);
        CHECK(b.wire.sentCount == 4);
        b.expectSent(3, MARKER_1018);
        b.answer(1, 0x1018, 0x5a, 1);
        CHECK(!b.services.unresolved(0) && b.services.requalifications(0) == 1);
    }
    {   // M1: a second marker reply before the reopening withdraws the settlement: reported.
        Bench b(O::SdoReusePolicy::ACCEPTED_RISK_WITH_MARKER);
        O::RequestId lost;
        timeOut(b, lost);
        b.turn(1001);
        b.expectSent(2, MARKER_1000);
        b.wire.push(b.upload(1, 0x1000, DEVICE_TYPE), b.now + 1);
        b.wire.push(b.upload(1, 0x1000, DEVICE_TYPE), b.now + 1);
        b.turn(b.now + 1);
        CHECK(b.services.unresolved(0) && b.steps[unsigned(O::ResyncStep::REOPENED)] == 0);
        CHECK(b.steps[unsigned(O::ResyncStep::MARKER_FAILED)] == 1 && !b.health().resyncPending);
        for (unsigned i = 0; i < 5; ++i) b.turn(b.now + 1000);
        CHECK(b.wire.sentCount == 3 && b.steps[unsigned(O::ResyncStep::MARKER_FAILED)] == 1);
    }
    {   // M4: every listed object is a pair the route remembers: the step reports instead of
        // waiting. With the list cut to 1000:00 and 1018:01 here; the default list goes on below.
        Bench b(O::SdoReusePolicy::ACCEPTED_RISK_WITH_MARKER);
        O::SdoReuse two;
        two.markerCount = 2;
        CHECK(b.owner.setSdoReuse(b.binding(), two, 0));
        const O::SequenceId s = b.reserve(1, 1);
        const O::RequestId first = b.read(s, 0x1000);
        b.turn(0);
        b.answer(1, 0x1000, DEVICE_TYPE);
        CHECK(b.owner.release(first) && b.owner.beginOperation(s, 2, b.now + 100000, b.now));
        O::BusRequest lostRead;
        lostRead.sequence = s;
        lostRead.transfer = b.marker1018(s);
        O::RequestId lost;
        CHECK(b.owner.admit(lostRead, b.now, lost) == O::BusAdmission::ACCEPTED);
        b.turn(10);
        b.turn(1010);
        CHECK(b.done(lost).service.outcome == C::SdoOutcome::TIMEOUT && b.owner.release(lost));
        b.turn(1011);
        CHECK(b.steps[unsigned(O::ResyncStep::MARKER_UNAVAILABLE)] == 1 && !b.health().resyncPending);
        CHECK(b.wire.sentCount == 3 && b.owner.stats().markers == 0 && b.services.unresolved(0));
    }
    {   // The list is tried in order: 1000:00 completed last and 1018:01 is retained, so the
        // default list reads 1018:02.
        Bench b(O::SdoReusePolicy::ACCEPTED_RISK_WITH_MARKER);
        const O::SequenceId s = b.reserve(1, 1);
        const O::RequestId first = b.read(s, 0x1000);
        b.turn(0);
        b.answer(1, 0x1000, DEVICE_TYPE);
        CHECK(b.owner.release(first) && b.owner.beginOperation(s, 2, b.now + 100000, b.now));
        O::BusRequest lostRead;
        lostRead.sequence = s;
        lostRead.transfer = b.marker1018(s);
        O::RequestId lost;
        CHECK(b.owner.admit(lostRead, b.now, lost) == O::BusAdmission::ACCEPTED);
        b.turn(10);
        b.turn(1010);
        CHECK(b.done(lost).service.outcome == C::SdoOutcome::TIMEOUT && b.owner.release(lost));
        b.turn(1011);
        const uint8_t marker1018_02[8] = {0x40, 0x18, 0x10, 2, 0, 0, 0, 0};
        CHECK(b.wire.sentCount == 4 && b.owner.stats().markers == 1);
        b.expectSent(3, marker1018_02);
        b.answer(1, 0x1018, 0x5a, 2);
        CHECK(!b.services.unresolved(0) && b.services.requalifications(0) == 1);
    }
    {   // Marker budgets: the lost read had a deliberately short response budget. Inherited, the
        // marker times out the same way; configured, it outlasts the drive's ordinary reply time.
        for (unsigned configured = 0; configured < 2; ++configured) {
            Bench b(O::SdoReusePolicy::ACCEPTED_RISK_WITH_MARKER);
            O::SdoReuse reuse;
            if (configured)
                reuse.markerResponseTimeoutUs = 5000;
            CHECK(b.owner.setSdoReuse(b.binding(), reuse, 0));
            const O::RequestId lost = b.read(b.reserve(1, 1), 0x6064, 1, 50);
            b.turn(0);
            b.turn(50);
            CHECK(b.done(lost).service.outcome == C::SdoOutcome::TIMEOUT && b.owner.release(lost));
            b.turn(51); // The abort's report: the marker goes.
            CHECK(b.wire.sentCount == 3);
            b.expectSent(2, MARKER_1000);
            b.turn(51 + 1000); // A reply 1 ms later, typical of the bench drive's SDO server.
            b.answer(1, 0x1000, DEVICE_TYPE);
            CHECK(b.services.unresolved(0) == !configured);
            CHECK(b.steps[unsigned(O::ResyncStep::MARKER_FAILED)] == unsigned(!configured));
        }
    }
    {   // Marker settings are validated before anything changes.
        Bench b(O::SdoReusePolicy::ACCEPTED_RISK_WITH_MARKER);
        O::SdoReuse bad;
        bad.markerCount = 0;
        CHECK(!b.owner.setSdoReuse(b.binding(), bad, 0));
        bad.markerCount = O::SdoReuse::MARKER_OBJECTS + 1;
        CHECK(!b.owner.setSdoReuse(b.binding(), bad, 0));
        bad = O::SdoReuse();
        bad.markers[3] = bad.markers[1]; // A duplicate pair.
        CHECK(!b.owner.setSdoReuse(b.binding(), bad, 0));
        bad.markerCount = 3; // The duplicate is no longer used.
        CHECK(b.owner.setSdoReuse(b.binding(), bad, 0));
        bad = O::SdoReuse();
        bad.markers[2].size = 5;
        CHECK(!b.owner.setSdoReuse(b.binding(), bad, 0));
        bad = O::SdoReuse();
        bad.markers[1].index = 0;
        CHECK(!b.owner.setSdoReuse(b.binding(), bad, 0));
        bad = O::SdoReuse();
        bad.markers[0].size = 2;
        bad.markers[0].expectedKnown = true;
        bad.markers[0].expectedValue = 0x10000; // Does not fit two bytes.
        CHECK(!b.owner.setSdoReuse(b.binding(), bad, 0));
        O::SdoReuse kept;
        CHECK(b.owner.sdoReuse(b.binding(), kept) && kept.markerCount == 3);
    }
    {   // M4: a re-acceptance on a route with no earlier request has no budget: reported.
        Bench b(O::SdoReusePolicy::ACCEPTED_RISK_WITH_MARKER);
        b.wire.push(OwnerRig::frame(0x581, 4), 1); // Names no object: refuses reuse.
        b.turn(1);
        CHECK(b.services.unresolved(0) && b.services.reuseRefused(0));
        CHECK(b.owner.reacceptReuse(b.binding(), b.now));
        b.turn(2);
        CHECK(b.steps[unsigned(O::ResyncStep::MARKER_UNAVAILABLE)] == 1 && !b.health().resyncPending);
        CHECK(b.wire.sentCount == 0 && b.services.unresolved(0));
        // With configured marker budgets the same re-acceptance builds its marker.
        O::SdoReuse budgeted;
        budgeted.markerQueueTimeoutUs = budgeted.markerResponseTimeoutUs = 2000;
        CHECK(b.owner.setSdoReuse(b.binding(), budgeted, b.now));
        b.wire.push(OwnerRig::frame(0x581, 4), b.now + 1);
        b.turn(b.now + 1);
        CHECK(b.services.reuseRefused(0) && b.owner.reacceptReuse(b.binding(), b.now));
        b.turn(b.now + 1);
        CHECK(b.wire.sentCount == 1 && b.owner.stats().markers == 1);
        b.expectSent(0, MARKER_1000);
    }
}

// C2: a frame naming no object while a request waits refuses reuse, as on the idle path.
void shortFrameWhileWaitingRefusesReuse() {
    C::SdoClient c; CHECK(c.configure(0, route(), true, true));
    const C::SdoRequestId id = admit(c, work(false)); take(c);
    M::Frame shortFrame = reply(0x43);
    shortFrame.dlc = shortFrame.payloadLength = 4;
    CHECK(c.receive(shortFrame, 1));
    CHECK(terminal(c, id, C::SdoOutcome::CLIENT_ABORT).clientAbortQueued);
    CHECK(c.unresolved(0) && c.reuseRefused(0));
    CHECK(!c.settleWithoutMarker(0, fakePremises(), 2));
}
} // namespace

int main() {
    cancelEveryTxBoundary(); failedPremisesCannotReopen(); controllerCacheSurvivesLocalInvalidation();
    cleanupEligibilityAfterReleaseAndInvalidation();
    if (PAIRED_CHANNELS) distinctRouteAndUnrelatedEvents();
    partialWorkflowAndReadEffects();
    duplicateSegmentsAndSameObjectPayloads();
    toggleWrapAmbiguityLimitation();
    capturedAndProcessingTime(); terminalPrecedenceAndLaterObservations();
    sameTimeTerminalOrdering(); segmentCaptureAndBorrowedObservation(); extendedObservationPreservesLiveRequest();
    staleBeforeHandoffAndObservedDuplicates();
    if (PAIRED_CHANNELS) transmitEligibilityAndBindingInvalidation();
    oneHandoffCannotBecomeNewIntent();
    nonwrappingTimeAndZeroCapture();
    drainMarkerClientBoundary();
    controllerEventsSamplePossibleWork();
    reopeningClearsArmedExperimentGrants();
    strayDuringOrdinaryTransferIsRetained();
    timeoutAbortIsCleanupOnly();
    settlementWithoutMarker();
    reusePolicyAfterTimeout();
    noMarkerKeepsRetainedPairs();
    markerChecks();
    shortFrameWhileWaitingRefusesReuse();
    std::printf("SDO reconciliation: %u checks\n", checks);
    std::printf("Host sizeof: SdoClient=%zu, SdoRxObservation=%zu bytes\n",
                sizeof(C::SdoClient), sizeof(C::SdoRxObservation));
}
