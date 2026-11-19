// SPDX-License-Identifier: MIT
#pragma once
typedef void* TaskHandle_t;
TaskHandle_t xTaskGetCurrentTaskHandle();
int xTaskGetCoreID(TaskHandle_t);
int xPortGetCoreID();
// Prompt 56 event wake: the fake records each notification and reports the scripted yield flag.
void vTaskNotifyGiveFromISR(TaskHandle_t, BaseType_t*);
