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
#define CHECK(condition) do { ++checks; if (!(condition)) { std::fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #condition); std::abort(); } } while (false)

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
    // download: admitted, staged initiation, handed-off initiation, staged
    // segment, handed-off segment, staged final segment, handed-off final.
    for (unsigned phase = 0; phase < 7; ++phase) {
        C::SdoClient c; CHECK(c.configure(0, route(), true, true));
        const C::SdoRequestId id = admit(c, work(true, 10));
        C::SdoTxId latest;
        if (phase >= 1) CHECK(c.service(0));
        if (phase >= 2) CHECK(take(c, 1, 0, &latest).data[0] == 0x21);
        if (phase >= 3) CHECK(c.receive(reply(0x60), 2));
        if (phase >= 4) CHECK(take(c, 3, 0, &latest).data[0] == 0x00);
        if (phase >= 5) CHECK(c.receive(reply(0x20), 4));
        if (phase >= 6) CHECK(take(c, 5, 0, &latest).data[0] == 0x19);
        const uint32_t handed = c.port(0)->handedOff;
        CHECK(c.cancel(id, 6));
        const C::SdoCompletion saved = terminal(c, id, C::SdoOutcome::CANCELLED);
        CHECK(saved.transmissionPossible == (phase >= 2));
        CHECK(saved.effectPossible == (phase >= 2) && saved.effectUncertain == (phase >= 2));
        CHECK(saved.transfer.size == 10 && saved.transfer.data[9] == 10);
        CHECK(!c.port(0)->queueOccupied);
        CHECK(!c.canTransmit(latest, 7));
        CHECK(!c.confirmTx(latest, 7));
        if (phase >= 2) CHECK(!c.receive(reply(phase >= 6 ? 0x30 : phase >= 4 ? 0x20 : 0x60), 8));
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
    CHECK(!c.canTransmit(cleanup, 4)); // Cleanup staging cutoff remains fixed after release/handoff.
    blocked(c, work(), 3);
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
} // namespace

int main() {
    cancelEveryTxBoundary(); failedPremisesCannotReopen(); controllerCacheSurvivesLocalInvalidation();
    cleanupEligibilityAfterReleaseAndInvalidation();
    distinctRouteAndUnrelatedEvents(); partialWorkflowAndReadEffects();
    duplicateSegmentsAndSameObjectPayloads();
    capturedAndProcessingTime(); terminalPrecedenceAndLaterObservations();
    sameTimeTerminalOrdering(); segmentCaptureAndBorrowedObservation(); extendedObservationPreservesLiveRequest();
    staleBeforeHandoffAndObservedDuplicates(); transmitEligibilityAndBindingInvalidation();
    oneHandoffCannotBecomeNewIntent();
    nonwrappingTimeAndZeroCapture();
    std::printf("SDO reconciliation: %u checks\n", checks);
    std::printf("Host sizeof: SdoClient=%zu, SdoRxObservation=%zu bytes\n",
                sizeof(C::SdoClient), sizeof(C::SdoRxObservation));
}
