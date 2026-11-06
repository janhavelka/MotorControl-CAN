# Lichuan native driver settings

The installed `MotorControlCAN::LICHUAN_C` API in
`MotorControlCAN/profiles/lichuan_c/DriverSettings.h` implements three finite
typed driver **read** groups. The same `DriverContext` and lifecycle also serve
four [I/O groups](io_settings.md) through checked `getIo` and five
[control/tuning groups](tuning.md) through `getControl/getTuning`.
[Prompt 20](reports/prompt_20.md) remains Partial. Explicit position parameters
and the 2202 selector have typed writes. `2200`, subdivision, `607D` and mode
selection keep the gates listed under Setting decisions.
See [the fresh audit](reports/prompts_20_21_audit.md) for the correction to the
earlier blanket write blocker. No generic integer-write substitute is provided.

## Frozen row ownership and read coverage

Every row below is documented, implemented and host tested as a raw read, with
hardware qualification **false**. The canonical [ledger](reference/lichuan_objects.md)
retains exact EDS lines, PDF pages, model scope and separate whole-object/write
coverage. All read effects remain `UNQUALIFIED`; these are ordinary reads on an
already usable channel, not a non-consuming bootstrap probe.

| Group / object | Native observation | Original evidence | Write gate |
| --- | --- | --- | --- |
| DRIVER `2200:00` | U16 `directionRaw` | EDS 1274–1282; EN86/ZH86 p18 | No typed write, see Setting decisions. Q04: labels do not establish physical positive direction/CAN effect. |
| DRIVER `2201:00` | U16 `subdivisionRaw` | EDS 1283–1291; EN86/ZH86 p11/p18 | Q04: default 0 and 0–65535 versus 400–51200 conflict; scale/activation unknown. |
| SOFT_LIMITS `2202:00` | U16 `softLimitSelectorRaw` | EDS 1292–1300; EN86/ZH86 p18 | Typed write of codes 0 and 1 below. Q05: default 10 stays raw; homing and enforcement unqualified. |
| SOFT_LIMITS `607D:00` | EDS U8, raw `softLimitEntries` with `softLimitEntriesWidth` | EDS 1918–1926; two-byte 0 measured on both specimens, see `CountWidths` in [reads](reads.md) | RO count, never a setter or discovery bound. |
| SOFT_LIMITS `607D:01/:02` | I32 `minimumNative/maximumNative` | EDS 1927–1942; EN86 p16/p17, ZH86 p17 | Q05: coordinate basis, operating range, activation and enforcement unresolved. |
| MOTION_PARAMETERS `6081:00` | U32 `positionSpeedRpm` | EDS 1943–1951; EN86/ZH86 p11/p17 | Explicit native 0..3000 rpm settings implemented under the reserved position-mode contract below; no speed-zero stop claim. |
| MOTION_PARAMETERS `6083:00` | U32 `rampUpTimeMs` | EDS 1952–1960; EN86/ZH86 p11/p13 | Explicit native 0..2000 ms settings implemented under the contract below; physical zero behavior remains unqualified. |
| MOTION_PARAMETERS `6084:00` | U32 `rampDownTimeMs` | EDS 1961–1969; EN86/ZH86 p11/p13 | Same guarded setting path; milliseconds are not an acceleration rate or stop guarantee. |

Mode request `6060:00` remains write-owned by 20 and effect-gated by Q10/Q11;
the EDS is WO, so it is never uploaded. Current/holding rows `2502..2507`
remain **solely in prompt 22**, alongside loop/motor-rating prerequisites. Their
source units are peak mA, percentages and ms; source RW is distinct from the
reviewed software read-only restriction. No inferred 0–100 percentage range or
base-versus-maximum relationship is introduced here.

## Ordinary lifecycle

`prepareDriverRead` copies the selected `DriverGroup` into a caller-owned
`DriverContext`; `nextDriver` yields one owned upload, and `advanceDriver`
consumes the actual owner's retained matching `SdoCompletion`. Inspect
`context.read.state/outcome/status/evidence/uncertain` for terminal history.
The wrappers reuse the [existing read lifecycle](reads.md), including unsigned
abort evidence, immutable deadline, local termination and invalidation precedence.
Repeated `nextDriver` calls do not authorize repeated admission or retry.

`getDriver` requires successful completion of the whole group, the actual current
binding/configuration/node epochs, supplied monotonic time and nonzero inclusive
maximum age. It publishes one `DriverObservation` atomically; wrong kind,
incomplete/stale evidence or invalidation leaves output unchanged. The group
selects meaningful fields; `fieldCount`, per-field provenance and `window` expose
acquisition age/skew. This is not an atomic device snapshot. Time zero is valid.

Unknown selectors, count values and numeric extremes remain raw observations.
Subdivision zero is not a denominator; selector 10 is not converted to true;
reversed limits are not a valid write candidate. Reads of `607D` use only the
three reviewed subentries, regardless of the returned count. A native limit
cannot be assigned motor/load coordinates without independent scale evidence.
Generic velocity and acceleration types retain their separate dimensions.

The [compiled offline caller](../examples/driver_settings_preview/README.md)
shows this lifecycle without CLI, controller or SDK. The registered
`test/driver_settings_test.cpp` exercises all nine rows through a fake actual
service owner and checks negative/partial failure paths. Isolated installed and
exported consumers use the same public API without ledger, Python, vendor PDFs,
catalogue strings or CANopenNode. Intended CLI ownership is the future typed
device settings route in prompts 40–42; no CLI command exists here.

## Explicit position-parameter settings

`MotorControlCAN/profiles/lichuan_c/Settings.h` implements `DriverSettings`,
`DriverSettingsContext` and `prepareDriverSettings`. The entire candidate and
actual retained prerequisites are checked before yielding any work:

- An explicit caller-established CL86-C or OL86-C model; opaque profile IDs and
  raw `2000` values do not establish that model or firmware qualification.
- A continuous live `AxisReservation` issued by the sole application owner
  before acquisition. The owner excludes other control, mode, target, RPDO and
  settings work, and revokes it on contradictory observations or lifecycle loss.
  Pass the actual live reservation on every call, not a stale retained copy.
- Actual matching acknowledged `6040:00=0x000F` U16 download evidence, followed
  by fresh checked STATUS/MONITORS and native parameter reads on the same epochs.
  Require mode 1, status low bits 7, and monitor running/alarm/disabled bits clear.
  Only these independently agreed source bits are used; no full dialect is guessed.
- Complete native bounds: 6081 0..3000 rpm, 6083/6084 0..2000 ms, service budgets, inclusive evidence age and
  a new nonwrapping configuration generation. No SDK or hidden clock is involved.

EN86/ZH86 PDF p11 give native ranges; p12 separates these parameter writes
from a later controlword bit-4 trigger. Missing retained-control plumbing was a
software gap, not proof that all writes lacked manufacturer support. This API
never emits `6040`, `6060`, a target/trigger, enabling preamble, save or reset.
Zero is accepted as a documented register value. Speed zero is not a stop
command; ramp zero promises no instantaneous acceleration or trajectory.
Candidates default to invalid values until all fields are explicitly selected.

`positionSpeedRpm` is a native rpm register value. A common rate reaches it only
through `prepareVelocityTarget` with `MOTOR_TURNS` and `MINUTE`. A
`COMMAND_INCREMENTS` value may fall inside 0..3000 and still be wrong, and nothing
here can detect that, so this call does not claim to.

The state read must have ended cleanly: a failed or uncertain read is refused even
when its STATUS and MONITORS blocks completed, as a stop refuses it. Those blocks
stay readable as history. A settings call is never urgent, so the cost is one fresh
read rather than an admission from a window with a known service problem.

Use overloaded `nextDriver` and `advanceDriver` with the current live reservation
and actual binding. The sequence writes only changed fields in fixed source
order, awaiting each matching acknowledgement before the next field. It retains
`confirmed`, exact `requested` values, acknowledgement times and the last raw
terminal/abort. Partial failure is not rollback. A no-change candidate succeeds
from fresh prior evidence with no TX or generation change.

After queue admission and application preflight, call `noteDriverTransmission`
**before** `SdoClient::nextTx` or another possible controller handoff. It advances
the actual application/axis binding configuration generation exactly once;
old snapshots and prepared targets then fail their ordinary epoch checks. It
must not alias context/reservation storage. Retire separately admitted work before
local cancel/deadline/invalidation. Cancellation is not motor stop. Unknown
write or unqualified read effects remain retained and never replay automatically.

An explicit settings call advances configuration confidence this way. A move's
own `607A`, `6081`, `6083` and `6084` downloads do not: they are phases of that
move, do not pass through these calls and cannot invalidate its prepared target.
They do make a retained `MOTION_PARAMETERS` observation untrustworthy, because a
later no-change candidate would otherwise succeed with no TX while the drive
holds the move's value. So the move owner calls
`retireMotionParameters(retained, current, work)`, once per retained context and
per own download, after queue admission and preflight and before any possible
transmission. This includes a finished settings context's `readback`. Do not
transmit if it refuses. After that, `getDriver` and `prepareDriverSettings` refuse
the context until a fresh read replaces it; its raw values stay as history. A
copy the owner did not pass is unreachable, so keep one retained context per
axis. It has no caller until the move lifecycle arrives in packet 20, and it does
not prove that the write happened. `607C` has no ruling yet and is refused.

The exclusive transaction alone may finish its copied old-epoch downloads.
After releasing all old results, the application explicitly calls
`SdoClient::advanceConfiguration` to update the **idle, healthy same route** to
the new configuration epoch. It cannot reopen quarantine, change identifiers,
rebind a node or renew any timestamp. Cache observers and PDO qualification are
withdrawn without inventing a reboot. Readback then uses the actual new epoch.
Every returned native word must match the full candidate before `getDriver`
publishes a current typed result. Wrong kind, incomplete/failing/mismatched,
stale or revoked work leaves getter output unchanged.

The real fake-controller caller in `test/settings_write_test.cpp` issues and
revokes reservations around the existing SDO owner, acquires prerequisites,
checks literal downloads, releases results, advances the configuration epoch
and consumes matched readback. It also tests partial effects, uncertainty,
late acknowledgement/deadline distinctions and old prepared-target rejection.
This is native software evidence, not an installed enable/motion capability.
`test/settings_owner_test.cpp` runs the same calls through the real `CanBusOwner`
and `SdoClient` against a scripted dictionary holding the values the two specimens
reported. Its answers are scripted, so it establishes the library's behavior only.

A finished transaction keeps its frozen `AxisReservation`. Every `prepare*` settings
call therefore takes the actual current `ReadTarget` beside the reservation, and
refuses with `ReadError::TARGET`, before publishing anything, when the two differ. A
token left from a finished transaction or a lost reply cannot report an unchanged
candidate as success against a drive that may hold another value. Construct a new
value holding the same owner facts and the current target, take fresh evidence and
prepare again. A plan prepared while current still goes stale when the generation
moves: `nextDriver`, `noteDriverTransmission` and `getDriver` refuse it, so read a
result through `getDriver`, not `state`.

## The 2202 selector

`prepareSoftLimitSettings` writes this one selector. The other group fields are
retained and compared at readback, so a write that disturbs `607D` fails rather than
succeeding. It never emits `6040`, `6060`, a target, a save or a reset, and needs no
position-mode evidence.

No source gives a state for this object, so all of these prerequisites are project
policy: a live reservation at the current target, an explicit model, a clean STATUS
and MONITORS read under that reservation, and a fresh complete prior read holding a
documented code. `2100` must report no running or alarm bit and no undocumented bit
7..15. Bit 4 is deliberately unused, because both specimens report it clear with no
enable in any retained record; that observation is raw, not an enabled claim.

`AFTER_HOMING` is refused while `2100` reports homing complete, even when the stored
code is already 1. p18's "effective after homing" defers the effect, and once homing
is reported nothing defers it, so it would be immediate and unqualified. It also
needs observed `607D:01` strictly below `607D:02`, which is project policy too: no
source orders them. Disarming needs neither, so reversed limits can still be turned off.

A matching readback proves the stored code and nothing else. The call reports no
coordinate sign, activation time or protection. It advances the configuration
generation like any explicit setting, so prepared targets and earlier evidence go stale.

## Setting decisions

`Settled` means a typed write exists for that domain. It does not mean the effect is
physically established. Pages are PDF pages. No value below is hardware-qualified.

| Object | Decision | Evidence | What closes the rest |
| --- | --- | --- | --- |
| `2200` | **Gate kept, no write.** A domain is not enough: effect is needed too | EN86/ZH86 p18 say only `0 Pulse Sign`, `1 Pulse /Sign`, and neither says what the codes do under CAN; EDS `[2200]` U16 RW default 0; both specimens 0 | The same native target at each code, a finite move, independent shaft observation and raw `6064`, plus a power cycle for activation. Motion, so packet 19 or later. A flip with no motion shows only that the code is accepted, so it closes nothing |
| `2201` | **Gate kept, no write.** At most 400..51200 if later admitted | p11 0..65535 versus p18 400..51200; EDS default 0; both specimens 1000 | Manufacturer range, then at the baseline and one other in-range value: raw `6064`, `607A`, `607D` after the write, and measured displacement for one fixed native target. The ratio gives the scale; a power cycle between them gives activation. No sweep |
| `2202` | **Settled, codes 0 and 1**, from a documented prior code. Code 1 needs `607D:01` below `607D:02` (project policy) and no homing report | EN86/ZH86 p18 `0 noneffective`, `1 effective after homing`; EDS default 10 undocumented, both specimens 0. On 2026-11-04 both specimens accepted codes 1 and 0, each written and read back (trials 604 to 607, [report](reports/2026-11-04_bench.md)) | Enforcement, which that readback does not show. See the note below: no test closes this until `607D` has a domain |
| `607D:01/:02` | **Gate kept, no write.** No admissible domain | EDS and EN86 p16/p17, ZH86 p17 give I32 and a name only; both specimens signed extremes. `607A` and `607C` ranges are not its range | **A manufacturer range or coordinate basis.** See the note below. `2100` bits need homing first, so no no-motion test can show the frame |
| `6060` | **Gate kept, no write here.** Codes 1, 3, 6 share U8 and I8 encoding; 0 is undefined | p11, p13; EDS `[6060]` WO U8 versus manual RW I8; both specimens display 0 with `6041` low bits 7, the documentary pattern for `6040=15` | Packet 15's isolated `6060:00=1` with fresh mode, status, monitor, error and feedback evidence and independent shaft observation. Enabling in mode 3 or 6 and changing mode while enabled are separate gates (Q03, Q11, Q21). ZH86 p13 says the motor starts at the end of `6,7,15` in mode 3, with `60FF` reading 120 on both specimens, so code 3 is not a no-motion test |

`2202` and `607D` cannot close each other. `2202` enforcement needs a completed homing
and one finite move toward a limit under independent travel protection, recording
`2100` bits 5 and 6, `6064` at the boundary and the reaction. That move needs a finite
`607D`, `607D` has no typed write, and the only observation that could give it a domain
is that same move. So this needs a manufacturer range or coordinate basis first; no bench
sequence inside the current gates can supply it.

Current/holding remains solely prompt 22. No request writes a native setting merely
because an EDS flag is RW. The position-parameter physical path needs acknowledged
`6040=000F`, which is an enable. Packet 15's gated enable has since run on both
specimens (trials 108, 118, 206, 210, 405, 704 and 712), and the position parameters
were then written by the bench `position` route (110 to 113 on 2026-11-03, where 110
was refused and 112 is its rearm; 211, 406, 709 and 713) and by the library moves in
the [position guide](position_parameters.md). Trial 709 wrote `6081`, `6083` and
`6084` for a 300 rpm move with 500 ms ramps. All of this is judged from drive reports
on one firmware; none of it qualifies a parameter's effect.
