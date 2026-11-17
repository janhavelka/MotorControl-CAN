# HIL campaign after the power cycle, 2026-11-16

The user's request, verbatim: "do all HIL testing regarding new implementation and finish up all tests that are missing
and are able to run now.. Motor power cycled". Bench: the S2 board on COM15 (serial 94A990398B40), two CL86-C drives
(firmware `1000=00040192`, `1009=000c`, `100A=0020`), nodes 1 and 2 at 1 Mbit/s, the standing bench authorization of
2026-11-02. Image: `17386514` (ELF `20cc8ea2…f0d54`, SDK 5.5.5, PONG `routes=15 pdo=7`), flashed at 09:15Z
([upload](../bench/20261116T0915Z-s2-upload/), [ping check](../bench/20261116-hil/00-ping/)). It contains the
[52/54 audit](audit_52_54.md) corrections, which no earlier image had. Evidence:
[`docs/bench/20261116-hil/`](../bench/20261116-hil/README.md). Every value is a drive or owner report; no shaft,
supply or independent CAN observation was made. Labels as elsewhere: **M** measured, **I** inferred.

The two-axis trials ran later the same day on a second image, `b7033c12` (ELF `59a90519…5cb207`, PONG `routes=16`),
flashed at 17:53Z after a further user power cycle ([upload](../bench/20261116T1753Z-s2-upload/),
[ping check](../bench/20261116-hil/20-pair-ping/)). That image adds the paired arming the auditor asked for
([two-axis trials](#two-axis-trials-hm1-to-hm9)).

## Summary

| Area | Result |
| --- | --- |
| External boundary | The power cycle retired [prompt 54](prompt_54.md)'s two motion restrictions: both drives read the power-up report (`8037`/`0001`, mode 0, `6064` 0, `60FF=120`) at 09:19Z |
| Regression (bench routes) | Velocity 60 rpm, position moves, stops: clean on both nodes |
| Prompt 59 leftovers | Node 1 position ladder 300 to 3000 rpm, arrival within ±1 count; node 2 reverse −60 to −3000 rpm, clean (ratio 0.987 to 0.991) |
| Prompt 52/54 changes, bench routes | 14 of 14 position changes (immediate speed, immediate retarget, buffered retarget) and 6 of 10 velocity changes completed; the 4 others were correct budget refusals |
| Prompt 52/54 changes, console | 7 of 7 typed velocity changes executed, a stop during a change ramp included; typed move replacements admitted only at the edge of the replacement window, then `DEADLINE` by design (experiment) |
| M14 (NMT Stop during motion) | 4 of 4: the drive kept running at the commanded speed through NMT Stopped and Pre-operational; only the run's own SDO quick stop stopped it |
| Two-axis (HM1 to HM5) | Not executable on `17386514`: the bench refuses an arming line while the console holds an operation, and one arming expires before two can be printed. On `b7033c12`, with one paired arming before either moves: HM1, HM2, HM5 passed; HM3's group HALT passed, and the group quick stop 2.3 s later had only the best-effort path (unconfirmed by definition) |
| SYNC | 20 ms for 30 minutes passed: 90,000 SYNC frames confirmed, 90,000 TPDOs from each node captured completely; the 10 ms run is still the open target (prompt 56) |
| S21 discriminator | 3000 rpm with 500 ms ramps at `2201=51200` was clean on node 1: a pulse-rate limit, not the deviation threshold, fits |

## Boundary and setup facts (M)

- After the power cycle both drives produce **no heartbeat** (`1017` read 0 on both) and boot **Pre-operational**
  (heartbeat state 127 once `1017` was set). The heartbeats seen in prompt 54 came from a setting the power cycle
  cleared; `1017` is volatile like `2201`. Set with the `h`/`j` experiment (0 → 1000 ms, read back, no save,
  [record](../bench/20261116-hil/12-hb-tpdo-setup/)).
- TPDO1 transmission type read **255** after the power cycle on both nodes and was set to 1 in Pre-operational,
  then NMT Start ([record](../bench/20261116-hil/13-tpdo1-type1/)). With type 1 and no SYNC producer the bus carries
  no PDO traffic, which keeps RX quiet for the motion trials.
- NMT commands need the node bound in the current MCU boot: a plan of `N1,o` alone is refused (`unconfigured`);
  identity uploads (`3`, `4`) bind them first. A plan's `n` listing must finish (`END network`) before the next
  command.

## Regression and prompt 59 (bench routes)

Node 1 and node 2 ran 60 rpm velocity runs (ratio 0.992, stop confirmed) and node 2 a 300 rpm position move.

Node 1 position ladder (2201 = 1000, ramps 1000 ms below 1200 rpm, else 2000 ms):

| rpm | Arrival error (counts) | Peak `606C` |
| --- | --- | --- |
| 300 | 0 | 298 |
| 600 | +1 | 594 |
| 1200 | −1 | 1188 |
| 1800 | +1 | 1785 |
| 2400 | 0 | 2381 |
| 3000 | −1 | 2975 |

Node 2 reverse velocity, one run each through the reverse probe: −60, −300, −1000, −2000 and −3000 rpm, all clean,
`606C` negative and `6064` running down at 0.987 to 0.991 of the command, as forward and as node 1 on 2026-11-13. The
shaft direction was not observed (Q03 stays open for the physical direction).

## Changes during motion through the bench routes (prompt 52 forms, prompt 54 item 3)

**Velocity change (`Tv`)**, both nodes, change at the 5 s running cycle, the run's own stop at 12 s:

| Run | Change | Result |
| --- | --- | --- |
| 30 rpm, ramps 500 ms | → 60 | Executed; next cycle 60 rpm; own HALT confirmed |
| 60 rpm, ramps 500 ms | → 30 | Executed; next cycle 30 rpm |
| 6 rpm, ramps 200 ms | → 60 | Executed; 0.06 s later 9 to 11 rpm, then 60 |
| 60 rpm, ramps 2000 ms, change at 9 s | → 6 | Refused before any write: "velocity change and observation do not fit the original run budget" |
| 12 rpm, ramps 1000 ms, change at 9 s | → 30 | Refused, the same reason |

The refusals are correct: with the 5 s observation interval the 10 s cycle leaves no room for the post-change
observation before the 12 s stop. The route cannot express a stop inside a change ramp (every shorter-interval plan
exceeds the 128-terminal record, mostly because of the fixed enable report window); the console trials below did.

**Position change (`Tp`)**, both nodes, 7 trials each, change after the second 500 ms cycle, planned stop at 6 s:

| Case, speed | Ramps | Observed (`606C` every 500 ms) | Stop |
| --- | --- | --- | --- |
| 5 immediate speed, 12 → 30 rpm | 1000 ms | 5, 11, then 18, 30 within 1 s | Quick, from moving `003F`, confirmed |
| 5 immediate speed, 60 → 12 rpm | 200 ms | 60, then 11 within 0.5 s | As above |
| 5 immediate speed, 30 → 60 rpm | 500 ms | 30, then 56, 60 | HALT from `003F`, confirmed; drive left halted, then released |
| 6 immediate retarget, 30 rpm, +600 | 500 ms | 30 through the old endpoint, arrival at the new endpoint before the stop | Stop after arrival, confirmed |
| 6 immediate retarget, 60 rpm, +1500 | 200 ms | 60 throughout, stopped before the new endpoint | Quick, confirmed |
| 7 buffered retarget, 30 rpm, +600 | 500 ms | 30, 30, dip to 5 near the old endpoint, 24, 30 | Quick from `001F`, confirmed |
| 7 buffered retarget, 12 rpm, +300 | 1000 ms | 13, 11, 5, 0 at the old endpoint, 5 | As above |

All 14 completed (`failure=0`). This repeats prompt 52's node 2 findings on both nodes and at more speeds and
ramps. The 500 ms samples bound the ramp response of an immediate speed change (Q26) but do not measure it.

## Changes during motion through the console (typed path)

The console path runs `ProbeConsole → ProbeMotion → AxisSession` with the arming's `allowChanges` flag (an attended
experiment; no record is qualified). Two tool limits shaped the method. `scripts/s2_bench_console.py` opens the port
once per invocation, so records that arrive between invocations are lost (the first trials kept no
`velocity_change` record and no own-stop terminal). And the arming expires about 3 s after its notice. A
single-port executor (`tools/sched.py`) therefore ran each trial: one port session, every byte kept, timed sends.

**Console regression**, both nodes: two moves to completion (60 and 120 rpm, `OBSERVED`), a 90 rpm move ended by
`stop N fast` (move `CANCELLED`/uncertain, stop `OBSERVED`, `reply_qualifies:true`); a 30 rpm velocity run with its
own fast stop (`OBSERVED`) and a 45 rpm run ended by an explicit fast stop.

**Typed velocity change** (`velocity change ID … rpm motor configured`), 7 executed, 0 refused:

| Node | Run, ramps | Change | `velocity_change` first report | Run terminal |
| --- | --- | --- | --- | --- |
| 1 | 12 rpm, 500 ms (multi-invocation run) | → 30 at 5.5 s | not captured | own stop; drive left `60FF=30` |
| 2 | 12 rpm, 500 ms | → 30 at 5.5 s | 13 rpm | `OBSERVED` (own fast stop) |
| 1, 2 | 60 rpm, 1000 ms | → 12 at 5.5 s | 58, 60 rpm | `OBSERVED` |
| 1, 2 | 6 rpm, 2000 ms | → 60 at 6.0 s, `stop N fast` 0.6 s later | 7 rpm | run `CANCELLED`, stop `OBSERVED` |
| 1, 2 | 6 rpm, 500 ms | → 60 at 5.5 s | 7, 9 rpm | `OBSERVED` |

Every change record reads `execution_code=1`, `uncertain=false`, `qualified=false`. The stop during the 2000 ms
ramp is the case the bench route could not express.

**Typed move replacement** (`move speed ID 60 immediate`, `move ramps ID 200 800 immediate`): a 3000-count move at
30 rpm. The console gives every move a fixed 10 s deadline
([`ProbeConsoleMotion.cpp`](../../examples/common/ProbeConsoleMotion.cpp), `tail`), and a replacement needs 21
services, about 7.35 s, to remain ([`Position.cpp`](../../src/profiles/lichuan_c/Position.cpp),
`checkReplacement`). So a change must be offered within about 2.65 s of the move request. On this bench the move's
reply arrives about 2.65 s after the request, because it waits behind the arming record's 2.7 s printout. The
offer therefore lands at the edge: 5 offers at 1.5 s after the reply and 1 delayed by a `debug raw` command were
refused ("replacement services do not fit the original deadline"); 4 offered right after the reply were admitted.
Admitted replacements recorded `move_relation` (original → replacement, observation superseded); the original
ended `CANCELLED`/uncertain and the replacement `DEADLINE`/uncertain while the drive reported arrival exactly at
the target (`6064=607A`, `9437`). That outcome is by design: an attended-experiment replacement "never claims
completion" ([`Position.h`](../../include/MotorControlCAN/profiles/lichuan_c/Position.h), `MoveReplacementEvidence`).
One admitted ramp change on node 1 ended on the executor's failure path (a refused record release, below) with a
fast stop. `move retarget` was refused as documented ("changed target needs independently measured feedback
mapping"; this bench installs no relation).

## M14: NMT Stop during a slow velocity run (route `velocity-nmt`)

The helper needs fresh exact Operational evidence and a running gap that leaves seven services before the stop.
Ten attempts ended `nmt-window` with no NMT frame sent, each run then stopping by its own quick stop: three before
any heartbeat existed (`1017=0`), three with 13 s runs (the next poll came about 2.1 s before the stop, under the
2.45 s needed), and four on nodes that an earlier trial had left Pre-operational. With heartbeats, NMT Start before
each trial and 17 s runs:

| Node, rpm | NMT Stop observed (heartbeat) | Pre-operational observed | `606C` / `6064` 5 s later | Own quick stop |
| --- | --- | --- | --- | --- |
| 1, 6 | 0.88 s after the request | 1.00 s after its request | 5 rpm, +501 counts | Confirmed, released |
| 2, 6 | 0.91 s | 1.00 s | 5 rpm, +501 counts | Confirmed, released |
| 1, 30 | 0.91 s | 1.00 s | 30 rpm, +2504 counts | Confirmed, released |
| 2, 30 | 0.89 s | 0.99 s | 30 rpm, +2503 counts | Confirmed, released |

**M14 result (M, 4 of 4, one firmware):** NMT Stop and then Pre-operational do not stop or slow an enabled CL86-C
in velocity mode; it keeps the commanded speed until an SDO quick stop. NMT Stop must never be treated as a motor
stop, as the library already states. Each trial leaves its node Pre-operational.

## Two-axis trials (HM1 to HM9)

Not executable on this image. Both axes must be armed before either moves, because the bench refuses every T line,
arming included, while the console holds an operation ([`main.cpp`](../../examples/s2_bench/main/main.cpp),
`consoleHeld`, refusal `console`). An arming expires after 3.15 s (`BenchConsole::arm`), and printing one arming's
record takes about 2.7 s at 115200 baud, so the first arming is stale before the second is printed. HM1's attempt
moved node 1, refused node 2's arming (`TRIAL_REFUSED reason=console`) and ended on its failure path
(`stop all fast`: `group_stop_axis` axis 1 `OBSERVED`). HM2, HM3 and HM5 were not started. The
`scripts/paired_motion.py` runner has the same limit, as its research note predicted. Needed: a bench route that arms
both axes in one record, or arming that does not print its record before the motion line.

### Correction: paired arming (image `b7033c12`, route protocol 16)

The auditor's proposed fix, implemented in `b7033c12`: one `Ta` (moves) or `Te` (velocity) line reads and checks both
nodes in one record. The console then arms each axis exactly as its own `TY` or `TB` line would. The record prints one
`TRIAL_PAIR` line instead of its dump. Its full dump (`d`) is allowed only after the console's results are released, so
it never holds a motion or stop reply. The freshness cap (3.15 s) and the global guard against T lines during console
work are unchanged; the moves and stops are separate console lines with separate admissions and results, so nothing
here is simultaneous or atomic motion. Native regression: `test/bench_console_test.cpp` drives main.cpp's admission
policy (now `BenchConsole.h`'s `benchActivity`, `trialLineRefusal`, `dumpRefusal`) over a 115200-baud and an
output-blocked UART model. The trials below run on `scripts/s2_bench_console.py`'s executor
([`pairhil.py`](../bench/20261116-hil/tools/pairhil.py), [`sched.py`](../bench/20261116-hil/tools/sched.py)), one
MCU boot per two-axis setup. Both drives were first given new stop records and enables, because the earlier ones
predated the power cycle (series S0108 to S0111). All values M (drive and owner reports), absolute moves at 30 rpm
(native), 500 ms ramps, observation every 500 ms:

| Trial | Arming (route time, `TRIAL_RES`) | Move or run accepted, after the arming notice (host clock, about 15 ms resolution) | Result |
| --- | --- | --- | --- |
| HM1: both axes 3000 counts, opposite directions, to completion (S0112) | `Ta` 56461: both checks passed, 0.265 s | Node 1 +0.31 s, node 2 +0.63 s (age cap 3.15 s) | Both `OBSERVED`, completion 1; ended exactly on target (8320, 14319); completions 0.32 s apart |
| HM2: both 3500 counts; HALT node 1 at 3 s (S0112, same boot) | `Ta` 56464, 0.264 s | +0.31 s, +0.63 s | Node 1: `stop 1 normal` `OBSERVED` 0.8 s after the request; its move `CANCELLED`, uncertain (an interrupted move), halted at 6617 (from 8320 toward 4820). Node 2 completed to 17819, `OBSERVED`. No node 1 controlword from node 2's work, nor the reverse |
| HM3: both 4000 counts; group HALT at 3 s, then group quick stop (S0115) | `Ta` 56480, 0.265 s | +0.30 s, +0.61 s | `stop all normal`: both axes `OBSERVED` (`group_stop_axis`), moves `CANCELLED`, uncertain; halted at 4917 and 19367. `stop all fast` 2.3 s later: both axes `STOP_SENT_UNCONFIRMED` (see below); the next boot read both released and still (`6041=0050`, `606C` 0) |
| HM5: both velocity 30 rpm, 12 s, each with its own quick stop at the due time (S0118) | `Te` 56492: both released mode 3 standstill checks passed, 0.250 s | +0.19 s, +0.39 s | Both runs `OBSERVED` with their own stop; both ended released and still (5533 and 5518 counts travelled), `2100=0014` (the known latch after a moving quick stop) |

After each trial the console's results were released, then `d` dumped the full record (36 terminals, 108 traffic
frames, `lost=0`), and `r` released it. In every record, node 2's reads end in `node=2`. `tail=16 tailLost=1` in each
record is the post-terminal tail filling with the motion traffic that followed, which the console's own records
hold. The evidence packer parsed all four sessions with no malformed or unterminated line, five of five summaries
each.

HM3's group quick stop was the best-effort path. `STOP_SENT_UNCONFIRMED` means the `000B` left the controller and
nothing can confirm it. The console names a stop's state from the session's last move cycle, not from the earlier
stop's own observation. The HALT had cancelled both moves 2.3 s before, so that state was past the stop's 2.45 s
age cap, and only the arming line's best-effort budget (`be=1500`) remained. The sessions then kept the
uncertainty, and the bench's own reads in that boot were refused (`reason=session`) until the next boot, as
designed. A second stop that is meant to be confirmed must follow the first within that cap, or its state must come
from a fresher read (backlog M01).

Not run: HM4 and HM6 to HM9 need a lost reply, a refused axis or a fault on one axis, which this bench cannot inject
physically, and the sustained mixed soak needs its own session.

### 2026-11-17: HM3 again after the freshest-state fix (image `7ded4336`)

`db4e5f3e` lets `AxisSession::requestStop` use the freshest complete report it holds, so a quick stop after a finished
HALT is sequenced on the HALT's own cycle, and a checked stop now clears the move fence. Image `7ded4336` (ELF
`e0ae2c3b…8f6274`, flashed 11:38Z after the user's power cycle, [upload](../bench/20261117T1138Z-s2-upload/),
[ping check](../bench/20261116-hil/21-relax-ping/)) ran HM3 again ([`relaxhil.py`](../bench/20261116-hil/tools/relaxhil.py),
series S0119 to S0123). All M:

- Setup without stop records: each drive's power-up report, mode 1 and the best-effort `000B` release as before, then
  `s2_bench.py enable --released` from the fresh released report (`8050`/`0011`, mode 1) on both nodes: both enables
  `failure=0` (trials 56506 and 56507). No 60 rpm stop-record run was needed.
- `Ta` 56508, both moves of 4000 counts at 30 rpm admitted; `stop all normal` at 3 s: both axes `OBSERVED`.
- `stop all fast` 2.3 s after the HALT request (1.6 s after its terminal): both axes `OBSERVED`, `uncertain:false`
  (on 2026-11-16 both were `STOP_SENT_UNCONFIRMED`). The bench's own reads afterwards succeeded (on 2026-11-16 they were
  refused `reason=session`): both drives released and still (`6041=0050`, `606C` 0) at 6064 -1712 and 1555.
- The paired record parsed with no malformed or unterminated line, five of five summaries.

## SYNC

Both TPDO1 at type 1, both nodes Operational with heartbeats, no other producer, `scripts/sync_watch.py` on the final
image ([short](../bench/20261116-hil/16-sync-10ms-short/), [30 minutes](../bench/20261116-hil/17-sync-20ms-30min/)):

| Run | SYNC confirmed | TPDOs per node | Capture | Timing (software and driver bounds) |
| --- | --- | --- | --- | --- |
| 10 ms, 5 s, 500 slots | 500 / 500 | 500 | Complete | Short check only |
| 20 ms, 1800 s, 90,000 slots | 90,000 / 90,000 (`LOCAL_TX`, 90,000 terminals confirmed) | 90,000 (`0x181`, `0x182`) | 183,598 frames, all printed, `tapDropped` 0, `timing_complete`, no controller loss or error | Owner handoff gaps 17.99 to 23.00 ms, worst lateness 4.78 ms; receive gaps 17.6 to 23.4 ms (mean 20.0, sd 1.0) per node; heartbeats 1799 per node at 1000.8 ms |

**20 ms passes the 30-minute requirement** on this image: no missed slot, no lost frame. Prompt 54 asked for the
longest period that passes when 10 ms fails; 10 ms failed at about 430 s on 2026-11-16 (prompt 54, run 38), and
20 ms is the first period qualified for 30 minutes. These are local transmission confirmations and driver receive
bounds, not wire times; Q27 (same-cycle sample identity) stays open. Both TPDO1 were restored to type 255, the value
read after the power cycle, in Pre-operational ([record](../bench/20261116-hil/18-tpdo1-restore/)).

## S21 discriminator and `2500` persistence

Node 1, `2201=51200`, velocity through the range probe: 60 rpm clean, then **3000 rpm with 500 ms ramps clean**
(cruise 2972 rpm, ratio 0.991). On 2026-11-13 the same speed with 2000 ms ramps was clean and FF05 came only from
3200 rpm (node 2) and 3516 rpm (node 1). Four times the acceleration at the same 2.56 MHz did not fault, so the
limit fits a command-pulse rate ceiling and not the `250A` deviation threshold (I, one run). No FF05, so no
enable-fault state. Two preparations were needed first: the console fast stops left node 1 in the `0014` latch
(running set, in place clear), from which no HALT path starts (S20), and a 17 s quick-stop run (a settled stop
record) then a mode 1 enable (word 7 clears the latch) and a stationary quick stop gave `0010`. The final quick
stop left `0014` again, so `2201` stays 51200 until the next power cycle (measured volatile on 2026-11-13).

Node 2: `2500` written to 1 (open loop) without a save, read back 1, drive released. The next power cycle shows
whether it persists (backlog S21, prompt 72 bench step 1).

**After the user's power cycle (about 16:55Z): both are volatile.** Both drives read the power-up report
(`2100=0001`, `6041=8037`, `6061=00`), then took mode 1 and the best-effort `000B` release as before. Node 2's `2500`
read 2 (closed loop, `2501` 4000), so the unsaved 1 did not persist (series S0105, loop route `unchanged`). Node 1's
`2201` read 1000, so the unsaved 51200 did not persist (series S0106/S0107, subdivision route `unchanged`); node 2's
`2201` showed the same on 2026-11-13. One power cycle each, without a save; nothing was written to restore them.

## Tooling findings

- `r` (release) can be refused just after `END trial` prints: the firmware still holds its dumping flag
  ([`main.cpp`](../../examples/s2_bench/main/main.cpp), `!dumping && route->release()`). Twice in this campaign;
  the executor now waits 0.4 s after `END trial`. Neither caused motion; both ended on a fast stop.
- `scripts/s2_bench_console.py` loses records between invocations (above) and stops at the first `ok:false` record,
  which an interrupted motion's `result` always is.
- Console `debug raw` printed no trace records on this bench in the traced move trial; the replacement's
  observation cycles remain unrecorded.
- STATS showed `conRej` up to 1524 and `traceOver` 417 in one console boot: console output refusals under load,
  another way records can be lost; recorded, not investigated.
- On `b7033c12`, `s2_bench_console.py` gained `Ta`/`Te`, `d`, `wait ID`, `pause MS` and `release-all`. Two axes'
  moves can now run in one invocation, with per-axis and group stops sent while they move.

## Not run, and why

- Synchronous RPDO start/stream and the shared quick-stop RPDO: M09/Q28 still gate them.
- DI5/DI6 stop: no wired fixture.
- 10 ms SYNC for 30 minutes: known to fail at about 430 s (prompt 54, run 38); prompt 56 owns the throughput fix.
