/** @file SdoTransfer.h
 * @brief Owned CANopen service intent and retained evidence, independent of a stack or controller.
 * No declaration opens a device, reads time, retries, allocates or establishes motion completion.
 * SPDX-License-Identifier: MIT
 */
#pragma once
#include <MotorControlCAN/canopen/Sdo.h>

namespace MotorControlCAN {
namespace CANOPEN {

/** Explicit upload effect policy; an EDS upload/access flag is insufficient evidence. */
enum class SdoReadEffect : uint8_t {
    UNQUALIFIED, ///< Upload may change/consume device state; preserve possible effects.
    REVIEWED_NON_CHANGING ///< Applicable source review establishes no drive-changing effect.
};
/** Native service terminal cause, separate from local admission rejection and drive state. */
enum class SdoOutcome : uint8_t {
    SUCCESS, ///< Matching SDO completed; a download acknowledgement is not motion completion.
    SERVER_ABORT, ///< Matching unsigned server abort; earlier workflow writes are not undone.
    CLIENT_ABORT, ///< Malformed/size/toggle response rejected locally.
    TIMEOUT, ///< Immutable overall, queue or response cutoff reached.
    CANCELLED, ///< Local cancellation; never a motor stop.
    BUS_OFF, ///< Application reports loss of the physical interface.
    REBOOT, ///< Application reports server reboot and invalidates its channels.
    TRANSPORT_ERROR, ///< Application/controller or selected-stack boundary failure.
    UNSUPPORTED ///< Valid discovered transfer form exceeds admitted capability.
};
/** Complete copied service intent. This is not an object-access permission. */
struct SdoTransfer {
    SdoChannel channel; ///< Explicit source-backed route and copied binding/epochs.
    uint32_t operation = 0; ///< Nonzero application operation identity; absent from the wire.
    uint32_t request = 0; ///< Nonzero application request identity; absent from the wire.
    uint32_t step = 0; ///< Nonzero workflow step identity; absent from the wire.
    uint16_t index = 0; ///< Explicit reviewed object index.
    uint8_t subindex = 0; ///< Explicit object subindex.
    SdoDirection direction = SdoDirection::UPLOAD; ///< Selected CANopen service direction.
    uint8_t size = 0; ///< Exact expected upload/download size, 1..32 bytes; scalar widths are 1..4.
    uint8_t data[32] = {}; ///< Owned download bytes [0,size); ignored for an upload.
    SdoReadEffect readEffect = SdoReadEffect::UNQUALIFIED; ///< Independent reviewed upload policy.
    bool allowSegmentedUpload = true; ///< False requires explicit unsupported settlement if discovered.
    uint64_t deadlineUs = 0; ///< Exclusive immutable overall cutoff in caller monotonic microseconds.
    uint32_t queueTimeoutUs = 0; ///< Nonzero maximum staging delay per frame, clamped to deadlineUs.
    uint32_t responseTimeoutUs = 0; ///< Nonzero response interval after handoff, clamped to deadlineUs.
};
/** Immutable first terminal, retained until explicit release by its stationary adapter owner. */
struct SdoCompletion {
    SdoTransfer transfer; ///< Complete admission copy, including payload, target and local identities.
    SdoOutcome outcome = SdoOutcome::TRANSPORT_ERROR; ///< Service cause, never application health.
    uint64_t completedUs = 0; ///< Supplied processing time at terminal publication; zero is valid.
    bool queued = false; ///< A request frame entered retirable local staging; no TX is implied.
    bool transmissionPossible = false; ///< At least one request frame crossed the application boundary.
    bool transmissionConfirmed = false; ///< Application confirmed a frame TX/ACK; not SDO success.
    bool protocolAcknowledged = false; ///< Matching complete SDO success only.
    bool effectPossible = false; ///< Possible device-changing effect, including a successful unqualified read.
    bool effectUncertain = false; ///< Possible device-changing effect without service success.
    bool abortValid = false; ///< Makes the full unsigned abortCode meaningful, including zero.
    uint32_t abortCode = 0; ///< Full raw server/client abort; never narrowed through Status::detail.
    int32_t stackReturn = 0; ///< Selected engine return, independent of the raw abort.
    uint8_t data[32] = {}; ///< Owned upload bytes [0,size) on success; otherwise no value is published.
    uint8_t size = 0; ///< Exact successful upload byte count; zero otherwise.
    Frame reply; ///< Last received matching frame, copied before callback; inspect only if replyValid.
    bool replyValid = false; ///< Raw response envelope exists; not a claim that it was well formed.
    bool clientAbortQueued = false; ///< Cleanup was staged at publication; never claims it was sent.
};

} // namespace CANOPEN
} // namespace MotorControlCAN
