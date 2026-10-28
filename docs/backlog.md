# Executable backlog

This is the current work/evidence queue after the source audit. Requirements
and exact prompt owners remain in [the requirements matrix](requirements_matrix.md)
and [execution progress](prompts/PROGRESS.md); [coverage](coverage.md) records
what is actually evidenced. No driver function is implemented yet. Missing
device facts gate the smallest dependent feature, not all independent work.

## Documentary and firmware questions

| ID | Open fact and consequence | Owner / next concrete action |
| --- | --- | --- |
| D01 | Original data is verified, but the canonical reviewed implementation ledger does not exist | 2: account for99 indexes / 202 scalars plus manual-only claims with source/page, type/access/units/effect and explicit owners; 3 reviews semantics |
| D02 | EN86/ZH86/OP86 disagree on status bits3/7/10/12/14; generic CiA402 masks do not settle this | 3 retains alternatives; 29 obtains matched-firmware evidence; gate dependent interpretation/completion in19/30-36, allow evidenced raw reads |
| D03 | DO polarity/function addresses conflict in 2400; function 2/3 meaning and ZH57 terminal numbering also conflict | 3/21: review exact model/firmware mapping; 29 needs isolated authorized evidence, never try both disputed writes |
| D04 | 6040/6060 WO versus RW claims; 6061 monitored RO versus a translated write instruction | 2/3: preserve access claims and separate SDO/PDO policy; 19/30 must not assume read-modify-write or write observed mode |
| D05 | Velocity start order/sign and retained-target behavior at enable are unresolved | 29: reviewed finite experiment or applicable manufacturer clarification; gate30/35 transition semantics; no enabling preamble for31/32 |
| D06 | Actual feedback scale, subdivision 2201 default 0/range and 2202 default 10/choices0-1 disagree | 3/20/29: establish scale/default/soft-limit meaning; gate target conversion/reference in33/34/36, not pure checked arithmetic |
| D07 | Ramp objects 6083/6084/609A are ms, but zero behavior, stop options 605D and stop latency are unqualified; 605A/6085 are absent | 3/20/29/31/33-36: retain native time and exact unknowns; no generic quick-stop fallback or invented device timing guarantee; caller deadlines remain explicit policy |
| D08 | I/O filters and tuning 2500-2520 have incomplete units/ranges/effects; an EDS RW flag is not a safe setting | 2/3/21/22: assign every row once, keep opaque/reserved fields unresolved; no automatic tuning or protective-input reassignment |
| D09 | Node 2203 activation/DIP precedence, live COB-ID changes, 2300 save/reset completion and 2301 reset edge are incomplete | 3/29/32/38/39: separate volatile, observed and persistent state; no hidden save/restart, delay guess or replay |
| D10 | No drive-side host-loss stop guarantee; 1017 default 0 and missing 1016 do not prove one | 3/15/25/29: separate observed liveness, service health and physical behavior; identify required manufacturer/fixture evidence |
| D11 | Reused PDO ID/DLC can accept delayed old semantics despite a new host generation | 17/18/24/38/45: prove an applicable transition/retirement boundary, distinct route or observable marker; keep ambiguous samples unqualified |
| D12 | Generic EDS identity does not bind every CL/OL variant or installed firmware | 2-4/29/37: explicit model/profile provenance and identity limitations; N5/EPOS4 remain contrasts, not supported products |

The [object](audit/LICHUAN_OBJECT_FACTS.md), [motion](audit/LICHUAN_MOTION_FACTS.md)
and [public-code](audit/LICHUAN_PUBLIC_IMPLEMENTATIONS.md) notes locate primary
evidence. Reopen original tables before implementation; these notes do not
resolve the above facts by themselves.

## Software and integration work

| ID | Bounded work | Owner and completion evidence |
| --- | --- | --- |
| S01 | Stack selection and protocol-neutral common boundary | 4-6: source-backed contrasts, buildable fit experiment, ownership/resource decision |
| S02 | Native install/export and verification foundation | 7-8: C++11 source/install consumers, real test registration, generated checks and strict Doxygen |
| S03 | Exact status/unit/axis behavior | 9-10: atomic invalid-input results, extreme arithmetic, explicit reference/scale provenance |
| S04 | CAN frames, generated object policy and SDO lifecycle | 11-14: independent byte vectors, schema/freshness, deadlines, cancellation, raw aborts and late-reply ambiguity |
| S05 | Network/PDO and typed groups | 15-22: passive dispatch, observed configuration, explicit remap and typed read/settings/actions; retain feature gates |
| S06 | One owner with retained outcomes and passive diagnostics | 23-25: capacity/fairness/urgent admission, result lifetime, queue retirement and health-counter semantics |
| S07 | Shared examples and evidence harness | 26-29: real public path with fakes, thin adapter/framework entry points, conditional acquisition |
| S08 | Common motion and native commissioning/persistence | 30-39: each assigned supported function with positive/negative tests, Doxygen and failure evidence |
| S09 | Shared CLI and Python automation | 40-44: one inventory/schema, correlation, exact parser, finite campaigns and offline failures |
| I01 | Integrate delayed18/20-22 work through completed owner | Original owning prompts plus45: actual settings/remap traces, not only synthetic owner fixtures |
| I02 | FieldCore-like consumer and multi-drive contrasts | 45-46: IDs/generations/effects without importing FieldCore types; one physical receiver and protocol-specific services |
| I03 | Native package and support/documentation closure | 49-50: installed scope matches actual APIs/tests/firmware/qualification; no placeholder profile or invented release status |

## Tool and physical gates

| ID | Current limit | Owner / next action |
| --- | --- | --- |
| T01 | No CAN build/verifier/workflow/public headers exist yet | 7-8 establish real commands; do not claim the planning checker compiles a library |
| T02 | Clang/MSVC and ESP-IDF entry point absent from current PATH; IDF_PATH unset | 7/8/27/28 select and verify actual required tools in the appropriate environment; present audit is not blocked |
| T03 | Local Doxygen 1.13.2 differs from MCP45HVX1's 1.15.0 reference pin | 8 select/test a supported docs version; neither assume availability of 1.15.0 nor copy incompatible options blindly |
| H01 | No established physical CAN fixture or applicable live authorization | 29 fill [bench facts](hardware_bench.md), qualify isolated acquisition and retain raw data; no port guessing or hardware activity in this audit |
| H02 | No physical drive qualification, load/service-gap measurements or sustained tests | 47-48 run separately authorized finite/stress campaigns after software prerequisites; report exact fixture/model/firmware |

Close an item only with its named evidence. Update a newly affected prompt to
Partial/Blocked when necessary, keep its remaining work visible, and continue
independent assigned work. Commit and sync complete checked chunks on the
recorded execution branch. A raw accessor, UNSUPPORTED stub, simulated reply
or passing compile does not close an unimplemented semantic function or a
hardware question.
