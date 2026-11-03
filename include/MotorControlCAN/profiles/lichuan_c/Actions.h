/** @file Actions.h
 * @brief Finite source-backed 86C stop, fault-clear and device-zero actions.
 * SPDX-License-Identifier: MIT
 * Stop transition contract: CL86-C/OL86-C only, one continuous actual owner reservation
 * acquired before the last positively acknowledged 6040:00 U16 word. Fresh SDO
 * STATUS (mode 1, low bits 7) and MONITORS (alarm/released clear, running either)
 * must follow that acknowledgement. The application proves no intervening
 * control/mode/target/RPDO/configuration write, uncertain work or lifecycle change.
 * A copied or reacquired token cannot establish this continuous history. The
 * library cannot see that history, so the application's session must supply it
 * (see examples/common/AxisSession.h): a successful stop consumes the prior
 * eligibility even if its copied token remains available.
 * Allowed predecessors (DEC-8): 000F documented, EN86/ZH86 p12 and OP86 p10 for 000B,
 * ZH86 p12 and OP86 p10 only for 010F. 001F/005F/003F/007F, and 010F to 000B only, are
 * project decisions with no documented arrow, so predecessorDocumented is false.
 * HALT emits only 010F; QUICK_STOP emits only 000B. Neither enables first. A prior
 * 000B, a prior 010F for HALT, any other word and a released state are rejected.
 * After ACK, fresh five-field MONITORS+STATUS SDO cycles require mode 1 and no
 * alarm/release; monitor running clear establishes OBSERVED controller-not-running
 * during that sequential read window. Other mode/alarm/release is outside the
 * observation scope, not proof that physical stopping failed. Statusword dialect
 * bits never decide completion. No 605A/6085, guessed 605D, queue disposition,
 * torque/reference retention, shaft standstill or firmware qualification is claimed.
 * One owner must retire obsolete queued/controller work before stop admission.
 * BestEffortStop is the separate DEC-2 path for an unresolved route or missing history.
 * Independently received EMCY/fault/lifecycle withdrawal must be delivered as a
 * local terminal before accepting later observation completions; an older monitor
 * read never overrides a newer routed fault.
 * Preparation and advancement validate first, then write the caller's context in place,
 * so no call copies a context onto the stack and a refused call changes nothing. Keep
 * contexts in static or owner storage, never in task locals.
 */
#pragma once
#include <MotorControlCAN/ActionOperation.h>
#include <MotorControlCAN/Axis.h>
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
    uint64_t observationIntervalUs = 0; ///< Nonzero delay between complete observation cycles.
    uint64_t maximumAgeUs =
        0; ///< Nonzero inclusive earliest-bound age cap for prerequisite/result blocks.
};
/** Concrete action's active phase. */
enum class ActionPhase : uint8_t {
    COMMAND,   ///< Current ordered native download pending; never replay it.
    OBSERVING, ///< One finite action-specific state read cycle pending.
    WAITING    ///< Completion not yet reported; next cycle waits until eligibleUs.
};
/** Which sources show a stop's whole-word arrow. OP86's body carries a retained 57C/86C
 * heading conflict, and no source is firmware evidence for a specimen. */
enum class PredecessorBasis : uint8_t {
    NONE,             ///< Not a stop context.
    EN86_ZH86_OP86,   ///< 000F to 000B: EN86 and ZH86 p12, OP86 p10.
    ZH86_OP86,        ///< 000F to 010F: ZH86 p12 and OP86 p10; EN86 omits the row.
    PROJECT_DECISION  ///< DEC-8: derived word with no documented arrow.
};
/** Owned yielded service, not admission or transmission permission after epochs change. */
struct PreparedAction {
    CANOPEN::SdoTransfer transfer; ///< Complete copied service when ready.
    bool ready = false;            ///< False for terminal or not-yet-eligible work.
    uint64_t eligibleUs = 0;       ///< Earliest caller processing time for this work.
};
/** What a clear attempt set out to clear: raw prerequisite values, as read at admission.
 * Later cycles replace the context's observations, so this keeps the incident. Per-field
 * provenance is not kept: all four blocks share the reservation's route and node epoch,
 * and earliestUs/latestUs bound them together. The application keeps its full read if it
 * needs each field's times. No value here is an interpretation or a reset claim.
 */
struct AlarmIncident {
    bool present = false;          ///< True only in a CLEAR_ALARM context.
    uint32_t readOperation = 0;    ///< Operation of the read that established the prerequisite.
    uint64_t earliestUs = 0;       ///< Earliest observation bound across the four blocks.
    uint64_t latestUs = 0;         ///< Latest observation bound across the four blocks.
    uint8_t errorRegister = 0;     ///< ERRORS: 1001 raw.
    uint16_t errorCode = 0;        ///< ERRORS: 603F raw, FF05 at admission.
    uint8_t errorCount = 0;        ///< ERROR_HISTORY: active entries, 0..4.
    uint32_t errorHistory[4] = {}; ///< ERROR_HISTORY: native error values.
    uint16_t monitor = 0;          ///< MONITORS: 2100 raw.
    uint16_t logicalInputs = 0;    ///< MONITORS: 2101 raw.
    uint16_t logicalOutputs = 0;   ///< MONITORS: 2102 raw.
    uint16_t statusword = 0;       ///< STATUS: 6041 raw.
    uint8_t modeRaw = 0;           ///< STATUS: 6061 complete byte.
};
/** Caller-owned concrete action context. Serialize access; do not edit active fields.
 * Histories are inspectable after terminal and survive failure. Inputs are borrowed
 * during calls only. Every yielded/advanced service needs the actual live reservation.
 */
struct ActionContext {
    SettingsReservation reservation; ///< Frozen route, model and continuous exclusion.
    ActionKind actionKind =
        ActionKind::STOP;                   ///< Explicit operation; kind remains the stop selector.
    StopKind kind = StopKind::HALT;         ///< Explicit native selection.
    ActionOptions options;                  ///< Immutable caller budgets.
    ActionState state = ActionState::EMPTY; ///< Overall phase, distinct from drive state.
    ActionPhase phase = ActionPhase::COMMAND; ///< Current concrete work phase.
    uint16_t predecessorWord = 0; ///< STOP only: acknowledged 6040 word this stop follows.
    bool predecessorDocumented = false; ///< STOP only: false for DEC-8 project-decision arrows.
    PredecessorBasis predecessorBasis = PredecessorBasis::NONE; ///< STOP only: which sources show it.
    bool finalErrorRead = false; ///< CLEAR_ALARM only: alarm reported clear; read ERRORS once.
    ActionExecution execution =
        ActionExecution::NOT_TRANSMITTED; ///< Effecting command evidence only.
    ActionCompletion completion = ActionCompletion::NOT_OBSERVED; ///< Narrow report evidence only.
    ActionOutcome outcome = ActionOutcome::NONE; ///< Immutable first terminal cause.
    Status status;          ///< Local/native operation status, separate from health.
    uint32_t operation = 0; ///< Nonzero live operation identity.
    uint32_t step = 1;      ///< Nonwrapping identity across every ordered download and observation.
    uint32_t observations = 0; ///< Complete cycles, bounded by identity exhaustion.
    uint64_t startedUs = 0;    ///< Caller preparation time, zero valid.
    uint64_t servicedUs = 0;   ///< Last local mutation time.
    uint64_t deadlineUs = 0; ///< Immutable exclusive deadline; equality expires before publication.
    uint64_t eligibleUs = 0; ///< Earliest next-cycle admission time.
    uint64_t stepStartedUs = 0;     ///< Earliest admission for the current external service.
    bool transmissionNoted = false; ///< Current download crossed possible-TX boundary.
    bool effectsPossible = false;   ///< At least one prefix command may have reached the drive.
    bool uncertain =
        false; ///< Unconfirmed command/read effects or missing supported action observation.
    CANOPEN::SdoCompletion controlEvidence; ///< STOP only: prior positively acknowledged word.
    /** Each ordered download terminal, including failure. The effecting command is slot 0
     * for a stop or alarm clear (value 1) and slot 1 for a counter clear (value 1). */
    CANOPEN::SdoCompletion commandHistory[3];
    uint8_t commandEvidenceMask = 0; ///< Bits identify retained commandHistory slots.
    uint8_t commandCursor = 0;       ///< Current download slot: stop 0, alarm 0..1, counter 0..2.
    AlarmIncident incident; ///< CLEAR_ALARM only: the prerequisite, kept after later cycles.
    Generation originalReferenceGeneration = 0; ///< Counter action's frozen confidence epoch.
    bool coordinatesInvalidated =
        false; ///< Counter trigger crossed possible-TX boundary; never undone.
    CANOPEN::SdoCompletion
        observationEvidence; ///< Latest actual external read terminal, including failure.
    bool observationEvidencePresent = false; ///< Makes observationEvidence meaningful.
    ReadContext read; ///< Current finite raw SDO cycle, retaining acquired field provenance.
    StateObservation
        statusObservation; ///< Latest complete cycle STATUS; preserved on later failure.
    StateObservation
        monitorObservation; ///< Latest complete cycle MONITORS; sequential, not atomic.
    /** Selected by its block. CLEAR_ALARM: raw 1001/603F, the prerequisite until the final read.
     * SET_DEVICE_POSITION: signed 6064/606C of the latest cycle, no scale/home proof. Stop: unused.
     * The default block is ERRORS with fieldCount 0, so check fieldCount before reading it. */
    StateObservation extraObservation;
    bool observationKnown = false;        ///< All required blocks of a complete cycle are retained.
};
/** Best-effort stop context (DEC-2), keyed to a target because its route has no usable
 * reservation. It yields only 6040:00=000B and needs no state evidence. It never
 * reaches SUCCEEDED: state FAILED with STOP_SENT_UNCONFIRMED means a frame left the
 * controller, and nothing is known about the drive. Serialize access.
 */
struct BestEffortStop {
    ReadTarget target;                      ///< Frozen route and node epoch at preparation.
    ReadOptions service;                    ///< Nonzero queue/response budgets.
    ActionState state = ActionState::EMPTY; ///< ACTIVE until terminal; never SUCCEEDED.
    ActionExecution execution =
        ActionExecution::NOT_TRANSMITTED; ///< UNKNOWN once a frame may have left; never ACKNOWLEDGED.
    ActionOutcome outcome = ActionOutcome::NONE; ///< Immutable first terminal cause.
    Status status;                 ///< Local status, separate from health.
    uint32_t operation = 0;        ///< Nonzero live operation identity.
    uint64_t startedUs = 0;        ///< Caller preparation time, zero valid.
    uint64_t servicedUs = 0;       ///< Last local mutation time.
    uint64_t deadlineUs = 0;       ///< Immutable exclusive cutoff, fixed at preparation.
    bool admitted = false;         ///< The owner holds this work; only its terminal can settle it.
    bool effectsPossible = false;  ///< The frame may have reached the drive.
    CANOPEN::SdoCompletion commandEvidence; ///< Owner terminal, when commandEvidencePresent.
    bool commandEvidencePresent = false;    ///< Makes commandEvidence meaningful.
};
/** The DEC-8 table for mode 1: may this stop follow this latest acknowledged 6040 word?
 * @param[in] word Latest acknowledged controlword.
 * @param[in] kind Native stop selection.
 * @return True for an allowed predecessor, documented or a project decision.
 */
bool stopPredecessorAllowed(uint16_t word, StopKind kind) noexcept;
/** Check budgets for prepareStop or prepareEscalation, before any work is retired.
 * @param[in] options Caller policies.
 * @return True with nonzero budgets, a maximumAgeUs covering five fields, and DECLARED_U16
 * identity widths: an action's reads are state reads, which take no other setting.
 */
bool stopOptionsValid(const ActionOptions& options) noexcept;
/** Return a context to EMPTY in place. `context = ActionContext()` builds a stack temporary
 * of the whole context on the generated code we measured; this does not.
 * @param[out] context Context to reset; its retained history is gone.
 */
void resetAction(ActionContext& context) noexcept;
/** Report what prepareStop would refuse, changing nothing. For a caller that must know before
 * it moves other state, such as an owner operation, and only then writes the context.
 * @param[in] output The context prepareStop would write; read only for its overlap checks.
 * @param[in] reservation As prepareStop. @param[in] state As prepareStop.
 * @param[in] control As prepareStop. @param[in] kind As prepareStop.
 * @param[in] operation As prepareStop. @param[in] nowUs As prepareStop.
 * @param[in] deadlineUs As prepareStop. @param[in] options As prepareStop.
 * @return The status prepareStop would give. OK means prepareStop with these same unchanged
 * values cannot refuse.
 */
Status validateStop(const ActionContext& output, const SettingsReservation& reservation,
                 const ReadContext& state, const CANOPEN::SdoCompletion& control, StopKind kind,
                 uint32_t operation, uint64_t nowUs, uint64_t deadlineUs,
                 const ActionOptions& options) noexcept;
/** Report what prepareEscalation would refuse, changing nothing.
 * @param[in] output The context prepareEscalation would write; read only for its overlap checks.
 * @param[in] running As prepareEscalation. @param[in] operation As prepareEscalation.
 * @param[in] nowUs As prepareEscalation. @param[in] deadlineUs As prepareEscalation.
 * @param[in] options As prepareEscalation.
 * @return The status prepareEscalation would give. OK means prepareEscalation with these same
 * unchanged values cannot refuse.
 */
Status validateEscalation(const ActionContext& output, const ActionContext& running, uint32_t operation,
                       uint64_t nowUs, uint64_t deadlineUs, const ActionOptions& options) noexcept;
/** Prepare only the DEC-8 stop transitions; all failures leave output unchanged.
 * @param[out] output Owned context; must not overlap any input.
 * @param[in] reservation Actual continuous exclusive owner reservation, not a fresh replacement.
 * @param[in] state Fresh complete SDO STATUS/MONITORS after control; mode 1/low7, alarm/release clear.
 * @param[in] control Actual last acknowledged 6040:00 U16 under the same reservation/history, of
 * any age. Only the read blocks expire. The word must be an allowed predecessor of kind.
 * @param[in] kind Native HALT or QUICK_STOP; no option-code interpretation.
 * @param[in] operation Nonzero identity unique while old owner work exists.
 * @param[in] nowUs Supplied nonwrapping monotonic microseconds, zero valid.
 * @param[in] deadlineUs Immutable exclusive future cutoff for command and observations.
 * @param[in] options Explicit nonzero caller service/observation/freshness policies. maximumAgeUs
 * must cover five fields at the queue plus response budgets, or no cycle could ever qualify.
 * service.identityWidths must stay DECLARED_U16.
 * @return Local validation status only; emits no I/O and establishes no firmware qualification.
 */
Status prepareStop(ActionContext& output, const SettingsReservation& reservation,
                   const ReadContext& state, const CANOPEN::SdoCompletion& control, StopKind kind,
                   uint32_t operation, uint64_t nowUs, uint64_t deadlineUs,
                   const ActionOptions& options) noexcept;
/** Prepare QUICK_STOP from a running HALT, using its own acknowledgement and latest cycle.
 * No separate read can run during a stop, so this is that path's fresh state. The
 * word is 010F, a project decision, so predecessorDocumented is false.
 * @param[out] output Owned context; must not overlap any input.
 * @param[in] running The running or just finished HALT.
 * @param[in] operation Nonzero identity. @param[in] nowUs Monotonic microseconds.
 * @param[in] deadlineUs Exclusive future cutoff. @param[in] options As prepareStop.
 * @return Local validation. An unacknowledged HALT, or no fresh complete cycle after
 * its acknowledgement, rejects, and the application takes the best-effort path.
 */
Status prepareEscalation(ActionContext& output, const ActionContext& running, uint32_t operation,
                         uint64_t nowUs, uint64_t deadlineUs, const ActionOptions& options) noexcept;
/** Prepare a CL86-C/OL86-C clear attempt; emits no enable or motion command.
 * Requires complete fresh SDO ERRORS/ERROR_HISTORY/MONITORS/STATUS acquired under
 * reservation: mode 1, monitor not-running, alarm set, raw 603F=FF05. Writes
 * 2301 U16=1 then 0 only after each ACK. Returning to ineffective 0 is library
 * policy, not a claimed vendor edge/rearm requirement. Completion requires fresh
 * mode 1/not-running/alarm-clear reports, then one ERRORS read: these reads may
 * consume data, so polling never repeats them. 603F may retain its old raw value.
 * maximumAgeUs must cover five fields at the queue plus response budgets, and
 * service.identityWidths must stay DECLARED_U16.
 * @param[out] output Owned context, unchanged on failure and disjoint from inputs.
 * @param[in] reservation Continuous actual owner exclusion, CL86-C/OL86-C.
 * @param[in] state Complete fresh prerequisite; its raw values stay in incident.
 * @param[in] operation Nonzero live identity. @param[in] nowUs Monotonic microseconds.
 * @param[in] deadlineUs Exclusive future cutoff. @param[in] options Nonzero caller policies.
 * @return Local validation only. Caller/owner separately retains EMCY/error history;
 * this action never erases failed operations, retries, enables or resumes work.
 */
Status prepareClearAlarm(ActionContext& output, const SettingsReservation& reservation,
                         const ReadContext& state, uint32_t operation, uint64_t nowUs,
                         uint64_t deadlineUs, const ActionOptions& options) noexcept;
/** Prepare an explicit zero-only device counter clear; nonzero is UNSUPPORTED.
 * Writes 2302 U16 values 0,1,0, each ACK before the next. Fresh initial mode 1,
 * monitor not-running/alarm-clear is required. At possible trigger transmission
 * use the axis-aware notice below. It withdraws coordinates before any possible
 * effect. Post-ACK reads use that new configuration epoch and observe signed
 * raw 6064 zero only; no host origin, physical home, arbitrary setter, feedback
 * mapping, standstill or zero-velocity claim is established.
 * @param[out] output Owned atomic context, disjoint from all inputs.
 * @param[in] reservation Actual continuous CL86-C/OL86-C exclusion.
 * @param[in] state Fresh successful SDO MONITORS/STATUS under reservation.
 * @param[in] nativePosition Native device request; only exactly zero supported.
 * @param[in] axis Valid matching axis with room in both configuration/confidence epochs.
 * @param[in] operation Nonzero live identity. @param[in] nowUs Monotonic microseconds.
 * @param[in] deadlineUs Exclusive future cutoff. @param[in] options Explicit caller policies;
 * maximumAgeUs must cover seven fields at the queue plus response budgets, and
 * service.identityWidths must stay DECLARED_U16.
 * @return Local validation. After trigger, retire old owner results/work and advance
 * owner configuration before admitting the new-epoch observation uploads.
 */
Status prepareSetDevicePosition(ActionContext& output, const SettingsReservation& reservation,
                                const ReadContext& state, int64_t nativePosition,
                                const AxisConfig& axis, uint32_t operation, uint64_t nowUs,
                                uint64_t deadlineUs, const ActionOptions& options) noexcept;
/** Yield copied work without mutation, I/O or automatic replay.
 * @param[in] context Prepared action. @param[in] live Actual continuous owner reservation.
 * @param[in] current Actual current full target/epochs. @param[in] nowUs Nondecreasing caller time.
 * @param[out] output Unchanged on rejection; no overlap with inputs. ready=false during wait/terminal.
 * @return Local status; active calls at/after cutoff reject until explicitly advanced to DEADLINE.
 */
Status nextAction(const ActionContext& context, const SettingsReservation& live,
                  const ReadTarget& current, uint64_t nowUs, PreparedAction& output) noexcept;
/** Record possible stop/alarm command transmission; never sends or modifies a binding.
 * Counter actions reject this overload to prevent bypassing coordinate invalidation.
 * @param[in,out] context Pending download, exactly one notice per step. Uploads use owner terminals.
 * @param[in] live Actual reservation. @param[in] current Actual target/epochs.
 * @param[in] nowUs Nondecreasing time before cutoff and not before eligibility.
 * @return Local status; rejected/overlapping inputs leave context unchanged.
 */
Status noteActionTransmission(ActionContext& context, const SettingsReservation& live,
                              const ReadTarget& current, uint64_t nowUs) noexcept;
/** Axis-aware counter notice; required for every counter-action download.
 * First possible U16=1 trigger atomically invalidates coordinates/origins/limits,
 * advances both axis epochs and current's configuration, and records uncertainty.
 * Initial/final zero only note possible TX. No rollback follows owner refusal or
 * cancellation after this conservative boundary. Does no I/O or persistence.
 * @param[in,out] context Active counter context. @param[in] live Frozen live reservation.
 * @param[in,out] current Actual current target, old then exactly one advanced configuration.
 * @param[in,out] axis Actual frozen matching axis interpretation.
 * @param[in,out] reference Position witness to withdraw, no replacement synthesized.
 * @param[in] nowUs Monotonic time before cutoff. @return Atomic status on rejection;
 * all mutable outputs and borrowed reservation must be mutually disjoint.
 */
Status noteActionTransmission(ActionContext& context, const SettingsReservation& live,
                              ReadTarget& current, AxisConfig& axis, AxisReference& reference,
                              uint64_t nowUs) noexcept;
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
/** Prepare a best-effort stop; no state evidence, no history and no traffic.
 * @param[out] output Owned context; unchanged on failure; must not overlap any input.
 * @param[in] target Actual current route and node epoch.
 * @param[in] operation Nonzero identity. @param[in] nowUs Monotonic microseconds.
 * @param[in] deadlineUs Immutable exclusive future cutoff; a recovery never extends it.
 * @param[in] service Nonzero queue and response budgets.
 * @return Local validation only.
 */
Status prepareBestEffortStop(BestEffortStop& output, const ReadTarget& target, uint32_t operation,
                             uint64_t nowUs, uint64_t deadlineUs,
                             const ReadOptions& service) noexcept;
/** Yield the exact stop for CanBusOwner::admitBestEffortStop, without mutation or I/O.
 * @param[in] context Active stop not yet admitted. @param[in] current Actual current target.
 * @param[in] nowUs Nondecreasing time. @param[out] output Unchanged on rejection.
 * @return Local status; a changed target or reached deadline rejects.
 */
Status nextBestEffortStop(const BestEffortStop& context, const ReadTarget& current, uint64_t nowUs,
                          PreparedAction& output) noexcept;
/** Record that the owner accepted the stop; from here only its terminal can settle it.
 * @param[in,out] context Active stop not yet admitted. @param[in] nowUs Nondecreasing time.
 * @return Local status; rejection leaves context unchanged.
 */
Status noteBestEffortAdmission(BestEffortStop& context, uint64_t nowUs) noexcept;
/** Consume the owner terminal of an admitted stop. The target may have changed since.
 * SUCCESS, a server abort or UNSUPPORTED cannot be this path's terminal and is rejected as
 * correlation, so a matching-looking reply cannot make this stop succeed.
 * @param[in,out] context Admitted stop. @param[in] evidence Borrowed terminal, no overlap.
 * @param[in] nowUs Processing time at least the completion and previous mutation.
 * @return Local call status. The accepted terminal sets outcome and execution.
 */
Status advanceBestEffortStop(BestEffortStop& context, const CANOPEN::SdoCompletion& evidence,
                             uint64_t nowUs) noexcept;
/** End a stop that was never admitted, so nothing can have left.
 * @param[in,out] context Active stop. @param[in] reason CANCELLED, DEADLINE, INVALIDATED or TRANSPORT_ERROR.
 * @param[in] nowUs Nondecreasing time; an inclusive deadline wins over the local reason.
 * @return Local status; an admitted stop must end through its terminal.
 */
Status advanceBestEffortStop(BestEffortStop& context, ActionOutcome reason,
                             uint64_t nowUs) noexcept;
} // namespace LICHUAN_C
} // namespace MotorControlCAN
