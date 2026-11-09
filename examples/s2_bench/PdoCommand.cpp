// SPDX-License-Identifier: MIT
#include "PdoCommand.h"

namespace MotorControlCANExample {
namespace {
// One unsigned decimal field: at least one digit, no sign, no leading spaces, no overflow.
bool number(const char*& text, const char* end, uint32_t& output) noexcept {
    if (text == end || *text < '0' || *text > '9')
        return false;
    uint32_t value = 0;
    while (text != end && *text >= '0' && *text <= '9') {
        const uint32_t digit = uint32_t(*text - '0');
        if (value > (UINT32_MAX - digit) / 10)
            return false;
        value = value * 10 + digit;
        ++text;
    }
    output = value;
    return true;
}
bool comma(const char*& text, const char* end) noexcept {
    if (text == end || *text != ',')
        return false;
    ++text;
    return true;
}
bool node(const char*& text, const char* end, uint8_t& output) noexcept {
    uint32_t value = 0;
    if (!number(text, end, value) || value < 1 || value > 2)
        return false;
    output = uint8_t(value);
    return true;
}
bool set(const char*& text, const char* end, uint8_t& output) noexcept {
    uint32_t value = 0;
    if (!number(text, end, value) || value > 3)
        return false;
    output = uint8_t(value);
    return true;
}
} // namespace

bool parsePdoCommand(const char* text, size_t length, PdoCommand& output) noexcept {
    if (!text || !length)
        return false;
    const char* end = text + length;
    const char letter = *text++;
    PdoCommand result;
    uint32_t value = 0;
    switch (letter) {
    case 'N':
        result.kind = PdoCommandKind::NMT;
        if (!node(text, end, result.node) || !comma(text, end) || text == end)
            return false;
        // Reset commands are deliberately absent: they invalidate every node fact and are not
        // part of these experiments.
        result.nmt = *text == 'o' ? 0x01 : *text == 's' ? 0x02 : *text == 'p' ? 0x80 : 0;
        if (!result.nmt)
            return false;
        ++text;
        break;
    case 'P':
        result.kind = PdoCommandKind::WATCH;
        if (!number(text, end, result.seconds) || result.seconds > 600)
            return false;
        break;
    case 'R':
        result.kind = PdoCommandKind::READ;
        if (!node(text, end, result.node) || !comma(text, end) || text == end ||
            (*text != 't' && *text != 'r'))
            return false;
        result.transmit = *text++ == 't';
        if (!comma(text, end) || !set(text, end, result.set))
            return false;
        break;
    case 'W':
        result.kind = PdoCommandKind::WRITE;
        if (!node(text, end, result.node) || !comma(text, end) || !set(text, end, result.set) ||
            !comma(text, end) || !number(text, end, value) || value > UINT8_MAX ||
            !comma(text, end) || !number(text, end, result.baseline) || !comma(text, end) ||
            !number(text, end, result.value))
            return false;
        result.subindex = uint8_t(value);
        result.transmit = true;
        break;
    case 'E':
        // Only the two measured literals; 200, 10 and every other value refuse here as syntax.
        result.kind = PdoCommandKind::CONFIGURE;
        if (!node(text, end, result.node) || !comma(text, end) || !number(text, end, value) ||
            (value != 0 && value != 100))
            return false;
        result.eventTimer = uint16_t(value);
        break;
    case 'C':
        // C0 alone stops; any other line states identity, duration and frame capacity together.
        result.kind = PdoCommandKind::CAPTURE;
        if (!number(text, end, result.capture))
            return false;
        if (result.capture == 0)
            break;
        if (!comma(text, end) || !number(text, end, result.seconds) || result.seconds < 1 ||
            result.seconds > 600 || !comma(text, end) || !number(text, end, value) || value < 1 ||
            value > UINT16_MAX)
            return false;
        result.frames = uint16_t(value);
        break;
    case 'D':
        // The mode letter, then the window, the host selection and the identity expectation.
        result.kind = PdoCommandKind::DISCOVER;
        if (text == end)
            return false;
        result.discovery = *text == 'o' ? 0 : *text == 'p' ? 1 : *text == 'i' ? 2 : 3;
        if (result.discovery > 2)
            return false;
        ++text;
        if (!comma(text, end) || !number(text, end, result.seconds) || result.seconds < 1 ||
            result.seconds > 60 || !comma(text, end) || !number(text, end, value) || value > 2)
            return false;
        result.select = uint8_t(value);
        if (!comma(text, end) || !number(text, end, value) || value > 1 ||
            (value && result.discovery != 2))
            return false;
        result.expect = value != 0;
        break;
    case 'K':
        result.kind = PdoCommandKind::COMMUNICATION;
        if (!node(text, end, result.node))
            return false;
        break;
    case 'H':
        // Only the reviewed literal: 0 (a reverse transition), 500 and every other value refuse here.
        result.kind = PdoCommandKind::HEARTBEAT;
        if (!node(text, end, result.node) || !comma(text, end) || !number(text, end, value) ||
            value != 1000)
            return false;
        result.producerTime = uint16_t(value);
        break;
    default:
        return false;
    }
    if (text != end)
        return false;
    output = result;
    return true;
}
} // namespace MotorControlCANExample
