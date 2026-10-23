# Fresh naming, health and ownership re-audit

Audit date: 2026-10-23, Europe/Prague. Scope: current local source names,
application ownership, diagnostics, tests/runners and configured checks needed
to write [the selected CAN conventions](../prompts/CONVENTIONS.md). This is
documentation/source inspection, not execution of the implementation promptset,
sibling test/CI success, vendor protocol validation or hardware qualification.
No sibling source, original reference, baseline snapshot or FieldCore file was
changed. No fetch, checkout, flash, physical transfer or motor operation ran.

Read `Projects/AGENTS.md`, target `AGENTS.md`, prompt README/COMMON/stage 00,
multi-drive decision, current architecture/API mapping and historical ecosystem
and RS audits before resolving names. Sibling root guidance was inspected as
source context; its publishing/bench instructions do not authorize target work.
The target was already a dirty promptset with concurrent work. Only the new
`docs/prompts/CONVENTIONS.md` and this report belong to this audit task.

## 1. Exact local source identities

All paths below are relative to `C:/Users/Honza/Documents/Projects`. These are
local HEADs; no claim of upstream synchronization is made. `git status --short`
was empty for every listed repository except MotorControl-RS.

| Repository | Fresh HEAD | Dirty state / delta from historical audit |
| --- | --- | --- |
| MotorControl-RS | `540e640c4bb7bff989082d765778fcaebfdf0d7e` | Untracked TL57 reference material listed below; previous RS audit used `5da4ba7f13be457ea7bf9706ffd7d546821669c7` |
| ADS1115 | `235af3e8c722661a89a44766a4ccd903a654f6fe` | Clean; ecosystem audit used `381236ebfed813cd7a9aca34520ce031d8a0ff6b` |
| INA228 | `b28c5ea69bcb4d26b351c52c24d3c6fae9fd282b` | Clean; unchanged HEAD |
| SHT3x-main | `f65f15573e3c9b1ef04cce72ad08f307d021e563` | Clean; unchanged HEAD |
| PCA9555 | `498178e4d9e4acb6fab7d8b816ad5021efe8806a` | Clean; unchanged HEAD |
| MB85RC | `1bf7c293549b1038b2d8f3b7a8d655fa74201e6e` | Clean; unchanged HEAD |
| MCP45HVX1 | `c6c7feefd79b9fc16ddc7b7786ad187e5185fde6` | Clean; ecosystem audit used `d3eae4a01113d3badee1b6156571666bffe91ba2` |
| BME280 | `d538f2e203ee26d6ddbc86f86d2b0cf4c73a6e98` | Clean; unchanged HEAD |
| RV3032-C7 | `aabd8d9204e75072403fefb60cf8d2679a7f56de` | Clean; unchanged HEAD |
| FieldCore-node | `da94ed5280ed42ee3a55e0d977f9cb6565c56b51` | Clean; unchanged HEAD; read-only consumer context |

MotorControl-RS untracked paths observed:

```text
docs/pdf-extracted-md/Lichuan_TL-R_Serial_Examples_CN.md
docs/pdf-extracted-md/Lichuan_TL57-R_Operation_Manual_CN.md
docs/pdf-extracted-md/Lichuan_TL57-R_User_Manual_CN.md
docs/reference/lichuan_tl57_rs485_models.json
docs/reference/lichuan_tl57_rs485_registers.json
docs/reference/lichuan_tl57_rs485_sources.json
docs/vendor/lichuan_tl57_rs485/
```

These are not CAN capability evidence. Current tracked RS operation/owner
headers, implementations and callers were inspected directly; the old copied
baseline was not overwritten. The target HEAD at first inspection was
`1d103c2fd94a7839a18afeb9b305b8a45c74c0da`, with extensive existing tracked and
untracked promptset work; a target HEAD alone does not identify that working
tree's documentation contents.

## 2. Status and operation evidence

| Observed source | Exact names / behavior | Selected CAN consequence |
| --- | --- | --- |
| `MotorControl-RS/include/MotorControlRS/Status.h:13` | `Err::{OK,INVALID_CONFIG,ILLEGAL_VALUE,UNSUPPORTED,CRC_ERROR,FRAME_ERROR,EXCEPTION}`; `Status { code, detail, msg }`; `isOk()`, explicit bool; free `Ok()` and `errToString()` | Use RS helper spellings; remove RTU-specific errors; propose `PROTOCOL_ERROR` with separately retained raw abort |
| `ADS1115/include/ADS1115/Status.h:39`; `INA228/include/INA228/Status.h:86` | Member `ok()`, `is(Err)`, `inProgress()`, explicit bool; static `Status::Ok()`/`Status::Error()` | Similar style, different concrete API; these names are not RS API |
| `SHT3x-main/include/SHT3x/Status.h:67`; `PCA9555/include/PCA9555/Status.h:70`; `MB85RC/include/MB85RC/Status.h:50`; `RV3032-C7/include/RV3032/Status.h:56` | Same broad I2C helper family, library-specific enum/detail domains | Do not mechanically copy I2C codes or an asynchronous status into pure CAN preparation |
| `MCP45HVX1/include/MCP45HVX1/Status.h:34`; `BME280/include/BME280/Status.h:76` | MCP has `ok()`/`inProgress()` and static factories; BME also has `is()` and overloads of `Error()` | Even I2C helper surfaces are not identical |
| `MotorControl-RS/include/MotorControlRS/profiles/ess_rs/Reads.h:176` | `prepareIdentity`, `prepareConfig`, `prepareState`, `nextRead`, `advanceRead`, `getIdentity`, `getConfig`, `getStateBlock` | Preserve operation-specific bounded reads and checked getters |
| RS `profiles/ess_rs/{Position,Actions,Velocity,Homing}.h` | `prepareMoveAbsolute/Relative/Angle`, `nextMove/advanceMove`; action/velocity/home families; `getHomeReference` | Preserve meanings; concrete contexts carry terminal outcome/evidence; no fictional generic result getters |
| RS `profiles/ess_rs/{DriverSettings,ControlSettings,Segments,Tuning}.h` | `nextDriver`, `advanceDriver`, `getDriver`, `getIo`, `getControl`, `getSegment`, `getTuning` | Typed native groups stay profile-specific; a raw object API does not complete native coverage |
| RS `ActionOperation.h:50`, `ReadOperation.h:80`; `src/profiles/ess_rs/Actions.cpp:9` | State/outcome/execution/completion/reason enums are independent; `Status::detail` holds a concrete reason | Preserve this separation; add a named valid-service-rejection outcome for CAN rather than treating an abort as malformed data |

RS `MoveContext` retains setup and trigger execution separately plus staging,
trigger, activity, position-match, observation and failure evidence. The
operation's `status` is not a standalone result getter. Some profile data
contains serial tuples, 16-bit native RPM, subdivision and ESS feedback
assumptions. Matching names does not authorize copying those restrictions into
multi-vendor common motion. The selected contract explicitly keeps logical
axis identity and exact common targets separate from native representation.

## 3. Actual health names, counter scope and differences

All eight I2C libraries expose the familiar method family `state()`,
`driverState()`, `lastOkMs()`, `lastErrorMs()`, `lastError()`,
`consecutiveFailures()`, `totalFailures()`, `totalSuccess()` in their main public
header. This is source-level naming similarity, not a uniform lifecycle or
health ABI. No installed RS `Health`, `DriverState`, `health()` or sensor-shaped
driver lifecycle was found at the inspected seam.

| Repository and source anchors | Actual policy/state/count behavior |
| --- | --- |
| ADS1115 `include/ADS1115/ADS1115.h:16,:490`; `Config.h:163`; `src/ADS1115.cpp:2656` | Four `DriverState` values UNINIT/READY/DEGRADED/OFFLINE; owner-safe `DriverConfig` uses fixed passive threshold five, diagnostic only; totals and consecutive failures saturate; transport tracking is not motor completion |
| INA228 `include/INA228/Config.h:135,:193`; `INA228.h:21,:168,:516`; `src/INA228.cpp:2970` | `HealthPolicy::{PASSIVE,LATCH_OFFLINE}`, default PASSIVE; OFFLINE latching/admission only for explicit legacy policy; separate `HardwareState`; saturating totals/consecutive count |
| SHT3x-main `include/SHT3x/Config.h:172,:232`; `SHT3x.h:389`; `src/SHT3x.cpp:2570` | `HealthPolicy::{OBSERVE_ONLY,LATCH_OFFLINE}`, default LATCH_OFFLINE; health records complete logical operations including protocol failures; raw callback counters are separate; saturation |
| PCA9555 `include/PCA9555/PCA9555.h:16,:201,:248,:424`; `src/PCA9555.cpp:231` | Exactly three states UNINIT/READY/DEGRADED; passive; saturating counters; cleanup uses distinct tracking so successful pointer cleanup does not erase the failed read |
| MB85RC `include/MB85RC/MB85RC.h:15,:60,:73,:319,:456`; `src/MB85RC.cpp:2130` | Four states; OFFLINE diagnostic, does not gate owner writes; uint32 session success/failure totals explicitly wrap; uint8 consecutive count saturates |
| MCP45HVX1 `include/MCP45HVX1/MCP45HVX1.h:112,:275,:312`; `src/MCP45HVX1.cpp:1423` | Four states with legacy OFFLINE admission; saturating totals/consecutive count; last error survives success; uncertain hardware state is separate |
| BME280 `include/BME280/BME280.h:334,:699,:723`; `src/BME280.cpp:2492` | Four states plus independent `ConfigSyncState`; semantic failures beyond callback health; saturating totals, explicit time-valid flags |
| RV3032-C7 `include/RV3032/RV3032.h:63,:621,:650,:666`; `src/RV3032.cpp:2745` | Four observational states; uint32 callback totals explicitly wrap; consecutive failures saturate; EEPROM/job state is independent |

Selected CAN choice: proposed example-only `DriverState`, `Health`, `Stats` and
cached accessors, defined in the conventions file. `Health` alone owns
per-binding logical-service totals, live consecutive count and service times.
`Stats` alone owns physical-interface/controller/queue counters. Saturation is a
deliberate choice supported by RS/ADS/INA/PCA/BME, **not** a claim that every
sibling saturates. No configurable OFFLINE latch or sensor retry policy enters
CAN. Endpoint liveness, logical failure history and drive faults remain
separate; a valid abort can prove communication while failing the requested
service. `stats reset` cannot clear live consecutive-failure evidence or faults.

## 4. Actual owner and consumer names

`MotorControl-RS/examples/common/RtuBusOwner.h` defines `BusOwner`, `RequestId`,
`SequenceId`, `BusAdmission`, `Outcome` and immutable retained `Completion`.
The public owner methods are **`admit`, `admitUrgent`, `service`, `result`,
`release`, `cancel`**, not `submit` and not owner `poll`. `recover`,
`recoveryResult`, `releaseRecovery`, `beginSequence`, `invalidate` and
`invalidateProducer` retain explicit control/result ownership. The runner
underneath has its own `poll()`; that does not rename the bus-owner contract.
`result(id)` is non-consuming until `release(id)`. `RequestId` includes the
originating owner and slot generation, with no claim of wire correlation.

`MotorControl-RS/examples/common/RtuRunner.h:143` defines example `Stats` with
saturating `started`, `frames`, `failed`, `timeouts`, `cancelled`, `rxBytes`,
`discarded`, `echoBytes`, `traceOverwritten`. Its accessors are `stats()` and
`clearStats()`; clearing does not clear terminal evidence or interlock.
The UART adapter separately has `CaptureStats`, `stats()` and `resetStats()`.
CAN selects owner `stats()/clearStats()` and CAN frame metrics; it must not copy
RTU byte gaps, local echo, DE/RE or UART capture fields.

FieldCore source inspected read-only:

- `include/TunnelMonitor/contracts/Health.h`: actual `HealthState` values
  `Initializing`, `Ok`, `Degraded`, `Fault`, `Unknown`, `Disabled`, plus independent
  presence/role/service/bus facts. No CAN owner is established by these enums.
- `include/TunnelMonitor/contracts/MotorControl.h`: `MotorAdmission`,
  `MotorWireSettlement`, `MotorExecution`, `MotorStoppedEvidence`,
  `MotorWriteEffect`, exact `MotorAxisSnapshot`, request/binding generations,
  retained uncertainty and capacity assertions.
- `src/devices/rs485_motor/Rs485MotorModule.cpp`: adapts actual RS free functions
  and typed evidence; runtime configuration rejects motor retries; stop has
  different prerequisites from move. It is a consumer example, not a CAN adapter.
- `include/TunnelMonitor/rs485/Rs485OwnerTransaction.h` and
  `include/TunnelMonitor/i2c/I2cOwnerTransport.h`: existing bounded owner boundaries,
  physical byte/effect observations and explicit deadlines remain framework-owned.
- `src/devices/i2c/Sht3xModule.cpp:110` explicitly selects
  `SHT3x::HealthPolicy::OBSERVE_ONLY`; `Ina228Module.cpp:107` requires PASSIVE.
  Thus even actual FieldCore integration chooses different library enum spellings.
- `test/native/test_owner_rs485/suite_main.cpp:97` registers real motor exact-take,
  interrupted stop, independent terminal, ingress race, queue pressure and
  reconciliation tests in `test_rs485_task_fake.cpp`.

The selected CAN integration stays in this repository. No FieldCore enum,
header, scheduler or device module becomes a mandatory core dependency.

## 5. Callers, tests, runners, CI and documentation actually inspected

This is a focused source inventory, not an assertion that every line or test was
audited. Header and implementation paths in sections 2-4 are additional inputs.

| Repository | Additional inspected paths and relevant behavior |
| --- | --- |
| MotorControl-RS | `examples/probe_cli/ProbeApp.cpp`, `ProbeConsole.h/.cpp`, `ConsoleReport.h`; `examples/common/RtuBusOwner.cpp`; `test/bus_scheduling_test.cpp`; `scripts/bench_probe.py`, `scripts/verify.py`; `CMakeLists.txt`, `.github/workflows/verify.yml`: real command inventory, @ID JSONL, retained context formatting, owner scheduling, finite runner, explicit test registration and C++11 consumers |
| ADS1115 | `examples/02_owner_safe_poll/main.cpp`, `examples/common/HealthView.h`; `test/test_basic.cpp`; `tools/run_i2c_hil.py`; `.github/workflows/ci.yml`: externally serialized one-callback owner turns, passive health, parser-test naming and strict Doxygen |
| INA228 | `test/test_basic.cpp` at `test_passive_health_never_suppresses_owner_requested_transport` and legacy latch tests; `tools/test_run_i2c_hil_parser.py`, `tools/run_i2c_hil.py`; `.github/workflows/ci.yml`: passive versus latch, complete health evidence, `--parser-self-test` and dry-run |
| SHT3x-main | `examples/common/TransferStats.h`; `test/test_basic.cpp` logical-failure/OBSERVE_ONLY/saturation tests; `tools/sht3x_cli_contract.py`; `.github/workflows/ci.yml`: separate transfer/logical counts and `CommandSpec` execution/mutation classes shared across platform semantics |
| PCA9555 | `test/test_basic.cpp` cancellation cleanup and `test_input_read_failures_survive_successful_cleanup_in_health_and_validity`; `tools/check_cli_contract.py`, `tools/test_run_i2c_hil_parser.py` and configured runner paths; `.github/workflows/ci.yml`: primary-failure retention, CLI/IDF/HIL gates and Doxygen |
| MB85RC | `test/test_basic.cpp` observational-OFFLINE and instruction-budget tests; `tools/_contract_data.py`, configured `tools/hil_runner.py` parser entry; `.github/workflows/ci.yml`: explicit shared CLI inventory and no universal I2C offline gate |
| MCP45HVX1 | `examples/common/HealthView.h`, `examples/common/CliShell.h` inventory; `test/test_basic.cpp` pre-mutation versus uncertain writes and recovery; configured `tools/run_hil_mcp45hvx1.py`/parser tests; `.github/workflows/ci.yml`: uncertainty and `Doxyfile.api` plus Doxyfile gates |
| BME280 | `examples/common/HealthView.h`; `test/test_basic.cpp` short transfer/partial apply/cancel/dirty-state tests; configured parser/CLI/IDF gates; `.github/workflows/ci.yml`: Arduino-specific view stays example-only; sanitizer configuration and Doxygen exist |
| RV3032-C7 | Actual `test/test_native/test_datetime.cpp` (not nonexistent `test/test_basic.cpp`); `tools/test_hil_health_snapshot.py`, `tools/hil_cli_runner.py`; `.github/workflows/ci.yml`: counter-wrap assertion, observational OFFLINE, strict full-snapshot parser, `python -S` parser/dry-run and ABI gate |
| FieldCore-node | Owner/module/contracts/tests above; `.github/workflows/verify.yml`, `docs.yml`: verifier-owned gate list, `scripts/build_doxygen.py` and documentation test registration |

Representative commands observed in configured source, **not run by this audit**:

```text
# MotorControl-RS native workflow
python scripts/verify.py --mode quick --build-dir build/verification
# MotorControl-RS firmware workflows
python scripts/verify.py --mode full --firmware arduino --build-dir build/verification
python scripts/verify.py --mode full --firmware idf --build-dir build/verification
# INA228 configured parser check (other siblings differ)
python tools/run_i2c_hil.py --parser-self-test
# RV3032 configured dependency-light parser check
python -S tools/hil_cli_runner.py --parser-self-test
```

The current RS workflow pins `espressif/idf:v5.5.5` for native IDF. Toolchain
versions are inputs to a later adapter decision, not tested CAN compatibility.
The RS workflow actually delegates checks to `scripts/verify.py`; the planned
CAN verifier must run its own real targets. I2C scripts/CLI aliases/IDF folder
spellings differ and must not be generated from analogy.

Public RS headers already use Doxygen comments, but no root Doxyfile was found
in the inspected source inventory. A failed attempt to read one was resolved by
file discovery; this report does not invent an RS documentation generation gate.
Strict Doxygen for CAN is a selected addition based on observed sibling and
FieldCore gates plus the requested documentation scope.

## 6. Local validation and limits

Revision/dirty inspection used `git rev-parse HEAD` and `git status --short` in
all ten checkouts. Source discovery used `rg --files`; focused symbols and
callers/tests/workflows used `rg -n` and bounded reads. Initial cross-repository
relative-path mistakes were corrected using the actual repository roots; no
missing file is represented as inspected code. The real RV3032 native test
location and header-only RS `ConsoleReport.h` were verified after discovery.

Local validation after writing the two documents:

- A focused Python check passed strict UTF-8/replacement-character checks,
  balanced fences, consistent tables and all six local Markdown links in these
  files. It checked 71 table rows and existence of 101 exact source paths in all
  ten repositories. The source inventory is inspection evidence, not test coverage.
- `git diff --check -- docs/prompts/CONVENTIONS.md docs/audit/CONVENTIONS_REAUDIT.md`
  returned zero; both files were new/untracked, so the focused content checker
  above provides the substantive document checks.
- `python scripts/check_promptset.py` checked 20 stages, 30 requirements, 20
  multi-drive gates, three reference packs and 103 hashed artifacts. Its first
  run failed only on seven links from concurrently edited stages 00-06 to the
  not-yet-created `DELEGATION.md`. Neither owned document produced an integrity
  error. The lead must rerun after integration; this is not a whole-tree pass.

No sibling suite,
remote CI, firmware build or hardware campaign is required for this source-backed
documentation task; none is claimed. Old RS quick-verifier evidence remains
pinned to its own earlier revision and is not fresh validation of current CAN.

Remaining implementation gates are explicit: stage 02 proves the chosen
CANopen stack/owner boundary and target type signatures; stage 03 creates and
executes CAN build/install/Doxygen checks; stage 04 implements chosen status/unit
semantics; stage 12 implements the selected example health/counter contract;
stages 14-17 exercise real shared application/CLI/runner behavior. The present
two documents provide concrete naming choices and acceptance criteria only.

## 7. Integrated closeout

After integration, DELEGATION and all shared contracts exist and the planning
checker passes with 20 concrete stages, 35 requirements, five shared contracts,
three reference packs and 103 hashed artifacts. The transient missing links
above were resolved. Independent review clarified service-counting units,
cancellation, health projection/reset lifetimes and the actual RS Python file
responsibilities in the maintained contracts. See [FINAL_REVIEW.md](FINAL_REVIEW.md)
for closeout and the subsequent concurrent source drift. Historical source
revisions and scoped checks above have not been relabelled as later inspections.
