/** @file Frame.h
 * @brief Owned Classical CAN frame values and format validation.
 * SPDX-License-Identifier: MIT
 */
#pragma once

#include <MotorControlCAN/Status.h>

namespace MotorControlCAN {

/** Owned Classical CAN frame preserving wire format, DLC and actual byte count.
 * RTR has no data payload even when its requested DLC is nonzero. The fd flag
 * expresses a requirement rejected by this Classical-only type; there is no FD
 * storage path. A future FD adapter requires its own bounded contract. Default
 * is a valid empty standard data frame. Unused data bytes have no meaning.
 * Timestamps, error frames and controller/bus status stay outside this wire value.
 * There is no raw flag mask to silently discard unknown adapter flags. */
struct Frame {
    uint32_t id = 0; ///< Identifier, 0..0x7ff standard or 0..0x1fffffff extended.
    uint8_t dlc = 0; ///< Classical wire DLC, 0..8; requested byte count for RTR.
    uint8_t payloadLength = 0; ///< Copied data bytes, 0..8; zero for RTR, equal to DLC otherwise.
    bool extended = false; ///< Preserve 29-bit wire format even when id fits 11 bits.
    bool rtr = false; ///< Remote request with no data payload.
    bool fd = false; ///< Unsupported FD requirement; rejected by validateFrame before other checks.
    bool brs = false; ///< FD bit-rate-switch wire flag; invalid without FD, which is unsupported here.
    bool esi = false; ///< FD error-state-indicator wire flag; invalid in Classical frames, not host health.
    uint8_t data[8] = {}; ///< Owned payload storage; only payloadLength bytes are meaningful.
};

/** Validate identifier, format, DLC and payload shape without protocol restrictions.
 * Both standard and extended data/RTR frames are representable. Independent calls
 * share no mutable state; the caller serializes concurrent mutation of the input.
 * Performs bounded validation only, with no allocation, I/O or clock access.
 * @param[in] frame Caller-owned frame; unchanged and borrowed only during this call.
 * @return UNSUPPORTED for fd=true, taking precedence over other invalid fields;
 * otherwise FRAME_ERROR for FD-only brs/esi flags, an out-of-range identifier,
 * DLC/length, RTR data, or data length unequal to DLC. Returns OK for a
 * representable Classical frame.
 * All returned detail values are zero. Success sends no frame and establishes no
 * protocol acceptance, controller capability or device acknowledgement.
 */
Status validateFrame(const Frame& frame) noexcept;

} // namespace MotorControlCAN
