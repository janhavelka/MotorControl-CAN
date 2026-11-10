/** @file ConfigurationRestore.h
 * @brief Explicit application capture and re-application of three recorded motion parameters.
 * SPDX-License-Identifier: MIT
 * No I/O, clocks, allocation or persistent command. One caller owns these records and contexts.
 */
#pragma once
#include <MotorControlCAN/profiles/lichuan_c/Settings.h>

namespace MotorControlCANExample {
/** Owned partial configuration, never a complete backup or a factory-default image.
 * Treat captured fields as immutable. External record identifiers refer to caller-retained evidence;
 * raw identity equality alone cannot distinguish two specimens of the same firmware.
 */
struct ConfigurationRecord {
    MotorControlCAN::LICHUAN_C::SettingsModel model =
        MotorControlCAN::LICHUAN_C::SettingsModel::UNKNOWN; ///< Independently established model.
    uint32_t recordId = 0; ///< Nonzero original application capture/provenance record.
    uint32_t fixtureRecord = 0; ///< Nonzero actual specimen/nameplate/wiring record.
    uint32_t firmwareRecord = 0; ///< Nonzero actual model/firmware applicability record.
    uint64_t capturedUs = 0; ///< Supplied capture time; individual acquisition times remain below.
    MotorControlCAN::LICHUAN_C::IdentityObservation identity; ///< Original route, epochs and evidence.
    MotorControlCAN::LICHUAN_C::DriverObservation configuration; ///< Only 6081/6083/6084 and provenance.
};
/** Explicit current-specimen reconciliation, including any changed service route.
 * The application establishes these facts independently; these references cannot authenticate them.
 */
struct ConfigurationRestoreReview {
    MotorControlCAN::LICHUAN_C::ReadTarget target; ///< Exact reconciled route and current epochs.
    uint32_t fixtureRecord = 0; ///< Must match the original actual specimen record.
    uint32_t firmwareRecord = 0; ///< Must match the original firmware record.
    uint32_t routeRecord = 0; ///< Nonzero reviewed current identity/route reconciliation record.
};
/** One explicit re-application; all partial writes and uncertainty stay in settings.
 * Use the ordinary nextDriver/noteDriverTransmission/advanceDriver/getDriver functions on settings,
 * with the actual live owner reservation. No wrapper retries, rollback, save, enable or reset exists.
 */
struct ConfigurationRestoreContext {
    ConfigurationRecord original; ///< Original owned record survives failure of any later field.
    MotorControlCAN::LICHUAN_C::IdentityObservation identity; ///< Checked current specimen observation.
    ConfigurationRestoreReview review; ///< Explicit current-route reconciliation retained unchanged.
    MotorControlCAN::LICHUAN_C::DriverSettingsContext settings; ///< Existing three-field write engine.
};
/** Capture an original partial record atomically from the ordinary checked read path.
 * @param[out] output Unchanged on refusal; no input may overlap it.
 * @param[in] reservation Actual live owner exclusion and independently established model.
 * @param[in] current Actual route/epochs, exactly matching the reservation.
 * @param[in] identity Complete fresh SDO identity acquired under this reservation.
 * @param[in] configuration Complete fresh MOTION_PARAMETERS SDO read under this reservation.
 * @param[in] recordId Nonzero retained original capture record.
 * @param[in] fixtureRecord Nonzero actual specimen provenance, not inferred from identity codes.
 * @param[in] firmwareRecord Nonzero independently established model/firmware provenance.
 * @param[in] nowUs Supplied monotonic capture microseconds, zero valid.
 * @param[in] maximumAgeUs Nonzero inclusive acquisition-age limit.
 * @return Local status only. Raw out-of-range values may be recorded; re-application validates them.
 */
MotorControlCAN::Status captureConfiguration(
    ConfigurationRecord& output, const MotorControlCAN::LICHUAN_C::AxisReservation& reservation,
    const MotorControlCAN::LICHUAN_C::ReadTarget& current,
    const MotorControlCAN::LICHUAN_C::ReadContext& identity,
    const MotorControlCAN::LICHUAN_C::DriverContext& configuration, uint32_t recordId,
    uint32_t fixtureRecord, uint32_t firmwareRecord, uint64_t nowUs,
    uint64_t maximumAgeUs) noexcept;
/** Prepare an explicit re-application of the recorded three motion fields atomically.
 * @param[out] output Unchanged on refusal; no input may overlap any of its storage.
 * @param[in] original Immutable original partial record; no guessed defaults or undocumented fields.
 * @param[in] review Explicit same-specimen/firmware and current-route reconciliation references.
 * @param[in] reservation Actual current live owner exclusion and same independently established model.
 * @param[in] current Actual reconciled route/epochs; old epochs may differ, no automatic binding.
 * @param[in] identity Fresh checked complete identity under the current reservation; all raw codes and
 * widths must match the original. Equality supplements the external specimen record, never replaces it.
 * @param[in] prior Fresh current MOTION_PARAMETERS read after the acknowledged control write.
 * @param[in] state Fresh clean STATUS/MONITORS after that control, as prepareDriverSettings requires.
 * @param[in] control Actual acknowledged 6040=000F under the reservation; no enable is generated.
 * @param[in] operation Nonzero unique live settings identity.
 * @param[in] nowUs Supplied monotonic preparation microseconds at least the original capture time.
 * @param[in] deadlineUs Immutable exclusive overall cutoff strictly after nowUs.
 * @param[in] options Explicit nonzero service budgets, as prepareDriverSettings.
 * @param[in] maximumAgeUs Nonzero inclusive freshness limit for the current prerequisites.
 * @return Local status from validation or prepareDriverSettings. Matching unchanged fields need no
 * write; failures can leave earlier fields applied. Success proves matching live registers only.
 */
MotorControlCAN::Status prepareConfigurationRestore(
    ConfigurationRestoreContext& output, const ConfigurationRecord& original,
    const ConfigurationRestoreReview& review,
    const MotorControlCAN::LICHUAN_C::AxisReservation& reservation,
    const MotorControlCAN::LICHUAN_C::ReadTarget& current,
    const MotorControlCAN::LICHUAN_C::ReadContext& identity,
    const MotorControlCAN::LICHUAN_C::DriverContext& prior,
    const MotorControlCAN::LICHUAN_C::ReadContext& state,
    const MotorControlCAN::CANOPEN::SdoCompletion& control, uint32_t operation,
    uint64_t nowUs, uint64_t deadlineUs, const MotorControlCAN::LICHUAN_C::ReadOptions& options,
    uint64_t maximumAgeUs) noexcept;
} // namespace MotorControlCANExample
