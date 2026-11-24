# Prompt 57 report: regular enable, release, mode select and firmware identity policy

State: In progress; stages (a), (b) and (c) complete, stage (d) acceptance next. Run by the unattended Claude loop of 2026-11-23
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
- Raw `6060` mode writes: the only builder is `examples/common/BenchRoute.cpp:2622-2627` (routes `MODE_ONE`,
  `MODE_THREE`, `MODE_PROBE`); `AxisSession.cpp` does not write `6060`, it only exempts a `6060=1/6` download from
  its `motionWritten_` history (`:54-57`, `:275`, `:327`). `Velocity.cpp:318` has its own `VelocityPhase::MODE`
  write of `6060=3`. `Objects.h` has no `6060` descriptor (`Velocity.cpp:16`: the signedness claims conflict).
  Corrected in stage (a); slice 1 named `AxisSession.cpp` wrongly.

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

## Source audit (stage (a), 2026-11-24)

| Sibling | HEAD and state | Behavior adopted or rejected (path, symbol) |
| --- | --- | --- |
| MotorControl-RS | `44e7f4f559b3` (2026-11-23), four files dirty with concurrent RS work (`examples/common/RtuRunner.cpp`, `RtuRunner.h`, `examples/probe_cli/DiscoveryApp.h`, `test/discovery_app_test.cpp`); 141 commits since prompt 56's `58038c5`, none in `Actions.h/.cpp`, `ActionOperation.h`, `Persistence.h/.cpp`, `Discovery.h`, `Reads.h` | Enable `profiles/ess_rs/Actions.h` `prepareEnable` (unchanged since prompt 56): adopted the name, prepare/next/advance, the acknowledgement versus completion split and no replay; rejected its ungated start and one-bit settlement (CiA 402 enable is a controlword sequence judged by the statusword and needs the fresh released or power-up start). Release `prepareRelease` (unchanged): adopted the name, no standstill claim and an application-set observation window; rejected the ESS HOMED and brake-delay facts as CL86-C facts. Mode select: no RS counterpart; `prepareModeSelect` is CANopen-native. Identity `DriverSettings.cpp` `validateModelAndEffects`, `Persistence.cpp` `verifyPersistence`: adopted raw identity as evidence with the declared binding selecting the profile; rejected refusal on a firmware difference (the owner's no-firmware-lock direction). Save/restore `prepareSave`/`prepareFactoryRestore` (unchanged): adopted one explicit write, acknowledgement is not persistence, restart-only verification; RS keeps its guards mandatory, so MCC's optional guards diverge from RS by the owner's 2026-11-17 direction |
| FieldCore-node | `453db51f399a`, clean (one commit since `c40ea834`, no motor files) | `MotorOperation {None, Move, ReadState, UrgentStop}` (`include/TunnelMonitor/contracts/MotorControl.h:15`); `Rs485MotorModule::setEnabled` (`src/devices/rs485_motor/Rs485MotorModule.cpp:648`) is a local admission flag that sends nothing. Adopted: module admission stays separate from drive enable. FieldCore never calls a drive enable or release |

## Evidence check (stage (a))

Original pages rendered and read (EN86 p11-12, p15, p18; ZH86 p11-14, p15, p18; OP86 p10):

- Modes 1, 3, 6: confirmed (EN86 p11, ZH86 p11).
- `6060` write-only: contradicted by the manual tables, which list `6060` RW; the WO access comes from the EDS
  (`LC_StepDriverCAN_V1.1.eds:1862`) and the D04 policy (`docs/backlog.md:127`). The prompt now cites the EDS.
- `6061` readback: confirmed (ZH86 p12-14; RO in both p11 tables; EN86's "set 6061" is the known mistranslation).
- No manual release word: confirmed; `6040=0` appears only paired with status low bits 000. OP86 p10 carries a CL57-C
  header and is not independent CL86-C evidence.
- `2300` 1 restores, 2 saves: confirmed (EN86/ZH86 p18; p15 also says 2 saves).
- Fault reset `15->128`: confirmed on ZH86 p12 and OP86 p10, absent from EN86 p12; EN86/ZH86 p15 say set bit 7, then
  clear it manually. Row 3's `0x0080` alone follows the measured trials, not that documented two-step procedure.

Release table rows against the retained evidence:

- Row 1 confirmed: 21 power-up enables `8437/0001` -> `8031/0011` -> `8033/0001` -> `8437/0001` (node 1 trials 108,
  206, 405, 704, 824, 1004, 1104, 1305, 1404, 1504, 1604, 1703; node 2 trials 118, 210, 712, 827, 1106, 1410, 1510,
  1610, 1723; `docs/bench/*-p3-node*`); free shaft after trial 103 (`docs/bench/20261103T0833Z-p3-node1`).
- Row 2 confirmed except `2500`: `docs/bench/20261113-ladder/night.log:8607-8620` (5976), `:8763-8800` (5990/5991),
  `packed/S0193-n2-disable.tar.xz`. Trial 5991 did not read `2500`; open loop (`2500=1`) is inferred from history
  (written and read back by trial 5973, two later refused writes, set to 2 by trial 5996). The library still requires a
  fresh `2500=1` for this row; the row's evidence scope records the inference.
- Row 3 confirmed on node 1: archives `packed/20261113T141347Z-cw-0000-n1-5402`, `...141402Z-cw-000b-n1-5403`,
  `...141417Z-cw-0006-n1-5404`, `...141445Z-cw-0080-n1-5405` with the `cw-campaign*.jsonl` ledgers; trial numbers
  5402-5405 are reused by node 2 runs in `night.log:3113-3143`, so citations use the directory names. The "8 of 8" are
  trials 5362, 5396, 5452, 5500, 5524 (node 1) and 5632, 5650, 5668 (node 2), `night.log:2728, 3067, 3644, 4131,
  4370, 5479, 5660, 5841`.
- Rows 4 and 5: the latched `4050/0014` and `C050/0015` have `2100` running set as well as released set; the table did
  not state which row wins.
- Counts: about 130 mode writes confirmed (131), of which 23 are power-up writes from `8037/0001` in mode 0, which the
  evidence report's section d admitted and the prompt dropped; 111 enables with 2 early failures confirmed (30 records
  103-2402, 28 succeeded, 103 and 105 failed; 83 in `docs/bench/20261111-night/night.log`).
- `docs/drive_actions.md` is stale: "Release: pending, with no API" (lines 286-308) says no release word was tried;
  `scripts/s2_bench.py` still accepts `--released-by-recorded-stop` (lines 109, 680, 869).

## Lead decisions (stage (a))

1. Mode select also starts from the measured power-up report `8037/0001` in mode 0 (23 writes, evidence report section
   d), besides a not-enabled report. Without it a freshly powered drive admits no mode select, release (row 1 needs
   mode 1) or enable. The prompt text now says so.
2. Release row 4 precedes row 5: a report with `2100` released set answers "already released" with no TX whatever the
   running bit; the result reports the running bit and never claims standstill. Nothing is sent, so nothing is risked.
3. Release row 2 stays; admission requires a fresh `2500=1`; the row's evidence note records that trial 5991's open loop
   was inferred from history.
4. Persistence: the four `PersistenceReview` records become optional, settable save guards, off by default ("remove this
   restriction, it could be saved"). The restore's backup guard is settable and on by default because a restore is
   destructive; `routeBackRecord`, which the prompt did not name, folds into that backup guard. Verification after the
   owner's power cycle stays the evidence of persistence.
5. Identity: the position, velocity and homing gates refuse on the product fields only (`1000`, `1018:01`, `1018:02`,
   `2000`) and flag the firmware fields (`1009`, `100A`, `1018:03`, `2001`) as "firmware unqualified" with both
   identities; `prepareEnable` gains an identity input (an API change) under the same rule; the Identity policy bullet
   (outcomes, admission by outcome, stop exemption, NMT bootstrap) governs; the CL86-C/OL86-C binding keeps its
   existing model check.
6. The Lichuan C extensions disposition map does not exist (`docs/reference/lichuan_operations.json` schema 2 has no
   model columns), so that conditional update does not apply now.

## Implementer brief for stage (b)

One implementer (Opus, xhigh) owns the code for all slices; the lead integrates, runs combined checks and commits.
Slices, each a coherent checked commit:

- b1 Mode select: `ActionKind::SELECT_MODE`, `prepareModeSelect` (modes 1, 3, 6; start not enabled or the measured
  power-up `8037/0001` mode 0; one `6060` write, settled when `6061` reads the mode; never reads `6060`, never writes
  `6061`; refuses undocumented codes and while enabled), plumbing in `Actions.cpp` (`commandCount :14-16`,
  `observationMask :72-81`, kind bound `:84`, `command() :195-220`, `finishCycle :367-520`); replace
  `BenchRoute.cpp:2622-2627` and the per-boot rule (`:1140-1144`, `predecessor() :2734-2737`, `modeAdmitted_ :2558`)
  with the fresh check; `s2_bench.py` `mode1`/`mode3`; `docs/reference/lichuan_operations.json` (`:342-360`, console
  bindings, `manual.mode_selection :1519`) so `scripts/check_lichuan_operations.py` passes; tests.
- b2 Enable: `EnableGate::MEASURED` with its reviewed record built from the retained enables; the released start from a
  fresh report alone, also `8031/0011` after word 6 (today's `0x50` rule at `Actions.cpp:872` refuses it); remove the
  recorded-stop premise (`EnableStart::RELEASED_BY_RECORDED_STOP`, `s2_bench.py --released-by-recorded-stop`,
  `check_stop_record`), keep `ATTENDED_EXPERIMENT` for unmeasured states; `test/enable_action_test.cpp` and the
  session, bench-route and console tests the survey lists.
- b3 Release: `ActionKind::RELEASE`, `prepareRelease` with the table in one place (rows 1 to 5 with decisions 2 and 3),
  settlement from the drive's report, refusal before TX, the already-released answer, the explicit experiment option
  and its limits, a lost reply kept UNKNOWN and never resent; tests asserting exact wire words per row.
- b4 Identity policy (decision 5) in `Position.cpp:179-184, 381-385, 999-1002`, `Velocity.cpp:55-60, 223-227, 628`,
  `Homing.cpp:61-66, 193-197`, and `prepareEnable`; the unqualified-firmware flag in every terminal result; the mismatch
  tests (`move_absolute_test.cpp:325`, `velocity_test.cpp:309, 741`, `homing_test.cpp:337`, `MoveRig.h`,
  `VelocityRig.h`).
- b5 Save and restore with optional guards (decision 4), `Persistence.cpp:19-24, 99, 160, 299-300`; the console's
  `persistence save parameters` prepares its own context (`ProbeConsole.cpp:2580-2650`,
  `ProbeCommissioning.cpp:68-75, 295`); a bench save route; `persistence_test.cpp`,
  `probe_commissioning_app_test.cpp`.
- b6 Console and documents: `mode AXIS_ID 1|3|6`, `enable AXIS_ID` (today `ProbeConsoleMotion.cpp:627` takes no axis),
  `motor-release AXIS_ID` replacing the caps-only row (`ProbeConsole.cpp:171`), `release OPERATION_ID` unchanged
  (`:87`); `docs/cli_contract.md`, `docs/api_cli_mapping.md`, `scripts/console_scripts/local_routes.txt:210`,
  `test/motion_console_test.cpp:240-248`, `test/bench_probe_cli_test.py:101`, `scripts/bench_probe.py`, the three
  example READMEs; the drive actions, position, velocity, homing and commissioning guides, coverage, backlog (D04, D17,
  S20, S21), CHANGELOG and Doxygen.

Stage (c): a new reviewer; the release settlement and the identity policy go to a stronger reviewer (Fable, high).

## Stage (b) implementation (2026-11-24)

One implementer (Opus, xhigh) delivered the six slices; the lead checked each on a clean worktree of its exact commit
(`python scripts/verify.py --mode quick`, `check_promptset.py --check-git-ref HEAD`, `check_lichuan_operations.py`) and
pushed it.

| Slice | Commit | Content | CI |
| --- | --- | --- | --- |
| b1 Mode select | `cb8f0ec8` | `ActionKind::SELECT_MODE`, `prepareModeSelect` (modes 1, 3, 6; not-enabled or the power-up `8037/0001` mode 0 start for mode 1 only, the 23 measured writes; already-reported mode answered with nothing sent); bench `MODE_ONE`/`MODE_THREE` through the session; per-boot history rule replaced | verify.yml on later commits |
| b2 Enable | `92dbdb55` | `EnableGate::MEASURED` from the fresh power-up or released report (`0x50` or `0x31`, so `8031/0011`); recorded-stop premise removed (`RELEASED_BY_RECORDED_STOP` a deprecated alias); `--released-by-recorded-stop` removed; mode 6 stays attended | verify.yml on later commits |
| b3 Release | `d9956317` | `ActionKind::RELEASE`, `prepareRelease` on the pure `classifyRelease` table; row 4 first; settlement from the row's report; experiment option `0x0000`/`0x0006` | verify.yml on later commits |
| b4 Identity | `3d1f01c5` | Product fields refuse, firmware fields run with `IdentityCheck.firmwareUnqualified`; `prepareEnable` takes an `IdentityObservation` (API change); references are the two measured specimens, EDS values reported | run 45413647691 succeeded |
| b5 Save and restore | `821f51ed` | Review records settable guards (off by default); restore backup guard on by default; console save prepares its own context; S2 save route `Ts` | run 45422433482 succeeded |
| b6 Console and documents | `3586ac2b` | `mode`, `enable AXIS_ID`, `motor-release AXIS_ID`; `firmware_unqualified` in every motion terminal; `restore ... without-backup`; release route `Tr` with `s2_bench.py motor-release`; contracts, guides, coverage, backlog | pending |

Implementer decisions accepted by the lead and open for the stage (c) reviewer: mode select and release take no
identity input (they do not move the motor; CONTRADICTED is not produced while the profile has no candidate set);
`enable` requires the axis ID (grammar change); the console relies on application-named reports for these commands
like the other motion commands, so on the S2 they run as bench routes; the S2 save route allocates its work area on
first use from internal memory (allocation failure refuses the route).

## Stage (c) independent review and fixes (2026-11-24)

Two new read-only reviewers checked the stage (b) diff at `3586ac2b`: release settlement and identity policy (Fable,
high) and mode select, enable, save/restore, console and documents (Opus, xhigh). The lead verified each finding; the
implementer fixed all accepted items with a regression test each; commit `d06eeb0e`, clean-worktree verify quick,
promptset, ledger and `s2_bench_test.py` passed; the implementer also ran the s2 and idf-s2 firmware builds.

| Finding | Severity | Disposition |
| --- | --- | --- |
| Mode select and release sent with no identity (reproduced) | High | Fixed: both take an `IdentityObservation` with enable's rule; already-selected and already-released answers send nothing. Reverses the stage (b) decision |
| No shipped image could reach `mode`, `enable N`, `motor-release N` or the console save (confirmed by search) | High | Fixed: the console commands read their own report (`ProbeMotion::startAction`, `SelectedAxis.h`), the bench console attaches commissioning, the S2 power-up enable is MEASURED without a typed shaft observation, `s2_bench.py save --persistent` |
| Measured enable on an enabled drive after the session's own `000F` sent word 6 | Medium | Fixed: answered as already enabled, success with nothing sent |
| Mode select admitted report shapes beyond its contract (reproduced) | Medium | Fixed: released standstill, or running only as the exact quick-stop latch; undocumented and soft-limit bits refused |
| Release ignored `6041` bits 3 and 7 (reproduced) | Medium | Fixed: refused as an alarm, never overridden by the experiment option |
| A move replacement dropped the original's firmware flag (reproduced) | Medium | Fixed |
| Release row 2 could use a `2500` read older than the last write (inferred) | Low | Fixed |
| Enable had one hard-coded reference; a documented-only match reported CONFIRMED | Low | Fixed: optional caller MEASURED reference (`ActionOptions::identityReference`); documented-only match is UNCONFIRMED |
| RELEASE route not classed as motion | Low | Fixed in the bench and the runner |
| Stale documents and test gaps | Low | Fixed (backlog S08, source map, commissioning `without-backup`, first-token CLI guard, `firmware_unqualified:false` test) |

Estimated, not measured on a board: `SelectedAxis` about 60 KB of internal RAM on first selection, the bench console
helper about 13.5 KB on first arming, about 2 KB per bench node; an allocation failure refuses the command with no TX.
