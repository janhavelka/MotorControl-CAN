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

Every loss, in both runs, was a 64-byte transfer in the first lines of a summary burst after about 60 s of quiet, the stress1 signature. **The host keepalive did not prevent it**, so the loss does not follow the host's own transmit idling. That left the S2-to-host direction idling, which [R1 to R3](#overnight-communication-stress) then refuted: with under a second of quiet the loss rate per burst stayed the same, and it followed the 20 ms read load. All reads succeeded in both runs; only console lines were lost, and the numbering detected each one.

## Overnight communication stress

Read-only soaks on `2920479`, motion waived off: no enable, mode change, write or motion. Before each run the S2 was reset through esptool's hard reset and pinged twice. The first ping catches the boot banner and is kept only in disposable build output; the second is retained. Node 1 stayed enabled and holding at 4728, node 2 released at 896 with `2100=0014`, as the motion campaign left them.

### R1: 20 ms reads, 1 s summaries

[`20261105-stress-r1-summary1s`](../bench/20261105-stress-r1-summary1s/transitions.txt), 15:42Z to 18:42Z. Its capture, timing and summary files are stored as lossless xz; `COMPRESSED.json` holds the original SHA-256 values, and the `rx.bin` value matches the runner's record. R2 is stored the same way.

**CAN: clean.** 184,761 operations and 646,669 SDO steps per node, every one successful. No timeout, abort, TX rejection, drop, bus error or recovery; TEC and REC 0 throughout. Largest service gap 4.8 ms, slowest operation 6.9 ms, least free stack 9,008 bytes, heap unchanged. Against the 60 s-summary A/B runs, the extra console output cost 0.8% of the read rate (184,761 against 186,169 operations).

**Transitions.** At the first replies both nodes went from `health=UNINIT` to `READY` and from `eligibility=0x4` to `0xE`, as at every start. After that only node 2's position changed:

| UTC | Node 2 `6064` | `606C` |
| --- | --- | --- |
| 16:42:01 | 896 → 895 → 850 → 814 → 741 → 690 → 654 | 0, −45, 0, −45, −22, −5, 0 rpm |
| 16:42:03 | 654 → 658 → 721 → 781 → 821 | 9, 35, 1, 0 rpm |

**The released, free shaft of node 2 turned by 242 pulses one way and 167 back within 2 s, while the S2 sent nothing but reads.** Node 2's statusword stayed `4050` (switch on disabled) and `2100` stayed `0014`; node 1, enabled and holding, did not move. A drive in that state applies no torque, so an outside force turned the shaft. **Explained on 2026-11-06:** the user reported turning it by hand to see whether anything happened. The read soak recorded the hand turn in position and signed speed (`606C` down to −45 rpm), with no change of state or fault, as expected for a released drive.

**Console: 182 losses in 10,686 bursts (1.7%), against 2 to 3 in 180 (1.1 to 1.7%) at 60 s summaries.** Every loss again fell in the first 600 bytes of a burst, which began after under a second of quiet. The rate per burst did not change with the quiet spell, from 0.8 s to 60 s, so **the idle-link explanation is refuted**: neither the host's transmit direction (the A/B) nor the S2's is the trigger. Per half hour R1 lost 20, 25, 28, 32, 42 and 35.

The same 1 s bursts lost nothing in 1,199 bursts on 2026-11-03 ([`20261103-soak-burst1s`](../bench/20261103-soak-burst1s/summary.txt)), where reads ran every 200 ms. At R1's first-half-hour rate it should have shown about 13 losses. So the loss follows the 20 ms read load, or the image (`261a25b` against `2920479`), not quiet time. The later runs separate the two.

### R2: R1 repeated after an S2 reset

[`20261105-stress-r2-summary1s-repeat`](../bench/20261105-stress-r2-summary1s-repeat/transitions.txt), 18:45Z to 21:45Z, same settings.

**CAN: clean**, as R1: 184,764 operations and 646,680 steps per node, all successful; TEC and REC 0; largest gap 4.8 ms. **No signal changed after the first replies**: node 2's shaft stayed at 821.

**Console: 212 losses in 10,672 bursts (2.0%)**, 28, 42, 25, 36, 45 and 36 per half hour, all in the first 602 bytes of a burst. The S2 reset did not bring back R1's lower first half hour, and R2 shows no rise, so **R1's rise was scatter, not something accumulating** in the S2 or in the host's session. Between the two runs the rate per burst is steady at about 1.7 to 2.0%.

The runner refuses read intervals under 20 ms (`--interval-ms must be 20..60000`), so a 5 ms run was not possible; 20 ms is the heaviest load it offers.

### R3: 200 ms reads, 1 s summaries

[`20261105-stress-r3-reads200ms`](../bench/20261105-stress-r3-reads200ms/transitions.txt), 21:46Z to 00:46Z, on a fresh S2 reset with the same image and the same bridge session.

**CAN: clean.** 26,997 operations and 94,495 steps per node, all successful, exactly one operation per node every 400 ms. TEC and REC 0, largest gap 4.2 ms, no signal changed after the first replies.

**Console: no loss in 10,791 bursts**, with the same 1 s summaries, about the same 16.6 MB of output and the same image. At R1 and R2's rate it would have lost about 200. So **the loss follows the 20 ms read load, not the image**, and the 2026-11-03 200 ms run's clean result was not luck.

**What the read load changes.** At 20 ms the S2 cannot keep the schedule: it slipped 140,119 ticks and an operation waited for its predecessor in 2.0 million turns, giving about 34 operations per second instead of 50. At 200 ms both counts are 0. The firmware writes console bytes straight into the S2's 128-byte UART FIFO, at most 16 per service turn and only while there is room (`uart_tx_chars` with no TX ring, `examples/common/EspProbePlatform.cpp`; `OUTPUT_BUDGET`, `examples/common/ProbePlatform.h`). A turn ends with a 1 ms tick delay, so an idle loop can feed up to 16 kB/s against the line's 11.5 kB/s. A loop busy with CAN service feeds less, and the UART stream breaks up. It still cannot drop a byte the FIFO accepted. The firmware's own line counter stayed contiguous in every run. What vanishes is a whole 64-byte USB full-speed transfer (one 35-byte one on 2026-11-03), so the bytes are lost after the S2's UART: in the ESP-Prog-2 bridge, on the USB link or in the host driver. **Leading hypothesis, not shown:** the bridge mishandles the broken-up UART stream it receives under the 20 ms load.

### R4: 50 ms reads, 1 s summaries

[`20261106-stress-r4-reads50ms`](../bench/20261106-stress-r4-reads50ms/transitions.txt), 00:47Z to 03:47Z, fresh S2 reset.

**CAN: clean.** 107,990 and 107,989 operations per node, all successful, one per node every 100 ms; TEC and REC 0, largest gap 4.6 ms, no signal change after the first replies.

**Console: 72 losses in 10,799 bursts (0.67%)**, 19, 12, 12, 12, 9 and 8 per half hour. **The schedule held**: 0 slips, and only 39 turns in which an operation waited for its predecessor. So a saturated loop is not needed for the loss; its rate rises with the CAN operation rate. The losses also spread further into a burst than at 20 ms: reported lines began up to 1,455 bytes after the summary line, against at most 602.

**No byte was ever altered.** In R1 to R4, 67.2 MB of console capture, no line holds a byte outside printable ASCII, and every damaged line is a clean splice. Electrical noise on the S2-to-bridge UART would sometimes corrupt bytes; it never did, so the loss is a deletion in the data path, not a corrupted signal.

### R5: 100 ms reads, 1 s summaries

[`20261106-stress-r5-reads100ms`](../bench/20261106-stress-r5-reads100ms/transitions.txt), 03:48Z to 06:48Z, fresh S2 reset.

**CAN: clean.** 53,995 and 53,994 operations per node, all successful; TEC and REC 0, largest gap 4.2 ms, 0 slips, no signal change after the first replies.

**Console: 8 losses in 10,799 bursts (0.07%)**, between 375 and 1,421 bytes into a burst; no byte outside printable ASCII.

### The loss against the read rate

All five runs used the same image and 1 s summaries, so they differ only in the read interval:

| Run | Reads | Operations per second | Slips | Bursts | Losses | Per burst |
| --- | --- | --- | --- | --- | --- | --- |
| R1 | 20 ms | 34 | 140,119 | 10,686 | 182 | 1.7% |
| R2 | 20 ms | 34 | 140,026 | 10,672 | 212 | 2.0% |
| R4 | 50 ms | 20 | 0 | 10,799 | 72 | 0.67% |
| R5 | 100 ms | 10 | 0 | 10,799 | 8 | 0.07% |
| R3 | 200 ms | 5 | 0 | 10,791 | 0 | 0 |

**The loss rises much faster than the read rate.** Doubling it from 10 to 20 operations per second multiplied the loss by about 9. That fits a loss that needs several CAN service events to coincide within one stretch of console output, more than one proportional to the frames sent, but it is a shape, not a mechanism. **Practical rule from these runs:** at up to 5 read operations per second the console stayed lossless for 3 h; above that, expect detected losses and keep relying on the line numbering. The CAN side never lost a read in any run, so none of this affects the library's evidence.

### R6: closing endurance soak, cut by a power failure

[`20261106-stress-r6-endurance`](../bench/20261106-stress-r6-endurance/transitions.txt), 20 ms reads and 60 s summaries from 06:49Z, planned until 13:00Z.

**The host PC lost mains power at about 08:34Z**, 1 h 45 min in. The S2 and the ESP-Prog-2, powered from its USB, went down with it, so the runner wrote no `run.json` and the trial replays as `incomplete` (`capture_failed`, `soak_no_end`). Up to the cut, the last summary at 08:34:03Z:

- **CAN: clean.** 108,607 and 108,608 operations per node, all successful except one per node still in flight when that summary was printed; 760,253 frames each way with no TX rejection, drop, timeout or bus error; TEC and REC 0; largest gap 4.8 ms; no signal change after the first replies.
- **Console: no loss in 105 bursts.** At the A/B's 60 s-summary rate about 1.5 to 2 were expected, so this is not a significant difference.

**The drives kept their power.** After the PC came back, the S2 was reset and pinged, and the first replies of R7 at 09:17Z read both nodes exactly as before the cut: node 1 enabled (`0037`) at 4728 with `2100=0000`, node 2 released (`4050`) at 821 with `2100=0014`. A host or MCU power loss therefore left the drives' state unchanged. The S2 lost its session, as on every MCU restart, and sent no write when it came back.
