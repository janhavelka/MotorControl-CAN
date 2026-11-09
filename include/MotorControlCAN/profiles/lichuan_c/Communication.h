/** @file Communication.h
 * @brief Explicit communication acquisition and one reviewed heartbeat transition.
 * No I/O, clock, allocation or NMT. PDO templates/readbacks do not qualify live maps.
 * Only the explicit reviewed heartbeat workflow yields a download; reads never do.
 * All new PDO/2203 rows retain UNQUALIFIED/Q20 effects and need one-use reviewed acquisition
 * through the existing owner. NETWORK preserves prompt 19's read semantics.
 * SPDX-License-Identifier: MIT
 */
#pragma once
#include <MotorControlCAN/canopen/Network.h>
#include <MotorControlCAN/profiles/lichuan_c/Reads.h>
#include <MotorControlCAN/profiles/lichuan_c/Settings.h>

namespace MotorControlCAN {
namespace LICHUAN_C {
/** Finite native read groups; no generic object accessor or communication write permission. */
enum class CommunicationGroup : uint8_t {
    NETWORK,            ///< Existing 1005/1014/1017/1200 configuration read, six fields.
    NODE_SETTING,       ///< 2203 stored U16 selector, not the active node or DIP position.
    RPDO_COMMUNICATION, ///< One 1400..1403 record, :00..05, selected by options.pdoNumber.
    TPDO_COMMUNICATION, ///< One 1800..1803 record, :00..05.
    RPDO_MAPPING,       ///< One 1600..1603 record, :00..04, including inactive slots.
    TPDO_MAPPING        ///< One 1A00..1A03 record, :00..04.
};
/** Caller-owned finite read. Raw partial failure/effect evidence remains in read until reset. */
struct CommunicationReadContext {
    ReadContext read; ///< Same bounded service lifecycle, no duplicate SDO engine.
};
/** Raw communication record; native units/flags are not established live settings. */
struct PdoCommunicationObservation {
    uint8_t entriesRaw = 0; ///< :00 count, not an acquisition bound.
    uint32_t cobIdRaw = 0;  ///< :01 complete flags/identifier word, not active-route proof.
    uint8_t transmissionTypeRaw = 0; ///< :02 uninterpreted native value.
    uint16_t inhibitTimeRaw = 0;     ///< :03 raw units remain Q17 gated.
    uint8_t compatibilityRaw = 0;    ///< :04 preserved even when reserved/unused.
    uint16_t eventTimerRaw = 0;      ///< :05 raw units and zero semantics remain Q17 gated.
};
/** Raw mapping record, distinct from confirmed map contents and qualified PDO samples. */
struct PdoMappingObservation {
    uint8_t countRaw = 0; ///< :00 observed count; even invalid values are retained as raw evidence.
    uint32_t words[4] = {}; ///< :01..04 exact native words, including inactive entries.
};
/** Complete group publication, not an atomic device snapshot or persistence claim. */
struct CommunicationObservation {
    ReadTarget target; ///< Exact original channel, binding/configuration and node epochs.
    CommunicationGroup group = CommunicationGroup::NETWORK; ///< Which fields below are meaningful.
    uint8_t pdoNumber = 0;  ///< 1..4 for PDO groups, zero otherwise; profile-local capacity only.
    uint8_t fieldCount = 0; ///< 1, 5 or 6 independently acquired fields.
    ReadWindow window;      ///< Earliest/latest original evidence and skew, zero timestamps valid.
    ReadProvenance fields[6];  ///< Owned per-field provenance; only fieldCount entries meaningful.
    ConfigObservation network; ///< NETWORK only; includes raw 1017, not a vendor-confirmed ms unit.
    uint16_t nodeSettingRaw = 0; ///< NODE_SETTING only; zero/unknown values never create node zero.
    PdoCommunicationObservation communication; ///< RPDO/TPDO_COMMUNICATION only.
    PdoMappingObservation mapping;             ///< RPDO/TPDO_MAPPING only.
};
/** Prepare one explicit group without selecting, configuring or contacting a device.
 * @param[out] output Replaced only on success; overlap with inputs rejects unchanged.
 * @param[in] target Configured exact channel and nonzero node epoch.
 * @param[in] operation Nonzero caller identity, not a wire correlation token.
 * @param[in] nowUs Supplied monotonic microseconds, zero valid.
 * @param[in] deadlineUs Immutable future overall cutoff.
 * @param[in] options Nonzero queue/response budgets; pdoNumber 1..4 only for PDO groups.
 * @param[in] group Explicit finite record group; all inactive/reserved raw fields are acquired.
 * @return Local validation only. Q20 reviewed acquisition and native service eligibility remain
 * independent requirements at admission. No download, NMT, save, reset or guessed route.
 */
Status prepareCommunicationRead(CommunicationReadContext& output, const ReadTarget& target,
                                uint32_t operation, uint64_t nowUs, uint64_t deadlineUs,
                                const ReadOptions& options, CommunicationGroup group) noexcept;
/** @param[in] context Prepared current group. @param[in] nowUs Supplied monotonic us.
 * @param[out] output Unchanged on error; ready=false for terminal context, as nextRead.
 * @return Next finite typed upload; no I/O and no automatic retry.
 */
Status nextCommunication(const CommunicationReadContext& context, uint64_t nowUs,
                         PreparedRead& output) noexcept;
/** @param[in,out] context Owned context. @param[in] input Exact matched service evidence.
 * @param[in] nowUs Supplied delivery us. @return Same atomic matching/failure contract as advanceRead.
 */
Status advanceCommunication(CommunicationReadContext& context, const CANOPEN::SdoCompletion& input,
                            uint64_t nowUs) noexcept;
/** @param[in,out] context Owned context. @param[in] reason Explicit local terminal reason.
 * @param[in] nowUs Supplied monotonic us. @return Retains partial values and unknown effects; no stop.
 */
Status advanceCommunication(CommunicationReadContext& context, ReadOutcome reason,
                            uint64_t nowUs) noexcept;
/** Publish a complete raw group with its original provenance and skew.
 * @param[in] context Successful correctly typed group; partial failure remains raw history there.
 * @param[in] current Exact current target/configuration/node epochs.
 * @param[in] nowUs Supplied publication us, no clock read.
 * @param[in] maximumAgeUs Nonzero inclusive earliest-evidence age cap.
 * @param[out] output Unchanged on wrong kind, overlap, incomplete, stale, invalidated or failed read.
 * @return Local checked publication only. Stored settings are not actual routes, persistent values,
 * DIP position, qualified maps or sample provenance; no I/O, write, NMT or implicit reconciliation.
 */
Status getCommunication(const CommunicationReadContext& context, const ReadTarget& current,
                        uint64_t nowUs, uint64_t maximumAgeUs,
                        CommunicationObservation& output) noexcept;
/** Explicit provenance for the single measured heartbeat transition. */
enum class CommunicationScope : uint8_t {
    UNQUALIFIED,         ///< Invalid default; no write work.
    ISOLATED_EXPERIMENT, ///< Caller-reviewed finite experiment; no production capability.
    FIXTURE_QUALIFIED    ///< Exact specimen/firmware procedure established externally.
};
/** Caller attestations, never inferred from SDO success or raw identity codes. */
struct CommunicationReview {
    ReadTarget target; ///< Exact original route, binding/configuration and node epochs.
    CommunicationScope scope = CommunicationScope::UNQUALIFIED; ///< Explicit acquisition scope.
    uint32_t fixtureRecord = 0;       ///< Nonzero retained actual fixture record.
    uint32_t firmwareRecord = 0;      ///< Nonzero exact firmware/specimen record.
    uint32_t procedureRecord = 0;     ///< Nonzero independently reviewed procedure record.
    uint32_t authorizationRecord = 0; ///< Nonzero execution authorization record.
};
/** One explicit native candidate; no bitrate, route, node or persistence setter. */
struct CommunicationRequest {
    uint16_t producerTimeRaw = UINT16_MAX; ///< Only 1000, from fresh raw 0, is supported.
};
/** Actual owner NMT observation, independent of service availability. */
struct CommunicationNmt {
    bool known = false; ///< False means unknown, not Operational; time is ignored then.
    CANOPEN::NmtState state = CANOPEN::NmtState::INITIALIZING; ///< Meaningful only when known.
    CANOPEN::ObservationTime time; ///< Qualified exact capture or receive bounds when known.
};
/** Finite local workflow phase, never a network or drive state. */
enum class CommunicationState : uint8_t {
    EMPTY,                 ///< Unprepared.
    WRITING,               ///< The one 1017:00 U16 download awaits a matched terminal.
    AWAITING_CONFIRMATION, ///< Acknowledged download; explicit confirmation preparation required.
    READING_BACK,          ///< One exact 1017:00 U16 upload at the next configuration epoch.
    SUCCEEDED,             ///< Acknowledged write plus matching stored-value readback.
    FAILED                 ///< Immutable failed intent; independent later reads cannot resume it.
};
/** Unknown device requirements stay separate from verified register contents. */
enum class CommunicationRequirement : uint8_t {
    UNKNOWN,     ///< Applicable activation/restart/persistence requirement is not established.
    REQUIRED,    ///< Reserved for source-established requirements; not inferred here.
    NOT_REQUIRED ///< Reserved for source-established absence; not inferred here.
};
/** Caller-owned single download and explicit single readback, using existing SDO services.
 * Exact supported change: CL86_C, 1017:00 U16 raw 0 to raw 1000, as observed on the
 * retained bench specimen. No claim covers arbitrary values, disable, other firmware,
 * host-loss action, motion stop or persistent activation. Effects and terminals survive
 * local failure until the caller explicitly replaces this context.
 */
struct CommunicationContext {
    AxisReservation reservation;  ///< Actual continuous exclusion, original route frozen.
    CommunicationReview review;   ///< Owned original review; success never promotes its scope.
    CommunicationRequest request; ///< Owned exact candidate, frozen at preparation.
    CommunicationState state = CommunicationState::EMPTY; ///< Current finite phase.
    ReadOutcome outcome = ReadOutcome::NONE;      ///< Workflow result, separate from write result.
    ReadOutcome writeOutcome = ReadOutcome::NONE; ///< Immutable after the download terminal.
    Status status;              ///< Workflow status, never a health or motion-completion claim.
    Status writeStatus;         ///< Original download status, retained through readback failure.
    ReadOptions options;        ///< Nonzero caller queue/response budgets, not device guarantees.
    uint64_t maximumAgeUs = 0;  ///< Nonzero inclusive baseline/NMT/readback age policy.
    uint64_t startedUs = 0;     ///< Supplied preparation time; zero valid.
    uint64_t servicedUs = 0;    ///< Last accepted mutation time.
    uint64_t stepStartedUs = 0; ///< Earliest allowed admission of current native step.
    uint64_t deadlineUs = 0;    ///< Immutable exclusive cutoff; confirmation cannot extend it.
    uint32_t operation = 0;     ///< Nonzero caller operation/request identity, not on the wire.
    bool requirePreOperational =
        false;                      ///< Frozen service prerequisite; unknown is not Operational.
    bool transmissionNoted = false; ///< Pre-handoff download notice accepted exactly once.
    bool effectsPossible = false;   ///< Possible download withdrew dependent configuration.
    bool prerequisitesWithdrawn = false; ///< Caller must withdraw producer/maps/reference/work too.
    bool writeAcknowledged = false;      ///< Matched SDO acknowledgement, not physical activation.
    bool uncertain = false;     ///< Unknown write/read effects or incomplete register confirmation.
    bool readbackKnown = false; ///< Raw successful upload is retained even on mismatch.
    bool readbackMatched = false; ///< Exact raw 1000 readback before the immutable cutoff.
    uint16_t readbackRaw = 0;     ///< Meaningful only with readbackKnown; not a persistence claim.
    CommunicationRequirement restart = CommunicationRequirement::UNKNOWN; ///< Always unknown here.
    CommunicationRequirement persistence =
        CommunicationRequirement::UNKNOWN; ///< Always unknown here.
    CANOPEN::SdoCompletion baseline;       ///< Owned exact fresh 1017 raw 0 upload, original epoch.
    CANOPEN::SdoCompletion
        writeEvidence; ///< Owned download terminal, meaningful if writeEvidencePresent.
    bool writeEvidencePresent =
        false; ///< Local withdrawal alone does not fabricate a native terminal.
    CANOPEN::SdoCompletion
        confirmationEvidence;         ///< Owned upload terminal when confirmationPresent.
    bool confirmationPresent = false; ///< Confirmation evidence is meaningful.
};
/** Checked stored configuration observation, never an effective producer or persistent setting. */
struct CommunicationResult {
    ReadTarget target; ///< Exact next configuration epoch and unchanged route/node epoch.
    uint16_t producerTimeRaw = 0; ///< Matching stored raw 1000; timing remains specimen scoped.
    ReadWindow window; ///< Original confirmation service interval; zero timestamps valid.
    CommunicationScope scope = CommunicationScope::UNQUALIFIED; ///< Original caller review scope.
    CommunicationRequirement restart = CommunicationRequirement::UNKNOWN;     ///< Still unresolved.
    CommunicationRequirement persistence = CommunicationRequirement::UNKNOWN; ///< Still unresolved.
};
/** Prepare only the exact reviewed 1017 raw 0 to 1000 transition, without I/O.
 * @param[out] output Replaced only on success; no input overlap.
 * @param[in] reservation Actual continuous CL86_C owner exclusion, acquired before baseline.
 * @param[in] review Exact target and nonzero fixture/firmware/procedure/authorization records.
 * @param[in] request Only producerTimeRaw=1000; no implicit default or reverse transition.
 * @param[in] baseline Fresh exact U16 successful unqualified/accepted 1017:00 upload of raw 0.
 * @param[in] nmt Actual state; unknown permits only ISOLATED_EXPERIMENT with this service baseline;
 * known state must be fresh qualified PRE_OPERATIONAL. Stopped never becomes serviceable by a read.
 * @param[in] operation Nonzero unique live operation/request identity.
 * @param[in] nowUs Supplied monotonic microseconds, zero valid.
 * @param[in] deadlineUs Immutable exclusive cutoff after nowUs, including queue/response budgets.
 * @param[in] options Nonzero budgets; no identity/count width override or PDO selector.
 * @param[in] maximumAgeUs Nonzero inclusive baseline/NMT/result age cap.
 * @return Local validation only. No write permission, owner reservation, NMT or model is fabricated.
 * Owner premise this profile cannot see: confirmation needs the application's owner to move its
 * reservation to the next configuration epoch after the acknowledged write, and the reference
 * adapter refuses that without current ordinary SDO service or with a pending NMT transition for
 * the node (SdoClient::configurationEligible). Neither follows from the granted baseline or the
 * write. Check it before the download is admitted: after a possible TX a refused migration leaves
 * the write acknowledged, FAILED and uncertain, never confirmed.
 */
Status prepareCommunication(CommunicationContext& output, const AxisReservation& reservation,
                            const CommunicationReview& review, const CommunicationRequest& request,
                            const CANOPEN::SdoCompletion& baseline, const CommunicationNmt& nmt,
                            uint32_t operation, uint64_t nowUs, uint64_t deadlineUs,
                            const ReadOptions& options, uint64_t maximumAgeUs) noexcept;
/** Arm one explicit confirmation after the acknowledged write and actual owner epoch update.
 * @param[in,out] context AWAITING_CONFIRMATION only; failed operations cannot resume.
 * @param[in] live Actual same continuous reservation, not a stale copied lease.
 * @param[in] current Actual adapter target after owner.advanceConfiguration; exactly next config
 * epoch, frozen channel COB-IDs and unchanged node/binding epochs. Core cannot update owner storage;
 * that migration has its own premises (see prepareCommunication).
 * @param[in] nmt Current qualified Pre-operational or experiment's unknown serviceable state.
 * @param[in] nowUs Monotonic supplied time; original deadline and budgets remain unchanged.
 * @return Atomic status. Caller first releases native write result and establishes local retirement.
 */
Status prepareCommunicationConfirmation(CommunicationContext& context, const AxisReservation& live,
                                        const ReadTarget& current, const CommunicationNmt& nmt,
                                        uint64_t nowUs) noexcept;
/** Yield the current single native step, without admission, replay or I/O.
 * @param[in] context Prepared write, explicitly armed readback or terminal context.
 * @param[in] live Actual same reservation.
 * @param[in] current Actual current binding: original before notice, next afterwards.
 * @param[in] nmt Actual current state, rechecked independently by owner before enqueue.
 * @param[in] nowUs Nondecreasing time before active deadline; finite service budgets must fit.
 * @param[out] output Unchanged on refusal; ready=false while awaiting confirmation or terminal.
 * @return Local status; step1 download uses original route/epoch, step2 upload uses next epoch.
 */
Status nextCommunication(const CommunicationContext& context, const AxisReservation& live,
                         const Binding& current, const CommunicationNmt& nmt, uint64_t nowUs,
                         PreparedRead& output) noexcept;
/** Withdraw configuration before possible download handoff, exactly once.
 * @param[in,out] context Active download, untouched on refusal.
 * @param[in] live Actual continuous reservation.
 * @param[in,out] current Actual application binding; configuration generation advances once.
 * @param[in] nmt Actual current state with applicable evidence.
 * @param[in] nowUs Supplied monotonic time before original cutoff, with service budget remaining.
 * @return Atomic local status. Call after admission and before SdoClient::nextTx/handoff.
 * Caller withdraws effective producer confidence, mappings, reference and prepared work at this
 * boundary. This function sends nothing, clears no quarantine and does not update adapter storage.
 */
Status noteCommunicationTransmission(CommunicationContext& context, const AxisReservation& live,
                                     Binding& current, const CommunicationNmt& nmt,
                                     uint64_t nowUs) noexcept;
/** Consume one exact current retained owner terminal, preserving original download evidence.
 * @param[in,out] context WRITING or READING_BACK; invalid input leaves it unchanged.
 * @param[in] live Actual continuous reservation; revoke on reset/rebind/state contradiction.
 * @param[in] current Actual expected application binding.
 * @param[in] evidence Exact matched original/native work, including unsigned abort and effects.
 * @param[in] nowUs Supplied time at least completion and prior mutation; cutoff wins at equality.
 * @return Local consumption status. A failed terminal ends intent permanently, without retry,
 * guessed route, implicit NMT/save/reset or automatic readback. ACK alone awaits explicit work.
 */
Status advanceCommunication(CommunicationContext& context, const AxisReservation& live,
                            const Binding& current, const CANOPEN::SdoCompletion& evidence,
                            uint64_t nowUs) noexcept;
/** Withdraw host work after the owner separately establishes its retirement.
 * @param[in,out] context Nonterminal prepared workflow; terminal contexts cannot be rewritten.
 * @param[in] reason CANCELLED, DEADLINE or INVALIDATED; never a motor stop.
 * @param[in] nowUs Nondecreasing supplied time; deadline wins at equality.
 * @return Atomic local status; possible effects and all earlier terminals remain retained.
 */
Status advanceCommunication(CommunicationContext& context, ReadOutcome reason,
                            uint64_t nowUs) noexcept;
/** Publish only matching complete current stored-value confirmation.
 * @param[in] context Successful acknowledged-and-confirmed workflow.
 * @param[in] live Actual continuous reservation.
 * @param[in] current Actual next-epoch adapter route and unchanged node epoch.
 * @param[in] nowUs Supplied publication time, not before confirmation delivery.
 * @param[out] output Unchanged on failure, staleness, invalidation or overlap.
 * @return Checked raw observation, not active timing, persistence, identity or host-loss behavior.
 */
Status getCommunication(const CommunicationContext& context, const AxisReservation& live,
                        const ReadTarget& current, uint64_t nowUs,
                        CommunicationResult& output) noexcept;

} // namespace LICHUAN_C
} // namespace MotorControlCAN
