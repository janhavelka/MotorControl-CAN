// SPDX-License-Identifier: MIT
#include <MotorControlCAN/can/Frame.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace M = MotorControlCAN;
namespace {
void check(bool valid, int line) {
    if (!valid) { std::fprintf(stderr, "frame_test.cpp:%d\n", line); std::abort(); }
}
#define CHECK(value) check(static_cast<bool>(value), __LINE__)

void shapes() {
    const uint32_t ids[] = {0, 0x117, 0x7ff, 0x800, 0x1fffffff, 0x20000000, UINT32_MAX};
    for (unsigned format = 0; format < 2; ++format) {
        for (unsigned remote = 0; remote < 2; ++remote) {
            for (uint32_t id : ids) {
                for (uint8_t dlc = 0; dlc <= 9; ++dlc) {
                    for (uint8_t length = 0; length <= 9; ++length) {
                        M::Frame f;
                        f.id = id; f.extended = format != 0; f.rtr = remote != 0;
                        f.dlc = dlc; f.payloadLength = length;
                        std::memset(f.data, 0xa5, sizeof f.data);
                        const bool expected = id <= (format ? 0x1fffffffu : 0x7ffu) &&
                            dlc <= 8 && length <= 8 && (remote ? length == 0 : length == dlc);
                        unsigned char original[sizeof f]; std::memcpy(original, &f, sizeof f);
                        const M::Status status = M::validateFrame(f);
                        CHECK(status.isOk() == expected);
                        CHECK(expected || status.code == M::Err::FRAME_ERROR);
                        CHECK(std::memcmp(original, &f, sizeof f) == 0);
                    }
                }
            }
        }
    }
    for (unsigned flags = 0; flags < 8; ++flags) {
        M::Frame f; f.fd = (flags & 1u) != 0; f.brs = (flags & 2u) != 0; f.esi = (flags & 4u) != 0;
        const M::Status s = M::validateFrame(f);
        CHECK(f.fd ? s.code == M::Err::UNSUPPORTED :
            (f.brs || f.esi ? s.code == M::Err::FRAME_ERROR : s.isOk()));
        f.fd = true; f.rtr = true; f.id = UINT32_MAX; f.dlc = f.payloadLength = 255;
        CHECK(M::validateFrame(f).code == M::Err::UNSUPPORTED); // Capability failure takes precedence.
    }
}
void protocolContrasts() {
    // Original pinned VESC comm_can.md: Frame Format / Single-Frame Commands.
    // Shape only: no VESC support, applied current, acknowledgement or completion.
    M::Frame vesc; vesc.extended = true; vesc.id = 0x117;
    vesc.dlc = vesc.payloadLength = 4;
    const uint8_t data[] = {0, 0, 0xc7, 0x38};
    std::memcpy(vesc.data, data, sizeof data);
    CHECK(M::validateFrame(vesc));
    vesc.extended = false;
    CHECK(M::validateFrame(vesc)); // Common shape is valid; a VESC route checks its own format.

    // ODrive CANSimple 0.6.12 Get_Encoder_Estimates, command09, illustrative node5.
    M::Frame odrive; odrive.id = (5u << 5) | 0x09u;
    odrive.rtr = true; odrive.dlc = 8; odrive.payloadLength = 0;
    std::memset(odrive.data, 0xff, sizeof odrive.data); // Inactive storage cannot become RTR payload.
    CHECK(M::validateFrame(odrive));
    odrive.payloadLength = 1;
    CHECK(M::validateFrame(odrive).code == M::Err::FRAME_ERROR);

    // Pinned moteus can.md extended FD example requires a future FD adapter.
    M::Frame moteus; moteus.id = 0x8001; moteus.extended = true; moteus.fd = true;
    moteus.dlc = 10; moteus.payloadLength = 16;
    CHECK(M::validateFrame(moteus).code == M::Err::UNSUPPORTED); // Never truncate to eight.
}
} // namespace
int main() { shapes(); protocolContrasts(); return 0; }
