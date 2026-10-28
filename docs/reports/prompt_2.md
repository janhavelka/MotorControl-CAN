# Prompt 2 source inventory and ledger intake

Status: Complete. Date: 2026-10-28. This report covers the user's assigned
[prompt 2](../prompts/2_source_ledger.md) on the existing version-2 execution
branch `promptset/motorcontrol-can`, based on `7878f7c` from `origin/main`.
Prompt 1's checked source map and bench inventory are prerequisites, not a
separate implementation branch. No CAN driver, generator, drive write or live
hardware exchange was added.

## Delivered intake

| Obligation | Evidence and boundary |
| --- | --- |
| Original identity and byte integrity | [Ledger sources](../reference/lichuan_objects.json) record ID, original URL, acquired path, SHA-256, manifest and printed titles, revision statement, named models and applicability evidence for EN86, ZH86, OP86, ZH57, OP57 and EDS. [SOURCES.json](../reference/SOURCES.json) and the original files retain their bytes and separate licensing. |
| Lossless dictionary accounting | The ledger stores all 99 EDS index headers, 202 typed scalar sections, original fields/line spans, object-list membership, other EDS sections and 27 address-gap ranges. It preserves absent values and 11 `$NODEID` expressions verbatim. |
| Manual-only meaning | Fifty-two pending semantic-function rows cite original PDF pages, related EDS entries, scope and a later owner. A manual index review found zero additional CANopen object addresses; the `6000h` heading, PA labels and MODBUS columns have exclusion reasons. |
| Conflicting evidence | Twenty-four conflicts retain competing source claims and exact references with no selected winner. `1018:00` HighLimit 4/default 3 beside actual `00..03` children is retained separately as anomaly A01; no `1018:04` was invented. |
| Schema and checks | [Schema guide](../reference/lichuan_objects.md), [validator](../../scripts/check_lichuan_ledger.py), and [adversarial tests](../../scripts/check_lichuan_ledger_test.py). The ordinary promptset checker invokes the ledger validator. |
| Handoff | [Backlog](../backlog.md), [roadmap](../roadmap.md), [coverage](../coverage.md), [PROGRESS](../prompts/PROGRESS.md), README and CHANGELOG now point to semantic review in prompt 3 without advertising support. |

The original EDS declares 76 scalar-root headers and 23 record headers with
126 typed children, including count subentries; it declares no ObjectType 8
array header. Type counts are U8 42, U16 91, U32 61, I16 1 and I32 7.
Original access flags are RO 34, RW 166 and WO 2; PDO flags are 0 for 112
entries and 1 for 90. Four `2400` entries named Reserved retain their raw RW/PDO
metadata but have an explicit exclusion from typed public access. Sixty-five
typed entries omit a default. These counts describe EDS metadata, not
implemented or permitted operations. Every row has separate documented,
implemented, host-tested and hardware-qualified coverage fields; only
documented is true. No access, effect, prerequisites, unit, scaling, edge or
firmware binding was silently approved.

## Original source and applicability review

All five PDF covers and the EDS FileInfo/DeviceInfo were reopened. The
[source metadata rows](../reference/lichuan_objects.json) hold full URLs and
SHA-256 values; the original source [manifest](../reference/SOURCES.json) also
records retrieval provenance. EN86 and ZH86 covers name CL86-C/OL86-C; OP86's
cover names those 86C models but many internal headers say 57C. ZH57 and OP57
covers name CL57-C/OL57-C. Both OP filenames/manifest labels call them
"operating manual" while their printed covers say "User Manual"; the ledger
records both names. The EDS identifies a generic Lichuan stepper family, not
an installed CL86-C firmware. Its version/creation/modification fields and PDF
metadata dates do not establish firmware identity or that one edition supersedes
another. The 57C material is a design contrast, not an implemented profile.

The independent PDF review inspected rendered original cover and conflict
pages where extraction could reorder columns or obscure notation. Representative
checks included scalar `6040`/`6060` access, `60FF` signedness, record `2400`
with count/reserved children, mapping record `1602` with active count zero but
four declared slots, `1018` declared-child anomaly, and 86C/57C supply and DO
terminal claims. The original EDS, not the extracted JSON, is the validator's
structural authority. The extracted inventory was useful for locating rows but
omits the EDS `Comments`, `DummyUsage` and `DynamicChannels` sections; the new
ledger retains them. The manual review found no extra CANopen index absent
from the EDS. Generic CiA402 objects absent here are recorded as absence
examples, not as a measured unsupported response.

The [public-code provenance audit](../audit/LICHUAN_PUBLIC_IMPLEMENTATIONS.md)
remains context only: CANopenNode and canopen-python corroborate mechanics, and
the public Lichuan example retains its own license and documented SDO-command,
homing-width and ownership defects. No implementation or authority was copied
from it. The original vendor pack was neither modified nor incorporated into a
future distributable core license.

## Current sibling and consumer delta

The lead delegated independent read-only EDS accounting, original-PDF review
and sibling/FieldCore checks; the lead owns schema, merged ledger, documentation,
verification and Git. All named checkouts were present and clean. Their HEADs
match the current revisions recorded by prompt 1, so no dirty-file behavior is
attributed to a hash:

| Checkout | Current HEAD | Relevant seam verified |
| --- | --- | --- |
| `../MotorControl-RS` | `bd831c859daa9c8c07b8d27a2dd879bb4e750511` | `docs/reference/ess_rs_registers.json`, `scripts/generate_ess_registers.py`, `test/generator_test.py`, `include/MotorControlRS/Registers.h`, `src/Registers.cpp`, codec/ProbeConsole consumers and freshness workflow. Its approved register policy is separate from raw vendor input. |
| `../ADS1115` | `235af3e8c722661a89a44766a4ccd903a654f6fe` | Typed configuration, generation and caller-owned publication; no motor write policy imported. |
| `../INA228` | `b28c5ea69bcb4d26b351c52c24d3c6fae9fd282b` | Typed read/configuration and destructive DIAG_ALERT/identity handling; no universal I2C lifecycle inferred. |
| `../MB85RC` | `1bf7c293549b1038b2d8f3b7a8d655fa74201e6e` | Variant scope remains explicit, supporting an unbound EDS scope instead of automatic CL/OL applicability. |
| `../PCA9555` | `498178e4d9e4acb6fab7d8b816ad5021efe8806a` | Reserved command bits and typed-setting safeguards reinforce exclusion of reserved EDS names despite permissive raw flags. |
| `../FieldCore-node` | `5ad9292dc8481cf08be9a61f662495a6474d1da0` | Configuration/identity generation and owner/backend effects remain application-owned; this repository was read only. |

RS's generator and its freshness mode demonstrate the later reviewed-policy
pattern but do not validate Lichuan access. Its generated declarations, CLI and
package are not duplicated. The current target checkout has a planning checker
and its tests; it has no CAN package, native build, device examples or CI
workflow yet. No new C++ declarations require Doxygen in this prompt.

## Verification and review

| Check | Result |
| --- | --- |
| `python scripts/check_lichuan_ledger.py` | PASS: 99 indexes, 202 typed entries and 52 manual items; exact original hashes/fields, scopes, addresses, types, refs, coverage and unresolved claims checked. |
| `python scripts/check_lichuan_ledger_test.py` | PASS: 32 tests including malformed rows, numeric formats/ranges, duplicate keys/addresses, missing applicability, fabricated values/subindex, invalid coverage and false conflict resolution. |
| `python scripts/check_promptset.py --check-git-ref HEAD` | PASS: 50 prompts, 35 requirements, 123 retained artifact hashes and 123 committed source bytes. |
| `python scripts/check_promptset_test.py` | PASS: 10 planning-checker regressions. |
| `python scripts/generate_ess_registers.py --check` and `python test/generator_test.py` from `../MotorControl-RS` | PASS: current RS generated policy is fresh (221 descriptors/242 words); 18 focused tests. |
| `python scripts/check_promptset.py --check-git-ref INDEX` | PASS after explicit staging: 123 retained artifact hashes and staged Git bytes, 99 ledger objects, 202 entries, 52 manual items and 24 unresolved conflicts. |
| `git diff --check` and `git diff --cached --check` | PASS after integration and staging. |

The validator was reviewed independently against raw EDS sections and
adversarial inputs. Review found that object headers needed an enforced EDS
source reference, manual object addresses needed uniqueness within scope,
malformed nested rows needed reported errors instead of exceptions, and the
`1018` inconsistency needed an explicit row; all were corrected before
acceptance. Original PDF page review corrected several intake citations and
kept the velocity, access, DO, supply and status alternatives unresolved.
No unchanged sibling suite was repeated. No CAN compilation, firmware build,
remote CI or hardware test was run; none is a prompt-2 acceptance gate and the
target has no CI workflow to dispatch.

## Handoff and limits

Prompt 3 must review semantic access, units, scaling, effects, prerequisites,
edges, exact model/firmware scope and contradictions against original pages.
Prompt 4 owns additional-manufacturer contrasts; prompt 12 owns generation
from a reviewed policy. The [backlog](../backlog.md) retains unresolved firmware
and physical evidence questions. Nothing in this intake permits writing an
EDS RW entry or qualifies an installed drive. The actual CAN bench remains
unknown in [hardware_bench.md](../hardware_bench.md).

Delivery is the commit containing this report on `promptset/motorcontrol-can`.
The lead reruns the committed `HEAD` byte check, pushes to the configured
upstream and reports the actual commit and fresh ahead/behind result in the
final handoff; the report avoids a self-referential commit hash.
