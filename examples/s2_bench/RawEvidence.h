/** @file RawEvidence.h
 * @brief Fixed reviewed identity/network, PDO record, heartbeat and PDO entry experiments
 * through one owner.
 * SPDX-License-Identifier: MIT
 * Application only: no I/O, clock, heap, retry or motion interpretation.
 */
#pragma once
#include "DeviceEvidence.h"

namespace MotorControlCANExample {
using MotorControlCANOwner::Stats;
using MotorControlCANOwner::RequestId;
using MotorControlCANOwner::SequenceId;
using MotorControlCANOwner::BusAdmission;
using MotorControlCANOwner::Completion;
using MotorControlCANOwner::OwnerResources;
using MotorControlCANOwner::CanBusOwner;
/** Fixed inventories and exact experiments; no selection grants a general raw accessor. */
enum class RawEvidenceInventory : uint8_t {
    IDENTITY_NETWORK, ///< Existing identity/TPDO baseline with explicit optional network fields.
    MOTION_READINESS, ///< Thirty fixed motion/configuration/DI uploads; no download option.
    PDO_RECORD,       ///< Nine uploads of one selected PDO set's communication and mapping records.
    PDO_COMM_WRITE    ///< Exact read, one download and exact readback of one TPDO entry.
};
/** One exact documentary address/width; this does not qualify read effects. */
struct RawEvidenceField {
    /** Construct documentary metadata, without I/O or validation. */
    constexpr RawEvidenceField(uint16_t i = 0, uint8_t s = 0, uint8_t w = 0) noexcept
        : index(i), subindex(s), documentedWidth(w) {}
    uint16_t index;          ///< Exact CANopen object index.
    uint8_t subindex;        ///< Exact subindex.
    uint8_t documentedWidth; ///< EDS bytes; raw intake still retains conflicting actual sizes.
};
/** Inspect the one fixed readiness inventory without traffic or mutation.
 * @param ordinal Zero-based 0..29. @param output Owned field, unchanged on rejection.
 * @return True for a complete documentary row; no pointer is retained.
 */
bool motionReadinessField(size_t ordinal, RawEvidenceField& output) noexcept;
/** One PDO set and, for PDO_COMM_WRITE only, one exact communication-entry change.
 * Values are the entry's raw bytes as an unsigned little-endian integer. Other
 * inventories ignore this selection, and PDO_RECORD ignores subindex, baseline and value.
 * No field requests a mapping, NMT state, PDO sample qualification or a save.
 */
struct RawPdoSelection {
    bool transmit = true;
    ///< PDO_RECORD: 1800/1A00+set if true, 1400/1600+set if false. PDO_COMM_WRITE requires true.
    uint8_t set = 0; ///< Zero-based PDO set 0..3, so 1800+set is the communication record.
    uint8_t subindex = 0;
    ///< PDO_COMM_WRITE entry and EDS width: 1 COB-ID U32, 2 transmission type U8,
    ///< 3 inhibit time U16, 5 event timer U16. Raw timer bytes; no unit is established.
    uint32_t baseline = 0;
    ///< PDO_COMM_WRITE: exact value the first read must return before any download.
    uint32_t value = 0;
    ///< PDO_COMM_WRITE: exact value downloaded once and required on readback; differs
    ///< from baseline. For subindex 1 only bit 31 (PDO not valid) may differ.
};
/** Exact fixed inventory and owned review assertions; not arbitrary object access.
 * review.plan supplies fixture metadata, target, operation id, deadline and budgets.
 * Its kind must be IDENTITY with stateMask zero; includeState explicitly appends
 * 6041:0 and 6061:0 to the identity and TPDO1 inventory documented below.
 */
struct RawEvidencePlan {
    ReadExperiment review; ///< Copied review references; no assertion authenticates external facts.
    RawEvidenceInventory inventory = RawEvidenceInventory::IDENTITY_NETWORK;
    ///< Explicit fixed inventory; all but IDENTITY_NETWORK reject the three flags below.
    uint8_t slot = 0; ///< Existing adapter slot matching the entire target route and node epoch.
    uint32_t firstAuthorizationId =
        0;                     ///< First of the selected inventory's increasing one-use grants.
    bool includeState = false; ///< Explicit optional raw statusword and mode-display uploads.
    bool includeNetwork =
        false; ///< Requires includeState; append heartbeat, monitor, fault, position, subdivision and velocity.
    bool enableHeartbeat =
        false; ///< Separate exact three-step 1017:0 experiment: read zero, write1000ms, read1000. Incompatible with the inventory flags.
    RawPdoSelection pdo; ///< Used only by PDO_RECORD and PDO_COMM_WRITE; validated by begin().
};
enum class RawEvidenceFailure : uint8_t {
    NONE,          ///< All selected uploads completed; no typed or physical qualification.
    AUTHORIZATION, ///< Exact grant refused; this field was not admitted and is never retried.
    ADMISSION,     ///< Owner refused one granted field; unused grant withdrawn.
    SERVICE,       ///< First failed terminal retained, including uncertain effects.
    CORRELATION,   ///< Terminal identity/epoch differs from the sole pending field.
    EVIDENCE_LOSS, ///< Copied traffic overflow/loss; no further field is admitted.
    INVALIDATED,   ///< Binding/node lifecycle changed before the next field admission.
    DEADLINE,      ///< Immutable overall cutoff reached between fields.
    READBACK       ///< Exact heartbeat/PDO entry baseline or readback differs; no retry/rollback.
};
enum { RAW_EVIDENCE_FIELDS = 30, RAW_EVIDENCE_TRAFFIC = 112, RAW_EVIDENCE_TAIL = 16 };
/** Owned record; only count prefixes are valid. Raw widths are observations.
 * Inventory: 1000:0,1009:0,100A:0,1018:0..3,2000:0,2001:0,
 * 1800:1,2,3,5,1A00:0..4, optionally 6041:0,6061:0. Every upload has
 * four-byte capacity and UNQUALIFIED effects. Network appends 1017/2100/603F/6064/2201/606C.
 * MOTION_READINESS is a separate upload-only inventory: 1001,603F,6041,6061,
 * 2100,2101,6064,606C,607A,6081,6083,6084,605D,60FF,2200,2201,2202,
 * 607D:1/2,2500,2501,2400:1..8,60FD (subindex zero unless stated).
 * Its four-byte capacity preserves server-indicated widths, raw signed bits and
 * access/type conflicts. Neither successful reads nor 2201 establish shaft scale.
 * PDO_RECORD uploads one set: communication :01,:02,:03,:05 then mapping :00..:04,
 * from 1800/1A00+set for a transmit PDO or 1400/1600+set for a receive PDO, with the
 * same four-byte capacity and UNQUALIFIED effects. A successful read establishes no valid
 * or active mapping.
 * The separate heartbeat experiment uses exact U16 reads and one explicit1000ms
 * download only after a zero baseline. It changes producer timing, never motor
 * state, and makes no persistence or watchdog guarantee. No value confirms a typed service.
 * PDO_COMM_WRITE reads 1800+set:subindex at its exact width, downloads value only
 * after the exact baseline, then requires the exact value on readback. Success shows
 * only that this entry read back as value. It qualifies no PDO sample and
 * establishes no timer unit, persistence, producer timing or drive effect. A rejected
 * download keeps its full SDO abort code as observed evidence; nothing is rolled back.
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
     * Field counts, and so grant identities: IDENTITY_NETWORK 18, 20 or 26,
     * heartbeat 3, MOTION_READINESS 30, PDO_RECORD 9, PDO_COMM_WRITE 3.
     * PDO plans need set 0..3. PDO_COMM_WRITE also needs transmit, subindex 1, 2, 3
     * or 5, baseline and value within that width and different, a COB-ID change of
     * bit 31 only, and a transmission type 0..240, 254 or 255.
     */
    EvidenceAdmission begin(const RawEvidencePlan& plan, uint64_t nowUs) noexcept;
    /** Copy owner diagnostics and consume at most one matching field terminal.
     * Admits at most one next exact grant/upload; never receives or services owner.
     * A preceding accepted TX awaiting its final report or exact retirement proof
     * delays the next grant within the original deadline without another attempt.
     * Stops on the first failure; no automatic retries or recovery. Only the
     * explicitly selected heartbeat experiment yields its fixed1017 download, and
     * only PDO_COMM_WRITE its one validated entry download. Only the heartbeat
     * experiment withdraws and sets the adapter's producer interval.
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
