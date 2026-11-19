# Prompt 56: host throughput, event wake and owner turn cost

State: **Complete**. Measurements, owner change C, the clock and
cache change and the event wake are on `main`; each step was measured on the S2 bench with both
CL86-C drives released, using reads only. The position trial of step 1 was not run: it needs an
enable, and nobody was at the bench.

## Scope

[Prompt 56](../prompts/56_host_throughput.md), from the
[host throughput report](2026-11-12_host_throughput.md). The user's direction of 2026-11-19 kept
review and self-repair short: one independent review pass, important findings only.

## Source audit

| Repository | Revision and state | Seam and disposition |
| --- | --- | --- |
| MotorControl-RS | `e4d0e3d` at planning; `58038c5` at this report, with concurrent RS work in `examples/probe_cli` (three files modified). No change in `examples/probe_idf`, `src` or `include` between them | Owner pacing is the example platform's `Platform::idle(ms)` with `vTaskDelay` (`examples/probe_idf/main/IdfPlatform.cpp:46`). Adopted: pacing belongs to the example platform. Rejected for CAN: tick-only pacing, replaced by the TWAI event wake |
| FieldCore-node | `c40ea834`, clean, read-only | Task handoffs use `xTaskNotifyGive` and `ulTaskNotifyTake(pdTRUE, timeout)` (`src/storage/StorageDiagnostics.cpp:3482`, `:3506`). Adopted: the take-with-timeout wait. Its notify is task to task; the ISR notify is CAN's own |

## Delivered

- **Measurement** (`dcb685b6`, fix `4ed4c834`): `FirmwareProbe::takePhaseTiming` (cycles of
  `pollEvents`, the app and owner turn, `promote` and the evidence collector, through the optional
  `ProbeCallbacks::cycles`), `IdfTwaiDriver::takeTiming` (submission to TX done, TX done to the next
  reception, RX FIFO high-water mark) and the bench `PassMeter`. `s` prints three `TIMING` lines
  ([S2 bench README](../../examples/s2_bench/README.md) "Pass timing").
- **Owner change C** (`4ff994f4`): the TX scan tries only staged channels and expires once after
  the scan ([owner guide](../can_owner.md)). Host turn at N = 32: about 210 us before, 30 us after.
  Two of the five required test cases cannot be built: the admission rules make a staged peer frame
  beside live peer work unreachable (one reservation per node, peer stops wait, markers and cleanup
  aborts only on unresolved routes). The reachable effect is tested and fails without the change.
- **240 MHz** (`fd9c3eb7`) for the shared S2 and S3 images and the bench image, checked by the
  firmware gates.
- **16 KB instruction cache** for the S2 bench image (`360c0ed8`), replacing the prompt's `-O2`
  (below).
- **Event wake** (`d89fddd4`): `IdfTwaiDriver::setOwnerWake` and the bounded
  `EspProbePlatform::idle` ([TWAI SDK guide](../adapters/twai_sdk.md) "Event wake"). Both ESP-IDF
  mains set it.
- Corrections with the measured figures in the multi-drive preparation, capacity, limitations and
  overnight soak reports and in prompt 51.

## Measurements

S2 bench, nodes 1 and 2 released in mode 1, reads only. "Read" is one SDO upload from admission to
completion in the soak (`SOAK_NODE` mean); the soak is two nodes at a 20 ms interval. Cycles and
times come from the `TIMING` lines; "owner turn" is the `app` phase (owner plus sessions).

| Image | Idle pass | Idle owner turn | Soak pass | Soak owner turn | Soak passes/s | Read | Records |
| --- | --- | --- | --- | --- | --- | --- | --- |
| Baseline `dcb685b6`: 160 MHz, 8 KB I-cache | 891 us | 82k cycles, 512 us | 2.74 ms | 224k, 1.40 ms | 298 | 5.76 ms | [baseline](../bench/20261119-p56-baseline/) (20 min soak) |
| `fd9c3eb7`: + change C, 240 MHz | 837 us | 110k, 457 us | 2.57 ms | 312k, 1.30 ms | 323 | 5.77 ms | [240 MHz](../bench/20261119-p56-240mhz/) (3 min) |
| Experiment: + 8 KB data cache | 833 us | 110k, 457 us | 2.46 ms | 299k, 1.25 ms | 336 | 5.77 ms | [cache v1](../bench/20261119-p56-cache-v1/) (60 s) |
| Experiment: + 16 KB I-cache | 356 us | 48k, 201 us | 2.01 ms | 240k, 1.00 ms | 392 | 4.87 ms | [cache v2](../bench/20261119-p56-cache-v2/) (60 s) |
| `d89fddd4`: + 16 KB I-cache, event wake | 335 us | 48k, 200 us | 2.16 ms | 257k, 1.07 ms | 419 | 4.22 ms | [wake](../bench/20261119-p56-wake/) (5 min) |

- **The drive is fast.** TX done to the next reception: 0.15 to 0.26 ms, mean 0.20 ms, in every
  run. Submission to TX done: 0.22 ms. The 2 to 3 ms "reply" of the 2026-11-12 report was host
  pacing.
- **The pass is bound by instruction fetch, not the clock.** 240 MHz added cycles (82k to 110k for
  the same idle turn): solving the two runs gives about 26k cycles of compute and about 350 us of
  fixed time. The S2 bench runs code and FreeRTOS from flash through an 8 KB instruction cache
  (B01 also removed the data cache). The 8 KB data cache changed nothing; the 16 KB instruction
  cache cut the idle pass 2.3 times and the soak read by 0.9 ms. It costs 8 KB of internal RAM:
  free heap after boot 81476 bytes before, 73284 after.
- **`-O2` was not applied.** The prompt asked for it, but the gate pins `-Os` for B01, and larger
  code would make a fetch-bound pass slower. A measured `-O2` build stays open if needed.
- **The wake** shortened the soak read from 4.87 to 4.22 ms and raised the passes to 419 a second;
  each node completed 17.2 operations a second against 12.2 at baseline, every one successful. Under
  load the owner-and-sessions turn still takes about 1.07 ms, and the bench's own collectors and
  console about another 1 ms. Those, not the tick, now bound the pass.
- Every soak and identity read in these runs succeeded; TEC and REC stayed 0.

## SYNC 10 ms, 30 minutes

Image `d89fddd4`, both nodes released, heartbeats 1000 ms, both TPDO1 at type 1 and Operational,
no other producer, `scripts/sync_watch.py` as in prompt 54
([setup](../bench/20261119-p56-sync/02-type1-operational/), [short](../bench/20261119-p56-sync/03-sync-10ms-short/),
[30 minutes](../bench/20261119-p56-sync/04-sync-10ms-30min/), [restore](../bench/20261119-p56-sync/05-restore/)):

| Run | SYNC confirmed | TPDOs per node | Capture | Timing (software and driver bounds) |
| --- | --- | --- | --- | --- |
| 10 ms, 5 s | 500 / 500 (`LOCAL_TX`) | 500 | Complete | Short check |
| 10 ms, 1800 s, 180,000 slots | 180,000 / 180,000 (`LOCAL_TX`, 180,000 terminals confirmed) | 180,000 (`0x181`, `0x182`), none rejected | 363,598 frames, all printed, `tapDropped` 0, `timing_complete` | Owner handoff gaps 7.24 to 13.0 ms, worst lateness 4.56 ms; receive gaps 6.45 to 13.67 ms (mean 10.0, sd 0.42) per node; heartbeats 1799 per node at 1000.8 ms |

**10 ms now passes the 30-minute requirement** on this image (prompt 54 item 2). Before this
prompt it ended after 42,988 slots (about 430 s) on a missed processing deadline (prompt 54, run
38), and 20 ms was the shortest period that passed. These are local transmission confirmations and
driver receive bounds, not wire times; Q27 (same-cycle sample identity) stays open. One run on these
two specimens, not qualification for more axes. Afterwards both TPDO1 were restored to type 255 in
Pre-operational; the 1000 ms heartbeat stays until the next power cycle. The first setup plan
(`01-setup`) was refused at its first NMT request, sent while the network listing still printed;
the second plan waited for each listing's end.

## Review

One independent review of change C and the wake. Change C: no defect; the claim that skipping
empty channels changes no handoff holds. Wake: one important defect, fixed before commit. The
first bound reset each tick and so never fired at passes over about 250 us; with a notification
pending after every pass the owner task would never block, starving the idle task and tripping the
task watchdog's idle check. The bound now counts passes in a row without blocking, and the test
advances no tick.

## Not done and next

- Step 1's position trial (needs an enable at the bench).
- The 16 KB instruction cache for the shared S2 image, pending its own RAM measurement.
- Step 2's follow-up (the receive loop's per-frame expiry, a linear `expire`): the owner turn under
  load is now the largest single phase, so this is the next measured candidate, then the bench
  collectors' cost. Several sub-turns per wake (step 5) were not needed for the SYNC result below.
- A console-loss check under the new pass rate.

## Branch and commits

Branch `main` (prompt 42: no new branches until version 0): `dcb685b6`, `4ed4c834`, `4ff994f4`,
`fd9c3eb7`, `360c0ed8`, `d89fddd4`, and this report's commit.
