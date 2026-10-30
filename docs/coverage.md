# Requirement coverage after pure CAN/SDO checks

The [requirements matrix](requirements_matrix.md) owns the full acceptance criteria.
This ledger tracks the actual milestone evidence and retains every requirement ID
and its current prompt owners. The [source map](source_map.md) and
[prompt 1 report](reports/prompt_1.md) and [prompt 2 report](reports/prompt_2.md)
substantiate the audit and intake entries. [Prompt 3](reports/prompt_3.md) and
the [reviewed dictionary guide](reference/lichuan_review.md) substantiate the
completed documentary policy and remaining feature-specific gates. The
[prompt 4 contrast ledger](reference/drive_contrasts.md) records test-only
N5, EPOS4 and proprietary-frame cases; it adds no installed support.

[Prompt 5](reports/prompt_5.md) adds an actual native CANopenNode fit experiment
and [one selected service boundary](decisions/canopen_stack.md). It establishes
host proof evidence only. [Prompt 6](reports/prompt_6.md) adds concrete contracts,
an actual owner/read/write proof and core-only drive contrasts. Production
controller and vendor drive support remain pending. [Prompt 7](reports/prompt_7.md)
adds the [minimal package](getting_started.md), version freshness and source/install/export
tests. Status, endpoint identity, Classical frame validation and prompt 9's
[exact units/quantization](units_status.md) and prompt 10's [pure axis/reference API](axis_contract.md) and prompt 11's [pure SDO checks](canopen_services.md) are installed; no motion profile is installed.
[Prompt 8](reports/prompt_8.md) adds real quick/full verification, strict public
documentation and passing Windows quick and Linux GCC/Clang sanitizer CI.

The [fresh prompt-14 audit](reports/prompt_14_audit.md) strengthens R12/R20
with segmented-upload cancellation boundaries and a toggle-wrap counterexample.
It also supplies current independent review evidence for R32. No runtime or
hardware-support claim changed.

The [fresh audit of prompts 15-17](reports/prompts_15_17_audit.md) adds regressions
for initial service-proof age/origin, independent matching-response history and
unknown-capture forwarding. R05/R11/R20/R32 retain host-only scope.

[Prompt 18](reports/prompt_18.md) completed its source and prerequisite review
but remains **Blocked** on Q17 mutation semantics. No remapping/timing operation
or failure-at-each-mutation-step test is delivered; R12/R13 retain that scope.
Existing PDO layout/readback/provenance tests are prerequisite evidence only.
The [fresh Prompt 18 audit](reports/prompt_18.md#fresh-independent-audit)
corrects the standard-versus-device evidence distinction and narrows Q17's
dependencies; it grants no new runtime capability or hardware qualification.

**Pending** means no CAN implementation, corresponding CAN host test, firmware
build or hardware qualification has been delivered for that requirement. Finite raw Lichuan reads are implemented under 19; no operational drive
profile or hardware qualification is claimed. The small installed API does not implement motion; prototype evidence below does not change
that support claim. Reference/data verification is not driver testing.

| Requirement | Prompt owners | Evidence now / remaining scope |
| --- | --- | --- |
| R01 | 1-2; recurring all | Prompt 14 refresh records current RS ownership, cancellation and retained-outcome implementations/tests; source map links current proof declarations. Recurring audits remain. |
| R02 | All | Prompt 14 refresh inspects ADS1115/INA228 cancellation/effect/result seams and actual callers/tests; differences from CAN result/release retained. Recurring audits remain. |
| R03 | 5-10, 40-42, 49-50 | Prompt 6 implements prepare/next/advance, typed getIdentity and concrete contexts plus admit/admitUrgent/service/result/release/cancel in the prototype; pure units/axis APIs are now installed under 9-10; 19 adds installed typed raw read contexts/getters; motion operations/CLI remain pending. |
| R04 | 5-12, 15, 17, 26-28, 45-46, 49-50 | C++11 api_contract and fixtures build without stack/SDK/Python/siblings; optional owner uses the selected C99 client. Prompt 7 source/install/export consumers pass; the core excludes the optional owner/stack. |
| R05 | 5-6, 13-18, 23-25, 45-46 | [Owner contract](can_owner.md) fixes one table, lifetimes and per-call RX/TX/event budgets including zero-budget behavior. Native tests cover copied payload, single RX, reservations, urgent capacity, immutable operation deadlines and retained terminals. Prompt 13 promotes the selected-stack SDO lifecycle with persistent channel quarantine; 15 adds passive node facts and synchronous invalidation to that table; full-owner extensions remain 23. |
| R06 | 2-4, 19-22, 29-36, 47-50 | Pending |
| R07 | 1-4 | Original bytes verified; every EDS entry and all 53 manual functions reviewed with exact sources. Four Lichuan model scopes and N5/EPOS4 documentary contrasts complete. |
| R08 | 2-4, 19-22, 29-36, 47-48 | All 24 conflicting claim sets retain original alternatives and reviewed dispositions; 21 bounded feature gates remain open. No firmware winner invented. |
| R09 | 9-10, 21-22, 32-36 | Production exact rational parsing/conversion, independent scales, integer rounding and bounded radians/binary32 quantization are host tested. Pure axis/reference mutation, exact endpoints, freshness/epoch checks and offline preview are host tested under 10; native scaling and drive actions retain later owners. |
| R10 | 9-10, 13-25, 30-32 | Production Status preserves selected call-result semantics/static labels. Production SDO checks and the proof preserve raw U32 aborts separately; acknowledged write retains completion NOT_OBSERVED. Real drive/health/state behavior remains pending. |
| R11 | 11-18, 23-25 | Real expedited/two-segment SDO proof plus prompt-6 continuous raw event dispatch and boot-up invalidation even when the observation ring is full. Prompt 11 adds installed checked initiation envelopes, legal segmented/abort evidence and real selected-engine vectors. Prompt 13 implements the optional bounded SDO lifecycle and checked 1-32-byte forms; 15 adds pure network codecs, shared node facts and reset/cache invalidation; 16 adds explicit NMT and service eligibility; 17 adds checked actual maps, qualified raw PDO observations and per-route invalidation; 19 adds checked raw SDO/PDO projection with per-field age/skew; remapping remains 18. |
| R12 | 13-14, 21-25, 30-39, 45-46 | Production SDO tests cover copied intent/results, bounded queue/response/overall deadlines, per-route quarantine, abort cleanup and no replay. The [independent prompt-13 audit](reports/prompt_13_audit.md) adds abort-during-staging and reply-before-controller-confirmation regressions. Prompt 14 adds separate capture/processing observations, adversarial correlation, copied-TX eligibility, binding invalidation and permanent quarantine where reopening evidence is missing. Real SDK retirement remains pending; [source/evidence gates](reports/prompt_14_sources.md) stay explicit. |
| R13 | 2-4, 19-22, 30-39, 49-50 | Reviewed documentary access/effect/value policy and exact native owners exist. Prompt 12 generates 194 compact descriptors and complete source/semantic coverage with active freshness checks; four conflicting types and four reserved entries are withheld/excluded. 19 implements 30 raw read rows with separate read coverage; all remaining read rows have explicit owners 20-22/38. Q20 blocks the non-consuming probe. Descriptors do not complete setters or actions. |
| R14 | 21-25, 29-36, 47-48 | Pending |
| R15 | 21-22, 29, 32-36, 47-48 | Pending |
| R16 | 37-39 | Pending |
| R17 | 26-28, 40-42, 49-50 | Pending |
| R18 | 40-44 | Pending |
| R19 | 29, 43-44, 47-48 | Pending |
| R20 | 7-28, 30-46, 49-50 | Thirty-four explicit CTests cover core/units/axis, Classical frames, SDO checks/selected-engine vectors, preview/native consumer/proofs, version/checker/verification/doc failures and planning evidence. Quick runs 32; full runs 33 plus a retained package contract. Generator/object/header and lifecycle tests use the existing registration; typed read tests and preview extend the same registration; current integrated evidence is in the [prompt-19 report](reports/prompt_19.md). |
| R21 | 1-2, 5-6, 23-25, 45-46 | Prompt-6 read-only FieldCore refresh plus common identity/configuration generations and exact-intent fixtures. Actual FieldCore-like integration remains 45–46; no framework types imported. |
| R22 | All | Execution branch/base recorded; this prompt uses checked commit/sync delivery; recurring duty. |
| R23 | All; final 49-50 | Current audit, roadmap, backlog, bench and support limits documented; release scope pending. |
| R24 | All; architecture 5-6 | One selected stack and one documented fixed ownership table, no runtime selection or universal operation engine. Host owner with two node/SDO slots, one NMT result and eight PDO routes sizeof 10528 bytes on Windows x86-64, including transient initial service/SDO times; embedded sizing, timing and controller qualification pending. |
| R25 | 7-8, 49-50 | Prompt 7 delivers C++11 core, installed/exported consumers, independent headers, C++17/no-RTTI checks, exact distribution lists and MIT attribution. Prompt 8 adds working verifier/CI/strict docs; final delivery status remains in its report. |
| R26 | 1-10, 45-46, 49-50 | Core-only N5 stepper/BLDC and EPOS4 fixtures pass with optional full-step geometry, independent scales and C11 capability/calibration gates. No additional installed driver or vendor sequence. |
| R27 | 5-6, 9-18, 23-25, 45-46 | Generic proprietary address 500/subaxis 2 and servo-without-CANopen-state review attempts pass. Synthetic CANopen profile/owner reject unsupported protocol before admission. Separate real protocol implementation pending. |
| R28 | 5-6, 11-12, 15, 17, 23-28, 45-46 | Classical eight-byte values preserve extended/RTR shape; FD and Classical BRS/ESI combinations explicitly fail. Installed SDO checks narrow service frames while common source-backed VESC/ODrive shapes remain valid. Controller adapters and FD implementation remain pending. |
| R29 | 2-4, 9-10, 21-22, 30-36, 45-46 | Production tests exercise independent command/feedback/load/linear scales and exact time dimensions without full-step geometry. C10 keeps Lichuan ms distinct from acceleration. Pure axis/reference policy is implemented; native/physical scaling qualification remains pending. |
| R30 | 5-6, 29, 45-50 | Explicit test-only N5/EPOS4/proprietary fixtures and synthetic profile do not install or advertise another driver. Actual additional profile/qualification milestones remain pending. |
| R31 | All; 1-2, 9-10, 23-25, 40-42 | Literal conventions mapped to actual prototype functions, contexts and owner methods, with current RS/I2C source evidence. Production motion operations/CLI/health remain pending; pure units and axis targets are installed. |
| R32 | All | Prompt 6 separated header ownership, source/fixture work and stronger independent boundary review; corrections verified against actual code and independent native probes. The [fresh prompts 10-11 audit](reports/prompts_10_11_audit.md) repeats arithmetic, protocol and package review against current source; no production defect found. Lead owns integration and Git. |
| R33 | All; tooling 7-8, final 49-50 | Five distinct contracts, proof README, source comments and status docs accompany prompt 6. Focused Doxygen on all public proof headers passes without warnings; prompt 8 now enforces strict installed API docs and extraction with real failure fixtures. |
| R34 | All | Same actual upstream engine reused with a direct fixed-table owner and explicit prototype limits; no second engine or runtime selector. Production integration remains pending. |
| R35 | 1-50 | Current owners retained; prompts 1-17 complete; 18 Blocked and 19 Partial, with exact local/CI evidence in their reports. All later feature, integration and qualification obligations remain pending. |

Later stages replace Pending with links to real APIs, tests and evidence, keeping
documented, implemented, host-tested, firmware-built and hardware-qualified
status distinct. Do not close a semantic feature using a descriptor, raw accessor
or UNSUPPORTED stub. [D01](backlog.md) closes documentary review only; the
remaining entries and Q01-Q21 preserve the actual feature/evidence obligations.
