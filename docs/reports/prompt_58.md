# Prompt 58 report: measured records through the library API and the console

State: In progress; stage (a) complete, stage (b) next. Run by the unattended Claude loop of 2026-11-23
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
