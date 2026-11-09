# Prompt 38: explicit communication commissioning

## Delivered scope and remaining gates

Implemented all 89 assigned reads, C-09's firmware handoff guard and the supported
conditional heartbeat workflow. Prompt 38 remains **Partial** for unestablished
node/SYNC/EMCY/SDO-route mutation procedures. These remain assigned here with
specific Q16 evidence, rather than moving to persistence or general remapping.
The original prompt explicitly gates unsupported firmware and leaves these facts
unresolved. No unsupported setter or successful placeholder was added.

The consumed artifacts are prompt 24's owner retirement/results and prompt 37's
exact host route selection. Same execution: promptset-v2 based on `7878f7c`,
branch `promptset/motorcontrol-can`, upstream `origin/promptset/motorcontrol-can`.
Entry `73727c0a1cdc5f4251f0ab8a90dd430efd4cf0c7` was 0 ahead/0 behind after fetch,
with an empty index. Three unrelated untracked `.vscode` JSON files were preserved.
No physical port, CAN traffic, flashing, DIP/topology change or motor activity ran.

## Refreshed source and applicability

| Source | Revision / state | Adopted or rejected seam |
| --- | --- | --- |
| MotorControl-RS | `cd27b12389f568198b72cecb6606123ea4c089bc`, clean | Newly read ESS `Communication.h/.cpp`, communication and application tests, `CommunicationApp.h`, `ProbeApp::invalidateAxis/invalidateSerialConfidence/selectTarget`. Adopt explicit preparation/confirmation, frozen intent and retained unknown write effects. Reject ESS register numbers, serial tuples and its save/restart rules as CAN evidence. CAN confirmation here requires ACK; failed intents stay terminal. |
| INA228 | `73f954838d3fac8cce747d54d0ad738940d4c625`, clean | `src/INA228.cpp::invalidateHardwareState`, `getSettings` and retained-dirty tests: unchanged since prompt 32. Desired state differs from verified hardware state; validity withdrawal preserves the first dirty cause. No sensor retry/reinitialization policy adopted. |
| FieldCore-node | `5ad9292dc8481cf08be9a61f662495a6474d1da0`, clean | `src/devices/rs485_motor/Rs485MotorModule.cpp::configure/invalidateConfiguration/onBusInvalidated`: binding seam unchanged since prompt 22. Adopt increasing replacement epochs and withdrawal of readiness/reference while retaining results. Read-only; no FieldCore integration claimed. |

Original EDS and rendered EN86 PDF9/10/15/16/18, ZH86 PDF9/10/18 and OP86 PDF7/8
were reopened. The misleading OP86 internal 57C header remains an applicability
limitation. SHA256 values remain:

- EDS: `dd3c96e7282c012a522dddb5bbdbbf561c1f5c23716b31f12472f45829d4f52c`.
- EN86: `401df6c49c54fe56fed7c606ca45718f352235746e2faaeb65be778eef6c4b9f`.
- ZH86: `3969c914aa291662e8c77941bcfbb0559769a96b75306266370890d03e824d13`.
- OP86: `4c87679d210c99f4ac4f6ce29e7f2c82b13221fdf81c47ee59a989f708f504b5`.

EDS locators: `1005` line225, `1014`252, `1017`261, `1200:00..02`320/329/338,
PDO communication347–586/788–1027, mapping587–787/1028–1228 and `2203`1301.
Independent accounting: 48 communication rows +40 mapping rows +1 node-setting
row =89; 32 U8, 17 U16 and 40 U32. EDS access is eight RO and 81 RW. The eight
compatibility entries retain their stricter reviewed RO policy despite EDS RW.
Inactive mapping slots and absent defaults are preserved, not synthesized.

Source-confirmed: DIP SW1–5 node selection, SW6–7 bitrate, SW8 termination;
`2203` U16 and documentary range 0..127; every PDO field width. No object reports
DIP state. Stored `2203` is not the active route; zero is not unicast. There is
no software bitrate/LSS workflow or documented 50-kbit/s DIP choice.

General protocol interpretations remain **CiA 301: general vendor claim, service
unconfirmed**. The heartbeat millisecond unit has primary CiA poster/pinned-stack
support, not a vendor-wide timing guarantee. The retained
[2026-11-02 checkpoint](2026-11-02_nmt_pdo_checkpoint.md) measured only node 1's
raw `1017` 0→1000 ACK/readback and about 1.0008-second intervals, then raw zero
after one power cycle. No new physical measurement was made. Restart and
persistence remain unknown per row, and Q17 inhibit/event semantics stay separate.

## Implementation and failure contracts

- `CommunicationReadContext` reuses the existing bounded read plan, matching and
  atomic getter. All 16 PDO records include every child; count is never a read
  bound. Existing six-field configuration coverage remains owned by prompt 19.
- The ledger and generator now describe 191 read shapes and 14 conditional
  setting rows. Only the new heartbeat setting contributes the explicit heartbeat
  workflow count. Whole-object support, hardware coverage, DEC-1 and raw source
  policy remain unchanged. No generated runtime descriptor grants writes.
- Independent review found that confirmed channels could bypass explicit upload
  review. New `SdoTransfer::requireUploadReview` enforces an exact one-use grant
  even there; all 89 new uploads and heartbeat confirmation set it. Grant identity
  and terminal matching include the flag; it is invalid on downloads. Ordinary
  legacy service policies remain separate. Effects stay UNQUALIFIED/Q20.
- `CommunicationContext` supports only reviewed CL86-C `1017` raw 0→1000:
  continuous reservation, exact fresh zero baseline, explicit review records,
  one acknowledged download and explicitly armed same-deadline U16 readback.
  Unknown exact NMT is permitted only for the reviewed isolated experiment with
  matching service evidence; known-current state must satisfy the reviewed
  Pre-operational prerequisite. It never fabricates Operational or starts NMT.
- The pre-handoff notice advances actual configuration once. The native owner
  test withdraws producer and affected PDO confidence, preserves uncertainty,
  then requires local retirement/configuration migration before readback. No
  readback reinstalls maps, reference or same-ID sample provenance.
- Failed intent cannot resume; original ACK/abort/effect evidence survives later
  failure. Wrong-route silence, lost ACK, reboot/rebind and readback mismatch
  produce no retry, guessed route or persistence action. Q18 still gates actual
  unresolved-channel reopening.
- C-09: FirmwareProbe already forwarded a hook, but null/unrelated checks could
  authorize downloads. It now refuses missing authorization. One explicit bench
  dispatcher checks exact pending sequence/work before concrete raw/PDO/settings/
  motion preflight and notices. AxisSession retains two additional work copies
  (224 native bytes before ABI padding); no second receiver or node table exists.
  The best-effort stop exception is preserved. This is owner-to-adapter handoff,
  not repeated application preflight after deferred SDK submission.

Independent review also found stale baseline reuse before heartbeat handoff and
missing retained-write epoch/time checks at publication. Both have isolated
regressions. Invalid grant-on-download authorization was rejected as well as
native admission. Test fixture corrections preserved real retirement and queued
NMT behavior rather than assuming immediate terminal settlement.

## Verification and delivery

Focused strict C++11/GCC suites pass: communication (5261 checks), heartbeat
communication (703), firmware_probe, bench_route, raw_evidence and pdo_experiment.
The heartbeat tests use actual fake owner/adapter storage, mandatory grants,
delayed TX retirement and queued Pre-operational→Operational/Stopped/boot events.
The read suite independently transcribes original EDS widths/access, exercises
95 uploads and checks reserved/inactive/raw extreme values and atomic failures.

```text
cmake -S . -B .build/p38-native -G Ninja -DBUILD_TESTING=ON -DCMAKE_BUILD_TYPE=Release
cmake --build .build/p38-native --target communication_test heartbeat_communication_test firmware_probe_test bench_route_test raw_evidence_test pdo_experiment_test --parallel 4
ctest --test-dir .build/p38-native -R "^(communication|heartbeat_communication|firmware_probe|bench_route|raw_evidence|pdo_experiment)$" --output-on-failure
python scripts/verify.py --mode full --compiler gcc --build-dir .build/p38-full
```

The full native build passed. Initial integration exposed duplicate variable
names in the added installed-consumer example, the old 13-setting fixture count
and a silent-capture fixture relying on missing firmware authorization. Corrected
the example names/count and gave only that fixture's exact synthetic pending
request one-shot authorization. Failed evidence remains in `.build/p38-full`
and `.build/p38-final-full`; no test or production gate was weakened.

Final integrated recheck passed all **112 contracts**, registration and strict
Doxygen 1.13.2: 34 installed headers, 2986 members and 134 required free functions.
Commands and tool/status logs are in `.build/p38-final-recheck/summary.json`.
It reuses the actual built tree, rebuilds the corrected fixture and runs every
contract except the separately executed package gate. The old failed summaries
were preserved. The final-commit CI repeats the full verifier from scratch.

`python test/package_test.py --compiler g++ --cmake cmake --work-dir .build/p38-package-final`
passed all 23 recorded commands: exported source, independent headers, C++11,
C++17/no-RTTI, relocated install, isolated native applications, missing-dependency
negative and IDF CMake boundary. Consumer builds need no Python, ledger, SDK,
vendor references, sibling checkout or network. The new consumer actually calls
the communication API rather than only including its header.

Actual ESP-IDF 5.5.5 S2 compile/link passed with 512 compile entries, including
the explicit dispatcher in main. Retained result: `.build/p38-s2/summary.json`.
The reviewed SDK/toolchain pins are unchanged. Commands used the already installed
tool environment, read-only:

```powershell
$env:IDF_PATH = 'C:/pio/packages/framework-espidf'
$env:IDF_TOOLS_PATH = (Resolve-Path ../MotorControl-RS/build/p25/idf-tools).Path
$env:ESP_ROM_ELF_DIR = 'C:/pio/packages/tool-esp-rom-elfs'
$env:Path = 'C:/pio/packages/toolchain-xtensa-esp-elf/bin;C:/pio/packages/tool-cmake/bin;C:/pio/packages/tool-ninja;' + $env:Path
python scripts/check_firmware.py --target s2 --idf-python ../MotorControl-RS/build/p25/idf-python/Scripts/python.exe --build-dir .build/p38-s2
$firmwareResult = Get-Content .build/p38-s2/summary.json -Raw | ConvertFrom-Json
$compileDb = Join-Path $firmwareResult.sdk_build_evidence.s2 'compile_commands.json'
python scripts/check_stack.py --require-toolchain --toolchain-dir C:/pio/packages/toolchain-xtensa-esp-elf/bin --build-dir .build/p38-stack --report .build/p38-stack/report.json --strict --target esp32s2 --compile-commands "esp32s2=$compileDb" --jobs 2
```

Strict stack checks passed at `-Os` and `-Og`, with no frame above 2048 bytes or
known chain above 6144. S2 communication getter frame is 1280 bytes; heartbeat
preparation is 1008. `AxisSession` is now 35592 bytes versus 35368 before C-09,
an exact 224-byte increase for its two owned work copies. Unknown/indirect/SDK
frames remain explicit lower-bound limitations; no runtime stack high-water,
service-gap or physical qualification is inferred. Arduino/S3/component and
Linux ASan/UBSan are the required final-commit CI jobs.

Generated freshness, scoped clang-format 14 dry-run, diff and promptset checks
passed. Staged/HEAD provenance checks preserve all 140 original artifacts,
99 EDS indexes, 202 explicit scalar entries and 53 manual-only items. Sibling
suites were not run to fill the report.

Exact final-commit CI and fresh upstream comparison belong to the
handoff and ignored `.build/p38-ci/final.json` receipt. Earlier passing commits
and the old physical checkpoint are not represented as this implementation.

Documentation: commissioning/owner/firmware/adapter guidance, public Doxygen,
README/CHANGELOG, source map, API map, coverage, backlog, roadmap and PROGRESS
were updated together. New files are registered in CMake, isolated package
consumers, the checker and its explicit test fixture. Generated files came from
the actual generator; originals and `.gitattributes` were preserved.

Delegation: `p38_sources` (gpt-6.1-sol/high) independently audited primary/sibling
sources, implemented read wrappers and the bounded heartbeat source/tests;
`p38_tests` (gpt-6.1-sol/high) owned independent read/admission tests and found the
confirmed-channel grant gap; `p38_route_review` (gpt-6-astra/high) owned C-09 glue
and independently reviewed heartbeat/route/deadline/getter behavior. Lead fixed
shared policy, reviewed their actual code, integrated ledger/build/package/docs
and owns all Git and final verification. No worker changed siblings or hardware.

Maintainability: one existing SDO engine/receiver and finite read storage are
reused. New modules are typed raw projection and one concrete write/confirmation
lifecycle, not a generic configuration executor. No heap, SDK, I/O, hidden clock,
retry, model inference or persistence enters installed code. Broader mutation
stays assigned to prompt 38 pending exact evidence; prompt 18 owns remapping and
39 owns save/reset. This turn executes only prompt 38.

## Bench integration

Run on hardware on 2026-11-09 ([2026-11-09 bench round](2026-11-09_bench_r2.md)): `K` read all 95 fields on both nodes. `H` failed on both nodes: the `1017:00` write was acknowledged and took effect (heartbeats `7F` at 1.0008 s, `1017=1000` read back by `K` afterwards), but the owner refused the configuration-epoch migration, so the record ended `failure=10 uncertain=1` with no readback. Cause and fix (backlog S14): the `N` intent's NMT transition stayed pending (no heartbeat at producer time 0, 5 s window), and `SdoClient::advanceConfiguration` refuses while it is pending; the migration came 1.54 s (node 1) and 1.50 s (node 2) before the windows ended. `H` had checked only ordinary service, which the probe after `N` gave. `SdoClient::configurationEligible` now reports the migration's node premises, and `H` checks them at admission, before the baseline and before the write. A host test over the real owner, adapter, `ProbeApp` and `DiscoveryBench` reproduces the hardware sequence; seven mutants are killed. Not yet rerun on hardware: the drives read raw 1000 until their next power cycle. The text below was written before that run.


The S2 bench image with PONG `pdo=4` adds `K<node>` and `H<node>,1000`
([hardware bench](../hardware_bench.md#discovery-communication-reads-and-heartbeat-commissioning)).
`K` reads all eighteen typed groups of one node, 95 uploads, each group through
`prepareCommunicationRead`, `nextCommunication`, `advanceCommunication` and `getCommunication`, every
unqualified upload behind its own exact one-use grant. `H` runs `CommunicationContext` for the reviewed
`1017:00` raw 0 to raw 1000 transition through the bench's download dispatcher: exact baseline,
withdrawal of the producer interval and the node's PDO routes plus the profile notice before the
download can leave, owner epoch migration after TX retirement, confirmation and publication. Only
raw 1000 exists; the reverse transition has no reviewed workflow, so restoration is the operator's
power cycle, verified by `K`.

Integration found one contract gap, recorded here for decision rather than worked around silently:
on an unconfirmed route with unknown exact NMT, `prepareCommunication` accepts the reviewed
isolated experiment, but `CanBusOwner::advanceConfiguration` (through `SdoClient::advanceConfiguration`)
refuses the migration unless the adapter reports `ordinarySdo`, which neither the granted baseline
nor the granted write establishes. A host reproduction (the bench record without its precondition,
over the real owner with an unconfirmed route) acknowledged the write and then ended in
`CONFIGURATION` with `migrated=0` and uncertainty: an applied write that cannot be confirmed. The
library's own owner test configures the channel as confirmed and so does not meet the case. The
bench therefore refuses `H` before anything is sent unless the adapter already reports ordinary
service (a fresh accepted `D` probe after any NMT intent provides it). Neither `Communication.h`
nor `advanceConfiguration`'s Doxygen states this precondition.

Host tests (`communication_bench`) cover the exact 95 upload frames in order, typed lines, a native
abort mid-sequence (earlier groups kept, nothing after it), a lost reply (owner timeout, possible
effects kept, route unresolved), a boot-up mid-read, a refused grant, and for `H` the exact three
frames, the baseline mismatch refused before any write with dependent facts untouched, known
ineligible NMT, a lost write acknowledgement, a readback mismatch, Operational or Pre-operational
first heartbeats at the acknowledgement, the service precondition and a dispatcher that does not
match (download closed unsent).

A bench run would show, per node: the stored raw values of all 89 rows plus the six network fields
(read effects still unknown), whether `1017:00` accepts the one write and reads back raw 1000, the
exact frames, and with a `P` watch the resulting heartbeat period. It would not show active producer
timing from the readback alone, persistence or restart behavior (still unknown per row), any
host-loss reaction of the drive, a live route, map or sample from the stored words, the meaning of
inhibit or event units (Q17), or any software restoration.
