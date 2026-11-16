/** @file WiredStopOutput.h
 * @brief Explicit example-owned electrical stop signal; no GPIO or motion qualification.
 * SPDX-License-Identifier: MIT
 */
#pragma once
#include <MotorControlCAN/Status.h>

namespace MotorControlCANExample {
/** Application wiring facts and synchronous bounded output callback.
 * Callback success means the host applied the requested electrical level only.
 * The application establishes the actual DI function, active level and wiring first;
 * this helper neither configures/saves the drive nor releases/enables a motor.
 */
struct WiredStopSignal {
    void* context = nullptr; ///< Borrowed application hardware context.
    MotorControlCAN::Status (*writeLevel)(void*, bool high) = nullptr; ///< Explicit host I/O.
    bool activeHigh = false;  ///< Actual wired active electrical level, not a drive default.
    bool wiringKnown = false; ///< Caller verified this signal and its active level.
};
/** Latest host attempt, not a drive-input readback or stopped observation. */
struct WiredStopResult {
    bool attempted = false;         ///< A callback was invoked by an explicit request.
    bool asserted = false;          ///< Requested logical assertion, not observed input state.
    bool electricalHigh = false;    ///< Electrical level passed to the callback.
    bool uncertain = false;         ///< Callback failure may have changed the electrical output.
    MotorControlCAN::Status status; ///< Host callback result, independent of motor state.
};
/** Explicit assertion/release only. No startup action, destructor release, retry or clock. */
class WiredStopOutput {
  public:
    /** Construct without I/O. @param signal Copied wiring facts and borrowed callback context. */
    explicit WiredStopOutput(const WiredStopSignal& signal) noexcept : signal_(signal) {}
    /** Write once at explicit caller request. Missing wiring/callback fails without I/O
     * or result change. Deassertion does not authorize motion or clear drive faults.
     * @param asserted True asserts the configured electrical signal; false deasserts it.
     * @return Wiring validation or callback result; a failed callback leaves output uncertain. */
    MotorControlCAN::Status setAsserted(bool asserted) noexcept;
    /** @return Latest explicit host attempt; no controller or physical stop evidence. */
    const WiredStopResult& result() const noexcept { return result_; }

  private:
    WiredStopSignal signal_;
    WiredStopResult result_;
};
} // namespace MotorControlCANExample
