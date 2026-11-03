# Packet 12: driver settings and scale-dependent gaps

Resumes prompt 20. Host and fake evidence only, because bench time went to the communication soak and no firmware route calls these writes yet. **No specimen has accepted any write here.**

## Delivered

See the [settings guide](../../driver_settings.md) and the [changelog](../../../CHANGELOG.md).

- `prepareDirectionSettings` (2200) and `prepareSoftLimitSettings` (2202), which establish a stored code only.
- `test/settings_owner_test.cpp` runs settings through the real `CanBusOwner` and `SdoClient` against a scripted dictionary holding the specimens' values, with the packet's whole case list. 25 hand mutants of `Settings.cpp` are killed.
- Packet 10's distinction proved end to end: an explicit call advances the generation once, at the first possible download. Admission alone, queue or preflight rejection and a pre-TX cancel do not. A move's own writes through the hook leave its prepared target valid and retire the prior and a finished `readback`.

## Decisions per setting

Pages are PDF pages.

| Object | Ruling | Evidence | What closes the rest |
| --- | --- | --- | --- |
| 2200 | Typed write, codes 0 and 1. Stored code only | EN86 and ZH86 p18 agree; EDS U16 RW default 0 | Same native target at each code, a finite move, independent shaft observation and raw `6064`. That is motion, so packet 19 or later |
| 2201 | **Gate kept, no write** | p11 `0..65535` against p18 `400..51200`; EDS default 0; specimens 1000. Every later pulse target would change by an unmeasured ratio | Manufacturer range, then the baseline and one other in-range value: raw `6064`, `607A`, `607D`, measured displacement for one fixed native target, and a power cycle |
| 2202 | Typed write, codes 0 and 1, from a documented prior code only. Code 1 needs observed `607D:01 < :02`. A prior or candidate 10 refuses | EN86 and ZH86 p18; EDS default 10 is undocumented; specimens 0 | Enforcement: completed homing and one finite move toward a limit under independent travel protection |
| `607D:01/:02` | **Gate kept, no write.** I32 and a name are not a domain | EDS; EN86 p16-17, ZH86 p17 | Manufacturer range and coordinate basis, or the 2202 test |
| 6060 | **Gate kept, no write here** | p11, p13; EDS WO U8 against manual RW I8; ZH86 p13 starts the motor at the end of `6,7,15` in mode 3, and `60FF` reads 120 | Packet 15 step 3 |

The state prerequisite for 2200 and 2202 is **project policy; no source gives one**: a live reservation, explicit model, a clean STATUS and MONITORS read, and no 2100 running or alarm bit. 2100 bit 4 (released) is deliberately unused, because both specimens report it clear with no enable in any retained record.

## Rulings on questions left by earlier packets

- **Packet 10, a failed state read.** `prepareDriverSettings` now refuses a failed or uncertain state read even when STATUS and MONITORS completed. A settings call is never urgent, so the cost is one fresh read.
- **Packet 9, raw `positionSpeedRpm`.** It stays raw: a native register value with its unit in its name. So the `preparePositionParameters` caller is the seam: packet 20 must route common rates through `prepareVelocityTarget` under `MOTOR_TURNS` and `MINUTE`, and check the frozen fields. **That gap stays open until the caller exists.**

## Documented hazard

A finished transaction keeps its frozen `AxisReservation`. A stale token plus old evidence can still prepare, including a false no-op with `state==SUCCEEDED`. `nextDriver`, `noteDriverTransmission` and `getDriver` all refuse it, and a test proves this. So read results through `getDriver`, not `state`.

## Physical tests not run

They need a bounded harness route that does not exist yet, and neither enables nor moves. Run them with one drive at a time, a full readiness image before and after, and stop on any abort, timeout, uncertain terminal, boot-up, bus fault, change in 2100, 6041, 6061, 6064 or 606C, or reported shaft movement. Never retry.

| Trial | Node | Expected on the wire | Closes |
| --- | --- | --- | --- |
| T1, 2202 selector | 1 | `601: 2B 02 22 00 01 00 00 00`, readback, then `2B 02 22 00 00 00 00 00`, readback, then no frames on a repeat | Code acceptance and the default-10 question on this firmware only, not enforcement |
| T2, 2200 selector, after T1 | 1 | `2B 00 22 00 01 00 00 00`, readback, then `2B 00 22 00 00 00 00 00`, readback | Code acceptance and whether 2201 or status reacts, not direction |
| T3 | 2 | The same | The same |

The return write is a separate explicit operation, sent only if the forward write was acknowledged and read back. An uncertain write is not replayed, and the possibly non-default value is reported. Volatility across a power cycle is unknown, and nothing is saved.

## Sibling refresh

| Sibling | Revision | Dirty | Adopted | Rejected |
| --- | --- | --- | --- | --- |
| MotorControl-RS | `e59c5ca` | Clean | `validatePriorSettingsAndStandstill` and `LIMIT_DEPENDENCY`: limits known off before geometry; `DefaultDirection`, `SoftLimitEnable` | `legal()` subdivision `400..51200` is ESS, not evidence here. `limitSemanticsQualified` before arming: a recorded delta, since p18 defers the effect until homing. Snapshot restore |
| BME280 | `48b8214` | Clean | Staging, then a generation that stales old samples | Local rollback: a motor write is not rolled back |
| INA228 | `73f9548` | Clean | Less trusted, evidence kept | Driver-object flag |
| ADS1115 | `d613a13` | Clean | Single owner; a dirty observation needs fresh acquisition | Sensor auto-recovery and retry |
| FieldCore-node | `5ad9292` | Clean | Consistent with generation withdrawal; no integration impact | None |

## Open

Prompt 20 stays Partial: the 2201, 607D and 6060 gates above, the physical meaning of 2200 and enforcement of 2202. `examples/driver_settings_preview` stays read-only.

## Checks

See the [execution table](README.md).
