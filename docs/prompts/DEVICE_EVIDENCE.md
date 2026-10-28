# Concrete Lichuan evidence for the numbered prompts

This reading map includes the 24 October enrichment and [28 October corrections](../audit/PROMPTSET_REVIEW_2026-10-28.md). The detailed notes below preserve source claims and disagreements; they are **not** the reviewed production ledger that prompts 2 and 3 must create. Read only the groups owned by the current prompt, reopen their cited original pages/EDS sections and apply COMMON's correction loop. No reference example authorizes traffic to a connected motor.

## Evidence and ownership

| Prompt owners | Concrete material to inspect | Required distinction |
| --- | --- | --- |
| 2, 3, 12 | [Object facts](../audit/LICHUAN_OBJECT_FACTS.md) and [complete EDS inventory](../reference/extracted/LC_StepDriverCAN_V1.1.object_inventory.json) | Lossless imported fields versus reviewed permissions, units and effects |
| 5, 11, 13, 14 | [Public implementation review](../audit/LICHUAN_PUBLIC_IMPLEMENTATIONS.md) and its pinned source files | CANopen mechanics versus third-party defects and application timing choices |
| 15, 16, 17, 18, 24, 38, 45 | Object facts: communication, identity and PDO records; public implementation review: constants and sample provenance | SDO access versus PDO direction; confirmed maps versus delayed same-route samples; device support versus stack behavior |
| 19, 20 | Object facts: observation, motor/current, subdivision and soft limits; [motion facts](../audit/LICHUAN_MOTION_FACTS.md) | Width/range/default/unit versus safe read effects and real position scale |
| 21 | Object facts: `0x2400`, `0x2401` through `0x2407`, `0x60FD/0x60FE` | Conflicting subindices, terminal labels, function numbers, masks and filter units |
| 22 | Object facts: complete `0x2500` through `0x2520` inventory | Gain/filter/current/resolution effects are not one generic writable group |
| 29, 30, 31, 32 | Motion facts: state dialects, enable/stop, fault clear and position clear | Requested command, protocol acknowledgement, observed drive state and physical result |
| 33, 34, 35, 36 | Motion facts: position, angle scale, signed velocity, homing and timings | Documented operation sequence versus disputed firmware behavior |
| 37, 38, 39 | Object facts: identity, DIP settings, communication and persistence | Host binding versus device configuration; save/reset effects versus guessed defaults |
| 25, 40 through 48 | Relevant object/motion groups plus public-code counterexamples | CLI/runners/health use reviewed public behavior and preserve raw evidence; they do not resolve missing firmware facts |
| 49, 50 | Source ledger, these notes, source manifests and coverage deltas | Documentary, implemented, host-tested and hardware-qualified are separate claims |

Original vendor documents and source IDs are in the [reference index](../reference/README.md) and [main manifest](../reference/SOURCES.json). Page numbers in the new notes are one-based **PDF pages**, not printed page numbers. The [fresh download comparison](../audit/LICHUAN_SOURCE_RECHECK.json) found all five PDFs and the EDS byte-identical to the retained copies on 24 October; changing HTML does not establish changed firmware. Public code has [its own URL, commit, hash and license manifest](../reference/public_code/SOURCES.json).

The [28 October repeat download](../audit/LICHUAN_SOURCE_RECHECK_2026-10-28.json) again found all five PDFs and the EDS byte-identical. The corrections concern interpretation and omitted contradictions, not new numeric constants or replaced vendor documents.

## Facts that must remain explicit

- CL86-C is a finished drive. Its object dictionary is an external CANopen interface, not the register map of an identified internal chip. CL2-86C, TL-C, RS485 and EtherCAT models need separate applicability evidence.
- The 99 EDS indexes and 202 explicitly typed scalar entries are an intake count. Metadata, subindex counts, reserved fields and action objects do not become 202 safe settings or supported APIs.
- `0x6040` is U16 but EDS says WO while manuals say RW. `0x6060/0x6061` are U8 in EDS and I8 in manual tables; the observed-mode object is not a command destination. Keep raw bytes and each access claim until reviewed.
- English, Chinese and operating-manual status layouts differ. Do not use a generic CiA402 status mask, a convenient translation or a public example to pick the runtime interpretation.
- Digital-output addresses, function values and terminal labels conflict even within the same manual. ZH57 labels `60FE` bits16/17/18 DO0/1/2 on p8 and DO1/2/3 on p17. A width check or silent renumbering cannot establish the physical terminal.
- `0x6083/0x6084/0x609A` are documented as ramp **times in ms**. SDO timeout, boot time, persistence completion, stop latency and drive action on host loss are not supplied as usable guarantees by the retained documents.
- `0x605A` and `0x6085` are not in the retained Lichuan dictionaries. Their generic CiA402 meanings do not establish support. `0x605D` exists but its supported options still need evidence.
- EDS producer heartbeat `0x1017` defaults to zero. No observed heartbeat is not proof of an offline node, and host-side heartbeat supervision does not prove a motor watchdog.
- Never execute the EDS defaults or third-party initialization sequence. No implicit enable, save, home, fault clear, position clear, network reset/start or output reassignment belongs in startup, probing or recovery.
- Never interpret the stop/reset arrows as an instruction to enable first. Derive an evidenced current-state transition or reject before TX.
- PDO mapping readback does not qualify delayed samples on a reused route. Host generations are not on the wire; keep ambiguous same-ID/same-length observations unqualified until the reviewed provenance contract passes.

## Per-feature evidence record

Keep the schema simple: object/bit/sequence/timing claim; exact value or explicit unknown; source ID and page/section or repository commit/path/lines; model/firmware applicability; conflicting claims; disposition; owning prompt; required observation. For a timing also record its unit and origin (vendor example, specified bound, standard unit, caller policy or measurement). For an address include subindex, width, sign, access, meaning, mapping eligibility and side effects. For a trigger include its edge, prerequisites and what later evidence can prove.

Use the full inventory to avoid omissions, not to bypass this review. A corrected ledger drives code generation, typed APIs, tests, CLI display and documentation. Tests must have independent expected values from cited sources; a test computed by the same importer only checks self-consistency. Give source extraction and implementation review to different agents when useful, keeping one lead responsible for the merged facts and the existing single branch.

Missing firmware or hardware evidence blocks only the dependent feature. Finish documentary accounting and independently supported software work, retain the exact open obligation, and use prompt 29's reviewed, separately authorized experiment path when measurements are needed. Do not invent a value to make a prompt Complete.
