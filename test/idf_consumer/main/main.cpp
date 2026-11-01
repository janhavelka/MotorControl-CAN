// SPDX-License-Identifier: MIT
// Compile public headers through the component's exported include directory.
#include <MotorControlCAN/MotorControlCAN.h>
#include <MotorControlCAN/ReadOperation.h>
#include <MotorControlCAN/canopen/Network.h>
#include <MotorControlCAN/canopen/Pdo.h>
#include <MotorControlCAN/canopen/PdoState.h>
#include <MotorControlCAN/canopen/SdoTransfer.h>
#include <MotorControlCAN/profiles/lichuan_c/Catalogue.h>
#include <MotorControlCAN/profiles/lichuan_c/DriverSettings.h>
#include <MotorControlCAN/profiles/lichuan_c/IoSettings.h>
#include <MotorControlCAN/profiles/lichuan_c/Objects.h>
#include <MotorControlCAN/profiles/lichuan_c/Pdo.h>
#include <MotorControlCAN/profiles/lichuan_c/Reads.h>
#include <MotorControlCAN/profiles/lichuan_c/Settings.h>
#include <MotorControlCAN/profiles/lichuan_c/Tuning.h>
#include <MotorControlCAN/profiles/lichuan_c/Types.h>

#include <cstdlib>

namespace {
// Retained writes keep checked calls observable in an optimized link.
volatile int64_t retainedExactTarget = 0;
volatile uint32_t retainedChecks = 0;
}

extern "C" void app_main() {
    using namespace MotorControlCAN;
    AxisConfig axis;
    axis.binding.logicalAxisId = 91;
    axis.binding.endpoint.profile = 17;
    axis.binding.endpoint.address = 1000;
    axis.binding.endpoint.subaxis = 2;
    axis.binding.bindingGeneration = 3;
    axis.binding.configurationGeneration = 4;
    if (!validateAxisConfig(axis).isOk())
        std::abort();

    PositionRequest request;
    request.configurationGeneration = axis.binding.configurationGeneration;
    request.value = Rational(INT64_C(9007199254740993));
    PreparedTarget prepared;
    const Status status = preparePosition(request, axis, nullptr, prepared);
    if (!status.isOk() || status.code != Err::OK || !prepared.exactArithmetic ||
        prepared.effectiveNative != request.value.numerator ||
        prepared.endpointNative != request.value.numerator ||
        !sameBinding(prepared.binding, axis.binding))
        std::abort();
    retainedExactTarget = prepared.effectiveNative;
    retainedChecks = 1;

    ++request.configurationGeneration;
    const Status stale = preparePosition(request, axis, nullptr, prepared);
    if (stale.code != Err::INVALID_CONFIG ||
        stale.detail != static_cast<int32_t>(AxisError::STALE_GENERATION) ||
        prepared.effectiveNative != retainedExactTarget)
        std::abort();
    retainedChecks = 3;

    Frame frame;
    frame.id = 0x1fffffff;
    frame.extended = true;
    frame.rtr = true;
    frame.dlc = 8;
    if (!validateFrame(frame).isOk())
        std::abort();
    frame.payloadLength = 1;
    if (validateFrame(frame).code != Err::FRAME_ERROR)
        std::abort();
    retainedChecks = 7;
}
