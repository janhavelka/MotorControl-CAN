# Prompt 45 Adversarial ownership and motion integration

Apply [COMMON.md](COMMON.md). Prerequisites: [36](36_homing.md), [39](39_persistence.md), [44](44_python_campaigns.md).

## Objective and boundaries

Audit and strengthen end-to-end failure behavior across the implemented owner, services, motion and console/runners. Deliver reproducible counterexamples, root-cause fixes and focused regressions. Earlier prompts already require correct uncertainty, deadlines and queue retirement; this prompt verifies their integration rather than postponing those fundamentals. Independent package/consumer/drive-contrast audit follows in prompt 46. No hardware or sibling repository changes are required.

## Read and audit

Refresh current RS common motion, owner, health, result/cancellation and runner failure behavior; inspect affected ADS1115/INA228 owner/parser tests and FieldCore's actual motor result/lifetime boundary. Read target workflows, implemented public contracts, retained reports and known failures. Apply [CONVENTIONS.md](CONVENTIONS.md), [MULTIDRIVE.md](MULTIDRIVE.md), [DOCUMENTATION.md](DOCUMENTATION.md), [MAINTAINABILITY.md](MAINTAINABILITY.md) and [DELEGATION.md](DELEGATION.md). Use source code and observable behavior, not just previous authors' summaries.

### Concrete evidence to recheck

Include [public-code counterexamples](../audit/LICHUAN_PUBLIC_IMPLEMENTATIONS.md) and [source conflicts](../audit/LICHUAN_MOTION_FACTS.md): wrong SDO initiation command, homing width mismatch, cross-node cached status, old completion bits, write-to-mode-display, unconfirmed enable, hidden save and generic quick-stop-object assumptions. Test rejection/uncertainty through ordinary APIs and one owner. Numeric negative fixtures do not establish an undocumented drive response; report what is simulated separately from hardware captures.

## Implement

1. Build deterministic end-to-end traces around public APIs. Prepare/admit a move, lose the trigger response, request stop, deliver a late acknowledgement and inspect both retained results. Prove later stop/recovery never rewrites the original uncertain motion into success.
2. Exercise one node's active SDO while other nodes send PDO, heartbeat and EMCY. Combine observer/output/CPU pressure with bounded owner service and urgent stop admission at each operation phase. Include heartbeat loss, EMCY storms, bus-off, malformed/duplicate/delayed replies, stale same-object acknowledgements, controller queued TX and receive/result overflow. No second receiver or automatic write retry may appear in a fix.
3. Test boot-up, binding/configuration changes and PDO remapping between preparation and admission or during operation. Prove stale work and pre-recovery frames cannot execute as new intent. After a timed-out object write, a late response lacking an echoed value must not confirm a later different value.
4. Cover timestamp versus processing-time precedence, exact deadline boundaries, cancellation ordering and no-progress paths. Exercise result/caller-buffer lifetime through release, ID exhaustion and queue capacity. Check saturated counters and `clearStats` while a failure streak or uncertain result remains live; clearing counters must preserve health history and operation evidence.
5. Verify common RS parity through observable contracts: exact units, preparation without I/O, atomic publication on invalid calls, unsupported rejection, admission versus wire/execution, stale evidence and stop/cancel separation. Different namespaces/types do not require source or binary compatibility.
6. Use supported sanitizers, strict warnings and deterministic malformed-input/fuzz-style corpora for these paths. Inspect core no-heap/no-framework/no-I/O assumptions and measure contexts/queues/service work. Avoid arbitrary coverage quotas, tests that reproduce helpers or a new general testing framework. Preserve failing evidence and fix the cause rather than lengthening timeouts or hiding retries.

## Verify and exit

Each finding records source, observable consequence, required fix and regression. Run focused tests while fixing and the appropriate integrated native/Python suite once the combined changes are ready. Recheck changed Doxygen/generated data and affected examples. Major behavior changes require CI on the actual final commit under root policy; unavailable checks remain explicit. Do not declare success from a happy-path run or delete a known failure because it later passed. Remaining documentary/physical unknowns retain precise evidence tasks and are separate from unresolved software defects.

## Documentation and delegation

Update failure/uncertainty/owner contracts, coverage, relevant examples and CHANGELOG with fixes. Source comments explain non-obvious invariants and public Doxygen reflects actual results/lifetimes. Give an independent stronger reviewer the contracts and changed files to challenge late-event/stop/recovery assumptions. Normal engineering agents may own disjoint adversarial test files after shared behavior is agreed. The lead resolves findings and runs combined checks; worker success alone does not prove integration.

Close this prompt's owned audit/fix scope and checks; record `docs/reports/prompt_45.md`, retained failure evidence and remaining external gates. Commit and sync on the same recorded execution branch before handoff, including useful complete checkpoints when appropriate.
