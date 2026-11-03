/** @file Actions.h
 * @brief Finite source-backed 86C stop, fault-clear and device-zero actions.
 * SPDX-License-Identifier: MIT
 * Stop transition contract: CL86-C/OL86-C only, one continuous actual owner reservation
 * acquired before the last positively acknowledged 6040:00 U16 word. Fresh SDO
 * STATUS (mode 1, low bits 7) and MONITORS (alarm/released clear, running either)
 * must follow that acknowledgement. The application proves no intervening
 * control/mode/target/RPDO/configuration write, uncertain work or lifecycle change.
 * A copied or reacquired reservation cannot establish this continuous history. The
 * library cannot see that history, so the application's session must supply it
 * (see examples/common/AxisSession.h): a successful stop consumes the prior
 * eligibility even if its copied reservation remains available.
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
 * read never overrides a newer routed fault. While admitted work is still in flight, deliver it
 * with noteActionWithdrawal instead: that work's terminal is then consumed as evidence, it cannot
 * complete or advance the action, and the outcome is the withdrawal's reason.
 * Enable transition contract: GATED. NO SPECIMEN HAS RUN prepareEnable, and every claim about it
 * below is fake-owner evidence. EN86/ZH86 p12 and OP86 p9 document 6040 words 6, 7, 15 about 10 ms
 * apart in position mode. No source says what the drive does with an earlier target when it
 * enables, so EnableEvidence must carry the caller's explicit statements and a closed gate refuses.
 * Mode 1 is mandatory: ZH86 p13 and OP86 p11 start the motor at the end of 6, 7, 15 in mode 3, and
 * a mode 3 enable has no approved path. Release has no API: no source pairs a command with an
 * effect. Both specimens read 6041 low bits 111 and 2100 released clear at power-up, the pattern
 * the manual pairs with an enabled drive, so word 6 may be a 15-to-6 step that drops holding torque.
 * Read docs/drive_actions.md for the full table.
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
/** The manual's "about 10 ms" between enable words (EN86/ZH86 p12, OP86 p9), in microseconds. It is
 * the shortest spacing admitted, as project policy: no source gives a tolerance. It is an interval
 * to honor, not a settling time, so reaching it establishes nothing about the drive. */
constexpr uint64_t ENABLE_WORD_INTERVAL_US = 10000;
/** Services one enable schedules at least: the 607A and 60FF uploads, three words and one
 * seven-field cycle after each word. prepareEnable needs a deadline beyond three intervals plus this
 * many queue-and-response budgets. Polls beyond one cycle per word are not counted. */
constexpr uint64_t ENABLE_SERVICE_STEPS = 26;
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
    /** ENABLE only: least time from one word's acknowledgement to the next word's eligibility, at
     * least ENABLE_WORD_INTERVAL_US. Other actions ignore it. A later word is also held for that
     * step's observation, so the actual spacing can be longer; commandHistory keeps the times. */
    uint64_t commandIntervalUs = 0;
};
/** Concrete action's active phase. */
enum class ActionPhase : uint8_t {
    COMMAND,   ///< Current ordered native download pending; never replay it.
    OBSERVING, ///< One finite action-specific state read cycle pending.
    WAITING,   ///< Completion not yet reported; next cycle waits until eligibleUs.
    TARGETS    ///< ENABLE only: the single 607A and 60FF uploads, before any 6040 word.
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
/** What ties the admission's 603F code to the alarm that 2100 reported in the same read. */
enum class AlarmCodeLink : uint8_t {
    NONE,      ///< Not a CLEAR_ALARM context.
    /** The only value admission produces. 603F read FF05 and 2100 reported an alarm, but no source
     * ties the code to that alarm, and 603F may keep an earlier code (Q13, D-26). So the alarm being
     * cleared may be another event than the FF05 one, and may not be a resettable class at all. */
    UNRESOLVED
};
/** What a clear attempt set out to clear: raw prerequisite values, as read at admission.
 * Later cycles replace the context's observations, so this keeps the incident. Per-field
 * provenance is not kept: all four blocks share the reservation's route and node epoch,
 * and earliestUs/latestUs bound them together. The application keeps its full read if it
 * needs each field's times. No value here is an interpretation or a reset claim.
 */
struct AlarmIncident {
    bool present = false;          ///< True only in a CLEAR_ALARM context.
    /** Always UNRESOLVED in a CLEAR_ALARM context: the identity of the reported alarm is unknown. */
    AlarmCodeLink codeLink = AlarmCodeLink::NONE;
    uint32_t readOperation = 0;    ///< Operation of the read that established the prerequisite.
    uint64_t earliestUs = 0;       ///< Earliest observation bound across the four blocks.
    uint64_t latestUs = 0;         ///< Latest observation bound across the four blocks.
    uint8_t errorRegister = 0;     ///< ERRORS: 1001 raw.
    uint16_t errorCode = 0;        ///< ERRORS: 603F raw, FF05 at admission; possibly an earlier event's.
    uint8_t errorCount = 0;        ///< ERROR_HISTORY: active entries, 0..4.
    uint32_t errorHistory[4] = {}; ///< ERROR_HISTORY: native error values, layout undocumented.
    uint16_t monitor = 0;          ///< MONITORS: 2100 raw.
    uint16_t logicalInputs = 0;    ///< MONITORS: 2101 raw.
    uint16_t logicalOutputs = 0;   ///< MONITORS: 2102 raw.
    uint16_t statusword = 0;       ///< STATUS: 6041 raw.
    uint8_t modeRaw = 0;           ///< STATUS: 6061 complete byte.
};
/** SET_DEVICE_POSITION only: the raw feedback read at admission, kept after later cycles replace
 * extraObservation. When startPosition was already zero, a later raw zero shows no change at all.
 * Neither value says which internal counter 2302 clears (Q14).
 */
struct CounterBaseline {
    bool present = false;       ///< True only in a SET_DEVICE_POSITION context.
    int32_t startPosition = 0;  ///< FEEDBACK: 6064 raw at admission, no scale or home meaning.
    int32_t startVelocity = 0;  ///< FEEDBACK: 606C raw at admission; admission requires zero.
    uint64_t earliestUs = 0;    ///< Earliest bound of that FEEDBACK block.
    uint64_t latestUs = 0;      ///< Latest bound of that FEEDBACK block.
};
/** Whether enable may run at all. This is the capability gate, not a readiness claim. */
enum class EnableGate : uint8_t {
    CLOSED,             ///< Default: prepareEnable returns UNSUPPORTED with no output change.
    ATTENDED_EXPERIMENT ///< One attended trial with an independent drive-power cutoff; no specimen has run it.
};
/** What the application's own retained record says happened before this enable. */
enum class EnableStart : uint8_t {
    UNKNOWN, ///< Default: refused. Fresh flags cannot tell idle from an interrupted curve.
    /** No 6040, target, ramp or other mode write reached this drive since its last power-up, as far
     * as this application's retained record can show, apart from one acknowledged 6060=1. A
     * statement the library cannot check, and not proof that the drive holds no pending curve. */
    NO_MOTION_WRITE_RECORDED
};
/** The attendant's own observation of the shaft after power-up, before any word. The library cannot
 * check it. A start report matching the manual's enabled pattern needs one of the two values. */
enum class ShaftObservation : uint8_t {
    NONE,   ///< Default: not observed. Refused when the start report matches the enabled pattern.
    FREE,   ///< Turned freely by hand: no holding torque observed.
    HOLDING ///< Resisted by hand: holding torque observed, which word 6 may then remove.
};
/** The caller's explicit evidence for one enable. Defaults are refused.
 * The reviewed values are read and compared, so they cannot go stale silently, but a match means only
 * that someone looked at these exact bits. No value is thereby called safe, known or intended. Both
 * targets need review whatever they read, zero included: an absolute target of zero is not a
 * no-motion claim, and 60FF=0 has no documented meaning.
 */
struct EnableEvidence {
    EnableGate gate = EnableGate::CLOSED;     ///< Must be explicitly opened for an attended trial.
    /** Nonzero identity of the attended trial record. AxisSession consumes each once, in rising order,
     * for the life of the session object. */
    uint32_t trial = 0;
    /** The exact reservation, and so the exact binding, this trial was reviewed for. A copy made under
     * another reservation, as after an MCU restart or by another session, does not match it. */
    AxisReservation reservation;
    /** The exact operation identity this enable runs under. Owner operations only rise on a
     * reservation, so the evidence opens at most one enable there, whichever session holds it. */
    uint32_t operation = 0;
    uint64_t expiresUs = 0; ///< Exclusive cutoff of this evidence in the caller's clock domain.
    EnableStart start = EnableStart::UNKNOWN; ///< Application's record of the drive's prior history.
    ShaftObservation powerUpShaft = ShaftObservation::NONE; ///< Attendant's observation at power-up.
    bool reviewed = false;                    ///< The three reviewed fields below are meaningful.
    uint16_t reviewedStatusword = 0;  ///< Exact 6041 at admission. Disputed bits are never interpreted.
    int32_t reviewedPositionTarget = 0; ///< Exact 607A read before any word. History, not intent.
    int32_t reviewedVelocityTarget = 0; ///< Exact 60FF read before any word. History, not intent.
};
/** One enable word's accepted observation, retained after later cycles replace the blocks. */
struct EnableStepRecord {
    bool present = false;     ///< This step's expected report was accepted.
    uint16_t statusword = 0;  ///< 6041 raw of that cycle.
    uint16_t monitor = 0;     ///< 2100 raw of that cycle.
    int32_t position = 0;     ///< 6064 raw of that cycle, no scale.
    int32_t velocity = 0;     ///< 606C raw of that cycle, no scale.
    uint64_t earliestUs = 0;  ///< Earliest bound of that cycle, sequential and not atomic.
    uint64_t latestUs = 0;    ///< Latest bound of that cycle.
};
/** ENABLE history kept for inspection, including after failure. None of it is an interpretation. */
struct EnableRecord {
    EnableEvidence evidence;      ///< Copied caller evidence.
    uint16_t startStatusword = 0; ///< Admission 6041; bits above 0..2 must not change during the sequence.
    uint16_t startMonitor = 0;    ///< Admission 2100; bits other than released must not change.
    int32_t startPosition = 0;    ///< Admission 6064; any later raw difference ends the sequence.
    uint8_t targetCursor = 0;     ///< Current upload: 0 is 607A, 1 is 60FF.
    bool positionTargetKnown = false; ///< positionTarget holds an acquired value.
    bool velocityTargetKnown = false; ///< velocityTarget holds an acquired value.
    int32_t positionTarget = 0;   ///< 607A as read before the first word; history, not intent.
    int32_t velocityTarget = 0;   ///< 60FF as read before the first word; history, not intent.
    EnableStepRecord steps[3];    ///< Accepted observation after words 6, 7 and 15.
};
/** Caller-owned concrete action context. Serialize access; do not edit active fields.
 * Histories are inspectable after terminal and survive failure. Inputs are borrowed
 * during calls only. Every yielded/advanced service needs the actual live reservation.
 */
struct ActionContext {
    AxisReservation reservation; ///< Frozen route, model and continuous exclusion.
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
    /** A local reason recorded by noteActionWithdrawal while work was in flight. NONE otherwise.
     * Once set, nothing new is yielded, and the next consumed terminal ends the action with it. */
    ActionOutcome withdrawal = ActionOutcome::NONE;
    bool transmissionNoted = false; ///< Current download crossed possible-TX boundary.
    bool effectsPossible = false;   ///< At least one prefix command may have reached the drive.
    bool uncertain =
        false; ///< Unconfirmed command/read effects or missing supported action observation.
    CANOPEN::SdoCompletion controlEvidence; ///< STOP only: prior positively acknowledged word.
    /** Each ordered download terminal, including failure. The effecting command is slot 0
     * for a stop or alarm clear (value 1) and slot 1 for a counter clear (value 1). In an enable
     * every slot is effecting: words 6, 7 and 15, and a prefix stays retained after failure. */
    CANOPEN::SdoCompletion commandHistory[3];
    uint8_t commandEvidenceMask = 0; ///< Bits identify retained commandHistory slots.
    uint8_t commandCursor = 0;       ///< Current download slot: stop 0, alarm 0..1, counter 0..2, enable 0..2.
    AlarmIncident incident; ///< CLEAR_ALARM only: the prerequisite, kept after later cycles.
    CounterBaseline baseline; ///< SET_DEVICE_POSITION only: admission feedback, kept after later cycles.
    EnableRecord enable;    ///< ENABLE only: evidence, targets and per-step observations.
    Generation originalReferenceGeneration = 0; ///< Counter action's frozen confidence epoch.
    bool coordinatesInvalidated =
        false; ///< Counter trigger crossed possible-TX boundary; never undone.
    /** CLEAR_ALARM and SET_DEVICE_POSITION only, set when the action fails: its value-1 write may
     * have reached the drive and the 0 after it has no positive acknowledgement. The object may still
     * hold 1. What a held 1 does, and whether a later 1 is a new edge, is undocumented (Q13, Q14,
     * D-27). Never cleared, and nothing resends either value: a later attempt is new intent. */
    bool valueOneMayRemain = false;
    CANOPEN::SdoCompletion
        observationEvidence; ///< Latest actual external read terminal, including failure.
    bool observationEvidencePresent = false; ///< Makes observationEvidence meaningful.
    ReadContext read; ///< Current finite raw SDO cycle, retaining acquired field provenance.
    StateObservation
        statusObservation; ///< Latest complete cycle STATUS; preserved on later failure.
    StateObservation
        monitorObservation; ///< Latest complete cycle MONITORS; sequential, not atomic.
    /** Selected by its block. CLEAR_ALARM: raw 1001/603F, the prerequisite until the final read.
     * SET_DEVICE_POSITION and ENABLE: signed 6064/606C, the admission's until the first cycle
     * replaces it, no scale/home proof.
     * Stop: unused.
     * The default block is ERRORS with fieldCount 0, so check fieldCount before reading it. */
    StateObservation extraObservation;
    bool observationKnown = false;        ///< All required blocks of a complete cycle are retained.
};
/** Best-effort stop context (DEC-2), keyed to a target because its route has no usable
 * reservation. It yields only 6040:00=000B and needs no state evidence. It never
 * reaches SUCCEEDED: state FAILED with STOP_SENT_UNCONFIRMED means a frame left the
 * controller, and nothing is known about the drive. An independent physical stop stays
 * mandatory. Serialize access.
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
Status validateStop(const ActionContext& output, const AxisReservation& reservation,
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
Status prepareStop(ActionContext& output, const AxisReservation& reservation,
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
/** Prepare one CL86-C/OL86-C alarm-clear attempt; emits no enable, controlword or motion command.
 * Requires complete fresh SDO ERRORS/ERROR_HISTORY/MONITORS/STATUS acquired under
 * reservation: mode 1, 2100 running clear and alarm set, raw 603F=FF05. That code narrows the
 * attempt; it does not identify the reported alarm, so incident.codeLink is UNRESOLVED (D-26).
 * Writes 2301 U16=1, then 0 only after the 1's acknowledgement. The 0 returns to the documented
 * ineffective value as library policy: no source requires an edge, a rearm or a pulse, and no
 * arming 0 precedes the 1 (D-27). Completion requires fresh mode 1, running and alarm clear in
 * 2100 after the 0's acknowledgement, then one ERRORS read: ERRORS and ERROR_HISTORY may consume
 * data, so polling reads MONITORS and STATUS only and never rereads the history. 603F may keep
 * its old raw value; the final read keeps it raw. A persisting alarm repolls until the deadline
 * and never repeats a write. maximumAgeUs must cover five fields at the queue plus response
 * budgets, and service.identityWidths must stay DECLARED_U16.
 * @param[out] output Owned context, unchanged on failure and disjoint from inputs.
 * @param[in] reservation Continuous actual owner exclusion, CL86-C/OL86-C.
 * @param[in] state Complete fresh prerequisite; its raw values stay in incident.
 * @param[in] operation Nonzero live identity. @param[in] nowUs Monotonic microseconds.
 * @param[in] deadlineUs Exclusive future cutoff. @param[in] options Nonzero caller policies.
 * @return Local validation only. Caller/owner separately retains EMCY/error history;
 * this action never erases failed operations, retries, enables or resumes work. An observed
 * alarm-clear report is not proof that the cause is gone or permission to close an incident.
 */
Status prepareClearAlarm(ActionContext& output, const AxisReservation& reservation,
                         const ReadContext& state, uint32_t operation, uint64_t nowUs,
                         uint64_t deadlineUs, const ActionOptions& options) noexcept;
/** Prepare an explicit zero-only device counter clear; nonzero is UNSUPPORTED right after the
 * overlap check, before any state, axis or budget check.
 * Writes 2302 U16 values 0, 1, 0, each after the previous acknowledgement. The leading 0 arms the
 * documented 0 to 1 change and the last 0 is the manual return (EN86/ZH86 p15). At every possible
 * transmission use the axis-aware notice below: the first possible 1 withdraws origins, soft-limit
 * qualification, the reference witness and so every prepared target and cached observation,
 * exactly once, before the 1 can leave. Configured limit values stay. Post-ACK reads use that new
 * configuration epoch and observe signed raw 6064 zero only; no host origin, physical home,
 * arbitrary setter, counter mapping, standstill or zero-velocity claim is established, and an
 * already-zero baseline shows no change at all.
 * @param[out] output Owned atomic context, disjoint from all inputs.
 * @param[in] reservation Actual continuous CL86-C/OL86-C exclusion.
 * @param[in] state Complete fresh SDO MONITORS, STATUS and FEEDBACK under reservation: mode 1,
 * 2100 running and alarm clear, 606C raw zero. 6064 is kept raw in baseline. This is the library's
 * conservative stationarity evidence, not an observed shaft: a live test also needs that.
 * @param[in] nativePosition Native device request; only exactly zero supported.
 * @param[in] axis Valid matching axis with room in both configuration/confidence epochs.
 * @param[in] operation Nonzero live identity. @param[in] nowUs Monotonic microseconds.
 * @param[in] deadlineUs Exclusive future cutoff. @param[in] options Explicit caller policies;
 * maximumAgeUs must cover seven fields at the queue plus response budgets, and
 * service.identityWidths must stay DECLARED_U16.
 * @return Local validation. All three writes keep the original binding. After the last
 * acknowledgement, release old-epoch results and call CanBusOwner::advanceConfiguration before
 * admitting the new-epoch observation uploads. An uncertain write leaves its route unresolved:
 * no epoch advance, result release, new reservation or MCU restart qualifies it.
 */
Status prepareSetDevicePosition(ActionContext& output, const AxisReservation& reservation,
                                const ReadContext& state, int64_t nativePosition,
                                const AxisConfig& axis, uint32_t operation, uint64_t nowUs,
                                uint64_t deadlineUs, const ActionOptions& options) noexcept;
/** Report what prepareEnable would refuse, changing nothing. For a caller that must know before it
 * moves other state, such as an owner operation.
 * @param[in] output The context prepareEnable would write; read only for its overlap checks.
 * @param[in] reservation As prepareEnable. @param[in] state As prepareEnable.
 * @param[in] evidence As prepareEnable. @param[in] operation As prepareEnable.
 * @param[in] nowUs As prepareEnable. @param[in] deadlineUs As prepareEnable.
 * @param[in] options As prepareEnable.
 * @return The status prepareEnable would give. OK means prepareEnable with these same unchanged
 * values cannot refuse.
 */
Status validateEnable(const ActionContext& output, const AxisReservation& reservation,
                      const ReadContext& state, const EnableEvidence& evidence, uint32_t operation,
                      uint64_t nowUs, uint64_t deadlineUs, const ActionOptions& options) noexcept;
/** Prepare the documented position-mode enable sequence. GATED: no specimen has run it.
 * Writes nothing itself. nextAction first yields one upload each of 607A and 60FF, as the project's
 * accepted action prerequisite, not a poll; their effects stay unknown. Their bytes must equal the
 * reviewed values, or the action fails before any 6040 word: this is the decision for a target of
 * unknown origin. It then yields 6040 U16 words 6, 7, 15, one at a time. After each acknowledgement
 * it reads 2100..2102, 6041, 6061, 6064 and 606C until 6041 bits 0..2 read 001, 011 or 111, then
 * holds the next word until options.commandIntervalUs after that acknowledgement. Those patterns
 * come from applying the correspondence table of EN86/ZH86 p12 (6040=6 gives 001, 7 gives 011, 15
 * gives 111) as an expectation; no source documents a report after a write or its delay. Success
 * also needs 2100 released clear. Each cycle ends the sequence if mode leaves 1, a 2100 bit other
 * than released changes, a 6041 bit above 0..2 changes, 6064 changes or 606C is nonzero. 2100
 * decides alarm and running because EN86 p17 and ZH86 p18 agree on it; disputed 6041 bits decide
 * nothing and are only compared. Never zeroes a target, changes mode, clears a fault, replays a
 * word or issues a stop. Fresh specimen reads show low bits 7 and released clear with no enable in
 * any retained record, so only the 1, 3, 7 progression distinguishes this sequence; even success is
 * a drive report, not torque.
 * @param[out] output Owned context, unchanged on failure, disjoint from all inputs.
 * @param[in] reservation Continuous actual owner reservation, CL86-C/OL86-C.
 * @param[in] state Complete fresh SDO read of ERRORS, MONITORS, STATUS and FEEDBACK acquired under
 * reservation: mode 1; 2100 position-complete set, running, alarm, both soft limits and every
 * undocumented bit clear; 2100 released either way, because specimens read it clear before any
 * enable; 6041 equal to the reviewed word; 1001, 603F and 606C zero. Mode 0, 3 and 6 are refused.
 * Low bits 111 with released clear, the manual's enabled pattern and the specimens' power-up
 * report, are refused unless evidence.powerUpShaft records an attended observation.
 * @param[in] evidence Explicit gate, trial, reservation, operation, expiry, start history, shaft
 * observation and exact reviewed values, copied. A closed gate returns UNSUPPORTED; anything else
 * missing or mismatched returns INVALID_CONFIG.
 * @param[in] operation Nonzero identity unique while old owner work exists, equal to
 * evidence.operation.
 * @param[in] nowUs Nondecreasing monotonic microseconds, zero valid, before evidence.expiresUs.
 * @param[in] deadlineUs Immutable exclusive cutoff, more than three intervals plus
 * ENABLE_SERVICE_STEPS queue-and-response budgets ahead. That guarantees only that a drive which
 * answers every service within its budgets and shows each report in the first cycle can finish:
 * it does not guarantee completion. A later report, a fault, a lost reply or a withdrawal can still
 * end the sequence after word 6 or 7, leaving the drive in a partial state whose effects are
 * unknown.
 * @param[in] options Nonzero budgets; commandIntervalUs at least ENABLE_WORD_INTERVAL_US;
 * maximumAgeUs covering nine fields at the queue plus response budgets; DECLARED_U16 identity widths.
 * @return Local validation only. No I/O. Never establishes firmware qualification or a safe enable.
 */
Status prepareEnable(ActionContext& output, const AxisReservation& reservation,
                     const ReadContext& state, const EnableEvidence& evidence, uint32_t operation,
                     uint64_t nowUs, uint64_t deadlineUs, const ActionOptions& options) noexcept;
/** Record a local withdrawal while this action's admitted work is still in flight, so that its
 * terminal is consumed as evidence and cannot complete the action. Sends nothing and stops nothing.
 * @param[in,out] context Active context with no earlier withdrawal; unchanged on refusal.
 * @param[in] live Actual reservation; INVALIDATED alone permits a changed or withdrawn reservation.
 * @param[in] current Actual full target and epochs, checked except for INVALIDATED.
 * @param[in] reason CANCELLED, DRIVE_FAULT or INVALIDATED. At or after the cutoff it is DEADLINE.
 * @param[in] nowUs Nondecreasing supplied time.
 * @return Local status. Afterwards nextAction yields nothing, and the next accepted terminal ends
 * the action with this reason, keeping that terminal in commandHistory or the observation evidence.
 * The local advanceAction overload still ends it at once once the work has been retired.
 */
Status noteActionWithdrawal(ActionContext& context, const AxisReservation& live,
                            const ReadTarget& current, ActionOutcome reason, uint64_t nowUs) noexcept;
/** Yield copied work without mutation, I/O or automatic replay.
 * @param[in] context Prepared action. @param[in] live Actual continuous owner reservation.
 * @param[in] current Actual current full target/epochs. @param[in] nowUs Nondecreasing caller time.
 * @param[out] output Unchanged on rejection; no overlap with inputs. ready=false during wait/terminal.
 * @return Local status; active calls at/after cutoff reject until explicitly advanced to DEADLINE.
 */
Status nextAction(const ActionContext& context, const AxisReservation& live,
                  const ReadTarget& current, uint64_t nowUs, PreparedAction& output) noexcept;
/** Record possible stop/alarm/enable command transmission; never sends or modifies a binding.
 * Counter actions reject this overload to prevent bypassing coordinate invalidation.
 * @param[in,out] context Pending download, exactly one notice per step. Uploads use owner terminals.
 * @param[in] live Actual reservation. @param[in] current Actual target/epochs.
 * @param[in] nowUs Nondecreasing time before cutoff and not before eligibility.
 * @return Local status; rejected/overlapping inputs leave context unchanged.
 */
Status noteActionTransmission(ActionContext& context, const AxisReservation& live,
                              const ReadTarget& current, uint64_t nowUs) noexcept;
/** Axis-aware counter notice; required for every counter-action download.
 * First possible U16=1 trigger atomically invalidates coordinates/origins/limits,
 * advances both axis epochs and current's configuration, and records uncertainty.
 * Initial/final zero only note possible TX and never withdraw again. No rollback follows
 * owner refusal or cancellation after this conservative boundary. A trigger refused or
 * cancelled before this notice leaves coordinatesInvalidated false and every epoch as it
 * was. Does no I/O or persistence.
 * @param[in,out] context Active counter context. @param[in] live Frozen live reservation.
 * @param[in,out] current Actual current target, old then exactly one advanced configuration.
 * @param[in,out] axis Actual frozen matching axis interpretation.
 * @param[in,out] reference Position witness to withdraw, no replacement synthesized.
 * @param[in] nowUs Monotonic time before cutoff. @return Atomic status on rejection;
 * all mutable outputs and borrowed reservation must be mutually disjoint.
 */
Status noteActionTransmission(ActionContext& context, const AxisReservation& live,
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
Status advanceAction(ActionContext& context, const AxisReservation& live,
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
Status advanceAction(ActionContext& context, const AxisReservation& live,
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
