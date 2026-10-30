# Lichuan control and tuning

`MotorControlCAN/profiles/lichuan_c/Tuning.h` provides finite typed native reads
and explicit `0x2500` loop selection. It reuses `DriverContext`,
`DriverSettingsContext`, `nextDriver` and `advanceDriver`; it adds no service
engine, clock, allocation or controller access. Prompt 22 remains **Partial** on
the precise write-evidence gates below. No confirmed read is left unimplemented.

## Assignment and source evidence

All 33 scalar entries `2500..2520:00` are U16 RW/PDO-mappable in the original
[EDS](reference/vendor/LC_StepDriverCAN_V1.1.eds), sections 2500..2520,
lines 1559..1822. No EDS default, lower limit or upper limit is supplied.
The original [EN86 manual](reference/vendor/86C_CANopen_user_manual_en.pdf),
PDF p19 covers 2500..2516 and p20 covers 2517..2519; the original
[ZH86 manual](reference/vendor/86C_CANopen_user_manual_zh.pdf), PDF p19 covers
2500..2505 and p20 covers 2506..2519. Printed pages are 17 and 18.
The manuals' covers identify CL86-C/OL86-C; their firmware identity is unknown.
PDF p3 distinguishes CL86-C's incremental encoder from OL86-C's absent encoder.
Those facts do not specify the numeric convention of 2501.

Every row below has read owner and native setting owner **22**. All reads are
documented, implemented and host tested; hardware qualification is false.
Unknown words remain valid raw observations. The canonical
[ledger and generated coverage](reference/lichuan_objects.md) preserve source
claims and separate raw reads, conditional settings and whole-function coverage.

| Exact rows | Read API/group | Native facts | Write disposition and exact blocker |
| --- | --- | --- | --- |
| 2500:00 | `prepareControlRead`, `getControl` | 0 ineffective, 1 open loop, 2 closed loop; distinct from motion mode 6060 | `prepareControlSettings` implemented for explicit CL86-C/OL86-C binding; OL86-C closed-loop rejected. Physical activation timing/firmware qualification remains unknown. |
| 2501:00 | Same control read | Encoder resolution raw U16 | Pending in 22: line/count convention and admissible domain unknown. |
| 2502:00 | `prepareTuningRead(CURRENT_HOLDING)`, `getTuning` | Maximum peak current in mA | Pending in 22: admissible domain and motor/drive peak-current bounds. Product-table 8 A has no peak/RMS qualification. |
| 2503..2506:00 | Same current/holding group | Closed-loop maximum/base, open-loop maximum and holding ratios in percent | Pending in 22: ratio basis and admissible domains; no invented 0..100 bound or base/max ordering. |
| 2507:00 | Same current/holding group | Holding time in ms | Pending in 22: accepted domain and zero/sentinel behavior; U16 width is not an operating range. |
| 2508:00 | `prepareTuningRead(FILTER_POSITION)`, `getTuning` | Low-pass enabling raw word | Pending in 22: documented value set absent; not a Boolean setter. |
| 2509/250D:00 | Same filter/position group | Low-pass/averaging coefficients | Pending in 22: coefficient domains, equations and physical units. |
| 250A/250B:00 | Same filter/position group | Position alarm/in-position thresholds | Pending in 22: threshold scale and admissible domains. These are settings, not measured following error. |
| 250C:00 | Same filter/position group | In-position time raw word | Pending in 22: time unit and admissible domain. No ms conversion. |
| 250E..2519:00 | `prepareTuningRead(LOOP_GAINS)`, `getTuning` | Twelve named native factory loop parameters | Pending in 22: admissible domains/equations/scales. Manuals advise adjustment normally is unnecessary. Velocity nodes have no inferred ordering or rpm unit. |
| 251A..2520:00 | `prepareTuningRead(EDS_GAINS)`, `getTuning` | Seven EDS-only words in address order | Pending in 22: no reviewed manual meaning, units or effects; no semantic setters from symbolic strings. |

There are no confirmed-but-unimplemented tuning rows in this assignment. The
remaining mutations are documentary uncertainty, not proven unsupported model
capabilities. CL2-86C, 57C and other manufacturers acquire no support from this
86C-specific setting subset. Current, current limit and torque stay distinct.

## Read lifecycle and output guarantees

Prepare one group with explicit target/epochs, nonzero operation ID, immutable
exclusive deadline and queue/response budgets. Yield its copied uploads with
`nextDriver`; supply matching retained service terminals to `advanceDriver`.
Only then call the corresponding checked getter with the actual current target
and an inclusive maximum age. The getters publish a complete local candidate;
wrong kind, partial failure, stale/future evidence, epoch mismatch or overlapping
output leaves the destination unchanged.

Each field retains its admission-to-completion interval, delivery time and
binding/configuration/node generations. Time zero is valid. A group is not an
atomic device snapshot: `ReadWindow` exposes earliest/latest bounds and skew.
Delivering an old terminal later does not renew its freshness. Every upload
retains `UNQUALIFIED` effects under Q20; none enters the non-changing probe or
automatic reconciliation. Abort, partial read history and uncertain effects
remain in the context after failure.

```cpp
namespace L = MotorControlCAN::LICHUAN_C;
L::DriverContext read;
// target/options/time/operation come from the sole application owner.
auto status = L::prepareTuningRead(read, target, operation, nowUs, deadlineUs,
                                  options, L::TuningGroup::CURRENT_HOLDING);
// Use nextDriver/read service/advanceDriver, then getTuning with current epochs.
// No controller is opened or lifecycle action performed by preparation.
```

The compiled [standalone consumer](../test/installed_consumer/main.cpp) exercises
control acquisition, an explicit loop candidate and withdrawal before admission.
The independent [native suite](../test/tuning_test.cpp) drives all 33 literal
objects and the loop write through the selected service adapter. The
[application-owner suite](../test/can_bus_owner_test.cpp) additionally drives
actual control reads and loop settings through copied admission, scheduling,
pre-TX notification and retained completion, including failed readback and
uncertain downloads. Its illustrative words/model premises do not qualify a
real drive.

## Explicit loop mutation and retained effects

The application establishes an actual model-bound exclusive
`SettingsReservation` before the complete control read. The same
[settings exclusion and transmission contract](driver_settings.md#explicit-position-parameter-settings)
applies: competing control/mode/target/RPDO/settings work is excluded, and each
call checks the actual still-live reservation and current configuration binding.
This host reservation cannot correlate identical late wire replies or reopen a
quarantined SDO channel.

`ControlSettings` defaults to invalid `UNKNOWN`. Whole-candidate preparation
accepts only documented enum values, rejects absent model binding and OL86-C
closed-loop selection, and preserves output on failure. Only a changed 2500
word is downloaded. The 2501 encoder word is never written. No-op candidates
retain the fresh prior result and do not withdraw configuration.

Before handing a queued changed word to a controller, call
`noteDriverTransmission` with the **actual application/axis binding**. This
withdraws the configuration generation once, invalidating dependent scale,
reference, prepared targets and old snapshots before effects become possible.
Do not use a disposable binding copy as the application truth. Independent
stable observations are not refreshed by this change.

A matching download acknowledgement records `confirmed` bit 0, its requested
word and acknowledgement time. It proves a register acknowledgement only.
After releasing service results, explicitly advance the healthy idle adapter
configuration with `advanceConfiguration`; read back 2500 and 2501 at that new
epoch. Both must match the candidate image, including unchanged encoder data.
A mismatch fails without publishing an observation. Failure during readback
retains the confirmed write; cancellation after possible transmission retains
uncertainty. No rollback, retry, enable, stop, save or reset is emitted.

`INEFFECTIVE` is the vendor selector label. It is not advertised as motor
disable, stop or release. Requested loop mode, read register value, actual
physical activation and motion completion are separate facts.

EN86/ZH86 PDF p19 document the selector's type, access and values without a
drive-state precondition or activation interval. This API confirms the native
register transaction only; it does not establish a qualified physical loop
transition. The broader Q09 evidence gate remains open for unestablished tuning
domains/effects and model/firmware qualification. No disabled-state rule or
fixed activation delay is inferred from another profile.

## Other accounting and verification

`1003:00..04` reads remain 19 (history clearing/persistence stays separately
assigned). `1005/1014/1017/1200` configuration reads remain 19, while
communication mutations remain 38; the 1200 count declaration belongs to 13.
All 88 PDO-record reads remain 38; their declarations/observation and mutation
owners remain 17/18. No row transfers silently into this tuning assignment.
The ledger validator has exact ownership regression tests.

Implementation: `src/profiles/lichuan_c/Reads.cpp` and `Settings.cpp`.
Native tests: `test/tuning_test.cpp`, including literal vendor words,
wrong-width/access replies, every-position read failures, model/enum rejection,
unknown prior repair, ACK/readback mismatch, cancellation and configuration
invalidation. Generator/schema tests separately check exact 95 read descriptors
and the sole conditional tuning setting. Planned CLI owner is prompt 41;
no tuning CLI or hardware qualification exists yet.

```sh
python scripts/generate_lichuan_objects.py --check
python scripts/check_lichuan_ledger_test.py
python scripts/generate_lichuan_objects_test.py
ctest --test-dir .build/p22-native -R '^(tuning|settings_write|reads)$' --output-on-failure
python scripts/verify.py --mode quick --build-dir .build/p22-quick
python scripts/verify.py --mode full --build-dir .build/p22-full
```
