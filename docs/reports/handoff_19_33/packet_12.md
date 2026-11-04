# Packet 12: driver settings and scale-dependent gaps

Resumes prompt 20. When this packet closed, host and fake evidence only, because bench time went to the communication soak and no firmware route called these writes. **No specimen has accepted any write here.** The `settings-2202` route ran T1 and T2 on 2026-11-04 and stopped at its prior read, before any write: see [the bench result](#bench-2026-11-04-t1-and-t2-blocked-before-the-write) below.

## Delivered

See the [settings guide](../../driver_settings.md) and the [changelog](../../../CHANGELOG.md).

- `prepareSoftLimitSettings` (2202), which establishes a stored code only.
- `test/settings_owner_test.cpp` runs settings through the real `CanBusOwner` and `SdoClient` against a scripted dictionary holding the specimens' values, with the packet's whole case list. Hand mutants of `Settings.cpp` are killed (not retained).
- Packet 10's distinction is proved end to end: an explicit call advances the generation once, at the first possible download. Admission alone, queue or preflight rejection and a pre-TX cancel do not. A move's own writes through the hook leave its prepared target valid and retire the prior and a finished `readback`.

## Decisions per setting

Pages are PDF pages.

| Object | Ruling | Evidence | What closes the rest |
| --- | --- | --- | --- |
| 2200 | **Gate kept, no write.** The packet requires an effect as well as a domain, and EN86 and ZH86 p18 say only `0 Pulse Sign`, `1 Pulse /Sign`, with nothing on what the codes do under CAN | EN86 and ZH86 p18; EDS U16 RW default 0 | Same native target at each code, a finite move, independent shaft observation and raw `6064`, and a power cycle for activation. That is motion, so packet 19 or later |
| 2201 | **Gate kept, no write** | p11 `0..65535` against p18 `400..51200`; EDS default 0; specimens 1000. Every later pulse target would change by an unmeasured ratio | Manufacturer range, then the baseline and one other in-range value: raw `6064`, `607A`, `607D`, measured displacement for one fixed native target, and a power cycle |
| 2202 | Typed write, codes 0 and 1, from a documented prior code only. Code 1 needs observed `607D:01 < :02`, which is project policy, and is refused while 2100 reports homing complete, because p18's deferral no longer applies and the effect would be immediate. A prior or candidate 10 refuses | EN86 and ZH86 p18; EDS default 10 is undocumented; specimens 0 | Enforcement needs a finite `607D`, so a manufacturer range or coordinate basis |
| `607D:01/:02` | **Gate kept, no write.** I32 and a name are not a domain. The 2202 test cannot close it: that test needs a finite `607D` first | EDS; EN86 p16-17, ZH86 p17 | Manufacturer range and coordinate basis |
| 6060 | **Gate kept, no write here** | p11, p13; EDS WO U8 against manual RW I8; ZH86 p13 starts the motor at the end of `6,7,15` in mode 3, and `60FF` reads 120 | Packet 15 step 3 |

The state prerequisite for 2202 is **project policy; no source gives one**: a live reservation, explicit model, a clean STATUS and MONITORS read, and no 2100 running, alarm or undocumented bit 7..15. 2100 bit 4 (released) is deliberately unused, because both specimens report it clear with no enable in any retained record. Bit 4 clear and `6041=0x8037` also match the documented enabled pattern, so on these specimens a write's effect on a possibly energized drive is unestablished.

## Rulings on questions left by earlier packets

- **Packet 10, a failed state read.** `prepareDriverSettings` now refuses a failed or uncertain state read even when STATUS and MONITORS completed. A settings call is never urgent, so the cost is one fresh read.
- **Packet 9, raw `positionSpeedRpm`.** It stays raw: a native register value with its unit in its name. So the `preparePositionParameters` caller is the seam: packet 20 must route common rates through `prepareVelocityTarget` under `MOTOR_TURNS` and `MINUTE`, and check the frozen fields. **That gap stays open until the caller exists.**

## Independent review, all fixed

An independent reviewer rendered the pages and probed the code. It found no frame commanding motion, and T1's bytes as captured through the real owner are correct. It did find these:

| Finding | Fix |
| --- | --- |
| A frozen token plus old evidence prepared a no-op with `state=SUCCEEDED` and `uncertain=false`, sending nothing while the drive held another value, even after a lost reply | Every settings `prepare*` takes the actual current target and refuses a stale reservation before publishing |
| A typed 2200 write established a domain and no effect, which the packet does not allow | Withdrawn, and gated |
| 2202 could arm on a homed drive, where p18's deferral was the reason for dropping RS's gate | Refused while homing is reported. **RS requires homing complete to enable, so this is the opposite, a recorded delta**: arming must precede homing. A stored code already 1 is refused too while homed |
| Closure conditions for 2202 and `607D` were circular | Stated plainly: they need a manufacturer range or coordinate basis |
| Undocumented 2100 bits passed | Bits 7..15 refuse for 2202 |
| Selector wire bytes were never asserted, and three mutants survived (not retained) | Pinned |

## Bench, 2026-11-04: T1 and T2 blocked before the write

The `settings-2202` route (`TD`, `e7f28ac`) ran code 1 on node 1 ([313](../../bench/20261104T1405Z-settings2202-node1-313/rx.txt)) and node 2 ([315](../../bench/20261104T1405Z-settings2202-node2-315/rx.txt)). Its `SOFT_LIMITS` prior read got `2202=0`, then **`607D:00` answered as two bytes**, `4B 7D 60 00 00 00 00 00`, against the EDS U8, and the strict typed read aborted it (`06070010`). The route ended before `prepareSoftLimitSettings` ran, **no write was sent**, and the abort left each route unresolved, so the returns (314, 316) were refused `session`. Both drives read the specimens' code 0, as before. See the [bench report](../2026-11-04_bench.md) and backlog D16.

**Still owed:** T1 and T2 themselves, after the typed `SOFT_LIMITS` read has an explicit measured-width option for `607D:00`, as `1009`/`100A` have. That option is being implemented. Until then no `2202` code has been written to any specimen, and the default-10 question stays open.

## The planned trials

Written before the route existed; the route now implements them. Each neither enables nor moves. Run them with one drive at a time, a full readiness image before and after, and stop on any abort, timeout, uncertain terminal, boot-up, bus fault, change in 2100, 6041, 6061, 6064 or 606C, or reported shaft movement. Never retry.

| Trial | Node | Expected on the wire | Closes |
| --- | --- | --- | --- |
| T1, 2202 selector | 1 | `601: 2B 02 22 00 01 00 00 00`, readback, then `2B 02 22 00 00 00 00 00`, readback. A repeat of the last value sends no write, after its fresh evidence reads | Code acceptance and the default-10 question on this firmware only, not enforcement |
| T2 | 2 | The same | The same |

The return write is a separate explicit operation, sent only if the forward write was acknowledged and read back. An uncertain write is not replayed, and the possibly non-default value is reported. Volatility across a power cycle is unknown, and nothing is saved.

## Sibling refresh

| Sibling | Revision | Dirty | Adopted | Rejected |
| --- | --- | --- | --- | --- |
| MotorControl-RS | `e59c5ca` | Clean | `validatePriorSettingsAndStandstill` and `LIMIT_DEPENDENCY`: limits known off before geometry; `SoftLimitEnable`. `DefaultDirection` was adopted here and then withdrawn with the typed `2200` write in `99ff2b0`, beside packet 13 | `legal()` subdivision `400..51200` is ESS, not evidence here. `limitSemanticsQualified` before arming: a recorded delta, since p18 defers the effect until homing. Snapshot restore |
| BME280 | `48b8214` | Clean | Staging, then a generation that stales old samples | Local rollback: a motor write is not rolled back |
| INA228 | `73f9548` | Clean | Less trusted, evidence kept | Driver-object flag |
| ADS1115 | `d613a13` | Clean | Single owner; a dirty observation needs fresh acquisition | Sensor auto-recovery and retry |
| FieldCore-node | `5ad9292` | Clean | Consistent with generation withdrawal; no integration impact | None |

## Open

Prompt 20 stays Partial: the 2200, 2201, 607D and 6060 gates above, enforcement of 2202, and the `607D:00` reply width that keeps T1 and T2 from reaching their write. A stored 2202 of 10 has no typed way out, deliberately. `examples/driver_settings_preview` stays read-only.

## Checks

See the [execution table](README.md).
