# Fresh independent audit of prompts 22 and 23

Date: 2026-10-30. Promptset v2, branch `promptset/motorcontrol-can`, upstream
`origin/promptset/motorcontrol-can`, original base `7878f7c`. Entry
`8926356f381f302467d0f54a15e551cc1830c72f` had a clean index/worktree; fresh fetch
showed 0 ahead/0 behind. Inspected both original prompts in full, their actual
implementation commits (`084c270` and `8926356`), current code, callers, tests,
ledger, generated policy, package registration, verifier and workflow. Previous
stage reports served as navigation, not proof of correctness.

## Findings and corrections

1. **Real-owner tuning failure coverage was incomplete.** The existing tuning
   suite tests each read row and settings failure directly against `SdoClient`.
   The new application-owner suite exercised the actual loop setting only on
   success. Prompt 22's resumed integration requires retained partial effects
   through that owner. Added actual `prepareControlSettings` scenarios using
   copied admission, scheduling, pre-TX notification and retained completion:
   guard rejection before possible TX, unconfirmed download with late ACK, and
   acknowledged download followed by
   either control readback field failing by abort, timeout or value mismatch.
   Assertions preserve the acknowledged prefix/timestamp, partial readback,
   uncertainty, immutable owner results, getter sentinels, one configuration
   withdrawal and exactly one download. Another node progresses while the result
   remains unread; an unread ACK prevents configuration synchronization.
2. **Unused request storage duplicated the node generation.** Removed the private
   `Slot::nodeGeneration` field and its assignment. The reservation still owns its
   lifecycle dependency, and the copied completion retains the original native
   generation. No validity check or evidence was removed. No new abstraction,
   service engine, receiver, node table or installed dependency was introduced.
3. **The tuning guide called its direct service fixture an application owner.**
   Corrected that distinction and linked the real owner failure-path tests.
   Updated the current owner guide, CHANGELOG and progress without promoting
   blocked prerequisites or physical qualification.

No additional functional defect was established in the owner, finite raw tuning
reads, complete-candidate validation, atomic getters or generated access policy.
The existing fairness, capacities, copied data, nonwrapping IDs, exact TX reports,
deadline/quarantine and passive event tests remain active. No urgent/cancel/
recovery API or motion implementation was added.

## Source review and disputed scope

Reopened original [EDS](../reference/vendor/LC_StepDriverCAN_V1.1.eds) `[2500]`
lines 1559–1566 and [EN86](../reference/vendor/86C_CANopen_user_manual_en.pdf)/
[ZH86](../reference/vendor/86C_CANopen_user_manual_zh.pdf) PDF p19 (printed 17),
including rendered tables. They establish U16/RW and selector values 0/1/2.
The 2500 row documents no drive-state precondition, activation interval or reset
sequence. EN86 p19 also confirms mA/percent/ms for 2502–2507 while 250C has no
time unit; the opaque 251A–2520 tail still has no reviewed manual semantics.

One reviewer proposed blocking every changed 2500 candidate because Q09 remains
open. A second independent review and the lead's original-page inspection found
no contradiction in the existing narrow native register transaction: explicit
applicable model, documented selector domain, continuous exclusion, conservative
configuration withdrawal, matching ACK and exact new-epoch readback. It claims
neither qualified physical loop activation nor a measured firmware contract.
No guessed disabled-state, encoder scale, delay or firmware predicate was added.
The guide now states that boundary explicitly. Q09 remains open for the other
unestablished tuning domains/effects and physical model/firmware qualification;
the 32 withheld mutations remain withheld. No ledger permission or source claim
was expanded, and no hardware measurement was made.

Error-history/communication ownership was rechecked independently against the
canonical ledger and its regression: 1003 reads remain 19; configuration reads
1005/1014/1017/1200 remain 19; PDO-record reads and communication
writes retain their named 38 assignment. This audit adds no catch-all group.

## Recurring sibling refresh

All current sibling worktrees are clean and revisions are unchanged from 22/23.
Actual implementations plus a caller or focused test were read; unchanged
sibling suites were not rerun.

| Checkout | HEAD | Actual inspected seam |
| --- | --- | --- |
| MotorControl-RS | `f812d7c1d364a03be86f02ae8f648dc8fca23d42` | Tuning.cpp:55–95, ControlSettings.cpp:4–33 and their validation tests; RtuBusOwner.cpp:75–146/367–406 copies/reserves before publication and cycles producer heads; ProbeApp.cpp:1611–1625/1911–1936 advances typed work outside owner. |
| ADS1115 | `235af3e8c722661a89a44766a4ccd903a654f6fe` | ADS1115.cpp:273–328 whole-candidate validation; owner-safe-poll main.cpp:246–279 checks retained terminal kind/sample before publication. |
| INA228 | `b28c5ea69bcb4d26b351c52c24d3c6fae9fd282b` | INA228.cpp:466–545/940–965 separates candidate validity and evidence withdrawal; test_basic.cpp:2523–2554 rejects invalid candidates without I/O; bringup CLI main.cpp:336–374 is the actual owner caller. |
| MB85RC | `1bf7c293549b1038b2d8f3b7a8d655fa74201e6e` | MB85RC.cpp:2017–2062/1272–1278 variant policy and retained results; test_basic.cpp:5483–5497 retains prefix/blocks admission; DiagnosticCore.h:143–170 consumes actual results. |
| FieldCore-node | `5ad9292dc8481cf08be9a61f662495a6474d1da0` | SettingsService.cpp:38–74 publishes whole candidates; Rs485MotorModule.cpp:50–75/340–392/640–679 and Rs485Task.cpp:1241–1296 retain identity/results while releasing bus waits; fake-task test:3581–3622 permits unrelated work. Read only. |

CAN retains RS-style non-consuming result/release, copied admitted data and
external operation stepping. Sensor retry/consuming getters, MB85RC borrowed
buffers, ESS register values and firmware requirements are not imported.
Selected-stack source/pin and existing SDO uncertainty behavior were also
reopened; the audit does not change that boundary.

Delegation: `audit22_tuning` (`gpt-6-astra`, high) reviewed finite coverage,
original sources, settings and unresolved effects. `audit23_owner`
(`gpt-6-astra`, high) independently traced owner correlation/lifetimes and
checked original selector sources. `audit2223_sources_tests` (`gpt-6.1-sol`, high)
refreshed live sibling seams and owned only the disjoint owner-test changes.
Lead verified findings against code/source/diff, removed unused storage, owned
documentation and integrated verification/Git. No worker Git, sibling or hardware
mutation occurred.

## Verification and handoff

Local tools: Python 3.12.10, GCC 15.1.0, CMake/CTest 4.0.1, Ninja 1.12.1,
Doxygen 1.13.2 and clang-format 14.0.0. Linux sanitizers/Clang remain unavailable
on this Windows host; the existing remote jobs provide those gates.

| Integrated check | Result |
| --- | --- |
| Strict Release C++11 build and focused owner/preview/SDO/reconciliation/network/NMT/PDO/read/settings/tuning suites | PASS, 10/10. Owner suite: 2,252 checks, including eight new actual settings failure scenarios. |
| `python scripts/verify.py --mode quick --build-dir .build/p2223-audit-quick` | PASS, 40/40 contracts; repository/version/object freshness and negative checker tests included. |
| `python scripts/verify.py --mode full --build-dir .build/p2223-audit-full` | PASS, 41/41 contracts plus independent package checks and strict Doxygen. |
| Isolated source/export/install consumers | PASS: C++11/C++17/no-RTTI, exact 22-header inventory; no application owner, Python, SDK, ledger or selected stack requirement in the core package. |
| Strict public documentation | PASS, 22 installed public headers plus the owner interface, 1,332 members and 61 free functions extracted. |
| `python scripts/check_promptset.py --check-git-ref HEAD`, repository and diff checks | PASS; all 137 protected artifacts, 99 indexes/202 typed entries, 53 manual functions and 24 conflicts retained. Staged bytes and committed HEAD are rechecked at delivery. |
| clang-format `--dry-run --Werror` on the changed owner/test files | PASS. |

Host measurement after removing unused storage: owner 2,840 bytes (previously
2,888), Completion unchanged at 272 bytes, Windows x64 GCC. This is not an
embedded memory or service-gap qualification. Existing entry baseline focused
checks also passed 6/6; independent owner/service checks passed 4/4, and final
independent delta review repeated all 2,252 owner assertions successfully.

The integrated local runs cover task-owned changes over entry `8926356` and
record that worktree rather than claiming the entry commit contains the audit.
Retained command/tool/status logs are
`.build/p2223-audit-quick/20261030T142740042717Z-59132-_bdl5gtf` and
`.build/p2223-audit-full/20261030T142740117495Z-62340-yq739qw2`; each parent has
`summary.json`. The final-commit CI receipt is retained separately in
`.build/p2223-audit-ci.json` after remote inspection. No test was disabled or
weakened and no prior evidence was deleted.

Prompt 22 remains Partial on its explicit unresolved mutation facts; 23 remains
Partial solely on inherited 19/Q20. This audit closes the confirmed software
coverage gap without changing those evidence limits. Urgent capacity, full
cancellation and controller recovery remain 24. No physical qualification,
firmware build or CAN traffic is claimed. Commit/sync and exact final-commit
CI evidence accompany the delivery handoff.
