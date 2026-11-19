# 12A — First-contact questions for the first MKS fixture

Execute this stage under the [execution contract](execution_contract.md).
Status: NOT STARTED; hardware NOT TESTED. This stage does not itself authorize hardware work. Run it only when an
identified SERVO42D_CAN or SERVO57D_CAN fixture, a dedicated CAN interface with no CANopen node and applicable
authorization exist, and the owner is present. While [COMMON](../COMMON.md) records the owner as away, every question
below is a row in the [deferred table](../README.md) with the exact action and the cases it unlocks. Never use the
CL86-C bench or COM15. A question runs as soon as the route and minimum stage its row names have landed, on the
dialects its row names; none waits for 12B.

## Work

1. Before first power record board, revision and nameplate (MK-D24), motor (1.8°, phase resistance below 10 Ω, radial
   magnet installed, MK109 p.115), supply (12-24 V), wiring with shielded twisted pair and common ground (MK109 p.20),
   termination (42D jumper R13, 57D SW1 pin 1, MK109 p.20, p.22) and a free, unloaded shaft. Record the host board,
   transceiver, adapter image and interface. An unrecorded item is a named gap, not a passed check.
2. **A used drive can move at power-on** through FFH auto-run or 9AH power-on homing (MK109 p.27, p.85). First power
   with the shaft free and nothing attached, observe, then read the menu (Mode, CanRate, CanID, CanRSP, CanCRC,
   0_Mode, About) before connecting CAN. FFH auto-run has no menu item (MK109 p.21-29), so its state stays unknown
   until a 00H FF read is established, and every later power-on is treated as possibly moving. Record what was
   observed and by whom.
3. Derive the forbidden-command and allowed-write lists for the session from the ledger: never FFH, 9AH, 50H, 0FH;
   persistent writes only inside a question that names its restore path; snapshot each setting through its own read
   route before changing it, restore exactly the captured value in its dialect-specific form in the same session and
   compare the final readback with the snapshot. A setting whose pre-value no established read route (03, 12A Q3) or
   recorded menu reading gives is not written; the question is NOT RUN with that gap. Never a presumed default as the
   restore value. Rehearse each restore on the fake drive from nondefault starting values first.
4. Evidence paths follow contract 02 "First-contact questions": a question either runs through the production route
   its stage admits at that point (named in the table), or rests on manufacturer evidence, or is a separately
   assigned, isolated, independently reviewed experiment under the
   [conditional experiment prerequisites](../../device_conflicts.md#conditional-experiment-prerequisites), through the
   ordinary checked codec and the same owner with its permitted actions, observation, snapshot and restoration
   boundary written first. Otherwise it is NOT RUN with the evidence it still needs. No production refusal is opened,
   weakened or bypassed for a question, and no raw console or frame path is used. Retain firmware and edition, raw
   traffic with host timestamps, controller error counters, service gaps, every operation and step identity and
   restoration results, through the normal debug observer. Recompute check bytes for the actual ID. No automatic
   replay after an uncertain write, and one lead alone holds the interface.
5. Run the questions whose prerequisites exist, smallest effect first:

| Question | Route, minimum stage and dialects | Work when prerequisites exist | Remaining limit |
| --- | --- | --- | --- |
| Q1 transport | 02b probe of the declared dialect (40H on the V1.0.9 tables, 01's chosen read on V1_0_0); after 02b with S9 | Declare the menu's bitrate, ID, check mode and respond byte, with the group ID and active byte `UNKNOWN` (02 step 1, 02b step 5) unless an established read gives them; one probe read; record reply identifier, DLC and latency; an ID above 0xFF only through a planned 8BH change (Q10) | Sample point and timing stay undocumented; no ID or bitrate sweep |
| Q2 identity | 03 identity read (40H) and 42H where the dialect documents it; after 03; V1.0.9 tables only, not applicable on V1_0_0 (no 40H) | 40H raw bytes against the screen's About firmware and date; 42H; compare with MK109 and MK109A for any distinguishing field (MK-D01, D05) | A field that matches on one unit does not identify an edition for all units |
| Q3 reply shapes | 03 typed reads of the declared dialect (a code absent from it, such as 38H outside MK109 or 40H on V1_0_0, is not applicable); 00H only through 03 step 2's bootstrap query for codes whose read effect 01 reviewed, otherwise manufacturer evidence, a reviewed experiment (step 4) or NOT RUN; after 03 | 38H, 39H, 3BH and 40H lengths (MK-D04..D06, D38); 00H on each reviewed ledger setting code for the readable set and FF FF behavior (MK-D10, D34) | Reads only; undocumented codes are not probed; a bootstrap result becomes readback evidence only once the ledger records the code established |
| Q4a direction, position | 05 relative F4H and FDH moves; after 05 | Enabled, small F4H move of a few hundred counts each way, then FDH the same; compare 31H, 35H and 32H signs with a marked shaft the owner observes (MK-D02) | One unit and firmware |
| Q4b direction, speed | 06 finite F6H run; after 06 | One short bounded F6H run each way; compare 32H sign with the marked shaft (MK-D02) | One unit and firmware |
| Q5 uplink check | No production route: 10 refuses the 0x6B switch until this question resolves it; only a reviewed isolated experiment (step 4) with the checked 4CH builder; MK109 only | With restart control and a restore plan: 4CH to 0x6B, one read, record the uplink check byte, 4CH back to the snapshot mode, read back (MK-D08) | Persistent write; never left in 0x6B; on a missing reply restore through the menu; otherwise NOT RUN |
| Q6 respond-off scope | No production route for the reads: 09 treats them as unavailable under respond 0 and 10 quarantines the address; only a reviewed isolated experiment (step 4) with the checked 8CH builder, after 09 and 10 | Snapshot both 8CH bytes by 00H 8C where Q3 established it (the CanRSP menu shows the respond byte only, MK109 p.26), 8CH respond 0, then one read and one F1H; restore the snapshot bytes and compare by read (MK-D11) | Without a read snapshot of both bytes NOT RUN; if reads stop replying, set CanRSP to Enable through the menu and confirm both bytes by read; no blind repeats |
| Q7 host silence | 08 98H write and 06's bounded F6H run-time run; after 06 and 08; V1.0.9 tables | Snapshot 98H by 00H 98 (readable per Q3) and write the test value while quiescent, before starting the bounded run; vary which frames are sent (own-ID reads, other-ID frames, none); record stop type, enable after and any frame (MK-D18) | Restore the snapshot value, not 0, and compare by read; without a readable snapshot NOT RUN; the value after restart belongs to Q11 (after 10); an observed stop is not a safety qualification |
| Q8 power-on state | 03 reads after 10's gated 41H restart and after an owner power cycle; after 10 | After a 41H restart and a power cycle: 3AH, 31H, 33H, stream and latch state, including whether a command held under 4AH survives (MK-D13, D19, D30; 09 step 7's power-cycle inference) | Power cycle needs the owner; 41H only with FFH and 9AH read off |
| Q9a completion | 05 encoder moves and F5H update (V1.0.9 tables), 04 stops (F7H 01 on MK109 only); after 05 | Small encoder moves with the drive's existing 95H setting (read by 00H 95 where established, else recorded unknown); F7H, F7H 01 and mode stops during a move; an F5H update during a move; immediate and completion reply latency (MK-D17, D36) | No commanded stall or load; no repeated trigger after a lost reply; no 95H write |
| Q9b completion threshold | 08 95H write with a snapshot by an established 00H 95 read and step 3's restoration, then 05 moves; after 08 | Q9a's small moves with 95H disabled, then 95H restored to the snapshot and compared by read (MK-D17) | Persistent write; without a readable snapshot NOT RUN |
| Q10 ID and bitrate | 10 communication operations with both tuples reserved; after 10 | One 8BH or 8AH change: which tuple carries the reply and when it applies; restore and confirm (MK-D20) | Persistent; one change at a time; restore through the menu if the node is lost |
| Q11 persistence | 08 setting writes and 10's restart and 3FH restore; after 10 | Change one named setting per persistence class, restart, compare; one 3FH restore with full before and after snapshots (MK-D21, D22) | No repeated save campaign; 3FH only through 10's gate (explicit power-on motion acceptance, safe fixture, restore plan) with restart control |
| Q12 streams and latching | 09 report, latch and 4BH operations; after 09; V1.0.9 tables | 01H on 31H at a few periods while polling other codes; error counters and bus load; 4AH with one latched small move, one 4BH, then a second 4BH (MK-D12) | Second 4BH only on a free shaft with a small move and 09 step 7's recorded per-drive acceptance that any latched move may execute; the library never repeats it |
| Q13 homing and limits | 07 homing and limit operations; after 07; V1.0.9 tables | Origin-switch and mechanical homing on the actual switch or stop fixture; 3BH, F1H and 31H after homing; homing onto the left switch with EndLimit (MK-D16, D25); F7H during mechanical homing at the chosen Hm_ma (MK109 p.81, 07 step 8) | Single-turn method only if the owner accepts arming power-on homing (MK-D37); never disable limits |
| Q14 malformed frames | None: no production or checked-codec route builds a frame with a wrong check byte, a wrong DLC or an undocumented code, and the experiment prerequisites exclude a raw sender | Answer from manufacturer evidence (MK-D09); a harness sending such frames to a stationary released drive needs the user to change those prerequisites explicitly first | NOT RUN until then; never a motion code; stop at the first unexpected effect |

6. If a question fails or surprises, preserve the scenario, reproduce deliberately within authorization and measure
   competing causes; never hide it with retries, longer timeouts or a later pass.

## Deliver

A dated report `docs/reports/mks_servo_can_12a_<date>.md` of the questions run: prerequisites, raw results, restorations
and unresolved measurements. Ledger discrepancy dispositions updated with evidence references (board, firmware,
edition, trace path); fixture-specific qualification applied only through the ordinary API and console routes; deferred
rows for unrun questions; PROGRESS hardware column updated.

## Acceptance

Every run question names its evidence path and has retained raw evidence and a final readback compared with its
snapshot; every changed setting ended restored to that snapshot or is recorded as left changed with the reason. A
question without an admitted path is NOT RUN, and no production refusal changed for it. A result is scoped to its
board, firmware and edition and closes a discrepancy only for that scope. Unrun questions stay named gaps. Software
remains implementable and testable without this stage; release qualification stays with
[12B](12_qualification_and_release.md). Commit and push the report and ledger updates.
