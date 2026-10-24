# Prompt 3 Reviewed Lichuan semantics and family gates

Apply [COMMON.md](COMMON.md). Prerequisites: [2 Source inventory and ledger intake](2_source_ledger.md).

## Objective and boundaries

Turn the complete intake into a reviewed Lichuan dictionary, family capability matrix and bounded uncertainty map. Finish documentary review of the supplied scope here. Missing firmware evidence becomes a precise gate on affected behavior, never an invented default or a claim that an UNSUPPORTED stub completes the feature. Code generation and typed operations belong to later prompts.

## Read and audit

Refresh RS, ADS1115 and INA228 and inspect their relevant native-type, access and effect distinctions. Inspect current FieldCore configuration provenance read-only; compare MB85RC variants, MCP45HVX1 action/value distinctions and RV3032-C7 uncertain effects where useful. Read declarations, actual callers and focused tests. Follow [CONVENTIONS.md](CONVENTIONS.md#3-motion-typed-reads-and-terminal-evidence).

Read original EN86/ZH86 and 57-series manuals together with EDS, concentrating on exact pages identified in intake. Do not use generic CiA402 semantics or another manufacturer's manual to resolve Lichuan contradictions. Apply [MULTIDRIVE.md](MULTIDRIVE.md#required-contrasting-test-cases) when distinguishing profile-specific objects from common intent.

### Concrete evidence to recheck

Review the [object conflicts](../audit/LICHUAN_OBJECT_FACTS.md) and [motion conflicts](../audit/LICHUAN_MOTION_FACTS.md) row by row. Concrete blockers include `0x2400` digital-output subindices/function numbers, three differing status layouts, `0x60FF` signedness/start order, mode U8 versus I8, and monitoring-object access. EN86's instruction to set `0x6061` does not override its read-only observation role. `0x605A/0x6085` are absent from retained dictionaries; `0x605D` exists without usable option semantics. Preserve each conflicting claim and gate only the dependent field/operation; a merged union of values is not a resolution.

## Implement

1. Review every ledger row's type, width, signedness, access, bounds/default, units, mapping eligibility, model/firmware scope, actual effect and prerequisite. Preserve a source/page or EDS section for each fact. An EDS read/write flag does not establish that an object is a harmless setting; an upload is not automatically non-consuming.
2. Publish decision tables for status bits 3/7/8/10/12/14/15, velocity start order and signedness, actual-position scale, `0x6083/0x6084/0x609A` millisecond ramp times, `0x2201` subdivision, `0x2202` soft limits, `0x2300` save/reset, `0x2301` alarm clear, `0x2302` position-clear edge, output encodings, `0x2400` reserved/filter fields and tuning fields with missing semantics. Each table states conflicting sources, consequences, chosen disposition and exact evidence still needed.
3. Resolve documented node versus broadcast semantics, DIP/parameter identity, bitrates including the EDS-only 50 kbit/s claim, default and configurable COB-IDs, required transfer forms, NMT/heartbeat/EMCY/SYNC and PDO limits. `0x1017` defaults to zero; absent `0x1016` evidence cannot establish drive action on host loss. All explicit EDS scalar widths are one to four bytes; this does not prove every server upload is expedited.
4. Define separate applicability/capability entries for CL86-C, OL86-C, CL57-C and OL57-C. Record motor/feedback differences and identity limitations. Generic EDS identity values cannot auto-detect every model; reviewed user-selected bindings retain provenance. Similar names do not prove common firmware or internal silicon.
5. Assign every semantic function to its actual implementation owner: typed reads 19; driver/motion/scale settings 20; I/O settings/output actions 21; tuning/nonpersistent groups 22; enable/release 30; stops 31; fault/reference actions 32; position 33; angle integration 34; velocity 35; homing 36; discovery 37; communication commissioning 38; persistence/reset 39. Use 12–18 for catalogue/protocol/PDO infrastructure. Do not leave an unbounded “remaining native features” bucket.
6. Produce readable dictionary/family coverage and a qualification checklist. Dispositions distinguish supported evidence, conservative read-only scope, pending firmware qualification and explicitly unsupported actual capability. Every open item names its smallest affected feature and required observation.

## Verify and exit

Validate the completed ledger and all intake accounting. Independently inspect representative original PDF tables and both disputed status diagrams. Check that unresolved types/effects cannot accidentally enter writable coverage. No save, factory reset or motion is an evidence-gathering action in this prompt. Run integrity, ledger and diff checks.

Close all documentary review scope in `docs/reports/prompt_3.md`; retain device-evidence obligations visibly without calling their features complete. Update PROGRESS, coverage and affected backlog/roadmap entries, then commit and sync the same branch. Useful reviewed ledger groups may be checkpointed with their checks and documentation.

## Documentation and delegation

Apply [DOCUMENTATION.md](DOCUMENTATION.md#source-evidence-for-this-contract) and [MAINTAINABILITY.md](MAINTAINABILITY.md#make-control-flow-and-state-reviewable). Under [DELEGATION.md](DELEGATION.md#task-brief-and-handoff), separate EN/ZH review from EDS checks; one lead merges the ledger. Use independent stronger reasoning review for status, trigger, stop and persistence contradictions. Reviewers return source-backed decisions, not guessed resolutions.
