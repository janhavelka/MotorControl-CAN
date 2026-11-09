/** @file CommunicationBench.h
 * @brief The K and H lines: prompt 38's typed communication reads of one node and its guarded
 * heartbeat commissioning, each through the profile's typed lifecycle and the ordinary owner.
 * SPDX-License-Identifier: MIT
 * Application only: no I/O, clock, heap, retry, NMT, save or reset. K yields uploads only and is
 * never a download producer; H yields exactly one 1017:00 download, matched by its preflight.
 */
#pragma once
#include "BenchTerminal.h"
#include "DeviceEvidence.h"
#include <MotorControlCAN/profiles/lichuan_c/Communication.h>

namespace MotorControlCANExample {
using MotorControlCANOwner::BusAdmission;
using MotorControlCANOwner::SequenceId;
enum {
    COMMUNICATION_GROUPS = 18,  ///< NETWORK, NODE_SETTING and the sixteen PDO records.
    COMMUNICATION_UPLOADS = 95, ///< 6 + 1 + 4 x 6 + 4 x 5 + 4 x 6 + 4 x 5 typed uploads.
    COMMUNICATION_LINE = 320,   ///< Line buffer; the longest line at its widest fits.
    COMMUNICATION_PRODUCER = 4  ///< Owner producer of both records; app 1, raw 2, PDO 3.
};
/** Why a K record ended, separate from each group's own read outcome. */
enum class CommunicationBenchFailure : uint8_t {
    NONE,          ///< All eighteen groups read and published by getCommunication.
    AUTHORIZATION, ///< A one-use upload grant was refused; nothing of that field was sent.
    ADMISSION,     ///< The owner refused one upload.
    SERVICE,       ///< A group's read ended without success; its terminal stays held.
    CORRELATION,   ///< An owner terminal did not match the one pending upload.
    INVALIDATED,   ///< Route, node epoch or reservation changed between uploads.
    DEADLINE,      ///< The immutable overall cutoff was reached.
    PREPARATION,   ///< The profile refused a group's preparation or its next upload.
    PUBLICATION    ///< A successful group was refused by getCommunication.
};
/** One checked K-line request. */
struct CommunicationBenchRequest {
    uint32_t id = 0;         ///< First of eighteen nonzero increasing operation identities.
    uint8_t node = 0;        ///< 1 or 2: slot node - 1 at its current configured route.
    uint32_t firstGrant = 0; ///< First of at most 95 increasing one-use upload grants on that slot.
    uint64_t deadlineUs = 0; ///< Immutable overall cutoff of all eighteen groups.
};
/** The typed values of one published group, copied from its CommunicationObservation. */
struct CommunicationGroupResult {
    MotorControlCAN::LICHUAN_C::CommunicationGroup group =
        MotorControlCAN::LICHUAN_C::CommunicationGroup::NETWORK; ///< Which fields are meaningful.
    uint8_t pdoNumber = 0;  ///< 1..4 for a PDO group, else 0.
    uint8_t fieldCount = 0; ///< 1, 5 or 6.
    uint32_t raw[6] = {};   ///< Group-order raw words, as the observation types them.
    MotorControlCAN::LICHUAN_C::ReadWindow window; ///< Original acquisition window and skew.
};
/** Owned K record, held until release. */
struct CommunicationBenchRecord {
    CommunicationBenchRequest request; ///< Exact admitted request.
    uint64_t startedUs = 0, completedUs = 0, maxServiceGapUs = 0; ///< Owner times.
    bool terminal = false;                                        ///< Immutable until release.
    CommunicationBenchFailure failure = CommunicationBenchFailure::NONE; ///< First terminal cause.
    BusAdmission admission = BusAdmission::ACCEPTED; ///< Meaningful for ADMISSION only.
    uint8_t group = 0;     ///< Group being read when the record ended; 18 after success.
    uint8_t published = 0; ///< Groups published, a prefix of the fixed order.
    MotorControlCAN::ReadOutcome outcome = MotorControlCAN::ReadOutcome::NONE;
    ///< That group's read outcome: SUCCESS after a complete record.
    bool uncertain = false;    ///< That group's read retained possible effects.
    MotorControlCAN::Status status; ///< Last refused profile call, OK otherwise.
    uint8_t grants = 0;        ///< One-use grants issued.
    uint8_t terminalCount = 0; ///< Copied owner terminals, at most COMMUNICATION_UPLOADS.
    BenchTerminal terminals[COMMUNICATION_UPLOADS];     ///< Valid prefix only.
    CommunicationGroupResult groups[COMMUNICATION_GROUPS]; ///< Valid published prefix only.
};
/** K: every typed communication group of one node, in a fixed order: NETWORK, NODE_SETTING,
 * TPDO1..4 communication, TPDO1..4 mapping, RPDO1..4 communication, RPDO1..4 mapping.
 * Each group is one CommunicationReadContext through prepareCommunicationRead,
 * nextCommunication, advanceCommunication and getCommunication. Every UNQUALIFIED upload gets an
 * exact one-use reviewed grant first (Q20); the read effects stay unknown. The first failure ends
 * the record and holds its terminal; nothing is retried, written or reconciled.
 * Call after the firmware turn, with the owner's last time.
 */
class CommunicationReads {
  public:
    CommunicationReads(CanBusOwner& owner, MotorControlCAN::CANOPEN::SdoClient& services) noexcept;
    CommunicationReads(const CommunicationReads&) = delete;
    CommunicationReads& operator=(const CommunicationReads&) = delete;
    /** Reserve the node's binding as producer 4; nothing is sent here.
     * @return Null when admitted, else busy, invalid, route, unresolved, grant or reserved. grant: the
     * request's first operation or first grant is not above the channel's last experimental grant
     * (SdoClient::lastExperiment), which a route may have drawn; main.cpp continues its counters
     * from there. reserved: another holder keeps the axis's owner reservation, on the bench an open
     * route AxisSession until the bench's session close (BenchRoute::closeSessions). */
    const char* begin(const CommunicationBenchRequest& request, uint64_t nowUs) noexcept;
    /** Consume at most one terminal and admit at most one next upload. @return False for
     * backward time or an owner release refusal. */
    bool service(uint64_t nowUs) noexcept;
    const CommunicationBenchRecord* record() const noexcept { return occupied_ ? &record_ : nullptr; }
    /** Header, the published group lines, RESULT lines and END. @return False past the end. */
    bool formatLine(unsigned cursor, char* output, size_t size) const noexcept;
    /** Release after terminal and local TX retirement; quarantine stays. */
    bool release() noexcept;

  private:
    CanBusOwner& owner_;
    MotorControlCAN::CANOPEN::SdoClient& services_;
    CommunicationBenchRecord record_;
    MotorControlCAN::LICHUAN_C::ReadTarget target_;
    MotorControlCAN::LICHUAN_C::CommunicationReadContext context_;
    MotorControlCAN::LICHUAN_C::CommunicationObservation observation_; // Getter output scratch.
    MotorControlCAN::CANOPEN::SdoTransfer expected_;
    SequenceId sequence_;
    RequestId pending_;
    uint64_t lastUs_ = 0;
    uint32_t lastId_ = 0;
    bool occupied_ = false, pendingValid_ = false, prepared_ = false;
    uint8_t slot() const noexcept { return uint8_t(record_.request.node - 1); }
    bool current() const noexcept;
    void finish(CommunicationBenchFailure why, uint64_t nowUs) noexcept;
};

/** Why an H record ended. */
enum class HeartbeatBenchFailure : uint8_t {
    NONE,          ///< Baseline 0, acknowledged write, migration and matching readback 1000.
    AUTHORIZATION, ///< A one-use grant was refused; that step was not sent.
    ADMISSION,     ///< The owner refused one step.
    SERVICE,       ///< A step ended without success; its terminal and any uncertainty stay.
    CORRELATION,   ///< An owner terminal did not match the one pending step.
    EVIDENCE_LOSS, ///< Copied traffic overflowed; no further step was admitted.
    INVALIDATED,   ///< Route, node epoch or reservation changed.
    DEADLINE,      ///< The immutable overall cutoff was reached.
    NMT,           ///< A known current NMT state was not Pre-operational; nothing was sent.
    BASELINE,      ///< prepareCommunication refused the baseline before any write.
    CONFIGURATION, ///< The profile or owner refused a notice, migration or confirmation step.
    READBACK,      ///< Acknowledged write, but the readback differed from raw 1000.
    PUBLICATION,   ///< getCommunication refused the confirmed result.
    /** The owner's epoch migration premises (see HeartbeatCommission) stopped holding before the
     * baseline or before the write: nothing, or only the baseline upload, was sent; no write. */
    ELIGIBILITY
};
/** One checked H-line request; only producerTimeRaw 1000 exists. */
struct HeartbeatBenchRequest {
    uint32_t id = 0;                ///< Baseline operation; id + 1 is the commissioning operation.
    uint8_t node = 0;               ///< 1 or 2.
    uint16_t producerTimeRaw = 0;   ///< Must be 1000: prompt 38's only reviewed transition.
    uint32_t firstGrant = 0;        ///< First of three increasing one-use grants on that slot.
    uint64_t deadlineUs = 0;        ///< Immutable overall cutoff, also the profile's.
};
enum {
    HEARTBEAT_TERMINALS = 3, ///< Baseline upload, download, readback.
    HEARTBEAT_TRAFFIC = 64,  ///< Copied owner traffic before the terminal.
    HEARTBEAT_TAIL = 16      ///< Copied owner traffic after it.
};
/** Owned H record, held until release. */
struct HeartbeatBenchRecord {
    HeartbeatBenchRequest request; ///< Exact admitted request.
    MotorControlCAN::LICHUAN_C::AxisReservation reservation; ///< Built from the held sequence.
    MotorControlCAN::LICHUAN_C::CommunicationReview review; ///< Fixed bench attestations.
    MotorControlCAN::LICHUAN_C::CommunicationNmt nmt; ///< NMT observation the baseline used.
    MotorControlCAN::LICHUAN_C::CommunicationContext context; ///< Profile workflow and evidence.
    MotorControlCAN::LICHUAN_C::CommunicationResult result;   ///< Meaningful when published.
    MotorControlCAN::Binding current; ///< Application binding; advanced once at the notice.
    MotorControlCAN::Status status;   ///< Last refused profile call, OK otherwise.
    bool prepared = false;  ///< prepareCommunication accepted the baseline.
    bool migrated = false;  ///< owner.advanceConfiguration moved the route to the next epoch.
    bool published = false; ///< getCommunication returned the confirmed stored value.
    bool terminal = false, evidenceLost = false, tailOverflow = false; ///< Record state.
    HeartbeatBenchFailure failure = HeartbeatBenchFailure::NONE; ///< First terminal cause.
    BusAdmission admission = BusAdmission::ACCEPTED; ///< Meaningful for ADMISSION only.
    uint64_t startedUs = 0, completedUs = 0, maxServiceGapUs = 0; ///< Owner times.
    uint8_t attempted = 0, terminalCount = 0; ///< Grants issued and terminals copied.
    BenchTerminal terminals[HEARTBEAT_TERMINALS]; ///< Valid prefix only.
    uint32_t droppedTraffic = 0;                  ///< Saturating minimum lost count.
    uint8_t trafficCount = 0, tailCount = 0;      ///< Valid prefixes.
    EvidenceTraffic traffic[HEARTBEAT_TRAFFIC], tail[HEARTBEAT_TAIL]; ///< Owner traffic copies.
};
/** H: the reviewed CL86-C 1017:00 raw 0 to raw 1000 transition through prepareCommunication,
 * nextCommunication, noteCommunicationTransmission, advanceCommunication, owner
 * advanceConfiguration, prepareCommunicationConfirmation and getCommunication. Scope is
 * ISOLATED_EXPERIMENT. The baseline must read raw 0, so a node already at any other value is
 * refused before a write. The producer interval and the node's PDO routes are withdrawn before
 * the download can leave and are not restored: a stored readback is not active timing.
 * There is no reverse transition, save, reset or NMT here; restoration is a separate bench step.
 *
 * Precondition, checked at begin, again before the baseline and again before the write is
 * admitted: the node premises of the owner's epoch migration, SdoClient::configurationEligible,
 * must hold. Otherwise the owner's advanceConfiguration refuses the migration after an
 * acknowledged write, which leaves an applied write that cannot be confirmed (2026-11-09, both
 * nodes). Two of them need explicit bench steps:
 * - Ordinary SDO service: a current qualified Pre-operational or Operational state, or service
 *   evidence from an accepted, ungranted upload after any NMT intent, such as the D line's probe.
 *   The baseline, write and readback here are granted experiments and establish none.
 * - No pending NMT transition: an N line leaves its node's transition pending until a matching
 *   heartbeat or boot-up is observed or until its 5 s wait window ends. A node at producer time 0
 *   sends no heartbeat, so H can follow N only after that window.
 * begin refuses "service" or "transition" for these, "configuration" for any other premise.
 */
class HeartbeatCommission {
  public:
    HeartbeatCommission(CanBusOwner& owner, MotorControlCAN::CANOPEN::SdoClient& services) noexcept;
    HeartbeatCommission(const HeartbeatCommission&) = delete;
    HeartbeatCommission& operator=(const HeartbeatCommission&) = delete;
    /** Reserve the node's binding as producer 4; nothing is sent here.
     * @return Null when admitted, else busy, invalid, value, route, unresolved, grant (as for
     * CommunicationReads), service, transition or configuration (a migration premise is missing, see
     * the precondition above), or reserved (another holder keeps the axis's reservation, as for
     * CommunicationReads). */
    const char* begin(const HeartbeatBenchRequest& request, uint64_t nowUs) noexcept;
    /** Consume at most one terminal and admit at most one next step. @return False for
     * backward time or an owner release refusal. */
    bool service(uint64_t nowUs) noexcept;
    /** Match the exact, already noticed pending download at the owner-to-adapter handoff.
     * Snapshot getters and pure profile calls only. @return False for anything else. */
    bool preflight(SequenceId sequence, const MotorControlCAN::CANOPEN::SdoTransfer& transfer,
                   uint64_t nowUs) const noexcept;
    const HeartbeatBenchRecord* record() const noexcept { return occupied_ ? &record_ : nullptr; }
    /** HB, HB_PREP, RESULT lines, TRAFFIC and TAIL lines, END. @return False past the end. */
    bool formatLine(unsigned cursor, char* output, size_t size) const noexcept;
    /** Release after terminal and local TX retirement; quarantine, epochs and the withdrawn
     * producer interval stay as they are. */
    bool release() noexcept;

  private:
    CanBusOwner& owner_;
    MotorControlCAN::CANOPEN::SdoClient& services_;
    HeartbeatBenchRecord record_;
    MotorControlCAN::CANOPEN::SdoCompletion baseline_;
    MotorControlCAN::CANOPEN::SdoTransfer expected_;
    SequenceId sequence_;
    RequestId pending_;
    uint64_t lastUs_ = 0;
    uint32_t lastId_ = 0, overwritten_ = 0, rxLost_ = 0;
    bool occupied_ = false, pendingValid_ = false;
    HeartbeatBenchFailure requested_ = HeartbeatBenchFailure::NONE;
    uint8_t slot() const noexcept { return uint8_t(record_.request.node - 1); }
    bool current() const noexcept;
    const char* migrationRefusal(uint8_t slot, uint64_t nowUs) const noexcept;
    MotorControlCAN::LICHUAN_C::ReadTarget currentTarget() const noexcept;
    void currentNmt(uint64_t nowUs, MotorControlCAN::LICHUAN_C::CommunicationNmt& output) const noexcept;
    void capture(bool tail) noexcept;
    void lose(uint32_t count, bool tail) noexcept;
    void finish(HeartbeatBenchFailure why, uint64_t nowUs) noexcept;
    bool withdraw(uint64_t nowUs) noexcept;
};
} // namespace MotorControlCANExample
