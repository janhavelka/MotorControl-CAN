// SPDX-License-Identifier: MIT
#pragma once
#include "esp_twai.h"
typedef int gpio_num_t;
typedef int twai_clock_source_t;
struct twai_timing_basic_config_t {
    uint32_t bitrate;
    uint16_t sp_permill, ssp_permill;
};
struct twai_onchip_node_config_t {
    struct {
        gpio_num_t tx, rx, quanta_clk_out, bus_off_indicator;
    } io_cfg;
    twai_clock_source_t clk_src;
    twai_timing_basic_config_t bit_timing, data_timing;
    uint32_t timestamp_resolution_hz;
    int8_t fail_retry_cnt;
    uint32_t tx_queue_depth;
    int intr_priority;
    struct {
        uint32_t enable_self_test : 1, enable_loopback : 1, enable_listen_only : 1,
            no_receive_rtr : 1, sleep_allow_pd : 1;
    } flags;
};
esp_err_t twai_new_node_onchip(const twai_onchip_node_config_t*, twai_node_handle_t*);
