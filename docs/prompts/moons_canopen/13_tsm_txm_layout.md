# 13 — TSM/TXM-C 2010 object layout

Execute this stage under the [execution contract](execution_contract.md).
Status: NOT STARTED. Requires the 02-12 contexts and the AMP10 ledger entries of 02. MO-D02, MO-D29, MO-D30
and MO-D32 are owned here; MO-D03 (2010 units), MO-D05 (codes 2 and 6) and MO-D23 (restore "not yet") apply.

The integrated TSM17C, TSM23C, TSM24C, TSM34C, TXM24C and TXM34C (C-xCG part numbers, TSM34 p16; TSM24 p2-p3)
follow EDS-TSM, whose objects match the 2010 AMP10 layout, not UM21. No current CANopen manual for them was
found; AMP10 Rev F names only ST5/10-C, STM-C, STAC6-C and SV7-C (AMP10 p3, p9), and Rev K, reported to name
TSM/TXM, was not obtainable (remote_only AMP10K; MO-D30). Applicability of AMP10 is therefore inferred from the
EDS layout. This stage never merges the 2010 layout into the UM21 tables, contexts or tests.

## Work

1. Evidence gate. Part A runs now: generate the AMP10 table and implement identity and read-only typed reads,
   every meaning labelled "inferred from EDS-TSM and AMP10 Rev F". Part B (motion, homing, settings,
   watchdog, persistence) stays BLOCKED until the owner obtains AMP10 Rev K or a current TSM/TXM CANopen manual
   in a browser and an intake adds it to the pack with its hash. While the owner is away, record that action
   and the operations it unlocks as a row of the [deferred table](../README.md); do not wait for it.
2. Generation: emit the AMP10 table from the ledger with its own key type, so an AMP10 key cannot index a UM21
   table and the reverse. Vendor objects include 0x7001-0x7012, user registers 0x4000-0x4016, tuning and
   configuration 0x5000-0x5029, TPDO masks 0x2014-0x2017, 0x2100 error status bits (OCTET_STRING of 10 bytes),
   0x2101/0x2102 node ID and bit rate, 0x2103/0x2104 SYNC counter and time, 0x2200 FWName and 0x2300 user
   watchdog (EDS-TSM). 0x2300 means user watchdog here, save/restore on the Lichuan CL86-C and NanoJ on N5:
   object numbers never carry meaning across profiles.
3. Identity, with stage 03's admission table and outcomes: the application declares the TSM/TXM-C row and the
   exact model. Layout evidence is vendor ID 0x000002D9 plus an abort of the absent 0x2050 with its raw code
   retained and its read effect reviewed; another abort code, a timeout or a 0x2050 value is UNCONFIRMED (a
   value is evidence of UM21). 0x2200 FWName and 0x100A are recorded raw and identify no model: a value equal
   to an EDS default ("UNK", "2.12") or any other unreferenced string is not model evidence. The exact model
   stays the declaration; Part A reads are labelled inferred, and every model-specific Part B operation stays
   gated until the new manual and identification evidence for that model (a fixture-recorded string or object
   tied to the model's label) both exist. Never read 0x2100 typed before the layout is confirmed: it is 10
   bytes here and 23 INT32 registers in UM21.
4. Units (MO-D03): AMP10 p19 gives current 0.01 A, velocity 0.0042 rps, acceleration 0.1667 rps/s and distance
   1 step, with an arithmetic error in its own example (550 x 0.1667 stated as 91.85). The exact values 1/240
   rev/s and 1/6 rev/s² come from HCR p285-p286 (J, U and V registers in 0.25 rpm) and p283 (A register in
   10 rpm/s), which describe SCL registers, not these objects. Use 1/240 and 1/6 only as a reviewed inference
   recorded in the ledger; common physical units need a caller qualification bound to fixture evidence.
5. Widths (AMP10 p14): 0x606C, 0x6081, 0x6083, 0x6084, 0x6099 sub 1-2, 0x609A and 0x60FF are 32-bit objects of
   which only the low 16 bits are used, and the upper bits are written as zero; 0x7003 and 0x60FE use 8 bits.
   Check prepared values against 16 bits. Whether a negative 0x60FF is a 16-bit or 32-bit two's complement is
   not stated: reverse velocity stays refused until resolved. 0x7009 velocity actual is UNSIGNED16 in EDS-TSM
   (MO-D32): it cannot show direction as declared; publish it raw.
6. Feedback: 0x700A position and 0x7009 velocity are "calculated via motor encoder", while 0x6064 and 0x606C are
   calculated targets (AMP10 p16-p17). Keep commanded and encoder-derived values separate; 20000 counts/rev
   encoders (TSM24 p3, TSM34 p4) against 0x5017/0x5018 readings stay facts, not a scale.
7. Modes (MO-D29): TSM24 p3 and TXM24 p3 list torque, velocity, position, homing and Q; TSM34 p4 lists torque
   and velocity only; TXM34 p4 torque, velocity and position. Admit modes per declared model only in Part B,
   from its own hardware manual and with item 3's identification evidence for that model; Part A admits no
   mode. Homing on the 34-frame motors stays unsupported.
8. Part B items, recorded now and implemented with the new manual: quick stop codes "2 & 6 only" (AMP10 p16;
   code 6 stays in Quick stop active, unlike UM21); halt keeps torque in velocity mode and frees the shaft in
   torque mode (AMP10 p26, p45); AMP10 p21's 0x001F single-step enable-and-start; home switch 0x7001 range 1-6
   (AMP10 p17) and methods 1-14, 17-30, 33-35 without 37, method 35 ignoring the home offset (AMP10 p42, p44);
   user registers 0x4000-0x4016 with Q register names (AMP10 p18); watchdog 0x2300; TPDO masks 0x2014-0x2017;
   RTR and statusword-change triggering of AMP10 p51-p52, which stays unsupported (MO-D13); node ID and bitrate
   by switch on TSM24C and only by Quick Tuner on TXM24C, TSM34C, TXM34C (TSM24 p12; TXM24 p12; TSM34 p19;
   TXM34 p20); 0x1011 sub 1 "(not yet)" (MO-D23) keeps restore unsupported.

## Deliver

Part A: generated AMP10 headers, the AMP10 identity and read paths with their own observation types,
`test/moons_canopen_amp10_reads_test.cpp`, console read routes for TSM/TXM-C bindings, ledger and coverage rows
with "inferred" applicability, report. Part B: the remaining operations through the existing contexts with
layout-typed descriptors and their own tests, only after the manual intake.

## Acceptance

No AMP10 constant, unit or fixture is reachable from a UM21 binding, and no UM21 one from an AMP10 binding;
tests prove both. Width checks reject values above 16 bits; MO-D32 stays raw. Part B operations reject with
their gate before TX while blocked, and a declared model without identification evidence admits no
model-specific operation; "UNK", "2.12" or an arbitrary abort never confirms a model. Generator check, family
checkers, quick and full verify and exact-commit
CI pass. Hardware: NOT RUN.
