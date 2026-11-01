// SPDX-License-Identifier: MIT
#pragma once
#include "freertos/FreeRTOS.h"
#include <stddef.h>
typedef int esp_err_t;
#define ESP_OK 0
struct usb_serial_jtag_driver_config_t {
    uint32_t tx_buffer_size;
    uint32_t rx_buffer_size;
};
bool usb_serial_jtag_is_driver_installed();
esp_err_t usb_serial_jtag_driver_install(usb_serial_jtag_driver_config_t*);
esp_err_t usb_serial_jtag_driver_uninstall();
int usb_serial_jtag_read_bytes(void*, uint32_t, TickType_t);
int usb_serial_jtag_write_bytes(const void*, size_t, TickType_t);
