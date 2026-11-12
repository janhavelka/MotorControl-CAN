/** @file Capacity.h
 * @brief Compile-time capacity set of the optional application adapters and examples.
 * SPDX-License-Identifier: MIT
 *
 * Not installed core: the portable core has no axis, node or channel capacity. These values size
 * caller-owned adapter and example objects (`SdoClient`, `CanBusOwner`, `TwaiAdapter`, `ProbeApp`,
 * `DiscoveryPlan`), which every application compiles from source. Nothing here allocates: the
 * application decides where those objects live (static internal RAM, a startup heap block or
 * PSRAM).
 *
 * One program uses one capacity set. Override a value only in the build of the whole application,
 * never on a single target or source file:
 * - CMake: cache variables of the same names (`adapters/canopennode/CMakeLists.txt`), applied as
 *   PUBLIC compile definitions of `MotorControlCAN::CanopenNode`, so the owner, the TWAI adapter,
 *   the examples and the tests inherit them;
 * - ESP-IDF: the integers in the application's `Kconfig.projbuild`
 *   (`adapters/canopennode/Kconfig.capacity`), applied to the application component that compiles
 *   the adapter sources;
 * - PlatformIO: `build_flags = -DMOTORCONTROLCAN_SDO_CHANNELS=8`, which reach every source of the
 *   environment.
 *
 * The defaults reproduce the two-channel layout of earlier releases byte for byte. Each limit is
 * checked here by `static_assert`. Translation units compiled with different values refuse to
 * link on GCC and Clang (CapacityLinkTag) instead of running with different object layouts.
 *
 * Optional macros, each derived from the channel count when left undefined:
 * - `MOTORCONTROLCAN_OWNER_REQUESTS`: shared ordinary owner request/result slots. Default
 *   max(6, channels + 4), so each channel can hold one queued request with four to spare. At
 *   least 1, with channels + requests at most 255 (`RequestId::slot` is 8-bit).
 * - `MOTORCONTROLCAN_OWNER_PRODUCERS`: owner fairness producers, application identities
 *   1..PRODUCERS. Default max(4, channels + 2), so each session can have its own. Range 1..255.
 * - `MOTORCONTROLCAN_DISCOVERY_TARGETS`: targets of one `DiscoveryPlan`. Default min(channels, 2):
 *   a larger scan runs in batches over the configured channels, so no channel carries a retained
 *   finding it does not need. Range 1..channels and at most 28, because an identity scan needs
 *   nine services per target in an 8-bit budget.
 *
 * A larger value is a storage count, not a timing or bus qualification. The TWAI adapter submits
 * at most one SDK frame per service turn, so stopping N axes needs at least N turns, and SDO
 * monitoring of many axes is bounded by that host TX rate and by bus time (docs/can_owner.md).
 */
#pragma once

#ifndef MOTORCONTROLCAN_SDO_CHANNELS
/** SDO channels on one physical interface, one per drive axis route. Default 2.
 * Sizes `SdoClient::CHANNELS`, and with it the owner's reservations (`SEQUENCES`) and urgent
 * slots (`URGENT_REQUESTS`), the TWAI TX slots (`CHANNELS + 1`) and the `ProbeApp` sessions.
 * Range 1 and up, with channels + `MOTORCONTROLCAN_OWNER_REQUESTS` at most 255.
 */
#define MOTORCONTROLCAN_SDO_CHANNELS 2
#endif
#ifndef MOTORCONTROLCAN_PDO_ROUTES
/** Permanently bound PDO wire routes on one interface (`SdoClient::PDO_ROUTES`), independent of
 * the channel count. Default 8, range 1..255.
 */
#define MOTORCONTROLCAN_PDO_ROUTES 8
#endif

namespace MotorControlCAN {
namespace CANOPEN {
/** The capacity set this translation unit sees, mapped once from the macros above.
 * Adapter and example classes take their array bounds from here and nowhere else. Every value is
 * a count of fixed storage; none is a CANopen node number.
 */
struct AdapterCapacity {
    enum {
        SDO_CHANNELS = MOTORCONTROLCAN_SDO_CHANNELS, ///< SDO channels, one per axis route.
#ifdef MOTORCONTROLCAN_OWNER_REQUESTS
        OWNER_REQUESTS = MOTORCONTROLCAN_OWNER_REQUESTS, ///< Ordinary owner request/result slots.
#else
        OWNER_REQUESTS = SDO_CHANNELS + 4 > 6 ? SDO_CHANNELS + 4 : 6, ///< Ordinary owner slots.
#endif
#ifdef MOTORCONTROLCAN_OWNER_PRODUCERS
        OWNER_PRODUCERS = MOTORCONTROLCAN_OWNER_PRODUCERS, ///< Owner fairness producers.
#else
        OWNER_PRODUCERS = SDO_CHANNELS + 2 > 4 ? SDO_CHANNELS + 2 : 4, ///< Owner fairness producers.
#endif
        PDO_ROUTES = MOTORCONTROLCAN_PDO_ROUTES, ///< Bound PDO wire routes.
#ifdef MOTORCONTROLCAN_DISCOVERY_TARGETS
        DISCOVERY_TARGETS = MOTORCONTROLCAN_DISCOVERY_TARGETS ///< Targets of one discovery scan.
#else
        DISCOVERY_TARGETS = SDO_CHANNELS < 2 ? SDO_CHANNELS : 2 ///< Targets of one discovery scan.
#endif
    };
};
static_assert(int(AdapterCapacity::SDO_CHANNELS) >= 1, "MOTORCONTROLCAN_SDO_CHANNELS must be at least 1");
static_assert(int(AdapterCapacity::OWNER_REQUESTS) >= 1, "MOTORCONTROLCAN_OWNER_REQUESTS must be at least 1");
static_assert(int(AdapterCapacity::SDO_CHANNELS) + int(AdapterCapacity::OWNER_REQUESTS) <= 255,
              "SDO channels plus owner requests must fit the 8-bit request slot index");
static_assert(int(AdapterCapacity::OWNER_PRODUCERS) >= 1 && int(AdapterCapacity::OWNER_PRODUCERS) <= 255,
              "MOTORCONTROLCAN_OWNER_PRODUCERS must be 1..255");
static_assert(int(AdapterCapacity::PDO_ROUTES) >= 1 && int(AdapterCapacity::PDO_ROUTES) <= 255,
              "MOTORCONTROLCAN_PDO_ROUTES must be 1..255");
static_assert(int(AdapterCapacity::DISCOVERY_TARGETS) >= 1 &&
                  int(AdapterCapacity::DISCOVERY_TARGETS) <= int(AdapterCapacity::SDO_CHANNELS),
              "MOTORCONTROLCAN_DISCOVERY_TARGETS must be 1..channels");
static_assert(int(AdapterCapacity::DISCOVERY_TARGETS) * 9 <= 255,
              "An identity scan of every discovery target must fit its 8-bit service budget");

/** Link-time capacity check. Its one specialization for the values above is defined in
 * SdoClient.cpp. Every translation unit that includes this header references the specialization
 * of the values it saw, so linking a unit built with other values fails with an undefined
 * reference that names that unit's set, instead of running with two layouts of the same object.
 * This guards native builds that combine separately compiled static libraries. A linker that
 * discards unreferenced sections (`--gc-sections`, as ESP-IDF links) may drop the reference
 * unchecked; those builds compile every adapter source in one component with one set of values.
 * @tparam Channels SDO channels. @tparam Requests Ordinary owner slots.
 * @tparam Producers Owner producers. @tparam PdoRoutes PDO routes.
 * @tparam DiscoveryTargets Targets of one discovery scan.
 */
template <unsigned Channels, unsigned Requests, unsigned Producers, unsigned PdoRoutes,
          unsigned DiscoveryTargets>
struct CapacityLinkTag {
    static void check(); ///< Defined only for the capacity set SdoClient.cpp was compiled with.
};
/** The capacity set of this translation unit, as a CapacityLinkTag specialization. */
typedef CapacityLinkTag<unsigned(AdapterCapacity::SDO_CHANNELS), unsigned(AdapterCapacity::OWNER_REQUESTS),
                        unsigned(AdapterCapacity::OWNER_PRODUCERS), unsigned(AdapterCapacity::PDO_ROUTES),
                        unsigned(AdapterCapacity::DISCOVERY_TARGETS)>
    ConfiguredCapacityTag;
extern template struct CapacityLinkTag<
    unsigned(AdapterCapacity::SDO_CHANNELS), unsigned(AdapterCapacity::OWNER_REQUESTS),
    unsigned(AdapterCapacity::OWNER_PRODUCERS), unsigned(AdapterCapacity::PDO_ROUTES),
    unsigned(AdapterCapacity::DISCOVERY_TARGETS)>;
/// @cond INTERNAL
namespace {
// Emitted in every unit, though nothing reads it, so the reference reaches the linker.
#if defined(__GNUC__) || defined(__clang__)
__attribute__((used))
#endif
void (*const capacityLinkReference)() = &ConfiguredCapacityTag::check;
} // namespace
/// @endcond
} // namespace CANOPEN
} // namespace MotorControlCAN
