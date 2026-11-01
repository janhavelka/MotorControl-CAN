// SPDX-License-Identifier: MIT
#pragma once
typedef void* TaskHandle_t;
TaskHandle_t xTaskGetCurrentTaskHandle();
int xTaskGetCoreID(TaskHandle_t);
int xPortGetCoreID();
