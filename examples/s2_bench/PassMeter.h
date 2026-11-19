/** @file PassMeter.h
 * @brief Bench pass-time statistics for prompt 56's throughput measurements; example code only.
 * SPDX-License-Identifier: MIT
 */
#pragma once
#include <stdint.h>

namespace MotorControlCANExample {
/** One window of owner-task passes: how long each pass worked (from the loop top to the idle
 * call), how often passes started, and the owner's queue high-water marks sampled once per pass.
 * Times are supplied microseconds. Counts and sums saturate. Plain value; no clock or I/O. */
struct PassMeter {
    /// Work histogram buckets: under 1, 2, 3 and 5 ms, then 5 ms or longer.
    enum { BUCKETS = 5 };
    uint64_t windowStartUs = 0; ///< First pass start of this window; meaningful when passes > 0.
    uint64_t lastStartUs = 0;   ///< Latest pass start.
    uint32_t passes = 0;        ///< Passes recorded.
    uint64_t workSumUs = 0;     ///< Pass work, summed.
    uint32_t workMinUs = UINT32_MAX, workMaxUs = 0;
    uint32_t periodMaxUs = 0;   ///< Largest time between two pass starts in this window.
    uint32_t histogram[BUCKETS] = {};
    uint8_t queuedHigh = 0, activeHigh = 0, retainedHigh = 0; ///< OwnerResources maxima.

    /** Record one pass. @param startUs Its start. @param endUs Its end, not before startUs. */
    void record(uint64_t startUs, uint64_t endUs) noexcept {
        if (endUs < startUs)
            return;
        const uint64_t spent = endUs - startUs;
        const uint32_t work = spent > UINT32_MAX ? UINT32_MAX : uint32_t(spent);
        if (passes == 0)
            windowStartUs = startUs;
        else if (startUs >= lastStartUs) {
            const uint64_t period = startUs - lastStartUs;
            const uint32_t p = period > UINT32_MAX ? UINT32_MAX : uint32_t(period);
            if (p > periodMaxUs)
                periodMaxUs = p;
        }
        lastStartUs = startUs;
        if (passes != UINT32_MAX)
            ++passes;
        if (workSumUs <= UINT64_MAX - work)
            workSumUs += work;
        if (work < workMinUs)
            workMinUs = work;
        if (work > workMaxUs)
            workMaxUs = work;
        const uint32_t bounds[BUCKETS - 1] = {1000, 2000, 3000, 5000};
        unsigned b = 0;
        while (b < BUCKETS - 1 && work >= bounds[b])
            ++b;
        if (histogram[b] != UINT32_MAX)
            ++histogram[b];
    }
    /** Keep the largest queue depths seen at a pass end. */
    void sample(uint8_t queued, uint8_t active, uint8_t retained) noexcept {
        if (queued > queuedHigh)
            queuedHigh = queued;
        if (active > activeHigh)
            activeHigh = active;
        if (retained > retainedHigh)
            retainedHigh = retained;
    }
};
} // namespace MotorControlCANExample
