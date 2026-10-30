/** @file Settings.h
 * @brief Finite explicit 86C settings transactions, without controller I/O or clocks.
 * SPDX-License-Identifier: MIT
 * Applications supply actual retained service evidence and continuous exclusion
 * from the sole owner. No controlword/mode/save/reset preamble is emitted.
 * DI changes can immediately affect external enable/stop/limit controls.
 */
#pragma once
#include <MotorControlCAN/profiles/lichuan_c/IoSettings.h>

namespace MotorControlCAN {
namespace LICHUAN_C {
/** Caller-established model identity; no inference from raw identity codes. */
enum class SettingsModel : uint8_t {
    UNKNOWN, ///< No applicable model established.
    CL86_C,  ///< EN86/ZH86 documented closed-loop 86C scope; firmware qualification separate.
    OL86_C   ///< EN86/ZH86 documented open-loop 86C scope; firmware qualification separate.
};
/** Application-issued continuous exclusive sequence reservation, never a wire token.
 * The sole owner issues this before prerequisite acquisition, prevents competing
 * control/mode/target/RPDO/settings work, and revokes it on contradictory state,
 * reboot/rebind/recovery or uncertainty. Pass the actual live reservation on
 * every call; copying an expired token does not establish exclusion.
 */
struct SettingsReservation {
    ReadTarget target; ///< Frozen pre-transaction service route, binding/config/node epochs.
    SettingsModel model = SettingsModel::UNKNOWN; ///< Actual application/nameplate model binding.
    uint32_t owner = 0;                           ///< Nonzero stable local owner identity.
    uint32_t sequence = 0;     ///< Nonzero exclusive sequence identity, not reused with live work.
    Generation generation = 0; ///< Nonzero owner control epoch; never wrap/reuse live tokens.
    uint64_t acquiredUs = 0;   ///< Supplied acquisition time, zero valid.
};
/** Native position-mode parameters; native times are not acceleration rates. Defaults are invalid until explicitly set. */
struct DriverSettings {
    uint32_t positionSpeedRpm = UINT32_MAX; ///< Explicit requested 6081, inclusive 0..3000 rpm.
    uint32_t rampUpTimeMs = UINT32_MAX;     ///< Explicit requested 6083, inclusive 0..2000 ms.
    uint32_t rampDownTimeMs = UINT32_MAX;   ///< Explicit requested 6084, inclusive 0..2000 ms.
};
/** Complete explicit DI register candidate; no DO, filter or electrical-level request. */
struct IoSettings {
    uint16_t invertedInputs =
        0xffff; ///< Invalid until explicitly set to bits0..6; high bits come from prior readback.
    InputFunction functions[7] = {InputFunction::UNKNOWN, InputFunction::UNKNOWN,
                                  InputFunction::UNKNOWN, InputFunction::UNKNOWN,
                                  InputFunction::UNKNOWN, InputFunction::UNKNOWN,
                                  InputFunction::UNKNOWN};
    ///< DI0..6: explicitly select all codes0..13; defaults cannot disable protections.
};
/** Concrete bounded settings phase. */
enum class SettingsState : uint8_t {
    EMPTY,        ///< Not prepared.
    WRITING,      ///< Changed fields await individual acknowledgements.
    READING_BACK, ///< All writes acknowledged; full typed group is read at the new config epoch.
    SUCCEEDED,    ///< Requested register image matched; no electrical/physical activation claim.
    FAILED        ///< Retained failure, partial acknowledgements and possible effects survive.
};
/** Caller-owned finite driver/DI transaction; do not edit fields while active.
 * Masks use the fixed read group's field positions. Confirmed values are in
 * requested[]; the last exact terminal/abort is in evidence. No heap/hidden retry.
 */
struct DriverSettingsContext {
    SettingsReservation
        reservation;        ///< Copied continuous reservation and original transport epoch.
    DriverContext readback; ///< Fixed group plan and later new-generation readback/provenance.
    SettingsState state = SettingsState::EMPTY; ///< Current explicit phase.
    ReadOutcome outcome = ReadOutcome::NONE; ///< Terminal cause, independent of call status/health.
    Status status;                           ///< Retained local/native operation result.
    uint32_t operation = 0;                  ///< Nonzero caller operation/request identity.
    uint32_t requested[9] =
        {}; ///< Owned exact native candidate, count fields retained but never written.
    uint16_t changed =
        0; ///< Selected changed fields only; source-order writes, not device atomicity.
    uint16_t confirmed = 0; ///< Individually acknowledged write fields, retained on later failure.
    uint64_t acknowledgedUs[9] = {}; ///< Per-confirmed-field processing time; zero valid.
    uint8_t cursor = 0;              ///< Current changed field; bounded by selected group's count.
    uint64_t startedUs = 0;          ///< Supplied preparation time.
    uint64_t deadlineUs = 0;         ///< Exclusive immutable overall deadline, never renewed.
    uint64_t servicedUs = 0;         ///< Last local progression time.
    uint64_t stepStartedUs =
        0; ///< Earliest admission time for the current step, before possible-TX notice.
    bool transmissionNoted = false; ///< Current download crossed the owner's possible-TX boundary.
    bool effectsPossible =
        false;              ///< A download may have transmitted; configuration epoch withdrawn.
    bool uncertain = false; ///< Unconfirmed possible effects or unqualified read effects retained.
    CANOPEN::SdoCompletion evidence; ///< Last copied sole-owner terminal, including full raw abort.
    bool evidencePresent = false;    ///< Makes evidence meaningful.
};
/** Prepare explicit position parameters using actual retained pre-trigger evidence.
 * @param[out] output Owned context, unchanged on rejection; no input overlap.
 * @param[in] reservation Actual live sole-owner exclusion acquired before all prerequisites.
 * @param[in] prior Complete fresh MOTION_PARAMETERS context on reservation.target.
 * @param[in] state Complete checked STATUS and MONITORS read context after control completion.
 * @param[in] control Actual successful acknowledged 6040:00 U16 download 0x000F under this reservation.
 * @param[in] settings Complete explicit native candidate; validation precedes any yielded work.
 * @param[in] operation Nonzero unique live operation identity.
 * @param[in] nowUs Supplied nonwrapping microseconds, zero valid.
 * @param[in] deadlineUs Exclusive overall cutoff strictly after nowUs.
 * @param[in] options Nonzero service budgets, clamped by overall deadline.
 * @param[in] maximumAgeUs Nonzero inclusive earliest-observation age cap for all prerequisites.
 * @return Local status. Requires mode1, status low bits7, monitor running/alarm/disabled bits clear.
 * This conservative host subset never writes 6040/6060 or relies on disputed status bits.
 */
Status prepareDriverSettings(DriverSettingsContext& output, const SettingsReservation& reservation,
                             const DriverContext& prior, const ReadContext& state,
                             const CANOPEN::SdoCompletion& control, const DriverSettings& settings,
                             uint32_t operation, uint64_t nowUs, uint64_t deadlineUs,
                             const ReadOptions& options, uint64_t maximumAgeUs) noexcept;
/** Prepare complete explicit DI assignment/polarity, preserving unknown polarity bits.
 * @param[out] output Owned context, unchanged on rejection; no input overlap.
 * @param[in] reservation Actual live owner exclusion, CL86_C or OL86_C only.
 * @param[in] prior Fresh complete IO_INPUT_CONFIGURATION acquired under that reservation.
 * @param[in] settings Complete explicit codes/bits; no invented uniqueness or drive-mode rule.
 * @param[in] operation Nonzero unique live operation identity.
 * @param[in] nowUs Supplied nonwrapping microseconds.
 * @param[in] deadlineUs Exclusive immutable cutoff after nowUs.
 * @param[in] options Nonzero service budgets.
 * @param[in] maximumAgeUs Nonzero inclusive earliest-observation age cap.
 * @return Local status. An unchanged candidate succeeds using fresh prior evidence without TX/epoch change.
 * Writes only changed fields in ascending subindex order;
 * intermediate effects are possible, not an atomic or electrically safe transition.
 */
Status prepareIoSettings(DriverSettingsContext& output, const SettingsReservation& reservation,
                         const DriverContext& prior, const IoSettings& settings, uint32_t operation,
                         uint64_t nowUs, uint64_t deadlineUs, const ReadOptions& options,
                         uint64_t maximumAgeUs) noexcept;
/** Yield owned work without admitting or transmitting it.
 * @param[in] context Prepared transaction. @param[in] live Actual current reservation, not stale copy.
 * @param[in] current Actual application binding; old config epoch before possible TX, next epoch afterwards.
 * @param[in] nowUs Nondecreasing supplied time before active deadline.
 * @param[out] output Unchanged on failure; terminal calls set ready=false only; no input overlap.
 * @return Local status. Readback requests use the new config epoch; owner must update its
 * idle binding after releasing old write results, without resetting or retrying the device.
 */
Status nextDriver(const DriverSettingsContext& context, const SettingsReservation& live,
                  const Binding& current, uint64_t nowUs, PreparedRead& output) noexcept;
/** Withdraw actual configuration prerequisites immediately BEFORE possible download TX.
 * @param[in,out] context Active write; one notice per yielded download.
 * @param[in] live Actual live reservation. @param[in,out] current Actual application/axis binding.
 * Its configuration generation advances exactly once; old prepared work/read snapshots fail epoch checks.
 * @param[in] nowUs Nondecreasing supplied time before the overall cutoff.
 * @return Local status, all outputs unchanged on failure. Call after queue admission and
 * application preflight, before SdoClient::nextTx/handoff. Queue rejection requires no notice.
 * Exhaustion rejects preparation.
 */
Status noteDriverTransmission(DriverSettingsContext& context, const SettingsReservation& live,
                              Binding& current, uint64_t nowUs) noexcept;
/** Consume a matching retained terminal; failures are operation outcomes, not retries.
 * @param[in,out] context Active settings. @param[in] live Actual live reservation.
 * @param[in] current Actual application binding after any configuration withdrawal.
 * @param[in] evidence Call-only sole-owner terminal; no context overlap.
 * @param[in] nowUs Supplied time at least completion and previous progression; deadline wins at equality.
 * @return Local validation status; valid failures settle context with retained partial effects.
 * Possible downloads require noteDriverTransmission before handoff. Readback mismatch fails atomically.
 */
Status advanceDriver(DriverSettingsContext& context, const SettingsReservation& live,
                     const Binding& current, const CANOPEN::SdoCompletion& evidence,
                     uint64_t nowUs) noexcept;
/** Local withdrawal after separately retiring owner work; never stop/rollback/replay.
 * @param[in,out] context Active settings. @param[in] reason CANCELLED, DEADLINE or INVALIDATED.
 * @param[in] nowUs Nondecreasing supplied time; deadline takes precedence.
 * @return Local status; errors preserve context, earlier effects/results remain retained.
 */
Status advanceDriver(DriverSettingsContext& context, ReadOutcome reason, uint64_t nowUs) noexcept;
/** Publish complete matched driver readback at the actual configuration epoch (unchanged for no-op).
 * @param[in] context Successful driver settings. @param[in] live Actual current reservation.
 * @param[in] current Actual current read target/epochs. @param[in] nowUs Supplied publication time.
 * @param[in] maximumAgeUs Nonzero inclusive age cap. @param[out] output Unchanged on failure, no overlap.
 * @return Checked raw register result, not physical activation or motion completion.
 */
Status getDriver(const DriverSettingsContext& context, const SettingsReservation& live,
                 const ReadTarget& current, uint64_t nowUs, uint64_t maximumAgeUs,
                 DriverObservation& output) noexcept;
/** Publish complete matched DI readback at the actual configuration epoch (unchanged for no-op).
 * @param[in] context Successful DI settings. @param[in] live Actual current reservation.
 * @param[in] current Actual current read target/epochs. @param[in] nowUs Supplied publication time.
 * @param[in] maximumAgeUs Nonzero inclusive age cap. @param[out] output Unchanged on failure, no overlap.
 * @return Checked documentary/raw result, not electrical level, wiring or protective effectiveness.
 */
Status getIo(const DriverSettingsContext& context, const SettingsReservation& live,
             const ReadTarget& current, uint64_t nowUs, uint64_t maximumAgeUs,
             IoObservation& output) noexcept;
} // namespace LICHUAN_C
} // namespace MotorControlCAN
