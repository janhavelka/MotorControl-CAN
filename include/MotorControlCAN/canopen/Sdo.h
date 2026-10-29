/** @file Sdo.h
 * @brief Pure checked scalar SDO initiation envelopes and little-endian words.
 * CANopenNode remains the selected encoder/session engine. These functions
 * perform no I/O, time acquisition, allocation, retry or transfer progression.
 * Inputs are borrowed only during calls; successful replies own their bytes.
 * Independent calls share no mutable state; callers serialize shared outputs.
 * All outputs remain unchanged on failure, including prohibited overlap.
 * SPDX-License-Identifier: MIT
 */
#pragma once
#include <stddef.h>
#include <MotorControlCAN/Endpoint.h>
#include <MotorControlCAN/can/Frame.h>

namespace MotorControlCAN {
/** Protocol-local CANopen representation checks; no common axis state machine. */
namespace CANOPEN {

/** Explicit active channel. The application establishes its physical interface,
 * current binding and route before calling a codec. CAN data contains no host
 * generation/channel token: copying this provenance cannot qualify a late reply.
 * Custom COB-IDs are supported; no 0x600/0x580 node-derived default is inferred. */
struct SdoChannel {
    Binding binding; ///< CANOPEN binding; address is server node 1..127, channel is application-selected.
    uint32_t clientToServerId = 0; ///< Active standard request CAN-ID, 1..0x7ff; no COB-ID configuration flags.
    uint32_t serverToClientId = 0; ///< Active standard reply CAN-ID, 1..0x7ff, distinct from request ID.
};
/** Initiation direction selected by admitted service intent, never by reply data. */
enum class SdoDirection : uint8_t {
    UPLOAD, ///< Client requests a read; initial request command is always 0x40.
    DOWNLOAD ///< Client writes an explicitly sized expedited scalar.
};
/** Expected scalar contract supplied from reviewed profile policy. This is
 * representation metadata, not object access permission or live device evidence.
 * Index/subindex are compared exactly; this layer adds no object whitelist. */
struct SdoExpectation {
    SdoChannel channel; ///< Explicit bound physical-route provenance and CAN-IDs.
    uint16_t index = 0; ///< Expected native index, including any explicitly selected value.
    uint8_t subindex = 0; ///< Exact expected subindex.
    uint8_t width = 0; ///< Reviewed scalar byte width 1..4; zero is invalid, never guessed.
    SdoDirection direction = SdoDirection::UPLOAD; ///< Selects request/reply command family.
};
/** Checked initiation representation, separate from higher service outcome. */
enum class SdoReplyKind : uint8_t {
    EXPEDITED_UPLOAD, ///< Copied scalar payload; size may come from explicit expectation.
    DOWNLOAD_ACK, ///< Matching initiation acknowledgement, not motion completion.
    SEGMENTED_UPLOAD, ///< Legal initiation, with no scalar data or completion yet.
    ABORT ///< Matching native rejection evidence, with every unsigned abort bit retained.
};
/** Owned decoded envelope. Default expectation is invalid. Fields selected by
 * kind are active; default zero fields never manufacture data/size/abort meaning.
 * Raw frame retains ignored reserved/padding bytes for later service evidence. */
struct SdoReply {
    SdoExpectation expected; ///< Copied caller expectation, not an on-wire correlation token.
    Frame frame; ///< Complete checked standard eight-byte data frame, owned by this result.
    SdoReplyKind kind = SdoReplyKind::EXPEDITED_UPLOAD; ///< Meaning of the active fields.
    uint8_t length = 0; ///< Active data prefix for expedited upload only, exactly expected.width.
    uint8_t data[4] = {}; ///< Owned little-endian scalar prefix; inactive suffix is zero.
    bool sizeIndicated = false; ///< True only when upload initiation actually indicates size.
    uint32_t indicatedSize = 0; ///< Wire-indicated byte count when sizeIndicated; zero otherwise.
    uint32_t abortCode = 0; ///< All raw abort bits, meaningful only for ABORT (including code zero).
};
/** Function-specific local detail; never a narrowed native abort. */
enum class SdoError : int32_t {
    NONE, ///< No SDO-specific detail; common frame validation also retains detail zero.
    INVALID_CHANNEL, ///< Invalid binding/protocol/node/active CAN-IDs or identical direction IDs.
    INVALID_EXPECTATION, ///< Invalid direction or reviewed width.
    FRAME_KIND, ///< Valid Classical format outside this standard-data CANopen boundary.
    LENGTH, ///< Initiation envelope is not exactly eight bytes.
    COB_ID, ///< Identifier does not match the explicit selected direction/channel.
    OBJECT, ///< Index or subindex differs from expectation.
    COMMAND, ///< Unexpected command family or nonzero reserved/meaningless command bits.
    WIDTH, ///< Legal size indication conflicts with the reviewed scalar width.
    RESERVED, ///< Noncanonical reserved transmit bytes from a selected-stack request.
    CAPACITY, ///< Null/short/overflowing borrowed byte span or invalid word width.
    OVERLAP ///< Output overlaps a borrowed input object/span.
};

/** Validate explicit protocol-local active channel facts, without bus discovery.
 * @param[in] channel Borrowed caller facts; unchanged.
 * @return OK for valid CANOPEN binding/node and two distinct active 11-bit IDs;
 * INVALID_CONFIG/INVALID_CHANNEL otherwise. Bus compatibility remains owner policy. */
Status validateSdoChannel(const SdoChannel& channel) noexcept;

/** Check a selected-stack outgoing scalar initiation before controller handoff.
 * UPLOAD requires 0x40 and zero reserved tail; DOWNLOAD requires the sized
 * expedited command for width 1/2/3/4 (2f/2b/27/23) and zero unused tail. This
 * is the canonical CANopenNode TX contract, not a second encoder or a claim that
 * receive padding must be zero. No payload value or write permission is inferred.
 * @param[in] frame Borrowed owned-frame value; no bytes are retained or changed.
 * @param[in] expected Bound channel, direction, object and reviewed width.
 * @return Local shape/expectation status, never transmission or protocol success.
 * Invalid expectation precedes frame validation; FD is UNSUPPORTED; unexpected
 * extended/RTR is UNSUPPORTED/FRAME_KIND. Common shape failures retain
 * validateFrame's status/detail; SDO length/object/command mismatches use
 * FRAME_ERROR with SdoError. A legal download size conflicting with reviewed
 * width is ILLEGAL_VALUE/WIDTH. No output or mutable session exists. */
Status checkRequest(const Frame& frame, const SdoExpectation& expected) noexcept;

/** Classify/check an initiation reply atomically. Upload commands 40/41 are
 * segmented, 42 expedited without size, 43/47/4b/4f sized expedited; download
 * acknowledgement is exactly 60. Reserved command bits and nonzero nn when
 * e/s do not both apply reject. Unused receive bytes are opaque per the selected
 * stack: ACK/unsized-segment tails and expedited padding are not interpreted.
 * Every indicated size must match reviewed width. Unsized expedited data uses
 * only expected.width; no absent size bits are guessed. A legal segmented reply
 * remains SEGMENTED_UPLOAD, never an expedited value or completion.
 * @param[in] frame Borrowed standard eight-byte envelope.
 * @param[in] expected Complete explicit route/object/direction/width expectation.
 * @param[out] output Owned checked reply; unchanged on failure. Neither input
 * may overlap any part of output, including output.frame/output.expected.
 * @return OK includes valid ABORT and SEGMENTED_UPLOAD representation; a higher
 * service settles rejection/unsupported progress. INVALID_CONFIG for expectation,
 * ILLEGAL_VALUE/WIDTH for valid wire size conflicting with metadata,
 * ILLEGAL_VALUE/OVERLAP for aliasing; other frame errors follow checkRequest's
 * envelope rules. Success is neither operation permission nor motion completion. */
Status checkReply(const Frame& frame, const SdoExpectation& expected, SdoReply& output) noexcept;

/** Read an unsigned little-endian word without sign interpretation or narrowing.
 * @param[in] bytes Call-only borrowed span, non-null and readable for available bytes.
 * @param[in] available Span size; must cover width. Bytes beyond width are not read.
 * @param[in] width Exact selected byte width 1..4.
 * @param[out] output Unsigned word; unchanged on failure, must not overlap any
 * part of the supplied span. No pointer is retained.
 * @return OK for a complete word; ILLEGAL_VALUE with CAPACITY/OVERLAP otherwise. */
Status readWord(const uint8_t* bytes, size_t available, uint8_t width, uint32_t& output) noexcept;

/** Read a signed two's-complement little-endian word with portable sign extension.
 * No signed shift/overflow or out-of-range unsigned-to-signed conversion.
 * @param[in] bytes Call-only borrowed span, non-null and readable for available bytes.
 * @param[in] available Span size; bytes beyond width are not read.
 * @param[in] width Exact width 1..4; e.g. width 3 yields -8388608..8388607.
 * @param[out] output Signed word, unchanged on failure; no span overlap permitted.
 * @return Same local capacity/overlap status as readWord. No I/O or allocation. */
Status readSignedWord(const uint8_t* bytes, size_t available, uint8_t width, int32_t& output) noexcept;

} // namespace CANOPEN
} // namespace MotorControlCAN
