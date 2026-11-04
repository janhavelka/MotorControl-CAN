# Packet 14: control, current and tuning groups

Resumes prompt 22. When this packet closed, host and fake evidence only. **Of the 33 rows, only the existing 2500 loop selector has a typed write. No specimen has accepted it.** T0, the read of 2502..2520, ran on both specimens on 2026-11-04 ([results](#bench-2026-11-04-t0)); T1 and T2 have not run.

## Delivered

See the [tuning guide](../../tuning.md) and the [changelog](../../../CHANGELOG.md).

- **The 2500 write, verified through the real owner, found no defect.** Literal frames for every transition, both models and stored words `0, 1, 2, 3, 0x1234, 0xffff`; no-op silence; whole-candidate refusal; frozen tokens and aliases; immutable queued bytes; aborted and lost downloads with no replay; aborted, lost and mismatched readback at each word; cancellation; deadlines; boot-up, bus-off and EMCY; notice before TX; retirement before epoch migration; and that no other word or object is written. The unchanged 2501 stays unchanged.
- Every prompt 22 ledger row carries `review.next_evidence`, and the checker refuses a row without one.
- 21 hand mutants of the control logic are killed (not retained).

## Contract changes, both project policy

- `prepareControlSettings` takes a clean STATUS and MONITORS read made under the reservation, and refuses on a failed or uncertain read, or 2100 running, alarm or undocumented bits 7..15. No source gives a state rule for 2500. It matches 2202, and RS refuses its control settings unless stationary with no alarm (`validatePriorSettingsAndStandstill`). It is not a "disabled" requirement.
- `CLOSED_LOOP` is refused while the stored 2501 reads 0, from RS `ZERO_ENCODER_SCALE`.

## Decisions per subgroup

Pages are PDF pages. EDS: all U16 RW, PDO-mappable, with no default or limits.

| Rows | Decision | Next concrete evidence |
| --- | --- | --- |
| 2500 | **Typed write.** EN86 and ZH86 p19: 0 noneffective, 1 open, 2 closed. OL86-C has no encoder (p3), so closed loop is refused for it. An ACK means the register accepted the value, not that the loop changed. Selector 0 is not disable, release or stop, and 2500 is not 6060 | Bench register acceptance; manufacturer activation timing and what 0 does |
| 2501 | Gate. p19 gives no unit or range. 4000 equals 4 × 1000 lines (p3) only by inference | Manufacturer unit and range, or `6064` counts over one independently measured revolution |
| 2502 | Gate. p19 says peak, mA, no range. 8 A is the drive's ceiling, not a setting, and not the LC60H2127's rating | Manufacturer range and peak or RMS convention; the motor's rated phase current |
| 2503..2505 | Gate. Percent with no base, range or ordering | Percent-base formula and ranges, then an intermediate-current check across the write order |
| 2506, 2507 | Gate. "Locking" ratio and time, EN p19, ZH p20, with no range or zero meaning | Manufacturer meaning, then a timed current measurement at standstill |
| 2508 | Gate. No code set | Manufacturer code set; no record has read this word |
| 2509, 250D | Gate. No equation; the EDS calls 250D a time, the manuals a coefficient | Manufacturer equations and units |
| 250A, 250B | Gate. No scale. These are settings, not measured following error | Unit, range and which report each drives |
| 250C | Gate. No time unit | Unit, then a timed in-position report |
| 250E..2519 | Gate. "Factory default, adjustment not required generally" | Domain, scale, ordering, and whether the product page's auto-tuning rewrites them |
| 251A..2520 | Gate. EDS only, in none of the five manuals | A manufacturer document defining them |

A correction: "8 A has no peak or RMS qualification" holds only for the EN86/ZH86 p3 table. `CL86-C_manufacturer_product.html` lists "Maximum peak current: 8A" among its features, while its own specification table says "Maximum Output Current 8A" with no peak label, and its supply ranges (DC30..110 V, AC20..80 V) contradict p3. So it describes another revision, and its peak claim is not evidence for this drive. It also lists "Motor parameter auto-tuning", which no manual or object describes.

**Recorded delta from RS.** RS refuses every control write, the algorithm selector included, without `CONTROL_EFFECTS_REQUIRED` qualification and a qualified current tuple (`DriverSettings.cpp:366-433`). CAN's 2500 write has no such gate, and the attended T1 is its only evidence gate. Both specimens report the enabled pattern `6041=0x8037` with 2100 bit 4 clear, and OP86 p5 note 4 says the motor is shaft-locked after power-up, though OP86 repeats the 57C manual there. The guide and `Tuning.h` carry this warning.

Read and write ownership is unchanged and pinned: `1003`, and `1005`, `1014`, `1017`, `1200:01..02` reads stay with 19, their writes with 38; `1200:00` with 13; PDO records with 38, 17, 18 and 39.

## Independent review, fixed

A reviewer rendered the pages and probed the code. The source text, page boundaries, gates and ownership pins held, and T1's bytes are a correct expedited download. It found a stale 2500 ledger prerequisite, a ledger claim that CL86-C accepts the enum, missing citations, a wrong count, contradictory selector-0 wording, an unconditional no-op claim, no energized-drive warning where users read, and a T1 plan whose abort condition a supply ammeter cannot measure. All fixed here.

## Bench, 2026-11-04: T0

The `readiness-tuning` route (`TU`) read all 31 words, with an accepted image before and after, on node 1 ([309](../../bench/20261104T1313Z-tuning-node1-309/rx.txt)) and node 2 ([310](../../bench/20261104T1313Z-tuning-node2-310/rx.txt)), each upload under its own one-use grant. Both nodes read the same image, in decimal: `2502=8400`, `2503=100`, `2504=40`, `2505=100`, `2506=40`, `2507=200`, `2508=0`, `2509=5`, `250A=4000`, `250B=20`, `250C=10`, `250D=512`, `250E=6144`, `250F=1228`, `2510=40`, `2511=20`, `2512=2560`, `2513=114`, `2514=25`, `2515=2560`, `2516=1024`, `2517=100`, `2518=20`, `2519=4`, `251A=12001`, `251B=1000`, `251C=1024`, `251D=1228`, `251E=8`, `251F=0`, `2520=0`. These are raw words: no unit, scale or domain follows from them, and every gate in the table above stands. The read effects stay unknown. A first attempt (305, 306) was refused `session`, because the `60FE:00` abort of packet 13 P1 had left both routes unresolved. See the [bench report](../2026-11-04_bench.md).

**Still owed:** T1 and T2. `2502=8400` does not open T1: its go condition compares 2502, read as a peak current, with the LC60H2127's rated phase current, which is still not established, and only a phase-current probe can watch the abort condition. T1 has no bench route.

## The planned trials

Written before the routes existed.

- **T0, read only.** Typed reads of 2502..2520, node 1 then node 2. These words are outside the accepted read list, so the read path needs its own review first.
- **T1, 2500 register acceptance, node 1.** `601: 2B 00 25 00 01 00 00 00`, readback of 2500 and 2501, then the return `2B 00 25 00 02 00 00 00` as a separate operation. **Selecting open loop may push current into a drive that reads as enabled**, and may snap the rotor.
  - Go only if T0's 2502, read as a peak current, does not exceed the LC60H2127's rated phase current from its nameplate or datasheet. If the rating is unknown, do not run.
  - An unloaded shaft, the independent cutoff in reach, one drive, `6061` read as 0 first, and the full readiness image before and after.
  - **Only a current probe on one motor phase** can detect a rise against a phase rating; a supply ammeter cannot, because at standstill supply current is far below phase current.
  - Watch phase current and the shaft for at least the 2507 locking time T0 read, after both the forward write and the return. Whether 2507 is a delay or a duration is unknown, so cover the longer reading.
  - Abort on any current rise above the rating, movement, readiness change, EMCY, bus fault, uncertain terminal or boot-up, and report that 2500 may be left at 1.
  - If 2501 or the state changes, or an alarm appears, the library refuses the return, and the drive stays in open loop. Report that state; do not force the return.
  - Persistence across a power cycle is unknown, and nothing is saved. A power cycle is not a restore.
- **T2.** T1 on node 2, after T1 passes.

No write to selector 0 or to 2501..2520: no domain is known.

## Sibling refresh

| Sibling | Revision | Dirty | Adopted | Rejected |
| --- | --- | --- | --- | --- |
| MotorControl-RS | `6499571` | Clean | Standstill and no-alarm prerequisite; the zero-encoder rule. RS's effects and current-tuple gate is recorded above as a delta | `legalControl` and `tuningParameterInfo` ESS domains; `maximumEffectiveCurrentMa`; the `encoderScaleUsable` flag |
| MB85RC | `1a45e88` | Clean | `_selectVariant`: exact model, unknown refused | Auto-detection |
| INA228, ADS1115 | `73f9548`, `d613a13` | Clean | Whole-configuration validation; lowered trust keeps evidence | Driver-object flag, auto-recovery |
| FieldCore-node | `5ad9292` | Clean | `SettingsService::buildCandidate` matches the whole-candidate approach | No CAN tuning consumer |

## Open

OL86-C might abort the 2501 read, which would block every loop write on that model; no OL86-C specimen exists to check.

## Checks

See the [execution table](README.md).
