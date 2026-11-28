// SPDX-License-Identifier: MIT
#include "ConfigurationRestore.h"

namespace MotorControlCANExample {
namespace M = MotorControlCAN;
namespace L = MotorControlCAN::LICHUAN_C;
namespace {
M::Status error(M::ReadError reason, const char* message) {
    return M::Status(M::Err::INVALID_CONFIG, static_cast<int32_t>(reason), message);
}
bool overlaps(const void* a, size_t an, const void* b, size_t bn) {
    const uintptr_t x = reinterpret_cast<uintptr_t>(a), y = reinterpret_cast<uintptr_t>(b);
    return x >= y ? x - y < bn : y - x < an;
}
bool sameTarget(const L::ReadTarget& a, const L::ReadTarget& b) {
    return a.nodeGeneration == b.nodeGeneration &&
           M::sameBinding(a.channel.binding, b.channel.binding) &&
           a.channel.clientToServerId == b.channel.clientToServerId &&
           a.channel.serverToClientId == b.channel.serverToClientId;
}
bool sameIdentity(const L::IdentityObservation& a, const L::IdentityObservation& b) {
    return a.deviceType == b.deviceType && a.hardwareVersion == b.hardwareVersion &&
           a.hardwareVersionWidth == b.hardwareVersionWidth && a.softwareVersion == b.softwareVersion &&
           a.softwareVersionWidth == b.softwareVersionWidth && a.identityEntries == b.identityEntries &&
           a.vendorId == b.vendorId && a.productCode == b.productCode && a.revision == b.revision &&
           a.modelCode == b.modelCode && a.firmwareCode == b.firmwareCode;
}
bool modelKnown(L::SettingsModel model) {
    return model == L::SettingsModel::CL86_C || model == L::SettingsModel::OL86_C;
}
M::Status checkIdentity(const L::ReadContext& identity, const L::AxisReservation& reservation,
                        const L::ReadTarget& current, uint64_t now, uint64_t age,
                        L::IdentityObservation& output) {
    if (!modelKnown(reservation.model) || !reservation.owner || !reservation.sequence ||
        !reservation.generation || !sameTarget(reservation.target, current) ||
        identity.source != L::ReadSource::SDO || identity.uncertain ||
        !L::getIdentity(identity, current, now, age, output) ||
        output.window.earliestUs < reservation.acquiredUs)
        return error(M::ReadError::TARGET, "fresh identity under the actual owner reservation required");
    return M::Ok();
}
} // namespace

M::Status captureConfiguration(ConfigurationRecord& output, const L::AxisReservation& reservation,
                               const L::ReadTarget& current, const L::ReadContext& identity,
                               const L::DriverContext& configuration, uint32_t recordId,
                               uint32_t fixtureRecord, uint32_t firmwareRecord, uint64_t now,
                               uint64_t age) noexcept {
    if (overlaps(&output, sizeof output, &reservation, sizeof reservation) ||
        overlaps(&output, sizeof output, &current, sizeof current) ||
        overlaps(&output, sizeof output, &identity, sizeof identity) ||
        overlaps(&output, sizeof output, &configuration, sizeof configuration))
        return error(M::ReadError::OVERLAP, "configuration capture overlaps input");
    if (!recordId || !fixtureRecord || !firmwareRecord)
        return error(M::ReadError::ARGUMENT, "original configuration provenance required");
    ConfigurationRecord candidate;
    M::Status checked = checkIdentity(identity, reservation, current, now, age, candidate.identity);
    if (!checked)
        return checked;
    if (configuration.read.source != L::ReadSource::SDO || configuration.read.uncertain ||
        !L::getDriver(configuration, current, now, age, candidate.configuration) ||
        candidate.configuration.group != L::DriverGroup::MOTION_PARAMETERS ||
        candidate.configuration.window.earliestUs < reservation.acquiredUs)
        return error(M::ReadError::CORRELATION, "fresh reserved motion configuration required");
    candidate.model = reservation.model;
    candidate.recordId = recordId;
    candidate.fixtureRecord = fixtureRecord;
    candidate.firmwareRecord = firmwareRecord;
    candidate.capturedUs = now;
    output = candidate;
    return M::Ok();
}
M::Status prepareConfigurationRestore(
    ConfigurationRestoreContext& output, const ConfigurationRecord& original,
    const ConfigurationRestoreReview& review, const L::AxisReservation& reservation,
    const L::ReadTarget& current, const L::ReadContext& identity, const L::DriverContext& prior,
    const L::ReadContext& state, const M::CANOPEN::SdoCompletion& control, uint32_t operation,
    uint64_t now, uint64_t deadline, const L::ReadOptions& options, uint64_t age) noexcept {
    if (overlaps(&output, sizeof output, &original, sizeof original) ||
        overlaps(&output, sizeof output, &review, sizeof review) ||
        overlaps(&output, sizeof output, &reservation, sizeof reservation) ||
        overlaps(&output, sizeof output, &current, sizeof current) ||
        overlaps(&output, sizeof output, &identity, sizeof identity) ||
        overlaps(&output, sizeof output, &prior, sizeof prior) ||
        overlaps(&output, sizeof output, &state, sizeof state) ||
        overlaps(&output, sizeof output, &control, sizeof control) ||
        overlaps(&output, sizeof output, &options, sizeof options))
        return error(M::ReadError::OVERLAP, "configuration restore overlaps input");
    if (!original.recordId || !original.fixtureRecord || !original.firmwareRecord ||
        !modelKnown(original.model) || original.model != reservation.model ||
        original.configuration.group != L::DriverGroup::MOTION_PARAMETERS ||
        original.configuration.fieldCount != 3 ||
        !sameTarget(original.identity.target, original.configuration.target) ||
        original.capturedUs > now || original.identity.window.latestUs > original.capturedUs ||
        original.configuration.window.latestUs > original.capturedUs ||
        review.fixtureRecord != original.fixtureRecord ||
        review.firmwareRecord != original.firmwareRecord || !review.routeRecord ||
        !sameTarget(review.target, current))
        return error(M::ReadError::TARGET, "original record and explicit same-specimen route review required");
    L::IdentityObservation observed;
    const M::Status checked = checkIdentity(identity, reservation, current, now, age, observed);
    if (!checked)
        return checked;
    if (!sameIdentity(original.identity, observed))
        return error(M::ReadError::CORRELATION, "current identity differs from original configuration record");
    L::DriverSettings settings;
    settings.positionSpeedRpm = original.configuration.positionSpeedRpm;
    settings.rampUpTimeMs = original.configuration.rampUpTimeMs;
    settings.rampDownTimeMs = original.configuration.rampDownTimeMs;
    // The existing engine validates the whole current candidate atomically. Once it succeeds,
    // only infallible copies remain; avoid a second large settings context on the application stack.
    const M::Status prepared = L::prepareDriverSettings(output.settings, reservation, current, prior,
        state, control, settings, operation, now, deadline, options, age);
    if (!prepared)
        return prepared;
    output.original = original;
    output.identity = observed;
    output.review = review;
    return M::Ok();
}
// Prompt 61, decision 3: the backup record before a factory restore, its comparison and typed candidates.
namespace {
constexpr uint32_t MOTION_RPM_MAXIMUM = 3000, MOTION_RAMP_MAXIMUM = 2000; // DriverSettings' typed ranges.
uint16_t groupBit(L::DriverGroup group) {
    return uint16_t(1u << unsigned(group));
}
BackupField field(uint16_t index, uint8_t subindex, uint32_t raw, BackupWrite write) {
    BackupField f;
    f.index = index;
    f.subindex = subindex;
    f.raw = raw;
    f.write = write;
    return f;
}
// The typed write a recorded value can use; NONE when no typed candidate can name it.
BackupWrite when(bool nameable, BackupWrite write) {
    return nameable ? write : BackupWrite::NONE;
}
// The group's fields, in source order, from its typed getter; false when the getter refuses.
M::Status groupFields(L::DriverGroup group, const L::DriverContext& read, const L::ReadTarget& current,
                      uint64_t now, uint64_t age, BackupField* out, uint8_t& count) {
    count = 0;
    if (group == L::DriverGroup::IO_INPUT_CONFIGURATION) {
        L::IoObservation io;
        const M::Status ok = L::getIo(read, current, now, age, io);
        if (!ok)
            return ok;
        if (io.group != group)
            return error(M::ReadError::ARGUMENT, "the read is another group");
        out[count++] = field(0x2400, 0, io.ioEntries, BackupWrite::NONE);
        out[count++] = field(0x2400, 1, io.inputPolarityRaw, BackupWrite::IO);
        for (uint8_t i = 0; i < 7; ++i)
            out[count++] = field(0x2400, uint8_t(2 + i), io.inputFunctionRaw[i],
                                 when(io.inputFunctionRaw[i] <= uint16_t(L::InputFunction::USER6), BackupWrite::IO));
        return M::Ok();
    }
    if (group == L::DriverGroup::CONTROL) {
        L::ControlObservation c;
        const M::Status ok = L::getControl(read, current, now, age, c);
        if (!ok)
            return ok;
        out[count++] = field(0x2500, 0, c.loopModeRaw, when(c.loopMode != L::LoopMode::UNKNOWN, BackupWrite::CONTROL));
        out[count++] = field(0x2501, 0, c.encoderResolutionRaw, BackupWrite::NONE);
        return M::Ok();
    }
    L::DriverObservation d;
    const M::Status ok = L::getDriver(read, current, now, age, d);
    if (!ok)
        return ok;
    if (d.group != group)
        return error(M::ReadError::ARGUMENT, "the read is another group");
    if (group == L::DriverGroup::DRIVER) {
        out[count++] = field(0x2200, 0, d.directionRaw, BackupWrite::NONE);
        out[count++] = field(0x2201, 0, d.subdivisionRaw,
                             when(d.subdivisionRaw >= L::SUBDIVISION_MINIMUM && d.subdivisionRaw <= L::SUBDIVISION_MAXIMUM,
                                  BackupWrite::SUBDIVISION));
    } else if (group == L::DriverGroup::SOFT_LIMITS) {
        out[count++] = field(0x2202, 0, d.softLimitSelectorRaw, when(d.softLimitSelectorRaw <= 1, BackupWrite::SOFT_LIMIT));
        out[count++] = field(0x607d, 0, d.softLimitEntries, BackupWrite::NONE);
        out[count++] = field(0x607d, 1, uint32_t(d.minimumNative), BackupWrite::NONE);
        out[count++] = field(0x607d, 2, uint32_t(d.maximumNative), BackupWrite::NONE);
    } else {
        out[count++] = field(0x6081, 0, d.positionSpeedRpm, when(d.positionSpeedRpm <= MOTION_RPM_MAXIMUM, BackupWrite::MOTION));
        out[count++] = field(0x6083, 0, d.rampUpTimeMs, when(d.rampUpTimeMs <= MOTION_RAMP_MAXIMUM, BackupWrite::MOTION));
        out[count++] = field(0x6084, 0, d.rampDownTimeMs, when(d.rampDownTimeMs <= MOTION_RAMP_MAXIMUM, BackupWrite::MOTION));
    }
    return M::Ok();
}
const BackupField* find(const PersistenceBackup& record, uint16_t index, uint8_t subindex) {
    for (uint8_t i = 0; i < record.fieldCount && i < BACKUP_FIELDS; ++i)
        if (record.fields[i].index == index && record.fields[i].subindex == subindex)
            return &record.fields[i];
    return nullptr;
}
// MOTION and IO candidates are whole: every recorded field of that write must be nameable and read again.
bool wholeCandidate(const PersistenceBackup& backup, const PersistenceBackup& after, BackupWrite write) {
    const uint16_t index = write == BackupWrite::MOTION ? 0x6081 : 0x2400;
    const uint8_t first = write == BackupWrite::MOTION ? 0 : 1, last = write == BackupWrite::MOTION ? 0 : 8;
    const uint16_t indices[3] = {0x6081, 0x6083, 0x6084};
    for (uint8_t k = 0; k < (write == BackupWrite::MOTION ? 3 : last - first + 1); ++k) {
        const uint16_t at = write == BackupWrite::MOTION ? indices[k] : index;
        const uint8_t sub = write == BackupWrite::MOTION ? 0 : uint8_t(first + k);
        const BackupField* b = find(backup, at, sub);
        if (!b || b->write != write || !find(after, at, sub))
            return false;
    }
    return true;
}
} // namespace
M::Status recordBackupGroup(PersistenceBackup& backup, uint32_t recordId, L::DriverGroup group,
                            const L::DriverContext& read, const L::ReadTarget& current, uint64_t nowUs,
                            uint64_t maximumAgeUs) noexcept {
    if (overlaps(&backup, sizeof backup, &read, sizeof read) ||
        overlaps(&backup, sizeof backup, &current, sizeof current))
        return error(M::ReadError::ARGUMENT, "backup storage overlaps an input");
    const unsigned g = unsigned(group);
    if (!recordId || (backup.recordId && backup.recordId != recordId) || g > 15 ||
        !(BACKUP_GROUPS_SUPPORTED & groupBit(group)) || (backup.groupsRead & groupBit(group)))
        return error(M::ReadError::ARGUMENT, "a nonzero record and a supported group not yet recorded required");
    if (backup.recordId && !sameTarget(backup.target, current))
        return error(M::ReadError::TARGET, "one record holds one route and its epochs");
    BackupField fields[9];
    uint8_t count = 0;
    const M::Status ok = groupFields(group, read, current, nowUs, maximumAgeUs, fields, count);
    if (!ok)
        return ok;
    if (backup.fieldCount + count > BACKUP_FIELDS)
        return error(M::ReadError::ARGUMENT, "backup record full");
    if (!backup.recordId) {
        backup.recordId = recordId;
        backup.capturedUs = nowUs;
        backup.target = current;
    }
    for (uint8_t i = 0; i < count; ++i)
        backup.fields[backup.fieldCount++] = fields[i];
    backup.groupsRead = uint16_t(backup.groupsRead | groupBit(group));
    backup.groupsUnknown = uint16_t(BACKUP_GROUPS_ALL & ~backup.groupsRead);
    backup.unknown = BACKUP_UNKNOWN_ALL; // The complete set, the factory values and other objects stay unknown.
    backup.complete = false;
    return M::Ok();
}
M::Status compareBackup(BackupComparison& output, const PersistenceBackup& backup,
                        const PersistenceBackup& after) noexcept {
    if (overlaps(&output, sizeof output, &backup, sizeof backup) ||
        overlaps(&output, sizeof output, &after, sizeof after))
        return error(M::ReadError::ARGUMENT, "comparison storage overlaps an input");
    if (!backup.recordId || !after.recordId || backup.recordId == after.recordId ||
        backup.fieldCount > BACKUP_FIELDS || after.fieldCount > BACKUP_FIELDS)
        return error(M::ReadError::ARGUMENT, "two distinct nonzero records required");
    BackupComparison c;
    c.backupRecord = backup.recordId;
    c.afterRecord = after.recordId;
    c.fieldCount = backup.fieldCount;
    c.groupsUnknown = backup.groupsUnknown;
    for (uint8_t i = 0; i < backup.fieldCount; ++i) {
        const BackupField& b = backup.fields[i];
        const BackupField* a = find(after, b.index, b.subindex);
        BackupChange& out = c.change[i];
        if (!a)
            out = BackupChange::NOT_REREAD;
        else if (a->raw == b.raw)
            out = BackupChange::UNCHANGED;
        else if (b.write == BackupWrite::NONE)
            out = BackupChange::CHANGED;
        else if ((b.write == BackupWrite::MOTION || b.write == BackupWrite::IO) &&
                 !wholeCandidate(backup, after, b.write))
            out = BackupChange::CHANGED;
        else if (b.index == 0x2400 && b.subindex == 1 && ((a->raw ^ b.raw) & ~uint32_t(L::INPUT_POLARITY_BITS)))
            out = BackupChange::CHANGED; // prepareIoSettings keeps the current high bits: it cannot restore them.
        else
            out = BackupChange::REAPPLY;
        ++(out == BackupChange::UNCHANGED ? c.unchanged
           : out == BackupChange::REAPPLY ? c.reapply
           : out == BackupChange::CHANGED ? c.changed
                                          : c.notReread);
    }
    output = c;
    return M::Ok();
}
M::Status prepareBackupReapply(BackupReapply& output, const PersistenceBackup& backup,
                               const BackupComparison& comparison) noexcept {
    if (overlaps(&output, sizeof output, &backup, sizeof backup) ||
        overlaps(&output, sizeof output, &comparison, sizeof comparison))
        return error(M::ReadError::ARGUMENT, "candidate storage overlaps an input");
    if (!backup.recordId || comparison.backupRecord != backup.recordId || comparison.fieldCount != backup.fieldCount ||
        backup.fieldCount > BACKUP_FIELDS)
        return error(M::ReadError::ARGUMENT, "the comparison of this backup required");
    BackupReapply r;
    for (uint8_t i = 0; i < backup.fieldCount; ++i) {
        if (comparison.change[i] != BackupChange::REAPPLY)
            continue;
        const BackupField& b = backup.fields[i];
        r.writes = uint8_t(r.writes | 1u << unsigned(b.write));
        switch (b.write) {
        case BackupWrite::MOTION:
            // The whole candidate from the backup: compareBackup admitted REAPPLY only with all three present.
            r.motion.positionSpeedRpm = find(backup, 0x6081, 0)->raw;
            r.motion.rampUpTimeMs = find(backup, 0x6083, 0)->raw;
            r.motion.rampDownTimeMs = find(backup, 0x6084, 0)->raw;
            break;
        case BackupWrite::SOFT_LIMIT:
            r.softLimit.enable = static_cast<L::SoftLimitEnable>(b.raw);
            break;
        case BackupWrite::SUBDIVISION:
            r.subdivision.subdivisionRaw = uint16_t(b.raw);
            break;
        case BackupWrite::IO:
            r.io.invertedInputs = uint16_t(find(backup, 0x2400, 1)->raw & L::INPUT_POLARITY_BITS);
            for (uint8_t k = 0; k < 7; ++k)
                r.io.functions[k] = static_cast<L::InputFunction>(find(backup, 0x2400, uint8_t(2 + k))->raw);
            break;
        case BackupWrite::CONTROL:
            r.control.loopMode = static_cast<L::LoopMode>(b.raw);
            break;
        case BackupWrite::NONE:
            break;
        }
    }
    output = r;
    return M::Ok();
}
} // namespace MotorControlCANExample
