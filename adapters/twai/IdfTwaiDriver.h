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
/** Stationary internal-RAM driver; construct on a pinned task's internal stack or
 * in static internal RAM. Construction has no I/O; explicit start occurs through
 * TwaiAdapter. The same pinned task owns all cooperative calls and destruction.
 * ISR callbacks copy bounded data only. No callback invokes the CAN owner.
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

  private:
    twai_node_handle_t node_ = nullptr;
    TaskHandle_t task_ = nullptr;
    int core_ = -1, lastError_ = 0;
    uint32_t epoch_ = 0, serial_ = 0;
    bool enabled_ = false, retiring_ = false, pending_ = false, busOff_ = false;
    twai_frame_t tx_ = {};
    uint8_t txBytes_[8] = {};
    ReceivedFrame rx_[RX_CAPACITY];
    uint8_t head_ = 0, count_ = 0, capacity_ = 0;
    TwaiEvents events_;
    portMUX_TYPE mux_ = portMUX_INITIALIZER_UNLOCKED;
    bool taskValid() const noexcept;
    bool start(const TwaiConfig&, uint32_t) noexcept;
    bool stop() noexcept;
    bool submit(const MotorControlCAN::Frame&, uint32_t) noexcept;
    TwaiEvents events() noexcept;
    bool receive(ReceivedFrame&) noexcept;
    static bool txDone(twai_node_handle_t, const twai_tx_done_event_data_t*, void*);
    static bool rxDone(twai_node_handle_t, const twai_rx_done_event_data_t*, void*);
    static bool stateChanged(twai_node_handle_t, const twai_state_change_event_data_t*, void*);
    static bool error(twai_node_handle_t, const twai_error_event_data_t*, void*);
};
} // namespace MotorControlCANExample
