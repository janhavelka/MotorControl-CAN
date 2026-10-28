> Historical v1 promptset (00-19), retained for traceability. Do not execute this copy. Use the [current prompt index](../../README.md). Relative documentation links were relocated; task wording is retained from `dc8270740d0e16f19e170e2a982f67389c8a2c56`.

# Stage 19 Complete documentation, packaging and release audit

Apply [COMMON.md](COMMON.md). Prerequisites: stages 00–17; stage 18 is required for any claimed physical qualification.

## Audit

Refresh current RS release/package/source map/verification conventions and the relevant I2C release/checker patterns. Inspect actual target code, all public consumers, CLI inventory, Python runners, generated ledger, stage reports, CI and retained hardware evidence. Audit implementation against the user's whole-library requirements, not just this stage's changed files.

## Deliver

1. Write complete getting-started/native consumer, API, units, profile/dialect, CAN owner/service, shared CLI/JSONL, Arduino/IDF setup, Python runner, commissioning and troubleshooting docs. Include ordinary C++ read/move/stop examples calling the same public functions as CLI. Keep board/runtime details out of core integration instructions.
2. Finalize the native object/function coverage matrix for every supported family entry. Every confirmed in-scope function has a typed public path and tests, CLI exposure or stated deliberate API-only rationale, and separate hardware status. Missing confirmed implementation is unfinished work; documentary ambiguity is an explicitly scoped gap, not full support.
3. Audit dependency/license/export boundaries. Preserve MIT attribution for adapted code and stack licenses where used. Exclude vendor PDFs/EDS, extracted full text, source snapshots, test tools and hardware logs from core software packages. Verify third-party reference redistribution separately before any public publishing; local research copies do not license republication.
4. Check one version source, generated freshness, clean CMake source/install/export usage, PlatformIO, native-IDF component and standalone Arduino/IDF examples. Core consumers require no sibling checkout, Python, PDF, SDK or downloaded stack unless explicitly opting into an adapter.
5. Update changelog, release scope, backlog/roadmap, AGENTS and completed prompt reports. Archive prompts only when their actual implementation scope is complete; preserve unperformed HIL/open documentation obligations as actionable tasks. Mark a software release candidate accurately when qualification is pending.
6. Perform a final independent review of naming/ownership/status/health, no side effects on cached reads/startup, stop/uncertainty/queue retirement, source contradictions, resource bounds, CLI/API parity and FieldCore independence. Resolve confirmed defects and rerun affected gates.
7. Verify the accepted multi-drive decision and `MULTIDRIVE.md` gates. Publish separate researched/fixture/implemented/qualified inventories and the new-drive playbook. Confirm the architecture accepts both stepper and servo configurations without forcing CANopen, full-step geometry, fixed RPM widths, or native acknowledgements into common APIs. Do not list N5, EPOS4 or another surveyed candidate as supported because its design fixture passed.

## Concrete implementation contract

Review the result as a new user and as a maintainer. Starting from README alone, verify that a native caller can install/include/link the library, prepare exact units, drive a concrete context with supplied events and inspect a terminal result. Separately verify firmware example build/configuration and Python offline/live instructions. Every copied command must be real and current.

Check these inventories against actual files and behavior:

- General files: README, docs index, CHANGELOG, CONTRIBUTING, SECURITY, LICENSE, AGENTS, metadata, Doxyfile and formatting guidance.
- Contracts: units/axis, profile/native coverage, service/owner/health, CLI/JSONL, verification, commissioning/bench, consumer handoff and new-profile procedure.
- Source documentation: public declarations/fields/enums, implementation invariants, adapter ownership and Python reusable helpers.
- Support: researched candidates, test fixtures, implemented profile capabilities, exact firmware builds and independently qualified hardware.
- Delivery: source/install/export consumers, optional SDK/stack dependencies, generated freshness, example parity and retained check evidence.

No release wording may flatten these categories into “supports all CAN motors” or “fully CiA402 compatible.” State the implemented services/modes and selected dialects. Each required confirmed ledger row must have real code and relevant tests; unresolved documentary fields stay explicit, with the affected claim withheld. Do not remove a feature row to make the matrix green.

Run strict Doxygen and maintained-example compilation. Confirm XML/HTML includes public free functions and generated declarations rather than an empty successful run. Check that links refer to maintained contracts, not only historical stage reports. Review the CHANGELOG for public semantics and migration impact, not a transcript of the agent's work.

The final handoff names exact versions/revision, check results, scope, remaining device facts, hardware limits and next steps. It remains useful without reading the chat. Publishing/tagging follows actual authorization, and local software completion never fabricates a remote CI result.

## Documentation and work split

Apply [DOCUMENTATION.md](DOCUMENTATION.md) and [MAINTAINABILITY.md](MAINTAINABILITY.md). Close documentation obligations accumulated at every stage; this is a final consistency review, not the first time API comments are written. Remove obsolete duplicated instructions after updating current references and preserving historical evidence.

Use [DELEGATION.md](DELEGATION.md) for independent package/user-doc and hard-contract reviews. The lead owns support language, integrated final verification and Git/release actions. Report unresolved review findings honestly instead of calling the library complete.

## Verify and exit

Run the final appropriate full verifier, package consumer checks and mandatory remote CI on the actual final commit when the repository has a remote. Retain exact logs/revision and list unavailable checks separately. Do not claim remote CI from an older green run or call a compile-only board hardware-tested.

The final report must state implemented models/firmware/dialects/services/functions, host/firmware/hardware evidence, unresolved limitations and how to consume the library. Do not create a release tag, publish packages or invent a remote merely because the docs describe a release workflow; follow the session's actual publishing authorization.

Completion means the authorized implementation scope and required checks are finished, with every support claim justified. A complete set of Markdown prompts is the planning deliverable; a complete executable library requires actual implementation evidence.
