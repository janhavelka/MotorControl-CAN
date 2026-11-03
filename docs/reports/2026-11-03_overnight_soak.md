# Overnight communication soak, 2026-11-03

Communication only: no enable, motion, NMT, PDO or target write. Each read is an `accepted_not_qualified` object (DEC-1), and `603F`, `1001`, `1003` are never read. Times are UTC unless marked local (UTC+1). This is communication evidence on two specimens, not electrical, sustained-load or motion qualification.

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
| [`20261103-soak-stress1`](../bench/20261103-soak-stress1/summary.txt) | `--until 07:30 --interval-ms 20` (local) | 3 h 10 min | See below | 0 |

## night2, 200 ms

**36,555 operations and 127,954 SDO steps, all successful.** 128,032 frames each way, with no TX rejection, RX drop or loss, unmatched frame, timeout, cancellation, bus-off, recovery or TWAI error. TEC and REC stayed 0. The worst service gap was 3,788 us (owner gap 3,995 us); queue, active and retained high water were 1. Free stack stayed 9,296 B, internal heap 128,388 B (minimum 123,948 B). SDO latency per step was 4.83 to 5.81 ms, mean 4.89 ms; node 2 had 121 steps above 5 ms, node 1 none. The run ended `reason=stopped` with the read in flight settled.

## stress1, 20 ms

<!-- final figures are added when the run ends at 07:30 local -->

By its 05:07Z summary, 111,718 operations and 391,019 steps, all successful, with the same zero counters as night2 and TEC and REC 0. The worst service gap was 4,844 us. Latency per step ranged 4.81 to 5.92 ms, mean 4.89 ms, and under 1 in 2,000 steps took more than 5 ms.

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

## Console loss at 05:22Z: 64 bytes between the S2 and the host

**Observed.** After 5 h with none, the host parser reported `gaps=2 malformed=1` in stress1's 05:22:49Z digest. The firmware reported `anomalies=0`, and its own line counter stayed contiguous (`SOAK_CON lines=1416`). Raw line 1415 of `trial-01/rx.bin` is the `SOAK_SUM n=1414` line cut after `twaiErr=0 twai`, spliced onto the tail of `SOAK_GAP n=1415` from `Win=4715`. So `n=1415` is missing and `n=1414` is malformed. The missing span, `Flags=0 twaiDrop=0`, the newline and `SOAK_GAP n=1415 t=… gapMax=4844 gap`, is **exactly 64 bytes**: one USB full-speed bulk packet.

**Where it was lost.** The splice lies inside one contiguous 351-byte buffer that pyserial returned (`rx.times.csv` offset 194,670), so the loss happened below pyserial. Every summary minute shows the same pattern, a 1-byte wake then 351 to 479 bytes already buffered, with no loss in 123 other minutes, so the host buffer was far from full. The host's receive delay against firmware time only drifts linearly (137 to 220 ms over the run, clock drift), and the loss minute is not an outlier. The firmware writes whole lines only after measuring room, so it cannot emit half a line. What remains is the Windows CDC driver (`usbser` 10.0.26100.9278), the USB link, or the ESP-Prog-2 bridge (`303A:1002`, composite CDC, MSC and JTAG). A whole 64-byte packet points at a device-side buffer drop.

**Competing triggers.** At that minute the host was running parallel builds by the review and fix workers. Hypothesis A: delayed host USB service under load let the bridge's buffer overflow. Hypothesis B: the bridge drops packets intrinsically, at a low rate. The separating observation is the loss rate with and without controlled host load at the same console rate; see the reproduction below. A host CPU, DPC and interrupt log runs from 06:25 local for correlation.

**Effect on the evidence.** The line numbering detected it, as designed. The bus, owner and node counters are cumulative, so the next summary restored them; only the `gapWin` window value for that minute is lost. A lost transition would show as a chain break (`soak_transition_gap`), and none occurred. No CAN behavior is implicated.

## Final run

<!-- added after the run -->

## What this does and does not show

It shows that the ordinary typed-read path, owner and TWAI adapter sustain this load on these two specimens without a fault, including at the drives' reply-time limit. It does not cover TPDO or bus load, arbitration loss, bus errors, a no-ACK, recovery, or any write. Those are packet 3's questions, and most need load or an observer this fixture lacks.
