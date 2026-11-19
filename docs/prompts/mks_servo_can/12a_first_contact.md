# 12A — First-contact questions for the first MKS fixture

Execute this stage under the [execution contract](execution_contract.md).
Status: NOT STARTED; hardware NOT TESTED. This stage does not itself authorize hardware work. Run it only when an
identified SERVO42D_CAN or SERVO57D_CAN fixture, a dedicated CAN interface with no CANopen node and applicable
authorization exist, and the owner is present. While [COMMON](../COMMON.md) records the owner as away, every question
below is a row in the [deferred table](../README.md) with the exact action and the cases it unlocks. Never use the
CL86-C bench or COM15. Questions run as soon as their API route exists: Q1-Q3 after 03, Q4 and Q9 after 05, the others
after their stage; they do not wait for 12B.

## Work

1. Before first power record board, revision and nameplate (MK-D24), motor (1.8°, phase resistance below 10 Ω, radial
   magnet installed, MK109 p.115), supply (12-24 V), wiring with shielded twisted pair and common ground (MK109 p.20),
   termination (42D jumper R13, 57D SW1 pin 1, MK109 p.20, p.22) and a free, unloaded shaft. Record the host board,
   transceiver, adapter image and interface. An unrecorded item is a named gap, not a passed check.
2. **A used drive can move at power-on** through FFH auto-run or 9AH power-on homing (MK109 p.27, p.85). First power
   with the shaft free and nothing attached, observe, then read the menu (Mode, CanRate, CanID, CanRSP, CanCRC,
   0_Mode, About) before connecting CAN. Record what was observed and by whom.
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

| Question | Work when prerequisites exist | Remaining limit |
| --- | --- | --- |
| Q1 transport | Declare the menu's bitrate, ID and check mode; one 40H probe; record reply identifier, DLC and latency; an ID above 0xFF only through a planned 8BH change (Q10) | Sample point and timing stay undocumented; no ID or bitrate sweep |
| Q2 identity | 40H raw bytes against the screen's About firmware and date; 42H; compare with MK109 and MK109A for any distinguishing field (MK-D01, D05) | A field that matches on one unit does not identify an edition for all units |
| Q3 reply shapes | 38H, 39H, 3BH and 40H lengths (MK-D04..D06); 00H on each ledger setting code for the readable set and FF FF behavior (MK-D10, D34) | Reads only; undocumented codes are not probed |
| Q4 direction | Enabled, small F4H move of a few hundred counts each way; compare 31H, 35H and 32H signs with a marked shaft the owner observes (MK-D02) | One unit and firmware; FDH and F6H direction bits checked separately with equally small bounded commands |
| Q5 uplink check | No production route: 10 refuses the 0x6B switch until this question resolves it. Only as a reviewed isolated experiment (step 4) with restart control and a restore plan: 4CH to 0x6B, one read, record the uplink check byte, 4CH back to the snapshot mode, read back (MK-D08) | Persistent write; never left in 0x6B; on a missing reply restore through the menu; otherwise NOT RUN |
| Q6 respond-off scope | No production route for the reads: 09 treats them as unavailable under respond 0 and 10 quarantines the address. Only as a reviewed isolated experiment (step 4): snapshot 8CH (00H 8C if Q3 established it, or the recorded CanRSP menu reading for both bytes), 8CH respond 0, then one read and one F1H; restore the snapshot bytes and compare by read (MK-D11) | Without a snapshot of both bytes NOT RUN; if reads stop replying, restore the snapshot through the menu; no blind repeats |
| Q7 host silence | Production 08 route: snapshot 98H by 00H 98 (readable per Q3) and write the test value while quiescent, before starting a bounded F6H run-time run; vary which frames are sent (own-ID reads, other-ID frames, none); record stop type, enable after, any frame and the value after restart (MK-D18) | Restore the snapshot value, not 0, and compare by read; without a readable snapshot NOT RUN; an observed stop is not a safety qualification |
| Q8 power-on state | After a 41H restart and a power cycle: 3AH, 31H, 33H, stream and latch state (MK-D13, D19, D30) | Power cycle needs the owner; 41H only with FFH and 9AH read off |
| Q9 completion | Small encoder moves with 95H at default and disabled; F7H, F7H 01 and mode stops during a move; an F5H update during a move; immediate and completion reply latency (MK-D17, D36) | No commanded stall or load; no repeated trigger after a lost reply |
| Q10 ID and bitrate | One 8BH or 8AH change with old and new tuples reserved: which tuple carries the reply and when it applies; restore and confirm (MK-D20) | Persistent; one change at a time; restore through the menu if the node is lost |
| Q11 persistence | Change one named setting per persistence class, restart, compare; one 3FH restore with full before and after snapshots (MK-D21, D22) | No repeated save campaign; 3FH only through 10's gate (explicit power-on motion acceptance, safe fixture, restore plan) with restart control |
| Q12 streams and latching | 01H on 31H at a few periods while polling other codes; error counters and bus load; 4AH with one latched small move, one 4BH, then a second 4BH (MK-D12) | Second 4BH only on a free shaft with a small move; the library never repeats it |
| Q13 homing and limits | Origin-switch and mechanical homing on the actual switch or stop fixture; 3BH, F1H and 31H after homing; homing onto the left switch with EndLimit (MK-D16, D25) | Single-turn method only if the owner accepts arming power-on homing (MK-D37); never disable limits |
| Q14 malformed frames | No production or checked-codec route builds a frame with a wrong check byte, a wrong DLC or an undocumented code, and the experiment prerequisites exclude a raw sender. Answer from manufacturer evidence (MK-D09); a harness sending such frames to a stationary released drive needs the user to change those prerequisites explicitly first | NOT RUN until then; never a motion code; stop at the first unexpected effect |

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
