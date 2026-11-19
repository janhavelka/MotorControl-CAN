# 04 — Mode select, enable, release and stops

Execute this stage under the [execution contract](execution_contract.md).
Status: NOT STARTED. Runs after 03. DS-D17, DS-D21 and DS-D29 belong here; DS-D04 gates every state interpretation.
MULTIDRIVE rows 30, 31, 32 and 57 apply. Use strong independent review for the state, stop and uncertainty logic.

## Work

1. Source-state table (canopen_shared 02 "Operation admission", state transitions), delivered before any other
   item exposes a route. Rows are keyed by dialect, the fresh report (bits 0..2 pattern, the dialect's alarm bit,
   `6061` mode, freshness) and the requested intent (mode select, enable, each release action, `QUICK_STOP`); each
   row gives the first permitted word, an already-satisfied result with no TX, or a refusal. Only the documented
   projections 000, 001, 011 and 111 (DS2C p12) can admit anything; any other pattern, a set alarm bit, an unknown
   mode and a stale or missing report refuse. No CiA 402 state is named or decoded. The table also defines the
   `released` and `enabled` predicates; 05, 06, 06b, 07 and 08 consume them and test no low bits of their own.
2. Mode select. `prepareModeSelect` writes `6060:00` (I8 in the manual, U8 in DS2EDS; identical bytes for 1, 3, 4, 6;
   DS-D08) and completes only on a fresh `6061` equal to the request, read once; a mismatch ends with its evidence.
   Accept 1, 3, 4 and 6 (DS2C p10, p27); refuse 0, negative and undocumented values before TX. Refuse unless the
   table classifies the fresh report as released: a mode change with retained targets may start motion (DS2C
   p12-13), and DS2 documents no mode-change timing (CP-D24 shows vendors differ).
3. Enable. `prepareEnable` writes whole words 0x0006, 0x0007, 0x000F, each after the previous SDO confirmation, with a
   caller-supplied host spacing; the manual's "about 10 ms" (DS2C p11-13) is an example, not a minimum. The table row
   chooses the first word; a fresh 111 in mode 1 or 6 returns already enabled with no TX, never a shutdown first.
   That result carries no trigger predecessor (item 10).
   Each word's documented bits 0..2 (001, 011, 111; DS2C p12) are checked on one fresh state read and recorded.
   Admission: fresh `6061` is 1 or 6. In modes 3 and 4 enabling starts motion, so it belongs to 06 and 06b and is
   refused here.
   Bit 4 stays 0 in all three words, so no position or homing trigger is issued. Whether the factory `P02-00`
   default (1 in DS2C p15 and MINI p17, 9 in IBL60 p16; DS-D29) accepts bus control is unknown: an enable that does
   not reach 111 ends with its evidence, without retry, and 12a asks the question.
4. Release. Name two explicit native actions from the documented example (DS2C p12): shutdown 0x0006 (bits 001) and
   disable voltage 0x0000 (bits 000). Choose the default by a recorded `DS-DEC-4` with its reason. Neither is a stop.
   Torque removal, coast and brake timing are not documented (DS-D26; `P02-09`/`P02-10` delays are drive-internal,
   DS2C p16), so release requires a fresh stationary report or an explicit caller acceptance, and its result never
   claims standstill or brake engagement. Follow [prompt 57](../57_regular_drive_operations.md)'s release-table shape
   with DS2's own rows; do not copy Lichuan C words or reports.
5. Stops. A profile-local `StopKind` with one supported value: `QUICK_STOP` = 0x000B, documented as "emergency stop"
   15 to 11 in the position-mode table (DS2C p11). Its reaction is undocumented: `605A` is absent, `6085` exists
   (U32, pul/s², default 10000; DS2C p28) without a stated link, and the state after the stop is unknown. Use in modes
   3, 4 and 6 is a project decision (`DS-DEC-2`) with its reason, scoped to DS2. Halt (controlword bit 8 with `605D`
   0 normal or 1 quick stop; DS2C p27, DS2EDS default 1; DS-D17) stays UNSUPPORTED: bit 8 is undocumented. A zero
   velocity target is not a stop (06).
6. Stop predecessors and completion. The only documented arrow starts at 15 in position mode (DS2C p11; the same table
   in MINI p13, IBL60 p12 and ISL60 p15). `DS-DEC-2` records one row per mode and predecessor it admits, keyed by item
   10's history (word 0x000B after 15, 31, 95 or 0x001F, and after each uncertain word, in modes 1, 3, 4 and 6), each
   with its basis (DS2C p11 for 15 in mode 1, project decision otherwise) and the reaction as undocumented. 06 and 06b
   refuse a run whose mode has no row (canopen_shared 02 "Operation admission", timed runs). DEC-8 does not transfer.
   A stop ends `ACKNOWLEDGED` or `UNKNOWN`; `OBSERVED` needs a fresh post-stop report the stage names per dialect
   (DS2C bit 10 "speed zero"; the IBL60 dialect has no zero-speed bit, so fresh zero `606C` samples are a fixture
   hypothesis). Never enable to prepare a stop. Urgent admission uses `admitUrgent`; an in-flight request settles
   first.
7. Seam S3 opt-in, claimed in the canopen_shared table before the shared edit. Its default deny landed with S2
   (02b), so until this item every DS2 binding gets `INVALID` from `admitBestEffortStop`. The best-effort stop on an
   unresolved route is opt-in per binding with a reviewed exact shape (object, size, data, ledger review ID;
   canopen_shared 02). DS2 opts in `6040:00=000B` (DS2C p11) only when `DS-DEC-3` records the review the S3 bullet
   requires: the word's reaction from every state a DS2 drive may be in after an uncertain write (modes 1, 3, 4 and 6
   after any word of item 10's history, an uncertain mode select included) and how it treats bit 8 (DS2 never sets
   it; halt is UNSUPPORTED). No DS2 source documents those reactions, so the review rests on the principles pack's
   reading that a Quick stop command names no enabling transition (CP-D10) and says so; unless the lead or owner
   accepts it, DS2 keeps the default deny. Once opted in, the attempt is eligible only on an unresolved route of the
   exact current binding and channel generation and follows S3's unchanged rules (at most one transmission, never
   success, never a qualified reply, `needsStop` kept); Lichuan C keeps DEC-2. No shape, another family's shape and a
   stale binding or channel generation still return `INVALID` with no TX and no fallback word. If another family's
   stop stage extended S3 first, add DS2's row and tests.
8. Fault clear and position set. `prepareClearAlarm` returns UNSUPPORTED with a CAPABILITY detail before TX: no
   controlword fault reset is documented; `P0D-01` and DI FunIN.2 (edge-triggered; some faults need servo-on off
   first) are unreachable or physical (DS2C p9, p23; DS-D21). `prepareSetDevicePosition` is UNSUPPORTED: DS2 has no
   object, and the "clear position" section is a broken reference (DS2C p3; DS-D20). Homing method 35 is 07's.
9. Seam S8. Compare DS2 and Lichuan C enable and bits-0..2 code. Extract a shared CiA 402 helper only for identical
   evidenced semantics (whole-word constants, the bits 0..2 projection); everything else stays profile-local.
10. Controlword history and the trigger predecessor. `6040` is never read: it is WO in DS2EDS l.1199, IBLEDS l.1073
   and ISLEDS l.1073 (RW in DS2C p27; DS-D08), and a 111 report does not show bit 4. The bit-4 rising edge (DS2C p11,
   p13; IBL60 p12, p14) therefore starts from host history, as in Lichuan C (`profiles/lichuan_c/Position.h`
   `control`, `Actions.h` file comment): the caller supplies the actual latest acknowledged `6040:00` download under
   one continuous owner reservation and the current binding, node, session and configuration generations, followed
   by a fresh report that the `enabled` predicate accepts in the trigger's mode. 05 and 07 admit a trigger only when
   that word is exactly 0x000F with no later `6040` or `6060` write. Only three transitions establish it: this
   stage's enable sequence ending in an acknowledged 0x000F, 05's `DS-DEC-5` rearm inside its move and 07's explicit
   homing interrupt. A trigger, a stop (`QUICK_STOP` or a best-effort word), an uncertain or aborted `6040` write, an
   admitted NMT command, boot-up, rebind, bus-off and owner invalidation leave none, and "already enabled" creates
   none: attaching to an enabled drive needs an explicit release and enable first. A move or homing never writes
   0x000F to make one; without one it refuses before TX and says why.

## Deliver

`Actions.h`/`Actions.cpp` with `ActionContext`, the source-state table and its `released`/`enabled` predicates, the
controlword history and trigger-predecessor check, mode select, enable, the two release actions, `QUICK_STOP`, the
best-effort stop row and explicit UNSUPPORTED preparations; `test/lichuan_ds2_actions_test.cpp`,
`test/lichuan_ds2_stop_test.cpp`; owner-backed console routes `ds2 mode`, `ds2 enable`, `ds2 release`, `ds2 stop`;
`DS-DEC-2`, `DS-DEC-3` and `DS-DEC-4` records in the ledger; `docs/stop_actions.md` and `docs/drive_actions.md`
DS2 sections; coverage, operation map, Doxygen, CHANGELOG and the seam table.

## Acceptance

Tests assert the first emitted frame, or none, for every row of the source-state table and both dialects, including an
already enabled drive (no TX), undocumented patterns, a set alarm bit and a stale report. They also cover mode select
refused unless released, mode mismatch in `6061`, enable refused in modes 3 and 4, each enable word lost or aborted,
an enable that never shows 111, release refused without stationary evidence, stop while queued, in flight and waiting,
a stop on an unresolved route that never succeeds, both dialects' completion evidence, a completion bit already high,
UNSUPPORTED fault clear and halt with no TX, and no replay after a lost reply. The trigger predecessor exists after an
acknowledged enable and is absent, with exact owner traffic, after an already-enabled attachment, a trigger, a stop,
an uncertain or aborted word, an NMT command, boot-up and rebind. The best-effort stop returns `INVALID` with no TX
before `DS-DEC-3`'s opt-in and sends one word after it; a fake server accepting 0x000B clears no gate. Lichuan C stop
shapes and tests are unchanged. Quick and full verification and required CI on the exact final pushed commit pass.
Hardware NOT TESTED. Commit, push and hand off before any motion stage exposes a route.
