// SPDX-License-Identifier: MIT
#pragma once
// Pinned ESP32-S2 soc_caps.h: 47 pads, GPIO22..25 absent, GPIO46 input-only.
// Like the SDK macros, these shift by the pin and need a bounded caller.
typedef int esp_err_t;
#ifndef ESP_OK
#define ESP_OK 0
#endif
typedef int gpio_num_t;
enum { GPIO_NUM_MAX = 47 };
enum gpio_mode_t { GPIO_MODE_DISABLE = 0, GPIO_MODE_INPUT = 1, GPIO_MODE_OUTPUT = 2 };
static const unsigned long long SOC_GPIO_VALID_GPIO_MASK =
    0x7fffffffffffULL & ~((1ULL << 22) | (1ULL << 23) | (1ULL << 24) | (1ULL << 25));
static const unsigned long long SOC_GPIO_VALID_OUTPUT_GPIO_MASK =
    SOC_GPIO_VALID_GPIO_MASK & ~(1ULL << 46);
#define GPIO_IS_VALID_OUTPUT_GPIO(p)                                                               \
    ((p) >= 0 && (((1ULL << (p)) & SOC_GPIO_VALID_OUTPUT_GPIO_MASK) != 0))
esp_err_t gpio_set_level(gpio_num_t, unsigned);
esp_err_t gpio_set_direction(gpio_num_t, gpio_mode_t);
