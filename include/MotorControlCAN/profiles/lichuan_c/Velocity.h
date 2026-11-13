/** @file Velocity.h
 * @brief Finite mode-3 velocity through the ordinary CAN owner, under a measured record or an
 * explicit attended experiment scope.
 * No clocks, allocation, I/O, retries or implicit stops. All times are supplied monotonic
 * microseconds in 0..INT64_MAX; now >= deadline expires even for earlier received frames.
 * SPDX-License-Identifier: MIT
 */
#pragma once
#include <MotorControlCAN/VelocityOperation.h>
#include <MotorControlCAN/profiles/lichuan_c/Actions.h>
#include <MotorControlCAN/profiles/lichuan_c/DriverSettings.h>
#include <MotorControlCAN/profiles/lichuan_c/State.h>

namespace MotorControlCAN {
namespace LICHUAN_C {
/** Capability evidence is explicit; manuals and simulated handshakes cannot open this gate. */
enum class VelocityGate : uint8_t {
    CLOSED,   ///< Default: no operation prepared.
    MEASURED, ///< Caller retains a selected-firmware record establishing the stated facts.
    /** One attended experiment that measures the mode 3 start and stop instead of claiming them.
     * startQualified, stopQualified, modeChangeQualified and reverseQualified must all be false, so
     * the drive must already report released and stationary in mode 3. The rate is at most
     * VELOCITY_EXPERIMENT_MAXIMUM_RPM and the duration at most
     * VELOCITY_EXPERIMENT_MAXIMUM_DURATION_US; it is positive unless VelocityEvidence::reverseProbe is
     * set. Every other check is unchanged. The attendant's independent power cutoff is the control the
     * library cannot see. The context keeps this gate, so a run under it never reads as qualified, and a
     * successful terminal qualifies nothing. */
    ATTENDED_EXPERIMENT
};
/** Highest positive rate an ATTENDED_EXPERIMENT can state, in motor rpm. Project policy for an
 * unmeasured start and stop; it says nothing about the drive. */
constexpr uint32_t VELOCITY_EXPERIMENT_MAXIMUM_RPM = 60;
/** Longest preparation-anchored duration an ATTENDED_EXPERIMENT can request, in microseconds.
 * Project policy, as above; the setup budget must still fit inside it. */
constexpr uint64_t VELOCITY_EXPERIMENT_MAXIMUM_DURATION_US = 20000000;
/** Highest positive rate an ATTENDED_EXPERIMENT with rangeProbe can state, in motor rpm: twice the
 * documented 3000 rpm (EN86 p13), as project policy for the prompt 59 range probe. It says nothing about
 * what the drive or motor can reach. */
constexpr uint32_t VELOCITY_RANGE_PROBE_MAXIMUM_RPM = 6000;
/** Shortest 6083 and 6084 ramp a range probe admits, in ms. Project policy, so a probe step never
 * starts or stops at the drive's shortest documented ramp. */
constexpr uint32_t VELOCITY_RANGE_PROBE_MINIMUM_RAMP_MS = 200;
/** Over-speed allowance of every running cycle, in motor rpm: a running or enable cycle whose 606C
 * magnitude exceeds the command's by more than a fifth of it plus this value ends the run with its stop
 * requested. Project policy; 606C read in 2 rpm steps at cruise on both specimens. */
constexpr uint32_t VELOCITY_OVERSPEED_MARGIN_RPM = 10;
/** Largest 606C magnitude of the sign opposite to the command that a VelocityEvidence::reverseProbe run
 * tolerates, in motor rpm: from the enable report on, a cycle whose 606C opposes the command by more ends
 * the run with its stop requested, also before running was observed. Project policy; 606C read in 2 rpm
 * steps at cruise on both specimens, so one step either way is not read as a direction. */
constexpr uint32_t VELOCITY_REVERSE_PROBE_SIGN_MARGIN_RPM = 2;
/** Most extra final-enable cycles one enable report window can schedule: enableWindowUs divided by
 * enableSpacingUs, rounded down, may not exceed it. Project policy bounding the observation count and
 * the setup budget; it says nothing about the drive. */
constexpr uint32_t VELOCITY_ENABLE_WINDOW_MAXIMUM_CYCLES = 16;
/** Exact raw identity tuple, not model autodetection or inferred firmware compatibility. */
struct VelocityFirmware {
    uint32_t deviceType = 0;      ///< 1000 raw.
    uint16_t hardwareVersion = 0; ///< 1009 raw.
    uint16_t softwareVersion = 0; ///< 100A raw.
    uint32_t vendorId = 0;        ///< 1018:01 raw.
    uint32_t productCode = 0;     ///< 1018:02 raw.
    uint32_t revision = 0;        ///< 1018:03 raw.
    uint16_t modelCode = 0;       ///< 2000 raw.
    uint16_t firmwareCode = 0;    ///< 2001 raw.
};
/** One record qualifies only its explicitly bounded rate/start/stop scope. The implemented
 * sequence is ZH86 PDF p13: mode3 observation, target/ramps, then 6/7/15. It never combines
 * EN86's conflicting enable-first sequence with this one. */
struct VelocityEvidence {
    VelocityGate gate = VelocityGate::CLOSED; ///< Closed until established, or an attended experiment.
    uint32_t record = 0; ///< Nonzero retained evidence identity; for an experiment, its own record.
    SettingsModel model = SettingsModel::UNKNOWN; ///< Explicit selected model.
    VelocityFirmware firmware; ///< Must match the supplied acquired identity tuple.
    StatusLayout layout =
        StatusLayout::UNSPECIFIED;      ///< Qualified status dialect; not chosen by language.
    bool startQualified = false; ///< Released, target-before-enable sequence established; MEASURED only.
    bool stopQualified = false;  ///< Selected stop behavior established in mode3; MEASURED only.
    StopKind stopKind = StopKind::HALT; ///< The stop kind established by this record.
    bool modeChangeQualified =
        false; ///< Mode change while released established; enabled changes always reject.
    uint8_t modeChangeFromRaw =
        0; ///< Exact qualified released source mode (0, 1 or 6), never a wildcard.
    /** Negative 60FF behavior independently established; MEASURED only, never by an experiment. */
    bool reverseQualified = false;
    /** ATTENDED_EXPERIMENT only: the prompt 59 range probe. It raises the experiment's rate cap from
     * VELOCITY_EXPERIMENT_MAXIMUM_RPM to VELOCITY_RANGE_PROBE_MAXIMUM_RPM and requires ramps of at
     * least VELOCITY_RANGE_PROBE_MINIMUM_RAMP_MS. A probe run qualifies nothing, as every experiment;
     * any other gate refuses it. */
    bool rangeProbe = false;
    /** ATTENDED_EXPERIMENT only: the prompt 59 reverse probe, the run that measures what a negative 60FF
     * does. No source states a direction rule: the EDS and the p17 dictionaries declare 60FF I32, while
     * the EN86/ZH86 p13 mode tables give it as UNSIGNED32 0~3000 (ledger conflict Q03, open). With it a
     * negative rate is admitted down to -maximumReverseRpm, written as its two's complement I32, with the
     * experiment's own magnitude cap (VELOCITY_EXPERIMENT_MAXIMUM_RPM, or VELOCITY_RANGE_PROBE_MAXIMUM_RPM
     * with rangeProbe); positive rates stay as without it, and zero stays refused. From the enable report
     * on, every cycle also checks 606C's sign (VELOCITY_REVERSE_PROBE_SIGN_MARGIN_RPM). A reverse probe run
     * qualifies nothing, so reverseQualified stays false: only a reviewed record, under MEASURED, sets that.
     * Any other gate refuses it. */
    bool reverseProbe = false;
    uint32_t maximumForwardRpm = 0; ///< Qualified positive range, 1..3000 rpm.
    /** Negative rate magnitude, in motor rpm: with reverseQualified the qualified range, 1..3000; with
     * reverseProbe the experiment's stated range, 1 to its cap; otherwise 0. */
    uint32_t maximumReverseRpm = 0;
};
/** Native ramps are milliseconds, not rpm/s. Zero semantics remain unsupported. */
struct VelocityProfile {
    uint32_t rampUpTimeMs = 0;   ///< Reviewed positive 6083 time, at most 2000 ms.
    uint32_t rampDownTimeMs = 0; ///< Reviewed positive 6084 time, at most 2000 ms.
};
/** Caller scheduling policy; no default is a manufacturer timeout or usable configuration. */
struct VelocityOptions {
    ReadOptions service; ///< Positive queue/response budgets for every transfer.
    uint64_t pollIntervalUs =
        0;                 ///< Maximum lateness after stop due; also delay between running cycles.
    /** Positive bound on interval-spaced running cycles; setup adds at most four, plus at most
     * enableWindowUs / enableSpacingUs final-enable cycles with a window. */
    uint32_t maxPolls = 0;
    uint64_t maximumAgeUs = 0; ///< Inclusive earliest-bound age cap for a seven-field cycle.
    uint64_t requiredStopBudgetUs =
        0; ///< Positive caller reserve after due, at least eight services.
    uint64_t commandIntervalUs =
        0; ///< At least 10000 us after each enable-word ACK before the next word.
    StopKind stopKind = StopKind::HALT; ///< Explicit independently qualified stop kind.
    /** Final-enable report window in microseconds after word 15's acknowledgement (backlog S17). 0, the
     * default, keeps the one-cycle rule: the first cycle after word 15 must report the enable. A nonzero
     * window is the application's bounded wait, never a device timing: on 2026-11-09 the first cycle
     * after word 15 read 6041 8033 in all 13 retained records of one firmware, and no record
     * establishes when the enable report follows in mode 3. So it is admitted only under
     * VelocityGate::ATTENDED_EXPERIMENT, the run that measures it; any other gate refuses it. While the
     * window lasts, a final-enable cycle that still shows word 7's report exactly (mode 3, clean, low bits
     * 011, 2100 released and running clear, 606C 0, 6064 word 7's cycle's) schedules another one
     * enableSpacingUs later, as long as that one is eligible at most enableWindowUs after the
     * acknowledgement and before stop due. Any other report fails as without a window, as does the last
     * such report the window admits; both request the run's own stop. The window adds at most
     * enableWindowUs to the setup time the duration must cover. The 6064 reference is word 7's cycle,
     * not the admission: word 7 pulled node 2's rotor 12 and 5 counts before any observation (trials
     * 1742, 1762); no bound on that pull is claimed. */
    uint64_t enableWindowUs = 0;
    /** Delay after a window cycle that still shows word 7's report before the next, in microseconds.
     * 0 without a window; with one, 1..enableWindowUs, and enableWindowUs / enableSpacingUs at most
     * VELOCITY_ENABLE_WINDOW_MAXIMUM_CYCLES, the most extra cycles the window can add. */
    uint64_t enableSpacingUs = 0;
};
/** Direct phases; there is one SDO engine and no internal motion scheduler. */
enum class VelocityPhase : uint8_t {
    MODE,       ///< Optional 6060=3, only from the qualified released start.
    PARAMETERS, ///< 60FF, 6083, 6084 exactly once each.
    ENABLE,     ///< Ordered 6,7,15; each followed by fresh observation.
    WAITING,    ///< A cycle becomes eligible at eligibleUs.
    OBSERVING,  ///< Borrowed ReadContext holds the current seven-field cycle.
    STOPPING    ///< Application composes its existing urgent stop; no new velocity work.
};
/** Why the current seven-field cycle is required. */
enum class VelocityObservation : uint8_t {
    MODE,   ///< Confirm mode3 while still released and stationary.
    ENABLE, ///< Confirm the current enable word before advancing.
    RUNNING ///< Fresh running evidence; never physical movement or standstill proof.
};
/** Owned summary of one complete sequential observation cycle. */
struct VelocitySample {
    bool present = false;    ///< Complete source blocks were checked.
    uint32_t cycle = 0;      ///< Nonwrapping cycle count; zero identifies admission.
    uint16_t statusword = 0; ///< Raw 6041, interpreted only with evidence.layout.
    uint16_t monitor = 0;    ///< Raw 2100; never merged into statusword.
    uint8_t modeRaw = 0;     ///< Raw 6061.
    int32_t position = 0;    ///< Raw 6064; no scale or home claim.
    int32_t velocity = 0;    ///< Signed 606C; used for direction and zero observations.
    uint64_t earliestUs = 0; ///< Earliest acquisition bound, not physical arrival.
    uint64_t latestUs = 0;   ///< Latest completion bound.
};
/** Owned request yielded for normal admission; ready is not permission after provenance changes. */
struct PreparedVelocity {
    CANOPEN::SdoTransfer transfer; ///< Complete copied SDO when ready.
    bool ready = false;            ///< One service is eligible.
    bool observe = false;          ///< Begin a new external ReadContext cycle.
    uint64_t eligibleUs = 0;       ///< Earliest eligible caller processing time.
};
/** Concrete terminal result and retained uncertain effects. Serialize access. Borrowed inputs live
 * only during calls; external ReadContext storage remains the caller's. No result getter is needed.
 * needsStop survives cancellation, failure and recovery while any enable word may have left.
 * Proved non-transmission can refine its provisional notice; otherwise only this operation's
 * checked stop settles it. A separate later operation cannot rewrite this terminal. */
struct VelocityContext {
    AxisReservation reservation;            ///< Frozen logical/wire/configuration/node identity.
    VelocityEvidence evidence;              ///< Selected-firmware capability record.
    FiniteVelocityRequest request;          ///< Immutable engineering intent and duration.
    VelocityProfile profile;                ///< Immutable native ramps.
    VelocityOptions options;                ///< Immutable service and scheduling bounds.
    AxisConfig axis;                        ///< Frozen conversion/configuration snapshot.
    PreparedVelocityTarget rate;            ///< Single exact preparation and quantization evidence.
    ActionState state = ActionState::EMPTY; ///< ACTIVE includes pending composed stop.
    VelocityPhase phase = VelocityPhase::PARAMETERS;             ///< Current direct phase.
    VelocityObservation observation = VelocityObservation::MODE; ///< Current/next cycle purpose.
    ActionOutcome outcome = ActionOutcome::NONE; ///< First failure or observed stop terminal.
    ActionOutcome firstFailure =
        ActionOutcome::NONE; ///< Preserved when a bounded stop still remains.
    VelocityInterruption interruption = VelocityInterruption::NONE; ///< Local terminal reason.
    VelocityInterruption withdrawal =
        VelocityInterruption::NONE; ///< In-flight withdrawal, never new work.
    ActionExecution setup = ActionExecution::NOT_TRANSMITTED;     ///< Parameter/setup aggregate.
    ActionExecution execution = ActionExecution::NOT_TRANSMITTED; ///< Final enable-word evidence.
    ActionCompletion completion =
        ActionCompletion::NOT_OBSERVED; ///< Checked stop observation only.
    Status status;                      ///< Local terminal or first-failure status.
    uint32_t operation = 0;             ///< Nonzero original operation identity.
    uint32_t step = 1;                  ///< Nonwrapping service identity.
    uint32_t observations = 0;          ///< All complete cycles; never wraps, including setup.
    uint32_t runningPolls = 0;          ///< Complete RUNNING cycles, bounded by maxPolls.
    uint8_t setupCursor = 0;            ///< 60FF/6083/6084 index, 0..2.
    uint8_t commandCursor = 0;          ///< 6/7/15 index, 0..2.
    uint8_t setupEvidenceMask = 0;      ///< Retained setupHistory slots.
    uint8_t commandEvidenceMask = 0;    ///< Retained commandHistory slots.
    uint64_t startedUs = 0;             ///< Preparation time; duration starts here.
    uint64_t servicedUs = 0;            ///< Last mutation time.
    uint64_t stopDueUs = 0;     ///< Fixed startedUs+durationUs; never restarted at admission.
    uint64_t deadlineUs = 0;    ///< Fixed inclusive processing cutoff.
    uint64_t eligibleUs = 0;    ///< Next service/cycle eligibility.
    uint64_t stepStartedUs = 0; ///< Earliest permitted service admission.
    uint64_t observationStartedUs = 0; ///< Exact current borrowed read preparation time.
    uint64_t stopRequestedUs = 0;      ///< Earliest matching composed stop preparation.
    bool transmissionNoted = false;    ///< Current download crossed possible-TX boundary.
    bool effectsPossible = false;      ///< A native setting/command may have reached the drive.
    bool uncertain = false;     ///< Unconfirmed writes, reads or possibly running motor remain.
    bool readUncertain = false; ///< Acquired read lifecycle had unresolved effects/correlation.
    bool needsStop =
        false; ///< A possibly enabling word needs independently checked stop settlement.
    bool stopRequested =
        false; ///< Caller must explicitly compose its stop in the remaining budget.
    bool runningObserved =
        false; ///< Fresh mode3/status/monitor/direction report, not physical motion.
    bool modeEvidencePresent = false;        ///< modeEvidence meaningful.
    bool observationEvidencePresent = false; ///< observationEvidence meaningful.
    CANOPEN::SdoCompletion modeEvidence;     ///< Optional mode write terminal.
    CANOPEN::SdoCompletion
        setupHistory[3]; ///< Actual 60FF/6083/6084 terminals, including partial failure.
    CANOPEN::SdoCompletion
        commandHistory[3]; ///< Actual 6/7/15 terminals, including partial failure.
    CANOPEN::SdoCompletion
        control; ///< Latest positively acknowledged controlword for stop composition.
    CANOPEN::SdoCompletion observationEvidence; ///< Latest actual read terminal, including failure.
    VelocitySample admission;                   ///< Fresh released admission evidence.
    VelocitySample latest;      ///< Latest complete observation retained across failure.
    uint32_t stopOperation = 0; ///< Accepted distinct stop operation identity, zero until settled.
    ActionExecution stopExecution =
        ActionExecution::NOT_TRANSMITTED;            ///< Independent stop command evidence.
    /** Independent stop outcome; with stopOperation zero, why the requested stop was not composed. */
    ActionOutcome stopOutcome = ActionOutcome::NONE;
    CANOPEN::SdoCompletion
        stopEvidence; ///< Retained stop command terminal, never motion confirmation.
    /** Final-enable cycles that still showed word 7's report while an enable report window was set,
     * including a last one that ended the run because the window admitted no further cycle: at most
     * enableWindowUs / enableSpacingUs + 1. Always 0 without a window. */
    uint32_t enablePending = 0;
    VelocitySample pendingFirst; ///< First such cycle, present when enablePending is nonzero.
    VelocitySample pendingLast;  ///< Latest such cycle, present when enablePending is nonzero.
    /** The cycle that first reported the final enable (6041 low bits 111, 2100 released clear), with or
     * without a window; absent while no cycle has. With the acknowledgement time of word 15
     * (commandHistory[2].completedUs) and pendingLast, it brackets the time to the enable report. */
    VelocitySample enableReport;
};

/** Atomically prepare a finite selected-firmware run. All references are borrowed for the call.
 * Non-enabled stationary admission, exact identity, integer rpm, nonzero rate, supported ramps,
 * duration coverage and fixed stop budget are checked before publication. No I/O occurs.
 * CLOSED, a MEASURED record without both start and stop facts, an ATTENDED_EXPERIMENT that
 * claims any qualification or exceeds its rate or duration bound, and a nonzero enable report window,
 * a range probe or a reverse probe under any gate but ATTENDED_EXPERIMENT are refused. A negative rate
 * needs reverseQualified (MEASURED) or reverseProbe (ATTENDED_EXPERIMENT) and a magnitude within
 * maximumReverseRpm. The admission's 2100 running bit must be clear,
 * except in mode 3 together with the release, 6041 low bits 000 and 606C zero: the latch a moving quick
 * stop left on both specimens (2100 0015, 6041 C050; reads 1514 and 1608, backlog S18). Word 6's cycle
 * must then repeat the admission's 6064, and from word 7's cycle on the latch must be clear.
 * @param[out] output Candidate unchanged on failure. @param[in] reservation Continuous exclusion.
 * @param[in] current Current target. @param[in] state Fresh complete state read.
 * @param[in] identity Acquired raw identity. @param[in] evidence Explicit firmware evidence.
 * @param[in] request Signed rate and preparation-time duration. @param[in] profile Native ramps.
 * @param[in] axis Exact conversion facts. @param[in] operation Nonzero identity.
 * @param[in] nowUs Preparation time. @param[in] deadlineUs Fixed cutoff.
 * @param[in] options Caller budgets. @return Local status, no work on refusal. */
Status prepareVelocity(VelocityContext& output, const AxisReservation& reservation,
                       const ReadTarget& current, const ReadContext& state,
                       const IdentityObservation& identity, const VelocityEvidence& evidence,
                       const FiniteVelocityRequest& request, const VelocityProfile& profile,
                       const AxisConfig& axis, uint32_t operation, uint64_t nowUs,
                       uint64_t deadlineUs, const VelocityOptions& options) noexcept;
/** Validate the same candidate without changing it, for owner admission before in-place publication.
 * @param[in] output Candidate address used to check overlap. @param[in] reservation Exclusion.
 * @param[in] current Target. @param[in] state Fresh read. @param[in] identity Acquired identity.
 * @param[in] evidence Firmware evidence. @param[in] request Rate and duration.
 * @param[in] profile Native ramps. @param[in] axis Conversion facts. @param[in] operation Identity.
 * @param[in] nowUs Preparation time. @param[in] deadlineUs Fixed cutoff. @param[in] options Budgets.
 * @return Exactly the preparation validation status; all inputs unchanged. */
Status validateVelocity(const VelocityContext& output, const AxisReservation& reservation,
                        const ReadTarget& current, const ReadContext& state,
                        const IdentityObservation& identity, const VelocityEvidence& evidence,
                        const FiniteVelocityRequest& request, const VelocityProfile& profile,
                        const AxisConfig& axis, uint32_t operation, uint64_t nowUs,
                        uint64_t deadlineUs, const VelocityOptions& options) noexcept;
/** Yield copied work atomically; never replay noted writes or yield motion at/after stopDueUs.
 * A fresh service needs its complete queue/response budget before stop due; every download
 * also carries stopDueUs as its service cutoff so queued start work cannot use the stop reserve.
 * @param[in] context Prepared operation. @param[in] observation Current external read.
 * @param[in] live Current reservation. @param[in] current Current target. @param[in] nowUs Time.
 * @param[out] output Unchanged on refusal. @return Local status; terminal yields no work. */
Status nextVelocity(const VelocityContext& context, const ReadContext& observation,
                    const AxisReservation& live, const ReadTarget& current, uint64_t nowUs,
                    PreparedVelocity& output) noexcept;
/** Begin an eligible external read, atomically with context. No service is admitted here.
 * @param[in,out] context Active operation. @param[out] observation Caller-owned read storage.
 * @param[in] live Current reservation. @param[in] current Current target. @param[in] nowUs Time.
 * @return Local status; both outputs unchanged on refusal. */
Status beginVelocityObservation(VelocityContext& context, ReadContext& observation,
                                const AxisReservation& live, const ReadTarget& current,
                                uint64_t nowUs) noexcept;
/** Check the frozen rate/configuration and live provenance immediately before admission.
 * Recheck at controller handover too: the complete service budget must still fit before stop due.
 * @param[in] context Active operation. @param[in] live Reservation. @param[in] current Target.
 * @param[in] axis Current exact conversion facts. @param[in] nowUs Time. @return Local status. */
Status validateVelocityAdmission(const VelocityContext& context, const AxisReservation& live,
                                 const ReadTarget& current, const AxisConfig& axis,
                                 uint64_t nowUs) noexcept;
/** Mark possible write transmission; retire retained MOTION_PARAMETERS after either ramp write.
 * Does not advance configuration generation or prove CAN/service acknowledgement.
 * @param[in,out] context Active operation. @param[in] live Reservation. @param[in] current Target.
 * @param[in] axis Current conversion facts. @param[in,out] retained Retained driver snapshots.
 * @param[in] count Number of pointers, null only when zero. @param[in] nowUs Time.
 * @return Local status, unchanged on validation failure. */
Status noteVelocityTransmission(VelocityContext& context, const AxisReservation& live,
                                const ReadTarget& current, const AxisConfig& axis,
                                DriverContext* const* retained, uint8_t count,
                                uint64_t nowUs) noexcept;
/** Consume one exactly correlated owner terminal; owns no receive loop or SDO engine.
 * @param[in,out] context Active operation. @param[in,out] observation Current external read.
 * @param[in] live Reservation. @param[in] current Target. @param[in] evidence Actual owner terminal.
 * @param[in] nowUs Processing time; equality with deadline expires. @return Local status. */
Status advanceVelocity(VelocityContext& context, ReadContext& observation,
                       const AxisReservation& live, const ReadTarget& current,
                       const CANOPEN::SdoCompletion& evidence, uint64_t nowUs) noexcept;
/** End locally without any implicit stop. Historical effects/needsStop survive.
 * @param[in,out] context Active operation. @param[in,out] observation Current read.
 * @param[in] live Reservation. @param[in] current Target. @param[in] reason Explicit withdrawal.
 * @param[in] nowUs Time. @return Local status; invalidation accepts changed live provenance. */
Status advanceVelocity(VelocityContext& context, ReadContext& observation,
                       const AxisReservation& live, const ReadTarget& current,
                       VelocityInterruption reason, uint64_t nowUs) noexcept;
/** Withdraw in-flight work; its terminal must still be consumed before local completion.
 * @param[in,out] context Active operation. @param[in] live Reservation. @param[in] current Target.
 * @param[in] reason Explicit withdrawal. @param[in] nowUs Time. @return Local status. */
Status noteVelocityWithdrawal(VelocityContext& context, const AxisReservation& live,
                              const ReadTarget& current, VelocityInterruption reason,
                              uint64_t nowUs) noexcept;
/** Service preparation-anchored duration only with no admitted transaction. At stop due requests
 * one composed explicit stop within the original deadline. Strictly later than due+pollInterval
 * retains SERVICE_TIMEOUT even if that stop succeeds. Local cancellation never enters this path.
 * @param[in,out] context Active operation. @param[in] live Reservation. @param[in] current Target.
 * @param[in] nowUs Supplied time. @param[in] transactionAdmitted True forbids this service call.
 * @param[out] stopDue Explicit stop requested; unchanged on refusal. @return Local status. */
Status serviceVelocity(VelocityContext& context, const AxisReservation& live,
                       const ReadTarget& current, uint64_t nowUs, bool transactionAdmitted,
                       bool& stopDue) noexcept;
/** Settle the distinct urgent stop, preserving the first velocity failure. No stop is generated.
 * Its observed completion settles needsStop only when the run's latest cycle before the stop request
 * (latest) reported the drive enabled and running in mode 3 with no alarm or release, the state an
 * ordinary mode 3 stop admission needs. Otherwise the drive was not reported running before the stop
 * either, so its not-running report shows nothing the stop did: the stop's own record keeps what it
 * saw, and this run keeps needsStop, completion NOT_OBSERVED and its first failure. After such a report
 * a QUICK_STOP also completes on a released standstill (backlog S18; ActionContext::standstillObserving).
 * @param[in,out] context STOPPING operation. @param[in] stop Same route/kind/deadline, newer operation.
 * @param[in] nowUs Processing time. @return Local status; failed terminal is never rewritten. */
Status advanceVelocityStop(VelocityContext& context, const ActionContext& stop,
                           uint64_t nowUs) noexcept;
/** Record that the application could not compose the stop this run requested. Nothing is sent: the
 * run ends FAILED with its first failure kept, stopOutcome set to reason, stopExecution NOT_TRANSMITTED
 * and needsStop and uncertainty kept. stopRequested with stopOperation zero and this stopOutcome is the
 * application's signal that no stop of this run left; it must stop the axis by other means.
 * @param[in,out] context STOPPING operation with no composed stop.
 * @param[in] reason INVALIDATED, or DEADLINE at or after the deadline; DEADLINE replaces INVALIDATED
 * at or after it. @param[in] nowUs Processing time. @return Local status; unchanged on refusal. */
Status advanceVelocityStop(VelocityContext& context, ActionOutcome reason, uint64_t nowUs) noexcept;
/** Check the stop that follows this run's own acknowledged final 000F, changing nothing. It is the
 * stop prepareStop would compose from that word, without the drive-state admission prepareStop needs:
 * a report after the 000F need not show its effect yet. On 2026-11-09 the first cycle after it, about
 * 25 ms after its acknowledgement, still read 6041 8033 in all 13 retained enable and velocity records
 * of one firmware, and in trial 1009 the drive was found running 37 s later. HALT emits 010F and
 * QUICK_STOP 000B; neither sets an enabling bit 000F did not already request, and neither is sent
 * after any other word. Its result is unqualified: advanceVelocityStop settles nothing with it unless
 * the run had reported running, and an application must not treat its observation as a checked stop.
 * The run is either ACTIVE in STOPPING with its stop requested and none composed, in which case kind
 * and deadlineUs must be the run's own, or ended (FAILED, or ACTIVE with a recorded withdrawal) while
 * needsStop remains, for a later explicit stop of the application's own kind and deadline.
 * @param[in] output Context prepareVelocityStop would write; read only for overlap checks.
 * @param[in] run This finite run. @param[in] live Current reservation. @param[in] current Target.
 * @param[in] control The application's actual latest acknowledged 6040 terminal; it must be the run's
 * own word 15 terminal, so no later word intervened. @param[in] kind Native stop selection.
 * @param[in] operation New operation identity, above the run's. @param[in] nowUs Preparation time.
 * @param[in] deadlineUs Exclusive cutoff. @param[in] options Stop budgets, as prepareStop's.
 * @return Exactly prepareVelocityStop's status; all inputs unchanged. */
Status validateVelocityStop(const ActionContext& output, const VelocityContext& run,
                            const AxisReservation& live, const ReadTarget& current,
                            const CANOPEN::SdoCompletion& control, StopKind kind,
                            uint32_t operation, uint64_t nowUs, uint64_t deadlineUs,
                            const ActionOptions& options) noexcept;
/** Prepare that stop in place as an ordinary mode 3 STOP ActionContext (predecessor 000F,
 * PROJECT_DECISION, not documented, no admission observation), for the same nextAction and
 * advanceAction path as prepareStop's. The run's own requested QUICK_STOP, when the run's latest cycle
 * reported running and options.maximumAgeUs covers seven fields, sets standstillObserving: trials 1513
 * and 1607 read 2100 0014, 6041 C050 28 ms after 000B, then 0015 with 606C 0 and 6064 still (backlog
 * S18). Any other stop prepared here keeps a release outside its scope. The run itself is not written.
 * @param[out] output Unchanged on refusal. @param[in] run As validateVelocityStop.
 * @param[in] live As validateVelocityStop. @param[in] current As validateVelocityStop.
 * @param[in] control As validateVelocityStop. @param[in] kind As validateVelocityStop.
 * @param[in] operation As validateVelocityStop. @param[in] nowUs As validateVelocityStop.
 * @param[in] deadlineUs As validateVelocityStop. @param[in] options As validateVelocityStop.
 * @return Local status; no I/O. */
Status prepareVelocityStop(ActionContext& output, const VelocityContext& run,
                           const AxisReservation& live, const ReadTarget& current,
                           const CANOPEN::SdoCompletion& control, StopKind kind, uint32_t operation,
                           uint64_t nowUs, uint64_t deadlineUs, const ActionOptions& options) noexcept;
/** Check a fresh report of exactly the halted state a settled velocity HALT leaves (backlog S18):
 * complete ordinary SDO STATUS and MONITORS reading 6041 9437, 2100 0001 (in place, not running, not
 * released, no alarm) and mode 3, each block beginning at or after afterUs. When the read's plan holds
 * FEEDBACK, it must be fresh too and 606C must read zero. Trials 1507, 1613 and 1706 left that state.
 * @param[in] state Read to judge. @param[in] current Target it must belong to.
 * @param[in] afterUs Earliest admissible field time, such as the caller's latest write.
 * @param[in] nowUs Judging time. @param[in] maximumAgeUs Freshness bound of every judged block.
 * @return OK for exactly that report; nothing is written. */
Status validateHaltedVelocityReport(const ReadContext& state, const ReadTarget& current, uint64_t afterUs,
                                    uint64_t nowUs, uint64_t maximumAgeUs) noexcept;
/** Check the attended experiment's quick stop of a drive a velocity run's own HALT left halted,
 * changing nothing (backlog S18). After a settled HALT both specimens stay enabled and halted in
 * mode 3 (6041 9437, 2100 0001: trials 1507, 1613 and 1706), and nothing else leaves that state
 * without a power cycle. No source or record says what 000B does there, so this one 000B is an
 * experiment: the run must have run under ATTENDED_EXPERIMENT and ended SUCCEEDED with its HALT
 * acknowledged and observed, its acknowledged 010F must still be the latest word, and a fresh report
 * after that word must be exactly the halted one (validateHaltedVelocityReport). The stop reads FEEDBACK from its
 * first cycle and completes only on a released standstill: two released cycles in a row with 6041
 * low bits 000, 606C zero and 6064 unchanged. A not-running report alone completes nothing, because
 * the drive was idle before the stop.
 * @param[in] output Context prepareHaltedVelocityQuickStop would write; read only for overlap checks.
 * @param[in] run The finished run. @param[in] live Current reservation, the run's.
 * @param[in] current Target. @param[in] state Fresh complete SDO MONITORS and STATUS after control.
 * @param[in] control The application's actual latest acknowledged 6040 terminal: the run's own 010F.
 * @param[in] operation New identity above the run's stop. @param[in] nowUs Preparation time.
 * @param[in] deadlineUs Exclusive cutoff. @param[in] options Stop budgets, as prepareStop's, with a
 * maximumAgeUs covering seven fields.
 * @return Exactly prepareHaltedVelocityQuickStop's status; all inputs unchanged. */
Status validateHaltedVelocityQuickStop(const ActionContext& output, const VelocityContext& run,
                                       const AxisReservation& live, const ReadTarget& current,
                                       const ReadContext& state, const CANOPEN::SdoCompletion& control,
                                       uint32_t operation, uint64_t nowUs, uint64_t deadlineUs,
                                       const ActionOptions& options) noexcept;
/** Prepare that stop in place as a mode 3 QUICK_STOP ActionContext (predecessor 010F,
 * PROJECT_DECISION, not documented, standstillObserving set, the admission report retained), for the
 * same nextAction and advanceAction path as prepareStop's. Its result is unqualified: an application
 * must not let it clear any fence. The run itself is not written.
 * @param[out] output Unchanged on refusal. @param[in] run As validateHaltedVelocityQuickStop.
 * @param[in] live As validateHaltedVelocityQuickStop. @param[in] current As validateHaltedVelocityQuickStop.
 * @param[in] state As validateHaltedVelocityQuickStop. @param[in] control As validateHaltedVelocityQuickStop.
 * @param[in] operation As validateHaltedVelocityQuickStop. @param[in] nowUs As validateHaltedVelocityQuickStop.
 * @param[in] deadlineUs As validateHaltedVelocityQuickStop.
 * @param[in] options As validateHaltedVelocityQuickStop.
 * @return Local status; no I/O. */
Status prepareHaltedVelocityQuickStop(ActionContext& output, const VelocityContext& run,
                                      const AxisReservation& live, const ReadTarget& current,
                                      const ReadContext& state, const CANOPEN::SdoCompletion& control,
                                      uint32_t operation, uint64_t nowUs, uint64_t deadlineUs,
                                      const ActionOptions& options) noexcept;
/** Retain an unconfirmed DEC-2 stop; it can never clear needsStop or prove completion.
 * @param[in,out] context STOPPING operation. @param[in] stop Distinct same-route stop.
 * @param[in] nowUs Processing time. @return Local status; no success can result. */
Status advanceVelocityStop(VelocityContext& context, const BestEffortStop& stop,
                           uint64_t nowUs) noexcept;
/** Clear only caller-owned unused storage; the application must first release admitted work.
 * @param[in,out] context Storage replaced by EMPTY, with no device effect. */
void resetVelocity(VelocityContext& context) noexcept;
} // namespace LICHUAN_C
} // namespace MotorControlCAN
