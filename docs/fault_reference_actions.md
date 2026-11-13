# Fault clearing and device counter zero

`prepareClearAlarm` and `prepareSetDevicePosition` use the existing concrete
Lichuan `ActionContext`, `nextAction` and `advanceAction`. Applications provide
the same continuous owner reservation, current binding, time and finite budgets.
These operations are explicit requests, never startup, recovery or retry policy.
The core performs no I/O and owns copies of the evidence it consumes.

Evidence classes below: **source** means the original vendor page or EDS says it;
**inference** is our reading of a source; **policy** is a project decision;
**host** is fake-owner test evidence; **device-reported** is a drive's own report on a
named specimen, not an independent physical observation. The counter zero ran once, on node 2 on 2026-11-04 (below).
The only alarm so far, `FF05` on node 1 in mode 3 on 2026-11-13, was first refused before any write by the
mode 1 admission of that time, which led to the
[mode 3 admission](fault_reference_actions.md#mode-3-admission-project-policy-2026-11-13) (packets 17 and 18). The
`2301` method then ran once with writes, on that alarm (trial 5323), and its report led to the
[completion rule](fault_reference_actions.md#completion-measured-2026-11-13). Later that day both
methods ran on `FF05` alarms of both specimens, and the
[controlword method](fault_reference_actions.md#two-reset-methods), the only one that left the drive
released, became the default.

## Supported transition tables

Mode and monitor restrictions are conservative library admission policy, not a
claimed manufacturer state machine. All prerequisites are fresh checked SDO
observations acquired under the actual live reservation. Selecting CL86-C or
OL86-C is an explicit model binding, not firmware detection. No row sends an
enable, a mode, a target, a save, a restore or an NMT command, and no controlword
but the controlword method's `6040` `0x0080` and `0x0000`.

### Alarm clear (packet 17)

| Case | Fault provenance | Mode and state | Native route and values | Rearm and ACK | Required new observation | Raw error and history | Final uncertainty |
| --- | --- | --- | --- | --- | --- | --- | --- |
| Admitted attempt | One read under the reservation: `2100` alarm set and raw `603F=FF05`, with `1001` and the `1003` count and entries. `incident.codeLink=UNRESOLVED`: the code does not identify the alarm (D-26) | `6061=1` with `2100` running clear, or `6061=3` with `2100` released and running clear or latched beside the release with `606C` raw `0` in the same read's FEEDBACK (policy, [below](fault_reference_actions.md#mode-3-admission-project-policy-2026-11-13)). `6041` kept raw at admission. Both methods admit alike | `CONTROLWORD_BIT7` (default since 2026-11-13): `6040:00` U16 `0x0080`, then `0x0000` (source: EN86/ZH86 p15, OP86 p13; `15->128` in ZH86 p12 and OP86 p10). `OBJECT_2301`: `2301:00` U16 `1`, then `0` (source: EN86 p18 and EDS; ZH86 p18 and OP86 omit `2301`). [Methods](fault_reference_actions.md#two-reset-methods) | The second value returns to the documented ineffective value: for `2301` as policy, since no source requires an edge, rearm or pulse and no arming `0` precedes the `1` (D-27); for `6040` as p15's manual return. Each write needs its own matching ACK before the next; the first write's ACK is `execution=ACKNOWLEDGED` | After the second ACK, MONITORS and STATUS in the admission's mode until the [completion rule](fault_reference_actions.md#completion-measured-2026-11-13) holds: mode 1 with `2100` running and alarm clear; mode 3 with `6041` low bits `000`, `2100` alarm and running clear and `6041` bits 3 and 7 clear, the release either way. Then one ERRORS read | `incident` keeps the admission values. The final ERRORS read is kept raw, even when `603F` still reads `FF05`. `1003` is never read again | `OBSERVED` means a later report without the alarm, not that the cause is gone. `valueOneMayRemain` if the second write lacks a positive ACK |
| Alarm persists after both ACKs | As admitted | Each cycle needs the admission's mode and that mode's rule, else `OBSERVATION_SCOPE`: in mode 3 the alarm still reported needs the release and running only after a latched admission, and `6041` bits 3 or 7 still set with `2100` alarm clear keep the attempt polling | No further write | No write repeats | Repolls MONITORS and STATUS only, at `observationIntervalUs`, until the deadline | Unchanged | `DEADLINE`, `execution=ACKNOWLEDGED`, `uncertain` |
| Withdrawal during the attempt | A new EMCY, boot-up, lifecycle change or cancel, delivered by the application | Any | The admitted write or read settles; nothing new is yielded | A terminal that still correlates joins `commandHistory` or the observation evidence. After a node-epoch change it no longer does, and the application ends the attempt locally with `INVALIDATED` | None | Unchanged; the owner's newest EMCY is separate | Outcome is the withdrawal reason; `valueOneMayRemain` follows the same rule |
| Rejected before work | Any other code, including `FF01` overcurrent, `FF02..FF04` (never mapped from LED counts) and `0`; no alarm; missing ERRORS or ERROR_HISTORY; stale or foreign read | Mode other than 1 or 3; running in mode 1; mode 3 not released; mode 3 running without a FEEDBACK block or with `606C` nonzero | None | None | None | None | Output unchanged |
| Method mismatch | A terminal of the other method's object, or a context whose `clearMethod` is out of range or set on another action | Any | None from it | Refused as `CORRELATION`, or the context refused by every call | None | Unchanged | `stopPredecessorAllowed` stays false for `0x0080` and `0x0000` |

### Device counter zero (packet 18)

| Case | Admission evidence | Mode and state | Native route and values | ACK and withdrawal | Required new observation | Final uncertainty |
| --- | --- | --- | --- | --- | --- | --- |
| Admitted zero | Requested native position exactly `0`; one read under the reservation of MONITORS, STATUS and FEEDBACK; matching axis with room in both epochs | `6061=1`, `2100` running and alarm clear, `606C` raw `0` (policy). `6064` kept raw in `baseline` | `2302:00` U16 `0`, `1`, `0` (source: EN86/ZH86 p15 describe the `0 -> 1` change and the manual return; EDS U16 RW) | Each write needs its ACK before the next. At the first possible transmission of the `1`, the axis-aware notice withdraws origins, soft-limit qualification and the reference witness, and advances the configuration and reference epochs, exactly once. Configured limit values and units stay | After the last ACK and owner epoch synchronization, MONITORS, STATUS and FEEDBACK on the new epoch until raw `6064` reads `0`, with mode 1 and running and alarm clear | `OBSERVED` is a raw zero, not a home, an origin or a counter mapping. An already-zero `baseline` shows no change at all |
| Nonzero request | Any | Any | None: `UNSUPPORTED` right after the overlap check, before any state, axis or budget check | None | None | Output unchanged |
| Rejected before possible effect | Alarm, running, other mode, nonzero `606C`, missing FEEDBACK, stale or exhausted epochs; or the trigger refused or cancelled before its notice | Any | None or a prefix of `0` | `coordinatesInvalidated=false`, epochs and validity as they were | None | Distinguishable from a withdrawal: nothing moved |
| Trigger possibly sent, then failure | Admitted | Any | `0`, possibly `1` | Withdrawal stays; no later result restores it. A definite later no-TX result refines `execution` only | None for this action | `valueOneMayRemain` if the final `0` lacks a positive ACK; an uncertain write leaves the route unresolved |

## Fault identity and the FF05 admission

EN86 p16 names `603F` codes `FF01` overcurrent and `FF05` "too high position
deviation" (source). EN86 p20 lists LED flash classes, with "Position
out-of-tolerance alarm" marked resettable (source). The link between the two is
by name only: p20 gives no code and no reset route, so the link is an inference
(D-26). Matching five flashes to `FF05`, or deriving `FF02..FF04` from flash
counts, is not done.

`603F` latching is undocumented, and EDS says RO where EN86 says RW (C07). A
`603F` that still reads `FF05` from an earlier event, beside a new alarm with
another cause, admits the attempt in exactly the same way. The library cannot
tell these apart, so admission records `incident.codeLink=UNRESOLVED` and claims
nothing about which event, or which reset class, the reported alarm belongs to.
The `FF05` requirement only narrows the attempt. An application that saw `603F`
already reading `FF05` while the alarm was clear (for instance a previous
attempt's final ERRORS read) has direct evidence that the code is not fresh.

## Mode 3 admission (project policy, 2026-11-13)

No source ties the clear to a mode: EN86/ZH86 p20 mark the position
out-of-tolerance class resettable and name none. The mode 1 restriction was
library policy from where the clear was designed; no specimen had an alarm then,
and none has run a clear since. Mode 3 is now admitted too, as policy, for the
following reason.

**Finding** (device-reported, node 1, firmware `1000=00040192`, `1009=000c`,
`100A=0020`, 2026-11-13, [ladder records](bench/20261113-ladder/)). A mode 3 run
at 4500 rpm with 2000 ms ramps (trial 5198, the range probe) raised EMCY
`05 FF 81 ...`: `603F=FF05`, `1001=81`. The drive then read `2100=001D` (in
place, running, alarm, released), `6041=C018`, `6061=3`, `606C=0` (trials 5199
to 5202). Both clear attempts (trials 5203 and 5210) were refused before any
write with "fresh mode 1 not-running SDO evidence required", and the `mode1`
route refused the mode change while `1001` was set (trial 5208, `why=error-register`).
So the alarm had no clear route but a power cycle.

**Rule.** Mode 3 is admitted with `2100` alarm set, raw `603F=FF05`, ERRORS and
ERROR_HISTORY as in mode 1, and `2100` released set. `2100` running may be
clear, or set beside the release as the latch a moving stop leaves (`0014`/`0015`
in trials 1513 and 1607) and the `FF05` run left (`001D`): then the same read
must include a fresh FEEDBACK block with `606C` raw `0`, and a read without it is
refused. Mode 1 is unchanged: running set there is refused, latched or not.

**Why released.** The drive keeps its `60FF` target (4500 after the finding), and
no source says what a drive does after a clear. A de-energized drive is the only
one admitted, and every later cycle that still reports the alarm must still report
released. The action sends no enable or `6060`, the `2301` method no `6040` at
all, and nothing resumes the run.

**Completion** is [below](fault_reference_actions.md#completion-measured-2026-11-13).
Every cycle must repeat the admission's mode (`incident.modeRaw`): a mode 3 attempt
is not completed by a report in mode 1, nor a mode 1 attempt by one in mode 3.
Context checks refuse an incident that admission could not have produced.

Not established: whether a later enable resumes the retained target, and what the
drive's report after the clear means for torque (below).

## Two reset methods

**Measured on 2026-11-13** ([bench report](reports/2026-11-13_velocity_limits_and_fault_recovery.md), FF05 raised by
velocity runs at the speed or acceleration limit, two specimens, one firmware): `CONTROLWORD_BIT7` cleared the alarm
and left the drive released (`6041=8050`, `2100=0011`) 11 of 11 times. `OBJECT_2301` cleared it 8 of 9 times but left
`6041=8010`, `2100=0001`, which only a following `6040` = 0x0080 word released (8 of 8; 0x0000, 0x000B and 0x0006 did
nothing, and an NMT Reset Node changed nothing); once, at `2201=51200`, the FF05 came back right after the `2301`
clear.

**Default (project decision, 2026-11-13).** On the user's decision "Alarm clear - pick the best method" and this
evidence, `CONTROLWORD_BIT7` is the default of `prepareClearAlarm` and `AxisSession::requestClearAlarm`, and so of
the console `alarm-clear` and `ProbeMotion::requestAlarmClear`, which name no method. It was `OBJECT_2301` before;
`OBJECT_2301` stays selectable, and a caller that needs the `2301` writes names it. The scope of the evidence is
narrow: one firmware (`1000=00040192`, `1009=000C`, `100A=0020`), two specimens, `FF05` only, every clear admitted
in mode 3 from a released drive. Mode 1 clears, other alarms, other firmwares and OL86-C are unmeasured for either
method. `OBJECT_2301` on its own can complete in `6041=8010`, `2100=0001`, a report no S2 bench route admits, from which
only a later `6040` = 0x0080 released the drive; the `2301` method never writes that word.

`prepareClearAlarm(..., AlarmClearMethod method = CONTROLWORD_BIT7)` and
`AxisSession::requestClearAlarm(..., method)` choose the reset. The method changes
the two writes and nothing else: admission, the incident, the cycles, completion,
the single final ERRORS read, `valueOneMayRemain` and every refusal are shared.
Each second value goes out only after the first one's positive acknowledgement;
no write repeats, and no other object or value is written.

| Method | Writes (exact frames to node `n`) | Sources | Measured |
| --- | --- | --- | --- |
| `OBJECT_2301` | `2B 01 23 00 01 00 00 00`, then `2B 01 23 00 00 00 00 00` | EN86 p18 "Alarm reset, 0: Noneffective; 1: Effective", U16 RW; EDS `Obj.ClearAlm` U16 RW, PDO, default 0. ZH86 p18 and OP86 omit `2301`. The `0` is policy (D-27) | 9 attempts on 2026-11-13, mode 3, `FF05`, first node 1 trial 5323 ([below](fault_reference_actions.md#completion-measured-2026-11-13)): 8 cleared the alarm and left `6041=8010`, `2100=0001`; once (`2201=51200`) the alarm came back right after |
| `CONTROLWORD_BIT7` (default) | `2B 40 60 00 80 00 00 00`, then `2B 40 60 00 00 00 00 00` | EN86 and ZH86 p15, OP86 p13, "3.5.2 Alarm reset": "Set Bit7 of Control Word 6040h-00 to 1 to reset the drive alarm. It should be set to 0 manually." ZH86 p12 and OP86 p10 list `15->128`, "alarm reset", in the position-mode word table; EN86 p12 lacks the row | 11 of 11 on 2026-11-13, both specimens, mode 3, `FF05`: alarm cleared, drive released (`6041=8050`, `2100=0011`) |

Pages checked on the original PDFs on 2026-11-13 (text layer; the p12 tables also
in the [re-audit](reports/2026-11-13_fault_recovery_and_motor_limits.md)). The
manuals give the reset only after word 15; no source gives another predecessor,
so sending `0x0080` after whatever word the drive last took is policy: the action
never sends the 15, which would be an enable. Both words have bits 0..3 clear,
which the p12 tables pair with `6041` low bits `000` (`6040=0`), so on a drive
reporting itself switched on they may also release it (inference; every measured
clear was admitted from a released drive). Measured on this firmware: after a bit 7
reset the drive reported `6041=8050`, `2100=0011`, and after a `2301` clear
`8010`/`0001`, so the two are not equivalent here. Unknown: which of the two words
acted (the action reads only after the `0x0000`), whether bit 7 acts on its rising
edge or its level, and what other firmwares honor. No hold time between the two
writes is documented. Each acknowledged word of the controlword method becomes the
`AxisSession`'s latest controlword like any other, ending at `0x0000`, which
`stopPredecessorAllowed` does not list.

The S2 `clear-alarm` route names the method on its line (protocol 10), and
`scripts/s2_bench.py --method 2301|controlword` requires it
([route](hardware_bench.md#attended-bench-routes)). NMT Reset Node and Reset
Communication are separate attended experiments of the `N` line, not clear methods.

## Completion, measured 2026-11-13

**Finding** (device-reported, node 1, firmware `1000=00040192`, `1009=000c`,
`100A=0020`, image `6dee27b`, trial 5323, [ladder records](bench/20261113-ladder/)).
The `2301` method was admitted in mode 3 from `2100=001D`, `6041=C018`, `6061=3`,
`606C=0`, `1001=81`, `603F=FF05` and a `1003` count of 0. Both writes were
acknowledged. The first cycle after the `0`'s acknowledgement read `2100=0001`
(in place; alarm, running and released clear), `2101=0000`, `2102=0000` (it read
`0001` before), `6041=8010` (bit 3 clear, low bits `000`) and `6061=3`. The next
route's read (trial 5324) showed `1001=00`, `603F=0000`, `606C=0` and `6064=5497620`,
5 counts below the admission's `5497625`: on this firmware `603F` returned to zero
after this clear (C07). The rule of that image, released in every mode 3 cycle,
ended the attempt `OBSERVATION_SCOPE` (`failure=10 why=clear`), with no final ERRORS
read.

**Rule.** After the second write's acknowledgement a cycle completes the attempt,
and the one ERRORS read follows, when it reports the admission's mode and:

- mode 1: `2100` running and alarm clear; `6041` decides nothing, as before;
- mode 3: `6041` bits 0..2 `000`, `2100` alarm and running clear, and `6041` bits 3
  and 7 clear (the alarm bit of ZH86 p12, and of EN86 p12 and OP86 p10). `2100`
  released may read set or clear.

In mode 3, any cycle with `6041` low bits other than `000` (a switched-on pattern
such as `8031`, `8033`, `8027` or `8037`) ends the attempt `OBSERVATION_SCOPE`: the
drive keeps its `60FF` target. So does running with the alarm clear, a mode
change, or, while `2100` still reports the alarm, a lost release or running the
admission did not read as its latch. `2100` alarm still set, or `6041` bit 3 or 7
still set beside a clear `2100` alarm, keeps the attempt polling until the
deadline. The same rule completes both methods; the host tests replay trial
5323's exact report under each.

**Not established.** What `6041=8010` beside `2100=0001` means for torque: both
specimens report `8037` with `2100=0001` at power-up and held torque then, and
`8010` has low bits `000` with the release bit clear, a combination no source
explains. The shaft was not observed. One specimen, one firmware, one alarm:
this qualifies nothing, and whether the drive enables again from this report is a
separate measurement.

**The state after the clear, and its open transition** (device-reported, node 1,
2026-11-13, image `abe945b`, route protocol 10). After trial 5323 node 1 kept
reporting `6041=8010`, `2100=0001`, `6061=3`, `606C=0`, a steady `6064`, `1001=00`,
`603F=0000`, with `60FF` still the stored `4500`. No bench route admits that report:
`mode1` refuses it (`why=unreleased`), `stop` refuses it (failure 11), and a velocity
needs the released report (`8050`/`0011`, or the latched `0015`). An NMT Reset Node
changed nothing: the drive sent only its boot-up frame, and the next reads were the
same. The power-up report is `8037`/`0001` (holding torque) and the released report
after a `000B` is `8050`/`0011`. Which non-enabling controlword moves this report to
the released one is the question of the S2 `controlword` route (protocol 11, `TT`;
[hardware bench](hardware_bench.md)): one fresh full read with `6041` low bits `000`,
then exactly one reviewed word, `0000`, `0006`, `000B` or `0080`, and observation
cycles. Measured with it later on 2026-11-13 ([bench report](reports/2026-11-13_velocity_limits_and_fault_recovery.md)):
`0000`, `000B` and `0006` changed nothing (trials 5402 to 5404); `0080` released the
drive at once to `8050`/`0011`, mode and position kept (trial 5405, then 8 of 8).

## Rearm and the final zero

EN86 p18 and EDS `[2301]` define U16 `0` ineffective and `1` effective. The
second write returns to the documented ineffective value as library policy, not
a manufacturer pulse, edge or rearm rule. No hold time or auto-clear behavior is
known, and on the measured firmware it is not equivalent to the controlword reset
([methods](fault_reference_actions.md#two-reset-methods)). The controlword method's
`0x0000` is EN86/ZH86 p15's "set to 0 manually"; whether bit 7 acts on its edge
or its level is not documented either.

`2301` has no documented arming zero (D-27). If the `0` failed, timed out, was
rejected or never left, the object may still hold `1`, and if it is
edge-triggered, which is undocumented, a later `1` would make no edge. The
context then sets `valueOneMayRemain` when it fails: the `1` may have taken
effect (its terminal says an effect was possible, or its possible transmission
was noted before any terminal) and the `0` has no positive acknowledgement. An SDO
abort of the `1` sets it too, conservatively: the service layer counts an abort
as a possible effect. Under the controlword method the same flag means that
`6040` may still hold `0x0080`, bit 7 set, which p15 says must be returned to
`0` by hand. The
same field covers `2302`, where a held `1` has undocumented effects of its own.
Nothing clears the flag or resends either value, and a new operation identity
does not make a new edge. A further attempt is new user intent, after the cause
of the failure is established and the route is resolved.

## Historical errors and independent results

An alarm clear keeps `AlarmIncident incident`: the raw prerequisite values of
all four retained blocks, namely `1001`, `603F`, the error-history count and
entries, `2100` to `2102`, `6041` and `6061`, with the read's operation and one
earliest/latest bound across them. Later cycles replace the context's
observations; this record stays. It does not keep each field's own provenance:
every field shares the frozen reservation's route and node epoch, and the
application keeps its own read if it needs per-field times. A device-position
zero keeps `CounterBaseline baseline`, its admission `6064` and `606C` raw with
their bounds.

The sole owner's network state keeps only the **latest** EMCY and a count, so an
application that needs the original EMCY copies it before requesting a clear.
Interrupted operation results stay in the owner until released. Neither action
clears host event history, releases another result or declares application
health restored. An acknowledged clear is not permission to close the incident.

`2100` bit 3 has consistent alarm meaning in EN86 p17 and ZH86 p18. The
conflicting `6041` alarm bits (bit 3 in ZH86 p12, bit 7 in EN86 p12 and OP86 p10)
are retained raw. Since trial 5323 a mode 3 completion needs both clear, so no
document's alarm bit is ignored; neither decides a mode 1 attempt, and neither
is read as a reset success on its own. A subsequent nonzero `603F` is preserved
even when the monitor no longer reports alarm. This differs from the RS ESS profile, whose completion
needs its alarm word to read zero: CAN has no source that `603F` returns to zero.

ERRORS and ERROR_HISTORY may consume data, as reviewed for packet 7, so an
attempt reads ERRORS once, after the alarm has cleared, and never rereads the
history. Each download terminal is retained separately in `commandHistory`: the
effecting write is slot 0 for `2301=1` or `6040=0x0080` and slot 1 for `2302=1`. `execution`
describes that write, while the other slots record the arm and return-to-zero
writes. A successful trigger followed by a failed final zero keeps its trigger
acknowledgement; it does not claim rearm or completion. `completion=OBSERVED`
requires the new typed observations, never just an SDO reply. Their sequential
acquisition window is not an atomic physical snapshot. All read effects remain
`UNQUALIFIED` under the existing reviewed ledger.

The application delivers new fault and lifecycle evidence before advancing older
read results: `noteActionWithdrawal` while a service is in flight, or the local
`advanceAction` overload once it has been retired. Whether an EMCY during a clear
is a new fault is application policy. An EMCY whose code is `0000` is retained
raw like any other; it is neither a new fault nor recovery evidence, and neither
is an old EMCY. A fault on another node, with its own route, reservation and
network slot, changes nothing in this node's action. Alarm clear grants no
permission to resume an old operation.

## Coordinate withdrawal and owner synchronization

Device counter changes use the existing `invalidateAxisReference` contract with
`ReferenceLoss::COORDINATES`. The axis-aware `noteActionTransmission` overload
is mandatory for this action; the ordinary overload rejects it.

Before the `2302=1` trigger may leave the owner, the checked overload atomically
withdraws both origins, soft-limit qualification and the supplied reference, and
advances configuration/reference generations exactly once. The application
current target advances with the axis. Old prepared targets and cached
observations then fail generation checks, including reference-free native
absolute targets. Units, scales and the configured soft and native limit values
are retained: this withdraws validity, it does not disable limits, and the action
writes no input assignment. No replacement origin or known-home witness appears.

The preceding arm-zero does not claim a counter change, and a trigger refused by
the guard or cancelled while still queued leaves every epoch and validity as it
was. After possible trigger transmission, failure, cancellation or even a later
definite no-TX result cannot silently restore old coordinate confidence. Epoch
exhaustion rejects before work; it never wraps into a previously valid identity.

All three writes retain the original wire binding and continuous reservation.
After their old-epoch results and external TX have been released/retired, call
`CanBusOwner::advanceConfiguration` with the new application binding before
admitting fresh readback. The action's frozen reservation remains the original
exclusion token; observation work uses the new configuration. The owner's
configuration callback revokes PDO/cache provenance but must recognize this as
synchronization of an already withdrawn axis, **not** increment the axis a
second time. See [owner guidance](can_owner.md). `AxisSession` makes this call
itself, as its section below describes.

A failed final-zero write with definite retirement still permits explicit
synchronization after its retained old-epoch result is released, provided the
SDO channel is resolved. Any later read is a new operation using the new epoch.
It neither completes the failed action nor restores an origin or home. An
uncertain trigger or final zero is different: releasing its result or sequence,
a late TX report, a late acknowledgement and an advanced host epoch do not
permit a fresh reservation on the unresolved channel.

If a trigger/rearm reply is lost or retirement is unproved, the existing SDO
quarantine remains. No trailing cleanup, write replay, reset, new reservation or
alternate receiver bypasses it. Later observations belong to a new explicitly
admitted acquisition; they cannot rewrite the failed action's terminal. An MCU
restart loses the context, the axis epochs and the quarantine together: the new
`SdoClient` may claim `freshSessionQualified` only on its own external evidence,
and the restarted application has no coordinate confidence to restore.

## Application caller: `AxisSession`

[`AxisSession`](../examples/common/AxisSession.h) is the example application's
caller of both actions (backlog S12), tested through the real owner in
`axis_session_test`. The S2 bench's `counter-zero` and `clear-alarm` routes call
it. `requestCounterZero` ran once on a specimen (below). `requestClearAlarm`'s first
two attempts on 2026-11-13 were refused before any write, its `2301` method then
ran (trial 5323 first, above), and later its controlword method
([methods](fault_reference_actions.md#two-reset-methods)). The route's read includes
FEEDBACK, for the mode 3 latched release.

- **Request.** `requestClearAlarm(state, options, deadlineUs, nowUs, method)`, the
  method defaulting to `CONTROLWORD_BIT7` (`OBJECT_2301` before 2026-11-13), and
  `requestCounterZero(state, nativePosition, axis, reference, options,
  deadlineUs, nowUs)` prepare `clearAction()` on the session's reservation. The
  two share that one slot, and `actionKind` says which ran. Either is refused,
  with nothing changed, while a stop, enable, move, clear or command is live, or
  while `clearBlocked()`. A nonzero `nativePosition` returns `UNSUPPORTED` before
  any state, axis or budget check. `service()` then admits every write and read
  through the ordinary owner path, never a write twice.
- **Notices.** Each write's possible-TX notice is made when the session admits
  it, before any owner turn can carry it. For the counter clear's `1` that
  notice is the coordinate withdrawal, so `axis` and `reference` stay borrowed
  until the clear ends. If the application changed them in between, the notice
  refuses and the `1` is cancelled unsent, with nothing withdrawn.
- **Synchronization.** After the last `0` is acknowledged and released, the
  session calls `CanBusOwner::advanceConfiguration` itself, before the first
  read on the new epoch. After any other end once the `1` was admitted, even a
  `1` cancelled unsent, it makes the same call once nothing of it is
  outstanding. Neither withdraws coordinates: the application's configuration
  observer finds the axis already advanced and must not advance it again. An
  unresolved route never moves. A latest controlword from the old epoch is
  withdrawn. A session closed first drops the pending call, which the
  application then makes.
- **Stop.** `requestStop` ends an active clear first, even when the stop itself
  is refused. A write the owner has not handed over is cancelled, a request in
  flight settles into the context as evidence, and `preflight()` refuses a
  withdrawn clear's write at handover. The clear's writes withdrew the latest
  controlword, so the stop usually takes the `000B` branch. A first write
  cancelled unsent restores that word. Under the controlword method, the default, an
  acknowledged reset word becomes the latest instead (`0x0080`, then `0x0000`);
  neither is a stop predecessor, so a fresh idle report refuses the stop and any
  other report takes the `000B` branch, as for enable words 6 and 7.
- **Block.** A clear that ends with `valueOneMayRemain` sets `clearBlocked()`.
  Every later request except a stop is then refused for the session object's
  life, across result release, close and open, as MotorControl-RS
  `actionConflicts` does. Nothing clears it or resends a value. After an MCU
  restart only the application's own record can carry it.
- **EMCY.** The session has no EMCY view. The application copies the incident's
  EMCY before the request, since the owner keeps only the latest. It routes a
  later one with `cancelClear(nowUs, DRIVE_FAULT)`: the request in flight settles
  as evidence, and the clear ends with that reason. A clear withdrawn this way, or
  held back by `preflight`'s `driveFault`, does not set `faultBlocked()`.
- **Fault fence.** After a move or enable was withdrawn for a drive fault,
  `faultBlocked()` refuses `requestCounterZero` like every motion request.
  `requestClearAlarm` stays admissible under it, and a clear's success does not
  lift it; resuming motion needs a new session object.
- **Afterwards.** `requestEnable` stays refused because the session has written.
  No save, resume, home or host origin follows either clear.

## Time, support and remaining qualification

Caller time is nondecreasing monotonic microseconds. The immutable deadline is
exclusive: processing at or after it expires the action. Poll intervals and
queue/response budgets are caller policy. A reply captured before a cutoff but
processed afterward does not turn an expired action into success. Cancellation
is local withdrawal after retiring admitted work, not another drive command.
Rejected calls preserve their outputs.

The [prompt 32 report](reports/prompt_32.md) separates implemented conditional
paths and host tests from missing model/firmware qualification. Both resets' effect
beyond `FF05` in mode 3 on the one measured firmware, other fault classes, arbitrary
device-position setting and internal resume/counter relationships remain precise
gates. Counter zero, host origin,
successful homing, release and factory reset stay distinct. No action here
publishes a home reference or qualified physical measurement.

### Physical cases

Both need the lead at the bench, one drive, the independent drive-power cutoff
in hand and a free, table-mounted shaft. The alarm clear ran on `FF05` in mode 3
on 2026-11-13: the `2301` method 9 times, first on node 1 (trial 5323,
[above](fault_reference_actions.md#completion-measured-2026-11-13)), and the
controlword method 11 times on both specimens
([methods](fault_reference_actions.md#two-reset-methods)). The counter zero ran once:

**Counter zero, node 2, 2026-11-04** (trial 323, the bench `counter-zero`
route, [report](reports/2026-11-04_bench.md)). Node 2 was switched on and
energized at `6064=1180`, mode 1, `2100=0000`, `6041=0033`, `606C=0`, with `1001`
and `603F` 0; that count was left by a quick-stopped move and a word 7 pull-back
([drive actions](drive_actions.md)), so `baseline` held 1180, not zero. `2302`
`0`, `1`, `0` were each acknowledged, the coordinates were withdrawn, and the
next cycle read raw `6064=0` with `2100`, `6041` and `606C` unchanged:
`SUCCEEDED`, with no `valueOneMayRemain`. No motion was reported, but the
shaft was not observed: the user asked that motion be judged from drive reports.
Whether an internal command counter changed with `6064` is not shown. One
specimen, one firmware; this is a raw zero, not a home or origin.

The cases as planned:

1. **Alarm clear.** Needs a naturally present, identified alarm: `2100` alarm
   set, the LED flash count recorded, and `603F`, `1001`, `1003` read once. Do
   not create one by electrical fault, shaft obstruction, wiring disconnection
   or overcurrent. Record the raw blocks and the EMCY copy, then run one
   `prepareClearAlarm` through the public path with a named method, recording the
   exact bytes (`2B 01 23 00 01 00 00 00` and `2B 01 23 00 00 00 00 00`, or
   `2B 40 60 00 80 00 00 00` and `2B 40 60 00 00 00 00 00`), each TX report and
   ACK, the later cycles and the single final ERRORS read, and the LED flash
   count in one 5 s window. Abort, with no further write, on any report the
   completion rule puts outside its scope, an uncertain write, lost evidence or a
   bus fault. A persisting alarm is retained as the result. One attempt per
   identified incident; a repeat, or the other method, is a new reviewed trial.
   Record `6041` and `2100` before and after under either method: neither is
   documented for a reset, and the two methods ended in different reports.
2. **Counter zero.** Stationary, alarm-free drive in mode 1 with `606C` zero and
   an independent shaft mark. Record `607A`, `6064`, `606C`, the monitor words,
   `6041` and the axis reference metadata. At power-up both specimens read
   actual `0` and `607A=5000`, its EDS default, where a zero can only show that
   an already-zero raw stays zero; a nonzero baseline needs an earlier move, as
   on 2026-11-04 above. Do not write an arbitrary counter to manufacture one. Run one
   `prepareSetDevicePosition` with the bytes `2B 02 23 00 00 00 00 00`,
   `2B 02 23 00 01 00 00 00` and `2B 02 23 00 00 00 00 00`, then the owner epoch
   synchronization, the new-epoch cycles and the shaft check. Abort on any
   motion, alarm, uncertain write or bus fault; a missing final `0` keeps
   `valueOneMayRemain` and the route's quarantine.

Storage is fixed and caller-owned. The action context takes several kilobytes on
the pinned Xtensa compiler; `scripts/check_stack.py` measures the current size of
`ActionContext` per target in its report's `context_sizes`. Keep it in static or
owner storage, not a task local. Calls validate and then write that storage in place,
with no context copy on the stack; `scripts/check_stack.py` holds their frames
and known call chains to the project limits. Those are compiler measurements
with visible unknown edges, not a measured task high-water mark. No heap or
target stack sufficiency is implied by the native tests.
