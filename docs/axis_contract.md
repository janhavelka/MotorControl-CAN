# Common axis and exact intent contract

The prompt-6 [value declarations](../experiments/api_ownership/Contracts.h) and
[synthetic operations](../experiments/api_ownership/Operations.h) are a native
ownership proof, not an installed motion library. This page owns their common
identity, capability and coordinate semantics. The [architecture](architecture.md)
owns dependency direction; the [profile contract](profile_contract.md) owns typed
operations; the [CAN owner](can_owner.md) owns scheduling and invalidation.

## Identity and capability facts

`Binding.logicalAxisId` is a nonzero application axis identity. `Endpoint` holds
the selected protocol, physical bus, profile identity, address, channel and
subaxis separately. Generic `validateBinding` requires nonzero logical/profile
identities and generations and a recognized protocol. It imposes no address
range, CANopen state or relationship between logical identity and wire address.
An abstract proprietary address above 127 with subaxis 2 is valid common data.
The selected CANopen service owner applies its own addressed-route constraints;
the synthetic CANopen profile rejects proprietary bindings before yielding SDO
work. These are different validation boundaries.

`Capabilities` keeps motor kind, selected feedback, position/velocity/torque/brake
availability and calibration facts independent. Motor technology selects neither
transport nor command scale. `requireCapability` rejects an absent capability
with `UNSUPPORTED`, and a present capability requiring unqualified calibration
with `INVALID_CONFIG`. The position preparation calls this same check before
producing work. Position alone requires neither torque nor brake capability.
Checking or selecting facts performs no calibration, enabling or brake action.

`bindingGeneration` identifies a route/identity epoch;
`configurationGeneration` identifies its coordinate interpretation. Both are
nonzero host values. They never substitute for wire correlation, and must not
wrap or be reused while old work can remain. The application owns changes and
invalidates dependent work/evidence through its owner. The proof supplies no
configuration mutation, persistence or automatic recovery API.

## Exact coordinates and independent scales

`Rational` retains a signed I64 numerator and positive U64 denominator without a
floating-point round trip. `PositionRequest` carries the exact value, dimension,
coordinate frame, configuration generation, rounding intent and exact native
quantization allowance. Its ordinary absolute target retains all turns.

| Coordinate frame | Accepted proof input | Required interpretation |
| --- | --- | --- |
| `NATIVE` | `NATIVE_INCREMENTS` | Already in selected signed command coordinates; no origin or angular scale is consumed |
| `MOTOR` | `TURNS` or `DEGREES` | Explicit known host origin and command increments per motor turn |
| `LOAD` | `TURNS` or `DEGREES` | Motor requirements plus motor turns per load turn |

`AxisConfig` contains inclusive signed I32 native limits and a known/unknown
host origin. An origin changes coordinate interpretation, not a drive counter.
The proof's angular conversion adds the supplied `originNative` after exact
scaling. It neither wraps an angle nor infers a current-position reference.
Those I32 limits/origin describe this synthetic profile's selected native
representation, not a universal width for other profiles. Keep profile-native
bounds/conversion separate when migrating the common exact intent; a future
floating or wider native profile must supply its own checked representation.

`UnitConfig` stores command increments/motor turn, motor turns/load turn,
feedback counts/motor turn and optional full steps/motor turn independently.
Each scale carries `ScaleSource`; a zero numerator means unavailable metadata.
A used scale must be positive and have known provenance. Provenance describes
the supplied evidence class, not verification that a setting is active on a
connected drive. Unused feedback/full-step scales are not prerequisites.

The [contrast ledger](reference/drive_contrasts.md) owns documentary scope.
Its N5 one-turn/3600-unit vector fits either stepper or BLDC facts without full
steps. The abstract independent-scale vector produces
`1/2 load turn × 3/2 motor turns/load turn × 4096 increments/motor turn = 3072`
command increments; a separate 2048-count feedback scale is retained without
changing that target. An abstract EPOS4 contrast likewise needs no stepper
geometry. None of these inputs declares a native vendor sequence implemented.

`VelocityRequest` retains wide signed exact intent and explicit position/time
dimensions, including values such as ±70000. There is no proof velocity engine
or implied native RPM conversion. `NativeRamp` distinguishes milliseconds from
rpm/s; equal numeric settings do not imply equal dimensions or trajectories.

## Deliberately restricted prototype arithmetic

The conversion inside `ProofProfile::prepareMoveAbsolute` implements only
`Rounding::EXACT` with zero quantization allowance. It checks exact divisibility
and signed I32/native limits. Used input numerator magnitudes, denominators and
scale components are bounded by 1,000,000; intermediate products, including a
degree denominator factor, must fit the explicit 10^18 prototype bound. An
otherwise representable value outside those bounds is rejected. Fractional
native targets and unsupported frames/units are rejected before work exists.

This is not production exact arithmetic. Prompts 9–10 replace it with their
full checked units/coordinate contracts. Relative motion, wrapped-angle path
selection, reference/origin mutation and approximate/native floating conversion
are absent from this proof. Preparation builds a complete local candidate;
every failure leaves the caller's existing context unchanged.

Typed identity publication checks the complete current binding, including both
generations, rather than accepting a matching address alone. Its exact behavior
belongs to the [profile contract](profile_contract.md). Evidence age policy and
native CANopen observations remain separate from common coordinate identity;
see [CANopen services](canopen_services.md).
