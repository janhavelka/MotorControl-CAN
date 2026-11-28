/** @file ConfigurationRestore.h
 * @brief Explicit application capture and re-application of recorded configuration: three motion
 * parameters, and the partial backup record a factory restore's guard names (prompt 61).
 * SPDX-License-Identifier: MIT
 * No I/O, clocks, allocation or persistent command. One caller owns these records and contexts.
 */
#pragma once
#include <MotorControlCAN/profiles/lichuan_c/Tuning.h>

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
/** Most fields one PersistenceBackup holds: MOTION_PARAMETERS 3, DRIVER 2, SOFT_LIMITS 4,
 * IO_INPUT_CONFIGURATION 9 and CONTROL 2. */
constexpr uint8_t BACKUP_FIELDS = 20;
/** The library's typed write that can set a backup field again, or none. A field is re-applicable only
 * through its typed write, never through a raw download. */
enum class BackupWrite : uint8_t {
    NONE,        ///< No typed write: a change is reported as changed and never re-applied.
    MOTION,      ///< 6081/6083/6084 through prepareDriverSettings (prepareConfigurationRestore).
    SOFT_LIMIT,  ///< 2202 code 0 or 1 through prepareSoftLimitSettings; 607D has no typed write.
    SUBDIVISION, ///< 2201 through prepareSubdivisionSettings; 2200 has no typed write.
    IO,          ///< 2400:01 bits 0..6 and 2400:02..08 codes 0..13 through prepareIoSettings.
    CONTROL      ///< 2500 code 0..2 through prepareControlSettings; 2501 has no typed write.
};
/** One recorded field: its object and its raw value bits as the typed getter published them. */
struct BackupField {
    uint16_t index = 0;   ///< Native object index.
    uint8_t subindex = 0; ///< Exact subindex.
    uint32_t raw = 0;     ///< Raw value bits; a signed field keeps its two's complement bits.
    /** The typed write of this field; NONE when it has none, or when the recorded value is one no typed
     * candidate can name (an undocumented code or a value outside the typed range). */
    BackupWrite write = BackupWrite::NONE;
};
/** Bits of PersistenceBackup::unknown: what no backup record here covers. All are always set. */
constexpr uint8_t BACKUP_UNKNOWN_PERSISTENT_SET = 0x01; ///< The complete set a save stores (prompt 39, Q15).
constexpr uint8_t BACKUP_UNKNOWN_FACTORY_VALUES = 0x02; ///< The values a factory restore writes.
/** Objects outside the typed groups recorded here: identity, 1017, 2203, the PDO records, tuning, 2300 and
 * every object no typed group reads. */
constexpr uint8_t BACKUP_UNKNOWN_OTHER_OBJECTS = 0x04;
constexpr uint8_t BACKUP_UNKNOWN_ALL = 0x07; ///< Every BACKUP_UNKNOWN bit.
/** The typed groups recordBackupGroup accepts, as bits (1 << DriverGroup). */
constexpr uint16_t BACKUP_GROUPS_SUPPORTED =
    uint16_t(1u << unsigned(MotorControlCAN::LICHUAN_C::DriverGroup::DRIVER) |
             1u << unsigned(MotorControlCAN::LICHUAN_C::DriverGroup::SOFT_LIMITS) |
             1u << unsigned(MotorControlCAN::LICHUAN_C::DriverGroup::MOTION_PARAMETERS) |
             1u << unsigned(MotorControlCAN::LICHUAN_C::DriverGroup::IO_INPUT_CONFIGURATION) |
             1u << unsigned(MotorControlCAN::LICHUAN_C::DriverGroup::CONTROL));
/** Every DriverGroup bit, (1 << DriverGroup) for DRIVER..PERSISTENCE. */
constexpr uint16_t BACKUP_GROUPS_ALL =
    uint16_t((1u << (unsigned(MotorControlCAN::LICHUAN_C::DriverGroup::PERSISTENCE) + 1)) - 1);
/** The record a factory restore's backup guard names (prompt 61, decision 3): the typed groups the
 * application actually read back before the restore, with what stays unknown listed. It is never a
 * complete or restorable device image, because the complete persistent set and the factory values are
 * unknown (prompt 39): complete is always false and unknown always BACKUP_UNKNOWN_ALL.
 * PersistenceReview::backupRecord carries recordId, and the library keeps only that identity. The same
 * type holds the groups read again after a restore, which compareBackup compares with the backup.
 * Treat recorded fields as immutable.
 */
struct PersistenceBackup {
    uint32_t recordId = 0;   ///< Nonzero application record identity once a group is recorded.
    uint64_t capturedUs = 0; ///< Supplied time of the first recorded group.
    MotorControlCAN::LICHUAN_C::ReadTarget target; ///< The route and epochs of the first recorded group.
    uint16_t groupsRead = 0; ///< Bits (1 << DriverGroup) of the typed groups recorded.
    /** Bits (1 << DriverGroup) of every group not recorded: BACKUP_GROUPS_ALL without groupsRead. */
    uint16_t groupsUnknown = BACKUP_GROUPS_ALL;
    uint8_t unknown = BACKUP_UNKNOWN_ALL; ///< Always every BACKUP_UNKNOWN bit; nothing here clears one.
    bool complete = false;  ///< Always false; no function here sets it.
    uint8_t fieldCount = 0; ///< Recorded fields, in recording order and source order within a group.
    BackupField fields[BACKUP_FIELDS]; ///< The recorded fields.
};
/** Record one complete fresh typed group read into a backup record, atomically.
 * The first call on an empty record (recordId zero) starts it with recordId; later calls must name the
 * same identity and the same route and epochs. Each group is recorded at most once.
 * @param[in,out] backup Unchanged on refusal; no input may overlap it.
 * @param[in] recordId Nonzero application record identity, retained with the record's evidence.
 * @param[in] group DRIVER, SOFT_LIMITS, MOTION_PARAMETERS (getDriver), IO_INPUT_CONFIGURATION (getIo) or
 * CONTROL (getControl); any other group stays listed in groupsUnknown.
 * @param[in] read The complete successful group read; the typed getter judges it.
 * @param[in] current Actual route and epochs, as for the getter.
 * @param[in] nowUs Supplied monotonic microseconds.
 * @param[in] maximumAgeUs Nonzero inclusive age limit, as for the getter.
 * @return OK; INVALID_CONFIG with ReadError::ARGUMENT for a zero or different record identity, an
 * unsupported or repeated group or overlapping storage; INVALID_CONFIG with ReadError::TARGET for another
 * route than the record's; otherwise the getter's own status. No I/O.
 */
MotorControlCAN::Status recordBackupGroup(PersistenceBackup& backup, uint32_t recordId,
                                          MotorControlCAN::LICHUAN_C::DriverGroup group,
                                          const MotorControlCAN::LICHUAN_C::DriverContext& read,
                                          const MotorControlCAN::LICHUAN_C::ReadTarget& current,
                                          uint64_t nowUs, uint64_t maximumAgeUs) noexcept;
/** What a re-read after a factory restore found for one backup field. */
enum class BackupChange : uint8_t {
    UNCHANGED, ///< The re-read raw value equals the backup's.
    REAPPLY,   ///< Changed, and its typed write can name the backup's value: prepareBackupReapply.
    CHANGED,   ///< Changed, and no typed write can name the backup's value: reported, never written.
    NOT_REREAD ///< The re-read record lacks the field: its current value is unknown.
};
/** Field-by-field comparison of a backup record with a later re-read of the same groups. */
struct BackupComparison {
    uint32_t backupRecord = 0; ///< The backup's recordId.
    uint32_t afterRecord = 0;  ///< The re-read's recordId.
    uint8_t fieldCount = 0;    ///< The backup's fieldCount.
    BackupChange change[BACKUP_FIELDS] = {}; ///< By backup field position.
    uint8_t unchanged = 0; ///< Fields UNCHANGED.
    uint8_t reapply = 0;   ///< Fields REAPPLY.
    uint8_t changed = 0;   ///< Fields CHANGED.
    uint8_t notReread = 0; ///< Fields NOT_REREAD.
    uint16_t groupsUnknown = BACKUP_GROUPS_ALL; ///< The backup's groupsUnknown: never compared.
    uint8_t unknown = BACKUP_UNKNOWN_ALL;       ///< The backup's unknown scope, always every bit.
    bool complete = false; ///< Always false: an equal re-read proves only the recorded fields.
};
/** Compare a backup with the record of the same groups read again after a factory restore, atomically.
 * @param[out] output Unchanged on refusal; no input may overlap it.
 * @param[in] backup The record read before the restore; nonzero recordId.
 * @param[in] after The record read afterwards; nonzero recordId other than the backup's.
 * @return OK, or INVALID_CONFIG with ReadError::ARGUMENT. A changed typed field whose whole typed
 * candidate cannot be built is CHANGED, never REAPPLY: a MOTION, IO or SUBDIVISION/SOFT_LIMIT/CONTROL
 * field whose recorded value no candidate can name, or a MOTION or IO field whose group-mates were not
 * read again. No I/O.
 */
MotorControlCAN::Status compareBackup(BackupComparison& output, const PersistenceBackup& backup,
                                      const PersistenceBackup& after) noexcept;
/** The typed candidates that set the REAPPLY fields of a comparison to the backup's values. */
struct BackupReapply {
    uint8_t writes = 0; ///< Bits (1 << BackupWrite) of the typed writes that have a candidate below.
    MotorControlCAN::LICHUAN_C::DriverSettings motion;           ///< MOTION: the three recorded values.
    MotorControlCAN::LICHUAN_C::SoftLimitSettings softLimit;     ///< SOFT_LIMIT: the recorded 2202 code.
    MotorControlCAN::LICHUAN_C::SubdivisionSettings subdivision; ///< SUBDIVISION: the recorded 2201.
    /** IO: the recorded 2400:01 bits 0..6 and the seven recorded codes; prepareIoSettings keeps the
     * current high polarity bits. */
    MotorControlCAN::LICHUAN_C::IoSettings io;
    MotorControlCAN::LICHUAN_C::ControlSettings control; ///< CONTROL: the recorded 2500 code.
};
/** Build the typed candidates for the REAPPLY fields of a comparison, atomically. Nothing is written:
 * each candidate is one explicit later request through its library typed write (BackupWrite), with that
 * write's own fresh prerequisites, readback and uncertainty. CHANGED fields get no candidate and stay
 * reported as changed.
 * @param[out] output Unchanged on refusal; no input may overlap it.
 * @param[in] backup The compared backup record.
 * @param[in] comparison compareBackup's output for that backup.
 * @return OK, also with no candidate; INVALID_CONFIG with ReadError::ARGUMENT for another record. No I/O.
 */
MotorControlCAN::Status prepareBackupReapply(BackupReapply& output, const PersistenceBackup& backup,
                                             const BackupComparison& comparison) noexcept;
} // namespace MotorControlCANExample
