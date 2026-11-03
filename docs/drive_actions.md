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
attended on both specimens on one firmware, is described below, and release has no API. The separate
[conditional stop lifecycle](stop_actions.md) uses common `ActionOperation.h`
evidence and concrete `Actions.h` contexts; it establishes neither of those. [Fault clear and counter zero](fault_reference_actions.md)
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

## Enable: gated, and run on two specimens

`prepareEnable` in `Actions.h` implements the documented position-mode sequence through the
ordinary `nextAction` and `advanceAction` lifecycle, and `AxisSession::requestEnable` runs it
through the sole owner. **It succeeded on both bench specimens on 2026-11-03**, after a fix that
session found (bit 10, below), on one firmware. That is not a qualification: the
measured paragraph after the transition table and the
[attended report](reports/2026-11-03_attended_bench.md) say what was and was not observed.
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
  to 1, then 3, then 7 could, and whether a specimen follows it is unmeasured.
- Under EN86, `0x8037` has zero-speed clear, which reads as running. Under ZH86 it reads as stopped.
  Decoding cannot decide this word.
- Mode 0 is not an operating mode here, so a specimen needs an explicit `6060=1` before an enable.
  That write has never been sent. Nothing has been enabled, and no shaft or torque observation of
  any kind has been retained.
- Low bits `111` with `2100` released clear is the pattern p12 pairs with an enabled drive, and both
  specimens show it at power-up. Word 6 may therefore be a 15-to-6 step that **removes holding
  torque**, not the first step towards it. Which one it is has not been observed.

### Project policy

Each of these is this project's decision, not a vendor statement.

- **Mode 1 only.** The ZH86 p13 sentence is why: in mode 3 the same words are documented to start the
  motor, and `60FF` reads nonzero on both specimens. EN86 disagrees. Mode 3 has no approved path.
- **A target of unknown origin fails closed.** `607A` and `60FF` readings are history, not intent. The
  action reads each once before any word and fails with `OBSERVATION_SCOPE` unless both exact
  values equal `EnableEvidence`'s reviewed values. Zero is not exempt: an absolute target of zero
  does not say no motion, and `60FF=0` has no documented meaning. A match says only that someone
  looked at those bits. It does not call them safe. The action never writes or zeroes a target.
- **`2100` decides alarm and running**, because its two manuals agree. Disputed `6041` bits decide
  nothing and are never merged into a union mask. They must equal the reviewed word at admission
  and must not change during the sequence. A change ends it with both words retained.
- **`2100` released is allowed either way at admission**, because specimens read it clear before any
  enable. In-place is required as one necessary condition: a curve left unfinished might show it
  clear. A set flag does not prove the absence of a pending curve.
- **Start history.** `EnableStart::NO_MOTION_WRITE_RECORDED` is the application's statement about the
  time before this session existed. The library cannot check it. `AxisSession` adds what it can see
  and refuses after it has itself admitted any write other than an acknowledged `6060=1`, for the
  rest of that object's life, across close and open. A stop admitted on any path counts.
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
- **An enabled-looking start needs an attendant.** A start report of low bits `111` with `2100`
  released clear is refused unless `EnableEvidence::powerUpShaft` records the attendant's own
  observation at power-up, `FREE` or `HOLDING`. The library cannot check it.
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
| Word 7 | Word 6 observed | Unchanged | `2B 40 60 00 07 00 00 00` | As above | As above with low bits 3 | As above | As above, prefix retained | Unknown |
| Word 15 | Word 7 observed | Unchanged | `2B 40 60 00 0F 00 00 00` | As above. On acknowledgement it becomes the latest controlword | As above with low bits 7 and `2100` released clear | Overall | As above. `SUCCEEDED` needs all of it | **A drive report, not torque.** Mode-1 motion is documented only from a later bit 4 edge, and whether an earlier curve resumes is unmeasured. Reference validity is not claimed either way |
| After success | Word 15 observed | Target unchanged by this action | None | None | A later move or stop needs its own fresh state. The action's last read qualifies a stop | None | A second request refuses: no re-enable on any evidence | Unknown |
| Enable in mode 3 | Any | Mode 3, with whatever `60FF` reads | None | None | The mode byte | None | Refused at admission | ZH86 p13 and OP86 p11: the motor starts. EN86 p13 disagrees |
| Enable in mode 0 or 6 | Any | Mode 0 undefined; mode 6 has its own procedure | None | None | The mode byte | None | Refused. Prompt 36 owns any mode 6 enable | None |
| Enable from `000F` or after a stop | Latest word `000F`, `000B` or `010F` in this session | Any | None | None | None | None | Refused, and never writing 6 merely to "reset" | A word from 15 back to 6 has no documented meaning here |
| Stop during the sequence | Acknowledged 6 or 7 | Unchanged | A fresh idle report: no word, since DEC-8 has no arrow from them. A fresh running report, or none: best-effort `000B`, as for the same word still in flight | `000B` path: [stop guide](stop_actions.md) | The stop's own | The stop's own | The enable ends either way. Use the independent cutoff | Unknown |
| Stop after `000F` | Acknowledged 15 | Unchanged | `010F` or `000B` with a fresh cycle after the acknowledgement, otherwise best-effort `000B` | [Stop guide](stop_actions.md) | [Stop guide](stop_actions.md) | Stop's own | The enable ends, and any word in flight settles into it | Unknown |

**Measured on node 1, 2026-11-03.** The shaft held torque at power-up, before any word from this
library, with `6041=8037` in mode 0 and `8437` after `6060=1`. Word 6 was acknowledged and the fresh
read showed `6041=8031` and `2100=0011`: low bits 001 as the table says, released set, and bit 10
cleared. The shaft then turned freely. So word 6 from this power-up state **drops holding torque**,
and bit 10 (EN86's zero-speed bit) follows the enable state. The library used to compare every bit
above 0..2, so it ended that enable at word 6 with `OBSERVATION_SCOPE` and sent nothing further. It
now ignores bit 10 there (`STATUS_EN86_ZERO_SPEED`), because `606C` already decides velocity.

Cancellation, a deadline, a reboot, bus-off, a changed epoch or an independently routed
`DRIVE_FAULT` ends the sequence locally at any step. None is a stop. `execution`, `effectsPossible`,
`uncertain` and `commandHistory` keep every prefix word, and nothing follows or is replayed. The
inclusive overall deadline wins at equality, including for a terminal received on time and processed
late. The owner gives `AxisSession` no EMCY view, so an application that routes a fault calls
`cancelEnable` with `DRIVE_FAULT`.

The [stop guide](stop_actions.md) explains why a stop needs the session's own latest word. The
session records each enable word through its one admission path, so an acknowledged `000F` becomes
that word, and the first of those admissions withdraws any older one. Settings and a later move keep
their own predicates on an acknowledged `000F`. Each still needs its own fresh report, and an
enable's success does not satisfy them by itself.

## Release: pending, with no API

There is deliberately no `prepareRelease`. Its contract needs one specific applicable command, the
states it is admissible from, and new observations that establish its effect, and no reviewed source
gives that:

- `6040=0` appears only as the example paired with low bits 000. No source says that word releases
  the motor, or from which state it may be written.
- Low bits 0 are a report. Both specimens reported them, with `2100` released, after a stationary
  `000B` from `000F` and from `010F` ([unattended report](reports/2026-11-03_unattended_bench.md)).
  That is a quick stop's measured effect from those two words, not a release command: no word was
  tried as a release, and the reference consequence is not stated.
- `2100` bit 4 reports released. It is an observation, not a command.
- Input function 4 is labeled motor enabling or MF, with no behavior text, and is not a CAN command
  (D-28).
- Generic CiA402 words 0, 6 and 7, an NMT stop and a brake-output label cannot supply it.

What would close it: a manufacturer statement, or a bounded experiment that first has a working
enable, then one explicit candidate word from a known state, with raw `6041`, `2100`, `606C`, an
independent shaft and holding-torque observation, and the reference consequence stated. Release is
not a prerequisite for enable or stop qualification. D-02's home-then-move gap depends on it, so that
gap stays open too.

## Bounded physical plan

For the lead, on an attended bench. P1 to P3 ran on both specimens in the
[attended session](reports/2026-11-03_attended_bench.md). P4's `000B` ran on node 2 in the
[unattended session](reports/2026-11-03_unattended_bench.md), from a library move's rearm `000F`
rather than the enable's own, and released the motor; P4's `010F` has not run. Default to node 1 alone, and add node 2 only
after node 1 passes. Before each state-changing trial, record the operator, cutoff and clear shafts,
and stop on any abort below. Never retry within a trial.

| Trial | Exact typed sequence | Decides | Abort on |
| --- | --- | --- | --- |
| P1, refresh (step 1). No attendant needed, no writes | Packet-11 image, then reads of `6041`, `6061`, `2100`, `2101`, `2102`, `6064`, `606C` on the DEC-1 accepted list; `1001` and `603F` as unqualified single reads, never polled, with the fault LEDs recorded before and after; `6081`, `6083`, `6084` through the typed motion-parameter read, which is unqualified; `1017` as an unqualified single read; and `607A`, `60FF` once each under a one-use experiment grant, because their ledger acceptance covers only the enable's own reads | Whether the November values still hold, and the exact words to review | Any abort, timeout, boot-up, bus fault or changed value: stop and report |
| P2, mode (step 3). **Attendant required.** One isolated write, shaft watched | Full read of P1. Then `2F 60 60 00 01 00 00 00`. No typed write or firmware route builds this. The only path is a caller-built transfer through `AxisSession::admitPrepared`, whose bytes `enable_action_test` asserts. Then fresh `6061`, `6041`, `2100`, `2101`, `2102`, `1001`, `603F`, `6064`, `606C`, repeated for a window chosen beforehand, while the attendant watches a mark on the shaft. Only once the reads stay quiet, and only if the attendant judges it safe, they check whether the shaft holds. No enable or trigger is appended | Whether `6060=1` is accepted, whether `6061` follows, whether status or monitor bits change, and whether an earlier target has any effect at mode change. Also whether the shaft holds or turns, which is the only torque evidence | Any shaft movement, abort, lost reply, boot-up, alarm, `2100` running or nonzero `606C`: open the cutoff, stop, keep the record. A lost reply means the mode is unknown and refuses an enable |
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
[prompt 30 report](reports/prompt_30.md) records actual software checks separately
from source agreement and hardware qualification.
