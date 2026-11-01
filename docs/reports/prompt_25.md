# Prompt 25 - passive health, statistics and diagnostics

The [fresh independent 25/26 audit](prompts_25_26_audit.md) fixes qualified expired
PDO delivery history while preserving capture freshness and uncertainty rules.
Its new regression and integrated verification supplement this original record.

Date: 2026-11-01. Promptset v2 continues on `promptset/motorcontrol-can`, upstream
`origin/promptset/motorcontrol-can`, original base `7878f7c`. Entry
`3172483` had a clean worktree/index; fresh fetch showed 0 ahead/0 behind.
The implementation uses the actual Prompt 24 owner and existing selected stack.

## Status and owned scope

**Owned software delivered; stage Partial solely on inherited 24/23/19/Q20.**
The missing non-consuming bootstrap read evidence remains Prompt 19's obligation.
No health projection, counter, diagnostic or native test is deferred to it.
Physical qualification, real controller/SDK retirement, remote-channel reopening
and drive/machine health retain their existing separate owners and limits.

Example-only `DriverState`, `Health`, `Stats`, diagnostic and resource types now
live with CanBusOwner. Checked cached getters reject unknown bindings and perform
no I/O, callbacks or clock reads. CanBusDiagnostics.cpp contains the direct
projection/reset/history methods; actual accounting hooks remain at the single
owner admission, native terminal, controller report and receive boundaries.
Private OwnerCounters.h provides the shared saturating arithmetic. No installed
core declaration, package dependency, second receiver/node registry or generic
health framework was added.

The [owner guide](../can_owner.md#passive-communication-health) owns exact state,
counter, reset and time semantics. Briefly: one attempted logical SDO service is
counted once across segments; cancellation, unsupported work and local queue
expiry are excluded from service totals. Valid aborts fail service and establish
liveness. Actual TX requires a matching final controller report. Passive events
cannot clear a failure streak or lastError. clearStats preserves live evidence,
uncertainty, interlocks, times, history and previous service-gap timestamp.
Host heartbeat age is explicit, inclusive and disabled at zero; unknown/disabled
producer timing alone never establishes Offline. No diagnostic label is used by
admission. There are no thresholds, retries or automatic recovery actions.

Eight fixed diagnostic copies preserve raw RX capture provenance, enqueue and
actual TX identities, full terminal/abort/uncertainty and controller/recovery
events. Full storage overwrites oldest copies with accounting, independently of
canonical dispatch/results. Resource snapshots and a bounded pull API let output
consumers run separately; the legacy synchronous observer still must not block
or perform output I/O. The native preview demonstrates cached access and reset
without adding transport callbacks.

## Source refresh and intentional differences

All listed siblings are clean. RS/ADS1115/INA228/FieldCore match Prompt 24;
SHT3x/MB85RC were refreshed for this seam. Actual declarations, implementations,
callers and focused tests were read; unchanged sibling suites were not rerun.

| Checkout | Revision | Actual inspected evidence |
| --- | --- | --- |
| MotorControl-RS | `f03eda09db4b38ec7e8053132534b2de658f77a5` | RtuRunner.h:143/214–224 counters/cached access/reset/trace; RtuRunner.cpp:9/96/120/134 saturation, overwrite and terminal accounting; ProbeApp.cpp:536/2525 callers; runner_test.cpp:262–268/614 and traffic_test.cpp:174 verify preserved terminals and unchanged normal traffic under full trace. |
| ADS1115 | `235af3e8c722661a89a44766a4ccd903a654f6fe` | ADS1115.cpp:2656 ignores in-progress, saturates totals/streak and retains prior error; owner-safe-poll main.cpp:195–264 and HealthView.h:46–61 inspect matched cached diagnostics; test_basic.cpp:5434/5510 separates Offline diagnostics from explicit transport/retry policy. |
| INA228 | `b28c5ea69bcb4d26b351c52c24d3c6fae9fd282b` | INA228.cpp:2966–3018 separates passive tracking and legacy latch admission; test_basic.cpp:5298/5354/5374 keeps error history and distinguishes passive/latching behavior; :4547/4734 checks stage errors, diagnostics and caller time. |
| SHT3x-main | `f65f15573e3c9b1ef04cce72ad08f307d021e563` | SHT3x.cpp:2550–2625 separates transport/logical-operation/protocol/latch facts; header:472–483 separates readiness cases; test_basic.cpp:4598/4637/4864 checks observation-only transport, saturation and not-ready counting. |
| MB85RC | `1bf7c293549b1038b2d8f3b7a8d655fa74201e6e` | MB85RC.cpp:2126–2164 saturates only the U8 streak; U32 lifetime totals intentionally wrap, explicitly tested at test_basic.cpp:4246. Cached accessors :284–301 and DiagnosticCore.h:143–170 are actual consumers. CAN deliberately saturates all selected totals. |
| FieldCore-node | `5ad9292dc8481cf08be9a61f662495a6474d1da0` | SystemHealthProjection.h:246–259/280–315 copies compiled owner caches; SystemRuntime.cpp:1225 calls it; native projection test:120–180/246–273 preserves independent roles/state/time and unchanged transport counters. Read only; its enums are not imported. |

RS establishes interface-counter/cached-access style, not an installed Health
lifecycle. Sensor readiness retries, failure-count thresholds and LATCH_OFFLINE
admission do not become CAN policy. CAN counts one confirmed service rather than
an entire multi-object operation and preserves motor-effect uncertainty separately.
Current verifier, explicit registration and workflow commands/pins were inspected
and reused; no workflow trigger/dependency changes were needed.

Original evidence was reopened, including fresh rendered vendor pages:
EN86 PDF p12 assigns alarm bit 7; ZH86 PDF p12 assigns bit 3; OP86 PDF p10 combines
bit 7 alarm and bit 14 running, and its page header says CL57-C/OL57-C despite its
retained 86 filename. These remain contradictory/applicability-limited claims.
Original EDS lines 261–268 establish 1017 U16/RW/non-PDO/default zero; 1841–1848
establish 6041 U16/RO/PDO with no bit map. Those are metadata, not effective live
configuration, dialect, firmware identity or host-loss stop behavior. No conflict
was resolved, ledger fact expanded or original artifact changed. Nothing was
physically measured or sent on CAN.

## Independent review and adversarial proof

Strong review caught that retained PDO sample history could appear current after
map requalification. Projection now uses the existing pdoFreshness contract
before accepting a sample. A same-ID/same-route invalidation/requalification test
keeps Health Uninit until genuinely fresh qualified RX. Review also required RX
capture times and exact TX/report identities in copied diagnostics; these are
now retained without becoming authoritative protocol input.

Final counter review also refined positive per-frame retirement: it must retire
that frame's queued/future final reports as well as prevent later TX. Adapters
unable to prove both return false and use normal report/recovery settlement.
The fake retires only matching records and preserves unrelated reports. This
keeps counting bounded without a diagnostic admission interlock or unbounded ID
history. Earlier unconfirmed TX remains unknown; retirement does not manufacture
actual-transmission evidence. The callback signature is unchanged and no real
SDK capability is asserted.

The independent suite covers state precedence, zero timestamps, disabled/unknown/
never-observed heartbeat, inclusive expiry, abort/liveness, segmentation counted
once, local cancellation/deadline precedence, malformed/unknown ingress, actual
TX versus enqueue, duplicates, reset lifetime, full history with ordinary/urgent
work, binding/configuration changes and extreme saturation/gaps. No existing
assertion, cutoff or check was weakened.

An independent-success-after-failure fixture retains an application copy and
terminal history, explicitly releases the old owner result/reservation, proves
all fake controller/server queues retired, and reconstructs the stationary fake
backend under an explicit fresh-session premise and new node epoch. A subsequent
service clears the current streak without rewriting the old failed operation,
error or uncertainty. This is an isolated external-proof fixture, not a new
production channel-reopening API or proof from a host epoch/quiet interval.
Production uncertain channels remain quarantined until applicable evidence exists.

Delegation: p25_sources (`gpt-6.1-sol`, high) performed the read-only recurring
source/PDF audit and owned owner guidance/native preview files. p25_tests
(`gpt-6.1-sol`, high) owned the independent health suite. p25_review
(`gpt-6-astra`, high) challenged terminal counting, freshness, reset and trace
provenance. Lead owned central types/routing, projection implementation, build/
registration, shared docs and Git. Workers made no Git, sibling or hardware changes.

## Verification and handoff

The final integrated focused owner/recovery/health/native-preview/service/passive
path passes 9/9. The independent health suite passes 863 checks. Host measurements on Windows
x64 GCC: owner 6,744 bytes, Health 72, Stats 56, Diagnostic 368, resource snapshot 7.
The backend remains separately owned. These are not embedded ABI/PSRAM/stack or
physical timing qualifications. Per-turn work remains bounded by RX8/TX2/report8/
start2/recovery1 and fixed health/PDO scans; at most 26 trace publications per
service turn (RX8 + report8 + TX2 + seven request terminals + one recovery closure).
maxServiceGapUs observes caller service spacing; it does not guarantee scheduling.

Local tools: Python 3.12.10, GCC 15.1.0, CMake/CTest 4.0.1, Ninja 1.12.1,
Doxygen 1.13.2 and clang-format 14.0.0. Local Clang/Linux sanitizers are
unavailable; the established exact-commit remote jobs provide those checks.

| Final local gate | Result |
| --- | --- |
| `python scripts/verify.py --mode quick --build-dir .build/p25-quick` | PASS, 42/42 registered contracts; repository/version/object freshness included. |
| `python scripts/verify.py --mode full --build-dir .build/p25-full` | PASS, 43/43 contracts plus independent package and strict Doxygen. |
| Isolated source/export/install consumers | PASS, C++11/C++17/no-RTTI; exact unchanged 22 core headers, no example health/owner/stack/SDK/Python requirement. |
| Strict Doxygen 1.13.2 and extracted API inventory | PASS, 22 installed headers plus application interfaces, 1,445 members and 61 free functions. |
| Promptset integrity/repository/diff checks | PASS, 137 protected artifacts, 99 EDS indexes/202 typed entries, 53 manual items and 24 conflicts retained. |
| clang-format 14 dry run | PASS on changed example/owner/test C++ files. |

The first integrated quick/full runs passed before final counter edge cases and
retirement refinement; their logs remain retained. The final reruns above are
`.build/p25-quick/20261101T075652465624Z-84664-ncx3h_0o` and
`.build/p25-full/20261101T075652465624Z-32472-g1q4n135`, each with command/tool/
status logs and a summary in its parent directory. They record entry revision
plus changed worktree rather than misattributing new code to entry `3172483`.
Final independent source review has no remaining findings. Staged/committed
integrity, push/synchronization and exact final-commit CI are checked at delivery;
the CI receipt is `.build/p25-final-ci.json`. No hardware qualification or real
firmware/SDK job is claimed.
