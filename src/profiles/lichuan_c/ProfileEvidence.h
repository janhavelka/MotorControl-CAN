// SPDX-License-Identifier: MIT
// Private to this profile's sources; never installed. Identity and terminal facts shared by the
// read, settings, action and PDO configuration lifecycles. Nothing here is admission or
// completion evidence by itself: every caller adds its own predicate, freshness and history.
#pragma once
#include "../../canopen/SdoEvidence.h"
#include <MotorControlCAN/profiles/lichuan_c/Settings.h>

namespace MotorControlCAN {
namespace LICHUAN_C {
namespace Detail {
using CANOPEN::overlaps;

inline bool sameTarget(const ReadTarget& a, const ReadTarget& b) noexcept {
    return a.nodeGeneration == b.nodeGeneration && CANOPEN::sameSdoChannel(a.channel, b.channel);
}
inline bool validReservation(const AxisReservation& r) noexcept {
    return r.owner && r.sequence && r.generation && r.target.nodeGeneration &&
           (r.model == SettingsModel::CL86_C || r.model == SettingsModel::OL86_C) &&
           CANOPEN::validateSdoChannel(r.target.channel);
}
inline bool sameReservation(const AxisReservation& a, const AxisReservation& b) noexcept {
    return validReservation(a) && validReservation(b) && sameTarget(a.target, b.target) &&
           a.model == b.model && a.owner == b.owner && a.sequence == b.sequence &&
           a.generation == b.generation && a.acquiredUs == b.acquiredUs;
}
inline uint16_t controlWord(const CANOPEN::SdoTransfer& t) noexcept {
    return uint16_t(t.data[0] | (uint16_t(t.data[1]) << 8));
}
// An acknowledged 6040:00 U16 download admitted under this reservation and finished by now. It
// does not say which word is acceptable, how old it may be or whether the drive may be running:
// settings needs 000F, a bounded age and a nonrunning report, while a stop must work on a running
// drive and accepts its DEC-8 predecessors with no age limit. Those differences stay with callers.
inline bool acknowledgedControlWrite(const CANOPEN::SdoCompletion& e, const AxisReservation& r,
                                     uint64_t now) noexcept {
    const CANOPEN::SdoTransfer& t = e.transfer;
    return CANOPEN::sameSdoChannel(t.channel, r.target.channel) &&
           e.nodeGeneration == r.target.nodeGeneration &&
           e.outcome == CANOPEN::SdoOutcome::SUCCESS &&
           t.direction == CANOPEN::SdoDirection::DOWNLOAD && t.index == 0x6040 && !t.subindex &&
           t.size == 2 && t.operation && t.request && t.step && t.deadlineUs > e.completedUs &&
           t.queueTimeoutUs && t.responseTimeoutUs && e.queued && e.transmissionPossible &&
           e.protocolAcknowledged && e.effectPossible && !e.effectUncertain && !e.abortValid &&
           !e.size && r.acquiredUs <= e.admittedUs && e.admittedUs <= e.completedUs &&
           e.completedUs <= now;
}
// Maps a service terminal that arrived before the overall cutoff, which every caller settles first.
// TIMEOUT here means a queue or response budget; whether a frame left stays in the evidence.
inline ReadOutcome readOutcome(CANOPEN::SdoOutcome o) noexcept {
    switch (o) {
    case CANOPEN::SdoOutcome::SUCCESS:
        return ReadOutcome::SUCCESS;
    case CANOPEN::SdoOutcome::SERVER_ABORT:
        return ReadOutcome::PROTOCOL_REJECTED;
    case CANOPEN::SdoOutcome::TIMEOUT:
        return ReadOutcome::SERVICE_TIMEOUT;
    case CANOPEN::SdoOutcome::CANCELLED:
        return ReadOutcome::CANCELLED;
    case CANOPEN::SdoOutcome::REBOOT:
    case CANOPEN::SdoOutcome::BINDING_INVALIDATED:
        return ReadOutcome::INVALIDATED;
    case CANOPEN::SdoOutcome::CLIENT_ABORT:
    case CANOPEN::SdoOutcome::UNSUPPORTED:
        return ReadOutcome::REPLY_ERROR;
    default:
        return ReadOutcome::TRANSPORT_ERROR;
    }
}
} // namespace Detail
} // namespace LICHUAN_C
} // namespace MotorControlCAN
