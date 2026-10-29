/** @file network_codec_test.cpp
 * @brief Independent passive network vectors and unchanged-output failures.
 * Synthetic payloads follow pinned CANopenNode CO_NMT_Heartbeat.h:70-75 and
 * CO_Emergency.h:69-78,101-108 at 9b8beed8367241e96ac03f916cd5a500bcb2cf23.
 * Manufacturer bytes remain opaque; these are not device captures.
 * SPDX-License-Identifier: MIT
 */
#include <MotorControlCAN/canopen/Network.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace M = MotorControlCAN;
namespace C = MotorControlCAN::CANOPEN;

namespace {
void check(bool condition, const char* expression, int line) {
    if (!condition) {
        std::fprintf(stderr, "network_codec_test.cpp:%d: %s\n", line, expression);
        std::abort();
    }
}
#define CHECK(expression) check(static_cast<bool>(expression), #expression, __LINE__)

template <typename T> struct Saved {
    unsigned char bytes[sizeof(T)];
    explicit Saved(const T& value) { std::memcpy(bytes, &value, sizeof value); }
    bool unchanged(const T& value) const { return std::memcmp(bytes, &value, sizeof value) == 0; }
};

C::NetworkRoute route() {
    C::NetworkRoute value;
    value.binding.logicalAxisId = 91;
    value.binding.endpoint.bus = 8;
    value.binding.endpoint.profile = 17;
    value.binding.endpoint.address = 5;
    value.binding.endpoint.channel = 2;
    value.binding.endpoint.subaxis = 9;
    value.binding.bindingGeneration = 3;
    value.binding.configurationGeneration = 4;
    value.generation = 6;
    value.heartbeatId = 0x705;
    value.emcyId = 0x345; // Intentionally differs from the EDS default node-5 ID.
    return value;
}

M::Frame heartbeat(uint8_t state = 5) {
    M::Frame value;
    value.id = 0x705;
    value.dlc = value.payloadLength = 1;
    value.data[0] = state;
    return value;
}

M::Frame emergency() {
    M::Frame value;
    value.id = 0x345;
    value.dlc = value.payloadLength = 8;
    const uint8_t bytes[] = {0x30, 0x81, 0x11, 0xde, 0xad, 0xbe, 0xef, 0x80};
    std::memcpy(value.data, bytes, sizeof bytes);
    return value;
}

C::NetworkEvent sentinel() {
    C::NetworkEvent output;
    output.route = route();
    output.frame = emergency();
    output.kind = C::NetworkEventKind::EMCY;
    output.state = C::NmtState::PRE_OPERATIONAL;
    output.errorCode = 0x1234;
    output.errorRegister = 0x87;
    for (size_t i = 0; i < sizeof output.manufacturer; ++i) output.manufacturer[i] = uint8_t(0xa0 + i);
    return output;
}

void rejected(const M::Frame& frame, const C::NetworkRoute& expected,
              const M::Binding& sourceBinding, M::Generation generation, M::Err code) {
    C::NetworkEvent output = sentinel();
    const Saved<C::NetworkEvent> before(output);
    const M::Status result = C::checkNetworkEvent(frame, expected, sourceBinding, generation, output);
    CHECK(result.code == code);
    CHECK(result.detail == 0);
    CHECK(before.unchanged(output));
}

void routeValidation() {
    CHECK(!C::validateNetworkRoute(C::NetworkRoute()));
    const C::NetworkRoute original = route();
    CHECK(C::validateNetworkRoute(original));
    C::NetworkRoute value = original;
    value.heartbeatId = 1; value.emcyId = 0x7ff; value.binding.endpoint.address = 1;
    CHECK(C::validateNetworkRoute(value));
    value.binding.endpoint.address = 127;
    CHECK(C::validateNetworkRoute(value));
    for (unsigned i = 0; i < 14; ++i) {
        value = original;
        switch (i) {
        case 0: value.binding.logicalAxisId = 0; break;
        case 1: value.binding.endpoint.profile = 0; break;
        case 2: value.binding.bindingGeneration = 0; break;
        case 3: value.binding.configurationGeneration = 0; break;
        case 4: value.binding.endpoint.protocol = M::Protocol::PROPRIETARY; break;
        case 5: value.binding.endpoint.protocol = static_cast<M::Protocol>(99); break;
        case 6: value.binding.endpoint.address = 0; break;
        case 7: value.binding.endpoint.address = 128; break;
        case 8: value.generation = 0; break;
        case 9: value.heartbeatId = 0; break;
        case 10: value.heartbeatId = 0x800; break;
        case 11: value.emcyId = 0; break;
        case 12: value.emcyId = 0x800; break;
        case 13: value.emcyId = value.heartbeatId; break;
        }
        CHECK(C::validateNetworkRoute(value).code == M::Err::INVALID_CONFIG);
        rejected(heartbeat(), value, value.binding, value.generation, M::Err::INVALID_CONFIG);
    }
}

void heartbeatVectors() {
    const C::NetworkRoute expected = route();
    const uint8_t values[] = {0x00, 0x04, 0x05, 0x7f};
    const C::NmtState states[] = {C::NmtState::INITIALIZING, C::NmtState::STOPPED,
                                 C::NmtState::OPERATIONAL, C::NmtState::PRE_OPERATIONAL};
    for (size_t i = 0; i < sizeof values; ++i) {
        C::NetworkEvent output = sentinel();
        M::Frame frame = heartbeat(values[i]);
        frame.data[7] = 0xa5; // Unused storage is copied without becoming payload.
        CHECK(C::checkNetworkEvent(frame, expected, expected.binding, 6, output));
        CHECK(output.kind == (i ? C::NetworkEventKind::HEARTBEAT : C::NetworkEventKind::BOOTUP));
        CHECK(output.state == states[i]);
        CHECK(M::sameBinding(output.route.binding, expected.binding));
        CHECK(output.route.generation == 6);
        CHECK(output.route.heartbeatId == 0x705 && output.route.emcyId == 0x345);
        CHECK(output.frame.data[0] == values[i] && output.frame.data[7] == 0xa5);
        CHECK(output.errorCode == 0 && output.errorRegister == 0);
        for (size_t j = 0; j < sizeof output.manufacturer; ++j) CHECK(output.manufacturer[j] == 0);
        frame.data[0] = 0x44;
        CHECK(output.frame.data[0] == values[i]);
    }
    const uint8_t unsupported[] = {0x01, 0x02, 0x03, 0x06, 0x80, 0x84, 0x85, 0xff};
    for (size_t i = 0; i < sizeof unsupported; ++i)
        rejected(heartbeat(unsupported[i]), expected, expected.binding, 6, M::Err::UNSUPPORTED);
}

void emergencyVectors() {
    const C::NetworkRoute expected = route();
    C::NetworkEvent output = sentinel();
    M::Frame frame = emergency();
    CHECK(C::checkNetworkEvent(frame, expected, expected.binding, 6, output));
    CHECK(output.kind == C::NetworkEventKind::EMCY);
    CHECK(output.errorCode == 0x8130 && output.errorRegister == 0x11);
    const uint8_t manufacturer[] = {0xde, 0xad, 0xbe, 0xef, 0x80};
    CHECK(std::memcmp(output.manufacturer, manufacturer, sizeof manufacturer) == 0);
    CHECK(std::memcmp(output.frame.data, frame.data, 8) == 0);
    frame.data[0] = 0xef; frame.data[1] = 0xbe; frame.data[2] = 0xff;
    CHECK(C::checkNetworkEvent(frame, expected, expected.binding, 6, output));
    CHECK(output.errorCode == 0xbeef && output.errorRegister == 0xff);
    CHECK(std::memcmp(output.manufacturer, manufacturer, sizeof manufacturer) == 0);
    frame.data[0] = frame.data[1] = frame.data[2] = 0;
    CHECK(C::checkNetworkEvent(frame, expected, expected.binding, 6, output));
    CHECK(output.errorCode == 0 && output.errorRegister == 0);
    CHECK(output.kind == C::NetworkEventKind::EMCY);
    frame.data[3] = 0;
    CHECK(output.manufacturer[0] == 0xde && output.frame.data[3] == 0xde);
    frame.id = 0x85;
    rejected(frame, expected, expected.binding, 6, M::Err::FRAME_ERROR);
}

void provenanceAndEnvelope() {
    const C::NetworkRoute expected = route();
    for (unsigned i = 0; i < 10; ++i) {
        M::Binding binding = expected.binding;
        switch (i) {
        case 0: ++binding.logicalAxisId; break;
        case 1: binding.endpoint.protocol = M::Protocol::PROPRIETARY; break;
        case 2: ++binding.endpoint.bus; break;
        case 3: ++binding.endpoint.profile; break;
        case 4: ++binding.endpoint.address; break;
        case 5: ++binding.endpoint.channel; break;
        case 6: ++binding.endpoint.subaxis; break;
        case 7: ++binding.bindingGeneration; break;
        case 8: ++binding.configurationGeneration; break;
        case 9: binding.bindingGeneration = 0; break;
        }
        rejected(heartbeat(), expected, binding, 6, M::Err::INVALID_CONFIG);
    }
    rejected(heartbeat(), expected, expected.binding, 0, M::Err::INVALID_CONFIG);
    rejected(heartbeat(), expected, expected.binding, 5, M::Err::INVALID_CONFIG);
    rejected(heartbeat(), expected, expected.binding, 7, M::Err::INVALID_CONFIG);
    for (unsigned i = 0; i < 11; ++i) {
        M::Frame frame = heartbeat();
        M::Err error = M::Err::FRAME_ERROR;
        switch (i) {
        case 0: frame.dlc = frame.payloadLength = 0; break;
        case 1: frame.dlc = frame.payloadLength = 2; break;
        case 2: frame.id = 0x706; break;
        case 3: frame.extended = true; error = M::Err::UNSUPPORTED; break;
        case 4: frame.rtr = true; frame.payloadLength = 0; error = M::Err::UNSUPPORTED; break;
        case 5: frame.fd = true; error = M::Err::UNSUPPORTED; break;
        case 6: frame.brs = true; break;
        case 7: frame.esi = true; break;
        case 8: frame.dlc = 2; break;
        case 9: frame.id = 0x800; break;
        case 10: frame.rtr = true; break;
        }
        rejected(frame, expected, expected.binding, 6, error);
    }
    M::Frame frame = emergency();
    frame.dlc = frame.payloadLength = 7;
    rejected(frame, expected, expected.binding, 6, M::Err::FRAME_ERROR);
}

void aliasRejection() {
    const C::NetworkRoute expected = route();
    const M::Frame frame = heartbeat();
    C::NetworkEvent output = sentinel();
    Saved<C::NetworkEvent> before(output);
    CHECK(C::checkNetworkEvent(output.frame, expected, expected.binding, 6, output).code == M::Err::ILLEGAL_VALUE);
    CHECK(before.unchanged(output));
    CHECK(C::checkNetworkEvent(frame, output.route, expected.binding, 6, output).code == M::Err::ILLEGAL_VALUE);
    CHECK(before.unchanged(output));
    CHECK(C::checkNetworkEvent(frame, expected, output.route.binding, 6, output).code == M::Err::ILLEGAL_VALUE);
    CHECK(before.unchanged(output));
}
} // namespace

int main() {
    routeValidation();
    heartbeatVectors();
    emergencyVectors();
    provenanceAndEnvelope();
    aliasRejection();
    std::puts("PASS: passive network codec vectors, provenance and atomic failure");
    return 0;
}
