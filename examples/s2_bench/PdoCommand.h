/** @file PdoCommand.h
 * @brief Bench console lines for the PDO sessions: NMT, watch, PDO record read and write, the
 * supported TPDO1 event-timer configuration, the bounded silent capture, and (PONG pdo=4) the
 * discovery scan, the typed communication reads and the guarded heartbeat commissioning.
 * SPDX-License-Identifier: MIT
 * Application only: parsing, no I/O, clock, heap or device access. A parsed line is a request
 * for one bounded local action; it admits nothing and establishes no device fact.
 */
#pragma once
#include <stddef.h>
#include <stdint.h>

namespace MotorControlCANExample {
/** One console line kind, named by its first character. */
enum class PdoCommandKind : uint8_t {
    NMT,   ///< `N<node>,<o|p|s>`: one addressed NMT Start, Enter Pre-operational or Stop.
    WATCH, ///< `P<seconds>`: watch received frames 1..600 s; `P0` ends a running watch early.
    READ,  ///< `R<node>,<t|r>,<set>`: nine uploads of one transmit or receive PDO set 0..3.
    WRITE, ///< `W<node>,<set>,<subindex>,<baseline>,<value>`: one TPDO communication entry.
    /** `E<node>,<0|100>`: the profile's TPDO1 event-timer change, 1800:05 to raw 0 or raw 100
     * from the other literal, through the typed lifecycle. Not the raw `W` experiment. */
    CONFIGURE,
    /** `C<capture>,<seconds>,<frames>`: one bounded silent capture (SilentCapture.h); `C0` ends a
     * running capture early. Sends nothing on CAN in either form. */
    CAPTURE,
    /** `D<o|p|i>,<seconds>,<select>,<expect>`: the library's discovery scan over both configured
     * nodes (DiscoveryBench.h): `o` passive observation, `p` one 1000:00 probe per node, `i` nine
     * identity uploads per node; a 1..60 s window; then host selection of node 1 or 2, or none (0),
     * which sends nothing; expect 1 (identity only) compares with the retained fixture identity. */
    DISCOVER,
    /** `K<node>`: every typed communication read group of one node (CommunicationBench.h),
     * 95 uploads, no write. */
    COMMUNICATION,
    /** `H<node>,1000`: the reviewed 1017:00 raw 0 to raw 1000 heartbeat commissioning. No other
     * value exists: the reverse transition is not a reviewed workflow. */
    HEARTBEAT
};
/** Parsed console request; only the fields of its kind are meaningful. Numbers are decimal. */
struct PdoCommand {
    PdoCommandKind kind = PdoCommandKind::NMT;
    uint8_t node = 0;      ///< NMT, READ, WRITE, CONFIGURE: CANopen node 1 or 2 of this fixture.
    uint8_t nmt = 0;       ///< NMT: CiA 301 command byte, 0x01 Start, 0x02 Stop, 0x80 Pre-operational.
    uint32_t seconds = 0;  ///< WATCH: 0 ends a running watch, else 1..600 s. CAPTURE: 1..600 s, 0 for C0.
    bool transmit = false; ///< READ: true 0x1800/0x1A00 + set, false 0x1400/0x1600 + set.
    uint8_t set = 0;       ///< READ, WRITE: PDO set 0..3, so TPDO1..TPDO4 or RPDO1..RPDO4.
    uint8_t subindex = 0;  ///< WRITE: 0x1800 + set subindex; RawEvidence checks 1, 2, 3 or 5.
    uint32_t baseline = 0; ///< WRITE: exact value the entry must read before it is changed.
    uint32_t value = 0;    ///< WRITE: exact new value; RawEvidence checks width and rules.
    uint16_t eventTimer = 0; ///< CONFIGURE: target raw 1800:05 literal, 0 or 100; no unit implied.
    uint32_t capture = 0;  ///< CAPTURE: host identity, nonzero; 0 (the whole line `C0`) is a stop.
    uint16_t frames = 0;   ///< CAPTURE: stated frame capacity 1..65535; SilentCapture bounds it.
    uint8_t discovery = 0; ///< DISCOVER: 0 passive (`o`), 1 probe (`p`), 2 identity (`i`).
    uint8_t select = 0;    ///< DISCOVER: host selection afterwards, node 1 or 2, 0 none.
    bool expect = false;   ///< DISCOVER: identity only; compare with the retained fixture identity.
    uint16_t producerTime = 0; ///< HEARTBEAT: requested 1017:00 raw literal, always 1000.
};
/** Parse one complete console line without its line end.
 * Checks syntax, node 1..2, set 0..3, watch seconds 0..600, capture seconds 1..600, the NMT
 * letter and the two event-timer literals 0 and 100 only; the experiment checks entry widths and
 * value rules itself, the profile checks the whole TPDO1 baseline before any write, and the
 * capture checks its identity order and frame capacity. A discovery line needs its mode letter,
 * seconds 1..60, selection 0..2 and expectation 0 or 1 (1 only with `i`); a heartbeat line needs
 * the literal 1000.
 * @param text Borrowed bytes, first byte the command letter; not retained.
 * @param length Byte count; no terminator is needed.
 * @param output Published only on true.
 * @return True for a well-formed line of one kind above.
 */
bool parsePdoCommand(const char* text, size_t length, PdoCommand& output) noexcept;
} // namespace MotorControlCANExample
