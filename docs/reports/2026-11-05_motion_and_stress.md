# Motion campaign and overnight communication stress, 2026-11-05

The user asked for every currently possible drive setting and motion to be run with the existing code: speeds, accelerations, normal and fast stops, and microstepping, watching for any unexpected behavior, error or state change. That was followed by a communication stress campaign until 14:00 local the next day. **One firmware, two specimens; nothing here qualifies a firmware or a production capability.** It continues the [2026-11-04 session](2026-11-04_bench.md).

**Not run: microstepping.** `2201` (command pulses per revolution) has no typed write in the library: its gate stays closed because the manuals give contradictory ranges (`0..65535` against `400..51200`) and no effect, and the request was not to add code ([packet 12](handoff_19_33/packet_12.md)).

**The overnight campaign started a day late.** A blocking question asking for the drive power cycle, sent at 19:08Z on 2026-11-04, was answered at 15:36Z on 2026-11-05, and the bench stayed idle meanwhile. The communication stress therefore ran from 15:42Z on 2026-11-05 to 13:00Z on 2026-11-06.

## Motion campaign

| Item | Value |
| --- | --- |
| Fixture | As on 2026-11-04: ESP-Prog-2 on COM15, ESP32-S2, two CL86-C, nodes 1 and 2, 1 Mbit/s, motors fixed to the table, shafts free |
| Image | `2920479` ([upload](../bench/20261104T1849Z-s2-upload/build.json)), reset by esptool, controller started |
| Power-up | The user power-cycled both drives at about 15:36Z. The shafts were not checked by hand, at the user's instruction: the enable's power-up observation was taken from the drives' own reports (low bits 111, `2100` released clear), the state every earlier power-up read as holding |
| Ledger | [`20261105-campaign.jsonl`](../bench/20261105-campaign.jsonl), 12 motion admissions, the firmware's per-boot cap |
| Observation | Drive reports only |

| Trial | Node | Case | Result |
| --- | --- | --- | --- |
| [701](../bench/20261105T1537Z-p1-node1-701/rx.txt), [702](../bench/20261105T1537Z-p1-node2-702/rx.txt) | 1, 2 | Reads | Both at the power-up state: `8037`, `2100=0001`, mode 0, `6064=0`, no error |
| 703, 704 | 1 | Mode 1, enable | As on every power-up: 6 → `8031`/`0011`, 7 → `8033`/`0001`, 15 → `8437`/`0001` |
| [705](../bench/20261105T1538Z-move10-node1-705/rx.txt) | 1 | Library move 0 → 1000, **10 rpm**, ramps 200/200 ms | `SUCCEEDED` at **1000**: `6064` 0, 241, 496, 751, 998, 1000 at 1.55 s cycles, about 165 pulses/s; `606C` 11, 9, 9, 1 |
| [706](../bench/20261105T1539Z-move60neg-node1-706/rx.txt) | 1 | Library move 1000 → −1000, **60 rpm**, ramps 1000/1000 ms | `SUCCEEDED`, **completion reported at −999**; `606C` −60 and −54: signed rpm. A read 40 s later ([707](../bench/20261105T1540Z-p1-node1-707/rx.txt)) gave −1000 with `607A=−1000` |
| [708](../bench/20261105T1540Z-moverel120-node1-708/rx.txt) | 1 | Library **relative** move +3000, **120 rpm**, ramps 100/100 ms, base `ACTUAL` | `SUCCEEDED` at **2000** exactly; `606C=120` at speed |
| [709](../bench/20261105T1540Z-position300-halt-node1-709/rx.txt) | 1 | Position 2000 → 12000, **300 rpm**, ramps 500/500, **HALT** after 2 cycles | `010F` at `6064=3336`, `606C=296`; running cleared about 0.6 s later, stop `OBSERVED` |
| [710](../bench/20261105T1541Z-rearm-halt-node1-710/rx.txt) | 1 | Rearm from the halt | Stopped at **4728**, 1,392 pulses after the `010F`; `000F` did not resume the move |
| 711, 712 | 2 | Mode 1, enable | As node 1 |
| [713](../bench/20261105T1541Z-position200-quick-node2-713/rx.txt) | 2 | Position 0 → 10000, **200 rpm**, ramps **2000/2000**, **quick stop** after 2 cycles | `000B` at `6064=854`, `606C=101`, still accelerating. Released at once with `2100=0014` (running latched), coasted to **896**, and two still cycles completed the stop `OBSERVED` |
| [714](../bench/20261105T1542Z-p1-node1-714/rx.txt), [715](../bench/20261105T1542Z-p1-node2-715/rx.txt) | 1, 2 | Reads | Node 1 enabled at 4728 (`0037`, `2100=0000`); node 2 released at 896 (`4050`, `2100=0014`) |

Every record: TEC and REC 0 at admission and terminal, `lost=0`, largest service gap 5 ms, least stack ever free 6,992 of 12,288 bytes. No EMCY, alarm, error register, refused route or console loss.

### What the drives showed

- **Speed and direction.** `6064` followed the commanded rate at 10, 60, 120, 200 and 300 rpm, and `606C` read the speed in signed rpm (−60 in the negative direction), lagging and averaging during ramps.
- **The in-place window is at least one pulse.** The 60 rpm move reported completion at −999 and settled on −1000 afterwards; on 2026-11-03 a move reported 251 and settled on 250. So `2100` bit 0 comes on within one pulse of the target, and the drive then settles onto it. A completion report is not the final position.
- **Deceleration follows the ramp setting.** The 300 rpm HALT with a 500 ms ramp-down took 1,392 pulses, close to the 1,250 that a linear ramp from 5,000 pulses/s gives, plus about 30 ms at speed before the drive reacted.
- **A fast stop releases the motor at any speed.** `000B` during acceleration at about 100 rpm released it, and the free shaft coasted 42 pulses. With a load, that coast is uncontrolled.
- **Clearing a halt never resumes it**, now at 300 rpm as at 6 rpm.

Nothing unexpected beyond these: no fault, no lost record, and every library outcome matched the drive's reports.

## Console-loss A/B, completed

Two runs of 3 h each with the drives answering, 20 ms reads, 60 s summaries, on `e7f28ac` and `2920479`:

| Run | Keepalive | Reads per node | Losses | When |
| --- | --- | --- | --- | --- |
| A [`20261104-soak-ab-plain3`](../bench/20261104-soak-ab-plain3/summary.txt) | Off | 186,169 | 2 in 180 bursts | 17:19:11, 17:50:11 |
| B [`20261104-soak-ab-keepalive3`](../bench/20261104-soak-ab-keepalive3/summary.txt) | 1 s, 10,568 newlines sent | 186,159 | 3 in 180 bursts | 19:55:53, 21:07:53, 21:12:53 |

Every loss, in both runs, was a 64-byte transfer in the first lines of a summary burst after about 60 s of quiet, the stress1 signature. **The host keepalive did not prevent it**, so the loss does not follow the host's own transmit idling. It still fits the S2-to-host direction idling: a bridge or driver buffer that drops the first transfer after a quiet spell. All reads succeeded in both runs; only console lines were lost, and the numbering detected each one.

## Overnight communication stress

To be completed as the runs end.
