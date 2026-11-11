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
/** Application authorization of one download at the owner-to-adapter handoff.
 * Missing authorization refuses ordinary downloads. The dispatcher must match an exact
 * pending workflow/sequence/transfer before its required notice and preflight check.
 * A successful check does not establish validity at a later deferred SDK submission. */
struct DownloadPreflight {
    void* context = nullptr; ///< Borrowed; outlives the registration.
    /** False withdraws the download: the owner closes it PREFLIGHT_REJECTED, provably unsent.
     * Runs inside the owner's turn: only snapshot getters may be consulted; never
     * reenter owner/service progression, admission, cancellation or configuration. */
    bool (*check)(void*, MotorControlCANOwner::SequenceId, const MotorControlCAN::CANOPEN::SdoTransfer&,
                  uint64_t) = nullptr;
};
/** Chip whose shared console and boot use fixed GPIOs; selects the pin reservations.
 * The values follow EspProbePlatform: S2 owns UART0, S3 owns USB Serial/JTAG.
 */
enum class FirmwareTarget : uint8_t {
    UNSPECIFIED, ///< No reservation applied; the caller owns console and system pin exclusion.
    ESP32S2,     ///< UART0 console on GPIO43 TX / GPIO44 RX (pinned SDK soc/uart_pins.h).
    ESP32S3      ///< USB Serial/JTAG console on GPIO19 D- / GPIO20 D+ (soc/io_mux_reg.h).
};
/** @param target Selected chip policy.
 * @param gpio Any integer; negative values are never console pins.
 * @return True when gpio belongs to that target's shared console and cannot carry CAN
 * or transceiver control. Pure; usable in board static_asserts. GPIO capability
 * (existence, output support) is the SDK's check, not this function's. FirmwareBoard.h
 * checks these literals against the pinned SDK constants in each real target build.
 */
constexpr bool firmwareConsolePin(FirmwareTarget target, int gpio) noexcept {
    return target == FirmwareTarget::ESP32S2   ? gpio == 43 || gpio == 44
           : target == FirmwareTarget::ESP32S3 ? gpio == 19 || gpio == 20
                                               : false;
}
/** Pins the chip drives itself, from reset and outside this firmware's control.
 * @param target Selected chip policy.
 * @param gpio Any integer; negative values are never system pins.
 * @return True for GPIO26..32 on both targets, the MSPI flash/PSRAM bus that the ROM,
 * bootloader and cache use from reset onwards (pinned SDK soc/spi_pins.h), and on S3
 * for GPIO43, U0TXD, where the ROM prints boot messages unless eFuse
 * UART_PRINT_CONTROL disables them (pinned esp_efuse_table.csv; the default enables
 * them). S2's U0TXD is already its console pin. Either use would put the chip's own
 * traffic on the CAN TX line or the silent input, or let it contend with the
 * transceiver's RXD output, before any firmware check runs; so every role is refused.
 * UNSPECIFIED reserves nothing. Pure; FirmwareBoard.h checks the literals against the
 * SDK constants. Strapping pins are not covered: their reset levels are board facts.
 */
constexpr bool firmwareSystemPin(FirmwareTarget target, int gpio) noexcept {
    return target != FirmwareTarget::UNSPECIFIED &&
           ((gpio >= 26 && gpio <= 32) || (target == FirmwareTarget::ESP32S3 && gpio == 43));
}
/** Level of the transceiver mode input that suppresses bus transmission. */
enum class SilentLevel : uint8_t {
    NONE, ///< No application-controlled silent input; transmission follows controller start.
    HIGH, ///< Driving the pin high requests silence (TJA1051 S: high = TX off, RX kept).
    LOW   ///< Driving the pin low requests silence.
};
/** Application-owned transceiver silent input; both fields are set together or neither. */
struct TransceiverConfig {
    int silentPin = -1;                        ///< GPIO wired to the silent input; -1 none.
    SilentLevel silentLevel = SilentLevel::NONE; ///< Board fact: which level means silent.
};
/** Application-owned GPIO used for the silent input; the core and adapters never drive it. */
struct SilentPin {
    void* context = nullptr; ///< Stationary; outlives the assembly.
    /** Latch the level, then enable the output. True only when every SDK call accepted it.
     * The result is a requested pin level, never a measured bus or transceiver mode. */
    bool (*drive)(void* context, int gpio, bool high) = nullptr;
};
/** Explicit local controller configuration; default leaves CAN disabled.
 * No node, drive, model, firmware, session or physical presence is inferred.
 */
struct FirmwareConfig {
    bool configured = false; ///< Explicit application confirmation of controller configuration.
    FirmwareTarget target = FirmwareTarget::UNSPECIFIED; ///< Console pin policy to apply.
    TwaiConfig controller;   ///< Pins, bitrate, queue depths and timing; no usable defaults.
    TransceiverConfig transceiver; ///< Optional silent input; NONE keeps the transceiver uncontrolled.
};
/** Board-level reason a configuration is refused; controller timing/queue values are
 * checked separately by TwaiAdapter and the SDK when the controller starts. */
enum class FirmwareConfigCheck : uint8_t {
    OK,              ///< Pins are present, distinct and outside the target's reserved pins.
    NOT_CONFIGURED,  ///< configured is false: the compile fixture or a missing board fact.
    MISSING_PIN,     ///< TX or RX GPIO is negative.
    CONSOLE_PIN,     ///< TX, RX or the silent pin is a console pin of the selected target.
    SYSTEM_PIN,      ///< TX, RX or the silent pin is a flash or boot-output pin (firmwareSystemPin).
    PIN_CONFLICT,    ///< TX equals RX, or the silent pin equals TX or RX.
    SILENT_INCOMPLETE, ///< Only one of silentPin/silentLevel is set, or no drive callback exists.
    INVALID_PLATFORM ///< prepare only: platform callbacks lack nowUs or supply idle.
};
/** Validate board facts only; performs no I/O. A given silent pin (>= 0) is checked for
 * console, system and TX/RX conflicts even when its level is missing. Pin capability
 * (existence, output support) is not checked here: FirmwareBoard.h checks it at compile
 * time for the shared entries. A caller passing other pins at run time sees an SDK
 * refusal later, as TRANSCEIVER_FAILED or CONTROLLER_FAILED, not as a refused board.
 * @param config Candidate configuration.
 * @param silentDriver True when the assembly has an application GPIO callback.
 * @return First failing check in declaration order, or OK; never INVALID_PLATFORM.
 */
FirmwareConfigCheck checkFirmwareConfig(const FirmwareConfig& config, bool silentDriver) noexcept;
/** Last requested level of the application-owned silent input. "Requested" means the
 * SDK accepted the GPIO calls; no state proves that the bus or transceiver is silent. */
enum class TransceiverState : uint8_t {
    NOT_CONTROLLED,   ///< No silent input configured, or prepare has not run.
    SILENT_REQUESTED, ///< Silent level accepted; the controller may or may not be running.
    NORMAL_REQUESTED, ///< Normal level accepted after the controller started: TX is possible.
    FAILED            ///< A drive call failed or the callback refused the pin; the level is
                      ///< unknown. The controller is then not running; it is retired only
                      ///< if its shutdown succeeded (FirmwareProbe::retired).
};
/** Local startup and progression diagnostics, never drive or communication health.
 * After a refused prepare, any start that did not run, any stop and any progression
 * failure, the controller never runs again in this boot and the owner refuses new work.
 */
enum class FirmwareBootStatus : uint8_t {
    NOT_STARTED,     ///< No task creation attempted.
    TASK_FAILED,     ///< Static task could not be created; controller was not initialized.
    PLATFORM_FAILED, ///< Console or platform callbacks failed; startup opened no controller.
    CONFIGURATION_REQUIRED, ///< Compile fixture; console available, CAN disabled.
    CONFIGURATION_REJECTED, ///< Board facts refused (FirmwareProbe::lastCheck); no GPIO or controller.
    TRANSCEIVER_FAILED,     ///< Silent level refused; controller never started. Wins over
                            ///< PLATFORM_FAILED: the pin level is unknown.
    INACTIVE,          ///< Board accepted, silent level requested if configured; awaits explicit start.
    CONTROLLER_FAILED, ///< Explicit start failed and its controller shutdown was confirmed.
    CONTROLLER_UNRETIRED, ///< Explicit start failed and its controller shutdown was refused: an SDK
                          ///< node may remain; only the adapter and requested silence inhibit TX.
    RUNNING,           ///< Explicit start succeeded; no selected drive/request implied.
    STOPPED,           ///< Explicit stop confirmed: silence accepted (if controlled), controller retired.
    STOP_FAILED,       ///< Explicit stop could not confirm silence, invalidation or retirement.
    PROGRESSION_FAILED ///< Clock/progression failure inhibited work and attempted retirement.
                       ///< Stays the status after a later stop; see faultRetired and retired.
};
/** One stationary assembly; the borrowed service store remains the sole node table.
 * Construction performs no I/O. prepare validates the board and requests a configured
 * transceiver silent; it never starts the controller. start is the explicit step that
 * opens the local controller and only then requests the normal transceiver level.
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
     * @param silent Copied application GPIO for a configured transceiver silent input;
     * default none. A configuration naming a silent pin is refused without it.
     */
    FirmwareProbe(MotorControlCAN::CANOPEN::SdoClient& services, TwaiDriver driver,
                  const ProbeCallbacks& callbacks, const SilentPin& silent = SilentPin()) noexcept;
    FirmwareProbe(const FirmwareProbe&) = delete; ///< Stationary callback/handle ownership.
    FirmwareProbe& operator=(const FirmwareProbe&) = delete; ///< Cannot move live state.
    /** Passive startup: validate board facts and request a configured transceiver silent.
     * One attempt per lifetime. Starts no controller and sends nothing.
     * @param config Copied on success. Target console and system pins, missing/equal pins
     * and an incomplete silent input are refused (see lastCheck) without any GPIO or
     * driver call.
     * @return True when the configuration is accepted and, if a silent input exists,
     * its silent level was accepted. A refused silent level leaves TransceiverState::FAILED.
     * On false the controller is never started by this assembly, and the owner is
     * invalidated at its last time so that it refuses new work instead of holding it.
     */
    bool prepare(const FirmwareConfig& config) noexcept;
    /** Explicit start after a successful prepare; one attempt, never a device request.
     * Opens the local controller first, then requests the normal transceiver level.
     * @return True only when both succeeded. Otherwise retires as stop does, once: a
     * failed controller start keeps the silent level requested; a refused normal level
     * requests silence again first. Then the owner is invalidated at its last time (it
     * refuses new work and settles what it held) and the controller shutdown is attempted
     * once; retired reports whether it was confirmed. No automatic retry; a later start
     * returns false.
     */
    bool start() noexcept;
    /** prepare then start, for callers that open the controller in one explicit step.
     * @param config Same contract as prepare.
     * @return True only after both succeeded.
     */
    bool begin(const FirmwareConfig& config) noexcept { return prepare(config) && start(); }
    /** Explicit local stop: request silence, then retire the controller as the failure
     * paths do: owner invalidation at its last time unless an earlier path already
     * confirmed it, and a shutdown attempt unless an earlier one was confirmed. One
     * attempt; nothing restarts. Valid after prepare: before start, after a refused
     * start and after a progression failure as well as while running.
     * Local stop is never a motor stop and settles no remote effect.
     * @return True when silence was accepted (or no silent input exists), the owner is
     * invalidated, and the controller is retired or never ran. False keeps the reported
     * uncertainty; see stopConfirmed and retired.
     */
    bool stop() noexcept;
    /** Acquire checked monotonic microseconds and perform one shared service turn.
     * @return True for a valid running turn. False drains bounded platform I/O
     * only when not running. A failed/backward clock or rejected app turn latches
     * failure, requests a configured transceiver silent, invalidates owner evidence
     * at the last valid time and attempts controller shutdown once. No guessed time or
     * automatic restart is used.
     * Caller invokes its scheduling idle after this call, including failure.
     */
    // Inactive turns progress only bounded host recovery and platform I/O; recovery
    // fails without starting/restarting a controller, including after explicit stop.
    bool service() noexcept;
    /** @return Local controller/application progression only; never drive readiness. */
    bool running() const noexcept { return running_; }
    /** @return Reason of the last prepare refusal; OK before prepare or after acceptance. */
    FirmwareConfigCheck lastCheck() const noexcept { return check_; }
    /** @return Last requested silent-input level; never a measured bus state. */
    TransceiverState transceiver() const noexcept { return transceiver_; }
    /** @return True after the one explicit start attempt, whatever its result. */
    bool started() const noexcept { return started_; }
    /** @return True after an explicit stop attempt. */
    bool stopped() const noexcept { return stopped_; }
    /** @return The result of that stop attempt; false before stop. */
    bool stopConfirmed() const noexcept { return stopConfirmed_; }
    /** @return True after a prepare that accepted the board and any silent level. */
    bool prepared() const noexcept { return prepared_; }
    /** @return True while no local controller resources are outstanding: start never ran,
     * or the last shutdown attempt (start failure, progression failure or stop) was
     * confirmed. False while running and after a refused shutdown, whose SDK node and
     * TX capability stay uncertain. Never a measured bus state.
     */
    bool retired() const noexcept { return retired_; }
    /** @return Latched failure of supplied time or progression, independent of USB pressure. */
    bool progressionFailed() const noexcept { return failure_ != FirmwareFailure::NONE; }
    /** @return Which step latched the failure; NONE while progressing. */
    FirmwareFailure failure() const noexcept { return failure_; }
    /** @return Failure invalidation and local TX retirement both confirmed by the failure
     * path itself. Meaningful only after progressionFailed; false retains the uncertainty
     * of the interval after the failure even when a later stop confirms retirement (then
     * retired and stopConfirmed are true).
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
    /** @return The borrowed sole service store, for explicit route configuration. */
    MotorControlCAN::CANOPEN::SdoClient& services() noexcept { return services_; }
    /** Register exact download authorization at owner-to-adapter handoff.
     * Call between service turns. A null check refuses ordinary downloads; the owner's
     * explicitly unsequenced best-effort stop retains its separate exemption.
     * @param preflight Copied; its context outlives the registration. */
    void setPreflight(const DownloadPreflight& preflight) noexcept { preflight_ = preflight; }

  private:
    // The owner holds one context for every transport callback, so it gets this assembly and
    // each callback forwards to the adapter's own. Only beforeTransmit adds anything.
    MotorControlCANOwner::Transport ownerTransport() noexcept;
    bool controlled() const noexcept { return config_.transceiver.silentLevel != SilentLevel::NONE; }
    bool requestSilent(bool silent) noexcept;
    bool invalidate() noexcept;
    bool retire() noexcept;
    MotorControlCAN::CANOPEN::SdoClient& services_;
    TwaiAdapter transport_;
    MotorControlCANOwner::Transport adapter_; // The adapter's callbacks, copied once.
    DownloadPreflight preflight_;
    SilentPin silent_;
    FirmwareConfig config_;
    CanBusOwner owner_;
    ProbePlatform platform_;
    ProbeApp app_;
    DeviceEvidence evidence_;
    uint64_t lastUs_ = 0;
    bool callbacksValid_ = false, attempted_ = false, prepared_ = false, started_ = false;
    bool running_ = false, stopped_ = false, stopConfirmed_ = false, faultRetired_ = false;
    bool retired_ = true;      // No controller resources outstanding.
    bool invalidated_ = false; // The owner accepted the final RECOVERING report.
    FirmwareConfigCheck check_ = FirmwareConfigCheck::OK;
    TransceiverState transceiver_ = TransceiverState::NOT_CONTROLLED;
    FirmwareFailure failure_ = FirmwareFailure::NONE;
};
/** Status projection used by the shared firmware task after every turn; pure, no I/O.
 * Explicit outcomes win over startup facts, so a hook that starts the controller after a
 * console failure reports RUNNING rather than PLATFORM_FAILED. Among startup facts a
 * refused silent level wins over a console failure, because its pin level is unknown.
 * @param firmware Assembly after its one prepare attempt.
 * @param platformReady Whether the console platform started.
 * @return Never NOT_STARTED or TASK_FAILED; those belong to task creation.
 */
FirmwareBootStatus firmwareStatus(const FirmwareProbe& firmware, bool platformReady) noexcept;
/** Optional explicit application work run by the shared firmware task after each turn.
 * The callee runs on the owner task only, may call the assembly's public API (start, stop,
 * app, owner, platform, services) and must stay bounded. No current production entry
 * supplies one; the finite shared-image check in FirmwareSmoke is its only caller.
 */
struct FirmwareTurnHook {
    void* context = nullptr; ///< Stationary; outlives the firmware task.
    void (*turn)(void* context, FirmwareProbe& firmware) = nullptr; ///< Null runs nothing.
};
} // namespace MotorControlCANExample
