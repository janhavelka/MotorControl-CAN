/** @file sdo_codec_test.cpp
 * @brief Independent initiation envelopes, scalar boundaries and atomic failures.
 * Wire fixtures come from pinned CANopenNode CO_SDOserver.h:109-159 and
 * CO_SDOclient.c:804-828,1218-1256,1542-1545, cross-checked against Python
 * struct/int.from_bytes. These synthetic bytes authorize no device operation.
 * Strict command bits and opaque unused receive bytes follow Parker PIO-337
 * Manual 88-022351-01A, "Initiate SDO Upload" and "SDO Examples", pp108-111.
 * No encoder helper generates expectations.
 * SPDX-License-Identifier: MIT
 */
#include <MotorControlCAN/canopen/Sdo.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>

namespace M = MotorControlCAN;
namespace C = MotorControlCAN::CANOPEN;

namespace {
void check(bool condition, const char* expression, int line) {
    if (!condition) {
        std::fprintf(stderr, "sdo_codec_test.cpp:%d: %s\n", line, expression);
        std::abort();
    }
}
#define CHECK(expression) check(static_cast<bool>(expression), #expression, __LINE__)

template <typename T> struct Saved {
    unsigned char bytes[sizeof(T)];
    explicit Saved(const T& value) { std::memcpy(bytes, &value, sizeof value); }
    bool unchanged(const T& value) const { return std::memcmp(bytes, &value, sizeof value) == 0; }
};

C::SdoExpectation expected(uint8_t width = 2,
                           C::SdoDirection direction = C::SdoDirection::UPLOAD) {
    C::SdoExpectation value;
    value.channel.binding.logicalAxisId = 91;
    value.channel.binding.endpoint.bus = 8;
    value.channel.binding.endpoint.profile = 17;
    value.channel.binding.endpoint.address = 5;
    value.channel.binding.endpoint.channel = 2;
    value.channel.binding.endpoint.subaxis = 9;
    value.channel.binding.bindingGeneration = 3;
    value.channel.binding.configurationGeneration = 4;
    value.channel.clientToServerId = 0x605;
    value.channel.serverToClientId = 0x585;
    value.index = 0x6041;
    value.width = width;
    value.direction = direction;
    return value;
}

M::Frame envelope(uint8_t command, const C::SdoExpectation& expectation, bool request = false) {
    M::Frame frame;
    frame.id = request ? expectation.channel.clientToServerId : expectation.channel.serverToClientId;
    frame.dlc = frame.payloadLength = 8;
    frame.data[0] = command;
    frame.data[1] = static_cast<uint8_t>(expectation.index);
    frame.data[2] = static_cast<uint8_t>(expectation.index >> 8);
    frame.data[3] = expectation.subindex;
    return frame;
}

C::SdoReply sentinel() {
    C::SdoReply value;
    value.expected = expected(3, C::SdoDirection::DOWNLOAD);
    value.frame = envelope(0x80, value.expected);
    value.kind = C::SdoReplyKind::ABORT;
    value.length = 3;
    value.data[0] = 0x11;
    value.data[1] = 0x22;
    value.data[2] = 0x33;
    value.data[3] = 0x44;
    value.sizeIndicated = true;
    value.indicatedSize = 0x12345678;
    value.abortCode = 0x89ABCDEF;
    return value;
}

void statusIs(M::Status status, M::Err code, C::SdoError detail) {
    CHECK(status.code == code);
    CHECK(status.detail == static_cast<int32_t>(detail));
}

void rejectReply(const M::Frame& frame, const C::SdoExpectation& expectation,
                 M::Err code, C::SdoError detail) {
    C::SdoReply output = sentinel();
    const Saved<C::SdoReply> saved(output);
    statusIs(C::checkReply(frame, expectation, output), code, detail);
    CHECK(saved.unchanged(output));
}

void channelAndExpectation() {
    const C::SdoExpectation original = expected();
    CHECK(C::validateSdoChannel(original.channel));
    C::SdoChannel channel = original.channel;
    channel.binding.endpoint.address = 1;
    channel.clientToServerId = 1;
    channel.serverToClientId = 0x7FF;
    CHECK(C::validateSdoChannel(channel));
    channel.binding.endpoint.address = 127;
    CHECK(C::validateSdoChannel(channel));

    for (unsigned variant = 0; variant < 15; ++variant) {
        channel = original.channel;
        switch (variant) {
        case 0: channel.binding.logicalAxisId = 0; break;
        case 1: channel.binding.endpoint.profile = 0; break;
        case 2: channel.binding.bindingGeneration = 0; break;
        case 3: channel.binding.configurationGeneration = 0; break;
        case 4: channel.binding.endpoint.protocol = M::Protocol::PROPRIETARY; break;
        case 5: channel.binding.endpoint.protocol = static_cast<M::Protocol>(255); break;
        case 6: channel.binding.endpoint.address = 0; break;
        case 7: channel.binding.endpoint.address = 128; break;
        case 8: channel.binding.endpoint.address = UINT32_MAX; break;
        case 9: channel.clientToServerId = 0; break;
        case 10: channel.serverToClientId = 0; break;
        case 11: channel.clientToServerId = 0x800; break;
        case 12: channel.serverToClientId = 0x800; break;
        case 13: channel.clientToServerId = channel.serverToClientId; break;
        case 14: channel.serverToClientId = 0x80000585; break;
        }
        statusIs(C::validateSdoChannel(channel), M::Err::INVALID_CONFIG,
                 C::SdoError::INVALID_CHANNEL);
        C::SdoExpectation invalid = original;
        invalid.channel = channel;
        M::Frame frame = envelope(0x4B, original);
        rejectReply(frame, invalid, M::Err::INVALID_CONFIG, C::SdoError::INVALID_CHANNEL);
        statusIs(C::checkRequest(envelope(0x40, original, true), invalid), M::Err::INVALID_CONFIG,
                 C::SdoError::INVALID_CHANNEL);
    }

    for (unsigned variant = 0; variant < 4; ++variant) {
        C::SdoExpectation invalid = original;
        if (variant == 0) invalid.width = 0;
        if (variant == 1) invalid.width = 5;
        if (variant == 2) invalid.width = 255;
        if (variant == 3) invalid.direction = static_cast<C::SdoDirection>(255);
        M::Frame frame = envelope(0x4B, original);
        frame.fd = true;
        rejectReply(frame, invalid, M::Err::INVALID_CONFIG, C::SdoError::INVALID_EXPECTATION);
        statusIs(C::checkRequest(frame, invalid), M::Err::INVALID_CONFIG,
                 C::SdoError::INVALID_EXPECTATION);
    }
}

void shapeAndCorrelation() {
    const C::SdoExpectation expectation = expected();
    M::Frame frame = envelope(0x4B, expectation);
    frame.id = expectation.channel.clientToServerId;
    rejectReply(frame, expectation, M::Err::FRAME_ERROR, C::SdoError::COB_ID);
    frame = envelope(0x4B, expectation);
    ++frame.id;
    rejectReply(frame, expectation, M::Err::FRAME_ERROR, C::SdoError::COB_ID);
    frame = envelope(0x4B, expectation);
    frame.data[1] ^= 1;
    rejectReply(frame, expectation, M::Err::FRAME_ERROR, C::SdoError::OBJECT);
    frame = envelope(0x4B, expectation);
    frame.data[2] ^= 1;
    rejectReply(frame, expectation, M::Err::FRAME_ERROR, C::SdoError::OBJECT);
    frame = envelope(0x4B, expectation);
    ++frame.data[3];
    rejectReply(frame, expectation, M::Err::FRAME_ERROR, C::SdoError::OBJECT);
    frame = envelope(0x4B, expectation);
    frame.extended = true;
    rejectReply(frame, expectation, M::Err::UNSUPPORTED, C::SdoError::FRAME_KIND);
    frame = envelope(0x4B, expectation);
    frame.rtr = true;
    frame.payloadLength = 0;
    rejectReply(frame, expectation, M::Err::UNSUPPORTED, C::SdoError::FRAME_KIND);
    frame = envelope(0x4B, expectation);
    frame.dlc = frame.payloadLength = 7;
    rejectReply(frame, expectation, M::Err::FRAME_ERROR, C::SdoError::LENGTH);

    // The same source-backed contrasts are valid common frames and rejected
    // specifically by CANopen, without inventing another production protocol.
    // VESC comm_can.md example: extended 0x117, four-byte SET_CURRENT format.
    M::Frame vesc; vesc.extended = true; vesc.id = 0x117;
    vesc.dlc = vesc.payloadLength = 4;
    const uint8_t vescBytes[] = {0, 0, 0xC7, 0x38};
    std::memcpy(vesc.data, vescBytes, sizeof vescBytes);
    CHECK(M::validateFrame(vesc));
    rejectReply(vesc, expectation, M::Err::UNSUPPORTED, C::SdoError::FRAME_KIND);
    // ODrive CANSimple 0.6.12 Get_Encoder_Estimates, illustrative node 5.
    M::Frame odrive; odrive.id = 0xA9; odrive.rtr = true; odrive.dlc = 8;
    CHECK(odrive.payloadLength == 0 && M::validateFrame(odrive));
    rejectReply(odrive, expectation, M::Err::UNSUPPORTED, C::SdoError::FRAME_KIND);

    C::SdoExpectation custom = expectation;
    custom.channel.clientToServerId = 0x21;
    custom.channel.serverToClientId = 0x32;
    C::SdoReply output;
    CHECK(C::checkReply(envelope(0x4B, custom), custom, output));
    CHECK(output.expected.channel.binding.endpoint.channel == 2);
    CHECK(output.expected.channel.binding.endpoint.subaxis == 9);
    CHECK(M::sameBinding(output.expected.channel.binding, custom.channel.binding));
    rejectReply(envelope(0x4B, expectation), custom, M::Err::FRAME_ERROR, C::SdoError::COB_ID);
}

void replyCommands() {
    const C::SdoExpectation upload = expected();
    const C::SdoExpectation download = expected(2, C::SdoDirection::DOWNLOAD);
    // Exhaust every command byte: reserved bits and the wrong phase cannot hide
    // behind command-class masks. Width mismatch is valid-wire metadata conflict.
    for (unsigned command = 0; command <= 255; ++command) {
        M::Frame frame = envelope(static_cast<uint8_t>(command), upload);
        frame.data[4] = 2;
        const bool uploadAccepted = command == 0x40 || command == 0x41 || command == 0x42 ||
                                    command == 0x4B || command == 0x80;
        C::SdoReply output;
        if (uploadAccepted) {
            CHECK(C::checkReply(frame, upload, output));
        } else if (command == 0x43 || command == 0x47 || command == 0x4F) {
            rejectReply(frame, upload, M::Err::ILLEGAL_VALUE, C::SdoError::WIDTH);
        } else {
            rejectReply(frame, upload, M::Err::FRAME_ERROR, C::SdoError::COMMAND);
        }
        if (command == 0x60 || command == 0x80) {
            CHECK(C::checkReply(frame, download, output));
        } else {
            rejectReply(frame, download, M::Err::FRAME_ERROR, C::SdoError::COMMAND);
        }
    }

    const uint8_t commands[] = {0x4F, 0x4B, 0x47, 0x43};
    for (uint8_t width = 1; width <= 4; ++width) {
        C::SdoExpectation expectation = expected(width);
        M::Frame frame = envelope(commands[width - 1], expectation);
        frame.data[4] = 0x12;
        frame.data[5] = 0x34;
        frame.data[6] = 0xA5;
        frame.data[7] = 0x5A;
        C::SdoReply output = sentinel();
        CHECK(C::checkReply(frame, expectation, output));
        CHECK(output.kind == C::SdoReplyKind::EXPEDITED_UPLOAD);
        CHECK(output.length == width && output.sizeIndicated && output.indicatedSize == width);
        CHECK(output.abortCode == 0);
        CHECK(std::memcmp(output.data, &frame.data[4], width) == 0);
        for (uint8_t i = width; i < 4; ++i) CHECK(output.data[i] == 0);
        CHECK(std::memcmp(output.frame.data, frame.data, 8) == 0);
        frame.data[0] = 0x42;
        CHECK(C::checkReply(frame, expectation, output));
        CHECK(output.length == width && !output.sizeIndicated && output.indicatedSize == 0);
        CHECK(std::memcmp(output.data, &frame.data[4], width) == 0);
        for (uint8_t i = width; i < 4; ++i) CHECK(output.data[i] == 0);
        frame.data[4] ^= 0xFF;
        CHECK(output.data[0] == 0x12); // Result owns bytes independently of borrowed frame.
    }

    M::Frame statusword = envelope(0x4B, upload);
    const uint8_t literal[] = {0x4B, 0x41, 0x60, 0, 0x34, 0x12, 0xA5, 0x5A};
    std::memcpy(statusword.data, literal, sizeof literal);
    C::SdoReply output;
    CHECK(C::checkReply(statusword, upload, output));
    uint32_t word = 0;
    CHECK(C::readWord(output.data, output.length, 2, word));
    CHECK(word == 0x1234);
}

void segmentedAndAbort() {
    const C::SdoExpectation expectation = expected();
    M::Frame frame = envelope(0x40, expectation);
    const uint8_t opaque[] = {0xFF, 0xA5, 0x5A, 0x80};
    std::memcpy(&frame.data[4], opaque, sizeof opaque);
    C::SdoReply output = sentinel();
    CHECK(C::checkReply(frame, expectation, output));
    CHECK(output.kind == C::SdoReplyKind::SEGMENTED_UPLOAD && output.length == 0);
    CHECK(!output.sizeIndicated && output.indicatedSize == 0 && output.abortCode == 0);
    for (unsigned i = 0; i < 4; ++i) CHECK(output.data[i] == 0);
    CHECK(std::memcmp(&output.frame.data[4], opaque, sizeof opaque) == 0);

    frame = envelope(0x41, expectation);
    frame.data[4] = 2;
    CHECK(C::checkReply(frame, expectation, output));
    CHECK(output.kind == C::SdoReplyKind::SEGMENTED_UPLOAD && output.length == 0);
    CHECK(output.sizeIndicated && output.indicatedSize == 2);
    for (unsigned i = 0; i < 4; ++i) CHECK(output.data[i] == 0);
    const uint32_t mismatches[] = {0, 1, 3, UINT32_MAX};
    for (unsigned i = 0; i < sizeof mismatches / sizeof mismatches[0]; ++i) {
        for (unsigned byte = 0; byte < 4; ++byte)
            frame.data[4 + byte] = static_cast<uint8_t>(mismatches[i] >> (8 * byte));
        rejectReply(frame, expectation, M::Err::ILLEGAL_VALUE, C::SdoError::WIDTH);
    }

    for (unsigned direction = 0; direction < 2; ++direction) {
        C::SdoExpectation wanted = expectation;
        wanted.direction = direction == 0 ? C::SdoDirection::UPLOAD : C::SdoDirection::DOWNLOAD;
        frame = envelope(0x80, wanted);
        const uint8_t rawAbort[] = {0x67, 0x45, 0x23, 0xF1};
        std::memcpy(&frame.data[4], rawAbort, sizeof rawAbort);
        CHECK(C::checkReply(frame, wanted, output));
        CHECK(output.kind == C::SdoReplyKind::ABORT && output.abortCode == 0xF1234567);
        CHECK(output.length == 0 && !output.sizeIndicated && output.indicatedSize == 0);
        for (unsigned i = 0; i < 4; ++i) CHECK(output.data[i] == 0);
        std::memset(&frame.data[4], 0, 4);
        CHECK(C::checkReply(frame, wanted, output));
        CHECK(output.kind == C::SdoReplyKind::ABORT && output.abortCode == 0);
        std::memset(&frame.data[4], 0xFF, 4);
        CHECK(C::checkReply(frame, wanted, output));
        CHECK(output.abortCode == UINT32_MAX);
        ++frame.data[3];
        rejectReply(frame, wanted, M::Err::FRAME_ERROR, C::SdoError::OBJECT);
    }

    C::SdoExpectation download = expected(2, C::SdoDirection::DOWNLOAD);
    frame = envelope(0x60, download);
    std::memcpy(&frame.data[4], opaque, sizeof opaque);
    CHECK(C::checkReply(frame, download, output));
    CHECK(output.kind == C::SdoReplyKind::DOWNLOAD_ACK && output.length == 0);
    CHECK(!output.sizeIndicated && output.indicatedSize == 0 && output.abortCode == 0);
    CHECK(std::memcmp(&output.frame.data[4], opaque, sizeof opaque) == 0);
}

void requests() {
    const uint8_t commands[] = {0x2F, 0x2B, 0x27, 0x23};
    for (uint8_t width = 1; width <= 4; ++width) {
        C::SdoExpectation expectation = expected(width);
        M::Frame frame = envelope(0x40, expectation, true);
        const uint8_t literal[] = {0x40, 0x41, 0x60, 0, 0, 0, 0, 0};
        CHECK(std::memcmp(frame.data, literal, sizeof literal) == 0);
        CHECK(C::checkRequest(frame, expectation));
        frame.data[0] = 0x4B; // Public example incorrectly copied a server reply command.
        CHECK(!C::checkRequest(frame, expectation));
        frame.data[0] = 0x40;
        frame.data[7] = 1;
        statusIs(C::checkRequest(frame, expectation), M::Err::FRAME_ERROR, C::SdoError::RESERVED);
        expectation.direction = C::SdoDirection::DOWNLOAD;
        frame = envelope(commands[width - 1], expectation, true);
        for (uint8_t i = 0; i < width; ++i) frame.data[4 + i] = 0x80;
        CHECK(C::checkRequest(frame, expectation));
        if (width != 4) {
            frame.data[4 + width] = 1;
            statusIs(C::checkRequest(frame, expectation), M::Err::FRAME_ERROR, C::SdoError::RESERVED);
        }
    }

    C::SdoExpectation ramp = expected(4, C::SdoDirection::DOWNLOAD);
    ramp.index = 0x609A; // Raw EDS609A is U32 even when the value fits one byte.
    M::Frame frame = envelope(0x23, ramp, true);
    const uint8_t ramp100[] = {0x23, 0x9A, 0x60, 0, 0x64, 0, 0, 0};
    std::memcpy(frame.data, ramp100, sizeof ramp100);
    CHECK(C::checkRequest(frame, ramp));
    frame.data[0] = 0x2B;
    CHECK(!C::checkRequest(frame, ramp));
    ramp.index = 0x6099;
    ramp.subindex = 1;
    frame = envelope(0x2B, ramp, true);
    frame.data[4] = 100;
    CHECK(!C::checkRequest(frame, ramp));
    frame.data[0] = 0x23;
    CHECK(C::checkRequest(frame, ramp));
    ++frame.id;
    statusIs(C::checkRequest(frame, ramp), M::Err::FRAME_ERROR, C::SdoError::COB_ID);
    frame = envelope(0x23, ramp, true);
    ++frame.data[3];
    statusIs(C::checkRequest(frame, ramp), M::Err::FRAME_ERROR, C::SdoError::OBJECT);
    frame = envelope(0x23, ramp, true);
    frame.extended = true;
    statusIs(C::checkRequest(frame, ramp), M::Err::UNSUPPORTED, C::SdoError::FRAME_KIND);
}

void words() {
    struct Vector {
        uint8_t width;
        uint8_t bytes[4];
        uint32_t unsignedValue;
        int32_t signedValue;
    };
    const Vector vectors[] = {
        {1, {0, 0, 0, 0}, 0, 0},
        {1, {0x7F, 0, 0, 0}, 127, 127},
        {1, {0x80, 0, 0, 0}, 128, -128},
        {1, {0xFF, 0, 0, 0}, 255, -1},
        {2, {0, 0x80, 0, 0}, 32768, -32768},
        {2, {0xFF, 0x7F, 0, 0}, 32767, 32767},
        {2, {0xFF, 0xFF, 0, 0}, 65535, -1},
        {3, {0, 0, 0x80, 0}, 8388608, -8388608},
        {3, {0xFF, 0xFF, 0x7F, 0}, 8388607, 8388607},
        {3, {0xFF, 0xFF, 0xFF, 0}, 16777215, -1},
        {4, {0, 0, 0, 0x80}, UINT32_C(2147483648), INT32_MIN},
        {4, {0xFF, 0xFF, 0xFF, 0x7F}, INT32_MAX, INT32_MAX},
        {4, {0xFF, 0xFF, 0xFF, 0xFF}, UINT32_MAX, -1},
        {4, {0x18, 0xFC, 0xFF, 0xFF}, UINT32_C(4294966296), -1000},
    };
    for (unsigned i = 0; i < sizeof vectors / sizeof vectors[0]; ++i) {
        const Vector& vector = vectors[i];
        uint32_t unsignedValue = 0x12345678;
        int32_t signedValue = 12345678;
        CHECK(C::readWord(vector.bytes, vector.width, vector.width, unsignedValue));
        CHECK(unsignedValue == vector.unsignedValue);
        CHECK(C::readSignedWord(vector.bytes, vector.width, vector.width, signedValue));
        CHECK(signedValue == vector.signedValue);
        uint8_t padded[] = {0xA5, 0xA5, 0xA5, 0xA5, 0xA5, 0xA5};
        std::memcpy(padded, vector.bytes, vector.width);
        CHECK(C::readWord(padded, sizeof padded, vector.width, unsignedValue));
        CHECK(unsignedValue == vector.unsignedValue);
        CHECK(C::readSignedWord(padded, sizeof padded, vector.width, signedValue));
        CHECK(signedValue == vector.signedValue);
    }
    // A genuinely one-byte object catches accidental four-byte reads under ASan.
    const uint8_t oneByte = 0x80;
    uint32_t unsignedValue = 42;
    int32_t signedValue = 42;
    CHECK(C::readWord(&oneByte, 1, 1, unsignedValue) && unsignedValue == 128);
    CHECK(C::readSignedWord(&oneByte, 1, 1, signedValue) && signedValue == -128);

    const uint8_t bytes[] = {1, 2, 3, 4};
    struct Invalid { const uint8_t* bytes; size_t available; uint8_t width; };
    const Invalid invalid[] = {
        {nullptr, 0, 1}, {nullptr, 4, 4}, {bytes, 0, 1}, {bytes, 3, 4},
        {bytes, 4, 0}, {bytes, 4, 5}, {bytes, 4, 255},
        {bytes, std::numeric_limits<size_t>::max(), 1},
    };
    for (unsigned i = 0; i < sizeof invalid / sizeof invalid[0]; ++i) {
        unsignedValue = 0xA5A5A5A5;
        signedValue = -123456;
        const Saved<uint32_t> savedUnsigned(unsignedValue);
        const Saved<int32_t> savedSigned(signedValue);
        statusIs(C::readWord(invalid[i].bytes, invalid[i].available, invalid[i].width, unsignedValue),
                 M::Err::ILLEGAL_VALUE, C::SdoError::CAPACITY);
        statusIs(C::readSignedWord(invalid[i].bytes, invalid[i].available, invalid[i].width, signedValue),
                 M::Err::ILLEGAL_VALUE, C::SdoError::CAPACITY);
        CHECK(savedUnsigned.unchanged(unsignedValue) && savedSigned.unchanged(signedValue));
    }
}

void overlap() {
    C::SdoReply output = sentinel();
    output.expected = expected();
    output.frame = envelope(0x4B, output.expected);
    const C::SdoExpectation expectation = output.expected;
    const M::Frame frame = output.frame;
    const Saved<C::SdoReply> saved(output);
    statusIs(C::checkReply(output.frame, expectation, output), M::Err::ILLEGAL_VALUE,
             C::SdoError::OVERLAP);
    CHECK(saved.unchanged(output));
    statusIs(C::checkReply(frame, output.expected, output), M::Err::ILLEGAL_VALUE,
             C::SdoError::OVERLAP);
    CHECK(saved.unchanged(output));
    statusIs(C::checkReply(output.frame, output.expected, output), M::Err::ILLEGAL_VALUE,
             C::SdoError::OVERLAP);
    CHECK(saved.unchanged(output));

    uint32_t unsignedValue = 0x12345678;
    int32_t signedValue = -12345678;
    const Saved<uint32_t> savedUnsigned(unsignedValue);
    const Saved<int32_t> savedSigned(signedValue);
    statusIs(C::readWord(reinterpret_cast<const uint8_t*>(&unsignedValue), sizeof unsignedValue,
                         4, unsignedValue), M::Err::ILLEGAL_VALUE, C::SdoError::OVERLAP);
    statusIs(C::readSignedWord(reinterpret_cast<const uint8_t*>(&signedValue), sizeof signedValue,
                               4, signedValue), M::Err::ILLEGAL_VALUE, C::SdoError::OVERLAP);
    CHECK(savedUnsigned.unchanged(unsignedValue) && savedSigned.unchanged(signedValue));

    struct Span { uint8_t prefix[4]; uint32_t output; } span = {{1, 2, 3, 4}, 0x87654321};
    const Saved<Span> savedSpan(span);
    // Even when the bytes actually decoded precede output, the entire declared
    // borrowed span must be disjoint. Its object representation is legal input.
    statusIs(C::readWord(reinterpret_cast<const uint8_t*>(&span), sizeof span, 1, span.output),
             M::Err::ILLEGAL_VALUE, C::SdoError::OVERLAP);
    CHECK(savedSpan.unchanged(span));
    struct SignedSpan { uint8_t prefix[4]; int32_t output; } signedSpan = {{1, 2, 3, 4}, -123};
    const Saved<SignedSpan> savedSignedSpan(signedSpan);
    statusIs(C::readSignedWord(reinterpret_cast<const uint8_t*>(&signedSpan), sizeof signedSpan,
                               1, signedSpan.output), M::Err::ILLEGAL_VALUE, C::SdoError::OVERLAP);
    CHECK(savedSignedSpan.unchanged(signedSpan));
}
} // namespace

int main() {
    channelAndExpectation();
    shapeAndCorrelation();
    replyCommands();
    segmentedAndAbort();
    requests();
    words();
    overlap();
    std::puts("checked SDO initiation and scalar contracts passed");
    return EXIT_SUCCESS;
}
