# Prompt 47 finite hardware qualification

## Scope and current disposition

Execution started on 15 November 2026 at `60bcefe5f2bd29429b8019361374e1776a2b5972`
on `main`, synchronized with `origin/main`, with an empty index and clean tree.
The owned assignment is finite qualification of delivered public operations,
console and runners. It does not implement later motion, recovery or release
features to manufacture a passing campaign.

Physical qualification is **Partial**. Raw plans and results are retained in the
[campaign directory](../bench/20261115-p47/README.md). Controller-reported results
and physical qualification are separate. The user explicitly authorized
unattended tests; no fresh shaft, torque, travel or electrical observation is
available, and no homing fixture was supplied.

## Consumed artifacts

- Prompt29: `DeviceEvidence`, reviewed per-object acquisition, actual fixture and
  conflict records, S2 bench assembly and retained prior measurements.
- Prompts43/44: `Console`, `bench_session`, finite scenarios and exclusive evidence
  artifacts; source tests are rerun for any changed capture behavior.
- Prompt45: retained action/stop/owner uncertainty and adversarial integration
  suites. Prompt46 supplies current production consumers and contrast checks.
- Prompt27/28: core0/UART0/S18/TWAI S2 implementation and current native-IDF build.
  The dedicated bench image exposes the shared console after explicit startup;
  the ordinary configured console image remains inactive without assembly hooks.

## Refreshed sources

| Repository | Revision/state | Adopted or rejected behavior |
| --- | --- | --- |
| MotorControl-RS | `f0741d459c7cfa16c762cac64f4a7a2708c0c76b`, dirty including runner/evidence files | Read live `bench_session.py::run_recorded/snapshot/cached_failure`, `bench_motion.py::phase_plan/profile_snapshot/_run_phase`, `bench_probe.py::observe_stopped/cleanup_stop_attempts/move_campaign`; retain primary and cleanup outcomes and bounded evidence. Reject RS port defaults, implicit configuration reads, retry/recovery and ESS semantics. The working tree, not HEAD alone, supplied this behavior. |
| ADS1115 | `e07622a99fe9f13c5d6a5a2497a8112a14185b9d`, clean | Verdict seam unchanged since44; checked finite `run_soak` bounds. Adopt contract/evidence distinction; reject sensor recovery policy. |
| MCP45HVX1 | `abfd749fbc641b9fe29e1eb2d6f6428ec4e9a4e2`, clean | `tools/run_hil_mcp45hvx1.py::capture_baseline/output_change_sequence/restore_baseline/verdict`: failed baseline blocks writes; explicit skips are not passes. Automatic restoration is not motor cleanup. |
| FieldCore-node | `c40ea8341edf8d665460b902cafefbe857a7f73f`, clean | Ownership/result seam unchanged since46: copied requests and independent retained uncertainty. No CAN integration inferred or sibling files changed. |

MotorControl-RS changed concurrently to
`4a5d09d011edac91ec1aa78a747493aac61dc82b`. The
[source delta](../bench/20261115-p47/source-delta.json) records its 30 dirty paths
and inspected source hashes. Nine named session/campaign functions retain their
AST across the two commits and final working files. Evidence-validator changes
strengthen checked-write/deadline/uncertainty handling; the policy above still
applies. The initial uncommitted state cannot be reconstructed from HEAD.

The independent source reviewer reopened original EN86 pp5, 9–15, 17–18,
ZH86 pp12–13, 19 and OP86 pp5, 9–11, rendered EN86 p11 and ZH86 p12, and checked the
affected EDS sections. Mode/start ordering, signed velocity, `2200` direction,
release, zero-ramp and homing conflicts remain explicit. No generic CiA402
fallback, new source resolution or family-wide qualification was introduced.

## Verification and remaining work

Current S2 source build passed its real firmware gate, including 43 original-byte
SDK manifest entries. The isolated 5.5.5 checkout needed only original LF bytes
restored in 42 manifest files; SDK source content equals the pinned Git blobs.
Its dirty label from stale index metadata is retained honestly. The existing
read-only esptool 4.12 Python satisfies the SDK requirements; no global tooling,
pin or source-hash check changed.

The full native verifier passed 147 registered contract tests, installed-package
consumers and strict Doxygen. Its earlier aggregate 300-second cutoff stopped a
passing suite near its end; the remaining tests passed individually within their
own limits. The aggregate guard is now 600 seconds; no individual limit or test
was removed. Both the original failure and passing run are retained in
[`verification`](../bench/20261115-p47/verification/full-verifier.json).

The source tree changed during the campaign. The full run covers the trace
implementation; later allocation/capacity changes receive their focused native
and target checks. Final documentation passes strict Doxygen, repository and
promptset integrity checks. Source/documentation diffs pass whitespace checks.
Original evidence retains CRLF and compiler-log trailing blank lines, which an
unfiltered Git whitespace check reports; those bytes are preserved under the
repository's reference policy. All 249 evidence files matched their staged Git
blobs, and all 92 source hashes in the two independent reviews matched originals.
Final exact-commit CI and synchronization are reported in the handoff, not
inferred from an older green build.

All five CI jobs passed on implementation checkpoint `bf8a0d9`
([run 43714816653](https://github.com/janhavelka/MotorControl-CAN/actions/runs/43714816653)):
Windows quick GCC, full Linux GCC/Clang with ASan/UBSan, Arduino 3.3.11, and
ESP-IDF 5.5.5 firmware/component/stack checks. This is distinct from the later
evidence/documentation checkpoint's exact-commit verification.

## Findings and corrections

| Finding | Evidence and correction | Retained limit |
| --- | --- | --- |
| SDK identity mismatch | Actual `v5.5.5-dirty` matched the build, but comparison normalized only one side. Normalize one optional `v` on both SDK labels; keep dirty/revision suffixes and other fields exact. 102 evidence and 77 flash tests passed. | The literal dirty label is retained; it does not bypass source hashes. |
| Owner diagnostics lost during identity bursts | Artifacts 08/09 each lost five diagnostic records, including the `100A` request/reply pair, despite successful typed reads. Add bounded application copies independent of UART pressure. | Trace capacity is finite; owner overwrites and application drops stay distinct. |
| Trace tail and off reply under pressure | A delayed record after an empty host read could be lost; tracing could fill both ordinary reply slots. Reserve one reply slot, report FIFO `trace_pending`, keep copied records draining after off, and bound host tail reads. | Pending-count delivery is not electrical completeness. Capture loss fails a requested trace while preserving the workload and primary failure. |
| S2 heap fragmentation | Artifact 17 refused console allocation with 28,500 bytes free. Allocate the three console blocks largest-first. Artifact 22 then opened successfully. | Total free heap alone does not prove a particular allocation fits. |
| Trace RAM versus two node contexts | The initial 128-record ring left the largest block below the second 3,856-byte axis allocation. Reduce the ring to 80 records/1,920 bytes; preserve the 70-record pressure test and explicit overflow accounting. | Sustained traffic can still exceed the console's output rate. No heap was added to the installed core. |
| Velocity record reservation rejected a finite plan | Replace separately over-counted running/stop cycles with their joint deadline bound in `BenchRoute::terminalBound` and `s2_bench.py::plan_bounds`. Independent review and a native early-failure/unsuccessful-stop regression cover the branch omitted by the first proposed simplification. | Runtime poll counts, deadlines, stop budget and 128-terminal capacity are unchanged. |

The 12 s / 4 s / 22 s native bench plan originally reserved 132 terminals against
128 slots. A proposed simple running-cycle reduction was rejected during review:
an early failure can start the run's own stop before its normal duration ends.
The reviewed joint running/stop reservation must include that branch and S16's
separate fallback. Runtime poll limits, preparation-time duration, immutable
deadlines and terminal capacity remain unchanged.

The final reservation is `87 + 7 * ceil(deadline / interval) + 3`. Normal running
starts after an interval, and its own stop can start early but stays within the
same deadline. S16 only follows an untransmitted own stop, so its cycles fit the
joint bound as well. The branch with extra unsent stop attempts uses no stop
observation cycles, leaving sufficient reserved space. The adversarial native
case retains 123 actual terminals. The selected 12 s / 5 s / 23 s request reserves
125; the rejected 12 s / 4 s / 22 s request still reserves 132. Neither experiment
raises a runtime limit to obtain admission.

Final-source focused checks passed: six native console/device/heap tests,
the bench-route adversarial suite, and 59 Python bench tests. The broader trace
changes also passed the console, adverse-stream, campaign and session tests,
including optimized Python. SDK-label comparisons passed 102 evidence and 77
flash tests. The final S2 image is `bf8a0d9518c6b42ff06d4f29516bf82ee66abf79`,
ELF SHA `9ca7701bb4f20efe9c383fd8fe4929e0314980bd2f2f1899652abcaa687b0399`.
Both motion contexts were allocated before motion; 2,880 heap bytes remained,
largest block 1,024, stack high-water free 5,636. Those figures remained unchanged
after the trials. They describe this finite campaign, not a worst-case bound.

## Device observations and qualification

The [independent baseline review](../bench/20261115-p47/baseline_review.json)
checks original raw files and hashes. Both specimens report `1000=262546`,
one-byte `1009=12` and `100A=32`, `1018:00=5`, vendor/product/revision
`4096/1/0`, and `2000/2001=1024/512`. These are opaque reported values, not a
newly decoded firmware-version string or unique model identity.

Before any motion, node 1 reported `6041=8050`, `2100=0011`, position 11329;
node 2 reported `6041=C050`, `2100=0015`, position 11327. Both reported mode 3,
zero velocity, `2101/2102=0`, `603F=0`, `2200=0`, `2201=1000`, and heartbeat
producer `1017=0`. TPDO1 readback was ID `181/182`, type 255, inhibit raw 100,
event timer 0, and three entries `60410010`, `60610008`, `60640020`. No PDO or
heartbeat mutation was performed in this campaign.

| Feature or observation | Current result | Qualification boundary / next measurement |
| --- | --- | --- |
| ESP-Prog2 UART, automatic EN/BOOT and S2 image identity | Builds, ROM identification, flashes and exact ping checks succeeded. | JTAG was not exercised. S3/P4 physical behavior is not inferred. |
| Requested-silent receive capture | Repeated five-second captures: zero frames, loss and attempted TX. | S18 request/TX gate is not measured electrical listen-only. A passive independent CAN observer is still needed. |
| Typed identity and state reads on both nodes | Matched ordinary public runner results. Final-image artifacts 32/33 retain all 40 diagnostics per ten-transfer identity campaign with zero reported loss; all six post-motion state blocks in 42 passed. Earlier repaired-image regressions 23/24/25 are retained separately. | Scope is these specimens, image and exchanges. Existing dictionary conflicts remain. |
| Configuration, heartbeat and TPDO baseline | Explicit raw acquisitions matched all 26 transfers per node; readiness matched 30 per node. | Readback is not persistence, PDO freshness, physical motion or a host-loss stop guarantee. |
| Invalid finite velocity candidate | Artifacts 34/36 rejected 1,000 ms on both final-image nodes before downloads, with the expected setup-budget error; 35/37 released only local evidence. The earlier 26/27 test is retained. | The successful negative tests do not qualify a start or stop. |
| Positive finite velocity and its stop | Trials 47301/47302 completed their public finite operation and own quick stop, each with 88 terminals/264 traffic records, zero loss and `needsStop=0`. Reported running velocity was 30 rpm, followed by zero. | Existing experimental gates remain false for start, stop, reverse and mode-change qualification. No physical speed, direction or standstill was observed. |
| Position, relative/absolute return, angle and zero target | No physical qualification in this campaign. | Need the selected mode transition, scale/reference and observed direction/travel/standstill evidence. |
| Homing and home-to-move transition | Not run. | Actual method-specific switches/polarity, direction/`2200`, offset and reference/completion evidence are absent. |
| Release, disputed I/O, persistence and reset | Not used as setup or cleanup. | Their existing source/firmware gates remain with their owning prompts. |

The [final independent review](../bench/20261115-p47/final_review.json) checks each
motion terminal against its raw request, matching response and TX report. Each
node received exactly seven downloads: `60FF=30`, `6083=500`, `6084=500`, then
`6040=6,7,15,11`. The 81 uploads and seven downloads all acknowledged without
abort. No `6060` write, S16 fallback or best-effort stop was needed. Operation
identifiers are session-local; node and route identity distinguish the records.

For both runs, the own stop was requested exactly 12,000,000 us after preparation,
its first TX followed 5,000 us later, and its second confirming observation ended
5,117,000 us after stop due, within the original 23,000,000 us deadline. Maximum
owner service gap during each trial was 6,000 us; final inspection observed a
7,000 us boot-lifetime maximum. CAN drops, TX rejects, unmatched responses and
error counters stayed zero. These are software times and counters, not measured
wire latency. The console's separate backpressure counter is not a CAN rejection.

Final `6041=C050`, `2100=0015`, mode 3 and `606C=0` matched the existing
experimental stopped-state rule on both nodes. Positions changed from 11329 to
16846 and from 11327 to 16845 respectively. These native feedback changes do not
establish physical travel, shaft direction or counts per revolution. Artifact 42
rechecked status, monitors and feedback through ordinary read-only public runners.
Local evidence was released only after complete dumps. No motion remains active
in the application; CAN stays explicitly active and the device's physical torque
state has not been independently measured. No further motion was attempted.
`TRIAL_LAST` in these native records is the retained pre-motion route read; final
state above comes from the own-stop uploads and the later ordinary reads.
The native TV route exercises public session/velocity/owner APIs. It does not
qualify positive motion through the streamed JSON console/Python motion runner;
that path still needs its own complete bounded capture. The two reviewers found
no remaining software blocker in the changed paths; finite tracing can still
overflow under other traffic, and its counters must be checked on every run.

Artifact 16's `no_end` was a plan error: it expected a separate `HEAPREG` line,
whereas the valid `HEAP` line already contains region data. Its capture completed
and was released. Artifact 20's unsupported helper byte and the first 25 host
option were rejected before port access; their corrected requests retain those
host mistakes explicitly. None is a drive failure or hidden device retry.

The user's exact unattended-motion statement and absent observer/cutoff operator
are retained in the scenario plan. The existing S2 application's documented waiver
exception permits the historical `attended=1` field to mean host admission by
waiver. It never supplies physical observations; all experimental qualification
flags remain false. No save, factory restore, alarm clear, input reassignment,
node/bitrate change or implicit mode change is authorized by diagnostic cleanup.

## Maintainability and handoff

The installed library and CAN owner remain unchanged. Corrections use the
existing console, largest-first allocator, public operation path and bounded
evidence records. There is no extra receiver, service engine, motion replay,
runtime framework or new default motor behavior. Tests exercise real lost-data,
pressure, allocation and timing boundaries rather than merely checking symbols.

Physical qualification cannot finish from unattended telemetry. The next
measurements are actual supply/nameplate and electrical observations, witnessed
direction/travel/stop behavior under the selected firmware, and the wired homing
fixture with its method-specific facts. Independent documentation/package work
can proceed with these obligations visible; prompts 48–50 were not executed here.

Checked implementation checkpoints `57d7a83`, `2bbd98f` and `bf8a0d9` were
committed and synchronized on `main`. The final evidence/documentation checkpoint
and exact-commit CI result are reported in the handoff after final verification.
