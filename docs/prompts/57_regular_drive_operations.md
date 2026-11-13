# Prompt 57 Regular enable, release, mode select and firmware identity policy

Apply [COMMON.md](COMMON.md). Prerequisites: [30](30_drive_actions.md), [42](42_cli_motion_routes.md), [45](45_fault_integration.md).

## Objective and boundaries

The user's requirements, verbatim (2026-11-12): "Enable is experiment-only, and there is no 'free the shaft' command -
add these to be regular commands"; "No typed mode switch - do it (write/add prompts) - this is just functionality
(literally a function in the lib to call it, right?)"; "Motion is locked to the exact measured firmware - we dont need
this rule"; "I want all of the above additions/functionality to be still accessed through the lib api so another
firmware can use it".

Deliver three regular library operations and one policy change, with the evidence in the
[motion capability report](../reports/2026-11-12_motion_capability_evidence.md) sections d, e and f:

- **Mode select:** `prepareModeSelect` with the existing `nextAction`/`advanceAction`, documented modes 1, 3 and 6
  only, written once while the drive is not enabled, settled when `6061` reads the new mode; never writes `6061`, never
  reads `6060` (WO, D04), no mode change while enabled (Q11). About 130 measured mode writes all succeeded. It replaces
  the raw `6060` writes in the examples (`AxisSession`, bench `MODE_ONE`/`MODE_THREE`). Undocumented codes stay in the
  D17 probe.
- **Enable:** a `MEASURED` enable gate beside CLOSED and ATTENDED_EXPERIMENT, opened by a reviewed record of the
  measured enables (111 succeeded, 2 early failures with known causes), so enable is a regular command.
- **Release (free the shaft):** `prepareRelease` (the RS name) writing word 6 (`0006`), which released the drive in
  every measured power-up enable and left node 1's shaft free by hand; settled from the drive's report, never by TX.
  No manual names the shaft-free word, so the record states that this is measured, not documented. Release from mode 1
  idle and from the halted mode 3 state are bench trials (the halted case may also resolve backlog S20).
  Added 2026-11-13 (backlog S21, [report](../reports/2026-11-13_enable_fault_state.md)): release must also cover the
  switched-on, stationary, alarm-free state a refused enable leaves (`6041=8033`, `2100=0001`, energized), which no
  library action admitted: the stop refuses a stationary report and the clear needs an alarm. There `6040=0x0000`
  (disable voltage) released node 2 once (trial 5991, to `8050`/`2100=0011`); word 6 from `8033` is unmeasured, and
  from `8010` (after a `2301` clear) `0x0000`, `0x0006` and `000B` did nothing while `0x0080` released it. Choose
  the word per measured predecessor state, refuse unmeasured ones, and give [prompt 72](72_enable_fault_guards.md)'s
  word 7 guard this release.
- **Identity policy:** motion stays refused when the product identity differs (`1000`, `1018:01` vendor, `1018:02`
  product, `2000`), because another product is another profile. A different firmware identity (`1009`, `100A`,
  `1018:03`, `2001`) no longer refuses: the operation runs and its terminal result carries "firmware unqualified" with
  both identities, so qualification stays honest without locking the user out.

Boundaries: no implicit enable on startup, selection, probe or recovery; release is not a stop and never claims
standstill; a mode select never enables; uncertainty after an unconfirmed write is kept; the console gets no logic of
its own (it calls these operations).

## Read and audit

For this prompt, refresh MotorControl-RS HEAD and dirty state and read its `prepareEnable`, `prepareRelease`, mode and
identity seams; RS is authoritative for common names. Inspect FieldCore-node read-only for enable/release use. If HEAD,
dirty state and seam match the last recorded row, write "unchanged since prompt N" and do not reread. A sibling row
must name a behavior adopted or rejected, with path and symbol. This does not relax vendor evidence, safety,
uncertainty or verification rules. Reopen EN86/ZH86 p11-12 for `6060`/`6061` and the controlword table, the drive
actions guide, `Actions.h`, `Position.cpp`, `Velocity.cpp`, `Homing.cpp` identity checks, and the bench records the
report cites. Apply [CONVENTIONS.md](CONVENTIONS.md), [MULTIDRIVE.md](MULTIDRIVE.md),
[DOCUMENTATION.md](DOCUMENTATION.md), [MAINTAINABILITY.md](MAINTAINABILITY.md) and [DELEGATION.md](DELEGATION.md).

## Implement

1. `ActionKind::SELECT_MODE` with `prepareModeSelect`, its evidence and terminal result; replace the example raw mode
   writes with it.
2. The `MEASURED` enable gate and its reviewed record built from the retained enable evidence; the attended experiment
   gate stays for unmeasured states.
3. `prepareRelease` with word 6 from the measured predecessor states, its settlement rule and refusal elsewhere.
4. The identity policy in the position, velocity and homing gates and in enable, with the unqualified-firmware flag
   in every terminal result and in the console output.
5. Console commands `mode ID 1|3|6`, `enable ID`, `release ID` as regular commands; bench routes for the release
   trials.

## Verify and exit

Host tests replay the retained enable and mode records, refuse a mode change while enabled and every undocumented
code, prove release never claims standstill, and cover product-identity refusal versus firmware-identity flagging.
Run the native, firmware, stack and documentation gates and CI on the actual final commit. Bench: release from mode 1
idle and from the halted mode 3 state on both nodes, then a short regression.

## Documentation and delegation

Update the drive actions, position, velocity and homing guides, the CLI contract, coverage, backlog (D04, D17, S20),
CHANGELOG and Doxygen in the same stage. A stronger reviewer challenges the release settlement and the identity
policy; normal coding agents own disjoint tests. The lead integrates and runs combined checks. Record
`docs/reports/prompt_57.md`. Commit and sync on the current branch before handoff.
