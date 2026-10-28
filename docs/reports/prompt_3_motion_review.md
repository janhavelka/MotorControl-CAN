# Prompt 3 independent motion-source review

Date: 2026-10-28. Read-only source review on `promptset/motorcontrol-can`;
the lead owns ledger integration, final validation and Git. This handoff records
documentary decisions, not implemented operations or firmware qualification.
Source IDs and original hashes are in the [ledger](../reference/lichuan_objects.json).
All page numbers below are one-based PDF file pages.

## Original evidence checked

- EN86: pp3, 11-18, 20; ZH86: pp3, 11-18, 20.
- OP86: pp9-14; ZH57: pp3, 8, 11-15, 17-18; OP57: pp9-14.
- Rendered and visually checked EN86/ZH86/ZH57 p12 and OP86/OP57 p10.
  Their differing status layouts are present in the original tables. PyMuPDF
  reported a structure-tree warning for OP86/OP57, but rendered complete,
  readable pages. The images were disposable files outside the checkout;
  original PDFs were unchanged.
- Reopened original EDS sections `603F`, `6040`, `6041`, `605D`, `6060`,
  `6061`, `6064`, `606C`, `607A`, `607C`, `607Dsub1/2`, `6081`, `6083`,
  `6084`, `6098`, `6099sub1/2`, `609A`, `60FF`, `2100`, `2200..2202`,
  `2300..2302`; checked absence of `605A` and `6085` sections.

## Status decisions

| Field | Original claims | Documentary disposition and smallest qualification gate |
| --- | --- | --- |
| Alarm, bits 3/7 | EN86 p12: bit7; ZH86 p12: bit3; OP86 p10: bit7. ZH57 p12 and OP57 p10: bit7. In each listed field, 1 means alarm. | Preserve separate source/model dialects and raw U16. A bound firmware needs correlation of raw status with an independently identified fault and cleared state before interpreting this field. Do not OR bits or infer the dialect from language/model name. |
| Homing, bit8 | All five status tables: 1 means homing complete; corroborated by EN86/ZH86/ZH57 p15, OP86/OP57 p13. | Document common claim; current homing completion still needs a fresh command-linked transition and verified reference. A previous high bit is not completion of new work. |
| Motion, bits 10/14 | EN86 p12/p15: bit10=1 means zero speed, 0 running. ZH86/ZH57 p12/p15 and OP86/OP57 p10/p13: bit14=1 running, 0 stopped. | Gate only decoded motion/stop evidence on matched firmware observation during running and stopping. Neither a logical bit nor zero feedback alone proves physical standstill. |
| Target effective, bit12 | EN86 p12 alone: 1 target effective. Other listed tables omit it. | Keep EN86-specific claim. No complete set-point acknowledgement/queue handshake is documented. Qualify transition and stale-value behavior before using it as command acceptance. |
| Position complete, bit15 | All five status tables: 1 positioning complete. | Document claim but qualify command correlation, deassertion/reassertion and in-position criteria. Do not transplant generic CiA402 target-reached masks. |
| Low bits 0..2 | Tables associate controlword 0/6/7/15 with 000/001/011/111. | These four examples are not a complete drive-state truth table. Raw reads remain available; unlisted state transitions remain unknown. |

EN86 p17 and ZH86/ZH57 p18 describe vendor `2100` bits: 0 position
complete, 1 homed, 2 running, 3 alarm, 4 released (zero means enabled),
5 positive software limit active, 6 negative software limit active.
Manuals say RO; EDS says RW and default 8. Choose conservative read-only
policy; neither the EDS default nor reading this object settles `6041` dialect
or proves a real startup alarm. A read's consumption semantics are not explicitly
specified; do not label all uploads harmless solely from RO/RW metadata.

## Motion and effect decisions

| Feature / later owner | Source-confirmed facts | Narrow remaining gate |
| --- | --- | --- |
| Mode request/display, 30/33/35/36 and 19 | EN86/ZH86/ZH57 p11 and OP86/OP57 p9 list modes 1/3/6; 0 undefined. Mode tables call `6060/6061` I8, EDS U8. EDS `6060` WO and `6061` RO; manual dictionary calls them RW. | Encode only documented positive values with identical one-byte representation; preserve unknown display bytes. Observe `6061`, never implement EN86's translated instruction to write it. The model/firmware must establish mode-change effects, especially while enabled. |
| Velocity, 35 | EN86 p13 enables 6,7,15 before parameters and then invokes bits4-6. ZH86/ZH57 p13 and OP86/OP57 p11 write target/ramp parameters before 6,7,15; that enabling starts motion. All mode tables say `60FF` U32, 0..3000 rpm; EDS and dictionaries EN86/ZH86/ZH57 p17 say I32. | Preserve ordering contradiction. Qualification must identify start event, retained-target effects on enable, enabled-mode changes, and separately negative/zero target behavior. Signed wire metadata does not approve negative operating range. Positive scalar representation can be reviewed independently from production start sequencing. |
| Command/observed scale, 19/20/33/34 | EN86/ZH86/ZH57 p11 and OP86/OP57 p9 label command position pulses. `607A` is I32, -1000000..1000000. EN86 p16 and ZH86/ZH57 p17 give `6064` I32 without physical scale; `606C` I32 also lacks units. CL variants list a 1000-line incremental encoder, OL no encoder (EN86/ZH86/ZH57 p3). | No source proves command pulses = feedback counts, subdivision = a usable revolution denominator, or feedback speed = rpm. Keep raw observations; gate shaft-angle and physical-speed conversion on independently established command/feedback scales, direction, coordinate origin and firmware. OL reported position must not be advertised as measured encoder position. |
| Ramps, 20/33/35/36 | `6083`, `6084`, `609A` are U32 **times**, 0..2000 ms, in EN86/ZH86/ZH57 pp11/13 and OP86/OP57 pp9/11. EDS names them times and defaults each to 100. | Native time fields are documented; zero behavior, physical acceleration formula, nonzero starting-velocity behavior and actual stop effect need qualification. Do not substitute acceleration units from another drive. |
| Subdivision, 20/34 | Mode tables allow `2201` 0..65535; EN86/ZH86/ZH57 p18 says 400..51200; EDS default 0. | Preserve range/default conflict. Qualified setting/readback and command/feedback change observations are required before conversion or live changes. Zero is not a valid assumed denominator. |
| Software limits, 20/33/36 | `2202`: p18 lists 0 inactive, 1 effective after homing; EDS default 10. `607D:01/:02` are I32 bounds. | Keep raw unknown 10, not Boolean normalization. Verify actual values, coordinate frame and activation after homing; clearing coordinates/reset/rebinding invalidates dependent host evidence. Unhomed protection is unproven. |
| Enable/release, 30 | EN86/ZH86/ZH57 p12 and OP86/OP57 p9/p10 show 6,7,15 with about 10 ms spacing. Status examples associate controlword 0 with low bits zero. | About 10 ms is documented approximate spacing, not a strict timing bound. No complete general release/state-transition recipe follows from the four status examples. Qualify current-state/mode transitions and retained target effects. |
| Stop, 31 | ZH86/ZH57 p12 and OP86/OP57 p10 call 15 to 271 stop; all five call 15 to 11 emergency stop. `605D` is I16 RW, default1, mapping0, without usable option meanings. `605A/6085` absent from dictionaries/EDS. | Do not enable as a stop preamble. Qualify each legal starting state, repeated-stop behavior, deceleration selection, queue handling and completion evidence. Do not invent torque-off, safety rating, distance/latency, or generic meanings for `605D=1`. Absence is lack of support evidence, not a measured SDO rejection. |
| Fault reset, 32 | EN86/ZH86/ZH57 p15 and OP86/OP57 p13 say set controlword bit7=1, then manually clear it. Chinese transition tables show 15 to 128. EN86 p18 describes `2301` 0 ineffective, 1 effective; ZH86/ZH57 p18 omit this entry. | Preserve full-word versus bit-action uncertainty; no enable preamble. Establish legal state, trigger/rearm semantics and fault-class effects separately for controlword and `2301`; never infer their equivalence or an explicit `2301` edge. |
| Coordinate clear, 32 | EN86/ZH86/ZH57 p15 and OP86/OP57 p13 explicitly describe `2302` 0 to 1, then manual return0. | Zero-only clear is documented. Qualify affected coordinate(s), homed/limit state and completion observation. It does not document arbitrary position assignment. Lost acknowledgement retains uncertainty; no automatic replay. |
| Homing, 36 | EN86/ZH86/ZH57 pp13-15 and OP86/OP57 pp11-13: methods17/18 search negative/positive limit, withdraw opposite for unspecified distance, creep back to switch. Method24 similarly uses positive origin; method29 searches negative origin and creeps positive until leaving switch. Bit4 rising starts, falling interrupts. `6099:02` rpm table/prose establishes creep speed despite 'time' label. | Keep method-specific fixture/input/polarity requirements. Qualify backoff/search bounds, initial-active-switch case, interrupt settlement, offset application and fresh reference evidence. No index-only or current-position homing method is documented. |
| Save/reset, 39 | EN86/ZH86/ZH57 p15 and OP86/OP57 p13 say SDO `2300=2` saves current parameters, mentioning homing speed/ramp/method. EN86/ZH86/ZH57 p18: 0 inactive, 1 factory restore, 2 save. EDS U16 RW/PDO1 does not make this an ordinary setting. | Keep explicit actions. No complete saved-object set, completion time, endurance, acknowledgement-versus-flash ordering, reboot behavior, post-reset identity or rearm semantics is documented. Qualify each promised result on identified firmware before claiming durable completion; no save/reset was performed here. |

EN86/ZH86/ZH57 p20 and OP86/OP57 p14 give LED flash counts per five
seconds: 1 overcurrent, 2 overvoltage, 3 undervoltage (restart reset);
4 memory error and 5 position deviation (resettable). EN86/ZH86 p16
only map numeric error codes FF01 and FF05. Do not derive FF02..FF04
from LED counts or use the five-second display window as a CAN timeout.

ZH57 p8 maps `60FE:01/:02` bits16/17/18 to DO0/1/2, requiring both
corresponding value/mask bits for output. ZH57 p17 names the same bits
DO1/2/3. This does not settle 86C terminal identity; preserve the 57C
conflict separately and qualify the exact output route in owner21.

## Intake corrections and bounded approval

The inspected numerical motion claims and source-page references match their
originals. Two wording/ownership corrections are needed during integration:

1. `fault_reset_controlword` says a return-to-zero **edge** is described.
   The source specifies set1 and manual clear0, without an explicit edge-trigger
   or pulse width. Use "manual return to zero"; retain trigger/rearm uncertainty.
   `2302` and homing/position bit4, in contrast, explicitly name edges.
2. `mode_selection` assigns both request and observation to prompt19. Keep
   typed `6061` observation under19; mode-changing effects belong to the
   operation owners30/33/35/36, with exact transitions separately qualified.

Documentary completion can approve source-backed scalar representations and
restrict ambiguous effects without asserting firmware qualification. Read-only
raw status/position/configuration observations should not be blocked by missing
physical-unit conversion or a disputed decoded field. Conversely, an approved
wire width or common positive enum subset does not complete a motion feature.
The lead's ledger remains the authoritative policy; this handoff is review evidence.

## Current ecosystem and checks

Live read-only revisions were unchanged from prompt2 and clean:
MotorControl-RS `bd831c859daa9c8c07b8d27a2dd879bb4e750511`, ADS1115
`235af3e8c722661a89a44766a4ccd903a654f6fe`, INA228
`b28c5ea69bcb4d26b351c52c24d3c6fae9fd282b`, FieldCore-node
`5ad9292dc8481cf08be9a61f662495a6474d1da0`.
Inspected RS `src/profiles/ess_rs/Actions.cpp` action preparation and uncertain
write evidence, ADS1115 `include/ADS1115/Config.h` conversion-read latch clearing,
INA228 `src/INA228.cpp` `readDiagAlertRaw`/`readAndClearDiagAlert`, and FieldCore
`include/TunnelMonitor/contracts/MotorControl.h` frozen target/generation,
owner deadlines and independent settlement/execution facts. These support
operation-specific effects and provenance; none supplies Lichuan wire behavior.
The lead's separate sibling review owns caller/test/runner/CI refresh for the stage.

Inspected local checker entrypoints; this checkout has no `.github` workflows.
`python scripts/check_promptset.py` passed (123 artifact hashes; 99 indexes,
202 entries, 53 manual items, 24 conflicts), `python scripts/check_lichuan_ledger.py`
passed, and `git diff --check` passed at reviewer handoff. These checks validate
the concurrent tree at that point, not the lead's later merged ledger revision.
No network, Git mutation, hardware action, CAN compilation or firmware test was
performed by this reviewer. Integrated checks and synchronization belong to
the lead's final prompt3 report.
