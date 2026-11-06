# Packet 4: bounded silent capture

[Packet 4](../../prompts/bench_closure_18_33/4_passive_capture.md), 2026-11-06. Software, host tests and firmware builds are recorded here. The S2 smoke check of the capture is pending: it needs this commit's bench image. Physical silence, no-ACK and no-TX are not established by this packet and stay open for an independent observer. Back to the [execution index](README.md).

## Start state and sibling audit

Branch `promptset/motorcontrol-can` at `5690ecf`. The tree was clean apart from the user's untracked `.vscode/` files, which were not touched. All sibling HEADs are clean and unchanged since the authoring row: MotorControl-RS `cd27b12`, INA228 `73f9548`, FieldCore-node `5ad9292d` (read-only). Only the seams the packet names were read.

| Sibling | Seam read | Adopted | Rejected |
| --- | --- | --- | --- |
| MotorControl-RS | `include/MotorControlRS/Traffic.h` `TrafficCapture`; `examples/common/RtuRunner.cpp` `Runner::setTrafficCapture`; `examples/probe_cli/DebugSession.h` `DebugSession::service`; `RtuRunner.h` `transmitEnabled` | Non-consuming copies in caller storage with an explicit loss count. Attach only while the runner is idle (here: owner idle and retired). One output line per service, holding for ordinary results. An uncertain direction counts as enabled (here: an unknown silent level keeps transmission latched closed) | The overwrite-oldest ring: a finite record keeps its first frames and counts the overflow, so its begin boundary stays fixed. The RS UART and DE wiring |
| INA228 | `include/INA228/INA228.h` `HardwareState`; `src/INA228.cpp` `INA228::invalidateHardwareState` | A refused GPIO request makes the level unknown (`TransceiverState::FAILED`), never the previous level. Nothing upgrades "requested" to "silent", and the first failure latches the gate | Resynchronization through a verified reinitialization: no GPIO readback is taken as transceiver evidence, and nothing re-verifies the level |
| FieldCore-node | `src/rs485/ShzkDeviceModule.cpp` `ShzkDeviceModule::onBusInvalidated`; `include/TunnelMonitor/calibration_capture/CalibrationCaptureModel.h` `observeMedia` | Retained results survive entry, capture and exit unchanged | Treating capture as a bus invalidation: the owner keeps dispatching and no node evidence becomes unknown, because receive continues and only local TX is closed |

## Source facts behind the design

The full table is in the [SDK evidence](../../adapters/twai_sdk.md#listen-only-mode-the-silent-transceiver-input-and-the-bench-capture) (two Kconfig sources were added to the [hash manifest](../../adapters/twai_sources.json), compared byte for byte with the pinned upstream URLs). In short:

- **Listen-only is fixed at creation.** The pinned node API offers it only as a creation flag (`esp_twai_onchip.h:36`) and has no call that changes the mode of a live node (`esp_twai.h:24-149`). The HAL writes the mode in reset mode only (`HAL:142-163`). Using it would mean recreating the node through owner recovery twice per capture, invalidating every reservation and node's freshness.
- **The S2 build lacks the listen-only errata workaround.** On ESP32-S2, listen-only still drives dominant active error flags unless REC is preset to 128 (`components/driver/twai/Kconfig.twai:52-61`, applied at `HAL:147-157`). That option belongs to the legacy `driver` component, which the bench (`set(COMPONENTS main)`) does not build. The resolved `sdkconfig` of this packet's `s2` gate has no such option.
- **The transceiver contract.** NXP TJA1051 Rev. 9, §7.1.2 and Table 4: a HIGH level on S disables the transmitter and keeps the receiver; S has an internal pull-down (§7.2.2). This requires the requested level to reach pin S. A successful GPIO call does not show that.

**Mode representation.** There is no new controller mode. The capture is a bench state (`SilentCapture`) over the running normal-mode node, consisting of three things:
- a software transmit gate in `SoakTwaiTap`, the last point before the SDK;
- a requested TJA1051 silent level on GPIO18;
- the tap's copied receive ring, shared with the watch and therefore exclusive with it.

## Requirements

| Requirement | Delivered | Tests |
| --- | --- | --- |
| 1 One finite capture, same owner, driver and RX copies, duration and capacity required, overflow and end reason | `SilentCapture` (`examples/s2_bench/SilentCapture.h/.cpp`): `CapturePlan` id, seconds 1..600, frames 1..128; `CaptureEnd` DURATION, STOPPED, CONTROLLER. Copies come from `SoakTwaiTap::copyReceives`, the PDO watch's path. There is no task, node or adapter of its own. Record: `stored`, `overflow`, `tapDropped`, `lost`. Console line `C<capture>,<seconds>,<frames>` / `C0` in `PdoCommand` | `planIsChecked`, `finiteExpiryAndExit`, `explicitStop`, `framesAreCopiedAndOverflowCounted`, `tapRingLossIsReported`; `pdo_command` `captureLines` |
| 2 Refuse entry while work is active or queued, from retirement evidence, refuse when unknown; no channel resolution or replay | `SilentCapture::ready`: controller running and the owner neither inhibited nor recovering; no queued or active SDO (ordinary and urgent), no pending NMT, typed read or acquisition; `localTxRetired` on every configured slot; no `nmtTxPending`; `SoakTwaiTap::sdkRetired` (no SENT frame without its exact final report, no FAILED_UNCERTAIN since a confirmed stop). GPIO is changed only after these checks | `entryRefusedWhileWorkQueuedOrActive`, `entryRefusedWhileTxReportPending`, `entryRefusedWhileCleanupAbortStaged`, `entryRefusedAfterUncertainSubmission`, `entryRefusedWhileNmtPending`, `entryRefusedForControllerAndReads`, `retainedTerminalsSurvive`, `tapRetirementEvidence` |
| 3 Reject every path to transmission while active; reconcile listen-only and the silent input | The tap gate refuses every submission as FAILED_UNSENT before `IdfTwaiDriver::submit`, the only caller of `twai_node_transmit`. The adapter reports it proved unsent and the owner withdraws it. Entry's retirement rule leaves nothing in the SDK for hardware retransmission. This application calls no SDK recovery. SDK and NXP facts as above | The systematic group `refusedOrdinaryUpload`, `refusedOrdinaryDownload`, `refusedUrgentWork`, `refusedBestEffortStop`, `refusedNmt`, `refusedDrainMarker`, `refusedCleanupAfterCancel`, `refusedAtTheDriverBoundary` (SDO request and abort, NMT, SYNC, RPDO, TPDO, heartbeat ID, extended, RTR), `recoveryEndsTheCapture` |
| 4 Normal dispatch kept; boundaries, clock, counters, level, overflow, image identity; unknown frames kept; stop, inspection and startup send nothing | The owner dispatches every frame unchanged. `CAP_TIME` begin, end and nominal times plus the largest service gap, `clock=esp_timer_us`. `CAP_CTRL` epoch, TEC/REC, owner and adapter inhibit, TWAI counters at both ends. `CAP_OWNER` owner totals and tap refusals at both ends. `CAP_LEVEL` requested levels, exit, reopen. `CAP_IMAGE` revision and ELF SHA-256. `CAP_F` bounds, epoch and `pre`/`post` marks; any identifier, extended and RTR frames are kept | `startupIsPassive`, `lateReceiveBoundaries`, `framesAreCopiedAndOverflowCounted`, `linesFit`; `s2_pdo_contract` `FirmwareTextTest` |
| 5 Fake tests: held/uncertain TX, GPIO and mode failures, all send paths, late RX, expiry, overflow, repeated exit, retained terminals | As above, plus `silentRefusedThenRestored`, `silentAndRestoreRefused`, `exitNormalRefused`, `controllerFaultEndsTheCapture`, `frozenClockStillEndsTheCapture`, `queuedWriteCannotLeaveAcrossExit` | 32 scenarios, 7128 checks |
| Entry and exit order | The gate closes before the silent request. The copies stop, then the normal request is made, then the gate opens, and only once `ready()` holds again. After a CONTROLLER end or a refused normal request, the gate stays closed for the boot | Pin calls record the gate state at each request (`finiteExpiryAndExit`, `silentRefusedThenRestored`) |
| Runner | `scripts/s2_pdo.py`: protocol `pdo=3` adds `C` (older images never get one); one held capture record released by its own line; `capture_problem` checks the terminal; `C0` cleanup only; preflight refuses `capture=1`; `analyze` reports captures per identifier | `s2_pdo_contract` `CaptureTest` (7), `FirmwareTextTest` (2), the preflight and protocol cases |
| Grammar and PONG | `scripts/s2_evidence.py` rows for every capture line; PONG `capture:b?`, `pdo=3`; PONG and the retained response buffer widened to 320 bytes, so the widest PONG (300) fits | `s2_soak_contract` (PONG fields), `s2_pdo_contract` `test_pong_fits_its_buffers` |

**Bench integration** (`examples/s2_bench/main/main.cpp`). Every GPIO18 request now goes through one `driveSilent` (`espSilentPin`), so one `TransceiverState` holds the last request: at boot, at `c`, on a progression failure and in the capture. A capture also needs:
- `controllerReady`;
- no soak, trial record, watch, dump, network listing, NMT result or other record.

While a capture collects, its record is held or its gate is closed:
- Only `p`, `s`, `n`, `d`, `r` and `C0` are served.
- Lines are still collected whole, then refused: `PDO_LINE`, `SOAK_REFUSED` and `TRIAL_REFUSED` answer `reason=capture`.

The record prints after the end and is released with `r`. `SilentCapture` is 7608 bytes on S2 (measured with the pinned Xtensa compiler) and is allocated from internal heap on first use. Its largest stack frame is 160 bytes (`begin`, `-Os`).

## Tests and mutants

Each mutant was applied to a copy of the tree (`.build/p4-mut/tree`), built, run and restored byte for byte (SHA-256 equal to the working file). The restored baseline then passed again.

| Mutant | Killed by |
| --- | --- |
| M1 entry ignores per-slot owner retirement | `entry-cleanup` (staged abort deferred, tap retired) |
| M2 entry ignores the tap's SDK retirement | `entry-uncertain` |
| M3 a refused silent request always reopens the gate | `silent-latched` |
| M4 silent level requested before the gate closes | `silent-restored` |
| M5 gate reopens at exit without idle and retirement evidence | `exit-queued-write` (an old queued write would leave) |
| M6 entry admits queued or active work | `entry-work` |
| M7 the closed gate forwards submissions | `silent-latched`, and every `send-*` scenario |
| M8 copies continue after the end | `late-rx` |
| M9 frame capacity ignores the plan | `overflow` |
| M10 a controller end requests the normal level | `send-recovery` |
| M11 any final report retires the tap's outstanding frame | `tap` |
| M12 exit opens the gate before the normal level | `exit-refused` |
| M13 a controller fault does not end the capture | `send-recovery` |
| M14 a confirmed stop keeps the tap's uncertainty | `tap` |
| M15 duration expiry one turn late | `expiry` |
| M16 a frozen clock keeps the capture collecting | `frozen-clock` |

M1 first survived, because the tap's evidence covered every earlier scenario. `entryRefusedWhileCleanupAbortStaged` builds the reachable state in which only the owner sees the staged work: a client abort after a segmented reply to an expedited-only upload, deferred by captured RX.

## Review

An independent read-only reviewer checked old queued writes across entry and exit, bypass paths, command gating and C++ bounds. It found no TX-safety defect. Its findings were handled as follows:

| Finding | Disposition |
| --- | --- |
| A clock failure in the turn after `begin` could leave the capture collecting for the rest of the boot. TX was still safe | Fixed: a failed controller ends the capture at the last accepted time (M16) |
| `controllerUsable` ignored the adapter's inhibit flag | Added. Every adapter inhibit is also reported to the owner, so no separate test can reach it |
| `txRefused` counts only begin to end; `reopened` reflects the moment of printing | Documented in `SilentCapture.h` and the bench guide. In the idle case the gate reopens in the end turn, before the dump |
| The capture assumes the level before entry was normal, and `ready()` cannot see an AxisSession that defers its next admission | Documented as the caller contract. The bench requires `controllerReady` and no held route |
| A frame only the S2 would acknowledge | Reasoned in the SDK evidence. The reviewer expected duplicates; this report expects none, because the S2's own controller detects a bit error on its unechoed ACK under CAN 2.0B. Both are expectations, unmeasured |

## Checks

All runs are on the final tree.

| Check | Result |
| --- | --- |
| Focused CTest in `.build/p4` (GCC 15.1.0, Release, `-Werror`), 38 tests: TWAI adapter and driver, `firmware_probe`, ESP platforms, `native_probe`, `pdo_experiment`, `raw_evidence`, `soak`, `soak_transcript`, `pdo_watch`, `silent_capture`, `pdo_command`, `bench_route` (and transcript), `probe_app`, owner suites, `network_receive_time`, PDO suites, `firmware_build_contract`, repository, documentation and planning contracts, every `s2_*` contract | 38 of 38 passed, exit 0. `silent_capture` 7128 checks, `pdo_command` 132, `s2_pdo_test.py` 35 tests, `s2_evidence_test.py` 87, `s2_soak_test.py` 41 |
| `python scripts/check_repository.py` | PASS, exit 0 |
| `python scripts/check_promptset.py`; `git diff --check` | Passed, exit 0, no errors; whitespace clean, new files included |
| Strict Doxygen 1.13.2 on a copy outside `.build` | Exit 0, 0 warnings. `check_documentation`: 28 headers, 2532 members, 101 free functions. The bench headers are not in `INPUT` |
| `check_firmware.py --target s2`, from PowerShell with the pinned IDF 5.5.5 tools, `--build-dir .build\p4-fw-s2` | Passed, exit 0, run `20261106T173837813901Z-43456-dgm1ac6b`. 43 SDK hashes verified, including the two added Kconfig files. The resolved `sdkconfig` has no `TWAI_ERRATA` option |
| `check_stack.py --strict --require-toolchain --target esp32s2 --compile-commands esp32s2=<that run>/s2/compile_commands.json --build-dir .build\p4-stack` | Passed, exit 0, 0 breaches. `SilentCapture` frames at most 160 bytes |
| Mutants | 16 of 16 killed, each file restored byte for byte |
| Staged tree alone, in a detached worktree outside `.build` | `verify.py --mode full --compiler gcc` passed every step, documentation included (28 headers); S2 bench firmware gate and strict S2 stack check passed |
| Remote CI | Recorded with the commit in the [execution index](README.md) |

## Stays open

Each of these needs a measurement or an instrument that this packet does not have.

- **Physical silence, no-ACK and no-TX.** Whether GPIO18 reaches TJA1051 S at a valid level. Whether the S2 drives no ACK or error flag while S is high. Whether TXD stays recessive.
- **Q18 and remote drains.** Whether a drive's earlier SDO session drained. An independent, verified listen-only observer on the bus is required for these. The PCAN-USB is available but not integrated.
- **The bench smoke check.** Not run: the lead's step on this commit's image.
- **Load and FIFO behavior.** Sustained-load behavior of the copy path and hardware FIFO overrun, which the SDK does not report.
- **Packet 3's unmeasured items.** The levels of GPIO18 and TXD across reset and boot remain as packet 3 recorded them.

The capture is bench-only. The installed core, `TwaiAdapter`, `IdfTwaiDriver` and `FirmwareProbe` are unchanged.

**Next measurement.** On this image with both drives in Pre-operational, run one 10 s capture. It should record about 20 heartbeats (`701`/`702` `7f`), with `txRefused=0` and owner `tx` equal at both ends. Then an independent listen-only observer should repeat that capture and record the bus at ACK slots. A single-drive variant, with the observer present, would test the silent input directly.
