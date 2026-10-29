# Prompt 14 SDO ambiguity and reconciliation

A [fresh independent audit](prompt_14_audit.md) subsequently clarified the
per-segment freshness premise and strengthened upload cancellation/toggle-wrap
regressions. No runtime defect was reproduced; its fresh checks are recorded
separately from the original implementation results below.

Executed 2026-10-29 on `promptset/motorcontrol-can`, continuing promptset v2
from `6af5283`. Initial worktree and index were clean; upstream fetch showed
0 ahead / 0 behind. No new branch, sibling mutation or hardware action occurred.
Prompt 13 and its independent audit supply the bounded selected-stack lifecycle.

## Source decisions and scope

The [live source refresh](prompt_14_sources.md) records exact sibling revisions,
dirty states, actual implementations/callers/tests, pinned public-code defects,
original PDF/EDS evidence and inspected controller headers. MotorControl-RS
advanced eight commits, with no changed owner/runner behavior; ADS1115, INA228
and FieldCore remained unchanged. CAN keeps its own conservative processing-time
cutoff rather than importing RTU silence or captured-history settlement rules.

Source-confirmed: ordinary download ACKs do not echo values or local tokens;
the selected stack has no ingress timestamp; inspected TWAI queue acceptance is
not TX completion. Unknown: Lichuan old-session lifetime, full local/remote
retirement bounds and per-object read effects. All 202 ledger entries remain
unqualified for non-changing reads. No contradictory device claim was resolved
by generic protocol semantics; no physical claim was measured. Originals,
reviewed ledger and generated catalogue permissions remain unchanged.

No evidence-backed reopening path exists for an unresolved real CL86-C channel.
Permanent quarantine is the implemented disposition, as permitted by prompt 14.
Its precise unsupported capabilities and needed evidence are in the
[service contract](../canopen_services.md#unsupported-reopening-and-required-evidence).
This does not implement motion, reset, retry, stop, a second engine or generic
axis recovery. No particular CL86-C read is asserted destructive.

## Delivered requirements

| Prompt requirement | Implementation and evidence |
| --- | --- |
| Receive/processing/cutoff/terminal precedence | Explicit service table; optional qualified capture-time overload; processing expiry wins even for earlier capture; zero and `UINT64_MAX` are covered. Invalid time is atomic. First terminal remains stable. |
| Identical/different-payload requests | Independent literal response vectors, index/subindex/command checks, stale capture quarantine and duplicate/segment tests. Same-object download ACKs carry no written value. Fresh local tokens cannot distinguish late identical wire bytes. |
| Only reviewed reopening | No reopening API or reset sequence. Tests refuse quiet-time, generation, heartbeat and client-abort attempts. No positive reopening case is fabricated without a sound premise. |
| Retirement and binding | `canTransmit` checks copied handoff eligibility before one-shot controller submission; `invalidate` retires staging and withdraws a slot permanently. Remote effects remain possible after handoff. Node reset invalidates all its channels; bus-off invalidates the interface. |
| Read effects and partial writes | Unqualified uploads retain possible effects; synthetic reviewed harmless reads still quarantine on ambiguity. A two-write test retains first success and second raw abort/uncertain effect independently. No ledger permissions widened. |
| Independent channels | Distinct, explicitly qualified synthetic server channel continues beside an unresolved one. Same peer/channel aliases, crossing IDs and host relabeling cannot replace it. No real extra Lichuan channel is claimed. |

`SdoRxObservation` keeps one copied route/frame and capture/processing times per
existing channel, separately from terminal storage. It survives release and
new admission; the next observation overwrites it. Invalid shapes/wrong objects
are visible as rejected observations; stale/unsolicited traffic quarantines.
It carries no misleading attribution to a new operation token. Inputs are copied
before mutation, including when a caller offers a borrowed prior observation.

The fixed two-channel adapter and actual CANopenNode engine remain authoritative.
The old experiment bridge maps the two new SDO outcomes to its existing transport
outcome. Portable values are installed; stack headers and adapter code remain
outside the standalone core package. New tests join real CTest inventory and its
repository-check fixtures. No workflow trigger or dependency pin was changed.

## Review and verification

Parallel bounded workers: `gpt-6.1-sol` high inspected sources and independently
wrote adversarial tests; `gpt-6-astra` high reviewed correlation/ownership and
precedence. The lead owned lifecycle mutations, shared declarations, registration,
documentation and Git. Workers did not commit or touch siblings.

The first quick run correctly exposed three old proof expectations that ignored
unsolicited pre-handoff responses. They now require a retained transport or
correlation failure, retired staging and blocked reuse. Positive normal exchange
coverage remains. Two intermediate test edits referenced nonexistent enum/method
names; fresh compilation caught them and actual declarations supplied the fix.
Those failed build/run logs are retained and are not counted as passing evidence.

Focused integrated build and five CTests (lifecycle, reconciliation, adapter,
ownership and fit proof): PASS. Production lifecycle: 6,249 checks; independent
reconciliation suite: 1,077 checks. Stronger review found no further defect;
its 86-check independent probe covered cleanup after release, equal-time order,
segment capture and observation aliasing. Those cases are now retained in the
registered suite, with additional foreign/stale handoff-handle negatives.

`python scripts/verify.py --mode full --compiler gcc --build-dir .build/p14_full`
passed 25/25 non-package CTests, independent source/export/relocated installed
consumers, C++11/C++17/no-RTTI, public-header isolation and strict Doxygen 1.13.2
with API extraction. First passing run: `20261029T191508203858Z-87612-7avc_spm`.
Final integrated run including retained review cases:
`20261029T191742680544Z-90876-_qqwvc48`, PASS all the same gates.
Original 134 artifacts, 99 EDS indexes, 202 typed entries and 53 manual functions
remain intact under `check_promptset.py`; diff checks pass.

Measured Windows x86-64 GCC 15.1.0 sizes: `SdoClient` 1,672 bytes (two channels),
`SdoRxObservation` 88 bytes, compatibility facade 1,832 bytes, outer proof owner
2,824 bytes. These nested sizes are not additive or embedded RAM/timing budgets.
No dynamic storage, controller I/O or clock acquisition was added; this is source
inspection, not runtime allocation instrumentation. Local Python 3.12.10,
CMake/CTest 4.0.1, Ninja 1.12.1 and Doxygen 1.13.2 were used. GCC/Clang sanitizer
execution belongs to Linux CI; unavailable local sanitizer runtime is not a pass.

## Documentation, remaining gates and handoff

Public Doxygen covers capture origin, release, immutable terminals, eligibility,
uncertain effects and channel reuse. Services owns precedence/reopening policy;
CAN owner owns controller/cache responsibilities; adapter README gives call order.
README, docs index, CHANGELOG, coverage, backlog, roadmap and PROGRESS track the
actual milestone rather than claiming an operational drive profile.

Device gates remain Q18 (server/session/timing), Q16 (altered routes), Q20
(non-changing read qualification), Q01 (model/firmware), and Q02 only where
status interpretation is needed. Real SDK retirement, ISR/capture ordering and
full owner integration retain prompts 23/24/27; physical acquisition remains 29.
Later gates do not reopen an unresolved channel silently. No physical CAN,
firmware qualification or memory/timing measurement on embedded hardware occurred.
The next numbered assignment is prompt 15; it is not executed by this handoff.

## Delivery

Implementation commit `be7ee1fac8295aa268d7b19dbf1199cbe0c90d34` was pushed to
`origin/promptset/motorcontrol-can`; a fresh fetch confirmed 0 ahead / 0 behind.
Staged and committed original-artifact checks both passed. Its
[push CI run 40939019340](https://github.com/janhavelka/MotorControl-CAN/actions/runs/40939019340)
completed successfully on that exact SHA: Windows quick GCC, full Linux GCC
ASan/UBSan, and full Linux Clang ASan/UBSan, including package/Doxygen gates.
Job IDs were `123677112086`, `123677111615` and `123677112161`, respectively.
The local read-only CI receipt is `.build/p14_ci/implementation.json`.

Prompt 14's owned software and evidence-policy scope is complete. This
documentation closure retains the same branch and is checked with integrity
and diff validation. Its own final commit, automatic CI and fresh remote
synchronization are verified in the handoff; implementation CI is not substituted
for that final-commit check. No task-owned implementation work remains pending.
