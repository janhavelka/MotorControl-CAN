# 03 — Typed reads, identity, status dialects and capabilities

Execute this stage under the [execution contract](execution_contract.md).
Status: NOT STARTED. Runs after 02b. DS-D01..DS-D05, DS-D14, DS-D19, DS-D30, DS-D33 and DS-D34 gate individual
interpretations; raw observations remain available while an interpretation is gated. MULTIDRIVE rows 19, 38, 43 and
57 apply.

## Work

1. Identity. `prepareIdentity` reads `1000:00`, `1018:00`, then `1018:01`..`03`, and `1018:04` only when the fresh
   `1018:00` is at least 4 (DS-D05: manuals and DS2EDS give 3, IBLEDS and ISLEDS 4 with a serial). Add `1009` and
   `100A` (U16, RO, no default; DS2C p24) as raw firmware identity. A reply whose size differs from the reviewed width
   is a `REPLY_ERROR` with its bytes kept; a width option needs a measured DS2 specimen (never `IdentityWidths`).
2. Identity outcome. Apply prompt 57's identity policy through the one comparison of the
   [identity outcomes](../lichuan_c_extensions/execution_contract.md#identity-outcomes-prompt-57s-policy): if prompt 57
   or the extensions stage 01 landed it, extend it with DS2's references and rule (execution contract); otherwise land
   it profile-neutral as that section states, so they extend it. No DS2-local classifier. The result carries the outcome, both
   documented claims of the declared model (manual and EDS), the live value and every matching model. `1000` is
   compared with 0x00040192 and reported; it never yields a motor kind or servo/stepper fact (DS-D03). The explicit
   acceptance of the execution contract's table is a caller declaration bound to binding generation, settable and
   clearable through the API and `ds2 identity accept|clear`; it never admits a vendor-object write on CONTRADICTED.
   Record the comparison's location in the report.
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
   `609A`, `607C`), limits (`607D:01`, `607D:02` raw, DS-D15), scale and polarity (`6091:01/02`, `607E`) and halt
   option (`605D`, DS-D17). `2406h` is an explicit single read for IBL models only, refused on a CONTRADICTED
   outcome; an abort `0602 0000h` records "absent on this specimen". Values stay raw with the ledger's
   documented units and conflicting claims; no unit conversion happens here.
6. Capabilities. The caller supplies `Capabilities` per configuration. Position, velocity and torque are available
   only for models whose sources document modes 1, 3, 4 and 6 (DS2C p10, p27; each manual's mode section). Feedback is
   `ENCODER` with the declared suffix; resolution stays a separate scale fact (`P00-31`/`P00-32` are unreachable).
   `holdingBrake` is caller-declared (DS-D26). The motors are permanent-magnet servo motors with five pole pairs
   (DS2C p14-15); `MotorKind` has no such value. Record UNKNOWN unless the common-vocabulary owner (prompt 64)
   accepts BLDC or adds a value; do not decide it in the profile.
7. Fill `DiscoveryCapabilities` from the inventory; scans stay unsupported until 11. A compatible reply never proves
   manufacturer, model or firmware.
8. Read effects and polling. Every read keeps `effectPossible` until `DS-DEC-1` exists; a state read used by a later
   operation is an explicit read of that operation, not background polling. Typed getters check kind, phase,
   binding and configuration generation and leave output unchanged on failure.

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
tables and `interpretStatus`), DS2's identity references and model-scoped rule on the shared comparison (or the
comparison itself if it lands here); `test/lichuan_ds2_reads_test.cpp`,
`test/lichuan_ds2_state_test.cpp`; owner-backed API and console cases for `ds2 identity`, `ds2 state` and
`ds2 config <group>` (BLOCKED until S4); coverage, operation map, `docs/reads.md` DS2 section, Doxygen, CHANGELOG and
`docs/reports/lichuan_ds2_canopen_03.md`.

## Acceptance

Tests cover each outcome with each model's claim set (both manual and EDS fake variants): CONFIRMED, UNCONFIRMED with
and without a declared-model match, CONTRADICTED by another DS2 group and by 0x1000/0x0001, a stale observation after
boot-up or rebinding; `1018:04` read only when announced, width mismatch kept raw, wrong-dialect interpretation
impossible for every model, `6060` never read, `2406h` refused outside IBL and on CONTRADICTED, unchanged outputs on
malformed or partial sequences, deadline, cancel, stale generation and late reply. Lichuan C reads and frames are unchanged. Quick and full verification and required
CI on the exact final pushed commit pass. Hardware NOT TESTED. Commit, push and hand off.
