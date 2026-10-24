# Prompt 50 Final independent release-candidate review

Apply [COMMON.md](COMMON.md). Prerequisites: [49](49_documentation_package_audit.md).

Physical claims additionally require the relevant completed evidence from [47](47_hardware_finite.md) and [48](48_hardware_stress.md).

## Objective and boundaries

Perform the final independent contract review, resolve confirmed defects and verify the exact final software candidate. Deliver a self-contained handoff with implementation scope, consumption instructions, revision/check evidence and remaining physical/vendor limitations. A completed software review does not complete pending hardware campaigns or justify a whole-library claim when confirmed software scope is still missing. No tag, package publication, base-branch merge or sibling change is authorized by this prompt.

## Read and audit

Refresh relevant RS release/ownership/result behavior, ADS1115/INA228 verification and documentation practice, and FieldCore's current consumer boundary read-only. Inspect actual final files, all numbered prompt reports, requirement/native/API/CLI coverage, pending issues, CI and hardware artifacts. Review the user's entire accepted scope rather than just the final diff. Apply [CONVENTIONS.md](CONVENTIONS.md), [MULTIDRIVE.md](MULTIDRIVE.md), [DOCUMENTATION.md](DOCUMENTATION.md), [MAINTAINABILITY.md](MAINTAINABILITY.md) and [DELEGATION.md](DELEGATION.md).

### Concrete evidence to recheck

Revisit [DEVICE_EVIDENCE.md](DEVICE_EVIDENCE.md), all owned ledger rows and factual corrections. Independently sample original PDF tables, raw EDS sections and pinned source lines against implementation/tests; don't review only generated summaries. Verify that unknown timing/access/effect claims never became defaults or unconditional capability. Check full 99-index/202-scalar accounting plus manual-only semantics, while keeping documented, implemented, host-tested and hardware-qualified counts distinct.

## Implement

1. Commission an independent review of exact naming/units, status/health/result separation, ownership/lifetimes, no side effects on startup/cached reads, queue retirement, stop/cancel distinction, uncertainty, source contradictions and bounded resources. Reviewers receive contracts and changed files, not an instruction to endorse prior decisions. Findings need a concrete consequence, source and reproducible counterexample where possible.
2. Review CLI/API/Python parity, frame/protocol boundaries and framework independence. Confirm no console-only feature, hidden retry, raw validation bypass, second receiver or mandatory SDK/FieldCore dependency entered the public core. Recheck the selected stack/profile boundary against the actual installed files.
3. Resolve confirmed defects with the simplest maintainable fix and matching tests/docs/Doxygen. Preserve uncertain hardware facts rather than supplying invented defaults. Do not remove coverage rows, relax tests or extend timeouts without a demonstrated cause to make the candidate appear complete.
4. Reconcile implemented model/firmware/dialect/services/functions with documented, host-tested, firmware-built and qualified evidence. N5/EPOS4/proprietary fixtures prove only their stated design contracts. Confirm the new-drive procedure and remaining extension limits; do not advertise universal stepper/servo/CANopen support.
5. Update final scope, backlog/roadmap, AGENTS where behavior changed, progress and release-candidate notes. Archive execution instructions only when their real scope is complete; retain pending qualification and source-conflict obligations as actionable tasks. Label the result a software candidate when physical evidence is outstanding.
6. Prepare a concise handoff that names exact versions/commit, native consumption and firmware/runner entry points, actual check results, implemented capabilities, known limits, unresolved measurements and next work. It must remain useful without the conversation or every intermediate report.

## Verify and exit

Run the final appropriate full verifier, native/Python suites, generated freshness, Doxygen, installed/package consumers and required example gates against the integrated candidate. Reuse already valid unchanged evidence where appropriate; rerun checks affected by any later fix. On a configured remote, verify required major-milestone CI on the actual final commit after synchronization; an older green run does not verify it. Retain exact revision/log evidence and state unavailable or unperformed checks honestly. Do not claim library completion while confirmed software defects or required software scope remain open.

If remote/network/toolchain failures prevent a check or sync, preserve completed local work and report the exact blocker and unpushed commits. Do not invent a remote, force-push, bypass hooks or change triggers to hide failures. Physical campaigns may remain pending while this software audit closes with an explicit limited candidate verdict; full physical qualification stays unfinished.

## Documentation and delegation

Use normal engineering reviewers for package/docs/coverage and stronger available reasoning for uncertain state/stop/ownership contracts. Follow [DELEGATION.md](DELEGATION.md): bounded read-only or disjoint work, lead-owned shared contracts/Git and integrated verification. Record actual model/effort or fallback. The final report separates resolved findings from external evidence gaps and includes source comments/docs fixed alongside code.

Close the owned software review and checks; record `docs/reports/prompt_50.md` and final progress truthfully. Commit and sync on the same recorded execution branch, verify fresh ahead/behind status and report exact commits, CI/check outcomes and any remaining work. Useful complete checkpoints remain allowed; publication requires its own authorization.
