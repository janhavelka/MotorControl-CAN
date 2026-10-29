# Axis coordinates and reference evidence

[Axis.h](../include/MotorControlCAN/Axis.h) implements pure host configuration,
coordinate conversion and target preparation. These functions acquire no time,
perform no device I/O, and establish neither motion permission nor completion.
The caller owns configuration, qualified evidence, time and serialization.
[Units](units_status.md) owns reusable arithmetic; [architecture](architecture.md)
and the [owner contract](can_owner.md) own integration boundaries.

## Identity, scales and origins

`Binding.logicalAxisId` is independent of protocol, bus, profile, address,
channel and subaxis. Common validation requires the documented nonzero identity
and generation fields, without imposing a CANopen node range. Protocol owners
subsequently validate their routes. The offline example uses address 500 and
subaxis 256 as abstract common-contract inputs.

`AxisConfig.units` retains independent command increments per motor turn,
feedback counts per selected encoder basis, motor/load gearing, optional full
steps and configured linear travel. Encoder identity and polarity are explicit.
Missing optional scale is unknown evidence, represented by `0/1` with `UNKNOWN`
provenance. It is legal until a conversion needs it; malformed supplied facts
fail validation. A servo needs no full-step or subdivision geometry. `ASSUMED`
scales are usable caller assumptions, without becoming qualified device facts.

| Frame | Coordinate interpretation |
| --- | --- |
| `NATIVE` | Only `STEPS`, meaning command increments; no host origin or polarity adjustment |
| `MOTOR` | Angles refer to motor turns and need no load gear; millimetres are invalid |
| `LOAD` | Angles refer to load turns; linear travel consumes explicit millimetres per load turn |

Command increments, full steps and feedback counts retain their own explicit
scale/basis semantics in mechanical frames. Non-native command increments follow
command polarity; selected encoder counts follow encoder polarity and basis.

Absolute mechanical targets add the host `originNative`. Absolute encoder-count
targets instead require the independent `encoderOriginNative`: knowing the
mechanical origin does not establish the selected encoder's zero. Relative
displacements consume neither origin. `setAxisOrigin` selects `MECHANICAL` or
`ENCODER`, requires stationary idle native-coordinate evidence, preserves the
other zero, clears soft limits and advances configuration/reference epochs. It
changes no drive counter. Encoder-origin establishment also requires a selected
encoder scale and source identity.

## Checked preparation and quantization

`preparePosition` validates the complete candidate before publishing output.
Ordinary absolute targets retain all turns. Relative requests always require the
chosen `ACTUAL`, `COMMANDED` or `QUEUED` reference, admitted by the configuration's
`supportedRelativeBases` policy; an unavailable basis never falls back to another.
Absolute unwrapped requests may omit a witness with `ReferencePolicy::OPTIONAL`.
`REQUIRED` explicitly demands one. A supplied witness is validated even when it
was optional.

Exact arithmetic uses `Rational` and mixed `NativeQuantity` storage. The latter
retains an integral part and signed proper fraction, avoiding a large
`origin * denominator` product. The Units conversion, addition and quantization
functions check fixed-storage overflow. Exact integers beyond `2^53` remain
distinct because integer conversion does not pass through binary floating point.

Absolute targets add origin before quantization. Relative targets quantize the
displacement before adding the selected integer basis. Both the requested exact
endpoint and effective endpoint must satisfy inclusive native and soft limits;
rounding cannot rescue an out-of-bounds request. Limits constrain absolute
coordinates, so a negative displacement is legal when its start and endpoint
are in range. A supplied starting position must also satisfy those limits.

`INTEGER` produces a checked signed `int64_t`. Selected rounding is exact,
nearest with ties to even, toward zero, floor or ceiling. The caller supplies a
nonnegative exact native-unit error allowance; `quantization` preserves the exact
signed error. `BINARY32` uses nearest-even conversion with an exact error check
and upward absolute error bound. Its active output is `effectiveFloat`;
`endpointFloat` is display data and cannot replace retained exact evidence.
Profiles later check their own wire width and permissions.

Nonzero radians require explicit approximation and a positive finite allowance.
The approximation assumes IEEE round-to-nearest without changing the floating
environment. Outward intervals cover conversion, origin/basis arithmetic and
quantization; a midpoint cannot settle a boundary decision. The interval must
fit the configured allowances and binary64's exact-integer range `[-2^53, 2^53]`.
An interval crossing a limit or rounding boundary rejects. Exact zero radians
uses exact arithmetic after the selected scale requirements are validated.

Wrapped orientation is an absolute motor/load angle with a fresh selected-basis
reference and known mechanical origin. `POSITIVE`, `NEGATIVE` and `SHORTEST`
select physical direction before command polarity. Equal orientation stays
still. Exact half-turn ties reject unless explicitly directed positive or
negative. Approximation must resolve the direction; a boundary-crossing interval
rejects. Selected paths fail at limits instead of choosing another turn, and
quantization cannot reverse the selected direction. These are target primitives;
angle motion sequences remain later work.

## Reference lifetime and configuration changes

An `AxisReference` carries the complete matching binding, configuration and
reference epochs, chosen basis, qualified command-coordinate relation and
original observation time. Microseconds belong to one monotonic application
domain, `0..INT64_MAX`, without wrapping. Future observations reject. A nonzero
`maximumAgeUs` is an inclusive maximum age; zero disables expiry while future
checks remain active. Only a new qualified position observation advances
`observedUs`. Unrelated status observations cannot renew position evidence.

`configureAxis` validates current and candidate configuration plus idle stationary
evidence before advancing epochs. The candidate must retain the current binding,
epochs and origin fields; route rebinding belongs to the owner. Position-scale,
polarity, encoder identity/basis or position-format changes clear origins and
soft limits. Unchanged position interpretation can return retained evidence with
updated epochs and its original observation time and age policy. New or changed
soft limits additionally require a known native-position relation.

`validatePreparedTarget` rechecks frozen binding/configuration and any consumed
reference before admission. Admission time cannot precede that witness's supplied
preparation/current time. Replacing its observation, basis, native position,
provenance or age policy requires preparation again. Advancing `nowUs` performs
freshness checking and does not renew `observedUs`. Reference age is application
policy, separate from later post-trigger completion evidence.

| Explicit loss | Invalidation and retained dependencies |
| --- | --- |
| `POSITION` | Uncertain movement or release revokes the position witness and advances its epoch; origins and soft limits remain. Reference-free absolute targets retain their valid configuration. |
| `COORDINATES` | Reboot, rebinding, changed dialect/mapping/scale or device counter clear revokes the witness, both origins and soft limits, and advances configuration too. All earlier prepared targets become stale. |

The application selects the loss justified by actual evidence; no event is
inferred automatically. Normal fallible functions leave all outputs unchanged.
`invalidateAxisReference` is the explicit exception: exhaustion still clears
the supplied witness and latches `exhausted`, returning an error. Generations
never wrap or reuse zero. Once exhausted, retire the binding and old work before
constructing fresh provenance; clearing the latch is not recovery. Configuration
and origin changes otherwise reject exhausted generations atomically.

## Signed velocity and an offline example

`prepareVelocityTarget` consumes exact signed velocity, including zero, and
produces command increments per configured `nativeVelocityTime`. Native frame
requires exactly that spatial/time dimension. Mechanical frames use the same
independent scale facts; no position origin, witness or position limits are
consumed. Integer output is wide signed arithmetic, including values beyond
ESS's 16-bit RPM fields. Binary32 uses the same explicit quantization policy.
A profile later converts to its documented native representation; zero here is
arithmetic and does not issue a stop. Dimensional acceleration and millisecond
ramp parameters remain separate quantities.

The [compiled offline preview](../examples/units_preview/README.md) supplies
4096 command increments/motor turn, gearing `3/2`, 20 mm/load turn, independent
2048 encoder counts/motor turn, and host origin zero, all explicitly assumed:

`5 mm / 20 mm per load turn * 3/2 motor turns per load turn * 4096 = 1536`

The corresponding illustrative feedback displacement is 768 counts. The example
also shows native binary32 `1/10` with a declared error tolerance and signed
velocity `-70000`, without opening any controller. These values are abstract
common invariants, not installed N5/EPOS4/proprietary profile support. The
[contrast ledger](reference/drive_contrasts.md) retains vendor applicability.

## Device evidence and the retained proof

The original Lichuan documents distinguish `0x2201` subdivision, `0x607A`
commanded position, `0x6064` observed position, position deviation and a 1000-line
encoder description. They establish no interchangeable command/feedback scale,
quadrature multiplier or qualified live counts/turn. EDS defaults are metadata,
not live scale. `0x2302` is a triggered device counter-clear action, separately
documented as a `0 -> 1` edge with manual return to zero; it cannot implement
arbitrary host origin setting or prove homing. See the source/page details in
[motion evidence](audit/LICHUAN_MOTION_FACTS.md).

The retained [ownership proof](../experiments/api_ownership/Contracts.h) uses
production `AxisConfig`, requests and `preparePosition`. Its `ProofAxisConfig`
adds synthetic capability facts and I32 profile limits. Synthetic
`prepareMoveAbsolute` continues to prove admission/ownership below that boundary;
it is not installed drive support or a production finite motion implementation.
Public motion sequence vocabulary and ownership remain in the
[profile contract](profile_contract.md).
