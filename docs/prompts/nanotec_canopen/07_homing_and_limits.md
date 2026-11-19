# 07 — Homing, limit switches and software limits

Execute this stage under the [execution contract](execution_contract.md).
Status: NOT STARTED. Runs after 05. NT-D11 and NT-D18 apply. Use [prompt 77](../77_switch_ready_homing_limits.md)
for the protocol-neutral limit admission and outcome names; the input functions,
polarity, methods and reactions below are Nanotec facts.

## Work

1. Method table from N5 pp82-89 (rendered figures, not extracted prose), applied to
   C5E pp80-87, CL4E pp80-87 and PD4C pp71-78 after their pages are compared: methods
   1-14 with index, 17-30 without index (the text also says "17 to 32", NT-D11), 33
   and 34 to the next index, 35 at the current position, and the negative
   homing-on-block methods -1, -2, -7..-14, -17, -18, -23..-30 (N5 p84). For each,
   record direction, required switches or index, block detection, and the final
   coordinate. Methods 23-30 say "home switch" where the limit switch is meant and
   method 26 mentions an index pulse (N5 p87, NT-D11): use the figures and record the
   text error. Negative methods mean something else for other vendors (CP-D14).
2. Prerequisites per method: home and limit inputs enabled in 3240h, and 3701h = -1 so
   that a limit does not block the homing travel (N5 p82); an index needs an encoder
   with index and pulses of at least 31.25 µs (N5 p387); block methods need active
   closed loop, established as for PT (06 item 6: a fresh, same-session bit 15 = 1 in
   Operation enabled, not the configuration alone; refused before the trigger without
   it), and 203Ah:01 (mA, or a percentage of 2031h if negative) and 203Ah:02 in ms (N5 p84,
   p268). Speeds 6099h:01 (preset 50) and 6099h:02 (preset 10) with :02 lower than :01;
   609Ah is used only when starting (N5 pp386-387); home offset 607Ch. Refuse a method
   whose prerequisites are not declared and observed; never disable a limit or
   reassign an input to make homing run.
3. Method 35 (the preset, 6098h = 23h, N5 p385) sets the current position without
   motion and does not need Operation enabled (N5 p89). Treat it as an explicit
   coordinate operation, not as physical homing, and never run it because the preset
   selects it (NT-D11). It invalidates host origin and reference generations.
4. `prepareHome`/`nextHome`/`advanceHome` (6060h = 6, bit 4 rising; N5 p82). Status
   bits 13/12/10: 000 running, 001 interrupted or not started, 010 homed since restart
   but target not reached, 011 complete, 100 error while turning, 101 error at
   standstill. Bit 12 stays set after the first completion since restart and is reset
   during a later homing (N5 p83), so completion needs a post-trigger observation that
   left the pre-trigger pattern and then shows 011. A fast method that completes
   between samples ends ACKNOWLEDGED with unobserved completion, never COMPLETE.
   Clearing bit 4 interrupts homing. Errors keep the motion state they report.
5. Reference publication: 6062h and 6064h are zeroed by homing and 6063h is not
   (N5 p360); 3243h captures the home-switch position (N5 p321). `getHomeReference`
   publishes only after observed completion of the same trigger generation. Auto setup
   (04b) and a restart invalidate any home reference.
6. Limit switches (N5 pp66-67, pp97-101): inputs 1-3 can be negative limit, positive
   limit and home once enabled in 3240h:01; :02 inverts, :03/:04 force values, :06
   selects the level, :08 enables routing via 3242h. On a limit the position is
   stored, statusword bit 7 is set and 3701h acts: -2 no reaction and discard, -1
   (factory) no reaction, 0 coast to Switch on disabled, 1 and 2 ramp to Switch on
   disabled, 5 and 6 ramp to Quick stop active energised. Travel past the stored
   position is blocked unless 3701h is -1 or -2. Report the configured reaction; never
   claim a limit stops the drive with the factory value. Discarding stored positions
   (3701h = -2 briefly) is an explicit operation, needed also after both limits fired.
   An unconnected input does not prove its function is disabled.
7. Interlock: with 3240h:01 bit 3, 60FDh bit 3 = 0 raises a fault with the 605Eh
   reaction, error 5440h (N5 p101, p164). Software limits are as in 05 item 7.

## Deliver

`profiles/nanotec_canopen/Homing.h/.cpp` with the method table, `HomePrerequisites`,
`HomeContext`, method-35 coordinate operation and limit reaction reads and the
discard operation; `test/nanotec_canopen_homing_test.cpp`; console routes `nanotec home`
and `nanotec limits` with parity tests; prompt 77 rows for this profile.

## Acceptance

Tests cover every method's admission row, refused methods without declared switches
or index, block methods refused in open loop and on a configured closed-loop axis
without a fresh bit 15 = 1, speed ordering, a stale bit 12 from an
earlier homing, interruption by clearing bit 4, both error patterns, stop during
homing, method 35 never run implicitly, reference publication only after observed
completion, and each 3701h value's reported reaction. Quick and full verification and
CI on the exact final pushed commit pass. Hardware NOT RUN; physical homing needs the
actual switches or index fixture.
