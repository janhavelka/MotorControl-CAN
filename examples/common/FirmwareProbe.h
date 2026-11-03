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
/** Which step of a firmware turn latched the failure. */
enum class FirmwareFailure : uint8_t {
    NONE,        ///< Still progressing.
    CLOCK,       ///< Failed or backward supplied time.
    APPLICATION, ///< ProbeApp service, which includes the owner turn, was rejected.
    EVIDENCE     ///< DeviceEvidence service was rejected.
};
/** Application check of one download just before the owner hands it over, forwarded from
 * Transport::beforeTransmit. AxisSession::preflight is the intended callee. */
struct DownloadPreflight {
    void* context = nullptr; ///< Borrowed; outlives the registration.
    /** False withdraws the download: the owner closes it PREFLIGHT_REJECTED, provably unsent.
     * Runs inside the owner's turn and must not call the owner. */
    bool (*check)(void*, MotorControlCANOwner::SequenceId, const MotorControlCAN::CANOPEN::SdoTransfer&,
                  uint64_t) = nullptr;
};
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
    /** @return Latched failure of supplied time or progression, independent of USB pressure. */
    bool progressionFailed() const noexcept { return failure_ != FirmwareFailure::NONE; }
    /** @return Which step latched the failure; NONE while progressing. */
    FirmwareFailure failure() const noexcept { return failure_; }
    /** @return Failure invalidation and local TX retirement both confirmed.
     * Meaningful only after progressionFailed; false retains local retirement uncertainty.
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
    /** Register the check the owner runs before each download leaves; a null check removes it.
     * Call between service turns. Without one, every download is handed over unchecked.
     * @param preflight Copied; its context outlives the registration. */
    void setPreflight(const DownloadPreflight& preflight) noexcept { preflight_ = preflight; }

  private:
    // The owner holds one context for every transport callback, so it gets this assembly and
    // each callback forwards to the adapter's own. Only beforeTransmit adds anything.
    MotorControlCANOwner::Transport ownerTransport() noexcept;
    TwaiAdapter transport_;
    MotorControlCANOwner::Transport adapter_; // The adapter's callbacks, copied once.
    DownloadPreflight preflight_;
    CanBusOwner owner_;
    ProbePlatform platform_;
    ProbeApp app_;
    DeviceEvidence evidence_;
    uint64_t lastUs_ = 0;
    bool callbacksValid_ = false, attempted_ = false, running_ = false;
    bool faultRetired_ = false;
    FirmwareFailure failure_ = FirmwareFailure::NONE;
};
} // namespace MotorControlCANExample
