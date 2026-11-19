# 08 — I/O, brake, encoder and drive settings

Execute this stage under the [execution contract](execution_contract.md).
Status: NOT STARTED. Runs after 03 and 05. DS-D24..DS-D28, DS-D30 and DS-D31 belong here; DS-D34 applies to
`6091:02`. MULTIDRIVE rows 20, 21, 22 and 69 apply. Most vendor functions are P-parameters without a CANopen index;
this stage implements the reachable 6000-group settings and records every unreachable function as UNSUPPORTED with
its evidence reason.

## Work

1. One settings context. `SettingsContext` with `prepareDriverRead`, `prepareDriverSettings`, `nextDriver`,
   `advanceDriver`, `getDriver`: prior read, whole-candidate validation, ordered writes, readback, partial evidence and
   a restore record. No automatic save exists (10). Admission requires a released drive or a fresh stationary report;
   the effect columns of the P tables do not cover 6000-group objects, so activation timing is unknown.
2. Reachable settings, each with ledger key, range and unit from 02: following-error window `6065` (U32 pul, default
   131072), `6066` (U16, "following-error count", RW in the table; read-only until its meaning is clear), in-position
   window `6067` (U32 pul, 500) and time `6068` (U16 ms, 100), velocity window `606D` (pul/s) and time `606E` (ms),
   quick-stop deceleration `6085` (U32 pul/s², 10000), electronic gear `6091:01`/`:02` (DS2C p27-28; the `:02` write
   width stays under DS-D34 until the ledger's reviewed policy settles it). A `6091` change invalidates scale and
   origin generations. `607E` polarity (U8, range 0..65535, no bit meanings; DS2C p28) and `605D`
   (halt unsupported, DS-D17) are read-only.
3. Inputs. `60FD` bits 0..2 are documented live states; bits 16..31 are "user" without mapping; the printed default
   10000 is text only (DS-D30). DI functions and logic (`P03-02`..`P03-13`, functions 0..37, logic low, high, rising,
   falling, both; DS2C p9, p16-17) are unreachable: the caller declares them per binding for 07 and the application.
   ISL touch probes FunIN.38/39 with `60B8h` (ISL60 p12, ISL80 p11) are UNSUPPORTED (DS-D28).
4. Outputs. `60FE:01` physical outputs and `:02` enable mask, bits 16..31 user, 0 invalid and 1 valid (DS2C p29); which
   bit drives DO1 or DO2 and how `60FE` interacts with `P04` assignments are undocumented. Read both; writes stay
   UNSUPPORTED until a fixture maps a bit to a terminal under an explicit plan. `P0D-17`..`P0D-19` forcing is never a
   profile path. FunOUT functions (DS2C p10, p17; IBL adds FunOUT.9 brake) are caller-declared assignments.
5. Brake (DS-D26). No CANopen brake object exists. DS2 drives BRK+/BRK- from an internal supply with `P02-09`/`P02-10`
   delays (DS2C p6, p16; Er.625/Er.626, DS2C p31); IBL and ISL use DO FunOUT.9 (IBL60 p11, ISL60 p13); MINI shows no
   brake pins but a brake in its wiring figure (MINI p8, p10). `holdingBrake` stays a caller fact; the library never
   actuates or infers a brake and 04's release result never claims engagement.
6. Encoder. The declared suffix gives 17 or 23 bits, incremental or absolute; `P00-31`/`P00-32` lines and `P02-01`
   absolute mode are unreachable (DS2C p15-16). Whether `6064` restores a multi-turn absolute position after power-up
   is undocumented: no reference survives a reboot without fresh evidence. `P0D-20` absolute reset, `P0D-04` encoder
   ROM, `P0D-03` angle identification and IBL `P0D-13` calibration (IBL60 p24) are vendor-tool operations, never
   library paths; calibration evidence is caller-declared.
7. Gains and tuning. `P08` and `P09` (DS2C p18-21), including auto-tuning and inertia identification that move the
   motor, and gain switching FunIN.3, have no CANopen index: UNSUPPORTED. Never probe `2000h`-`5FFFh` for a hidden map.
8. Motor, ratings, faults. Motor and drive codes `P00-00`/`P01-02` are vendor-tool settings with a password and power
   cycle (ISLMP p1-2, DS2OP p5-6; Er.120; DS-D31): UNSUPPORTED; the caller declares the motor. Ratings (DS-D24) and
   product-page conflicts (DS-D25) are documentation facts with no current-limit API (`6072` is torque). Fault texts
   copied from an AC drive (DS-D27) stay in a documentation-only catalogue keyed by raw code; protections `P0A` are
   unreachable and never disabled.
9. Following error. `6065` (window, default 131072 pul; DS2C p27) and `P0A-10` (position-deviation fault threshold,
   default 3145728, encoder or command units; DS2C p21, Er.B00 on p32) have no stated relation. A `6065` setter
   documents that it may not move the fault threshold; no setter lowers protection to provoke a fault.
10. Dependent evidence. Under CiA 402, `6067`/`6068` and `606D`/`606E` define the windows behind "reached" reports;
   DS2 sources do not link them to the dialect bits 05 and 06 observe. Treat a change as one that may alter those
   bits: it bumps the configuration generation, so retained completion evidence and declarations bound to the old
   values are not reused.

## Deliver

`Settings.h`/`Settings.cpp` with `SettingsContext`, `Io.h`/`Io.cpp` for `60FD`/`60FE` observations;
`test/lichuan_ds2_settings_test.cpp`, `test/lichuan_ds2_io_test.cpp`; owner-backed `ds2 settings` and `ds2 io`
routes; every unreachable function as a coverage row with reason; `docs/driver_settings.md` and `docs/io_settings.md`
DS2 sections; Doxygen and CHANGELOG.

## Acceptance

Tests cover full-candidate validation, ordered writes and readback, partial failure with restore record, refusal while
enabled, scale invalidation on `6091`, `60FE` writes refused, read-only `607E`, `605D` and `6066`, and an UNSUPPORTED
result without TX for every unreachable function. The coverage denominator lists every P-parameter group. Quick and
full verification and required CI on the exact final pushed commit pass. Physical I/O, brake and encoder checks need a
DS2 fixture; hardware NOT TESTED. Commit, push and hand off.
