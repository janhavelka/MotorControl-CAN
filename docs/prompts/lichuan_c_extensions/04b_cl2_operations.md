# 04b — CL2 operations

Execute this stage under the [execution contract](execution_contract.md).
Status: NOT STARTED.

Admit the four CL2 models to existing operations where CL2 pages document the behavior, using
[04a](04a_cl2_rows.md)'s mode-aware status decoder and CL2 observations and stage 01's model binding and
identity outcome. CL2 uses the C-family controlword list but a different statusword, homing table, loop-mode
code and input codes, so most C-family evidence does not transfer. Prompt 77 is consumed if landed.

## Work

1. Common admission, exactly as in [03b](03b_tlc_operations.md) item 1 with CL2 in place of TL-C: current
   `ModelBinding`, same-model reservation, a ledger row and disposition-map cell, prompt 57's identity rules
   rechecked at every changing step; evidence records of the CL2 model whose firmware differences are
   reported and never refuse, while a missing behavioral fact refuses with its own gate (X15, X16, X11); and
   03b's observation source, with the owner's CL2 read acceptance (04a item 3) required before any polling
   and samples only from the mode-aware decoder of 04a item 4.
2. Enable. 6, 7, 15 at about 10 ms (CL257 p12). The C-family scope rule does not transfer: its bit 10 mask would
   hide a CL2 completion flag, and its released test reads `2100` bit4, whose polarity CL2 does not state
   (LX-D28). A CL2 scope rule uses bit3 alarm, the mode-aware bit 10 and `2100` bits 2 and 3, keeps bit4 raw,
   with a CL2 source-state table (canopen_shared 02 "Operation admission", state transitions). `EnableGate`
   stays `CLOSED` by default; as in 03b item 2, `ATTENDED_EXPERIMENT` under its existing terms (07a K5's
   route, mode 1, targets equal to the actual position) and prompt 57's measured gate with a CL2 enable
   record (X15) admit it.
3. Stops. 15 to 271 stop, 15 to 11 emergency stop (CL257 p12): `StopKind::HALT` (modes 1/3) and
   `QUICK_STOP` for predecessor 15 only, HALT's mode-6 form on CL257 p13's bit 4 "1 to 0: interrupt homing".
   Other predecessors only from CL2 records (X15); no CL2 motion without an admitted stop for its
   predecessors, with no first-measurement exception. Best-effort `000B` is unchanged with unknown effect
   (with seam S3 landed, set explicitly from the CL2 ledger review of CL257 p12, as in 03b). The statusword has
   no running bit, so stop completion stays `UNKNOWN` without CL2 stop evidence (X15); `2100` bit2 is a
   controller report. `605D` (RW I16, CL257 p19) has no documented values and is never written.
4. Position. `6060`=1, `607A` -1000000..1000000 command pulses, `6081` 0..3000 rpm, `6083`/`6084` 0..2000 ms
   (CL257 p11, p19); triggers 15 to 31, 95, 63, 127 (CL257 p12). Completion is bit 10 in mode 1 through the
   mode-aware decoder and item 1's observation source, with fresh deassertion after the trigger and
   reassertion required as CL2 evidence (X08); a bit already high proves nothing. `2201` 400..51200 (CL257
   p16) sets command pulses; the encoder field `2501` is not a scale (Q04 analog). Moves need CL2
   `MoveEvidence` (X16).
5. Velocity. CL257 p13: mode 3, write `60FF` and the ramps, then 6, 7, 15; enabling starts motion. This is one
   consistent CL2 claim, still unverified, so `prepareVelocity` refuses CL2 until a CL2 `VelocityEvidence`
   record exists (X16). Mode 3 has no documented completion or zero-speed bit; settlement uses `606C` and `6064`
   observations under the existing rules, never bit 10.
6. Homing (X11). The CL2 appendix "servo homing methods" (CL257 p21..31) lists 1..14, 17..30, 33, 34 and 35 with
   CiA 402-style edge descriptions; 17/18 end at a limit falling edge and 24/29 at a home rising edge after
   reversals, unlike the C family (EN86 p13..14; LX-D26). Do not reuse `HomingMethod` with C meanings: add a CL2
   method table (proposed `Cl2HomingMethod`) with per-method prerequisites: negative or positive limit, home
   switch, start direction and index pulse. Index methods (1..14, 33, 34) are refused on CL2-57C and CL2-86C
   even with a caller declaration: their encoder connector documents no Z source (CL257 p6 offers ABZ wiring
   only on request; LX-D26, LX-D34), and a declaration cannot show that the drive variant exposes or uses one;
   they stay refused until applicable source evidence and X11's qualification exist. On CL257C-ABZ and
   CL286C-ABZ, where Z may be left unconnected (ABZ57 p6), an index method needs the caller's fixture record
   that Z is wired on that drive. Every method checks its switch and start-direction prerequisites before
   setup or trigger, and a refusal sends nothing. Method 35 (CL257 p31) is refused: it names `60E6` and
   `606C` as offset, which the dictionary does not support (LX-D27). Start on bit4 rising, interrupt on
   falling (CL257 p13). Completion is bit 10 in mode 6 with fresh transitions from item 1's observation
   source. `6099:01/02`, `609A`, `607C` per CL257 p13 (LX-D33 typos retained). Never disable a limit.
7. Alarm clear. Both routes are documented: `2301` (P0D-03, 0 none, 1 effective; CL257 p16) and `6040` bit 7
   then manual 0 (CL257 p14). The caller names the method; the C default does not transfer. A clear never
   claims the fault is gone: Err2/Err3 recover by themselves, Err5 needs power-off or manual reset, Err8/Err9
   concern the battery and Err9 is reset by P0D.07 without a CANopen index (CL257 p20; LX-D30, X13).
8. Position clear. `2302` (P0D-04; CL257 p14, p16) with the C-family reference invalidation and a stationary
   admission over CL2 observations (`2100` bit2 running and bit3 alarm clear, `606C` zero) from one explicit
   fresh state read through the model-taking getter; a C-family or TL-C sample is refused.
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
    factory restore (P0D-02; CL257 p14, p16) are documented, and neither sends a node or bitrate write. That
    is all the library can state: the node is DIP-selected except all OFF, labelled "custom" with its source
    undocumented (04a item 10, LX-D32), and CL257 p14 names homing speed, ramps and homing mode as typical
    saved settings, not a complete list. So a save or restore result, on success and on a lost
    acknowledgement, keeps "route after restart unverified" (X18); a restart invalidates dependent evidence,
    and the next session reacquires explicitly on a declared route with no replay. The C-family persistence
    uncertainty applies. No implicit save.
12. Motor matching (P02.19, P00.00, P00.03, P00.12, P00.13; CL257 p10, LX-D29) is not exposed.
13. Release and mode select, as 03b item 12 with CL2 pages: before prompt 57 lands both are NOT_IMPLEMENTED;
    after it, release stays GATED under X15 and `prepareModeSelect` admits only CL257's modes 1, 3 and 6 (CL257
    p11..13) with a CL2 source-state table, otherwise GATED under X15.

## Decisions that do not transfer here

`enableScopeHolds` facts, `MONITOR_RELEASED`, `STATUS_EN86_ZERO_SPEED`, mode-6 completion on bit 8,
`HomingMethod` trajectories, `InputFunction::ENABLE_MF` for code 4, `LoopMode::CLOSED_LOOP`=2, DEC-8
predecessors, DEC-2's measured premise, the `AlarmClearMethod` default, FF01/FF05 decoding, Q04 scale, S21,
the release table's CL86-C rows and the existing `sameFirmware` refusal.

## Deliver

CL2 admission in `Actions`, `Position`, `Velocity`, `Homing`, `Settings`, `Tuning` and `Persistence`; the CL2
homing table and its admission; `test/cl2_operations_test.cpp` through the real owner and `LichuanCl2Drive.h`:
bit 10 completion only with fresh transitions, a pre-high bit 10, mode 3 without completion, a mode or
generation change between samples, a stale or other-model sample, a missing transition, a fault, no read
acceptance (completion `UNKNOWN`) and expiry of the one operation deadline, each with no inferred standstill
and no later trigger; method refusals before setup or trigger with no TX (index on CL2-57C/86C with and
without a declared Z, index on ABZ without the Z fixture record, a missing switch or start-direction
prerequisite, 35, C-family meaning of 17); code 4 release, `2500`=2 refused, `2501` never written, `605D`
never written, both alarm-clear routes; save and restore sending no node or bitrate write and keeping the
route-after-restart obligation on success and on a lost acknowledgement; CL86-C evidence refused; the paired
firmware-only and missing-fact evidence tests of 03b. Ledger, coverage, `docs/api_cli_mapping.md`,
CHANGELOG, report and `PROGRESS.md`.

## Acceptance

Each CL2 operation is admitted with cited CL2 pages and a CL2 evidence input, or refused before TX with its
disposition from stage 02's map. No C-family meaning of bit 10, method numbers or input code 4 reaches a CL2
binding, and no firmware difference refuses one. Quick and full verification, firmware builds and
exact-commit CI pass. Hardware NOT RUN; X08, X11, X12, X14, X15, X16 and X18 stay open.
