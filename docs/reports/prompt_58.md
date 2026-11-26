# Prompt 58 report: measured records through the library API and the console

State: In progress; stages (a), (b) and (c) complete, stage (d) acceptance next. Run by the unattended Claude loop of 2026-11-23
([COMMON "Owner absence", Schedule](../prompts/COMMON.md#owner-absence-20-november-to-about-4-december-2026)).

## Starting point

- Starting revision: `8a27d58a` on `main` (equal to `origin/main`).
- Prompt: [58_measured_records.md](../prompts/58_measured_records.md), read in full.
- Consumed artifacts: prompt 42 (Complete) and prompt 57 (Partial, software complete at `fd73d198`, CI 45472978832):
  `EnableGate::MEASURED`, `IdentityValues`/`checkIdentity` (`Reads.h:239, 311, 321`), `IdentityReference` and
  `ActionOptions::identityReference` (`Actions.h:96-109`), and the console commands of [report 57](prompt_57.md).

## Source audit (stage (a), 2026-11-25)

| Sibling | HEAD and state | Behavior adopted or rejected (path, symbol) |
| --- | --- | --- |
| MotorControl-RS | `b20b8dbb11f0` (2026-11-25), clean; 9 commits since prompt 57's `44e7f4f559b3`, none in `include/` or `src/`; the actions seams are unchanged since prompt 57 | Caller-qualified fact structs `ESS_RS::MovePrerequisites` (`Position.h:88`), `StoredPositionQualification` (`:51`, binding plus `qualifiedUs`/`maximumAgeUs`), `VelocityPrerequisites` (`Velocity.h:11`, "cannot broaden"), `HomePrerequisites` (`Homing.h:85`); example console `QualificationRoute`/`QualificationCommand` (`examples/probe_cli/QualificationConsole.h:18, 21`) and `install`/`inspect`/`revokeHomeQualification` (`ProbeApp.h:417-425`); `VelocityPromoted.h:12` ("each entry changes only in its own commit with its evidence"). Adopted: install/inspect/revoke semantics (here load/show/use/drop), RAM-only state, invalidation on rebinding, recovery and restart, never broadening, per-entry evidence provenance. Rejected (diverges from RS by the user's 2026-11-12 requirement): RS has no text record codec, version, CRC or storage hooks; MCC adds them |
| FieldCore-node | `453db51f399a`, clean, unchanged since prompt 57 | Settings belong to the application (`SettingsService`, FRAM records with magic, version and `storageCrc32`, `settings/SettingsPersistence.h`); motor facts come from commissioning and are never persisted. Adopted: the application owns storage; the library supplies only the codec and validation. It would not consume these records today (inferred) |

## Baseline (record types and how they are supplied)

- Position: `MoveGate` (`Position.h:113`), `MoveFirmware` (`:148`), `MoveEvidence` (`:177`; acceptance/completion
  `MoveSignal`, `CompletionRule`, `RelativeBase`), validated in `checkMove` (`Position.cpp:319`). No speed, ramp or
  distance range fields.
- Velocity: `VelocityGate` (`Velocity.h:17`), experiment caps 60 rpm / 20 s (`:30, 35`), `VelocityEvidence` (`:73`),
  `VelocityChangeEvidence` (`:114`), validated in `check` (`Velocity.cpp:151`). No minimum rpm, duration, ramp-range,
  per-stop-kind or `2201` scale field; MEASURED refuses `enableWindowUs` (`Velocity.cpp:198`).
- Homing: `HomeGate` (`Homing.h:26`), `HomeEvidence` (`:49`); no shipped example opens it.
- Enable: `EnableGate::MEASURED` (`Actions.h:214`), `EnableEvidence` (`:311`); the record is compiled into the library
  (`measuredIdentity()`, `Reads.h:295`); only `ActionOptions::identityReference` is loadable.
- Supply today: `ProbeMotion::set{Move,Velocity,Home,Enable,Action}Prerequisites` (`ProbeMotion.h:158-222`) copy the
  evidence without validating it (preparation validates); the console's "home qualification" and "velocity
  qualification" rows say "No console-created qualification" (`ProbeConsole.cpp:272-283`); the bench `Y` and `B` lines
  (`BenchRoute.h:614-618`, `BenchRoute.cpp:427, 456, 545-546`; `BenchConsole.cpp:447, 477, 552`) load records from text,
  `B` only as attended experiment, line integrity CRC-32 IEEE (`routeLineCrc`, `BenchRoute.cpp:987`, example-only).
- Storage: nothing persistent on the S2; no example uses NVS, Preferences or a file for records.
- Checker obligations for a new public header (`scripts/check_lichuan_operations.py:284-382`): a ledger family in
  `docs/reference/lichuan_operations.json` with the exact function inventory, registered tests calling each function,
  `api_only_reason`, a working console route or `console_api_only_reason`, Python scenarios or `python_api_only_reason`;
  and registration in `CMakeLists.txt`, `scripts/check_repository.py`, `test/repository_test.py`,
  `test/package_test.py`, `scripts/check_stack.py` and `examples/common/firmware_sources.json`.

## Evidence for the reviewed records (stage (a) scan of all retained raw records)

All 8708 `rx.txt` captures, loose and packed, and the result ledgers were scanned.

- Identity, MEASURED: 648 identical identity read sets on nodes 1 and 2 (`1000=00040192`, `1009=0C`, `100A=20`,
  `1018:01=1000`, `1018:02=1`, `1018:03=0`, `2000=0400`, `2001=0200`; e.g. `docs/bench/20261110T0948Z-s17-velocity-node1-1407/rx.txt:20-28`).
- Every velocity and enable run on hardware used `ATTENDED_EXPERIMENT`; no MEASURED velocity or enable gate has run;
  `MoveGate::MEASURED` has.
- Enable, mode 1: 177 successful enables (node 1 111, node 2 66) from five start reports: power-up `8437/0001` (21),
  `4050/0014` (62), `8050/0011` (44), `C050/0015` (44), `0050/0010` (6); failures 103 and 105 (2026-11-03); `8031/0011`
  as a start is inferred (trial 105 is the only one and failed under the old position rule). Mode 6 unmeasured.
- Position completion: acceptance STATUSWORD `0x1000/0x1000`, completion MONITOR `0x0001/0x0001`, rule
  `AFTER_ABSENCE`, base `ACTUAL` (records 111, 409, 830). 232 library completions, every move without a stop completed:
  2-3000 rpm, ramps 100-2000 ms, 100-169410 counts; dense at or below 300 rpm (220 moves), one move per step per node
  from 600 to 3000 rpm; absolute moves through the library only 3 (at or below 60 rpm, 2000 counts).
- Velocity: 334 records, all attended, ZH86 layout, mode 3: forward 6-60 rpm dense (rounds 7-10 and the night soak:
  180 runs, ramps 200 ms, 12 s), above 60 rpm range-probe runs only; cruise 0.987-0.992 of the command; the enable
  report arrived 0.70-0.77 s after word 15 in all 315 captured runs; HALT settled at 6-300 rpm (and 4000-4500 rpm on
  single runs), quick stop settled only at 6-60 rpm (147 runs; every quick stop above 60 rpm ended DEADLINE); FF05 at
  4250 rpm (node 1) and 4625 rpm (node 2) with `2201=1000`, both fault at 4000 rpm with 200 ms ramps; reverse sign
  measured, physical direction not observed.
- Homing: 11 admission records, all refused before any write; no switches wired; no record possible.
- Summaries that disagree with the raw records (stage (b) corrects them): the enable count (177, not 111; the start list
  lacks `0050/0010`), quick stops above 60 rpm called "clean" while the library ended them DEADLINE, "4000 ok, FF05 at
  4500" fits node 1 only roughly, velocity speeds "6, 30, 60 only" (round 9 also ran 15 and 45), the night soak's "308
  completed moves" includes 113 raw bench routes, and `results.jsonl`'s `last` field is the admission report.

## Lead decisions (stage (a))

1. Record format (deliverable 1): one line of `key=value` fields with `v=1` and a trailing CRC-32 (IEEE, zlib-compatible,
   bitwise, no table, no heap) over the preceding text; the codec lives in the Lichuan C profile; the bench's
   `routeLineCrc` may then call the same function. Decode validates the whole record and publishes nothing on failure,
   with a specific refusal reason per field.
2. A record carries its kind (enable, position, velocity), its basis per field (MEASURED, DOCUMENTED, INFERRED), the
   identity tuple it was measured on (prompt 57 policy: product fields must match, firmware differences are reported),
   its evidence reference, and its ranges: for position the rpm, ramp and distance range; for velocity the rpm range,
   direction, ramp range, maximum duration, `2201` scale, stop kinds with their rpm range and the enable-report window;
   for enable the mode and the admitted start reports. The record layer checks a request against those ranges before
   the library's own preparation, which still applies; a record never widens a library limit.
3. Velocity under MEASURED may use an enable-report window up to the record's stated bound (the measured arrival is
   0.70-0.77 s after word 15); without it a MEASURED run would fail after word 15.
4. Reviewed records (deliverable 4), conservative:
   - enable: mode 1, the five measured start reports, identity as measured; mode 6 stays attended;
   - position completion: 2-300 rpm, ramps 100-2000 ms, up to 169410 counts, relative moves with base `ACTUAL` and the
     measured acceptance and completion signals; faster or absolute moves beyond the measured three stay attended;
   - velocity: forward 6-60 rpm, 200 ms ramps, at most 12 s, `2201=1000`, released starts, HALT and quick stop within
     6-60 rpm; the HALT start from the `0014` latch is excluded (S20); everything else stays attended. The stronger
     reviewer of stage (c) checks every value against the raw records.
   - homing: no record.
5. Storage hooks belong to the examples (ESP-IDF NVS, Arduino Preferences, native file); records stay in RAM unless
   saved explicitly; nothing is restored automatically at boot; a loaded record never enables or moves.

## Implementer brief for stage (b)

One implementer (Opus, xhigh) owns the code for all slices; the lead integrates, verifies each slice with
`verify.py --mode full` (or quick plus `test/package_test.py`) on a short clean worktree, and commits it.

- b1 Codec: the record types, encode/decode, CRC-32, whole-record validation with refusal reasons, round trip, CRC and
  version refusal, partial and out-of-range records refused; the public header with its ledger family and every
  registration the checker requires.
- b2 Range enforcement and the velocity enable window: request checks against a bound record before preparation (never
  widening), `enableWindowUs` under MEASURED up to the record's bound, tests that a record cannot widen a limit.
- b3 API: load, show, use (bind to an axis), drop, through the session (`ProbeMotion`) replacing direct
  `set...Prerequisites` for records; invalidation on rebinding, recovery and restart.
- b4 Console `record load|show|use|save|restore|drop` calling the API only; example storage hooks (NVS, Preferences,
  native file), no automatic restore; tests that the console uses only the API.
- b5 The reviewed records (decision 4) as committed text files with their evidence directories and review notes;
  documents (position, velocity, homing, drive actions guides, CLI contract, coverage, CHANGELOG, Doxygen) and the
  summary corrections listed above.

Stage (c): a stronger reviewer (Fable, high) checks every reviewed record against its raw evidence; a second reviewer
checks codec, API and console.

## Stage (b) slice b5: the reviewed records

Three reviewed records are committed under [docs/records](../records/), one canonical line per file, written only
by `reviewed_records_test --write` (`encodeRecord`); the registered test `reviewed_records` decodes each file,
checks every value against the reviewed one, requires the file to equal `encodeRecord`'s line byte for byte and
checks what each record opens. Beside each record a review note cites, for every included and excluded run, the
raw capture lines (`rx.txt`, loose or `archive!member`) and the result-ledger rows.

| Record | Values | Runs | Narrowed from decision 4 |
| --- | --- | --- | --- |
| [enable](../records/lichuan_c_enable_mode1.md) | mode 1; starts `8437/0001`, `4050/0014`, `8050/0011`, `C050/0015`, `0050/0010` | 177 enables (node 1 111, node 2 66); excluded 103, 105 (failed) and 52015 (refused before any write) | Nothing |
| [relative position](../records/lichuan_c_position_relative.md) | base `ACTUAL`, 6..300 rpm, ramps 100..1000 ms, up to 38200 counts, `6041` `0x1000` / `2100` `0x0001`, `AFTER_ABSENCE` | 217 completed library moves (node 1 169, node 2 48); every arrival within one count of start plus displacement | rpm 2 to 6 (the 2 rpm move, 121, is absolute); ramps 2000 to 1000 ms and 169410 to 38200 counts (both only at 1200 to 3000 rpm) |
| [forward velocity](../records/lichuan_c_velocity_forward.md) | ZH86 (documented), 6..60 rpm forward, ramps 200 ms, at most 12 s, `2201=1000` INFERRED, HALT 6..60, quick stop 6..40, window 2 s, starts `8050/0011`, `C050/0015` | 104 attended runs (node 1 81, node 2 23) | Quick stop 60 to 40 rpm (6 night-soak quick stops at 45 to 60 rpm ended `DEADLINE` unsettled); starts without `4050/0014`, because a record's starts cover both stop kinds and decision 4 excludes the HALT start from that latch |

Decision needed: no velocity run in the record read `2201` (reads of `1000` on 2026-11-09 and 2026-11-13 bracket
them, and the first retained write is on 2026-11-13 at 14:26Z), so its basis is INFERRED, and `makeVelocityEvidence`
refuses a record whose `2201` is not MEASURED. The committed velocity record therefore opens nothing; the test
checks that refusal. Either a run on this identity reads `2201` in its own capture and a new record states it
MEASURED, or the rule is relaxed by a reviewed decision. The position and enable records open their routes.

Lead decision (2026-11-26): the rule stays. A gate fact a record opens must be MEASURED, so the velocity record is
kept as committed, with `2201=1000` INFERRED, and opens nothing. The next measurement is an attended-experiment
velocity run on this identity (6 to 40 rpm, 200 ms ramps) whose own capture reads `2201` before and after the run;
it is read-only apart from the run itself, so COMMON's owner-absence rules allow it unattended. Prompt 66's hardware
qualification owns that run; a new record states `2201` MEASURED only from it.

Recount against the stage (a) evidence above: 687 identity read sets, not 648 (39 captures name no node; all
identical); quick stops at 45 to 60 rpm did not all settle (above); the enable report arrived 0.702 to 0.749 s after
word 15's acknowledgement in the 104 included runs. Not covered by any record: absolute moves, homing, mode 6,
reverse velocity, the record format's missing minimum travel (a relative record admits any displacement up to its
bound; the shortest measured was 100 counts) and combinations inside a record's box that never ran together.

### Summary corrections (slice b5)

Each correction names the old claim, the raw evidence and the replacement; documents that keep corrections carry a
dated note beside the change. Raw evidence (`docs/bench`) was not edited.

| Document | Old claim | Raw evidence | Replacement |
| --- | --- | --- | --- |
| `docs/drive_actions.md`, `docs/coverage.md`, `Actions.h` (`EnableGate::MEASURED`), `docs/prompts/57_regular_drive_operations.md`, `docs/reports/prompt_57.md`, `docs/reports/2026-11-12_motion_capability_evidence.md` | 111 successful enables (28 attended, 83 night soak); start list without `0050/0010` | The 177 `TRIAL_ENABLE` lines listed in the [enable note](../records/lichuan_c_enable_mode1.md): 88 night-soak captures (83 `results.jsonl` rows and 5 console-motion series), 61 later runs (2026-11-13 to 2026-11-19); 6 starts from `0050/0010` | 177 enables, with `0050/0010` (6); 116 in the 2026-11-12 report, whose date predates the 61. Historical CHANGELOG entries are left as written |
| `docs/reports/2026-11-13_fault_recovery_and_motor_limits.md` section 4 | Node 1 clean 60..4000 rpm, FF05 at 4500; node 2 none yet; "clean" | Ladder captures `TRIAL_EMCY` `05ff81...` (FF05): node 1 trials 5366 (4250 rpm), 5348 and 5356 (4500), 5390 (4000 rpm, 200 ms); node 2 5654 (4625), 5408 (4000 rpm, 200 ms); 5374 (node 1, 4125) and 5416 (node 2, 4500) ran without fault; `TRIAL_VELOCITY` state 3 outcome 6 on the quick-stop runs above 60 rpm | With `2201=1000` (node 2 inferred), FF05 from 4250 rpm on node 1 and 4625 rpm on node 2, and on both at 4000 rpm with 200 ms ramps; "clean" means no fault, while every quick-stop run above 60 rpm ended `DEADLINE` |
| `docs/reports/2026-11-12_motion_capability_evidence.md`, `docs/reports/2026-11-10_bench_r8.md` | Velocity at 6, 30 and 60 rpm only (rounds 7 to 9) | Round 9 captures `20261110T1101Z-s20-c1-velocity-node1-1706` (15 rpm), `...-c2-velocity-node2-1726` (45), `...-c7-velocity-node1-1822` (15), `...-c10-velocity-node1-1882` (45), `...-c11-velocity-node2-1902` (15), `TRIAL_PLAN` line 4 | 6, 15, 30, 45 and 60 rpm |
| `docs/reports/2026-11-12_night_soak.md` | 308 completed moves | `docs/bench/20261111-night/results.jsonl` `kind: arrival` rows: 195 with `route: move` (152 exact, 22 at +1, 21 at -1) and 113 with `route: position` (86, 23, 4) | 308 arrivals: 195 library moves and 113 raw `position` route moves |
| No repository document outside `docs/bench` | (`results.jsonl` `last` read as a final state) | `docs/bench/20261111-night/tools/night.py:179-184` takes the last `TRIAL_LAST passed=1` line, which for the library routes is the admission read: `results.jsonl:1` `last` `0011/8050` while its capture's word 15 reported `8437/0001` | Recorded here only: `last` is the admission report for `enable`, `move` and `velocity` rows, the final read for the raw `position` route |

## Stage (b) commits and checks (2026-11-26)

| Slice | Commit | Checks |
| --- | --- | --- |
| b1 record codec | `ac6119e2` | CI 45715301122 failed on test-only compiler warnings (clang `-Wstring-plus-int`, gcc 13 `-Wformat-truncation` in `records_test.cpp`); fixed in `9a47bd9b` |
| b2 record checks and evidence builders | `9272f3f5`, fix `9a47bd9b` | CI 45720671246 succeeded on `9a47bd9b` (covers b1 and b2) |
| b3 record load, show, use, drop through `ProbeMotion` | `5bad53e5` | `verify.py --mode full` on a clean worktree: 177 of 177 tests, build, registration, package; strict Doxygen in a worktree outside `.build` (39 headers); CI 45726261736 succeeded |
| b4 console `record` commands and storage hooks | `8c9c35b5` | Full verify on a clean worktree: 178 of 178 tests, package; strict Doxygen (39 headers); `check_firmware.py --target all` by the implementer; CI 45731593420 succeeded |
| b5 reviewed records, notes, guides, summary corrections | `f5d129f7` | Full verify on a clean worktree: 179 of 179 tests, build, registration, package; `check_promptset.py --check-git-ref HEAD`, `check_lichuan_operations.py`, `test/package_test.py`; strict Doxygen (39 headers). CI is checked on the pushed commit |

A worktree under `.build` extracts nothing in Doxygen (the Doxyfile excludes `*/MotorControl-CAN/.build/*`), so the
full mode's documentation step there fails vacuously; Doxygen ran in a worktree outside it instead. Locally,
`planning_validator_contract` fails only in the main checkout, where the held untracked maxon files add 23 hashed
artifacts; it passes on every clean worktree.

## Stage (c) review (2026-11-26)

Two new reviewer agents, read-only, on `7ed82e5a` (MotorControl-RS `6544bf7`: three commits since stage (a), all in
`examples/probe_cli`; the qualification seams in `include/` and `src/` unchanged):

- Records against raw evidence (Fable, high): rescanned all 8708 captures, the ledgers and the console transcripts.
  Every included row of the notes matched its cited lines (177 enables, 217 moves, 104 velocity runs), the counts
  and the CRCs recomputed, no EMCY in any included capture. Findings: high, nine velocity enables from exactly
  `8050/0011` failed at word 7 in the S21 enable-fault state and the notes did not account for them; medium, the
  enable-report window's lower part is contradicted, the position box admits combinations that never ran (38200
  counts once, at 300 rpm) and rests on an inferred `2201`, and HALT above 30 rpm rests on two runs; low, the quick
  stop cut confirmed, observation cadence unbounded (trial 1407), console transcripts not fully scanned (no in-box
  failure found), image hashes unreachable from `main`.
- Codec, checks, API, console and storage (Opus, xhigh): a 900k-line mutation fuzz of the codec found no accepted
  invalid line and stable canonical ids. Findings: high, no shipped console route let a position or velocity record
  act (the probe consoles name no move or velocity prerequisites, the bench console attached no records); medium,
  an enable record did not enforce its own identity, and velocity changes and move replacements bypassed a bound
  record; low, the position format has no scale field, the Arduino restore opened Preferences read-write, the file
  store's interruption claim, and the meaning of `record_id`.

Lead dispositions: every high and medium finding fixed in the round below, except where stated here.

- S21 and the enable record: it keeps `8050/0011` (44 mode 1 enables from it, none failed; no mode 1 enable ran in
  the S21 state). The state needs an earlier FF05 above the pulse ceiling, which no record's range reaches, and the
  start report cannot show it; the guard (word 7 `606C` check, pulse-rate admission) and the mode 1 bench step stay
  with prompt 72. The built-in `EnableGate::MEASURED` of prompt 57 has the same exposure without any record.
- `2201`: the velocity record stays closed (`2201=1000` INFERRED, decision above). `6081` and `60FF` are native motor
  rpm, so the rpm claims do not depend on `2201`; the position format keeps no scale field and its note states the
  native units and the inference. Prompt 66's run that reads `2201` in its own capture reopens the velocity record.
- Backlog, not fixed: a minimum enable-report window and an observation cadence in the record format (S22),
  per-rate-band distance bounds for position records, and a full scan of the console transcripts.

## Stage (c) fix round (2026-11-26)

The two stage (c) reviews' accepted items, as the lead listed them, on HEAD `7ed82e5a`:

| Item | Change | Checks |
| --- | --- | --- |
| Records reached no shipped move or velocity route | `BenchAxis` owns its `AxisRecords` (1408 bytes per node, the same in the host and the S2 builds, compile-time size) and attaches them; the bench console attaches the ESP-IDF NVS store (`BenchConsole::attachRecordStore`, `main.cpp`). The CLI contract and the guides now say that on the probe consoles (`SelectedAxis`) only the enable record affects a route: they never name move or velocity prerequisites | `bench_console_test` `testRecordBoundMove`: load and bind a relative record through the console, a move outside its box refused before TX (`DISTANCE`, `SPEED`), one inside run under the record's id |
| Enable record identity | `EnableEvidence::recordIdentity` (filled by `makeEnableEvidence`) replaces `measuredIdentity()` as the MEASURED enable's reference, so `firmwareUnqualified` compares with the record; `checkRecordEnable` takes the identity observation and refuses other product fields (`RecordCheckError::IDENTITY`) | `record_checks_test` `enableIdentityAgainstTheRecord`: a record on another firmware, the drive reading its firmware (not flagged) and `measuredIdentity()`'s (flagged), another product refused by both checks and admitted when the drive reads it |
| Changes under a record | `ProbeMotion::requestMoveReplacement` and `requestVelocityChange` refuse a run admitted under a bound record (ActionError `STATE`) before any TX; attended runs unchanged | `probe_records_test` (velocity change refused under the record, the attended run's change meets its own gate), `bench_console_test` (`move speed` under the record refused, no download) |
| Stores and record id | Arduino restore opens Preferences read-only and answers `TOO_LARGE`; `FileRecordStore`'s comment states what an interruption between remove and rename leaves; the CLI contract's `record_id` is the CRC-32 of the canonical line | Firmware build gate |
| S21 | The velocity record's start is `C050/0015` only. Both notes and `drive_actions.md` state the hazard: after an FF05 at `2201=51200` the drive moves at word 7 of every enable, and the fresh `8050/0011` report cannot show that state; the enable record keeps `8050/0011` (no mode 1 counterexample), prompt 72 owns the word 7 guard and the mode 1 bench step. The lead cited 8 runs (FF05 in 7); the scan finds a ninth, 5889 (node 2, 3000 rpm, 500 ms ramps), with the same signature and its FF05 | Notes list all nine with their capture lines |
| Velocity HALT | 6..30 rpm: above 30 only runs 1882 (45 rpm) and 1613 (60 rpm). With `C050/0015` alone, HALT from that start ran once (1902, 15 rpm); the range rests on runs from `8050/0011`, stated in the note as an unmeasured combination of the box | `reviewed_records_test` |
| Velocity window | The note and `velocity.md`: the last `8033/0001` poll ended 0.398 to 0.399 s after word 15's acknowledgement and the report was seen by 0.747 to 0.749 s (102 runs); shorter windows are contradicted, format v1 has no minimum; backlog S22 (minimum window and observation cadence, trial 1407) | |
| Position ranges | The note and `position_parameters.md`: native ranges (6081 rpm, 607A increments) at `2201=1000` INFERRED; physical travel per increment follows the drive's `2201`; 38200 counts ran once (3141, 300 rpm), at 6 rpm at most 1000; untested combinations stay as stated; no format change | |
| Image hashes | Each image not reachable from `main` names its tree-identical `main` commit: `e17271a` = `6320892c`, `4247e59` = `7cd8c210`, `faffa22` = `0b2d6f58`, `a3c5fa4` = `fd0ac052`, `cb7941c` = `e5bd4998` | `git rev-parse <hash>^{tree}` against `git log main --format="%H %T"` |

The re-encoded velocity record (`halt_rpm=6..30`, `starts=C050/0015`) was written by `reviewed_records_test --write`;
the test failed on the old line first (`halt`), as a stale record must.

Lead checks of the fix round `14d8b7a2` on a clean worktree: `verify.py --mode full` built, registered and passed 178
of 179 tests; `planning_validator_contract` exceeded its 90 s limit while the firmware gates ran in parallel and passed
alone in 55 s on the same build. `test/package_test.py`, `check_promptset.py --check-git-ref HEAD`,
`check_lichuan_operations.py`, `s2_bench_test.py`, strict Doxygen outside `.build` (39 headers) and
`check_firmware.py --target all` (Arduino, IDF S3 and S2, storage, S2 bench, component; no flashing) passed.
