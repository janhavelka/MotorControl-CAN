# Lichuan C reviewed dictionary schema

[lichuan_objects.json](lichuan_objects.json) is the single reviewed dictionary for
the retained Lichuan CANopen stepper material. Schema 2 completes documentary
review of all 202 entries and 53 manual functions. Read the
[decision tables and family gates](lichuan_review.md) alongside it. The review
grants no SDO write, PDO mapping, implemented feature or firmware qualification.
The original EDS and PDFs remain in
[SOURCES.json](SOURCES.json) under their own copyright and are excluded from a
core package.

## Scope and source identity

`sources` retains each original source ID, manifest title, observed cover title,
acquired path, URL, SHA-256, document revision statement, named models and the
evidence for that applicability. The OP86 and OP57 manifest names say
"operating manual" because of their filenames; their printed covers say
"User Manual." OP86's internal headers name 57C despite its 86C cover. PDF
metadata dates and the EDS FileInfo dates are not installed-firmware evidence.

`scopes` separates the generic unbound EDS from the 86C manuals and the 57C
contrast. An EDS object refers to `eds_family_unbound`, which names no exact
model. Manual semantic rows use `manual_86` or `manual_57_contrast`; each PDF
reference retains the source's own model claim. No row has a firmware binding.
The validator checks each scope's manufacturer, model set, family and dialect
against the source intake. Manual rows must cite a PDF for their own model scope.

Raw identity acquisition may precede a selected-model binding. Retain the route,
bytes, source and observation provenance; do not decode generic IDs as an exact
model without evidence. A gate applies to its named field/effect and selected
model, not every operation touching the containing object. In particular, Q07's
57C output-label conflict does not block 86C output qualification, and an unknown
feedback scale does not block retention of a raw feedback value.

## Row schema

| Field | Meaning and validation |
| --- | --- |
| `objects[]` | One row for each EDS index. `key` is canonical `0xXXXX`; `index` is the matching integer; `kind` is the original `ObjectType` 7 scalar, 8 array or 9 record; `object_list` is the original list membership. No array header occurs in this EDS. |
| `raw_fields`, `source_ref` | Exact EDS field spelling/value and named section/line span. The header and every typed child are checked against the original EDS, not just its extraction. |
| `entries[]` | All explicitly typed scalar sections, keyed `0xXXXX:YY`. A scalar root has one entry at `:00`. A record retains its own header and every declared child, including the count entry at `:00`. `SubNumber` counts that entry. |
| `role`, `disposition`, `exclusion_reason` | `count`, `value` or `reserved`; disposition is `supported_evidence`, `conservative_read_only`, `pending_qualification` or `excluded`. These describe documentary scope. The four EDS `2400` Reserved entries remain excluded from typed public access despite RW/PDO flags. |
| `type`, `access_eds`, `pdo_mapping_eds` | Exact parsed EDS type, width, signedness, access and mapping claim. The separate reviewed type/value policy retains conflicting manual interpretations. Source representation is not an operating range. |
| `access_reviewed` | Conservative SDO surface: RO, WO, RW or `null` for excluded entries. It never widens original access. RW/WO still require the separate write policy, binding and feature evidence; they do not authorize transactions. `6061`, monitors, errors and opaque settings retain conservative read-only scope. |
| `pdo_mapping_reviewed` | `false` when the source gives no eligibility or the entry is excluded; `null` means source eligibility remains unqualified. No `true` approval exists in this review. PDO direction/effect policy remains separate from SDO access, including WO commands present in RPDO defaults. |
| `low_limit_eds`, `high_limit_eds`, `default_eds` | Original strings or `null` when absent. `$NODEID+0x...` remains an expression. A missing value is never zero and `null` is never a writable default. |
| `units`, `scaling`, `effect`, `prerequisites`, `edge_behavior` | Reviewed descriptions, including explicit unknowns. Native ramp milliseconds are distinct from physical acceleration; command pulses do not establish feedback, home-offset or limit units. Only `2302` and mode-specific bit-4 actions have the stated source-backed edges; reset-bit manual return does not establish a pulse width. |
| `source_refs`, `confidence`, `conflict_ids`, `owner_prompt` | Exact EDS and applicable per-entry PDF pages; confidence identifies the source claim, not measured behavior. The owner is the actual later implementation prompt, not prompt 3's documentary reviewer. |
| `review` on an entry | `status=complete` means all documentary facets were reviewed. `type_policy` and `value_policy` state usable representation and limits/default treatment. `read_effect=not_established` forbids promotion to non-consuming qualification. `write_policy` is `not_writable`, `qualification_required` or `reserved`; none authorizes a device write. `gate_ids` links the smallest dependent evidence questions. |
| `review.generation_type` | Required declaration disposition: `resolved` for the 194 compact descriptors, `withheld` for the four unresolved complete signed types, or `excluded` for the four reserved entries. The validator checks consistency with reviewed policy. This does not resolve other conflicts, qualify effects or change semantic coverage. |
| `review.pdo_observation` | Ten default-mapped targets have explicit source-backed passive directions, role, representation and source references. `live_qualification_required=true` is mandatory. Unresolved `6060/6061/60FF` representation stays RAW. These facts generate layout validation only; existing `PdoPolicy::UNQUALIFIED`, SDO permissions, action gates and semantic coverage are unchanged. |
| `coverage` | Separate `documented`, `implemented`, `host_tested`, `hardware_qualified` booleans. `documented=true` means a source describes the row, not that the meaning or writable use is approved. The other three are false throughout this intake. |
| `read_owner_prompt` | Required independent owner for each readable, non-reserved entry: 30 rows in 19, 101 communication/PDO rows in 38, 9 driver rows in 20, 23 I/O rows in 21 and 33 tuning rows in 22. Missing/null owners fail validation for readable entries; WO/reserved entries may remain unassigned. Write ownership is unchanged. |
| `read_coverage` | The 30 prompt-19, nine prompt-20, 23 prompt-21 and 33 prompt-22 raw read rows separately record read implementation/testing and hardware-qualified false. Whole-object `coverage` above remains unchanged; no setter/action or read-effect permission follows. The validator checks each implemented group's exact owner scope, shape and implications. Prompt 22 admits only the scalar `2500..2520:00` entries owned by 22 with their exact U16 source type. |
| `manual_only[]` | All 53 functions retain model scope, original pages, related entries and independent coverage. `disposition=reviewed` and `review.decision` close documentary review. The owner, required gates and action obligation remain explicit even where corresponding entries are currently read-only. No raw accessor or unsupported stub completes these future functions. There are no additional CANopen object addresses in the five manuals. |
| `conflicts[]` | All 24 original competing claim sets and references remain intact. `resolution=unresolved`, `decision=null` and `resolution_evidence=null` mean no firmware winner was selected. The separate `review.disposition` records the conservative decision or limited documentary normalization, with consequences and gates. This is not fabricated resolution evidence. |
| `source_anomalies[]` | Non-resolution structural questions with unique IDs and explicit source scope. `A01` retains `1018:00` HighLimit 4/default 3 beside the actual declared `00..03` children; no `1018:04` is created. |
| `manual_index_inventory` | Accounts for zero additional CANopen object addresses, excluded nonobject labels such as `6000h`, and absent generic-index examples. Absence is not a measured unsupported response. |
| `semantic_review` | Completed documentary scope/date, binding rule, 21 open feature gates, four separate model entries and explicitly scoped EDS capability claims. A gate names applicable models, later owners and the required observation/clarification. Family firmware remains unknown; implementation and hardware qualification remain false. |

The ledger preserves the EDS's FileInfo, DeviceInfo, object lists, Comments,
DummyUsage and empty DynamicChannels sections. Losslessness here means all
declared EDS object fields and relevant source metadata are retained verbatim;
the immutable original file remains the full byte-for-byte source.

The 57C statusword bit-7 alarm/ready claim is a separate model contrast to
the 86C bit-3 claim; it does not resolve the 86C status conflict. ZH86's own
page 8 and page 19 disagree on DO addresses and function values, so both
locations remain in conflicts C01/C02.

An explicit unsupported capability is different from missing evidence: the OL
product tables say no encoder, and unbound EDS metadata says `LSS_Supported=0`.
Absent `1016`, `605A`, `6085`, `60F4` or `6502` establishes no implemented API
or firmware behavior by itself. These distinctions remain visible in the family
and network tables. Do not promote a type-correct setter with unknown effects
to writable coverage to satisfy a later prompt.

## Accounting and commands

The original EDS declares 99 index headers: 76 scalar roots and 23 records.
The records have 126 typed children, for 202 typed entries total: 42 U8,
91 U16, 61 U32, one I16 and seven I32. Access flags are 34 RO, 166 RW and
two WO; PDO flags are 112 zero and 90 one. Four `2400` entries are named
Reserved. The ledger records 27 ranges of index gaps and keeps inactive
declared mapping slots distinct from active mapping counts. These are source
counts, not supported operations.

Run from the repository root:

```text
python scripts/check_lichuan_ledger.py
python scripts/check_lichuan_ledger_test.py
python scripts/generate_lichuan_objects.py
python scripts/generate_lichuan_objects.py --check
python scripts/generate_lichuan_objects_test.py
python scripts/check_promptset.py --check-git-ref HEAD
```

The first command validates original hashes/fields/spans, addresses, all source
accounting, reviewed policy, model scope, feature ownership, mandatory gates and
independent coverage. It also preserves the original conflict claim inventory;
changing that guard requires a source-backed correction record. The second
challenges missing obligations, malformed rows, false non-consuming reads and
unsafe writable or resolution claims. The planning checker runs the same
validator. These checks cannot prove a PDF's meaning or device behavior: the
[prompt 3 report](../reports/prompt_3.md) records the original-page review and
its independent challenges. Future evidence-based schema changes must update
the policy guards and their regression cases together, preserving original
claims and the precise reason for each change.

## Generated native declarations and access policy

The generator emits checked-in `Types.h`, `Objects.h`, passive-layout `Pdo.h` and optional `Catalogue.h`
under `include/MotorControlCAN/profiles/lichuan_c/`, plus
[readable generated accounting](lichuan_generated.md). Its only editable object
source is this ledger. It first runs full original-source/schema/accounting
validation, renders every candidate in memory and stages all files before
replacement. A failed replacement restores prior file contents. Check mode
compares exact UTF-8/LF bytes and changes nothing, including on failure.
Both quick/full verification and CI run this real freshness command; version
generation remains independent. Developer generation needs Python's standard
library and retained evidence. Ordinary installed/exported consumers need none
of Python, the ledger, originals or generator execution.

```cpp
#include <MotorControlCAN/profiles/lichuan_c/Objects.h>
// Representation metadata only; it grants no transfer or semantic operation.
static_assert(MotorControlCAN::LICHUAN_C::OBJECT_1009_00.widthBytes == 2,
              "Lichuan hardware version is a U16 code");
```

The compact header has named profile-scoped constants and restrictive policy;
it contains no strings, runtime object dictionary or write-admission helper.
Its bounded `readDescriptor` lookup contains exactly 95 reviewed read shapes
from prompts 19 through 22, without granting read-effect qualification.
Descriptive strings and original limit/default spelling live in the
separately included `Catalogue.h`. A missing catalogue does not prevent numeric
consumption. Neither header is pulled into the common umbrella. Package tests
compile all public headers independently and exercise relocated consumers with
the catalogue removed.

Catalogue text is encoded as UTF-8 bytes in portable C++11 literals, including
supplementary Unicode characters. Fixed-width byte escapes preserve following
digits, and question-mark escapes prevent trigraph translation. An isolated
Unicode surrogate is invalid UTF-8 and fails before any output is published.

`6060`, `6061`, `6098` and `60FF` have known byte widths but conflicting complete
signed types. Their original EDS claims, manual alternatives, owners and gates
stay visible in coverage; no typed descriptor or alternate catalogue route is
generated for them. The malformed `607A` spelling has a reviewed I32 documentary
normalization, so it retains a descriptor. Reserved `2400:09/0A/0B/10` remain
excluded. Counts keep their own native types and access: `2400:00` is U16 and
mapping counts can be RW, while `1018` retains only children `00..03`.

All reads retain `UNQUALIFIED` effects; RO does not mean harmless polling.
Every write policy is forbidden or qualification-required. PDO eligibility is
independent: the EDS RPDO1 claims for WO commands do not add SDO uploads,
approve active maps or enable actions. No default PDO template is implemented
here; observed mapping belongs to prompt 17. Lichuan `0x2300` save/reset cannot
dispatch N5 NanoJ control merely because its index matches.

Generated coverage retains each row's documentary disposition independently of
its declaration disposition: a resolved type may still be pending qualification.
It accounts separately for all 99 indexes, 202 typed source
entries, 53 manual functions, 24 conflicts, gaps and anomaly A01. Descriptor
presence is separate from typed reads/settings/actions/workflows and their
documented, implemented, host-tested and hardware-qualified flags. Whole-object
semantic implementation/test/qualification fields remain false; separate raw-read
and conditional-setting coverage records their narrower software behavior.

## Conditional setting coverage

`setting_scope` and separate `setting_coverage` fields describe twelve implemented
native setting rows: `position_parameters_86c` (6081/6083/6084) and
`explicit_di_configuration_86c` (2400:01..08), plus
`explicit_loop_mode_86c` (2500:00 only, owned by prompt 22).
No other tuning row may acquire that scope. Four booleans retain documentary,
implementation, host-test and hardware-qualified states; hardware remains false.
These scopes require explicit CL86-C/OL86-C binding, actual prerequisites,
continuous owner reservation, partial-effect retention and matched readback.
Loop-mode writes permit native codes 0..2 for CL86-C; OL86-C rejects closed-loop
code 2 because its documented model has no encoder. This is a conditional native
configuration setting, distinct from CiA402 motion mode 6060.
They do not change generic descriptor write policy or whole-object/other-mode
coverage. The validator rejects unknown scopes, wrong owners/addresses/access,
inconsistent tests and invented hardware qualification. Generated documentation
reports these counts separately; generated declarations do not implement settings.
See the [fresh audit](../reports/prompts_20_21_audit.md) for corrected parameter/DI
source interpretation and [the tuning guide](../tuning.md) for the explicit
loop-selector implementation and remaining write gates. Vendor bytes remain immutable.

Read ownership also accounts for deferred communication records. Error history
`1003:00..04` remains owned/read by 19. `1005`, `1014`, `1017` and
`1200:01..02` retain semantic ownership in 38 and raw-read ownership in 19;
`1200:00` retains its service-record count owner 13 and read owner 19.
All 88 PDO record entries retain read owner 38, separately from the eight
communication-count declarations owned by 17 and the remaining 80 semantic
entries owned by 18. None acquires prompt-22 read coverage. The complete deferred
read-owner-38 inventory remains 101 rows.
