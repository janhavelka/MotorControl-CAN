// SPDX-License-Identifier: MIT
#include "IdfTwaiDriver.h"
#include "driver/gpio.h"
#include "esp_idf_version.h"
#include "esp_memory_utils.h"
#include "esp_timer.h"
#include "esp_twai_onchip.h"
#include "hal/twai_ll.h"
#include "sdkconfig.h"

#if ESP_IDF_VERSION != ESP_IDF_VERSION_VAL(5, 5, 5)
#error "TWAI adapter requires reviewed ESP-IDF 5.5.5"
#endif
#if !CONFIG_IDF_TARGET_ESP32S3 && !CONFIG_IDF_TARGET_ESP32S2
#error "TWAI node retirement proof covers ESP32-S2 and ESP32-S3 only"
#endif
#if CONFIG_TWAI_ISR_CACHE_SAFE || CONFIG_TWAI_ISR_IN_IRAM || CONFIG_PM_ENABLE || CONFIG_FREERTOS_SMP
#error "TWAI adapter requires cache-safe ISR, PM and FreeRTOS SMP disabled"
#endif

namespace MotorControlCANExample {
namespace {
void increment(uint32_t& value) {
    if (value != UINT32_MAX)
        ++value;
}
} // namespace
TwaiDriver IdfTwaiDriver::driver() noexcept {
    TwaiDriver result;
    result.context = this;
    result.start = [](void* p, const TwaiConfig& config, uint32_t epoch) {
        return static_cast<IdfTwaiDriver*>(p)->start(config, epoch);
    };
    result.stop = [](void* p) { return static_cast<IdfTwaiDriver*>(p)->stop(); };
    result.submit = [](void* p, const MotorControlCAN::Frame& frame, uint32_t serial) {
        return static_cast<IdfTwaiDriver*>(p)->submit(frame, serial);
    };
    result.events = [](void* p) { return static_cast<IdfTwaiDriver*>(p)->events(); };
    result.receive = [](void* p, ReceivedFrame& frame, uint64_t cutoffUs) {
        return static_cast<IdfTwaiDriver*>(p)->receive(frame, cutoffUs);
    };
    return result;
}
bool IdfTwaiDriver::taskValid() const noexcept {
    return task_ && xTaskGetCurrentTaskHandle() == task_ && xTaskGetCoreID(task_) == core_ &&
           xPortGetCoreID() == core_;
}
bool IdfTwaiDriver::start(const TwaiConfig& config, uint32_t epoch) noexcept {
    // SDK GPIO validity macros shift by the pin; bound it before evaluating them.
    if (node_ || !epoch || !esp_ptr_internal(this) ||
        !esp_ptr_internal(reinterpret_cast<const uint8_t*>(this) + sizeof(*this) - 1) ||
        config.txPin >= GPIO_NUM_MAX || config.rxPin >= GPIO_NUM_MAX ||
        !GPIO_IS_VALID_OUTPUT_GPIO(config.txPin) || !GPIO_IS_VALID_GPIO(config.rxPin) ||
        config.txPin == config.rxPin || !config.bitrate || config.sdkTxQueueDepth != 1 ||
        config.samplePointPermill >= 1000 || !config.rxQueueDepth ||
        config.rxQueueDepth > RX_CAPACITY)
        return false;
    if (!task_) {
        const auto task = xTaskGetCurrentTaskHandle();
        const int core = xTaskGetCoreID(task);
        if (core < 0 || core >= portNUM_PROCESSORS)
            return false;
        task_ = task;
        core_ = core;
    }
    if (!taskValid())
        return false;
    epoch_ = epoch;
    capacity_ = config.rxQueueDepth;
    retiring_ = pending_ = busOff_ = false;
    receiveBoundaryKnown_ = false;
    receiveBoundaryUs_ = 0;
    clockKnown_ = false;
    lastClockUs_ = 0;
    events_ = TwaiEvents();
    head_ = count_ = 0;
    twai_onchip_node_config_t configNative = {};
    configNative.io_cfg.tx = static_cast<gpio_num_t>(config.txPin);
    configNative.io_cfg.rx = static_cast<gpio_num_t>(config.rxPin);
    configNative.io_cfg.quanta_clk_out = static_cast<gpio_num_t>(-1);
    configNative.io_cfg.bus_off_indicator = static_cast<gpio_num_t>(-1);
    configNative.bit_timing.bitrate = config.bitrate;
    configNative.bit_timing.sp_permill = config.samplePointPermill;
    configNative.fail_retry_cnt = 0;
    configNative.tx_queue_depth = 1;
    // Timestamp disabled: the reviewed S2/S3 HAL has no wire-arrival timestamp.
    lastError_ = twai_new_node_onchip(&configNative, &node_);
    if (lastError_ != ESP_OK)
        return false;
    twai_event_callbacks_t callbacks = {};
    callbacks.on_tx_done = txDone;
    callbacks.on_rx_done = rxDone;
    callbacks.on_state_change = stateChanged;
    callbacks.on_error = error;
    lastError_ = twai_node_register_event_callbacks(node_, &callbacks, this);
    if (lastError_ != ESP_OK)
        return false; // Keep handle for explicit retirement.
    twai_mask_filter_config_t filter = {};
    lastError_ = twai_node_config_mask_filter(node_, 0, &filter);
    if (lastError_ != ESP_OK)
        return false;
    lastError_ = twai_node_enable(node_);
    enabled_ = lastError_ == ESP_OK;
    portENTER_CRITICAL(&mux_);
    refreshReceiveBoundary();
    portEXIT_CRITICAL(&mux_);
    return enabled_;
}
bool IdfTwaiDriver::stop() noexcept {
    if (!task_ && !node_)
        return true;
    if (!taskValid())
        return false;
    portENTER_CRITICAL(&mux_);
    retiring_ = true;
    portEXIT_CRITICAL(&mux_);
    if (node_) {
        // Query real state: BUS_OFF and disabled share an SDK enum. Never recover
        // or re-enable the old node: both can restart its retained transactions.
        twai_node_status_t status = {};
        lastError_ = twai_node_get_info(node_, &status, nullptr);
        if (lastError_ != ESP_OK)
            return false;
        if (status.state != TWAI_ERROR_BUS_OFF) {
            lastError_ = twai_node_disable(node_);
            if (lastError_ != ESP_OK)
                return false;
        }
        enabled_ = false;
        // Exact S2/S3 same-core proof: HAL masks peripheral interrupts before
        // deinit; deletion frees the ISR before buffers. See SDK evidence table.
        lastError_ = twai_node_delete(node_);
        if (lastError_ != ESP_OK)
            return false;
        node_ = nullptr;
    }
    pending_ = busOff_ = false;
    receiveBoundaryKnown_ = false;
    receiveBoundaryUs_ = 0;
    clockKnown_ = false;
    lastClockUs_ = 0;
    events_ = TwaiEvents();
    head_ = count_ = 0;
    return true;
}
bool IdfTwaiDriver::submit(const MotorControlCAN::Frame& frame, uint32_t serial) noexcept {
    if (!taskValid() || !node_ || !enabled_ || !serial || !MotorControlCAN::validateFrame(frame))
        return false;
    portENTER_CRITICAL(&mux_);
    if (retiring_ || pending_ || events_.fault || events_.rxDropped || busOff_ || count_) {
        portEXIT_CRITICAL(&mux_);
        return false;
    }
    // Keep the final captured-RX/fault guard and SDK handoff indivisible to this
    // core's ISR. The pinned SDK's single-outstanding path only nests its event
    // lock and starts HAL TX; it does not wait or call callbacks synchronously.
    tx_ = twai_frame_t();
    tx_.header.id = frame.id;
    tx_.header.dlc = frame.dlc;
    tx_.header.ide = frame.extended;
    tx_.header.rtr = frame.rtr;
    tx_.buffer = txBytes_;
    tx_.buffer_len = frame.payloadLength;
    for (unsigned i = 0; i < frame.payloadLength; ++i)
        txBytes_[i] = frame.data[i];
    serial_ = serial;
    pending_ = true;
    lastError_ = twai_node_transmit(node_, &tx_, 0);
    if (lastError_ != ESP_OK)
        pending_ = false;
    portEXIT_CRITICAL(&mux_);
    return lastError_ == ESP_OK;
}
TwaiEvents IdfTwaiDriver::events() noexcept {
    if (!taskValid()) {
        TwaiEvents failed;
        failed.fault = true;
        return failed;
    }
    portENTER_CRITICAL(&mux_);
    refreshReceiveBoundary();
    TwaiEvents result = events_;
    result.rxPending = count_ != 0;
    events_ = TwaiEvents();
    portEXIT_CRITICAL(&mux_);
    return result;
}
void IdfTwaiDriver::refreshReceiveBoundary() noexcept {
    if (!node_ || !enabled_ || retiring_ || busOff_ || events_.fault || events_.rxDropped || count_)
        return;
    // This pinned private LL read does not drain/clear hardware. Same-core
    // ownership excludes an SDK frame between FIFO pop and its RX callback.
    // Sample BEFORE the empty check: an arrival after the check is then no older
    // than this bound. This bounds frame completion, never its start bit.
    const int64_t sampled = esp_timer_get_time();
    if (sampled < 0 || (clockKnown_ && uint64_t(sampled) < lastClockUs_)) {
        events_.fault = true;
        receiveBoundaryKnown_ = false;
    } else {
        clockKnown_ = true;
        lastClockUs_ = static_cast<uint64_t>(sampled);
        if (twai_ll_get_rx_msg_count(TWAI_LL_GET_HW(0)) == 0) {
            receiveBoundaryUs_ = lastClockUs_;
            receiveBoundaryKnown_ = true;
        }
    }
}
bool IdfTwaiDriver::receive(ReceivedFrame& frame, uint64_t cutoffUs) noexcept {
    if (!taskValid())
        return false;
    portENTER_CRITICAL(&mux_);
    refreshReceiveBoundary();
    const bool available =
        count_ != 0 && !retiring_ && !busOff_ && !events_.fault && !events_.rxDropped &&
        rx_[head_].receivedLatestUs <= cutoffUs;
    if (available) {
        frame = rx_[head_];
        head_ = (head_ + 1) % RX_CAPACITY;
        --count_;
    }
    portEXIT_CRITICAL(&mux_);
    return available;
}
bool IdfTwaiDriver::txDone(twai_node_handle_t, const twai_tx_done_event_data_t* event, void* p) {
    auto& self = *static_cast<IdfTwaiDriver*>(p);
    portENTER_CRITICAL_ISR(&self.mux_);
    if (!self.pending_ || event->done_tx_frame != &self.tx_ || self.events_.txDone)
        self.events_.fault = true;
    else {
        self.events_.txDone = true;
        self.events_.transmitted = event->is_tx_success;
        self.events_.serial = self.serial_;
        self.pending_ = false;
    }
    portEXIT_CRITICAL_ISR(&self.mux_);
    return false;
}
bool IdfTwaiDriver::rxDone(twai_node_handle_t node, const twai_rx_done_event_data_t*, void* p) {
    auto& self = *static_cast<IdfTwaiDriver*>(p);
    uint8_t bytes[8] = {};
    twai_frame_t native = {};
    native.buffer = bytes;
    native.buffer_len = sizeof(bytes);
    const bool read = twai_node_receive_from_isr(node, &native) == ESP_OK;
    const int64_t observed = esp_timer_get_time();
    ReceivedFrame received;
    received.controllerEpoch = self.epoch_;
    received.frame.id = native.header.id;
    received.frame.extended = native.header.ide;
    received.frame.rtr = native.header.rtr;
    received.frame.fd = native.header.fdf;
    received.frame.brs = native.header.brs;
    received.frame.esi = native.header.esi;
    const bool validDlc = native.header.dlc <= 8;
    if (validDlc) {
        received.frame.dlc = static_cast<uint8_t>(native.header.dlc);
        received.frame.payloadLength = received.frame.rtr ? 0 : received.frame.dlc;
        for (unsigned i = 0; i < received.frame.payloadLength; ++i)
            received.frame.data[i] = bytes[i];
    }
    portENTER_CRITICAL_ISR(&self.mux_);
    if (observed < 0 || (self.clockKnown_ && uint64_t(observed) < self.lastClockUs_)) {
        self.events_.fault = true;
        self.receiveBoundaryKnown_ = false;
    } else {
        self.clockKnown_ = true;
        self.lastClockUs_ = static_cast<uint64_t>(observed);
    }
    received.receiveBoundsKnown = self.receiveBoundaryKnown_ && observed >= 0;
    received.receivedEarliestUs = self.receiveBoundaryUs_;
    received.receivedLatestUs = observed < 0 ? 0 : static_cast<uint64_t>(observed);
    if (!read || !validDlc || !MotorControlCAN::validateFrame(received.frame) ||
        self.count_ == self.capacity_ || self.retiring_ || self.events_.fault)
        increment(self.events_.rxDropped);
    else {
        self.rx_[(self.head_ + self.count_) % RX_CAPACITY] = received;
        ++self.count_;
    }
    portEXIT_CRITICAL_ISR(&self.mux_);
    return false;
}
bool IdfTwaiDriver::stateChanged(twai_node_handle_t, const twai_state_change_event_data_t* event,
                                 void* p) {
    auto& self = *static_cast<IdfTwaiDriver*>(p);
    portENTER_CRITICAL_ISR(&self.mux_);
    if (event->new_sta == TWAI_ERROR_BUS_OFF) {
        self.busOff_ = true;
        self.events_.busOff = true;
    }
    portEXIT_CRITICAL_ISR(&self.mux_);
    return false;
}
bool IdfTwaiDriver::error(twai_node_handle_t, const twai_error_event_data_t* event, void* p) {
    auto& self = *static_cast<IdfTwaiDriver*>(p);
    portENTER_CRITICAL_ISR(&self.mux_);
    increment(self.events_.busErrors);
    self.events_.errorFlags |= event->err_flags.val;
    portEXIT_CRITICAL_ISR(&self.mux_);
    return false;
}
} // namespace MotorControlCANExample
