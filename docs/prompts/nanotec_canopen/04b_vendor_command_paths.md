# 04b — Vendor command paths: Plug & Drive interface, NanoJ and auto setup

Execute this stage under the [execution contract](execution_contract.md).
Status: NOT STARTED. Runs after 03 and 04. Nanotec-specific: these paths move the motor or rewrite
CiA 402 objects outside the state machine, so every motion and settings stage
applies the admission rule of item 1. Review section 6 (last item) and section 12.

## Work

1. Exclusive-control admission, consumed by 04-10. A CiA 402 motion, enable or
   settings operation is admitted only with fresh, generation-bound `ControlPaths`
   from 03 showing all of:
   - NanoJ not running: 2301h bit 0 (RUN) = 0. A program that maps 6040h rewrites it
     every 1 ms and the fieldbus loses control (N5 pp154-155). No declaration
     overrides a running program.
   - Plug & Drive interface off (2290h bit 0 = 0), or a caller declaration, bound to
     binding and boot generation, that nothing writes 2291h: no other master, no RPDO
     mapping 2291h (checked from the 03 mapping readback) and no NanoJ program. The
     interface is on ex works (PDI p5; EDS 2290h = 01h for every member), and a write
     of 2291h:04 executes switch-off, clear error, quick stop, auto setup, homing, PP,
     PV or PT immediately (PDI p6) and overwrites 6040h, 6060h, 607Ah, 6081h and
     60FFh (PDI p14). Statusword bit 9 cannot show this (NT-D26).
   - A caller declaration that no other path writes the object dictionary: N5
     Ethernet REST (N5 pp35-36), C5-E/CL4-E USB with cfg.txt, CL4-E Modbus RTU on the
     shared X1 connector (CL4E p19, NT-D28). The library cannot observe these.
   Boot-up, rebinding and any change to the mapping invalidate the declaration.
2. Plug & Drive control: `preparePdiControl` writes 2290h:00 = 0 or 1 in RAM with
   readback (`RetryClass::IDEMPOTENT_SETTING`); only on explicit request, never at
   startup, probe or recovery. 2290h is savable in category application (N5 p112,
   p280); saving belongs to 10. 2290h exists from FIR-v1748 (N5 p280), which is one
   more reason the FIR-v1650 row stays refused. The library never writes 2291h: Plug
   & Drive commands are ledger rows "unsupported by host policy: second command path".
3. NanoJ control: typed reads of 2300h (bit 0 ON, bit 2 AutoYield), 2301h (bit 0 RUN,
   bit 2 ERR) and 2302h (error code) (N5 pp283-284). `prepareNanojStop` writes 2300h
   bit 0 = 0 and observes RUN = 0; `prepareNanojStart` sets bit 0 with a rising edge,
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
   `prepareAutoSetup`, `MOTION_LIKE`. Prerequisites declared by the caller: no load,
   nobody touching the motor, free rotation; observed: NanoJ stopped (2300h bit 0 =
   0, N5 p40), admitted identity and exclusive control. Sequence per N5 p42 (an
   example, checked against p41): mode -2, enable, rising bit 4, wait for statusword
   bit 12 (done; bit 10 means index found), then bit 4 = 0. Consequences the result
   must carry: results were saved automatically to categories drive (1010h:05) and
   tuning (1010h:06), a persistent change; the coordinate system is invalid until a
   restart, and homing alone does not suffice (N5 p42); fieldbus service may lag
   (N5 p40), so a timeout leaves `UNKNOWN` and is never retried. Afterwards every
   reference, scale and calibration generation is invalid and motion is refused
   until a boot-up is observed. PD4-C ran auto setup at the factory (4016h:01,
   PD4C p42, p85, p327): refuse unless the caller explicitly requests a re-run.
5. Refused vendor modes: clock-direction 6060h = -1 (N5 pp94-96) and PD4-C special
   drive modes 4015h (PD4C p326) switch command authority to external signals. Both
   are ledger rows with a reason; mode select (04) refuses them.

## Deliver

`profiles/nanotec_canopen/ControlPaths.h/.cpp` (admission check and the three
operations), `test/nanotec_canopen_control_paths_test.cpp`, the admission check
called from every existing motion and settings preparation, console routes `nanotec
pdi`, `nanotec nanoj` and `nanotec autosetup` with parity tests, and the coverage rows
for every Plug & Drive command and NanoJ function.

## Acceptance

Tests show that each motion preparation refuses before TX when NanoJ runs, when the
interface is on without a valid declaration, after a boot-up or mapping change, and
for a declaration of another binding; that 2290h writes are confirmed by readback;
that NanoJ start is never repeated after a lost reply; and that auto setup's result
carries the saved categories, the restart requirement and invalidated generations,
with a timeout ending `UNKNOWN`. No test or example performs auto setup implicitly.
Quick and full verification and CI on the exact final pushed commit pass. Hardware
NOT RUN; auto setup is never run on any bench during the owner's absence.
