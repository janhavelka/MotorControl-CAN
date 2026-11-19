# 07 — Homing and limits

Execute this stage under the [execution contract](execution_contract.md).
Status: NOT STARTED. Runs after 04 and 05. DS-D15 and DS-D18 belong here; DS-D08 and DS-D16 apply to `6098`, `6099`
and `609A`. MULTIDRIVE rows 32, 36 and 77 apply. Review the rendered method figures (DS2C p34-45), not extracted
prose.

## Work

1. Method table. From DS2C p13 and the appendix p34-45 build one row per documented method, 1..14, 17..30 and 33..35:
   start direction, required signals (negative or positive limit, home switch, Z pulse), edge and side, departure and
   the speed used in each segment (`6099:01` search, `6099:02` after the home is found; DS2C p28). Methods 15, 16, 31
   and 32 reject before TX; IBL60 p30's range 1..14, 17..35 (DS-D18) does not make 31/32 documented methods. Compare
   the IBL and ISL appendices with DS2C and record any difference per model.
2. Method 35 (DS-D18). DS2C p45 sets the current position as home after controlword 0x0F to 0x1F, with behavior chosen
   by `60E6` and an offset in "`606C`". `60E6` is absent from every table and EDS and `606C` is actual velocity, so
   method 35 is UNSUPPORTED until a source defines them. Never write `60E6` or use `606C` as an offset.
3. Sequence (DS2C p13; IBL60 p14). Select mode 6 and enable through 04, then write `6098`, `6099:01`, `6099:02`,
   `609A` (and `607C` when requested), then raise bit 4 (word 0x001F from a confirmed 0x000F); 1 to 0 interrupts.
   The manual writes the parameters after enabling; keep that order unless a source permits another.
4. Parameters. Always write the requested `6098` and read it back: defaults 17 (DS2C p28) and 0 (DS2EDS), I08 versus
   U8 (DS-D08); positive methods have identical bytes. Speeds are pul/s and `609A` pul/s² in the tables, while DS2C
   p13 step 3 calls `609A` an accel/decel time, DS2EDS gives 120/60 and 100, and DS2OP p7 gives 0..3000 and 0..2000
   (DS-D16). Use 05's rate and ramp declarations; without them only native values are written, labelled "unit
   unresolved". `607C` home offset is I32 pul, -1000000..+1000000 in the mode table (DS2C p13).
5. Completion. `DS2C_TABLE`: bit 8 "homing complete" must be low after the trigger and then rise, with bit 10 speed
   zero as supporting evidence. `IBL60_TABLE`: bit 10 in mode 6 means homing complete (IBL60 p14), same rule. No
   homing-error bit and no homing timeout are documented, so the caller supplies a bounded observation budget and a
   timeout ends the operation with a stop obligation, never a re-trigger. The position value after homing (CiA: the
   home offset) is not stated by DS2 sources: publish a host reference only after completion evidence and a fresh
   `6064` read, and invalidate the previous reference generation when the trigger is sent.
6. Signals. `60FD` bits 0, 1 and 2 report negative limit, positive limit and home (DS2C p28) as live observations.
   Assignments are P-parameters (factory DI4 = P-OT 14, DI5 = N-OT 15, DI6 = home switch 31, logic 0; DS2C p9) and
   unreachable, so the caller declares wiring and assignments per binding; a method needing a signal the declaration
   lacks is refused. An unconnected terminal still has its function. Z-pulse methods (1..14, 33, 34) need a caller
   declaration that the encoder provides an index; the magnetic encoders' index is undocumented.
7. Software limits (DS-D15). `607D` prints both rows as sub-index 01, positive first, with defaults 2147483647 and
   2147483648 (DS2C p28); DS2EDS has sub 1 MIN and sub 2 MAX. Read both raw; write neither until a fixture read of
   unchanged defaults or a source settles the order. Activation is unstated; Er.D09 and Er.D10 (DS2C p32) name limit
   faults. Hardware limits FunIN.14/15 block one direction (DS2C p9); Er.950/Er.952 are warnings (DS2C p33).
8. Shared vocabulary. Once [prompt 77](../77_switch_ready_homing_limits.md) lands, express input facts, limit admission
   and outcome names through its protocol-neutral types; DS2 keeps its own reaction facts and inherits none.
9. References across power cycles. With an absolute encoder (R17, R23) the drive may keep a multi-turn position, but
   no source says `6064` or the home reference survives power-up (08). A boot-up, NMT reset or binding change
   invalidates the host reference; re-establish it by fresh evidence, never by a replayed homing start.
10. Stops and interruption. A stop during homing follows 04; the result keeps the stop's own evidence, marks the
   reference invalid and never resumes the search. Bit 4 falling (word 0x000F, DS2C p13 "interrupt homing") is a
   separate native interrupt: offer it as an acknowledged-only homing interrupt, never as a stop, because its effect
   on motion is undocumented.

## Deliver

`Homing.h`/`Homing.cpp` with `HomeContext`, the method table and `getHomeReference`; limit reads in `Reads`;
`test/lichuan_ds2_homing_test.cpp` and `test/lichuan_ds2_limits_test.cpp`; owner-backed `ds2 home` and `ds2 limits`;
coverage, operation map, `docs/homing.md` DS2 section, Doxygen and CHANGELOG.

## Acceptance

Tests cover every method's admission against declared signals, rejected 15/16/31/32/35, parameter order after enable,
`6098` readback mismatch, a stale or already-high completion bit for both dialects, timeout with a stop obligation and
no re-trigger, interruption by bit 4 falling and by an explicit stop, reference publication only after fresh evidence,
and `607D` writes refused. Quick and full verification and required CI on the exact final pushed commit pass.
Physical homing needs a DS2 fixture with real switches or a qualified index; hardware NOT TESTED. Commit, push and
hand off.
