# Overnight communication soak, 2026-11-03

Communication only, from 01:17Z to 08:00Z: no enable, motion, NMT, PDO or target write. Each read is an `accepted_not_qualified` object (DEC-1), and `603F`, `1001`, `1003` are never read. Times are UTC unless marked local (UTC+1). This is communication evidence on two specimens, not electrical, sustained-load or motion qualification.

## Setup

| Item | Value |
| --- | --- |
| Fixture | ESP-Prog2 on COM15, serial `94A990398B40`, ESP32-S2, nodes 1 and 2 at 1 Mbit/s |
| Image, night2 and stress1 | Built from `fcea8b4`, flashed by `scripts/s2_flash.py` at 01:14Z, verified by `ping` (`docs/bench/20261103T011449Z-s2-upload`). Both runs share that one boot |
| Reads | `6041`, `6061`, `6064`, `606C`, then `2100`, `2101`, `2102`, alternating; one identity read per node with `MEASURED_U8` |
| Operation | One at a time, alternating nodes, 2 s deadline, 250 ms queue and response budgets, failures retained and never retried |
| Watch | A scratch watcher read each summary and stopped on any failed, refused or stopped operation, TX rejection, drop, bus error or transition. It never fired |

## Runs

| Run | Command | Duration | Operations per node | Failed |
| --- | --- | --- | --- | --- |
| `20261103-soak-pilot1` | Dirty image, `b0b5417` plus work, 200 ms | 120 s | 297 | 0 |
| `20261103-soak-night1` | Same image, 200 ms | About 15 min, stopped for a clean reflash | 1,920 | 0 |
| [`20261103-soak-night2`](../bench/20261103-soak-night2/summary.txt) | `--until 09:00 --interval-ms 200`, stopped at 03:19Z ([stop record](../bench/20261103T0319Z-soak-stop/rx.txt)) to start the stress run | 2 h 01 min 51 s | 18,278 and 18,277 | 0 |
| [`20261103-soak-stress1`](../bench/20261103-soak-stress1/summary.txt) | `--until 07:30 --interval-ms 20` (local) | 3 h 10 min | 196,727 and 196,726 | 0 |

## night2, 200 ms

**36,555 operations and 127,954 SDO steps, all successful.** 128,032 frames each way, with no TX rejection, RX drop or loss, unmatched frame, timeout, cancellation, bus-off, recovery or TWAI error. TEC and REC stayed 0. The worst service gap was 3,788 us (owner gap 3,995 us); queue, active and retained high water were 1. Free stack stayed 9,296 B, internal heap 128,388 B (minimum 123,948 B). SDO latency per step was 4.83 to 5.81 ms, mean 4.89 ms; node 2 had 121 steps above 5 ms, node 1 none. The run ended `reason=stopped` with the read in flight settled.

## stress1, 20 ms

**393,453 operations and 1,377,097 SDO steps, all successful** (196,727 and 196,726 per node, 3 h 10 min 11 s, `reason=duration`, the last read settled). 1,505,129 frames each way, with the same zero counters as night2 and TEC and REC 0. The worst service gap was 4,844 us (owner gap 5,191 us), free stack stayed 9,296 B and internal heap 128,388 B. Latency per step ranged 4.81 to 5.92 ms, mean 4.89 ms; 631 of 1,377,097 steps took more than 5 ms. The host parser's verdict is `incomplete` only because of the console losses below.

**The run is bound by the drives' reply time, as intended.** Each step waits about 4.9 ms for the drive's reply, and an operation is 3 or 4 steps, so it lasts about 15 to 20 ms. A 20 ms interval across two nodes therefore cannot be met, and about 34 operations a second ran instead of 50. The scheduler records this rather than catching up: `slips` counts ticks already past when the next was set (about 775 a minute), and `defBusy` counts turns a due read waited for the previous one. Neither is a failure.

## Status transitions

Only first observations were recorded in every run, and no signal changed after them. At the start of night2 and of stress1, both nodes read:

| Signal | Value | Meaning |
| --- | --- | --- |
| `health` | 0 | `DriverState::UNINIT`: no current qualified endpoint evidence. **It stayed so all night**, through 1.5 million successful exchanges, because no reply qualified service; see the defect below |
| `streak`, `lastError`, `unresolved` | 0 | No failure, no unresolved work |
| `nmt` | `ff` | No NMT observation: the drives' heartbeat producer is not established (`producer=0`, `heartbeat=0`), so freshness stays producer-unknown, as designed |
| `eligibility` | `0xC` | Probe upload allowed, and a service qualification recorded but not current, so **no ordinary SDO**. See the defect below |
| Identity | `1000=00040192`, `1009=0C`, `100A=20`, `1018=5,1000,1,0`, `2000=400`, `2001=200` | Same as packet 11 |
| State | `6041=8037`, `6061=0`, `6064=0`, `606C=0`, `2100=1`, `2101=0`, `2102=0` | Same as packet 11 on both nodes |
| Interface | Controller running, epoch 1, every error and loss counter 0 | |

Two interface counters start nonzero and never change. Both are cumulative since the 01:14Z boot, and both come from the packet 11 baseline commands run on that boot before night2:

- `submitOverwritten=0x4d` (77). The soak's TWAI tap keeps one submission record, and only the soak takes it. The baseline's 9 identity and 30 readiness reads per node made 78 single-frame uploads, so 77 overwrote an untaken record. On the earlier boot that ran pilot1 and night1, it was 0.
- `conRej=2524`. The console's refused submissions, all retries by the baseline's evidence dumps; no line was lost. The soak writes only after measuring room and never adds to it.

## Defect found: no ordinary SDO on this fixture

`eligibility=0xC` and `health=UNINIT` mean the owner never held qualified evidence of these drives, and the session could not have sent any write on this fixture: enable, sequenced stop, move and settings routes would all refuse `eligibility`. Service qualification needed an exact receive time; the S2 shim gives only bounds, and the drives send no heartbeat. The fix qualifies service when a reply's earliest receive bound is strictly after its handoff and after the node boundary ([receive-time guide](../adapters/twai_receive_time.md#sdo-service-qualification), regression `test/bounded_service_test.cpp`). Its retest on hardware is in the final runs below.

## Console loss between the S2 and the host

**Observed.** After 2 h of stress1 with none, the host parser found three losses. The firmware reported `anomalies=0` throughout, and its own line counter stayed contiguous, so it emitted every line.

| UTC | Line | What arrived | Missing |
| --- | --- | --- | --- |
| 05:22:48 | `SOAK_SUM n=1414` | Cut after `twaiErr=0 twai`, spliced onto the tail of `SOAK_GAP n=1415` | **64 bytes** |
| 05:43:48 | `SOAK_SUM n=1645` | `rx=` spliced onto `resFull=0` | **64 bytes** (210 against 146) |
| 06:07:48 | `SOAK_GAP n=1910` | `gapMax=4844 ga` spliced onto `=1 retHw=1` | **35 bytes** (165 against 130) |

**Where it was lost.** Each splice lies inside one contiguous buffer that pyserial returned, so the bytes were lost below pyserial. Every summary minute shows a 1-byte wake, then 351 to 479 bytes already buffered, and 186 of the 189 bursts arrived whole, so the host buffer was far from full. The firmware writes whole lines only after measuring room. That leaves the Windows CDC driver (`usbser` 10.0.26100.9278), the USB link, or the ESP-Prog-2 bridge (`303A:1002`, a composite CDC, MSC and JTAG device). Whole transfers of up to 64 bytes are missing, which points at a dropped USB transfer or a bridge receive overrun.

**What it is not.**
- **Host load.** The second and third losses came with the host at 11% and 7% CPU and under 0.6% DPC time (`typeperf`, every 2 s from 06:25 local).
- **Host read latency.** Measured against firmware time, the host's receive delay only drifts linearly, by clock drift, and the loss minutes are not outliers.
- **Display power.** Screen-off periods (Kernel-Power 566) do not line up with the losses.

**What they share.** All three fell in the first 300 bytes of a summary burst, after about 60 s of UART silence. Windows USB selective suspend is enabled on this host, AC and DC; it was left unchanged.

**Reproduction on the new image** (`261a25b`, flashed 06:31Z, same bridge, host otherwise idle):

| Run | Reads | Quiet spell before each burst | Bursts | Lost |
| --- | --- | --- | --- | --- |
| night2 (old image) | 200 ms | about 60 s | 122 | 0 |
| stress1 (old image) | 20 ms | about 60 s | 189 | **3** |
| [`20261103-soak-burst1s`](../bench/20261103-soak-burst1s/summary.txt) | 200 ms | under 1 s | 1,199 | 0 |
| [`20261103-soak-idle10s`](../bench/20261103-soak-idle10s/summary.txt) | 200 ms | about 10 s | 180 | 0 |
| [`20261103-soak-final`](../bench/20261103-soak-final/summary.txt) | 20 ms | about 60 s | 36 | 0 |

At stress1's rate, the 1 s run would have shown about 19 losses, the 10 s run about 3 and the final run about 0.6. None showed any, so a per-burst random fault, and any per-byte fault in the S2's console path (1.8 MB passed through it in the 1 s run), are ruled out. The final run, at stress1's settings on a fresh bridge session, had too few bursts to tell either way. **The culprit is not yet definitive.** Three factors remain confounded: a quiet spell of about a minute, the 20 ms read load, and the bridge's state, since flashing the S2 through the bridge also restarted its serial session. The leading mechanism is a receive overrun in the bridge: if the host's USB stack is slow to resume polling after a long idle (USB selective suspend is enabled on this host), the bridge's buffer overflows at 11.5 kB/s and a chunk of up to 64 bytes is discarded. That fits each observation so far, but none has proved it.

**Next measurement.** An A/B pair of runs at stress1's settings, each at least 3 h: one as stress1, one with `--keepalive-s 1`, which keeps host-to-bridge traffic going through the quiet spells. If the loss follows the idle link, the keepalive run stays clean. Changing the host's USB power policy is the owner's decision and was not done.

**Mitigation now.** Loss is detected by the line numbers, never silent, and costs at most one summary window's `gapWin`. Runs whose quiet spells are at most about 10 s showed none.

### A/B attempt, 2026-11-03 evening

Run on `e724a39`, on a fresh boot flashed at 16:42Z, unattended under the user's waiver. Both runs used stress1's settings: run A plain from 16:43:53, then run B with `--keepalive-s 1` from 19:44:08, 3 h each ([A](../bench/20261103-soak-ab-plain2/summary.txt), [B](../bench/20261103-soak-ab-keepalive2/summary.txt)).

| Phase | Console | Bursts after about 60 s of quiet | Transfers lost |
| --- | --- | --- | --- |
| A, 16:43 to 18:13, drives answering | Idle between summary bursts | 89 | **3**, at 17:51:54, 17:58:54 and 18:12:54, 64 bytes each, in the first lines of a burst |
| A, 18:13 to 19:44, drives silent | Continuously busy | 0 | 0 |
| B, 19:44 to 22:44, drives silent, keepalive on | Continuously busy | 0 | 0 |

At 18:13:49 both drives stopped acknowledging; the cause was not observed. After that, the soak firmware printed one bus anomaly per pass, so the link was never idle. That made about 1.5 million lines with no transfer loss. Run B started with both nodes stopped, still unresolved from A, and never had a quiet spell, so **the keepalive's effect was not tested.** At the end of each run, the firmware could not format its final `SOAK_CON` (`OUTPUT_FORMAT_ERROR`). That was a firmware formatting failure, not loss on the link; it and the flood are fixed in `711c632` (see the [unattended report](2026-11-03_unattended_bench.md#after-the-campaign-new-image-error-history-final-state)).

So far: 6 losses in 278 bursts that followed about a minute of quiet (stress1 and A), and none in 4.5 h of continuous output or in 1,379 bursts after quiet spells of at most about 10 s. A fresh bridge session did not prevent loss: A's first loss came 68 minutes after the flash. These results still fit loss following the idle link, but they do not prove it, because the 20 ms read load ran only in the phases that lost data. The next measurement is unchanged and needs the drives answering.

**Rerun with the drives answering, 2026-11-04** ([report](2026-11-05_motion_and_stress.md#console-loss-ab-completed)): 3 h each, same settings. Run A, plain, lost 2 transfers in 180 bursts; run B, with `--keepalive-s 1` (10,568 newlines sent), lost 3. Every loss was again in the first lines of a burst after about 60 s of quiet. So the host keepalive does not prevent it, and the loss does not follow the host's transmit idling. It still fits the S2-to-host direction idling, which only output from the S2 itself can keep busy.

**Effect on the evidence.** The line numbering detected each loss, as designed. Bus, owner and node counters are cumulative, so the next summary restored them; only `gapWin` for those minutes is lost. A lost transition would show as a chain break, and none occurred. No CAN behavior is implicated.

## Final runs on the new image

The S2 was flashed with `261a25b` at 06:31Z through `scripts/s2_flash.py`, checked by `ping` and `--check-ping` (matching ELF hash), and its controller started at 1 Mbit/s with no request ([upload](../bench/20261103T0632Z-s2-upload), [ping](../bench/20261103T0632Z-s2-ping), [start](../bench/20261103T0633Z-start)). Three soaks followed on that boot, every one with verdict `complete`, no lost line, and every operation successful:

| Run | Settings | Duration | Operations per node | Steps per node | Worst gap | Free stack |
| --- | --- | --- | --- | --- | --- | --- |
| [`20261103-soak-burst1s`](../bench/20261103-soak-burst1s/summary.txt) | 200 ms, summary 1 s | 20 min | 2,997 and 2,997 | 10,495 and 10,495 | 4,178 us | 8,976 B |
| [`20261103-soak-idle10s`](../bench/20261103-soak-idle10s/summary.txt) | 200 ms, summary 10 s | 30 min | 4,497 and 4,497 | 15,745 and 15,745 | 3,928 us | 8,976 B |
| [`20261103-soak-final`](../bench/20261103-soak-final/summary.txt) | 20 ms, summary 60 s | 36 min | 37,219 and 37,218 | 130,272 and 130,269 | 4,784 us | 8,976 B |

**Transitions.** At the first replies of the first run, both nodes went from `health=UNINIT` to `READY` and from `eligibility=0x4` to `0xE`. That is the hardware retest of the qualification fix, and it held through all three runs with no further change of any signal. The final run's 313,021 frames each way had no TX rejection, drop, timeout, bus error or recovery, and TEC and REC stayed 0.

**Differences from the old image, measured, not attributed.** Free stack under the soak fell from 9,296 B to 8,976 B. `261a25b` doubled the console line buffer on the task stack, and added the forwarding owner transport. The slowest step rose from 5.92 ms to 6.93 ms; 147 of 260,541 steps took more than 5 ms, all under 10 ms.

## What this does and does not show

It shows that the ordinary typed-read path, owner and TWAI adapter sustain this load on these two specimens without a fault, including at the drives' reply-time limit. It does not cover TPDO or bus load, arbitration loss, bus errors, a no-ACK, recovery, or any write. Those are packet 3's questions, and most need load or an observer this fixture lacks.
