/** @file Network.h
 * @brief Pure checked CANopen boot-up, heartbeat and emergency envelopes.
 * No I/O, clock access, allocation, retry or network configuration occurs here.
 * Inputs are borrowed during calls only; successful events own their bytes.
 * Independent calls share no mutable state; callers serialize shared outputs.
 * SPDX-License-Identifier: MIT
 */
#pragma once

#include <MotorControlCAN/Endpoint.h>
#include <MotorControlCAN/can/Frame.h>

namespace MotorControlCAN {
namespace CANOPEN {

/** Supported native NMT observations; these are separate from drive state. */
enum class NmtState : uint8_t {
    INITIALIZING = 0, ///< Boot-up indication, not a periodic heartbeat.
    STOPPED = 4, ///< NMT Stopped; does not establish physical standstill.
    OPERATIONAL = 5, ///< NMT Operational; does not establish drive enablement.
    PRE_OPERATIONAL = 127 ///< NMT Pre-operational.
};

/** Which checked event fields are meaningful. */
enum class NetworkEventKind : uint8_t {
    BOOTUP, ///< Initializing payload; requires owner lifecycle handling.
    HEARTBEAT, ///< Periodic native NMT-state observation.
    EMCY ///< Raw error evidence; does not settle a command or clear a fault.
};

/** Explicit event routing supplied by the application; default is invalid.
 * Binding and generation are host provenance, never wire correlation tags.
 * No identifier is derived from the node address or EDS factory metadata. */
struct NetworkRoute {
    Binding binding; ///< CANOPEN node 1..127 plus physical interface and profile identity.
    Generation generation = 1; ///< Nonzero event-route epoch; never reuse while old ingress exists.
    uint32_t heartbeatId = 0; ///< Configured standard heartbeat CAN-ID, 1..0x7ff.
    uint32_t emcyId = 0; ///< Configured standard EMCY CAN-ID, 1..0x7ff, distinct from heartbeatId.
};

/** Owned checked event. Only fields selected by kind carry decoded meaning.
 * The default route is invalid. This value carries no timestamp or freshness;
 * the receive owner supplies observation time and lifecycle policy separately. */
struct NetworkEvent {
    NetworkRoute route; ///< Copied expected host provenance; no on-wire generation claim.
    Frame frame; ///< Entire checked Classical standard data envelope, including raw payload.
    NetworkEventKind kind = NetworkEventKind::BOOTUP; ///< Selects heartbeat/state or error fields.
    NmtState state = NmtState::INITIALIZING; ///< Valid for BOOTUP and HEARTBEAT only.
    uint16_t errorCode = 0; ///< Raw little-endian EMCY error code; unknown and zero codes are retained.
    uint8_t errorRegister = 0; ///< Entire EMCY register byte, including reserved bit 6 without interpretation.
    uint8_t manufacturer[5] = {}; ///< Owned EMCY bytes 3..7; no Lichuan-specific interpretation.
};

/** Validate supplied route facts without probing or configuring any device.
 * @param[in] route Call-only borrowed binding, nonzero route generation and two
 * distinct configured standard identifiers. The binding must name CANOPEN node
 * 1..127. Bus compatibility and live configuration evidence remain owner policy.
 * @return OK for valid route representation; INVALID_CONFIG otherwise. Detail
 * is zero. Success establishes neither device presence nor identifier freshness.
 */
Status validateNetworkRoute(const NetworkRoute& route) noexcept;

/** Check and copy one passive network event atomically.
 * Supported heartbeat payloads are 00 (boot-up), 04, 05 and 7f. EMCY requires
 * eight bytes; every error-code/register/manufacturer bit is retained. Unknown
 * error codes remain valid raw evidence. This is an envelope boundary for the
 * selected service backend; it performs no state progression or health policy.
 * @param[in] frame Call-only borrowed Classical standard data frame. Heartbeat
 * length is exactly one byte; EMCY length is exactly eight bytes.
 * @param[in] expected Explicit configured identifiers and current provenance.
 * @param[in] sourceBinding Earliest-ingress host binding; every field must match
 * expected.binding. Copying a current binding cannot qualify delayed wire data.
 * @param[in] sourceGeneration Earliest-ingress route epoch; must equal the
 * nonzero expected.generation. This value is not carried on the CAN wire.
 * @param[out] output Owned event, unchanged on failure. No part of output may
 * overlap frame, expected or sourceBinding; references are never retained.
 * @return OK proves a matching supported envelope only. ILLEGAL_VALUE rejects
 * overlap before any other validation. INVALID_CONFIG rejects invalid expected
 * route or mismatched provenance. validateFrame errors retain its status;
 * extended/RTR and unknown heartbeat values are UNSUPPORTED. Other route/length
 * mismatches are FRAME_ERROR. All detail values are zero. A boot-up is not a
 * heartbeat, and neither event proves watchdog behavior or physical motion.
 */
Status checkNetworkEvent(const Frame& frame, const NetworkRoute& expected,
                         const Binding& sourceBinding, Generation sourceGeneration,
                         NetworkEvent& output) noexcept;

} // namespace CANOPEN
} // namespace MotorControlCAN
