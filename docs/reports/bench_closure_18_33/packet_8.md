# Packet 8: finite relative moves and stops (host part)

[Packet 8](../../prompts/bench_closure_18_33/8_motion_stop_campaign.md), host work only, 2026-11-07. **Draft:** the motion items 2 to 4 did not run, and the packet stays open. Back to the [execution index](README.md); the bench plan is the [procedure](packet_8_procedure.md).

## Start state

| Fact | Value |
| --- | --- |
| Branch, revision | `promptset/motorcontrol-can` at `3353ed2`; `a04777e` (packet 3 smoke and packet 8 start baseline, by the lead) landed during this work and touches none of its files except a neighboring `CHANGELOG.md` entry. Untracked `.vscode/` editor files untouched |
| Bench image | `3353ed2`, `elf=8bce27078fa1bddd...`, `routes=5`, `trial=0`, `motion=0` ([ping](../../bench/20261107T212519Z-s2-ping/rx.txt)) |
| CAN | Started, 1 Mbit/s, TX17 RX16 S18 ([start](../../bench/20261107T212534Z-p8-start/rx.txt)); both nodes heartbeat `7F` |
| Node 1 | `6041=0037` (enabled), `2100=0000`, `6061=1`, `6064=4728`, `606C=0`, `607A=12000`, `1001=00`, `603F=0000`: unchanged since trials 709 and 710 on 2026-11-05 ([readiness1](../../bench/20261107T212536Z-p8-readiness1/rx.txt)) |
| Node 2 | `6041=4050` (released), `2100=0014` (running latched, released), `6061=1`, `6064=821`, `606C=0`, `607A=10000`, no error ([readiness2](../../bench/20261107T212546Z-p8-readiness2/rx.txt)) |

**Sibling audit.** HEAD and dirty state checked on 2026-11-07; both clean and unchanged since the authoring row (MotorControl-RS `cd27b12`, FieldCore-node `5ad9292d`). FieldCore was not reread. The RS seam for this packet:

- `src/Axis.cpp` `invalidateAxisReference`: adopted. A coordinate loss withdraws the origins, soft-limit qualification and the witness's `nativeKnown`, `idle` and `stationary` and advances the epoch once. CAN keeps two epochs (configuration and reference), and test 30 checks that each advances exactly once.
- `src/profiles/ess_rs/Position.cpp` `finish`: adopted. A failed move after any possibly transmitted trigger stays `uncertain`. CAN adds the session's move fence and the unresolved route (test 28).
- `src/profiles/ess_rs/Actions.cpp` `prepareClearAlarm` through `prepareKind`: rejected. RS prepares a clear without a state input, so a healthy drive would get the command. CAN refuses before work unless `2100` reports the alarm with raw `603F=FF05` (item 5d). `observed()` for `CLEAR_ALARM` (alarm word zero) is rejected as before: no CAN source says `603F` returns to zero. `observed()` for `STOP` (running clear) is the CAN HALT rule; the quick-stop release exception is CAN's own.

## Requirements

| Item | Status | Where |
| --- | --- | --- |
| 1 Bounded route support and plan | Met without new route code. One runner refusal added (below). Procedure written and dry-run checked | [procedure](packet_8_procedure.md); `bench_route_test.cpp` `testPacket8Plan`; `s2_bench.py` `check_bounds` |
| 2 Node 1 negative relative move | Not run: blocked (below). Planned as trial 805 | Procedure step 3 |
| 3 Node 2 ACTUAL versus COMMANDED | Not run: blocked. Planned as trials 808 to 810 through the experiment routes; node 2's base stays `UNMEASURED` | Procedure step 4 |
| 4 HALT and quick stop from `005F` | Not run: blocked. Planned as 812 (HALT interrupting node 1's relative move, node 2 observing) and 813 (quick stop from node 2's acknowledged `005F`); a quick stop interrupting a moving `005F` needs the next boot | Procedure step 5 |
| 5 Unknown trigger, stop and counter consequences | Met as host timelines through the real owner; table below | `move_relative_test.cpp`, `alarm_clear_test.cpp`, `axis_session_test.cpp`, `bench_route_test.cpp` |
| 6 Defects | None found in library or firmware. One runner gap fixed: a move deadline the library refuses was counted before the refusal | `scripts/s2_bench.py` |

### Item 5: consequences mapped to tests

| Consequence | Existing coverage | New test | What it adds |
| --- | --- | --- | --- |
| a. `005F` reply lost (`UNKNOWN`): next move admission fenced; a stop still admissible from the retained cycle or as best-effort `000B`; nothing replays | `move_relative_test.cpp` `ambiguousLateAck` (silent drive, quick stop from an empty read, fence, late ACK changes nothing); `matrixCase` possible trigger / missing (fence, at most one `005F`); `bench_route_test.cpp` `testRelative` "relative-trigger-lost" (no rearm, no stop by itself) | `lostTriggerThenStop` (test 28): drive silent or acting on the trigger with only its reply lost; HALT and quick stop; from the retained admission read and from `moveObservation()` | The new move is refused unsent with `TARGET` ("no usable reservation") because the lost reply left the route unresolved, and the fence stays set behind it. With a zero best-effort deadline the stop is refused with nothing sent. Otherwise exactly one frame follows the lost trigger, `000B`, never `010F`, `000F` or a second `005F`, even for a HALT request; it stays `STOP_SENT_UNCONFIRMED`; the move's record is unchanged by the stop. New: confirms existing correct behavior |
| b. HALT and quick stop from an acknowledged `005F`, each its own request, move and stop retained independently, the other node receiving passive observations | `stopFromHeldTrigger` (HALT, one node); `bothNodesUrgent` (both nodes stopping); `urgentBesideOtherWork` (the other node with a queued, held or handed-off move download); `bench_route_test.cpp` `testRelative` (planned HALT from `005F` in the experiment route) | `stopsBesideObserver` (test 29), HALT and quick stop | Node 2 only reads, through its own session: one upload is queued, unsent, when node 1's stop is requested, and it and later reads complete. Node 2's uploads interleave on the bus between node 1's stop word and its last read; node 2 receives no download and keeps its own latest `000F`. The stop is sequenced from `005F` (`PROJECT_DECISION`), no `000F` precedes it; move and stop records are unchanged by later observer traffic. Confirms existing behavior |
| c. Counter zero (`2302` `0 1 0`) after a move: reference and witness invalidated, `2302` history kept, no home or rebase, a later relative move from the old witness refused | `counter_clear_test.cpp` (withdrawal, stage matrix, synchronization); `axis_session_test.cpp` `counterZeroThroughSession` (session path, from idle); `move_relative_test.cpp` and `move_absolute_test.cpp` `counterZeroInvalidates` (a prepared, unsent move, direct library calls) | `counterZeroAfterMove` (test 30) | After a completed relative move, through `AxisSession`: exactly three downloads `2302` 0, 1, 0, each acknowledged in its own slot and unchanged afterwards; axis epochs each advanced once, no origin, the borrowed witness withdrawn, no latest word left; the completed move's record unchanged (no rebase); on the new epoch and after an explicit `000F`, the old witness, as kept or re-dated, is refused (`STALE_GENERATION`), and only a witness of the new epoch admits a move. The move rig's fake now zeroes `6064` on `2302` 0 then 1, from trial 323's one report. Confirms existing behavior |
| d. Alarm clear on a healthy drive refused before TX | `alarm_clear_test.cpp` `admissionScope` (monitor 0 with `603F=FF05`: `STATE`, context unchanged); `axis_session_test.cpp` `clearRefusalsChangeNothing` (session unchanged, nothing sent); `bench_route_test.cpp` `testClearAlarm` (`no-alarm`, refused before the library, nothing written) | Also in test 30, after the move, with `603F` still holding a historic `FF05` | The current API refuses, and would not send: the library requires the reported `2100` alarm bit before any check of the code, and the S2 route refuses earlier. Confirms existing behavior |

### New regressions and their mutants

Each mutant was built in `.build/p8host`, the new test run alone (a copy of the suite whose `runAll` or `main` calls only it) and the whole suite run, then the product file restored byte for byte (SHA-256 compared) and rebuilt; every new test passes again on the restored tree.

| Test | Mutant | New test alone | Whole suite, first failure |
| --- | --- | --- | --- |
| 28 `lostTriggerThenStop` | `AxisSession::afterMove` fences only after an `ACKNOWLEDGED` trigger | Fails: `r.a.moveFenced() && ...` (silent, HALT) | `matrixCase` pre-trigger / missing |
| 29 `stopsBesideObserver` | `CanBusOwner::beginUrgentOperation` cancels every sequence's queued work, not only its own | Fails: the observer's queued upload ends unsuccessful | `bothNodesUrgent` |
| 30 `counterZeroAfterMove` | `Axis.cpp` `evidence` ignores the binding and reference epochs | Fails: the old, re-dated witness admits a move | `gates` |
| 30 `counterZeroAfterMove` | `Actions.cpp` `validateExplicit` no longer needs the alarm bit for an alarm clear | Fails: the healthy drive's clear is admitted | The new test itself (other binaries' `admissionScope` and `clearRefusalsChangeNothing` would too) |
| `testPacket8Plan` | `BenchRoute::begin` admits one motion line fewer per boot | Fails: line 813 refused as `budget` | `testMotionBudget` |
| `test_short_move_deadline_refuses_before_sending` | `check_bounds` without the move-deadline rule | Fails: the 4550 ms line is sent (status 0, not 2) | n/a (one test) |

Restored SHA-256: `AxisSession.cpp` `ff3939d1...`, `CanBusOwner.cpp` `8800f2ae...`, `Axis.cpp` `24349d00...`, `Actions.cpp` `6443178b...`, `BenchRoute.cpp` `7f03af0f...`, `s2_bench.py` `3a54bf38...`, each identical to the file before mutation.

## Route support (item 1)

Every planned motion case runs through existing routes and `AxisSession`; no route code was added, and no new image is needed. `testPacket8Plan` replays the procedure's twelve admissions in order through the real `BenchRoute` (fake identity and signals). Findings that shaped the plan:

1. **A move deadline must exceed 4550 ms.** The library refuses a move deadline that does not exceed `MOVE_SERVICE_STEPS` (13) services at the routes' 100 ms queue and 250 ms response budgets. The firmware counts the line and its planned stop first, and the runner did not check it: a first draft of 812 at 3000 ms would have spent two of the twelve admissions. `scripts/s2_bench.py` `check_bounds` now refuses such a line before anything is sent or counted, and `--dry-run` shows it (runner test and mutant above; [bench guide](../../hardware_bench.md) and CHANGELOG updated).
2. **One stop from a moving `005F` per node per power-up.** After a HALT from `005F` no rearm exists (only a POSITION record's planned HALT is a rearm predecessor), a quick stop releases the drive, and a move interrupted by a stop fences its session; each stop must be that node's last motion of the boot. With items 2 and 3 first, the twelve admissions hold one moving stop (HALT, 812) and one stationary stop from an acknowledged `005F` (quick stop, 813). A quick stop interrupting a moving `005F` needs the next boot; its line is dry-run checked in the procedure. A route that rearms after a RELATIVE record's HALT would fit both moving stops in one boot (eleven admissions), but needs new firmware and a protocol change; it was not added.
3. **The experiment route `relative` has no observer.** The observed case uses the production `move` with `--move-base actual` and `--observer 2`; its planned stop also comes from the held `005F`. Record 409 is node 1's measured base, so node 1 moves and node 2 observes.
4. **Budgets.** Relative `move` with observer and planned HALT: 122 of 128 terminals at a 5000 ms deadline, 2500 ms interval, 1500 ms stop deadline polled every 750 ms. Item 2 at 30 s needs an interval of 3000 ms (117 terminals; 2500 ms would be 131).

## Procedure

[packet_8_procedure.md](packet_8_procedure.md): one drive power cycle, then 13 trial lines (801 to 813) with every field, one ledger `docs\bench\<date>-p8-campaign.jsonl` created by `--new-campaign` on 803 only, release after every route, the run-time fields (enable `6041`, `607A`, `60FF`, every `--start-position`, identity), the attendant questions, the expected record lines and the stop conditions. Every line passed `--dry-run` with exit status 0; against a scratch ledger the campaign reached exactly 12 motion admissions and refused a thirteenth.

## Blocked, and why

- **The user is absent on 2026-11-07.** Every enable needs the attendant's own felt shaft observation: `scripts/s2_bench.py` `before_send` asks `power_up_shaft` and refuses `UNOBSERVED` unsent, and `--unattended-waiver` refuses `enable` outright. Behind it, `validateEnable` refuses a start report that reads enabled without that observation. A drive report is never a substitute (trials 704 and 712, see their NOTE.txt).
- **Node 1 is still enabled from an earlier boot** (`0037` since 2026-11-05) with no host history in this S2 boot and no reviewed release transition, and node 2 is released with running latched. Neither admits an enable (`2100` not in place; written since power-up), and no route may write to node 1 as it is. A power cycle of both drives is the first physical step.
- Without an enable no motion case can run: each needs an acknowledged `000F` in the current session.

## Remaining gaps and next measurement

| Gap | Kind | Next smallest measurement |
| --- | --- | --- |
| Node 1 negative relative, node 2 base, HALT from moving `005F`, quick stop from acknowledged `005F` | Device facts, drive reports | The procedure's boot: power cycle, then 801 to 813 with the attendant |
| Quick stop interrupting a moving `005F` | Device fact | Next boot: the procedure's trial 905 line after mode 1 and enable of node 1 |
| Node 2 production relative move | Device fact | After 810 gives a usable base: one `move` on node 2 with record 810 and that base |
| Release | No reviewed transition | A reviewed candidate from manufacturer evidence, with an independent shaft or torque observation; outside this packet |
| Alarm clear on a real alarm | Naturally occurring alarm only | When an identified alarm appears by itself: `readiness-history` then one `clear-alarm` ([fault actions](../../fault_reference_actions.md)); never induced |
| DO, filters, tuning, soft-limit enforcement | Instrument-dependent settings | Meters, a filter stimulus, a phase-current probe or travel protection, as their packets name |
| Command pulses per turn, direction | Physical scale | An independent angle measurement for a known displacement; the attendant's mark is a coarse check only |
| Counter zero on node 1 | Device fact, optional | Only with a justified nonzero baseline, for example after 805 (`6064` about -100), in a later boot with a spare admission |
| Other firmware | Untested firmware | One firmware on two specimens; any other needs its own identity and records |

Lost replies, the stop beside an observer, counter zero after a move and the healthy-drive alarm refusal are host timelines only; none is planned physically, as the packet requires.

## Checks

| Check | Result |
| --- | --- |
| Focused CTest in `.build/p8host` (GCC, Release, `-Werror`): `move_*`, `axis_session`, `enable_action`, `stop_action`, `alarm_clear`, `counter_clear`, `bench_route`, owner, service and reference suites | 23/23 passed (exit 0); `move_relative` 182,216 checks, `bench_route` 70,665 |
| Whole CTest set, same build | 103/105 passed; `planning_integrity` and `planning_validator_contract` failed only because this report did not exist yet, and passed (exit 0) once it did |
| `python scripts/s2_bench_test.py` | 33 tests OK (exit 0) |
| `python scripts/check_repository.py` | PASS (exit 0) |
| `python scripts/check_promptset.py` | No errors (exit 0) |
| `git diff --check` | Clean (exit 0) |
| Firmware build, Doxygen, remote CI | Not run: no library source, public header or firmware changed; the bench image is unchanged |

Original prompts 29 and 31 to 33 stay Partial. Packet 8 stays open until its bench items run.
