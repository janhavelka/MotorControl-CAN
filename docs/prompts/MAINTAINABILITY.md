# Maintainability and readability contract

Apply with [COMMON.md](COMMON.md), [DOCUMENTATION.md](DOCUMENTATION.md) and the active numbered prompt. These are implementation/review requirements, not claims that a driver or its checks already exist. The source evidence and live revision/dirty-state snapshot are recorded in DOCUMENTATION; refresh the relevant implementations, callers, tests and workflows at each stage.

## Keep the smallest design that expresses the real contract

Prefer ordinary C++11 free functions, fixed value types, concrete caller-owned contexts and explicit control flow. Before adding code, identify what existing function, owner, type or table can be reused, simplified or removed. Select the smallest change that closes the actual behavior gap. Preserve meaningful error/evidence distinctions; reducing a motor outcome to a boolean is not simplification.

Do not implement a framework because the architecture diagram has several boxes. Add a module, abstraction, template, strategy, virtual interface or generated layer only when a current implemented requirement needs it. Explain its concrete callers, responsibility and advantage over a direct function/table. Two similar functions alone do not justify a generic engine if their ownership, evidence or side effects differ. Keep a few clear explicit operations when generalization would hide their protocol contract.

No speculative runtime plugin registry, dependency-injection container, universal transaction executor, generic result getter, event bus, reflection system or empty future profile. Do not make all profiles inherit CANopen concepts or split public APIs only by stepper/servo labels. Test-only N5/EPOS4/proprietary-protocol contrasts test a boundary; they do not justify shipping stub drivers. Evaluate the actual CANopen stack once at its service boundary; keep one active service implementation and one physical receiver.

## Structure and naming

- Keep each function/header/module about one responsibility a reviewer can name. Consolidate tiny cohesive types; split files when ownership/dependency/API boundaries demand it. No arbitrary line/function/file count limits and no one-function-per-file policy.
- Keep dependency direction visible: common intent/units → selected profile → service work/results → application owner/adapter. Core headers do not include framework, board, FieldCore or chosen-stack SDK headers. Private helpers stay private unless a real consumer needs them.
- Reuse MotorControl-RS names for the same meanings and ecosystem `Status`/`Err` helpers after the live audit. Prefer direct verbs and typed nouns. Preserve operation-specific `prepare...`, `next...`, `advance...` and checked read getters; do not introduce synonyms for the same phase.
- Make units/basis part of types or unmistakable field names. `timeoutUs`, `rampTimeMs` and `observedPosition` say more than `time`, `acceleration` or `value` when the latter could misstate the contract. Do not force RS RPM widths or full-step geometry into CAN/profile-neutral types.
- Use named `constexpr` constants for reviewed limits/masks, a scoped enum for a closed set and small structs for related facts. No new macro-based API or template metaprogramming where a typed value or ordinary overload works. Keep generated identifiers stable and reviewable.
- At stage 03 establish `.clang-format` and document the chosen style. Follow it for new code and changed regions; avoid repository-wide formatting in a behavioral stage. RS's inspected style and I2C's two-space Google-derived config differ: choose once, record the difference, and avoid style mixing or importing SDK-first include rules into the portable core.

## Make control flow and state reviewable

Validate a complete candidate before changing outputs/context or admitting work. Use short validation paths with meaningful status/detail, then a straightforward success path. Never hide mutation or I/O inside a predicate, getter, constructor, conversion or formatting helper. Preparation and pure parsing publish atomically on success; document any explicit count-reset exception.

Keep protocol phases explicit in the concrete operation context. Document legal transitions, terminal conditions and event correlation next to the state handling. A small switch or table can be clearer than a configurable engine. Avoid overlapping flags for mutually exclusive phases and duplicated “current” state in several owners. Retain independent facts when they really are independent: link state, NMT state, drive state, mode, freshness, fault, outcome and application health are not one enum.

Make generations, capacity bounds and evidence provenance visible where checked. Do not use a local generation to claim an SDO response has a wire correlation token. Reject wrong/stale evidence before publication; preserve raw native errors. Explain delicate ordering with a short reason, especially queue retirement, boot-up invalidation, stop interruption, partial remapping and cancellation after submitted work.

Keep error paths as explicit as success paths. Do not swallow an error with a fallback default, turn a timeout into success, clear uncertain effects during recovery or retry a motor write automatically. An unsupported capability fails before admission with a useful reason. Do not scatter defensive checks without an invariant: establish validation boundaries, validate untrusted input there, and use internal invariants only when the public path proves them. Assertions never replace public runtime validation or manufacture successful release behavior.

Fixed storage and bounded work are part of readability. State exact queue/frame/payload/channel/trace bounds and ownership. No heap, hidden clock/sleep, automatic retry, background worker or logging in the installed core. Use a simple bounded loop when the maximum is known; document the bound and budget behavior. Avoid unnecessary copies, but do not retain call-only borrowed buffers to avoid a small required copy. Admitted work/results own their data.

Keep one authoritative configuration/coverage source. Generate repetitive reviewed descriptors from it when the source is large enough to justify generation, and provide a deterministic check mode. Do not hand-maintain the same object widths/access rules in codecs, CLI and tests. Generated code must remain inspectable and must not drag descriptive catalogue strings into minimal core consumers. Keep actual behavior assertions independently derived from documented cases so tests do not merely trust the generator they test.

## Comments and documentation are engineering tools

Use [DOCUMENTATION.md](DOCUMENTATION.md) for public Doxygen and the documentation inventory. In source comments explain why: an overflow boundary, a protocol ordering requirement, what a timestamp proves, why a result remains uncertain or a vendor contradiction. A useful comment names the invariant/evidence; a redundant comment paraphrases the assignment. Do not produce a paragraph for each obvious statement or boilerplate tags with no semantics.

When code is hard to describe, first check whether the names/control flow/state can be made simpler. Keep the contract in the public header and implementation rationale beside the relevant code; link longer design decisions. Do not duplicate the same evolving contract in several docs or force readers to reconstruct current behavior from stage reports. Keep TODOs actionable with an issue/backlog pointer and missing evidence; never use TODO or an UNSUPPORTED stub as a completed deliverable.

Examples and diagnostics should demonstrate the same public operations a normal caller uses. Keep Arduino/IDF entry points and transport glue thin around the shared application. Keep Python scripts small, with explicit arguments, bounded subprocess/I/O, useful failures and lazy imports for optional live hardware dependencies. Prefer existing verifier/checker registration over a second overlapping runner or a proliferation of one-off scripts.

## Tests and refactoring

Choose tests from observable contracts, especially invalid input, unchanged output, boundaries/overflow, capacity exhaustion, wrong/stale events, terminal result retention and uncertain external effects. A refactor must retain those contracts; do not weaken assertions, thresholds or checks to make a simpler implementation green. Add a meaningful regression for a real defect, not a test duplicating a helper's internal sequence.

Reuse small clear fixtures for repeated setup, preserving the scenario's intent in each test. Avoid an elaborate test DSL or whole-system emulator when a small supplied event/transport fake proves the seam. Use a source/install consumer to expose accidental private dependencies and a traceable test-only contrast to expose protocol assumptions. Hardware evidence stays independent of host tests.

Refactor only the scope needed by the stage, and separate behavior changes from mechanical cleanup where review needs it. Remove superseded helpers/duplicate paths after updating real callers/tests/docs; do not leave two APIs active “for flexibility” without a compatibility requirement. Existing public compatibility changes need explicit migration and changelog entries. A fixed-resource performance claim needs a measurement/bound, not a clever optimization or larger architecture.

## Stage review and exit

During planning, record the proposed responsibility/owner and actual reused source. At exit, the stage report records a short maintainability review, integrated with its normal decisions/checks rather than a duplicate report:

```text
Reused/simplified/removed: paths, actual callers and reason
New modules/types/abstractions: present requirement, owner and simpler alternative
State/storage/dependency effects: capacities and any increased coupling
Readability review: units/names/control flow/comments and documentation updated
Checks: affected contract tests/consumer builds/docs/style, exact results
Deferred cleanup: only concrete gaps with an owning stage; otherwise N/A
```

Reviewers must be able to follow one ordinary operation and its failure/uncertainty path from public request through yielded work and retained result without a hidden second owner. If a new abstraction obscures that path, simplify it before exiting. Style checks support this review; no complexity score, code-coverage percentage, comment count or line quota replaces it.

Stage 02 records the architecture/abstraction decisions and actual storage ownership. Stage 03 establishes style, dependency/package guards and the shared verifier. Stages 04–13 keep concrete operations/capabilities and state handling small while documenting their exact contracts. Stages 14–16 preserve shared application/CLI/runner semantics and thin platform adapters. Stage 17 reviews duplicated ownership and adversarial contracts across consumers. Stage 19 removes obsolete paths, checks source/install usability and confirms maintainable docs without expanding release scope.

The evidence behind these choices is explicit rather than a universal sibling template: current RS uses concrete free-function operation contexts and no root formatter config; ADS1115's contribution rules require existing helpers, external serialization, bounded ownership and formatting; SHT3x's agent guidance explicitly prefers reuse/deletion and simple existing owners; FieldCore's guidance and actual motor module favor independent owner integration with generation checks. CAN keeps its stricter allocation-free C++11 installed core and its own motion uncertainty rules. It does not inherit sensor retry/replay policy, sibling Git authorization or FieldCore product types.
