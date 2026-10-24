# Prompt 22 Tuning and remaining nonpersistent groups

Apply [COMMON.md](COMMON.md). Prerequisites: [19](19_typed_reads.md), [20](20_driver_settings.md).

## Objective and boundaries

Complete the tuning/control and specifically assigned nonpersistent native groups left by prompts 19–21. The main documented accounting range is `0x2500–0x2520`; review actual subindices, types and access instead of assuming the whole numeric range is populated. “Remaining” means the explicit ledger assignment made before this prompt, not an open-ended invitation to add every future feature.

Use [CONVENTIONS](CONVENTIONS.md#3-motion-typed-reads-and-terminal-evidence) and [MULTIDRIVE](MULTIDRIVE.md). Communication writes belong to 38, persistence/reset to 39, drive actions to 30–32 and trajectories to 33–36. Do not create torque/current motion or cyclic APIs from tuning terminology.

## Read and audit

Refresh MotorControl-RS, ADS1115 and INA228 revisions and dirty state. Read RS `Tuning`, `ControlSettings`, typed getter callers and group validation tests; inspect MB85RC variant policy and INA228 configuration validity. Review relevant FieldCore parameter consumers read-only. Check original Lichuan manual pages against EDS entries for each assigned tuning or other native field, including read effects and firmware contradictions.

### Concrete evidence to recheck

The [complete tuning table](../audit/LICHUAN_OBJECT_FACTS.md) accounts for all 33 indexes `2500..2520`, each U16 RW/PDO-mappable with no EDS default/range. `2500` selects drive loop mode (0 ineffective, 1 open, 2 closed), distinct from motion mode `6060`. `2501` encoder-resolution convention is unspecified; `2502` uses mA, `2503..2506` percent and `2507` ms. Do not infer `250C` in-position time is ms merely from its name. `251A..2520` have EDS-only gain names without reviewed units/effects. Keep that tail unresolved rather than manufacturing meaningful setters from strings.

## Implement

1. List the finite assigned groups and their exact row ownership. Confirm every native row already has a named implementation prompt. Missing classification is a ledger defect to fix explicitly, not permission to silently enlarge this task or discard the row.
2. Implement each confirmed group through direct typed preparation, stepping and checked result access. Reuse established read/settings primitives while keeping group-specific validation clear. Do not introduce a generic runtime settings engine or close coverage using descriptors alone.
3. Validate the complete candidate before any write, including firmware scope, native signedness/width, ranges, reserved bits, units, state prerequisites and cross-field constraints. Preserve raw unknown values on reads without advertising their unproved interpretation.
4. Retain confirmed partial writes, uncertain effects and documented readback. Update only the generations justified by observed or unknown effects. No implicit save/reset, tuning reset, fault clear or motor enable. A read with a consuming effect must be explicit and cannot be used as automatic reconciliation.
5. Reconcile error-history and communication-record accounting with prompt 19 and 38. Ensure `0x1003`, `0x1005/0x1014/0x1017/0x1200` and PDO records have explicit read/write owners and coverage, rather than disappearing between groups. Implement only this prompt's assigned nonpersistent rows.
6. Update native coverage with evidence, public API, implementation/test locations, planned CLI route, hardware status and exact blocker. Mark confirmed-but-unimplemented separately from document conflicts and genuinely unsupported model capabilities.

## Verify and exit

Test every owned type, access/range rule, cross-field invalid candidate before TX, wrong context, atomic read output, partial writes, uncertain write, readback mismatch and generation invalidation. Use direct public API tests and independently derived vendor cases; generated descriptors cannot be their own sole oracle. Assert no hidden communication change or persistent action occurs.

Exit only when this finite assignment is complete. If additional confirmed work is discovered, give it a named owning prompt and return the affected prior prompt to pending; do not claim completeness or append an unlimited catch-all. Run focused suites, catalogue/coverage freshness and documentation checks.

## Documentation and delegation

Apply [DOCUMENTATION](DOCUMENTATION.md), [MAINTAINABILITY](MAINTAINABILITY.md) and [DELEGATION](DELEGATION.md). Document tuning units, validity and partial effects; keep source contradictions visible. Disjoint established groups suit normal coding agents. Use independent stronger review for ambiguous effects, with one owner for canonical ledger/generator edits.

Close scope in `docs/reports/prompt_22.md`, update coverage, Doxygen and affected user documentation, then commit and sync on the same execution branch. Useful complete verified groups may be synchronized as intermediate checkpoints.
