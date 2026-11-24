# Lichuan state interpretation, enable and action boundary

`MotorControlCAN/profiles/lichuan_c/State.h` provides the passive
`getStateInterpretation` getter in `MotorControlCAN::LICHUAN_C`. It interprets
already acquired STATUS and MONITORS blocks through the existing checked
`getStateBlock` path. It performs no I/O, changes no drive state and produces
no action work. A caller holding only one 6041 word, from a STATUS block or a
qualified TPDO sample, decodes it with `interpretStatusword`, and tests object
2100 with the `MONITOR_` masks beside it. Those give the same meanings, but
prove no route, freshness or firmware: a decoded word is not admission or
completion evidence by itself, and each caller must prove those facts itself. A gated enable, run
attended on both specimens on one firmware, the typed mode select and the measured release are
described below. The separate
[conditional stop lifecycle](https://github.com/janhavelka/MotorControl-CAN/blob/main/docs/stop_actions.md) uses common `ActionOperation.h`
evidence and concrete `Actions.h` contexts; it establishes neither of those. [Fault clear and counter zero](https://github.com/janhavelka/MotorControl-CAN/blob/main/docs/fault_reference_actions.md)
also use this lifecycle with their own documented writes and report criteria;
they do not supply release, home or factory-reset semantics.

The [typed-read contract](reads.md) owns acquisition, read effects, target and
epoch checks, age, observation windows and PDO provenance. The state getter
does not bypass those checks or turn an unqualified upload into a non-consuming
read. Rejection leaves its output unchanged. Successful interpretation remains
documentary interpretation of drive reports, not physical qualification or
application readiness.

## Raw observations and documentary layouts

Select `StatusLayout::UNSPECIFIED`, `EN86`, `ZH86`, `ZH57` or `OP86` explicitly.
These names identify source descriptions, not detected firmware variants.
Selecting one does not prove that it applies to the connected drive. An
unspecified layout retains all raw status bits without assigning status meanings;
monitor and positive mode meanings remain independently decoded.
Raw words, unknown bits and disagreement remain available for inspection.

| Observation | Documentary meaning | Original source |
| --- | --- | --- |
| `6041` low three bits | Patterns 0/1/3/7 are associated with the examples `6040=0/6/7/15`. Other patterns are unspecified. | EN86/ZH86/ZH57 PDF p12; OP86 p10 |
| `6041` alarm and motion | EN86: alarm bit 7, zero speed bit 10, target effective bit 12. ZH86: alarm bit 3, running bit 14. ZH57/OP86: alarm bit 7, running bit 14. | Same tables |
| `6041` completion flags | Bit 8 reports homing complete; bit 15 reports position-mode positioning complete. | Same tables |
| `6061` mode | 1 position, 3 velocity, 6 homing; 0 undefined. Other raw bytes have no established meaning here. | EN86/ZH86/ZH57 p11; OP86 p9; EDS `[6061]` |
| `2100` motion monitor | Bits 0/1 positioning/homing complete, 2 running, 3 alarm, 4 released, 5/6 positive/negative software-limit status. Bit 4 clear reports enabled. | EN86 p17; ZH86/ZH57 p18 |
| `2101`, `2102` | Retained input/output monitor words; no inferred external function, brake action or electrical measurement. | Same monitor tables; EDS `[2101]`, `[2102]` |

The low-three examples are not a complete CiA402 state machine. A pattern does
not reconstruct the last controlword or establish release, readiness, fault
reaction or quick-stop state. `6040` is WO in the EDS; actual command evidence
must come from retained owner work, not a fabricated controlword readback.
Mode display is observed, never written to implement the English translation's
instruction to "set" 6061.

Status/monitor mismatch is reported without choosing whichever value looks
ready. Acquisition of several objects is not atomic: observation skew can also
explain disagreement. Absence of mismatch does not qualify a firmware layout.
A set completion flag has no automatic association with a new operation and
does not establish standstill, winding current, brake state or reference validity.
The OP86 filename and cover name 86C, while its relevant internal pages carry a
CL57-C/OL57-C header; retain that applicability uncertainty.

## Identity policy (prompt 57)

The application's declared binding selects the profile and the model (`CL86_C` or `OL86_C`, as
`AxisReservation::model` already requires); an identity read only confirms it and never selects or
changes a declaration. Enable, the position moves, velocity and homing compare a current identity read
with their reference in `checkIdentity` (`Reads.h`) and freeze the `IdentityCheck` in their context, so
every terminal result carries it. Stops make no comparison: an explicitly requested stop is never refused
on identity.

| Fields | A difference from the reference |
| --- | --- |
| Product: `1000`, `1018:01` vendor, `1018:02` product, `2000` | Refused before any TX with `UNSUPPORTED` (outcome `UNCONFIRMED`): another product is another profile. No gate or option overrides it |
| Firmware: `1009`, `100A`, `1018:03`, `2001` | The operation runs; `IdentityCheck::firmwareUnqualified` is set ("firmware unqualified") and the result keeps both tuples, `expected` and `observed` |

The reference is the record the operation's qualification was made on: the caller's measured
`MoveEvidence`, `VelocityEvidence` or `HomeEvidence::firmware`, and for the enable `measuredIdentity()`,
the two CL86-C specimens (`1000=00040192`, `1009=0C`, `100A=20`, `1018:01..03=1000/1/0`, `2000=0400`,
`2001=0200`), on which every reviewed enable record was made. No OL86-C identity is documented or
measured, so an `OL86_C` binding is compared with the same values. A move replacement or a velocity change
compares its own record with the observed identity its run already holds. An observation of another boot,
binding or configuration, or with a window after the supplied time, is never compared and refuses the
same way.

The documented reference `documentedIdentity()` holds the EDS `DefaultValue` entries
(`1000=00040192`, `1009=2608`, `100A=0001`, `1018:01..03=1000/1/0`, `2000=0001`, `2001=0000`). It is
compared and reported (`documentedMatch`) but admits nothing on its own: its `2000` default is not what
either specimen reads, the EDS is family-unbound, and the CL86-C/OL86-C manual gives no identity values.
A product match does not prove a CL86-C either: the Lichuan DS-C and TL-C EDS files declare the same
`1000`, `1018:01` and `1018:02` (backlog D18). This profile takes no candidate set, so the declared model
is the only candidate and the outcome `CONTRADICTED` is never produced. The family promptsets
`lichuan_c_extensions` and `lichuan_ds2_canopen` extend the same comparison with their model-scoped
references and the candidate set. Mode select and release make no identity comparison yet.

## Enable: a regular measured command, and the attended trial

Since prompt 57 the mode 1 enable is a regular command under `EnableGate::MEASURED`, opened by the
reviewed record of the retained enables: 111 succeeded on both CL86-C specimens, one firmware
(`1000=00040192`, `1009=000c`, `100A=0020`), 28 of the 30 attended records 103 to 2402 and 83 night-soak
runs; records 103 and 105 (2026-11-03, node 1) failed before the bit 10 fix and after a hand
displacement. Its start is the fresh report alone, which the library classifies into
`EnableRecord::start`:

| Start | Fresh report | Measured basis |
| --- | --- | --- |
| `POWER_UP` | Mode 1, `6041` low bits 111, `2100` in place with released, running, alarm, both soft limits and every undocumented bit clear | 21 power-up records: `8437`/`0001`, word 6 `8031`/`0011`, word 7 `8033`/`0001`, word 15 `8437`/`0001`. Both shafts held torque there by hand and word 6 released it, so no shaft observation is asked |
| `RELEASED` | Mode 1, `2100` released (in place and the running latch either way, the other bits clear), `6041` bits 0..6 `0x50` or `0x31` | The released standstill a quick stop leaves (`8050`/`0011`, latches `4050`/`0014`, `C050`/`0015`); `8031`/`0011` is the report word 6 leaves on that path, which the earlier `0x50` rule refused |

Both gates also take a current identity read under the identity policy above:
product fields that differ from `measuredIdentity()` refuse, firmware fields that differ run the enable
flagged firmware unqualified in `EnableRecord::identity`.
`1001`, `603F` and `606C` must read zero. No trial identity, expiry, reservation copy, shaft observation,
reviewed value, retained stop record or per-boot word history is consulted: `AxisSession` needs only a
report taken after its own latest write; the console's `enable AXIS_ID` runs it on the report and identity the
application names (`ProbeMotion::setActionPrerequisites`). `607A` and `60FF` are still read once before any word and
kept, not compared: no mode 1 enable of the record started motion from a retained target. A caller may
name `RELEASED` or `POWER_UP` to require that class. Any other report, such as the switched-on
`8033`/`0001` of backlog S21, and mode 6 stay with `EnableGate::ATTENDED_EXPERIMENT`, which keeps every
statement below. `EnableStart::RELEASED_BY_RECORDED_STOP` is now a deprecated alias of `RELEASED`: the
recorded-stop premise of 2026-11-10 is gone. Every observation rule of the sequence below applies to
both gates.

`prepareEnable` in `Actions.h` implements the documented position-mode sequence through the
ordinary `nextAction` and `advanceAction` lifecycle, and `AxisSession::requestEnable` runs it
through the sole owner. **It succeeded on both bench specimens on 2026-11-03**, after a fix that
session found (bit 10, below), on one firmware, and again on 2026-11-04 with the same reports, twice
on node 1 and once on node 2 ([report](https://github.com/janhavelka/MotorControl-CAN/blob/main/docs/reports/2026-11-04_bench.md)), and on 2026-11-05 once on each
node (trials 704 and 712, [report](https://github.com/janhavelka/MotorControl-CAN/blob/main/docs/reports/2026-11-05_motion_and_stress.md)), seven times in all. The
power-up shaft entries of 704 and 712 were the lead's reading of the drive's start report, not an
observation (see the NOTE.txt in each trial directory). That is not a qualification: the
measured paragraphs after the transition table and the
[attended report](https://github.com/janhavelka/MotorControl-CAN/blob/main/docs/reports/2026-11-03_attended_bench.md) say what was and was not observed.
`EnableEvidence::gate` defaults to `CLOSED`, which returns `UNSUPPORTED`. Opening it to
`ATTENDED_EXPERIMENT` states an attended trial with an independent drive-power cutoff. It is not a
readiness claim, and it does not qualify the capability. The evidence is also bound to one trial:
a nonzero `trial` identity, the exact `reservation` and so the exact binding, the exact `operation`
the enable runs under, and an exclusive `expiresUs`. `AxisSession` consumes each trial identity
once, in rising order, for the session object's life. Owner operations only rise on a reservation,
so a copy cannot open a second enable there, through any session, and a restarted MCU's new
reservation does not match it. A restart that reproduced the same reservation, operation and clock
values could match. Only the application's trial log, which must never reissue an identity, closes
that case.

### Documented claims

Pages are one-based PDF pages. OP86 pages 9 to 11 carry a CL57-C/OL57-C header under an 86C cover,
so they are not independent 86C evidence.

| Claim | Source |
| --- | --- |
| Position mode: set `6060=1`, watch `6061` become 1, write 6, 7, 15 "about 10 ms" apart, after which the motor can be enabled. ZH86 p12 verbatim: "往控制字中依次写入 6、7、15，中间间隔 10ms 左右，写入完成后，电机即可使能". Then write target and ramps, then start with bits 4 to 6 | EN86 and ZH86 p12, OP86 p9 |
| EN86 tells the reader to "set" `6061`. ZH86 says to monitor it, and the p11 tables list it RO | EN86 and ZH86 p11 and p12 |
| Table row `6->7->15`: ZH86 and OP86 label it enable (使能). EN86 labels it "Yes" | ZH86 p12, OP86 p10, EN86 p12 |
| Bit 4 rising starts a new target. Bit 5 updates immediately or after the curve. Bit 6 selects relative | EN86 and ZH86 p12, OP86 p10 |
| Low three bits of `6041` are shown with `6040` values: 0/000, 6/001, 7/011, 15/111. ZH86 p12 verbatim: "6040=6 时，6041 的对应位为 001". This is a correspondence table. No source documents a report after a write, or how long it takes. The action's 1, 3, 7 expectation applies the table; it is not a documented report | EN86 and ZH86 p12, OP86 p10 |
| Alarm: bit 7 in EN86 and OP86, bit 3 in ZH86. Motion: EN86 bit 10 zero-speed and bit 12 target effective; ZH86 and OP86 bit 14 running. All show bit 8 homed and bit 15 positioned | Same tables |
| `2100`: bit 0 in place, 1 homed, 2 running, 3 alarm, 4 released (0 enabled), 5 and 6 soft limits | EN86 p17, ZH86 p18 |
| Velocity mode, ZH86 and OP86: set parameters, then 6, 7, 15, after which the motor is enabled **and starts running** | ZH86 p13, OP86 p11 |
| Velocity mode, EN86: 6, 7, 15, then parameters, then bits 4 to 6. This conflicts with the above | EN86 p13 |
| RPDO1 defaults to `6040`, `60FF` and `6060` in one frame on `0x200+node`, transmission type 255 | EDS `[1400sub1]`, `[1400sub2]`, `[1600sub0..3]` |
| `6040` EDS WO U16 default 0, manuals RW. `6041` RO U16 default 0. `6060` EDS WO U8 default 0, manuals RW I8. `6061` EDS RO U8 default 0, ZH86 p17 lists RW I8. `2100` EDS RW U16 default 8, manuals RO. `607A` I32 default 5000, `60FF` I32 default 120 | EDS `[6040]`, `[6041]`, `[6060]`, `[6061]`, `[2100]`, `[607A]`, `[60FF]` |

No source says what an earlier target does when a drive enables in mode 1, how long a word takes to
show in `6041`, what tolerance "about 10 ms" has, what happens on a late or missing word, or whether
torque follows any report. Silence is not a statement that nothing happens.

### Measured firmware behavior

Two CL86-C specimens, node 1 and 2, in the retained November 2 reads: `6041=0x8037`, `6061=0`,
`2100=1`, `1001=0`, `603F=0`, `6064=0`, `606C=0`, `607A=5000`, `60FF=120`. No retained record shows a
write to `6040`, `6060`, `6061`, `607A` or `60FF`, or a read before the power cycle. `607A`, `60FF`
and `6061` equal their EDS defaults. `6041` and `2100` do not: their EDS defaults are 0 and 8.

This changes how a report can be read:

- A specimen reads low bits `111` and `2100` released clear with **no enable in any record**. So the
  final report alone cannot distinguish this sequence from the power-up state. Only the progression
  to 1, then 3, then 7 could. Both specimens followed it in every enable of 2026-11-03 and 2026-11-04
  after the bit 10 fix (below).
- Under EN86, `0x8037` has zero-speed clear, which reads as running. Under ZH86 it reads as stopped.
  Decoding cannot decide this word.
- Mode 0 is not an operating mode here, so a specimen needs an explicit `6060=1` before an enable.
  In the November 2 records that write had never been sent and nothing had been enabled; the
  measured paragraphs below give what the later sessions saw.
- Low bits `111` with `2100` released clear is the pattern p12 pairs with an enabled drive, and both
  specimens show it at power-up. Word 6 may therefore be a 15-to-6 step that **removes holding
  torque**, not the first step towards it. On 2026-11-03 it was: both shafts held torque at power-up,
  word 6 set `2100` released on both, and node 1's shaft then turned freely (below).

### Project policy

Each of these is this project's decision, not a vendor statement.

- **Mode 1 only.** The ZH86 p13 sentence is why: in mode 3 the same words are documented to start the
  motor, and `60FF` reads nonzero on both specimens. EN86 disagrees. Mode 3 has no approved path.
- **A target of unknown origin fails closed in an attended trial.** `607A` and `60FF` readings are
  history, not intent. The action reads each once before any word and, under `ATTENDED_EXPERIMENT`,
  fails with `OBSERVATION_SCOPE` unless both exact values equal `EnableEvidence`'s reviewed values;
  under `MEASURED` it keeps them without comparing. Zero is not exempt: an absolute target of zero
  does not say no motion, and `60FF=0` has no documented meaning. A match says only that someone
  looked at those bits. It does not call them safe. The action never writes or zeroes a target.
- **`2100` decides alarm and running**, because its two manuals agree. Disputed `6041` bits decide
  nothing and are never merged into a union mask. They must equal the reviewed word at admission
  and must not change during the sequence. A change ends it with both words retained.
- **`2100` released is allowed either way at admission**, because specimens read it clear before any
  enable. In-place is required as one necessary condition: a curve left unfinished might show it
  clear. A set flag does not prove the absence of a pending curve.
- **Start history in an attended trial.** `EnableStart::NO_MOTION_WRITE_RECORDED` is the application's
  statement about the time before this session existed. The library cannot check it. `AxisSession`
  adds what it can see and refuses an attended trial after it has itself admitted any write other than
  an acknowledged documented mode select (`6060` = 1, 3 or 6), for the rest of that object's life,
  across close and open. A stop admitted on any path counts. A measured enable consults none of this.
- **A released start** (`EnableStart::RELEASED`, 2026-11-10; the recorded-stop premise went with prompt
  57). With the drives kept powered, the start is the released standstill the fresh report shows; no
  record of the stop that left it is asked. It admits mode 1 only, under either open gate, from a
  released standstill: `2100` released, alarm, both soft limits and every undocumented bit clear, the
  in-place bit set or clear and `2100` running set or clear; `6041` bits 0..6 `0x50`, the pattern of every
  measured release (low bits `000`), or `0x31`, the report word 6 leaves (`8031`/`0011`), bits 14 and 15 as
  reported;
  `1001`, `603F` and `606C` zero. Running set beside the release is the latch a quick stop during a move
  leaves: node 1 read `2100=0014`, `6041=4050`, -2017 after the quick stop of move 2108 (trial 2109, the
  target -3522 not reached), node 2 `0015`, `C050` in mode 3 after a halted-state quick stop (trial 1924).
  `6041` bit 14 read set exactly while the latch was. The sequence is the same, with these differences,
  each measured from released starts on one firmware or chosen for them here:
  - `6041` bits 5 and 6 may change: from every release so far word 6 moved bits 0..6 from `0x50` to
    `0x31` (trial 1782 in mode 3, `8050` to `8031`; trial 308 in mode 1, `4050` to `4031`).
  - The latch, `2100` running and `6041` bit 14, may stay or clear in word 6's cycles and must read clear
    in every cycle from word 7 on. Word 7 cleared it in trial 308 (`4031`/`0014` to `0033`/`0000`, mode 1)
    and trial 1742 (`C031`/`0015` to `8033`/`0001`, mode 3). A latch still reported then ends the sequence
    in word 7's cycle, word 15 unsent, with that cycle kept in the context's observations. The drive is
    then switched on and may hold torque; no word 15 reached it, and no record has a switched-on drive
    move (trial 308 stayed switched on). No stop has a sequenced arrow from word 7: a stop request on a
    fresh running report takes the DEC-2 `000B` only when the policy allows a best effort, and is refused
    on a fresh idle one. No other `2100` bit, the in-place bit included, may change.
  - `6064` must repeat the admission value in word 6's cycles, released. Word 7 energizes the drive and its
    position report moves to the drive's internal command (mode 3: -6 to +12 counts in trials 1742 to
    1862; mode 1: 1187 to 1194 to 1180 over half a second in trial 308, +1 in trial 2102). No bound on that
    move is known, so word 7's cycles are not compared. Word 15 waits instead until two cycles in a row
    after word 7 report low bits `011` and the same `6064`; the second is word 7's accepted cycle. Trial
    2102 sent word 15 after one such cycle (15257), and its report moved on to 15260 in word 15's cycles;
    which word moved it is unknown.
  - From word 15 on every cycle must repeat word 7's accepted `6064` with `606C` zero. The drive keeps its
    target, an interrupted move's included (after trial 2108 `607A` read -2000, the relative displacement
    from -1522, so that move's target was -3522), and whether words 6, 7 and 15 resume it is unmeasured;
    a `000F` rearm after a HALT did not (trials 829 and 2107). A change ends the sequence with `OBSERVATION_SCOPE` and a status message that says so; the drive
    may then be enabled and moving, so the caller requests a `QUICK_STOP` at once. After an acknowledged
    word 15 that stop follows the session's `000F` (the library's mode 1 arrow: a fresh report with low
    bits `111`, released and alarm clear, running either way).

  The record keeps `6064` before word 6 and in each word's accepted cycle, success claims no position
  continuity across word 7, and a later move takes its start position from a fresh read. The shaft
  observation is not consulted for a released start; the bench records `FREE` from the released report at
  the user's instruction of 2026-11-10 ("do not ask if holding or not, read it"), never felt.
  `NO_MOTION_WRITE_RECORDED` keeps every rule it had. Under `ATTENDED_EXPERIMENT` the session still
  refuses after any write of its own; under `MEASURED` it does not.
- **Each word waits for its report.** An acknowledgement and an elapsed interval establish nothing,
  so the next word needs the previous word's documented low bits in a fresh cycle.
- **Spacing is at least 10 ms from the acknowledgement.** `ENABLE_WORD_INTERVAL_US` is the admitted
  minimum, because no source gives a tolerance. It is honored, never read as settling. Actual gaps can be
  longer, and `commandHistory` keeps them. A deadline too short for two intervals refuses before
  any word.
- **Any raw feedback movement ends the sequence.** `6064` must equal its admission value and `606C`
  must be zero. There is no tolerance, because none is sourced, so a closed-loop drive that settles a
  few counts on enable will fail here. That result would be useful evidence, not a defect to hide.
- **Fault state.** `1001` and `603F` must read zero, so an old `603F` left by an earlier alarm refuses too.
- **An enabled-looking start needs an attendant in an attended trial.** Under `ATTENDED_EXPERIMENT` a
  start report of low bits `111` with `2100` released clear is refused unless
  `EnableEvidence::powerUpShaft` records the attendant's own observation at power-up, `FREE` or
  `HOLDING`. The library cannot check it. Under `MEASURED` that report is the measured `POWER_UP` start.
- **Deadline.** Admission needs more than three intervals plus `ENABLE_SERVICE_STEPS` (26)
  queue-and-response budgets: two uploads, three words and one seven-field cycle per word. That
  guarantees only that a drive answering every service within its budgets, with each report in
  the first cycle, can finish. It does not guarantee completion. A later report, a fault, a lost
  reply or a withdrawal can still end the sequence after word 6 or 7, and the drive is then in a
  partial state with those words' effects unknown.
- **Withdrawal wins.** A cancel, a routed fault or a stop request during an enable ends it. With a
  request in flight, `noteActionWithdrawal` records the reason first. That request's terminal is
  then kept as evidence in `commandHistory` or the observation evidence, but it cannot complete or
  advance the action, and the outcome is the withdrawal's reason.
- **Target reads are an accepted action prerequisite.** The ledger records one upload each of `607A`
  and `60FF` per enable as this project's risk acceptance (`read_disposition`
  `accepted_action_prerequisite`, `semantic_review.action_read_acceptance`), classified
  `ACCEPTED_UNQUALIFIED` in code. It is not a poll, a typed read or evidence. Their effects stay
  unknown, and Q20 stays open.
- **Outside the session's sight.** RPDO1 defaults to `6040`, `60FF` and `6060` in one frame on
  `0x200+node`. A frame there from any device would write all three, and no session would see it.
  Only the absence of other CAN masters on the bench rules this out. The owner sends no RPDO.

### Transition table

Common to every row: one continuous owner reservation, an explicit CL86-C or OL86-C model, the
immutable overall deadline, and no zeroing, replay, fault clear, mode change, stop or NMT from this
action. `6040` is a whole-word write, never a read-modify-write.

| Step | Starting state and provenance | Mode and target history | Exact command | Service result | Required fresh observation | Deadline | Failure handling | Torque and reference |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| Admit | Complete SDO read of ERRORS, MONITORS, STATUS, FEEDBACK under this reservation and within `maximumAgeUs`: `2100` in place, with running, alarm, soft limits and unknown bits clear; `6041` equal to the reviewed word; `1001`, `603F`, `606C` zero. Gate open, start recorded, session clean | Reported mode 1 in that read. Modes 0, 3, 6 and any other byte refuse | None | None | The read above | Must leave more than two intervals | Refused atomically. No output changes, no traffic | No claim |
| Targets | As admitted | Reads, in order, each once. Their effect stays unknown: neither object is on the DEC-1 accepted list | `40 7A 60 00`, then `40 FF 60 00` | Each I32 upload must succeed | Both values equal the reviewed values | Overall | Mismatch: `OBSERVATION_SCOPE`. Abort: `PROTOCOL_REJECTED`. Lost reply: `SERVICE_TIMEOUT`, uncertain. **No `6040` word was sent** | None. Targets stay as they were |
| Word 6 | Targets matched | Unchanged | `2B 40 60 00 06 00 00 00` | Acknowledged; abort is `REJECTED` and a lost reply `UNKNOWN`, never replayed | New MONITORS, STATUS, FEEDBACK starting after the acknowledgement: low bits 001 by the p12 table, mode 1, every bit but released and `6041` bit 10 unchanged, `6064` unchanged, `606C` zero. Poll at `observationIntervalUs` | Overall; next word no earlier than the acknowledgement plus the interval | Out of scope: `OBSERVATION_SCOPE`, word retained, no later word. Low bits never arriving: `DEADLINE` | **Possible torque loss.** From the specimens' enabled-looking start this may be 15 to 6, dropping holding torque. Low bits 1 is a table example, not a torque state |
| Word 7 | Word 6 observed | Unchanged | `2B 40 60 00 07 00 00 00` | As above | As above with low bits 3 | As above | As above, prefix retained | Measured, one firmware: `2100` released clears. In mode 1, `6064` returned toward the last commanded position after a displacement (below; whether the shaft moved is not shown), which this row's `6064` rule then ends as out of scope. In mode 3 the same step is a counter switch with `606C` 0 ([enable-fault report](https://github.com/janhavelka/MotorControl-CAN/blob/main/docs/reports/2026-11-13_enable_fault_state.md#the-two-counters-m-with-i-where-marked)) |
| Word 15 | Word 7 observed | Unchanged | `2B 40 60 00 0F 00 00 00` | As above. On acknowledgement it becomes the latest controlword | As above with low bits 7 and `2100` released clear | Overall | As above. `SUCCEEDED` needs all of it | **A drive report, not torque.** Mode-1 motion is documented only from a later bit 4 edge, and whether an earlier curve resumes is unmeasured. Reference validity is not claimed either way |
| After success | Word 15 observed | Target unchanged by this action | None | None | A later move or stop needs its own fresh state. The action's last read qualifies a stop | None | A second request refuses: no re-enable on any evidence | Unknown |
| Enable in mode 3 | Any | Mode 3, with whatever `60FF` reads | None | None | The mode byte | None | Refused at admission | ZH86 p13 and OP86 p11: the motor starts. EN86 p13 disagrees |
| Enable in mode 0 | Any | Undefined mode | None | None | The mode byte | None | Refused | None |
| Enable in mode 6 | Non-enabled released, stationary and exact reviewed evidence | `modeRaw=6`, `homingEnableQualified`, ordinary explicit trial evidence | 6/7/15 with spacing | Fresh mode6 state after each ACK | No automatic home trigger | Retained action outcome | Conditional; see [homing](https://github.com/janhavelka/MotorControl-CAN/blob/main/docs/homing.md) | No reference published |
| Enable from `000F` or after a stop | Latest word `000F`, `000B` or `010F` in this session | Any | None | None | None | None | Refused, and never writing 6 merely to "reset" | A word from 15 back to 6 has no documented meaning here |
| Enable from a released standstill | Fresh released standstill in mode 1 (`6041` bits 0..6 `0x50` or `0x31`), in place either way, the running latch of a moving quick stop admitted (`0014`/`4050`, trial 2109); no stop record. An attended trial also needs a clean session | Mode 1; reviewed targets in an attended trial, kept and not compared under `MEASURED`; an interrupted move's `607A` retained | 6, 7, 15 as above; word 15 only after two still cycles of word 7's report | As above | As above, except `6041` bits 5 and 6; the latch (`2100` running, `6041` bit 14) may stay only in word 6's cycles; `6064` repeats the start in word 6's cycles, is free in word 7's, and must repeat word 7's accepted value from word 15 on | As above | As above. A change after word 15 ends it with a message to stop the drive: the caller requests a `QUICK_STOP` at once | `MEASURED` or an attended trial. Word 7 moves the position report to the drive's internal command, with no bound known; no continuity is claimed. Whether word 15 resumes a retained target is unmeasured |
| Stop during the sequence | Acknowledged 6 or 7 | Unchanged | A fresh idle report: no word, since DEC-8 has no arrow from them. A fresh running report, or none: best-effort `000B`, as for the same word still in flight | `000B` path: [stop guide](https://github.com/janhavelka/MotorControl-CAN/blob/main/docs/stop_actions.md) | The stop's own | The stop's own | The enable ends either way. Use the independent cutoff | Unknown |
| Stop after `000F` | Acknowledged 15 | Unchanged | `010F` or `000B` with a fresh cycle after the acknowledgement, otherwise best-effort `000B` | [Stop guide](https://github.com/janhavelka/MotorControl-CAN/blob/main/docs/stop_actions.md) | [Stop guide](https://github.com/janhavelka/MotorControl-CAN/blob/main/docs/stop_actions.md) | Stop's own | The enable ends, and any word in flight settles into it | Unknown |

**Measured on node 1, 2026-11-03.** The shaft held torque at power-up, before any word from this
library, with `6041=8037` in mode 0 and `8437` after `6060=1`. Word 6 was acknowledged and the fresh
read showed `6041=8031` and `2100=0011`: low bits 001 as the table says, released set, and bit 10
cleared. The shaft then turned freely. So word 6 from this power-up state **drops holding torque**,
and bit 10 (EN86's zero-speed bit) follows the enable state. The library used to compare every bit
above 0..2, so it ended that enable at word 6 with `OBSERVATION_SCOPE` and sent nothing further. It
now ignores bit 10 there (`STATUS_EN86_ZERO_SPEED`), because `606C` already decides velocity.
`606C` reads rpm on this configuration: 30 during moves at 30 rpm and 1 at 2 rpm, on both nodes
(2026-11-04).

**Word 7 returns a displaced position report, drive-reported on node 2, 2026-11-04** (trial 308, the
bench `word67` route, not this action). Reported released after a quick stop during a move, the
drive read `6064=1187`, about 10 pulses past the stop's last move sample. Word 6 gave `6041=4031`, still released, at 1187. Word 7 gave `6041=0033` and
`2100=0000`: energized, and `6064` read 1194 twice, then 1180 for six cycles, near where the
trajectory was when `000B` arrived. The hypothesis that word 7 restores the drive's last commanded
position, neither resetting the counter nor adopting the actual position, is consistent with this
(settled at 1180) and with node 1's trial 603 (settled at 1560 after a quick stop at 1558); it is not
shown. It would also explain trial 105 of 2026-11-03, `6064` from −545 to 0 at word 7 after a hand
check, a pull-back of about half a turn.
**Never move a released shaft by hand before an enable, and expect motion at word 7 whenever the
shaft was displaced.** One specimen, a 7-pulse displacement, judged from drive reports; the shaft
was not observed ([report](https://github.com/janhavelka/MotorControl-CAN/blob/main/docs/reports/2026-11-04_bench.md)). The enable's position rule, `6064` unchanged
through the sequence, ends such an enable at word 7, as in trial 105, but only after the motion.

Cancellation, a deadline, a reboot, bus-off, a changed epoch or an independently routed
`DRIVE_FAULT` ends the sequence locally at any step. None is a stop. `execution`, `effectsPossible`,
`uncertain` and `commandHistory` keep every prefix word, and nothing follows or is replayed. The
inclusive overall deadline wins at equality, including for a terminal received on time and processed
late. The owner gives `AxisSession` no EMCY view, so an application that routes a fault calls
`cancelEnable` with `DRIVE_FAULT`, and passes its unhandled-fault fact to `preflight` so that an
enable word queued in the EMCY's own turn is refused unsent. A withdrawn enable's queued word is
refused at handover. Either way `faultBlocked()` is set: later enables, moves and counter zeros
are refused for the session object's life, while a stop and an alarm clear stay admissible.

The [stop guide](https://github.com/janhavelka/MotorControl-CAN/blob/main/docs/stop_actions.md) explains why a stop needs the session's own latest word. The
session records each enable word through its one admission path, so an acknowledged `000F` becomes
that word, and the first of those admissions withdraws any older one. Settings and a later move keep
their own predicates on an acknowledged `000F`. Each still needs its own fresh report, and an
enable's success does not satisfy them by itself.

## Mode select: the typed `6060` write (prompt 57)

`prepareModeSelect` in `Actions.h` (`ActionKind::SELECT_MODE`) writes one documented operating mode
and settles only on the drive's own mode report. It runs through the ordinary `nextAction`,
`noteActionTransmission` and `advanceAction` lifecycle; `AxisSession::requestModeSelect` runs it
through the sole owner in the session's explicit-action slot, which it shares with the alarm and
counter clears. The console reaches it as `mode AXIS_ID 1|3|6` (`ProbeMotion::requestModeSelect`, on
the report the application names); the bench as the `mode1` and `mode3` routes.

| Item | Rule | Basis |
| --- | --- | --- |
| Codes | `1` position, `3` velocity, `6` homing only. Any other byte, `0` included, is `UNSUPPORTED` before any other check; undocumented codes stay in the D17 probe (`MODE_PROBE`) | EN86 and ZH86 p11 |
| Wire | One download `2F 60 60 00 <mode> 00 00 00`; the manuals' INTEGER8 and the EDS U8 give the same byte | EN86/ZH86 p11, EDS `[6060]` |
| Never | A `6060` upload (write-only in the EDS, D04; the manual tables list RW), a `6061` download (the drive's report; EN86's "set 6061" is a mistranslation), any `6040` word, an enable, a resend | EDS `LC_StepDriverCAN_V1.1.eds`, ZH86 p12-14 |
| Admission | A fresh complete SDO read of `ERRORS`, `MONITORS`, `STATUS`, `FEEDBACK` under the reservation: `1001`, `603F` and `606C` zero, `2100` alarm clear, and either the drive reporting itself not enabled (`2100` bit 4 released with `6041` low bits 000 or 001; running set only beside the release as a moving quick stop's latch) or the measured power-up report (`6041=8037`, `2100=0001`, `6061=0`) to mode 1 only, because all 23 measured writes from it were `6060=1` | Lead decision 1 of [report 57](https://github.com/janhavelka/MotorControl-CAN/blob/main/docs/reports/prompt_57.md) |
| Already selected | A not-enabled report whose `6061` already reads the request is answered at preparation: `SUCCEEDED`, `OBSERVED`, execution `NOT_TRANSMITTED`, `modeSelect.alreadySelected`; nothing is written or sent, as the release table's already-released row answers | Lead decision, 2026-11-24 |
| Refused before TX | An enabled drive (low bits 111 with `2100` released clear, except the power-up report): no source documents a mode change while enabled (Q11). Also the switched-on reports `8033`/`0001` and `8010`/`0001`, from which no write was measured | Q11 |
| Settlement | MONITORS, STATUS and FEEDBACK every `observationIntervalUs` after the acknowledgement until a cycle reports `6061` equal to the request. Each cycle must keep `6061` at the start or requested mode, `2100` alarm clear, `2100` released and the class of `6041` bits 0..2 as admitted, running only where admitted, `606C` zero and `6064` at the admission value; other `6041` bits decide nothing (bit 10 went `8037` to `8437` at power-up). Anything else ends `OBSERVATION_SCOPE` | Project policy over the measured reports |
| Result | `execution` keeps the `6060` acknowledgement, `completion` the `6061` report. A lost reply stays `UNKNOWN` and uncertain, an abort `REJECTED`, and nothing is resent. `retryClass` is `NOT_REPEATABLE` | `ActionOperation.h` |

Measured: about 130 writes on two CL86-C specimens, one firmware (`1000=00040192`, `1009=000c`,
`100A=0020`), all acknowledged and read back: 23 from the power-up report (packet 15 P2, records 102
to 1722), and 1 to 3 and 3 to 1 from released reports (trials 917, 2101 and 2221, and the night
soaks). No mode 6 write and no write from another start mode has run; a success is the drive's mode
report, not a qualification of that mode's motion. The bench routes `MODE_ONE` and `MODE_THREE` use
this operation since prompt 57, judged on their fresh reads alone: the earlier per-boot rule, which
refused a mode change after any controlword or write of the boot, is gone.

## Release: the measured release table (prompt 57)

Reached from the console as `motor-release AXIS_ID` (`ProbeMotion::requestRelease`, on the report the
application names) and from the bench as the `release` route (`Tr`, with the experiment option); `release
ID` stays the console's retained-result command and sends no frame.

`prepareRelease` in `Actions.h` (`ActionKind::RELEASE`, the MotorControl-RS name) frees the shaft by the
release table: one `6040` word chosen by the fresh report, settled only by the drive's own released
report, never by TX. `classifyRelease` is the table itself, a pure function that prompt 72's guard
calls too; `AxisSession::requestRelease` runs it through the sole owner in the explicit-action slot. No
manual names a release word: EN86/ZH86 p12 and OP86 p10 only pair `6040=0` with `6041` low bits 000, and
OP86 p10 carries a CL57-C header. So every row is measured, not documented, and keeps its scope: two
CL86-C specimens on one firmware (`1000=00040192`, `1009=000c`, `100A=0020`).

| Row | Fresh report | Word | Settled by | Evidence and scope |
| --- | --- | --- | --- | --- |
| 1 `POWER_UP_IDLE` | Mode 1, `6041` low bits 111, `2100=0001`, `606C`, `1001`, `603F` zero: `8437`/`0001` | `0x0006` | Low bits 001, `2100` released (`8031`/`0011`) | The first word of 21 power-up enables, both specimens (node 1 trials 108 to 1703, node 2 trials 118 to 1723, `docs/bench/*-p3-node*`); node 1's shaft then turned freely by hand (2026-11-03). The same report after this library's word 15 cannot be told apart |
| 2 `SWITCHED_ON_OPEN_LOOP` | Mode 3, low bits 011, `2100=0001`, `606C`, `1001`, `603F` zero, and a fresh `2500=1`: `8033`/`0001` | `0x0000` | Low bits 000, `2100` released (`8050`/`0011`) | Once: node 2, trial 5991 (`docs/bench/20261113-ladder/night.log:8763-8800`, `packed/S0193-n2-disable.tar.xz`), about 15 minutes after trial 5976's refused enable. Trial 5991 read no `2500`: its open loop is inferred from history (trial 5973), so the library requires a fresh `2500=1`. That read gave `606C` 1711, inferred not a rotor speed |
| 3 `ALARM_CLEARED` | Mode 3, low bits 000, `2100=0001`, `606C`, `1001`, `603F` zero: `8010`/`0001` | `0x0080` alone | Low bits 000, `2100` released (`8050`/`0011`) | Node 1: `packed/20261113T141445Z-cw-0080-n1-5405` after `...141347Z-cw-0000-n1-5402`, `...141402Z-cw-000b-n1-5403` and `...141417Z-cw-0006-n1-5404` changed nothing; then 8 of 8 (trials 5362, 5396, 5452, 5500, 5524 on node 1; 5632, 5650, 5668 on node 2). The fault-reset bit (ZH86 p12, OP86 p10, EN86/ZH86 p15), the measured exit from this report only |
| 4 `ALREADY_RELEASED` | `2100` released with low bits 000 or 001 (`8050`/`0011`, `4050`/`0014`, `C050`/`0015`, `8031`/`0011`), any mode, whatever its running bit | None | At preparation: `SUCCEEDED`, `NOT_TRANSMITTED` | Already released; the running bit, a nonzero `606C`, the `2100` alarm bit and a nonzero `1001` or `603F` are reported, and nothing is cleared (lead decision 2 and the b3 review) |
| 5 `REFUSED` | Everything else | None | Refused before TX | `ReleaseRefusal` names the reason: `ALARM` (the alarm clear's case), `RUNNING` (a stop's case), `RELEASED` beside other low bits, `MOVING` (`606C` nonzero, running clear, S21), `MODE` (a row's report in another mode, such as the halted `9437`/`0001`), `LOOP` (row 2 without a fresh `2500=1`), `PATTERN` (such as S20's `1437`/`0000` or `0037`/`0000`) |

A release settles when a fresh cycle after the acknowledgement reports the row's low bits with `2100`
released; `606C` is recorded with it (`ReleaseRecord::settledVelocity`) and a nonzero value is reported,
as in trial 5991. Each cycle must keep the admission's mode, `2100` alarm clear and running set only
where the admission read it; anything else ends `OBSERVATION_SCOPE`. A release never claims standstill,
holding torque or a reference. A refused or unacknowledged word is not repeated; a lost reply stays
`UNKNOWN` and uncertain, and a new attempt is a new operation from a new fresh read. The acknowledged word
becomes the session's latest controlword, which no stop follows.

The experiment option (`ReleaseExperiment`, off by default) admits `0x0000` or `0x0006` only: never
`0x0080` outside row 3, never a word with bits 0..2 all set. It sends from a report outside the measured
rows whose refusal is `MOVING`, `MODE`, `LOOP` or `PATTERN`, or from a row 1 to 3 report with another
word than its row's (word 6 from `8033`), and marks the result `EXPERIMENT`; its settlement is the p12
table's low bits for that word (000 for 0, 001 for 6) with `2100` released. It never overrides an alarm,
`2100` running or an already released report, and only a reviewed measured result adds a row. The release
bench trials of the prompt run this way; they are not run yet.

## Bounded physical plan

For the lead, on an attended bench. P1 to P3 ran on both specimens in the
[attended session](https://github.com/janhavelka/MotorControl-CAN/blob/main/docs/reports/2026-11-03_attended_bench.md). P4's `000B` ran on node 2 in the
[unattended session](https://github.com/janhavelka/MotorControl-CAN/blob/main/docs/reports/2026-11-03_unattended_bench.md), from a library move's rearm `000F`
rather than the enable's own, and released the motor. On 2026-11-04 P4's `010F` from the enable's own
`000F` ran on node 1 and left the drive enabled and in place ([report](https://github.com/janhavelka/MotorControl-CAN/blob/main/docs/reports/2026-11-04_bench.md)). Default to node 1 alone, and add node 2 only
after node 1 passes. Before each state-changing trial, record the operator, cutoff and clear shafts,
and stop on any abort below. Never retry within a trial.

| Trial | Exact typed sequence | Decides | Abort on |
| --- | --- | --- | --- |
| P1, refresh (step 1). No attendant needed, no writes | Packet-11 image, then reads of `6041`, `6061`, `2100`, `2101`, `2102`, `6064`, `606C` on the DEC-1 accepted list; `1001` and `603F` as unqualified single reads, never polled, with the fault LEDs recorded before and after; `6081`, `6083`, `6084` through the typed motion-parameter read, which is unqualified; `1017` as an unqualified single read; and `607A`, `60FF` once each under a one-use experiment grant, because their ledger acceptance covers only the enable's own reads | Whether the November values still hold, and the exact words to review | Any abort, timeout, boot-up, bus fault or changed value: stop and report |
| P2, mode (step 3). **Attendant required.** One isolated write, shaft watched | Full read of P1. Then `2F 60 60 00 01 00 00 00`. When this plan ran, no typed write or firmware route built it, and the only path was a caller-built transfer through `AxisSession::admitPrepared`; since prompt 57 `prepareModeSelect` builds it (Mode select above). Then fresh `6061`, `6041`, `2100`, `2101`, `2102`, `1001`, `603F`, `6064`, `606C`, repeated for a window chosen beforehand, while the attendant watches a mark on the shaft. Only once the reads stay quiet, and only if the attendant judges it safe, they check whether the shaft holds. No enable or trigger is appended | Whether `6060=1` is accepted, whether `6061` follows, whether status or monitor bits change, and whether an earlier target has any effect at mode change. Also whether the shaft holds or turns, which is the only torque evidence | Any shaft movement, abort, lost reply, boot-up, alarm, `2100` running or nonzero `606C`: open the cutoff, stop, keep the record. A lost reply means the mode is unknown and refuses an enable |
| P3, enable. **Attendant required**, cutoff in hand, separate authorization of the trial's values | In the same power-up as a clean P2, with nothing else written: the attendant checks by hand whether the shaft holds and records `FREE` or `HOLDING`; fresh read; `requestEnable` with `ATTENDED_EXPERIMENT`, a new trial identity, the session's reservation and `nextOperation()`, a short expiry, `NO_MOTION_WRITE_RECORDED`, that shaft observation and the exact reviewed `6041`, `607A`, `60FF`; interval `ENABLE_WORD_INTERVAL_US`; a deadline chosen beforehand and at least the admitted minimum. Before and after each word, the attendant checks holding torque by hand, with the cutoff in the other hand. Word 6 may remove holding torque. Wire: `40 7A 60 00`, `40 FF 60 00`, `2B 40 60 00 06 00 00 00`, cycle, `2B 40 60 00 07 00 00 00`, cycle, `2B 40 60 00 0F 00 00 00`, cycle | Whether low bits go 1, 3, 7, whether `2100` bit 4 moves, which bits above bit 2 change, whether `6064` settles, and from the attendant whether torque appears at which word and whether the shaft moves. Whether an earlier target or curve does anything here is the point of the trial | Any shaft movement; any terminal other than `OBSERVED`, whose raw words are then kept; EMCY; an alarm. Open the cutoff on unexpected motion. Do not retry on failure: a failed prefix leaves the state unknown. Power-cycle before any new trial |
| P4, stop after enable. Attendant, after P3 succeeded with no movement | `requestStop` with the enable's own last read, then fresh cycles | `000B` and `010F` from an idle `000F`: acknowledgement, reports, and whether torque changes. Packet 16 owns moving stops | As P3 |

Prerequisites the lead must establish first: firmware holds no `AxisSession` and has no route for a
`6060` write, an action or an attended trial, so these need a bench route on `examples/s2_bench`. That
also brings the firmware's `sizeof(AxisSession)`, now larger by one context, and packet 16's stack
measurement. Neither `607A` nor `60FF` is on the DEC-1 accepted list, so P3's two reads are the owner's
explicit choice, and they use `UNQUALIFIED`.

If a specimen does not follow 1, 3, 7, the sequence ends at `DEADLINE` after word 6 with nothing more
sent. That is an answer for this table, and the fix is a reviewed change here, not a longer deadline.

## Time, uncertainty and remaining verification

The passive getter follows the existing read windows and current-time age
checks. It never processes a service reply or rewrites a terminal result.
Current `advanceRead` applies its overall processing-time cutoff even when
a service completed before that cutoff; decoding does not relax this rule.
See [service ownership](canopen_services.md) for receive timestamps and channel
reconciliation, which remain distinct from a multi-step operation deadline.

Enable tests receive-versus-processing-time precedence, including on-time
receipt processed late and first receipt after expiry. Matching service acknowledgement, observed state and physical completion
remain separate. Lost replies retain unknown execution; later observations do
not authorize replay. Cancellation/recovery cannot implicitly stop, release,
clear faults or enable the drive.

`test/enable_action_test.cpp` covers every enable word and transition through the real owner, with
literal bytes: unknown bits, sequential-sample disagreement, wrong mode, nonzero velocity target of
unknown origin, fresh and copied history, fault at each step, delayed responses,
cancellation and deadline precedence, reboot, stale generations, atomic rejection and a stop after each
step. All of it runs against a fake and qualifies nothing. Release has no tests because it has no
behavior. Decoder checks do not close these obligations. The
[prompt 30 report](https://github.com/janhavelka/MotorControl-CAN/blob/main/docs/reports/prompt_30.md) records actual software checks separately
from source agreement and hardware qualification.
