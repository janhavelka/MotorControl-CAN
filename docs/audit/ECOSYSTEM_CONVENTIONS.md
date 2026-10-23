# I2C ecosystem and FieldCore integration audit

Audited locally on 2026-10-23. This is source/configuration evidence for the
MotorControl-CAN implementation promptset, not a claim of fresh test, CI, or
hardware success. No sibling repository was modified and no hardware was
accessed. Source references below are relative to `C:/Users/Honza/Documents/Projects`;
line numbers identify the inspected snapshots and must be refreshed when code changes.

## Scope and inspected revisions

The shared `Projects/AGENTS.md` and each listed repository's root `AGENTS.md`
were read. Searches excluded `.git`, `.pio`, archived material and worktrees
when finding current source. All nine worktrees below were clean when inspected
with `git status --short`; these are local HEADs, not assertions about remote
branch synchronization or release qualification.

| Repository | Local HEAD | Main reason for including it |
| --- | --- | --- |
| ADS1115 | `381236ebfed813cd7a9aca34520ce031d8a0ff6b` | Explicit owner-loop example, bounded operation tokens, CLI and packaging |
| INA228 | `b28c5ea69bcb4d26b351c52c24d3c6fae9fd282b` | Cooperative jobs, hardware-state validity, rich diagnostics |
| SHT3x-main | `f65f15573e3c9b1ef04cce72ad08f307d021e563` | Logical versus transport health, shared neutral CLI processor, parser tests |
| BME280 | `d538f2e203ee26d6ddbc86f86d2b0cf4c73a6e98` | Staged configuration, partial effects, package and checker regression tests |
| MCP45HVX1 | `d3eae4a01113d3badee1b6156571666bffe91ba2` | Real-output mutation controls, uncertain state, consolidated validation runner |
| MB85RC | `1bf7c293549b1038b2d8f3b7a8d655fa74201e6e` | Family variants, transport byte/effect evidence, retained transfer results |
| PCA9555 | `498178e4d9e4acb6fab7d8b816ad5021efe8806a` | Passive lifecycle, per-attempt transport contract, cancellation cleanup |
| RV3032-C7 | `aabd8d9204e75072403fefb60cf8d2679a7f56de` | Persistent-write ambiguity, protocol-specific bounded exceptions, ABI checks |
| FieldCore-node | `da94ed5280ed42ee3a55e0d977f9cb6565c56b51` | Actual module/owner integration and existing MotorControl-RS adapter |

The eight libraries are a deliberately broad comparison set; this is not an
assertion that every sibling I2C repository was exhaustively audited. MotorControl-RS
is audited separately and remains the primary naming/architecture reference.

## Conclusion that must survive into every implementation prompt

The ecosystem is consistent about ownership, bounded work, explicit errors,
framework boundaries, reviewable tests and diagnostic tooling. It is **not**
uniform about lifecycle method names, transport signatures, health policy,
state counts, C++ language floor, example directory spelling or runner flags.
Copying one old I2C template wholesale would introduce incorrect assumptions.

Use MotorControl-RS's pure prepared-work/event architecture and function names
where the operation has the same meaning. Use the I2C repositories to audit
conventions and consumer requirements. Do not add a parallel `begin/tick` driver
engine merely to look like an I2C sensor if the RS core already expresses the
same operation as `prepare*`, `next*`, `advance*` and typed retained observations.
The future CAN adapter must translate CAN-specific evidence; it must not copy
RS485 byte framing or I2C transaction semantics.

## Verified naming and status conventions

- Public headers live in `include/<LibraryName>/`, implementation in `src/`;
  `Status.h`, `Config.h`, `CommandTable.h` and generated `Version.h` recur.
  A multi-profile motor library should retain the MotorControl-RS profile layout
  instead of flattening everything into one sensor-shaped class.
- Types and enums use PascalCase; functions and configuration fields use
  camelCase; enum values normally use uppercase names. Most I2C classes use
  `_camelCase` private members. FieldCore instead uses many `camelCase_`
  members and PascalCase enum values. These are separate codebases; do not
  transplant FieldCore formatting into the library.
- `Status` contains `Err code`, `int32_t detail`, and `const char* msg` with
  static lifetime. Representative implementation:
  `ADS1115/include/ADS1115/Status.h:39`. It provides `ok()`, `is(Err)`,
  `inProgress()`, explicit bool, `Ok()` and `Error()` helpers. Exact existing
  MotorControl-RS helpers take precedence over adding duplicate spellings.
- Error enums are library-specific. Preserve meaningful transport/protocol/
  validation/admission distinctions. Do not copy `I2C_NACK_ADDR` into CAN;
  do not collapse a CAN transport failure, SDO abort, unsupported object,
  command acceptance, or unknown physical effect into one boolean.
- Existing enum values are append-only where compatibility exists. MB85RC
  demonstrates explicitly fixed numbers and separate `BusyDetail` values:
  `MB85RC/include/MB85RC/Status.h:10` and `:37`. Similar English names do not
  imply identical numeric values across libraries.
- `state()` and `driverState()` aliases occur in
  `ADS1115/include/ADS1115/ADS1115.h:490`,
  `INA228/include/INA228/INA228.h:516`,
  `MB85RC/include/MB85RC/MB85RC.h:319`, and
  `PCA9555/include/PCA9555/PCA9555.h:248`. Preserve useful vocabulary without
  pretending all libraries share one health ABI.

## Lifecycle and ownership: evidence and important differences

| Library | Current public surface and evidence | Important distinction |
| --- | --- | --- |
| ADS1115 | `bind(DriverConfig, DeviceProfile)`, `startInitialize`, `startApplyProfile`, `startRead`, `startRecover`, `poll`, `takeResult`, `unbind`; `include/ADS1115/ADS1115.h:256`, `:270`, `:358`, `:363`; implementation `src/ADS1115.cpp:308` | Separate driver binding and device profile; synchronous `begin/tick/end` retained as compatibility |
| INA228 | `bind`, `startInitialize`, `startReinitialize`, `startInstantaneousSample`, `pollJob`, `cancelJob`, `takeJobResult`; `include/INA228/INA228.h:305`, `:340`, `:367`, `:386` | Operation IDs/request tokens and `HardwareState` distinct from health; exact job limits documented |
| SHT3x | `bind`, `requestMeasurement`, `requestEnsureIdle`, `pollJob(nowMs,budget,result)`, `cancelJob(reason,result)`; `include/SHT3x/SHT3x.h:253`, `:279`, `:288`, `:305`, `:500` | Terminal identity/result delivered through poll/cancel; not a `takeJobResult` API |
| BME280 | `startInitJob`, `startForcedMeasurementJob`, `startApplySettingsJob`, `startResyncJob`, `pollJob`, `cancelJob`; `include/BME280/BME280.h:541`, `:571`, `:594` | Retains synchronous lifecycle; `ConfigSyncState` and calibration validity are separate measurement gates |
| MCP45HVX1 | `begin`, `startSetWiperJob`, `startReadSnapshotJob`, `startRecoverJob`, `pollJob`; `include/MCP45HVX1/MCP45HVX1.h:169`, `:188`, `:212`, `:223` | `begin()` is not a generic passive-bind contract; implementation clears state before validation (`src/MCP45HVX1.cpp:42`), and legacy OFFLINE gating remains |
| MB85RC | `bind`, `begin`, `requestRead/Write/Fill/Verify/VerifiedWrite`, `pollTransfer`, `takeTransferResult`, `cancelTransfer`; `include/MB85RC/MB85RC.h:251`, `:775`, `:857`, `:874`, `:890` | Buffer lifetime, byte progress, variant selection and unconsumed terminal admission matter; `bind()` rejects pending results (`src/MB85RC.cpp:135`) |
| PCA9555 | Passive `bind` and `begin` alias; `startApplyImage`, `startVerifyImage`, `startReadInputs`, `pollOperation`, `cancelOperation`, `takeOperationResult`; `include/PCA9555/PCA9555.h:233`, `:273`, `:286`, `:297` | Required pointer cleanup may continue after cancellation before a terminal result; only three health states |
| RV3032 | Passive `begin`, `probe`, `start...Job`, `pollJob`, `pollEeprom`, local `end`; `include/RV3032/RV3032.h:541`, `:762`, `:905` | Dedicated bounded startup primary-cell ensure is an explicit narrow synchronous exception, not a reusable general recovery pattern |

The common production properties to preserve are:

1. Binding/admission/preparation is bus-silent where advertised; only the owner
   executes physical transfers. A successful local bind proves neither device
   identity nor readiness nor configuration synchronization.
2. Fixed memory, explicit caller time, immutable overall deadlines, finite
   transfer/step budgets, zero-budget behavior and observable waiting phases.
   No hidden worker, bus ownership, lock, framework delay, allocator or logger
   inside the core.
3. One clear operation owner with exact request/target/generation correlation;
   retained terminal results cannot be silently overwritten. Cancellation is
   local intent plus factual settlement of already submitted work, never proof
   that a device-side operation did not execute.
4. Bus handles, pins, bitrate, filters, interrupt queues, scheduling,
   serialization and bus recovery belong to the application adapter/owner.
   A library's protocol reconciliation must not secretly reset shared hardware.
5. Configuration/coordinate caches and command effects require independent
   validity. Partial or ambiguous writes preserve original errors and require
   explicit reconciliation before state-dependent actions.

Concrete transport evolution is visible in
`PCA9555/include/PCA9555/Config.h:12`: `TransportCode`, `WriteEffect`, completed
TX/RX counts and a terminal `TransportResult`. Its callbacks at `:66` explicitly
forbid retry, pointer retention and recursive driver entry. BME280 and MB85RC
also use richer transport results; ADS1115/INA228/SHT3x/MCP45HVX1/RV3032 expose
`Status` callbacks. Thus “all I2C libraries use Status-returning transport” is
false even though fallible public APIs consistently expose typed status.

`ADS1115/examples/02_owner_safe_poll/main.cpp:1` provides a useful standalone
owner example: application mutex and bus setup, transfer timeout including lock
time, one callback per owner pass, retained token results and bounded recovery
attempts. Copy the ownership demonstration, not its Wire calls or ADC-specific
retry/readiness behavior.

## Health semantics require an explicit design decision

Health must be inspectable without hardware I/O. It must not erase a failed
operation merely because a later cleanup frame or unrelated heartbeat succeeds.
Transport liveness, CANopen node state, drive statusword/faults, command effect,
configuration validity and physical position confidence are different facts.

| Evidence | Why it matters for CAN |
| --- | --- |
| ADS1115 tracks callbacks and saturates counters; `src/ADS1115.cpp:2666` | Define what one success means; a transmitted frame is not an accepted drive operation |
| INA228 `HardwareState` at `include/INA228/INA228.h:168`, health policy at `include/INA228/Config.h:135` | `PASSIVE` is default; legacy `LATCH_OFFLINE` exists. Health policy and synchronized configuration are distinct |
| SHT3x logical health after CRC/status validation, `src/SHT3x.cpp:2583`; `AGENTS.md` logical-operation section | A valid transport callback can still produce a failed protocol operation; separate physical and logical counts |
| SHT3x `Config::healthPolicy` defaults to `LATCH_OFFLINE`, `include/SHT3x/Config.h:232` | Do not infer observational default from general ownership language. FieldCore explicitly selects `OBSERVE_ONLY` in `src/devices/i2c/Sht3xModule.cpp:110` |
| PCA9555 `DriverState` has only UNINIT/READY/DEGRADED, `include/PCA9555/PCA9555.h:16` | Four-state health is not universal. Successful pointer cleanup intentionally does not clear preceding read failure |
| BME280 has semantic-failure handling beyond raw transport health and `ConfigSyncState`, `include/BME280/BME280.h:334` | Document failure attribution and admission independently, including status/readback errors |
| MB85RC session totals explicitly wrap, `include/MB85RC/MB85RC.h:73`, `src/MB85RC.cpp:2142`; RV3032 totals also wrap, `src/RV3032.cpp:2757` | Saturation is desirable for the new library, but not an existing universal convention |
| MCP45HVX1 OFFLINE admission gates, `src/MCP45HVX1.cpp:793`, with uncertain-state helpers in its public class | Do not import automatic health gating into a motor stop/reconciliation path |

For MotorControl-CAN, keep MotorControl-RS's existing observation/health
ownership when available. If additional health types are necessary, specify
their counters, reset lifetime, time validity, overflow behavior, protocol versus
transport attribution and effect on admission before implementing them.
Transport health must never silently authorize motion, infer standstill, clear
an uncertain trigger, or block an otherwise valid explicit stop.

## CLI, examples, Python and CI conventions

Observed layout:

```text
include/<Library>/        framework-neutral public API
src/                     core implementation
examples/01_basic_bringup_cli/
examples/common/         example-only glue; never public library dependencies
examples/<native-IDF-example>/
test/                    native protocol/contract/fault tests
tools/                   guards, parser regressions, HIL runner
scripts/                 version generator, Windows pio.cmd
docs/                    durable contracts, device sources, validation evidence
.github/workflows/       CI
library.json, platformio.ini, CMakeLists.txt, idf_component.yml, Doxyfile
AGENTS.md, README.md, CHANGELOG.md, CONTRIBUTING.md, SECURITY.md
```

Directory exceptions are real: ADS1115/INA228 use `examples/esp_idf/basic`,
SHT3x/BME280 use `examples/idf/basic`, MCP45HVX1/MB85RC/PCA9555 use
`examples/espidf_basic`; RV3032 currently has Arduino examples and IDF notes
without the same root IDF component/example layout. Choose MotorControl-RS's
layout for the new repository and explain deliberate additions once.

Common diagnostic vocabulary includes `help/?`, `version/ver`, `cfg/settings`,
`drv`, `state`, `probe`, `recover`, read/status commands and explicit job/result
inspection. Not every repository has every command. `scan` means I2C ACK
discovery in these examples; it is not a license to send broadcast CANopen
commands. CAN discovery must be separately specified and bounded.

Representative executable contracts:

- `SHT3x-main/tools/sht3x_cli_contract.py:15` classifies commands as cached,
  pure, core jobs, bounded synchronous work, CLI jobs, raw I/O or lifecycle;
  `COMMAND_SPECS` ties help, aliases, execution owner and mutation classes.
  Its current CLI implementation is framework-neutral example code shared by
  Arduino and native IDF, not an Arduino compatibility facade.
- `MB85RC/tools/_contract_data.py:3` centralizes mandatory command names.
- `PCA9555/examples/common/CliStyle.h:14` defines help sections, command columns,
  ANSI result colors and the `> ` prompt. Styling stays outside core APIs.
- `MCP45HVX1/tools/run_hil_mcp45hvx1.py:382` records NOT_RUN and explicit unsafe
  skips; output changes first require a readable baseline and later restoration.
  For a motor, restoring coordinates or replaying a move is not an acceptable
  generic cleanup. Adapt the classification model and retain truthful effects.
- `PCA9555/tools/run_i2c_hil.py:1629` separates dry-run verdicts;
  `:1667` writes machine/human summaries; `:1803` explicitly states that a dry
  run is not physical HIL. Follow this evidence distinction.
- Parser entry flags differ: ADS1115 CI invokes `--parser-test`; INA228,
  MCP45HVX1 and other runners use `--parser-self-test`. Inspect actual argument
  parsers instead of generating commands by analogy.
- Live serial support is imported lazily in MCP45HVX1's runner (`:141`),
  allowing parser checks without pyserial/hardware. Runners bound reads,
  writes, quiet periods, command durations and finite stress/soak runs.

The future CLI contract must be shared by help, dispatch, both framework
examples, parser tests and the Python runner. Assert actual bytes/semantics,
including malformed commands, integer overflow, stale identity, explicit motion
opt-in, cached status inspection, cancellation and result retention. Mutation
tokens and host flags belong to the tool/application surface; avoid making them
an unrelated policy dependency of the core library.

CI patterns inspected, with exact source anchors:

| Repository source | Gates present in configuration |
| --- | --- |
| `INA228/.github/workflows/ci.yml:85`, `:111`, `:147` | Native Unity, Arduino S2/S3, true IDF build, Python compile/parser/dry-run tests, CLI/owner/timing guards, generated version, strict Doxygen, package compile |
| `BME280/.github/workflows/ci.yml:72`, `:75`, `:92` | Native and native sanitizer tests, CLI/HIL/IDF contracts, release metadata, package contents and checker regression tests |
| `MCP45HVX1/.github/workflows/ci.yml:78`, `:103`; `tools/validate.py:31` | Static guards, parser self-test, native suite, compiled Arduino CLI behavior tests, standalone core compile; local wrapper aggregates these |
| `PCA9555/.github/workflows/ci.yml:74`, `:119` | Version, portability/timing/CLI/HIL/IDF contracts, parser tests, native suite, packaged consumer and strict docs |
| `RV3032-C7/.github/workflows/ci.yml:98`, `:105`, `:119` | ABI guard, Python `-S` parser tests/dry-run, source and packed-package checks |
| `ADS1115/.github/workflows/ci.yml:151` | Packaged standalone compile explicitly uses C++11; do not claim every sibling's minimum is C++17 |

Most inspected projects build a C++17 core; choose and prove the new library's
language floor against MotorControl-RS rather than silently raising it.
Pinned SDKs differ (for example INA228 IDF v6.0.1, BME280 v5.3.2, PCA9555 v5.4).
Do not mix a newer API with an older copied toolchain manifest. Audit actual
CAN/TWAI adapter APIs against the selected SDK at implementation time.

On Windows reuse `scripts/pio.cmd` and the user's managed PlatformIO. Do not
install a second Core. The shared `Projects/AGENTS.md` requires relevant local
checks and exact-final-commit CI for major code/build/integration changes; this
documentation audit did not dispatch sibling CI. Version generation and package
export lists must be checked, including downstream use without source-tree
scripts or examples. Archive smoke tests should compile public headers and link
a real consumer using only shipped files.

Useful fault-test precedents, inspected as source but not run here:

- ADS1115 `test/test_basic.cpp:915`: initialization failure after first write
  preserves dirty state.
- INA228 `test/test_basic.cpp:3237`: cooperative binding is zero-I2C.
- SHT3x `test/test_basic.cpp:3561`: owner identity, zero budget, exactly-once
  terminal delivery; `:2334` counts readback mismatch as one logical failure.
- BME280 `test/test_basic.cpp:908`: short successful callback is a failed
  transfer without retry; `:2479` tests cancellation after each exposed write.
- MCP45HVX1 `test/test_basic.cpp:1668`: partial mutating sequence stays uncertain.
- MB85RC `test/test_basic.cpp:1662`: health OFFLINE does not gate owner writes;
  `:1790` checks one-instruction budget.
- PCA9555 `test/test_basic.cpp:549`, `:668`, `:884`: budget/result retention,
  cancellation cleanup, and failure at every apply phase without hidden retry.
- RV3032 `test/test_native/test_datetime.cpp:3772`, `:4096`: ambiguous persistent
  write reconciliation and failure ordinals never replay the write-one command.

## FieldCore-node: current consumer boundary

`FieldCore-node/AGENTS.md` makes `docs/guidelines/` authoritative, says resource
owners call hardware libraries, and forbids services/CLI/Web from owning buses.
`docs/guidelines/ownership.md:39` distinguishes ownership names from runtime
threads: current enabled I2C and RS485 owners have RTOS workers, while services
consume bounded command/status/result contracts. This is not a requirement that
MotorControl-CAN create an RTOS task.

Concrete current interfaces:

- `include/TunnelMonitor/i2c/I2cOwnerTransport.h:12` separates transport status,
  write effect, byte counts and timing; its owner callbacks take immutable
  deadlines. The module never receives an unrestricted global bus handle.
- `src/devices/i2c/Ina228Module.cpp:377` calls the real library's
  `pollJob(..., 1U)`, `:596` consumes the exact library operation ID, and
  `:699` copies cached typed status. Its callbacks pass the original deadline
  through the owner (`:925`, `:970`). This is an actual implementation of the
  budget/identity/transport boundary, not just guideline text.
- `include/TunnelMonitor/device/Rs485MotorModule.h:3` includes the real
  MotorControlRS profile headers. The module holds fixed `MoveContext`,
  `ActionContext`, `ReadContext`, copied pending work and one retained result.
- `src/devices/rs485_motor/Rs485MotorModule.cpp:131` validates the full motor
  identity/deadline and translates application commands to `prepareMoveAbsolute`,
  `prepareMoveRelative`, `prepareStop` and read preparation.
- The same module's `:254` calls `nextMove/nextAction/nextRead` and returns
  copied prepared work to the owner. `:363` accepts factual owner observations;
  `:411` separates response qualification from mere TX acceptance. `:640`
  takes the retained motor result; `:684` provides cached status.
- `include/TunnelMonitor/contracts/MotorControl.h:16` limits the current typed
  firmware contract to Move, ReadState and UrgentStop. It preserves rational
  commissioned scale, target/configuration/reference generations, absolute
  deadlines, independent admission/wire/execution outcomes and setup/trigger
  effects. A fresh observation and a command ACK are not interchangeable.
- `include/TunnelMonitor/rs485/Rs485OwnerTransaction.h:130` retains actual
  transport settlement, accessible RX count versus total bytes, clock validity,
  source/timing qualification and execution uncertainty. Those are RS485
  contracts; future CAN integration needs a CAN-specific equivalent rather
  than reusing a byte stream container.

**Deployment qualification:** the motor adapter is implemented but dormant.
`docs/guidelines/rs485_devices.md:7` and `:19` say neither current product
selects the motor module or library; `:73` reiterates this. Its host dependency
is MotorControl-RS v1.0.0:
`config/build_profiles.json:138`, with resolved commit
`4125a092d91b4b6423d1664ee51fff209110006f` in
`scripts/prepare_rodtester_host_dependencies.py:12`.
`scripts/check_motor_runtime.py:39` builds the real library and integration
fixtures. Existing source and check scripts are not proof those checks or
physical motor qualification passed during this audit.

No existing CAN owner/backend was established by this audit of current include/
src and authoritative bus guidance. Do not describe the new library as already
integrated with FieldCore, add a dormant CAN subsystem there, or import
`TunnelMonitor` application types into this repository.

### Framework-independent integration pattern to demonstrate

```text
UI / CLI / application service
    -> fixed command + exact request/target/generation + original deadline
application CAN owner
    -> profile module holding one library operation context
    -> library prepare*/next* produces copied bounded CAN work or wait
application CAN owner
    -> chosen CAN backend, bounded TX and bounded RX demultiplexing
    -> factual qualified event into the library's advance* operation
profile module
    -> retained typed terminal result + copied cached diagnostics
UI / CLI / application service
    -> exact result consumption and application-specific motion policy
```

This is a proposed integration pattern, not a claim of implemented CAN APIs.
Show it in a small standalone owner example with the real library, one host
fake transport confined to tests, and separately a native IDF/Arduino adapter.
Applications may use an RTOS worker, a cooperative loop or another scheduler;
the library imposes only serialization, memory lifetime, event and deadline
contracts. Multi-node CAN RX belongs to one shared owner/router so one device
instance cannot consume another's frames. Do not create a general service
registry or framework to demonstrate this.

Preserve FieldCore-relevant semantics without hard-coding its policy: exact
target generations, no reused result IDs, fixed result retention, no automatic
motion replay, read-only reconciliation after ambiguity, explicit stop that
does not inherit move-only coordinate readiness, and cached status that never
performs I/O or acknowledges an uncertain motion.

## Required recurring audit checklist for the AI coder

Run this at the start of **every prompt**, not just repository creation. Record
a short delta when a previously audited HEAD is unchanged; do not reread huge
historical archives or dispatch unrelated sibling CI.

1. Read applicable `AGENTS.md`, current MotorControl-CAN contracts, prior prompt
   evidence and open decisions. Inspect branch/HEAD/dirty state; preserve other
   agents' work. Sibling instructions are evidence for comparison, not authority
   to edit/push sibling repositories from this task.
2. Inspect current MotorControl-RS public/profile headers, examples, owner
   contracts, CLI, tests, runners and CI for the feature being implemented.
   Capture HEAD and semantic/function mapping before adding names.
3. Inspect the current I2C comparison set above for public Status, configuration,
   ownership, bounded lifecycle, health and naming. Refresh exact lines and
   describe exceptions; never assert a single shared API where none exists.
4. For async/transport changes inspect ADS1115, INA228, SHT3x, PCA9555 and MB85RC
   real source and failure tests. For mutation/persistence inspect MCP45HVX1 and
   RV3032 effects/cleanup tests. For CLI/tooling inspect SHT3x contracts,
   MCP45HVX1 runner tests and BME280/PCA9555 packaging gates.
5. For integration-facing changes inspect current FieldCore guidelines,
   actual owner/module code and exact dependency pins. Distinguish selected
   production code, dormant implementation, target specification and historical
   evidence. Keep integration adapters out of the core and out of FieldCore
   unless separately requested.
6. Write a compact decision row: desired behavior; MotorControl-RS name;
   relevant I2C/consumer precedent; CAN-specific difference; evidence source;
   test proving the contract. Reuse names only when their semantics match.
7. Read the CI/workflow and package configuration before writing tests or build
   changes. Reuse actual commands and pinned dependencies. A static grep guard
   supplements behavioral tests; it does not prove a transport state machine.
8. Implement and run focused native fault/identity/deadline/budget tests;
   appropriate standalone/package and framework builds follow the changed scope.
   Never attach to a live motor or emit output-changing traffic as part of an
   otherwise hardware-free runner self-test.
9. Update API/CLI/reference mappings, evidence gaps and validation state. Report
   tests actually run with revision/commands; mark unavailable hardware and
   unsupported family capabilities honestly. Do not relabel a dry run as HIL.
10. At a major integration milestone, verify required CI on the exact final
    commit when a remote/workflow exists. Do not claim a source audit or an older
    green run proves the new commit.

## Audit limits and concrete cautions

- This audit checked selected headers, implementations, test sources, examples,
  manifests and workflows. It did not run sibling test suites, prove every
  function correct, inspect all historical branches or certify hardware.
- Even authoritative contributor prose can omit a compatibility exception:
  SHT3x's actual default OFFLINE latch and MB85RC/RV3032 wrapping counters are
  concrete reasons to audit code and tests together.
- MCP45HVX1's local `tools/validate.py:69` can print SKIP when a compiler is
  missing and still finish; the new validation runner must distinguish required
  checks from optional checks rather than interpreting process success as full
  coverage.
- I2C-specific recovery, ACK-based discovery, write-read byte effects, CRC
  layouts, address ranges, aliases and ADC/sensor statuses do not transfer to
  CANopen unchanged. The device/protocol evidence audit owns those facts.
- Framework-neutral means the shipped library compiles and is usable without
  Arduino, ESP-IDF, FieldCore or example helpers. Merely placing hardware calls
  behind a class named adapter is insufficient if public headers include them.
