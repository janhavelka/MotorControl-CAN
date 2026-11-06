# PDO stream on the bench, 2026-11-06

The user asked for all relevant PDO testing on the two CL86-C drives: start the actual PDO stream, check that the setup works and whether it can be stopped, and document the timings and whatever else matters for later use of these drives. **One firmware identity, two specimens; nothing here qualifies a firmware, a production PDO capability or a sample.** It follows the [motion and stress report](2026-11-05_motion_and_stress.md).

## Fixture and software

| Item | Value |
| --- | --- |
| Fixture | ESP-Prog-2 on COM15 (USB serial `94A990398B40`), ESP32-S2, TJA1051, two CL86-C, nodes 1 and 2, 1 Mbit/s, motors fixed to the table, shafts free |
| Drive state at the start | Both drives powered since the user's power cycle of 2026-11-05 15:36Z, both in NMT Pre-operational. Node 1 enabled and holding at 4728 (`6041=0037`), node 2 released at 821 (`6041=4050`, `2100=0014`) |
| Images | `4eb7683` ([upload](../bench/20261106T1300Z-s2-upload/build.json)) for the first three sessions; `56e12ae` ([upload](../bench/20261106T1309Z-s2-upload/build.json)), with the watch fix below, for the rest. Both built from a clean worktree and checked by ping against the build record |
| Tools | New in `4eb7683`: the bench firmware's `N`, `P`, `R` and `W` lines, the frame watch and `scripts/s2_pdo.py` ([bench README](../../examples/s2_bench/README.md#pdo-stream-sessions), [CHANGELOG](../../CHANGELOG.md)). Times are the S2's monotonic microseconds; a frame's time is its ISR clock sample, an upper bound, never a bus timestamp |

## Sessions

Each directory holds `plan.json`, `session.json`, `rx.bin` and `rx.txt`; `python scripts/s2_pdo.py analyze DIR` reproduces every number below.

| Session | Image | What it did | Outcome |
| --- | --- | --- | --- |
| [`1301Z-pdo-setup`](../bench/20261106T1301Z-pdo-setup/rx.txt) | `4eb7683` | Heartbeat `1017` 0 → 1000 on both nodes, then all 16 PDO records (TPDO and RPDO sets 1..4) of both nodes | Completed; 72 entries per node read |
| [`1302Z-pdo-node2-first`](../bench/20261106T1302Z-pdo-node2-first/rx.txt) | `4eb7683` | Node 2: Start, TPDO1 event timer 0 → 100, Pre-operational, timer back to 0 | Completed |
| [`1303Z-pdo-node2-timing`](../bench/20261106T1303Z-pdo-node2-timing/rx.txt) | `4eb7683` | Node 2: Start, event timer 0 → 10 | Write succeeded; the plan stopped (`no_end`), see the watch defect below. Node 2 left Operational at 10 ms |
| [`1304Z`](../bench/20261106T1304Z-pdo-node2-restore/rx.txt), [`1305Zb-pdo-node2-restore`](../bench/20261106T1305Zb-pdo-node2-restore/rx.txt) | `4eb7683` | Recovery: release the held record (the first try was refused because of it), node 2 to Pre-operational, timer 10 → 0 | Completed |
| [`1311Z-pdo-node2-event-timer`](../bench/20261106T1311Z-pdo-node2-event-timer/rx.txt) | `56e12ae` | Fresh identity and state of both nodes, TPDO1 baseline, node 2 Start, 10 s unchanged, timer 100 and 200 for 10 s each, Pre-operational, Start, Stop, Start from Stopped, Pre-operational | Completed |
| [`1314Z-pdo-node2-restore-timer`](../bench/20261106T1314Z-pdo-node2-restore-timer/rx.txt) | `56e12ae` | Separate restoration: node 2 timer 200 → 0, record read back | Completed |
| [`1314Z-pdo-node1-stream`](../bench/20261106T1314Z-pdo-node1-stream/rx.txt) | `56e12ae` | Node 1 (enabled, holding): Start, 10 s unchanged, timer 100 for 10 s, Pre-operational | Completed |
| [`1315Z-pdo-node1-restore-timer`](../bench/20261106T1315Z-pdo-node1-restore-timer/rx.txt) | `56e12ae` | Separate restoration: node 1 timer 100 → 0, record read back | Completed |
| [`1316Z-pdo-both-streams-soak`](../bench/20261106T1316Z-pdo-both-streams-soak/rx.txt) | `56e12ae` | Both nodes timer 100, both Started, a 60 s read soak at 20 ms, both Pre-operational | Completed |
| [`1317Z-pdo-both-restore`](../bench/20261106T1317Z-pdo-both-restore/rx.txt) | `56e12ae` | Separate restoration: both timers 100 → 0, both records read back | Completed |

**Final state:** both nodes Pre-operational, every TPDO1 entry back on its measured baseline (read back), node 1 still enabled and holding at 4728, node 2 released at 821. **One change remains:** the heartbeat producer `1017` is 1000 on both nodes, not the 0 they started with. No route writes it back to 0; on 2026-11-02 it reverted at the next power-up.

## The live PDO configuration

Read on both nodes on 2026-11-06, identical apart from the node ID in each COB-ID. Every entry answered at its EDS width (`:01` four bytes, `:02` one, `:03` and `:05` two, map count one, map words four), so the exact reads of the write experiment cannot hit a width abort.

| Set | COB-ID (node 1/2) | Type | Inhibit raw | Event raw | Map, as read | EDS default |
| --- | --- | --- | --- | --- | --- | --- |
| TPDO1 | 181 / 182, valid | 255 | 100 | 0 | `6041:00` 16, `6061:00` 8, `6064:00` 32 bits (7 bytes) | inhibit 0, event 100; map `6041`, `60FD`, `6061` |
| TPDO2 | 281 / 282, valid | 255 | 100 | 0 | empty (count 0) | event 100; map `6064`, `606C` |
| TPDO3 | 381 / 382, valid | 255 | 100 | 0 | empty | event 100; empty |
| TPDO4 | 481 / 482, valid | 255 | 100 | 0 | empty | event 100; empty |
| RPDO1 | 201 / 202, valid | 255 | 0 | 1000 | `6040:00` 16, `6060:00` 8, `6098:00` 8, `607A:00` 32 bits (8 bytes) | map `6040`, `60FF`, `6060` |
| RPDO2 | 301 / 302, valid | 255 | 0 | 1000 | empty | map `607A`, `6081` |
| RPDO3, RPDO4 | 401/402, 501/502, valid | 255 | 0 | 1000 | empty | empty |

**The EDS defaults are not the live configuration.** TPDO1's map and timers were already known to differ (2026-11-02); TPDO2 and RPDO1/RPDO2 now also differ, and only TPDO1 carries data. RPDO1 maps the controlword, mode, homing method and target position: a PDO producer on this bus could command motion. This session sent no RPDO.

## What the drives did

**Boot state.** Both nodes' heartbeats read `7F`, Pre-operational, before any NMT command, confirmed once a second.

**Start.** On each addressed Start, Start from Stopped included, the node sent one frame on every TPDO within about 0.1 ms: TPDO1 with its 7 bytes, TPDO2..4 with no data (DLC 0). The heartbeat reported `05` at its next scheduled beat, 0.07 to 1.0 s later. The heartbeat keeps its own schedule and is not sent on a transition.

**Event timer 0 with type 255.** After the Start frame nothing more came while the drive stood still: 10 s on each node. Whether a change of a mapped value sends a frame was not tested; that needs motion or a hand turn.

**Event timer writes.** Every write of `1800:05` was accepted in Operational and in Pre-operational, without an abort. In Operational the drive sent one TPDO1 frame immediately, about 0.6 ms before the S2 had processed its SDO download response, and the new period ran from that frame.

**Periods.** Measured on the S2's clock, from the watch's per-frame times; the heartbeat gives a fourth point:

| Raw value | Gaps measured | Period p50 | Spread | Session |
| --- | --- | --- | --- | --- |
| `1800:05` = 10 | 1,365 (device count) | 11.00 ms mean; printed gaps 10.84..11.08 ms | | `1303Z-pdo-node2-timing` |
| `1800:05` = 100 | 101 / 51 / 100 | 100.973 / 100.973 / 100.984 ms | sd 0.018..0.026 ms | node 2 (two sessions), node 1 |
| `1800:05` = 200 | 49 + 23 + 23 | 200.955 ms | sd 0.009..0.011 ms | `1311Z-pdo-node2-event-timer` |
| `1017` = 1000 | 58 per node | 1000.795 ms | sd 0.02 ms | heartbeats, both nodes |

All four fit one model within 0.03 ms: **period = (raw value + 1) × 1 ms on a drive clock 0.018% fast against the S2's** (10 → 10.998, 100 → 100.98, 200 → 200.96, 1000 → 1000.82 ms). So the event timer counts milliseconds, and each period is one millisecond longer than the raw value. This is a fit to four values on one firmware, not a documented rule. The 0.018% is relative to the S2's crystal, which was not calibrated.

**Pre-operational and Stop.** Both ended the stream at once: the last frame was the last one scheduled before the request (3 to 95 ms before its TX was confirmed, depending on the phase), and none followed. In Stopped the heartbeat continued, reporting `04`, and the S2 sent no SDO, as the owner refuses SDO to a Stopped node. Start from Stopped resumed the stream with the timer as configured, its first period counted from the Start frame.

**Drive state through every transition.** Neither drive's state changed. Node 1's TPDO1 read `6041=0037`, `6061=1`, `6064=4728` in every frame: enabled and holding, through about 20 s in Operational with RPDO1's 1000 ms event timer running and no RPDO received. Node 2 read `4050`, `1`, `821` in every frame. No EMCY appeared in any session. **Start and Pre-operational on node 1 (enabled), and Start, Pre-operational and Stop on node 2 (released), changed neither drive's CiA402 state or position,** and the receive PDO timer raised nothing without any RPDO. Stop was not sent to the enabled node. These are observations, not a host-loss or watchdog behavior: none was tested.

**Content.** Every TPDO1 payload decoded exactly by the map read in the same session, and matched the SDO reads of `6041`, `6061` and `6064`.

## Both streams during a read soak

Node 1 and node 2 at timer 100 in Operational, while a 60 s soak read both nodes every 20 ms ([session](../bench/20261106T1316Z-pdo-both-streams-soak/rx.txt)):

- **PDO:** 650 frames per node, mean period 100.977 ms, sd 43 to 47 µs against 9 to 26 µs on a quiet bus. The extra spread includes the S2's own service latency under load.
- **SDO:** 784 and 783 operations, all successful, no timeout; TEC and REC 0. The slowest operation took 11.9 ms, against 6.9 ms with no stream, and the mean rose from 4.9 to 5.7 ms.
- **The soak harness copes badly with PDO traffic.** It counts each PDO frame as unmatched, an anomaly. It logged 654 transition lines for it, held reads for 7.5 s of anomaly dumps, and its console refused 10,350 lines. Its read rate fell to 13 per second per node, from 17. The owner's 4-entry network history dropped 927 unmatched observations, and its trace was overwritten 730 times. The watch still lost nothing on the tap, and only 4 frame lines went unprinted. This is a limit of the diagnostic harness, which assumes a bus without PDOs, not a CAN or library failure; recorded as backlog S13.

## Defects found and fixed

- **The watch starved record dumps** (`1303Z`). At 10 ms one stream fills the S2's 11.5 kB/s console on its own. The watch printed one frame line per turn whenever there was room, so a write record's dump took 15 s and the runner gave up on a write that had succeeded, leaving node 2 Operational at 10 ms and the record held. Fixed in `56e12ae`: the watch holds its lines while a dump, a network listing or an NMT result waits; held frames still count. Retested in every later session: no step waited on output.
- **The runner did not check terminals.** Packet 6 of the [bench continuation](../prompts/bench_closure_18_33/6_pdo_measurement.md) found that `run_steps` treated a step as answered on its end line, so a failed write or an untransmitted NMT request would not have stopped the plan. Fixed in `05fd03e`, before the `56e12ae` sessions: each terminal is parsed and must show success on the right node and command, or the plan ends. No failed terminal occurred in any session, before or after.
- **Planning error:** the first recovery try (`1304Z`) was refused because the held write record blocks NMT, correctly; the second released it first.

## For later use of these drives

- **To get a periodic stream:** set `1800:05` (TPDO1 event timer) in Pre-operational or Operational, then Start. Expect one frame on every TPDO at Start, then TPDO1 every raw + 1 ms. TPDO2..4 carry nothing until remapped, and every Start still sends them as empty frames.
- **To stop it:** Enter Pre-operational (SDO stays available) or Stop (SDO refused until Pre-operational or Start). Both act at once, and neither changes the drive's CiA402 state. Neither is a motor stop.
- **Watch the heartbeat, not the frames, for the NMT state,** and allow up to one heartbeat period for it to report a transition.
- **Budget:** a 10 ms TPDO is 100 frames per second per node; the S2 handles it, but its console cannot print every frame. The device statistics stay complete.
- **Do not assume the EDS defaults.** Read the live records first; on these specimens they differ in maps and timers.
- **RPDO1 maps the controlword and target position.** Any RPDO producer on the bus can command motion once a node is Operational.

## Not tested, and why

| Question | Why not | Next step |
| --- | --- | --- |
| Change-driven TPDO (type 255, timer 0) | Needs a mapped value to change: motion or a hand turn | One finite move or a user hand turn, with the watch running |
| Inhibit time `1800:03` | Periodic traffic cannot show it; packet 6 requires a faster evidenced event source | Same, with timer 0 and a known change rate |
| Transmission types other than 255, SYNC | Not swept, per packet 6; no SYNC producer on the bus | A reviewed single-type experiment |
| COB-ID valid bit `1800:01` bit 31 | Left out of this round | One write pair, under its own plan |
| RPDO | No host producer; RPDO1 commands motion | Prompt 18 and 38 work, with motion review |
| Persistence of `1800:05` | Every change was restored by an explicit write | A power cycle after a change, with a read |
| Exact frame times | The S2 gives receive bounds only | A listen-only analyzer with hardware timestamps |

## Relation to the bench continuation, packet 6

The user's [bench continuation](../prompts/bench_closure_18_33/README.md) appeared at 12:48Z, during this work, and was not assigned to this session. Its packet 6 covers the same measurement, so its stricter rules were adopted from the `56e12ae` sessions on: the terminal check above, one node and TPDO1 first, an unchanged baseline in Operational, timer candidates 100 and 200 with 10 s watches, no transmission-type sweep, no SYNC or RPDO, and restoration as a separate explicit action. Before it was seen, timer 10 had been measured, which exceeds its two-candidate bound; node 1 and the combined soak followed it as further single-node cycles. Not done here: packet 1, packet 6's prerequisite; the ledger's Q17 update; and the `packet_6.md` record and transition record for packet 7. This report and its sessions are the input for them.
