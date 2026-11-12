// SPDX-License-Identifier: MIT
// Deliberately mismatched unit for the capacity_mismatch test: compiled with the header defaults
// (two channels) and linked against an adapter build with eight. It must not link; see
// test/capacity_mismatch.cmake. It is never run.
#include "SdoClient.h"

int main() {
    // A store this unit lays out for two channels, used through code compiled for eight.
    static MotorControlCAN::CANOPEN::SdoClient services;
    return services.channel(0) ? 1 : 0;
}
