# Prompt 57 report: regular enable, release, mode select and firmware identity policy

State: In progress, stage (a) (read and assess). Run by the unattended Claude loop of 2026-11-23
([COMMON "Owner absence", Schedule](../prompts/COMMON.md#owner-absence-20-november-to-about-4-december-2026)).

## Starting point

- Starting revision: `9669d3e3` on `main` (equal to `origin/main` at the start of this stage).
- Prompt: [57_regular_drive_operations.md](../prompts/57_regular_drive_operations.md), read in full.
- Consumed artifacts and their states in [PROGRESS](../prompts/PROGRESS.md):
  - 30 Drive state enable and release: Partial. The gated enable (`EnableGate::ATTENDED_EXPERIMENT`, closed by
    default) exists and succeeded on both specimens in the attended session; release is absent. Its enable code is
    the artifact this prompt extends. [Report 30](prompt_30.md).
  - 42 Motion CLI and platform parity: Complete. `motor-release` is a caps-only row gated on D-17; this prompt
    replaces it. [Report 42](prompt_42.md).
  - 45 Adversarial ownership and motion integration: Complete software audit. [Report 45](prompt_45.md).
- Evidence the prompt cites, present at the starting revision:
  [motion capability evidence](2026-11-12_motion_capability_evidence.md) sections d, e, f;
  [enable fault state](2026-11-13_enable_fault_state.md) (backlog S21).

## Baseline in the code (checked at the starting revision)

- `include/MotorControlCAN/ActionOperation.h` `ActionKind`: `STOP`, `CLEAR_ALARM`, `SET_DEVICE_POSITION`, `ENABLE`.
  No `SELECT_MODE` and no `RELEASE` exist yet.
- `include/MotorControlCAN/profiles/lichuan_c/Actions.h` `EnableGate`: `CLOSED` (default), `ATTENDED_EXPERIMENT`.
  No `MEASURED` gate. `EnableStart` includes `RELEASED_BY_RECORDED_STOP` (the retained-stop premise this prompt
  removes) and defaults to `UNKNOWN` (refused).
- `include/MotorControlCAN/profiles/lichuan_c/Persistence.h` `PersistenceReview`: mandatory nonzero
  `fixtureRecord` and the other review records; `prepareSave` and `prepareFactoryRestore` exist and consume them.
- Raw `6060` mode writes remain in `examples/common/AxisSession.cpp` (and the bench `MODE_ONE`/`MODE_THREE` routes
  the prompt names).

## Deliverables (from the prompt)

1. `ActionKind::SELECT_MODE` and `prepareModeSelect` (modes 1, 3, 6; written once while not enabled; settled on
   `6061`; never reads `6060`, never writes `6061`); examples and bench use it; the bench's per-boot controlword
   history rule is replaced by the fresh not-enabled check.
2. A `MEASURED` enable gate from the reviewed record of the retained enables, started from the fresh released or
   power-up report alone (also the `8031`/`0011` report a release leaves); the recorded-stop premise goes.
3. `ActionKind::RELEASE` and `prepareRelease` implementing the release table exactly, with settlement from the drive's
   report, refusal before TX of every other report, the already-released answer without TX and the explicit
   experiment option; prompt 72's guard will call it.
4. The identity policy (the Identity policy bullet: outcomes, admission by outcome, stop exemption, the NMT
   bootstrap command, the vendor-object and motion rules) in the position, velocity, homing and enable gates, with
   the unqualified-firmware flag in every terminal result and in the console output.
5. Console `mode AXIS_ID 1|3|6`, `enable AXIS_ID`, `motor-release AXIS_ID`; `release OPERATION_ID` keeps its meaning;
   the inventory, JSON records, contracts, READMEs, scripts and tests listed in the prompt change together.
6. Save parameters with optional, settable review guards (API, console, bench route); factory restore with its
   optional backup guard and stated backup scope.
7. If the Lichuan C extensions stage 02 map exists at landing, update its release and mode-select cells (prompt text).

## Owner absence (COMMON)

- The bench step "set `1017=1000` and save on both drives, read it back, and confirm it after the user's next power
  cycle" is persistent and needs the owner's power cycle: deferred to the owner's return (deferred table).
- The release trials and the re-enable after each release run under the written waiver only if they leave no state
  that only a power cycle clears; stage (d) decides each trial against COMMON and the current bench records, and a
  trial that cannot meet the rules stays NOT RUN with its reason.

## Planned checks

`python scripts/check_promptset.py`; `git diff --check`; `python scripts/verify.py --mode quick` and `--mode full`;
the Doxygen gate; the firmware gates of [verification.md](../verification.md) (public headers and examples change);
the console tests the prompt names (`test/motion_console_test.cpp`, `test/bench_probe_cli_test.py`); CI on the final
pushed commit.

## Still to do in stage (a)

- The recurring source audit: MotorControl-RS HEAD, dirty state and its `prepareEnable`, `prepareRelease`, mode and
  identity seams; FieldCore-node read-only for enable and release use.
- Reopen EN86/ZH86 p11-12 (`6060`/`6061`, controlword table), the drive actions guide, `Actions.h`,
  `Position.cpp`, `Velocity.cpp`, `Homing.cpp` identity checks and the bench records the evidence report cites.
- The implementer brief for stage (b), including which deliverables a stronger reviewer challenges (release
  settlement and the identity policy).
