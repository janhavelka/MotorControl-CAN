# 03 — Reads, identity and dialect selection

Execute this stage under the [execution contract](execution_contract.md).
Status: NOT STARTED. Run after 02b. MK-D01, D05, D10, D24, D28, D30 and D34 gate individual interpretations; raw
values stay available where an interpretation is gated.

## Work

1. Dialect declaration: consume 02's `DialectDeclaration` (dialect, check mode, uplink check, reply policy, group ID
   and the caller's expected board code 2 or 4) as 02b binds it to a binding generation: no default, no I/O, a change
   invalidates that binding's reads, qualifications and pending work, and reads without one return `INVALID_CONFIG`
   before admission. This stage adds the typed reads and the `mks dialect` route over it.
2. Typed reads with raw-preserving observations and explicit validity flags (`prepareIdentity`, `prepareConfig`,
   `prepareState`, `nextRead`, `advanceRead`, checked getters):
   - encoder 30H (carry int32, value 0..0x3FFF), 31H (int48, the coordinate for F4H/F5H), 35H (raw int48), 33H
     (received pulses, int32, also viewed as the FEH absolute position, MK109 p.95), 32H (int16 rpm), 39H (angle error),
     34H (I/O bits; with 9EH remap bit 0 is En and bit 1 Dir, MK109 p.62), 38H (mV, mA; MK109 dialect only), 3AH
     (enable), 3BH (homing status, one or two bytes per dialect), 3EH (stall flag), F1H (run status 0..6), 40H, 42H;
   - 00H configuration read per ledger code, with the PID shape `00 96 00` separately (MK109 p.37, p.51). The readable
     set is not listed (MK-D10): each code stays "read unestablished" until 12A Q3 or a document confirms it. A 00H
     read of an unestablished code is a bootstrap query, admitted only for a ledger setting code whose 00H read effect
     01 reviewed; its result is raw and marked "read unestablished", never 08's prior-read or readback evidence.
     `[code, FF, FF]` means not readable only for a one-byte setting; for a two-byte setting it is ambiguous with
     0xFFFF (MK-D34) and is published raw with no decoded value.
3. Units and provenance: 31H and 35H at 16384 counts per turn; their relation and the 33H meaning stay unresolved
   (MK-D30), so each is a separate typed field and none is called "actual position" without qualification. The
   coordinate origin is the power-on position or the last zeroing (MK109 p.32, p.95; inference: volatile across
   restart). Direction sign is unresolved (MK-D02): publish native signs; a common-direction projection needs a
   caller-supplied, generation-bound `directionQualified` declaration with its evidence reference. Angle-error scales
   differ per command (MK-D28); 39H is 51200 per turn for the V1.0.9 tables. V1_0_0's 39H is printed `int16_t` while
   its text maps 0..FFFF to 0..360° (MK100 p.13, MK-D38): publish the raw 16 bits and the signed reading as separate
   fields at 65536 per turn, the signed reading unqualified.
4. Validity: encoder fields are measured shaft angle only when 40H reports calibrated; a magnetless or distant motor
   can give wrong F1H and position values (MK109 p.80, p.88), so an uncalibrated or unknown-calibration binding
   publishes raw values without a position claim. F1H is meaningful in the dialect's bus modes; in the V1.0.9 tables'
   CR_* modes it reports only calibration (p.80). 32H and F1H are firmware reports, not measured shaft speed or
   standstill.
5. Identity: `IdentityObservation` from 40H: calibration flag, board code, raw firmware bytes (undecoded, MK-D05),
   bootloader interface (0x20 CAN, 0x10 RS485). Board codes 2 and 4 are in scope; 1, 3, 5 and 7 are RS485 boards and
   6 and 8 are 28D/35D CAN (not established): each refuses dependent work as a contradiction. Model-dependent work
   (08's current and I/O limits, 99H and 9FH applicability, board-specific inputs) needs a fresh observed board code
   equal to the declared one: an in-scope but different code (2 observed with 4 declared, or the reverse) is a
   contradiction too, with nothing sent, outputs unchanged and the binding's model-dependent qualifications
   invalidated. Fresh means admission evidence, not a bootstrap observation: the probe and an identity read record
   any checked 40H frame, `possiblyLate` included, as an observation of a responder and its raw fields, but dependent
   work needs a 40H reply to a read handed off in the current binding generation and identity session, captured
   after that handoff and attributed `QUALIFIED` or `QUIET_INTERVAL` (02b step 6). A `possiblyLate` or earlier-session
   40H admits nothing. Stops (04) consume no identity: as [prompt 57](../57_regular_drive_operations.md) exempts
   stops, an explicitly requested stop stays admitted on a missing, stale or contradicting identity (its route,
   generation and dialect stop-row checks stay), its result carrying that identity and claiming no standstill.
   40H cannot separate
   MK109 from MK109A (MK-D01); the observation says so. 40H is first documented in MK109A, so a V1_0_0 binding has no
   identity read; whether its caller declaration can stand as identity is [README](README.md) open question 3, and
   until it is answered V1_0_0 writes and motion stay gated. 42H returns the 4-byte user ID raw.
   Under contract 02 "Identity and device evidence" the coverage row of each later write or motion names the 40H
   product and interface fields it consumes by ledger key, the holder (an application record keyed to binding,
   generation and the host-observed session) and the invalidators: controller restart or bus-off, rebinding, a
   dialect declaration change, 41H, 3FH, a confirmed ID or bitrate change, a power cycle the caller records (02b
   step 6) and a contradicting 40H read. The protocol
   has no boot-up report (review section 14 rule 10), so an uncommanded power cycle is not observed; record that
   limit. Firmware bytes are not consumed: dependent terminal results carry them raw with the declared dialect,
   flagged unqualified, as in prompt 57's identity policy.
6. Work-mode gate: read 82H through 00H where readable. Motion stages admit only a known bus mode of the declared
   dialect, by readback or an explicit caller declaration recorded in the evidence: SR_OPEN, SR_CLOSE or SR_vFOC in
   the V1.0.9 tables (MK109 p.23, p.78), CR_CAN (mode 3) in `V1_0_0`, its only bus mode (MK100 p.7, p.15). Any other
   or unknown mode refuses motion before TX. The gate is independent of write authorization: `V1_0_0` motion stays
   refused by the identity gate (step 5) whatever the mode.
7. Capabilities: fill `ReadCapabilities` and `DiscoveryCapabilities` per dialect from the ledger; `Capabilities` stay
   caller-supplied (stepper motor kind; encoder feedback present on the board, closed-loop control only in CLOSE and
   vFOC modes). Board revision (MK-D24) is a recorded fact, not a capability.
8. Streams: a polled read of code c on an address with an active or unknown-state 01H stream for c is refused before TX
   (09 implements streams; this stage adds the refusal check and its test). Every code the declared dialect can
   report starts `UNKNOWN` at first bind, rebind and restart (09 step 3); the dialect declaration says which codes can
   stream, never their state. The probe and the 40H identity read are not refused: a responder and the static 40H
   fields hold as observations whether the frame is a reply or a sample, and the result records which it may be;
   admission evidence follows step 5.
9. Retry class per read from 01's decision; without the owner decision reads are `NOT_REPEATABLE`.

## Deliver

`Reads.h/.cpp`, `ReadContext`, `DialectDeclaration`, typed observations (`EncoderObservation`, `StateObservation`,
`IdentityObservation`, `ConfigObservation`), `test/mks_servo_can_reads_test.cpp`, owner-backed app and console cases
(`mks read identity|config|state`, `mks dialect`), API/CLI parity (console routes and parity once S4 lands), coverage
rows, report and PROGRESS row.

## Acceptance

Exact request bytes per read and dialect; every accepted reply length and the raw-retaining failures; sign extension
of int24/int48; validity flags for calibration, mode and dialect; the work-mode gate per dialect (CR_CAN admitted by
the gate on V1_0_0, SR_* refused there, CR_* refused on the V1.0.9 tables); 00H FF FF handling per setting width;
refused reads for undeclared bindings, absent codes (38H on MK109A or V1_0_0, 40H on V1_0_0) and active or unknown
streams, including a stream left active before the host started; with the correct label, wrong (RS485 or 28D/35D
board code, bootloader 0x10), swapped (board 2 observed with 4 declared and the reverse: no TX, unchanged output,
model-dependent qualifications invalidated), missing and stale (after restart notification, rebinding or a recorded
power cycle) identity refusing dependent work with nothing sent; a delayed (`possiblyLate`) 40H at attachment and
after rebinding recorded as an observation that admits no model-dependent write; a stop admitted on contradicting
identity; a 00H bootstrap query of an unestablished code published raw and refused as 08 readback evidence; stale
generation, cancellation and deadline cases;
unchanged outputs on failure. A good reply never proves model, firmware edition, readiness or direction. The full
matrix of contract 02 "Registration acceptance" for this major change and CI on the exact final pushed commit pass.
Hardware NOT RUN. Commit, push and update the ledger and handoff.
