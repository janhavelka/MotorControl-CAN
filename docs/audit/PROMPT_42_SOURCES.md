# Prompt 42 motion and action source audit

Checked 2026-11-11. This audit used local current sibling code and retained
original manufacturer artifacts. No sibling edits, device activity or firmware
qualification occurred. The [reviewed ledger](../reference/lichuan_objects.json)
retains device applicability; [motion facts](LICHUAN_MOTION_FACTS.md) and the
[CLI contract](../cli_contract.md) own their respective detailed contracts.

## Refreshed sibling seam

| Repository | Current revision/state | Adopted and rejected behavior |
| --- | --- | --- |
| MotorControl-RS | `cd27b12389f568198b72cecb6606123ea4c089bc`, clean | Console inventory unchanged since prompt 40. Inspected additional motion/action parsing and application admission/result seam below. Adopt exact requests, explicit units, distinct command/operation identities, retained terminal effects and local cancellation. Reject RS boot speed/scale/origin defaults, RTU addressing/registers, native ramp policy assumptions and stop replacement without an escalation check. |
| FieldCore-node | `5ad9292dc8481cf08be9a61f662495a6474d1da0`, clean | Earlier ownership seam unchanged since prompt 39; not reread. Inspected the explicitly requested `include/TunnelMonitor/contracts/MotorControl.h:110-154` result boundary. `MotorResult` keeps immutable matching command identity, independent admission/wire/execution, setup/trigger effects, completion, retained library status and historical uncertainty. `MotorStatus` separates ordinary and stop reservations. Adopt those distinctions; do not import product types or treat elapsed time/result take as reconciliation. This integration boundary is scoped to its RS v1.0.0 pin, not an implemented CAN integration. |

RS `examples/probe_cli/ProbeConsole.cpp::COMMANDS`, `dispatch`, `positionUnit`,
`rateUnit`, `coordinateFrame`, `relativeBasis`, `rounding`, `anglePath` and
`halfTurnTie` establish the finite grammar and exact token meanings. Simple
`moveby`/`moveto` accept only `VALUE [steps|deg|turn|mm]`; speed and ramp are
separate host choices. Advanced moves retain explicit units/frame, relative
basis, angle path/tie, rounding and setup policy. Finite velocity retains its
duration and explicit stop policy. RS accepts `rpm`, `turns/s` and each position
unit followed by `/s`; native rate dimension is independently configured.

The additional application implementation read was
`examples/probe_cli/ProbeApp.cpp::startAction`, `startMove`, `startVelocity`,
`startHome`, `lookup`, `cancel`, `release`, `admitActionStep`, `admitMoveStep`,
`admitVelocityStep`, `admitHomeStep`, and their service callers. Preparation and
bounded record capacity precede admission. Lookup preserves results; explicit
release frees terminal storage. Cancellation retires unsent work and lets an
in-flight transaction settle. Stops use reserved capacity. RS stop replacement
does not itself enforce normal-to-fast escalation direction: CAN deliberately
requires that check. No later stop result may replace a failed or uncertain
motion's retained evidence.

Removed RS aliases remain rejected: `ping`, bare `reset`, `health check`,
`profile ess_rs`, `?`, `ver`, `help advanced`, `profile ess_rs enable` and
`stop direct`. `motor ...` was never implemented grammar.

## Original device artifacts reopened

PDF pages below are physical file pages. EN86/ZH86 cover CL86-C/OL86-C without
a matched firmware revision. The EDS is a generic family artifact. PDF text was
read directly from the retained originals using PyMuPDF; the retained rendered
ZH86 page 12 was additionally inspected for its controlword/status tables.

| Claim | Primary evidence and disposition |
| --- | --- |
| Position speed and ramps | EN86/ZH86 PDF11 and EDS `[6081]`, `[6083]`, `[6084]`: U32 RW position-profile speed `6081:00`, 0..3000 rpm; ramp times `6083:00`/`6084:00`, 0..2000 ms. These are times, not physical acceleration. Zero behavior remains a native lifecycle gate; no working defaults are inferred from EDS literals. |
| Velocity target | EN86/ZH86 PDF13 distinguishes `60FF:00` from position-profile speed. Table says U32 and 0..3000 rpm, while EDS `[60FF]` is I32 RW. Preserve the signedness/range conflict and selected-firmware reverse gate. |
| Rate destination | The rpm labels require integer motor turns per minute. Default command-increments/second is a different dimension even when its number is in range. Pure `prepareVelocityTarget` gives 267 for one load turn/minute at 3200 increments/motor turn and gear 5 under that default destination; application preflight rejects it before any native write. No parser may feed a rounded number back as a replacement exact request. |
| Homing | EN86/ZH86 PDF13-15, EDS `[6098]`, `[6099sub1]`, `[6099sub2]`, `[609A]`, `[607C]`: methods 17/18 need negative/positive limits respectively; 24/29 need origin switch. Method 29 completes on leaving the origin switch and is not merely sign-reversed method 24. Search/creep rates are rpm; `609A` is milliseconds. Input assignment/polarity, actual fixture, mode/start/stop and offset meaning remain evidence gates. |
| Counter clear and host origin | EN86/ZH86 PDF15 and EDS `[2302]`: U16 RW zero-only clear on rising 0-to-1, manually return to 0. This neither defines a host origin nor arbitrary position assignment/homed status. `axis origin` remains a host-only coordinate operation. |
| Hidden save during zeroing | EN86/ZH86 PDF15 makes `2300:00=2` a separate save. Pinned public example `docs/reference/public_code/srt/SRT_OpenCan.cpp::set_motor_pos_zero` (upstream `30ec0a617fdbbcb6d17b42edc85c5210344f6796`, lines 166-175) unconditionally saves after clear despite its optional-save comment. Reject that behavior; a clear or origin route never saves implicitly. |
| Release D-17 | EN86/ZH86 PDF12 only associates `6040=0` with three low status bits. No release command or torque/reference contract follows. `motor-release` remains visible as gated capability, absent from dispatch; no success stub or invented `prepareRelease` is justified. |
| Stop/enable/status contradictions | ZH86 PDF12 gives `15->271` stop and `15->11` emergency-stop candidates; EN86 omits the former. The arrows do not authorize an enable preamble. EN86 bit7/bit10 status differs from ZH86 bit3/bit14. PDF13 velocity start order also differs. Keep explicit selected-firmware evidence; parsing cannot choose a dialect, encoding or enable sequence. |

The rechecked SHA256 values are:

```text
86C_CANopen_user_manual_en.pdf
401df6c49c54fe56fed7c606ca45718f352235746e2faaeb65be778eef6c4b9f
86C_CANopen_user_manual_zh.pdf
3969c914aa291662e8c77941bcfbb0559769a96b75306266370890d03e824d13
LC_StepDriverCAN_V1.1.eds
dd3c96e7282c012a522dddb5bbdbbf561c1f5c23716b31f12472f45829d4f52c
```

## Existing public mapping and host intent

The common request types are `MoveRequest{position, speed}` and
`FiniteVelocityRequest{velocity, durationUs}`. Native `HomeRequest` contains
method, search/creep engineering rates, ramp milliseconds and native offset.
The existing `AxisSession` request methods call `prepareMoveAbsolute`,
`prepareMoveRelative`, `prepareMoveAngle`, `prepareVelocity`, `prepareHome`,
`prepareEnable`, `prepareStop`, `prepareClearAlarm` and
`prepareSetDevicePosition`. `ProbeApp::openAxis` borrows the caller's existing
concrete session and reservation; a second CLI state machine is unnecessary.
Finite velocity duration begins at successful preparation, including queue/setup
time. Actual operation contexts retain the final rpm/error evidence.

The application-only [MotionIntent](../../examples/common/MotionIntent.h)
retains exact host speed and known ramp flags without boot defaults. Its pure
builder requires declared command scale, all host choices, current configuration
and the selected supported relative basis. Simple absolute moves additionally
require explicit `axis origin`. Setters accept documentary zero as inspectable
intent, while a move rejects zero speed or unqualified zero-ramp behavior.
An explicit `stepsperturn` declaration constructs an ASSUMED host scale; it
does not prove the drive's active geometry or change device settings.

CAN's actual `MoveSetup` has `WRITE_ALL` and `REUSE_STORED` only. The latter
checks actual prior values and writes differences. Therefore `motion write`
maps to `WRITE_ALL` and `motion verify` to `REUSE_STORED`; RS stored-only
semantics are unavailable and `motion stored` must remain explicitly gated.
Silently aliasing stored-only intent to a policy that can write is incorrect.

The module's focused native test covers atomic failures, explicit choices,
scale/ramp boundaries, stale generations, unsupported bases, the in-range 267
dimension error, correct/wrong native time dimensions, signed boundaries,
nonwrapping duration and the exact 5/2 rpm nearest-even result with -1/2 rpm
error. This is host arithmetic/preflight evidence, not operation or hardware
qualification. The stage report records combined application/platform gates.

Fresh integrated review also reopened RS `ProbeConsole.cpp` lines1690-1704 and
`SimpleMotion.h::simplePositionRequest`. Adopted simple CLI semantics: relative
steps are NATIVE; absolute steps and angles are MOTOR; millimetres are LOAD.
Nearest command increment uses ties-to-even with exact half-increment tolerance.
Programmatic `moveValue` is a different RS seam and is not the CLI framing oracle.
The first CAN parser draft selected native absolute steps/load angles and exact
rounding; this was corrected before delivery with nonzero-origin/gearing/half-tie
regressions in `motion_intent_test` and actual console vectors. No RS boot origin,
scale, speed or ramp defaults were adopted.
