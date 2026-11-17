# FieldCore-node RS485 handling: audit for a reference self-repair example (2026-11-17)

Read-only audit of `C:\Users\Honza\Documents\Projects\FieldCore-node`, branch `rodtester`, HEAD
`c40ea8341edf8d665460b902cafefbe857a7f73f`, working tree clean. Nothing was changed in either repository. Purpose: the
user asked for a reference solution in the examples, verbatim (2026-11-17): "an example can implement all of these as
a reference solution - take rs485 implementation from fieldcore-node library for how rs485 is handled and take the
relevant parts and implement them in here (audit the fieldcore code thoroughly and write new prompts)". "All of these"
is the policy the previous answer listed: no retry, no automatic fault clear, and uncertainty that lasts for the whole
session. The resulting assignment is [prompt 74](../prompts/74_reference_self_repair_example.md); the library-level
primitives stay in [prompt 55](../prompts/55_self_repair.md).

Sections 1 to 8 are observed code facts (paths relative to the FieldCore-node root). Section 9 and the adoption table
are recommendations.

**Scope.** `Rs485MotorModule` exists and is exercised by the host fixtures in `test/host/rodtester/*`, but no build
profile instantiates it at this HEAD (`docs/guidelines/rs485_devices.md`, "Typed motor ownership": "Neither current
product selects this module"). The production RS485 behavior comes from the two sensor modules, `ShzkDeviceModule` and
`VibWireDeviceModule`; the motor policy is implemented and host-tested but no product runs it.

## 1. Bus ownership and scheduling

- One `Rs485Task` (`include/TunnelMonitor/rs485/Rs485Task.h`, `src/rs485/Rs485Task.cpp`) owns one `Rs485Backend`
  (`ArduinoRs485Backend`: UART2 with DE/RE through `writeDirection()`, or `UART_MODE_RS485_HALF_DUPLEX` in HIL builds).
  At most one physical transaction exists globally (`ActiveTransport transport_`), one active command per module.
- `worker()` in `src/rs485/Rs485Diagnostics.cpp` is a static FreeRTOS task (8 KiB stack, priority 3, no core
  affinity). Each iteration: `serviceMotorControl` (two cancel intents, one stop, one reconcile), at most one ingress
  command (non-blocking `xQueueReceive`), `ownerPoll` → `Rs485Task::poll`, `finishOwnerCycle` (snapshots and results
  published under a mux), then `vTaskDelay(5 ms)` (`kWorkerPollIntervalMs`). The foreground never touches mutable
  owner state; it uses ingress reservations and published copies.
- Fixed arrays (`contracts/Rs485Devices.h`): request queue 8, result queue 8, trace 16, bindings 8. `popRecoveryWork`
  first, cancels jump the queue in `popNextWork`, otherwise FIFO; ready modules are served round-robin
  (`startReadyModuleTransaction`, `nextBindingIndex_`), an active urgent stop first.
- A motor has two lanes, `motor_[0]` ordinary and `motor_[1]` urgent; the urgent lane bypasses the queue, and
  `serviceMotorControl` interrupts the active transport and the ordinary lane before starting the stop.
- Half-duplex turnaround (`TransportPhase`): Setup (per-transaction baud, `clearRx`) → PreTxGuard (2 ms,
  `kRs485PreTxGuardMs`) → WriteAndFlush (one drain, `settleTransmit` → `uart_wait_tx_done`; a failed drain sets
  `txUnsettled_` and the bus Fault, cleared only by explicit recovery) → WaitResponse → InterRequestGap (20 ms,
  `kRs485InterRequestGapMs`).
- Times come from `RuntimeTiming::monotonicMs()`; the motor/RTU path also needs `Rs485Backend::monotonicUs` and fails
  closed without it.

## 2. Timeouts

- Per transaction: `Rs485BusTransaction::responseTimeoutMs` starts after the TX drain and is clamped to the command
  deadline (`processWriteAndFlush`); optional `interByteTimeoutMs` outside RTU, `frameGapUs` on RTU. Backend steps are
  capped at 600 ms (`kRs485MaxBackendStepWaitMs`), 32 ms on RTU (`kRs485RtuStepWaitMs`).
- Per command: `DeviceCommand::deadline`, default 35 s (`kRs485DefaultCommandDeadlineMs`), 40 s for the VibWire
  measurement. Retries must fit inside it; it is never extended.
- Per device, settable at runtime: SHZK 1000 ms and VibWire 1500 ms response timeouts at 9600 baud
  (`TunnelMonitorRuntimeSettings.h`); catalogue range 100..5000 ms (`TunnelMonitorSettingCatalog.h`, `UIntRange`),
  enforced again in `ShzkDeviceModule::bind`/`applyRuntimeConfig`. The motor host fixtures use 100 ms at 115200 baud
  with `frameGapUs = 1750`.
- Owner heartbeat stale after 10 s (`kRs485OwnerHeartbeatStaleMs`, `rs485RuntimeServiceHealth`).

## 3. Retries

- Sensors only (`ShzkDeviceModule::retryOrFinish`, `VibWireDeviceModule`): a response timeout with zero received bytes,
  or a parse failure (CRC, address, function, length, exception). Never a backend error, `DeadlineExpired` or a
  cancel.
- `retries` 0..3 (SHZK default 2, VibWire 0), backoff `75 ms << attempt` (`kRs485RetryBaseBackoffMs`: 75, 150,
  300 ms). The module parks in `RetryWait` through a `WaitUntil` step and does not hold the bus. A retry needs the
  command deadline not to have expired; configured retries used up give `Rs485RetryExhausted`.
- Motors: none. `Rs485MotorModule::applyRuntimeConfig` rejects `config.retries != 0` ("A motor never inherits the
  sensor automatic retry policy"). The owner itself only refuses `retries > 3U` in `stageInstanceConfig`; the
  module-level check is the motor guard.
- No resend after a rejected envelope: the module keeps the step and never resends it; `processWriteAndFlush` drains
  bytes already accepted but "never emits a new request or retries it".
- Idempotence is implicit: retries exist only in the read-only sensor modules. There is no write-retry class.

## 4. Device presence and health

- Module level (`ShzkDeviceModule::updateStatus`): one miss per command terminal after retries, and only for "no bytes
  at all" (`noByteAbsence`); a garbled reply sets `Degraded` without counting a miss. At `staleAfterMisses` misses:
  `Absent`/`Degraded` (`staleCount++` once); at `faultAfterMisses`: `Absent`/`Fault`. Profile thresholds 2 and 5
  (`TunnelMonitorBuildProfile.h`, `kRs485StaleAfterMisses{2U}`, `kRs485FaultAfterMisses{5U}`); `bind()` requires stale > 0
  and fault > stale. Between the two, a Measure returns `DeviceResultStatus::Stale` with `Rs485DeviceStale`.
- Return: one success sets `Present`/`Ok`, resets the counter and emits `RS485_DEVICE_RETURNED`
  (`Rs485Task::updateBusFromResult`). No latch and no re-initialization (the sensors are stateless).
- Probing: `PeriodicAcquisitionRuntime::startDeviceRefreshIfDue` sends a cheap `Probe`
  (`measurementLiveRefreshRequestKind`) at the configured poll interval (default 10 s, 1..3600 s) with a startup
  probe. There is no separate cadence and no fail-fast for an absent device: it is probed at the same rate.
- Application level (`core/HealthCoordinator.h`): `evaluatePresentHealthState` ages `lastSuccessUptimeMs` against a
  `DeviceHealthPolicy`; boot grace 30 s (5..120 s); each row has a `Required`/`Optional` role and a distinct
  `Disabled` state; `watchdogRequireRs485` optionally adds the owner to the watchdog quorum.
- Invalidation: explicit bus recovery calls `invalidateAllModules`; the motor loses its qualified state and
  axis/reference, `inhibitMotor()` runs and the target generation is bumped
  (`Rs485MotorModule::invalidateConfiguration`). Disable clears the cache; enable resets presence to `Unknown`.
  `Cancelled` touches neither health nor counters.
- Motor health has no miss counter: any failure `Degraded`, any success `Ok`/`Present` (`Rs485MotorModule::finish`).

## 5. Fault handling and motion inhibit

- No automatic or manual device alarm/fault clear exists in `src/` or `include/`.
- Recovery is local only. Owner re-initialization: `attemptInitialization`/`ownerPoll` call `begin` again on a fixed
  5000 ms due time after a backend failure, with no catch-up burst and no replay. Explicit bus recovery:
  `submitBusRecovery`, single-flight with its own retained `Rs485BusRecoveryResult`, triggered only by CLI
  `rs485 recover` (`Rs485Cli.cpp`): interrupt all work, one drain, `clearRx` or `begin`, invalidate modules. The
  guideline states: "Ordinary timeout, parser error, absence, and configured-retry exhaustion do not recreate or
  automatically recover the owner."
- Motion inhibit: `executionUnknown`, an unknown setup/trigger result, any stop admission and any configuration change
  call `inhibitMotor()` (increments `inhibitGeneration`). `Rs485Task::reconcileMotorState` clears it only when the
  generation matches, the matching `ReadState` succeeded, is qualified, shows `ConfirmedStopped`, was observed after
  `inhibitedAtUs_` and is younger than 500 ms, and no operation is queued or active. Historical `executionUnknown`
  stays in retained results; readiness is not requalified.
- The RodTester workflow (`RodCycleCoordinator.cpp`, Stopping and Fault phases) runs this automatically after a
  qualified stop: a fresh read, then `ports.reconcileMotor`. If admission fails it holds the fault for an operator
  instead of renewing its deadlines.

## 6. Uncertainty and stale data

- Per result (`DeviceMeasurement.h`): `completedUptimeMs`, `validMask`/`staleMask`/`errorMask`, status
  `Ok`/`Absent`/`Timeout`/`Stale`/`Cancelled`/`Failed`/`Disabled`. `ReadLast` serves the cache only after an `Ok`
  terminal (`lastTerminalWasOk_`); `copyStatus` still exposes cached values after a failure, so the application checks
  `lastStatus`, presence and the uptimes.
- Motor (`MotorResult`): `admission`/`wire`/`execution`, setup/trigger `MotorWriteEffect`, `executionUnknown`,
  `motionInhibited`. Uncertainty does not age away; it blocks moves until the reconciliation in section 5. It is not a
  session-long lock.

## 7. Configuration surface

- Compile time: a static binding table (`makeRs485DeviceBinding<T>`), capacity 8, at most one motor
  (`bindingMetadataValid`), matched to the profile `DeviceSpec` rows; `attachBindings` is rejected after `begin`, so no
  device is added at runtime. Thresholds, deadlines and capacities are `constexpr` profile facts.
- Runtime: `Rs485InstanceRuntimeConfig {address, baud, responseTimeoutMs, retries}` plus `enabled`
  (`DeviceEnableMode::RuntimeToggle`). `stageInstanceConfig` validates (retries at most 3, timeout nonzero, baud in the
  allowed set) and `applyPendingConfigs` applies atomically at a transaction boundary. An enable edit waits for the
  current transaction; a disable settles it first. A motor configuration change invalidates the qualified state and
  inhibits motion.
- Persistence: FRAM A/B slots with CRC and generation (`SettingsPersistence.h`, `TunnelMonitorSettingsCodec.cpp`); the
  catalogue declares type, range and `WritableFram` per key (`TunnelMonitorSettingCatalog.h`); CLI and web write
  through the settings service; the worker re-stages each cycle (`stageSettings`).
- Guards: ranges are checked twice (catalogue, then module). The motor's retry guard is a hard rejection, not a
  switch. HIL builds can override timing with `setHilTiming`.

## 8. Counters and diagnostics

- `Rs485BusMetrics`: state and ready flags; heartbeat, last-transfer and last-error uptimes; request/result/trace depths
  and high-water marks; submitted, completed, timeout, retryExhausted, queueOverflow, traceDropped and recovery
  start/ok/fail counts; `lastError`, `lastEventCode`.
- Per device (`Rs485DeviceStatus`/`DeviceModuleStatus`): success/error/timeout counts, `consecutiveMissCount`,
  `staleCount`, `parserErrorCount`, `lastParserError`, last attempt/success/error uptimes.
- Trace: a 16-entry `Rs485TraceRecord` ring with sequence number, dropped count and the first 4 TX/RX bytes.
- Events, rate-limited by `EventPolicy.h`: `RS485_TIMEOUT` and `PARSER_ERROR` at 60 s, `DEVICE_STALE` (error) at 60 s,
  `DEVICE_RETURNED` (info) at 10 s. Worker slow-operation tracking (at least 100 ms), stack high-water and duration in
  `recordDeviceOwnerMetric`. CLI: `rs485 status`, `rs485 recover`, `rs485 trace`, per-device probe/read.

## 9. Transferability (recommendation)

Worth transferring: an explicit Unknown/Present/Degraded/Absent/Fault state machine with validated thresholds; a miss
means no response, counted once per operation after its retries; one success restores presence; retries only for
reads, inside the original deadline, with backoff and a distinct "exhausted" code; zero motor retries; configuration
staged and applied at a boundary; local re-initialization on a fixed due time with no replay; explicit single-flight
recovery with its own result; device health separate from application health, with boot grace; bounded counters and
traces with drop counts; an urgent-stop lane outside the queue; generation-matched reconciliation of uncertainty with
fresh standstill evidence.

Not transferable to CAN: DE/RE, pre-TX guard and inter-request gap; TX drain settlement and `txUnsettled_`; echo
stripping; RTU frame-gap closure; per-transaction baud switching; one global transaction with round-robin transmit.
CAN has arbitration and hardware ACK, concurrent SDO channels per node and asynchronous heartbeat, PDO and EMCY
traffic. FieldCore has no fail-fast for offline devices, no slow probe cadence and no fault clear, so those parts of
the design have no FieldCore precedent.

## Recommended adoption for MotorControl-CAN

| Mechanism | Disposition | How |
| --- | --- | --- |
| Node presence | Adopt | Unknown → Online → Degraded → Offline, plus Disabled. A miss is one SDO operation ending without a response, counted after its retries; an SDO abort, a wrong reply or an EMCY is "present but erroring", not a miss. Thresholds 2 and 5, settable, validated (degraded > 0, offline > degraded). One response, heartbeat or boot-up sets Online, resets the counter and emits a returned event. Presence never reopens a route and never lifts a motion inhibit. |
| Offline handling | Adapt (beyond FieldCore) | Fail fast: new non-stop requests to an Offline node are refused as unavailable. A settable probe interval for Offline nodes, one cheap reviewed read (for example `1000`) or heartbeat/boot-up observation. Never an NMT reset on loss. |
| Timeouts | Adapt | A per-node response timeout with a validated range, plus a per-operation deadline that retries never extend. The default comes from measured TWAI/SDO latency, not FieldCore's 1000 ms (a 9600-baud RS485 value). |
| Retry classes | Adopt the structure | R: reviewed non-consuming reads, 0..3 retries (default 0 for drive-state reads), backoff from a settable base, only on no response or a malformed reply, never on deadline expiry or a local controller error, with an "exhausted" code. W: idempotent absolute settings with readback, off by default. M: motion start, enable, homing, save and NMT, never retried, enforced by policy validation as `Rs485MotorModule::applyRuntimeConfig` does. A quick stop may be repeated, each repeat a new, separately identified attempt. |
| Uncertainty | Adopt FieldCore's reconciliation | An unknown execution or a stop admission inhibits motion under a generation counter; the inhibit lifts on a matching generation, a fresh qualified standstill read observed after the inhibit, an age bound (FieldCore: 500 ms) and nothing in flight. An opt-in automatic stop → read → reconcile sequence as in `RodCycleCoordinator`. Retained results keep their history. |
| Controller recovery | Adapt | Re-initialize on a fixed due time, rate-limited, no catch-up and no replay, for TWAI bus-off or a frame without TX-done; explicit operator recovery single-flight with its own result; recovery invalidates bindings and motion evidence. |
| Fault clear | New | Example only, off by default; only while released or idle; limited per window and per boot; never enables and never retries motion. |
| Health reporting | Adopt | Per-node status: presence, health, success/error/timeout/miss/stale counts, last attempt/success/error times, last error; bus metrics with depths, high-water marks, overflow and drop counts; a bounded trace ring with sequence numbers; rate-limited state-change events; application-level age thresholds with boot grace and Required/Optional roles. |
| Configuration | Adopt | Caller-owned node slots and a `NodePolicy` (response timeout, read retries, retry backoff, degraded and offline thresholds, probe interval, quick-stop repeat, automatic reconcile, automatic fault clear off, fault-clear limit); `validateNodePolicy()` and staging at an idle boundary; a change that affects interpretation invalidates evidence. Persistence and console setters live in the example, not in the portable core. |
| Half-duplex mechanics | Reject | Section 9. |

Key paths: `src/rs485/Rs485Task.cpp`, `src/rs485/ShzkDeviceModule.cpp`, `src/devices/rs485_motor/Rs485MotorModule.cpp`,
`src/rs485/Rs485Diagnostics.cpp`, `include/TunnelMonitor/core/HealthCoordinator.h`,
`src/measurement/rodtester/RodCycleCoordinator.cpp`, `docs/guidelines/rs485_devices.md`.
