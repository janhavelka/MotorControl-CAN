// SPDX-License-Identifier: MIT
#include <MotorControlCAN/Native.h>
#include <limits.h>
#include <stddef.h>
#include <stdint.h>

namespace MotorControlCAN {
namespace {
bool overlaps(const void* a, size_t na, const void* b, size_t nb) {
    const uintptr_t x = reinterpret_cast<uintptr_t>(a), y = reinterpret_cast<uintptr_t>(b);
    return x <= y ? y - x < na : x - y < nb;
}
}
Status convertNativeRamp(const NativeRamp& input, RampKind destinationKind,
                         AccelerationUnit destinationUnit, const UnitConfig& config,
                         NativeRamp& output) noexcept {
    if (overlaps(&output, sizeof output, &input, sizeof input) ||
        overlaps(&output, sizeof output, &config, sizeof config))
        return Status(Err::INVALID_CONFIG, int32_t(UnitError::INVALID_ARGUMENT), "ramp output overlaps input");
    if (input.kind > RampKind::ACCELERATION || destinationKind > RampKind::ACCELERATION ||
        !input.value.denominator || (input.kind == RampKind::TIME_MS && input.value.numerator < 0))
        return Status(Err::INVALID_CONFIG, int32_t(UnitError::INVALID_ARGUMENT), "invalid native ramp");
    if (input.kind != destinationKind)
        return Status(Err::UNSUPPORTED, int32_t(UnitError::INVALID_UNIT), "time is not acceleration");
    NativeRamp candidate = input;
    if (input.kind == RampKind::ACCELERATION) {
        const Status converted = convertAcceleration(input.value, input.accelerationUnit,
                                                      destinationUnit, config, candidate.value);
        if (!converted)
            return converted;
        candidate.accelerationUnit = destinationUnit;
    }
    output = candidate;
    return Ok();
}
Status checkNativeInteger(int64_t value, uint8_t widthBytes, bool signedValue) noexcept {
    if (widthBytes != 1 && widthBytes != 2 && widthBytes != 4 && widthBytes != 8)
        return Status(Err::INVALID_CONFIG, int32_t(UnitError::INVALID_ARGUMENT), "invalid native width");
    if (widthBytes == 8) {
        if (signedValue || value >= 0)
            return Ok();
    } else if (signedValue) {
        const int64_t bound = int64_t(1) << (unsigned(widthBytes) * 8 - 1);
        if (value >= -bound && value < bound)
            return Ok();
    } else if (value >= 0 && uint64_t(value) < (UINT64_C(1) << (unsigned(widthBytes) * 8))) {
        return Ok();
    }
    return Status(Err::ILLEGAL_VALUE, int32_t(UnitError::ARITHMETIC_OVERFLOW), "native integer out of range");
}
} // namespace MotorControlCAN
