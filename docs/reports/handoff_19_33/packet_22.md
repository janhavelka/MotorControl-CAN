# Packet 22: finite two-node HIL regression and failure closure

**Session of 2026-11-04.** After the user power-cycled both drives, a third campaign ran over five ledgers and three images, with the user at the bench but asking that motion be judged from the drives' own reports: 55 sends, **27 motion admissions** and 8 settings sends, every boot within its caps. It retested the quick-stop fix, ran a stop during a library move twice, the relative case (step 4) and an observed two-node move with its quick stop (step 6), and found and fixed three defects (below). See the [campaign record](#campaign-record-third-campaign-2026-11-04) and the [bench report](../2026-11-04_bench.md).

**Unattended session, 2026-11-03.** A second campaign ran on the user's written waiver, with nobody at the bench, on the attended session's boot: 3 motion admissions. Packet 20's `move` route ran once on node 2 and succeeded, with node 1 unchanged; a stationary `000B` ran on each node and released the motor. One defect was found and fixed on the host (below), and retested on hardware on 2026-11-04. See the [unattended bench report](../2026-11-03_unattended_bench.md).

**Attended session, 2026-11-03.** A first campaign ran: 12 motion admissions (the cap), enables on both nodes, one absolute move with rearm and one moving HALT stop on node 1. One defect was found and fixed (the enable compared `6041` bit 10, `e17271a`). Still owed: `000B`, a node 2 move, packet 20's `move` route and the first `1003` read. The route records lacked controller counters, which step 7 asks for; `TRIAL_RES` now carries them. See the [attended bench report](../2026-11-03_attended_bench.md).

**Before that session the motion campaign had not run.** Its prerequisites are physically unmet: no drive has been enabled (packet 15), no stop qualified (packet 16), and no position evidence acquired (packet 19). The campaign also needs an attendant and an independent cutoff. **Zero motion admissions were made**, on any boot.

## What ran instead

The communication part of this packet's regression ran overnight as soaks, through the ordinary owner and typed reads, reads only, on both specimens. See the [overnight soak report](../2026-11-03_overnight_soak.md) for figures, transitions and evidence links.

## Defects, causes, fixes and retests

| Defect | Cause | Fix | Regression | HIL retest |
| --- | --- | --- | --- | --- |
| On the S2, every SDO write by the session was refused before work, because the node never had ordinary-SDO eligibility (soak `eligibility=0xC`: probe upload allowed and a qualification recorded but not current, so no ordinary SDO). Enable, sequenced stop and move routes would all have refused `eligibility`, and the owner reported both drives `UNINIT` all night | Service qualification needed an exact receive time. The S2 shim gives only receive bounds, and the drives produce no heartbeat, so nothing could ever make ordinary SDO current | A reply qualifies service when its earliest receive bound is strictly after its handoff and after the node boundary (`SdoClient::recordSdo` with `ObservationTime`, [receive-time guide](../../adapters/twai_receive_time.md#sdo-service-qualification)) | `test/bounded_service_test.cpp`; `sdo_transfer_test`, `network_receive_time_test`, `device_evidence_test` and `bench_route_test` updated | **Passed, reads only, on `261a25b`:** both nodes went from `eligibility=0x4` to `0xE` (ordinary SDO) and `health` from `UNINIT` to `READY` with their first qualified replies, where `fcea8b4` held `0xC` and `UNINIT` all night. See the soak report |
| No enable could complete on these specimens: trial 103 ended at word 6 with `OBSERVATION_SCOPE` | The scope rule compared every `6041` bit above 0..2 across the enable, and bit 10 follows the enable state itself (`8437` → `8031` → `8437`) | `e17271a`: bit 10 (`STATUS_EN86_ZERO_SPEED`) is not compared; `606C` still decides velocity, and every other bit still ends the enable | `zeroSpeedBitFollowsTheEnable` in `test/enable_action_test.cpp`, replaying the measured bytes; fails without the fix | **Passed** on both nodes, trials 108 and 118 |
| The route records kept no controller error counters, which step 7 asks for | `RouteResources` sampled stack and heap only | `TRIAL_RES` adds `tec0 rec0 tec1 rec1`, sampled at admission and terminal from the TWAI driver; optional in the parser, so older records still parse | `bench_route_test` (the fake sampler's counters reach the line) and `test_route_controller_counters_are_parsed_and_older_records_still_fit` | **Passed** on 2026-11-04: every admitted route's record carries them, and all read 0 at admission and terminal |
| A quick stop could never complete on these drives: trials 124 and 126 (unattended) ended `OBSERVATION_SCOPE` | The stop rule treated every released report as outside its scope, a provisional policy written before `000B` was measured | `QUICK_STOP` only: a not-running cycle with `2100` released, `6041` low bits `000`, mode 1 and no alarm is `OBSERVED`, and `monitorObservation` keeps the release. A `HALT` still refuses a release | `quickStopReleaseIsTheMeasuredTerminal` in `test/stop_action_test.cpp`, replaying trials 124 and 126 and five neighbors; fails without the fix | **Passed** on 2026-11-04 after a power cycle: `000B` from a stationary `010F` on node 1 ended `OBSERVED` (trial 208) |
| The soak parser read the EVIDENCE header `outcome=1` (read success) as a failure | Wrong outcome table in `scripts/s2_evidence.py` | `READ_OUTCOMES`, failure only above 1 (`edba06e`) | `test_measured_width_identity_succeeds_and_keeps_its_discrepancy` in `scripts/s2_evidence_test.py` | Packet 11's live baseline |
| The soak watcher flagged an operation in flight as an anomaly | `ops - opsOk` is 1 while an operation runs | Watch failure, refusal and stop counters instead | Scratch tool only, not shipped | Stress run, no false alarm since |
| **Open, not a library defect.** Three console losses in stress1, whole chunks of 64, 64 and 35 bytes, between the S2's UART and pyserial | Localized below pyserial (driver, USB link or the ESP-Prog-2 bridge) and after about a minute of console silence. Not host load, host read latency, or the S2's console path. Idle time, 20 ms read load and bridge state are still confounded | None yet. The line numbers detect each loss; runs with quiet spells of at most about 10 s showed none (1,379 bursts) | Host option `--keepalive-s` (`test_keepalive_writes_only_newlines_and_counts_them`) for the A/B test | First A/B attempt (2026-11-03 evening): run A lost three more 64-byte chunks in 89 bursts, then both drives went silent. Run B never had a quiet spell, so the keepalive is untested. On 2026-11-04 run A ran 56 minutes with the drives answering ([summary](../../bench/20261104-soak-ab-plain/summary.txt)): 58,262 and 58,261 reads, all successful, and 2 losses, each early in a summary burst after about 60 s of quiet. It was cut short for the motion work; run B, with `--keepalive-s 1`, is still owed. See the [soak report](../2026-11-03_overnight_soak.md#ab-attempt-2026-11-03-evening) |
| Under a lasting bus fault the soak printed one bus anomaly per pass, about 1.5 million lines, and no summary for 3 h. The first anomaly's dump was never closed. At each run's end `SOAK_CON` printed as `OUTPUT_FORMAT_ERROR` | Anomalies are never rate limited, and events go before summaries. The 240-byte line buffer was shorter than `SOAK_CON` with large counters | One anomaly per value or counter signal per summary window. A 416-byte line buffer and a 512-byte console queue hold every soak line at its type maxima (`711c632`) | `testLastingBusFaultKeepsSummaries`, `test_every_soak_line_fits_its_buffer_at_type_maxima`; each fails without the fix | **Retested** on the same no-ACK, 5 min ([`20261103-soak-noack`](../../bench/20261103-soak-noack/summary.txt)): every summary printed, no line lost, 6 bus anomalies |
| `s2_evidence_contract` failed on `065b23e` | That commit retained the unanswered boot-time ping `20261103T1642Z-s2-ping-b`, and the test lists such pings by name | Listed with the earlier one (`711c632`) | The test itself | None needed |
| 2026-11-04, trial 213: a stop's last observation read began 6 ms before the stop's deadline and was cut there with its request possibly sent. Node 2's route stayed unresolved until an MCU restart (trial 214 refused `session`) | Actions and moves scheduled cycles up to their deadline, and the owner cuts an exchange still open at the deadline; Q18 keeps the channel closed | `nextAction`/`nextMove` yield no new observation read once one exchange's queue and response budgets no longer fit before the deadline (`5b5b6c1`); the routes too (`e7f28ac`) | `lateObservationIsHeldBack`, `lateReadIsHeldBack`; each fails without the fix | **Passed**, trial 413: the stop's reads ended about 0.4 s before its deadline and the route stayed usable |
| 2026-11-04, trial 213: a quick stop during a move could never complete | After `000B` released a moving motor the drive kept `2100` running set (`0014`), for 66 minutes at least | `QUICK_STOP`: once a cycle reads released, low bits `000` and running, later cycles also read `6064`/`606C`, and two with `606C` 0 and `6064` unchanged complete it (`standstillObserving`, `5b5b6c1`) | `latchedRunningQuickStopCompletesOnStandstill`; fails without the rule | **Passed**, trial 413: `OBSERVED` about 0.6 s after the command |
| 2026-11-04, trials 303 and 304: the evidence parser called the `60FE:00` client-abort records `inconsistent` | A cleanup abort frame's TX report was counted as a second report of the request | Matched to its own frame by `txr`/`txf` (`e7f28ac`) | Built from the real capture | 303 and 304 replay as `failed-evidence` |
| **Open, 2026-11-04, not a library defect:** `607D:00` and `60FE:00` answer as two-byte replies of 0 against the EDS U8, so the strict typed reads abort and the route stays unresolved (backlog D16) | A device fact against the EDS | A measured-width option for these counts, being implemented | Pending | Pending: packet 12 T1/T2 and the `60FE` readback |
| **Planning error, 2026-11-04:** the return of trial 321's polarity write was refused by the runner as a fourth `io-di3` admission against the cap of 3 | The plan put four `io-di3` cases on one node in one ledger | None needed: the cap held. The write did not survive the next power cycle (trial 403) | None | None |

`conRej=2524`, constant through both long runs, is not a defect. It counts the console's refused submissions since boot, all retries by earlier evidence dumps, and no line was lost (see the [bench guide](../../hardware_bench.md)).

## Campaign record, first campaign (2026-11-03)

The plan was recorded before each send in the ledger [`20261103-campaign.jsonl`](../../bench/20261103-campaign.jsonl), one exact route line per trial. The fields as they stood:

| Field | State |
| --- | --- |
| Drives and motors | Two CL86-C labelled drives, nodes 1 and 2, 1 Mbit/s, shafts free and marked, motors fixed to the table. Motor model not established; this configuration measured 1000 command pulses per revolution on node 1 (`2201=1000`) |
| Firmware evidence | Typed identity 9 of 9 per node (packet 11). No firmware version qualifies a behavior |
| Controller, SDK and image | ESP32-S2 on ESP-Prog-2 COM15, IDF 5.5.5; images `71c8670`, then `e17271a`, each verified by `ping` |
| I/O assignments | Not read in this campaign; packet 13's P1 plan remains its record |
| Native state before | `607A=5000`, `60FF=120`, mode 0, `6041=8037`, `2100=0001`, `6064=0`, `6081`/`6083`/`6084` 60/100/100 on node 1 (trial 101); node 2 the same (trial 116) |
| Targets, rates, ramps | Absolute 0 → 250 and 250 → 750 pulses, 2 rpm, 500 ms ramps |
| Deadlines, polls | Enable 10 s at 800 ms per poll; mode 1 and rearm 500 ms polls; position 15 or 8 cycles of 1000 ms; HALT deadline 2 s polled every 250 ms; route cutoff 60 s |
| Independent stop and observation | The user at the drive-power cutoff throughout, paper shaft marks |
| Error history | `1003` was not read in the attended session (backlog H03). `1001` and `603F` were 0 with the fault LED off before and after. After the unattended campaign, on `e724a39`, the first `1003` reads (trials 128 and 129) found `1003:00=0` on both nodes, with `1001` and `603F` 0 before and after; the LED was not observed |
| Admissions | 17 sends, 12 motion admissions (the cap), at most 3 per route and node |

**Budget.** The firmware caps admissions per boot: 12 motion admissions, 3 per route and node, 60 s per route (`ROUTE_MOTION_TOTAL`, `ROUTE_MOTION_PER_CASE`, `ROUTE_CUTOFF_MS`), and a planned stop inside a move counts as one more. Those counts reset on reboot, so `scripts/s2_bench.py` also keeps a campaign ledger across boots: every motion command needs `--campaign PATH`, a send entry is written before the line goes out, and it refuses at the same caps or a trial number not above every earlier one. A missing ledger is refused unless `--new-campaign` starts it, so a mistyped path cannot reset the count.

## Campaign record, second campaign (2026-11-03, unattended)

The full record, written before transmitting, is in the [unattended bench report](../2026-11-03_unattended_bench.md#campaign-record-written-before-transmitting). What differs from the first:

| Field | State |
| --- | --- |
| Authorization | The user's written waiver, verbatim in the report, replacing the attendant and independent cutoff for this campaign only. `scripts/s2_bench.py --unattended-waiver` stored it in each request and send entry, asked no attendant question and stored every eye-only answer as `unattended: not observed` |
| Image | `e17271a`, the attended session's boot, not reflashed: a new boot would lose both sessions' latest controlwords. So no record holds controller counters |
| Ledger and budget | [`20261103b-campaign.jsonl`](../../bench/20261103b-campaign.jsonl), new; 9 sends, 3 motion admissions planned and made, one admission of the boot left unused |
| Cases | Trial 121, node 2, `move` 0 → 250 at 2 rpm with packet 19's evidence; trial 124, node 2, stationary `000B` from the move's `000F`; trial 126, node 1, stationary `000B` from the halted `010F` |
| Observation | Drive reports only. No shaft, LED or other physical observation |
| Result | No alarm, nonzero `1001` or `603F`, EMCY, lost record or console loss. Least free stack 7,024 of 12,288 bytes during the move, largest service gap 5 ms, heap unchanged at 50,060 bytes free |

## Campaign record, third campaign (2026-11-04)

The record written before transmitting is in the [bench report](../2026-11-04_bench.md#campaign-record-written-before-transmitting). What differs from the first two:

| Field | State |
| --- | --- |
| Authorization and observation | The user power-cycled both drives, was at the bench, and asked that the work run autonomously, with motion confirmed from the drives' own reports. At 11:37Z they reported both shafts holding torque since power-up and both fault LEDs off, and both shafts holding again after their second power cycle at about 15:38Z. Every later shaft or LED question is recorded as not observed |
| Images | `711c632`, the boot of 2026-11-03 23:18Z, not reflashed; then `70eeddd` and `e7f28ac`, each flashed and checked by `ping` and `--check-ping`. The soak between them ran on an image flashed from the same sources as `711c632`, whose ping reports base `e356baf` |
| Ledgers and counts | [`20261104`](../../bench/20261104-campaign.jsonl): 14 sends, 10 motion admissions (trial 213's planned stop counts twice). [`20261104b`](../../bench/20261104b-campaign.jsonl): 10 sends, 2. [`20261104c`](../../bench/20261104c-campaign.jsonl): 12 sends, 1, and 7 settings sends. [`20261104d`](../../bench/20261104d-campaign.jsonl), after the second power cycle: 13 sends, 12 (the boot's cap), and 1 settings send. [`20261104e`](../../bench/20261104e-campaign.jsonl): 6 sends, 2 |
| Native state before | Both nodes at the power-up state of 2026-11-02 (trials 201, 202): `6041=8037`, `2100=0001`, mode 0, `6064=0`, `607A=5000`, `60FF=120`, `6081=60`, `6083=6084=100`, `1017=0`, `1001` and `603F` 0; the same `6041`, `2100`, mode and `6064` after the second power cycle (401, 402). `1003` empty on both, `1001` and `603F` 0 before and after (203, 204) |
| Cases | Node 1: enable, stationary `010F` and `000B` (207, 208), `000B` on a released state (307), the DI3 settings (319 to 321), a halted move and its clear (406, 408), the relative base and a library relative move (409 to 411), an observed move with a quick stop (413). Node 2: enable, the `position` experiment and rearm (211, 212), a library move with a quick stop (213), word 7 (308), the counter zero (323), `000B` on the power-up state (503). Both: the I/O and tuning reads (303, 304, 309, 310) and the `2202` reads (313, 315) |

## The packet's cases

| Case | Route | State after the three campaigns |
| --- | --- | --- |
| Absolute move, node 1 | `position` (isolated experiment) | **Ran** (trial 111): accepted, complete, quarter turn CCW. On 2026-11-04 the same values on node 2 completed at exactly 250 and held it for 8 s (211, 212) |
| Production absolute lifecycle | `move`, through `AxisSession::requestMove` | **Ran** (trial 121, node 2, unattended): `SUCCEEDED` with its own rearm, acceptance `6041` `0x1000` and completion `2100` `0x0001` under `AFTER_ABSENCE` as on node 1. Completion read `6064=251`, the next read 250. On 2026-11-04 it ran with planned stops (213, 413) and once refused before any write, for a deadline too short for its services (412) |
| Relative move (step 4) | `relative` for the base, then `move` with `--move-base actual` | **Ran on node 1, 2026-11-04:** after a halted move left actual 104 against target 600, `005F` with +200 ended at 304, so the base is `ACTUAL` (409); then `requestMoveRelative` with that base, +100, `SUCCEEDED` at 404 with its own rearm (411). Positive displacement only; no `COMMANDED` or `QUEUED` case |
| Stop during a move (step 5) | `position` or `move` with a named stop | **HALT ran** at 2 rpm (113) and 6 rpm (406), stopped and held; clearing it does not resume (408). **`000B` ran** during two library moves at 30 rpm: the drive releases the motor and latches `2100` running, so the first ended `NOT_OBSERVED` (213) and, after the fixes, the second `OBSERVED` (413). Each interrupted move kept its own outcome (interrupted, fenced) apart from its stop's own operation and result. No escalation ran |
| Stationary stop | `stop` | **`000B` ran** from `000F` (124, node 2) and from `010F` (126 and, retesting the fix, 208, node 1), and **`010F` from `000F`** (207), which held. `000B` with no known predecessor: on a released drive it changed nothing (307); on the never-enabled power-up state it released the drive (503) |
| Second case on the same node | `rearm` after `position`; the move's own rearm runs inside the lifecycle | **Rearm ran** (trial 112), then trial 113 on the same node; the `move`'s own rearm ran in trial 121, then trial 124 on node 2; on 2026-11-04 after `position` (212), after a halt (408) and after `005F` (410) |
| Node 2 | Any route on node 2 | P1 to P3 ran (trials 116 to 118), then the `move` (trial 121); node 1's retained state read unchanged afterwards (trial 123). On 2026-11-04 node 2 ran the enable, `position`, a library move with a stop, word 7, the counter zero and `000B` on its power-up state |
| Two-node work with an urgent stop (step 6) | `move` with `--observer` | **Ran** (413): node 1 moved and took its planned quick stop while node 2 was read through its own route, session and read context, with no write. Node 2 read unmoved in both cycles; no unmatched or dropped frame on either node; service gap 6 ms. The stop stayed on the moving node: no urgent stop on the observed one, and no simultaneous motion |
| Task stack during actions | Every route records it | **Measured:** least free 8,492 of 12,288 bytes during the HALT, then 7,024 during the unattended `move`; the stops went no lower. On 2026-11-04 the least was 6,992, during the relative move (411); the moving quick stops reached 7,024 (213) and went no lower than 6,992 (413). High-water marks since each boot, not a bound |
| Controller counters | `TRIAL_RES tec0 rec0 tec1 rec1`, added after the first campaign | Absent from the first two campaigns' records. **On 2026-11-04 TEC and REC read 0 at admission and terminal in every admitted route's record** |
| Retained state and loss (step 7) | The closing reads | After the last trials: node 1 released with the latched running bit at `6064=1569` (`4050`, `2100=0014`, 505), node 2 released at 0 (`8050`, `2100=0011`, 506). No alarm, nonzero `1001` or `603F`, EMCY, lost record or dropped frame in any route record. One console chunk was lost mid-record in 318, the UART loss of the soaks, not CAN. The DI3 polarity write did not survive the second power cycle (403); no `2202`, `2500`, save or restore was written |

## Review of the campaign tools

An independent reviewer read the `rearm` and `move` routes, the `position` changes, the host ledger and the parser, and ran a probe against the real fake owner. **It found no path that sends a motion or controlword write the operator did not ask for.** Fixed, each with a test that fails without its fix:

| Finding | Fix |
| --- | --- |
| Lost evidence never changed an action route's outcome: a move with a flooded trace finished `NONE` | Loss ends the route `EVIDENCE_LOSS` and withdraws an active enable or move locally (`lossCancel=1`); nothing is sent because of it |
| Two invocations sharing a ledger could both pass the caps | An exclusive lock per invocation, and a reload and recheck before the send entry |
| A damaged or edited ledger was accepted | A `seq` and SHA-256 chain, and consistency checks on each send entry. The tail and a restored copy still need the ledger committed with each session |
| The parser logged move and stop outcomes as one signal | Separate names for enable, stop and move results |
| A planned stop refused at the cutoff let the move run on | The move or position write is refused unless the stop's span fits the cutoff |
| The predecessor was checked one turn before the first write | Checked again when the write is offered |
| A corrupted command line could parse as another valid plan | Line protocol 3: a CRC-32 on every route line, refused `crc` otherwise |
| `begin()` trusted the parser's ranges | `begin()` validates the plan itself |

The lead added one more: an EMCY withdrawal used the route's own time, which the session refuses if the owner's clock is ahead. It now uses the later of the two, as the loss and abort paths do. The attended session ran on protocol 3 images; the 2026-11-04 campaign on protocols 3 to 5, with the controller counters.

## Sibling refresh

| Sibling | Revision | Dirty | Disposition |
| --- | --- | --- | --- |
| MotorControl-RS | `a13f767` (`src`, `include` unchanged since `6499571`) | Clean | HW-15 (`docs/reports/2026-11-02_standalone_promptset_audit.md`): the ESS drive acknowledged but ignored a start within about 0.6 s of an enable; RS holds starts for `ENABLE_SETTLE_US` in its application (`MotorControl-RS-overnight` `examples/probe_cli/ProbeApp.cpp:50`). **Rejected** as a CL86-C fact or library constant. **Adopted** as a trial observation: record the enable-to-trigger interval, and never read an acknowledged trigger as acceptance, which the move already refuses to do. HW-14 as in [packet 16](packet_16.md) |
| ADS1115 | `d613a13` | Clean | Adopted: `INDETERMINATE` keeps an uncertain hardware effect as its own terminal (`include/ADS1115/ADS1115.h:51`) |
| INA228 | `73f9548` | Clean | Adopted: a failed side-effecting transfer counts as possibly applied (`INA228::_failJob`) |
| FieldCore-node | `5ad9292` | Clean | Adopted: separate stop and ordinary histories (`RodCycleCoordinator::stopHistory`, `ordinaryHistory`); the campaign keeps the interrupted move's outcome apart from the stop's |

## Next smallest measurement

The third campaign covered what this section listed after the second, except the measurements below. The drives answered again after their power cycle; why they went silent at 18:13:49 UTC on 2026-11-03 was not observed. At the end node 1 is released with the latched running bit and node 2 released, so the next enable on either needs another power cycle. What remains:

1. **Packet 12 T1/T2,** once the typed `SOFT_LIMITS` read has the measured-width option for `607D:00` (backlog D16), which is being implemented. The `60FE` readback of packet 13 P1 waits for the same option.
2. **Measurements software cannot make:** packet 13 P4 (an output at `2400:0E`), with an operator, meters on DO0..2 and DOCOM at 0 V; packet 14 T1/T2, with a phase-current probe and the motor's rated phase current; packet 17, on a naturally present alarm, which has not occurred; a `1003` read with the fault LED observed around it (the 2026-11-04 reads recorded it unobserved).
3. **Communication:** the console-loss A/B soak, unattended, 3 h each: run A ran 56 minutes on 2026-11-04 and lost 2 chunks, and run B with `--keepalive-s 1` has not run. Q18 and packet 3's TWAI questions wait for prompt 47's listen-only capture, as the user asked.

Measured on 2026-11-04 and no longer owed here: the quick-stop retest, the stationary `010F`, `000B` during a move, `000B` with no known predecessor on a released state (no change, 307) and on the never-enabled power-up state (released, 503), the relative base, the counter zero, word 7's pull-back and the 251 count (250 exactly). Each is one or two specimens on one firmware, judged from drive reports.

## Checks

See the [execution table](README.md) and the soak report.
