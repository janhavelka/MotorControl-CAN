/** @file RawEvidence.h
 * @brief Finite reviewed raw-upload experiment using the existing application owner.
 * SPDX-License-Identifier: MIT
 * Application only: no I/O, clock, heap, retry, typed interpretation or writes.
 */
#pragma once
#include "DeviceEvidence.h"

namespace MotorControlCANExample {
/** Exact fixed inventory and owned review assertions; not arbitrary object access.
 * review.plan supplies fixture metadata, target, operation id, deadline and budgets.
 * Its kind must be IDENTITY with stateMask zero; includeState explicitly appends
 * 6041:0 and 6061:0 to the identity and TPDO1 inventory documented below.
 */
struct RawEvidencePlan {
    ReadExperiment review; ///< Copied review references; no assertion authenticates external facts.
    uint8_t slot = 0; ///< Existing adapter slot matching the entire target route and node epoch.
    uint32_t firstAuthorizationId = 0; ///< First of 18 or 20 increasing one-use grant serials.
    bool includeState = false; ///< Explicit optional raw statusword and mode-display uploads.
};
enum class RawEvidenceFailure : uint8_t {
    NONE,          ///< All selected uploads completed; no typed or physical qualification.
    AUTHORIZATION, ///< Exact grant refused; this field was not admitted and is never retried.
    ADMISSION,     ///< Owner refused one granted field; unused grant withdrawn.
    SERVICE,       ///< First failed terminal retained, including uncertain effects.
    CORRELATION,   ///< Terminal identity/epoch differs from the sole pending field.
    EVIDENCE_LOSS, ///< Copied traffic overflow/loss; no further field is admitted.
    INVALIDATED,   ///< Binding/node lifecycle changed before the next field admission.
    DEADLINE       ///< Immutable overall cutoff reached between fields.
};
enum { RAW_EVIDENCE_FIELDS = 20, RAW_EVIDENCE_TRAFFIC = 96, RAW_EVIDENCE_TAIL = 16 };
/** Owned record; only count prefixes are valid. Raw widths are observations.
 * Inventory: 1000:0,1009:0,100A:0,1018:0..3,2000:0,2001:0,
 * 1800:1,2,3,5,1A00:0..4, optionally 6041:0,6061:0. Every upload has
 * four-byte capacity and UNQUALIFIED effects. No value confirms a typed service.
 * Terminal fields never change after publication. Cleanup traffic is appended
 * separately until explicit release and never rewrites the first terminal.
 */
struct RawEvidenceRecord {
    RawEvidencePlan plan;
    uint64_t startedUs = 0, completedUs = 0, maxServiceGapUs = 0;
    bool terminal = false, evidenceLost = false;
    RawEvidenceFailure failure = RawEvidenceFailure::NONE;
    BusAdmission admission = BusAdmission::ACCEPTED; ///< Meaningful for ADMISSION only.
    uint8_t intendedCount = 0, attemptedCount = 0, terminalCount = 0;
    Completion terminals[RAW_EVIDENCE_FIELDS]; ///< Exact owner copies, including failed prefix.
    Stats initialStats, finalStats; ///< Final snapshot frozen at first collector terminal.
    OwnerResources initialResources, finalResources;
    uint32_t droppedTraffic = 0;
    uint8_t trafficCount = 0;
    EvidenceTraffic traffic[RAW_EVIDENCE_TRAFFIC];
    uint8_t tailCount = 0;     ///< Post-terminal diagnostic prefix, independent of trafficCount.
    bool tailOverflow = false; ///< Includes owner overwrites/loss after collector terminal.
    EvidenceTraffic tail[RAW_EVIDENCE_TAIL];
};
/** One cooperative caller services owner first, then this collector each turn.
 * The owner/adapter outlive this object and remain the sole receive/service path.
 * Do not drain diagnostics, use DeviceEvidence, clear statistics, or operate the
 * reserved axis concurrently. Startup and construction are passive.
 */
class RawEvidence {
  public:
    RawEvidence(CanBusOwner& owner, MotorControlCAN::CANOPEN::SdoClient& services) noexcept;
    RawEvidence(const RawEvidence&) = delete;
    RawEvidence& operator=(const RawEvidence&) = delete;
    /** Reserve producer 2 and one operation, without sending or granting a field.
     * Caller supplies nondecreasing monotonic microseconds, a future exclusive
     * deadline, nonzero increasing operation/review IDs and explicit 250000 us
     * queue/response budgets. Rejection retains any existing result unchanged.
     */
    EvidenceAdmission begin(const RawEvidencePlan& plan, uint64_t nowUs) noexcept;
    /** Copy owner diagnostics and consume at most one matching field terminal.
     * Admits at most one next exact grant/upload; never receives or services owner.
     * A preceding accepted TX awaiting its final report or exact retirement proof
     * delays the next grant within the original deadline without another attempt.
     * Stops on the first failure; no automatic retries, recovery or device writes.
     * After terminal, only the bounded cleanup tail can change. Backward time
     * returns false without advancing this collector.
     */
    bool service(uint64_t nowUs) noexcept;
    const RawEvidenceRecord* record() const noexcept { return occupied_ ? &record_ : nullptr; }
    /** Release owned history/reservation only after terminal and local TX cleanup.
     * False preserves history while authoritative owner TX state awaits staging
     * retirement, a final report or exact controller retirement proof. Diagnostic
     * loss cannot release an outstanding handoff. Does not clear route quarantine
     * or qualify remote effects.
     */
    bool release() noexcept;

  private:
    CanBusOwner& owner_;
    MotorControlCAN::CANOPEN::SdoClient& services_;
    RawEvidenceRecord record_;
    SequenceId sequence_;
    RequestId pending_;
    MotorControlCAN::CANOPEN::SdoTransfer expected_;
    uint64_t lastUs_ = 0;
    uint32_t lastOperation_ = 0, overwritten_ = 0, rxLost_ = 0;
    bool occupied_ = false, pendingValid_ = false;
    void capture(bool tail) noexcept;
    void lose(uint32_t count, bool tail) noexcept;
    void finish(RawEvidenceFailure why, uint64_t nowUs) noexcept;
    bool current() const noexcept;
};
} // namespace MotorControlCANExample
