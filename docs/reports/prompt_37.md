# Prompt 37: explicit discovery and host binding

## Scope and execution

Implemented inventory, checked minimal probe, explicit host selection and finite
passive/probe/identity discovery over existing application-owned storage. The
consumed prerequisites are prompt19's DEC-1 accepted typed reads and prompt25's
owner/cached observations, including the later checked channel/epoch and drain
contracts. Their unresolved device evidence does not prevent these artifacts.
Q01/Q20 still prevent automatic exact-model/read-effect claims; Q18 gates real
channel reopening. No wire configuration or persistence belongs here.

Entry `86d7ab1` was on `promptset/motorcontrol-can`, tracking
`origin/promptset/motorcontrol-can`, 0 ahead/0 behind after fetch, with no staged
changes. Three unrelated untracked `.vscode` JSON files were preserved. This is
the same promptset-v2 execution based on `7878f7c`, not a new branch/worktree.
No port, live discovery, flashing or motor activity was performed. Physical
discovery/commissioning remains separately recorded bench work.

## Source audit and device evidence

| Checkout | Current revision / dirty state | Adopted / rejected seam |
| --- | --- | --- |
| MotorControl-RS | `cd27b12389f568198b72cecb6606123ea4c089bc`, clean | `include/MotorControlRS/Discovery.h`, `src/Discovery.cpp`, `profiles/ess_rs/Discovery.h/.cpp`, `DiscoveryScan`/`DiscoveryApp` and `test/discovery_test.cpp` / `discovery_app_test.cpp`: adopt static implemented inventory, prepare/check probe, application scan/results and finite bounds. Reject serial tuple switching, ESS register/address assumptions and its nonChanging=true guarantee as CAN authority. This discovery seam was newly read at the unchanged HEAD. |
| MB85RC | `1a45e882bb3b1d29f5a18538a7658b0ed0f8a424`, clean | `src/MB85RC.cpp::readDeviceId/_selectVariant/_validateActiveDeviceId` and `test/test_basic.cpp::test_bind_is_zero_io_and_fixed_variant_is_immediately_usable`: adopt explicit zero-I/O selection distinct from acquired identity; AUTO needs established manufacturer/product mapping. CAN has no equivalent unique CL86-C mapping, so no AUTO inference is installed. |
| FieldCore-node | `5ad9292dc8481cf08be9a61f662495a6474d1da0`, clean | Identity/admission unchanged since prompt36: `Rs485MotorModule::configure/startMotor` and `Rs485RuntimeIngress::commandIdentityValid` retain request/generation/deadline checks. Newly inspected `src/cli/owners/Rs485Cli.cpp` rs485-discover branch and native CLI tests submit bounded configured-target probes through the existing owner. Adopt configured-target bounds; reject arbitrary address sweeps and sensor ACK/retry policy as CAN identification. |

Original EDS and rendered EN86 PDF15/16 were reopened, not just audit notes:

- EDS SHA256 `dd3c96e7282c012a522dddb5bbdbbf561c1f5c23716b31f12472f45829d4f52c`.
- EN86 SHA256 `401df6c49c54fe56fed7c606ca45718f352235746e2faaeb65be778eef6c4b9f`.
- `1000` is RO U32/default0x40192; `1009/100A` RO U16/default0x2608/1.
  `1018` has count plus only vendor/product/revision, defaults0x1000/1/0 and
  no serial subindex04. `2000/2001` are RO U16/default1/0.
- EN86's table lists1000/1009/100A;1018 is EDS evidence. Generic metadata cannot
  uniquely select CL86-C, firmware, interface variant, motor or feedback.
- Q01 retains specimen contradictions: selected U8 version replies/count5 and
  raw model0x0400/firmware0x0200 do not establish a general map. Width selection
  remains explicit through existing `IdentityWidths`; no automatic retry guesses.
- DEC-1 already accepts probe and nine identity uploads with unknown effects.
  The implementation preserves ACCEPTED_UNQUALIFIED, possible effects and
  uncertainty. No source, access permission, immutable original or ledger fact
  was changed, inferred as safe or newly physically measured.

Only named sibling seams were inspected; no unchanged sibling suite ran.

## Slot design and implementation

The [slot/timeout design](../commissioning.md#slot-and-timeout-design-prompt-37-before-implementation)
was written before active scan implementation. Current storage has two permanent
shared node/channel slots with no safe general release/rebind API. Active scans
therefore require explicit configured SDO routes; passive-only network slots
require no SDO session or guessed COB-ID. No scan creates, rebinds or frees a
slot. Count/capacity/bitrate/domain/epoch errors reject before work. Two absent
targets can leave both channels unresolved; result release does not recover
capacity. Arbitrary-node sweeps remain unavailable and are documented as such.

Installed core:

- `Discovery.h/.cpp`: exactly one Lichuan C software-family entry,
  `discoveryProfileCount`, `getDiscoveryProfile`, `getDiscoveryCapabilities`.
  Family enum and application's numeric Endpoint.profile are separate. No N5,
  EPOS4 or proprietary fixture is advertised as an installed profile.
- Existing `prepareProbe` stays the single1000 upload; new `checkProbe` uses
  the same checked read getter boundary and original provenance. Failed,
  wrong-kind, stale, invalidated or overlapping projections leave output unchanged.
  Failed/abort terminals remain inspectable in the owned ReadContext.

Application:

- `DiscoveryScan.h/.cpp` wraps the actual `ProbeApp`/`CanBusOwner`/`SdoClient`
  graph. One frozen plan selects PASSIVE, PROBE or IDENTITY, exact targets,
  physical interface/current bitrate, queue/response/age/service/per-turn budgets
  and an immutable deadline. It consumes at most one service terminal per turn
  and starts at most one finite typed read. No second receiver or service engine.
- Passive copies use current qualified node/PDO caches and bounded existing
  observer history after normal routing. Boot-up is limited to the scan window;
  current pre-scan heartbeat/PDO evidence is allowed with original times. History
  overflow is counted; normal consumers and independent transport-observer copies
  continue. No unknown/PDO-looking identifier becomes a binding or valid position.
- Retained findings distinguish silence, protocol rejection, generic identity,
  explicit expected-identity contradictions, policy-unsupported firmware,
  invalidation/cancel/deadline and suspected duplicate evidence. One valid reply
  cannot exclude identical duplicate nodes. No claim establishes motor/feedback.
- `ProbeApp::requestProbe/getProbe` reuse the ordinary read path; a service
  overload accepts explicit owner work budgets, including zero starts. Selection
  checks full channel IDs and node epoch via cached `CanBusOwner::matchesChannel`.
  No reads, configuration, NMT, enable, reset, retry or hidden drain during selection.
- Results are non-consuming and frozen after batch completion until release.
  Raw read/native abort/effect history survives invalidation; positive identity
  findings are withdrawn on epoch loss before publication. Negative policy
  classifications remain, independently of final `bindingCurrent` and quarantine.
- A malformed reply may make the existing SDO engine issue bounded client-abort
  cleanup. This is protocol termination, not an object download or retry. Tests
  distinguish it explicitly; discovery itself submits uploads only.

Independent review found and corrected stale route selection, passive deadline
promotion, same-turn read-success/boot-up promotion, pre-scan boot-up/stale
heartbeat reuse, final slot accounting and negative-classification retention.
Regressions isolate each case; no gate was weakened to make them pass.

## Verification and handoff

Focused Release GCC/C++11 strict build and seven CTest contracts passed:
discovery, discovery_scan, discovery_preview, probe_app, reads, can_bus_owner and
network_events. The two new suites report 404 and 1994 synthetic checks. Native
preview uses the ordinary owner and fake replies, with no SDK/controller/CLI.
The final integrated results are recorded below; exact-final CI status belongs
to the handoff and ignored receipt, rather than an older passing commit.

```text
cmake -S . -B .build/p37-native -G Ninja -DBUILD_TESTING=ON -DCMAKE_BUILD_TYPE=Release
cmake --build .build/p37-native --target discovery_test discovery_scan_test discovery_preview probe_app_test reads_test can_bus_owner_test network_events_test --parallel 4
ctest --test-dir .build/p37-native -R "^(discovery|discovery_scan|discovery_preview|probe_app|reads|can_bus_owner|network_events)$" --output-on-failure
python scripts/verify.py --mode full --compiler gcc --build-dir .build/p37-release-full
```

Integration caught three fixture omissions: the firmware recovery test selected
a stale node epoch, and the repository/package expected-export lists lacked the
new commissioning guide. The firmware fixture now selects and checks the actual
current route before asserting recovery inhibition. Both allowlists now include
the shipped guide. The failed runs remain in `.build/p37-full` and
`.build/p37-final-full`; focused reruns passed before the final full run.

The integrated `.build/p37-release-full/summary.json` records all 110 contracts,
registered inventory, version/object freshness and the isolated package gate
passing. Its final documentation step exposed an omitted Doxygen input for the
new commissioning link. `Doxyfile` now includes that page and `DiscoveryScan.h`;
the scan enums/capacity have explicit documentation too. The strict recheck
passed with 33 installed headers, 2902 extracted members and all 127 required
free functions. Commands were `doxygen Doxyfile` and the existing
`verify.check_documentation(ROOT)` extraction check; retained command/tool/result
logs are in `.build/p37-docs-final/summary.json`. The earlier full summary remains
failed rather than being rewritten. The final-commit CI repeats the complete
integrated path. No documentation warnings or input were suppressed.

The package gate independently passed exported C++11 source, C++17/no-RTTI and
relocated installed consumers, strict independent public headers, missing-header
negative checks and the copied native discovery example. Those consumers cannot
find repository-private headers, Python, SDKs, sibling checkouts or references.
Formatting checks, `git diff --check` and promptset provenance checks passed;
the latter still account for 140 original artifacts, 99 EDS indexes, 202 scalar
entries and 53 manual-only items. Original vendor bytes and generated objects
are unchanged.

The actual S2 SDK build passed with 510 compile entries and the shared discovery
source present. ESP-IDF 5.5.5 commit
`b774170ff46c393eeb5e495ea37936038d3f4f4f` and Xtensa
`esp-14.2.0_20260121` were unchanged. Retained commands/results:

```powershell
$env:IDF_PATH = 'C:/pio/packages/framework-espidf'
$env:IDF_TOOLS_PATH = (Resolve-Path ../MotorControl-RS/build/p25/idf-tools).Path
$env:ESP_ROM_ELF_DIR = 'C:/pio/packages/tool-esp-rom-elfs'
$env:Path = 'C:/pio/packages/toolchain-xtensa-esp-elf/bin;C:/pio/packages/tool-cmake/bin;C:/pio/packages/tool-ninja;' + $env:Path
python scripts/check_firmware.py --target s2 --idf-python ../MotorControl-RS/build/p25/idf-python/Scripts/python.exe --build-dir .build/p37-s2-final
$firmwareResult = Get-Content .build/p37-s2-final/summary.json -Raw | ConvertFrom-Json
$compileDb = Join-Path $firmwareResult.sdk_build_evidence.s2 'compile_commands.json'
python scripts/check_stack.py --require-toolchain --toolchain-dir C:/pio/packages/toolchain-xtensa-esp-elf/bin --build-dir .build/p37-stack --report .build/p37-stack/report.json --strict --target esp32s2 --compile-commands "esp32s2=$compileDb" --jobs 2
```

Strict S2 stack checks passed at both `-Os` and `-Og`, with no frame above 2048
bytes or known call chain above 6144 bytes. `.build/p37-stack/report.json` retains
compiler/source hashes, flags and explicit unmeasured SDK/indirect-call limits.
These are compile-time lower bounds, not runtime stack high-water qualification.
The SDK build reused sibling tool installations read-only; it changed no sibling
source and sent no device traffic. S3, Arduino and Linux sanitizer execution are
covered by the required final-commit CI jobs, not inferred from the S2 result.

New core header/source, native contracts/example, installed/exported consumers,
repository checks, firmware source inventory and package documentation exports
are registered together. Public Doxygen, commissioning/setup guide, README,
CHANGELOG, source map, coverage, backlog, roadmap and PROGRESS describe actual
scope and remaining limitations.

Delegation: `p37_sources` (gpt-6.1-sol/high) audited named sources and owned only
core inventory source, checkProbe implementation and its tests; `p37_scan_tests`
(gpt-6.1-sol/high) owned only scan tests/native preview; `p37_owner_review`
(gpt-6-astra/high) independently challenged ownership, timing and retention
read-only. The lead owned public contracts, application/owner changes, integration,
verification, docs and Git. No worker used hardware or mutated Git/siblings.

Maintainability: reused finite typed reads, exact existing channels/epochs,
normal dispatch and owner retention. Added one small inventory unit and one
bounded application scan, with no plugin framework, node registry, allocation,
clock, universal executor or generated catalogue dependency. No confirmed owned
software correction is deferred. Physical/Q01/Q18/Q20 evidence is unchanged.
Next numbered work is prompt38; this turn is assigned prompt37 only. Final
commit, push/fresh-fetch comparison and exact CI receipt are recorded in the
user handoff and `.build/p37-ci/final.json`; unrelated IDE files remain untouched.

## Bench integration

Run on hardware on 2026-11-09 ([2026-11-09 bench round](2026-11-09_bench_r2.md)): passive, probe and identity scans and host selection on both nodes, as described below; selection sent nothing. The text below was written before that run.


The S2 bench image with PONG `pdo=4` adds the `D<o|p|i>,<seconds>,<select>,<expect>` line
([hardware bench](../hardware_bench.md#discovery-communication-reads-and-heartbeat-commissioning)).
`examples/s2_bench/DiscoveryBench` runs this prompt's `DiscoveryScan` over both configured slots
through the firmware's own `ProbeApp`, owner and service store, then performs the requested host
selection with `ProbeApp::select`. The scan receives a zero owner budget so that the shared firmware
turn remains the only owner progression; the record copies each upload terminal before the scan
consumes it and counts admitted uploads itself. This exposes one integration fact, not a defect:
`DiscoveryScan` assumes it is the app's service caller, so its `servicesAdmitted` stays 0 when an
outer firmware turn admits the reads. Host tests (`discovery_bench`) exercise passive, probe and
identity scans, expectation match and contradiction, selection without traffic, a silent node left
unresolved with its uncertainty, and every admission refusal; no download, NMT or drain marker
reaches the fake wire.

A bench run would show, per specimen and firmware: which configured nodes answer an accepted
`1000:00` probe and the nine identity uploads under `MEASURED_U8`, whether both still match the
retained 2026-11-04 identity, whether passive observation finds heartbeat or boot-up evidence
(it can only when `1017` is nonzero or a boot-up falls in the window), and that host selection
sends nothing. It would not show model or firmware identification, motor or feedback facts, a
collision-free bus (one reply never excludes a duplicate node), behavior of any other node
number, or anything about arbitrary-node scanning, which stays unavailable.
