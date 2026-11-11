/** @file ProbePlatform.h
 * @brief Bounded application byte I/O and external time; no CAN receive path.
 * SPDX-License-Identifier: MIT
 */
#pragma once
#include <stddef.h>
#include <stdint.h>

namespace MotorControlCANExample {
/** Borrowed stationary platform callbacks. All calls are cooperative, bounded,
 * nonblocking and nonreentrant; no callback may receive CAN or step the owner.
 * Byte buffers are never borrowed across calls. Missing I/O callbacks mean no
 * progress. Clock acquisition/wrap extension belong to the platform, not core.
 */
struct ProbeCallbacks {
    void* context = nullptr; ///< Borrowed for the platform/application lifetime.
    bool (*nowUs)(void*,
                  uint64_t&) = nullptr; ///< Publish nonwrapping monotonic us on true; zero valid.
    bool (*readByte)(void*,
                     uint8_t&) = nullptr; ///< One byte on true; false empty, output unchanged.
    bool (*writeByte)(void*, uint8_t) = nullptr; ///< True accepts this byte; false accepts nothing.
    void (*idle)(void*) =
        nullptr; ///< Optional one bounded idle/diagnostic opportunity; never waits for I/O.
};
/** Cached platform pressure. Each total saturates at UINT32_MAX, except outputWritten, which
 * wraps so that a receiver can compare differences over any duration.
 */
struct ProbeIoStats {
    uint32_t inputDropped = 0;   ///< Newest consumed input bytes dropped at full input capacity.
    uint32_t outputRejected = 0; ///< Whole output submissions refused for insufficient capacity.
    /** Queued bytes that writeByte accepted since construction, modulo 2^32. Acceptance by the
     * platform (for example a UART FIFO) is not receipt by the far end. */
    uint32_t outputWritten = 0;
    uint8_t inputUsed = 0;   ///< Current buffered bytes, including embedded zero.
    uint16_t outputUsed = 0; ///< Current queued bytes; partial platform writes preserve suffix.
};
/** Fixed application-local buffers. This is neither a parser nor a CAN adapter.
 * Caller serializes all methods; callbacks cannot reenter. Slow output does not
 * prevent the shared application from servicing CAN first on every turn.
 */
class ProbePlatform {
  public:
    enum {
        INPUT_CAPACITY = 64,   ///< Bytes; full ingress drops newest with accounting.
        OUTPUT_CAPACITY = 512, ///< Bytes; submissions are atomic, no partial enqueue.
        INPUT_BUDGET = 8,      ///< Maximum readByte calls per service turn.
        OUTPUT_BUDGET = 16     ///< Maximum writeByte calls per service turn.
    };
    /** @param[in] callbacks Copied functions, borrowed context; construction does no I/O. */
    explicit ProbePlatform(const ProbeCallbacks& callbacks) noexcept;
    ProbePlatform(const ProbePlatform&) = delete; ///< Stationary callback/buffer ownership.
    ProbePlatform& operator=(const ProbePlatform&) = delete; ///< No shared mutable buffers.
    /** @param[out] output Supplied platform microseconds, unchanged if no clock/value.
     * @return Clock callback result; no owner progress or hidden fallback clock.
     */
    bool nowUs(uint64_t& output) const noexcept;
    /** Run bounded input/output then one optional idle callback; no CAN or parsing. */
    void service() noexcept;
    /** @param[out] output Oldest byte on true, unchanged when empty.
     * @return True consumes one application input byte only; never a CAN frame.
     */
    bool takeInput(uint8_t& output) noexcept;
    /** Consume the parser's loss latch, independently of saturating counters.
     * One input/parser owner calls this before consuming buffered bytes. Every
     * dropped byte sets it, including after inputDropped saturates.
     * @return True once per observed loss interval; clears only this latch.
     * No callbacks, clock, CAN receive or buffer mutation occurs.
     */
    bool takeInputLoss() noexcept;
    /** Copy complete output bytes. No pointer is retained; input may be released on return.
     * @param[in] bytes Borrowed nonnull span when size>0; null is allowed for size zero.
     * @param[in] size Byte count, including embedded zero; at most OUTPUT_CAPACITY.
     * @return True copies all bytes. False preserves queued output; capacity refusal
     * increments outputRejected once. Invalid null spans reject without accounting.
     */
    bool writeOutput(const uint8_t* bytes, size_t size) noexcept;
    /** @return Owned cached pressure copy; no callbacks or reset of live buffers. */
    ProbeIoStats stats() const noexcept { return stats_; }

  private:
    ProbeCallbacks callbacks_;
    ProbeIoStats stats_;
    uint8_t input_[INPUT_CAPACITY] = {}, output_[OUTPUT_CAPACITY] = {};
    uint8_t inputHead_ = 0;
    uint16_t outputHead_ = 0;
    bool inputLoss_ = false;
};
} // namespace MotorControlCANExample
