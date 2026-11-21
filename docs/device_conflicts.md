# Device evidence questions and acquisition gates

Reviewed 2026-11-02 for prompt 29. Subsequent [live S2 acquisition](reports/2026-11-02_s2_live_connection.md)
confirmed both CL86-C nodes and captured 20 replies per node. One-byte
`1009=0x0C` / `100A=0x20` contradict the EDS U16 declarations; identity count 5
exceeds the EDS limit 4, and the actual TPDO1 layout differs from its template.
The report retains raw identity/configuration without inferring human version
encoding, NMT state or motion/PDO mutation semantics. The later [NMT and TPDO checkpoint](reports/2026-11-02_nmt_pdo_checkpoint.md) adds the measurements in the physical column below. The authoritative decisions remain in
[the reviewed object ledger](reference/lichuan_objects.json), including its
`semantic_review.gates` Q01-Q21 and conflicts C01-C24. This guide connects those
gates to the next bounded observation; it does not approve a firmware dialect
or replace the ledger. Q01 retains the observed wire-width conflict without
overwriting the original dictionary. [The bench record](hardware_bench.md) owns fixture and
admission requirements.

## Source and evidence boundaries

Prompt 30's [state/action review](drive_actions.md) narrows enable and release
questions without closing a gate. Position 6/7/15 order is documented; its missing
input is established initial/retained-motion provenance, not the sequence itself.
Release needs a sourced CAN command, starting states, new observations and
torque/reference consequences. Neither low status bits zero nor an acknowledged
`6040=0` supplies that contract. The new passive decoder cannot establish these
facts by selecting a document layout.

EN86, ZH86 and OP86 mean the retained
[English manual](reference/vendor/86C_CANopen_user_manual_en.pdf),
[Chinese manual](reference/vendor/86C_CANopen_user_manual_zh.pdf) and
[operating manual](reference/vendor/CL86C_OL86C_operating_manual_zh.pdf).
EDS means [LC_StepDriverCAN_V1.1.eds](reference/vendor/LC_StepDriverCAN_V1.1.eds).
ZH57 means the [57C contrast manual](reference/vendor/57C_CANopen_user_manual_zh.pdf).
Pages below are one-based PDF pages. Original bytes and provenance are in
[SOURCES.json](reference/SOURCES.json); none was replaced by this review.

The prompt-29 review reopened the original EDS sections and rendered the
conflicting status, DO, velocity, position/ramp and clear/save tables and
procedures. The original pages confirm the existing disagreements. OP86's
86C cover and internal 57C headers remain an applicability conflict. Agreement
between manuals is documentary evidence only; no retained source binds these
claims to an installed firmware revision. No new resolution is asserted.

For the host column, **R** means the existing typed-read contracts exercise raw
widths, ownership, validation and terminal evidence; **C** means codecs or
host-side contracts cover a representation, not the device behavior in question;
**P** means the dependent physical behavior remains pending. These refer to
[read coverage](reports/prompt_19.md) and the relevant settings/owner reports,
not a new claim that a motion API exists. The physical column distinguishes
the acquired raw values from pending behavioral qualification; neither a fake
SDO reply nor a firmware build is a physical observation.

The executable [offline procedure](../examples/device_evidence/README.md) accepts
only `device_evidence --fake-only`; it demonstrates bounded collection and
failure evidence through the ordinary owner. Its synthetic replies/review
references resolve none of the gates below. The [bench guide](hardware_bench.md)
describes the actual harness API, retained records and firmware integration.

## Questions in plain language

These questions explain what could happen on a real drive and what we can do
about it. The [evidence table](#conflict-and-observation-table) has exact CANopen
settings and test results. The [owner-return checklist](prompts/README.md#deferred-to-owner-return)
lists the bench work that needs someone present.

### Identify the drive and trust its status

#### Which drive model and reply format should we use? (Q01, D18)

**Example.** The CL86-C and DS-C have the same vendor and product numbers in
their Electronic Data Sheets (EDS files, the lists of CANopen settings). Setting
`2300` has different meanings on those models. Separately, the tested CL86-C
drives answer some identity queries with one byte where the EDS says two.
Some count queries go the other way and return two bytes. The count is zero
even though the numbered entries can still be read.

- **Name the exact model at setup and use the documented format by default.**
  This avoids choosing the wrong meaning for a write. Setup needs the correct
  model, and the unusual replies remain unreadable through the default API.
- **Enable the existing reply-size exceptions where the measured behavior
  applies.** This lets us read those fields on the matching drives. It adds an
  explicit setup choice, and the meaning of the reported zero stays open.

The [backlog](backlog.md#documentary-and-firmware-questions) records the model
conflict and the affected count fields.

#### When can we say a move finished or a fault appeared? (Q02)

**Example.** A drive may still report the previous move's "done" flag when the
next move begins. Treating that flag as current would report arrival while the
shaft is still moving. The manuals also disagree on which flags mean fault or
motion, so those meanings need checking on the exact firmware.

- **Verify the flags on the exact firmware and tie them to the current
  command.** This yields a usable result and needs traffic captures alongside
  an independent observation of the drive.
- **Show the raw flags when their meaning or timing is unknown.** This keeps
  diagnostic information visible while the application treats the outcome as
  unknown.

#### Could reading an error erase evidence? (Q20)

**Example.** A background read of the fault history (`1003`) could change what
a technician sees later. Seeing the same reply twice leaves that possibility
open.

- **Keep routine reads within the already accepted list.** Existing monitoring
  continues, with fewer fields available for automatic polling.
- **Ask the manufacturer or verify one field with a before-and-after check.**
  This can add that field to routine monitoring, once the reply's effect is
  understood. It needs exact vendor evidence or an independent observation,
  such as watching the fault indicator while a reviewed read runs.

### Know what motion and input/output signals will do

#### Which way will the motor turn, and how far? (Q03, Q04)

**Example.** The application asks for one positive turn. The drive may count
requested motor steps and measured shaft position in different units, and its
positive direction may differ from the machine's.

- **Use the drive's own units for routes that already work.** This starts with
  the available measurements and leaves conversion to the application.
- **Measure both scales and the direction on the real shaft.** This lets the
  application use turns or degrees. Record the motor and the drive's steps per
  revolution setting so the result can be checked after setup changes.

#### When is a travel limit or home position valid? (Q05, Q14, Q21)

**Example.** The drive restarts with the positive travel switch held. It should
refuse a move farther in that direction. Can it move away, and will finding
the home switch establish a known coordinate? Setting the position to zero
alone gives neither answer.

- **Refuse starts while the switch state or position reference is uncertain,
  then verify one way to find home.** This has fewer ways to start in error
  and can require someone at the bench to recover.
- **Allow a verified move away from an active limit and add drive-side
  position limits.** This can let the machine recover itself. It needs exact
  wiring and direction checks, evidence of how the drive reacts, and a rule
  for when a restart or settings change makes the home position unknown again.

#### Which terminal changes, and how short a signal can the drive see? (Q06-Q08, Q19)

**Example.** Turning on digital output 0 (DO0) should leave output 1 as it was.
A quick tap on a limit input might also be visible on the wire and too short
for the drive's input filter to report.

- **Measure known terminals with one steady signal at a time.** This checks
  which setting controls each terminal, which electrical level means active,
  and whether the output command preserves the other outputs. A meter is
  enough for steady states.
- **Apply timed pulses and record the terminal and the drive's report.** This
  can establish which short signals the drive recognizes. It needs a
  repeatable source and a capture with a shared time reference.

#### Which settings and operating modes are usable in production? (Q09, Q10, D17)

**Example.** A disabled drive may echo an experimental mode in its mode
display (`6061`). The echo alone leaves motion, torque and stop behavior
unknown. A setting described as writable likewise leaves its useful range
unknown.

- **Expose verified reads and keep writes and modes to the verified set.**
  This keeps setup predictable and leaves optional features unavailable.
- **Verify one additional mode or setting for a real application.** This adds
  the needed feature and confines testing and maintenance to that feature.
  The [backlog](backlog.md#documentary-and-firmware-questions) lists the
  mode-display observations and their scope.

#### What counts as a successful start, stop or fault clear? (Q11-Q13)

**Example.** A start reply is lost. The drive may already be moving. A reply to
a subsequent stop can arrive while the shaft is still slowing, and an old
fault code can still be visible during a later fault-clear request.

- **Report uncertainty, keep the original command from being sent again, and
  use a separately supported stop when available.** Require fresh evidence
  before another start or fault clear. The operator may need to check the
  drive's state.
- **Verify each missing transition on its own.** This can support more cases.
  It needs independent observation of the actual motion, stopped state, or
  holding torque relevant to the operation.

#### Can we change a move while it is running? (Q11, Q22-Q26)

**Example.** The application asks for a lower speed before the same endpoint.
The drive may accept the setting and continue at the original speed. If the
application asks for a zero ramp time, it is also unclear how the drive
interprets having zero time to speed up or slow down.

- **Use the verified move and replacement behavior already supported.** This
  keeps timing and completion easier to check and can require waiting or
  stopping before issuing a new command.
- **Verify one specific running update at a time.** A verified update can
  avoid an interruption. Measure when it takes effect, whether it waits in the
  drive, and how it interacts with a subsequent stop.

### Change configuration or recover from loss

#### Which settings and address will the drive use after restart? (Q15, Q16)

**Example.** A drive at address 2 acknowledges an address change. After
restart it could answer at the new address, still answer at 2, or follow the
small hardware address switches (DIP switches).

- **Use a known hardware address and leave runtime changes unsaved.** This
  makes it easier to return to the previous setup and may require reapplying
  settings after boot.
- **Verify one save or address-change procedure with someone at the bench.**
  This enables repeatable deployment. It needs a backup, a way to recover the
  drive, and a comparison of the settings before and after restart.

#### Can the controller trust updates immediately after changing their layout? (Q17)

**Example.** A PDO is a short CANopen message that carries values such as
position or a command. Its map says what each byte means. When the map
changes, an old eight-byte message can arrive late on the same CAN message
number (CAN ID). The new map could read old position bytes as a different value.

- **Keep a fixed, verified layout where it carries the needed values.** This
  avoids the transition and limits customization.
- **Verify how to clear old traffic before using the new layout, or use a
  separately verified CAN ID.** This allows a change and needs an observed
  cutoff, a distinct message number, or another reliable way to tell the layouts
  apart.

#### Can the controller recover its link, and what happens to a moving drive? (Q18)

**Example.** The cable is pulled during a velocity run. The controller knows
it lost contact while the shaft may still be moving. After reconnection, a
late reply can look like the answer to a new request for the same setting.

- **Keep new requests on that channel blocked if replies must match with
  certainty.** This protects result integrity and can delay recovery.
- **Use the already accepted marker option when that risk is acceptable.**
  It sends a distinct, reviewed read before reopening the channel. This can
  restore service sooner. It assumes one requester and a drive that answers
  in order, so the result still carries a risk marker until those assumptions
  can be verified.
- **Reopen once the controller has finished its own transmissions.** The
  existing option without a marker avoids waiting for a separate reply. An
  old reply can still be taken as the result of a new request for the same
  setting. [Request recovery options](canopen_services.md#settling-and-reopening-a-route)

If the machine needs the drive to stop on link loss, verify the separate
wired stop input for that use. [Host-loss behavior](host_loss.md)

#### Do two drives sample or start together, and can a queued start be withdrawn? (Q27, Q28)

**Example.** The controller sends SYNC, a shared timing message. Drive A and
drive B each send a feedback message (TPDO) just afterwards. Drive B may still
be reporting a position sampled earlier. In a separate case, Drive A may have
stored a start sent in a command message (RPDO) for the next SYNC, even though
Drive B never received its start. A later SYNC could start A by itself.

- **Keep the starts independent and label when feedback was received.** This
  avoids relying on the drive's stored start. Starts can be separated by time,
  and reception time leaves the true sample time open.
- **Verify sample timing and queued-start withdrawal as separate questions.**
  That can reduce the time between starts and improve comparisons between
  drives. It needs independent timing evidence and proof that a later SYNC
  cannot release a start that the application tried to withdraw.

## Conflict and observation table

All gates below are open. Q20 applies to every proposed upload, including
identity: an RO access flag does not establish that reading is non-consuming.
The DEC-1 accepted list lets those reads proceed as owner-accepted risk; it
closes Q20 for that list as a project decision only, not as a device fact.
An entry in the observation column is a candidate for a reviewed acquisition,
not permission to transmit it. Every conditional experiment also requires the
[finite experiment prerequisites](#conditional-experiment-prerequisites).

| Gate / disputed fact | Original source claims and vendor agreement | Raw observations needed and conditional experiment | Affected capability and exact gate | Host | Physical |
| --- | --- | --- | --- | --- | --- |
| Q01 / identity and firmware | EDS `[1000]`, `[1009]`, `[100A]`, `[1018sub0..3]`, `[2000]`, `[2001]`; EN86 pp15/17, ZH86 pp15/17. Numeric widths agree; no code-to-firmware binding. `1018:04` absent despite count limit 4. C18 limits OP86 applicability. | Label/serial and manufacturer firmware correspondence; exact nine typed identity replies, route and configuration. Keep missing/aborted fields. No index sweep or fabricated serial field. | Model-specific interpretation/effects require an applicable binding. Raw identity may precede binding only under Q20 admission (`1000`, `1018:00..03`, `1009`, `100A`, `2000`, `2001` are on the DEC-1 accepted list). | R | Measured, two specimens: U8 `1009=0x0C`, U8 `100A=0x20`, `1018:00=5`, `1018:01..03=0x1000/1/0`, against EDS U16 defaults 0x2608/1 and count limit 4, default 3. The default typed read (`DECLARED_U16`) of `1009` ended with client abort 06070010, uncertain. Decision: the original EDS is unchanged. `IdentityWidths::MEASURED_U8` is an explicit application option for a fixture it has established; nothing detects the width. `1018:00=5` stays raw, no version text is inferred, and `hardwareVersionWidth`/`softwareVersionWidth` report the reply bytes. Identity stays unbound and non-unique |
| Q01 / array count widths `607D:00`, `60FE:00` | EDS `[607Dsub0]` and `[60FEsub0]`: RO U8 HighestSubIndex, default 2. No manual page gives another width. | Whether the firmware holds a U16 or pads a U8, and what the count of 0 means while `607D:01/:02` answer as I32 (raw uploads 2026-11-02, `20261102-final-node1/2`). `60FE:01/:02` were read on both specimens under the measured count width, as U32 0 and 0 (trials 608, 609). A manufacturer statement, or a reviewed read of each child after the count. No subindex sweep. | `DRIVER_SOFT_LIMITS` and `IO_OUTPUT_COMMAND` typed reads under the default `DECLARED_U8`. With `CountWidths::MEASURED_U16` neither is blocked any more: the 2202 write (packet 12 T1/T2), which needs a fresh complete SOFT_LIMITS read, ran on both specimens (trials 604 to 607), and the I/O command readback group (packet 13 P1) completed (608, 609). The count's meaning stays unresolved. | R | Measured 2026-11-04, two specimens of one firmware identity (`1000=0x00040192`, `1009=0x0C`, `100A=0x20`, `1018:01..03=0x1000/1/0`, `2000=0x0400`, `2001=0x0200`): each count is an expedited two-byte 0, `4B 7D 60 00 00 00 00 00` ([node 1, trial 313](bench/20261104T1405Z-settings2202-node1-313/rx.txt), [node 2, trial 315](bench/20261104T1405Z-settings2202-node2-315/rx.txt)) and `4B FE 60 00 00 00 00 00` ([node 1, trial 303](bench/20261104T1309Z-io-node1-303/rx.txt), [node 2, trial 304](bench/20261104T1309Z-io-node2-304/rx.txt)). Old claim: the EDS U8. The default typed read (`DECLARED_U8`) ended with client abort 06070010, uncertain, which leaves the route unresolved until an MCU restart. Decision: the original EDS is unchanged. `CountWidths::MEASURED_U16` is an explicit application option for a fixture it has established, for those two group reads only; nothing detects the width and a one-byte reply then fails. `softLimitEntriesWidth`/`outputEntriesWidth` report the reply bytes. The 0 stays raw: not an empty array, not a discovery bound. Its meaning is unresolved. Under `MEASURED_U16` both groups completed on both specimens (604 to 609): `607D:01/:02` at the signed extremes, `60FE:01/:02` 0 and 0 |
| Q20 / read effects | EDS access metadata and EN86 pp15-18 / ZH86 pp15-19 establish readable values, not upload side effects. C06/C07 preserve monitor/error access conflicts. No non-consuming guarantee found. | Manufacturer statement per selected object, or explicitly authorized matched-firmware experiment with independent before/after diagnostic/event observation and a specified finite number of uploads. Stable repeated values alone cannot rule out consumed events. | Polling and ordinary reads stay blocked for every row not on the DEC-1 accepted list (`1000`, `1018:00..03`, `1009`, `100A`, `2000`, `2001`, `6041`, `6061`, `6064`, `606C`, `2100`, `2101`, `2102`). DEC-1 is the owner's risk acceptance of 2026-11-02, not read-effect evidence; `read_effect` stays `not_established`. Packet 11 read all 16 listed rows live on two specimens, with no read-effect observation. Packet 15 adds one `607A` and one `60FF` upload per enable as `accepted_action_prerequisite`, a bounded prerequisite and not a poll; both specimens ran an enable attended on 2026-11-03 (trials 108, 118), which is no read-effect evidence. `603F`, `1001`, `1003` stay unqualified explicit single reads, never polled; their first bench reads (trials 128, 129, 203) recorded the fault LED as unobserved. Ordinary typed read success retains unknown effects; it does not close Q20. Owner prompts 19, 29, 37, 41, 44, 47. | R | Raw uploads completed under one-use experiment grants. Packet 11's typed identity and soak reads, and its 30 raw readiness uploads, succeeded on both specimens. Stable repeats and successful reads are not read-effect evidence |
| Q02 / state, fault and done bits (C05) | EN86 p12: alarm bit7, zero-speed bit10, target-effective bit12. ZH86 p12: alarm bit3, running bit14. OP86 p10: alarm bit7, running bit14. All show homed bit8 and position-done bit15. Low bits 0..2 are examples, not a complete CiA402 decoder. EDS `[6041]` is U16 RO. | Raw `6041`, `2100`, `603F`, mode and independent enabled/released, motion, standstill and fault observations, with timestamps. Observe bits8/15 before, during and after a separately approved finite operation; distinguish pre-existing done from current-command completion. Do not induce an electrical fault merely to obtain a bit. | Interpreted state, fault, stop, move and home completion require exact firmware dialect and freshness evidence. No union mask or generic fallback. Packet 19's bench `position` route is the acquisition path for acceptance and completion; it ran on node 1 in the attended session ([packet 19](reports/handoff_19_33/packet_19.md)). | R; P | Measured, two specimens: raw `6041=0x8037`, `2100=1`. Decision: raw only, no dialect or enable claim. No independent shaft or torque observation was retained. Attended 2026-11-03, node 1 ([report](reports/2026-11-03_attended_bench.md)): idle `8437`, moving `5037` (bit 14 running, 12 set, 15 and 10 clear), done `9437`; bit 12 latched until the `000F` rearm; `2100` `0001`/`0004`/`0001`. After a HALT short of the target, `0437` and `2100=0000`: neither bit 15 nor in-place. EN86's bits 10, 12, 15 and ZH86's 14 all appear: neither table alone. One specimen and firmware. Unattended 2026-11-03, node 2 ([report](reports/2026-11-03_unattended_bench.md)): the library `move` saw `5037`/`0004` in its first cycle and `9437`/`0001` at completion, node 1's bits, then `8437` after its rearm. After a stationary `000B`, `8050`/`0011` (node 2, from `000F`) and `0050`/`0010` (node 1, from `010F`): low bits 000, bit 6 set and not interpreted, bit 15 unchanged |
| Q03 / velocity sign and start (C11/C17) | EDS `[60FF]` I32 and EN86/ZH86 p17 I32 conflict with p13 U32 0..3000 rpm. EN86 p13 enables before parameters then refers to bits4..6; ZH86 p13 and OP86 p11 set parameters then enable. | Readback alone cannot establish direction/start. One independently reviewed candidate sequence must specify signed bytes, actual start transition, current mode/state and retained target; observe shaft direction/speed and raw status. Positive, negative and zero each need scoped evidence. Never try both recipes as a search. | Production velocity start, signed range, mode/enable transitions and zero-target meaning gated. A signed codec is not negative-speed support. | C; P | Measured, two specimens, after a power cycle: `60FF=120`, equal to the EDS default. No write or pre-cycle read in any retained record. Not evidence of retained intent |
| Q04 / subdivision and feedback scale (C12/C21) | EN86/ZH86 p11 `2201` 0..65535 versus p18 400..51200; EDS `[2201]` default0. `607A` is I32 pulses; p11/EDS correct p16 `I3C` typography. EDS `[6064]`/`[606C]` I32; EN86 p3's 1000-line encoder is not command/feedback scaling. | Actual `2201`, `2500`, `2501`, `6064`, `606C`, motor/gearing and firmware. Independently measured displacement/speed for a finite approved target establishes separate command and feedback scales/sign. Accepted subdivision set/range needs manufacturer evidence; do not sweep settings. | Physical/angle conversion, feedback units and subdivision setters require nonzero applicable scale and configuration provenance. Never assume 4000 counts/rev. `2200` and `2201` have no typed write: p18 labels `2200` without saying what its codes do under CAN. See [setting decisions](driver_settings.md#setting-decisions). Packet 19's bench `position` route is the acquisition path, with exposure bounded independently of the scale being measured; it ran on node 1 in the attended session ([packet 19](reports/handoff_19_33/packet_19.md)). | R/C; P | Attended 2026-11-03, node 1: `2201=1000`, +250 pulses turned about 1/4 turn counter-clockwise from the shaft end, and `6064` ended at 250, the command's units. `606C` read 1 at 2 rpm, unit open. Node 2's scale is not measured: its unattended move to 250 had no shaft observation, and completion was reported at `6064=251`, then 250 ([report](reports/2026-11-03_unattended_bench.md)). Later `606C` read 30 at 30 rpm (trials 213, 413) and -60 at -60 rpm (706): signed rpm as reported by the drive ([2026-11-04](reports/2026-11-04_bench.md), [2026-11-05](reports/2026-11-05_motion_and_stress.md)). Night soak 2026-11-11, both nodes, modes 1 and 3, 1,026 cruise intervals at 20 speeds from 6 to 300 rpm: the `6064` rate on the S2 clock fits floor(rpm × 1000/60) × 0.99201 counts/s within 0.0002 at every speed, and `606C` reads in 2 rpm steps at cruise. Heartbeat and TPDO timers run within 0.04 % of the S2 clock, so the shortfall is not a ratio between the S2 and the drive's communication timer. Cause unknown; counting shaft turns over a +100,000 pulse move would separate a pulses-per-turn difference from a rate shortfall; rpm at another `2201` is unmeasured ([units re-audit](reports/2026-11-13_units_reaudit.md) section 3) |
| Q05 / software limits (C13) | EN86/ZH86 p18 `2202`: 0 disabled, 1 effective after homing; EDS `[2202]` default10. EDS `[607Dsub1/2]` I32 bounds; EN86 pp16-17. | Preserve actual enable code and bounds. Establish coordinate basis, homing prerequisite, inclusive/exclusive boundary and response with independent travel protection before any isolated boundary test. | Limits cannot be relied on as protection or written from default10; matching readback does not prove reaction. `2202` has a typed write of codes 0 and 1 from a documented prior code; `607D` has none, as no source gives it a domain, and closing either needs a manufacturer range or coordinate basis first. See [setting decisions](driver_settings.md#setting-decisions). | R/C; P | none |
| Q06 / DO addresses and functions (C01/C02/C04) | EN86 pp8/18, ZH86 p8, OP86 p6: polarity `2400:0B`, DO0..2 `:0C..0E`. EDS `[2400subB..F]` and ZH86 p19: `:0B` reserved, polarity `:0C`, DO0..2 `:0D..0F`. EN86/ZH86 p8 and OP86 p7 assign 2 running/3 homed; EN86 p18/ZH86 p19 assign 2 homed/3 drive status. Polarity table specifies only bits0/1. | Obtain exact firmware map first. Record existing assignments and isolated terminal voltage/state. Only then consider one exact reviewed assignment or polarity experiment on an identified terminal, with brakes/actuators isolated. Never write both disputed addresses, infer bit2 or reinterpret PA labels as CAN indexes. | DO assignment/polarity/function2/3 writes and interpretation remain gated per firmware. Raw equal values do not identify which address is correct. | R/C; P | none |
| Q06/Q07 / output command and mask | OP86 p7 says `60FE:01/:02` bits16..18 both1 assert DO0..2; EDS confirms RW U32, not a full truth table. ZH57 pp8/17 disagree DO0..2 versus DO1..3 (C03); this does not resolve 86C. | With exact terminal binding and isolated load, capture command/mask, `2102` and measured terminals for a finite reviewed set/clear/unchanged-bit case. Require a source-backed exact command/mask candidate, independent control and explicit review/authorization; the remaining clear/preserve effect may be the declared experiment question. Never guess the disputed function address or trial both layouts. | Command/mask clear semantics, unchanged-bit preservation and physical terminal mapping gated. `2102` bits0..2 are not `60FE` bits16..18. | R/C; P | none |
| Q08 / input filters (C15/C16) | EDS `[2400subA]` reserved versus EN86 p18 filter coefficient. EDS `[2401]..[2407]` defaults50/40/100/4000/10/2/16 versus EN86/ZH86 p8 all2. No time equation supplied. | Applicable firmware map, accepted range/unit and existing filter values; independent timestamped input stimulus only after reviewed electrical and function assignment. Do not write reserved `:0A` or derive milliseconds from a name. | Filter setters and time interpretation gated; documented raw observations do not resolve coefficients. | R/C; P | none |
| Q09 / tuning and feedback | EN86/ZH86 pp19-20 cover `2500..2519`; EDS `[2500]..[2520]` all RW U16 without ranges/defaults. `251A..2520` have EDS-only names. Current mA/percent and some timing units do not supply safe operating bounds. | Manufacturer model/motor-specific ranges, loop applicability and units for each chosen field; existing raw values, ratings and measured effect only under an individually bounded tuning experiment. No automatic tuning or current sweep. | Each affected setting/conversion remains gated independently; no torque capability inferred. | R/C; P | none |
| Q10 / mode/method encoding (C09/C10/C22/C23) | EN86 pp11/13/16-17 and EDS `[6060]`, `[6061]`, `[6098]` disagree I8/U8/access. Positive modes1/3/6 and methods17/18/24/29 share encoding. `6099:02` p13 says creep time with rpm; p14 prose says speed. | Preserve raw `6061` and `6098`; obtain applicable semantics before interpreting high-bit codes or method0. Use mode display as an observation, never obey translated instruction to write `6061`. | High-bit/negative codes and default method0 unsupported; valid positive encoding does not authorize a mode transition or homing. | R/C; P | Measured, two specimens: U8 `6061=0` in every acquisition, equal to the EDS default. EN86 p11 lists 0 as Undefined. Decision: treat mode 0 as not an operating mode or enable predecessor, semantics unqualified; never write `6061` |
| Q11 / enable, target edge and rearm (C08) | EN86/ZH86 p12 and OP86 pp9-10 show controlword6/7/15, approximately10 ms spacing, bit4 rising-edge start and bit5 update behavior; EDS `[6040]`/`[6060]` WO versus manual RW. No complete acceptance/rearm handshake or queue contract. | Per-mode/current-state retained-target behavior; raw target/status and independent motion capture around one approved edge and explicit return. Determine when the next edge is admissible and whether old done survives. Lost-ack cases remain uncertain, never replayed. Also measure home-then-move: no source describes changing `6060` while enabled (D-02), and prompt 36 owns any explicit mode 6 enable. | Enable/release, target rearm, fresh move completion and immediate-update behavior gated. No command read-modify-write, invented FIFO or fixed device deadline. | C; P | Measured, two specimens, after a power cycle: `607A=5000`, equal to the EDS default, with no write or pre-cycle read in any retained record. Effect on mode change or enable unknown. Decision: no implicit zeroing, replay or blind enable. Attended 2026-11-03, both nodes: holding torque at power-up before any word; word 6 released (`8031`, `2100=0011`), word 7 energized (`8033`), word 15 gave `8437`. `000F` after completion only cleared bit 12; nothing moved. Unattended, node 2: the library move's own `000F` rearm was acknowledged and the next read was `8437` at 250 ([report](reports/2026-11-03_unattended_bench.md)) |
| Q11 / zero ramps | EN86/ZH86 pp11/13 and EDS `[6083]`, `[6084]`, `[609A]`: U32 ramp times, mode-table range0..2000 ms. No zero behavior or physical acceleration formula is stated in words. Found 2026-11-03 (packet 19 page review): the OP86 p9 curve starts and ends its ramps at a labelled nonzero 起始速度 (start velocity), while the EN86/ZH86 p11 curves start and end at zero, and no start-velocity object appears on these pages; OP86 carries the C18 header caveat. Re-audit 2026-11-13, figures zoomed (EN86, ZH86, ZH57 p11; OP86, OP57 p9): the acceleration-time bracket runs from the curve's start to the plateau labelled target velocity, the deceleration-time bracket from the plateau back down, so the figure depicts time to and from the commanded speed; position section only, no figure for velocity or homing. EN86/ZH86 p17 name `6081`/`6083`/`6084` profile velocity, acceleration and deceleration, so the rpm and ms units rest on the p11/p13 tables and the p11 figure axes; the EDS names (`accel time`, `decel time`, `homing accel/decel time`) say time without a unit. | Manufacturer zero interpretation, or a separately bounded experiment with independently controlled motion/stop and measured speed/time. Do not substitute a long timeout for evidence. | Zero ramp admission and physical acceleration/deceleration conversion gated; raw ms is separate from host deadline. | R/C; P | Inferred from drive reports, one firmware ([units re-audit](reports/2026-11-13_units_reaudit.md) section 2): trial 713 (200 rpm, 2000 ms; `6064=854` with `606C=101`), trial 709 (300 rpm HALT, `6084=500`, 1392 counts) and round 8's mode 3 HALTs (`6084=200`, 60 and 121 counts) fit a ramp between rest and the commanded speed with a 20 to 30 ms reaction, and exclude a time per fixed 1000 or 3000 rpm. Zero, ramps between two nonzero speeds and the homing ramp are not measured |
| Q12 / halt and quick stop | ZH86 p12/OP86 p10 show15->271; EN86/ZH86 p12 show15->11. EDS `[605D]` I16 default1 with no applicable option meanings; `605A`/`6085` absent. | Manufacturer current-state transitions and option meanings; independently bounded stop test recording submitted/acknowledged time, motion/standstill, remaining targets and settlement. Independent stop must not depend on the disputed transition. | Stop action/terminal guarantees and queue-discard claims gated. Never enable first to stop; no generic option-code fallback. Packet 16: the bench `stop` route first ran twice, unattended, on 2026-11-03. A `QUICK_STOP` completes on the reported release below and keeps it in `monitorObservation`, retested on hardware (trial 208); a moving `QUICK_STOP` completes on standstill (413, 713); a `HALT` still refuses a release. Still unmeasured: HALT from `003F`, stops from `007F`, homing stops and the remaining mode/rate/firmware cases (the measured `005F`, mode 3 and prompt52 quick-stop cases are scoped in the stop guide), and a moving `010F` to `000B` escalation ([stop guide](stop_actions.md), [packet 16](reports/handoff_19_33/packet_16.md)). | C; P | Drive reports on one firmware; torque was not observed. Attended 2026-11-03, node 1: `010F` from `001F` during a 2 rpm move stopped it within its ramp, observed by `2100`; `6064` stayed 316 for 26 s (trial 113; the attendant was not sure about the shaft). Unattended 2026-11-03 ([report](reports/2026-11-03_unattended_bench.md)): a stationary `000B`, the manuals' `15->11` "Emergency stop" arrow from `000F` on node 2 (124), and from the halted `010F` on node 1 (126), which no source documents. Within one cycle `6041=8050`, `2100=0011` (node 2) and `0050`, `0010` (node 1): low bits 000 (the pattern the manuals pair with `6040=0`) and released. Neither moved; node 1 stayed at 316 with `607A=750` retained. In those two stationary unattended trials the drives' reports showed no torque-holding stop state; the manuals' low-bit examples pair no pattern with `6040=11`, so neither manual is contradicted. 2026-11-04 ([report](reports/2026-11-04_bench.md)): a stationary `010F` from `000F` left `6041=8437` and `2100=0001` unchanged (207); `000B` from that `010F` reported released (208); `000B` during moves from `001F` at 30 rpm reported released with `2100` running latched (213 node 2, 413 node 1); `000B` on a released drive changed nothing (307, node 1, four cycles); `000B` at node 2's never-enabled power-up state reported released (503). 2026-11-05 ([report](reports/2026-11-05_motion_and_stress.md)): a HALT at 300 rpm on node 1 (709), which `000F` did not resume (710); `000B` at about 100 rpm while accelerating on node 2, reported released with running latched, then standstill at 896 (713). The user turned node 2's released shaft by hand on 2026-11-05 at 16:42Z, during the R1 soak. Still unmeasured: HALT from `003F`, stops from `007F`, homing stops and the remaining mode/rate/firmware cases (the measured `005F`, mode 3 and prompt52 quick-stop cases are scoped in the stop guide) and a moving `010F` to `000B` escalation. **2026-11-16, prompt52:** node 2 trial52013 acknowledged a positive `60FF` change from 30 to 60 rpm and its original timed `000B` settled from controller reports ([raw record](bench/20261116-p52/31-velocity-change-node2/rx.txt)); trial52034 reported 56/60/60/58/60 rpm after an immediate same-endpoint speed change, then a moving `003F` to `000B` stop settled with fresh zero-velocity/unchanged-position reports ([raw record](bench/20261116-p52/109-long-speed-change/rx.txt), [analysis](reports/prompt_52_measurements.md)). These are one specimen/firmware's controller observations, not physical standstill, stop latency, target retirement or family qualification |
| Q13 / fault clear | EN86/ZH86 p15: controlword bit7 set then manually clear; ZH86 p12/OP86 p10:15->128. EN86 p18 `2301`0/1; ZH86 p18 omits it. EN86 p16/EDS `[603F]`: only FF01/FF05 identified; p20 first three LED faults require restart. Inference, not a source link: p16 names FF05 position deviation and p20 names a resettable position out-of-tolerance alarm, with no code or route on p20 (D-26). `2301` has no documented arming zero (D-27). | Existing fault cause, independently observed clearance, raw error/status and exact chosen route/rearm. Use a manufacturer-approved reproducible condition if a deliberate fault is required. Check whether `603F` keeps a stale FF05 that could admit a clear for a later alarm, and whether a later `2301=1` without a preceding 0 is a new edge. No automatic restart or electrical-fault injection. | Clear route, resettable set and rearm remain gated; never write `603F`, enable as preamble or derive FF02..04 from LED counts. Packet 17 (host only): the action records `codeLink=UNRESOLVED` and sets `valueOneMayRemain` when a possible `1` lacks an acknowledged `0`. 2026-11-13: both routes are `AlarmClearMethod` values with one admission and completion; `2301` cleared node 1's `FF05` once in mode 3 (trial 5323: then `2100=0001`, `6041=8010`), and the bit 7 route has not run. | R/C; P | none |
| Q14 / position clear and offset | EN86/ZH86 p15: `2302`0->1 clears current position, return manually0. EDS `[2302]` U16; `[607C]` I32; p13 home offset range ±1000000. No arbitrary-position setter or full reference-effect promise. | Before/after command/feedback coordinates, independent stationary shaft, home/limit reference and chosen offset application point. Isolate one reviewed clear or offset case; uncertain acknowledgement forbids replay. | Position clear, offset and reference publication gated; zero is not homing or a verified host origin. Packet 18 (host only): admission needs `606C` zero and keeps the `6064` baseline; `valueOneMayRemain` covers a possibly held `1`. Both specimens read `6064=0`, so a first run can show no change. | R/C; P | none |
| Q15 / save and factory reset | EN86/ZH86 p15/OP86 p13: SDO `2300=2` save; EN86/ZH86 p18:1 factory reset,0 ineffective. EDS `[2300]` U16. No completion/endurance/power-loss/complete saved-set guarantee. | Review one exact source-backed action/value, known baseline, independent power control and an independently established recovery route. Declare trigger/rearm, saved-field set, completion or reset/route effects still unknown as the experiment question; they need not be assumed before the test. Retain reads before/after an explicitly approved planned restart. Power interruption is a separate experiment. | Save/reset completion and durable persistence gated. SDO response is not durable completion; no guessed delay, retry or automatic restoration. | C; P | none |
| Q16 / node, bitrate and route (C20) | EN86/ZH86 pp9-10/18 and EDS `[2203]`, `[1200sub1/2]`: configurable address/routes; DIP all-OFF user-defined. EDS DeviceInfo advertises50 kbit/s, manual DIP only125/250/500/1000. Activation and precedence unspecified. | Actual DIP photos, route/bitrate evidence, raw applicable settings and manufacturer precedence/activation. Any change needs exact old/new route, finite loss handling and independent recovery; no scanning or guessed bitrate write. | Node/route setters and50 kbit/s selection gated; declared parameter0 is not a unicast node. | R/C; P | matchednodes1/2at1Mbit/s; changes untested |
| Q17 / PDO mapping and qualification | EDS `[1400..1403]`, `[1600..1603]`, `[1800..1803]`, `[1A00..1A03]`; EN86 pp15-16 mapping tables. Mapping flags/defaults alone do not establish firmware remap restrictions, timer semantics or producer retirement. | Exact active map/count/IDs; applicable NMT/drive restrictions and readback. For a separately admitted remap, capture transition/queued traffic with an evidenced retirement boundary, distinct verified route or observable marker. | Map-content confirmation is distinct from delayed-sample qualification. New host epoch plus same-ID/same-length data cannot prove freshness. Timer changes retain separate gates. | C; P | Measured, node 1 only: six ACKs and nine exact reads, count 2, `60410010,60640020`, slot 3 `60640020`, slot 4 0, COB 181, type 255, inhibit 100, event 0. After one power cycle: count 3 and the original words, with no save or restore frame. Decision: fixed map 3 to map 2 subset in isolated experiment scope; success not promoted. No PDO frame was observed, so samples stay unqualified. Persistence, timers and provenance open. **2026-11-06, both specimens** ([report](reports/2026-11-06_pdo_stream.md)): all 16 records read; the live maps and timers differ from the EDS (TPDO1 inhibit 100 and event 0, TPDO2..4 empty, RPDO1 `6040`/`6060`/`6098`/`607A`, RPDO2 empty). First TPDO frames observed: one on every TPDO at each Start, then TPDO1 periodic after a `1800:05` write, at periods consistent with raw + 1 ms for 10, 100 and 200 (a fit on one firmware; unit, origin and other values open). Where the session read the map (`1311Z`, `1314Z`), payloads decoded by it and matched the SDO reads; constant payloads cannot show freshness. Transition record: [packet 6](reports/bench_closure_18_33/packet_6.md#transition-record-for-packet-7). Samples stay unqualified: the S2 has receive bounds only and no PDO route was configured |
| Q18 / communication, reset and host loss | EDS `[1017]` default0, `[1014]`, DeviceInfo; no `[1016]` consumer or manufacturer host-loss stop guarantee. No documented upload maxima, reset ready delay or bus-off motor reaction. | Capture ordinary owner traffic/EMCY/heartbeat with explicit timestamp basis. Measure whether the server answers one request at a time and in order, which the DEC-3 drain marker assumes (CiA 301 from memory, not vendor-confirmed; no retained Lichuan or CiA text covers it): reply order across a slow write followed by a read, a new request while one is pending, and duplicate delivery. Until then no real route may set the premise. Each deliberate reset, bus fault or host-loss experiment requires its own finite authorization and independent physical control. | Per-firmware service behavior and loss/recovery guarantees remain unqualified. Timeout policy and CAN TX success cannot establish motor state. | C; P | Measured, node 1: `1017` read 0, wrote 1000, read 1000; `701`=7F before and after addressed NMT command 128, so TX is confirmed and no transition is demonstrated. Boot frames 702 and 701 captured with the receive intervals in the checkpoint, in a capture that reported drops, then `1017=0`. One cycle only. Decision: 1000 ms is an experiment value; no host-loss claim. **2026-11-06** ([report](reports/2026-11-06_pdo_stream.md)): both nodes boot Pre-operational (`7F`). Addressed Start, Pre-operational, Stop and Start from Stopped on node 2, Start and Pre-operational on node 1: each heartbeat reported the new state (`05`, `7F`, `04`) at its next scheduled beat, 0.07 to 1.0 s later. Neither drive's reported CiA402 state or position changed, and no EMCY appeared in the watched intervals (`1014` not read). The enabled node 1 was read by SDO every 20 ms for 60 s while Operational (`1316Z`); no RPDO was ever received, so a CiA 301 deadline monitor would not have been armed. No frame arrived at the next scheduled slot after a Pre-operational or Stop (bound 19 to 122 ms after TX confirmation); the order of the last frame against the request is open in `1302Z`. Still no host-loss or watchdog test |
| Q19 / fixture, inputs and defaults (C14/C19/C24) | Three supply ranges inside each manual (D-07): panel artwork (EN86 and ZH86 p4, OP86 p3) and the manufacturer page 30..110 VDC/20..80 VAC; spec tables (EN86 and ZH86 p3) 36..110 VDC/18..80 VAC; wiring (EN86 and ZH86 pp6-7, OP86 p5) 24..50 VDC. The listing is retained only as a URL, so its AC upper bound is unverified. Use the nameplate. OP86 pp5-14 are identical to OP57, so do not count them as independent 86C evidence (D-08). Diagrams label DI3 enabling and DO alarm/completion/brake where tables default to 0, and DI codes 4..6 have no behavior text: treat diagrams as wiring examples and those codes as unqualified stop or release routes (D-28). Editorial: DI code 3 is negative limit on ZH86 p8/OP86 p7 and reverse limit on ZH86 p19; ZH86 p16 also gives `603F` RW; `6099:02` is named creep time (rpm) on EN86/ZH86 p13 (D-29). EN86/ZH86 p8 DI defaults1/2/3 and DO0=1 versus EDS zeros. EDS `[60FD]` U32 is not an independent bit/function map. | Exact label and applicable electrical approval, inspected wiring, input assignments/polarity, physical signals and measured levels. Establish `60FD` mapping independently of `2101`. Resolve only the selected model's supply conflict. | All live work depends on applicable fixture/authorization; unused terminals do not imply disabled functions. Do not disable protection to run a test. | R/C; P | reportedrails/labels/cutoff; functionalpath works |
| Q21 / homing and reference | EN86/ZH86 pp13-15 and OP86 pp11-13:17 negative limit,18 positive limit,24 positive home approach,29 negative approach then leave switch positively; bit4 falling edge interrupts. No numeric reversal/search bounds. | Method-specific actual switch/polarity and independent position/travel observation; raw switch/mode/status/offset and fresh bit8. One finite approved method must bound travel/time independently; interrupted homing must remain distinct from completed/home-reference valid. | Each home method, interruption behavior and resulting reference gated separately. No fabricated backoff distance or generic CiA method substitution. | C; P | none |

## Corrected reading of the November 2 inventory

Old claim, in the [checkpoint](reports/2026-11-02_nmt_pdo_checkpoint.md), the [motion plan](reports/2026-11-02_pdo_motion_plan.md), the ledger and earlier guides: `607A=5000` and `60FF=120` are nonzero retained targets, stale intent or not defaults.

Evidence: EDS `[607A]` DefaultValue=5000 and `[60FF]` DefaultValue=120. `6081=60`, `6083=100`, `6084=100`, `605D=1` and `6061=0` also equal their EDS defaults. The only retained reads are after the power cycle, and the seven non-upload SDO requests in all retained records are one `1017` write and six TPDO1 writes.

Replacement: these are measured reads equal to EDS defaults on two specimens. Whether they are firmware initial values, retained values or anything else is unknown, and their effect on a mode change or enable is unknown. Do not call them retained intent, a firmware default or safe. This changes no gate. None of these observations establishes a status dialect, harmless reads, a safe enable or a fresh PDO sample. The dated reports keep their text with a correction note.

## Conditional experiment prerequisites

The first preference is applicable manufacturer clarification. When a question
cannot be answered by the accepted or source-reviewed reads, prompt 29 permits one
isolated evidence experiment without waiting for completed motion APIs. That
permission to design the experiment is not live authorization and does not
enable a production capability.

Before implementation/admission of an experiment, record all of the following:

1. One question/gate, exact model/interface/firmware and fixture; source-backed
   candidate action and every object, subindex, width, value, order and intended
   effect. No unknown-index sweep or choice between competing write recipes.
2. Exact applicable user authorization, one operator, current state and
   independent physical observation/stop/control. Bounds include time, count,
   travel, speed/current where relevant and retained-data capacity. The disputed
   stop/completion interpretation cannot be the sole admission/termination guard.
3. A finite sequence through the ordinary checked public typed codec/service
   and the same owner; independent review and fake tests for exact successful
   bytes, abort, timeout, stale response, boot-up and interruption before use.
   Experimental support stays in the evidence harness, without a raw sender or
   replacement motor engine. Unsupported actions stay blocked until this exact
   path can validate them.
4. Explicit failure branches and reconciliation. A submitted action without
   confirmed acknowledgement may have executed. Retain uncertainty; stop local
   sequencing, use the independent fixture control as planned, and do not retry,
   reset, re-enable or restore settings automatically. Local cancellation is
   not a motor stop.
5. Retained raw request/response/event traffic, intent and step identities,
   controller outcomes, timestamps/service gaps, overflow/resource evidence and
   independent observations. A successful repeat never erases a prior failure.

Finite experiments have run: one-use explicit uploads, the `1017` write,
addressed NMT command 128 and fixed TPDO1. Later specimen-scoped motion
experiments are recorded in the [velocity contract](velocity.md) and bench
reports; they do not establish all qualification prerequisites or other models.
The [prompt47 campaign](reports/prompt_47.md) separates current communication
evidence from unavailable physical observations. [Prompt48](reports/prompt_48.md)
adds finite read/output load only: no Q15 persistence, host-loss stop, power-up
torque or cross-model conflict is resolved by it. When new evidence arrives,
record its fixture/firmware scope and limitations, review the canonical ledger
first, then update affected profile gates, generated declarations, meaningful
tests, Doxygen and guides together. A measured result for one specimen never
silently qualifies another Lichuan model, N5 or EPOS4.

## Prompt 51 SYNC evidence gates

Q27 and Q28 are recorded in the canonical ledger before dependent implementation.
EDS `1005`, `1400:02`..`1403:02`, `1800:02`..`1803:02` establish stored
width/access/defaults, not supported synchronous behavior. The [source review](reports/prompt_51_sources.md)
separates original manufacturer claims from CANopenNode/N5 design contrasts.

| Gate | Explicit experiment | Required evidence before qualification |
| --- | --- | --- |
| Q27 synchronous TPDO | TPDO1 type 255 to 1, finite host SYNC, separately requested restore to 255; no save | Count, latency and loss on the selected fixture; independently establish same-cycle sample provenance and remote/host retirement. Processing order alone is insufficient. |
| Q28 synchronous RPDO | Reviewed target-only no-motion map first; controlword start only after M09 review | Buffering, last-wins, event timer, all-RPDO final local TX before SYNC, and actual remote-buffer withdrawal. An asynchronous controlword is not evidence of withdrawal. |

After a possibly buffered start, every later SYNC stays inhibited until the
reviewed withdrawal boundary is established. Host recovery cannot clear that
hazard. No present trial qualifies 10 ms operation; the sustained two-axis trial
also requires prompt 56's throughput work.

P51 bench update (2026-11-16): node 1 accepted the explicit TPDO1 type 255-to-1
change and restoration. Twenty SYNCs at 100 ms and twenty at 10 ms each produced
twenty TPDO1 frames. The [retained report](reports/prompt_51.md) separates this
observed response from Q27's still-open same-cycle and sustained qualification.
Q28 remains unmeasured; no RPDO was sent.

## Prompt 52 changes during motion

Q22-Q26 are recorded in the canonical ledger before dependent implementation.
Original EN86/ZH86 p12-13 and EDS descriptors were rechecked on 2026-11-16;
[the source study](reports/2026-11-12_on_the_fly_changes.md) separates their
claims from proposed experiments. All five gates begin open. Explicit experimental
admission is not firmware qualification and never opens the ordinary capability.

| Gate | Required observation | Smallest affected feature |
| --- | --- | --- |
| Q22 | Bare607A/6081/6083/6084 effects; mid-curve bit4 falling edge, raw6041 bits10/12,6064,606C | Parameter staging and falling-edge replacement |
| Q23 | Immediate003F same-endpoint speed and ahead-target replacement; fresh acceptance/completion and stop | Immediate replacement |
| Q24 | Buffered001F ahead-target behavior, consumption and queue capacity | Buffered replacement; one outstanding only |
| Q25 | Positive60FF change during an existing run, observed response and subsequent stop | Finite velocity change |
| Q26 | Ramp response for different-size changes and stop after changed6084 | Ramp changes and stop-curve claims |

Each finite trial retains exact bytes, field acknowledgements, rejected and
unknown effects. A local token or old latched bit12 never proves acceptance of
the replacement. Relative mid-curve targets, reversal and zero/sign-changing
velocity stay refused (F15). Disabled mode-display probes for2/4/7/8/9/10 are
separate: acceptance by6061 establishes no supported motion mode.

Prompt52 trial52013 on node2 acknowledged a positive60FF change30 to60rpm,
reported60rpm in a later full cycle, and completed its original finite quick
stop. The [raw record](bench/20261116-p52/31-velocity-change-node2/rx.txt)
retains the exact firmware tuple and original stopDue/deadline. This establishes
one positive controller response and stop, not a ramp law, physical speed,
reverse response or a qualified family-wide capability. Q25 remains open for
those limits and finished-operation qualification in54. Node1 accepted/displayed
all six disabled-mode probe values; none was enabled or advertised as supported.

Initial position measurements now cover all eight cases, plus a longer immediate
speed change and moving stop from `003F`. Bare parameter/target writes were
acknowledged without observable active-curve effect; falling bit4 cleared bit12
while motion continued. Immediate and deferred target extensions differed in
their sampled trajectory near the old endpoint. The [raw analysis](reports/prompt_52_measurements.md)
records each disposition, exact firmware tuple and limits. These observations
do not establish queue depth, acceptance latency, physical ramp laws or
finished-operation qualification. Ignored active-change capabilities stay
closed; explicit experiments remain distinct from measured production gates.
