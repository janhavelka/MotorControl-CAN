# Packet 11: bounded HIL diagnostics and repeatable evidence

Executes prompt 29's diagnostic tooling and fresh baseline acquisition. No enable, motion, NMT, heartbeat, PDO or target write ran. **A soak is communication evidence on two specimens and one firmware image. It is not electrical, sustained-load, per-family or motion qualification.**

## Delivered

- **`scripts/s2_flash.py`.** Builds with the pinned tools, requires the recorded USB identity and chip MAC, checks generated flash arguments and the embedded ELF hash, and retains hashes and logs. Ran on real hardware twice. Identity after flash comes from `ping` plus `--check-ping`.
- **`scripts/s2_evidence.py`.** Offline parser with one record-grammar table. Replays retained nodes 1 and 2: 30 terminals and 90 traffic records each, and extracts a transition log. Unknown lines are preserved and counted.
- **Instrumentation.** Records distinguish queue admission, adapter handoff, SDK submission, TX report, RX bounds, terminal and retirement. `END network` no longer drops under console pressure, and `TRAFFIC`, `RESULT`, `STATS` and `PONG` gained controller state, epochs and time.
- **Soak mode**, in `examples/s2_bench/Soak.{h,cpp}` and `scripts/s2_soak.py`. It uses the ordinary owner and typed reads, one operation at a time, alternating nodes, and tracks every health, status, controller and NMT transition. The first anomalies of each kind keep full trace dumps. See the [bench guide](../../hardware_bench.md).

A soak **reads only** `6041`, `6061`, `6064`, `606C`, `2100`, `2101`, `2102`, and one identity read per node, all `accepted_not_qualified`. It never reads `603F`, `1001` or `1003`. Nothing reopens a route, retries or restarts: an anomaly stops that node's reads and keeps its evidence.

## Baseline

Fixture by software identity: ESP-Prog2 on COM15, serial `94A990398B40`, VID/PID `303A:1002`, ESP32-S2, nodes 1 and 2 at 1 Mbit/s. Image from clean `fcea8b4`, verified by `ping`. The operator's power cutoff and shaft state cannot be seen from software; no state-changing command ran.

| Procedure | Node 1 | Node 2 |
| --- | --- | --- |
| Typed identity with `MEASURED_U8` | 9 of 9, first live typed success | 9 of 9 |
| Readiness, 30 uploads | 30 of 30, no failure or uncertainty | 30 of 30 |
| Readiness against `20261102-final-node1/2` | **All 30 fields identical** | **All 30 fields identical** |

This closes packet 4's live comparison: `1009` and `100A` answer in one byte, still recorded as a difference from the EDS's two.

Pilot soak, 120 s, on the earlier dirty image: 297 of 297 operations per node. A 15-minute continuation: 1,920 of 1,920 per node. No anomaly, zero TX rejections or drops, `gapMax` under 4 ms, `stackFree` about 9.3 KB.

## Parser defects found on the first real typed success

A typed read succeeding had never been captured, so `outcome=1` was read as a failure, marking the capture inconsistent and then failed. The header reports `ReadOutcome`, where 1 is success, and the parser used the SDO table. Fixed, with a regression on the live captures.

## Physical questions

Packet 3's seven:

| Question | Status |
| --- | --- |
| 1. Failed TX and `on_error` under TPDO load | **Not answered.** The soak gives a no-load baseline only: `twaiErrors`, flags, TEC and REC |
| 2. No-ACK retry loop | Only if one happens. Not injected |
| 3. Lost TX command against a pending frame | Visible if it occurs: a submission with no report and no error delta |
| 4. `twai_node_disable` stopping a retrying frame | **Not answered.** Needs a listen-only observer |
| 5. `is_tx_success` on a bus-off TX-done | Only on a natural bus-off |
| 6. FIFO on DLC 9..15 | **Not answered.** Nothing on this bus sends such frames |
| 7. Duplicate delivery after a late error | Observational only |

From packet 5, a reply overtaking its TX report is now counted as `earlyReply`, with `retireMax` beside it.

## Q18, the drain marker's premise

**Unresolved, and no real route can reopen.** The Lichuan manuals and EDS are silent on SDO concurrency or order, and CiA 301 is not retained. A soak cannot answer: it reads one at a time and never issues a slow write. A read-only exchange shows only that replies matched while nothing was outstanding, and one passing exchange cannot establish universal ordering.

The missing observation is a slow write then a read, with a new request while one is pending and duplicate delivery watched, seen by a verified listen-only observer. No such observer is verified on this fixture, and inducing an uncertain write to manufacture the test is forbidden. So packets that need a reopened route must plan for an MCU restart.

## Other open items

- `603F`, `1001`, `1003` first read with LED state before and after still needs an operator. Readiness read them once per node, values zero, with no LED observation. `motion-readiness` reads `2100` monitors too: all as before.
- The image is not byte-reproducible from a clean tree. Identity is the retained hash of each build, and `CONFIG_APP_REPRODUCIBLE_BUILD` is untested.
- `Soak` allocates 10,752 bytes from internal heap on first use. `dram0_0_seg` has about 2.5 KB free, so the first firmware holding an `AxisSession` must use the heap too.

## Sibling refresh

| Sibling | Revision | Dirty | Seam | Disposition |
| --- | --- | --- | --- | --- |
| MotorControl-RS | `e59c5ca` | Clean | `scripts/bench_session.py`, `_bench_evidence.py`, `bench_probe.py` | Adopted: port ownership, retained errors, correlation. RS's retry and cleanup choices are not adopted |
| PCA9555 | `414e0f6` | Clean | Dry-run guard | Adopted for the tools' `--dry-run` |
| FieldCore-node | `5ad9292` | Clean | Copied request and result | No CAN runner there; read-only |

## Checks

See the [execution table](README.md). Overnight results are in the [soak report](../2026-11-03_overnight_soak.md).
