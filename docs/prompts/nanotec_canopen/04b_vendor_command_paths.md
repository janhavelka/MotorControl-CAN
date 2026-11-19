# 04b — Vendor command paths: Plug & Drive interface, NanoJ and auto setup

Execute this stage under the [execution contract](execution_contract.md).
Status: NOT STARTED. Item 1 runs right after 03 and lands, tested, before 04 exposes
enable (04 consumes it); items 2-5 run after 04. Nanotec-specific: these paths move the motor or rewrite
CiA 402 objects outside the state machine, so every motion and settings stage
applies the admission rule of item 1. Review section 6 (last item) and section 12.

## Work

1. Exclusive-control admission, a pure predicate over 03's `ControlPaths`, consumed
   by 04-10. A CiA 402 motion, enable or settings operation is admitted only when every
   row below admits, from fresh observations bound to binding, boot and control-path
   generations. Command objects are 6040h, 6060h, 2291h, 2300h and the setpoint and
   output objects 607Ah, 6081h, 60FFh, 6071h, 6042h, 60C1h and 60FEh (the ledger's PDO
   columns say which are RPDO-mappable). The single-master declaration is one caller
   declaration, bound to binding and boot generation, that nothing but this owner's
   application writes the node's object dictionary or sends its RPDO COB-IDs: no
   other CANopen master, no N5 Ethernet REST (N5 pp35-36), no C5-E/CL4-E USB cfg.txt,
   no CL4-E Modbus RTU on the shared X1 connector (CL4E p19, NT-D28). Requiring it is
   the single-master guard, a settable guard (lead decision 2026-11-19, the owner's
   engineering direction of 2026-11-17): on by default; the application may switch it
   off per binding and then owns that responsibility. With the guard off, the rows
   below that need the declaration admit on their observed conditions alone (Plug &
   Drive: no valid RPDO mapping 2291h and the NanoJ row admitting; RPDOs: no S14 route
   of this owner for that COB-ID; other writers: admitted). The observed rows (NanoJ,
   vendor modes, unread maps) are not part of the guard. Every result that consumed
   the predicate reports the guard setting and the declaration or its absence.

   | Path | Observed (03) | Admits when | Refuses when | Invalidated by |
   | --- | --- | --- | --- | --- |
   | NanoJ | 2300h bit 0 (ON), 2301h bit 0 (RUN) | Both 0. A program that maps 6040h rewrites it every 1 ms (N5 pp154-155) | Either is 1 or unread, or a NanoJ start or stop of this library is unresolved; no declaration overrides | Boot-up (autostart unknown, NT-D32), this library's NanoJ start (before its TX), any 2300h write |
   | Plug & Drive interface | 2290h bit 0; RPDOs mapping 2291h | 2290h = 0; or 2290h = 1 with the single-master declaration (or the guard off), no valid RPDO mapping 2291h and the NanoJ row admitting; or the firmware row's evidence says the interface is absent (FIR-v1650: 2290h, 2291h and 2292h exist from FIR-v1748, N5 pp280-282, and the N5V2 index lists none), decided without a 2290h upload (13) | Otherwise, including an unread or aborted 2290h on a row where it exists. It is on ex works (PDI p5; EDS 2290h = 01h for every member); 2291h:04 runs switch-off, clear error, quick stop, auto setup, homing, PP, PV or PT immediately (PDI p6) and overwrites 6040h, 6060h, 607Ah, 6081h and 60FFh (PDI p14); bit 9 cannot show it (NT-D26) | Boot-up, rebinding, a 2290h write, any map change |
   | RPDOs | Valid bit, transmission type and mapped objects of 1400h-1407h and 1600h-1607h | No valid RPDO maps a command object; or the single-master declaration (or the guard off) and no S14 route of this owner for that COB-ID (the owner sends no RPDO data today) | A valid command map without the declaration while the guard is on, in every NMT state (another master's NMT start makes the node Operational; RPDO1 maps 6040h and 6060h by default, 09 item 3); an unread map. A pending synchronous RPDO is also S13's case (09) | Remap, COB-ID change, boot-up, rebinding |
   | Vendor modes | 6061h; on PD4-C 4015h:01 | 6061h is 0 or a mode 6502h flags; 4015h:01 = 0 | 6061h -1, -2, another value or unread; 4015h:01 = 2: the state machine then follows digital input 1 only and 6040h state requests have no effect (PD4C p33, p327) | Boot-up, a 6060h or 4015h write |
   | Other writers | Not observable | The single-master declaration, or the guard off | No declaration with the guard on | Boot-up, rebinding |

   Handing authority away: `prepareNanojStart`, `preparePdiControl` with 1 and
   `prepareAutoSetup` advance the binding's control-path generation before their first
   TX. Ordinary work admitted under an older generation, queued work included, sends
   no further changing step and ends with its confirmed and uncertain effects. A lost
   reply leaves that path unknown. Only fresh `ControlPaths` read after the change admit
   ordinary control again: after this library's NanoJ start that means an explicit
   NanoJ stop and a read showing both NanoJ bits 0, because RUN = 0 while the program
   loads (up to 200 ms, N5 p283) is not exclusion.
   Auto setup's own steps run under the generation it advanced, by item 4's
   operation-local authorization, never by this predicate.
   Gaining exclusive control: `prepareNanojStop` and `preparePdiControl` with 0 are the
   narrow routes to a passing predicate. They need admitted identity (prompt 57
   "Admission by outcome"; both are vendor-object downloads, never under the stop
   exemption), write preflight
   (canopen_shared 02 "Operation admission") and an explicit request, not the
   predicate itself; they never enable, move or save, and they report the drive state
   the stopped path left, observed afterwards. No route clears 4015h or leaves an
   external-signal mode; those stay refused with the reason.
2. Plug & Drive control: `preparePdiControl` writes 2290h:00 = 0 or 1 in RAM with
   readback (`RetryClass::IDEMPOTENT_SETTING`); only on explicit request, never at
   startup, probe or recovery. 2290h is savable in category application (N5 p112,
   p280); saving belongs to 10. 2290h exists from FIR-v1748 (N5 p280), so on FIR-v1650
   this setter refuses through stage 13's `FIR1650_PLUG_AND_DRIVE` gate. The library never writes 2291h: Plug
   & Drive commands are ledger rows "unsupported by host policy: second command path".
3. NanoJ control: typed reads of 2300h (bit 0 ON, bit 2 AutoYield), 2301h (bit 0 RUN,
   bit 2 ERR) and 2302h (error code) (N5 pp283-284). `prepareNanojStop` writes 2300h
   bit 0 = 0 and observes both NanoJ bits 0 (item 1's narrow route); `prepareNanojStart`
   sets bit 0 with a rising edge,
   which reloads the program and may take up to 200 ms (N5 p283); it is
   `MOTION_LIKE`, never repeated, and refused unless explicitly requested with the
   caller accepting that the program's behavior is unknown to the library. Program
   upload, AutoYield and 2700h user storage are out of scope. Record per model
   whether a present program starts at power-on (N5 p283) or only via cfg.txt on USB
   controllers (FWUPD p6), NT-D32, as recorded in stage 01; while unknown, no result
   assumes either. Object 2300h means NanoJ here and save/restore on Lichuan; the S2
   guards keep the two apart (MULTIDRIVE case 7), and COMMON's owner-absence `2300:00`
   ban is the CL86-C fixture's, not a Nanotec rule (execution contract, "Hardware").
4. Auto setup (6060h = -2; N5 pp39-42, p96): an explicit commissioning operation
   `prepareAutoSetup`, `MOTION_LIKE`, never repeated, with its own state sequence. It
   does not call 04's mode select or enable; their -2 refusal and item 1's -2 row stay
   for every other operation.
   - Admission before the first TX: the explicit request; caller declarations (no
     load, nobody touching the motor, free rotation, N5 p40) and 04 item 2's enable
     side-effect declarations; admitted identity, write preflight and one operation
     deadline (canopen_shared 02 "Operation admission"); item 1's predicate admitting,
     so NanoJ is stopped (2300h bit 0 = 0, N5 p40) and 6061h is neither -1 nor -2; a
     fresh state of Switch on disabled, Ready to switch on or Switched on without fault
     (Operation enabled, Quick stop active and the fault states refuse: an explicit
     release or reset comes first). PD4-C ran auto setup at the factory (4016h:01,
     PD4C p42, p85, p327): refuse unless the caller explicitly requests a re-run.
   - Authority: admission advances the control-path generation before the first TX
     (item 1), retiring older ordinary work; the operation's own steps below are then
     the only changing work admitted on the binding until its terminal. Stops stay
     admitted (04 item 4).
   - Steps (the N5 p42 example, checked against pp40-41 and p96), each confirmed by
     its SDO reply and a fresh read: 6060h = FEh, confirmed by 6061h = FEh; the enable
     words of 04's enable column from the observed state; in a fresh Operation enabled
     with 6061h = FEh and 6040h bit 4 = 0, the trigger 001Fh (bit 4 rising, p41, p96);
     poll 6041h for bit 12 (completed; bit 10 means index found, p41); then 0000h, the
     example's final word (p42), leaving Switch on disabled. Before the trigger TX every
     reference, scale and calibration generation is invalidated, and motion stays
     refused until a boot-up is observed, because the coordinate system is invalid
     until a restart and homing alone does not suffice (p42).
   - Failure after a changing step, never retried: a lost or aborted 6060h reply leaves
     the mode unknown and sends nothing further; a lost enable word leaves the state
     unknown (possibly enabled in -2) and sends nothing further; a lost trigger reply,
     the deadline, a fault or a stop before bit 12 leaves the run possibly started or
     finished (fieldbus service may lag during it, p40). Each result lists confirmed
     and uncertain steps, ends `UNKNOWN` where any is uncertain, and leaves ordinary
     control refused until fresh `ControlPaths` read after an observed boot-up.
   - Persistence: an observed bit 12 means the drive reports completion, and the
     manual states that the results were saved to categories drive (1010h:05) and
     tuning (1010h:06) (p42), which the library does not read back; a trigger that may
     have been sent without an observed completion is a possible persistent change to
     those categories, reported as such, never as a confirmed save.
5. Refused vendor modes: clock-direction 6060h = -1 (N5 pp94-96) and PD4-C special
   drive modes 4015h (PD4C pp326-327, p33) switch command authority to external
   signals. Both are ledger rows with a reason; mode select (04) refuses selecting
   -1, and item 1 refuses ordinary control while either is active or unread.

## Deliver

`profiles/nanotec_canopen/ControlPaths.h/.cpp` (admission check, delivered and
committed first with its tests, then the three operations),
`test/nanotec_canopen_control_paths_test.cpp`, the admission check
called from every existing motion and settings preparation, console routes `nanotec
pdi`, `nanotec nanoj` and `nanotec autosetup` with parity tests, and the coverage rows
for every Plug & Drive command and NanoJ function.

## Acceptance

Item 1's predicate is tested before 04 lands enable: each row admitting, active and
unread; each motion preparation refuses before TX when NanoJ runs or 2300h bit 0 is
set, when the interface is on without a valid declaration, when a valid RPDO maps a
command object without the declaration (default RPDO1 included, in Pre-operational
and Operational), when 6061h is -1 or PD4-C 4015h:01 = 2, after a boot-up or mapping
change, and for a declaration of another binding; on a FIR-v1650 row the Plug & Drive
row admits without any 2290h upload, while an unread or aborted 2290h on a FIR-v2213
row refuses. With the single-master guard off
the same factory configuration is admitted without a declaration, the observed rows
still refuse, and each result reports the guard setting and declaration. Further tests: queued ordinary work
across this library's NanoJ start sends no further step; RUN still 0 after the start
reply stays refused; lost start and stop replies leave the path unknown and refused;
NanoJ stop and Plug & Drive off are admitted while the predicate refuses and never
enable; 2290h writes are confirmed by readback;
NanoJ start is never repeated after a lost reply. Auto setup: refused from each
non-admitted state, with the predicate refusing, and without its declarations; queued
ordinary work retired at admission; ordinary mode select and enable still refuse -2;
the frame sequence of item 4 from each admitted state; a lost 6060h reply (no enable
word sent), a lost enable word (no trigger sent), a lost trigger reply, a timeout, a
fault and a stop before bit 12 (each `UNKNOWN` with a possible persistent change, no
repeat, ordinary control refused until a boot-up); generations invalidated before the
trigger frame; completion (bit 12, then 0000h) carrying the manual's saved
categories, the restart requirement and invalidated generations. No test or example performs auto setup implicitly.
Quick and full verification and CI on the exact final pushed commit pass. Hardware
NOT RUN; auto setup is never run on any bench during the owner's absence.
