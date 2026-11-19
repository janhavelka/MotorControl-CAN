# MKS SERVO42D/57D CAN: engineering evidence review

Review date 2026-11-19. Document evidence only: nothing in this file is a hardware observation, and no profile, codec or test exists. Citations name the source ID from [SOURCES.json](SOURCES.json) and the one-based PDF page, for example MK109 p.30. Statements marked **(inference)** are this review's reasoning from the cited text or from general CAN mechanics, not vendor claims. Hexadecimal is the manual's default number base (MK109 p.16); this review writes it with an `H` suffix or `0x` prefix.

The protocol is **not CANopen**. It is Makerbase's own command/response protocol on classical CAN data frames. It has no NMT, SDO, PDO, SYNC, EMCY, LSS, object dictionary or CiA 402 state machine, and a profile must not invent them.

## 1. Scope and exact applicability

| Item | Evidence | Limit |
|---|---|---|
| Products | MKS SERVO42D_CAN (NEMA17) and SERVO57D_CAN (NEMA23) closed-loop stepper driver boards with pulse and CAN interfaces (MK109 p.8, p.11) | RS485 variants and the Modbus manual are separate products and protocols; not covered |
| SERVO57D_BUS | A third interface drawing "SERVO57D_BUS" (MK109 p.12); the purchase link reads "57D_CAN/BUS" (MK109 p.8) | Its relation to 57D_CAN is not explained |
| SERVO28D/35D | Rows for 28D/35D appear in current defaults (MK109 p.23, p.28), port tables (MK109 p.14, p.62-63) and the 40H board codes S28D_CAN = 6, S35D_CAN = 8 (MK109 p.38) | The manual title and wiring cover only 42D/57D. Treat 28D/35D CAN as **not established**; they need their own evidence |
| Board revisions | Product table: "MKS SERVO42D V1.1" and "MKS SERVO57D V1.3" (MK109 p.10); repository schematics are 42D_CAN V1.0_003 and 57D_CAN V1.1_001 (MKSCH42, MKSCH57) | The schematics are older than the boards the manual names (MK-D24) |
| Hardware facts | 42D: 64 MHz Cortex-M4, 4 MOSFETs, 0-3000 mA; 57D: 144 MHz Cortex-M4, 8 MOSFETs, 0-5200 mA; 12-24 V; 14-bit magnetic encoder (MK109 p.10). Schematics show TJA1051T/3 transceiver with S tied to ground, ESDA6V1L protection, MT6816 encoder, N32L403KBQ7 (42D) and N32L406CBL7 (57D) MCUs (MKSCH42, MKSCH57) | Schematic part identity is for those schematic revisions only |
| Firmware | Each manual says it "corresponds to firmware version" N (MK109 p.1, MK109A p.1, MK106 p.1, MK100 p.1). Version notes cover V1.0.0 (Mar-2023) to V1.0.9 (MK109 p.1-3, MKNOTES) | Two different V1.0.9 manuals exist (Section 13, MK-D01). No firmware image was inspected |
| Motor | Closed-loop use requires a 1.8° motor, phase resistance below 10 Ω and a radial magnet (MK109 p.115) | Step angle is an assumption of every pulse-unit conversion |

## 2. Sources and retrieval

All originals are commit-pinned raw files from the official repository [makerbase-motor/MKS-SERVO42D-57D](https://github.com/makerbase-motor/MKS-SERVO42D-57D) (license field null), retrieved 2026-11-19 with HTTP 200.

| ID | What it is | Why it is kept |
|---|---|---|
| MK109 | CAN manual V1.0.9, file dated 2026-11-12, 118 pages, 3 767 364 bytes, SHA-256 `7342b712…cdcd1` | Newest manufacturer revision; matches the market survey's recorded size and hash |
| MK109A | CAN manual V1.0.9, PDF created 2025-12-18, 106 pages; deleted from HEAD in commit `85e21642` on 2026-11-13, fetched from commit `eaf787cb` | Same version label, different command set (0FH instead of 4CH; no 38H, F7 01, 50H) |
| MK106 | CAN manual V1.0.6 (Sep-2024), 60 pages | Last pre-redesign edition; byte layouts and reply sets that V1.0.9 dropped or changed |
| MK100 | CAN manual V1.0.0 (03/2023), 43 pages | Only source for the original firmware V1.0.0 encodings |
| MKNOTES | Repository README version table | Firmware release notes; last edited 2025-12-18 |
| MKSCH42, MKSCH57 | CAN board schematics | Transceiver, termination and MCU identification |

Retrieval attempts that did not produce a retained original:

| Target | Result |
|---|---|
| AliExpress listings 1005005593922187 (42D_CAN) and 1005005389508166 (57D_CAN/BUS) linked on MK109 p.8 | HTTP 200, about 78 kB JavaScript shells with only an `og:title`; no variant, revision or firmware data; not stored |
| CAN manuals V1.0.1 to V1.0.5 | HTTP 200; compared, superseded by MK106 plus the version notes; hashes recorded in the manifest note; not stored |
| CAN manuals for firmware V1.0.7 and V1.0.8 | Not present in the repository at any examined commit |
| "MKS SERVO42&57D IAP Upgrade commands.pdf" (cited MK109 p.57) | Not in the repository tree |
| github.com/makerbase-motor/MKS-SERVO42D (cited MK109 p.118) | HTTP 404; MKS-SERVO57D redirects to the combined repository |
| Arduino example 11 `.ino` | HTTP 200, read as text: RS485 frame with 0xFA header, not CAN; not stored. Other examples share the "Serial" naming |
| MKS_ServoD_Control V1.0.3 source zip (21.8 MB) | Listed in memory; `Form1.cs` uses `SerialPort` and the 0xFA header, no CAN code; nothing extracted or run; not stored. The CAN-capable V1.0.9.2 tool is a `.rar` and was not downloaded |

No vendor was contacted. Third-party libraries named by the survey were not reviewed here; they could corroborate frame mechanics but not firmware behavior.

## 3. Interface and frame format

### 3.1 Physical layer, bitrate and address

- Standard (11-bit) data frames, data field at most 8 bytes (MK109 p.30). Extended frames, RTR and CAN FD are not mentioned anywhere; a profile must emit only standard data frames.
- Bitrates 125k, 250k, 500k (default), 1M, set by menu CanRate or 8AH with codes 00..03 (MK109 p.25, p.46). Sample point and bit timing are not documented.
- The CAN identifier is the slave address. Range 00..2047: 00 is broadcast, default 01 (MK109 p.30); "1 broadcast CAN ID, 2047 slave CAN ID" (MK109 p.10). Menu CanID offers 01..16 (MK109 p.26); larger values need 8BH with a uint16 address (MK109 p.46). Part 4 says the menu covers "01~10", which equals 16 only under the manual's hexadecimal convention (MK-D20).
- A group ID (8DH, 01..0x7FF, since firmware V1.0.1) makes a drive also execute frames sent to that ID (MK109 p.49; MKNOTES).
- Termination: 120 Ω is not needed for a single drive (MK109 p.20). On 42D it is R13 behind a 2-pin jumper (MKSCH42; "do not connect a shorting cap", MK109 p.20); on 57D it is DIP switch SW1 pin 1 (MK109 p.22). Common ground and shielded twisted pair are required (MK109 p.20).
- The transceiver's silent-mode pin S is tied to ground on both schematics (normal mode), so every powered drive participates in CAN ACK (MKSCH42, MKSCH57). **(inference)** A CAN-level ACK proves only that some node received the frame.

### 3.2 Frame layout, byte order and checksum

Downlink (host to drive) and uplink (drive to host) use the same layout: `ID = slave address`, `DLC = n`, Byte1 = function code, Byte2..Byte(n-1) = data, Byte n = check byte (MK109 p.30). The drive replies on **its own slave ID**, the same identifier the host used, and echoes the function code in Byte1 (MK109 p.31, p.36-37 and p.112 traces showing tx and rx both on 0x001).

- Byte order: big-endian for commands and replies (MK109 p.16).
- Check byte, mode SUM-8 (default): `(ID + Byte1 + ... + Byte(n-1)) & 0xFF` (MK109 p.16, p.30; MK106 p.14 states the formula). Every worked example in MK109 that this review recomputed (more than 40, including group ID 0x50 and broadcast ID 0) agrees. The manual never shows an ID above 0xFF; **(inference)** because the sum is taken modulo 256, adding the full 11-bit ID or only its low byte gives the same result.
- Check byte, fixed mode: always 0x6B, selected by menu CanCRC or 4CH 01 (MK109 p.26, p.48). Exists only in the MK109 edition. Whether uplink frames also carry 0x6B is not stated (MK-D08).
- MK109A instead documents 0FH 01 "no-verification mode": the drive executes frames without checking the check byte; volatile, re-armed at every power-on (MK109A p.45). MK109 no longer lists 0FH.
- Behavior on a wrong check byte, a wrong DLC or an unknown code is not documented (MK-D09). **(inference)** A silently ignored frame is indistinguishable from an absent node.

### 3.3 Signed and packed encodings

| Field | Encoding | Source |
|---|---|---|
| Speed in F6H/FDH | Byte2 bit 7 = direction (0 = CCW, 1 = CW), bits 6-4 reserved, bits 3-0 = speed bits 11-8; Byte3 = speed bits 7-0; 0..3000 | MK109 p.82, p.88 |
| Speed in FEH/F4H/F5H | uint16 in Byte2-3, 0..3000 RPM | MK109 p.92, p.96, p.100 |
| Relative pulses FDH | unsigned 24-bit magnitude 0..0xFFFFFF in Byte5-7; sign is the direction bit | MK109 p.88 |
| Absolute pulses FEH, relative/absolute coordinates F4H/F5H | int24 two's complement, ±8388607 | MK109 p.92, p.96, p.100 |
| Encoder 30H | carry int32 + value uint16 (0..0x3FFF) | MK109 p.31 |
| Encoder 31H, 35H | int48 | MK109 p.32-33 |
| Speed 32H | int16 RPM | MK109 p.32 |
| Pulses 33H, angle error 39H | int32 | MK109 p.33-34 |
| F6H run time | 24-bit, 10 ms units | MK109 p.82 |

## 4. Command and reply inventory

Request data are the bytes between the code and the check byte. "Status" replies are DLC 3 `[code, status, check]` with 1 = success and 0 = fail unless stated. "Since" is the firmware that introduced the command according to the version notes or the first manual that shows it. DLC values in the vendor tables are not reliable (Section 14).

### 4.1 Reads

| Code | Function | Request data | Reply data | Since | Source |
|---|---|---|---|---|---|
| 30H | Encoder carry and in-turn value | none | carry int32, value uint16 (DLC 8) | 1.0.0 | MK109 p.31 |
| 31H | Accumulated multi-turn encoder, 16384 counts/rev; the coordinate for F4H/F5H | none | int48 (DLC 8) | 1.0.0 | MK109 p.32, p.96 |
| 32H | Real-time speed | none | int16 RPM (DLC 4) | 1.0.0 | MK109 p.32 |
| 33H | Received pulse count; also used to view the FEH absolute pulse position | none | int32 (DLC 6) | 1.0.0 | MK109 p.33, p.95 |
| 34H | I/O port state | none | uint8: b0 IN_1, b1 IN_2, b2 OUT_1, b3 OUT_2; with 9EH remap b0 = En, b1 = Dir | 1.0.3 | MK109 p.62 |
| 35H | Raw accumulated encoder | none | int48 (DLC 8) | 1.0.6 | MK109 p.33 |
| 38H | Supply voltage and current | none | V_ACT uint16 mV, I_ACT uint16 mA; table says DLC 5, the screenshot shows DLC 6 | MK109 only | MK109 p.40 |
| 39H | Position angle error, 51200 = 360° | none | int32; table says DLC 4 (MK-D04) | 1.0.0 (int16) | MK109 p.34; MK100 p.13 |
| 3AH | Enable state | none | 1 enabled, 0 not enabled | 1.0.0 | MK109 p.34 |
| 3BH | Homing status | none | MK109: status1 (single-turn), status2 (other homing); each 0 running, 1 success, 2 fail (DLC 4). MK106: one byte (DLC 3) | 1.0.0; 2 bytes 1.0.7 | MK109 p.35; MK106 p.19 |
| 3EH | Stall flag | none | 0 not stalled, 1 stalled | 1.0.0 | MK109 p.55 |
| 40H | Version | none | Byte2 b7-b4 cal flag, b3-b0 hardVer; Bytes3-4 "firmVer[3]"; Byte5 bootVer (0x10 RS485, 0x20 CAN) | first documented in MK109A | MK109 p.38 |
| 42H | Read user ID | none | 4-byte ID (DLC 6) | first documented in MK109A | MK109 p.39 |
| F1H | Motor run status | none | 0 query fail, 1 stop, 2 accelerating, 3 decelerating, 4 full speed, 5 homing, 6 calibrating | 1.0.0 | MK109 p.80 |
| 00H | Read a configuration value | code of the setting | `[code, value as written]`, or `[code, FF, FF]` when not readable | 1.0.6 | MK109 p.37 |
| 01H | Periodic report of a read-only value | code, period uint16 ms (0 stops) | `[01, code, status]`, then unsolicited `[code, value]` frames each period | 1.0.9 | MK109 p.36 |

The readable set for 00H is not listed (MK-D10). PID values are read with a sub-command, `00 96 00`, a DLC 4 request (MK109 p.51).

### 4.2 Configuration writes

| Code | Function | Request data | Reply | Since | Source |
|---|---|---|---|---|---|
| 80H | Calibrate encoder; the motor rotates and must be unloaded | 00 | 0 calibrating, 1 success, 2 fail | 1.0.0 | MK109 p.41, p.115 |
| 82H | Work mode | 0 CR_OPEN, 1 CR_CLOSE, 2 CR_vFOC (default), 3 SR_OPEN, 4 SR_CLOSE, 5 SR_vFOC | status | 1.0.0; values 3-5 changed in 1.0.1 | MK109 p.41; MK100 p.15 |
| 83H | Working current | uint16 mA (saved); or uint16 mA, 00 (not saved) | 0 fail, 1 saved, 2 set without saving | 1.0.0; unsaved form 1.0.7 | MK109 p.42 |
| 84H | Microsteps 1..256 | uint8 (encoding of 256 not stated) | status | 1.0.0 | MK109 p.43 |
| 85H | En pin active level (pulse mode only) | 0 low, 1 high, 2 always | status | 1.0.0 | MK109 p.44, p.24 |
| 86H | Positive direction (pulse interface only) | 0 CW, 1 CCW | status | 1.0.0 | MK109 p.44 |
| 87H | Screen auto-off | 0/1 | status | 1.0.0 | MK109 p.45 |
| 88H | Overcurrent (locked-rotor) protection | 0 off (default), 1 on | status | 1.0.0 | MK109 p.53 |
| 89H | 256-step interpolation | 0/1 (default 1) | status | 1.0.0 | MK109 p.45 |
| 8AH | Bitrate | 0 125k, 1 250k, 2 500k, 3 1M | status | 1.0.0 | MK109 p.46 |
| 8BH | Slave CAN ID | uint16 | status | 1.0.0 | MK109 p.46 |
| 8CH | Reply policy | XX respond 0/1, YY active 0/1 (MK100: one byte) | status | 1.0.0; YY 1.0.4 | MK109 p.47; MK100 p.19 |
| 8DH | Group ID | uint16 0x01..0x7FF | status | 1.0.1 | MK109 p.49 |
| 8FH | Key lock | MK109: 00 and 01 both "Key lock"; MK106: 01 lock, 00 unlock | status | 1.0.3 | MK109 p.48; MK106 p.27 |
| 4CH | Check-byte mode | 0 SUM-8, 1 fixed 0x6B | status | MK109 only | MK109 p.48 |
| 0FH | No-verification mode (volatile) | 01 | status | MK109A only | MK109A p.45 |
| 95H | Position-reached threshold for the "complete" reply | enable (default 1), uint16 (default 200; 65535 = 360°) | status | 1.0.8 | MK109 p.50 |
| 96H, 97H | vFOC and CLOSE PID gains | sub 00: Kp, Ki; sub 01: Kd, Kv; uint16 0..1024 | status | 1.0.8 | MK109 p.51-52 |
| 98H | Host-silence protection time | 4 bytes, ms, 0 off (default) | status | 1.0.9 | MK109 p.50 |
| 9BH | Holding current (OPEN/CLOSE only) | 0..8 = 10..90 % | status | 1.0.4 | MK109 p.43 |
| 9DH | Position out-of-tolerance protection | Byte2 enable; Tim uint16 (1 ≈ 15 ms); Errors uint16 (28000 = 360°) | status | 1.0.6 | MK109 p.54; MK106 p.34 |
| 3DH | Release stall state | none | 1 released, 0 fail | 1.0.0 | MK109 p.55 |
| 3FH | Restore factory settings; the drive restarts | none | 1 success, 0 fail | 1.0.0 | MK109 p.56 |
| 41H | Restart, parameters unchanged | none | 1 success, 0 fail | 1.0.5 | MK109 p.56 |
| 42H | Write user ID | 4 bytes | 1 success, 0 fail | first documented in MK109A | MK109 p.39 |
| 50H | IAP control | 01 enter boot (restarts into IAP), 02 silent, 03 exit silent | status | MK109 only | MK109 p.57-58 |
| 36H | Write outputs | mask/value bits (Section 9) | 1 success, 0 fail | 1.0.6 | MK109 p.63 |
| 9FH | IN_1 as input or output (28D/35D/42D) | 0 input, 1 output | status | first documented in MK109A | MK109 p.62 |
| 99H | OUT_2 pulse divider (57D) | divLevel, divPeriod 4 bytes | status | first documented in MK109A | MK109 p.64 |
| 9EH | Limit port remap to En/Dir (bus mode only) | 0/1 | status | 1.0.4 | MK109 p.66 |
| 90H | Homing and limit parameters | homeTrig 0/1, homeDir 0 CW/1 CCW, homeSpeed uint16 RPM, EndLimit 0/1, hm_mode 0 switch/1 mechanical/2 single-turn | status | 1.0.3; mode 1 1.0.5; mode 2 1.0.9 | MK109 p.67; MK106 p.28 |
| 94H | Mechanical homing offset and current | Hm_offset 4 bytes (default 0x2000 = 180°), Hm_ma uint16 mA | status | 1.0.5 | MK109 p.68 |
| 9AH | Power-on single-turn homing | mode 0 off/1 dir/2 nearest, zero 0 clear/1 set/2 keep, speed 0..4, dir 0 CW/1 CCW | status | 1.0.3 | MK109 p.69 |
| 92H | Set current position as zero | none | status | 1.0.1 | MK109 p.69 |
| 4AH | Synchronization flag | 0 off (default), 1 on | status | 1.0.8 | MK109 p.106 |

### 4.3 Motion and enable

| Code | Function | Request data | Reply status values | Source |
|---|---|---|---|---|
| F3H | Enable in bus mode | 0 release shaft, 1 hold | 1 success, 0 fail | MK109 p.81 |
| F7H | Emergency stop | none; or 01 = "priority" stop regardless of current (MK109 only) | 1 success, 0 fail | MK109 p.81; MK106 p.39 |
| F6H | Speed mode | dir+speed, acc (DLC 5); or + runTime (DLC 8, since 1.0.7) | 0 fail, 1 start, 2 complete, 3 limit stop, 5 latched for sync | MK109 p.82-83 |
| F6H speed 0 | Speed-mode stop, ramp with acc, immediate when acc = 0 | 00 00 acc | 0 fail, 1 stop starting, 2 stop complete | MK109 p.84 |
| FFH | Power-on auto-run of the last F6H command | C8 enable, CA disable | 0 fail, 1 start setting, 2 complete | MK109 p.85 |
| FDH | Relative move in pulses | dir+speed, acc, uint24 pulses | 0 fail, 1 start, 2 complete, 3 limit stop, 5 latched | MK109 p.88-89 |
| FEH | Absolute move in pulses | speed, acc, int24 | same | MK109 p.92-93 |
| F4H | Relative move in encoder counts | speed, acc, int24 | same | MK109 p.96-97 |
| F5H | Absolute move in encoder counts; may be re-sent while running to change speed and target | speed, acc, int24 | same | MK109 p.100-101, p.104 |
| FDH/FEH/F4H/F5H speed 0 | Stop of that mode (ramp with acc, immediate when acc = 0) | speed 0, acc, position 0 | 0 fail, 1 stop starting, 2 stop complete, 3 stopped by limit | MK109 p.90, p.94, p.98, p.102 |
| 91H | Execute homing | 00 origin homing (default when omitted), 01 coordinate zeroing (move to the stored zero) | 0 fail, 1 start, 2 complete | MK109 p.70 |
| 4BH | Execute latched commands | none; sent to ID 00 | no reply | MK109 p.107 |

All motion commands are valid only in the bus modes SR_OPEN/SR_CLOSE/SR_vFOC (MK109 p.78, p.82, p.88). The response to a motion command sent in a pulse mode is not documented.

## 5. Replies, broadcast and reply phases

- **No reply**: frames to the broadcast ID 00 or a group ID (since firmware V1.0.2, MK109 p.1, p.30-31, p.49); 4BH (MK109 p.107); everything except 50H while silent (MK109 p.58); 8CH XX = 0 (MK109 p.47). The V1.0.2 note implies that firmware V1.0.0-V1.0.1 did reply to broadcast and group frames.
- **8CH scope**: the menu equivalent applies "in speed/position mode" (MK109 p.26), and the example is a position command (MK109 p.47). Whether XX = 0 also suppresses replies to reads and configuration writes is not stated (MK-D11).
- **Phases** with the default 8CH 01 01: an immediate status 1 (start) or 0 (fail), then, after the motion ends, an unsolicited status 2 (complete) or 3 (stopped by an end limit) carrying the same code (MK109 p.47). With 8CH 01 00 only the immediate reply is sent (MK109 p.47); the host must then poll F1H (MK109 p.26, p.47).
- **Completion condition**: in position mode "complete" is returned only when the actual position error is below the 95H threshold, and "may not" be returned at all (MK109 p.50, p.89). With no magnet or a distant motor, F1H and position replies can be wrong; the manual's workaround is to disable the 95H check and use open-loop mode (MK109 p.80, p.88).
- **Status 5**: with 4AH enabled, a motion command is accepted, answered with 5 and held until 4BH arrives (MK109 p.83, p.89, p.106-107). The manual suggests repeating 4BH about every 1 ms when drives may miss it (MK109 p.107). Whether a second 4BH can restart or repeat a latched move is not documented (MK-D12).
- **Periodic reports** (01H) arrive as `[code, value]` frames identical in form to the polled reply for that code (MK109 p.36).
- No asynchronous frame is documented for stall, protection, heartbeat expiry, limit hits outside a move, boot-up or errors (Section 8).

## 6. Identity and the equivalent of a state machine

Identity: 40H returns calibration flag, board code (1 S42D_485, 2 S42D_CAN, 3 S57D_485, 4 S57D_CAN, 5-8 for 28D/35D), firmware version, and the bootloader interface (MK109 p.38). The encoding of `firmVer[3]` in two bytes is not explained (MK-D05). 42H stores a 4-byte user ID (MK109 p.39). There is no serial number, vendor ID or product code; the screen "About" item shows firmware and date (MK109 p.29). Two different builds both carry firmware V1.0.9 (Section 13), so 40H alone may not identify the command set.

There is no CiA 402 controlword/statusword. The observable state is spread over separate commands:

| State | Set by | Read by | Notes |
|---|---|---|---|
| Control source | 82H: CR_* = STEP/DIR/EN pulse interface, SR_* = bus | 00H 82 | Default CR_vFOC, so a new drive ignores bus motion until set to SR_* (MK109 p.23, p.78) |
| Loop type | 82H: OPEN (no encoder, max 400 RPM), CLOSE (1500 RPM), vFOC (3000 RPM, adaptive current up to Ma) | 00H 82 | MK109 p.23 |
| Enable | F3H in bus mode; En pin in pulse mode | 3AH | Power-on enable state in bus mode is not documented (MK-D13) |
| Motion | motion commands | F1H, 32H | F1H valid in SR_* modes; in CR_* only calibration is reported (MK109 p.80) |
| Stall/protection latch | 88H, 9DH arm; trip releases the shaft | 3EH; screen "Wrong..."/"Wrong2..." | Cleared by 3DH, the Enter key or releasing the shaft (MK109 p.53-55, p.116) |
| Homing | 91H, power-on 9AH | 3BH, F1H = 5 | MK109 p.35, p.70 |
| Calibration | 80H | 40H cal flag, F1H = 6 | MK109 p.38, p.41 |
| Sync armed | 4AH | 00H 4A (not confirmed readable) | MK109 p.106 |
| Silent / boot | 50H 02 / 01 | none while silent | MK109 p.57-58 |

There is no fault-reset command other than 3DH for the stall latch, no quick-stop or halt option, and no brake control. The enable sequence documented by examples is: set an SR_* mode (MK109 p.86), then F3H 01 where needed, then a motion command. The coordinate-zeroing example requires the motor to be enabled first (MK109 p.74).

## 7. Units and scaling

- **Pulses** (FDH, FEH, 33H): microsteps per full step × 200 per revolution for a 1.8° motor; 3200 per revolution at 16 microsteps (MK109 p.17, p.115). Default 16 microsteps (MK109 p.24).
- **Encoder counts** (30H, 31H, 35H, F4H, F5H): 16384 per revolution, independent of microsteps (MK109 p.17, p.96). The int24 coordinate field limits F4H/F5H targets to ±8388607 counts, about ±512 revolutions, although 31H reports int48.
- **Speed** (bus commands): code 0..3000 meaning RPM at 16, 32 or 64 microsteps; other microstep settings scale by 16/MStep (8 → ×2, 128 → ÷8). Above the mode limit the motor runs at the limit (MK109 p.78). The rule for non-power-of-two microsteps, and whether it also applies to F4H/F5H and 90H homing speed, is not stated (MK-D14).
- **Acceleration code** acc 0..255: the speed changes by 1 RPM every (256 − acc) × 50 µs; acc = 0 means no ramp (MK109 p.79). **(inference)** The ramp rate is 20000 / (256 − acc) RPM/s, from about 78 RPM/s (acc 1) to 20000 RPM/s (acc 255); acc 236 gives 1000 RPM/s, matching the manual's 1 RPM per ms table. Whether the 1 RPM step also scales with microsteps is not stated.
- **Firmware V1.0.0 differs** (MK100 p.22-23): speed code 0..1600 with RPM = speed × 6000 / (MStep × 200), and acc 0..32 added every 10 ms. MK106 p.55 still calls speed 0x140 "600 RPM", a V1.0.0 leftover; MK109 p.82 says 320 RPM (MK-D15).
- **Direction**: F6H/FDH bit 7: 0 = CCW, 1 = CW (MK109 p.82, p.88); 32H speed is positive for CCW (MK109 p.32). MK109 text says 31H and 35H increase by 0x4000 per CW turn (p.32-33), while its 30H/31H examples show +0x4000 for CCW (p.31-32). MK100 had CW positive for speed and encoder (MK100 p.11-12). The relative-move example calls dir 0 "clockwise" (MK109 p.91) and the homing example calls homeDir 0 (CW) "forward" (MK109 p.71). The sign relation between commanded direction, encoder counts and 32H is therefore unresolved (MK-D02).
- **Errors and thresholds** use three different angle scales: 39H 51200 = 360° (MK109 p.34), 95H 65535 = 360° (p.50), 9DH 28000 = 360° (p.54). V1.0.0 39H used 0..0xFFFF = 360° as int16 (MK100 p.13).
- **Current**: mA. 42D/28D/35D maximum 3000 mA, 57D 5200 mA; menu steps 200 or 400 mA; defaults 1600 mA (42D), 3200 mA (57D), 600 mA (28D), 800 mA (35D) (MK109 p.23, p.42). Setting the maximum is discouraged (MK109 p.42).
- **Voltage/current readback** 38H: mV and mA (MK109 p.40).

## 8. Limits and protection

- **End limits**: enabled by 90H EndLimit or the menu; after first use or any limit parameter change a homing (91H) is required (MK109 p.14, p.67, p.75). Left limit stops leftward motion, right limit stops rightward motion; bus mode only; ignored in pulse mode (MK109 p.75). IN_1 is home or left limit; IN_2 (57D only) right limit; 9EH remaps them to En/Dir and then ignores IN_1/IN_2 (MK109 p.14, p.66). "Left" is defined only by examples (left limit with dir bit 1, right with dir bit 0, MK109 p.77). A stopped move replies status 3 (MK109 p.89). MK106 adds that with EndLimit enabled, homing onto the left switch releases the shaft (MK106 p.12, p.29); MK109 omits that note (MK-D16).
- **Stall protection** (88H, default off): overcurrent detection releases the shaft (MK109 p.24, p.53). **Position out-of-tolerance** (9DH, default off): trips when the error exceeds Errors within Tim × ~15 ms (example 180° within 300 ms) (MK109 p.54). Both release the shaft. The flag is read with 3EH and cleared with 3DH; a new stall trips again (MK109 p.55). OUT_1 on 57D outputs stall state (MK109 p.14). No frame reports a trip, and an active move's reply after a trip is not documented (MK-D17).
- **Host-silence protection** (98H, firmware V1.0.9): "if the motor does not receive any instructions from the host computer within the set protection time, it will control the motor to stop urgently" (MK109 p.50; MK109A p.44). Time in ms, default 0 = off. Not documented: which frames count (own ID, group, broadcast, frames with a bad check byte), whether the stop ramps or is immediate, whether the motor stays enabled, whether any frame reports the event, whether the setting persists, and the field's signedness (MK-D18). It is not a CANopen heartbeat: the drive produces no heartbeat frames.
- **Emergency stop** F7H "may fail to stop the unit if the current is too low"; F7H 01 stops regardless of current (MK109 p.81). V1.0.9 notes a fix so F7 stops at low current (MK109 p.2). Immediate stops above 1000 RPM are discouraged (MK109 p.81, p.84). Whether F7H leaves the motor enabled is not stated.
- **Supply**: screen errors for voltage below 12 V or above 24 V (MK109 p.116); 38H reads supply voltage. No CAN-visible voltage fault is documented.
- **No software position limits, following-error object, temperature limit or current-limit fault** are documented beyond the above.

## 9. I/O

34H reads IN_1, IN_2, OUT_1, OUT_2 (MK109 p.62). 36H writes outputs: bits 7-6 OUT2_mask, bits 5-4 OUT1_mask (0 no write, 1 write, 2 keep), bit 3 OUT_2 value, bit 2 OUT_1/IN_1 value (MK109 p.63). On 28D/35D/42D, 9FH 01 turns IN_1 into an output written by bit 2 without the mask (MK109 p.62-63). 99H toggles OUT_2 ("PEND") every divPeriod pulses when divPeriod ≥ 100 (57D only; MK109 p.64). External NPN switches are the default; 57D SW1 pins 2-3 select internal 5 V or external 3.3-24 V switch supply (MK109 p.22). MK106 says En can release the stall latch (MK106 p.10); in V1.0.6 the En line could also trigger single-turn homing by a ~200 ms pulse (MK106 p.33-34).

## 10. Homing

| Method | Configuration | Behavior | Source |
|---|---|---|---|
| Origin switch (hm_mode 0) | 90H level, direction, speed | Searches in homeDir, stops on the rising edge, marks coordinate zero; if started on the switch, first backs off | MK109 p.65, p.71 |
| Mechanical stop (hm_mode 1) | 90H plus 94H offset and current | Drives at Hm_ma current until stalled, moves back by Hm_offset (default 0x2000 counts), marks zero | MK109 p.28, p.65, p.68, p.72 |
| Single-turn (hm_mode 2 / 9AH) | 9AH mode, zero point, speed 0..4, direction | Moves to the stored in-turn zero; with 9AH mode ≠ 0 it runs **automatically at every power-on** | MK109 p.27, p.65, p.69, p.73 |
| Set zero | 92H | Current position becomes zero, no motion | MK109 p.69 |
| Coordinate zeroing | 91H 01 | Moves to the known zero at zero-point speed; motor must be enabled | MK109 p.66, p.74 |

91H replies 1 (start) then 2 (complete) or 0 (fail); 3BH reads the result (MK109 p.35, p.70). Absolute moves require a zeroing first (MK109 p.92, p.100), yet the same pages' examples say that without it the power-on position is zero (MK109 p.95, p.103) (MK-D19). 31H is "the motor rotation range recorded ... after power-on" (MK109 p.32). **(inference)** The coordinate origin is volatile and must be re-established after every reset.

## 11. Persistence

- Menu settings save automatically and take effect immediately (MK109 p.9). Commands documented as menu equivalents (82H-8BH, 9BH, 90H, 9AH, 88H) therefore presumably persist; only 83H states it explicitly, by offering an unsaved variant (MK109 p.42). There is no separate save command (MK-D21).
- Explicitly volatile: 83H with trailing 00 (MK109 p.42) and 0FH (MK109A p.45). Persistence of 98H, 01H, 4AH, 8CH, 95H, 9DH, 96H/97H, 42H and F3H enable is not stated.
- Persistent behavior with motion side effects: FFH stores the last F6H command and runs it at every power-on (MK109 p.85; the example also says the motor slows and stops when FFH C8 is sent, p.87); 9AH power-on homing (MK109 p.27).
- With unchanged limit/home parameters no new homing is needed after power-on for the limit function (MK109 p.76).
- 3FH restores defaults and restarts; since firmware V1.0.8 no recalibration is needed (MK109 p.1, p.56; MK106 p.33 required recalibration). Which settings, including CAN ID and bitrate, return to default is not listed (MK-D22). IAP upgrades keep user parameters (MK109 p.57).
- **(inference)** Frequent configuration writes probably wear flash; the manual gives no endurance figure.

## 12. Timing statements

| Statement | Source |
|---|---|
| Torque loop 20 kHz, speed and position loops 10 kHz | MK109 p.10 |
| Acceleration step (256 − acc) × 50 µs | MK109 p.79 |
| 9DH Tim unit about 15 ms | MK109 p.54 |
| 98H and 01H periods in ms; F6H run time in 10 ms | MK109 p.36, p.50, p.82 |
| Repeat 4BH about every 1 ms if needed | MK109 p.107 |
| Screen auto-off about 15 s | MK109 p.24 |
| Vendor screenshots show replies about 1 ms after requests | MK109 p.112 (illustration, not a specification) |

Not documented: reply latency, minimum command period, receive queue depth, behavior when frames arrive faster than processed, calibration time, restart time after 41H/3FH/50H, and the delay before a changed ID or bitrate takes effect.

## 13. Firmware-dependent differences

| Firmware | Change relevant to a CAN backend | Source |
|---|---|---|
| 1.0.0 | Mode 3 = CR_CAN (only bus mode); speed 0..1600 with RPM = speed × 30 / MStep; acc 0..32 per 10 ms; 32H and encoder CW-positive; 39H int16; 8CH one byte; replies to broadcast implied by the V1.0.2 note | MK100 p.7, p.11-13, p.15, p.19, p.22-23 |
| 1.0.1 | SR_OPEN/SR_CLOSE added, so mode 3 becomes SR_OPEN and SR_vFOC = 5; speed/acc redefined; 92H; group ID 8DH | MKNOTES; MK109 p.1 |
| 1.0.2 | No reply to broadcast or group frames; OUT_1 stall output | MK109 p.1 |
| 1.0.3 | 9AH, 8FH, 34H, left/right limits | MK109 p.1 |
| 1.0.4 | 9BH, FEH, 8CH active byte, F7H, 9EH | MK109 p.1 |
| 1.0.5 | Mechanical homing (90H mode 1, 94H); F4H/F5H step-error fix; F5H real-time update; 41H | MK109 p.1 |
| 1.0.6 | 35H, 9DH (MK106: b1 En-triggered homing, b0 position protection), 00H read, 36H | MK109 p.2; MK106 p.33-35 |
| 1.0.7 | 3BH two bytes; 83H unsaved form; F6H run time | MK109 p.2 |
| 1.0.8 | Restore without recalibration; 4AH/4BH sync; 95H; 96H/97H | MK109 p.2 |
| 1.0.9 (Nov-2025, MK109A) | 01H periodic reports; 98H; 91H coordinate zeroing; 0FH no-verification; 90H mode 2 | MK109A p.2, p.31, p.44-45, p.57 |
| 1.0.9 (Sep-2026, MK109) | 4CH/CanCRC fixed 0x6B; 0FH no longer documented; fixes for a speed-mode-with-limits start needing two commands, current resetting to 400 mA, F7 at low current, 94H offset 0 moving backwards; F7H 01; 38H; 50H silent/boot; 9DH documented with only an enable byte | MK109 p.2-3, p.40, p.48, p.54, p.57-58, p.81 |

The PC tool requires firmware 1.0.9; older firmware works only with generic CAN tools (MK109 p.117). A profile must pin firmware and edition, read 40H, and reject or separately gate commands its firmware does not document.

## 14. What a CAN backend must not assume

1. **No application acknowledgement** for broadcast, group, 4BH, 8CH XX = 0, or silent mode. A transmitted frame and its CAN ACK are not acceptance.
2. **No sequence numbers or transaction IDs.** A reply is matched only by ID and echoed code. Two outstanding requests with the same code to one drive, a periodic 01H report, and a late status 2/3 from an earlier move are indistinguishable from a fresh reply. Keep at most one outstanding request per (drive, code), treat 01H codes as streamed values, and never attribute a "complete" to a move without a boundary (MK109 p.36, p.47).
3. **F5H in-flight updates** make it undocumented whether the superseded command still produces 2. Do not count completions.
4. **"Complete" may never arrive** (95H threshold, stall trip, host-silence stop, wrong encoder readings). Use host deadlines and poll F1H/3EH/31H; preserve execution uncertainty after any unconfirmed motion command and never replay it.
5. **Request and reply share one identifier.** **(inference)** A drive whose group ID equals another drive's slave ID will receive that drive's replies as commands, and they carry a valid SUM-8 check byte; keep group IDs disjoint from all slave IDs. Host and drive frames on the same ID cannot be separated by arbitration.
6. **One protocol domain per bus.** **(inference)** The drives use the whole 11-bit space; ID 0x000 is both CANopen NMT and MKS broadcast, and 0x080 SYNC, 0x081-0x0FF EMCY and 0x701-0x77F heartbeat are valid MKS slave IDs. Do not share a bus with CANopen.
7. **The check byte is not integrity.** It may be fixed 0x6B or disabled (0FH) and its uplink use in 0x6B mode is undocumented. Configure the host to the drive's known mode before probing; a mismatch looks like an absent node.
8. **The DLC in vendor tables is unreliable** (39H, 38H, 40H, F7H 01, 9AH, 94H in MK106, MK109 p.34, p.38, p.40, p.69, p.81; MK106 p.31). Parse by code, validate against the documented layout, and accept the firmware-specific variants (3BH 1 or 2 bytes, F6H 5 or 8 bytes, F7H with or without 01).
9. **Firmware V1.0.9 is not one command set** (MK109 vs MK109A).
10. **No bus-visible fault, stall, limit, boot-up or heartbeat events.** Reboot detection needs reads (for example 40H, 3AH, 31H after an expected change); coordinate origin and possibly enable are lost on restart.
11. **Host-silence protection is undefined in detail**; do not assume that status polls feed it, that it disables the motor, or that it reports anything. Never enable it implicitly.
12. **Bus motion works only in SR_* modes**; the default is CR_vFOC. Changing mode, enable, current, ID, bitrate, check mode, 8CH, FFH, 9AH, 4AH or 50H are explicit user operations, never side effects of probing or recovery.
13. **Speed and acceleration are codes**, not SI quantities; speed depends on the microstep setting; pulse units depend on microsteps and a 1.8° motor; coordinate targets are limited to int24.
14. **Direction sign is unresolved** (MK-D02); establish it by measurement per firmware before mapping common positive direction.
15. **8BH/8AH**: which ID or bitrate carries the reply, and when the change applies, is undocumented. Expect to lose the node and rediscover it explicitly.
16. **Configuration writes persist**; do not write settings cyclically. 3FH and 50H 01 restart the drive.
17. **Stop variants differ**: speed-0 commands ramp with acc or stop immediately; F7H may fail at low current unless F7H 01 is used; none is a documented disable. Local cancellation is not a motor stop.

## 15. Discrepancies and gaps

- **MK-D01** Two different manuals are both "V1.0.9 for firmware V1.0.9": MK109A (firmware dated Nov-2025, 0FH, no 4CH/38H/F7 01/50H) and MK109 (Sep-2026, 4CH, 38H, F7 01, 50H, bug fixes). MKNOTES still lists the Nov-2025 entry and "no-verification mode". Unresolved: how 40H distinguishes the builds.
- **MK-D02** Direction convention: dir 0 = CCW and 32H CCW-positive (MK109 p.32, p.82) versus 31H/35H "CW += 0x4000" text, CCW +0x4000 examples (MK109 p.31-33), dir 0 called clockwise (p.91), homeDir 0 (CW) called forward (p.71); V1.0.0 was CW-positive (MK100 p.11-12).
- **MK-D03** Version history dates: MKNOTES lists manual V1.0.7 with firmware "V1.0.6"; MK109 p.2 says V1.0.7. MK109 places firmware V1.0.9 at Sep-2026, MK109A and MKNOTES at Nov-2025.
- **MK-D04** 39H reply table says DLC 4 with an int32 field (needs DLC 6) in MK109 p.34 and MK106 p.18; V1.0.0 used int16 with DLC 4 (MK100 p.13).
- **MK-D05** 40H table says DLC 5 but lists six bytes and "firmVer[3]" in two bytes (MK109 p.38).
- **MK-D06** 38H table says DLC 5; layout and screenshot give DLC 6 (MK109 p.40).
- **MK-D07** Other DLC or length errors: F7H 01 shown with DLC 2 (MK109 p.81); 9AH DLC 5 for six bytes (MK109 p.69); 94H DLC 6 for eight bytes (MK106 p.31); 8CH example with nine data bytes (MK109 p.47); F5H stop example with nine data bytes (MK109 p.103, p.104).
- **MK-D08** Uplink check byte in fixed 0x6B mode is not stated (MK109 p.26, p.48).
- **MK-D09** Handling of bad check byte, wrong DLC, unknown code or out-of-range values is not documented.
- **MK-D10** The set of settings readable with 00H is not listed; PID reads use a different request shape (MK109 p.37, p.51).
- **MK-D11** Scope of 8CH XX = 0 (motion only or all replies) is unclear (MK109 p.26, p.47).
- **MK-D12** Effect of repeated 4BH on already started latched moves is not documented (MK109 p.107).
- **MK-D13** Enable state after power-on or restart in bus mode is not documented.
- **MK-D14** Speed scaling outside 16/32/64 microsteps and its application to F4H/F5H/90H are not defined (MK109 p.78).
- **MK-D15** MK106 p.55 says speed 0x140 runs at 600 RPM; MK106 p.40 and MK109 p.82 say 320 RPM. The 600 RPM value matches the V1.0.0 formula (MK100 p.22).
- **MK-D16** MK106 says homing onto the left switch with EndLimit enabled releases the shaft (MK106 p.12, p.29); MK109 is silent. Unknown whether behavior changed.
- **MK-D17** Reply of an active move after a stall trip, host-silence stop or emergency stop is not documented.
- **MK-D18** 98H: feeding frames, stop type, resulting enable state, reporting, persistence and field signedness are not documented (MK109 p.50).
- **MK-D19** Absolute moves "must" be preceded by zeroing (MK109 p.92, p.100) while examples say power-on position is zero by default (MK109 p.95, p.103).
- **MK-D20** CAN ID menu range is "01~10" in Part 4 (MK109 p.30) and 01..16 in the menu section (p.26); consistent only if "10" is hexadecimal.
- **MK-D21** Persistence is stated per command only for 83H and 0FH; there is no general persistence table.
- **MK-D22** 3FH does not list which settings (CAN ID, bitrate, check mode, group ID) return to default (MK109 p.56).
- **MK-D23** 8FH: MK109 p.48 labels both 00 and 01 "Key lock"; MK106 p.27 says 01 lock, 00 unlock. 5.2.13 heading says "Same as CanRate" for the check-mode command (MK109 p.48).
- **MK-D24** Board revisions: manual names 42D V1.1 and 57D V1.3 (MK109 p.10); schematics are 42D_CAN V1.0_003 and 57D_CAN V1.1_001. 57D MOSFET rating is 40 V/60 A on p.10 and 30 V/70 A on p.9.
- **MK-D25** Hm_Ma defaults are 42D 400 mA / 57D 800 mA in MK109 p.28 but 42D 800 mA / 57D 400 mA in MK106 p.12.
- **MK-D26** RS485 frames copied into the CAN manuals: IAP silent commands written as `FA 00 50 02 CRC` and an `FB addr 50` reply (MK109 p.58); `FA 01 3E` and `FA 01 FD ...` (MK106 p.19, p.26).
- **MK-D27** 9DH layout: MK106 p.34 defines Byte2 b1 = En-triggered single-turn homing and b0 = position protection; MK109 p.54 documents Byte2 only as enable. Whether b1 still works is unknown.
- **MK-D28** Three different angle scales for position error and thresholds (39H 51200, 95H 65535, 9DH 28000 per 360°) without explanation (MK109 p.34, p.50, p.54).
- **MK-D29** 84H takes a uint8 for 1..256 microsteps; the encoding of 256 is not stated (MK109 p.43; MK106 p.22 says 00~FF).
- **MK-D30** The relation between 31H (accumulated) and 35H (raw accumulated) encoder values is not explained, and 33H "received pulses" doubles as the FEH absolute position without a definition (MK109 p.32-33, p.95).
- **MK-D31** MK106 headers say V1.0.5 on every page although the version table and file name say V1.0.6.
- **MK-D32** Silent mode: "does not respond to commands other than 50" (MK109 p.58) does not say whether other commands are ignored or executed silently.
- **MK-D33** No CAN manual for firmware V1.0.7 or V1.0.8 and no IAP command document are published; SERVO28D/35D CAN coverage is only incidental.

## 16. What a profile can implement from this evidence

From documents alone a profile can implement, behind a pinned firmware and edition gate: the standard-frame codec with SUM-8 and fixed-0x6B check modes and big-endian signed fields; typed reads (30H-35H, 38H, 39H-3EH, 40H, 42H, F1H, 00H); explicit configuration operations (82H-9FH, 4CH, 95H-98H) as user-requested commands that persist; enable (F3H); speed mode, relative/absolute pulse and encoder-count moves with immediate status and optional unsolicited completion; ramped and immediate stops and both emergency-stop forms; homing configuration and execution; I/O reads and writes; broadcast/group/synchronized dispatch modeled as unacknowledged.

Exact common targets can map to encoder counts (16384/rev) for F4H/F5H with checked int24 range, and to pulses only with a known microstep setting and step angle. Speed and acceleration can map only through the documented code rules with explicit quantization, and only for 16/32/64 microsteps until other settings are measured.

Blocked until measured on an actual board and firmware: direction sign (MK-D02), reply DLCs for 38H/39H/40H, 98H semantics, completion behavior after stall/limit/host-silence/F5H updates, 8CH scope, 8AH/8BH reply behavior, reset effects on enable and coordinates, check-byte mode on replies, and how 40H distinguishes the two V1.0.9 builds. A complete profile also needs a decision on supporting firmware older than V1.0.9 and on SERVO28D/35D.
