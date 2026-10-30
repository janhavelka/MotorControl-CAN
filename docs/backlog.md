# Executable backlog

This is the current work/evidence queue after the Lichuan review,
[stepper/servo contrasts](reference/drive_contrasts.md) and
[CANopen service decision](decisions/canopen_stack.md). Requirements
and exact prompt owners remain in [the requirements matrix](requirements_matrix.md)
and [execution progress](prompts/PROGRESS.md); [coverage](coverage.md) records
what is actually evidenced. A [synthetic API/owner proof](reports/prompt_6.md) now
exists; finite [raw Lichuan reads](reads.md) are now implemented. Missing
device facts gate the smallest dependent feature, not all independent work.

## Documentary and firmware questions

| ID | Open fact and consequence | Owner / next concrete action |
| --- | --- | --- |
| D01 | **Documentary review closed:** the [reviewed dictionary](reference/lichuan_objects.md) covers 99 indexes, 202 entries and 53 manual functions, exact sources, effect/access policy, four model scopes and 21 qualification gates | [Prompt 3](reports/prompt_3.md) complete; 12 later generates reviewed policy. No feature implementation or firmware qualification closed by this documentary result. |
| D02 | EN86/ZH86/OP86 disagree on status bits 3/7/10/12/14; ZH57 p12 has a separate 57C bit-7 alarm claim. Generic CiA402 masks do not settle the 86C conflict | Q02: 29 obtains matched-firmware evidence; 19/30-36 gate only dependent interpretation/completion. Consistent bits 8/15 still need fresh operation evidence; raw reads remain distinct. |
| D03 | DO polarity/function addresses conflict in 2400; function 2/3 meaning and ZH57 terminal numbering also conflict | Q06/Q07: 21/29 need selected-model mapping and isolated output/mask evidence. 57C labels do not block 86C; never try both disputed writes. |
| D04 | 6040/6060 WO versus RW claims; 6061 monitored RO versus a translated write instruction | Reviewed conservative policy: WO commands, RO display, separate PDO eligibility. 19/20/30 preserve this; Q10/Q11 qualify mode-change effects, without read-modify-write or writes to observed mode. |
| D05 | Velocity start order/sign and retained-target behavior at enable are unresolved | 29: reviewed finite experiment or applicable manufacturer clarification; gate 30/35 transition semantics; no enabling preamble for 31/32 |
| D06 | Actual feedback/offset/limit scales, subdivision 2201 default 0/range and 2202 default 10/choices 0-1 disagree; 607D operating bounds are absent | Q04/Q05/Q14: 20/29 establish selected-field meaning; gate conversion/reference in 33/34/36, not raw observations or pure arithmetic. Do not copy target limits/units into another object. |
| D07 | Ramp objects 6083/6084/609A are ms, but zero behavior, stop options 605D and stop latency are unqualified; 605A/6085 are absent | Q11/Q12: 20/29/31/33-36 retain native time and unknowns; no generic quick-stop fallback or invented device timing guarantee. Caller deadlines remain explicit policy. |
| D08 | I/O filters and tuning 2500-2520 have incomplete units/ranges/effects; an EDS RW flag is not a safe setting | Q08/Q09: 21/22 qualify the selected field; documentary review is complete. Opaque/reserved fields stay restricted, peak current stays distinct from RMS, and 250C has no assumed time unit. |
| D09 | Node 2203 activation/DIP precedence, live COB-ID changes, 2300 save/reset completion and 2301 reset trigger/rearm are incomplete | Q13/Q15/Q16: 29/32/38/39 separate volatile, observed and persistent state; manual reset-bit return is not an established edge/pulse. No hidden save/restart, delay guess or replay. |
| D10 | No drive-side host-loss stop guarantee; 1017 default 0 and missing 1016 do not prove one | Q18: 15/25/29 separate observed liveness, service health and physical behavior. Scalar widths also do not establish all-expedited uploads; 13 handles actual service form. |
| D11 | Reused PDO ID/DLC can accept delayed old semantics despite a new host generation | 17/18/24/38/45: prove an applicable transition/retirement boundary, distinct route or observable marker; keep ambiguous samples unqualified |
| D12 | Generic EDS identity does not bind every CL/OL variant or installed firmware | Q01: 29/37 retain explicit model provenance; raw discovery may precede binding. Four model entries distinguish OL no-encoder hardware from unknown software capability. Prompt 4's N5/EPOS4 contrasts are documentary evidence, not installed profiles. |
| D13 | Read consumption and complete 60FD bit layout are not established by source access flags | Q20/Q19: 19 implements raw values with unknown effects. Its non-consuming probe remains open until applicable read-effect evidence exists. Do not copy 2101 bits or infer a non-consuming read. |
| D14 | No complete local/remote settlement boundary for reopening an unresolved Lichuan SDO channel; no proven second server channel | 14 implements permanent quarantine and explicit unsupported capabilities. Q18/Q16/Q20 plus model provenance gate any later reopening: prove old TX/RX retirement, server session bounds and permitted confirmation, not a quiet delay or host epoch. See [source evidence](reports/prompt_14_sources.md). |
| D15 | Lichuan applicability of standard variable/dynamic remapping, device restrictions and activation/readback remain unconfirmed | Q17: [Prompt 18 is Blocked](reports/prompt_18.md#fresh-independent-audit). Confirm the selected subset, then implement its typed sequence and every-step failures in 18. Timer/SYNC facts gate only dependent settings; sample qualification stays separate. Keep persistence effects explicit. |

The canonical [qualification checklist](reference/lichuan_review.md) and ledger
give the exact required observations and selected-model scope for Q01-Q21.
The [object](audit/LICHUAN_OBJECT_FACTS.md), [motion](audit/LICHUAN_MOTION_FACTS.md)
and [public-code](audit/LICHUAN_PUBLIC_IMPLEMENTATIONS.md) notes locate primary
evidence. Reopen original tables before implementation; these notes do not
resolve the above facts by themselves.

## Software and integration work

| ID | Bounded work | Owner and completion evidence |
| --- | --- | --- |
| S01 | Stack selection and protocol-neutral common boundary | Complete for 5–6: [CANopenNode decision](decisions/canopen_stack.md), [concrete contracts](architecture.md), copied flows and contrast fixtures. Numeric and coordinate implementation now live in production Units/Axis; real profile/service integration retains its later owners. |
| S02 | Native install/export and verification foundation | Complete for 7–8: isolated consumers, quick/full verification, CI and strict Doxygen; exact results in the [prompt 8 report](reports/prompt_8.md) |
| S03 | Exact status/unit/axis behavior | 9 complete: production [exact units](units_status.md). 10 complete: [axis/reference/origin/path policy](axis_contract.md), checked targets and offline preview; local and Windows/GCC/Clang sanitizer CI pass in [its report](reports/prompt_10.md). Native device conversions and finite motion remain 20/33-36. |
| S04 | CAN frames, generated object policy and SDO lifecycle | 11 complete: installed pure frame/SDO checks, full abort evidence, atomic word decoding and real selected-engine vectors pass local full and Windows/GCC/Clang sanitizer CI; [report](reports/prompt_11.md). 12 complete: reviewed generation/freshness, optional strings and full accounting pass local and Windows/GCC/Clang sanitizer CI; [report](reports/prompt_12.md). 13 delivers the production optional SDO lifecycle: immutable cutoffs, bounded 32-byte FIFO draining, checked replies, retained effects and persistent per-channel quarantine. 14 adds timestamped observations, explicit precedence, copied-TX eligibility and adversarial settlement tests; unresolved channels remain quarantined because reopening evidence is absent. See [report](reports/prompt_14.md). |
| S05 | Network/PDO and typed groups | 15 implements checked passive events and synchronous lifecycle invalidation in the existing owner table; 16 implements explicit NMT intent/TX/observations, domain invalidation and service eligibility; 17 completes actual-map/passive sample provenance and checked decoding; 19 implements 30 finite raw read rows and checked getters; Q20 blocks its non-consuming probe. 20 adds nine native driver/limit/motion-parameter read rows; its explicit write prerequisites and effects remain open. 18/21-22 retain explicit configuration/remap and settings groups with feature gates. |
| S06 | One owner with retained outcomes and passive diagnostics | 6 proves one fixed table, fairness/urgent capacity, copied results, deadlines and fake retirement. 13 promotes its selected-stack SDO storage/lifecycle; 15 extends passive events and dependency invalidation; 16 adds explicit NMT and eligibility in that same storage; 23 still owns full-owner dispatch; 24–25 add qualified recovery and health-counter semantics. |
| S07 | Shared examples and evidence harness | 26-29: real public path and thin adapters. Inspected TWAI queue clear cannot retire an active hardware frame; 27 must verify actual controller evidence and synchronization. Conditional acquisition remains 29. |
| S08 | Common motion and native commissioning/persistence | 30-39: each assigned supported function with positive/negative tests, Doxygen and failure evidence |
| S09 | Shared CLI and Python automation | 40-44: one inventory/schema, correlation, exact parser, finite campaigns and offline failures |
| S10 | Resume typed PDO configuration after Q17 evidence | 18 owns preparation/progression, immutable deadlines, exact partial effects, reviewed settings and final readback publication. Reuse the existing SDO/PDO owner; integrate with the completed application owner if available on resumption. No always-rejecting stub or generic vendor sequence completes this item. |
| I01 | Integrate delayed 18/20-22 work through completed owner | Original owning prompts plus 45: actual settings/remap traces, not only synthetic owner fixtures |
| I02 | FieldCore-like consumer and multi-drive contrasts | 45-46: IDs/generations/effects without importing FieldCore types; one physical receiver and protocol-specific services |
| I03 | Native package and support/documentation closure | 49-50: installed scope matches actual APIs/tests/firmware/qualification; no placeholder profile or invented release status |

## Tool and physical gates

| ID | Current limit | Owner / next action |
| --- | --- | --- |
| T01 | Minimal native package, registered proofs and verification workflow exist | Current quick/full and package gates validate software; the planning checker validates original evidence. See the [maintained gate table](verification.md). |
| T02 | Local Clang/MSVC and ESP-IDF entry point absent; local GCC sanitizer runtimes absent; IDF_PATH unset | GCC/Clang sanitizer gates passed in Linux CI. Local requests for unavailable tools fail honestly; 27/28 establish actual SDK tools. MSVC is not a current gate. |
| T03 | Strict Doxygen 1.13.2 selected and tested on Windows and Linux | 8 complete: exact release/source pin and real documentation failures checked; maintain the pin and warning policy with future public APIs |
| H01 | No established physical CAN fixture or applicable live authorization | 29 fill [bench facts](hardware_bench.md), qualify isolated acquisition and retain raw data; no port guessing or hardware activity in this audit |
| H02 | No physical drive qualification, load/service-gap measurements or sustained tests | 47-48 run separately authorized finite/stress campaigns after software prerequisites; report exact fixture/model/firmware |

Close an item only with its named evidence. Update a newly affected prompt to
Partial/Blocked when necessary, keep its remaining work visible, and continue
independent assigned work. Commit and sync complete checked chunks on the
recorded execution branch. A raw accessor, UNSUPPORTED stub, simulated reply
or passing compile does not close an unimplemented semantic function or a
hardware question.
