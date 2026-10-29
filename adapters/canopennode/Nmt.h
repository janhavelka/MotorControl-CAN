/** @file Nmt.h
 * @brief Owned explicit manager requests; NMT has no remote acknowledgement.
 * SPDX-License-Identifier: MIT
 */
#pragma once
#include "NetworkState.h"

namespace MotorControlCAN {
namespace CANOPEN {
class SdoClient;
/** Copied explicit NMT intent; default target and budgets are invalid. */
struct NmtTransfer {
    NmtCommand command = NmtCommand::START; ///< Reviewed network command, never a drive stop/enable.
    bool broadcast = false; ///< True targets the entire selected physical CANopen domain.
    uint32_t bus = 0; ///< Explicit physical interface; zero is permitted.
    NetworkRoute target; ///< Exact current addressed route; ignored for broadcast, never a node-zero axis.
    uint32_t request = 0; ///< Nonzero application identity, not transmitted.
    uint64_t deadlineUs = 0; ///< Immutable exclusive overall microsecond cutoff.
    uint32_t queueTimeoutUs = 0; ///< Nonzero staging interval, clamped to deadlineUs.
};
/** Nonwrapping handle for the owner's single retained manager request. */
struct NmtRequestId {
    const SdoClient* owner = nullptr; ///< Stationary originating owner; borrowed for its lifetime.
    uint32_t generation = 0; ///< Nonzero admission generation; exhaustion rejects new work.
};
/** Local terminal cause, independent of remote observed state. */
enum class NmtOutcome : uint8_t {
    TRANSMITTED, ///< Controller reports actual TX; no remote confirmation exists.
    QUEUE_REJECTED, ///< Selected producer could not stage bytes; proven unsent here.
    TIMEOUT, ///< Queue or overall cutoff reached, never requested-state promotion.
    CANCELLED, ///< Local withdrawal, not a network or physical motor stop.
    BUS_OFF, ///< Controller unavailable; possible earlier TX remains possible.
    INVALIDATED ///< Target/controller lifecycle changed before local completion.
};
/** One affected local binding, including shared-node SDO channels. */
struct NmtAffected {
    Binding binding; ///< Frozen local identity, never a node-zero binding.
    Generation previousGeneration = 0; ///< Passive epoch before handoff.
    Generation generation = 0; ///< Epoch after handoff; nonreset commands preserve it.
};
/** Immutable first local terminal until releaseNmt; no synthetic remote success. */
struct NmtCompletion {
    NmtTransfer transfer; ///< Admission copy, independent of caller mutations.
    NmtOutcome outcome = NmtOutcome::INVALIDATED; ///< Local cause only.
    uint64_t completedUs = 0; ///< Supplied processing microseconds at publication.
    bool queued = false; ///< Native producer accepted retirable staging only.
    bool transmissionPossible = false; ///< Bytes crossed the application handoff boundary.
    bool transmissionConfirmed = false; ///< Actual controller TX report, never exact remote state.
    uint8_t affectedCount = 0; ///< Number of meaningful affected entries, at most two.
    NmtAffected affected[2]; ///< Copied local bindings/epochs; broadcast may also affect unbound remote nodes.
};
/** Separate decisions for ordinary service and first possible passive PDO evidence. */
struct ServiceEligibility {
    bool exactNmtKnown = false; ///< Qualified current heartbeat/boot-up observation, never inferred from SDO/TX.
    NmtState state = NmtState::INITIALIZING; ///< Meaningful only when exactNmtKnown.
    bool ordinarySdo = false; ///< Still requires reviewed object/drive prerequisites and a usable SDO channel.
    bool nonChangingProbe = false; ///< Explicit bounded reviewed upload may establish serviceability.
    bool pdoTransmit = false; ///< Requires current Operational plus separately qualified map/configuration.
    bool tpdoReceive = false; ///< May enter configured-map validation even with unknown exact NMT; not itself qualification.
};
/** Evaluate current node evidence without mutation, I/O or state promotion.
 * @param[in] node Call-only stable node snapshot; age policy is stored with it.
 * @param[in] nowUs Supplied monotonic microseconds, not before retained observations.
 * @param[out] output Published atomically; may not overlap node.
 * @return False for future time/invalid age policy/overlap; output unchanged.
 * Known-current Stopped/Initializing blocks ordinary services and probes. Unknown
 * exact state permits a reviewed non-changing probe. No TPDO inference is made.
 */
bool serviceEligibility(const NodeSnapshot& node, uint64_t nowUs, ServiceEligibility& output);
} // namespace CANOPEN
} // namespace MotorControlCAN
