// SPDX-License-Identifier: MIT
// Compile the actual SDK shim against narrow host fakes. No SDK build or CAN I/O.
#include "IdfTwaiDriver.h"
#include "esp_twai_onchip.h"
#include "hal/twai_ll.h"
#include <climits>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace E = MotorControlCANExample;
namespace O = MotorControlCANOwner;
namespace M = MotorControlCAN;
namespace {
unsigned checks = 0;
#define CHECK(c)                                                                                   \
    do {                                                                                           \
        ++checks;                                                                                  \
        if (!(c)) {                                                                                \
            std::fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #c);                           \
            std::abort();                                                                          \
        }                                                                                          \
    } while (false)
enum class Call { CREATE, REGISTER, FILTER, ENABLE, INFO, DISABLE, DELETE, SUBMIT, RECEIVE };
struct FakeSdk {
    Call calls[128] = {};
    unsigned count = 0, transmissions = 0, receives = 0;
    unsigned criticalDepth = 0, bootAfterTransmissions = 0;
    int64_t nowUs = 0;
    uint32_t hardwareRxCount = 0;
    bool frameDuringClockSample = false, frameAfterEmptyCheck = false;
    Call fail = Call::RECEIVE;
    bool failEnabled = false, memoryInternal = true, live = false;
    bool immediate = false, rxFail = false;
    bool bootOnExit = false, deferredDone = false, deferredSuccess = false;
    // Bus-off taking effect after the state query, seen by the SDK at disable.
    bool busOffAtDisable = false, invalidStateAtDisable = false;
    unsigned infoCalls = 0, failInfoAtCall = 0;
    const twai_frame_t* deferredPointer = nullptr;
    int currentCore = 1, taskCore = 1;
    TaskHandle_t task = reinterpret_cast<TaskHandle_t>(1);
    twai_error_state_t state = TWAI_ERROR_BUS_OFF;
    twai_onchip_node_config_t config = {};
    twai_mask_filter_config_t filter = {};
    twai_event_callbacks_t callbacks = {};
    void* context = nullptr;
    const twai_frame_t* pending = nullptr;
    twai_frame_header_t rxHeader = {};
    uint8_t rxBytes[8] = {};
    twai_node_handle_t node() { return reinterpret_cast<twai_node_handle_t>(this); }
    bool record(Call call) {
        CHECK(count < 128);
        calls[count++] = call;
        return failEnabled && fail == call;
    }
    void done(bool ok = true, const twai_frame_t* pointer = nullptr) {
        CHECK(live && callbacks.on_tx_done);
        if (criticalDepth) {
            CHECK(!deferredDone);
            deferredDone = true;
            deferredSuccess = ok;
            deferredPointer = pointer;
            return;
        }
        twai_tx_done_event_data_t event = {ok, pointer ? pointer : pending};
        CHECK(!callbacks.on_tx_done(node(), &event, context));
    }
    void receiveCallback() {
        CHECK(live && callbacks.on_rx_done && criticalDepth == 0);
        twai_rx_done_event_data_t event = {};
        CHECK(!callbacks.on_rx_done(node(), &event, context));
    }
    void boot() {
        bootAfterTransmissions = transmissions;
        rxHeader = twai_frame_header_t();
        rxHeader.id = 0x701;
        rxHeader.dlc = 1;
        rxBytes[0] = 0;
        receiveCallback();
    }
    void receive(uint32_t id, uint16_t dlc, bool extended = false, bool remote = false,
                 bool fd = false, bool brs = false, bool esi = false) {
        CHECK(live && callbacks.on_rx_done);
        rxHeader = twai_frame_header_t();
        rxHeader.id = id;
        rxHeader.dlc = dlc;
        rxHeader.ide = extended;
        rxHeader.rtr = remote;
        rxHeader.fdf = fd;
        rxHeader.brs = brs;
        rxHeader.esi = esi;
        rxHeader.timestamp = 987654; // Driver must not promote this to qualified arrival.
        for (unsigned i = 0; i < 8; ++i)
            rxBytes[i] = uint8_t(i + 1);
        receiveCallback();
    }
    void busOff() {
        CHECK(live && callbacks.on_state_change);
        twai_state_change_event_data_t event = {state, TWAI_ERROR_BUS_OFF};
        state = TWAI_ERROR_BUS_OFF;
        CHECK(!callbacks.on_state_change(node(), &event, context));
    }
    void error(uint32_t flags = 0x10) {
        CHECK(live && callbacks.on_error);
        twai_error_event_data_t event = {};
        event.err_flags.val = flags;
        CHECK(!callbacks.on_error(node(), &event, context));
    }
} sdk;
E::TwaiConfig config(uint8_t capacity = 16) {
    E::TwaiConfig value;
    value.txPin = 4;
    value.rxPin = 5;
    value.bitrate = 250000;
    value.samplePointPermill = 800;
    value.sdkTxQueueDepth = 1;
    value.rxQueueDepth = capacity;
    return value;
}
M::Frame frame() {
    M::Frame value;
    value.id = 0x123;
    value.dlc = value.payloadLength = 8;
    for (unsigned i = 0; i < 8; ++i)
        value.data[i] = uint8_t(i + 11);
    return value;
}
void reset() {
    CHECK(sdk.criticalDepth == 0 && !sdk.deferredDone && !sdk.bootOnExit);
    sdk = FakeSdk();
}
void stop(E::TwaiDriver& driver) {
    CHECK(driver.stop(driver.context));
    CHECK(!sdk.live);
}
} // namespace

twai_dev_t fakeTwaiHardware;
int64_t esp_timer_get_time() {
    if (sdk.frameDuringClockSample) {
        CHECK(sdk.criticalDepth != 0);
        sdk.frameDuringClockSample = false;
        sdk.hardwareRxCount = 1;
    }
    return sdk.nowUs;
}
uint32_t twai_ll_get_rx_msg_count(twai_dev_t* hardware) {
    CHECK(hardware == &fakeTwaiHardware && sdk.live && sdk.criticalDepth != 0);
    const uint32_t count = sdk.hardwareRxCount;
    if (sdk.frameAfterEmptyCheck) {
        CHECK(!count);
        sdk.frameAfterEmptyCheck = false;
        sdk.bootOnExit = true;
    }
    return count;
}

void fakeEnterCritical(portMUX_TYPE* mux) {
    CHECK(mux);
    ++sdk.criticalDepth;
}
void fakeExitCritical(portMUX_TYPE* mux) {
    CHECK(mux && sdk.criticalDepth);
    if (--sdk.criticalDepth)
        return;
    if (sdk.bootOnExit) {
        sdk.bootOnExit = false;
        sdk.boot();
    }
    if (sdk.deferredDone) {
        sdk.deferredDone = false;
        sdk.done(sdk.deferredSuccess, sdk.deferredPointer);
    }
}

TaskHandle_t xTaskGetCurrentTaskHandle() {
    return sdk.task;
}
int xTaskGetCoreID(TaskHandle_t) {
    return sdk.taskCore;
}
int xPortGetCoreID() {
    return sdk.currentCore;
}
bool esp_ptr_internal(const void*) {
    return sdk.memoryInternal;
}
esp_err_t twai_new_node_onchip(const twai_onchip_node_config_t* config, twai_node_handle_t* node) {
    if (sdk.record(Call::CREATE))
        return ESP_FAIL;
    CHECK(!sdk.live);
    sdk.config = *config;
    sdk.live = true;
    sdk.state = TWAI_ERROR_BUS_OFF;
    *node = sdk.node();
    return ESP_OK;
}
esp_err_t twai_node_register_event_callbacks(twai_node_handle_t node,
                                             const twai_event_callbacks_t* callbacks,
                                             void* context) {
    CHECK(node == sdk.node() && sdk.live);
    if (sdk.record(Call::REGISTER))
        return ESP_FAIL;
    sdk.callbacks = *callbacks;
    sdk.context = context;
    return ESP_OK;
}
esp_err_t twai_node_config_mask_filter(twai_node_handle_t node, uint8_t id,
                                       const twai_mask_filter_config_t* filter) {
    CHECK(node == sdk.node() && id == 0 && sdk.live);
    if (sdk.record(Call::FILTER))
        return ESP_FAIL;
    sdk.filter = *filter;
    return ESP_OK;
}
esp_err_t twai_node_enable(twai_node_handle_t node) {
    CHECK(node == sdk.node() && sdk.live);
    // Exact relevant SDK ordering: HAL starts and state changes before interrupt
    // enable can return an error. A false API result may still need disable.
    sdk.state = TWAI_ERROR_ACTIVE;
    return sdk.record(Call::ENABLE) ? ESP_FAIL : ESP_OK;
}
esp_err_t twai_node_disable(twai_node_handle_t node) {
    CHECK(node == sdk.node() && sdk.live);
    if (sdk.busOffAtDisable) {
        sdk.busOffAtDisable = false;
        sdk.state = TWAI_ERROR_BUS_OFF;
    }
    if (sdk.record(Call::DISABLE))
        return ESP_FAIL;
    if (sdk.state == TWAI_ERROR_BUS_OFF || sdk.invalidStateAtDisable)
        return ESP_ERR_INVALID_STATE;
    sdk.state = TWAI_ERROR_BUS_OFF;
    return ESP_OK;
}
esp_err_t twai_node_delete(twai_node_handle_t node) {
    CHECK(node == sdk.node() && sdk.live);
    if (sdk.record(Call::DELETE))
        return ESP_FAIL;
    if (sdk.state != TWAI_ERROR_BUS_OFF)
        return ESP_ERR_INVALID_STATE;
    sdk.pending = nullptr;
    sdk.live = false;
    sdk.callbacks = twai_event_callbacks_t();
    return ESP_OK;
}
esp_err_t twai_node_get_info(twai_node_handle_t node, twai_node_status_t* status,
                             twai_node_record_t*) {
    CHECK(node == sdk.node() && sdk.live);
    ++sdk.infoCalls;
    if (sdk.record(Call::INFO) || sdk.infoCalls == sdk.failInfoAtCall)
        return ESP_FAIL;
    status->state = sdk.state;
    return ESP_OK;
}
esp_err_t twai_node_transmit(twai_node_handle_t node, const twai_frame_t* frame, int timeout) {
    CHECK(node == sdk.node() && sdk.live && timeout == 0);
    // The reviewed direct SDK handoff masks interrupts internally too. Its exit
    // must not deliver an ISR while the driver's outer section still holds.
    portMUX_TYPE sdkMux = portMUX_INITIALIZER_UNLOCKED;
    portENTER_CRITICAL(&sdkMux);
    if (sdk.record(Call::SUBMIT)) {
        portEXIT_CRITICAL(&sdkMux);
        return ESP_FAIL;
    }
    ++sdk.transmissions;
    sdk.pending = frame;
    if (sdk.immediate)
        sdk.done();
    portEXIT_CRITICAL(&sdkMux);
    return ESP_OK;
}
esp_err_t twai_node_receive_from_isr(twai_node_handle_t node, twai_frame_t* output) {
    CHECK(node == sdk.node() && sdk.live && output->buffer_len == 8);
    ++sdk.receives;
    sdk.record(Call::RECEIVE);
    if (sdk.rxFail)
        return ESP_FAIL;
    output->header = sdk.rxHeader;
    if (!sdk.rxHeader.rtr) {
        const unsigned count = sdk.rxHeader.dlc > 8 ? 8 : sdk.rxHeader.dlc;
        std::memcpy(output->buffer, sdk.rxBytes, count);
    }
    return ESP_OK;
}

namespace {
void initializationAndConfiguration() {
    reset();
    E::IdfTwaiDriver instance;
    auto driver = instance.driver();
    CHECK(sdk.count == 0 && sdk.transmissions == 0);
    auto invalid = config();
    invalid.rxQueueDepth = 0;
    CHECK(!driver.start(driver.context, invalid, 1) && sdk.count == 0);
    sdk.memoryInternal = false;
    CHECK(!driver.start(driver.context, config(), 1) && sdk.count == 0);
    sdk.memoryInternal = true;
    sdk.taskCore = -1;
    CHECK(!driver.start(driver.context, config(), 1) && sdk.count == 0);
    sdk.taskCore = 1;
    CHECK(driver.start(driver.context, config(2), 7));
    CHECK(sdk.count == 4 && sdk.calls[0] == Call::CREATE && sdk.calls[3] == Call::ENABLE);
    CHECK(sdk.config.io_cfg.tx == 4 && sdk.config.io_cfg.rx == 5);
    CHECK(sdk.config.io_cfg.quanta_clk_out == -1 && sdk.config.io_cfg.bus_off_indicator == -1);
    CHECK(sdk.config.bit_timing.bitrate == 250000 && sdk.config.bit_timing.sp_permill == 800);
    // Recorded contract change (DEC-4): hardware retransmits until ACK. Was 0.
    CHECK(sdk.config.fail_retry_cnt == -1 && sdk.config.tx_queue_depth == 1);
    CHECK(sdk.config.timestamp_resolution_hz == 0 && !sdk.config.flags.enable_self_test);
    CHECK(!sdk.config.flags.enable_loopback && !sdk.config.flags.enable_listen_only);
    CHECK(sdk.filter.id == 0 && sdk.filter.mask == 0);
    CHECK(!driver.start(driver.context, config(), 8));
    CHECK(sdk.transmissions == 0);
    stop(driver);
}

void gpioValidationMatchesPinnedS3() {
    const int invalidPins[] = {-1, -64, 22, 23, 24, 25, 49, 64, 128, INT_MAX};
    for (int pin : invalidPins) {
        for (unsigned tx = 0; tx < 2; ++tx) {
            reset();
            E::IdfTwaiDriver instance;
            auto driver = instance.driver();
            auto value = config();
            if (tx)
                value.txPin = pin;
            else
                value.rxPin = pin;
            CHECK(!driver.start(driver.context, value, 1));
            CHECK(sdk.count == 0 && !sdk.live && sdk.transmissions == 0);
        }
    }
    const int validPins[] = {0, 21, 26, 46, 48};
    for (int pin : validPins) {
        for (unsigned tx = 0; tx < 2; ++tx) {
            reset();
            E::IdfTwaiDriver instance;
            auto driver = instance.driver();
            auto value = config();
            if (tx)
                value.txPin = pin;
            else
                value.rxPin = pin;
            CHECK(driver.start(driver.context, value, 1));
            CHECK((tx ? sdk.config.io_cfg.tx : sdk.config.io_cfg.rx) == pin);
            stop(driver);
        }
    }
}

void actualReceiveCallbacksValidateBeforePublishing() {
    reset();
    E::IdfTwaiDriver instance;
    auto driver = instance.driver();
    CHECK(driver.start(driver.context, config(2), 7));
    // DLC 9..15 moved to classicalDlcAndFramingReachOwnerStorage. Anything above a
    // four-bit field is still not a Classical frame and is still dropped.
    for (uint16_t dlc = 16; dlc <= 22; ++dlc)
        sdk.receive(0x123, dlc);
    sdk.receive(0x123, 8, false, false, true);
    sdk.receive(0x123, 8, false, false, false, true);
    sdk.receive(0x123, 8, false, false, false, false, true);
    sdk.receive(0x800, 8);
    sdk.rxFail = true;
    sdk.receive(0x123, 8);
    sdk.rxFail = false;
    O::ReceivedFrame result;
    result.frame.id = 777;
    CHECK(!driver.receive(driver.context, result, UINT64_MAX) && result.frame.id == 777);
    auto events = driver.events(driver.context);
    CHECK(events.rxDropped == 12 && !events.txDone);
    CHECK(driver.events(driver.context).rxDropped == 0);
    sdk.receive(0x1fffffff, 8, true, true);
    sdk.receive(0x123, 8);
    sdk.receive(0x456, 1); // Configured capacity two, despite sixteen physical slots.
    CHECK(driver.events(driver.context).rxDropped == 1);
    CHECK(driver.receive(driver.context, result, UINT64_MAX));
    CHECK(result.controllerEpoch == 7 && result.frame.id == 0x1fffffff);
    CHECK(result.frame.extended && result.frame.rtr && result.frame.dlc == 8);
    CHECK(result.frame.payloadLength == 0 && !result.receivedTimeKnown && result.receivedUs == 0);
    CHECK(driver.receive(driver.context, result, UINT64_MAX));
    CHECK(result.frame.id == 0x123 && result.frame.payloadLength == 8 && result.frame.data[7] == 8);
    CHECK(!driver.receive(driver.context, result, UINT64_MAX));
    for (unsigned i = 0; i < 20; ++i) {
        sdk.receive(0x100 + i, 1);
        CHECK(driver.receive(driver.context, result, UINT64_MAX) && result.frame.id == 0x100 + i);
    }
    stop(driver);
}

void receiveIntervalsUseOnlyProvenEmptyIngress() {
    reset();
    sdk.nowUs = 10;
    E::IdfTwaiDriver instance;
    auto driver = instance.driver();
    CHECK(driver.start(driver.context, config(), 1));
    sdk.nowUs = 40; // Arbitrarily delayed ISR: no inferred latency bound.
    sdk.receive(0x701, 1);
    O::ReceivedFrame copied;
    copied.frame.id = 777;
    CHECK(!driver.receive(driver.context, copied, 39) && copied.frame.id == 777);
    CHECK(driver.events(driver.context).rxPending);
    CHECK(driver.receive(driver.context, copied, 40));
    CHECK(copied.receiveBoundsKnown && !copied.receivedTimeKnown);
    CHECK(copied.receivedUs == 0 && copied.receivedEarliestUs == 10 && copied.receivedLatestUs == 40);

    // Empty software queue is not evidence that the hardware FIFO is empty.
    sdk.nowUs = 50; sdk.hardwareRxCount = 1;
    CHECK(!driver.receive(driver.context, copied, 50));
    sdk.hardwareRxCount = 0; sdk.nowUs = 60;
    sdk.receive(0x701, 1);
    CHECK(driver.receive(driver.context, copied, 60));
    CHECK(copied.receivedEarliestUs == 10 && copied.receivedLatestUs == 60);

    // A frame completing between the clock sample and register read must prevent
    // a new boundary, even though no callback has populated the software ring.
    sdk.nowUs = 70; sdk.frameDuringClockSample = true;
    CHECK(!driver.receive(driver.context, copied, 70));
    sdk.hardwareRxCount = 0; sdk.nowUs = 80;
    sdk.receive(0x701, 1);
    CHECK(driver.receive(driver.context, copied, 80));
    CHECK(copied.receivedEarliestUs == 10 && copied.receivedLatestUs == 80);

    sdk.nowUs = 90;
    CHECK(!driver.events(driver.context).rxPending);
    sdk.nowUs = 100; sdk.frameAfterEmptyCheck = true;
    driver.events(driver.context); // Callback is deferred until critical exit.
    CHECK(!driver.receive(driver.context, copied, 99));
    CHECK(driver.receive(driver.context, copied, 100));
    CHECK(copied.receiveBoundsKnown && copied.receivedEarliestUs == 100 && copied.receivedLatestUs == 100);
    CHECK(copied.frame.id == 0x701 && copied.frame.data[0] == 0);

    // A restarted controller cannot inherit the old empty-boundary certificate.
    stop(driver);
    sdk.nowUs = 200; sdk.hardwareRxCount = 1;
    CHECK(driver.start(driver.context, config(), 2));
    sdk.hardwareRxCount = 0; sdk.nowUs = 210;
    sdk.receive(0x701, 1);
    CHECK(driver.receive(driver.context, copied, 210));
    CHECK(!copied.receiveBoundsKnown && !copied.receivedTimeKnown && copied.controllerEpoch == 2);
    stop(driver);
}

void receiveClockFailureInhibitsQualification() {
    for (unsigned caseNumber = 0; caseNumber < 3; ++caseNumber) {
        reset(); sdk.nowUs = 10;
        E::IdfTwaiDriver instance;
        auto driver = instance.driver();
        CHECK(driver.start(driver.context, config(), 1));
        if (caseNumber == 2) {
            sdk.nowUs = 30;
            sdk.receive(0x701, 1);
            O::ReceivedFrame first;
            CHECK(driver.receive(driver.context, first, 30));
        }
        sdk.nowUs = caseNumber == 0 ? -1 : caseNumber == 1 ? 9 : 20;
        sdk.receive(0x701, 1);
        O::ReceivedFrame copied;
        CHECK(!driver.receive(driver.context, copied, 100));
        const auto events = driver.events(driver.context);
        CHECK(events.fault && events.rxDropped == 1);
        stop(driver);
    }
}

void actualTxCallbacksOwnAndCorrelate() {
    for (unsigned immediate = 0; immediate < 2; ++immediate) {
        reset();
        sdk.immediate = immediate != 0;
        E::IdfTwaiDriver instance;
        auto driver = instance.driver();
        CHECK(driver.start(driver.context, config(), 1));
        auto value = frame();
        CHECK(driver.submit(driver.context, value, 91) == E::TwaiSubmit::SENT);
        value.data[0] = 0xee;
        CHECK(sdk.pending->buffer[0] == 11 && sdk.pending->header.id == 0x123);
        CHECK(sdk.pending->buffer_len == 8 && sdk.transmissions == 1);
        if (!immediate) {
            CHECK(!driver.events(driver.context).txDone);
            // One outstanding: refused without SDK contact, the first frame is untouched.
            CHECK(driver.submit(driver.context, value, 92) == E::TwaiSubmit::FAILED_UNSENT);
            CHECK(sdk.transmissions == 1);
            sdk.done(true);
        }
        auto events = driver.events(driver.context);
        CHECK(events.txDone && events.transmitted && events.serial == 91 && !events.fault);
        CHECK(!driver.events(driver.context).txDone);
        sdk.immediate = false;
        value.id = 0x1fffffff;
        value.extended = value.rtr = true;
        value.payloadLength = 0;
        CHECK(driver.submit(driver.context, value, 92) == E::TwaiSubmit::SENT);
        CHECK(sdk.pending->header.ide && sdk.pending->header.rtr);
        CHECK(sdk.pending->header.dlc == 8 && sdk.pending->buffer_len == 0);
        sdk.done(false);
        events = driver.events(driver.context);
        CHECK(events.txDone && !events.transmitted && events.serial == 92);
        stop(driver);
    }
    reset();
    E::IdfTwaiDriver instance;
    auto driver = instance.driver();
    CHECK(driver.start(driver.context, config(), 1));
    auto value = frame();
    CHECK(driver.submit(driver.context, value, 1) == E::TwaiSubmit::SENT);
    const twai_frame_t wrong = {};
    sdk.done(true, &wrong);
    CHECK(driver.events(driver.context).fault);
    CHECK(driver.submit(driver.context, value, 2) == E::TwaiSubmit::FAILED_UNSENT);
    sdk.done();
    sdk.done();
    const auto duplicate = driver.events(driver.context);
    CHECK(duplicate.txDone && duplicate.serial == 1 && duplicate.fault);
    stop(driver);
}

void submissionFailureAndBusOffRemainExplicit() {
    reset();
    E::IdfTwaiDriver instance;
    auto driver = instance.driver();
    CHECK(driver.start(driver.context, config(), 1));
    auto value = frame();
    value.fd = true;
    CHECK(driver.submit(driver.context, value, 1) == E::TwaiSubmit::FAILED_UNSENT);
    CHECK(sdk.transmissions == 0);
    value.fd = false;
    sdk.fail = Call::SUBMIT;
    sdk.failEnabled = true;
    sdk.immediate = true; // Rejected SDK handoffs must never schedule completion.
    CHECK(driver.submit(driver.context, value, 1) == E::TwaiSubmit::FAILED_UNSENT);
    CHECK(sdk.pending == nullptr);
    CHECK(instance.lastError() == ESP_FAIL);
    const auto rejected = driver.events(driver.context);
    CHECK(!rejected.txDone && !rejected.fault && !sdk.deferredDone);
    sdk.failEnabled = false;
    sdk.immediate = false;
    CHECK(driver.submit(driver.context, value, 2) == E::TwaiSubmit::SENT);
    sdk.error(0x02);
    sdk.error();
    sdk.busOff();
    auto events = driver.events(driver.context);
    CHECK(events.busOff && events.busErrors == 2 && events.errorFlags == 0x12 && !events.txDone);
    CHECK(driver.submit(driver.context, value, 3) == E::TwaiSubmit::FAILED_UNSENT);
    CHECK(!driver.events(driver.context).busOff);
    const unsigned before = sdk.count;
    stop(driver);
    CHECK(sdk.calls[before] == Call::INFO && sdk.calls[before + 1] == Call::DELETE);
    CHECK(sdk.pending == nullptr && sdk.transmissions == 1);
    CHECK(driver.start(driver.context, config(), 2));
    O::ReceivedFrame copied;
    CHECK(!driver.receive(driver.context, copied, UINT64_MAX));
    CHECK(!driver.events(driver.context).txDone && sdk.transmissions == 1);
    sdk.receive(0x123, 1);
    CHECK(driver.receive(driver.context, copied, UINT64_MAX) && copied.controllerEpoch == 2);
    stop(driver);
}

void unreadRxAndLostEvidenceBlockTheSdkHandoff() {
    reset();
    E::IdfTwaiDriver instance;
    auto driver = instance.driver();
    CHECK(driver.start(driver.context, config(), 1));
    sdk.boot();
    CHECK(driver.events(driver.context).rxPending);
    CHECK(driver.events(driver.context).rxPending); // Snapshot cannot consume RX.
    const unsigned beforeSubmit = sdk.count;
    // Captured RX: a transient deferral with no SDK contact, not a failure.
    CHECK(driver.submit(driver.context, frame(), 1) == E::TwaiSubmit::DEFERRED);
    CHECK(sdk.transmissions == 0);
    CHECK(sdk.count == beforeSubmit); // No rejected or accepted SDK handoff occurred.
    O::ReceivedFrame received;
    CHECK(driver.receive(driver.context, received, UINT64_MAX));
    CHECK(!driver.events(driver.context).rxPending);
    // DLC 15 is now valid, so an out-of-range identifier supplies the same lost RX.
    sdk.receive(0x800, 8);
    CHECK(driver.submit(driver.context, frame(), 1) == E::TwaiSubmit::FAILED_UNSENT);
    CHECK(sdk.transmissions == 0);
    CHECK(driver.events(driver.context).rxDropped == 1);
    CHECK(driver.submit(driver.context, frame(), 2) == E::TwaiSubmit::SENT);
    sdk.done();
    sdk.done(); // Pending is already false; fault itself must gate next submission.
    CHECK(driver.submit(driver.context, frame(), 3) == E::TwaiSubmit::FAILED_UNSENT);
    CHECK(sdk.transmissions == 1);
    CHECK(driver.events(driver.context).fault);
    stop(driver);
}

void guardedHandoffPrecedesNewRxCallbacks() {
    reset();
    E::IdfTwaiDriver instance;
    auto driver = instance.driver();
    CHECK(driver.start(driver.context, config(), 1));
    // Fire the real RX callback at the first point interrupts become unmasked.
    // An exit between the final ingress guard and the SDK call delivers this
    // boot-up before handing off the frame, invalidating the guarded premise.
    sdk.bootOnExit = true;
    CHECK(driver.submit(driver.context, frame(), 1) == E::TwaiSubmit::SENT);
    CHECK(sdk.bootAfterTransmissions == 1 && sdk.transmissions == 1);
    CHECK(!sdk.bootOnExit && sdk.criticalDepth == 0);
    CHECK(driver.events(driver.context).rxPending);
    O::ReceivedFrame boot;
    CHECK(driver.receive(driver.context, boot, UINT64_MAX));
    CHECK(boot.frame.id == 0x701 && boot.frame.payloadLength == 1 && boot.frame.data[0] == 0);
    sdk.done();
    CHECK(driver.events(driver.context).txDone);
    stop(driver);
}

void lifecycleFailuresPreserveOwnershipAndTask() {
    const Call failures[] = {Call::CREATE, Call::REGISTER, Call::FILTER, Call::ENABLE};
    for (Call failure : failures) {
        reset();
        sdk.fail = failure;
        sdk.failEnabled = true;
        E::IdfTwaiDriver instance;
        auto driver = instance.driver();
        CHECK(!driver.start(driver.context, config(), 1));
        CHECK(sdk.transmissions == 0 && instance.lastError() == ESP_FAIL);
        sdk.failEnabled = false;
        // Failed enable may already have made hardware active. Teardown must
        // inspect actual state and disable it before deletion.
        stop(driver);
    }
    const Call teardownFailures[] = {Call::INFO, Call::DISABLE, Call::DELETE};
    for (Call failure : teardownFailures) {
        reset();
        E::IdfTwaiDriver instance;
        auto driver = instance.driver();
        CHECK(driver.start(driver.context, config(), 1));
        CHECK(driver.submit(driver.context, frame(), 1) == E::TwaiSubmit::SENT);
        sdk.receive(0x123, 1);
        const twai_frame_t* retained = sdk.pending;
        sdk.fail = failure;
        sdk.failEnabled = true;
        CHECK(!driver.stop(driver.context) && sdk.live && sdk.pending == retained);
        CHECK(driver.submit(driver.context, frame(), 2) == E::TwaiSubmit::FAILED_UNSENT);
        sdk.failEnabled = false;
        stop(driver);
        CHECK(sdk.pending == nullptr);
        CHECK(driver.start(driver.context, config(), 2));
        O::ReceivedFrame copied;
        CHECK(!driver.receive(driver.context, copied, UINT64_MAX));
        CHECK(!driver.events(driver.context).txDone && sdk.transmissions == 1);
        stop(driver);
    }
    reset();
    E::IdfTwaiDriver instance;
    auto driver = instance.driver();
    CHECK(driver.start(driver.context, config(), 1));
    const unsigned before = sdk.count;
    sdk.currentCore = 0;
    CHECK(!driver.stop(driver.context) && sdk.count == before);
    CHECK(driver.submit(driver.context, frame(), 1) == E::TwaiSubmit::FAILED_UNSENT);
    CHECK(driver.events(driver.context).fault);
    sdk.currentCore = 1;
    sdk.task = reinterpret_cast<TaskHandle_t>(2);
    CHECK(!driver.stop(driver.context) && sdk.count == before);
    sdk.task = reinterpret_cast<TaskHandle_t>(1);
    stop(driver);
}

// Classical DLC is a four-bit wire code: 9..15 are legal and still mean eight bytes.
void classicalDlcAndFramingReachOwnerStorage() {
    reset();
    E::IdfTwaiDriver instance;
    auto driver = instance.driver();
    CHECK(driver.start(driver.context, config(), 3));
    O::ReceivedFrame out;
    for (uint16_t dlc = 0; dlc <= 15; ++dlc) {
        const uint8_t bytes = dlc > 8 ? 8 : uint8_t(dlc);
        sdk.receive(0x123, dlc);
        CHECK(driver.receive(driver.context, out, UINT64_MAX));
        CHECK(out.frame.id == 0x123 && !out.frame.extended && !out.frame.rtr && !out.frame.fd);
        CHECK(out.frame.dlc == bytes && out.frame.payloadLength == bytes);
        CHECK(M::validateFrame(out.frame).isOk() && out.controllerEpoch == 3);
        for (unsigned i = 0; i < 8; ++i)
            CHECK(out.frame.data[i] == (i < bytes ? uint8_t(i + 1) : 0));
        // Remote: the code survives clamped, and the frame never carries a payload.
        sdk.receive(0x1fffffff, dlc, true, true);
        CHECK(driver.receive(driver.context, out, UINT64_MAX));
        CHECK(out.frame.id == 0x1fffffff && out.frame.extended && out.frame.rtr);
        CHECK(out.frame.dlc == bytes && out.frame.payloadLength == 0);
        CHECK(M::validateFrame(out.frame).isOk());
        for (unsigned i = 0; i < 8; ++i)
            CHECK(out.frame.data[i] == 0);
        // Extended data and standard remote are independent of each other.
        sdk.receive(0x1abcdef0, dlc, true, false);
        CHECK(driver.receive(driver.context, out, UINT64_MAX));
        CHECK(out.frame.id == 0x1abcdef0 && out.frame.extended && !out.frame.rtr);
        CHECK(out.frame.dlc == bytes && out.frame.payloadLength == bytes);
        sdk.receive(0x7ff, dlc, false, true);
        CHECK(driver.receive(driver.context, out, UINT64_MAX));
        CHECK(out.frame.id == 0x7ff && !out.frame.extended && out.frame.rtr);
        CHECK(out.frame.dlc == bytes && out.frame.payloadLength == 0);
    }
    auto events = driver.events(driver.context);
    CHECK(events.rxDropped == 0 && !events.fault);
    // Rejected explicitly as a counted drop, at every DLC class: FD, BRS, ESI, bad IDs.
    const uint16_t codes[] = {0, 8, 15};
    unsigned expected = 0;
    for (uint16_t dlc : codes) {
        sdk.receive(0x123, dlc, false, false, true);
        sdk.receive(0x123, dlc, false, false, false, true);
        sdk.receive(0x123, dlc, false, false, false, false, true);
        sdk.receive(0x800, dlc);
        sdk.receive(0x20000000, dlc, true);
        expected += 5;
    }
    out = O::ReceivedFrame();
    out.frame.id = 777;
    CHECK(!driver.receive(driver.context, out, UINT64_MAX) && out.frame.id == 777);
    events = driver.events(driver.context);
    CHECK(events.rxDropped == expected);
    // Transmit framing reaches the SDK unchanged.
    auto value = frame();
    value.id = 0x7ff;
    value.dlc = value.payloadLength = 3;
    CHECK(driver.submit(driver.context, value, 1) == E::TwaiSubmit::SENT);
    CHECK(sdk.pending->header.id == 0x7ff && !sdk.pending->header.ide && !sdk.pending->header.rtr);
    CHECK(sdk.pending->header.dlc == 3 && sdk.pending->buffer_len == 3);
    CHECK(sdk.pending->buffer[2] == 13);
    sdk.done();
    CHECK(driver.events(driver.context).txDone);
    value.id = 0x1fffffff;
    value.extended = true;
    CHECK(driver.submit(driver.context, value, 2) == E::TwaiSubmit::SENT);
    CHECK(sdk.pending->header.id == 0x1fffffff && sdk.pending->header.ide);
    CHECK(!sdk.pending->header.rtr && sdk.pending->header.dlc == 3);
    sdk.done();
    CHECK(driver.events(driver.context).txDone);
    value.rtr = true;
    value.dlc = 5;
    value.payloadLength = 0;
    CHECK(driver.submit(driver.context, value, 3) == E::TwaiSubmit::SENT);
    CHECK(sdk.pending->header.ide && sdk.pending->header.rtr);
    CHECK(sdk.pending->header.dlc == 5 && sdk.pending->buffer_len == 0);
    sdk.done();
    CHECK(driver.events(driver.context).txDone);
    // Invalid transmit shapes are refused before any SDK contact.
    const unsigned before = sdk.transmissions;
    M::Frame bad = frame();
    bad.fd = true;
    CHECK(driver.submit(driver.context, bad, 4) == E::TwaiSubmit::FAILED_UNSENT);
    bad = frame();
    bad.dlc = 9;
    CHECK(driver.submit(driver.context, bad, 4) == E::TwaiSubmit::FAILED_UNSENT);
    bad = frame();
    bad.id = 0x800;
    CHECK(driver.submit(driver.context, bad, 4) == E::TwaiSubmit::FAILED_UNSENT);
    CHECK(sdk.transmissions == before);
    stop(driver);
}

// Hardware retransmission makes bus-off a normal path, and it can arrive between the
// state query and the disable. Deletion accepts that state, so stop still completes.
void stopToleratesBusOffBetweenQueryAndDisable() {
    reset();
    {
        E::IdfTwaiDriver instance;
        auto driver = instance.driver();
        CHECK(driver.start(driver.context, config(), 1));
        CHECK(driver.submit(driver.context, frame(), 1) == E::TwaiSubmit::SENT);
        const unsigned before = sdk.count;
        sdk.busOffAtDisable = true;
        CHECK(driver.stop(driver.context));
        CHECK(sdk.count == before + 4);
        CHECK(sdk.calls[before] == Call::INFO && sdk.calls[before + 1] == Call::DISABLE);
        CHECK(sdk.calls[before + 2] == Call::INFO && sdk.calls[before + 3] == Call::DELETE);
        CHECK(!sdk.live && sdk.pending == nullptr);
        CHECK(sdk.transmissions == 1);
        // A fresh node carries nothing of the old one.
        CHECK(driver.start(driver.context, config(), 2));
        CHECK(!driver.events(driver.context).txDone);
        CHECK(driver.submit(driver.context, frame(), 2) == E::TwaiSubmit::SENT);
        stop(driver);
    }
    // INVALID_STATE without bus-off is not this race: stop fails and retains everything.
    reset();
    {
        E::IdfTwaiDriver instance;
        auto driver = instance.driver();
        CHECK(driver.start(driver.context, config(), 1));
        CHECK(driver.submit(driver.context, frame(), 1) == E::TwaiSubmit::SENT);
        const twai_frame_t* retained = sdk.pending;
        const unsigned before = sdk.count;
        sdk.invalidStateAtDisable = true;
        CHECK(!driver.stop(driver.context));
        CHECK(sdk.count == before + 3);
        CHECK(sdk.calls[before] == Call::INFO && sdk.calls[before + 1] == Call::DISABLE);
        CHECK(sdk.calls[before + 2] == Call::INFO);
        // The successful query must not hide why the stop failed.
        CHECK(instance.lastError() == ESP_ERR_INVALID_STATE);
        CHECK(sdk.live && sdk.pending == retained);
        CHECK(driver.submit(driver.context, frame(), 2) == E::TwaiSubmit::FAILED_UNSENT);
        sdk.invalidStateAtDisable = false;
        stop(driver); // A later explicit attempt can still finish the same node.
        CHECK(sdk.pending == nullptr && instance.lastError() == ESP_OK);
    }
    // The second query itself fails: the state stays unknown, so nothing is deleted.
    reset();
    {
        E::IdfTwaiDriver instance;
        auto driver = instance.driver();
        CHECK(driver.start(driver.context, config(), 1));
        CHECK(driver.submit(driver.context, frame(), 1) == E::TwaiSubmit::SENT);
        const twai_frame_t* retained = sdk.pending;
        sdk.busOffAtDisable = true;
        sdk.failInfoAtCall = sdk.infoCalls + 2;
        CHECK(!driver.stop(driver.context));
        CHECK(instance.lastError() == ESP_FAIL);
        CHECK(sdk.live && sdk.pending == retained);
        sdk.failInfoAtCall = 0;
        stop(driver); // Now plainly bus-off: query, then delete without a disable.
        CHECK(sdk.pending == nullptr);
    }
    // Deletion still fails honestly after the tolerated race.
    reset();
    {
        E::IdfTwaiDriver instance;
        auto driver = instance.driver();
        CHECK(driver.start(driver.context, config(), 1));
        CHECK(driver.submit(driver.context, frame(), 1) == E::TwaiSubmit::SENT);
        const twai_frame_t* retained = sdk.pending;
        sdk.busOffAtDisable = true;
        sdk.fail = Call::DELETE;
        sdk.failEnabled = true;
        CHECK(!driver.stop(driver.context));
        CHECK(sdk.live && sdk.pending == retained && instance.lastError() == ESP_FAIL);
        sdk.failEnabled = false;
        stop(driver);
    }
    // A disable error other than INVALID_STATE never takes the tolerant path.
    reset();
    {
        E::IdfTwaiDriver instance;
        auto driver = instance.driver();
        CHECK(driver.start(driver.context, config(), 1));
        const unsigned before = sdk.count;
        sdk.fail = Call::DISABLE;
        sdk.failEnabled = true;
        CHECK(!driver.stop(driver.context));
        CHECK(sdk.count == before + 2 && sdk.live);
        CHECK(instance.lastError() == ESP_FAIL);
        sdk.failEnabled = false;
        stop(driver);
    }
}

// The driver copies is_tx_success verbatim. The SDK source does not establish what it
// reads when a frame ends with bus-off, so this field cannot be delivery evidence there.
void finalReportOnBusOffIsCopiedNotInterpreted() {
    for (unsigned success = 0; success < 2; ++success) {
        reset();
        E::IdfTwaiDriver instance;
        auto driver = instance.driver();
        CHECK(driver.start(driver.context, config(), 1));
        CHECK(driver.submit(driver.context, frame(), 7) == E::TwaiSubmit::SENT);
        sdk.done(success != 0);
        sdk.busOff();
        const auto events = driver.events(driver.context);
        CHECK(events.txDone && events.serial == 7 && events.busOff);
        CHECK(events.transmitted == (success != 0));
        CHECK(sdk.transmissions == 1);
        CHECK(driver.submit(driver.context, frame(), 8) == E::TwaiSubmit::FAILED_UNSENT);
        CHECK(sdk.transmissions == 1);
        stop(driver);
    }
}

// SDK review: a frame completing just before stop has its report discarded. The driver
// offers no "not delivered" fact: the mailbox is simply gone with the old node.
void reportOfAFrameCompletingBeforeStopIsDiscarded() {
    reset();
    E::IdfTwaiDriver instance;
    auto driver = instance.driver();
    CHECK(driver.start(driver.context, config(), 1));
    CHECK(driver.submit(driver.context, frame(), 5) == E::TwaiSubmit::SENT);
    sdk.done(true);
    stop(driver);
    CHECK(sdk.transmissions == 1 && sdk.pending == nullptr);
    CHECK(driver.start(driver.context, config(), 2));
    const auto events = driver.events(driver.context);
    CHECK(!events.txDone && !events.transmitted && events.serial == 0 && !events.fault);
    stop(driver);
}
} // namespace

int main() {
    initializationAndConfiguration();
    gpioValidationMatchesPinnedS3();
    actualReceiveCallbacksValidateBeforePublishing();
    receiveIntervalsUseOnlyProvenEmptyIngress();
    receiveClockFailureInhibitsQualification();
    actualTxCallbacksOwnAndCorrelate();
    submissionFailureAndBusOffRemainExplicit();
    unreadRxAndLostEvidenceBlockTheSdkHandoff();
    guardedHandoffPrecedesNewRxCallbacks();
    lifecycleFailuresPreserveOwnershipAndTask();
    classicalDlcAndFramingReachOwnerStorage();
    stopToleratesBusOffBetweenQueryAndDisable();
    finalReportOnBusOffIsCopiedNotInterpreted();
    reportOfAFrameCompletingBeforeStopIsDiscarded();
    std::printf("TWAI SDK shim host simulation: %u checks; host sizeof(IdfTwaiDriver)=%zu\n",
                checks, sizeof(E::IdfTwaiDriver));
}
