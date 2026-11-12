# On-the-fly speed, ramp and target changes: sources, gaps and a proposed path (2026-11-12)

Read-only investigation. No library file changed, no serial port was opened and no frame was sent. The question is
how to change speed, ramps and targets while a move or velocity run is in progress, and how to test it on the bench.
Page numbers are PDF pages, as in the ledger: EN86 PDF p12 is printed page 10. EN86, ZH86 and OP86 are
[86C_CANopen_user_manual_en.pdf](../reference/vendor/86C_CANopen_user_manual_en.pdf),
[86C_CANopen_user_manual_zh.pdf](../reference/vendor/86C_CANopen_user_manual_zh.pdf) and
[CL86C_OL86C_operating_manual_zh.pdf](../reference/vendor/CL86C_OL86C_operating_manual_zh.pdf) (running header
CL57-C/OL57-C, caveat C18). EDS is [LC_StepDriverCAN_V1.1.eds](../reference/vendor/LC_StepDriverCAN_V1.1.eds), cited
by line. EN86 p12/p13 and ZH86 p12 were checked on the rendered page images in `docs/reference/extracted/`; the rest
comes from the extracted text.

Status words used below: **documented** (a retained source says it), **inferred** (a reading of a source, not a
statement), **measured** (drive reports on the S2 bench, two specimens of one firmware identity: `1000=0x00040192`,
`1009=0x0C`, `100A=0x20`, `1018:01..03=0x1000/1/0`, `2000=0x0400`, `2001=0x0200`), **undocumented**.

## 1. What the library refuses today

| Place | Behaviour |
| --- | --- |
| [Position.h](../../include/MotorControlCAN/profiles/lichuan_c/Position.h) `MoveProfile::immediateUpdate` | Bit 5 (`003F`, `007F`) returns `UNSUPPORTED`, `MoveError::CAPABILITY`, before any work ([Position.cpp](../../src/profiles/lichuan_c/Position.cpp) lines 364-367) |
| Same header, `RelativeBase` comment | A move is admitted only from an idle drive and "never puts an edge into a running curve" |
| DEC-8 ([prompt 33](../prompts/33_position_moves.md), [audit](../prompts/PROMPTSET_AUDIT_34_50_2026-11-02.md)) | Bit 4 is held (`001F`/`005F`) until the move is terminal, then `000F` is written as its own step, because the mode-1 falling edge is unspecified |
| [Velocity.h](../../include/MotorControlCAN/profiles/lichuan_c/Velocity.h) `VelocityContext` | One `PreparedVelocityTarget rate`; `60FF`, `6083`, `6084` written once each; `stopDueUs` fixed at preparation |
| [cli_contract.md](../cli_contract.md), prompt 42 section | `speed`, `accel`, `decel` are host intent for the next move; `velocity` has no change route; `cancel` of a velocity run is refused |
| [Night soak](2026-11-12_night_soak.md) | "Not testable: changing speed on the fly"; series used successive moves at 6 to 300 rpm and runs at 6 to 60 rpm instead |

## 2. What the CL86-C sources document

| Item | Source | Content | Status |
| --- | --- | --- | --- |
| `6040` bit 4 | EN86/ZH86 p12, OP86 p10 | 1: new target position, rising edge | Documented |
| `6040` bit 5 | Same | 0: update motion parameters after the current position curve completes; 1: update immediately | Documented |
| `6040` bit 6 | Same | 0 absolute, 1 relative | Documented |
| Transition table | Same | `15->31`, `15->95`, `15->63` ("absolute positioning immediately according to the new motion parameters"), `15->127` (relative, immediately). Every arrow starts from 15 (`000F`); none starts from 31/95 or a running curve | Documented. That bit 5 = 0 implies a set-point can arrive during a curve is inferred |
| Bit 9 (change on set-point) | None of EN86, ZH86, OP86; the EDS has no bit definitions | Absent | Undocumented |
| Bit-4 falling edge | EN86/ZH86 p15, homing only: "1->0: Stopping homing" | Has a stop meaning in mode 6; mode 1 says nothing | Documented for mode 6; mode 1 undocumented |
| `6041` bit 12 | EN86 p12 only: "0: Target position to be effective, 1: Target position, effective". ZH86 p12 and OP86 p10 omit bit 12 (C05) | Candidate acceptance report | Documented (EN86 only); measured: set while moving (`5037`), latched after completion until the `000F` rearm (Q02, node 1 and node 2) |
| `6041` bits 10, 14, 15; `2100` bit 0 | EN86 p12 bit 10 zero speed; ZH86 p12/OP86 p10 bit 14 running; all bit 15 in place; EN86 p17 `2100` bit 0 in place | Completion candidates | Measured: 2100 bit 0 and bit 12 used by the bench record (Q02) |
| `607A:00` | EN86/ZH86 p11 I32 RW -1000000..+1000000 pulses; p16 prints "I3C"; EDS line 1894 I32 RW, PDOMapping=1, default 5000 | Target | Documented. Effect of a write during a curve: only through the bit 5 sentence |
| `6081:00` | p11 "Target velocity (rpm)" U32 RW 0..3000; p17 "Profile velocity"; EDS line 1943 "posmode speed" U32 RW, PDOMapping=1, default 60 | Position-profile speed | Documented. Bare write during a curve: undocumented |
| `6083:00`, `6084:00` | p11 and p13 (position and velocity tables) U32 RW 0..2000 ms; p17 "Profile acceleration/deceleration"; EDS lines 1952, 1961, PDOMapping=1, default 100 | Ramp times shared by modes 1 and 3 | Documented as milliseconds. Physical formula, zero, and change during motion: undocumented (Q11) |
| Start velocity | OP86 p9 curve starts and ends its ramps at 起始速度 (start velocity); EN86/ZH86 p11 curves start at zero; no object | Curve shape | Conflict (Q11 row), no object |
| `60FF:00` | p13 U32 0..3000 rpm; p17 and EDS line 2060 I32 RW, PDOMapping=1, default 120 | Velocity target | Conflict C11/Q03. Change while running: undocumented |
| Velocity start | EN86 p13: enable 6/7/15, then parameters, then "Bit4-Bit6 ... See Section 3.1.2" (no such section exists). ZH86 p13, OP86 p11: parameters, then 6/7/15, "the motor starts running" | Start recipe | Conflict C17/Q03; the library implements ZH86. Nothing about later `60FF` writes |
| Manufacturer ramp objects | EN86 pp17-20 and the EDS: no 2xxx ramp, jerk or start-speed object; no `607F`, `6085`, `6086`, `60F2`. `609A` is homing only | None | Undocumented |
| `605D` | EDS I16 RW, PDOMapping=0, default 1; no option meanings (Q12) | Halt option | No usable meaning |
| PDO path | EDS `1600` default `60400010,60FF0020,60600008`, `1601` `607A0020,60810020`; live maps measured 2026-11-06: RPDO1 `6040/6060/6098/607A`, RPDO2 empty ([report](2026-11-06_pdo_stream.md)) | All six objects are mappable | Mapping flags documented; no RPDO was ever produced by the host. An RPDO carries no confirmation |
| Set-point buffering, capacity, ignore-when-full, acknowledge timing | Nowhere | - | Undocumented |

Contradictions to keep side by side: C05 status layouts (EN86 versus ZH86/OP86); C11 `60FF` U32 versus I32; C17 the
two velocity start orders; the OP86 start-velocity curve versus EN86/ZH86; `6081` named "Target velocity" on p11
and "Profile velocity" on p17 (editorial); the EN86 p13 reference to a missing section 3.1.2.

## 3. What the reviewed ledger and gates already say

| Source | Content |
| --- | --- |
| [lichuan_review.md](../reference/lichuan_review.md), row "Position trigger/update; 33" | Bit 5 after-curve versus immediate; "Qualify trigger rearm, acceptance, queue/update behavior and fresh completion without replaying uncertain writes. Q11" |
| [lichuan_objects.json](../reference/lichuan_objects.json) `manual_only` `position_update_policy` | Claim "Controlword bit 5 distinguishes after-current-curve and immediate update", EN86/ZH86 p12, `documented_manual`, owner 33, gates Q01/Q04/Q11/Q02, `write_policy` `qualification_required` |
| Same, object rows `607A`, `6081`, `6083`, `6084`, `60FF` | `pdo_mapping_eds` "1", `pdo_mapping_reviewed` null, `edge_behavior` "Not specified by the retained source", `pending_qualification`. Their `coverage` flags all read false, including `implemented`, although prompts 33 and 35 write these objects; worth checking when the rows are next touched |
| [device_conflicts.md](../device_conflicts.md) Q11 | "Enable/release, target rearm, fresh move completion and immediate-update behavior gated. No command read-modify-write, invented FIFO or fixed device deadline" |
| Q02, Q03, Q12, Q17, Q20 | Q02 measured bits above; Q03 velocity sign and start; Q12 "Still unmeasured: stops from `005F`/`003F`/`007F`"; Q17 PDO maps; Q20: `6081`/`6083`/`6084` are not on the DEC-1 read list, `607A`/`60FF` only as one upload per enable |
| [LICHUAN_MOTION_FACTS.md](../audit/LICHUAN_MOTION_FACTS.md) | `15->63`/`15->127`: "Ordinary API must not imply arbitrary safe replanning or queue depth"; "Repeated writes and lost acknowledgements need normal uncertainty handling" |

Why immediate update was refused: prompt 33 says "Immediate update (`15->63`, `15->127`) implies a new edge during a
curve, by inference, so it has its own gate". A second set-point needs bit 4 to fall and rise again during the curve,
and DEC-8 avoids the falling edge because its mode-1 meaning is unspecified while mode 6 documents it as a stop. The
measured bit-12 latch means the drive only shows a new acceptance after a `000F`, which the library never writes
mid-curve. Packets 19 to 21 kept `003F`/`007F` out of the measured record. For velocity, prompt 35 admits one rate per
run, and trial 1009 showed that a report after an acknowledged word need not show its effect yet: in mode 1 the drive
reported operation enabled more than 28 ms and at most about 880 ms after word 15 ([velocity.md](../velocity.md)).

## 4. MotorControl-RS and the contrasts

MotorControl-RS HEAD `cd27b12`, clean working tree, read-only.

| RS symbol or file | What it does | Disposition for CAN |
| --- | --- | --- |
| `include/MotorControlRS/profiles/ess_rs/Position.h` `PositionInterruption::{IGNORE, INTERRUPT_CURRENT}` | Profile-specific serial bit 3 choice in `PositionCommand`/`MoveContext` | Adopt the pattern: profile-specific, not a common field. CAN already has `MoveProfile::immediateUpdate` |
| Same, `StoredPositionQualification` | Immutable copied witness of the original: target, generation, original operation ID, profile words, age cap | Adopt: a replacement needs a frozen witness of the original's confirmed trigger, never readback alone |
| Same, `PositionCommand` comment; `docs/ess_position.md` "Native stored-profile interruption" | Rejects `WRITE_ALL`/`VERIFY_AND_UPDATE` during motion ("active staging effects are unresolved"); only retriggers the exact stored profile; terminal `ACKNOWLEDGED`/`NOT_OBSERVED`; start-or-interrupt semantics; one replacement per original | Adopt the default rejection and start-or-interrupt framing. CAN can go further only after measuring that parameter writes do not change a running curve (Q22) |
| `examples/probe_cli/MoveRelation.h` `MoveRelation`, `ProbeApp.h` `submitPositionInterrupt`, `copyMoveResult`; `docs/cli_contract.md` stage 07 `position interrupt <original-id>` | Original/replacement/stop results kept separately; `observationSuperseded` means the old observer was cancelled locally, not that the drive was interrupted | Adopt relation, retention and the possible-motion interlock. Use a different verb: RS `interrupt` retriggers a stored profile, a CAN retarget writes a new target |
| `docs/console.md` "Speed/ramp choices apply on the next move"; `docs/archive/reports/2026-10-06_rounding_and_live_speed.md`; `docs/backlog.md` N05 and "Same-endpoint in-motion speed changes remain unavailable" | Live speed not established; `speed` rejects changes during a move | Adopt: `speed`/`accel`/`decel` stay next-move intent |
| `docs/axis_contract.md` | "S-curve, on-the-fly target changes and blending are optional capabilities, not implicit features" | Adopt: no field in common `MoveRequest`/`VelocityRequest` |
| `src/profiles/ess_rs/Registers.cpp` notes (ESS p25) | ESS normal stop uses the deceleration configured before motion | ESS-only evidence; reject transfer. It becomes a CL86-C question (Q26) |
| `ESS_RS::VelocityContext`, `prepareVelocity`, `serviceVelocity` | No rate change | No RS precedent for a velocity change: a CAN name is a new decision |

Contrasts, as design evidence only:

| Drive | Retained evidence | Design consequence |
| --- | --- | --- |
| Nanotec N5 ([manual](../reference/multidrive/vendor/nanotec_n5_canopen_v3.3.0.pdf) v3.3.0/FIR-v2213, PDF pp68-70) | Bit 4 new set-point; bit 5 "Change Set-Point Immediately" bypasses a buffer; bit 9 table (pass through the first target); bit 12 set-point acknowledge in sync with bit 4; one buffered set-point, newest ignored when full; `60F2` bits 4/5 can reset bit 4; bit 9 ignored if the ramp speed is not met | Buffer depth, acknowledge handshake and blending are profile facts. Do not read the CL86-C bit 5 sentence as N5 semantics |
| N5 profile velocity (PDF pp77-78) | `60FF` target speed; `6083`/`6084`/`6085`/`6086` ramps; bit 10 target speed reached within `606D`/`606E`; no trigger bit described | A velocity change is a profile fact even where no trigger exists; completion needs a window rule |
| maxon EPOS4 ([drive_contrasts.md](../reference/drive_contrasts.md) rows, EP rel13740 pp23-24, 220, 222, 230, 236) | PP new set-point edge, separate bit 12, bit 10 target reached, `6067` default disabling the window; ramps are rpm/s | Ramp change is acceleration there and time on CL86-C. EPOS4's immediate/buffer behaviour is not recorded here and its PDF is remote-only: no claim |

## 5. Proposed design

Principles, all from the existing contracts:

1. A change is its own explicit, operation-specific step. Host intent (`speed`, `accel`, `decel`) never reaches a
   running move.
2. Each change type is gated `CLOSED` by default, opened by a `MEASURED` record for the exact firmware tuple or by an
   attended experiment scope with project caps, as `VelocityGate` already does.
3. Absolute targets only, same direction first. Relative (`007F`), wrapped angle and reversal stay refused.
4. A change never extends `deadlineUs` or `stopDueUs` and never consumes the reserved stop capacity. A pending stop
   refuses every change.
5. Any unconfirmed change write leaves the motion uncertain and is never replayed. Acceptance and the new state need
   fresh post-change observations inside a window, not the first cycle (trial 1009).
6. SDO first. RPDO delivery has no confirmation and waits for Q17.

### Velocity rate change (prompt 35)

| Part | Proposal |
| --- | --- |
| Conversion | Existing pure `prepareVelocityTarget` for the new rate, integer motor rpm |
| Admission | New `prepareVelocityChange(VelocityContext&, const PreparedVelocityTarget&, ...)` admits one pending change into the running context; no new `next`/`advance` names: `nextVelocity` yields the download between observation cycles, `advanceVelocity` consumes it, `serviceVelocity` is unchanged |
| Refusals | Run not ACTIVE or before its final-enable evidence; stop pending or due; change in flight; sign change or zero; outside the gate's range (experiment: at most `VELOCITY_EXPERIMENT_MAXIMUM_RPM`); budget not before `stopDueUs` minus the stop reserve; history full; epochs or identity not current |
| Work | One `60FF` download. First prototype keeps the run's frozen ramps; a later variant writes `6083`/`6084` before `60FF`, each once |
| Retained evidence | Fixed `changes[N]` (for example N = 4) with request, quantization, `ActionExecution`, SDO terminal, first cycle at which the drive's report matched under the record's rule, and `uncertain` |
| Failure | Any non-acknowledged change ends the run `FAILED` with that first failure and requests the run's own stop at once, as the trial 1009 fix does. A project decision until Q25 says what an abort leaves applied |
| Completion meaning | "Drive report showed the new rate" only under a measured tolerance and window rule; otherwise `NOT_OBSERVED`. `606C` unit is measured as signed rpm on one firmware only (Q04) |

### Position retarget and same-endpoint speed change (prompt 33)

A replacement is a new `MoveContext` prepared through `prepareMoveAbsolute` with `MoveProfile::immediateUpdate = true`
and a new replacement input (original operation ID plus a copy of the original's frozen evidence, trigger terminal and
acceptance sample) that replaces idle admission. It takes over the original's reservation inside `AxisSession`.

| Phase | Work | Gate |
| --- | --- | --- |
| PARAMETERS | `607A`, `6081`, `6083`, `6084` as planned; same-endpoint speed change writes the original `607A` value again | Q22: bare writes must not change the running curve |
| HANDSHAKE (new) | `000F` during the curve, then observe acceptance (bit 12) absent | Q22: falling edge must not stop the curve |
| TRIGGER | `003F` | Q23 |
| WAITING/OBSERVING | New acceptance, then completion after absence, under the existing `CompletionRule` | Q23: acceptance and completion attribution |
| REARM | `000F` after observed completion | As today |

The original ends locally with the RS-style relation (`observationSuperseded`); at most one replacement per original;
the stop stays reserved. If the original completes before `003F`, the replacement is an ordinary absolute move from
idle, which is acceptable only because the target is absolute; the relation records it. Buffered set-points
(`001F` mid-curve, bit 5 = 0) stay refused until Q24.

### Ramp change rules

| Change | Rule |
| --- | --- |
| `6083`/`6084` in a position replacement | Only as part of the replacement's set-point; milliseconds, not acceleration |
| `6083`/`6084` in a velocity change | Later variant, written before `60FF`; partial setup is uncertain |
| Bare ramp writes during motion | Never emitted by the library (Q22) |
| Deceleration of a stop after a change | Which `6084` applies is unknown (Q26); stop documentation must say so |
| Zero ramp | Refused (Q11) |
| Physical meaning | Unknown: time per change, time from zero to target, or a fixed slope (Q26) |

### Console grammar (prompt 42) and runners (prompt 44)

| Grammar | Path | Notes |
| --- | --- | --- |
| `velocity change ID VALUE RATE FRAME configured [round POLICY ERROR [approx ERROR]]` | `ProbeMotion::requestVelocityChange`, `AxisSession`, `prepareVelocityChange` | Reply `accepted` with `change_index`; `result ID` of the run lists `changes:[{index,requested,native_rpm,error,execution,observed_cycle,uncertain}]` |
| `move retarget ID absolute VALUE UNIT FRAME RPM configured [setup write\|verify] [round POLICY ERROR [approx ERROR]]` | Replacement move | New `operation_id` and `relation:{original,replacement,observation_superseded}` |
| `move speed ID RPM` | Replacement with the original endpoint | Same relation fields |

All three are caps-only gated with the Q reason until their gates open, and refuse before traffic on an unknown,
terminal or wrong-kind ID, a pending stop, a change in flight, a relative or angle original, a sign change or an
exhausted budget. The names need a decision recorded in [CONVENTIONS.md](../prompts/CONVENTIONS.md) and the API/CLI
map before implementation. Python scenarios (prompt 44): `velocity-step`, `position-speed-change`,
`position-retarget`, dry-run first, never replaying an uncertain change.

## 6. Proposed evidence gates

Q01 to Q21 are in use; Q22 to Q26 are free today.

| Q | Question | Blocks |
| --- | --- | --- |
| Q22 | Mode 1 during a curve: does a bare write to `607A`/`6081`/`6083`/`6084` change it; does `001F` to `000F` stop it, and does bit 12 clear | Any replacement; relaxing DEC-8 for replacements only |
| Q23 | `000F` to `003F` mid-curve: same-endpoint speed change, new endpoint ahead, acceptance handshake, completion attribution, transition shape | Replacement `MEASURED` gate |
| Q24 | `000F` to `001F` mid-curve (bit 5 = 0): buffered or ignored, depth, behaviour when full | Buffered set-points |
| Q25 | Mode 3 running: does a `60FF` SDO write change the rate with `000F` held, latency, ramp used; abort effect | Velocity change `MEASURED` gate |
| Q26 | Ramp meaning under change: `6083`/`6084` as time per change, from zero, or slope; which `6084` a later HALT uses | Ramp variants; stop wording |

Stops from `003F` and from a changed velocity run are already open rows in Q12.

## 7. Bench measurements and traces

Each trial runs through a prompt 29 conditional-experiment bench route, not the production API. Capture for every
trial: identity tuple, node, `2201` (node 1 measured 1000 pulses per turn), mode, attendant cutoff; the raw CAN trace
of every frame with S2 timestamps (`rx.txt`/`rx.bin` as in `docs/bench/`), including SDO requests and responses,
heartbeat and EMCY; SDO cycles of `6041`, `2100`, `6061`, `6064`, `606C` at a fixed interval (the bench sustained
20 ms on node 1 for 60 s, Q18); optionally a TPDO1 copy (`6041`/`6061`/`6064`) at event timer raw 10 (periods
consistent with about 11 ms on this firmware, Q17), which stays an unqualified diagnostic. Do not read
`6081`/`6083`/`6084` back without a Q20 one-use grant; the download acknowledgements are the write evidence. Results
are drive reports only. A failed or unknown write ends the trial with no replay.

Example frames (node 1, request COB `0x601`, response `0x581`, response `60 ii ii 00 00 00 00 00`):
`6081=120` `23 81 60 00 78 00 00 00`; `60FF=60` `23 FF 60 00 3C 00 00 00`; `6040=000F` `2B 40 60 00 0F 00 00 00`;
`001F` `2B 40 60 00 1F 00 00 00`; `003F` `2B 40 60 00 3F 00 00 00`; `607A=15000` `23 7A 60 00 98 3A 00 00`;
`6083=1000` `23 83 60 00 E8 03 00 00`. Confirm each against the library codec output before use.

| Trial | Gate | Setup and action | Outcomes that discriminate |
| --- | --- | --- | --- |
| T0 | Baseline | Absolute 0 to 10000 at 60 rpm, ramps 500 ms; existing night-soak series can serve | Reference slope and completion timing |
| T1 | Q22 | Same move; at about 3000 write `6081=120` only | Slope unchanged (bare writes inert) or doubled (writes live) |
| T2 | Q22 | Same move; at about 3000 write `000F` only | Continues to 10000 with bit 12 clear (CiA 402-like handshake); decelerates and stops (falling edge is a stop); bit 12 stays set (no handshake) |
| T3 | Q23 | After T1 inert and T2 continue: `6081=120`, `000F`, wait for bit 12 absent, `003F` | New slope and transition shape (blended, or stop and restart); bit 12 again; `2100` bit 0 and final 10000 |
| T4 | Q23 | After T3: `607A=15000` at about 4000 (extend); separately `607A=6000` at about 3000 (shorten ahead) | Endpoint reached, maximum `6064`, overshoot, completion attribution |
| T5 | Q24 | After T2: `607A=15000`, `000F`, `001F` at about 3000 | Finishes 10000 then 15000 (buffer); goes at once (bit 5 ignored); ignored |
| T6 | Q25 | Velocity run 30 rpm, ramps 500 ms, 12 s, HALT; `60FF=60` at about 4 s, `60FF=30` at about 8 s | Rate follows within a window and with what latency; or ignored with `000F` held. Never try a bit-4 edge in the same trial |
| T7 | Q26 | `6083=1000`; separate runs changing 30 to 45 and 30 to 60 | Equal change times: time per change. About 333 ms and 500 ms: time from zero to target. Near-instant: slope to 3000 rpm |
| T8 | Q12 | HALT `010F` and QUICK_STOP `000B` after T3's `003F` and after T6's change | Stop behaviour from the changed state; which `6084` governs (Q26) |

Reversal, relative `007F`, zero and negative `60FF` are not on this list. They wait for Q03 reverse and the T4 results.

## 8. Proposed backlog, in priority order

The two-digit IDs keep them apart from the W1 to W9 workstreams in PROGRESS and the backlog; rename at adoption if
that is still ambiguous.

| ID | Item | Owner prompt | Prerequisites | Smallest next step or measurement |
| --- | --- | --- | --- | --- |
| F01 | Record Q22 to Q26 in `device_conflicts.md` and matching ledger semantic claims; link them from the refusal comments | 29, with 33 and 35 | None | Documentation change only |
| F02 | Measure a mid-run velocity rate change | 35, through a 29 route | F01; velocity experiment route as used in the night soak | T6 with one change, 30 to 60 rpm, node 1 |
| F03 | Host velocity change step in `VelocityContext`, `CLOSED` by default, experiment cap | 35 | None for host work; F02 for `MEASURED` | Fake-owner tests: change during pending stop, budget at due, unknown download, sign change |
| F04 | Console `velocity change`, caps entry, JSONL fields, parser | 42 (parser 43) | F03 | Native console tests of grammar and refusals |
| F05 | Measure bare writes and the falling edge during a mode-1 curve | 33, through a 29 route | F01 | T1, then T2 |
| F06 | Measure an immediate same-endpoint speed change | 33, through a 29 route | F05 shows inert writes and a continuing curve | T3 |
| F07 | Measure stops after `003F` and after a velocity change | 31 | F06, F02 | T8 HALT from `003F` |
| F08 | Host replacement lifecycle (absolute, same direction) with relation and interlock, `CLOSED` by default | 33 | F05 to fix the phase order; F06 and F07 for `MEASURED` | Name decision in CONVENTIONS and API/CLI map; fake-owner tests including completion before `003F` |
| F09 | Console `move retarget` and `move speed` | 42 | F08 | Native console tests |
| F10 | Measure ramp meaning under change | 35, 33 | F02 | T7 |
| F11 | Measure new-endpoint retargets ahead of the position | 33 | F06, F07 | T4 extend, then shorten |
| F12 | Python scenarios `velocity-step`, `position-speed-change`, `position-retarget` | 44 | F04 (F09 for position) | Dry-run parser and verdict tests |
| F13 | Finite qualification of the change routes | 47 | Selected subset of F02 to F12 | Short regression plus focused change series |
| F14 | Measure buffered set-points (bit 5 = 0) | 33 | F05 | T5 |
| F15 | Keep refused: relative `007F` mid-curve, reversal, `60FF` sign change or zero during a run | 33, 35 | Q03 reverse, F11 | None until those close |
| F16 | RPDO delivery of changes | 18, 38 | Q17 RPDO producer and retirement evidence | Design only; no confirmation exists on that path |

## 9. Not established by this report

No source states the CL86-C set-point buffer, its capacity, a bit-9 function, the mode-1 falling-edge meaning,
whether bare parameter writes affect a running curve, whether `60FF` changes apply while running, the physical ramp
formula or which deceleration a stop uses after a change. Nothing here was measured; the proposed trials define how
to measure it on the two connected specimens, and a result there would qualify only that firmware tuple.
