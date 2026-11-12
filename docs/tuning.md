# Lichuan control and tuning

`MotorControlCAN/profiles/lichuan_c/Tuning.h` provides finite typed native reads
and explicit `0x2500` loop selection. It reuses `DriverContext`,
`DriverSettingsContext`, `nextDriver` and `advanceDriver`; it adds no service
engine, clock, allocation or controller access. Prompt 22 remains **Partial**:
2500 is the only typed write, and each of the other 32 rows keeps a named gate
below. No confirmed read is left unimplemented. **No specimen has accepted any
write here**; all write evidence is host and fake-owner evidence.

## Assignment and source evidence

All 33 scalar entries `2500..2520:00` are U16 RW/PDO-mappable in the original
[EDS](reference/vendor/LC_StepDriverCAN_V1.1.eds), sections 2500..2520,
lines 1559..1822. No EDS default, lower limit or upper limit is supplied.
The original [EN86 manual](reference/vendor/86C_CANopen_user_manual_en.pdf),
PDF p19 covers 2500..2516 and p20 covers 2517..2519; the original
[ZH86 manual](reference/vendor/86C_CANopen_user_manual_zh.pdf), PDF p19 covers
2500..2505, with the 2505 name running onto p20, and p20 covers 2506..2519.
Printed pages are 17 and 18. Both pages were rendered again for packet 14.
251A..2520 appear in no retained manual (EN86, ZH86, OP86, ZH57, OP57).
The manuals' covers identify CL86-C/OL86-C; their firmware identity is unknown.
PDF p3 distinguishes CL86-C's 1000-line incremental encoder from OL86-C's absent
encoder. That does not specify the numeric convention of 2501.

Every row below has read owner and native setting owner **22**. All reads are
documented, implemented and host tested; hardware qualification is false. Both
specimens read 2502..2520 once, on 2026-11-04, through the bench
`readiness-tuning` route, with identical values ([below](tuning.md#specimen-readings-2026-11-04)).
Unknown words remain valid raw observations. The canonical
[ledger and generated coverage](reference/lichuan_objects.md) preserve source
claims, and each row's `review.next_evidence` names what would close its gate;
the ledger checker refuses a Prompt 22 row without one.

## Decisions per subgroup

Pages are PDF pages. "No range" means neither manual nor the EDS gives an
admissible domain, so the U16 width is not one.

| Rows | Source facts | Decision | Next concrete evidence |
| --- | --- | --- | --- |
| 2500 | EN86/ZH86 p19: 0 noneffective (无效), 1 open loop, 2 closed loop. EN86/ZH86 p3: OL86-C has no encoder | **Typed write**, `prepareControlSettings`, below. The library admits 0..2 for CL86-C and 0..1 for OL86-C; no specimen has accepted a write. An ACK is register acceptance only | The attended trial T1: an explicit write from 2 to 1 and back on one CL86-C at its installed firmware, with readback; a manufacturer statement of when a change applies and what 0 does |
| 2501 | EN86/ZH86 p19 "Encoder resolution", no unit or range. EN86/ZH86 p3: CL86-C 1000-line encoder. Both specimens read 4000, which matches 4 × 1000 lines only by inference | **Gate**, read back unchanged on every loop write | Manufacturer unit (lines or counts), range and whether a fixed encoder makes it read-only; without a write, the 6064 change over one independently measured revolution |
| 2502 | EN86/ZH86 p19 "Max. peak current", mA, no range. EN86/ZH86 p3: 8 A maximum output current, no peak label. The [product page](reference/vendor/CL86-C_manufacturer_product.html) says "Maximum peak current: 8A" under Features, but "Maximum Output Current 8A" in its spec table, and its supply ranges (DC30..110 V, AC20..80 V) differ from p3, so it describes another revision | **Gate.** 8 A is a drive ceiling, not a setting or the LC60H2127's rating | Manufacturer range and peak/RMS convention for this firmware; the installed LC60H2127 variant's rated phase current from its nameplate or datasheet |
| 2503..2505 | EN86 p19, ZH86 p19..20: closed-loop maximum, closed-loop base and open-loop maximum ratios, unit %. No base, range or ordering | **Gate.** A % label does not establish 0..100; no base ≤ maximum rule is invented | Manufacturer formula for the percent base, the range and any ordering; then an intermediate-current check over the write order, as RS `validateIntermediateCurrentTuple` does |
| 2506, 2507 | EN86 p19 "Current ratio for locking" (%) and "Locking current time" (ms); ZH86 p20 锁机电流比例/时间. "Holding" is this library's name | **Gate.** No range, no zero meaning, no start condition; delay or duration unknown for 2507 | Manufacturer meaning, range and zero behavior; then a timed current-probe measurement at standstill |
| 2508 | EN86 p19, ZH86 p20 "Low-pass filtering enabling", factory default, "adjustment not required generally" | **Gate.** No code set, so not a Boolean | Manufacturer code set and effect; both specimens read 0 |
| 2509, 250D | Low-pass and mean filtering coefficients, same pages. The EDS names 250D `AvrFiltime`, a time | **Gate.** No equation, unit or range; coefficient against time unresolved for 250D | Manufacturer equations, sample period, units and ranges |
| 250A, 250B | "Out-of-tolerance alarm threshold" and "Positioning completion threshold", same pages. EN86/ZH86 p20 list a position out-of-tolerance alarm with no linked threshold | **Gate.** No scale or range. A threshold is a setting, not measured following error; 250A is never widened to make a test pass | Manufacturer unit (encoder counts, command pulses or other), range, and the report each drives; then a comparison against 6064 after a finite move |
| 250C | "Positioning completion time", same pages, no time unit | **Gate.** No ms conversion | Manufacturer time unit and range; then a timed in-position report after a finite move |
| 250E..2519 | Twelve named current, velocity and position loop parameters, EN86 p19 (2517..2519 on p20), ZH86 p20, all "factory default, adjustment not required generally" | **Gate.** No domain, scale or ordering; velocity nodes have no rpm unit | Manufacturer domain, scale and ordering for each, and whether the product page's "motor parameter auto-tuning" rewrites them |
| 251A..2520 | EDS only: `v_kp`, `v_ki`, `v_kc`, `v_kvfbd1`, `v_kvfbd2`, `v_kvff`, `v_kacff` | **Gate.** A symbolic name is not a meaning | Any manufacturer document that names these words with meaning, unit and range |

Corrections recorded in packet 14: the earlier claim that the product's 8 A has
"no peak/RMS qualification" holds for the manual table and the product page's
spec table; only the product page's Features list says peak, on a page whose
supply ranges belong to another revision. Either way it is not a setting for the
installed motor. The product page is not a ledger source, so the ledger cites
p3 only. The ledger now keeps the manuals' "locking" label for 2506/2507
instead of "standstill", and cites every page its prose names.
There are no confirmed-but-unimplemented tuning rows. The withheld writes are
documentary gaps, not proven unsupported capabilities.

### Specimen readings, 2026-11-04

Raw words, decimal, identical on both CL86-C specimens on one firmware (trials
309 and 310, [report](reports/2026-11-04_bench.md),
[packet 14](reports/handoff_19_33/packet_14.md)): `2502=8400`, `2503=100`,
`2504=40`, `2505=100`, `2506=40`, `2507=200`, `2508=0`, `2509=5`, `250A=4000`,
`250B=20`, `250C=10`, `250D=512`, `250E=6144`, `250F=1228`, `2510=40`,
`2511=20`, `2512=2560`, `2513=114`, `2514=25`, `2515=2560`, `2516=1024`,
`2517=100`, `2518=20`, `2519=4`, `251A=12001`, `251B=1000`, `251C=1024`,
`251D=1228`, `251E=8`, `251F=0`, `2520=0`. No unit, scale or domain follows
from a value, and every gate above stands. `2502=8400` does not open T1: its go
condition also needs the installed motor's rated phase current, and the trial a
phase-current probe. CL2-86C, 57C and other
manufacturers acquire no support from this 86C subset. Current, current limit
and torque stay distinct.

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
remain in the context after failure. Reads of 2502..2520 are outside the
accepted polling list, so bench use needs its own reviewed acquisition path: the
S2 bench's `readiness-tuning` route is one, with a one-use grant per upload.

```cpp
namespace L = MotorControlCAN::LICHUAN_C;
L::DriverContext read;
// target/options/time/operation come from the sole application owner.
auto status = L::prepareTuningRead(read, target, operation, nowUs, deadlineUs,
                                  options, L::TuningGroup::CURRENT_HOLDING);
// Use nextDriver/read service/advanceDriver, then getTuning with current epochs.
// No controller is opened or lifecycle action performed by preparation.
```

## Explicit loop mutation and retained effects

The application establishes an actual model-bound exclusive `AxisReservation`
before a clean STATUS and MONITORS state read and the complete control read.
The same [settings exclusion and transmission contract](driver_settings.md#explicit-position-parameter-settings)
applies: competing control/mode/target/RPDO/settings work is excluded, and each
call checks the actual still-live reservation and current target. This host
reservation cannot correlate identical late wire replies or reopen a
quarantined SDO channel.

`prepareControlSettings(output, reservation, current, prior, state, settings, ...)`
validates the whole candidate before publishing anything:

- `ControlSettings` defaults to invalid `UNKNOWN`; only 0, 1 and 2 pass. An
  absent or unknown model is refused, and so is `CLOSED_LOOP` for OL86-C.
- A stored undocumented selector word is readable raw and can be replaced.
- **Project policy, not a vendor rule**, as for 2202: the state read must have
  succeeded cleanly under the reservation, with no 2100 running, alarm or
  undocumented bit 7..15 report. Bit 4 decides nothing. This is not evidence of
  a stopped shaft or a safe transition; RS refuses its control settings unless
  stationary with no alarm. An alarm refuses every selector, deliberately: open
  loop cannot be selected to get past an out-of-tolerance alarm, even one
  caused by a lost encoder (EN86 PDF p20, cause 3).
- **Project policy**: `CLOSED_LOOP` is refused while 2501 reads zero. Whatever
  2501 counts, zero of them cannot scale feedback. Leaving closed loop stays
  possible.

Only a changed 2500 word is downloaded; 2501 is never written. An unchanged
candidate that passes the state and 2501 rules succeeds from the fresh prior
read with no frame, no notice and no epoch change. Before handing the queued
word to a controller, call
`noteDriverTransmission` with the actual application/axis binding. That
withdraws the configuration generation once, so prepared targets and old reads
fail their epoch checks before any effect is possible.

A matching acknowledgement records `confirmed` bit 0 and its time. It proves
register acceptance only, not physical loop activation. After releasing the
write's result, advance the owner's configuration with `advanceConfiguration`;
readback of 2500 and 2501 runs at the new epoch, and both must match, including
the unchanged 2501. A mismatch or a failure at either word publishes nothing
and keeps the acknowledgement. A lost reply, cancellation after possible
transmission, reboot, bus-off or deadline keeps uncertainty. No rollback, retry,
enable, stop, save or reset is emitted.

The manual calls selector 0 (`INEFFECTIVE`) noneffective. Whether it removes
torque is unknown, so it is not a disable, release or stop the library can rely
on. Loop selector 2500 is not motion mode 6060. Requested selector, read
register value, physical activation and motion completion are separate facts.

**Warning: the drive may be energized when the write lands.** Both specimens
report `6041=0x8037` and 2100 bit 4 clear with no enable written in any
retained record, which matches the documented enabled pattern, and OP86 PDF p5
note 4 says the motor is shaft-locked after power-up (OP86 pp5-14 repeat the
57C manual, so this is not independent 86C evidence). On the bench both
shafts held torque at power-up, checked by hand on 2026-11-03 and reported by
the user on 2026-11-04. A 2500 change can therefore act on an energized motor: closed to open may snap the rotor or
change the phase current. No source gives a drive-state precondition or an
activation interval, and none is inferred from another profile.

Recorded delta from RS: RS refuses every control write, the algorithm selector
included, unless `CONTROL_EFFECTS_REQUIRED` qualification and a qualified
current tuple are supplied (`DriverSettings.cpp` lines 366-433). This 2500 write
has no such gate. Its only evidence gate is an attended bench trial (T1): one
write from 2 to 1 and back on one CL86-C, with a phase-current probe, an
unloaded shaft and the drive power cutoff at hand. Until that passes, a changed
selector is an unqualified action on a possibly energized drive.

## Other accounting

`1003:00..04` reads and their read-only declarations remain 19.
`1005/1014/1017/1200:01..02` configuration reads remain 19 and their mutations
38; the `1200:00` count declaration belongs to 13. All 88 PDO-record reads
remain 38; their declarations and mutations remain 17/18, and persistence 39.
No row transfers silently into this tuning assignment, and the ledger validator
has exact ownership regression tests.

## Verification

Implementation: `src/profiles/lichuan_c/Tuning.cpp` and `Settings.cpp`.

- [`test/settings_owner_test.cpp`](../test/settings_owner_test.cpp) runs the
  loop write through the real `CanBusOwner` and `SdoClient` against a scripted
  dictionary holding the specimens' 2500=2 and 2501=4000: literal frames for
  every transition, no-op silence, whole-candidate and state refusals, the
  inclusive age bound, frozen tokens, aliases, immutable queued bytes, aborted
  and lost downloads, failed, lost and mismatched readback at each word,
  cancellation before and after handoff, preflight and controller refusal,
  deadlines, boot-up, bus-off, EMCY, notice-before-TX, retirement before epoch
  migration, and that no other word or object is written.
- [`test/tuning_test.cpp`](../test/tuning_test.cpp) drives all 33 literal reads
  and the model/enum and policy matrix through the direct service adapter.
- [`test/settings_write_test.cpp`](../test/settings_write_test.cpp) checks that
  refused calls leave a retained control transaction unchanged.
- [`test/can_bus_owner_test.cpp`](../test/can_bus_owner_test.cpp) keeps the
  owner's retained partial-effect scenarios for the loop write.

21 hand mutants of the control-settings logic were each killed in a scratch
copy. Generator/schema tests check the 95 read descriptors, the sole
conditional tuning setting and that each withheld row's next evidence reaches
the generated coverage. The planned CLI owner is prompt 41; no tuning CLI or
hardware qualification exists yet.

```sh
python scripts/generate_lichuan_objects.py --check
python scripts/check_lichuan_ledger_test.py
python scripts/generate_lichuan_objects_test.py
ctest --test-dir <build> -R "^(tuning|settings_write|settings_owner|can_bus_owner)$" --output-on-failure
```

## Changing gains and currents

The library reads every gain and current word, but writes only the loop selector `2500`. No other
row has a typed write, because no source gives a safe range for it. Meanings come from the
original EN86/ZH86 PDF pages 19-20. Every row is U16 RW in the EDS, with no default and no limits.
"Specimens" means the identical readings of 2026-11-04 listed above.

| Index | Meaning | Unit and range evidence | Specimens | Library access today |
| --- | --- | --- | --- | --- |
| 2500 | Loop mode: 0 noneffective, 1 open, 2 closed loop | 0..2, documented | 2 | Read; typed write `prepareControlSettings` (console `control set`); never run on a drive |
| 2501 | Encoder resolution | No unit or range | 4000 | Read only |
| 2502 | Maximum peak current | mA, no range. EN86 PDF p3 gives 8 A maximum output current, but both drives read 8400 (8.4 A); no source explains this | 8400 | Read only |
| 2503..2505 | Closed-loop maximum, closed-loop base and open-loop maximum current ratios | %, no base, range or ordering | 100, 40, 100 | Read only |
| 2506, 2507 | Locking (holding) current ratio and time | % and ms, no range or zero meaning | 40, 200 | Read only |
| 2508..250D | Filters, position alarm threshold, in-position threshold and time | No units; "factory default, adjustment not required generally" | Listed above | Read only |
| 250E..2519 | Current, velocity and position loop gains | No unit, scale or range; same note | Listed above | Read only |
| 251A..2520 | EDS-only names | Names only | Listed above | Read only |

Read them with `prepareTuningRead` and `getTuning`, or the console `tuning ... read` lines. Each
upload needs its own one-use review (Q20).

### How to change them today

- **The vendor's debugging software.** This is the manufacturer's route. The manuals list an RS232
  debugging port (EN86 PDF p3-5). They say I/O functions can be set "through the CANopen bus or the
  upper computer debugging software" (EN86/ZH86 PDF p8). OP86 PDF p4, a page copied from the 57C
  manual, says the software and its adapter cable come from Lichuan sales or after-sales. The
  manuals do not say that the tool edits `2502..2520`, whether it saves, or which firmware it
  supports. This project has not obtained or run it. Use it with Lichuan's settings for the
  installed motor.
- **No DIP switch.** SW1..SW8 set only the node, the bitrate and the termination (EN86 PDF p9-10).
- **Not through this library.** No typed write, console line or bench route exists for
  `2501..2520`. The S2 bench's raw `W` line writes PDO records only. The generic SDO layer could
  carry a raw download from your own code, but nothing would check the value, and nothing here
  has tried it. Treat that as unsupported and unsafe.

Risks of any change:

- A current above the motor's rating can overheat or damage the motor. The installed LC60H2127's
  rated phase current is not established.
- Wrong gains can make the loop oscillate, drop holding torque or raise the position
  out-of-tolerance alarm. Widening `250A` can hide a real fault.
- The motor may be energized when a change lands. See the warning under "Explicit loop mutation
  and retained effects" above.
- Nobody has measured whether a change to these words is volatile. Other bench writes (`1017`, the
  TPDO1 map, `2400:01`, `6083`) were gone after one power cycle with no save. After a save
  (`2300=2`) a change may stay, and the library cannot write most of these words back. What a
  factory restore does to them is unknown.

### What a typed write needs

Prompt 22 owns these rows. Before a row or group gets a write, it needs:

1. The manufacturer's range, unit and convention for the installed firmware. For `2502`: peak or
   RMS, and why 8400 exceeds 8 A. For `2503..2506`: the percent base and ordering. For `2507`:
   delay or duration.
2. The installed motor's rated phase current, from its nameplate or datasheet.
3. A check of the whole current tuple before any current write, as MotorControl-RS
   `validateIntermediateCurrentTuple` does.
4. One attended trial per group on one specimen: one write and its readback, with a phase-current
   probe, an unloaded shaft and the power cutoff at hand. Then the original value goes back, and a
   power cycle shows whether the change was volatile.
5. The `2500` lifecycle above: whole-candidate validation, withdrawal at possible transmission,
   readback and retained uncertainty, with no save.

The page checks, the evidence labels and the proposed trials are in the
[configuration evidence report](reports/2026-11-12_configuration_evidence.md).
