# Prompt 15 Passive network events and invalidation

## Scope and execution

Implemented pure passive CANopen event envelopes and continuous node evidence
in the existing optional application-owned channel table. Explicit NMT,
heartbeat/PDO configuration, drive actions, recovery/reopening and hardware
traffic are outside this stage. Started clean at
`a473e6e3b1851dcad59a5de56dd0d330b6133e49` on the continuing promptset-v2
`promptset/motorcontrol-can` branch. Fresh fetch confirmed 0 ahead / 0 behind
`origin/promptset/motorcontrol-can`; no branch or sibling changes were made.

## Delivered contracts

| Requirement | Actual implementation and evidence |
| --- | --- |
| Pure checked representations | [Network.h](../../include/MotorControlCAN/canopen/Network.h) / [core codec](../../src/canopen/Network.cpp): explicit configured IDs, complete binding/epoch, standard data/length checks, supported heartbeat states and all raw EMCY bytes; failure output remains unchanged |
| One bounded storage owner | [SdoClient](../../adapters/canopennode/SdoClient.h) extends its existing two slots with one passive anchor per physical node, shared across that node's SDO channels. Passive-only configuration needs no SDO session premise; no parallel registry or receiver |
| Independent evidence | [NetworkState.h](../../adapters/canopennode/NetworkState.h): controller reports, producer interval facts, heartbeat/NMT/checked SDO times and raw EMCY remain independent. Drive state/mode, cache validity, faults and command results belong to their actual application contexts |
| Complete invalidation | [Owner projection](../../adapters/canopennode/Network.cpp): boot-up, reset, binding/dialect/configuration withdrawal and reported controller recovery synchronously revoke dependencies and retire matching SDO work; no replay or implicit enable/start/fault clear |
| History and diagnostics | Four copied records, drop newest with saturating loss count; state updates and callbacks precede history pressure. Unknown/rejected/stale records preserve raw bytes and supplied old provenance. Latest EMCY/counter survives as separate history |
| Clock and lifetime | Separate known/unknown capture and processing time; zero valid, future/backward inputs reject atomically, per-fact ordering, explicit inclusive age policy, nonwrapping event epochs and fail-closed exhaustion |
| Existing consumers | Retained fit facade and outer owner forward frames through the production table once; their raw observation/counter views consume copies. Installed source/consumer/header checks exercise the new stack-free codec |

Boot-up is not a heartbeat. Unknown producer timing, explicitly disabled,
never observed, unqualified and expired are distinct. A later heartbeat does
not requalify old configuration/reference or alter a drive fault/SDO outcome.
A late boot-up not proven before the lifecycle boundary still invalidates;
newer NMT history retains its original epoch instead of being stamped current.

## Explicit passive implementation decision

The [maintained stack decision](../decisions/canopen_stack.md#prompt-15-passive-boundary-correction)
records a narrow change from the initial native HB/EM consumer plan. Pure
checked envelopes plus one owner projection are the only active passive
consumers. CANopenNode remains the SDO engine; active NMT/PDO duties remain
16-18. This does not claim native HB/EM consumers run in production and leaves
no duplicate-consumer integration task pending for heartbeat or EMCY.

The pinned native heartbeat consumer accepts unsupported bytes and can coalesce
reset/state arrivals; the emergency callback lacks a context, length check and
arbitrary configured-route dispatch. Using those alongside the projection
would duplicate timing/state or require global callback state. An isolated
actual-stack parity harness verifies the supported bytes and demonstrates
those limits. All callbacks in the production projection retain ordinary
caller-owned context, no global registry.

## Source confirmation and recurring audit

Original `docs/reference/vendor/LC_StepDriverCAN_V1.1.eds`:
`1014:00` lines 252-259 is RW U32/default `$NODEID+0x80`, non-PDO;
`1017:00` lines 261-268 is RW U16/default zero, non-PDO. Sections
`1016`, `100C`, `100D` are absent. This is not measured firmware rejection.
EN86/ZH86 PDF page 3 (printed 1), including rendered communication tables,
lists EMCY without defining a host-loss stop policy or manufacturer bytes.
No default is installed as live configuration; no source conflict or ledger
permission was changed. No fact was physically measured.

Selected CANopenNode pin `9b8beed8367241e96ac03f916cd5a500bcb2cf23`:

| Primary source | Verified fact / acquired SHA-256 |
| --- | --- |
| [CO_NMT_Heartbeat.c](https://github.com/CANopenNode/CANopenNode/blob/9b8beed8367241e96ac03f916cd5a500bcb2cf23/301/CO_NMT_Heartbeat.c) | Millisecond conversion and boot-up even with producer interval zero; `2ad2b2f92ea1198de0ca3b28465d50039a8de09f6f33587ee9735c7b63e2589d` |
| [CO_HBconsumer.c](https://github.com/CANopenNode/CANopenNode/blob/9b8beed8367241e96ac03f916cd5a500bcb2cf23/301/CO_HBconsumer.c) | Raw state callback/default route and boot-up versus active monitoring; `8507404f1fcf5e66680699cb3f698ea805d337a6f5c01e7688a780ac54ca0a8e` |
| [CO_Emergency.c](https://github.com/CANopenNode/CANopenNode/blob/9b8beed8367241e96ac03f916cd5a500bcb2cf23/301/CO_Emergency.c) | Eight-byte raw decode, callback/default filter limitations; `be85a4515c3b5b0c0da37e9025a4f60d8eae4fef739d7b4df463c16705bb06cb` |

Acquired exact raw bytes and Git blob hashes, parity harness/config/commands
and results are retained under `.build/p15-protocol/` (`SOURCES.json`,
`run_parity.ps1`, `parity.cpp`, `results.txt`). This is test corroboration,
not a production build dependency or manufacturer semantics. Retained original
headers/EDS/PDFs and the eleven-file offline engine pin remain unchanged.

All refreshed sibling checkouts were clean, unchanged from prompt 14:

| Checkout | Revision | Actual implementation/caller/test seam |
| --- | --- | --- |
| MotorControl-RS | `a5a02b74a536fe5875e6256a32477dc850d96807` | `examples/probe_cli/StateCache.h` current/fresh/stateResult; ProbeApp invalidation/harvest; RtuBusOwner producer generations, RtuRunner copied traffic; probe_app/traffic/runner tests |
| ADS1115 | `235af3e8c722661a89a44766a4ccd903a654f6fe` | `src/ADS1115.cpp` bind/start/poll/take/cancel; owner-safe poll example; basic cancellation/retention tests; FieldCore tracked ingress |
| INA228 | `b28c5ea69bcb4d26b351c52c24d3c6fae9fd282b` | `src/INA228.cpp` conversion origin/CNVRF and configuration generation; public continuous-read contract and destructive-read/freshness tests |
| FieldCore-node | `5ad9292dc8481cf08be9a61f662495a6474d1da0` | `src/i2c/I2cDiagnostics.cpp` tracked command identity; Ads1115Module cached timestamp/invalidation; Rs485MotorModule binding/terminal invalidation; Rs485Task axis/reference withdrawal and native consumer tests |

CAN deliberately keeps per-event capture/processing provenance, processing
cutoff precedence and drop-newest history. RS captured RTU closure/silence and
sensor conversion readiness are not CAN node/session freshness. No unchanged
sibling suites were run and no sibling code or hardware was touched.

## Delegation and review

Read-only source reviews and disjoint pure-codec/event-vector implementation
ran in parallel using the inherited model/effort. A fresh `gpt-6-astra` high
review independently challenged reset, late timestamps, shared anchors and
generations. One lead owns routing, storage, public integration and Git.

Confirmed review corrections: legacy `invalidate(slot)` now revokes attached
node/cache evidence; stale explicit ingress retains original route tags; an
invalidated passive-only node cannot gain a fresh SDO alias through another
slot. An independent executable tests those paths, future time, known zero,
independent heartbeat/EMCY ordering, observer deduplication and epoch maximum.
Actual axis-cache tests invoke `invalidateAxisReference(COORDINATES)`, revoke
readback scales and reject prepared targets while retaining faults/results.
The retained fit's existing short-SDO filter remains a compatibility policy;
production malformed SDO closure is tested directly. No assertions or gates
were weakened to finish migration.

## Verification and handoff

Focused integrated strict C++11 build and eight codec/lifecycle/owner/consumer
contracts pass. `network_events_test` reports 1,102 checks. Pure codec vectors,
independent actual-stack parity and the stronger reset/late-event executable
pass. Repository checker plus its 15 registration/failure tests and direct
Doxygen 1.13.2 pass. Final quick/full, package, integrity and final-commit CI
results are recorded before handoff.

`python scripts/verify.py --mode quick --compiler gcc --build-dir .build/p15-quick`
passed 26/26 contracts, retained as
`20261029T201708644968Z-81960-3vt5pote`.
`python scripts/verify.py --mode full --compiler gcc --build-dir .build/p15-full`
passed 27/27 non-package CTests, separate source/export/relocated installation
consumers, 13 independent public headers, C++11/C++17/no-RTTI and strict
Doxygen/API extraction. Final integrated run:
`20261029T201958187468Z-14960-rzxwoxfl`. The preceding run caught the missing
new-source entry in the package test's exact expected inventory; that inventory
now includes `src/canopen/Network.cpp`, with its strict equality check intact.
The failed log remains in run `20261029T201827117156Z-64396-wfm358uk`.

Local tools: Python 3.12.10, CMake/CTest 4.0.1, Ninja 1.12.1, GCC 15.1.0 and
Doxygen 1.13.2. Local Clang/Linux ASan/UBSan are unavailable on this Windows
host; the existing final-commit CI supplies those jobs. Planning integrity,
generated version/object freshness and diff checks pass; staged and committed
original-artifact checks are repeated around delivery. All 134 originals,
99 EDS indexes, 202 typed entries and 53 manual functions remain accounted for.

Current Windows x86-64 sizeof values: SdoClient 3000, compatibility facade
3160, outer proof owner 4152, request 112 and completion 208 bytes. These nested
sizes overlap and do not establish embedded stack/RAM, service timing or load
qualification. Fixed capacities and per-call work are documented in the source.

Software scope is complete. Final commit, upstream synchronization and exact
Windows/GCC/Clang CI status are checked before handoff and reported with the
resulting SHA; adding or preserving workflow YAML is not treated as execution.

Reused/simplified: existing channel table, SDO engine, frame/Status/binding
checks, axis invalidation and verifier/package pipeline. New code is only pure
event values/checks, one passive projection and a four-record copied history;
no allocation, background receiver, plugin framework, native consumer duplicate
or generic health machine. Public Doxygen, service/owner guides, package
consumer, source map, coverage, backlog, roadmap and PROGRESS accompany it.
No confirmed prompt-15 software obligation remains assigned to a later stage;
real controller/firmware/drive/watchdog qualification remains unperformed.
