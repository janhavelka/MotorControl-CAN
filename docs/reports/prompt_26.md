# Prompt 26 - native examples and shared application

The [fresh independent 25/26 audit](prompts_25_26_audit.md) rechecks actual native,
application, package and lifetime behavior. It fixes an owner health-history gap;
no additional application defect or support claim was found.

Date: 2026-11-01. Same promptset v2, original base `7878f7c`, branch
`promptset/motorcontrol-can`, upstream `origin/promptset/motorcontrol-can`.
Entry `fce1a25` had clean worktree/index; fresh fetch showed 0 ahead/0 behind.
Prerequisite reports 10, 19 and 25 and actual code/checks were read.

## Owned scope and status

*Packet 1 re-evaluated this stage as Complete with inherited gates, under the usable-prerequisite rule. See [PROGRESS](../prompts/PROGRESS.md).*

**Owned software delivered; stage Partial solely on inherited 19/25/Q20.**
The missing source-backed non-consuming bootstrap probe remains Prompt 19's
obligation; native fake mode explicitly supplies its fresh/usable-session premise.
No Prompt 26 application, example or test is deferred to that evidence gate.
No installed drive support, real controller, firmware/CLI or physical qualification
is added. Prompt 27's real TWAI/controller retirement audit is next executable work.

The [application guide](../native_application.md) owns responsibility, time,
retention and capacity rules. New ProbeApp/ProbePlatform are example-only;
no installed core or existing service/node storage was changed. One selected
target and concrete typed context wrap explicit identity/configuration reads.
The existing owner remains the only CAN receiver. The application services it
first even under full output, admits at most one new field per turn and leaves
each canonical terminal available for explicit inspection before advancement
and release. Raw typed observations remain currentness-checked; model matching,
status interpretation and scale qualification are not inferred.

ProbePlatform copies complete output submissions into a 256-byte ring, drains
at most 16 bytes/turn, and retains suffixes under refusal. Its 64-byte input ring
accepts at most eight callbacks/turn; newest overflow bytes are counted/dropped.
Counters saturate. No input parser exists. Time acquisition and one optional
bounded idle/diagnostic callback stay at this application seam. Diagnostic copies
use the existing owner pull API, without consuming authoritative RX.

The existing units_preview now explicitly demonstrates successful exact native
preparation, a stale-generation rejection preserving sentinel -123456789 and
preserved binary32 output after excessive-error rejection. Command increments
4096/turn and encoder feedback 2048/turn remain independent illustrative facts.

native_probe accepts only `--fake`. Its deterministic transport owns 16 ingress
frames and four TX reports. Each upload handoff preflights all queues before
publishing a separate controller report, heartbeat, EMCY, unrelated-node frame
and response. Fifteen explicit uploads use the ordinary owner/adapter and public
typed read API, not fabricated read completions. Synthetic values are not vendor
defaults or identity evidence. No write, NMT command, retry or startup request is
implemented. Unknown upload effects retain their existing uncertainty contract.

## Refreshed source evidence

All listed siblings were initially clean; no sibling edits or unchanged suites
were performed by this task. RS advanced concurrently after the initial audit:
the final inspected HEAD is `aa9946971b92769216949379c7f35ecb66fcf676`, with
25 modified paths in homing implementation/tests, console formatting, evidence
scripts and related docs. Those changes were preserved. The inspected current
ProbeApp.cpp/ProbePlatform.h and owner source remain unchanged; modified
probe_app_monitor_control_test.cpp changes only homing support expectations.
ADS1115/INA228/FieldCore remain clean at the revisions below. A HEAD alone does
not identify RS's concurrent dirty homing behavior; none is imported here.
Since Prompt 25 RS advanced from `f03eda09` through homing commits
`e157ce0`, `74173ca`, `51fef5e`, `aa99469`. Its relevant seams did not change.
Its installed consumer now recognizes implemented homing method 24; that does
not add a CAN homing API here.

| Checkout / current revision | Read implementation and actual caller/test evidence |
| --- | --- |
| RS initial `51fef5ed41c9ec232016e6c3116bbb239c24979b`; refreshed `aa9946971b92769216949379c7f35ecb66fcf676` with dirty paths above | examples/probe_cli/ProbePlatform.h:7–17 supplies cooperative console/idle seam; ProbeApp.cpp:247–269 bounds output and :3096–3159 services owner before bounded input/output. test/probe_app_test.cpp:134–186 checks blocked/partial output and retained results. Installed consumer CMake/main and verifier source/export/install checks were inspected. |
| ADS1115 `235af3e8c722661a89a44766a4ccd903a654f6fe` | src/ADS1115.cpp:537–571 supplied time/work budget and :1079–1094 token result; examples/02_owner_safe_poll/main.cpp:195–264/321–344 actual result/health/time caller; test/test_basic.cpp:2108–2142 zero budget/stable terminal. |
| INA228 `b28c5ea69bcb4d26b351c52c24d3c6fae9fd282b` | src/INA228.cpp:870–940 checked state/IDs and :1395–1461 supplied time/bounded poll; examples/01_basic_bringup_cli/main.cpp:336–406 matched terminals/deadline; ESP-IDF basic main.cpp:9–11/97–117 external ownership/cached health; test_basic.cpp:5009–5035/5374–5400 budget and passive health. |
| FieldCore `5ad9292dc8481cf08be9a61f662495a6474d1da0` | Rs485MotorModule.cpp:340–363 one yielded transaction/correlated event and :640–680 retained result/invalidation; Rs485Task.cpp:1181–1205 matching completion; test/host/rodtester/motor_module.cpp:77–149/233 direct contexts and ACK versus stopped. Read only; no CAN integration inferred. |

Current CAN verifier/workflow/explicit test registration and sibling workflows
were inspected. Reused supplied time, retained ownership and bounded output
patterns. Deliberate differences: RS ProbeApp imports Esp32S3Uart/SDK/PSRAM;
this application has no SDK/framework types. Sensor initialization, retry,
failure thresholds and latching admission policy are not imported.

Original EDS sections 1000,1009,100A,1018:00–03,2000,2001 and communication
1005,1014,1017,1200:00–02 were reopened. U16 versions, U8 counts and U32 routes
match the existing reviewed read declarations. There is no 1018:04 entry.
No manual bit/sequence/timing fact is newly implemented; the previous documentary
conflicts and Q20 remain unchanged. Routes and fixture values are explicit
synthetic premises, not measured configuration or a new permission claim.

## Verification

Native tools: Python 3.12.10, GCC 15.1.0, CMake/CTest 4.0.1, Ninja 1.12.1,
Doxygen 1.13.2 and clang-format 14.0.0. Linux sanitizers/Clang are checked by
the existing exact-commit CI jobs, not claimed as locally available on Windows.

Focused Release source build and CTest passed 9/9: probe_app, native_probe,
units_preview, owner, recovery, health, reads, network events and PDO observations.
The new application suite passed 1310 checks: typed success/configuration,
full high-bit abort, timeout/uncertainty, unrelated traffic, retained lifetimes,
no startup commands, output pressure/input overflow, callback/time failure,
queue cancellation/cutoff, boot-up invalidation and fake atomic capacity refusals.

Integrated commands and actual results:

```text
cmake -S . -B .build/p26-native -G Ninja -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=ON
cmake --build .build/p26-native --parallel 2
ctest --test-dir .build/p26-native -R "^(probe_app|native_probe|units_preview|can_bus_owner|can_bus_recovery|can_bus_health|reads|network_events|pdo_observations)$" --output-on-failure
python scripts/verify.py --mode quick --build-dir .build/p26-quick
python scripts/verify.py --mode full --build-dir .build/p26-full
python scripts/check_repository.py
python test/repository_test.py
doxygen Doxyfile
python scripts/check_promptset.py
git diff --check
```

- Focused 9/9, new application 1310 checks; native --fake and offline preview
  both exit zero using their documented example-only commands. No Python package
  discovery occurs with BUILD_TESTING=OFF. The installed units_preview also built
  and ran separately. Invalid live-mode argument exits 2, without opening hardware.
- Quick **44/44**, logs `.build/p26-quick/20261101T082248868871Z-35848-kbt35c_h`.
- Full **45/45**, plus independent package and documentation gates, logs
  `.build/p26-full/20261101T082248867871Z-89288-p17zq627`. Summaries retain
  command arguments, tool versions, statuses and artifact paths.
- Isolated exported source and relocated installed consumers pass C++11,
  C++17/no-RTTI and independent public headers. New copied application/adapter/fake
  sources build against only the relocated installed core; both real examples run
  from that consumer. Checkout-relative core includes, sibling repositories, SDKs,
  Python, source ledger, reference documents and network access are unnecessary.
  The pinned optional stack is an explicit local application dependency.
- Exact installed file list remains unchanged at 22 public headers plus library,
  CMake metadata and software license. Neither shared application nor fake is
  exported as production profile support. Missing public dependency still fails.
- Strict Doxygen covers 22 installed headers plus the application/adapter
  interfaces: 1495 extracted members and 61 installed free functions. Formatting
  dry-run/Werror passes all nine affected C++ paths. Repository checker, its 15
  negative/positive tests, version/object freshness and promptset integrity pass.
- Working and staged/committed provenance checks preserve all 137 protected
  artifacts, 99 EDS indexes, 202 explicitly typed entries and 53 manual items.
  No artifact or attributes were modified. The final commit check follows commit.

Measured GCC Windows x86-64 instance sizes: FakeTransport 816 bytes, existing
SdoClient 10528, CanBusOwner 6744, ProbeApp 2056 and ProbePlatform 376. The native
demo prints its own sizes. These are object storage measurements, not embedded
stack/heap/timing or physical CAN qualification. There is no new controller
service-gap guarantee; callbacks must meet their documented bounded contract.

## Review and documentation

Reused the actual Axis/Units, Lichuan read lifecycle, CanBusOwner, CANopenNode
adapter and private saturating counter helper. No universal operation engine,
parallel node table/receiver, runtime plugin or future motion stub was introduced.
New modules separate current application policy from external platform facilities.
The fake stays in its example target and is excluded from core distribution.

Independent review found and fixed backdated consumption of owner-local terminals.
A regression keeps cancellation at time 10 retained when consumed at time 0.
Review also simplified all terminal consumption to the owner's canonical copied
service evidence, preserving early queue timeout provenance and honest no-TX
uncertainty instead of relabeling it as invalidation. No remaining review finding.

Delegation: gpt-6.1-sol/high audited sources and owned units_preview only;
gpt-6.1-sol/high owned native fake/demo/test files; gpt-6-astra/high independently
reviewed routing, retention, deadlines and pressure. Lead owned common contracts,
CMake/checker/package/docs integration and all Git operations, and ran integrated
checks. Worker passes alone were not used as final integration evidence.

README/CHANGELOG, example READMEs, application/owner/architecture guides,
source map, API/CLI map, coverage/backlog/roadmap, verification and PROGRESS
were updated together. New shared headers are included in strict Doxygen.
Core install/export remains unchanged; descriptive dictionaries/reference material,
SDKs, sibling snapshots and example support remain optional/excluded.

## Commit, synchronization and next work

Lead stages explicit owned paths, reviews the complete staged diff, commits and
pushes on the recorded execution branch, then verifies fresh upstream equality.
This application/build integration is a milestone; the existing automatic
Windows quick and Linux GCC/Clang sanitizer jobs must be checked on the exact
resulting commit. Final SHA, run URL, terminal status and sync result are recorded
in the final handoff and ignored `.build/p26-final-ci.json` receipt, without
claiming that YAML or an older passing run executed the new commit.

Only Prompt 26 was assigned. Next executable work is Prompt 27's TWAI adapter
and real SDK/controller evidence, consuming this completed shared application.
Q20 and real remote-channel reopening retain their existing evidence obligations;
no hardware qualification or firmware build was performed here.
