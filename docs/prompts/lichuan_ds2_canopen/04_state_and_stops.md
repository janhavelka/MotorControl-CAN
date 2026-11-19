# 04 — Mode select, enable, release and stops

Execute this stage under the [execution contract](execution_contract.md).
Status: NOT STARTED. Runs after 03. DS-D17, DS-D21 and DS-D29 belong here; DS-D04 gates every state interpretation.
MULTIDRIVE rows 30, 31, 32 and 57 apply. Use strong independent review for the state, stop and uncertainty logic.

## Work

1. Mode select. `prepareModeSelect` writes `6060:00` (I8 in the manual, U8 in DS2EDS; identical bytes for 1, 3, 4, 6;
   DS-D08) and completes only on a fresh `6061` equal to the request. Accept 1, 3, 4 and 6 (DS2C p10, p27); refuse 0,
   negative and undocumented values before TX. Refuse while the fresh status bits 0..2 read 111 or are unknown: a
   mode change with retained targets may start motion (DS2C p12-13), and DS2 documents no mode-change timing
   (CP-D24 shows vendors differ).
2. Enable. `prepareEnable` writes whole words 0x0006, 0x0007, 0x000F, each after the previous SDO confirmation, with a
   caller-supplied host spacing; the manual's "about 10 ms" (DS2C p11-13) is an example, not a minimum. Each word's
   documented bits 0..2 (001, 011, 111; DS2C p12) are checked on a fresh state read and recorded. Admission: fresh
   `6061` is 1 or 6. In modes 3 and 4 enabling starts motion, so it belongs to 06 and 06b and is refused here.
   Bit 4 stays 0 in all three words, so no position or homing trigger is issued. Whether the factory `P02-00`
   default (1 in DS2C p15 and MINI p17, 9 in IBL60 p16; DS-D29) accepts bus control is unknown: an enable that does
   not reach 111 ends with its evidence, without retry, and 12a asks the question.
3. Release. Name two explicit native actions from the documented example (DS2C p12): shutdown 0x0006 (bits 001) and
   disable voltage 0x0000 (bits 000). Choose the default by a recorded `DS-DEC-4` with its reason. Neither is a stop.
   Torque removal, coast and brake timing are not documented (DS-D26; `P02-09`/`P02-10` delays are drive-internal,
   DS2C p16), so release requires a fresh stationary report or an explicit caller acceptance, and its result never
   claims standstill or brake engagement. Follow [prompt 57](../57_regular_drive_operations.md)'s release-table shape
   with DS2's own rows; do not copy Lichuan C words or reports.
4. Stops. A profile-local `StopKind` with one supported value: `QUICK_STOP` = 0x000B, documented as "emergency stop"
   15 to 11 in the position-mode table (DS2C p11). Its reaction is undocumented: `605A` is absent, `6085` exists
   (U32, pul/s², default 10000; DS2C p28) without a stated link, and the state after the stop is unknown. Use in modes
   3, 4 and 6 is a project decision (`DS-DEC-2`) with its reason, scoped to DS2. Halt (controlword bit 8 with `605D`
   0 normal or 1 quick stop; DS2C p27, DS2EDS default 1; DS-D17) stays UNSUPPORTED: bit 8 is undocumented. A zero
   velocity target is not a stop (06).
5. Stop predecessors and completion. The only documented arrow starts at 15. Record DS2's predecessor rule
   (`DS-DEC-2`) for trigger words 31, 95, 63, 127 and for modes 3, 4 and 6; DEC-8 does not transfer. A stop ends
   `ACKNOWLEDGED` or `UNKNOWN`; `OBSERVED` needs a fresh post-stop report the stage names per dialect (DS2C bit 10
   "speed zero"; the IBL60 dialect has no zero-speed bit, so fresh zero `606C` samples are a fixture hypothesis).
   Never enable to prepare a stop. Urgent admission uses `admitUrgent`; an in-flight request settles first.
6. Seam S3, claimed in the canopen_shared table before the shared edit. The best-effort stop on an unresolved route
   is opt-in per binding with a reviewed exact shape (object, size, data, ledger review ID; canopen_shared 02). DS2's
   shape is `6040:00=000B` by DS2C p11 under `DS-DEC-3`, set for each DS2 binding, never success and never a qualified
   reply; Lichuan C keeps DEC-2. No shape, another family's shape and a stale binding or channel generation return
   `INVALID` with no TX and no fallback word. If another family landed S3, extend it with DS2's row and tests.
7. Fault clear and position set. `prepareClearAlarm` returns UNSUPPORTED with a CAPABILITY detail before TX: no
   controlword fault reset is documented; `P0D-01` and DI FunIN.2 (edge-triggered; some faults need servo-on off
   first) are unreachable or physical (DS2C p9, p23; DS-D21). `prepareSetDevicePosition` is UNSUPPORTED: DS2 has no
   object, and the "clear position" section is a broken reference (DS2C p3; DS-D20). Homing method 35 is 07's.
8. Seam S8. Compare DS2 and Lichuan C enable and bits-0..2 code. Extract a shared CiA 402 helper only for identical
   evidenced semantics (whole-word constants, the bits 0..2 projection); everything else stays profile-local.

## Deliver

`Actions.h`/`Actions.cpp` with `ActionContext`, mode select, enable, the two release actions, `QUICK_STOP`, the
best-effort stop row and explicit UNSUPPORTED preparations; `test/lichuan_ds2_actions_test.cpp`,
`test/lichuan_ds2_stop_test.cpp`; owner-backed console routes `ds2 mode`, `ds2 enable`, `ds2 release`, `ds2 stop`;
`DS-DEC-2`, `DS-DEC-3` and `DS-DEC-4` records in the ledger; `docs/stop_actions.md` and `docs/drive_actions.md`
DS2 sections; coverage, operation map, Doxygen, CHANGELOG and the seam table.

## Acceptance

Tests cover mode select refused while enabled, mode mismatch in `6061`, enable refused in modes 3 and 4, each enable
word lost or aborted, an enable that never shows 111, release refused without stationary evidence, stop while queued,
in flight and waiting, a stop on an unresolved route that never succeeds, both dialects' completion evidence, a
completion bit already high, UNSUPPORTED fault clear and halt with no TX, and no replay after a lost reply. Lichuan C
stop shapes and tests are unchanged. Quick and full verification and required CI on the exact final pushed commit
pass. Hardware NOT TESTED. Commit, push and hand off before any motion stage exposes a route.
