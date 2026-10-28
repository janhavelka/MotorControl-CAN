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
Goal and prompt: one concrete result and why it is needed
Read first: applicable AGENTS, COMMON, CONVENTIONS, active prompt and its named sections of DOCUMENTATION,
            MAINTAINABILITY and relevant evidence
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

Before concurrent edits, assign disjoint paths and name the owner of shared files. The lead owns public contract decisions, central headers, catalogue/schema generation, build/test registration, command inventory and shared Git operations. At a new promptset execution's start, the lead creates and records one dedicated branch; all agents/stages and resumed sessions use it for that whole execution. Do not create agent branches or worktrees. Worker agents do not commit, push, reset, rebase, stash, switch branches, revert another agent's work or create remotes. They report overlapping changes immediately. No two workers edit a generated file and its source independently.

Read-only research can proceed while an implementer works, but dependent edits wait for the shared contract. A subagent may propose further delegation; it must respect the same scope and shared concurrency limit. Avoid nested delegation for small tasks. When a worker finishes, inspect its actual diff/output and run affected integration checks against the combined result. A worker's passing test does not prove the final tree passes.

The lead commits and syncs each coherent integrated chunk after its relevant checks, including useful checkpoints during a prompt and before finishing that prompt. Do not wait for every unrelated worker or the full promptset when a disjoint completed chunk is ready. Verify staged ownership and upstream synchronization under root AGENTS. A worker handoff must identify completed files/hunks, checks and remaining work so the lead can make a reviewable commit without including unfinished or unrelated edits.

## Prompt-specific work split

Each numbered prompt gives its concrete work split and reviewer focus. These grouped defaults guide model/effort selection; they are not a required agent count or a second task list.

| Prompts | Useful independent work | Lead or single writer responsibility |
| --- | --- | --- |
| 1-4 | Bounded source audits, EDS accounting and independent conflict review | Canonical source map, ledger and applicability decisions |
| 5-10 | Stack fit, isolated consumers, independent units/ownership vectors | Public boundary, memory ownership, tooling and exact arithmetic |
| 11-18 | Independent protocol vectors, generator checks, adverse-event scenarios | One service implementation and channel/node storage; strong correlation/remap review |
| 19-22 | Disjoint reviewed typed groups and source/test checks | Shared descriptors, read/effect types and ledger ownership |
| 23-25 | Capacity/fairness tests and passive counter checks | One owner, TX retirement and uncertain effects; strong recovery review |
| 26-29 | Disjoint platform checks and read-only evidence review | Shared application/adapter contract and explicit fixture gates |
| 30-36 | Independent state/stop/trigger/timing/homing review | Concrete operation states and retained results; no concurrent writers in one state machine |
| 37-39 | Discovery tests and route/persistence failure cases | Binding generation, partial effects and reconciliation |
| 40-44 | Parser/schema negatives, coverage/help review and offline runner tests | Canonical command inventory, JSONL/Console correlation and no-replay policy |
| 45-46 | Independent faults, consumer/contrast/coverage reviews | Integrate fixes and run complete affected gates |
| 47-48 | Read-only evidence review and host regressions | One authorized bench operator; no concurrent hardware commands |
| 49-50 | Independent docs/package/support and hard-contract review | Exact candidate revision, actual validation and honest remaining scope |

Record actual task/model/effort, ownership and result briefly in `docs/reports/prompt_N.md`. Solo work is valid when appropriate. Do not create an agent-management subsystem or recurring report ceremony. The lead closes all owned work, verifies the integrated result, commits and syncs each prompt and coherent completed checkpoints on the same execution branch.
