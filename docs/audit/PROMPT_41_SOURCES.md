# Prompt 41 source and capability audit

Checked 2026-11-11. This is a read-only source audit of the console/application
seam. No sibling was edited or tested, and no hardware activity occurred.
The [object ledger](../reference/lichuan_objects.json),
[read contract](../reads.md) and original references retain device applicability
and evidence limits; this audit does not qualify a model or firmware.

## Current sibling evidence

| Repository | Refreshed HEAD and state | Adopted and rejected behavior |
| --- | --- | --- |
| MotorControl-RS | `cd27b12389f568198b72cecb6606123ea4c089bc`, clean | Console inventory/classification unchanged since prompt 40. The additional application seam below was inspected for prompt 41. Adopt explicit typed requests, retained outcomes and host selection invalidation; reject RTU register values, serial tuple grammar, addressing limits and device timing. |
| FieldCore-node | `5ad9292dc8481cf08be9a61f662495a6474d1da0`, clean | Ownership seam unchanged since prompt 39; not reread. `src/devices/rs485_motor/Rs485MotorModule.cpp::applyRuntimeConfig` rejects runtime address/baud changes, and `src/rs485/Rs485Task.cpp` retires work before reconfiguration. No CAN integration or persistence behavior is inferred. |

The additional RS implementation read was:

- `examples/probe_cli/ProbeApp.cpp::startTypedRead`: public preparation precedes
  admission; bounded retained records keep command and operation IDs separate.
- `ProbeApp.cpp::startDriver`: configuration exclusion and actual fresh
  group/state prerequisites govern settings admission.
- `ProbeApp.cpp::selectTarget`: busy work prevents selection; successful
  selection withdraws binding-dependent coordinates, scale and qualification.
- `examples/probe_cli/MotionProfileApp.h::motionProfileOperation` and
  `submitMotionProfile`: inspect and forget are local; restoration is explicit,
  preserves the original record and retains an uncertain write independently
  from later readback. CAN uses its existing typed three-field restore helper,
  rather than copying RS register packing.
- `examples/probe_cli/PersistenceApp.h::persistenceCaptureStart` and
  `servicePersistence`: explicit typed acquisition and retained command outcome
  remain separate from subsequent verification. RS does not establish a CAN
  save scope, restart requirement or flash completion interval.

## Original device sources reopened

PDF page numbers below are physical PDF pages, not printed footer numbers.
Sources are retained under [vendor references](../reference/vendor/).

| Fact | Original evidence and disposition |
| --- | --- |
| Requested versus observed mode | EDS `[6060]` is WO U8; `[6061]` is RO U8. Preserve requested `6060` and observed `6061` separately. EN86 PDF15's instruction to set the monitored mode does not authorize writing `6061`; access/signedness contradictions stay in the ledger. |
| Diagnostic outputs | EN86 PDF17 and ZH86 PDF18 document `2102:00` U16 bits0..2 as logical Y0..Y2 output status. Their RO label contradicts the EDS RW flag. Neither field is an output command or electrical-level measurement. |
| Output commands | EDS `[60FEsub1]`/`[60FEsub2]` are RW U32 physical outputs/mask. OP86 PDF7 describes user-defined DO0..2 at bits16..18 with both corresponding words' bits set. OP86 PDF5–14 and OP57 have identical text after whitespace normalization, so the copied 57C body supplies no independent 86C applicability. ZH57 PDF8 labels DO0..2, while PDF17 labels DO1..3. The 86C CANopen manuals do not supply that bit layout. Keep terminal identity/assignment and output writes gated. |
| Stored node setting | EN86/ZH86 PDF18 and EDS `[2203]` describe U16 CAN ID, manual range0..127. This is drive configuration, not host selection or proof of the active node. Zero is not an ordinary unicast endpoint; activation, DIP precedence and persistence require their own evidence. |
| Save parameters | EN86/ZH86 PDF15 and PDF18, EDS `[2300]`: U16 value2 saves current parameters. Listed homing examples do not establish an exhaustive saved set or durability/completion timing. |
| Restore factory settings | EN86/ZH86 PDF18, EDS `[2300]`: U16 value1 restores factory settings; zero is ineffective. No vendor restart command, rearm sequence or complete response/route/default behavior is established. Host re-application of a recorded configuration is a separate intent. |

These are documentary confirmations and preserved contradictions. No inference
from a related model, EDS default or successful host handshake replaces the
remaining firmware and physical evidence.

## Read policy and typed coverage

DEC-1 accepts unknown upload effects for exactly these 16 rows:
`1000:00`, `1009:00`, `100A:00`, `1018:00..03`, `2000:00`, `2001:00`,
`6041:00`, `6061:00`, `6064:00`, `606C:00`, `2100:00`, `2101:00`, `2102:00`.
This is owner risk acceptance, not a finding that reads are non-consuming.
`read_effect` remains `not_established`.

An ordinary command must check every selected descriptor before reserving or
admitting work. `ProbeApp::requestOrdinaryRead` applies this complete-plan check.
The separate existing reviewed acquisition paths remain explicit; low-level
service availability and an RO flag do not grant ordinary CLI permission.
An accepted prefix cannot justify transmitting part of a rejected plan.

| Family | Actual public path and remaining gate |
| --- | --- |
| Probe and identity | `prepareProbe/checkProbe`, `prepareIdentity/getIdentity`; the selected rows are on DEC-1. Presence/raw identity does not establish unique model or firmware support. Identity width is an explicit fixture choice. |
| State | `prepareState/getStateBlock`; STATUS, MONITORS and FEEDBACK use accepted rows. ERRORS, ERROR_HISTORY and INPUTS contain rows outside DEC-1 and ordinary requests are Q20 rejected. Unknown mode/status/I/O interpretation remains explicit. |
| Configuration and native groups | `prepareConfig/getConfig`, `prepareDriverRead/getDriver/getIo`, `prepareControlRead/getControl`, `prepareTuningRead/getTuning`. Their configuration/native objects are outside DEC-1. Typed acquisition exists; it is not ordinary read permission or setter coverage. |
| Driver settings | `prepareDriverSettings`: complete `6081` rpm0..3000 and `6083/6084` ramp-ms0..2000 candidate, fresh reserved prior/state and the actual acknowledged `6040=000F` prerequisite. No implicit enable. |
| Soft-limit settings | `prepareSoftLimitSettings`: only `2202` codes0/1; no `607D` setter or qualified enforcement/frame/reaction claim. |
| I/O settings | `prepareIoSettings`: explicit complete DI0..6 polarity/function candidate, codes0..13, fresh prior/state. DO layout/actions and filter setters remain withheld. Undefined function0 does not prove a protection is disabled. |
| Control and tuning | `prepareControlSettings`: only `2500` codes0..2; OL86 refuses closed loop. No encoder `2501` setter or `2502..2520` tuning setter. Preserve unknown units/ranges in typed reads. |
| Discovery and selection | `DiscoveryScan` uses an explicit bounded set of existing configured slots at the current protocol/interface/bitrate. Passive, probe and identity are separate intents. Findings do not bind a target or retire a timed-out channel. `ProbeApp::select` selects reviewed host route facts without writing `2203`. |
| Communication | `prepareCommunicationRead/getCommunication` acquire finite raw records, subject to Q20. `prepareCommunication` supports only the reviewed CL86-C `1017` raw0→1000 transition with exact baseline/review and explicit confirmation. No node, bitrate or arbitrary heartbeat setter follows from it. |
| PDO configuration | `Tpdo1ConfigurationContext` supports its exact reviewed map3→map2 and event-timer raw0↔100 transitions. Readback is not qualified live sample evidence; no unrestricted remapping route is justified. |
| NMT and passive network evidence | Existing owner `admitNmt/nmtResult/releaseNmt` retains local transmission evidence separately from observed node state. NMT stop is not motor quick stop; reset requests do not establish parameter reload. Passive debug/inspection copies ordinary owner traffic. |
| Persistence and host re-application | `prepareSave/prepareFactoryRestore` need actual prerequisites and explicit review; factory restore additionally needs external backup/route-back evidence. `verifyPersistence` cannot rewrite an uncertain command. `captureConfiguration/prepareConfigurationRestore` retain and re-apply only `6081/6083/6084`, without implicit save, reset, rollback or replay. |

Unsupported settings and unqualified reads must return their actionable gate.
An arbitrary object command, raw controller sender or CLI-only implementation
cannot substitute for a missing typed capability.
