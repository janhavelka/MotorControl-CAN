# 12a — First-contact questions

Execute this stage under the [execution contract](execution_contract.md).
Status: NOT STARTED. Hardware NOT TESTED. Runs only when an exact DS2-family fixture and applicable user
authorization exist and the owner-absence rules of [COMMON.md](../COMMON.md) no longer apply. It grants no
authorization and never uses the CL86-C bench. Each question runs once its route exists: Q1 and Q2 after 03, Q3
after 04, Q4..Q7 after 05, the others after their stage. Software stages never wait for it.

## Work

1. Before first power, record the fixture:
   - exact model, full suffix and nameplate; supply (24..70 VDC per DS2C p5) and its current limit;
   - transceiver, adapter, termination and node DIP positions, and the bitrate as set with the vendor tool;
   - DI wiring and the declared assignments, DO loads, brake presence and how it is wired;
   - free-shaft or load condition, mechanical stops and who, if anyone, can observe the shaft.
2. Wait at least 10 s between power-off and power-on (DS2OP p9). Use the production API, the `ds2` console and the
   normal debug observer; retain firmware identity, settings before and after, raw traffic, timing bounds, service
   gaps and restoration results. No question repeats an uncertain write automatically.
3. Answer the questions below in order of their prerequisites.

| Question | Work when prerequisites exist | Remaining limit |
| --- | --- | --- |
| Q1 identity | Read `1000`, `1018:00`..`03`, `:04` only if announced, `1009`, `100A`; compute the identity outcome against the declared model (DS-D01, DS-D02, DS-D03, DS-D05). Add a model- and firmware-scoped MEASURED identity row to the ledger. | One specimen settles neither claim for other units or firmware. Never read undocumented objects to learn more. |
| Q2 snapshot | Read 03's state and configuration groups, `607D:01/:02`, `6091`, both PDO record sets, `1014`, `1017`, `605D`, and the IBL `2406h` where declared. No writes. | Unchanged `607D` defaults may order its sub-indexes (DS-D15) only if the values are the printed defaults. NMT state stays unknown without a heartbeat. |
| Q3 bus control and enable | With a free shaft, mode 1 and no trigger, run 04's enable and record bits 0..2 after each word, then each release action (DS-D29). | No enable in modes 3 or 4 here. A failed enable is recorded, not retried; `P02-00` changes are vendor-tool work outside the library. |
| Q4 status dialect | One small nonnegative absolute move; poll `6041` and record bits 10, 12 and 15 (or 3 and 10) before trigger, after acknowledgement and at rest (DS-D04). | One run is fixture evidence for that model and firmware. No observed transition leaves the dialect unresolved; no automatic repeat. |
| Q5 units and scale | Compare `607A` delta, `6064` delta and an independent shaft measurement with `6091` and the declared encoder; if no independent measurement exists, record "not observed". | Command-scaled feedback alone cannot qualify the encoder relation. |
| Q6 ramp unit | Two short finite position moves with different `6083`/`6084` values, distance small enough that both readings (ms or pul/s²) stay bounded; fit `6064`/`606C` time series (DS-D16). | Restore the prior ramp words and read them back. Homing `609A` needs its own run in Q10. |
| Q7 rate unit | One finite position move with a small `6081`; compare `606C` and the move duration with pul/s (DS-D16, DS-D33). | Settles the unit for that firmware only. |
| Q8 negative encodings | Not executed as proposed. Obtain vendor evidence, or first qualify a drive-side bound (ordered and active `607D`) under a separately reviewed plan whose worst case under the U32 reading is bounded by that drive limit (DS-D09, DS-D10). | A host stop after link loss is not a bound. |
| Q9 stops | During a small finite position move send `QUICK_STOP`; record the reported state, `606C`, `6085` effect and whether bits 0..2 change. Then release. | Halt, velocity-mode and torque-mode stops need their own bounded plans; two stable samples do not prove standstill. |
| Q10 velocity, torque, homing | Each only with a written bounded plan: velocity at low speed and short duration; torque at low `6071` with a low speed limit (`607F` or `2406h`) and short duration; homing method 33 or 34 only when an index is declared, or a switch method with real switches. | Whether the speed limit clamps torque mode (DS-D11) and whether the encoder provides a Z pulse are the questions, not assumptions. |
| Q11 PDO and EMCY | With an explicit plan, NMT start and record TPDO frames against the Q2 readback maps; observe naturally occurring EMCY only. | No remap, no RPDO frame, no fault induction (DS-D06, DS-D19). |
| Q12 persistence | With actual restart control, change `1017` (and later one 6000-group setting), power-cycle, read back and restore (DS-D20). | No save path exists; never write `1010`, `1011` or `2xxx` to find one. |
| Q13 absolute position | R17 or R23 only: record `6064` before power-off and after power-on with the shaft untouched (DS-D25 and 08's open question). | A person must confirm the shaft did not move; otherwise "not observed". |
| Q14 limit and home inputs | With the drive released, a person actuates each wired switch; record `60FD` bits 0..2 for each state against the declared assignments (DS2C p9, p28). | Unwired terminals still have their factory function; no assignment is changed by the library. |
| Q15 brake | Only on a model and fixture with a brake and a safe load: record enable, release and stop timing against the declared brake (DS-D26). | The library never drives the brake; vertical loads need their own fixture plan. |

4. Order and rules:
   - Run Q1 and Q2 first. Stop if the identity outcome is CONTRADICTED until the declaration is checked against the
     nameplate. Q3 precedes every motion question. Q8 never runs without its stated preconditions.
   - Each motion question names its bound (distance, duration, speed, torque), its stop step and its restoration
     before the first frame. A question that needs a person (Q5, Q13, Q14, Q15) records "not observed" when nobody
     is present.
   - Acquiring and wiring a DS2-family fixture is an owner action: while the owner is away it is a row in the
     [deferred table](../README.md#deferred-to-owner-return), with these questions as the cases it unlocks.

## Deliver

- A dated report `docs/reports/lichuan_ds2_canopen_12a_<date>.md` with the fixture record, the questions actually run, their
  prerequisites, raw evidence links, results and the measurements still missing.
- Ledger rows scoped to the specimen's model and firmware (observed identity, dialect, units, ramp, stop and input
  observations), with the DS-D or `DS-G` each one narrows. No DS-D is closed for other models or firmware.

## Acceptance

Every question run has its bound, stop step and restoration recorded before its first frame, and its raw evidence
retained. Qualifications enter only through the ordinary declarations and ledger rows; no library default changes
because one specimen behaved one way. Release qualification stays with [stage 12](12_qualification_release.md).
