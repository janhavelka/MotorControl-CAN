# Lichuan homing and reference evidence

The native speed, ramp and position units, their whole-rpm and ramp-time limits and the measured delivered rate are
explained in [Speed, ramp and position units](speed_units.md).

Homing is conditional on evidence for the selected drive, firmware and physical
fixture. The original CL86-C/OL86-C manuals document four methods; they do not
qualify a connected specimen, its switch wiring or physical reference accuracy.
The [reviewed ledger](reference/lichuan_review.md) owns the device claims and
open conflicts. The [axis contract](axis_contract.md) owns host coordinates.
Method numbers belong to the Lichuan profile, not common motion or motor kind.

## Methods and signals

The following sequences were checked against the original [EN86](reference/vendor/86C_CANopen_user_manual_en.pdf)
and [ZH86](reference/vendor/86C_CANopen_user_manual_zh.pdf), PDF pages 13–15.
Positive and negative name the manual's native directions. Their physical
direction, including the effect of `2200`, requires fixture evidence.

| Method | Required assigned input | Documented sequence and terminal switch |
| --- | --- | --- |
| 17, negative limit | Function 3, negative limit | Search negative; stop on the limit; withdraw positive at search speed for an unspecified distance; stop; creep negative to the limit; stop and complete with the limit active. |
| 18, positive limit | Function 2, positive limit | Search positive; stop on the limit; withdraw negative at search speed for an unspecified distance; stop; creep positive to the limit; stop and complete with the limit active. |
| 24, positive origin | Function 1, origin | Search positive; stop on origin; withdraw negative at search speed for an unspecified distance; stop; creep positive to origin; stop and complete with origin active. |
| 29, negative origin | Function 1, origin | Search negative; stop on origin; creep positive until **leaving** origin; stop and complete with origin inactive. |

Method 29 is not a reversed method 24. No retained source supplies a numeric
withdrawal distance, a maximum search time, an encoder-index method or a
current-position homing method. Starting with the required switch active and
meeting a limit during an origin search have no documented behavior. Those
conditions need separate evidence before admission.

Read `2400:01..08` and `2101` before admission. `2400:01` bits 0..6 select the
default or reversed level of X0..X6; `2400:02..08` assign their functions.
`2101` bits 0..6 report the corresponding **effective** inputs: 1 effective,
0 ineffective (EN86 p17, ZH86 p18). Do not invert these logical flags again.
Requiring function 3 for method 17, 2 for 18 and 1 for 24/29 is host policy
inferred from the matching signal names, not an explicit manufacturer rule.
Only the selected method's switch is required. Configured stop, emergency-stop
and other protective inputs still matter even when a terminal is unconnected.
The operation never reassigns them. `60FD` has no documented layout and cannot
replace `2101`.

## Parameters and transition

| Object | Wire representation and documentary range | Admission boundary |
| --- | --- | --- |
| `6098:00` | One-byte method; EDS U8, dictionary I8 | Only 17, 18, 24, 29; positive bytes agree despite the signedness conflict. |
| `6099:01/:02` | U32 search/creep motor rpm, 0..3000 | Positive integer rpm with evidenced rate basis; zero motion values refused until qualified. |
| `609A:00` | U32 acceleration/deceleration time, 0..2000 ms | Positive time; zero semantics remain Q11 gated. |
| `607C:00` | I32 home offset, -1000000..1000000 | Exact native value with separately evidenced units, sign, application point and write order. |

The p13 table calls `6099:02` creep "time" while retaining rpm; the method
steps describe creep speed. The retained [EDS](reference/vendor/LC_StepDriverCAN_V1.1.eds)
confirms the widths above. It supplies metadata, not accepted device values or
read-side-effect qualification. General acquisition of these objects and the
`2400` input configuration remains Q20 gated; the accepted `2101` observation
does not open those other reads.

Both rpm conversions must retain `MOTOR_TURNS`, `MINUTE` and integer
representation. A command-increment or per-second `effectiveNative` can be
numerically in range and still be the wrong rpm. Nonzero intent that rounds to
zero is rejected. Both engineering intent and converted native rates must be
positive: a negative intent cannot cancel reversed coordinate polarity. Use
explicit positive native motor rpm when a mechanical-frame conversion reverses
the sign. The public-code [SRT homing example](reference/public_code/srt/SRT_OpenCan.cpp)
at lines 146–152 writes the wrong two-byte widths and a 5000 ms ramp; it is
not an alternate supported sequence. `6040` itself remains two bytes.

Enabling in mode 6 is an explicit, separately gated action. Homing preparation
never enables implicitly. It requires fresh enabled mode-6 state and the latest
acknowledged `6040=000F`. Admission also requires stationary reports from the
selected statusword dialect, `2100` and `606C`. A different observed mode while
enabled is refused before transmission. The p15 recipe requests mode 6, observes
mode 6, enables with `6,7,15` at approximately 10 ms spacing, writes `6098`, `6099:01/:02` and
`609A`, then raises bit 4. It establishes no path for changing mode while enabled.
In particular, "home, then move" still needs Q11 transition evidence.

`607C` appears in the p13 table but not that procedural write list. Its placement
in an executable setup sequence remains closed until a specimen record or
manufacturer clarification establishes its effect. No command-pulse unit or
final-coordinate formula is inferred from another object. Homing setup writes
belong to the home operation; they retire retained `MOTION_PARAMETERS` reads
without advancing the configuration generation.

The start edge is bit 4 rising. The host word `001F` is inferred from `000F`
plus bit 4; the homing page documents the edge rather than that complete word.
Hold bit 4 high during the home. Falling bit 4 interrupts homing (EN86 p15
"Stopping homing", ZH86 p15 interrupt); a position-style immediate `001F`
then `000F` rearm would interrupt it. Bits 5 and 6 have no mode-6 meaning in
these sources. The explicit mode-6 stop row uses the acknowledged `001F` then
`000F`, with `predecessorDocumented=false`. No enabling preamble is part of stop.
An urgent unresolved-route `000B` has only the separate [stop-policy](stop_actions.md)
meaning and cannot qualify completion.

## Completion, uncertainty and reference

Both original p15 tables call statusword bit 8 homing complete. Their motion
status differs: EN86 bit 10 means velocity zero when set; ZH86 bit 14 means
running when set. Preserve that conflict and the selected evidenced dialect.
An SDO reply proves neither a successful home nor physical standstill.

New reference publication requires completion correlated with the method,
operation, binding and configuration. A fresh post-trigger bit-8 low-to-high
transition, an observed `2100` running report and the selected method's switch
sequence are conservative host qualification requirements; the manuals do not
specify bit-reset timing or promise that polling observes every transition.
The completion cycle must also report `2100` homing complete, running clear and
zero raw `606C`, with the selected statusword dialect reporting stationary:
EN86 bit 10 set, or ZH86/OP86 bit 14 clear. A contradictory running report cannot
qualify completion or publish a reference; polling continues within its original
bound. Intermediate running cycles may contain disagreement from sequential
sampling and are not rejected for that alone. Disagreeing completion reports
do not publish a reference.
Method 29 needs an observed active-to-inactive origin transition. That
observation alone cannot prove the
documented positive creep direction. Previously latched completion, an ACK,
an elapsed delay or a late sample cannot establish a new reference.

[`prepareHome`, `nextHome` and `advanceHome`](../include/MotorControlCAN/profiles/lichuan_c/Homing.h)
use caller-owned `HomeContext`; `getHomeReference` is a checked getter.
`HomeEvidence` owns the exact selected method, identity, input mapping/polarity,
raw direction and qualified native parameters. A successful home reference, a host
origin assignment and device-counter clear are distinct evidence. Reference
publication must consume only the one expected coordinate-generation advance;
unexpected binding/configuration changes, boot-up, loss of coordinates, a
direction change or counter exhaustion reject publication. A later operation
must refuse an invalidated home reference. Host tests establish these software
rules, not physical repeatability or absolute accuracy.

Retain the first home failure separately from the first stop failure. A timeout,
fault, impossible input state, restart, cancellation or unconfirmed write cannot
be repaired by a later stale completion. Processing at `now >= deadlineUs`
expires even if a response arrived earlier. A lost start/stop reply preserves
possible execution. Local cancellation is not a motor stop; recovery never
replays or resumes home and cannot erase retained uncertainty.

After controller loss advances the node epoch, a later failed best-effort stop
on the same binding and SDO route remains a separate retained failure. An older
epoch or a changed binding or route is rejected. Recording the newer failure
does not qualify channel reuse or a stopped state, clear `needsStop`, or repair
the home outcome.

## Remaining specimen evidence

No physical homing measurement was made by the prompt-36 source review, and no
homing motion has run since; the admission reads of 2026-11-09 are recorded
[below](#bench-observations-2026-11-09). The
conditional prompt-29 acquisition route or manufacturer clarification must
establish each dependent fact before that feature is qualified:

1. Record exact specimen/firmware, motor, board/transceiver, node/bitrate, wiring,
   polarity and input assignments with the actual independent cutoff provisions.
2. Establish Q20 non-consuming acquisition for the home and input-configuration
   objects. Capture raw `2200` in the configuration generation and measure its
   effect on search/creep direction under Q04.
3. Establish the separate mode-6 enable/start transition and interrupt/settlement
   behavior under Q11/Q21, including the admitted initial switch state. Do not
   infer a release command to obtain a non-enabled starting state.
4. Determine Q14 offset units, sign, write order, affected counters and final
   coordinate relation. Observe fresh completion and method-specific switches
   together with independent shaft/reference evidence under Q21.
5. Retain bounded raw traffic, operation/step identities, times, service gaps and
   first home/stop failures. Qualify only the measured method, rates, direction,
   offset and exact firmware scope; do not replay an uncertain write.

The [contrast ledger](reference/drive_contrasts.md) records why supported method
numbers cannot be shared with EPOS4. It creates no additional installed profile.

### Bench observations, 2026-11-09

The S2 bench `home` route reads what `requestHome` consumes and, with a CLOSED
`HomeEvidence`, ends at the library's refusal before any write. Its 29 admission
reads completed four times on the two CL86-C specimens, one firmware tuple, nothing
written and nothing moved (trials 913, 915, 1107, 1108; trials 903 to 906 stopped
after their identity reads, refused `why=grant` by the bench ordering of backlog S15)
([round 2](reports/2026-11-09_bench_r2.md), [round 4](reports/2026-11-09_bench_r4.md)).

- **Measured, both specimens, all four runs:** `2400:00` raw 16, `2400:01` polarity 0,
  `2400:02..04` (DI0..DI2) functions 1, 2, 3, `2400:05..08` (DI3..DI6) 0, `2101=0`
  (no input effective), `2200=0`, `2201=1000`. Functions 1, 2, 3 are origin, positive
  limit and negative limit by the manuals' labels. No switch is known to be wired:
  an assigned function says nothing about wiring, and an unconnected input is not a
  disabled function. Nothing about direction, offset, mode 6 or completion is measured.
- **Refusals measured:** without a controlword in that MCU boot the session refused
  first, `code=1 detail=3` ("home needs the latest controlword and later state";
  node 1 method 24 and node 2 method 17, image `757f1cd`). After an attended enable,
  with `000F` the latest word, both nodes reached the library and were refused at its
  CLOSED gate, `code=3 detail=7` ("homing needs exact selected-firmware qualification";
  node 1 method 24 and node 2 method 17, image `627bb42`). Zero downloads in all four.
- **Qualified:** nothing. The gate stays closed; items 1 to 5 above remain.

`bench_route` replays all four records from their captured replies, value for value
(`replayRouteBootR2c`, `replayRouteBootR4b`).

## Ordinary owner path

`AxisSession::requestHome` validates the acquired identity, input configuration,
raw direction, current enabled mode-6 state and actual latest acknowledged
`000F`, then prepares the fixed `HomeContext` in place. It neither changes mode
nor enables. Supply mutable application-owned axis/reference storage that
outlives the home, exact `HomeEvidence` and finite service/poll/deadline budgets.
Known stale reference storage is refused before any setup write or owner
operation is admitted.

Call the sole owner's service and then `AxisSession::service` regularly. The
session yields each parameter through that same owner's ordinary queue and
consumes its actual terminal before proceeding. It shares the existing two
motion-observation buffers: `homeObservation()` retains the last complete cycle
while the other receives the next one. Heartbeat, PDO and EMCY dispatch continue
through the ordinary receiver. The transport's `beforeTransmit` calls session
`preflight` so a changed configuration or routed fault can refuse a queued
download before controller handover.

After all five setup downloads, `nextHome` first requests a local trigger
boundary. `beginHomeTrigger` invalidates coordinates once, clears the old
witness and advances the axis/configuration epochs. The session then calls
`CanBusOwner::advanceConfiguration` before rebuilding and admitting `001F`.
That actual trigger, its acknowledgement, subsequent polls and a later confirmed
stop carry the new configuration binding. The home's frozen reservation retains
the original binding to check exactly one expected advance. No terminal is
rebased or manufactured, and a locally cancelled unsent trigger still leaves
the old coordinates withdrawn.

`homeAction()` retains setup/start/completion and independent first home/stop
failures. `cancelHome` ends local work and sends no stop. To interrupt motion,
call `requestStop(HALT, homeObservation(), policy, now)` explicitly; the mode-6
path emits the falling bit-4 edge and retains its own observation. An explicitly
requested best-effort quick stop on an unresolved route remains unconfirmed.
Possible motion keeps `homeNeedsStop()` set across terminal release and reopen;
only a later checked stop observation can clear the session's admission fence.
Neither that stop nor recovery changes a failed home into a successful home or
publishes its reference.

The exact `parametersQualified` record must establish that all five setup
writes, including `607C`, leave command coordinates unchanged until the trigger.
Firmware that applies the offset immediately cannot use this sequence. This
write-order/application fact remains deferred with Q14; a documentary offset
range alone cannot open it. The record separately establishes final raw `6064`
and its exact command-coordinate relation before `getHomeReference` may publish
an actual-position witness. Publication leaves the host mechanical/encoder
origins unset.
