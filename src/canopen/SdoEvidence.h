// SPDX-License-Identifier: MIT
// Private to the core library sources; never installed, so adapters and examples keep their own
// helpers. These decide whether a retained service terminal can be believed. They are free
// functions with no state, not an operation engine: each caller keeps its own phases and times.
#pragma once
#include <MotorControlCAN/canopen/SdoTransfer.h>
#include <cstddef>
#include <cstdint>

namespace MotorControlCAN {
namespace CANOPEN {

// True when the two byte ranges share at least one byte. An empty range shares none, wherever it
// points. Unlike comparing `a + aSize`, this cannot wrap.
inline bool overlaps(const void* a, size_t aSize, const void* b, size_t bSize) noexcept {
    const uintptr_t x = reinterpret_cast<uintptr_t>(a), y = reinterpret_cast<uintptr_t>(b);
    return aSize && bSize && (x >= y ? x - y < bSize : y - x < aSize);
}

inline bool sameSdoChannel(const SdoChannel& a, const SdoChannel& b) noexcept {
    return sameBinding(a.binding, b.binding) && a.clientToServerId == b.clientToServerId &&
           a.serverToClientId == b.serverToClientId;
}

// True when a retained terminal's admission copy is exactly the work the caller expected.
// Download bytes are part of the identity because a different payload is a different command.
inline bool sameSdoWork(const SdoTransfer& a, const SdoTransfer& b) noexcept {
    if (!sameSdoChannel(a.channel, b.channel) || a.operation != b.operation ||
        a.request != b.request || a.step != b.step || a.index != b.index ||
        a.subindex != b.subindex || a.direction != b.direction || a.size != b.size ||
        a.uploadSize != b.uploadSize || a.readEffect != b.readEffect ||
        a.allowSegmentedUpload != b.allowSegmentedUpload ||
        a.requirePreOperational != b.requirePreOperational || a.deadlineUs != b.deadlineUs ||
        a.queueTimeoutUs != b.queueTimeoutUs || a.responseTimeoutUs != b.responseTimeoutUs)
        return false;
    if (a.direction == SdoDirection::DOWNLOAD)
        for (size_t i = 0; i < a.size && i < sizeof a.data; ++i)
            if (a.data[i] != b.data[i])
                return false;
    return true;
}

// Facts every ordinary terminal must keep consistent, whichever way the service ran. Possible
// transmission is independent of the outcome, so a timeout may or may not have left the host.
// UNCONFIRMED_SENT is a best-effort stop's own result and is never ordinary evidence here.
inline bool terminalConsistent(const SdoCompletion& e) noexcept {
    const bool success = e.outcome == SdoOutcome::SUCCESS;
    const bool possible = e.transmissionPossible;
    return e.outcome <= SdoOutcome::UNSUPPORTED && (!possible || e.queued) &&
           (!e.transmissionConfirmed || possible) && e.effectPossible == possible &&
           e.effectUncertain == (possible && !success) && e.protocolAcknowledged == success &&
           (!success || possible) &&
           // Only an abort carries an abort code, and an answered service staged no client abort.
           (!e.abortValid || e.outcome == SdoOutcome::SERVER_ABORT ||
            e.outcome == SdoOutcome::CLIENT_ABORT) &&
           (!e.clientAbortQueued || (!success && e.outcome != SdoOutcome::SERVER_ABORT)) &&
           (e.outcome != SdoOutcome::SERVER_ABORT || (e.abortValid && possible));
}

// An upload whose read effect stays unqualified. A failed one carries no bytes.
inline bool checkUploadTerminal(const SdoCompletion& e, uint8_t expectedSize) noexcept {
    return terminalConsistent(e) &&
           e.size == (e.outcome == SdoOutcome::SUCCESS ? expectedSize : uint8_t(0));
}

// A download, which never returns bytes. Possible transmission needs the caller's own earlier
// possible-TX notice, because the effects it withdraws must already be withdrawn.
inline bool checkDownloadTerminal(const SdoCompletion& e, bool transmissionNoted) noexcept {
    return terminalConsistent(e) && !e.size && (!e.transmissionPossible || transmissionNoted);
}

} // namespace CANOPEN
} // namespace MotorControlCAN
