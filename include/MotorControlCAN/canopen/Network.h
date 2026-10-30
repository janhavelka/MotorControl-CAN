/** @file Network.h
 * @brief Pure checked CANopen NMT requests, boot-up, heartbeat and emergency envelopes.
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

/** Independent observation time; zero is valid and unknown capture is explicit. */
struct ObservationTime {
    bool present = false; ///< A checked observation exists.
    bool receivedTimeKnown = false; ///< Source-qualified capture clock is available.
    uint64_t receivedUs = 0; ///< Original capture microseconds, meaningful only when known.
    uint64_t processedUs = 0; ///< Supplied owner processing microseconds, never substituted for capture.
    Generation generation = 0; ///< Source event epoch; compare with current route before qualification.
};
/** Explicit NMT manager commands; independent of native drive state and motion. */
enum class NmtCommand : uint8_t {
    START = 1, ///< Request NMT Operational; does not establish drive enablement.
    STOP = 2, ///< Request NMT Stopped; does not establish physical standstill.
    PRE_OPERATIONAL = 0x80, ///< Request NMT Pre-operational.
    RESET_NODE = 0x81, ///< Explicit request to reset the remote node.
    RESET_COMMUNICATION = 0x82 ///< Explicit request to reset remote CANopen communication.
};

/** Encode one explicit NMT manager request without sending or admitting work.
 * Produces standard Classical data CAN-ID zero, DLC and payload length two,
 * with command then target bytes and zeroed unused storage. All format flags
 * are false. No state, binding, queue, timing or remote evidence is changed.
 * Independent calls share no mutable state; callers serialize shared outputs.
 * @param[in] command One of the five supported explicit manager commands.
 * @param[in] target Zero broadcasts to the CANopen network; 1..127 addresses
 * one node. Broadcast zero is not an ordinary axis binding.
 * @param[out] output Caller-owned encoded frame, unchanged on failure. No
 * references are retained; successful output owns its bytes.
 * @return OK for a complete request envelope, UNSUPPORTED for an unknown
 * command, or INVALID_CONFIG for target above 127. Command validation precedes
 * target validation. All detail values are zero. Success proves no controller
 * acceptance, transmission, remote transition or motion completion; the caller
 * owns NMT manager authority, explicit execution and observation policy.
 */
Status makeNmtRequest(NmtCommand command, uint8_t target, Frame& output) noexcept;

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
