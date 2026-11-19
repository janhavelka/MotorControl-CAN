/** @file IdfTwaiDriver.h
 * @brief ESP-IDF 5.5.5 ESP32-S2/S3 TWAI node shim; application-only SDK boundary.
 * SPDX-License-Identifier: MIT
 */
#pragma once
#include "TwaiAdapter.h"
#include "esp_twai.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

namespace MotorControlCANExample {
using MotorControlCANOwner::ReceivedFrame;
/** TX and reply timing copied in the SDK callbacks, for pass-cost diagnostics (prompt 56).
 * ESP timer microseconds, sampled in the callbacks, so ISR latency is included. Sums saturate.
 * A "reply" is the first frame received after a successful TX done: the SDO reply on a bus
 * with no other traffic, any frame otherwise. It measures no drive behavior by itself. */
struct TwaiTiming {
    uint32_t txDone = 0;                    ///< TX-done callbacks timed in this window.
    uint64_t submitToDoneSumUs = 0;         ///< Submission to TX-done callback, summed.
    uint32_t submitToDoneMinUs = UINT32_MAX; ///< Smallest; UINT32_MAX when txDone is 0.
    uint32_t submitToDoneMaxUs = 0;         ///< Largest.
    uint32_t replies = 0;                   ///< First receptions after a successful TX done.
    uint64_t doneToReplySumUs = 0;          ///< TX done to that reception's callback, summed.
    uint32_t doneToReplyMinUs = UINT32_MAX; ///< Smallest; UINT32_MAX when replies is 0.
    uint32_t doneToReplyMaxUs = 0;          ///< Largest.
    uint8_t rxHighWater = 0;                ///< Most frames held in the RX copy FIFO at once.
};
/** Stationary internal-RAM driver; construct on a pinned task's internal stack or
 * in static internal RAM. Construction has no I/O; explicit start occurs through
 * TwaiAdapter. The same pinned task owns all cooperative calls and destruction.
 * ISR callbacks copy bounded data and sample the ESP timer for an upper receive
 * bound. No callback invokes the CAN owner; no hard ISR-duration bound is claimed.
 * Call shutdown successfully before destroying this object; failure requires
 * keeping its storage alive. SDK allocations are local controller resources.
 * Cache-safe ISR and dynamic power management are unsupported in this first shim.
 */
class IdfTwaiDriver {
  public:
    enum {
        RX_CAPACITY = 16 ///< FIFO copies; full drops newest and latches loss.
    };
    /** No I/O or allocation; storage must remain stationary through successful shutdown. */
    IdfTwaiDriver() noexcept = default;
    IdfTwaiDriver(const IdfTwaiDriver&) = delete;            ///< Callback/SDK pointers cannot move.
    IdfTwaiDriver& operator=(const IdfTwaiDriver&) = delete; ///< No ownership transfer.
    /** @return Thin callbacks, borrowing this object until successful shutdown. */
    TwaiDriver driver() noexcept;
    /** @return Last raw SDK error, zero for latest successful SDK call; cached only. */
    int lastError() const noexcept { return lastError_; }
    /** Read the controller's error counters through the SDK, for diagnostics.
     * Not for an ISR; call at a diagnostic rate, not per service turn.
     * @param[out] transmit TX error counter, unchanged on failure.
     * @param[out] receive RX error counter, unchanged on failure.
     * @return True when the node exists and the SDK answered.
     */
    bool errorCounters(uint32_t& transmit, uint32_t& receive) const noexcept;
    /** Read the error state, TEC and REC through twai_node_get_info (esp_twai.h, ESP-IDF 5.5.5;
     * twai_node_status_t in esp_twai_types.h). The 5.5.5 node API has no separate bus-error or
     * arbitration-lost counter: twai_node_record_t::bus_err_num counts both together, so the
     * adapter counts error callbacks instead (TwaiEvents). On the pinned task only.
     * @param[out] output Copied status, unchanged on failure.
     * @return True when the node exists and the SDK answered.
     */
    bool status(TwaiControllerStatus& output) const noexcept;
    /** Copy the timing window and start a new one. On the pinned task only; diagnostics only.
     * @param[out] output The window since the previous call. @return False off the pinned task. */
    bool takeTiming(TwaiTiming& output) noexcept;

  private:
    twai_node_handle_t node_ = nullptr;
    TaskHandle_t task_ = nullptr;
    int core_ = -1, lastError_ = 0;
    uint32_t epoch_ = 0, serial_ = 0;
    bool enabled_ = false, retiring_ = false, pending_ = false, busOff_ = false;
    bool receiveBoundaryKnown_ = false;
    uint64_t receiveBoundaryUs_ = 0;
    bool clockKnown_ = false;
    uint64_t lastClockUs_ = 0;
    twai_frame_t tx_ = {};
    uint8_t txBytes_[8] = {};
    ReceivedFrame rx_[RX_CAPACITY];
    uint8_t head_ = 0, count_ = 0, capacity_ = 0;
    TwaiEvents events_;
    TwaiTiming timing_;              // Guarded by mux_.
    int64_t submittedUs_ = -1;       // The pending frame's submission time; -1 none.
    int64_t awaitingReplyUs_ = -1;   // The last successful TX done awaiting a reception; -1 none.
    portMUX_TYPE mux_ = portMUX_INITIALIZER_UNLOCKED;
    bool taskValid() const noexcept;
    bool start(const TwaiConfig&, uint32_t) noexcept;
    bool stop() noexcept;
    TwaiSubmit submit(const MotorControlCAN::Frame&, uint32_t) noexcept;
    TwaiEvents events() noexcept;
    bool receive(ReceivedFrame&, uint64_t cutoffUs) noexcept;
    void refreshReceiveBoundary() noexcept; // Caller holds mux_; reads time before empty FIFO proof.
    static bool txDone(twai_node_handle_t, const twai_tx_done_event_data_t*, void*);
    static bool rxDone(twai_node_handle_t, const twai_rx_done_event_data_t*, void*);
    static bool stateChanged(twai_node_handle_t, const twai_state_change_event_data_t*, void*);
    static bool error(twai_node_handle_t, const twai_error_event_data_t*, void*);
};
} // namespace MotorControlCANExample
