/** @file Actions.h
 * @brief Finite source-backed 86C stop command and controller-report observation.
 * SPDX-License-Identifier: MIT
 * Transition contract: CL86-C/OL86-C only, one continuous actual owner reservation
 * acquired before the last positively acknowledged 6040:00 U16=000F. Fresh SDO
 * STATUS (mode 1, low bits 7) and MONITORS (alarm/released clear, running either)
 * must follow that acknowledgement. The application proves no intervening
 * control/mode/target/RPDO/configuration write, uncertain work or lifecycle change.
 * A copied or reacquired token cannot establish this continuous history.
 * control must be the actual latest acknowledged controlword: a successful stop
 * consumes the prior000F eligibility even if its copied token remains available.
 * HALT emits only 010F; QUICK_STOP emits only 000B. Neither enables first.
 * Repeated stop from a prior 010F/000B or released state is outside this subset.
 * After ACK, fresh five-field MONITORS+STATUS SDO cycles require mode 1 and no
 * alarm/release; monitor running clear establishes OBSERVED controller-not-running
 * during that sequential read window. Other mode/alarm/release is outside the
 * observation scope, not proof that physical stopping failed. Statusword dialect
 * bits never decide completion. No 605A/6085, guessed 605D, queue disposition,
 * torque/reference retention, shaft standstill or firmware qualification is claimed.
 * One owner must retire obsolete queued/controller work before stop admission.
 * Independently received EMCY/fault/lifecycle withdrawal must be delivered as a
 * local terminal before accepting later observation completions; an older monitor
 * read never overrides a newer routed fault.
 */
#pragma once
#include <MotorControlCAN/ActionOperation.h>
#include <MotorControlCAN/profiles/lichuan_c/Settings.h>

namespace MotorControlCAN {
namespace LICHUAN_C {
/** Exact documentary native command selection, without generic deceleration promises. */
enum class StopKind : uint8_t {
    HALT,      ///< ZH86 p12/OP86 p10 000F to 010F normal-stop example; EN86 omits the row.
    QUICK_STOP ///< EN86/ZH86 p12 and OP86 p10 000F to 000B example, no certified safety claim.
};
/** Caller policy, not manufacturer response/stop timing. Defaults intentionally reject. */
struct ActionOptions {
    ReadOptions service;                ///< Nonzero queue/response budgets for each service.
    uint64_t observationIntervalUs = 0; ///< Nonzero delay between complete running-report cycles.
    uint64_t maximumAgeUs =
        0; ///< Nonzero inclusive earliest-bound age cap for prerequisite/result blocks.
};
/** Concrete action's active phase. */
enum class ActionPhase : uint8_t {
    COMMAND,   ///< One native download pending; never replay it.
    OBSERVING, ///< One finite five-field read cycle pending.
    WAITING    ///< Prior cycle reported running; next cycle waits until eligibleUs.
};
/** Owned yielded service, not admission or transmission permission after epochs change. */
struct PreparedAction {
    CANOPEN::SdoTransfer transfer; ///< Complete copied service when ready.
    bool ready = false;            ///< False for terminal or not-yet-eligible work.
    uint64_t eligibleUs = 0;       ///< Earliest caller processing time for this work.
};
/** Caller-owned concrete stop context. Serialize access; do not edit active fields.
 * Histories are inspectable after terminal and survive failure. Inputs are borrowed
 * during calls only. Every yielded/advanced service needs the actual live reservation.
 */
struct ActionContext {
    SettingsReservation reservation;          ///< Frozen route, model and continuous exclusion.
    StopKind kind = StopKind::HALT;           ///< Explicit native selection.
    ActionOptions options;                    ///< Immutable caller budgets.
    ActionState state = ActionState::EMPTY;   ///< Overall phase, distinct from drive state.
    ActionPhase phase = ActionPhase::COMMAND; ///< Current concrete work phase.
    ActionExecution execution = ActionExecution::NOT_TRANSMITTED; ///< Command evidence only.
    ActionCompletion completion = ActionCompletion::NOT_OBSERVED; ///< Narrow report evidence only.
    ActionOutcome outcome = ActionOutcome::NONE; ///< Immutable first terminal cause.
    Status status;          ///< Local/native operation status, separate from health.
    uint32_t operation = 0; ///< Nonzero live operation identity.
    uint32_t step = 1;      ///< Nonwrapping external service identity; command 1, observations 2+.
    uint32_t observations = 0; ///< Complete five-field cycles, saturating by identity exhaustion.
    uint64_t startedUs = 0;    ///< Caller preparation time, zero valid.
    uint64_t servicedUs = 0;   ///< Last local mutation time.
    uint64_t deadlineUs = 0; ///< Immutable exclusive deadline; equality expires before publication.
    uint64_t eligibleUs = 0; ///< Earliest next-cycle admission time.
    uint64_t stepStartedUs = 0;     ///< Earliest admission for the current external service.
    bool transmissionNoted = false; ///< Stop download crossed possible-TX boundary.
    bool effectsPossible = false;   ///< Command may have reached the drive.
    bool uncertain =
        false; ///< Unconfirmed command/read effects or missing supported stop observation.
    CANOPEN::SdoCompletion controlEvidence; ///< Prior positively acknowledged 000F prerequisite.
    CANOPEN::SdoCompletion
        commandEvidence;                 ///< Exact stop terminal; later reads never overwrite it.
    bool commandEvidencePresent = false; ///< Makes commandEvidence meaningful.
    CANOPEN::SdoCompletion
        observationEvidence; ///< Latest actual external read terminal, including failure.
    bool observationEvidencePresent = false; ///< Makes observationEvidence meaningful.
    ReadContext read; ///< Current finite raw SDO cycle, retaining acquired field provenance.
    StateObservation
        statusObservation; ///< Latest complete cycle STATUS; preserved on later failure.
    StateObservation
        monitorObservation;        ///< Latest complete cycle MONITORS; sequential, not atomic.
    bool observationKnown = false; ///< Both complete retained observations exist.
};
/** Prepare only the narrow documented stop transition; all failures leave output unchanged.
 * @param[out] output Owned context; must not overlap any input.
 * @param[in] reservation Actual continuous exclusive owner reservation, not a fresh replacement.
 * @param[in] state Fresh complete SDO STATUS/MONITORS after control; mode 1/low7, alarm/release clear.
 * @param[in] control Actual last acknowledged 6040:00 U16=000F under the same reservation/history.
 * @param[in] kind Native HALT or QUICK_STOP; no option-code interpretation.
 * @param[in] operation Nonzero identity unique while old owner work exists.
 * @param[in] nowUs Supplied nonwrapping monotonic microseconds, zero valid.
 * @param[in] deadlineUs Immutable exclusive future cutoff for command and observations.
 * @param[in] options Explicit nonzero caller service/observation/freshness policies.
 * @return Local validation status only; emits no I/O and establishes no firmware qualification.
 */
Status prepareStop(ActionContext& output, const SettingsReservation& reservation,
                   const ReadContext& state, const CANOPEN::SdoCompletion& control, StopKind kind,
                   uint32_t operation, uint64_t nowUs, uint64_t deadlineUs,
                   const ActionOptions& options) noexcept;
/** Yield copied work without mutation, I/O or automatic replay.
 * @param[in] context Prepared action. @param[in] live Actual continuous owner reservation.
 * @param[in] current Actual current full target/epochs. @param[in] nowUs Nondecreasing caller time.
 * @param[out] output Unchanged on rejection; no overlap with inputs. ready=false during wait/terminal.
 * @return Local status; active calls at/after cutoff reject until explicitly advanced to DEADLINE.
 */
Status nextAction(const ActionContext& context, const SettingsReservation& live,
                  const ReadTarget& current, uint64_t nowUs, PreparedAction& output) noexcept;
/** Record possible command transmission before owner handoff; never sends or modifies a binding.
 * @param[in,out] context Pending stop download, exactly one notice. Uploads use owner terminals.
 * @param[in] live Actual reservation. @param[in] current Actual target/epochs.
 * @param[in] nowUs Nondecreasing time before cutoff and not before eligibility.
 * @return Local status; rejected/overlapping inputs leave context unchanged.
 */
Status noteActionTransmission(ActionContext& context, const SettingsReservation& live,
                              const ReadTarget& current, uint64_t nowUs) noexcept;
/** Consume actual matching retained owner terminal; invalid evidence is atomic.
 * @param[in,out] context Active action. @param[in] live Actual continuous reservation.
 * @param[in] current Actual full target/epochs. @param[in] evidence Borrowed terminal, no overlap.
 * @param[in] nowUs Supplied processing time at least completion and previous mutation.
 * @return Local call status. Accepted service failures settle operation; deadline wins at equality.
 * Matching owner evidence supersedes a conservative possible-TX notice. It cannot prove wire
 * freshness: channel ambiguity/quarantine must already be handled by the sole owner.
 */
Status advanceAction(ActionContext& context, const SettingsReservation& live,
                     const ReadTarget& current, const CANOPEN::SdoCompletion& evidence,
                     uint64_t nowUs) noexcept;
/** Local termination after separately retiring any admitted work; never another stop/replay.
 * @param[in,out] context Active context, retained evidence preserved.
 * @param[in] live Actual reservation; INVALIDATED alone permits a changed/withdrawn reservation.
 * @param[in] current Actual full target/epochs, checked except for INVALIDATED.
 * @param[in] reason CANCELLED, DEADLINE, INVALIDATED or independently checked DRIVE_FAULT.
 * @param[in] nowUs Nondecreasing supplied time; inclusive deadline wins over local reason.
 * @return Local status. Earlier terminal is immutable; command ACK survives observer cancellation.
 */
Status advanceAction(ActionContext& context, const SettingsReservation& live,
                     const ReadTarget& current, ActionOutcome reason, uint64_t nowUs) noexcept;
} // namespace LICHUAN_C
} // namespace MotorControlCAN
