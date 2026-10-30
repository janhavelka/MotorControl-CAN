# Lichuan native driver settings

The installed `MotorControlCAN::LICHUAN_C` API in
`MotorControlCAN/profiles/lichuan_c/DriverSettings.h` implements three finite
typed driver **read** groups. The same `DriverContext` and lifecycle also serve
four [I/O groups](io_settings.md) through checked `getIo` and five
[control/tuning groups](tuning.md) through `getControl/getTuning`.
[Prompt 20](reports/prompt_20.md) remains Partial:
the confirmed position-parameter setting group is implemented, while direction,
subdivision, soft limits and mode selection retain their source-specific gates.
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
| DRIVER `2200:00` | U16 `directionRaw` | EDS 1274–1282; EN86/ZH86 p18 | Q04: labels do not establish physical positive direction/CAN effect. |
| DRIVER `2201:00` | U16 `subdivisionRaw` | EDS 1283–1291; EN86/ZH86 p11/p18 | Q04: default 0 and 0–65535 versus 400–51200 conflict; scale/activation unknown. |
| SOFT_LIMITS `2202:00` | U16 `softLimitSelectorRaw` | EDS 1292–1300; EN86/ZH86 p18 | Q05: preserve default 10 versus 0/1 and homing/enforcement prerequisites. |
| SOFT_LIMITS `607D:00` | U8 `softLimitEntries` | EDS 1918–1926 | RO count, never a setter or discovery bound. |
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
- A continuous live `SettingsReservation` issued by the sole application owner
  before acquisition. The owner excludes other control, mode, target, RPDO and
  settings work, and revokes it on contradictory observations or lifecycle loss.
  Pass the actual live token on every call, not a stale retained copy.
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
must not alias context/token storage. Retire separately admitted work before
local cancel/deadline/invalidation. Cancellation is not motor stop. Unknown
write or unqualified read effects remain retained and never replay automatically.

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

## Remaining prompt-20 gates

Direction/CAN effect, conflicting subdivision range/default/scale activation,
soft-limit selector/bounds/frame/enforcement and mode transition effects remain
unresolved as listed above. Current/holding remains solely prompt 22. Those
fields still owe their typed write validation, effects, readback and dependency
invalidation when their evidence arrives, including later real owner integration.
No request writes a native setting merely because an EDS flag is RW.
