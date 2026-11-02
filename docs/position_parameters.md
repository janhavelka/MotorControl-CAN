# Lichuan native position parameters

`LICHUAN_C::preparePositionParameters` prepares an owned numeric candidate for
the documented CL86-C/OL86-C position objects. It performs no I/O, yields no
service work and does not implement a finite move. The
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

Parameter writes in the current settings lifecycle advance configuration at
possible transmission. A target prepared before changed settings cannot simply
be submitted afterward. The future move lifecycle must order setup and capture,
or explicitly reprepare/revalidate at the new epoch; it must not relabel old
evidence with a new generation.

## Finite motion remains gated

The documented `000F -> 001F` absolute and `000F -> 005F` relative transitions
establish command examples and bit4's rising edge. They do not establish a
command-correlated acceptance/completion handshake, relative base, target FIFO
or refresh lease. `StateInterpretation` remains documentary; selecting its
EN86 layout or receiving a new upload of an old completion bit is not firmware
qualification. No arbitrary qualification Boolean enables motion here.

The current stop contract requires the actual latest acknowledged `000F`.
After `001F`/`005F`, or an uncertain trigger, that prerequisite is gone. Passing
an older `000F` or sending it to manufacture stop eligibility is invalid.
Prompt 31 still owns qualified stop transitions from those states.

An unknown submitted trigger leaves the existing SDO route unresolved. Releasing
results, changing host IDs or recovery cannot reopen it. A different case is an
acknowledged trigger followed by missing move completion: the communication
route can remain resolved. The future move context must retain trajectory
uncertainty and inhibit new motion independently of owner communication health.
It must preserve stop/interrupted results and never replay a relative trigger.

Consequently `prepareMoveAbsolute`, `prepareMoveRelative`, `nextMove`,
`advanceMove` and their concrete `MoveContext` remain unimplemented obligations
of prompt 33. No empty common move framework or always-rejecting lifecycle is
installed. The [stage report](reports/prompt_33.md) lists the exact device evidence
needed before these paths and their interruption tests can be implemented.
