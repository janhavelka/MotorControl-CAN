// SPDX-License-Identifier: MIT
#include "WiredStopOutput.h"

namespace MotorControlCANExample {
MotorControlCAN::Status WiredStopOutput::setAsserted(bool asserted) noexcept {
    if (!signal_.wiringKnown || !signal_.writeLevel)
        return MotorControlCAN::Status(MotorControlCAN::Err::INVALID_CONFIG, 0,
                                       "explicit wired signal and active level required");
    result_.attempted = true;
    result_.asserted = asserted;
    result_.electricalHigh = asserted == signal_.activeHigh;
    result_.status = signal_.writeLevel(signal_.context, result_.electricalHigh);
    result_.uncertain = !bool(result_.status);
    return result_.status;
}
} // namespace MotorControlCANExample
