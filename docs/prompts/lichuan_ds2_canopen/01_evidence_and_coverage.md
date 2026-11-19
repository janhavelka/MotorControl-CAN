# 01 — Evidence, inventory and operation coverage

Execute this stage under the [execution contract](execution_contract.md).
Status: NOT STARTED. The pack already exists; do not re-download unchanged originals. This stage changes no product
code and needs no hardware.

## Work

1. Run [canopen_shared 01](../canopen_shared/01_existing_code_and_tooling.md) at the current HEAD: record HEAD, dirty
   state and PROGRESS rows 57, 63, 64, 65, 73, 75 and 77; refresh MotorControl-RS and its profile seam; mark every
   inventory row confirmed, changed or absent after reading the symbol and its callers. Classify each row for DS2.
   Needed seams: S1, S2, S6, S7 and S10 (owned here or by the first family), S3 and S8 (stage 04), S4 and S5
   (prompts 64 and 65); S13 is consumed (09, 11). S9, S11 and S12 are not needed (README). Record every claim and
   landing in the canopen_shared seam table since `5045423d`.
2. Establish the protocol from family evidence: CANopen CiA 301/402 by DS2C p24 (`1000` text), the EDS
   `[DeviceInfo]` sections and the object tables (DS2C p24-29). Modbus and the USB debug port (DS2C p22-23, DS2OP p5)
   are other documented paths to P-parameters and stay excluded.
3. Check the pack's integrity: `check_promptset.py` counts it among `reference_packs` and hashes every original; run
   it with `--check-git-ref HEAD`. Confirm `.gitattributes` disables line-ending conversion for the pack's `vendor/`
   and `extracted/` paths. If the CANopen principles pack is still uncommitted, cite it by path and record that its
   bytes are not yet under a committed hash; do not commit it from this stage.
4. Seam coordination. Other family promptsets (Nanotec N5, maxon EPOS4, MKS, Lichuan C extensions, MOONS') plan to
   land the same seams. Read their READMEs and record, per seam, which family is expected to execute first and the
   current claim and landed state in the canopen_shared table. A DS2 stage claims a seam there before its shared
   edit; DS2 never builds a parallel copy of a claimed or landed seam.
5. Build the model matrix from rendered pages, one row per model: manual, EDS (MINI has none; never borrow DS2EDS),
   status dialect (DS2C p12-13, MINI p13/p15, IBL80 p13 versus IBL60 p13-14, ISL60 p15/p17, ISL80 p14/p16; DS-D04),
   torque speed-limit object (`607F` versus `2406h`, IBL60 p13, IBL80 p13; DS-D11; confirm the ISL80 page), DIP node
   range (DS2C p6-7, IBL60 p7, IBL80 p7; DS-D22), brake form (DS2C p6, MINI p8/p10, IBL60 p7/p11, ISL60 p8/p13;
   DS-D26), touch probe (ISL60 p12; DS-D28), encoder claims (DS2C p4, IBL60 p5, ISL60 p6; DS-D25), ratings (DS-D24),
   motor codes (DS-D31) and the `P02-00` default (DS-D29). Each cell cites its page. An independent read-only
   reviewer (DELEGATION) re-checks the matrix against the rendered pages without seeing the lead's conclusions.
6. Write the identity decision (backlog D18, DS-D01, DS-D02, DS-D03, DS-D05) into the coverage file: binding by the
   declared model; DS2's references and model-scoped rule on prompt 57's identity outcomes, as in the execution
   contract; identity reads confirm or contradict only. Record that prompt 57's Identity policy now states D18 and
   leaves model-scoped checks to the Lichuan family sets, and that prompts 70 and 75 still assume one expected
   product identity per node, where DS2 needs its claim set (11).
7. Intake corrections (old claim, evidence, scope, disposition). The pack's discrepancy list omits two object
   conflicts; reopen each page and EDS line, and record both in the coverage file and the report with the next
   free IDs. The pack review is not edited.
   - DS-D33, `6081` default and unit. Old claim: none in the pack list (the review lists `6081` among pul/s
     objects). Evidence: DS2C p28 gives the position-mode target speed in pul/s, U32 RW, default 10000; DS2EDS
     l.1398-1405 `ParameterName=posmode speed`, U32, `DefaultValue=60`. Scope: DS2 group documents. Disposition:
     open; 02 records the conflict row, 05 owns the rate unit, 12a Q7 measures it.
   - DS-D34, `6091:02` type. Old claim: "type not printed" (review, Units and scaling) without a DS-D. Evidence:
     DS2C p28 prints no type for the denominator while `6091:01` is I32; DS2EDS l.1464-1470 declares
     `ShaftRevolutions` `DataType=0x0004` (I32). Scope: DS2 group documents. Disposition: open; 02 records the
     conflict row, 05 (scale) and 08 (gear setter) consume it.
8. Apply the Lichuan C boundary table of the execution contract: confirm each named `LICHUAN_C` symbol exists at HEAD
   and add any new Lichuan-specific symbol to the "must not reuse" side. Note in the report that the DS2C status
   table is textually the EN86 table, and why that is not shared code.
9. Create `docs/lichuan_ds2_coverage.md` following canopen_shared 02 "Capabilities and the common operation mapping":
   one row per common operation (identity, state and configuration reads, probe, enable, release, mode select,
   absolute, relative and angle move, finite velocity, finite torque, homing, each stop kind, fault clear, device
   position set, save, restore, node and bitrate change, heartbeat change, PDO observation and remap, SYNC) and one row
   per documented native function, including prose-only functions and every P-parameter group. Columns: native
   objects and words, sources, DS-D and planned `DS-G` gates, owning stage, the identity consumed (ledger key, holder
   and invalidators, canopen_shared 02), planned disposition and the independent documented, implemented,
   host-tested and hardware-qualified columns. Only `documented` may be filled now.
10. Record the planned unsupported set with its evidence reason: fault clear (DS-D21), save and restore (DS-D20),
   node and bitrate change (DS-D22, DS-D23), halt (DS-D17), gains and auto-tuning (P08/P09, DS2C p18-21, no CANopen
   index), brake control (DS-D26), touch probe (DS-D28), homing method 35 (DS-D18), `60FE` outputs (DS2C p29), RPDO
   control and PDO remap (DS-D06), SYNC-driven PDOs (DS-D32), and the side-effect P-parameters of the review.
11. Create `PROGRESS.md` in this directory: one row per stage (01, 02, 02b, 03, 04, 05, 06, 06b, 07, 08, 09, 10, 11,
    12a, 12) with software state, inherited gates, hardware column (NOT TESTED) and handoff link.

## Deliver

- `docs/lichuan_ds2_coverage.md` (planned dispositions; nothing marked implemented) and its link from
  `docs/coverage.md` as a planned family, `PROGRESS.md` here, and `docs/reports/lichuan_ds2_canopen_01.md` with the
  inventory record, the RS audit row, the FieldCore boundary note, the model matrix, the intake corrections and the
  boundary confirmation.
- No source, script, ledger JSON or test change. No backlog item closes; backlog D18 gains a pointer to the decision.

## Acceptance

Every inventory row was read with its callers at the recorded HEAD. Every DS-D01..DS-D34 appears in the coverage file
with an owning stage. The model matrix cites a rendered page for each cell, and MINI carries no EDS fact. Nothing
from the Lichuan C must-not list is planned as reuse. `python scripts/check_promptset.py` and `git diff --check` pass.
Documentation-only: no remote CI dispatch. Commit and push the owned files with the report.
