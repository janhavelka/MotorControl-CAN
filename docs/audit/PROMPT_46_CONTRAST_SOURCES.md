# Prompt 46 contrast source review

Reviewed on 2026-11-15. These are documentary inputs to host contract fixtures,
not implemented N5, EPOS4, VESC, ODrive or moteus drivers, live configuration,
or hardware qualification. Original retained sources and provenance remain in
[drive contrasts](../reference/drive_contrasts.md) and its linked manifests.

## N5

Reopened the retained `nanotec_n5_canopen_v3.3.0.pdf`, version 3.3.0,
FIR-v2213. Page numbers below are one-based PDF pages. Inspected rendered pages
65 and 297 because extraction reorders formula operands and bit labels.
Its checked SHA-256 is
`4c60e1415c3f2421a010c57d3fd050b49bed87606a0fda769dd874052867ea41`.

| Fixture fact | Primary evidence and disposition |
| --- | --- |
| Motor, feedback and control mode are separate facts | Pages 38, 297–298: `3202h:00` is a U32 RW RX-PDO bitfield. Bit 6 selects BLDC; bit 0 selects closed/open loop; bit 3 enables current reduction in open loop. `0x08` and `0x40` are **p38 example values for the `3202h` bitfield**, not profile identifiers or a complete motor enumeration. Neither enters `Endpoint.profile`. |
| Constrained combinations | Pages 297–298: changing bit 0 is prohibited while Operation enabled; slow-speed bit 7 requires closed loop; current reduction bit 3 applies in open loop; auto-alignment bit 4 concerns closed-loop operation and can move the rotor. These are not unconstrained interchangeable settings. No fixture writes them. |
| Position scale without full-step geometry | Pages 62–64: explicitly selected `60A8h=0xFF410000` uses tenths of a degree; 3600 commands per motor turn only with 1:1 gearing. This configuration, not the motor technology, supplies command scale. |
| Acceleration dimension | Pages 64–65, 376: `6083h/6084h` are U32 acceleration/deceleration, with speed units from `60A9h` per second and explicit `6097h:01/:02` factor. The rendered formula is numerator 01 divided by denominator 02, agreeing with prose; text extraction reverses its order. Fixtures use explicit 1:1 and 3:2 factors. The latter normalizes selected raw 4 to exact 6 rpm/s before production conversion to 36 degrees/s²; no raw object decoder is claimed. |
| Real unresolved document error | Page 64 defines `60A9h` time in bits 8–15 and exponent in bits 24–31. Page 65's parenthetical bit descriptions swap these fields for its examples. The encoded example values and page 64 layout remain distinguishable from those incorrect parentheses. No live unit decoder or profile implementation is inferred. |
| Width versus velocity capability | Page 349: `6042h:00` is I16 RW RX-PDO, used only as a checked wire-width rejection case. Page 412: `60FFh:00` is I32 RW RX-PDO; page 364: `606Ch:00` is I32 RO TX-PDO. A wide signed host request does not qualify an actual operating range. |
| Persistence index collision | Page 283: `2300h:00` is U32 RW RX-PDO NanoJ Control; bit 0 controls program execution. Pages 169–172 and 176: `1010h` uses `0x65766173` and `1011h` uses `0x64616F6C`; selected subindices determine scope. These meanings cannot select Lichuan persistence. |
| Resource scope | Page 127 identifies mappings `1400h–1407h`, `1600h–1607h`, `1800h–1807h`, `1A00h–1A07h`, each with up to eight payload bytes. Pages 192–193 enumerate eight mapping slots. The four-PDO Lichuan profile limit is not a common axis limit. |
| Holding brake | Page 107 describes automatic/manual brake output configuration. It is distinct from electrical braking and does not establish the abstract C11 capability combinations as N5 variants. |

## EPOS4

Reopened the [official rel13740 URL](https://www.maxongroup.com/medias/sys_master/root/9579215585310/EPOS4-Firmware-Specification-En.pdf)
through the web reader on 2026-11-15; it exposes edition 2026-07, 310 pages.
Direct local downloads returned HTTP 403. Independently reopened the existing
disposable cached PDF and copied it into `.build/p46-contrasts` for inspection.
SHA-256 of the bytes actually read:
`66fe32f15ab0bcbf9a008d0d0da8ffe395064c6716edcb3f3b28907f7160e64f`.
This matches the earlier recorded audit hash; it is not a claim that a new local
download succeeded. The PDF remains outside Git and installed artifacts.

Pages 230 and 236 establish U32 acceleration and rpm/s (`60AAh=0x00C00300`);
page 235 allows decimal rpm velocity scales. Page 256 defines `60FFh` as I32.
Pages 71–72 define `1010h:01` save and `1011h:01` restore signatures. The
dictionary overview and full extracted document contain no `2300`; absence is
scoped to rel13740. Page 193 distinguishes holding brakes from load braking and
requires relevant sensor/configuration evidence. None establishes a supported
EPOS4 product or selected firmware.

## Other CAN protocols

Reopened the original pinned files listed in
[`other_can/SOURCES.json`](../reference/other_can/SOURCES.json).

| Contrast | Primary source and reviewed boundary |
| --- | --- |
| VESC C5 | `vesc_comm_can.md`, Frame Format and Single-Frame Commands: extended identifier `0x0117`, four data bytes `00 00 C7 38`, signed big-endian scale 1000. Numeric ID fitting 11 bits does not permit standard format on that selected route. No applied-current or acknowledgement claim follows. |
| ODrive C6 | CANSimple 0.6.12 overview and Get_Encoder_Estimates: command `0x09`, standard RTR request, no data; an empty data-frame request is also documented but susceptible to collision with the response. Response contains two little-endian float32 values. RxSdo/TxSdo names are not CANopen services. |
| moteus C3/C7 | `moteus_can.md` defines little-endian IEEE binary32 and FD transport, with reply-request identifier `0x8001`. `Frame` cannot contain the documented 16-byte request. The fixture tests the FD requirement flag only; it does not pretend truncated bytes are a request. The retained prose's extended-ID threshold error remains documented in the contrast ledger. |
| C8 | `moteus_registers.md`, register `0x00b`, reports trajectory completion while following final velocity. A nonzero terminal velocity therefore does not contradict native trajectory-done, and done does not establish standstill. The no-acknowledgement setter is an abstract invariant, not an invented moteus setter implementation. |
| C11 | `moteus_registers.md` mode 15 and VESC current-brake commands describe electrical braking; N5 p107 and EPOS4 p193 describe holding-brake configuration. Abstract absent/unknown capability and calibration cases must preserve this distinction. |

The reviewed common seam covers explicit host units, route identity, bounded
Classical frame representation and separate evidence. It does not demonstrate
that every future protocol, feedback system, ramp or drive fits unchanged.

The Lichuan comparison was checked against original EN86 PDF pages 11 and 18
and `LC_StepDriverCAN_V1.1.eds`: `6083/6084` are millisecond ramp times;
`2300:00` is U16 parameter operation (1 factory restore, 2 save), and the EDS
does not declare `1010/1011`. No save signature or NanoJ meaning transfers to it.
