# Prompt length and workload review — 24 October 2026

The active promptset now has **50 complete assignments numbered 1–50**, replacing the former 00–19 stages. Start with the [current index](../prompts/README.md) or [prompt 1](../prompts/1_source_audit.md). Library implementation has not started; every execution entry remains Pending.

## Findings

The old prompts were not unusually long as prose. Their main problem was the amount of independent engineering work inside one assignment: all typed reads/settings, multiple motion state machines, the entire owner and health system, several platform adapters, the complete CLI, or the whole runner family. These scopes made it easy to stop after one working subset while leaving tests, documentation or another subsystem unfinished.

The recurring reading requirement also loaded six shared documents totaling 11,254 words before root guidance, plans, audits and source code. Much of that was unrelated to an individual change. Shared rules remain authoritative, but COMMON now directs each prompt to the relevant sections, prerequisite reports and actual source delta. Every prompt still refreshes RS, ADS1115, INA228 and its affected I2C/FieldCore seam. Source searches alone are not treated as audits.

| Measurement | Previous assignments | Current assignments |
| --- | --- | --- |
| Number of executable prompts | 20, labeled 00–19 | 50, labeled 1–50 |
| Shortest body | 649 words | 531 words |
| Median body | 749 words | 638.5 words |
| Longest body | 988 words | 767 words |
| Scope unit | Frequently several subsystems | One bounded evidence, implementation or integration result |
| Reading route | Broad recurring full-document list | Current prompt, prerequisite outputs and relevant contract/source sections |

The current column includes the later concrete Lichuan evidence enrichment in 40 prompts. Immediately after the split, bodies measured 513 to 690 words, median 585.5; that checkpoint remains recorded in the JSON. Full source tables are reached through focused links, without adding new implementation scope or requiring every prompt to read the whole evidence pack.

Counts use whitespace-separated words, not a tokenizer or an estimate of coding time. The [per-prompt measurements](PROMPT_SIZING.json) list every original and replacement file. No word quota determines correctness, and no prompt can guarantee a particular model will finish within one context window. The useful limit is a coherent result whose implementation, tests and documentation can be reviewed together.

## Split and ordering decisions

[SPLIT_MAP.md](../prompts/SPLIT_MAP.md) maps every original stage to its replacement owners and explains the split. [SEQUENCE.json](../prompts/SEQUENCE.json) defines the current filenames, numerical order, explicit earlier prerequisites and original-stage ownership. Old assignments and shared guidance are retained under [archive/v1](../prompts/archive/v1/README.md), clearly marked historical; their relative links were relocated. They are not extra implementation work or required recurring reading.

- Source inventory, reviewed Lichuan semantics and alternate-drive evidence have separate exits. The reviewed ledger assigns each function to an actual implementation owner before code generation.
- Stack selection, API/ownership proof, native packaging and verification/documentation tooling are separate results. Exact arithmetic precedes axis/reference composition.
- SDO lifecycle, reconciliation, passive network events, NMT eligibility, PDO observations and remapping have separate failure tests. The first usable service subset already preserves deadlines, uncertainty and unresolved channels; later reviews do not defer basic correctness.
- Reads and three named settings groups replace one broad native-surface task. Enable/release, stop, fault/reference actions, position, angles, velocity and homing each close their own contract. Coverage reconciliation does not become a catch-all implementation stage.
- Owner dispatch, urgent/recovery mechanics and health precede native/controller/firmware examples and early device-evidence work. Early owner tests use concrete test-only work, without installing fake motion profiles. Actual stop integration belongs to its later operation prompt.
- The evidence-readiness prompt retains a conditional isolated experiment route for facts that block motion. It can be resumed with exact source/action/fixture/bounds, independent stop/observation provisions and authorization, without requiring the completed motion API or later CLI/campaign. Production capabilities remain gated; missing evidence is not a successful qualification.
- CLI work is split into parser/local routes, typed device routes and motion/platform parity. Help exposes only implemented routes. Python Console/evidence primitives precede the finite campaign/session family, preserving all six established module responsibilities.
- Fault integration, consumer/contrast coverage, physical qualification, documentation/package review and final candidate verification have separate exits. Hardware prompts remain pending/blocked without evidence; software candidate review cannot fabricate physical support.

## What every prompt carries

Each numbered file has linked prerequisites, an objective and explicit boundaries, targeted source reading, concrete files/APIs/behavior, focused acceptance checks, documentation/Doxygen duties, a useful subagent/reviewer split and `docs/reports/prompt_N.md`. Model selection remains based on runtime availability and task complexity. The lead owns shared contracts, integration and Git.

COMMON requires all owned deliverables and required checks before Complete. Success stubs, TODOs, unimplemented confirmed ledger rows and silent deferral do not satisfy the exit. A missing external fact is a precise Partial/Blocked result, with verified independent work retained. Later prompts can proceed only when their prerequisites are met. The single-prompt entry point stops after its handoff; whole-sequence execution continues only when assigned.

The Git rule remains unchanged: create one new dedicated branch at execution start, keep it throughout the promptset and resumed sessions, and commit/sync every prompt and useful completed checkpoints. This rewrite continued on the existing maintenance branch `audit/can-promptset-2026-10-24`; it did not begin prompt 1 or merge to the base branch.

## Review and validation

Three agents audited and authored disjoint foundation, operation and integration groups. Each then reviewed a different group against its original assignments. Cross-review corrected explicit prerequisites for NMT/PDO, settings-owner and I/O CLI work; the exact-arithmetic source route; duplicated `units_preview` creation; and the early experimental-evidence dependency cycle. The integrated correction to that cycle received a further independent read-only review.

Live RS, ADS1115, INA228 and FieldCore revisions remained the same as the [preceding correctness audit](PROMPTSET_REVIEW_2026-10-24.md), with clean sibling worktrees at inspection. Relevant source/guidance and RS Console/session ownership were reread. No sibling edit, CAN implementation, firmware run or hardware action was performed.

The updated [planning validator](../../scripts/check_promptset.py) checks consecutive natural IDs, exact active-file inventory, earlier-only prerequisites, matching prerequisite links/index/progress rows, original-stage ownership, every prompt's sections/contracts/report/commit-sync closure, all 50 multi-drive gates, all 35 requirements, maintained Markdown links and the 123 retained artifact hashes after evidence enrichment. [Focused regression checks](../../scripts/check_promptset_test.py) cover the current valid sequence and rejection of a missing prompt, forward dependency, omitted prerequisite link, missing commit/sync closure lost original scope, a corrupted EDS default and a missing scalar. The validator also compares the complete EDS intake fields, addresses, widths, source line spans and counts against the original file. These checks mutate only in-memory reads.

Run `python scripts/check_promptset.py --check-git-ref HEAD` and `python scripts/check_promptset_test.py`. [PLANNING_VALIDATION.json](PLANNING_VALIDATION.json) records package integrity. These checks validate assignment structure and retained evidence, not the correctness of future motor-control code. Independent content review remains necessary when changing the scopes.
