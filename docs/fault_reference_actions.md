# Fault clearing and device counter zero

`prepareClearAlarm` and `prepareSetDevicePosition` use the existing concrete
Lichuan `ActionContext`, `nextAction` and `advanceAction`. Applications provide
the same continuous owner reservation, current binding, time and finite budgets.
These operations are explicit requests, never startup, recovery or retry policy.
The core performs no I/O and owns copies of the evidence it consumes.

## Reviewed transition table

This table defines the scoped implementation. Mode and monitor restrictions
below are conservative library admission policy, not a claimed manufacturer
state machine. All prerequisites are fresh checked SDO observations acquired
under the actual live reservation; selecting CL86-C/OL86-C remains an explicit
model binding, not automatic firmware detection.

| Action | Admission | Exact native sequence | New completion evidence | Consequences not established |
| --- | --- | --- | --- | --- |
| Alarm clear | Mode 1, monitor not running and alarm present, `603F=FF05`; complete ERRORS, ERROR_HISTORY, MONITORS and STATUS retained | `2301:00` U16 `1`, then `0`, each with its own matching acknowledgement | Fresh mode 1 / not-running report and `2100` alarm bit clear, after both writes | Fault cause removed, repeat-trigger/edge semantics, torque, reference or absence of internal resumption |
| Device-position zero | Requested native position exactly zero; mode 1, monitor not running and no alarm; matching valid axis epochs with room to advance | `2302:00` U16 `0`, `1`, `0`, acknowledging each before the next | Fresh mode 1 / not-running / no-alarm reports and `6064` raw signed actual position equal to zero | Physical home, arbitrary position setting, all internal counters cleared, command/feedback scale or preserved coordinate reference |
| Other position value, unsupported state/mode/fault or stale binding | Reject preparation atomically | No work | None | No clamp, guessed reset route, enabling preamble or factory reset |

EN86 PDF p18 and EDS `[2301]` define U16 `0` ineffective / `1` effective.
The second write explicitly returns to the documented ineffective value. It is
a library policy, **not** a manufacturer-specified pulse, edge or rearm rule.
No hold time, auto-clear behavior or equivalence to controlword reset is known.
`2301` has no documented arming zero: if a final `0` was never sent and the object
is edge-triggered, which is undocumented, a later `1` would produce no edge (Q13).
An acknowledged attempt that leaves the alarm present waits for new bounded
observations until its deadline; it does not repeat the writes.

EN86 p16 identifies `FF05` as position deviation and p20 lists a position
out-of-tolerance alarm as resettable. The two are linked only by name: p20 gives no
code and no reset route, so that link is an inference. This guide also notes `603F`
may retain its value, so a stale `FF05` could admit a clear for a later alarm; Q13
keeps both open, and packet 17 corrects the action's error text. `FF01` overcurrent requires restart under the source's fault table;
this API does not restart it. No `FF02..FF04` code mapping is inferred from LED
counts. Other faults remain outside this action's admitted scope.

EN86/ZH86 p15 explicitly describe the `2302` rising `0 -> 1` and manual return
to zero. EDS `[2302]` confirms the U16 object. EN86 p16 names `6064` actual
position, I32 RO. The clear prose does not explicitly map every internal counter
to that object: the result is an observed raw zero, not proof of which counters
changed or that a previously zero counter changed at all.

`6040` bit-7 reset prose and the full-word `15 -> 128` table remain a separate,
unimplemented route. Nothing sends `000F`, `6,7,15`, NMT reset/start, `2300`
save/factory-reset or a motion trigger. Mode is observed, never changed by these
actions. Digital-output and tuning operations keep their existing owners.

## Historical errors and independent results

The context copies the initial read context, including original raw `603F`,
status and error-history entries. The sole owner's original EMCY records and
interrupted operation results remain separate; the application retains its
incident snapshot before requesting clear. Neither action clears host event
history, releases another result or declares application health restored.

`2100` bit 3 has consistent alarm meaning in EN86 p17 and ZH86 p18. The
conflicting `6041` alarm bits are retained raw and are not combined into a reset
success mask. `603F` latching/clearing semantics are unspecified: a subsequent
nonzero raw value is preserved even if the monitor no longer reports alarm.
This difference from the RS profile is intentional.

Each download terminal is retained separately. `execution` describes the
effecting write (`2301=1` or `2302=1`), while command history records the arm and
return-to-zero writes too. A successful trigger followed by a failed final zero
keeps its trigger acknowledgement; it does not claim rearm or completion.
`completion=OBSERVED` requires the new typed observations, never just an SDO
reply. Their sequential acquisition window is not an atomic physical snapshot.
All read effects remain `UNQUALIFIED` under the existing reviewed ledger.

The application delivers new fault/lifecycle evidence before advancing older
read results. It distinguishes the known incident being cleared from a newly
reported fault; neither an old EMCY nor an EMCY reset envelope supplies physical
recovery evidence. Alarm clear grants no permission to resume an old operation.

## Coordinate withdrawal and owner synchronization

Device counter changes use the existing `invalidateAxisReference` contract with
`ReferenceLoss::COORDINATES`. The axis-aware `noteActionTransmission` overload
is mandatory for this action; the ordinary overload rejects it.

Before the `2302=1` trigger may leave the owner, the checked overload atomically
withdraws both origins, soft limits and the supplied reference, and advances
configuration/reference generations exactly once. The application current
target advances with the axis. Old prepared targets and cached observations
then fail generation checks, including reference-free native absolute targets.
Units/scales are retained; no replacement origin or known-home witness appears.

The preceding arm-zero does not claim a counter change. After possible trigger
transmission, failure, cancellation or even a later definite no-TX result cannot
silently restore old coordinate confidence. Epoch exhaustion rejects before
work; it never wraps into a previously valid identity.

All three writes retain the original wire binding and continuous reservation.
After their old-epoch results and external TX have been released/retired, call
`CanBusOwner::advanceConfiguration` with the new application binding before
admitting fresh readback. The action's frozen reservation remains the original
exclusion token; observation work uses the new configuration. The owner's
configuration callback revokes PDO/cache provenance but must recognize this as
synchronization of an already withdrawn axis, **not** increment the axis a
second time. See [owner guidance](can_owner.md).

A failed final-zero write with definite retirement still permits explicit
synchronization after its retained old-epoch result is released, provided the
SDO channel is resolved. Any later read is a new operation using the new epoch.
It neither completes the failed action nor restores an origin or home. An
uncertain trigger is different: releasing its result or sequence, a late TX
report and a late acknowledgement do not permit a fresh reservation on the
unresolved channel.

If a trigger/rearm reply is lost or retirement is unproved, the existing SDO
quarantine remains. No trailing cleanup, write replay, reset, new reservation or
alternate receiver bypasses it. Later observations belong to a new explicitly
admitted acquisition; they cannot rewrite the failed action's terminal.

## Time, support and remaining qualification

Caller time is nondecreasing monotonic microseconds. The immutable deadline is
exclusive: processing at or after it expires the action. Poll intervals and queue/response budgets are caller
policy. A reply captured before a cutoff but processed afterward does not turn
an expired action into success. Cancellation is local withdrawal after retiring
admitted work, not another drive command. Rejected calls preserve their outputs.

The [prompt 32 report](reports/prompt_32.md) separates implemented conditional
paths and host tests from missing model/firmware qualification. The controlword
reset route, other fault classes, arbitrary device-position setting and internal
resume/counter relationships remain precise gates. Counter zero, host origin,
successful homing, release and factory reset stay distinct. No action here
publishes a home reference or qualified physical measurement.

Storage is fixed and caller-owned. On the Windows GCC 15.1 x64 ABI, the action
context occupies 7,496 bytes, including the copied original read and three
download terminals. This is an object-size measurement, not target stack usage.
Atomic candidate copies also consume stack; firmware integration must measure
its actual stack budget before placing this lifecycle in a task. No heap or
target stack sufficiency is implied by the native tests.
