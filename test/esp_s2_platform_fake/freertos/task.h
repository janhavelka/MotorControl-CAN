// SPDX-License-Identifier: MIT
#pragma once
#include "FreeRTOS.h"
typedef void* TaskHandle_t;
TaskHandle_t xTaskGetCurrentTaskHandle();
int xTaskGetCoreID(TaskHandle_t);
int xPortGetCoreID();
void vTaskDelay(TickType_t);
