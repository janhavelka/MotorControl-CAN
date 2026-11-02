# Device evidence questions and acquisition gates

Reviewed 2026-11-02 for prompt 29. Subsequent [live S2 acquisition](reports/2026-11-02_s2_live_connection.md)
confirmed both CL86-C nodes and captured 20 replies per node. One-byte
`1009=0x0C` / `100A=0x20` contradict the EDS U16 declarations; identity count 5
exceeds the EDS limit 4, and the actual TPDO1 layout differs from its template.
The report retains raw identity/configuration without inferring human version
encoding, NMT state or motion/PDO mutation semantics. The authoritative decisions remain in
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

## Conflict and observation table

All gates below are open. Q20 applies to every proposed upload, including
identity: an RO access flag does not establish that reading is non-consuming.
An entry in the observation column is a candidate for a reviewed acquisition,
not permission to transmit it. Every conditional experiment also requires the
[finite experiment prerequisites](#conditional-experiment-prerequisites).

| Gate / disputed fact | Original source claims and vendor agreement | Raw observations needed and conditional experiment | Affected capability and exact gate | Host | Physical |
| --- | --- | --- | --- | --- | --- |
| Q01 / identity and firmware | EDS `[1000]`, `[1009]`, `[100A]`, `[1018sub0..3]`, `[2000]`, `[2001]`; EN86 pp15/17, ZH86 pp15/17. Numeric widths agree; no code-to-firmware binding. `1018:04` absent despite count limit 4. C18 limits OP86 applicability. | Label/serial and manufacturer firmware correspondence; exact nine typed identity replies, route and configuration. Keep missing/aborted fields. No index sweep or fabricated serial field. | Model-specific interpretation/effects require an applicable binding. Raw identity may precede binding only under Q20 admission. | R | raw identity acquired; widths/count conflict |
| Q20 / read effects | EDS access metadata and EN86 pp15-18 / ZH86 pp15-19 establish readable values, not upload side effects. C06/C07 preserve monitor/error access conflicts. No non-consuming guarantee found. | Manufacturer statement per selected object, or explicitly authorized matched-firmware experiment with independent before/after diagnostic/event observation and a specified finite number of uploads. Stable repeated values alone cannot rule out consumed events. | Non-changing probe/polling blocked for every unreviewed row. Ordinary typed read success retains unknown effects; it does not close Q20. | R | none |
| Q02 / state, fault and done bits (C05) | EN86 p12: alarm bit7, zero-speed bit10, target-effective bit12. ZH86 p12: alarm bit3, running bit14. OP86 p10: alarm bit7, running bit14. All show homed bit8 and position-done bit15. Low bits 0..2 are examples, not a complete CiA402 decoder. EDS `[6041]` is U16 RO. | Raw `6041`, `2100`, `603F`, mode and independent enabled/released, motion, standstill and fault observations, with timestamps. Observe bits8/15 before, during and after a separately approved finite operation; distinguish pre-existing done from current-command completion. Do not induce an electrical fault merely to obtain a bit. | Interpreted state, fault, stop, move and home completion require exact firmware dialect and freshness evidence. No union mask or generic fallback. | R; P | raw6041=8037 only; dialect unqualified |
| Q03 / velocity sign and start (C11/C17) | EDS `[60FF]` I32 and EN86/ZH86 p17 I32 conflict with p13 U32 0..3000 rpm. EN86 p13 enables before parameters then refers to bits4..6; ZH86 p13 and OP86 p11 set parameters then enable. | Readback alone cannot establish direction/start. One independently reviewed candidate sequence must specify signed bytes, actual start transition, current mode/state and retained target; observe shaft direction/speed and raw status. Positive, negative and zero each need scoped evidence. Never try both recipes as a search. | Production velocity start, signed range, mode/enable transitions and zero-target meaning gated. A signed codec is not negative-speed support. | C; P | none |
| Q04 / subdivision and feedback scale (C12/C21) | EN86/ZH86 p11 `2201` 0..65535 versus p18 400..51200; EDS `[2201]` default0. `607A` is I32 pulses; p11/EDS correct p16 `I3C` typography. EDS `[6064]`/`[606C]` I32; EN86 p3's 1000-line encoder is not command/feedback scaling. | Actual `2201`, `2500`, `2501`, `6064`, `606C`, motor/gearing and firmware. Independently measured displacement/speed for a finite approved target establishes separate command and feedback scales/sign. Accepted subdivision set/range needs manufacturer evidence; do not sweep settings. | Physical/angle conversion, feedback units and subdivision setters require nonzero applicable scale and configuration provenance. Never assume 4000 counts/rev. | R/C; P | none |
| Q05 / software limits (C13) | EN86/ZH86 p18 `2202`: 0 disabled, 1 effective after homing; EDS `[2202]` default10. EDS `[607Dsub1/2]` I32 bounds; EN86 pp16-17. | Preserve actual enable code and bounds. Establish coordinate basis, homing prerequisite, inclusive/exclusive boundary and response with independent travel protection before any isolated boundary test. | Limits cannot be relied on as protection or written from default10; matching readback does not prove reaction. | R/C; P | none |
| Q06 / DO addresses and functions (C01/C02/C04) | EN86 pp8/18, ZH86 p8, OP86 p6: polarity `2400:0B`, DO0..2 `:0C..0E`. EDS `[2400subB..F]` and ZH86 p19: `:0B` reserved, polarity `:0C`, DO0..2 `:0D..0F`. EN86/ZH86 p8 and OP86 p7 assign 2 running/3 homed; EN86 p18/ZH86 p19 assign 2 homed/3 drive status. Polarity table specifies only bits0/1. | Obtain exact firmware map first. Record existing assignments and isolated terminal voltage/state. Only then consider one exact reviewed assignment or polarity experiment on an identified terminal, with brakes/actuators isolated. Never write both disputed addresses, infer bit2 or reinterpret PA labels as CAN indexes. | DO assignment/polarity/function2/3 writes and interpretation remain gated per firmware. Raw equal values do not identify which address is correct. | R/C; P | none |
| Q06/Q07 / output command and mask | OP86 p7 says `60FE:01/:02` bits16..18 both1 assert DO0..2; EDS confirms RW U32, not a full truth table. ZH57 pp8/17 disagree DO0..2 versus DO1..3 (C03); this does not resolve 86C. | With exact terminal binding and isolated load, capture command/mask, `2102` and measured terminals for a finite reviewed set/clear/unchanged-bit case. Require a source-backed exact command/mask candidate, independent control and explicit review/authorization; the remaining clear/preserve effect may be the declared experiment question. Never guess the disputed function address or trial both layouts. | Command/mask clear semantics, unchanged-bit preservation and physical terminal mapping gated. `2102` bits0..2 are not `60FE` bits16..18. | R/C; P | none |
| Q08 / input filters (C15/C16) | EDS `[2400subA]` reserved versus EN86 p18 filter coefficient. EDS `[2401]..[2407]` defaults50/40/100/4000/10/2/16 versus EN86/ZH86 p8 all2. No time equation supplied. | Applicable firmware map, accepted range/unit and existing filter values; independent timestamped input stimulus only after reviewed electrical and function assignment. Do not write reserved `:0A` or derive milliseconds from a name. | Filter setters and time interpretation gated; documented raw observations do not resolve coefficients. | R/C; P | none |
| Q09 / tuning and feedback | EN86/ZH86 pp19-20 cover `2500..2519`; EDS `[2500]..[2520]` all RW U16 without ranges/defaults. `251A..2520` have EDS-only names. Current mA/percent and some timing units do not supply safe operating bounds. | Manufacturer model/motor-specific ranges, loop applicability and units for each chosen field; existing raw values, ratings and measured effect only under an individually bounded tuning experiment. No automatic tuning or current sweep. | Each affected setting/conversion remains gated independently; no torque capability inferred. | R/C; P | none |
| Q10 / mode/method encoding (C09/C10/C22/C23) | EN86 pp11/13/16-17 and EDS `[6060]`, `[6061]`, `[6098]` disagree I8/U8/access. Positive modes1/3/6 and methods17/18/24/29 share encoding. `6099:02` p13 says creep time with rpm; p14 prose says speed. | Preserve raw `6061` and `6098`; obtain applicable semantics before interpreting high-bit codes or method0. Use mode display as an observation, never obey translated instruction to write `6061`. | High-bit/negative codes and default method0 unsupported; valid positive encoding does not authorize a mode transition or homing. | R/C; P | raw6061=0 only; semantics unqualified |
| Q11 / enable, target edge and rearm (C08) | EN86/ZH86 p12 and OP86 pp9-10 show controlword6/7/15, approximately10 ms spacing, bit4 rising-edge start and bit5 update behavior; EDS `[6040]`/`[6060]` WO versus manual RW. No complete acceptance/rearm handshake or queue contract. | Per-mode/current-state retained-target behavior; raw target/status and independent motion capture around one approved edge and explicit return. Determine when the next edge is admissible and whether old done survives. Lost-ack cases remain uncertain, never replayed. | Enable/release, target rearm, fresh move completion and immediate-update behavior gated. No command read-modify-write, invented FIFO or fixed device deadline. | C; P | none |
| Q11 / zero ramps | EN86/ZH86 pp11/13 and EDS `[6083]`, `[6084]`, `[609A]`: U32 ramp times, mode-table range0..2000 ms. No zero behavior or physical acceleration formula stated. | Manufacturer zero interpretation, or a separately bounded experiment with independently controlled motion/stop and measured speed/time. Do not substitute a long timeout for evidence. | Zero ramp admission and physical acceleration/deceleration conversion gated; raw ms is separate from host deadline. | R/C; P | none |
| Q12 / halt and quick stop | ZH86 p12/OP86 p10 show15->271; EN86/ZH86 p12 show15->11. EDS `[605D]` I16 default1 with no applicable option meanings; `605A`/`6085` absent. | Manufacturer current-state transitions and option meanings; independently bounded stop test recording submitted/acknowledged time, motion/standstill, remaining targets and settlement. Independent stop must not depend on the disputed transition. | Stop action/terminal guarantees and queue-discard claims gated. Never enable first to stop; no generic option-code fallback. | C; P | none |
| Q13 / fault clear | EN86/ZH86 p15: controlword bit7 set then manually clear; ZH86 p12/OP86 p10:15->128. EN86 p18 `2301`0/1; ZH86 p18 omits it. EN86 p16/EDS `[603F]`: only FF01/FF05 identified; p20 first three LED faults require restart. | Existing fault cause, independently observed clearance, raw error/status and exact chosen route/rearm. Use a manufacturer-approved reproducible condition if a deliberate fault is required. No automatic restart or electrical-fault injection. | Clear route, resettable set and rearm remain gated; never write `603F`, enable as preamble or derive FF02..04 from LED counts. | R/C; P | none |
| Q14 / position clear and offset | EN86/ZH86 p15: `2302`0->1 clears current position, return manually0. EDS `[2302]` U16; `[607C]` I32; p13 home offset range ±1000000. No arbitrary-position setter or full reference-effect promise. | Before/after command/feedback coordinates, independent stationary shaft, home/limit reference and chosen offset application point. Isolate one reviewed clear or offset case; uncertain acknowledgement forbids replay. | Position clear, offset and reference publication gated; zero is not homing or a verified host origin. | R/C; P | none |
| Q15 / save and factory reset | EN86/ZH86 p15/OP86 p13: SDO `2300=2` save; EN86/ZH86 p18:1 factory reset,0 ineffective. EDS `[2300]` U16. No completion/endurance/power-loss/complete saved-set guarantee. | Review one exact source-backed action/value, known baseline, independent power control and an independently established recovery route. Declare trigger/rearm, saved-field set, completion or reset/route effects still unknown as the experiment question; they need not be assumed before the test. Retain reads before/after an explicitly approved planned restart. Power interruption is a separate experiment. | Save/reset completion and durable persistence gated. SDO response is not durable completion; no guessed delay, retry or automatic restoration. | C; P | none |
| Q16 / node, bitrate and route (C20) | EN86/ZH86 pp9-10/18 and EDS `[2203]`, `[1200sub1/2]`: configurable address/routes; DIP all-OFF user-defined. EDS DeviceInfo advertises50 kbit/s, manual DIP only125/250/500/1000. Activation and precedence unspecified. | Actual DIP photos, route/bitrate evidence, raw applicable settings and manufacturer precedence/activation. Any change needs exact old/new route, finite loss handling and independent recovery; no scanning or guessed bitrate write. | Node/route setters and50 kbit/s selection gated; declared parameter0 is not a unicast node. | R/C; P | matchednodes1/2at1Mbit/s; changes untested |
| Q17 / PDO mapping and qualification | EDS `[1400..1403]`, `[1600..1603]`, `[1800..1803]`, `[1A00..1A03]`; EN86 pp15-16 mapping tables. Mapping flags/defaults alone do not establish firmware remap restrictions, timer semantics or producer retirement. | Exact active map/count/IDs; applicable NMT/drive restrictions and readback. For a separately admitted remap, capture transition/queued traffic with an evidenced retirement boundary, distinct verified route or observable marker. | Map-content confirmation is distinct from delayed-sample qualification. New host epoch plus same-ID/same-length data cannot prove freshness. Timer changes retain separate gates. | C; P | TPDO1 readback acquired; mutation untested |
| Q18 / communication, reset and host loss | EDS `[1017]` default0, `[1014]`, DeviceInfo; no `[1016]` consumer or manufacturer host-loss stop guarantee. No documented upload maxima, reset ready delay or bus-off motor reaction. | Capture ordinary owner traffic/EMCY/heartbeat with explicit timestamp basis. Each deliberate reset, bus fault or host-loss experiment requires its own finite authorization and independent physical control. | Per-firmware service behavior and loss/recovery guarantees remain unqualified. Timeout policy and CAN TX success cannot establish motor state. | C; P | none |
| Q19 / fixture, inputs and defaults (C14/C19/C24) | EN86 p3 supply18..80 VAC/36..110 VDC versus pp6-7 wiring24..50 VDC. EN86/ZH86 p8 DI defaults1/2/3 and DO0=1 versus EDS zeros. EDS `[60FD]` U32 is not an independent bit/function map. | Exact label and applicable electrical approval, inspected wiring, input assignments/polarity, physical signals and measured levels. Establish `60FD` mapping independently of `2101`. Resolve only the selected model's supply conflict. | All live work depends on applicable fixture/authorization; unused terminals do not imply disabled functions. Do not disable protection to run a test. | R/C; P | reportedrails/labels/cutoff; functionalpath works |
| Q21 / homing and reference | EN86/ZH86 pp13-15 and OP86 pp11-13:17 negative limit,18 positive limit,24 positive home approach,29 negative approach then leave switch positively; bit4 falling edge interrupts. No numeric reversal/search bounds. | Method-specific actual switch/polarity and independent position/travel observation; raw switch/mode/status/offset and fresh bit8. One finite approved method must bound travel/time independently; interrupted homing must remain distinct from completed/home-reference valid. | Each home method, interruption behavior and resulting reference gated separately. No fabricated backoff distance or generic CiA method substitution. | C; P | none |

## Conditional experiment prerequisites

The first preference is applicable manufacturer clarification. When a question
cannot be answered by approved non-changing reads, prompt 29 permits one
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

No live sequence currently satisfies these prerequisites. When evidence arrives,
record its fixture/firmware scope and limitations, review the canonical ledger
first, then update affected profile gates, generated declarations, meaningful
tests, Doxygen and guides together. A measured result for one specimen never
silently qualifies another Lichuan model, N5 or EPOS4.
