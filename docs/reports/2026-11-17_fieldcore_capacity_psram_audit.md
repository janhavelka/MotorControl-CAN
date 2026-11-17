# FieldCore-node capacity, PSRAM, stacks and console: audit for MotorControl-CAN (2026-11-17)

Read-only audit of `C:\Users\Honza\Documents\Projects\FieldCore-node`, branch `rodtester` (upstream
`origin/rodtester`, not fetched), HEAD `c40ea8341edf8d665460b902cafefbe857a7f73f` (2026-11-13), working tree clean.
Since `5ad9292d`, the commit the [capacity study](2026-11-12_capacity_configuration.md) recorded, only `library.json`
metadata changed (`git diff --stat 5ad9292d HEAD`), so its code seams are unchanged. Nothing was changed in either
repository.

The user's requests, verbatim (2026-11-17): "you are saying these are compile time things and that bench tests are
kind of clumsy and not flexible to add another dive on the fly or monitor more drives?" and "increase if the mcu allows
for it - also i want to design around using psram when the mcu allows for it (add promptsets, get inspired with
fieldcore-node)". The assignments are [prompt 75](../prompts/75_runtime_node_table.md) (runtime node table, monitoring,
bench flexibility) and [prompt 76](../prompts/76_psram_storage_placement.md) (PSRAM-aware placement and stacks);
[prompt 67](../prompts/67_s3_psram_qualification.md) keeps the S3 board qualification.

Sections 1 to 6 are observed facts (paths relative to the FieldCore-node root). Section 7 and the adoption table are
recommendations.

**Scope.** Every FieldCore firmware environment targets the ESP32-S3-N16R8 with 8 MB OPI PSRAM
(`boards/esp32-s3-wroom-n16r8.json`, `platformio.profiles.generated.ini`: `tunnelmonitor_wifi`, `co2control_wifi` and
their `_hil` variants). There is no ESP32-S2 or no-PSRAM firmware build; the native host builds set
`TUNNELMONITOR_REQUIRE_PSRAM=0` and use `std::malloc`.

## 1. PSRAM use

- Configuration: no custom sdkconfig. PSRAM comes from the pioarduino prebuilt Arduino core through the board settings
  `"memory_type": "qio_opi"`, `"psram_type": "opi"` and `-DBOARD_HAS_PSRAM`.
- Detection is heap-based only: `System::captureMemorySnapshot` (`src/system/SystemRuntime.cpp`) calls
  `heap_caps_get_total_size(MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT)`; there are no `esp_psram_*` or `psramFound` calls. It
  is required when the build flag `TUNNELMONITOR_REQUIRE_PSRAM` is set or `Board::kExpectedPsramMb > 0` (8 in every
  board header, for example `include/TunnelMonitor/board/tunnelmonitor/TunnelMonitorS3Hw210Board.h`);
  `psramRequirementOk` needs at least `kExpectedPsramMb` MiB (`expectedPsramBytes()`).
- Allocator: `include/TunnelMonitor/core/PsramAllocator.h` (`psramAllocate`, `psramAllocateZeroed`, `psramFree`) uses
  `heap_caps_malloc`/`heap_caps_calloc` with `MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT` on S3 and `std::malloc` elsewhere. It
  never falls back to internal RAM. Callers use placement new and an explicit destructor plus `psramFree` (for example
  `StorageTask::~StorageTask`).
- In PSRAM (about 40 call sites): the 8192-record volatile record queue (`kStorageQueueCapacityRecords`,
  `contracts/Capacities.h`) and its controls (`StorageTask::ensureOwnerBuffersAllocated`); replay and event upload
  stores, `StorageTrace` and `StorageDiagnosticBuffers` rings; the FRAM working records `RecordBuffers`, 6528 bytes
  (`SettingsPersistenceService::allocateRecordsAndStartLoad`); RS485 `RuntimeBuffers`
  (`src/rs485/Rs485Diagnostics.cpp`, `ensureRuntimeBuffersAllocated`), whose `IngressState` holds `motor_[2]` including
  the urgent-stop tracked-result slot (`src/rs485/Rs485RuntimeIngress.h`); I2C and CPU diagnostics (`TaskStatus_t[64]`),
  health scratch and the transition trace; the CLI async serial payload (5248 bytes), its 32-entry pending tracker and
  `CliSnapshotScratch`; web JSON documents, route buffers and cloud body buffers.
- Internal by rule (`docs/guidelines/decisions.md`, "Buffer placement"): owner queues, active transaction state,
  DMA/backend staging, synchronization objects and small hot data; "PSRAM remains non-default". In code: all task
  stacks and TCBs are static internal `.bss` arrays; `Rs485Task::requestQueue_`, `resultQueue_` and `rxBuffer_` are
  static owner arrays; FreeRTOS queue storage (`g_commandQueueStorage`) and mutexes are static; the display framebuffer
  is a static 1024-byte buffer. `disableFrameworkLogOutput` creates the log mutex early, "before worker/network
  allocations can exhaust internal RAM". No explicit `MALLOC_CAP_DMA` or `INTERNAL` allocation exists: internal
  placement is by static storage.
- Allocation timing: boot preallocation in `App::setup` order (`src/app/App.cpp`: serial transport, health scratch,
  storage, web, cloud, settings); lazy first use with attempt-once flags (`allocationAttempted_`) for CLI and CPU
  diagnostic buffers; transient allocate/free per transfer (`src/storage/StorageTaskTransfer.cpp`). Only the settings
  record buffers retry, every 5000 ms (`kOwnerInitializationRetryIntervalMs`).
- Fallback is per call site: fail closed (`StorageTask::begin` records `PSRAM_UNAVAILABLE` and enters `StorageFault`;
  settings enter `PersistenceUnavailable` with `ErrorCode::PsramUnavailable`; `rs485RuntimeBegin` returns false) or
  fail soft (the CLI pending tracker drops to zero capacity; `status`/`health` use a bounded internal 1336-byte object
  and print "Device detail unavailable: no scratch"; async serial output becomes unavailable while synchronous commands
  work). A capacity ladder in `attachStorageWorkerQueue` (`src/storage/StorageDiagnostics.cpp`, through
  `StorageTask::attachFirstAvailableQueue`) tries 8192, 4096, 2048, 1024, 512, 256 and 128 records and publishes the
  capacity obtained as `StorageStatus::psramQueueCapacity`. Native tests inject a failure at each site
  (`g_failNextRecordAllocation`, `storageTaskFailNextRecordQueueControlAllocationForTest`).
- Missing PSRAM on a board causes a deliberate watchdog reset loop: `WatchdogRuntime::bootGateForStatus` returns
  `RequiredPsramFault`, `endLoop` blocks the feed and the task watchdog (`kWatchdogTaskTimeoutMs` 15000,
  `trigger_panic=true`) resets; the OTA self-check fails too (`OtaService.cpp`, `inputs.psramOk`).
- Reporting: CLI `memory` (`CliSystem.cpp` `printMemory`) shows internal, 8-bit and PSRAM total, free, boot-low free,
  largest block and its low, PSRAM detected/required/requirement, each worker stack (configured and minimum free) and
  the board's expected PSRAM. Web exposes `psram_free_b`/`psram_free_min_b`. HIL builds record per-boot-phase heap
  checkpoints (`TM_RECORD_HIL_BOOT_HEAP`) and a `heap_caps_register_failed_alloc_callback` ring (size, caps, task,
  8-frame backtrace). There is no per-block placement report. Measured on hardware (COM11, 2026-09-13, Wi-Fi on):
  internal free about 63 to 64 KB, PSRAM free about 2.70 MB of 8 MB.

## 2. Device capacity and runtime configuration

- The inventory is compile-time by explicit policy (`docs/guidelines/compile_time_product_extension.md`: products are
  extended "without adding a runtime registry"; `docs/guidelines/rs485_devices.md`: "There is no dynamic registry,
  heap/virtual module interface, raw transaction API, protocol fallback, or runtime-defined device inventory"). Each
  product builds fixed modules and a const binding span (`g_rs485Bindings`,
  `src/product/tunnelmonitor/TunnelMonitorRuntimeComposition.cpp`), selected by one `#elif` in
  `include/TunnelMonitor/product/SelectedBuildProfile.h`.
- Maxima (`contracts/Rs485Devices.h`, `contracts/Capacities.h`): `kRs485BindingCapacity` 8, RS485 request and result
  queues 8, `kRs485TraceCapacity` 16, `kMaxBuildProfileDeviceSpecs` 16, `kMaxDeviceHealthEntries` 16. Actual
  capacities derive from the profile with `static_assert` (`PeriodicAcquisitionRuntime.cpp`,
  `kSelectedAcquiredDeviceCapacity`; health-row asserts in `SystemRuntime.cpp`). Per-device owner state is
  `Rs485Task::bindingRuntime_[kRs485BindingCapacity]`.
- Runtime changes: per instance `Rs485InstanceRuntimeConfig {address, baud, responseTimeoutMs, retries}` and enable
  (`setEnabled`, for `DeviceEnableMode::RuntimeToggle`), through CLI `settings set|get|list|save`
  (`src/cli/CliSettings.cpp`, `runSettingsCommand`) or the web, staged by
  `TunnelMonitorRuntimeComposition::stageRs485RuntimeSettings`. A disabled binding is "valid, visible and bus-silent".
  "A runtime update cannot change kind, binding count, reading inventory, schema, framing, pins, guards, or
  capacities." Adding or removing a device at runtime is impossible by design.

## 3. Tasks, stacks and watchdog

All workers use `xTaskCreateStaticPinnedToCore` with static internal stacks sized in bytes (`src/system/StackDiagnostics.h`
notes that `uxTaskGetStackHighWaterMark` reports bytes on ESP-IDF).

| Task | Stack (bytes) | Priority | Core | Source |
| --- | ---: | ---: | --- | --- |
| Arduino loop (App, CLI, measurement) | 20480 | Arduino default | 1 | `src/main.cpp` `SET_LOOP_TASK_STACK_SIZE(20*1024)` |
| StorageTask | 24576 | 1 | any | `StorageDiagnostics.cpp` |
| I2cTask | 12288 | 2 | any | `I2cDiagnostics.cpp` |
| Rs485Task | 8192 | 3 (HIL 4) | any (HIL core 1) | `Rs485Diagnostics.cpp` |
| CloudTask | 16384 (HIL 24576) | 1 | any | `cloud/CloudRuntime.h` |
| WifiTask | 8192 | 2 | 0 | `WifiRuntime.cpp` |
| e2_owner | 8192 | 2 | any | `E2Config.h` |
| esp_http_server | 12288 | 1 | SDK | `WebPortalRuntime.cpp` |

Sizes are fixed in code, not derived ("measured HIL margins do not qualify future workloads", `ownership.md`). High-water
marks are published (`workerStackConfiguredBytes`, `workerStackFreeMinBytes`) and printed by CLI `memory`; one
numeric rule exists (`validation_plan.md`: below 4096 bytes free on the cloud worker, recover margin). Measured minimum
free on COM11: loop 9248, WiFi 5044, Cloud 9780, Storage 9275, I2C 7352, E2 5520 bytes. Watchdog: ESP-IDF task
watchdog 15000 ms with `trigger_panic=true`; only the loop task is subscribed, fed from one end-of-loop decision with a
`MainLoop` (stale after 2000 ms) and `Supervisor` quorum; workers publish heartbeats but are not subscribed.

## 4. Console

The console uses the S3's USB Serial/JTAG peripheral, not a UART: `src/cli/SerialCli.cpp` raises `#error` unless
`ARDUINO_USB_MODE == 1`; `configureSerialTransport` installs `usb_serial_jtag_driver_install` with 256-byte RX and TX
rings, after masking interrupts inherited across a CPU reset. `monitor_speed = 115200` is nominal (inferred: USB
ignores it). Bounds (`src/cli/SerialCli.h`): 192-byte line (`kCliLineCapacity`); 256 bytes read and one command
handled per poll; non-blocking 64-byte writes; a 1000 ms deadline per response; an end marker
`TM_CLI_RESPONSE_END`; a 4224-byte result budget (measured maximum 4217); 5248-byte async staging in PSRAM. An overflow
discards the whole publication and counts it (`asyncOutputOverflowCount`); incomplete responses are counted;
framework logs are discarded (`esp_log_set_vprintf(discardFrameworkLog)`, `CORE_DEBUG_LEVEL=0`).

## 5. Settings persistence

Core settings live in FRAM (MB85RC at 0x50; `docs/guidelines/settings.md`, `include/TunnelMonitor/contracts/RtcFram.h`):
grouped A/B records with CRC32, 1024-byte slots (A at 0x0000, B at 0x0800), magic `TMC8`, record 3 / payload 11 /
schema 4, a 36-byte header, little-endian images, zero tail required, all-0 or all-0xFF meaning missing; settings,
app-state and runtime-state groups are separate. `ProfileSettingsCodec`
(`include/TunnelMonitor/settings/ProfileSettingsCodec.h`) walks a field table with a `constexpr layoutValid` check.
Credentials are in NVS (`src/wifi/EspNvsWifiCredentialStore.cpp`). Rules: safe defaults; validate or clamp at the
boundary (`SettingsApplyStatus::AppliedClamped`, `Rejected*`); corrupt or missing records load defaults
(`SettingsPersistenceState::DefaultsActive`); "Bad settings must not boot-loop"; explicit `settings save`;
`applyGeneration`; SD backup/restore. Settings load after the early I2C owners, and settings-dependent owners start
later (`beginSettingsDependentDeviceOwners`).

## 6. Monitoring of many devices

RS485 is master/slave polling: one owner per bus, one transaction at a time, a 20 ms inter-request gap and a 2 ms
pre-TX guard. `MeasurementRuntime` owns per-device poll settings (for example `rs485.shzk.poll_interval_s`), live
refresh bounded separately from the four recorded-cycle slots, and optional-device polling every 5000 ms with a
15000 ms stale threshold (`TunnelMonitorDeviceCadence.h`). Status is copied under `portMUX`; "A status read is not an
advancing poll". Bus time grows with the sum of per-device transactions; there is no unsolicited traffic, unlike CAN
heartbeat, PDO and EMCY.

## 7. Patterns (recommendation)

Worth adopting: explicit placement decided at each call site; boot preallocation before network or owner start;
attempt-once flags; a declared fail-closed or fail-soft outcome per site; a capacity ladder that reports what it got;
heap low-water and largest-block reporting; static stacks with high-water reporting; bounded console publications;
A/B CRC records with versions; runtime per-instance configuration inside compile-time capacity; "disabled = visible
and silent". To avoid: a hard PSRAM requirement enforced by a watchdog reset loop; an allocator with no internal
fallback; active command and urgent-stop tracking in PSRAM; transient PSRAM allocation inside operations; a policy that
forbids any runtime inventory, which is the user's complaint about the bench.

## Recommended adoption for MotorControl-CAN

| Topic | Disposition | Proposal |
| --- | --- | --- |
| Runtime node table | Adapt | Keep `MOTORCONTROLCAN_SDO_CHANNELS` (and the bench's node maximum) as the storage bound; add a runtime table of active entries (node ID, compiled-in profile, role monitor-only or controlled, enabled). Unlike FieldCore, allow adding, removing or rebinding a slot at runtime, but only while that slot is idle (no admitted work, no unconfirmed write, no motion uncertainty); removal or rebinding invalidates that node's evidence and queued work; adding a node sends no NMT, enables nothing and moves nothing. "Disabled = visible, bus-silent" is a slot state. |
| Monitor-only drives | Adopt the idea, adapt to CAN | Passive heartbeat, EMCY and TPDO observation costs little per node; SDO polling is bounded by host TX turns and bus time (`adapters/canopennode/Capacity.h`). Separate the observation period from the stale threshold, both settable with defaults and a floor. |
| PSRAM cold storage | Keep MotorControl-CAN's | Keep `examples/common/StartupColdStorage.h` (external attempt, single internal fallback, reported placement), which already reports placement per block; reject FieldCore's no-fallback allocator and hard requirement; add a per-site native failure seam. |
| Capacity ladder | Adapt narrowly | Only for cold history depth (traces, record rings), publishing the capacity obtained; for the node count, refuse "requested N, fits M" before work is admitted, never shrink silently. |
| Allocation timing | Adopt | Allocate once in a fixed startup order before the TWAI driver and owner start; lazy allocation only for console or diagnostic scratch; no transient PSRAM allocation in the control path. (Superseded 2026-11-17, audit A18: [prompt 76](../prompts/76_psram_storage_placement.md) preallocates console and diagnostic scratch at startup too.) |
| Hot data | Adopt FieldCore's rule, reject its exception | Owner, SDO channels, TWAI ISR and callback storage, stop and urgent reservations, stacks and TCBs stay internal; do not copy FieldCore's urgent-stop tracking in PSRAM. (Extended 2026-11-17, audit A05: every live `AxisSession` and active operation context stays internal too.) |
| Memory report | Adopt | A console `memory` command: internal, 8-bit and PSRAM total, free, low-water and largest block, each task's configured stack and minimum free, the placement of every block; per-phase boot heap checkpoints; a bench-only failed-allocation ring. |
| Stacks | Adopt | Static internal stacks in bytes with high-water reporting and a numeric margin rule (for example at least 4096 bytes free). Size larger stacks on S3 only after measuring the chain; never put stacks in PSRAM. |
| Console | Adapt | Bounded publications (per-response byte budget, whole-publication discard with a counter (narrowed 2026-11-17, audit A11: diagnostics and refusals of unadmitted commands only, [prompt 75](../prompts/75_runtime_node_table.md)), non-blocking chunked writes, end marker, framework logs suppressed, one command per poll). On S3 use USB Serial/JTAG, where baud does not limit. The ESP32-S2 has no USB Serial/JTAG peripheral (USB OTG/TinyUSB CDC only); there, raise the UART rate only after measuring the USB bridge (for example 921600). |
| Settings storage | Adapt, examples only | A/B CRC records in NVS with magic, record/schema versions, zero tail and erased-means-missing; defaults and "needs provisioning" on corruption, never a boot loop; explicit save; persist the node table, expected bitrate and guards; at boot load the table but start no NMT, enable or motion. Reject FieldCore's catalogue/web/backup machinery as too heavy for a library example. |
| Settable guards | Adopt | Validate or clamp at the boundary and report applied, clamped or rejected, with safe defaults; guards optional and user-settable, expert settings behind an explicit flag. |
| Board facts | Adapt | Keep the existing CMake/Kconfig/PlatformIO capacity macros; a per-board facts header (expected PSRAM size and type) for the S3 examples, compared with `heap_caps_get_total_size`; reject the generated-profile toolchain. |
| Missing PSRAM | Reject FieldCore's watchdog loop | Report the shortfall and run internally at the count that fits; any watchdog belongs to the application example, with a central feed decision. |

Key paths: `include/TunnelMonitor/core/PsramAllocator.h`, `src/system/SystemRuntime.cpp`, `src/storage/StorageTask.cpp`,
`src/storage/StorageDiagnostics.cpp`, `src/cli/SerialCli.cpp`, `src/cli/SerialCli.h`, `src/rs485/Rs485Diagnostics.cpp`,
`include/TunnelMonitor/rs485/Rs485DeviceBinding.h`, `src/system/WatchdogRuntime.cpp`,
`src/settings/SettingsPersistenceService.cpp`, `docs/guidelines/decisions.md`, `docs/guidelines/settings.md`,
`docs/guidelines/rs485_devices.md`.
