// SPDX-License-Identifier: MIT
// Pins the private evidence checks directly. Their oracles are written out here, not derived from
// the helpers, and the public read, settings and action suites exercise them through real callers.
#include "canopen/SdoEvidence.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <initializer_list>

namespace C = MotorControlCAN::CANOPEN;
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

// The former spellings disagreed only when one range was empty. An empty range now shares nothing,
// in either argument order, including when it points inside the other range.
void overlapBehavior() {
    unsigned char bytes[32] = {};
    unsigned char* p = bytes + 8;
    CHECK(C::overlaps(p, 4, p, 4));
    CHECK(C::overlaps(p, 4, p + 3, 4));
    CHECK(C::overlaps(p + 3, 4, p, 4));
    CHECK(C::overlaps(p, 8, p + 2, 2));
    CHECK(C::overlaps(p + 2, 2, p, 8));
    CHECK(C::overlaps(p, 1, p, 1));
    CHECK(!C::overlaps(p, 4, p + 4, 4));
    CHECK(!C::overlaps(p + 4, 4, p, 4));
    CHECK(!C::overlaps(p, 0, p, 0));
    CHECK(!C::overlaps(p, 0, p, 4));
    CHECK(!C::overlaps(p, 4, p, 0));
    CHECK(!C::overlaps(p, 8, p + 4, 0));
    CHECK(!C::overlaps(p + 4, 0, p, 8));
    // Sizes near the address space limit must not wrap into a false answer.
    const size_t huge = SIZE_MAX / 2;
    CHECK(C::overlaps(bytes, huge, p, 1));
    CHECK(C::overlaps(p, 1, bytes, huge));
    CHECK(!C::overlaps(p, 1, bytes, 8));
}

C::SdoTransfer download() {
    C::SdoTransfer t;
    t.channel.binding.logicalAxisId = 17;
    t.channel.binding.endpoint.profile = 41;
    t.channel.binding.endpoint.address = 5;
    t.channel.binding.bindingGeneration = 3;
    t.channel.binding.configurationGeneration = 7;
    t.channel.clientToServerId = 0x605;
    t.channel.serverToClientId = 0x585;
    t.operation = 2;
    t.request = 3;
    t.step = 4;
    t.index = 0x6040;
    t.direction = C::SdoDirection::DOWNLOAD;
    t.size = 2;
    t.data[0] = 0x0f;
    t.deadlineUs = 100;
    t.queueTimeoutUs = 5;
    t.responseTimeoutUs = 6;
    return t;
}
void workIdentity() {
    const C::SdoTransfer base = download();
    CHECK(C::sameSdoChannel(base.channel, base.channel));
    CHECK(C::sameSdoWork(base, base));
    const unsigned mutations = 18;
    for (unsigned which = 0; which < mutations; ++which) {
        C::SdoTransfer other = base;
        switch (which) {
        case 0: ++other.channel.binding.logicalAxisId; break;
        case 1: ++other.channel.binding.configurationGeneration; break;
        case 2: ++other.channel.clientToServerId; break;
        case 3: ++other.channel.serverToClientId; break;
        case 4: ++other.operation; break;
        case 5: ++other.request; break;
        case 6: ++other.step; break;
        case 7: ++other.index; break;
        case 8: ++other.subindex; break;
        case 9: other.direction = C::SdoDirection::UPLOAD; break;
        case 10: ++other.size; break;
        case 11: other.uploadSize = C::SdoUploadSize::SERVER_INDICATED; break;
        case 12: other.readEffect = C::SdoReadEffect::ACCEPTED_UNQUALIFIED; break;
        case 13: other.allowSegmentedUpload = false; break;
        case 14: other.requirePreOperational = true; break;
        case 15: ++other.deadlineUs; break;
        case 16: ++other.queueTimeoutUs; break;
        default: ++other.responseTimeoutUs; break;
        }
        CHECK(!C::sameSdoWork(other, base));
        CHECK(!C::sameSdoWork(base, other));
    }
    // A different command word is different work, inside the declared size only.
    C::SdoTransfer payload = base;
    payload.data[1] = 1;
    CHECK(!C::sameSdoWork(payload, base));
    payload = base;
    payload.data[2] = 0xff;
    CHECK(C::sameSdoWork(payload, base));
    // Upload bytes are not part of the request.
    C::SdoTransfer upload = base, stale = base;
    upload.direction = stale.direction = C::SdoDirection::UPLOAD;
    stale.data[0] = 0xff;
    CHECK(C::sameSdoWork(stale, upload));
    // A corrupt size cannot read past the owned bytes.
    C::SdoTransfer oversized = base, oversized2 = base;
    oversized.size = oversized2.size = 255;
    CHECK(C::sameSdoWork(oversized, oversized2));
}

C::SdoCompletion settled(C::SdoOutcome outcome, bool possible) {
    C::SdoCompletion e;
    e.outcome = outcome;
    e.queued = e.transmissionPossible = e.effectPossible = possible;
    const bool success = outcome == C::SdoOutcome::SUCCESS;
    e.effectUncertain = possible && !success;
    e.protocolAcknowledged = success;
    e.abortValid = outcome == C::SdoOutcome::SERVER_ABORT;
    return e;
}
void terminalChecks() {
    C::SdoCompletion e = settled(C::SdoOutcome::SUCCESS, true);
    CHECK(C::checkDownloadTerminal(e, true));
    CHECK(!C::checkDownloadTerminal(e, false)); // Possible TX needs the earlier notice.
    CHECK(!C::checkUploadTerminal(e, 2));       // An upload success returned no bytes.
    e.size = 2;
    CHECK(C::checkUploadTerminal(e, 2));
    CHECK(!C::checkUploadTerminal(e, 4));
    CHECK(!C::checkDownloadTerminal(e, true)); // A download returns none.

    // A timeout is consistent whether or not anything could have left the host.
    for (bool possible : {false, true}) {
        const C::SdoCompletion timeout = settled(C::SdoOutcome::TIMEOUT, possible);
        CHECK(C::checkUploadTerminal(timeout, 2));
        CHECK(C::checkDownloadTerminal(timeout, possible));
        C::SdoCompletion bytes = timeout;
        bytes.size = 2;
        CHECK(!C::checkUploadTerminal(bytes, 2));
    }
    CHECK(C::checkDownloadTerminal(settled(C::SdoOutcome::TIMEOUT, false), false));

    const C::SdoCompletion abort = settled(C::SdoOutcome::SERVER_ABORT, true);
    CHECK(C::checkDownloadTerminal(abort, true));
    C::SdoCompletion bad = abort;
    bad.abortValid = false;
    CHECK(!C::checkDownloadTerminal(bad, true));
    CHECK(!C::checkDownloadTerminal(settled(C::SdoOutcome::SERVER_ABORT, false), true));

    // Each independently contradictory field, for both directions.
    const unsigned contradictions = 7;
    for (unsigned which = 0; which < contradictions; ++which) {
        C::SdoCompletion wrong = settled(C::SdoOutcome::TIMEOUT, true);
        switch (which) {
        case 0: wrong.queued = false; break;
        case 1: wrong.effectPossible = false; break;
        case 2: wrong.effectUncertain = false; break;
        case 3: wrong.protocolAcknowledged = true; break;
        case 4: wrong.transmissionPossible = false; wrong.transmissionConfirmed = true; break;
        case 5: wrong.outcome = C::SdoOutcome::UNCONFIRMED_SENT; break;
        default: wrong.outcome = C::SdoOutcome::SUCCESS; break; // Uncertain success.
        }
        CHECK(!C::checkUploadTerminal(wrong, 0));
        CHECK(!C::checkDownloadTerminal(wrong, true));
    }
    C::SdoCompletion abortedSuccess = settled(C::SdoOutcome::SUCCESS, true);
    abortedSuccess.abortValid = true;
    CHECK(!C::checkDownloadTerminal(abortedSuccess, true));
    C::SdoCompletion silentSuccess = settled(C::SdoOutcome::SUCCESS, false);
    silentSuccess.protocolAcknowledged = true;
    CHECK(!C::checkDownloadTerminal(silentSuccess, false));
}
} // namespace

int main() {
    overlapBehavior();
    workIdentity();
    terminalChecks();
    std::printf("sdo_evidence: %u checks passed\n", checks);
    return 0;
}
