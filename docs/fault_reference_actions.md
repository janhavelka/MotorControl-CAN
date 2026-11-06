# Fault clearing and device counter zero

`prepareClearAlarm` and `prepareSetDevicePosition` use the existing concrete
Lichuan `ActionContext`, `nextAction` and `advanceAction`. Applications provide
the same continuous owner reservation, current binding, time and finite budgets.
These operations are explicit requests, never startup, recovery or retry policy.
The core performs no I/O and owns copies of the evidence it consumes.

Evidence classes below: **source** means the original vendor page or EDS says it;
**inference** is our reading of a source; **policy** is a project decision;
**host** is fake-owner test evidence; **device-reported** is a drive's own report on a
named specimen, not an independent physical observation. The counter zero ran once, on node 2 on 2026-11-04 (below); no
specimen has had an alarm, so the alarm clear has not run (packets 17 and 18).

## Supported transition tables

Mode and monitor restrictions are conservative library admission policy, not a
claimed manufacturer state machine. All prerequisites are fresh checked SDO
observations acquired under the actual live reservation. Selecting CL86-C or
OL86-C is an explicit model binding, not firmware detection. No row sends an
enable, a controlword, a mode, a target, a save, a restore or an NMT command.

### Alarm clear (packet 17)

| Case | Fault provenance | Mode and state | Native route and values | Rearm and ACK | Required new observation | Raw error and history | Final uncertainty |
| --- | --- | --- | --- | --- | --- | --- | --- |
| Admitted attempt | One read under the reservation: `2100` alarm set and raw `603F=FF05`, with `1001` and the `1003` count and entries. `incident.codeLink=UNRESOLVED`: the code does not identify the alarm (D-26) | `6061=1`, `2100` running clear. `6041` kept raw, never decisive | `2301:00` U16 `1`, then `0` (source: EN86 p18 and EDS; ZH86 p18 and OP86 omit `2301`) | The `0` returns to the documented ineffective value as policy. No source requires an edge, rearm or pulse, and no arming `0` precedes the `1` (D-27). Each write needs its own matching ACK before the next; the `1`'s ACK is `execution=ACKNOWLEDGED` | After the `0`'s ACK, MONITORS and STATUS until `2100` alarm clear with mode 1 and running clear, then one ERRORS read | `incident` keeps the admission values. The final ERRORS read is kept raw, even when `603F` still reads `FF05`. `1003` is never read again | `OBSERVED` means a later report without the alarm bit, not that the cause is gone. `valueOneMayRemain` if the `0` lacks a positive ACK |
| Alarm persists after both ACKs | As admitted | Each cycle needs mode 1 and running clear, else `OBSERVATION_SCOPE` | No further write | No write repeats | Repolls MONITORS and STATUS only, at `observationIntervalUs`, until the deadline | Unchanged | `DEADLINE`, `execution=ACKNOWLEDGED`, `uncertain` |
| Withdrawal during the attempt | A new EMCY, boot-up, lifecycle change or cancel, delivered by the application | Any | The admitted write or read settles; nothing new is yielded | A terminal that still correlates joins `commandHistory` or the observation evidence. After a node-epoch change it no longer does, and the application ends the attempt locally with `INVALIDATED` | None | Unchanged; the owner's newest EMCY is separate | Outcome is the withdrawal reason; `valueOneMayRemain` follows the same rule |
| Rejected before work | Any other code, including `FF01` overcurrent, `FF02..FF04` (never mapped from LED counts) and `0`; no alarm; missing ERRORS or ERROR_HISTORY; stale or foreign read | Mode other than 1, running | None | None | None | None | Output unchanged |
| Controlword reset | `6040` bit 7 (EN86/ZH86 p15) or `15->128` (ZH86 p12, OP86 p10) | n/a | Not implemented. No API sends it; `stopPredecessorAllowed(0x0080)` is false and a `6040` terminal does not correlate with a clear | n/a | n/a | n/a | Separate question in Q13 |

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

## Rearm and the final zero

EN86 p18 and EDS `[2301]` define U16 `0` ineffective and `1` effective. The
second write returns to the documented ineffective value as library policy, not
a manufacturer pulse, edge or rearm rule. No hold time, auto-clear behavior or
equivalence to the controlword reset is known.

`2301` has no documented arming zero (D-27). If the `0` failed, timed out, was
rejected or never left, the object may still hold `1`, and if it is
edge-triggered, which is undocumented, a later `1` would make no edge. The
context then sets `valueOneMayRemain` when it fails: the `1` may have taken
effect (its terminal says an effect was possible, or its possible transmission
was noted before any terminal) and the `0` has no positive acknowledgement. An SDO
abort of the `1` sets it too, conservatively: the service layer counts an abort
as a possible effect. The
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
conflicting `6041` alarm bits are retained raw and are not combined into a reset
success mask. A subsequent nonzero `603F` is preserved even when the monitor no
longer reports alarm. This differs from the RS ESS profile, whose completion
needs its alarm word to read zero: CAN has no source that `603F` returns to zero.

ERRORS and ERROR_HISTORY may consume data, as reviewed for packet 7, so an
attempt reads ERRORS once, after the alarm has cleared, and never rereads the
history. Each download terminal is retained separately in `commandHistory`: the
effecting write is slot 0 for `2301=1` and slot 1 for `2302=1`. `execution`
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
it. `requestCounterZero` ran once on a specimen (below); `requestClearAlarm` has
not, because no alarm has occurred.

- **Request.** `requestClearAlarm(state, options, deadlineUs, nowUs)` and
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
  cancelled unsent restores that word.
- **Block.** A clear that ends with `valueOneMayRemain` sets `clearBlocked()`.
  Every later request except a stop is then refused for the session object's
  life, across result release, close and open, as MotorControl-RS
  `actionConflicts` does. Nothing clears it or resends a value. After an MCU
  restart only the application's own record can carry it.
- **EMCY.** The session has no EMCY view. The application copies the incident's
  EMCY before the request, since the owner keeps only the latest. It routes a
  later one with `cancelClear(nowUs, DRIVE_FAULT)`: the request in flight settles
  as evidence, and the clear ends with that reason.
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
paths and host tests from missing model/firmware qualification. The controlword
reset route, other fault classes, arbitrary device-position setting and internal
resume/counter relationships remain precise gates. Counter zero, host origin,
successful homing, release and factory reset stay distinct. No action here
publishes a home reference or qualified physical measurement.

### Physical cases

Both need the lead at the bench, one drive, the independent drive-power cutoff
in hand and a free, table-mounted shaft. The alarm clear has not run. The
counter zero ran once:

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
   `prepareClearAlarm` through the public path, recording the exact bytes
   `2B 01 23 00 01 00 00 00` and `2B 01 23 00 00 00 00 00`, each TX report and
   ACK, the later cycles and the single final ERRORS read, and the LED. Abort,
   with no further write, on any running report, mode change, uncertain write,
   lost evidence or bus fault. A persisting alarm is retained as the result.
   One attempt per identified incident; a repeat is a new reviewed trial.
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
