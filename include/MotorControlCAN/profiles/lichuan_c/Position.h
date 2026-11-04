/** @file Position.h
 * @brief 86C position parameters and the finite absolute and relative move lifecycle.
 * Functions perform bounded C++11 work without I/O, allocation, clock access, retries or replay.
 * SPDX-License-Identifier: MIT
 *
 * preparePositionParameters is pure arithmetic. Both moves are GATED: the acceptance and completion
 * reports of these specimens are not established, so MoveEvidence carries the caller's reviewed
 * measurement and a closed gate refuses. A success with those reports unqualified cannot happen.
 * A relative move is gated twice: the manuals give the bit 6 trigger but not what the drive adds
 * the displacement to, so MoveEvidence::relativeBase must name the measured base. No base is
 * measured on any specimen, so every relative request is refused with UNSUPPORTED today. The
 * absolute lifecycle ran once on one specimen and firmware (docs/position_parameters.md); no specimen
 * has run a relative one, and fake-owner tests are the rest of the evidence.
 *
 * Phase order (D-01): the caller's fresh state read proves mode 1, low bits 111 and released clear
 * after the actual latest acknowledged 000F under this reservation, which no move writes itself.
 * Then 607A, 6081, 6083, 6084 in that order (EN86/ZH86 p11-12; OP86 p9-10, whose running header names
 * CL57-C/OL57-C), then the bit 4 edge, 000F
 * to 001F for an absolute target or 000F to 005F for a relative one, held until the move is terminal
 * (DEC-8). Then fresh observation cycles collect a new acceptance and a new completion under the
 * caller's evidence. Only after an observed completion is 000F written, as its own visible step; its
 * result never erases that observation, and a failed or uncertain 000F is never retried. No 6060 or
 * 6061 write, enable, fault clear, counter zero, origin change or stop is emitted. Immediate update
 * (003F/007F) is refused. A relative move is never converted to an absolute one. Every yielded
 * download and observation needs the actual live reservation; contexts are written in place, never
 * copied onto the stack.
 */
#pragma once
#include <MotorControlCAN/MoveOperation.h>
#include <MotorControlCAN/profiles/lichuan_c/Settings.h>

namespace MotorControlCAN {
namespace LICHUAN_C {
/** Function-specific Status::detail for pure parameter preparation. */
enum class PositionParameterError : int32_t {
    NONE,           ///< No parameter error.
    ARGUMENT,       ///< Wrapped request or supplied time outside its monotonic domain.
    MODEL,          ///< No explicit CL86_C or OL86_C documentary applicability.
    REPRESENTATION, ///< Integer native representation is required; binary32 is unsupported.
    RANGE,          ///< Requested/effective target, profile rpm or ramp time exceeds its range.
    OVERLAP,        ///< Mutable output overlaps a borrowed input.
    AXIS_ERROR_BASE = 1000 ///< Added to an underlying AxisError, preserving its Err and message.
};
/** Owned parameter intent, independent of settings readback or device admission.
 * EDS [607A]/[6081]/[6083]/[6084] provide the object widths, and EN86/ZH86 PDF p11
 * and OP86 p9 the documentary ranges; the EDS gives no limits. Neither zero speed/ramp nor zero displacement
 * establishes a useful trajectory, standstill or completion. Retain this value
 * without editing fields; later admission must independently check current
 * binding, configuration, reference, firmware and operation prerequisites.
 */
struct PreparedPositionParameters {
    SettingsModel model =
        SettingsModel::UNKNOWN; ///< Explicit documented model, not firmware qualification.
    PreparedTarget target; ///< Exact intent, endpoint, conversion error and any frozen reference.
    AxisConfig axis; ///< Complete original units, origins, limits, binding and confidence epochs.
    DriverSettings settings; ///< Owned requested 6081 rpm and 6083/6084 ramp times in milliseconds.
    int32_t targetNative =
        0; ///< 607A candidate: absolute coordinate or relative displacement in pulses.
    uint64_t preparedUs = 0; ///< Supplied preparation time; zero is valid, not an observation time.
};
/** Prepare an immutable native parameter candidate atomically.
 * Target position 607A is I32 with inclusive -1000000..1000000 command pulses;
 * 6081 is U32 with 0..3000 rpm, and 6083/6084 are U32 with 0..2000 ms.
 * Requested AND quantized encoded targets must fit the documentary range.
 * Exact arithmetic checks the requested value exactly. Explicit approximate
 * arithmetic uses the existing upward total error bound around the effective
 * integer, rejecting conservatively when that interval crosses either bound.
 * Relative range checks apply to displacement; preparePosition separately
 * checks requested/effective absolute endpoints against the supplied axis limits.
 * All three relative bases describe host arithmetic only when the supplied axis
 * policy and matching witness allow them; no drive FIFO or wire basis is inferred.
 * Absolute targets without a witness retain unknown displacement. Zero is valid
 * parameter arithmetic and never yields a completed-move result.
 *
 * @param[out] output Owned snapshot, unchanged on every failure; must not overlap
 * request, axis, reference or settings, including their nested subobjects.
 * @param[in] model Explicit CL86_C or OL86_C selection; other values reject.
 * @param[in] request Unwrapped position intent, with the existing explicit
 * approximation policy for nonzero radians; no implicit approximation is added.
 * @param[in] axis Complete caller-owned host coordinate configuration.
 * @param[in] reference Optional witness, required by preparePosition policies.
 * A supplied witness is always checked. Its observation time, age cap, basis and
 * provenance are preserved; a local copy uses nowUs as the validation time.
 * A cached reference.nowUs later than nowUs rejects as a backward clock.
 * @param[in] settings Explicit native profile values; invalid defaults reject.
 * @param[in] nowUs Actual current nonwrapping monotonic microseconds in the same
 * domain as reference, inclusive 0..INT64_MAX. No timestamp is sampled or renewed.
 * @return OK proves parameter arithmetic only. Local errors use
 * PositionParameterError. Axis failures retain their Err/message and add
 * AXIS_ERROR_BASE to detail. Inputs are borrowed for this call only; the result
 * owns its data. Callers serialize mutation of shared inputs/output. No service
 * route, deadline, reservation, mode, acceptance or completion is established.
 */
Status preparePositionParameters(PreparedPositionParameters& output, SettingsModel model,
                                 const PositionRequest& request, const AxisConfig& axis,
                                 const AxisReference* reference, const DriverSettings& settings,
                                 uint64_t nowUs) noexcept;

/** Services one absolute or relative move schedules at least: up to four parameter downloads, the
 * trigger, one seven-field observation cycle and the rearm. Both preparations need a deadline beyond
 * this many queue-and-response budgets. Further cycles are not counted, so it guarantees no completion. */
constexpr uint64_t MOVE_SERVICE_STEPS = 13;
/** Fields of one move observation cycle: MONITORS 2100..2102, STATUS 6041 and 6061, FEEDBACK 6064
 * and 606C, read in that order, sequentially and not as an atomic snapshot. */
constexpr unsigned MOVE_OBSERVATION_FIELDS = 7;
/** Whether a move may run at all. The capability gate, not a readiness claim. */
enum class MoveGate : uint8_t {
    CLOSED,  ///< Default: both move preparations return UNSUPPORTED with nothing changed.
    /** The caller's reviewed measurement of acceptance and completion on this exact firmware, from the
     * packet 19 trial. The library cannot check the measurement, only bind and apply it. */
    MEASURED
};
/** Which raw word of an observation cycle a move signal is read from. */
enum class MoveSignalSource : uint8_t {
    NONE,       ///< Default: no signal. Refused.
    STATUSWORD, ///< 6041 of the STATUS block. No layout is selected or implied by this.
    MONITOR     ///< 2100 of the MONITORS block.
};
/** One measured report: present when `(word & mask) == value`. No default mask exists: EN86 bit 12,
 * bit 15 and 2100 bit 0 are documentary candidates whose behavior on these specimens is unmeasured. */
struct MoveSignal {
    MoveSignalSource source = MoveSignalSource::NONE; ///< Word the report is read from.
    uint16_t mask = 0;  ///< Nonzero bits examined.
    uint16_t value = 0; ///< Bits within mask that mean present.
};
/** What makes a completion report new for this trigger, as the caller's measurement established. */
enum class CompletionRule : uint8_t {
    NONE, ///< Default: refused.
    /** Completion counts only in a cycle after an earlier post-trigger cycle reported it absent, with
     * acceptance reported in that cycle or before. A move shorter than one cycle then never completes,
     * and ends at its deadline with completion NOT_OBSERVED. */
    AFTER_ABSENCE,
    /** Completion counts in any cycle after the cycle that first reported acceptance. Only valid if the
     * measurement showed that acceptance withdraws an earlier completion report. */
    AFTER_ACCEPTANCE
};
/** Raw identity values the measurement was made on, compared exactly with a current identity read. */
struct MoveFirmware {
    uint32_t deviceType = 0;      ///< 1000 raw.
    uint16_t hardwareVersion = 0; ///< 1009 raw value.
    uint16_t softwareVersion = 0; ///< 100A raw value.
    uint32_t vendorId = 0;        ///< 1018:01 raw.
    uint32_t productCode = 0;     ///< 1018:02 raw.
    uint32_t revision = 0;        ///< 1018:03 raw.
    uint16_t modelCode = 0;       ///< 2000 raw.
    uint16_t firmwareCode = 0;    ///< 2001 raw.
};
/** What the drive adds a relative displacement to on the 000F to 005F edge, as the caller's
 * measurement established on this exact firmware. EN86/ZH86 p12 and OP86 p10 give the 15 to 95
 * trigger and bit 6 as relative positioning, but no base. A queued base is absent: this lifecycle
 * admits a move only from an idle drive and never puts an edge into a running curve. Bit 5 = 0 is
 * documented as updating the parameters after the current curve completes (EN86/ZH86 p12), with no
 * capacity, acknowledgement or base.
 */
enum class RelativeBase : uint8_t {
    UNMEASURED, ///< Default: every relative request returns UNSUPPORTED with RELATIVE_BASIS.
    /** Measured: the drive adds the displacement to its actual position at the edge. Admits a
     * relative request whose basis and witness are ACTUAL. */
    ACTUAL,
    /** Measured: the drive adds the displacement to its last commanded position, which may differ
     * from its actual one. Admits a relative request whose basis and witness are COMMANDED. */
    COMMANDED
};
/** The caller's explicit move capability evidence. Defaults refuse. It is copied and frozen as the
 * move's dialect: a later change of evidence does not reach a running move.
 */
struct MoveEvidence {
    MoveGate gate = MoveGate::CLOSED; ///< Must be MEASURED.
    uint32_t record = 0; ///< Nonzero identity of the reviewed measurement record; the dialect epoch.
    SettingsModel model = SettingsModel::UNKNOWN; ///< Model measured; must equal the reservation's.
    MoveFirmware firmware; ///< Identity measured; must equal the current identity read.
    /** Report that this trigger was taken. It must be absent in the admission read, so an earlier
     * acceptance cannot pass for this one. */
    MoveSignal acceptance;
    MoveSignal completion; ///< Report that the controller considers the move complete.
    CompletionRule rule = CompletionRule::NONE; ///< What makes a completion report new.
    /** Relative moves only: the drive's own base, measured in the same record. Ignored by
     * prepareMoveAbsolute. The acceptance and completion signals apply to the 005F trigger only if
     * the record measured them on it. */
    RelativeBase relativeBase = RelativeBase::UNMEASURED;
};
/** How the move treats 6081, 6083 and 6084. 607A is always written. */
enum class MoveSetup : uint8_t {
    WRITE_ALL,   ///< Default: write all four parameters.
    /** Skip each of 6081, 6083, 6084 that a supplied fresh, complete, unretired MOTION_PARAMETERS read
     * under this reservation shows already equal. Contradicted or retired reads are refused. */
    REUSE_STORED
};
/** Native 86C profile choices for one move. Defaults refuse. */
struct MoveProfile {
    uint32_t rampUpTimeMs = UINT32_MAX;   ///< 6083 acceleration time, 1..2000 ms; zero refused (Q11).
    uint32_t rampDownTimeMs = UINT32_MAX; ///< 6084 deceleration time, 1..2000 ms; zero refused (Q11).
    MoveSetup setup = MoveSetup::WRITE_ALL; ///< Parameter write policy.
    /** Bit 5 immediate update (003F, or 007F for a relative target). A new edge during a curve is
     * unqualified, so true is UNSUPPORTED for both moves. */
    bool immediateUpdate = false;
};
/** Caller policy, not manufacturer timing. Defaults refuse. */
struct MoveOptions {
    ReadOptions service; ///< Nonzero queue and response budgets per service; DECLARED_U16 identity widths.
    uint64_t observationIntervalUs = 0; ///< Nonzero delay from one finished cycle to the next.
    /** Nonzero inclusive age cap for the admission read and each cycle, at least seven fields at the
     * queue plus response budgets. */
    uint64_t maximumAgeUs = 0;
};
/** Concrete phase of an active move. */
enum class MovePhase : uint8_t {
    PARAMETERS, ///< Current parameter download pending; never replayed.
    TRIGGER,    ///< 6040=001F, or 005F for a relative move, pending; never replayed.
    WAITING,    ///< Next observation cycle due at eligibleUs; beginMoveObservation prepares it.
    OBSERVING,  ///< The borrowed observation read's cycle is in progress.
    REARM       ///< Completion observed; 6040=000F pending as its own step.
};
/** One observation the move kept, raw. Not an interpretation, standstill or position claim. */
struct MoveSample {
    bool present = false;    ///< This record holds an observation.
    uint32_t cycle = 0;      ///< Post-trigger cycle, from 1; 0 is the admission read.
    uint16_t statusword = 0; ///< 6041 raw.
    uint16_t monitor = 0;    ///< 2100 raw.
    uint8_t modeRaw = 0;     ///< 6061 raw.
    bool feedback = false;   ///< position and velocity were read; false for the admission read.
    int32_t position = 0;    ///< 6064 signed raw, no scale or command-coordinate relation.
    int32_t velocity = 0;    ///< 606C signed raw, no unit.
    uint64_t earliestUs = 0; ///< Earliest bound of the fields kept here.
    uint64_t latestUs = 0;   ///< Latest bound of the fields kept here.
};
/** Caller-owned concrete absolute or relative move. The terminal context is the result. Serialize
 * access and do not edit fields. Histories survive failure. Inputs are borrowed during calls only;
 * the context owns everything it keeps, and it never holds a ReadContext: observations run in a
 * borrowed one.
 *
 * A relative move keeps two different bases apart. The host arithmetic reference is
 * parameters.target.retainedReference, the witness snapshot the endpoint was computed from. The
 * drive's native base is evidence.relativeBase, the caller's measurement of what the drive adds the
 * displacement to. They name the same basis, or preparation refused. parameters.target.endpointNative
 * is the host's expected endpoint; whether the drive reaches it depends on its base equalling the
 * witness at the edge, which the idle, stationary, fresh witness supports and nothing here observes.
 * A completion report never establishes that endpoint.
 */
struct MoveContext {
    AxisReservation reservation; ///< Frozen route, model, epochs and continuous exclusion.
    MoveEvidence evidence;       ///< Frozen capability evidence: the dialect, and the native base.
    MoveProfile profile;         ///< Frozen native profile choices.
    MoveOptions options;         ///< Frozen caller budgets.
    /** Frozen requested and effective 607A value, quantization error, units and frame, scale, origin,
     * limits, binding, configuration and reference epochs, the exact witness snapshot, and the
     * 6081/6083/6084 values written. target.requested.relative selects the trigger word. For a
     * relative move targetNative is the displacement and target.endpointNative the host endpoint. */
    PreparedPositionParameters parameters;
    /** Frozen speed: requested rate, quantized once in rpm (MOTOR_TURNS per MINUTE), with its error.
     * Its effectiveNative is parameters.settings.positionSpeedRpm. */
    PreparedVelocityTarget speed;
    MoveDisplacement displacement = MoveDisplacement::UNKNOWN; ///< Whether a witness fixed it.
    /** KNOWN only: endpoint minus witness, never zero. For a relative move, the quantized
     * displacement written to 607A. */
    int64_t displacementNative = 0;
    ActionState state = ActionState::EMPTY;    ///< Overall phase, distinct from drive state.
    MovePhase phase = MovePhase::PARAMETERS;   ///< Current concrete work.
    ActionOutcome outcome = ActionOutcome::NONE; ///< Immutable first terminal cause.
    MoveInterruption interruption = MoveInterruption::NONE; ///< Local reason, when one ended it.
    /** Reason recorded while work was in flight. Nothing new is yielded, and the next consumed
     * terminal ends the move with it, kept as evidence. */
    MoveInterruption withdrawal = MoveInterruption::NONE;
    /** Parameter downloads together: ACKNOWLEDGED once every planned one was, UNKNOWN for a partial
     * setup in which some may have reached the drive; setupHistory says which. */
    ActionExecution setup = ActionExecution::NOT_TRANSMITTED;
    ActionExecution execution = ActionExecution::NOT_TRANSMITTED; ///< The 001F or 005F trigger only.
    /** Controller report under the frozen evidence only, never independent motion or standstill. Once
     * OBSERVED it stays, whatever the rearm does. */
    ActionCompletion completion = ActionCompletion::NOT_OBSERVED;
    ActionExecution rearm = ActionExecution::NOT_TRANSMITTED; ///< The 000F after completion only.
    Status status;            ///< Local or native operation status, separate from health.
    uint32_t operation = 0;   ///< Nonzero live operation identity, kept for every result.
    uint32_t step = 1;        ///< Nonwrapping identity across every download and observation upload.
    uint32_t observations = 0; ///< Complete post-trigger cycles.
    uint64_t startedUs = 0;   ///< Caller preparation time, zero valid.
    uint64_t servicedUs = 0;  ///< Last local mutation time.
    uint64_t deadlineUs = 0;  ///< Immutable exclusive cutoff; `now >= deadlineUs` expires.
    uint64_t eligibleUs = 0;  ///< Earliest time for the current work.
    uint64_t stepStartedUs = 0;        ///< Earliest admission for the current service.
    uint64_t observationStartedUs = 0; ///< Preparation time of the borrowed read now in use.
    uint8_t setupMask = 0;         ///< Downloads planned: bit 0 607A, 1 6081, 2 6083, 3 6084.
    uint8_t setupCursor = 0;       ///< Current parameter bit.
    uint8_t setupEvidenceMask = 0; ///< setupHistory slots holding a terminal.
    bool transmissionNoted = false; ///< Current download crossed the possible-TX boundary.
    bool effectsPossible = false;   ///< At least one download may have reached the drive.
    /** Unconfirmed download effects, unqualified read effects, or a possibly sent trigger without an
     * observed completion. */
    bool uncertain = false;
    bool readUncertain = false; ///< An observation upload left an unqualified read effect; never refined.
    CANOPEN::SdoCompletion control; ///< Predecessor: the acknowledged 000F the move followed.
    CANOPEN::SdoCompletion setupHistory[4]; ///< Each parameter terminal by bit, including failure.
    CANOPEN::SdoCompletion trigger;      ///< 001F or 005F terminal, when triggerPresent.
    bool triggerPresent = false;         ///< Makes trigger meaningful.
    CANOPEN::SdoCompletion rearmEvidence; ///< 000F terminal, when rearmPresent.
    bool rearmPresent = false;           ///< Makes rearmEvidence meaningful.
    CANOPEN::SdoCompletion observationEvidence; ///< Latest observation upload terminal.
    bool observationEvidencePresent = false;    ///< Makes observationEvidence meaningful.
    MoveSample admission;  ///< Prerequisite read before any write.
    MoveSample latest;     ///< Latest complete cycle.
    MoveSample acceptance; ///< First cycle reporting acceptance.
    MoveSample absence;    ///< First post-trigger cycle reporting completion absent.
    MoveSample completed;  ///< Cycle whose completion report qualified.
};
/** Owned yielded work, not admission or transmission permission. */
struct PreparedMove {
    CANOPEN::SdoTransfer transfer; ///< Complete copied service when ready.
    bool ready = false;            ///< A download or observation upload is ready.
    /** An observation cycle is due: prepare it with beginMoveObservation, then call nextMove again. */
    bool observe = false;
    uint64_t eligibleUs = 0; ///< Earliest caller processing time for the current work.
};
/** Report what prepareMoveAbsolute would refuse, changing nothing. For a caller that must know before
 * it moves other state, such as an owner operation.
 * @param[in] output The context prepareMoveAbsolute would write; read only for overlap checks.
 * @param[in] reservation As prepareMoveAbsolute.
 * @param[in] current As prepareMoveAbsolute.
 * @param[in] state As prepareMoveAbsolute.
 * @param[in] control As prepareMoveAbsolute.
 * @param[in] identity As prepareMoveAbsolute.
 * @param[in] evidence As prepareMoveAbsolute.
 * @param[in] request As prepareMoveAbsolute.
 * @param[in] profile As prepareMoveAbsolute.
 * @param[in] stored As prepareMoveAbsolute.
 * @param[in] axis As prepareMoveAbsolute.
 * @param[in] reference As prepareMoveAbsolute.
 * @param[in] operation As prepareMoveAbsolute.
 * @param[in] nowUs As prepareMoveAbsolute.
 * @param[in] deadlineUs As prepareMoveAbsolute.
 * @param[in] options As prepareMoveAbsolute.
 * @return The status prepareMoveAbsolute would give. OK means it cannot refuse the same values.
 */
Status validateMoveAbsolute(const MoveContext& output, const AxisReservation& reservation,
                            const ReadTarget& current, const ReadContext& state,
                            const CANOPEN::SdoCompletion& control,
                            const IdentityObservation& identity, const MoveEvidence& evidence,
                            const MoveRequest& request, const MoveProfile& profile,
                            const DriverContext* stored, const AxisConfig& axis,
                            const AxisReference* reference, uint32_t operation, uint64_t nowUs,
                            uint64_t deadlineUs, const MoveOptions& options) noexcept;
/** Prepare one finite absolute move. GATED. Writes nothing itself; all refusals leave output unchanged.
 * @param[out] output Owned context, constructed in place on success; must not overlap any input.
 * @param[in] reservation Actual continuous owner reservation, CL86_C or OL86_C, acquired before the
 * control acknowledgement and the state read.
 * @param[in] current Actual current route and epochs; must equal reservation.target, so a token left
 * from a finished or uncertain transaction is refused.
 * @param[in] state Complete fresh SDO read under the reservation, after control completed, with
 * STATUS and MONITORS: mode 1, 6041 low bits 111, and 2100 with no running, alarm, released, soft-limit
 * or undocumented bit. Fresh flags alone cannot show an enable (specimens read 111 at power-up), so
 * the 000F history is the enable evidence. The acceptance report must be absent here.
 * @param[in] control The actual latest acknowledged 6040:00 U16 under this reservation; exactly 000F.
 * A move never writes 000F to make one. After a trigger, a stop or an uncertain word none exists.
 * @param[in] identity Current identity read on reservation.target, node epoch included, whose raw values
 * equal evidence.firmware exactly. Missing firmware evidence refuses with UNSUPPORTED.
 * @param[in] evidence Capability gate and measured signals, copied. CLOSED returns UNSUPPORTED.
 * @param[in] request Unwrapped absolute position and positive speed, both with configuration epoch
 * axis.binding.configurationGeneration. A relative or wrapped position is refused here.
 * @param[in] profile Native ramps and policies. Immediate update returns UNSUPPORTED.
 * @param[in] stored REUSE_STORED only: fresh complete MOTION_PARAMETERS read under the reservation on
 * current, unretired. Ignored, and may be null, for WRITE_ALL. It must also be in the retained list of
 * every later noteMoveTransmission on this axis, or a later reuse can miss a value a move wrote.
 * @param[in] axis Current coordinate configuration. The speed is prepared once with
 * prepareVelocityTarget and must come out integer rpm: MOTOR_TURNS per MINUTE. Any other destination,
 * such as COMMAND_INCREMENTS per SECOND, is refused even when its value fits 6081.
 * @param[in] reference Optional witness. A supplied established one must be idle, stationary, ACTUAL
 * and fresh, observed at or after control completed, and fixes the displacement, which must not be
 * zero. Without one the displacement stays UNKNOWN and an absolute zero is an ordinary target.
 * @param[in] operation Nonzero identity, unique while old owner work exists.
 * @param[in] nowUs Nondecreasing monotonic microseconds, 0..INT64_MAX.
 * @param[in] deadlineUs Immutable exclusive cutoff, beyond MOVE_SERVICE_STEPS queue-and-response
 * budgets. That cannot guarantee completion.
 * @param[in] options Nonzero budgets and interval; maximumAgeUs covering seven fields.
 * @return Local validation only. 607A and the speed must fit their documented ranges both as requested
 * and as quantized: 607A -1000000..1000000 pulses, 6081 1..3000 rpm. Details use MoveError; position
 * failures add PARAMETER_ERROR_BASE to a PositionParameterError and speed failures add RATE_ERROR_BASE
 * to an AxisError, keeping the original Err and message. No I/O.
 */
Status prepareMoveAbsolute(MoveContext& output, const AxisReservation& reservation,
                           const ReadTarget& current, const ReadContext& state,
                           const CANOPEN::SdoCompletion& control, const IdentityObservation& identity,
                           const MoveEvidence& evidence, const MoveRequest& request,
                           const MoveProfile& profile, const DriverContext* stored,
                           const AxisConfig& axis, const AxisReference* reference,
                           uint32_t operation, uint64_t nowUs, uint64_t deadlineUs,
                           const MoveOptions& options) noexcept;
/** Report what prepareMoveRelative would refuse, changing nothing.
 * @param[in] output The context prepareMoveRelative would write; read only for overlap checks.
 * @param[in] reservation As prepareMoveRelative.
 * @param[in] current As prepareMoveRelative.
 * @param[in] state As prepareMoveRelative.
 * @param[in] control As prepareMoveRelative.
 * @param[in] identity As prepareMoveRelative.
 * @param[in] evidence As prepareMoveRelative.
 * @param[in] request As prepareMoveRelative.
 * @param[in] profile As prepareMoveRelative.
 * @param[in] stored As prepareMoveRelative.
 * @param[in] axis As prepareMoveRelative.
 * @param[in] reference As prepareMoveRelative.
 * @param[in] operation As prepareMoveRelative.
 * @param[in] nowUs As prepareMoveRelative.
 * @param[in] deadlineUs As prepareMoveRelative.
 * @param[in] options As prepareMoveRelative.
 * @return The status prepareMoveRelative would give. OK means it cannot refuse the same values.
 */
Status validateMoveRelative(const MoveContext& output, const AxisReservation& reservation,
                            const ReadTarget& current, const ReadContext& state,
                            const CANOPEN::SdoCompletion& control,
                            const IdentityObservation& identity, const MoveEvidence& evidence,
                            const MoveRequest& request, const MoveProfile& profile,
                            const DriverContext* stored, const AxisConfig& axis,
                            const AxisReference& reference, uint32_t operation, uint64_t nowUs,
                            uint64_t deadlineUs, const MoveOptions& options) noexcept;
/** Prepare one finite relative move: the same phases, gates and progression as prepareMoveAbsolute,
 * with 607A holding the displacement and the edge 000F to 005F. GATED twice: a closed gate returns
 * UNSUPPORTED with CAPABILITY, and a request whose basis the evidence's relativeBase does not name,
 * including every QUEUED request, returns UNSUPPORTED with RELATIVE_BASIS. No relative base is
 * measured on any specimen. Writes nothing itself; all refusals leave output unchanged.
 * @param[out] output Owned context, constructed in place on success; must not overlap any input.
 * @param[in] reservation As prepareMoveAbsolute.
 * @param[in] current As prepareMoveAbsolute.
 * @param[in] state As prepareMoveAbsolute.
 * @param[in] control As prepareMoveAbsolute: exactly 000F.
 * @param[in] identity As prepareMoveAbsolute.
 * @param[in] evidence As prepareMoveAbsolute, with relativeBase equal to request.position.basis.
 * @param[in] request Unwrapped relative position whose basis is the measured base, and positive speed,
 * both with configuration epoch axis.binding.configurationGeneration. Absolute or wrapped intent is
 * refused here. Never converted to an absolute target, and no origin or counter is changed.
 * @param[in] profile As prepareMoveAbsolute. Immediate update (007F) returns UNSUPPORTED.
 * @param[in] stored As prepareMoveAbsolute.
 * @param[in] axis Current coordinate configuration, whose supportedRelativeBases must admit the basis.
 * The displacement must fit -1000000..1000000 pulses requested and quantized, and both endpoints must
 * fit the axis's native and soft limits.
 * @param[in] reference Required host witness of the request's basis: known, idle, stationary, fresh,
 * with a nonzero age cap, observed at or after control completed. Its snapshot is frozen, and every
 * parameter and trigger notice rechecks it against the caller's current witness.
 * @param[in] operation As prepareMoveAbsolute.
 * @param[in] nowUs As prepareMoveAbsolute.
 * @param[in] deadlineUs As prepareMoveAbsolute.
 * @param[in] options As prepareMoveAbsolute.
 * @return Local validation only, with prepareMoveAbsolute's details. A zero effective displacement
 * returns ZERO_DISPLACEMENT (R-08) before any write. No I/O.
 */
Status prepareMoveRelative(MoveContext& output, const AxisReservation& reservation,
                           const ReadTarget& current, const ReadContext& state,
                           const CANOPEN::SdoCompletion& control, const IdentityObservation& identity,
                           const MoveEvidence& evidence, const MoveRequest& request,
                           const MoveProfile& profile, const DriverContext* stored,
                           const AxisConfig& axis, const AxisReference& reference,
                           uint32_t operation, uint64_t nowUs, uint64_t deadlineUs,
                           const MoveOptions& options) noexcept;
/** Return a context to EMPTY in place, without a stack temporary.
 * @param[out] context Context to reset; its retained history is gone.
 */
void resetMove(MoveContext& context) noexcept;
/** Yield copied work without mutation, I/O or replay.
 * @param[in] context Prepared move.
 * @param[in] observation The borrowed read the move's cycles run in. Used only in OBSERVING, when it
 * must be the read beginMoveObservation prepared.
 * @param[in] live Actual continuous owner reservation.
 * @param[in] current Actual current route and epochs.
 * @param[in] nowUs Nondecreasing caller time.
 * @param[out] output Unchanged on refusal; no overlap with inputs. Neither flag is set while waiting,
 * withdrawn or terminal, or once the current download has had its noteMoveTransmission: it is in
 * flight and is never offered again. Nor is either set for an observation once one exchange's
 * queue and response budgets no longer fit before the deadline, so no read can be cut there; the
 * move then ends at its deadline. Downloads are not held back.
 * @return Local status. An active move at or after its cutoff refuses with TIME until it is ended.
 */
Status nextMove(const MoveContext& context, const ReadContext& observation,
                const AxisReservation& live, const ReadTarget& current, uint64_t nowUs,
                PreparedMove& output) noexcept;
/** Prepare the due observation cycle in a borrowed read, in place. The move's own operation, deadline
 * and budgets are used, with MONITORS, STATUS and FEEDBACK.
 * @param[in,out] context Move in WAITING whose cycle is due.
 * @param[out] observation Read to replace; must not overlap the other inputs. Keep it stationary until
 * the cycle ends. A different read holding the last complete cycle stays untouched, so a stop request
 * can still use it.
 * @param[in] live Actual reservation. @param[in] current Actual target and epochs.
 * @param[in] nowUs Time at or after eligibility and before the cutoff.
 * @return Local status; both outputs unchanged on refusal.
 */
Status beginMoveObservation(MoveContext& context, ReadContext& observation,
                            const AxisReservation& live, const ReadTarget& current,
                            uint64_t nowUs) noexcept;
/** Record that the current download may now be transmitted. Call after owner admission and preflight,
 * before the next owner turn. A parameter or trigger download is the move's actual admission, so it
 * is rechecked here first: validatePreparedTarget against the current axis and witness, which must
 * still be idle and stationary, and the admission state read's age against options.maximumAgeUs.
 * A parameter download then calls retireMotionParameters on each retained context, so that a
 * settings call cannot report an unchanged value with no TX while the drive holds the move's. If any
 * of this refuses, the context does not change and the download must not be transmitted: cancel its
 * admission. Contexts listed before a refused retirement stay retired, which only withdraws trust.
 * The rearm takes no recheck: it follows an observed completion.
 * @param[in,out] context Active move with a pending download; one notice per download.
 * @param[in] live Actual reservation. @param[in] current Actual target and epochs.
 * @param[in] axis The caller's current coordinate configuration of this axis, so that a later
 * configureAxis, origin change or reference invalidation refuses the next write.
 * @param[in] reference The caller's current witness when the move used one, null otherwise. A changed,
 * aged, moving or invalidated witness refuses.
 * @param[in,out] retained Every retained MOTION_PARAMETERS-capable settings context of this axis,
 * including a finished settings context's readback; null only with count zero.
 * @param[in] count Entries in retained; each must be non-null.
 * @param[in] nowUs Time at or after eligibility and before the cutoff.
 * @return Local status. A recheck failure on the target or witness returns the AxisError detail plus
 * PARAMETER_ERROR_BASE and PositionParameterError::AXIS_ERROR_BASE, REFERENCE for a moving witness,
 * or STATE for an expired admission read. Observation uploads take no notice: their owner terminal
 * settles them.
 */
Status noteMoveTransmission(MoveContext& context, const AxisReservation& live,
                            const ReadTarget& current, const AxisConfig& axis,
                            const AxisReference* reference, DriverContext* const* retained,
                            uint8_t count, uint64_t nowUs) noexcept;
/** Record a local reason while work is in flight, so its terminal is kept as evidence and cannot
 * advance the move. Sends nothing and stops nothing.
 * @param[in,out] context Active move with no earlier withdrawal; unchanged on refusal.
 * @param[in] live Actual reservation; INVALIDATED alone permits a changed one.
 * @param[in] current Actual target; checked except for INVALIDATED.
 * @param[in] reason CANCELLED, STOP, DRIVE_FAULT or INVALIDATED. At or after the cutoff it is DEADLINE.
 * @param[in] nowUs Nondecreasing time.
 * @return Local status.
 */
Status noteMoveWithdrawal(MoveContext& context, const AxisReservation& live,
                          const ReadTarget& current, MoveInterruption reason, uint64_t nowUs) noexcept;
/** Consume the actual owner terminal of the move's current work; invalid evidence changes nothing.
 * A download terminal settles its phase. An observation terminal advances the borrowed read; a finished
 * cycle is then checked against the frozen evidence: mode 1, low bits 111, 2100 alarm, released,
 * soft-limit and undocumented bits clear, or the move ends OBSERVATION_SCOPE. Acceptance and
 * completion are recorded as their rule allows, and only in cycles that began after the trigger's
 * acknowledgement. Successful SDO, an old completion, a completion before acceptance and a cycle
 * consumed at or after the cutoff never finish a move.
 * @param[in,out] context Active move.
 * @param[in,out] observation Borrowed read; advanced only in OBSERVING, when it must be the move's.
 * @param[in] live Actual reservation. @param[in] current Actual target and epochs.
 * @param[in] evidence Borrowed terminal; no overlap with outputs.
 * @param[in] nowUs Processing time, at least the completion and the last mutation. `now >= deadlineUs`
 * ends the move DEADLINE even if the reply was received earlier; nothing is backdated.
 * @return Local call status. Accepted service failures settle the move with their evidence. A trigger
 * acknowledged past the cutoff stays ACKNOWLEDGED, and the move ends DEADLINE and uncertain.
 */
Status advanceMove(MoveContext& context, ReadContext& observation, const AxisReservation& live,
                   const ReadTarget& current, const CANOPEN::SdoCompletion& evidence,
                   uint64_t nowUs) noexcept;
/** End a move locally after separately retiring any admitted work. Never a stop, rearm or replay.
 * @param[in,out] context Active move; retained evidence preserved.
 * @param[in,out] observation Borrowed read; an admitted active cycle in it is withdrawn too.
 * @param[in] live Actual reservation; INVALIDATED alone permits a changed one.
 * @param[in] current Actual target; checked except for INVALIDATED.
 * @param[in] reason CANCELLED, STOP, DRIVE_FAULT, INVALIDATED or DEADLINE; the cutoff wins at equality.
 * @param[in] nowUs Nondecreasing time.
 * @return Local status. Acknowledged evidence survives, and a possibly sent trigger leaves the move
 * uncertain: the drive may still be moving.
 */
Status advanceMove(MoveContext& context, ReadContext& observation, const AxisReservation& live,
                   const ReadTarget& current, MoveInterruption reason, uint64_t nowUs) noexcept;
} // namespace LICHUAN_C
} // namespace MotorControlCAN
