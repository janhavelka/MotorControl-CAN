/** @file Homing.h
 * @brief Conditional 86C switch homing through the ordinary sole CAN owner.
 * SPDX-License-Identifier: MIT
 * No I/O, clocks, allocation, retries, mode changes or implicit enable. All times are supplied
 * nonwrapping microseconds in 0..INT64_MAX. Processing at now >= deadlineUs expires. Parameter
 * writes are 6098, 6099:01/:02, 609A, 607C, then 6040=001F held until completion or explicit
 * interruption. The offset order and effect require exact selected-firmware evidence. A local
 * cancellation is not a motor stop. Contexts and external observation storage remain caller-owned.
 */
#pragma once
#include <MotorControlCAN/HomeOperation.h>
#include <MotorControlCAN/profiles/lichuan_c/Actions.h>
#include <MotorControlCAN/profiles/lichuan_c/IoSettings.h>
#include <MotorControlCAN/profiles/lichuan_c/State.h>

namespace MotorControlCAN {
namespace LICHUAN_C {
/** Lichuan-local method identifiers; no shared numeric method vocabulary is implied. */
enum class HomingMethod : uint8_t {
    NEGATIVE_LIMIT = 17,  ///< Negative limit search.
    POSITIVE_LIMIT = 18,  ///< Positive limit search.
    POSITIVE_ORIGIN = 24, ///< Positive origin search.
    NEGATIVE_ORIGIN = 29  ///< Negative search; completion on leaving origin during positive creep.
};
/** Capability gate; simulated handshakes and documentary ranges do not open it. */
enum class HomeGate : uint8_t {
    CLOSED,  ///< Default: admission refuses without changing output.
    MEASURED ///< Caller retains the exact specimen/firmware/method experiment record.
};
/** Exact raw acquired identity, independent of model selection. */
struct HomeFirmware {
    uint32_t deviceType = 0;      ///< 1000 raw.
    uint16_t hardwareVersion = 0; ///< 1009 raw.
    uint16_t softwareVersion = 0; ///< 100A raw.
    uint32_t vendorId = 0;        ///< 1018:01 raw.
    uint32_t productCode = 0;     ///< 1018:02 raw.
    uint32_t revision = 0;        ///< 1018:03 raw.
    uint16_t modelCode = 0;       ///< 2000 raw.
    uint16_t firmwareCode = 0;    ///< 2001 raw.
};
/** Exact qualified scope. None of these facts is inferred from a successful read or write.
 * The record establishes the method's direction under raw 2200, actual wiring and input polarity,
 * positive search/creep rpm, ramp time, offset ordering/sign/units/effect, bit8 completion sequence
 * and the final 6064-to-command relation. Changed native values require a different record.
 */
struct HomeEvidence {
    HomeGate gate = HomeGate::CLOSED;             ///< Explicit capability gate.
    uint32_t record = 0;                          ///< Nonzero application evidence record identity.
    SettingsModel model = SettingsModel::UNKNOWN; ///< Explicit CL86_C or OL86_C.
    HomeFirmware firmware;                        ///< Must match the acquired tuple.
    ReadTarget specimen; ///< Exact specimen binding/node/configuration for this qualification.
    HomingMethod method = HomingMethod::NEGATIVE_LIMIT; ///< One qualified method.
    StatusLayout layout = StatusLayout::UNSPECIFIED;    ///< Selected documentary alarm dialect.
    uint32_t searchSpeedRpm = 0;     ///< Exact qualified positive 6099:01, at most 3000.
    uint32_t creepSpeedRpm = 0;      ///< Exact qualified positive 6099:02, at most 3000.
    uint32_t rampTimeMs = 0;         ///< Exact qualified positive 609A, at most 2000.
    int32_t offsetNative = 0;        ///< Exact qualified signed 607C, -1000000..1000000.
    uint16_t directionRaw = 0;       ///< Exact qualified 2200; no inferred physical sign.
    uint16_t inputPolarityRaw = 0;   ///< Exact measured 2400:01 baseline; never applied twice.
    uint16_t inputFunctions[7] = {}; ///< Exact acquired 2400:02..08 assignment baseline.
    uint8_t wiredInputs = 0; ///< DI0..6 with independently established physical prerequisites.
    bool methodQualified =
        false; ///< Exact trajectory, direction and completion sequence qualified.
    /** Exact rates/ramp/offset and write order qualified, including that all five setup writes
     * leave the current command-coordinate relation unchanged until the trigger. A firmware that
     * applies 607C immediately cannot use this lifecycle or retain its pre-trigger reference. */
    bool parametersQualified = false;
    bool inputsQualified = false; ///< Logical input interpretation, polarity and wiring qualified.
    bool startOnActiveSwitchQualified =
        false; ///< Explicitly permits an active required input at start.
    bool originLimitEncounterQualified =
        false;                       ///< Qualified limit encounter during an origin search.
    bool referenceQualified = false; ///< Final feedback relation below independently established.
    int32_t finalFeedbackNative = 0; ///< Exact expected 6064 at qualified completion.
    int64_t referenceNative = 0;     ///< Actual command coordinate corresponding to that feedback.
};
/** Engineering rates are converted once to checked integer motor rpm before any work. Both the
 * request and resulting native rpm must be positive. Standard signed MOTOR/LOAD conversion keeps
 * commandPolarity: with reversed coordinates, use explicit positive NATIVE motor rpm. Negative
 * engineering rates cannot cancel that polarity to select a homing direction; method/2200 owns it. */
struct HomeRequest {
    HomingMethod method = HomingMethod::NEGATIVE_LIMIT; ///< Profile-specific method.
    VelocityRequest searchSpeed; ///< Positive engineering rate; nonzero rounding to zero refuses.
    VelocityRequest creepSpeed;  ///< Positive engineering creep rate.
    uint32_t rampTimeMs = 0;     ///< Native milliseconds, 1..2000, not acceleration.
    int32_t offsetNative = 0;    ///< Native offset; meaning remains exact-record gated.
};
/** Caller policy; defaults are deliberately unusable and never vendor timing defaults. */
struct HomeOptions {
    ReadOptions service;         ///< Positive queue and response budgets.
    uint64_t pollIntervalUs = 0; ///< Positive delay between complete observation cycles.
    uint32_t maxPolls =
        0; ///< At least two complete cycles for fresh low/high, no counter wrapping.
    uint64_t maximumAgeUs = 0; ///< Positive inclusive age cap from earliest acquisition bounds.
};
/** Explicit lifecycle; no position-style rearm interrupts an active home. */
enum class HomePhase : uint8_t {
    PARAMETERS, ///< Five operation-owned downloads, in documented/evidence-gated order.
    TRIGGER,    ///< One 001F rising edge from the reserved acknowledged 000F.
    WAITING,    ///< Next seven-field cycle eligible at eligibleUs.
    OBSERVING   ///< Caller-owned ReadContext contains that cycle.
};
/** One acquired sequential MONITORS+STATUS+FEEDBACK cycle, not an atomic snapshot. */
struct HomeSample {
    bool present = false;       ///< Complete checked sample available.
    uint32_t cycle = 0;         ///< Nonwrapping observation identity; admission is zero.
    uint16_t monitor = 0;       ///< Raw 2100.
    uint16_t logicalInputs = 0; ///< Raw 2101 logical DI levels; no second polarity transform.
    uint16_t statusword = 0;    ///< Raw 6041; mode6 completion is bit8, not generic CiA402 bits.
    uint8_t modeRaw = 0;        ///< Raw 6061.
    int32_t position = 0;       ///< Raw 6064.
    int32_t velocity = 0;       ///< Raw 606C; zero is a controller report only.
    uint64_t earliestUs = 0;    ///< Earliest acquisition bound.
    uint64_t latestUs = 0;      ///< Latest completion bound.
};
/** Work copied for the ordinary owner; not renewed admission permission. */
struct PreparedHome {
    CANOPEN::SdoTransfer transfer; ///< Complete service when ready.
    bool ready = false;            ///< One eligible copied service.
    bool observe = false;          ///< Begin a fresh external state read.
    bool prepareTrigger = false;   ///< Withdraw old coordinates before trigger service admission.
    uint64_t eligibleUs = 0;       ///< Earliest eligible processing time.
};
/** Concrete caller-owned operation; inputs are borrowed only during calls. Keep storage outside
 * small task stacks. One expected coordinate/configuration advance occurs before a possible
 * trigger, even if later transmission fails. No later stop, recovery or stale completion can turn
 * a failed home into a successful one. The reference getter never sets a host origin.
 */
struct HomeContext {
    AxisReservation reservation; ///< Frozen uninterrupted owner exclusion and original target.
    HomeEvidence evidence;       ///< Owned exact qualification.
    HomeRequest request;         ///< Owned intent.
    HomeOptions options;         ///< Fixed service and polling budgets.
    AxisConfig axis;             ///< Frozen original conversion/configuration facts.
    PreparedVelocityTarget searchSpeed;          ///< Single checked search conversion.
    PreparedVelocityTarget creepSpeed;           ///< Single checked creep conversion.
    ActionState state = ActionState::EMPTY;      ///< Current state; terminal is retained.
    HomePhase phase = HomePhase::PARAMETERS;     ///< Current phase.
    ActionOutcome outcome = ActionOutcome::NONE; ///< First operation terminal.
    ActionExecution setup = ActionExecution::NOT_TRANSMITTED;     ///< Aggregate parameter evidence.
    ActionExecution execution = ActionExecution::NOT_TRANSMITTED; ///< Trigger evidence only.
    ActionCompletion completion = ActionCompletion::NOT_OBSERVED; ///< Fresh qualified completion.
    HomeInterruption interruption = HomeInterruption::NONE;       ///< Local termination reason.
    HomeInterruption withdrawal = HomeInterruption::NONE; ///< In-flight work must settle first.
    Status status;                     ///< Terminal local status, distinct from call result.
    uint32_t operation = 0;            ///< Nonzero operation identity.
    uint32_t step = 1;                 ///< Nonwrapping service identity.
    uint32_t observations = 0;         ///< Complete observed cycles, bounded by maxPolls.
    uint8_t setupCursor = 0;           ///< Current parameter, 0..4.
    uint8_t setupEvidenceMask = 0;     ///< Actual setupHistory terminals.
    uint64_t startedUs = 0;            ///< Preparation time.
    uint64_t servicedUs = 0;           ///< Last processing time.
    uint64_t deadlineUs = 0;           ///< Immutable inclusive processing cutoff.
    uint64_t eligibleUs = 0;           ///< Next eligible cycle/service.
    uint64_t stepStartedUs = 0;        ///< Earliest current-service admission.
    uint64_t observationStartedUs = 0; ///< Current external-read preparation time.
    uint64_t prerequisiteUs = 0; ///< Earliest state, input mapping and direction acquisition bound.
    uint64_t withdrawalUs = 0;   ///< First explicit in-flight withdrawal time, zero when absent.
    bool transmissionNoted = false;      ///< Current write crossed possible transmission boundary.
    bool coordinatesInvalidated = false; ///< Exactly one COORDINATES withdrawal performed.
    bool effectsPossible = false;        ///< Any setup/trigger effect may have occurred.
    bool uncertain = false;              ///< Retained write/read/motion uncertainty.
    bool readUncertain = false;          ///< Unresolved read service evidence.
    bool needsStop = false;              ///< Trigger possibly sent without checked stop/completion.
    bool triggerEvidencePresent = false; ///< trigger is an actual terminal.
    bool observationEvidencePresent = false; ///< observationEvidence meaningful.
    bool requiredInputObserved = false;      ///< Method switch was active in a post-trigger cycle.
    HomeSample admission;                    ///< Fresh enabled stationary mode6 prerequisite.
    HomeSample latest;                       ///< Latest complete sample, including failure scope.
    HomeSample absence;                      ///< First post-trigger bit8-clear cycle.
    HomeSample activity;                     ///< First post-trigger controller-running cycle.
    HomeSample completed; ///< Qualified completion cycle, immutable after terminal.
    CANOPEN::SdoCompletion setupHistory[5]; ///< Real parameter terminals in order.
    CANOPEN::SdoCompletion trigger;         ///< Actual 001F terminal, including failure.
    CANOPEN::SdoCompletion
        control; ///< Latest acknowledged controlword for explicit stop composition.
    CANOPEN::SdoCompletion observationEvidence; ///< Latest actual read terminal.
    uint32_t stopOperation = 0; ///< Separately settled stop identity, zero until present.
    ActionOutcome stopOutcome = ActionOutcome::NONE; ///< Independent first stop failure/success.
    Status stopStatus; ///< Independent exact stop status; never overwrites the first home failure.
    ActionExecution stopExecution =
        ActionExecution::NOT_TRANSMITTED; ///< Independent stop execution.
    CANOPEN::SdoCompletion stopEvidence;  ///< Retained stop command terminal.
};

/** Validate atomically; no writes, mode change or enable are admitted on refusal.
 * @param[in] output Address checked for overlap. @param[in] live Continuous reservation.
 * @param[in] current Current target. @param[in] state Fresh ordinary state read.
 * @param[in] control Latest acknowledged 000F, before state and under live reservation.
 * @param[in] identity Acquired exact firmware identity. @param[in] inputs Reviewed 2400 acquisition.
 * @param[in] driver Fresh 2200 acquisition. @param[in] evidence Exact capability qualification.
 * @param[in] request Explicit intent. @param[in] axis Frozen host conversion facts.
 * @param[in] operation Nonzero identity. @param[in] nowUs Processing time.
 * @param[in] deadlineUs Fixed cutoff. @param[in] options Caller budgets.
 * @return Local status; no input/output changes. */
Status validateHome(const HomeContext& output, const AxisReservation& live,
                    const ReadTarget& current, const ReadContext& state,
                    const CANOPEN::SdoCompletion& control, const IdentityObservation& identity,
                    const DriverContext& inputs, const DriverContext& driver,
                    const HomeEvidence& evidence, const HomeRequest& request,
                    const AxisConfig& axis, uint32_t operation, uint64_t nowUs, uint64_t deadlineUs,
                    const HomeOptions& options) noexcept;
/** Prepare the fully checked operation in place; output stays unchanged on failure.
 * @param[out] output Caller storage. @param[in] live Continuous reservation.
 * @param[in] current Current target. @param[in] state Fresh ordinary state read.
 * @param[in] control Latest acknowledged 000F. @param[in] identity Exact acquired identity.
 * @param[in] inputs Reviewed 2400 acquisition. @param[in] driver Fresh 2200 acquisition.
 * @param[in] evidence Exact capability qualification. @param[in] request Explicit intent.
 * @param[in] axis Host conversion facts. @param[in] operation Nonzero identity.
 * @param[in] nowUs Processing time. @param[in] deadlineUs Fixed cutoff.
 * @param[in] options Caller budgets. @return Local status; OK is not execution. */
Status prepareHome(HomeContext& output, const AxisReservation& live, const ReadTarget& current,
                   const ReadContext& state, const CANOPEN::SdoCompletion& control,
                   const IdentityObservation& identity, const DriverContext& inputs,
                   const DriverContext& driver, const HomeEvidence& evidence,
                   const HomeRequest& request, const AxisConfig& axis, uint32_t operation,
                   uint64_t nowUs, uint64_t deadlineUs, const HomeOptions& options) noexcept;
/** Yield one copied service or wait; output unchanged on refusal.
 * @param[in] context Prepared home. @param[in] observation External read.
 * @param[in] live Reservation. @param[in] current Current target including expected advance.
 * @param[in] nowUs Time. @param[out] output Copied work. @return Local status. */
Status nextHome(const HomeContext& context, const ReadContext& observation,
                const AxisReservation& live, const ReadTarget& current, uint64_t nowUs,
                PreparedHome& output) noexcept;
/** Begin one complete seven-field cycle; neither output changes on refusal.
 * @param[in,out] context Active home. @param[out] observation External storage.
 * @param[in] live Reservation. @param[in] current Target. @param[in] nowUs Time.
 * @return Local status; no service is admitted here. */
Status beginHomeObservation(HomeContext& context, ReadContext& observation,
                            const AxisReservation& live, const ReadTarget& current,
                            uint64_t nowUs) noexcept;
/** Withdraw coordinates once before trigger admission. The owner then advances its configuration
 * to current and requests nextHome again; that trigger and its real acknowledgement carry the new
 * binding. Even failed admission cannot restore the old coordinate witness. This is a conservative
 * host withdrawal, not evidence that a command was transmitted.
 * @param[in,out] context Home awaiting trigger. @param[in] live Original reservation.
 * @param[in,out] current Current target, advanced once. @param[in,out] axis Current coordinate epochs.
 * @param[in,out] reference Old witness cleared. @param[in] nowUs Time.
 * @return Local status; outputs unchanged on refusal. */
Status beginHomeTrigger(HomeContext& context, const AxisReservation& live, ReadTarget& current,
                        AxisConfig& axis, AxisReference& reference, uint64_t nowUs) noexcept;
/** Recheck immediately before download admission and controller handover.
 * @param[in] context Active home. @param[in] live Reservation. @param[in] current Target.
 * @param[in] axis Current host configuration. @param[in] nowUs Time. @return Local status. */
Status validateHomeAdmission(const HomeContext& context, const AxisReservation& live,
                             const ReadTarget& current, const AxisConfig& axis,
                             uint64_t nowUs) noexcept;
/** Note possible transmission; beginHomeTrigger must already have withdrawn coordinates.
 * Parameter writes retire all supplied MOTION_PARAMETERS observations without advancing epochs.
 * @param[in,out] context Home. @param[in] live Original reservation.
 * @param[in,out] current Current target; unchanged here.
 * @param[in,out] axis Axis; unchanged here.
 * @param[in,out] reference Current witness; unchanged here.
 * @param[in,out] retained Retained driver snapshots, null only with count zero.
 * @param[in] count Pointer count. @param[in] nowUs Time.
 * @return Local status; full validation precedes every mutation. */
Status noteHomeTransmission(HomeContext& context, const AxisReservation& live, ReadTarget& current,
                            AxisConfig& axis, AxisReference& reference,
                            DriverContext* const* retained, uint8_t count, uint64_t nowUs) noexcept;
/** Consume exactly correlated owner evidence; late processing fails even for earlier received data.
 * @param[in,out] context Home. @param[in,out] observation External read.
 * @param[in] live Reservation. @param[in] current Target. @param[in] evidence Owner terminal.
 * @param[in] nowUs Processing time. @return Local status; terminal outcome remains separate. */
Status advanceHome(HomeContext& context, ReadContext& observation, const AxisReservation& live,
                   const ReadTarget& current, const CANOPEN::SdoCompletion& evidence,
                   uint64_t nowUs) noexcept;
/** End locally after admitted work is retired; never sends a motor stop.
 * @param[in,out] context Home. @param[in,out] observation External read.
 * @param[in] live Reservation. @param[in] current Target. @param[in] reason Explicit interruption.
 * @param[in] nowUs Time. @return Local status; possible effects and needsStop survive. */
Status advanceHome(HomeContext& context, ReadContext& observation, const AxisReservation& live,
                   const ReadTarget& current, HomeInterruption reason, uint64_t nowUs) noexcept;
/** Withdraw in-flight work; its terminal still must be consumed before any other operation.
 * @param[in,out] context Home. @param[in] live Reservation. @param[in] current Target.
 * @param[in] reason Explicit interruption. @param[in] nowUs Time. @return Local status. */
Status noteHomeWithdrawal(HomeContext& context, const AxisReservation& live,
                          const ReadTarget& current, HomeInterruption reason,
                          uint64_t nowUs) noexcept;
/** Retain separate mode6 HALT result; it never establishes home completion or repairs failure.
 * @param[in,out] context Failed/interrupted home. @param[in] stop Distinct explicit stop.
 * @param[in] nowUs Time. @return Local status; first stop terminal is immutable. */
Status advanceHomeStop(HomeContext& context, const ActionContext& stop, uint64_t nowUs) noexcept;
/** Retain unconfirmed urgent DEC-2 stop without clearing needsStop or reference uncertainty.
 * @param[in,out] context Failed/interrupted home. @param[in] stop Distinct explicit stop.
 * @param[in] nowUs Time. @return Local status; first stop terminal is immutable. */
Status advanceHomeStop(HomeContext& context, const BestEffortStop& stop, uint64_t nowUs) noexcept;
/** Publish qualified actual command coordinates, never a host-origin change or physical guarantee.
 * @param[in] context Successful fresh home. @param[in] current Actual current target.
 * @param[in] axis Actual current axis epochs. @param[in] nowUs Time.
 * @param[in] maximumAgeUs Positive inclusive age cap from completion's earliest bound.
 * @param[out] output Witness unchanged on failure; no overlap with inputs.
 * @return Local status; later invalidation makes a retained successful home unusable. */
Status getHomeReference(const HomeContext& context, const ReadTarget& current,
                        const AxisConfig& axis, uint64_t nowUs, uint64_t maximumAgeUs,
                        AxisReference& output) noexcept;
/** Clear unused storage only after the owner retires admitted work.
 * @param[in,out] context Replaced by EMPTY without device effects. */
void resetHome(HomeContext& context) noexcept;
} // namespace LICHUAN_C
} // namespace MotorControlCAN
