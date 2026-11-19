// SPDX-License-Identifier: MIT
// Host fake of the ESP-IDF 5.5.5 cycle counter (esp_cpu.h); a counter that advances per read.
#pragma once
#include <stdint.h>
typedef uint32_t esp_cpu_cycle_count_t;
inline esp_cpu_cycle_count_t esp_cpu_get_cycle_count() {
    static esp_cpu_cycle_count_t counter = 0;
    return counter += 3;
}
