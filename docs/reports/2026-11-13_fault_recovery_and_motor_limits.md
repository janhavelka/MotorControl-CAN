# Fault recovery and motor limits, CL86-C (2026-11-13)

Read-only research. Labels: **D** documented (original page or EDS), **M** measured (drive reports, two CL86-C
specimens, firmware `dt=00040192 hw=000c sw=0020`, `2201=1000`), **I** inferred. PDF pages rendered with PyMuPDF and
viewed: EN86 p3, p4 (two zoomed crops), p11, p12, p13, p15, p16, p18, p19, p20; ZH86 p12, p13, p18, p20; OP86 p5,
p10, p13, p14 (CL57-C headers, D-08). ZH57/OP57 and other pages: extracted text only. Records:
[ladder](../bench/20261113-ladder/), trials 5194, 5198 to 5212, rows to 12:49Z.

## 0. The FF05 event (M, raw trace)

- Trial 5198, node 1, mode 3, `60FF=4500`, ramps 2000 ms: `000F` acknowledged at t=15.525 s; the one running cycle
  (16.27 s) read `606C=600`. No frame went to node 1 between 16.272 s and the EMCY `05 FF 81 00 ...` at 18.037 s,
  2.51 s after the `000F`.
- The quick stop (5199) came 8.7 s later, and its first reads already showed `2100=001D`, `6041=C018`. **No stop
  preceded the alarm.** The ladder's 41 quick stops from 1 to 4000 rpm on both nodes raised none.
- `6041=C018` sets bit 3, ZH86 p12's alarm bit (EN86 p12 says bit 7). `1003` count 0. `2102` went `0000` to `0001`
  (I: DO0 alarm output, fitting `2400:0D=1` under the EDS layout).
- The `mode1` refusal (5208, `why=error-register`) was the bench route's own check: only uploads went out. Node 1 was
  retired at 12:29Z still alarmed, `60FF=4500` retained.

## 1. Alarm recovery

| Route | Evidence | Status |
| --- | --- | --- |
| `6040` bit 7 | D: EN86/ZH86 p15, OP86 p13, "Alarm reset": set bit 7 to 1 "to reset the drive alarm. It should be set to 0 manually." ZH86 p12, OP86 p10: `15->128` 报警复位 in the position-mode table; EN86 p12 lacks the row | Not implemented (Q13), never run. `0x0080` clears bits 0..3; p12 pairs `6040=0` with low bits `000`, so `6->7->15` would follow (I) |
| `2301` | D: EN86 p18 "Alarm reset, 0 noneffective, 1 effective", U16 RW; EDS `Obj.ClearAlm` U16 RW, PDO, default 0. ZH86 p18 omits it | `prepareClearAlarm` (1 then 0), now admitted in mode 3 when released ([fault actions](../fault_reference_actions.md)). Never run on an alarm |
| Toggle enable `6/7/15` | D: no page says it clears an alarm; neither manual has a CiA402 state-machine figure, only the p12 tables | Not measured. In mode 3 the enable starts motion at the retained `60FF` (D: ZH86 p13 step 3 "电机使能, 电机开始运行"; M: every velocity trial): with 4500 retained it is a 4500 rpm command. Not a reset |
| DI input | D: DI codes 0..13 (EN86 p8, p18) include no alarm clear; 4 is enable/MF | MF effect on alarms undocumented; no DI wired |
| Power cycle | D: p20 marks 1 to 3 flashes "Restart reset", 4 and 5 "Resettable" | I: a restart also clears 4 and 5. Not measured for FF05 |

No source names a mode, state or standstill for a reset, or the state after one (D, by absence). FF05 left the drive
released (M: `2100` bit 4; user: shaft free), so expect a released drive needing a new enable after a clear (I).
`603F` latching is undocumented (C07), and the library's enable needs `1001=0` and `603F=0`
([drive actions](../drive_actions.md)): a `603F` still reading FF05 after a clear keeps enable refused.

## 2. LED flash codes

EN86/ZH86 p20, OP86/OP57 p14 (D). No colours are documented: EN86 p4 draws two lamps under one label "Power
supply/alarm".

| Flashes per 5 s | Alarm | Causes | Reset |
| --- | --- | --- | --- |
| 1 | Overcurrent | Motor lead short, motor or drive damaged | Restart |
| 2 | Overvoltage | Supply too high, drive damaged | Restart |
| 3 | Undervoltage | Supply too low, drive damaged | Restart |
| 4 | Memory read/write error | Drive damaged | Resettable |
| 5 | Position out-of-tolerance | Phase order, phase loss, encoder disconnected, load blocked, too fast | Resettable |

FF05 ("too high position deviation", EN86 p16) matches 5 flashes by name only (I, D-26). The user saw "red, blinking"
without a count; count one 5 s window next time.

## 3. Threshold `250A`

D: EN86 p19 "Out-of-tolerance alarm threshold", ZH86 p20 超差报警阈值, "factory default, adjustment not required
generally", U16 RW; EDS `Obj.PosAlrm` U16 RW, PDO, no default or limits. Unit, range and scaling with `2201` are
undocumented. M: both read 4000. No typed write; widening it would hide faults.

| If `250A` counts | At `2201=1000` | At `2201=51200` |
| --- | --- | --- |
| Encoder counts (`2501=4000`, quadrature unestablished) | 1 turn | 1 turn |
| Command pulses | 4 turns | 0.078 turn (28°) |

I: the `6064` rate of 0.99 × command is not a lag the alarm sees. In `6064` units it would pass 4000 after 6 s at
4000 rpm, and 12 s runs there passed.

## 4. Acceleration, torque and top speed

D: no torque-speed curve, rated speed, inertia or step-rate limit in any retained manual or the product page
(searched). Matched motor "60/86" (EN86 p3). 8 A maximum output current (p3); `2502` reads 8400 mA, unexplained (I:
5.8 A × √2 = 8.2 A would fit a peak setting for the LC60H2127's 5.8 A, recorded in the
[2026-11-02 plan](2026-11-02_pdo_motion_plan.md) from an unretained page). Supply ranges conflict (Q19); the bench
voltage is unrecorded. `60FF` 0..3000 rpm (EN86 p13); ramps 0..2000 ms (p11, p13); a fixed ramp time makes
acceleration proportional to the target (I, [units re-audit](2026-11-13_units_reaudit.md)).

| Node | Clean (M) | Failed (M) |
| --- | --- | --- |
| 1 | 60..4000 rpm, 2000 ms ramps | FF05 at 4500, 2000 ms (2250 rpm/s) |
| 2 | 60..4000 rpm, 2000 ms; 1000 rpm with 300 ms; 3500 rpm with 500 ms (7000 rpm/s) | None yet |

I (general physics): torque falls with speed as winding inductance and back-EMF use up the supply voltage. An
unloaded rotor needs only J·α plus friction, so voltage, phase current and the loop set the top speed. Beyond it the
rotor lags, the lag reaches `250A`, and FF05 fires (cause 5, "too fast"). 7000 rpm/s at 3500 passed while 2250 rpm/s
to 4500 failed, so speed limited node 1, not acceleration. Shorter ramps add risk near the top speed or with inertia.

## 5. Stop behavior

D: `605A` and `6085` are absent from the EDS; `605D` (I16, default 1) has no option meanings; arrows `15->11` (EN86,
ZH86 p12) and `15->271` (ZH86 p12). No stop deceleration is documented.

M: `000B` releases the drive (`2100` bit 4, low bits `000`). From 4000 rpm `606C` still read 2936 (node 1) and 3028
rpm (node 2) about 45 ms later, then the shaft coasted about 2000 counts more; coast grows roughly with speed squared
(65 counts at 1000 rpm, 953 at 3000, 2074 at 4000). At 60 rpm it stopped within about 7 ms. A 2000 ms `6084` ramp from
4000 rpm would cover about 66,700 counts, so the quick stop ignores `6084` (I). HALT distances fit `6084` (I from M,
Q11).

Stop-caused FF05: no source (D). A released motor has no loop to lag (I). A HALT with a short `6084` against load
inertia could raise it (I).

## 6. Subdivision `2201` and loop mode `2500`

D: `2201` 0..65535 (EN86 p11) against 400..51200 (EN86/ZH86 p18); EDS U16 RW, default 0. No page states an effect on
resolution, smoothness, torque or pulse rate. `607A` is "number of pulses", ±1,000,000 (p11). `2500`: 0 noneffective,
1 open loop, 2 closed loop (EN86 p19). M: 1000 pulses per turn at `2201=1000`; `2201` never written (Q04).

I: at `2201=N` the `607A` range is ±1,000,000/N turns (±19.5 at 51200); 4000 rpm is 66.7 k pulses/s at 1000 but 3.4 M
at 51200, where any rate ceiling would show; `6064` wraps after 2^31/N turns (about 10 minutes at 4000 rpm, 51200).
Whether rpm objects stay motor rpm is unmeasured. A low-speed FF05 after raising `2201` would show `250A` counts
command pulses. Open loop: whether the deviation check runs is undocumented; the "encoder disconnected" cause implies
it uses the encoder; lost steps go uncorrected and possibly unreported. No `2500` write has been accepted, and the
library refuses one while alarmed ([tuning](../tuning.md)).

## 7. Recommended test plan

Before: record supply voltage and current limit and the motor nameplate. One node at a time, unloaded shafts. In mode
3 write `60FF` to the intended speed before any enable.

1. **Find the clear route on node 1's present alarm** (if not yet power-cycled): copy the EMCY, count flashes, read
   ERRORS and ERROR_HISTORY, run `prepareClearAlarm` once. If the alarm persists, decide on one attended
   `6040=0x0080` then `0x0000` (documented; needs a library route). Power cycle last. Until a CAN route works, each
   FF05 costs a power cycle.
2. **Healthy again:** `2100` bit 3, `6041` bit 3 and `1001` clear; `603F` recorded (may stay FF05); `2102` bit 0
   back to 0; LED pattern gone. Then `60FF=60`, `6083=6084=1000`, mode 3 read back, `6->7->15` (low bits `111`,
   `2100` bit 4 clear), 10 s at 60 rpm with `606C` within a few rpm, `6064` slope about 990 counts/s, no EMCY; then
   HALT, and quick stop to a released standstill. Re-baseline `6064` after each enable: it stepped back 10,113 to
   16,019 counts at the energize word in the 3600, 4000 and 4500 runs (M, unexplained).
3. **Top speed**, 2000 ms ramps: 4100, 4200, 4300, 4400 rpm per node. Stop at the first FF05, clear, check, repeat
   the last clean step once.
4. **Acceleration** at 3000 rpm and at the top clean speed minus 300: ramps 1000, 500, 300, 200, 100 ms (zero is
   refused and undocumented).
5. **Stops** at 3000 rpm: HALT with `6084` 1000, 500, 300, 200 ms, recording distance.
6. **Subdivision**, only after a reviewed `2201` write: 1000, 4000, 10000, 51200. Read back `2201`, `2501`, `250A`;
   one-turn move for pulses per turn; 60 rpm for rpm units; then 600, 1200, 2400, 3000 rpm; volatility after one
   power cycle.
7. **Open loop** (`2500=1`) only as the attended T1 trial with a phase-current probe.

Record per step: command and readback, `6041`, `2100`, `606C`, `6064`, EMCY, `1001`/`603F` after the run, LED count.
