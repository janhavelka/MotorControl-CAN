// SPDX-License-Identifier: MIT
// Compile-only ABI measurements. No objects are constructed or firmware linked.
#include "IdfTwaiDriver.h"
extern "C" {
unsigned char motorcontrolcan_twai_driver_bytes[sizeof(MotorControlCANExample::IdfTwaiDriver)];
unsigned char motorcontrolcan_twai_adapter_bytes[sizeof(MotorControlCANExample::TwaiAdapter)];
}
