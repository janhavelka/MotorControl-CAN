# Delegation and model selection

The user authorizes subagents and model selection according to task complexity. Use them when independent work or an independent review will improve the result. The lead remains responsible for the integrated code, evidence and final answer. Routine commits/pushes are authorized under root AGENTS; delegation does not expand the implementation scope or authorize hardware activity, release publication or sibling edits.

OpenAI's multi-agent guidance recommends bounded independent tasks and warns that dependent work or shared edits can make delegation less useful. Its reasoning guidance distinguishes routine work from hard design/debugging and advises checking the effort values each model supports. The project policy below applies those principles to this library; it is an engineering choice, not a model benchmark. Sources checked 2026-10-23: [multi-agent guidance](https://developers.openai.com/api/docs/guides/responses-multi-agent) and [reasoning guidance](https://developers.openai.com/api/docs/guides/reasoning).

## Choose from the running environment

Inspect the models, reasoning options, concurrency limits and context-sharing rules actually exposed by the agent runtime. Do not guess an identifier, change the user's global configuration or install a tool merely to obtain a preferred model. If model overrides are unavailable, inherit the available model and keep the same task boundaries and review standard. State the fallback in the stage report; it does not block otherwise executable work.

The following is a dated starting point based on the models exposed in this authoring session. Re-evaluate availability when executing the promptset. It does not assert public API availability, price, account access or a permanent model ranking.

| Task complexity | Starting model/effort when exposed | Suitable work | Review requirement |
| --- | --- | --- | --- |
| Small and well specified | `gpt-6-luna`, medium | Link/path inventory, unchanged schema checks, bounded formatting, documentation cross-reference check | Lead checks output; no independent protocol decision |
| Normal engineering | `gpt-6.1-sol`, medium | Focused implementation under a fixed contract, Python/parser tests, examples, documentation updates | Lead reviews integrated behavior |
| Substantial source reasoning | `gpt-6.1-sol`, high | Cross-library audit, exact arithmetic, service implementation, adapter integration, API documentation | Independent review for state/ownership/side effects |
| Difficult or conflicting contracts | `gpt-6-astra`, high | Vendor contradictions, architecture, late-SDO ambiguity, uncertain motion/stop, final fault review | Lead reconciles findings against source and tests |

Use a stronger model or higher supported effort when the task needs it, especially after a concrete reasoning failure. Do not escalate to maximum effort for all tasks or delegate every helper to a separate agent. A smaller model may verify a settled table; it must not settle unresolved motor semantics because it is cheaper. No model can replace missing firmware evidence or a physical measurement. The lead can ask a second reviewer to challenge a difficult decision, but resolves disagreement using evidence rather than majority vote.

If an override requires a fresh context, give the agent a complete bounded brief. Do not combine an override with an incompatible full-history option. Never claim to have changed the current lead model when the runtime only permits child-model selection. Keep parallelism within the actual session limit; there is no requirement to use every slot.

## Task brief and handoff

Each delegated task receives this information in one readable brief:

```text
Goal and stage: one concrete result and why it is needed
Read first: applicable AGENTS, COMMON, CONVENTIONS, active prompt,
            DOCUMENTATION, MAINTAINABILITY and relevant evidence
Scope: owned files or read-only review; files another agent owns
Contract: selected names/types, prerequisites, unresolved facts
Sources: sibling paths/revisions and exact vendor IDs/pages to verify
Checks: focused commands and expected observable behavior
Output: changed files or findings with source locations, checks,
        remaining uncertainty and conflicts needing lead decision
Limits: same promptset branch; no worker Git mutations/sibling edits/hardware/
        release publication; preserve concurrent work
```

An independent reviewer should receive the contract and changed files, not instructions to endorse the author's conclusion. Ask for a failing scenario, a source contradiction or a missing acceptance condition. Return “no findings” when supported; do not invent issues to justify delegation. Short investigations return findings rather than unnecessary new report files.

Before concurrent edits, assign disjoint paths and name the owner of shared files. The lead owns public contract decisions, central headers, catalogue/schema generation, build/test registration, command inventory and shared Git operations. Keep all agents and stages on the same promptset branch; do not create agent branches or worktrees. Worker agents do not commit, push, reset, rebase, stash, switch branches, revert another agent's work or create remotes. They report overlapping changes immediately. No two workers edit a generated file and its source independently.

Read-only research can proceed while an implementer works, but dependent edits wait for the shared contract. A subagent may propose further delegation; it must respect the same scope and shared concurrency limit. Avoid nested delegation for small tasks. When a worker finishes, inspect its actual diff/output and run affected integration checks against the combined result. A worker's passing test does not prove the final tree passes.

The lead commits and syncs each coherent integrated chunk after its relevant checks, including useful checkpoints during a prompt and before finishing that prompt. Do not wait for every unrelated worker or the full promptset when a disjoint completed chunk is ready. Verify staged ownership and upstream synchronization under root AGENTS. A worker handoff must identify completed files/hunks, checks and remaining work so the lead can make a reviewable commit without including unfinished or unrelated edits.

## Stage-specific work split

These are useful splits, not a mandatory agent count. “Strong review” means the difficult-contract tier above or an available equivalent.

| Stage | Useful independent work | Lead or single writer responsibility |
| --- | --- | --- |
| 00 | RS audit, I2C/FieldCore audit, reference inventory | Reconcile concrete naming and changed revisions |
| 01 | EN/ZH source comparison, EDS accounting; strong conflict review | Canonical ledger and support decisions |
| 02 | Stack proof, alternate-drive contract review; strong architecture review | Public boundary, memory ownership and stack choice |
| 03 | Isolated consumer check, Doxygen/style configuration review | CMake/verifier/CI integration and metadata |
| 04 | Arithmetic vectors, independent overflow/rounding review | Public units/status/axis semantics |
| 05 | Independent wire vectors, generator checks | Frame/object API and generation source |
| 06 | Fake-service adversarial cases; strong correlation/deadline review | Channel state, stack adapter and service lifecycle |
| 07 | Event/PDO vectors, remap-failure review | Single routing path and node storage |
| 08 | Disjoint typed settings groups and source checks | Shared result types, access policy and coverage ledger |
| 09 | Independent transition/stop evidence review | Action sequence, urgent interruption and uncertainty |
| 10 | Exact-target review, motion timeline tests | Position handshake and retained result contract |
| 11 | Homing method evidence, velocity deadline review | Finite service/stop behavior and native operation scope |
| 12 | Fairness/capacity tests; strong stale-TX/owner review | Physical owner, health accounting and shared storage |
| 13 | Non-changing discovery tests, persistence effect review | Commissioning sequence and binding invalidation |
| 14 | Disjoint Arduino/IDF/SocketCAN adapter checks | Shared app behavior and adapter evidence contract |
| 15 | Parser/JSONL adversarial tests, help/coverage audit | Command inventory, schema and side-effect classification |
| 16 | Pure parser/evidence tests, scenario prerequisite review | Runner/session correlation and no-replay policy |
| 17 | Independent fault, API/coverage and portability reviews | Resolve findings and run integrated verification |
| 18 | Read-only evidence review and host regression tests | One authorized bench operator; no concurrent hardware commands |
| 19 | Independent support/documentation/package review | Release scope, exact revision/check evidence and final report |

Record task, model/effort actually used, file ownership and result in the normal stage report. If working alone, record that briefly. Do not create an agent-management subsystem or recurring report ceremony. Use short progress updates and plain engineering language; report a concrete finding and next action.
