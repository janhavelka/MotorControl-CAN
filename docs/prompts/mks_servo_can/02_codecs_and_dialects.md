# 02 — Bounded codecs and dialect tables

Execute this stage under the [execution contract](execution_contract.md).
Status: NOT STARTED. Run after 01's reviewed ledger and its S7 checker split, which must land before or with this
stage's first public header (canopen_shared 01 "Intake rules", seam S7). Everything here is verified on the host; no
owner, transport or hardware is involved.

## Work

1. Add `Types.h` and `Codec.h` under `include/MotorControlCAN/profiles/mks_servo_can/` with sources, and the generated
   private `Commands.h` from 01's generator. Types: `Dialect`, `CheckMode` (`SUM8`, `FIXED_6B`), `UplinkCheck`,
   `SlaveAddress` (1..2047), `GroupAddress` (1..0x7FF), `FunctionCode`, `ReplyPhase`, `StatusCode` with raw value
   retention. No catalogue strings in the codec link; keep descriptive names in the optional catalogue.
2. Frame shape: standard 11-bit data frames only, `extended`, `rtr` and `fd` false, DLC 2..8 (code plus check byte at
   minimum), built through `can/Frame.h` and `validateFrame` (MK109 p.30). Any other frame is refused before parsing.
   Broadcast ID 0 and group IDs are transmit destinations only; no reply is expected on them after V1.0.2 (MK109 p.1,
   p.30-31, p.49), so a frame received on them is foreign, never a reply.
3. Check byte: SUM-8 is `(ID + Byte1 + ... + Byte(n-1)) & 0xFF` (MK109 p.16, p.30; MK106 p.14). The manual never shows
   an ID above 0xFF; summing the full 11-bit ID or only its low byte gives the same result modulo 256 (inference), and
   12A confirms an ID above 0xFF. Fixed mode always writes 0x6B (MK109 p.26, p.48; MK109 dialect only). The uplink check
   in fixed mode is undocumented (MK-D08): `UplinkCheck` is a declaration with no default, and replies are not
   interpreted until it is declared. MK109A's no-verification mode (0FH, MK109A p.45) is never built.
4. Field encodings, big-endian (MK109 p.16), each with its own checked builder and parser and named range constants:
   - F6H and FDH: Byte2 bit 7 direction, bits 6-4 reserved and written zero, bits 3-0 with Byte3 a 12-bit speed
     (MK109 p.82, p.88); reject reserved bits on parse;
   - FEH, F4H, F5H: uint16 speed, uint8 acc, int24 two's complement position limited to the printed -8388607..+8388607
     (MK109 p.92, p.96, p.100); -8388608 is outside the documented range and is refused;
   - FDH relative pulses: uint24 magnitude 0..0xFFFFFF with the sign carried by the direction bit;
   - F6H run time uint24 in 10 ms units (MK109 p.82); 30H carry int32 plus value uint16 0..0x3FFF; 31H and 35H int48
     sign-extended; 33H and 39H int32; 32H int16 rpm; 38H uint16 mV and mA;
   - 98H four-byte time in ms: signedness is unknown (MK-D18), so values with bit 31 set are refused;
   - 94H four-byte Hm_offset: signedness undocumented, the same refusal applies; 84H value 256 has no documented
     encoding (MK-D29) and is refused.
5. Dialect tables, generated from the ledger and selected only by an explicit `Dialect` argument:
   - `V1_0_9_SEP2026` (MK109): 4CH, 38H, F7H 01, 50H documented (50H still never built); 0FH absent.
   - `V1_0_9_NOV2025` (MK109A): 0FH documented (never built); no 4CH, 38H, F7H 01 or 50H (MK-D01).
   - `V1_0_0` (MK100): work mode 3 is CR_CAN, speed 0..1600, acc 0..32, 8CH one byte, 39H int16 with 0..0xFFFF per
     turn, CW-positive speed and encoder (MK100 p.7, p.11-13, p.15, p.19, p.22-23); F6H, FDH, F4H, F5H and 91H exist,
     while FEH (1.0.4), 40H and other later commands are absent. The same example bytes `F6 01 40 02` mean code 320,
     600 rpm at 16 microsteps, under V1_0_0 and 320 rpm under the V1.0.9 tables (MK100 p.22, p.25; MK109 p.82).
   A builder for a code or form absent from the selected table returns `UNSUPPORTED` with no output change. Never
   derive the table from a version label, a 40H reply or a successful exchange.
6. Reply validation against an expectation (address, code, phase, dialect, uplink check, accepted lengths). Accept the
   layout-derived length, not the printed DLC: 39H DLC 6 for int32 in the V1.0.9 tables and DLC 4 int16 for V1_0_0;
   38H DLC 6; 40H DLC 6 with the firmware bytes kept raw (MK-D04..D06). Accept the documented variants per dialect: 3BH
   with one or two status bytes, F6H with 5 or 8 request bytes, F7H with or without 01, 91H with or without the mode
   byte (MK109 p.35, p.70, p.81-82; MK106 p.19). Other lengths fail with the raw bytes retained.
7. Status decoding is per code, phase and request kind (start, stop, setting): 0 fail, 1 start or success, 2 complete,
   3 stopped by limit, 5 latched for synchronization (MK109 p.83, p.89, p.93, p.97), 83H's 1 saved and 2 not saved
   (p.42), 80H's 0 calibrating, 1 success, 2 fail (p.41). An undocumented status value stays raw with execution
   unknown, never mapped to rejection or success.
8. Vectors: every vendor example in `mks_servo_can_vectors.json` builds and parses byte-exact, including group 0x50,
   broadcast 0 and the 83H checksum example (MK109 p.16, p.49, p.105). Malformed examples stay negative fixtures:
   nine-byte 8CH and F5H stop frames (MK109 p.47, p.103-104), F7H 01 printed with DLC 2 (p.81), RS485 frames with
   0xFA/0xFB headers (MK109 p.58; MK106 p.19, p.26). Recompute checksums by a separate method in the Python cross-check.

## Deliver

- `Types.h`, `Codec.h` and `src/profiles/mks_servo_can/Codec.cpp`; generated private `Commands.h`; Doxygen for every
  public declaration (units, ranges, dialect scope, output on failure, borrowed buffers).
- `test/mks_servo_can_codec_test.cpp` with inline vendor bytes, and the vector cross-check in
  `test/mks_servo_can_inventory_test.py`; registered in CTest and the repository checks.
- Root `CMakeLists.txt` `MOTORCONTROLCAN_SOURCES`, an `install(FILES ...)` rule for `profiles/mks_servo_can/`,
  `test/package_test.py` `HEADERS` and exact archive set; `library.json` and `scripts/check_repository.py` `EXPORTS`
  stay unchanged (contract 02 "Multi-profile consumers and package scope"); installed C++11 consumers for core only,
  Lichuan only, MKS codec only and both, proving no catalogue strings or vendor files are linked or packaged.
- Coverage and ledger `implemented`/`host_tested` updates; `docs/reports/mks_servo_can_02.md`; PROGRESS row.

## Acceptance

All vendor vectors pass, negative fixtures are rejected, every field boundary (minimum, maximum, one beyond, reserved
bits, sign extension) is tested, wrong-dialect builders refuse without output change, and parsing is atomic with raw
bytes retained on failure. No owner, admission or device claim exists yet. Quick and full verification, the family
checks and CI on the exact final pushed commit pass (new public API is major). Hardware NOT RUN. Commit and push.
