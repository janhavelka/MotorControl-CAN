# Prompt 28: Arduino and ESP-IDF integration

## Scope, status and execution

Owned firmware/platform software is implemented. Stage status is **Partial only
on inherited 27/24/26/19 Q20 non-consuming-probe evidence**. The already verified
application, owner and TWAI adapter are consumed independently. No source claim,
drive permission or remote-session qualification changed. No flashing, monitor,
serial/CAN port opening or physical activity was performed.

Execution continues on `promptset/motorcontrol-can`, upstream
`origin/promptset/motorcontrol-can`, entry commit
`00718cc46d6295859c5cb37f91db1de628310c7f`. Entry tree/index were clean; fresh fetch
showed 0 ahead / 0 behind. Promptset version 2/base `7878f7c` remains unchanged.
The established remote is `https://github.com/janhavelka/MotorControl-CAN.git`.

## Current source audit

All relevant checkouts remained clean and unchanged from prompt 27:

| Source | Revision | Relevant implementation/caller evidence and intentional differences |
| --- | --- | --- |
| MotorControl-RS | `f608587fd477157257b06ca4cc4751dc61104cf1` | Arduino/IDF startup, ProbeApp/ProbePlatform, Esp32UsbConsole, board pins, PlatformIO pins and component consumers show one pinned owner task and bounded USB output. UART/DE pins, RS port defaults, N16R8 and PSRAM assumptions do not transfer. |
| ADS1115 | `235af3e8c722661a89a44766a4ccd903a654f6fe` | Owner/IDF example setup, supplied-time progression and native consumer/CI boundaries retain application-owned SDK/I/O. No sensor retry/timing policy imported. |
| INA228 | `b28c5ea69bcb4d26b351c52c24d3c6fae9fd282b` | Owner/IDF example setup and failed teardown retention support stationary context lifetime; sensor configuration/state semantics remain separate. |
| FieldCore-node | `5ad9292dc8481cf08be9a61f662495a6474d1da0` | Bus/backend ownership, cancel/fault and dormant motor integration read-only. No FieldCore type or alternative receiver imported. |

No sibling files or suites were changed/run. `gpt-6.1-sol/high` owned SDK platform
fakes and Arduino build checks, `gpt-6.1-sol/medium` owned the isolated component
consumer and firmware/build-checker tests, and `gpt-6-astra/high` independently
reviewed startup, time, ownership and failure behavior. The lead owns shared
contracts, CMake/verifier/workflow, integration and Git on the same branch.

SDK facts are pinned to ESP-IDF 5.5.5
`b774170ff46c393eeb5e495ea37936038d3f4f4f`, ESP32-S3. Arduino uses pioarduino
55.03.311 / Arduino 3.3.11 / IDF libraries `5.5.5+sha.b774170ff46` under `C:/p55s`.
The [TWAI source hashes](../adapters/twai_sources.json) and
[additional original USB/time/task hashes](../adapters/esp_platform_sources.json)
match installed SDK bytes; all six additional files were independently fetched
from the exact original commit and byte-compared. Original vendor references,
CANopenNode snapshots and attributes are unchanged. SDK material remains separately
licensed; no SDK source copy was added to the core distribution.

Source-confirmed SDK behavior: single-byte zero-tick reads/writes do not wait for
USB host progress; accepted writes copy their byte; USB ISR RX-ring overflow has
no public loss counter; static task stack size uses bytes; ESP timer is signed
64-bit microseconds. No 32-bit wrap extender is needed. Elapsed service/ISR timing,
stack high-water marks and physical behavior remain unmeasured.

## Implemented result and review corrections

- [Shared firmware assembly](../../examples/common/FirmwareProbe.h) composes the
  existing application/owner/TWAI around the same borrowed service store. Both
  thin entries call the same static owner task and finite
  [source inventory](../../examples/common/firmware_sources.json). No parser,
  second receiver/node table or SDK-specific motor execution path exists.
- [Board configuration](../../examples/common/FirmwareBoard.h) requires explicit
  pins/bitrate/RX capacity for deployable builds. The compile fixture leaves CAN
  disabled. GPIO19/20 remain USB pins; the SDK checks remaining pin capabilities.
  No drive binding, node, model, firmware, session or drive request is created.
- [SDK platform](../../examples/common/EspProbePlatform.h) supplies checked time,
  one-byte zero-tick USB operations, retained errors/refusal counts and one-tick
  idle after owner and transport service. Internal stationary task/SDK storage,
  bounded queues/budgets and loss limits are documented in the
  [firmware guide](../firmware_examples.md). Console pressure cannot take RX.
- A failed/backward clock inhibits work, invalidates owner evidence and attempts
  retirement once. Independent review reproduced invalidation at an obsolete
  firmware timestamp after a public owner call advanced time. The fix uses the
  owner's authoritative cached `lastTimeUs()`; a regression advances to 200 us
  and proves invalidation. Failed retirement preserves uncertainty/storage.
- SDK platform initialization failure now yields instead of entering a busy loop.
  Arduino's `DISABLED` macro required shared headers before Arduino.h; the actual
  Arduino build exposed and verified that include-order correction.
- [Build checker](../../scripts/check_firmware.py) reuses retained verifier logs,
  structured subprocesses, bounded timeouts and nonzero failures. It validates
  original SDK bytes and actual compiler inputs, requiring ELF/BIN outputs.
  Arduino, standalone IDF and isolated core-component consumption are independent
  real gates. The component stage contains only root CMake/metadata/include/src;
  it cannot use checkout-relative owner/stack/reference sources.
- A final combined run reproduced a Windows SDK dependency-file path limit
  under the deep timestamped component build directory. SDK compilation now uses
  short external temporary roots; linked images/maps/compiler databases and SDK
  logs are copied into the retained run directory. No compiler limit or check is
  disabled, and the original failed invocation remains retained.
- Native quick/full remain SDK-free; explicit firmware gates are registered in
  [CI](../../.github/workflows/verify.yml) with exact SDK/framework pins and no
  changed automatic triggers. Firmware checks never flash or monitor.
- SocketCAN is deliberately omitted; the existing owner and fake fixtures cover
  the current boundary without another adapter. Console/schema parity stays 42.

README, CHANGELOG, contributor/build/verification guides, documentation index,
source map, architecture, coverage/backlog/roadmap and PROGRESS describe this
working boundary. Doxygen covers new application/platform interfaces. Core
package allowlists exclude firmware, SDKs, source evidence, tests and fakes.

## Verification

Local environment: Windows, Python 3.12.10, CMake/CTest 4.0.1, Ninja 1.12.1,
GCC 15.1.0 and Doxygen 1.13.2. SDK builds use constrained IDF Python 3.12.10,
Xtensa GCC 14.2.0 `esp-14.2.0_20260121`, SDK CMake 4.0.3/Ninja 1.13.1.
Dependency constraint checks remain enabled. SDK installation has no Git metadata,
so idf.py reports a benign Git-description fallback; source hashes/version match.

Commands from the checkout, with explicit local environment paths:

```powershell
$env:PLATFORMIO_CORE_DIR = 'C:/p55s'
python scripts/check_firmware.py --target arduino --pio C:/p55s/penv/Scripts/pio.exe --build-dir .build/p28-arduino
$env:IDF_PATH = 'C:/pio/packages/framework-espidf'
$env:IDF_TOOLS_PATH = (Resolve-Path ../MotorControl-RS/build/p25/idf-tools).Path
$env:ESP_ROM_ELF_DIR = 'C:/pio/packages/tool-esp-rom-elfs'
$env:Path = 'C:/pio/packages/toolchain-xtensa-esp-elf/bin;C:/pio/packages/tool-cmake/bin;C:/pio/packages/tool-ninja;' + $env:Path
python scripts/check_firmware.py --target idf --idf-python ../MotorControl-RS/build/p25/idf-python/Scripts/python.exe --build-dir .build/p28-idf
python scripts/check_firmware.py --target component --idf-python ../MotorControl-RS/build/p25/idf-python/Scripts/python.exe --build-dir .build/p28-component
```

Actual initial build results: Arduino firmware ELF/BIN linked, all 18 shared
sources and nine portable sources compiled, 50,696 static RAM bytes and 342,861
application flash bytes. Native-IDF standalone and separate isolated component
ELF/BIN builds passed. SDK runtime allocations and physical stack/service gaps
were not measured. The deployable Arduino environment without explicit macros
fails before linking, as required. Direct cross-platform commands and expected
local banners are in each example README; no flash instruction was executed.

Independent build review also found that resolving a Linux virtualenv's Python
symlink selected the base interpreter. The checker now preserves its lexical venv
path and rejects every pinned SDK spelling of disabled dependency constraints.
Synthetic SDK tests prove these paths without requiring an SDK.

Exact source paths read include RS `examples/probe_cli/ArduinoPlatform.cpp`,
`Esp32UsbConsole.cpp`, `ProbePlatform.h`, `main.cpp`, `ProbeApp.cpp`,
`examples/common/BoardPins.h`, `examples/probe_idf/main/IdfPlatform.cpp`, `main.cpp`
and component CMake, plus `test/idf_config_test.py`, the component consumer,
PlatformIO/verifier/workflow. ADS1115 `examples/02_owner_safe_poll/main.cpp`,
`examples/esp_idf/basic/main/Ads1115IdfI2cTransport.cpp` and caller/tests were
checked; INA228's corresponding `Ina228IdfI2cTransport.cpp`, caller/tests and
workflow were checked. FieldCore `docs/guidelines/runtime_connections.md`,
`src/rs485/ArduinoRs485Backend.cpp` and dormant
`src/devices/rs485_motor/Rs485MotorModule.cpp` were read without edits.

Host tests: `firmware_probe` passes 419 checks, including startup/selection no-TX,
clock zero/equality/U32 boundary/failure, bounded pressure, interleaved passive
observations, real typed read, retained result and explicit recovery.
`esp_probe_platform` passes 3121 checks against the actual platform implementation
with SDK fakes. `firmware_build_contract` passes 19 independent negative/positive
inventory/compiler-input tests. Strict Doxygen passed with all new interfaces.
Integrated local full verification passed 50 registered tests plus the separately
retained package contract. The package gate proved independent headers,
C++11/C++17/no-RTTI, relocated install, exported source, isolated native
applications and missing-dependency rejection. Strict Doxygen and the current
version/object freshness checks passed. Quick verification also passed all 49 registered non-full tests. Remote CI follows.

Retained logs are under `.build/p28-*`; each checker summary records exact
commands, tool versions, status, artifact locations and compiler-input counts.
Initial failed build attempts remain retained alongside passing invocations.

## Remaining evidence and next work

Q20 still prevents a documented non-consuming probe/session confirmation. This
stage does not bypass it or close the Partial predecessors. There is no established
board/transceiver/pin/bitrate/node/drive/motor/firmware fixture, no USB/CAN physical
qualification, no measured stack/service gaps and no motor authorization from
RS defaults. No owned software task is moved to a later catch-all stage.
Prompt 29 is next for the offline evidence harness and explicit fixture record;
physical acquisition remains conditional. Command/schema parity remains 40-42.

## Integration and synchronization

Local full and quick verification, separate Arduino/IDF/component gates,
strict Doxygen, original-reference HEAD provenance and whitespace checks passed.
Final SDK reruns use short temporary build roots and retained output; no required
local gate is unavailable. The full suite's package consumer remains SDK-free.
Logs: `.build/p28-full/summary.json`, `.build/p28-quick/summary.json`,
`.build/p28-arduino/summary.json`, `.build/p28-sdk-final-idf/summary.json` and
`.build/p28-sdk-final-component/summary.json`. The failed long-path combined run
remains `.build/p28-firmware-final/summary.json`; its Arduino/standalone steps
passed and component failed before the short-path correction, not after it.

Implementation and portability checkpoints were committed and synchronized on
the recorded branch. Remote results below were checked against their exact SHAs;
no remote run was inferred from workflow YAML.

### First remote checkpoint and portability correction

Implementation `b76e129e5ec0e13990db01f522de2999d1db6cb8` was committed and
pushed; fresh fetch showed 0/0. [CI run 41295844931](https://github.com/janhavelka/MotorControl-CAN/actions/runs/41295844931)
passed the real Arduino and IDF/isolated-component jobs. Windows compiled all
native tests and passed firmware/owner tests, but the new checker test failed:
temporary files were on C: while checkout was on D:, and Windows normalized a
short SDK path alias. The test now controls/restores its cwd and independently
requires canonical SDK paths while preserving lexical virtualenv paths. All 19
checks pass locally; no production behavior/check was weakened. Original
failure/artifact retained under `.build/p28-ci/windows-first/`.

### Verified integrated checkpoint

`195788d986161c9187e2d59f08c486050bfbc4ba` is synchronized, with a fresh 0/0
comparison. [CI run 41296131807](https://github.com/janhavelka/MotorControl-CAN/actions/runs/41296131807)
completed successfully on that exact commit:

| Required job | Result |
| --- | --- |
| Windows GCC quick, including corrected path/alias regressions | Passed |
| Linux GCC full with ASan/UBSan, package and strict Doxygen | Passed |
| Linux Clang full with ASan/UBSan, package and strict Doxygen | Passed |
| Arduino 3.3.11 compile/link and actual shared-source inputs | Passed |
| ESP-IDF 5.5.5 standalone firmware plus isolated core component | Passed |

Machine-readable job evidence is retained in
`.build/p28-ci/verified-195788d.json`; workflow artifacts retain command/version
logs, summaries, compiler databases and firmware products. Initial failed Windows
CI remains linked above. No failed check was skipped or weakened.

This documentation checkpoint records the verified implementation. Its final
automatically triggered CI and fresh upstream comparison are checked separately
for handoff. Owned software/build scope is closed; the stage remains Partial only
on inherited Q20. No task-owned changes or unperformed local build remain, and
physical/console obligations retain their named later owners.

## Fresh independent audit of prompts 27 and 28

The [combined audit](prompts_27_28_audit.md) rechecks the original prompts,
implementation/diff, exact SDK and current sibling sources. It corrects captured
RX versus SDK handoff ordering, oversized GPIO validation, shared SDK checker
validation and failed-build diagnostic retention. Independent regressions fail
before the fixes and pass afterward. Stage scope and inherited Q20 Partial
status remain unchanged; no physical qualification is claimed.
