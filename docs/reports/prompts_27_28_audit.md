# Independent audit of prompts 27 and 28

## Scope and entry state

Re-read both original prompts in full and inspected the implementation and diff,
not earlier completion summaries. Entry was
`00a251334637d13a3d7bb6787f7af1536336f381` on
`promptset/motorcontrol-can`, upstream `origin/promptset/motorcontrol-can`.
The tree/index were clean; fresh fetch showed 0 ahead / 0 behind. Promptset
version 2/base `7878f7c` is unchanged. No branch/worktree or remote was created.

The audit owns corrections within 27/28. Both retain **Partial only on inherited
24/26/19 Q20 read-effect evidence**. No probe permission, firmware identity,
controller qualification or physical drive fact was invented. No ports, flash,
live CAN, motor activity, sibling edits or sibling test suites were used.

## Independent reviews and current source delta

Three fresh parallel reviews used the actual source/diff: `gpt-6-astra/high`
challenged SDK/ISR/retirement uncertainty; `gpt-6.1-sol/high` inspected firmware
ownership/platform behavior; another `gpt-6.1-sol/high` inspected build/tests and
refreshed siblings. Review began read-only; disjoint test/fake files were assigned
after findings. The lead verified reproductions, changed production/shared files,
integrated all checks and owns Git. Worker success was not integrated verification.

| Source | Current revision / state | Inspected seam and result |
| --- | --- | --- |
| MotorControl-RS | `f608587fd477157257b06ca4cc4751dc61104cf1`, initially clean; final unrelated test edits below | `examples/common/Esp32S3Uart.cpp::validPin`, `examples/probe_cli/{ArduinoPlatform,Esp32UsbConsole,ProbeApp}.cpp`, IDF startup/platform/component, PlatformIO/verifier/workflow and native consumers. Upper GPIO bounds precede SDK shifts; one pinned owner and retained failed cleanup are reusable. UART47/48/DE21, COM13 and PSRAM/board assumptions do not transfer. |
| ADS1115 | `235af3e8c722661a89a44766a4ccd903a654f6fe`, clean | `examples/02_owner_safe_poll/main.cpp`, IDF transport/setup caller, `test/test_basic.cpp` and CI. Supplied-time/budget/token ownership remains applicable; sensor recovery policy does not become CAN retry. |
| INA228 | `b28c5ea69bcb4d26b351c52c24d3c6fae9fd282b`, clean | `examples/esp_idf/basic/main/Ina228IdfI2cTransport.cpp`, setup caller, basic tests and CI. Failed teardown retains borrowed resources; no sensor initialization policy imported. |
| FieldCore-node | `5ad9292dc8481cf08be9a61f662495a6474d1da0`, clean | `docs/guidelines/runtime_connections.md`, `src/rs485/ArduinoRs485Backend.cpp` and dormant motor binding/effect/cancel sections. Read-only ownership/generation reference; no new receiver or framework dependency. |

All revisions are unchanged from prompt 28. The final refresh observed concurrent
RS-only edits in `test/home_app_test.cpp`, `test/home_console_parity_test.py` and
`test/position_interrupt_app_test.cpp`. Their diff concerns homing/position
application tests, outside the inspected adapter/startup implementation seams;
those implementation files remain unchanged. These dirty tests were not used as
new CAN behavior evidence, modified or committed here. Other siblings stay clean.
Installed SDK remains ESP-IDF 5.5.5,
commit `b774170ff46c393eeb5e495ea37936038d3f4f4f`, initially ESP32-S3.
Resolved `C:/p55s` packages still match pioarduino 55.03.311, Arduino 3.3.11 and
IDF libraries `5.5.5+sha.b774170ff46`. No dependency version changed.

## Confirmed findings and corrections

### 1. Captured boot-up could precede SDK admission after the final guard

`IdfTwaiDriver::submit` released its mux after the RX/fault check and metadata
publication, before `twai_node_transmit`. An actual boot-up RX callback injected
at that exit produced zero prior SDK transmissions, then submission proceeded:

```text
Injected boot-up RX callback at driver guard exit; SDK transmissions=0
After submit: accepted=1, transmissions=1, unreadRX=1
```

Scratch reproduction is retained under `.build/audit2728-transport/`; the new
`guardedHandoffPrecedesNewRxCallbacks` regression independently failed before the
fix. The final guard, SDK handoff and failed-submit pending reset now share one
critical section. Previously copied ingress still prevents any SDK call.

The [reviewed SDK evidence](../adapters/twai_sdk.md#final-handoff-and-gpio-validation)
checks the actual nonblocking path: one outstanding frame, same pinned task/core,
nested event-group lock, atomic idle mount and HAL submission. SDK callbacks run
only from ISR; previous `hw_busy` clears after completion callback but before the
owner task resumes. Physical arrivals while masked remain unknown. This does not
manufacture wire freshness, remote acceptance or motion completion.

The host fake now models nested masking and defers immediate completion until
outer exit. It injects the real boot-up callback at the former race boundary;
new handoff ordering and existing before-guard rejection both pass. No second
receive path or node storage was added.

### 2. Positive out-of-range GPIO values reached undefined SDK shifts

The exact SDK GPIO macros check nonnegative pins and then shift `1ULL` by the
pin. Without an upper bound, pin 64 reached undefined behavior; the scratch host
reproduction admitted it and made four SDK initialization calls. This was a host
manifestation of undefined behavior, not evidence of physical pin selection.

Both pins are now bounded below `GPIO_NUM_MAX` before those macros. Regression
vectors include negative values, absent S3 pins 22-25, 49, 64, 128 and `INT_MAX`,
requiring no SDK call. Valid boundary pins include GPIO46. The fake now mirrors
the actual S3 mask instead of incorrectly accepting 22-25 and rejecting 46.
GPIO validity does not establish board wiring or transceiver suitability.

Six additional original SDK files were fetched at the exact pinned commit,
byte-compared with the installed SDK and added to the existing provenance
manifest: GPIO declarations/enumeration and FreeRTOS event-group/task/critical
implementation. The TWAI manifest now verifies 24 files; the separate platform
manifest verifies six. Original vendor/retained snapshots and attributes are
unchanged; SDK licensing remains separate.

### 3. The prompt-27 SDK gate lost Linux virtualenv identity

`check_twai_sdk.py` still resolved the selected venv Python symlink, unlike the
firmware gate. A CLI reproduction showed base Python and a base
`IDF_PYTHON_ENV_PATH`. It also accepted `0`, `n`, `N`, `NO`, `No`, `nO` as dependency
check settings, even though pinned `tools/idf_tools.py:3474` disables checks for
lowercased `0/n/no`.

The TWAI checker now reuses `check_firmware.sdk_environment` with its TWAI-only
manifest scope. The helper retains lexical Python identity, canonicalizes the
SDK root, rejects every disabled spelling and records original source counts.
Duplicated setup/version code was removed. CLI regressions exercise the actual
helper through `check_twai_sdk.main`, with only tool invocation mocked. Both
original failures were reproduced before correction; altered SDK bytes also fail.

### 4. Failed IDF builds discarded SDK-local diagnostic evidence

The outer Runner preserved output, but build/link failure skipped `retain_idf`;
short temporary cleanup deleted resolved `sdkconfig`, SDK logs and configure
diagnostics. Retention now runs on success and failure, tolerates absent images
and records a separate evidence location. Failed builds additionally preserve
available CMake cache/configure diagnostics. SDK objects/caches are not core
package dependencies.

An evidence-copy error cannot replace the original build exception; it is
recorded separately. If compilation otherwise succeeded, a copy failure still
fails the gate. Regressions verify partial evidence after temporary deletion,
missing BIN output, original failure identity and successful-build retention
failure. Existing exact compiler-input validation remains active.

### 5. Adapter clock failure used a stale invalidation timestamp

After servicing at 101 us, a public owner call advanced its accepted time to
200 us. Adapter service at 99 us then inhibited transport but reported RECOVERING
at the old adapter time 101; the owner rejected it. The reproduced state remained
AVAILABLE, node generation unchanged and the active download nonterminal, despite
transport inhibition. Scratch evidence is
`.build/audit2728-transport/clock_repro.cpp`.

The fault path now uses `owner.lastTimeUs()`, the existing authority already used
by `FirmwareProbe`. The active-write regression requires owner inhibition,
dependent generation invalidation and a retained uncertain terminal at 200 us.
The SDK frame remains active until positive retirement; no stop, replay or remote
settlement is inferred. No new clock or state table was introduced.

## Requirements and simplicity review

| Original scope | Actual evidence / disposition |
| --- | --- |
| 27 explicit configuration, frame formats and SDK limits | Existing typed configuration and source-backed fake frame vectors preserved; upper GPIO validation corrected; FD stays explicitly unsupported. |
| 27 enqueue/TX uncertainty, stop/recovery and bounded RX | Actual callback/retirement sources, fake failures and owner integration rechecked; no replay, re-enable or invented remote settlement. Final handoff race corrected. |
| 27 compile, SDK evidence and capacities | Actual pinned static-archive gate rerun; source hashes extended for the correction; existing capacities and ISR timing limitations unchanged. |
| 28 shared Arduino/IDF startup and explicit board fixture | Both thin entries still call the same application/owner/task inventory. Default fixture CAN-disabled; no constructor/selection/startup device request. |
| 28 bounded I/O, time and internal storage | Actual USB/time/task implementations and host failure/boundary tests inspected; no firmware runtime defect found. |
| 28 pinned builds and separate component | Real Arduino/standalone-IDF/isolated-component gates and compiler-input proof rerun. Core package remains SDK-free. |
| 28 native examples and omission of SocketCAN | Existing offline/fake examples remain registered and buildable; deliberate SocketCAN omission remains documented. |

No universal HAL, alternate stack, receiver, registry, command parser or motion
API was introduced. SDK setup is shared by two real callers; transport mutation
remains in one existing implementation. New fakes model only the relevant
interleaving and SDK mask, rather than a general scheduler. No tests or gates
were weakened. Coverage/source/physical status stays separate.

Affected documentation: adapter README and SDK table, firmware and verification
guides, CHANGELOG, stage reports, coverage and PROGRESS. Backlog/roadmap stage
owners and remaining Q20/physical work did not change, so their assignments were
not rewritten. No installed header or generated object/version output changed.

## Integrated verification

Focused pre/post-fix reproductions and independent test reviews are recorded
above. Current focused tests pass: `twai_driver` 1331 checks, `twai_adapter` 518 checks including the clock-fault regression,
`firmware_probe` 419; platform/application suites also passed independently.
`python test/firmware_build_test.py` passes all 26 tests.

Commands used with the unchanged pinned local environment:

```powershell
$env:IDF_PATH = 'C:/pio/packages/framework-espidf'
$env:IDF_TOOLS_PATH = (Resolve-Path ../MotorControl-RS/build/p25/idf-tools).Path
$env:ESP_ROM_ELF_DIR = 'C:/pio/packages/tool-esp-rom-elfs'
$env:Path = 'C:/pio/packages/toolchain-xtensa-esp-elf/bin;C:/pio/packages/tool-cmake/bin;C:/pio/packages/tool-ninja;' + $env:Path
python scripts/check_twai_sdk.py --idf-python ../MotorControl-RS/build/p25/idf-python/Scripts/python.exe --build-dir .build/a27-sdk-final
$env:PLATFORMIO_CORE_DIR = 'C:/p55s'
python scripts/check_firmware.py --target all --pio C:/p55s/penv/Scripts/pio.exe --idf-python ../MotorControl-RS/build/p25/idf-python/Scripts/python.exe --build-dir .build/audit2728-final-firmware
```

Both real SDK commands passed. The latter built Arduino ELF/BIN, standalone-IDF
ELF/BIN and a separate isolated portable component ELF/BIN, checking actual shared
inputs. Retained summaries/logs are `.build/a27-sdk-final/` and
`.build/audit2728-final-firmware/`. No command flashed or ran firmware.

Native/tool versions: host GCC 15.1, Python 3.12.10, CMake/CTest 4.0.1,
Ninja 1.12.1, Doxygen 1.13.2; SDK Xtensa GCC 14.2.0
`esp-14.2.0_20260121`, CMake 4.0.3, Ninja 1.13.1. Local sanitizer runtime/Clang
remain unavailable; native Linux GCC/Clang sanitizer results require remote CI.
Final integrated native verification passed:

```text
python scripts/verify.py --mode full --compiler gcc --build-dir .build/audit2728-final-full
python test/firmware_build_test.py
python scripts/check_promptset.py --check-git-ref HEAD
doxygen Doxyfile
git diff --check
```

Full verification passed 50 registered tests plus the separately retained package
contract. The package gate covers source/export/relocated-install consumers,
independent public headers, C++11/C++17/no-RTTI and SDK-free isolated native
applications. The preceding quick run passed 49 registered non-full contracts;
final full verification covers that set after the last clock correction. Version
and object freshness, strict Doxygen, repository/test registration and original
137-artifact HEAD provenance passed. Final firmware compile databases contain
87 Arduino entries, 511 standalone-IDF entries and 488 isolated-component entries;
all required shared/core inputs were checked. No local required build is unavailable.

Native summary/logs are under `.build/audit2728-final-full/`; fresh focused
review/reproduction output is under `.build/audit2728-firmware/` and
`.build/audit2728-transport/`. These results are from the integrated dirty files
listed by the verifier before commit, rather than an unqualified entry HEAD hash.
After explicit staging, INDEX/HEAD provenance checks and synchronization, the
five automatic remote jobs are checked on the exact final audit commit. Their
receipt is retained in `.build/audit2728-ci/final.json` and the final user handoff.

## Handoff

Original source/model/read-effect questions and physical qualification remain
unchanged. No confirmed owned software correction is deferred. The lead commits
and synchronizes the integrated audit on the same execution branch, checks
staged/HEAD provenance, and verifies all five automatic CI jobs on the exact final
commit. Final commit/sync/CI status is recorded in the user handoff and receipt.
