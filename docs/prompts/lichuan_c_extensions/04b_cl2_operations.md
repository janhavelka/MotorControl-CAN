# 04b — CL2 operations

Execute this stage under the [execution contract](execution_contract.md).
Status: NOT STARTED.

Admit the four CL2 models to existing operations where CL2 pages document the behavior, using
[04a](04a_cl2_rows.md)'s mode-aware status decoder and CL2 observations and stage 01's model binding and
identity outcome. CL2 uses the C-family controlword list but a different statusword, homing table, loop-mode
code and input codes, so most C-family evidence does not transfer. Prompt 77 is consumed if landed. Items
1, 3, 4, 5, 6, 8, 10 and 13 consume rules that 03b items 1, 3, 4, 5, 6, 8, 10 and 12 define (the halt-state
derivation of item 3, the move interruption choice of item 4, the declared stop's fallback deadline, the
stop obligation and the homing fixture signal record of items 5 and 6, item 8's retained
last-motion terminal input and rechecks before each write, item 10's caller peak-current record and its
refusals among them); whichever of 03b and 04b runs first lands their shared parts for both groups with
their tests (03b introduction).

## Work

1. Common admission, exactly as in [03b](03b_tlc_operations.md) item 1 with CL2 in place of TL-C: current
   `ModelBinding`, same-model reservation, a ledger row and disposition-map cell, prompt 57's "Admission by
   outcome" rechecked at every changing step with its stop exemption for item 3's reviewed stops; evidence
   records of the CL2 model whose firmware differences are
   reported and never refuse, while a missing behavioral fact refuses with its own gate (X15, X16, X11); and
   03b's observation source, with the owner's CL2 read acceptance (04a item 3) required before any polling
   and samples only from the mode-aware decoder's bracketed acquisitions of 04a item 4.
2. Enable. 6, 7, 15 at about 10 ms (CL257 p12). The C-family scope rule does not transfer: its bit 10 mask would
   hide a CL2 completion flag, and its released test reads `2100` bit4, whose polarity CL2 does not state
   (LX-D28). A CL2 scope rule uses bit3 alarm, the mode-aware bit 10 and `2100` bits 2 and 3, keeps bit4 raw,
   with a CL2 source-state table (canopen_shared 02 "Operation admission", state transitions). Its rows and
   its end after word 15 are 03b item 2's: CL257 p12 prints the same pairing (6040=0 with 000, 6 with 001,
   7 with 011, 15 with 111), the low bits come from 04a item 4's bracketed acquisition with both `6061`
   replies in the gate's mode, and `2100` bit2 running stands in for bit14. `EnableGate`
   stays `CLOSED` by default; as in 03b item 2, `ATTENDED_EXPERIMENT` under its existing terms (07a K5's
   route, mode 1, the target at the actual position, observed and never written) and prompt 57's measured
   gate with a CL2 enable record (X15) admit it. Raw `607A` equal to `6064` is a screen only (CL257 p11 and
   p19 give `6064` no unit, so command pulses or encoder counts are undocumented): the attended route also
   needs 03b item 2's caller basis record for the CL2 fixture, otherwise it refuses with
   MISSING_PREREQUISITE and zero TX. CL2 models have encoders (CL257 p4, p10; ABZ57 p4), but no CL2 page
   says that `6064` reports them, so unless the basis record shows that, the route also needs 03b item 2's
   actual-position record and refuses the same way without it.
3. Stops. 15 to 271 stop, 15 to 11 emergency stop (CL257 p12): `StopKind::HALT` (modes 1/3) and
   `QUICK_STOP` for predecessor 15 only, HALT's mode-6 form on CL257 p13's bit 4 "1 to 0: interrupt homing".
   Other predecessors only from CL2 records (X15); no CL2 motion without an admitted stop for its
   predecessors, with no first-measurement exception. Halted and unknown-halt predecessors follow 03b item
   3: CL257 p12 gives controlword bit 8 no meaning other than the 271 stop, so HALT stays `010F`, the quick
   stop goes out as `010B` with its undocumented status recorded, and no stop is refused merely for an
   unknown halt state. The halt state at dispatch follows 03b item 3's derivation from the words actually
   sent (a declared external writer making it unknown); no CL2 status bit shows a halt state (04a item 4),
   and no CL2 source states that a `6040` upload returns the received word. CL257 p12 documents 15 to 11
   only, so the best-effort `000B` is denied for CL2 exactly
   as 03b item 3 denies it for TL-C, until a reviewed CL2 decision covers an unknown predecessor (X15), at
   both levels: S3's explicit no shape on every CL2 binding before its first traffic, kept across rebind
   (04a item 3, landed by 03a item 2 or 04a), so a direct `admitBestEffortStop` returns `INVALID` with no
   TX, and the `prepareBestEffortStop` refusal. The same tests apply. The
   statusword has no running bit, so stop completion stays `UNKNOWN` without CL2 stop evidence (X15); `2100` bit2 is a
   controller report. `605D` (RW I16, CL257 p19) has no documented values and is never written.
4. Position. `6060`=1, `607A` -1000000..1000000 command pulses, `6081` 0..3000 rpm, `6083`/`6084` 0..2000 ms
   (CL257 p11, p19); triggers 15 to 31, 95, 63, 127 (CL257 p12). Completion is bit 10 in mode 1 through the
   mode-aware decoder's bracketed acquisitions and item 1's observation source, with fresh deassertion
   after the trigger and reassertion required as CL2 evidence (X08); a bit already high proves nothing.
   `2201` 400..51200 (CL257 p16) sets command pulses; the encoder field `2501` is not a scale (Q04 analog).
   Moves need CL2 `MoveEvidence` (X16) and take 03b item 4's interruption choice; CL2 has no S3 shape
   (item 3), so an interruptible move needs the application-declared independent stop with its fallback
   deadline.
5. Velocity. CL257 p13: mode 3, write `60FF` and the ramps, then 6, 7, 15; enabling starts motion. This is one
   consistent CL2 claim, still unverified, so `prepareVelocity` refuses CL2 until a CL2 `VelocityEvidence`
   record exists (X16). Mode 3 has no documented completion or zero-speed bit; settlement uses `606C` and `6064`
   observations under the existing rules, never bit 10. An admitted run follows 03b item 5's timed-run rule
   (canopen_shared 02 "Operation admission"), with item 3's stop table and, since CL2 has no S3 shape, an
   application-declared independent stop named in the result with its fallback deadline, without which
   preparation refuses; the application invokes it as 03b item 5 states (its elapsed-deadline and
   late-service clauses and the stop obligation that blocks rebinding and re-declaration included), and
   until X15's CL2 stop record and read acceptance exist every run that reaches stop due ends with it
   invoked.
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
   setup or trigger against 03b item 6's fixture signal record and one fresh read of `2400:01..08` and
   `2101` (CL2 codes 1 origin, 2 positive limit, 3 negative limit, CL257 p10), and a refusal sends nothing. Method 35 (CL257 p31) is refused: it names `60E6` and
   `606C` as offset, which the dictionary does not support (LX-D27). Start on bit4 rising, interrupt on
   falling (CL257 p13). Completion is bit 10 in mode 6 with fresh transitions from item 1's observation
   source. `6099:01/02`, `609A`, `607C` per CL257 p13 (LX-D33 typos retained). Never disable a limit.
   Write preflight (canopen_shared 02 "Write preflight"): before any homing setup download, every write the
   home may issue (`6098`, `6099:01/02`, `609A`, `607C` and its `6040` words; the C path changes no mode)
   needs a reviewed CL2 write-policy row for the bound model, `6098:00` with 04a item 11's narrow
   disposition among them; a
   missing or unresolved row blocks the whole home with nothing sent, and method or fixture evidence or a
   fake server's acceptance never stands in for it.
   Each admitted method also needs a CL2 homing record for that model and method (X11), as TL-C needs X17's:
   its validated signal sequence, its search bound and the predecessors its interrupt admits. A page
   description and the prerequisite checks are not that record, and without it the method is GATED.
   Interruption follows 03b item 6 (read acceptance, the caller's latest interruption time with the stop
   budget before the deadline, an optional travel bound acted on only from observations of bounded age, the
   transfer cap at stop due and a declared independent stop with its fallback deadline, all before TX; at
   the latest interruption time, on a fresh observation past the travel bound, or when observation is lost
   or expires after a possibly sent trigger, the application requests the bit-4-falling HALT as the
   separate urgent stop; otherwise `needsStop`, motion and reference stay unknown, and the application
   invokes the declared stop under 03b item 6's fallback rule, an early interruption with an upload
   pending included). No CL2 source documents a host-loss
   reaction (LX review, "Limits and protection"), so the drive is not assumed to stop by itself.
7. Alarm clear, with a CL2 source-state table (canopen_shared 02 "State transitions"). Both routes are
   documented: `2301` (P0D-03, 0 none, 1 effective; CL257 p16) and `6040` bit 7 then manual 0 (CL257 p14;
   15 to 128 on p12). The caller names the method; the C default does not transfer. CL257 p20 gives each
   fault its reset: only Err5 (motor position deviation) lists a manual reset, Err2/Err3 recover by
   themselves, the others need a power-off, and Err9 also needs P0D.07, which has no CANopen index. `603F`
   has no code list (CL257 p19; LX-D30), so no CANopen report identifies Err5. The row comes from one fresh
   04a item 4 bracketed acquisition with MONITORS and raw `603F`, never from retained history:

   | Method | Fresh report | First word, or none | Disposition |
   | --- | --- | --- | --- |
   | `CONTROLWORD_BIT7` | Bit3 alarm set, both `6061` replies 1, low bits 111 (p12 pairs them with 6040=15), `2100` bit2 running clear | `0x0080`, then `0x0000` only after its acknowledgement | GATED under X13 until a CL2 fault record of that model links the CANopen report (raw `603F`, bit3, `2100` bit3) to a fault p20 lists as manually resettable; then admitted from this report only |
   | `OBJECT_2301` | The same report | `2301` = 1, then 0 after its acknowledgement (the 0 is library policy, as D-27) | GATED under X13 as above; the record also gives the report after the write, since p16 states no starting state and the C-family `2301` results are CL86-C measurements |
   | Either | Bit3 alarm set in mode 3 or 6, or with other low bits | None | GATED under X13 and X15: p12 prints 15 to 128 only for position mode |
   | Either | `2100` bit2 running set | None | Refused: a running drive needs a stop first |
   | Either | Bit3 clear | None | Reported as no alarm; nothing sent |

   Uncertain writes, completion and wording follow 03b item 7: a possibly sent first value without an
   acknowledgement sends nothing further and reports the reset possibly asserted; nothing is repeated;
   completion needs a fresh bracketed read with bit3 clear; a clear never claims the fault is gone.
8. Position clear. `2302` (P0D-04; CL257 p14, p16) follows 03b item 8's admission over CL2 observations.
   It is GATED under X12 until a CL2 monitor record of that model qualifies `2100` bit2 running and bit3
   alarm. With it, one explicit fresh state read through the model-taking getter must show mode 1 (`6061`
   from the same acquisition), `2100` bit2 running and bit3 alarm clear and `606C` zero. 03b item 8's
   exclusion of an earlier motion left unknown and its rechecks before each write apply unchanged; a C-family
   or TL-C sample is refused. As in 03b item 8 this is a controller-reported predicate: the result claims
   no standstill.
9. I/O settings. A CL2 DI candidate covers `2400:01` bits 0..6 and `2400:02..08` codes 0..6 with CL2 meanings
   (code 4 motor release); codes 7..22 are refused as undescribed (LX-D25). Output polarity `2400:0C` bits 0..2
   and functions `0D..0F` codes 0..5, 7, 9..11 follow CL2's one documented layout (CL257 p10, p17) and may be
   implemented with explicit caller intent; codes 5 (brake) and 7 (motor enable) also need a caller
   acknowledgement of the external load. `60FE` output command stays gated: only "both bits set gives output"
   is documented (CL257 p10), not clearing or masking.
10. Tuning. CL2 loop-mode setter with codes 0, 1, 3; 2 is refused (LX-D22). `2501` is never written (X10).
    `2502` as a native value needs both bounds: 0..7000 or 0..8000 mA by model (04a item 7; CL257 p17 and
    ABZ57 p17 print 0..7000, CL286 p17 and ABZ86 p17 0..8000) and the caller's X14 record for the attached
    motor, a positive peak-current limit in mA (the basis of "最大峰值电流 单位mA"), as 03b item 10 requires
    for TL-C. An admitted value satisfies both; a missing or zero motor limit, or a value inside the drive
    range but above the motor limit, refuses with zero TX. `2503..2519` bounded native setters where CL257
    p17..18 print a range; others refuse.
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
    p11..13) with a CL2 source-state table, otherwise GATED under X15. A later prompt 57 landing applies
    these cells as 03b item 12 says.

## Decisions that do not transfer here

`enableScopeHolds` facts, `MONITOR_RELEASED`, `STATUS_EN86_ZERO_SPEED`, mode-6 completion on bit 8,
`HomingMethod` trajectories, `InputFunction::ENABLE_MF` for code 4, `LoopMode::CLOSED_LOOP`=2, DEC-8
predecessors, DEC-2's measured premise, the `AlarmClearMethod` default, FF01/FF05 decoding, Q04 scale, S21,
the release table's CL86-C rows and the existing `sameFirmware` refusal.

## Deliver

CL2 admission in `Actions`, `Position`, `Velocity`, `Homing`, `Settings`, `Tuning` and `Persistence`; the CL2
homing table and its admission; `test/cl2_operations_test.cpp` through the real owner and `LichuanCl2Drive.h`:
bit 10 completion only with fresh transitions, a pre-high bit 10, mode 3 without completion, a mode or
generation change between samples, an acknowledged `6060` write whose `6061` display changes only after the
statusword sample (`UNKNOWN`, no completion), a stale or other-model sample, a missing transition, a fault, no read
acceptance (completion `UNKNOWN`) and expiry of the one operation deadline, each with no inferred standstill
and no later trigger; method refusals before setup or trigger with no TX (index on CL2-57C/86C with and
without a declared Z, index on ABZ without the Z fixture record, a missing switch or start-direction
prerequisite, 35, C-family meaning of 17, a method without its X11 record, a missing or conflicting `6098`
write-policy row with a valid method record, which sends no homing setup download at all, not only no
method write or trigger); code 4 release, `2500`=2
refused, `2501` never written, `605D` never written; the alarm-clear table (every row GATED with zero TX
without the X13 record; with a synthetic record, the first frame or none for every row, a lost first and a
lost second acknowledgement, an already clear request, mode 3 and running set); the attended enable with
raw `607A` equal to `6064` and no basis record, and with a basis record that does not show encoder-reported
`6064` and no actual-position record (each MISSING_PREREQUISITE, zero TX); 03b's enable source-state rows
and word-15 end with bracketed acquisitions; switch prerequisites with correct assignment bytes and no
fixture signal record, the signal on another input or another polarity (zero TX); `2502` with no motor limit, a
zero limit and a value inside the drive range above the motor limit (zero TX); save and restore sending no
node or bitrate write and keeping the route-after-restart obligation on success and on a lost
acknowledgement; CL86-C evidence refused; the paired firmware-only and missing-fact evidence tests, the
identity stop-exemption tests, the best-effort refusals (a direct `admitBestEffortStop` included), the
halted and unknown-halt stop rows, the halt-derivation, move-interruption, timed-run (an elapsed fallback
deadline and a released result before stop due included), service-level fallback, late-service,
homing-interruption and position-clear tests of 03b with CL2 observations. Ledger, coverage, `docs/api_cli_mapping.md`,
CHANGELOG, report and `PROGRESS.md`.

## Acceptance

Each CL2 operation is admitted with cited CL2 pages and a CL2 evidence input, or refused before TX with its
disposition from stage 02's map. No C-family meaning of bit 10, method numbers or input code 4 reaches a CL2
binding, and no firmware difference refuses one. Every CL2 operation, reviewed stops included, runs on a
binding that already carries S3's explicit no shape (04a item 3). Quick and full verification, firmware
builds and exact-commit CI pass. Hardware NOT RUN; X08, X11, X12, X13, X14, X15, X16 and X18 stay open.
