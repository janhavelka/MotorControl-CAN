# Enable pull and the enable-fault state: sources and records, 2026-11-13

Read-only research for backlog S21 and the
[afternoon report](2026-11-13_velocity_limits_and_fault_recovery.md#afternoon-position-mode-the-51200-limit-and-the-enable-fault-state).
Labels: **D** documented (original page or EDS), **M** measured (retained drive reports), **I** inferred, **G**
generic CiA402/CiA301, not a Lichuan statement. One-based PDF pages rendered with PyMuPDF and viewed: EN86 p3, 8,
11, 12, 13, 15..20; ZH86 p8, 12, 15, 17..20; OP86 p5, 13 (CL57-C headers). ZH57, OP57, other OP86 pages, the EDS and
the product page: text. Records: `rx.txt` from the [packed series](../bench/20261113-ladder/packed/) named below.

## Summary

- No retained source mentions following error, command position, deviation or command clearing, or what an enable
  does to position (D, by absence).
- The records settle the "step back" as hypothesis (b), a counter switch, not motion. They contradict (a) as the
  cause of the enable fault: one faulting enable had no visible gap.
- The fault state fits (c): a persistent condition not visible in `6064`, with `606C` 121 to 256 rpm at word 7 and
  FF05 0.50 s later, 4 of 4 cases.

## 1. `6064` and `606C`

D: `6064` "Current position"/当前位置 (EN86/ZH86 p11), "actual position" (EN86 p16, ZH86 p17), I32 RO, no unit; EDS
"actual position", PDO, default 0. `606C` "Velocity actual value", I32 RO, same pages; EDS "actual speed". `607A` is
"number of pulses" (EN86 p11 figure). Nothing says encoder or command, or that the source changes with state.

M: released, `6064` follows the rotor (trial 5194: +1,986 counts after the first read following a 4000 rpm `000B`;
the coasting shaft after FF05). Nothing is commanded then, so it is encoder-derived (I). In the first cycle after
word 7 (42 ms after its acknowledgement) it shows another value in every decoded mode 3 enable:

| Trial (series) | Node, `2201` | Released | Word 7 (change) | `606C` | Then |
| --- | --- | --- | --- | --- | --- |
| 5194 (S0024) | 1, 1000 | 4,724,646 | 4,711,926 (−12,720) | 0 | Clean run |
| 5198 (S0025) | 1, 1000 | 5,416,578 | 5,400,559 (−16,019) | 0 | Run; FF05 at 4500 rpm |
| 5604 (S0113) | 1, 51200 | 144,426,915 | 143,920,381 (−506,534) | 0 | Clean run |
| 5684 (S0124) | 2, 1000 | 10,673,960 | 10,626,014 (−47,946) | 0 | Clean run |
| 5881 (S0160) | 2, 51200 | 97,454,515 | 97,221,925 (−232,590) | 0 | Run; FF05 at 3200 rpm |
| 5679 (S0123) | 1, 1000, fault state | 183,104,266 | 182,416,780 (−687,486) | 175 | FF05 |
| 5733 (S0136) | 1, 1000, after `2302` | 0 | 0 (0) | 175 | FF05 |
| 5835 (S0148) | 1, 51200, after `2302` | −40 | 0 (+40) | 256 | FF05 |
| 5901 (S0164) | 2, 1000, fault state | 100,100,622 | 99,767,821 (−332,801) | 121 | FF05 |

I: 12,720 counts in 42 ms at 1000 pulses per turn averages over 18,000 rpm, so the step is a counter switch. At motion
start the drive re-bases: 5684's first running read was the released value plus 37 at 15 rpm, where continuing from
the word 7 value needs 47,983 counts. The word 7 value is not the stop-instant position: in 5198 it lies about 11,500
counts below 5194's position extrapolated to its `000B`, while the whole coast was about 4,400. Which internal
counter word 7 shows is undocumented.

`606C` stayed 0 through the healthy jumps, so it is not the reported `6064`'s slope (I). In the fault state it read
121 to 256 rpm while released `6064` before and after differed by −2 (5679), −20 (5733), −8 (5901) and −7,257 at
51200 (5835): oscillation or a non-encoder quantity, unresolved.

## 2. Enable and position

D: only "Write 6, 7 and 15 into the control word in turn at an interval of about 10ms. After writing, the motor can
be enabled" (EN86/ZH86 p12), plus the low-bit table. OP86 p5 note 4 (CL57-C page): "驱动器上电后电机默认即为锁轴状态",
shaft-locked after power-up. Nothing on aligning, holding or returning to a command position.

M: power-up `6041=8037`, `2100` bit 4 clear, `6064=0`, holding torque ([drive actions](../drive_actions.md)). Mode 3
re-bases at motion start (above). The mode 1 cases there (trials 105, 308, 603) fit both a pull-back and a counter
switch; no shaft observation separates them.

## 3. Deviation clear and `2302`

D: DI codes 0..13 (EN86/ZH86 p8; EN86 p18; ZH86 p19): undefined, origin, limits, 4 "Motor enabling signal" (p8) or
"Motor MF signal" (p18/p19), stop, emergency stop, user 0..6. None clears a deviation, pulse count or alarm. No
object in `2000..2520` or the EDS is named for deviation or command clearing.

D: `2302`, EN86/ZH86 p15, OP86 p13: "When the value in Index 2302h-00 changes from 0 to 1, the current position value
will be cleared to zero, which should be manually set to 0"; p18 "Current position clearing"; EDS `Obj.ClearPos` U16
RW, PDO. M (5733): afterwards released and word 7 values both read 0, so it zeroes both counters (I). The enable
still faulted.

## 4. Loop selector `2500`

D: EN86/ZH86 p19 "Drive operation mode": 0 noneffective, 1 open loop, 2 closed loop, U16 RW; EDS PDO, no default.
`2505` (open-loop current ratio) shows open loop is a working mode. Nothing on when a change applies, in which state,
or persistence. EN86/ZH86 p20 list "Encoder disconnected" as an out-of-tolerance cause, so the check uses the encoder
(I); whether open loop checks tolerance, and whether a 2-1-2 toggle reinitializes the loop, is undocumented and
untested. The typed write refuses while alarmed and has never run ([tuning](../tuning.md)).

## 5. `250A` and other tuning words

D (EN86 p19, ZH86 p20): `250A` out-of-tolerance alarm threshold, `250B`/`250C` positioning completion threshold and
time, `250D` mean filtering coefficient, `2508`/`2509` low-pass enable and coefficient, `250E..2519` loop gains, all
"Factory default setting, adjustment not required generally", U16 RW, no unit or range; `2506`/`2507` locking current
% and ms; `251A..2520` EDS names only. M: `250A=4000`, `250B=20`, `250C=10`, `250D=512`, `2508=0`, `2509=5`. None is
documented for deviation filtering or enable behavior.

M: FF05 came 0.50 to 0.51 s after the word 7 acknowledgement in all four fault-state enables, for visible gaps of 0 to
687,486 counts. I: a time-qualified check or an internal error not shown in `6064`. No source gives 0.5 s.

## 6. Quick stop and fault reactions

D: "15->11 Emergency stop" (EN86 p12); ZH86 p12 adds 15->271 停止 and 15->128 报警复位. No deceleration, torque or
brake behavior. EDS: `605D` halt option code (I16, default 1, no meanings); no `605A`, `605B`, `605C`, `605E`, `6085`.
DO function 5 brake release (EN86 p8) has no timing. M: `000B` and FF05 both release (`2100` bit 4) and the rotor
coasts (user: shaft free after FF05).

## 7. Options

No source documents any of these for this state. Ranked by evidence.

| # | Option | Evidence | Bench test |
| --- | --- | --- | --- |
| P1 | Keep `rpm × 2201 / 60` below about 2.5 MHz | M: the state followed only FF05s at 51200 (2.7 to 3.0 MHz); after FF05s at 1000, 10000 and 25600 the next enables were clean | None; a library guard |
| P2 | On nonzero `606C` at word 7, write `6040=0` at once | M: 4 of 4 fault-state enables, 0 of 5 healthy. Limits motion and alarm; repairs nothing | Fault-state drive, 60 rpm enable, release within 100 ms; record `6064`, EMCY |
| P3 | Stop with HALT (`010F`, `6084`), not `000B` | M: HALT stays enabled, no coast. No link to the fault shown | Existing routes |
| R1 | Power cycle | D: p20 "Restart reset" names other classes. M: not in these records | Lead's run |
| R2 | `2500` 2, 1, 2, released and alarm-free | D: object only | After a controlword clear: write 1, read back `2500`, `2501`, `2100`, `6041`; write 2, read back; P2's 60 rpm enable. `606C=0` at word 7 and no FF05 at 0.5 s would show a reinitialization |
| R3 | Open-loop run, as diagnosis | D: mode exists | `2500=1`, 60 rpm enable, watch `606C`, `6064`, EMCY; restore 2 |
| R4 | Mode 1 enable | D: mode exists | `6060=1`, reviewed `607A`, enable, word 7 `606C` |
| R5 | `2300=1` factory restore | D: p18; rewrites every parameter, runtime effect unknown | Last; record parameters first |
| R6 | DI function 4 | D: label only; not wired | Wiring and a `2400` write |
| R7 | NMT Reset Communication | G: communication parameters only; Reset Node did nothing | Low value |

Tried and failed (M): `6040` bit 7, `2301`, `2302`, NMT Reset Node, `2201` to 51200 or 1000. The RS232 tool and the
product page's "Motor parameter auto-tuning function" have no documented CAN trigger and were not obtained.

Absent from the EDS (D): `6062`, `6063`, `6065..6068`, `60F4`, `60FC`, `605A..605C`, `605E`, `6085`: no following-error
window or timeout, demand or internal position, or stop and fault reaction options (G meanings) to read or set.

## Consequences for S21

- Drop "refuse an enable while `6064` and the last commanded position differ": the gap is routine and was zero in
  faulting enable 5733.
- Reword the inferred cause in the afternoon report, the backlog and [fault actions](../fault_reference_actions.md).
- The word 7 "pulled back" claim in [drive actions](../drive_actions.md) is not shown for mode 3, where the records
  show a counter switch.
