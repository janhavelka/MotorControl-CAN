# Packet 22: finite two-node HIL regression and failure closure

**Unattended session, 2026-11-03.** A second campaign ran on the user's written waiver, with nobody at the bench, on the attended session's boot: 3 motion admissions. Packet 20's `move` route ran once on node 2 and succeeded, with node 1 unchanged; a stationary `000B` ran on each node and released the motor. One defect was found and fixed on the host (below), not yet retested on hardware. See the [unattended bench report](../2026-11-03_unattended_bench.md).

**Attended session, 2026-11-03.** A first campaign ran: 12 motion admissions (the cap), enables on both nodes, one absolute move with rearm and one moving HALT stop on node 1. One defect was found and fixed (the enable compared `6041` bit 10, `e17271a`). Still owed: `000B`, a node 2 move, packet 20's `move` route and the first `1003` read. The route records lacked controller counters, which step 7 asks for; `TRIAL_RES` now carries them. See the [attended bench report](../2026-11-03_attended_bench.md).

**Before that session the motion campaign had not run.** Its prerequisites are physically unmet: no drive has been enabled (packet 15), no stop qualified (packet 16), and no position evidence acquired (packet 19). The campaign also needs an attendant and an independent cutoff. **Zero motion admissions were made**, on any boot.

## What ran instead

The communication part of this packet's regression ran overnight as soaks, through the ordinary owner and typed reads, reads only, on both specimens. See the [overnight soak report](../2026-11-03_overnight_soak.md) for figures, transitions and evidence links.

## Defects, causes, fixes and retests

| Defect | Cause | Fix | Regression | HIL retest |
| --- | --- | --- | --- | --- |
| On the S2, every SDO write by the session was refused before work, because the node never had ordinary-SDO eligibility (soak `eligibility=0xC`: probe upload allowed and a qualification recorded but not current, so no ordinary SDO). Enable, sequenced stop and move routes would all have refused `eligibility`, and the owner reported both drives `UNINIT` all night | Service qualification needed an exact receive time. The S2 shim gives only receive bounds, and the drives produce no heartbeat, so nothing could ever make ordinary SDO current | A reply qualifies service when its earliest receive bound is strictly after its handoff and after the node boundary (`SdoClient::recordSdo` with `ObservationTime`, [receive-time guide](../../adapters/twai_receive_time.md#sdo-service-qualification)) | `test/bounded_service_test.cpp`; `sdo_transfer_test`, `network_receive_time_test`, `device_evidence_test` and `bench_route_test` updated | **Passed, reads only, on `261a25b`:** both nodes went from `eligibility=0x4` to `0xE` (ordinary SDO) and `health` from `UNINIT` to `READY` with their first qualified replies, where `fcea8b4` held `0xC` and `UNINIT` all night. See the soak report |
| No enable could complete on these specimens: trial 103 ended at word 6 with `OBSERVATION_SCOPE` | The scope rule compared every `6041` bit above 0..2 across the enable, and bit 10 follows the enable state itself (`8437` → `8031` → `8437`) | `e17271a`: bit 10 (`STATUS_EN86_ZERO_SPEED`) is not compared; `606C` still decides velocity, and every other bit still ends the enable | `zeroSpeedBitFollowsTheEnable` in `test/enable_action_test.cpp`, replaying the measured bytes; fails without the fix | **Passed** on both nodes, trials 108 and 118 |
| The route records kept no controller error counters, which step 7 asks for | `RouteResources` sampled stack and heap only | `TRIAL_RES` adds `tec0 rec0 tec1 rec1`, sampled at admission and terminal from the TWAI driver; optional in the parser, so older records still parse | `bench_route_test` (the fake sampler's counters reach the line) and `test_route_controller_counters_are_parsed_and_older_records_still_fit` | Pending: the unattended campaign ran on the unflashed `e17271a` image, so it needs a reflash |
| A quick stop could never complete on these drives: trials 124 and 126 (unattended) ended `OBSERVATION_SCOPE` | The stop rule treated every released report as outside its scope, a provisional policy written before `000B` was measured | `QUICK_STOP` only: a not-running cycle with `2100` released, `6041` low bits `000`, mode 1 and no alarm is `OBSERVED`, and `monitorObservation` keeps the release. A `HALT` still refuses a release | `quickStopReleaseIsTheMeasuredTerminal` in `test/stop_action_test.cpp`, replaying trials 124 and 126 and five neighbors; fails without the fix | **Not retested.** Both drives are released, and no enable can be admitted until they are power-cycled and an attendant observes the power-up shaft |
| The soak parser read the EVIDENCE header `outcome=1` (read success) as a failure | Wrong outcome table in `scripts/s2_evidence.py` | `READ_OUTCOMES`, failure only above 1 (`edba06e`) | `test_measured_width_identity_succeeds_and_keeps_its_discrepancy` in `scripts/s2_evidence_test.py` | Packet 11's live baseline |
| The soak watcher flagged an operation in flight as an anomaly | `ops - opsOk` is 1 while an operation runs | Watch failure, refusal and stop counters instead | Scratch tool only, not shipped | Stress run, no false alarm since |
| **Open, not a library defect.** Three console losses in stress1, whole chunks of 64, 64 and 35 bytes, between the S2's UART and pyserial | Localized below pyserial (driver, USB link or the ESP-Prog-2 bridge) and after about a minute of console silence. Not host load, host read latency, or the S2's console path. Idle time, 20 ms read load and bridge state are still confounded | None yet. The line numbers detect each loss; runs with quiet spells of at most about 10 s showed none (1,379 bursts) | Host option `--keepalive-s` (`test_keepalive_writes_only_newlines_and_counts_them`) for the A/B test | Pending: A/B at stress1's settings, 3 h each, with and without the keepalive. See the soak report |

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
| Error history | `1003` was not read (backlog H03). `1001` and `603F` were 0 with the fault LED off before and after |
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

## The packet's cases

| Case | Route | State after both campaigns |
| --- | --- | --- |
| Absolute move, node 1 | `position` (isolated experiment) | **Ran** (trial 111): accepted, complete, quarter turn CCW |
| Production absolute lifecycle | `move`, through `AxisSession::requestMove` | **Ran** (trial 121, node 2, unattended): `SUCCEEDED` with its own rearm, acceptance `6041` `0x1000` and completion `2100` `0x0001` under `AFTER_ABSENCE` as on node 1. Completion read `6064=251`, the next read 250 |
| Relative move | None, by design | Packet 21's wire base is unevidenced, so every request is refused |
| Stop during a move | `position` with a named stop, or `stop` | **HALT ran** (trial 113), stopped and held; `000B` during a move not run |
| Stationary stop | `stop` | **`000B` ran** from `000F` (trial 124, node 2) and from `010F` (trial 126, node 1): each drive left operation and released the motor, and neither moved. The fixed quick stop is not retested. A stationary `010F` from `000F` not run |
| Second case on the same node | `rearm` after `position`; the move's own rearm runs inside the lifecycle | **Rearm ran** (trial 112), then trial 113 on the same node; the `move`'s own rearm ran in trial 121, then trial 124 on node 2 |
| Node 2 | Any route on node 2 | P1 to P3 ran (trials 116 to 118), then the `move` (trial 121); node 1's retained state read unchanged afterwards (trial 123) |
| Two-node work with an urgent stop on the other node | Not possible as routes stand: one route runs at a time | A two-node plan with its own bounds; keep the second axis stationary meanwhile |
| Task stack during actions | Every route records it | **Measured:** least free 8,492 of 12,288 bytes during the HALT, then 7,024 during the unattended `move`; the stops went no lower |
| Controller counters | `TRIAL_RES tec0 rec0 tec1 rec1`, added after the first campaign | In neither campaign's records: the second ran on the unflashed image. No controller report and no failed TX report occurred in either |

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

The lead added one more: an EMCY withdrawal used the route's own time, which the session refuses if the owner's clock is ahead. It now uses the later of the two, as the loss and abort paths do. The attended session ran on protocol 3 images. The board runs `e17271a`, without the controller counters: reflash before the next campaign.

## Sibling refresh

| Sibling | Revision | Dirty | Disposition |
| --- | --- | --- | --- |
| MotorControl-RS | `a13f767` (`src`, `include` unchanged since `6499571`) | Clean | HW-15 (`docs/reports/2026-11-02_standalone_promptset_audit.md`): the ESS drive acknowledged but ignored a start within about 0.6 s of an enable; RS holds starts for `ENABLE_SETTLE_US` in its application (`MotorControl-RS-overnight` `examples/probe_cli/ProbeApp.cpp:50`). **Rejected** as a CL86-C fact or library constant. **Adopted** as a trial observation: record the enable-to-trigger interval, and never read an acknowledged trigger as acceptance, which the move already refuses to do. HW-14 as in [packet 16](packet_16.md) |
| ADS1115 | `d613a13` | Clean | Adopted: `INDETERMINATE` keeps an uncertain hardware effect as its own terminal (`include/ADS1115/ADS1115.h:51`) |
| INA228 | `73f9548` | Clean | Adopted: a failed side-effecting transfer counts as possibly applied (`INA228::_failJob`) |
| FieldCore-node | `5ad9292` | Clean | Adopted: separate stop and ordinary histories (`RodCycleCoordinator::stopHistory`, `ordinaryHistory`); the campaign keeps the interrupted move's outcome apart from the stop's |

## Next smallest measurement

The second campaign covered the plan this section held before it, except the stationary `010F` and the `1003` read. Its move ran on node 2, not node 1, because node 1's session ended at the HALT's `010F` and so had no admitted `000F` predecessor. Both drives are now released, and both have had motion writes since their last power-up, so no enable can be admitted until they are power-cycled. What remains:

1. **An attended campaign** with its own ledger and record, attendant and cutoff, after a power cycle of both drives and a reflash to the current commit, so that every record carries controller counters:
   - the enable on one node, with the attendant's power-up shaft observation, then a stationary `000B` from its `000F`: the hardware retest of the fixed quick stop, which should end `OBSERVED` with the release kept if the drive repeats trial 124;
   - a stationary `010F` from idle `000F` on the other node, since a node left released or halted cannot be enabled again on that power-up;
   - the first `1003` read, with the fault LED observed before and after: pending.
2. **Later, each with its own route or measurement:** `000B` during a move; packet 21's relative base; the settings, alarm-clear and counter-zero trials of packets 12 to 14, 17 and 18 (backlog H03, S12); whether word 7 pulls a displaced shaft back; whether completion at `6064=251`, then 250, is overshoot or counter dither.
3. **Communication:** the unattended console-loss A/B soak, 3 h each with and without `--keepalive-s` (defects table above).

`000B` with no known predecessor on a released state stays gated (packet 16 step 3). Two released states are now evidenced: word 6's inside an enable (`2100` released set, shaft free, trial 103) and the state `000B` leaves (trials 124 and 126). The library refuses a stop on an idle released drive on both paths, so neither opens it, and no route stops in word 6's state on purpose.

## Checks

See the [execution table](README.md) and the soak report.
