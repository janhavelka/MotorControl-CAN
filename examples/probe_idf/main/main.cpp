// SPDX-License-Identifier: MIT
#include "EspProbeMain.h"
#include "FirmwareBoard.h"

extern "C" void app_main() {
    MotorControlCANExample::startProbeFirmware(MotorControlCANExample::firmwareBoardConfig());
}
