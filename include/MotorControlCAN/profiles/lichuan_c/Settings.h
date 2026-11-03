/** @file Settings.h
 * @brief Finite explicit 86C settings transactions, without controller I/O or clocks.
 * SPDX-License-Identifier: MIT
 * Applications supply actual retained service evidence and continuous exclusion
 * from the sole owner. No controlword/mode/save/reset preamble is emitted.
 * DI changes can immediately affect external enable/stop/limit controls.
 *
 * Configuration confidence: an explicit settings call advances the configuration generation at its
 * first possible download, so a target prepared earlier goes stale. A move, velocity or homing
 * operation's own writes are phases of it instead. They do not pass through these calls and do not advance
 * the generation, so they cannot invalidate that move's prepared target. They do make retained
 * MOTION_PARAMETERS observations untrustworthy; see retireMotionParameters.
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
/** Actual continuous exclusion of one axis, held by the sole application owner.
 * It records exclusion that already exists; it is not a permission a profile can
 * construct, and it is never carried on the wire. The owner establishes it before
 * prerequisite acquisition, prevents competing control/mode/target/RPDO/settings
 * work, and revokes it on contradictory state, reboot/rebind/recovery or
 * uncertainty. Settings, stop, action and PDO configuration work all use it. Pass
 * the actual live reservation on every call; a copy of an expired reservation does
 * not establish exclusion.
 */
struct AxisReservation {
    ReadTarget target; ///< Frozen pre-transaction service route, binding/config/node epochs.
    SettingsModel model = SettingsModel::UNKNOWN; ///< Actual application/nameplate model binding.
    uint32_t owner = 0;                           ///< Nonzero stable local owner identity.
    uint32_t sequence = 0;     ///< Nonzero exclusive sequence identity, not reused with live work.
    Generation generation = 0; ///< Nonzero owner control epoch; never wrap/reuse a live one.
    uint64_t acquiredUs = 0;   ///< Supplied acquisition time, zero valid.
};
/** Native position-mode parameters; native times are not acceleration rates. Defaults are invalid until explicitly set. */
struct DriverSettings {
    uint32_t positionSpeedRpm = UINT32_MAX; ///< Explicit requested 6081, inclusive 0..3000 rpm.
    uint32_t rampUpTimeMs = UINT32_MAX;     ///< Explicit requested 6083, inclusive 0..2000 ms.
    uint32_t rampDownTimeMs = UINT32_MAX;   ///< Explicit requested 6084, inclusive 0..2000 ms.
};
/** 2200 codes under their EN86/ZH86 p18 labels. Neither label states a physical or CAN direction. */
enum class DefaultDirection : uint16_t {
    PULSE_SIGN = 0,          ///< Code 0, "Pulse Sign"; no positive-direction claim.
    PULSE_INVERTED_SIGN = 1, ///< Code 1, "Pulse /Sign"; no positive-direction claim.
    UNKNOWN = 0xffff         ///< Invalid candidate default.
};
/** Explicit 2200 candidate; the default is invalid until set. */
struct DirectionSettings {
    DefaultDirection direction = DefaultDirection::UNKNOWN; ///< Requested 2200 code 0 or 1.
};
/** 2202 codes under their EN86/ZH86 p18 labels. The EDS default 10 is not a documented code. */
enum class SoftLimitEnable : uint16_t {
    LIMITS_OFF = 0,   ///< Code 0, "noneffective".
    AFTER_HOMING = 1, ///< Code 1, "effective after homing"; enforcement and frame are unqualified.
    UNKNOWN = 0xffff  ///< Invalid candidate default.
};
/** Explicit 2202 candidate; the default is invalid until set. 607D has no typed write. */
struct SoftLimitSettings {
    SoftLimitEnable enable = SoftLimitEnable::UNKNOWN; ///< Requested 2202 code 0 or 1.
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
/** Caller-owned finite driver/DI/control transaction; do not edit fields while active.
 * Masks use the fixed read group's field positions. Confirmed values are in
 * requested[]; the last exact terminal/abort is in evidence. No heap/hidden retry.
 */
struct DriverSettingsContext {
    AxisReservation reservation; ///< Copied continuous reservation and original transport epoch.
    DriverContext readback;      ///< Fixed group plan and later new-generation readback/provenance.
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
 * @param[in] state STATUS and MONITORS read after control completion. The whole read must have
 * succeeded with no uncertain effect, as a stop requires: a settings call is never urgent, so a
 * failed sibling block costs one fresh read, not an admission on a doubtful window.
 * @param[in] control Actual successful acknowledged 6040:00 U16 download 0x000F under this reservation.
 * @param[in] settings Complete explicit native candidate, in native units: 6081 is rpm whatever the
 * axis rate basis is. A common rate reaches it only through prepareVelocityTarget under
 * MOTOR_TURNS and MINUTE; validation precedes any yielded work.
 * @param[in] operation Nonzero unique live operation identity.
 * @param[in] nowUs Supplied nonwrapping microseconds, zero valid.
 * @param[in] deadlineUs Exclusive overall cutoff strictly after nowUs.
 * @param[in] options Nonzero service budgets, clamped by overall deadline.
 * @param[in] maximumAgeUs Nonzero inclusive earliest-observation age cap for all prerequisites.
 * @return Local status. Requires mode1, status low bits7, monitor running/alarm/disabled bits clear.
 * This conservative host subset never writes 6040/6060 or relies on disputed status bits.
 */
Status prepareDriverSettings(DriverSettingsContext& output, const AxisReservation& reservation,
                             const DriverContext& prior, const ReadContext& state,
                             const CANOPEN::SdoCompletion& control, const DriverSettings& settings,
                             uint32_t operation, uint64_t nowUs, uint64_t deadlineUs,
                             const ReadOptions& options, uint64_t maximumAgeUs) noexcept;
/** Prepare one explicit write of the 2200 selector, without a position-mode preamble.
 * Only the stored code is established. Whether it reverses CAN motion or changes 6064, 607A, 607D or
 * homing, and when it takes effect, stay Q04 gates. The application still owns its sign and witness.
 * Prerequisites are project policy, as no source gives a state for this object: both documented prior
 * codes, limits off (RS's geometry dependency) and no running or alarm report. Bit 4 of 2100 is not
 * used, since both specimens report it clear at power-up with no enable.
 * @param[out] output Owned context, unchanged on rejection; no input overlap.
 * @param[in] reservation Actual live sole-owner exclusion acquired before all prerequisites.
 * @param[in] prior Fresh complete DRIVER context; 2201 is retained, never written, and rechecked.
 * @param[in] limits Fresh complete SOFT_LIMITS context reporting 2202=0. Set it off explicitly first.
 * @param[in] state Clean STATUS and MONITORS read under the reservation, as prepareDriverSettings.
 * @param[in] settings Explicit code 0 or 1.
 * @param[in] operation Nonzero unique live operation identity.
 * @param[in] nowUs Supplied nonwrapping microseconds, zero valid.
 * @param[in] deadlineUs Exclusive overall cutoff strictly after nowUs.
 * @param[in] options Nonzero service budgets, clamped by the overall deadline.
 * @param[in] maximumAgeUs Nonzero inclusive earliest-observation age cap for all prerequisites.
 * @return Local status. An unchanged candidate succeeds from fresh evidence without TX or epoch change.
 */
Status prepareDirectionSettings(DriverSettingsContext& output, const AxisReservation& reservation,
                                const DriverContext& prior, const DriverContext& limits,
                                const ReadContext& state, const DirectionSettings& settings,
                                uint32_t operation, uint64_t nowUs, uint64_t deadlineUs,
                                const ReadOptions& options, uint64_t maximumAgeUs) noexcept;
/** Prepare one explicit write of the 2202 selector; 607D stays read-only here.
 * Matching readback proves the stored code only. It is not protection: the coordinate frame, boundary
 * and reaction of the limits stay Q05 gates. A prior code outside 0/1, such as the EDS default 10, is
 * refused, as no source describes leaving it.
 * @param[out] output Owned context, unchanged on rejection; no input overlap.
 * @param[in] reservation Actual live sole-owner exclusion acquired before all prerequisites.
 * @param[in] prior Fresh complete SOFT_LIMITS context; 607D is retained, never written, and rechecked.
 * AFTER_HOMING needs its observed minimum strictly below its maximum.
 * @param[in] state Clean STATUS and MONITORS read under the reservation, reporting no running or alarm.
 * @param[in] settings Explicit code 0 or 1.
 * @param[in] operation Nonzero unique live operation identity.
 * @param[in] nowUs Supplied nonwrapping microseconds, zero valid.
 * @param[in] deadlineUs Exclusive overall cutoff strictly after nowUs.
 * @param[in] options Nonzero service budgets, clamped by the overall deadline.
 * @param[in] maximumAgeUs Nonzero inclusive earliest-observation age cap for all prerequisites.
 * @return Local status. An unchanged candidate succeeds from fresh evidence without TX or epoch change.
 */
Status prepareSoftLimitSettings(DriverSettingsContext& output, const AxisReservation& reservation,
                                const DriverContext& prior, const ReadContext& state,
                                const SoftLimitSettings& settings, uint32_t operation,
                                uint64_t nowUs, uint64_t deadlineUs, const ReadOptions& options,
                                uint64_t maximumAgeUs) noexcept;
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
Status prepareIoSettings(DriverSettingsContext& output, const AxisReservation& reservation,
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
Status nextDriver(const DriverSettingsContext& context, const AxisReservation& live,
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
Status noteDriverTransmission(DriverSettingsContext& context, const AxisReservation& live,
                              Binding& current, uint64_t nowUs) noexcept;
/** Consume a matching retained terminal; failures are operation outcomes, not retries.
 * @param[in,out] context Active settings. @param[in] live Actual live reservation.
 * @param[in] current Actual application binding after any configuration withdrawal.
 * @param[in] evidence Call-only sole-owner terminal; no context overlap.
 * @param[in] nowUs Supplied time at least completion and previous progression; deadline wins at equality.
 * @return Local validation status; valid failures settle context with retained partial effects.
 * Possible downloads require noteDriverTransmission before handoff. Readback mismatch fails atomically.
 */
Status advanceDriver(DriverSettingsContext& context, const AxisReservation& live,
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
Status getDriver(const DriverSettingsContext& context, const AxisReservation& live,
                 const ReadTarget& current, uint64_t nowUs, uint64_t maximumAgeUs,
                 DriverObservation& output) noexcept;
/** Publish complete matched DI readback at the actual configuration epoch (unchanged for no-op).
 * @param[in] context Successful DI settings. @param[in] live Actual current reservation.
 * @param[in] current Actual current read target/epochs. @param[in] nowUs Supplied publication time.
 * @param[in] maximumAgeUs Nonzero inclusive age cap. @param[out] output Unchanged on failure, no overlap.
 * @return Checked documentary/raw result, not electrical level, wiring or protective effectiveness.
 */
Status getIo(const DriverSettingsContext& context, const AxisReservation& live,
             const ReadTarget& current, uint64_t nowUs, uint64_t maximumAgeUs,
             IoObservation& output) noexcept;
/** Withdraw a retained MOTION_PARAMETERS observation because a move's own write may reach the drive.
 * Without this, prepareDriverSettings could report an unchanged candidate as success, with no TX,
 * while the drive holds the move's value. A move owner calls this once per retained context,
 * including a finished settings context's `readback`, for each own download, after queue admission
 * and application preflight and before any possible transmission. Do not transmit if it refuses.
 * The library cannot reach an unlisted copy, so keep one retained context per axis.
 * After retirement getDriver and prepareDriverSettings refuse this context until a fresh read
 * replaces it; its raw values and terminal remain as history. The configuration generation does not
 * move, so the write's own prepared target stays valid.
 * @param[in,out] retained Context to withdraw, on this axis at any generation. EMPTY and other
 * kinds are unchanged, so a caller can pass each of its contexts. Another axis is refused.
 * @param[in] current Actual current route and epochs.
 * @param[in] work Exact admitted download on `current`, at its EDS width: 6081, 6083, 6084, 6098,
 * 6099:01, 6099:02, 609A or 607C, which retire, or 607A or 60FF, which are accepted and retire
 * nothing. Prompt 36 still owes its own ruling on 607C. Anything else is refused.
 * @return OK, or INVALID_CONFIG with ReadError TARGET, ARGUMENT or OVERLAP and nothing changed.
 * Performs no I/O and does not prove the write happened.
 */
Status retireMotionParameters(DriverContext& retained, const ReadTarget& current,
                              const CANOPEN::SdoTransfer& work) noexcept;
} // namespace LICHUAN_C
} // namespace MotorControlCAN
