// SPDX-License-Identifier: MIT
#pragma once
#include "FreeRTOS.h"
typedef void* TaskHandle_t;
TaskHandle_t xTaskGetCurrentTaskHandle();
int xTaskGetCoreID(TaskHandle_t);
int xPortGetCoreID();
void vTaskDelay(TickType_t);
// Prompt 56 event wake: the bounded notification wait in EspProbePlatform::idle.
uint32_t ulTaskNotifyTake(BaseType_t, TickType_t);
