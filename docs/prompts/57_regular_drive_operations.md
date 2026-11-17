# Prompt 57 Regular enable, release, mode select and firmware identity policy

Apply [COMMON.md](COMMON.md). Prerequisites: [30](30_drive_actions.md), [42](42_cli_motion_routes.md), [45](45_fault_integration.md).

## Objective and boundaries

The user's requirements, verbatim (2026-11-12): "Enable is experiment-only, and there is no 'free the shaft' command -
add these to be regular commands"; "No typed mode switch - do it (write/add prompts) - this is just functionality
(literally a function in the lib to call it, right?)"; "Motion is locked to the exact measured firmware - we dont need
this rule"; "I want all of the above additions/functionality to be still accessed through the lib api so another
firmware can use it".

Added 2026-11-17. The user, verbatim, on the bench's per-boot controlword rule: "whats controlword and whats it used
for"; on the recorded stop a released enable requires: "again - a nonsense complication"; and generally: "In general
we are after good engineering practises and more responsibility on the end user of this firmware with possible
optional settable guards"; and on saving: "remove this restriction, it could be saved", about the measured fact that
`1017` (heartbeat), the TPDO1 type, `2201` and `2500` revert at every power cycle without a save, so every session
starts with setup again ([HIL report](../reports/2026-11-16_hil_campaign.md)). Three complications go:

- The released enable's recorded-stop premise. Today `EnableStart::RELEASED_BY_RECORDED_STOP` (`Actions.h`) asks the
  application to state that a stop it observed left the drive released, `scripts/s2_bench.py` demands that stop's
  evidence directory (`--released-by-recorded-stop`, `check_stop_record`), and so every series after a power cycle
  first runs a 60 rpm HALT velocity run and a halted quick stop only to produce a record. The regular enable instead
  starts from a fresh report showing the released standstill (today's `prepareEnable` rule for that start: mode 1,
  `2100` released, `6041` bits 0..6 `0x50`, `1001`, `603F` and `606C` zero), and the application owns that
  responsibility. The bench runner's record requirement is removed in the 2026-11-17 change; this prompt removes the
  premise from the library gate and its documents.
- The bench's per-boot controlword history rule (`BenchRoute::begin`: a mode write refused after any controlword or
  motion write in this boot, an enable refused after motion writes), which forces one MCU boot per mode change and
  enable. The regular mode select's check is a fresh report showing the drive not enabled; the enable's is the fresh
  released or power-up report. No per-boot history is required.
- The save's mandatory review records. `prepareSave`/`prepareFactoryRestore` (`Persistence.h`; one `2300` write, 2
  saves and 1 restores, `Persistence.cpp`) require a `PersistenceReview` with nonzero `fixtureRecord`,
  `firmwareRecord`, `procedureRecord` and `authorizationRecord`, and the console's `persistence save parameters` only
  consumes a context armed externally (`ProbeCommissioning::armPersistence`), so neither the bench nor an application
  can simply save.

Deliver four regular library operations (mode select, enable, release, save) and one policy change, with the evidence
in the [motion capability report](../reports/2026-11-12_motion_capability_evidence.md) sections d, e and f:

- **Mode select:** `prepareModeSelect` with the existing `nextAction`/`advanceAction`, documented modes 1, 3 and 6
  only, written once while the drive is not enabled, settled when `6061` reads the new mode; never writes `6061`, never
  reads `6060` (WO, D04), no mode change while enabled (Q11). About 130 measured mode writes all succeeded. It replaces
  the raw `6060` writes in the examples (`AxisSession`, bench `MODE_ONE`/`MODE_THREE`). Undocumented codes stay in the
  D17 probe.
- **Enable:** a `MEASURED` enable gate beside CLOSED and ATTENDED_EXPERIMENT, opened by a reviewed record of the
  measured enables (111 succeeded, 2 early failures with known causes), so enable is a regular command. Its start is
  the fresh report alone: the released standstill or the measured power-up report, without a retained stop record or
  a per-boot word history (2026-11-17, above).
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
- **Save parameters:** a regular library command, `prepareSave` with the review records as optional, settable guards
  instead of mandatory fields, reachable from the console (`persistence save parameters` preparing its own context)
  and from the bench. It is explicit only, never implicit on startup, selection, probe or recovery (AGENTS.md); its
  evidence is the `2300` acknowledgement, a readback of the saved values and, when the user next power-cycles, the
  same values read before any write. Factory restore stays a separate explicit command, with the complete backup
  recommended as an optional guard. [Prompt 61](61_drive_configuration.md)'s save trials and per-node save count use
  this command.
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
   writes with it, and replace the bench's per-boot controlword history rule with the fresh not-enabled check.
2. The `MEASURED` enable gate and its reviewed record built from the retained enable evidence, started from the fresh
   released or power-up report alone; `EnableStart::RELEASED_BY_RECORDED_STOP` loses its retained-record premise (or is
   replaced by a released start), and the documents and runners stop asking for a stop record. The attended
   experiment gate stays for unmeasured states.
3. `prepareRelease` with word 6 from the measured predecessor states, its settlement rule and refusal elsewhere.
4. The identity policy in the position, velocity and homing gates and in enable, with the unqualified-firmware flag
   in every terminal result and in the console output.
5. Console commands `mode ID 1|3|6`, `enable ID`, `release ID` as regular commands; bench routes for the release
   trials.
6. Save parameters with optional review guards through the API, the console and a bench route; factory restore with
   its optional backup guard.

## Verify and exit

Host tests replay the retained enable and mode records, refuse a mode change while enabled and every undocumented
code, prove release never claims standstill, cover product-identity refusal versus firmware-identity flagging, and
cover the save with and without each review guard, a save never sent implicitly, and a refused or unacknowledged
`2300` write reported as uncertain without a repeat.
Run the native, firmware, stack and documentation gates and CI on the actual final commit. Bench: release from mode 1
idle and from the halted mode 3 state on both nodes; set `1017=1000` and save on both drives, read it back, and
confirm it after the user's next power cycle; then a short regression.

## Documentation and delegation

Update the drive actions, position, velocity and homing guides, the commissioning guide's persistence section, the CLI
contract, coverage, backlog (D04, D17, S20, S21),
CHANGELOG and Doxygen in the same stage. A stronger reviewer challenges the release settlement and the identity
policy; normal coding agents own disjoint tests. The lead integrates and runs combined checks. Record
`docs/reports/prompt_57.md`. Commit and sync on the current branch before handoff.
