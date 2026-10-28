# Final planning review and source drift

This is the historical 23 October review. The later [24 October correctness and simplicity audit](PROMPTSET_REVIEW_2026-10-24.md) records additional corrections, including the exact velocity timer origin, stage dependencies and Git artifact preservation. Use the current prompts for execution.

Final review date: 23 October 2026. Two independent review passes checked the implementation prompts against the source audits and device research. Corrections incorporated into the package include:

- Literal RS result access through contexts and typed getters, actual `*_test.cpp`/`*_test.py` conventions, missing settings/subdivision/profile CLI groups, and active `capture-read` semantics.
- Immutable operation deadlines, NMT command settlement versus observed state, broadcast invalidation, known-state service eligibility and unknown-state read-only discovery.
- An early evidence phase for resolving firmware conflicts before dependent motion implementation, while keeping the later full hardware campaign gated by tested software.
- Explicit local-controller startup versus unsolicited device mutations, and controller-observed versus independently measured physical completion.

## Subsequent stepper and servo scope review

The user's expanded request adds multiple CAN drive manufacturers and both stepper and servo control. [The new decision](../multidrive_decision.md), [CANopen survey](CANOPEN_DRIVE_SURVEY.md), [other protocol survey](OTHER_CAN_PROTOCOLS.md) and [fresh source audit](MULTIDRIVE_SOURCE_REAUDIT.md) support one common motion library with independent protocol/profile layers. Lichuan remains first; N5/EPOS4 are required design contrasts. Updated prompts explicitly avoid stepper-only geometry, fixed RPM fields, mandatory CANopen state/address semantics, global four-PDO limits and shared vendor-object meanings.

A further independent protocol review tightened RTR DLC versus payload length, profile-specific heartbeat configuration, watchdog refresh even during finite trajectories, and pre-freeze N5/EPOS4 contract fixtures. No real second driver or hardware support was claimed. Four more CANopen PDFs and pinned proprietary-protocol references were saved; unavailable originals are explicitly listed in the supplemental manifests.

## Concurrent changes observed

The target directory had no Git repository at the first inspection. During authoring, a repository with initial commit `1d103c2` and origin `https://github.com/janhavelka/MotorControl-CAN.git` appeared. This audit did not initialize it, create the remote, commit or push. Existing committed reference URL lists were preserved when their temporary extraction copies had been removed during source-pack preparation. Future stages inspect current Git state instead of relying on the original empty-directory fact.

MotorControl-RS was clean when inspected, tested and copied at HEAD `5da4ba7f13be457ea7bf9706ffd7d546821669c7`. Later, concurrent documentation edits appeared with the same HEAD. The first live comparison found differences in `AGENTS.md`, `README.md`, `docs/profile_contract.md`, and `docs/source_map.md`; a subsequent comparison also found `docs/architecture.md`. The observed edits update FieldCore/RodTester integration descriptions; additional documentation/backlog/prompt changes outside the selective snapshot were also present. They were not edited or reverted by this audit. The live worktree can continue changing independently of this pinned deliverable.

The supplied source audit, 78-test verification and copied baseline describe the clean captured state. The original-byte snapshot hashes remain authoritative and pass. The optional live comparison reports the differences observed at its own check and is retained in [LIVE_BASELINE_COMPARISON.json](LIVE_BASELINE_COMPARISON.json). This is source drift, not corruption of the delivered artifacts. Future implementation must inspect the actual live changes, including current FieldCore source, before relying on changed descriptions.

## Local validation

Run `python scripts/check_promptset.py` for the offline planning package. It checks 20 stage prompts and concrete contracts, 20 multi-drive stage gates, 35 requirements, five shared contracts, three reference packs, maintained Markdown links/fences/tables, artifact hashes, the complete selective snapshot inventory and retained RS verification evidence. Original downloaded Markdown files retain their source-relative links and are checked by hash instead of rewritten. [PLANNING_VALIDATION.json](PLANNING_VALIDATION.json) records the final result.

The planning checker does not compile a CAN library, run firmware CI or qualify hardware. The current deliverable deliberately contains no installed MotorControlCAN implementation. No hardware access or sibling-source mutation was performed.

## Comprehensive prompt revision

The user's further request for concrete source-fitted instructions is implemented across all 20 stages. Each now names specific files/APIs, meaningful failure cases, documentation duties and useful work splits. New shared contracts select [conventions](../prompts/CONVENTIONS.md), [documentation/Doxygen](../prompts/DOCUMENTATION.md), [maintainability](../prompts/MAINTAINABILITY.md) and [delegation/model choices](../prompts/DELEGATION.md). COMMON, the copyable assignment, AGENTS, architecture/API maps and requirements all incorporate them. The planning CHANGELOG records actual delivered material; implementation progress remains pending.

Two source/documentation agents used `gpt-6.1-sol` with high reasoning; independent architecture/protocol review used `gpt-6-astra` with high reasoning. They owned disjoint documents or read-only review. The lead integrated shared contracts and stage files. Model recommendations in the delivered policy are dated runtime choices with explicit fallback, not permanent availability or pricing claims. Official multi-agent/reasoning guidance is linked in DELEGATION.

The [fresh conventions audit](CONVENTIONS_REAUDIT.md) records exact source paths, revisions and observed differences. The integrated review corrected:

- RS owner names to `admit/admitUrgent/service/result/release/cancel`, and preserved free `Ok()` / `Status::isOk()` rather than I2C aliases.
- Separate status/protocol/operation/health facts, exact proposed health field names, one admitted replied/confirmed service as the accounting unit, cancellation handling, ordered state projection and rebinding/reset lifetimes.
- SDO late-reply ambiguity for same-object writes even when values differ, conservative unresolved-channel behavior and explicit event/deadline precedence.
- Fixed velocity stop-due versus final-deadline semantics and separate later stop actions that preserve the original failed/uncertain result.
- Shared owner storage across stages 06/07/12, small stage-02 prototypes, and native coverage rows assigned to the stages that actually implement them.
- Python responsibility parity: RS `Console` is in `bench_probe.py`, session orchestration in `bench_session.py`, and campaigns in `bench_scenarios.py`.
- Doxygen wording that distinguishes an invalid/rejected call from an accepted failure event that validly changes an operation context.

The independent targeted closeout confirmed these findings resolved. This was source/document review, not runtime verification of a CAN implementation. The final local checker passed, including 103 hashed artifacts, and `git diff --check` passed with only configured LF/CRLF conversion notices.

Three focused in-memory negative checks also passed: the validator rejected a missing concrete stage section, a missing COMMON delegation-contract link and a removed R35 requirement. These checks changed only mocked reads, not repository files or reference bytes. They verify the new structural guards, not library behavior.

### Further concurrent source drift

The fresh conventions/doc audit inspected MotorControl-RS at `540e640c4bb7bff989082d765778fcaebfdf0d7e`. During review it advanced to `0c0d96227e928f1ea172e6ecebefe5710e124f97`, then the lead observed `e39c095a1f177cb3000df48396f77f9e60521a15`. Diff inspection from the audited revision showed reference/research/prompt/documentation additions and a reference-preparation script; the inspected installed headers, source, examples, runners and workflow were unchanged. The source findings therefore retain their original provenance. They are not new test results for these later commits.

FieldCore stayed at `da94ed5280ed42ee3a55e0d977f9cb6565c56b51` but developed unrelated working-tree documentation/guidance/reference and documentation-tool/test changes during review. They were preserved; no FieldCore CAN implementation was inferred. ADS1115 and INA228 retained the fresh audit revisions. Future stages must refresh live source again rather than treating any observed HEAD as permanently current. No commit, push, remote CI dispatch or sibling edit was made by this prompt revision.
