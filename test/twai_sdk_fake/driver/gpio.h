// SPDX-License-Identifier: MIT
#pragma once
// Pinned ESP32-S3 gpio_num.h / soc_caps.h: GPIO22..25 do not exist;
// GPIO46 supports output. Match the SDK's shift macros, including their need
// for the caller to exclude oversized integers before evaluating a shift.
enum { GPIO_NUM_MAX = 49 };
static const unsigned long long SOC_GPIO_VALID_GPIO_MASK =
    0x1ffffffffffffULL & ~((1ULL << 22) | (1ULL << 23) | (1ULL << 24) | (1ULL << 25));
static const unsigned long long SOC_GPIO_VALID_OUTPUT_GPIO_MASK = SOC_GPIO_VALID_GPIO_MASK;
#define GPIO_IS_VALID_OUTPUT_GPIO(p)                                                               \
    ((p) >= 0 && (((1ULL << (p)) & SOC_GPIO_VALID_OUTPUT_GPIO_MASK) != 0))
#define GPIO_IS_VALID_GPIO(p) ((p) >= 0 && (((1ULL << (p)) & SOC_GPIO_VALID_GPIO_MASK) != 0))
