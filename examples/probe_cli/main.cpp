// SPDX-License-Identifier: MIT
#include "EspProbeMain.h"
#include "FirmwareBoard.h"
// Arduino's DISABLED macro must not rewrite the shared heartbeat enum declaration.
#include <Arduino.h>

void setup() {
    MotorControlCANExample::startProbeFirmware(MotorControlCANExample::firmwareBoardConfig());
}
void loop() {
    // All CAN/application/USB work belongs to the one static pinned owner task.
    vTaskDelay(1);
}
