# Prompt 23 — ordinary CAN owner dispatch and retained results

Date: 2026-10-30. Promptset v2 continues on `promptset/motorcontrol-can`, upstream
`origin/promptset/motorcontrol-can`, original base `7878f7c`. Entry commit
`084c2708f221fc828f7225bfbdc8360449dfb3a3` had a clean worktree/index; fresh
fetch showed 0 ahead/0 behind. No new branch, remote or sibling mutation.

## Status and owned scope

**Owned software delivered; stage Partial solely on inherited 19/Q20.**
The existing raw read lifecycle is implemented and usable on an explicitly
established channel. Prompt 19's non-consuming bootstrap probe remains blocked
by missing read-effect evidence. COMMON does not permit marking that prerequisite
complete, so 23 retains Partial without deferring any of its ordinary-owner
deliverables. Remapping/settings gates in 18/20–22 are not owner prerequisites.

Implemented `examples/common/CanBusOwner.h/.cpp`, a narrow borrowed controller
interface, real typed read/confirmed-setting integration and the offline native
consumer. The installed core remains unchanged C++11 without I/O, clocks, heap
or scheduling. Urgent capacity, full cancellation, controller retirement/recovery
and real motion remain with their named later prompts; no success stubs exist.

## Source refresh and actual evidence

Every inspected checkout is clean and unchanged from 22. The audit delta is
owner scheduling, copied admission, retention and integration rather than
another tuning review. No unchanged sibling suite was rerun.

| Checkout | HEAD | Implementations, callers and focused tests inspected |
| --- | --- | --- |
| MotorControl-RS | f812d7c1d364a03be86f02ae8f648dc8fca23d42 | RtuBusOwner.h:11–25/47–87/90–184: copied expectations, local IDs, non-consuming result/release; cpp:75–146 reserves/copies before publication, :367–406 cyclic producer heads and fixed deadlines. ProbeApp.cpp:620–635/1911–1936/1973–1982/2282–2287 yields/advances typed work outside owner. bus_owner_test.cpp:148–193/253–311 and bus_scheduling_test.cpp:453–485/551–575 cover retention, mutation, IDs and fairness. |
| ADS1115 | 235af3e8c722661a89a44766a4ccd903a654f6fe | 02_owner_safe_poll/main.cpp:79–100/246–279/300–331 keeps locking/controller calls in application and checks terminal kind/sample validity. Sensor retry/recovery is not imported. |
| INA228 | b28c5ea69bcb4d26b351c52c24d3c6fae9fd282b | INA228.h:239–271 separates identity, generation and payload validity; cpp:940–965 revokes trust while preserving diagnostics; test_basic.cpp:3200–3220 rejects unrelated adoption of active work; bringup and IDF callers use matching pollJob/takeJobResult. |
| MB85RC | 1bf7c293549b1038b2d8f3b7a8d655fa74201e6e | Header:165–177/853–890 and cpp:1272–1278/1554–1559 retain then consume terminal results and borrow transfer buffers. test_basic.cpp:5483–5497 preserves failed prefix/blocks new admission; DiagnosticCore.h:143–170 is an actual caller. CAN deliberately copies input and keeps RS-style non-consuming result/release. |
| FieldCore-node | 5ad9292dc8481cf08be9a61f662495a6474d1da0 | Rs485Task.cpp:1241–1296 schedules yielded module work and lets WAIT release bus; Rs485MotorModule.cpp:340–392/640–679 retains correlation/results across invalidation. test_rs485_task_fake.cpp:3581–3622 allows another sensor during motor reservation. Motor integration remains dormant in build_graph.md:161; no CAN integration claimed. |

RS roles `admit/service/result/release`, `RequestId/SequenceId/BusAdmission/Completion`
are preserved. RS producer sequences do not themselves reserve axes; CAN adds
explicit logical-axis/physical-peer exclusion. No UART/CRC/timing, sensor replay,
consuming getters or borrowed transfer-buffer policy is imported.

The selected adapter/stack and prompt-6 storage proof were reopened. The new
owner uses the actual `SdoClient` directly; it does not wrap the old synthetic
bridge. The pinned CANopenNode source remains `9b8beed8367241e96ac03f916cd5a500bcb2cf23`.
Source-confirmed: protocol replies have no host transaction IDs; nextTx marks
possible TX, controller confirmation is separate, and existing receive dispatch
handles PDO/network before SDO. Unknown: real peer/session retirement and Q20
read effects. No contradictory manufacturer claim was resolved or physically
measured. Native example routes/words are explicit fake fixtures, checked against
the existing EDS 1200/1014/2500/2501 shapes, never installed drive configuration.

## Implementation and review decisions

- One borrowed adapter retains the authoritative channel/node/event/PDO table.
  Six copied request/result slots, two reservations and two active associations
  add scheduling. There is no second receiver or current node registry.
- Admission reserves terminal storage and copies every service byte/expectation.
  Results stay immutable until explicit release. Foreign/stale handles and
  nonwrapping serial exhaustion reject without changing output or queued work.
- Cyclic eligible-producer selection skips busy channels; sequence FIFO and
  physical-peer exclusion preserve settings ownership without blocking other
  nodes or passive events. Each service call has explicit bounded RX/TX/report/
  start budgets, including all-zero expiry without I/O callbacks.
- `beginOperation` permits several prerequisite operations under continuous
  exclusion, with a fresh strictly increasing operation ID and immutable deadline.
  Operation stepping and all interpreted results remain outside the scheduler.
- A small adapter seam exposes authoritative route/policy inspection and accepts
  an optional initial queue cutoff. This preserves owner waiting age without
  rewriting transferred budgets or native admission timestamps. Native defaults
  remain unchanged and direct cutoff/invalid-ID tests were added.
- Controller enqueue, exact-token actual TX reports, protocol confirmation and
  effect uncertainty remain independent. The first download guard notifies the
  actual settings context before possible handoff; it is not called for cleanup.
  Typed loop write/ACK/configuration sync/readback retains its original token.
- Strong independent review found inconsistent same-peer quarantine handling:
  beginSequence could publish an immediately unusable reservation, and active
  work could survive its revocation. Both are corrected. Independent alternate-
  channel regressions prevent later ACK/segmented success or staged segment TX.
- Final review preserves raw observer diagnostics for rejected FD/future frames
  without qualifying them; valid extended/RTR format copies remain available.
  A Release compiler warning exposed an unbounded test response helper; explicit
  supported widths/capacity now guard it. Checks were not weakened.

Maintainability: one concrete application class and fixed value/callback types
express current work. Existing SDO/native codecs, passive invalidation, typed
contexts and configuration epoch checks are reused. No runtime settings engine,
motion dependency or speculative profile was introduced. The current
[owner contract](../can_owner.md) replaces obsolete proof-only guidance while
the original proof/reports remain retained evidence. README, CHANGELOG, maps,
coverage, progress, backlog, roadmap and verification inventory track this stage.

Delegation: `p23_sources` (`gpt-6.1-sol`, high) performed the read-only recurring
audit and owned the disjoint independent native suite. `p23_review`
(`gpt-6-astra`, high) reviewed ownership/correlation against actual code.
`p23_example` (`gpt-6.1-sol`, medium) owned the three native consumer files.
Lead owned shared contracts, routing mutations, adapter seams, integration,
documentation, formatting and Git. Workers made no Git/sibling/hardware changes.

## Verification and delivery

Local tools: Python 3.12.10, CMake/CTest 4.0.1, Ninja 1.12.1, GCC 15.1.0,
Doxygen 1.13.2 and clang-format 14.0.0. Clang/Linux sanitizers are unavailable
locally; the existing Linux GCC/Clang sanitizer jobs run on the final commit.

Focused Release strict C++11 owner and native example builds pass; 10/10 focused
owner/service/read/settings/passive suites pass. The owner suite passes 1,152
checks. Host measurements: owner 2,888 bytes plus the existing separately owned
adapter; Completion 272 bytes (Windows x64 GCC). These are not an embedded ABI,
stack/PSRAM budget or measured hardware service-gap guarantee.

| Check | Result |
| --- | --- |
| `python scripts/verify.py --mode quick --build-dir .build/p23-quick` | Pass, 40/40 registered contracts; repository, version and object freshness checks included. |
| `python scripts/verify.py --mode full --build-dir .build/p23-full` | Pass, 41/41 contracts plus independent package checks and strict Doxygen 1.13.2. |
| Isolated source/export/install consumers | Pass, C++11/C++17 and no-RTTI; exact 22 installed headers, no application owner, reference, Python, SDK or stack requirement in the core package. |
| Strict documentation | Pass, 22 installed public headers and the application owner interface; 1,333 members and 61 free functions extracted. |
| `python scripts/check_promptset.py` | Pass, 137 protected artifacts, 99 EDS indexes/202 typed entries, 53 manual-only items and 24 conflicts retained. |
| `git diff --check` | Pass. |

These local runs tested the integrated task-owned working changes over entry
commit `084c270`, rather than claiming that entry commit already contains the new
owner. Command, tool/version, status and artifact records are retained under
`.build/p23-quick/20261030T140533096588Z-79980-rv660mve` and
`.build/p23-full/20261030T140546953475Z-23912-52tgna1n`, with summaries in their
parent directories. No disposable cleanup removes prior evidence.

Staged integrity, committed integrity, push, fresh ahead/behind and exact
final-commit CI status are checked for the delivery handoff; its CI receipt is
retained as `.build/p23-final-ci.json`. No hardware traffic, firmware build or
physical qualification is claimed.

## Fresh independent audit

The subsequent [22/23 audit](prompts_22_23_audit.md) rechecks the original prompts,
code and primary sources. It adds actual settings failure/retention scenarios
through the owner and removes unused duplicate request storage. Its report
records the current checks and unchanged evidence limits.
