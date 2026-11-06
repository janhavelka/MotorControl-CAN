# Lichuan native position parameters and finite moves

`LICHUAN_C::preparePositionParameters` prepares an owned numeric candidate for
the documented CL86-C/OL86-C position objects. It performs no I/O and yields no
service work; the gated absolute and relative moves below build on it. The
[native example](../examples/position_preview/README.md) uses the installed
C++11 core without a CLI, SDK, CAN controller or stack.

## Numeric contract

| Candidate | Source and checked limits |
| --- | --- |
| `targetNative` | `607A`: I32 (EDS), -1,000,000 through +1,000,000 command pulses (EN86/ZH86 PDF p11, OP86 p9). The EDS gives types and defaults, no limits |
| `settings.positionSpeedRpm` | `6081`: U32, 0 through 3000 rpm (p11). EN86/ZH86 p17 name it profile velocity, and the EDS posmode speed; the unit rests on p11 |
| `settings.rampUpTimeMs` | `6083`: U32, 0 through 2000 ms |
| `settings.rampDownTimeMs` | `6084`: U32, 0 through 2000 ms |

The function reuses `preparePosition` for exact units, coordinate origins,
rounding, error allowances, endpoint limits and reference validation. It keeps
the complete `PreparedTarget`, `AxisConfig` and requested `DriverSettings`, plus
explicit model and preparation time. Inputs are borrowed during the call only;
rejection leaves output unchanged, including when output overlaps an input.
Binary32 native output and wrapped intent are outside this interface. Existing
explicit approximate-radian conversion remains available under its checked
error allowance; no implicit approximation is added.

The native object range applies to both requested and effective encoded values.
For a relative request that value is a displacement, not the resulting endpoint.
For example, base 900,000 plus displacement 200,000 gives `targetNative=200000`
and `target.endpointNative=1100000`. This is valid arithmetic if the separately
supplied axis limits allow the endpoint; the `607A` table does not establish an
absolute travel limit for relative moves. A request just beyond the encoded
limit rejects even if its rounding policy would bring the integer back inside.
Approximate requests conservatively require the entire existing error interval
to remain inside that range.

ACTUAL, COMMANDED and QUEUED bases are tested as common host arithmetic with
explicit matching witnesses and axis policy. None is a supported Lichuan
relative execution basis by default: the relative move below needs separate
evidence of the drive's own base, which one specimen has supplied for `ACTUAL`. Current preparation time is used to recheck a
supplied witness without renewing its observation time or age limit. A zero
native target without a witness says nothing about displacement or standstill;
a zero relative displacement is also only arithmetic. Numeric zero speed and
ramp times follow the source table but do not qualify their trajectory effects.

## Snapshots and future admission

This snapshot is requested intent, not a verified settings cache. A later move
must recheck its current route, binding/configuration/reference generations,
firmware and operation prerequisites. Existing `validatePreparedTarget` checks
the common target against actual current configuration/reference; a stored
snapshot is not a replacement for those live facts. Contradictory observations,
settings changes, reboot and coordinate invalidation revoke dependent prepared
targets under the existing axis/owner contracts.

An explicit settings call advances the configuration epoch at its first
possible download, so a target prepared before it cannot be submitted afterward.
A move's own parameter writes do not move the epoch; see the move section below.

## Finite absolute move (gated)

`prepareMoveAbsolute`, `nextMove`, `beginMoveObservation`, `noteMoveTransmission`,
`noteMoveWithdrawal` and `advanceMove` in the same header run one finite
absolute move through the sole owner. The concrete `MoveContext` is the result.
Common intent and outcome names are in `MotorControlCAN/MoveOperation.h`.
`prepareMoveAbsolute` refuses relative intent; the relative move further down
uses the same calls.

**The move is gated.** No source establishes which report shows that this
trigger was accepted, or that a completion report is new. `MoveEvidence`
therefore carries the caller's reviewed measurement, and its default
`MoveGate::CLOSED` returns `UNSUPPORTED` before anything else is checked.

**One specimen run, 2026-11-03.** With node 1's measured evidence from
[packet 19](reports/handoff_19_33/packet_19.md) (acceptance `6041` mask and value
`0x1000`, completion `2100` mask and value `0x0001`, `AFTER_ABSENCE`), the bench
`move` route ran this lifecycle once through `AxisSession::requestMove` on node 2,
unattended: 0 to 250 pulses at 2 rpm, `SUCCEEDED` with completion observed and its
own `000F` rearm acknowledged, the same bits as node 1. Completion was reported at
`6064=251`, and the next read gave 250; whether the drive overshot and returned or
the counter dithers inside the in-place window is not established. No shaft was
observed, and node 2's scale is not measured. That is one specimen and one firmware,
not a qualification ([unattended report](reports/2026-11-03_unattended_bench.md)).

**Later runs, 2026-11-04,** judged from drive reports with no shaft observed
([report](reports/2026-11-04_bench.md)). The same 0 to 250 at 2 rpm on node 2,
through the application-local `position` route rather than this lifecycle,
completed at exactly 250 and held it for 8 s, so the 251 did not recur; a one-pulse
overshoot is still possible. This lifecycle then ran with a planned quick stop
twice at 30 rpm (250 to 2250 on node 2, 404 to 2404 on node 1): each move ended
interrupted and fenced, its stop with its own result. It refused one plan before
any write, because the deadline did not cover every scheduled service at its
budgets. The relative move below succeeded (trial 411). `606C` reads rpm on this
configuration.

**Later runs, 2026-11-05,** node 1, drive reports only
([report](reports/2026-11-05_motion_and_stress.md)). This lifecycle succeeded
at 10 rpm from 0 to 1000 with 200 ms ramps, completing at exactly 1000 (705), and
at 60 rpm from 1000 to -1000 with 1000 ms ramps, where completion was reported at
-999 and a read 40 s later gave -1000 (706, 707). The relative lifecycle succeeded
with +3000 at 120 rpm, completing at exactly 2000 (708). Across the retained moves,
library and `position` route, completion was reported 0 or 1 pulse from the target
(121 and 706: 1; 211, 411, 705 and 708: 0); the in-place window's size is not
measured. Everything else for these lifecycles
is fake-owner evidence.

### Admission, before any write

| Input | Required |
| --- | --- |
| Reservation and current target | The actual live reservation on CL86-C or OL86-C, equal to the current route, binding, configuration and node epochs |
| Predecessor | The actual latest acknowledged `6040:00` under that reservation, exactly `000F`. A move never writes `000F` to make one. After a trigger, a stop or an uncertain word there is none |
| State read | Complete fresh SDO STATUS and MONITORS after that `000F`: mode 1, `6041` low bits 111, `2100` with no running, alarm, released, soft-limit or undocumented bit. Fresh flags cannot show an enable, since the specimens read 111 at power-up, so the `000F` history is the enable evidence (D-01, prompt 30) |
| Identity | A current identity read on the same route and node epoch, whose raw `1000`, `1009`, `100A`, `1018:01..03`, `2000` and `2001` equal the evidence. Another firmware or boot is `UNSUPPORTED` |
| Evidence | `MEASURED`, a nonzero record identity, the reservation's model, two signals with a nonzero mask and a value inside it, and a completion rule. The acceptance report must be absent in the state read |
| Target | Unwrapped absolute intent. `607A` must fit -1,000,000..1,000,000 pulses both as requested and as quantized |
| Speed | Prepared once with `prepareVelocityTarget` and refused unless it comes out integer motor turns per minute, because `6081` is rpm. 1 load turn/min at 3,200 increments per turn and gear 5 is 267 under the default `COMMAND_INCREMENTS` per `SECOND`: inside `0..3000` and wrong, so it is refused. Requested and quantized values must be in 1..3000 rpm |
| Ramps | `6083` and `6084` in 1..2000 ms. Zero is a documentary value with no qualified meaning (Q11) |
| Witness | Optional. A supplied one must be idle, stationary, `ACTUAL`, fresh and observed at or after the predecessor `000F` completed, and then fixes the displacement, which must not be zero (R-08). Without one the displacement stays `UNKNOWN`; an absolute zero is an ordinary target, and nothing manufactures a no-op success |
| Deadline | Beyond `MOVE_SERVICE_STEPS` (13) queue-and-response budgets. That cannot guarantee completion |
| Refused forms | Immediate update (`003F`), relative or wrapped intent, `MEASURED_U8` or `MEASURED_U16` widths, an age cap below seven fields |

Preparation validates everything, then constructs the context in place.
`validateMoveAbsolute` reports the same refusal with nothing written, so an
owner operation can be started only once admission is known to pass.

### Phases and wire order

| Phase | Frames, node 1 | Note |
| --- | --- | --- |
| `PARAMETERS` | `23 7A 60 00 ..`, `23 81 60 00 ..`, `23 83 60 00 ..`, `23 84 60 00 ..` | EN86/ZH86 p11-12, OP86 p9-10 order (OP86 under a CL57-C/OL57-C running header, C18). `REUSE_STORED` skips each of `6081`, `6083`, `6084` that a fresh, unretired MOTION_PARAMETERS read shows already equal |
| `TRIGGER` | `2B 40 60 00 1F 00 00 00` | `000F` to `001F`, held until the move is terminal (DEC-8) |
| `WAITING`, `OBSERVING` | Uploads of `2100`, `2101`, `2102`, `6041`, `6061`, `6064`, `606C` | One cycle per interval, each beginning after the trigger's acknowledgement |
| `REARM` | `2B 40 60 00 0F 00 00 00` | Only after an observed completion, as its own visible step |

No `6060` or `6061` write, enable word, fault clear or stop is ever emitted. Each
download takes `noteMoveTransmission` after owner admission and before any owner
turn could carry it. A parameter or trigger notice is the move's actual
admission, so it first rechecks what preparation checked: `validatePreparedTarget`
against the caller's current `AxisConfig` and witness, a witness still idle and
stationary, and the admission state read within `maximumAgeUs`. A later
`configureAxis`, origin change, `invalidateAxisReference` or replaced, moving or
aged witness therefore refuses the next write, and the download must be
cancelled unsent; the rearm takes no recheck. The owner hands the frame over
only at a later turn. `AxisSession::preflight`, called from the application's
`Transport::beforeTransmit`, reruns the same recheck at that handover, and a
refusal there closes the download provably unsent (`PREFLIGHT_REJECTED`). Both
uses call one nonmutating function, `validateMoveAdmission(context, axis,
reference, nowUs)`, with the same status and detail; mutation stays in
`noteMoveTransmission`.
Without that hook the recheck holds at admission only. For a parameter download the call
then runs `retireMotionParameters` on every retained context; if one refuses,
the download must be cancelled unsent too. The configuration epoch does not
move, so the move's own target stays valid while a later settings call needs a
fresh read. Any explicit settings call, or a device counter zero, does move it,
and the move then refuses every call but its own ending. A target with no
witness survives a `POSITION` loss, as `validatePreparedTarget` defines.

Observations run in a borrowed `ReadContext`, never inside the move context, so
the application can keep the last complete cycle for a stop. Each cycle must
report mode 1, low bits 111, and `2100` alarm, released, soft-limit and
undocumented bits clear, or the move ends `OBSERVATION_SCOPE` with that raw
cycle kept.

### Acceptance and completion

Both are the caller's measured `MoveSignal`s: present when `(word & mask) ==
value`, from `6041` or `2100`. The library supplies no default mask; EN86 bit 12,
bit 15 and `2100` bit 0 are documentary candidates only. Acceptance counts in
the first post-trigger cycle that reports it. Completion counts only after
acceptance, and only when new under the evidence's rule:

| Rule | Completion counts in |
| --- | --- |
| `AFTER_ABSENCE` | A cycle after an earlier post-trigger cycle reported completion absent. A move shorter than one cycle never completes and ends at its deadline |
| `AFTER_ACCEPTANCE` | Any cycle after the cycle that first reported acceptance. Only valid if the measurement showed that acceptance withdraws an earlier completion |

A successful SDO, an old completion, a completion before acceptance, or a cycle
consumed at or after the cutoff never finishes a move. Completion is a
controller report: no position, standstill or torque is claimed. The rearm's
result never erases it, and a rejected or lost `000F` is never resent.

### Interruption, uncertainty and later motion

`now >= deadlineUs` expires at processing time, even when a reply was received
earlier (G-13). Cancel, an application-routed drive fault, a reboot or rebinding,
and a stop request all end the move; work in flight settles first and its
terminal is kept as evidence. Nothing is resumed or replayed, and a local cancel
is not a motor stop. A trigger that may have reached the drive leaves the move
`uncertain` until a completion is observed.

`AxisSession` closes a move download the owner has not yet handed to the
controller when the move is withdrawn, so a queued parameter or trigger never
leaves after a cancel, fault or stop request, even a refused one. Work that may
have left is never cancelled; it settles. When the owner closes the work of a
withdrawn reservation as `CANCELLED`, the session records `INVALIDATED` first,
so the move does not report a cancel it never made.

`AxisSession` integrates it with one member, one request, one service branch and
one advance. Its two observation reads alternate, so `moveObservation()` holds
the last complete cycle. A stop request ends the move first. From the held `001F`
with such a cycle after it, the stop is sequenced (`predecessorWord` `001F`,
`predecessorDocumented` false, no `000F` preamble); otherwise it takes the
best-effort `000B`. After a trigger whose reply was lost the route stays
unresolved, so only that best-effort stop can follow, and its reply never
qualifies. After any trigger that may have reached the drive without an observed
completion, including one the drive answered with an abort, the session refuses
new moves for the object's life, whatever later idle report or explicit `000F`
arrives. A read reused under `REUSE_STORED` must be one the session retires on
every move write, named with `retainSettings`; any other is refused. After a
notice, `nextMove` never offers the same download again, and a partial setup
reports `setup` as `UNKNOWN` with `setupHistory` saying which writes were
acknowledged.

### Evidence packet 19 must supply before the gate can open

All of it per specimen and firmware, with raw traffic and independent shaft
observation:

1. The raw identity values above, read with the width policy the fixture needs.
2. Which word, mask and value report that this trigger was accepted; that it is
   absent at an idle `000F` before the trigger and after the `000F` rearm; when it
   appears after the `001F` acknowledgement; whether it holds while bit 4 stays set.
3. Which word, mask and value report completion; its value at an idle `000F`;
   whether it clears on acceptance or only while moving, including a move shorter
   than one observation cycle and a target equal to the current position.
4. That mode stays 1, low bits stay 111, and `2100` alarm, released, soft-limit and
   undocumented bits stay clear through a move, so the scope check is not a false
   failure.
5. That `607A`, `6081`, `6083` and `6084` written at an idle `000F` cause no motion
   before the edge, and that the shaft moves the commanded direction and distance
   for one native target.
6. What the `001F` to `000F` rearm does after completion: any change in position,
   acceptance or completion.
7. The DEC-8 stop rows from `001F` belong to packet 16.

## Finite relative move (gated twice)

`prepareMoveRelative` and `validateMoveRelative` run a relative move through the
same `nextMove`, `beginMoveObservation`, `noteMoveTransmission`,
`noteMoveWithdrawal` and `advanceMove`, on the same `MoveContext`. There is no
second engine: the phases, gates, observation rules, rearm and uncertainty above
all apply. The differences:

| Item | Relative move |
| --- | --- |
| `607A` | The quantized displacement, -1,000,000..1,000,000 pulses requested and quantized. Both endpoints must fit the axis's native and soft limits; the `607A` table bounds the displacement only |
| Trigger | `000F` to `005F` (EN86/ZH86 p12, OP86 p10: 15 to 95, bit 6 relative), held until terminal. `007F` immediate update is `UNSUPPORTED` |
| Witness | Required, of the request's basis: known, idle, stationary, fresh, with a nonzero age cap, and observed at or after the predecessor `000F` completed, so a witness from before an earlier move or enable is refused |
| Displacement | Always `KNOWN`; zero, including a request that quantizes to zero, is `ZERO_DISPLACEMENT` before any write (R-08) |
| Snapshot | `parameters.target` keeps the request, the displacement and its exact quantization error, the host endpoint and the exact witness snapshot with its epochs |

**The drive's base is a second gate.** The manuals give the relative trigger
but not what the drive adds the displacement to. `MoveEvidence::relativeBase`
names the caller's measurement of that base on the exact firmware:

| Request basis | `relativeBase` needed | Otherwise |
| --- | --- | --- |
| `ACTUAL` | `ACTUAL` | `UNSUPPORTED`, `RELATIVE_BASIS` |
| `COMMANDED` | `COMMANDED` | `UNSUPPORTED`, `RELATIVE_BASIS` |
| `QUEUED` | None exists | Always `UNSUPPORTED`, `RELATIVE_BASIS`: a queued base needs an edge in a running curve, and this lifecycle starts a move only from an idle drive. The manuals document bit 5 = 0 as updating the parameters after the current curve completes (EN86/ZH86 p12, OP86 p10), but give no capacity, acknowledgement or base for it |

The default is `UNMEASURED`, so a relative request is refused unless the caller
names a measured base. **Node 1 measured `ACTUAL` on 2026-11-04** (trial 409):
after a halted move left it at 104 with target 600, `005F` with +200 ended at
304. With that record, `requestMoveRelative` +100 under `ACTUAL` then
`SUCCEEDED` at 404, its computed endpoint, with its own rearm (trial 411), and on
2026-11-05 +3000 at 120 rpm `SUCCEEDED` at 2000 (trial 708). One specimen, one
firmware, positive displacements, judged from drive reports with no shaft observed ([report](reports/2026-11-04_bench.md),
[packet 21](reports/handoff_19_33/packet_21.md)). The axis's
`supportedRelativeBases` is a separate host-arithmetic policy and is still checked.

Two bases stay apart in the context. `parameters.target.retainedReference` is
the host arithmetic reference; `evidence.relativeBase` is the drive's measured
base. `parameters.target.endpointNative` is only the host's expected endpoint:
the drive reaches it only if its base equals the witness at the edge. A
completion report never establishes it, nothing writes it back into a witness,
and after a reference change `validatePreparedTarget` refuses the frozen target.
Revalidation is explicit, with a new witness, and starts a new move.

`AxisSession::requestMoveRelative` takes the same path as `requestMove`. Both
borrow the axis configuration and witness until the move is terminal, for the
admission rechecks and their `preflight` repeat. A refused recheck ends the move
`INVALIDATED`, and `AxisSession::moveRefusal` keeps its reason: a changed, aged
or invalidated target or witness, a moving witness, or an aged admission read.
Each call is one explicit edge: identical values twice are
two moves with two witnesses, and nothing retries a trigger, a parameter or a
rearm. After a trigger that may have reached the drive without an observed
completion, new moves stay fenced as for an absolute move.

**Drive faults.** One owner turn receives before it transmits, so an EMCY
received in a turn does not by itself stop a download queued before it from
leaving in that same turn. The application keeps a per-node EMCY baseline and
passes its unhandled-fault fact as `preflight`'s `driveFault`; that node's
queued parameter, trigger or rearm download is then refused unsent and the move
ends `DRIVE_FAULT`. A frame already handed over settles as evidence, with its
possible effect kept. A move or enable withdrawn for a drive fault sets
`AxisSession::faultBlocked()`: for that session object's life, across release,
close and open, every later move, relative move, enable and counter zero is
refused (`STATE`); a stop and an alarm clear stay admissible, and a clear's
success does not lift it. Resuming motion is an explicit application decision
with a new session object. A fault reported only after the move has already
ended withdraws nothing and sets no fence; the next move's fresh admission read
must then show the drive's state.

### Evidence a relative base needs

Per specimen and firmware, with raw traffic and an independent shaft
observation, beside the absolute items above:

1. Whether `000F` to `005F` moves the shaft by the written displacement from
   its actual position, from the last commanded endpoint, or otherwise. The two
   differ only when actual and commanded positions differ, for example after a
   halted move or a stall, so the trial needs such a start.
2. That the absolute acceptance and completion signals behave the same after
   `005F`, or the record must carry measured ones for it.
3. What `005F` does with a zero `607A`, which the library never sends.

For node 1 on its firmware, items 1 and 2 are answered by drive reports, without
a shaft observation: the base is the actual position, and acceptance (`6041` bit
12 from the first cycle) and completion (`2100` bit 0 absent while running, then
present) behave as after `001F`. Item 3, node 2, a negative displacement and the
shaft remain.
