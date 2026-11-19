# 12A — Nanotec first-contact questions

Execute this stage under the [execution contract](execution_contract.md).
Status: NOT STARTED; hardware NOT TESTED. Runs only when an identified Nanotec
fixture and the user's authorization for it exist. No Nanotec fixture exists, and
none can be connected while the owner is away (COMMON). This stage never uses the
CL86-C bench. Questions run as soon as their API route exists: Q1-Q4 after 03,
Q5-Q7 after 04 and 04b, Q8 after 05, the rest after their stage.

## Work

Before first power, record model, nameplate hardware revision, motor and feedback
configuration, free-shaft or load arrangement, switch positions, transceiver, board
and image. Derive the forbidden and RAM-only object lists from the ledger. Use only
the production API and the ordinary debug observer, retain raw traffic, timing and
service gaps, restore every RAM change and read it back, and never replay an
uncertain write. A saved 6040h, 6060h or 2300h can make a unit behave differently
from its manual at first enable (review section 10).

| Question | Work when prerequisites exist | Remaining limit |
| --- | --- | --- |
| Q1 identity (NT-D01, D02, D03, D14, D29) | Read 1000h, 1008h, 1009h, 100Ah, 1018h:01-04, 4040h. Record which firmware the unit ships and, for PD4-C, which product code it reports. | One unit is one sample; it does not say what current production ships. A refused row stops here except for further reads. |
| Q2 boot and NMT (NT-D06, D17) | Power on with the host listening: count boot-up messages and their spacing, observe 2007h:01, 2084h and 1F80h, and the NMT state reached. | Boot spacing is a fixture fact, not a documented delay. |
| Q3 control paths (04b, NT-D32) | Read 2290h, 2300h, 2301h, 2302h, RPDO maps and 60F2h. Report Plug & Drive and NanoJ state before any write, including whether a stored program is running right after power-on. | A stored NanoJ program is not inspected or changed here. |
| Q4 configuration snapshot | Read the 03 groups: 3202h, 3203h, units, reactions 605Ah-605Eh, 3700h, 3701h, 6007h, 1029h, 1016h, 1017h, 3240h, 6098h, 607Dh, 1020h. | No write; no inference of wiring from input levels. |
| Q5 bitrate code (NT-D04) | With restart control and a recovery switch position, set 2005h to 87h, store 1010h:0Ah, restart, observe the bitrate; restore 88h. | Only with explicit authorization of a communication change; never during the owner's absence. |
| Q6 enable side effects | With brake, auto alignment and commutation settings recorded, run the 04 enable on a free shaft; observe alignment motion and bit 15. Release with each kind. | Shaft observation needs a person or an instrument. |
| Q7 stops | Small finite PP move, then HALT and QUICK_STOP with recorded 605Ah and 605Dh; record the ending state and standstill evidence. | Two quiet samples do not prove standstill. |
| Q8 PP and units (NT-D19) | One small relative and one absolute move with 60A8h at its preset; compare 6064h, 6063h and independently observed rotation; record bit 12 and bit 10 timing and the 60F2h basis. | Open-loop 6064h is commanded position. |
| Q9 homing | Method 35 as a coordinate operation; a switch or index method only with that fixture. Record the 13/12/10 pattern after the trigger. | No limit disabled to make a method run. |
| Q10 host loss (NT-D08, D24) | Only after seam S11 has landed and the 09b 1016h setter passed its tests: configure 1016h and 6007h in RAM, stop the heartbeat during a bounded slow move, record EMCY, NMT and drive reactions and their order. | Needs a separate reviewed plan: the motor may coast. |
| Q11 persistence (NT-D09) | With restart control: change one named communication-category value, save 1010h:02, restart, compare; remap one PDO, save, restart, check which category kept it. | One save per change and one to restore; no save campaign. |
| Q12 LSS (NT-D05) | Only with separate authorization: a single LSS identify or inquire exchange with one node on the bus. | No configure, store or bit-timing service. |
| Q13 FIR-v2538 (NT-D02) | Only on a unit that reports it: read 2007h:02 and the PDO COB-IDs. | Read-only; the row stays refused until stage 13. |

## Deliver

A dated report `docs/reports/nanotec_canopen_12a_<date>.md` of the questions
actually run, their exact prerequisites, raw results and restoration outcome, and
the measurements still missing; ledger and coverage updates scoped to that unit's
model, firmware build and configuration, applied through ordinary API and ledger
routes.

## Acceptance

Each answered question cites its raw evidence and scope; each unanswered one names
the missing prerequisite. No result is generalized to another model, firmware or
motor configuration, and no setting is left changed. Release qualification stays
with [12B](12_qualification_and_release.md).
