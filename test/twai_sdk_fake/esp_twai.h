// SPDX-License-Identifier: MIT
// Narrow host simulation of the reviewed public field/signature subset.
// This is not an ESP-IDF compile or a hardware controller implementation.
#pragma once
#include <cstddef>
#include <cstdint>
typedef int esp_err_t;
enum { ESP_OK = 0, ESP_FAIL = -1, ESP_ERR_INVALID_STATE = 0x103 };
typedef struct twai_node_base* twai_node_handle_t;
enum twai_error_state_t {
    TWAI_ERROR_ACTIVE,
    TWAI_ERROR_WARNING,
    TWAI_ERROR_PASSIVE,
    TWAI_ERROR_BUS_OFF
};
struct twai_frame_header_t {
    uint32_t id;
    uint16_t dlc;
    uint32_t ide : 1, rtr : 1, fdf : 1, brs : 1, esi : 1;
    union {
        uint64_t timestamp;
        uint64_t trigger_time;
    };
};
struct twai_frame_t {
    twai_frame_header_t header;
    uint8_t* buffer;
    std::size_t buffer_len;
};
struct twai_tx_done_event_data_t {
    bool is_tx_success;
    const twai_frame_t* done_tx_frame;
};
struct twai_rx_done_event_data_t {};
struct twai_state_change_event_data_t {
    twai_error_state_t old_sta, new_sta;
};
union twai_error_flags_t {
    uint32_t val;
};
struct twai_error_event_data_t {
    twai_error_flags_t err_flags;
};
struct twai_event_callbacks_t {
    bool (*on_tx_done)(twai_node_handle_t, const twai_tx_done_event_data_t*, void*);
    bool (*on_rx_done)(twai_node_handle_t, const twai_rx_done_event_data_t*, void*);
    bool (*on_state_change)(twai_node_handle_t, const twai_state_change_event_data_t*, void*);
    bool (*on_error)(twai_node_handle_t, const twai_error_event_data_t*, void*);
};
struct twai_node_status_t {
    twai_error_state_t state;
    uint16_t tx_error_count, rx_error_count;
    uint32_t tx_queue_remaining;
};
struct twai_node_record_t {
    uint32_t bus_err_num;
};
struct twai_mask_filter_config_t {
    uint32_t id;
    uint32_t mask;
    uint32_t is_ext : 1, no_classic : 1, no_fd : 1, dual_filter : 1;
};
esp_err_t twai_node_register_event_callbacks(twai_node_handle_t, const twai_event_callbacks_t*,
                                             void*);
esp_err_t twai_node_config_mask_filter(twai_node_handle_t, uint8_t,
                                       const twai_mask_filter_config_t*);
esp_err_t twai_node_enable(twai_node_handle_t);
esp_err_t twai_node_disable(twai_node_handle_t);
esp_err_t twai_node_delete(twai_node_handle_t);
esp_err_t twai_node_get_info(twai_node_handle_t, twai_node_status_t*, twai_node_record_t*);
esp_err_t twai_node_transmit(twai_node_handle_t, const twai_frame_t*, int);
esp_err_t twai_node_receive_from_isr(twai_node_handle_t, twai_frame_t*);
