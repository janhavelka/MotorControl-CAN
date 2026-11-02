// SPDX-License-Identifier: MIT
#include "ProbePlatform.h"

namespace MotorControlCANExample {
namespace {
void addSaturating(uint32_t& value) noexcept {
    if (value != UINT32_MAX)
        ++value;
}
} // namespace

ProbePlatform::ProbePlatform(const ProbeCallbacks& callbacks) noexcept : callbacks_(callbacks) {}
bool ProbePlatform::nowUs(uint64_t& output) const noexcept {
    uint64_t candidate = 0;
    if (!callbacks_.nowUs || !callbacks_.nowUs(callbacks_.context, candidate))
        return false;
    output = candidate;
    return true;
}
void ProbePlatform::service() noexcept {
    for (unsigned i = 0; callbacks_.readByte && i < INPUT_BUDGET; ++i) {
        uint8_t byte = 0;
        if (!callbacks_.readByte(callbacks_.context, byte))
            break;
        if (stats_.inputUsed == INPUT_CAPACITY) {
            addSaturating(stats_.inputDropped);
        } else {
            input_[(inputHead_ + stats_.inputUsed) % INPUT_CAPACITY] = byte;
            ++stats_.inputUsed;
        }
    }
    for (unsigned i = 0; callbacks_.writeByte && stats_.outputUsed && i < OUTPUT_BUDGET; ++i) {
        if (!callbacks_.writeByte(callbacks_.context, output_[outputHead_]))
            break;
        outputHead_ = (outputHead_ + 1) % OUTPUT_CAPACITY;
        --stats_.outputUsed;
    }
    if (callbacks_.idle)
        callbacks_.idle(callbacks_.context);
}
bool ProbePlatform::takeInput(uint8_t& output) noexcept {
    if (!stats_.inputUsed)
        return false;
    output = input_[inputHead_];
    inputHead_ = (inputHead_ + 1) % INPUT_CAPACITY;
    --stats_.inputUsed;
    return true;
}
bool ProbePlatform::writeOutput(const uint8_t* bytes, size_t size) noexcept {
    if (!bytes && size)
        return false;
    if (size > static_cast<size_t>(OUTPUT_CAPACITY - stats_.outputUsed)) {
        addSaturating(stats_.outputRejected);
        return false;
    }
    for (size_t i = 0; i < size; ++i)
        output_[(outputHead_ + stats_.outputUsed + i) % OUTPUT_CAPACITY] = bytes[i];
    stats_.outputUsed += static_cast<uint16_t>(size);
    return true;
}
} // namespace MotorControlCANExample
