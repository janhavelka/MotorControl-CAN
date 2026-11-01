# Fresh independent audit of prompts 25 and 26

Date: 2026-11-01. Re-read both original prompts in full, shared contracts and
root/parent guidance. Inspected actual commits `fce1a25` and `2b85103`, current
source, callers, tests, package checker and workflow. Entry was clean at
`2b851034cd4647861a4fa84570485ce380b9f3dd` on the existing
`promptset/motorcontrol-can` branch; fresh upstream comparison was 0/0.

## Finding and correction

One confirmed defect: CanBusDiagnostics.cpp required a PDO sample to be FRESH
before recording its checked delivery time. An accepted, currently qualified
sample with capture time 5, processing time 11 and age cap 5 therefore left
`lastSeenUs` at 4. This contradicted the separate processing-time history contract.
It did correctly refuse READY, but lost valid delivery diagnostics.

The lead reproduced the new regression failure before changing production code;
logs are `.build/audit2526-before-fix.log` and the independent reviewer's
`.build/audit2526-health/delayed-pdo-before-fix.log`. The narrow fix accepts
FRESH or EXPIRED samples for historical delivery. The existing observation
capture-age check still controls current communication evidence. All map,
binding, lifecycle, qualification identity and capture-boundary checks remain.
Neither an expired delivery nor a new host epoch makes stale data current.

The extended [health regression](../../test/can_bus_health_test.cpp) checks the
delayed qualified sample, unchanged service totals, malformed mapped length,
unknown capture, existing revoked/requalified sample history and a later fresh
sample restoring READY. Doxygen and the [owner guide](../can_owner.md) now state
the distinction explicitly; CHANGELOG records the corrected behavior.

No other confirmed production, packaging or scope gap was found. Prompt 26's
explicit local read-withdrawal uncertainty was investigated: it deliberately
uses the documented conservative core read contract when no canonical owner
terminal exists. It is not a retry, fabricated transmission or new uncertainty
settlement route. No speculative API or generic framework was added.

## Coverage and independent review

| Original requirement | Actual inspected implementation and evidence |
| --- | --- |
| Cached binding health, timestamp zero, passive precedence and no admission policy | CanBusDiagnostics.cpp, owner interfaces and health tests; getters remain I/O/clock-free. Heartbeat disabled/unknown alone is inconclusive; raw drive status stays uninterpreted. |
| Logical service counting, segments, abort, cancellation and retained failure | Owner terminal hooks and health tests exercise one count per attempted SDO, neutral local/unsupported work, abort liveness and later independent success without rewriting an earlier failure. |
| Exact interface counter boundaries and reset lifetimes | CanBusOwner.cpp, private saturation helper and tests cover actual TX reports, refusal deduplication, reset, rebind, exhaustion and preserved live evidence/uncertainty. |
| Bounded diagnostics and fair ordinary/urgent service | Fixed trace/resource storage, observer-copy paths and full-history tests. Callbacks must remain bounded; stalled pull consumption cannot drain authoritative RX or block service. |
| Existing exact offline example | units_preview uses public Axis/Units, independent command/encoder scale, successful preparation, meaningful rejection and unchanged output. |
| Native fake and shared application | ProbeApp/ProbePlatform and native_probe use the ordinary owner, explicit supplied time/requests, raw typed getters and retained results; no second receiver, startup traffic, SDK or temporary motion/parser API. |
| Bounded application I/O and retention | Probe tests cover overflow/backpressure, owned submissions, failure/deadline, unrelated traffic, queue cutoff, boot-up invalidation and explicit release. Independent scratch tests also compare 1,000 ring-wrap turns against FIFO models and exercise retained completion across reboot/deadline. |
| Build/package/docs boundaries | Actual CTest inventory and checker, strict Doxygen inputs and isolated installed-core application consumer were inspected and executed. All 22 installed core headers remain unchanged; application/fake/owner/stack stay optional and uninstalled. |

Delegation used disjoint/read-only tasks on the same branch: gpt-6-astra/high
reviewed health, counters, uncertainty and state; gpt-6-astra/high reviewed the
shared application and adversarial lifetime/ring cases; gpt-6.1-sol/high audited
coverage, package consumers, original evidence and live siblings. Only the health
reviewer edited the assigned regression file. The lead verified the finding,
owned production/docs integration and Git, and ran integrated verification.
No reviewer changed a sibling, Git state or hardware.

## Fresh source and applicability snapshot

All following worktrees were clean when inspected. RS advanced from Prompt 26's
`aa994697` through negative-limit homing implementation/evidence; the owner,
counter, ProbeApp/platform seams consumed here remain unchanged. Its new homing
capability is not imported into CAN. Other revisions match the prior stage.

| Checkout / revision | Implementation, real caller and test inspected |
| --- | --- |
| RS `6363afc724c0b94169652cd93074293d4ce56862` | examples/common/RtuRunner.h/.cpp counters/reset/trace, examples/probe_cli/ProbeApp.cpp/ProbePlatform.h, test/runner_test.cpp/traffic_test.cpp/probe_app_test.cpp, installed consumers and actual verifier/workflow. |
| ADS1115 `235af3e8c722661a89a44766a4ccd903a654f6fe` | src/ADS1115.cpp:2656; owner-safe-poll caller:195–264, cached HealthView and test_basic.cpp:5434/5510. |
| INA228 `b28c5ea69bcb4d26b351c52c24d3c6fae9fd282b` | src/INA228.cpp:2966–3018; CLI matched-result/time callers:345–400; test_basic.cpp:5298/5354/5374. |
| SHT3x-main `f65f15573e3c9b1ef04cce72ad08f307d021e563` | src/SHT3x.cpp:2550–2625, Sht3xCli.cpp:654–715, test_basic.cpp:4598 and health runner. |
| MB85RC `1bf7c293549b1038b2d8f3b7a8d655fa74201e6e` | src/MB85RC.cpp:284–301/2126–2164, DiagnosticCore.h:143–170, wrapping-total regression test_basic.cpp:4246. CAN deliberately saturates selected totals. |
| FieldCore `5ad9292dc8481cf08be9a61f662495a6474d1da0` | SystemHealthProjection.h:246–315 and runtime/native callers; Rs485MotorModule.cpp:340–363/640–680, Rs485Task.cpp:1181–1205. Read-only reference; no CAN integration claimed. |

Sensor retries, latches and thresholds remain excluded from CAN admission.
Current source/checker/runner/workflow behavior was read; unchanged sibling
suites were not rerun. Original EDS 1017 and 6041 sections were reopened, and
fresh original PDF renders confirm EN86 page 12 alarm bit 7, ZH86 page 12 bit 3
and OP86 page 10 bit 7/running bit 14. OP86's page header still names 57C despite
the retained 86 filename. Four original hashes match the manifest. No firmware
identity, watchdog behavior, effect permission or status dialect is established
by freshness. Original bytes, ledger and access policy were not changed.

## Verification and handoff

Independent reviewers passed focused health/owner/recovery 3/3, native/application
3/3 and coverage/consumer 5/5 builds. Application scratch tests passed 22,057
checks, including independently modeled FIFO wrap/backpressure; scratch source
and binaries remain in `.build/audit2526-app`. Its renamed original main has a
return warning; strict production builds were checked separately. Coverage
review separately built and ran the documented native examples with testing off,
checked invalid --live exit 2 and passed the full isolated package contract.

Lead rebuilt the integrated tree and passed focused 11/11: units_preview,
native_probe, probe_app, owner, recovery, health, owner_preview, SDO lifecycle,
SDO reconciliation, network events and PDO observations. The health executable
now passes 891 checks; application remains 1310. Object sizes and resource/work
bounds are unchanged.

Integrated commands and final local results:

```text
cmake --build .build/p26-native --parallel 2
ctest --test-dir .build/p26-native -R "^(can_bus_health|can_bus_owner|can_bus_recovery|probe_app|native_probe|units_preview|owner_preview|sdo_transfer|sdo_reconciliation|network_events|pdo_observations)$" --output-on-failure
python scripts/verify.py --mode quick --build-dir .build/audit2526-quick
python scripts/verify.py --mode full --build-dir .build/audit2526-full
python scripts/check_repository.py
python scripts/check_promptset.py
git diff --check
```

- Focused 11/11, quick **44/44**, full **45/45** plus isolated package and strict
  Doxygen. Logs are `.build/audit2526-quick/20261101T084040377962Z-81572-xhvt7yym`
  and `.build/audit2526-full/20261101T084040310758Z-60328-t4b_ww6_`.
- GCC 15.1.0, Python 3.12.10, CMake/CTest 4.0.1, Ninja 1.12.1, Doxygen 1.13.2
  and clang-format 14.0.0. The full gate extracts all 22 installed headers,
  application/adapter interfaces, 1495 members and 61 installed free functions.
- Explicit registration, version/object freshness, repository checker, style and
  diff checks pass. Promptset integrity preserves 137 protected artifacts and
  full 99-index/202-typed-entry/53-manual-item accounting. Staged and committed
  provenance are checked at Git closure; no original/generated byte changes.
- Independent package builds use exported source and relocated installed core,
  including the copied native application, C++11/C++17/no-RTTI and header tests.
  No SDK, sibling, Python, source ledger or network dependency leaks into core
  consumption. Linux ASan/UBSan/Clang are not claimed locally available on this
  Windows host; their actual automatic CI outcomes are verified separately.

The original stages retain Partial solely on their inherited Q20 prerequisite;
this audit adds no new blocker or deferred software task. Backlog/roadmap/native
ledger ownership is unchanged. No firmware build, controller/motor traffic,
hardware qualification or Prompt 27/28 implementation was performed. Final
commit/CI receipt belongs to this audit, not the older passing stage commits.
The lead commits explicit owned paths and pushes this same branch, then checks
fresh ahead/behind and existing automatic CI on that exact commit. Final SHA,
run URL/status and synchronization are in the final handoff and ignored
`.build/audit2526-final-ci.json`; no manual workflow dispatch or trigger change.
