# Prompt 5 execution report

Later decision change: [prompt 15](prompt_15.md) replaces the initial native
HB/EM consumer choice with checked passive codecs and one owner projection,
based on pinned-consumer callback/envelope limits and actual parity checks.
The [maintained decision](../decisions/canopen_stack.md#prompt-15-passive-boundary-correction)
owns that correction; active NMT/PDO integration remains assigned to 16-18.
The original fit results and forward assignments below are historical.

Date: 2026-10-28. Status: **Complete** for the assigned service fit experiment
and decision. Prerequisites 3 and 4 were complete at starting HEAD `aa8a3bc`.
The existing `promptset/motorcontrol-can` branch was reused; initial working
tree was clean and fresh fetch showed 0 ahead / 0 behind its upstream.
Prompt 6 and later production implementation were not executed.

## Delivered scope

| Prompt requirement | Actual evidence |
| --- | --- |
| Current source audit | [Sibling/controller review](prompt_5_source_review.md): RS, ADS1115, INA228 and FieldCore implementations, real callers/tests, current revisions and clean state. No sibling edits/fetches or repeated unchanged sibling suites. |
| Primary candidate inspection | [Independent stack review](prompt_5_stack_review.md): immutable upstream source, configuration, native blank port, callback/storage/allocation, SDO lifecycle, cancellation and network/PDO premises. Current upstream HEAD equals retained pin. |
| Required service inventory | [Decision](../decisions/canopen_stack.md) ties expedited/segmented SDO, NMT, heartbeat/boot-up, EMCY, configured PDO and selected SYNC to reviewed scope. No block/LSS/retry/startup assumption. |
| Buildable exchange | [Standalone proof](../../experiments/canopen_service_fit/README.md), actual pinned C SDO engine plus local C++11 owner and fake controller. No SDK, Python, sibling, network or hardware needed to build/run. |
| Ownership and boundedness | Copied admission bytes, stationary callback context, one RX dispatch, supplied time, one fake queue and one retained result; tested handoff/retirement and terminal ownership. Capacities/types remain provisional, not public ABI. |
| Allocation and resource facts | Caller-owned service-object setup and steady paths inspected; `sizeof` measured on Windows x86-64. No allocator instrumentation, embedded stack/RAM or latency measurement claimed. |
| One engine selected | CANopenNode behind an optional application adapter; explicit local-subset comparison finds no essential blocker justifying reimplementation. No second active service engine or runtime selection layer. |
| Export/license effects | Eleven exact upstream blobs and Apache-2.0 license/notices retained. Standalone build has no install/export rule. Future core cannot depend on stack headers/objects/SDK/Python/network; production consumer checks belong to 7. |
| Documentation and handoff | Decision, proof README/comments, source map, architecture plan, backlog, roadmap, coverage, root/docs indexes, changelog and PROGRESS updated. Exact unresolved adapter duties assigned below. |

The lead split work between a `gpt-6.1-sol` high-reasoning native builder, a
`gpt-6.1-sol` source/controller auditor and a `gpt-6-astra` high-reasoning
independent ownership/lifecycle reviewer. Workers used disjoint files, returned
source-backed findings and made no Git mutations. The lead integrated and
reviewed the result and owns commit/sync.

## Verification

- Native Release configure/build and CTest passed using CMake 4.0.1, Ninja 1.12.1
  and MinGW GCC/G++ 15.1.0: one executable test, with checks active under NDEBUG.
  Commands are in the proof README. The executable runs six scenario groups.
- Executed expedited and two-segment upload/download; caller-buffer mutation;
  interleaved heartbeat/EMCY/PDO copies; matching-object/raw-abort handling;
  malformed frame/response rejection; exact-deadline RX; full-TX deadline;
  cancellation before/after handoff; send failure; retained results and no replay.
- Independent review found a final-handoff deadline gap. `handoff(nowUs)` now
  checks expiry itself; direct handoff at the exact deadline without an
  intervening service turn yields timeout and zero handed-off frames. The
  regression and independent reread/build passed.
- Host object sizes: frame 16, controller 88, SDO client 176, completion 56,
  complete owner 392 bytes. Embedded members overlap these figures; they are
  not additive or an embedded memory/stack budget.
- CMake source-hash guard rejects a copied-manifest wrong-hash fixture.
  Repository checker regressions cover missing license, bad hash/revision/source
  mapping, escaped paths and unexpected vendor files.
- `python scripts/check_promptset_test.py`: 18 tests passed.
  `python scripts/check_promptset.py`: passed, 50 prompts, 134 hashed artifacts
  (the previous 123 plus eleven build sources). The integrated ledger check
  retains 99 indexes / 202 entries / 53 manual functions / 24 conflicts.
  Original documents and reviewed semantic ledger were not changed.
- Working-tree and Git INDEX artifact checks passed for all 134 originals;
  staged whitespace checks passed. Committed bytes are rechecked during final
  delivery. The repository has no CI workflow yet; prompt 8 owns its setup.
  No firmware build, live CAN traffic or hardware test was run.

## Retained limitations and next owners

The [decision](../decisions/canopen_stack.md) is the maintained contract for this
choice. The source reviews preserve exact upstream/SDK locations and findings.
No unresolved candidate blocker prevents this documentary/native experiment
from completing; the following production obligations remain visible:

| Scope | Remaining work and owner |
| --- | --- |
| Public contract | 6 freezes work/result/frame/capacity/time/retirement types and proves multi-drive/protocol boundaries. Do not install or copy provisional proof types as a finished ABI. |
| Build/package | 7 registers proof and actual source/install consumers; 8 supplies verifier/CI and strict public documentation gates. |
| SDO | 13 integrates guards, proper bounded FIFO draining and broader size/toggle/fault tests; 14 implements evidenced retirement/quarantine/reconciliation. Local IDs and reconstruction never establish wire freshness. |
| Network/PDO | 15-18 integrate real stack consumers/producers, EMCY context/envelope, per-frame boot invalidation and PDO sample provenance. Proof event counters establish only interleaving/copy ownership. |
| Application/controller | 23-25 retain one receiver and existing service storage; 27 verifies actual TWAI driver/synchronization/evidence. SDK acceptance can already start transmission; software queue clearing is not hardware retirement. |
| Device semantics and hardware | All 21 Lichuan qualification gates remain unchanged. Bench/model/firmware/controller/transceiver are not established. 29 and 47-48 own separately authorized evidence/qualification. No supported drive claim follows. |

The proof assumes a qualified initial fake route and a cooperative little-endian
host. It permanently interlocks after uncertain transmitted failure and does not
provide production recovery. Its 32-byte storage does not ensure every upload
through 32 bytes completes: ordinary full segments can stall after 28 bytes
without draining, then time out without truncated success. The ten-byte
segmented case demonstrates engine fit; capacity-boundary integration remains 13.

Next assignment: [prompt 6](../prompts/6_api_ownership.md). Commit and sync this
checked prompt on the existing execution branch; preserve all later obligations.

## Fresh independent audit

On 2026-10-28, at starting HEAD `49584d5`, the lead and three fresh-context
reviewers reread original prompts 4 and 5 in full and inspected the actual
committed diff, local proof, upstream implementation and tests. Work stayed on
`promptset/motorcontrol-can`; the starting tree was clean and fresh fetch showed
0 ahead / 0 behind. Review roles were `gpt-6-astra` high for lifecycle/ownership,
`gpt-6.1-sol` high for documentary contrasts, and `gpt-6.1-sol` high for
build/provenance and live sibling source checks. Workers did not mutate Git.

No prompt-5 runtime, ownership, package-boundary or unnecessary-abstraction
defect was confirmed. The lead reread the service/receive/finish/handoff paths,
fake driver and tests against the review findings. The actual upstream RX,
initiate/process/close paths still support the selected boundary. No alternate
engine or speculative refactor was added. The three documentary gaps and their
corrections are recorded in [prompt 4's fresh audit](prompt_4.md#fresh-independent-audit-after-prompt-5).

Independent fresh Release configure/build/CTest runs in
`.build/reaudit_code` and `.build/reaudit_verification` passed (1/1 each), with
the same measured 16/88/176/56/392-byte host sizes. A temporary independent
review harness also passed 98 cases: write lengths 1-32, segmented reads 1-28
with/without size indication, five post-handoff cancellation phases, deadline
overflow/backward time, toggle and short/long transfer faults, and send failure
after a write handoff. The lead inspected and reran that harness successfully. These were additional
audit probes in ignored build output, not newly registered production tests or
claims of larger transfer support. The maintained native experiment and its
documented limitations are unchanged.

The verifier review compared all eleven vendor files byte-for-byte with
`git show` at the immutable upstream revision: 313,512 bytes including LICENSE,
no upstream NOTICE. All four sibling revisions in the source report remained
clean before/after inspection of actual implementations, callers/tests and
workflow commands; there was no source delta. No sibling suite, firmware build,
remote CI or hardware test was run. The repository still has no CI workflow.

The lead's fresh Release configure/build in `.build/reaudit_final` and CTest
passed (1/1), as did 18 checker regressions, exact contrast arithmetic,
promptset/ledger integrity, HEAD artifact hashes and whitespace validation.
Staged/final committed artifacts are checked again during delivery. The fixture corrections
leave the prompt-5 engine decision unchanged. Public contracts, full-capacity
upload draining, channel reconciliation and real controller qualification
remain with their original later owners; they are not hidden completed work.
