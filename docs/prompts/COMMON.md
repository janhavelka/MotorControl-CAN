# Common execution contract

Every numbered prompt incorporates this contract and root [AGENTS.md](../../AGENTS.md). The active sequence is [SEQUENCE.json](SEQUENCE.json): execute prompts **1 through 50** in numerical order. Historical 00-19 files in `archive/v1/` are traceability records, not additional assignments. Each current prompt is one complete work unit with its own tests, documentation and commit/sync gate.

## Read and prepare

1. Read root/applicable parent AGENTS, this contract, the active prompt and the reports for its listed prerequisites. Inspect the current branch, upstream, worktree and incoming changes. Create one new dedicated branch only when starting a new promptset execution; record its identity/base/upstream in PROGRESS and retain it throughout all prompts and resumed sessions. Maintenance of these prompts does not start implementation.
2. Prompt 1 establishes the source map and shared decisions. Thereafter read the active prompt's named sections of [CONVENTIONS.md](CONVENTIONS.md), [DOCUMENTATION.md](DOCUMENTATION.md), [MAINTAINABILITY.md](MAINTAINABILITY.md), [DELEGATION.md](DELEGATION.md) and its [MULTIDRIVE.md](MULTIDRIVE.md) row. Read relevant requirement/architecture/API-map sections and changed contracts, not every historical report. These contracts remain binding; a short reading route does not waive them. In a fresh session, reload the relevant contracts rather than relying on remembered identifiers.
3. For **every prompt**, refresh live MotorControl-RS, ADS1115 and INA228 revisions/dirty state and inspect the actual code, callers/tests/docs relevant to this work. Add the named I2C libraries and FieldCore consumer seam read-only. Record the delta. A symbol search locates evidence; read enough implementation to establish meaning. Unchanged unrelated modules and suites need not be reread or rerun. If a source is unavailable, identify the limit and use the pinned evidence only for its covered seam.
4. For device facts, verify the applicable original PDF/EDS/hash and exact pages. CL86-C is a finished drive, not an identified IC. Keep conflicting firmware/model facts explicit. Check predecessor outputs actually exist and passed; do not assume a report means unfinished code is usable.

## Implement the bounded result

Use the active prompt's objective and owned ledger rows as its scope. Keep code, meaningful tests, Doxygen, affected guides/examples and CHANGELOG together. Follow the concrete RS names and selected CAN conventions. Prefer direct C++11 functions, fixed values and caller-owned contexts; no framework/core I/O, hidden time, heap, retry or speculative generic engine.

Keep one application receive owner and one selected CANopen service backend. Preserve unsolicited events, exact units, raw native errors, terminal-result retention and uncertainty. Preparation/validation publish atomically. A local token cannot correlate an indistinguishable late SDO reply. Queue admission/CAN TX, protocol acknowledgement and motion completion remain different facts. No startup/probe/recovery action silently enables, moves, clears faults, changes settings or replays uncertain work.

Common motion supports stepper and servo intent without forcing CANopen addressing/state, full-step geometry or fixed RPM widths into every profile. N5/EPOS4 and proprietary-frame contrasts remain small test fixtures, not advertised drivers. Preserve original sources/licenses and exclude reference material from core distribution.

Subagents are authorized under DELEGATION. Give disjoint files or read-only reviews and a complete bounded brief; choose only available models/efforts appropriate to complexity. Keep shared contracts and Git under the lead. Strong independent review is useful for arithmetic, state, deadlines, uncertainty and stop behavior. Solo execution remains valid when delegation is unavailable; the lead still reviews the whole result.

## Verify, finish and sync

Run the active prompt's focused acceptance checks and affected existing build/docs/CI gates. Test observable behavior, including errors and unchanged output where required. Do not postpone necessary tests to a later integration prompt. Later audits strengthen already-correct components. Hardware and fake/compile results are separate evidence.

Before marking **Complete**, close every owned deliverable and required check. No TODO, success stub, unimplemented confirmed ledger row, failed required check or silently deferred acceptance item remains in that prompt. Later prompts own only their explicitly named later work. A newly discovered task too large for the current scope requires an explicit revised assignment and traceability; do not quietly move it to a catch-all final audit.

Missing firmware facts, tools, authorization or hardware can block particular work. Record **Blocked** or **Partial**, exact unfulfilled criteria and the next evidence needed; commit verified independent chunks. Continue later independent prompts only when their listed prerequisites are satisfied. Do not mark a blocked dependency complete to continue. Prompt 29 retains an isolated, reviewed and explicitly authorized experimental route for facts that block motion; it does not depend on completed motion APIs or the full later campaign. Resume that evidence acquisition when its prerequisites arrive, then resume the blocked feature. Prompts 47-48 remain pending/blocked without physical evidence; software candidate prompts 49-50 can complete while reporting that qualification gap. This is not a claim that the entire library is qualified.

Write `docs/reports/prompt_N.md`: objective/owned scope; source revisions and changes; implemented artifacts and decisions; exact checks/results; documentation; blockers; next work; branch/commits/sync. Update [PROGRESS.md](PROGRESS.md). Update backlog/roadmap/coverage when their facts change; link the authoritative contract rather than copying the report everywhere.

**Commit and sync after every prompt**, and during it when a coherent tested chunk is ready. Keep the same execution branch. The lead stages only owned changes, reviews the diff, commits, pushes and verifies fresh upstream ahead/behind state. Preserve concurrent work; no force-push, empty no-op commits or base-branch merge without authorization. A failed sync stays an explicit blocker. Frequent pushes do not require manually dispatching full CI for small changes; use the root/parent impact-based policy.

When assigned one prompt, finish its handoff and stop. Continue through later prompts without routine confirmation only when the user assigned the whole sequence or a range.
