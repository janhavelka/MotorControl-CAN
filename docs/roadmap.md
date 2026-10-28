# Implementation roadmap

Execution identity: version 2, started 2026-10-28 on
`promptset/motorcontrol-can`, base `7878f7c`. Keep this branch for the complete
promptset and resumed sessions. [Prompt 1](reports/prompt_1.md) and
[prompt 2](reports/prompt_2.md) are complete. The next assigned work is prompt 3;
this handoff does not execute later prompts in this turn.

The [sequence manifest](prompts/SEQUENCE.json) owns exact dependencies;
[PROGRESS](prompts/PROGRESS.md) owns stage status. This roadmap gives executable
milestones, not another dependency graph. Execute numeric order among ready
work, commit/sync each checked prompt or useful complete chunk, and preserve
blocked obligations under [COMMON](prompts/COMMON.md).

| Prompts | Deliverable and boundary | Current state / next acceptance |
| --- | --- | --- |
| 1 | Current source map, live delta, tools, bench facts and backlog | Complete; evidence and checks in its report |
| 2-4 | Full object/source intake, reviewed Lichuan semantics, N5/EPOS4 contrasts | 2 complete: 99 indexes / 202 typed entries and 52 manual semantic rows accounted; next work 3 reviews meaning/model scope, then 4 contrasts |
| 5-6 | Buildable stack fit experiment and common API/ownership proof | Pending; choose one service backend; prove stepper/servo and non-CANopen boundary without placeholder drivers |
| 7-8 | Native package, source/install consumers, CI/verifier and Doxygen | Pending; reconcile available tools with actual selected pins; no current CAN build/CI target exists |
| 9-10 | Status, exact units and axis/reference conversions | Pending; no native RPM width, stepper geometry or guessed feedback scale in common intent |
| 11-14 | CAN/SDO codecs, reviewed generated dictionary, bounded service/reconciliation | Pending; preserve full aborts, publication/ownership and late identical-response uncertainty |
| 15-18 | Passive network events, explicit NMT, observed PDO maps and remapping | Pending; qualify sample provenance separately from map contents; 18 may require device evidence |
| 19-22 | Typed reads, motion/scale, I/O/output and tuning groups | Pending; only evidenced fields become usable APIs; record partial effects and unresolved access/units |
| 23-25 | Owner dispatch/results, urgent/cancel/recovery, passive health/statistics | Pending; path depends on 17/19, not completed 18/20-22; synthetic owner fixtures do not implement drive settings |
| 26-28 | Native example, TWAI adapter, shared Arduino/ESP-IDF application | Pending; choose actual SDK/board configuration; compile without assuming a connected fixture |
| 29 | Evidence harness, fixture record and conflict acquisition | Software readiness can finish offline; physical acquisition is conditional and separately authorized |
| 30-32 | Enable/release, stop, fault/reference actions | Pending; use qualified current-state transitions, never enable as a stop/reset preamble |
| 33-36 | Position, angles, finite velocity and homing | Pending; explicit trigger/acceptance/completion, reference/scale and bounded deadlines |
| 37-39 | Discovery, communication changes, save/reset | Pending; no writes in discovery or hidden persistence; retain uncertain route/partial effects |
| 40-42 | Strict CLI parser, typed device and motion routes | Pending; one shared app/inventory, exact units, cached versus refreshing routes and schema correlation |
| 43-44 | Python session/parser and finite campaigns | Pending; offline/fake tests plus explicit connection and retained failure evidence; no uncertain-write replay |
| 45-46 | Integrated faults and FieldCore-like/alternate-drive consumers | Pending; test real resumed settings/remap through owner and preserve framework independence |
| 47-48 | Finite physical tests and sustained-load qualification | Pending; actual bench and authorization required; no measured CAN evidence exists |
| 49-50 | Package/docs audit and final review | Pending; software verdict and physical qualification stay separate; no release publication implied |

Next assignment is [prompt 3](prompts/3_lichuan_semantics.md), beginning with the
[source intake ledger](reference/lichuan_objects.md). Its imported metadata is
not an approved writable policy or a supported drive feature.

The former stage numbering is mapped in [SPLIT_MAP](prompts/SPLIT_MAP.md).
Use current owners 11/12 for codecs/catalogue, 13/14 for services, 15-18 for
network/PDO, 19-22 for typed reads/settings, 30-32 for actions, 33-36 for motion,
and 37-39 for commissioning/persistence. Do not restore the old stage 08
catch-all. Preserve all supplied prompts and historical audit evidence.
