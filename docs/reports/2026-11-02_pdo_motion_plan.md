# Limited TPDO1 experiment and subsequent motion readiness

**Correction, packet 1.** This report calls `607A=5000` and `60FF=120` retained targets or stale intent. Both equal their EDS defaults and no retained record shows a write or a pre-cycle read, so that reading is unsupported. See the [corrected reading](../device_conflicts.md#corrected-reading-of-the-november-2-inventory). The measurements below are unchanged.

Prepared on 2026-11-02 against branch `promptset/motorcontrol-can`, entry
`7764c89`. This is an independent source review and proposed finite procedure.
It records no new hardware execution, successful remapping, motion qualification
or completed motion API. The lead owns implementation, integration checks, the
only bench session and Git synchronization. The user has authorized explicit
NMT/freshness acquisition, extended reads and a limited PDO experiment now, and
requested preparation for a later small motion test. Preparation does not extend
that scope to an arbitrary motion, reset, save or input reassignment.

## Evidence and the questions being tested

The [last retained connection record](2026-11-02_s2_live_connection.md) establishes
both user-labelled CL86-C specimens on nodes 1/2, 1 Mbit/s, ESP32-S2/IDF 5.5.5,
TJA1051, TX17/RX16/S18 and UART COM15. The user confirmed free shafts, motors
bolted to the table and independent drive-power cutoff. A later user image/report
suggests motor label **LC60H2127**, partly obscured by paper; specimen identity
still needs an unobscured reading. Exact installed ratings, encoder/wiring
configuration and physical scale remain unrecorded. Keep these
specimens separate despite their shared raw identity tuple.

Reopened original EN86 PDF pp10-13/15-19, ZH86 pp10-12/18-19, OP86 pp9-10 and
the EDS sections named below. Rendered EN86 p12, ZH86 p12 and OP86 p10 were
visually inspected, including their differing status tables. Original hashes
and applicability are in [SOURCES.json](../reference/SOURCES.json). Page numbers
are one-based PDF pages. The OP86 cover/internal 86C/57C inconsistency remains.

| Claim | Source and disposition |
| --- | --- |
| TPDO communication `1800:01/02/03/05` are U32/U8/U16/U16 RW; mapping `1A00:00` is U8 RW 0..4 and `:01..04` U32 RW | EDS `[1800sub1..5]`, `[1A00sub0..4]`; EN86 p16 lists TPDO mapping records RW. Documented fields, selected-firmware mutation behavior unmeasured. |
| Variable mapping uses Pre-operational and the disable/count-zero/entries/count/enable order | [CiA PDO protocol, mapping procedures](https://www.can-cia.org/can-knowledge/pdo-protocol), checked 2026-11-02. Conditional protocol procedure; it does not assert this Lichuan firmware implements variable mapping. |
| `6041` U16 and `6064` I32 are RO, PDO-mappable | EDS `[6041]`, `[6064]`; EN86 pp11/16. The actual existing map contains both. A mapping entry is not a status dialect or physical-position scale. |
| `1017:00` is Producer Heartbeat Time, U16 RW, EDS default zero | EDS `[1017]`; default is not live readback. Current producer interval and heartbeat state remain acquisition questions. |
| Position mode is `6060=1`, observed through `6061`; enable is `6040=6,7,15` about 10 ms apart | EN86/ZH86 pp11-12; OP86 p9. ZH86 explicitly says monitor `6061`; EN86's instruction to set it is contradicted by the mode table and EDS RO. |
| Relative position starts on `000F -> 005F`; absolute on `000F -> 001F` | EN86/ZH86 p12 and OP86 p10. Bit4 rises; bit5 remains zero; bit6 selects relative. These examples do not establish the relative base, command FIFO, completion correlation or repeated-command safety. |
| Normal stop candidate `000F -> 010F`; quick-stop candidate `000F -> 000B` | ZH86 p12/OP86 p10; EN86 p12 includes only quick stop. No enable preamble, certified safety, torque, queue or stop-distance promise follows. |
| Monitor `2100` bit2 running, bit3 alarm, bit4 released (zero enabled) | EN86 p17/ZH86 p18 agree; EDS `[2100]` says RW while manuals say RO. Read only in this procedure. |
| Target `607A` is I32 pulses within -1000000..1000000; `6081` U32 0..3000 rpm; `6083/6084` U32 0..2000 ms | EN86/ZH86 p11, EDS named entries. Zero-ramp behavior and physical command/feedback conversion remain unknown. |

Both prior specimens returned `1800:01=181/182`, `:02=255`, `:03=100`,
`:05=0`, `1A00:00=3`, slots `60410010,60610008,60640020,0`,
`6041=8037`, `6061=0`. These are measured historical values, not admission
evidence for a new session. EDS templates differ. `1009/100A` were explicitly
one byte `0C/20`, and `1018:00=5`; retain these contradictions without guessing
a human firmware version or querying an undocumented fifth identity child.

The experiment asks whether this exact firmware accepts that conditional
mapping procedure and confirms the intended live map. Successful mutation is
its measurement, not a prerequisite. Production remapping, timer support,
persistence and qualified PDO sample provenance need their own evidence and
tests after the experiment.

## Admission and bounded acquisition

Use the existing one receive owner, SDO engine, checked public codecs and retained
completion path. Add only the exact reviewed experiment work that is missing;
the current `RawEvidence` and `ReadExperiment` interfaces are upload collectors,
not general download executors. Do not bypass public validation with UART raw
frames or create a second receiver. Each emitted request retains operation,
step, full route/epochs, payload, immutable deadline, native abort and actual TX
retirement. Diagnostic copies cannot decide whether a TX is still pending.

Select node 1 first. Keep node 2's ordinary heartbeat/EMCY/PDO dispatch active;
send no broadcast. Before any download, finish and release previous terminals
and establish the live exclusive reservation, selected profile and current
controller/SDO/NMT evidence. Record the exact reviewed source revision and image
hash, fixture, intent, bounds and independent power cutoff in the acquisition.
Fake-test the complete proposed sequence and failure at every step first.

Use explicit application policies, recorded as such: each SDO queue and response
budget 250000 us, read phase at most 30 s, mutation phase at most 10 s, passive
observation at most 2 s, and no automatic retry. Clamp every step to its phase's
original deadline. These are experiment budgets, not Lichuan response guarantees.
Keep acquisition storage large enough for the selected finite inventory and
record any trace loss, actual service gaps and stack headroom.

First perform this exact finite read inventory, recording server-indicated
width separately from successful typed `EXACT` evidence:

| Objects | Purpose and conservative interpretation |
| --- | --- |
| `1000`, `1009`, `100A`, `1018:00..03`, `2000`, `2001` | Reconfirm specimen/session raw identity; preserve the known width/count conflicts. No index/subindex sweep. |
| `1017` | Read actual producer setting before deciding how NMT observation can be obtained. A zero value explains neither every missing heartbeat nor drive host-loss behavior. |
| `1001`, `603F`, `6041`, `6061`, `2100`, `2101`, `6064`, `606C` | Raw error, status, monitor, logical input and feedback baseline. Preserve individual acquisition bounds; this is a sequential block. Do not infer actual-speed scale from target rpm. |
| `1800:01/02/03/05`, `1A00:00..04` | Exact complete TPDO1 baseline, including inactive slots. Abort mutation admission on any unexpected layout/flags/value pending review. |
| `607A`, `6081`, `6083`, `6084`, `605D`, `60FF`, `2200`, `2201`, `2202`, `607D:01/02`, `2500`, `2501` | Later motion preparation only: current targets, ramps, halt option raw value, direction/subdivision/limits, control and encoder settings. Do not write a default or assume a supported value merely because it can be read. |
| `2400:01..08`, `60FD` | DI polarity and seven assignments, plus raw inputs. Use EN86 p18/ZH86 p18 assignments 0 undefined, 1 origin, 2/3 limits, 4 MF, 5 stop, 6 quick stop, 7..13 user inputs; do not disable an assigned function to permit testing. |

EN86/ZH86 p19 and EDS `[2500]/[2501]` confirm U16 RW drive control mode
(0 ineffective, 1 open loop, 2 closed loop) and encoder resolution, without
establishing encoder scale or quadrature convention. No read of WO
`6040/6060` is required. Preserve Q20 unknown
read effects under the existing exact experimental grant; raw acquisition does
not promote production descriptors to non-consuming or repair typed widths.

Request addressed Pre-operational using the public NMT producer: CAN standard
data ID `000`, DLC 2, bytes `80 01` (node 2 later: `80 02`). Retain local TX
result; NMT has no service acknowledgement. Require current post-handoff
`701` data byte `7F` under the reviewed fresh observation-boundary contract
before admitting mapping writes. A matched SDO does not distinguish Operational
from Pre-operational. With unknown capture time, do not relabel dequeue time
as arrival or manufacture strict post-handoff evidence.

If `1017=0`, heartbeat observation is an additional concrete configuration
experiment, not an implicit consequence of NMT. A candidate is exact U16
`1017=1000` (`2B 17 10 00 E8 03 00 00`), read back and observe for at most 5 s.
The selected pinned CANopenNode `301/CO_NMT_Heartbeat.c:56-65,104-111`
multiplies U16 `1017` by 1000 for microseconds, corroborating millisecond units;
its producer is separate evidence from this Lichuan firmware's activation.
Receive-boundary proof must still be established before execution. Include this exact
mutation in the authorized freshness procedure and fake tests; otherwise retain
the missing NMT observation as the narrow blocker. Do not infer host-loss
stopping, save it, or silently restore it after an uncertain download.

## One TPDO1 mapping experiment

Only proceed if fresh complete baseline is exactly the measured map above and
the reviewed post-NMT Pre-operational observation is available. Snapshot it as
the original configuration. The chosen experimental map is two entries:
`60410010,60640020`, payload U16 status then I32 actual position, six bytes.
Leave type 255, inhibit 100, event 0 and inactive slots 3/4 unchanged. No RPDO,
SYNC, controlword, mode, motor target, save or reset belongs in this phase.

The original three-entry map is seven bytes. Six bytes distinguish this test
layout from that immediately preceding layout, but do not establish that no
older six-byte traffic exists or qualify ordinary PDO publication. The
experiment can retain and decode diagnostic copies without making that claim.

Perform the following downloads in order, only after the prior SDO success and
local controller TX retirement. After all six acknowledgements, perform nine
exact final uploads in the baseline inventory's order and compare every value.
The expected values below are final readback criteria, not extra intermediate
upload commands. The bytes are independent review vectors; the public codec emits
the actual frames through the owner. All SDO frames use `601/581`, standard data,
DLC 8; replace node identifiers for a separately admitted node-2 run.

| Step | Exact write | Node-1 SDO request bytes | Required resulting value |
| --- | --- | --- | --- |
| 1 | `1800:01` U32 `80000181` | `23 00 18 01 81 01 00 80` | Same disabled COB-ID. Preserve every original flag other than bit31. |
| 2 | `1A00:00` U8 `00` | `2F 00 1A 00 00 00 00 00` | Count zero; no active map is claimed. |
| 3 | `1A00:01` U32 `60410010` | `23 00 1A 01 10 00 41 60` | Slot1 exactly `60410010`. |
| 4 | `1A00:02` U32 `60640020` | `23 00 1A 02 20 00 64 60` | Slot2 exactly `60640020`. |
| 5 | `1A00:00` U8 `02` | `2F 00 1A 00 02 00 00 00` | Final count two, active slots as above, slots3/4 preserved. |
| 6 | `1800:01` U32 `00000181` | `23 00 18 01 81 01 00 00` | Enabled original COB-ID, type/inhibit/event unchanged, full map reread. |

For node 2 the disable/enable values are `80000182/00000182`; do not change
its COB-ID to node 1's. A success reply is `60` plus the matching index/subindex,
not proof of subsequent steps. Native abort `80` retains the full abort word.
Unknown write effects stop progression. A new host epoch cannot repair an
unresolved SDO channel or make old same-route replies fresh.

All nine baseline uploads and all fifteen typed configuration services carry
`SdoTransfer::requirePreOperational=true`. The selected adapter checks this
prerequisite again at admission, native handoff and actual controller enqueue.
That closes the race where an Operational heartbeat is received after local
admission, or after handoff but before a queued SDK submission. The ordinary
rule that SDO works in Operational is insufficient for this particular experiment.

After complete readback, explicitly request addressed Operational (`01 01`),
observe current `701=05` when available and collect at most 2 s of ordinary
owner traffic. Require no motor action. Type255/event0 does not promise a PDO
during this window; no received PDO is not evidence that accepted map writes
failed. If a six-byte `181` frame arrives, retain raw status/position bytes and
their observation provenance; compare nearby SDO values with their separate
times. Do not declare simultaneous equality, qualified capture or shaft motion
from those comparisons.

An event-timer/SYNC/RTR trial would be a separate exact reviewed experiment.
Do not escalate to it automatically because passive observation was quiet.

There is no automatic rollback. On complete unambiguous success, restoring the
original map can be a separately recorded finite follow-up using the same
Pre-operational/disable/count0/three original entries/count3/enable/readback
procedure. Original entries are `60410010,60610008,60640020`; original timers
and COB-ID come from this session's snapshot. Reused seven-byte samples remain
unqualified. A partially failed mutation must first be reconciled by exact
supported reads and channel/owner recovery; never replay the whole script.

| Failure boundary | Retained effect and next action |
| --- | --- |
| Refusal before possible TX | Configuration unchanged by that step; preserve prior confirmed prefix. |
| Confirmed disable/count0 | PDO can remain disabled or unmapped. Record exactly that, without certifying the desired map. |
| Entry/count/enable abort or timeout | Retain each ACK and last exact readback; affected map/active state may be partial or unknown. Stop further writes and withdraw dependent samples. |
| Cancel, boot-up, bus-off or binding change | Retire owner work, retain uncertain remote effects, revoke dependent evidence. Local cancellation is not a drive stop. |
| Lost trace or unqualified time | Report acquisition/provenance limit. Do not rewrite it as a device rejection or physical result. |

## Concrete preparation for the subsequent small motion test

The manufacturer's current
[60-series closed-loop motor page](https://en.xlichuan.com/product-xq/71.html),
checked 2026-11-02, explicitly lists LC60H2127 with 1.8-degree full-step angle,
1000 PPR encoder, 5.8 A phase current and 5.5 N m holding torque. The page also
allows customer variants. Thus 200 full steps per motor turn is an arithmetic
consequence of that representative 1.8-degree specification, conditional on the
actual specimen matching it; it is not inferred from the model spelling. Neither
1000 PPR nor 200 full steps establishes this CL86-C's `607A` command-pulse scale,
`6064` feedback-count scale, quadrature convention or installed current setting.
Do not set a current, infer 4000 feedback counts or convert sixteen command
pulses into degrees from that page alone. The Chinese page was indexed with the
same candidate model but direct retrieval returned 403; the English primary
page was opened successfully. No original reference snapshot was replaced.

The future finite test can investigate enable, one absolute edge, status dialect,
feedback change and a selected stop without waiting for finished production
motion APIs. Prompt 29 expressly permits an isolated reviewed experiment through
the public validated owner path. The disputed behavior is an intended
measurement; independently controlled drive power and physical observation must
carry admission and intervention rather than that disputed behavior alone.

Before any motion execution, finish the readings above, record actual motor and
drive nameplate/current/wiring facts and a physically acceptable movement
envelope, establish one operator at the independent cutoff and an independent
view of each shaft. Select one specimen only. Confirm the other cannot receive
motion writes. Active or unexplained stop/limit/MF inputs and faults require
understanding, not disabling. Electrical qualification remains separate; do not
choose a supply from conflicting manual tables. The lead must record motion
authorization and review of the actual finite sequence/image before execution.

The native candidate is the **absolute native command coordinate 16**,
profile speed **1 rpm**, acceleration/deceleration time **1000 ms** each, at
most **5 s** for its observation phase. These deliberately chosen application
values fit EN86/ZH86 p11 and avoid the unknown zero-ramp case. They are not
vendor defaults, a proved angular displacement or a proved physical speed cap
under unverified motor/scale configuration. Command/native-counter/feedback and
reference qualification remain prerequisites; actual position zero alone does
not establish a small physical move. If those physical bounds cannot
be accepted for this fixture, revise this exact candidate before admission.
No automatic return stroke, negative speed, immediate update, homing, position
zero, fault clear or parameter save is part of it.

For the dry run, retain an exact expected request inventory with `6060` U8=1,
`6040` U16 values 6/7/15, `607A` I32=16, `6081` U32=1,
`6083/6084` U32=1000 and the single `6040` U16=31 trigger, separated into the
phases below. Derive these through the public typed preparation and exact SDO
codec/service path; compare literal wire fixtures independently, inject refusal,
abort, late reply, reset and cancellation at every step, and assert that no
second trigger or automatic stop/re-enable/save is admitted. This future motion
experiment is not implemented by the PDO-only harness delivered in this change.
Run it first with a fake owner and retain a no-device dry-run record for review.
Only an authorized separately admitted physical experiment can fill the motor
and firmware evidence columns.

The reviewed experiment should progress in separately retained phases:

1. Acquire fresh raw `6041/6061/2100/603F/1001/6064/606C`, inspect the shaft
   independently, and retain target/configuration readback. `8037`/mode0 does not
   prove disabled, standstill or a selected status dialect. If `2100` reports
   released, retain that observation; if enabled/unknown, the exact mode-change
   trial must explicitly account for possible retained-target motion using the
   independent cutoff. Do not insert an undocumented release command.
2. Use the documented position-mode candidate: exact U8 `6060=1`, ACK, observe
   `6061=1` and raw monitor/status. This is itself a state-changing experiment;
   unexpected movement, alarm, wrong mode or timeout ends further progression.
   Never write `6061`. Observe whether a retained velocity/position target had
   any effect instead of assuming its absence in advance.
3. If that phase remains within the accepted envelope, send U16 `6040=0006`,
   `0007`, `000F`, about 10 ms between commands, preserving actual issue/ACK
   times and observations. Schedule in the application; no blocking core sleep.
   Every write must ACK and retire before the next. An ACK later than the planned
   spacing is recorded, not hidden with a shorter subsequent delay. Abort on
   unexpected physical movement, fault, lifecycle loss or uncertainty. This phase
   measures the low-three-bit examples and the agreed monitor's enabling meaning.
4. Under the same continuous owner reservation, after the acknowledged `000F`,
   acquire fresh mode 1/low7 and monitor alarm-clear/release-clear observations.
   This can establish the narrow prerequisite for the existing public
   `prepareStop`, independently of disputed running/completion dialect bits.
   It does not establish physical stopping behavior before measuring it.
5. Write I32 `607A=16`, U32 `6081=1`, `6083=1000`, `6084=1000`; require each
   ACK/retirement and exact readable parameter readback. Confirm current mode 1
   and raw fault/monitor evidence again. Then send exactly U16 `6040=001F`
   once, providing the documented `000F -> 001F` rising edge. Any uncertain
   trigger ends the sequence; do not resend or re-enable.
6. For at most 5 s, retain a bounded sequence of raw status/monitor/position/
   speed/error observations and independent shaft observations. Record bit15
   and monitor bit0 before/after the edge, running bits 10/14 and monitor bit2,
   and feedback delta. A pre-existing completed bit cannot finish this command.
   Failure to observe a short running interval does not prove it never ran.
7. Stop/intervention is a distinct operation. The independently available power
   cutoff does not depend on the software stop experiment succeeding. Existing
   `prepareStop` supports only a continuous-history last acknowledged `000F`
   transition: after the `001F` trigger it must reject that stale prerequisite.
   Do not forge new `000F` evidence, reacquire a token or enable to satisfy it.
   A source-reviewed `001F -> 000F -> 000B` rearm/stop candidate would add an
   unqualified transition, so it is not silently supplied here. First investigate
   `000F -> 000B` without a position trigger, or execute the one-shot move with
   independent power intervention and leave moving-stop behavior pending.

This staged choice closes the circular prerequisite: a stationary enable/stop
experiment can establish its narrow observations before a position trigger;
the absolute-target experiment can then use independent power interruption
without claiming a general stop API it has not established. Neither experiment
proves stop from the post-trigger controlword, release torque behavior or
production motion completion. `6040=0000` appears in a status example but is
not a source-backed universal release fallback. NMT Stop is also not a motor
stop. No `605A/6085` write or guessed `605D=1` deceleration meaning is allowed.

One stationary positive displacement cannot distinguish all possible relative
bases if actual and previous commanded positions coincide. Record exactly what
was discriminated. Do not deliberately create following error, stall a shaft
or interrupt an uncertain command merely to make the bases diverge. Production
relative-base support remains gated unless an independently bounded further
experiment or applicable manufacturer evidence resolves it. Physical angle
calibration, feedback scale, repeat/rearm, release, quick stop while moving and
all firmware variants remain separate pending obligations.

## Fresh read-only ecosystem audit and verification

All four sibling checkouts were clean at inspection; no sibling was fetched,
edited, tested or operated. This proportional audit inspected current working
files as well as revisions. The lead handles the shared branch fetch and sync.

| Checkout and revision | Inspected seam and delta from the prior bench report |
| --- | --- |
| MotorControl-RS `4e299d281d1da0635cf29b3dcd0b874166f9f498` | `src/profiles/ess_rs/Actions.cpp`, `test/actions_test.cpp`, `.github/workflows/verify.yml`: bounded explicit actions, ACK distinct from observed result, exact command tests and terminal uncertainty. Revision advanced from prior `513251d`; CAN keeps these common meanings, not RS registers, stop semantics or readiness assumptions. |
| ADS1115 `d613a13cd3d43471905c507bfb4094bba1c1da94` | `include/ADS1115/Config.h`, `examples/02_owner_safe_poll/main.cpp`, `test/test_basic.cpp`, `.github/workflows/ci.yml`: one externally serialized transfer, borrowed callback buffers, explicit budgets and real read side effects. Revision unchanged; no sensor retry/recovery policy imported. |
| INA228 `73f954838d3fac8cce747d54d0ad738940d4c625` | `src/INA228.cpp` cooperative initialization/readback and `test/test_basic.cpp` failure injection at every transfer, `.github/workflows/ci.yml`: partial/indeterminate effects and hardware validity retained. Revision unchanged; no sensor cleanup treated as permission to replay motor writes. |
| FieldCore-node `5ad9292dc8481cf08be9a61f662495a6474d1da0` | `src/devices/rs485_motor/Rs485MotorModule.cpp`, `test/host/rodtester/motor_module.cpp`, `scripts/verify.py`: actual finite-age/reference admission, explicit relative ACTUAL basis and stop policy. Revision unchanged; read-only consumer comparison, no CAN integration claim. |

## Delivered software subset and limits

The later bounded implementation assignment added
[`PdoConfiguration.h`](../../include/MotorControlCAN/profiles/lichuan_c/PdoConfiguration.h)
and its source, plus application-only
[`PdoExperiment`](../../examples/s2_bench/PdoExperiment.h) and two dedicated tests.
It implements exactly nine fresh baseline reads, the six downloads and nine
final exact reads above, not arbitrary remapping/timer configuration. Explicit
fixture/firmware/procedure/authorization records distinguish isolated experiment
from caller-established fixture qualification. Success never promotes the former.

The harness reserves actual producer 3/axis exclusion before waiting for a new
qualified Pre-operational observation; all baseline uploads follow that frozen
observation. It retains all 24 owner completions, a 112-record copied traffic
prefix and 16-record cleanup tail. It never receives/services the controller
itself. It advances from baseline operation id to id+1 for the core phase.
The conservative first possible-write notice occurs after local admission and
before another owner service turn, withdrawing the application configuration
epoch even if later refusal proves no TX. Existing affected TPDO1 maps/samples
are withdrawn before that notice; unrelated maps stay separate. Full readback
uses the next epoch only after the final write result is released and the actual
owner proves local TX retirement and advances its idle configuration.

There is no automatic map installation or PDO sample qualification. A replacement
on a previously bound route may require additional ordinary-channel confirmation
under the existing adapter policy, which isolated experiments deliberately do
not establish. The harness preserves this limit instead of bypassing it.
Host tests use the real owner and selected SDO engine, including native abort at
each of 24 steps and contradiction at each final readback. Core tests cover each
of 15 service steps with seven failure kinds, deadline equality, local cancel,
wrong epoch/step, exact baseline widths, bounded NMT time and unchanged outputs.

Repository workflow and `scripts/check_promptset.py` were inspected.
`python scripts/check_promptset.py` passed,
including 231 Markdown files, 2316 local links and 137 original hashes.
`git diff --check -- docs/reports/2026-11-02_pdo_motion_plan.md` and
`git diff --no-index --check -- NUL docs/reports/2026-11-02_pdo_motion_plan.md`
passed; the latter checks the currently untracked new file too. Independent
strict C++11 GCC builds passed **4785 core checks** and **6793 harness checks**
in `.build/pdo-motion-harness` / `.build/pdo-motion-source`, including the
Operational-heartbeat-before-owner-handoff regression. The independent owner
reviewer owns the delayed SDK enqueue regression. All six new C++ files were
formatted with the repository's clang-format14 tool. The lead rechecks after
integration and runs complete implementation/package/Doxygen/firmware gates.
No new hardware behavior or CI outcome is asserted by this worker report.

## Motion-readiness software and subsequent retained evidence

The follow-up assignment implemented
`RawEvidenceInventory::MOTION_READINESS`: exactly thirty uploads of the
motion/error/feedback/DI inventory above, with four-byte capacity,
`SERVER_INDICATED` actual widths and `UNQUALIFIED` effects. This inventory rejects
the identity/network/heartbeat flags, so selecting it cannot select the separate
heartbeat download. Raw storage now retains at most 30 terminals, 112 traffic
copies and a 16-record cleanup tail. Controls `v`/`b` and host choices
`motion-readiness1`/`motion-readiness2` have their own per-node one-use latches
and authorization serials 4000..4029. Each request copies the actual current
binding and node epoch, including a configuration change from an earlier PDO
experiment. Constructor/startup/cached dump remain passive.

Reopened all thirty original EDS sections, EN86 PDF pp11-12/16-19 and ZH86 p12.
All inventory addresses exist with RO/RW access in the EDS; no WO controlword or
mode request is uploaded. Their byte widths are explicitly enumerated in the
single application lookup `motionReadinessField`. Raw intake preserves a
conflicting actual width instead of repairing the source; the preview below
conservatively rejects such a conflict.

The lead acquired a successful
[node-1 readiness record](../bench/20261102-motion-readiness-node1/rx.txt),
which this worker inspected read-only. It contains 30 successful uploads and
no motor command. Important measured values are:

| Actual value | Consequence for the next experiment |
| --- | --- |
| `607A=5000`, `60FF=120` | Both retained targets are nonzero. They do not establish a previously accepted trajectory, but changing mode/enabling must account for their possible use. No implicit zeroing or replay is permitted. |
| `6041=8037`, `6061=0`, `2100=1` | Mode zero is undefined. The agreed monitor reports enabled and not running; its report is separate from physical standstill. EN86 bit10 and ZH86 bit14 still disagree under this raw status. No enable action becomes ready from these reads. |
| `1001=0`, `603F=0`, `6064=0`, `606C=0` | Sequential raw error/feedback observations, not an atomic state or independent shaft measurement. |
| `6081=60`, `6083/6084=100`, `605D=1` | Current native parameters. The proposed 1 rpm/1000 ms candidate is an explicit future change; raw halt option1 still supplies no braking promise. |
| `2201=1000`, `2500=2`, `2501=4000` | Actual subdivision, closed-loop selector and encoder-setting words. Neither the 1000/4000 relationship nor the motor's representative 1000 PPR specification establishes command or feedback scale. |
| `2202=0`, `607D:01=80000000`, `:02=7FFFFFFF` | Software limits are not established as a protective motion envelope. No limit setting is changed. |
| `2400:01=0`, `:02/03/04=1/2/3`, `:05..08=0`; `2101=0`, `60FD=0` | Origin/positive-limit/negative-limit functions remain assigned. Zero logical input observations do not physically disable those assignments. No input is cleared or reassigned for testing. |

The earlier limited TPDO mutation is separately retained in
[the lead's 24-service record](../bench/20261102-nmt-pdo-tpdo1/rx.txt).
The lead subsequently reports a real drive-power cycle and readback returning
heartbeat 0/map 3, with no save command. Do not assume the experimental map2 or
1000-ms heartbeat survives that lifecycle. The lead owns the corresponding
exact fixture/session and retained raw evidence summary; no worker operated
the bench.

[`MotionPreview`](../../examples/s2_bench/MotionPreview.h) is an application-only
offline review builder. It accepts only a complete successful thirty-field
readiness record, checks every address/subindex/documentary width, raw read
policy, operation/request/step, binding/node epoch and sequential acquisition
window, and copies the retained target/status/mode values. Its output always
has `executionReady=false`. It contains no transfer identities, grants,
admission loop, controller service or firmware command route.

The ten displayed native vectors retain the documentary position-mode candidate:
write mode 1, upload display 1, enable 6/7/15 with chosen 10-ms scheduling intervals,
target 16, profile 1 rpm, ramps 1000 ms and one 001F trigger. They are byte-checked
with the public codec and independently compared with the actual selected SDO
encoder. The separate 000F->000B stop value is tested with an independent encoder
instance, not as a fake post 001F transition. Its branch requires previously
authorized enable work already completed, fresh continuous control/state evidence
and independent stationary observation; never enable merely to prepare a stop.

The concrete next isolated physical question is the mode 1 transition with the
retained nonzero targets, before any enable/absolute-trigger progression. Its
exact candidate remains one `6060:00` U8=1, followed by fresh `6061/6041/2100`,
errors/feedback and independent shaft observations. The possible retained-target
effect belongs in its physical envelope and power-cutoff procedure. This is
not an automatically admitted command. Do not silently replace it with a
different undocumented initialization order or assert that writing `60FF=0`
has proved stop/release semantics. If the mode transition cannot stay within
the accepted independent controls, leave it pending and acquire applicable
manufacturer evidence. The earlier relative 16 proposal was withdrawn after the
retained target 5000 read: an unproved commanded base could instead imply 5016.
The first offline motion preview now uses absolute native coordinate 16, conditional
on command/native-counter/feedback/reference qualification. Actual 6064=0 alone
does not prove small travel or a physical angle. Relative-base behavior remains
a separate pending experiment; it is not used by this first candidate.

Host verification after this extension passed **10029 raw-collector checks** and
**536 preview checks** under strict C++11 GCC. The preview negatives include
address/subindex/operation/step/read-policy/width corruption at every one of the
thirty fields. All changed C++ files were formatted with clang-format14; Python
`--help` lists the two new upload-only host choices. The lead owns final combined
native/package/Doxygen/firmware verification and the live image/source manifest.

The later read-only source refresh found MotorControl-RS at
`776facd9a8ce3c8f9d6070b24a588338ac94fb88`, clean; its change from `4e299d2`
is an audit entry, with no delta in the inspected Position/tests/examples/runner/
CI seams. Current `Position.cpp`, `position_test.cpp`, verification runner and
FieldCore's actual move admission/reference consumers were inspected. RS retains
its own evidenced native limits and trigger uncertainty; CAN does not import its
subdivision-to-command-rate conversion. ADS1115, INA228 and FieldCore stayed clean
at the revisions in the earlier table; their callback, cancellation, partial-effect
tests and consumer seams were refreshed without modification or rerunning their
unchanged suites.
