# Packet 6: PDO measurement record

[Packet 6](../../prompts/bench_closure_18_33/6_pdo_measurement.md). The bench sessions ran on 2026-11-06 between 13:01Z and 13:17Z, on the user's direct request for PDO testing, before the continuation was assigned; this record was written after its assignment and after an independent review. The measurements and their narrowed reading are in the [PDO stream report](../2026-11-06_pdo_stream.md). Back to the [execution index](README.md).

## Start state

Both CL86-C drives powered since the user's power cycle of 2026-11-05 15:36Z, both NMT Pre-operational (heartbeat `7F` once `1017` was set). Node 1 enabled at 4728 (`6041=0037`), node 2 released at 821 (`6041=4050`, `2100=0014`). Sibling audit unchanged from the authoring rows (MotorControl-RS `cd27b12`, INA228 `73f9548`, FieldCore-node `5ad9292d`): RS settings prior-state and partial-effect handling is matched by the exact-baseline write (no write when the baseline differs, uncertainty kept after a possible TX).

## Sessions, images and runners

| Sessions | Image | Runner | Conformance to this packet |
| --- | --- | --- | --- |
| `1301Z` to `1305Zb` | `4eb7683` | `6a82d39` plus uncommitted edits (`host_dirty` true) | Before the packet's rules were seen. No terminal check: a step counted as answered on its end line. Every terminal did succeed in fact; `1303Z` stopped on the watch defect |
| `1311Z` to `1317Z` | `56e12ae` | `05fd03e` plus the uncommitted `3`/`4` identity-read support, committed later in `89a7eea` | Terminal check present, still keyed on the plan's expect text (P1 below). Packet 6's limits followed from `1311Z` on |

Which runner checked `1311Z`'s identity steps cannot be shown from the record; both succeeded (`rx.txt` lines 4 and 116). Sessions now record the SHA-256 of `s2_pdo.py` and `s2_evidence.py` and a tracked-files-only dirty state.

## Deviations from the packet

- The independent review of item 1 happened after the hardware runs, not before.
- Timer raw 10 was measured (`1303Z`) beyond the two-candidate bound. The `1303Z` plan also held timer 1000 and 1, inhibit 100 to 0 while the PDO was valid, the COB-ID valid bit and transmission types 254 and 1; only the watch starvation stopped it after its first write.
- `1302Z` restored the timer inside the same plan, not as a separate action.
- Dry runs were made but not retained; baseline values were checked by hand, not gated by the runner.
- `1316Z` and `1317Z` wrote with no watch running and took Pre-operational from earlier sessions' heartbeats.
- Node 1 was not read after its final Pre-operational; its last observation is the `1316Z` soak, ending 2 s earlier.

## Requirements

| Item | Status |
| --- | --- |
| 1 Review before hardware | Partial: reviewed afterwards. Confirmed properties: a missing or wrong baseline sends no write (`RawEvidence.cpp` exact read before the download; `raw_evidence_test`); a possibly transmitted write keeps its uncertainty in the record; lost evidence ends `EVIDENCE_LOSS`; cleanup sends only `P0`; the tap copies without consuming either node's traffic. Defects P1 to P5 below |
| 2 Plan, dry-run, TPDO1 baseline | Met in `1311Z`: fresh identity and state of both nodes, exact TPDO1 communication and map read; dry-run not retained |
| 3 Baseline, NMT prerequisites, final state | Met by hand: every `N` step followed the heartbeat state it required; the final intended state, Pre-operational, was reached on both nodes |
| 4 One field, two values, separate restore | Met in `1311Z` (`1800:05` raw 100, then 200, 10 s each; restoration in `1314Z` as its own session); not met before it (see deviations) |
| 5 Acceptance, timing, units kept apart | Met after narrowing: value acceptance by exact readback; timing as measured intervals with their bounds; units not established |
| 6 Transition record | Below |

## Review findings and fixes

- **P1** The runner's terminal check followed the plan's own expect text, so `expect: "NMT"` skipped the NMT check. Now each checked command (`N`, `R`, `W`, `h`, `j`, `3`, `4`) must name its exact admission and end lines, and the check follows the command letter.
- **P2** `q`, `w` (NMT Pre-operational) and `m` (the fixed TPDO1 experiment) were allowed with no checked terminal. They are no longer offered in plans.
- **P3** Session provenance did not name the tools that ran. Sessions now record their SHA-256 values.
- **P4** The NMT result line had only the TX completion time, so a frame received just before it could precede or follow the NMT frame on the wire. The firmware now prints the admission time (`admitted=`), and the analysis counts frames between the two as ambiguous, never on either side. This is in the next bench image; the retained sessions keep the old split, recorded as ambiguous where it matters (`1302Z`).
- **P5** No step checked its prerequisite state. A step can now carry `require`: the latest record of a kind (for example `NETWORK` for node 2, or the `RESULT` of `1800:05`) must hold stated values, or the plan stops unsent.

Tests: `s2_pdo_contract` (17 tests, each fix with its regression; the requirement gate was mutation-checked); the firmware change is in the S2 build.

## Transition record for packet 7

| Field | Record |
| --- | --- |
| Specimens | CL86-C nodes 1 and 2 of fixture OPTAK-S2-20261102, one firmware identity (`1000=0x00040192`, `1009=0x0C`, `100A=0x20`, `1018:01..03=0x1000/1/0`, `2000=0x0400`, `2001=0x0200`) |
| Baseline | `1800:01=0x18N` valid, `:02=255`, `:03=100`, `:05=0`; `1A00` count 3, `60410010 60610008 60640020`, slot 4 zero; TPDO2..4 empty; RPDO1 maps `6040/6060/6098/607A`; `1017=1000` until the next power cycle |
| Measured transitions of `1800:05` | 0 to 100 and 100 to 0: both nodes, Pre-operational and Operational. 100 to 200, 0 to 10: node 2, Operational. 200 to 0 and 10 to 0: node 2, Pre-operational. Each with exact baseline read, one acknowledged download and exact readback. Stopped not tried |
| Sequence | Fresh state and heartbeat, `R` read of the set, exact `W`, release, addressed Start when a stream is wanted, Pre-operational, then a separately checked restoration |
| Failure behavior | Baseline mismatch: no download (`READBACK`). Server abort: `SERVICE`, abort code kept, effect uncertain. Readback mismatch: `READBACK`. Evidence loss: `EVIDENCE_LOSS`. No rollback or retry. Abort, timeout and mismatch are fake-tested only; none occurred on the bench |
| Timing, supported | With the timer nonzero and type 255, TPDO1 is periodic in Operational; periods consistent with raw + 1 ms for raw 10, 100 and 200 on the S2's clock. A write in Operational is followed by one frame between request and response |
| Timing, not established | The unit and its origin, any other raw value, raw 0's meaning beyond "no periodic frame while stationary", inhibit, other transmission types, change-driven sending, persistence |
| Samples | None qualified: S2 receive bounds only, no PDO route configured, constant payloads on a reused COB-ID. A confirmed map is not a fresh sample |
| Captures | `1311Z` `rx.txt` lines 293-298, 336-354, 494-511; `1302Z` lines 126-131; `1316Z` line 447; restorations `1314Z`, `1315Z`, `1317Z` |

Q17 in the ledger now carries these facts as observations; its gate stays open.

## NMT boundary rerun, 2026-11-06 16:16Z

On image `f2c90ab`, whose NMT line prints its admission time, in the packet 7 session [1616Z-d](../../bench/20261106T1616Z-pdo-p7-replay-d/). Node 2 was Operational with TPDO1 at raw 100. The watch was running and `N2,p` was admitted at 344172616 µs, TX completed at 344177947 µs (5.3 ms later). The last TPDO1 frame arrived at 344126913 µs, 45.7 ms before admission. No frame fell between admission and completion, and none followed in 3 s. The next periodic slot, about 344227890 µs, lay after completion. Node 2's heartbeat changed from `05` to `7f` 236 ms after completion. At the Start in [1615Z-c](../../bench/20261106T1615Z-pdo-p7-replay-c/), the frames each TPDO sends at Start fell between admission and completion and are counted as ambiguous. The first periodic TPDO1 frame came 100.1 ms after completion. One boundary on one node, on the S2's receive clock; it narrows packet 6's split and establishes no general rule.

## Next measurements

- A change-driven frame through one finite move, with the watch running.
- Inhibit through a faster evidenced event source.
- Persistence: one power cycle after a change, then a read.
