# 04b — CL2 operations

Execute this stage under the [execution contract](execution_contract.md).
Status: NOT STARTED.

Admit the four CL2 models to existing operations where CL2 pages document the behavior, using
[04a](04a_cl2_rows.md)'s mode-aware status decoder and CL2 observations and stage 01's model binding and
identity outcome. CL2 uses the C-family controlword list but a different statusword, homing table, loop-mode
code and input codes, so most C-family evidence does not transfer. Prompt 77 is consumed if landed.

## Work

1. Common admission, as in [03b](03b_tlc_operations.md) item 1: current `ModelBinding`, same-model reservation,
   identity outcome other than CONTRADICTED before a manufacturer-area download (the execution contract's
   vendor-object rule), a ledger row, and CL2
   evidence records (model and firmware equal to the current identity) wherever the C path uses measurements.
2. Enable. 6, 7, 15 at about 10 ms (CL257 p12). The C-family scope rule does not transfer: its bit 10 mask would
   hide a CL2 completion flag, and its released test reads `2100` bit4, whose polarity CL2 does not state
   (LX-D28). A CL2 scope rule uses bit3 alarm, the mode-aware bit 10 and `2100` bits 2 and 3, keeps bit4 raw, and
   is admitted only with a CL2 enable record. `EnableGate` stays `CLOSED` by default.
3. Stops. 15 to 271 stop, 15 to 11 emergency stop (CL257 p12): `StopKind::HALT` (modes 1/3) and
   `QUICK_STOP` for predecessor 15 only, HALT's mode-6 form on CL257 p13's bit 4 "1 to 0: interrupt homing".
   Other predecessors only from CL2 records; no CL2 motion without an admitted stop for its predecessors,
   apart from 03b's first-measurement plan record. Best-effort `000B` is unchanged with unknown effect (with
   seam S3 landed, set explicitly from the CL2 ledger review of CL257 p12, as in 03b). The statusword has no
   running bit, so stop completion stays `UNKNOWN` without CL2 stop evidence; `2100` bit2 is a controller
   report. `605D` (RW I16, CL257 p19) has no documented values and is never written.
4. Position. `6060`=1, `607A` -1000000..1000000 command pulses, `6081` 0..3000 rpm, `6083`/`6084` 0..2000 ms
   (CL257 p11, p19); triggers 15 to 31, 95, 63, 127 (CL257 p12). Completion is bit 10 in mode 1 through the
   mode-aware decoder, with fresh deassertion after the trigger and reassertion required as CL2 evidence (X08);
   a bit already high proves nothing. `2201` 400..51200 (CL257 p16) sets command pulses; the encoder field
   `2501` is not a scale (Q04 analog). Moves need CL2 `MoveEvidence`.
5. Velocity. CL257 p13: mode 3, write `60FF` and the ramps, then 6, 7, 15; enabling starts motion. This is one
   consistent CL2 claim, still unverified, so `prepareVelocity` refuses CL2 until a CL2 `VelocityEvidence`
   record exists. Mode 3 has no documented completion or zero-speed bit; settlement uses `606C` and `6064`
   observations under the existing rules, never bit 10.
6. Homing (X11). The CL2 appendix "servo homing methods" (CL257 p21..31) lists 1..14, 17..30, 33, 34 and 35 with
   CiA 402-style edge descriptions; 17/18 end at a limit falling edge and 24/29 at a home rising edge after
   reversals, unlike the C family (EN86 p13..14; LX-D26). Do not reuse `HomingMethod` with C meanings: add a CL2
   method table (proposed `Cl2HomingMethod`) with per-method prerequisites: negative or positive limit, home
   switch, start direction and index pulse. Index methods (1..14, 33, 34) require a caller-declared index
   signal: CL2-57C/86C document no Z source (CL257 p6), the ABZ drives list Z as optional (ABZ57 p6). Method 35
   (CL257 p31) is refused: it names `60E6` and `606C` as offset, which the dictionary does not support (LX-D27).
   Start on bit4 rising, interrupt on falling (CL257 p13). Completion is bit 10 in mode 6 with fresh
   transitions. `6099:01/02`, `609A`, `607C` per CL257 p13 (LX-D33 typos retained). Never disable a limit.
7. Alarm clear. Both routes are documented: `2301` (P0D-03, 0 none, 1 effective; CL257 p16) and `6040` bit 7
   then manual 0 (CL257 p14). The caller names the method; the C default does not transfer. A clear never
   claims the fault is gone: Err2/Err3 recover by themselves, Err5 needs power-off or manual reset, Err8/Err9
   concern the battery and Err9 is reset by P0D.07 without a CANopen index (CL257 p20; LX-D30, X13).
8. Position clear. `2302` (P0D-04; CL257 p14, p16) with the C-family stationary admission and invalidation.
9. I/O settings. A CL2 DI candidate covers `2400:01` bits 0..6 and `2400:02..08` codes 0..6 with CL2 meanings
   (code 4 motor release); codes 7..22 are refused as undescribed (LX-D25). Output polarity `2400:0C` bits 0..2
   and functions `0D..0F` codes 0..5, 7, 9..11 follow CL2's one documented layout (CL257 p10, p17) and may be
   implemented with explicit caller intent; codes 5 (brake) and 7 (motor enable) also need a caller
   acknowledgement of the external load. `60FE` output command stays gated: only "both bits set gives output"
   is documented (CL257 p10), not clearing or masking.
10. Tuning. CL2 loop-mode setter with codes 0, 1, 3; 2 is refused (LX-D22). `2501` is never written (X10).
    `2502` within 0..7000 or 0..8000 mA by model (04a item 7) as a native value; the motor's rating remains the
    caller's limit (X14). `2503..2519` bounded native setters where CL257 p17..18 print a range; others refuse.
11. Soft limits and save. `2202` (P05-0F) 0/1 and `607D` as the C family documents them. `2300` save and
    factory restore (P0D-02; CL257 p14, p16) are documented; CL2 bitrate is DIP-only (CL257 p8), so no route
    change follows a save. CL257 p14 names homing speed, ramps and homing mode as typical saved settings, not a
    complete list: the C-family persistence uncertainty applies. No implicit save.
12. Motor matching (P02.19, P00.00, P00.03, P00.12, P00.13; CL257 p10, LX-D29) is not exposed.

## Decisions that do not transfer here

`enableScopeHolds` facts, `MONITOR_RELEASED`, `STATUS_EN86_ZERO_SPEED`, mode-6 completion on bit 8,
`HomingMethod` trajectories, `InputFunction::ENABLE_MF` for code 4, `LoopMode::CLOSED_LOOP`=2, DEC-8
predecessors, DEC-2's measured premise, the `AlarmClearMethod` default, FF01/FF05 decoding, Q04 scale, S21.

## Deliver

CL2 admission in `Actions`, `Position`, `Velocity`, `Homing`, `Settings`, `Tuning` and `Persistence`; the CL2
homing table and its admission; `test/cl2_operations_test.cpp` through the real owner and `LichuanCl2Drive.h`:
bit 10 completion only with fresh transitions, a pre-high bit 10, mode 3 without completion, method refusals
(index without a declared Z, 35, C-family meaning of 17), code 4 release, `2500`=2 refused, `2501` never
written, `605D` never written, both alarm-clear routes, save without route effect, CL86-C evidence refused.
Ledger, coverage, `docs/api_cli_mapping.md`, CHANGELOG, report and `PROGRESS.md`.

## Acceptance

Each CL2 operation is admitted with cited CL2 pages and a CL2 evidence input, or refused before TX with its
X gate. No C-family meaning of bit 10, method numbers or input code 4 reaches a CL2 binding. Quick and full
verification, firmware builds and exact-commit CI pass. Hardware NOT RUN; X08, X11, X12 and X14 stay open.
