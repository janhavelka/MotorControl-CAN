# 03b — TL-C operations

Execute this stage under the [execution contract](execution_contract.md).
Status: NOT STARTED.

Admit TL-C models to the existing operations only where TL-C pages document the behavior, and only with TL-C
evidence records where the C-family path relies on CL86-C measurements. Uses [03a](03a_tlc_rows.md)'s reads and
status table and stage 01's model binding and identity outcome. Prompts 57, 72 and 77 are consumed if landed.
Items 1, 3, 5 and 6 define rules that 04b consumes for CL2 (common admission, evidence records and
observation source; the best-effort denial; the timed-run stop path; homing interruption). Whichever of 03b
and 04b runs first lands them for both groups with their tests, and the other consumes them.

## Work

1. Common admission. Every admitted TL-C operation requires the current `ModelBinding`, a reservation of the
   same model, its own ledger row and cell in stage 02's disposition map, and prompt 57's "Admission by
   outcome" rechecked at every changing step (stage 01 items 5 and 7): only a current outcome in which a
   reference of the declared model matches admits. Item 3's reviewed stops are exempt from the identity
   refusal only, as prompt 57 states; they keep their route, generation and source-state checks.
   - Evidence records and firmware, defined here once (04b item 1 uses it). An operation whose C path takes
     a measured record (`MoveEvidence`, `VelocityEvidence`, enable, stop and homing records) takes a record of
     the same model; a CL86-C record never admits a TL-C binding. The record's product identity fields must
     equal the current observation's. Its firmware fields follow prompt 57: a difference from the current
     identity is reported in the terminal result as "firmware unqualified" with both identities and never
     refuses. A behavioral fact the operation needs and no record of the model establishes refuses it with
     that fact's own gate (X05 enable words and velocity order, X15 stop predecessors, X16 move signals,
     X17 homing), never with a firmware reason. New-model admission applies this whether or not prompt 57 has
     landed: no new model inherits the existing `sameFirmware` refusal (`Position.cpp`, `Velocity.cpp`,
     `Homing.cpp`), which the CL86-C and OL86-C paths keep until prompt 57 changes it.
   - Observation source, also used by 04b. Completion and standstill predicates take samples from 03a's
     model-taking getter only, bound to model, operation, trigger, mode and current generations. An
     operation's repeated observation cycles are polling, so they run only after the owner records the
     TL-C read acceptance (03a item 2); until then the operation ends with its acknowledgement evidence and
     completion and standstill stay `UNKNOWN`. Ending without observations retires no stop obligation: a
     finite run keeps `needsStop` and its stop due time (item 5). No TL-C TPDO route is qualified (03a item
     6). The one operation deadline of canopen_shared 02 "Operation admission" bounds every poll, and no
     deadline expiry infers standstill or permits a later trigger.
2. Enable. TLCEN p11 and TL57ZH p10 document 6, 7, 15 at about 10 ms; TL57OP p7 says at least 10 ms between
   frames. `EnableGate` stays `CLOSED` by default; the CL86-C attended trials qualify nothing for TL-C. The
   C-family scope rule (`enableScopeHolds`: bit 10 mask, 2100 released bit, released latch) is replaced for
   TL-C by a rule over the TL-C table (bit3 alarm, bit14 running) and TL-C monitor text, with a TL-C
   source-state table (canopen_shared 02 "Operation admission", state transitions) giving the admitted
   starting states and first word. Two gate values admit it: `ATTENDED_EXPERIMENT` under that gate's existing
   terms (one attended trial with an independent drive-power cutoff, qualifying nothing), which is 07a T5's
   route, in mode 1 with targets equal to the actual position, both observed in the fresh state read and
   never established by a write (07a T5); and prompt 57's measured gate, where landed,
   with a TL-C enable record (X05). TL57OP p7's third word `0x010F` is not an enable word here; it stays an
   X05 claim (LX-D08), and no path sends it as part of an enable.
3. Stops. TLCEN p11 lists 15 to 271 stop and 15 to 11 emergency stop, so `StopKind::HALT` in modes 1/3
   (`010F`) and `QUICK_STOP` (`000B`) are documented for predecessor 15 only; HALT's mode-6 form, bit 4
   falling from an acknowledged `001F`, rests on TLCEN p14 "1 -> 0: interrupt homing". DEC-8's other
   predecessors are CL86-C measurements: a TL-C stop admits another predecessor only from a TL-C record
   (X15). A TL-C motion operation is admitted only when the stop table admits the predecessors that motion
   leaves; no TL-C motion starts without an admitted stop, and production admission has no first-measurement
   exception (07a runs such measurements only as assigned experiments). The stop table follows canopen_shared
   02 "State transitions": a halted predecessor (`010F`) or one whose halt state is unknown is not
   predecessor 15, and no TL-C page documents a quick stop that keeps bit 8 set, so those rows refuse with
   X15. Ordinary stop admission (a fresh
   predecessor the table admits) is separate from the best-effort `000B` of DEC-2, which goes out after an
   uncertain write with the predecessor unknown. TLCEN p11 documents 15 to 11 only and DEC-2's premise is a
   CL86-C measurement, so TL-C has no best-effort shape until a reviewed TL-C decision covers that case
   (X15), refused at two levels:
   - Owner. Every TL-C binding carries canopen_shared 02 S3's explicit per-binding no shape, which overrides
     the Lichuan default. The application sets it where it binds a node with a declared TL-C model, before
     the binding's first traffic, beside 03a item 2's `SdoReusePolicy::NONE`; the owner keeps it across
     rebind. The example application's model-aware routes (binding, and 06 item 2's declaration) set it and
     check it, so a direct `admitBestEffortStop` returns `INVALID` with no TX, also after a timeout and a
     rebind.
   - Profile. `prepareBestEffortStop` takes the live `ModelBinding` (stage 01 item 7's migration rule) and
     refuses a TL-C binding before TX, and the example application's best-effort path does the same.

   S3's explicit no shape does not wait for S2: if it has not landed, this stage lands that part itself (the
   per-binding override of the profile default, its retention across rebind, the `INVALID` no-TX return of
   `admitBestEffortStop` and their tests; S3's default deny for other profiles stays with S2) and records the
   claim in the canopen_shared README. Until it has landed and the no shape is set, no TL-C changing operation
   other than this item's reviewed stops is admitted: those cells stay NOT_IMPLEMENTED with S3 as the missing
   dependency (stage 02 item 9). The reviewed stops stay admissible
   then, since refusing one could leave a running drive running. No parallel transmitter; CL86-C and OL86-C keep DEC-2 and its tests
   unchanged. Stop completion stays `UNKNOWN` without TL-C stop evidence (X15) and the observation source of
   item 1.
4. Position. Documentary objects and ranges match the C family (TLCEN p10): `6060`=1, `607A` I32
   -1000000..1000000 command pulses, `6081` 0..3000 rpm, `6083`/`6084` 0..2000 ms. Triggers are 15 to 31, 95, 63
   and 127 (TLCEN p11); TL57OP p7's `0x011F` start is refused (LX-D08). `preparePositionParameters` admits TL-C
   models with these bounds; `prepareMoveAbsolute`, `prepareMoveRelative` and `prepareMoveAngle` need TL-C
   `MoveEvidence` (X16): acceptance and completion on the TL-C table (bit 15 position complete with a fresh
   transition after the trigger, from item 1's observation source). Relative basis and scale stay caller
   qualifications under X16 (Q04 does not transfer).
5. Velocity. LX-D07: TLCEN p12 writes the targets and enabling starts motion; TL57OP p7 writes `60FF` after
   enabling, says that starts motion and that writing 0 stops it. The C17 decision does not transfer: TL-C
   start order stays X05, `prepareVelocity` refuses TL-C until a TL-C `VelocityEvidence` record fixes the order,
   and a zero target is never a stop operation. Once a record admits it, the run is a timed run under
   canopen_shared 02 "Operation admission" (the existing `durationUs`, `requiredStopBudgetUs`, `needsStop`
   and `STOPPING` design, with the application's separate urgent stop at stop due): item 1's
   acknowledgement-only end never clears `needsStop`, and preparation refuses unless item 3's stop table
   admits every state the run can leave, a lost start reply included. A stop row is not a stop path: after
   a lost start reply under `NONE` the route stays unresolved, and TL-C has neither an S3 shape nor a native
   equivalent (item 3). So preparation also refuses, before the first changing TX, unless the application
   declares an independent stop for the run (its own enable, STO or drive-power path), which the result
   names (canopen_shared 02 timed runs). If the urgent stop cannot be admitted at stop due, the result keeps
   `needsStop`, reports motion unknown and names that declared stop.
6. Homing. Methods 17, 18, 24 and 29 with the EN86 descriptions (TLCEN p12..13). Method 28 (TL57OP p7, written
   with a stray byte `0x1E`) is refused. Each method needs a TL-C homing record for that method (X17);
   completion uses TL-C bit 8 (TLCEN p14) from item 1's observation source.
   DI0 origin, DI1 positive and DI2 negative limit (TLCEN p7..8) are factory claims: read the actual
   `2400:02..06` assignment (prompt 77's input facts where landed); never disable a limit to run a method.
   Interruption, defined here once (04b item 6 uses it). A home whose signal or completion never arrives
   ends only when the host interrupts it, so it follows item 5's stop-path rule. Preparation refuses before
   TX without the model's read acceptance (without polling no completion can be established), without a
   search bound in the method's record (travel or time, the caller's value) that ends before the one
   operation deadline, and without an application-declared independent stop named in the result. The
   application requests item 3's mode-6 HALT (bit 4 falling from an acknowledged `001F`) when that bound or
   the deadline passes without the expected signal or completion transition, or when observation is lost
   or expires after a possibly sent trigger. Where that HALT cannot be admitted (no fresh predecessor, an
   unresolved route, no best-effort shape), only the declared stop remains: the result keeps `needsStop`
   and reports motion and reference unknown.
7. Alarm clear. `CONTROLWORD_BIT7` is documented (TLCEN p14; 15 to 128 on p11). `OBJECT_2301` is refused for
   TL-C: `2301` is declared by TLEDS l.1404 but absent from TLCEN p17 and TL57ZH p16 (LX-D11, X07). The C
   default choice is a CL86-C bench decision; TL-C callers name the method.
8. Position clear. `prepareSetDevicePosition` (`2302` 0 to 1, then manual 0; TLCEN p14, p17; TL57OP p7)
   changes the device reference, so its admission is defined apart from its result wording. It is GATED
   under X04 until a TL-C status record of that model qualifies the freshness of bits 3 and 14 after
   transitions (07a T4, 07b), as moves need `MoveEvidence`. With that record it is admitted, with the
   C-family reference invalidation, only when all of these hold:
   - one explicit fresh state read through the model-taking getter, under the continuous reservation that
     also carries the writes, shows mode 1, bit3 alarm and bit14 running clear (TLCEN p11) and `606C` zero;
     a C-family or other-model sample is refused;
   - no earlier operation on the binding left motion `UNKNOWN`, `needsStop` set or its route unresolved,
     as the application's retained terminal of the binding's last motion operation shows (a proposed
     preparation input);
   - mode, node generation and server session are rechecked before each `2302` write (stage 01 item 5),
     and a change withdraws the pending write unsent.

   This is a controller-reported predicate, not physical standstill: on TLO `6064` and `606C` are not
   measured feedback (LX review, "Units and scaling": TLO has no encoder), so the result claims no
   standstill. Where a test needs a stationary shaft, the attendant observes it (07a).
9. I/O settings. A TL-C DI candidate covers `2400:01` bits 0..4 and `2400:02..06` codes 0..13 (TLCEN p17);
   `2400:07..08` are never written, so the C-family seven-slot `IoSettings` does not transfer. Output functions,
   polarity and `60FE` stay gated (X06; LX-D10: DO0/DO1 against DO1/DO2, brake on DO2 in TL57OP p4).
10. Tuning and scale. `2500` 0/1/2 (TLCEN p18); `CLOSED_LOOP` is refused for TLO models (no encoder, TLCEN p3).
    `2502` is mA (TL57OP p7: 1000 is 1 A) with no documented bounds: its setter needs X14's fixture rating and
    stays refused until then (LX-D09: 2.0/4.0/6.0 A against 2.8/5.6/7.7 A). That rating is a caller record
    scoped to the fixture's model: a positive peak-current limit in mA, the basis `2502` uses ("Maximum
    Peak Current, Unit: mA", TLCEN p18). The caller converts a rating given in another basis with its own
    evidence, never the library. A missing or zero limit, or a value above it, refuses with zero TX
    (MISSING_PREREQUISITE or `INVALID_CONFIG`); 04b item 10 applies the same rule to CL2. `2508` onward are "factory default,
    generally not adjustable" (TLCEN p18..19): no setter. `2201` keeps 400..51200 (TLCEN p17); the CL86-C pulse-rate
    limit (S21, prompt 72) and the measured pulses per turn do not transfer.
11. Save and restore. `2300`=2 save and 1 factory restore are documented (TLCEN p14, p17; TL57OP p7). A TL-C
    save also makes a stored bitrate selection effective at the next restart when SW5 is ON (TL57OP p5).
    `prepareSave` for TL-C therefore needs fresh raw reads of `2402` and `2407` from the current node
    generation and a caller acknowledgement record that the route may change after restart; its result keeps
    that obligation on success and on a lost acknowledgement (X18). The library sending no selector or node
    write does not mean the device answers on the same route after restart: a selection staged earlier, by
    the vendor software for example (TL57OP p5), can take effect then. A restart invalidates dependent
    evidence, and the next session reacquires explicitly on a declared route with no replay. Factory restore
    carries the same obligation with the restored code unknown. Nothing saves implicitly. No TL-C save or
    restore runs on hardware while the owner is away: no TL-C fixture exists then (canopen_shared 02 "Bench
    steps and owner absence"; COMMON's rule 2 lists the CL86-C fixture's objects).
12. Release and mode select ([prompt 57](../57_regular_drive_operations.md)). Before prompt 57 lands both are
    NOT_IMPLEMENTED for every model (no library operation exists; the examples' raw `6060` writes stay
    CL86-C-only). After it lands, `prepareRelease` stays GATED under X15 for TL-C, because every release-table
    row is a CL86-C measurement, and `prepareModeSelect` admits TL-C only for the modes TLCEN documents (1,
    3 and 6, TLCEN p10..14) with a TL-C source-state table for its not-enabled check; without that table it is GATED under
    X15. Stage 02's disposition map records both states; if prompt 57 lands after this set's closeout, its
    landing commit applies them, as stage 02 item 9's checker rule requires.

## Decisions that do not transfer here

`EnableGate` trials, `enableScopeHolds` facts, DEC-8 predecessors, DEC-2's measured premise, `AlarmClearMethod`
default, C17, `HomingMethod` trajectory evidence, the seven-input `IoSettings`, Q04 scale, S21 and the velocity
ladder, the C-family save premise that `2300` changes no route, the release table's CL86-C rows, and the
existing `sameFirmware` refusal.

## Deliver

TL-C admission in `Actions`, `Position`, `Velocity`, `Homing`, `Settings`, `Tuning` and `Persistence` with
Doxygen naming pages and gates; `test/tlc_operations_test.cpp` through the real owner, `SdoClient` and
`LichuanTlcDrive.h`: documented words and frames, refusals of `0x010F` enable, `0x011F` start, method 28,
`2301`, closed loop on TLO, `2502` without a rating, `2400:07..08`, save without the bitrate acknowledgement,
CL86-C evidence on a TL-C binding, and a stop admitted before every admitted motion. Identity tests: a
contradiction before preparation and after the stop was queued, each leaving the reviewed stop admitted (no
refusal for identity alone, its result carrying the outcome and claiming no standstill) while every other
pending changing step is retired. Best-effort tests: an unresolved route after a possibly sent controlword
and a stale binding, each refused with no TX; a direct `admitBestEffortStop` on a TL-C binding carrying the
explicit no shape, after a timeout and after a rebind (`INVALID`, zero TX); CL86-C best-effort tests
unchanged. Stop-table tests with halted and unknown-halt predecessors (refused, no TX). Timed-run tests with
a synthetic TL-C velocity record: canopen_shared 02's cases (duration plus budget not fitting, lost start
reply, observation in flight near stop due, service resuming after stop due, queue-pressured stop); a run
without a declared independent stop, refused at preparation with no changing TX; and with one, a possibly
sent start followed by a timeout under `NONE` (`needsStop` kept, motion unknown, the declared stop named,
no CANopen stop TX). Homing interruption tests: no read acceptance, no search bound or no declared stop
(refused before TX), an absent or stuck signal, and observation loss or expiry after a possibly sent
trigger (HALT requested where admissible, otherwise `needsStop` kept and the declared stop named). Position
clear tests: GATED with zero TX without the X04 record; with a synthetic record, a stale zero or
running-clear report, an earlier move left `UNKNOWN`, an unresolved route, and a mode or generation change
before dispatch, each refused or withdrawn with no `2302` TX. `2502` tests: no limit, a zero limit and a
value above it, each with zero TX. Paired evidence tests:
a TL-C record differing from the current identity only in firmware (admitted, result reports "firmware
unqualified" with both identities) and one lacking the needed fact (refused with its gate, no TX).
Observation tests: a bit already high before the trigger, a mode change or new generation between samples,
a stale or other-model sample, a missing transition, a fault, no read acceptance (completion `UNKNOWN`) and
expiry of the one operation deadline, each with no inferred standstill and no later trigger. Save and
restore keep the route-after-restart obligation on success and on a lost acknowledgement. Ledger, coverage,
`docs/api_cli_mapping.md`, CHANGELOG, report and `PROGRESS.md`.

## Acceptance

Each TL-C operation is either admitted with cited pages and a TL-C evidence input, or refused before TX with
its disposition from stage 02's map: an X gate, NOT_DOCUMENTED, NOT_IMPLEMENTED or MISSING_PREREQUISITE. No
CL86-C measurement admits a TL-C binding, and no firmware difference refuses one. Existing CL86-C tests are
unchanged. No TL-C changing operation other than a reviewed stop is admitted before S3's explicit no shape
is set on its binding.
Quick and full verification, firmware builds and exact-commit CI pass. Hardware NOT RUN; X04, X05, X06,
X07, X14, X15, X16, X17 and X18 stay open.
