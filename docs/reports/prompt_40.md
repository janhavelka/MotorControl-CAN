# Prompt 40: shared CLI parser and local routes

Date: 2026-11-11. Execution branch `promptset/motorcontrol-can`, upstream
`origin/promptset/motorcontrol-can`, entry `87a6e945381c6ab904e3c97219c05df0652dc1e4`.
Entry index and tracked tree were clean; fresh fetch showed 0 ahead/0 behind.
Three unrelated untracked .vscode JSON files were preserved. Promptset-v2 base
`7878f7c` is unchanged. No hardware, port, flash or physical activity occurred.

Status: **Complete owned local/parser scope**. Device routes remain explicitly
owned by prompts 41/42; no hardware qualification is added.

## Scope and decisions

The usable prerequisite artifacts are prompt 26's ProbeApp/ProbePlatform and
native fake, and prompt 28's shared firmware entry/source inventory and platform
callbacks. Their existing device qualification and S2 Arduino package gates do
not prevent the local parser. This stage changes no vendor constant, permission,
scale, timing guarantee or object ledger. All new device routes remain absent.

One `consoleCommands` inventory owns grammar, actual API, capability, effect,
completion, help and gated owner/reason. Seventeen working families use cached
inspection, pure arithmetic or explicit local mutation. Gated families include
typed reads/settings/discovery/network/persistence and motion/action/simple-move
intent, with D-17's exact absent bus-release command reason. `help` contains only
working routes; `caps` exposes both inventory states. The durable grammar/schema
and full limits are in [the CLI contract](../cli_contract.md).

The parser is shared under examples/common, never installed core. It validates
whole ASCII lines atomically, numeric signs/ranges and exact public number
parsing. CAN chooses 255 text bytes and 24 command tokens plus correlation,
compared with RS's 127 bytes and 22 total tokens: explicit CAN exact values and
longer acquisition/coordinate terms fit without truncation. Output fits the
existing 512-byte platform ring: long help/capability/axis snapshots stream
whole records instead of enlarging the ring or truncating an object.

The CAN schema is `motorcontrolcan.console/1`, with protocol-3 correlation and
explicit product/profile identity. Wide numbers and exact fraction components
are decimal strings. This is a deliberate incompatibility with RS's bare JSON
wide integers, not a claim of compatibility from the protocol number. Original
request IDs and operation IDs are distinct; asynchronous mode is frozen, failed
and uncertain terminals survive unrelated commands and lookup until explicit
release after callback delivery. Real owner recovery and externally admitted
read contexts supply outcomes; no production success stub exists.

ProbeAppLocal holds an isolated offline workspace with no default geometry,
origin or position witness. Configure validates candidate copies and retires
previous reference/origin/soft limits. Origin and relative/wrapped previews
require caller-supplied fresh evidence. All arithmetic uses public Axis/Units
functions. Live selection remains unchanged. Host wiring labels declare use
only; interface/bitrate facts come from the application.

Default Arduino/IDF startup now services this same console after owner work,
including while CAN is inactive. Explicit smoke hooks preserve exclusive input
ownership; the separate S2 bench parser is not this production console. Native
`--console-fake FILE` prevalidates a bounded file before constructing the app,
requires explicit line termination (EOF cannot admit a truncated prefix), then
uses real shared parsing/service with bounded capture. File/stdout I/O is
outside fake owner progression. Core packages gain no SDK, console, Python or
reference dependency. Isolated optional example consumption includes the console.

## Source audit

All named sibling checkouts were clean; no sibling writes or tests were run.

| Source | HEAD and inspected behavior |
| --- | --- |
| MotorControl-RS | `cd27b12389f568198b72cecb6606123ea4c089bc`. Read examples/probe_cli/ProbeConsole.h and ProbeConsole.cpp::feed/dispatch/axisRequest/emit/serviceOutput; ProbeApp.cpp::lookup/axisCommand/drainOutput and scripts/bench_probe.py::Console._dispatch/identify plus actual protocol tests. Adopt canonical names, whole-line rejection, operation-frozen format, separate command/operation IDs and no-replay/result retention. Reject ESS device grammar/registers, defaults, serial addressing and bare wide JSON numbers. |
| ADS1115 | `d613a13cd3d43471905c507bfb4094bba1c1da94`. include/ADS1115/ADS1115.h::getSettings and src/ADS1115.cpp::getSettings/_updateHealth: adopt cache-only diagnostic snapshots and retained errors; reject sensor offline thresholds, aliases and replay policy. |
| INA228 | `73f954838d3fac8cce747d54d0ad738940d4c625`. getSettings/getDiagAlertSnapshot and examples/01_basic_bringup_cli/main.cpp: adopt cached-versus-explicit potentially consuming read distinction; reject sensor reinitialization and zero-as-unknown time. |
| SHT3x-main | `39d5b076960781cc5539e3ec3a37a96ffda743f7`. tools/sht3x_cli_contract.py::CommandSpec/COMMAND_SPECS/validate_contract, tools/check_cli_contract.py::compare_help/check_execution_metadata, examples/common/Sht3xCli.cpp::processCommandString: adopt canonical inventory and effect/completion classification; reject sensor aliases, status I/O and reset policy. |
| FieldCore-node | `5ad9292dc8481cf08be9a61f662495a6474d1da0`: unchanged since prompt 39; ownership seam not reread. Rs485MotorModule.cpp::applyRuntimeConfig rejects runtime address/baud changes and Rs485Task retires before reconfiguration. No CAN integration or framework dependency inferred. |

No new vendor/device claim was introduced, inferred or measured. Existing
unresolved effects and evidence remain attached to gated routes, not converted
into console permission. Current RS removed aliases and never-implemented
`motor ...` grammar are independently rejected.

## Review, tests and verification

Independent review found two concrete pressure bugs during implementation:
input loss after an earlier buffered newline could execute a damaged numeric
suffix, and result release could precede terminal callback delivery. The fixes
retire the entire pre-loss input prefix and wait for a post-loss terminator,
and retain the output-written delivery boundary. Both exact regressions pass.
A separate ProbePlatform input-loss latch keeps resynchronization effective even
after the saturating diagnostic byte counter can no longer advance. Repeated-loss
latch/whole-line regressions cover its acknowledgement boundary.
The review also found a duplicate JSON envelope key in axis geometry and an
orphaned console slot after external read release. The data key is now
command_scale, duplicate keys/nonfinite JSON are rejected by transcript tests,
and explicit console release never releases a replacement app read. Inspectable
frozen axis fields, complete gated families and the explicit positional
statistics schema are integrated. Inactive approximate/float fraction fields
remain null rather than presenting default zero-error evidence.

A Release test initially used assert, which NDEBUG removed. It was replaced by
always-on CHECK assertions, without changing compiler flags. An initial full
verifier stopped at exact test registration because the new Python transcript
requires a built executable argument. The checker now verifies both real producer
paths, with negative wrong/missing-binary tests; no skip or relaxed gate was added.
Failed logs remain under .build/p40-full. The later release-check checkpoint
caught an older native executable accepting EOF as a terminator after the new
truncation regression was added; the current loader rejects it before application
construction. A fresh full build below verifies the integrated final source.

An integrated full checkpoint passed 120/120 registered contracts, isolated
package consumers and strict Doxygen 1.13.2 (35 installed headers, 145 required
free functions). The final clean full run after all corrections passed all 120 contracts, package
consumers and strict Doxygen; its retained summary is
`.build/p40-complete/summary.json`. Focused direct output reports 481,872 checks;
the Python test parses 82 actual JSONL records plus the separate native entry,
rejecting duplicate keys, nonfinite JSON, lost wide integer types and malformed
batch execution.

Local SDK checkpoints built/linked the special S2 image and the default shared
IDF S2 application with ESP-IDF 5.5.5 / pinned b774170ff46c393eeb5e495ea37936038d3f4f4f
and Xtensa 14.2.0. Logs are `.build/p40-s2/summary.json` and
`.build/p40-idf/summary.json`. The compile inventory includes both new sources.
Strict S2 -Os/-Og frame/known-chain checks are repeated on the final source in
`.build/p40-stack-release/report.json`, with no exemptions. Graphs remain lower
bounds with explicit indirect/SDK/libc omissions; no runtime high-water or
physical service-gap measurement is claimed. Final-commit CI supplies Windows,
GCC/Clang sanitizer and Arduino/IDF compiler evidence separately.

Native cached `memory` measured app 2392, owner 14432, console 3752 and platform
640 bytes on this GCC 64-bit host. Target sizes differ; the initial S2 stack
report measured ProbeApp 2352 bytes. The console adds no heap requirement and
keeps four complete output records/two retained correlations. No runtime heap,
stack high-water or physical qualification value is invented.

Reproducible commands:

```text
python scripts/verify.py --mode quick --compiler gcc --build-dir .build/p40-quick
python scripts/verify.py --mode full --compiler gcc --build-dir .build/p40-complete
ctest --test-dir .build/p40-native -R "^(probe_console|console_protocol|probe_app_local)$" --output-on-failure
python scripts/check_firmware.py --target idf-s2 --idf-python ../MotorControl-RS/build/p25/idf-python/Scripts/python.exe --build-dir .build/p40-idf
python scripts/check_firmware.py --target s2 --idf-python ../MotorControl-RS/build/p25/idf-python/Scripts/python.exe --build-dir .build/p40-s2
python scripts/check_promptset.py --check-git-ref INDEX
python scripts/check_promptset.py --check-git-ref HEAD
git diff --check
```

The SDK commands require the existing configured IDF_PATH/IDF_TOOLS_PATH,
ESP_ROM_ELF_DIR and pinned toolchain/CMake/Ninja PATH, as in prompt 38's report.
No toolchain unavailable on Windows is represented as locally passing.

## Maintainability and delegation

Reused ProbeApp, owner, public Axis/Units, existing read/recovery state and the
512-byte platform ring. New ProbeAppLocal isolates host arithmetic/facts;
ProbeConsole owns only grammar, inventory, bounded formatting and correlation.
No second receiver, node table, settings/motion executor, discovery inventory or
runtime plugin system was introduced. No helper copies native object/controlword
logic. The two operation slots and two ordinary/two control output records own
all retained bytes. Formatting uses bounded candidates; overflowing records are
never emitted as complete JSON. Output pressure cannot gate owner service.

Delegation used available gpt-6.1-sol/high for source audit, disjoint local app and
native entry work, and independent parser/protocol tests. gpt-6-astra/high reviewed
correlation/overflow/backpressure and offline/live separation read-only. Lead
owns grammar/schema/inventory, registration, integration, documentation and Git.
Workers performed no Git mutations, hardware or sibling edits. Their results
were inspected against actual changes and rerun in the integrated tree.

Affected user docs: README, CHANGELOG, CLI contract, API mapping, source map,
coverage, native/firmware guidance, verifier guide, backlog/roadmap and PROGRESS.
Public example headers now participate in the strict Doxygen inputs. Original
sources, attributes, catalogue and generated version remain unchanged.

## Remaining work and synchronization

Prompt 41 owns typed device/selection/network/diagnostic routes and inactive-host
recovery after later explicit start/stop; prompt 42 owns actions/motion intent,
reserved stop routing and full platform parity. D-17 release, Q20 reads, unresolved
model/firmware settings and all physical qualification remain explicit gates.
No owned local route is replaced by an unsupported placeholder.

All local owned gates passed. The lead commits this checked unit on the recorded
execution branch, stages only the listed task files and preserves the three
unrelated IDE files. Required automatic CI is verified after push on the exact
commit containing this report, not on the prior 87a6e94 run. The final handoff
records its SHA, workflow URL/conclusion and fresh ahead/behind state; the retained
machine-readable receipt is `.build/p40-ci/final.json`. No remote pass is inferred
from adding source-list entries or from an older successful run.
