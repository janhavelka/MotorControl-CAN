/** @file Discovery.h
 * @brief Static implemented-profile inventory; no I/O, allocation or automatic selection.
 * SPDX-License-Identifier: MIT
 */
#pragma once
#include <MotorControlCAN/Endpoint.h>
#include <cstddef>

namespace MotorControlCAN {
/** Manufacturers with an implemented profile; surveyed products are not entries. */
enum class Manufacturer : uint8_t {
    LICHUAN ///< Manufacturer with the implemented C-family profile.
};
/** Implemented software family, distinct from application-assigned Endpoint.profile. */
enum class DriveProfile : uint8_t {
    LICHUAN_C ///< Implemented Lichuan C software family, not a detected model.
};
/** Static catalogue entry. All strings have static lifetime; not a detected identity. */
struct DiscoveryProfile {
    Manufacturer manufacturer = Manufacturer::LICHUAN; ///< Inventory identity only.
    DriveProfile profile = DriveProfile::LICHUAN_C;    ///< Implemented family selector.
    Protocol protocol = Protocol::CANOPEN;             ///< Selected native protocol domain.
    const char* manufacturerId = "lichuan";            ///< Stable inventory token.
    const char* manufacturerName = "Lichuan";          ///< Static display name.
    const char* profileId = "lichuan_c";   ///< Stable inventory token, not a wire value.
    const char* profileName = "Lichuan C"; ///< Software family, not model qualification.
};
/** Acquisition support only; no field grants a write, drive action or hardware qualification. */
struct DiscoveryCapabilities {
    bool probe = false;          ///< Minimal presence upload implemented.
    bool identity = false;       ///< Explicit raw identity acquisition implemented.
    bool nonChanging = false;    ///< False: DEC-1 accepts unknown read effects.
    bool exactModel = false;     ///< False: no unique model mapping is established.
    bool firmware = false;       ///< False: acquisition does not establish firmware support.
    uint32_t minimumAddress = 0; ///< Profile-local CANopen addressed-node lower bound.
    uint32_t maximumAddress = 0; ///< Profile-local bound, not a generic axis restriction.
    uint16_t probeIndex = 0;     ///< Profile's minimal SDO object, never a write route.
    uint8_t probeSubindex = 0;   ///< Exact minimal object subindex.
    uint8_t probeWidthBytes = 0; ///< Reviewed upload width.
};
/** @return Number of implemented profiles; bounded constant query with no I/O. */
std::size_t discoveryProfileCount() noexcept;
/** Copy static inventory metadata atomically.
 * @param[in] index Zero-based inventory index.
 * @param[out] output Unchanged for an invalid index. Strings remain static-lifetime.
 * @return Local status only; no detected device or capability permission.
 */
Status getDiscoveryProfile(std::size_t index, DiscoveryProfile& output) noexcept;
/** Copy implemented acquisition capabilities, independent of device presence.
 * @param[in] profile Implemented family selector, not Endpoint.profile.
 * @param[out] output Unchanged for unsupported selectors.
 * @return Local status; no clock, I/O, writes or hardware qualification.
 */
Status getDiscoveryCapabilities(DriveProfile profile, DiscoveryCapabilities& output) noexcept;
} // namespace MotorControlCAN
