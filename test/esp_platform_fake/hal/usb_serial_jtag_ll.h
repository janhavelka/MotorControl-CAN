// SPDX-License-Identifier: MIT
#pragma once
#include <stdint.h>
#define USB_SERIAL_JTAG_LL_INTR_MASK 0x7ffffU
void usb_serial_jtag_ll_disable_intr_mask(uint32_t);
