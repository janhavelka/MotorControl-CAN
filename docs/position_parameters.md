# Lichuan native position parameters and absolute moves

`LICHUAN_C::preparePositionParameters` prepares an owned numeric candidate for
the documented CL86-C/OL86-C position objects. It performs no I/O and yields no
service work; the gated absolute move below builds on it. The
[native example](../examples/position_preview/README.md) uses the installed
C++11 core without a CLI, SDK, CAN controller or stack.

## Numeric contract

| Candidate | Source and checked limits |
| --- | --- |
| `targetNative` | EN86/ZH86 PDF p11 and EDS `607A`: I32, -1,000,000 through +1,000,000 command pulses |
| `settings.positionSpeedRpm` | `6081`: U32, 0 through 3000 rpm |
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
explicit matching witnesses and axis policy. None is advertised as a supported
Lichuan relative execution basis. Current preparation time is used to recheck a
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
Relative moves belong to packet 21 and are refused here.

**The move is gated.** No source or specimen establishes which report shows that
this trigger was accepted, or that a completion report is new. `MoveEvidence`
therefore carries the caller's reviewed measurement, and its default
`MoveGate::CLOSED` returns `UNSUPPORTED` before anything else is checked. Only
fake-owner tests have run this lifecycle. No drive has.

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
| Witness | Optional. A supplied one must be idle, stationary, `ACTUAL` and fresh, and then fixes the displacement, which must not be zero (R-08). Without one the displacement stays `UNKNOWN`; an absolute zero is an ordinary target, and nothing manufactures a no-op success |
| Deadline | Beyond `MOVE_SERVICE_STEPS` (13) queue-and-response budgets. That cannot guarantee completion |
| Refused forms | Immediate update (`003F`), relative or wrapped intent, `MEASURED_U8` widths, an age cap below seven fields |

Preparation validates everything, then constructs the context in place.
`validateMoveAbsolute` reports the same refusal with nothing written, so an
owner operation can be started only once admission is known to pass.

### Phases and wire order

| Phase | Frames, node 1 | Note |
| --- | --- | --- |
| `PARAMETERS` | `23 7A 60 00 ..`, `23 81 60 00 ..`, `23 83 60 00 ..`, `23 84 60 00 ..` | EN86/ZH86 p11-12, OP86 p9-10 order. `REUSE_STORED` skips each of `6081`, `6083`, `6084` that a fresh, unretired MOTION_PARAMETERS read shows already equal |
| `TRIGGER` | `2B 40 60 00 1F 00 00 00` | `000F` to `001F`, held until the move is terminal (DEC-8) |
| `WAITING`, `OBSERVING` | Uploads of `2100`, `2101`, `2102`, `6041`, `6061`, `6064`, `606C` | One cycle per interval, each beginning after the trigger's acknowledgement |
| `REARM` | `2B 40 60 00 0F 00 00 00` | Only after an observed completion, as its own visible step |

No `6060` or `6061` write, enable word, fault clear or stop is ever emitted. Each
download takes `noteMoveTransmission` after owner admission and before any owner
turn could carry it. For a parameter download that call runs
`retireMotionParameters` on every retained context first; if one refuses, the
download must be cancelled unsent. The configuration epoch does not move, so the
move's own target stays valid while a later settings call needs a fresh read.
Any explicit settings call, or a device counter zero, does move it, and the move
then refuses every call but its own ending.

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
