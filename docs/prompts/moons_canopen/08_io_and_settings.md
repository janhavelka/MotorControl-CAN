# 08 — I/O and drive settings (blocks 08a..08c)

Execute this stage under the [execution contract](execution_contract.md).
Status: NOT STARTED. Requires 03; 08b also needs 07's limit and home-input rules. MO-D21, MO-D24 and MO-D25
are owned here; MO-D08 and MO-D09 apply to every object below.

## Work

1. One settings context for the family: `prepareDriverRead`, `prepareDriverSettings`, `nextDriver`,
   `advanceDriver`, `getDriver` and `getIo` (CONVENTIONS section 3) with prior read, whole-candidate
   validation, confirmed writes, readback and partial-failure evidence, delivered and tested with the first
   block. Stages 10 and 11 reuse it; stage 07 does not (its homing parameters are steps of `HomeContext`).
2. Groups follow the product: stepper objects 0x2601-0x2617 for STF-C (UM21 p82-p87) and step-servo objects
   0x2201-0x2252 for SSDC-C (UM21 p72-p81). A STF object on an SSDC binding, or the reverse, rejects before TX.
   Where UM21's index table and its detail table disagree (for example 0x2601 UINT8 in the p82 list, UINT16 in
   the p82 detail), add the conflict to the ledger and accept only the documented candidates.
3. Defaults: UM21 and the EDS disagree on many defaults (MO-D24: 0x2617/0x220D 3 versus 0, 0x2616 3 versus 0,
   0x260C sub 1 60 versus 0). Never write a default, never treat one as the drive's value, and never "restore
   defaults" by writing EDS values.
4. Settings change behavior, not only parameters: no setting is written as an incidental step of another
   operation, nothing is saved automatically (saving is stage 11), and each changing write requires the
   explicit quiescence policy of the block plus an idle drive (no set-point in process, no Q program running
   per 0x200B bit 14).
5. Invalidation: a change of 0x2604, 0x2208, 0x260F sub 1, 0x607E, 0x260D or 0x2612 invalidates scales,
   origins and dependent qualifications; a change of an input function invalidates limit and homing
   admission; a change of an output function invalidates output ownership.
6. Keep three I/O facts apart (MULTIDRIVE rows 21, 62 and 69): the physical wiring the application declares,
   the function the drive is configured with, and the logical level the drive reports in 0x60FD. An open
   terminal does not mean its configured function is disabled. Require a physical signal only for the
   operation that needs it, and never silently disable a limit, enable or alarm-reset input to make another
   test run. MCU pins stay application-owned.
7. Configuration written over RS-232 by Stepper Suite, STF Configurator or Quick Tuner (UM21 p9; STFHW
   p17-p19) bypasses CAN. A settings snapshot records which values were read over CAN in this node epoch;
   nothing assumes the drive's configuration was last written by this library.
8. Read-only and status objects in these groups (0x2207, 0x2218, 0x2216, 0x221C analog inputs, 0x2217 misc
   flags, 0x2019, 0x2030) are typed reads only; clearing 0x2217 belongs to stage 04's explicit clear
   operations. Product-specific objects absent from one EDS (MO-D08) stay unreadable on that product until
   a fixture shows them.

### Blocks

Deliver one tested, committed block at a time through the same context and the ordinary console route.

| Block | Objects and evidence | Policy and tests |
| --- | --- | --- |
| 08a motor, current and resolution | STF: 0x2601 idle and 0x2603 acceleration current ("Amps multiply 100", UM21 p82), 0x2604 steps/rev, 0x260D motor selection, 0x2612 custom motor sub 1-15, 0x260E open-winding velocity limit (counts/s), 0x2611 load ratio, 0x260A jerk delay. SSDC: 0x2201 peak current (mA), 0x2203 idle, 0x2206 acceleration current (absent from EDS-SSDC, MO-D08), RO 0x2207, 0x2218, 0x2216. Both: 0x6073 (0.01 A, UM21 p60) | Motor selection and custom motor data change the motor model and current: explicit commissioning intent only. 0x6073 is "maximum permissible torque creating current" in UM21 p60 but "Running Current" in AMP10 p16; its effect on an open-loop stepper is unknown (MO-D25), so its write stays unsupported on STF. Units stay native where a page gives none. `test/moons_canopen_motor_settings_test.cpp` |
| 08b inputs, outputs, brake and input functions | 0x60FD bits 16-23 X1-X8, bits 0-3 limits, home, interlock (UM21 p63). 0x60FE sub 1 Y1-Y4 in bits 0-3 and sub 2 mask, edge-triggered: write sub 2 first, then sub 1 (UM21 p63-p64; MO-D21). Output functions 0x2615/0x2214 (one 4-bit code per output, UM21 p87) and 0x2616/0x2213 alarm output on Y1; brake 0x2607/0x220C sub 1 mode, sub 2 and 3 delays in ms (UM21 p75, p83); enable input 0x2617/0x220D and alarm-reset input 0x2609/0x220E; input filters 0x260C sub 3-8 and 0x2210 in 200 us units (UM21 p76); SSDC analog 0x2212 and 0x221C | An output write requires that output's function to be general purpose (code 3) in a fresh read; a function-assigned output does not follow 0x60FE. Never toggle a brake output through 0x60FE. Writing the mask then the outputs is one operation with partial-failure evidence; mask-before-value is the documented order, not a generic CiA 402 layout. Changing the enable or alarm-reset input function changes who can enable or reset the drive: explicit, named, read back. Physical loads need their fixture. `test/moons_canopen_io_test.cpp` |
| 08c direction, filters and loop tuning | STF: 0x260F sub 1 reverse direction, sub 2 wave smoothing, sub 3 electronic damping (0 = enabled), sub 4 open-winding detect (UM21 p85), 0x2613 pulse noise filter, 0x2614 smooth filter, 0x2610 alarm mask. SSDC: 0x2208 rotation reverse, 0x220B smooth filter, 0x2211 notch filter (SSDC only), 0x2220-0x2227 gains and filters, 0x2204/0x2205 and 0x2252 in-position conditions (0x2205 in 200 us cycles), 0x221A alarm mask | 0x2610/0x221A "mask the LED flashing of the corresponding alarm" (UM21 p85); whether masking also suppresses the alarm's effect is not stated, so the setter is explicit and documents that gap. Gains have no documented units: native values only, no auto-tuning, no tuning as fault handling. Direction changes invalidate scales and origins. `test/moons_canopen_tuning_test.cpp` |

## Deliver

Profile `Settings.h/.cpp` with the settings context and typed descriptors per block; the three block test files
above with owner-backed API and console parity; console family settings routes; ledger coverage per object
(read support is not setter coverage); Doxygen; one report per block or a report with a section per block.

## Acceptance

Each block tests accepted and rejected enumerations, product refusal, whole-candidate validation, readback
mismatch, partial failure, stale cache, invalidation and no automatic save; 08b also tests the mask-then-value
order, a function-assigned output refusal and brake-output refusal. Every ledger row of the three groups has
an explicit disposition. Quick and full verify, firmware builds and exact-commit CI pass for each behavior
block. Physical I/O, brake and current validation needs fixtures. Hardware: NOT RUN.
