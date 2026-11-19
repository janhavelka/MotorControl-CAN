# 12A — First-contact questions for the first MKS fixture

Execute this stage under the [execution contract](execution_contract.md).
Status: NOT STARTED; hardware NOT TESTED. This stage does not itself authorize hardware work. Run it only when an
identified SERVO42D_CAN or SERVO57D_CAN fixture, a dedicated CAN interface with no CANopen node and applicable
authorization exist, and the owner is present. While [COMMON](../COMMON.md) records the owner as away, every question
below is a row in the [deferred table](../README.md) with the exact action and the cases it unlocks. Never use the
CL86-C bench or COM15. Questions run as soon as their API route exists: Q1-Q3 after 03, Q4 and Q9 after 05, the others
after their stage; they do not wait for 12B.

## Work

1. Before first power record board, revision and nameplate (MK-D24), motor (1.8°, magnet installed, MK109 p.115),
   supply (12-24 V), wiring, termination (42D jumper R13, 57D SW1 pin 1, MK109 p.20, p.22), common ground and a free,
   unloaded shaft. Record the host board, transceiver, adapter image and interface.
2. **A used drive can move at power-on** through FFH auto-run or 9AH power-on homing (MK109 p.27, p.85). First power
   with the shaft free and nothing attached, observe, then read the menu (Mode, CanRate, CanID, CanRSP, CanCRC,
   0_Mode, About) before connecting CAN. Record what was observed and by whom.
3. Derive the forbidden-command and allowed-write lists for the session from the ledger: never FFH, 9AH, 50H, 0FH;
   persistent writes only inside a question that names its restore path; snapshot each setting through its own read
   route before changing it, restore it in the same session and read the restore back.
4. Retain firmware and edition, raw traffic with host timestamps, controller error counters, service gaps, every
   operation and step identity and restoration results. Use the production API and the normal debug observer. Recompute
   check bytes for the actual ID. No automatic replay after an uncertain write, and one lead alone holds the interface.
5. Run the questions whose prerequisites exist, smallest effect first:

| Question | Work when prerequisites exist | Remaining limit |
| --- | --- | --- |
| Q1 transport | Declare the menu's bitrate, ID and check mode; one 40H probe; record reply identifier, DLC and latency; an ID above 0xFF only through a planned 8BH change (Q10) | Sample point and timing stay undocumented; no ID or bitrate sweep |
| Q2 identity | 40H raw bytes against the screen's About firmware and date; 42H; compare with MK109 and MK109A for any distinguishing field (MK-D01, D05) | A field that matches on one unit does not identify an edition for all units |
| Q3 reply shapes | 38H, 39H, 3BH and 40H lengths (MK-D04..D06); 00H on each ledger setting code for the readable set and FF FF behavior (MK-D10, D34) | Reads only; undocumented codes are not probed |
| Q4 direction | Enabled, small F4H move of a few hundred counts each way; compare 31H, 35H and 32H signs with a marked shaft the owner observes (MK-D02) | One unit and firmware; FDH and F6H direction bits checked separately with equally small bounded commands |
| Q5 uplink check | With restart control and a restore plan: 4CH to 0x6B, one read, record the uplink check byte, 4CH back to SUM-8, read back (MK-D08) | Persistent write; never left in 0x6B; on a missing reply restore through the menu |
| Q6 respond-off scope | 8CH respond 0, then one read and one F1H; restore 8CH 01 01 and confirm by read (MK-D11) | If reads stop replying, restore through the menu; no blind repeats |
| Q7 host silence | 98H with a short time during a bounded F6H run-time run; vary which frames are sent (own-ID reads, other-ID frames, none); record stop type, enable after, any frame and the value after restart (MK-D18) | Restore 98H to 0 and read back; an observed stop is not a safety qualification |
| Q8 power-on state | After a 41H restart and a power cycle: 3AH, 31H, 33H, stream and latch state (MK-D13, D19, D30) | Power cycle needs the owner; 41H only with FFH and 9AH read off |
| Q9 completion | Small encoder moves with 95H at default and disabled; F7H, F7H 01 and mode stops during a move; an F5H update during a move; immediate and completion reply latency (MK-D17, D36) | No commanded stall or load; no repeated trigger after a lost reply |
| Q10 ID and bitrate | One 8BH or 8AH change with old and new tuples reserved: which tuple carries the reply and when it applies; restore and confirm (MK-D20) | Persistent; one change at a time; restore through the menu if the node is lost |
| Q11 persistence | Change one named setting per persistence class, restart, compare; one 3FH restore with full before and after snapshots (MK-D21, D22) | No repeated save campaign; 3FH only with restart control and a complete restore plan |
| Q12 streams and latching | 01H on 31H at a few periods while polling other codes; error counters and bus load; 4AH with one latched small move, one 4BH, then a second 4BH (MK-D12) | Second 4BH only on a free shaft with a small move; the library never repeats it |
| Q13 homing and limits | Origin-switch and mechanical homing on the actual switch or stop fixture; 3BH, F1H and 31H after homing; homing onto the left switch with EndLimit (MK-D16, D25) | Single-turn method only if the owner accepts arming power-on homing (MK-D37); never disable limits |
| Q14 malformed frames | One reviewed frame each with a wrong check byte, a wrong DLC and an undocumented code at a stationary released drive (MK-D09) | Separate written plan; never a motion code; stop at the first unexpected effect |

6. If a question fails or surprises, preserve the scenario, reproduce deliberately within authorization and measure
   competing causes; never hide it with retries, longer timeouts or a later pass.

## Deliver

A dated report `docs/reports/mks_servo_can_12a_<date>.md` of the questions run: prerequisites, raw results, restorations
and unresolved measurements. Ledger discrepancy dispositions updated with evidence references (board, firmware,
edition, trace path); fixture-specific qualification applied only through the ordinary API and console routes; deferred
rows for unrun questions; PROGRESS hardware column updated.

## Acceptance

Every run question has retained raw evidence and a restore read-back; every changed setting ended restored or is
recorded as left changed with the reason. A result is scoped to its board, firmware and edition and closes a
discrepancy only for that scope. Unrun questions stay named gaps. Software remains implementable and testable without
this stage; release qualification stays with [12B](12_qualification_and_release.md). Commit and push the report and
ledger updates.
