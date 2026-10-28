# Requirement coverage after documentary review and drive contrasts

The [requirements matrix](requirements_matrix.md) owns the full acceptance criteria.
This ledger tracks the actual milestone evidence and retains every requirement ID
and its current prompt owners. The [source map](source_map.md) and
[prompt 1 report](reports/prompt_1.md) and [prompt 2 report](reports/prompt_2.md)
substantiate the audit and intake entries. [Prompt 3](reports/prompt_3.md) and
the [reviewed dictionary guide](reference/lichuan_review.md) substantiate the
completed documentary policy and remaining feature-specific gates. The
[prompt 4 contrast ledger](reference/drive_contrasts.md) records test-only
N5, EPOS4 and proprietary-frame cases; it adds no installed support.

**Pending** means no CAN implementation, corresponding CAN host test, firmware
build or hardware qualification has been delivered for that requirement. No CAN
driver/API exists for any row yet. Audit/process evidence below does not change
that support claim. Reference/data verification is not driver testing.

| Requirement | Prompt owners | Evidence now / remaining scope |
| --- | --- | --- |
| R01 | 1-2; recurring all | Current source audit plus prompt 3 native access/effect/configuration provenance, actual callers and focused tests recorded; recurring audits remain. |
| R02 | All | All requested I2C/codec seams inspected; recurring audit remains. |
| R03 | 5-10, 40-42, 49-50 | Pending |
| R04 | 5-12, 15, 17, 26-28, 45-46, 49-50 | Pending |
| R05 | 5-6, 13-18, 23-25, 45-46 | Pending |
| R06 | 2-4, 19-22, 29-36, 47-50 | Pending |
| R07 | 1-4 | Original bytes verified; every EDS entry and all 53 manual functions reviewed with exact sources. Four Lichuan model scopes and N5/EPOS4 documentary contrasts complete. |
| R08 | 2-4, 19-22, 29-36, 47-48 | All 24 conflicting claim sets retain original alternatives and reviewed dispositions; 21 bounded feature gates remain open. No firmware winner invented. |
| R09 | 9-10, 21-22, 32-36 | Pending |
| R10 | 9-10, 13-25, 30-32 | Pending |
| R11 | 11-18, 23-25 | Pending |
| R12 | 13-14, 21-25, 30-39, 45-46 | Pending |
| R13 | 2-4, 19-22, 30-39, 49-50 | Reviewed documentary access/effect/value policy and exact native owners exist. Generation, typed features and their tests remain pending; restricted raw reads do not complete setters/actions. |
| R14 | 21-25, 29-36, 47-48 | Pending |
| R15 | 21-22, 29, 32-36, 47-48 | Pending |
| R16 | 37-39 | Pending |
| R17 | 26-28, 40-42, 49-50 | Pending |
| R18 | 40-44 | Pending |
| R19 | 29, 43-44, 47-48 | Pending |
| R20 | 7-28, 30-46, 49-50 | Pending |
| R21 | 1-2, 5-6, 23-25, 45-46 | Current FieldCore contract/owner/consumer inspected read-only; CAN integration proof pending. |
| R22 | All | Execution branch/base recorded; this prompt uses checked commit/sync delivery; recurring duty. |
| R23 | All; final 49-50 | Current audit, roadmap, backlog, bench and support limits documented; release scope pending. |
| R24 | All; architecture 5-6 | Pending |
| R25 | 7-8, 49-50 | Pending |
| R26 | 1-10, 45-46, 49-50 | RS installed profile distinguished from design contrasts; prompt 4 source-backed N5 stepper/BLDC and EPOS4 fixture specification complete. Prompt 6 production-boundary proof remains. |
| R27 | 5-6, 9-18, 23-25, 45-46 | Pending |
| R28 | 5-6, 11-12, 15, 17, 23-28, 45-46 | Pending |
| R29 | 2-4, 9-10, 21-22, 30-36, 45-46 | Lichuan millisecond ramp times contrast with N5 and EPOS4 configured acceleration units; command/feedback/load scale independence documented. Common units/axis implementations and physical conversion evidence pending. |
| R30 | 5-6, 29, 45-50 | Pending |
| R31 | All; 1-2, 9-10, 23-25, 40-42 | Literal conventions mapped to declarations, implementations, callers and tests; CAN code pending. |
| R32 | All | Prompts 1-4 used bounded independent reviews, including separate N5, EPOS4 and proprietary-frame source checks. Lead owns shared decisions and Git. |
| R33 | All; tooling 7-8, final 49-50 | Audit documentation accompanies this milestone; public C++/Doxygen implementation not yet applicable. |
| R34 | All | Navigation and direct responsibility ownership recorded; no new runtime abstraction. |
| R35 | 1-50 | Current owners retained; prompts 1-4 complete. All later feature, integration and qualification obligations remain pending. |

Later stages replace Pending with links to real APIs, tests and evidence, keeping
documented, implemented, host-tested, firmware-built and hardware-qualified
status distinct. Do not close a semantic feature using a descriptor, raw accessor
or UNSUPPORTED stub. [D01](backlog.md) closes documentary review only; the
remaining entries and Q01-Q21 preserve the actual feature/evidence obligations.
