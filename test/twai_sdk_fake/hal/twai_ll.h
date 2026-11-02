// SPDX-License-Identifier: MIT
// Read-only private register seam, simulated only for the pinned SDK shim tests.
#pragma once
#include <cstdint>
struct twai_dev_t {};
extern twai_dev_t fakeTwaiHardware;
#define TWAI_LL_GET_HW(controller_id) ((controller_id) == 0 ? &fakeTwaiHardware : nullptr)
uint32_t twai_ll_get_rx_msg_count(twai_dev_t* hardware);
