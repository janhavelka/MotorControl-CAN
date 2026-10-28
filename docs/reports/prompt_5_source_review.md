# Prompt 5 live owner and controller source review

2026-10-28. Read-only source review supporting the narrow CANopen fit experiment.
The lead owns the service choice, integrated checks and Git changes on
`promptset/motorcontrol-can`. This report adds no production owner, SDK adapter,
hardware result or supported drive. Sources below are inspected implementations,
not evidence that their tests passed during this review.

## Source refresh and scope

All sibling paths are relative to `C:/Users/Honza/Documents/Projects`.
`git rev-parse HEAD` and `git status --short` were read before and after inspection.

| Checkout | Local HEAD | Before / after dirty state |
| --- | --- | --- |
| `MotorControl-RS` | `bd831c859daa9c8c07b8d27a2dd879bb4e750511` | clean / clean |
| `ADS1115` | `235af3e8c722661a89a44766a4ccd903a654f6fe` | clean / clean |
| `INA228` | `b28c5ea69bcb4d26b351c52c24d3c6fae9fd282b` | clean / clean |
| `FieldCore-node` | `5ad9292dc8481cf08be9a61f662495a6474d1da0` | clean / clean |

These match the prompt-4 report. The delta is a refreshed transport, callback,
deadline and retained-result inspection, plus actual installed TWAI source.
No sibling fetch, edit, suite, serial operation or hardware access ran. Other
I2C modules have no newly affected seam in this bounded service-choice audit.

Read the parent and target AGENTS, prompt README/COMMON, prompt 5,
CONVENTIONS section 4, DOCUMENTATION, MAINTAINABILITY, multi-drive decision,
prompt-4 report and the relevant conventions audit. The production owner and
common motion contracts remain assigned to later prompts.

## Observed sibling behavior and deliberate CAN boundary

| Source inspected at the HEAD above | Observed implementation and caller/test evidence | Consequence for CAN |
| --- | --- | --- |
| `MotorControl-RS/examples/common/RtuBusOwner.{h,cpp}` | `admit` validates before publishing the ID, copies request bytes and expectation values, reserves a result slot, and copies retained RX bytes at collection. Caller-owned arrays hold pending/result/producer storage; ordinary and urgent capacities are separate quotas. `result(id)` is non-consuming; `release(id)` only releases a matching terminal. IDs include owner identity, slot and non-wrapping generation. `service` polls once, bounds receive work, validates a frame before another dispatch and selects eligible producer heads. | Reuse these meanings and names for the application owner. A local ID remains host ownership identity and cannot correlate an indistinguishable SDO reply. Do not copy RTU byte timing, echo or DE/RE fields. |
| `MotorControl-RS/examples/common/RtuRunner.h`, `src/profiles/ess_rs/Actions.cpp`, `include/MotorControlRS/ActionOperation.h` | Port callbacks are bounded/nonblocking; one owner controls the runner, port and buffers. `Request.bytes` is copied on start. Acceptance, physical TX completion, checked reply and observed drive state remain separate. `prepareAction` builds a candidate before assignment; `nextAction` yields work/wait/terminal view; absolute microsecond deadlines and supplied time bound observation. `ActionEvent` borrows response storage for the advance call only. `cancelActive` avoids starting an unsent transaction merely to cancel it; accepted bytes without checked response preserve unknown execution. Recovery settles transport and does not replay work. | Keep admitted input/results owned, operation evidence concrete, and time caller-supplied. Cancellation cannot mean motor stop. An SDK send return cannot replace SDO confirmation. |
| `MotorControl-RS/examples/probe_cli/ProbeApp.cpp`, `test/bus_owner_test.cpp`, `test/actions_test.cpp` | The real app admits through `a.owner`, reads completion at lines 1595/1837, and explicitly releases at 1649/1880; its loop calls `owner.service` at 2590. `testCapacitiesAndIds` checks held-result pressure, stale/foreign IDs and unchanged rejected ID; `testTransientAndReadShapes` overwrites the caller request after admission and checks original transmitted bytes plus retained raw response. Deadline/clock/classification tests inspect delayed evidence. | Use a single ordinary public owner path in the proof; preserve terminal data across unrelated service turns and test caller-buffer mutation. |
| `ADS1115/include/ADS1115/Config.h`, `src/ADS1115.cpp`, `examples/02_owner_safe_poll/main.cpp`, `test/test_basic.cpp` | Callback buffers live for the call; callbacks finish synchronously, may not re-enter, and must include lock plus transfer in `timeoutMs`. The example subtracts lock time and pointer-write time before the read. `poll` uses supplied `nowMs` and transaction budget; callback timeout is clamped to remaining deadline. `_beginOperation` refuses an unread result. `takeResult` copies only a matching terminal then consumes it. Cancellation can require a bus-silent conversion reconciliation interval; it does not undo a prior write. Tests at 4603/4629 check deadline clamping and budget partition, at 4833/4859 cancellation before/after effect, and at 6432 terminal retention through the wrong polling facade. | Reuse explicit callback lifetime, budget and effect accounting. CAN's selected result/release contract deliberately follows RS rather than the sensor's consuming `takeResult`. Sensor conversion waits and recovery/profile replay are not motor retry or recovery policy. |
| `INA228/include/INA228/{Config,INA228}.h`, `src/INA228.cpp`, `examples/esp_idf/basic/main/Ina228IdfI2cTransport.cpp`, `test/test_basic.cpp` | App transport owns handles/locking/recovery. `_i2cWriteRaw` and `_i2cWriteReadRaw` forward the configured per-transfer timeout and application context to synchronous callbacks. The actual IDF callback uses `i2c_master_transmit`/`i2c_master_transmit_receive` and preserves the SDK error code. `pollJob` bounds transfers, forbids re-entry and uses supplied time for waits; `timeoutJob` is explicit owner expiry, not ADS-style admission of an absolute job deadline. `_finishJob` copies a terminal result; `takeJobResult` validates operation identity before consuming. `cancelJob`/`timeoutJob` are bus-silent but retain PARTIAL/INDETERMINATE effects and resynchronization need. Tests at 3489, 4156 and 5009 inspect budgets, cancellation/expiry effects and zero-budget waits. | There is no universal sibling lifecycle or deadline API to copy. Keep CAN service deadline enforcement explicit in its one owner; do not silently imply INA228 clamps every callback to an absolute job deadline. Retained service effects and communication health remain separate. |
| `FieldCore-node/include/TunnelMonitor/contracts/MotorControl.h`, `src/devices/rs485_motor/Rs485MotorModule.cpp`, `test/host/rodtester/{motor_module,motor_runtime}.cpp` | Commands freeze target/configuration generations and original deadline. The module yields a transaction, retains its pending identity, consumes a matching owner observation through `advanceMove`/`advanceAction`/`advanceRead`, and retains accepted-prefix evidence even when an observation envelope is rejected. `cancelConservesPrefixAndIndependentSettlementError` preserves 3 accepted bytes, a separate physical settlement error, unknown write effect and motion inhibition after cancellation; it asserts no second transaction. Common contract fields distinguish wire settlement, write effect, execution and stopped-state evidence. | Prospective CAN integration must expose equivalent evidence without importing RS address, RPM widths or FieldCore types into core. No FieldCore CAN integration is claimed or changed. |

Workflow/runner registration was inspected, not executed: RS
`.github/workflows/verify.yml`, `scripts/verify.py` and `CMakeLists.txt` register
native CTest owners/actions and source/install verification; Arduino is pinned to
pioarduino `55.03.311`, IDF CI to `v5.5.5`. ADS1115/INA228
`.github/workflows/ci.yml` run PlatformIO `6.1.19` native/firmware and Doxygen;
their `tools/run_i2c_hil.py` consume bounded CLI observations rather than proving
analog/device behavior from a fake. FieldCore's verify workflow delegates its
gate list to `scripts/verify.py`, including the motor host runner. None of these
commands is an implementation or hardware gate for this CAN proof.

The INA228 native-IDF example's real `pollOwnerJob` checks the active operation
identity before consuming its terminal. `runOwnerJob` supplies a one-transfer
budget and explicitly calls `timeoutJob` at its application deadline, then takes
that same terminal (`examples/esp_idf/basic/main/main.cpp:556`). The ADS1115
owner-safe example calls `poll(nowMs, 1)` and consumes the active token's result
only after `done` (`examples/02_owner_safe_poll/main.cpp:321`). These are different
existing caller contracts, not interchangeable common method names.

## Actual installed controller API: source facts

The candidate examined is the installed legacy/handle TWAI driver, not a proposed
board or a claim about a future different driver. `C:/pio/packages/framework-espidf/package.json`
reports package `3.50505`; `components/esp_common/include/esp_idf_version.h`
reports ESP-IDF **5.5.5**. The installed Arduino S3 library headers report the
same macros; `framework-arduinoespressif32-libs/package.json` reports
`5.5.5+sha.b774170ff46`. Package metadata is not a verified upstream Git revision.

Concrete source paths beneath `C:/pio/packages/framework-espidf/`:

| Source / line | Source-backed finding |
| --- | --- |
| `components/driver/twai/include/driver/twai.h:278` and `twai.c:708` | `twai_transmit_v2` accepts/initiates transmission, not successful delivery. It builds a local HAL frame; idle path immediately hands it to hardware before returning. The queue path copies that frame with `xQueueSend`. Caller payload need not remain borrowed after this synchronous call. `ticks_to_wait=0` avoids waiting for software queue capacity. |
| `components/driver/twai/twai.c:949` | `twai_clear_transmit_queue_v2` clears the whole software queue, with `tx_msg_count` left at one when the hardware buffer is occupied. It does not revoke an in-flight hardware frame and returns no per-request removal evidence. Header prose saying the transmit queue is cleared must not be expanded into a claim that nothing can subsequently transmit. |
| `components/driver/twai/include/driver/twai.h:245`, `twai.c:677` | `twai_stop_v2` stops all receive/transmit and clears software TX; its documented immediate interruption of an in-flight frame can cause another node to see an error. This is an interface-wide disruptive action, not selective request cancellation or a motor stop. It cannot undo already transmitted bytes or device effects. |
| `components/driver/twai/twai.c:185`, `twai.c:122`, `twai.c:842` | TX success/failure updates alert bits; bits accumulate by OR and are cleared when read. The interface has no request token or ordered per-frame completion record. TX alerts are controller evidence only; a later SDO result has its own confirmation semantics. |
| `components/driver/twai/twai.c:746` | `message->ss` selects zero HAL retries; the alternative configures indefinite controller retries. CAN link retransmission must be explicitly distinguished from replay of a completed/uncertain SDO request. This review selects neither setting as a drive guarantee. |

SHA256 identities for rechecking the actual installed bytes:

```text
components/driver/twai/include/driver/twai.h
d7982784094ea9f1f3e04d1fb643ce66ae2c267a7bc3ec2bb872e12d568f0768
components/driver/twai/twai.c
7680a0ef354cd67dc1add8c636675482f7f7c39123d13597f4a4ced3756d96bc
components/esp_common/include/esp_idf_version.h
374e0785047d9f656362eb4c750d936439dd871dbb38261570400dc9b3ad57c9
```

## Actionable adapter obligations and unresolved premises

The following are engineering consequences, not observed CAN bench behavior:

1. Keep frames in an owner-controlled queue until dispatch; owner cancellation
   can definitively remove those unsent frames. Once TWAI reports acceptance,
   conservatively retain possible transmission/effect unless stronger evidence
   settles that exact frame. Clearing only the software queue cannot prove it.
2. A zero-length SDK TX queue and at most one outstanding controller frame can
   simplify association of aggregate alerts. It does not supply a wire transaction
   ID, prove never-sent on cancellation, or distinguish delayed identical SDO
   replies. Exact alert/bus-off races and link retry policy need the real adapter's
   own tests and qualification in later stages.
3. One owner must receive all frames and deliver passive events during SDO waits,
   cancellation and reconciliation. A global stop/restart is not an ordinary
   cancel path because it interrupts unrelated receive traffic.
4. Host session generation rejects stale local work. Reusing an SDO channel after
   uncertain transfer still needs an explicit evidenced reconciliation/retirement
   boundary; a quiet interval or new local token alone does not identify a reply.
5. The chosen service engine must accept caller time, own admitted payload/session
   data, use bounded queues, publish terminal evidence before session reuse and
   avoid automatic SDO replay. The lead's pinned candidate review and executable
   establish that engine's actual properties; this sibling report cannot do so.

No source-confirmed controller fact above establishes CL86-C timing, firmware,
drive acceptance or physical completion. No device-facing change or physical
measurement was made. Production controller selection, fixture facts and actual
SDK/ISR storage/timing remain later adapter/bench work, not blockers to this
host-only experiment.

## Review checks

`python scripts/check_promptset.py` passed after this report was added: 50 prompts,
35 requirements, 123 retained hashes and maintained local links. This checks
planning/reference integrity only. `git diff --check --
docs/reports/prompt_5_source_review.md` passed. No sibling tests, target C++ proof,
firmware build or hardware qualification ran as part of this review. The lead
must verify the integrated proof and final changed tree separately.
