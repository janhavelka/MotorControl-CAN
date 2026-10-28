# Copyable execution assignments

Use the [numbered index](README.md). Filenames use ordinary integers **1-50**, without a stage 0 or lettered subprompts. The index is the execution order; alphabetical file sorting is not. Each prompt carries its concrete scope, source audit, dependencies, checks, documentation and delegation guidance through COMMON and the named contracts.

## Execute one prompt

Replace N with the next prompt number and give the agent this text together with that numbered file:

```text
Execute prompt N from docs/prompts/README.md in this repository. Read root
AGENTS.md, docs/prompts/COMMON.md and that prompt first. Verify its listed
prerequisites and current source evidence. Complete all its deliverables,
focused tests, affected documentation/Doxygen and its report. Use subagents
and available model/effort choices where useful, with disjoint ownership and
lead integration. Do not execute later prompts in this assignment.

If this begins a new promptset execution, create and record one new dedicated
branch from the inspected agreed base. Otherwise reuse the recorded branch.
Commit and sync meaningful completed chunks and all completed work before
handoff; verify upstream synchronization. Do not mark the prompt complete with
unfinished owned work or failed required checks. Report any external blocker
precisely and preserve the remaining obligation instead of inventing evidence.
```

## Execute the whole sequence or a range

```text
Implement MotorControl-CAN by executing prompts 1 through 50 from
 docs/prompts/README.md in numerical order, applying COMMON and root AGENTS.
For a requested range, execute only that range after checking its prerequisites.
Finish, test, document, commit and sync each ready prompt before proceeding.
If blocked, retain its Partial/Blocked status and exact remaining work, commit
and sync completed chunks, then continue the next independent ready work within
the assignment under COMMON. Return to earlier work when its evidence arrives.
Never mark unmet prerequisites satisfied or unfinished work Complete.
Keep one newly created execution branch throughout all prompts and resumed
sessions. Continue already-authorized independent work without routine
confirmation; retain blocked work and qualification gaps explicitly.
```

The result is one framework-independent C++11 library for CAN-controlled stepper and servo drives, Lichuan C over Classical CANopen first. Reuse audited RS naming/ownership, exact units, typed public functions, shared Arduino/ESP-IDF application/CLI, Python runners and native/package verification. FieldCore is a reference consumer, not a dependency. N5/EPOS4 are design contrasts; other CAN protocols remain future profiles. Use the original manuals and evidence ledger rather than generic CiA402 assumptions to settle device behavior.

Keep the common contracts authoritative: [CONVENTIONS](CONVENTIONS.md), [DOCUMENTATION](DOCUMENTATION.md), [MAINTAINABILITY](MAINTAINABILITY.md), [DELEGATION](DELEGATION.md) and [MULTIDRIVE](MULTIDRIVE.md). Read relevant sections and live source deltas each prompt. Write simple engineering language and readable code; no speculative framework or duplicated owner. Required public Doxygen and durable docs accompany each change; prompt 8 establishes strict tooling.

Do not edit sibling repositories or borrow RS bench authorization, wiring or motor limits. Actual CAN hardware work needs the established fixture and applicable authorization. Prompts 29 and 47-48 distinguish host readiness from physical qualification. No fake test, build or missing-feature stub proves real drive support. Final software and hardware verdicts remain separate.
