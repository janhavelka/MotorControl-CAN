/** @file FirmwareProbe.h
 * @brief Shared firmware assembly around the existing application and CAN owner.
 * SPDX-License-Identifier: MIT
 */
#pragma once
#include "DeviceEvidence.h"
#include "ProbeApp.h"
#include "TwaiAdapter.h"

namespace MotorControlCANExample {
using MotorControlCANOwner::CanBusOwner;
/** Explicit local controller configuration; default leaves CAN disabled.
 * Applies to the S3 USB Serial/JTAG examples, which reserve GPIO19/20 for USB.
 * No node, drive, model, firmware, session or physical presence is inferred.
 */
struct FirmwareConfig {
    bool configured = false; ///< Explicit application confirmation of controller configuration.
    TwaiConfig controller;   ///< Pins, bitrate, queue depths and timing; no usable defaults.
};
/** One stationary assembly; the borrowed service store remains the sole node table.
 * Construction performs no I/O. Explicit begin opens only the local controller.
 * Service acquires supplied platform time, runs ProbeApp/owner, then TWAI.
 * Platform callbacks must leave idle null: scheduling occurs after both services.
 * No selection, request, parser, automatic recovery or motion is introduced.
 */
class FirmwareProbe {
  public:
    /** Borrow existing application resources; callbacks never reenter the owner.
     * @param services Sole stationary service store, outlives this assembly.
     * @param driver Copied controller callbacks and stationary borrowed context.
     * @param callbacks Copied platform callbacks; nowUs required and idle must be null.
     */
    FirmwareProbe(MotorControlCAN::CANOPEN::SdoClient& services, TwaiDriver driver,
                  const ProbeCallbacks& callbacks) noexcept;
    FirmwareProbe(const FirmwareProbe&) = delete; ///< Stationary callback/handle ownership.
    FirmwareProbe& operator=(const FirmwareProbe&) = delete; ///< Cannot move live state.
    /** One explicit initialization attempt, never a device request.
     * @param config Confirmed local controller facts; GPIO19/20 cannot be CAN pins.
     * @return True only after local initialization. Missing/invalid configuration
     * calls no driver. Failure requires explicit retirement; no automatic retry.
     */
    bool begin(const FirmwareConfig& config) noexcept;
    /** Acquire checked monotonic microseconds and perform one shared service turn.
     * @return True for a valid running turn. False drains bounded platform I/O
     * only when not running. A failed/backward clock or rejected app turn latches
     * failure, invalidates owner evidence at the last valid time and attempts
     * controller shutdown once. No guessed time or automatic restart is used.
     * Caller invokes its scheduling idle after this call, including failure.
     */
    bool service() noexcept;
    /** @return Local controller/application progression only; never drive readiness. */
    bool running() const noexcept { return running_; }
    /** @return Latched failure of supplied time/progression, independent of USB pressure. */
    bool clockFailed() const noexcept { return clockFailed_; }
    /** @return Clock-failure invalidation and local TX retirement both confirmed.
     * Meaningful only after clockFailed; false retains local retirement uncertainty.
     */
    bool faultRetired() const noexcept { return faultRetired_; }
    /** @return Existing concrete application; calls are explicit, never startup actions. */
    ProbeApp& app() noexcept { return app_; }
    /** @return Same typed application evidence collector, fixed DEVICE provenance.
     * Requests require explicit fixture/authorization review; construction/startup
     * never selects a drive or starts acquisition. Service collects after ordinary work.
     */
    DeviceEvidence& evidence() noexcept { return evidence_; }
    /** @return Sole existing owner for explicit requests, recovery and retained results. */
    CanBusOwner& owner() noexcept { return owner_; }
    /** @return Bounded byte buffers/counters; no command parser exists. */
    ProbePlatform& platform() noexcept { return platform_; }
    /** @return Controller transport for diagnostics and explicit checked shutdown. */
    TwaiAdapter& transport() noexcept { return transport_; }

  private:
    TwaiAdapter transport_;
    CanBusOwner owner_;
    ProbePlatform platform_;
    ProbeApp app_;
    DeviceEvidence evidence_;
    uint64_t lastUs_ = 0;
    bool callbacksValid_ = false, attempted_ = false, running_ = false, clockFailed_ = false;
    bool faultRetired_ = false;
};
} // namespace MotorControlCANExample
