# Lichuan native driver settings

The installed `MotorControlCAN::LICHUAN_C` API in
`MotorControlCAN/profiles/lichuan_c/DriverSettings.h` implements three finite
typed driver **read** groups. The same `DriverContext` and lifecycle also serve
four [I/O groups](io_settings.md), published through checked `getIo` rather than
`getDriver`. [Prompt 20](reports/prompt_20.md) remains Partial:
device writes and their configuration effects are not implemented. There is no
always-rejecting `prepareDriverSettings` stub or generic integer-write substitute.

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
| MOTION_PARAMETERS `6081:00` | U32 `positionSpeedRpm` | EDS 1943–1951; EN86/ZH86 p11/p17 | Q11: document range 0–3000 rpm; compatible write state is not established. |
| MOTION_PARAMETERS `6083:00` | U32 `rampUpTimeMs` | EDS 1952–1960; EN86/ZH86 p11/p13 | Q11: document range 0–2000 ms; state and zero behavior remain unqualified. |
| MOTION_PARAMETERS `6084:00` | U32 `rampDownTimeMs` | EDS 1961–1969; EN86/ZH86 p11/p13 | Same gate; milliseconds are not an acceleration rate or stop guarantee. |

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

## Narrow write prerequisite still owed to prompt 20

EN86/ZH86 p12 documents selecting/observing position mode, illustrated enabling,
writing parameters, then separately triggering with controlword bit 4. Thus
positive `6081` 1–3000 rpm and `6083/6084` 1–2000 ms have documented native
values and a write location. Missing velocity-start order, physical ramp formula
or zero behavior does not invalidate that narrow documentary subset.

The current raw state/read API does not establish the compatible pre-trigger
control state, including the trigger level. Raw `6041` low bits, `6061=1`, a
generic SDO acknowledgement or a new caller Boolean cannot prove it. No installed
controlword-history/state contract currently supplies that evidence. Writes
therefore remain pending on concrete retained state/control provenance or
applicable manufacturer evidence, rather than a claim of absent numeric ranges.
Preparation must never enable or change controlword merely to create a prerequisite.

On resumption, implement whole-candidate validation, copied immutable writes,
confirmed per-step effects, uncertain final writes, matching readback and
configuration/reference invalidation. Preserve soft-limit coordinate basis;
invalidate dependent scale/reference/prepared targets after confirmed or uncertain
scale changes. Neither rollback, automatic retry nor implicit save/reset is
permitted. Those obligations stay in 20, including later owner integration;
raw read completion does not close them.
