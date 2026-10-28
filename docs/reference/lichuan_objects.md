# Lichuan C source intake schema

[lichuan_objects.json](lichuan_objects.json) is the single source-intake ledger for
the retained Lichuan CANopen stepper material. It records original metadata and
manual claims for review in prompt 3. It grants no SDO write, PDO mapping, drive
action or model/firmware support. The original EDS and PDFs remain in
[SOURCES.json](SOURCES.json) under their own copyright and are excluded from a
future core package.

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

## Row schema

| Field | Meaning and validation |
| --- | --- |
| `objects[]` | One row for each EDS index. `key` is canonical `0xXXXX`; `index` is the matching integer; `kind` is the original `ObjectType` 7 scalar, 8 array or 9 record; `object_list` is the original list membership. No array header occurs in this EDS. |
| `raw_fields`, `source_ref` | Exact EDS field spelling/value and named section/line span. The header and every typed child are checked against the original EDS, not just its extraction. |
| `entries[]` | All explicitly typed scalar sections, keyed `0xXXXX:YY`. A scalar root has one entry at `:00`. A record retains its own header and every declared child, including the count entry at `:00`. `SubNumber` counts that entry. |
| `role`, `disposition`, `exclusion_reason` | `count`, `value` or `reserved`; ordinary rows remain `pending_review`. The four EDS `2400` Reserved entries are `excluded` from typed public access despite RW/PDO flags, with a reason. |
| `type`, `access_eds`, `pdo_mapping_eds` | Parsed type code, width and signedness plus exact raw EDS access and mapping flags. `access_reviewed` and `pdo_mapping_reviewed` are `null` until semantic review. An EDS RW or PDO flag does not authorize an effect. |
| `low_limit_eds`, `high_limit_eds`, `default_eds` | Original strings or `null` when absent. `$NODEID+0x...` remains an expression. A missing value is never zero and `null` is never a writable default. |
| `units`, `scaling`, `effect`, `prerequisites`, `edge_behavior` | Explicitly unknown in this intake. Prompt 3 reviews semantics; later owning prompts qualify implementation and effects. |
| `source_refs`, `confidence`, `conflict_ids`, `owner_prompt` | Exact source locations, metadata-only confidence, linked unresolved conflicts and the next numbered owner. |
| `coverage` | Separate `documented`, `implemented`, `host_tested`, `hardware_qualified` booleans. `documented=true` means a source describes the row, not that the meaning or writable use is approved. The other three are false throughout this intake. |
| `manual_only[]` | Semantic functions described by manuals but not encoded as EDS metadata. Every row has a claim, model scope, original PDF page, related EDS entries where applicable, pending-review disposition and independent coverage fields. There are no additional CANopen object addresses in the five manuals. |
| `conflicts[]` | Competing source values with separate original locations. `resolution=unresolved`, `decision=null` and `resolution_evidence=null` throughout prompt 2. No file date or language selects a winner. |
| `source_anomalies[]` | Non-resolution structural questions with unique IDs and explicit source scope. `A01` retains `1018:00` HighLimit 4/default 3 beside the actual declared `00..03` children; no `1018:04` is created. |
| `manual_index_inventory` | Accounts for zero additional CANopen object addresses, excluded nonobject labels such as `6000h`, and absent generic-index examples. Absence is not a measured unsupported response. |

The ledger preserves the EDS's FileInfo, DeviceInfo, object lists, Comments,
DummyUsage and empty DynamicChannels sections. Losslessness here means all
declared EDS object fields and relevant source metadata are retained verbatim;
the immutable original file remains the full byte-for-byte source.

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
python scripts/check_promptset.py --check-git-ref HEAD
```

The first command validates source hashes, exact EDS fields/spans, canonical
addresses, types/widths/numeric strings, all declared sections, scope, original
PDF references, unresolved conflicts and independent coverage. The second
challenges malformed rows and unsafe resolution claims. The planning checker
also runs the ledger validator. Source content and page meaning still require
human review against rendered originals; prompt 3 owns that semantic decision.
