# Prompt 6 common API and ownership proof

Date: 2026-10-28. Scope: all six implementation items and verification criteria
of [prompt 6](../prompts/6_api_ownership.md). Prerequisite prompts 3–5 were
complete at starting commit `fea0d94`. Work stays on
`promptset/motorcontrol-can`, upstream `origin/promptset/motorcontrol-can`.
The initial tree was clean and fresh fetch showed 0 ahead / 0 behind. This
assignment delivers a buildable prototype and semantic freeze, not production
arithmetic, an installed driver or hardware qualification.

## Delivered scope

| Requirement | Concrete evidence |
| --- | --- |
| Distinct architecture contracts | [Architecture](../architecture.md), [axis](../axis_contract.md), [profile](../profile_contract.md), [service](../canopen_services.md) and [owner](../can_owner.md) each own separate facts |
| Concrete common types | `Contracts.h`: exact intent, separate binding/endpoint/subaxis/generations, Classical frame, copied work/evidence, independent motor/feedback/mode facts and raw native abort |
| RS operation vocabulary and typed evidence | `Operations.h/.cpp`: prepareIdentity/nextRead/advanceRead/getIdentity and prepareMoveAbsolute/nextMove/advanceMove; atomic candidates, concrete terminal contexts and stale-provenance checks |
| One bounded ownership table | `CanBusOwner.h/.cpp`: one actual selected client, two axis reservations, two ordinary plus one urgent slot, four observations, canonical retained results, immutable operation deadlines and explicit release |
| Actual native flows | `main.cpp`: read and representative target-write through CANopenNode, copied payload after caller mutation, interleaved heartbeat, retained results across later transfers, explicit context advancement/release |
| Multi-drive boundary | `fixtures_test.cpp`: N5 stepper/BLDC, abstract EPOS4 servo without full steps, independent command/feedback/load scales, wide signed intent, proprietary address 500/subaxis 2, Classical extended/RTR and rejected FD, C10 ramp dimensions and C11 capability/calibration gates |
| Documentation and migration | Every public proof declaration documented; [proof README](../../experiments/api_ownership/README.md), API/CLI intent, source map, status/coverage/backlog/roadmap and changelog updated; prompt 7 migration and 9–10 replacement arithmetic explicit |

Synthetic objects `0x2000:00` and `0x2001:00` have no manufacturer applicability.
The target-write terminal is ACKNOWLEDGED with completion NOT_OBSERVED. No
complete drive motion sequence is invented. There is no installed placeholder
profile, universal result getter, virtual registry or alternate CANopen engine.
The common exact intent is wider than this profile's restricted I32 conversion;
its native bounds/origin and documentary ramp tags must stay profile-specific
during migration.

## Source refresh and review

The [source review](prompt_6_source_review.md) records exact declarations,
implementations, real callers, tests and workflow seams. RS
`bd831c859daa9c8c07b8d27a2dd879bb4e750511`, ADS1115
`235af3e8c722661a89a44766a4ccd903a654f6fe`, INA228
`b28c5ea69bcb4d26b351c52c24d3c6fae9fd282b` and FieldCore-node
`5ad9292dc8481cf08be9a61f662495a6474d1da0` remained clean and unchanged.
FieldCore was read-only. Unchanged sibling suites were not repeated.

Retained conventions: prepare/next/advance, checked typed getters, direct
context terminal evidence, Status/Ok/isOk/errToString and application
admit/admitUrgent/service/result/release/cancel. Deliberate CAN differences:
independent protocol routing, no mandatory step geometry/native RPM width,
unsigned raw aborts, immutable copied work/results and one continuous receiver.
Sensor consuming takeResult APIs were not copied into this owner.

Work used separate coding/source-fixture agents (gpt-6.1-sol, high) and an
independent boundary reviewer (gpt-6-astra, high). One agent owned the shared
headers; fixture work followed the type decision. The lead owned integration,
owner/flow code, final checks and Git. The
[independent review](prompt_6_review.md) attempted both required servo and
non-CANopen boundary cases against actual code and ran a fresh native build.

Review corrections: reduced accidental 64-byte FD storage to Classical bytes;
preserved partial-effect uncertainty after a checked write abort; separated
client reply errors from server rejection; corrected the supplied-time claim;
bounded the final uint64 clock interval so queued dispatch cannot overflow the
bridge timer; made boot-up invalidate work even with a full event ring; fixed
each reservation's operation deadline so later steps cannot renew its lease.
The initial static-library driver/client link cycle was removed by using a
local object target for the optional owner adapter, without another engine or
platform-specific linker workaround.

## Verification

Native Release/NDEBUG build with GCC/G++ 15.1.0 and authored-code
`-Wall -Wextra -Wpedantic -Werror`:

```text
cmake -S experiments/api_ownership -B .build/api_ownership -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build .build/api_ownership
ctest --test-dir .build/api_ownership --output-on-failure
```

All three registered tests pass: `api_ownership`, `api_fixtures`, and the reused
`canopen_service_fit`. Checks remain active with NDEBUG. Core-only fixtures link
only `api_contract`, with no stack/framework/sibling dependency. The owner tests
cover copied inputs, retained terminals, stale handles, ordinary/urgent capacity
and fairness, no same-operation step replay, absolute queue/handoff deadlines,
deadline-renewal rejection, cancellation before/after TX, failed-read quarantine,
wrong-object response, raw abort, asynchronous overflow and boot-up invalidation.

Focused Doxygen 1.13.2 covers all three public proof headers with undocumented
member/enum/parameter and documentation errors treated as failures; zero
warnings. This local contract check does not establish prompt 8's future
repository documentation gate. XML inspection separately confirmed complete
documentation for all core declarations and enum values.
The reproducible command is `doxygen experiments/api_ownership/Doxyfile`.

Measured x64 Release `sizeof`: `ServiceWork=112`, `ServiceEvidence=104`, complete
owner `1384` bytes. Fixed storage and absence of allocation/clock/I/O in the pure
core and proof adapter were inspected in source. Host test reporting uses I/O.
No peak-stack, heap-profiler, embedded memory, timing or hardware measurements
were made. No SDK/firmware build was attempted. The repository currently has no
CI workflow; no remote passing CI result is claimed.

`python scripts/check_promptset.py` passes all 50 stages/35 requirements,
the complete 99-index/202-entry/53-function ledger and all 134 original artifact
hashes. `python -m unittest discover -s scripts -p '*_test.py'` passes all 73
planning/ledger regressions. `git diff --check` passes. Staged/committed artifact
hashes and sync are checked during delivery below. Intermediate integrity
failures while new documentation links were unfinished were resolved before
acceptance; they did not expose a ledger or native-proof regression.

## Remaining scope and handoff

There is no prompt-6 hardware blocker: all required proof work is host-only.
Device firmware gates remain unchanged. The bridge needs an externally qualified
fresh route and cannot identify delayed duplicate replies using local tokens.
The bounded proof's channel stays unresolved after failed possible TX or boot-up;
it has no recovery shortcut. These premises, the limited segmented upload FIFO,
real controller retirement and native network/PDO/drive behavior retain their
precise later owners in [services](../canopen_services.md). Reservation ordering
does not imply previous-step success; dependent steps must wait for operation
advancement. None of these documented future milestones is counted as an
implemented motor feature.

Next assignment: prompt 7, register/migrate the retained proof into the native
package without installing synthetic profiles, stack dependencies or prototype
native-width assumptions as universal common behavior. This turn stops after
prompt 6. Git delivery uses the existing execution branch, normal push and a
fresh upstream comparison; no sibling edits, release or base-branch merge.

Delivery verification is finalized with the checked commit and remote state.
