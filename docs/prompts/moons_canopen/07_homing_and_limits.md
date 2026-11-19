# 07 — Homing, coordinate changes and limits

Execute this stage under the [execution contract](execution_contract.md).
Status: NOT STARTED. Requires 03-05. MO-D07 is owned here. Consume prompt 77's protocol-neutral limit facts,
wiring declarations and outcome names where landed; the MOONS' input functions and reactions stay here. The
stage is self-contained: `HomeContext` writes its own homing parameters as confirmed steps (as stage 05's move
does) and reads the input functions through stage 03's config read; it needs no stage 08 settings context.

Review the rendered method figures of UM21 p23-p30 and the CiA reference in the principles pack s15 before
writing the table. Never borrow another profile's method numbering, index or limit semantics.

## Work

1. Method table (UM21 p22-p30): methods 1-14, 17-30, 33, 34, 35 and 37 are described; 15 and 16 are reserved
   (p27), 36 is reserved (p30), 31 and 32 are not described in UM21 and are reserved in AMP10 p42. 0x6098 is
   INT8, yet UM21 p62 lists "128 Manufacturer specific" and "1…37 Methods 1 to 35" (MO-D07). Admit only the
   described methods; 128 and negative values reject before TX. For each method record direction, switch,
   index requirement, edge and figure page.
2. Index methods (1-14, 33, 34) need an encoder index pulse. STF-C is open loop with no encoder (STFHW p3,
   p23): refuse them for STF-C. For SSDC-C an encoder exists (SSDCHW p3), but an accessible index is not stated
   for either motor encoder (20000 or 4096 counts/rev, SSDCHW p36); require a caller declaration bound to
   fixture evidence before admitting an index method.
3. Direction: UM21 p23 starts method 1 "leftward" with the CCW limit and p30 calls 33 negative; AMP10 p28 and
   p43 say method 1 homes to the first index CCW after the CW limit and 33 to the next index CW. The relation of
   CW/CCW, negative/positive, 0x607E and the direction settings 0x2208/0x260F sub 1 is not stated (MO-D07).
   Keep "which switch is approached first" unresolved per method; never infer it from another manual. A moving
   method is therefore admitted only with a caller-supplied direction and approach qualification bound to the
   binding, its configuration generation and fixture evidence (14a Q10): which terminal the method approaches
   first and in which native direction, under the 0x607E, 0x2208/0x260F sub 1 and input-function values read
   with it. A change of any of these, rebinding or boot-up invalidates it. Moving-method admission is one rule
   with two parts. The direction check is this qualification alone: until prompt 77's direction guard lands,
   a moving method without a current qualification refuses before TX; once it has landed, the check is that
   guard (on by default, settable per axis; with it off an admitted result names the guard and states that no
   direction qualification was held), and no second guard is built. The prerequisites stay in force whatever
   the guard's setting: item 1's method table, item 2's index declaration, item 4's input function and wiring
   declaration, and item 6's stop and travel plan. 14a Q10 never uses the guard-off path as first evidence.
4. Inputs: homing works on the logical limit and home values of 0x60FD bits 0-2 (UM21 p23, p63). Read the
   home switch input 0x2001 (X1-X8, default 3, UM21 p65) and the limit definition 0x260B (STF) or 0x220F (SSDC):
   1 closed-active, 2 open-active, 3 not used, 4-20 per HCR (UM21 p76, p84). STF wires X3 as CW limit and X4
   as CCW limit (STFHW p17). A method needing a switch is admitted only with the matching input function read
   and a caller wiring declaration, plus item 3's direction check; knowing which terminal is wired does not
   show that the method searches toward it. Never disable or reassign limits to make homing pass.
5. Objects and types: 0x6099 sub 1 switch-search and sub 2 zero-search speeds (UINT32 counts/s, UM21 p62);
   0x609A homing acceleration (INT32 on UM21 p22, UINT32 on p55/p62; MO-D07); 0x607C home offset INT32 in
   position units (UM21 p60); 0x2009 homing offset mode (UM21 p66). 0x2008 "Homing inner offset" (EDS only,
   INT16 default 4000) is undocumented: read raw, never write. 0x2202 hard-stop torque limit exists (UM21
   p72-p73) but no hard-stop method number does: hard-stop homing is unsupported.
6. Sequence for moving methods: mode 6 confirmed through 0x6061; Operation enabled observed; write method,
   speeds, acceleration, offset and offset mode, each confirmed; bit 4 low, then 0x001F to start (UM21 p23);
   the stop is stage 04 item 9's halt: 0x011F after an acknowledged start (UM21 p23), 0x010F after a lost start
   reply, which never raises bit 4 (stage 04 item 8). One absolute operation deadline (canopen_shared 02
   "Operation admission") derived from caller travel and speed limits, not a copied short poll count. The
   drive's limit reaction is undocumented (item 11) and a host deadline bounds nothing physical after
   communication loss. Homing is a host-ended run under canopen_shared 02 "Timed runs" (its homing sentence),
   and the request's stop and travel plan is that bullet's inputs: a caller-supplied latest interruption time
   in place of the duration, which with the caller's stop budget must fit before the operation deadline or
   preparation refuses; an optional travel bound that interrupts earlier, from 0x6064 observations no older
   than a caller-supplied age; every later transfer capped at the interruption time, after which the run
   issues no further work and keeps `needsStop` for the application's urgent stop; and the bullet's stop path.
   A MOONS' binding has no S3 shape and its route stays unresolved after a lost start reply (`SdoReuse`
   `NONE`, stage 04 item 10), so preparation refuses the run before its first changing TX (mode select
   included) unless the application declares an independent stop for it, named in every result (as stage 06
   item 4). A lost start reply keeps `UNKNOWN` and `needsStop`; never reissue it.
7. Completion (UM21 p22, bits 13/12/10): 0,0,0 in progress; 0,0,1 interrupted or not started; 0,1,0 attained,
   target not reached; 0,1,1 completed; 1,0,0 and 1,0,1 errors. 0,0,1 is also the idle pattern, and 0,1,1 can
   remain from an earlier homing, so completion requires a post-trigger transition (an observed in-progress
   pattern, or a pattern different from the pre-trigger read). A method that finishes between polls ends
   acknowledged with completion `NOT_OBSERVED`, not completed.
8. Final coordinate: with 0x2009 = 0 the motor stops at machine home and the current position becomes the home
   offset; with 0x2009 = 1 it moves by the home offset and that point becomes zero (UM21 p66). Publish
   `getHomeReference` only after qualified completion, with the documented expected 0x6064 (0x607C or 0) as an
   expectation to be checked by a fresh read, never set from statusword bits alone.
9. Methods 35 and 37 are coordinate methods, separate from moving homing, with their own admission table:
   35 takes the current position as home, 37 takes the position sensor value as home with 0x6064 = 0x607C
   afterwards, and UM21 p30 says neither requires Operation enabled. Item 6's sequence does not apply: whether
   a non-enabled drive accepts the bit 4 trigger, which words it needs and whether 35 applies 0x607C are not
   stated on the rendered p22-p30 pages; define each from them, otherwise keep the method refused with the gap
   recorded for 14a Q10. AMP10 p44 says the 2010 method 35 ignores the home offset (stage 13). Both retire
   host origin and reference generations (item 12) and need item 10's stationary admission as host policy.
10. Zero position 0x200C (write 1 zeroes "all position parameters such as" 0x6064, UM21 p67) is a separate
    explicit vendor operation with item 12's retirement. Which other objects it zeroes is not listed; record
    them as unknown and re-read 0x6064, 0x607A and 0x607C afterwards. Stationary admission, from fresh 0x6041,
    0x6061 and 0x200B reads within the operation (0x200B bits 4 "Moving", 6 "Stopping", 10 "Homing" and 14 "Q
    Program is running", UM21 p67, are the drive's indications, not a measured standstill):

    | Predecessor | Result |
    | --- | --- |
    | Switch on disabled, Ready to switch on or Switched on, the four 0x200B bits clear | Admitted: no drive function is enabled, and the next enable's transition 4 clears any pending set-point (UM21 p16) |
    | Operation enabled, the four bits clear, stage 04's controlword record known with no set-point that may be pending | Admitted |
    | Operation enabled with a set-point possibly pending (halted, quick-stopped or `UNKNOWN`) or the record unknown | Refused before TX: a later halt release would resume a target expressed in the old coordinates (UM21 p21) |
    | Any of the four bits set, or a move, run, homing or Q operation of this binding in flight or with `UNKNOWN` execution | Refused before TX |
    | Quick stop active, Fault reaction active, Fault, Not ready to switch on, or a read without a reply | Refused before TX: a stopping or fault function may still act (UM21 p16 transitions 11, 13, 14) |

    Item 12's retirement still precedes the possible TX, and a replacement reference also needs a fresh 0x200B
    read with the four bits clear, taken after the 0x6064, 0x607A and 0x607C reads.
11. Limits: no 0x607D software limits exist; report 0x607F and, on SSDC, 0x2209 and 0x220A (UM21 p74). 0x603F
    bits 0-2 and 0x60FD bits 0-2 report limits (UM21 p56, p63). The drive's reaction on reaching a limit is not
    documented; report observations and claim no stop behavior. Host soft limits follow prompt 77.
12. Coordinate retirement: a moving-homing trigger, a 35 or 37 trigger and a 0x200C write can rebase the
    drive's position from the moment their frame may have left. Before that frame is handed to the owner,
    retire the binding's host origin and reference generations, every prepared or queued position operation
    built on them and the host soft limits bound to them (prompt 77). A lost reply, timeout, cancellation, halt
    or unobserved completion never restores them, and a later preparation against a retired reference
    refuses before TX. A replacement comes only from item 8's qualified completion with a fresh 0x6064 read,
    or, for 35, 37 and 0x200C, from an acknowledged operation followed by fresh 0x6064, 0x607A and 0x607C
    reads and item 10's stationary 0x200B read; an `UNKNOWN` outcome leaves the axis without a reference
    until a later qualified home.

## Deliver

Profile `Homing.h/.cpp` with `HomeContext`, the method table and admission rules, `prepareZeroPosition`;
`test/moons_canopen_homing_test.cpp`; console `home` and zero-position routes with parity tests; ledger and
coverage rows per method; Doxygen; report.

## Acceptance

Tests cover every method's admission per product, refused 15, 16, 31, 32, 36, 128 and negatives, index
refusal on STF, an unknown or stale direction qualification (after a 0x607E,
direction or input-function change) with the direction check on or before prompt 77's guard lands (no TX) and
with the guard off (admitted, the guard named), a missing input function, wiring declaration, index
declaration or stop and travel plan refused also with the guard off, item 6's host-ended run (an
interruption time that no longer fits with the stop budget refused at preparation, the travel bound
interrupting earlier and a stale position observation not counted, no transfer after the interruption time,
`needsStop` kept after a lost start reply, and a run without a declared independent stop refused with zero
changing TX), stale completion patterns, fast
completion between polls, timeout after start, halt interruption, both offset modes, methods 35 and 37
through their own table (refused where the page leaves the sequence open), item 12's retirement (references
held before a lost trigger reply, an interrupted or unobserved home and a possibly executed 0x200C write are
unusable: a later position preparation against them sends nothing), item 10's stationary table for 0x200C
(running, halted with a pending target, an unknown record, each 0x200B bit set and an operation in flight
refused with no TX; a replacement reference withheld when the later 0x200B read shows motion), 0x200C and no
reissue.
Direction and limit reactions stay open with MO-D07. Quick and full verify, firmware builds and exact-commit CI
pass. Physical homing needs its switch fixture. Hardware: NOT RUN.
