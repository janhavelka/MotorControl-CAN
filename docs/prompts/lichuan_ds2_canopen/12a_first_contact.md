# 12a — First-contact questions

Execute this stage under the [execution contract](execution_contract.md).
Status: NOT STARTED. Hardware NOT TESTED. Runs only when an exact DS2-family fixture and applicable user
authorization exist and the owner-absence rules of [COMMON.md](../COMMON.md) no longer apply. It grants no
authorization and never uses the CL86-C bench. Each question records one evidence path under canopen_shared 02
"First-contact questions": a production route that admits it (named with its prerequisites), applicable vendor
evidence, or a separately assigned, reviewed experiment; otherwise it is NOT RUN with the evidence it still needs.
A production route is the `ds2` console route (which needs S4) or an installed example that calls the public API
through the real owner; without either the question is NOT RUN, and no raw read or frame path is created. Q1 and
Q2a run after 03, Q2b after 09, Q3 after 04, Q4..Q7 after 05, the others after their stage. Software stages never
wait for it.

## Work

1. Before first power, record the fixture and sign it off:
   - exact model, full suffix and nameplate; the supply range from that model's own manual and product page and the
     motor voltage code. Where they conflict (DS-D25: PG-IBL60 DC 24..50 V versus IBL60 p5, p7 DC 24..70 V) the
     supply setting needs a documented safe choice or manufacturer clarification before power, never the higher
     conflicting limit; record the supply's current limit;
   - the model's input interface (DI levels and common wiring, from its own manual), DI wiring and the declared
     assignments, DO loads, and brake versus braking-resistor terminal identification: DS2 BRK+/BRK- drive a brake,
     IBL and ISL BK-/BK+ are braking-resistor terminals with the brake on FunOUT.9, MINI shows no brake pins but a
     brake in its wiring figure (DS-D26); brake presence and how it is wired;
   - transceiver, adapter, termination and node DIP positions, and the bitrate as set with the vendor tool;
   - free-shaft or load condition, mechanical stops and who, if anyone, can observe the shaft.
2. Wait at least 10 s between power-off and power-on (DS2OP p9). Use the production API, the `ds2` console and the
   normal debug observer; retain firmware identity, settings before and after, raw traffic, timing bounds, service
   gaps and restoration results. No question repeats an uncertain write automatically.
3. Answer the questions below in order of their prerequisites.

| Question | Work when prerequisites exist | Remaining limit |
| --- | --- | --- |
| Q1 identity | Read `1000`, `1018:00`..`03`, `:04` only if announced, `1009`, `100A`; compute the identity outcome against the declared model (DS-D01, DS-D02, DS-D03, DS-D05). Add a model- and firmware-scoped MEASURED identity row to the ledger. | One specimen settles neither claim for other units or firmware. Never read undocumented objects to learn more. |
| Q2a device snapshot | After Q1 with an outcome the execution contract's table admits for configuration reads: 03's state and configuration groups (`607D:01`, and `:02` where the scope sources it, `6091`, `605D`, the IBL `2406h` where declared) through 03's route. No writes. | Unchanged `607D` defaults may order its sub-indexes (DS-D15) only if the values are the printed defaults. |
| Q2b network snapshot | After 09: both PDO record sets, `1014` and `1017` through 09's read plan. No writes. | NMT state stays unknown without a heartbeat. Only Q11 needs it. |
| Q3 bus control and enable | With a free shaft, mode 1 and no trigger, run 04's enable and record bits 0..2 after each word, then each release action (DS-D29). | No enable in modes 3 or 4 here. A failed enable is recorded, not retried; `P02-00` changes are vendor-tool work outside the library. |
| Q4 status dialect | One absolute move to a nonnegative target, bounded under the bounded-move rule (item 4); record bits 10, 12 and 15 (or 3 and 10) before trigger, after acknowledgement and at rest, through 05's observation cycles under `DS-DEC-1` or, without it, explicit single state reads (DS-D04). | One run is fixture evidence for that model and firmware. No observed transition leaves the dialect unresolved; no automatic repeat. |
| Q5 units and scale | Compare `607A` delta, `6064` delta and an independent shaft measurement with `6091` and the declared encoder; if no independent measurement exists, record "not observed". | Command-scaled feedback alone cannot qualify the encoder relation. |
| Q6 ramp unit | Production admits ramp writes only under a declared `RampKind` (05), so this question cannot use it to discover the unit, and a provisional declaration is never set as if it were the result. Evidence paths: manufacturer clarification, other applicable model-scoped evidence, or a separately assigned and reviewed conditional experiment that names the permitted `6083`/`6084` values and order, the snapshot before its first change, the observation (two short moves small enough that both readings, ms or pul/s², stay bounded; `6064`/`606C` time series) and its restoration boundary (DS-D16). | Otherwise NOT RUN. Homing `609A` needs its own run in Q10. |
| Q7 rate unit | One finite position move bounded under item 4's rule, with a `6081` value whose speed stays bounded both as pul/s and as rpm; compare `606C` and the move duration with pul/s (DS-D16, DS-D33). | Settles the unit for that firmware only. |
| Q8a negative position targets | Not executed as proposed. Obtain vendor evidence, or first qualify a drive-side bound effective in mode 1 (ordered and active `607D`) under a separately reviewed plan whose worst case under the U32 reading is bounded by that limit, with its speed and load consequences (DS-D09). | A host stop, or any bound that needs timely host service, is not a bound. Otherwise NOT RUN. |
| Q8b negative velocity targets | Not executed as proposed. A `607D` travel limit is no bound on speed in mode 3. Obtain vendor evidence, or a separately reviewed bound effective in mode 3 for the worst-case U32 reading of a negative word, independent of host service, such as a fixture that limits speed and load by itself (DS-D10, DS-D32). | The clamp or stop reaction under investigation never counts as the bound. Otherwise NOT RUN. |
| Q9 stops | During a finite position move bounded under item 4's rule, so that it ends within its bound if the stop has no effect, send `QUICK_STOP`; record the reported state, `606C`, `6085` effect and whether bits 0..2 change. Then release. | Halt, velocity-mode and torque-mode stops need their own bounded plans; two stable samples do not prove standstill. |
| Q10 velocity, torque, homing | Each only with a written bounded plan whose protective bound holds without timely host service (host loss has no documented drive action, DS-D32) and is not the behavior under investigation: velocity at low speed and short duration; torque at low `6071` only for a model whose torque operation 06b admits, and never relying on the `607F` or `2406h` limit or the host stop as the bound; homing method 33 or 34 only when an index is declared, or a switch method with real switches. | Whether the speed limit clamps torque mode (DS-D11) and whether the encoder provides a Z pulse are the questions, not assumptions. Without such a bound, from vendor evidence or an independently bounded fixture, the run is NOT RUN. |
| Q11 PDO and EMCY | With an explicit plan, NMT start and record TPDO frames against the Q2b readback maps; observe naturally occurring EMCY only. | No remap, no RPDO frame, no fault induction (DS-D06, DS-D19). |
| Q12 persistence | With actual restart control, change `1017` (and later one 6000-group setting), power-cycle, read back and restore (DS-D20). | No save path exists; never write `1010`, `1011` or `2xxx` to find one. |
| Q13 absolute position | R17 or R23 only: record `6064` before power-off and after power-on with the shaft untouched (DS-D25 and 08's open question). | A person must confirm the shaft did not move; otherwise "not observed". |
| Q14 limit and home inputs | With the drive released, a person actuates each wired switch; record `60FD` bits 0..2 for each state against the declared assignments (DS2C p9, p28). | Unwired terminals still have their factory function; no assignment is changed by the library. |
| Q15 brake | Only on a model and fixture with a brake and a safe load: record enable, release and stop timing against the declared brake (DS-D26). | The library never drives the brake; vertical loads need their own fixture plan. |

4. Order and rules:
   - Run Q1 and Q2a first; Q2b is a prerequisite of Q11 only. Stop if the identity outcome is CONTRADICTED until the
     declaration is checked against the nameplate. Q3 precedes every motion question. Q8a and Q8b never run without
     their stated preconditions.
   - Each motion question names its bound (distance, duration, speed, torque), its stop step and its restoration
     before the first frame. A question that needs a person (Q5, Q13, Q14, Q15) records "not observed" when nobody
     is present.
   - Bounded-move rule, the Q8 and Q10 bound applied to every question that commands motion (Q4 to Q10) and to 12's
     regression and qualification runs. Every mode: a protective bound, written before the first frame, that holds
     under every interpretation the run still depends on, without timely host service, and independently of the
     behavior under test (a stop's reaction, a clamp or the unit being measured never counts). Position runs (Q4,
     Q5, Q7, Q9, 12's moves) add: the bound covers the `pul` scale and encoder relation (DS-D34), the `6081` rate unit
     (pul/s or rpm, DS-D16 and DS-D33) and the ramp unit (DS-D16); the target is absolute and within the bounded
     displacement of a fresh `6064` read (Q8a's qualified drive-side limit is its own case); a relative move runs only
     with 05's verified-basis declaration and its own separately justified displacement bound. Velocity, torque and
     homing (Q8b, Q10) use the mode's own bound those rows name (speed and duration, torque and load, travel between
     real switches or to a declared index), never a target displacement. Without the bound its mode needs the
     question is NOT RUN.
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
