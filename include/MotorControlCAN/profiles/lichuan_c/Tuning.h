/** @file Tuning.h
 * @brief Finite Lichuan native control/tuning reads and explicit loop selection.
 * SPDX-License-Identifier: MIT
 * No I/O, clock, allocation or automatic action. Raw values retain unknown
 * units/ranges; an acquired word is not a qualified physical setting.
 */
#pragma once
#include <MotorControlCAN/profiles/lichuan_c/Settings.h>

namespace MotorControlCAN {
namespace LICHUAN_C {
/** Documented 2500 selector, independent of 6060 motion mode and motor kind. */
enum class LoopMode : uint16_t {
    INEFFECTIVE = 0, ///< Vendor label; no inferred disable/stop behavior.
    OPEN_LOOP = 1,   ///< Native open-loop selection, not a motion command.
    CLOSED_LOOP = 2, ///< Native closed-loop selection; no inferred encoder scale.
    UNKNOWN = 0xffff ///< Unknown observation or invalid candidate default.
};
/** Named finite read assignments; all entries are U16 at subindex zero. */
enum class TuningGroup : uint8_t {
    CURRENT_HOLDING, ///< 2502..2507, mA/percent/ms without invented operating limits.
    FILTER_POSITION, ///< 2508..250D, raw filter and position supervision words.
    LOOP_GAINS,      ///< 250E..2519 documented factory loop parameters.
    EDS_GAINS        ///< 251A..2520 EDS-only symbolic words, no semantic interpretation.
};
/** Complete two-word control observation; encoder convention remains unknown. */
struct ControlObservation {
    ReadTarget target;        ///< Current binding/configuration/node epochs checked by getter.
    uint16_t loopModeRaw = 0; ///< Exact 2500 word, including unknown values.
    LoopMode loopMode = LoopMode::UNKNOWN; ///< Decoded only for documented values 0..2.
    uint16_t encoderResolutionRaw = 0;     ///< 2501; no lines/counts/quadrature inference.
    ReadProvenance fields[2]; ///< Owned per-field history; not an atomic device snapshot.
    ReadWindow window;        ///< Conservative earliest/latest interval and skew.
};
/** Complete native group; only members of the selected group are meaningful.
 * Unknown values are preserved. No numeric word implies a safe operating range,
 * a current/torque equivalence, physical position scale or consumed-read policy.
 */
struct TuningObservation {
    TuningGroup group = TuningGroup::CURRENT_HOLDING; ///< Selects meaningful members.
    ReadTarget target;                                ///< Checked current target and epochs.
    uint16_t maximumPeakCurrentMa = 0;                ///< 2502 mA; motor/drive rating not inferred.
    uint16_t closedLoopMaximumPercent = 0;            ///< 2503; no assumed 0..100 bound.
    uint16_t closedLoopBasePercent = 0;               ///< 2504; no assumed ordering versus 2503.
    uint16_t openLoopMaximumPercent = 0;              ///< 2505; not a torque request.
    uint16_t holdingCurrentPercent = 0;               ///< 2506; no inferred disabled sentinel.
    uint16_t holdingTimeMs = 0;                ///< 2507 ms; zero/maximum operating meaning unknown.
    uint16_t lowPassEnableRaw = 0;             ///< 2508; no documented Boolean encoding.
    uint16_t lowPassCoefficientRaw = 0;        ///< 2509; equation/unit unknown.
    uint16_t positionAlarmThresholdRaw = 0;    ///< 250A setting, not measured following error.
    uint16_t inPositionThresholdRaw = 0;       ///< 250B; native coordinate scale unknown.
    uint16_t inPositionTimeRaw = 0;            ///< 250C; no documented time unit.
    uint16_t averagingFilterRaw = 0;           ///< 250D; equation/unit unknown.
    uint16_t currentProportionalFactorRaw = 0; ///< 250E factory loop parameter.
    uint16_t currentKpRaw = 0;           ///< 250F factory loop parameter, not physical torque.
    uint16_t currentKiRaw = 0;           ///< 2510 factory loop parameter.
    uint16_t currentKcRaw = 0;           ///< 2511 factory loop parameter.
    uint16_t velocityKp1Raw = 0;         ///< 2512 factory LA velocity parameter.
    uint16_t velocityKv1Raw = 0;         ///< 2513 factory LA velocity parameter.
    uint16_t velocityNode1Raw = 0;       ///< 2514; velocity scale unknown.
    uint16_t velocityKp2Raw = 0;         ///< 2515 factory LA velocity parameter.
    uint16_t velocityKv2Raw = 0;         ///< 2516 factory LA velocity parameter.
    uint16_t velocityNode2Raw = 0;       ///< 2517; no inferred ordering versus 2514.
    uint16_t velocityFeedforwardRaw = 0; ///< 2518 factory Kvf parameter.
    uint16_t positionKiRaw = 0;          ///< 2519 factory position loop gain.
    uint16_t edsGainsRaw[7] = {};        ///< Exact 251A..2520 in order; symbolic names only.
    uint8_t fieldCount = 0;              ///< 6, 6, 12 or 7 fields, selected group only.
    ReadProvenance fields[12];           ///< Owned independent per-field provenance.
    ReadWindow window;                   ///< Acquisition interval, not a simultaneous sample.
};
/** Explicit complete loop selector candidate; encoder settings are not writable here. */
struct ControlSettings {
    LoopMode loopMode = LoopMode::UNKNOWN; ///< Explicit documented 0, 1 or 2 only.
};
/** Prepare exactly 2500/2501 uploads; same ownership/time/failure contract as prepareDriverRead.
 * @param[out] output Replaced only on success, no overlap with inputs.
 * @param[in] target Actual current channel/binding/node epochs.
 * @param[in] operation Nonzero unique while prior work can exist.
 * @param[in] nowUs Nonwrapping monotonic microseconds; zero valid.
 * @param[in] deadlineUs Exclusive immutable cutoff greater than nowUs.
 * @param[in] options Explicit nonzero queue/response budgets.
 * @return Local preparation only; nextDriver/advanceDriver drive the bounded lifecycle.
 */
Status prepareControlRead(DriverContext& output, const ReadTarget& target, uint32_t operation,
                          uint64_t nowUs, uint64_t deadlineUs, const ReadOptions& options) noexcept;
/** Prepare one finite tuning group; parameters and guarantees match prepareControlRead.
 * @param[out] output Replaced only on success; no overlap with inputs.
 * @param[in] target Current explicit target/epochs.
 * @param[in] operation Nonzero operation identity.
 * @param[in] nowUs Nonwrapping monotonic supplied microseconds, zero valid.
 * @param[in] deadlineUs Exclusive immutable overall cutoff.
 * @param[in] options Nonzero explicit service budgets.
 * @param[in] group One declared finite group, no implicit full-dictionary sweep.
 * @return OK creates no I/O; all uploads retain unqualified effects, never implicit probes.
 */
Status prepareTuningRead(DriverContext& output, const ReadTarget& target, uint32_t operation,
                         uint64_t nowUs, uint64_t deadlineUs, const ReadOptions& options,
                         TuningGroup group) noexcept;
/** Publish a complete current control read; error preserves output, no overlap.
 * @param[in] context Successful CONTROL context only.
 * @param[in] current Actual current target and epochs.
 * @param[in] nowUs Supplied time at least delivery times.
 * @param[in] maximumAgeUs Nonzero inclusive cap from each earliest acquisition bound.
 * @param[out] output Owned observation, unchanged on failure.
 * @return OK establishes raw acquisition, not encoder scale or physical loop transition.
 */
Status getControl(const DriverContext& context, const ReadTarget& current, uint64_t nowUs,
                  uint64_t maximumAgeUs, ControlObservation& output) noexcept;
/** Publish a complete current tuning read, with the same atomic/time contract as getControl.
 * @param[in] context Successful finite TUNING context only.
 * @param[in] current Actual target and binding/configuration/node epochs.
 * @param[in] nowUs Supplied time at least delivery times.
 * @param[in] maximumAgeUs Nonzero inclusive age cap from earliest acquisition.
 * @param[out] output Owned observation, unchanged on failure; no input overlap.
 * @return OK is raw acquisition, not qualification of unknown ranges or units.
 */
Status getTuning(const DriverContext& context, const ReadTarget& current, uint64_t nowUs,
                 uint64_t maximumAgeUs, TuningObservation& output) noexcept;
/** Prepare explicit 86C loop selection using the existing settings lifecycle.
 * @param[out] output Replaced on success only; no overlap with inputs.
 * @param[in] reservation Live exclusive model-bound owner reservation acquired before prior.
 * @param[in] prior Complete fresh CONTROL read under the same reservation/epochs.
 * @param[in] settings Explicit enum candidate; no encoder setting or hidden state preamble.
 * @param[in] operation Nonzero operation identity, distinct from live work.
 * @param[in] nowUs Nondecreasing caller microseconds, zero valid.
 * @param[in] deadlineUs Exclusive immutable overall cutoff.
 * @param[in] options Nonzero queue/response budgets.
 * @param[in] maximumAgeUs Nonzero prerequisite age limit.
 * @return OK only prepares changed 2500 work. Use nextDriver, noteDriverTransmission
 * and advanceDriver. Possible transmission withdraws configuration once; ACK/readback
 * do not establish physical activation or motion. No enable/save/reset/retry occurs.
 */
Status prepareControlSettings(DriverSettingsContext& output, const SettingsReservation& reservation,
                              const DriverContext& prior, const ControlSettings& settings,
                              uint32_t operation, uint64_t nowUs, uint64_t deadlineUs,
                              const ReadOptions& options, uint64_t maximumAgeUs) noexcept;
/** Publish current matched control readback, with settings exclusion still live.
 * @param[in] context Successful explicit loop settings transaction.
 * @param[in] live Actual still-exclusive reservation; never a host wire-correlation tag.
 * @param[in] current Actual binding/configuration/node epochs after possible effects.
 * @param[in] nowUs Nondecreasing supplied microseconds.
 * @param[in] maximumAgeUs Nonzero inclusive earliest-observation age cap.
 * @param[out] output Owned result, unchanged on error; no input overlap.
 * @return OK confirms matching register readback only; partial/uncertain history stays in context.
 */
Status getControl(const DriverSettingsContext& context, const SettingsReservation& live,
                  const ReadTarget& current, uint64_t nowUs, uint64_t maximumAgeUs,
                  ControlObservation& output) noexcept;
} // namespace LICHUAN_C
} // namespace MotorControlCAN
