# 03b — TL-C operations

Execute this stage under the [execution contract](execution_contract.md).
Status: NOT STARTED.

Admit TL-C models to the existing operations only where TL-C pages document the behavior, and only with TL-C
evidence records where the C-family path relies on CL86-C measurements. Uses [03a](03a_tlc_rows.md)'s reads and
status table and stage 01's model binding and identity outcome. Prompts 57, 72 and 77 are consumed if landed.
Items 1, 3, 4, 5, 6, 8, 10 and 12 define rules that 04b consumes for CL2 (common admission, evidence records and
observation source; the best-effort denial and the halt-state derivation; the move interruption choice; the
timed-run stop path, its fallback deadline and the stop obligation; homing
fixture signals and interruption; position clear's
retained last-motion terminal input and its rechecks before each write; the caller's peak-current record and
its refusals; the release and mode-select dispositions). Whichever of 03b and 04b runs first lands their
shared parts for both groups with their tests, and the other consumes them.

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
   source-state table (canopen_shared 02 "Operation admission", state transitions). TLCEN p11 (TL57ZH p10
   the same) only pairs each controlword with low bits (6040=0 with 000, 6 with 001, 7 with 011, 15 with
   111) and documents no other starting state, so the rows, read from one fresh model-taking state read
   under the reservation, are:

   | Fresh report | First word, or none | Disposition |
   | --- | --- | --- |
   | Low bits 000, bit3 alarm and bit14 running clear, a mode the gate admits | `0x0006`; `0x0007` and `0x000F` each only after the previous word's acknowledgement and a fresh report with its paired low bits (001, then 011) and bits 3 and 14 clear | Admitted on a gate below |
   | Low bits 111 | None | Reported as already enabled, nothing sent; it admits nothing else |
   | Low bits 001, 011 or another pattern | None | GATED under X15: no TL-C page documents a start from a partial sequence |
   | Bit3 alarm or bit14 running set | None | Refused: an alarm needs item 7 first, a running drive a stop |
   | Another mode, or a stale, missing or other-model report | None | Refused before TX |

   A report during the sequence without the word's paired low bits, or with bit3 or bit14 set, ends it with
   no further word. After an acknowledged `0x000F`, a fresh report with low bits 111 is predecessor 15 for
   item 3's stop table; any other fresh report admits no stop row, so nothing further is sent, no stop is
   prepared, and the result reports that report with the enable state unknown (07a T5).
   Two gate values admit the first row: `ATTENDED_EXPERIMENT` under that gate's existing
   terms (one attended trial with an independent drive-power cutoff, qualifying nothing), which is 07a T5's
   route, in mode 1 with the target at the actual position, observed and never established by a write; and
   prompt 57's measured gate, where landed, with a TL-C enable record (X05). Raw `607A` equal to `6064` in
   the fresh state read is a screen, not that prerequisite: the same integer can name different positions,
   TLCEN p10 and p15 give `6064` no unit, and on TLO it is a controller report. The attended route also
   needs the caller's basis record for the fixture: evidence for that fixture, model and firmware that
   `607A` and `6064` share one coordinate basis, scale and origin (a vendor clarification or an assigned
   experiment's retained result; X16 stays open). Without it the route refuses with MISSING_PREREQUISITE
   and zero TX, also when the raw values are equal. The basis record makes the two values comparable; it
   does not make `6064` the shaft position. TLO models have no encoder (TLCEN p3; LX review "Units and
   scaling"), so on TLO the attended route also needs the caller's actual-position record for this
   admission: an independent fixture measurement of the shaft (an external encoder or a reviewed
   mechanical reference) on the basis record's coordinates, with no motion write since it was taken.
   Without it the equality is a controller-coordinate screen only and the route refuses with
   MISSING_PREREQUISITE and zero TX, a valid basis record and equal raw values notwithstanding; 07a T5
   then uses its bounded experiment or is NOT RUN. On TLC the same applies unless the basis record also
   shows that `6064` reports the encoder in the configured loop mode, which no TL-C page states. TL57OP
   p7's third word `0x010F` is not an enable word here; it stays an X05 claim (LX-D08), and no path sends
   it as part of an enable.
3. Stops. TLCEN p11 lists 15 to 271 stop and 15 to 11 emergency stop, so `StopKind::HALT` in modes 1/3
   (`010F`) and `QUICK_STOP` (`000B`) are documented for predecessor 15 only; HALT's mode-6 form, bit 4
   falling from an acknowledged `001F`, rests on TLCEN p14 "1 -> 0: interrupt homing". DEC-8's other
   predecessors are CL86-C measurements: a TL-C stop admits another predecessor only from a TL-C record
   (X15). A TL-C motion operation is admitted only when the stop table admits the predecessors that motion
   leaves; no TL-C motion starts without an admitted stop, and production admission has no first-measurement
   exception (07a runs such measurements only as assigned experiments). The stop table follows canopen_shared
   02 "State transitions": a stop word keeps a set halt bit and, with the halt state unknown, sets bit 8.
   TLCEN p11 gives controlword bit 8 no meaning other than the 271 stop, so from a fresh report with low
   bits 111 whose halt state is set or unknown, HALT stays `010F` and the quick stop goes out as `010B`; no
   stop is refused merely for an unknown halt state. The halt state at stop dispatch follows that section's
   derivation from the words actually sent: clear only after an acknowledged bit-8-clear word with no later
   possibly sent bit-8-set word in the current reservation, and unknown when a declared external writer
   makes the interface's controlword history unqualified (canopen_shared 02 "Ownership premise"). The TL-C
   status table has no halt bit (03a item 4), so the status bits never decide it. No TL-C source states
   that a `6040` upload returns the received word, so the readback alternative stays unused until a
   reviewed TL-C ledger row shows it. No TL-C page documents `010B` (p11 lists 15 to 11), so
   its effect stays X15's question, and the result records the halt state and that the word is
   undocumented. Ordinary stop admission (a fresh
   predecessor the table admits) is separate from the best-effort `000B` of DEC-2, which goes out after an
   uncertain write with the predecessor unknown. TLCEN p11 documents 15 to 11 only and DEC-2's premise is a
   CL86-C measurement, so TL-C has no best-effort shape until a reviewed TL-C decision covers that case
   (X15), refused at two levels:
   - Owner. Every TL-C binding carries canopen_shared 02 S3's explicit per-binding no shape, which overrides
     the Lichuan default. 03a item 2 sets and checks it where the application binds a node with a declared
     TL-C model, before the binding's first traffic, beside `SdoReusePolicy::NONE`, and lands it first if
     needed; the owner keeps it across rebind. The example application's model-aware routes (binding, and
     06 item 2's declaration) set it and check it, so a direct `admitBestEffortStop` returns `INVALID` with
     no TX, also after an operation's timeout and a rebind.
   - Profile. `prepareBestEffortStop` takes the live `ModelBinding` (stage 01 item 7's migration rule) and
     refuses a TL-C binding before TX, and the example application's best-effort path does the same.

   S3's explicit no shape is a prerequisite of the first TL-C traffic, landed or consumed by 03a item 2 (or
   04a), so every operation of this stage runs on a binding that already carries it. No parallel
   transmitter; CL86-C and OL86-C keep DEC-2 and its tests unchanged. Stop completion stays `UNKNOWN`
   without TL-C stop evidence (X15) and the observation source of item 1.
4. Position. Documentary objects and ranges match the C family (TLCEN p10): `6060`=1, `607A` I32
   -1000000..1000000 command pulses, `6081` 0..3000 rpm, `6083`/`6084` 0..2000 ms. Triggers are 15 to 31, 95, 63
   and 127 (TLCEN p11); TL57OP p7's `0x011F` start is refused (LX-D08). `preparePositionParameters` admits TL-C
   models with these bounds; `prepareMoveAbsolute`, `prepareMoveRelative` and `prepareMoveAngle` need TL-C
   `MoveEvidence` (X16): acceptance and completion on the TL-C table (bit 15 position complete with a fresh
   transition after the trigger, from item 1's observation source). Relative basis and scale stay caller
   qualifications under X16 (Q04 does not transfer).
   Interruption choice, defined here once (04b item 4 uses it). After a possibly sent trigger the drive runs
   the finite move onboard, and under `NONE` a lost trigger reply leaves the route unresolved with no TL-C
   stop that can still leave (item 3), so preparation takes the caller's choice before any changing TX. If
   the caller's motion envelope requires the move to be interruptible, the caller supplies a latest
   interruption time and the move is a host-ended run under canopen_shared 02 "Timed runs" with item 5's
   stop-path rule, that time in the duration's place: the stop budget fits before the one operation
   deadline, later transfers are capped at stop due, and an application-declared independent stop with its
   fallback deadline is named, all before the trigger, otherwise MISSING_PREREQUISITE with zero TX; a move
   confirmed complete from fresh observations before that time owes no stop. Otherwise the result states
   the onboard-completion premise the caller accepted (the drive completes the move as `607A`, `6081`,
   `6083` and `6084` were written, with no host interruption) and, after a lost trigger reply, keeps motion
   and completion unknown, sends nothing further and names no usable stop path. No path replays a trigger
   or infers completion from elapsed time.
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
   names (canopen_shared 02 timed runs). That declared stop carries the caller's fallback deadline, without
   which preparation refuses the same way, and the shared rule says when it is used: the application
   invokes it when the urgent stop is not admitted at stop due, misses its dispatch window, or leaves the
   stop effect unconfirmed at the fallback deadline; admission, TX or an acknowledgement of the urgent stop
   never retire that need. TL-C stop completion stays `UNKNOWN` without X15's record and item 1's read
   acceptance (item 3), so until both exist every run that reaches stop due ends with the declared stop
   invoked by its fallback deadline. The result keeps `needsStop`, reports motion unknown and names the
   declared stop and its fallback deadline; the application records when it invoked it. The shared rule's
   validity and late-service clauses apply as written there: a fallback deadline already elapsed at
   dispatch of the first possibly starting write refuses that write, and service resuming after the
   fallback deadline with the stop still owed invokes the declared stop at once, the result reporting the
   missed deadline and unknown motion, with no late start. Meeting the deadline rests on the application's
   service cadence or on an actuation path independent of it; the library assumes neither. The
   application keeps the stop obligation (`needsStop` with its stop due time and fallback deadline)
   independently of the run's result: releasing a retained result does not retire it, and while it is
   owed a rebinding or re-declaration of that node is refused (canopen_shared 02 timed runs; 06 item 2).
6. Homing. Methods 17, 18, 24 and 29 with the EN86 descriptions (TLCEN p12..13). Method 28 (TL57OP p7, written
   with a stray byte `0x1E`) is refused. Each method needs a TL-C homing record for that method (X17);
   completion uses TL-C bit 8 (TLCEN p14) from item 1's observation source.
   DI0 origin, DI1 positive and DI2 negative limit (TLCEN p7..8) are factory claims: read the actual
   `2400:02..06` assignment (prompt 77's input facts where landed); never disable a limit to run a method.
   Fixture signals, defined here once (04b item 6 uses it). An assignment shows a function, not a wired
   switch, so each method also needs the caller's fixture signal record for this binding, model and
   fixture (from 07a's wiring record): for each signal the method uses (TLCEN p12..13: 17 the negative
   limit switch, 18 the positive limit switch, 24 and 29 the home switch; function codes 3, 2 and 1, TLCEN
   p8), the input wired to it and the polarity it was reviewed with. Before setup or trigger, one fresh
   read of `2400:01..06` and `2101` in the current node generation must assign that code to that input with
   that polarity bit and meet any starting condition the method's X17 record states; the start direction
   is the method's own (17 and 29 negative, 18 and 24 positive). A missing record, one for another
   binding, model or fixture, a signal assigned to another input or none, another polarity or an unmet
   starting condition refuses with MISSING_PREREQUISITE and zero TX.
   Interruption, defined here once (04b item 6 uses it). A home whose signal or completion never arrives
   ends only when the host interrupts it, so it is a host-ended run under canopen_shared 02 "Timed runs"
   with item 5's stop-path rule. Preparation refuses before TX without the model's read acceptance (without
   polling no completion can be established), without the caller-supplied latest interruption time that
   takes the duration's place (the caller's stop budget must fit between it and the one operation
   deadline), and without an application-declared independent stop named in the result with item 5's
   fallback deadline. A travel bound,
   from the method's record or the caller, may interrupt earlier, but only on observations no older than a
   caller-supplied age; an older or missing observation neither triggers nor extends anything, and the
   latest interruption time still applies. Every later transfer of the home has `deadlineUs` at most the
   stop due time, so the channel is free then. At the latest interruption time, or earlier on a fresh
   observation past the travel bound, or when observation is lost or expires after a possibly sent
   trigger, the home issues no further work and the application requests item 3's mode-6 HALT (bit 4
   falling from an acknowledged `001F`) as the separate urgent stop with its own deadline. Where that HALT
   cannot be admitted (no fresh predecessor, an unresolved route, no best-effort shape), only the declared
   stop remains: the result keeps `needsStop` and reports motion and reference unknown until fresh evidence
   resolves them. The declared stop's fallback deadline applies as in item 5: the application invokes it
   when the HALT is not admitted, misses its dispatch window or leaves the stop unconfirmed at that
   deadline. An early interruption while an observation upload is pending is such a case: that upload's
   `deadlineUs` is capped only at the latest interruption time, so the HALT can wait behind it. HALT
   admission, TX or acknowledgement never retire the need.
7. Alarm clear, with a TL-C source-state table (canopen_shared 02 "State transitions"). TL-C callers name
   the method; the C default and the C admission (`2100` alarm with `603F` FF05 in mode 1 or 3) are CL86-C
   bench decisions. The row comes from one fresh state read through the model-taking getter under the
   reservation (STATUS, MONITORS, ERRORS with `603F` raw), never from retained history:

   | Method | Fresh report | First word, or none | Disposition |
   | --- | --- | --- | --- |
   | `CONTROLWORD_BIT7` | Bit3 alarm set, `603F` FF05 ("excessive position deviation", TLCEN p15), mode 1, low bits 111 (p11 pairs them with 6040=15), bit14 running clear | `0x0080`, then `0x0000` only after its acknowledgement (p11 "15->128 Alarm reset"; p14 "set it back to 0") | Admitted. That FF05 is p19's position error alarm ("Can be reset") is an inference the result records |
   | `CONTROLWORD_BIT7` | Bit3 alarm set with `603F` FF01 (overcurrent, "Restart to reset", p19) | None | Refused: the documented reset is a restart |
   | `CONTROLWORD_BIT7` | Bit3 alarm set with any other `603F`, in mode 3 or 6, or with low bits other than 111 | None | GATED under X07: p11 prints 15 to 128 only in the position-mode table, and p19's overvoltage, undervoltage and memory alarms have no documented code |
   | `CONTROLWORD_BIT7` | Bit14 running set | None | Refused: a running drive needs a stop first |
   | `CONTROLWORD_BIT7` | Bit3 clear | None | Reported as no alarm; nothing sent |
   | `OBJECT_2301` | Any | None | Refused under X07: `2301` is declared by TLEDS l.1404 but absent from TLCEN p17 and TL57ZH p16 (LX-D11) |

   A `0x0080` possibly sent without an acknowledgement sends nothing further: the result reports bit 7
   possibly asserted and the alarm and drive state unknown. A `0x0000` possibly sent without one reports bit
   7 possibly still asserted. Neither word is repeated or a stop predecessor, so the next changing operation
   starts from a fresh read and its own table. Completion needs a fresh read after the second
   acknowledgement with bit3 clear (polling needs item 1's read acceptance, otherwise `UNKNOWN`); a
   persisting alarm repolls until the one deadline, and no result claims the cause is gone. Both words have
   bits 0..3 clear, which p11 pairs with low bits 000, so the clear may also release the drive (inference,
   as the C path states); the result reports the observed low bits.
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
    X15. Stage 02's disposition map records both states. Whenever prompt 57 lands after stage 02's map
    exists, before this stage or after closeout, its landing commit applies them as stage 02 item 9 says:
    GATED under X15 for both while this stage's source-state table has not landed.

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
CL86-C evidence on a TL-C binding, an attended enable with raw `607A` equal to `6064` and no basis record
(MISSING_PREREQUISITE, zero TX), a TLO attended enable with equal raw values and a valid basis record but no
actual-position record, and a TLC one whose basis record does not show encoder-reported `6064` (each
MISSING_PREREQUISITE, zero TX), and a stop admitted before every admitted motion. Enable source-state tests:
the first frame or none for every row of item 2, a report without the paired low bits during the sequence
(no further word), and an acknowledged `0x000F` followed by a fresh report other than low bits 111 (nothing
further, no stop prepared, enable state unknown). Identity tests: a
contradiction before preparation and after the stop was queued, each leaving the reviewed stop admitted (no
refusal for identity alone, its result carrying the outcome and claiming no standstill) while every other
pending changing step is retired. Best-effort tests: an unresolved route after a possibly sent controlword
and a stale binding, each refused with no TX; a direct `admitBestEffortStop` on a TL-C binding carrying the
explicit no shape, after a timeout and after a rebind (`INVALID`, zero TX); CL86-C best-effort tests
unchanged. Stop-table tests with halted and unknown-halt predecessors (first frame `010F` or `010B`, bit 8
set, the halt state and the undocumented `010B` recorded). Halt-derivation tests at stop dispatch, each on an
admissible route with a fresh low-bits-111 report: an acknowledged `000F` followed by a possibly sent
`010F`, and a declared external writer after an acknowledged bit-8-clear word; each emits `010F` for HALT
and `010B` for the quick stop and records the halt state as unknown with its reason, whatever the status
bits show. Move tests with a synthetic TL-C move record: a lost trigger reply under `NONE` without the
interruption requirement (no replay, no inferred completion, motion and completion unknown, nothing
further sent, the premise stated) and with it (the urgent stop requested at the latest interruption time
where admissible, the declared stop invoked by its fallback deadline); an interruptible move without a
latest interruption time or without a declared stop, refused at preparation with zero TX. Timed-run tests with
a synthetic TL-C velocity record: canopen_shared 02's cases (duration plus budget not fitting, lost start
reply, observation in flight near stop due, service resuming after stop due, queue-pressured stop); a run
without a declared independent stop, refused at preparation with no changing TX; and with one, a possibly
sent start followed by a timeout under `NONE` (`needsStop` kept, motion unknown, the declared stop named,
no CANopen stop TX); a declared stop without a fallback deadline (refused at preparation); a fallback
deadline already elapsed at dispatch of the first possibly starting write (that write and every later one
unsent); and an acknowledgement-only run whose result is released before stop due, then a rebinding of
that node (refused, owner settings unchanged, the obligation still serviced: the urgent stop at stop due
and the declared stop by its fallback deadline; 06 tests the re-declaration). Fallback tests
at the service level, through the example application with a recording declared-stop hook and a fake
clock, for a run and a home: the urgent stop not admitted at stop due, admitted but not dispatched in its
window under queue pressure, acknowledged with no confirming observation at the fallback deadline, and an
early homing interruption while an observation upload is pending; each asserts that the hook was invoked
by the fallback deadline, that admission, TX or acknowledgement of the urgent stop did not cancel it, and
that `needsStop` and motion unknown are kept. Service resuming only after the fallback deadline with the
stop owed invokes the hook at once, and the result reports the missed deadline and unknown motion with no
late start. Homing signal tests: correct assignment bytes with no fixture
signal record, a record for another binding, the signal on another input, another polarity bit and an
unmet starting condition, each refused before setup or trigger with zero TX. Homing interruption tests: no
read acceptance, no latest interruption time, one that leaves no room for the stop budget, or no declared
stop (refused before TX); an absent or stuck signal; an
upload in flight at the latest interruption time (capped there, the HALT not delayed by it); a service cycle
resuming after that time (no further home work, the HALT requested at once where admissible) and after the
fallback deadline (the declared stop invoked at once, the miss and unknown motion reported); a travel bound seen only in a sample older than the age limit (no interruption from it,
the latest time still applies); and observation loss or expiry after a possibly sent trigger (HALT requested
where admissible, otherwise `needsStop` kept, motion and reference unknown and the declared stop named).
Alarm-clear tests: the first frame or none for every row of item 7, a lost first and a lost second
acknowledgement (no further word, bit 7 possibly asserted), an already clear request, FF01, an unmapped
code, mode 3 and running set. Position
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
unchanged. Every TL-C operation, reviewed stops included, runs on a binding that already carries S3's
explicit no shape (03a item 2).
Quick and full verification, firmware builds and exact-commit CI pass. Hardware NOT RUN; X04, X05, X06,
X07, X14, X15, X16, X17 and X18 stay open.
