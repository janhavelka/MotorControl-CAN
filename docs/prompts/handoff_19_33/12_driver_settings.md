# Packet 12: driver settings and scale-dependent gaps

Execute this packet fully. Apply [COMMON.md](COMMON.md).

Prerequisites: packet 10 (shared API repairs) and packet 11 (HIL diagnostics), as recorded in this handoff's index. Resume [original prompt 20](../20_driver_settings.md). Finish supported native settings and their owner integration; retain precise gates where device semantics remain unknown. Motion execution belongs to later packets.

## Inspect before changing

Read `docs/driver_settings.md`, `docs/reports/prompts_20_21_audit.md`, original prompt 20 and its historical report, W8/A-02 in [the repair audit](../PROMPTSET_AUDIT_34_50_2026-11-02.md), and current bench/motion-readiness records. The historical claim that all writes are blocked is obsolete: native position-parameter writes already exist.

Inspect `DriverSettings.h`, `Settings.h`, `src/profiles/lichuan_c/Reads.cpp`, `Settings.cpp`, `test/driver_settings_test.cpp`, `settings_write_test.cpp` and actual `CanBusOwner` integration. Follow `prepareDriverRead`, `prepareDriverSettings`, both `nextDriver`/`advanceDriver` overloads, `noteDriverTransmission` and checked `getDriver`. Use the reservation name resulting from packet 10; do not reintroduce the old public type after its migration.

Refresh RS `DriverSettings`, profile snapshots and validation tests, BME280 staged configuration, INA228 configuration/freshness, ADS1115 admitted ownership, and FieldCore configuration/reference invalidation read-only. Read real callers as well as declarations. Retain the established names while keeping Lichuan units and effects separate from ESS.

## Recheck exact source and fixture facts

Render EN86/ZH86 p11–13 and p16–18 and inspect EDS `2200..2202`, `6060`, `607D`, `6081/6083/6084`. Existing accepted native ranges are speed 0..3000 rpm and ramp times 0..2000 ms. Millisecond ramps are not acceleration rates; zero is a register value, not a stop or instantaneous-acceleration guarantee.

Open gates are `2200` CAN/physical direction meaning; `2201` default 0 versus detailed 400..51200 range and scale/activation; `2202` default 10 versus 0/1 and homing/enforcement; `607D` coordinate/range/enforcement; and `6060` transition/retained-target effects. Do not derive one object's limits or units from another. Current/holding `2502..2507` remain exclusively packet 14/original 22.

Both recorded CL86-C specimens report `2201=1000`, `2202=0`, `607D` signed extremes, `607A=5000`, `60FF=120` and mode 0. These are observations, not known-safe prerequisites; the `607A` and `60FF` values equal the EDS defaults, which does not establish where they came from ([correction](../../device_conflicts.md#corrected-reading-of-the-november-2-inventory)). An actual-position zero does not establish a usable reference or small absolute travel. The accepted-read policy from packet 4 does not automatically cover configuration objects; use its explicit reviewed acquisition route where required.

## Implement and integrate

Preserve the complete three-field position-parameter candidate and changed-fields-only sequence. Validate model, live exclusion, fresh matching prior/state/control evidence, widths/ranges, budgets and generation exhaustion before work. Never obtain missing settings prerequisites by silently enabling or changing mode. If live prerequisites require original prompts 29/30's evidence work, retain that boundary and finish independent host integration.

Apply packet 10's exact distinction: explicit settings calls still advance configuration generation and invalidate earlier prepared motion, including explicit speed/ramp updates. Only a move's own607A/6081/6083/6084 phases avoid `prepareDriverSettings` and do not advance their own intent generation; those phases must retire cached MOTION_PARAMETERS observations. Do not weaken stale-target rejection to compose the two paths. For explicit settings, withdraw dependent evidence before possible TX, preserve acknowledged prefixes and uncertainty, retire old work before epoch migration, then obtain exact current-generation readback. No-op candidates remain traffic-free.

For each open setting, establish the smallest admissible domain and effect from applicable evidence before adding a typed mutation. Add direct bounded functions or cohesive groups through the existing context pattern, not a generic arbitrary-object settings engine. A newly observed single value does not prove the full writable domain. Preserve source contradictions and firmware/fixture scope in the ledger; regenerate outputs rather than hand-editing them.

Bounded HIL tests may exercise supported values and justified transitions under COMMON. Record the original complete image, exact changed values, independent observation, intermediate effects and reconciliation. Restoring a prior value is another explicit operation, not automatic rollback. Do not alter scale, direction or limits to make motion appear correct, or clear/replay an uncertain write.

## Checks and closure

Test complete-candidate rejection before TX, all documented boundaries, unknown raw reads, reserved bits, stale/aliased contexts, immutable queued bytes, no-op, partial ACKs, lost reply, readback mismatch, cancellation/reboot/deadline and old-target admission. Exercise the real application owner; `settings_write_test.cpp`'s custom SDO fixture alone is insufficient. Keep the native example independent of CLI.

Delegate source/domain review and adversarial generation tests separately; one implementer owns settings progression and the lead merges shared contracts. Run `driver_settings`, `settings_write`, `reads`, `can_bus_owner`, affected axis/position tests, generated freshness, package/Doxygen and firmware gates. Use packet 11 diagnostics to investigate and retest actual failures. Update user docs, Doxygen, CHANGELOG and coverage with implemented versus physically established facts. Record `docs/reports/handoff_19_33/packet_12.md`, then commit and sync under COMMON. Unresolved fields remain explicit obligations, not unsupported stubs or a claim of complete prompt 20.
