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
} // namespace MotorControlCANExample
