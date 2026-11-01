/** @file FakeTransport.h
 * @brief Deterministic bounded native fixture, never a hardware CAN adapter.
 * SPDX-License-Identifier: MIT
 */
#pragma once
#include "CanBusOwner.h"
#include <MotorControlCAN/profiles/lichuan_c/Reads.h>

namespace MotorControlCANExample {
/** Synthetic server for the configured node 5 expedited identity/config uploads.
 * One owner alone calls receive. All queued frames/reports own their bytes.
 * Caller serializes methods and supplies nonwrapping time; no clocks or threads.
 * Synthetic raw values imply no real manufacturer, model or firmware identity.
 */
class FakeTransport {
  public:
    enum { INGRESS_CAPACITY = 16, REPORT_CAPACITY = 4 };
    enum class Mode : uint8_t { SUCCESS, ABORT, SILENT };
    /** @param[in] mode Fixed fixture response policy; construction performs no I/O. */
    explicit FakeTransport(Mode mode = Mode::SUCCESS) noexcept : mode_(mode) {}
    /** @return Borrowed callbacks; this stationary fixture outlives its owner. */
    Transport callbacks() noexcept;
    /** @return Explicit synthetic route; does not configure or contact a device. */
    static MotorControlCAN::LICHUAN_C::ReadTarget target() noexcept;
    /** @param[in] now Supplied monotonic microseconds, zero valid.
     * @return False unchanged for backward time; no owner progress occurs.
     */
    bool setTime(uint64_t now) noexcept;
    /** @return Last supplied microseconds; no hidden clock access. */
    uint64_t nowUs() const noexcept { return now_; }
    /** Queue an owned passive/test frame with current capture time and epoch 1.
     * @param[in] frame Borrowed valid Classical frame; copied only on success.
     * @return False unchanged for invalid input/full queue; no silent eviction.
     */
    bool push(const MotorControlCAN::Frame& frame) noexcept;
    /** @return Accepted upload handoffs; never writes, NMT or physical evidence. */
    unsigned transmitted() const noexcept { return transmitted_; }
    /** @return Every attempted handoff, including atomic refusal; useful for no-I/O checks. */
    unsigned transmitCalls() const noexcept { return transmitCalls_; }
    /** @return Diagnostic copies after the owner's ordinary routing. */
    unsigned observed() const noexcept { return observed_; }
    /** @return Number of receive callback calls, including empty polls. */
    unsigned receiveCalls() const noexcept { return receiveCalls_; }
    /** @return Current bounded ingress occupancy. */
    unsigned ingressUsed() const noexcept { return rxCount_; }
    /** @return Current bounded report occupancy. */
    unsigned reportsUsed() const noexcept { return reportCount_; }

  private:
    Mode mode_;
    uint64_t now_ = 0;
    ReceivedFrame incoming_[INGRESS_CAPACITY];
    TxReport reports_[REPORT_CAPACITY];
    unsigned rxHead_ = 0, rxCount_ = 0, reportHead_ = 0, reportCount_ = 0;
    unsigned transmitted_ = 0, transmitCalls_ = 0, observed_ = 0, receiveCalls_ = 0;
    static bool receive(void*, ReceivedFrame&) noexcept;
    static bool transmit(void*, const MotorControlCAN::Frame&, MotorControlCAN::CANOPEN::SdoTxId,
                         uint32_t) noexcept;
    static bool takeReport(void*, TxReport&) noexcept;
    static void observe(void*, const ReceivedFrame&, uint64_t) noexcept;
};
} // namespace MotorControlCANExample
