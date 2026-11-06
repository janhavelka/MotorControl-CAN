# Packet 5: console-loss diagnostics

[Packet 5](../../prompts/bench_closure_18_33/5_console_diagnostics.md), 2026-11-06. Host tools, one firmware diagnostic and a finite reproduction on the S2 fixture. Where the console bytes are lost is still unresolved; this packet shows they were lost after the S2's UART FIFO had accepted them. Back to the [execution index](README.md).

## Start state and sibling audit

Branch `promptset/motorcontrol-can`; the firmware diagnostic was committed as `e8b449d`. Siblings unchanged since the authoring rows, HEAD and dirty state checked on 2026-11-06: MotorControl-RS `cd27b12` and ADS1115 `d613a13`, both clean. No RS timing value was imported; the runner keeps its own bounds.

## What was built

| Item | Delivered |
| --- | --- |
| 1 Offline comparison of an independent UART capture with `rx.bin` | `scripts/s2_uart_compare.py --format raw --host rx.bin --external capture.bin`, optional per-side times CSV (`offset`, `bytes`, `monotonic_s` or `utc`, optional receiver `errors`), `.xz` inputs read in memory. Inputs are never modified. Its help states the connection: receive only on GPIO43 (S2 UART0 TX), common ground, 115200 8N1, a logic analyzer that reports framing, parity and break errors preferred. A second USB-UART on the same host cannot separate a host-side cause from a bridge one |
| 2 Defensible alignment | Anchored line and byte alignment within bounded windows. Deletions, insertions, replacements, reorders, missing tails and corrupt lines are reported with offsets. Competing alignments, contested anchors and unanchored regions are reported as ambiguous or `unalignable`, never as an exact location. Cross-device time needs a shared marker or stated bounds. Input caps: `--max-bytes`, `--max-lines` (default 1,000,000, at most 2,000,000), times CSV at most 4,000,000 rows and 256 MiB. Measured: R1 at 17 MB a side in 3.4 s and 210 MiB peak; 2M short lines in 40 s and 442 MiB |
| 3 Firmware diagnostic | `ProbeIoStats::outputWritten`, the bytes the UART FIFO accepted (modulo 2^32). `SOAK_CON` ends with `uartBytes=` and `queued=` at formatting, so each line's position in the S2 output stream is known. `scripts/s2_console_account.py rx.bin` compares, per pair of `SOAK_CON` lines, the bytes the S2 admitted with those the host received. It separates formatting and console admission (`lines`, `conRej`, the line numbers) from FIFO acceptance (`uartBytes`), and those from host receipt (`rx.bin`). Overhead: one 32-bit increment per accepted byte, at most 39 bytes per summary (0.3% of the line at 1 s summaries), 32 bytes of line buffer; service budgets unchanged |
| 4 Tests | `s2_uart_compare_contract` (46 tests): identical data, a literal 64-byte deletion, partial-line deletion, repeated identical chunks, insertion and duplication, corruption, truncation, clock offset, missing markers, oversized input, an unavailable or junk external capture, receiver errors, and damage present in both captures. Retained R1 bursts are replayed through the real parser after a manifest hash check. `s2_console_account_contract` (23 tests): the same damage kinds against the counters, the 2^32 wrap, boots, implausible values, interleaving, chunk boundaries; the clean R3 and damaged R1 captures, which predate the counters, compare nothing. The native soak transcript must balance in every window (`s2_soak_transcript`) |

An independent reviewer challenged the comparison tool's alignment and causal claims (findings D1 to D7: competing alignments, start and end assumptions, line cap, damage on the external side, the times CSV). Each was fixed with a regression test. The same review corrected overstatements in the 2026-11-05 stress report, appended there as a correction table; the original text is unchanged.

## Reproduction, 2026-11-06

Predeclared before the runs: image `e8b449d` (flashed with `s2_flash.py`, ping matched the upload, ELF `e9696c5f…`), `s2_soak.py --duration-s 1200 --summary-s 1 --reads full --max-trials 1`, read interval 200 ms then 20 ms, nothing else changed. A clean 200 ms run would be inconclusive, not a fix.

| Run | Read interval | CAN outcome | Console outcome | Counters |
| --- | --- | --- | --- | --- |
| [A](../../bench/20261106-p5-a-reads200ms/) 15:05Z | 200 ms | 5990 of 5990 read operations succeeded, 0 timeouts, 0 lost receives, 0 TX rejections | No line lost | 1197 of 1197 windows balance |
| [B](../../bench/20261106-p5-b-reads20ms/) 15:26Z | 20 ms | 40727 of 40727 read operations succeeded, 0 timeouts, 0 lost receives, 0 TX rejections; 15573 slips, by design at this interval | 31 lines lost in 24 gaps; 24 lines malformed | 23 of 1188 windows short by 1312 bytes in all, none long: 18 × 64 bytes, 1 × 48, 3 × 32, 1 × 16 |

Every missing byte in B had been accepted by the S2's UART FIFO, because the window's closing `SOAK_CON` line arrived and the FIFO sends in order. The loss therefore happened after `uart_tx_chars` accepted the bytes: on the UART line, in the ESP-Prog-2 bridge, in USB or in the host driver. These counters cannot tell those apart. Formatting rejection and console admission are excluded by `lines`, `conRej=0` and the contiguous line numbers. All losses are multiples of 16 bytes and at most 64, as in R1. Whether that granularity comes from the bridge, USB packets or the firmware's 16-byte feed per turn is a hypothesis, not shown.

Run A's verdict was `failed-evidence` because of two status anomalies. These were each node's first heartbeat after the reflash, NMT going from unknown (`ff`) to Pre-operational (`7f`). The drives have sent heartbeats since the PDO sessions set `1017=1000`. Nothing in them is console-related. Run B's verdict was `incomplete`, from the lost lines.

## Defect found in the soak record

Run B's first summary showed heartbeat gaps of 38 s (node 1) and 39 s (node 2). The drives' heartbeats were steady. Cause: `Soak::begin` cleared the soak's state, but the owner's network-observation copies from between the two runs were still queued, so the new soak measured its first gap from a heartbeat received before it began. A stale boot-up or EMCY would likewise have been reported as an event of the new soak. Earlier soaks ran with no heartbeat producer configured, so no retained heartbeat figure is affected. `Soak::begin` now drains those copies, counts them as `stale=` in `SOAK_BEGIN` (optional in the grammar), and never measures or reports them. Test: `testObservationsBeforeBeginBelongToNoSoak` (an accepted heartbeat and an EMCY before begin). It fails without the drain (3 heartbeats, the stale gap) and without the count. The fix is in the next bench image and needs a short soak regression there.

## Requirements

| Item | Status |
| --- | --- |
| 1, 2, 4 | Met, with the independent review above |
| 3 | Met: FIFO acceptance is now observable separately from admission and host receipt |
| 5 | Met: one 200 ms baseline and one bounded 20 ms reproduction on the same image |
| Verify | Offline tests registered; the diagnostic was built and flashed, and the runs above are its read regression. The stale-observation fix is built and host-tested, not yet flashed |
| Attribution | Unresolved: no independent receiver at GPIO43 was available |

## Next measurement

A logic analyzer on GPIO43 that decodes 8N1 at 115200 baud and reports framing, parity and break errors, sampling at 1 MS/s or more, with common ground and receive only. Start it before the host opens COM15 and stop it after the host closes it. Run a 20 ms soak like run B, export the decoded bytes as a raw binary in wire order, optionally with a times CSV, and run `s2_uart_compare.py --format raw --host trial-01/rx.bin --external <capture>`. If the lost bytes are on the wire, the bridge, USB or the host driver lost them. Telling those apart then needs a USB-level observation, or a different bridge or host. If they are not on the wire, the S2's UART output is the next suspect.
