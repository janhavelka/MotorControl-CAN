/** @file nmt_codec_test.cpp
 * @brief Independent NMT request bytes, frame format and atomic failures.
 * Literal vectors follow CANopenNode 9b8beed8367241e96ac03f916cd5a500bcb2cf23,
 * 301/CO_NMT_Heartbeat.h:51-56,81-87 and CO_NMT_Heartbeat.c:285-288.
 * These are protocol vectors, not device captures or motion guarantees.
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
        std::fprintf(stderr, "nmt_codec_test.cpp:%d: %s\n", line, expression);
        std::abort();
    }
}
#define CHECK(expression) check(static_cast<bool>(expression), #expression, __LINE__)

M::Frame sentinel() {
    M::Frame frame;
    frame.id = 0x1abcdef;
    frame.dlc = 8;
    frame.payloadLength = 7;
    frame.extended = frame.rtr = frame.fd = frame.brs = frame.esi = true;
    const uint8_t data[] = {0x12, 0x34, 0x56, 0x78, 0x9a, 0xbc, 0xde, 0xf0};
    std::memcpy(frame.data, data, sizeof data);
    return frame;
}

void literalVectors() {
    const C::NmtCommand commands[] = {
        C::NmtCommand::START, C::NmtCommand::STOP, C::NmtCommand::PRE_OPERATIONAL,
        C::NmtCommand::RESET_NODE, C::NmtCommand::RESET_COMMUNICATION};
    const uint8_t expectedCommands[] = {0x01, 0x02, 0x80, 0x81, 0x82};
    const uint8_t targets[] = {0, 1, 127};
    for (size_t command = 0; command < sizeof expectedCommands; ++command) {
        for (size_t target = 0; target < sizeof targets; ++target) {
            M::Frame output = sentinel();
            const M::Status result = C::makeNmtRequest(commands[command], targets[target], output);
            CHECK(result.code == M::Err::OK && result.detail == 0);
            CHECK(output.id == 0 && output.dlc == 2 && output.payloadLength == 2);
            CHECK(!output.extended && !output.rtr && !output.fd && !output.brs && !output.esi);
            CHECK(output.data[0] == expectedCommands[command]);
            CHECK(output.data[1] == targets[target]);
            for (size_t byte = 2; byte < sizeof output.data; ++byte) CHECK(output.data[byte] == 0);
            CHECK(M::validateFrame(output));
        }
    }
}

void rejected(C::NmtCommand command, uint8_t target, M::Err expected) {
    M::Frame output = sentinel();
    unsigned char before[sizeof output];
    std::memcpy(before, &output, sizeof output);
    const M::Status result = C::makeNmtRequest(command, target, output);
    CHECK(result.code == expected && result.detail == 0);
    CHECK(std::memcmp(before, &output, sizeof output) == 0);
}

void atomicFailures() {
    const uint8_t invalidCommands[] = {0, 3, 4, 5, 127, 131, 255};
    for (size_t i = 0; i < sizeof invalidCommands; ++i) {
        rejected(static_cast<C::NmtCommand>(invalidCommands[i]), 0, M::Err::UNSUPPORTED);
        rejected(static_cast<C::NmtCommand>(invalidCommands[i]), 127, M::Err::UNSUPPORTED);
    }
    rejected(C::NmtCommand::START, 128, M::Err::INVALID_CONFIG);
    rejected(C::NmtCommand::RESET_COMMUNICATION, 255, M::Err::INVALID_CONFIG);
    rejected(static_cast<C::NmtCommand>(255), 255, M::Err::UNSUPPORTED);
}
} // namespace

int main() {
    literalVectors();
    atomicFailures();
    return 0;
}
