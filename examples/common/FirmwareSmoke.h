/** @file FirmwareSmoke.h
 * @brief Test-only finite check of the shared firmware image: one operator trigger,
 * explicit start, then one typed identity read on each of two configured nodes.
 * SPDX-License-Identifier: MIT
 */
#pragma once
#include "FirmwareProbe.h"

namespace MotorControlCANExample {
/** Fixture facts for the check; every field is required and nothing has a default. */
struct SmokeConfig {
    uint8_t nodes[2] = {0, 0}; ///< Established CANopen node IDs 1..127, distinct; zero refuses.
    /** Nonzero caller budgets for each field; identityWidths as measured on the fixture. */
    MotorControlCAN::LICHUAN_C::ReadOptions options;
    uint32_t readWindowUs = 0; ///< Exclusive overall deadline of one node's read, from admission.
    const char* identity = nullptr; ///< Static image identity text printed with the start result.
};
/** Fixed finite phases; DONE is terminal for the image's lifetime. */
enum class SmokeStep : uint8_t {
    WAITING, ///< Passive: the ready line, then only the trigger byte; nothing is admitted.
    READING, ///< Explicit start succeeded; reading node index node().
    FINISH,  ///< Both results printed; the final marker is next.
    DONE     ///< Finished or refused; later input is counted and ignored.
};
/** One-shot owner-task hook. It is not a command parser: the single byte TRIGGER arms one
 * fixed sequence once per boot, and every other byte is only counted. Before it looks at any
 * input it prints `SMOKE ready <identity>` once, so the image is identified while it is still
 * passive (`SMOKE ready identity=unconfigured` without one). The sequence calls
 * FirmwareProbe::start, configures the two predefined-connection-set routes
 * (0x600/0x580/0x700/0x080 + node) as an explicit fixture premise, requests the existing
 * typed identity read through ProbeApp, consumes its terminals and prints the raw fields.
 * It never writes an object, retries, resets, selects a node at startup or reads without
 * the trigger. One line waits at a time for console room; the CAN owner never waits.
 */
class FirmwareSmoke {
  public:
    enum : uint8_t {
        TRIGGER = 'G', ///< The only byte that arms the check.
        LINE = 240     ///< Longest printed line in bytes, newline included.
    };
    /** @param config Copied; validated when the trigger arrives, never at construction. */
    explicit FirmwareSmoke(const SmokeConfig& config) noexcept;
    FirmwareSmoke(const FirmwareSmoke&) = delete;            ///< The hook borrows this object.
    FirmwareSmoke& operator=(const FirmwareSmoke&) = delete; ///< Stationary.
    /** @return Hook borrowing this stationary object for the firmware task's lifetime. */
    FirmwareTurnHook hook() noexcept;
    /** One bounded step on the owner task after FirmwareProbe::service.
     * @param firmware The shared assembly; only its public API is used.
     */
    void turn(FirmwareProbe& firmware) noexcept;
    /** @return Current phase; no I/O. */
    SmokeStep step() const noexcept { return step_; }
    /** @return Node index 0..1 being read; meaningful in READING. */
    uint8_t node() const noexcept { return node_; }
    /** @return Received non-trigger bytes, saturating; they cause nothing. */
    uint32_t ignoredBytes() const noexcept { return ignored_; }

  private:
    bool valid() const noexcept;
    void print(const char* format, ...) noexcept;
    void read(FirmwareProbe& firmware) noexcept;
    SmokeConfig config_;
    SmokeStep step_ = SmokeStep::WAITING;
    uint8_t node_ = 0;
    bool requested_ = false, announced_ = false;
    uint32_t ignored_ = 0;
    char line_[LINE] = {};
    size_t lineSize_ = 0;
};
} // namespace MotorControlCANExample
