# Prompt 3 reviewed Lichuan semantics and family gates

Date: 2026-10-28. Status: Complete for the assigned documentary review.
Scope is documentary review, not drive implementation or firmware qualification.
Prerequisite [prompt 2](prompt_2.md) supplied a complete intake. Execution
continued on `promptset/motorcontrol-can` from `f25aa0b`, with a clean worktree
and fresh upstream comparison 0 ahead / 0 behind. No new branch was created.

## Delivered review

Schema 2 of [the canonical dictionary](../reference/lichuan_objects.json)
reviews every one of its 99 index headers, 202 typed entries, 53 manual
semantic functions, 24 conflicting claim sets and the `1018` count anomaly.
The [schema guide](../reference/lichuan_objects.md) explains policy fields;
the [decision guide](../reference/lichuan_review.md) provides readable status,
motion, settings, I/O, tuning, network/PDO, four-model and qualification tables.
All original EDS fields, omissions, expressions and competing conflict claims
are retained. Original manuals, EDS, manifests and historical audits are unchanged.

Each entry now records reviewed access, units/basis, effect, prerequisites,
edge behavior, type/value policy, read-effect uncertainty, write policy and
feature gates, with exact EDS and applicable PDF references. Model/firmware
scope remains separate from a source's generic identity metadata. Raw identity
acquisition may precede exact-model selection; an unknown physical scale or
decoded status field does not forbid retaining raw observations.

| Documentary accounting | Result |
| --- | --- |
| Original structure | 76 scalar roots, 23 records, 126 typed children; no arrays; 27 index-gap ranges |
| Scalar wire types | 42 U8, 91 U16, 61 U32, 1 I16, 7 I32 |
| Raw access / PDO flags | 34 RO, 166 RW, 2 WO; 112 mapping-zero, 90 mapping-one |
| Missing defaults | 65 remain absent; 11 node-ID expressions remain literal metadata |
| Entry dispositions | 14 supported documentary facts, 8 conservative read-only observations, 176 requiring feature qualification, 4 excluded reserved entries |
| Reviewed write policies | 92 not writable, 106 qualification required, 4 reserved; zero unconditional write approvals |
| Manual functions | All 53 reviewed with retained later implementation obligations; no pending documentary row |
| Conflict inventory | All 24 original claim sets retained; reviewed conservative dispositions do not pretend to select a firmware winner |
| Qualification | Q01-Q21 are open, scoped to their affected features/models and exact required observations |
| Implementation evidence | Every entry/function remains implemented=false, host_tested=false, hardware_qualified=false; no firmware build exists |

`supported_evidence` means documentary representation, not hardware support.
Conservative entry reads do not close the corresponding future setter/action.
No unsupported stub or raw accessor has been counted as a completed feature.

## Source review and corrections

The [current sibling audit](prompt_3_source_review.md) records all seven exact
HEADs, clean before/after state, declarations, implementations, actual callers,
focused tests and workflow/runner seams. MotorControl-RS, ADS1115, INA228,
MB85RC, MCP45HVX1, RV3032-C7 and FieldCore-node were inspected read-only.
Revisions are unchanged from the prerequisite records; unchanged suites were
not rerun. The relevant findings remain native access versus authorization,
typed observation versus active/persistent configuration, consuming reads,
action effects, and explicit commissioned provenance. CAN retains the selected
RS naming and concrete contexts; no I2C retry, replay, framework type or
FieldCore configuration policy was imported.

The independent [motion review](prompt_3_motion_review.md) and
[network/I/O/family review](prompt_3_network_review.md) record exact original
pages and EDS sections. Both reviewed original files, not only research notes.
Rendered inspection included EN86/ZH86/ZH57 p12 and OP86/OP57 p10 status
tables, ZH57 p8/p17 output labels, and EN86/ZH86 p19 tuning/output tables.
The lead separately rendered and visually compared EN86/ZH86 p12 at final review.
OP86/OP57 produced recoverable PDF structure warnings but readable complete
renders. These are documentary observations, not physical measurements.

Corrections and selected dispositions:

- The intake fault-reset claim said a return-to-zero **edge** was described.
  EN86/ZH86 p15 and OP86 p13 say assert bit 7 and manually clear it; they do
  not specify an edge trigger or pulse width. The ledger now says so. `2302`
  still has its explicitly described rising edge and manual return.
- Mode selection was assigned to reads owner 19. `6061` remains observation
  under 19; native mode setting belongs to 20, with transition behavior owned
  by 30/33/35/36. `2301` alarm clear was incorrectly grouped under persistence;
  it belongs to 32. The 57C native semantics now have actual API owners rather
  than contrast-review prompt 4 as their final implementation owner.
- Expanded DI/filter/current/tuning related-entry groups beyond their endpoint
  addresses and supplied exact PDF references for each affected entry.
  Corrected `60FE`'s ZH86 dictionary reference to p17 and filter references to
  the pages containing each actual row. Source originals were not edited.
- Preserve three incompatible 86C status dialects and the separate 57C layout.
  Never merge masks. Consistent bits 8/15 still need fresh operation evidence.
- `6060=0` is documented as undefined, not an operating mode. Positive mode
  and homing codes have identical U8/I8 bytes; high-bit/negative meanings remain
  unknown. `6061` stays read-only despite the translated set instruction.
- `607A` is I32 from EDS and the position table despite `I3C` in one dictionary.
  `6099:02` is creep speed from rpm/prose, despite the word time. These bounded
  interpretations do not assert a firmware resolution of the original wording.
- `607D` has no documented operating limits or physical units; the range of
  another target/offset object must not be copied to it. `607C` has its own
  documented range but unknown native scale. Command, feedback and encoder
  scales remain independent. Native ramp fields are milliseconds, not rates.
- Keep output-address/function conflicts, 57C terminal labels, reserved/filter
  fields and unknown tuning effects visible. `2502` means peak current; no
  millisecond unit is assigned to `250C`. EDS-only tuning names are not proof
  of physical effect. The `60FE` ON condition does not define all mask cases.
- Raw SDO access does not determine PDO direction or approve action mapping.
  Four RPDO/four TPDO and their default layouts are source-backed metadata;
  delayed same-route traffic still needs a qualified retirement boundary.
- All explicitly typed EDS scalars fit one to four bytes; server transfer form
  remains an independent SDO fact. Node-zero NMT broadcast is distinct from
  addressed services. Pinned protocol source supplies wire rules, not Lichuan
  behavior. The guide identifies exact revisions and source locations.
- Four model records distinguish CL/OL 86C/57C, including the OL manuals'
  explicit lack of an encoder. Unbound EDS LSS=0 is negative metadata evidence;
  absent objects are not equally strong unsupported-capability evidence.
  Generic identity cannot auto-select every model or establish shared silicon.

The final review specifically challenged copied numeric bounds, unproven
units/effects, count-row versus child semantics, missing per-entry citations,
discovery prerequisite circularity and cross-model gates. The corrected ledger
retains no inferred `607D` range, universal status mask or unconditional write.

## Implementation ownership and remaining evidence

| Later owner | Bounded obligation |
| --- | --- |
| 12-18 | Catalogue/policy generation; SDO and reconciliation; passive events, explicit NMT, PDO observation/remapping. Source eligibility is not live activation. |
| 19 | Typed identity/configuration/state observations, raw unknowns and interpretation validity. |
| 20 | Driver/mode/ramp/subdivision/direction/soft-limit settings with configuration provenance. |
| 21 | DI/output settings, output actions, filters and model-specific terminal evidence. |
| 22 | All 33 tuning/current/loop entries, with each unknown setter/conversion still owed. |
| 30/31/32 | Enable/release; stops; fault reset, position clear and reference effects. Never enable as a stop/reset preamble. |
| 33/34/35/36 | Position; angle integration with independently established scale; finite velocity; homing/reference qualification. Angle integration is a host responsibility, not an invented vendor object. |
| 37/38/39 | Passive discovery and selected binding; explicit communication commissioning; explicit save/factory-reset durability. |
| 29/47/48 | Conditional acquisition and actual fixture/physical qualification; not completed by this review. |

Every gate names the smallest affected feature and the required clarification
or matched-firmware observation. Q06 applies to 86C outputs and Q07 to 57C;
status and electrical checks use only the selected model's evidence. Missing
firmware does not block unrelated host work. The existing CAN bench remains
unspecified: no board, transceiver, drive, motor, firmware or authorization was
inferred from RS. No save, reset, motion, CAN/serial access or vendor tool was
used as an evidence-gathering action.

## Validator and maintainability review

Extended the existing ledger checker rather than adding another policy engine
or generator. Schema 2 retains all raw EDS checks and independently protects
manual obligations, exact later owners, required feature gates, conservative
access and unestablished read effects. An inventory digest pins original
conflict alternatives/references to the prerequisite revision; a source-backed
correction must change the guard deliberately. Malformed nested rows return
errors rather than exceptions. Tests include deleted obligations, lost gates,
changed owners, invented non-consuming reads, write/mapping approval, false
family support and removal of competing source claims.

The ledger remains the canonical factual source. The validator's small owner
and gate tables guard assigned scope, not a second set of object values. The
guide explains decisions and links source bytes. No runtime module, C++ API,
state machine, dependency, storage bound or firmware build was added. Doxygen
is not applicable to this documentary stage; future public APIs retain that
requirement. README/index, CHANGELOG, coverage, source map, backlog, roadmap
and execution progress are updated with the documentary/implementation split.

## Delegation

| Task | Model/effort | Ownership and outcome |
| --- | --- | --- |
| Original motion/status/trigger/stop/persistence review; independent ledger challenge | gpt-6-astra / high | Read-only originals plus its report; found edge wording, units/range, citation and count-semantics corrections. |
| Original EDS/network/I/O/tuning/family review; independent challenge | gpt-6-astra / high | Its report and durable decision guide; found source-reference, peak-current, model-gate and identity-read issues. |
| Current RS/I2C/FieldCore review; policy-validator challenge and fixes | gpt-6.1-sol / high | Its report, then exclusive checker/test files; found erased obligations/gates and malformed-input failures and added regressions. |
| Integration | Lead | Canonical ledger/schema decisions, integrated review/checks, handoff and all Git mutations. |

## Verification and delivery

| Check | Integrated result |
| --- | --- |
| `python -B scripts/check_lichuan_ledger.py` | PASS: 99 indexes, 202 entries, 53 manual items; complete reviewed policy and original accounting |
| `python -B scripts/check_lichuan_ledger_test.py` | PASS: 55 regression tests, including 75 malformed-input subcases |
| `python -B scripts/check_promptset.py --check-git-ref HEAD` | PASS: 50 prompts, 35 requirements, all 123 retained artifact hashes and ledger accounting |
| `python -B scripts/check_promptset_test.py` | PASS: 11 planning-validator regressions |
| Direct comparison with prerequisite `f25aa0b` | PASS: all original source/scopes/EDS metadata, object and entry raw data, type/access/PDO/limit/default fields, intake accounting and conflict alternatives unchanged |
| `git diff --check` and final staged check | PASS: no whitespace errors; no original artifact mutations |

The same integrity check is applied to INDEX before committing and HEAD after
committing. Delivery stays on `promptset/motorcontrol-can` to its configured
`origin/promptset/motorcontrol-can` upstream. The commit containing this report
is the prompt-3 delivery; its exact ID and final fresh-fetch ahead/behind result
are reported in the handoff. No sibling was fetched, changed or committed.

No remote CI dispatch is required for this documentary milestone; the target
still has no build or CI workflow. Ledger/planning regressions are evidence
tooling tests, not CAN driver host tests or hardware qualification.

Next assignment is [prompt 4](../prompts/4_drive_contrasts.md), retaining the
finished Lichuan review and its open feature-specific obligations. No later
prompt was executed as part of this assignment.
