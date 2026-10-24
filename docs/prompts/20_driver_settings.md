# Prompt 20 Driver motion and scale settings

Apply [COMMON.md](COMMON.md). Prerequisites: [19](19_typed_reads.md).

## Objective and boundaries

Deliver complete typed read/write groups for the reviewed driver, motor/current/holding, motion-parameter, subdivision/scale and soft-limit rows assigned here. Keep these native settings separate from motion execution, digital I/O, tuning, communication changes and persistence. Those have their own prompts. This is a bounded collection of related settings with one validation and partial-effect contract, not a new configuration framework.

Follow [CONVENTIONS](CONVENTIONS.md#3-motion-typed-reads-and-terminal-evidence) and [MULTIDRIVE](MULTIDRIVE.md): native groups are profile-specific. Servo profiles need not expose subdivision; motor kind does not determine feedback or commanded position units.

## Read and audit

Refresh MotorControl-RS, ADS1115 and INA228 revisions and dirty state. Read RS `DriverSettings`, `ControlSettings`, motion-profile snapshots, group getters and whole-candidate validation tests. Inspect BME280 staged configuration and INA228 configuration/freshness handling. Read current FieldCore configuration-generation callers without editing that repository. Recheck original Lichuan pages and EDS types, access, ranges, ramp units and effects for every assigned field.

### Concrete evidence to recheck

Use [basic settings and tuning facts](../audit/LICHUAN_OBJECT_FACTS.md) plus [motion facts](../audit/LICHUAN_MOTION_FACTS.md). `2201` U16/default 0 conflicts with the detailed 400..51200 subdivision range; `2202` U16/default 10 conflicts with documented 0 disabled/1 effective after homing. `607D:01/:02` are I32 soft limits. `6083/6084` are U32 ramp times, mode-table range 0..2000 ms, not acceleration rates. Current/holding rows `2502..2507` use mA, percentages and ms as documented. Assign those rows here or to 22 once, with explicit ownership; do not implement them twice or invent percentage limits.

## Implement

1. Freeze the exact ledger rows and direct typed group names before coding. Preserve RS names where meanings match, including `prepareDriverRead`, `prepareDriverSettings`, `nextDriver`, `advanceDriver` and `getDriver` where appropriate. Give genuinely different native groups clear typed names; a raw object accessor is not their public API.
2. Read a group through the established read/service boundary. Validate a complete candidate before yielding any write: types, ranges, reserved bits, cross-field constraints, supported firmware and required drive state. Rejection leaves output and drive work unchanged.
3. Keep current, current limit, holding behavior, ramp time and rate units explicit. Lichuan ramp objects documented in milliseconds do not become physical acceleration automatically. Do not import ESS RPM widths, limits, subdivision defaults or encoder assumptions.
4. Use a concrete bounded sequence for each cohesive group. Retain the individual writes confirmed before a later failure, unknown write effects and the applicable readback. Readback uses the same type/unit interpretation as the write. No implicit save, reset, enable, fault clear or second retry path.
5. Apply confirmed or uncertain configuration effects to dependent generations. A subdivision or scale change invalidates affected position/reference evidence and prepared targets. Soft-limit validation preserves the chosen coordinate frame. Parameter snapshots cannot be reused after contradictory readback or a changed binding/configuration.
6. Complete the matching coverage entries with evidence, API, implementation, tests, intended CLI route, hardware status and blocker. Keep unresolved writes visible while allowing independently established reads; do not silently weaken a group to arbitrary integer writes.

## Verify and exit

Test complete-candidate rejection before first TX, every supported boundary and reserved-bit rule, cross-field failures, wrong context, immutable admitted settings, partial write failure, uncertain final write, readback mismatch and configuration/reference invalidation. Verify no command is emitted for unsupported capability or invalid state. Include at least one native caller preparing and advancing a group without CLI support.

Exit with every confirmed assigned group implemented, documented and tested, including its read and failure paths. A source conflict stays explicitly blocked for that field; do not mark an omitted confirmed setting complete. Run focused native, generated-freshness, consumer and Doxygen checks appropriate to these changes.

## Documentation and delegation

Apply [DOCUMENTATION](DOCUMENTATION.md), [MAINTAINABILITY](MAINTAINABILITY.md) and [DELEGATION](DELEGATION.md). Update typed settings examples and Doxygen for units, whole-candidate validation, partial effects and readback. Ordinary coding agents may own disjoint settled groups; reserve stronger review for scale/reference invalidation and uncertain writes. Keep shared context and ledger changes with one owner.

Record closure in `docs/reports/prompt_20.md`, update affected coverage and user documentation, then commit and sync on the existing execution branch. Commit and sync useful verified group checkpoints during the prompt when complete.
