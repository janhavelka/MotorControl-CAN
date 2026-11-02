/** @file PdoConfiguration.h
 * @brief Explicit bounded TPDO1 experiment/qualified-fixture configuration.
 * EN86 p16/EDS expose RW maps; CiA's variable-mapping procedure supplies the
 * conditional order. This does not qualify every CL86-C firmware. No I/O,
 * allocation, clock, NMT, motor command, retry, rollback or save occurs here.
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
/** Explicit finite sequence state, independent of native drive/NMT state. */
enum class PdoConfigurationState : uint8_t {
    EMPTY,        ///< Unprepared.
    WRITING,      ///< Six ordered acknowledged downloads pending.
    READING_BACK, ///< Nine exact final communication/mapping uploads pending.
    SUCCEEDED,    ///< Complete exact readback matched; samples still unqualified.
    FAILED        ///< First failure retained with partial/unknown effects.
};
/** Fixed caller-owned context. Do not edit active fields; serialize access.
 * The one allowed candidate changes map3 [6041,6061,6064] to map2 [6041,6064].
 * COB-ID flags, type255, inhibit100, event0 and inactive slots are preserved.
 * Six writes: disable, count0, slot1, slot2, count2, enable. Nine final reads:
 * 1800:01/02/03/05 and 1A00:00..04. One failed step never rolls back/replays.
 */
struct Tpdo1ConfigurationContext {
    SettingsReservation reservation; ///< Actual continuous owner exclusion, frozen original route.
    Tpdo1ConfigurationReview review; ///< Owned admission provenance; never promoted automatically.
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
    uint8_t cursor = 0;               ///< Write0..5 or read0..8 according to state.
    uint8_t acknowledgedMask = 0;     ///< Bits0..5 positively acknowledged downloads.
    uint8_t readbackCount = 0;        ///< Exact acquired prefix count, 0..9.
    bool transmissionNoted = false;   ///< Current download crossed possible-TX boundary.
    bool effectsPossible = false;     ///< First possible write withdrew configuration epoch.
    bool uncertain = false;           ///< Unconfirmed write/read effects or incomplete map remain.
    bool mapConfirmed = false;        ///< Only full successful final readback sets true.
    CANOPEN::SdoCompletion evidence;  ///< Latest retained full terminal, including unsigned abort.
    bool evidencePresent = false;     ///< Makes evidence meaningful; no traffic implied by default.
};
/** Validate a complete exact baseline and prepare the one fixed map2 candidate.
 * @param[out] output Owned context, unchanged on failure; no overlap with inputs.
 * @param[in] reservation Actual continuous CL86_C/OL86_C owner reservation.
 * @param[in] review Explicit exact fixture/firmware experiment or qualification records.
 * @param[in] baseline Nine successful EXACT SDO upload completions in documented order.
 * All must carry requirePreOperational, follow reservation and confirmed preOperational.processedUs, share its
 * node epoch and original binding, be fresh, sequential and have actual widths.
 * Required values: enabled standard COB-ID, type255/inhibit100/event0,
 * count3 and 60410010,60610008,60640020,0. No EDS default substitution.
 * @param[in] count Exactly nine; null rejects.
 * @param[in] nmt Actual currently qualified NMT state; must be PRE_OPERATIONAL.
 * @param[in] preOperational Actual owner observation with exact capture OR conservative
 * receive bounds, matching node epoch; not an inferred SDO state or dequeue timestamp.
 * @param[in] operation Nonzero unique live work identity.
 * @param[in] nowUs Supplied nondecreasing microseconds, zero valid.
 * @param[in] deadlineUs Exclusive immutable overall cutoff after nowUs.
 * @param[in] options Nonzero caller service budgets, not device timing guarantees.
 * @param[in] maximumAgeUs Nonzero inclusive earliest-bound age policy.
 * @return Local validation only. All work uses existing public SDO services;
 * experiment scope grants no ordinary service permission or production capability.
 * Every yielded service carries requirePreOperational; the selected adapter must
 * enforce it again at admission, handoff and actual controller enqueue.
 */
Status prepareTpdo1Configuration(Tpdo1ConfigurationContext& output,
                                 const SettingsReservation& reservation,
                                 const Tpdo1ConfigurationReview& review,
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
 */
Status nextPdoConfiguration(const Tpdo1ConfigurationContext& context,
                            const SettingsReservation& live, const Binding& current,
                            CANOPEN::NmtState nmt, const CANOPEN::ObservationTime& preOperational,
                            uint64_t nowUs, PreparedRead& output) noexcept;
/** Record possible download TX before owner handoff, never send or retry.
 * @param[in,out] context Active download, one notice per step.
 * @param[in] live Actual reservation. @param[in,out] current Actual binding configuration
 * advances exactly once. Withdraw dependent maps/samples immediately; no sample proof follows.
 * @param[in] nmt Actual current state. @param[in] preOperational Current qualified observation.
 * @param[in] nowUs Supplied nondecreasing time before cutoff.
 * @return Atomic validation status; mutable arguments and live/time must not overlap.
 */
Status notePdoConfigurationTransmission(Tpdo1ConfigurationContext& context,
                                        const SettingsReservation& live, Binding& current,
                                        CANOPEN::NmtState nmt,
                                        const CANOPEN::ObservationTime& preOperational,
                                        uint64_t nowUs) noexcept;
/** Consume one actual matching retained owner terminal, not a fabricated response.
 * @param[in,out] context Active context. @param[in] live Actual continuous reservation.
 * @param[in] current Actual current binding. @param[in] evidence Borrowed full terminal.
 * @param[in] nowUs Supplied processing time at least terminal and previous progression.
 * @return Local validation, unchanged on invalid evidence/overlap. Valid failure
 * settles context without rollback. Caller revokes reservation on NMT/lifecycle change;
 * admission/enqueue checks current NMT separately. Deadline wins at equality.
 */
Status advancePdoConfiguration(Tpdo1ConfigurationContext& context, const SettingsReservation& live,
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
 * @param[in] context Successful sequence. @param[in] live Actual reservation.
 * @param[in] current Actual next-generation binding. @param[in] nowUs Supplied time.
 * @param[in] mappingGeneration New nonzero mapping epoch allocated by sole owner.
 * @param[in] evidenceId Nonzero retained readback record identity, not a wire tag.
 * @param[out] output Owned map evidence, unchanged on failure/no input overlap.
 * @return Local current/fresh contents validation. No retirement boundary, physical
 * capture, NMT inference, device firmware capability or old-frame qualification follows.
 */
Status getTpdo1Configuration(const Tpdo1ConfigurationContext& context,
                             const SettingsReservation& live, const Binding& current,
                             uint64_t nowUs, Generation mappingGeneration, uint32_t evidenceId,
                             CANOPEN::PdoMapEvidence& output) noexcept;
} // namespace LICHUAN_C
} // namespace MotorControlCAN
