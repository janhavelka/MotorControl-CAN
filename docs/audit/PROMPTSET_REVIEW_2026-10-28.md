# Prompt correction review, 28 October 2026

This maintenance review corrects the implementation instructions and evidence
notes. The checkout remains planning-only: no CAN library, firmware or physical
qualification was implemented. All 50 implementation stages remain Pending.
Work stays on `audit/can-promptset-2026-10-24`, starting from `b93e609`.

## Source review

The [official download index](https://www.xlichuan.com/download/23.html) and
retained model-specific manuals were rechecked. Fresh downloads of all five
PDFs and the EDS were byte-identical to the retained originals. Product/index
HTML changed; that is not evidence of changed firmware. The
[download comparison](LICHUAN_SOURCE_RECHECK_2026-10-28.json) records URLs,
HTTP results, byte counts and SHA256 values. No original artifact was replaced.

Three independent read-only reviewers covered motion/state semantics, EDS and
I/O facts, and execution dependencies/current sibling conventions. Original
rendered tables were used for the output-label finding; pinned source code was
read for PDO reception and direction eligibility. Existing EDS accounting was
confirmed: 99 indexes, 202 scalar entries, 23 records, 34 RO/166 RW/2 WO entries
and 90 PDO-mappable entries. No justified new numeric correction was found in
the reviewed motion widths, object addresses or defaults.

## Corrections

| Finding | Evidence and consequence | Applied correction |
| --- | --- | --- |
| Evidence harness depended on work needing its evidence | Prompt 23 required completed 18/20/21/22; 29 depended transitively on 23. Remapping, I/O and tuning facts can require firmware acquisition. | Owner 23 now requires 6/14/17/19. It proves ownership using typed reads and confirmed settings, or an explicitly test-only synthetic service sequence. Firmware-gated features retain their obligations and integrate through the owner when resumed. Prompt 45 tests the combined real paths. |
| Strict sequential wording contradicted blocked-work continuation | START_HERE/README required completing every prompt before proceeding, while COMMON allowed independent progress. | Execute numeric order among ready work within the assigned range. Record Partial/Blocked, commit/sync checked chunks, continue independent work and return when evidence arrives. No missing prerequisite is treated as satisfied and no partial prompt becomes Complete. |
| Stop/reset arrows could be read as unconditional write scripts | ZH86 PDF p12 section 3.2.3 and OP86 p10 give `15 -> 271`, `15 -> 11`, `15 -> 128`. ZH86 p13 section 3.3.2 and OP86 p11 use `6,7,15` to start velocity operation. | Rename the motion-table column as documented transitions/examples. Prompts 31/32 forbid an enabling preamble before stop/reset and require an evidenced current-state transition or rejection. Repeated stop, halted/released stop and fault-reset traces explicitly test this. This is not a new assertion about measured target retention. |
| ZH57 output terminal labels contradict each other | ZH57 PDF p8 section 2.2.2 labels `60FE` bits16/17/18 DO0/1/2; PDF p17 section 3.6.2 labels both `60FE:01/:02` DO1/2/3. | Preserve both in object facts and prompts 3/21. Bind terminal identity by model/firmware evidence; do not silently renumber, infer a fourth output or transfer the 57C claim to 86C. |
| SDO access could be conflated with PDO eligibility | EDS WO `6040/6060` are mapped in default RPDO1. Pinned CANopenNode `CO_PDO.c` lines119-131 checks RPDO and TPDO attributes separately. | Prompts 12/17/18 separate SDO access, PDO direction and action permission. Readback of mapping records does not require uploading mapped WO command targets. Preserve the default template in tests without granting SDO read permission. |
| A host PDO generation cannot identify every delayed frame | Pinned CANopenNode `CO_PDO.h` lines48-65 and `CO_PDO.c` lines463-501 contain no wire map generation. Internal flag clearing at lines559-570 does not retire external/remote queues. | Prompts 17/18/24/38/45 distinguish confirmed configuration from qualified samples. Reused routes with old incompatible semantics need an evidenced transition/retirement boundary, distinct verified route or applicable observable marker. Otherwise keep ambiguous samples diagnostic/unqualified. Host-only flush, readback, new generation or arbitrary wait alone is insufficient. |

The PDO regression uses old `[6064:I32,606C:I32]` bytes first arriving after
the route has changed to `[606C:I32,6064:I32]`, with the same CAN ID and eight-byte
length. It must not attach an artificial old-generation label that gives the
test information absent on the wire. Already queued local frames are tested
separately. This is a protocol counterexample, not a measured CL86-C queue
behavior. Unrelated stable routes continue normally; no new generic engine is
required. Detailed evidence is in the
[public implementation review](LICHUAN_PUBLIC_IMPLEMENTATIONS.md#pdo-sample-provenance-after-a-route-changes).

All changes preserve the single execution branch, per-prompt commit/sync,
framework independence, exact common RS vocabulary, continuous Doxygen and
plain engineering language. No new supported product, action or default is
inferred from these corrections. Unresolved source conflicts remain precise
feature gates, with acquisition in prompt 29 and qualification in 47-48.

## Live ecosystem audit

| Repository | Inspected HEAD | Working tree | Relevant delta |
| --- | --- | --- | --- |
| MotorControl-RS | `bd831c859daa9c8c07b8d27a2dd879bb4e750511` | Clean | Unchanged; action/velocity and owner contracts reread |
| ADS1115 | `235af3e8c722661a89a44766a4ccd903a654f6fe` | Clean | Unchanged; typed reads, comparator side effects and publication inspected |
| INA228 | `b28c5ea69bcb4d26b351c52c24d3c6fae9fd282b` | Clean | Unchanged; diagnostic read/clear, validity and owner behavior inspected |
| SHT3x-main | `f65f15573e3c9b1ef04cce72ad08f307d021e563` | Clean | Revision unchanged; no unrelated implementation audit repeated |
| FieldCore-node | `5ad9292dc8481cf08be9a61f662495a6474d1da0` | Clean | Advanced from `a002aba`; diagnostics hooks inspected |

FieldCore's `include/TunnelMonitor/i2c/I2cDiagnostics.h` and
`src/i2c/I2cDiagnostics.cpp` add native hooks at actual admission/publication
boundaries. They copy evidence without consuming retained results or reentering
runtime; admission observation precedes possible synchronous rejection
publication. This reinforces the existing diagnostic-copy contract.
`Rs485MotorModule.cpp`, `Rs485Task.cpp`, `SystemHealthProjection.h` and the
ADS1115/INA228 modules are unchanged from the earlier baseline.

Current RS owner behavior still reserves retained completion on copied
admission and offers non-consuming `result()` until `release()` or destruction.
Velocity duration begins at successful preparation, including queue/setup time.
FieldCore preserves identity/address/generation checks and separate execution
effects. Sensor read side effects reinforce per-object read classification;
they do not establish Lichuan side effects. No sibling files were changed and
no unchanged sibling test suite was rerun.

## Workload and validation

The 50 prompt bodies contain **561-779 whitespace-separated words, median
650**. [Current measurements](PROMPT_SIZING.json) retain the earlier split and
enrichment checkpoints. Detailed source arguments stay in focused linked
references; no new numbered stage or unbounded assignment is needed. Word
counts do not estimate implementation effort or replace scope review.

The planning checker now verifies README prerequisites against SEQUENCE and
rejects any transitive dependency from evidence prompt 29 onto firmware-gated
18/20/21/22. Its regression suite adds both dependency-deadlock and index-drift
cases to the existing numbering, scope, closure and EDS-intake checks.

The merged corrections received independent motion, object/source and execution
reviews. All 10 planning regression tests pass. Prompt/reference integrity and
working-tree whitespace checks pass; the final staged and committed artifact
checks use the commands below. Source citations distinguish configured-ID
routing from receive validity/length checks rather than attributing both to
the receive callback.

Required checks for this change are `python scripts/check_promptset_test.py`,
`python scripts/check_promptset.py --check-git-ref INDEX` before commit, the
same check against `HEAD` after commit, and `git diff --check`. The retained
[validation result](PLANNING_VALIDATION.json) records final counts and errors.
These checks validate planning structure, links and all 123 original/derived
reference and baseline artifacts; they do not build a CAN driver or qualify
hardware. No CAN traffic or motor activity was performed.
