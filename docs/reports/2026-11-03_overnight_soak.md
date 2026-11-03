# Overnight communication soak, 2026-11-03

Communication only: no enable, motion, NMT, PDO or target write. Each read is an `accepted_not_qualified` object (DEC-1), and `603F`, `1001`, `1003` are never read. Interim; final figures are added at the end of the run.

## Setup

| Item | Value |
| --- | --- |
| Fixture | ESP-Prog2 on COM15, serial `94A990398B40`, ESP32-S2, nodes 1 and 2 at 1 Mbit/s |
| Image | Built from `fcea8b4`, flashed by `scripts/s2_flash.py`, verified by `ping` (`docs/bench/20261103T011449Z-s2-upload`) |
| Run | `python scripts/s2_soak.py --until 09:00 --interval-ms 200 --summary-s 60 --reads full --output docs/bench/20261103-soak-night2` |
| Reads | `6041`, `6061`, `6064`, `606C`; then `2100`, `2101`, `2102`; one identity read per node with `MEASURED_U8` |
| Operation | One at a time, alternating nodes, 2 s deadline, 250 ms queue and response budgets, failures retained and never retried |

## Earlier runs

| Run | Image | Duration | Operations per node | Anomalies |
| --- | --- | --- | --- | --- |
| `20261103-soak-pilot1` | Dirty, `b0b5417` plus work | 120 s | 297 of 297 | 0 |
| `20261103-soak-night1` | Same | About 15 min, stopped for a clean reflash | 1,920 of 1,920 | 0 |

## Interim at 03:42

84 minutes in: **12,600 of 12,600 operations and 44,106 of 44,106 SDO steps, zero anomalies.**

| Measure | Value |
| --- | --- |
| RX and TX frames | 88,286 each; no TX rejection, RX drop, loss or unmatched frame |
| Timeouts, cancellations, bus-off, recovery | 0 |
| TWAI errors and flags; TEC, REC | 0; 0, 0 |
| Max service gap | 3,788 us; owner gap 3,995 us |
| Queue, active and retained high water | 1, 1, 1 |
| Stack free, internal heap | 9,296 B; 128,388 B, minimum 123,948 B |
| Status transitions after the first observation | None |

The only transitions recorded are each signal's first observation. Every status value held, including `2100=1` on node 2, through the run. No heartbeat was seen: the drives' heartbeat producer is not established, so freshness stays producer-unknown, as designed.

## What this does and does not show

It shows that the ordinary typed-read path, owner and TWAI adapter sustain this load on these two specimens without a fault. It does not cover TPDO or bus load, arbitration loss, bus errors, a no-ACK, recovery, or any write. Those are packet 3's questions, and most need load or an observer this fixture lacks.
