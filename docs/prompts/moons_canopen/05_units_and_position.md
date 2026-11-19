# 05 — Units and finite profile position

Execute this stage under the [execution contract](execution_contract.md).
Status: NOT STARTED. Requires 03-04. MO-D03, MO-D20 and MO-D31 are owned here; MO-D09 applies to 0x607E.

## Work

1. Native units (UM21 layout). Record per object, from the pages: 0x607A target position INT32 in "position
   units" (UM21 p60; examples say steps, p88); 0x6081, 0x607F, 0x606C in counts/s (UM21 p59, p61); 0x6083,
   0x6084, 0x6085 in counts/s² (UM21 p61-p62); 0x6065 following-error window in encoder counts (UM21 p59);
   0x60F4 in "user-defined position units" (UM21 p63). The examples relabel the same values step/s and step/s²
   (UM21 p88-p89; MO-D03). No factor group, gear ratio or feed constant exists. Keep the native unit an open
   fact per product until stage 14a or vendor evidence resolves it.
2. Resolution reads: STF 0x2604 steps per revolution (UINT16 RW, UM21 p82; 200-51200 by HCR p85 EG); SSDC
   0x2207 steps per revolution and 0x2218 encoder resolution (RO, UM21 p72, p74, p79), encoders of 20000 or
   4096 counts/rev (SSDCHW p3, p36). For SSDC, whether "counts" mean encoder counts per 0x2218 or gearing steps
   per 0x2207 is not stated (MO-D03). A resolution read is a typed fact of this generation, not a scale.
3. Common-unit conversion goes through `UnitConfig`, `preparePosition` and `prepareVelocityTarget` with a
   caller-supplied, generation-bound scale qualification that names the native unit (for example "STF-C
   position unit = 1 step at 0x2604 = 20000"), its evidence and the resolution value it was bound to. A
   0x2604, 0x2207, 0x2218, direction or polarity change invalidates it. Without it only the native-unit route
   runs. If the core lacks a native acceleration-rate basis (counts/s², not a ramp time), add it as a reviewed
   core change with tests; a ramp time never becomes an acceleration and the profile takes no shortcut.
4. Widths and quantization: check the prepared native values with `checkNativeInteger` against the reviewed
   types (0x607A INT32; 0x6081, 0x6083, 0x6084 UINT32 per UM21 p17 and the EDS). Report quantization exactly.
   Zero profile velocity or acceleration rejects before TX as host policy (no documented minimum). A target
   velocity above a fresh 0x607F read rejects before TX; whether the drive would clamp is unknown.
5. Feedback meaning: EDS-STF and EDS-SSDC name 0x6064 "Position value calculated"; UM21 offers no encoder
   position object and does not say what 0x6064 is (MO-D20). STF-C is open loop, so 0x6064 cannot be measured.
   Publish 0x6064 with `measured=false` for both products; endpoint checks compare drive-reported values only.
   A measured-position claim on SSDC needs a caller-supplied qualification bound to 14a evidence.
6. Set-point protocol (UM21 p17-p21): controlword bit 4 new set-point (rising edge), bit 5 change set
   immediately, bit 6 relative, bit 9 change on set-point; statusword bit 12 set-point acknowledge, bit 10
   target reached, bit 13 following error. UM21 contradicts itself on bit 12 (MO-D31): p18 says 1 means the
   previous set-point is in process and overwriting is accepted, p21 says 1 means the two-deep buffer is full
   and a new set-point is ignored without error. Until resolved, admit a new set-point only with bit 12 = 0
   observed in a fresh read, and treat acceptance as unobserved unless a post-trigger transition is seen.
7. First subset, `MoveSetup` write-all: one finite move from an idle, enabled drive in mode 1 (0x6061 = 1, no
   fault, bit 12 = 0). Write 0x6081, 0x6083, 0x6084 and 0x607A, each confirmed; write the start word with bit 4
   low (0x000F absolute or 0x004F relative) unless the last confirmed word already had it low; then 0x001F or
   0x005F (UM21 p19); then clear bit 4. Bit 5 and bit 9 stay 0 in this subset. Every write has one immutable
   deadline; a lost reply keeps `UNKNOWN` and is never replayed.
8. Completion: native completion needs post-trigger evidence: an observed acknowledge (bit 12 high after the
   trigger) or bit 10 falling after the trigger, then bit 10 high with bit 12 low. A bit 10 already high before
   the trigger never manufactures completion. Generic target arrival additionally compares 0x6064 with the
   prepared target and stays "drive-reported" under item 5. A requested guarantee whose evidence cannot be
   obtained rejects before TX.
9. Relative moves: UM21 does not state the basis of a relative target (previous target or actual position). A
   common relative move needs a caller `relativeBasisVerified` qualification for the named basis, otherwise
   it returns the unresolved-basis error before TX; the native bit-6 path stays explicit and acknowledged-only.
10. Interruption: halt pauses and resumes the set-point on release (stage 04 item 8); quick stop ends disabled;
    both are explicit with retained results. Replacement during motion with bit 5 = 1 (UM21 p19 single
    set-point) or buffered set-points with bit 9 waits for MO-D31 and prompt 52's change contract.
11. Polarity 0x607E (RO on UM21 p61, RW on p55, rww in the EDS; MO-D09) and the direction settings are read
    only here; a change belongs to stage 08c and invalidates scales and origins.

## Deliver

Profile `Position.h/.cpp` with `MoveContext` and the native move record; any reviewed core unit addition with
its tests; `test/moons_canopen_position_test.cpp`; console `prepare`, `moveto`, `moveby`, `move` routes for
MOONS' bindings with API/CLI parity; units section in `docs/moons_canopen_coverage.md`; Doxygen; report.

## Acceptance

Tests cover unit and quantization boundaries, INT32 and UINT32 limits, missing or stale scale qualification,
resolution change invalidation, bit 12 high at admission, stale bit 10, lost trigger reply, partial setup,
deadline after each write, halt and quick stop during the move, relative basis refusal and no replay. Native
units, the bit 12 meaning and measured feedback stay open with their MO-D IDs. Quick and full verify, firmware
builds and exact-commit CI pass. Hardware: NOT RUN.
