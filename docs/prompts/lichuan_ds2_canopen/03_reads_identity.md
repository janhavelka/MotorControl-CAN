# 03 — Typed reads, identity, status dialects and capabilities

Execute this stage under the [execution contract](execution_contract.md).
Status: NOT STARTED. Runs after 02b. DS-D01..DS-D05, DS-D14, DS-D19, DS-D30, DS-D33 and DS-D34 gate individual
interpretations; raw observations remain available while an interpretation is gated. MULTIDRIVE rows 19, 38, 43 and
57 apply.

## Work

1. Identity. `prepareIdentity` reads `1000:00`, `1018:00`, then each of `1018:01`..`04` only when the fresh `1018:00`
   announces it and the declared model's scope sources it (DS-D05: manuals and DS2EDS give 3 with no sub04, IBLEDS and
   ISLEDS 4 with a serial). A short count leaves the unannounced fields unread; a count above the scope's sourced
   sub-indexes is kept raw as a contradiction and reads nothing absent from the sources. Add `1009` and `100A` (U16,
   RO, no default; DS2C p24) as raw firmware identity. A reply whose size differs from the reviewed width is a
   `REPLY_ERROR` with its bytes kept; a width option needs a measured DS2 specimen (never `IdentityWidths`).
2. Identity outcome. Apply [prompt 57](../57_regular_drive_operations.md)'s Identity policy through its one comparison:
   if prompt 57 or the extensions stage 01 landed it, extend it with DS2's references and candidates (execution
   contract); otherwise land it profile-neutral as prompt 57 and the extensions contract state, so they extend it. No
   DS2-local classifier. The result carries what prompt 57 lists, plus both documented claims of the declared model
   (manual and EDS). `1000` is one of the product fields compared; it never yields a motor kind or servo/stepper fact
   (DS-D03). There is no caller acceptance (prompt 57): the application corrects a non-matching specimen by
   supplying a MEASURED reference through the API. Record the comparison's location in the report.
3. State. `prepareState` reads raw `6041`, `6061` (I8 display, never written), `6064`, `606C`, `6077` (read-only
   despite the RW object-table row; DS-D14), `60FD` (bits 0..2 negative limit, positive limit, home; bits 16..31
   "user"; the printed default 10000 is retained text, DS-D30) and `603F` (raw U16; no Er-code mapping, DS-D19), plus
   `1001`. `1003` is an explicit single read: `:00`, then only the announced sub-indexes up to 4 (DS2C p24 says five
   stores but lists four; DS-D19). Never read `6060` as a mode display: DS2EDS declares it WO (DS-D08).
4. Status dialects (DS-D04). A profile-local `StatusDialect` selected by `Model` through the 01 matrix:
   `DS2C_TABLE` (DS2C p12-13, MINI p13/p15, IBL80 p13): bit 7 alarm, bit 8 homing complete, bit 10 speed zero, bit 12
   target position effective, bit 15 positioning complete; `IBL60_TABLE` (IBL60 p13-14, ISL60 p15/p17, ISL80
   p14/p16): bit 3 alarm, bit 10 positioning complete in mode 1 and homing complete in mode 6. Bits 0..2 follow the
   documented 0/6/7/15 example only. Each interpretation is "documented, firmware unqualified"; undocumented bits stay
   raw. Do not build a CiA 402 state decoder, name fault, quick-stop or switch-on-disabled states, or reuse
   `StatusLayout`. The documented alarm bit is a drive report, not a fault-state decoder or a clearable condition.
5. Configuration groups. `prepareConfig` with bounded groups that read only objects of the declared model's scope:
   position profile (`6081`, `6083`, `6084`, `6085`, `6065`, `6066`, `6067`, `6068`), velocity (`60FF`, `606D`,
   `606E`, `607F`), torque (`6071`, `6072`, `6087`, and `2406h` for IBL models only), homing (`6098`, `6099:01/02`,
   `609A`, `607C`), limits (`607D:01` raw, and `607D:02` raw only in scopes whose EDS sources it; MINI's manual prints
   both rows as `:01` and it has no EDS, so MINI reads `:01` only; DS-D15), scale and polarity (`6091:01/02`, `607E`)
   and halt option (`605D`, DS-D17). Configuration groups and `2406h` follow the execution contract's admission
   table. `2406h` is an explicit single read for IBL models only; an abort `0602 0000h` records "absent on this
   specimen". Values stay raw with the ledger's documented units and conflicting claims; no unit conversion happens
   here.
6. Capabilities. The caller supplies `Capabilities` per configuration. Position, velocity and torque are available
   only for models whose sources document modes 1, 3, 4 and 6 (DS2C p10, p27; each manual's mode section). Feedback is
   `ENCODER` with the declared suffix; resolution stays a separate scale fact (`P00-31`/`P00-32` are unreachable).
   `holdingBrake` is caller-declared (DS-D26). The motors are permanent-magnet servo motors with five pole pairs
   (DS2C p14-15); `MotorKind` has no such value. Record UNKNOWN unless the common-vocabulary owner (prompt 64)
   accepts BLDC or adds a value; do not decide it in the profile.
7. Fill `DiscoveryCapabilities` from the inventory; scans stay unsupported until 11. A compatible reply never proves
   manufacturer, model or firmware.
8. Read effects and polling. Every read keeps `effectPossible` until `DS-DEC-1` exists. A fixed read that an
   operation's sequence names (its admission report, the read after an enable word, a post-stop report) is an
   explicit read of that operation; repeating a read until a condition holds (completion, homing, a mode change) is
   polling. This stage prepares the `DS-DEC-1` record from the ledger's `read_effect` review: the exact state objects
   an operation may repeat, per scope, the repeat policy and per-operation observation budget. Selecting it stays an
   owner or lead decision at implementation time, recorded with `selected_by` and `selected_on` and marked for the
   owner's review (12). Marker objects for S10 are a separate decision; DS2 bindings keep `NONE` (02b). Without
   `DS-DEC-1` no context issues a repeated upload: 05's move ends at acknowledgement with completion `NOT_OBSERVED`,
   07 refuses homing, and 04 to 06b use only their named single reads. Typed getters check kind, phase, binding and
   configuration generation and leave output unchanged on failure.

| Interpretation | Gate | What the stage publishes |
| --- | --- | --- |
| Product identity | DS-D01, DS-D02, DS-D05 | Raw values, both documented claims, identity outcome and matching models |
| Motor technology from `1000` | DS-D03 | Raw `1000` only; never a motor kind |
| Status bit meanings | DS-D04 | Raw word plus the declared model's dialect, labelled documented and unqualified |
| `6077` access | DS-D14 | Read-only raw 0.1 % value |
| `1003` length, `603F` meaning | DS-D19 | Raw entries and code; no Er-code text from a reply |
| `60FD` bits beyond 0..2 | DS-D30 | Raw bits |
| `607D` sub-index order | DS-D15 | Both raw values, no minimum or maximum label |
| Units or width of any configuration value | DS-D09, DS-D10, DS-D16, DS-D33, DS-D34 | Raw native integers with the ledger's conflicting unit and type claims |

## Deliver

`Reads.h`/`Reads.cpp` (identity, state, configuration plans and checked getters), `State.h`/`State.cpp` (dialect
tables and `interpretStatus`), DS2's identity references and candidates on the shared comparison (or the
comparison itself if it lands here), the prepared `DS-DEC-1` record in the ledger; `test/lichuan_ds2_reads_test.cpp`,
`test/lichuan_ds2_state_test.cpp`; owner-backed API and console cases for `ds2 identity`, `ds2 state` and
`ds2 config <group>` (BLOCKED until S4); coverage, operation map, `docs/reads.md` DS2 section, Doxygen, CHANGELOG and
`docs/reports/lichuan_ds2_canopen_03.md`.

## Acceptance

Two test sets stay separate. Comparison truth-table tests use explicitly synthetic references and cover every
outcome: CONFIRMED, UNCONFIRMED with and without a declared-model match, with an unread field and with a candidate
lacking references, and CONTRADICTED. Production-claim tests use the documented references with both fake-server
variants (manual and EDS) per model and assert the actual outcome and matching-model list: the manual pair gives
UNCONFIRMED with all nine models, the DS2EDS/ISLEDS pair UNCONFIRMED with five, the IBLEDS pair UNCONFIRMED with two;
a pair that only other models' references hold (the IBLEDS pair on a DS2 or ISL declaration, the DS2EDS/ISLEDS pair
on a MINI or IBL declaration) and 0x1000/0x0001 give CONTRADICTED; MINI uses manual-only fixtures; a stale observation
after boot-up or rebinding has no outcome. Specimen references are reserved for qualified MEASURED rows. Also:
`1018:04` read only when announced and sourced (short, oversized and contradictory counts, no absent object read),
width mismatch kept raw, wrong-dialect interpretation impossible for every model, `6060` never read, `607D:02` never
read for MINI, `2406h` refused outside IBL and per the admission table, `DS-DEC-1` absent giving no repeated upload,
unchanged outputs on malformed or partial sequences, deadline, cancel, stale generation and late reply. Lichuan C reads and frames are unchanged. Quick and full verification and required
CI on the exact final pushed commit pass. Hardware NOT TESTED. Commit, push and hand off.
