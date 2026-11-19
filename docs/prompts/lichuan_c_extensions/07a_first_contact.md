# 07a — First contact per model

Execute this stage under the [execution contract](execution_contract.md).
Status: NOT STARTED.

Run only when an exact TL-C or CL2 fixture exists, the owner has returned (COMMON's owner-absence section is
no longer in force) and the applicable authorization covers it. The standing authorization and the S2/COM15
fixture describe two CL86-C drives; they establish no TL-C or CL2 fixture. Use a separate bench configuration
and interface for the new drive; never put it on, or address it through, the CL86-C nodes on COM15. Questions
run per model as soon as their route exists: reads after [03a](03a_tlc_rows.md) or [04a](04a_cl2_rows.md),
motion after [03b](03b_tlc_operations.md) or [04b](04b_cl2_operations.md). This stage answers questions; it
does not qualify a release ([07b](07b_qualification_and_release.md) does).

## Work

Hardware is NOT TESTED for every model.

### Before first power

Record nameplate and order code, the declared model, encoder type, DIP positions (TL-C SW1..SW4 node, SW5
bitrate, SW6 termination; CL2 SW1..SW5 node, SW6/SW7 bitrate, SW8 termination), wiring of every input (TL-C
defaults DI0 origin, DI1 positive and DI2 negative limit are factory claims, TLCEN p7..8), brake, free shaft
and transceiver. Derive the model's forbidden-object list from its ledger rows.

Fixture electrical record, reviewed by the owner before first power; without it every bench step is NOT RUN.
For the exact unit it states each item below from applicable evidence (the unit's own terminal marking or
label, or the vendor's written clarification), never by choosing between conflicting manual texts:

- Power connection and polarity. TLCEN p6 describes both power pins, VDC and GND, as "DC Power Negative
  Terminal" (LX-D14). The CL2-86C main terminals are both labelled AC while DC 30..110 V is also allowed,
  with no DC polarity stated (CL286 p5, LX-D31).
- Supply voltage inside every documented range for the frame size: LX-D09 for TL-C (for example TL86
  24..70 V on TLCEN p6 against DC24..48 V in the TLCEN p7 figure), LX-D31 for CL2-86C. The narrowest
  documented range is a ceiling, not bench permission (X14).
- Every connected output and common terminal, the brake release included (LX-D10: DO0/DO1 on TLCEN p6..8
  against DO1/DO2 with DO2 as brake release in the TLCEN p7 figure and TL57OP p4; TLCEN p7's note that DOCOM
  may only go to 0 V, on a model without a DOCOM pin; DI 24 V on TLCEN p6 against 12..24 V on TL57OP p4).
  Outputs stay unconnected, and a motor with a brake stays unused, until the record resolves them; inputs
  are wired only as the record states.

Use the production API and the normal debug observer; retain raw traffic, timing, service gaps, settings
snapshots and restoration results. Never replay an uncertain write; no automatic retry.

### Evidence paths

Each question follows canopen_shared 02 "First-contact questions". "Production" names the route that admits
it at the stage where it runs, with its prerequisites. "Experiment" means it runs only as a separately
assigned, isolated and independently reviewed experiment under that section, naming its permitted actions,
observation, original-value snapshot, independent power cutoff, restoration boundary and failure boundary;
without one it is NOT RUN, with the evidence it still needs. No question uses a raw console or frame bypass,
a temporarily opened refusal or 03b/04b admission without its evidence record. Every production route that
changes the drive (enable, alarm clear, save) also needs T1's or K2's record loaded, because prompt 57's
precedence refuses it without a declared-model match. Ordinary reads (T2, T3, T7, K6, K8) are refused while
the binding is CONTRADICTED (execution contract, routes by outcome), so they run after that record is
loaded or under another outcome; targeted NMT has no route. Observations attach to their gates; first
contact opens no gate (07b or a later reviewed change scoped to the model does).

### TL-C questions

| Question | Evidence path and work | Remaining limit |
| --- | --- | --- |
| T1 identity (X01) | Production, after 03a: `prepareIdentity` (03a item 1) with the serial option. Read `1000`, `1009`, `100A` (record reply widths), `1018:00..04`, `2000`, `2001`; retain the raw observation and compare it with the CL86-C record. It becomes a MEASURED reference only when entered with its provenance id in the application's static configuration (06 item 2, the one loading path); until then it is evidence, the binding has no TL-C match and every changing request stays refused (prompt 57), so T5 and later questions need it loaded | Equal values mean identity cannot discriminate; one specimen is not a family |
| T2 bitrate candidates (X02) | Production: 03a's raw input-filter read, explicit single uploads. With SW5 OFF (1 Mbit/s, TLCEN p9), read `2402` and `2407` once each; compare with TLEDS defaults 40 and 16 and codes 0..3 | No write. A selector change is an experiment needing vendor clarification or a reviewed plan with SW5 ON, one code, one save, a restart, a host ready at the new bitrate and SW5 OFF as the documented way back |
| T3 node selection (X03) | Production: the raw `2203` read (`ReadKind::COMMUNICATION_NODE`) and passive boot-up observation (03a item 6). With an owner-set all-OFF DIP, observe which node boots | DIP changes and power cycles are owner actions; no `2203` write |
| T4 status table (X04) | Production: 03a's TL-C state read, at power-up and after T5's enable. Transitions during and after a move: experiment | Bits 3/8/14/15 freshness needs transitions after a trigger, not a stable bit |
| T5 enable and start words (X05) | Production, after 03b with T1's record loaded: 03b item 2's `ATTENDED_EXPERIMENT` enable. State-read plan: one fresh 03a state read (`StateBlock` STATUS, MONITORS, FEEDBACK and ERRORS through the model-taking getter, under the reservation) must show a starting state 03b's TL-C source-state table admits and `6061` = 1. Earlier explicit single uploads of `607A` and `60FF` give the reviewed values the enable evidence holds, and `prepareEnable` uploads them again before word 6 and stops on any difference (`Actions.h`). The reviewed `607A` must equal the read's `6064` as raw values, but that is a screen, not the prerequisite: TLCEN p10 ("Current Position") and p15 ("actual position") list `6064` as I32 with no unit beside `607A`, so the basis stays X16's question, and on TLO `6064` is a controller report. The production route runs only with 03b item 2's basis record for this fixture (both values on one coordinate basis, scale and origin); without it T5 runs only as a separately assigned experiment whose plan explicitly allows and bounds unexpected motion (travel, speed and the cutoff that ends it), or is NOT RUN. Before word 6 only stop readiness is established, never a prepared stop: the predecessor-15 HALT or quick stop is a reviewed 03b stop-table row, the channel's urgent path is free and the independent power cutoff is attended. The stop is prepared only from a fresh read showing predecessor 15 after word 15's acknowledgement. Nothing is written to make these hold: no TL-C mode select exists before prompt 57 and 03b item 12, and no target write outside an admitted move. TLEDS declares `6060` and `6061` 0 and `607A` 5000 (l.1088, l.1096, l.1119), declared defaults that would fail both checks. Partial enable: if the sequence ends with word 15 known unsent, nothing further is sent, the last observed state is recorded and the power cutoff is the only stop. If word 15 was possibly sent with no acknowledgement or no fresh observation, no predecessor is admitted, the route may be unresolved and TL-C has no best-effort shape (03b item 3): the attendant uses the power cutoff and the result keeps the enable state unknown. If word 15 is acknowledged and a fresh read shows predecessor 15, the stop is prepared from it. A reviewed plan that does not cover these branches leaves T5 an experiment or NOT RUN. Whether `0x010F` as third word enables or halts: experiment | `0x011F` start and method 28 stay untested without vendor clarification (LX-D08) |
| T6 velocity order (X05) | Experiment: one small finite velocity run per order (targets first; enable first), stop prepared, duration bounded | Writing 0 is not a stop; host duration is not a drive guarantee |
| T7 I/O (X06) | Production: 03a's input, output and monitor reads of `2400:01..0F`, `2101`, `2102`; map each physical input. Each output with an isolated load: experiment (output functions stay gated, 03b item 9) | No reassignment to provoke a function; never disable a limit |
| T8 homing (X17) | Experiment: methods 17/18/24/29 only with the actual switches and a bounded plan; record final coordinate and bit 8 | Method 28 is not run |
| T9 alarm reset (X07) | Production, with T1's record loaded, only on 03b item 7's admitted row: `prepareClearAlarm` with `CONTROLWORD_BIT7` on a naturally occurring alarm whose fresh read shows bit3, `603F` FF05, mode 1, low bits 111 and bit14 clear. Any other report is a refused or gated row: NOT RUN, or an assigned experiment. A `2301` read has no TL-C route: experiment | No fault induction without its own plan |
| T10 save (X18) | Production: 03b item 11's `prepareSave` with its bitrate acknowledgement, with restart control: change one named field, save once, restart, compare; read `2402`/`2407` before and after | No save campaign. The library writes no selector, but a selection staged earlier can take effect at the restart (TL57OP p5); the host is ready to reacquire at the candidate bitrates |

### CL2 questions

| Question | Evidence path and work | Remaining limit |
| --- | --- | --- |
| K1 SDO route (X09) | Production: `prepareProbe` (`1000:00`, documented on CL257 p14) on `0x600+node`; record reply or silence | A reply proves a CANopen server, not the model |
| K2 identity (X01) | Production, after 04a: 04a item 2's identity read with the `1018` option on. Read `1000`, `1009`, `100A`, `1018:00..03`, `2000`, `2001`; keep aborts raw; retain the observation, which becomes a reference only through 06 item 2's static configuration, as in T1 | Abort on `1018` is evidence about `1018`, not about the model |
| K3 communication records (X09) | Experiment, after the owner accepts the read risk: explicit single reads of `1001`, `1005`, `1014`, `1017`, `1200:01/02`, `1400..1403`, `1800..1803`, `1A00..1A03`. Production: passive boot-up and heartbeat observation (04a item 1) | Read effects stay unqualified; no writes |
| K4 bit 10 by mode (X08) | Production: 04a's state read in the current mode, at power-up and after K5's enable. Mode 1 move and mode 6 homing transitions: experiment; mode 3: observe without interpretation | One transition is fixture evidence, not every firmware |
| K5 monitor bits (X12) | Production, after 04b with K2's record loaded: 04b item 2's `ATTENDED_EXPERIMENT` enable under T5's state-read plan and conditions: CL2 source-state table; `6061` = 1 from 04a item 4's bracketed acquisition; the reviewed `607A` and `60FF`; raw `607A` equal to `6064` as a screen only, since CL257 p11 and p19 give `6064` no unit, so whether it counts command pulses or encoder counts is undocumented; the production route needs 04b item 2's basis record for this fixture, otherwise an assigned experiment that bounds unexpected motion, or NOT RUN. Stop readiness before word 6 and a stop prepared only from a fresh predecessor-15 read after word 15; nothing written to make them hold; T5's three partial-enable branches. Otherwise experiment or NOT RUN. Then 04a's monitor read: `2100` bit4 released against enabled; bits 5/6 against wired limit inputs actuated by the owner | Polarity holds for this firmware only |
| K6 loop and encoder (X10) | Production: 04a's control read of `2500` and `2501`; compare `2501` with the encoder declared at P00-13 by the vendor tool | No `2501` write; no motor matching through CAN |
| K7 homing (X11) | Experiment: one method per available signal set (limit, home, Z on ABZ with the Z fixture record), bounded, with final coordinate and bit 10 | Index methods on CL2-57C/86C are NOT RUN: no documented Z source (04b item 6) |
| K8 input codes (X12) | Production: 04a's input-configuration read of `2400:02..08`. Code 4 (release): experiment, with the drive released | No input reassignment to make a test run |
| K9 faults (X13) | Production: 04a's state read of `603F` (without `1001`) and LED codes against CL257 p20, for naturally occurring faults; `1001` only inside K3's experiment. An alarm clear is NOT RUN: 04b item 7's rows stay GATED until these observations give a CL2 fault record, and then run only on the row it admits | No battery or encoder fault induction |

## Deliver

`docs/reports/lichuan_c_extensions_07a_<model>_<date>.md` per fixture: questions run, each with its evidence
path, prerequisites, raw evidence paths under `docs/bench/`, results, unresolved measurements; the fixture
electrical record; identity records entered through the application's static configuration (06 item 2) and
other qualification inputs through ordinary API/CLI routes; observations attached to their ledger gates,
every gate still open; `PROGRESS.md` hardware column.

## Acceptance

No power is applied before the fixture electrical record exists. Every question run has retained raw traffic
and restoration; every question not run says why (no fixture, owner away, no authorization, no electrical
record, missing signal, unmet precondition, no assigned experiment) and what evidence it still needs. No
result is generalized beyond its model, firmware and fixture. Software work never waits for this stage.
