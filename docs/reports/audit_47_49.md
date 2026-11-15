# Fresh audit of prompts 47–49

Started on 2026-11-15 from clean, synchronized `main` at
`90db5609fe5e077380920e20f084ca1aaa452f7b`. Read all three original prompts in
full, their shared contracts and actual implementation diffs. Three independent
agents reviewed finite trials, load evidence/tooling and package/documentation;
the lead checked findings against source and retained artifacts. No hardware,
sibling mutation, release or later prompt was executed.

## Findings and corrections

| Scope | Finding and disposition |
| --- | --- |
| Prompt 47 finite trial changes | No confirmed software defect in the trace FIFO/tail, failure retention, allocator or terminal-reservation changes. Original trials 47301/47302 replay with 88 terminals and 264 traffic records each; exact seven downloads and 81 uploads remain consistent. Earlier lossy and allocation-failure captures remain immutable. |
| Prompt 48 request correlation | Confirmed bug: changing recorded duration, cadence, read set or transmitted request while retaining a matching trial number could still yield `complete`. The checker now compares supplied request fields and decoded bytes with `SOAK_BEGIN`, and checks the recorded image base. Malformed supplied metadata fails rather than supplying evidence. |
| Prompt 48 measurements | Confirmed gaps: malformed periodic/final vectors, impossible final step-success counts and reported diagnostic loss could retain a complete verdict. Vector shape checks apply to every row; comparisons between rows apply only to settled final evidence. Honest failures and cumulative owner observations remain distinct. |
| Prompt 48 finite host budget | Non-finite/negative reconnect and keepalive intervals were accepted. Port enumeration and backward wall-clock changes could extend the campaign. Invalid timing rejects before port access; enumeration and later admission consume one fixed monotonic budget. Existing finite capture/settlement grace is unchanged. |
| Prompt 48 qualification | The 64-byte deficit in campaign C remains incomplete and unlocalized; C2's passing repetition is no fix. Host-read pause is not link loss. Persistence/restart, physical fault and related-model trials remain unperformed. |
| Prompt 49 package/API/docs | No confirmed defect. Independent archive/install/README consumers, generated freshness, operation coverage and strict Doxygen pass. Descriptor process metadata remains absent from installed headers, retained in the ledger, with migration documented. |

The corrections reuse the existing evidence validator and finite runner.
Offline replay also determines the live runner's final verdict. No second parser/service engine,
request replay, firmware change or new device capability is introduced.
Optional historical metadata stays optional; its absence does not establish
that an unspecified workload ran. Tests mutate actual retained captures and
preserve original failures instead of replacing evidence with a synthetic pass.

## Requirements coverage and limits

| Original requirements | Verified scope / remaining obligation |
| --- | --- |
| 47.1–3 fixture, bounded plan, source conflicts | Original fixture/scenario/read evidence is retained with actual image identity and specimen scope. Missing nameplate/electrical and independent physical observations remain explicit. No conflict is resolved by simulation. |
| 47.4–5 motion, stop, mode transition and home | Two finite velocity/controller-stop trials have matched service evidence. Physical direction/travel/standstill, position/angle and home-to-move qualification remain pending with their original prerequisites; positive streamed JSON/Python motion also remains unqualified. |
| 47.6 failed-test investigation | Retained failed captures, focused fixes and later checks remain distinct. No new device-facing change requires another physical trial in this audit. |
| 48.1–4 plan, interruption, persistence and load | A/B/C/C2/D raw outcomes retain their separate bounds and verdicts. Host request/admitted-plan matching is corrected. Read load and console pause do not close physical link/fault/restart or persistence work. |
| 48.5–6 related models and causes | OL86-C/CL57-C/OL57-C fixtures remain unavailable. Serial-loss localization still needs simultaneous UART/bridge or USB evidence; no cause is invented. |
| 49.1–3 user workflow, general files and contracts | Literal README native install/build succeeds inside the exported archive; installed read/move/stop example exercises supplied events and missing-evidence rejection. Offline commands and API migration match code. |
| 49.4–6 coverage, versions and distribution | 202 EDS entries, 53 manual obligations and 191 public overloads reconcile. Archive/relocated install/core-only consumers and strict public documentation pass; vendor material and optional SDK/stack stay outside the core. |

Prompts 47 and 48 remain **Partial**. Prompt 49 retains its completed software
scope and inherited gates, including prompt 18/Q17. This audit cannot manufacture
the absent physical observations or related-model fixtures.

## Sources and independent review

[Source refresh](audit_47_49_sources.json) records current sibling revisions,
dirty state and working-file hashes. RS advanced since the earlier report;
the package/version/documentation files have identical working bytes to prompt 49.
Current finite-session/failure functions were read again. Concurrent RS group-coordinate
changes are recorded separately; the named failure functions retain their ASTs. ADS1115/INA228,
MCP45HVX1 and FieldCore retained their recorded seams; sensor restoration/retry,
RS addresses/timing and inferred FieldCore CAN support remain rejected.

| Sibling seam | Audit decision |
| --- | --- |
| RS `scripts/bench_session.py::run_recorded`, `bench_probe.py::observe_stopped/cleanup_stop_attempts` | Keep finite evidence and separate primary/cleanup outcomes; reject automatic motor restoration and RS device timings. |
| RS `CMakeLists.txt`, `scripts/verify.py`, `scripts/generate_version.py` | Package/version seam unchanged since prompt 49; keep explicit source/export boundaries and one version source. |
| ADS1115 `tools/run_i2c_hil.py::run_soak`, `Doxyfile` | Unchanged since prompts 48/49; retain bounded tests and strict documentation, not sensor recovery policy. |
| INA228 `tools/check_core_timing_guard.py`, `Doxyfile` | Unchanged since prompt 49; retain actual core-source checks and API extraction. |
| MCP45HVX1 `tools/run_hil_mcp45hvx1.py::capture_baseline/verdict` | Unchanged since prompt 48; a failed prerequisite or unsafe skip is not a pass. Its restoration policy is not imported. |
| FieldCore `include/TunnelMonitor/contracts/Health.h`, `src/system/SystemHealthProjection.h` | Unchanged since prompt 49; existing generic health consumers do not establish CAN integration. |

Original EN86 pages 11/12/15/18 and EDS 1017/6040/6041/60FF/2300 were reopened;
the finite-trial reviewer independently checked EN86/ZH86 velocity procedures.
Access, status and ordering contradictions remain visible. No source constant,
firmware qualification flag or original reference was changed.

Reviewers used `gpt-6-astra` high for finite-trial uncertainty and independent
correlation review, and `gpt-6.1-sol` high for load tooling and package contracts.
Only the load reviewer edited its assigned validator/tests; the lead owns
documentation, integration, Git and final verification.

## Verification and handoff

The integrated full verifier passed:

```text
python scripts/verify.py --mode full --compiler gcc --build-dir .build/audit47-49-final
```

All 147 selected CTest contracts passed (287.5 s), followed by package consumers
and strict Doxygen 1.13.2. Tools were GCC 15.1.0, CMake 4.0.1, Ninja 1.12.1 and
Python 3.12.10. Local sanitizers are unavailable on Windows; required Linux CI
supplies those checks. This is a working-tree check, not a claim that the starting
commit contains the corrections. The final parser refinements also passed their
focused checks on the final files.

Focused results: 136 parser cases, 47 runner cases and 26 accounting cases; the
finite-trial review separately passed 23 session and 59 bench-plan cases. Fresh
replay of all 31 original soak captures preserves 19 incomplete, nine complete
and three failed-evidence verdicts. Both original velocity captures remain
complete at 88 terminals / 264 traffic records each. Deliberately corrupted
metadata/counters now fail; the original bytes were never edited.

[Verification evidence](audit_47_49_verification.json) retains commands, tools,
source/capture hashes and verdicts. Independent package/README and documentation
checks are also retained under `.build/audit49-fresh-package` and
`.build/audit49-fresh-doxygen`. Generated/operation, repository, promptset,
original-reference and whitespace checks pass. No hardware run was performed.

Work remains on `main`; the final commit, upstream synchronization and required
exact-commit CI result are reported in the handoff after push. No release tag or
package publication is authorized or performed. The next independent prompt
remains 50; physical obligations are listed above and in the original reports.
