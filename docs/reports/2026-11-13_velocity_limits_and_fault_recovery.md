# Velocity limits, acceleration, microstepping and fault recovery, 2026-11-13

Bench session on the two-drive S2 fixture (CL86-C, one firmware: `1000=00040192`, `1009=000C`, `100A=0020`), velocity
mode 3, under the user's written waiver for this campaign: "Unattended, I accept it" (the option text: "Waiver for
this ladder campaign only: run the velocity ladder, above-limit probe and resolution grid without me at the bench. I
take responsibility; motors bolted, free shafts."). Requests that shaped it, verbatim: "check those boundaries with
practical tests - no need for stopping after first signs of problems - explore the boundaries well"; "try these
motors and how they behave regarding the speeds but also accelerations and microstepping"; "for the acceleration
sweeps - only test these at the edge speeds where it could matter"; "first read the datasheets and try different
approaches to recover a fault driver so you can recover and test more thoroughly the settings and be sure you can
recover from that". One physical observation, relayed by the user during the first fault: "I see the alarm on node 1
- record the shaft turns free and red light blinking." Every other result below is a drive report.

Evidence: [20261113-ladder](../bench/20261113-ladder/NOTE.txt) (log, one JSON line per run, 152 packed record
directories, tools). Images: `28f23ec` (range probe, `2201` write), `6dee27b` (alarm clear in mode 3), `abe945b`
(controlword clear method, NMT reset experiments), `b350da4` (controlword experiment route); each flash and ping is
in its own `docs/bench/20261113T*` directory. Manual facts: [fault recovery and motor limits](2026-11-13_fault_recovery_and_motor_limits.md),
[units re-audit](2026-11-13_units_reaudit.md).

## Method

- Each run: release in mode 3, `60FF`/`6083`/`6084`, words 6, 7, 15 (ZH86 p13), 12 s at the rate, the run's own stop
  at due (HALT when the start report had no running latch, otherwise a quick stop), then reads. One run per MCU
  boot and campaign ledger (runner caps). Cruise speed comes from the record's own `6064` uploads and their times.
- Runs that ended with a quick stop show `DEADLINE` as their only failure: the stop's settlement needs two still
  cycles after its first, and no 12 s plan fits that in the record's 128 terminals. The drive itself stopped within
  about 7 ms each time (`6064` unchanged 7 ms after the stop at 60 rpm, trial 5112) and read released afterwards.
  These runs are counted as clean when running was observed and the cruise was at least 90 % of the command.
- A fault (`FF05`) is recorded, the drive recovered (below), and the climb continues: a failing step repeats once,
  the boundary is bisected to the larger of 100 rpm and 5 %, and higher steps then run once each.

## Speed (2201 = 1000)

| Node | Clean | First fault (FF05) | Notes |
| --- | --- | --- | --- |
| 1 | 60 to 4000 rpm, 4125 rpm | 4250 rpm (4500 twice) | 2000 ms ramps from 1200 rpm |
| 2 | 60 to 4000 rpm, 4500 rpm | 4625 rpm (4750, 5000 twice, 5500, 6000) | same |

- The measured cruise was 0.987 to 0.992 of the command at every clean step (`6064` slope), the same ratio the night
  soak found; `606C` read within a few rpm of the command.
- 3001 rpm and every step above 3000 ran exactly like the steps below it: the drive does not clamp at the documented
  3000 rpm.
- 1 rpm: `606C` never left 0, so the library never saw the run as running (its rule needs a nonzero `606C`).
- The FF05 came about 0.5 s after the ramp should have reached the speed, with no stop sent (node 1, 4500 rpm, trial
  5198: EMCY at 2.51 s after word 15's acknowledgement, the run's stop due 9.4 s later). The user suspected the fast
  stop; the record rules it out, and none of about 40 quick stops at up to 4000 rpm raised an alarm.
- The two specimens differ by about 400 rpm in top speed. The cause (motor, wiring, supply, tuning) is not
  established; the supply voltage is not recorded.

## Acceleration at the edge speeds (2201 = 1000)

| Speed | 1000 ms | 500 ms | 300 ms | 200 ms |
| --- | --- | --- | --- | --- |
| 1000 rpm, node 2 | clean | clean | clean | not run |
| 3500 rpm, node 2 | clean | clean | | clean (overshoot to 3626 rpm in the first report) |
| 4000 rpm, node 1 | clean | clean | | FF05 |
| 4000 rpm, node 2 | clean | clean | | FF05 |

So near the top speed both rate and acceleration matter: 4000 rpm in 200 ms (about 333 turns/s²) faults on both
drives, 3500 rpm in 200 ms (about 292 turns/s²) does not on node 2. The ramp is taken as the time from rest to the
commanded speed (EN86 p11 figure; units re-audit).

## Microstepping (2201, node 1)

| 2201 | Clean (rpm, pulse rate) | First fault |
| --- | --- | --- |
| 1000 | to 4125 (69 kHz) | 4250 |
| 4000 | to 4250 (283 kHz) | 4375 |
| 10000 | to 4250 (708 kHz) | 4375 |
| 25600 | to 4000 (1.71 MHz) | 4500 (not bisected) |
| 51200 | to 2344 (2.0 MHz) | 3516 (3.0 MHz) |

- A `2201` write took effect at the next enable, without a power cycle: at 60 rpm `6064` rose 3947, 9868, 25266 and
  50526 counts/s at 4000, 10000, 25600 and 51200. `6081`/`60FF` stay motor rpm whatever the subdivision, and
  `6064` counts in the new pulses. Every value was restored to 1000 and read back.
- There is no 200 kHz or 500 kHz limit on this bus drive: runs were clean at command pulse rates up to 2 MHz. The
  speed limit is the motor's (about 4100 to 4600 rpm here); finer resolution raised node 1's limit slightly (4250
  instead of 4125).
- At 51200 the limit fell to between 2344 and 3516 rpm. Two explanations fit and neither is established: a pulse
  rate ceiling between 2 and 3 MHz, or the deviation threshold `250A=4000`, which in command pulses is only about
  28° of rotor angle at 51200. Also at 51200, after one alarm clear the FF05 came back (below), which fits the
  second explanation.

## Fault and recovery

After an FF05 the drive reads `2100=001D`, `6041=C018`, `1001=81`, `603F=FF05`, releases torque (shaft free, red LED
blinking: user's observation; EN86/ZH86 p20 list 5 flashes per 5 s for position out-of-tolerance) and `6064` keeps
following the coasting rotor. Approaches tried, in order:

| Approach | Result |
| --- | --- |
| `prepareClearAlarm` before 2026-11-13 | Refused: it required mode 1, and the mode 1 write is refused while the error register is set |
| `2301` = 1 then 0 (`prepareClearAlarm`, `OBJECT_2301`, now admitted in mode 3) | 9 attempts: 8 cleared the alarm and left `6041=8010`, `2100=0001` (low bits 000, not released); once (51200) the FF05 came back right after |
| NMT Reset Node to the drive in `8010` | Boot-up frame within about 1 ms; mode, position, `60FF` and the `8010` report unchanged. Not a power-cycle substitute |
| `6040` = 0x0000, 0x000B or 0x0006 in `8010` | No change (trials 5402 to 5404) |
| `6040` = 0x0080 in `8010` | Released at once: `8050`, `2100=0011`, mode and position kept (trial 5405; then 8 of 8) |
| `prepareClearAlarm`, `CONTROLWORD_BIT7` (`6040` = 0x0080 then 0x0000) | 11 of 11 on both drives: alarm cleared and released (`8050`/`0011`) in one step, including the 51200 fault the `2301` clear could not hold |
| Toggling enable (6, 7, 15) | Not a documented reset; not tried: in mode 3 it starts the motor at the stored `60FF` |

After every recovery the next run was clean (first: node 1, 60 rpm, HALT settled, trial 5343). Recommendation: use the
controlword method as the recovery; the `2301` method needs the extra `6040` = 0x0080 word afterwards. It became the
library's default the same afternoon (`8090d85`).

## Library and bench changes made for this session

`28f23ec` range probe (up to 6000 rpm, attended experiment only), over-speed stop, typed volatile `2201` write;
`6dee27b` alarm clear in mode 3; `abe945b` controlword clear method, measured mode 3 completion, NMT reset
experiments; `b350da4` single-controlword experiment route. All host-tested; CI runs on each push.

## Final state

Both drives released and alarm-free in mode 3 (`6041=8050`, `2100=0011`), `2201=1000`, `60FF=60` (written and read
back as hygiene), `6064` far from zero (node 1 about 183 million counts after the high-resolution runs). Not run: the
position-mode ladder, reverse velocity, `2201` outside the five grid values, the 25600 bisection, whether a `2201`
value survives a power cycle.

## Afternoon: position mode, the 51200 limit and the enable-fault state

The user then asked: "Do this> the position-mode speed ladder, reverse velocity, and pinning down the 51200 limit",
and "Alarm clear - pick the best method". The controlword reset became the library default (`8090d85`).

**Position mode (node 2, 2201 = 1000).** Finite moves through the library's `move` route under the measured move
record, travelling long enough to cruise (half of each ramp plus 1.2 s at speed):

| rpm | Peak `606C` | Peak `6064` slope | Arrival error |
| --- | --- | --- | --- |
| 300 | 300 | 298 | 0 |
| 600 | 596 | 595 | 0 |
| 1200 | 1192 | 1154 | 0 |
| 1800 | 1786 | 1730 | 0 |
| 2400 | 2381 | 2307 | 0 |
| 3000 | 2975 | 2884 | 0 |

Every move ended exactly on its target. The slope column is coarse (one to two second samples). Node 1's position
ladder could not run (below). Above 3000 rpm position mode needs its own probe (`6081` is capped at the documented
3000 rpm in the library); not done.

**The 51200 limit.** Node 2, ascending at 2201 = 51200 with 2000 ms ramps and stopping at the first fault: clean at
60, 2344, 2600, 2800 and 3000 rpm (2.56 MHz), FF05 at 3200 rpm (2.73 MHz). Node 1 had been clean at 2344 and faulted
at 3516. Both drives were far below their own speed limit (node 2 runs 4500 rpm at 1000 pulses per turn), so at 51200
the limit is about 2.6 to 2.7 MHz of command pulses on both drives. A pulse-rate ceiling fits this; the deviation
threshold (`250A`, about 28° at 51200) is not excluded. The discriminating run (3000 rpm with a 500 ms ramp) could not
be made: the drive faulted at the enable first (below).

**The enable-fault state.** After an FF05 at 51200, both drives (node 1 at 15:22Z, node 2 at 15:48Z) fault at every
later enable. At enable word 7 (`6041=8033`, before word 15) the drive reports 175 to 256 rpm in `606C` with no motion
command. The library's velocity enable refuses it at once (word 7's cycle must be stationary), and the drive raises
FF05 (`6041=803B`). On node 1 at 51200 the rotor moved about 7300 counts (about 51°) before the fault. Nothing
software could reach cleared it: the controlword clear and the `2301` clear (both remove the alarm, and the next
enable faults again), counter zero (`6064` to 0), NMT Reset Node, and `2201` back to 51200 or 1000. The explanation
first written here (the command position kept through coasts, recovered by the loop at enable) does not hold: the
`6064` step at word 7 is a routine counter switch, clean enables had larger steps, and one faulting enable had none.
The later investigation, with the power cycle that cleared the state, the loop selector, the open-loop enable and
`6040=0x0000`, is in [the enable-fault report](2026-11-13_enable_fault_state.md).

**Reverse velocity.** Implemented and host-tested (`8090d85`: `VelocityEvidence::reverseProbe`, runner `--reverse`,
route protocol 12) and flashed, but not run: both drives are in the enable-fault state.

Both drives were left released and alarm-free (`6041=8050`, `2100=0011`) at `2201=1000` with `60FF=60`. Their next
enable faults until a power cycle.

## Open questions

- Why the top speed differs by about 400 rpm between the specimens; the supply voltage and motor nameplates.
- The 51200 limit: pulse-rate ceiling or the deviation threshold in pulses (a write of `250A` would separate them;
  it is not authorized).
- The enable-fault state: a power cycle cleared it, nothing else did; what it is stays open
  ([enable-fault report](2026-11-13_enable_fault_state.md)). Then the reverse runs and the 51200 discriminator.
- The cruise ratio of about 0.99 (units re-audit) and the step back of `6064` at enable.
