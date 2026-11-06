/** @file PdoConfiguration.h
 * @brief Explicit bounded TPDO1 configuration: two reviewed transitions and nothing else.
 * EN86 p16/EDS expose RW communication and mapping records; CiA's variable-mapping
 * procedure supplies the conditional order of the map change. The 1800:05 event-timer
 * transition rests on exact baseline, write and readback measurements in Pre-operational on
 * two CL86-C specimens of one firmware identity (bench continuation packet 6). Neither
 * qualifies every CL86-C firmware. No I/O, allocation, clock, NMT, motor command, retry,
 * rollback or save occurs here.
 * SPDX-License-Identifier: MIT
 */
#pragma once
#include <MotorControlCAN/canopen/Network.h>
#include <MotorControlCAN/profiles/lichuan_c/Settings.h>

namespace MotorControlCAN {
namespace LICHUAN_C {
/** Admission provenance, never inferred from a successful service exchange. */
enum class PdoConfigurationScope : uint8_t {
    UNQUALIFIED,         ///< Invalid default; no work.
    ISOLATED_EXPERIMENT, ///< Explicit finite experiment, not a production capability.
    FIXTURE_QUALIFIED    ///< Caller-established exact fixture/firmware procedure qualification.
};
/** Caller attestations scoped to the actual reservation's specimen/firmware.
 * Record identifiers refer to retained external evidence, not wire tokens or
 * authentication. No success automatically promotes ISOLATED_EXPERIMENT.
 */
struct Tpdo1ConfigurationReview {
    ReadTarget target; ///< Exact original binding/channel/node epoch.
    PdoConfigurationScope scope = PdoConfigurationScope::UNQUALIFIED; ///< Explicit scope.
    uint32_t fixtureRecord = 0;       ///< Nonzero established fixture record.
    uint32_t firmwareRecord = 0;      ///< Nonzero exact raw firmware/specimen evidence record.
    uint32_t procedureRecord = 0;     ///< Nonzero source and independent procedure review record.
    uint32_t authorizationRecord = 0; ///< Nonzero explicit execution authorization record.
};
/** The one reviewed TPDO1 change a context carries. No other edit can be prepared. */
enum class Tpdo1Transition : uint8_t {
    NONE,         ///< Invalid default; preparation rejects it.
    MAP3_TO_MAP2, ///< Six writes: map [6041,6061,6064] to [6041,6064]; isolated node-1 experiment origin.
    EVENT_TIMER   ///< One 1800:05 write between the two measured raw literals; all else kept.
};
/** Raw U16 values of TPDO1 event timer 1800:05 (EDS RW U16, not PDO-mappable) this profile writes.
 * These are raw values, not a time setting: no unit is established. On the two CL86-C bench
 * specimens, raw 100 gave periods of about 101 ms on the S2's clock while Operational, and
 * raw 0 gave no periodic frame while the drive stood still. Neither fact is claimed for other
 * firmware, other values, change-driven sending or persistence. Raw 0 to raw 100 and raw 100
 * to raw 0, with exact readback in Pre-operational, are the only transitions measured on both
 * specimens; 200 to 0 and 10 to 0 were measured on node 2 only and are not supported. No other
 * value can be prepared.
 */
enum class Tpdo1EventTimerRaw : uint16_t {
    RAW_0 = 0,    ///< Raw 0, the specimens' observed baseline; no claim about what zero means.
    RAW_100 = 100 ///< Raw 100; unit not established (see the enumeration note).
};
/** Small typed intent, copied and frozen at preparation. The default is invalid. */
struct Tpdo1ConfigurationRequest {
    Tpdo1Transition transition = Tpdo1Transition::NONE; ///< Explicit transition; NONE rejects.
    /** Raw 1800:05 literal the final readback must hold. EVENT_TIMER: the target, and the
     * baseline must read the other literal, so only 0 to 100 or 100 to 0. MAP3_TO_MAP2:
     * RAW_0, the value its baseline requires and the map change preserves.
     */
    Tpdo1EventTimerRaw eventTimer = Tpdo1EventTimerRaw::RAW_0;
};
/** Explicit finite sequence state, independent of native drive/NMT state. */
enum class PdoConfigurationState : uint8_t {
    EMPTY,        ///< Unprepared.
    WRITING,      ///< Ordered acknowledged downloads pending: six for the map, one for the timer.
    READING_BACK, ///< Nine exact final communication/mapping uploads pending.
    SUCCEEDED,    ///< Complete exact readback matched; samples still unqualified.
    FAILED        ///< First failure retained with partial/unknown effects.
};
/** Fixed caller-owned context. Do not edit active fields; serialize access.
 * Steps are numbered 1..W for the W downloads, then W+1..W+9 for the nine final uploads
 * 1800:01/02/03/05 and 1A00:00..04; W is 6 for MAP3_TO_MAP2 and 1 for EVENT_TIMER.
 * MAP3_TO_MAP2 writes disable, count0, slot1, slot2, count2, enable, and preserves COB-ID
 * flags, type255, inhibit100, event0 and inactive slots. EVENT_TIMER writes 1800:05 only.
 * One failed step never rolls back or replays; possible earlier effects stay retained.
 */
struct Tpdo1ConfigurationContext {
    AxisReservation reservation; ///< Actual continuous owner exclusion, frozen original route.
    Tpdo1ConfigurationReview review; ///< Owned admission provenance; never promoted automatically.
    Tpdo1ConfigurationRequest request; ///< Frozen copied intent; never changed after preparation.
    PdoConfigurationState state = PdoConfigurationState::EMPTY; ///< Current finite phase.
    ReadOutcome outcome = ReadOutcome::NONE; ///< Terminal cause, not health or motion.
    Status status;       ///< Retained operation status, distinct from progression call status.
    ReadOptions options; ///< Explicit nonzero service budgets, clamped to deadline.
    uint64_t maximumAgeUs = 0;  ///< Nonzero inclusive conservative NMT/baseline/result age policy.
    uint64_t startedUs = 0;     ///< Preparation processing time, zero valid.
    uint64_t servicedUs = 0;    ///< Last accepted mutation time.
    uint64_t stepStartedUs = 0; ///< Earliest admission of current step.
    uint64_t deadlineUs = 0;    ///< Immutable exclusive overall cutoff.
    uint64_t readbackStartedUs =
        0; ///< Earliest admitted final upload, meaningful after first read.
    uint64_t readbackCompletedUs = 0; ///< Latest final upload completion, not physical capture.
    uint32_t operation = 0;           ///< Nonzero live operation/request identity.
    uint32_t original[9] = {};        ///< Complete validated original communication/map snapshot.
    uint32_t readback[9] = {};        ///< Only readbackCount acquired values are meaningful.
    uint8_t cursor = 0;               ///< Index of the current write or read according to state.
    uint8_t acknowledgedMask = 0; ///< Bit k: write step k+1 acknowledged, even if then expired.
    uint8_t confirmedStep = 0; ///< Last step acknowledged or read back matching before the cutoff; 0 none.
    uint8_t firstEffectStep = 0; ///< Write step whose possible TX first withdrew the configuration; 0 none.
    /** FAILED only: the step current when the sequence ended, never confirmed. Its terminal
     * failed, mismatched or expired, or none was consumed (local withdrawal). A write step
     * noted as possibly sent without acknowledgement has an unknown effect. 0 otherwise.
     */
    uint8_t failedStep = 0;
    uint8_t readbackCount = 0;        ///< Exact acquired prefix count, 0..9.
    bool transmissionNoted = false;   ///< Current download crossed possible-TX boundary.
    bool effectsPossible = false;     ///< First possible write withdrew configuration epoch.
    bool uncertain = false;           ///< Unconfirmed write/read effects or incomplete map remain.
    bool mapConfirmed = false;        ///< Only full successful final readback sets true.
    CANOPEN::SdoCompletion evidence;  ///< Latest retained full terminal, including unsigned abort.
    bool evidencePresent = false;     ///< Makes evidence meaningful; no traffic implied by default.
};
/** Prepare the fixed map3-to-map2 change; kept for existing callers.
 * Identical to the request overload with transition MAP3_TO_MAP2 and eventTimer RAW_0; see
 * that overload for every parameter, check and result.
 * @param[out] output Owned context, unchanged on failure.
 * @param[in] reservation Actual continuous CL86_C/OL86_C owner reservation.
 * @param[in] review Explicit exact fixture/firmware experiment or qualification records.
 * @param[in] baseline Nine successful EXACT SDO upload completions in documented order.
 * @param[in] count Exactly nine; null rejects.
 * @param[in] nmt Actual currently qualified NMT state; must be PRE_OPERATIONAL.
 * @param[in] preOperational Actual owner observation with exact capture or receive bounds.
 * @param[in] operation Nonzero unique live work identity.
 * @param[in] nowUs Supplied nondecreasing microseconds, zero valid.
 * @param[in] deadlineUs Exclusive immutable overall cutoff after nowUs.
 * @param[in] options Nonzero caller service budgets, not device timing guarantees.
 * @param[in] maximumAgeUs Nonzero inclusive earliest-bound age policy.
 * @return Local validation only, as for the request overload.
 */
Status prepareTpdo1Configuration(Tpdo1ConfigurationContext& output,
                                 const AxisReservation& reservation,
                                 const Tpdo1ConfigurationReview& review,
                                 const CANOPEN::SdoCompletion* baseline, size_t count,
                                 CANOPEN::NmtState nmt,
                                 const CANOPEN::ObservationTime& preOperational, uint32_t operation,
                                 uint64_t nowUs, uint64_t deadlineUs, const ReadOptions& options,
                                 uint64_t maximumAgeUs) noexcept;
/** Validate a complete exact baseline and prepare one reviewed TPDO1 transition.
 * Every check below precedes any change to output; no work exists until nextPdoConfiguration.
 * @param[out] output Owned context, unchanged on failure; no overlap with inputs.
 * @param[in] reservation Actual continuous CL86_C/OL86_C owner reservation; EVENT_TIMER needs
 * CL86_C, the only model it was measured on. Its binding's profile identity must represent
 * the published map through pdoProfile().
 * @param[in] review Explicit exact fixture/firmware experiment or qualification records,
 * scope ISOLATED_EXPERIMENT or FIXTURE_QUALIFIED, target equal to the reservation's.
 * @param[in] request Copied intent. NONE, a MAP3_TO_MAP2 request whose eventTimer is not
 * RAW_0 and any value outside Tpdo1EventTimerRaw's two literals reject.
 * @param[in] baseline Nine successful EXACT SDO upload completions in documented order:
 * 1800:01/02/03/05, 1A00:00..04. All must carry requirePreOperational, follow reservation and
 * confirmed preOperational.processedUs, share its node epoch and original binding, be fresh,
 * sequential and have the EDS widths. Both transitions require type255, inhibit raw100,
 * count3 and 60410010,60610008,60640020,0. MAP3_TO_MAP2 also requires an enabled standard
 * COB-ID (bits 0..10 nonzero, bit 30 either value) and event raw0. EVENT_TIMER requires COB-ID
 * exactly 0x180 + node, the EDS $NODEID+0x180 rule (valid, standard, bit 30 clear; measured on
 * nodes 1 and 2), and event equal to the literal other than request.eventTimer. No EDS default
 * substitution.
 * @param[in] count Exactly nine; null rejects.
 * @param[in] nmt Actual currently qualified NMT state; must be PRE_OPERATIONAL.
 * @param[in] preOperational Actual owner observation with exact capture OR conservative
 * receive bounds, matching node epoch; not an inferred SDO state or dequeue timestamp.
 * @param[in] operation Nonzero unique live work identity.
 * @param[in] nowUs Supplied nondecreasing microseconds, zero valid.
 * @param[in] deadlineUs Exclusive immutable overall cutoff after nowUs.
 * @param[in] options Nonzero caller service budgets, not device timing guarantees.
 * @param[in] maximumAgeUs Nonzero inclusive earliest-bound age policy.
 * @return Ok() when prepared; this admits and sends nothing. Otherwise Err::INVALID_CONFIG
 * with a ReadError detail, checked in this order: ARGUMENT for a null or short baseline;
 * OVERLAP; ARGUMENT for request, scope, records, reservation, NMT evidence, deadline or
 * budgets; then Err::UNSUPPORTED, detail ReadError::TARGET, for EVENT_TIMER on a model other
 * than CL86_C; CORRELATION for baseline provenance; VALUE for baseline values outside the
 * transition or a map the binding's profile cannot represent. All work uses existing
 * public SDO services; experiment scope grants no ordinary service permission or production
 * capability. Every yielded service carries requirePreOperational; the selected adapter must
 * enforce it again at admission, handoff and actual controller enqueue.
 */
Status prepareTpdo1Configuration(Tpdo1ConfigurationContext& output,
                                 const AxisReservation& reservation,
                                 const Tpdo1ConfigurationReview& review,
                                 const Tpdo1ConfigurationRequest& request,
                                 const CANOPEN::SdoCompletion* baseline, size_t count,
                                 CANOPEN::NmtState nmt,
                                 const CANOPEN::ObservationTime& preOperational, uint32_t operation,
                                 uint64_t nowUs, uint64_t deadlineUs, const ReadOptions& options,
                                 uint64_t maximumAgeUs) noexcept;
/** Yield copied work, without admission or transmission.
 * @param[in] context Prepared fixed sequence. @param[in] live Actual continuous reservation.
 * @param[in] current Actual binding; original epoch before first possible TX, next afterwards.
 * @param[in] nmt Current actual qualified state; PRE_OPERATIONAL while active.
 * @param[in] preOperational Current actual owner observation with qualified bounds/capture.
 * @param[in] nowUs Supplied nondecreasing time; active calls at cutoff reject.
 * @param[out] output Unchanged on rejection, ready=false at terminal; no input overlap.
 * @return Local status. Downloads retain old transport epoch; final uploads use
 * next epoch after owner retirement/reconciliation. Owner checks NMT before enqueue too.
 * The EVENT_TIMER download is 1800:05, two bytes, the requested literal little-endian.
 */
Status nextPdoConfiguration(const Tpdo1ConfigurationContext& context,
                            const AxisReservation& live, const Binding& current,
                            CANOPEN::NmtState nmt, const CANOPEN::ObservationTime& preOperational,
                            uint64_t nowUs, PreparedRead& output) noexcept;
/** Record possible download TX before owner handoff, never send or retry.
 * @param[in,out] context Active download, one notice per step. The first notice also sets
 * firstEffectStep.
 * @param[in] live Actual reservation. @param[in,out] current Actual binding configuration
 * advances exactly once. Withdraw dependent maps/samples immediately; no sample proof follows.
 * @param[in] nmt Actual current state. @param[in] preOperational Current qualified observation.
 * @param[in] nowUs Supplied nondecreasing time before cutoff.
 * @return Atomic validation status; mutable arguments and live/time must not overlap.
 */
Status notePdoConfigurationTransmission(Tpdo1ConfigurationContext& context,
                                        const AxisReservation& live, Binding& current,
                                        CANOPEN::NmtState nmt,
                                        const CANOPEN::ObservationTime& preOperational,
                                        uint64_t nowUs) noexcept;
/** Consume one actual matching retained owner terminal, not a fabricated response.
 * @param[in,out] context Active context. @param[in] live Actual continuous reservation.
 * @param[in] current Actual current binding. @param[in] evidence Borrowed full terminal.
 * @param[in] nowUs Supplied processing time at least terminal and previous progression.
 * @return Local validation, unchanged on invalid evidence/overlap. Valid failure
 * settles context without rollback and records failedStep. Caller revokes reservation on
 * NMT/lifecycle change; admission/enqueue checks current NMT separately. Deadline wins at
 * equality.
 */
Status advancePdoConfiguration(Tpdo1ConfigurationContext& context, const AxisReservation& live,
                               const Binding& current, const CANOPEN::SdoCompletion& evidence,
                               uint64_t nowUs) noexcept;
/** Local withdrawal after separately retiring owner work; never a motor stop.
 * @param[in,out] context Active sequence, prior effects/history retained.
 * @param[in] reason CANCELLED, DEADLINE or INVALIDATED.
 * @param[in] nowUs Supplied nondecreasing time; deadline has precedence.
 * @return Atomic validation status; terminal cannot be rewritten.
 */
Status advancePdoConfiguration(Tpdo1ConfigurationContext& context, ReadOutcome reason,
                               uint64_t nowUs) noexcept;
/** Publish only complete exact readback contents, never sample qualification.
 * MAP3_TO_MAP2 publishes count2 [6041,6064]; EVENT_TIMER publishes the unchanged count3
 * [6041,6061,6064] map. Both carry the original COB-ID parameter and type 255.
 * @param[in] context Successful sequence. @param[in] live Actual reservation.
 * @param[in] current Actual next-generation binding. @param[in] nowUs Supplied time.
 * @param[in] mappingGeneration New nonzero mapping epoch allocated by sole owner.
 * @param[in] evidenceId Nonzero retained readback record identity, not a wire tag.
 * @param[out] output Owned map evidence, unchanged on failure/no input overlap.
 * @return Local current/fresh contents validation. No retirement boundary, physical
 * capture, NMT inference, device firmware capability or old-frame qualification follows.
 */
Status getTpdo1Configuration(const Tpdo1ConfigurationContext& context,
                             const AxisReservation& live, const Binding& current,
                             uint64_t nowUs, Generation mappingGeneration, uint32_t evidenceId,
                             CANOPEN::PdoMapEvidence& output) noexcept;
/** Publish the same checked map evidence together with the confirmed 1800:05 literal.
 * Both outputs change together on success and neither changes on failure.
 * @param[in] context Successful sequence. @param[in] live Actual reservation.
 * @param[in] current Actual next-generation binding. @param[in] nowUs Supplied time.
 * @param[in] mappingGeneration New nonzero mapping epoch allocated by sole owner.
 * @param[in] evidenceId Nonzero retained readback record identity, not a wire tag.
 * @param[out] output Owned map evidence; no overlap with inputs or eventTimer.
 * @param[out] eventTimer Raw 1800:05 literal the final exact readback returned; a raw value
 * whose unit is not established, not a period.
 * @return As the overload without eventTimer; overlap of the two outputs also rejects.
 */
Status getTpdo1Configuration(const Tpdo1ConfigurationContext& context,
                             const AxisReservation& live, const Binding& current,
                             uint64_t nowUs, Generation mappingGeneration, uint32_t evidenceId,
                             CANOPEN::PdoMapEvidence& output,
                             Tpdo1EventTimerRaw& eventTimer) noexcept;
} // namespace LICHUAN_C
} // namespace MotorControlCAN
