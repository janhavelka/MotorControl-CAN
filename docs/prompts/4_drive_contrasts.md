# Prompt 4 Stepper and servo contrast ledger

Apply [COMMON.md](COMMON.md). Prerequisites: [2 Source inventory and ledger intake](2_source_ledger.md), [3 Reviewed Lichuan semantics and family gates](3_lichuan_semantics.md).

## Objective and boundaries

Produce a small, source-backed comparison that prevents common motion types from becoming Lichuan-only or stepper-only. The deliverable is a reviewed contrast ledger and concrete design cases for subsequent proofs. Do not implement N5, EPOS4 or proprietary protocol drivers, copy their full dictionaries or expand the initial Lichuan product scope.

## Read and audit

Refresh RS, ADS1115 and INA228 and inspect their current variant/capability/type boundaries. Inspect FieldCore's logical axis, exact target and binding contracts read-only. Read RS Units/Axis and multiple profiles, including callers and boundary tests that reveal native RPM/count assumptions. Apply [CONVENTIONS.md](CONVENTIONS.md#3-motion-typed-reads-and-terminal-evidence), the multidrive decision and [MULTIDRIVE.md](MULTIDRIVE.md#required-contrasting-test-cases).

Use original available N5 and EPOS4 manuals and the supplied CANopen/proprietary surveys. Verify model, firmware and document scope before citing a value. Check primary ODrive, VESC and moteus material only at the framing, endpoint, acknowledgement and native-representation boundaries needed for extension cases. Missing sources remain explicit; a marketing page is not a wire specification.

### Concrete evidence to recheck

The [public research](../audit/LICHUAN_PUBLIC_IMPLEMENTATIONS.md) identifies newer CL2-86C/CL2-57C products with absolute-encoder interfaces and USB-C. Treat these as different applicability scopes from the original CL86-C incremental-encoder family. Do not import their I/O counts, scaling, objects or manuals merely because the manufacturer and name prefix match. Keep N5/EPOS4 contrasts as assigned; discovering another product does not add an implementation obligation here.

## Implement

1. Create a compact contrast ledger covering Lichuan C, N5 in stepper and BLDC configurations, and EPOS4 servo behavior. For each selected fact record source/page/version, native width/sign, command and feedback units, ramp interpretation, mode/capability, trigger/completion evidence, stop/brake/watchdog behavior and identity/configuration prerequisites. Keep motor kind, feedback and supported modes independent.
2. Record cases requiring common exact coordinates without mandatory full-step/subdivision geometry; independent command increments, encoder counts and load scale; generic signed velocity wider than ESS native RPM fields; logical subaxes independent of a seven-bit CANopen node; and explicit quantization for a documented native floating representation. Current, current limit and torque are not interchangeable quantities.
3. Compare Lichuan millisecond ramp-time objects with a source-backed different acceleration representation. Keep profile conversion responsible for the distinction. Record N5's eight versus Lichuan's four PDO sets, and the unrelated manufacturer meanings of `0x2300`. Neither a global four-PDO limit nor object-number-only action dispatch is acceptable.
4. Define test-only fixture inputs and expected common invariants for prompts 6, 9–11 and 46. Include real extended-ID/RTR frame shapes where documented, Classical-only FD rejection, absent native acknowledgement/completion, and protocol-specific state optionality. Clearly label vendor-backed values, protocol-format examples and abstract common invariants. Do not invent a missing manufacturer's state sequence to make a fixture compile.
5. Write a short boundary decision: common intent and endpoint identity remain protocol-neutral; CANopen native states and services belong below profiles; one physical interface has one receive owner. Multiple vendors on CANopen share one service engine. Mixed protocols on a bus require explicit compatibility analysis and are not enabled by the existence of this ledger.

## Verify and exit

Review each chosen example against its cited original and check that it challenges production types rather than requiring fake production drivers. Verify that a servo case has no full-step geometry and an endpoint/subaxis case does not assume generic node range 1–127. Ensure researched products cannot appear as installed or hardware-qualified support. Run promptset integrity and diff checks; no physical test or second full dictionary is required.

Close the contrast ledger and fixture specification in `docs/reports/prompt_4.md`, update PROGRESS and affected design/backlog facts, then commit and sync the same branch. Record inaccessible evidence and omit unsupported native claims while retaining the associated abstract invariant.

## Documentation and delegation

Follow [DOCUMENTATION.md](DOCUMENTATION.md#source-evidence-for-this-contract) and [MAINTAINABILITY.md](MAINTAINABILITY.md#keep-the-smallest-design-that-expresses-the-real-contract). Under [DELEGATION.md](DELEGATION.md#task-brief-and-handoff), N5 and EPOS4 source reviews can run independently. The lead merges common-type consequences; a stronger independent reviewer tries to expose a hidden CANopen, stepper or RPM assumption. Keep fixture work small and evidence-based.
