# 07 — Homing, coordinate changes and limits

Execute this stage under the [execution contract](execution_contract.md).
Status: NOT STARTED. Requires 03-05. MO-D07 is owned here. Consume prompt 77's protocol-neutral limit facts,
wiring declarations and outcome names where landed; the MOONS' input functions and reactions stay here.

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
   Keep "which switch is approached first" unresolved per method; never infer it from another manual.
4. Inputs: homing works on the logical limit and home values of 0x60FD bits 0-2 (UM21 p23, p63). Read the
   home switch input 0x2001 (X1-X8, default 3, UM21 p65) and the limit definition 0x260B (STF) or 0x220F (SSDC):
   1 closed-active, 2 open-active, 3 not used, 4-20 per HCR (UM21 p76, p84). STF wires X3 as CW limit and X4
   as CCW limit (STFHW p17). A method needing a switch is admitted only with the matching input function read
   and a caller wiring declaration; never disable or reassign limits to make homing pass.
5. Objects and types: 0x6099 sub 1 switch-search and sub 2 zero-search speeds (UINT32 counts/s, UM21 p62);
   0x609A homing acceleration (INT32 on UM21 p22, UINT32 on p55/p62; MO-D07); 0x607C home offset INT32 in
   position units (UM21 p60); 0x2009 homing offset mode (UM21 p66). 0x2008 "Homing inner offset" (EDS only,
   INT16 default 4000) is undocumented: read raw, never write. 0x2202 hard-stop torque limit exists (UM21
   p72-p73) but no hard-stop method number does: hard-stop homing is unsupported.
6. Sequence: mode 6 confirmed through 0x6061; Operation enabled observed; write method, speeds, acceleration,
   offset and offset mode, each confirmed; bit 4 low, then 0x001F to start (UM21 p23); halt with 0x011F stops.
   One immutable deadline derived from caller travel and speed limits, not a copied short poll count. A lost
   start reply keeps `UNKNOWN`; never reissue it.
7. Completion (UM21 p22, bits 13/12/10): 0,0,0 in progress; 0,0,1 interrupted or not started; 0,1,0 attained,
   target not reached; 0,1,1 completed; 1,0,0 and 1,0,1 errors. 0,0,1 is also the idle pattern, and 0,1,1 can
   remain from an earlier homing, so completion requires a post-trigger transition (an observed in-progress
   pattern, or a pattern different from the pre-trigger read). A method that finishes between polls ends
   acknowledged with completion `NOT_OBSERVED`, not completed.
8. Final coordinate: with 0x2009 = 0 the motor stops at machine home and the current position becomes the home
   offset; with 0x2009 = 1 it moves by the home offset and that point becomes zero (UM21 p66). Publish
   `getHomeReference` only after qualified completion, with the documented expected 0x6064 (0x607C or 0) as an
   expectation to be checked by a fresh read, never set from statusword bits alone.
9. Methods 35 and 37 change coordinates without motion: 35 takes the current position as home and needs no
   Operation enabled; 37 sets 0x6064 = 0x607C (UM21 p30). AMP10 p44 says the 2010 method 35 ignores the home
   offset (stage 13). Both invalidate host origin and reference generations and need explicit stationary
   admission as host policy.
10. Zero position 0x200C (write 1 zeroes "all position parameters such as" 0x6064, UM21 p67) is a separate
    explicit vendor operation with the same invalidation. Which other objects it zeroes is not listed; record
    them as unknown and re-read 0x6064, 0x607A and 0x607C afterwards.
11. Limits: no 0x607D software limits exist; report 0x607F and, on SSDC, 0x2209 and 0x220A (UM21 p74). 0x603F
    bits 0-2 and 0x60FD bits 0-2 report limits (UM21 p56, p63). The drive's reaction on reaching a limit is not
    documented; report observations and claim no stop behavior. Host soft limits follow prompt 77.

## Deliver

Profile `Homing.h/.cpp` with `HomeContext`, the method table and admission rules, `prepareZeroPosition`;
`test/moons_canopen_homing_test.cpp`; console `home` and zero-position routes with parity tests; ledger and
coverage rows per method; Doxygen; report.

## Acceptance

Tests cover every method's admission per product, refused 15, 16, 31, 32, 36, 128 and negatives, index
refusal on STF, missing input function, stale completion patterns, fast completion between polls, timeout
after start, halt interruption, both offset modes, methods 35 and 37 invalidation, 0x200C and no reissue.
Direction and limit reactions stay open with MO-D07. Quick and full verify, firmware builds and exact-commit CI
pass. Physical homing needs its switch fixture. Hardware: NOT RUN.
