# Implementation roadmap

Execution identity: version 2, started 2026-10-28 on
`promptset/motorcontrol-can`, base `7878f7c`. Keep this branch for the complete
promptset and resumed sessions. [Prompt 1](reports/prompt_1.md) and
[prompt 2](reports/prompt_2.md), [prompt 3](reports/prompt_3.md) and
[prompt 4](reports/prompt_4.md), [prompt 5](reports/prompt_5.md) and
[prompt 6](reports/prompt_6.md), [prompt 7](reports/prompt_7.md) and
[prompt 8](reports/prompt_8.md) and [prompt 9](reports/prompt_9.md) are complete.
[Prompt 10](reports/prompt_10.md) is complete with pure axis targets, offline
preview and passing local/Windows/GCC/Clang sanitizer CI.
[Prompt 11](reports/prompt_11.md) is complete with pure frame/SDO checks and passing
local full plus Windows/GCC/Clang sanitizer CI. [Prompt 12](reports/prompt_12.md)
is complete with reviewed generation and passing local/Windows/GCC/Clang
sanitizer CI. The next numbered assignment is prompt 13;
this handoff does not execute later prompts in this turn.

The [sequence manifest](prompts/SEQUENCE.json) owns exact dependencies;
[PROGRESS](prompts/PROGRESS.md) owns stage status. This roadmap gives executable
milestones, not another dependency graph. Execute numeric order among ready
work, commit/sync each checked prompt or useful complete chunk, and preserve
blocked obligations under [COMMON](prompts/COMMON.md).

| Prompts | Deliverable and boundary | Current state / next acceptance |
| --- | --- | --- |
| 1 | Current source map, live delta, tools, bench facts and backlog | Complete; evidence and checks in its report |
| 2-4 | Full object/source intake, reviewed Lichuan semantics, N5/EPOS4 contrasts | Complete: 99 indexes / 202 entries and 53 manual functions reviewed; four Lichuan model scopes and 21 open feature gates. [Test-only contrast ledger](reference/drive_contrasts.md) covers N5 stepper/BLDC, EPOS4 and protocol boundary cases without claiming support. |
| 5-6 | Buildable stack fit experiment and common API/ownership proof | Complete: one selected engine, concrete common contracts, one fixed owner table and checked read/write/contrast fixtures. Production package/units now exist under 7–9; production axis/reference policy now exists under 10. |
| 7-8 | Native package, source/install consumers, CI/verifier and Doxygen | Complete: minimal C++11 core with isolated source/install/export consumers, quick/full verification, strict Doxygen and passing Windows quick and Linux GCC/Clang sanitizer CI. Exact results are in the stage reports. |
| 9-10 | Status, exact units and axis/reference conversions | 9 complete: production exact units/status and quantization with local/CI evidence. 10 complete: pure axis/reference conversions, offline preview and local/Windows/GCC/Clang sanitizer CI. No native RPM width, mandatory step geometry or guessed feedback scale. |
| 11 | Classical CAN and checked SDO codec boundary | Complete: local full and exact-commit Windows/GCC/Clang sanitizer CI pass. One selected encoder, explicit channel/width correlation, full aborts and unchanged failure outputs. |
| 12 | Reviewed generated object metadata and coverage | Complete: 194 resolved descriptors, four withheld/four excluded and active freshness; local and exact-commit Windows/GCC/Clang sanitizer CI pass. No semantic operation is implemented. |
| 13-14 | Bounded service/reconciliation | Pending; integrate pure checks with the selected engine, preserve publication/ownership and late identical-response uncertainty |
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

Next assignment is [prompt 13](prompts/13_sdo_lifecycle.md),
bounded SDO transfer lifecycle over the implemented pure SDO boundary and
[reviewed object metadata](reference/lichuan_generated.md),
[axis](axis_contract.md) and [production units](units_status.md), under the established
[verification gates](verification.md). The
[reviewed Lichuan dictionary](reference/lichuan_objects.md) and
[contrast cases](reference/drive_contrasts.md) are documentary inputs;
[Lichuan qualification gates](reference/lichuan_review.md) remain open for the
affected later features. No profile is implemented yet.

The former stage numbering is mapped in [SPLIT_MAP](prompts/SPLIT_MAP.md).
Use current owners 11/12 for codecs/catalogue, 13/14 for services, 15-18 for
network/PDO, 19-22 for typed reads/settings, 30-32 for actions, 33-36 for motion,
and 37-39 for commissioning/persistence. Do not restore the old stage 08
catch-all. Preserve all supplied prompts and historical audit evidence.
