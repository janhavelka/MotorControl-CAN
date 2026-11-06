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
    default:
        return false;
    }
    if (text != end)
        return false;
    output = result;
    return true;
}
} // namespace MotorControlCANExample
