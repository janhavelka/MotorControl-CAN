# Independent promptset audit: current disposition

Reviewed on 15 November 2026 against `main` at
`efba7186087f57deb0facb771f524633d0778c3f`, plus the corrections delivered with
this report. The initial tree/index were clean; `origin/main` matched after
fetch. Work remains on `main` under the user's version0 override.

The substantial external audit is
[PROMPTSET_AUDIT_34_50_2026-11-02.md](../prompts/PROMPTSET_AUDIT_34_50_2026-11-02.md).
Its **125 distinct numbered findings** were checked individually against current
declarations, implementations, callers/tests and relevant original evidence.
There is no D-18 in that source; none was invented to fill the numbering gap.
Its findings and historical verification record are preserved, not rewritten as
though today's implementation existed at its baseline.

Also located: [the October 23 final planning review](FINAL_REVIEW.md),
[October 24 review](PROMPTSET_REVIEW_2026-10-24.md),
[October 28 correction review](PROMPTSET_REVIEW_2026-10-28.md), and later numbered
implementation audit reports. These provide history and repair references; this
125-row review addresses the November 2 independent auditor's complete finding
inventory, not a claim to have re-executed every historical campaign.

| Finding group | Count | Individual dispositions |
| --- | --- | --- |
| C, owner/adapter | 20 | [Owner review](2026-11-15_auditor_disposition_owner.md), including DEC-1 through DEC-8 |
| A, core/API | 20 | [Core review](2026-11-15_auditor_disposition_core.md) |
| G, dependency/contracts | 14 | Table below |
| D, device facts | 28 | [Device/source review](2026-11-15_auditor_disposition_device.md) |
| R, RS parity | 14 | Same device/source review |
| S, siblings | 12 | Same device/source review |
| X, drive contrasts | 12 | Same device/source review |
| N, documentation | 5 | Table below |

“Resolved” closes the reported defect, not physical qualification. “Partial”
means an actual requirement remains. A corrected prompt is not implemented
code. The detailed reviewer tables describe the entry baseline and identify
this review's small corrections; the delivered changes below record which were
applied. Remaining scope stays with its existing owner rather than duplicated
in a new generic audit prompt.

## Dependency and shared-contract findings

| ID | Current verdict | Evidence and disposition |
| --- | --- | --- |
| G-01 | Resolved | `SEQUENCE.json` and prompt 38 depend on 24/37, not 18. The real `check_promptset.py:ancestors` guard prevents 18 from becoming a candidate prerequisite. Prompts 49/50 explicitly retain Q17 without claiming remapping complete. Existing `PdoConfiguration` code remains a narrow subset; general remapping is still prompt 18. |
| G-02 | Resolved contract; stale navigation corrected here | COMMON defines usable consumed artifacts and owned completion separately from inherited gates; PROGRESS has an inherited-gates column. Sequence 43 depends on 40; 44 depends on 42/43. Current 23 is Complete with Q20 inherited, while 29 retains conditional acquisition scope. Corrected the stale “23 remains Partial” owner guide, 50/71/72 range disagreement, copyable branch instructions and current resume point. Do not re-run completed prompts merely because README links prompt 1. |
| G-03 | Resolved instructions; evidence obligations remain | Prompts 31-36 explicitly route early evidence through 29 or manufacturer clarification. Prompt 47 depends on 29/44/45, not 46, and separates fixture readiness from software. The current homing/mode/scale gates remain; no host simulation is promoted to firmware evidence. |
| G-04 | Resolved original missing-platform defect | `EspProbePlatform`, `EspProbeMain`, `FirmwareBoard`, `IdfTwaiDriver` and `examples/s2_bench` now implement the S2 path. `IdfTwaiDriver::refreshReceiveBoundary` bounds reception using supplied driver/FIFO evidence; it does not call processing time physical arrival. Pinned S2/S3 compile/stack gates and retained bench records exist. An S2 result is not S3/P4 qualification; prompt 67 owns further MCU qualification. |
| G-05 | Resolved | Shared `ActionOperation.h`, profile `Actions.h` and `Actions.cpp` implement enable/release capability handling and stops in the existing lifecycle. No duplicate action engine or header is needed. Release's evidence-gated scope remains explicit. |
| G-06 | Resolved bounded software, Q18 still open | `Reads.cpp:prepareProbe` prepares the typed PROBE plan; `src/Discovery.cpp:getDiscoveryCapabilities` names `1000:00`, four bytes, without claiming non-changing effects. `DiscoveryScan::start/passive/classify/finish` uses existing configured slots, retains raw findings and counts unresolved/free capacity. `discovery_scan_test.cpp:permanentSlotsAndSilence` covers absent responders. It does not promise a full 1..127 sweep through two permanently reusable slots. Prompt 55 owns conditional repair policy. |
| G-07 | Resolved ownership defect | The canonical ledger has 89 read rows owned by 38, six by 36, two by 32, and one each by 31/33/35/39. `607C` belongs to 36. `Communication.cpp` and `communication_test.cpp:allRowsThroughOwner` implement the 89-row acquisition; assigned motion/persistence reads have their own APIs/gates. Ownership accounting is distinct from completion of every gated read or write. |
| G-08 | Resolved assignment/console location | `api_cli_mapping.md` names owners for host/config/wiring/memory and device/motion routes. Actual `ProbeConsole` lives in `examples/common`; `native_probe` uses the same implementation. `CommandInventory.cpp` and `ProbeConsole` distinguish implemented dispatch from gates. Prompts 40-42 and their tests exist; no unnamed console branch or startup motion defaults are required. |
| G-09 | Resolved acceptance wording | Prompts 45/49/50 distinguish implemented conditional paths, inherited gates and a scoped software candidate. Prompt 36 explicitly keeps unfinished earlier scope Partial. PROGRESS still marks 18/19/30-33/35-36/38-39 Partial. This review does not relabel them Complete to unlock consumers. |
| G-10 | Resolved by prompt46 | Prompt 36's `prompt_36_coverage.md` reconciles its direct rows by hand. Prompt 46 owns `scripts/check_lichuan_operations.py`; 49/50 run it. [Prompt46](../reports/prompt_46.md) now delivers the active checker, including negative tests and retained pending obligations. `607C` is in the homing operation/read assignment. Do not claim mechanical operation coverage exists merely because object generation passes. |
| G-11 | Resolved package description | `CMakeLists.txt` builds common code and Lichuan together; existing options select examples and the optional stack. Core-only means a consumer using only common headers, not a separate profile build. Prompts 33-39 name source/header inventories and 46/49 use the correct consumer distinction. The Discovery umbrella omission found under A-16 is fixed separately in this review. |
| G-12 | Resolved | `docs/runners.md`, `docs/commissioning.md`, `reference/lichuan_generated.md`, `api_cli_mapping.md` and `coverage.md` exist and have distinct responsibilities. The architecture plan names them. No duplicate `lichuan_api_cli_coverage.md` deliverable is needed. |
| G-13 | Resolved | CONVENTIONS section 3 and prompts 33-36 state the operation processing cutoff. `Reads.cpp:nextRead/advanceRead` and `Actions.cpp` preserve `now >= deadlineUs` expiry; read/action boundary tests distinguish it from earlier service receive-time settlement. A timely service response does not revive an expired operation. |
| G-14 | Resolved software contrasts by prompt46 | MULTIDRIVE row 45 covers integrated implemented paths; row 46 and prompt 46 explicitly own the no-device-ack setter and trajectory-done-with-nonzero-velocity contrast. No such production capability is claimed from prompt 45's CANopen tests. [Prompt46](../reports/prompt_46.md) closes X-01/X-03/X-04/X-10 through production-linked cases; surveyed products remain unsupported fixtures. |

## Documentation findings

| ID | Current verdict | Evidence and disposition |
| --- | --- | --- |
| N-01 | Partial at entry; corrected here | AGENTS/CONVENTIONS already distinguish implementation and gates, but `architecture.md` still claimed no installed driver and future CLI/startup; `reference/drive_contrasts.md` still claimed no driver or matched specimen. Updated those introductions to current implemented scope with links to coverage and exact qualification limits. |
| N-02 | Partial at entry; corrected here | PROGRESS had regained a reverse-chronological preamble and its “Next ready” still said 45 after 45 completed. Replaced the preamble/next-work section with current state, retained the table/reports, and shortened the three oversized history rows without removing their obligations. README/COMMON/START_HERE now agree on the active 72-file sequence and main-branch override. The existing prompt checker rejects the specified mojibake sequences; no byte conversion of originals was performed. |
| N-03 | Resolved | `canopen_services.md` describes actual `SdoClient::receive` network dispatch, consumed/unmatched semantics, observer history and bus-off/boot invalidation. It no longer claims event decoding awaits prompt 15. Current `CanBusOwner::receive` and `SdoClient::receive` agree with that description. |
| N-04 | Resolved original NMT claim; residual state wording corrected | Owner SDO/NMT paths share transport evidence (`TransportTxId`, `admitNmt`, `transmitNmt`); service abort and healthy streak tests exist. Corrected the obsolete prompt-23 Partial claim. Future SYNC/RPDO traffic and concurrent application controls remain separately assigned, not implied by the sentence about one receiver. |
| N-05 | Resolved | `architecture_plan.md` now names common console code, native entry and coverage ownership; `architecture.md` correctly describes the root common-plus-Lichuan target. Corrected its remaining stale implementation introduction in this pass. A planned tree is explicitly not a delivered-file inventory. |

## Changes applied and larger work retained

- **A-16:** added `Discovery.h` to the public umbrella. `core_test.cpp` now includes
  only that library umbrella and exercises inventory, capabilities and unchanged
  output on refusal. The negative compile without the include fails, demonstrating
  that another direct include cannot mask this omission.
- **D-09:** the original OP86 PDF p5 note 4 describes power-up lock; p9's 6/7/15
  procedure describes enabling. These do not by themselves contradict one another.
  The lead viewed both rendered pages independently. Updated 39/47/48 and the bench
  source row; family-wide applicability and the observed two-specimen scope stay
  separate. No controlword, state mask or capability flag changed.
- **X-11:** C1b now cites the N5 command/encoder sources, not EPOS4 increments-only
  tables, for distinct representations. Its numeric vector remains illustrative.
- **C-14:** the SDK guide now accurately says RTR DLC 9..15 is normalized to eight
  too, with no payload; actual unusual-DLC hardware behavior remains unmeasured.
- **X-01/X-03/X-04/X-10:** tightened prompt 46 so a prototype-only label cannot
  close its required production cases. It already owns the needed small production
  seams, width/ramp/feedback cases and coverage checker. No second generic engine,
  placeholder profile or duplicate fixture family is requested.
- **A-17:** extended prompt 49 with exact generator/initializer/consumer checks
  and migration requirements for removing `ObjectDescriptor.ownerPrompt`. This
  changes public layout, so it stays a reviewed package/API task rather than an
  unannounced field deletion during this maintenance pass.
- **C-05/DEC-3 and C-20/DEC-4:** extended prompt 55's existing work with actual
  `CanBusOwner::admitDrainMarker` ownership, duplicate/server/bridge premises,
  marker-value validation before reuse, finite no-listener probing, presence versus
  session readiness and no historical-result rewrite. Corrected its stale claim
  that FieldCore motors already retry. Application-accepted premises remain labeled
  assumptions, not CiA guarantees. The configured recovery/stop/fault policy is
  still requested; it is not implemented by these edits.

The original DEC summaries are historical. In particular, “one transmission”
means one application submission under the selected hardware-retry mode;
neither FIFO ordering nor a marker value proves absence of every delayed duplicate.
Current service contracts and the revised prompt 55 carry the complete boundary.
No vendor source bytes, EDS, generated device constants or firmware qualification
flags were changed. No hardware was opened, flashed or exercised.

## Review and verification

Three bounded parallel reviewers used `gpt-6-astra` high for owner/correlation
and device/source evidence, and `gpt-6.1-sol` high for core/API findings. The lead
checked integrated diffs, dependency/documentation findings, the core regression,
original rendered D-09 pages and the proposed recovery contract. A second owner
review challenged prompt 55's correction. Workers made no Git mutations.

Source refresh and per-finding locations are in the linked tables. RS advanced
since prompt 45 to clean `19a1a4b48cb2665d0373b750d3d47730344f22fd`; FieldCore
remained clean at `c40ea8341edf8d665460b902cafefbe857a7f73f`. The retained manuals
and EDS were reopened; remote-only EPOS4 rel13740 was fetched into memory and
matched its recorded SHA-256. No downloaded program was executed.

At the final sibling refresh RS had advanced again to clean
`2f41e27f2378c644b1358ed9a6af63c35f817058`. The two intervening commits changed
only standalone audit/report Markdown; the inspected implementation, caller and
runner seams were unchanged. FieldCore remained at the revision above.

| Check | Result |
| --- | --- |
| Finding accounting | All 125 source IDs occur exactly once across these four disposition files; no missing or extra ID |
| Focused native review | 18/18 CTests passed; removing the new umbrella include from a temporary negative compile fails as expected |
| `python scripts/verify.py --mode full --compiler gcc --build-dir .build/auditor-review-full` | Passed: build, registration, 142/142 CTests, generated freshness, package consumers and strict Doxygen |
| Package boundaries | Exported source, independent headers, C++11/C++17/no-RTTI, relocated install, isolated native applications, missing-dependency refusal and IDF CMake boundary passed |
| `python scripts/check_promptset.py --check-git-ref INDEX` | Passed: 72 prompts, 35 requirements, 140 original artifact hashes, 99 indexes/202 entries and 24 ledger conflicts |
| `git diff --check` and staged equivalent | Passed |
| Physical/firmware qualification | Not run; this change adds no device-facing operation or source constant |

The full local logs are retained at
`.build/auditor-review-full/20261115T085631798095Z-47904-0w16r0o9/`;
the focused build is `.build/auditor-review-core/`. Later report-only wording
does not change the tested C++ tree. The only code change is the public umbrella
include and its regression; this is not a runtime/API redesign. No manual remote
CI dispatch is needed under the impact-based policy. Automatic push checks still
apply; reports of prior CI and bench passes verify their original revisions only.
The lead commits these owned paths and synchronizes `main` normally; the handoff
identifies the actual commit and final upstream state.
