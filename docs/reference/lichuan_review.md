# Reviewed Lichuan C semantics

This guide explains the documentary decisions in
[lichuan_objects.json](lichuan_objects.json). The ledger is the canonical source
for reviewed field policy, retained contradictory claims, applicability and
qualification gates. Documentary review is complete; no library operation or
installed firmware is implemented or qualified by these tables. An EDS RW or
PDO flag describes source metadata, not permission to perform an action.

Raw identity acquisition may precede exact model identification. Retain its
route, source, time and uncertainty, then record the user-selected model and
firmware evidence before applying model-specific interpretations. Missing
physical conversion or a disputed status bit does not prevent retaining a raw
observation. Unknown read-consumption behavior prevents claiming harmless
continuous polling; it is not evidence that every read is destructive.

Gate only the affected field, interpretation or operation. In particular,
57C output-label qualification does not block an independently established
86C output contract. An absent object is not a measured unsupported response.
Explicit negative evidence is narrower: the OL product tables state no encoder;
the generic EDS states `LSS_Supported=0`. Neither fact establishes all other
capabilities of an installed drive.

## Original sources and scope

Page numbers are one-based PDF file pages, including covers. Originals, hashes
and provenance are retained in [SOURCES.json](SOURCES.json).

| ID | Original | Scope |
| --- | --- | --- |
| EDS | [LC_StepDriverCAN_V1.1.eds](vendor/LC_StepDriverCAN_V1.1.eds) | Generic Lichuan stepper metadata; no exact installed-model/firmware binding. |
| EN86 | [English 86C manual](vendor/86C_CANopen_user_manual_en.pdf) | Cover names CL86-C/OL86-C. |
| ZH86 | [Chinese 86C manual](vendor/86C_CANopen_user_manual_zh.pdf) | Cover names CL86-C/OL86-C; disagrees internally on output addresses. |
| OP86 | [86C operating-manual file](vendor/CL86C_OL86C_operating_manual_zh.pdf) | Cover names 86C; internal headers say 57C. Preserve this applicability defect. |
| ZH57 | [Chinese 57C manual](vendor/57C_CANopen_user_manual_zh.pdf) | CL57-C/OL57-C contrast; output labels conflict internally. |
| OP57 | [57C operating-manual file](vendor/CL57C_OL57C_operating_manual_zh.pdf) | CL57-C/OL57-C contrast. |

## Status and observation decisions

The status tables are EN86/ZH86/ZH57 p12 and OP86/OP57 p10; homing status
also appears on EN86/ZH86/ZH57 p15 and OP86/OP57 p13. No language, majority
vote or generic CiA402 mask selects the installed firmware's interpretation.

| `6041` bit | EN86 claim | ZH86 claim | OP86 claim | 57C contrast | Disposition |
| --- | --- | --- | --- | --- | --- |
| 3 | No listed meaning in this table | 1 alarm | No listed meaning | No listed meaning | Retain competing alarm layouts; correlate the selected model's raw word with independently known fault state. Q02. |
| 7 | 1 alarm | No listed meaning in this table | 1 alarm | ZH57/OP57: 1 alarm | Do not combine bits 3/7 into one alarm mask. Q02. |
| 8 | 1 homing complete | Same | Same | Same | Consistent documentary claim; require fresh operation-linked reference evidence. Q02/Q21. |
| 10 | 1 zero speed; 0 running | Not listed | Not listed | Not listed | EN86-specific motion evidence; qualify before stop/completion use. Q02/Q12. |
| 12 | 1 target effective | Not listed | Not listed | Not listed | No complete acknowledgement handshake follows; establish transition and stale-value behavior. Q02/Q11. |
| 14 | Not listed | 1 running; 0 stopped | Same | Same | Alternative motion layout; no universal union with bit10. Q02. |
| 15 | 1 position complete | Same | Same | Same | Require command correlation, deassertion/reassertion and established completion criteria. Q02/Q11. |

Bits0..2 are illustrated by controlword 0/6/7/15 corresponding to
000/001/011/111. Those examples are not a complete drive-state decoder.
Neither a logical status bit nor zero reported velocity alone qualifies
physical standstill.

EN86 p17 and ZH86/ZH57 p18 define `2100` bits 0..6 as position complete,
homed, running, alarm, released, positive soft-limit active and negative
soft-limit active. Bit4 zero means enabled. `2101` describes logical input
states at bits 0..6; `2102` describes logical output states at bits 0..2.
Choose conservative RO policy despite EDS RW flags. EDS `2100=8` is a
default claim, not an observed startup alarm. These observations are not an
atomic snapshot or a resolution of the `6041` dialect. Owner 19 retains raw
values and provenance. `60FD` is a separate U32 digital-input object whose
complete bit layout is not established by EN86/ZH86 p17 or the EDS; do not
copy `2101`'s layout into it.

## Motion, settings and action decisions

| Contract and owner | Source-backed facts | Consequence and remaining gate |
| --- | --- | --- |
| Mode request/display; 19,20,30,33,35,36 | EN86/ZH86/ZH57 p11: `6060` values 0 undefined,1 position,3 velocity,6 homing; `6061` observes mode. EDS uses U8 versus manuals I8; EDS command WO/display RO versus dictionary RW. | Positive documented codes have identical wire bytes. Owner 20 owns the native mode setting; operation owners retain their transition prerequisites. Preserve unknown display bytes; never write `6061`. Q10/Q11. |
| Velocity; 35 | EN86 p13 enables 6,7,15 before target/ramp parameters, then mentions bits4..6. ZH86/ZH57 p13 and OP86/OP57 p11 place parameters before enable, which starts motion. Procedure tables call `60FF` U32,0..3000 rpm; EDS and dictionaries call it I32. | Do not merge start recipes or infer a negative operating range from signed storage. Establish actual start transition, retained target, zero/negative behavior and enabled-mode effects. Q03. |
| Position target and feedback; 19,33,34 | EN86/ZH86/ZH57 p11: `607A` I32,-1000000..1000000, command pulses; `6081` U32,0..3000 rpm. EN86 p16/ZH86 p17: `6064` I32 and `606C` I32 without physical scale. `I3C` printed for `607A` is contradicted by the I32 mode table and EDS. | Retain source typo and use reviewed I32 representation. Command pulses, encoder lines, feedback counts and shaft revolutions are not interchangeable. `606C` units are not established as rpm. Q04. |
| Ramp fields; 20,33,35,36 | EN86/ZH86/ZH57 p11/p13 and OP86/OP57 p9/p11: `6083/6084/609A` are U32 time fields, 0..2000 ms; EDS defaults 100. | Preserve milliseconds, not generic acceleration units. Qualify zero, nonzero initial-speed behavior and physical ramp/stop effect; a document default is not initialization policy. Q11/Q21. |
| Subdivision; 20,34 | `2201` mode table0..65535 versus EN86/ZH86/ZH57 p18 setting range400..51200; EDS default 0. | Preserve conflicting bounds/default. Accepted values and relation to command/feedback scales need evidence; never use zero as a revolution denominator. Q04. |
| Direction; 20,34 | EN86/ZH86 p18: `2200` choices0 Pulse Sign,1 Pulse /Sign. | This does not establish clockwise shaft direction or a coordinate convention. Q04. |
| Software limits/offset; 20,32,33,36 | EN86/ZH86 p18: `2202`0 disabled,1 effective after homing; EDS default 10. `607D:01/:02` are I32 minimum/maximum bounds; EN86/ZH86 p13 separately gives `607C` I32 home offset range -1000000..1000000. | Do not normalize unknown10 to true. `607D` accepted numeric limits, units and coordinate basis are unknown; `607C` native offset units/application point are unknown. Do not copy another object's range into `607D`, or command-pulse units into either. Q04/Q05/Q14. |
| Enable/release; 30 | EN86/ZH86/ZH57 p12 and OP86/OP57 p9/p10 show 6,7,15 at about 10 ms spacing. | State-transition examples require current state/mode. No universal startup/release script or strict response deadline follows. Retained targets may matter when enabling. Q11. |
| Position trigger/update; 33 | EN86/ZH86 p12: controlword bit 4 rising triggers new target; bit5 selects after-current-curve versus immediate update; bit6 absolute versus relative. Examples15→31/95/63/127 combine these bits. | Qualify trigger rearm, acceptance, queue/update behavior and fresh completion without replaying uncertain writes. Q11. |
| Stops; 31 | ZH86/ZH57 p12 and OP86/OP57 p10:15→271 stop; all five sources:15→11 emergency stop. EDS `605D` I16 RW/default 1 has no usable option semantics; `605A/6085` absent. | Never enable merely to prepare a stop. Legal starting states, deceleration selection, settlement and physical latency require evidence. No torque-off or safety rating is established. Q12. |
| Fault reset; 32 | EN86/ZH86/ZH57 p15 and OP86/OP57 p13: set controlword bit 7, then manually clear; Chinese transition tables show 15→128. EN86 p18/EDS declare `2301`; ZH86 p18 omits it. | Do not infer full-word/bit-operation equivalence, a `2301` edge or universal fault reset. `603F` remains RO under conservative policy. Q13. |
| Position clear; 32 | EN86/ZH86/ZH57 p15 and OP86/OP57 p13: `2302` rising0→1 clears current position, then manually return0, through SDO. | This is zero-only clearing, not arbitrary position assignment or proof of homing. Determine affected counters/reference/limit state; lost acknowledgement retains uncertainty. Q14. |
| Homing; 36 | EN86/ZH86/ZH57 p13–15 and OP86/OP57 p11–13: methods 17 negative limit,18 positive limit,24 positive origin,29 negative origin. Methods 17/18/24 withdraw then creep back;29 creeps away until leaving origin. Bit4 rising starts, falling interrupts. `6099:02` is creep speed despite EN86's table label “time”. | Positive method bytes agree despite U8/I8 metadata. No index-only/current-position method, backoff distance or search bound is established. Require only the selected method's actual signals; qualify initial-active switch, offset, interruption and fresh reference. Q10/Q21. |
| Save/factory reset; 39 | EN86/ZH86/ZH57 p15 and OP86/OP57 p13 explicitly use SDO `2300=2` to save. EN86/ZH86 p18:0 ineffective,1 restore factory,2 save. EDS U16 RW/PDO1. | These are explicit actions, not ordinary settings. No complete saved-field list, durable-completion signal/time, endurance, rearm or restart/route behavior is established. SDO acknowledgement alone does not prove flash completion. Q15. |

LED tables on EN86/ZH86/ZH57 p20 and OP86/OP57 p14 distinguish restart-reset
overcurrent/overvoltage/undervoltage from resettable memory/position errors.
Their five-second flash window is not a CAN timeout. EN86/ZH86 p16 identify
FF01 overcurrent and FF05 position deviation; LED numbering does not establish
FF02..FF04 wire codes. Fault, command outcome, network state and application
health remain separate facts.

## I/O and tuning decisions

| Field | Conflicting or incomplete evidence | Reviewed disposition; owner 21 unless stated |
| --- | --- | --- |
| Output addresses | EN86 p8/p18, ZH86 p8, OP86 p6: polarity2400:0B, DO0..2 at0C..0E. EDS and ZH86 p19: polarity0C, DO0..2 at0D..0F. | Preserve both layouts. Gate affected semantic reads/writes/mapping until exact firmware layout is known. Do not try both writes. Q06. |
| Output function2/3 | EN86/ZH86 p8 and OP86 p7:2 running,3 homing complete. EN86 p18/ZH86 p19:2 homing complete,3 drive status. | Gate these meanings separately. Consistent0 undefined,1 alarm,4 in-position,5 brake release,9..11 user0..2 do not resolve address ambiguity;6..8 remain undocumented. Q06. |
| Output polarity | EN86 p18/ZH86 p19 describe only bits 0/1 despite three outputs. | Do not invent bit 2 behavior. Q06. |
| `60FE:01/:02` | OP86 p7: output and mask bits 16..18 both1 produce DO0..2 output. ZH57 p8 says DO0/1/2; p17 labels both subentries DO1/2/3. | The ON condition does not establish every clear/mask-zero behavior. Identify actual terminal, assignment and polarity. Keep Q06 for 86C and Q07 for 57C; no fourth terminal or automatic brake control. |
| DI assignments/polarity | EN86/ZH86 p8/p18:2400:01 bits 0..6 invert DI0..6; sub2..8 functions 0 undefined,1 origin,2 positive limit,3 negative limit,4 enable/MF,5 stop,6 emergency stop,7..13 user0..6. | Consistent input facts can proceed independently of output conflicts. Physical wiring/active level is separate. Do not disable missing protective inputs implicitly. Q19. |
| Factory assignments | EDS DI/DO assignments all0 versus EN86/ZH86 p8 DI0/1/2=1/2/3 and DO0=1. | Read active configuration; no default-writing initialization. |
| Reserved/filter entries | EDS2400:09/0A/0B/10 reserved; manuals call0A filter and0B participates in output conflict. | Keep reserved exclusion; no typed setter from RW/PDO flags. Resolve only the disputed function. Q06/Q08. |
| `2401..2407` filters | Manuals EN86 p8/p18–19, ZH86 p8/p19, OP86 p6–7 give all2; EDS50/40/100/4000/10/2/16. No coefficient equation/time unit. | Retain raw coefficients/default claims; no millisecond conversion or guessed operating range. Q08. |
| Brake example | OP86 p6 says PA_01E while same-page table says PA_02E. | Preserve editorial contradiction; PA label is not another CANopen address. Q06/Q19. |

Tuning owner 22 covers all `2500..2520` fields, each RW U16/PDO1 in the EDS
with no default or operating limits. EN86 p19–20 and ZH86 p19–20 establish
the meanings below. Native observations remain separate from qualified setters
and physical conversions; Q09 applies to the selected field, not an entire
unrelated tuning campaign.

| Entries | Documented meaning | Missing facts |
| --- | --- | --- |
| 2500 | 0 ineffective,1 open loop,2 closed loop | Model eligibility and change effect; not 6060 motion mode or proof of OL feedback capability. |
| 2501 | Encoder resolution | Lines/counts/quadrature basis and range; a 1000-line encoder does not establish command counts/revolution. |
| 2502 | Maximum **peak** current, mA | Drive/motor-specific range and effect; not RMS current. |
| 2503/2504/2505/2506 | Closed-loop maximum/base, open-loop maximum, locking current ratios, percent | Reference basis and accepted limits; percent does not prove 0..100 is permitted. |
| 2507 | Locking-current time, ms | Accepted limits and transition behavior. |
| 2508/2509 | Low-pass enable/coefficient | Enable codes, equation and bounds. |
| 250A/250B/250C | Position-error alarm threshold, positioning-completion threshold/time | Threshold basis and time unit; 250C is not established as milliseconds. |
| 250D/250E/250F/2510/2511 | Mean-filter coefficient, current-loop ratio, Kp, Ki, Kc | Units, equations, bounds and activation. |
| 2512/2513/2514/2515/2516/2517/2518/2519 | LA velocity Kp1/Kv1/node1/Kp2/Kv2/node2, feedforward Kvf, position-loop Ki | Gain/velocity scales, equations, bounds and effects. |
| 251A..2520 | EDS symbolic names only: v_kp/v_ki/v_kc/v_kvfbd1/v_kvfbd2/v_kvff/v_kacff | Physical meaning and effect unknown; names do not establish a tuning algorithm. |

## Network and transfer boundary

Manufacturer evidence establishes advertised services and object metadata.
Pinned public implementations corroborate protocol mechanics only; none
resolves a Lichuan behavior or firmware contradiction. Exact revisions and
licenses are retained in [public-code provenance](public_code/SOURCES.json).

| Protocol reference | Established boundary |
| --- | --- |
| CANopenNode `9b8beed8367241e96ac03f916cd5a500bcb2cf23`, [CO_NMT_Heartbeat.h](public_code/canopennode/301/CO_NMT_Heartbeat.h), lines44–98 | NMT command/node payload; node 0 addresses all nodes. Commands1/2/128/129/130 and state bytes0/4/5/127. Service eligibility distinguishes operational/pre-operational/stopped. This is not a motor-stop guarantee. |
| canopen-python `14b3336bc973023e82325426adf6d71046ebcf55`, [nmt.py](public_code/canopen_python/canopen/nmt.py), `NmtMaster.send_command`, `NmtSlave.send_command`, `start_heartbeat` | NMT identifier0/two bytes; boot-up and heartbeat node+0x700; boot-up byte0; producer time interpreted in ms and zero disables production. Does not establish Lichuan startup state or host-loss action. |
| Same CANopenNode pin, [CO_SDOclient.c](public_code/canopennode/301/CO_SDOclient.c), upload response handling lines1216–1257 | Upload verifies index/subindex and selects expedited/segmented handling from response flags. Scalar width does not predetermine server response form. |
| Same pin, [CO_PDO.h](public_code/canopennode/301/CO_PDO.h), PDO description; [CO_PDO.c](public_code/canopennode/301/CO_PDO.c), lines119–131 | Mapping/direction eligibility is distinct from SDO access. Stack synchronous processing and runtime-remapping options are stack behavior, not proof of drive support. |
| Same pin, [CO_Emergency.h](public_code/canopennode/301/CO_Emergency.h), Emergency producer description | Error code and error-register fields are distinct; the stack's remaining-byte encoding is implementation-specific and must not become a Lichuan vendor-data decoder. |

Use individual CANopen node IDs 1..127 for addressed service work. Explicit
NMT broadcast is a separate request; it does not create broadcast SDO.
EN86/ZH86 p18 `2203` parameter range0..127 does not authorize unicast node 0.
Its stored-zero meaning, activation, DIP precedence and persistence remain Q16.

EN86/ZH86/ZH57 p9–10 and OP86 p7–8 document SW1..5 weights 1/2/4/8/16
for nodes 1..31; all OFF is user-defined. SW6/SW7 ON/ON=125k,
OFF/ON=250k, ON/OFF=500k, OFF/OFF=1000k. SW8 ON inserts 120 ohms.
EDS `[DeviceInfo]` also advertises 50k without a manual selection method;
10/20/800k flags are unset. Do not invent a software bitrate object. Two
RJ45 connectors and the p3 `2 x 1 Mbps` text do not establish two controllers,
2 Mbit/s or CAN FD. The32-axis product claim is not a common-library limit.

| EDS source | Metadata and boundary |
| --- | --- |
| 1005 | RW U32 SYNC COB-ID, default 0x80; no 1006 period object. SYNC production and supported synchronous PDO modes remain unqualified. |
| 1014 | RW U32 EMCY COB-ID, default node+0x80. Preserve raw events independently of drive-state and health interpretation. |
| 1200:01/:02 | RW U32 SDO request/response IDs, defaults node+0x600/node+0x580. Configurable routes require actual activation/retirement evidence; U32 does not approve every identifier flag. |
| 1017 | RW U16 producer heartbeat, default 0. Host observation cannot establish drive action on host loss;1016 consumer evidence is absent. |
| 1001/1003 | Error register and four history entries;1003 count is RO U8,0..4. Do not import generic history-clear writes. |
| DeviceInfo | SimpleBootUpSlave1, Master0, DynamicChannelsSupported0, GroupMessaging0, LSS_Supported0, four RPDO/four TPDO. Negative claims are scoped to this unbound EDS. |

All explicit EDS scalar widths are 1, 2 or 4 bytes. They fit expedited payload
capacity; this does not prove every upload is expedited, segmented service
is absent or block transfer is supported. Owners 13–16 handle bounded service
and network evidence. CAN TX success is neither SDO success nor drive
acceptance. No manufacturer SDO maximum, reboot delay, flash wait, bus-off
recovery time or drive host-loss stop is established.

## PDO metadata and qualification

Each EDS communication record1400..1403/1800..1803 has count 5, RW U32
COB-ID, U8 transmission type default 255, U16 inhibit default 0, U8 compatibility
default 0 and U16 event value default 1000 for receive/100 for transmit.
These are raw source values; they do not prove a motor watchdog or all accepted
transmission modes. No useful setter meaning is established for compatibility
entries. Mapping records have four U32 slots and a U8 active count 0..4.

[Prompt 18's original-source review](../reports/prompt_18.md) found no applicable
disable/edit/enable procedure or timing-setting contract. Q17 now explicitly
requires these facts before typed mutation; existing passive layouts and
readback checks do not complete that feature.

| Route | Default ID | Mapping record and active words | Derived payload |
| --- | --- | --- | --- |
| RPDO1 | node+0x200 | 1600:60400010,60FF0020,60600008 | 7 bytes |
| RPDO2 | node+0x300 | 1601:607A0020,60810020 | 8 bytes |
| RPDO3/4 | node+0x400/500 | 1602/1603 count 0 | No active layout declared |
| TPDO1 | node+0x180 | 1A00:60410010,60FD0020,60610008 | 7 bytes |
| TPDO2 | node+0x280 | 1A01:60640020,606C0020 | 8 bytes |
| TPDO3/4 | node+0x380/480 | 1A02/1A03 count 0 | No active layout declared |

Payload lengths are arithmetic, not observed traffic. Four mapping slots and
eight Classical CAN bytes are independent limits. EDS WO 6040/6060 occur in
default RPDO1: SDO upload permission cannot be required for every RPDO target.
Conversely, an action's PDO flag does not authorize periodic motion, save or
reset traffic. Owners 17/18 require verified routes/maps and effect-specific
eligibility. A new host generation and map readback cannot distinguish old
same-ID/same-length samples; Q17 requires an established retirement boundary,
distinct verified route or applicable observable marker.

## Family capability and identity

| Model | Product table | Motor/feedback | Documented modes/protocol | Qualification boundary |
| --- | --- | --- | --- | --- |
| CL86-C | EN86/ZH86 p3 | Stepper, matched 60/86 motors, 1000-line incremental encoder; max 8 A claim | Position,velocity,homing; CANopen CiA301/CiA402 | Encoder lines do not establish feedback/command scale. |
| OL86-C | EN86/ZH86 p3 | Stepper, matched 60/86 motors, explicitly no encoder; max 8 A claim | Same documented modes/protocol | Reported position is not thereby measured rotor feedback. |
| CL57-C | ZH57 p3 | Stepper, matched 42/57/60 motors, 1000-line incremental encoder; max 5 A claim | Same documented modes/protocol | Separate contrast; no 86C firmware inheritance. |
| OL57-C | ZH57 p3 | Stepper, matched 42/57/60 motors, explicitly no encoder; max 5 A claim | Same documented modes/protocol | Separate contrast; no encoder/closed-loop capability from shared EDS. |

These documentary entries are not four implemented profiles. Torque/cyclic
modes and servo-motor applicability are not established. No internal IC or
shared firmware follows from similar model names. Supply contradictions remain
model-specific: 86C p3 product 18..80VAC/36..110VDC versus p6 terminal 24..50VDC;
ZH57 p3 product 24..48VDC versus p6 terminal 24..50VDC. Use neither as a bench
default; Q19 concerns the actual selected fixture.

EDS identity vendor 0x1000/product 1/revision 0 is generic. `1018` declares
sub0..3 only: count default 3 despite HighLimit 4; no serial-number sub4 is
declared. `1009/100A` and manufacturer model/version `2000/2001` are numeric
U16, not strings. EN86 p17/ZH86 p18 provides no code-to-model/firmware table.
Discovery owner 37 retains raw identity and user-selected binding provenance;
it cannot auto-select all families from generic values.

## Qualification checklist and owning work

The exact required evidence, applicable models and current status live in
[`semantic_review.gates` Q01–Q21](lichuan_objects.json). The checklist below
selects the obligation needed by an operation; it does not require completing
every row before an unrelated raw observation or host-only implementation.

| Gates | Check before claiming the affected result | Later owner |
| --- | --- | --- |
| Q01 | Exact model/firmware interpretation and binding provenance; raw discovery may precede this. | 19,37 |
| Q02/Q20 | Applicable status dialect, freshness and read/poll effects. | 19 and consuming operations |
| Q03/Q10/Q11 | Velocity start/sign; documented mode/method bytes; enable, trigger, ramp and acceptance effects. | 20,30,33,35,36 |
| Q04/Q05/Q14 | Independent command/feedback scales, limits and offset/clear reference effects. | 19,20,22,32–34,36 |
| Q06/Q07/Q08 | Selected model's output layout/identity and affected input-filter semantics. | 21 |
| Q09 | Selected tuning field's model prerequisites, units, limits and effect. | 22 |
| Q12/Q13 | Stop transitions/completion and fault-specific reset/rearm. | 31,32 |
| Q15 | Save/reset scope, durable completion and interrupted-operation uncertainty. | 39 |
| Q16/Q17/Q18 | Explicit communication commissioning, PDO retirement and actual service/network behavior. | 13–18,25,37,38 |
| Q19/Q21 | Actual authorized electrical fixture and method-specific homing signals/reference evidence. | 29,36,47,48 |

No startup, selection, probe, cached read or recovery implicitly enables motion,
clears faults, changes routing, saves parameters or replays an uncertain write.
Unconfirmed effects remain uncertain until relevant evidence reconciles them.
