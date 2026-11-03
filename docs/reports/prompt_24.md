# Prompt 24 - urgent owner capacity and transport recovery

**Reopened by audit W5.** [Packet 5](handoff_19_33/packet_5.md) changed abort and controller-event settlement and added the drain marker. Quarantine statements below describe the earlier contract.

**Reopened by audit W6.** [Packet 6](handoff_19_33/packet_6.md) replaced the one urgent slot with one per channel and added `beginUrgentOperation`, `findSequence` and the best-effort stop route. [Packet 7](handoff_19_33/packet_7.md) consumes them. Capacity statements below describe the earlier contract.

**W5 and W6 closed, 2026-11-03.** Packets 5, 6 and 7 delivered them in `73f029b`, `25a332f` and `9685200`, and [packet 10](handoff_19_33/packet_10.md)'s closure audit confirmed them for host scope, with CI green at `5306eaf`. Still open: no real route can reopen until Q18 closes, which [packet 11](handoff_19_33/packet_11.md) left unresolved; no drive has run a stop path (packet 16); and the C-09 remainder belongs to prompt 38.

Date: 2026-10-31. Promptset v2 continues on `promptset/motorcontrol-can`, upstream
`origin/promptset/motorcontrol-can`, original base `7878f7c`. Entry commit
`29906a203cf2a988cc9c393ea97f019d3443e36d` had a clean worktree/index; fresh
fetch showed 0 ahead/0 behind. No new branch or sibling mutation.

## Status and scope

**Owned software delivered; stage Partial solely on inherited 23/19/Q20.**
Prompt 23's owner/service boundary is implemented and consumed here. Its missing
non-consuming bootstrap probe evidence remains Prompt 19's Q20 obligation;
COMMON requires retaining the prerequisite status. No Prompt 24 owner mechanism
is deferred to that probe or a future motion implementation.

The application owner now reserves urgent work/results, cancels individual work,
invalidates sequences/producers, tracks external TX retirement and performs
explicit finite host/controller recovery. These mechanisms use the existing
selected service store. There is no installed stop driver, device recovery
sequence, new service engine or SDK port. Real stop actions remain Prompt 31.

## Source refresh and evidence

All listed checkouts were clean before and after read-only inspection. RS
advanced from `f812d7c` through four ESS homing commits; its owner and scheduling
tests were unchanged. Other listed revisions match their previous audit.
No unchanged sibling suite was rerun.

| Checkout | HEAD | Actual implementations, callers and tests inspected |
| --- | --- | --- |
| MotorControl-RS | f03eda09db4b38ec7e8053132534b2de658f77a5 | `examples/common/RtuBusOwner.h` and `.cpp`: reserved urgent/recovery results, copied admission, nonwrapping IDs, sequence cancellation and retained uncertainty. `ProbeApp.cpp` urgent admission, cancel, recover and releaseRecovery callers; `bus_scheduling_test.cpp` urgent pressure, cancellation phases, deadline/recovery and delayed identical replies. |
| ADS1115 | 235af3e8c722661a89a44766a4ccd903a654f6fe | `02_owner_safe_poll/main.cpp`: application-owned lock/time, poll loop and matched terminal validity. No sensor retry policy imported. |
| INA228 | b28c5ea69bcb4d26b351c52c24d3c6fae9fd282b | Header identity/generation/validity facts; implementation `invalidateHardwareState` withdraws trust and cancels while retaining dirty diagnostics; `test_basic.cpp` invalidation and terminal retention cases. |
| PCA9555 | 498178e4d9e4acb6fab7d8b816ad5021efe8806a | Header cancellation/result rules, implementation cancellation/mandatory cleanup and native failure-prefix tests. Borrowed I2C transfer storage is not copied into the CAN contract. |
| FieldCore-node | 5ad9292dc8481cf08be9a61f662495a6474d1da0 | `Rs485MotorModule.cpp` producer/configuration invalidation and retained result correlation; `Rs485Task.cpp` settlement before cancellation/recovery and `txUnsettled` gate; host motor-module tests for retained prefixes/invalidation. No FieldCore changes. |

RS names `admitUrgent`, `cancel`, `recover`, `recoveryResult`, `releaseRecovery`,
`beginSequence`, `invalidate` and `invalidateProducer` retain their corresponding
roles. Deliberate CAN differences are explicit: `cancel` closes one request;
group revocation uses `invalidate`. Axis reservations and permanent SDO
quarantine remain separate. CAN processing-time cutoffs do not retroactively
accept buffered evidence as RS's settled receive-history path may do.

Reopened the selected adapter's `Driver.cpp`, `CO_driver_target.h`,
`SdoClient.cpp` TX/confirm/cancel/receive/retirement paths and `Pdo.cpp`
qualification boundaries. CANopenNode remains pinned to
`9b8beed8367241e96ac03f916cd5a500bcb2cf23`. Source-confirmed: stack staging is
locally retirable; `nextTx` permits possible external effect, `confirmTx` needs
actual evidence, and ordinary SDO responses carry no host token. Unknown:
real SDK/controller retirement and remote session/producer retirement bounds.
No manufacturer constant, ledger interpretation or timing guarantee changed.
Synthetic `0x2345`/`0x2346` requests are test work, not Lichuan commands or stops.
Nothing was physically measured or transmitted on CAN.

## Implementation and acceptance coverage

The authoritative API and timelines are in [can_owner.md](../can_owner.md) and
the source Doxygen; this report records decisions and verification rather than
copying those contracts.

| Requirement | Implemented artifacts and evidence |
| --- | --- |
| Independent urgent admission/results | Six ordinary slots plus one reserved urgent slot in `CanBusOwner`; recovery has its own retained slot. New operation identities may coexist with old retained terminals. Tests fill ordinary requests/results, retain urgent results and prove FIFO/explicit cancellation before new urgent intent. |
| Cancellation and retained effects | `cancel`, sequence/producer invalidation and native first-terminal retention; phase tests cover local queued, staged, uncertain enqueue, external accepted, TX-confirmed, protocol-confirmed and already-terminal work. A caller may cancel local work without claiming a motor stop. |
| Local/stack/SDK/controller retirement | One exact external frame/epoch record per channel, including cleanup aborts. Failed/missing retirement inhibits that channel; exact final TX reports or explicit successful whole-controller recovery can retire local work. Configuration advance also checks outstanding external handoffs. |
| Occupied-channel urgent policy | Same-axis FIFO, no implicit preemption, no quarantine bypass. Other-node work, heartbeat/PDO/EMCY and raw observation remain independently budgeted. Tests retain ambiguous SDO/PDO state through recovery. |
| Explicit bounded recovery | `recover`, `recoveryResult`, `releaseRecovery`, controller reporting and RX-overflow invalidation. One begin call and at most one poll per budgeted turn; immutable deadline/results, nonwrapping IDs/epochs and no hidden replay. Missing hooks, refusal, failure and timeout retain the TX interlock. |
| Time/evidence precedence | Processing-time expiry precedes buffered RX/cancel, inclusive cutoff wins, earlier published terminals survive. Qualified capture time remains separate; old ingress epochs reach diagnostics only. Tests include zero, backward time, maximum timestamps and late evidence. |

Central state remains in one implementer-owned application class with direct
fixed scans. A shared eligibility helper handles ordinary/urgent selection;
there is no second queue engine or runtime stop framework. The installed C++11
core and its dependency/export boundary remain unchanged.

Independent strong review found that clearing only an external record's pending
flag after a delayed final report left its blocked flag set. The final report
now clears the complete record. A response-before-TX-report segmented regression
proves subsequent progress. Review also corrected the documented cancellation
callback bound: up to two channel retirements may run, not only the selected
request's channel. Final review checks the integrated implementation and tests.

The final scope review also required explicit missing-hook and two-vendor cases.
The suite now tests null begin/poll/both recovery callbacks and uses separate
abstract Lichuan/N5 profile bindings through one real service backend. One
confirmed result and one unconfirmed write retain their original identity and
evidence across recovery. These fixtures do not implement either vendor's motion
sequence. The fake controller models pending frames separately from enqueue
history and can drain them explicitly; positive retirement actually prevents
their later departure, while failed retirement preserves that possibility.

## Documentation and delegation

Updated public application Doxygen, owner guidance, native example callback
signature/README, root README/CHANGELOG, API map, source map, coverage, verification
inventory, progress, backlog and roadmap. These explicitly retain unsupported
physical retirement/session recovery and Prompt 31 stop integration.

`p24_source_audit` (`gpt-6.1-sol`, high) inspected sibling/selected-stack sources
and owned owner/example guidance. `p24_tests` (`gpt-6.1-sol`, high) owned the
independent adversarial suite. `p24_contract_review` (`gpt-6-astra`, high)
challenged state, occupied-channel and stale-TX premises. Lead owned central
state/headers, test registration, integration, verification and Git. Workers
made no Git, sibling or hardware changes.

## Verification and handoff

Local tools: Python 3.12.10, GCC 15.1.0, CMake/CTest 4.0.1, Ninja 1.12.1,
Doxygen 1.13.2 and isolated clang-format 14.0.0. Local Linux sanitizers are
unavailable; the existing exact-commit Linux jobs supply that check.

The initial quick run built successfully and passed 39/40 contracts. One old
owner assertion required consuming all terminal results before the next operation;
this now contradicts the intentional retained-results/urgent contract. It was
replaced with an assertion that the next operation starts and the old terminal
is unchanged. The segmented stale-report scenario now supplies the actual first
frame TX report before another external handoff, then proves a stale report
cannot affect that next frame. Focused owner/native consumer checks pass after
these contract corrections; no timeout or verification threshold was weakened.
Initial evidence is retained in
`.build/p24-quick/20261031T220509106881Z-32812-anexhpue`.

The first full verifier passed 42/42 contracts, independent package gates and
strict Doxygen, retained under
`.build/p24-full/20261031T221458443512Z-70216-0hlcwi12`.
The final two-vendor test initially omitted the owner's required network binding
setup and correctly failed reservation admission. Adding that explicit setup
corrected the fixture without changing implementation or bypassing validation.
The rebuilt owner, recovery and native consumer tests pass 3/3; direct suites
pass 2,253 owner checks and 1,440 recovery checks. On Windows x64 GCC,
`sizeof(CanBusOwner)` is 3,432 bytes and `Completion` is 272 bytes, plus the
separately owned service adapter. These are host ABI measurements, not board
memory or service-gap guarantees.

Final integrated run:
`python scripts/verify.py --mode full --build-dir .build/p24-final`, retained at
`.build/p24-final/20261031T221639418840Z-80492-k6fbpwxo`.

| Gate | Final local result |
| --- | --- |
| Strict Release native build and registered contracts | Pass, 42/42; owner/service/native consumer and new recovery suite included. |
| Independent source/export/install package consumers | Pass; C++11/C++17 and no-RTTI, unchanged 22 installed headers without example owner/stack/SDK dependencies. |
| Strict Doxygen 1.13.2 | Pass; 22 installed public headers plus application declarations, 1,377 members and 61 free functions extracted. |
| Repository, version and reviewed-object freshness | Pass in the full verifier. |
| Promptset integrity | Pass; 137 original artifacts, 99 EDS indexes/202 typed entries, 53 manual-only items and 24 conflicts retained. |
| clang-format 14 dry-run and `git diff --check` | Pass for changed C++ and task-owned diff. |

Final independent ownership and scope reviews have no remaining findings.
Local checks cover integrated working changes over entry `29906a2`; the delivery
commit is identified in the handoff rather than misattributing them to that entry
revision. Staged/HEAD integrity, push, fresh ahead/behind and exact final-commit
CI are checked at delivery; the CI receipt is retained locally as
`.build/p24-final-ci.json`. No manual CI dispatch or workflow change is needed:
the established push trigger supplies Windows quick and Linux GCC/Clang full
ASan/UBSan jobs.

Remaining limits: no real controller/SDK retirement claim, no remote SDO channel
reopening or PDO provenance renewal from a host epoch, no actual motor stop and
no hardware qualification. Prompt 25 can consume these owned mechanisms while
the inherited prerequisite status and the separate firmware gates remain visible.
