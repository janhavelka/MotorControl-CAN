/** @file Sync.h
 * @brief Pure DLC-zero CANopen SYNC envelopes and a finite caller-clock schedule.
 * SPDX-License-Identifier: MIT
 */
#pragma once

#include <MotorControlCAN/Endpoint.h>
#include <MotorControlCAN/can/Frame.h>

namespace MotorControlCAN {
namespace CANOPEN {

/** Encode one unconfirmed standard Classical data SYNC; no work is admitted.
 * @param[in] configured1005 Explicit configured standard COB-ID. The default
 * EDS identifier is 0x80. All upper bits, including drive-producer bit 30 and
 * extended-format bit 29, are rejected. CiA301 restricted identifiers are rejected:
 * 0..0x7f, 0x101..0x180, 0x581..0x5ff, 0x601..0x67f, 0x6e0..0x6ff, 0x701..0x7ff.
 * No counter is inferred without 1019 evidence.
 * @param[out] output Owned DLC-zero frame, unchanged on failure.
 * @return OK or INVALID_CONFIG, detail zero. Success establishes no TX or drive effect.
 */
Status makeSyncRequest(uint32_t configured1005, Frame& output) noexcept;

/** Check a passive DLC-zero SYNC envelope without timestamp or producer claims.
 * @param[in] frame Borrowed only during this call; standard Classical data, DLC zero.
 * @param[in] configured1005 Expected configured identifier, with makeSyncRequest rules.
 * @return INVALID_CONFIG for unsupported COB-ID representation; validateFrame
 * errors are preserved, extended/RTR is UNSUPPORTED, ID/length mismatch is FRAME_ERROR,
 * otherwise OK. Detail zero. A matching frame alone does not identify its sender.
 */
Status checkSyncFrame(const Frame& frame, uint32_t configured1005) noexcept;

/** Explicit finite application schedule, independent of device object 1006. */
struct SyncOptions {
    uint32_t configured1005 = 0x80; ///< Configured standard DLC-zero SYNC identifier.
    Generation generation = 0; ///< Nonzero host epoch, strictly increasing on reuse.
    uint64_t originUs = 0; ///< First due instant in supplied monotonic microseconds.
    uint64_t periodUs = 0; ///< Nonzero application period/window, in microseconds.
    uint32_t count = 0; ///< Finite scheduled-slot count; zero is invalid.
};

/** Local schedule phase; neither success nor failure establishes motor behavior. */
enum class SyncState : uint8_t {
    EMPTY, ///< No schedule prepared.
    ACTIVE, ///< Waiting for the next fixed-origin slot.
    WAITING_TX, ///< Exactly one yielded request requires an owner terminal.
    SUCCEEDED, ///< All requested slots have actual local TX evidence.
    FAILED ///< Disarmed terminal; a yielded request may still require retirement.
};

/** Unconfirmed-service local outcome, separate from protocol acknowledgement/effect. */
enum class SyncOutcome : uint8_t {
    NONE, ///< No terminal local result.
    LOCAL_TX, ///< Controller actually transmitted; never drive acceptance.
    REFUSED, ///< Owner refused before any possible transmission.
    BUS_OFF, ///< Controller failure; pending transmission may remain possible.
    CANCELLED, ///< Local disarm/withdrawal; never a motor stop.
    MISSED_DEADLINE, ///< Inclusive slot cutoff reached; no catch-up or retry.
    PRODUCER_CONFLICT, ///< Owner observed another producer; application resolution required.
    INVALIDATED, ///< Controller/route/lifecycle evidence was withdrawn.
    TRANSPORT_ERROR ///< Controller failure without actual TX confirmation; possible stays separate.
};

/** Owned work identity; available is false while waiting or after termination. */
struct SyncWork {
    bool available = false; ///< True only on the call that yields this slot.
    Generation generation = 0; ///< Schedule epoch, never an on-wire correlation tag.
    uint32_t slot = 0; ///< Zero-based finite slot identity.
    uint64_t dueUs = 0; ///< Inclusive earliest application handoff instant.
    uint64_t expiresUs = 0; ///< Exclusive handoff deadline; owner must enforce it too.
    Frame frame; ///< Owned DLC-zero request bytes.
};

/** Final owner evidence for exactly one yielded work item, not queue acceptance. */
struct SyncCompletion {
    Generation generation = 0; ///< Must match yielded work.
    uint32_t slot = 0; ///< Must match yielded work.
    SyncOutcome outcome = SyncOutcome::NONE; ///< LOCAL_TX or a documented owner failure.
    bool transmissionPossible = false; ///< Cannot exclude that this frame left the host.
    bool transmissionConfirmed = false; ///< Actual controller TX; requires LOCAL_TX and possible.
    int32_t controllerDetail = 0; ///< Opaque retained local transport detail.
};

/** Caller-owned bounded context; serialize calls and do not mutate fields directly.
 * No pointers, clock, transport, thread, retry, allocation or automatic producer.
 * A terminal retains prior TX evidence. An expired yielded request still needs
 * application cancellation/retirement and advanceSync before context reuse.
 */
struct SyncContext {
    SyncOptions options; ///< Owned admitted schedule.
    SyncState state = SyncState::EMPTY; ///< Explicit schedule phase.
    SyncOutcome outcome = SyncOutcome::NONE; ///< Sticky terminal cause.
    uint32_t transmittedCount = 0; ///< Actual LOCAL_TX reports, including a late terminal report.
    uint32_t nextSlot = 0; ///< Next slot; advances only on actual LOCAL_TX.
    uint64_t lastNowUs = 0; ///< Last successful progression call's supplied time.
    bool awaitingCompletion = false; ///< Yielded owner request has not been retired.
    SyncWork pending; ///< Owned yielded identity retained through deadline failure.
    bool completionPresent = false; ///< lastCompletion carries checked owner evidence.
    SyncCompletion lastCompletion; ///< Most recent checked final owner report.
    bool transmissionPossible = false; ///< Any yielded slot may have left the host; sticky.
};

/** Validate and publish a complete finite schedule atomically; performs no I/O.
 * @param[in] options First due must be at least nowUs; count and period nonzero;
 * originUs + count*periodUs must fit uint64_t. Generation must be nonzero and
 * strictly exceed the context's retained epoch. Borrowed only during this call.
 * @param[in] nowUs Supplied monotonic microseconds; zero is valid.
 * @param[in,out] context Changed only on success; active or unretired work is rejected.
 * Must not overlap options. A terminal may be replaced only with a fresh epoch.
 * @return INVALID_CONFIG for configuration/state/time/epoch, ILLEGAL_VALUE for
 * overlap or arithmetic overflow, otherwise OK. All detail values are zero.
 */
Status prepareSync(const SyncOptions& options, uint64_t nowUs, SyncContext& context) noexcept;

/** Yield at most one due slot; application owns transport admission and retirement.
 * Slot k has window [originUs+k*periodUs, originUs+(k+1)*periodUs).
 * A missed window disarms; no catch-up, replay or timer restart occurs.
 * @param[in,out] context Prepared schedule; failures retain any pending identity/evidence.
 * @param[in] nowUs Nondecreasing supplied microseconds, including terminal calls.
 * @param[out] output Changed atomically on success; available false before due,
 * while awaiting a terminal, or after termination. Must not overlap context.
 * @return INVALID_CONFIG for empty context/backwards time; ILLEGAL_VALUE for overlap;
 * otherwise OK, including a locally expired schedule. Detail zero. Expiry does
 * not cancel a previously yielded owner request; the application must retire it.
 */
Status nextSync(SyncContext& context, uint64_t nowUs, SyncWork& output) noexcept;

/** Consume one checked final owner terminal, never mere admission/queue acceptance.
 * LOCAL_TX requires possible and confirmed; other supported owner outcomes must
 * be unconfirmed, and REFUSED must exclude transmission. NONE is not an owner
 * terminal. Owner MISSED_DEADLINE retains its immutable cutoff result even when
 * application processing is delayed. Failures disarm and never retry.
 * @param[in,out] context Must retain exactly one yielded, unretired identity.
 * @param[in] completion Borrowed checked report, copied on success. No overlap with context.
 * @param[in] nowUs Nondecreasing supplied microseconds. At/after the slot cutoff,
 * retain MISSED_DEADLINE even if a late report confirms TX. An existing terminal
 * cause stays unchanged, but late possible/confirmed evidence remains retained.
 * @return INVALID_CONFIG for state/time/correlation/evidence; ILLEGAL_VALUE for
 * overlap; otherwise OK. Context unchanged on call failure; detail zero.
 */
Status advanceSync(SyncContext& context, const SyncCompletion& completion,
                   uint64_t nowUs) noexcept;

/** Apply an explicit owner/application lifecycle interruption without an owner terminal.
 * A pending yielded identity remains awaitingCompletion and still needs exact
 * owner cancellation/terminal consumption. No frame is sent or retired here.
 * @param[in,out] context Prepared schedule; an existing terminal cause/evidence is retained.
 * @param[in] interruption BUS_OFF, PRODUCER_CONFLICT, INVALIDATED or CANCELLED only.
 * @param[in] nowUs Nondecreasing supplied microseconds; an expired active slot
 * retains MISSED_DEADLINE instead of replacing its cutoff with the interruption.
 * @return INVALID_CONFIG for empty context, backwards time or unsupported reason;
 * otherwise OK. Failed calls leave context unchanged; all detail values are zero.
 */
Status advanceSync(SyncContext& context, SyncOutcome interruption, uint64_t nowUs) noexcept;

/** Disarm locally without transmitting, retiring work or changing a retained cause.
 * @param[in,out] context Prepared context. Pending owner work remains awaitingCompletion.
 * @param[in] nowUs Nondecreasing supplied microseconds.
 * @return INVALID_CONFIG for empty context/backwards time, otherwise OK; detail zero.
 * The inclusive slot deadline takes precedence over a new cancellation cause.
 * The application separately cancels/retires any yielded owner request.
 */
Status cancelSync(SyncContext& context, uint64_t nowUs) noexcept;

} // namespace CANOPEN
} // namespace MotorControlCAN
