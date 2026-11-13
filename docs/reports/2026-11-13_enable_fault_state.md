# Enable-fault state (backlog S21): investigation, 2026-11-13

The user's request after the first power cycle: "power cycled - investigate the finding thoroughly and investigate
all other possible options". Two CL86-C specimens, one firmware (`1000=00040192`, `1009=000c`, `100A=0020`), S2 bench
on COM15, under the user's unattended waiver for this campaign. Every value below is a drive report unless it is
marked otherwise. Labels: **M** measured, **I** inferred, **D** documented. Earlier context:
[afternoon report](2026-11-13_velocity_limits_and_fault_recovery.md#afternoon-position-mode-the-51200-limit-and-the-enable-fault-state)
and the [source audit](2026-11-13_enable_pull_sources.md). Evidence: `docs/bench/20261113-ladder/` (series S0163 to
S0197, `night.log`, `ladder.jsonl`, the NMT plans' directories).

## Summary

- **Trigger (M, 3 of 3).** An FF05 raised while running above the command-pulse ceiling at `2201=51200`: node 1 at
  3516 rpm (trial 5612), node 2 at 3200 rpm (trial 5881), and node 2 again at 3300 rpm (trial 5935) right after a
  power cycle had cleared the state. FF05s at `2201` 1000, 10000 and 25600 (speed and acceleration limits) were
  followed by clean enables.
- **Signature (M).** At enable word 7 the drive reports `6041=8033` and `606C` 121 to 256 rpm with no motion command,
  then FF05 0.50 to 0.51 s after word 7's acknowledgement (`6041=803B`, `2100=0019`). Each attempt moved node 2's
  released position about 8,150 counts (about 0.16 turn at 51200) toward the frozen command position.
- **Recovery (M).** Only a power cycle cleared it. No software option did, including those tried for the first
  time today: NMT Reset Communication, the loop selector `2500` toggle, an open-loop enable, `6040=0x0000` from the
  switched-on state, and a second NMT Reset Node.
- **New library gap (M).** In open loop the velocity enable refused at word 7 and left node 2 switched on and
  energized (`8033`, `2100=0001`, no alarm). No library action releases that state. The bench's
  [controlword route](../hardware_bench.md) now admits `6040=0x0000` there (`0db0c1f`), and that word released the
  drive (`8050`, `2100=0011`).
- **Prevention.** Keep `rpm × 2201 / 60` below about 2.5 MHz. The cause inside the drive is not known.

## The two counters (M, with I where marked)

While released, `6064` follows the rotor, including every coast after a quick stop or a fault (I: encoder). From
enable word 7 on it shows a second counter that does not include the coasts (I: the command position). The step at
word 7 takes one 42 ms cycle with `606C` 0, so it is a switch of counters, not motion ([audit, section
1](2026-11-13_enable_pull_sources.md#1-6064-and-606c)). The step grows by each coast and not by HALT stops, and a
power cycle or counter zero (`2302`) resets both counters:

| Node 2 trial | Run before | Word 7 step | Note |
| --- | --- | --- | --- |
| 5266 to 5290 | quick stops 2400 to 4000 rpm, `2201=1000` | −2,365 to −13,332 | Grows with every quick-stop coast |
| 5294 to 5302 | quick stops at 1000 rpm | −16,709 to −17,094 | About 190 counts per stop |
| 5684 to 5740 | HALT stops at 60 rpm | −47,946 to −47,929 | HALT adds nothing |
| 5922 | First enable after the power cycle, `2201=51200` | 0 | Clean |
| 5932 | After one 2344 rpm quick stop | −51,843 | Clean; about one turn of coast |
| 5935 | After a HALT | −51,581 | Clean |
| 5873, 5877, 5881 | Quick stops at 2800 and 3000 rpm, before the first fault | −65,470, −142,394, −232,590 | All clean at word 7 |
| 5942, 5951, 5965, 5976 | Fault state, after the 3300 rpm FF05 | −161,279, −153,112, −144,908, −136,818 | `606C` 256 at word 7, FF05 |

Clean enables had steps up to 232,590 counts, and faulting enables had smaller ones; one faulting enable on node 1
(trial 5733, after `2302`) had none. So the step does not cause the fault. In the fault state, word 7 always showed
the same command count on node 2 (26,655,924 at 16:28Z to 16:45Z), while the released count fell 8,167, then 8,204,
then 8,090 counts with each faulting attempt (I: the closed loop turned the rotor toward the command position for
0.5 s, then faulted).

## Recovery options tried

Node 2 unless stated; each step in a fresh MCU boot.

| Option | When (UTC) | Result |
| --- | --- | --- |
| `6040` 0x0080 then 0x0000 (library default clear) | Node 1, afternoon; 16:27 to 16:44 | Alarm cleared, released `8050`/`0011`; the next enable faulted |
| `2301` 1 then 0 | Node 1, afternoon | Same |
| Counter zero `2302` | Node 1, trials 5733, 5835 | Both counters 0; the next enable faulted |
| `2201` to 51200 or 1000 | Both nodes, afternoon | No change |
| NMT Reset Node | Node 1 afternoon; node 2 17:02 | Boot-up frame only; reports unchanged |
| NMT Reset Communication | 16:28 | Boot-up frame only; the next enable faulted (16:30) |
| `2500` 2 → 1 → 2, released, read back | 16:42 to 16:44 | Written and confirmed; the next enable faulted (16:44) |
| Open-loop enable (`2500=1`, 60 rpm) | 16:45, trial 5976 | Word 7: `8033`, `606C` 256, `6064` the command count. The library refused before word 15; no FF05. The drive stayed switched on, `8033`, `2100=0001` |
| Stop route on that state | 16:45, 16:47 | Refused: a stationary report has nothing to stop |
| `6040=0x0000` from `8033` | 17:00, trial 5991 | Released, `8050`/`0011`. `606C` then read 1711 with `6064` at the command count, both constant from 17:00 to 17:03 across MCU boots |
| `2500=2` again, released | 17:01 | Written and confirmed; reports unchanged |
| Power cycle (user) | Before 16:17 | Cleared both drives: power-up `8037`/`0001`, `6064` 0; node 2 then ran clean at 60 and 2344 rpm at 51200 |

Not tried: factory restore `2300` (rewrites every parameter; not authorized), DI function 4 (not wired), and a mode 1
enable. The mode 1 route was possible before the open-loop test; now `606C` reads nonzero, and every enable route
refuses it, as it must for a drive that might be turning.

The held `606C=1711` while released is not a rotor speed (I): a released rotor cannot hold a steady 1711 rpm for three minutes.
No one observed the shaft. In open loop `6064` shows the command count, so it cannot show the rotor.

## What the state is

Not established. The records fit a persistent internal condition that `6064` does not show, which makes `606C`
nonzero whenever the drive leaves the switched-off states and makes the closed loop pull and then fault (I). It
begins with an FF05 during a run above the pulse ceiling. The 0.5 s delay matches no documented threshold or timer
(D, by absence). The ceiling itself is about 2.6 to 2.7 MHz on both nodes: node 2 was clean at 3000 rpm (2.56 MHz)
and faulted at 3200 (2.73 MHz); 2344 rpm with a 200 ms ramp was clean. That supports a pulse-rate limit rather than
the `250A` deviation threshold, which the 3000 rpm, 500 ms ramp run would separate (not done).

## Consequences

**For applications (project guidance).**

- Keep the commanded pulse rate, `rpm × 2201 / 60`, below about 2.5 MHz on this firmware. At `2201=1000` the speed
  limit (about 4100 to 4500 rpm, 70 to 75 kHz) comes first.
- After any FF05 at a fine resolution, prove the drive with a short slow enable before trusting it; a cleared alarm
  is not proof.
- If a short slow enable reports `606C` nonzero at word 7, stop enabling and plan a power cycle.

**For the library (proposed, not implemented; backlog S21; planned in [prompt 57](../prompts/57_regular_drive_operations.md)
for the release and [prompt 72](../prompts/72_enable_fault_guards.md) for the rest).**

1. A pulse-rate admission check: refuse a velocity or position rate whose `rpm × 2201 / 60` exceeds a profile limit,
   with the measured value as evidence and the margin as project policy.
2. A release path for a switched-on, stationary, alarm-free drive. The velocity enable's word 7 refusal leaves the
   drive switched on, and neither the stop (stationary) nor the clear (no alarm) admits it. `6040=0x0000` released
   `8033` once on this firmware, in open loop (trial 5991).
3. On `606C` nonzero at word 7, send that `0x0000` at once instead of waiting for the drive's FF05. Whether this
   prevents the fault or the 0.16-turn pull in closed loop is untested.

Dropped from the earlier proposal: refusing an enable when `6064` and the last commanded position differ. The
difference is routine (table above) and was zero in a faulting enable.

## State left and next measurements

- Node 2: released `8050`/`0011`, mode 3, `2201=51200`, `2500=2` (read back), `606C` reading 1711, `6064=26655924`,
  `60FF=60`, in the enable-fault state. Every enable route refuses it while `606C` is nonzero.
- Node 1: clean since the power cycle; after its reverse runs, released (`6041=C050`, `2100=0015`), `2201=1000`,
  `60FF=60`.
- 17:30Z (user, relayed): "i see both drives green leds - not cycled yet". At 17:33Z a check through the library:
  node 1 ran 60 rpm (running reported, cruise ratio 0.987, released afterwards); node 2 was refused before any write
  (`606C` 1711 while released, trial 6036). Node 2 is not operable until a power cycle; its green LED matches its
  report of no alarm, which this state does not need.

Next, each needing a power cycle:

1. Power cycle node 2, then read `2201` and `2500`: they were never saved, so this shows whether they persist.
   Restore `2201=1000` afterwards.
2. In a reproduced fault state (one 3300 rpm FF05 at 51200): a mode 1 enable, and proposal 3 timed within 100 ms
   of word 7.
3. The discriminator: 3000 rpm with a 500 ms ramp at 51200.
4. Node 1's position ladder and node 2's reverse runs, still owed to prompt 59. Node 1's reverse runs were made
   at 17:08Z to 17:13Z, all clean ([afternoon report](2026-11-13_velocity_limits_and_fault_recovery.md)).
