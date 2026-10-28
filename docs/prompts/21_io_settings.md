# Prompt 21 Digital I/O settings and output actions

Apply [COMMON.md](COMMON.md). Prerequisites: [19](19_typed_reads.md), [20](20_driver_settings.md).

## Objective and boundaries

Complete the reviewed digital I/O group: typed observations, function/polarity/filter configuration and explicit supported output actions. This prompt owns the assigned `0x60FE`, `0x2400` and `0x2401–0x2407` rows at their actual subindices. These indices are accounting groups, not a blanket declaration that every field exists or is writable. Homing interpretation and position-reference actions remain prompts 36 and 32.

Apply [CONVENTIONS](CONVENTIONS.md#3-motion-typed-reads-and-terminal-evidence) and [MULTIDRIVE](MULTIDRIVE.md). Physical wiring, configured function, observed level and whether an operation requires that signal are independent facts.

## Read and audit

Refresh MotorControl-RS, ADS1115 and INA228 revisions and dirty state. Read RS I/O settings/actions, typed `getIo` callers and invalid-setting tests. Inspect PCA9555 output/observation validity and MCP45HVX1 explicit output controls; compare INA228 state validity. Review relevant FieldCore input projections read-only. Recheck original Lichuan tables and EDS for function numbers, polarity, masks, filter units, access and model applicability.

### Concrete evidence to recheck

The [rendered-source review](../audit/LICHUAN_OBJECT_FACTS.md) found a real address conflict: EDS and ZH86 p19 place DO polarity at `0x2400:0C`, DO0/1/2 at `:0D/:0E/:0F`; EN86 p8/p18, ZH86 p8 and OP86 p6 shift these to `:0B/:0C/:0D/:0E`. DO function 2/3 meanings also differ between wiring and dictionary tables. EDS filter defaults `50,40,100,4000,10,2,16` conflict with OP86's defaults of 2. These facts must not become a writable union or a reset recipe. Gate the actual address/function interpretation by firmware evidence.

## Implement

1. Freeze assigned rows and their read/write/action classification. Extend the established typed group/context pattern using RS names only for matching semantics. Provide checked `getIo` and explicit output-action functions where the ledger supports them. Keep input observation and output mutation visibly different.
2. Decode raw values without inventing a meaning for reserved or contradictory function codes. Retain unknown raw values and validity. Input level does not prove a terminal is wired or that its configured limit/stop function is disabled.
3. Validate complete settings candidates before first write, including mask widths, polarity, function compatibility, filter ranges, state requirements and family bounds. Preserve confirmed partial effects and uncertain writes. Readback must distinguish output value from function assignment and actual electrical observation.
4. Keep output value/mask operations constrained to reviewed subindex, bit and physical-terminal identities. ZH57 PDF p8 labels `60FE` bits16/17/18 DO0/1/2; p17 instead labels them DO1/2/3. Preserve this conflict, do not infer another output or transfer the numbering to 86C, and test explicit model-bound terminal selection. Do not manufacture a generic digital-port API that writes reserved bits or assumes every terminal can be both input and output.
5. Invalidate affected configuration and operation prerequisites after confirmed or uncertain reassignment. No startup, probe, recovery or test helper silently disables stops/limits or changes functions to make another operation pass. Unconnected I/O may remain unused; require physical signals only for operations that actually need them.
6. Close this prompt's read, settings and explicit output-action coverage. Record missing electrical/firmware facts precisely. Do not defer its confirmed output actions to a later “remaining native functions” task.

## Verify and exit

Cover every reviewed field and function code, boundary masks, read-only policy, wrong mode/model, invalid candidate before TX, partial reconfiguration, readback mismatch, stale observations and generation changes. Test output level versus configured function independently. Include an unused-input configuration and a method-dependent prerequisite fixture without pretending a fake signal qualifies hardware.

Assert no hidden factory reset, save, fault clear, enable or protective-input reassignment. Exercise direct public functions independently from the console. If resumed after owner construction, integrate and test confirmed groups/actions through that owner before closing this scope; prompt 45 checks combined paths. Exit with all confirmed I/O rows complete and unresolved fields gated; a raw accessor or unsupported placeholder does not satisfy typed coverage. Run focused native, documentation and existing settings gates.

## Documentation and delegation

Apply [DOCUMENTATION](DOCUMENTATION.md), [MAINTAINABILITY](MAINTAINABILITY.md) and [DELEGATION](DELEGATION.md). Document pin/function/level distinctions, explicit mutation and failure consequences in user guidance and Doxygen. A normal coding agent can implement established groups; an independent reviewer verifies source values and whole-candidate tests. Keep one owner for each mutating sequence and canonical ledger edits.

Record results in `docs/reports/prompt_21.md`, update coverage and affected examples, then commit and sync on the recorded execution branch. Completed verified read or settings groups may form earlier synchronized checkpoints.
