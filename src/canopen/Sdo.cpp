// SPDX-License-Identifier: MIT
#include <MotorControlCAN/canopen/Sdo.h>

namespace MotorControlCAN {
namespace CANOPEN {
namespace {
Status fail(SdoError reason, const char* message, Err code = Err::FRAME_ERROR) {
    return Status(code, static_cast<int32_t>(reason), message);
}
bool spanValid(const void* pointer, size_t size) {
    return size <= UINTPTR_MAX - reinterpret_cast<uintptr_t>(pointer);
}
bool overlaps(const void* a, size_t aSize, const void* b, size_t bSize) {
    const uintptr_t first = reinterpret_cast<uintptr_t>(a);
    const uintptr_t second = reinterpret_cast<uintptr_t>(b);
    return first < second + bSize && second < first + aSize;
}
uint32_t word(const uint8_t* bytes, uint8_t width) {
    uint32_t result = 0;
    for (uint8_t i = 0; i < width; ++i) result |= uint32_t(bytes[i]) << (8u * i);
    return result;
}
Status expectation(const SdoExpectation& e) {
    Status s = validateSdoChannel(e.channel);
    if (!s) return s;
    if (!e.width || e.width > 4 || e.direction > SdoDirection::DOWNLOAD)
        return fail(SdoError::INVALID_EXPECTATION, "invalid reviewed SDO width/direction", Err::INVALID_CONFIG);
    return Ok();
}
Status envelope(const Frame& frame, const SdoExpectation& e, bool reply) {
    Status s = validateFrame(frame);
    if (!s) return s;
    if (frame.extended || frame.rtr)
        return fail(SdoError::FRAME_KIND, "SDO channel requires standard data frames", Err::UNSUPPORTED);
    if (frame.payloadLength != 8)
        return fail(SdoError::LENGTH, "SDO initiation requires eight bytes");
    const uint32_t id = reply ? e.channel.serverToClientId : e.channel.clientToServerId;
    if (frame.id != id) return fail(SdoError::COB_ID, "wrong selected SDO channel identifier");
    if (word(frame.data + 1, 2) != e.index || frame.data[3] != e.subindex)
        return fail(SdoError::OBJECT, "wrong SDO index/subindex");
    return Ok();
}
Status widthMatch(uint32_t indicated, uint8_t reviewed) {
    return indicated == reviewed ? Ok() :
        fail(SdoError::WIDTH, "SDO size conflicts with reviewed scalar width", Err::ILLEGAL_VALUE);
}
Status wordInput(const uint8_t* bytes, size_t available, uint8_t width, const void* output, size_t outputSize) {
    if (!bytes || !width || width > 4 || available < width || !spanValid(bytes, available))
        return fail(SdoError::CAPACITY, "invalid borrowed word span/width", Err::ILLEGAL_VALUE);
    if (overlaps(bytes, available, output, outputSize))
        return fail(SdoError::OVERLAP, "word output overlaps borrowed bytes", Err::ILLEGAL_VALUE);
    return Ok();
}
} // namespace

Status validateSdoChannel(const SdoChannel& c) noexcept {
    if (!validateBinding(c.binding) || c.binding.endpoint.protocol != Protocol::CANOPEN ||
        !c.binding.endpoint.address || c.binding.endpoint.address > 127 ||
        !c.clientToServerId || c.clientToServerId > 0x7ff ||
        !c.serverToClientId || c.serverToClientId > 0x7ff || c.clientToServerId == c.serverToClientId)
        return fail(SdoError::INVALID_CHANNEL, "invalid active CANopen SDO channel", Err::INVALID_CONFIG);
    return Ok();
}
Status checkRequest(const Frame& frame, const SdoExpectation& e) noexcept {
    Status s = expectation(e); if (!s) return s;
    s = envelope(frame, e, false); if (!s) return s;
    const uint8_t command = frame.data[0];
    uint8_t active = 0;
    if (e.direction == SdoDirection::UPLOAD) {
        if (command != 0x40) return fail(SdoError::COMMAND, "upload initiation command must be 0x40");
    } else {
        // The selected engine always indicates the known scalar download size.
        if ((command & 0xf3u) != 0x23)
            return fail(SdoError::COMMAND, "unexpected canonical scalar download command");
        active = uint8_t(4u - ((command >> 2) & 3u));
        s = widthMatch(active, e.width); if (!s) return s;
    }
    for (uint8_t i = uint8_t(4u + active); i < 8; ++i)
        if (frame.data[i]) return fail(SdoError::RESERVED, "nonzero canonical request padding");
    return Ok();
}
Status checkReply(const Frame& frame, const SdoExpectation& e, SdoReply& output) noexcept {
    if (overlaps(&frame, sizeof frame, &output, sizeof output) ||
        overlaps(&e, sizeof e, &output, sizeof output))
        return fail(SdoError::OVERLAP, "reply output overlaps borrowed input", Err::ILLEGAL_VALUE);
    Status s = expectation(e); if (!s) return s;
    s = envelope(frame, e, true); if (!s) return s;
    SdoReply next; next.expected = e; next.frame = frame;
    const uint8_t command = frame.data[0];
    if (command == 0x80) {
        next.kind = SdoReplyKind::ABORT;
        next.abortCode = word(frame.data + 4, 4);
    } else if (e.direction == SdoDirection::DOWNLOAD) {
        if (command != 0x60) return fail(SdoError::COMMAND, "expected download acknowledgement");
        next.kind = SdoReplyKind::DOWNLOAD_ACK;
    } else {
        if ((command & 0xf0u) != 0x40 ||
            ((command & 3u) != 3u && (command & 0x0cu)))
            return fail(SdoError::COMMAND, "invalid upload initiation command/size bits");
        next.sizeIndicated = (command & 1u) != 0;
        if (command & 2u) {
            next.kind = SdoReplyKind::EXPEDITED_UPLOAD;
            next.length = e.width;
            if (next.sizeIndicated) {
                next.indicatedSize = 4u - ((command >> 2) & 3u);
                s = widthMatch(next.indicatedSize, e.width); if (!s) return s;
            }
            // Unsized replies use only the reviewed prefix, never all four by guess.
            for (uint8_t i = 0; i < next.length; ++i) next.data[i] = frame.data[4u + i];
        } else {
            next.kind = SdoReplyKind::SEGMENTED_UPLOAD;
            if (next.sizeIndicated) {
                next.indicatedSize = word(frame.data + 4, 4);
                s = widthMatch(next.indicatedSize, e.width); if (!s) return s;
            }
        }
    }
    output = next;
    return Ok();
}
Status readWord(const uint8_t* bytes, size_t available, uint8_t width, uint32_t& output) noexcept {
    Status s = wordInput(bytes, available, width, &output, sizeof output);
    if (!s) return s;
    output = word(bytes, width);
    return Ok();
}
Status readSignedWord(const uint8_t* bytes, size_t available, uint8_t width, int32_t& output) noexcept {
    Status s = wordInput(bytes, available, width, &output, sizeof output);
    if (!s) return s;
    const uint32_t bits = word(bytes, width);
    const unsigned count = unsigned(width) * 8u;
    const int64_t value = (bits & (uint32_t(1) << (count - 1u))) ?
        int64_t(bits) - (INT64_C(1) << count) : int64_t(bits);
    output = static_cast<int32_t>(value);
    return Ok();
}
} // namespace CANOPEN
} // namespace MotorControlCAN
