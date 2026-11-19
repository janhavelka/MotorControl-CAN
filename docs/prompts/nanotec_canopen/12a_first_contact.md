# 12A — Nanotec first-contact questions

Execute this stage under the [execution contract](execution_contract.md).
Status: NOT STARTED; hardware NOT TESTED. Runs only when an identified Nanotec
fixture and the user's authorization for it exist. No Nanotec fixture exists, and
none can be connected while the owner is away (COMMON). This stage never uses the
CL86-C bench. It qualifies a fixture and never opens a gate (canopen_shared 02
"First-contact questions"). Each question below names one executable evidence path:
a production route that admits it at the stage where it runs, with its prerequisites;
vendor or other applicable manufacturer evidence; or a separately assigned, isolated,
independently reviewed experiment under the
[conditional experiment prerequisites](../../device_conflicts.md#conditional-experiment-prerequisites).
A question runs as soon as its route exists; without a path it is NOT RUN with the
evidence it still needs. No raw console or frame bypass, no weakened or temporarily
opened production refusal.

## Work

Before first power, record model, nameplate hardware revision, motor and feedback
configuration, free-shaft or load arrangement, switch positions, transceiver, board
and image. Derive the forbidden and RAM-only object lists from the ledger. Use the
named production routes and the ordinary debug observer, retain raw traffic, timing
and service gaps, and never replay an uncertain write. Before a question's first
change, snapshot the original value of every object it may change; restore to that
snapshot (never a presumed default), read it back, and record what stays changed if
the restoration fails. A saved 6040h, 6060h or 2300h can make a unit behave
differently from its manual at first enable (review section 10). Every observation is
that specimen's behavior.

| Question | Evidence path and prerequisites | Work | Remaining limit |
| --- | --- | --- | --- |
| Q1 identity (NT-D01, D02, D03, D14, D29) | 03 `prepareIdentity` | Read 1000h, 1008h, 1009h, 100Ah, 1018h:01-04, 4040h. Record which firmware the unit ships and, for PD4-C, which product code it reports. | One unit is one sample; it does not say what current production ships. An unqualified row continues with its stage 13 gates; a refusing product outcome (prompt 57 "Admission by outcome") stops every changing question here, leaving bootstrap identity and state reads. |
| Q2 boot and NMT (NT-D06, D17) | 03's boot group (2007h:01, 2084h) and supervision group (1F80h) and the 09 boot-up observer, after 09 | Power on with the host listening: count boot-up messages and their spacing, read 2007h:01, 2084h and 1F80h, and observe the NMT state reached; record whether an identity read sent between two boot-ups is answered and with which values. | Boot spacing and boot-loader SDO behavior are fixture facts, not documented ones. A recorded unanswered identity read between the two boot-ups is the observation 09 item 8 accepts for readiness on that model and firmware row; one answered, or none recorded, keeps readiness UNKNOWN there. |
| Q3 control paths (04b, NT-D32) | 03 `ControlPaths` reads | Read 2290h, 2300h, 2301h, 2302h, RPDO maps, 60F2h and on PD4-C 4015h. Report Plug & Drive and NanoJ state before any write, including whether a stored program is running right after power-on. | A stored NanoJ program is not inspected or changed here. |
| Q4 configuration snapshot | 03 `prepareConfig` | Read the 03 groups: 3202h, 3203h, units, reactions 605Ah-605Eh, 3700h, 3701h, 6007h, 1029h, 1016h, 1017h, 3240h, 6098h, 607Dh, 1020h. | No write; no inference of wiring from input levels. |
| Q5 bitrate code (NT-D04) | No production route: 10 item 2 refuses 87h. Vendor evidence, or a separately assigned reviewed experiment naming its actions (2005h = 87h, store 1010h:0Ah, restart), the observation (bitrate on the bus), the snapshot (2005h, 2009h, switch position) and the restoration boundary (snapshot value stored and restarted; recovery by the model's documented switch position, N5 p37 position 1, which needs physical access) | Only inside that experiment: its actions, observation and restoration in its stated order. Otherwise NOT RUN. | Only with explicit authorization of a communication change; never during the owner's absence. An observed 1 Mbit/s does not show that 87h is a recognized code rather than the object page's fallback for an unlisted value. |
| Q6 enable side effects | 04 `prepareEnable` and `prepareRelease`, after 04 and 04b item 1 | With brake, auto alignment and commutation settings recorded, enable on a free shaft; observe alignment motion and bit 15. Release with each kind. | Shaft observation needs a person or an instrument. |
| Q7 stops | 05 finite PP move and 04 `prepareStop`, after 05 | Small finite PP move, then HALT and QUICK_STOP with recorded 605Ah and 605Dh; record the ending state and standstill evidence. | Two quiet samples do not prove standstill. |
| Q8 PP and units (NT-D19) | 05 moves | One small relative and one absolute move with 60A8h at its preset; compare 6064h, 6063h and independently observed rotation; record bit 12 and bit 10 timing and the 60F2h basis. | Open-loop 6064h is commanded position. |
| Q9 homing | 07 `prepareHome` | Method 35 as a coordinate operation; a switch or index method only with that fixture. Record the 13/12/10 pattern after the trigger. | No limit disabled to make a method run. |
| Q10 host loss (NT-D08, D24) | 09b setters with seam S11 landed and its runtime producer check passing, a 06a finite velocity move, and a separate reviewed plan | Configure 1016h and 6007h in RAM, stop the heartbeat during a bounded slow move, record EMCY, NMT and drive reactions and their order. | The motor may coast. |
| Q11 persistence (NT-D09) | Communication part: 10 `prepareSave` of 1010h:02 under its save gate, with restart control and both guards on: the PDO-persistence guard (no PDO write by this library since a boot-up observed in the owner session) and 04b item 1's single-master guard with its declaration, which together, and only as far as the declaration holds, keep a PDO change other than the boot-loaded configuration from being persisted; the map readback (09 `PdoConfig`) taken before the save is compared with one taken after that boot-up, and any difference stops the question. PDO part: no production route (10 item 7 gates the PDO save); vendor evidence or a separately assigned reviewed experiment | Change one named non-PDO communication-category value, save 1010h:02, restart, compare. The PDO part is NOT RUN without its path. | One save per change and one to restore; no save campaign. |
| Q12 LSS (NT-D05) | No production route: the library has no LSS service (10 item 4). Vendor evidence, or a reviewed experiment under the separate shared LSS assignment | A single LSS identify or inquire exchange with one node on the bus, only with that assignment; otherwise NOT RUN. | No configure, store or bit-timing service. |
| Q13 FIR-v2538 (NT-D02) | 03 identity, 03's boot group (2007h:02 is requested only on that row) and 09's typed COB-ID readback (`PdoConfig`), after 09; the row runs unqualified with its gates | Only on a unit that reports it: read 2007h:02 and the PDO COB-IDs. | Read-only; the row stays unqualified with its gates until stage 13. |

## Deliver

A dated report `docs/reports/nanotec_canopen_12a_<date>.md` of the questions
actually run, their evidence path and exact prerequisites, original-value snapshots,
raw results and restoration outcome, and the measurements still missing; ledger and
coverage updates scoped to that unit's model, firmware build and configuration,
applied through ordinary API and ledger routes.

## Acceptance

Each answered question cites its evidence path, raw evidence and scope; each
unanswered one is NOT RUN with the missing prerequisite or evidence. No result is
generalized to another model, firmware or motor configuration, no production refusal
was opened, and no setting is left changed beyond a recorded restoration failure.
Release qualification stays with [12B](12_qualification_and_release.md).
