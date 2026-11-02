# Independent audit of prompts 29 and 30

Audited 2026-11-02 at entry `992b2e22697569ed4096500c474fba44b30c0048` on
`promptset/motorcontrol-can`, upstream `origin/promptset/motorcontrol-can`.
Fresh fetch showed 0 ahead / 0 behind, an empty index and only three unrelated
untracked `.vscode` files. These were preserved. Promptset version 2/base
`7878f7c` and the execution branch are unchanged.

## Findings and corrections

The lead and three independent reviewers reread both original prompts in full
and inspected commits `accaa3e` and `1d68ccd`, current code, actual callers,
tests, build/export inventories and original device sources. No runtime defect,
incorrect bit transcription or unnecessary alternate engine was confirmed.

Two missing direct regression cases were added to `test/drive_state_test.cpp`:

- Passing the result's live `target` subobject as the getter's current target
  returns `ReadError::OVERLAP` without changing the result or read context. This
  tests a legal alias, without invalid object lifetimes or reinterpret casts.
- A boot-up frame received through the real owner advances the actual node
  epoch. Checked interpretation with that current epoch rejects historical data
  with `ReadError::TARGET`, preserving the raw context and earlier result. No
  additional request is transmitted.

The stale coverage R35 summary omitted delivered work after prompt 21. It now
links current progress: 1-17 Complete, 18 Blocked, 19-30 Partial, 31-50 Pending.
Production code, public contracts, source ledger and generated policy are
unchanged. The small test addition reuses the existing rig and snapshot helper;
no new fixture framework, storage or service implementation was needed.

## Original scope and evidence

| Scope | Audit conclusion |
| --- | --- |
| 29: finite harness and records | DeviceEvidence uses the ordinary ProbeApp and sole owner; construction/inspection is passive. Plans/reviews, exact requests, consumed terminals, failed evidence, traffic, loss counters and timing are retained in fixed storage. |
| 29: adverse outcomes | Exact-byte success, abort, timeout, stale reply, boot-up, interruption, trace loss, cancellation races and no replay are covered. Firmware keeps DEVICE provenance; experiment admission does not change generated read effects. |
| 29: conditional device work | Ordinary non-changing acquisition remains Q20 gated. The implemented experiment route is finite typed unknown-effect reads; state-changing sequences still require an exact independently reviewed fixture/action. UART ROM identification does not qualify CAN semantics. |
| 30: decoder | Original source layouts remain separate. Raw mode/status/monitor fields, unknown bits, disagreement and five acquisition intervals survive. Publication validates current target/epoch/age atomically through existing checked getters. |
| 30: enable | Position 6/7/15 with nominal 10 ms spacing is documented before a separate position trigger. Missing evidence is a producible initial/no-pending-motion boundary, not the sequence itself. Fresh mode/released/nonrunning observations or a new reservation do not prove old trajectory retirement. |
| 30: release | No applicable source specifies the CAN release command and its torque/reference effects. `6040=0` associated with low status bits zero is not that contract. |
| 30: remaining implementation | Concrete enable/release APIs, retained controlword/action evidence, owner lifecycle, stale-target and fault-at-each-step tests, and action deadline/cancel precedence remain pending. Decoder tests cannot discharge those requirements. |

Both stages remain **Partial** with their existing precise gates. No source-backed
production action was found hidden behind an overly broad blocker. Unknown
prerequisites were not replaced with caller permission flags or generic CiA402
semantics. The existing read contract makes consumption at the overall cutoff
settle DEADLINE, even for an earlier service terminal; future action tests must
state their precedence explicitly rather than assuming RS timing behavior.

The original EN86 pp8,11-13,15,17-18; ZH86 pp8,11-13,15,18-19; ZH57 p12;
OP86 pp9-11 and relevant EDS sections were reopened. State/monitor diagrams and
conflicting output tables were visually inspected. All five original hashes
match the canonical ledger. The alarm/motion layouts, monitor release inversion,
positive mode encodings and WO controlword access match the implemented scope.
No device fact was corrected or physically measured during this audit.

## Current ecosystem inspection

| Checkout / final revision | Actual source seams and delta |
| --- | --- |
| MotorControl-RS `363255a5b4cbf3d539c2756ad48d84a25a5cfb36` | `ActionOperation.h`, ESS `Actions.cpp`, action tests, `_bench_evidence.py`, `bench_session.py` and verifier workflow. Separate acknowledgement/observation and uncertain effects remain relevant; ESS release values do not transfer. Production action/bench sources unchanged since `2642c30`. |
| ADS1115 `235af3e8c722661a89a44766a4ccd903a654f6fe` | Owner example, `takeResult`, atomic sample/token tests, HIL verdicts and CI. Explicit result consumption is retained; sensor retry/recovery is not imported. Unchanged. |
| INA228 `b28c5ea69bcb4d26b351c52c24d3c6fae9fd282b` | Hardware invalidation, checked identity, tracked diagnostic reads, validity/destructive-read tests and CI. Current validity is revoked without erasing history; actual sensor consuming reads do not establish Lichuan read effects. Unchanged. |
| FieldCore-node `5ad9292dc8481cf08be9a61f662495a6474d1da0` | Motor module admission/wire/setup/trigger/completion projections, actual host fixture and verifier. Health and observed completion do not replace execution evidence. Unchanged and read-only. |

Initial RS inspection saw `9849bd9` with concurrent TL57 prompt-document edits;
these were committed by the other session before final inspection. All final
trees were clean. No uncommitted behavior was attributed to HEAD, and no sibling
was changed or tested by this audit.

The handoff recheck found RS at `6145bd8a12eda501cd1b3dcbf561b8ce39373a61`, clean.
Its two additional commits change five prompt/audit documentation files only;
the inspected production and test seams remain unchanged. A concurrent untracked
`docs/audit/PROMPTSET_AUDIT_34_50_2026-11-02.md` appeared in this checkout during
verification. It is outside this assignment and is left untouched and uncommitted.

## Verification and handoff

Independent focused checks pass: DeviceEvidence 1,468, firmware probe 910 and
state interpretation 6,953 assertions. The native evidence example passes.
An independent quick verifier passed 52/52 selected CTests; quick mode explicitly
omits package and Doxygen. Integrated full verification passed:

| Check | Result |
| --- | --- |
| `python scripts/verify.py --mode full --compiler gcc --build-dir .build/audit2930-full` | PASS: 53/53 CTests, package consumers and strict Doxygen 1.13.2 |
| Exported/relocated package | PASS: independent headers, C++11/C++17/no-RTTI, isolated native applications and IDF CMake boundary |
| `python scripts/check_promptset.py` | PASS: local links, prompt inventory and 137 original hashes |
| `python scripts/check_lichuan_ledger.py` / `generate_lichuan_objects.py --check` | PASS: source/scope and generated freshness unchanged |
| clang-format 14 and `git diff --check` | PASS for the added tests and combined changes |

The full run used GCC 15.1, CMake 4.0.1 and Python 3.12.10; exact commands/logs
are in `.build/audit2930-full/summary.json`, run
`20261102T113133172430Z-51456-mxaiuh9g`.
Local independent evidence also lives under `.build/audit2930-harness`,
`.build/audit2930-contracts` and `.build/audit2930-full`.

Strong source/scope review used `gpt-6-astra` high. Independent harness and
ecosystem/package/test reviews used `gpt-6.1-sol` high. The lead inspected the
actual production and test diffs and original diagrams, and owns integrated
checks/Git. Only the assigned test file was delegated for editing.

CHANGELOG, progress, coverage and both stage-report links accompany this audit.
No public behavior changed, so no new API documentation or example was required.
No live port, reset, flash or CAN operation occurred. Firmware rebuild/manual CI
dispatch is not required for this narrow test/documentation change; automatic
push checks remain enabled and are not claimed as passed before their results.
The handoff records the delivery commit and fresh upstream comparison.
