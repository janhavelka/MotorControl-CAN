/** @file Velocity.h
 * @brief Finite, explicitly qualified mode-3 velocity through the ordinary CAN owner.
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
    CLOSED,  ///< Default: no operation prepared.
    MEASURED ///< Caller retains a selected-firmware record establishing the stated facts.
};
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
    VelocityGate gate = VelocityGate::CLOSED;     ///< Closed until independently established.
    uint32_t record = 0;                          ///< Nonzero retained evidence identity.
    SettingsModel model = SettingsModel::UNKNOWN; ///< Explicit selected model.
    VelocityFirmware firmware; ///< Must match the supplied acquired identity tuple.
    StatusLayout layout =
        StatusLayout::UNSPECIFIED;      ///< Qualified status dialect; not chosen by language.
    bool startQualified = false;        ///< Released, target-before-enable sequence established.
    bool stopQualified = false;         ///< Selected stop behavior established in mode3.
    StopKind stopKind = StopKind::HALT; ///< The stop kind established by this record.
    bool modeChangeQualified =
        false; ///< Mode change while released established; enabled changes always reject.
    uint8_t modeChangeFromRaw =
        0; ///< Exact qualified released source mode (0, 1 or 6), never a wildcard.
    bool reverseQualified = false;  ///< Negative 60FF behavior independently established.
    uint32_t maximumForwardRpm = 0; ///< Qualified positive range, 1..3000 rpm.
    uint32_t maximumReverseRpm = 0; ///< Qualified magnitude if reverseQualified, at most 3000 rpm.
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
    uint32_t maxPolls = 0; ///< Bound on complete cycles, including mode/enable observations.
    uint64_t maximumAgeUs = 0; ///< Inclusive earliest-bound age cap for a seven-field cycle.
    uint64_t requiredStopBudgetUs =
        0; ///< Positive caller reserve after due, at least eight services.
    uint64_t commandIntervalUs =
        0; ///< At least 10000 us after each enable-word ACK before the next word.
    StopKind stopKind = StopKind::HALT; ///< Explicit independently qualified stop kind.
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
    uint32_t observations = 0;          ///< Complete cycles, bounded by maxPolls.
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
    ActionOutcome stopOutcome = ActionOutcome::NONE; ///< Independent stop outcome.
    CANOPEN::SdoCompletion
        stopEvidence; ///< Retained stop command terminal, never motion confirmation.
};

/** Atomically prepare a finite selected-firmware run. All references are borrowed for the call.
 * Non-enabled stationary admission, exact identity, integer rpm, nonzero rate, supported ramps,
 * duration coverage and fixed stop budget are checked before publication. No I/O occurs.
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
 * @param[in,out] context STOPPING operation. @param[in] stop Same route/kind/deadline, newer operation.
 * @param[in] nowUs Processing time. @return Local status; failed terminal is never rewritten. */
Status advanceVelocityStop(VelocityContext& context, const ActionContext& stop,
                           uint64_t nowUs) noexcept;
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
