# Units re-audit: speed, ramp, position and count objects, and the 0.992 rate (2026-11-13)

User request, verbatim (2026-11-13): "Units - document this well - especially the rpm problem - reaudit the datasheet
for this finding". Earlier (2026-11-12): "Native units only - extend this (explore the possibilities so its physically
achievable, industry standard - simplest functional)".

Read-only research on `main` at `c864c7e`. The working tree held the lead's uncommitted edits (`docs/README.md`,
`docs/backlog.md`, prompts 59 and 62, prompt index files); they were read, not changed. Nothing was flashed or sent to
a drive; the only compilation was a scratch program in the session scratchpad (section 5). MotorControl-RS was checked for the ramp and acceleration seam only: HEAD
`0984e14`, clean. Its ESS profile also keeps ramp times as native words (`accelerationTime`, `decelerationTime` in
`include/MotorControlRS/profiles/ess_rs/Position.h` and `Velocity.h`, "never physical acceleration") and has no
acceleration-to-ramp conversion, so there is no RS behavior to adopt here. FieldCore was not inspected (no ownership
seam is involved). The user-facing result is [Speed, ramp and position units](../speed_units.md).

## How to read the evidence

- **Visual**: I rendered the original PDF page to an image this session (PyMuPDF, 110 dpi; figure crops at 220 dpi)
  and read the image.
- **Text**: I extracted the text of every page of all five PDFs this session and searched it; I did not look at the
  image of that page.
- **EDS**: I read `docs/reference/vendor/LC_StepDriverCAN_V1.1.eds` directly (every object name, type and default).
- **Measured**: retained bench records, recomputed by me where a number is given with a method. Two CL86-C specimens
  (nodes 1 and 2), one firmware identity (`1000=00040192`, `1009=0C`, `100A=20`, `1018:01..03=1000/1/0`,
  `2000=0400`, `2001=0200`), `2201=1000`, `2500=2`, `2501=4000`. Nothing is qualified.
- **Inferred**: my arithmetic or reasoning, never a device fact.

Abbreviations as in [device conflicts](../device_conflicts.md): EN86, ZH86, ZH57 are the 86C English, 86C Chinese
and 57C Chinese CANopen manuals; OP86 and OP57 are the two "operating manuals" (操作手册). Page numbers are one-based
PDF pages (PDF p11 is printed page 9).

**Visually reopened:** EN86 p3, p11 (and figure crop), p12, p13, p15, p16, p17, p18, p19, p20; ZH86 p11 (and figure
crop), p13, p17, p18, p19, p20; ZH57 p11, p13; OP86 p9, p11; OP57 p9.

**Text only:** all other pages of the five PDFs, including EN86 p6 and p14, ZH86 p3, p12, p14 to p16, ZH57 p3, p12,
p14 to p20, OP86 p1 to p8, p10, p12 to p14, and OP57 apart from p9; the product page snapshot
`CL86-C_manufacturer_product.html`; the download index. EN86, ZH86 and ZH57 p4, p5, p7 and p9 carry raster drawings
(dimensions, connectors, wiring); I did not reopen them, and their extracted labels hold no speed or time term.

## 1. Objects: unit, type, range and wording

ZH57 repeats ZH86 word for word on every page listed below except the header, p3 specifications, p12 status bit and
p17 output bits (text diff). OP57 repeats OP86 on every page apart from the cover model name and the section
numbering on p2 and p4 (text diff). So a claim cited for ZH86 or OP86 also holds in ZH57 or OP57 unless a row says
otherwise.

### Speed objects

| Object | Claim | Pages | Seen | Change from earlier reports |
| --- | --- | --- | --- | --- |
| `6081` | Mode table: "Target velocity (rpm)" / 目标速度（rpm）, 0 ~ 3000, UNSIGNED32, RW. Dictionary: "Profile velocity", U32, RW, no unit. EDS: "posmode speed", U32, RW, PDO 1, default 60 | EN86 p11, p17; ZH86 p11, p17; ZH57 p11; OP86/OP57 p9; EDS | Visual (EN86, ZH86, ZH57, OP86, OP57) | None |
| `60FF` | Mode table: "Target velocity (rpm)", 0 ~ 3000, UNSIGNED32, RW. Dictionary: "Target Velocity", I32, RW. EDS: "velocity speed", I32 (0x0004), RW, PDO 1, default 120 | EN86 p13, p17; ZH86 p13, p17; ZH57 p13; OP86 p11; EDS | Visual | None. The type conflict (Q03, C11) stands as recorded |
| `6099:01` | "Homing velocity (rpm)" / 回零速度（rpm）, 0 ~ 3000, UNSIGNED32, RW. Dictionary: "Homing Research speeds for switch", U32. EDS: "homing speed", U32, default 120 | EN86 p13, p17; ZH86 p13, p17; ZH57 p13; OP86 p11; EDS | Visual | Reopened visually (was text) |
| `6099:02` | Table name "Homing creep time (rpm)" / 回零爬行时间（rpm）, 0 ~ 3000, UNSIGNED32, RW. The method steps call it creep velocity / 回零爬行速度. Dictionary: "Homing Research speeds for zero", U32. EDS: "homing search speed", U32, default 60 | EN86 p13, p14, p15, p17; ZH86 p13 to p15, p17; ZH57 p13; OP86 p11 to p13; EDS | Visual (p13, p15, p17) | The earlier capability report listed it as rpm without the "time" wording. Already recorded in Q10 and `docs/homing.md`; read as speed in rpm |
| `606C` | Dictionary: "Velocity actual value", I32, RO. EDS: "actual speed", I32, RO, PDO 1, no default. No unit on any page | EN86 p16; ZH86 p17; EDS | Visual | No documentary change; see measured cruise readings in section 3 |
| `6080`, `607F`, `6085`, `606B`, `6062`, `6063`, `6069`, `606A`, `6086` | Absent from the EDS object list and from every manual page | EDS; all PDF text | EDS, text | None |

No page gives any speed in a unit other than whole rpm: no 0.1 rpm, rps, r/s, r/min, pulses/s, Hz or kHz, and no
decimal speed. The only Hz figure is the vibration rating "10~60 Hz" (EN86 p3 visual; ZH86 and ZH57 p3 text; product
page). Every decimal number in the extracted text is a section number or the drive size `150*97.5*52.6`.

### Ramp and other time objects

| Object | Claim | Pages | Seen | Change |
| --- | --- | --- | --- | --- |
| `6083` | Mode tables: "Acceleration time (ms)" / 加速时间（ms）, 0 ~ 2000, UNSIGNED32, RW, in both the position and velocity tables. Dictionary: "Profile acceleration", U32. EDS: "accel time", U32, PDO 1, default 100 | EN86 p11, p13, p17; ZH86 p11, p13, p17; OP86 p9, p11; EDS | Visual | None for the table. Figure: see section 2 |
| `6084` | Same as `6083` with "Deceleration time (ms)" / 减速时间（ms）; dictionary "Profile deceleration"; EDS "decel time", default 100 | Same | Visual | Same |
| `609A` | "Acceleration / deceleration time (ms)" / 加减速时间（ms）, 0 ~ 2000, UNSIGNED32, RW: one value for both directions. Dictionary: "Homing acceleration", U32. EDS: "homing accel/decel time", default 100 | EN86 p13, p17; ZH86 p13, p17; OP86 p11; EDS | Visual | None |
| `2507` | "Locking current time", Unit: ms / 锁机电流时间 单位ms, U16, RW. EDS `Obj.suo_time`. A current setting, not a motion ramp | EN86 p19; ZH86 p20; EDS | Visual | Newly listed |
| `250C` | "Positioning completion time" / 定位完成时间, U16, "factory default, adjustment not required generally". No unit | EN86 p19; ZH86 p20; EDS `Obj.PosinTime` | Visual | Newly listed |
| `2514`, `2517` | "Velocity node 1/2" / 速度节点1/2, U16, factory default. No unit. EDS `Obj.an_spd1/2`. Tuning breakpoints, not commands | EN86 p19, p20; ZH86 p20 | Visual | Newly listed. `2512`, `2513`, `2515`, `2516`, `2518` are velocity loop gains, also unitless, and EDS `251A`..`2520` are not in any manual |
| `1017` | EDS "Producer Heartbeat Time", U16, default 0, no unit; no manual page. Measured as milliseconds plus one (section 3) | EDS; ledger | EDS | None; measurement recomputed |
| `1800:05` and the other TPDO event timers | EDS "Event Timer", U16, default 100, no unit; no manual page. Measured as milliseconds plus one (2026-11-06 report, recomputed in section 3) | EDS | EDS | None |
| Control word spacing | "at an interval of about 10ms" between words 6, 7 and 15 | EN86 p12, p13, p15; ZH86 p12 to p14 | Visual (EN86) | None |

### Position and count objects

| Object | Claim | Pages | Seen | Change |
| --- | --- | --- | --- | --- |
| `607A` | "Target position", -1000000 ~ +1000000, INTEGER32, RW. The p11 figure labels the target position "number of pulses" / 脉冲数. Dictionary I3C (typography; EDS I32 0x0004, default 5000) | EN86 p11, p16; ZH86 p11, p17; OP86 p9; EDS | Visual | None |
| `607C` | "Homing offset" / 回零偏移, -1000000 ~ +1000000, INTEGER32, RW. No unit on any page. EDS "home offset", I32, default 0 | EN86 p13, p16; ZH86 p13, p17; EDS | Visual | **Correction:** the capability report listed it as command pulses. The unit is not stated; pulses is an inference from the shared range with `607A`. The ledger already says "units not established" |
| `607D:01/:02` | "Min/Max Position Limit", I32, RW, no unit, no range. EDS "Soft MIN/MAX Limit", I32, no default | EN86 p16, p17; ZH86 p17; EDS | Visual | Newly listed |
| `6064` | Table "Current position" / 当前位置, INTEGER32, RO, no range or unit. Dictionary "actual position", I32, RO. EDS default 0 | EN86 p11, p16; ZH86 p11, p17; EDS | Visual | No documentary change |
| `2201` | Position table "Subdivision" / 细分, 0 ~ 65535, UNSIGNED16, RW. Dictionary "Subdivision setting" / 细分设置, 400~51200, U16. EDS `Obj.MicDivide`, U16, default 0. No page says pulses per revolution or relates it to rpm | EN86 p11, p18; ZH86 p11, p18; OP86 p9; EDS | Visual | None. Q04/C12 stands |
| `2501` | "Encoder resolution" / 编码器分辨率, U16, RW, no unit, no range. EDS `Obj.Encoder`, no default. Both specimens read 4000 | EN86 p19; ZH86 p19; EDS | Visual | Reopened visually |
| `2500` | "Drive operation mode": 0 noneffective, 1 open loop, 2 closed loop. Specimens read 2 | EN86 p19; ZH86 p19 | Visual | None |
| Encoder | CL86-C "1000-line incremental", OL86-C none | EN86 p3; ZH86 p3; product page | Visual (EN86) | None. ZH57 p3: CL57-C also 1000-line |
| `2200` | "Default direction 0: Pulse Sign; 1: Pulse /Sign" / 默认方向, U16, RW. Nothing says what it does under CAN | EN86 p18; ZH86 p18 | Visual | None |

### Relation between rpm, `2201` and the encoder (question 3)

- **Documented:** nothing. No page states that rpm is motor-shaft rpm, how rpm converts to pulses, or that `2201` is
  pulses per revolution. The p11 position table lists `2201` among the position-mode parameters next to `607A`, and
  the p11 figure gives the target in pulses; that is the whole documentary link.
- **Inferred:** the drive has no gear, feed or factor objects (EDS), so its rpm can only refer to the motor shaft.
  "Subdivision" (细分) in Lichuan's pulse-drive usage is pulses per motor revolution; the measurements below agree at
  `2201=1000`. `2501=4000` with a 1000-line encoder suggests quadrature counts; nothing establishes it.
- **Measured** (`2201=1000` only): +250 pulses turned the shaft about a quarter turn (node 1, 2026-11-03, by eye);
  `6064` ends exactly at `607A` targets; `606C` reads near the commanded rpm; the `6064` rate is
  0.992 × floor(rpm × 1000/60) counts/s (section 3).
- **Unmeasured:** whether `6081`/`60FF` stay motor rpm when `2201` changes. `2201` was never written.

## 2. Ramp definition (question 2)

**Text:** no page defines the ramp in words or gives a formula. The tables say "Acceleration time (ms)"; the EDS names
say "accel time", "decel time" and "homing accel/decel time".

**Figure, EN86 p11, ZH86 p11, ZH57 p11 (visual, zoomed):** the position-mode motion curve plots velocity (rpm) against
time (ms). The "Acceleration time" / 加速时间 bracket runs from the start of the curve at zero velocity to the dashed
line where the curve reaches the plateau labelled "Target velocity" / 目标速度. The "Deceleration time" / 减速时间
bracket runs from the end of the plateau to zero velocity. The hatched area is labelled the target position (number of
pulses).

**Figure, OP86 p9 and OP57 p9 (visual):** the same curve, but it starts at a labelled 起始速度 (start velocity) above
zero and decelerates back to about that level; the acceleration bracket runs from the start velocity to the target
velocity. No start-velocity object exists on any page or in the EDS. Both pages carry the "CL57-C/OL57-C" running
header (C18).

So the documents depict, but do not state, that the acceleration time is the time from rest (or from the start
velocity, in OP86/OP57) to the commanded target speed, and the deceleration time the time from that speed back down.
The figure is in the position-mode section only; the velocity and homing sections have no figure. Nothing describes a
ramp between two nonzero speeds, zero ramp times or the curve shape beyond straight lines.

**Measured fits (inferred from drive reports, one firmware):**

| Trial | Setup | Observation | Time-to-target law | Fixed-reference law (time per 1000 or 3000 rpm) |
| --- | --- | --- | --- | --- |
| [713](../bench/20261105T1541Z-position200-quick-node2-713/rx.txt) node 2, mode 1 | 200 rpm, `6083=6084=2000` | 1.036 s after the trigger's acknowledgement: `6064=854`, `606C=101` | v² = 2ax gives 101.2 rpm at 854 counts for 100 rpm/s: fits | 500 or 1500 rpm/s would reach the 200 rpm cap before 854 counts; `606C` would read about 200: excluded |
| [709](../bench/20261105T1540Z-position300-halt-node1-709/rx.txt) node 1, mode 1 | 300 rpm, `6084=500`, HALT | 1392 counts from the HALT to standstill | 1250 counts plus about 28 ms at speed before the drive reacted: fits | 375 (per 1000 rpm) or 125 (per 3000 rpm) counts: excluded unless the reaction were 200 to 250 ms |
| Round 8 (1507, 1613), mode 3 | 30 and 60 rpm, `6084=200`, HALT | 60 and 121 counts after the due time | 50 and 99 counts plus about 20 ms: fits | About 2 and 6 counts (per 1000 rpm): excluded unless the reaction were about 115 ms |

The fits use the `6064` count, which is exact, and one `606C` sample, which has 2 rpm steps (section 3); they do not
measure the curve shape, the reaction time or whether the slope carries the 0.992 factor. One ramp pair per trial, two
specimens, one firmware. Q11 stays open for: a statement in words, velocity and homing modes, ramps between two
nonzero speeds, the start velocity of the OP86 figure and zero.

Inferred formula under this reading: a = (v_rpm / 60) / (t_ms / 1000) turns/s², or a = v_rpm × 1000 / t_ms rpm/s,
where v is the commanded speed of that move. A fixed ramp time therefore gives a higher acceleration at a higher speed.

## 3. The 0.992 rate (question 5)

### What was known

Rounds 8 and 9 measured the `6064` rate at 0.992 of the command at 6, 15, 30, 45 and 60 rpm in velocity mode
([round 8](2026-11-10_bench_r8.md), [round 9](2026-11-10_bench_r9.md)), round 10 the same in position mode at 30 and
60 rpm ([round 10](2026-11-10_bench_r10.md)). Round 8 named three candidate causes (drive speed, counts per turn, the
S2 clock) and noted a heartbeat period of 1.0008 s.

### Does the drive's clock run slow? Recomputed timer evidence

Method: from every retained `rx.txt` with a passive watch, take each heartbeat (`701`/`702`) and TPDO1 (`181`/`182`)
frame's receive upper bound (`hi`, S2 clock), count whole periods between successive frames, and divide the span by
the period count. Only steady sessions are listed.

| Session | Frames | Raw setting | Periods | Mean period (S2 clock) |
| --- | --- | --- | --- | --- |
| [2026-11-06 both-streams soak](../bench/20261106T1316Z-pdo-both-streams-soak/rx.txt) | Heartbeat node 1 / node 2 | `1017=1000` | 71 / 71 over 71.1 s | 1000.794 / 1000.794 ms |
| Same | TPDO1 node 1 / node 2 | `1800:05=100` | 649 / 649 over 65.5 s | 100.978 / 100.978 ms |
| [2026-11-06 event timer](../bench/20261106T1311Z-pdo-node2-event-timer/rx.txt) | Heartbeat node 1 / node 2 | `1017=1000` | 58 / 58 | 1000.795 / 1000.794 ms |
| Same | TPDO1 node 2 | `1800:05=100` and `200` | 102 and 101 gaps | 100.972 and 200.934 ms (report p50 200.955 ms) |
| [2026-11-06 node 1 stream](../bench/20261106T1314Z-pdo-node1-stream/rx.txt) | Heartbeat node 1 / node 2 | `1017=1000` | 27 / 27 | 1000.795 / 1000.794 ms |
| [2026-11-09 node 1](../bench/20261109T1245Z-r4a-hb-node1/rx.txt), [node 2](../bench/20261109T1246Z-r4a-hb-node2/rx.txt) | Heartbeat | `1017=1000` | 13 to 14 | 1000.793 to 1000.796 ms |

Every timer reads (raw + 1) ms on a time base within 0.02 % to 0.04 % of the S2's (1001 × 0.99979 = 1000.79;
101 × 0.99978 = 100.98), as the [2026-11-06 report](2026-11-06_pdo_stream.md) already found. A drive clock 0.8 % slow
would give heartbeats of about 1009 ms and TPDO periods of about 101.8 ms. The 6064 rates and these periods were both
measured on the same S2 clock, so the S2 crystal's own error cancels and cannot produce the 0.8 % either.

The cheap measurement the request names (100+ heartbeats against the host clock) is therefore already retained:
71 consecutive heartbeats per node in one session, and four sessions on two days. **Excluded:** a 0.8 % ratio between
the drive's communication timer and the S2 clock. **Not excluded:** a motion loop with its own time base, or with a
period that differs from its nominal value, which heartbeat timing cannot see.

### New: the rate over 6 to 300 rpm, all modes (night soak)

Method: unpack the 167 series of the [night soak](../bench/20261111-night/NOTE.txt) (2026-11-11, both nodes,
`position`, `move` and `velocity` routes). In each run's `rx.txt`, take the `6064:00` and `606C:00` upload results
(little-endian data, completion time on the S2 clock), pair each `6064` with the `606C` read that follows it within
50 ms, and keep consecutive pairs at least 0.3 s apart in which both `606C` readings are within 2 rpm of the command.
Ratio = |Δ`6064`| / (Δt × rpm × 1000/60). Each `6064` read sits at the same place in its cycle, so a fixed sampling
offset cancels; the 3 ms upload window bounds the per-interval timing error at 0.3 % at 1 s spacing and less at the
usual 4 s. The scripts were run from the session scratchpad and are not retained; this paragraph is the full method.

1,026 cruise intervals at 20 speeds. Factor from the 411 intervals at whole-pulse speeds of 60 rpm and above:
**0.99201** (standard deviation 0.00025 per interval).

| rpm | Nominal pulses/s at `2201=1000` | Measured ratio | floor(pulses/s) × 0.99201 / nominal | Intervals |
| --- | --- | --- | --- | --- |
| 6 | 100 | 0.9919 | 0.9920 | 98 |
| 10 | 166.67 | 0.9881 | 0.9880 | 113 |
| 12 | 200 | 0.9921 | 0.9920 | 38 |
| 15 | 250 | 0.9922 | 0.9920 | 20 |
| 18 | 300 | 0.9922 | 0.9920 | 16 |
| 20 | 333.33 | 0.9909 | 0.9910 | 88 |
| 24 | 400 | 0.9921 | 0.9920 | 12 |
| 30 | 500 | 0.9920 | 0.9920 | 76 |
| 35 | 583.33 | 0.9914 | 0.9914 | 14 |
| 40 | 666.67 | 0.9909 | 0.9910 | 22 |
| 45 | 750 | 0.9920 | 0.9920 | 104 |
| 48 | 800 | 0.9921 | 0.9920 | 6 |
| 50 | 833.33 | 0.9916 | 0.9916 | 8 |
| 60 | 1000 | 0.9920 | 0.9920 | 97 |
| 90 | 1500 | 0.9920 | 0.9920 | 56 |
| 120 | 2000 | 0.9920 | 0.9920 | 49 |
| 150 | 2500 | 0.9920 | 0.9920 | 57 |
| 180 | 3000 | 0.9920 | 0.9920 | 68 |
| 240 | 4000 | 0.9920 | 0.9920 | 52 |
| 300 | 5000 | 0.9920 | 0.9920 | 32 |

Observed (one firmware, `2201=1000`, two specimens, mode 1 and mode 3, velocity mode up to 60 rpm only):

- The ratio is constant at 0.9920 from 6 to 300 rpm wherever rpm × 1000/60 is a whole number (rpm a multiple of 3).
- At 10, 20, 35, 40 and 50 rpm, where it is not, the rate is lower by exactly the fraction that truncating to whole
  pulses per second removes (10 rpm: 166.67 to 166 pulses/s).
- The empirical model **`6064` rate = floor(rpm × 1000 / 60) × 0.99201 counts/s** matches all 20 speeds within
  0.0002. It is a fit, not a mechanism. Whether the truncation is to whole pulses/s at every `2201`, or uses `2201` at
  all, is not established because `2201` was never changed.
- Moves still end exactly at their `607A` targets, so the `6064` count has the command's scale; the shortfall is in
  rate, so cruise phases last about 0.8 % longer than nominal (inferred).
- `606C` at cruise read values in 2 rpm steps: 5 or 7 at 6 rpm, 9 or 11 at 10 rpm, 28 or 30 at 30 rpm, 58 or 60 at
  60 rpm, 296, 298 or 300 at 300 rpm. Its mean at 90 to 300 rpm is 0.990 to 0.993 of the command, consistent with the
  `6064` rate; at low speed it is biased (mean 5.3 at 6 rpm). It is a coarse estimate; use the `6064` slope to judge
  speed.

**Cause: not established.** Remaining candidates, none measured: (a) the trajectory generator's period or internal
scaling differs from nominal by 0.8 %; (b) the drive moves the shaft one turn per slightly more or fewer than `2201`
pulses, so the shaft speed differs from the `6064` rate; (c) something else in the firmware. The constant ratio across
speeds and modes rules out a fixed delay or a sampling artifact.

**Cheap measurements that would separate them:**

1. Shaft turns per pulse (no new code; one attended move or a camera): at `2201=1000` move +100,000 pulses and
   compare the shaft mark before and after. 100 turns exactly means (b) is false; candidate (b) at 0.8 % would leave
   the mark about 0.8 turn (290°) off. Nothing else needed.
2. An optical tachometer at 300 rpm in velocity mode: 297.6 against 300 rpm needs about 0.1 % accuracy.
3. Prompt 59 Part 3 repeats the rate at `2201` = 4000, 10000, 25600 and 51200: if the factor stays 0.992 and the
   truncation stays at whole pulses/s, the effect is in the rpm-to-pulse path; if it changes, it depends on `2201`.

## 4. Pulse and step-frequency limits (question 4)

- **No retained source states a pulse or step frequency** for any model: all five PDFs, the EDS and the product page
  were searched for kHz, Hz, 频率, 脉冲, pulse, 最高, r/min and rpm. Pulse wording appears only in the p3 introduction
  ("avoid the pulse loss problem of traditional drives" / 脉冲丢失), the p11 figure (target position in pulses) and the
  `2200` labels.
- **The two "operating manuals" are not pulse-input manuals.** OP86 (file "ol86c和cl86c操作手册", cover CL86-C/OL86-C)
  and OP57 are CANopen bus manuals with RS485/CAN, RS232, encoder and DI/DO ports and the same object tables; their
  running header reads CL57-C/OL57-C (C18). No manual for a Lichuan pulse-input drive is retained, so no pulse-input
  frequency (the usual 200 kHz or 500 kHz class) can be cited. If one is acquired, its limit applies to that model's
  pulse input, not to the CANopen drive's internal command generation.
- **The CL86-C has no pulse input.** Its ports are CAN (RJ45 x 2), RS232, encoder, motor, power and seven 24 V DI and
  three DO (EN86 p5 to p8, text; p3 visual).
- **Inferred arithmetic:** nominal command pulses per second = rpm × `2201` / 60. At `2201=1000`, 3000 rpm is 50 kHz;
  at `2201=51200`, 3000 rpm would be 2.56 MHz, and 200 kHz and 500 kHz would be reached at about 234 and 586 rpm.
  Prompt 59 Part 3 owns the measurement.
- The only documented speed-related failure is the position out-of-tolerance alarm (5 flashes per 5 s, `603F=FF05`),
  whose listed causes include "Too fast" / 速度过快 and which is resettable (EN86 p20 visual, ZH86 p20 visual).

## 5. Library state and industry practice (question 6)

**What exists (host-side factor group):** `Rational` exact numbers, `PositionUnit`, `VelocityUnit`,
`AccelerationUnit`, `UnitConfig` (command steps per motor turn, gear, lead, encoder scale, polarity) and exact
conversions in `include/MotorControlCAN/Units.h`; frames, `NativeRateBasis::MOTOR_TURNS`, `nativeVelocityTime` and
`prepareVelocityTarget` in `include/MotorControlCAN/Axis.h`. Speeds are quantized once with `Rounding`
(EXACT, NEAREST ties to even, TOWARD_ZERO, FLOOR, CEIL) against an exact allowance in rpm, and the effective integer
and exact error are kept in `PreparedVelocityTarget`. The Lichuan profile then requires integer motor turns per minute
in 1..3000 (`checkSpeed`, `src/profiles/lichuan_c/Position.cpp:248`; velocity and homing equivalents in
`Velocity.cpp` and `Homing.cpp:131`). I compiled a scratch program against `src/Axis.cpp`, `src/Units.cpp` and
`src/Endpoint.cpp` with a 5 mm lead: 1 mm/s gives 12 rpm, 1.5 mm/s 18 rpm, 1.3 mm/s is refused under EXACT
(detail 109, `FRACTIONAL_TARGET`) and gives 16 rpm with error +2/5 rpm under NEAREST, 0.1 mm/s gives 1 rpm with error
-1/5 rpm under NEAREST with allowance 1/2 and is refused with allowance 1/10 (detail 110), 0.03 mm/s gives 0 rpm
(error -9/25) and 260 mm/s gives 3120 rpm, both left to the profile's 1..3000 check, and with 10:1 gearing 0.1 mm/s
and 1.3 mm/s are exactly 12 and 156 rpm.

**What is native only:** ramps are integer milliseconds 1..2000 (`MoveProfile` in
`include/MotorControlCAN/profiles/lichuan_c/Position.h:198`, `VelocityProfile` in `Velocity.h:72`, the homing ramp);
`AccelerationUnit` and `convertAcceleration` exist but nothing converts an acceleration to a ramp time. Console:
`speed VALUE [rpm]` and the `move ... RPM configured` speed take exact motor rpm only; `accel`/`decel` take ms only;
`velocity VALUE RATE FRAME ...` already takes user rate units and a rounding policy
(`examples/common/ProbeConsoleMotion.cpp:344`).

**Industry practice (general knowledge, not vendor evidence):** CiA 402 drives that have the factor group (Nanotec N5
through `60A8`/`60A9` and `6097`; maxon EPOS4 with `6083` in rpm/s, `docs/reference/drive_contrasts.md` C10) take user
units and accelerations directly. PLC motion interfaces (PLCopen-style move blocks) take position, velocity,
acceleration and deceleration in user units. The Lichuan has no factor group, so the simplest functional equivalent is
host-side: user speed to integer rpm with a stated rounding, user acceleration to integer milliseconds per request
using the speed actually sent, and both effective values reported. That is prompt 60, gated on the ramp law that
prompt 59 measures.

## 6. Conflicts

| Conflict | Sources side by side | Status |
| --- | --- | --- |
| `60FF` type and range (Q03, C11) | p13 mode tables U32 0~3000 against p17 dictionaries and EDS I32 | Unchanged; prompt 59 |
| `2201` range (Q04, C12) | p11 0~65535 U16 against p18 400~51200 U16; EDS default 0 | Unchanged; prompt 59 Part 3 tests five points inside both |
| `6099:02` name (Q10) | p13 "creep time (rpm)" against p14/p15 "creep velocity", p17 "speeds", EDS "search speed" | Unchanged; read as rpm |
| Ramp start (Q11) | EN86/ZH86/ZH57 p11 figure from zero against OP86/OP57 p9 figure from a labelled start velocity with no object | Unchanged; now described in detail |
| Unit of `607C` | Range equal to `607A`, no unit on any page | Inference only (pulses) |
| `6083`/`6084` names | Mode tables "time (ms)"; p17 dictionaries "Profile acceleration/deceleration"; EDS "accel time"/"decel time" | The CiA402 names in the dictionary do not make them accelerations; units rest on the p11/p13 tables and the figure axes |

## 7. Changes against the 2026-11-12 capability report

| Section | Old claim | Evidence | New claim |
| --- | --- | --- | --- |
| a, Measured | Ramp meaning "No manual states it (Q11)" | EN86/ZH86/ZH57 p11 figure brackets (visual, zoomed); OP86/OP57 p9 | Depicted, not stated: the position-mode figure shows the acceleration time from rest to the target speed and the deceleration time from it to rest; OP86/OP57 start from a labelled start velocity. Still no text or formula, and no figure for velocity or homing |
| a, Measured | 713 and 709 "Both fit" time-to-target | v² = 2ax on 713; 709 stop distance; round 8 | They also exclude a fixed-reference law (time per 1000 or 3000 rpm), unless the drive's reaction to a HALT were 115 to 250 ms against the 20 to 30 ms the fits need |
| a, Not present | "the only pulse wording is `2200`'s labels" | EN86 p3, p11 figure | Pulse wording also in the p3 introduction and the p11 figure; none gives a frequency |
| a, Measured; c | `6064` rose 0.992 times the command at 6, 30 and 60 rpm; "an unexplained observation" | Night soak, 1,026 intervals, 20 speeds 6..300 rpm, three routes, both nodes | 0.99201 × floor(rpm × 1000/60) counts/s at every measured speed; extra truncation loss at rpm not divisible by 3; cause still unknown |
| c, Physically achievable | "Speed resolution is 1 motor rpm ... 16.7 pulses/s" | Same | The command resolution is 1 rpm; the delivered rate is also truncated to whole pulses/s (inferred model, `2201=1000`) and scaled by 0.992 |
| a (none) and round 8 | Candidate causes include "the S2 clock" and drive speed | Section 3 timer table | A 0.8 % ratio between the S2 and the drive's communication timer is excluded; a separate motion time base is not |
| c, native units | `606C` "signed rpm, lagging and averaging in ramps" | Night soak cruise readings | Also: 2 rpm steps at cruise; mean 0.990..0.993 of command at 90..300 rpm; biased at low speed |
| c, native units | `607A` target, `607C` home offset: "I32 command pulses" | EN86/ZH86 p13 (visual) | `607C` has the same range but no stated unit; pulses is inferred |
| a, Documented | `6099:01/:02` "0 ~ 3000 rpm, U32" (text) | EN86/ZH86/ZH57 p13 (visual) | Same, plus the p13 wording "creep time (rpm)" for `:02` (already in Q10) |
| Page status | EN86 p3, p16, p19, p20; ZH57 p13; OP86 p11; OP57 p11 by text | This session | Reopened visually; content unchanged |

Unchanged and confirmed: the 0..3000 rpm and 0..2000 ms ranges, U32 types in the tables, `60FF` I32 in the dictionary
and EDS, the `2201` range conflict, the absence of every factor-group and maximum-speed object, the absence of any
pulse frequency, and the FF05 "Too fast" cause.

## 8. Proposed edits to files I do not own

- **Ledger** (`docs/reference/lichuan_objects.json`, then regenerate): `0x6064:00` scaling or value policy: add the
  night-soak rate model and the timer finding with this report's path. `0x606C:00` units: "signed rpm as reported;
  cruise readings in 2 rpm steps at `2201=1000`". `0x6083:00`, `0x6084:00`, `0x609A:00`: add the p11 figure reading
  (EN86/ZH86/ZH57 p11, OP86/OP57 p9) and the 713/709/round 8 fits, keeping "not acceleration units". Gate Q11
  `next_evidence`: time to 95 % speed at several (rpm, ms) pairs in both modes. Gate Q04: the shaft-mark measurement
  of section 3.
- **`docs/device_conflicts.md` Q03** (not edited): its physical column could add trial 2001 (`60FF=3001` stored
  without a range check, [round 9](2026-11-10_bench_r9.md)).
- **`docs/velocity.md`, `docs/position_parameters.md`, `docs/units_status.md`, `docs/homing.md`:** a one-line link to
  [the speed units guide](../speed_units.md) where rpm and ramp units are introduced.
- **Prompt 59:** in its facts paragraph, "`6064` runs at 0.992 times the command" could become "at 0.992 × floor(rpm ×
  `2201`/60) counts/s (night soak, 6..300 rpm, cause unknown)", and Part 3 could record the truncation at each grid
  point. Add the shaft-mark measurement (section 3, item 1) to item 2's Part 1.
- **Prompt 60:** convert the acceleration with the effective integer rpm, report the effective acceleration exactly,
  and never compensate the 0.992 silently. RS names the native ramp fields `accelerationTime`/`decelerationTime`
  (ESS profile); the Lichuan profile uses `rampUpTimeMs`/`rampDownTimeMs`. Both are profile-native; prompt 60 should
  decide the common acceleration request name against RS.
- **`docs/reference/drive_contrasts.md` C10:** "no LC time-to-acceleration formula ... is inferred" could add that the
  p11 figure depicts time-to-target, with the measured fits, while no formula is stated.

## Checks

After writing: `python scripts/check_promptset.py` reported no errors (before, it reported five missing links to this
report and the guide); `python scripts/check_repository.py` passed; `git diff --check` reported nothing, and the two
new files have no trailing whitespace or CR characters. No build, test or firmware gate applies to these
documentation-only changes.
