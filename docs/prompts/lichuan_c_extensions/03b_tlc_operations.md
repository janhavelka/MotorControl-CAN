# 03b — TL-C operations

Execute this stage under the [execution contract](execution_contract.md).
Status: NOT STARTED.

Admit TL-C models to the existing operations only where TL-C pages document the behavior, and only with TL-C
evidence records where the C-family path relies on CL86-C measurements. Uses [03a](03a_tlc_rows.md)'s reads and
status table and stage 01's model binding and identity outcome. Prompts 57, 72 and 77 are consumed if landed.

## Work

1. Common admission. Every admitted TL-C operation requires the current `ModelBinding`, a reservation of the
   same model, an identity outcome other than CONTRADICTED before any manufacturer-area download (the
   execution contract's vendor-object rule), and its own ledger row. Operations whose C path takes measured evidence (`MoveEvidence`, `VelocityEvidence`, enable and
   stop records) take a TL-C record with model and firmware equal to the current identity; a CL86-C record
   never admits a TL-C binding.
2. Enable. TLCEN p11 and TL57ZH p10 document 6, 7, 15 at about 10 ms; TL57OP p7 says at least 10 ms between
   frames. `EnableGate` stays `CLOSED` by default; the CL86-C attended trials qualify nothing for TL-C. The
   C-family scope rule (`enableScopeHolds`: bit 10 mask, 2100 released bit, released latch) is replaced for
   TL-C by a rule over the TL-C table (bit3 alarm, bit14 running) and TL-C monitor text, admitted only with a
   TL-C enable record. TL57OP p7's third word `0x010F` is not an enable word here; it stays an X05 claim for
   07a (LX-D08), and no path sends it as part of an enable.
3. Stops. TLCEN p11 lists 15 to 271 stop and 15 to 11 emergency stop, so `StopKind::HALT` in modes 1/3
   (`010F`) and `QUICK_STOP` (`000B`) are documented for predecessor 15 only; HALT's mode-6 form, bit 4
   falling from an acknowledged `001F`, rests on TLCEN p14 "1 -> 0: interrupt homing". DEC-8's other
   predecessors are CL86-C measurements: a TL-C stop admits another predecessor only from a TL-C record. A TL-C motion operation is
   admitted only when the stop table admits the predecessors that motion leaves, or, for 07a's first
   measurements only, with a caller plan record naming a bounded finite move, the best-effort fallback and an
   independent power cutoff; no other TL-C motion starts without an admitted stop. The best-effort `000B` route is unchanged; for TL-C it is the documented 15 to 11
   shape with unknown effect. If canopen_shared seam S3 has landed, the model declaration sets the binding's
   shape explicitly from the TL-C ledger review (TLCEN p11) instead of relying on the Lichuan default. Stop
   completion stays `UNKNOWN` without TL-C stop evidence.
4. Position. Documentary objects and ranges match the C family (TLCEN p10): `6060`=1, `607A` I32
   -1000000..1000000 command pulses, `6081` 0..3000 rpm, `6083`/`6084` 0..2000 ms. Triggers are 15 to 31, 95, 63
   and 127 (TLCEN p11); TL57OP p7's `0x011F` start is refused (LX-D08). `preparePositionParameters` admits TL-C
   models with these bounds; `prepareMoveAbsolute`, `prepareMoveRelative` and `prepareMoveAngle` need TL-C
   `MoveEvidence`. Relative basis and scale stay caller qualifications (Q04 does not transfer).
5. Velocity. LX-D07: TLCEN p12 writes the targets and enabling starts motion; TL57OP p7 writes `60FF` after
   enabling, says that starts motion and that writing 0 stops it. The C17 decision does not transfer: TL-C
   start order stays X05, `prepareVelocity` refuses TL-C until a TL-C `VelocityEvidence` record fixes the order,
   and a zero target is never a stop operation.
6. Homing. Methods 17, 18, 24 and 29 with the EN86 descriptions (TLCEN p12..13). Method 28 (TL57OP p7, written
   with a stray byte `0x1E`) is refused. Completion uses TL-C bit 8 (TLCEN p14) only with TL-C homing evidence.
   DI0 origin, DI1 positive and DI2 negative limit (TLCEN p7..8) are factory claims: read the actual
   `2400:02..06` assignment (prompt 77's input facts where landed); never disable a limit to run a method.
7. Alarm clear. `CONTROLWORD_BIT7` is documented (TLCEN p14; 15 to 128 on p11). `OBJECT_2301` is refused for
   TL-C: `2301` is declared by TLEDS l.1404 but absent from TLCEN p17 and TL57ZH p16 (LX-D11, X07). The C
   default choice is a CL86-C bench decision; TL-C callers name the method.
8. Position clear. `prepareSetDevicePosition` (`2302` 0 to 1, then manual 0; TLCEN p14, p17; TL57OP p7) is
   admitted for TL-C with the C-family stationary admission and reference invalidation.
9. I/O settings. A TL-C DI candidate covers `2400:01` bits 0..4 and `2400:02..06` codes 0..13 (TLCEN p17);
   `2400:07..08` are never written, so the C-family seven-slot `IoSettings` does not transfer. Output functions,
   polarity and `60FE` stay gated (X06; LX-D10: DO0/DO1 against DO1/DO2, brake on DO2 in TL57OP p4).
10. Tuning and scale. `2500` 0/1/2 (TLCEN p18); `CLOSED_LOOP` is refused for TLO models (no encoder, TLCEN p3).
    `2502` is mA (TL57OP p7: 1000 is 1 A) with no documented bounds: its setter needs X14's fixture rating and
    stays refused until then (LX-D09: 2.0/4.0/6.0 A against 2.8/5.6/7.7 A). `2508` onward are "factory default,
    generally not adjustable" (TLCEN p18..19): no setter. `2201` keeps 400..51200 (TLCEN p17); the CL86-C pulse-rate
    limit (S21, prompt 72) and the measured pulses per turn do not transfer.
11. Save and restore. `2300`=2 save and 1 factory restore are documented (TLCEN p14, p17; TL57OP p7). A TL-C
    save also makes a stored bitrate selection effective at the next restart when SW5 is ON (TL57OP p5).
    `prepareSave` for TL-C therefore needs fresh raw reads of `2402` and `2407` from the current node
    generation and a caller acknowledgement record that the route may change after restart; its result keeps
    that obligation. Factory restore carries the same obligation with the restored code unknown. Nothing saves
    implicitly. No TL-C save or restore runs on hardware while the owner is away: no TL-C fixture exists then
    (canopen_shared 02 "Bench steps and owner absence"; COMMON's rule 2 lists the CL86-C fixture's objects).

## Decisions that do not transfer here

`EnableGate` trials, `enableScopeHolds` facts, DEC-8 predecessors, DEC-2's measured premise, `AlarmClearMethod`
default, C17, `HomingMethod` trajectory evidence, the seven-input `IoSettings`, Q04 scale, S21 and the velocity
ladder, and the C-family save premise that `2300` changes no route.

## Deliver

TL-C admission in `Actions`, `Position`, `Velocity`, `Homing`, `Settings`, `Tuning` and `Persistence` with
Doxygen naming pages and gates; `test/tlc_operations_test.cpp` through the real owner, `SdoClient` and
`LichuanTlcDrive.h`: documented words and frames, refusals of `0x010F` enable, `0x011F` start, method 28,
`2301`, closed loop on TLO, `2502` without a rating, `2400:07..08`, save without the bitrate acknowledgement,
CL86-C evidence on a TL-C binding, and a stop admitted before every admitted motion. Ledger, coverage,
`docs/api_cli_mapping.md`, CHANGELOG, report and `PROGRESS.md`.

## Acceptance

Each TL-C operation is either admitted with cited pages and a TL-C evidence input, or refused before TX with
its X gate. No CL86-C measurement admits a TL-C binding. Existing CL86-C tests are unchanged. Quick and full
verification, firmware builds and exact-commit CI pass. Hardware NOT RUN; X05, X06, X07 and X14 stay open.
