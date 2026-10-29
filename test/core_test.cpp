// SPDX-License-Identifier: MIT
// Checks remain active with NDEBUG; production functions perform no reporting I/O.
#include <MotorControlCAN/Status.h>
#include <MotorControlCAN/Endpoint.h>
#include <MotorControlCAN/can/Frame.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>

#define CHECK(condition) do { if (!(condition)) { std::fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #condition); std::abort(); } } while (false)

namespace M = MotorControlCAN;

namespace {

constexpr M::Status success = M::Ok();
static_assert(success.code == M::Err::OK && success.detail == 0, "constexpr default success");
static_assert(success.isOk() && static_cast<bool>(success), "constexpr success predicates");
constexpr M::Status failure(M::Err::ILLEGAL_VALUE, -7, "out of range");
static_assert(!failure.isOk() && !static_cast<bool>(failure), "constexpr failure predicates");

void statusContracts() {
    CHECK(std::strcmp(success.msg, "") == 0);
    CHECK(failure.detail == -7 && std::strcmp(failure.msg, "out of range") == 0);
    const M::Err codes[] = { M::Err::OK, M::Err::INVALID_CONFIG, M::Err::ILLEGAL_VALUE,
                            M::Err::UNSUPPORTED, M::Err::FRAME_ERROR, M::Err::PROTOCOL_ERROR };
    const char* labels[] = { "OK", "INVALID_CONFIG", "ILLEGAL_VALUE", "UNSUPPORTED",
                             "FRAME_ERROR", "PROTOCOL_ERROR" };
    for (unsigned i = 0; i < sizeof codes / sizeof codes[0]; ++i)
        CHECK(std::strcmp(M::errToString(codes[i]), labels[i]) == 0);
    CHECK(std::strcmp(M::errToString(static_cast<M::Err>(255)), "UNKNOWN") == 0);
}

M::Binding binding() {
    M::Binding value;
    value.logicalAxisId = 91;
    value.endpoint.profile = 17;
    value.bindingGeneration = 3;
    value.configurationGeneration = 4;
    return value;
}

void checkInvalidBinding(const M::Binding& value) {
    const M::Status result = M::validateBinding(value);
    CHECK(result.code == M::Err::INVALID_CONFIG);
    CHECK(result.detail == static_cast<int32_t>(M::BindingError::INVALID_TARGET));
    CHECK(result.detail == 1); // Retained proof's public numeric reason remains stable.
}

void bindingContracts() {
    const M::Binding empty;
    checkInvalidBinding(empty);
    CHECK(M::sameBinding(empty, empty)); // Comparison deliberately does not validate.

    const M::Binding original = binding();
    CHECK(M::validateBinding(original));
    CHECK(M::sameBinding(original, original));

    M::Binding value = original;
    value.logicalAxisId = 0;
    checkInvalidBinding(value);
    value = original;
    value.endpoint.profile = 0;
    checkInvalidBinding(value);
    value = original;
    value.bindingGeneration = 0;
    checkInvalidBinding(value);
    value = original;
    value.configurationGeneration = 0;
    checkInvalidBinding(value);
    value = original;
    value.endpoint.protocol = static_cast<M::Protocol>(255);
    checkInvalidBinding(value);

    // Every identity field independently participates in correlation.
    value = original;
    ++value.logicalAxisId;
    CHECK(!M::sameBinding(original, value));
    value = original;
    ++value.bindingGeneration;
    CHECK(!M::sameBinding(original, value));
    value = original;
    ++value.configurationGeneration;
    CHECK(!M::sameBinding(original, value));
    value = original;
    value.endpoint.protocol = M::Protocol::PROPRIETARY;
    CHECK(!M::sameBinding(original, value));
    value = original;
    ++value.endpoint.bus;
    CHECK(!M::sameBinding(original, value));
    value = original;
    ++value.endpoint.profile;
    CHECK(!M::sameBinding(original, value));
    value = original;
    ++value.endpoint.address;
    CHECK(!M::sameBinding(original, value));
    value = original;
    ++value.endpoint.channel;
    CHECK(!M::sameBinding(original, value));
    value = original;
    ++value.endpoint.subaxis;
    CHECK(!M::sameBinding(original, value));

    // Generic endpoint identity does not import CANopen node limits or motor geometry.
    value = original;
    value.endpoint.address = UINT32_MAX;
    value.endpoint.channel = UINT16_MAX;
    value.endpoint.subaxis = UINT16_MAX;
    CHECK(M::validateBinding(value));
    value.endpoint.protocol = M::Protocol::PROPRIETARY;
    CHECK(M::validateBinding(value));
    value.endpoint.address = 0;
    CHECK(M::validateBinding(value));
}

void checkFrameError(const M::Frame& frame, M::Err expected = M::Err::FRAME_ERROR) {
    const M::Status result = M::validateFrame(frame);
    CHECK(result.code == expected && result.detail == 0);
}

void frameContracts() {
    M::Frame frame;
    CHECK(M::validateFrame(frame));
    frame.id = 0x7FF;
    CHECK(M::validateFrame(frame));
    frame.id = 0x800;
    checkFrameError(frame);
    frame.extended = true;
    CHECK(M::validateFrame(frame));
    frame.id = 0x1FFFFFFF;
    CHECK(M::validateFrame(frame));
    frame.id = 0x20000000;
    checkFrameError(frame);
    frame.id = 0x123;
    CHECK(M::validateFrame(frame) && frame.extended); // Low numeric ID retains its wire format.

    frame.dlc = frame.payloadLength = 8;
    frame.data[7] = 0xA5;
    CHECK(M::validateFrame(frame));
    M::Frame owned = frame;
    frame.data[7] = 0;
    CHECK(owned.data[7] == 0xA5);
    frame.dlc = 9;
    checkFrameError(frame);
    frame.dlc = 8;
    frame.payloadLength = 9;
    checkFrameError(frame);
    frame.payloadLength = 7;
    checkFrameError(frame);
    frame.rtr = true;
    checkFrameError(frame);
    frame.payloadLength = 0;
    CHECK(M::validateFrame(frame)); // RTR requests eight bytes and carries none.
    frame.dlc = 0;
    CHECK(M::validateFrame(frame));
    frame.fd = true;
    checkFrameError(frame, M::Err::UNSUPPORTED);
    frame.dlc = 255;
    frame.id = UINT32_MAX;
    checkFrameError(frame, M::Err::UNSUPPORTED); // FD rejection has defined precedence.
    frame.fd = false;
    checkFrameError(frame);
}

} // namespace

int main() {
    statusContracts();
    bindingContracts();
    frameContracts();
    std::puts("portable core contracts passed");
    return EXIT_SUCCESS;
}
