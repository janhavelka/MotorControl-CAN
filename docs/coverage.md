# Requirement coverage during source intake

The [requirements matrix](requirements_matrix.md) owns the full acceptance criteria.
This ledger tracks the actual milestone evidence and retains every requirement ID
and its current prompt owners. The [source map](source_map.md) and
[prompt 1 report](reports/prompt_1.md) and [prompt 2 report](reports/prompt_2.md)
substantiate the audit and intake entries.

**Pending** means no CAN implementation, corresponding CAN host test, firmware
build or hardware qualification has been delivered for that requirement. No CAN
driver/API exists for any row yet. Audit/process evidence below does not change
that support claim. Reference/data verification is not driver testing.

| Requirement | Prompt owners | Evidence now / remaining scope |
| --- | --- | --- |
| R01 | 1-2; recurring all | Current RS source/CLI/package audit and prompt 2 dictionary/freshness delta recorded; recurring audits remain. |
| R02 | All | All requested I2C/codec seams inspected; recurring audit remains. |
| R03 | 5-10, 40-42, 49-50 | Pending |
| R04 | 5-12, 15, 17, 26-28, 45-46, 49-50 | Pending |
| R05 | 5-6, 13-18, 23-25, 45-46 | Pending |
| R06 | 2-4, 19-22, 29-36, 47-50 | Pending |
| R07 | 1-4 | Original bytes verified; prompt 2 accounts for every EDS section and 53 manual semantic items with source references. Semantic review and profile applicability remain 3-4. |
| R08 | 2-4, 19-22, 29-36, 47-48 | Twenty-four conflicting claims are retained side by side; no firmware contradiction resolved. |
| R09 | 9-10, 21-22, 32-36 | Pending |
| R10 | 9-10, 13-25, 30-32 | Pending |
| R11 | 11-18, 23-25 | Pending |
| R12 | 13-14, 21-25, 30-39, 45-46 | Pending |
| R13 | 2-4, 19-22, 30-39, 49-50 | Source metadata and separate coverage fields exist; typed native policy and feature implementation pending. |
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
| R26 | 1-10, 45-46, 49-50 | RS installed profile distinguished from design contrasts; CAN contrasts4/6 pending. |
| R27 | 5-6, 9-18, 23-25, 45-46 | Pending |
| R28 | 5-6, 11-12, 15, 17, 23-28, 45-46 | Pending |
| R29 | 2-4, 9-10, 21-22, 30-36, 45-46 | Pending |
| R30 | 5-6, 29, 45-50 | Pending |
| R31 | All; 1-2, 9-10, 23-25, 40-42 | Literal conventions mapped to declarations, implementations, callers and tests; CAN code pending. |
| R32 | All | Prompt 1 and 2 used bounded independent source reviews; lead owns schema, contracts, Git and integrated checks. |
| R33 | All; tooling 7-8, final 49-50 | Audit documentation accompanies this milestone; public C++/Doxygen implementation not yet applicable. |
| R34 | All | Navigation and direct responsibility ownership recorded; no new runtime abstraction. |
| R35 | 1-50 | Current 1-50 owners retained; prompts 1-2 complete, semantic review and later deliverables pending. |

Later stages replace Pending with links to real APIs, tests and evidence, keeping
documented, implemented, host-tested, firmware-built and hardware-qualified
status distinct. Do not close a semantic feature using a descriptor, raw accessor
or UNSUPPORTED stub. [D01](backlog.md) now tracks semantic review of the
completed source intake, not missing accounting.
